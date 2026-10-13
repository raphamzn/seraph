#include <QFile>
#include <cstdio>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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
};

QTEST_GUILESS_MAIN(TestReaderDocument)
#include "tst_readerdocument.moc"
