#ifndef STREAMUP_NOTIFICATION_MANAGER_HPP
#define STREAMUP_NOTIFICATION_MANAGER_HPP

#include <QSystemTrayIcon>
#include <QString>

namespace StreamUP {
namespace NotificationManager {

/**
 * @brief Structure to hold notification data for system tray
 */
struct SystemTrayNotification {
    QSystemTrayIcon::MessageIcon icon;
    QString title;
    QString body;
};

/**
 * @brief Send a notification to the system tray
 * @param icon The icon type for the notification (Information, Warning, Critical)
 * @param title The title of the notification
 * @param body The body text of the notification
 * 
 * This function respects the global notification mute setting from SettingsManager.
 * If notifications are muted or system tray is unavailable, the notification will be logged only.
 */
void SendTrayNotification(QSystemTrayIcon::MessageIcon icon, const QString& title, const QString& body);

/**
 * @brief Check if system tray notifications are available on this platform
 * @return bool True if system tray supports notifications, false otherwise
 */
bool IsSystemTrayAvailable();

/**
 * @brief Send an information notification
 * @param title The title of the notification
 * @param body The body text of the notification
 */
void SendInfoNotification(const QString& title, const QString& body);

/**
 * @brief Send a warning notification
 * @param title The title of the notification
 * @param body The body text of the notification
 */
void SendWarningNotification(const QString& title, const QString& body);

/**
 * @brief Send a critical/error notification
 * @param title The title of the notification
 * @param body The body text of the notification
 */
void SendCriticalNotification(const QString& title, const QString& body);

} // namespace NotificationManager
} // namespace StreamUP

#endif // STREAMUP_NOTIFICATION_MANAGER_HPP
