import QtQuick
import QtQuick.Controls
import Seraph

// `seraph --view <file>`: one markdown document per window, rendered by the
// same MarkdownView as the quick preview.
ApplicationWindow {
    id: window

    width: 900
    height: 1000
    visible: true
    color: Theme.base
    title: markdownView.title !== "" ? markdownView.title
        : (readerDocument.fileName !== "" ? readerDocument.fileName : "Seraph Reader")

    readonly property var zoomSteps: [0.5, 0.67, 0.8, 0.9, 1.0, 1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0]
    readonly property bool showingError: readerDocument.errorMessage !== ""
        || (readerDocument.missing && readerDocument.content === "")

    Component.onCompleted: Theme.zoom = config.readerZoom

    function setZoom(zoom) {
        var fraction = markdownView.scrollFraction()
        Theme.zoom = zoom
        config.saveReaderZoom(zoom)
        markdownView.scrollToFraction(fraction)
    }

    function zoomStep(direction) {
        var current = Theme.zoom
        if (direction > 0) {
            for (var i = 0; i < zoomSteps.length; i++) {
                if (zoomSteps[i] > current + 0.001) {
                    setZoom(zoomSteps[i])
                    return
                }
            }
        } else {
            for (var j = zoomSteps.length - 1; j >= 0; j--) {
                if (zoomSteps[j] < current - 0.001) {
                    setZoom(zoomSteps[j])
                    return
                }
            }
        }
    }

    // The configured editor when installed, else the first one detected.
    function editorId() {
        var editors = editorLauncher.editors
        for (var i = 0; i < editors.length; i++) {
            if (editors[i].id === config.readerEditor)
                return editors[i].id
        }
        return editors.length > 0 ? editors[0].id : ""
    }

    function followLink(link) {
        var target = readerDocument.classifyLink(link)
        switch (target.kind) {
        case "anchor":
            if (target.fragment !== "")
                markdownView.scrollToAnchor(target.fragment)
            break
        case "markdown":
            readerDocument.navigate(target.path, target.fragment, markdownView.scrollY)
            break
        case "file":
        case "web":
            Qt.openUrlExternally(target.url)
            break
        }
    }

    // Re-render only when an image inside prose arrives: image blocks update
    // in place, and every re-render drops the user's selection. Plain
    // counters, not a binding on the document, which the re-render replaces.
    property int inlineImageRevision: 0
    property int inlineImagesShown: 0

    function fetchedInlineImages(document) {
        var inline = document && document.inlineRemoteImages ? document.inlineRemoteImages : []
        var images = imageFetcher.images
        var fetched = 0
        for (var i = 0; i < inline.length; i++) {
            if (images[inline[i]] && images[inline[i]].path)
                fetched++
        }
        return fetched
    }

    Connections {
        target: imageFetcher
        function onImagesChanged() {
            if (window.fetchedInlineImages(markdownView.document) !== window.inlineImagesShown)
                window.inlineImageRevision++
        }
    }

    function openSearch() { searchBar.open() }
    function closeSearch() {
        searchBar.visible = false
        markdownView.clearSearch()
        keyHandler.forceActiveFocus()
    }

    Connections {
        target: readerDocument
        function onShown(reason, fragment, scrollY) {
            if (reason === "live")
                return
            if (fragment !== "")
                markdownView.scrollToAnchor(fragment)
            else
                markdownView.scrollToY(scrollY >= 0 ? scrollY : 0)
        }
    }

    // Mouse "back" button, under everything so it never eats normal clicks.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        onClicked: readerDocument.goBack()
    }

    // Scroll keys. Prose never takes focus, so this keeps it unless the
    // search field has it.
    Item {
        id: keyHandler
        anchors.fill: parent
        focus: true
        Keys.onPressed: (event) => {
            switch (event.key) {
            case Qt.Key_Up: markdownView.scrollByLines(-3); break
            case Qt.Key_Down: markdownView.scrollByLines(3); break
            case Qt.Key_PageUp: markdownView.scrollByPages(-1); break
            case Qt.Key_PageDown: markdownView.scrollByPages(1); break
            case Qt.Key_Space: markdownView.scrollByPages(event.modifiers & Qt.ShiftModifier ? -1 : 1); break
            case Qt.Key_Home: markdownView.scrollToEdge(true); break
            case Qt.Key_End: markdownView.scrollToEdge(false); break
            default: return
            }
            event.accepted = true
        }
    }

    Shortcut { sequence: "Ctrl+F"; onActivated: window.openSearch() }
    Shortcut { sequence: "Ctrl+R"; onActivated: readerDocument.reload() }
    Shortcut {
        sequence: "Ctrl+E"
        onActivated: {
            var id = window.editorId()
            if (id !== "")
                editorLauncher.openIn(id, readerDocument.filePath)
        }
    }
    Shortcut { sequence: "Ctrl+W"; onActivated: window.close() }
    Shortcut { sequence: "Escape"; onActivated: searchBar.visible ? window.closeSearch() : window.close() }
    Shortcut { sequences: ["Ctrl+=", "Ctrl++"]; onActivated: window.zoomStep(1) }
    Shortcut { sequence: "Ctrl+-"; onActivated: window.zoomStep(-1) }
    Shortcut { sequence: "Ctrl+0"; onActivated: window.setZoom(1.0) }
    Shortcut { sequence: "Alt+Left"; onActivated: readerDocument.goBack() }
    Shortcut {
        sequences: [StandardKey.Copy]
        onActivated: {
            var text = markdownView.selectedText()
            if (text !== "")
                fileOps.copyTextToClipboard(text)
        }
    }

    // Deleted while open: the last version stays, with this notice on top.
    Rectangle {
        id: missingBanner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        visible: readerDocument.missing && readerDocument.content !== ""
        height: visible ? Math.round(34 * Theme.uiScale) : 0
        color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.18)

        Text {
            anchors.centerIn: parent
            text: "The file was deleted. Showing the last version until it comes back."
            color: Theme.text
            font.pointSize: Theme.fontSmall
        }
    }

    ReaderSearchBar {
        id: searchBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: missingBanner.bottom
        visible: false
        count: markdownView.searchCount
        index: markdownView.searchIndex
        onSearchRequested: (query) => markdownView.search(query)
        onNextRequested: markdownView.searchNext()
        onPreviousRequested: markdownView.searchPrevious()
        onCloseRequested: window.closeSearch()
    }

    MarkdownView {
        id: markdownView
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchBar.bottom
        anchors.bottom: parent.bottom
        visible: !window.showingError
        source: readerDocument.errorMessage === "" ? readerDocument.content : ""
        baseDir: readerDocument.baseDir
        fileName: readerDocument.fileName
        keepScrollOnChange: true
        interceptLinks: true
        renderOptions: ({
            fetchRemoteImages: true,
            remoteImages: imageFetcher.images,
            inlineImageMaxWidth: Math.round(680 * Theme.uiScale)
        })
        remoteImages: imageFetcher.images
        renderRevision: window.inlineImageRevision
        onLinkRequested: (link) => window.followLink(link)
        onCopyRequested: (text) => fileOps.copyTextToClipboard(text)
        onRendered: (document) => {
            window.inlineImagesShown = window.fetchedInlineImages(document)
            imageFetcher.fetch(document.remoteImages || [])
        }
    }

    // Missing, unreadable or binary file: say so instead of closing.
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, Math.round(560 * Theme.uiScale))
        spacing: Math.round(10 * Theme.uiScale)
        visible: window.showingError

        Text {
            width: parent.width
            text: readerDocument.errorMessage !== "" ? readerDocument.errorMessage : "File not found"
            color: Theme.text
            font.pointSize: Theme.fontLarge
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            text: readerDocument.filePath
            color: Theme.muted
            font.pointSize: Theme.fontSmall
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere
        }
        Text {
            width: parent.width
            visible: readerDocument.canGoBack
            text: "Alt+Left goes back"
            color: Theme.subtext
            font.pointSize: Theme.fontSmall
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
