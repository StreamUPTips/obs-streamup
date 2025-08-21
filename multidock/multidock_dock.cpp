#include "multidock_dock.hpp"
#include "inner_dock_host.hpp"
#include "persistence.hpp"
#include "multidock_utils.hpp"
#include "../ui/ui-styles.hpp"
#include <obs-module.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QToolBar>
#include <QAction>

namespace StreamUP {
namespace MultiDock {

MultiDockDock::MultiDockDock(const QString& id, const QString& name, QWidget* parent)
    : QFrame(parent)
    , m_id(id)
    , m_name(name)
    , m_innerHost(nullptr)
    , m_statusLabel(nullptr)
    , m_addDockAction(nullptr)
    , m_returnDockAction(nullptr)
    , m_closeDockAction(nullptr)
    , m_lockDockAction(nullptr)
    , m_docksLocked(false)
{
    SetupUi();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Created MultiDockDock '%s' with ID '%s'", 
         m_name.toUtf8().constData(), m_id.toUtf8().constData());
}

MultiDockDock::~MultiDockDock()
{
    blog(LOG_INFO, "[StreamUP MultiDock] Destroying MultiDockDock '%s'", 
         m_name.toUtf8().constData());
}

void MultiDockDock::SetupUi()
{
    // Set object name for identification  
    setObjectName(QString("streamup_multidock_%1").arg(m_id));
    
    // Create main layout directly on this QFrame
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Create a container widget for the inner host with padding
    QWidget* innerContainer = new QWidget();
    innerContainer->setObjectName("MultiDockInnerContainer");
    innerContainer->setStyleSheet("QWidget#MultiDockInnerContainer { background-color: #0d0d0d; }");
    QVBoxLayout* innerLayout = new QVBoxLayout(innerContainer);
    innerLayout->setContentsMargins(12, 12, 12, 12); // 12px padding on all sides
    innerLayout->setSpacing(0);
    
    // Create inner host as a direct child
    m_innerHost = new InnerDockHost(m_id, this);
    
    // Remove window flags to make it look integrated
    m_innerHost->setWindowFlags(Qt::Widget);
    
    // Add inner host to the container layout
    innerLayout->addWidget(m_innerHost, 1);
    
    // Add container to main layout (takes most space)
    mainLayout->addWidget(innerContainer, 1);
    
    // Create and add the toolbar at the bottom of our layout (no padding)
    CreateBottomToolbar(mainLayout);
    
    // No auto-save - we save on OBS shutdown
    
    // Set minimum size to ensure usability
    setMinimumSize(400, 300);
    
    // Set the background color to #0d0d0d
    setStyleSheet("MultiDockDock { background-color: #0d0d0d; }");
    setFrameStyle(QFrame::NoFrame);
}

void MultiDockDock::SetName(const QString& name)
{
    m_name = name;
    // OBS will handle the title when this widget is registered as a dock
    
    blog(LOG_INFO, "[StreamUP MultiDock] Renamed MultiDock '%s' to '%s'", 
         m_id.toUtf8().constData(), m_name.toUtf8().constData());
}

void MultiDockDock::SaveState()
{
    if (!m_innerHost) {
        return;
    }
    
    QStringList capturedDockIds = m_innerHost->GetCapturedDockIds();
    QByteArray layout = m_innerHost->SaveLayout();
    
    SaveMultiDockState(m_id, capturedDockIds, layout);
    
    blog(LOG_INFO, "[StreamUP MultiDock] Saved state for MultiDock '%s': %d captured docks", 
         m_id.toUtf8().constData(), capturedDockIds.size());
}

void MultiDockDock::LoadState()
{
    if (!m_innerHost) {
        return;
    }
    
    QStringList capturedDockIds;
    QByteArray layout;
    
    if (!LoadMultiDockState(m_id, capturedDockIds, layout)) {
        blog(LOG_INFO, "[StreamUP MultiDock] No saved state found for MultiDock '%s'", 
             m_id.toUtf8().constData());
        return;
    }
    
    // Try to restore captured docks
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        blog(LOG_ERROR, "[StreamUP MultiDock] Cannot restore docks: main window not found");
        return;
    }
    
    QList<QDockWidget*> allDocks = FindAllObsDocks(mainWindow);
    int restoredCount = 0;
    
    for (const QString& dockId : capturedDockIds) {
        QDockWidget* dock = nullptr;
        
        // Try to find dock by ID
        for (QDockWidget* candidate : allDocks) {
            if (GenerateDockId(candidate) == dockId) {
                dock = candidate;
                break;
            }
        }
        
        if (dock && !IsMultiDockContainer(dock)) {
            m_innerHost->AddDock(dock);
            restoredCount++;
        } else {
            blog(LOG_WARNING, "[StreamUP MultiDock] Could not restore dock with ID '%s'", 
                 dockId.toUtf8().constData());
        }
    }
    
    // Restore layout after adding docks
    if (!layout.isEmpty()) {
        m_innerHost->RestoreLayout(layout);
    }
    
    // Ensure the MultiDock is properly shown and toolbar state is updated
    if (restoredCount > 0) {
        // Make sure the inner host is visible
        m_innerHost->show();
        
        // Use a timer to reapply dock features after OBS finishes initialization
        QTimer::singleShot(1000, [this]() {
            if (m_innerHost) {
                m_innerHost->ReapplyDockFeatures();
            }
        });
    }
    
    blog(LOG_INFO, "[StreamUP MultiDock] Restored %d out of %d docks for MultiDock '%s'", 
         restoredCount, capturedDockIds.size(), m_id.toUtf8().constData());
}

void MultiDockDock::CreateBottomToolbar(QVBoxLayout* layout)
{
    if (!layout || !m_innerHost) {
        return;
    }
    
    // Create a proper QToolBar instead of a custom widget
    QToolBar* toolBar = new QToolBar("MultiDock Controls", this);
    toolBar->setObjectName("MultiDockBottomToolbar");
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolBar->setOrientation(Qt::Horizontal);
    
    // Make it look more like a standard toolbar
    toolBar->setIconSize(QSize(16, 16));
    
    // Add Dock action
    QAction* addDockAction = toolBar->addAction("Add Dock");
    addDockAction->setToolTip("Add an OBS dock to this MultiDock");
    connect(addDockAction, &QAction::triggered, [this]() {
        if (m_innerHost) {
            m_innerHost->ShowAddDockDialog();
        }
    });
    
    toolBar->addSeparator();
    
    // Close Dock action (returns dock to original position)
    QAction* closeDockAction = toolBar->addAction("Close Dock");
    closeDockAction->setToolTip("Return the selected dock to the main OBS window");
    connect(closeDockAction, &QAction::triggered, [this]() {
        if (m_innerHost) {
            m_innerHost->ReturnCurrentDock();
        }
    });
    
    // Lock Docks action
    QAction* lockDockAction = toolBar->addAction("🔓 Unlock");
    lockDockAction->setToolTip("Toggle dock locking (prevents moving/resizing when locked)");
    lockDockAction->setCheckable(true);
    connect(lockDockAction, &QAction::triggered, [this, lockDockAction](bool checked) {
        m_docksLocked = checked;
        lockDockAction->setText(m_docksLocked ? "🔒 Lock" : "🔓 Unlock");
        lockDockAction->setToolTip(m_docksLocked ? "Docks are locked (click to unlock)" : "Docks are unlocked (click to lock)");
        
        if (m_innerHost) {
            m_innerHost->SetDocksLocked(m_docksLocked);
        }
    });
    
    // Store references for later access
    m_statusLabel = nullptr; // No status label anymore
    m_addDockAction = addDockAction;
    m_returnDockAction = nullptr; // No separate return action
    m_closeDockAction = closeDockAction;
    m_lockDockAction = lockDockAction;
    
    // Add toolbar widget to the bottom of the layout
    layout->addWidget(toolBar, 0); // 0 means don't stretch
    
    blog(LOG_INFO, "[StreamUP MultiDock] Created bottom toolbar for MultiDock '%s'", 
         m_id.toUtf8().constData());
}

void MultiDockDock::UpdateToolbarState()
{
    if (!m_innerHost) {
        return;
    }
    
    QList<QDockWidget*> docks = m_innerHost->GetAllDocks();
    QDockWidget* currentDock = m_innerHost->GetCurrentDock();
    bool hasCurrentDock = currentDock != nullptr;
    
    // Update close dock action state (only enabled when there's a current dock)
    if (m_closeDockAction) {
        m_closeDockAction->setEnabled(hasCurrentDock);
    }
}


} // namespace MultiDock
} // namespace StreamUP

#include "multidock_dock.moc"