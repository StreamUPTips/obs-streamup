#include "multidock_utils.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCryptographicHash>

namespace StreamUP {
namespace MultiDock {

QMainWindow* GetObsMainWindow()
{
    void* main_window_ptr = obs_frontend_get_main_window();
    if (!main_window_ptr) {
        blog(LOG_ERROR, "[StreamUP MultiDock] Could not get main OBS window");
        return nullptr;
    }
    return reinterpret_cast<QMainWindow*>(main_window_ptr);
}

QList<QDockWidget*> FindAllObsDocks(QMainWindow* mainWindow)
{
    if (!mainWindow) {
        return QList<QDockWidget*>();
    }
    
    QList<QDockWidget*> docks = mainWindow->findChildren<QDockWidget*>();
    
    blog(LOG_INFO, "[StreamUP MultiDock] Found %d dock widgets in main window", docks.size());
    
    // Log dock names for debugging
    for (const auto* dock : docks) {
        if (dock) {
            QString name = dock->windowTitle();
            QString objectName = dock->objectName();
            blog(LOG_INFO, "[StreamUP MultiDock] Dock: '%s' (objectName: '%s')", 
                 name.toUtf8().constData(), objectName.toUtf8().constData());
        }
    }
    
    return docks;
}

DockId GenerateDockId(QDockWidget* dock)
{
    if (!dock) {
        return QString();
    }
    
    // Prefer objectName if available and not empty
    QString objectName = dock->objectName();
    if (!objectName.isEmpty()) {
        return objectName;
    }
    
    // Fallback: use hash of window title + class name for stability
    QString title = dock->windowTitle();
    QString className = dock->metaObject()->className();
    QString combined = title + "|" + className;
    
    QByteArray hash = QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Md5);
    return "dock_" + hash.toHex();
}

bool IsMultiDockContainer(QDockWidget* dock)
{
    if (!dock) {
        return false;
    }
    
    // Check if this dock's objectName indicates it's a MultiDock container
    QString objectName = dock->objectName();
    return objectName.startsWith("streamup_multidock_");
}

} // namespace MultiDock
} // namespace StreamUP