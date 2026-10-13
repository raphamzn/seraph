#include <QDir>
#include <QFile>
#include <cstdio>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <unistd.h>

#include "services/readerdocument.h"

class TestReaderDocument : public QObject
{
    Q_OBJECT

private:
    static void writeFile(const QString &path, const QByteArray &data)
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(data);
    }

private slots:
    void testOpenReadsTheWholeFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("big.md");
        QByteArray data = "# Big\n\n";
        while (data.size() < 300 * 1024)
            data += "Line of prose that keeps going on and on.\n";
        data += "THE END\n";
        writeFile(path, data);

        ReaderDocument doc;
        QSignalSpy shown(&doc, &ReaderDocument::shown);
        doc.open(path);

        QCOMPARE(doc.content().size(), data.size());
        QVERIFY(doc.content().endsWith("THE END\n"));
        QCOMPARE(doc.errorMessage(), QString());
        QCOMPARE(doc.missing(), false);
        QCOMPARE(doc.fileName(), QString("big.md"));
        QCOMPARE(doc.baseDir(), dir.path());
        QCOMPARE(shown.count(), 1);
        QCOMPARE(shown.at(0).at(0).toString(), QString("open"));
    }

    void testMissingFileIsReportedWithoutError()
    {
        QTemporaryDir dir;
        ReaderDocument doc;
        doc.open(dir.filePath("nope.md"));

        QCOMPARE(doc.missing(), true);
        QCOMPARE(doc.content(), QString());
        QCOMPARE(doc.errorMessage(), QString());
    }

    void testBinaryFileGetsThePreviewMessage()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("blob.md");
        writeFile(path, QByteArray("\x00\x01\x02\x03binary\x00\x00", 12));

        ReaderDocument doc;
        doc.open(path);

        QCOMPARE(doc.errorMessage(),
                 QString("This file looks binary and cannot be previewed as text."));
        QCOMPARE(doc.content(), QString());
    }

    void testUnreadableFileExplainsWhy()
    {
        if (getuid() == 0)
            QSKIP("root reads everything");
        QTemporaryDir dir;
        const QString path = dir.filePath("locked.md");
        writeFile(path, "# Secret\n");
        QFile::setPermissions(path, QFileDevice::Permissions());

        ReaderDocument doc;
        doc.open(path);

        QVERIFY2(doc.errorMessage().startsWith("Cannot open locked.md:"),
                 qPrintable(doc.errorMessage()));
        QCOMPARE(doc.missing(), false);
    }

    void testLiveReloadOnWrite()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.md");
        writeFile(path, "# One\n");
        ReaderDocument doc;
        doc.setDebounceInterval(20);
        doc.open(path);
        QSignalSpy shown(&doc, &ReaderDocument::shown);

        writeFile(path, "# Two\n");

        QTRY_COMPARE(doc.content(), QString("# Two\n"));
        QCOMPARE(shown.count(), 1);
        QCOMPARE(shown.at(0).at(0).toString(), QString("live"));
    }

    void testSaveByRenameKeepsFollowingTheFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.md");
        writeFile(path, "# One\n");
        ReaderDocument doc;
        doc.setDebounceInterval(20);
        doc.open(path);

        for (const QByteArray &version : {QByteArray("# Two\n"), QByteArray("# Three\n")}) {
            const QString temp = dir.filePath(".doc.md.swp");
            writeFile(temp, version);
            QCOMPARE(::rename(QFile::encodeName(temp).constData(),
                              QFile::encodeName(path).constData()), 0);
            QTRY_COMPARE(doc.content(), QString::fromUtf8(version));
        }
    }

    void testDeletedFileKeepsLastVersionAndComesBack()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.md");
        writeFile(path, "# One\n");
        ReaderDocument doc;
        doc.setDebounceInterval(20);
        doc.open(path);

        QVERIFY(QFile::remove(path));
        QTRY_COMPARE(doc.missing(), true);
        QCOMPARE(doc.content(), QString("# One\n"));

        writeFile(path, "# Back\n");
        QTRY_COMPARE(doc.missing(), false);
        QTRY_COMPARE(doc.content(), QString("# Back\n"));
    }

    void testFileCreatedAfterOpeningMissingIsLoaded()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("later.md");
        ReaderDocument doc;
        doc.setDebounceInterval(20);
        doc.open(path);
        QCOMPARE(doc.missing(), true);

        writeFile(path, "# Now\n");
        QTRY_COMPARE(doc.content(), QString("# Now\n"));
        QCOMPARE(doc.missing(), false);
    }

    // An editor that keeps the old file open after a save-by-rename used to
    // leave the watch on the old inode: later in-place writes went unseen.
    void testInPlaceWriteAfterRenameWhileOldFileIsHeldOpen()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.md");
        writeFile(path, "# A\n");
        QFile held(path);
        QVERIFY(held.open(QIODevice::ReadOnly));
        ReaderDocument doc;
        doc.setDebounceInterval(20);
        doc.open(path);

        const QString temp = dir.filePath(".doc.md.tmp");
        writeFile(temp, "# B\n");
        QCOMPARE(::rename(QFile::encodeName(temp).constData(), QFile::encodeName(path).constData()), 0);
        QTRY_COMPARE(doc.content(), QString("# B\n"));

        writeFile(path, "# C\n");
        QTRY_COMPARE(doc.content(), QString("# C\n"));
    }

    void testWriteBurstReloadsOnce()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.md");
        writeFile(path, "# 0\n");
        ReaderDocument doc;
        doc.setDebounceInterval(80);
        doc.open(path);
        QSignalSpy changed(&doc, &ReaderDocument::contentChanged);

        for (int i = 1; i <= 5; ++i)
            writeFile(path, QByteArray("# ") + QByteArray::number(i) + "\n");

        QTRY_COMPARE(doc.content(), QString("# 5\n"));
        QTest::qWait(200);
        QCOMPARE(changed.count(), 1);
    }

    void testOtherFilesInTheFolderDoNotRerender()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("doc.md");
        writeFile(path, "# Same\n");
        ReaderDocument doc;
        doc.setDebounceInterval(20);
        doc.open(path);
        QSignalSpy changed(&doc, &ReaderDocument::contentChanged);
        QSignalSpy shown(&doc, &ReaderDocument::shown);

        writeFile(dir.filePath("other.txt"), "noise");
        QTest::qWait(250);

        QCOMPARE(changed.count(), 0);
        QCOMPARE(shown.count(), 0);
    }

    void testClassifyLink_data()
    {
        QTest::addColumn<QString>("link");
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("path");
        QTest::addColumn<QString>("fragment");

        QTest::newRow("anchor") << "#install-guide" << "anchor" << "" << "install-guide";
        QTest::newRow("encoded anchor") << "#a%C3%A7%C3%A3o" << "anchor" << "" << "ação";
        QTest::newRow("http") << "https://example.org/x" << "web" << "" << "";
        QTest::newRow("mailto") << "mailto:a@b.c" << "web" << "" << "";
        QTest::newRow("resolved md with fragment")
            << "file:///docs/guide.md#Setup" << "markdown" << "/docs/guide.md" << "Setup";
        QTest::newRow("encoded spaces and accents")
            << "file:///docs/Notas%20A%C3%A7%C3%A3o.md" << "markdown" << "/docs/Notas Ação.md" << "";
        QTest::newRow("relative raw path")
            << "../other/README.MD" << "markdown" << "/other/README.MD" << "";
        QTest::newRow("same file with fragment")
            << "file:///docs/current.md#part-2" << "anchor" << "" << "part-2";
        QTest::newRow("pdf") << "file:///docs/manual.pdf" << "file" << "/docs/manual.pdf" << "";
        QTest::newRow("empty") << "" << "none" << "" << "";
    }

    void testClassifyLink()
    {
        QFETCH(QString, link);
        QFETCH(QString, kind);
        QFETCH(QString, path);
        QFETCH(QString, fragment);

        const QVariantMap r = ReaderDocument::classifyLink(link, "/docs/current.md");
        QCOMPARE(r.value("kind").toString(), kind);
        QCOMPARE(r.value("path").toString(), path);
        QCOMPARE(r.value("fragment").toString(), fragment);
    }

    void testFileLinkCarriesAnOpenableUrl()
    {
        const QVariantMap r = ReaderDocument::classifyLink("file:///docs/a%20b.pdf", "/docs/current.md");
        QCOMPARE(r.value("url").toString(), QUrl::fromLocalFile("/docs/a b.pdf").toString());
    }

    void testFolderLinkIsAFileEvenWithMdSuffix()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir("notes.md"));
        const QVariantMap r = ReaderDocument::classifyLink(
            QUrl::fromLocalFile(dir.filePath("notes.md")).toString(), dir.filePath("current.md"));
        QCOMPARE(r.value("kind").toString(), QString("file"));
    }

    void testNavigateAndGoBackRestoresScroll()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a.md"), "# A\n");
        writeFile(dir.filePath("b.md"), "# B\n");
        ReaderDocument doc;
        doc.open(dir.filePath("a.md"));
        QSignalSpy shown(&doc, &ReaderDocument::shown);

        doc.navigate(dir.filePath("b.md"), "intro", 120.5);
        QCOMPARE(doc.filePath(), dir.filePath("b.md"));
        QCOMPARE(doc.content(), QString("# B\n"));
        QCOMPARE(doc.canGoBack(), true);
        QCOMPARE(shown.last().at(0).toString(), QString("navigate"));
        QCOMPARE(shown.last().at(1).toString(), QString("intro"));

        QVERIFY(doc.goBack());
        QCOMPARE(doc.filePath(), dir.filePath("a.md"));
        QCOMPARE(doc.content(), QString("# A\n"));
        QCOMPARE(doc.canGoBack(), false);
        QCOMPARE(shown.last().at(0).toString(), QString("back"));
        QCOMPARE(shown.last().at(2).toReal(), 120.5);

        QVERIFY(!doc.goBack());
    }

    void testNavigateToMissingFileThenBack()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a.md"), "# A\n");
        ReaderDocument doc;
        doc.open(dir.filePath("a.md"));

        doc.navigate(dir.filePath("ghost.md"), QString(), 40);
        QCOMPARE(doc.missing(), true);
        QCOMPARE(doc.content(), QString());

        QVERIFY(doc.goBack());
        QCOMPARE(doc.missing(), false);
        QCOMPARE(doc.content(), QString("# A\n"));
    }

    void testOpenForgetsHistory()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("a.md"), "# A\n");
        writeFile(dir.filePath("b.md"), "# B\n");
        ReaderDocument doc;
        doc.open(dir.filePath("a.md"));
        doc.navigate(dir.filePath("b.md"), QString(), 0);
        doc.open(dir.filePath("a.md"));
        QCOMPARE(doc.canGoBack(), false);
    }
};

QTEST_GUILESS_MAIN(TestReaderDocument)
#include "tst_readerdocument.moc"
