#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>

// Downloads the remote images of the document shown by the `--view` reader
// (never used by the quick preview). Each url is fetched once per process
// into a private temp folder, with a timeout and a size cap, and kept only
// when Qt can read it as an image. Results go to MarkdownRenderer's
// `remoteImages` option. Exposed to QML as `imageFetcher`.
class ImageFetcher : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap images READ images NOTIFY imagesChanged)
    // Bumped with every finished url; the view re-renders when it changes.
    Q_PROPERTY(int revision READ revision NOTIFY imagesChanged)

public:
    static constexpr int DefaultTimeoutMs = 15000;
    static constexpr qint64 DefaultMaxBytes = 10 * 1024 * 1024;

    explicit ImageFetcher(QObject *parent = nullptr);

    QVariantMap images() const { return m_images; }
    int revision() const { return m_revision; }

    Q_INVOKABLE void fetch(const QStringList &urls);

    void setTimeoutMs(int ms) { m_timeoutMs = ms; }
    void setMaxBytes(qint64 bytes) { m_maxBytes = bytes; }
    // Results arriving together are announced once, after this delay.
    void setNotifyDelay(int ms) { m_notify.setInterval(ms); }

signals:
    void imagesChanged();

private:
    void start(const QString &url);
    void finish(const QString &url, const QVariantMap &entry);

    QNetworkAccessManager m_network;
    QTemporaryDir m_dir;
    QVariantMap m_images;
    QSet<QString> m_requested;
    QTimer m_notify;
    int m_revision = 0;
    int m_timeoutMs = DefaultTimeoutMs;
    qint64 m_maxBytes = DefaultMaxBytes;
};
