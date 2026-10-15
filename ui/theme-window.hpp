#pragma once

#include <QDialog>
#include <QPointer>

namespace StreamUP {
namespace ThemeWindow {

// Show theme window
void ShowThemeWindow();

// The theme screenshot carousel from the theme window, for use elsewhere.
// Height is in unscaled pixels.
QWidget* CreateThemePreviewCarousel(int height, QWidget* parent = nullptr);

// Check if theme window is currently open
bool IsThemeWindowOpen();

// Internal dialog creation
void CreateThemeDialog();

} // namespace ThemeWindow
} // namespace StreamUP
