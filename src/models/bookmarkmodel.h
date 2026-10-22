#pragma once

#include <QAbstractListModel>
#include <QStringList>

class BookmarkModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,   // label when set, else the folder name
        PathRole,
        IconRole,                      // freedesktop icon-theme name (legacy)
        LabelRole,                     // raw label, "" when unset
        QuillIconRole,                 // built-in icon name (style or default)
        ColorRoleRole,                 // theme role name or ""
        IsRemoteRole,
        RemoteHintRole,                // "scheme · host" for remote bookmarks
        KeyRole,                       // config key: path as written in paths()
    };

    explicit BookmarkModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setBookmarks(const QStringList &paths);
    QStringList paths() const;

    // Styles keyed like ConfigManager::bookmarkStyles(); unknown colour roles
    // degrade to none. setStyle() is the user's edit and is reported through
    // styleChanged() so the config can be written; setStyles() is the load.
    void setStyles(const QVariantMap &styles);
    Q_INVOKABLE QVariantMap styleFor(int index) const;
    Q_INVOKABLE void setStyle(int index, const QString &icon, const QString &color,
                              const QString &label);
    static QString defaultQuillIcon(const QString &expandedPath);

    Q_INVOKABLE void addBookmark(const QString &path);
    Q_INVOKABLE void insertBookmark(const QString &path, int index);
    Q_INVOKABLE void removeBookmark(int index);
    Q_INVOKABLE void moveBookmark(int from, int to);
    Q_INVOKABLE bool containsPath(const QString &path) const;

signals:
    void countChanged();
    void bookmarksChanged();
    void styleChanged(const QString &key, const QString &icon, const QString &color,
                      const QString &label);

private:
    struct Bookmark {
        QString name;        // folder display name
        QString path;        // expanded
        QString icon;        // legacy freedesktop icon name
        QString key;         // ~-shortened path, the config key
        QString styleIcon;   // "" when no style
        QString color;       // validated theme role or ""
        QString label;       // "" when no label
        bool remote = false;
    };

    QList<Bookmark> m_bookmarks;
    QMap<QString, QVariantMap> m_styles;   // keyed by Bookmark::key
    static QString expandPath(const QString &path);
    static QString keyForPath(const QString &expandedPath);
    static QString validColorRole(const QString &color);
    static QString iconForPath(const QString &path, const QString &name);
    void applyStyle(Bookmark &bookmark) const;
    Bookmark makeBookmark(const QString &path) const;
};
