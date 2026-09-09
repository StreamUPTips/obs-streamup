#include "multidock_utils.hpp"
#include <streamup/debug-logger.hpp>
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
    return static_cast<QMainWindow*>(main_window_ptr);
}

QList<QDockWidget*> FindAllObsDocks(QMainWindow* mainWindow)
{
    if (!mainWindow) {
        return QList<QDockWidget*>();
    }
    
    QList<QDockWidget*> docks = mainWindow->findChildren<QDockWidget*>();
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Utils", "Found %d dock widgets in main window", docks.size());
    
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

QStringList ExtractDockNamesFromLayout(const QByteArray& layout)
{
    QStringList names;
    if (layout.isEmpty()) {
        return names;
    }

    // QMainWindow's saved state is a QDataStream of, among other things, the
    // object name of every dock and toolbar it knows about. QDataStream writes
    // a QString as a quint32 byte count followed by UTF-16BE data, so walk the
    // buffer looking for a length field whose payload decodes as a plausible
    // object name. Anything that does not is skipped, not guessed at.
    const uchar* data = reinterpret_cast<const uchar*>(layout.constData());
    const int size = layout.size();

    // Object names are short; the cap keeps a bogus length field from making us
    // read a large slice of unrelated bytes.
    constexpr quint32 kMaxNameBytes = 512;

    for (int i = 0; i + 4 < size; ++i) {
        const quint32 len = (static_cast<quint32>(data[i]) << 24) |
                            (static_cast<quint32>(data[i + 1]) << 16) |
                            (static_cast<quint32>(data[i + 2]) << 8) |
                            static_cast<quint32>(data[i + 3]);

        // UTF-16 payloads are an even number of bytes, and an empty or null
        // string carries no name worth recording.
        if (len < 2 || len > kMaxNameBytes || (len % 2) != 0) {
            continue;
        }
        if (static_cast<quint32>(size - (i + 4)) < len) {
            continue;
        }

        const uchar* payload = data + i + 4;
        bool printable = true;
        for (quint32 j = 0; j < len; j += 2) {
            // Dock object names are ASCII in practice; requiring a zero high
            // byte and a printable low byte is what keeps this from matching
            // arbitrary geometry integers.
            if (payload[j] != 0x00 || payload[j + 1] < 0x20 || payload[j + 1] > 0x7e) {
                printable = false;
                break;
            }
        }
        if (!printable) {
            continue;
        }

        // Big-endian payload, and the check above proved every high byte is
        // zero, so the low byte is the character.
        QString name;
        name.reserve(static_cast<int>(len / 2));
        for (quint32 j = 0; j < len; j += 2) {
            name.append(QChar(static_cast<char16_t>(payload[j + 1])));
        }

        if (!name.isEmpty() && !names.contains(name)) {
            names.append(name);
        }

        // Skip past the payload we just consumed.
        i += 4 + static_cast<int>(len) - 1;
    }

    return names;
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
    if (objectName.contains("_")) {
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
