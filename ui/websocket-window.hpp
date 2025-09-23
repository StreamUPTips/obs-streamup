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
namespace WebSocketWindow {

/**
 * @brief Shows the StreamUP WebSocket documentation window with all available commands
 * @param showInternalTools Whether to show internal developer tools (requires Shift key)
 */
void ShowWebSocketWindow(bool showInternalTools = false);

/**
 * @brief Creates a command display item with copy buttons for OBS Raw and CPH formats
 * @param command The WebSocket command name
 * @param description Brief description of what the command does
 * @return QWidget* The created command widget
 */
QWidget* CreateCommandWidget(const QString& command, const QString& description);


} // namespace WebSocketWindow
} // namespace StreamUP
