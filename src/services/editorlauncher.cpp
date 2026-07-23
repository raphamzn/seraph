#include "services/editorlauncher.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QVariantMap>

namespace {

struct Candidate {
    const char *id;
    const char *name;
    QStringList bins;         // launcher binaries to look for in PATH
    QStringList desktops;     // .desktop ids to fall back to
    const char *icon;         // icon-theme name
};

// Curated list of common editors/IDEs. `bins` are checked in PATH first;
// `desktops` are the fallback for GUI installs without a PATH launcher.
QVector<Candidate> knownEditors()
{
    return {
        {"vscode", "VS Code", {"code"}, {"code.desktop", "visual-studio-code.desktop"}, "vscode"},
        {"vscode-insiders", "VS Code Insiders", {"code-insiders"}, {"code-insiders.desktop"}, "vscode-insiders"},
        {"vscodium", "VSCodium", {"codium", "vscodium"}, {"codium.desktop", "vscodium.desktop"}, "vscodium"},
        {"cursor", "Cursor", {"cursor"}, {"cursor.desktop"}, "cursor"},
        {"windsurf", "Windsurf", {"windsurf"}, {"windsurf.desktop"}, "windsurf"},
        // On Arch the zed binary ships as `zeditor` to avoid a name clash.
        {"zed", "Zed", {"zeditor", "zed"}, {"dev.zed.Zed.desktop", "zed.desktop"}, "zed"},
        {"sublime", "Sublime Text", {"subl"}, {"sublime_text.desktop"}, "sublime-text"},
        {"phpstorm", "PhpStorm", {"phpstorm"}, {"phpstorm.desktop", "jetbrains-phpstorm.desktop"}, "phpstorm"},
        {"webstorm", "WebStorm", {"webstorm"}, {"webstorm.desktop", "jetbrains-webstorm.desktop"}, "webstorm"},
        {"pycharm", "PyCharm", {"pycharm"}, {"pycharm.desktop", "jetbrains-pycharm.desktop"}, "pycharm"},
        {"intellij", "IntelliJ IDEA", {"idea"}, {"jetbrains-idea.desktop", "jetbrains-idea-ce.desktop", "idea.desktop"}, "intellij-idea"},
        {"goland", "GoLand", {"goland"}, {"goland.desktop", "jetbrains-goland.desktop"}, "goland"},
        {"clion", "CLion", {"clion"}, {"clion.desktop", "jetbrains-clion.desktop"}, "clion"},
        {"rubymine", "RubyMine", {"rubymine"}, {"rubymine.desktop", "jetbrains-rubymine.desktop"}, "rubymine"},
        {"rider", "Rider", {"rider"}, {"rider.desktop", "jetbrains-rider.desktop"}, "rider"},
        {"datagrip", "DataGrip", {"datagrip"}, {"datagrip.desktop", "jetbrains-datagrip.desktop"}, "datagrip"},
        {"androidstudio", "Android Studio", {"android-studio"}, {"android-studio.desktop"}, "android-studio"},
        {"fleet", "Fleet", {"fleet"}, {"fleet.desktop"}, "fleet"},
    };
}

bool desktopExists(const QString &id)
{
    return !QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id).isEmpty();
}

} // namespace

EditorLauncher::EditorLauncher(QObject *parent) : QObject(parent)
{
    detect();
}

void EditorLauncher::detect()
{
    for (const Candidate &c : knownEditors()) {
        Editor ed;
        for (const QString &bin : c.bins) {
            // Store the absolute path so launching is immune to any PATH
            // difference between now and when the user clicks.
            const QString exe = QStandardPaths::findExecutable(bin);
            if (!exe.isEmpty()) {
                ed.program = exe;
                break;
            }
        }
        if (ed.program.isEmpty()) {
            for (const QString &d : c.desktops) {
                if (desktopExists(d)) {
                    ed.desktopId = d;
                    break;
                }
            }
        }
        if (ed.program.isEmpty() && ed.desktopId.isEmpty())
            continue;
        ed.id = QString::fromLatin1(c.id);
        ed.name = QString::fromLatin1(c.name);
        ed.iconName = QString::fromLatin1(c.icon);
        m_editors.append(ed);
    }
}

QVariantList EditorLauncher::editors() const
{
    QVariantList list;
    for (const Editor &ed : m_editors) {
        list.append(QVariantMap{
            {QStringLiteral("id"), ed.id},
            {QStringLiteral("name"), ed.name},
            {QStringLiteral("iconName"), ed.iconName},
        });
    }
    return list;
}

void EditorLauncher::openIn(const QString &editorId, const QString &path)
{
    if (editorId.isEmpty() || path.isEmpty())
        return;

    const Editor *target = nullptr;
    for (const Editor &ed : m_editors) {
        if (ed.id == editorId) {
            target = &ed;
            break;
        }
    }
    if (!target)
        return;

    const QString localPath = QFileInfo(path).absoluteFilePath();
    const bool sandboxed = QFileInfo::exists(QStringLiteral("/.flatpak-info"));

    QString program;
    QStringList args;
    if (!target->program.isEmpty()) {
        program = target->program;
        args = {localPath};
    } else {
        program = QStringLiteral("gtk-launch");
        args = {target->desktopId, localPath};
    }

    // Under Flatpak the editor lives on the host, so route through the portal.
    if (sandboxed) {
        args.prepend(program);
        args.prepend(QStringLiteral("--host"));
        program = QStringLiteral("flatpak-spawn");
    }

    QProcess::startDetached(program, args);
}
