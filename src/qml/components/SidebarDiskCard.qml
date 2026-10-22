import QtQuick
import QtQuick.Layouts
import Seraph
import Quill as Quill

// One disk or removable device in the sidebar: name and mount point, a
// thick usage bar labelled with the used space, and a footer with the free
// space, filesystem and bus. Removable media get a USB badge and an eject
// button; an unmounted device keeps a short card that says what a click does.
Item {
    id: card

    property string name: ""
    property string mountPoint: ""
    property string fsType: ""
    property string busLabel: ""
    property real totalSize: 0
    property real freeSpace: 0
    property real usagePercent: 0
    property bool removable: false
    property bool mounted: false
    property bool hovered: false
    signal ejectClicked()

    readonly property color levelColor: usagePercent >= 90
        ? Theme.error
        : usagePercent >= 75 ? Theme.warning : Theme.accent
    readonly property real usedBytes: Math.max(0, totalSize - freeSpace)
    // gvfs mounts (MTP phones, network shares) report no sizes at all: no
    // bar and no "free" figure for them, just the filesystem and bus.
    readonly property bool hasSizes: mounted && totalSize > 0
    readonly property string footerText: mounted
        ? [hasSizes ? qsTr("%1 free").arg(Quill.Format.bytes(freeSpace)) : "", fsType, busLabel]
              .filter(function(s) { return s !== "" }).join(" · ")
        : qsTr("Click to mount")

    implicitHeight: hasSizes ? 58 : 40
    height: implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: card.hovered
            ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
            : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.045)
        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        anchors.verticalCenter: parent.verticalCenter
        spacing: 5

        // Name · badge · mount point · eject
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Loader {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                sourceComponent: card.removable ? usbIcon : (card.mounted ? driveIcon : driveOffIcon)
            }

            Text {
                Layout.fillWidth: true
                text: card.name
                color: card.mounted ? Theme.text : Theme.muted
                font.pointSize: Theme.fontNormal
                elide: Text.ElideRight
            }

            Rectangle {
                visible: card.removable
                Layout.preferredWidth: usbBadgeLabel.implicitWidth + 8
                Layout.preferredHeight: usbBadgeLabel.implicitHeight + 2
                radius: 3
                color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)

                Text {
                    id: usbBadgeLabel
                    anchors.centerIn: parent
                    text: "USB"
                    color: Theme.subtext
                    font.pointSize: Math.max(6, Theme.fontSmall - 2)
                    font.letterSpacing: 0.6
                }
            }

            Text {
                visible: card.mounted && card.mountPoint !== "" && card.mountPoint !== card.name
                Layout.maximumWidth: Math.round(card.width * 0.42)
                text: card.mountPoint
                color: Theme.muted
                font.pointSize: Theme.fontSmall
                elide: Text.ElideMiddle
            }

            Rectangle {
                id: ejectButton
                visible: card.removable && card.mounted
                Layout.preferredWidth: 18
                Layout.preferredHeight: 18
                radius: 9
                color: ejectHover.hovered
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
                    : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                Text {
                    anchors.centerIn: parent
                    text: "⏏"
                    color: ejectHover.hovered ? Theme.accent : Theme.muted
                    font.pointSize: Theme.fontNormal
                }
                HoverHandler { id: ejectHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: card.ejectClicked() }
            }
        }

        // Usage bar with the used space written inside it
        Rectangle {
            visible: card.hasSizes
            Layout.fillWidth: true
            Layout.preferredHeight: 12
            radius: 6
            color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)

            Rectangle {
                id: usageFill
                width: Math.round(parent.width * Math.min(Math.max(card.usagePercent, 0) / 100.0, 1.0))
                height: parent.height
                radius: parent.radius
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.accent }
                    GradientStop { position: 1.0; color: card.levelColor }
                }
                Behavior on width { NumberAnimation { duration: Theme.animDuration } }
            }

            Text {
                id: usedLabel
                x: 7
                anchors.verticalCenter: parent.verticalCenter
                visible: usageFill.width >= implicitWidth + 14
                text: qsTr("%1 used").arg(Quill.Format.bytes(card.usedBytes))
                color: Theme.crust
                font.pointSize: Math.max(7, Theme.fontSmall - 2)
                font.bold: true
            }
        }

        Text {
            Layout.fillWidth: true
            text: card.footerText
            color: Theme.muted
            font.pointSize: Theme.fontSmall
            elide: Text.ElideRight
        }
    }

    Component { id: usbIcon; IconUsb { size: 16; color: card.mounted ? Theme.subtext : Theme.muted } }
    Component { id: driveIcon; IconHardDrive { size: 16; color: Theme.subtext } }
    Component { id: driveOffIcon; IconHardDriveOff { size: 16; color: Theme.muted } }
}
