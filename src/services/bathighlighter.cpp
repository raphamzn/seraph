#include "services/bathighlighter.h"

#include <QColor>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace BatHighlighter {

namespace {

QColor ansiColor(int code, bool bright)
{
    static const QColor normalColors[] = {
        QColor(QStringLiteral("#1e1e2e")), QColor(QStringLiteral("#f38ba8")),
        QColor(QStringLiteral("#a6e3a1")), QColor(QStringLiteral("#f9e2af")),
        QColor(QStringLiteral("#89b4fa")), QColor(QStringLiteral("#cba6f7")),
        QColor(QStringLiteral("#94e2d5")), QColor(QStringLiteral("#bac2de"))
    };
    static const QColor brightColors[] = {
        QColor(QStringLiteral("#45475a")), QColor(QStringLiteral("#eba0ac")),
        QColor(QStringLiteral("#a6e3a1")), QColor(QStringLiteral("#f9e2af")),
        QColor(QStringLiteral("#89dceb")), QColor(QStringLiteral("#f5c2e7")),
        QColor(QStringLiteral("#94e2d5")), QColor(QStringLiteral("#f5e0dc"))
    };

    if (code < 0 || code > 7)
        return {};
    return bright ? brightColors[code] : normalColors[code];
}

QColor ansi256Color(int index)
{
    if (index < 0)
        return {};
    if (index < 8)
        return ansiColor(index, false);
    if (index < 16)
        return ansiColor(index - 8, true);
    if (index < 232) {
        const int base = index - 16;
        const int r = base / 36;
        const int g = (base / 6) % 6;
        const int b = base % 6;
        auto scale = [](int value) { return value == 0 ? 0 : 55 + value * 40; };
        return QColor(scale(r), scale(g), scale(b));
    }
    if (index < 256) {
        const int gray = 8 + (index - 232) * 10;
        return QColor(gray, gray, gray);
    }
    return {};
}

struct AnsiState {
    bool bold = false;
    bool italic = false;
    bool underline = false;
    QColor fg;
    QColor bg;
};

QString htmlStyle(const AnsiState &state)
{
    QStringList style;
    if (state.fg.isValid())
        style.append(QStringLiteral("color:%1").arg(state.fg.name()));
    if (state.bg.isValid())
        style.append(QStringLiteral("background-color:%1").arg(state.bg.name()));
    if (state.bold)
        style.append(QStringLiteral("font-weight:700"));
    if (state.italic)
        style.append(QStringLiteral("font-style:italic"));
    if (state.underline)
        style.append(QStringLiteral("text-decoration:underline"));
    return style.join(QStringLiteral(";"));
}

void applyAnsiCode(AnsiState &state, const QList<int> &codes)
{
    QList<int> values = codes;
    if (values.isEmpty())
        values.append(0);

    for (int i = 0; i < values.size(); ++i) {
        const int code = values.at(i);
        if (code == 0) {
            state = {};
        } else if (code == 1) {
            state.bold = true;
        } else if (code == 3) {
            state.italic = true;
        } else if (code == 4) {
            state.underline = true;
        } else if (code == 22) {
            state.bold = false;
        } else if (code == 23) {
            state.italic = false;
        } else if (code == 24) {
            state.underline = false;
        } else if (code >= 30 && code <= 37) {
            state.fg = ansiColor(code - 30, false);
        } else if (code >= 90 && code <= 97) {
            state.fg = ansiColor(code - 90, true);
        } else if (code == 39) {
            state.fg = QColor();
        } else if (code >= 40 && code <= 47) {
            state.bg = ansiColor(code - 40, false);
        } else if (code >= 100 && code <= 107) {
            state.bg = ansiColor(code - 100, true);
        } else if (code == 49) {
            state.bg = QColor();
        } else if ((code == 38 || code == 48) && i + 1 < values.size()) {
            QColor color;
            const int mode = values.at(++i);
            if (mode == 5 && i + 1 < values.size()) {
                color = ansi256Color(values.at(++i));
            } else if (mode == 2 && i + 3 < values.size()) {
                color = QColor(values.at(i + 1), values.at(i + 2), values.at(i + 3));
                i += 3;
            }

            if (code == 38)
                state.fg = color;
            else
                state.bg = color;
        }
    }
}

}

QString executable()
{
    static const QString executable = []() {
        const QString bat = QStandardPaths::findExecutable(QStringLiteral("bat"));
        if (!bat.isEmpty())
            return bat;
        return QStandardPaths::findExecutable(QStringLiteral("batcat"));
    }();

    return executable;
}

QString ansiToHtml(const QByteArray &ansiText)
{
    QString html = QStringLiteral("<pre style=\"margin:0;font-family:monospace;white-space:pre;\">");
    AnsiState state;
    bool spanOpen = false;

    auto updateSpan = [&]() {
        if (spanOpen) {
            html += QStringLiteral("</span>");
            spanOpen = false;
        }
        const QString style = htmlStyle(state);
        if (!style.isEmpty()) {
            html += QStringLiteral("<span style=\"") + style.toHtmlEscaped() + QStringLiteral("\">");
            spanOpen = true;
        }
    };

    int index = 0;
    while (index < ansiText.size()) {
        if (ansiText.at(index) == '\x1b' && index + 1 < ansiText.size() && ansiText.at(index + 1) == '[') {
            const int seqStart = index + 2;
            int seqEnd = seqStart;
            while (seqEnd < ansiText.size() && ansiText.at(seqEnd) != 'm')
                ++seqEnd;

            if (seqEnd < ansiText.size() && ansiText.at(seqEnd) == 'm') {
                const QByteArray params = ansiText.mid(seqStart, seqEnd - seqStart);
                QList<int> codes;
                const QList<QByteArray> parts = params.split(';');
                for (const QByteArray &part : parts) {
                    if (part.isEmpty())
                        codes.append(0);
                    else
                        codes.append(part.toInt());
                }
                applyAnsiCode(state, codes);
                updateSpan();
                index = seqEnd + 1;
                continue;
            }
        }

        int nextEscape = ansiText.indexOf('\x1b', index);
        if (nextEscape < 0)
            nextEscape = ansiText.size();
        QString chunk = QString::fromUtf8(ansiText.mid(index, nextEscape - index));
        chunk.replace(QStringLiteral("\t"), QStringLiteral("    "));
        html += chunk.toHtmlEscaped();
        index = nextEscape;
    }

    if (spanOpen)
        html += QStringLiteral("</span>");
    html += QStringLiteral("</pre>");
    return html;
}

QByteArray highlightFile(const QString &path, int maxLines, QString *error)
{
    if (error)
        error->clear();

    const QString bat = executable();
    if (bat.isEmpty())
        return {};

    QStringList args = {
        QStringLiteral("--color=always"),
        QStringLiteral("--paging=never"),
        QStringLiteral("--style=plain"),
        QStringLiteral("--wrap=never")
    };
    if (maxLines > 0)
        args.append(QStringLiteral("--line-range=:%1").arg(maxLines));
    args.append(QStringLiteral("--"));
    args.append(path);

    QProcess proc;
    proc.start(bat, args);
    if (!proc.waitForFinished(10000)) {
        if (error)
            *error = QStringLiteral("bat preview timed out");
        return {};
    }
    if (proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return {};
    }

    return proc.readAllStandardOutput();
}

QByteArray highlightSnippet(const QString &code, const QString &language, const QString &theme,
                            QString *error)
{
    if (error)
        error->clear();

    const QString bat = executable();
    if (bat.isEmpty() || language.isEmpty())
        return {};

    QStringList args = {
        QStringLiteral("--color=always"),
        QStringLiteral("--paging=never"),
        QStringLiteral("--style=plain"),
        QStringLiteral("--wrap=never"),
        QStringLiteral("--language"),
        language
    };
    if (!theme.isEmpty())
        args << QStringLiteral("--theme") << theme;

    QProcess proc;
    proc.start(bat, args);
    if (!proc.waitForStarted(3000)) {
        if (error)
            *error = QStringLiteral("bat could not be started");
        return {};
    }
    proc.write(code.toUtf8());
    proc.closeWriteChannel();
    if (!proc.waitForFinished(10000)) {
        proc.kill();
        if (error)
            *error = QStringLiteral("bat highlighting timed out");
        return {};
    }
    if (proc.exitCode() != 0) {
        if (error)
            *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return {};
    }

    return proc.readAllStandardOutput();
}

}
