#include "streamup-toolbar-config.hpp"
#include "settings-manager.hpp"
#include "streamup-toolbar-status.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <obs-frontend-api.h>
#include <obs-data.h>
#include <util/config-file.h>

namespace StreamUP {
namespace ToolbarConfig {

// ToolbarItem base implementation
QJsonObject ToolbarItem::toJson() const {
    QJsonObject obj;
    obj["type"] = static_cast<int>(type);
    obj["id"] = id;
    obj["visible"] = visible;
    return obj;
}

void ToolbarItem::fromJson(const QJsonObject& json) {
    id = json["id"].toString();
    visible = json["visible"].toBool(true);
}

// ButtonItem implementation
QJsonObject ButtonItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["buttonType"] = buttonType;
    obj["iconPath"] = iconPath;
    obj["tooltip"] = tooltip;
    obj["checkable"] = checkable;
    return obj;
}

void ButtonItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    buttonType = json["buttonType"].toString();
    iconPath = json["iconPath"].toString();
    tooltip = json["tooltip"].toString();
    checkable = json["checkable"].toBool(false);
}

// SeparatorItem implementation
QJsonObject SeparatorItem::toJson() const {
    return ToolbarItem::toJson();
}

void SeparatorItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
}

// CustomSpacerItem implementation
QJsonObject CustomSpacerItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["size"] = size;
    return obj;
}

void CustomSpacerItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    size = json["size"].toInt(20);
}

// DockButtonItem implementation
QJsonObject DockButtonItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["dockButtonType"] = dockButtonType;
    obj["name"] = name;
    obj["iconPath"] = iconPath;
    obj["tooltip"] = tooltip;
    return obj;
}

void DockButtonItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    dockButtonType = json["dockButtonType"].toString();
    name = json["name"].toString();
    iconPath = json["iconPath"].toString();
    tooltip = json["tooltip"].toString();
}

// HotkeyButtonItem implementation
QJsonObject HotkeyButtonItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["hotkeyName"] = hotkeyName;
    obj["hotkeyContext"] = hotkeyContext;
    obj["displayName"] = displayName;
    obj["iconPath"] = iconPath;
    obj["customIconPath"] = customIconPath;
    obj["tooltip"] = tooltip;
    obj["useCustomIcon"] = useCustomIcon;
    return obj;
}

void HotkeyButtonItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    hotkeyName = json["hotkeyName"].toString();
    hotkeyContext = json["hotkeyContext"].toString();
    displayName = json["displayName"].toString();
    iconPath = json["iconPath"].toString();
    customIconPath = json["customIconPath"].toString();
    tooltip = json["tooltip"].toString();
    useCustomIcon = json["useCustomIcon"].toBool(false);
}

namespace {

// Key used to remember whether the "toolbar editor was upgraded" notice has been
// shown. Kept in the same settings blob as the configuration so it survives a
// restart and only ever fires once per user.
constexpr const char* kUpgradeNoticeShownKey = "toolbar_upgrade_notice_shown";

// Version 1 stored ItemType as a raw enum index that included Group at 4, so a
// legacy HotkeyButton came through as 5. Anything unrecognised is dropped.
enum class LegacyItemType { Button = 0, Separator = 1, CustomSpacer = 2, DockButton = 3, Group = 4, HotkeyButton = 5 };

std::shared_ptr<ToolbarItem> createItem(ItemType type, const QJsonObject& json) {
    const QString id = json["id"].toString();

    std::shared_ptr<ToolbarItem> item;
    switch (type) {
    case ItemType::Button:
        // pause and save_replay are auto-managed companions of record and
        // replay_buffer, so an explicit item for either is no longer valid.
        if (const QString buttonType = json["buttonType"].toString();
            buttonType == "pause" || buttonType == "save_replay") {
            return nullptr;
        }
        item = std::make_shared<ButtonItem>(id, json["buttonType"].toString());
        break;
    case ItemType::Separator:
        item = std::make_shared<SeparatorItem>(id);
        break;
    case ItemType::CustomSpacer:
        item = std::make_shared<CustomSpacerItem>(id, json["size"].toInt(20));
        break;
    case ItemType::DockButton:
        item = std::make_shared<DockButtonItem>(id, json["dockButtonType"].toString(), json["name"].toString());
        break;
    case ItemType::HotkeyButton:
        item = std::make_shared<HotkeyButtonItem>(id, json["hotkeyName"].toString(), json["displayName"].toString());
        break;
    case ItemType::WebSocketButton:
        item = std::make_shared<WebSocketButtonItem>(id, json["requestType"].toString());
        break;
    case ItemType::StatusItem: {
        // A kind this build does not know about is dropped rather than shown as
        // an empty readout that never updates.
        const QString kind = json["kind"].toString();
        ToolbarStatus::Kind parsed;
        if (!ToolbarStatus::kindFromKey(kind, parsed)) {
            return nullptr;
        }
        item = std::make_shared<StatusItem>(id, kind);
        break;
    }
    }

    if (item) {
        item->fromJson(json);
    }
    return item;
}

// Appends the version 1 array to target, expanding groups in place so their
// children keep the order the user arranged them in.
void migrateLegacyItems(const QJsonArray& array, QList<std::shared_ptr<ToolbarItem>>& target) {
    for (const auto value : array) {
        const QJsonObject itemObj = value.toObject();

        switch (static_cast<LegacyItemType>(itemObj["type"].toInt(-1))) {
        case LegacyItemType::Group:
            migrateLegacyItems(itemObj["childItems"].toArray(), target);
            continue;
        case LegacyItemType::Button:
            if (auto item = createItem(ItemType::Button, itemObj)) target.append(item);
            continue;
        case LegacyItemType::Separator:
            if (auto item = createItem(ItemType::Separator, itemObj)) target.append(item);
            continue;
        case LegacyItemType::CustomSpacer:
            if (auto item = createItem(ItemType::CustomSpacer, itemObj)) target.append(item);
            continue;
        case LegacyItemType::DockButton:
            if (auto item = createItem(ItemType::DockButton, itemObj)) target.append(item);
            continue;
        case LegacyItemType::HotkeyButton:
            if (auto item = createItem(ItemType::HotkeyButton, itemObj)) target.append(item);
            continue;
        default:
            // A type this build no longer knows about, so there is nothing to map it to.
            continue;
        }
    }
}

} // namespace

// ToolbarConfiguration implementation
QJsonObject WebSocketButtonItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["source"] = static_cast<int>(source);
    obj["requestType"] = requestType;
    obj["vendorName"] = vendorName;
    obj["requestData"] = requestData;
    obj["displayName"] = displayName;
    obj["iconPath"] = iconPath;
    obj["customIconPath"] = customIconPath;
    obj["tooltip"] = tooltip;
    obj["useCustomIcon"] = useCustomIcon;
    return obj;
}

void WebSocketButtonItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    // Range-checked: a stored value outside the enum would otherwise be cast
    // into a source that does not exist.
    const int rawSource = json["source"].toInt(static_cast<int>(Source::ObsWebSocket));
    source = (rawSource >= static_cast<int>(Source::ObsWebSocket) && rawSource <= static_cast<int>(Source::Vendor))
                     ? static_cast<Source>(rawSource)
                     : Source::ObsWebSocket;
    requestType = json["requestType"].toString();
    vendorName = json["vendorName"].toString();
    requestData = json["requestData"].toString();
    displayName = json["displayName"].toString();
    iconPath = json["iconPath"].toString();
    customIconPath = json["customIconPath"].toString();
    tooltip = json["tooltip"].toString();
    useCustomIcon = json["useCustomIcon"].toBool();
}

// StatusItem implementation
QJsonObject StatusItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["kind"] = kind;
    obj["showIcon"] = showIcon;
    obj["showHours"] = showHours;
    return obj;
}

void StatusItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    kind = json["kind"].toString();
    showIcon = json["showIcon"].toBool(true);
    showHours = json["showHours"].toBool(false);
}

// Mirrors the registration list in streamup.cpp. It is kept by hand because
// obs-websocket cannot be asked what a vendor has registered, so there is no
// way to derive it at runtime.
QStringList streamUPVendorRequests() {
    static const QStringList requests = {
        "ActivateAllVideoCaptureDevices",
        "CheckRequiredPlugins",
        "CopyHideTransition",
        "CopyShowTransition",
        "CreateBackup",
        "DeactivateAllVideoCaptureDevices",
        "GetAllSourcesLocked",
        "GetBackupInfo",
        "GetBlendingMethod",
        "GetCurrentSceneSourcesLocked",
        "GetDeinterlacing",
        "GetDownmixMono",
        "GetHideTransition",
        "GetPluginVersion",
        "GetRecordingOutputPath",
        "GetScaleFiltering",
        "GetSelectedSource",
        "GetSelectedVisibility",
        "GetShowTransition",
        "GetStreamBitrate",
        "GetVLCCurrentFile",
        "GroupSelectedSources",
        "LoadStreamUpFile",
        "OpenSceneFilters",
        "OpenSourceFilters",
        "OpenSourceInteraction",
        "OpenSourceProperties",
        "PasteHideTransition",
        "PasteShowTransition",
        "RefreshAllVideoCaptureDevices",
        "RefreshAudioMonitoring",
        "RefreshBrowserSources",
        "SetBlendingMethod",
        "SetDeinterlacing",
        "SetDownmixMono",
        "SetHideTransition",
        "SetScaleFiltering",
        "SetShowTransition",
        "ToggleLockAllSources",
        "ToggleLockCurrentSceneSources",
        "ToggleVisibilitySelectedSources",
        "getBitrate",
        "getCurrentSource",
        "getHideTransition",
        "getOutputFilePath",
        "getShowTransition",
        "loadStreamupFile",
        "openSceneFilters",
        "openSourceFilters",
        "openSourceInteract",
        "openSourceProperties",
        "setHideTransition",
        "setShowTransition",
        "toggleLockAllSources",
        "toggleLockCurrentSources",
        "version",
        "vlcGetCurrentFile",
    };
    return requests;
}

bool ToolbarConfiguration::saveToSettings() const {
    obs_data_t* settings = StreamUP::SettingsManager::LoadSettings();
    if (!settings) {
        settings = obs_data_create();
    }
    
    QJsonObject configObj = toJson();
    QJsonDocument doc(configObj);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    
    // Save toolbar configuration to settings
    obs_data_set_string(settings, "toolbar_configuration", jsonString.toUtf8().constData());
    
    bool success = StreamUP::SettingsManager::SaveSettings(settings);
    
    // Invalidate cache since settings changed
    if (success) {
        invalidateCache();
    }
    
    obs_data_release(settings);
    return success;
}

bool ToolbarConfiguration::loadFromSettings() {
    obs_data_t* settings = StreamUP::SettingsManager::LoadSettings();
    if (!settings) {
        // No settings file, use default configuration
        setDefaultConfiguration();
        invalidateCache();
        return true;
    }
    
    const char* jsonString = obs_data_get_string(settings, "toolbar_configuration");
    obs_data_release(settings);
    
    if (!jsonString || strlen(jsonString) == 0) {
        // No saved configuration, use default
        setDefaultConfiguration();
        invalidateCache();
        return true;
    }
    
    // Check if configuration has changed using dirty-flag caching
    QString currentJsonString = QString::fromUtf8(jsonString);
    if (configCacheValid && currentJsonString == lastLoadedJsonString) {
        // Configuration hasn't changed, skip expensive JSON parsing
        return true;
    }
    
    // Parse JSON since configuration changed
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(currentJsonString.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse toolbar configuration:" << error.errorString();
        setDefaultConfiguration();
        invalidateCache();
        return false;
    }
    
    fromJson(doc.object());

    if (migratedOnLoad) {
        // Write the upgraded blob straight back so the migration runs once, and arm
        // the notice so the user is told their layout was carried over.
        obs_data_t* noticeSettings = StreamUP::SettingsManager::LoadSettings();
        if (noticeSettings) {
            obs_data_set_bool(noticeSettings, kUpgradeNoticeShownKey, false);
            StreamUP::SettingsManager::SaveSettings(noticeSettings);
            obs_data_release(noticeSettings);
        }

        saveToSettings();
        return true;
    }

    // Update cache state
    lastLoadedJsonString = currentJsonString;
    configCacheValid = true;

    return true;
}

bool ToolbarConfiguration::consumeMigrationNotice() {
    obs_data_t* settings = StreamUP::SettingsManager::LoadSettings();
    if (!settings) {
        return false;
    }

    // Only an explicit false counts: absence means there was nothing to migrate.
    const bool pending = obs_data_has_user_value(settings, kUpgradeNoticeShownKey) &&
                         !obs_data_get_bool(settings, kUpgradeNoticeShownKey);

    if (pending) {
        obs_data_set_bool(settings, kUpgradeNoticeShownKey, true);
        StreamUP::SettingsManager::SaveSettings(settings);
    }

    obs_data_release(settings);
    return pending;
}

void ToolbarConfiguration::invalidateCache() const {
    configCacheValid = false;
    lastLoadedJsonString.clear();
}

QJsonObject ToolbarConfiguration::toJson() const {
    QJsonObject obj;
    QJsonArray itemsArray;
    
    for (const auto& item : items) {
        itemsArray.append(item->toJson());
    }
    
    obj["items"] = itemsArray;
    obj["version"] = kConfigurationVersion;
    return obj;
}

void ToolbarConfiguration::fromJson(const QJsonObject& json) {
    items.clear();
    migratedOnLoad = false;

    const QJsonArray itemsArray = json["items"].toArray();

    // A missing version means the blob predates versioning entirely.
    if (json["version"].toInt(0) < kConfigurationVersion) {
        migratedOnLoad = true;
        migrateLegacyItems(itemsArray, items);
        return;
    }


    for (const auto value : itemsArray) {
        const QJsonObject itemObj = value.toObject();
        const int rawType = itemObj["type"].toInt(-1);

        if (rawType < static_cast<int>(ItemType::Button) || rawType > static_cast<int>(kLastItemType)) {
            continue;
        }

        if (auto item = createItem(static_cast<ItemType>(rawType), itemObj)) {
            items.append(item);
        }
    }
}

void ToolbarConfiguration::setDefaultConfiguration() {
    items.clear();
    
    // Recreate the default toolbar configuration as defined in setupUI()
    auto builtinButtons = ButtonRegistry::getBuiltinButtons();
    
    // Stream button
    addItem(std::make_shared<ButtonItem>("stream", "stream"));
    addItem(std::make_shared<SeparatorItem>("sep1"));
    
    // Recording section
    addItem(std::make_shared<ButtonItem>("record", "record"));
    // Note: pause button is auto-managed by record button, not configurable
    addItem(std::make_shared<SeparatorItem>("sep2"));
    
    // Replay buffer section
    addItem(std::make_shared<ButtonItem>("replay_buffer", "replay_buffer"));
    // Note: save_replay button is auto-managed by replay_buffer button, not configurable
    addItem(std::make_shared<SeparatorItem>("sep3"));
    
    // Virtual camera section
    addItem(std::make_shared<ButtonItem>("virtual_camera", "virtual_camera"));
    addItem(std::make_shared<ButtonItem>("virtual_camera_config", "virtual_camera_config"));
    addItem(std::make_shared<SeparatorItem>("sep4"));
    
    // Studio mode
    addItem(std::make_shared<ButtonItem>("studio_mode", "studio_mode"));
    addItem(std::make_shared<SeparatorItem>("sep5"));
    
    // Settings
    addItem(std::make_shared<ButtonItem>("settings", "settings"));
    
    // StreamUP settings (special button, always at the end)
    addItem(std::make_shared<ButtonItem>("streamup_settings", "streamup_settings"));
}

void ToolbarConfiguration::addItem(std::shared_ptr<ToolbarItem> item) {
    items.append(item);
}

void ToolbarConfiguration::removeItem(const QString& id) {
    items.removeAll(findItem(id));
}

void ToolbarConfiguration::moveItem(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= items.size() || 
        toIndex < 0 || toIndex >= items.size() || fromIndex == toIndex) {
        return;
    }
    
    items.move(fromIndex, toIndex);
}

std::shared_ptr<ToolbarItem> ToolbarConfiguration::findItem(const QString& id) const {
    for (const auto& item : items) {
        if (item->id == id) {
            return item;
        }
    }
    return nullptr;
}

int ToolbarConfiguration::getItemIndex(const QString& id) const {
    for (int i = 0; i < items.size(); ++i) {
        if (items[i]->id == id) {
            return i;
        }
    }
    return -1;
}

QList<std::shared_ptr<ToolbarItem>> ToolbarConfiguration::getFlattenedItems() const {
    QList<std::shared_ptr<ToolbarItem>> result;
    result.reserve(items.size());

    for (const auto& item : items) {
        // Guard against a null slipping in from a partially parsed configuration.
        if (item) {
            result.append(item);
        }
    }

    return result;
}

QList<DockButtonItem> ToolbarConfiguration::getAvailableDockButtons() {
    QList<DockButtonItem> buttons;
    
    // Lock All Sources - use actual themed icon
    auto lockAllButton = DockButtonItem("dock_lock_all_sources", "lock_all_sources", "Lock All Sources");
    lockAllButton.iconPath = "all-scene-source-locked";
    lockAllButton.tooltip = "Lock All Sources in All Scenes";
    buttons.append(lockAllButton);
    
    // Lock Current Scene Sources - use actual themed icon
    auto lockCurrentButton = DockButtonItem("dock_lock_current_sources", "lock_current_sources", "Lock Sources in Current Scene");
    lockCurrentButton.iconPath = "current-scene-source-locked";
    lockCurrentButton.tooltip = "Lock Sources in Current Scene";
    buttons.append(lockCurrentButton);
    
    // Refresh Audio Monitoring - use actual themed icon
    auto refreshAudioButton = DockButtonItem("dock_refresh_audio", "refresh_audio", "Refresh Audio Monitoring");
    refreshAudioButton.iconPath = "refresh-audio-monitoring";
    refreshAudioButton.tooltip = "Refresh Audio Monitoring";
    buttons.append(refreshAudioButton);
    
    // Refresh Browser Sources - use actual themed icon
    auto refreshBrowserButton = DockButtonItem("dock_refresh_browser", "refresh_browser", "Refresh Browser Sources");
    refreshBrowserButton.iconPath = "refresh-browser-sources";
    refreshBrowserButton.tooltip = "Refresh All Browser Sources";
    buttons.append(refreshBrowserButton);
    
    // Video Capture Controls - use camera icon
    auto videoCaptureButton = DockButtonItem("dock_video_capture", "video_capture", "Video Capture Controls");
    videoCaptureButton.iconPath = "camera";
    videoCaptureButton.tooltip = "Video Capture Controls";
    buttons.append(videoCaptureButton);
    
    // Activate Video Devices - use actual themed icon
    auto activateVideoButton = DockButtonItem("dock_activate_video_devices", "activate_video_devices", "Activate All Video Devices");
    activateVideoButton.iconPath = "video-capture-device-activate";
    activateVideoButton.tooltip = "Activate All Video Capture Devices";
    buttons.append(activateVideoButton);
    
    // Deactivate Video Devices - use actual themed icon
    auto deactivateVideoButton = DockButtonItem("dock_deactivate_video_devices", "deactivate_video_devices", "Deactivate All Video Devices");
    deactivateVideoButton.iconPath = "video-capture-device-deactivate";
    deactivateVideoButton.tooltip = "Deactivate All Video Capture Devices";
    buttons.append(deactivateVideoButton);
    
    // Refresh Video Devices - use actual themed icon
    auto refreshVideoButton = DockButtonItem("dock_refresh_video_devices", "refresh_video_devices", "Refresh All Video Devices");
    refreshVideoButton.iconPath = "video-capture-device-refresh";
    refreshVideoButton.tooltip = "Refresh All Video Capture Devices";
    buttons.append(refreshVideoButton);

    // Group Selected Sources
    auto groupSelectedButton = DockButtonItem("dock_group_selected_sources", "group_selected_sources", "Group Selected Sources");
    groupSelectedButton.iconPath = "add-sources-to-group";
    groupSelectedButton.tooltip = "Group Selected Sources in Current Scene";
    buttons.append(groupSelectedButton);

    // Toggle Visibility of Selected Sources
    auto toggleVisibilityButton = DockButtonItem("dock_toggle_visibility_selected_sources", "toggle_visibility_selected_sources", "Toggle Visibility of Selected Sources");
    toggleVisibilityButton.iconPath = "visible";
    toggleVisibilityButton.tooltip = "Toggle Visibility of Selected Sources";
    buttons.append(toggleVisibilityButton);

    // Note: StreamUP Settings button removed from available dock buttons
    // It's always present on the toolbar by default and doesn't need to be added as a custom button

    return buttons;
}

// ButtonRegistry implementation
QList<BuiltinButtonInfo> ButtonRegistry::getBuiltinButtons() {
    QList<BuiltinButtonInfo> buttons;
    
    buttons.append({"stream", "stream", "Stream", "streaming-inactive", "Start/Stop Streaming", true});
    buttons.append({"record", "record", "Record", "record-off", "Start/Stop Recording", true});
    // Note: pause button is auto-managed by record button, not configurable
    buttons.append({"replay_buffer", "replay_buffer", "Replay Buffer", "replay-buffer-off", "Start/Stop Replay Buffer", true});
    // Note: save_replay button is auto-managed by replay_buffer button, not configurable
    buttons.append({"virtual_camera", "virtual_camera", "Virtual Camera", "virtual-camera", "Start/Stop Virtual Camera", true});
    buttons.append({"virtual_camera_config", "virtual_camera_config", "Virtual Camera Config", "virtual-camera-settings", "Virtual Camera Configuration", false});
    buttons.append({"studio_mode", "studio_mode", "Studio Mode", "studio-mode", "Toggle Studio Mode", true});
    buttons.append({"settings", "settings", "Settings", "settings", "Open Settings", false});
    
    return buttons;
}

BuiltinButtonInfo ButtonRegistry::getButtonInfo(const QString& type) {
    auto buttons = getBuiltinButtons();
    for (const auto& button : buttons) {
        if (button.type == type) {
            return button;
        }
    }
    return BuiltinButtonInfo(); // Return default/empty info if not found
}

} // namespace ToolbarConfig
} // namespace StreamUP
