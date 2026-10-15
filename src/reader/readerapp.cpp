#include "reader/readerapp.h"

#include "appenvironment.h"
#include "services/configmanager.h"
#include "services/editorlauncher.h"
#include "services/fileoperations.h"
#include "services/imagefetcher.h"
#include "services/languagemanager.h"
#include "services/markdownrenderer.h"
#include "services/readerdocument.h"
#include "services/runtimefeaturesservice.h"
#include "services/systemappearance.h"
#include "services/themeloader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

int runReader(QGuiApplication &app, const QString &path, bool timingEnabled,
              const std::function<void(const char *)> &mark)
{
    QQuickStyle::setStyle("Basic");
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    const AppEnvironment env = resolveAppEnvironment();
    auto *config = new ConfigManager(env.configPath, &app, env.themeDirs, ThemeLoader::AutoTheme);
    auto *languageManager = new LanguageManager(&app);
    languageManager->apply(config->language());
    QObject::connect(config, &ConfigManager::configChanged, languageManager, [config, languageManager]() {
        languageManager->apply(config->language());
    });
    app.setFont(resolveUiFont(config->fontFamily()));
    auto *appearance = new SystemAppearance(&app);
    auto *theme = new ThemeLoader(&app);
    theme->setThemeSearchPaths(env.themeDirs);
    theme->setSystemColorScheme(appearance->colorScheme());
    QObject::connect(appearance, &SystemAppearance::colorSchemeChanged,
                     theme, &ThemeLoader::setSystemColorScheme);
    theme->setTheme(config->theme(), config->lightTheme(), config->darkTheme());
    QObject::connect(config, &ConfigManager::configChanged, &app, [config, theme, &app]() {
        theme->setTheme(config->theme(), config->lightTheme(), config->darkTheme());
        app.setFont(resolveUiFont(config->fontFamily()));
    });
    mark("reader: theme ready");

    auto *document = new ReaderDocument(&app);
    auto *imageFetcher = new ImageFetcher(&app);
    auto *markdownRenderer = new MarkdownRenderer(&app);
    auto *runtimeFeatures = new RuntimeFeaturesService(&app);
    auto *fileOps = new FileOperations(&app);
    auto *editorLauncher = new EditorLauncher(&app);

    // Read before the QML loads, so the first frame already has the document.
    document->open(QFileInfo(path).absoluteFilePath());
    mark("reader: document read");

    QQmlApplicationEngine engine;
    languageManager->setEngine(&engine);
    addSeraphImportPaths(engine, env.dataDir);
    QQmlContext *context = engine.rootContext();
    context->setContextProperty("config", config);
    context->setContextProperty("languageManager", languageManager);
    context->setContextProperty("theme", theme);
    context->setContextProperty("readerDocument", document);
    context->setContextProperty("imageFetcher", imageFetcher);
    context->setContextProperty("markdownRenderer", markdownRenderer);
    context->setContextProperty("runtimeFeatures", runtimeFeatures);
    context->setContextProperty("fileOps", fileOps);
    context->setContextProperty("editorLauncher", editorLauncher);

    const QString installedQml = env.dataDir.isEmpty()
        ? QString()
        : QDir(env.dataDir).filePath(QStringLiteral("Seraph/qml/ReaderMain.qml"));
    if (!installedQml.isEmpty() && QFile::exists(installedQml))
        engine.load(QUrl::fromLocalFile(installedQml));
    else
        engine.loadFromModule("Seraph", "ReaderMain");
    mark("reader: engine.load done");

    if (engine.rootObjects().isEmpty())
        return -1;

    if (timingEnabled) {
        if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            auto *conn = new QMetaObject::Connection;
            *conn = QObject::connect(win, &QQuickWindow::frameSwapped, win, [conn, mark]() {
                mark("reader: first frame swapped");
                QObject::disconnect(*conn);
                delete conn;
            }, Qt::QueuedConnection);
        }
    }

#ifndef NDEBUG
    // Dev aid, left out of Release builds: SERAPH_GRAB=<file.png> saves the
    // window a moment after it is shown and quits, so the reader can be
    // checked offscreen (QT_QPA_PLATFORM=offscreen) without the desktop.
    const QString grabPath = qEnvironmentVariable("SERAPH_GRAB");
    if (!grabPath.isEmpty()) {
        if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            const int delay = qEnvironmentVariableIsSet("SERAPH_GRAB_DELAY")
                ? qEnvironmentVariableIntValue("SERAPH_GRAB_DELAY") : 2500;
            QTimer::singleShot(delay, win, [win, grabPath, &app]() {
                win->grabWindow().save(grabPath);
                app.quit();
            });
        }
    }
#endif

    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, &QGuiApplication::quit);
    return app.exec();
}
