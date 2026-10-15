import QtQuick
import QtQuick.Controls
import Seraph

// Reader-style markdown preview: a centred reading column of MarkdownBlock
// delegates fed by MarkdownRenderer, a meta strip (word count, reading time),
// the front matter as a card, and a clickable outline of the headings.
Item {
    id: root

    property string source: ""
    property string baseDir: ""
    property string fileName: ""
    property bool showOutline: true
    // Space kept clear above the outline for overlays the host draws there.
    property real outlineTopInset: 0
    // Width of overlays the host draws at the top-right; the meta strip
    // wraps early so its chips never run underneath them.
    property real headerRightInset: 0

    property var document: root.emptyDocument()

    signal copyRequested(string text)

    // Reader-only knobs; the quick preview leaves them at their defaults.
    property bool keepScrollOnChange: false
    property bool interceptLinks: false
    property var renderOptions: ({})
    property int renderRevision: 0
    // Fetched remote images, handed to image blocks so they update in place.
    property var remoteImages: ({})
    signal linkRequested(string link)
    signal rendered(var document)

    readonly property real scrollY: flick.contentY

    // The visible slice of the reading column, in its coordinates, padded by
    // a screen on each side so blocks are already drawn when they scroll in.
    // Snapped to half-screen steps: every block re-checks itself only when
    // the slice moves a step, not on every pixel scrolled.
    readonly property real _viewportStep: Math.max(1, Math.round(flick.height / 2))
    readonly property real _viewportAnchor: Math.floor(flick.contentY / _viewportStep) * _viewportStep
    readonly property real viewportTop: _viewportAnchor - content.y - flick.height
    readonly property real viewportBottom: _viewportAnchor - content.y + 2 * flick.height + _viewportStep

    readonly property var outline: document && document.outline ? document.outline : []
    readonly property var blocks: document && document.blocks ? document.blocks : []
    readonly property var frontMatter: document && document.frontMatter ? document.frontMatter : []
    readonly property var stats: document && document.stats ? document.stats : ({})
    readonly property string title: document && document.title ? document.title : ""

    readonly property bool hasOutline: outline.length >= 2
    // The rail needs its own width plus a column that is still readable;
    // a laptop-sized pane (about 720 logical px) still qualifies.
    readonly property real outlineRailWidth: Math.round(200 * Theme.uiScale)
    readonly property bool outlineFits: width >= outlineRailWidth + Math.round(400 * Theme.uiScale)
    readonly property bool outlineAvailable: hasOutline && outlineFits
    readonly property bool outlineVisible: showOutline && outlineAvailable
    readonly property real outlineWidth: outlineVisible ? outlineRailWidth : 0
    readonly property real readingWidth: Math.round(720 * Theme.uiScale)
    readonly property real horizontalMargin: Math.round((outlineVisible ? 20 : 28) * Theme.uiScale)

    readonly property color dividerColor: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10)
    readonly property color cardColor: Qt.rgba(Theme.crust.r, Theme.crust.g, Theme.crust.b, 0.55)
    readonly property color cardBorder: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.08)
    readonly property color pillColor: Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.07)
    // Inline `code` gets an opaque chip colour: rich text cannot use alpha.
    readonly property color inlineCodeBackground: Qt.tint(Theme.base,
        Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10))
    readonly property bool lightTheme: (0.299 * Theme.base.r + 0.587 * Theme.base.g + 0.114 * Theme.base.b) > 0.55

    // Theme colours are baked into the inline HTML, so they are part of the
    // key that triggers a re-render along with the document itself.
    readonly property string renderKey: source + "\u0000" + baseDir + "\u0000"
        + inlineCodeBackground.toString() + Theme.accent.toString() + (lightTheme ? "L" : "D")
        + "\u0000" + renderRevision
    onRenderKeyChanged: rerender()
    Component.onCompleted: rerender()
    // The key changes while bindings settle at creation and onCompleted
    // follows; one render per distinct key is enough.
    property string _renderedKey: ""

    function emptyDocument() {
        return { blocks: [], outline: [], frontMatter: [], title: "", stats: { words: 0, readingMinutes: 0, headings: 0, codeBlocks: 0 } }
    }

    function rerender() {
        if (renderKey === _renderedKey)
            return
        _renderedKey = renderKey
        var keptY = flick.contentY
        if (source === "") {
            document = emptyDocument()
            return
        }
        var options = {
            highlightCode: runtimeFeatures.batAvailable,
            // bat's default theme is dark; on a light base pick a light one.
            batTheme: lightTheme ? "GitHub" : "",
            codeBackground: inlineCodeBackground.toString(),
            linkColor: Theme.accent.toString()
        }
        for (var key in renderOptions)
            options[key] = renderOptions[key]
        document = markdownRenderer.render(source, baseDir, options)
        _rerunSearch()
        if (keepScrollOnChange)
            scrollToY(keptY)
        else
            flick.contentY = 0
        rendered(document)
    }

    // Find in document: every hit is {target, start, end} on a markdownText
    // TextEdit; the current one is shown as that TextEdit's selection.
    property string _searchQuery: ""
    property var _hits: []
    property int searchIndex: -1
    readonly property int searchCount: _hits.length

    function _textTargets() {
        var out = []
        function walk(item) {
            if (item.objectName === "markdownText") {
                out.push(item)
                return
            }
            for (var i = 0; i < item.children.length; i++)
                walk(item.children[i])
        }
        for (var b = 0; b < blockRepeater.count; b++) {
            var block = blockRepeater.itemAt(b)
            if (block)
                walk(block)
        }
        return out
    }

    function _findHits(query) {
        var hits = []
        if (query === "")
            return hits
        var needle = query.toLocaleLowerCase()
        var targets = _textTargets()
        for (var t = 0; t < targets.length; t++) {
            var hay = targets[t].getText(0, targets[t].length).toLocaleLowerCase()
            var from = 0
            var at
            while ((at = hay.indexOf(needle, from)) !== -1) {
                hits.push({ target: targets[t], start: at, end: at + needle.length })
                from = at + needle.length
            }
        }
        return hits
    }

    function _showHit(index, reveal) {
        var hit = _hits[index]
        searchIndex = index
        if (!hit || !hit.target)
            return
        hit.target.select(hit.start, hit.end)
        if (!reveal)
            return
        var rect = hit.target.positionToRectangle(hit.start)
        var host = hit.target.hostFlick
        if (host) {
            var x = hit.target.x + rect.x
            if (x < host.contentX || x > host.contentX + host.width - 40)
                host.contentX = Math.max(0, Math.min(host.contentWidth - host.width, x - host.width / 3))
        }
        var point = hit.target.mapToItem(content, rect.x, rect.y)
        var maxY = Math.max(0, flick.contentHeight - flick.height)
        scrollAnimation.stop()
        scrollAnimation.to = Math.max(0, Math.min(maxY, content.y + point.y - flick.height / 3))
        scrollAnimation.start()
    }

    function search(query) {
        if (_selectionOwner)
            _selectionOwner.deselect()
        _searchQuery = query
        _hits = _findHits(query)
        searchIndex = -1
        if (_hits.length > 0)
            _showHit(0, true)
        return _hits.length
    }

    function searchNext() {
        if (_hits.length > 0)
            _showHit((searchIndex + 1) % _hits.length, true)
    }

    function searchPrevious() {
        if (_hits.length > 0)
            _showHit((searchIndex - 1 + _hits.length) % _hits.length, true)
    }

    function clearSearch() {
        if (_selectionOwner)
            _selectionOwner.deselect()
        _searchQuery = ""
        _hits = []
        searchIndex = -1
    }

    // After a re-render (live reload, image arrival) the TextEdits are new:
    // find the hits again and keep the same ordinal, without scrolling.
    function _rerunSearch() {
        if (_searchQuery === "")
            return
        var keep = searchIndex
        Qt.callLater(function() {
            _hits = _findHits(_searchQuery)
            searchIndex = -1
            if (_hits.length > 0)
                _showHit(Math.max(0, Math.min(keep, _hits.length - 1)), false)
        })
    }

    // Scroll requests made right after a re-render wait for the column to be
    // laid out; the last request wins (a "back" restore beats a kept scroll).
    property var _pendingScroll: null

    function _queueScroll(request) {
        _pendingScroll = request
        Qt.callLater(_applyPendingScroll)
    }

    function _applyPendingScroll() {
        var request = _pendingScroll
        _pendingScroll = null
        if (!request)
            return
        content.forceLayout()
        if (request.kind === "anchor") {
            _scrollToAnchorNow(request.value)
            return
        }
        var y = request.kind === "fraction" ? request.value * flick.contentHeight : request.value
        var maxY = Math.max(0, flick.contentHeight - flick.height)
        scrollAnimation.stop()
        flick.contentY = Math.max(0, Math.min(maxY, y))
    }

    function scrollToY(y) { _queueScroll({ kind: "y", value: y }) }
    function scrollToAnchor(fragment) { _queueScroll({ kind: "anchor", value: fragment }) }
    function scrollFraction() { return flick.contentHeight > 0 ? flick.contentY / flick.contentHeight : 0 }
    function scrollToFraction(f) { _queueScroll({ kind: "fraction", value: f }) }

    function _scrollToAnchorNow(fragment) {
        var wanted = fragment.toLowerCase()
        for (var i = 0; i < outline.length; i++) {
            if (slugify(outline[i].text) === wanted) {
                scrollToBlock(outline[i].blockIndex)
                return true
            }
        }
        return false
    }

    // One selection at a time across blocks: starting one clears the last.
    property Item _selectionOwner: null

    function _selectionStarted(item) {
        if (_selectionOwner && _selectionOwner !== item)
            _selectionOwner.deselect()
        _selectionOwner = item
    }

    function selectedText() {
        return _selectionOwner ? _selectionOwner.selectedText : ""
    }

    function slugify(text) {
        return text.toLowerCase().replace(/[^\p{L}\p{N}\s-]/gu, "").trim().replace(/\s+/g, "-")
    }

    function openLink(link) {
        if (link.startsWith("#")) {
            _scrollToAnchorNow(decodeURIComponent(link.substring(1)))
            return
        }
        if (interceptLinks) {
            linkRequested(link)
            return
        }
        Qt.openUrlExternally(link)
    }

    // Keyboard scrolling, driven by the host's key handlers.
    function scrollBy(delta) {
        var maxY = Math.max(0, flick.contentHeight - flick.height)
        scrollAnimation.stop()
        scrollAnimation.to = Math.max(0, Math.min(maxY, flick.contentY + delta))
        scrollAnimation.start()
    }

    function scrollByLines(lines) {
        scrollBy(lines * Math.round(28 * Theme.uiScale))
    }

    function scrollByPages(pages) {
        scrollBy(pages * Math.max(0, flick.height - Math.round(48 * Theme.uiScale)))
    }

    function scrollToEdge(top) {
        scrollBy(top ? -flick.contentHeight : flick.contentHeight)
    }

    function scrollToBlock(index) {
        var item = blockRepeater.itemAt(index)
        if (!item)
            return
        var target = content.y + item.y - Math.round(12 * Theme.uiScale)
        var maxY = Math.max(0, flick.contentHeight - flick.height)
        scrollAnimation.stop()
        scrollAnimation.to = Math.max(0, Math.min(maxY, target))
        scrollAnimation.start()
    }

    // The heading whose section is at the top of the viewport.
    readonly property int currentOutlineIndex: {
        var probe = flick.contentY + Math.round(72 * Theme.uiScale)
        var current = 0
        for (var i = 0; i < outline.length; i++) {
            var item = blockRepeater.itemAt(outline[i].blockIndex)
            if (!item)
                break
            if (content.y + item.y <= probe)
                current = i
            else
                break
        }
        return current
    }

    NumberAnimation {
        id: scrollAnimation
        target: flick
        property: "contentY"
        duration: Theme.animDuration
        easing.type: Easing.OutCubic
    }

    component MetaChip: Rectangle {
        property string label: ""
        property bool accented: false
        visible: label !== ""
        width: chipText.implicitWidth + 18
        height: Math.round(22 * Theme.uiScale)
        radius: height / 2
        color: root.pillColor
        border.width: 1
        border.color: root.cardBorder

        Text {
            id: chipText
            anchors.centerIn: parent
            text: parent.label
            color: parent.accented ? Theme.accent : Theme.subtext
            font.pointSize: Theme.fontSmall
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.rightMargin: root.outlineWidth
        contentWidth: width
        contentHeight: content.y + content.height + Math.round(40 * Theme.uiScale)
        clip: true
        boundsMovement: Flickable.StopAtBounds
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            width: Math.min(flick.width - 2 * root.horizontalMargin, root.readingWidth)
            x: Math.round((flick.width - width) / 2)
            y: Math.round(18 * Theme.uiScale)
            spacing: Math.round(14 * Theme.uiScale)

            // Meta strip: what the reader is about to read. Narrowed by
            // however far the host's overlay pills reach into the column.
            Flow {
                readonly property real intrusion: Math.max(0,
                    (content.x + content.width) - (root.width - root.headerRightInset))
                width: parent.width - intrusion
                spacing: 6

                MetaChip {
                    label: root.fileName
                    accented: true
                }
                MetaChip {
                    label: root.stats.words > 0 ? qsTr("%Ln word(s)", "", root.stats.words) : ""
                }
                MetaChip {
                    label: root.stats.readingMinutes > 0 ? qsTr("~%n min read", "", root.stats.readingMinutes) : ""
                }
                MetaChip {
                    label: root.stats.headings > 0
                        ? qsTr("%n heading(s)", "", root.stats.headings) : ""
                }
                MetaChip {
                    label: root.stats.codeBlocks > 0
                        ? qsTr("%n code block(s)", "", root.stats.codeBlocks) : ""
                }
            }

            // Front matter as a key/value card instead of raw YAML.
            Rectangle {
                visible: root.frontMatter.length > 0
                width: parent.width
                height: frontMatterColumn.height + Math.round(24 * Theme.uiScale)
                radius: Theme.radiusMedium
                color: root.cardColor
                border.width: 1
                border.color: root.cardBorder

                Column {
                    id: frontMatterColumn
                    x: Math.round(16 * Theme.uiScale)
                    y: Math.round(12 * Theme.uiScale)
                    width: parent.width - 2 * x
                    spacing: Math.round(6 * Theme.uiScale)

                    Repeater {
                        model: root.frontMatter

                        Row {
                            id: frontMatterRow
                            required property var modelData
                            width: frontMatterColumn.width
                            spacing: 12

                            Text {
                                width: Math.round(110 * Theme.uiScale)
                                text: frontMatterRow.modelData.key || ""
                                color: Theme.muted
                                font.pointSize: Theme.fontSmall
                                font.family: "monospace"
                                elide: Text.ElideRight
                            }

                            Text {
                                width: frontMatterRow.width - Math.round(110 * Theme.uiScale) - frontMatterRow.spacing
                                text: frontMatterRow.modelData.value || ""
                                color: Theme.text
                                font.pointSize: Theme.fontNormal
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }

            Repeater {
                id: blockRepeater
                model: root.blocks

                MarkdownBlock {
                    required property var modelData
                    block: modelData
                    inViewport: y + height >= root.viewportTop && y <= root.viewportBottom
                    columnWidth: content.width
                    inlineCodeBackground: root.inlineCodeBackground
                    remoteImages: root.remoteImages
                    onLinkActivated: (link) => root.openLink(link)
                    onCopyRequested: (text) => root.copyRequested(text)
                    onSelectionStarted: (item) => root._selectionStarted(item)
                }
            }

            Text {
                visible: root.blocks.length === 0
                width: parent.width
                text: root.source === "" ? "" : qsTr("Nothing to render in this document.")
                color: Theme.muted
                font.pointSize: Theme.fontNormal
                horizontalAlignment: Text.AlignHCenter
                topPadding: Math.round(40 * Theme.uiScale)
            }
        }

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
    }

    KineticWheelScroller {
        anchors.fill: flick
        flickable: flick
        wheelStep: 28
        touchpadMultiplier: 1.35
        minVelocity: 90
        maxVelocity: 2600
        kineticGain: 0.68
    }

    // Outline rail: every heading, current section marked, click to jump.
    Item {
        id: outlinePanel
        visible: root.outlineVisible
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.topMargin: root.outlineTopInset
        width: root.outlineWidth

        Rectangle {
            id: outlineRail
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: root.dividerColor
        }

        Text {
            id: outlineTitle
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Math.round(18 * Theme.uiScale)
            anchors.rightMargin: Math.round(12 * Theme.uiScale)
            anchors.topMargin: Math.round(14 * Theme.uiScale)
            text: qsTr("ON THIS PAGE")
            color: Theme.muted
            font.pointSize: Theme.fontSmall - 1
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
        }

        ListView {
            id: outlineList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: outlineTitle.bottom
            anchors.bottom: parent.bottom
            anchors.topMargin: Math.round(10 * Theme.uiScale)
            clip: true
            model: root.outline
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                id: outlineEntry
                required property var modelData
                required property int index
                readonly property bool current: index === root.currentOutlineIndex
                readonly property int level: Math.max(1, Math.min(6, modelData.level || 1))
                width: outlineList.width
                height: entryText.implicitHeight + Math.round(8 * Theme.uiScale)

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 1
                    color: entryHover.hovered
                        ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.05)
                        : "transparent"
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 3
                    anchors.bottomMargin: 3
                    width: 2
                    radius: 1
                    color: Theme.accent
                    visible: outlineEntry.current
                }

                Text {
                    id: entryText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Math.round(18 * Theme.uiScale) + (outlineEntry.level - 1) * Math.round(11 * Theme.uiScale)
                    anchors.rightMargin: Math.round(12 * Theme.uiScale)
                    text: outlineEntry.modelData.text || ""
                    color: outlineEntry.current ? Theme.text : Theme.subtext
                    font.pointSize: outlineEntry.level === 1 ? Theme.fontNormal : Theme.fontSmall
                    font.weight: outlineEntry.current || outlineEntry.level === 1 ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                HoverHandler { id: entryHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: root.scrollToBlock(outlineEntry.modelData.blockIndex) }
            }
        }
    }
}
