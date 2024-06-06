#include "streamup.hpp"
#include "version.h"
#include "obs-websocket-api.h"
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <graphics/image-file.h>
#include <iostream>
#include <obs.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs-data.h>
#include <obs-encoder.h>
#include <pthread.h>
#include <regex>
#include <QApplication>
#include <QCoreApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QImage>
#include <QBuffer>
#include <QDir>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <sstream>
#include <util/platform.h>
#include <unordered_set>
#include <cctype>

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

//--------------------INSTALL A PRODUCT--------------------
void ResizeMoveSetting(obs_data_t *obs_data, float factor)
{
	double x = obs_data_get_double(obs_data, "x");
	obs_data_set_double(obs_data, "x", x * factor);
	double y = obs_data_get_double(obs_data, "y");
	obs_data_set_double(obs_data, "y", y * factor);
	obs_data_release(obs_data);
}

void ResizeMoveFilters(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);
	float factor = *((float *)param);
	if (strcmp(obs_source_get_unversioned_id(child),
		   "move_source_filter") != 0)
		return;
	obs_data_t *settings = obs_source_get_settings(child);
	ResizeMoveSetting(obs_data_get_obj(settings, "pos"), factor);
	ResizeMoveSetting(obs_data_get_obj(settings, "bounds"), factor);
	const char *source_name = obs_data_get_string(settings, "source");
	obs_source_t *source = (source_name && strlen(source_name))
				       ? obs_get_source_by_name(source_name)
				       : nullptr;
	if (!obs_scene_from_source(source) && !obs_group_from_source(source)) {
		ResizeMoveSetting(obs_data_get_obj(settings, "scale"), factor);
	}
	obs_source_release(source);
	obs_data_set_string(settings, "transform_text", "");
	obs_data_release(settings);
	return;
}

void ResizeSceneItems(obs_data_t *settings, float factor)
{
	obs_data_array_t *items = obs_data_get_array(settings, "items");
	size_t count = obs_data_array_count(items);

	//Set a groups custom size
	if (obs_data_get_bool(settings, "custom_size")) {
		obs_data_set_int(settings, "cx",
				 obs_data_get_int(settings, "cx") * factor);
		obs_data_set_int(settings, "cy",
				 obs_data_get_int(settings, "cy") * factor);
	}
	//Set all sources pos, bounds and scale
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item_data = obs_data_array_item(items, i);
		vec2 vec2;
		obs_data_get_vec2(item_data, "pos", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "pos", &vec2);
		obs_data_get_vec2(item_data, "bounds", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "bounds", &vec2);
		const char *name = obs_data_get_string(item_data, "name");
		obs_source_t *item_source = obs_get_source_by_name(name);
		obs_source_release(item_source);
		if (item_source && (obs_scene_from_source(item_source) ||
				    obs_group_from_source(item_source))) {
			obs_data_release(item_data);
			continue;
		}
		obs_data_get_vec2(item_data, "scale", &vec2);
		vec2.x *= factor;
		vec2.y *= factor;
		obs_data_set_vec2(item_data, "scale", &vec2);
		obs_data_release(item_data);
	}
	obs_data_array_release(items);
}

void ConvertSettingPath(obs_data_t *settings, const char *setting_name,
			const QString &path, const char *sub_folder)
{
	const char *file = obs_data_get_string(settings, setting_name);
	if (!file || !strlen(file))
		return;
	if (QFile::exists(file))
		return;
	const QString file_name = QFileInfo(QT_UTF8(file)).fileName();
	QString filePath = path + "/Resources/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name,
				    QT_TO_UTF8(filePath));
		return;
	}
	filePath = path + "/" + sub_folder + "/" + file_name;
	if (QFile::exists(filePath)) {
		obs_data_set_string(settings, setting_name,
				    QT_TO_UTF8(filePath));
	}
}

void ConvertFilterPaths(obs_data_t *filter_data, const QString &path)
{
	const char *id = obs_data_get_string(filter_data, "id");
	if (strcmp(id, "shader_filter") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "shader_file_name", path,
				   "Shader Filters");
		obs_data_release(settings);
	} else if (strcmp(id, "mask_filter") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(filter_data, "settings");
		ConvertSettingPath(settings, "image_path", path, "Image Masks");
		obs_data_release(settings);
	}
}

void ConvertSourcePaths(obs_data_t *source_data, const QString &path)
{
	const char *id = obs_data_get_string(source_data, "id");
	if (strcmp(id, "image_source") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "file", path, "Image Sources");
		obs_data_release(settings);
	} else if (strcmp(id, "ffmpeg_source") == 0) {
		obs_data_t *settings =
			obs_data_get_obj(source_data, "settings");
		ConvertSettingPath(settings, "local_file", path,
				   "Media Sources");
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
	obs_data_array_t *items_orig =
		obs_data_get_array(scene_settings_orig, "items");
	const size_t item_count_orig = obs_data_array_count(items_orig);
	for (size_t j = 0; j < item_count_orig; j++) {
		obs_data_t *item_data_orig = obs_data_array_item(items_orig, j);
		const char *name_orig =
			obs_data_get_string(item_data_orig, "name");
		bool found = false;
		for (size_t k = 0; k < item_count; k++) {
			obs_data_t *item_data = obs_data_array_item(items, k);
			const char *name =
				obs_data_get_string(item_data, "name");
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
		const char *filter_name =
			obs_data_get_string(filter_data, "name");
		obs_source_t *filter =
			obs_source_get_filter_by_name(s, filter_name);
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
			obs_data_array_t *filters =
				obs_data_get_array(sourceData, "filters");
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
			obs_data_t *scene_settings =
				obs_data_get_obj(sourceData, "settings");
			if (w != 1920) {
				ResizeSceneItems(scene_settings, factor);
				if (new_source)
					obs_source_enum_filters(
						s, ResizeMoveFilters, &factor);
			}
			if (!new_source) {
				obs_scene_enum_items(
					scene,
					[](obs_scene_t *, obs_sceneitem_t *item,
					   void *d) {
						std::list<obs_source_t
								  *> *sources =
							(std::list<obs_source_t *>
								 *)d;
						obs_source_t *si =
							obs_sceneitem_get_source(
								item);
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

//--------------------HELPERS--------------------
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

QLabel *CreateRichTextLabel(const QString &text, bool bold, bool wrap,
			    Qt::Alignment alignment = Qt::Alignment())
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
	icon->setPixmap(QApplication::style()->standardIcon(iconName).pixmap(
		pixmapSize, pixmapSize));
	icon->setStyleSheet("padding-top: 3px;");
	return icon;
}

QHBoxLayout *AddIconAndText(const QStyle::StandardPixmap &iconText,
			    const char *labelText)
{
	QLabel *icon = CreateIconLabel(iconText);
	QLabel *text = CreateRichTextLabel(obs_module_text(labelText), false,
					   true, Qt::AlignTop);

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

void CreateLabelWithLink(QLayout *layout, const QString &text,
			 const QString &url, int row, int column)
{
	QLabel *label =
		CreateRichTextLabel(text, false, false, Qt::AlignCenter);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);

	QObject::connect(label, &QLabel::linkActivated,
			 [url]() { QDesktopServices::openUrl(QUrl(url)); });

	static_cast<QGridLayout *>(layout)->addWidget(label, row, column);
}

void CreateButton(QLayout *layout, const QString &text,
		  const std::function<void()> &onClick)
{
	QPushButton *button = new QPushButton(text);
	QObject::connect(button, &QPushButton::clicked, onClick);
	layout->addWidget(button);
}

void CreateRefreshDialog(const char *infoText1, const char *infoText2,
			 const char *infoText3, const QString &titleText,
			 const std::function<void()> &buttonCallback,
			 const QString &jsonString, const char *how1,
			 const char *how2, const char *how3)
{
	ShowDialogOnUIThread([infoText1, infoText2, infoText3, titleText,
			      buttonCallback, jsonString, how1, how2, how3]() {
		const char *titleTextChar = titleText.toUtf8().constData();
		QString titleStr = obs_module_text(titleTextChar);
		QString infoText1Str = obs_module_text(infoText1);
		QString infoText2Str = obs_module_text(infoText2);
		QString infoText3Str = obs_module_text(infoText3);
		QString howTo1Str = obs_module_text(how1);
		QString howTo2Str = obs_module_text(how2);
		QString howTo3Str = obs_module_text(how3);

		// Create Window
		QDialog *dialog = CreateDialogWindow(titleTextChar);
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		// Create buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();

		CreateButton(buttonLayout, obs_module_text("Cancel"),
			     [dialog]() { dialog->close(); });

		CreateButton(buttonLayout, titleStr, [=]() {
			buttonCallback();
			dialog->close();
		});

		// Create Icon + Text then add to dialog layout
		dialogLayout->addLayout(AddIconAndText(
			QStyle::SP_MessageBoxInformation, infoText1));
		dialogLayout->addSpacing(10);

		// Info 2
		QLabel *info2 = CreateRichTextLabel(infoText2Str, false, true,
						    Qt::AlignTop);
		dialogLayout->addWidget(info2, 0, Qt::AlignTop);
		dialogLayout->addSpacing(10);

		// Create a group box to contain info3 and the "Copy JSON" button
		QGroupBox *info3Box =
			new QGroupBox(obs_module_text("HowToUse"));
		info3Box->setMinimumWidth(350);
		QVBoxLayout *info3BoxLayout = CreateVBoxLayout(info3Box);
		QLabel *info3 = CreateRichTextLabel(infoText3Str, false, true);
		QLabel *howTo1 = CreateRichTextLabel(howTo1Str, false, true);
		QLabel *howTo2 = CreateRichTextLabel(howTo2Str, false, true);
		QLabel *howTo3 = CreateRichTextLabel(howTo3Str, false, true);
		info3BoxLayout->addWidget(info3);
		info3BoxLayout->addSpacing(5);
		info3BoxLayout->addWidget(howTo1);
		info3BoxLayout->addWidget(howTo2);
		info3BoxLayout->addWidget(howTo3);

		// Create a button to copy the JSON to clipboard
		QPushButton *copyJsonButton =
			new QPushButton(obs_module_text("CopyWebsocketJson"));
		copyJsonButton->setToolTip(
			obs_module_text("CopyWebsocketJsonTooltip"));
		QObject::connect(copyJsonButton, &QPushButton::clicked,
				 [=]() { CopyToClipboard(jsonString); });

		info3BoxLayout->addWidget(copyJsonButton);
		dialogLayout->addWidget(info3Box);

		dialogLayout->addSpacing(10);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}


//--------------------CHECK FOR PLUGIN UPDATES ETC--------------------
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

std::map<std::string, PluginInfo> all_plugins;
std::map<std::string, PluginInfo> required_plugins;

struct request_data {
	std::string url;
	std::string response;
};

void ErrorDialog(const QString &errorMessage)
{
	ShowDialogOnUIThread([errorMessage]() {
		// Create dialog window
		QDialog *dialog = CreateDialogWindow("WindowErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		// Convert error message to QString
		QString displayMessage = errorMessage.isEmpty()
						 ? "Unknown error occurred."
						 : errorMessage;

		// Create Icon + Text then add to dialog layout
		dialogLayout->addLayout(
			AddIconAndText(QStyle::SP_MessageBoxCritical,
				       displayMessage.toUtf8().constData()));

		// Create buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, "OK",
			     [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}


size_t WriteCallback(void *contents, size_t size, size_t nmemb,
		     std::string *out)
{
	size_t totalSize = size * nmemb;
	out->append((char *)contents, totalSize);
	return totalSize;
}

void *MakeApiRequest(void *arg)
{
	request_data *data = (request_data *)arg;
	CURL *curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, data->url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data->response);
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			blog(LOG_INFO,
			     "[StreamUP] curl_easy_perform() failed: %s\n",
			     curl_easy_strerror(res));
		curl_easy_cleanup(curl);
	}
	return NULL;
}

void InitialiseRequiredModules()
{
	pthread_t thread;
	request_data req_data;
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
		blog(LOG_INFO, "[StreamUP] Error loading plugins from %s",
		     req_data.url.c_str());
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
			info.windowsURL =
				obs_data_get_string(downloads, "windows");
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

char *GetFilePath()
{
	char *path = nullptr;
	char *path_abs = nullptr;

	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		path = obs_module_config_path("../../logs/");
		path_abs = os_get_abs_path_ptr(path);

		if (path_abs[strlen(path_abs) - 1] != '/' &&
		    path_abs[strlen(path_abs) - 1] != '\\') {
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
			blog(LOG_INFO,
			     "[StreamUP] OBS doesn't have files in the install directory.");
			bfree(path_abs);
			return NULL;
		} else {
			// The directory contains files
			return path_abs;
		}
	} else {
		// The directory does not exist
		blog(LOG_INFO,
		     "[StreamUP] OBS log file folder does not exist in the install directory.");
		bfree(path_abs);
		return NULL;
	}
}

std::string get_most_recent_file(std::string dirpath)
{
	auto it = std::filesystem::directory_iterator(dirpath);
	auto last_write_time = decltype(it->last_write_time())::min();
	std::filesystem::directory_entry newest_file;

	for (const auto &entry : it) {
		if (entry.is_regular_file() &&
		    entry.path().extension() == ".txt") {
			auto current_write_time = entry.last_write_time();
			if (current_write_time > last_write_time) {
				last_write_time = current_write_time;
				newest_file = entry;
			}
		}
	}

	return newest_file.path().string();
}

std::string search_string_in_file(const char *path, const char *search)
{
	std::string filepath = get_most_recent_file(path);
	FILE *file = fopen(filepath.c_str(), "r+");
	char line[256];
	std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	std::regex version_regex_double("[0-9]+\\.[0-9]+");

	if (file) {
		while (fgets(line, sizeof(line), file)) {
			char *found_ptr = strstr(line, search);
			if (found_ptr) {
				// Process the rest of the line from the found_ptr
				std::string remaining_line =
					std::string(found_ptr + strlen(search));
				std::smatch match;

				// Try matching with x.x.x format
				if (std::regex_search(remaining_line, match,
						      version_regex_triple) &&
				    match.size() > 0) {
					fclose(file);
					return match.str(0);
				}

				// If x.x.x format not found, try x.x format
				if (std::regex_search(remaining_line, match,
						      version_regex_double) &&
				    match.size() > 0) {
					fclose(file);
					return match.str(0);
				}
			}
		}
		fclose(file);
	} else {
		blog(LOG_ERROR, "Failed to open file: %s", filepath.c_str());
	}

	return "";
}

void PluginsUpToDateOutput(bool manuallyTriggered)
{
	if (manuallyTriggered) {
		ShowDialogOnUIThread([]() {
			// Create window
			QDialog *dialog =
				CreateDialogWindow("WindowUpToDateTitle");
			QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
			dialogLayout->setContentsMargins(20, 15, 20, 10);

			// Create Icon + Text then add to dialog layout
			dialogLayout->addLayout(
				AddIconAndText(QStyle::SP_DialogApplyButton,
					       "WindowUpToDateMessage"));

			// Create buttons
			QHBoxLayout *buttonLayout = new QHBoxLayout();
			CreateButton(buttonLayout, obs_module_text("OK"),
				     [dialog]() { dialog->close(); });

			dialogLayout->addLayout(buttonLayout);
			dialog->setLayout(dialogLayout);
			dialog->show();
		});
	}
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate)
{
	ShowDialogOnUIThread([errorMsgMissing, errorMsgUpdate]() {
		// Create window
		QDialog *dialog = CreateDialogWindow("WindowPluginErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 20);

		// Create Icon + Text for intro then add to dialog layout
		const char *errorText = (errorMsgMissing != "NULL")
						? "WindowPluginErrorMissing"
						: "WindowPluginErrorUpdating";
		dialogLayout->addLayout(AddIconAndText(
			QStyle::SP_MessageBoxWarning, errorText));

		dialogLayout->addSpacing(10);

		QLabel *pluginErrorInfo = CreateRichTextLabel(
			obs_module_text("WindowPluginErrorInfo"), false, true);
		dialogLayout->addWidget(pluginErrorInfo);
		dialogLayout->addSpacing(10);

		// Plugins need updating
		if (!errorMsgUpdate.empty()) {
			// Create list of plugins
			QLabel *pluginsToUpdateList = CreateRichTextLabel(
				QString::fromStdString(errorMsgUpdate), false,
				false, Qt::AlignCenter);

			// Add plugins to a box
			QGroupBox *pluginsToUpdateBox =
				new QGroupBox(obs_module_text(
					"WindowPluginErrorUpdateGroup"));
			QVBoxLayout *pluginsToUpdateBoxLayout =
				new QVBoxLayout(pluginsToUpdateBox);
			pluginsToUpdateBoxLayout->addWidget(
				pluginsToUpdateList);

			dialogLayout->addWidget(pluginsToUpdateBox);
			if (errorMsgMissing != "NULL") {
				dialogLayout->addSpacing(10);
			}
		}

		// Plugins are missing
		if (errorMsgMissing != "NULL") {
			// Create list of plugins
			QLabel *pluginsMissingList = CreateRichTextLabel(
				QString::fromStdString(errorMsgMissing), false,
				false, Qt::AlignCenter);

			// Add plugins to a box
			QGroupBox *pluginsMissingBox =
				new QGroupBox(obs_module_text(
					"WindowPluginErrorMissingGroup"));
			QVBoxLayout *pluginsMissingBoxLayout =
				new QVBoxLayout(pluginsMissingBox);
			pluginsMissingBoxLayout->addWidget(pluginsMissingList);
			dialogLayout->addWidget(pluginsMissingBox);
		}

		if (errorMsgMissing != "NULL") {
			QLabel *pluginstallerLabel = CreateRichTextLabel(
				obs_module_text("WindowPluginErrorFooter"),
				false, false, Qt::AlignCenter);
			dialogLayout->addWidget(pluginstallerLabel);
		}

		// Create buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();

		CreateButton(buttonLayout, obs_module_text("OK"),
			     [dialog]() { dialog->close(); });

		if (errorMsgMissing != "NULL") {
			QPushButton *pluginstallerButton = new QPushButton(
				obs_module_text("MenuDownloadPluginstaller"));
			QObject::connect(
				pluginstallerButton, &QPushButton::clicked,
				[]() {
					QDesktopServices::openUrl(QUrl(
						"https://streamup.tips/product/plugin-installer"));
				});
			buttonLayout->addWidget(pluginstallerButton);
		}

		dialogLayout->addLayout(buttonLayout);

		dialog->setLayout(dialogLayout);
		dialog->show();
	});
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
		// Default or error case
		url = plugin_info.windowsURL;
	}
	return url;
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

		std::string installed_version =
			search_string_in_file(filepath, search_string.c_str());

		if (!installed_version.empty()) {
			installedPlugins.emplace_back(plugin_name,
						      installed_version);
		}
	}

	bfree(filepath);

	return installedPlugins;
}

std::string RemoveDots(const std::string &version)
{
	std::string result;
	for (char c : version) {
		if (c != '.') {
			result += c;
		}
	}
	return result;
}

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

	// Normalize the length of version parts
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
		// Error in parsing version parts
		return false;
	}
}

void CheckAllPluginsForUpdates(bool manuallyTriggered)
{
	if (all_plugins.empty()) {
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return;
	}

	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgUpdate = "";
	std::vector<std::pair<std::string, std::string>> installedPlugins =
		GetInstalledPlugins();

	for (const auto &plugin : installedPlugins) {
		const std::string &plugin_name = plugin.first;
		const std::string &installed_version = plugin.second;
		const PluginInfo &plugin_info = all_plugins[plugin_name];
		const std::string &required_version = plugin_info.version;

		if (installed_version != required_version &&
		    IsVersionLessThan(installed_version, required_version)) {
			version_mismatch_modules.emplace(plugin_name,
							 installed_version);
		}
	}

	// Determine the path to the local app data folder
	std::string appDataPath =
		GetLocalAppDataPath(); // Implement this function based on your platform
	std::string filePath =
		appDataPath + "/StreamUP-OutdatedPluginsList.txt";

	// Open the file in write mode
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
		errorMsgUpdate +=
			"<div style=\"text-align: center;\">"
			"<table style=\"border-collapse: collapse; width: 100%;\">"
			"<tr style=\"background-color: #212121;\">"
			"<th style=\"padding: 4px 10px; text-align: center;\">Plugin Name</th>"
			"<th style=\"padding: 4px 10px; text-align: center;\">Installed</th>"
			"<th style=\"padding: 4px 10px; text-align: center;\">Current</th>"
			"<th style=\"padding: 4px 10px; text-align: center;\">Direct Download</th>"
			"</tr>";

		for (const auto &module : version_mismatch_modules) {
			const PluginInfo &plugin_info =
				all_plugins[module.first];
			const std::string &required_version =
				plugin_info.version;
			const std::string &forum_link = plugin_info.generalURL;
			const std::string &direct_download_link =
				GetPlatformURL(plugin_info);

			errorMsgUpdate +=
				"<tr style=\"border: 1px solid #ddd;\">"
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

		// Check if the search_string contains "[ignore]"
		if (search_string.find("[ignore]") != std::string::npos) {
			continue; // Skip to the next iteration
		}

		std::string installed_version =
			search_string_in_file(filepath, search_string.c_str());

		// Only add to missing_modules if plugin is required and not installed
		if (installed_version.empty() && plugin_info.required) {
			missing_modules.emplace(plugin_name, required_version);
		}
		// Only add to version_mismatch_modules if plugin is required and there's a version mismatch
		else if (!installed_version.empty() &&
			 installed_version != required_version &&
			 plugin_info.required &&
			 IsVersionLessThan(installed_version,
					   required_version)) {
			version_mismatch_modules.emplace(plugin_name,
							 installed_version);
		}
	}

	bfree(filepath);

	bool hasUpdates = !version_mismatch_modules.empty();
	bool hasMissingPlugins = !missing_modules.empty();

	if (hasUpdates || hasMissingPlugins) {
		if (hasUpdates) {
			errorMsgUpdate +=
				"<div style=\"text-align: center;\">"
				"<table style=\"border-collapse: collapse; width: 100%;\">"
				"<tr style=\"background-color: #212121;\">"
				"<th style=\"padding: 4px 10px; text-align: center;\">Plugin Name</th>"
				"<th style=\"padding: 4px 10px; text-align: center;\">Installed</th>"
				"<th style=\"padding: 4px 10px; text-align: center;\">Current</th>"
				"<th style=\"padding: 4px 10px; text-align: center;\">Direct Download</th>"
				"</tr>";

			for (const auto &module : version_mismatch_modules) {
				const PluginInfo &plugin_info =
					all_plugins[module.first];
				const std::string &required_version =
					plugin_info.version;
				const std::string &forum_link =
					plugin_info.generalURL;
				const std::string &direct_download_link =
					GetPlatformURL(plugin_info);

				errorMsgUpdate +=
					"<tr style=\"border: 1px solid #ddd;\">"
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

			// Remove the last <br> tag from errorMsgUpdate
			if (!errorMsgUpdate.empty() &&
			    errorMsgUpdate.substr(errorMsgUpdate.length() -
						  4) == "<br>") {
				errorMsgUpdate = errorMsgUpdate.substr(
					0,
					errorMsgUpdate.length() -
						4); // Remove the last 4 characters ("<br>")
			}
		}

		if (hasMissingPlugins) {
			errorMsgMissing +=
				"<div style=\"text-align: center;\">"
				"<table style=\"border-collapse: collapse; width: 100%;\">"
				"<tr style=\"background-color: #212121;\">"
				"<th style=\"padding: 4px 10px; text-align: center;\">Plugin Name</th>"
				"<th style=\"padding: 4px 10px; text-align: center;\">Direct Download</th>"
				"</tr>";

			for (auto it = missing_modules.begin();
			     it != missing_modules.end(); ++it) {
				const std::string &moduleName = it->first;
				const PluginInfo &pluginInfo =
					required_plugins[moduleName];
				const std::string &forum_link =
					pluginInfo.generalURL;
				const std::string &direct_download_link =
					GetPlatformURL(pluginInfo);

				errorMsgMissing +=
					"<tr style=\"border: 1px solid #ddd;\">"
					"<td style=\"padding: 2px 10px; text-align: left;\"><a href=\"" +
					forum_link + "\">" + moduleName +
					"</a></td>"
					"<td style=\"padding: 2px 10px; text-align: center;\"><a href=\"" +
					direct_download_link +
					"\">Download</a></td>"
					"</tr>";
			}

			errorMsgMissing += "</table></div>";

			// Remove the last <br> tag from errorMsgMissing
			if (!errorMsgMissing.empty() &&
			    errorMsgMissing.substr(errorMsgMissing.length() -
						   4) == "<br>") {
				errorMsgMissing = errorMsgMissing.substr(
					0,
					errorMsgMissing.length() -
						4); // Remove the last 4 characters ("<br>")
			}
		}

		PluginsHaveIssue(hasMissingPlugins ? errorMsgMissing : "NULL",
				 errorMsgUpdate);

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

//-------------------TOOLS-------------------
const char *monitoringTypeToString(obs_monitoring_type type)
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

bool EnumSourcesAudioMonitoring(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	// Get the source name
	const char *source_name = obs_source_get_name(source);

	obs_monitoring_type original_monitoring_type =
		obs_source_get_monitoring_type(source);

	if (original_monitoring_type != OBS_MONITORING_TYPE_NONE) {
		// Set the audio monitoring type to "OBS_MONITORING_TYPE_NONE".
		obs_source_set_monitoring_type(source,
					       OBS_MONITORING_TYPE_NONE);

		// Set the audio monitoring type back to the original monitoring type.
		obs_source_set_monitoring_type(source,
					       original_monitoring_type);

		blog(LOG_INFO,
		     "[StreamUP] '%s' has refreshed audio monitoring type: '%s'",
		     source_name,
		     monitoringTypeToString(original_monitoring_type));
	}

	return true;
}

bool EnumSourcesBrowser(void *data, obs_source_t *source)
{
	UNUSED_PARAMETER(data);

	//Get source ID
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
		blog(LOG_INFO, "[StreamUP] Refreshed '%s' browser source",
		     source_name);

		obs_data_release(settings);
	}

	return true;
}

void RefreshAudioMonitoringTypes()
{
	CreateRefreshDialog(
		"RefreshAudioMonitoringInfo1", "RefreshAudioMonitoringInfo2",
		"RefreshAudioMonitoringInfo3", "RefreshAudioMonitoring",
		[]() { obs_enum_sources(EnumSourcesAudioMonitoring, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_audio_monitoring",
                            "requestData": null
                        }
                    })",
		"RefreshAudioMonitoringHowTo1", "RefreshAudioMonitoringHowTo2",
		"RefreshAudioMonitoringHowTo3");
}

void RefreshBrowserSources()
{
	CreateRefreshDialog(
		"RefreshBrowserSourcesInfo1", "RefreshBrowserSourcesInfo2",
		"RefreshBrowserSourcesInfo3", "RefreshBrowserSources",
		[]() { obs_enum_sources(EnumSourcesBrowser, nullptr); },
		R"(
                    {
                        "requestType": "CallVendorRequest",
                        "requestData": {
                            "vendorName": "streamup",
                            "requestType": "refresh_browser_sources",
                            "requestData": null
                        }
                    })",
		"RefreshBrowserSourcesHowTo1", "RefreshBrowserSourcesHowTo2",
		"RefreshBrowserSourcesHowTo3");
}

//--------------------MENU & ABOUT-------------------
void LoadStreamUpFile(bool forceLoad = false)
{
	if (!forceLoad) {
		bool arePluginsUpToDate = CheckrequiredOBSPlugins(true);

		if (!arePluginsUpToDate) {
			return;
		}
	}

	QString fileName = QFileDialog::getOpenFileName(
		nullptr, QT_UTF8(obs_module_text("Load")), QString(),
		"StreamUP File (*.streamup)");

	if (!fileName.isEmpty()) {
		obs_data_t *data =
			obs_data_create_from_json_file(QT_TO_UTF8(fileName));
		LoadScene(data, QFileInfo(fileName).absolutePath());
		obs_data_release(data);
	}
}

std::vector<std::string> search_modules_in_file(char *path)
{
	std::unordered_set<std::string> ignoreModules = {
		"obs-websocket",      "coreaudio-encoder", "decklink-captions",
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

	std::string filepath = get_most_recent_file(path);
	FILE *file = fopen(filepath.c_str(), "r");
	char line[256];
	bool in_section = false;
	std::vector<std::string> collected_modules;
	std::regex timestamp_regex(
		"^[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}:"); // Regex for timestamp

	if (file) {
		while (fgets(line, sizeof(line), file) != NULL) {
			std::string str_line(line);
			// Remove timestamp
			str_line = std::regex_replace(str_line, timestamp_regex,
						      "");
			// Remove leading and trailing whitespace
			str_line.erase(0,
				       str_line.find_first_not_of(" \t\r\n"));
			str_line.erase(str_line.find_last_not_of(" \t\r\n") +
				       1);

			if (str_line.find("Loaded Modules:") !=
			    std::string::npos) {
				in_section = true;
			} else if (str_line.find(
					   "---------------------------------") !=
				   std::string::npos) {
				in_section = false;
			}

			if (in_section && !str_line.empty() &&
			    str_line != "Loaded Modules:") {
				// Remove the file extension from the module name based on the platform
				size_t suffix_pos = std::string::npos;
				if (strcmp(PLATFORM_NAME, "windows") == 0) {
					suffix_pos = str_line.find(".dll");
				} else if (strcmp(PLATFORM_NAME, "linux") ==
					   0) {
					suffix_pos = str_line.find(".so");
				}

				if (suffix_pos != std::string::npos) {
					str_line =
						str_line.substr(0, suffix_pos);
				}

				// Check if the module name is in the ignoreModules set
				if (ignoreModules.find(str_line) ==
				    ignoreModules.end()) {
					// Check if the module name exists in the all_plugins map
					bool foundInApi = false;
					for (const auto &pair : all_plugins) {
						if (pair.second.moduleName ==
						    str_line) {
							foundInApi = true;
							break;
						}
					}
					if (!foundInApi) {
						// If not found in API and not ignored, add the module name to the collected_modules vector
						collected_modules.push_back(
							str_line);
					}
				}
			}
		}
		fclose(file);
	} else {
		blog(LOG_ERROR, "Failed to open log file: %s",
		     filepath.c_str());
	}

	// Custom comparison function for case-insensitive sorting
	auto case_insensitive_compare = [](const std::string &a,
					   const std::string &b) {
		return std::lexicographical_compare(
			a.begin(), a.end(), b.begin(), b.end(),
			[](char char1, char char2) {
				return std::tolower(char1) <
				       std::tolower(char2);
			});
	};

	// Sort the vector alphabetically (case-insensitively)
	std::sort(collected_modules.begin(), collected_modules.end(),
		  case_insensitive_compare);

	return collected_modules;
}

void SetLabelWithSortedModules(QLabel *label,
			       const std::vector<std::string> &moduleNames)
{
	QString text;
	if (moduleNames.empty()) {
		text = obs_module_text(
			"WindowSettingsUpdaterIncompatibleModules");
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

QString GetForumLink(const std::string &pluginName)
{
	const PluginInfo &pluginInfo = all_plugins[pluginName];
	return QString::fromStdString(pluginInfo.generalURL);
}

void ShowInstalledPluginsDialog()
{
	ShowDialogOnUIThread([]() {
		// Get the installed plugins
		std::vector<std::pair<std::string, std::string>>
			installedPlugins = GetInstalledPlugins();

		// Create the menu dialog
		QDialog *dialog =
			CreateDialogWindow("WindowSettingsInstalledPlugins");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		// Information on what the installed plugins is
		dialogLayout->addLayout(
			AddIconAndText(QStyle::SP_MessageBoxInformation,
				       "WindowSettingsInstalledPluginsInfo1"));
		dialogLayout->addSpacing(5);

		QLabel *info2 = CreateRichTextLabel(
			obs_module_text("WindowSettingsInstalledPluginsInfo2"),
			false, true, Qt::AlignTop);
		dialogLayout->addWidget(info2, 0, Qt::AlignTop);

		QLabel *info3 = CreateRichTextLabel(
			obs_module_text("WindowSettingsInstalledPluginsInfo3"),
			false, true, Qt::AlignTop);
		dialogLayout->addWidget(info3, 0, Qt::AlignTop);

		// Compatible plugins
		QString compatiblePluginsString;
		if (installedPlugins.empty()) {
			compatiblePluginsString = obs_module_text(
				"WindowSettingsInstalledPlugins");
		} else {
			compatiblePluginsString = "";
			QString tempText;

			for (const auto &plugin : installedPlugins) {
				const std::string &pluginName = plugin.first;
				const std::string &pluginVersion =
					plugin.second;
				const QString forumLink =
					GetForumLink(pluginName);

				// Create a clickable link with the plugin name
				tempText +=
					"<a href=\"" + forumLink + "\">" +
					QString::fromStdString(pluginName) +
					"</a> (" +
					QString::fromStdString(pluginVersion) +
					")<br>";
			}

			// Remove the last <br> tag if it exists
			if (tempText.endsWith("<br>")) {
				tempText.chop(4);
			}

			compatiblePluginsString = tempText;
		}

		QLabel *compatiblePluginsList = CreateRichTextLabel(
			compatiblePluginsString, false, false);
		QGroupBox *compatiblePluginsBox = new QGroupBox(
			obs_module_text("WindowSettingsUpdaterCompatible"));
		compatiblePluginsBox->setMinimumWidth(180);
		QVBoxLayout *compatiblePluginsBoxLayout =
			CreateVBoxLayout(compatiblePluginsBox);
		compatiblePluginsBoxLayout->addWidget(compatiblePluginsList);

		// Incompatible plugins
		QGroupBox *incompatiblePluginsBox = new QGroupBox(
			obs_module_text("WindowSettingsUpdaterIncompatible"));
		incompatiblePluginsBox->setMinimumWidth(180);
		QVBoxLayout *incompatiblePluginsBoxLayout =
			CreateVBoxLayout(incompatiblePluginsBox);
		QLabel *incompatiblePluginsList = new QLabel;
		// Set the text of incompatiblePluginsList with the sorted list of module names
		std::vector<std::string> moduleNames =
			search_modules_in_file(GetFilePath());
		SetLabelWithSortedModules(incompatiblePluginsList, moduleNames);
		incompatiblePluginsBoxLayout->addWidget(
			incompatiblePluginsList);

		// Combine plugin boxes
		QHBoxLayout *pluginBoxesLayout = new QHBoxLayout();
		pluginBoxesLayout->addWidget(compatiblePluginsBox);
		pluginBoxesLayout->addWidget(incompatiblePluginsBox);

		// Create a horizontal layout for the close button
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, obs_module_text("Close"),
			     [dialog]() { dialog->close(); });

		// Combine layouts
		pluginBoxesLayout->setAlignment(Qt::AlignHCenter);
		dialogLayout->addLayout(pluginBoxesLayout);
		dialogLayout->addLayout(buttonLayout);

		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

void ShowAboutDialog()
{
	ShowDialogOnUIThread([]() {
		// Get version
		std::string version = PROJECT_VERSION;

		// Create the menu dialog
		QDialog *dialog = CreateDialogWindow("WindowAboutTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		// Information
		QString informationRaw =
			"StreamUP OBS plugin (version " +
			QString::fromStdString(version) +
			")<br>by <b>Andi Stone</b> (<b>Andilippi</b>)";
		QLayout *textLayout =
			AddIconAndText(QStyle::SP_MessageBoxInformation,
				       informationRaw.toUtf8().constData());
		dialogLayout->addLayout(textLayout);
		dialogLayout->addSpacing(10);

		// Create Support plugin box
		QGroupBox *supportBox =
			new QGroupBox(obs_module_text("Support"));
		supportBox->setMaximumWidth(500);
		QVBoxLayout *supportBoxLayout = CreateVBoxLayout(supportBox);
		supportBoxLayout->addWidget(CreateRichTextLabel(
			obs_module_text("WindowAboutSupport"), false, true,
			Qt::AlignCenter));

		// Support->Patreon and Ko-fi links
		QGridLayout *supportLinksLayout = new QGridLayout;
		CreateLabelWithLink(
			supportLinksLayout,
			"<b><a href='https://patreon.com/andilippi'>Andi's Patreon</a><br><a href='https://ko-fi.com/andilippi'>Andi's Ko-Fi</a></b>",
			"https://patreon.com/andilippi", 0, 0);
		CreateLabelWithLink(
			supportLinksLayout,
			"<b><a href='https://patreon.com/streamup'>StreamUP's Patreon</a><br><a href='https://ko-fi.com/streamup'>StreamUP's Ko-Fi</a></b>",
			"https://patreon.com/streamup", 0, 1);
		// Set to main layout
		supportLinksLayout->setHorizontalSpacing(20);
		supportLinksLayout->setAlignment(Qt::AlignCenter);
		supportBoxLayout->addLayout(supportLinksLayout);
		dialogLayout->addWidget(supportBox);

		// Create Social Media box
		QGroupBox *socialBox = new QGroupBox(
			obs_module_text("WindowAboutSocialsTitle"));
		socialBox->setMaximumWidth(500);
		QVBoxLayout *socialBoxLayout = CreateVBoxLayout(socialBox);
		socialBoxLayout->addWidget(CreateRichTextLabel(
			obs_module_text("WindowAboutSocialsMsg"), false, true,
			Qt::AlignCenter));

		// Andi social links
		QGridLayout *socialLinksLayout = new QGridLayout;
		CreateLabelWithLink(
			socialLinksLayout,
			"<b><a href='https://andistonemedia.mystl.ink'>All Andi's Links</a></b>",
			"https://andistonemedia.mystl.ink", 0, 0);
		socialBoxLayout->addLayout(socialLinksLayout);

		// Set to main layout
		dialogLayout->addWidget(socialBox);

		dialogLayout->addSpacing(10);

		// Special thanks
		QLabel *thanksText = CreateRichTextLabel(
			obs_module_text("WindowAboutThanks"), false, true,
			Qt::AlignCenter);
		dialogLayout->addWidget(thanksText);

		// Buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();

		CreateButton(buttonLayout, obs_module_text("Donate"), []() {
			QDesktopServices::openUrl(
				QUrl("https://paypal.me/andilippi"));
		});
		CreateButton(buttonLayout, obs_module_text("Close"),
			     [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);

		//Final set
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

void SaveSettings(obs_data_t *settings)
{
	char *configPath = obs_module_config_path("configs.json");

	if (obs_data_save_json(settings, configPath)) {
		blog(LOG_INFO, "[StreamUP] Settings saved to %s", configPath);
	} else {
		blog(LOG_WARNING, "Failed to save settings to file.");
	}

	bfree(configPath);
}

obs_data_t *LoadSettings()
{
	char *file = obs_module_config_path("configs.json");
	char *path_abs = os_get_abs_path_ptr(file);

	obs_data_t *settings = obs_data_create_from_json_file(path_abs);

	if (!settings) {
		blog(LOG_INFO,
		     "[StreamUP] Settings not found. Creating default settings...");
		os_mkdirs(obs_module_config_path(""));
		// Create default settings
		settings = obs_data_create();
		obs_data_set_bool(settings, "run_at_startup", true);
		if (obs_data_save_json(settings, path_abs)) {
			blog(LOG_INFO, "[StreamUP] Settings saved to %s",
			     path_abs);
		} else {
			blog(LOG_WARNING, "Failed to save settings to file.");
		}

	} else {
		blog(LOG_INFO, "[StreamUP] Settings loaded successfully");
	}
	bfree(file);
	bfree(path_abs);
	return settings;
}

void ShowSettingsDialog()
{
	ShowDialogOnUIThread([]() {
		obs_data_t *settings = LoadSettings();

		// Create the menu dialog
		QDialog *dialog = CreateDialogWindow("WindowSettingsTitle");
		QFormLayout *dialogLayout = new QFormLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		// Create the title label for "General"
		QLabel *titleLabel =
			CreateRichTextLabel("General", true, false);

		// Add the title label to the form layout
		dialogLayout->addRow(titleLabel);

		// Create the boolean property
		obs_properties_t *props = obs_properties_create();
		obs_property_t *p = obs_properties_add_bool(
			props, "run_at_startup",
			obs_module_text("WindowSettingsRunOnStartup"));

		// Create the label and checkbox
		QLabel *label = CreateRichTextLabel(
			obs_module_text("WindowSettingsRunOnStartup"), false,
			false);
		QCheckBox *checkBox = new QCheckBox();
		checkBox->setChecked(
			obs_data_get_bool(settings, obs_property_name(p)));

		// Connect the checkbox stateChanged signal to update the property value
		QObject::connect(checkBox, &QCheckBox::stateChanged,
				 [=](int state) {
					 bool newValue = (state == Qt::Checked);
					 obs_data_set_bool(settings,
							   obs_property_name(p),
							   newValue);
				 });

		// Add the label and checkbox to the form layout
		dialogLayout->addRow(label, checkBox);

		// Add a small spacer above the separator
		dialogLayout->addItem(new QSpacerItem(0, 5));

		// Create the label for plugin management title
		QLabel *pluginLabel = CreateRichTextLabel(
			obs_module_text("WindowSettingsPluginManagement"), true,
			false);

		// Create the button for viewing installed plugins
		QPushButton *pluginButton = new QPushButton(
			obs_module_text("WindowSettingsViewInstalledPlugins"));

		// Connect the plugin button's clicked signal to show the installed plugins menu
		QObject::connect(pluginButton, &QPushButton::clicked,
				 ShowInstalledPluginsDialog);

		// Add the plugin label and button to the form layout
		dialogLayout->addRow(pluginLabel);
		dialogLayout->addRow(pluginButton);

		// Create Cancel / Save buttons in a horizontal layout
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, obs_module_text("Cancel"),
			     [dialog, settings]() {
				     obs_data_release(settings);
				     dialog->close();
			     });

		CreateButton(buttonLayout, obs_module_text("Save"), [=]() {
			SaveSettings(settings);
			dialog->close();
		});

		// Create a widget to hold the button layout
		QWidget *buttonWidget = new QWidget();
		buttonWidget->setLayout(buttonLayout);

		// Add the button widget as a single row to the form layout
		dialogLayout->addRow(buttonWidget);

		dialog->setLayout(dialogLayout);

		// Connect the dialog's finished signal to release the settings
		QObject::connect(dialog, &QDialog::finished, [=](int) {
			obs_data_release(settings),
				obs_properties_destroy(props);
		});

		dialog->show();
	});
}

static void LoadMenu(QMenu *menu)
{

	menu->clear();
	QAction *a;
	QMenu *toolsMenu = new QMenu(obs_module_text("MenuTools"), menu);

	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		a = menu->addAction(obs_module_text("MenuInstallProduct"));
		QObject::connect(a, &QAction::triggered, []() {
			// Check if the Shift key is held down
			Qt::KeyboardModifiers modifiers =
				QApplication::keyboardModifiers();
			bool shiftKeyPressed = modifiers & Qt::ShiftModifier;

			LoadStreamUpFile(shiftKeyPressed);
		});
		a = menu->addAction(obs_module_text("MenuDownloadProduct"));
		QObject::connect(a, &QAction::triggered, []() {
			QDesktopServices::openUrl(
				QUrl("https://streamup.tips/"));
		});

		a = menu->addAction(obs_module_text("MenuCheckRequirements"));
		QObject::connect(a, &QAction::triggered,
				 [] { CheckrequiredOBSPlugins(); });
		menu->addSeparator();
	}

	a = menu->addAction(obs_module_text("MenuCheckPluginUpdates"));
	QObject::connect(a, &QAction::triggered,
			 [] { CheckAllPluginsForUpdates(true); });

	a = toolsMenu->addAction(obs_module_text("MenuRefreshAudioMonitoring"));
	QObject::connect(a, &QAction::triggered,
			 [] { RefreshAudioMonitoringTypes(); });

	a = toolsMenu->addAction(obs_module_text("MenuRefreshBrowserSources"));
	QObject::connect(a, &QAction::triggered,
			 [] { RefreshBrowserSources(); });

	menu->addMenu(toolsMenu);

	menu->addSeparator();

	a = menu->addAction(obs_module_text("MenuAbout"));
	QObject::connect(a, &QAction::triggered, [] { ShowAboutDialog(); });

	a = menu->addAction(obs_module_text("MenuSettings"));
	QObject::connect(a, &QAction::triggered, [] { ShowSettingsDialog(); });
}

//--------------------GENERAL OBS--------------------
obs_websocket_vendor vendor = nullptr;

void vendor_request_bitrate(obs_data_t *request_data, obs_data_t *response_data,
			    void *private_data)
{
	UNUSED_PARAMETER(request_data);
	UNUSED_PARAMETER(private_data);

	// Get the streaming output
	obs_output_t *streamOutput = obs_frontend_get_streaming_output();

	// If there's no streaming output or streaming is inactive, we do nothing
	if (!streamOutput || !obs_frontend_streaming_active()) {
		obs_data_set_string(response_data, "error",
				    "Streaming is not active.");
		return;
	}

	// Get total bytes sent
	uint64_t bytesSent = obs_output_get_total_bytes(streamOutput);

	// Store current time in nanoseconds
	uint64_t currentTime = os_gettime_ns();

	// Static variables to hold the previous bytes sent and the previous timestamp
	static uint64_t lastBytesSent = 0;
	static uint64_t lastTime = 0;

	if (bytesSent < lastBytesSent)
		bytesSent = 0;

	// Calculate bytes sent since last check
	uint64_t bytesBetween = bytesSent - lastBytesSent;
	double timePassed = (currentTime - lastTime) /
			    1000000000.0; // Convert ns to seconds

	uint64_t bytesPerSec = 0;
	if (timePassed != 0)
		bytesPerSec = bytesBetween / timePassed;

	// Convert bytes/sec to kbits/sec
	uint64_t kbitsPerSec = (bytesPerSec * 8) / 1024;

	// Update last bytes and last time
	lastBytesSent = bytesSent;
	lastTime = currentTime;

	// Set the kbits/sec in the response data
	obs_data_set_int(response_data, "kbits-per-sec", kbitsPerSec);
}

void vendor_request_version(obs_data_t *request_data, obs_data_t *response_data,
			    void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_check_plugins(obs_data_t *request_data,
				  obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool pluginsUpToDate = CheckrequiredOBSPlugins(true);
	obs_data_set_bool(response_data, "success", pluginsUpToDate);
}

void vendor_request_refresh_audio_monitoring(obs_data_t *request_data,
					     obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_enum_sources(EnumSourcesAudioMonitoring, nullptr);
	obs_data_set_bool(response_data, "Audio monitoring refreshed", true);
}

void vendor_request_get_show_hide_transition(obs_data_t *request_data,
					     obs_data_t *response_data,
					     void *private_data,
					     bool transition_type)
{
	UNUSED_PARAMETER(private_data);

	const char *scene_name = obs_data_get_string(request_data, "sceneName");
	const char *source_name =
		obs_data_get_string(request_data, "sourceName");

	obs_source_t *scene_source = obs_get_source_by_name(scene_name);
	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found.");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	obs_sceneitem_t *scene_item = obs_scene_find_source(scene, source_name);
	if (!scene_item) {
		obs_data_set_string(response_data, "error",
				    "Source not found in scene.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	obs_source_t *transition =
		obs_sceneitem_get_transition(scene_item, transition_type);
	if (!transition) {
		obs_data_set_string(response_data, "error",
				    "No transition set for this item.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	obs_data_t *settings = obs_source_get_settings(transition);
	if (!settings) {
		blog(LOG_WARNING, "Failed to get settings for transition: %s",
		     obs_source_get_name(transition));
		obs_data_set_string(response_data, "error",
				    "Failed to get transition settings.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	uint32_t transition_duration = obs_sceneitem_get_transition_duration(
		scene_item, transition_type);

	obs_data_set_string(response_data, "transitionName",
			    obs_source_get_name(transition));
	obs_data_set_string(response_data, "transitionType",
			    obs_source_get_id(transition));
	obs_data_set_obj(response_data, "transitionSettings", settings);
	obs_data_set_int(response_data, "transitionDuration",
			 transition_duration);

	obs_data_set_bool(response_data, "success", true);

	obs_source_release(scene_source);
	obs_data_release(settings);
}

void vendor_request_get_show_transition(obs_data_t *request_data,
					obs_data_t *response_data,
					void *private_data)
{
	vendor_request_get_show_hide_transition(request_data, response_data,
						private_data, true);
}

void vendor_request_get_hide_transition(obs_data_t *request_data,
					obs_data_t *response_data,
					void *private_data)
{
	vendor_request_get_show_hide_transition(request_data, response_data,
						private_data, false);
}

void vendor_request_set_show_hide_transition(obs_data_t *request_data,
					     obs_data_t *response_data,
					     void *private_data,
					     bool show_transition)
{
	UNUSED_PARAMETER(private_data);

	const char *scene_name = obs_data_get_string(request_data, "sceneName");
	const char *source_name =
		obs_data_get_string(request_data, "sourceName");
	const char *transition_type =
		obs_data_get_string(request_data, "transitionType");
	obs_data_t *transition_settings =
		obs_data_get_obj(request_data, "transitionSettings");
	uint32_t transition_duration =
		obs_data_get_int(request_data, "transitionDuration");

	obs_source_t *scene_source = obs_get_source_by_name(scene_name);
	if (!scene_source) {
		obs_data_set_string(response_data, "error", "Scene not found.");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_scene_t *scene = obs_scene_from_source(scene_source);
	obs_sceneitem_t *scene_item = obs_scene_find_source(scene, source_name);
	if (!scene_item) {
		obs_data_set_string(response_data, "error",
				    "Source not found in scene.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	obs_source_t *transition = obs_source_create_private(
		transition_type, "Scene Transition", NULL);
	if (!transition) {
		obs_data_set_string(
			response_data, "error",
			"Unable to create transition of specified type.");
		obs_data_set_bool(response_data, "success", false);
		obs_source_release(scene_source);
		return;
	}

	if (transition_settings) {
		obs_source_update(transition, transition_settings);
	}

	obs_sceneitem_set_transition(scene_item, show_transition, transition);
	obs_sceneitem_set_transition_duration(scene_item, show_transition,
					      transition_duration);

	obs_data_set_bool(response_data, "success", true);

	obs_source_release(transition);
	obs_source_release(scene_source);
}

void vendor_request_set_show_transition(obs_data_t *request_data,
					obs_data_t *response_data,
					void *private_data)
{
	vendor_request_set_show_hide_transition(request_data, response_data,
						private_data, true);
}

void vendor_request_set_hide_transition(obs_data_t *request_data,
					obs_data_t *response_data,
					void *private_data)
{
	vendor_request_set_show_hide_transition(request_data, response_data,
						private_data, false);
}

static void hotkey_refresh_audio_monitoring(void *data, obs_hotkey_id id,
					    obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(EnumSourcesAudioMonitoring, nullptr);
}

void vendor_request_refresh_browser_sources(obs_data_t *request_data,
					    obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_enum_sources(EnumSourcesBrowser, nullptr);
	obs_data_set_bool(response_data, "Browser sources refreshed", true);
}

static void hotkey_refresh_browser_sources(void *data, obs_hotkey_id id,
					   obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(data);
	if (!pressed)
		return;
	obs_enum_sources(EnumSourcesBrowser, nullptr);
}

struct SceneItemEnumData {
	bool isAnySourceSelected = false;
	const char *selectedSourceName = nullptr;
};

static bool enum_scene_items_callback(obs_scene_t *scene, obs_sceneitem_t *item,
				      void *param)
{
	SceneItemEnumData *data = static_cast<SceneItemEnumData *>(param);
	bool isSelected = obs_sceneitem_selected(item);
	if (isSelected) {
		data->isAnySourceSelected = true;
		obs_source_t *source = obs_sceneitem_get_source(item);
		data->selectedSourceName = obs_source_get_name(source);
		blog(LOG_INFO, "[StreamUP] source = %s is selected",
		     data->selectedSourceName ? data->selectedSourceName
					      : "Unknown");
	}
	return true;
}

bool enum_scene_items(obs_scene_t *scene, const char **selected_source_name)
{
	SceneItemEnumData data;

	obs_scene_enum_items(scene, enum_scene_items_callback, &data);

	if (data.isAnySourceSelected) {
		*selected_source_name = data.selectedSourceName;
	}
	return data.isAnySourceSelected;
}

void vendor_request_get_current_selected_source(obs_data_t *request_data,
						obs_data_t *response_data,
						void *private_data)
{
	obs_source_t *current_scene = obs_frontend_get_current_scene();
	const char *scene_name = obs_source_get_name(current_scene);
	blog(LOG_INFO, "[StreamUP] current_scene = %s",
	     scene_name ? scene_name : "Unknown");

	if (!current_scene)
		return;

	obs_scene_t *scene = obs_scene_from_source(current_scene);
	if (!scene) {
		obs_source_release(current_scene);
		return;
	}

	const char *selected_source_name = nullptr;
	bool isAnySourceSelected =
		enum_scene_items(scene, &selected_source_name);

	obs_source_release(current_scene);

	// Decide the response
	if (!isAnySourceSelected) {
		blog(LOG_INFO,
		     "[StreamUP] No selected source. Current scene: %s",
		     scene_name);
		obs_data_set_string(response_data, "currentScene", scene_name);
	} else {
		blog(LOG_INFO, "[StreamUP] Selected source: %s",
		     selected_source_name);
		obs_data_set_string(response_data, "selectedSource",
				    selected_source_name);
	}
}

void vendor_request_get_output_file_path(obs_data_t *request_data,
					 obs_data_t *response_data,
					 void *private_data)
{
	char *path = obs_frontend_get_current_record_output_path();
	obs_data_set_string(response_data, "outputFilePath", path);
}

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] loaded version %s", PROJECT_VERSION);

	//Load menu
	QAction *action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("StreamUP")));
	QMenu *menu = new QMenu();
	action->setMenu(menu);
	QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenu(menu); });

	//Register OBS WebSocket vendor and requests
	vendor = obs_websocket_register_vendor("streamup");
	if (!vendor)
		return true;

	//OBSws -> Request Recording Output
	obs_websocket_vendor_register_request(
		vendor, "getOutputFilePath",
		vendor_request_get_output_file_path, nullptr);

	//OBSws -> Request Bitrate
	obs_websocket_vendor_register_request(
		vendor, "getCurrentSource",
		vendor_request_get_current_selected_source, nullptr);

	//OBSws -> Request Source Show Transition
	obs_websocket_vendor_register_request(
		vendor, "getShowTransition", vendor_request_get_show_transition,
		nullptr);
	//OBSws -> Request Source Hide Transition
	obs_websocket_vendor_register_request(
		vendor, "getHideTransition", vendor_request_get_hide_transition,
		nullptr);

	// OBSws -> Request Source Set Show Transition
	obs_websocket_vendor_register_request(
		vendor, "setShowTransition", vendor_request_set_show_transition,
		nullptr);

	// OBSws -> Request Source Set Hide Transition
	obs_websocket_vendor_register_request(
		vendor, "setHideTransition", vendor_request_set_hide_transition,
		nullptr);

	//OBSws -> Request Bitrate
	obs_websocket_vendor_register_request(vendor, "getBitrate",
					      vendor_request_bitrate, nullptr);
	//OBSws -> Request Version
	obs_websocket_vendor_register_request(vendor, "version",
					      vendor_request_version, nullptr);
	//OBSws -> Check Plugins
	obs_websocket_vendor_register_request(
		vendor, "check_plugins", vendor_request_check_plugins, nullptr);
	//OBSws & Hotkey -> Refresh Audio Monitoring
	obs_websocket_vendor_register_request(
		vendor, "refresh_audio_monitoring",
		vendor_request_refresh_audio_monitoring, nullptr);

	obs_hotkey_register_frontend("refresh_audio_monitoring",
				     obs_module_text("RefreshAudioMonitoring"),
				     hotkey_refresh_audio_monitoring, nullptr);
	//OBSws -> Refresh Browser Sources
	obs_websocket_vendor_register_request(
		vendor, "refresh_browser_sources",
		vendor_request_refresh_browser_sources, nullptr);

	obs_hotkey_register_frontend("refresh_browser_sources",
				     obs_module_text("RefreshBrowserSources"),
				     hotkey_refresh_browser_sources, nullptr);

	return true;
}

void obs_module_post_load(void)
{ 
	InitialiseRequiredModules();

	// Load run on startup settings
	obs_data_t *settings = LoadSettings();
	bool runAtStartup = obs_data_get_bool(settings, "run_at_startup");
	if (runAtStartup) {
		CheckAllPluginsForUpdates(false);
	}
	obs_data_release(settings);
}

void obs_module_unload() {}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("StreamUP");
}
