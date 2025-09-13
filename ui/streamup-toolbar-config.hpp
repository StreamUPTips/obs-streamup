#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QIcon>
#include <QDateTime>
#include <memory>
#include <functional>

namespace StreamUP {
namespace ToolbarConfig {

enum class ItemType {
    Button,
    Separator,
    CustomSpacer,
    DockButton,
    Group,
    HotkeyButton
};

// Base class for all toolbar items
class ToolbarItem {
public:
    ItemType type;
    QString id;
    bool visible = true;
    
    ToolbarItem(ItemType t, const QString& itemId) : type(t), id(itemId) {}
    virtual ~ToolbarItem() = default;
    
    virtual QJsonObject toJson() const;
    virtual void fromJson(const QJsonObject& json);
};

// Built-in OBS/StreamUp buttons (stream, record, etc.)
class ButtonItem : public ToolbarItem {
public:
    QString buttonType; // "stream", "record", "pause", etc.
    QString iconPath;
    QString tooltip;
    bool checkable = false;
    
    ButtonItem(const QString& itemId, const QString& type) 
        : ToolbarItem(ItemType::Button, itemId), buttonType(type) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// Separator items
class SeparatorItem : public ToolbarItem {
public:
    SeparatorItem(const QString& itemId) 
        : ToolbarItem(ItemType::Separator, itemId) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// Custom spacer items with configurable size
class CustomSpacerItem : public ToolbarItem {
public:
    int size = 20; // Size in pixels
    bool isStretch = false; // If true, uses stretch instead of fixed size
    
    CustomSpacerItem(const QString& itemId, int spacerSize = 20) 
        : ToolbarItem(ItemType::CustomSpacer, itemId), size(spacerSize) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// StreamUp dock buttons that can be added to toolbar
class DockButtonItem : public ToolbarItem {
public:
    QString dockButtonType; // "lock_sources", "refresh_audio", etc.
    QString name;
    QString iconPath;
    QString tooltip;
    std::function<void()> callback;
    
    DockButtonItem(const QString& itemId, const QString& type, const QString& displayName) 
        : ToolbarItem(ItemType::DockButton, itemId), dockButtonType(type), name(displayName) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// Hotkey buttons that trigger OBS hotkeys
class HotkeyButtonItem : public ToolbarItem {
public:
    QString hotkeyName;        // OBS hotkey internal name (e.g., "OBSBasic.StartStreaming")
    QString displayName;       // User-friendly name for the hotkey
    QString iconPath;          // Icon for the button (from available icons)
    QString customIconPath;    // User-uploaded custom icon path
    QString tooltip;           // Button tooltip
    bool useCustomIcon = false; // Whether to use custom icon vs default/selected icon
    
    HotkeyButtonItem(const QString& itemId, const QString& hotkey, const QString& display) 
        : ToolbarItem(ItemType::HotkeyButton, itemId), hotkeyName(hotkey), displayName(display) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// Group items for configuration UI organization (doesn't affect actual toolbar)
class GroupItem : public ToolbarItem {
public:
    QString name;
    QList<std::shared_ptr<ToolbarItem>> childItems;
    bool expanded = true; // For UI display
    
    GroupItem(const QString& itemId, const QString& groupName) 
        : ToolbarItem(ItemType::Group, itemId), name(groupName) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
    
    // Utility methods for managing child items
    void addChild(std::shared_ptr<ToolbarItem> child);
    void removeChild(const QString& childId);
    void moveChild(int fromIndex, int toIndex);
    std::shared_ptr<ToolbarItem> findChild(const QString& childId) const;
    int getChildIndex(const QString& childId) const;
};

// Main configuration class
class ToolbarConfiguration {
public:
    QList<std::shared_ptr<ToolbarItem>> items;
    
    // Save/load configuration
    bool saveToSettings() const;
    bool loadFromSettings();
    
    // JSON serialization
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
    
    // Default configuration
    void setDefaultConfiguration();
    
    // Utility methods
    void addItem(std::shared_ptr<ToolbarItem> item);
    void removeItem(const QString& id);
    void moveItem(int fromIndex, int toIndex);
    std::shared_ptr<ToolbarItem> findItem(const QString& id) const;
    int getItemIndex(const QString& id) const;
    
    // Group-specific utility methods
    void addItemToGroup(const QString& groupId, std::shared_ptr<ToolbarItem> item);
    void removeItemFromGroup(const QString& groupId, const QString& itemId);
    void moveItemToGroup(const QString& itemId, const QString& targetGroupId);
    void moveItemOutOfGroup(const QString& itemId);
    QList<std::shared_ptr<ToolbarItem>> getFlattenedItems() const; // Get all items ignoring groups
    
    // Get available dock buttons
    static QList<DockButtonItem> getAvailableDockButtons();
    
private:
    // Configuration caching for performance optimization
    mutable bool configCacheValid = false;
    mutable QString lastLoadedJsonString;
    
    // Mark configuration as needing reload
    void invalidateCache() const;
};

// Available built-in button types
struct BuiltinButtonInfo {
    QString id;
    QString type;
    QString displayName;
    QString defaultIcon;
    QString defaultTooltip;
    bool checkable;
};

// Registry of all available button types
class ButtonRegistry {
public:
    static QList<BuiltinButtonInfo> getBuiltinButtons();
    static BuiltinButtonInfo getButtonInfo(const QString& type);
};

} // namespace ToolbarConfig
} // namespace StreamUP