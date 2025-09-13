#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <obs.h>
#include <obs-hotkey.h>
#include <obs-frontend-api.h>

namespace StreamUP {

// Structure to hold hotkey information
struct HotkeyInfo {
    QString name;           // Internal hotkey name (e.g., "OBSBasic.StartStreaming")
    QString description;    // Display description (e.g., "Start Streaming")
    obs_hotkey_id id;      // Hotkey ID for triggering
    obs_hotkey_registerer_t registererType; // Frontend, Source, etc.
    QString context;       // Context name for source/output hotkeys (empty for frontend)
    bool isEnabled;        // Whether the hotkey is currently enabled
    
    HotkeyInfo() : id(OBS_INVALID_HOTKEY_ID), registererType(OBS_HOTKEY_REGISTERER_FRONTEND), isEnabled(false) {}
    HotkeyInfo(const QString& n, const QString& desc, obs_hotkey_id hid, obs_hotkey_registerer_t type)
        : name(n), description(desc), id(hid), registererType(type), isEnabled(true) {}
};

// Manager class for OBS hotkeys
class OBSHotkeyManager {
public:
    // Get all available hotkeys from OBS
    static QList<HotkeyInfo> getAvailableHotkeys();
    
    // Get only frontend hotkeys (most useful for toolbar buttons)
    static QList<HotkeyInfo> getFrontendHotkeys();
    
    // Trigger a hotkey by name
    static bool triggerHotkey(const QString& hotkeyName);
    
    // Trigger a hotkey by ID
    static bool triggerHotkeyById(obs_hotkey_id hotkeyId);
    
    // Get hotkey description by name
    static QString getHotkeyDescription(const QString& hotkeyName);
    
    // Check if a hotkey exists
    static bool hotkeyExists(const QString& hotkeyName);
    
    // Get default icon for a hotkey (based on hotkey name)
    static QString getDefaultHotkeyIcon(const QString& hotkeyName);
    
    // Get categorized hotkeys for UI display
    static QMap<QString, QList<HotkeyInfo>> getCategorizedHotkeys();

private:
    // Internal helper functions
    static obs_hotkey_t* findHotkeyByName(const QString& hotkeyName);
    static QString formatHotkeyDescription(obs_hotkey_t* hotkey);
    static QString getHotkeyCategory(const QString& hotkeyName);
    
    // Static enumeration callback
    static bool enumerationCallback(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey);
};

} // namespace StreamUP