#include "plugin-manager.hpp"
#include "streamup-common.hpp"
#include "plugin-state.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "path-utils.hpp"
#include "http-client.hpp"
#include "ui-helpers.hpp"
#include <obs-module.h>
#include <obs-data.h>
#include <curl/curl.h>
#include <pthread.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QLabel>
#include <fstream>
#include <iostream>
#include <regex>
#include <util/platform.h>

// UI functions now accessed through StreamUP::UIHelpers namespace
extern char *GetFilePath();

namespace StreamUP {
namespace PluginManager {

//-------------------ERROR HANDLING FUNCTIONS-------------------
void ErrorDialog(const QString &errorMessage)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([errorMessage]() {
		QDialog *dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowErrorTitle");
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

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
			dialogLayout->setContentsMargins(20, 15, 20, 10);

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
		dialogLayout->setContentsMargins(20, 15, 20, 20);

		const char *errorText = (errorMsgMissing != "NULL") ? "WindowPluginErrorMissing" : "WindowPluginErrorUpdating";
		dialogLayout->addLayout(StreamUP::UIHelpers::AddIconAndText(QStyle::SP_MessageBoxWarning, errorText));
		dialogLayout->addSpacing(10);

		if (errorMsgMissing != "NULL") {
			QLabel *label = new QLabel(QString::fromStdString(errorMsgMissing));
			label->setTextFormat(Qt::RichText);
			label->setWordWrap(true);
			label->setOpenExternalLinks(true);
			label->setAlignment(Qt::AlignTop);
			dialogLayout->addWidget(label);
			dialogLayout->addSpacing(10);
		}

		if (!errorMsgUpdate.empty()) {
			QLabel *label = new QLabel(QString::fromStdString(errorMsgUpdate));
			label->setTextFormat(Qt::RichText);
			label->setWordWrap(true);
			label->setOpenExternalLinks(true);
			label->setAlignment(Qt::AlignTop);
			dialogLayout->addWidget(label);
		}

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("OK"), [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
	});
}

//-------------------PLUGIN UPDATE FUNCTIONS-------------------
void CheckAllPluginsForUpdates(bool manuallyTriggered)
{
	const auto& allPlugins = StreamUP::GetAllPlugins();
	if (allPlugins.empty()) {
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return;
	}

	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgUpdate = "";
	std::vector<std::pair<std::string, std::string>> installedPlugins = GetInstalledPlugins();

	for (const auto &plugin : installedPlugins) {
		const std::string &plugin_name = plugin.first;
		const std::string &installed_version = plugin.second;
		const auto& allPlugins = StreamUP::GetAllPlugins();
		const StreamUP::PluginInfo &plugin_info = allPlugins.at(plugin_name);
		const std::string &required_version = plugin_info.version;

		if (installed_version != required_version && VersionUtils::IsVersionLessThan(installed_version, required_version)) {
			version_mismatch_modules.emplace(plugin_name, installed_version);
		}
	}

	QString appDataPath = PathUtils::GetLocalAppDataPath();
	QString filePath = appDataPath + "/StreamUP-OutdatedPluginsList.txt";

	std::ofstream outFile(filePath.toStdString(), std::ios::out);
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
			const auto& allPlugins = StreamUP::GetAllPlugins();
			const StreamUP::PluginInfo &plugin_info = allPlugins.at(module.first);
			const std::string &required_version = plugin_info.version;
			const std::string &forum_link = plugin_info.generalURL;
			const std::string &direct_download_link = StringUtils::GetPlatformURL(
				QString::fromStdString(plugin_info.windowsURL),
				QString::fromStdString(plugin_info.macURL),
				QString::fromStdString(plugin_info.linuxURL),
				QString::fromStdString(plugin_info.generalURL)).toStdString();

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

// HTTP functionality moved to StreamUP::HttpClient module

void InitialiseRequiredModules()
{
	std::string api_response;
	const std::string url = "https://api.streamup.tips/plugins";
	
	if (!StreamUP::HttpClient::MakeGetRequest(url, api_response)) {
		blog(LOG_WARNING, "[StreamUP] Failed to fetch plugins from API");
		return;
	}

	if (api_response.find("Error:") != std::string::npos) {
		ErrorDialog(QString::fromStdString(api_response));
		return;
	}

	if (api_response.empty()) {
		blog(LOG_INFO, "[StreamUP] Error loading plugins from %s", url.c_str());
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

		StreamUP::PluginState::Instance().AddPlugin(name, info);

		if (obs_data_get_bool(plugin, "required")) {
			StreamUP::PluginState::Instance().AddRequiredPlugin(name, info);
		}

		obs_data_release(plugin);
	}

	obs_data_array_release(plugins);
	obs_data_release(data);
}

bool CheckrequiredOBSPlugins(bool isLoadStreamUpFile)
{
	const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
	if (requiredPlugins.empty()) {
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

	for (const auto &module : requiredPlugins) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &required_version = plugin_info.version;
		const std::string &search_string = plugin_info.searchString;

		if (search_string.find("[ignore]") != std::string::npos) {
			continue; // Skip to the next iteration
		}

		std::string installed_version = SearchStringInFileForVersion(filepath, search_string.c_str());

		if (installed_version.empty() && plugin_info.required) {
			missing_modules.emplace(plugin_name, required_version);
		} else if (!installed_version.empty() && installed_version != required_version && plugin_info.required &&
			   VersionUtils::IsVersionLessThan(installed_version, required_version)) {
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
				const auto& allPlugins = StreamUP::GetAllPlugins();
			const StreamUP::PluginInfo &plugin_info = allPlugins.at(module.first);
				const std::string &required_version = plugin_info.version;
				const std::string &forum_link = plugin_info.generalURL;
				const std::string &direct_download_link = StringUtils::GetPlatformURL(
					QString::fromStdString(plugin_info.windowsURL),
					QString::fromStdString(plugin_info.macURL),
					QString::fromStdString(plugin_info.linuxURL),
					QString::fromStdString(plugin_info.generalURL)).toStdString();

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
				const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
				const StreamUP::PluginInfo &pluginInfo = requiredPlugins.at(moduleName);
				const std::string &forum_link = pluginInfo.generalURL;
				const std::string &direct_download_link = StringUtils::GetPlatformURL(
					QString::fromStdString(pluginInfo.windowsURL),
					QString::fromStdString(pluginInfo.macURL),
					QString::fromStdString(pluginInfo.linuxURL),
					QString::fromStdString(pluginInfo.generalURL)).toStdString();

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

std::string SearchStringInFileForVersion(const char *path, const char *search)
{
	QString filepath = PathUtils::GetMostRecentFile(QString::fromLocal8Bit(path));
	
	if (filepath.isEmpty()) {
		return "";
	}
	
	FILE *file = fopen(filepath.toLocal8Bit().constData(), "r+");
	if (!file) {
		return "";
	}
	
	char line[256];
	std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	std::regex version_regex_double("[0-9]+\\.[0-9]+");

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

		std::string installed_version = SearchStringInFileForVersion(filepath, search_string.c_str());

		if (!installed_version.empty()) {
			installedPlugins.emplace_back(plugin_name, installed_version);
		}
	}

	bfree(filepath);

	return installedPlugins;
}

} // namespace PluginManager
} // namespace StreamUP