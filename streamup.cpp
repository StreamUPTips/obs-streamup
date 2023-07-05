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
#include <QCoreApplication>
#include <QCheckBox>
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
#include <QTimer>
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

//--------------------CHECK FOR PLUGIN UPDATES ETC--------------------
void ShowNonModalDialog(const std::function<void()> &dialogFunction)
{
	QMetaObject::invokeMethod(qApp, dialogFunction, Qt::QueuedConnection);
}

QLabel *CreateRichTextLabel(const QString &text)
{
	QLabel *label = new QLabel;
	label->setText(text);
	label->setTextFormat(Qt::RichText);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	return label;
}

void ErrorDialog(const QString &errorMessage)
{
	ShowNonModalDialog([errorMessage]() {
		QDialog *errorDialog = new QDialog();
		errorDialog->setWindowTitle(
			obs_module_text("WindowErrorTitle"));

		QVBoxLayout *errorLayout = new QVBoxLayout(errorDialog);
		errorLayout->setContentsMargins(20, 15, 20, 10);

		QLabel *iconLabel = new QLabel();
		iconLabel->setPixmap(
			QApplication::style()
				->standardIcon(QStyle::SP_MessageBoxCritical)
				.pixmap(32, 32));

		QLabel *errorLabel = CreateRichTextLabel(errorMessage);
		errorLabel->setWordWrap(true);

		QHBoxLayout *topLayout = new QHBoxLayout();
		topLayout->addWidget(iconLabel);
		topLayout->addSpacing(5);
		topLayout->addWidget(errorLabel, 1);

		errorLayout->addLayout(topLayout);
		errorLayout->addSpacing(10);

		QPushButton *okButton = new QPushButton("OK");
		QObject::connect(okButton, &QPushButton::clicked, errorDialog,
				 &QDialog::close);

		errorLayout->addWidget(okButton);
		errorDialog->setLayout(errorLayout);
		errorDialog->setWindowFlags(Qt::Window);
		errorDialog->setAttribute(Qt::WA_DeleteOnClose);
		errorDialog->setFixedSize(errorDialog->sizeHint());

		errorDialog->show();
	});
}

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
			blog(LOG_INFO, "curl_easy_perform() failed: %s\n",
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
		blog(LOG_INFO, "Error creating thread\n");
		return;
	}

	if (pthread_join(thread, NULL)) {
		blog(LOG_INFO, "Error joining thread\n");
		return;
	}

	std::string api_response = req_data.response;
	//api_response = "";
	/* if (api_response.find("Error:") != std::string::npos) {
		ErrorMessage errorMsg;
		errorMsg.showMessage(QString::fromStdString(api_response));
		return;
	}

	if (api_response == "") {
		ErrorMessage errorMsg;
		errorMsg.showMessage(
			"Unable to retrive plugin list from the StreamUP API.");
		return;
	}*/

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

char *GetFilePath()
{
	char *path;
	char *path_abs;
	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		path = obs_module_config_path("../../logs/");
		path_abs = os_get_abs_path_ptr(path);

		if (path_abs[strlen(path_abs) - 1] != '/' &&
		    path_abs[strlen(path_abs) - 1] != '\\') {
			// Create a new string with appended "/"
			char *newPathAbs = new char[strlen(path_abs) + 2];
			strcpy(newPathAbs, path_abs);
			strcat(newPathAbs, "/");

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

		path_abs = new char[path_str.size() + 1];
		std::strcpy(path_abs, path_str.c_str());
	}
	bfree(path);

	if (std::filesystem::exists(path_abs)) {
		// The directory exists
		std::filesystem::directory_iterator dir(path_abs);
		if (dir == std::filesystem::directory_iterator{}) {
			// The directory is empty
			blog(LOG_INFO,
			     "OBS doesn't have files in the install directory.");
			return NULL;
		} else {
			// The directory contains files
			return path_abs;
		}
	} else {
		// The directory does not exist
		blog(LOG_INFO,
		     "OBS log file folder does not exist in the install directory.");

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
	ShowNonModalDialog([]() {
		QDialog *successDialog = new QDialog();
		successDialog->setWindowTitle(
			obs_module_text("WindowUpToDateTitle"));
		successDialog->setFixedSize(successDialog->sizeHint());

		QVBoxLayout *successLayout = new QVBoxLayout(successDialog);
		successLayout->setContentsMargins(20, 15, 20, 10);

		QLabel *iconLabel = new QLabel();
		iconLabel->setPixmap(
			QApplication::style()
				->standardIcon(QStyle::SP_DialogApplyButton)
				.pixmap(32, 32));

		QLabel *successLabel = CreateRichTextLabel(
			obs_module_text("WindowUpToDateMessage"));

		QHBoxLayout *topLayout = new QHBoxLayout();
		topLayout->addWidget(iconLabel);
		topLayout->addSpacing(10);
		topLayout->addWidget(successLabel, 1);

		successLayout->addLayout(topLayout);
		successLayout->addSpacing(10);

		QPushButton *okButton = new QPushButton(obs_module_text("OK"));
		QObject::connect(okButton, &QPushButton::clicked, successDialog,
				 &QDialog::close);

		successLayout->addWidget(okButton);
		successDialog->setLayout(successLayout);
		successDialog->setWindowFlags(Qt::Window);
		successDialog->setAttribute(Qt::WA_DeleteOnClose);
		successDialog->show();
	});
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate)
{
	ShowNonModalDialog([errorMsgMissing, errorMsgUpdate]() {
		QDialog *dialog = new QDialog();
		dialog->setWindowTitle(
			obs_module_text("WindowPluginErrorTitle"));
		dialog->setFixedSize(dialog->sizeHint());

		QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
		mainLayout->setContentsMargins(20, 15, 20, 10);

		// First layout (icon and main message)
		QLabel *iconLabel = new QLabel();
		iconLabel->setPixmap(
			QApplication::style()
				->standardIcon(QStyle::SP_MessageBoxWarning)
				.pixmap(64, 64));
		iconLabel->setStyleSheet("padding-top: 3px;");

		QLabel *mainMessageLabel = CreateRichTextLabel(
			(errorMsgMissing != "NULL")
				? obs_module_text("WindowPluginErrorMissing")
				: obs_module_text("WindowPluginErrorUpdating"));

		QVBoxLayout *iconLayout = new QVBoxLayout();
		iconLayout->addWidget(iconLabel);
		iconLayout->addStretch();

		QVBoxLayout *textLayout = new QVBoxLayout();
		textLayout->addWidget(mainMessageLabel);
		textLayout->addStretch();

		QHBoxLayout *topLayout = new QHBoxLayout();
		topLayout->addStretch(1);
		topLayout->addLayout(iconLayout);
		topLayout->addLayout(textLayout, 1);
		topLayout->addStretch(1);

		mainLayout->addLayout(topLayout);
		mainLayout->addSpacing(10);

		// Third layout (detailed error report for plugins to update)
		if (!errorMsgUpdate.empty()) {
			QLabel *updateLabel = CreateRichTextLabel(
				QString::fromStdString(errorMsgUpdate));

			QGroupBox *updateBox = new QGroupBox(obs_module_text(
				"WindowPluginErrorUpdateGroup"));
			QVBoxLayout *updateBoxLayout = new QVBoxLayout();
			updateBoxLayout->addWidget(updateLabel);
			updateBox->setLayout(updateBoxLayout);

			mainLayout->addWidget(updateBox);
		}

		// Second layout (detailed error report for missing plugins)
		if (errorMsgMissing != "NULL") {
			mainLayout->addSpacing(10);

			QLabel *missingLabel = CreateRichTextLabel(
				QString::fromStdString(errorMsgMissing));

			QGroupBox *missingBox = new QGroupBox(obs_module_text(
				"WindowPluginErrorMissingGroup"));
			QVBoxLayout *missingBoxLayout = new QVBoxLayout();
			missingBoxLayout->addWidget(missingLabel);
			missingBox->setLayout(missingBoxLayout);

			mainLayout->addWidget(missingBox);
		}

		mainLayout->addSpacing(5);

		if (errorMsgMissing != "NULL") {
			mainLayout->addSpacing(5);
			QLabel *pluginstallerLabel = CreateRichTextLabel(
				obs_module_text("WindowPluginErrorFooter"));
			pluginstallerLabel->setAlignment(Qt::AlignCenter);
			QVBoxLayout *pluginstallerLayout = new QVBoxLayout();
			pluginstallerLayout->addWidget(pluginstallerLabel);
			mainLayout->addLayout(pluginstallerLayout);
		}

		// Buttons
		QPushButton *okButton = new QPushButton(obs_module_text("OK"));
		QObject::connect(okButton, &QPushButton::clicked, dialog,
				 &QWidget::close);

		QHBoxLayout *buttonLayout = new QHBoxLayout();

		if (errorMsgMissing != "NULL") {
			QPushButton *customButton = new QPushButton(
				obs_module_text("DownloadPluginstaller"));
			QObject::connect(customButton, &QPushButton::clicked, []() {
				QDesktopServices::openUrl(QUrl(
					"https://streamup.tips/product/plugin-installer"));
			});
			buttonLayout->addWidget(customButton);
		}

		buttonLayout->addWidget(okButton);

		mainLayout->addLayout(buttonLayout);

		dialog->setLayout(mainLayout);
		dialog->setWindowFlags(Qt::Window);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
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

void CheckAllPluginsForUpdates()
{
	if (all_plugins.empty()) {
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return;
	} else {
		std::map<std::string, std::string> version_mismatch_modules;
		std::string errorMsgUpdate = "";
		char *filepath = GetFilePath();

		for (const auto &module : all_plugins) {
			std::string plugin_name = module.first;
			PluginInfo plugin_info = module.second;
			std::string required_version = plugin_info.version;
			std::string url = GetPlatformURL(plugin_info);
			std::string search_string = plugin_info.searchString;

			std::string installed_version = search_string_in_file(
				filepath, search_string.c_str());

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
				PluginInfo plugin_info =
					all_plugins[module.first];
				std::string required_version =
					plugin_info.version;
				std::string url = GetPlatformURL(plugin_info);
				errorMsgUpdate +=
					"<li><a href=\"" + url + "\">" +
					module.first + "</a> (" +
					obs_module_text("Installed") + ": " +
					module.second + ", " +
					obs_module_text("Current") + ": " +
					required_version + ")</li>";
			}
			errorMsgUpdate += "</ul>";

			PluginsHaveIssue("NULL", errorMsgUpdate);
			version_mismatch_modules.clear();
		} else {
			PluginsUpToDateOutput();
		}
	}
}

bool CheckRecommendedOBSPlugins()
{
	if (recommended_plugins.empty()) {
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return false;
	} else {

		std::map<std::string, std::string> missing_modules;
		std::map<std::string, std::string> version_mismatch_modules;
		std::string errorMsgMissing = "";
		std::string errorMsgUpdate = "";
		char *filepath = GetFilePath();

		for (const auto &module : recommended_plugins) {
			std::string plugin_name = module.first;
			PluginInfo plugin_info = module.second;
			std::string required_version = plugin_info.version;
			std::string url = GetPlatformURL(plugin_info);
			std::string search_string = plugin_info.searchString;

			std::string installed_version = search_string_in_file(
				filepath, search_string.c_str());

			// Only add to missing_modules if plugin is recommended and not installed
			if (installed_version.empty() &&
			    plugin_info.recommended) {
				missing_modules.insert(
					{plugin_name, required_version});
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

		if (!missing_modules.empty() ||
		    !version_mismatch_modules.empty()) {
			if (!missing_modules.empty()) {
				errorMsgMissing += "<ul>";
				for (const auto &module : missing_modules) {
					std::string plugin_name = module.first;
					std::string required_version =
						module.second;
					PluginInfo plugin_info =
						recommended_plugins[plugin_name];
					std::string url =
						GetPlatformURL(plugin_info);

					errorMsgMissing +=
						"<li><a href=\"" + url + "\">" +
						plugin_name + "</a></li>";
				}
				errorMsgMissing += "</ul>";
			}

			if (!version_mismatch_modules.empty()) {
				errorMsgUpdate += "<ul>";
				for (const auto &module :
				     version_mismatch_modules) {
					PluginInfo plugin_info =
						all_plugins[module.first];
					std::string required_version =
						plugin_info.version;
					std::string url =
						GetPlatformURL(plugin_info);
					errorMsgUpdate +=
						"<li><a href=\"" + url + "\">" +
						module.first + "</a> (" +
						obs_module_text("Installed") +
						": " +
						module.second + ", " +
						obs_module_text("Current") +
						": " +
						required_version + ")</li>";
				}
				errorMsgUpdate += "</ul>";
			}
			PluginsHaveIssue(errorMsgMissing, errorMsgUpdate);
			missing_modules.clear();
			version_mismatch_modules.clear();
			return false;

		} else {
			PluginsUpToDateOutput();
			return true;
		}
	}
}

//--------------------MENU & ABOUT-------------------
void LoadStreamUpFile(void *private_data)
{
	UNUSED_PARAMETER(private_data);

	if (!CheckRecommendedOBSPlugins()) {
		return;
	}

	QString fileName = QFileDialog::getOpenFileName(
		nullptr, QT_UTF8(obs_module_text("Load")), QString(),
		"StreamUP File (*.streamup)");
	if (fileName.isEmpty()) {
		return;
	}
	obs_data_t *data = obs_data_create_from_json_file(QT_TO_UTF8(fileName));
	LoadScene(data, QFileInfo(fileName).absolutePath());
	obs_data_release(data);
}

void ShowAboutDialog()
{
	// Version
	std::string version = PROJECT_VERSION;
	QDialog *dialog = new QDialog();
	dialog->setWindowTitle(obs_module_text("WindowAboutTitle"));
	dialog->setFixedSize(dialog->sizeHint());

	QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
	mainLayout->setContentsMargins(20, 15, 20, 10);

	QHBoxLayout *topLayout = new QHBoxLayout();
	mainLayout->addLayout(topLayout);
	topLayout->setAlignment(Qt::AlignCenter);

	QLabel *iconLabel = new QLabel();
	iconLabel->setPixmap(
		QApplication::style()
			->standardIcon(QStyle::SP_MessageBoxInformation)
			.pixmap(64, 64));
	topLayout->addWidget(iconLabel);

	QLabel *mainMessageLabel = CreateRichTextLabel(
		"StreamUP OBS plugin (version " +
		QString::fromStdString(version) +
		")<br>by <b>Andi Stone</b> (<b>Andilippi</b>)");
	mainMessageLabel->setAlignment(Qt::AlignHCenter);
	topLayout->addWidget(mainMessageLabel);
	mainMessageLabel->setStyleSheet("margin-left: 0px; margin-top: 0px;");

	mainLayout->addSpacing(10);

	// Support QGroupBox
	QGroupBox *supportBox = new QGroupBox(obs_module_text("Support"));
	QVBoxLayout *supportBoxLayout = new QVBoxLayout;
	mainLayout->addWidget(supportBox);
	supportBox->setLayout(supportBoxLayout);

	// Second message
	QLabel *secondText =
		CreateRichTextLabel(obs_module_text("WindowAboutSupport"));
	secondText->setAlignment(Qt::AlignCenter);
	supportBoxLayout->addWidget(secondText);
	supportBoxLayout->addSpacing(5);

	// Patreon and Ko-fi links
	QGridLayout *gridLayout = new QGridLayout;
	supportBoxLayout->addLayout(gridLayout);
	QLabel *textLabel1 = CreateRichTextLabel(
		"<b><a href='https://patreon.com/andilippi'>Andi's Patreon</a><br><a href='https://ko-fi.com/andilippi'>Andi's Ko-Fi</a></b>");
	textLabel1->setAlignment(Qt::AlignCenter);
	gridLayout->addWidget(textLabel1, 0, 0);
	QLabel *textLabel2 = CreateRichTextLabel(
		"<b><a href='https://patreon.com/streamup'>StreamUP's Patreon</a><br><a href='https://ko-fi.com/streamup'>StreamUP's Ko-Fi</a></b>");
	textLabel2->setAlignment(Qt::AlignCenter);
	gridLayout->addWidget(textLabel2, 0, 1);
	gridLayout->setHorizontalSpacing(20);
	gridLayout->setAlignment(Qt::AlignCenter);

	// Spacer between QGroupBoxes
	mainLayout->addSpacing(10);

	// Andi's Socials QGroupBox
	QGroupBox *socialBox =
		new QGroupBox(obs_module_text("WindowAboutSocialsTitle"));
	QVBoxLayout *socialBoxLayout = new QVBoxLayout;
	mainLayout->addWidget(socialBox);
	socialBox->setLayout(socialBoxLayout);

	// Third message
	QLabel *thirdText =
		CreateRichTextLabel(obs_module_text("WindowAboutSocialsMsg"));
	thirdText->setAlignment(Qt::AlignCenter);
	socialBoxLayout->addWidget(thirdText);
	socialBoxLayout->addSpacing(5);

	// Andi social links
	QGridLayout *gridLayout2 = new QGridLayout;
	socialBoxLayout->addLayout(gridLayout2);

	QLabel *textLabel3 = CreateRichTextLabel(
		"<b><a href='https://youtube.com/andilippi'>YouTube</a></b>");
	gridLayout2->addWidget(textLabel3, 0, 0);

	QLabel *textLabel4 = CreateRichTextLabel(
		"<b><a href='https://twitch.tv/andilippi'>Twitch</a></b>");
	gridLayout2->addWidget(textLabel4, 0, 1);

	QLabel *textLabel5 = CreateRichTextLabel(
		"<b><a href='https://twitter.com/andi_stone'>Twitter</a></b>");
	gridLayout2->addWidget(textLabel5, 0, 2);

	gridLayout2->setHorizontalSpacing(30);
	gridLayout2->setAlignment(Qt::AlignCenter);

	mainLayout->addSpacing(10);

	// Last message
	QLabel *lastText =
		CreateRichTextLabel(obs_module_text("WindowAboutThanks"));
	lastText->setAlignment(Qt::AlignCenter);
	mainLayout->addWidget(lastText);

	// Buttons
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	mainLayout->addLayout(buttonLayout);

	QPushButton *donateButton = new QPushButton(obs_module_text("Donate"));
	buttonLayout->addWidget(donateButton);
	QObject::connect(donateButton, &QPushButton::clicked, []() {
		QDesktopServices::openUrl(QUrl("https://paypal.me/andilippi"));
	});

	QPushButton *closeButton = new QPushButton(obs_module_text("Close"));
	buttonLayout->addWidget(closeButton);
	QObject::connect(closeButton, &QPushButton::clicked, dialog,
			 &QDialog::close);

	dialog->setLayout(mainLayout);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowFlags(Qt::Window);
	dialog->show();
}

obs_data_t *LoadSettings()
{
	char *configPath = obs_module_config_path("config.json");
	os_mkdirs(configPath);
	obs_data_t *settings = obs_data_create_from_json_file(configPath);
	bfree(configPath);
	return settings;
}

void SaveSettings(obs_data_t *settings)
{
	char *configPath = obs_module_config_path("config.json");

	if (obs_data_save_json(settings, configPath)) {
		blog(LOG_INFO, "Settings saved to %s", configPath);
	} else {
		blog(LOG_WARNING, "Failed to save settings to file.");
	}

	bfree(configPath);
}

void ShowSettingsDialog()
{
	obs_data_t *settings = LoadSettings();

	// Create the settings dialog
	QDialog *dialog = new QDialog();
	dialog->setWindowTitle(obs_module_text("WindowSettingsTitle"));
	dialog->setFixedSize(dialog->sizeHint());

	QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
	mainLayout->setContentsMargins(20, 15, 20, 10);

	// Create the boolean property
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p = obs_properties_add_bool(
		props, "run_at_startup",
		obs_module_text("WindowSettingsRunOnStartup"));

	// Create a horizontal layout for the label and checkbox
	QHBoxLayout *checkboxLayout = new QHBoxLayout();
	QLabel *label =
		new QLabel(obs_module_text("WindowSettingsRunOnStartup"));
	QCheckBox *checkBox = new QCheckBox();
	checkBox->setChecked(obs_data_get_bool(settings, obs_property_name(p)));
	checkboxLayout->addWidget(checkBox);
	checkboxLayout->addWidget(label);

	// Connect the checkbox stateChanged signal to update the property value
	QObject::connect(checkBox, &QCheckBox::stateChanged, [=](int state) {
		bool newValue = (state == Qt::Checked);
		obs_data_set_bool(settings, obs_property_name(p), newValue);
	});

	// Add the checkbox layout to the main layout
	mainLayout->addLayout(checkboxLayout);

	// Create a horizontal layout for the buttons
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	QPushButton *cancelButton = new QPushButton(obs_module_text("Cancel"));
	QPushButton *saveButton = new QPushButton(obs_module_text("Save"));
	buttonLayout->addWidget(cancelButton);
	buttonLayout->addWidget(saveButton);

	// Connect the cancel button's clicked signal to close the dialog
	QObject::connect(cancelButton, &QPushButton::clicked, dialog,
			 &QDialog::close);

	// Connect the save button's clicked signal to save the settings and close the dialog
	QObject::connect(saveButton, &QPushButton::clicked, [=]() {
		SaveSettings(settings);
		dialog->close();
	});

	// Add the button layout to the main layout
	mainLayout->addLayout(buttonLayout);

	dialog->setLayout(mainLayout);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowFlags(Qt::Window);

	// Connect the dialog's finished signal to release the settings
	QObject::connect(dialog, &QDialog::finished, [=](int) {
		obs_data_release(settings), obs_properties_destroy(props);
	});

	dialog->show();
}

static void LoadMenu(QMenu *menu)
{

	menu->clear();
	QAction *a;

	if (strcmp(PLATFORM_NAME, "windows") == 0) {
		a = menu->addAction(obs_module_text("MenuInstallProduct"));
		QObject::connect(a, &QAction::triggered,
				 [] { LoadStreamUpFile(NULL); });
		a = menu->addAction(obs_module_text("MenuDownloadProduct"));
		QObject::connect(a, &QAction::triggered, []() {
			QDesktopServices::openUrl(
				QUrl("https://streamup.tips/"));
		});

		a = menu->addAction(obs_module_text("MenuCheckRequirements"));
		QObject::connect(a, &QAction::triggered,
				 [] { CheckRecommendedOBSPlugins(); });
		menu->addSeparator();
	}

	a = menu->addAction(obs_module_text("MenuCheckPluginUpdates"));
	QObject::connect(a, &QAction::triggered,
			 [] { CheckAllPluginsForUpdates(); });
	menu->addSeparator();

	a = menu->addAction(obs_module_text("MenuAbout"));
	QObject::connect(a, &QAction::triggered, [] { ShowAboutDialog(); });

	a = menu->addAction(obs_module_text("MenuSettings"));
	QObject::connect(a, &QAction::triggered, [] { ShowSettingsDialog(); });
}

//--------------------GENERAL OBS--------------------

bool obs_module_load()
{
	blog(LOG_INFO, "[StreamUP] loaded version %s", PROJECT_VERSION);

	InitialiseRequiredModules();

	//Load run on startup settings
	obs_data_t *settings = LoadSettings();
	bool runAtStartup = obs_data_get_bool(settings, "run_at_startup");
	if (runAtStartup) {
		CheckAllPluginsForUpdates();
	}
	obs_data_release(settings);

	//load menu
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
