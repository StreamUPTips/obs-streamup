#ifndef STREAMUP_MULTIDOCK_UTILS_HPP
#define STREAMUP_MULTIDOCK_UTILS_HPP

#include <QMainWindow>
#include <QDockWidget>
#include <QList>
#include <QPointer>
#include <QString>

namespace StreamUP {
namespace MultiDock {

// Type definitions for Stage 1
using DockId = QString;

struct OriginalPlacement {
    QPointer<QMainWindow> main;
    Qt::DockWidgetArea area;
    bool wasFloating;
    QSize minimumSize;
    QSize maximumSize;
    QSize sizeHint;
    Qt::ContextMenuPolicy contextMenuPolicy;
};

struct CapturedDock {
    QPointer<QDockWidget> widget;
    OriginalPlacement original;
};

/**
 * @brief Get the main OBS window
 * @return Pointer to the main OBS window, or nullptr if not found
 */
QMainWindow* GetObsMainWindow();

/**
 * @brief Enumerate all available dock widgets in the OBS main window
 * @param mainWindow The main window to search in
 * @return List of all QDockWidget instances found
 */
QList<QDockWidget*> FindAllObsDocks(QMainWindow* mainWindow);

/**
 * @brief Generate a stable ID for a dock widget
 * @param dock The dock widget to generate ID for
 * @return Stable string identifier
 */
DockId GenerateDockId(QDockWidget* dock);

/**
 * @brief Check if a dock is a MultiDock container (to prevent self-capture)
 * @param dock The dock widget to check
 * @return True if this is a MultiDock container
 */
bool IsMultiDockContainer(QDockWidget* dock);

// Helper functions for cleaner dock identification
bool IsQuickAccessUtilityDock(QDockWidget* dock);
QString GetQuickAccessStableId(QDockWidget* dock);
bool IsStableObjectName(const QString& objectName);
QString CreateFallbackId(QDockWidget* dock);

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_UTILS_HPP