#include "services/themeloader.h"
#define TOML_HEADER_ONLY 1
#include "third_party/toml.hpp"
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QTimer>

const QString ThemeLoader::AutoTheme = QStringLiteral("auto");

QMap<QString, QColor> ThemeLoader::s_defaults = {
    {"base", QColor("#1e1e2e")}, {"mantle", QColor("#181825")},
    {"crust", QColor("#11111b")}, {"surface", QColor("#313244")},
    {"overlay", QColor("#45475a")}, {"text", QColor("#cdd6f4")},
    {"subtext", QColor("#bac2de")}, {"muted", QColor("#6c7086")},
    {"accent", QColor("#89b4fa")}, {"success", QColor("#a6e3a1")},
    {"warning", QColor("#f9e2af")}, {"error", QColor("#f38ba8")},
    {"purple", QColor("#cba6f7")},
};

ThemeLoader::ThemeLoader(QObject *parent)
    : QObject(parent)
    , m_colors(s_defaults)
    , m_lightTheme(QStringLiteral("catppuccin-latte"))
    , m_darkTheme(QStringLiteral("catppuccin-mocha"))
{
    // Editors and themers rewrite rather than modify in place, so the watcher
    // drops the path on every save; re-resolving on each hit re-arms it. The
    // delay coalesces the write burst a rewrite produces.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        QTimer::singleShot(50, this, [this]() { applyRequestedTheme(); });
    });
}

void ThemeLoader::setThemeSearchPaths(const QStringList &dirs)
{
    m_themeDirs = dirs;
}

void ThemeLoader::setTheme(const QString &nameOrPath, const QString &lightTheme,
                           const QString &darkTheme)
{
    m_requestedTheme = nameOrPath.trimmed();
    if (!lightTheme.trimmed().isEmpty())
        m_lightTheme = lightTheme.trimmed();
    if (!darkTheme.trimmed().isEmpty())
        m_darkTheme = darkTheme.trimmed();

    if (followSystem() && !m_warnedMissingScheme
        && systemColorScheme() == QLatin1String("unknown")) {
        m_warnedMissingScheme = true;
        qWarning() << "Seraph: no system color scheme reported (is xdg-desktop-portal "
                      "running?); falling back to the dark theme";
    }
    applyRequestedTheme();
}

void ThemeLoader::applyRequestedTheme()
{
    loadTheme(resolvedThemeName(), QString());
}

QString ThemeLoader::resolveThemeFile(const QString &nameOrPath) const
{
    if (nameOrPath.isEmpty())
        return QString();
    if (QFile::exists(nameOrPath))
        return nameOrPath;

    // Bare name: first search path that has it wins, so a user theme shadows
    // a bundled one of the same name.
    for (const QString &dir : m_themeDirs) {
        if (dir.isEmpty())
            continue;
        const QString candidate = QDir(dir).filePath(nameOrPath + ".toml");
        if (QFile::exists(candidate))
            return candidate;
    }
    return QString();
}

void ThemeLoader::watchThemeFile(const QString &filePath)
{
    if (!m_watchedFile.isEmpty() && m_watchedFile != filePath)
        m_watcher.removePath(m_watchedFile);

    m_watchedFile = filePath;
    if (filePath.isEmpty() || m_watcher.files().contains(filePath))
        return;
    m_watcher.addPath(filePath);
}

QString ThemeLoader::resolvedThemeName() const
{
    if (!followSystem())
        return m_requestedTheme;
    // "unknown" resolves to dark, matching Seraph's historical default.
    return systemColorScheme() == QLatin1String("light") ? m_lightTheme : m_darkTheme;
}

bool ThemeLoader::followSystem() const
{
    return m_requestedTheme.compare(AutoTheme, Qt::CaseInsensitive) == 0;
}

QString ThemeLoader::systemColorScheme() const
{
    switch (m_systemScheme) {
    case Qt::ColorScheme::Light:
        return QStringLiteral("light");
    case Qt::ColorScheme::Dark:
        return QStringLiteral("dark");
    default:
        return QStringLiteral("unknown");
    }
}

void ThemeLoader::setSystemColorScheme(Qt::ColorScheme scheme)
{
    if (m_systemScheme == scheme)
        return;

    m_systemScheme = scheme;
    emit systemColorSchemeChanged();
    if (followSystem())
        applyRequestedTheme();
}

void ThemeLoader::loadTheme(const QString &nameOrPath, const QString &themesDir)
{
    if (!themesDir.isEmpty())
        m_themeDirs = QStringList{themesDir};

    m_colors = s_defaults;
    m_activeTheme = nameOrPath;
    const QString filePath = resolveThemeFile(nameOrPath);
    watchThemeFile(filePath);
    if (filePath.isEmpty()) {
        qWarning() << "Theme not found:" << nameOrPath;
        emit themeChanged();
        return;
    }
    try {
        auto config = toml::parse_file(filePath.toStdString());
        if (auto colors = config["colors"].as_table()) {
            for (const auto &[key, val] : *colors) {
                if (auto v = val.value<std::string>()) {
                    QString colorStr = QString::fromStdString(*v);
                    QColor c(colorStr);
                    if (c.isValid())
                        m_colors[QString::fromStdString(std::string(key))] = c;
                }
            }
        }
    } catch (const toml::parse_error &err) {
        qWarning() << "Theme parse error:" << err.what();
    }
    emit themeChanged();
}

QColor ThemeLoader::color(const QString &name) const
{
    return m_colors.value(name, s_defaults.value(name, QColor("#ff00ff")));
}
