#include "multidock_dock.hpp"
#include "../utilities/debug-logger.hpp"
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
#include <QIcon>
#include <QDockWidget>
#include <QMainWindow>
#include <QPainter>
#include <QPixmap>
#include <QtSvg/QSvgRenderer>

namespace StreamUP {
namespace MultiDock {

// Helper function to create colored icons from SVG resources
static QIcon CreateColoredIcon(const QString& svgPath, const QColor& color, const QSize& size = QSize(16, 16))
{
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QSvgRenderer renderer(svgPath);
    if (renderer.isValid()) {
        renderer.render(&painter);
        
        // Apply color overlay
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
    }
    
    return QIcon(pixmap);
}

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
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Creation", "Created MultiDockDock '%s' with ID '%s'",
         m_name.toUtf8().constData(), m_id.toUtf8().constData());
}

MultiDockDock::~MultiDockDock()
{
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Cleanup", "Destroying MultiDockDock '%s'",
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
    
    // Find and update the OBS dock widget's title
    QMainWindow* mainWindow = GetObsMainWindow();
    if (mainWindow) {
        QDockWidget* obsDocWidget = mainWindow->findChild<QDockWidget*>(m_id);
        if (obsDocWidget) {
            obsDocWidget->setWindowTitle(name);
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Management", "Updated OBS dock title to '%s'",
                 name.toUtf8().constData());
        } else {
            StreamUP::DebugLogger::LogWarning("MultiDock", "Management: Could not find OBS dock widget to update title");
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Management", "Renamed MultiDock '%s' to '%s'",
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
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "State", "Saved state for MultiDock '%s': %d captured docks",
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
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "State", "No saved state found for MultiDock '%s'",
             m_id.toUtf8().constData());
        return;
    }
    
    // Try to restore captured docks
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        StreamUP::DebugLogger::LogError("MultiDock", "Cannot restore docks: main window not found");
        return;
    }
    
    QList<QDockWidget*> allDocks = FindAllObsDocks(mainWindow);
    int restoredCount = 0;
    
    for (const QString& dockId : capturedDockIds) {
        QDockWidget* dock = nullptr;
        
        // Try to find dock by ID
        for (QDockWidget* candidate : allDocks) {
            QString candidateId = GenerateDockId(candidate);
            if (candidateId == dockId) {
                dock = candidate;
                break;
            }
        }
        
        if (dock && !IsMultiDockContainer(dock)) {
            m_innerHost->AddDock(dock);
            restoredCount++;
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Successfully restored dock '%s' with ID '%s'",
                 dock->windowTitle().toUtf8().constData(), dockId.toUtf8().constData());
        } else {
            // Enhanced debugging for failed dock restoration
            StreamUP::DebugLogger::LogWarningFormat("MultiDock", "Restoration", "Could not restore dock with ID '%s'",
                 dockId.toUtf8().constData());
            
            // Log all available dock IDs for debugging
            StreamUP::DebugLogger::LogDebug("MultiDock", "Restoration", "Available docks for debugging:");
            for (QDockWidget* candidate : allDocks) {
                QString candidateId = GenerateDockId(candidate);
                QString objectName = candidate->objectName();
                QString title = candidate->windowTitle();
                bool isMultiDock = IsMultiDockContainer(candidate);
                
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "  - ID:'%s' ObjectName:'%s' Title:'%s' IsMultiDock:%s",
                     candidateId.toUtf8().constData(),
                     objectName.toUtf8().constData(),
                     title.toUtf8().constData(),
                     isMultiDock ? "true" : "false");
            }
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
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Restored %d out of %d docks for MultiDock '%s'",
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
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setOrientation(Qt::Horizontal);
    
    // Make it look more like a standard OBS toolbar
    toolBar->setIconSize(QSize(16, 16));
    
    // Apply exact OBS toolbar styling with correct colors and height
    toolBar->setStyleSheet(
        "QToolBar {"
        "    background-color: #161617;"
        "    min-height: 24px;"
        "    max-height: 24px;"
        "    border: none;"
        "    padding: 2px 4px 2px 4px;"
        "}"
        "QToolButton {"
        "    background: transparent;"
        "    border: none;"
        "    border-radius: 6px;"
        "    margin: 0px 8px 0px 8px;"
        "    min-width: 20px;"
        "    max-width: 20px;"
        "    min-height: 20px;"
        "    max-height: 20px;"
        "    padding: 0px;"
        "}"
        "QToolButton:hover {"
        "    background-color: #0f7bcf;"
        "}"
        "QToolButton:pressed {"
        "    background-color: #0a5a9c;"
        "}"
        "QToolButton:disabled {"
        "    background: transparent;"
        "}"
    );
    
    // Add Dock action with #fefefe plus icon
    QAction* addDockAction = toolBar->addAction(CreateColoredIcon(":/res/images/plus.svg", QColor("#fefefe")), "");
    addDockAction->setToolTip("Add an OBS dock to this MultiDock");
    connect(addDockAction, &QAction::triggered, [this]() {
        if (m_innerHost) {
            m_innerHost->ShowAddDockDialog();
        }
    });
    
    // Lock Docks action with colored lock icons (start unlocked with #3a3a3d)
    QAction* lockDockAction = toolBar->addAction(CreateColoredIcon(":/res/images/unlocked.svg", QColor("#3a3a3d")), "");
    lockDockAction->setToolTip("Docks are unlocked (click to lock)");
    // Remove checkable to prevent blue background when locked
    connect(lockDockAction, &QAction::triggered, [this, lockDockAction]() {
        m_docksLocked = !m_docksLocked;
        if (m_docksLocked) {
            lockDockAction->setIcon(CreateColoredIcon(":/res/images/locked.svg", QColor("#fefefe")));
        } else {
            lockDockAction->setIcon(CreateColoredIcon(":/res/images/unlocked.svg", QColor("#3a3a3d")));
        }
        lockDockAction->setToolTip(m_docksLocked ? "Docks are locked (click to unlock)" : "Docks are unlocked (click to lock)");
        
        if (m_innerHost) {
            m_innerHost->SetDocksLocked(m_docksLocked);
            // Update toolbar state after lock change
            UpdateToolbarState();
        }
    });
    
    // Store references for later access
    m_statusLabel = nullptr; // No status label anymore
    m_addDockAction = addDockAction;
    m_returnDockAction = nullptr; // No separate return action
    m_closeDockAction = nullptr; // No close dock action anymore
    m_lockDockAction = lockDockAction;
    
    // Add toolbar widget to the bottom of the layout
    layout->addWidget(toolBar, 0); // 0 means don't stretch
    
    // Initialize toolbar button states
    UpdateToolbarState();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "UI", "Created bottom toolbar for MultiDock '%s'",
         m_id.toUtf8().constData());
}

void MultiDockDock::UpdateToolbarState()
{
    if (!m_innerHost) {
        return;
    }
    
    bool isLocked = m_docksLocked;
    
    // Update button states based on lock status
    if (m_addDockAction) {
        m_addDockAction->setEnabled(!isLocked);
        if (isLocked) {
            m_addDockAction->setIcon(CreateColoredIcon(":/res/images/plus.svg", QColor("#3a3a3d")));
        } else {
            m_addDockAction->setIcon(CreateColoredIcon(":/res/images/plus.svg", QColor("#fefefe")));
        }
    }
}


} // namespace MultiDock
} // namespace StreamUP

#include "multidock_dock.moc"
