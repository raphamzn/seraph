#include "services/readerdocument.h"

#include "services/previewservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <sys/stat.h>

ReaderDocument::ReaderDocument(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);

    // Editors (and Claude) save by writing a new file and renaming it over the
    // old one, which drops the file watch; the folder watch sees the rename
    // and rearmWatch() re-adds the file. The delay coalesces the write burst
    // a save produces into one read.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_debounce, &QTimer::timeout, this, [this]() {
        load(QStringLiteral("live"), QString(), -1);
    });
}

QString ReaderDocument::fileName() const
{
    return QFileInfo(m_path).fileName();
}

QString ReaderDocument::baseDir() const
{
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).absolutePath();
}

void ReaderDocument::setDebounceInterval(int ms)
{
    m_debounce.setInterval(ms);
}

void ReaderDocument::open(const QString &path)
{
    const bool hadHistory = !m_history.isEmpty();
    m_history.clear();
    if (hadHistory)
        emit historyChanged();
    setPath(path);
    load(QStringLiteral("open"), QString(), -1);
}

void ReaderDocument::setPath(const QString &path)
{
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (clean == m_path)
        return;
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_path = clean;
}

void ReaderDocument::load(const QString &reason, const QString &fragment, qreal scrollY)
{
    const bool live = reason == QLatin1String("live");
    const DiskStamp stamp = stampOf(m_path);
    // Folder events fire for every file in it; skip the read when this one
    // is byte-for-byte where it was.
    if (live && stamp.inode != 0 && stamp == m_readStamp && !m_missing) {
        rearmWatch();
        return;
    }
    m_readStamp = stamp;
    QString text;
    QString error;
    const QFileInfo info(m_path);
    const bool missingNow = !info.exists();

    if (!missingNow) {
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Cannot open %1: %2").arg(info.fileName(), file.errorString());
        } else if (file.size() > MaxFileBytes) {
            error = QStringLiteral("%1 is too large to show (over 32 MB).").arg(info.fileName());
        } else {
            const QByteArray data = file.readAll();
            if (PreviewService::looksBinary(data))
                error = QStringLiteral("This file looks binary and cannot be previewed as text.");
            else
                text = PreviewService::decodeText(data);
        }
    } else if (live) {
        // Deleted while open: keep the last version on screen.
        text = m_content;
    }

    if (!live)
        emit documentChanged();

    const bool contentDiffers = text != m_content;
    m_content = text;
    if (contentDiffers)
        emit contentChanged();

    if (error != m_error || missingNow != m_missing) {
        m_error = error;
        m_missing = missingNow;
        emit statusChanged();
    }

    rearmWatch();

    if (!live || contentDiffers)
        emit shown(reason, fragment, scrollY);
}

void ReaderDocument::rearmWatch()
{
    if (m_path.isEmpty())
        return;
    const QString dir = QFileInfo(m_path).absolutePath();
    if (QFileInfo::exists(dir) && !m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);
    if (!QFileInfo::exists(m_path))
        return;
    // A save-by-rename puts a new inode at the path. When something still
    // holds the old one open, inotify keeps watching it and the path stays
    // in files(), so compare inodes instead of trusting files().
    const quint64 inode = stampOf(m_path).inode;
    if (m_watcher.files().contains(m_path) && inode == m_watchedInode)
        return;
    m_watcher.removePath(m_path);
    if (m_watcher.addPath(m_path))
        m_watchedInode = inode;
}

ReaderDocument::DiskStamp ReaderDocument::stampOf(const QString &path)
{
    struct stat st;
    if (::stat(QFile::encodeName(path).constData(), &st) != 0)
        return {};
    return {static_cast<quint64>(st.st_ino), static_cast<qint64>(st.st_size),
            static_cast<qint64>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec};
}

void ReaderDocument::reload()
{
    load(QStringLiteral("live"), QString(), -1);
}

QVariantMap ReaderDocument::classifyLink(const QString &link) const
{
    return classifyLink(link, m_path);
}

void ReaderDocument::navigate(const QString &path, const QString &fragment, qreal currentScrollY)
{
    m_history.append(HistoryEntry{m_path, currentScrollY});
    emit historyChanged();
    setPath(path);
    load(QStringLiteral("navigate"), fragment, -1);
}

bool ReaderDocument::goBack()
{
    if (m_history.isEmpty())
        return false;
    const HistoryEntry entry = m_history.takeLast();
    emit historyChanged();
    setPath(entry.path);
    load(QStringLiteral("back"), QString(), entry.scrollY);
    return true;
}

QVariantMap ReaderDocument::classifyLink(const QString &link, const QString &currentFile)
{
    const auto result = [](const QString &kind, const QString &path = QString(),
                           const QString &fragment = QString(), const QString &url = QString()) {
        return QVariantMap{{QStringLiteral("kind"), kind}, {QStringLiteral("path"), path},
                           {QStringLiteral("fragment"), fragment}, {QStringLiteral("url"), url}};
    };
    const auto decode = [](const QString &text) {
        return QUrl::fromPercentEncoding(text.toUtf8());
    };

    if (link.isEmpty())
        return result(QStringLiteral("none"));
    if (link.startsWith(QLatin1Char('#')))
        return result(QStringLiteral("anchor"), QString(), decode(link.mid(1)));

    const QUrl url(link);
    QString path;
    QString fragment;
    if (url.isLocalFile()) {
        path = url.toLocalFile();
        fragment = url.fragment(QUrl::FullyDecoded);
    } else if (url.scheme().isEmpty()) {
        // A raw path the renderer left alone (absolute, or relative with no baseDir).
        const int hash = link.indexOf(QLatin1Char('#'));
        const QString raw = decode(hash >= 0 ? link.left(hash) : link);
        fragment = hash >= 0 ? decode(link.mid(hash + 1)) : QString();
        path = QDir::isAbsolutePath(raw) ? raw : QFileInfo(currentFile).dir().filePath(raw);
    } else {
        return result(QStringLiteral("web"), QString(), QString(), link);
    }

    path = QDir::cleanPath(path);
    if (!currentFile.isEmpty() && path == QDir::cleanPath(currentFile))
        return result(QStringLiteral("anchor"), QString(), fragment);
    if (isMarkdownPath(path) && !QFileInfo(path).isDir())
        return result(QStringLiteral("markdown"), path, fragment);
    return result(QStringLiteral("file"), path, QString(), QUrl::fromLocalFile(path).toString());
}

bool ReaderDocument::isMarkdownPath(const QString &path)
{
    static const QStringList suffixes{
        QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("mdown"),
        QStringLiteral("mkd"), QStringLiteral("mkdn"), QStringLiteral("mdwn")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}
