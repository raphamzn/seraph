import QtQuick
import Seraph

// Ctrl+F bar of the markdown reader: Enter / Shift+Enter step through the
// hits, Esc (handled by the window) closes it.
Rectangle {
    id: root

    property int count: 0
    property int index: -1

    signal searchRequested(string query)
    signal nextRequested()
    signal previousRequested()
    signal closeRequested()

    function open() {
        visible = true
        input.forceActiveFocus()
        input.selectAll()
    }

    height: visible ? Math.round(44 * Theme.uiScale) : 0
    color: Theme.mantle

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
    }

    Row {
        anchors.fill: parent
        anchors.leftMargin: Math.round(16 * Theme.uiScale)
        anchors.rightMargin: Math.round(12 * Theme.uiScale)
        spacing: Math.round(10 * Theme.uiScale)

        IconSearch {
            anchors.verticalCenter: parent.verticalCenter
            size: Math.round(15 * Theme.uiScale)
            color: Theme.subtext
        }

        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - counter.width - controls.width - 2 * parent.spacing
            height: input.implicitHeight

            TextInput {
                id: input
                anchors.left: parent.left
                anchors.right: parent.right
                color: Theme.text
                selectionColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                font.pointSize: Theme.fontNormal
                clip: true
                onTextChanged: root.searchRequested(text)
                Keys.onReturnPressed: (event) => event.modifiers & Qt.ShiftModifier
                    ? root.previousRequested() : root.nextRequested()
                Keys.onEnterPressed: (event) => event.modifiers & Qt.ShiftModifier
                    ? root.previousRequested() : root.nextRequested()
            }

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                visible: input.text === ""
                text: qsTr("Find in document")
                color: Theme.muted
                font.pointSize: Theme.fontNormal
            }
        }

        Text {
            id: counter
            anchors.verticalCenter: parent.verticalCenter
            text: input.text === "" ? ""
                : (root.count === 0 ? qsTr("No results") : qsTr("%1 of %2").arg(root.index + 1).arg(root.count))
            color: root.count === 0 && input.text !== "" ? Theme.warning : Theme.subtext
            font.pointSize: Theme.fontSmall
        }

        Row {
            id: controls
            anchors.verticalCenter: parent.verticalCenter
            spacing: Math.round(4 * Theme.uiScale)

            Repeater {
                model: [
                    { icon: "up", action: "previous" },
                    { icon: "down", action: "next" },
                    { icon: "close", action: "close" }
                ]

                Rectangle {
                    id: button
                    required property var modelData
                    width: Math.round(26 * Theme.uiScale)
                    height: width
                    radius: Theme.radiusSmall
                    color: buttonHover.hovered ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08) : "transparent"

                    IconChevronUp {
                        anchors.centerIn: parent
                        visible: button.modelData.icon === "up"
                        size: Math.round(14 * Theme.uiScale)
                        color: Theme.subtext
                    }
                    IconChevronDown {
                        anchors.centerIn: parent
                        visible: button.modelData.icon === "down"
                        size: Math.round(14 * Theme.uiScale)
                        color: Theme.subtext
                    }
                    IconX {
                        anchors.centerIn: parent
                        visible: button.modelData.icon === "close"
                        size: Math.round(14 * Theme.uiScale)
                        color: Theme.subtext
                    }

                    HoverHandler { id: buttonHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            if (button.modelData.action === "previous") root.previousRequested()
                            else if (button.modelData.action === "next") root.nextRequested()
                            else root.closeRequested()
                        }
                    }
                }
            }
        }
    }
}
