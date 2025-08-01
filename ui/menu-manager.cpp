#include "menu-manager.hpp"
#include "source-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"
#include "splash-screen.hpp"
#include "tools-window.hpp"
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
            StreamUP::FileManager::LoadStreamupFile(QApplication::keyboardModifiers() & Qt::ShiftModifier); 
        });
        
        action = menu->addAction(obs_module_text("MenuDownloadProduct"));
        QObject::connect(action, &QAction::triggered, []() { 
            QDesktopServices::openUrl(QUrl("https://streamup.tips/")); 
        });

        action = menu->addAction(obs_module_text("MenuCheckRequirements"));
        QObject::connect(action, &QAction::triggered, []() { 
            StreamUP::PluginManager::CheckrequiredOBSPlugins(); 
        });
        
        menu->addSeparator();
    }

    // Plugin updates (all platforms)
    action = menu->addAction(obs_module_text("MenuCheckPluginUpdates"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::PluginManager::CheckAllPluginsForUpdates(true); 
    });

    // Tools window
    action = menu->addAction(obs_module_text("MenuTools"));
    QObject::connect(action, &QAction::triggered, []() { 
        StreamUP::ToolsWindow::ShowToolsWindow(); 
    });

    menu->addSeparator();

    // About and Settings
    action = menu->addAction(obs_module_text("MenuAbout"));
    QObject::connect(action, &QAction::triggered, []() { StreamUP::SplashScreen::ShowSplashScreen(); });

    action = menu->addAction(obs_module_text("MenuSettings"));
    QObject::connect(action, &QAction::triggered, []() { SettingsDialog(); });
}


} // namespace MenuManager  
} // namespace StreamUP