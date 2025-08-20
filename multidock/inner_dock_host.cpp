#include "inner_dock_host.hpp"
#include "add_dock_dialog.hpp"
#include "persistence.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QSizePolicy>
#include <QEvent>
#include <QMouseEvent>

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
    , m_autoSaveTimer(new QTimer(this))
{
    SetupDockOptions();
    SetupToolBar();
    
    // Setup auto-save timer
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(AUTO_SAVE_DELAY_MS);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &InnerDockHost::LayoutChanged);
    
    // Initialize the toolbar state after toolbar is created
    UpdateToolBarState();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Created InnerDockHost for '%s'", 
         m_multiDockId.toUtf8().constData());
}

InnerDockHost::~InnerDockHost()
{
    blog(LOG_INFO, "[StreamUP MultiDock] Destroying InnerDockHost for '%s'", 
         m_multiDockId.toUtf8().constData());
}

bool InnerDockHost::eventFilter(QObject* watched, QEvent* event)
{
    // Track focus events to update current dock selection
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
        // Find which dock this object belongs to
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (widget) {
            QDockWidget* dock = nullptr;
            
            // If the watched object is a dock widget itself
            if ((dock = qobject_cast<QDockWidget*>(watched))) {
                // Direct dock widget
            } else {
                // Search up the parent hierarchy to find the dock widget
                QWidget* parent = widget;
                while (parent && !dock) {
                    parent = parent->parentWidget();
                    dock = qobject_cast<QDockWidget*>(parent);
                }
            }
            
            // Update current dock if we found a valid captured dock
            if (dock && m_capturedDocks.contains(GenerateDockId(dock))) {
                if (m_currentDock != dock) {
                    m_currentDock = dock;
                    UpdateToolBarState();
                    blog(LOG_INFO, "[StreamUP MultiDock] Current dock changed to '%s'", 
                         dock->windowTitle().toUtf8().constData());
                }
            }
        }
    }
    
    return QMainWindow::eventFilter(watched, event);
}

void InnerDockHost::SetupDockOptions()
{
    setDockOptions(AllowTabbedDocks | AllowNestedDocks | AnimatedDocks);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
}

void InnerDockHost::SetupToolBar()
{
    m_toolBar = addToolBar("MultiDock Controls");
    m_toolBar->setMovable(false);
    m_toolBar->setFloatable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // Make sure toolbar is always visible and at the top
    addToolBarBreak();
    m_toolBar->setObjectName("MultiDockToolBar");
    
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
    
    // Record original placement
    OriginalPlacement original;
    original.main = qobject_cast<QMainWindow*>(dock->parent());
    if (original.main) {
        original.area = original.main->dockWidgetArea(dock);
        original.wasFloating = dock->isFloating();
    } else {
        original.area = Qt::RightDockWidgetArea;
        original.wasFloating = false;
    }
    
    // Store captured dock info
    CapturedDock captured;
    captured.widget = dock;
    captured.original = original;
    m_capturedDocks[dockId] = captured;
    
    // Move dock to this host
    addDockWidget(area, dock);
    ConnectDockSignals(dock);
    
    // Set as current dock and make it visible/focused
    m_currentDock = dock;
    dock->show();
    dock->raise();
    
    UpdateToolBarState();
    TriggerAutoSave();
    
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
    
    UpdateToolBarState();
    TriggerAutoSave();
    
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
        restoreState(layout);
        blog(LOG_INFO, "[StreamUP MultiDock] Restored layout for MultiDock '%s'", 
             m_multiDockId.toUtf8().constData());
    }
}

QByteArray InnerDockHost::SaveLayout() const
{
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
        TriggerAutoSave();
    }
}

void InnerDockHost::OnDockLocationChanged(Qt::DockWidgetArea area)
{
    Q_UNUSED(area)
    TriggerAutoSave();
}

void InnerDockHost::OnTabifiedDockActivated(QDockWidget* dock)
{
    m_currentDock = dock;
    UpdateToolBarState();
}

void InnerDockHost::OnDockVisibilityChanged(bool visible)
{
    Q_UNUSED(visible)
    TriggerAutoSave();
}

void InnerDockHost::TriggerAutoSave()
{
    m_autoSaveTimer->start();
}

void InnerDockHost::ConnectDockSignals(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    connect(dock, &QDockWidget::dockLocationChanged, this, &InnerDockHost::OnDockLocationChanged);
    connect(dock, &QDockWidget::visibilityChanged, this, &InnerDockHost::OnDockVisibilityChanged);
    
    // Connect to the dock's focus events to track current selection
    dock->installEventFilter(this);
    
    // Also connect to the dock's widget focus if it has one
    if (dock->widget()) {
        dock->widget()->installEventFilter(this);
    }
}

void InnerDockHost::DisconnectDockSignals(QDockWidget* dock)
{
    if (!dock) {
        return;
    }
    
    disconnect(dock, &QDockWidget::dockLocationChanged, this, &InnerDockHost::OnDockLocationChanged);
    disconnect(dock, &QDockWidget::visibilityChanged, this, &InnerDockHost::OnDockVisibilityChanged);
    
    // Remove event filters
    dock->removeEventFilter(this);
    if (dock->widget()) {
        dock->widget()->removeEventFilter(this);
    }
}

void InnerDockHost::UpdateToolBarState()
{
    int dockCount = m_capturedDocks.size();
    bool hasCurrentDock = m_currentDock != nullptr;
    
    // Update action states
    m_returnDockAction->setEnabled(hasCurrentDock);
    m_closeDockAction->setEnabled(hasCurrentDock && m_currentDock->isVisible());
    
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

} // namespace MultiDock
} // namespace StreamUP

#include "inner_dock_host.moc"