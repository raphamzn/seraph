#include <QFile>
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
};

QTEST_GUILESS_MAIN(TestReaderDocument)
#include "tst_readerdocument.moc"
