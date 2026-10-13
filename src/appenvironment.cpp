#include "appenvironment.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <functional>

namespace {

QString firstExistingDir(const QStringList &paths)
{
    for (const QString &path : paths) {
        const QString cleanPath = QDir::cleanPath(path);
        if (QDir(cleanPath).exists())
            return cleanPath;
    }
    return QString();
}

// One-time migration from the pre-fork HyprFM config location so existing
// users keep their settings, bookmarks and per-folder sort order when they
// switch to Seraph. Only runs when Seraph has no config of its own yet.
void migrateLegacyConfig(const QString &configDir)
{
    if (QDir(configDir).exists())
        return;
    const QString legacyConfigDir =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.config/hyprfm";
    if (!QDir(legacyConfigDir).exists())
        return;
    std::function<void(const QString &, const QString &)> copyDir =
        [&](const QString &from, const QString &to) {
            QDir().mkpath(to);
            const QDir src(from);
            const auto entries = src.entryInfoList(
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
            for (const QFileInfo &fi : entries) {
                const QString target = to + '/' + fi.fileName();
                if (fi.isDir())
                    copyDir(fi.absoluteFilePath(), target);
                else
                    QFile::copy(fi.absoluteFilePath(), target);
            }
        };
    copyDir(legacyConfigDir, configDir);
    qInfo() << "Seraph: migrated config from" << legacyConfigDir << "to" << configDir;
}

} // namespace

AppEnvironment resolveAppEnvironment()
{
    AppEnvironment env;
    env.configDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                    + "/.config/seraph";
    migrateLegacyConfig(env.configDir);
    QDir().mkpath(env.configDir);
    env.configPath = env.configDir + "/config.toml";

    const QString appDir = QCoreApplication::applicationDirPath();
    env.dataDir = firstExistingDir({
        QDir(appDir).filePath("../share/seraph"),
        QDir(appDir).filePath("../../share/seraph"),
        QStringLiteral(SERAPH_DATA_DIR),
        QStringLiteral(SERAPH_SOURCE_DIR),
    });

    QStringList themeSearchPaths = {
        QDir(appDir).filePath("../themes"),
        QDir(appDir).filePath("../../themes"),
        QStringLiteral(SERAPH_DATA_DIR) + "/themes",
        QStringLiteral(SERAPH_SOURCE_DIR) + "/themes",
    };
    if (!env.dataDir.isEmpty())
        themeSearchPaths.prepend(QDir(env.dataDir).filePath("themes"));

    const QString bundledThemesDir = firstExistingDir(themeSearchPaths);
    if (env.dataDir.isEmpty())
        qWarning() << "Seraph: unable to locate data directory";
    if (bundledThemesDir.isEmpty())
        qWarning() << "Seraph: unable to locate themes directory";

    // The user's own themes dir comes first so a theme dropped there shadows a
    // bundled one of the same name — and so a system themer can generate one.
    // Listed unconditionally: a themer may create it while Seraph is running,
    // and lookups check each candidate file anyway.
    env.themeDirs = QStringList{env.configDir + "/themes"};
    if (!bundledThemesDir.isEmpty())
        env.themeDirs.append(bundledThemesDir);
    return env;
}

QFont resolveUiFont(const QString &preferredFamily)
{
    // Resolve the platform UI font first so the app does not depend on
    // theme-local font defaults that may not exist inside a sandbox.
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    if (font.family().isEmpty())
        font = QGuiApplication::font();

    if (!preferredFamily.trimmed().isEmpty())
        font.setFamily(preferredFamily.trimmed());

    font.setHintingPreference(QFont::PreferFullHinting);
    return font;
}

void addSeraphImportPaths(QQmlApplicationEngine &engine, const QString &dataDir)
{
    // Prefer the installed data layout, but keep source-tree fallbacks for dev builds.
    if (!dataDir.isEmpty()) {
        engine.addImportPath(dataDir);                           // Seraph module
        engine.addImportPath(QDir(dataDir).filePath("src/qml")); // Quill module
    }
    engine.addImportPath(QStringLiteral(SERAPH_DATA_DIR));
    engine.addImportPath(QStringLiteral(SERAPH_DATA_DIR "/src/qml"));
    engine.addImportPath(QStringLiteral(SERAPH_SOURCE_DIR));
    engine.addImportPath(QStringLiteral(SERAPH_SOURCE_DIR "/src/qml"));
}
