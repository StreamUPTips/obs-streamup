#ifndef STREAMUP_MULTIDOCK_UTILS_HPP
#define STREAMUP_MULTIDOCK_UTILS_HPP

#include <QMainWindow>
#include <QDockWidget>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QSize>

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
    QVariant classProperty; // Store original "class" property for theme styling
};

struct CapturedDock {
    QPointer<QDockWidget> widget;
    OriginalPlacement original;

    // A captured dock's floor is set by its content's LAYOUT, not by the dock,
    // so relaxing the dock alone leaves it as wide as the content's layout says
    // it must be. The constraint is lifted while the dock lives in a MultiDock
    // and put back when it leaves. Null/unset for a dock whose content has no
    // layout of its own.
    QPointer<QWidget> contentWidget;
    QSize contentMinimumSize;
    int contentSizeConstraint = -1; // QLayout::SizeConstraint, -1 when untouched
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

/**
 * @brief List the dock object names referenced by a saved QMainWindow state
 *
 * QMainWindow::restoreState() keeps an entry for every dock name in the saved
 * state, including ones that are never re-added. Those become zero-size
 * placeholders that occupy slots in the layout tree and stop drag-and-drop and
 * tabifying from working in that window. There is no Qt API to enumerate or
 * drop them, so this walks the serialised stream looking for the QDataStream
 * QString fields the format is built from.
 *
 * This is a heuristic read of a private format and is only ever used to decide
 * whether a layout is safe to restore - a false positive costs one default
 * re-layout, never data.
 *
 * @param layout Saved state as produced by QMainWindow::saveState()
 * @return Object names found in the stream
 */
QStringList ExtractDockNamesFromLayout(const QByteArray& layout);

// Helper functions for cleaner dock identification
bool IsQuickAccessUtilityDock(QDockWidget* dock);
QString GetQuickAccessStableId(QDockWidget* dock);
bool IsStableObjectName(const QString& objectName);
QString CreateFallbackId(QDockWidget* dock);

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_UTILS_HPP
