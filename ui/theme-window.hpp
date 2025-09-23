#pragma once

#include <QDialog>
#include <QPointer>

namespace StreamUP {
namespace ThemeWindow {

// Show theme window
void ShowThemeWindow();

// Check if theme window is currently open
bool IsThemeWindowOpen();

// Internal dialog creation
void CreateThemeDialog();

} // namespace ThemeWindow
} // namespace StreamUP
