import QtQuick
import Seraph

// Small-caps section title for the sidebar ("Pinned", "Disks"), with an
// optional "+" action on the right.
Item {
    id: header

    property string title: ""
    property bool showAdd: false
    property bool addEnabled: true
    property string addToolTip: ""
    signal addClicked()

    implicitHeight: Math.round(22 * Theme.uiScale)
    height: implicitHeight

    Text {
        anchors.left: parent.left
        anchors.leftMargin: Math.round(Theme.spacing * 1.5)
        anchors.right: addButton.visible ? addButton.left : parent.right
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        text: header.title
        color: Theme.muted
        font.pointSize: Math.max(7, Theme.fontSmall - 1)
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 1.2
        elide: Text.ElideRight
    }

    Rectangle {
        id: addButton
        visible: header.showAdd
        width: 18; height: 18
        radius: 9
        anchors.right: parent.right
        anchors.rightMargin: Math.round(Theme.spacing * 1.5)
        anchors.verticalCenter: parent.verticalCenter
        color: addHover.hovered && header.addEnabled
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
            : "transparent"
        opacity: header.addEnabled ? 1 : 0.35
        Behavior on color { ColorAnimation { duration: Theme.animDuration } }

        IconPlus {
            anchors.centerIn: parent
            size: 12
            color: addHover.hovered && header.addEnabled ? Theme.accent : Theme.muted
        }

        HoverHandler {
            id: addHover
            enabled: header.addEnabled
            cursorShape: Qt.PointingHandCursor
        }
        TapHandler {
            enabled: header.addEnabled
            onTapped: header.addClicked()
        }
    }
}
