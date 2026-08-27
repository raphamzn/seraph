#pragma once

#include <QDBusVariant>
#include <QObject>
#include <QString>
#include <Qt>

// Reports the desktop's light/dark preference and keeps reporting it as it
// changes.
//
// Two sources, in priority order:
//  1. xdg-desktop-portal's org.freedesktop.appearance/color-scheme over DBus.
//     Works regardless of which Qt platform theme is loaded, which matters
//     because qt6ct (a common choice on wlroots compositors) never fills in
//     QStyleHints::colorScheme().
//  2. QStyleHints::colorScheme(), for platform themes that do resolve it.
class SystemAppearance : public QObject
{
    Q_OBJECT
    // "light", "dark", or "unknown" when neither source reports a preference.
    Q_PROPERTY(QString colorSchemeName READ colorSchemeName NOTIFY colorSchemeChanged)

public:
    explicit SystemAppearance(QObject *parent = nullptr);

    Qt::ColorScheme colorScheme() const { return m_effective; }
    QString colorSchemeName() const;

signals:
    void colorSchemeChanged(Qt::ColorScheme scheme);

private slots:
    // org.freedesktop.portal.Settings.SettingChanged(s namespace, s key, v value)
    void onPortalSettingChanged(const QString &nameSpace, const QString &key,
                                const QDBusVariant &value);

private:
    void setPortalScheme(Qt::ColorScheme scheme);
    void setStyleHintsScheme(Qt::ColorScheme scheme);
    void updateEffective();
    void subscribeToPortal();
    void readPortalOnce();

    Qt::ColorScheme m_portal = Qt::ColorScheme::Unknown;
    Qt::ColorScheme m_styleHints = Qt::ColorScheme::Unknown;
    Qt::ColorScheme m_effective = Qt::ColorScheme::Unknown;
};
