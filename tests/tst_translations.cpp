#include <QTest>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

// Fails when a translatable string in the sources has no pt_BR translation,
// so a new feature can't ship English-only by accident.
class TestTranslations : public QObject
{
    Q_OBJECT

private slots:
    void testEverySourceStringIsTranslated()
    {
        QString lupdate = QStandardPaths::findExecutable("lupdate", {QStringLiteral(QT_HOST_BINS)});
        if (lupdate.isEmpty())
            lupdate = QStandardPaths::findExecutable("lupdate");
        if (lupdate.isEmpty())
            QSKIP("lupdate not available");

        QTemporaryDir dir;
        const QString ts = dir.filePath("check.ts");
        QVERIFY(QFile::copy(QStringLiteral(SERAPH_TS_FILE), ts));
        QFile::setPermissions(ts, QFile::ReadOwner | QFile::WriteOwner);

        QProcess proc;
        proc.start(lupdate, {QStringLiteral("@") + QStringLiteral(SERAPH_I18N_LIST),
                             "-no-obsolete", "-locations", "none", "-ts", ts});
        QVERIFY(proc.waitForFinished(60000));
        QCOMPARE(proc.exitCode(), 0);

        QFile file(ts);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString content = QString::fromUtf8(file.readAll());

        static const QRegularExpression unfinished(
            QStringLiteral("<source>((?:(?!</source>).)*)</source>\\s*(?:<comment>(?:(?!</comment>).)*</comment>\\s*)?"
                           "<translation type=\"unfinished\""),
            QRegularExpression::DotMatchesEverythingOption);
        QStringList missing;
        for (auto it = unfinished.globalMatch(content); it.hasNext();)
            missing << it.next().captured(1);

        if (!missing.isEmpty())
            QFAIL(qPrintable(QStringLiteral("Untranslated (%1): %2 — run "
                "`cmake --build build --target update_translations` and translate them")
                .arg(missing.size()).arg(missing.mid(0, 20).join(QStringLiteral(" | ")))));
    }
};

QTEST_MAIN(TestTranslations)
#include "tst_translations.moc"
