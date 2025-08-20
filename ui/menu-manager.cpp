#include "menu-manager.hpp"
#include "source-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"
#include "splash-screen.hpp"
#include "patch-notes-window.hpp"
#include "theme-window.hpp"
#include "websocket-window.hpp"
#include "../multidock/multidock_dialogs.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QAction>
#include <QDesktopServices>
#include <QUrl>

// Platform detection
#if defined(_WIN32)
#define PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define PLATFORM_NAME "macos"  
#elif defined(__linux__)
#define PLATFORM_NAME "linux"
#else
#define PLATFORM_NAME "unknown"
#endif

// Forward declarations for dialog functions (defined in main streamup.cpp for now)
extern void SettingsDialog();

namespace StreamUP {
namespace MenuManager {

void InitializeMenu()
{
    QMenu* menu = new QMenu();
    
#if defined(_WIN32)
    // Windows: Add to main menu bar
    void* main_window_ptr = obs_frontend_get_main_window();
    if (!main_window_ptr) {
        blog(LOG_ERROR, "[StreamUP] Could not find main window");
        return;
    }

    QMainWindow* main_window = static_cast<QMainWindow*>(main_window_ptr);
    QMenuBar* menuBar = main_window->menuBar();
    if (!menuBar) {
        blog(LOG_ERROR, "[StreamUP] Could not find main menu bar");
        return;
    }

    QMenu* topLevelMenu = new QMenu(obs_module_text("StreamUP"), menuBar);
    menuBar->addMenu(topLevelMenu);
    menu = topLevelMenu;
#else
    // macOS and Linux: Add to Tools menu
    QAction* action = static_cast<QAction*>(obs_frontend_add_tools_menu_qaction(obs_module_text("StreamUP")));
    action->setMenu(menu);
#endif

    // Connect dynamic loader
    QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenuItems(menu); });
}

void LoadMenuItems(QMenu* menu)
{
    menu->clear();
    QAction* action;

    // Platform-specific actions (Windows only)
    if (strcmp(PLATFORM_NAME, "windows") == 0) {
        action = menu->addAction(obs_module_text("MenuInstallProduct"));
        QObject::connect(action, &QAction::triggered, []() { 
            // Check if Shift is held to force load
            if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
                StreamUP::FileManager::LoadStreamupFile(true);
            } else {
                StreamUP::FileManager::LoadStreamupFileWithWarning();
            }
        });
        
        action = menu->addAction(obs_module_text("MenuDownloadProduct"));
        QObject::connect(action, &QAction::triggered, []() { 
            QDesktopServices::openUrl(QUrl("https://streamup.tips/")); 
        });

        action = menu->addAction(obs_module_text("MenuCheckRequirements"));
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
    action = menu->addAction(obs_module_text("MenuCheckPluginUpdates"));
    QObject::connect(action, &QAction::triggered, []() { 
        // Hold Shift to force refresh cache
        if (QApplication::keyboardModifiers() & Qt::ShiftModifier) {
            StreamUP::PluginManager::PerformPluginCheckAndCache();
        }
        StreamUP::PluginManager::ShowCachedPluginUpdatesDialog(); 
    });

    // Tools submenu
    QMenu* toolsMenu = menu->addMenu(obs_module_text("MenuTools"));
    
    // Source Management tools
    action = toolsMenu->addAction(obs_module_text("MenuLockAllSources"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::SourceManager::ToggleLockAllSources(); 
    });
    
    action = toolsMenu->addAction(obs_module_text("MenuLockAllCurrentSources"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::SourceManager::ToggleLockSourcesInCurrentScene(); 
    });
    
    toolsMenu->addSeparator();
    
    // Audio/Video tools
    action = toolsMenu->addAction(obs_module_text("MenuRefreshAudioMonitoring"));
    QObject::connect(action, &QAction::triggered, []() { 
        obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr); 
    });
    
    action = toolsMenu->addAction(obs_module_text("MenuRefreshBrowserSources"));
    QObject::connect(action, &QAction::triggered, []() { 
        obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr); 
    });
    
    // Video device management submenu
    QMenu* videoDeviceMenu = toolsMenu->addMenu(obs_module_text("MenuVideoCaptureDevices"));
    
    action = videoDeviceMenu->addAction(obs_module_text("MenuActivateVideoCaptureDevices"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::SourceManager::ActivateAllVideoCaptureDevices(); 
    });
    
    action = videoDeviceMenu->addAction(obs_module_text("MenuDeactivateVideoCaptureDevices"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::SourceManager::DeactivateAllVideoCaptureDevices(); 
    });
    
    action = videoDeviceMenu->addAction(obs_module_text("MenuRefreshVideoCaptureDevices"));
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

    menu->addSeparator();

    // Theme, WebSocket Commands, Settings, Patch Notes, About (in requested order)
    action = menu->addAction(obs_module_text("MenuTheme"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::ThemeWindow::ShowThemeWindow(); });

    action = menu->addAction(obs_module_text("MenuWebSocket"));
    QObject::connect(action, &QAction::triggered, []() { 
        // Check if Shift is held to show internal tools
        bool showInternalTools = QApplication::keyboardModifiers() & Qt::ShiftModifier;
        StreamUP::WebSocketWindow::ShowWebSocketWindow(showInternalTools); 
    });

    action = menu->addAction(obs_module_text("MenuSettings"));
    QObject::connect(action, &QAction::triggered, []() { SettingsDialog(); });

    action = menu->addAction(obs_module_text("MenuPatchNotes"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::PatchNotesWindow::ShowPatchNotesWindow(); });

    action = menu->addAction(obs_module_text("MenuAbout"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::SplashScreen::ShowSplashScreen(); });
}


} // namespace MenuManager  
} // namespace StreamUP