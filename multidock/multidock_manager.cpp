#include "multidock_manager.hpp"
#include "../utilities/debug-logger.hpp"
#include "multidock_dock.hpp"
#include "inner_dock_host.hpp"
#include "multidock_utils.hpp"
#include "persistence.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QUuid>
#include <QDockWidget>
#include <QTimer>

namespace StreamUP {
namespace MultiDock {

MultiDockManager* MultiDockManager::s_instance = nullptr;

MultiDockManager::MultiDockManager(QObject* parent)
    : QObject(parent)
    , m_hasRetriedRestoration(false)
{
    // Register for OBS frontend events to save state more reliably
    obs_frontend_add_event_callback(OnFrontendEvent, this);
    
}

MultiDockManager::~MultiDockManager()
{
    // Remove event callback
    obs_frontend_remove_event_callback(OnFrontendEvent, this);
    
}

MultiDockManager* MultiDockManager::Instance()
{
    return s_instance;
}

void MultiDockManager::Initialize()
{
    if (s_instance) {
        StreamUP::DebugLogger::LogWarning("MultiDock", "MultiDockManager already initialized");
        return;
    }
    
    s_instance = new MultiDockManager();
    s_instance->LoadAllMultiDocks();
    
}

void MultiDockManager::OnFrontendEvent(enum obs_frontend_event event, void *private_data)
{
    MultiDockManager* manager = static_cast<MultiDockManager*>(private_data);
    if (!manager) {
        return;
    }
    
    switch (event) {
    case OBS_FRONTEND_EVENT_EXIT:
    case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
        manager->SaveAllMultiDocks();
        break;
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        // Retry failed restorations after all plugins have loaded
        if (!manager->m_hasRetriedRestoration && !manager->m_pendingRetryIds.isEmpty()) {
            // Use a timer to ensure all dock widgets are fully initialized
            QTimer::singleShot(2000, manager, &MultiDockManager::RetryFailedRestorations);
        }
        break;
    default:
        // Ignore other events
        break;
    }
}

void MultiDockManager::Shutdown()
{
    if (!s_instance) {
        return;
    }
    
    // Don't save during shutdown - the widgets may already be destroyed
    // We've already saved when needed via the event callback
    
    // Clean up remaining MultiDocks if any still exist
    QList<MultiDockDock*> multiDocks = s_instance->GetAllMultiDocks();
    for (MultiDockDock* multiDock : multiDocks) {
        if (multiDock) {
            s_instance->UnregisterFromObs(multiDock);
            multiDock->deleteLater();
        }
    }
    
    delete s_instance;
    s_instance = nullptr;
    
}

QString MultiDockManager::CreateMultiDock(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        StreamUP::DebugLogger::LogWarning("MultiDock", "Cannot create MultiDock with empty name");
        return QString();
    }
    
    QString id = GenerateUniqueId();
    QString trimmedName = name.trimmed();
    
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        StreamUP::DebugLogger::LogError("MultiDock", "Cannot create MultiDock: main window not found");
        return QString();
    }
    
    MultiDockDock* multiDock = new MultiDockDock(id, trimmedName, nullptr);
    m_multiDocks[id] = multiDock;
    
    // Store persistent info
    MultiDockInfo info;
    info.id = id;
    info.name = trimmedName;
    m_persistentInfo[id] = info;
    
    // Connect destroyed signal to handle cleanup
    connect(multiDock, &QObject::destroyed, this, &MultiDockManager::OnMultiDockDestroyed);
    
    RegisterWithObs(multiDock);
    
    // Auto-open the multidock after creation
    if (mainWindow) {
        // Find the dock widget that was just registered and show it
        QDockWidget* obsDocWidget = mainWindow->findChild<QDockWidget*>(id);
        if (obsDocWidget) {
            obsDocWidget->show();
            obsDocWidget->raise();
        }
    }
    
    // Save the updated list
    SaveAllMultiDocks();
    
    return id;
}

bool MultiDockManager::RemoveMultiDock(const QString& id)
{
    if (!m_multiDocks.contains(id)) {
        return false;
    }
    
    MultiDockDock* multiDock = m_multiDocks.value(id).data();
    
    // If we have the widget, return all captured docks first
    if (multiDock && multiDock->GetInnerHost()) {
        
        // Get all captured docks and return them to main window
        QList<QDockWidget*> allDocks = multiDock->GetInnerHost()->GetAllDocks();
        for (QDockWidget* dock : allDocks) {
            if (dock) {
                multiDock->GetInnerHost()->RemoveDock(dock);
            }
        }
        
    }
    
    // Remove from our tracking
    m_multiDocks.remove(id);
    m_persistentInfo.remove(id);
    
    // Remove from persistent storage
    RemoveMultiDockState(id);
    SaveAllMultiDocks();
    
    // Now unregister the empty MultiDock from OBS
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
    obs_frontend_remove_dock(id.toUtf8().constData());
#endif
    
    return true;
}

bool MultiDockManager::RenameMultiDock(const QString& id, const QString& newName)
{
    if (!m_multiDocks.contains(id) || newName.trimmed().isEmpty()) {
        return false;
    }
    
    MultiDockDock* multiDock = m_multiDocks[id];
    if (!multiDock) {
        return false;
    }
    
    QString trimmedName = newName.trimmed();
    QString oldName = multiDock->GetName();
    
    multiDock->SetName(trimmedName);
    
    // Update persistent info
    m_persistentInfo[id].name = trimmedName;
    
    SaveAllMultiDocks();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Management", "Renamed MultiDock '%s' to '%s'",
         oldName.toUtf8().constData(), trimmedName.toUtf8().constData());
    
    return true;
}

MultiDockDock* MultiDockManager::GetMultiDock(const QString& id) const
{
    return m_multiDocks.value(id).data();
}

QList<MultiDockDock*> MultiDockManager::GetAllMultiDocks() const
{
    QList<MultiDockDock*> result;
    for (MultiDockDock* multiDock : m_multiDocks) {
        if (multiDock) {
            result.append(multiDock);
        }
    }
    return result;
}

QList<MultiDockInfo> MultiDockManager::GetMultiDockInfoList() const
{
    // Return persistent info instead of relying on widget existence
    QList<MultiDockInfo> result;
    for (auto it = m_persistentInfo.begin(); it != m_persistentInfo.end(); ++it) {
        result.append(it.value());
    }
    return result;
}

bool MultiDockManager::IsMultiDockVisible(const QString& id) const
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return false;
    }
    
    // Find the QDockWidget by ID
    QDockWidget* dockWidget = mainWindow->findChild<QDockWidget*>(id);
    if (!dockWidget) {
        return false;
    }
    
    return dockWidget->isVisible();
}

bool MultiDockManager::SetMultiDockVisible(const QString& id, bool visible)
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return false;
    }
    
    // Find the QDockWidget by ID
    QDockWidget* dockWidget = mainWindow->findChild<QDockWidget*>(id);
    if (!dockWidget) {
        return false;
    }
    
    if (visible) {
        dockWidget->show();
        dockWidget->raise();
    } else {
        dockWidget->hide();
    }
    
    return true;
}

void MultiDockManager::LoadAllMultiDocks()
{
    QList<MultiDockInfo> multiDockList = LoadMultiDockList();
    
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        StreamUP::DebugLogger::LogError("MultiDock", "Cannot load MultiDocks: main window not found");
        return;
    }
    
    for (const MultiDockInfo& info : multiDockList) {
        if (m_multiDocks.contains(info.id)) {
            continue; // Already loaded
        }
        
        MultiDockDock* multiDock = new MultiDockDock(info.id, info.name, nullptr);
        m_multiDocks[info.id] = multiDock;
        
        // Store persistent info
        m_persistentInfo[info.id] = info;
        
        connect(multiDock, &QObject::destroyed, this, &MultiDockManager::OnMultiDockDestroyed);
        
        RegisterWithObs(multiDock);
        
        // Load the saved state and check if retry is needed
        multiDock->LoadState();
        
        // Only add to retry list if there are missing docks to restore
        QStringList capturedDockIds;
        QByteArray layout;
        if (LoadMultiDockState(info.id, capturedDockIds, layout) && !capturedDockIds.isEmpty()) {
            int currentDockCount = multiDock->GetInnerHost()->GetAllDocks().size();
            if (currentDockCount < capturedDockIds.size()) {
                m_pendingRetryIds.append(info.id);
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Loading", "Added '%s' to retry list: has %d/%d docks",
                     info.name.toUtf8().constData(), currentDockCount, capturedDockIds.size());
            } else {
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Loading", "Skipping retry for '%s': already has all %d docks",
                     info.name.toUtf8().constData(), capturedDockIds.size());
            }
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Loading", "Loaded %d MultiDocks from persistent storage",
         multiDockList.size());
}

void MultiDockManager::SaveAllMultiDocks()
{
    QList<MultiDockInfo> multiDockList = GetMultiDockInfoList();
    SaveMultiDockList(multiDockList);
    
    // Save individual states
    for (const MultiDockInfo& info : multiDockList) {
        MultiDockDock* multiDock = GetMultiDock(info.id);
        if (multiDock) {
            multiDock->SaveState();
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Saving", "Saved %d MultiDocks to persistent storage",
         multiDockList.size());
}

void MultiDockManager::RetryFailedRestorations()
{
    if (m_hasRetriedRestoration) {
        return; // Already retried once
    }
    
    m_hasRetriedRestoration = true;
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Starting retry restoration for %d MultiDocks after OBS finished loading",
         m_pendingRetryIds.size());
    
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        StreamUP::DebugLogger::LogError("MultiDock", "Cannot retry restoration: main window not found");
        return;
    }
    
    // Get fresh list of all available docks (now including late-loading plugin docks)
    QList<QDockWidget*> allDocks = FindAllObsDocks(mainWindow);
    
    // Pre-generate dock ID mappings to avoid redundant GenerateDockId() calls
    QHash<QString, QDockWidget*> availableDockMap;
    for (QDockWidget* dock : allDocks) {
        if (!IsMultiDockContainer(dock)) {
            QString dockId = GenerateDockId(dock);
            availableDockMap[dockId] = dock;
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Pre-generated %d dock ID mappings for efficient retry restoration",
         availableDockMap.size());
    
    int totalRetryAttempts = 0;
    int successfulRetries = 0;
    
    for (const QString& multiDockId : m_pendingRetryIds) {
        MultiDockDock* multiDock = GetMultiDock(multiDockId);
        if (!multiDock || !multiDock->GetInnerHost()) {
            continue;
        }
        
        // Check how many docks are currently in this MultiDock
        int currentDockCount = multiDock->GetInnerHost()->GetAllDocks().size();
        
        // Load saved state to see how many docks should be restored
        QStringList capturedDockIds;
        QByteArray layout;
        if (!LoadMultiDockState(multiDockId, capturedDockIds, layout)) {
            continue; // No saved state
        }
        
        // If we already have all expected docks, skip
        if (currentDockCount >= capturedDockIds.size()) {
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "MultiDock '%s' already has %d/%d docks, skipping retry",
                 multiDockId.toUtf8().constData(), currentDockCount, capturedDockIds.size());
            continue;
        }
        
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Retrying restoration for MultiDock '%s': has %d/%d docks",
             multiDockId.toUtf8().constData(), currentDockCount, capturedDockIds.size());
        
        // Pre-generate existing dock IDs to avoid redundant GenerateDockId() calls
        QSet<QString> existingDockIds;
        for (QDockWidget* existingDock : multiDock->GetInnerHost()->GetAllDocks()) {
            existingDockIds.insert(GenerateDockId(existingDock));
        }
        
        // Try to find and add missing docks using pre-generated mapping
        for (const QString& dockId : capturedDockIds) {
            // Skip if dock is already in the MultiDock
            if (existingDockIds.contains(dockId)) {
                continue;
            }
            
            totalRetryAttempts++;
            
            // Use the pre-generated mapping for O(1) lookup
            if (availableDockMap.contains(dockId)) {
                QDockWidget* dock = availableDockMap[dockId];
                multiDock->GetInnerHost()->AddDock(dock);
                successfulRetries++;
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Successfully restored dock '%s' during retry",
                     dock->windowTitle().toUtf8().constData());
            } else {
                StreamUP::DebugLogger::LogWarningFormat("MultiDock", "Still could not find dock with ID '%s' during retry", 
                     dockId.toUtf8().constData());
            }
        }
        
        // Restore layout if we added any docks
        if (successfulRetries > 0 && !layout.isEmpty()) {
            multiDock->GetInnerHost()->RestoreLayout(layout);
        }
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Restoration", "Retry restoration completed: %d/%d dock restorations successful",
         successfulRetries, totalRetryAttempts);
    
    // Clear the retry list
    m_pendingRetryIds.clear();
}

void MultiDockManager::OnMultiDockDestroyed(QObject* obj)
{
    if (!obj) {
        return;
    }
    
    // Find the destroyed MultiDock - with QPointer, it automatically becomes null
    // The persistent info is maintained separately
    for (auto it = m_multiDocks.begin(); it != m_multiDocks.end(); ++it) {
        if (it.value().data() == static_cast<MultiDockDock*>(obj)) {
            QString id = it.key();
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Management", "Widget destroyed for MultiDock ID '%s', QPointer automatically set to null",
                 id.toUtf8().constData());
            
            // QPointer automatically becomes null when object is destroyed
            // Persistent info is maintained in m_persistentInfo, so this MultiDock will be restored on next load
            break;
        }
    }
}

QString MultiDockManager::GenerateUniqueId() const
{
    QString id;
    do {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } while (m_multiDocks.contains(id));
    
    return id;
}

void MultiDockManager::RegisterWithObs(MultiDockDock* multiDock)
{
    if (!multiDock) {
        return;
    }
    
    QString id = multiDock->GetId();
    QString title = multiDock->GetName();
    QMainWindow* mainWindow = GetObsMainWindow();
    
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 0, 0)
    // Modern API - wrap in QDockWidget and use custom dock registration (no menu entry)
    auto dock = new QDockWidget(mainWindow);
    dock->setObjectName(id);
    dock->setWindowTitle(title);
    dock->setWidget(multiDock);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setFloating(true);
    dock->hide();
    
    // Use custom dock registration to avoid appearing in OBS View > Docks menu
    bool success = obs_frontend_add_custom_qdock(id.toUtf8().constData(), dock);
    
    if (success) {
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Registration", "Registered MultiDock '%s' with OBS as custom dock (hidden from menu, ID: %s)",
             title.toUtf8().constData(), id.toUtf8().constData());
    } else {
        StreamUP::DebugLogger::LogErrorFormat("MultiDock", "Failed to register MultiDock '%s' with OBS", 
             title.toUtf8().constData());
    }
#else
    // Legacy API - wrap in QDockWidget and use regular dock registration
    auto dock = new QDockWidget(mainWindow);
    dock->setObjectName(id);
    dock->setWindowTitle(title);
    dock->setWidget(multiDock);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setFloating(true);
    dock->hide();
    obs_frontend_add_dock(dock);
    
    // Hide from dock menu after registration
    dock->toggleViewAction()->setVisible(false);
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Registration", "Registered MultiDock '%s' with OBS (hidden from menu)",
         title.toUtf8().constData());
#endif
}

void MultiDockManager::UnregisterFromObs(MultiDockDock* multiDock)
{
    if (!multiDock) {
        return;
    }
    
    // Unregister the dock from OBS using the dock ID
    QString id = multiDock->GetId();
    QString name = multiDock->GetName();
    
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
    obs_frontend_remove_dock(id.toUtf8().constData());
#endif
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Registration", "Unregistered MultiDock '%s' from OBS (ID: %s)",
         name.toUtf8().constData(), id.toUtf8().constData());
}

} // namespace MultiDock
} // namespace StreamUP

#include "multidock_manager.moc"