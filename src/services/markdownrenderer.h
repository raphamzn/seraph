#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

// Turns markdown source into a block description that MarkdownView.qml
// renders with native QML delegates (headings, code cards, tables, ...).
//
// Parsing is done by Qt's own CommonMark/GitHub parser (QTextDocument), so
// no external tool is involved; `bat` is only used, when installed, to
// colour fenced code blocks.
class MarkdownRenderer : public QObject
{
    Q_OBJECT

public:
    explicit MarkdownRenderer(QObject *parent = nullptr);

    // Result keys:
    //   blocks      list of {type, ...} maps in document order
    //   outline     list of {level, text, blockIndex} for every heading
    //   frontMatter list of {key, value} parsed from a leading YAML block
    //   title       front matter title, else the first H1, else ""
    //   stats       {words, readingMinutes, headings, codeBlocks, links, images}
    //
    // baseDir resolves relative image/link targets. Options:
    //   highlightCode        bool   run bat on fenced code (default true)
    //   maxHighlightedBlocks int    fences to run bat on, at most (default 24)
    //   batTheme             string bat --theme to use (default: bat's own default)
    //   codeBackground       color  inline-code background (default: none)
    //   codeColor            color  inline-code foreground (default: inherit)
    Q_INVOKABLE QVariantMap render(const QString &markdown, const QString &baseDir = QString(),
                                   const QVariantMap &options = QVariantMap()) const;
};
