#include <QTest>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSize>
#include <QTemporaryDir>
#include "providers/iconprovider.h"

class TestIconProvider : public QObject
{
    Q_OBJECT

private:
    static void writeThemeIndex(const QString &path, const QString &inherits)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QByteArray body = "[Icon Theme]\nName=test\nDirectories=scalable/mimetypes\n";
        if (!inherits.isEmpty())
            body += "Inherits=" + inherits.toUtf8() + "\n";
        f.write(body);
    }

    static void writeSquare(const QString &path, const QString &color)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\">"
                               "<rect width=\"32\" height=\"32\" fill=\"%1\"/></svg>")
                    .arg(color)
                    .toUtf8());
    }

private slots:
    void testConstruction()
    {
        // Should not crash with a valid theme name
        IconProvider provider("Adwaita");
        Q_UNUSED(provider);

        // Should not crash with a nonexistent theme name
        IconProvider provider2("nonexistent-theme");
        Q_UNUSED(provider2);
    }

    void testMissingIconReturnsImage()
    {
        IconProvider provider("Adwaita");
        QSize size;
        QImage img = provider.requestImage("completely-nonexistent-icon-xyz", &size, QSize(48, 48));
        QVERIFY(!img.isNull());
    }

    void testDefaultSizeWhenNotRequested()
    {
        IconProvider provider("Adwaita");
        QSize size;
        // QSize(-1,-1) should trigger the default 48x48
        QImage img = provider.requestImage("completely-nonexistent-icon-xyz", &size, QSize(-1, -1));
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 48);
        QCOMPARE(img.height(), 48);
    }

    void testRequestedSize()
    {
        IconProvider provider("Adwaita");
        QSize size;
        QImage img = provider.requestImage("completely-nonexistent-icon-xyz", &size, QSize(24, 24));
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 24);
        QCOMPARE(img.height(), 24);
    }

    void testColorTintParsing()
    {
        IconProvider provider("Adwaita");
        QSize size;
        // Should not crash and should return a non-null image
        QImage img = provider.requestImage("text-x-generic?color=#ff0000", &size, QSize(48, 48));
        QVERIFY(!img.isNull());
    }

    void testInvalidTintColor()
    {
        IconProvider provider("Adwaita");
        QSize size;
        // "notacolor" is not a valid QColor — should be handled gracefully
        QImage img = provider.requestImage("text-x-generic?color=notacolor", &size, QSize(48, 48));
        QVERIFY(!img.isNull());
    }

    void testSymbolicIconFallback()
    {
        IconProvider provider("Adwaita");
        QSize size;
        // A nonexistent symbolic icon should return a 1x1 transparent image
        // with *size set to QSize(0,0)
        QImage img = provider.requestImage("nonexistent-symbolic", &size, QSize(48, 48));
        QVERIFY(!img.isNull());
        QCOMPARE(size, QSize(0, 0));
        // The pixel should be transparent
        QCOMPARE(qAlpha(img.pixel(0, 0)), 0);
    }

    void testKnownIcon_data()
    {
        QTest::addColumn<QString>("iconName");
        QTest::newRow("folder")          << "folder";
        QTest::newRow("text-x-generic")  << "text-x-generic";
        QTest::newRow("image-x-generic") << "image-x-generic";
        QTest::newRow("audio-x-generic") << "audio-x-generic";
    }

    void testKnownIcon()
    {
        QFETCH(QString, iconName);
        IconProvider provider("Adwaita");
        QSize size;
        QImage img = provider.requestImage(iconName, &size, QSize(48, 48));
        // Even if the icon isn't installed, we should get a valid image back
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), 48);
        QCOMPARE(img.height(), 48);
    }

    // The tint takes the theme's hue but must leave lightness alone, or the
    // folder's own shading — tab vs body, emblem vs background — flattens out.
    void testFolderTintRehuesWithoutFlattening()
    {
        IconProvider provider("Adwaita");
        QSize s1, s2;
        const QImage plain = provider.requestImage("folder", &s1, QSize(64, 64));
        const QImage tinted =
            provider.requestImage("folder?folder_tint=%2300ff00", &s2, QSize(64, 64));

        int compared = 0;
        int distinctLightness = 0;
        float firstLightness = -1;
        for (int y = 0; y < tinted.height(); ++y) {
            for (int x = 0; x < tinted.width(); ++x) {
                const QColor before = plain.pixelColor(x, y);
                const QColor after = tinted.pixelColor(x, y);
                if (before.alpha() < 255)
                    continue;

                float bh = 0, bs = 0, bl = 0, ah = 0, as = 0, al = 0;
                before.getHslF(&bh, &bs, &bl);
                after.getHslF(&ah, &as, &al);
                if (bs < 0.34f)  // neutral pixels keep their own hue by design
                    continue;

                ++compared;
                QVERIFY2(qAbs(ah - 1.0f / 3.0f) < 0.02f, "coloured pixels take the tint hue");
                QVERIFY2(qAbs(al - bl) < 0.02f, "lightness is preserved");
                if (firstLightness < 0)
                    firstLightness = al;
                else if (qAbs(al - firstLightness) > 0.05f)
                    ++distinctLightness;
            }
        }

        if (compared == 0)
            QSKIP("Adwaita folder icon not installed");
        QVERIFY2(distinctLightness > 0, "the icon still has more than one shade");
    }

    void testFolderTintLeavesOtherIconsAlone()
    {
        IconProvider provider("Adwaita");
        QSize s1, s2;
        const QImage plain = provider.requestImage("text-x-generic", &s1, QSize(48, 48));
        const QImage tinted =
            provider.requestImage("text-x-generic?folder_tint=%2300ff00", &s2, QSize(48, 48));

        QCOMPARE(tinted, plain);
    }

    // A theme's Inherits= chain decides what fills its gaps. The provider used
    // to consult a hardcoded list of popular themes instead, which silently
    // repainted the app whenever one of them happened to get installed.
    void testFollowsInheritsChain()
    {
        QTemporaryDir home;
        const QString icons = home.path() + "/.icons";
        QVERIFY(QDir().mkpath(icons + "/child/scalable/mimetypes"));
        QVERIFY(QDir().mkpath(icons + "/parent/scalable/mimetypes"));
        QVERIFY(QDir().mkpath(icons + "/stranger/scalable/mimetypes"));
        writeThemeIndex(icons + "/child/index.theme", "parent");
        writeThemeIndex(icons + "/parent/index.theme", QString());
        writeThemeIndex(icons + "/stranger/index.theme", QString());
        writeSquare(icons + "/parent/scalable/mimetypes/application-pdf.svg", "#ff0000");
        writeSquare(icons + "/stranger/scalable/mimetypes/application-zip.svg", "#00ff00");

        const QByteArray realHome = qgetenv("HOME");
        qputenv("HOME", home.path().toUtf8());
        IconProvider provider("child");
        QSize size;
        const QImage inherited = provider.requestImage("application-pdf", &size, QSize(32, 32));
        const QImage unrelated = provider.requestImage("application-zip", &size, QSize(32, 32));
        qputenv("HOME", realHome);

        QCOMPARE(inherited.pixelColor(16, 16), QColor("#ff0000"));
        QVERIFY2(unrelated.pixelColor(16, 16) != QColor("#00ff00"),
                 "a theme outside the chain must not be consulted");
    }

    // Falling back to the MIME type's generic icon is what keeps a thin theme
    // from dropping every office format onto the same plain text page.
    void testFallsBackToGenericMimeIcon()
    {
        QTemporaryDir home;
        const QString icons = home.path() + "/.icons";
        QVERIFY(QDir().mkpath(icons + "/thin/scalable/mimetypes"));
        writeThemeIndex(icons + "/thin/index.theme", QString());
        // No application-pdf; only the generic icon it maps to.
        writeSquare(icons + "/thin/scalable/mimetypes/x-office-document.svg", "#0000ff");

        const QByteArray realHome = qgetenv("HOME");
        qputenv("HOME", home.path().toUtf8());
        IconProvider provider("thin");
        QSize size;
        const QImage img = provider.requestImage("application-pdf", &size, QSize(32, 32));
        qputenv("HOME", realHome);

        QCOMPARE(img.pixelColor(16, 16), QColor("#0000ff"));
    }

    void testMultipleQueryParams()
    {
        IconProvider provider("Adwaita");
        QSize size;
        // Extra unknown params should be silently ignored; color param still parsed
        QImage img = provider.requestImage("text-x-generic?color=#00ff00&other=value", &size, QSize(48, 48));
        QVERIFY(!img.isNull());
    }

    void testTintChangesPixels()
    {
        IconProvider provider("Adwaita");
        QSize size1, size2;

        QImage untinted = provider.requestImage("text-x-generic", &size1, QSize(48, 48));
        QImage tinted   = provider.requestImage("text-x-generic?color=#ff0000", &size2, QSize(48, 48));

        QVERIFY(!untinted.isNull());
        QVERIFY(!tinted.isNull());

        // Check whether the icon was actually found (i.e., has any opaque pixels).
        // If no opaque pixels are present the icon is not installed; skip the comparison.
        bool hasOpaquePixel = false;
        for (int y = 0; y < untinted.height() && !hasOpaquePixel; ++y)
            for (int x = 0; x < untinted.width() && !hasOpaquePixel; ++x)
                if (qAlpha(untinted.pixel(x, y)) > 0)
                    hasOpaquePixel = true;

        if (!hasOpaquePixel) {
            QSKIP("text-x-generic icon not found on this system; skipping tint pixel check");
        }

        // Every opaque pixel in the tinted image should have R=255, G=0, B=0
        bool foundTintedPixel = false;
        for (int y = 0; y < tinted.height(); ++y) {
            for (int x = 0; x < tinted.width(); ++x) {
                QRgb px = tinted.pixel(x, y);
                if (qAlpha(px) > 0) {
                    QCOMPARE(qRed(px),   255);
                    QCOMPARE(qGreen(px), 0);
                    QCOMPARE(qBlue(px),  0);
                    foundTintedPixel = true;
                }
            }
        }
        QVERIFY(foundTintedPixel);
    }
};

QTEST_MAIN(TestIconProvider)
#include "tst_iconprovider.moc"
