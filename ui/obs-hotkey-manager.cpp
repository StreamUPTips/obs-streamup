#include "obs-hotkey-manager.hpp"
#include <QDebug>
#include <obs-frontend-api.h>
#include <util/base.h>

namespace StreamUP {

// Static callback for hotkey enumeration
bool OBSHotkeyManager::enumerationCallback(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey) {
    if (!data || !hotkey) return true;
    
    QList<HotkeyInfo>* hotkeyList = static_cast<QList<HotkeyInfo>*>(data);
    
    const char* name = obs_hotkey_get_name(hotkey);
    const char* description = obs_hotkey_get_description(hotkey);
    obs_hotkey_registerer_t registererType = obs_hotkey_get_registerer_type(hotkey);
    
    if (name && description) {
        QString qName = QString::fromUtf8(name);
        QString qDescription = QString::fromUtf8(description);
        
        HotkeyInfo info(qName, qDescription, id, registererType);

        // Every audio source registers hotkeys with identical names
        // ("libobs.mute" and friends), so the name alone cannot tell them
        // apart. The registerer can, and it is only reachable here.
        if (registererType == OBS_HOTKEY_REGISTERER_SOURCE) {
            auto* weakSource = static_cast<obs_weak_source_t*>(obs_hotkey_get_registerer(hotkey));
            if (weakSource) {
                if (obs_source_t* source = obs_weak_source_get_source(weakSource)) {
                    const char* sourceName = obs_source_get_name(source);
                    const char* sourceId = obs_source_get_id(source);
                    info.context = sourceName ? QString::fromUtf8(sourceName) : QString();
                    info.contextType = sourceId ? QString::fromUtf8(sourceId) : QString();
                    obs_source_release(source);
                }
            }
        }

        hotkeyList->append(info);
    }
    
    return true; // Continue enumeration
}

QList<HotkeyInfo> OBSHotkeyManager::getAvailableHotkeys() {
    QList<HotkeyInfo> hotkeys;
    obs_enum_hotkeys(enumerationCallback, &hotkeys);
    return hotkeys;
}

QList<HotkeyInfo> OBSHotkeyManager::getFrontendHotkeys() {
    QList<HotkeyInfo> allHotkeys = getAvailableHotkeys();
    QList<HotkeyInfo> frontendHotkeys;
    
    for (const auto& hotkey : allHotkeys) {
        if (hotkey.registererType == OBS_HOTKEY_REGISTERER_FRONTEND) {
            frontendHotkeys.append(hotkey);
        }
    }
    
    return frontendHotkeys;
}

bool OBSHotkeyManager::triggerHotkey(const QString& hotkeyName) {
    return triggerHotkey(hotkeyName, QString());
}

bool OBSHotkeyManager::triggerHotkey(const QString& hotkeyName, const QString& context) {
    // Match on the registering source as well as the name where we have one.
    // Without it, a button meant to mute one camera fires whichever source's
    // "libobs.mute" happens to enumerate first.
    const QList<HotkeyInfo> hotkeys = getAvailableHotkeys();
    for (const auto& info : hotkeys) {
        if (info.name != hotkeyName)
            continue;
        if (!context.isEmpty() && info.context != context)
            continue;
        return triggerHotkeyById(info.id);
    }

    qWarning() << "[StreamUP] Hotkey not found:" << hotkeyName << "context:" << context;
    return false;
}

bool OBSHotkeyManager::triggerHotkeyById(obs_hotkey_id hotkeyId) {
    if (hotkeyId == OBS_INVALID_HOTKEY_ID) {
        qWarning() << "[StreamUP] Invalid hotkey ID";
        return false;
    }
    
    // Trigger hotkey press and release
    obs_hotkey_trigger_routed_callback(hotkeyId, true);  // Press
    obs_hotkey_trigger_routed_callback(hotkeyId, false); // Release
    
    return true;
}

QString OBSHotkeyManager::getHotkeyDescription(const QString& hotkeyName) {
    obs_hotkey_t* hotkey = findHotkeyByName(hotkeyName);
    if (!hotkey) {
        return QString();
    }
    
    const char* description = obs_hotkey_get_description(hotkey);
    return description ? QString::fromUtf8(description) : QString();
}

bool OBSHotkeyManager::hotkeyExists(const QString& hotkeyName) {
    return findHotkeyByName(hotkeyName) != nullptr;
}

QString OBSHotkeyManager::getDefaultHotkeyIcon(const QString& hotkeyName) {
    // Map common hotkeys to appropriate icons
    static QMap<QString, QString> iconMap = {
        // Basic OBS functions
        {"OBSBasic.StartStreaming", "streaming-inactive"},
        {"OBSBasic.StopStreaming", "streaming"},
        {"OBSBasic.ForceStopStreaming", "streaming"},
        {"OBSBasic.StartRecording", "record-off"},
        {"OBSBasic.StopRecording", "record-on"},
        {"OBSBasic.PauseRecording", "pause"},
        {"OBSBasic.UnpauseRecording", "pause"},
        {"OBSBasic.SplitFile", "save"},
        {"OBSBasic.StartReplayBuffer", "replay-buffer-off"},
        {"OBSBasic.StopReplayBuffer", "replay-buffer-on"},
        {"OBSBasic.SaveReplayBuffer", "save-replay"},
        {"OBSBasic.StartVirtualCam", "virtual-camera"},
        {"OBSBasic.StopVirtualCam", "virtual-camera"},
        {"OBSBasic.EnablePreview", "camera"},
        {"OBSBasic.DisablePreview", "camera"},
        {"OBSBasic.EnablePreviewProgram", "studio-mode"},
        {"OBSBasic.DisablePreviewProgram", "studio-mode"},
        {"OBSBasic.TransitionStudio", "studio-mode"},
        
        // Scene transitions
        {"OBSBasic.Transition", "settings"},
        {"OBSBasic.ResetStats", "refresh"},
        
        // Source controls - use generic icons from OBS
        {"libobs.show_scene_item", "visible"},
        {"libobs.hide_scene_item", "invisible"},
        {"libobs.mute", "mute"},
        {"libobs.unmute", "mute"},
        {"libobs.push_to_mute", "mute"},
        {"libobs.push_to_talk", "mute"},
        
        // StreamUP custom hotkeys
        {"streamup_refresh_browser_sources", "refresh-browser-sources"},
        {"streamup_lock_all_sources", "all-scene-source-locked"},
        {"streamup_refresh_audio_monitoring", "refresh-audio-monitoring"},
        {"streamup_lock_current_sources", "current-scene-source-locked"},
        {"streamup_activate_all_video_capture_devices", "video-capture-device-activate"},
        {"streamup_deactivate_all_video_capture_devices", "video-capture-device-deactivate"},
        {"streamup_refresh_all_video_capture_devices", "video-capture-device-refresh"},
        {"streamup_copy_show_transition", "copy"},
        {"streamup_copy_hide_transition", "copy"},
        {"streamup_paste_show_transition", "paste"},
        {"streamup_paste_hide_transition", "paste"}
    };
    
    // Check exact match first
    if (iconMap.contains(hotkeyName)) {
        return iconMap[hotkeyName];
    }
    
    // Check for partial matches
    if (hotkeyName.contains("stream", Qt::CaseInsensitive)) {
        return "streaming-inactive";
    } else if (hotkeyName.contains("record", Qt::CaseInsensitive)) {
        return "record-off";
    } else if (hotkeyName.contains("replay", Qt::CaseInsensitive)) {
        return "replay-buffer-off";
    } else if (hotkeyName.contains("virtual", Qt::CaseInsensitive) || hotkeyName.contains("camera", Qt::CaseInsensitive)) {
        return "virtual-camera";
    } else if (hotkeyName.contains("studio", Qt::CaseInsensitive) || hotkeyName.contains("preview", Qt::CaseInsensitive)) {
        return "studio-mode";
    } else if (hotkeyName.contains("mute", Qt::CaseInsensitive) || hotkeyName.contains("audio", Qt::CaseInsensitive)) {
        return "mute";
    } else if (hotkeyName.contains("scene", Qt::CaseInsensitive)) {
        return "scenes";
    } else if (hotkeyName.contains("source", Qt::CaseInsensitive)) {
        return "sources";
    } else if (hotkeyName.contains("filter", Qt::CaseInsensitive)) {
        return "filter";
    } else if (hotkeyName.contains("refresh", Qt::CaseInsensitive)) {
        return "refresh";
    } else if (hotkeyName.contains("pause", Qt::CaseInsensitive)) {
        return "pause";
    } else if (hotkeyName.contains("save", Qt::CaseInsensitive)) {
        return "save";
    }
    
    // Default fallback icon
    return "settings";
}

QMap<QString, QList<HotkeyInfo>> OBSHotkeyManager::getCategorizedHotkeys() {
    QList<HotkeyInfo> allHotkeys = getAvailableHotkeys();
    QMap<QString, QList<HotkeyInfo>> categorized;
    
    for (const auto& hotkey : allHotkeys) {
        QString category = getHotkeyCategory(hotkey);
        categorized[category].append(hotkey);
    }
    
    return categorized;
}

obs_hotkey_t* OBSHotkeyManager::findHotkeyByName(const QString& hotkeyName) {
    struct EnumData {
        QString targetName;
        obs_hotkey_t* foundHotkey;
    };
    
    EnumData enumData;
    enumData.targetName = hotkeyName;
    enumData.foundHotkey = nullptr;
    
    auto callback = [](void* data, obs_hotkey_id id, obs_hotkey_t* hotkey) -> bool {
        Q_UNUSED(id);
        EnumData* enumData = static_cast<EnumData*>(data);
        
        const char* name = obs_hotkey_get_name(hotkey);
        if (name && QString::fromUtf8(name) == enumData->targetName) {
            enumData->foundHotkey = hotkey;
            return false; // Stop enumeration
        }
        return true; // Continue enumeration
    };
    
    obs_enum_hotkeys(callback, &enumData);
    return enumData.foundHotkey;
}

QString OBSHotkeyManager::formatHotkeyDescription(obs_hotkey_t* hotkey) {
    if (!hotkey) return QString();
    
    const char* description = obs_hotkey_get_description(hotkey);
    return description ? QString::fromUtf8(description) : QString();
}

QString OBSHotkeyManager::displayLabel(const HotkeyInfo& info) {
    // Where the category already names the source, the plain description is
    // enough ("Mute" under "[CAM] Main"). Scenes and groups are listed flat,
    // so the row has to carry the name or you get a wall of "Switch to scene".
    if (info.registererType == OBS_HOTKEY_REGISTERER_SOURCE && !info.context.isEmpty() &&
        (info.contextType == "scene" || info.contextType == "group")) {
        return QString("%1 — %2").arg(info.context, info.description);
    }
    return info.description;
}

QString OBSHotkeyManager::getHotkeyCategory(const HotkeyInfo& info) {
    const QString& hotkeyName = info.name;

    // Source-registered hotkeys are categorised by their source first, before
    // any name-prefix rules. OBS registers the scene switch hotkey on the scene
    // itself under the name "OBSBasic.SelectScene", so a name-first test threw
    // every scene into one bucket where they were indistinguishable.
    if (info.registererType == OBS_HOTKEY_REGISTERER_SOURCE) {
        // The source is carried on the info, captured while the hotkey was in
        // hand. Looking it up again by name put every source's Mute under
        // whichever source enumerated first.
        const QString displayName = info.context.isEmpty() ? QStringLiteral("Unnamed Source") : info.context;
        const QString sourceType = info.contextType;

        // Scenes get one flat list rather than a group per scene: each scene
        // has a single hotkey, so a group would be a header with one child.
        // The scene name goes on the row itself (see displayLabel).
        if (sourceType == "scene") {
            return QStringLiteral("Scenes");
        }
        if (sourceType == "group") {
            return QStringLiteral("Groups");
        }
        if (sourceType.contains("audio") || sourceType.startsWith("wasapi_") || sourceType.startsWith("pulse_") ||
            sourceType.startsWith("alsa_") || sourceType.startsWith("coreaudio_")) {
            return QString("Sources › Audio › %1").arg(displayName);
        }
        if (sourceType.contains("video") || sourceType == "dshow_input" || sourceType == "v4l2_input" ||
            sourceType == "av_capture_input" || sourceType == "image_source" || sourceType == "slideshow") {
            return QString("Sources › Video › %1").arg(displayName);
        }
        if (sourceType == "browser_source") {
            return QString("Sources › Browser › %1").arg(displayName);
        }
        if (sourceType.contains("text")) {
            return QString("Sources › Text › %1").arg(displayName);
        }
        return QString("Sources › Other › %1").arg(displayName);
    }

    if (hotkeyName.startsWith("OBSBasic.")) {
        if (hotkeyName.contains("Stream")) {
            return "General › Streaming";
        } else if (hotkeyName.contains("Record")) {
            return "General › Recording";
        } else if (hotkeyName.contains("Replay")) {
            return "General › Replay Buffer";
        } else if (hotkeyName.contains("Virtual")) {
            return "General › Virtual Camera";
        } else if (hotkeyName.contains("Studio") || hotkeyName.contains("Preview")) {
            return "General › Studio Mode";
        } else if (hotkeyName.contains("Scene") || hotkeyName.contains("Transition")) {
            return "General › Scenes";
        }
        return "General › Other";
    }

    if (hotkeyName.startsWith("streamup_")) {
        return "Plugins › StreamUP";
    }

    if (hotkeyName.startsWith("libobs.")) {
        return "Sources › Other";
    }

    return "Plugins › Other";
}

} // namespace StreamUP
