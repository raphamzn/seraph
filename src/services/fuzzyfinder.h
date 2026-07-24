#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

class QProcess;

// Fuzzy quick-open model. On open() it enumerates every file and directory
// under a root with `fd` (respecting .gitignore), then setQuery() fuzzy-ranks
// them so the QML overlay can show the best matches as the user types. Exposed
// to QML as the `fuzzyFinder` context property.
class FuzzyFinder : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int total READ total NOTIFY totalChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,  // basename
        DirRole,                      // containing dir, relative to root ("" at root)
        PathRole,                     // absolute path
        IsDirRole,
    };

    explicit FuzzyFinder(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_matches.size()); }
    bool scanning() const { return m_scanning; }
    int total() const { return static_cast<int>(m_all.size()); }

    // Start enumerating under `rootPath` (local paths only) and reset the query.
    Q_INVOKABLE void open(const QString &rootPath);
    // Re-rank the enumerated entries against `query`.
    Q_INVOKABLE void setQuery(const QString &query);
    // Cancel the scan and drop all results.
    Q_INVOKABLE void close();
    // {path, isDir, name} for a visible row — used to activate a result.
    Q_INVOKABLE QVariantMap entryAt(int row) const;

signals:
    void countChanged();
    void scanningChanged();
    void totalChanged();

private:
    QString absFor(int allIndex) const;
    void applyQuery();
    void onFdReadyRead();
    void onFdFinished();
    void stopFd();

    QString m_root;
    QString m_fdPath;
    QString m_query;
    QVector<QString> m_all;      // paths relative to m_root
    QVector<int> m_matches;      // indices into m_all, ranked best-first
    QProcess *m_fd = nullptr;
    bool m_scanning = false;

    static constexpr int kMaxResults = 250;
    static constexpr int kMaxEntries = 60000;  // safety cap for huge trees
};
