#pragma once

#include <QFont>
#include <QString>
#include <QStringList>

class QQmlApplicationEngine;

// Paths and setup shared by the file manager and the `--view` reader, so the
// two entry points find the same config, themes and QML modules.
struct AppEnvironment {
    QString configDir;     // ~/.config/seraph (created; HyprFM config migrated once)
    QString configPath;    // configDir + "/config.toml"
    QString dataDir;       // installed data dir, or the source tree in dev builds
    QStringList themeDirs; // the user's themes first, then the bundled ones
};

AppEnvironment resolveAppEnvironment();

// The platform UI font, with the configured family when one is set.
QFont resolveUiFont(const QString &preferredFamily);

void addSeraphImportPaths(QQmlApplicationEngine &engine, const QString &dataDir);
