#include "models/bookmarkmodel.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

namespace {

bool isRemoteUri(const QString &path)
{
    const QUrl url(path);
    return url.isValid() && !url.scheme().isEmpty()
        && url.scheme() != QStringLiteral("file")
        && url.scheme() != QStringLiteral("trash");
}

QString remoteHint(const QString &path)
{
    const QUrl url(path);
    if (url.host().isEmpty())
        return url.scheme();
    return url.scheme() + QStringLiteral(" · ") + url.host();
}

QString bookmarkDisplayName(const QString &path)
{
    if (!isRemoteUri(path)) {
        QString name = QDir(path).dirName();
        return name.isEmpty() ? path : name;
    }

    const QUrl url(path);
    const QString fileName = QUrl::fromPercentEncoding(url.fileName().toUtf8());
    if (!fileName.isEmpty())
        return fileName;
    if (!url.host().isEmpty())
        return url.host();
    return url.scheme().toUpper();
}

}

BookmarkModel::BookmarkModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int BookmarkModel::rowCount(const QModelIndex &) const
{
    return m_bookmarks.size();
}

QVariant BookmarkModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_bookmarks.size())
        return {};

    const auto &bm = m_bookmarks.at(index.row());
    switch (role) {
    case NameRole: return bm.label.isEmpty() ? bm.name : bm.label;
    case PathRole: return bm.path;
    case IconRole: return bm.icon;
    case LabelRole: return bm.label;
    case QuillIconRole: return bm.styleIcon.isEmpty() ? defaultQuillIcon(bm.path) : bm.styleIcon;
    case ColorRoleRole: return bm.color;
    case IsRemoteRole: return bm.remote;
    case RemoteHintRole: return bm.remote ? remoteHint(bm.path) : QString();
    case KeyRole: return bm.key;
    }
    return {};
}

QHash<int, QByteArray> BookmarkModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IconRole, "icon"},
        {LabelRole, "label"},
        {QuillIconRole, "quillIcon"},
        {ColorRoleRole, "colorRole"},
        {IsRemoteRole, "isRemote"},
        {RemoteHintRole, "remoteHint"},
        {KeyRole, "styleKey"},
    };
}

void BookmarkModel::setStyles(const QVariantMap &styles)
{
    m_styles.clear();
    for (auto it = styles.cbegin(); it != styles.cend(); ++it) {
        const QString key = keyForPath(expandPath(it.key()));
        QVariantMap style = it.value().toMap();
        style.insert(QStringLiteral("color"), validColorRole(style.value(QStringLiteral("color")).toString()));
        m_styles.insert(key, style);
    }
    for (auto &bm : m_bookmarks)
        applyStyle(bm);
    if (!m_bookmarks.isEmpty())
        emit dataChanged(index(0, 0), index(m_bookmarks.size() - 1, 0));
}

QVariantMap BookmarkModel::styleFor(int row) const
{
    if (row < 0 || row >= m_bookmarks.size())
        return {};
    const Bookmark &bm = m_bookmarks.at(row);
    return {
        {QStringLiteral("icon"), bm.styleIcon},
        {QStringLiteral("color"), bm.color},
        {QStringLiteral("label"), bm.label},
        {QStringLiteral("folderName"), bm.name},
        {QStringLiteral("key"), bm.key},
        {QStringLiteral("path"), bm.path},
    };
}

void BookmarkModel::setStyle(int row, const QString &icon, const QString &color, const QString &label)
{
    if (row < 0 || row >= m_bookmarks.size())
        return;

    Bookmark &bm = m_bookmarks[row];
    const QString validColor = validColorRole(color);
    QVariantMap style;
    if (!icon.isEmpty())
        style.insert(QStringLiteral("icon"), icon);
    if (!validColor.isEmpty())
        style.insert(QStringLiteral("color"), validColor);
    if (!label.isEmpty())
        style.insert(QStringLiteral("label"), label);

    if (style.isEmpty())
        m_styles.remove(bm.key);
    else
        m_styles.insert(bm.key, style);
    applyStyle(bm);

    emit dataChanged(index(row, 0), index(row, 0));
    emit styleChanged(bm.key, icon, validColor, label);
}

QString BookmarkModel::defaultQuillIcon(const QString &expandedPath)
{
    if (expandedPath.isEmpty())
        return QStringLiteral("Folder");
    if (isRemoteUri(expandedPath))
        return QStringLiteral("Globe");

    // Specific dirs first: an unset XDG dir falls back to the home folder and
    // must not steal the Home icon.
    static const QVector<QPair<QStandardPaths::StandardLocation, QString>> xdgIcons = {
        {QStandardPaths::DocumentsLocation, QStringLiteral("FileText")},
        {QStandardPaths::DownloadLocation, QStringLiteral("Download")},
        {QStandardPaths::PicturesLocation, QStringLiteral("Image")},
        {QStandardPaths::MusicLocation, QStringLiteral("Music")},
        {QStandardPaths::MoviesLocation, QStringLiteral("Video")},
        {QStandardPaths::DesktopLocation, QStringLiteral("Monitor")},
    };
    const QString cleaned = QDir::cleanPath(expandedPath);
    const QString home = QDir::cleanPath(QDir::homePath());
    for (const auto &entry : xdgIcons) {
        const QString resolved = QStandardPaths::writableLocation(entry.first);
        if (!resolved.isEmpty() && QDir::cleanPath(resolved) != home && QDir::cleanPath(resolved) == cleaned)
            return entry.second;
    }
    if (cleaned == home)
        return QStringLiteral("Home");
    return QStringLiteral("Folder");
}

QString BookmarkModel::keyForPath(const QString &expandedPath)
{
    const QString home = QDir::homePath();
    if (expandedPath == home)
        return QStringLiteral("~");
    if (expandedPath.startsWith(home + QLatin1Char('/')))
        return QStringLiteral("~") + expandedPath.mid(home.length());
    return expandedPath;
}

QString BookmarkModel::validColorRole(const QString &color)
{
    static const QStringList roles = {
        QStringLiteral("accent"), QStringLiteral("success"), QStringLiteral("warning"),
        QStringLiteral("error"), QStringLiteral("subtext"), QStringLiteral("muted"),
    };
    return roles.contains(color) ? color : QString();
}

void BookmarkModel::applyStyle(Bookmark &bm) const
{
    const QVariantMap style = m_styles.value(bm.key);
    bm.styleIcon = style.value(QStringLiteral("icon")).toString();
    bm.color = validColorRole(style.value(QStringLiteral("color")).toString());
    bm.label = style.value(QStringLiteral("label")).toString();
}

void BookmarkModel::setBookmarks(const QStringList &paths)
{
    QList<Bookmark> updatedBookmarks;
    updatedBookmarks.reserve(paths.size());

    bool unchanged = paths.size() == m_bookmarks.size();
    for (int i = 0; i < paths.size(); ++i) {
        const Bookmark bookmark = makeBookmark(paths.at(i));
        updatedBookmarks.append(bookmark);

        if (unchanged) {
            const Bookmark &current = m_bookmarks.at(i);
            unchanged = current.path == bookmark.path
                        && current.name == bookmark.name
                        && current.icon == bookmark.icon;
        }
    }

    if (unchanged)
        return;

    beginResetModel();
    m_bookmarks = updatedBookmarks;
    endResetModel();
    emit countChanged();
}

QStringList BookmarkModel::paths() const
{
    QStringList result;
    for (const auto &bm : m_bookmarks)
        result.append(bm.key);   // stored as ~/... for portability
    return result;
}

QStringList BookmarkModel::expandedPaths() const
{
    QStringList result;
    for (const auto &bm : m_bookmarks)
        result.append(bm.path);
    return result;
}

void BookmarkModel::addBookmark(const QString &path)
{
    QString expanded = expandPath(path);
    if (containsPath(expanded))
        return;

    int row = m_bookmarks.size();
    beginInsertRows(QModelIndex(), row, row);
    m_bookmarks.append(makeBookmark(expanded));
    endInsertRows();
    emit countChanged();
    emit bookmarksChanged();
}

void BookmarkModel::insertBookmark(const QString &path, int index)
{
    QString expanded = expandPath(path);
    if (containsPath(expanded))
        return;

    int row = qBound(0, index, m_bookmarks.size());
    beginInsertRows(QModelIndex(), row, row);
    m_bookmarks.insert(row, makeBookmark(expanded));
    endInsertRows();
    emit countChanged();
    emit bookmarksChanged();
}

void BookmarkModel::removeBookmark(int index)
{
    if (index < 0 || index >= m_bookmarks.size())
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_bookmarks.removeAt(index);
    endRemoveRows();
    emit countChanged();
    emit bookmarksChanged();
}

void BookmarkModel::moveBookmark(int from, int to)
{
    if (from < 0 || from >= m_bookmarks.size() ||
        to < 0 || to >= m_bookmarks.size() || from == to)
        return;

    // QAbstractListModel::beginMoveRows requires special handling when moving down
    int destRow = (to > from) ? to + 1 : to;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), destRow))
        return;
    m_bookmarks.move(from, to);
    endMoveRows();
    emit bookmarksChanged();
}

bool BookmarkModel::containsPath(const QString &path) const
{
    QString expanded = expandPath(path);
    for (const auto &bm : m_bookmarks) {
        if (bm.path == expanded)
            return true;
    }
    return false;
}

BookmarkModel::Bookmark BookmarkModel::makeBookmark(const QString &rawPath) const
{
    QString expanded = expandPath(rawPath);
    QString name = bookmarkDisplayName(expanded);
    Bookmark bm;
    bm.name = name;
    bm.path = expanded;
    bm.icon = iconForPath(expanded, name.toLower());
    bm.key = keyForPath(expanded);
    bm.remote = isRemoteUri(expanded);
    applyStyle(bm);
    return bm;
}

QString BookmarkModel::expandPath(const QString &path)
{
    const QUrl url(path);
    if (url.isValid() && !url.scheme().isEmpty() && url.scheme() != QStringLiteral("file"))
        return url.toString(QUrl::FullyEncoded);
    if (path.startsWith("~/"))
        return QDir::homePath() + path.mid(1);
    return path;
}

QString BookmarkModel::iconForPath(const QString &path, const QString &name)
{
    // Match the XDG user dirs first: on a localised system the folder is named
    // "Imagens" or "Documentos", which the English name table below misses.
    static const QVector<QPair<QStandardPaths::StandardLocation, QString>> xdgIcons = {
        {QStandardPaths::HomeLocation, QStringLiteral("go-home")},
        {QStandardPaths::DocumentsLocation, QStringLiteral("folder-documents")},
        {QStandardPaths::DownloadLocation, QStringLiteral("folder-download")},
        {QStandardPaths::PicturesLocation, QStringLiteral("folder-pictures")},
        {QStandardPaths::MusicLocation, QStringLiteral("folder-music")},
        {QStandardPaths::MoviesLocation, QStringLiteral("folder-videos")},
        {QStandardPaths::DesktopLocation, QStringLiteral("user-desktop")},
    };

    if (!path.isEmpty()) {
        const QString cleaned = QDir::cleanPath(path);
        for (const auto &entry : xdgIcons) {
            const QString resolved = QStandardPaths::writableLocation(entry.first);
            if (!resolved.isEmpty() && QDir::cleanPath(resolved) == cleaned)
                return entry.second;
        }
    }

    static const QMap<QString, QString> icons = {
        {"home", "go-home"},
        {"documents", "folder-documents"},
        {"downloads", "folder-download"},
        {"pictures", "folder-pictures"},
        {"music", "folder-music"},
        {"videos", "folder-videos"},
        {"desktop", "user-desktop"},
        {"projects", "folder-development"},
        {"sftp", "folder-remote"},
        {"smb", "folder-remote"},
        {"ftp", "folder-remote"},
        {"network", "folder-remote"},
    };
    return icons.value(name, "folder");
}
