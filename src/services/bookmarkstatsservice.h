#pragma once

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

// Item count and last modification of each pinned local folder, for the
// sidebar cards. Scans run through QtConcurrent and land back on this
// object's thread; a folder is rescanned, debounced, whenever its directory
// changes. Remote (URI) bookmarks are never scanned and always report -1.
class BookmarkStatsService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit BookmarkStatsService(QObject *parent = nullptr);

    int revision() const { return m_revision; }

    void setPaths(const QStringList &paths);
    void setIncludeHidden(bool includeHidden);

    // {"itemCount": int (-1 while unknown or unreadable), "modified": QDateTime}
    Q_INVOKABLE QVariantMap statsFor(const QString &path) const;
    // "now", "%n min", "today", "yesterday", "%n d", else the short date; ""
    // for an invalid time.
    Q_INVOKABLE QString relativeTime(const QDateTime &when) const;

signals:
    void statsChanged(const QString &path);
    void revisionChanged();

private:
    struct Stats {
        int itemCount = -1;
        QDateTime modified;
    };

    static bool isLocalPath(const QString &path);
    void requestScan(const QString &path, int delayMs);
    void runPendingScans();
    void applyResult(const QString &path, const Stats &stats);

    QStringList m_paths;
    bool m_includeHidden = false;
    QHash<QString, Stats> m_stats;
    QSet<QString> m_pending;
    QTimer m_debounce;
    QFileSystemWatcher m_watcher;
    int m_revision = 0;
};
