#pragma once
#include <QObject>
#include <QColor>
#include <QFileSystemWatcher>
#include <QMap>
#include <QString>
#include <QStringList>
#include <Qt>

class ThemeLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor base READ base NOTIFY themeChanged)
    Q_PROPERTY(QColor mantle READ mantle NOTIFY themeChanged)
    Q_PROPERTY(QColor crust READ crust NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor overlay READ overlay NOTIFY themeChanged)
    Q_PROPERTY(QColor text READ text NOTIFY themeChanged)
    Q_PROPERTY(QColor subtext READ subtext NOTIFY themeChanged)
    Q_PROPERTY(QColor muted READ muted NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QColor success READ success NOTIFY themeChanged)
    Q_PROPERTY(QColor warning READ warning NOTIFY themeChanged)
    Q_PROPERTY(QColor error READ error NOTIFY themeChanged)
    // Theme file actually in use. Same as the requested name, except when
    // "auto" was requested: then it is whichever of the light/dark pair matches
    // the system color scheme.
    Q_PROPERTY(QString activeTheme READ activeTheme NOTIFY themeChanged)
    Q_PROPERTY(bool followSystem READ followSystem NOTIFY themeChanged)
    // "light", "dark", or "unknown" when nothing reports a preference (no
    // xdg-desktop-portal answering org.freedesktop.appearance).
    Q_PROPERTY(QString systemColorScheme READ systemColorScheme NOTIFY systemColorSchemeChanged)

public:
    // Theme setting value meaning "track the system color scheme".
    static const QString AutoTheme;

    explicit ThemeLoader(QObject *parent = nullptr);
    // Directories searched for a theme referenced by bare name, in order —
    // the user's own themes dir shadows the bundled one.
    void setThemeSearchPaths(const QStringList &dirs);
    void loadTheme(const QString &nameOrPath, const QString &themesDir);
    // Like loadTheme(), but nameOrPath may be AutoTheme; the loader then keeps
    // following the system color scheme until setTheme() is called again.
    // Resolves names against setThemeSearchPaths().
    void setTheme(const QString &nameOrPath, const QString &lightTheme,
                  const QString &darkTheme);
    QColor color(const QString &name) const;
    QColor base() const { return color("base"); }
    QColor mantle() const { return color("mantle"); }
    QColor crust() const { return color("crust"); }
    QColor surface() const { return color("surface"); }
    QColor overlay() const { return color("overlay"); }
    QColor text() const { return color("text"); }
    QColor subtext() const { return color("subtext"); }
    QColor muted() const { return color("muted"); }
    QColor accent() const { return color("accent"); }
    QColor success() const { return color("success"); }
    QColor warning() const { return color("warning"); }
    QColor error() const { return color("error"); }
    QString activeTheme() const { return m_activeTheme; }
    bool followSystem() const;
    QString systemColorScheme() const;
    // Feeds the loader the system light/dark preference. main.cpp drives this
    // from SystemAppearance; tests drive it directly.
    void setSystemColorScheme(Qt::ColorScheme scheme);
signals:
    void themeChanged();
    void systemColorSchemeChanged();
private:
    void applyRequestedTheme();
    QString resolvedThemeName() const;
    QString resolveThemeFile(const QString &nameOrPath) const;
    // Watches the loaded file so editing a theme — or a system themer
    // rewriting one — recolors the running app.
    void watchThemeFile(const QString &filePath);

    QMap<QString, QColor> m_colors;
    QString m_requestedTheme;
    QString m_activeTheme;
    QStringList m_themeDirs;
    QString m_lightTheme;
    QString m_darkTheme;
    Qt::ColorScheme m_systemScheme = Qt::ColorScheme::Unknown;
    bool m_warnedMissingScheme = false;
    QFileSystemWatcher m_watcher;
    QString m_watchedFile;
    static QMap<QString, QColor> s_defaults;
};
