#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QCheckBox>
#include <obs-data.h>

namespace StreamUP {
namespace SplashScreen {

// Splash screen conditions
enum class ShowCondition {
    FirstInstall,
    VersionUpdate,
    Never
};

// Show splash screen on first install or version update
void ShowSplashScreenIfNeeded();

// Show splash screen manually (for testing/menu access)
void ShowSplashScreen();

// Check if splash screen should be shown
ShowCondition CheckSplashCondition();

// Update version tracking after splash is shown
void UpdateVersionTracking();

// Internal dialog creation
void CreateSplashDialog(ShowCondition condition = ShowCondition::VersionUpdate);

} // namespace SplashScreen
} // namespace StreamUP