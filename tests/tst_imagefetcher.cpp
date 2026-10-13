#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QUrl>

#include "services/imagefetcher.h"

// Just enough HTTP/1.1 to serve canned responses; "/hang" never answers.
class TinyHttpServer : public QObject
{
public:
    struct Response { int status; QByteArray body; };
    QHash<QString, Response> routes;
    int requests = 0;

    TinyHttpServer()
    {
        QVERIFY(m_server.listen(QHostAddress::LocalHost));
        connect(&m_server, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    m_buffers[socket] += socket->readAll();
                    if (!m_buffers[socket].contains("\r\n\r\n"))
                        return;
                    ++requests;
                    const QString path = QString::fromLatin1(m_buffers[socket].split(' ').value(1));
                    if (path == "/hang")
                        return;
                    const Response r = routes.value(path, Response{404, "missing"});
                    socket->write("HTTP/1.1 " + QByteArray::number(r.status) + " X\r\nContent-Length: "
                                  + QByteArray::number(r.body.size()) + "\r\nConnection: close\r\n\r\n" + r.body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QString url(const QString &path) const
    {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(m_server.serverPort()).arg(path);
    }

private:
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
};

class TestImageFetcher : public QObject
{
    Q_OBJECT

private:
    static QByteArray png(int w, int h)
    {
        QImage image(w, h, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return bytes;
    }

private slots:
    void testFetchStoresImageWithItsSize()
    {
        TinyHttpServer server;
        server.routes["/a.png"] = {200, png(4, 3)};
        ImageFetcher fetcher;
        fetcher.setNotifyDelay(0);
        const QString url = server.url("/a.png");

        fetcher.fetch({url});

        QTRY_VERIFY(fetcher.images().contains(url));
        const QVariantMap entry = fetcher.images().value(url).toMap();
        QVERIFY(QFile::exists(QUrl(entry.value("path").toString()).toLocalFile()));
        QCOMPARE(entry.value("width").toInt(), 4);
        QCOMPARE(entry.value("height").toInt(), 3);
        QCOMPARE(fetcher.revision(), 1);
    }

    void testHtmlErrorPageIsNotAnImage()
    {
        TinyHttpServer server;
        server.routes["/fake.png"] = {200, "<html><body>rate limited</body></html>"};
        ImageFetcher fetcher;
        const QString url = server.url("/fake.png");
        fetcher.fetch({url});
        QTRY_VERIFY(fetcher.images().value(url).toMap().value("failed").toBool());
    }

    void testNotFoundFails()
    {
        TinyHttpServer server;
        ImageFetcher fetcher;
        const QString url = server.url("/nothing.png");
        fetcher.fetch({url});
        QTRY_VERIFY(fetcher.images().value(url).toMap().value("failed").toBool());
    }

    void testTooLargeFails()
    {
        TinyHttpServer server;
        server.routes["/big.png"] = {200, png(200, 200)};
        ImageFetcher fetcher;
        fetcher.setMaxBytes(100);
        const QString url = server.url("/big.png");
        fetcher.fetch({url});
        QTRY_VERIFY(fetcher.images().value(url).toMap().value("failed").toBool());
    }

    void testSlowServerTimesOut()
    {
        TinyHttpServer server;
        ImageFetcher fetcher;
        fetcher.setTimeoutMs(300);
        const QString url = server.url("/hang");
        fetcher.fetch({url});
        QTRY_VERIFY_WITH_TIMEOUT(fetcher.images().value(url).toMap().value("failed").toBool(), 5000);
    }

    void testNonHttpUrlsAreRefused()
    {
        ImageFetcher fetcher;
        fetcher.fetch({"file:///etc/hostname", "ftp://x.y/a.png"});
        QTRY_VERIFY(fetcher.images().value("file:///etc/hostname").toMap().value("failed").toBool());
        QTRY_VERIFY(fetcher.images().value("ftp://x.y/a.png").toMap().value("failed").toBool());
    }

    void testEachUrlIsRequestedOnce()
    {
        TinyHttpServer server;
        server.routes["/a.png"] = {200, png(2, 2)};
        ImageFetcher fetcher;
        const QString url = server.url("/a.png");
        fetcher.fetch({url, url});
        QTRY_VERIFY(fetcher.images().contains(url));
        fetcher.fetch({url});
        QTest::qWait(100);
        QCOMPARE(server.requests, 1);
    }
};

QTEST_GUILESS_MAIN(TestImageFetcher)
#include "tst_imagefetcher.moc"
