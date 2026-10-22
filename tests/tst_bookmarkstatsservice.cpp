#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QDateTime>
#include "services/bookmarkstatsservice.h"

// Item counts and modification times for the pinned-folder cards: computed
// off the GUI thread, debounced, never for remote locations.
class TestBookmarkStatsService : public QObject
{
    Q_OBJECT

    static void touch(const QString &path, const QByteArray &content = "x")
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    }

private slots:
    void testCountsDirectChildren()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir("a"));
        touch(dir.path() + "/b.txt");
        touch(dir.path() + "/.hidden");

        BookmarkStatsService svc;
        QSignalSpy spy(&svc, &BookmarkStatsService::statsChanged);
        svc.setPaths({dir.path()});
        QVERIFY(spy.wait(3000));

        const QVariantMap stats = svc.statsFor(dir.path());
        QCOMPARE(stats.value("itemCount").toInt(), 2);
        QVERIFY(stats.value("modified").toDateTime().isValid());
    }

    void testHiddenToggleRescans()
    {
        QTemporaryDir dir;
        touch(dir.path() + "/b.txt");
        touch(dir.path() + "/.hidden");

        BookmarkStatsService svc;
        QSignalSpy spy(&svc, &BookmarkStatsService::statsChanged);
        svc.setPaths({dir.path()});
        QVERIFY(spy.wait(3000));
        QCOMPARE(svc.statsFor(dir.path()).value("itemCount").toInt(), 1);

        spy.clear();
        svc.setIncludeHidden(true);
        QVERIFY(spy.wait(3000));
        QCOMPARE(svc.statsFor(dir.path()).value("itemCount").toInt(), 2);
    }

    void testStatsForIsImmediateAndLaterFilled()
    {
        QTemporaryDir dir;
        for (int i = 0; i < 3000; ++i)
            touch(dir.path() + QStringLiteral("/f%1").arg(i), "");

        BookmarkStatsService svc;
        QSignalSpy spy(&svc, &BookmarkStatsService::statsChanged);
        QElapsedTimer t;
        t.start();
        svc.setPaths({dir.path()});
        const QVariantMap first = svc.statsFor(dir.path());
        QVERIFY2(t.elapsed() < 50, qPrintable(QString::number(t.elapsed())));
        QCOMPARE(first.value("itemCount").toInt(), -1);
        QVERIFY(!first.value("modified").toDateTime().isValid());

        QVERIFY(spy.wait(5000));
        QCOMPARE(svc.statsFor(dir.path()).value("itemCount").toInt(), 3000);
    }

    void testUnreadableAndRemoteGiveMinusOne()
    {
        BookmarkStatsService svc;
        QSignalSpy spy(&svc, &BookmarkStatsService::statsChanged);
        svc.setPaths({"/nonexistent/path/xyz", "sftp://box/srv"});
        spy.wait(700);
        QCOMPARE(svc.statsFor("/nonexistent/path/xyz").value("itemCount").toInt(), -1);
        QCOMPARE(svc.statsFor("sftp://box/srv").value("itemCount").toInt(), -1);
    }

    void testFileChangeBumpsRevision()
    {
        QTemporaryDir dir;
        touch(dir.path() + "/one.txt");

        BookmarkStatsService svc;
        QSignalSpy spy(&svc, &BookmarkStatsService::statsChanged);
        svc.setPaths({dir.path()});
        QVERIFY(spy.wait(3000));
        const int before = svc.revision();
        QCOMPARE(svc.statsFor(dir.path()).value("itemCount").toInt(), 1);

        spy.clear();
        touch(dir.path() + "/two.txt");
        QVERIFY(spy.wait(2500));
        QVERIFY(svc.revision() > before);
        QCOMPARE(svc.statsFor(dir.path()).value("itemCount").toInt(), 2);
    }

    void testRelativeTime()
    {
        BookmarkStatsService svc;
        const QDateTime now = QDateTime::currentDateTime();
        QCOMPARE(svc.relativeTime(now.addSecs(-10)), QString("now"));
        QCOMPARE(svc.relativeTime(now.addSecs(-5 * 60)), QString("5 min"));
        QCOMPARE(svc.relativeTime(now.addDays(-1)), QString("yesterday"));
        QCOMPARE(svc.relativeTime(now.addDays(-3)), QString("3 d"));
        QVERIFY(!svc.relativeTime(now.addDays(-30)).isEmpty());
        QCOMPARE(svc.relativeTime(QDateTime()), QString());
    }
};

QTEST_GUILESS_MAIN(TestBookmarkStatsService)
#include "tst_bookmarkstatsservice.moc"
