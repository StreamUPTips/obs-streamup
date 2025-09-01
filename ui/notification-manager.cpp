#include "notification-manager.hpp"
#include "settings-manager.hpp"
#include "error-handler.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QSystemTrayIcon>

namespace StreamUP {
namespace NotificationManager {

void SendTrayNotification(QSystemTrayIcon::MessageIcon icon, const QString& title, const QString& body)
{
    if (StreamUP::SettingsManager::AreNotificationsMuted()) {
        StreamUP::ErrorHandler::LogInfo("Notifications are muted, skipping tray notification", StreamUP::ErrorHandler::Category::UI);
        return;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable() || !QSystemTrayIcon::supportsMessages()) {
        StreamUP::ErrorHandler::LogWarning("System tray notifications not available on this platform", StreamUP::ErrorHandler::Category::UI);
        return;
    }

    SystemTrayNotification* notification = new SystemTrayNotification{icon, title, body};

    obs_queue_task(
        OBS_TASK_UI,
        [](void* param) {
            auto notification = static_cast<SystemTrayNotification*>(param);
            void* systemTrayPtr = obs_frontend_get_system_tray();
            if (systemTrayPtr) {
                auto systemTray = static_cast<QSystemTrayIcon*>(systemTrayPtr);
                QString prefixedTitle = QString("[%1] %2").arg(obs_module_text("App.Name")).arg(notification->title);
                systemTray->showMessage(prefixedTitle, notification->body, notification->icon);
            }
            delete notification;
        },
        (void*)notification, false);
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