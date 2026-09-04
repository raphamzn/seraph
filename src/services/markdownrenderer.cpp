#include "services/markdownrenderer.h"
#include "services/bathighlighter.h"

#include <QRegularExpression>
#include <QtMath>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextList>
#include <QTextTable>
#include <QUrl>
#include <QVariantList>

namespace {

// Qt's GitHub dialect turns _text_ into underline (md4c's MD_FLAG_UNDERLINE,
// 0x4000). CommonMark readers expect emphasis there, so that flag is masked
// out while keeping tables, strikethrough and task lists.
QTextDocument::MarkdownFeatures parserFeatures()
{
    const int underlineFlag = 0x4000;
    return QTextDocument::MarkdownFeatures(
        QTextDocument::MarkdownFeature(int(QTextDocument::MarkdownDialectGitHub) & ~underlineFlag));
}

// Relative image/link targets are resolved against the document's folder as
// file:// URLs. Absolute paths, anchors and anything with a scheme pass
// through untouched. The target is already URL syntax (the author wrote it),
// so only baseDir is encoded — re-encoding the target would double escapes.
QString resolveTarget(const QString &target, const QString &baseDir)
{
    if (target.isEmpty() || baseDir.isEmpty())
        return target;
    static const QRegularExpression absolute(QStringLiteral("^([a-zA-Z][a-zA-Z0-9+.\\-]*:|/|#)"));
    if (absolute.match(target).hasMatch())
        return target;
    QString relative = target;
    if (relative.startsWith(QStringLiteral("./")))
        relative.remove(0, 2);
    const QUrl joined(QUrl::fromLocalFile(baseDir).toString(QUrl::FullyEncoded) + QLatin1Char('/') + relative);
    // "../" and "./" segments are folded so QML gets a plain path to open.
    return joined.adjusted(QUrl::NormalizePathSegments).toString(QUrl::FullyEncoded);
}

// A leading YAML block delimited by `---` lines. Only flat `key: value`
// pairs are understood; indented continuation lines (lists, nested maps)
// are folded into the previous key's value so they still show up.
QVariantList extractFrontMatter(QString *markdown)
{
    static const QRegularExpression frontMatter(
        QStringLiteral("\\A---[ \\t]*\\r?\\n(.*?)\\r?\\n---[ \\t]*(?:\\r?\\n|\\z)"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = frontMatter.match(*markdown);
    if (!match.hasMatch())
        return {};

    static const QRegularExpression pair(QStringLiteral("^([A-Za-z0-9_.\\-]+)\\s*:\\s*(.*)$"));
    QVariantList entries;
    const QStringList lines = match.captured(1).split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QRegularExpressionMatch pairMatch = pair.match(rawLine.trimmed());
        const bool continuation = rawLine.startsWith(QLatin1Char(' ')) || rawLine.startsWith(QLatin1Char('\t'))
            || line.startsWith(QStringLiteral("- "));
        if (pairMatch.hasMatch() && !continuation) {
            QVariantMap entry;
            entry["key"] = pairMatch.captured(1);
            QString value = pairMatch.captured(2).trimmed();
            if (value.size() >= 2 && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
                                      || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))))
                value = value.mid(1, value.size() - 2);
            entry["value"] = value;
            entries.append(entry);
        } else if (!entries.isEmpty()) {
            QVariantMap last = entries.last().toMap();
            QString piece = line;
            if (piece.startsWith(QStringLiteral("- ")))
                piece.remove(0, 2);
            const QString previous = last.value("value").toString();
            last["value"] = previous.isEmpty() ? piece : previous + QStringLiteral(", ") + piece;
            entries.last() = last;
        }
    }

    markdown->remove(0, match.capturedLength(0));
    return entries;
}

// Qt reports every fenced line as a block with identical properties, so two
// fences with only a blank line between them look exactly like one. The
// source knows better: count the lines inside each fence, in document
// order, and the collector splits merged runs back up.
QList<int> fenceLineCounts(const QString &source)
{
    static const QRegularExpression fence(QStringLiteral("^[ \\t>]*(`{3,}|~{3,})"));
    QList<int> counts;
    bool inFence = false;
    QChar fenceChar;
    int fenceLength = 0;
    int lines = 0;
    const QStringList sourceLines = source.split(QLatin1Char('\n'));
    for (const QString &line : sourceLines) {
        const QRegularExpressionMatch match = fence.match(line);
        if (!inFence) {
            if (match.hasMatch()) {
                inFence = true;
                fenceChar = match.captured(1).at(0);
                fenceLength = match.captured(1).size();
                lines = 0;
            }
            continue;
        }
        const bool closes = match.hasMatch() && match.captured(1).at(0) == fenceChar
            && match.captured(1).size() >= fenceLength
            && line.mid(match.capturedEnd(1)).trimmed().isEmpty();
        if (closes) {
            counts.append(lines);
            inFence = false;
            continue;
        }
        ++lines;
    }
    if (inFence)
        counts.append(lines);
    return counts;
}

// Qt turns a hard line break (two trailing spaces or a backslash) into a
// new block that looks exactly like a new paragraph. A line separator
// character survives the parser inside the paragraph instead, so hard
// breaks are rewritten to one before parsing and inlineHtml emits <br>.
// Fenced and indented code, where trailing spaces are content, are skipped.
QString markHardBreaks(const QString &source)
{
    static const QRegularExpression fence(QStringLiteral("^[ \\t>]*(`{3,}|~{3,})"));
    QStringList lines = source.split(QLatin1Char('\n'));
    bool inFence = false;
    QChar fenceChar;
    int fenceLength = 0;
    bool previousBlank = true;
    bool inIndentedCode = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString &line = lines[i];
        const QRegularExpressionMatch match = fence.match(line);
        if (inFence) {
            if (match.hasMatch() && match.captured(1).at(0) == fenceChar
                && match.captured(1).size() >= fenceLength
                && line.mid(match.capturedEnd(1)).trimmed().isEmpty())
                inFence = false;
            previousBlank = false;
            continue;
        }
        if (match.hasMatch()) {
            inFence = true;
            fenceChar = match.captured(1).at(0);
            fenceLength = match.captured(1).size();
            previousBlank = false;
            continue;
        }

        const bool blank = line.trimmed().isEmpty();
        const bool indented = line.startsWith(QStringLiteral("    ")) || line.startsWith(QLatin1Char('\t'));
        if (indented && (previousBlank || inIndentedCode))
            inIndentedCode = true;
        else if (!blank)
            inIndentedCode = false;
        if (blank) {
            previousBlank = true;
            continue;
        }
        previousBlank = false;
        if (inIndentedCode)
            continue;

        // A break only means something when the paragraph goes on.
        const bool continues = i + 1 < lines.size() && !lines.at(i + 1).trimmed().isEmpty();
        if (!continues)
            continue;
        if (line.endsWith(QStringLiteral("  "))) {
            int end = line.size();
            while (end > 0 && line.at(end - 1) == QLatin1Char(' '))
                --end;
            line = line.left(end) + QChar(QChar::LineSeparator);
        } else if (line.endsWith(QLatin1Char('\\'))) {
            int backslashes = 0;
            for (int j = line.size() - 1; j >= 0 && line.at(j) == QLatin1Char('\\'); --j)
                ++backslashes;
            if (backslashes % 2 == 1) {
                line.chop(1);
                line += QChar(QChar::LineSeparator);
            }
        }
    }
    return lines.join(QLatin1Char('\n'));
}

// Block text for titles, outlines and word counts: no image placeholders,
// no line separators.
QString plainText(const QTextBlock &block)
{
    QString text = block.text();
    text.remove(QChar::ObjectReplacementCharacter);
    text.replace(QChar(QChar::LineSeparator), QLatin1Char(' '));
    return text.simplified();
}

// Only what the file manager can read locally is embedded. Anything fetched
// over the network stays out of a file preview.
bool isRemoteSource(const QString &source)
{
    static const QRegularExpression scheme(QStringLiteral("^([a-zA-Z][a-zA-Z0-9+.\\-]*):"));
    const QRegularExpressionMatch match = scheme.match(source);
    if (!match.hasMatch())
        return false;
    const QString name = match.captured(1).toLower();
    return name != QLatin1String("file") && name != QLatin1String("data")
        && name != QLatin1String("qrc") && name != QLatin1String("image");
}

struct InlineContext {
    // Headings and table headers are bold as a whole (QML sets the font
    // weight), so a nested <b> would only fight it and is dropped.
    bool implicitBold = false;
    QString baseDir;
    QString codeBackground;
    QString codeColor;
};

QString inlineCodeStyle(const InlineContext &context)
{
    QString style = QStringLiteral("font-family:monospace");
    if (!context.codeBackground.isEmpty())
        style += QStringLiteral(";background-color:") + context.codeBackground;
    if (!context.codeColor.isEmpty())
        style += QStringLiteral(";color:") + context.codeColor;
    return style;
}

QString imageTag(const QTextImageFormat &image, const InlineContext &context)
{
    const QString alt = image.property(QTextFormat::ImageAltText).toString();
    const QString source = resolveTarget(image.name(), context.baseDir);
    if (isRemoteSource(source))
        return (alt.isEmpty() ? QStringLiteral("[image]") : alt).toHtmlEscaped();
    return QStringLiteral("<img src=\"") + source.toHtmlEscaped()
        + QStringLiteral("\" alt=\"") + alt.toHtmlEscaped() + QStringLiteral("\">");
}

QString inlineHtml(const QTextBlock &block, const InlineContext &context)
{
    QString html;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid())
            continue;
        const QTextCharFormat format = fragment.charFormat();

        if (format.isImageFormat()) {
            html += imageTag(format.toImageFormat(), context);
            continue;
        }

        QString text = fragment.text().toHtmlEscaped();

        if (format.fontFixedPitch()) {
            text = QStringLiteral("<span style=\"") + inlineCodeStyle(context).toHtmlEscaped()
                + QStringLiteral("\">") + text + QStringLiteral("</span>");
        }
        const bool bold = format.fontWeight() >= QFont::Bold && !context.implicitBold;
        if (bold)
            text = QStringLiteral("<b>") + text + QStringLiteral("</b>");
        if (format.fontItalic())
            text = QStringLiteral("<i>") + text + QStringLiteral("</i>");
        if (format.fontStrikeOut())
            text = QStringLiteral("<s>") + text + QStringLiteral("</s>");
        if (format.isAnchor()) {
            text = QStringLiteral("<a href=\"")
                + resolveTarget(format.anchorHref(), context.baseDir).toHtmlEscaped()
                + QStringLiteral("\">") + text + QStringLiteral("</a>");
        }

        html += text;
    }

    // Hard breaks: the separator plus the soft-break space that follows it.
    static const QRegularExpression hardBreak(QStringLiteral("\u2028\\s*"));
    html.replace(hardBreak, QStringLiteral("<br>"));
    while (html.endsWith(QStringLiteral("<br>")))
        html.chop(4);
    while (html.startsWith(QStringLiteral("<br>")))
        html.remove(0, 4);
    return html;
}

// A paragraph made of nothing but one image is promoted to an image block,
// so QML can size and caption it instead of squeezing it into a text run.
bool soleImage(const QTextBlock &block, QTextImageFormat *image)
{
    int fragments = 0;
    for (auto it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid())
            continue;
        ++fragments;
        if (fragments > 1 || !fragment.charFormat().isImageFormat())
            return false;
        *image = fragment.charFormat().toImageFormat();
    }
    return fragments == 1;
}

QString alignmentName(Qt::Alignment alignment)
{
    switch (alignment & Qt::AlignHorizontal_Mask) {
    case Qt::AlignRight:
        return QStringLiteral("right");
    case Qt::AlignHCenter:
        return QStringLiteral("center");
    default:
        return QStringLiteral("left");
    }
}

// Walks the parsed document and groups Qt's per-line/per-item blocks into
// the coarser blocks the QML view renders: one code card per fence, one
// quote per run of quoted paragraphs, one list per top-level QTextList.
class BlockCollector
{
public:
    struct Stats {
        int words = 0;
        int headings = 0;
        int codeBlocks = 0;
        int links = 0;
        int images = 0;
    };

    struct Options {
        bool highlightCode = true;
        int maxHighlightedBlocks = 24;
        QString batTheme;
        QList<int> fenceLineCounts;
    };

    BlockCollector(const QString &baseDir, const InlineContext &context, const Options &options)
        : m_baseDir(baseDir), m_context(context), m_options(options)
    {
    }

    QVariantList blocks()
    {
        flushAll();
        return m_blocks;
    }

    QVariantList outline() const { return m_outline; }
    QString firstTopHeading() const { return m_firstTopHeading; }
    Stats stats() const { return m_stats; }

    void visitFrame(QTextFrame *frame)
    {
        for (auto it = frame->begin(); !it.atEnd(); ++it) {
            if (QTextFrame *child = it.currentFrame()) {
                flushAll();
                if (QTextTable *table = qobject_cast<QTextTable *>(child))
                    visitTable(table);
                else
                    visitFrame(child);
            } else {
                visitBlock(it.currentBlock());
            }
        }
    }

private:
    // Words, links and images of a prose block (everything but code).
    void countProse(const QTextBlock &block)
    {
        m_stats.words += plainText(block).split(QLatin1Char(' '), Qt::SkipEmptyParts).size();

        QString previousHref;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextCharFormat format = it.fragment().charFormat();
            if (format.isImageFormat())
                ++m_stats.images;
            const QString href = format.isAnchor() ? format.anchorHref() : QString();
            if (!href.isEmpty() && href != previousHref)
                ++m_stats.links;
            previousHref = href;
        }
    }

    void visitBlock(const QTextBlock &block)
    {
        const QTextBlockFormat format = block.blockFormat();

        if (format.hasProperty(QTextFormat::BlockCodeLanguage)) {
            const QString language = format.stringProperty(QTextFormat::BlockCodeLanguage);
            const bool fenced = format.hasProperty(QTextFormat::BlockCodeFence);
            if (!m_inCode || language != m_codeLanguage || fenced != m_codeFenced) {
                flushAll();
                m_inCode = true;
                m_codeLanguage = language;
                m_codeFenced = fenced;
            }
            m_codeLines.append(block.text());
            return;
        }
        flushCode();
        countProse(block);

        if (QTextList *list = block.textList()) {
            visitListItem(block, list);
            return;
        }
        flushList();

        if (format.intProperty(QTextFormat::BlockQuoteLevel) > 0) {
            QVariantMap item;
            item["type"] = QStringLiteral("paragraph");
            item["html"] = inlineHtml(block, m_context);
            item["level"] = format.intProperty(QTextFormat::BlockQuoteLevel);
            m_quoteItems.append(item);
            return;
        }
        flushQuote();

        if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            appendParagraph(block);
            QVariantMap rule;
            rule["type"] = QStringLiteral("rule");
            m_blocks.append(rule);
            return;
        }

        if (format.headingLevel() > 0) {
            InlineContext headingContext = m_context;
            headingContext.implicitBold = true;
            QVariantMap heading;
            heading["type"] = QStringLiteral("heading");
            heading["level"] = format.headingLevel();
            heading["text"] = plainText(block);
            heading["html"] = inlineHtml(block, headingContext);

            QVariantMap entry;
            entry["level"] = format.headingLevel();
            entry["text"] = plainText(block);
            entry["blockIndex"] = m_blocks.size();
            m_outline.append(entry);
            if (format.headingLevel() == 1 && m_firstTopHeading.isEmpty())
                m_firstTopHeading = plainText(block);
            ++m_stats.headings;

            m_blocks.append(heading);
            return;
        }

        QTextImageFormat image;
        if (soleImage(block, &image)) {
            QVariantMap imageBlock;
            imageBlock["type"] = QStringLiteral("image");
            const QString source = resolveTarget(image.name(), m_baseDir);
            imageBlock["source"] = source;
            imageBlock["remote"] = isRemoteSource(source);
            imageBlock["alt"] = image.property(QTextFormat::ImageAltText).toString();
            imageBlock["title"] = image.property(QTextFormat::ImageTitle).toString();
            m_blocks.append(imageBlock);
            return;
        }

        appendParagraph(block);
    }

    void appendParagraph(const QTextBlock &block)
    {
        const QString html = inlineHtml(block, m_context);
        if (html.trimmed().isEmpty())
            return;
        QVariantMap paragraph;
        paragraph["type"] = QStringLiteral("paragraph");
        paragraph["html"] = html;
        const QString align = alignmentName(block.blockFormat().alignment());
        if (align != QLatin1String("left"))
            paragraph["align"] = align;
        m_blocks.append(paragraph);
    }

    void visitListItem(const QTextBlock &block, QTextList *list)
    {
        const QTextListFormat listFormat = list->format();
        const int depth = qMax(1, listFormat.indent());

        // Each top-level QTextList is one markdown list; nested lists are
        // separate QTextList objects that stay inside the current group.
        if (depth == 1 && (!m_inList || list != m_listRoot)) {
            flushList();
            m_inList = true;
            m_listRoot = list;
        } else if (!m_inList) {
            m_inList = true;
            m_listRoot = nullptr;
        }

        QVariantMap item;
        item["html"] = inlineHtml(block, m_context);
        item["depth"] = depth;
        item["ordered"] = listFormat.style() <= QTextListFormat::ListDecimal;
        item["number"] = listFormat.start() + list->itemNumber(block);
        int checked = -1;
        switch (block.blockFormat().marker()) {
        case QTextBlockFormat::MarkerType::Unchecked:
            checked = 0;
            break;
        case QTextBlockFormat::MarkerType::Checked:
            checked = 1;
            break;
        default:
            break;
        }
        item["checked"] = checked;
        m_listItems.append(item);
    }

    void visitTable(QTextTable *table)
    {
        QVariantMap tableBlock;
        tableBlock["type"] = QStringLiteral("table");
        QVariantList header;
        QVariantList rows;
        InlineContext headerContext = m_context;
        headerContext.implicitBold = true;
        for (int row = 0; row < table->rows(); ++row) {
            QVariantList cells;
            for (int column = 0; column < table->columns(); ++column) {
                const QTextTableCell cell = table->cellAt(row, column);
                QStringList parts;
                for (auto it = cell.begin(); !it.atEnd(); ++it) {
                    countProse(it.currentBlock());
                    const QString html = inlineHtml(it.currentBlock(), row == 0 ? headerContext : m_context);
                    if (!html.isEmpty())
                        parts.append(html);
                }
                QVariantMap cellMap;
                cellMap["html"] = parts.join(QStringLiteral("<br>"));
                cellMap["align"] = alignmentName(cell.firstCursorPosition().blockFormat().alignment());
                cells.append(cellMap);
            }
            // Markdown tables always start with a header row.
            if (row == 0)
                header = cells;
            else
                rows.append(QVariant(cells));
        }
        tableBlock["header"] = header;
        tableBlock["rows"] = rows;
        m_blocks.append(tableBlock);
    }

    // Lines of each fence in the current run, from the source scan. Falls
    // back to one block when the scan and the parser disagree (a fence the
    // scan could not see), and stops trusting the scan from then on.
    QList<int> fenceSegments()
    {
        QList<int> segments;
        if (!m_codeFenced || m_fenceSyncLost)
            return segments;
        int total = 0;
        int index = m_nextFence;
        while (index < m_options.fenceLineCounts.size() && total < m_codeLines.size()) {
            segments.append(m_options.fenceLineCounts.at(index));
            total += m_options.fenceLineCounts.at(index);
            ++index;
        }
        if (total != m_codeLines.size()) {
            m_fenceSyncLost = true;
            return {};
        }
        m_nextFence = index;
        return segments;
    }

    void flushCode()
    {
        if (!m_inCode)
            return;
        QList<int> segments = fenceSegments();
        if (segments.isEmpty())
            segments.append(m_codeLines.size());

        int offset = 0;
        for (int count : segments) {
            const QStringList lines = m_codeLines.mid(offset, count);
            offset += count;
            QVariantMap code;
            code["type"] = QStringLiteral("code");
            code["language"] = m_codeLanguage;
            const QString source = lines.join(QLatin1Char('\n'));
            code["code"] = source;
            code["lineCount"] = lines.size();
            code["html"] = highlightedHtml(source, m_codeLanguage);
            ++m_stats.codeBlocks;
            m_blocks.append(code);
        }
        m_codeLines.clear();
        m_codeLanguage.clear();
        m_codeFenced = false;
        m_inCode = false;
    }

    void flushQuote()
    {
        if (m_quoteItems.isEmpty())
            return;
        QVariantMap quote;
        quote["type"] = QStringLiteral("quote");
        quote["items"] = m_quoteItems;
        m_blocks.append(quote);
        m_quoteItems.clear();
    }

    void flushList()
    {
        if (!m_inList)
            return;
        QVariantMap list;
        list["type"] = QStringLiteral("list");
        list["items"] = m_listItems;
        m_blocks.append(list);
        m_listItems.clear();
        m_listRoot = nullptr;
        m_inList = false;
    }

    void flushAll()
    {
        flushCode();
        flushList();
        flushQuote();
    }

    // Rich text for a fenced block, or "" when bat is off, missing, or does
    // not know the language (the QML side then shows the plain source).
    QString highlightedHtml(const QString &source, const QString &language)
    {
        if (!m_options.highlightCode || language.isEmpty()
            || m_highlighted >= m_options.maxHighlightedBlocks)
            return {};
        ++m_highlighted;
        QString error;
        QByteArray colored = BatHighlighter::highlightSnippet(source, language, m_options.batTheme, &error);
        if (colored.isEmpty())
            return {};
        while (colored.endsWith('\n'))
            colored.chop(1);
        return BatHighlighter::ansiToHtml(colored);
    }

    QString m_baseDir;
    InlineContext m_context;
    Options m_options;
    QVariantList m_blocks;
    QVariantList m_outline;
    QString m_firstTopHeading;
    Stats m_stats;
    int m_highlighted = 0;

    bool m_inCode = false;
    bool m_codeFenced = false;
    QString m_codeLanguage;
    QStringList m_codeLines;
    int m_nextFence = 0;
    bool m_fenceSyncLost = false;

    QVariantList m_quoteItems;

    bool m_inList = false;
    QTextList *m_listRoot = nullptr;
    QVariantList m_listItems;
};

} // namespace

MarkdownRenderer::MarkdownRenderer(QObject *parent)
    : QObject(parent)
{
}

QVariantMap MarkdownRenderer::render(const QString &markdown, const QString &baseDir,
                                     const QVariantMap &options) const
{
    QString source = markdown;
    const QVariantList frontMatter = extractFrontMatter(&source);
    source = markHardBreaks(source);

    QTextDocument document;
    document.setMarkdown(source, parserFeatures());

    InlineContext context;
    context.baseDir = baseDir;
    context.codeBackground = options.value("codeBackground").toString();
    context.codeColor = options.value("codeColor").toString();
    BlockCollector::Options collectorOptions;
    collectorOptions.highlightCode = options.value("highlightCode", true).toBool();
    collectorOptions.maxHighlightedBlocks = options.value("maxHighlightedBlocks", 24).toInt();
    collectorOptions.batTheme = options.value("batTheme").toString();
    collectorOptions.fenceLineCounts = fenceLineCounts(source);

    BlockCollector collector(baseDir, context, collectorOptions);
    collector.visitFrame(document.rootFrame());

    QVariantMap result;
    result["blocks"] = collector.blocks();
    result["outline"] = collector.outline();
    result["frontMatter"] = frontMatter;

    QString title;
    for (const QVariant &entry : frontMatter) {
        const QVariantMap map = entry.toMap();
        if (map.value("key").toString().compare(QLatin1String("title"), Qt::CaseInsensitive) == 0) {
            title = map.value("value").toString();
            break;
        }
    }
    if (title.isEmpty())
        title = collector.firstTopHeading();
    result["title"] = title;

    const BlockCollector::Stats stats = collector.stats();
    QVariantMap statsMap;
    statsMap["words"] = stats.words;
    statsMap["readingMinutes"] = stats.words > 0 ? qCeil(stats.words / 200.0) : 0;
    statsMap["headings"] = stats.headings;
    statsMap["codeBlocks"] = stats.codeBlocks;
    statsMap["links"] = stats.links;
    statsMap["images"] = stats.images;
    result["stats"] = statsMap;
    return result;
}
