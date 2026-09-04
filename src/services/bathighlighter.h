#pragma once

#include <QByteArray>
#include <QString>

// Syntax highlighting through the external `bat` tool. Shared by the text
// preview (whole files) and the markdown preview (fenced code blocks).
namespace BatHighlighter {

// Absolute path of `bat` (or Debian's `batcat`), empty when not installed.
QString executable();

// Convert bat's ANSI-colored output into Qt rich text (a <pre> block).
QString ansiToHtml(const QByteArray &ansiText);

// Highlight the first maxLines of a file. Returns ANSI-colored bytes, or an
// empty array with *error set when bat is missing, fails, or times out.
QByteArray highlightFile(const QString &path, int maxLines, QString *error);

// Highlight a snippet fed through stdin as the given bat language name
// (fence info string: "cpp", "json", "sh"...). theme is bat's --theme; empty
// keeps bat's own default. Unknown languages make bat fail, which returns an
// empty array so callers fall back to plain text.
QByteArray highlightSnippet(const QString &code, const QString &language, const QString &theme,
                            QString *error);

}
