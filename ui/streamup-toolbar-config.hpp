#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QIcon>
#include <QDateTime>
#include <memory>
#include <functional>

namespace StreamUP {
namespace ToolbarConfig {

// Schema version written by toJson(). Anything older is migrated on load.
constexpr int kConfigurationVersion = 2;

// Note: the stored value is the integer below, so appending only is safe.
// Version 1 blobs used a different numbering (it carried a Group entry between
// DockButton and HotkeyButton), which the migration path remaps.
enum class ItemType {
    Button,
    Separator,
    CustomSpacer,
    DockButton,
    HotkeyButton,
    WebSocketButton,
    StatusItem
};

// The last valid value, used to range-check a stored type integer on load.
// Anything outside the enum is dropped, so this MUST be updated whenever a new
// item type is added, or the new type silently vanishes on the next reload.
constexpr ItemType kLastItemType = ItemType::StatusItem;

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

    DockButtonItem(const QString& itemId, const QString& type, const QString& displayName) 
        : ToolbarItem(ItemType::DockButton, itemId), dockButtonType(type), name(displayName) {}
    
    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// Hotkey buttons that trigger OBS hotkeys
class HotkeyButtonItem : public ToolbarItem {
public:
    QString hotkeyName;        // OBS hotkey internal name (e.g., "OBSBasic.StartStreaming")
    QString hotkeyContext;     // Name of the source that registered it. Source
                               // hotkeys share names ("libobs.mute" on every
                               // audio source), so the name alone picks the
                               // wrong one.
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

// Buttons that fire an obs-websocket request when pressed.
//
// Three sources, because obs-websocket offers no way to ask what is registered:
// the standard request set (a list we ship), StreamUP's own vendor requests
// (registered in streamup.cpp under the vendor name "streamup"), and anything
// else, where the user names the vendor themselves.
class WebSocketButtonItem : public ToolbarItem {
public:
    enum class Source {
        ObsWebSocket, // a standard obs-websocket request
        StreamUP,     // the "streamup" vendor
        Vendor        // some other plugin's vendor, named in vendorName
    };

    Source source = Source::ObsWebSocket;
    QString requestType;       // e.g. "SetCurrentProgramScene"
    QString vendorName;        // only used when source is Vendor
    QString requestData;       // JSON object as text, empty when no arguments
    QString displayName;       // what the user called it
    QString iconPath;
    QString customIconPath;
    QString tooltip;
    bool useCustomIcon = false;

    WebSocketButtonItem(const QString& itemId, const QString& request)
        : ToolbarItem(ItemType::WebSocketButton, itemId), requestType(request) {}

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// A readout from the OBS status bar, placed on the toolbar like any other item.
//
// The kind is stored by name rather than by index, so the list can be reordered
// or added to without rewriting anyone's saved toolbar. An unknown name on load
// drops the item, which is the same treatment an unknown item type gets.
class StatusItem : public ToolbarItem {
public:
    QString kind = QStringLiteral("cpu"); // see ToolbarStatus::kindKey
    bool showLabel = true;                // "CPU: 11%" rather than "11%"
    bool showHours = false;               // durations show hours under an hour

    StatusItem(const QString& itemId, const QString& statusKind)
        : ToolbarItem(ItemType::StatusItem, itemId), kind(statusKind) {}

    QJsonObject toJson() const override;
    void fromJson(const QJsonObject& json) override;
};

// The requests StreamUP registers on its own vendor, so a picker can offer them
// without obs-websocket being able to enumerate anything.
QStringList streamUPVendorRequests();

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
    
    // The item list the toolbar builds from, with unusable entries filtered out
    QList<std::shared_ptr<ToolbarItem>> getFlattenedItems() const;

    // Get available dock buttons
    static QList<DockButtonItem> getAvailableDockButtons();

    // True once after a version 1 config was migrated, so a caller can show the
    // upgrade notice exactly once. The answer is persisted, not just in-memory,
    // so a user who never opens the toolbar this session still sees it later.
    bool consumeMigrationNotice();

private:
    // Configuration caching for performance optimization
    mutable bool configCacheValid = false;
    mutable QString lastLoadedJsonString;

    // Set by fromJson() when the parsed blob was pre-version 2
    bool migratedOnLoad = false;

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
