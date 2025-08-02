#include "plugin-manager.hpp"
#include "streamup-common.hpp"
#include "plugin-state.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "path-utils.hpp"
#include "http-client.hpp"
#include "ui-helpers.hpp"
#include "obs-wrappers.hpp"
#include "error-handler.hpp"
#include <obs-module.h>
#include <obs-data.h>
#include <curl/curl.h>
#include <pthread.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>
#include <QPushButton>
#include <QObject>
#include <QTimer>
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
	std::string message = errorMessage.isEmpty() ? "Unknown error occurred." : errorMessage.toStdString();
	StreamUP::ErrorHandler::ShowErrorDialog("Plugin Error", message);
}

void PluginsUpToDateOutput(bool manuallyTriggered)
{
	if (manuallyTriggered) {
		StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
			QDialog *toast = new QDialog();
			toast->setWindowTitle("StreamUP");
			toast->setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
			toast->setAttribute(Qt::WA_DeleteOnClose);
			toast->setStyleSheet("QDialog { background: #22c55e; border-radius: 8px; }");
			toast->resize(400, 100);
			toast->setFixedSize(400, 100);
			
			QVBoxLayout *toastLayout = new QVBoxLayout(toast);
			toastLayout->setContentsMargins(20, 15, 20, 15);
			toastLayout->setSpacing(8);
			
			QLabel *messageLabel = new QLabel("✓ All plugins are up to date!");
			messageLabel->setStyleSheet(
				"QLabel {"
				"color: white;"
				"font-size: 16px;"
				"font-weight: bold;"
				"background: transparent;"
				"border: none;"
				"}");
			messageLabel->setAlignment(Qt::AlignCenter);
			toastLayout->addWidget(messageLabel);
			
			QLabel *countdownLabel = new QLabel("Auto closing in 3 seconds");
			countdownLabel->setStyleSheet(
				"QLabel {"
				"color: rgba(255, 255, 255, 0.8);"
				"font-size: 12px;"
				"background: transparent;"
				"border: none;"
				"}");
			countdownLabel->setAlignment(Qt::AlignCenter);
			toastLayout->addWidget(countdownLabel);
			
			toast->setLayout(toastLayout);
			toast->show();
			
			// Create countdown timer
			QTimer *countdownTimer = new QTimer(toast);
			int *remainingSeconds = new int(3);
			
			QObject::connect(countdownTimer, &QTimer::timeout, [countdownLabel, remainingSeconds, toast, countdownTimer]() {
				(*remainingSeconds)--;
				if (*remainingSeconds > 0) {
					countdownLabel->setText(QString("Auto closing in %1 seconds").arg(*remainingSeconds));
				} else {
					countdownTimer->stop();
					toast->close();
				}
			});
			
			countdownTimer->start(1000); // Update every second
		});
	}
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([errorMsgMissing, errorMsgUpdate]() {
		QDialog *dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowPluginErrorTitle");
		dialog->setStyleSheet("QDialog { background: #13171f; }");
		
		// Dynamic sizing based on content
		int maxHeight = 600;
		int minHeight = 150;
		int maxWidth = 900;
		int minWidth = 700;
		
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 15);
		dialogLayout->setSpacing(15);

		// Dynamic title based on what issues exist
		QString titleText;
		bool hasMissing = (errorMsgMissing != "NULL");
		bool hasUpdates = (!errorMsgUpdate.empty());
		
		if (hasMissing && hasUpdates) {
			titleText = "Missing Plugins & Updates Available";
		} else if (hasMissing) {
			titleText = "Missing Required Plugins";
		} else if (hasUpdates) {
			titleText = "Plugin Updates Available";
		}

		// Header section
		QLabel *titleLabel = new QLabel(titleText);
		titleLabel->setStyleSheet(
			"QLabel {"
			"color: white;"
			"font-size: 20px;"
			"font-weight: bold;"
			"margin: 0px 0px 10px 0px;"
			"padding: 0px;"
			"}");
		titleLabel->setAlignment(Qt::AlignCenter);
		dialogLayout->addWidget(titleLabel);

		// Dynamic description based on content
		QString descText;
		if (hasMissing && hasUpdates) {
			descText = "Some required plugins are missing and others need updates to work correctly.";
		} else if (hasMissing) {
			descText = "The following plugins are required but not installed.";
		} else if (hasUpdates) {
			descText = "The following plugins have updates available.";
		}
		
		QLabel *descLabel = new QLabel(descText);
		descLabel->setStyleSheet(
			"QLabel {"
			"color: #9ca3af;"
			"font-size: 14px;"
			"margin: 0px 0px 10px 0px;"
			"padding: 0px;"
			"}");
		descLabel->setWordWrap(true);
		descLabel->setAlignment(Qt::AlignCenter);
		dialogLayout->addWidget(descLabel);

		// Scrollable content area
		QScrollArea *scrollArea = new QScrollArea();
		scrollArea->setWidgetResizable(true);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scrollArea->setStyleSheet(
			"QScrollArea {"
			"border: none;"
			"background: #13171f;"
			"}"
			"QScrollBar:vertical {"
			"background: #374151;"
			"width: 12px;"
			"border-radius: 6px;"
			"}"
			"QScrollBar::handle:vertical {"
			"background: #6b7280;"
			"border-radius: 6px;"
			"min-height: 20px;"
			"}"
			"QScrollBar::handle:vertical:hover {"
			"background: #9ca3af;"
			"}");

		QWidget *contentWidget = new QWidget();
		contentWidget->setStyleSheet("background: #13171f;");
		QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
		contentLayout->setContentsMargins(5, 5, 5, 5);
		contentLayout->setSpacing(10);

		if (hasMissing) {
			QGroupBox *missingGroup = new QGroupBox("Missing Plugins");
			missingGroup->setStyleSheet(
				"QGroupBox {"
				"color: white;"
				"font-size: 16px;"
				"font-weight: bold;"
				"border: 2px solid #ef4444;"
				"border-radius: 10px;"
				"margin-top: 14px;"
				"padding-top: 10px;"
				"background: #1f2937;"
				"}"
				"QGroupBox::title {"
				"subcontrol-origin: margin;"
				"left: 15px;"
				"padding: 0 8px 0 8px;"
				"color: #ef4444;"
				"}");
			
			QVBoxLayout *missingLayout = new QVBoxLayout(missingGroup);
			missingLayout->setContentsMargins(10, 10, 10, 10);
			
			QLabel *missingLabel = new QLabel(QString::fromStdString(errorMsgMissing));
			missingLabel->setTextFormat(Qt::RichText);
			missingLabel->setWordWrap(true);
			missingLabel->setOpenExternalLinks(true);
			missingLabel->setStyleSheet(
				"QLabel {"
				"color: #f3f4f6;"
				"font-size: 13px;"
				"background: transparent;"
				"border: none;"
				"}");
			missingLayout->addWidget(missingLabel);
			contentLayout->addWidget(missingGroup);
		}

		if (hasUpdates) {
			QGroupBox *updateGroup = new QGroupBox("Plugin Updates Available");
			updateGroup->setStyleSheet(
				"QGroupBox {"
				"color: white;"
				"font-size: 16px;"
				"font-weight: bold;"
				"border: 2px solid #f59e0b;"
				"border-radius: 10px;"
				"margin-top: 14px;"
				"padding-top: 10px;"
				"background: #1f2937;"
				"}"
				"QGroupBox::title {"
				"subcontrol-origin: margin;"
				"left: 15px;"
				"padding: 0 8px 0 8px;"
				"color: #f59e0b;"
				"}");
			
			QVBoxLayout *updateLayout = new QVBoxLayout(updateGroup);
			updateLayout->setContentsMargins(10, 10, 10, 10);
			
			QLabel *updateLabel = new QLabel(QString::fromStdString(errorMsgUpdate));
			updateLabel->setTextFormat(Qt::RichText);
			updateLabel->setWordWrap(true);
			updateLabel->setOpenExternalLinks(true);
			updateLabel->setStyleSheet(
				"QLabel {"
				"color: #f3f4f6;"
				"font-size: 13px;"
				"background: transparent;"
				"border: none;"
				"}");
			updateLayout->addWidget(updateLabel);
			contentLayout->addWidget(updateGroup);
		}

		scrollArea->setWidget(contentWidget);
		dialogLayout->addWidget(scrollArea);

		// Button section
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->addStretch();
		
		QPushButton *okButton = new QPushButton(obs_module_text("OK"));
		okButton->setStyleSheet(
			"QPushButton {"
			"background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);"
			"color: white;"
			"border: none;"
			"padding: 12px 24px;"
			"font-size: 14px;"
			"font-weight: bold;"
			"border-radius: 6px;"
			"min-width: 100px;"
			"}"
			"QPushButton:hover {"
			"background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);"
			"}");
		QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		buttonLayout->addWidget(okButton);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		
		// Set initial size and show
		dialog->show();
		
		// Calculate proper size after layout is complete
		QTimer::singleShot(10, [dialog, dialogLayout, maxHeight, minHeight, maxWidth, minWidth]() {
			// Force layout calculation
			dialog->adjustSize();
			QSize sizeHint = dialog->sizeHint();
			
			// Calculate appropriate size based on content
			int width = qMax(minWidth, qMin(sizeHint.width() + 20, maxWidth));
			int height = qMax(minHeight, qMin(sizeHint.height() + 10, maxHeight));
			
			dialog->resize(width, height);
			dialog->setMaximumSize(maxWidth, maxHeight);
			dialog->setMinimumSize(minWidth, minHeight);
		});
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
		errorMsgUpdate += "<table width=\"100%\" border=\"1\" cellpadding=\"8\" cellspacing=\"0\" style=\"border-collapse: collapse; background: #2d3748; color: white; border-radius: 8px; overflow: hidden;\">"
				  "<tr style=\"background: #4a5568; font-weight: bold;\">"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">Plugin Name</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">Installed Version</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">Current Version</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">Download Link</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">Website Link</td>"
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

			errorMsgUpdate += "<tr>"
					  "<td style=\"padding: 8px; border: 1px solid #718096; font-weight: bold;\">" + module.first + "</td>"
					  "<td style=\"padding: 8px; border: 1px solid #718096; color: #fc8181;\">v" + module.second + "</td>"
					  "<td style=\"padding: 8px; border: 1px solid #718096; color: #68d391;\">v" + required_version + "</td>"
					  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + direct_download_link + "\" style=\"color: #60a5fa;\">Download</a></td>"
					  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + forum_link + "\" style=\"color: #60a5fa;\">Website</a></td>"
					  "</tr>";
		}

		errorMsgUpdate += "</table>";
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
		StreamUP::ErrorHandler::LogWarning("Failed to fetch plugins from API: " + url, StreamUP::ErrorHandler::Category::Network);
		return;
	}

	if (api_response.find("Error:") != std::string::npos) {
		StreamUP::ErrorHandler::ShowErrorDialog("API Error", api_response);
		return;
	}

	if (api_response.empty()) {
		StreamUP::ErrorHandler::LogError("Empty response from plugins API: " + url, StreamUP::ErrorHandler::Category::Network);
		StreamUP::ErrorHandler::ShowErrorDialog("Plugin Load Error", obs_module_text("WindowErrorLoadIssue"));
		return;
	}

	auto data = OBSWrappers::MakeOBSDataFromJson(api_response.c_str());
	auto plugins = OBSWrappers::GetArrayProperty(data.get(), "plugins");

	size_t count = obs_data_array_count(plugins.get());
	for (size_t i = 0; i < count; ++i) {
		auto plugin = OBSWrappers::OBSDataPtr(obs_data_array_item(plugins.get(), i));

		std::string name = OBSWrappers::GetStringProperty(plugin.get(), "name");

		PluginInfo info;
		info.version = OBSWrappers::GetStringProperty(plugin.get(), "version");
		auto downloads = OBSWrappers::GetObjectProperty(plugin.get(), "downloads");
		if (downloads) {
			info.windowsURL = OBSWrappers::GetStringProperty(downloads.get(), "windows");
			info.macURL = OBSWrappers::GetStringProperty(downloads.get(), "macOS");
			info.linuxURL = OBSWrappers::GetStringProperty(downloads.get(), "linux");
		}
		info.searchString = OBSWrappers::GetStringProperty(plugin.get(), "searchString");
		info.generalURL = OBSWrappers::GetStringProperty(plugin.get(), "url");
		info.moduleName = OBSWrappers::GetStringProperty(plugin.get(), "moduleName");

		StreamUP::PluginState::Instance().AddPlugin(name, info);

		if (OBSWrappers::GetBoolProperty(plugin.get(), "required")) {
			StreamUP::PluginState::Instance().AddRequiredPlugin(name, info);
		}
	}
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
			errorMsgUpdate += "<table width=\"100%\" border=\"1\" cellpadding=\"8\" cellspacing=\"0\" style=\"border-collapse: collapse; background: #2d3748; color: white; border-radius: 8px; overflow: hidden;\">"
					  "<tr style=\"background: #4a5568; font-weight: bold;\">"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">Plugin Name</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">Installed Version</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">Current Version</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">Download Link</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">Website Link</td>"
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

				errorMsgUpdate += "<tr>"
						  "<td style=\"padding: 8px; border: 1px solid #718096; font-weight: bold;\">" + module.first + "</td>"
						  "<td style=\"padding: 8px; border: 1px solid #718096; color: #fc8181;\">v" + module.second + "</td>"
						  "<td style=\"padding: 8px; border: 1px solid #718096; color: #68d391;\">v" + required_version + "</td>"
						  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + direct_download_link + "\" style=\"color: #60a5fa;\">Download</a></td>"
						  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + forum_link + "\" style=\"color: #60a5fa;\">Website</a></td>"
						  "</tr>";
			}

			errorMsgUpdate += "</table>";

			if (!errorMsgUpdate.empty() && errorMsgUpdate.substr(errorMsgUpdate.length() - 4) == "<br>") {
				errorMsgUpdate = errorMsgUpdate.substr(0, errorMsgUpdate.length() - 4);
			}
		}

		if (hasMissingPlugins) {
			errorMsgMissing += "<table width=\"100%\" border=\"1\" cellpadding=\"8\" cellspacing=\"0\" style=\"border-collapse: collapse; background: #2d3748; color: white; border-radius: 8px; overflow: hidden;\">"
					   "<tr style=\"background: #4a5568; font-weight: bold;\">"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">Plugin Name</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">Status</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">Current Version</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">Download Link</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">Website Link</td>"
					   "</tr>";

			for (auto it = missing_modules.begin(); it != missing_modules.end(); ++it) {
				const std::string &moduleName = it->first;
				const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
				const StreamUP::PluginInfo &pluginInfo = requiredPlugins.at(moduleName);
				const std::string &forum_link = pluginInfo.generalURL;
				const std::string &required_version = pluginInfo.version;
				const std::string &direct_download_link = StringUtils::GetPlatformURL(
					QString::fromStdString(pluginInfo.windowsURL),
					QString::fromStdString(pluginInfo.macURL),
					QString::fromStdString(pluginInfo.linuxURL),
					QString::fromStdString(pluginInfo.generalURL)).toStdString();

				errorMsgMissing += "<tr>"
						   "<td style=\"padding: 8px; border: 1px solid #718096; font-weight: bold;\">" + moduleName + "</td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096; color: #fc8181;\">MISSING</td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096; color: #68d391;\">v" + required_version + "</td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + direct_download_link + "\" style=\"color: #60a5fa;\">Download</a></td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + forum_link + "\" style=\"color: #60a5fa;\">Website</a></td>"
						   "</tr>";
			}

			errorMsgMissing += "</table>";

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
