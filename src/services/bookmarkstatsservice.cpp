#include "services/bookmarkstatsservice.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLocale>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr int kDebounceMs = 500;
}

BookmarkStatsService::BookmarkStatsService(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    connect(&m_debounce, &QTimer::timeout, this, &BookmarkStatsService::runPendingScans);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        requestScan(path, kDebounceMs);
    });
}

bool BookmarkStatsService::isLocalPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QUrl url(path);
    return !(url.isValid() && !url.scheme().isEmpty() && url.scheme() != QStringLiteral("file"));
}

void BookmarkStatsService::setPaths(const QStringList &paths)
{
    QStringList local;
    for (const QString &p : paths) {
        if (isLocalPath(p) && !local.contains(p))
            local.append(p);
    }

    // Stop watching what left; start watching and scan what arrived.
    for (const QString &old : std::as_const(m_paths)) {
        if (!local.contains(old)) {
            m_watcher.removePath(old);
            m_stats.remove(old);
            m_pending.remove(old);
        }
    }
    for (const QString &p : std::as_const(local)) {
        if (!m_paths.contains(p)) {
            m_watcher.addPath(p);   // fails silently for unreadable paths
            requestScan(p, 0);
        }
    }
    m_paths = local;
}

void BookmarkStatsService::setIncludeHidden(bool includeHidden)
{
    if (m_includeHidden == includeHidden)
        return;
    m_includeHidden = includeHidden;
    for (const QString &p : std::as_const(m_paths))
        requestScan(p, 0);
}

QVariantMap BookmarkStatsService::statsFor(const QString &path) const
{
    const Stats stats = m_stats.value(path);
    return {
        {QStringLiteral("itemCount"), stats.itemCount},
        {QStringLiteral("modified"), stats.modified},
    };
}

QString BookmarkStatsService::relativeTime(const QDateTime &when) const
{
    if (!when.isValid())
        return QString();

    const QDateTime now = QDateTime::currentDateTime();
    const qint64 secs = when.secsTo(now);
    if (secs < 60)
        return tr("now");
    if (secs < 3600)
        return tr("%n min", nullptr, int(secs / 60));

    const qint64 days = when.date().daysTo(now.date());
    if (days <= 0)
        return tr("today");
    if (days == 1)
        return tr("yesterday");
    if (days < 7)
        return tr("%n d", nullptr, int(days));
    return QLocale().toString(when.date(), QLocale::ShortFormat);
}

void BookmarkStatsService::requestScan(const QString &path, int delayMs)
{
    if (!m_paths.contains(path) && delayMs > 0)
        return;   // a watcher event for a path that was unpinned meanwhile
    m_pending.insert(path);
    // A pending immediate request wins over a running debounce.
    if (!m_debounce.isActive() || delayMs < m_debounce.remainingTime())
        m_debounce.start(delayMs);
}

void BookmarkStatsService::runPendingScans()
{
    const QSet<QString> batch = m_pending;
    m_pending.clear();
    const bool includeHidden = m_includeHidden;

    for (const QString &path : batch) {
        auto *watcher = new QFutureWatcher<Stats>(this);
        connect(watcher, &QFutureWatcher<Stats>::finished, this, [this, watcher, path]() {
            applyResult(path, watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([path, includeHidden]() {
            Stats stats;
            const QFileInfo info(path);
            if (!info.isDir() || !info.isReadable())
                return stats;
            QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
            if (includeHidden)
                filters |= QDir::Hidden;
            stats.itemCount = QDir(path).entryList(filters).size();
            stats.modified = info.lastModified();
            return stats;
        }));
    }
}

void BookmarkStatsService::applyResult(const QString &path, const Stats &stats)
{
    if (!m_paths.contains(path))
        return;
    m_stats.insert(path, stats);
    // The watcher drops a path whose directory was replaced; re-arm it.
    if (!m_watcher.directories().contains(path) && stats.itemCount >= 0)
        m_watcher.addPath(path);
    ++m_revision;
    emit statsChanged(path);
    emit revisionChanged();
}
