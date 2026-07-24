import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Seraph
import Quill as Q

// Command-palette style quick-open. Enumerates the current tree via the C++
// `fuzzyFinder` model and lets the user jump to any file/folder by fuzzy name.
Q.Dialog {
    id: root
    anchors.fill: parent
    z: 1002
    dialogWidth: Math.min(720, width - 40)
    title: "Quick Open"
    subtitle: "Type to fuzzy-search files and folders. Enter to jump, Esc to close."

    // Emitted when the user picks a result.
    signal activated(string path, bool isDir)

    function openAt(rootPath) {
        fuzzyFinder.open(rootPath)
        searchField.text = ""
        open()
        Qt.callLater(function() { searchField.inputItem.forceActiveFocus() })
    }

    function activateCurrent() {
        if (resultsList.currentIndex < 0 || resultsList.count === 0)
            return
        var e = fuzzyFinder.entryAt(resultsList.currentIndex)
        if (!e || !e.path)
            return
        root.close()
        root.activated(e.path, e.isDir)
    }

    onClosed: fuzzyFinder.close()

    // Keep a valid selection as the result set changes.
    Connections {
        target: fuzzyFinder
        function onCountChanged() {
            if (resultsList.currentIndex < 0 && fuzzyFinder.count > 0)
                resultsList.currentIndex = 0
            else if (resultsList.currentIndex >= fuzzyFinder.count)
                resultsList.currentIndex = fuzzyFinder.count > 0 ? 0 : -1
        }
    }

    Q.TextField {
        id: searchField
        Layout.fillWidth: true
        variant: "filled"
        icon: "Search"
        placeholder: "Search files by name..."

        onTextEdited: {
            fuzzyFinder.setQuery(text)
            resultsList.currentIndex = fuzzyFinder.count > 0 ? 0 : -1
        }
        onSubmitted: root.activateCurrent()

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Down) {
                if (resultsList.currentIndex < resultsList.count - 1)
                    resultsList.currentIndex++
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                if (resultsList.currentIndex > 0)
                    resultsList.currentIndex--
                event.accepted = true
            } else if (event.key === Qt.Key_PageDown) {
                resultsList.currentIndex = Math.min(resultsList.count - 1, resultsList.currentIndex + 8)
                event.accepted = true
            } else if (event.key === Qt.Key_PageUp) {
                resultsList.currentIndex = Math.max(0, resultsList.currentIndex - 8)
                event.accepted = true
            } else if (event.key === Qt.Key_Escape) {
                root.close()
                event.accepted = true
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                root.activateCurrent()
                event.accepted = true
            }
        }
    }

    Item {
        Layout.fillWidth: true
        implicitHeight: fuzzyFinder.count === 0
            ? 72
            : Math.min(440, fuzzyFinder.count * 46 + 4)

        Text {
            anchors.centerIn: parent
            visible: fuzzyFinder.count === 0
            text: fuzzyFinder.scanning
                ? "Scanning..."
                : (searchField.text === "" ? "No files here" : "No matches")
            color: Theme.subtext
            font.pointSize: Theme.fontNormal
        }

        ListView {
            id: resultsList
            anchors.fill: parent
            visible: fuzzyFinder.count > 0
            clip: true
            model: fuzzyFinder
            currentIndex: 0
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 80
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            // Keep the keyboard-selected row scrolled into view.
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: Rectangle {
                id: resultRow
                required property int index
                required property string name
                required property string dir
                required property string path
                required property bool isDir

                width: ListView.view.width
                height: 46
                color: resultRow.ListView.isCurrentItem
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16)
                    : (rowHover.hovered ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.05) : "transparent")
                radius: Theme.radiusSmall

                HoverHandler { id: rowHover }
                TapHandler {
                    onTapped: {
                        resultsList.currentIndex = resultRow.index
                        root.activateCurrent()
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: resultRow.isDir ? resultRow.name + "/" : resultRow.name
                        color: Theme.text
                        font.pointSize: Theme.fontNormal
                        font.weight: Font.DemiBold
                        elide: Text.ElideMiddle
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: resultRow.dir !== ""
                        text: resultRow.dir
                        color: Theme.subtext
                        font.pointSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }
}
