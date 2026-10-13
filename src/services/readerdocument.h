#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

// The document behind a `seraph --view` window: reads the whole file, follows
// it on disk (live reload, delete and reappear, save-by-rename), classifies
// the links the reader follows, and keeps the back-history of markdown
// documents opened in the same window. Exposed to QML as `readerDocument`.
class ReaderDocument : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged)
    Q_PROPERTY(QString baseDir READ baseDir NOTIFY documentChanged)
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)
    // Why an existing file cannot be shown (unreadable, binary, too large); "" otherwise.
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
    // The file is not on disk right now. With content still set, it was
    // deleted while open and the last version stays on screen.
    Q_PROPERTY(bool missing READ missing NOTIFY statusChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)

public:
    static constexpr qint64 MaxFileBytes = 32 * 1024 * 1024;

    explicit ReaderDocument(QObject *parent = nullptr);

    QString filePath() const { return m_path; }
    QString fileName() const;
    QString baseDir() const;
    QString content() const { return m_content; }
    QString errorMessage() const { return m_error; }
    bool missing() const { return m_missing; }
    bool canGoBack() const { return !m_history.isEmpty(); }

    // Shows `path` and forgets the history (the window's first document).
    Q_INVOKABLE void open(const QString &path);
    // Follows a link to another markdown file, remembering where the current
    // one was scrolled to so goBack() can return there.
    Q_INVOKABLE void navigate(const QString &path, const QString &fragment, qreal currentScrollY);
    Q_INVOKABLE bool goBack();
    // Re-reads the file now (Ctrl+R); the view keeps its scroll position.
    Q_INVOKABLE void reload();

    // {kind, path, fragment, url} for a link clicked in the current document.
    // kind: "anchor" (fragment in this document), "markdown" (open here),
    // "file" (another local file or folder: system default app), "web"
    // (anything with a non-file scheme: system handler), "none".
    Q_INVOKABLE QVariantMap classifyLink(const QString &link) const;
    static QVariantMap classifyLink(const QString &link, const QString &currentFile);
    static bool isMarkdownPath(const QString &path);

    // Tests shorten the coalescing delay for disk events.
    void setDebounceInterval(int ms);

signals:
    void documentChanged();
    void contentChanged();
    void statusChanged();
    void historyChanged();
    // After every load. reason: "open", "navigate", "back" or "live". The view
    // keeps its scroll for "live", else jumps to `fragment` when set, else to
    // scrollY when >= 0, else to the top.
    void shown(const QString &reason, const QString &fragment, qreal scrollY);

private:
    struct HistoryEntry {
        QString path;
        qreal scrollY = 0;
    };

    void setPath(const QString &path);
    void load(const QString &reason, const QString &fragment, qreal scrollY);
    void rearmWatch();

    QString m_path;
    QString m_content;
    QString m_error;
    bool m_missing = false;
    QVector<HistoryEntry> m_history;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
