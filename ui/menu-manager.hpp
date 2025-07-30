#ifndef STREAMUP_MENU_MANAGER_HPP
#define STREAMUP_MENU_MANAGER_HPP

#include <QMenu>

namespace StreamUP {
namespace MenuManager {

/**
 * @brief Initialize and register the StreamUP menu system
 * 
 * Creates the main StreamUP menu and registers it with OBS Studio.
 * On Windows, adds it to the main menu bar.
 * On macOS and Linux, adds it to the Tools menu.
 */
void InitializeMenu();

/**
 * @brief Populate the StreamUP menu with current actions
 * @param menu The menu to populate with actions
 * 
 * Dynamically loads menu items based on platform and current state.
 * Called each time the menu is about to be shown.
 */
void LoadMenuItems(QMenu* menu);

/**
 * @brief Create and configure the Tools submenu
 * @param parentMenu The parent menu to attach the Tools submenu to
 * @return QMenu* Pointer to the created Tools submenu
 */
QMenu* CreateToolsSubmenu(QMenu* parentMenu);

} // namespace MenuManager
} // namespace StreamUP

#endif // STREAMUP_MENU_MANAGER_HPP