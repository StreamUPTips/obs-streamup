#include "streamup-toolbar-config.hpp"
#include "settings-manager.hpp"
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
    obj["isStretch"] = isStretch;
    return obj;
}

void CustomSpacerItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    size = json["size"].toInt(20);
    isStretch = json["isStretch"].toBool(false);
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

// ToolbarConfiguration implementation
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
    obs_data_release(settings);
    return success;
}

bool ToolbarConfiguration::loadFromSettings() {
    obs_data_t* settings = StreamUP::SettingsManager::LoadSettings();
    if (!settings) {
        // No settings file, use default configuration
        setDefaultConfiguration();
        return true;
    }
    
    const char* jsonString = obs_data_get_string(settings, "toolbar_configuration");
    obs_data_release(settings);
    
    if (!jsonString || strlen(jsonString) == 0) {
        // No saved configuration, use default
        setDefaultConfiguration();
        return true;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonString), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse toolbar configuration:" << error.errorString();
        setDefaultConfiguration();
        return false;
    }
    
    fromJson(doc.object());
    return true;
}

QJsonObject ToolbarConfiguration::toJson() const {
    QJsonObject obj;
    QJsonArray itemsArray;
    
    for (const auto& item : items) {
        itemsArray.append(item->toJson());
    }
    
    obj["items"] = itemsArray;
    obj["version"] = 1;
    return obj;
}

void ToolbarConfiguration::fromJson(const QJsonObject& json) {
    items.clear();
    
    QJsonArray itemsArray = json["items"].toArray();
    for (const auto& value : itemsArray) {
        QJsonObject itemObj = value.toObject();
        ItemType type = static_cast<ItemType>(itemObj["type"].toInt());
        QString id = itemObj["id"].toString();
        
        std::shared_ptr<ToolbarItem> item;
        
        switch (type) {
        case ItemType::Button: {
            QString buttonType = itemObj["buttonType"].toString();
            auto buttonItem = std::make_shared<ButtonItem>(id, buttonType);
            buttonItem->fromJson(itemObj);
            item = buttonItem;
            break;
        }
        case ItemType::Separator: {
            auto separatorItem = std::make_shared<SeparatorItem>(id);
            separatorItem->fromJson(itemObj);
            item = separatorItem;
            break;
        }
        case ItemType::CustomSpacer: {
            int size = itemObj["size"].toInt(20);
            auto spacerItem = std::make_shared<CustomSpacerItem>(id, size);
            spacerItem->fromJson(itemObj);
            item = spacerItem;
            break;
        }
        case ItemType::DockButton: {
            QString dockType = itemObj["dockButtonType"].toString();
            QString name = itemObj["name"].toString();
            auto dockItem = std::make_shared<DockButtonItem>(id, dockType, name);
            dockItem->fromJson(itemObj);
            item = dockItem;
            break;
        }
        }
        
        if (item) {
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
    addItem(std::make_shared<ButtonItem>("pause", "pause"));
    addItem(std::make_shared<SeparatorItem>("sep2"));
    
    // Replay buffer section
    addItem(std::make_shared<ButtonItem>("replay_buffer", "replay_buffer"));
    addItem(std::make_shared<ButtonItem>("save_replay", "save_replay"));
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
    
    // StreamUP Settings - use StreamUP logo icon
    auto streamupSettingsButton = DockButtonItem("dock_streamup_settings", "streamup_settings", "Open StreamUP Settings");
    streamupSettingsButton.iconPath = "streamup-logo-button";
    streamupSettingsButton.tooltip = "Open StreamUP Settings";
    buttons.append(streamupSettingsButton);
    
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