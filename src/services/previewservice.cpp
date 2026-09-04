#include "services/previewservice.h"
#include "services/bathighlighter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QColor>
#include <QCryptographicHash>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QRawFont>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString encodedUri(const QString &path)
{
    return QUrl(path).toString(QUrl::FullyEncoded);
}

bool runningInFlatpak()
{
    static const bool inSandbox = QFile::exists(QStringLiteral("/.flatpak-info"));
    return inSandbox;
}

// Spawn `gio cat <uri>` for reading trash:// URIs. Inside a Flatpak we
// route through `flatpak-spawn --host` so the host's gio reads from the
// host's real trash (the sandbox's gio sees only an empty per-app trash).
void startGioCat(QProcess &proc, const QString &uri)
{
    if (runningInFlatpak()) {
        proc.start(QStringLiteral("flatpak-spawn"),
                   {QStringLiteral("--host"), QStringLiteral("gio"),
                    QStringLiteral("cat"), uri});
    } else {
        proc.start(QStringLiteral("gio"), {QStringLiteral("cat"), uri});
    }
}

}

PreviewService::PreviewService(QObject *parent)
    : QObject(parent)
{
}

bool PreviewService::pdfPreviewAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("pdftoppm")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("pdfinfo")).isEmpty();
}

void PreviewService::refreshSupport()
{
    emit supportChanged();
}

QVariantMap PreviewService::loadTextPreview(const QString &path, int maxBytes, int maxLines) const
{
    QVariantMap result;
    bool truncated = false;
    QString error;
    const QByteArray data = readPathBytes(path, maxBytes, &truncated, &error);

    if (!error.isEmpty()) {
        result["content"] = QString();
        result["html"] = QString();
        result["truncated"] = false;
        result["isBinary"] = false;
        result["usesBat"] = false;
        result["error"] = error;
        return result;
    }

    const bool binary = looksBinary(data);
    QString text;
    if (!binary)
        text = decodeText(data);

    QStringList lines = text.split('\n');
    if (maxLines > 0 && lines.size() > maxLines) {
        lines = lines.mid(0, maxLines);
        truncated = true;
    }

    const QString plainText = lines.join('\n');
    result["content"] = plainText;
    result["html"] = QString();
    result["truncated"] = truncated;
    result["isBinary"] = binary;
    result["usesBat"] = false;
    result["error"] = QString();
    result["lineCount"] = lines.size();

    if (!binary) {
        const QString previewPath = localPreviewPath(path);
        if (!previewPath.isEmpty()) {
            QString batError;
            const QByteArray coloredOutput = BatHighlighter::highlightFile(previewPath, maxLines, &batError);
            if (!coloredOutput.isEmpty()) {
                result["html"] = BatHighlighter::ansiToHtml(coloredOutput);
                result["usesBat"] = true;
            }
        }
    }

    return result;
}

QVariantMap PreviewService::loadDirectoryPreview(const QString &path, int maxEntries) const
{
    QVariantMap result;
    bool truncated = false;
    QString error;
    const QStringList entries = listDirectoryEntries(path, maxEntries, &truncated, &error);

    result["entries"] = entries;
    result["truncated"] = truncated;
    result["error"] = error;
    result["count"] = entries.size();
    return result;
}

QVariantMap PreviewService::loadArchivePreview(const QString &path, int maxEntries) const
{
    QVariantMap result;
    result["entries"] = QStringList();
    result["truncated"] = false;
    result["error"] = QString();
    result["count"] = 0;

    // Determine list command based on archive type
    // Reuse the same detection as fileoperations
    const QString lower = path.toLower();
    QString program;
    QStringList args;

    if (lower.endsWith(".zip")) {
        program = "unzip";
        args = {"-Z1", path};
    } else if (lower.endsWith(".tar.gz") || lower.endsWith(".tgz")) {
        program = "tar";
        args = {"-tzf", path};
    } else if (lower.endsWith(".tar.xz") || lower.endsWith(".txz")) {
        program = "tar";
        args = {"-tJf", path};
    } else if (lower.endsWith(".tar.bz2") || lower.endsWith(".tbz2")) {
        program = "tar";
        args = {"-tjf", path};
    } else if (lower.endsWith(".tar")) {
        program = "tar";
        args = {"-tf", path};
    } else if (lower.endsWith(".7z") || lower.endsWith(".rar")) {
        program = "7z";
        args = {"l", "-slt", path};
    } else {
        result["error"] = "Unsupported archive format";
        return result;
    }

    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0) {
        result["error"] = "Could not list archive contents";
        return result;
    }

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QStringList entries;
    bool truncated = false;

    if (program == "7z") {
        // 7z -slt output: "Path = filename" lines
        static const QRegularExpression pathRe(R"(^Path = (.+)$)", QRegularExpression::MultilineOption);
        auto it = pathRe.globalMatch(output);
        while (it.hasNext()) {
            const QString entry = it.next().captured(1).trimmed();
            if (entry.isEmpty() || entry == path)
                continue;
            if (entries.size() >= maxEntries) { truncated = true; break; }
            entries.append(entry);
        }
    } else {
        const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty())
                continue;
            if (entries.size() >= maxEntries) { truncated = true; break; }
            entries.append(trimmed);
        }
    }

    result["entries"] = entries;
    result["truncated"] = truncated;
    result["count"] = entries.size();
    return result;
}

QString PreviewService::localPreviewPath(const QString &path) const
{
    if (path.isEmpty())
        return {};

    if (!isTrashUri(path))
        return QFileInfo::exists(path) ? path : QString();

    QString cacheRoot = QDir::homePath() + "/.cache/seraph/preview-cache";
    QDir().mkpath(cacheRoot);

    const QString suffix = QFileInfo(QUrl(path).fileName()).suffix();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha1).toHex());
    const QString cachedPath = QDir(cacheRoot).filePath(suffix.isEmpty() ? hash : hash + "." + suffix);

    QProcess proc;
    startGioCat(proc, encodedUri(path));
    if (!proc.waitForFinished(10000) || proc.exitCode() != 0)
        return {};

    QFile cacheFile(cachedPath);
    if (!cacheFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};

    cacheFile.write(proc.readAllStandardOutput());
    cacheFile.close();

    return cachedPath;
}

QVariantMap PreviewService::loadFontPreview(const QString &path)
{
    QVariantMap result;
    result["family"] = QString();
    result["styleName"] = QString();
    result["weight"] = static_cast<int>(QFont::Normal);
    result["italic"] = false;
    result["valid"] = false;
    result["error"] = QString();

    if (path.isEmpty() || !QFileInfo::exists(path)) {
        result["error"] = QStringLiteral("Font file not found");
        return result;
    }

    // Short-circuit when the same path is already loaded so repeated reads
    // (e.g. preview refresh on selection change) don't thrash the database.
    const bool alreadyLoaded = m_activeFontPreviewId >= 0 && m_activeFontPreviewPath == path;

    if (!alreadyLoaded) {
        if (m_activeFontPreviewId >= 0) {
            QFontDatabase::removeApplicationFont(m_activeFontPreviewId);
            m_activeFontPreviewId = -1;
            m_activeFontPreviewPath.clear();
        }

        const int id = QFontDatabase::addApplicationFont(path);
        if (id < 0) {
            result["error"] = QStringLiteral("Unable to load font file");
            return result;
        }
        m_activeFontPreviewId = id;
        m_activeFontPreviewPath = path;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(m_activeFontPreviewId);
    if (families.isEmpty()) {
        result["error"] = QStringLiteral("Font contains no usable families");
        return result;
    }

    const QString family = families.first();

    // Pull exact face metadata straight from the file so variants of the
    // same family (e.g. MapleMono-Bold vs MapleMono-Italic) don't alias.
    QRawFont raw(path, 16.0);
    QString styleName = raw.isValid() ? raw.styleName() : QString();
    int weight = raw.isValid() ? raw.weight() : static_cast<int>(QFont::Normal);
    const bool italic = raw.isValid() ? (raw.style() != QFont::StyleNormal) : false;

    if (styleName.isEmpty()) {
        const QStringList styles = QFontDatabase::styles(family);
        if (!styles.isEmpty())
            styleName = styles.first();
    }

    result["family"] = family;
    result["styleName"] = styleName;
    result["weight"] = weight;
    result["italic"] = italic;
    result["valid"] = true;
    return result;
}

QVariantMap PreviewService::loadPdfPreview(const QString &path) const
{
    QVariantMap result;
    result["localPath"] = QString();
    result["pageCount"] = 0;
    result["error"] = QString();

    const QString localPath = localPreviewPath(path);
    if (localPath.isEmpty()) {
        result["error"] = QStringLiteral("Unable to prepare PDF preview");
        return result;
    }

    if (!pdfPreviewAvailable()) {
        result["error"] = QStringLiteral("Install poppler-utils for PDF preview");
        return result;
    }

    QProcess proc;
    proc.start(QStringLiteral("pdfinfo"), {localPath});
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
        result["error"] = QStringLiteral("Unable to open PDF document");
        return result;
    }

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    static const QRegularExpression pagesRe(QStringLiteral(R"(^Pages:\s*(\d+))"),
                                            QRegularExpression::MultilineOption);
    const auto m = pagesRe.match(out);
    if (!m.hasMatch()) {
        result["error"] = QStringLiteral("Unable to read PDF page count");
        return result;
    }

    result["localPath"] = localPath;
    result["pageCount"] = m.captured(1).toInt();
    return result;
}

QByteArray PreviewService::readPathBytes(const QString &path, qint64 maxBytes, bool *truncated,
                                         QString *error) const
{
    if (truncated)
        *truncated = false;
    if (error)
        error->clear();

    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("No file selected");
        return {};
    }

    const qint64 readLimit = qMax<qint64>(1, maxBytes) + 1;

    if (isTrashUri(path)) {
        QProcess proc;
        startGioCat(proc, encodedUri(path));
        if (!proc.waitForStarted(2000)) {
            if (error)
                *error = QStringLiteral("Failed to start preview reader");
            return {};
        }

        QByteArray data;
        while (proc.state() != QProcess::NotRunning) {
            if (!proc.waitForReadyRead(100))
                proc.waitForFinished(100);
            data += proc.readAllStandardOutput();
            if (data.size() >= readLimit) {
                proc.kill();
                proc.waitForFinished(1000);
                break;
            }
        }
        data += proc.readAllStandardOutput();

        if (proc.exitStatus() != QProcess::NormalExit && data.isEmpty()) {
            if (error)
                *error = QStringLiteral("Failed to read preview data");
            return {};
        }

        if (data.size() > maxBytes) {
            if (truncated)
                *truncated = true;
            data.truncate(maxBytes);
        }
        return data;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return {};
    }

    QByteArray data = file.read(readLimit);
    if (data.size() > maxBytes) {
        if (truncated)
            *truncated = true;
        data.truncate(maxBytes);
    }
    return data;
}

QStringList PreviewService::listDirectoryEntries(const QString &path, int maxEntries, bool *truncated,
                                                QString *error) const
{
    if (truncated)
        *truncated = false;
    if (error)
        error->clear();

    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("No folder selected");
        return {};
    }

    if (isTrashUri(path)) {
        QProcess proc;
        proc.start("gio", {"list", "-h", encodedUri(path)});
        if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
            if (error)
                *error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            return {};
        }

        const QStringList allEntries = QString::fromUtf8(proc.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
        if (truncated)
            *truncated = maxEntries > 0 && allEntries.size() > maxEntries;
        return maxEntries > 0 ? allEntries.mid(0, maxEntries) : allEntries;
    }

    QDir dir(path);
    if (!dir.exists()) {
        if (error)
            *error = QStringLiteral("Folder does not exist");
        return {};
    }

    const QFileInfoList allEntries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                                                       QDir::DirsFirst | QDir::IgnoreCase | QDir::Name);
    QStringList names;
    const int count = maxEntries > 0 ? qMin(maxEntries, allEntries.size()) : allEntries.size();
    for (int i = 0; i < count; ++i) {
        const QFileInfo &info = allEntries.at(i);
        names.append(info.isDir() ? info.fileName() + "/" : info.fileName());
    }

    if (truncated)
        *truncated = maxEntries > 0 && allEntries.size() > maxEntries;
    return names;
}

bool PreviewService::isTrashUri(const QString &path)
{
    return QUrl(path).scheme() == QStringLiteral("trash");
}

bool PreviewService::looksBinary(const QByteArray &data)
{
    if (data.contains('\0'))
        return true;

    const int sampleSize = qMin(data.size(), 4096);
    if (sampleSize <= 0)
        return false;

    int suspicious = 0;
    for (int i = 0; i < sampleSize; ++i) {
        const unsigned char ch = static_cast<unsigned char>(data.at(i));
        const bool isWhitespace = ch == '\n' || ch == '\r' || ch == '\t' || ch == '\f';
        if (!isWhitespace && ch < 0x20)
            ++suspicious;
    }

    return suspicious * 10 > sampleSize;
}

QString PreviewService::decodeText(const QByteArray &data)
{
    const QString utf8 = QString::fromUtf8(data);
    if (!utf8.contains(QChar::ReplacementCharacter))
        return utf8;
    return QString::fromLocal8Bit(data);
}
