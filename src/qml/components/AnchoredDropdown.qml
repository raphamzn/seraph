import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import Quill as Q

// Quill's Dropdown with two fixes, kept here because the PKGBUILD builds
// Quill from its upstream repo, so a fix inside src/qml/Quill would never
// ship. Same API as Q.Dropdown.
//  - The popup is placed when it opens. Quill binds its position to the
//    button's own x/y, which never change when an ancestor moves (the
//    Properties panel sliding in, a tab switch, a scrolled settings page), so
//    the list opened at the stale spot near the window's top-left corner.
//  - It closes when the dropdown stops being visible (Properties closed with
//    the list open left the list floating on screen).

Item {
    id: root
    property var model: []
    property int currentIndex: 0
    property string label: ""
    signal selected(int index, string value)
    implicitHeight: 34
    implicitWidth: 200
    Layout.fillWidth: true
    property bool dropdownOpen: false
    Accessible.role: Accessible.ComboBox
    Accessible.name: root.label !== "" ? root.label : (root.model[root.currentIndex] ?? "")
    Accessible.description: root.model[root.currentIndex] ?? ""

    // The host window the dropdown lives in. Close the popup whenever the
    // host moves, resizes, or loses focus so the popup never detaches from
    // its anchor.
    readonly property var _hostWindow: root.Window.window
    Connections {
        target: root._hostWindow
        ignoreUnknownSignals: true
        function onXChanged() { root.dropdownOpen = false }
        function onYChanged() { root.dropdownOpen = false }
        function onWidthChanged() { root.dropdownOpen = false }
        function onHeightChanged() { root.dropdownOpen = false }
        function onActiveChanged() {
            if (root._hostWindow && !root._hostWindow.active)
                root.dropdownOpen = false
        }
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        height: 34
        spacing: Q.Theme.spacingMd
        Text {
            visible: root.label !== ""
            text: root.label
            color: Q.Theme.textPrimary
            font.pixelSize: Q.Theme.fontSize
            font.family: Q.Theme.fontFamily
            Layout.preferredWidth: 140
        }
        Rectangle {
            id: buttonRect
            Layout.fillWidth: true
            height: 34
            radius: Q.Theme.radius
            color: Q.Theme.surface0
            border.color: root.dropdownOpen ? Q.Theme.primary : Q.Theme.surface1
            border.width: 1
            opacity: root.enabled ? 1.0 : 0.5
            Behavior on border.color { ColorAnimation { duration: Q.Theme.animDurationFast } }
            Row {
                anchors.fill: parent
                anchors.leftMargin: Q.Theme.spacing
                anchors.rightMargin: Q.Theme.spacing
                Text {
                    text: root.model[root.currentIndex] ?? ""
                    color: Q.Theme.textPrimary
                    font.pixelSize: Q.Theme.fontSize
                    font.family: Q.Theme.fontFamily
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - chevron.width
                    elide: Text.ElideRight
                }
                Text {
                    id: chevron
                    text: root.dropdownOpen ? "\uf077" : "\uf078"
                    color: Q.Theme.textTertiary
                    font.family: Q.Theme.iconFont
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    if (!root.enabled) return;
                    root.dropdownOpen = !root.dropdownOpen;
                }
            }
        }
    }

    // The popup is its own native Window. That avoids every class of
    // clipping / culling / z-order problem inside ancestor Flickables or
    // child Qt Windows (settings dialogs, frameless tool popups, etc.):
    // the compositor places it on top unconditionally. The earlier approach
    // of reparenting an Item to the window's contentItem was fragile — on
    // some hosts (notably a Qt.Dialog SettingsPanel) the popup children
    // got culled and the list never painted.
    Window {
        id: popupWindow
        flags: Qt.Popup | Qt.FramelessWindowHint | Qt.NoDropShadowWindowHint
        color: "transparent"
        transientParent: root._hostWindow ? root._hostWindow : null
        visible: false

        width: buttonRect.width
        height: Math.min(root.model.length * 34 + 8, 208)


        Rectangle {
            id: dropdownList
            anchors.fill: parent
            radius: Q.Theme.radius
            color: Q.Theme.surface0
            border.color: Q.Theme.surface1
            border.width: 1
            clip: true

            ListView {
                id: listView
                anchors.fill: parent
                anchors.margins: 4
                model: root.model
                currentIndex: root.currentIndex
                boundsBehavior: Flickable.StopAtBounds
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    width: listView.width
                    height: 30
                    radius: Q.Theme.radiusSm
                    color: index === root.currentIndex
                        ? Qt.rgba(Q.Theme.primary.r, Q.Theme.primary.g, Q.Theme.primary.b, 0.15)
                        : itemMouse.containsMouse ? Q.Theme.surface1 : "transparent"
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: Q.Theme.spacing
                        text: modelData
                        color: index === root.currentIndex ? Q.Theme.primary : Q.Theme.textPrimary
                        font.pixelSize: Q.Theme.fontSize
                        font.family: Q.Theme.fontFamily
                    }
                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.currentIndex = index;
                            root.selected(index, modelData);
                            root.dropdownOpen = false;
                        }
                    }
                }
            }
        }

        // Qt.Popup normally auto-closes on outside click, but be explicit
        // so losing focus (Escape, Alt-Tab) also closes the popup.
        onActiveChanged: {
            if (!active)
                root.dropdownOpen = false
        }
    }

    // Global coordinates of the spot just below the button, read when the
    // popup opens so wherever the panel ended up is where the list appears.
    function placePopup() {
        var hx = root._hostWindow ? root._hostWindow.x : 0
        var hy = root._hostWindow ? root._hostWindow.y : 0
        var local = buttonRect.mapToItem(null, 0, buttonRect.height + 4)
        popupWindow.x = hx + local.x
        popupWindow.y = hy + local.y
    }

    // Shown from here, not by a binding, so the position is set before the
    // popup maps.
    onDropdownOpenChanged: {
        if (dropdownOpen) {
            placePopup()
            popupWindow.visible = true
            forceActiveFocus()
        } else {
            popupWindow.visible = false
        }
    }
    onVisibleChanged: { if (!visible) dropdownOpen = false }
    Keys.onEscapePressed: dropdownOpen = false
    Keys.onUpPressed: { if (dropdownOpen && currentIndex > 0) currentIndex--; }
    Keys.onDownPressed: { if (dropdownOpen && currentIndex < model.length - 1) currentIndex++; }
    Keys.onReturnPressed: {
        if (dropdownOpen) {
            selected(currentIndex, model[currentIndex]);
            dropdownOpen = false;
        }
    }
}
