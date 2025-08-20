#include "multidock_manager.hpp"
#include "multidock_dock.hpp"
#include "multidock_utils.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QUuid>
#include <QDockWidget>

namespace StreamUP {
namespace MultiDock {

MultiDockManager* MultiDockManager::s_instance = nullptr;

MultiDockManager::MultiDockManager(QObject* parent)
    : QObject(parent)
{
    blog(LOG_INFO, "[StreamUP MultiDock] MultiDockManager created");
}

MultiDockManager::~MultiDockManager()
{
    blog(LOG_INFO, "[StreamUP MultiDock] MultiDockManager destroyed");
}

MultiDockManager* MultiDockManager::Instance()
{
    return s_instance;
}

void MultiDockManager::Initialize()
{
    if (s_instance) {
        blog(LOG_WARNING, "[StreamUP MultiDock] MultiDockManager already initialized");
        return;
    }
    
    s_instance = new MultiDockManager();
    s_instance->LoadAllMultiDocks();
    
    blog(LOG_INFO, "[StreamUP MultiDock] MultiDockManager initialized");
}

void MultiDockManager::Shutdown()
{
    if (!s_instance) {
        return;
    }
    
    s_instance->SaveAllMultiDocks();
    
    // Clean up all MultiDocks
    QList<MultiDockDock*> multiDocks = s_instance->GetAllMultiDocks();
    for (MultiDockDock* multiDock : multiDocks) {
        if (multiDock) {
            s_instance->UnregisterFromObs(multiDock);
            multiDock->deleteLater();
        }
    }
    
    delete s_instance;
    s_instance = nullptr;
    
    blog(LOG_INFO, "[StreamUP MultiDock] MultiDockManager shutdown");
}

QString MultiDockManager::CreateMultiDock(const QString& name)
{
    if (name.trimmed().isEmpty()) {
        blog(LOG_WARNING, "[StreamUP MultiDock] Cannot create MultiDock with empty name");
        return QString();
    }
    
    QString id = GenerateUniqueId();
    QString trimmedName = name.trimmed();
    
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        blog(LOG_ERROR, "[StreamUP MultiDock] Cannot create MultiDock: main window not found");
        return QString();
    }
    
    MultiDockDock* multiDock = new MultiDockDock(id, trimmedName, nullptr);
    m_multiDocks[id] = multiDock;
    
    // Connect destroyed signal to handle cleanup
    connect(multiDock, &QObject::destroyed, this, &MultiDockManager::OnMultiDockDestroyed);
    
    RegisterWithObs(multiDock);
    
    // Save the updated list
    SaveAllMultiDocks();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Created MultiDock '%s' with ID '%s'", 
         trimmedName.toUtf8().constData(), id.toUtf8().constData());
    
    return id;
}

bool MultiDockManager::RemoveMultiDock(const QString& id)
{
    if (!m_multiDocks.contains(id)) {
        return false;
    }
    
    MultiDockDock* multiDock = m_multiDocks[id];
    if (multiDock) {
        QString name = multiDock->GetName();
        
        UnregisterFromObs(multiDock);
        multiDock->deleteLater();
        
        blog(LOG_INFO, "[StreamUP MultiDock] Removed MultiDock '%s'", name.toUtf8().constData());
    }
    
    m_multiDocks.remove(id);
    
    // Remove from persistent storage
    RemoveMultiDockState(id);
    SaveAllMultiDocks();
    
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
    SaveAllMultiDocks();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Renamed MultiDock '%s' to '%s'", 
         oldName.toUtf8().constData(), trimmedName.toUtf8().constData());
    
    return true;
}

MultiDockDock* MultiDockManager::GetMultiDock(const QString& id) const
{
    return m_multiDocks.value(id, nullptr);
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
    QList<MultiDockInfo> result;
    for (auto it = m_multiDocks.begin(); it != m_multiDocks.end(); ++it) {
        if (it.value()) {
            MultiDockInfo info;
            info.id = it.key();
            info.name = it.value()->GetName();
            result.append(info);
        }
    }
    return result;
}

void MultiDockManager::LoadAllMultiDocks()
{
    QList<MultiDockInfo> multiDockList = LoadMultiDockList();
    
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        blog(LOG_ERROR, "[StreamUP MultiDock] Cannot load MultiDocks: main window not found");
        return;
    }
    
    for (const MultiDockInfo& info : multiDockList) {
        if (m_multiDocks.contains(info.id)) {
            continue; // Already loaded
        }
        
        MultiDockDock* multiDock = new MultiDockDock(info.id, info.name, nullptr);
        m_multiDocks[info.id] = multiDock;
        
        connect(multiDock, &QObject::destroyed, this, &MultiDockManager::OnMultiDockDestroyed);
        
        RegisterWithObs(multiDock);
        
        // Load the saved state
        multiDock->LoadState();
    }
    
    blog(LOG_INFO, "[StreamUP MultiDock] Loaded %d MultiDocks from persistent storage", 
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
    
    blog(LOG_INFO, "[StreamUP MultiDock] Saved %d MultiDocks to persistent storage", 
         multiDockList.size());
}

void MultiDockManager::OnMultiDockDestroyed(QObject* obj)
{
    // Find and remove the destroyed MultiDock from our hash
    for (auto it = m_multiDocks.begin(); it != m_multiDocks.end(); ++it) {
        if (it.value() == static_cast<MultiDockDock*>(obj)) {
            m_multiDocks.erase(it);
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
    // Modern API - register the widget directly
    bool success = obs_frontend_add_dock_by_id(id.toUtf8().constData(), 
                                               title.toUtf8().constData(), 
                                               multiDock);
    
    if (success) {
        blog(LOG_INFO, "[StreamUP MultiDock] Registered MultiDock '%s' with OBS (ID: %s)", 
             title.toUtf8().constData(), id.toUtf8().constData());
    } else {
        blog(LOG_ERROR, "[StreamUP MultiDock] Failed to register MultiDock '%s' with OBS", 
             title.toUtf8().constData());
    }
#else
    // Legacy API - wrap in QDockWidget
    auto dock = new QDockWidget(mainWindow);
    dock->setObjectName(id);
    dock->setWindowTitle(title);
    dock->setWidget(multiDock);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    dock->setFloating(true);
    dock->hide();
    obs_frontend_add_dock(dock);
    
    blog(LOG_INFO, "[StreamUP MultiDock] Registered MultiDock '%s' with OBS (legacy API)", 
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
    
    obs_frontend_remove_dock(id.toUtf8().constData());
    
    blog(LOG_INFO, "[StreamUP MultiDock] Unregistered MultiDock '%s' from OBS (ID: %s)", 
         name.toUtf8().constData(), id.toUtf8().constData());
}

} // namespace MultiDock
} // namespace StreamUP

#include "multidock_manager.moc"