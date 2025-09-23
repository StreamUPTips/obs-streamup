#include "menu-manager.hpp"
#include "../utilities/debug-logger.hpp"
#include <obs.h>
#include "source-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"
#include "splash-screen.hpp"
#include "patch-notes-window.hpp"
#include "theme-window.hpp"
#include "websocket-window.hpp"
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
    blog(LOG_INFO, "[StreamUP] InitializeMenu: Starting menu initialization");

    blog(LOG_INFO, "[StreamUP] InitializeMenu: Creating QMenu");
    QMenu* menu = new QMenu();
    if (!menu) {
        blog(LOG_ERROR, "[StreamUP] InitializeMenu: Failed to create QMenu");
        return;
    }
    blog(LOG_INFO, "[StreamUP] InitializeMenu: QMenu created successfully");
    
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

    QMenu* topLevelMenu = new QMenu(obs_module_text("StreamUP"), menuBar);
    menuBar->addMenu(topLevelMenu);
    menu = topLevelMenu;
#else
    // macOS and Linux: Add to Tools menu
    blog(LOG_INFO, "[StreamUP] InitializeMenu: Mac/Linux platform - adding to Tools menu");

    blog(LOG_INFO, "[StreamUP] InitializeMenu: Getting menu text");
    const char* menu_text = obs_module_text("StreamUP");
    blog(LOG_INFO, "[StreamUP] InitializeMenu: Menu text: %s", menu_text ? menu_text : "NULL");

    blog(LOG_INFO, "[StreamUP] InitializeMenu: Calling obs_frontend_add_tools_menu_qaction");
    QAction* action = static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(menu_text));
    if (!action) {
        blog(LOG_ERROR, "[StreamUP] InitializeMenu: obs_frontend_add_tools_menu_qaction returned NULL");
        return;
    }
    blog(LOG_INFO, "[StreamUP] InitializeMenu: obs_frontend_add_tools_menu_qaction succeeded");

    blog(LOG_INFO, "[StreamUP] InitializeMenu: Setting menu on action");
    action->setMenu(menu);
    blog(LOG_INFO, "[StreamUP] InitializeMenu: Menu set on action successfully");
#endif

    // Connect dynamic loader
    blog(LOG_INFO, "[StreamUP] InitializeMenu: Connecting dynamic loader");
    QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenuItems(menu); });

    blog(LOG_INFO, "[StreamUP] InitializeMenu: Menu initialization completed successfully");
}

void LoadMenuItems(QMenu* menu)
{
    menu->clear();
    QAction* action;

    // Platform-specific actions (Windows only)
    if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
        action = menu->addAction(obs_module_text("Menu.Plugin.InstallProduct"));
        QObject::connect(action, &QAction::triggered, []() { 
            // Check if Shift is held to force load
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                StreamUP::FileManager::LoadStreamupFile(true);
            } else {
                StreamUP::FileManager::LoadStreamupFileWithWarning();
            }
        });
        
        action = menu->addAction(obs_module_text("Menu.Plugin.DownloadProduct"));
        QObject::connect(action, &QAction::triggered, []() { 
            QDesktopServices::openUrl(QUrl("https://streamup.tips/")); 
        });

        action = menu->addAction(obs_module_text("Menu.Plugin.CheckRequirements"));
        QObject::connect(action, &QAction::triggered, []() { 
            // Hold Shift to force refresh cache
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                StreamUP::PluginManager::PerformPluginCheckAndCache();
            }
            StreamUP::PluginManager::ShowCachedPluginIssuesDialog(); 
        });
        
        menu->addSeparator();
    }

    // Plugin updates (all platforms)
    action = menu->addAction(obs_module_text("Menu.Plugin.CheckUpdates"));
    QObject::connect(action, &QAction::triggered, []() { 
        // Hold Shift to force refresh cache
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            StreamUP::PluginManager::PerformPluginCheckAndCache();
        }
        StreamUP::PluginManager::ShowCachedPluginUpdatesDialog(); 
    });

    // Tools submenu
    QMenu* toolsMenu = menu->addMenu(obs_module_text("Menu.Tools"));
    
    // Source Management tools
    action = toolsMenu->addAction(obs_module_text("Menu.Source.LockAllSources"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::SourceManager::ToggleLockAllSources(); 
    });
    
    action = toolsMenu->addAction(obs_module_text("Menu.Source.LockCurrentSources"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::SourceManager::ToggleLockSourcesInCurrentScene(); 
    });
    
    toolsMenu->addSeparator();
    
    // Audio/Video tools
    action = toolsMenu->addAction(obs_module_text("Menu.Source.RefreshAudioMonitoring"));
    QObject::connect(action, &QAction::triggered, []() { 
        obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr); 
    });
    
    action = toolsMenu->addAction(obs_module_text("Menu.Source.RefreshBrowserSources"));
    QObject::connect(action, &QAction::triggered, []() { 
        obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr); 
    });
    
    // Video device management submenu
    QMenu* videoDeviceMenu = toolsMenu->addMenu(obs_module_text("Menu.VideoCapture.Root"));
    
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

    // MultiDock submenu
    QMenu* multiDockMenu = menu->addMenu("MultiDock");
    
    action = multiDockMenu->addAction("New MultiDock...");
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::MultiDock::ShowNewMultiDockDialog(); 
    });
    
    action = multiDockMenu->addAction("Manage MultiDocks...");
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::MultiDock::ShowManageMultiDocksDialog(); 
    });
    
    // Get the MultiDockManager instance and add existing multidocks to menu
    StreamUP::MultiDock::MultiDockManager* manager = StreamUP::MultiDock::MultiDockManager::Instance();
    if (manager) {
        QList<StreamUP::MultiDock::MultiDockInfo> multiDocks = manager->GetMultiDockInfoList();
        
        // Only add separator and multidocks if there are any
        if (!multiDocks.isEmpty()) {
            multiDockMenu->addSeparator();
            
            // Add each multidock as a checkable action
            for (const StreamUP::MultiDock::MultiDockInfo& info : multiDocks) {
                QAction* dockAction = multiDockMenu->addAction(info.name);
                dockAction->setCheckable(true);
                
                // Check if the multidock is currently visible
                bool isVisible = manager->IsMultiDockVisible(info.id);
                dockAction->setChecked(isVisible);
                
                // Connect to toggle visibility
                QObject::connect(dockAction, &QAction::triggered, [manager, info](bool checked) {
                    manager->SetMultiDockVisible(info.id, checked);
                });
            }
        }
    }

    menu->addSeparator();

    // Theme, WebSocket Commands, Settings, Patch Notes, About (in requested order)
    action = menu->addAction(obs_module_text("Menu.Theme"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::ThemeWindow::ShowThemeWindow(); });

    action = menu->addAction(obs_module_text("Menu.WebSocket"));
    QObject::connect(action, &QAction::triggered, []() { 
        // Check if Shift is held to show internal tools
        bool showInternalTools = QApplication::keyboardModifiers() & Qt::ShiftModifier;
        StreamUP::WebSocketWindow::ShowWebSocketWindow(showInternalTools); 
    });

    action = menu->addAction(obs_module_text("Menu.Settings"));
    QObject::connect(action, &QAction::triggered, []() { SettingsDialog(); });

    action = menu->addAction(obs_module_text("Menu.PatchNotes"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::PatchNotesWindow::ShowPatchNotesWindow(); });

    action = menu->addAction(obs_module_text("Menu.About"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::SplashScreen::ShowSplashScreen(); });
}


} // namespace MenuManager  
} // namespace StreamUP