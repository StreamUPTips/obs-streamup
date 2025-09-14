#include "multidock_utils.hpp"
#include "../utilities/debug-logger.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCryptographicHash>
#include <QRegularExpression>

namespace StreamUP {
namespace MultiDock {

QMainWindow* GetObsMainWindow()
{
    void* main_window_ptr = obs_frontend_get_main_window();
    if (!main_window_ptr) {
        StreamUP::DebugLogger::LogError("MultiDock", "Could not get main OBS window");
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
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Utils", "Found %d dock widgets in main window", docks.size());
    
    // Log dock names for debugging
    for (const auto* dock : docks) {
        if (dock) {
            QString name = dock->windowTitle();
            QString objectName = dock->objectName();
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Utils", "Dock: '%s' (objectName: '%s')",
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
    
    QString objectName = dock->objectName();
    QString title = dock->windowTitle();
    
    // Plugin-specific handlers for known problematic cases
    if (IsQuickAccessUtilityDock(dock)) {
        return GetQuickAccessStableId(dock);
    }
    
    // Standard case: use objectName if it looks stable
    if (!objectName.isEmpty() && IsStableObjectName(objectName)) {
        return objectName;
    }
    
    // Fallback: create stable ID from title + widget type
    return CreateFallbackId(dock);
}

bool IsQuickAccessUtilityDock(QDockWidget* dock)
{
    QString objectName = dock->objectName();
    QString title = dock->windowTitle();
    
    return objectName.startsWith("quick-access-dock_") || 
           title.contains("Quick Access", Qt::CaseInsensitive);
}

QString GetQuickAccessStableId(QDockWidget* dock)
{
    QString title = dock->windowTitle();
    QString stableId = "qau_" + title.toLower().replace(" ", "_").replace("-", "_");
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "ID Generation", "Quick Access Utility dock '%s' -> stable ID '%s'", 
         title.toUtf8().constData(), stableId.toUtf8().constData());
    
    return stableId;
}


bool IsStableObjectName(const QString& objectName)
{
    // Empty names are not stable
    if (objectName.isEmpty()) {
        return false;
    }
    
    // UUIDs are not stable (pattern: 8-4-4-4-12 hex characters)
    QRegularExpression uuidPattern("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    if (uuidPattern.match(objectName).hasMatch()) {
        return false;
    }
    
    // Names ending with UUIDs are not stable (plugin-name_UUID pattern)
    if (objectName.contains("_") && objectName.length() > 30) {
        QStringList parts = objectName.split("_");
        if (parts.size() >= 2 && uuidPattern.match(parts.last()).hasMatch()) {
            return false;
        }
    }
    
    // Everything else is considered stable
    return true;
}

QString CreateFallbackId(QDockWidget* dock)
{
    QString title = dock->windowTitle();
    QWidget* containedWidget = dock->widget();
    
    // Create identifier from title and contained widget type
    QString identifier = title;
    if (containedWidget) {
        identifier += "_" + QString(containedWidget->metaObject()->className());
    }
    
    // Hash for consistency but keep it readable
    QByteArray hash = QCryptographicHash::hash(identifier.toUtf8(), QCryptographicHash::Md5);
    QString fallbackId = "dock_" + hash.toHex().left(12); // Shorter hash
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "ID Generation", "Created fallback ID '%s' for dock '%s'", 
         fallbackId.toUtf8().constData(), title.toUtf8().constData());
    
    return fallbackId;
}

bool IsMultiDockContainer(QDockWidget* dock)
{
    if (!dock) {
        return false;
    }
    
    // Check if this dock's objectName indicates it's a MultiDock container
    QString objectName = dock->objectName();
    if (objectName.startsWith("streamup_multidock_")) {
        return true;
    }
    
    // For OBS versions that wrap widgets in QDockWidget, check the contained widget
    QWidget* containedWidget = dock->widget();
    if (containedWidget) {
        QString containedObjectName = containedWidget->objectName();
        if (containedObjectName.startsWith("streamup_multidock_")) {
            return true;
        }
        
        // Check widget class name as backup
        QString className = containedWidget->metaObject()->className();
        if (className.contains("MultiDockDock")) {
            return true;
        }
    }
    
    return false;
}

} // namespace MultiDock
} // namespace StreamUP