#include "inner_dock_host.hpp"
#include "../utilities/debug-logger.hpp"
#include "multidock_dock.hpp"
#include "add_dock_dialog.hpp"
#include "persistence.hpp"
#include "../ui/ui-styles.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QSizePolicy>
#include <QEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QHBoxLayout>

namespace StreamUP {
namespace MultiDock {

InnerDockHost::InnerDockHost(const QString& multiDockId, QWidget* parent)
    : QMainWindow(parent)
    , m_multiDockId(multiDockId)
    , m_toolBar(nullptr)
    , m_addDockAction(nullptr)
    , m_returnDockAction(nullptr)
    , m_closeDockAction(nullptr)
    , m_statusLabel(nullptr)
    , m_docksLocked(false)
{
    SetupDockOptions();
    // Toolbar is now created by MultiDockDock instead
    
    // No timers - we save on OBS shutdown and update UI immediately when needed
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Host Creation", "Created InnerDockHost for '%s'", 
         m_multiDockId.toUtf8().constData());
}

InnerDockHost::~InnerDockHost()
{
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Host Destruction", "Destroying InnerDockHost for '%s'", 
         m_multiDockId.toUtf8().constData());
}


void InnerDockHost::SetupDockOptions()
{
    setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::South);
    
    // Don't create a central widget - let docks fill the entire space
    // This allows dock widgets to use all available space
    
    // Set window flags to prevent overlap with parent title
    setWindowFlags(Qt::Widget);
    
    // Set darker background only for the main window background (empty areas)
    // Style dock separators and margins to use the same color with 12px spacing
    QString bgColor = "#0d0d0d";
    setStyleSheet(QString(
        "InnerDockHost { background-color: %1; }"
        "QMainWindow { background-color: %1; }"
        "QMainWindow::separator { background-color: %1; width: 12px; height: 12px; }"
        "QSplitter::handle { background-color: %1; width: 12px; height: 12px; }"
        "QTabWidget::pane { border: none; margin: 6px; }"
        "QDockWidget { background-color: transparent; }"
        "QDockWidget::title { background-color: #161617; text-align: left; padding-left: 8px; }"
        "QDockWidget::close-button { subcontrol-position: top right; right: 10px; top: 4px; }"
        "QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
        "InnerDockHost QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
        "InnerDockHost > QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
    ).arg(bgColor));
}


void InnerDockHost::AddDock(QDockWidget* dock, Qt::DockWidgetArea area)
{
    if (!dock) {
        return;
    }
    
    DockId dockId = GenerateDockId(dock);
    if (m_capturedDocks.contains(dockId)) {
        StreamUP::DebugLogger::LogWarningFormat("MultiDock", "Add Dock", "Dock '%s' is already captured",
             dockId.toUtf8().constData());
        return;
    }
    
    // Record original placement and size information
    OriginalPlacement original;
    original.main = qobject_cast<QMainWindow*>(dock->parent());
    if (original.main) {
        original.area = original.main->dockWidgetArea(dock);
        original.wasFloating = dock->isFloating();
    } else {
        original.area = Qt::RightDockWidgetArea;
        original.wasFloating = false;
    }
    
    // Store original size constraints
    original.minimumSize = dock->minimumSize();
    original.maximumSize = dock->maximumSize();
    original.sizeHint = dock->sizeHint();
    
    // Store original context menu policy
    original.contextMenuPolicy = dock->contextMenuPolicy();
    
    // Store captured dock info
    CapturedDock captured;
    captured.widget = dock;
    captured.original = original;
    m_capturedDocks[dockId] = captured;
    
    // Move dock to this host - let it behave naturally
    addDockWidget(area, dock);
    ConnectDockSignals(dock);
    
    // Keep dock completely original - no custom modifications
    
    // Don't hide toolbars - users expect to see them
    
    // Ensure proper positioning and prevent overlap with parent
    dock->setFloating(false);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    
    // Set dock features based on lock state
    if (m_docksLocked) {
        dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    } else {
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    }
    
    // Make dock fill available space
    dock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (dock->widget()) {
        dock->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    
    // Make dock visible
    dock->show();
    dock->raise();
    
    // Update toolbar state
    UpdateToolBarState();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dock Management", "Added dock '%s' to MultiDock '%s'", 
         dock->windowTitle().toUtf8().constData(), m_multiDockId.toUtf8().constData());
}

void InnerDockHost::RemoveDock(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    DockId dockId = GenerateDockId(dock);
    if (!m_capturedDocks.contains(dockId)) {
        return;
    }
    
    CapturedDock captured = m_capturedDocks[dockId];
    DisconnectDockSignals(dock);
    
    // Restore original size constraints
    dock->setMinimumSize(captured.original.minimumSize);
    dock->setMaximumSize(captured.original.maximumSize);
    
    // Toolbars were not hidden, so no need to restore them
    
    // No custom modifications were made, so nothing to remove
    
    // Remove from this host
    removeDockWidget(dock);
    
    // Restore original settings
    dock->setContextMenuPolicy(captured.original.contextMenuPolicy);
    
    // Return to original parent if possible
    if (captured.original.main) {
        captured.original.main->addDockWidget(captured.original.area, dock);
        dock->setFloating(captured.original.wasFloating);
    }
    
    // Remove from captured list
    m_capturedDocks.remove(dockId);
    
    // Update toolbar state
    UpdateToolBarState();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dock Management", "Removed dock '%s' from MultiDock '%s'", 
         dock->windowTitle().toUtf8().constData(), m_multiDockId.toUtf8().constData());
}


QList<QDockWidget*> InnerDockHost::GetAllDocks() const
{
    QList<QDockWidget*> result;
    for (const CapturedDock& captured : m_capturedDocks) {
        if (captured.widget) {
            result.append(captured.widget);
        }
    }
    return result;
}

void InnerDockHost::RestoreLayout(const QByteArray& layout)
{
    if (!layout.isEmpty()) {
        // Skip toolbar-related restoration to avoid crashes
        // Only restore dock positions and geometry
        restoreState(layout);
        
        // Clean up any toolbars that might have been restored, but do it safely
        QList<QToolBar*> toolBars = findChildren<QToolBar*>();
        for (QToolBar* tb : toolBars) {
            if (tb && (tb->objectName() == "MultiDockToolBar" || tb->parent() == this)) {
                // Safely remove toolbar without immediate deletion
                tb->hide();
                tb->setVisible(false);
                removeToolBar(tb);
                // Don't delete immediately - just hide and remove from layout
                tb->setParent(nullptr);
            }
        }
        
        // Fix any dock positioning issues after restoration
        QList<QDockWidget*> docks = GetAllDocks();
        for (QDockWidget* dock : docks) {
            if (dock) {
                // Ensure docks are properly positioned and visible
                dock->show();
                dock->raise();
                // Reset any size constraints that might cause overlap
                dock->setFloating(false);
                
                // Reapply dock features to prevent popout buttons from reappearing
                dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
                dock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                if (dock->widget()) {
                    dock->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                }
            }
        }
        
        // Update toolbar state after restoration
        UpdateToolBarState();
        
        // Reapply dock features immediately after layout restoration
        ReapplyDockFeatures();
        
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Layout Restore", "Restored layout for MultiDock '%s'", 
             m_multiDockId.toUtf8().constData());
    }
}

QByteArray InnerDockHost::SaveLayout() const
{
    // Just save the state as is - we'll handle cleanup during restore
    // This avoids the complex const_cast operations that could cause issues
    return saveState();
}

QStringList InnerDockHost::GetCapturedDockIds() const
{
    return m_capturedDocks.keys();
}

void InnerDockHost::ShowAddDockDialog()
{
    AddDockDialog dialog(m_multiDockId, this);
    if (dialog.exec() == QDialog::Accepted) {
        QDockWidget* selectedDock = dialog.GetSelectedDock();
        if (selectedDock) {
            AddDock(selectedDock);
        }
    }
}




bool InnerDockHost::eventFilter(QObject* obj, QEvent* event)
{
    // Handle close events for dock widgets to return them instead of just hiding
    if (event->type() == QEvent::Close) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && m_capturedDocks.contains(GenerateDockId(dock))) {
            // Remove the dock from this MultiDock and return it to main window
            RemoveDock(dock);
            return true; // Event handled, don't let the dock close normally
        }
    }
    
    // Block resize-related events when docks are locked
    if (m_docksLocked) {
        if (event->type() == QEvent::MouseMove ||
            event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonRelease) {
            
            // Check if this is on a splitter or separator
            QWidget* widget = qobject_cast<QWidget*>(obj);
            if (widget) {
                QString className = widget->metaObject()->className();
                if (className.contains("Splitter") || className.contains("Separator")) {
                    return true; // Block the event
                }
            }
        }
        
        // Block cursor change events that show resize cursor
        if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            QWidget* widget = qobject_cast<QWidget*>(obj);
            if (widget) {
                QString className = widget->metaObject()->className();
                if (className.contains("Splitter") || className.contains("Separator")) {
                    widget->setCursor(Qt::ArrowCursor);
                    return true;
                }
            }
        }
    }
    
    
    return QMainWindow::eventFilter(obj, event);
}

void InnerDockHost::ConnectDockSignals(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    // Connect to dock widget's visibility changes to update toolbar
    connect(dock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        UpdateToolBarState(); // Update toolbar when visibility changes
    });
    
    // Override dock's close behavior to return dock to main window instead of just hiding
    // We need to install an event filter to catch close events
    dock->installEventFilter(this);
    
    // Disable context menu to prevent show/hide options
    dock->setContextMenuPolicy(Qt::NoContextMenu);
}

void InnerDockHost::DisconnectDockSignals(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    // Remove event filter
    dock->removeEventFilter(this);
    
    // Qt handles signal disconnections automatically when objects are destroyed
}

void InnerDockHost::UpdateToolBarState()
{
    // Safety check to prevent crashes during shutdown or destruction
    if (!parent()) {
        return;
    }
    
    // Find the parent MultiDockDock and call its UpdateToolbarState method
    MultiDockDock* parentDock = qobject_cast<MultiDockDock*>(parent());
    if (parentDock) {
        parentDock->UpdateToolbarState();
        return;
    }
    
    // Legacy toolbar no longer used - MultiDockDock handles toolbar now
}

void InnerDockHost::HideDockToolBars(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    // Find any QToolBar widgets inside the captured dock and hide them
    // This prevents duplicate toolbars (e.g., mixer controls) from appearing
    QWidget* dockWidget = dock->widget();
    if (dockWidget) {
        QList<QToolBar*> toolBars = dockWidget->findChildren<QToolBar*>();
        for (QToolBar* toolBar : toolBars) {
            if (toolBar) {
                toolBar->hide();
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Toolbar Management", "Hidden toolbar in captured dock: %s", 
                     toolBar->objectName().toUtf8().constData());
            }
        }
    }
}

void InnerDockHost::RestoreDockToolBars(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    // Find any QToolBar widgets inside the dock and show them again
    QWidget* dockWidget = dock->widget();
    if (dockWidget) {
        QList<QToolBar*> toolBars = dockWidget->findChildren<QToolBar*>();
        for (QToolBar* toolBar : toolBars) {
            if (toolBar) {
                toolBar->show();
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Toolbar Management", "Restored toolbar in returned dock: %s", 
                     toolBar->objectName().toUtf8().constData());
            }
        }
    }
}


void InnerDockHost::ReapplyDockFeatures()
{
    // Reapply stylesheet first to ensure close buttons show/hide correctly but keep 12px padding
    QString bgColor = "#0d0d0d";
    QString closeButtonStyle = m_docksLocked ? "width: 0px; height: 0px; subcontrol-position: top right; right: -20px;" : "";
    // When locked, disable separator interaction but keep visual spacing
    QString separatorStyle = m_docksLocked ? 
        "background-color: %1; width: 12px; height: 12px; border: none;" :
        "background-color: %1; width: 12px; height: 12px;";
    QString splitterStyle = m_docksLocked ? 
        "background-color: %1; width: 12px; height: 12px; border: none;" :
        "background-color: %1; width: 12px; height: 12px;";
    
    setStyleSheet(QString(
        "InnerDockHost { background-color: %1; }"
        "QMainWindow { background-color: %1; }"
        "QMainWindow::separator { " + separatorStyle + " }"
        "QSplitter::handle { " + splitterStyle + " }"
        "QTabWidget::pane { border: none; margin: 6px; }"
        "QDockWidget { background-color: transparent; }"
        "QDockWidget::title { background-color: #161617; text-align: left; padding-left: 8px; }"
        "QDockWidget::close-button { subcontrol-position: top right; right: 10px; top: 4px; %2 }"
        "QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
        "InnerDockHost QDockWidget::close-button { %2 }"
        "InnerDockHost > QDockWidget::close-button { %2 }"
        "InnerDockHost QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
        "InnerDockHost > QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
    ).arg(bgColor).arg(closeButtonStyle));
    
    // Reapply dock options based on lock state
    if (m_docksLocked) {
        setDockOptions(AllowTabbedDocks | ForceTabbedDocks);
        // Install event filter to block resize events when locked
        installEventFilter(this);
    } else {
        setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
        // Remove event filter when unlocked
        removeEventFilter(this);
    }
    
    QList<QDockWidget*> docks = GetAllDocks();
    for (QDockWidget* dock : docks) {
        if (dock) {
            // Reapply dock features based on lock state
            if (m_docksLocked) {
                dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
            } else {
                dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
            }
            dock->setFloating(false);
            dock->setAllowedAreas(Qt::AllDockWidgetAreas);
            dock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            if (dock->widget()) {
                dock->widget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            }
            
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Feature Management", "Reapplied features to dock '%s' (locked: %s)", 
                 dock->windowTitle().toUtf8().constData(), m_docksLocked ? "yes" : "no");
        }
    }
}

void InnerDockHost::SetDocksLocked(bool locked)
{
    m_docksLocked = locked;
    
    // Update stylesheet to show/hide close buttons but keep 12px padding when locked
    QString bgColor = "#0d0d0d";
    QString closeButtonStyle = m_docksLocked ? "width: 0px; height: 0px; subcontrol-position: top right; right: -20px;" : "";
    // When locked, disable separator interaction but keep visual spacing
    QString separatorStyle = m_docksLocked ? 
        "background-color: %1; width: 12px; height: 12px; border: none;" :
        "background-color: %1; width: 12px; height: 12px;";
    QString splitterStyle = m_docksLocked ? 
        "background-color: %1; width: 12px; height: 12px; border: none;" :
        "background-color: %1; width: 12px; height: 12px;";
    
    setStyleSheet(QString(
        "InnerDockHost { background-color: %1; }"
        "QMainWindow { background-color: %1; }"
        "QMainWindow::separator { " + separatorStyle + " }"
        "QSplitter::handle { " + splitterStyle + " }"
        "QTabWidget::pane { border: none; margin: 6px; }"
        "QDockWidget { background-color: transparent; border: none; }"
        "QDockWidget::title { background-color: #161617; text-align: left; padding-left: 8px; }"
        "QDockWidget::close-button { subcontrol-position: top right; right: 10px; top: 4px; %2 }"
        "QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
        "InnerDockHost QDockWidget::close-button { %2 }"
        "InnerDockHost > QDockWidget::close-button { %2 }"
        "InnerDockHost QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
        "InnerDockHost > QDockWidget::float-button { width: 0px; height: 0px; subcontrol-position: top right; right: -20px; }"
    ).arg(bgColor).arg(closeButtonStyle));
    
    // Disable/enable dock resizing by setting dock options
    if (m_docksLocked) {
        setDockOptions(AllowTabbedDocks | ForceTabbedDocks);
        // Install event filter to block resize events when locked
        installEventFilter(this);
    } else {
        setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
        // Remove event filter when unlocked
        removeEventFilter(this);
    }
    
    // Apply lock state to all current docks
    QList<QDockWidget*> docks = GetAllDocks();
    for (QDockWidget* dock : docks) {
        if (dock) {
            if (m_docksLocked) {
                // When locked: remove all features except title bar
                dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
            } else {
                // When unlocked: allow moving and closing
                dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
            }
            
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Lock Management", "%s dock '%s'", 
                 m_docksLocked ? "Locked" : "Unlocked", 
                 dock->windowTitle().toUtf8().constData());
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Lock Management", "Set docks locked state to: %s", 
         m_docksLocked ? "locked" : "unlocked");
}



} // namespace MultiDock
} // namespace StreamUP

#include "inner_dock_host.moc"
