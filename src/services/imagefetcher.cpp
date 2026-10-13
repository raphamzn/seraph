#include "services/imagefetcher.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

ImageFetcher::ImageFetcher(QObject *parent)
    : QObject(parent)
{
    m_notify.setSingleShot(true);
    m_notify.setInterval(120);
    connect(&m_notify, &QTimer::timeout, this, &ImageFetcher::imagesChanged);
}

void ImageFetcher::fetch(const QStringList &urls)
{
    for (const QString &url : urls) {
        if (url.isEmpty() || m_requested.contains(url))
            continue;
        m_requested.insert(url);
        start(url);
    }
}

void ImageFetcher::start(const QString &url)
{
    const QUrl target(url);
    const QString scheme = target.scheme().toLower();
    if (!m_dir.isValid() || (scheme != QLatin1String("http") && scheme != QLatin1String("https"))) {
        finish(url, QVariantMap{{QStringLiteral("failed"), true}});
        return;
    }

    QNetworkRequest request(target);
    request.setTransferTimeout(m_timeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Seraph"));
    QNetworkReply *reply = m_network.get(request);

    const auto tooBig = [this](qint64 bytes) { return m_maxBytes > 0 && bytes > m_maxBytes; };
    connect(reply, &QNetworkReply::metaDataChanged, reply, [reply, tooBig]() {
        const QVariant length = reply->header(QNetworkRequest::ContentLengthHeader);
        if (length.isValid() && tooBig(length.toLongLong()))
            reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply, tooBig](qint64 received, qint64) {
        if (tooBig(received))
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, tooBig]() {
        reply->deleteLater();
        const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
        if (data.isEmpty() || tooBig(data.size())) {
            finish(url, QVariantMap{{QStringLiteral("failed"), true}});
            return;
        }
        // Trust the bytes, not the Content-Type: raw hosts often send
        // text/plain, and an HTML error page must not become an "image".
        QBuffer buffer;
        buffer.setData(data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer);
        const QByteArray format = reader.format();
        if (format.isEmpty()) {
            finish(url, QVariantMap{{QStringLiteral("failed"), true}});
            return;
        }
        const QSize size = reader.size();
        const QString name = QString::fromLatin1(
            QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex())
            + QLatin1Char('.') + QString::fromLatin1(format);
        const QString path = QDir(m_dir.path()).filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            finish(url, QVariantMap{{QStringLiteral("failed"), true}});
            return;
        }
        file.close();
        finish(url, QVariantMap{
            {QStringLiteral("path"), QUrl::fromLocalFile(path).toString()},
            {QStringLiteral("width"), size.isValid() ? size.width() : 0},
            {QStringLiteral("height"), size.isValid() ? size.height() : 0},
        });
    });
}

void ImageFetcher::finish(const QString &url, const QVariantMap &entry)
{
    m_images.insert(url, entry);
    ++m_revision;
    m_notify.start();
}
