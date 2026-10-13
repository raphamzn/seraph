#include "services/readerdocument.h"

#include "services/previewservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

ReaderDocument::ReaderDocument(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(150);
}

QString ReaderDocument::fileName() const
{
    return QFileInfo(m_path).fileName();
}

QString ReaderDocument::baseDir() const
{
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).absolutePath();
}

void ReaderDocument::setDebounceInterval(int ms)
{
    m_debounce.setInterval(ms);
}

void ReaderDocument::open(const QString &path)
{
    const bool hadHistory = !m_history.isEmpty();
    m_history.clear();
    if (hadHistory)
        emit historyChanged();
    setPath(path);
    load(QStringLiteral("open"), QString(), -1);
}

void ReaderDocument::setPath(const QString &path)
{
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (clean == m_path)
        return;
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    m_path = clean;
}

void ReaderDocument::load(const QString &reason, const QString &fragment, qreal scrollY)
{
    const bool live = reason == QLatin1String("live");
    QString text;
    QString error;
    const QFileInfo info(m_path);
    const bool missingNow = !info.exists();

    if (!missingNow) {
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Cannot open %1: %2").arg(info.fileName(), file.errorString());
        } else if (file.size() > MaxFileBytes) {
            error = QStringLiteral("%1 is too large to show (over 32 MB).").arg(info.fileName());
        } else {
            const QByteArray data = file.readAll();
            if (PreviewService::looksBinary(data))
                error = QStringLiteral("This file looks binary and cannot be previewed as text.");
            else
                text = PreviewService::decodeText(data);
        }
    } else if (live) {
        // Deleted while open: keep the last version on screen.
        text = m_content;
    }

    if (!live)
        emit documentChanged();

    const bool contentDiffers = text != m_content;
    m_content = text;
    if (contentDiffers)
        emit contentChanged();

    if (error != m_error || missingNow != m_missing) {
        m_error = error;
        m_missing = missingNow;
        emit statusChanged();
    }

    rearmWatch();

    if (!live || contentDiffers)
        emit shown(reason, fragment, scrollY);
}

void ReaderDocument::rearmWatch()
{
    // Filled in by Task 4.
}

void ReaderDocument::navigate(const QString &, const QString &, qreal)
{
    // Filled in by Task 5.
}

bool ReaderDocument::goBack()
{
    // Filled in by Task 5.
    return false;
}

void ReaderDocument::reload()
{
    load(QStringLiteral("live"), QString(), -1);
}

QVariantMap ReaderDocument::classifyLink(const QString &link) const
{
    return classifyLink(link, m_path);
}

QVariantMap ReaderDocument::classifyLink(const QString &, const QString &)
{
    // Filled in by Task 5.
    return QVariantMap{{QStringLiteral("kind"), QStringLiteral("none")}};
}

bool ReaderDocument::isMarkdownPath(const QString &path)
{
    static const QStringList suffixes{
        QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("mdown"),
        QStringLiteral("mkd"), QStringLiteral("mkdn"), QStringLiteral("mdwn")};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}
