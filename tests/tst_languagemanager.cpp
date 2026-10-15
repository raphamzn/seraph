#include <QTest>
#include <QCoreApplication>
#include <QLocale>
#include <QSignalSpy>
#include "services/languagemanager.h"

class TestLanguageManager : public QObject
{
    Q_OBJECT

    static QString fixtureText()
    {
        return QCoreApplication::translate("LanguageFixture", "Settings");
    }

private slots:
    void testResolve_data()
    {
        QTest::addColumn<QString>("setting");
        QTest::addColumn<QString>("system");
        QTest::addColumn<QString>("expected");

        QTest::newRow("auto pt_BR") << "auto" << "pt_BR" << "pt_BR";
        QTest::newRow("auto pt_PT") << "auto" << "pt_PT" << "pt_BR";
        QTest::newRow("auto pt")    << "auto" << "pt"    << "pt_BR";
        QTest::newRow("auto en_US") << "auto" << "en_US" << "en";
        QTest::newRow("auto fr_FR") << "auto" << "fr_FR" << "en";
        QTest::newRow("auto C")     << "auto" << "C"     << "en";
        QTest::newRow("explicit pt on en system") << "pt_BR" << "en_US" << "pt_BR";
        QTest::newRow("explicit en on pt system") << "en" << "pt_BR" << "en";
        QTest::newRow("case/dash tolerant") << "PT-br" << "en_US" << "pt_BR";
        QTest::newRow("unknown acts as auto") << "klingon" << "pt_BR" << "pt_BR";
        QTest::newRow("empty acts as auto") << "" << "en_US" << "en";
    }

    void testResolve()
    {
        QFETCH(QString, setting);
        QFETCH(QString, system);
        QFETCH(QString, expected);
        QCOMPARE(LanguageManager::resolve(setting, QLocale(system)), expected);
    }

    void testApplySwitchesTranslationLive()
    {
        LanguageManager mgr(nullptr, QStringLiteral(":/i18n-test/test_"));
        QSignalSpy spy(&mgr, &LanguageManager::languageChanged);

        mgr.apply(QStringLiteral("pt_BR"), QLocale(QStringLiteral("en_US")));
        QCOMPARE(mgr.effectiveLanguage(), QString("pt_BR"));
        QCOMPARE(fixtureText(), QString::fromUtf8("Configurações"));
        QCOMPARE(QLocale().language(), QLocale::Portuguese);
        QCOMPARE(spy.count(), 1);

        mgr.apply(QStringLiteral("en"), QLocale(QStringLiteral("en_US")));
        QCOMPARE(mgr.effectiveLanguage(), QString("en"));
        QCOMPARE(fixtureText(), QString("Settings"));
        QCOMPARE(QLocale().language(), QLocale::English);
        QCOMPARE(spy.count(), 2);
    }

    void testApplySameLanguageDoesNotReemit()
    {
        LanguageManager mgr(nullptr, QStringLiteral(":/i18n-test/test_"));
        mgr.apply(QStringLiteral("auto"), QLocale(QStringLiteral("pt_BR")));
        QSignalSpy spy(&mgr, &LanguageManager::languageChanged);
        mgr.apply(QStringLiteral("pt_BR"), QLocale(QStringLiteral("en_US")));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(fixtureText(), QString::fromUtf8("Configurações"));
    }

    void testDecimalSeparatorFollowsLanguage()
    {
        LanguageManager mgr(nullptr, QStringLiteral(":/i18n-test/test_"));
        mgr.apply(QStringLiteral("pt_BR"), QLocale(QStringLiteral("en_US")));
        QCOMPARE(QLocale().toString(1.5, 'f', 1), QString("1,5"));
        mgr.apply(QStringLiteral("en"), QLocale(QStringLiteral("en_US")));
        QCOMPARE(QLocale().toString(1.5, 'f', 1), QString("1.5"));
    }
};

QTEST_MAIN(TestLanguageManager)
#include "tst_languagemanager.moc"
