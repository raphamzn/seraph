#pragma once

#include <QLocale>
#include <QObject>
#include <QPointer>
#include <QString>

class QQmlEngine;
class QTranslator;

// Resolves the `language` setting ("auto" | "pt_BR" | "en") to an effective
// UI language, installs its translator and default locale, and retranslates
// the QML engine so every qsTr binding re-evaluates in place.
class LanguageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString effectiveLanguage READ effectiveLanguage NOTIFY languageChanged)

public:
    explicit LanguageManager(QObject *parent = nullptr,
                             const QString &qmPrefix = QStringLiteral(":/i18n/seraph_"));

    static QString resolve(const QString &setting, const QLocale &system);

    void setEngine(QQmlEngine *engine);
    QString effectiveLanguage() const;

    void apply(const QString &setting);
    void apply(const QString &setting, const QLocale &system);

signals:
    void languageChanged();

private:
    QString m_qmPrefix;
    QString m_effective;
    QTranslator *m_translator = nullptr;
    QPointer<QQmlEngine> m_engine;
};
