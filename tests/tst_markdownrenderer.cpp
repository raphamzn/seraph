#include <QTest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVariantList>
#include <QVariantMap>

#include "services/markdownrenderer.h"

class TestMarkdownRenderer : public QObject
{
    Q_OBJECT

private:
    // Rendering without bat keeps the tests independent of what is installed.
    static QVariantMap renderPlain(const QString &markdown, const QString &baseDir = QString())
    {
        MarkdownRenderer renderer;
        QVariantMap options;
        options["highlightCode"] = false;
        return renderer.render(markdown, baseDir, options);
    }

    static QVariantList blocksOf(const QVariantMap &doc)
    {
        return doc.value("blocks").toList();
    }

    static QVariantMap block(const QVariantMap &doc, int index)
    {
        return blocksOf(doc).value(index).toMap();
    }

    static bool batAvailable()
    {
        return !QStandardPaths::findExecutable("bat").isEmpty()
            || !QStandardPaths::findExecutable("batcat").isEmpty();
    }

private slots:
    void testHeadingBlockCarriesLevelAndPlainText()
    {
        const QVariantMap doc = renderPlain("## Getting *started*\n");

        QCOMPARE(blocksOf(doc).size(), 1);
        const QVariantMap heading = block(doc, 0);
        QCOMPARE(heading.value("type").toString(), QString("heading"));
        QCOMPARE(heading.value("level").toInt(), 2);
        QCOMPARE(heading.value("text").toString(), QString("Getting started"));
        QVERIFY(heading.value("html").toString().contains("<i>started</i>"));
    }

    void testParagraphInlineFormatting()
    {
        const QVariantMap doc = renderPlain("Mix **bold**, _em_, ~~gone~~ and `code`.\n");

        const QVariantMap paragraph = block(doc, 0);
        QCOMPARE(paragraph.value("type").toString(), QString("paragraph"));
        const QString html = paragraph.value("html").toString();
        QVERIFY2(html.contains("<b>bold</b>"), qPrintable(html));
        QVERIFY2(html.contains("<i>em</i>"), qPrintable(html));
        QVERIFY2(html.contains("<s>gone</s>"), qPrintable(html));
        QVERIFY2(html.contains("font-family:monospace"), qPrintable(html));
        QVERIFY2(html.contains(">code</span>"), qPrintable(html));
    }

    void testLinkKeepsHrefAndText()
    {
        const QVariantMap doc = renderPlain("See [Qt](https://qt.io) now.\n");

        const QString html = block(doc, 0).value("html").toString();
        QVERIFY2(html.contains("<a href=\"https://qt.io\">Qt</a>"), qPrintable(html));
    }

    void testHardLineBreakStaysInsideTheParagraph()
    {
        const QVariantMap doc = renderPlain("one  \ntwo\n\nthree\\\nfour\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        QCOMPARE(block(doc, 0).value("html").toString(), QString("one<br>two"));
        QCOMPARE(block(doc, 1).value("html").toString(), QString("three<br>four"));
    }

    void testTrailingSpacesInsideCodeAreNotLineBreaks()
    {
        const QVariantMap doc = renderPlain("```\ncode  \nmore\n```\n\ntext\n\n    a  \n    b\n");

        QCOMPARE(block(doc, 0).value("code").toString(), QString("code  \nmore"));
        QCOMPARE(block(doc, 2).value("code").toString(), QString("a  \nb"));
    }

    void testFencedCodeBlockGroupsLinesWithLanguage()
    {
        const QVariantMap doc = renderPlain("```cpp\nint a;\n\nreturn a;\n```\n");

        QCOMPARE(blocksOf(doc).size(), 1);
        const QVariantMap code = block(doc, 0);
        QCOMPARE(code.value("type").toString(), QString("code"));
        QCOMPARE(code.value("language").toString(), QString("cpp"));
        QCOMPARE(code.value("code").toString(), QString("int a;\n\nreturn a;"));
        QCOMPARE(code.value("lineCount").toInt(), 3);
        QCOMPARE(code.value("html").toString(), QString());
    }

    void testAdjacentFencesStaySeparateBlocks()
    {
        const QVariantMap doc = renderPlain("```sh\na\n```\n\n```sh\nb\nc\n```\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        QCOMPARE(block(doc, 0).value("code").toString(), QString("a"));
        QCOMPARE(block(doc, 1).value("code").toString(), QString("b\nc"));
    }

    void testLongerFenceMayContainShorterFences()
    {
        const QVariantMap doc = renderPlain("````md\n```js\nx\n```\n````\n\ntext\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        QCOMPARE(block(doc, 0).value("code").toString(), QString("```js\nx\n```"));
        QCOMPARE(block(doc, 1).value("type").toString(), QString("paragraph"));
    }

    void testIndentedCodeBlockHasNoLanguage()
    {
        const QVariantMap doc = renderPlain("text\n\n    x = 1\n    y = 2\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        const QVariantMap code = block(doc, 1);
        QCOMPARE(code.value("type").toString(), QString("code"));
        QCOMPARE(code.value("language").toString(), QString());
        QCOMPARE(code.value("code").toString(), QString("x = 1\ny = 2"));
    }

    void testQuoteGroupsItsParagraphs()
    {
        const QVariantMap doc = renderPlain("> one\n>\n> two\n\nafter\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        const QVariantMap quote = block(doc, 0);
        QCOMPARE(quote.value("type").toString(), QString("quote"));
        const QVariantList items = quote.value("items").toList();
        QCOMPARE(items.size(), 2);
        QCOMPARE(items.at(0).toMap().value("html").toString(), QString("one"));
        QCOMPARE(items.at(1).toMap().value("html").toString(), QString("two"));
        QCOMPARE(block(doc, 1).value("type").toString(), QString("paragraph"));
    }

    void testListItemsCarryDepthOrderingAndTaskState()
    {
        const QVariantMap doc = renderPlain("- a\n  - b\n- [x] done\n\n1. x\n2. y\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        const QVariantList bullets = block(doc, 0).value("items").toList();
        QCOMPARE(block(doc, 0).value("type").toString(), QString("list"));
        QCOMPARE(bullets.size(), 3);
        QCOMPARE(bullets.at(0).toMap().value("html").toString(), QString("a"));
        QCOMPARE(bullets.at(0).toMap().value("depth").toInt(), 1);
        QCOMPARE(bullets.at(0).toMap().value("ordered").toBool(), false);
        QCOMPARE(bullets.at(0).toMap().value("checked").toInt(), -1);
        QCOMPARE(bullets.at(1).toMap().value("depth").toInt(), 2);
        QCOMPARE(bullets.at(2).toMap().value("checked").toInt(), 1);

        const QVariantList numbered = block(doc, 1).value("items").toList();
        QCOMPARE(numbered.size(), 2);
        QCOMPARE(numbered.at(0).toMap().value("ordered").toBool(), true);
        QCOMPARE(numbered.at(0).toMap().value("number").toInt(), 1);
        QCOMPARE(numbered.at(1).toMap().value("number").toInt(), 2);
    }

    void testHorizontalRuleBecomesRuleBlock()
    {
        const QVariantMap doc = renderPlain("a\n\n---\n\nb\n");

        QCOMPARE(blocksOf(doc).size(), 3);
        QCOMPARE(block(doc, 1).value("type").toString(), QString("rule"));
        QCOMPARE(block(doc, 2).value("html").toString(), QString("b"));
    }

    void testTableSplitsHeaderAndRowsWithAlignment()
    {
        const QVariantMap doc = renderPlain("| H1 | H2 |\n|:---|---:|\n| a | b |\n\nafter\n");

        QCOMPARE(blocksOf(doc).size(), 2);
        const QVariantMap table = block(doc, 0);
        QCOMPARE(table.value("type").toString(), QString("table"));
        const QVariantList header = table.value("header").toList();
        QCOMPARE(header.size(), 2);
        QCOMPARE(header.at(0).toMap().value("html").toString(), QString("H1"));
        const QVariantList rows = table.value("rows").toList();
        QCOMPARE(rows.size(), 1);
        const QVariantList cells = rows.at(0).toList();
        QCOMPARE(cells.at(0).toMap().value("html").toString(), QString("a"));
        QCOMPARE(cells.at(0).toMap().value("align").toString(), QString("left"));
        QCOMPARE(cells.at(1).toMap().value("align").toString(), QString("right"));
        QCOMPARE(block(doc, 1).value("html").toString(), QString("after"));
    }

    void testStandaloneImageBecomesImageBlock()
    {
        const QVariantMap doc = renderPlain("![Logo](img/logo.png \"Title\")\n", "/tmp/docs");

        QCOMPARE(blocksOf(doc).size(), 1);
        const QVariantMap image = block(doc, 0);
        QCOMPARE(image.value("type").toString(), QString("image"));
        QCOMPARE(image.value("source").toString(), QString("file:///tmp/docs/img/logo.png"));
        QCOMPARE(image.value("alt").toString(), QString("Logo"));
        QCOMPARE(image.value("title").toString(), QString("Title"));
    }

    void testInlineImageAndRelativeLinkResolveAgainstBaseDir()
    {
        const QVariantMap doc = renderPlain(
            "See ![i](i.png) and [doc](./docs/a.md) or [web](https://a.b).\n", "/tmp/d");

        const QString html = block(doc, 0).value("html").toString();
        QVERIFY2(html.contains("<img src=\"file:///tmp/d/i.png\""), qPrintable(html));
        QVERIFY2(html.contains("href=\"file:///tmp/d/docs/a.md\""), qPrintable(html));
        QVERIFY2(html.contains("href=\"https://a.b\""), qPrintable(html));
    }

    void testParentDirectorySegmentsAreNormalised()
    {
        const QVariantMap doc = renderPlain("[up](../up.md) ![i](./a/../b.png)\n", "/tmp/d/sub");

        const QString html = block(doc, 0).value("html").toString();
        QVERIFY2(html.contains("href=\"file:///tmp/d/up.md\""), qPrintable(html));
        QVERIFY2(html.contains("<img src=\"file:///tmp/d/sub/b.png\""), qPrintable(html));
    }

    void testFrontMatterIsParsedAndKeptOutOfBlocks()
    {
        const QVariantMap doc = renderPlain("---\ntitle: My Doc\ntags: [a, b]\n---\n\n# Head\n");

        const QVariantList frontMatter = doc.value("frontMatter").toList();
        QCOMPARE(frontMatter.size(), 2);
        QCOMPARE(frontMatter.at(0).toMap().value("key").toString(), QString("title"));
        QCOMPARE(frontMatter.at(0).toMap().value("value").toString(), QString("My Doc"));
        QCOMPARE(frontMatter.at(1).toMap().value("value").toString(), QString("[a, b]"));
        QCOMPARE(doc.value("title").toString(), QString("My Doc"));
        QCOMPARE(blocksOf(doc).size(), 1);
        QCOMPARE(block(doc, 0).value("type").toString(), QString("heading"));
    }

    void testTitleFallsBackToFirstTopLevelHeading()
    {
        const QVariantMap doc = renderPlain("intro\n\n## Not it\n\n# Real Title\n");

        QCOMPARE(doc.value("title").toString(), QString("Real Title"));
        QCOMPARE(doc.value("frontMatter").toList().size(), 0);
    }

    void testOutlineListsHeadingsWithBlockIndex()
    {
        const QVariantMap doc = renderPlain("# A\n\ntext\n\n## B\n\n### C\n");

        const QVariantList outline = doc.value("outline").toList();
        QCOMPARE(outline.size(), 3);
        QCOMPARE(outline.at(1).toMap().value("level").toInt(), 2);
        QCOMPARE(outline.at(1).toMap().value("text").toString(), QString("B"));
        QCOMPARE(outline.at(1).toMap().value("blockIndex").toInt(), 2);
    }

    void testStatsCountProseWordsAndReadingTime()
    {
        QStringList words;
        for (int i = 0; i < 450; ++i)
            words.append(QStringLiteral("w%1").arg(i));
        const QString markdown = "# T\n\n" + words.join(' ')
            + "\n\n```\nx y z\n```\n\n[l](http://a) ![i](i.png)\n";

        const QVariantMap stats = renderPlain(markdown).value("stats").toMap();
        QCOMPARE(stats.value("words").toInt(), 452);
        QCOMPARE(stats.value("readingMinutes").toInt(), 3);
        QCOMPARE(stats.value("headings").toInt(), 1);
        QCOMPARE(stats.value("codeBlocks").toInt(), 1);
        QCOMPARE(stats.value("links").toInt(), 1);
        QCOMPARE(stats.value("images").toInt(), 1);
    }

    void testFencedCodeIsHighlightedWithBat()
    {
        if (!batAvailable())
            QSKIP("bat is not installed");

        MarkdownRenderer renderer;
        const QVariantMap doc = renderer.render("```json\n{\"a\": 1}\n```\n");

        const QString html = block(doc, 0).value("html").toString();
        QVERIFY2(html.startsWith("<pre"), qPrintable(html));
        QVERIFY2(html.contains("<span style="), qPrintable(html));
        // bat wraps every token in its own span; compare the text only.
        QString text = html;
        text.remove(QRegularExpression("<[^>]+>"));
        QCOMPARE(text, QString("{&quot;a&quot;: 1}"));
    }

    void testHighlightingStopsAfterTheConfiguredBlockCount()
    {
        if (!batAvailable())
            QSKIP("bat is not installed");

        MarkdownRenderer renderer;
        QVariantMap options;
        options["maxHighlightedBlocks"] = 2;
        const QVariantMap doc = renderer.render(
            "```json\n1\n```\n\n```json\n2\n```\n\n```json\n3\n```\n", QString(), options);

        QVERIFY(!block(doc, 0).value("html").toString().isEmpty());
        QVERIFY(!block(doc, 1).value("html").toString().isEmpty());
        QCOMPARE(block(doc, 2).value("html").toString(), QString());
        QCOMPARE(block(doc, 2).value("code").toString(), QString("3"));
    }

    void testRemoteImagesAreNotEmbedded()
    {
        const QVariantMap doc = renderPlain(
            "![Badge](https://img.example/b.svg)\n\nInline ![alt text](http://x.y/i.png) here.\n", "/tmp/d");

        const QVariantMap image = block(doc, 0);
        QCOMPARE(image.value("type").toString(), QString("image"));
        QCOMPARE(image.value("remote").toBool(), true);
        QCOMPARE(image.value("source").toString(), QString("https://img.example/b.svg"));

        const QString html = block(doc, 1).value("html").toString();
        QVERIFY2(!html.contains("<img"), qPrintable(html));
        QVERIFY2(html.contains("alt text"), qPrintable(html));
    }

    void testLocalImageBlockIsNotRemote()
    {
        const QVariantMap doc = renderPlain("![Logo](logo.png)\n", "/tmp/d");

        QCOMPARE(block(doc, 0).value("remote").toBool(), false);
    }

    void testUnknownLanguageFallsBackToPlainCode()
    {
        MarkdownRenderer renderer;
        const QVariantMap doc = renderer.render("```no-such-lang-xyz\nfoo\n```\n\n```\nbare\n```\n");

        QCOMPARE(block(doc, 0).value("html").toString(), QString());
        QCOMPARE(block(doc, 0).value("code").toString(), QString("foo"));
        QCOMPARE(block(doc, 1).value("html").toString(), QString());
    }
};

QTEST_MAIN(TestMarkdownRenderer)
#include "tst_markdownrenderer.moc"
