#include "services/languagemanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QQmlEngine>
#include <QTranslator>

namespace {
const QString kPortuguese = QStringLiteral("pt_BR");
const QString kEnglish = QStringLiteral("en");
}

LanguageManager::LanguageManager(QObject *parent, const QString &qmPrefix)
    : QObject(parent)
    , m_qmPrefix(qmPrefix)
{
}

QString LanguageManager::resolve(const QString &setting, const QLocale &system)
{
    const QString normalized = setting.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
    if (normalized == QLatin1String("pt_br"))
        return kPortuguese;
    if (normalized == QLatin1String("en"))
        return kEnglish;
    if (!normalized.isEmpty() && normalized != QLatin1String("auto"))
        qWarning() << "Unknown language setting" << setting << "- following the system";

    return system.language() == QLocale::Portuguese ? kPortuguese : kEnglish;
}

void LanguageManager::setEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

QString LanguageManager::effectiveLanguage() const
{
    return m_effective;
}

void LanguageManager::apply(const QString &setting)
{
    apply(setting, QLocale::system());
}

void LanguageManager::apply(const QString &setting, const QLocale &system)
{
    const QString effective = resolve(setting, system);
    if (effective == m_effective)
        return;

    if (m_translator) {
        QCoreApplication::removeTranslator(m_translator);
        delete m_translator;
        m_translator = nullptr;
    }

    // English is the source language, but it still loads a plurals-only
    // translation so "%n item(s)" reads "1 item" / "2 items".
    auto *translator = new QTranslator(this);
    if (translator->load(m_qmPrefix + effective)) {
        QCoreApplication::installTranslator(translator);
        m_translator = translator;
    } else {
        qWarning() << "Could not load translation" << m_qmPrefix + effective;
        delete translator;
    }

    QLocale::setDefault(effective == kPortuguese
        ? QLocale(QLocale::Portuguese, QLocale::Brazil)
        : QLocale(QLocale::English, QLocale::UnitedStates));

    m_effective = effective;
    if (m_engine)
        m_engine->retranslate();
    emit languageChanged();
}
