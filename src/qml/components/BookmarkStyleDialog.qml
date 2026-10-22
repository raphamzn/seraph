import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Seraph
import Quill as Q

// "Customize…" for a pinned folder: label, theme colour and built-in icon,
// with a live preview card and a searchable icon grid. Nothing is written
// until Save; the host applies the result through bookmarks.setStyle().
Item {
    id: dialog
    visible: false
    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Customize pinned folder")

    signal applied(int index, string icon, string color, string label)

    property int targetIndex: -1
    property string folderName: ""
    property string folderPath: ""
    property string initialIcon: ""        // style icon as stored ("" = default)
    property string initialEffectiveIcon: "Folder"
    property string selectedIcon: "Folder"
    property string selectedColor: ""
    readonly property var colorRoles: ["", "accent", "success", "warning", "error", "subtext", "muted"]

    function roleColor(role) {
        switch (role) {
        case "accent": return Theme.accent
        case "success": return Theme.success
        case "warning": return Theme.warning
        case "error": return Theme.error
        case "subtext": return Theme.subtext
        case "muted": return Theme.muted
        }
        return "transparent"
    }

    function open(index) {
        var style = bookmarks.styleFor(index)
        if (!style || style.key === undefined)
            return
        targetIndex = index
        folderName = style.folderName || ""
        folderPath = style.path || ""
        initialIcon = style.icon || ""
        initialEffectiveIcon = bookmarks.data(bookmarks.index(index, 0), 261 /* QuillIconRole */) || "Folder"
        selectedIcon = initialEffectiveIcon
        selectedColor = style.color || ""
        labelField.text = style.label || ""
        searchField.text = ""
        var current = IconCatalog.names.indexOf(selectedIcon)
        if (current >= 0)
            iconGrid.positionViewAtIndex(current, GridView.Contain)
        else
            iconGrid.positionViewAtBeginning()

        visible = true
        box.opacity = 0
        box.scale = 0.88
        box.yOffset = -8
        openAnim.start()
        Qt.callLater(function() {
            labelField.inputItem.forceActiveFocus()
            labelField.inputItem.selectAll()
        })
    }

    function accept() {
        if (targetIndex < 0)
            return
        // Keep "no icon" when the user left the default alone, so the folder
        // keeps following its XDG default instead of pinning today's icon.
        var icon = (initialIcon === "" && selectedIcon === initialEffectiveIcon) ? "" : selectedIcon
        applied(targetIndex, icon, selectedColor, labelField.text.trim())
        closeAnim.start()
    }

    function reject() { closeAnim.start() }

    ParallelAnimation {
        id: openAnim
        NumberAnimation { target: box; property: "opacity"; from: 0; to: 1; duration: Theme.animDurationFast; easing.type: Theme.animEasingEnter; easing.bezierCurve: Theme.animBezierCurve }
        NumberAnimation { target: box; property: "scale"; from: 0.88; to: 1; duration: Theme.animDurationSlow; easing.type: Easing.OutBack; easing.overshoot: 0.8 }
        NumberAnimation { target: box; property: "yOffset"; from: -8; to: 0; duration: Theme.animDuration; easing.type: Theme.animEasingEnter; easing.bezierCurve: Theme.animBezierCurve }
    }
    SequentialAnimation {
        id: closeAnim
        ParallelAnimation {
            NumberAnimation { target: box; property: "opacity"; to: 0; duration: Theme.animDurationFast; easing.type: Theme.animEasingExit; easing.bezierCurve: Theme.animBezierCurve }
            NumberAnimation { target: box; property: "scale"; to: 0.92; duration: Theme.animDurationFast; easing.type: Theme.animEasingExit; easing.bezierCurve: Theme.animBezierCurve }
            NumberAnimation { target: box; property: "yOffset"; to: -4; duration: Theme.animDurationFast; easing.type: Theme.animEasingExit; easing.bezierCurve: Theme.animBezierCurve }
        }
        ScriptAction { script: { dialog.visible = false; dialog.targetIndex = -1 } }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: dialog.reject()
    }

    Item {
        id: box
        width: Math.min(500, dialog.width - 2 * Theme.spacing)
        height: card.implicitHeight
        anchors.centerIn: parent
        opacity: 0
        scale: 0.88
        transformOrigin: Item.Center
        property real yOffset: 0
        transform: Translate { y: box.yOffset }

        Keys.onEscapePressed: dialog.reject()

        Q.Card {
            id: card
            anchors.fill: parent
            title: qsTr("Customize pinned folder")
            padding: 20
            color: Theme.mantle
            border.color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.1)

            // The path can be long: elide it instead of letting it widen the card.
            Text {
                Layout.fillWidth: true
                Layout.topMargin: -Theme.spacing
                text: dialog.folderPath
                color: Theme.muted
                font.pointSize: Theme.fontSmall
                elide: Text.ElideMiddle
                maximumLineCount: 1
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 18

                // Left: label, colour, preview
                ColumnLayout {
                    Layout.preferredWidth: 200
                    Layout.maximumWidth: 200
                    Layout.fillHeight: false
                    Layout.alignment: Qt.AlignTop
                    spacing: 6

                    Text { text: qsTr("Label"); color: Theme.muted; font.pointSize: Theme.fontSmall - 1; font.bold: true; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                    Q.TextField {
                        id: labelField
                        Layout.fillWidth: true
                        variant: "filled"
                        placeholder: dialog.folderName
                        Keys.onReturnPressed: dialog.accept()
                        Keys.onEscapePressed: dialog.reject()
                    }

                    Text { Layout.topMargin: 8; text: qsTr("Color"); color: Theme.muted; font.pointSize: Theme.fontSmall - 1; font.bold: true; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: dialog.colorRoles

                            Rectangle {
                                id: swatch
                                required property string modelData
                                readonly property bool selected: dialog.selectedColor === modelData
                                width: 20; height: 20
                                radius: 10
                                color: modelData === "" ? "transparent" : dialog.roleColor(modelData)
                                border.width: modelData === "" ? 2 : (selected ? 2 : 0)
                                border.color: modelData === ""
                                    ? (selected ? Theme.text : Theme.overlay)
                                    : Theme.text
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: modelData === "" ? qsTr("No color") : modelData

                                // "none" swatch: a thin diagonal stroke
                                Rectangle {
                                    visible: swatch.modelData === ""
                                    anchors.centerIn: parent
                                    width: 12; height: 2
                                    rotation: -45
                                    color: Theme.overlay
                                }
                                Rectangle {
                                    visible: swatch.selected && swatch.modelData !== ""
                                    anchors.centerIn: parent
                                    width: 8; height: 8; radius: 4
                                    color: Theme.mantle
                                }
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: dialog.selectedColor = swatch.modelData }
                            }
                        }
                    }

                    Text { Layout.topMargin: 8; text: qsTr("Preview"); color: Theme.muted; font.pointSize: Theme.fontSmall - 1; font.bold: true; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                    SidebarPinnedCard {
                        Layout.fillWidth: true
                        label: labelField.text.trim() !== "" ? labelField.text.trim() : dialog.folderName
                        quillIcon: dialog.selectedIcon
                        colorRole: dialog.selectedColor
                        secondary: qsTr("%n item(s)", "", 12) + " · " + qsTr("today")
                        showSecondary: config.sidebarItemCounts
                    }
                }

                // Right: searchable icon grid
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: false
                    Layout.minimumWidth: 220
                    Layout.alignment: Qt.AlignTop
                    spacing: 6

                    Text { text: qsTr("Icon"); color: Theme.muted; font.pointSize: Theme.fontSmall - 1; font.bold: true; font.capitalization: Font.AllUppercase; font.letterSpacing: 1 }
                    Q.TextField {
                        id: searchField
                        Layout.fillWidth: true
                        variant: "filled"
                        placeholder: qsTr("Search icons…")
                        Keys.onEscapePressed: dialog.reject()
                        Keys.onReturnPressed: dialog.accept()
                    }

                    GridView {
                        id: iconGrid
                        Layout.fillWidth: true
                        Layout.fillHeight: false
                        Layout.preferredHeight: 6 * cellHeight
                        Layout.minimumHeight: 6 * cellHeight
                        Layout.maximumHeight: 6 * cellHeight
                        clip: true
                        // The icon Shapes (curve renderer) ignore the scissor clip and
                        // leak out of the viewport; rendering the grid as a layer bounds them.
                        layer.enabled: true
                        cacheBuffer: cellHeight
                        cellWidth: Math.floor(width / 6)
                        cellHeight: 36
                        boundsBehavior: Flickable.StopAtBounds
                        model: IconCatalog.matching(searchField.text)
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        delegate: Item {
                            id: cell
                            required property string modelData
                            readonly property bool selected: dialog.selectedIcon === modelData
                            width: iconGrid.cellWidth
                            height: iconGrid.cellHeight

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 2
                                radius: Theme.radiusSmall
                                color: cell.selected
                                    ? Qt.rgba(dialog.roleColor(dialog.selectedColor || "accent").r,
                                              dialog.roleColor(dialog.selectedColor || "accent").g,
                                              dialog.roleColor(dialog.selectedColor || "accent").b, 0.18)
                                    : (cellHover.hovered
                                        ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
                                        : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.045))
                                border.width: cell.selected ? 1 : 0
                                border.color: dialog.roleColor(dialog.selectedColor || "accent")

                                Loader {
                                    anchors.centerIn: parent
                                    source: "../icons/Icon" + cell.modelData + ".qml"
                                    onLoaded: {
                                        item.size = 16
                                        item.color = Qt.binding(function() {
                                            return cell.selected
                                                ? dialog.roleColor(dialog.selectedColor || "accent")
                                                : Theme.subtext
                                        })
                                    }
                                }
                                HoverHandler { id: cellHover; cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: dialog.selectedIcon = cell.modelData }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Layout.topMargin: Theme.spacing
                spacing: 12

                Q.Button {
                    id: cancelBtn
                    text: qsTr("Cancel")
                    variant: "ghost"
                    size: "small"
                    Keys.onEscapePressed: dialog.reject()
                    onClicked: dialog.reject()
                }
                Q.Button {
                    text: qsTr("Save")
                    variant: "primary"
                    size: "small"
                    Keys.onEscapePressed: dialog.reject()
                    onClicked: dialog.accept()
                }
            }
        }
    }
}
