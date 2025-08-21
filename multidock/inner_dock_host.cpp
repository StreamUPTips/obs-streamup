#include "inner_dock_host.hpp"
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
    , m_currentDock(nullptr)
{
    SetupDockOptions();
    // Toolbar is now created by MultiDockDock instead
    
    // No timers - we save on OBS shutdown and update UI immediately when needed
    
    blog(LOG_INFO, "[StreamUP MultiDock] Created InnerDockHost for '%s'", 
         m_multiDockId.toUtf8().constData());
}

InnerDockHost::~InnerDockHost()
{
    blog(LOG_INFO, "[StreamUP MultiDock] Destroying InnerDockHost for '%s'", 
         m_multiDockId.toUtf8().constData());
}


void InnerDockHost::SetupDockOptions()
{
    setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::South);
    
    // Create a central widget for the docking area
    // Margins are now handled by the parent MultiDockDock
    QWidget* centralWidget = new QWidget();
    centralWidget->setContentsMargins(0, 0, 0, 0);
    setCentralWidget(centralWidget);
    
    // Set window flags to prevent overlap with parent title
    setWindowFlags(Qt::Widget);
    
    // Set darker background only for the main window background (empty areas)
    // Style dock separators and margins to use the same color
    QString bgColor = "#0d0d0d";
    setStyleSheet(QString(
        "InnerDockHost { background-color: %1; }"
        "QMainWindow { background-color: %1; }"
        "QMainWindow::separator { background-color: %1; width: 6px; height: 6px; }"
        "QSplitter::handle { background-color: %1; }"
        "QTabWidget::pane { border: none; margin: 3px; }"
        "QDockWidget { background-color: transparent; }"
    ).arg(bgColor));
}

void InnerDockHost::SetupToolBar()
{
    // Make sure we're starting clean - but do it safely
    QList<QToolBar*> existingToolBars = findChildren<QToolBar*>();
    for (QToolBar* tb : existingToolBars) {
        if (tb && tb->parent() == this) {
            tb->setParent(nullptr);
            removeToolBar(tb);
            tb->deleteLater();
        }
    }
    
    m_toolBar = new QToolBar("MultiDock Controls", this);
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolBar->setObjectName("MultiDockToolBar");
    
    // Explicitly add to bottom toolbar area
    addToolBar(Qt::BottomToolBarArea, m_toolBar);
    
    // Make sure it stays at the bottom
    insertToolBarBreak(m_toolBar);
    
    // Add Dock action with icon
    m_addDockAction = m_toolBar->addAction("➕ Add Dock");
    m_addDockAction->setToolTip("Add an OBS dock to this MultiDock");
    connect(m_addDockAction, &QAction::triggered, this, &InnerDockHost::ShowAddDockDialog);
    
    m_toolBar->addSeparator();
    
    // Return Dock action with icon
    m_returnDockAction = m_toolBar->addAction("↩ Return Dock");
    m_returnDockAction->setToolTip("Return the selected dock to the main OBS window");
    connect(m_returnDockAction, &QAction::triggered, this, &InnerDockHost::ReturnCurrentDock);
    
    // Close Dock action with icon
    m_closeDockAction = m_toolBar->addAction("✖ Close Dock");
    m_closeDockAction->setToolTip("Hide the selected dock (keeps it in MultiDock)");
    connect(m_closeDockAction, &QAction::triggered, this, &InnerDockHost::CloseCurrentDock);
    
    // Add a stretch to push buttons to the left
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spacer);
    
    // Add status label
    m_statusLabel = new QLabel("No docks captured");
    m_statusLabel->setObjectName("MultiDockStatusLabel");
    m_toolBar->addWidget(m_statusLabel);
}

void InnerDockHost::AddDock(QDockWidget* dock, Qt::DockWidgetArea area)
{
    if (!dock) {
        return;
    }
    
    DockId dockId = GenerateDockId(dock);
    if (m_capturedDocks.contains(dockId)) {
        blog(LOG_WARNING, "[StreamUP MultiDock] Dock '%s' is already captured", 
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
    
    // Set as current dock and make it visible/focused
    m_currentDock = dock;
    dock->show();
    dock->raise();
    
    // Update toolbar state
    UpdateToolBarState();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Added dock '%s' to MultiDock '%s'", 
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
    
    // Return to original parent if possible
    if (captured.original.main) {
        captured.original.main->addDockWidget(captured.original.area, dock);
        dock->setFloating(captured.original.wasFloating);
    }
    
    // Remove from captured list
    m_capturedDocks.remove(dockId);
    
    // Update current dock
    if (m_currentDock == dock) {
        m_currentDock = nullptr;
        // Try to find another dock to select
        QList<QDockWidget*> docks = GetAllDocks();
        if (!docks.isEmpty()) {
            m_currentDock = docks.first();
        }
    }
    
    // Update toolbar state
    UpdateToolBarState();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Removed dock '%s' from MultiDock '%s'", 
         dock->windowTitle().toUtf8().constData(), m_multiDockId.toUtf8().constData());
}

QDockWidget* InnerDockHost::GetCurrentDock() const
{
    return m_currentDock;
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
            }
        }
        
        // Update toolbar state after restoration
        UpdateToolBarState();
        
        blog(LOG_INFO, "[StreamUP MultiDock] Restored layout for MultiDock '%s'", 
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

void InnerDockHost::ReturnCurrentDock()
{
    if (m_currentDock) {
        RemoveDock(m_currentDock);
    }
}

void InnerDockHost::CloseCurrentDock()
{
    if (m_currentDock) {
        m_currentDock->hide();
        UpdateToolBarState();
    }
}

void InnerDockHost::OnTabifiedDockActivated(QDockWidget* dock)
{
    m_currentDock = dock;
    UpdateToolBarState();
}

bool InnerDockHost::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || 
        event->type() == QEvent::FocusIn ||
        event->type() == QEvent::WindowActivate) {
        
        // Find which dock widget this object belongs to
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget) {
            QDockWidget* dock = nullptr;
            QWidget* parent = widget;
            
            // Walk up the parent chain to find the QDockWidget
            while (parent && !dock) {
                dock = qobject_cast<QDockWidget*>(parent);
                parent = parent->parentWidget();
            }
            
            if (dock && m_capturedDocks.contains(GenerateDockId(dock))) {
                m_currentDock = dock;
                UpdateToolBarState();
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
    
    // Track tab activation for current dock selection
    connect(this, &QMainWindow::tabifiedDockWidgetActivated, this, &InnerDockHost::OnTabifiedDockActivated);
    
    // Connect to dock widget's focus/visibility changes
    connect(dock, &QDockWidget::visibilityChanged, this, [this, dock](bool visible) {
        if (visible && dock->isActiveWindow()) {
            m_currentDock = dock;
        }
        UpdateToolBarState(); // Update toolbar when visibility changes
    });
    
    // Track when dock becomes active/focused by installing an event filter
    if (dock->widget()) {
        dock->widget()->installEventFilter(this);
    }
}

void InnerDockHost::DisconnectDockSignals(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    // Remove event filter from the dock's widget
    if (dock->widget()) {
        dock->widget()->removeEventFilter(this);
    }
    
    // Qt handles the rest of the signal disconnections automatically
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
    
    // Fallback: Legacy toolbar update for old toolbar (if any)
    if (!m_returnDockAction || !m_closeDockAction) {
        return; // No toolbar actions available
    }
    
    int dockCount = m_capturedDocks.size();
    bool hasCurrentDock = m_currentDock != nullptr;
    
    // Update action states
    m_returnDockAction->setEnabled(hasCurrentDock);
    m_closeDockAction->setEnabled(hasCurrentDock && m_currentDock && m_currentDock->isVisible());
    
    // Update status label
    if (m_statusLabel) {
        if (dockCount == 0) {
            m_statusLabel->setText("No docks captured");
        } else if (dockCount == 1) {
            m_statusLabel->setText("1 dock captured");
        } else {
            m_statusLabel->setText(QString("%1 docks captured").arg(dockCount));
        }
        
        // Add current selection info if available
        if (hasCurrentDock) {
            QString currentTitle = m_currentDock->windowTitle();
            if (!currentTitle.isEmpty()) {
                m_statusLabel->setText(m_statusLabel->text() + 
                                     QString(" | Selected: %1").arg(currentTitle));
            }
        }
    }
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
                blog(LOG_INFO, "[StreamUP MultiDock] Hidden toolbar in captured dock: %s", 
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
                blog(LOG_INFO, "[StreamUP MultiDock] Restored toolbar in returned dock: %s", 
                     toolBar->objectName().toUtf8().constData());
            }
        }
    }
}



} // namespace MultiDock
} // namespace StreamUP

#include "inner_dock_host.moc"
