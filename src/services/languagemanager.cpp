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
    const QLocale::Language language = effective == kPortuguese ? QLocale::Portuguese : QLocale::English;

    // Dates and numbers keep the system's regional formats (en_GB dates, pt_PT,
    // a de_DE user falling back to English) unless the user explicitly picked
    // a language other than the system's.
    const QString normalized = setting.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
    const bool explicitChoice = normalized == QLatin1String("pt_br") || normalized == QLatin1String("en");
    const QLocale formatLocale = !explicitChoice || system.language() == language
        ? system
        : (effective == kPortuguese ? QLocale(QLocale::Portuguese, QLocale::Brazil)
                                    : QLocale(QLocale::English, QLocale::UnitedStates));

    if (effective == m_effective && formatLocale == m_formatLocale)
        return;

    m_formatLocale = formatLocale;
    QLocale::setDefault(formatLocale);
    if (effective == m_effective) {
        // Same translation, different regional formats: views still re-read.
        emit languageChanged();
        return;
    }

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

    m_effective = effective;
    if (m_engine)
        m_engine->retranslate();
    emit languageChanged();
}
