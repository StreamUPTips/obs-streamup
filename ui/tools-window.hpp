#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QWidget>
#include <QScrollArea>
#include <functional>

namespace StreamUP {
namespace ToolsWindow {

/**
 * @brief Shows the StreamUP Tools window with all available tools organized by category
 */
void ShowToolsWindow();

/**
 * @brief Creates a tool button with modern styling and connects to the specified action
 * @param title The display name of the tool
 * @param description Brief description of what the tool does
 * @param action The function to execute when the tool is clicked
 * @return QPushButton* The created and styled button
 */
QPushButton* CreateToolButton(const QString& title, const QString& description, std::function<void()> action);

/**
 * @brief Shows video device management options within the current dialog
 * @param parentDialog The parent dialog to show options in
 */
void ShowVideoDeviceOptions(QDialog* parentDialog);

/**
 * @brief Shows video device management options inline within the same window
 * @param scrollArea The scroll area to replace content in
 * @param originalContent The original content widget to restore later
 */
void ShowVideoDeviceOptionsInline(QScrollArea* scrollArea, QWidget* originalContent);

/**
 * @brief Shows tool detail view inline within the same window
 * @param scrollArea The scroll area to replace content in
 * @param originalContent The original content widget to restore later
 * @param titleKey The localization key for the tool title
 * @param info1Key Info text 1 localization key
 * @param info2Key Info text 2 localization key
 * @param info3Key Info text 3 localization key
 * @param action The function to execute when the tool is activated
 * @param howTo1Key How-to step 1 localization key
 * @param howTo2Key How-to step 2 localization key
 * @param howTo3Key How-to step 3 localization key
 * @param howTo4Key How-to step 4 localization key
 */
void ShowToolDetailInline(QScrollArea* scrollArea, QWidget* originalContent, const char* titleKey,
                         const char* info1Key, const char* info2Key, const char* info3Key,
                         std::function<void()> action, const char* howTo1Key, const char* howTo2Key,
                         const char* howTo3Key, const char* howTo4Key, const char* websocketCommand = nullptr);


} // namespace ToolsWindow
} // namespace StreamUP