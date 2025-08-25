#include "obs-websocket-api.h"
#include "streamup.hpp"
#include "ui/dock/streamup-dock.hpp"
#include "ui/streamup-toolbar.hpp"
#include "ui/settings-manager.hpp"
#include "version.h"
#include "streamup-common.hpp"
#include "plugin-state.hpp"
#include "source-manager.hpp"
#include "file-manager.hpp"
#include "plugin-manager.hpp"
#include "websocket-api.hpp"
#include "hotkey-manager.hpp"
#include "ui-helpers.hpp"
#include "ui/ui-styles.hpp"
#include "menu-manager.hpp"
#include "notification-manager.hpp"
#include "http-client.hpp"
#include "utilities/path-utils.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "ui/splash-screen.hpp"
#include "ui/patch-notes-window.hpp"
#include "multidock/multidock_manager.hpp"
#include <filesystem>
#include <fstream>
#include <obs.h>
#include <obs-data.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QDesktopServices>
#include <QDialog>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QMainWindow>
#include <QObject>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QUrl>
#include <regex>
#include <thread>
#include <unordered_set>
#include <util/platform.h>



OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Andilippi");
OBS_MODULE_USE_DEFAULT_LOCALE("streamup", "en-US")


//--------------------STRUCTS & GLOBALS--------------------
// All structures and global state have been moved to appropriate modules



//--------------------PATH HELPERS--------------------
std::string GetLocalAppDataPath()
{
#ifdef _WIN32
	char *buf = nullptr;
	size_t sz = 0;
	if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr) {
		std::string path(buf);
		free(buf);
		return path;
	}
#endif
	return "";
}

char *GetFilePath()
{
	char *path = nullptr;
	char *path_abs = nullptr;

	if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
		path = obs_module_config_path("../../logs/");
		path_abs = os_get_abs_path_ptr(path);

		if (path_abs[strlen(path_abs) - 1] != '/' && path_abs[strlen(path_abs) - 1] != '\\') {
			// Create a new string with appended "/"
			size_t new_path_abs_size = strlen(path_abs) + 2;
			char *newPathAbs = (char *)bmalloc(new_path_abs_size);
			strcpy(newPathAbs, path_abs);
			strcat(newPathAbs, "/");

			// Free the old path_abs and reassign it
			bfree(path_abs);
			path_abs = newPathAbs;
		}
	} else {
		path = obs_module_config_path("");

		std::string path_str(path);
		std::string to_search = "/plugin_config/streamup/";
		std::string replace_str = "/logs/";

		size_t pos = path_str.find(to_search);

		// If found then replace it
		if (pos != std::string::npos) {
			path_str.replace(pos, to_search.size(), replace_str);
		}

		size_t path_abs_size = path_str.size() + 1;
		path_abs = (char *)bmalloc(path_abs_size);
		std::strcpy(path_abs, path_str.c_str());
	}
	bfree(path);

	blog(LOG_INFO, "[StreamUP] Path: %s", path_abs);

	// Use std::filesystem to check if the path exists
	std::string path_abs_str(path_abs);
	blog(LOG_INFO, "[StreamUP] Path: %s", path_abs_str.c_str());

	bool path_exists = std::filesystem::exists(path_abs_str);
	if (path_exists) {
		std::filesystem::directory_iterator dir(path_abs_str);
		if (dir == std::filesystem::directory_iterator{}) {
			// The directory is empty
			blog(LOG_INFO, "[StreamUP] OBS doesn't have files in the install directory.");
			bfree(path_abs);
			return NULL;
		} else {
			// The directory contains files
			return path_abs;
		}
	} else {
		// The directory does not exist
		blog(LOG_INFO, "[StreamUP] OBS log file folder does not exist in the install directory.");
		bfree(path_abs);
		return NULL;
	}
}

// GetMostRecentFile moved to StreamUP::PathUtils::GetMostRecentTxtFile


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

		StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("Cancel"), [dialog]() { dialog->close(); });

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

		QGroupBox *info3Box = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("HowToUse"), "info");
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

		QPushButton *copyJsonButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("CopyWebsocketJson"), "neutral");
		copyJsonButton->setToolTip(obs_module_text("CopyWebsocketJsonTooltip"));
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
			}
		}
		fclose(file);
	} else {
		blog(LOG_ERROR, "[StreamUP] Failed to open file: %s", filepath.c_str());
	}

	return "";
}

std::vector<std::pair<std::string, std::string>> GetInstalledPlugins()
{
	std::vector<std::pair<std::string, std::string>> installedPlugins;
	char *filepath = GetFilePath();
	if (filepath == NULL) {
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

std::string GetPlatformURL(const StreamUP::PluginInfo &plugin_info)
{
	std::string url;
	if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
		url = plugin_info.windowsURL;
	} else if (strcmp(STREAMUP_PLATFORM_NAME, "macos") == 0) {
		url = plugin_info.macURL;
	} else if (strcmp(STREAMUP_PLATFORM_NAME, "linux") == 0) {
		url = plugin_info.linuxURL;
	} else {
		url = plugin_info.windowsURL;
	}
	return url;
}

//-------------------ERROR AND UPDATE HANDLING-------------------
void ErrorDialog(const QString &errorMessage)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([errorMessage]() {
		QDialog *dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
		                                 StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::SPACING_SMALL);

		QString displayMessage = errorMessage.isEmpty() ? "Unknown error occurred." : errorMessage;

		dialogLayout->addLayout(StreamUP::UIHelpers::AddIconAndText(QStyle::SP_MessageBoxCritical, displayMessage.toUtf8().constData()));

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		StreamUP::UIHelpers::CreateButton(buttonLayout, "OK", [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

void PluginsUpToDateOutput(bool manuallyTriggered)
{
	if (manuallyTriggered) {
		StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
			QDialog *dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowUpToDateTitle");
			QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
			dialogLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
		                                 StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::SPACING_SMALL);

			dialogLayout->addLayout(StreamUP::UIHelpers::AddIconAndText(QStyle::SP_DialogApplyButton, "WindowUpToDateMessage"));

			QHBoxLayout *buttonLayout = new QHBoxLayout();
			StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("OK"), [dialog]() { dialog->close(); });

			dialogLayout->addLayout(buttonLayout);
			dialog->setLayout(dialogLayout);
			dialog->show();
		});
	}
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([errorMsgMissing, errorMsgUpdate]() {
		QDialog *dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowPluginErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
		                                 StreamUP::UIStyles::Sizes::PADDING_XL, 
		                                 StreamUP::UIStyles::Sizes::PADDING_XL);

		const char *errorText = (errorMsgMissing != "NULL") ? "WindowPluginErrorMissing" : "WindowPluginErrorUpdating";
		dialogLayout->addLayout(StreamUP::UIHelpers::AddIconAndText(QStyle::SP_MessageBoxWarning, errorText));
		dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

		QLabel *pluginErrorInfo = StreamUP::UIHelpers::CreateRichTextLabel(obs_module_text("WindowPluginErrorInfo"), false, true);
		dialogLayout->addWidget(pluginErrorInfo);
		dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

		if (!errorMsgUpdate.empty()) {
			QLabel *pluginsToUpdateList =
				StreamUP::UIHelpers::CreateRichTextLabel(QString::fromStdString(errorMsgUpdate), false, false, Qt::AlignCenter);
			QGroupBox *pluginsToUpdateBox = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowPluginErrorUpdateGroup"), "warning");
			QVBoxLayout *pluginsToUpdateBoxLayout = new QVBoxLayout(pluginsToUpdateBox);
			pluginsToUpdateBoxLayout->addWidget(pluginsToUpdateList);
			dialogLayout->addWidget(pluginsToUpdateBox);
			if (errorMsgMissing != "NULL") {
				dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
			}
		}

		if (errorMsgMissing != "NULL") {
			QLabel *pluginsMissingList =
				StreamUP::UIHelpers::CreateRichTextLabel(QString::fromStdString(errorMsgMissing), false, false, Qt::AlignCenter);
			QGroupBox *pluginsMissingBox = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowPluginErrorMissingGroup"), "error");
			QVBoxLayout *pluginsMissingBoxLayout = new QVBoxLayout(pluginsMissingBox);
			pluginsMissingBoxLayout->addWidget(pluginsMissingList);
			dialogLayout->addWidget(pluginsMissingBox);
		}

		if (errorMsgMissing != "NULL") {
			QLabel *pluginstallerLabel =
				StreamUP::UIHelpers::CreateRichTextLabel(obs_module_text("WindowPluginErrorFooter"), false, false, Qt::AlignCenter);
			dialogLayout->addWidget(pluginstallerLabel);
		}

		QHBoxLayout *buttonLayout = new QHBoxLayout();

		StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("OK"), [dialog]() { dialog->close(); });

		if (errorMsgMissing != "NULL") {
			QPushButton *pluginstallerButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("MenuDownloadPluginstaller"), "neutral");
			QObject::connect(pluginstallerButton, &QPushButton::clicked, []() {
				QDesktopServices::openUrl(QUrl("https://streamup.tips/product/plugin-installer"));
			});
			buttonLayout->addWidget(pluginstallerButton);
		}

		dialogLayout->addLayout(buttonLayout);

		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}


//-------------------OBS API HELPER FUNCTIONS-------------------



//-------------------- HELPER FUNCTIONS--------------------
static bool EnumSceneItemsCallback(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	StreamUP::SceneItemEnumData *data = static_cast<StreamUP::SceneItemEnumData *>(param);
	bool isSelected = obs_sceneitem_selected(item);
	if (isSelected) {
		data->isAnySourceSelected = true;
		obs_source_t *source = obs_sceneitem_get_source(item);
		data->selectedSourceName = obs_source_get_name(source);
	}
	return true;
}

bool EnumSceneItems(obs_scene_t *scene, const char **selected_source_name)
{
	StreamUP::SceneItemEnumData data;

	obs_scene_enum_items(scene, EnumSceneItemsCallback, &data);

	if (data.isAnySourceSelected) {
		*selected_source_name = data.selectedSourceName;
	}
	return data.isAnySourceSelected;
}

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
		blog(LOG_WARNING, "[StreamUP] Failed to get settings for transition: %s", obs_source_get_name(transition));
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
			blog(LOG_WARNING, "[StreamUP] Failed to get display name for transition ID: %s", transition_id);
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

//-------------------UTILITY FUNCTIONS-------------------
const char *MonitoringTypeToString(obs_monitoring_type type)
{
	switch (type) {
	case OBS_MONITORING_TYPE_NONE:
		return "None";
	case OBS_MONITORING_TYPE_MONITOR_ONLY:
		return "Monitor Only";
	case OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT:
		return "Monitor and Output";
	default:
		return "Unknown";
	}
}






//--------------------WEBSOCKET VENDOR REQUESTS--------------------
obs_websocket_vendor vendor = nullptr;







// GetForumLink moved to StreamUP::PluginManager::GetPluginForumLink

// SetLabelWithSortedModules functionality moved to UI helper functions

// SearchModulesInFile moved to StreamUP::PluginManager::SearchLoadedModulesInLogFile
/*
std::vector<std::string> SearchModulesInFile_OLD(const char *path)
{
	std::unordered_set<std::string> ignoreModules = {"obs-websocket",      "coreaudio-encoder", "decklink-captions",
							 "decklink-output-ui", "frontend-tools",    "image-source",
							 "obs-browser",        "obs-ffmpeg",        "obs-filters",
							 "obs-outputs",        "obs-qsv11",         "obs-text",
							 "obs-transitions",    "obs-vst",           "obs-x264",
							 "rtmp-services",      "text-freetype2",    "vlc-video",
							 "win-capture",        "win-dshow",         "win-wasapi",
							 "mac-avcapture",      "mac-capture",       "mac-syphon",
							 "mac-videotoolbox",   "mac-virtualcam",    "linux-v4l2",
							 "linux-pulseaudio",   "linux-pipewire",    "linux-jack",
							 "linux-capture",      "linux-source",      "obs-libfdk"};

	std::string filepath = StreamUP::PathUtils::GetMostRecentTxtFile(path);
	FILE *file = fopen(filepath.c_str(), "r");
	std::vector<std::string> collected_modules;
	std::regex timestamp_regex("^[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}:");

	if (file) {
		char line[LINE_BUFFER_SIZE];
		bool in_section = false;

		while (fgets(line, LINE_BUFFER_SIZE, file) != NULL) {
			std::string str_line(line);
			str_line = std::regex_replace(str_line, timestamp_regex, "");
			str_line.erase(0, str_line.find_first_not_of(" \t\r\n"));
			str_line.erase(str_line.find_last_not_of(" \t\r\n") + 1);

			if (str_line.find("Loaded Modules:") != std::string::npos) {
				in_section = true;
			} else if (str_line.find("---------------------------------") != std::string::npos) {
				in_section = false;
			}

			if (in_section && !str_line.empty() && str_line != "Loaded Modules:") {
				size_t suffix_pos = std::string::npos;
				if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
					suffix_pos = str_line.find(".dll");
				} else if (strcmp(STREAMUP_PLATFORM_NAME, "linux") == 0) {
					suffix_pos = str_line.find(".so");
				}

				if (suffix_pos != std::string::npos) {
					str_line = str_line.substr(0, suffix_pos);
				}

				if (ignoreModules.find(str_line) == ignoreModules.end()) {
					bool foundInApi = false;
					const auto& allPlugins = StreamUP::GetAllPlugins();
					for (const auto &pair : allPlugins) {
						if (pair.second.moduleName == str_line) {
							foundInApi = true;
							break;
						}
					}
					if (!foundInApi) {
						collected_modules.push_back(str_line);
					}
				}
			}
		}
		fclose(file);
	} else {
		blog(LOG_ERROR, "[StreamUP] Failed to open log file: %s", filepath.c_str());
	}

	std::sort(collected_modules.begin(), collected_modules.end(), [](const std::string &a, const std::string &b) {
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char char1, char char2) {
			return std::tolower(char1) < std::tolower(char2);
		});
	});

	return collected_modules;
}
*/


void SettingsDialog()
{
	// Settings dialog functionality moved to StreamUP::SettingsManager module
	StreamUP::SettingsManager::ShowSettingsDialog();
}

//--------------------MAIN MENU--------------------


//--------------------WEBSOCKET REGISTRATION--------------------

static void RegisterWebsocketRequests()
{
	vendor = obs_websocket_register_vendor("streamup");
	if (!vendor)
		return;

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
}


static void LoadStreamUPDock()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);

	auto *dock_widget = new StreamUPDock(main_window);

	const QString title = QString::fromUtf8(obs_module_text("StreamUPDock"));
	const auto name = "StreamUPDock";

#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 0, 0)
	obs_frontend_add_dock_by_id(name, title.toUtf8().constData(), dock_widget);
#else
	auto dock = new QDockWidget(main_window);
	dock->setObjectName(name);
	dock->setWindowTitle(title);
	dock->setWidget(dock_widget);
	dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
	dock->setFloating(true);
	dock->hide();
	obs_frontend_add_dock(dock);
#endif

	obs_frontend_pop_ui_translation();
}

static StreamUPToolbar* globalToolbar = nullptr;

// Forward declarations
void ApplyToolbarVisibility();
void ApplyToolbarPosition();

static void LoadStreamUPToolbar()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);

	globalToolbar = new StreamUPToolbar(main_window);
	
	// Get toolbar position from settings and add to appropriate area
	StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
	Qt::ToolBarArea area;
	switch (settings.toolbarPosition) {
		case StreamUP::SettingsManager::ToolbarPosition::Bottom:
			area = Qt::BottomToolBarArea;
			break;
		case StreamUP::SettingsManager::ToolbarPosition::Left:
			area = Qt::LeftToolBarArea;
			break;
		case StreamUP::SettingsManager::ToolbarPosition::Right:
			area = Qt::RightToolBarArea;
			break;
		default:
		case StreamUP::SettingsManager::ToolbarPosition::Top:
			area = Qt::TopToolBarArea;
			break;
	}
	main_window->addToolBar(area, globalToolbar);
	
	// Update position-aware theming after initial positioning
	globalToolbar->updatePositionAwareTheme();
	
	// Apply initial visibility setting
	ApplyToolbarVisibility();

	obs_frontend_pop_ui_translation();
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
			
			// Ensure toolbar visibility is maintained after repositioning
			ApplyToolbarVisibility();
		}
	}
}

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] loaded version %s", PROJECT_VERSION);

	StreamUP::MenuManager::InitializeMenu();

	RegisterWebsocketRequests();
	StreamUP::HotkeyManager::RegisterHotkeys();

	obs_frontend_add_save_callback(StreamUP::HotkeyManager::SaveLoadHotkeys, nullptr);

	LoadStreamUPDock();
	LoadStreamUPToolbar();

	// Initialize MultiDock system
	StreamUP::MultiDock::MultiDockManager::Initialize();

	return true;
}

static void OnOBSFinishedLoading(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);
	
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		blog(LOG_INFO, "[StreamUP] OBS finished loading, initializing plugin data...");
		
		// Remove the event callback since we only need this to run once
		obs_frontend_remove_event_callback(OnOBSFinishedLoading, nullptr);
		
		// Run initialization asynchronously to avoid any potential blocking
		std::thread initThread([]() {
			// Initialize plugin data from API
			StreamUP::PluginManager::InitialiseRequiredModules();
			
			// Always perform initial plugin check and cache results for efficiency
			// This ensures cached data is available even if startup check is disabled
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
	// Initialize settings system immediately (this is lightweight)
	StreamUP::SettingsManager::InitializeSettingsSystem();
	
	// Register callback to defer heavy initialization until OBS has finished loading
	obs_frontend_add_event_callback(OnOBSFinishedLoading, nullptr);
}

//--------------------EXIT COMMANDS--------------------
void obs_module_unload()
{
	obs_frontend_remove_save_callback(StreamUP::HotkeyManager::SaveLoadHotkeys, nullptr);
	StreamUP::HotkeyManager::UnregisterHotkeys();
	
	// Clean up toolbar - just nullify the pointer, let Qt/OBS handle destruction
	globalToolbar = nullptr;
	
	// Shutdown MultiDock system
	StreamUP::MultiDock::MultiDockManager::Shutdown();
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("StreamUP");
}
