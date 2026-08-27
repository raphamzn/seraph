pragma Singleton
import QtQuick

// URLs for the image providers and for local files.
//
// A path is data, not URL syntax: a file legitimately named
// "É verdade? sim.mp4" carries a "?" that ends the path as far as QUrl is
// concerned, and a "%" that starts an escape sequence. Pasting the path
// straight into a source string silently truncated it, and the only symptom
// was a thumbnail that never appeared. Every path goes through here so that
// bug has one place to live rather than one per call site.
QtObject {
    id: root

    // Percent-encodes each segment but keeps the separators, so the result is
    // still a path and not one long escaped blob.
    function encodePath(path) {
        return String(path).split("/").map(encodeURIComponent).join("/")
    }

    // mtime is part of the URL because QML caches provider images by URL:
    // editing the file has to produce a different one or the stale thumbnail
    // is what gets shown.
    function thumbnail(path, modified) {
        if (!path)
            return ""
        return "image://thumbnail/" + encodePath(path)
            + "?mtime=" + new Date(modified).getTime()
    }

    function pdfPreview(path, page) {
        if (!path)
            return ""
        return "image://pdfpreview/" + encodeURIComponent(path) + "?page=" + page
    }

    function file(path) {
        if (!path)
            return ""
        return "file://" + encodePath(path)
    }
}
