import QtQuick
import QtQuick.Layouts
import Seraph

Rectangle {
    id: root
    Accessible.role: Accessible.Grouping
    Accessible.name: "Search filters"

    property var activeTypes: []  // array of active type values
    property string activeDateFilter: ""
    property string activeSizeFilter: ""

    signal typeFilterChanged(string filter)
    signal dateFilterChanged(string filter)
    signal sizeFilterChanged(string filter)
    signal clearAllFilters()

    // The size filter is "<op><number><unit>" (e.g. ">50MB"), built from the
    // three controls. They keep their values while the filter is off, so it
    // comes back as the user left it; "" means off.
    property string sizeOp: "<"
    property string sizeNumber: "1"
    property string sizeUnit: "MB"
    readonly property bool sizeActive: activeSizeFilter !== ""

    function applyState(typeFilter, dateFilter, sizeFilter) {
        activeTypes = typeFilter ? typeFilter.split(",").filter(function(v) { return v !== "" }) : []
        activeDateFilter = dateFilter || ""
        activeSizeFilter = sizeFilter || ""
        var m = activeSizeFilter.match(/^([<>=])([\d.,]+)(KB|MB|GB)$/)
        // Leave the field alone when it already says this, so a sync
        // triggered by typing "1," does not rewrite it under the cursor
        if (m && activeSizeFilter !== sizeFilterString()) {
            sizeOp = m[1]
            sizeNumber = m[2]
            sizeUnit = m[3]
        }
    }

    function sizeFilterString() {
        var number = sizeNumber.replace(",", ".")
        if (!/^\d+(\.\d+)?$/.test(number))
            number = number.replace(/\.$/, "")
        return /^\d+(\.\d+)?$/.test(number) ? sizeOp + number + sizeUnit : ""
    }

    function updateSizeFilter() {
        activeSizeFilter = sizeFilterString()
        sizeFilterChanged(activeSizeFilter)
    }

    function isTypeActive(value) {
        return activeTypes.indexOf(value) >= 0
    }

    function toggleType(value) {
        var types = activeTypes.slice()
        var idx = types.indexOf(value)
        if (idx >= 0)
            types.splice(idx, 1)
        else
            types.push(value)
        activeTypes = types
        typeFilterChanged(types.join(","))
    }

    color: Theme.mantle
    implicitHeight: content.implicitHeight + 16

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            spacing: 16

            // -- Type filter --
            ColumnLayout {
                spacing: 4

                Text {
                    text: "Type"
                    font.pointSize: Theme.fontSmall
                    color: Theme.muted
                    font.weight: Font.Medium
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: [
                            { label: "Folders", value: "folders" },
                            { label: "Documents", value: "documents" },
                            { label: "Images", value: "images" },
                            { label: "Audio", value: "audio" },
                            { label: "Video", value: "video" },
                            { label: "Code", value: "code" },
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            width: chipText.implicitWidth + 16
                            height: 26
                            radius: 13
                            color: root.isTypeActive(modelData.value)
                                ? Theme.accent
                                : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: modelData.label
                                font.pointSize: Theme.fontSmall
                                color: root.isTypeActive(modelData.value) ? Theme.base : Theme.subtext
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.toggleType(modelData.value)
                            }
                        }
                    }
                }
            }

            // -- Separator --
            Rectangle { width: 1; Layout.fillHeight: true; color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.1) }

            // -- Date filter --
            ColumnLayout {
                spacing: 4

                Text {
                    text: "Modified"
                    font.pointSize: Theme.fontSmall
                    color: Theme.muted
                    font.weight: Font.Medium
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 4

                    Repeater {
                        model: [
                            { label: "Today", value: "today" },
                            { label: "This week", value: "week" },
                            { label: "This month", value: "month" },
                            { label: "This year", value: "year" },
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            width: dateText.implicitWidth + 16
                            height: 26
                            radius: 13
                            color: root.activeDateFilter === modelData.value
                                ? Theme.accent
                                : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                id: dateText
                                anchors.centerIn: parent
                                text: modelData.label
                                font.pointSize: Theme.fontSmall
                                color: root.activeDateFilter === modelData.value ? Theme.base : Theme.subtext
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.activeDateFilter === modelData.value) {
                                        root.activeDateFilter = ""
                                        root.dateFilterChanged("")
                                    } else {
                                        root.activeDateFilter = modelData.value
                                        root.dateFilterChanged(modelData.value)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // -- Separator --
            Rectangle { width: 1; Layout.fillHeight: true; color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.1) }

            // -- Size filter --
            ColumnLayout {
                spacing: 4

                Text {
                    text: "Size"
                    font.pointSize: Theme.fontSmall
                    color: Theme.muted
                    font.weight: Font.Medium
                }

                RowLayout {
                    spacing: 4

                    Repeater {
                        model: [
                            { label: "<", value: "<", name: "Smaller than" },
                            { label: "=", value: "=", name: "About" },
                            { label: ">", value: ">", name: "Larger than" },
                        ]

                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool chosen: root.sizeActive && root.sizeOp === modelData.value
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name
                            width: 26
                            height: 26
                            radius: 13
                            color: chosen
                                ? Theme.accent
                                : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                font.pointSize: Theme.fontSmall
                                color: parent.chosen ? Theme.base : Theme.subtext
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    // Clicking the operator in use turns the filter off
                                    if (parent.chosen) {
                                        root.activeSizeFilter = ""
                                        root.sizeFilterChanged("")
                                        return
                                    }
                                    root.sizeOp = modelData.value
                                    if (root.sizeFilterString() === "")
                                        root.sizeNumber = "1"
                                    root.updateSizeFilter()
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 26
                        Layout.leftMargin: 4
                        Layout.rightMargin: 4
                        radius: 13
                        color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)
                        border.width: 1
                        border.color: sizeInput.activeFocus
                            ? Theme.accent
                            : root.sizeActive ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6) : "transparent"

                        TextInput {
                            id: sizeInput
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            verticalAlignment: TextInput.AlignVCenter
                            horizontalAlignment: TextInput.AlignHCenter
                            clip: true
                            selectByMouse: true
                            text: root.sizeNumber
                            font.pointSize: Theme.fontSmall
                            color: root.sizeActive ? Theme.text : Theme.subtext
                            selectionColor: Theme.accent
                            selectedTextColor: Theme.base
                            validator: RegularExpressionValidator { regularExpression: /^\d{0,7}([.,]\d{0,3})?$/ }
                            Accessible.role: Accessible.EditableText
                            Accessible.name: "Size"
                            onTextEdited: {
                                root.sizeNumber = text
                                root.updateSizeFilter()
                            }
                            Keys.onEscapePressed: focus = false
                        }
                    }

                    Repeater {
                        model: ["KB", "MB", "GB"]

                        delegate: Rectangle {
                            required property string modelData
                            readonly property bool chosen: root.sizeActive && root.sizeUnit === modelData
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData
                            width: unitText.implicitWidth + 16
                            height: 26
                            radius: 13
                            color: chosen
                                ? Theme.accent
                                : Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)

                            Behavior on color { ColorAnimation { duration: 150 } }

                            Text {
                                id: unitText
                                anchors.centerIn: parent
                                text: modelData
                                font.pointSize: Theme.fontSmall
                                color: parent.chosen ? Theme.base : Theme.subtext
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.sizeUnit = modelData
                                    if (root.sizeFilterString() === "")
                                        root.sizeNumber = "1"
                                    root.updateSizeFilter()
                                }
                            }
                        }
                    }
                }
            }

            // -- Spacer + Clear --
            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: clearText.implicitWidth + 16
                Layout.preferredHeight: 26
                Layout.alignment: Qt.AlignBottom
                radius: 13
                color: clearHover.hovered
                    ? Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)
                    : "transparent"
                visible: root.activeTypes.length > 0 || root.activeDateFilter !== "" || root.activeSizeFilter !== ""

                Text {
                    id: clearText
                    anchors.centerIn: parent
                    text: "Clear all"
                    font.pointSize: Theme.fontSmall
                    color: Theme.error
                }

                HoverHandler { id: clearHover; cursorShape: Qt.PointingHandCursor }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.activeTypes = []
                        root.activeDateFilter = ""
                        root.activeSizeFilter = ""
                        root.clearAllFilters()
                    }
                }
            }
        }
    }
}
