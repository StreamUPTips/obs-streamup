#include "multidock_dock.hpp"
#include "inner_dock_host.hpp"
#include "persistence.hpp"
#include "multidock_utils.hpp"
#include <obs-module.h>
#include <QVBoxLayout>

namespace StreamUP {
namespace MultiDock {

MultiDockDock::MultiDockDock(const QString& id, const QString& name, QWidget* parent)
    : QFrame(parent)
    , m_id(id)
    , m_name(name)
    , m_innerHost(nullptr)
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
    
    // Create inner host as a direct child
    m_innerHost = new InnerDockHost(m_id, this);
    
    // Remove window flags to make it look integrated
    m_innerHost->setWindowFlags(Qt::Widget);
    
    // Add inner host to the main layout
    mainLayout->addWidget(m_innerHost);
    
    // Connect layout change signal for auto-save
    connect(m_innerHost, &InnerDockHost::LayoutChanged, this, &MultiDockDock::OnLayoutChanged);
    
    // Set minimum size to ensure usability
    setMinimumSize(400, 300);
    
    // Set frame style to be minimal but visible
    setFrameStyle(QFrame::StyledPanel);
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
    
    blog(LOG_INFO, "[StreamUP MultiDock] Restored %d out of %d docks for MultiDock '%s'", 
         restoredCount, capturedDockIds.size(), m_id.toUtf8().constData());
}

void MultiDockDock::OnLayoutChanged()
{
    // Auto-save when layout changes
    SaveState();
}

} // namespace MultiDock
} // namespace StreamUP

#include "multidock_dock.moc"