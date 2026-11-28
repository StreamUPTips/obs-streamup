#include "features-page.hpp"
#include "menu-manager.hpp"
#include "notification-manager.hpp"
#include <streamup/debug-logger.hpp>
#include <obs.h>
#include "source-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"
#include "splash-screen.hpp"
#include "patch-notes-window.hpp"
#include "theme-window.hpp"
#include "websocket-window.hpp"
#include "settings-manager.hpp"
#include "backup-dialog.hpp"
#include "restore-dialog.hpp"
#include "overlay-controls-window.hpp"
#include "my-products-window.hpp"
#include "../core/overlay-controls.hpp"
#include "../multidock/multidock_dialogs.hpp"
#include "../multidock/multidock_manager.hpp"
#include "../core/streamup-common.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>


// Forward declarations for dialog functions (defined in main streamup.cpp for now)
extern void SettingsDialog();

namespace StreamUP {
namespace MenuManager {

void InitializeMenu()
{
    blog(LOG_DEBUG, "[StreamUP] InitializeMenu: Starting menu initialization");

    blog(LOG_DEBUG, "[StreamUP] InitializeMenu: Creating QMenu");
    QMenu* menu = nullptr;

#if defined(_WIN32)
    // Windows: Add to main menu bar
    void* main_window_ptr = obs_frontend_get_main_window();
    if (!main_window_ptr) {
        StreamUP::DebugLogger::LogError("MenuManager", "Could not find main window");
        return;
    }

    QMainWindow* main_window = static_cast<QMainWindow*>(main_window_ptr);
    QMenuBar* menuBar = main_window->menuBar();
    if (!menuBar) {
        StreamUP::DebugLogger::LogError("MenuManager", "Could not find main menu bar");
        return;
    }

    menu = new QMenu(obs_module_text("StreamUP"), menuBar);
    menuBar->addMenu(menu);
#else
    // macOS and Linux: Add to Tools menu
    blog(LOG_DEBUG, "[StreamUP] InitializeMenu: Mac/Linux platform - adding to Tools menu");

    const char* menu_text = obs_module_text("StreamUP");
    blog(LOG_DEBUG, "[StreamUP] InitializeMenu: Menu text: %s", menu_text ? menu_text : "NULL");

    QAction* action = static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(menu_text));
    if (!action) {
        blog(LOG_ERROR, "[StreamUP] InitializeMenu: obs_frontend_add_tools_menu_qaction returned NULL");
        return;
    }

    menu = new QMenu(obs_module_text("StreamUP"));
    action->setMenu(menu);
#endif

    // Connect dynamic loader
    blog(LOG_DEBUG, "[StreamUP] InitializeMenu: Connecting dynamic loader");
    QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenuItems(menu); });

    blog(LOG_DEBUG, "[StreamUP] InitializeMenu: Menu initialization completed successfully");
}

void LoadMenuItems(QMenu* menu)
{
    menu->clear();
    QAction* action;

    // Read on every open (this runs on QMenu::aboutToShow), so a part
    // switched on or off on the Features page shows or hides here straight
    // away. A part that is off is left out entirely.
    StreamUP::SettingsManager::PluginSettings menuSettings = StreamUP::SettingsManager::GetCurrentSettings();
    const StreamUP::SettingsManager::ModuleSettings &menuMod = menuSettings.modules;

    // -- Products ---------------------------------------------------------
    // My Products: everything StreamUP installed in this collection, each
    // with its controls one click away.
    action = menu->addAction(obs_module_text("Menu.MyProducts"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::MyProductsWindow::Show(); });

#ifdef _WIN32
    action = menu->addAction(obs_module_text("Menu.Plugin.InstallProduct"));
    QObject::connect(action, &QAction::triggered, []() {
        // Hold Shift to skip the plugin check
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            StreamUP::FileManager::LoadStreamupFile(true);
        } else {
            StreamUP::FileManager::LoadStreamupFileWithWarning();
        }
    });

    action = menu->addAction(obs_module_text("Menu.Plugin.CheckRequirements"));
    QObject::connect(action, &QAction::triggered, []() {
        // Hold Shift to refresh the cached check first
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            StreamUP::PluginManager::PerformPluginCheckAndCache();
        }
        StreamUP::PluginManager::ShowCachedPluginIssuesDialog();
    });

    // A link to the website, so no ellipsis.
    action = menu->addAction(obs_module_text("Menu.Plugin.BrowseProducts"));
    QObject::connect(action, &QAction::triggered, []() {
        QDesktopServices::openUrl(QUrl("https://streamup.tips/"));
    });
#endif

    menu->addSeparator();

    // -- Tools ------------------------------------------------------------
    // Quick Actions: the source and device actions the StreamUP Dock's
    // buttons also run, so they follow that part's switch.
    if (menuMod.streamupDock) {
        QMenu* quickMenu = menu->addMenu(obs_module_text("Menu.QuickActions"));

        action = quickMenu->addAction(obs_module_text("Menu.Source.LockAllSources"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::SourceManager::ToggleLockAllSources();
        });

        action = quickMenu->addAction(obs_module_text("Menu.Source.LockCurrentSources"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::SourceManager::ToggleLockSourcesInCurrentScene();
        });

        quickMenu->addSeparator();

        action = quickMenu->addAction(obs_module_text("Menu.Source.RefreshAudioMonitoring"));
        QObject::connect(action, &QAction::triggered, []() {
            obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr);
            StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.AudioMonitoring.Title"),
                                                                obs_module_text("Feature.AudioMonitoring.Notification"));
        });

        action = quickMenu->addAction(obs_module_text("Menu.Source.RefreshBrowserSources"));
        QObject::connect(action, &QAction::triggered, []() {
            obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr);
            StreamUP::NotificationManager::SendInfoNotification(obs_module_text("Feature.BrowserSources.Title"),
                                                                obs_module_text("Feature.BrowserSources.Notification"));
        });

        QMenu* videoDeviceMenu = quickMenu->addMenu(obs_module_text("Menu.VideoCapture.Root"));

        action = videoDeviceMenu->addAction(obs_module_text("Menu.VideoCapture.ActivateAll"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::SourceManager::ActivateAllVideoCaptureDevices();
        });

        action = videoDeviceMenu->addAction(obs_module_text("Menu.VideoCapture.DeactivateAll"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::SourceManager::DeactivateAllVideoCaptureDevices();
        });

        action = videoDeviceMenu->addAction(obs_module_text("Menu.VideoCapture.RefreshAll"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::SourceManager::RefreshAllVideoCaptureDevices();
        });
    }

    if (menuMod.multiDock) {
        QMenu* multiDockMenu = menu->addMenu(obs_module_text("Menu.MultiDock"));

        action = multiDockMenu->addAction(obs_module_text("Menu.MultiDock.New"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::MultiDock::ShowNewMultiDockDialog();
        });

        action = multiDockMenu->addAction(obs_module_text("Menu.MultiDock.Manage"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::MultiDock::ShowManageMultiDocksDialog();
        });

        // Each existing Multi-Dock, ticked while it is showing.
        StreamUP::MultiDock::MultiDockManager* manager = StreamUP::MultiDock::MultiDockManager::Instance();
        if (manager) {
            QList<StreamUP::MultiDock::MultiDockInfo> multiDocks = manager->GetMultiDockInfoList();
            if (!multiDocks.isEmpty()) {
                multiDockMenu->addSeparator();
                for (const StreamUP::MultiDock::MultiDockInfo& info : multiDocks) {
                    QAction* dockAction = multiDockMenu->addAction(info.name);
                    dockAction->setCheckable(true);
                    dockAction->setChecked(manager->IsMultiDockVisible(info.id));
                    QObject::connect(dockAction, &QAction::triggered, [manager, info](bool checked) {
                        manager->SetMultiDockVisible(info.id, checked);
                    });
                }
            }
        }
    }

    if (menuMod.backup) {
        QMenu* backupMenu = menu->addMenu(obs_module_text("Menu.Backup"));
        action = backupMenu->addAction(obs_module_text("Menu.Backup.Create"));
        QObject::connect(action, &QAction::triggered, []() { StreamUP::Backup::ShowCreateBackupDialog(); });
        action = backupMenu->addAction(obs_module_text("Menu.Backup.Restore"));
        QObject::connect(action, &QAction::triggered, []() { StreamUP::Restore::ShowRestoreDialog(); });

        backupMenu->addSeparator();

        action = backupMenu->addAction(obs_module_text("Menu.Backup.Settings"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::SettingsManager::ShowSettingsDialog(StreamUP::SettingsManager::SettingsTabBackup);
        });
    }

    action = menu->addAction(obs_module_text("Menu.Plugin.CheckUpdates"));
    QObject::connect(action, &QAction::triggered, []() {
        // Hold Shift to refresh the cached check first
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            StreamUP::PluginManager::PerformPluginCheckAndCache();
        }
        StreamUP::PluginManager::ShowCachedPluginUpdatesDialog();
    });

    menu->addSeparator();

    // -- Set-up -----------------------------------------------------------
    // Settings opens on Tools and tweaks (what was the Features page), so a
    // part that is off can always be found again.
    action = menu->addAction(obs_module_text("Menu.Settings"));
    QObject::connect(action, &QAction::triggered, []() { SettingsDialog(); });

    menu->addSeparator();

    // -- Help -------------------------------------------------------------
    QMenu* helpMenu = menu->addMenu(obs_module_text("Menu.Help"));

    action = helpMenu->addAction(obs_module_text("Menu.PatchNotes"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::PatchNotesWindow::ShowPatchNotesWindow(); });

    // WebSocket Commands, while the WebSocket API is switched on.
    if (menuMod.websocketApi) {
        action = helpMenu->addAction(obs_module_text("Menu.WebSocket"));
        QObject::connect(action, &QAction::triggered, []() {
            // Hold Shift to show internal tools
            bool showInternalTools = QApplication::keyboardModifiers() & Qt::ShiftModifier;
            StreamUP::WebSocketWindow::ShowWebSocketWindow(showInternalTools);
        });
    }

    action = helpMenu->addAction(obs_module_text("Menu.StreamUPTheme"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::ThemeWindow::ShowThemeWindow(); });

    helpMenu->addSeparator();

    action = helpMenu->addAction(obs_module_text("Menu.About"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::SplashScreen::ShowSplashScreen(); });

    // Dev Tools submenu (only visible when Shift is held)
    if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
        menu->addSeparator();
        QMenu* devToolsMenu = menu->addMenu(obs_module_text("Menu.DevTools"));

        action = devToolsMenu->addAction(obs_module_text("FontUrlManager.Title"));
        QObject::connect(action, &QAction::triggered, []() {
            StreamUP::FileManager::ShowFontUrlManagerDialog();
        });
    }
}

} // namespace MenuManager  
} // namespace StreamUP
