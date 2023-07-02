#include "streamup.hpp"
#include "version.h"

#include <curl/curl.h>
#include <filesystem>
#include <obs.h>
#include <obs-module.h>
#include <obs-data.h>
#include <pthread.h>
#include <regex>
#include <QApplication>
#include <QBuffer>
#include <QDesktopServices>
#include <QDialog>
#include <QFileDialog>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <util/platform.h>

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

struct PluginInfo {
	std::string name;
	std::string version;
	std::string searchString;
	std::string windowsURL;
	std::string macURL;
	std::string linuxURL;
	bool recommended;
};

std::map<std::string, PluginInfo> all_plugins;
std::map<std::string, PluginInfo> recommended_plugins;

struct request_data {
	std::string url;
	std::string response;
};

size_t WriteCallback(void *contents, size_t size, size_t nmemb,
		     std::string *out)
{
	size_t totalSize = size * nmemb;
	out->append((char *)contents, totalSize);
	return totalSize;
}

void *make_api_request(void *arg)
{
	request_data *data = (request_data *)arg;
	CURL *curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, data->url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data->response);
		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			blog(LOG_INFO, "curl_easy_perform() failed: %s\n",
			     curl_easy_strerror(res));

		curl_easy_cleanup(curl);
	}
	return NULL;
}

void initialize_required_modules()
{
	pthread_t thread;
	request_data req_data;
	req_data.url = "https://api.streamup.tips/plugins";

	if (pthread_create(&thread, NULL, make_api_request, &req_data)) {
		blog(LOG_INFO, "Error creating thread\n");
		return;
	}

	if (pthread_join(thread, NULL)) {
		blog(LOG_INFO, "Error joining thread\n");
		return;
	}

	std::string api_response = req_data.response;

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

		all_plugins[name] = info;

		if (obs_data_get_bool(plugin, "recommended")) {
			recommended_plugins[name] = info;
		}

		obs_data_release(plugin);
	}

	obs_data_array_release(plugins);
	obs_data_release(data);
}

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

static void merge_scenes(obs_source_t *s, obs_data_t *scene_settings)
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

static void merge_filters(obs_source_t *s, obs_data_array_t *filters)
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
	uint32_t w = obs_source_get_width(obs_frontend_get_current_scene());
	float factor = (float)w / 1920.0f;
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
				merge_filters(s, filters);
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
				merge_scenes(s, scene_settings);
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

void LoadStreamUpFile(void *private_data)
{
	UNUSED_PARAMETER(private_data);
	QString fileName = QFileDialog::getOpenFileName(
		nullptr, QT_UTF8(obs_module_text("Load")), QString(),
		"StreamUP File (*.streamup)");
	if (fileName.isEmpty())
		return;
	obs_data_t *data = obs_data_create_from_json_file(QT_TO_UTF8(fileName));
	LoadScene(data, QFileInfo(fileName).absolutePath());
	obs_data_release(data);
}

char *GetFilePath()
{
	char *relative_filepath =
		obs_module_get_config_path(obs_current_module(), NULL);
	char *filepath = os_get_abs_path_ptr(relative_filepath);
	bfree(relative_filepath);

	const char *search;
	const char *replace;

	if (std::string(PLATFORM_NAME) == "windows") {
		search = "\\plugin_config\\streamup\\";
		replace = "\\logs\\";
	} else if (std::string(PLATFORM_NAME) == "mac" ||
		   std::string(PLATFORM_NAME) == "linux") {
		search = "/plugin_config/streamup/";
		replace = "/logs/";
	}

	char *found_ptr = strstr(filepath, search);

	if (found_ptr) {
		// Calculate new string size and allocate memory for it
		size_t new_str_size =
			strlen(filepath) - strlen(search) + strlen(replace) + 1;
		char *new_str = (char *)bmalloc(new_str_size);

		// Copy the part of the string before the found substring
		size_t length_before = found_ptr - filepath;
		strncpy(new_str, filepath, length_before);

		// Copy the replacement string
		strcpy(new_str + length_before, replace);

		// Copy the part of the string after the found substring
		strcpy(new_str + length_before + strlen(replace),
		       found_ptr + strlen(search));

		bfree(filepath);

		std::string dirpath = new_str;
		if (std::filesystem::exists(dirpath)) {
			// The directory exists
			std::filesystem::directory_iterator dir(dirpath);
			if (dir == std::filesystem::directory_iterator{}) {
				// The directory is empty
				blog(LOG_INFO,
				     "OBS doesn't have files in the install directory.");
				return NULL;
			} else {
				// The directory contains files
			}
			return new_str;

		} else {
			// The directory does not exist
			blog(LOG_INFO,
			     "OBS log file folder does not exist in the install directory.");
			bfree(new_str);
			return NULL;
		}

	} else {
		blog(LOG_INFO, "This error shouldn't appear!");
		bfree(filepath);
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

std::string search_string_in_file(char *path, const char *search)
{
	std::string filepath = get_most_recent_file(path);
	FILE *file = fopen(filepath.c_str(), "r+");
	char line[256];
	bool found = false;
	std::regex version_regex("[0-9]+\\.[0-9]+\\.[0-9]+");

	if (file) {
		while (fgets(line, sizeof(line), file)) {
			char *found_ptr = strstr(line, search);
			if (found_ptr) {
				found = true;
				std::string line_str(line);
				std::smatch match;

				if (std::regex_search(line_str, match,
						      version_regex) &&
				    match.size() > 0) {
					fclose(file);
					return match.str(0);
				}
			}
		}
		fclose(file);
		if (!found) {
			//Plugin Not Installed
		}
	} else {
		blog(LOG_ERROR, "Failed to open log file: %s",
		     filepath.c_str());
	}
	return "";
}

void PluginsUpToDateOutput()
{
	QDialog successDialog;
	QVBoxLayout *successLayout = new QVBoxLayout;

	QLabel *iconLabel = new QLabel;
	iconLabel->setPixmap(
		QApplication::style()
			->standardIcon(QStyle::SP_DialogApplyButton)
			.pixmap(32, 32));

	QLabel *successLabel = new QLabel;
	successLabel->setText(
		"All OBS plugins are up to date and installed correctly.");
	successLabel->setTextFormat(Qt::RichText);
	successLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	successLabel->setOpenExternalLinks(true);

	QHBoxLayout *topLayout = new QHBoxLayout;
	topLayout->addStretch(1);
	topLayout->addWidget(iconLabel);
	topLayout->addWidget(successLabel, 1);
	topLayout->addStretch(1);

	successLayout->addLayout(topLayout);

	QPushButton *okButton = new QPushButton("OK");
	QObject::connect(okButton, &QPushButton::clicked, &successDialog,
			 &QDialog::accept);

	successLayout->addWidget(okButton);

	successDialog.setLayout(successLayout);
	successDialog.setWindowTitle("StreamUP • All Plugins Up-to-Date");
	successDialog.setFixedSize(successDialog.sizeHint());
	successDialog.exec();
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate)
{
	QDialog dialog;
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(20, 10, 20, 10);

	// First layout (icon and main message)
	QLabel *iconLabel = new QLabel;
	iconLabel->setPixmap(
		QApplication::style()
			->standardIcon(QStyle::SP_MessageBoxWarning)
			.pixmap(64, 64));
	iconLabel->setStyleSheet("padding-top: 3px;");

	QLabel *mainMessageLabel = new QLabel;
	if (errorMsgMissing != "NULL") {
		mainMessageLabel->setText(QString::fromStdString(
			"OBS is missing plugins or has older versions.<br>StreamUP products may not function correctly!<br>"));
	} else {
		mainMessageLabel->setText(QString::fromStdString(
			"Some OBS plugins need an update.<br>Please click each one to download them!<br>"));
	}
	mainMessageLabel->setTextFormat(Qt::RichText);
	mainMessageLabel->setOpenExternalLinks(true);

	QVBoxLayout *iconLayout = new QVBoxLayout;
	iconLayout->addWidget(iconLabel);
	iconLayout->addStretch();

	QVBoxLayout *textLayout = new QVBoxLayout;
	textLayout->addWidget(mainMessageLabel);
	textLayout->addStretch();

	QHBoxLayout *topLayout = new QHBoxLayout;
	topLayout->addStretch(1);
	topLayout->addLayout(iconLayout);
	topLayout->addLayout(textLayout, 1);
	topLayout->addStretch(1);

	// Third layout (detailed error report for plugins to update)
	QLabel *updateLabel = new QLabel;
	updateLabel->setText(QString::fromStdString(errorMsgUpdate));
	updateLabel->setTextFormat(Qt::RichText);
	updateLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	updateLabel->setOpenExternalLinks(true);

	// Create a QGroupBox for plugins to update
	QGroupBox *updateBox = new QGroupBox("Plugins to Update");
	QVBoxLayout *updateBoxLayout = new QVBoxLayout;
	updateBoxLayout->addWidget(updateLabel);
	updateBox->setLayout(updateBoxLayout);

	// Add layouts and widgets to the main layout
	mainLayout->addLayout(topLayout);
	// Second layout (detailed error report for missing plugins)
	QLabel *missingLabel = new QLabel;

	if (errorMsgMissing != "NULL") {
		missingLabel->setText(QString::fromStdString(errorMsgMissing));
		missingLabel->setTextFormat(Qt::RichText);
		missingLabel->setTextInteractionFlags(
			Qt::TextBrowserInteraction);
		missingLabel->setOpenExternalLinks(true);
		// Create a QGroupBox for missing plugins
		QGroupBox *missingBox = new QGroupBox("Missing Plugins");
		QVBoxLayout *missingBoxLayout = new QVBoxLayout;
		missingBoxLayout->addWidget(missingLabel);
		missingBox->setLayout(missingBoxLayout);
		mainLayout->addWidget(missingBox);
	}

	if (!errorMsgUpdate.empty())
	{

	mainLayout->addWidget(updateBox);
	}

	if (errorMsgMissing != "NULL") {
		QLabel *pluginstallerLabel = new QLabel;
		pluginstallerLabel->setText(QString::fromStdString(
			"<br>Select '<b>Download StreamUP Pluginstaller</b>' to<br>use our tool to download them all at once."));
		pluginstallerLabel->setTextFormat(Qt::RichText);
		pluginstallerLabel->setAlignment(Qt::AlignCenter);
		QVBoxLayout *pluginstallerLayout = new QVBoxLayout;
		pluginstallerLayout->addWidget(pluginstallerLabel);
		mainLayout->addLayout(pluginstallerLayout);
	}

	// Buttons
	QPushButton *okButton = new QPushButton("OK");
	QObject::connect(okButton, &QPushButton::clicked, &dialog,
			 &QDialog::accept);

	// Create a QHBoxLayout for the buttons
	QHBoxLayout *buttonLayout = new QHBoxLayout;

	// Add the buttons to the buttonLayout
	if (errorMsgMissing != "NULL") {
		QPushButton *customButton =
			new QPushButton("Download StreamUP Pluginstaller");

		QObject::connect(customButton, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl(
				"https://streamup.tips/product/plugin-installer"));
		});
		buttonLayout->addWidget(customButton);
	}
	buttonLayout->addWidget(okButton);

	mainLayout->addLayout(buttonLayout);

	dialog.setLayout(mainLayout);
	dialog.setWindowTitle("StreamUP • Missing or Outdated Plugins");
	dialog.setFixedSize(dialog.sizeHint());
	dialog.exec();
}

void CheckAllPluginsForUpdates()
{
	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgUpdate = "";
	char *filepath = GetFilePath();

	for (const auto &module : all_plugins) {
		std::string plugin_name = module.first;
		PluginInfo plugin_info = module.second;
		std::string required_version = plugin_info.version;
		std::string url;
		if (std::string(PLATFORM_NAME) == "Windows") {
			url = plugin_info.windowsURL;
		} else if (std::string(PLATFORM_NAME) == "MacOS") {
			url = plugin_info.macURL;
		} else if (std::string(PLATFORM_NAME) == "Linux") {
			url = plugin_info.linuxURL;
		} else {
			// Default or error case
			url = plugin_info.windowsURL;
		}
		std::string search_string = plugin_info.searchString;

		std::string installed_version =
			search_string_in_file(filepath, search_string.c_str());

		if (!installed_version.empty() &&
		    installed_version != required_version) {
			version_mismatch_modules.insert(
				{plugin_name, installed_version});
		}
	}

	bfree(filepath);

	if (!version_mismatch_modules.empty()) {
		errorMsgUpdate += "<ul>";
		for (const auto &module : version_mismatch_modules) {
			PluginInfo plugin_info = all_plugins[module.first];
			std::string required_version = plugin_info.version;
			std::string url;
			if (std::string(PLATFORM_NAME) == "windows") {
				url = plugin_info.windowsURL;
			} else if (std::string(PLATFORM_NAME) == "mac") {
				url = plugin_info.macURL;
			} else if (std::string(PLATFORM_NAME) == "linux") {
				url = plugin_info.linuxURL;
			} else {
				// Default or error case
				url = plugin_info.windowsURL;
			}
			errorMsgUpdate +=
				"<li><a href=\"" + url + "\">" + module.first +
				"</a> (Installed: " + module.second +
				", Current: " + required_version + ")</li>";
		}
		errorMsgUpdate += "</ul>";

		PluginsHaveIssue("NULL", errorMsgUpdate);
		version_mismatch_modules.clear();
	} else {
		PluginsUpToDateOutput();
	}
}

void CheckRecommendedOBSPlugins()
{
	std::map<std::string, std::string> missing_modules;
	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgMissing = "";
	std::string errorMsgUpdate = "";
	char *filepath = GetFilePath();

	for (const auto &module : recommended_plugins) {
		std::string plugin_name = module.first;
		PluginInfo plugin_info = module.second;
		std::string required_version = plugin_info.version;
		std::string url;
		if (std::string(PLATFORM_NAME) == "Windows") {
			url = plugin_info.windowsURL;
		} else if (std::string(PLATFORM_NAME) == "MacOS") {
			url = plugin_info.macURL;
		} else if (std::string(PLATFORM_NAME) == "Linux") {
			url = plugin_info.linuxURL;
		} else {
			// Default or error case
			url = plugin_info.windowsURL;
		}
		std::string search_string = plugin_info.searchString;

		std::string installed_version =
			search_string_in_file(filepath, search_string.c_str());

		// Only add to missing_modules if plugin is recommended and not installed
		if (installed_version.empty() && plugin_info.recommended) {
			missing_modules.insert({plugin_name, required_version});
		}
		// Only add to version_mismatch_modules if plugin is recommended and there's a version mismatch
		else if (!installed_version.empty() &&
			 installed_version != required_version &&
			 plugin_info.recommended) {
			version_mismatch_modules.insert(
				{plugin_name, installed_version});
		}
	}

	bfree(filepath);

	if (!missing_modules.empty() || !version_mismatch_modules.empty()) {
		if (!missing_modules.empty()) {
			errorMsgMissing += "<ul>";
			for (const auto &module : missing_modules) {
				std::string plugin_name = module.first;
				std::string required_version = module.second;

				PluginInfo plugin_info =
					recommended_plugins[plugin_name];
				std::string url;
				if (std::string(PLATFORM_NAME) == "windows") {
					url = plugin_info.windowsURL;
				} else if (std::string(PLATFORM_NAME) ==
					   "mac") {
					url = plugin_info.macURL;
				} else if (std::string(PLATFORM_NAME) ==
					   "linux") {
					url = plugin_info.linuxURL;
				} else {
					// Default or error case
					url = plugin_info.windowsURL;
				}

				errorMsgMissing += "<li><a href=\"" + url +
						   "\">" + plugin_name +
						   "</a></li>";
			}
			errorMsgMissing += "</ul>";
		}

		if (!version_mismatch_modules.empty()) {
			errorMsgUpdate += "<ul>";
			for (const auto &module : version_mismatch_modules) {
				std::string plugin_name = module.first;
				std::string installed_version = module.second;

				PluginInfo plugin_info =
					recommended_plugins[plugin_name];
				std::string required_version =
					plugin_info.version;
				std::string url;
				if (std::string(PLATFORM_NAME) == "windows") {
					url = plugin_info.windowsURL;
				} else if (std::string(PLATFORM_NAME) ==
					   "mac") {
					url = plugin_info.macURL;
				} else if (std::string(PLATFORM_NAME) ==
					   "linux") {
					url = plugin_info.linuxURL;
				} else {
					// Default or error case
					url = plugin_info.windowsURL;
				}
				errorMsgUpdate +=
					"<li><a href=\"" + url + "\">" +
					plugin_name + "</a> (Installed: " +
					installed_version +
					", Current: " + required_version +
					")</li>";
			}
			errorMsgUpdate += "</ul>";
		}
		PluginsHaveIssue(errorMsgMissing, errorMsgUpdate);
		missing_modules.clear();
		version_mismatch_modules.clear();

	} else {
		PluginsUpToDateOutput();
	}
}

class AboutDialog : public QDialog {
public:
	AboutDialog(QWidget *parent = nullptr) : QDialog(parent)
	{
		// Version
		std::string version =
			PROJECT_VERSION; // Replace with actual version

		// Window settings
		setWindowTitle("StreamUP • About");
		setFixedSize(sizeHint());

		// Layouts
		QVBoxLayout *mainLayout = new QVBoxLayout;
		QHBoxLayout *topLayout = new QHBoxLayout;
		QVBoxLayout *iconLayout = new QVBoxLayout;
		QVBoxLayout *textLayout = new QVBoxLayout;
		QVBoxLayout *secondLayout = new QVBoxLayout;
		QVBoxLayout *thirdLayout = new QVBoxLayout;
		QVBoxLayout *lastLayout = new QVBoxLayout;
		QGridLayout *gridLayout = new QGridLayout;
		QGridLayout *gridLayout2 = new QGridLayout;
		QHBoxLayout *buttonLayout = new QHBoxLayout;

		// Main message and icon
		QLabel *iconLabel = new QLabel;
		iconLabel->setPixmap(
			QApplication::style()
				->standardIcon(QStyle::SP_MessageBoxInformation)
				.pixmap(64, 64));
		iconLabel->setStyleSheet("padding-top: 3px;");

		QLabel *mainMessageLabel = new QLabel;
		mainMessageLabel->setText(
			"StreamUP OBS plugin (version " +
			QString::fromStdString(version) +
			")<br>by <b>Andi Stone</b> (<b>Andilippi</b>)<br>");
		mainMessageLabel->setTextFormat(Qt::RichText);
		mainMessageLabel->setTextInteractionFlags(
			Qt::TextBrowserInteraction);
		mainMessageLabel->setOpenExternalLinks(true);

		iconLayout->addWidget(iconLabel);
		iconLayout->addStretch();
		textLayout->addWidget(mainMessageLabel);
		textLayout->addStretch();

		topLayout->addStretch(1);
		topLayout->addLayout(iconLayout);
		topLayout->addLayout(textLayout, 1);
		topLayout->addStretch(1);

		// Second message
		QLabel *secondText = new QLabel;
		secondText->setText(
			"Please consider supporting via the following<br>links or using the donation button below!");
		secondText->setTextFormat(Qt::RichText);
		secondText->setTextInteractionFlags(Qt::TextBrowserInteraction);
		secondText->setOpenExternalLinks(true);
		secondText->setAlignment(Qt::AlignCenter);

		secondLayout->addWidget(secondText);
		secondLayout->setAlignment(Qt::AlignCenter);

		// Patreon and Ko-fi links
		QLabel *textLabel1 = new QLabel;
		textLabel1->setText(
			"<a href='https://patreon.com/andilippi'>Andi's Patreon</a><br><a href='https://ko-fi.com/andilippi'>Andi's Ko-Fi</a>");
		textLabel1->setTextFormat(Qt::RichText);
		textLabel1->setTextInteractionFlags(Qt::TextBrowserInteraction);
		textLabel1->setOpenExternalLinks(true);

		QLabel *textLabel2 = new QLabel;
		textLabel2->setText(
			"<a href='https://patreon.com/streamup'>StreamUP's Patreon</a><br><a href='https://ko-fi.com/streamup'>StreamUP's Ko-Fi</a>");
		textLabel2->setTextFormat(Qt::RichText);
		textLabel2->setTextInteractionFlags(Qt::TextBrowserInteraction);
		textLabel2->setOpenExternalLinks(true);

		gridLayout->addWidget(textLabel1, 0, 0);
		gridLayout->addWidget(textLabel2, 0, 1);
		gridLayout->setHorizontalSpacing(40);
		gridLayout->setAlignment(Qt::AlignCenter);

		// Create a QGroupBox and add the second message and the Patreon and Ko-fi links
		QGroupBox *supportBox = new QGroupBox("Support");
		QVBoxLayout *boxLayout = new QVBoxLayout;
		boxLayout->addWidget(secondText);
		boxLayout->addLayout(gridLayout);
		supportBox->setLayout(boxLayout);

		// Third message
		QLabel *thirdText = new QLabel;
		thirdText->setText(
			"Follow Andi on the following platforms.<br>Be sure to stop by and say hi!");
		thirdText->setTextFormat(Qt::RichText);
		thirdText->setTextInteractionFlags(Qt::TextBrowserInteraction);
		thirdText->setOpenExternalLinks(true);
		thirdText->setAlignment(Qt::AlignCenter);

		thirdLayout->addWidget(thirdText);
		thirdLayout->setAlignment(Qt::AlignCenter);

		// Andi social links
		QLabel *textLabel3 = new QLabel;
		textLabel3->setText(
			"<a href='https://youtube.com/andilippi'>YouTube</a>");
		textLabel3->setTextFormat(Qt::RichText);
		textLabel3->setTextInteractionFlags(Qt::TextBrowserInteraction);
		textLabel3->setOpenExternalLinks(true);
		QLabel *textLabel4 = new QLabel;
		textLabel4->setText(
			"<a href='https://twitch.tv/andilippi'>Twitch</a>");
		textLabel4->setTextFormat(Qt::RichText);
		textLabel4->setTextInteractionFlags(Qt::TextBrowserInteraction);
		textLabel4->setOpenExternalLinks(true);
		QLabel *textLabel5 = new QLabel;
		textLabel5->setText(
			"<a href='https://twitter.com/andi_stone'>Twitter</a>");
		textLabel5->setTextFormat(Qt::RichText);
		textLabel5->setTextInteractionFlags(Qt::TextBrowserInteraction);
		textLabel5->setOpenExternalLinks(true);

		gridLayout2->addWidget(textLabel3, 0, 0);
		gridLayout2->addWidget(textLabel4, 0, 1);
		gridLayout2->addWidget(textLabel5, 0, 2);
		gridLayout2->setHorizontalSpacing(40);
		gridLayout2->setAlignment(Qt::AlignCenter);

		// Create a QGroupBox
		QGroupBox *socialBox = new QGroupBox("Andi's Socials");
		QVBoxLayout *boxLayout2 = new QVBoxLayout;
		boxLayout2->addWidget(thirdText);
		boxLayout2->addLayout(gridLayout2);
		socialBox->setLayout(boxLayout2);

		// Last message
		QLabel *lastText = new QLabel;
		lastText->setText(
			"<br>Special thanks to <b><a href='https://exeldro.com'>Exeldro</a></b>!<br>For creating the initial StreamUP plugin.");
		lastText->setTextFormat(Qt::RichText);
		lastText->setTextInteractionFlags(Qt::TextBrowserInteraction);
		lastText->setOpenExternalLinks(true);
		lastText->setAlignment(Qt::AlignCenter);

		lastLayout->addWidget(lastText);
		lastLayout->setAlignment(Qt::AlignCenter);

		// Donate button
		QPushButton *donateButton = new QPushButton("Donate", this);
		buttonLayout->addWidget(donateButton);

		// Connect donate button
		connect(donateButton, &QPushButton::clicked, [] {
			QDesktopServices::openUrl(
				QUrl("https://paypal.me/andilippi"));
		});

		// Close button
		QPushButton *closeButton = new QPushButton("Close", this);
		buttonLayout->addWidget(closeButton);

		// Connect close button
		connect(closeButton, &QPushButton::clicked, this,
			&QDialog::accept);

		// Add all layouts to the main layout
		mainLayout->setContentsMargins(20, 10, 20, 10);
		mainLayout->addLayout(topLayout);
		mainLayout->addWidget(supportBox);
		mainLayout->addWidget(socialBox);
		mainLayout->addLayout(lastLayout);
		mainLayout->addLayout(buttonLayout);

		// Set main layout
		setLayout(mainLayout);
	}
};

static void LoadMenu(QMenu *menu)
{

	menu->clear();
	QAction *a;

	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		a = menu->addAction(obs_module_text("Install a Product"));
		QObject::connect(a, &QAction::triggered,
				 [] { LoadStreamUpFile(NULL); });
		a = menu->addAction(
			obs_module_text("Check Required OBS Plugins"));
		QObject::connect(a, &QAction::triggered,
				 [] { CheckRecommendedOBSPlugins(); });
		menu->addSeparator();
	}

	a = menu->addAction(obs_module_text("Check for OBS Plugin Updates"));
	QObject::connect(a, &QAction::triggered,
			 [] { CheckAllPluginsForUpdates(); });
	menu->addSeparator();

	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		a = menu->addAction(obs_module_text("Download Products"));
		QObject::connect(a, &QAction::triggered, []() {
			QDesktopServices::openUrl(
				QUrl("https://streamup.tips/"));
		});
	}
	a = menu->addAction(obs_module_text("About"));
	QObject::connect(a, &QAction::triggered, [] {
		AboutDialog dialog;
		dialog.exec();
	});
}

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] loaded version %s", PROJECT_VERSION);
	initialize_required_modules();

	QAction *action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("StreamUP")));
	QMenu *menu = new QMenu();
	action->setMenu(menu);
	QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenu(menu); });

	return true;
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
