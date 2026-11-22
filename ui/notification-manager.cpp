#include "notification-manager.hpp"
#include "settings-manager.hpp"
#include "error-handler.hpp"
#include "streamup-kit.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QSystemTrayIcon>

namespace StreamUP {
namespace NotificationManager {

// Every StreamUP message is a small toast inside OBS now, not a Windows tray
// balloon: it appears where you are looking and says what happened in a few
// words. The body is that short line; the title only stands in when there is
// no body. "Mute StreamUP messages" still silences them.
void SendTrayNotification(QSystemTrayIcon::MessageIcon icon, const QString& title, const QString& body)
{
    if (StreamUP::SettingsManager::AreNotificationsMuted()) {
        StreamUP::ErrorHandler::LogInfo("Notifications are muted, skipping toast", StreamUP::ErrorHandler::Category::UI);
        return;
    }

    const StreamUP::Kit::Status status = icon == QSystemTrayIcon::Critical ? StreamUP::Kit::Status::Bad
                                       : icon == QSystemTrayIcon::Warning  ? StreamUP::Kit::Status::Warn
                                                                           : StreamUP::Kit::Status::Ok;
    StreamUP::Kit::Toast(body.isEmpty() ? title : body, status);
}

bool IsSystemTrayAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages();
}

void SendInfoNotification(const QString& title, const QString& body)
{
    SendTrayNotification(QSystemTrayIcon::Information, title, body);
}

void SendWarningNotification(const QString& title, const QString& body)
{
    SendTrayNotification(QSystemTrayIcon::Warning, title, body);
}

void SendCriticalNotification(const QString& title, const QString& body)
{
    SendTrayNotification(QSystemTrayIcon::Critical, title, body);
}

} // namespace NotificationManager
} // namespace StreamUP
