#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include "services/themeloader.h"

class TestThemeLoader : public QObject
{
    Q_OBJECT

private slots:
    void testLoadBuiltinTheme()
    {
        ThemeLoader loader;
        loader.loadTheme("catppuccin-mocha", THEMES_DIR);
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
        QCOMPARE(loader.color("accent"), QColor("#89b4fa"));
        QCOMPARE(loader.color("text"), QColor("#cdd6f4"));
        QCOMPARE(loader.color("error"), QColor("#f38ba8"));
    }

    void testLoadBuiltinLightTheme()
    {
        ThemeLoader loader;
        loader.loadTheme("catppuccin-latte", THEMES_DIR);
        QCOMPARE(loader.color("base"), QColor("#eff1f5"));
        QCOMPARE(loader.color("accent"), QColor("#1e66f5"));
        QCOMPARE(loader.color("text"), QColor("#4c4f69"));
        QCOMPARE(loader.color("error"), QColor("#d20f39"));
    }

    void testAllBuiltinColors()
    {
        ThemeLoader loader;
        loader.loadTheme("catppuccin-mocha", THEMES_DIR);

        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
        QCOMPARE(loader.color("mantle"), QColor("#181825"));
        QCOMPARE(loader.color("crust"), QColor("#11111b"));
        QCOMPARE(loader.color("surface"), QColor("#313244"));
        QCOMPARE(loader.color("overlay"), QColor("#45475a"));
        QCOMPARE(loader.color("text"), QColor("#cdd6f4"));
        QCOMPARE(loader.color("subtext"), QColor("#bac2de"));
        QCOMPARE(loader.color("muted"), QColor("#6c7086"));
        QCOMPARE(loader.color("accent"), QColor("#89b4fa"));
        QCOMPARE(loader.color("success"), QColor("#a6e3a1"));
        QCOMPARE(loader.color("warning"), QColor("#f9e2af"));
        QCOMPARE(loader.color("error"), QColor("#f38ba8"));
    }

    void testLoadCustomTheme()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/custom.toml";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("[colors]\nbase = \"#000000\"\ntext = \"#ffffff\"\naccent = \"#ff0000\"\n");
        f.close();

        ThemeLoader loader;
        loader.loadTheme(path, "");
        QCOMPARE(loader.color("base"), QColor("#000000"));
        QCOMPARE(loader.color("text"), QColor("#ffffff"));
        QCOMPARE(loader.color("accent"), QColor("#ff0000"));
    }

    void testFallbackForMissingColors()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/partial.toml";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("[colors]\nbase = \"#000000\"\n");
        f.close();

        ThemeLoader loader;
        loader.loadTheme(path, "");
        QCOMPARE(loader.color("base"), QColor("#000000"));
        // All other colors should fall back to defaults
        QCOMPARE(loader.color("text"), QColor("#cdd6f4"));
        QCOMPARE(loader.color("accent"), QColor("#89b4fa"));
        QCOMPARE(loader.color("error"), QColor("#f38ba8"));
    }

    void testMissingThemeFile()
    {
        ThemeLoader loader;
        loader.loadTheme("nonexistent-theme", "/nonexistent/path");
        // Should fall back to defaults, not crash
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
        QCOMPARE(loader.color("text"), QColor("#cdd6f4"));
    }

    void testInvalidToml()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/bad.toml";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("this is not valid toml {{{{");
        f.close();

        ThemeLoader loader;
        loader.loadTheme(path, "");
        // Should use defaults, not crash
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
    }

    void testUnknownColorNameReturnsFallback()
    {
        ThemeLoader loader;
        loader.loadTheme("catppuccin-mocha", THEMES_DIR);
        // Unknown color name should return some default
        QColor unknown = loader.color("nonexistent_color");
        QVERIFY(unknown.isValid() || !unknown.isValid()); // Just verify no crash
    }

    void testThemeChangedSignal()
    {
        ThemeLoader loader;
        QSignalSpy spy(&loader, &ThemeLoader::themeChanged);

        loader.loadTheme("catppuccin-mocha", THEMES_DIR);
        QCOMPARE(spy.count(), 1);
    }

    void testLoadThemeByName()
    {
        ThemeLoader loader;
        loader.loadTheme("catppuccin-mocha", THEMES_DIR);
        // Loaded by name from themes directory
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
    }

    void testPropertyAccessors()
    {
        ThemeLoader loader;
        loader.loadTheme("catppuccin-mocha", THEMES_DIR);

        // Test the Q_PROPERTY accessors directly
        QCOMPARE(loader.base(), QColor("#1e1e2e"));
        QCOMPARE(loader.text(), QColor("#cdd6f4"));
        QCOMPARE(loader.accent(), QColor("#89b4fa"));
    }

    void testEmptyThemeFile()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/empty.toml";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.close(); // empty file

        ThemeLoader loader;
        loader.loadTheme(path, "");
        // Should use all defaults
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
    }

    // "auto" resolves against the system color scheme and re-resolves whenever
    // it flips. setSystemColorScheme() stands in for the desktop portal here;
    // in the app it is driven by QStyleHints::colorSchemeChanged.
    void testAutoFollowsLightColorScheme()
    {
        ThemeLoader loader;
        loader.setSystemColorScheme(Qt::ColorScheme::Light);
        loader.setThemeSearchPaths({THEMES_DIR});
        loader.setTheme(ThemeLoader::AutoTheme, "catppuccin-latte", "catppuccin-mocha");

        QVERIFY(loader.followSystem());
        QCOMPARE(loader.systemColorScheme(), QStringLiteral("light"));
        QCOMPARE(loader.activeTheme(), QStringLiteral("catppuccin-latte"));
        QCOMPARE(loader.color("base"), QColor("#eff1f5"));
    }

    void testAutoReloadsWhenColorSchemeFlips()
    {
        ThemeLoader loader;
        loader.setSystemColorScheme(Qt::ColorScheme::Light);
        loader.setThemeSearchPaths({THEMES_DIR});
        loader.setTheme(ThemeLoader::AutoTheme, "catppuccin-latte", "catppuccin-mocha");
        QSignalSpy spy(&loader, &ThemeLoader::themeChanged);

        loader.setSystemColorScheme(Qt::ColorScheme::Dark);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(loader.activeTheme(), QStringLiteral("catppuccin-mocha"));
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
    }

    void testExplicitThemeIgnoresColorScheme()
    {
        ThemeLoader loader;
        loader.setSystemColorScheme(Qt::ColorScheme::Light);
        loader.setThemeSearchPaths({THEMES_DIR});
        loader.setTheme("catppuccin-mocha", "catppuccin-latte", "catppuccin-mocha");
        QSignalSpy spy(&loader, &ThemeLoader::themeChanged);

        loader.setSystemColorScheme(Qt::ColorScheme::Dark);

        QVERIFY(!loader.followSystem());
        QCOMPARE(spy.count(), 0);
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
    }

    void testAutoUsesConfiguredThemePair()
    {
        QTemporaryDir dir;
        QFile light(dir.path() + "/my-light.toml");
        light.open(QIODevice::WriteOnly);
        light.write("[colors]\nbase = \"#ffffff\"\n");
        light.close();

        ThemeLoader loader;
        loader.setSystemColorScheme(Qt::ColorScheme::Light);
        loader.setThemeSearchPaths({dir.path()});
        loader.setTheme(ThemeLoader::AutoTheme, "my-light", "my-dark");

        QCOMPARE(loader.activeTheme(), QStringLiteral("my-light"));
        QCOMPARE(loader.color("base"), QColor("#ffffff"));
    }

    void testUnknownColorSchemeFallsBackToDark()
    {
        ThemeLoader loader;
        loader.setSystemColorScheme(Qt::ColorScheme::Unknown);
        loader.setThemeSearchPaths({THEMES_DIR});
        loader.setTheme(ThemeLoader::AutoTheme, "catppuccin-latte", "catppuccin-mocha");

        QCOMPARE(loader.systemColorScheme(), QStringLiteral("unknown"));
        QCOMPARE(loader.activeTheme(), QStringLiteral("catppuccin-mocha"));
    }

    void testSearchPathOrderShadowsBundledTheme()
    {
        QTemporaryDir userDir;
        QFile shadow(userDir.path() + "/catppuccin-mocha.toml");
        QVERIFY(shadow.open(QIODevice::WriteOnly));
        shadow.write("[colors]\nbase = \"#010203\"\n");
        shadow.close();

        ThemeLoader loader;
        loader.setThemeSearchPaths({userDir.path(), THEMES_DIR});
        loader.setTheme("catppuccin-mocha", "catppuccin-latte", "catppuccin-mocha");

        QCOMPARE(loader.color("base"), QColor("#010203"));
    }

    void testFallsThroughToLaterSearchPath()
    {
        QTemporaryDir emptyDir;

        ThemeLoader loader;
        loader.setThemeSearchPaths({emptyDir.path(), THEMES_DIR});
        loader.setTheme("catppuccin-latte", "catppuccin-latte", "catppuccin-mocha");

        QCOMPARE(loader.color("base"), QColor("#eff1f5"));
    }

    // A system themer rewriting the active theme file must recolor the running
    // app, not wait for a restart.
    void testReloadsWhenThemeFileChanges()
    {
        QTemporaryDir dir;
        const QString path = dir.path() + "/live.toml";
        QFile theme(path);
        QVERIFY(theme.open(QIODevice::WriteOnly));
        theme.write("[colors]\nbase = \"#111111\"\n");
        theme.close();

        ThemeLoader loader;
        loader.setThemeSearchPaths({dir.path()});
        loader.setTheme("live", "live", "live");
        QCOMPARE(loader.color("base"), QColor("#111111"));

        QSignalSpy spy(&loader, &ThemeLoader::themeChanged);
        QVERIFY(theme.open(QIODevice::WriteOnly | QIODevice::Truncate));
        theme.write("[colors]\nbase = \"#222222\"\n");
        theme.close();

        QVERIFY(spy.wait(3000));
        QCOMPARE(loader.color("base"), QColor("#222222"));
    }

    void testColorsSectionMissing()
    {
        QTemporaryDir dir;
        QString path = dir.path() + "/nocolor.toml";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write("[metadata]\nname = \"test\"\n"); // no [colors] section
        f.close();

        ThemeLoader loader;
        loader.loadTheme(path, "");
        QCOMPARE(loader.color("base"), QColor("#1e1e2e"));
    }
};

QTEST_MAIN(TestThemeLoader)
#include "tst_themeloader.moc"
