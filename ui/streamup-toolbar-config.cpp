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

// HotkeyButtonItem implementation
QJsonObject HotkeyButtonItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["hotkeyName"] = hotkeyName;
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
    displayName = json["displayName"].toString();
    iconPath = json["iconPath"].toString();
    customIconPath = json["customIconPath"].toString();
    tooltip = json["tooltip"].toString();
    useCustomIcon = json["useCustomIcon"].toBool(false);
}

// GroupItem implementation
QJsonObject GroupItem::toJson() const {
    QJsonObject obj = ToolbarItem::toJson();
    obj["name"] = name;
    obj["expanded"] = expanded;
    
    QJsonArray childArray;
    for (const auto& child : childItems) {
        childArray.append(child->toJson());
    }
    obj["childItems"] = childArray;
    
    return obj;
}

void GroupItem::fromJson(const QJsonObject& json) {
    ToolbarItem::fromJson(json);
    name = json["name"].toString();
    expanded = json["expanded"].toBool(true);
    
    childItems.clear();
    QJsonArray childArray = json["childItems"].toArray();
    
    for (const auto& childValue : childArray) {
        QJsonObject childObj = childValue.toObject();
        ItemType childType = static_cast<ItemType>(childObj["type"].toInt());
        
        std::shared_ptr<ToolbarItem> childItem;
        switch (childType) {
            case ItemType::Button:
                childItem = std::make_shared<ButtonItem>("", "");
                break;
            case ItemType::Separator:
                childItem = std::make_shared<SeparatorItem>("");
                break;
            case ItemType::CustomSpacer:
                childItem = std::make_shared<CustomSpacerItem>("", 20);
                break;
            case ItemType::DockButton:
                childItem = std::make_shared<DockButtonItem>("", "", "");
                break;
            case ItemType::Group:
                childItem = std::make_shared<GroupItem>("", "");
                break;
            case ItemType::HotkeyButton:
                childItem = std::make_shared<HotkeyButtonItem>("", "", "");
                break;
        }
        
        if (childItem) {
            childItem->fromJson(childObj);
            childItems.append(childItem);
        }
    }
}

void GroupItem::addChild(std::shared_ptr<ToolbarItem> child) {
    if (child) {
        childItems.append(child);
    }
}

void GroupItem::removeChild(const QString& childId) {
    for (int i = 0; i < childItems.size(); ++i) {
        if (childItems[i]->id == childId) {
            childItems.removeAt(i);
            break;
        }
    }
}

void GroupItem::moveChild(int fromIndex, int toIndex) {
    if (fromIndex >= 0 && fromIndex < childItems.size() && 
        toIndex >= 0 && toIndex < childItems.size() && 
        fromIndex != toIndex) {
        
        auto item = childItems.takeAt(fromIndex);
        childItems.insert(toIndex, item);
    }
}

std::shared_ptr<ToolbarItem> GroupItem::findChild(const QString& childId) const {
    for (const auto& child : childItems) {
        if (child->id == childId) {
            return child;
        }
        // Recursively search in nested groups
        if (child->type == ItemType::Group) {
            auto groupChild = std::static_pointer_cast<GroupItem>(child);
            auto found = groupChild->findChild(childId);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}

int GroupItem::getChildIndex(const QString& childId) const {
    for (int i = 0; i < childItems.size(); ++i) {
        if (childItems[i]->id == childId) {
            return i;
        }
    }
    return -1;
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
    
    // Update cache state
    lastLoadedJsonString = currentJsonString;
    configCacheValid = true;
    
    return true;
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
            
            // Filter out old explicit pause/save_replay items - they are now auto-managed
            if (buttonType == "pause" || buttonType == "save_replay") {
                qInfo() << "[StreamUP] Filtering out old explicit" << buttonType << "button - now auto-managed by parent button";
                continue; // Skip this item
            }
            
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
        case ItemType::Group: {
            QString name = itemObj["name"].toString();
            auto groupItem = std::make_shared<GroupItem>(id, name);
            groupItem->fromJson(itemObj);
            item = groupItem;
            break;
        }
        case ItemType::HotkeyButton: {
            QString hotkeyName = itemObj["hotkeyName"].toString();
            QString displayName = itemObj["displayName"].toString();
            auto hotkeyItem = std::make_shared<HotkeyButtonItem>(id, hotkeyName, displayName);
            hotkeyItem->fromJson(itemObj);
            item = hotkeyItem;
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

void ToolbarConfiguration::addItemToGroup(const QString& groupId, std::shared_ptr<ToolbarItem> item) {
    if (!item) return;
    
    // Find the group
    for (auto& existingItem : items) {
        if (existingItem->type == ItemType::Group && existingItem->id == groupId) {
            auto group = std::static_pointer_cast<GroupItem>(existingItem);
            group->addChild(item);
            invalidateCache();
            return;
        }
    }
}

void ToolbarConfiguration::removeItemFromGroup(const QString& groupId, const QString& itemId) {
    // Find the group
    for (auto& existingItem : items) {
        if (existingItem->type == ItemType::Group && existingItem->id == groupId) {
            auto group = std::static_pointer_cast<GroupItem>(existingItem);
            group->removeChild(itemId);
            invalidateCache();
            return;
        }
    }
}

void ToolbarConfiguration::moveItemToGroup(const QString& itemId, const QString& targetGroupId) {
    // Find the item first
    std::shared_ptr<ToolbarItem> itemToMove = findItem(itemId);
    if (!itemToMove) return;
    
    // Remove from current location
    removeItem(itemId);
    
    // Add to target group
    addItemToGroup(targetGroupId, itemToMove);
}

void ToolbarConfiguration::moveItemOutOfGroup(const QString& itemId) {
    // Search for the item in all groups and remove it
    for (auto& existingItem : items) {
        if (existingItem->type == ItemType::Group) {
            auto group = std::static_pointer_cast<GroupItem>(existingItem);
            auto foundItem = group->findChild(itemId);
            if (foundItem) {
                group->removeChild(itemId);
                // Add to main items list
                items.append(foundItem);
                invalidateCache();
                return;
            }
        }
    }
}

QList<std::shared_ptr<ToolbarItem>> ToolbarConfiguration::getFlattenedItems() const {
    QList<std::shared_ptr<ToolbarItem>> result;
    
    for (const auto& item : items) {
        if (item->type == ItemType::Group) {
            auto group = std::static_pointer_cast<GroupItem>(item);
            // Add all child items from the group (but not the group itself)
            result.append(group->childItems);
        } else {
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