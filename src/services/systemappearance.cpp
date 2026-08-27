#include "services/systemappearance.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QDebug>
#include <QGuiApplication>
#include <QStyleHints>
#include <QVariant>

namespace {

constexpr auto kPortalService = "org.freedesktop.portal.Desktop";
constexpr auto kPortalPath = "/org/freedesktop/portal/desktop";
constexpr auto kSettingsInterface = "org.freedesktop.portal.Settings";
constexpr auto kAppearanceNamespace = "org.freedesktop.appearance";
constexpr auto kColorSchemeKey = "color-scheme";

// The portal hands the value back boxed, sometimes twice (Read() wraps what
// ReadOne() returns bare).
QVariant unbox(QVariant value)
{
    while (value.canConvert<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

// org.freedesktop.appearance color-scheme: 0 no preference, 1 dark, 2 light.
Qt::ColorScheme schemeFromPortalValue(const QVariant &value)
{
    bool ok = false;
    const uint raw = value.toUInt(&ok);
    if (!ok)
        return Qt::ColorScheme::Unknown;

    switch (raw) {
    case 1:
        return Qt::ColorScheme::Dark;
    case 2:
        return Qt::ColorScheme::Light;
    default:
        return Qt::ColorScheme::Unknown;
    }
}

QStyleHints *guiStyleHints()
{
    return qobject_cast<QGuiApplication *>(QCoreApplication::instance())
        ? QGuiApplication::styleHints()
        : nullptr;
}

}

SystemAppearance::SystemAppearance(QObject *parent) : QObject(parent)
{
    if (QStyleHints *hints = guiStyleHints()) {
        m_styleHints = hints->colorScheme();
        connect(hints, &QStyleHints::colorSchemeChanged,
                this, &SystemAppearance::setStyleHintsScheme);
    }

    subscribeToPortal();
    readPortalOnce();
    m_effective = m_portal != Qt::ColorScheme::Unknown ? m_portal : m_styleHints;
}

QString SystemAppearance::colorSchemeName() const
{
    switch (m_effective) {
    case Qt::ColorScheme::Light:
        return QStringLiteral("light");
    case Qt::ColorScheme::Dark:
        return QStringLiteral("dark");
    default:
        return QStringLiteral("unknown");
    }
}

void SystemAppearance::subscribeToPortal()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    bus.connect(kPortalService, kPortalPath, kSettingsInterface,
                QStringLiteral("SettingChanged"), this,
                SLOT(onPortalSettingChanged(QString, QString, QDBusVariant)));
}

void SystemAppearance::readPortalOnce()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;

    QDBusMessage request = QDBusMessage::createMethodCall(
        kPortalService, kPortalPath, kSettingsInterface, QStringLiteral("Read"));
    request << QString::fromLatin1(kAppearanceNamespace)
            << QString::fromLatin1(kColorSchemeKey);

    // Blocking, but bounded: the answer decides the very first frame's colors,
    // and a missing portal must not stall startup.
    const QDBusMessage reply = bus.call(request, QDBus::Block, 300);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;

    m_portal = schemeFromPortalValue(unbox(reply.arguments().constFirst()));
}

void SystemAppearance::onPortalSettingChanged(const QString &nameSpace, const QString &key,
                                              const QDBusVariant &value)
{
    if (nameSpace != QLatin1String(kAppearanceNamespace) || key != QLatin1String(kColorSchemeKey))
        return;

    setPortalScheme(schemeFromPortalValue(unbox(QVariant::fromValue(value))));
}

void SystemAppearance::setPortalScheme(Qt::ColorScheme scheme)
{
    if (m_portal == scheme)
        return;

    m_portal = scheme;
    updateEffective();
}

void SystemAppearance::setStyleHintsScheme(Qt::ColorScheme scheme)
{
    if (m_styleHints == scheme)
        return;

    m_styleHints = scheme;
    updateEffective();
}

void SystemAppearance::updateEffective()
{
    const Qt::ColorScheme effective =
        m_portal != Qt::ColorScheme::Unknown ? m_portal : m_styleHints;
    if (effective == m_effective)
        return;

    m_effective = effective;
    emit colorSchemeChanged(m_effective);
}
