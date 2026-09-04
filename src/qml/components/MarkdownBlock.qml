import QtQuick
import QtQuick.Controls
import Seraph

// One rendered markdown block (see MarkdownRenderer for the block shapes).
// Chosen by `block.type`; every delegate fills the reading column width.
Item {
    id: root

    property var block: ({})
    property real columnWidth: 600
    property color inlineCodeBackground: "transparent"

    signal linkActivated(string link)
    signal copyRequested(string text)

    readonly property string blockType: block && block.type ? block.type : ""
    readonly property real proseLineHeight: 1.45
    readonly property color dividerColor: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
    readonly property color cardColor: Qt.rgba(Theme.crust.r, Theme.crust.g, Theme.crust.b, 0.55)
    readonly property color cardBorder: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)
    readonly property color pillColor: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.07)

    // Extra breathing room above headings so sections read as sections.
    readonly property real topInset: {
        if (blockType !== "heading")
            return 0
        var level = block.level || 1
        return level <= 2 ? Math.round(18 * Theme.uiScale) : Math.round(10 * Theme.uiScale)
    }

    width: columnWidth
    height: loader.height + topInset

    function headingPointSize(level) {
        var base = Theme.fontNormal
        switch (level) {
        case 1: return Math.round(base * 1.9)
        case 2: return Math.round(base * 1.5)
        case 3: return Math.round(base * 1.25)
        case 4: return Math.round(base * 1.1)
        default: return base
        }
    }

    Loader {
        id: loader
        y: root.topInset
        width: root.columnWidth
        sourceComponent: {
            switch (root.blockType) {
            case "heading": return headingComponent
            case "code": return codeComponent
            case "quote": return quoteComponent
            case "list": return listComponent
            case "table": return tableComponent
            case "image": return imageComponent
            case "rule": return ruleComponent
            default: return paragraphComponent
            }
        }
    }

    // Prose runs share this: wrapped rich text with themed links.
    component ProseText: Text {
        property string html: ""
        width: parent ? parent.width : root.columnWidth
        text: html
        textFormat: Text.RichText
        wrapMode: Text.Wrap
        color: Theme.text
        linkColor: Theme.accent
        font.pointSize: Theme.fontNormal
        lineHeight: root.proseLineHeight
        onLinkActivated: (link) => root.linkActivated(link)

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: parent.hoveredLink !== "" ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    Component {
        id: headingComponent

        Column {
            width: root.columnWidth
            spacing: Math.round(6 * Theme.uiScale)

            Text {
                width: parent.width
                text: root.block.html || ""
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                color: (root.block.level || 1) >= 5 ? Theme.subtext : Theme.text
                linkColor: Theme.accent
                font.pointSize: root.headingPointSize(root.block.level || 1)
                font.weight: (root.block.level || 1) <= 4 ? Font.Bold : Font.DemiBold
                font.letterSpacing: (root.block.level || 1) === 1 ? -0.4 : 0
                lineHeight: 1.2
                onLinkActivated: (link) => root.linkActivated(link)
            }

            // H1/H2 get a hairline, the way GitHub separates major sections.
            Rectangle {
                width: parent.width
                height: 1
                visible: (root.block.level || 1) <= 2
                color: root.dividerColor
            }
        }
    }

    Component {
        id: paragraphComponent

        ProseText {
            html: root.block.html || ""
            horizontalAlignment: root.block.align === "center"
                ? Text.AlignHCenter
                : (root.block.align === "right" ? Text.AlignRight : Text.AlignLeft)
        }
    }

    Component {
        id: codeComponent

        Rectangle {
            id: codeCard
            width: root.columnWidth
            height: codeHeader.height + codeFlick.height
            radius: Theme.radiusMedium
            color: root.cardColor
            border.width: 1
            border.color: root.cardBorder
            clip: true

            readonly property string language: root.block.language || ""
            readonly property bool highlighted: (root.block.html || "") !== ""
            property bool copied: false

            Item {
                id: codeHeader
                width: parent.width
                height: Math.round(30 * Theme.uiScale)

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.035)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: root.dividerColor
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: codeCard.language !== "" ? codeCard.language.toUpperCase() : "CODE"
                        color: codeCard.language !== "" ? Theme.accent : Theme.muted
                        font.pointSize: Theme.fontSmall - 1
                        font.weight: Font.DemiBold
                        font.letterSpacing: 1.2
                        font.family: "monospace"
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (root.block.lineCount || 0) + (root.block.lineCount === 1 ? " line" : " lines")
                        color: Theme.muted
                        font.pointSize: Theme.fontSmall - 1
                    }
                }

                // Copy pill, mirrors the one on the text preview.
                Rectangle {
                    id: copyChip
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: copyRow.implicitWidth + 16
                    height: Math.round(20 * Theme.uiScale)
                    radius: height / 2
                    color: copyHover.hovered
                        ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.14)
                        : root.pillColor
                    Accessible.role: Accessible.Button
                    Accessible.name: "Copy code"

                    Row {
                        id: copyRow
                        anchors.centerIn: parent
                        spacing: 5

                        IconCopy {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !codeCard.copied
                            size: Math.round(11 * Theme.uiScale)
                            color: Theme.subtext
                        }

                        IconCheck {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: codeCard.copied
                            size: Math.round(11 * Theme.uiScale)
                            color: Theme.accent
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: codeCard.copied ? "Copied" : "Copy"
                            color: codeCard.copied ? Theme.accent : Theme.subtext
                            font.pointSize: Theme.fontSmall - 1
                        }
                    }

                    Timer { id: copiedTimer; interval: 1200; onTriggered: codeCard.copied = false }
                    HoverHandler { id: copyHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: {
                            root.copyRequested(root.block.code || "")
                            codeCard.copied = true
                            copiedTimer.restart()
                        }
                    }
                }
            }

            // Wide code scrolls sideways inside its card; the page keeps
            // scrolling vertically through the outer wheel handler.
            Flickable {
                id: codeFlick
                anchors.top: codeHeader.bottom
                width: parent.width
                height: codeText.implicitHeight + 24
                contentWidth: codeText.implicitWidth + 32
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                interactive: contentWidth > width

                TextEdit {
                    id: codeText
                    x: 16
                    y: 12
                    readOnly: true
                    selectByMouse: true
                    textFormat: codeCard.highlighted ? TextEdit.RichText : TextEdit.PlainText
                    text: codeCard.highlighted ? root.block.html : (root.block.code || "")
                    color: Theme.text
                    selectionColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
                    font.family: "monospace"
                    font.pointSize: Theme.fontSmall
                }

                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
            }
        }
    }

    Component {
        id: quoteComponent

        Row {
            width: root.columnWidth
            spacing: 14

            Rectangle {
                width: 3
                height: quoteColumn.height
                radius: 2
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.85)
            }

            Column {
                id: quoteColumn
                width: parent.width - 3 - parent.spacing
                spacing: Math.round(8 * Theme.uiScale)

                Repeater {
                    model: root.block.items || []

                    ProseText {
                        required property var modelData
                        width: quoteColumn.width - (Math.max(1, modelData.level || 1) - 1) * 16
                        x: (Math.max(1, modelData.level || 1) - 1) * 16
                        html: modelData.html || ""
                        color: Theme.subtext
                    }
                }
            }
        }
    }

    Component {
        id: listComponent

        Column {
            id: listColumn
            width: root.columnWidth
            spacing: Math.round(5 * Theme.uiScale)

            readonly property real indentStep: Math.round(22 * Theme.uiScale)
            readonly property real markerWidth: Math.round(22 * Theme.uiScale)

            Repeater {
                model: root.block.items || []

                Item {
                    id: listItem
                    required property var modelData
                    readonly property int depth: Math.max(1, modelData.depth || 1)
                    readonly property int checked: modelData.checked === undefined ? -1 : modelData.checked
                    // Height of the first wrapped line, to centre the marker on it.
                    readonly property real firstLineHeight: itemText.contentHeight / Math.max(1, itemText.lineCount)
                    width: listColumn.width
                    height: itemText.height

                    // Marker: bullet, number, or a task checkbox.
                    Item {
                        id: marker
                        x: (listItem.depth - 1) * listColumn.indentStep
                        width: listColumn.markerWidth
                        height: itemText.height

                        Rectangle {
                            visible: listItem.checked < 0 && !listItem.modelData.ordered
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: Math.round(listItem.firstLineHeight / 2 - height / 2)
                            width: listItem.depth === 1 ? 6 : 5
                            height: width
                            radius: width / 2
                            color: listItem.depth === 1 ? Theme.accent : "transparent"
                            border.width: listItem.depth === 1 ? 0 : 1.2
                            border.color: Theme.subtext
                        }

                        Text {
                            visible: listItem.checked < 0 && listItem.modelData.ordered
                            anchors.right: parent.right
                            anchors.rightMargin: 6
                            text: (listItem.modelData.number || 1) + "."
                            color: Theme.accent
                            font.pointSize: Theme.fontNormal
                            font.weight: Font.DemiBold
                            lineHeight: root.proseLineHeight
                        }

                        Rectangle {
                            visible: listItem.checked >= 0
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: Math.round(listItem.firstLineHeight / 2 - height / 2)
                            width: Math.round(14 * Theme.uiScale)
                            height: width
                            radius: 3
                            color: listItem.checked === 1 ? Theme.accent : "transparent"
                            border.width: 1.2
                            border.color: listItem.checked === 1 ? Theme.accent : Theme.subtext

                            IconCheck {
                                anchors.centerIn: parent
                                visible: listItem.checked === 1
                                size: parent.width - 4
                                color: Theme.base
                                strokeWidth: 2.2
                            }
                        }
                    }

                    ProseText {
                        id: itemText
                        x: marker.x + marker.width + 4
                        width: listItem.width - x
                        html: listItem.modelData.html || ""
                        color: listItem.checked === 1 ? Theme.subtext : Theme.text
                    }
                }
            }
        }
    }

    Component {
        id: tableComponent

        Rectangle {
            id: tableCard
            width: root.columnWidth
            height: tableColumn.height
            radius: Theme.radiusMedium
            color: "transparent"
            border.width: 1
            border.color: root.cardBorder
            clip: true

            readonly property var header: root.block.header || []
            readonly property var rows: root.block.rows || []
            readonly property int columns: Math.max(1, header.length)
            readonly property real cellPadding: Math.round(10 * Theme.uiScale)
            readonly property real cellWidth: width / columns

            function alignmentFor(cell) {
                if (!cell) return Text.AlignLeft
                if (cell.align === "right") return Text.AlignRight
                if (cell.align === "center") return Text.AlignHCenter
                return Text.AlignLeft
            }

            Column {
                id: tableColumn
                width: parent.width

                // Row 0 is the header; the rest are body rows.
                Repeater {
                    model: [tableCard.header].concat(tableCard.rows)

                    Item {
                        id: tableRow
                        required property var modelData
                        required property int index
                        readonly property bool isHeader: index === 0
                        width: tableCard.width
                        height: rowContent.height

                        Rectangle {
                            anchors.fill: parent
                            color: tableRow.isHeader
                                ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.06)
                                : (tableRow.index % 2 === 0
                                    ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.025)
                                    : "transparent")
                        }

                        Row {
                            id: rowContent

                            Repeater {
                                model: tableRow.modelData

                                Item {
                                    required property var modelData
                                    width: tableCard.cellWidth
                                    height: cellText.height + tableCard.cellPadding * 2

                                    ProseText {
                                        id: cellText
                                        x: tableCard.cellPadding
                                        y: tableCard.cellPadding
                                        width: parent.width - tableCard.cellPadding * 2
                                        html: modelData.html || ""
                                        horizontalAlignment: tableCard.alignmentFor(modelData)
                                        font.weight: tableRow.isHeader ? Font.DemiBold : Font.Normal
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: root.dividerColor
                            visible: tableRow.isHeader
                        }
                    }
                }
            }
        }
    }

    Component {
        id: imageComponent

        Column {
            id: figureColumn
            width: root.columnWidth
            spacing: Math.round(6 * Theme.uiScale)

            readonly property bool remote: root.block.remote === true
            readonly property string caption: root.block.title || root.block.alt || ""

            Image {
                id: figure
                visible: !figureColumn.remote && status !== Image.Error
                anchors.horizontalCenter: parent.horizontalCenter
                source: figureColumn.remote ? "" : (root.block.source || "")
                asynchronous: true
                smooth: true
                fillMode: Image.PreserveAspectFit
                width: status === Image.Ready && implicitWidth > 0
                    ? Math.min(parent.width, implicitWidth)
                    : parent.width
                height: status === Image.Ready && implicitWidth > 0
                    ? width * implicitHeight / implicitWidth
                    : (status === Image.Loading ? Math.round(120 * Theme.uiScale) : 0)
            }

            // Remote images are never fetched by a file preview; say what
            // would be there instead of leaving a hole.
            Rectangle {
                visible: figureColumn.remote || figure.status === Image.Error
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, placeholderRow.implicitWidth + 28)
                height: Math.round(30 * Theme.uiScale)
                radius: height / 2
                color: root.pillColor
                border.width: 1
                border.color: root.cardBorder

                Row {
                    id: placeholderRow
                    anchors.centerIn: parent
                    spacing: 7

                    IconImage {
                        anchors.verticalCenter: parent.verticalCenter
                        size: Math.round(13 * Theme.uiScale)
                        color: Theme.muted
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            var label = root.block.alt || "image"
                            if (figureColumn.remote) {
                                var host = (root.block.source || "").replace(/^[a-z]+:\/\//i, "").split("/")[0]
                                return label + (host ? "  ·  " + host : "")
                            }
                            return label + "  ·  not found"
                        }
                        color: Theme.subtext
                        font.pointSize: Theme.fontSmall
                        elide: Text.ElideMiddle
                    }
                }
            }

            Text {
                visible: !figureColumn.remote && figure.status === Image.Ready && figureColumn.caption !== ""
                width: parent.width
                text: figureColumn.caption
                color: Theme.muted
                font.pointSize: Theme.fontSmall
                font.italic: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }

    Component {
        id: ruleComponent

        Item {
            width: root.columnWidth
            height: Math.round(18 * Theme.uiScale)

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: 1
                color: root.dividerColor
            }
        }
    }
}
