#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

// Detects installed code editors / IDEs and launches them against a folder
// (or file) path, powering the "Open in…" context-menu submenu. Detection runs
// once at construction: for each known editor it probes the PATH for a launcher
// binary and, failing that, checks for a matching .desktop entry (covers
// JetBrains installs via Toolbox that only ship a desktop file). Exposed to QML
// as the `editorLauncher` context property.
class EditorLauncher : public QObject
{
    Q_OBJECT
    // List of {id, name, iconName} maps for the editors that are installed.
    Q_PROPERTY(QVariantList editors READ editors CONSTANT)

public:
    explicit EditorLauncher(QObject *parent = nullptr);

    QVariantList editors() const;

    // Open `path` (a folder or file) in the editor identified by `editorId`.
    Q_INVOKABLE void openIn(const QString &editorId, const QString &path);

private:
    struct Editor {
        QString id;
        QString name;
        QString iconName;
        QString program;    // launcher binary found in PATH (empty -> use desktopId)
        QString desktopId;  // .desktop id to launch via gtk-launch (empty -> use program)
    };

    void detect();

    QVector<Editor> m_editors;
};
