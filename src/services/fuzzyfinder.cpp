#include "services/fuzzyfinder.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QVariantMap>
#include <algorithm>

namespace {

bool isBoundary(QChar prev, QChar cur)
{
    return prev == QLatin1Char('/') || prev == QLatin1Char('_')
           || prev == QLatin1Char('-') || prev == QLatin1Char('.')
           || prev == QLatin1Char(' ')
           || (prev.isLower() && cur.isUpper());  // camelCase
}

// Subsequence fuzzy score of `query` (already lowercased) against `text`.
// Higher is better; returns -1 when the query is not a subsequence.
int fuzzyScore(const QString &text, const QString &query)
{
    if (query.isEmpty())
        return 0;

    const int n = text.size();
    const int m = query.size();
    int ti = 0, qi = 0, score = 0, streak = 0;
    while (ti < n && qi < m) {
        if (text.at(ti).toLower() == query.at(qi)) {
            score += 1 + streak * 4;                       // consecutive run
            const QChar prev = ti > 0 ? text.at(ti - 1) : QLatin1Char('/');
            if (isBoundary(prev, text.at(ti)))
                score += 10;                               // word boundary
            if (ti == 0)
                score += 6;
            ++streak;
            ++qi;
        } else {
            streak = 0;
        }
        ++ti;
    }
    if (qi < m)
        return -1;                                         // not all matched
    score -= n / 10;                                       // mild shorter-is-better
    return score;
}

} // namespace

FuzzyFinder::FuzzyFinder(QObject *parent) : QAbstractListModel(parent)
{
    m_fdPath = QStandardPaths::findExecutable(QStringLiteral("fd"));
    if (m_fdPath.isEmpty())
        m_fdPath = QStandardPaths::findExecutable(QStringLiteral("fdfind"));
}

QString FuzzyFinder::absFor(int allIndex) const
{
    return m_root + QLatin1Char('/') + m_all.at(allIndex);
}

int FuzzyFinder::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_matches.size());
}

QVariant FuzzyFinder::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= m_matches.size())
        return {};

    const QString &rel = m_all.at(m_matches.at(row));
    const int slash = rel.lastIndexOf(QLatin1Char('/'));

    switch (role) {
    case NameRole:
        return slash >= 0 ? rel.mid(slash + 1) : rel;
    case DirRole:
        return slash >= 0 ? rel.left(slash) : QString();
    case PathRole:
        return m_root + QLatin1Char('/') + rel;
    case IsDirRole:
        return QFileInfo(m_root + QLatin1Char('/') + rel).isDir();
    default:
        return {};
    }
}

QHash<int, QByteArray> FuzzyFinder::roleNames() const
{
    return {
        {NameRole, "name"},
        {DirRole, "dir"},
        {PathRole, "path"},
        {IsDirRole, "isDir"},
    };
}

void FuzzyFinder::open(const QString &rootPath)
{
    stopFd();

    beginResetModel();
    m_all.clear();
    m_matches.clear();
    m_query.clear();
    endResetModel();
    emit countChanged();
    emit totalChanged();

    m_root = rootPath;

    // Local paths only, and fd must be present.
    if (rootPath.isEmpty() || rootPath.contains(QStringLiteral("://")) || m_fdPath.isEmpty())
        return;

    m_fd = new QProcess(this);
    m_fd->setWorkingDirectory(rootPath);
    connect(m_fd, &QProcess::readyReadStandardOutput, this, &FuzzyFinder::onFdReadyRead);
    connect(m_fd, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FuzzyFinder::onFdFinished);

    m_fd->setProgram(m_fdPath);
    // No pattern -> list everything; respect .gitignore, include dotfiles but
    // never descend into .git.
    m_fd->setArguments({QStringLiteral("--type"), QStringLiteral("f"),
                        QStringLiteral("--type"), QStringLiteral("d"),
                        QStringLiteral("--hidden"),
                        QStringLiteral("--exclude"), QStringLiteral(".git"),
                        QStringLiteral("--color"), QStringLiteral("never")});
    m_scanning = true;
    emit scanningChanged();
    m_fd->start();
}

void FuzzyFinder::onFdReadyRead()
{
    if (!m_fd)
        return;

    bool grew = false;
    while (m_fd->canReadLine()) {
        if (m_all.size() >= kMaxEntries) {
            stopFd();
            break;
        }
        QString line = QString::fromUtf8(m_fd->readLine()).trimmed();
        if (line.endsWith(QLatin1Char('/')))
            line.chop(1);
        if (line.isEmpty())
            continue;
        m_all.append(line);
        grew = true;

        // While scanning with no query, stream results straight through.
        if (m_query.isEmpty() && m_matches.size() < kMaxResults) {
            const int row = static_cast<int>(m_matches.size());
            beginInsertRows(QModelIndex(), row, row);
            m_matches.append(m_all.size() - 1);
            endInsertRows();
            emit countChanged();
        }
    }
    if (grew)
        emit totalChanged();
}

void FuzzyFinder::onFdFinished()
{
    m_scanning = false;
    emit scanningChanged();
    // Final ranking pass now that everything is in.
    applyQuery();
}

void FuzzyFinder::setQuery(const QString &query)
{
    const QString trimmed = query.trimmed();
    if (trimmed == m_query)
        return;
    m_query = trimmed;
    applyQuery();
}

void FuzzyFinder::applyQuery()
{
    QVector<int> next;

    if (m_query.isEmpty()) {
        const int n = std::min(static_cast<int>(m_all.size()), kMaxResults);
        next.reserve(n);
        for (int i = 0; i < n; ++i)
            next.append(i);
    } else {
        const QString q = m_query.toLower();
        QVector<QPair<int, int>> scored;  // (score, allIndex)
        scored.reserve(m_all.size());
        for (int i = 0; i < m_all.size(); ++i) {
            const int s = fuzzyScore(m_all.at(i), q);
            if (s >= 0)
                scored.append({s, i});
        }
        std::sort(scored.begin(), scored.end(), [this](const QPair<int, int> &a, const QPair<int, int> &b) {
            if (a.first != b.first)
                return a.first > b.first;                       // higher score first
            const int la = m_all.at(a.second).size();
            const int lb = m_all.at(b.second).size();
            if (la != lb)
                return la < lb;                                 // shorter path first
            return m_all.at(a.second) < m_all.at(b.second);
        });
        const int n = std::min(static_cast<int>(scored.size()), kMaxResults);
        next.reserve(n);
        for (int i = 0; i < n; ++i)
            next.append(scored.at(i).second);
    }

    beginResetModel();
    m_matches = std::move(next);
    endResetModel();
    emit countChanged();
}

QVariantMap FuzzyFinder::entryAt(int row) const
{
    if (row < 0 || row >= m_matches.size())
        return {};
    const QString abs = absFor(m_matches.at(row));
    return QVariantMap{
        {QStringLiteral("path"), abs},
        {QStringLiteral("isDir"), QFileInfo(abs).isDir()},
    };
}

void FuzzyFinder::stopFd()
{
    if (m_fd) {
        m_fd->disconnect(this);
        if (m_fd->state() != QProcess::NotRunning) {
            m_fd->kill();
            m_fd->waitForFinished(500);
        }
        m_fd->deleteLater();
        m_fd = nullptr;
    }
    if (m_scanning) {
        m_scanning = false;
        emit scanningChanged();
    }
}

void FuzzyFinder::close()
{
    stopFd();
    beginResetModel();
    m_all.clear();
    m_matches.clear();
    m_query.clear();
    endResetModel();
    emit countChanged();
    emit totalChanged();
}
