#include <QTest>
#include <QSignalSpy>
#include <QAbstractItemModelTester>
#include <QDir>
#include <QStandardPaths>
#include "models/bookmarkmodel.h"

class TestBookmarkModel : public QObject
{
    Q_OBJECT

private slots:
    void testModelConsistency()
    {
        BookmarkModel model;
        auto *tester = new QAbstractItemModelTester(&model,
            QAbstractItemModelTester::FailureReportingMode::QtTest);
        Q_UNUSED(tester)

        model.setBookmarks({"~/Documents", "~/Downloads", "~/Pictures"});
        model.setBookmarks({"~/Music"});
        model.setBookmarks({});
    }

    void testLoadBookmarks()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Documents", "~/Downloads"});
        QCOMPARE(model.rowCount(), 2);
    }

    void testBookmarkData()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Documents"});
        QModelIndex idx = model.index(0);
        QString name = model.data(idx, BookmarkModel::NameRole).toString();
        QCOMPARE(name, QString("Documents"));
    }

    void testExpandTilde()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Documents"});
        QModelIndex idx = model.index(0);
        QString path = model.data(idx, BookmarkModel::PathRole).toString();
        QVERIFY(path.startsWith("/"));
        QVERIFY(path.endsWith("/Documents"));
        QVERIFY(!path.contains("~"));
    }

    void testIconForKnownPaths_data()
    {
        QTest::addColumn<QString>("bookmark");
        QTest::addColumn<bool>("hasIcon");

        QTest::newRow("Documents") << "~/Documents" << true;
        QTest::newRow("Downloads") << "~/Downloads" << true;
        QTest::newRow("Pictures") << "~/Pictures" << true;
        QTest::newRow("Music") << "~/Music" << true;
        QTest::newRow("Videos") << "~/Videos" << true;
        QTest::newRow("Unknown") << "~/RandomDir" << true; // should have fallback icon
    }

    void testIconForKnownPaths()
    {
        QFETCH(QString, bookmark);
        QFETCH(bool, hasIcon);

        BookmarkModel model;
        model.setBookmarks({bookmark});
        QModelIndex idx = model.index(0);
        QString icon = model.data(idx, BookmarkModel::IconRole).toString();
        QCOMPARE(!icon.isEmpty(), hasIcon);
    }

    void testEmptyBookmarkList()
    {
        BookmarkModel model;
        model.setBookmarks({});
        QCOMPARE(model.rowCount(), 0);
    }

    void testReplaceBookmarks()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Documents", "~/Downloads"});
        QCOMPARE(model.rowCount(), 2);

        model.setBookmarks({"~/Music"});
        QCOMPARE(model.rowCount(), 1);

        QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, BookmarkModel::NameRole).toString(), QString("Music"));
    }

    void testRoleNames()
    {
        BookmarkModel model;
        auto roles = model.roleNames();
        QCOMPARE(roles[BookmarkModel::NameRole], QByteArray("name"));
        QCOMPARE(roles[BookmarkModel::PathRole], QByteArray("path"));
        QCOMPARE(roles[BookmarkModel::IconRole], QByteArray("icon"));
        QCOMPARE(roles[BookmarkModel::LabelRole], QByteArray("label"));
        QCOMPARE(roles[BookmarkModel::QuillIconRole], QByteArray("quillIcon"));
        QCOMPARE(roles[BookmarkModel::ColorRoleRole], QByteArray("colorRole"));
        QCOMPARE(roles[BookmarkModel::IsRemoteRole], QByteArray("isRemote"));
        QCOMPARE(roles[BookmarkModel::RemoteHintRole], QByteArray("remoteHint"));
        QCOMPARE(roles[BookmarkModel::KeyRole], QByteArray("styleKey"));
    }

    void testInvalidIndex()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Documents"});
        QModelIndex bad = model.index(999);
        QVERIFY(!model.data(bad, BookmarkModel::NameRole).isValid());
    }

    void testAbsolutePath()
    {
        BookmarkModel model;
        model.setBookmarks({"/tmp"});
        QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, BookmarkModel::PathRole).toString(), QString("/tmp"));
        QCOMPARE(model.data(idx, BookmarkModel::NameRole).toString(), QString("tmp"));
    }

    void testRemoteBookmarkName()
    {
        BookmarkModel model;
        model.setBookmarks({"sftp://example.com/home/jim"});
        QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, BookmarkModel::PathRole).toString(), QString("sftp://example.com/home/jim"));
        QCOMPARE(model.data(idx, BookmarkModel::NameRole).toString(), QString("jim"));
    }

    void testRowsResetSignal()
    {
        BookmarkModel model;
        QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

        model.setBookmarks({"~/Documents"});
        QVERIFY(resetSpy.count() >= 1);
    }

    // --- Styles ---

    void testDefaultQuillIcons()
    {
        QCOMPARE(BookmarkModel::defaultQuillIcon(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)), QString("Image"));
        QCOMPARE(BookmarkModel::defaultQuillIcon(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)), QString("Download"));
        QCOMPARE(BookmarkModel::defaultQuillIcon(QStandardPaths::writableLocation(QStandardPaths::MusicLocation)), QString("Music"));
        QCOMPARE(BookmarkModel::defaultQuillIcon(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)), QString("Video"));
        QCOMPARE(BookmarkModel::defaultQuillIcon(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)), QString("FileText"));
        QCOMPARE(BookmarkModel::defaultQuillIcon(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)), QString("Monitor"));
        QCOMPARE(BookmarkModel::defaultQuillIcon(QDir::homePath()), QString("Home"));
        QCOMPARE(BookmarkModel::defaultQuillIcon("sftp://box/srv"), QString("Globe"));
        QCOMPARE(BookmarkModel::defaultQuillIcon("/tmp/whatever"), QString("Folder"));
    }

    void testStyleRolesAndLabel()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Projetos"});
        model.setStyles({{"~/Projetos", QVariantMap{{"icon", "Terminal"}, {"color", "accent"}, {"label", "Code"}}}});
        const QModelIndex i = model.index(0, 0);
        QCOMPARE(model.data(i, BookmarkModel::NameRole).toString(), QString("Code"));
        QCOMPARE(model.data(i, BookmarkModel::LabelRole).toString(), QString("Code"));
        QCOMPARE(model.data(i, BookmarkModel::QuillIconRole).toString(), QString("Terminal"));
        QCOMPARE(model.data(i, BookmarkModel::ColorRoleRole).toString(), QString("accent"));
        QCOMPARE(model.data(i, BookmarkModel::KeyRole).toString(), QString("~/Projetos"));
        QCOMPARE(model.data(i, BookmarkModel::IsRemoteRole).toBool(), false);
    }

    void testStyleSurvivesReorderAndDefaultsWithoutStyle()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Projetos", "/tmp"});
        model.setStyles({{"~/Projetos", QVariantMap{{"icon", "Star"}}}});
        model.setBookmarks({"/tmp", "~/Projetos"});
        QCOMPARE(model.data(model.index(1, 0), BookmarkModel::QuillIconRole).toString(), QString("Star"));
        QCOMPARE(model.data(model.index(0, 0), BookmarkModel::QuillIconRole).toString(), QString("Folder"));
        QCOMPARE(model.data(model.index(0, 0), BookmarkModel::LabelRole).toString(), QString());
        QCOMPARE(model.data(model.index(0, 0), BookmarkModel::ColorRoleRole).toString(), QString());
    }

    void testStyleUnknownColorDegradesToNone()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Projetos"});
        model.setStyles({{"~/Projetos", QVariantMap{{"color", "purple"}}}});
        QCOMPARE(model.data(model.index(0, 0), BookmarkModel::ColorRoleRole).toString(), QString());
        QCOMPARE(model.data(model.index(0, 0), BookmarkModel::NameRole).toString(), QString("Projetos"));
    }

    void testSetStyleEmitsAndUpdates()
    {
        BookmarkModel model;
        model.setBookmarks({"~/Projetos"});
        QSignalSpy spy(&model, &BookmarkModel::styleChanged);
        QSignalSpy dataSpy(&model, &BookmarkModel::dataChanged);
        model.setStyle(0, "Star", "warning", "");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("~/Projetos"));
        QCOMPARE(spy.first().at(1).toString(), QString("Star"));
        QCOMPARE(spy.first().at(2).toString(), QString("warning"));
        QCOMPARE(dataSpy.count(), 1);
        QCOMPARE(model.styleFor(0).value("icon").toString(), QString("Star"));
        QCOMPARE(model.styleFor(0).value("folderName").toString(), QString("Projetos"));
        QCOMPARE(model.styleFor(0).value("key").toString(), QString("~/Projetos"));
        QCOMPARE(model.data(model.index(0, 0), BookmarkModel::QuillIconRole).toString(), QString("Star"));

        // An unknown colour is stored as none and reported as none
        model.setStyle(0, "Star", "neon", "");
        QCOMPARE(spy.last().at(2).toString(), QString());
    }

    void testRemoteHint()
    {
        BookmarkModel model;
        model.setBookmarks({"sftp://user@box.lan/srv"});
        const QModelIndex i = model.index(0, 0);
        QCOMPARE(model.data(i, BookmarkModel::IsRemoteRole).toBool(), true);
        QCOMPARE(model.data(i, BookmarkModel::RemoteHintRole).toString(), QString("sftp · box.lan"));
        QCOMPARE(model.data(i, BookmarkModel::QuillIconRole).toString(), QString("Globe"));
    }
};

QTEST_MAIN(TestBookmarkModel)
#include "tst_bookmarkmodel.moc"
