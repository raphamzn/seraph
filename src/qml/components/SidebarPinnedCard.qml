import QtQuick
import Seraph

// One pinned folder in the sidebar: a square tinted with the folder's colour
// holding its icon, the label, and a second line with the item count and the
// last modification (or "scheme · host" for a remote bookmark). Every card in
// the section shares one height so the drag-to-reorder index math stays
// simple: 44 px with the second line, 32 px without.
Item {
    id: card

    property string label: ""
    property string quillIcon: "Folder"
    property string colorRole: ""
    property string secondary: ""
    property bool active: false
    property bool hovered: false
    property bool showSecondary: true
    property bool showBackground: true

    readonly property int cardHeight: showSecondary ? 44 : 32
    readonly property bool tinted: colorRole !== ""
    readonly property color roleColor: {
        switch (colorRole) {
        case "accent": return Theme.accent
        case "success": return Theme.success
        case "warning": return Theme.warning
        case "error": return Theme.error
        case "subtext": return Theme.subtext
        case "muted": return Theme.muted
        }
        return Theme.subtext
    }
    // An icon name the config may carry but the set does not know falls back
    // to the plain folder instead of an empty square.
    readonly property string iconName: IconCatalog.has(quillIcon) ? quillIcon : "Folder"
    readonly property int badgeSize: showSecondary ? 28 : 22

    implicitHeight: cardHeight
    height: cardHeight

    Rectangle {
        anchors.fill: parent
        visible: card.showBackground
        radius: Theme.radiusSmall
        color: card.active
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
            : card.hovered
                ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
                : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.045)
        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: Theme.spacing

        Rectangle {
            id: badge
            width: card.badgeSize
            height: card.badgeSize
            radius: Theme.radiusSmall
            anchors.verticalCenter: parent.verticalCenter
            color: card.tinted
                ? Qt.rgba(card.roleColor.r, card.roleColor.g, card.roleColor.b, 0.18)
                : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.06)
            Behavior on color { ColorAnimation { duration: Theme.animDuration } }

            Loader {
                anchors.centerIn: parent
                source: "../icons/Icon" + card.iconName + ".qml"
                onLoaded: {
                    item.size = Qt.binding(function() { return card.showSecondary ? 16 : 14 })
                    item.color = Qt.binding(function() {
                        return card.tinted ? card.roleColor : (card.active ? Theme.accent : Theme.subtext)
                    })
                }
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - badge.width - parent.spacing
            spacing: 1

            Text {
                width: parent.width
                text: card.label
                color: card.active ? Theme.accent : Theme.text
                font.pointSize: Theme.fontNormal
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: card.showSecondary && text !== ""
                text: card.secondary
                color: Theme.muted
                font.pointSize: Theme.fontSmall
                elide: Text.ElideRight
            }
        }
    }
}
