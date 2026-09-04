#include "services/configmanager.h"

#define TOML_HEADER_ONLY 1
#include "third_party/toml.hpp"

#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <fstream>

namespace {

struct ShortcutSpec {
    const char *action;
    const char *label;
};

const ShortcutSpec kShortcutSpecs[] = {
    {"open", "Open"},
    {"back", "Back"},
    {"forward", "Forward"},
    {"parent", "Go to Parent"},
    {"home", "Home"},
    {"refresh", "Refresh"},
    {"new_tab", "New Tab"},
    {"close_tab", "Close Tab"},
    {"reopen_tab", "Reopen Closed Tab"},
    {"open_in_new_tab", "Open in New Tab"},
    {"open_in_split", "Open in Split View"},
    {"copy", "Copy"},
    {"cut", "Cut"},
    {"paste", "Paste"},
    {"rename", "Rename"},
    {"new_folder", "New Folder"},
    {"new_file", "New File"},
    {"trash", "Move to Trash"},
    {"permanent_delete", "Permanent Delete"},
    {"toggle_hidden", "Toggle Hidden Files"},
    {"quick_preview", "Quick Preview"},
    {"search", "Search"},
    {"quick_open", "Quick Open (fuzzy find)"},
    {"context_menu", "Show Context Menu"},
    {"open_terminal", "Open in Terminal"},
    {"properties", "Properties"},
    {"path_bar", "Focus Path Bar"},
    {"toggle_sidebar", "Toggle Sidebar"},
    {"split_view", "Toggle Split View"},
    {"focus_next_pane", "Focus Next Pane"},
    {"focus_previous_pane", "Focus Previous Pane"},
    {"focus_left_pane", "Focus Left Pane"},
    {"focus_right_pane", "Focus Right Pane"},
    {"grid_view", "Grid View"},
    {"miller_view", "Miller View"},
    {"detailed_view", "Detailed View"},
    {"select_all", "Select All"},
    {"undo", "Undo"},
    {"redo", "Redo"},
    {"settings", "Open Settings"},
    {"keyboard_shortcuts", "Open Keyboard Shortcuts"},
};

QStringList iconSearchDirs()
{
    QStringList searchDirs;
    const QString home = QDir::homePath();
    searchDirs.append(home + "/.icons");
    searchDirs.append(home + "/.local/share/icons");
    searchDirs.append("/usr/share/icons");
    searchDirs.append("/usr/local/share/icons");

    const QString xdgDirs = qEnvironmentVariable("XDG_DATA_DIRS", "/usr/share:/usr/local/share");
    for (const QString &dir : xdgDirs.split(':')) {
        const QString iconDir = dir + "/icons";
        if (!searchDirs.contains(iconDir))
            searchDirs.append(iconDir);
    }

    return searchDirs;
}

// Default bookmarks follow the XDG user dirs, so they keep working on systems
// where the folders are localised (Documentos, Imagens, ...). Entries that
// don't resolve to a real directory are dropped instead of shipping a
// bookmark that opens an empty view.
QStringList defaultBookmarkPaths()
{
    QStringList paths = {
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QDir::homePath() + QStringLiteral("/Projects"),
    };

    paths.removeAll(QString());
    paths.removeDuplicates();
    paths.removeIf([](const QString &path) { return !QFileInfo(path).isDir(); });
    return paths;
}

} // namespace

QMap<QString, QString> ConfigManager::s_defaultShortcuts = {
    {"open", "Return"},
    {"back", "Alt+Left"},
    {"forward", "Alt+Right"},
    {"parent", "Alt+Up"},
    {"home", "Alt+Home"},
    {"refresh", "F5"},
    {"new_tab", "Ctrl+T"},
    {"close_tab", "Ctrl+W"},
    {"reopen_tab", "Ctrl+Shift+T"},
    {"open_in_new_tab", "Ctrl+Return"},
    {"open_in_split", "Ctrl+Shift+Return"},
    {"copy", "Ctrl+C"},
    {"cut", "Ctrl+X"},
    {"paste", "Ctrl+V"},
    {"rename", "F2"},
    {"new_folder", "Ctrl+Shift+N"},
    {"new_file", "Ctrl+N"},
    {"trash", "Delete"},
    {"permanent_delete", "Shift+Delete"},
    {"toggle_hidden", "Ctrl+H"},
    {"quick_preview", "Space"},
    {"search", "Ctrl+F"},
    {"quick_open", "Ctrl+P"},
    {"context_menu", "Shift+F10"},
    {"open_terminal", "Ctrl+Alt+T"},
    {"properties", "Alt+Return"},
    {"path_bar", "Ctrl+L"},
    {"toggle_sidebar", "F9"},
    {"split_view", "F3"},
    {"focus_next_pane", "F6"},
    {"focus_previous_pane", "Shift+F6"},
    {"focus_left_pane", "Ctrl+Alt+Left"},
    {"focus_right_pane", "Ctrl+Alt+Right"},
    {"grid_view", "Ctrl+1"},
    {"miller_view", "Ctrl+2"},
    {"detailed_view", "Ctrl+3"},
    {"select_all", "Ctrl+A"},
    {"undo", "Ctrl+Z"},
    {"redo", "Ctrl+Shift+Z"},
    {"settings", "Ctrl+,"},
    {"keyboard_shortcuts", "Ctrl+?"},
};

ConfigManager::ConfigManager(const QString &configPath, QObject *parent,
                             const QStringList &themeDirs, const QString &defaultTheme)
    : QObject(parent)
    , m_configPath(configPath)
    , m_themeDirs(themeDirs)
    , m_defaultThemeName(defaultTheme)
{
    setDefaults();
    loadConfig();
    loadFolderSort();

    if (QFile::exists(m_configPath)) {
        m_watcher.addPath(m_configPath);
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
            loadConfig();
            if (QFile::exists(m_configPath))
                m_watcher.addPath(m_configPath);
            emit configChanged();
        });
    }
}

QStringList ConfigManager::availableThemes() const
{
    // Same name in two directories is one theme: ThemeLoader resolves it from
    // the first search path that has it, so listing it twice would be a lie.
    QStringList themes;
    for (const QString &themeDir : m_themeDirs) {
        if (themeDir.isEmpty())
            continue;

        QDir dir(themeDir);
        const QStringList files =
            dir.entryList({"*.toml"}, QDir::Files, QDir::Name | QDir::IgnoreCase);
        for (const QString &fileName : files) {
            const QString name = QFileInfo(fileName).completeBaseName();
            if (!themes.contains(name))
                themes.append(name);
        }
    }
    themes.sort(Qt::CaseInsensitive);
    return themes;
}

QStringList ConfigManager::availableFonts() const
{
    QStringList fonts = QFontDatabase().families();
    fonts.removeDuplicates();
    fonts.sort(Qt::CaseInsensitive);
    return fonts;
}

QStringList ConfigManager::availableIconThemes() const
{
    QStringList themes;
    for (const QString &baseDir : iconSearchDirs()) {
        QDir dir(baseDir);
        const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (!QFile::exists(entry.filePath() + "/index.theme"))
                continue;

            const QString name = entry.fileName();
            if (!themes.contains(name))
                themes.append(name);
        }
    }

    themes.sort(Qt::CaseInsensitive);
    return themes;
}

void ConfigManager::setDefaults()
{
    m_theme = m_defaultThemeName.trimmed().isEmpty()
        ? QStringLiteral("catppuccin-mocha")
        : m_defaultThemeName.trimmed();
    m_lightTheme = QStringLiteral("catppuccin-latte");
    m_darkTheme = QStringLiteral("catppuccin-mocha");
    m_iconTheme = "Adwaita";
    m_builtinIcons = true;
    m_folderIconTint = QStringLiteral("off");
    m_fontFamily.clear();
    m_defaultView = "grid";
    m_startupLocation = "last";
    m_showHidden = false;
    m_sortBy = "name";
    m_sortAscending = true;
    m_rememberSortPerFolder = true;
    m_sidebarPosition = "left";
    m_sidebarWidth = 200;
    m_sidebarVisible = true;
    m_previewInfoWidth = 300;
    m_previewInfoVisible = true;
    m_millerParentFraction = 0.2;
    m_millerCurrentFraction = 0.5;
    m_bookmarks = defaultBookmarkPaths();
    m_radiusSmall = 4;
    m_radiusMedium = 8;
    m_radiusLarge = 12;
    m_transparencyEnabled = true;
    m_transparencyLevel = 1.0;
    m_animationsEnabled = true;
    m_animDurationFast = 100;
    m_animDuration = 200;
    m_animDurationSlow = 350;
    m_animCurveEnter = QStringLiteral("OutCubic");
    m_animCurveExit = QStringLiteral("InCubic");
    m_animCurveTransition = QStringLiteral("Bezier");
    m_showWindowControls = false;  // overridden by runtime detection
    m_showWindowControlsExplicit = false;
    m_windowButtonLayout = QStringLiteral(":minimize,maximize,close");
    m_shortcuts = s_defaultShortcuts;
}

void ConfigManager::loadConfig()
{
    if (!QFile::exists(m_configPath))
        return;

    try {
        m_fontFamily.clear();
        m_transparencyEnabled = true;
        m_transparencyLevel = 1.0;
        m_animationsEnabled = true;
        m_animDurationFast = 100;
        m_animDuration = 200;
        m_animDurationSlow = 350;
        m_animCurveEnter = QStringLiteral("OutCubic");
        m_animCurveExit = QStringLiteral("InCubic");
        m_animCurveTransition = QStringLiteral("Bezier");
        m_showWindowControlsExplicit = false;
        m_shortcuts = s_defaultShortcuts;

        auto config = toml::parse_file(m_configPath.toStdString());

        if (auto v = config["general"]["theme"].value<std::string>())
            m_theme = QString::fromStdString(*v);
        if (auto v = config["general"]["light_theme"].value<std::string>())
            m_lightTheme = QString::fromStdString(*v);
        if (auto v = config["general"]["dark_theme"].value<std::string>())
            m_darkTheme = QString::fromStdString(*v);
        if (auto v = config["general"]["icon_theme"].value<std::string>())
            m_iconTheme = QString::fromStdString(*v);
        if (auto v = config["general"]["builtin_icons"].value<bool>())
            m_builtinIcons = *v;
        if (auto v = config["general"]["folder_icon_tint"].value<std::string>())
            m_folderIconTint = QString::fromStdString(*v);
        if (auto v = config["general"]["font_family"].value<std::string>())
            m_fontFamily = QString::fromStdString(*v);
        if (auto v = config["general"]["default_view"].value<std::string>())
            m_defaultView = QString::fromStdString(*v);
        if (auto v = config["general"]["startup_location"].value<std::string>())
            m_startupLocation = QString::fromStdString(*v);
        if (auto v = config["general"]["show_hidden"].value<bool>())
            m_showHidden = *v;
        if (auto v = config["general"]["sort_by"].value<std::string>())
            m_sortBy = QString::fromStdString(*v);
        if (auto v = config["general"]["sort_ascending"].value<bool>())
            m_sortAscending = *v;
        if (auto v = config["general"]["remember_sort_per_folder"].value<bool>())
            m_rememberSortPerFolder = *v;

        if (auto v = config["sidebar"]["position"].value<std::string>())
            m_sidebarPosition = QString::fromStdString(*v);
        if (auto v = config["sidebar"]["width"].value<int64_t>())
            m_sidebarWidth = static_cast<int>(*v);
        if (auto v = config["sidebar"]["visible"].value<bool>())
            m_sidebarVisible = *v;

        if (auto v = config["preview"]["info_width"].value<int64_t>())
            m_previewInfoWidth = qBound(220, static_cast<int>(*v), 520);
        if (auto v = config["preview"]["info_visible"].value<bool>())
            m_previewInfoVisible = *v;

        if (auto v = config["miller"]["parent_fraction"].value<double>())
            m_millerParentFraction = qBound(0.1, *v, 0.6);
        if (auto v = config["miller"]["current_fraction"].value<double>())
            m_millerCurrentFraction = qBound(0.15, *v, 0.7);

        // Appearance
        if (auto v = config["appearance"]["radius_small"].value<int64_t>())
            m_radiusSmall = static_cast<int>(*v);
        if (auto v = config["appearance"]["radius_medium"].value<int64_t>())
            m_radiusMedium = static_cast<int>(*v);
        if (auto v = config["appearance"]["radius_large"].value<int64_t>())
            m_radiusLarge = static_cast<int>(*v);
        if (auto v = config["appearance"]["transparency_enabled"].value<bool>())
            m_transparencyEnabled = *v;
        if (auto v = config["appearance"]["transparency_level"].value<double>())
            m_transparencyLevel = qBound(0.0, *v, 1.0);
        if (auto v = config["appearance"]["animations_enabled"].value<bool>())
            m_animationsEnabled = *v;
        if (auto v = config["appearance"]["anim_duration_fast"].value<int64_t>())
            m_animDurationFast = qBound(0, static_cast<int>(*v), 1000);
        if (auto v = config["appearance"]["anim_duration"].value<int64_t>())
            m_animDuration = qBound(0, static_cast<int>(*v), 2000);
        if (auto v = config["appearance"]["anim_duration_slow"].value<int64_t>())
            m_animDurationSlow = qBound(0, static_cast<int>(*v), 3000);
        if (auto v = config["appearance"]["anim_curve_enter"].value<std::string>())
            m_animCurveEnter = QString::fromStdString(*v);
        if (auto v = config["appearance"]["anim_curve_exit"].value<std::string>())
            m_animCurveExit = QString::fromStdString(*v);
        if (auto v = config["appearance"]["anim_curve_transition"].value<std::string>())
            m_animCurveTransition = QString::fromStdString(*v);

        // Window controls
        if (auto v = config["window"]["show_controls"].value<bool>()) {
            m_showWindowControls = *v;
            m_showWindowControlsExplicit = true;
        }
        if (auto v = config["window"]["button_layout"].value<std::string>())
            m_windowButtonLayout = QString::fromStdString(*v);

        if (auto arr = config["bookmarks"]["paths"].as_array()) {
            m_bookmarks.clear();
            for (const auto &item : *arr) {
                if (auto v = item.value<std::string>())
                    m_bookmarks.append(QString::fromStdString(*v));
            }
        }

        m_customContextActions.clear();
        if (auto arr = config["context_menu"]["actions"].as_array()) {
            for (const auto &item : *arr) {
                if (auto tbl = item.as_table()) {
                    QVariantMap action;
                    if (auto v = (*tbl)["name"].value<std::string>())
                        action["name"] = QString::fromStdString(*v);
                    if (auto v = (*tbl)["command"].value<std::string>())
                        action["command"] = QString::fromStdString(*v);
                    if (auto types = (*tbl)["types"].as_array()) {
                        QStringList typeList;
                        for (const auto &t : *types) {
                            if (auto v = t.value<std::string>())
                                typeList.append(QString::fromStdString(*v));
                        }
                        action["types"] = typeList;
                    }
                    m_customContextActions.append(action);
                }
            }
        }

        if (auto tbl = config["shortcuts"].as_table()) {
            for (const auto &[key, val] : *tbl) {
                if (auto v = val.value<std::string>()) {
                    m_shortcuts[QString::fromStdString(std::string(key))] =
                        QString::fromStdString(*v);
                }
            }

            // Migrate the old default new-file shortcut so existing configs
            // pick up Ctrl+N unless the user chose a different custom binding.
            if (m_shortcuts.value(QStringLiteral("new_file")) == QStringLiteral("Ctrl+Alt+N"))
                m_shortcuts[QStringLiteral("new_file")] = s_defaultShortcuts.value(QStringLiteral("new_file"));
        }

    } catch (const toml::parse_error &err) {
        qWarning() << "Config parse error:" << err.what();
    }
}

QString ConfigManager::theme() const { return m_theme; }
QString ConfigManager::lightTheme() const { return m_lightTheme; }
QString ConfigManager::darkTheme() const { return m_darkTheme; }
bool ConfigManager::followSystemTheme() const { return m_theme.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0; }
QString ConfigManager::iconTheme() const { return m_iconTheme; }
bool ConfigManager::builtinIcons() const { return m_builtinIcons; }
QString ConfigManager::folderIconTint() const { return m_folderIconTint; }
QString ConfigManager::fontFamily() const { return m_fontFamily; }
QString ConfigManager::defaultView() const { return m_defaultView; }
QString ConfigManager::startupLocation() const { return m_startupLocation; }
bool ConfigManager::showHidden() const { return m_showHidden; }
QString ConfigManager::sortBy() const { return m_sortBy; }
bool ConfigManager::sortAscending() const { return m_sortAscending; }
bool ConfigManager::rememberSortPerFolder() const { return m_rememberSortPerFolder; }
QString ConfigManager::sidebarPosition() const { return m_sidebarPosition; }
int ConfigManager::sidebarWidth() const { return m_sidebarWidth; }
bool ConfigManager::sidebarVisible() const { return m_sidebarVisible; }
int ConfigManager::previewInfoWidth() const { return m_previewInfoWidth; }
bool ConfigManager::previewInfoVisible() const { return m_previewInfoVisible; }
double ConfigManager::millerParentFraction() const { return m_millerParentFraction; }
double ConfigManager::millerCurrentFraction() const { return m_millerCurrentFraction; }
QStringList ConfigManager::bookmarks() const { return m_bookmarks; }
int ConfigManager::radiusSmall() const { return m_radiusSmall; }
int ConfigManager::radiusMedium() const { return m_radiusMedium; }
int ConfigManager::radiusLarge() const { return m_radiusLarge; }
bool ConfigManager::transparencyEnabled() const { return m_transparencyEnabled; }
double ConfigManager::transparencyLevel() const { return m_transparencyLevel; }
bool ConfigManager::animationsEnabled() const { return m_animationsEnabled; }
int ConfigManager::animDurationFast() const { return m_animDurationFast; }
int ConfigManager::animDuration() const { return m_animDuration; }
int ConfigManager::animDurationSlow() const { return m_animDurationSlow; }
QString ConfigManager::animCurveEnter() const { return m_animCurveEnter; }
QString ConfigManager::animCurveExit() const { return m_animCurveExit; }
QString ConfigManager::animCurveTransition() const { return m_animCurveTransition; }
bool ConfigManager::showWindowControls() const { return m_showWindowControls; }

void ConfigManager::setShowWindowControlsDefault(bool value)
{
    if (!m_showWindowControlsExplicit)
        m_showWindowControls = value;
}

QString ConfigManager::windowButtonLayout() const { return m_windowButtonLayout; }

QVariantMap ConfigManager::shortcutMap() const
{
    QVariantMap shortcuts;
    for (const auto &spec : kShortcutSpecs) {
        const QString action = QString::fromUtf8(spec.action);
        shortcuts.insert(action, m_shortcuts.value(action, s_defaultShortcuts.value(action)));
    }
    return shortcuts;
}

QVariantList ConfigManager::shortcutDefinitions() const
{
    QVariantList definitions;
    definitions.reserve(static_cast<qsizetype>(sizeof(kShortcutSpecs) / sizeof(kShortcutSpecs[0])));

    for (const auto &spec : kShortcutSpecs) {
        const QString action = QString::fromUtf8(spec.action);
        QVariantMap definition;
        definition.insert("action", action);
        definition.insert("label", QString::fromUtf8(spec.label));
        definition.insert("defaultSequence", s_defaultShortcuts.value(action));
        definition.insert("sequence", m_shortcuts.value(action, s_defaultShortcuts.value(action)));
        definitions.append(definition);
    }

    return definitions;
}

QVariantList ConfigManager::customContextActions() const { return m_customContextActions; }

QString ConfigManager::shortcut(const QString &action) const
{
    return m_shortcuts.value(action, s_defaultShortcuts.value(action));
}

void ConfigManager::saveSettings(const QVariantMap &settings)
{
    if (settings.isEmpty())
        return;

    const bool wasWatchingConfig = m_watcher.files().contains(m_configPath);
    if (wasWatchingConfig)
        m_watcher.removePath(m_configPath);

    toml::table config;
    if (QFile::exists(m_configPath)) {
        try {
            config = toml::parse_file(m_configPath.toStdString());
        } catch (...) {}
    }

    toml::table general;
    if (auto existingGeneral = config["general"].as_table())
        general = *existingGeneral;

    if (settings.contains("theme")) {
        const QString theme = settings.value("theme").toString().trimmed();
        if (!theme.isEmpty()) {
            m_theme = theme;
            general.insert_or_assign("theme", theme.toStdString());
        }
    }

    if (settings.contains("lightTheme")) {
        const QString lightTheme = settings.value("lightTheme").toString().trimmed();
        if (!lightTheme.isEmpty()) {
            m_lightTheme = lightTheme;
            general.insert_or_assign("light_theme", lightTheme.toStdString());
        }
    }

    if (settings.contains("darkTheme")) {
        const QString darkTheme = settings.value("darkTheme").toString().trimmed();
        if (!darkTheme.isEmpty()) {
            m_darkTheme = darkTheme;
            general.insert_or_assign("dark_theme", darkTheme.toStdString());
        }
    }

    if (settings.contains("iconTheme")) {
        const QString iconTheme = settings.value("iconTheme").toString().trimmed();
        if (!iconTheme.isEmpty()) {
            m_iconTheme = iconTheme;
            general.insert_or_assign("icon_theme", iconTheme.toStdString());
        }
    }

    if (settings.contains("builtinIcons")) {
        m_builtinIcons = settings.value("builtinIcons").toBool();
        general.insert_or_assign("builtin_icons", m_builtinIcons);
    }

    if (settings.contains("folderIconTint")) {
        const QString tint = settings.value("folderIconTint").toString().trimmed();
        if (!tint.isEmpty()) {
            m_folderIconTint = tint;
            general.insert_or_assign("folder_icon_tint", tint.toStdString());
        }
    }

    if (settings.contains("fontFamily")) {
        m_fontFamily = settings.value("fontFamily").toString().trimmed();
        general.insert_or_assign("font_family", m_fontFamily.toStdString());
    }

    if (settings.contains("showHidden")) {
        m_showHidden = settings.value("showHidden").toBool();
        general.insert_or_assign("show_hidden", m_showHidden);
    }

    if (settings.contains("startupLocation")) {
        const QString loc = settings.value("startupLocation").toString().trimmed();
        if (loc == "home" || loc == "last") {
            m_startupLocation = loc;
            general.insert_or_assign("startup_location", loc.toStdString());
        }
    }

    if (settings.contains("defaultView")) {
        const QString view = settings.value("defaultView").toString().trimmed();
        if (view == "grid" || view == "detailed" || view == "miller") {
            m_defaultView = view;
            general.insert_or_assign("default_view", view.toStdString());
        }
    }

    if (settings.contains("sortBy")) {
        const QString sortBy = settings.value("sortBy").toString().trimmed();
        if (!sortBy.isEmpty()) {
            m_sortBy = sortBy;
            general.insert_or_assign("sort_by", sortBy.toStdString());
        }
    }

    if (settings.contains("sortAscending")) {
        m_sortAscending = settings.value("sortAscending").toBool();
        general.insert_or_assign("sort_ascending", m_sortAscending);
    }

    if (settings.contains("rememberSortPerFolder")) {
        m_rememberSortPerFolder = settings.value("rememberSortPerFolder").toBool();
        general.insert_or_assign("remember_sort_per_folder", m_rememberSortPerFolder);
    }

    if (!general.empty())
        config.insert_or_assign("general", std::move(general));

    toml::table sidebar;
    if (auto existingSidebar = config["sidebar"].as_table())
        sidebar = *existingSidebar;

    if (settings.contains("sidebarVisible")) {
        m_sidebarVisible = settings.value("sidebarVisible").toBool();
        sidebar.insert_or_assign("visible", m_sidebarVisible);
    }

    if (settings.contains("sidebarWidth")) {
        m_sidebarWidth = qBound(160, settings.value("sidebarWidth").toInt(), 480);
        sidebar.insert_or_assign("width", m_sidebarWidth);
    }

    if (settings.contains("sidebarPosition")) {
        const QString pos = settings.value("sidebarPosition").toString().trimmed();
        if (pos == "left" || pos == "right") {
            m_sidebarPosition = pos;
            sidebar.insert_or_assign("position", pos.toStdString());
        }
    }

    if (!sidebar.empty())
        config.insert_or_assign("sidebar", std::move(sidebar));

    if (settings.contains("previewInfoWidth") || settings.contains("previewInfoVisible")) {
        toml::table preview;
        if (auto existingPreview = config["preview"].as_table())
            preview = *existingPreview;

        if (settings.contains("previewInfoWidth")) {
            m_previewInfoWidth = qBound(220, settings.value("previewInfoWidth").toInt(), 520);
            preview.insert_or_assign("info_width", m_previewInfoWidth);
        }
        if (settings.contains("previewInfoVisible")) {
            m_previewInfoVisible = settings.value("previewInfoVisible").toBool();
            preview.insert_or_assign("info_visible", m_previewInfoVisible);
        }
        config.insert_or_assign("preview", std::move(preview));
    }

    if (settings.contains("millerParentFraction") || settings.contains("millerCurrentFraction")) {
        toml::table miller;
        if (auto existingMiller = config["miller"].as_table())
            miller = *existingMiller;

        if (settings.contains("millerParentFraction")) {
            m_millerParentFraction = qBound(0.1, settings.value("millerParentFraction").toDouble(), 0.6);
            miller.insert_or_assign("parent_fraction", m_millerParentFraction);
        }
        if (settings.contains("millerCurrentFraction")) {
            m_millerCurrentFraction = qBound(0.15, settings.value("millerCurrentFraction").toDouble(), 0.7);
            miller.insert_or_assign("current_fraction", m_millerCurrentFraction);
        }

        if (!miller.empty())
            config.insert_or_assign("miller", std::move(miller));
    }

    const bool updatesAppearance = settings.contains("radiusSmall")
        || settings.contains("radiusMedium")
        || settings.contains("radiusLarge")
        || settings.contains("transparencyEnabled")
        || settings.contains("transparencyLevel")
        || settings.contains("animationsEnabled")
        || settings.contains("animDurationFast")
        || settings.contains("animDuration")
        || settings.contains("animDurationSlow")
        || settings.contains("animCurveEnter")
        || settings.contains("animCurveExit")
        || settings.contains("animCurveTransition");
    if (updatesAppearance) {
        int radiusSmall = settings.contains("radiusSmall")
            ? qMax(0, settings.value("radiusSmall").toInt())
            : m_radiusSmall;
        int radiusMedium = settings.contains("radiusMedium")
            ? qMax(0, settings.value("radiusMedium").toInt())
            : m_radiusMedium;
        int radiusLarge = settings.contains("radiusLarge")
            ? qMax(0, settings.value("radiusLarge").toInt())
            : m_radiusLarge;

        radiusMedium = qMax(radiusMedium, radiusSmall);
        radiusLarge = qMax(radiusLarge, radiusMedium);

        m_radiusSmall = radiusSmall;
        m_radiusMedium = radiusMedium;
        m_radiusLarge = radiusLarge;
        m_transparencyEnabled = settings.contains("transparencyEnabled")
            ? settings.value("transparencyEnabled").toBool()
            : m_transparencyEnabled;
        m_transparencyLevel = settings.contains("transparencyLevel")
            ? qBound(0.0, settings.value("transparencyLevel").toDouble(), 1.0)
            : m_transparencyLevel;
        m_animationsEnabled = settings.contains("animationsEnabled")
            ? settings.value("animationsEnabled").toBool()
            : m_animationsEnabled;

        toml::table appearance;
        if (auto existingAppearance = config["appearance"].as_table())
            appearance = *existingAppearance;

        appearance.insert_or_assign("radius_small", m_radiusSmall);
        appearance.insert_or_assign("radius_medium", m_radiusMedium);
        appearance.insert_or_assign("radius_large", m_radiusLarge);
        appearance.insert_or_assign("transparency_enabled", m_transparencyEnabled);
        appearance.insert_or_assign("transparency_level", m_transparencyLevel);
        appearance.insert_or_assign("animations_enabled", m_animationsEnabled);

        if (settings.contains("animDurationFast"))
            m_animDurationFast = qBound(0, settings.value("animDurationFast").toInt(), 1000);
        if (settings.contains("animDuration"))
            m_animDuration = qBound(0, settings.value("animDuration").toInt(), 2000);
        if (settings.contains("animDurationSlow"))
            m_animDurationSlow = qBound(0, settings.value("animDurationSlow").toInt(), 3000);
        if (settings.contains("animCurveEnter"))
            m_animCurveEnter = settings.value("animCurveEnter").toString().trimmed();
        if (settings.contains("animCurveExit"))
            m_animCurveExit = settings.value("animCurveExit").toString().trimmed();
        if (settings.contains("animCurveTransition"))
            m_animCurveTransition = settings.value("animCurveTransition").toString().trimmed();

        appearance.insert_or_assign("anim_duration_fast", m_animDurationFast);
        appearance.insert_or_assign("anim_duration", m_animDuration);
        appearance.insert_or_assign("anim_duration_slow", m_animDurationSlow);
        appearance.insert_or_assign("anim_curve_enter", m_animCurveEnter.toStdString());
        appearance.insert_or_assign("anim_curve_exit", m_animCurveExit.toStdString());
        appearance.insert_or_assign("anim_curve_transition", m_animCurveTransition.toStdString());
        config.insert_or_assign("appearance", std::move(appearance));
    }

    // Window controls
    const bool updatesWindow = settings.contains("showWindowControls")
        || settings.contains("windowButtonLayout");
    if (updatesWindow) {
        toml::table windowTbl;
        if (auto existingWindow = config["window"].as_table())
            windowTbl = *existingWindow;

        if (settings.contains("showWindowControls")) {
            m_showWindowControls = settings.value("showWindowControls").toBool();
            m_showWindowControlsExplicit = true;
            windowTbl.insert_or_assign("show_controls", m_showWindowControls);
        }

        if (settings.contains("windowButtonLayout")) {
            m_windowButtonLayout = settings.value("windowButtonLayout").toString().trimmed();
            windowTbl.insert_or_assign("button_layout", m_windowButtonLayout.toStdString());
        }

        if (!windowTbl.empty())
            config.insert_or_assign("window", std::move(windowTbl));
    }

    std::ofstream ofs(m_configPath.toStdString());
    if (ofs.is_open()) {
        ofs << config;
        ofs.close();
    }

    if (QFile::exists(m_configPath))
        m_watcher.addPath(m_configPath);

    emit configChanged();
}

QString ConfigManager::folderSortStorePath() const
{
    return QFileInfo(m_configPath).dir().filePath(QStringLiteral("folder_sort.json"));
}

void ConfigManager::loadFolderSort()
{
    m_folderSort.clear();

    QFile f(folderSortStorePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;

    bool pruned = false;
    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString path = it.key();
        // Prune stale local paths that no longer exist. Non-local locations
        // (trash:/, sftp://, …) are kept since they can't be stat'd.
        if (path.startsWith(QLatin1Char('/')) && !QFileInfo::exists(path)) {
            pruned = true;
            continue;
        }
        const QJsonObject entry = it.value().toObject();
        m_folderSort.insert(path, QVariantMap{
            {QStringLiteral("by"), entry.value(QStringLiteral("by")).toString(QStringLiteral("name"))},
            {QStringLiteral("ascending"), entry.value(QStringLiteral("ascending")).toBool(true)},
        });
    }

    if (pruned)
        saveFolderSort();
}

void ConfigManager::saveFolderSort() const
{
    QJsonObject obj;
    for (auto it = m_folderSort.constBegin(); it != m_folderSort.constEnd(); ++it) {
        obj.insert(it.key(), QJsonObject{
            {QStringLiteral("by"), it.value().value(QStringLiteral("by")).toString()},
            {QStringLiteral("ascending"), it.value().value(QStringLiteral("ascending")).toBool()},
        });
    }

    QSaveFile f(folderSortStorePath());
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

QString ConfigManager::folderSortBy(const QString &path) const
{
    if (m_rememberSortPerFolder && m_folderSort.contains(path))
        return m_folderSort.value(path).value(QStringLiteral("by")).toString();
    return m_sortBy;
}

bool ConfigManager::folderSortAscending(const QString &path) const
{
    if (m_rememberSortPerFolder && m_folderSort.contains(path))
        return m_folderSort.value(path).value(QStringLiteral("ascending")).toBool();
    return m_sortAscending;
}

void ConfigManager::setFolderSort(const QString &path, const QString &sortBy,
                                  bool ascending)
{
    if (path.isEmpty())
        return;

    m_folderSort.insert(path, QVariantMap{
        {QStringLiteral("by"), sortBy},
        {QStringLiteral("ascending"), ascending},
    });
    saveFolderSort();
}

void ConfigManager::saveShortcuts(const QVariantMap &shortcuts)
{
    const bool wasWatchingConfig = m_watcher.files().contains(m_configPath);
    if (wasWatchingConfig)
        m_watcher.removePath(m_configPath);

    toml::table config;
    if (QFile::exists(m_configPath)) {
        try {
            config = toml::parse_file(m_configPath.toStdString());
        } catch (...) {}
    }

    m_shortcuts = s_defaultShortcuts;

    toml::table shortcutTable;
    for (const auto &spec : kShortcutSpecs) {
        const QString action = QString::fromUtf8(spec.action);
        const QString defaultSequence = s_defaultShortcuts.value(action);
        const QString sequence = shortcuts.value(action, defaultSequence).toString().trimmed();

        if (sequence.isEmpty())
            continue;

        m_shortcuts[action] = sequence;
        if (sequence != defaultSequence)
            shortcutTable.insert_or_assign(action.toStdString(), sequence.toStdString());
    }

    config.insert_or_assign("shortcuts", std::move(shortcutTable));

    std::ofstream ofs(m_configPath.toStdString());
    if (ofs.is_open()) {
        ofs << config;
        ofs.close();
    }

    if (QFile::exists(m_configPath))
        m_watcher.addPath(m_configPath);

    emit configChanged();
}

void ConfigManager::saveBookmarks(const QStringList &paths)
{
    m_bookmarks = paths;

    const bool wasWatchingConfig = m_watcher.files().contains(m_configPath);
    if (wasWatchingConfig)
        m_watcher.removePath(m_configPath);

    // Read existing config or create new
    toml::table config;
    if (QFile::exists(m_configPath)) {
        try {
            config = toml::parse_file(m_configPath.toStdString());
        } catch (...) {}
    }

    // Update bookmarks array
    toml::array arr;
    for (const auto &p : paths)
        arr.push_back(p.toStdString());
    config.insert_or_assign("bookmarks", toml::table{{"paths", std::move(arr)}});

    // Write back
    std::ofstream ofs(m_configPath.toStdString());
    if (ofs.is_open()) {
        ofs << config;
        ofs.close();
    }

    if (QFile::exists(m_configPath))
        m_watcher.addPath(m_configPath);
}

void ConfigManager::saveSidebarWidth(int width)
{
    saveSettings(QVariantMap{{"sidebarWidth", width}});
}

void ConfigManager::savePreviewInfoPanel(int width, bool visible)
{
    saveSettings(QVariantMap{{"previewInfoWidth", width}, {"previewInfoVisible", visible}});
}

void ConfigManager::saveMillerColumns(double parentFraction, double currentFraction)
{
    saveSettings(QVariantMap{
        {"millerParentFraction", parentFraction},
        {"millerCurrentFraction", currentFraction},
    });
}
