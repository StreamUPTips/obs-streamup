// Core plugin headers
#include "obs-websocket-api.h"
#include "streamup.hpp"
#include "version.h"

// Core functionality modules
#include "core/streamup-common.hpp"
#include "core/plugin-state.hpp"
#include "core/plugin-manager.hpp"
#include "integrations/websocket-api.hpp"
#include "utilities/path-utils.hpp"
#include "utilities/debug-logger.hpp"

// UI modules
#include "ui/dock/streamup-dock.hpp"
#include "ui/scene-organiser/scene-organiser-dock.hpp"
#include "ui/streamup-toolbar.hpp"
#include "ui/settings-manager.hpp"
#include "ui/ui-helpers.hpp"
#include "ui/ui-styles.hpp"
#include "ui/menu-manager.hpp"
#include "ui/notification-manager.hpp"
#include "ui/hotkey-manager.hpp"
#include "ui/splash-screen.hpp"
#include "ui/patch-notes-window.hpp"
#include "multidock/multidock_manager.hpp"

// Standard library
#include <filesystem>
#include <string>
#include <regex>
#include <thread>

// OBS headers
#include <obs.h>
#include <obs-data.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

// Qt headers - only what's needed for this file
#include <QDockWidget>
#include <QMainWindow>
#include <QTimer>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QStyle>
#include <QObject>
#include <QApplication>
#include <QThread>


OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Andilippi");
OBS_MODULE_USE_DEFAULT_LOCALE("streamup", "en-US")

void CreateToolDialog(const char *infoText1, const char *infoText2, const char *infoText3, const QString &titleText,
		      const std::function<void()> &buttonCallback, const QString &jsonString, const char *how1, const char *how2,
		      const char *how3, const char *how4, const char *notificationMessage)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([infoText1, infoText2, infoText3, titleText, buttonCallback, jsonString, how1, how2, how3, how4,
			      notificationMessage]() {
		const char *titleTextChar = titleText.toUtf8().constData();
		QString titleStr = obs_module_text(titleTextChar);
		QString infoText1Str = obs_module_text(infoText1);
		QString infoText2Str = obs_module_text(infoText2);
		QString infoText3Str = obs_module_text(infoText3);
		QString howTo1Str = obs_module_text(how1);
		QString howTo2Str = obs_module_text(how2);
		QString howTo3Str = obs_module_text(how3);
		QString howTo4Str = obs_module_text(how4);

		QDialog *dialog = StreamUP::UIHelpers::CreateDialogWindow(titleTextChar);
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
		                                 StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::SPACING_SMALL);

		QHBoxLayout *buttonLayout = new QHBoxLayout();

		StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("UI.Button.Cancel"), [dialog]() { dialog->close(); });

		StreamUP::UIHelpers::CreateButton(buttonLayout, titleStr, [=]() {
			buttonCallback();
			if (notificationMessage) {
				StreamUP::NotificationManager::SendInfoNotification(titleStr, obs_module_text(notificationMessage));
			}
			dialog->close();
		});

		dialogLayout->addLayout(StreamUP::UIHelpers::AddIconAndText(QStyle::SP_MessageBoxInformation, infoText1));
		dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

		QLabel *info2 = StreamUP::UIHelpers::CreateRichTextLabel(infoText2Str, false, true, Qt::AlignTop);
		dialogLayout->addWidget(info2, 0, Qt::AlignTop);
		dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

		QGroupBox *info3Box = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("UI.Message.HowToUse"), "info");
		info3Box->setMinimumWidth(350); // Keep specific width requirement for this dialog
		QVBoxLayout *info3BoxLayout = StreamUP::UIHelpers::CreateVBoxLayout(info3Box);
		QLabel *info3 = StreamUP::UIHelpers::CreateRichTextLabel(infoText3Str, false, true);
		QLabel *howTo1 = StreamUP::UIHelpers::CreateRichTextLabel(howTo1Str, false, true);
		QLabel *howTo2 = StreamUP::UIHelpers::CreateRichTextLabel(howTo2Str, false, true);
		QLabel *howTo3 = StreamUP::UIHelpers::CreateRichTextLabel(howTo3Str, false, true);
		QLabel *howTo4 = StreamUP::UIHelpers::CreateRichTextLabel(howTo4Str, false, true);
		info3BoxLayout->addWidget(info3);
		info3BoxLayout->addSpacing(5);
		info3BoxLayout->addWidget(howTo1);
		info3BoxLayout->addWidget(howTo2);
		info3BoxLayout->addWidget(howTo3);
		info3BoxLayout->addWidget(howTo4);

		QPushButton *copyJsonButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WebSocket.Button.Copy"), "neutral");
		copyJsonButton->setToolTip(obs_module_text("WebSocket.Button.CopyTooltip"));
		QObject::connect(copyJsonButton, &QPushButton::clicked, [=]() { StreamUP::UIHelpers::CopyToClipboard(jsonString); });
		info3BoxLayout->addWidget(copyJsonButton);
		dialogLayout->addWidget(info3Box);
		dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

//-------------------PLUGIN MANAGEMENT AND SETTINGS-------------------

std::string SearchStringInFile(const char *path, const char *search)
{
	std::string filepath = StreamUP::PathUtils::GetMostRecentTxtFile(path);
	FILE *file = fopen(filepath.c_str(), "r+");
	constexpr size_t LINE_BUFFER_SIZE = 256;
	char line[LINE_BUFFER_SIZE];
	std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	std::regex version_regex_double("[0-9]+\\.[0-9]+");
	std::regex version_regex_single("[0-9]+");

	if (file) {
		while (fgets(line, LINE_BUFFER_SIZE, file)) {
			char *found_ptr = strstr(line, search);
			if (found_ptr) {
				std::string remaining_line = std::string(found_ptr + strlen(search));
				std::smatch match;

				if (std::regex_search(remaining_line, match, version_regex_triple) && match.size() > 0) {
					fclose(file);
					return match.str(0);
				}

				if (std::regex_search(remaining_line, match, version_regex_double) && match.size() > 0) {
					fclose(file);
					return match.str(0);
				}

				if (std::regex_search(remaining_line, match, version_regex_single) && match.size() > 0) {
					fclose(file);
					return match.str(0);
				}
			}
		}
		fclose(file);
	} else {
		StreamUP::DebugLogger::LogErrorFormat("FileAccess", "Failed to open file: %s", filepath.c_str());
	}

	return "";
}

std::vector<std::pair<std::string, std::string>> GetInstalledPlugins()
{
	std::vector<std::pair<std::string, std::string>> installedPlugins;
	char *filepath = StreamUP::PathUtils::GetOBSLogPath();
	if (filepath == nullptr) {
		return installedPlugins;
	}

	const auto& allPlugins = StreamUP::GetAllPlugins();
	for (const auto &module : allPlugins) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &search_string = plugin_info.searchString;

		std::string installed_version = SearchStringInFile(filepath, search_string.c_str());

		if (!installed_version.empty()) {
			installedPlugins.emplace_back(plugin_name, installed_version);
		}
	}

	bfree(filepath);

	return installedPlugins;
}

//-------------------- HELPER FUNCTIONS--------------------

void GetShowHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data, bool transition_type)
{
	UNUSED_PARAMETER(private_data);

	const char *scene_name = obs_data_get_string(request_data, "sceneName");
	const char *source_name = obs_data_get_string(request_data, "sourceName");

	obs_source_t *scene_source = obs_get_source_by_name(scene_name);
	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found.");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	obs_sceneitem_t *scene_item = obs_scene_find_source(scene, source_name);
	if (!scene_item) {
		obs_data_set_string(response_data, "error", "Source not found in scene.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	// Get the transition for the scene item
	obs_source_t *transition = obs_sceneitem_get_transition(scene_item, transition_type);
	if (!transition) {
		obs_data_set_string(response_data, "error", "No transition set for this item.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	// Fetch transition settings
	obs_data_t *settings = obs_source_get_settings(transition);
	if (!settings) {
		StreamUP::DebugLogger::LogWarningFormat("Transitions", "Failed to get settings for transition: %s", obs_source_get_name(transition));
		obs_data_set_string(response_data, "error", "Failed to get transition settings.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	// Fetch the transition duration
	uint32_t transition_duration = obs_sceneitem_get_transition_duration(scene_item, transition_type);

	// Get the transition's display name using obs_source_get_display_name, which provides the user-friendly name
	const char *transition_display_name = obs_source_get_display_name(obs_source_get_id(transition));

	// Return the display name in "transitionType" (this replaces the internal ID)
	obs_data_set_string(response_data, "transitionType", transition_display_name);

	// Add transition settings and duration to the response
	obs_data_set_obj(response_data, "transitionSettings", settings);
	obs_data_set_int(response_data, "transitionDuration", transition_duration);

	obs_data_set_bool(response_data, "success", true);

	obs_source_release(scene_source);
	obs_data_release(settings);
}

const char *GetTransitionIDFromDisplayName(const char *display_name)
{
	const char *possible_transitions[] = {"cut_transition",   "fade_transition",        "swipe_transition",
					      "slide_transition", "obs_stinger_transition", "fade_to_color_transition",
					      "wipe_transition",  "scene_as_transition",    "move_transition",
					      "shader_transition"};

	// Iterate through all known transition types
	for (size_t i = 0; i < sizeof(possible_transitions) / sizeof(possible_transitions[0]); ++i) {
		const char *transition_id = possible_transitions[i];

		// Fetch the display name for this transition type
		const char *transition_display_name = obs_source_get_display_name(transition_id);

		// Check if the display name is valid before calling strcmp
		if (transition_display_name == NULL) {
			StreamUP::DebugLogger::LogWarningFormat("Transitions", "Failed to get display name for transition ID: %s", transition_id);
			continue; // Skip to the next transition type
		}

		// Compare the provided display name with the translated display name
		if (strcmp(display_name, transition_display_name) == 0) {
			return transition_id; // Return the corresponding internal ID
		}
	}

	return NULL; // If no matching transition type is found
}

void SetShowHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data, bool show_transition)
{
	UNUSED_PARAMETER(private_data);

	const char *scene_name = obs_data_get_string(request_data, "sceneName");
	const char *source_name = obs_data_get_string(request_data, "sourceName");
	const char *transition_display_name = obs_data_get_string(request_data, "transitionType"); // Expecting display name
	obs_data_t *transition_settings = obs_data_get_obj(request_data, "transitionSettings");
	uint32_t transition_duration = obs_data_get_int(request_data, "transitionDuration");

	// Find the internal transition ID from the display name
	const char *transition_type = GetTransitionIDFromDisplayName(transition_display_name);
	if (!transition_type) {
		obs_data_set_string(response_data, "error", "Invalid transition display name.");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_source_t *scene_source = obs_get_source_by_name(scene_name);
	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found.");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	obs_sceneitem_t *scene_item = obs_scene_find_source(scene, source_name);
	if (!scene_item) {
		obs_data_set_string(response_data, "error", "Source not found in scene.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	// Create the transition using the internal transition ID (from the display name)
	obs_source_t *transition = obs_source_create_private(transition_type, "Scene Transition", NULL);
	if (!transition) {
		obs_data_set_string(response_data, "error", "Unable to create transition of specified type.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	if (transition_settings) {
		obs_source_update(transition, transition_settings);
	}

	// Set the transition and its duration
	obs_sceneitem_set_transition(scene_item, show_transition, transition);
	obs_sceneitem_set_transition_duration(scene_item, show_transition, transition_duration);

	obs_data_set_bool(response_data, "success", true);

	obs_source_release(transition);
	obs_source_release(scene_source);
}

//--------------------WEBSOCKET VENDOR REQUESTS--------------------
obs_websocket_vendor vendor = nullptr;

void SettingsDialog()
{
	// Settings dialog functionality moved to StreamUP::SettingsManager module
	StreamUP::SettingsManager::ShowSettingsDialog();
}

//--------------------WEBSOCKET REGISTRATION--------------------
static void RegisterWebsocketRequests()
{
	blog(LOG_INFO, "[StreamUP] RegisterWebsocketRequests: Starting WebSocket vendor registration");
	vendor = obs_websocket_register_vendor("streamup");
	if (!vendor) {
		blog(LOG_WARNING, "[StreamUP] RegisterWebsocketRequests: Failed to register WebSocket vendor - obs-websocket may not be available");
		return;
	}
	blog(LOG_INFO, "[StreamUP] RegisterWebsocketRequests: WebSocket vendor registered successfully");

	// Register new properly named commands (PascalCase following OBS WebSocket conventions)
	// Utility commands
	obs_websocket_vendor_register_request(vendor, "GetStreamBitrate", StreamUP::WebSocketAPI::WebsocketRequestBitrate, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetPluginVersion", StreamUP::WebSocketAPI::WebsocketRequestVersion, nullptr);
	
	// Plugin management
	obs_websocket_vendor_register_request(vendor, "CheckRequiredPlugins", StreamUP::WebSocketAPI::WebsocketRequestCheckPlugins, nullptr);
	
	// Source management
	obs_websocket_vendor_register_request(vendor, "ToggleLockAllSources", StreamUP::WebSocketAPI::WebsocketRequestLockAllSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "ToggleLockCurrentSceneSources", StreamUP::WebSocketAPI::WebsocketRequestLockCurrentSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "RefreshAudioMonitoring", StreamUP::WebSocketAPI::WebsocketRequestRefreshAudioMonitoring, nullptr);
	obs_websocket_vendor_register_request(vendor, "RefreshBrowserSources", StreamUP::WebSocketAPI::WebsocketRequestRefreshBrowserSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetSelectedSource", StreamUP::WebSocketAPI::WebsocketRequestGetCurrentSelectedSource, nullptr);
	
	// Transition management
	obs_websocket_vendor_register_request(vendor, "GetShowTransition", StreamUP::WebSocketAPI::WebsocketRequestGetShowTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetHideTransition", StreamUP::WebSocketAPI::WebsocketRequestGetHideTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetShowTransition", StreamUP::WebSocketAPI::WebsocketRequestSetShowTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetHideTransition", StreamUP::WebSocketAPI::WebsocketRequestSetHideTransition, nullptr);
	
	// File and output management
	obs_websocket_vendor_register_request(vendor, "GetRecordingOutputPath", StreamUP::WebSocketAPI::WebsocketRequestGetOutputFilePath, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetVLCCurrentFile", StreamUP::WebSocketAPI::WebsocketRequestVLCGetCurrentFile, nullptr);
	obs_websocket_vendor_register_request(vendor, "LoadStreamUpFile", StreamUP::WebSocketAPI::WebsocketLoadStreamupFile, nullptr);
	
	// Source properties
	obs_websocket_vendor_register_request(vendor, "GetBlendingMethod", StreamUP::WebSocketAPI::WebsocketRequestGetBlendingMethod, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetBlendingMethod", StreamUP::WebSocketAPI::WebsocketRequestSetBlendingMethod, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetDeinterlacing", StreamUP::WebSocketAPI::WebsocketRequestGetDeinterlacing, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetDeinterlacing", StreamUP::WebSocketAPI::WebsocketRequestSetDeinterlacing, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetScaleFiltering", StreamUP::WebSocketAPI::WebsocketRequestGetScaleFiltering, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetScaleFiltering", StreamUP::WebSocketAPI::WebsocketRequestSetScaleFiltering, nullptr);
	obs_websocket_vendor_register_request(vendor, "GetDownmixMono", StreamUP::WebSocketAPI::WebsocketRequestGetDownmixMono, nullptr);
	obs_websocket_vendor_register_request(vendor, "SetDownmixMono", StreamUP::WebSocketAPI::WebsocketRequestSetDownmixMono, nullptr);
	
	// UI interaction
	obs_websocket_vendor_register_request(vendor, "OpenSourceProperties", StreamUP::WebSocketAPI::WebsocketOpenSourceProperties, nullptr);
	obs_websocket_vendor_register_request(vendor, "OpenSourceFilters", StreamUP::WebSocketAPI::WebsocketOpenSourceFilters, nullptr);
	obs_websocket_vendor_register_request(vendor, "OpenSourceInteraction", StreamUP::WebSocketAPI::WebsocketOpenSourceInteract, nullptr);
	obs_websocket_vendor_register_request(vendor, "OpenSceneFilters", StreamUP::WebSocketAPI::WebsocketOpenSceneFilters, nullptr);
	
	// Video capture device management
	obs_websocket_vendor_register_request(vendor, "ActivateAllVideoCaptureDevices", StreamUP::WebSocketAPI::WebsocketActivateAllVideoCaptureDevices, nullptr);
	obs_websocket_vendor_register_request(vendor, "DeactivateAllVideoCaptureDevices", StreamUP::WebSocketAPI::WebsocketDeactivateAllVideoCaptureDevices, nullptr);
	obs_websocket_vendor_register_request(vendor, "RefreshAllVideoCaptureDevices", StreamUP::WebSocketAPI::WebsocketRefreshAllVideoCaptureDevices, nullptr);
	
	// Backward compatibility - register old command names (deprecated)
	obs_websocket_vendor_register_request(vendor, "getOutputFilePath", StreamUP::WebSocketAPI::WebsocketRequestGetOutputFilePath, nullptr);
	obs_websocket_vendor_register_request(vendor, "getCurrentSource", StreamUP::WebSocketAPI::WebsocketRequestGetCurrentSelectedSource, nullptr);
	obs_websocket_vendor_register_request(vendor, "getShowTransition", StreamUP::WebSocketAPI::WebsocketRequestGetShowTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "getHideTransition", StreamUP::WebSocketAPI::WebsocketRequestGetHideTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "setShowTransition", StreamUP::WebSocketAPI::WebsocketRequestSetShowTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "setHideTransition", StreamUP::WebSocketAPI::WebsocketRequestSetHideTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "toggleLockCurrentSources", StreamUP::WebSocketAPI::WebsocketRequestLockCurrentSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "toggleLockAllSources", StreamUP::WebSocketAPI::WebsocketRequestLockAllSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "getBitrate", StreamUP::WebSocketAPI::WebsocketRequestBitrate, nullptr);
	obs_websocket_vendor_register_request(vendor, "version", StreamUP::WebSocketAPI::WebsocketRequestVersion, nullptr);
	obs_websocket_vendor_register_request(vendor, "check_plugins", StreamUP::WebSocketAPI::WebsocketRequestCheckPlugins, nullptr);
	obs_websocket_vendor_register_request(vendor, "refresh_audio_monitoring", StreamUP::WebSocketAPI::WebsocketRequestRefreshAudioMonitoring, nullptr);
	obs_websocket_vendor_register_request(vendor, "refresh_browser_sources", StreamUP::WebSocketAPI::WebsocketRequestRefreshBrowserSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "vlcGetCurrentFile", StreamUP::WebSocketAPI::WebsocketRequestVLCGetCurrentFile, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSourceProperties", StreamUP::WebSocketAPI::WebsocketOpenSourceProperties, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSourceFilters", StreamUP::WebSocketAPI::WebsocketOpenSourceFilters, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSourceInteract", StreamUP::WebSocketAPI::WebsocketOpenSourceInteract, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSceneFilters", StreamUP::WebSocketAPI::WebsocketOpenSceneFilters, nullptr);
	obs_websocket_vendor_register_request(vendor, "loadStreamupFile", StreamUP::WebSocketAPI::WebsocketLoadStreamupFile, nullptr);

	blog(LOG_INFO, "[StreamUP] RegisterWebsocketRequests: All WebSocket requests registered successfully");
}


static void LoadStreamUPDock()
{
	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Starting StreamUP dock creation");

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPDock: Failed to get main window");
		return;
	}
	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Got main window");

	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Pushing UI translation");
	obs_frontend_push_ui_translation(obs_module_get_string);

	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Creating StreamUPDock widget");

#ifdef __APPLE__
	blog(LOG_INFO, "[StreamUP] Mac: About to create StreamUPDock widget");
#endif

	StreamUPDock *dock_widget = nullptr;
	try {
		dock_widget = new StreamUPDock(main_window);
	} catch (const std::exception& e) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPDock: Exception creating StreamUPDock: %s", e.what());
		obs_frontend_pop_ui_translation();
		return;
	} catch (...) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPDock: Unknown exception creating StreamUPDock");
		obs_frontend_pop_ui_translation();
		return;
	}

	if (!dock_widget) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPDock: Failed to create StreamUPDock widget");
		obs_frontend_pop_ui_translation();
		return;
	}
	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: StreamUPDock widget created successfully");

	const QString title = QString::fromUtf8(obs_module_text("Dock.Title"));
	const auto name = "StreamUPDock";
	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Dock title: %s", title.toUtf8().constData());

	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Using new API - adding dock by ID");
	obs_frontend_add_dock_by_id(name, title.toUtf8().constData(), dock_widget);
	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: Dock added to frontend");

	obs_frontend_pop_ui_translation();
	blog(LOG_INFO, "[StreamUP] LoadStreamUPDock: StreamUP dock creation completed");
}

// Global Scene Organiser dock instances
static StreamUP::SceneOrganiser::SceneOrganiserDock* globalSceneOrganiserNormal = nullptr;
static StreamUP::SceneOrganiser::SceneOrganiserDock* globalSceneOrganiserVertical = nullptr;

static void LoadSceneOrganiserDocks()
{
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Starting Scene Organiser dock creation");

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Failed to get main window");
		return;
	}
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Got main window");

	obs_frontend_push_ui_translation(obs_module_get_string);

	// Always create Normal Canvas Scene Organiser
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Creating Normal Canvas Scene Organiser");

#ifdef __APPLE__
	blog(LOG_INFO, "[StreamUP] Mac: About to create Normal Canvas Scene Organiser");
#endif

	try {
		globalSceneOrganiserNormal = new StreamUP::SceneOrganiser::SceneOrganiserDock(
			StreamUP::SceneOrganiser::CanvasType::Normal, main_window);
	} catch (const std::exception& e) {
		blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Exception creating Normal Scene Organiser: %s", e.what());
		obs_frontend_pop_ui_translation();
		return;
	} catch (...) {
		blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Unknown exception creating Normal Scene Organiser");
		obs_frontend_pop_ui_translation();
		return;
	}

	if (!globalSceneOrganiserNormal) {
		blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Failed to create Normal Scene Organiser");
		obs_frontend_pop_ui_translation();
		return;
	}
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Normal Canvas Scene Organiser created successfully");

	const QString normalTitle = QString::fromUtf8(obs_module_text("SceneOrganiser.Label.NormalCanvas"));
	const auto normalName = "StreamUPSceneOrganiserNormal";

	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Adding Normal dock with new API");
	obs_frontend_add_dock_by_id(normalName, normalTitle.toUtf8().constData(), globalSceneOrganiserNormal);
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Normal Canvas dock added");

	// Create Vertical Canvas Scene Organiser only if Aitum Vertical plugin is detected
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Checking for Vertical plugin");
	if (StreamUP::SceneOrganiser::SceneOrganiserDock::IsVerticalPluginDetected()) {
		blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Vertical plugin detected, creating Vertical Canvas Scene Organiser");

#ifdef __APPLE__
		blog(LOG_INFO, "[StreamUP] Mac: About to create Vertical Canvas Scene Organiser");
#endif

		try {
			globalSceneOrganiserVertical = new StreamUP::SceneOrganiser::SceneOrganiserDock(
				StreamUP::SceneOrganiser::CanvasType::Vertical, main_window);
		} catch (const std::exception& e) {
			blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Exception creating Vertical Scene Organiser: %s", e.what());
			globalSceneOrganiserVertical = nullptr;
		} catch (...) {
			blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Unknown exception creating Vertical Scene Organiser");
			globalSceneOrganiserVertical = nullptr;
		}

		if (!globalSceneOrganiserVertical) {
			blog(LOG_ERROR, "[StreamUP] LoadSceneOrganiserDocks: Failed to create Vertical Scene Organiser");
		} else {
			blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Vertical Canvas Scene Organiser created successfully");

			const QString verticalTitle = QString::fromUtf8(obs_module_text("SceneOrganiser.Label.VerticalCanvas"));
			const auto verticalName = "StreamUPSceneOrganiserVertical";

			blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Adding Vertical dock with new API");
			obs_frontend_add_dock_by_id(verticalName, verticalTitle.toUtf8().constData(), globalSceneOrganiserVertical);
			blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Vertical Canvas dock added");
		}
	} else {
		blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Vertical plugin not detected, skipping Vertical Canvas");
	}

	obs_frontend_pop_ui_translation();
	blog(LOG_INFO, "[StreamUP] LoadSceneOrganiserDocks: Scene Organiser dock creation completed");
}

// Function to apply scene organiser visibility changes
void ApplySceneOrganiserVisibility()
{
	// This function is maintained for compatibility but no longer needed
	// since Scene Organisers are now always enabled
	StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Visibility", "ApplySceneOrganiserVisibility called - Scene Organisers are now always enabled");
}

static StreamUPToolbar* globalToolbar = nullptr;

// Forward declarations
void ApplyToolbarVisibility();
void ApplyToolbarPosition();

static void LoadStreamUPToolbar()
{
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Starting toolbar creation");

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPToolbar: Failed to get main window");
		return;
	}
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Got main window");

	obs_frontend_push_ui_translation(obs_module_get_string);

	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Creating StreamUPToolbar");

#ifdef __APPLE__
	blog(LOG_INFO, "[StreamUP] Mac: About to create StreamUPToolbar");
#endif

	try {
		globalToolbar = new StreamUPToolbar(main_window);
	} catch (const std::exception& e) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPToolbar: Exception creating StreamUPToolbar: %s", e.what());
		obs_frontend_pop_ui_translation();
		return;
	} catch (...) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPToolbar: Unknown exception creating StreamUPToolbar");
		obs_frontend_pop_ui_translation();
		return;
	}

	if (!globalToolbar) {
		blog(LOG_ERROR, "[StreamUP] LoadStreamUPToolbar: Failed to create StreamUPToolbar");
		obs_frontend_pop_ui_translation();
		return;
	}
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: StreamUPToolbar created successfully");

	// Get toolbar position from settings and add to appropriate area
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Getting toolbar position from settings");
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	Qt::ToolBarArea area;
	switch (settings.toolbarPosition) {
		case StreamUP::SettingsManager::ToolbarPosition::Bottom:
			area = Qt::BottomToolBarArea;
			blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Toolbar position set to Bottom");
			break;
		case StreamUP::SettingsManager::ToolbarPosition::Left:
			area = Qt::LeftToolBarArea;
			blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Toolbar position set to Left");
			break;
		case StreamUP::SettingsManager::ToolbarPosition::Right:
			area = Qt::RightToolBarArea;
			blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Toolbar position set to Right");
			break;
		default:
		case StreamUP::SettingsManager::ToolbarPosition::Top:
			area = Qt::TopToolBarArea;
			blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Toolbar position set to Top (default)");
			break;
	}

	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Adding toolbar to main window");
	main_window->addToolBar(area, globalToolbar);

	// Update position-aware theming after initial positioning
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Updating position-aware theme");
	globalToolbar->updatePositionAwareTheme();

	// Apply initial visibility setting
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Applying toolbar visibility");
	ApplyToolbarVisibility();

	obs_frontend_pop_ui_translation();
	blog(LOG_INFO, "[StreamUP] LoadStreamUPToolbar: Toolbar creation completed");
}

void ApplyToolbarVisibility()
{
	if (globalToolbar) {
		StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
		globalToolbar->setVisible(settings.showToolbar);
	}
}

void ApplyToolbarPosition()
{
	if (globalToolbar) {
		const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (main_window) {
			// Get current position setting
			StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
			Qt::ToolBarArea newArea;
			switch (settings.toolbarPosition) {
				case StreamUP::SettingsManager::ToolbarPosition::Bottom:
					newArea = Qt::BottomToolBarArea;
					break;
				case StreamUP::SettingsManager::ToolbarPosition::Left:
					newArea = Qt::LeftToolBarArea;
					break;
				case StreamUP::SettingsManager::ToolbarPosition::Right:
					newArea = Qt::RightToolBarArea;
					break;
				default:
				case StreamUP::SettingsManager::ToolbarPosition::Top:
					newArea = Qt::TopToolBarArea;
					break;
			}
			
			// Remove from current area and add to new area
			main_window->removeToolBar(globalToolbar);
			main_window->addToolBar(newArea, globalToolbar);
			
			// Update position-aware theming after repositioning
			globalToolbar->updatePositionAwareTheme();
			
			// Force a comprehensive style refresh to ensure position-aware styles are applied immediately
			StreamUP::DebugLogger::LogDebug("Toolbar", "Position Change", "Forcing style refresh after toolbar position change");
			
			// Refresh the main window and all its children to pick up the new position-aware styling
			main_window->style()->unpolish(main_window);
			main_window->style()->polish(main_window);
			
			// Force refresh of all child widgets (including toolbars)
			QList<QWidget*> allChildWidgets = main_window->findChildren<QWidget*>();
			for (QWidget* child : allChildWidgets) {
				if (child) {
					child->style()->unpolish(child);
					child->style()->polish(child);
				}
			}
			
			// Update and repaint the main window
			main_window->update();
			main_window->repaint();
			
			// Ensure toolbar visibility is maintained after repositioning
			ApplyToolbarVisibility();
		}
	}
}

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] Starting module load - version %s", PROJECT_VERSION);
#ifdef _WIN32
	blog(LOG_INFO, "[StreamUP] Platform: Windows");
#elif defined(__APPLE__)
	blog(LOG_INFO, "[StreamUP] Platform: macOS");
#elif defined(__linux__)
	blog(LOG_INFO, "[StreamUP] Platform: Linux");
#else
	blog(LOG_INFO, "[StreamUP] Platform: Unknown");
#endif

#ifdef __APPLE__
	blog(LOG_INFO, "[StreamUP] Mac platform detected - enabling Mac-specific diagnostics");
	blog(LOG_INFO, "[StreamUP] Mac: Checking Qt version and capabilities");
	blog(LOG_INFO, "[StreamUP] Mac: QT_VERSION_STR = %s", QT_VERSION_STR);
	blog(LOG_INFO, "[StreamUP] Mac: OBS API version = %d", LIBOBS_API_VER);

	// Check if main window is available (critical for dock creation on Mac)
	auto *test_main_window = obs_frontend_get_main_window();
	if (test_main_window) {
		blog(LOG_INFO, "[StreamUP] Mac: Main window is available at startup");
	} else {
		blog(LOG_WARNING, "[StreamUP] Mac: Main window is NOT available at startup - this may cause issues");
	}

	// Check if obs-websocket is available (common issue on Mac)
	blog(LOG_INFO, "[StreamUP] Mac: Checking obs-websocket availability");
	obs_websocket_vendor test_vendor = obs_websocket_register_vendor("streamup_test");
	if (test_vendor) {
		blog(LOG_INFO, "[StreamUP] Mac: obs-websocket is available");
		// Clean up test vendor immediately
		// Note: There's no official unregister function, but this test confirms availability
	} else {
		blog(LOG_WARNING, "[StreamUP] Mac: obs-websocket is NOT available - WebSocket features will be disabled");
	}

	// Check Qt event loop and threading
	blog(LOG_INFO, "[StreamUP] Mac: Checking Qt threading model");
	if (QApplication::instance()) {
		blog(LOG_INFO, "[StreamUP] Mac: Qt Application instance is available");
		if (QThread::currentThread() == QApplication::instance()->thread()) {
			blog(LOG_INFO, "[StreamUP] Mac: Running on main Qt thread - good");
		} else {
			blog(LOG_WARNING, "[StreamUP] Mac: NOT running on main Qt thread - potential threading issues");
		}
	} else {
		blog(LOG_ERROR, "[StreamUP] Mac: Qt Application instance is NOT available - this is bad");
	}
#endif

	blog(LOG_INFO, "[StreamUP] About to enter try block for step 1/8");

	try {
		blog(LOG_INFO, "[StreamUP] Inside try block - about to start step 1/8");

#ifdef __APPLE__
		// Mac workaround: Skip menu initialization entirely to isolate crash
		blog(LOG_INFO, "[StreamUP] Step 1/8: Mac detected - completely skipping menu initialization");
#else
		blog(LOG_INFO, "[StreamUP] Step 1/8: Starting menu initialization");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Starting menu initialization");
		StreamUP::MenuManager::InitializeMenu();
		blog(LOG_INFO, "[StreamUP] Step 1/8: Menu initialization completed");
#endif

		blog(LOG_INFO, "[StreamUP] Step 2/8: Registering WebSocket requests");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Registering WebSocket requests");
		RegisterWebsocketRequests();
		blog(LOG_INFO, "[StreamUP] Step 2/8: WebSocket requests registered");

		blog(LOG_INFO, "[StreamUP] Step 3/8: Registering hotkeys");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Registering hotkeys");
		StreamUP::HotkeyManager::RegisterHotkeys();
		blog(LOG_INFO, "[StreamUP] Step 3/8: Hotkeys registered");

		blog(LOG_INFO, "[StreamUP] Step 4/8: Adding save callback for hotkeys");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Adding save callback for hotkeys");
		obs_frontend_add_save_callback(StreamUP::HotkeyManager::SaveLoadHotkeys, nullptr);
		blog(LOG_INFO, "[StreamUP] Step 4/8: Save callback added");

		blog(LOG_INFO, "[StreamUP] Step 5/8: Loading StreamUP dock");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Loading StreamUP dock");
		LoadStreamUPDock();
		blog(LOG_INFO, "[StreamUP] Step 5/8: StreamUP dock loaded");

		blog(LOG_INFO, "[StreamUP] Step 6/8: Loading Scene Organiser docks");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Loading Scene Organiser docks");
		LoadSceneOrganiserDocks();
		blog(LOG_INFO, "[StreamUP] Step 6/8: Scene Organiser docks loaded");

		blog(LOG_INFO, "[StreamUP] Step 7/8: Loading StreamUP toolbar");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Loading StreamUP toolbar");
		LoadStreamUPToolbar();
		blog(LOG_INFO, "[StreamUP] Step 7/8: StreamUP toolbar loaded");

		blog(LOG_INFO, "[StreamUP] Step 8/8: Initializing MultiDock system");
		StreamUP::DebugLogger::LogDebug("Plugin", "Initialize", "Initializing MultiDock system");
		StreamUP::MultiDock::MultiDockManager::Initialize();
		blog(LOG_INFO, "[StreamUP] Step 8/8: MultiDock system initialized");

		blog(LOG_INFO, "[StreamUP] Plugin initialization completed successfully");
		StreamUP::DebugLogger::LogInfo("Plugin", "Plugin initialization completed successfully");

		// Mark initialization as complete - debug logging will now respect user settings
		StreamUP::DebugLogger::SetInitializationComplete(true);

		return true;
	} catch (const std::exception& e) {
		blog(LOG_ERROR, "[StreamUP] Exception during module load: %s", e.what());
		// Reset initialization status on error
		StreamUP::DebugLogger::SetInitializationComplete(false);
		return false;
	} catch (...) {
		blog(LOG_ERROR, "[StreamUP] Unknown exception during module load");
		// Reset initialization status on error
		StreamUP::DebugLogger::SetInitializationComplete(false);
		return false;
	}
}

static void OnOBSFinishedLoading(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);
	
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		StreamUP::DebugLogger::LogInfo("Plugin", "OBS finished loading, initializing plugin data...");

		// Remove the event callback since we only need this to run once
		StreamUP::DebugLogger::LogDebug("Plugin", "OBS Finished Loading", "Removing event callback");
		obs_frontend_remove_event_callback(OnOBSFinishedLoading, nullptr);

		// Run initialization asynchronously to avoid any potential blocking
		StreamUP::DebugLogger::LogDebug("Plugin", "OBS Finished Loading", "Starting async initialization thread");
		std::thread initThread([]() {
			// Initialize plugin data from API
			StreamUP::DebugLogger::LogDebug("Plugin", "Async Init", "Initializing required modules");
			StreamUP::PluginManager::InitialiseRequiredModules();

			// Always perform initial plugin check and cache results for efficiency
			// This ensures cached data is available even if startup check is disabled
			StreamUP::DebugLogger::LogDebug("Plugin", "Async Init", "Performing plugin check and cache");
			StreamUP::PluginManager::PerformPluginCheckAndCache();
			
			// Schedule startup UI to show on UI thread with delay
			StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
				constexpr int STARTUP_DELAY_MS = 2000;
				QTimer::singleShot(STARTUP_DELAY_MS, []() {
					// Check splash screen condition
					StreamUP::SplashScreen::ShowCondition condition = StreamUP::SplashScreen::CheckSplashCondition();
					
					if (condition == StreamUP::SplashScreen::ShowCondition::FirstInstall) {
						// Show welcome screen for first install
						StreamUP::SplashScreen::ShowSplashScreenIfNeeded();
					} else if (condition == StreamUP::SplashScreen::ShowCondition::VersionUpdate) {
						// Show patch notes popup for version updates
						StreamUP::SplashScreen::ShowSplashScreenIfNeeded();
					}
					// If Never, don't show anything
				});
			});
			
			// Schedule plugin update check with delay to allow welcome/patch notes windows to be shown first
			StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
				constexpr int PLUGIN_CHECK_DELAY_MS = 5000;
				QTimer::singleShot(PLUGIN_CHECK_DELAY_MS, []() {
					// Check for plugin updates on startup if enabled, but stay silent if up to date
					// Only show if no splash screen or patch notes window is open
					if (!StreamUP::SplashScreen::IsSplashScreenOpen() && !StreamUP::PatchNotesWindow::IsPatchNotesWindowOpen()) {
						StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
						if (settings.runAtStartup) {
							// Use the silent version that only shows dialogs if there are actual updates
							StreamUP::PluginManager::ShowCachedPluginUpdatesDialogSilent();
						}
					}
				});
			});
		});
		initThread.detach();
	}
}

void obs_module_post_load(void)
{
	blog(LOG_INFO, "[StreamUP] Starting post-load initialization");
	StreamUP::DebugLogger::LogDebug("Plugin", "Post Load", "Starting post-load initialization");

	try {
		// Initialize settings system immediately (this is lightweight)
		blog(LOG_INFO, "[StreamUP] Post-load step 1/2: Initializing settings system");
		StreamUP::DebugLogger::LogDebug("Plugin", "Post Load", "Initializing settings system");
		StreamUP::SettingsManager::InitializeSettingsSystem();
		blog(LOG_INFO, "[StreamUP] Post-load step 1/2: Settings system initialized");

		// Register callback to defer heavy initialization until OBS has finished loading
		blog(LOG_INFO, "[StreamUP] Post-load step 2/2: Registering OBS finished loading callback");
		StreamUP::DebugLogger::LogDebug("Plugin", "Post Load", "Registering OBS finished loading callback");
		obs_frontend_add_event_callback(OnOBSFinishedLoading, nullptr);
		blog(LOG_INFO, "[StreamUP] Post-load step 2/2: OBS finished loading callback registered");

		blog(LOG_INFO, "[StreamUP] Post-load initialization completed successfully");
		StreamUP::DebugLogger::LogDebug("Plugin", "Post Load", "Post-load initialization completed");
	} catch (const std::exception& e) {
		blog(LOG_ERROR, "[StreamUP] Exception during post-load: %s", e.what());
	} catch (...) {
		blog(LOG_ERROR, "[StreamUP] Unknown exception during post-load");
	}
}

//--------------------EXIT COMMANDS--------------------
void obs_module_unload()
{
	blog(LOG_INFO, "[StreamUP] Starting plugin unload process");
	StreamUP::DebugLogger::LogDebug("Plugin", "Unload", "Starting plugin unload process");

	// Reset initialization status during unload
	StreamUP::DebugLogger::SetInitializationComplete(false);

	try {
		blog(LOG_INFO, "[StreamUP] Unload step 1/5: Removing save callback for hotkeys");
		StreamUP::DebugLogger::LogDebug("Plugin", "Unload", "Removing save callback for hotkeys");
		obs_frontend_remove_save_callback(StreamUP::HotkeyManager::SaveLoadHotkeys, nullptr);

		blog(LOG_INFO, "[StreamUP] Unload step 2/5: Unregistering hotkeys");
		StreamUP::DebugLogger::LogDebug("Plugin", "Unload", "Unregistering hotkeys");
		StreamUP::HotkeyManager::UnregisterHotkeys();

		// Clean up toolbar - just nullify the pointer, let Qt/OBS handle destruction
		blog(LOG_INFO, "[StreamUP] Unload step 3/5: Cleaning up toolbar reference");
		StreamUP::DebugLogger::LogDebug("Plugin", "Unload", "Cleaning up toolbar reference");
		globalToolbar = nullptr;

		// Shutdown MultiDock system
		blog(LOG_INFO, "[StreamUP] Unload step 4/5: Shutting down MultiDock system");
		StreamUP::DebugLogger::LogDebug("Plugin", "Unload", "Shutting down MultiDock system");
		StreamUP::MultiDock::MultiDockManager::Shutdown();

		// Clean up settings cache
		blog(LOG_INFO, "[StreamUP] Unload step 5/5: Cleaning up settings cache");
		StreamUP::DebugLogger::LogDebug("Plugin", "Unload", "Cleaning up settings cache");
		StreamUP::SettingsManager::CleanupSettingsCache();

		blog(LOG_INFO, "[StreamUP] Plugin unload completed successfully");
		StreamUP::DebugLogger::LogInfo("Plugin", "Plugin unload completed successfully");
	} catch (const std::exception& e) {
		blog(LOG_ERROR, "[StreamUP] Exception during unload: %s", e.what());
	} catch (...) {
		blog(LOG_ERROR, "[StreamUP] Unknown exception during unload");
	}
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("App.Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("App.Name");
}
