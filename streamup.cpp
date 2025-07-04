#include "obs-websocket-api.h"
#include "streamup.hpp"
#include "streamup-dock.hpp"
#include "version.h"
#include <cctype>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <graphics/image-file.h>
#include <iostream>
#include <obs.h>
#include <obs-data.h>
#include <obs-encoder.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <pthread.h>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <util/platform.h>
#include <qscrollarea.h>

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

#if defined(_WIN32)
#define PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define PLATFORM_NAME "macos"
#elif defined(__linux__)
#define PLATFORM_NAME "linux"
#else
#define PLATFORM_NAME "unknown"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Andilippi");
OBS_MODULE_USE_DEFAULT_LOCALE("streamup", "en-US")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

//--------------------STRUCTS & GLOBALS--------------------
struct SceneItemEnumData {
	bool isAnySourceSelected = false;
	const char *selectedSourceName = nullptr;
};

struct PluginInfo {
	std::string name;
	std::string version;
	std::string searchString;
	std::string windowsURL;
	std::string macURL;
	std::string linuxURL;
	std::string generalURL;
	std::string moduleName;
	bool required;
};

struct RequestData {
	std::string url;
	std::string response;
};

struct SystemTrayNotification {
	QSystemTrayIcon::MessageIcon icon;
	QString title;
	QString body;
};

std::map<std::string, PluginInfo> all_plugins;
std::map<std::string, PluginInfo> required_plugins;
static bool notificationsMuted = false;

#define ADVANCED_MASKS_SETTINGS_SIZE 15
static const char *advanced_mask_settings[] = {"rectangle_width",
					       "rectangle_height",
					       "position_x",
					       "position_y",
					       "shape_center_x",
					       "shape_center_y",
					       "rectangle_corner_radius",
					       "mask_gradient_position",
					       "mask_gradient_width",
					       "circle_radius",
					       "heart_size",
					       "shape_star_outer_radius",
					       "shape_star_inner_radius",
					       "star_corner_radius",
					       "shape_feather_amount"};

//--------------------INSTALL A PRODUCT--------------------
void ResizeAdvancedMaskFilter(obs_source_t *filter, float factor)
{
	obs_data_t *settings = obs_source_get_settings(filter);
	if (!settings) {
		blog(LOG_ERROR, "ResizeAdvancedMaskFilter: settings is null");
		return;
	}

	for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
		if (obs_data_has_user_value(settings, advanced_mask_settings[i]))
			obs_data_set_double(settings, advanced_mask_settings[i],
					    obs_data_get_double(settings, advanced_mask_settings[i]) * factor);
	}
	obs_source_update(filter, settings);
	obs_data_release(settings);
}

void ResizeMoveSetting(obs_data_t *obs_data, float factor)
{
	double x = obs_data_get_double(obs_data, "x");
	obs_data_set_double(obs_data, "x", x * factor);
	double y = obs_data_get_double(obs_data, "y");
	obs_data_set_double(obs_data, "y", y * factor);
	obs_data_release(obs_data);
}

void ResizeMoveValueFilter(obs_source_t *filter, float factor)
{
	obs_data_t *settings = obs_source_get_settings(filter);
	if (!settings) {
		blog(LOG_ERROR, "ResizeMoveValueFilter: settings is null");
		return;
	}
	if (obs_data_get_int(settings, "move_value_type") == 1) {
		for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
			if (obs_data_has_user_value(settings, advanced_mask_settings[i]))
				obs_data_set_double(settings, advanced_mask_settings[i],
						    obs_data_get_double(settings, advanced_mask_settings[i]) * factor);
		}
	} else {
		const char *setting_name = obs_data_get_string(settings, "setting_name");
		for (size_t i = 0; i < ADVANCED_MASKS_SETTINGS_SIZE; i++) {
			if (strcmp(setting_name, advanced_mask_settings[i]) == 0) {
				if (obs_data_has_user_value(settings, "setting_float"))
					obs_data_set_double(settings, "setting_float",
							    obs_data_get_double(settings, "setting_float") * factor);
				if (obs_data_has_user_value(settings, "setting_float_min"))
					obs_data_set_double(settings, "setting_float_min",
							    obs_data_get_double(settings, "setting_float_min") * factor);
				if (obs_data_has_user_value(settings, "setting_float_max"))
					obs_data_set_double(settings, "setting_float_max",
							    obs_data_get_double(settings, "setting_float_max") * factor);
			}
		}
	}

	obs_source_update(filter, settings);
	obs_data_release(settings);
}

bool IsCloningSceneOrGroup(obs_source_t *source)
{
	if (!source)
		return false;

	const char *input_kind = obs_source_get_id(source);
	if (strcmp(input_kind, "source-clone") != 0)
		return false; // Not a source-clone, no need to check further.

	obs_data_t *source_settings = obs_source_get_settings(source);
	if (!source_settings)
		return false;

	if (obs_data_get_int(source_settings, "clone_type")) {
		obs_data_release(source_settings);
		return true;
	}

	const char *cloned_source_name = obs_data_get_string(source_settings, "clone");
	obs_source_t *cloned_source = obs_get_source_by_name(cloned_source_name);
	if (!cloned_source) {
		obs_data_release(source_settings);
		return false;
	}

	const char *cloned_source_kind = obs_source_get_unversioned_id(cloned_source);
	bool is_scene_or_group = (strcmp(cloned_source_kind, "scene") == 0 || strcmp(cloned_source_kind, "group") == 0);

	obs_source_release(cloned_source);
	obs_data_release(source_settings);

	return is_scene_or_group;
}

void ResizeMoveFilters(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	float factor = *((float *)param);

	const char *filter_id = obs_source_get_unversioned_id(child);

	if (strcmp(filter_id, "move_source_filter") == 0) {
		obs_data_t *settings = obs_source_get_settings(child);
		ResizeMoveSetting(obs_data_get_obj(settings, "pos"), factor);
		ResizeMoveSetting(obs_data_get_obj(settings, "bounds"), factor);
		const char *source_name = obs_data_get_string(settings, "source");
		obs_source_t *source = (source_name && strlen(source_name)) ? obs_get_source_by_name(source_name) : nullptr;
		// Skip resize if cloning a Scene or Group
		if (!obs_scene_from_source(source) && !obs_group_from_source(source) && !IsCloningSceneOrGroup(source)) {
			ResizeMoveSetting(obs_data_get_obj(settings, "scale"), factor);
		}
		obs_source_release(source);
		obs_data_set_string(settings, "transform_text", "");
		obs_data_release(settings);
	} else if (strcmp(filter_id, "advanced_masks_filter") == 0) {
		ResizeAdvancedMaskFilter(child, factor);
	} else if (strcmp(filter_id, "move_value_filter") == 0) {
		ResizeMoveValueFilter(child, factor);
	}
}

void ResizeSceneItems(obs_data_t *settings, float factor)
{
	obs_data_array_t *items = obs_data_get_array(settings, "items");
	size_t count = obs_data_array_count(items);

	if (obs_data_get_bool(settings, "custom_size")) {
		obs_data_set_int(settings, "cx", obs_data_get_int(settings, "cx") * factor);
		obs_data_set_int(settings, "cy", obs_data_get_int(settings, "cy") * factor);
	}

	for (size_t i = 0; i < count; i++) {
		obs_data_t *item_data = obs_data_array_item(items, i);
		const char *name = obs_data_get_string(item_data, "name");
		obs_source_t *item_source = obs_get_source_by_name(name);

		vec2 vec2;
		obs_data_get_vec2(item_data, "pos", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "pos", &vec2);
		obs_data_get_vec2(item_data, "bounds", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "bounds", &vec2);

		// Skip resizing if it's a source-clone and cloning a Scene or Group
		if (item_source && (obs_scene_from_source(item_source) || obs_group_from_source(item_source) ||
				    IsCloningSceneOrGroup(item_source))) {
			obs_data_get_vec2(item_data, "scale_ref", &vec2);
			vec2.x *= factor;
			vec2.y *= factor;
			obs_data_set_vec2(item_data, "scale_ref", &vec2);
		} else {
			obs_data_get_vec2(item_data, "scale", &vec2);
			vec2.x *= factor;
			vec2.y *= factor;
			obs_data_set_vec2(item_data, "scale", &vec2);
		}

		obs_source_release(item_source);
		obs_data_release(item_data);
	}

	obs_data_array_release(items);
}

void ConvertSettingPath(obs_data_t *settings, const char *setting_name, const QString &path, const char *sub_folder)
{
	const char *file = obs_data_get_string(settings, setting_name);
	if (!file || !strlen(file))
		return;
	if (QFile::exists(file))
		return;
	const QString file_name = QFileInfo(QT_UTF8(file)).fileName();
	QString filePath = path + "/Resources/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name, QT_TO_UTF8(filePath));
		return;
	}
	filePath = path + "/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name, QT_TO_UTF8(filePath));
	}
}

void ConvertFilterPaths(obs_data_t *filter_data, const QString &path)
{
	const char *id = obs_data_get_string(filter_data, "id");
	if (strcmp(id, "shader_filter") == 0) {
		obs_data_t *settings = obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "shader_file_name", path, "Shader Filters");
		obs_data_release(settings);
	} else if (strcmp(id, "mask_filter") == 0) {
		obs_data_t *settings = obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "image_path", path, "Image Masks");
		obs_data_release(settings);
	}
}

void ConvertSourcePaths(obs_data_t *source_data, const QString &path)
{
	const char *id = obs_data_get_string(source_data, "id");
	if (strcmp(id, "image_source") == 0) {
		obs_data_t *settings = obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "file", path, "Image Sources");
		obs_data_release(settings);
	} else if (strcmp(id, "ffmpeg_source") == 0) {
		obs_data_t *settings = obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "local_file", path, "Media Sources");
		obs_data_release(settings);
	}
	obs_data_array_t *filters = obs_data_get_array(source_data, "filters");
	if (!filters)
		return;
	const size_t count = obs_data_array_count(filters);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		ConvertFilterPaths(filter_data, path);
		obs_data_release(filter_data);
	}
	obs_data_array_release(filters);
}

static void MergeScenes(obs_source_t *s, obs_data_t *scene_settings)
{
	obs_source_save(s);
	obs_data_array_t *items = obs_data_get_array(scene_settings, "items");
	const size_t item_count = obs_data_array_count(items);
	obs_data_t *scene_settings_orig = obs_source_get_settings(s);
	obs_data_array_t *items_orig = obs_data_get_array(scene_settings_orig, "items");
	const size_t item_count_orig = obs_data_array_count(items_orig);
	for (size_t j = 0; j < item_count_orig; j++) {
		obs_data_t *item_data_orig = obs_data_array_item(items_orig, j);
		const char *name_orig = obs_data_get_string(item_data_orig, "name");
		bool found = false;
		for (size_t k = 0; k < item_count; k++) {
			obs_data_t *item_data = obs_data_array_item(items, k);
			const char *name = obs_data_get_string(item_data, "name");
			if (strcmp(name, name_orig) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			obs_data_array_push_back(items, item_data_orig);
		}
		obs_data_release(item_data_orig);
	}
	obs_data_array_release(items_orig);
	obs_data_release(scene_settings_orig);
	obs_data_array_release(items);
}

static void MergeFilters(obs_source_t *s, obs_data_array_t *filters)
{
	size_t count = obs_data_array_count(filters);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *filter_data = obs_data_array_item(filters, i);
		const char *filter_name = obs_data_get_string(filter_data, "name");
		obs_source_t *filter = obs_source_get_filter_by_name(s, filter_name);
		if (filter) {
			obs_data_release(filter_data);
			obs_source_release(filter);
			continue;
		}
		filter = obs_load_private_source(filter_data);
		if (filter) {
			obs_source_filter_add(s, filter);
			obs_source_release(filter);
		}
		obs_data_release(filter_data);
	}

	obs_data_array_release(filters);
}

static void LoadSources(obs_data_array_t *data, QString path)
{
	const size_t count = obs_data_array_count(data);
	std::list<obs_source_t *> ref_sources;
	std::list<obs_source_t *> load_sources;
	obs_source_t *cs = obs_frontend_get_current_scene();
	uint32_t w = obs_source_get_width(cs);
	float factor = (float)w / 1920.0f;
	obs_source_release(cs);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *sourceData = obs_data_array_item(data, i);
		const char *name = obs_data_get_string(sourceData, "name");

		bool new_source = true;
		obs_source_t *s = obs_get_source_by_name(name);
		if (!s) {
			ConvertSourcePaths(sourceData, path);
			s = obs_load_source(sourceData);
			load_sources.push_back(s);
		} else {
			new_source = false;
			obs_data_array_t *filters = obs_data_get_array(sourceData, "filters");
			if (filters) {
				MergeFilters(s, filters);
				obs_data_array_release(filters);
			}
		}
		if (s)
			ref_sources.push_back(s);
		obs_scene_t *scene = obs_scene_from_source(s);
		if (!scene)
			scene = obs_group_from_source(s);
		if (scene) {
			obs_data_t *scene_settings = obs_data_get_obj(sourceData, "settings");
			if (w != 1920) {
				ResizeSceneItems(scene_settings, factor);
				if (new_source)
					obs_source_enum_filters(s, ResizeMoveFilters, &factor);
			}
			if (!new_source) {
				obs_scene_enum_items(
					scene,
					[](obs_scene_t *, obs_sceneitem_t *item, void *d) {
						std::list<obs_source_t *> *sources = (std::list<obs_source_t *> *)d;
						obs_source_t *si = obs_sceneitem_get_source(item);
						si = obs_source_get_ref(si);
						if (si)
							sources->push_back(si);
						return true;
					},
					&ref_sources);
				MergeScenes(s, scene_settings);
			}
			obs_source_update(s, scene_settings);
			obs_data_release(scene_settings);
			load_sources.push_back(s);
		}
		obs_data_release(sourceData);
	}

	for (obs_source_t *source : load_sources)
		obs_source_load(source);

	for (obs_source_t *source : ref_sources)
		obs_source_release(source);
}

static void LoadScene(obs_data_t *data, QString path)
{
	if (!data)
		return;
	obs_data_array_t *sourcesData = obs_data_get_array(data, "sources");
	if (!sourcesData)
		return;
	LoadSources(sourcesData, path);
	obs_data_array_release(sourcesData);
}

//--------------------NOTIFICATION HELPERS--------------------
void SendTrayNotification(QSystemTrayIcon::MessageIcon icon, const QString &title, const QString &body)
{
	if (notificationsMuted) {
		blog(LOG_INFO, "[StreamUP] Notifications are muted.");
		return;
	}

	if (!QSystemTrayIcon::isSystemTrayAvailable() || !QSystemTrayIcon::supportsMessages())
		return;

	SystemTrayNotification *notification = new SystemTrayNotification{icon, title, body};

	obs_queue_task(
		OBS_TASK_UI,
		[](void *param) {
			auto notification = static_cast<SystemTrayNotification *>(param);
			void *systemTrayPtr = obs_frontend_get_system_tray();
			if (systemTrayPtr) {
				auto systemTray = static_cast<QSystemTrayIcon *>(systemTrayPtr);
				QString prefixedTitle = "[StreamUP] " + notification->title;
				systemTray->showMessage(prefixedTitle, notification->body, notification->icon);
			}
			delete notification;
		},
		(void *)notification, false);
}

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

	if (strcmp(PLATFORM_NAME, "windows") == 0) {
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

std::string GetMostRecentFile(std::string dirpath)
{
	auto it = std::filesystem::directory_iterator(dirpath);
	auto last_write_time = decltype(it->last_write_time())::min();
	std::filesystem::directory_entry newest_file;

	for (const auto &entry : it) {
		if (entry.is_regular_file() && entry.path().extension() == ".txt") {
			auto current_write_time = entry.last_write_time();
			if (current_write_time > last_write_time) {
				last_write_time = current_write_time;
				newest_file = entry;
			}
		}
	}

	return newest_file.path().string();
}

//--------------------UI HELPERS--------------------
void ShowDialogOnUIThread(const std::function<void()> &dialogFunction)
{
	QMetaObject::invokeMethod(qApp, dialogFunction, Qt::QueuedConnection);
}

void CopyToClipboard(const QString &text)
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(text);
}

QDialog *CreateDialogWindow(const char *windowTitle)
{
	QDialog *dialog = new QDialog();
	dialog->setWindowTitle(obs_module_text(windowTitle));
	dialog->setWindowFlags(Qt::Window);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	return dialog;
}

QLabel *CreateRichTextLabel(const QString &text, bool bold, bool wrap, Qt::Alignment alignment = Qt::Alignment())
{
	QLabel *label = new QLabel;
	label->setText(text);
	label->setTextFormat(Qt::RichText);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	if (bold) {
		label->setStyleSheet("font-weight: bold; font-size: 14px;");
	}
	if (wrap) {
		label->setWordWrap(true);
	}
	if (alignment != Qt::Alignment()) {
		label->setAlignment(alignment);
	}
	return label;
}

QLabel *CreateIconLabel(const QStyle::StandardPixmap &iconName)
{
	QLabel *icon = new QLabel();
	int pixmapSize = (strcmp(PLATFORM_NAME, "macos") == 0) ? 16 : 64;
	icon->setPixmap(QApplication::style()->standardIcon(iconName).pixmap(pixmapSize, pixmapSize));
	icon->setStyleSheet("padding-top: 3px;");
	return icon;
}

QHBoxLayout *AddIconAndText(const QStyle::StandardPixmap &iconText, const char *labelText)
{
	QLabel *icon = CreateIconLabel(iconText);
	QLabel *text = CreateRichTextLabel(obs_module_text(labelText), false, true, Qt::AlignTop);

	QHBoxLayout *iconTextLayout = new QHBoxLayout();
	iconTextLayout->addWidget(icon, 0, Qt::AlignTop);
	iconTextLayout->addSpacing(10);
	text->setWordWrap(true);
	iconTextLayout->addWidget(text, 1);

	return iconTextLayout;
}

QVBoxLayout *CreateVBoxLayout(QWidget *parent)
{
	QVBoxLayout *layout = new QVBoxLayout(parent);
	layout->setContentsMargins(20, 15, 20, 10);
	return layout;
}

void CreateLabelWithLink(QLayout *layout, const QString &text, const QString &url, int row, int column)
{
	QLabel *label = CreateRichTextLabel(text, false, false, Qt::AlignCenter);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	QObject::connect(label, &QLabel::linkActivated, [url]() { QDesktopServices::openUrl(QUrl(url)); });
	static_cast<QGridLayout *>(layout)->addWidget(label, row, column);
}

void CreateButton(QLayout *layout, const QString &text, const std::function<void()> &onClick)
{
	QPushButton *button = new QPushButton(text);
	QObject::connect(button, &QPushButton::clicked, onClick);
	layout->addWidget(button);
}

void CreateToolDialog(const char *infoText1, const char *infoText2, const char *infoText3, const QString &titleText,
		      const std::function<void()> &buttonCallback, const QString &jsonString, const char *how1, const char *how2,
		      const char *how3, const char *how4, const char *notificationMessage)
{
	ShowDialogOnUIThread([infoText1, infoText2, infoText3, titleText, buttonCallback, jsonString, how1, how2, how3, how4,
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

		QDialog *dialog = CreateDialogWindow(titleTextChar);
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		QHBoxLayout *buttonLayout = new QHBoxLayout();

		CreateButton(buttonLayout, obs_module_text("Cancel"), [dialog]() { dialog->close(); });

		CreateButton(buttonLayout, titleStr, [=]() {
			buttonCallback();
			if (notificationMessage) {
				SendTrayNotification(QSystemTrayIcon::Information, titleStr, obs_module_text(notificationMessage));
			}
			dialog->close();
		});

		dialogLayout->addLayout(AddIconAndText(QStyle::SP_MessageBoxInformation, infoText1));
		dialogLayout->addSpacing(10);

		QLabel *info2 = CreateRichTextLabel(infoText2Str, false, true, Qt::AlignTop);
		dialogLayout->addWidget(info2, 0, Qt::AlignTop);
		dialogLayout->addSpacing(10);

		QGroupBox *info3Box = new QGroupBox(obs_module_text("HowToUse"));
		info3Box->setMinimumWidth(350);
		QVBoxLayout *info3BoxLayout = CreateVBoxLayout(info3Box);
		QLabel *info3 = CreateRichTextLabel(infoText3Str, false, true);
		QLabel *howTo1 = CreateRichTextLabel(howTo1Str, false, true);
		QLabel *howTo2 = CreateRichTextLabel(howTo2Str, false, true);
		QLabel *howTo3 = CreateRichTextLabel(howTo3Str, false, true);
		QLabel *howTo4 = CreateRichTextLabel(howTo4Str, false, true);
		info3BoxLayout->addWidget(info3);
		info3BoxLayout->addSpacing(5);
		info3BoxLayout->addWidget(howTo1);
		info3BoxLayout->addWidget(howTo2);
		info3BoxLayout->addWidget(howTo3);
		info3BoxLayout->addWidget(howTo4);

		QPushButton *copyJsonButton = new QPushButton(obs_module_text("CopyWebsocketJson"));
		copyJsonButton->setToolTip(obs_module_text("CopyWebsocketJsonTooltip"));
		QObject::connect(copyJsonButton, &QPushButton::clicked, [=]() { CopyToClipboard(jsonString); });
		info3BoxLayout->addWidget(copyJsonButton);
		dialogLayout->addWidget(info3Box);
		dialogLayout->addSpacing(10);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

//-------------------PLUGIN MANAGEMENT AND SETTINGS-------------------
std::vector<std::string> SplitString(const std::string &input, char delimiter)
{
	std::vector<std::string> parts;
	std::istringstream stream(input);
	std::string part;

	while (std::getline(stream, part, delimiter)) {
		parts.push_back(part);
	}

	return parts;
}

bool IsVersionLessThan(const std::string &version1, const std::string &version2)
{
	std::vector<std::string> parts1 = SplitString(version1, '.');
	std::vector<std::string> parts2 = SplitString(version2, '.');

	while (parts1.size() < parts2.size())
		parts1.push_back("0");
	while (parts2.size() < parts1.size())
		parts2.push_back("0");

	try {
		for (size_t i = 0; i < parts1.size(); ++i) {
			int num1 = std::stoi(parts1[i]);
			int num2 = std::stoi(parts2[i]);

			if (num1 < num2)
				return true;
			else if (num1 > num2)
				return false;
		}
		return false; // Versions are equal
	} catch (const std::exception &) {
		return false;
	}
}

std::string SearchStringInFile(const char *path, const char *search)
{
	std::string filepath = GetMostRecentFile(path);
	FILE *file = fopen(filepath.c_str(), "r+");
	char line[256];
	std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	std::regex version_regex_double("[0-9]+\\.[0-9]+");

	if (file) {
		while (fgets(line, sizeof(line), file)) {
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

	for (const auto &module : all_plugins) {
		const std::string &plugin_name = module.first;
		const PluginInfo &plugin_info = module.second;
		const std::string &search_string = plugin_info.searchString;

		std::string installed_version = SearchStringInFile(filepath, search_string.c_str());

		if (!installed_version.empty()) {
			installedPlugins.emplace_back(plugin_name, installed_version);
		}
	}

	bfree(filepath);

	return installedPlugins;
}

std::string GetPlatformURL(const PluginInfo &plugin_info)
{
	std::string url;
	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		url = plugin_info.windowsURL;
	} else if (strcmp(PLATFORM_NAME, "macos") == 0) {
		url = plugin_info.macURL;
	} else if (strcmp(PLATFORM_NAME, "linux") == 0) {
		url = plugin_info.linuxURL;
	} else {
		url = plugin_info.windowsURL;
	}
	return url;
}

//-------------------ERROR AND UPDATE HANDLING-------------------
void ErrorDialog(const QString &errorMessage)
{
	ShowDialogOnUIThread([errorMessage]() {
		QDialog *dialog = CreateDialogWindow("WindowErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		QString displayMessage = errorMessage.isEmpty() ? "Unknown error occurred." : errorMessage;

		dialogLayout->addLayout(AddIconAndText(QStyle::SP_MessageBoxCritical, displayMessage.toUtf8().constData()));

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, "OK", [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

void PluginsUpToDateOutput(bool manuallyTriggered)
{
	if (manuallyTriggered) {
		ShowDialogOnUIThread([]() {
			QDialog *dialog = CreateDialogWindow("WindowUpToDateTitle");
			QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
			dialogLayout->setContentsMargins(20, 15, 20, 10);

			dialogLayout->addLayout(AddIconAndText(QStyle::SP_DialogApplyButton, "WindowUpToDateMessage"));

			QHBoxLayout *buttonLayout = new QHBoxLayout();
			CreateButton(buttonLayout, obs_module_text("OK"), [dialog]() { dialog->close(); });

			dialogLayout->addLayout(buttonLayout);
			dialog->setLayout(dialogLayout);
			dialog->show();
		});
	}
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate)
{
	ShowDialogOnUIThread([errorMsgMissing, errorMsgUpdate]() {
		QDialog *dialog = CreateDialogWindow("WindowPluginErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 20);

		const char *errorText = (errorMsgMissing != "NULL") ? "WindowPluginErrorMissing" : "WindowPluginErrorUpdating";
		dialogLayout->addLayout(AddIconAndText(QStyle::SP_MessageBoxWarning, errorText));
		dialogLayout->addSpacing(10);

		QLabel *pluginErrorInfo = CreateRichTextLabel(obs_module_text("WindowPluginErrorInfo"), false, true);
		dialogLayout->addWidget(pluginErrorInfo);
		dialogLayout->addSpacing(10);

		if (!errorMsgUpdate.empty()) {
			QLabel *pluginsToUpdateList =
				CreateRichTextLabel(QString::fromStdString(errorMsgUpdate), false, false, Qt::AlignCenter);
			QGroupBox *pluginsToUpdateBox = new QGroupBox(obs_module_text("WindowPluginErrorUpdateGroup"));
			QVBoxLayout *pluginsToUpdateBoxLayout = new QVBoxLayout(pluginsToUpdateBox);
			pluginsToUpdateBoxLayout->addWidget(pluginsToUpdateList);
			dialogLayout->addWidget(pluginsToUpdateBox);
			if (errorMsgMissing != "NULL") {
				dialogLayout->addSpacing(10);
			}
		}

		if (errorMsgMissing != "NULL") {
			QLabel *pluginsMissingList =
				CreateRichTextLabel(QString::fromStdString(errorMsgMissing), false, false, Qt::AlignCenter);
			QGroupBox *pluginsMissingBox = new QGroupBox(obs_module_text("WindowPluginErrorMissingGroup"));
			QVBoxLayout *pluginsMissingBoxLayout = new QVBoxLayout(pluginsMissingBox);
			pluginsMissingBoxLayout->addWidget(pluginsMissingList);
			dialogLayout->addWidget(pluginsMissingBox);
		}

		if (errorMsgMissing != "NULL") {
			QLabel *pluginstallerLabel =
				CreateRichTextLabel(obs_module_text("WindowPluginErrorFooter"), false, false, Qt::AlignCenter);
			dialogLayout->addWidget(pluginstallerLabel);
		}

		QHBoxLayout *buttonLayout = new QHBoxLayout();

		CreateButton(buttonLayout, obs_module_text("OK"), [dialog]() { dialog->close(); });

		if (errorMsgMissing != "NULL") {
			QPushButton *pluginstallerButton = new QPushButton(obs_module_text("MenuDownloadPluginstaller"));
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

void CheckAllPluginsForUpdates(bool manuallyTriggered)
{
	if (all_plugins.empty()) {
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return;
	}

	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgUpdate = "";
	std::vector<std::pair<std::string, std::string>> installedPlugins = GetInstalledPlugins();

	for (const auto &plugin : installedPlugins) {
		const std::string &plugin_name = plugin.first;
		const std::string &installed_version = plugin.second;
		const PluginInfo &plugin_info = all_plugins[plugin_name];
		const std::string &required_version = plugin_info.version;

		if (installed_version != required_version && IsVersionLessThan(installed_version, required_version)) {
			version_mismatch_modules.emplace(plugin_name, installed_version);
		}
	}

	std::string appDataPath = GetLocalAppDataPath();
	std::string filePath = appDataPath + "/StreamUP-OutdatedPluginsList.txt";

	std::ofstream outFile(filePath, std::ios::out);
	if (outFile.is_open()) {
		for (const auto &module : version_mismatch_modules) {
			outFile << module.first << "\n";
		}
		outFile.close();
	} else {
		std::cerr << "Error: Unable to open file for writing.\n";
	}

	if (!version_mismatch_modules.empty()) {
		errorMsgUpdate += "<div style=\"text-align: center;\">"
				  "<table style=\"border-collapse: collapse; width: 100%;\">"
				  "<tr style=\"background-color: #212121;\">"
				  "<th style=\"padding: 4px 10px; text-align: center;\">Plugin Name</th>"
				  "<th style=\"padding: 4px 10px; text-align: center;\">Installed</th>"
				  "<th style=\"padding: 4px 10px; text-align: center;\">Current</th>"
				  "<th style=\"padding: 4px 10px; text-align: center;\">Direct Download</th>"
				  "</tr>";

		for (const auto &module : version_mismatch_modules) {
			const PluginInfo &plugin_info = all_plugins[module.first];
			const std::string &required_version = plugin_info.version;
			const std::string &forum_link = plugin_info.generalURL;
			const std::string &direct_download_link = GetPlatformURL(plugin_info);

			errorMsgUpdate += "<tr style=\"border: 1px solid #ddd;\">"
					  "<td style=\"padding: 2px 10px; text-align: left;\"><a href=\"" +
					  forum_link + "\">" + module.first +
					  "</a></td>"
					  "<td style=\"padding: 2px 10px; text-align: center;\">" +
					  module.second +
					  "</td>"
					  "<td style=\"padding: 2px 10px; text-align: center;\">" +
					  required_version +
					  "</td>"
					  "<td style=\"padding: 2px 10px; text-align: center;\"><a href=\"" +
					  direct_download_link +
					  "\">Download</a></td>"
					  "</tr>";
		}

		errorMsgUpdate += "</table></div>";
		PluginsHaveIssue("NULL", errorMsgUpdate);
		version_mismatch_modules.clear();
	} else {
		PluginsUpToDateOutput(manuallyTriggered);
	}
}

//-------------------PLUGINS AND INITIALIZATION-------------------
size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *out)
{
	size_t totalSize = size * nmemb;
	out->append((char *)contents, totalSize);
	return totalSize;
}

void *MakeApiRequest(void *arg)
{
	RequestData *data = (RequestData *)arg;
	CURL *curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, data->url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data->response);
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			blog(LOG_INFO, "[StreamUP] curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_easy_cleanup(curl);
	}
	return NULL;
}

void InitialiseRequiredModules()
{
	pthread_t thread;
	RequestData req_data;
	req_data.url = "https://api.streamup.tips/plugins";

	if (pthread_create(&thread, NULL, MakeApiRequest, &req_data)) {
		blog(LOG_INFO, "[StreamUP] Error creating thread\n");
		return;
	}

	if (pthread_join(thread, NULL)) {
		blog(LOG_INFO, "[StreamUP] Error joining thread\n");
		return;
	}

	std::string api_response = req_data.response;

	if (api_response.find("Error:") != std::string::npos) {
		ErrorDialog(QString::fromStdString(api_response));
		return;
	}

	if (api_response == "") {
		blog(LOG_INFO, "[StreamUP] Error loading plugins from %s", req_data.url.c_str());
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return;
	}

	obs_data_t *data = obs_data_create_from_json(api_response.c_str());
	obs_data_array_t *plugins = obs_data_get_array(data, "plugins");

	size_t count = obs_data_array_count(plugins);
	for (size_t i = 0; i < count; ++i) {
		obs_data_t *plugin = obs_data_array_item(plugins, i);

		std::string name = obs_data_get_string(plugin, "name");

		PluginInfo info;
		info.version = obs_data_get_string(plugin, "version");
		obs_data_t *downloads = obs_data_get_obj(plugin, "downloads");
		if (downloads) {
			info.windowsURL = obs_data_get_string(downloads, "windows");
			info.macURL = obs_data_get_string(downloads, "macOS");
			info.linuxURL = obs_data_get_string(downloads, "linux");
			obs_data_release(downloads);
		}
		info.searchString = obs_data_get_string(plugin, "searchString");
		info.generalURL = obs_data_get_string(plugin, "url");
		info.moduleName = obs_data_get_string(plugin, "moduleName");

		all_plugins[name] = info;

		if (obs_data_get_bool(plugin, "required")) {
			required_plugins[name] = info;
		}

		obs_data_release(plugin);
	}

	obs_data_array_release(plugins);
	obs_data_release(data);
}

bool CheckrequiredOBSPlugins(bool isLoadStreamUpFile = false)
{
	if (required_plugins.empty()) {
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return false;
	}

	std::map<std::string, std::string> missing_modules;
	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgMissing = "";
	std::string errorMsgUpdate = "";
	char *filepath = GetFilePath();
	if (filepath == NULL) {
		return false;
	}

	for (const auto &module : required_plugins) {
		const std::string &plugin_name = module.first;
		const PluginInfo &plugin_info = module.second;
		const std::string &required_version = plugin_info.version;
		const std::string &search_string = plugin_info.searchString;

		if (search_string.find("[ignore]") != std::string::npos) {
			continue; // Skip to the next iteration
		}

		std::string installed_version = SearchStringInFile(filepath, search_string.c_str());

		if (installed_version.empty() && plugin_info.required) {
			missing_modules.emplace(plugin_name, required_version);
		} else if (!installed_version.empty() && installed_version != required_version && plugin_info.required &&
			   IsVersionLessThan(installed_version, required_version)) {
			version_mismatch_modules.emplace(plugin_name, installed_version);
		}
	}

	bfree(filepath);

	bool hasUpdates = !version_mismatch_modules.empty();
	bool hasMissingPlugins = !missing_modules.empty();

	if (hasUpdates || hasMissingPlugins) {
		if (hasUpdates) {
			errorMsgUpdate += "<div style=\"text-align: center;\">"
					  "<table style=\"border-collapse: collapse; width: 100%;\">"
					  "<tr style=\"background-color: #212121;\">"
					  "<th style=\"padding: 4px 10px; text-align: center;\">Plugin Name</th>"
					  "<th style=\"padding: 4px 10px; text-align: center;\">Installed</th>"
					  "<th style=\"padding: 4px 10px; text-align: center;\">Current</th>"
					  "<th style=\"padding: 4px 10px; text-align: center;\">Direct Download</th>"
					  "</tr>";

			for (const auto &module : version_mismatch_modules) {
				const PluginInfo &plugin_info = all_plugins[module.first];
				const std::string &required_version = plugin_info.version;
				const std::string &forum_link = plugin_info.generalURL;
				const std::string &direct_download_link = GetPlatformURL(plugin_info);

				errorMsgUpdate += "<tr style=\"border: 1px solid #ddd;\">"
						  "<td style=\"padding: 2px 10px; text-align: left;\"><a href=\"" +
						  forum_link + "\">" + module.first +
						  "</a></td>"
						  "<td style=\"padding: 2px 10px; text-align: center;\">" +
						  module.second +
						  "</td>"
						  "<td style=\"padding: 2px 10px; text-align: center;\">" +
						  required_version +
						  "</td>"
						  "<td style=\"padding: 2px 10px; text-align: center;\"><a href=\"" +
						  direct_download_link +
						  "\">Download</a></td>"
						  "</tr>";
			}

			errorMsgUpdate += "</table></div>";

			if (!errorMsgUpdate.empty() && errorMsgUpdate.substr(errorMsgUpdate.length() - 4) == "<br>") {
				errorMsgUpdate = errorMsgUpdate.substr(0, errorMsgUpdate.length() - 4);
			}
		}

		if (hasMissingPlugins) {
			errorMsgMissing += "<div style=\"text-align: center;\">"
					   "<table style=\"border-collapse: collapse; width: 100%;\">"
					   "<tr style=\"background-color: #212121;\">"
					   "<th style=\"padding: 4px 10px; text-align: center;\">Plugin Name</th>"
					   "<th style=\"padding: 4px 10px; text-align: center;\">Direct Download</th>"
					   "</tr>";

			for (auto it = missing_modules.begin(); it != missing_modules.end(); ++it) {
				const std::string &moduleName = it->first;
				const PluginInfo &pluginInfo = required_plugins[moduleName];
				const std::string &forum_link = pluginInfo.generalURL;
				const std::string &direct_download_link = GetPlatformURL(pluginInfo);

				errorMsgMissing += "<tr style=\"border: 1px solid #ddd;\">"
						   "<td style=\"padding: 2px 10px; text-align: left;\"><a href=\"" +
						   forum_link + "\">" + moduleName +
						   "</a></td>"
						   "<td style=\"padding: 2px 10px; text-align: center;\"><a href=\"" +
						   direct_download_link +
						   "\">Download</a></td>"
						   "</tr>";
			}

			errorMsgMissing += "</table></div>";

			if (!errorMsgMissing.empty() && errorMsgMissing.substr(errorMsgMissing.length() - 4) == "<br>") {
				errorMsgMissing = errorMsgMissing.substr(0, errorMsgMissing.length() - 4);
			}
		}

		PluginsHaveIssue(hasMissingPlugins ? errorMsgMissing : "NULL", errorMsgUpdate);

		missing_modules.clear();
		version_mismatch_modules.clear();
		return false;

	} else {
		if (!isLoadStreamUpFile) {
			PluginsUpToDateOutput(true);
		}
		return true;
	}
}

//-------------------- HELPER FUNCTIONS--------------------
static bool EnumSceneItemsCallback(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	SceneItemEnumData *data = static_cast<SceneItemEnumData *>(param);
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
	SceneItemEnumData data;

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

bool RefreshAudioMonitoring(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	const char *source_name = obs_source_get_name(source);

	obs_monitoring_type original_monitoring_type = obs_source_get_monitoring_type(source);

	if (original_monitoring_type != OBS_MONITORING_TYPE_NONE) {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
		obs_source_set_monitoring_type(source, original_monitoring_type);

		blog(LOG_INFO, "[StreamUP] '%s' has refreshed audio monitoring type: '%s'", source_name,
		     MonitoringTypeToString(original_monitoring_type));
	}

	return true;
}

bool RefreshBrowserSources(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	const char *source_id = obs_source_get_id(source);
	const char *source_name = obs_source_get_name(source);

	if (strcmp(source_id, "browser_source") == 0) {
		obs_data_t *settings = obs_source_get_settings(source);
		int fps = obs_data_get_int(settings, "fps");

		if (fps % 2 == 0) {
			obs_data_set_int(settings, "fps", fps + 1);
		} else {
			obs_data_set_int(settings, "fps", fps - 1);
		}
		obs_source_update(source, settings);
		blog(LOG_INFO, "[StreamUP] Refreshed '%s' browser source", source_name);

		obs_data_release(settings);
	}

	return true;
}

void RefreshAudioMonitoringDialog()
{
	CreateToolDialog(
		"RefreshAudioMonitoringInfo1", "RefreshAudioMonitoringInfo2", "RefreshAudioMonitoringInfo3",
		"RefreshAudioMonitoring", []() { obs_enum_sources(RefreshAudioMonitoring, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_audio_monitoring",
                            "requestData": null
                        }
                    })",
		"RefreshAudioMonitoringHowTo1", "RefreshAudioMonitoringHowTo2", "RefreshAudioMonitoringHowTo3",
		"RefreshAudioMonitoringHowTo4", "RefreshAudioMonitoringNotification");
}

void RefreshBrowserSourcesDialog()
{
	CreateToolDialog(
		"RefreshBrowserSourcesInfo1", "RefreshBrowserSourcesInfo2", "RefreshBrowserSourcesInfo3", "RefreshBrowserSources",
		[]() { obs_enum_sources(RefreshBrowserSources, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_browser_sources",
                            "requestData": null
                        }
                    })",
		"RefreshBrowserSourcesHowTo1", "RefreshBrowserSourcesHowTo2", "RefreshBrowserSourcesHowTo3",
		"RefreshBrowserSourcesHowTo4", "RefreshBrowserSourcesNotification");
}

// Structure to hold data for finding selected items
struct SceneFindBoxData {
	std::vector<obs_sceneitem_t *> sceneItems;

	// Constructor
	SceneFindBoxData() = default;

	// Deleted copy/move constructors and assignment operators to avoid accidental copying
	SceneFindBoxData(const SceneFindBoxData &) = delete;
	SceneFindBoxData(SceneFindBoxData &&) = delete;
	SceneFindBoxData &operator=(const SceneFindBoxData &) = delete;
	SceneFindBoxData &operator=(SceneFindBoxData &&) = delete;
};

// Callback function to find selected scene items (handles groups as well)
bool FindSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	SceneFindBoxData *data = reinterpret_cast<SceneFindBoxData *>(param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (source) {
		// If the item is selected, add it to the list
		if (obs_sceneitem_selected(item)) {
			data->sceneItems.push_back(item);
		}

		// If the source is a group, recursively enumerate its items
		obs_scene_t *group_scene = obs_group_from_source(source);
		if (group_scene) {
			obs_scene_enum_items(group_scene, FindSelected, param); // Enumerate items inside the group
		}
	}

	return true; // Continue enumerating items
}

// Function to get the currently selected source name
const char *GetSelectedSourceFromCurrentScene()
{
	// Get the current scene source from the frontend
	obs_source_t *current_scene_source = obs_frontend_get_current_scene();
	if (!current_scene_source) {
		blog(LOG_INFO, "[StreamUP] No active scene.");
		return nullptr;
	}

	// Get the scene from the source
	obs_scene_t *scene = obs_scene_from_source(current_scene_source);
	obs_source_release(current_scene_source); // Always release the source when done

	if (!scene) {
		blog(LOG_INFO, "[StreamUP] No active scene found.");
		return nullptr;
	}

	// Data structure to hold the selected items found
	SceneFindBoxData data;

	// Enumerate through the scene items and find the selected ones, including groups
	obs_scene_enum_items(scene, FindSelected, &data);

	// If there is exactly one selected item, return its source name
	if (data.sceneItems.size() == 1) {
		obs_source_t *selected_source = obs_sceneitem_get_source(data.sceneItems[0]);
		return obs_source_get_name(selected_source); // Return the source name
	}

	blog(LOG_INFO, "[StreamUP] No selected source or multiple selected sources.");
	return nullptr; // No source or multiple sources selected
}

//-------------------LOCK SOURCE MANAGEMENT-------------------
bool CheckIfAnyUnlocked(obs_scene_t *scene);

bool CheckGroupItemsIfAnyUnlocked(obs_source_t *group)
{
	obs_scene_t *group_scene = obs_group_from_source(group);
	if (!group_scene) {
		return false;
	}

	return CheckIfAnyUnlocked(group_scene);
}

bool CheckIfAnyUnlocked(obs_scene_t *scene)
{
	bool any_unlocked = false;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
			bool *any_unlocked = static_cast<bool *>(param);
			obs_source_t *source = obs_sceneitem_get_source(item);

			UNUSED_PARAMETER(scene);

			if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
				if (CheckGroupItemsIfAnyUnlocked(source)) {
					*any_unlocked = true;
					return false; // Stop enumeration
				}
			}

			if (!obs_sceneitem_locked(item)) {
				*any_unlocked = true;
				return false; // Stop enumeration
			}
			return true; // Continue enumeration
		},
		&any_unlocked);
	return any_unlocked;
}

void ToggleLockGroupItems(obs_source_t *group, bool lock);

bool ToggleLockSceneItemCallback(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	UNUSED_PARAMETER(scene);

	bool *lock = static_cast<bool *>(param);
	obs_source_t *source = obs_sceneitem_get_source(item);

	if (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE) {
		ToggleLockGroupItems(source, *lock);
	}

	obs_sceneitem_set_locked(item, *lock);

	return true; // Return true to continue enumeration
}

void ToggleLockGroupItems(obs_source_t *group, bool lock)
{
	obs_scene_t *group_scene = obs_group_from_source(group);
	if (!group_scene) {
		return;
	}

	obs_scene_enum_items(group_scene, ToggleLockSceneItemCallback, &lock);
}

bool ToggleLockSceneItems(obs_scene_t *scene, bool lock)
{
	obs_scene_enum_items(scene, ToggleLockSceneItemCallback, &lock);
	return lock;
}

bool ToggleLockSourcesInCurrentScene(bool sendNotification = true)
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		blog(LOG_WARNING, "[StreamUP] No current scene found.");
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return false;
	}

	bool any_unlocked = CheckIfAnyUnlocked(scene);
	ToggleLockSceneItems(scene, any_unlocked);
	obs_source_release(current_scene);

	if (sendNotification) {
		if (any_unlocked) {
			blog(LOG_INFO, "[StreamUP] All sources in the current scene have been locked.");
			SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("SourceLockSystem"),
					     obs_module_text("LockedCurrentSources"));
		} else {
			blog(LOG_INFO, "[StreamUP] All sources in the current scene have been unlocked.");
			SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("SourceLockSystem"),
					     obs_module_text("UnlockedCurrentSources"));
		}
	}

	return any_unlocked;
}

bool CheckIfAnyUnlockedCallback(void *param, obs_source_t *source)
{
	bool *any_unlocked = static_cast<bool *>(param);
	obs_scene_t *scene = obs_scene_from_source(source);

	if (CheckIfAnyUnlocked(scene)) {
		*any_unlocked = true;
		return false; // Stop enumeration
	}

	return true; // Continue enumeration
}

bool CheckIfAnyUnlockedInAllScenes()
{
	bool any_unlocked = false;
	obs_enum_scenes(CheckIfAnyUnlockedCallback, &any_unlocked);
	return any_unlocked;
}

bool ToggleLockSceneItemsCallback(void *param, obs_source_t *source)
{
	bool *lock = static_cast<bool *>(param);
	obs_scene_t *scene = obs_scene_from_source(source);
	ToggleLockSceneItems(scene, *lock);
	return true; // Continue enumeration
}

void ToggleLockSourcesInAllScenes(bool lock)
{
	obs_enum_scenes(ToggleLockSceneItemsCallback, &lock);
}

bool ToggleLockAllSources(bool sendNotification = true)
{
	bool any_unlocked = CheckIfAnyUnlockedInAllScenes();
	ToggleLockSourcesInAllScenes(any_unlocked);

	if (sendNotification) {
		if (any_unlocked) {
			blog(LOG_INFO, "[StreamUP] All sources in all scenes have been locked.");
			SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("SourceLockSystem"),
					     obs_module_text("LockedAllSources"));
		} else {
			blog(LOG_INFO, "[StreamUP] All sources in all scenes have been unlocked.");
			SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("SourceLockSystem"),
					     obs_module_text("UnlockedAllSources"));
		}
	}

	return any_unlocked;
}

bool AreAllSourcesLockedInCurrentScene()
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		return false;
	}

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return false;
	}

	bool any_unlocked = CheckIfAnyUnlocked(scene);
	obs_source_release(current_scene);

	return !any_unlocked;
}

bool AreAllSourcesLockedInAllScenes()
{
	return !CheckIfAnyUnlockedInAllScenes();
}

void LockAllSourcesDialog()
{
	CreateToolDialog(
		"LockAllSourcesInfo1", "LockAllSourcesInfo2", "LockAllSourcesInfo3", "LockAllSources",
		[]() { ToggleLockAllSources(); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "toggleLockAllSources",
                            "requestData": null
                        }
                    })",
		"LockAllSourcesHowTo1", "LockAllSourcesHowTo2", "LockAllSourcesHowTo3", "LockAllSourcesHowTo4", NULL);
}

void LockAllCurrentSourcesDialog()
{
	CreateToolDialog(
		"LockAllCurrentSourcesInfo1", "LockAllCurrentSourcesInfo2", "LockAllCurrentSourcesInfo3", "LockAllCurrentSources",
		[]() { ToggleLockSourcesInCurrentScene(); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "toggleLockCurrentSources",
                            "requestData": null
                        }
                    })",
		"LockAllCurrentSourcesHowTo1", "LockAllCurrentSourcesHowTo2", "LockAllCurrentSourcesHowTo3",
		"LockAllCurrentSourcesHowTo4", NULL);
}

//--------------------WEBSOCKET VENDOR REQUESTS--------------------
obs_websocket_vendor vendor = nullptr;

bool LoadStreamupFileFromPath(const QString &file_path, bool forceLoad = false)
{
	if (!forceLoad) {
		if (!CheckrequiredOBSPlugins(true)) {
			return false;
		}
	}

	// Load the .streamup file from the file path
	obs_data_t *data = obs_data_create_from_json_file(QT_TO_UTF8(file_path));
	if (data) {
		LoadScene(data, QFileInfo(file_path).absolutePath());
		obs_data_release(data);
		return true;
	}

	return false;
}

void LoadStreamupFile(bool forceLoad = false)
{
	if (!forceLoad) {
		if (!CheckrequiredOBSPlugins(true)) {
			return;
		}
	}

	QString fileName =
		QFileDialog::getOpenFileName(nullptr, QT_UTF8(obs_module_text("Load")), QString(), "StreamUP File (*.streamup)");
	if (!fileName.isEmpty()) {
		LoadStreamupFileFromPath(fileName, forceLoad);
	}
}

void WebsocketRequestBitrate(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	obs_output_t *streamOutput = obs_frontend_get_streaming_output();
	if (!streamOutput || !obs_frontend_streaming_active()) {
		obs_data_set_string(response_data, "error", "Streaming is not active.");
		return;
	}

	uint64_t bytesSent = obs_output_get_total_bytes(streamOutput);
	uint64_t currentTime = os_gettime_ns();
	static uint64_t lastBytesSent = 0;
	static uint64_t lastTime = 0;
	static bool initialized = false;

	if (!initialized) {
		lastBytesSent = bytesSent;
		lastTime = currentTime;
		initialized = true;
		obs_data_set_int(response_data, "kbits-per-sec", 0);
		return;
	}

	if (bytesSent < lastBytesSent) {
		bytesSent = 0;
	}

	uint64_t bytesBetween = bytesSent - lastBytesSent;
	double timePassed = (currentTime - lastTime) / 1000000000.0;

	// Ensure timePassed is greater than zero to avoid division by zero
	uint64_t bytesPerSec = 0;
	if (timePassed > 0)
		bytesPerSec = bytesBetween / timePassed;

	uint64_t kbitsPerSec = (bytesPerSec * 8) / 1024;

	lastBytesSent = bytesSent;
	lastTime = currentTime;

	obs_data_set_int(response_data, "kbits-per-sec", kbitsPerSec);
}

void WebsocketRequestVersion(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

void WebsocketRequestCheckPlugins(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool pluginsUpToDate = CheckrequiredOBSPlugins(true);
	obs_data_set_bool(response_data, "success", pluginsUpToDate);
}

void WebsocketRequestLockAllSources(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool lockState = ToggleLockAllSources();
	obs_data_set_bool(response_data, "lockState", lockState);
}

void WebsocketRequestLockCurrentSources(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool lockState = ToggleLockSourcesInCurrentScene();
	obs_data_set_bool(response_data, "lockState", lockState);
}

void WebsocketRequestRefreshAudioMonitoring(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_enum_sources(RefreshAudioMonitoring, nullptr);
	obs_data_set_bool(response_data, "Audio monitoring refreshed", true);
}

void WebsocketRequestGetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	GetShowHideTransition(request_data, response_data, private_data, true);
}

void WebsocketRequestGetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	GetShowHideTransition(request_data, response_data, private_data, false);
}

void WebsocketRequestSetShowTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	SetShowHideTransition(request_data, response_data, private_data, true);
}

void WebsocketRequestSetHideTransition(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	SetShowHideTransition(request_data, response_data, private_data, false);
}

void WebsocketRequestRefreshBrowserSources(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_enum_sources(RefreshBrowserSources, nullptr);
	obs_data_set_bool(response_data, "Browser sources refreshed", true);
}

void WebsocketRequestGetCurrentSelectedSource(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (selected_source_name) {
		obs_data_set_string(response_data, "selectedSource", selected_source_name);
	} else {
		blog(LOG_INFO, "[StreamUP] No selected source.");
		obs_data_set_string(response_data, "selectedSource", "None");
	}
}

void WebsocketRequestGetOutputFilePath(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	char *path = obs_frontend_get_current_record_output_path();
	obs_data_set_string(response_data, "outputFilePath", path);
}

void WebsocketRequestVLCGetCurrentFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *source_name = obs_data_get_string(request_data, "sourceName");
	if (!source_name) {
		obs_data_set_string(response_data, "error", "No source name provided");
		return;
	}

	obs_source_t *source = obs_get_source_by_name(source_name);
	if (!source) {
		obs_data_set_string(response_data, "error", "Source not found");
		return;
	}

	if (strcmp(obs_source_get_unversioned_id(source), "vlc_source") == 0) {
		proc_handler_t *ph = obs_source_get_proc_handler(source);
		if (ph) {
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_string(&cd, "tag_id", "title");
			if (proc_handler_call(ph, "get_metadata", &cd)) {
				const char *title = calldata_string(&cd, "tag_data");
				if (title) {
					obs_data_set_string(response_data, "title", title);
				} else {
					obs_data_set_string(response_data, "error", "No title metadata found");
				}
			} else {
				obs_data_set_string(response_data, "error", "Failed to call get_metadata");
			}
			calldata_free(&cd);
		} else {
			obs_data_set_string(response_data, "error", "Failed to get procedure handler");
		}
	} else {
		obs_data_set_string(response_data, "error", "Source is not a VLC source");
	}

	obs_source_release(source);
}

void WebsocketOpenSourceProperties(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		blog(LOG_INFO, "[StreamUP] No source selected for properties.");
		return;
	}

	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		obs_frontend_open_source_properties(selected_source);
		obs_source_release(selected_source);
		obs_data_set_string(response_data, "status", "Properties opened.");
	} else {
		obs_data_set_string(response_data, "error", "Failed to find source.");
	}
}

void WebsocketOpenSourceFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		blog(LOG_INFO, "[StreamUP] No source selected for filters.");
		return;
	}

	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		obs_frontend_open_source_filters(selected_source);
		obs_source_release(selected_source);
		obs_data_set_string(response_data, "status", "Filters opened.");
	} else {
		obs_data_set_string(response_data, "error", "Failed to find source.");
	}
}

void WebsocketOpenSourceInteract(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		obs_data_set_string(response_data, "error", "No source selected.");
		blog(LOG_INFO, "[StreamUP] No source selected for interaction.");
		return;
	}

	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		obs_frontend_open_source_interaction(selected_source);
		obs_source_release(selected_source);
		obs_data_set_string(response_data, "status", "Interact window opened.");
	} else {
		obs_data_set_string(response_data, "error", "Failed to find source.");
	}
}

void WebsocketOpenSceneFilters(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		obs_data_set_string(response_data, "error", "No current scene.");
		blog(LOG_INFO, "[StreamUP] No current scene for filters.");
		return;
	}

	obs_frontend_open_source_filters(current_scene);
	obs_source_release(current_scene);
	obs_data_set_string(response_data, "status", "Scene filters opened.");
}

void WebsocketLoadStreamupFile(obs_data_t *request_data, obs_data_t *response_data, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	// Log the entire request for debugging
	const char *request_data_json = obs_data_get_json(request_data);
	blog(LOG_INFO, "Websocket request data: %s", request_data_json);

	// Extract the "file" parameter as a string (file path)
	const char *file_path = obs_data_get_string(request_data, "file");
	bool force_load = obs_data_get_bool(request_data, "force_load");

	if (!file_path || !strlen(file_path)) {
		// If the "file" path is missing, return an error response and log it
		blog(LOG_ERROR, "WebsocketLoadStreamupFile: 'file' parameter is missing or invalid");
		obs_data_set_string(response_data, "error", "'file' path is missing or invalid");
		return;
	}

	// Log the extracted file path for debugging
	blog(LOG_INFO, "Extracted 'file' path: %s", file_path);

	// Call the function to load the .streamup file from the path
	if (!LoadStreamupFileFromPath(QString::fromUtf8(file_path), force_load)) {
		obs_data_set_string(response_data, "error", "Failed to load streamup file");
		return;
	}

	// Return success response
	obs_data_set_string(response_data, "status", "success");
}

//--------------------HOTKEY HANDLERS--------------------
obs_hotkey_id refreshBrowserSourcesHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id refreshAudioMonitoringHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id lockAllSourcesHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id lockCurrentSourcesHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id openSourcePropertiesHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id openSourceFiltersHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id openSceneFiltersHotkey = OBS_INVALID_HOTKEY_ID;
obs_hotkey_id openSourceInteractHotkey = OBS_INVALID_HOTKEY_ID;

static void HotkeyRefreshBrowserSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(RefreshBrowserSources, nullptr);
	SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("RefreshBrowserSources"),
			     "Action completed successfully.");
}

static void HotkeyLockAllSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	ToggleLockAllSources();
}

static void HotkeyRefreshAudioMonitoring(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(RefreshAudioMonitoring, nullptr);
	SendTrayNotification(QSystemTrayIcon::Information, obs_module_text("RefreshAudioMonitoring"),
			     "Action completed successfully.");
}

static void HotkeyLockCurrentSources(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	ToggleLockSourcesInCurrentScene();
}

static void HotkeyOpenSourceProperties(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the name of the currently selected source
	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		blog(LOG_INFO, "[StreamUP] No source selected, cannot open properties.");
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the properties of the selected source
		obs_frontend_open_source_properties(selected_source);
		obs_source_release(selected_source);
	} else {
		blog(LOG_INFO, "[StreamUP] Failed to find source: %s", selected_source_name);
	}
}

static void HotkeyOpenSourceFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the name of the currently selected source
	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		blog(LOG_INFO, "[StreamUP] No source selected, cannot open filters.");
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the filters of the selected source
		obs_frontend_open_source_filters(selected_source);
		obs_source_release(selected_source);
	} else {
		blog(LOG_INFO, "[StreamUP] Failed to find source: %s", selected_source_name);
	}
}

static void HotkeyOpenSourceInteract(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the name of the currently selected source
	const char *selected_source_name = GetSelectedSourceFromCurrentScene();
	if (!selected_source_name) {
		blog(LOG_INFO, "[StreamUP] No source selected, cannot open interact window.");
		return;
	}

	// Find the source by name
	obs_source_t *selected_source = obs_get_source_by_name(selected_source_name);
	if (selected_source) {
		// Open the interact window of the selected source
		obs_frontend_open_source_interaction(selected_source);
		obs_source_release(selected_source); // Release reference count
	} else {
		blog(LOG_INFO, "[StreamUP] Failed to find source: %s", selected_source_name);
	}
}

static void HotkeyOpenSceneFilters(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);

	if (!pressed)
		return;

	// Get the current scene
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		blog(LOG_INFO, "[StreamUP] No current scene found, cannot open filters.");
		return;
	}

	// Open the filters of the current scene
	obs_frontend_open_source_filters(current_scene);

	// Release reference count for the current scene
	obs_source_release(current_scene);
}

static void SaveLoadHotkeys(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		// save hotkeys
		obs_data_array_t *hotkeySaveArray;

		hotkeySaveArray = obs_hotkey_save(refreshBrowserSourcesHotkey);
		obs_data_set_array(save_data, "refreshBrowserSourcesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(lockAllSourcesHotkey);
		obs_data_set_array(save_data, "lockAllSourcesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(refreshAudioMonitoringHotkey);
		obs_data_set_array(save_data, "refreshAudioMonitoringHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(lockCurrentSourcesHotkey);
		obs_data_set_array(save_data, "lockCurrentSourcesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSourceInteractHotkey);
		obs_data_set_array(save_data, "openSourceInteractHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSceneFiltersHotkey);
		obs_data_set_array(save_data, "openSceneFiltersHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSourceFiltersHotkey);
		obs_data_set_array(save_data, "openSourceFiltersHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

		hotkeySaveArray = obs_hotkey_save(openSourcePropertiesHotkey);
		obs_data_set_array(save_data, "openSourcePropertiesHotkey", hotkeySaveArray);
		obs_data_array_release(hotkeySaveArray);

	} else {
		// load hotkeys
		obs_data_array_t *hotkeyLoadArray;

		hotkeyLoadArray = obs_data_get_array(save_data, "refreshBrowserSourcesHotkey");
		obs_hotkey_load(refreshBrowserSourcesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "lockAllSourcesHotkey");
		obs_hotkey_load(lockAllSourcesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "refreshAudioMonitoringHotkey");
		obs_hotkey_load(refreshAudioMonitoringHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "lockCurrentSourcesHotkey");
		obs_hotkey_load(lockCurrentSourcesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSourceInteractHotkey");
		obs_hotkey_load(openSourceInteractHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSceneFiltersHotkey");
		obs_hotkey_load(openSceneFiltersHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSourceFiltersHotkey");
		obs_hotkey_load(openSourceFiltersHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);

		hotkeyLoadArray = obs_data_get_array(save_data, "openSourcePropertiesHotkey");
		obs_hotkey_load(openSourcePropertiesHotkey, hotkeyLoadArray);
		obs_data_array_release(hotkeyLoadArray);
	}
}

//--------------------MENU HELPERS--------------------
static obs_data_t *SaveLoadSettingsCallback(obs_data_t *save_data, bool saving)
{
	char *configPath = obs_module_config_path("configs.json");
	obs_data_t *data = nullptr;

	if (saving) {
		if (obs_data_save_json(save_data, configPath)) {
			blog(LOG_INFO, "[StreamUP] Settings saved to %s", configPath);
		} else {
			blog(LOG_WARNING, "[StreamUP] Failed to save settings to file.");
		}
	} else {
		data = obs_data_create_from_json_file(configPath);

		if (!data) {
			blog(LOG_INFO, "[StreamUP] Settings not found. Creating default settings...");
			char *config_path = obs_module_config_path("");
			if (config_path) {
				os_mkdirs(config_path);
				bfree(config_path);
			}

			data = obs_data_create();
			obs_data_set_bool(data, "run_at_startup", true);
			obs_data_set_bool(data, "notifications_mute", false);

			if (obs_data_save_json(data, configPath)) {
				blog(LOG_INFO, "[StreamUP] Default settings saved to %s", configPath);
			} else {
				blog(LOG_WARNING, "[StreamUP] Failed to save default settings to file.");
			}
		} else {
			blog(LOG_INFO, "[StreamUP] Settings loaded successfully from %s", configPath);
		}
	}

	bfree(configPath);
	return data;
}

QString GetForumLink(const std::string &pluginName)
{
	const PluginInfo &pluginInfo = all_plugins[pluginName];
	return QString::fromStdString(pluginInfo.generalURL);
}

void SetLabelWithSortedModules(QLabel *label, const std::vector<std::string> &moduleNames)
{
	QString text;
	if (moduleNames.empty()) {
		text = obs_module_text("WindowSettingsUpdaterIncompatibleModules");
	} else {
		for (const std::string &moduleName : moduleNames) {
			if (!text.isEmpty()) {
				text += "<br>";
			}
			text += QString::fromStdString(moduleName);
		}
	}

	label->setMaximumWidth(300);
	label->setWordWrap(true);
	label->setTextFormat(Qt::RichText);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	label->setText(text);
}

std::vector<std::string> SearchModulesInFile(const char *path)
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

	std::string filepath = GetMostRecentFile(path);
	FILE *file = fopen(filepath.c_str(), "r");
	std::vector<std::string> collected_modules;
	std::regex timestamp_regex("^[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}:");

	if (file) {
		char line[256];
		bool in_section = false;

		while (fgets(line, sizeof(line), file) != NULL) {
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
				if (strcmp(PLATFORM_NAME, "windows") == 0) {
					suffix_pos = str_line.find(".dll");
				} else if (strcmp(PLATFORM_NAME, "linux") == 0) {
					suffix_pos = str_line.find(".so");
				}

				if (suffix_pos != std::string::npos) {
					str_line = str_line.substr(0, suffix_pos);
				}

				if (ignoreModules.find(str_line) == ignoreModules.end()) {
					bool foundInApi = false;
					for (const auto &pair : all_plugins) {
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

//--------------------SETTINGS MENU--------------------
void InstalledPluginsDialog()
{
	ShowDialogOnUIThread([]() {
		auto installedPlugins = GetInstalledPlugins();

		QDialog *dialog = CreateDialogWindow("WindowSettingsInstalledPlugins");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		dialogLayout->addLayout(AddIconAndText(QStyle::SP_MessageBoxInformation, "WindowSettingsInstalledPluginsInfo1"));
		dialogLayout->addSpacing(5);

		dialogLayout->addWidget(
			CreateRichTextLabel(obs_module_text("WindowSettingsInstalledPluginsInfo2"), false, true, Qt::AlignTop));
		dialogLayout->addWidget(
			CreateRichTextLabel(obs_module_text("WindowSettingsInstalledPluginsInfo3"), false, true, Qt::AlignTop));

		QString compatiblePluginsString;
		if (installedPlugins.empty()) {
			compatiblePluginsString = obs_module_text("WindowSettingsInstalledPlugins");
		} else {
			for (const auto &plugin : installedPlugins) {
				const auto &pluginName = plugin.first;
				const auto &pluginVersion = plugin.second;
				const QString forumLink = GetForumLink(pluginName);

				compatiblePluginsString += "<a href=\"" + forumLink + "\">" + QString::fromStdString(pluginName) +
							   "</a> (" + QString::fromStdString(pluginVersion) + ")<br>";
			}
			if (compatiblePluginsString.endsWith("<br>")) {
				compatiblePluginsString.chop(4);
			}
		}

		QLabel *compatiblePluginsList = CreateRichTextLabel(compatiblePluginsString, false, false);
		QGroupBox *compatiblePluginsBox = new QGroupBox(obs_module_text("WindowSettingsUpdaterCompatible"));
		QVBoxLayout *compatiblePluginsBoxLayout = CreateVBoxLayout(compatiblePluginsBox);
		compatiblePluginsBoxLayout->addWidget(compatiblePluginsList);

		QScrollArea *compatibleScrollArea = new QScrollArea;
		compatibleScrollArea->setWidgetResizable(true);
		compatibleScrollArea->setWidget(compatiblePluginsBox);
		compatibleScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		compatibleScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		compatibleScrollArea->setMinimumWidth(200);

		QGroupBox *incompatiblePluginsBox = new QGroupBox(obs_module_text("WindowSettingsUpdaterIncompatible"));
		QVBoxLayout *incompatiblePluginsBoxLayout = CreateVBoxLayout(incompatiblePluginsBox);
		QLabel *incompatiblePluginsList = new QLabel;
		char *filePath = GetFilePath();
		SetLabelWithSortedModules(incompatiblePluginsList, SearchModulesInFile(filePath));
		bfree(filePath);
		incompatiblePluginsBoxLayout->addWidget(incompatiblePluginsList);

		QScrollArea *incompatibleScrollArea = new QScrollArea;
		incompatibleScrollArea->setWidgetResizable(true);
		incompatibleScrollArea->setWidget(incompatiblePluginsBox);
		incompatibleScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		incompatibleScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		incompatibleScrollArea->setMinimumWidth(200);

		QHBoxLayout *pluginBoxesLayout = new QHBoxLayout();
		pluginBoxesLayout->addWidget(compatibleScrollArea);
		pluginBoxesLayout->addWidget(incompatibleScrollArea);

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, obs_module_text("Close"), [dialog]() { dialog->close(); });

		pluginBoxesLayout->setAlignment(Qt::AlignHCenter);
		dialogLayout->addLayout(pluginBoxesLayout);
		dialogLayout->addLayout(buttonLayout);

		dialog->setLayout(dialogLayout);

		dialog->show();
	});
}

void SettingsDialog()
{
	ShowDialogOnUIThread([]() {
		obs_data_t *settings = SaveLoadSettingsCallback(nullptr, false);

		QDialog *dialog = CreateDialogWindow("WindowSettingsTitle");
		QFormLayout *dialogLayout = new QFormLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		QLabel *titleLabel = CreateRichTextLabel("General", true, false);
		dialogLayout->addRow(titleLabel);

		// Run at startup setting
		obs_properties_t *props = obs_properties_create();
		obs_property_t *runAtStartupProp =
			obs_properties_add_bool(props, "run_at_startup", obs_module_text("WindowSettingsRunOnStartup"));

		QCheckBox *runAtStartupCheckBox = new QCheckBox(obs_module_text("WindowSettingsRunOnStartup"));
		runAtStartupCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(runAtStartupProp)));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		QObject::connect(runAtStartupCheckBox, &QCheckBox::checkStateChanged, [=](int state) {
#else
		QObject::connect(runAtStartupCheckBox, &QCheckBox::stateChanged, [=](int state) {
#endif
			obs_data_set_bool(settings, obs_property_name(runAtStartupProp), state == Qt::Checked);
		});

		dialogLayout->addWidget(runAtStartupCheckBox);

		// Notifications mute setting
		obs_property_t *notificationsMuteProp =
			obs_properties_add_bool(props, "notifications_mute", obs_module_text("WindowSettingsNotificationsMute"));

		QCheckBox *notificationsMuteCheckBox = new QCheckBox(obs_module_text("WindowSettingsNotificationsMute"));
		notificationsMuteCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(notificationsMuteProp)));
		notificationsMuteCheckBox->setToolTip(obs_module_text("WindowSettingsNotificationsMuteTooltip"));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		QObject::connect(notificationsMuteCheckBox, &QCheckBox::checkStateChanged, [=](int state) {
#else
		QObject::connect(notificationsMuteCheckBox, &QCheckBox::stateChanged, [=](int state) {
#endif
			bool isChecked = (state == Qt::Checked);
			obs_data_set_bool(settings, obs_property_name(notificationsMuteProp), isChecked);
			notificationsMuted = isChecked;
		});

		dialogLayout->addWidget(notificationsMuteCheckBox);

		// Spacer
		dialogLayout->addItem(new QSpacerItem(0, 5));

		// Plugin management
		QLabel *pluginLabel = CreateRichTextLabel(obs_module_text("WindowSettingsPluginManagement"), true, false);
		QPushButton *pluginButton = new QPushButton(obs_module_text("WindowSettingsViewInstalledPlugins"));
		QObject::connect(pluginButton, &QPushButton::clicked, InstalledPluginsDialog);

		dialogLayout->addRow(pluginLabel);
		dialogLayout->addRow(pluginButton);

		// Buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, obs_module_text("Cancel"), [dialog, settings]() {
			obs_data_release(settings);
			dialog->close();
		});

		CreateButton(buttonLayout, obs_module_text("Save"), [=]() {
			SaveLoadSettingsCallback(settings, true);
			dialog->close();
		});

		QWidget *buttonWidget = new QWidget();
		buttonWidget->setLayout(buttonLayout);
		dialogLayout->addRow(buttonWidget);

		dialog->setLayout(dialogLayout);

		QObject::connect(dialog, &QDialog::finished, [=](int) {
			obs_data_release(settings);
			obs_properties_destroy(props);
		});

		dialog->show();
	});
}

//--------------------MAIN MENU--------------------

void AboutDialog()
{
	ShowDialogOnUIThread([]() {
		std::string version = PROJECT_VERSION;

		QDialog *dialog = CreateDialogWindow("WindowAboutTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		QString informationRaw = "StreamUP OBS plugin (version " + QString::fromStdString(version) +
					 ")<br>by <b>Andi Stone</b> (<b>Andilippi</b>)";
		dialogLayout->addLayout(AddIconAndText(QStyle::SP_MessageBoxInformation, informationRaw.toUtf8().constData()));
		dialogLayout->addSpacing(10);

		QGroupBox *supportBox = new QGroupBox(obs_module_text("Support"));
		supportBox->setMaximumWidth(500);
		QVBoxLayout *supportBoxLayout = CreateVBoxLayout(supportBox);
		supportBoxLayout->addWidget(
			CreateRichTextLabel(obs_module_text("WindowAboutSupport"), false, true, Qt::AlignCenter));

		// Create a clickable button that opens the new link
		QPushButton *membershipButton = new QPushButton("Andi's Memberships");
		membershipButton->setCursor(Qt::PointingHandCursor);
		membershipButton->setStyleSheet("QPushButton {"
						"  background-color: #fcd34d;"
						"  color: black;"
						"  border: none;"
						"  padding: 8px 16px;"
						"  font-weight: bold;"
						"  border-radius: 18px;"
						"  width: 200px;"
						"  height: 20px;"
						"}"
						"QPushButton:hover {"
						"  background-color: #fde68a;"
						"}");

		QObject::connect(membershipButton, &QPushButton::clicked,
				 []() { QDesktopServices::openUrl(QUrl("https://andilippi.co.uk")); });

		QHBoxLayout *centerButtonLayout = new QHBoxLayout;
		centerButtonLayout->addStretch();
		centerButtonLayout->addWidget(membershipButton);
		centerButtonLayout->addStretch();
		supportBoxLayout->addLayout(centerButtonLayout);

		QHBoxLayout *streamupLinksLayout = new QHBoxLayout;
		streamupLinksLayout->setSpacing(20);
		streamupLinksLayout->setAlignment(Qt::AlignCenter);

		auto createLinkButton = [](const QString &text, const QString &url, const QString &bgColor = "#93c5fd") {
			QPushButton *btn = new QPushButton(text);
			btn->setCursor(Qt::PointingHandCursor);
			btn->setStyleSheet("QPushButton {"
					   "  background-color: " +
					   bgColor +
					   ";"
					   "  color: black;"
					   "  border: none;"
					   "  padding: 8px 16px;"
					   "  font-weight: bold;"
					   "  border-radius: 18px;"
					   "}"
					   "QPushButton:hover {"
					   "  background-color: #bfdbfe;"
					   "}");
			QObject::connect(btn, &QPushButton::clicked, [url]() { QDesktopServices::openUrl(QUrl(url)); });
			return btn;
		};

		streamupLinksLayout->addWidget(createLinkButton("StreamUP Patreon", "https://patreon.com/streamup"));
		streamupLinksLayout->addWidget(createLinkButton("StreamUP Ko-Fi", "https://ko-fi.com/streamup"));

		supportBoxLayout->addLayout(streamupLinksLayout);

		dialogLayout->addWidget(supportBox);

		QGroupBox *socialBox = new QGroupBox(obs_module_text("WindowAboutSocialsTitle"));
		socialBox->setMaximumWidth(500);
		QVBoxLayout *socialBoxLayout = CreateVBoxLayout(socialBox);
		socialBoxLayout->addWidget(
			CreateRichTextLabel(obs_module_text("WindowAboutSocialsMsg"), false, true, Qt::AlignCenter));

		QHBoxLayout *socialLinksButtonLayout = new QHBoxLayout;
		socialLinksButtonLayout->setAlignment(Qt::AlignCenter);

		QPushButton *allLinksButton = createLinkButton("All Andi's Links", "https://doras.to/andi", "#a5b4fc");
		socialLinksButtonLayout->addWidget(allLinksButton);

		socialBoxLayout->addLayout(socialLinksButtonLayout);
		dialogLayout->addWidget(socialBox);

		dialogLayout->addSpacing(10);

		dialogLayout->addWidget(CreateRichTextLabel(obs_module_text("WindowAboutThanks"), false, true, Qt::AlignCenter));

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, obs_module_text("Donate"),
			     []() { QDesktopServices::openUrl(QUrl("https://paypal.me/andilippi")); });
		CreateButton(buttonLayout, obs_module_text("Close"), [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);

		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

//--------------------STARTUP COMMANDS--------------------
static void LoadMenu(QMenu *menu);

static void InitialiseMenu()
{
	QMenu *menu = new QMenu();
#if defined(_WIN32)
	// Windows: Add to main menu bar
	void *main_window_ptr = obs_frontend_get_main_window();
	if (!main_window_ptr) {
		blog(LOG_ERROR, "Could not find main window");
		return;
	}

	QMainWindow *main_window = static_cast<QMainWindow *>(main_window_ptr);
	QMenuBar *menuBar = main_window->menuBar();
	if (!menuBar) {
		blog(LOG_ERROR, "Could not find main menu bar");
		return;
	}

	QMenu *topLevelMenu = new QMenu(obs_module_text("StreamUP"), menuBar);
	menuBar->addMenu(topLevelMenu);
	menu = topLevelMenu;

#else
	// macOS and Linux: Add to Tools menu
	QAction *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("StreamUP")));
	action->setMenu(menu);
#endif

	// Connect dynamic loader
	QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenu(menu); });
}

static void LoadMenu(QMenu *menu)
{
	menu->clear();
	QAction *a;

	// Check if running on Windows platform
	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		a = menu->addAction(obs_module_text("MenuInstallProduct"));
		QObject::connect(a, &QAction::triggered,
				 []() { LoadStreamupFile(QApplication::keyboardModifiers() & Qt::ShiftModifier); });
		a = menu->addAction(obs_module_text("MenuDownloadProduct"));
		QObject::connect(a, &QAction::triggered, []() { QDesktopServices::openUrl(QUrl("https://streamup.tips/")); });

		a = menu->addAction(obs_module_text("MenuCheckRequirements"));
		QObject::connect(a, &QAction::triggered, []() { CheckrequiredOBSPlugins(); });
		menu->addSeparator();
	}

	// Check plugin updates
	a = menu->addAction(obs_module_text("MenuCheckPluginUpdates"));
	QObject::connect(a, &QAction::triggered, []() { CheckAllPluginsForUpdates(true); });

	// Create "Tools" submenu
	QMenu *toolsMenu = menu->addMenu(obs_module_text("MenuTools"));

	// Add actions to the "Tools" submenu
	a = toolsMenu->addAction(obs_module_text("MenuLockAllCurrentSources"));
	QObject::connect(a, &QAction::triggered, []() { LockAllCurrentSourcesDialog(); });

	a = toolsMenu->addAction(obs_module_text("MenuLockAllSources"));
	QObject::connect(a, &QAction::triggered, []() { LockAllSourcesDialog(); });

	toolsMenu->addSeparator();

	a = toolsMenu->addAction(obs_module_text("MenuRefreshAudioMonitoring"));
	QObject::connect(a, &QAction::triggered, []() { RefreshAudioMonitoringDialog(); });

	a = toolsMenu->addAction(obs_module_text("MenuRefreshBrowserSources"));
	QObject::connect(a, &QAction::triggered, []() { RefreshBrowserSourcesDialog(); });

	menu->addSeparator();

	// Add remaining actions
	a = menu->addAction(obs_module_text("MenuAbout"));
	QObject::connect(a, &QAction::triggered, []() { AboutDialog(); });

	a = menu->addAction(obs_module_text("MenuSettings"));
	QObject::connect(a, &QAction::triggered, []() { SettingsDialog(); });
}

static void RegisterWebsocketRequests()
{
	vendor = obs_websocket_register_vendor("streamup");
	if (!vendor)
		return;

	obs_websocket_vendor_register_request(vendor, "getOutputFilePath", WebsocketRequestGetOutputFilePath, nullptr);
	obs_websocket_vendor_register_request(vendor, "getCurrentSource", WebsocketRequestGetCurrentSelectedSource, nullptr);
	obs_websocket_vendor_register_request(vendor, "getShowTransition", WebsocketRequestGetShowTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "getHideTransition", WebsocketRequestGetHideTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "setShowTransition", WebsocketRequestSetShowTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "setHideTransition", WebsocketRequestSetHideTransition, nullptr);
	obs_websocket_vendor_register_request(vendor, "toggleLockCurrentSources", WebsocketRequestLockCurrentSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "toggleLockAllSources", WebsocketRequestLockAllSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "getBitrate", WebsocketRequestBitrate, nullptr);
	obs_websocket_vendor_register_request(vendor, "version", WebsocketRequestVersion, nullptr);
	obs_websocket_vendor_register_request(vendor, "check_plugins", WebsocketRequestCheckPlugins, nullptr);
	obs_websocket_vendor_register_request(vendor, "refresh_audio_monitoring", WebsocketRequestRefreshAudioMonitoring, nullptr);
	obs_websocket_vendor_register_request(vendor, "refresh_browser_sources", WebsocketRequestRefreshBrowserSources, nullptr);
	obs_websocket_vendor_register_request(vendor, "vlcGetCurrentFile", WebsocketRequestVLCGetCurrentFile, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSourceProperties", WebsocketOpenSourceProperties, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSourceFilters", WebsocketOpenSourceFilters, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSourceInteract", WebsocketOpenSourceInteract, nullptr);
	obs_websocket_vendor_register_request(vendor, "openSceneFilters", WebsocketOpenSceneFilters, nullptr);
	obs_websocket_vendor_register_request(vendor, "loadStreamupFile", WebsocketLoadStreamupFile, nullptr);
}

static void RegisterHotkeys()
{
	// Refresh Browser Sources Hotkey
	refreshBrowserSourcesHotkey = obs_hotkey_register_frontend(
		"refresh_browser_sources", obs_module_text("RefreshBrowserSources"), HotkeyRefreshBrowserSources, nullptr);

	// Refresh Audio Monitoring Hotkey
	refreshAudioMonitoringHotkey = obs_hotkey_register_frontend(
		"refresh_audio_monitoring", obs_module_text("RefreshAudioMonitoring"), HotkeyRefreshAudioMonitoring, nullptr);

	// Lock All Sources Hotkey
	lockAllSourcesHotkey = obs_hotkey_register_frontend("toggle_lock_all_sources", obs_module_text("LockAllSources"),
							    HotkeyLockAllSources, nullptr);

	// Lock Curent Scenes Sources Hotkey
	lockCurrentSourcesHotkey = obs_hotkey_register_frontend(
		"toggle_lock_current_sources", obs_module_text("LockAllCurrentSources"), HotkeyLockCurrentSources, nullptr);

	// Open Source Properties Hotkey
	openSourcePropertiesHotkey = obs_hotkey_register_frontend("open_source_properties", obs_module_text("OpenSourceProperties"),
								  HotkeyOpenSourceProperties, nullptr);

	// Open Source Filter Hotkey
	openSourceFiltersHotkey = obs_hotkey_register_frontend("open_source_filters", obs_module_text("OpenSourceFilters"),
							       HotkeyOpenSourceFilters, nullptr);

	// Open Source Interact Hotkey
	openSourceInteractHotkey = obs_hotkey_register_frontend("open_source_interact", obs_module_text("OpenSourceInteract"),
								HotkeyOpenSourceInteract, nullptr);

	// Open Scenes Filter Hotkey
	openSceneFiltersHotkey = obs_hotkey_register_frontend("open_scene_filters", obs_module_text("OpenSceneFilters"),
							      HotkeyOpenSceneFilters, nullptr);
}

static void LoadStreamUPDock()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_push_ui_translation(obs_module_get_string);

	auto *dock_widget = new StreamUPDock(main_window);

	const QString title = QString::fromUtf8(obs_module_text("StreamUP Dock"));
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

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] loaded version %s", PROJECT_VERSION);

	InitialiseMenu();

	RegisterWebsocketRequests();
	RegisterHotkeys();

	obs_frontend_add_save_callback(SaveLoadHotkeys, nullptr);

	LoadStreamUPDock();

	return true;
}

void obs_module_post_load(void)
{
	InitialiseRequiredModules();

	// Load settings
	obs_data_t *settings = SaveLoadSettingsCallback(nullptr, false);

	if (settings) {
		bool runAtStartup = obs_data_get_bool(settings, "run_at_startup");
		if (runAtStartup) {
			CheckAllPluginsForUpdates(false);
		}

		notificationsMuted = obs_data_get_bool(settings, "notifications_mute");
		blog(LOG_INFO, "[StreamUP] Notifications mute setting: %s", notificationsMuted ? "true" : "false");

	} else {
		blog(LOG_WARNING, "[StreamUP] Failed to load settings in post load.");
	}

	obs_data_release(settings);
}

//--------------------EXIT COMMANDS--------------------
void obs_module_unload()
{
	obs_frontend_remove_save_callback(SaveLoadHotkeys, nullptr);

	obs_hotkey_unregister(refreshBrowserSourcesHotkey);
	obs_hotkey_unregister(lockAllSourcesHotkey);
	obs_hotkey_unregister(refreshAudioMonitoringHotkey);
	obs_hotkey_unregister(lockCurrentSourcesHotkey);
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("StreamUP");
}
