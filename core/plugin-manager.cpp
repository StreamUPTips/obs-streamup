#include "plugin-manager.hpp"
#include "streamup-common.hpp"
#include "plugin-state.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "path-utils.hpp"
#include "http-client.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
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
#include <QSizePolicy>
#include <fstream>
#include <iostream>
#include <regex>
#include <functional>
#include <util/platform.h>
#include <obs-frontend-api.h>

// UI functions now accessed through StreamUP::UIHelpers namespace
extern char *GetFilePath();

namespace StreamUP {
namespace PluginManager {

//-------------------ERROR HANDLING FUNCTIONS-------------------
void ErrorDialog(const QString &errorMessage)
{
	std::string message = errorMessage.isEmpty() ? obs_module_text("UnknownErrorOccurred") : errorMessage.toStdString();
	StreamUP::ErrorHandler::ShowErrorDialog("Plugin Error", message);
}

void PluginsUpToDateOutput(bool manuallyTriggered)
{
	if (manuallyTriggered) {
		StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
			QDialog *toast = new QDialog();
			toast->setWindowTitle(obs_module_text("StreamUP"));
			toast->setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
			toast->setAttribute(Qt::WA_DeleteOnClose);
			toast->setStyleSheet(QString("QDialog { background: %1; border-radius: %2px; }")
				.arg(StreamUP::UIStyles::Colors::SUCCESS)
				.arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
			toast->resize(400, 100);
			toast->setFixedSize(400, 100);
			
			QVBoxLayout *toastLayout = new QVBoxLayout(toast);
			toastLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
				StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
				StreamUP::UIStyles::Sizes::PADDING_XL, 
				StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
			toastLayout->setSpacing(8);
			
			QLabel *messageLabel = new QLabel(obs_module_text("AllPluginsUpToDate"));
			messageLabel->setStyleSheet(QString(
				"QLabel {"
				"color: white;"
				"font-size: %1px;"
				"font-weight: bold;"
				"background: transparent;"
				"border: none;"
				"}").arg(StreamUP::UIStyles::Sizes::FONT_SIZE_MEDIUM));
			messageLabel->setAlignment(Qt::AlignCenter);
			toastLayout->addWidget(messageLabel);
			
			QLabel *countdownLabel = new QLabel(obs_module_text("AutoClosingIn3Seconds"));
			countdownLabel->setStyleSheet(QString(
				"QLabel {"
				"color: rgba(255, 255, 255, 0.8);"
				"font-size: %1px;"
				"background: transparent;"
				"border: none;"
				"}").arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY));
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
					countdownLabel->setText(QString(obs_module_text("AutoClosingInNSeconds")).arg(*remainingSeconds));
				} else {
					countdownTimer->stop();
					toast->close();
				}
			});
			
			countdownTimer->start(1000); // Update every second
		});
	}
}

void PluginsHaveIssue(std::string errorMsgMissing, std::string errorMsgUpdate, std::function<void()> continueCallback)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([errorMsgMissing, errorMsgUpdate, continueCallback]() {
		// Create styled dialog with dynamic title
		bool hasMissing = (errorMsgMissing != "NULL");
		bool hasUpdates = (!errorMsgUpdate.empty());
		
		QString titleText;
		if (hasMissing && hasUpdates) {
			titleText = obs_module_text("MissingPluginsAndUpdatesAvailable");
		} else if (hasMissing) {
			titleText = obs_module_text("MissingRequiredPlugins");
		} else if (hasUpdates) {
			titleText = obs_module_text("PluginUpdatesAvailable");
		}

		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);
		
		// Set larger minimum size to accommodate expanded boxes
		dialog->setMinimumSize(600, 400);
		dialog->resize(700, 500);
		
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(0, 0, 0, 0);
		dialogLayout->setSpacing(0);

		// Header section (same style as WebSocket Commands)
		QWidget* headerWidget = new QWidget();
		headerWidget->setObjectName("headerWidget");
		headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px %4px %3px; }")
			.arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL + StreamUP::UIStyles::Sizes::PADDING_MEDIUM) // More padding at top
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL)
			.arg(StreamUP::UIStyles::Sizes::PADDING_XL)); // Standard padding at bottom
		
		QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
		headerLayout->setContentsMargins(0, 0, 0, 0);
		
		QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(titleText);
		titleLabel->setAlignment(Qt::AlignCenter);
		headerLayout->addWidget(titleLabel);
		
		// Add reduced spacing between title and description
		headerLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_SMALL);
		
		QString descText;
		if (hasMissing && hasUpdates) {
			descText = obs_module_text("SomePluginsMissingAndNeedUpdates");
		} else if (hasMissing) {
			descText = obs_module_text("FollowingPluginsRequiredNotInstalled");
		} else if (hasUpdates) {
			descText = obs_module_text("FollowingPluginsHaveUpdatesAvailable");
		}
		
		QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(descText);
		headerLayout->addWidget(subtitleLabel);
		
		dialogLayout->addWidget(headerWidget);


		// Content area - direct layout without main scrolling
		QVBoxLayout *contentLayout = new QVBoxLayout();
		contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
			StreamUP::UIStyles::Sizes::PADDING_XL, 
			StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
			StreamUP::UIStyles::Sizes::PADDING_XL);
		contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
		
		dialogLayout->addLayout(contentLayout);

		if (hasMissing) {
			// Create expandable GroupBox with internal scrolling
			QGroupBox *missingGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowPluginErrorMissingGroup"), "error");
			missingGroup->setMinimumWidth(500);
			missingGroup->setMinimumHeight(150);
			// Let it expand to fill available space
			missingGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			
			QVBoxLayout *missingLayout = new QVBoxLayout(missingGroup);
			missingLayout->setContentsMargins(8, 20, 8, 8);
			
			// ScrollArea goes INSIDE the GroupBox
			QScrollArea *missingScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
			QLabel *missingLabel = StreamUP::UIStyles::CreateStyledContent(QString::fromStdString(errorMsgMissing));
			missingScrollArea->setWidget(missingLabel);
			missingScrollArea->setStyleSheet(missingScrollArea->styleSheet() + 
				"QScrollArea { background: transparent; border: none; }");
			
			missingLayout->addWidget(missingScrollArea);
			contentLayout->addWidget(missingGroup);
		}

		if (hasUpdates) {
			// Create expandable GroupBox with internal scrolling
			QGroupBox *updateGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("UpdatesAvailable"), "warning");
			updateGroup->setMinimumWidth(500);
			updateGroup->setMinimumHeight(150);
			// Let it expand to fill available space
			updateGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			
			QVBoxLayout *updateLayout = new QVBoxLayout(updateGroup);
			updateLayout->setContentsMargins(8, 20, 8, 8);
			
			// ScrollArea goes INSIDE the GroupBox
			QScrollArea *updateScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
			QLabel *updateLabel = StreamUP::UIStyles::CreateStyledContent(QString::fromStdString(errorMsgUpdate));
			updateScrollArea->setWidget(updateLabel);
			updateScrollArea->setStyleSheet(updateScrollArea->styleSheet() + 
				"QScrollArea { background: transparent; border: none; }");
			
			updateLayout->addWidget(updateScrollArea);
			contentLayout->addWidget(updateGroup);
		}

		// Content is now directly in the dialog layout

		// Add warning message above buttons if there's a continue callback (meaning this is for install product)
		if (continueCallback) {
			// Add spacing before warning
			dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
			
			QLabel *warningLabel = new QLabel("⚠️ " + QString(obs_module_text("WarningContinueAnyway")));
			warningLabel->setWordWrap(true);
			warningLabel->setStyleSheet(QString(
				"QLabel {"
				"background: rgba(45, 55, 72, 0.8);"
				"color: #fbbf24;"
				"border: 1px solid #f59e0b;"
				"border-radius: %1px;"
				"padding: %2px;"
				"margin: %3px %4px;"
				"font-size: %5px;"
				"line-height: 1.4;"
				"}")
				.arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS)
				.arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
				.arg(StreamUP::UIStyles::Sizes::SPACING_SMALL)
				.arg(StreamUP::UIStyles::Sizes::PADDING_XL + 5)
				.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
			dialogLayout->addWidget(warningLabel);
		}

		// Add styled buttons
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
			StreamUP::UIStyles::Sizes::SPACING_MEDIUM, 
			StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
			StreamUP::UIStyles::Sizes::PADDING_XL);
		buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		buttonLayout->addStretch();
		
		// Add Continue Anyway button if callback is provided
		if (continueCallback) {
			QPushButton *continueButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("ContinueAnyway"), "warning");
			QObject::connect(continueButton, &QPushButton::clicked, [dialog, continueCallback]() {
				dialog->close();
				continueCallback();
			});
			buttonLayout->addWidget(continueButton);
		}
		
		QPushButton *okButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("OK"), "neutral", 30, 100);
		QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		buttonLayout->addWidget(okButton);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		
		// Apply auto-sizing
		StreamUP::UIStyles::ApplyAutoSizing(dialog);
	});
}


//-------------------PLUGIN UPDATE FUNCTIONS-------------------
void CheckAllPluginsForUpdates(bool manuallyTriggered)
{
	StreamUP::ErrorHandler::LogInfo("Starting plugin update check (manually triggered: " + std::string(manuallyTriggered ? "true" : "false") + ")", StreamUP::ErrorHandler::Category::Plugin);
	
	const auto& allPlugins = StreamUP::GetAllPlugins();
	StreamUP::ErrorHandler::LogInfo("Loaded " + std::to_string(allPlugins.size()) + " plugins from registry", StreamUP::ErrorHandler::Category::Plugin);
	
	if (allPlugins.empty()) {
		StreamUP::ErrorHandler::LogError("No plugins loaded from registry - this should not happen", StreamUP::ErrorHandler::Category::Plugin);
		ErrorDialog(obs_module_text("WindowErrorLoadIssue"));
		return;
	}

	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgUpdate = "";
	std::vector<std::pair<std::string, std::string>> installedPlugins = GetInstalledPlugins();
	StreamUP::ErrorHandler::LogInfo("Found " + std::to_string(installedPlugins.size()) + " installed plugins", StreamUP::ErrorHandler::Category::Plugin);

	for (const auto &plugin : installedPlugins) {
		const std::string &plugin_name = plugin.first;
		const std::string &installed_version = plugin.second;
		StreamUP::ErrorHandler::LogInfo("Checking plugin: " + plugin_name + " (installed: v" + installed_version + ")", StreamUP::ErrorHandler::Category::Plugin);
		
		const auto& allPlugins = StreamUP::GetAllPlugins();
		auto plugin_it = allPlugins.find(plugin_name);
		if (plugin_it == allPlugins.end()) {
			StreamUP::ErrorHandler::LogWarning("Plugin \"" + plugin_name + "\" not found in registry - skipping update check", StreamUP::ErrorHandler::Category::Plugin);
			continue;
		}
		
		const StreamUP::PluginInfo &plugin_info = plugin_it->second;
		const std::string &required_version = plugin_info.version;
		StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" registry version: v" + required_version, StreamUP::ErrorHandler::Category::Plugin);

		if (installed_version != required_version) {
			StreamUP::ErrorHandler::LogInfo("Version mismatch detected for \"" + plugin_name + "\" - checking if update needed", StreamUP::ErrorHandler::Category::Plugin);
			if (VersionUtils::IsVersionLessThan(installed_version, required_version)) {
				StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" needs update: v" + installed_version + " -> v" + required_version, StreamUP::ErrorHandler::Category::Plugin);
				version_mismatch_modules.emplace(plugin_name, installed_version);
			} else {
				StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" is newer than registry version (v" + installed_version + " > v" + required_version + ")", StreamUP::ErrorHandler::Category::Plugin);
			}
		} else {
			StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" is up to date (v" + installed_version + ")", StreamUP::ErrorHandler::Category::Plugin);
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

	StreamUP::ErrorHandler::LogInfo("Update check complete - found " + std::to_string(version_mismatch_modules.size()) + " plugins needing updates", StreamUP::ErrorHandler::Category::Plugin);
	
	if (!version_mismatch_modules.empty()) {
		errorMsgUpdate += "<table width=\"100%\" border=\"1\" cellpadding=\"8\" cellspacing=\"0\" style=\"border-collapse: collapse; background: #2d3748; color: white; border-radius: 8px; overflow: hidden;\">"
				  "<tr style=\"background: #4a5568; font-weight: bold;\">"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("PluginName")) + "</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("InstalledVersion")) + "</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("CurrentVersion")) + "</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("DownloadLink")) + "</td>"
				  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("WebsiteLink")) + "</td>"
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
					  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + direct_download_link + "\" style=\"color: #60a5fa;\">" + std::string(obs_module_text("Download")) + "</a></td>"
					  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + forum_link + "\" style=\"color: #60a5fa;\">" + std::string(obs_module_text("Website")) + "</a></td>"
					  "</tr>";
		}

		errorMsgUpdate += "</table>";
		PluginsHaveIssue("NULL", errorMsgUpdate);
		version_mismatch_modules.clear();
	} else {
		StreamUP::ErrorHandler::LogInfo("All plugins are up to date", StreamUP::ErrorHandler::Category::Plugin);
		PluginsUpToDateOutput(manuallyTriggered);
	}
}

// HTTP functionality moved to StreamUP::HttpClient module

void InitialiseRequiredModules()
{
	StreamUP::ErrorHandler::LogInfo("Starting plugin registry initialization", StreamUP::ErrorHandler::Category::Plugin);
	
	std::string api_response;
	const std::string url = "https://api.streamup.tips/plugins";
	
	StreamUP::ErrorHandler::LogInfo("Fetching plugins from API: " + url, StreamUP::ErrorHandler::Category::Network);
	
	if (!StreamUP::HttpClient::MakeGetRequest(url, api_response)) {
		StreamUP::ErrorHandler::LogWarning("Failed to fetch plugins from API: " + url, StreamUP::ErrorHandler::Category::Network);
		return;
	}

	if (api_response.find("Error:") != std::string::npos) {
		StreamUP::ErrorHandler::LogError("API returned error: " + api_response, StreamUP::ErrorHandler::Category::Network);
		StreamUP::ErrorHandler::ShowErrorDialog("API Error", api_response);
		return;
	}

	if (api_response.empty()) {
		StreamUP::ErrorHandler::LogError("Empty response from plugins API: " + url, StreamUP::ErrorHandler::Category::Network);
		StreamUP::ErrorHandler::ShowErrorDialog("Plugin Load Error", obs_module_text("WindowErrorLoadIssue"));
		return;
	}
	
	StreamUP::ErrorHandler::LogInfo("Successfully received API response, parsing JSON...", StreamUP::ErrorHandler::Category::Network);

	auto data = OBSWrappers::MakeOBSDataFromJson(api_response.c_str());
	auto plugins = OBSWrappers::GetArrayProperty(data.get(), "plugins");

	size_t count = obs_data_array_count(plugins.get());
	StreamUP::ErrorHandler::LogInfo("Found " + std::to_string(count) + " plugins in API response", StreamUP::ErrorHandler::Category::Plugin);
	
	int required_count = 0;
	
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
		info.required = OBSWrappers::GetBoolProperty(plugin.get(), "required");

		StreamUP::ErrorHandler::LogInfo("Loading plugin: \"" + name + "\" (version: v" + info.version + ", required: " + (info.required ? "true" : "false") + ")", StreamUP::ErrorHandler::Category::Plugin);
		StreamUP::ErrorHandler::LogInfo("Search string for \"" + name + "\": \"" + info.searchString + "\"", StreamUP::ErrorHandler::Category::Plugin);

		StreamUP::PluginState::Instance().AddPlugin(name, info);

		if (info.required) {
			StreamUP::PluginState::Instance().AddRequiredPlugin(name, info);
			required_count++;
		}
	}
	
	StreamUP::ErrorHandler::LogInfo("Plugin registry initialization complete: " + std::to_string(count) + " total plugins, " + std::to_string(required_count) + " required plugins", StreamUP::ErrorHandler::Category::Plugin);
}

bool CheckrequiredOBSPluginsWithoutUI(bool isLoadStreamUpFile)
{
	StreamUP::ErrorHandler::LogInfo("Starting required plugins check without UI (isLoadStreamUpFile: " + std::string(isLoadStreamUpFile ? "true" : "false") + ")", StreamUP::ErrorHandler::Category::Plugin);
	
	const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
	StreamUP::ErrorHandler::LogInfo("Loaded " + std::to_string(requiredPlugins.size()) + " required plugins from registry", StreamUP::ErrorHandler::Category::Plugin);
	
	if (requiredPlugins.empty()) {
		StreamUP::ErrorHandler::LogError("Required plugins list is empty", StreamUP::ErrorHandler::Category::Plugin);
		return false;
	}

	std::map<std::string, std::string> missing_modules;
	std::map<std::string, std::string> version_mismatch_modules;
	char *filepath = GetFilePath();
	StreamUP::ErrorHandler::LogInfo("OBS log file path: " + std::string(filepath ? filepath : "NULL"), StreamUP::ErrorHandler::Category::Plugin);
	
	if (filepath == NULL) {
		StreamUP::ErrorHandler::LogError("Failed to get OBS log file path", StreamUP::ErrorHandler::Category::Plugin);
		return false;
	}

	for (const auto &module : requiredPlugins) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &required_version = plugin_info.version;
		const std::string &search_string = plugin_info.searchString;

		StreamUP::ErrorHandler::LogInfo("Checking required plugin: \"" + plugin_name + "\" (required: v" + required_version + ")", StreamUP::ErrorHandler::Category::Plugin);
		StreamUP::ErrorHandler::LogInfo("Search string for \"" + plugin_name + "\": \"" + search_string + "\"", StreamUP::ErrorHandler::Category::Plugin);

		if (search_string.find("[ignore]") != std::string::npos) {
			StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" has [ignore] flag - skipping check", StreamUP::ErrorHandler::Category::Plugin);
			continue; // Skip to the next iteration
		}

		std::string installed_version = SearchStringInFileForVersion(filepath, search_string.c_str());
		StreamUP::ErrorHandler::LogInfo("Search result for \"" + plugin_name + "\": " + (installed_version.empty() ? "NOT FOUND" : "v" + installed_version), StreamUP::ErrorHandler::Category::Plugin);

		if (installed_version.empty() && plugin_info.required) {
			StreamUP::ErrorHandler::LogWarning("Required plugin \"" + plugin_name + "\" is MISSING", StreamUP::ErrorHandler::Category::Plugin);
			missing_modules.emplace(plugin_name, required_version);
		} else if (!installed_version.empty() && installed_version != required_version && plugin_info.required &&
			   VersionUtils::IsVersionLessThan(installed_version, required_version)) {
			StreamUP::ErrorHandler::LogWarning("Required plugin \"" + plugin_name + "\" needs UPDATE: v" + installed_version + " -> v" + required_version, StreamUP::ErrorHandler::Category::Plugin);
			version_mismatch_modules.emplace(plugin_name, installed_version);
		} else if (!installed_version.empty()) {
			StreamUP::ErrorHandler::LogInfo("Required plugin \"" + plugin_name + "\" is OK: v" + installed_version + " (required: v" + required_version + ")", StreamUP::ErrorHandler::Category::Plugin);
		}
	}

	bfree(filepath);

	bool hasUpdates = !version_mismatch_modules.empty();
	bool hasMissingPlugins = !missing_modules.empty();

	StreamUP::ErrorHandler::LogInfo("Plugin check summary: " + std::to_string(missing_modules.size()) + " missing, " + std::to_string(version_mismatch_modules.size()) + " need updates", StreamUP::ErrorHandler::Category::Plugin);

	if (hasUpdates || hasMissingPlugins) {
		// Log the issues without showing UI
		if (hasMissingPlugins) {
			StreamUP::ErrorHandler::LogError("MISSING REQUIRED PLUGINS:", StreamUP::ErrorHandler::Category::Plugin);
			for (const auto &module : missing_modules) {
				StreamUP::ErrorHandler::LogError("  - " + module.first + " (required version: v" + module.second + ")", 
					StreamUP::ErrorHandler::Category::Plugin);
			}
		}
		if (hasUpdates) {
			StreamUP::ErrorHandler::LogError("PLUGINS NEED UPDATES:", StreamUP::ErrorHandler::Category::Plugin);
			for (const auto &module : version_mismatch_modules) {
				StreamUP::ErrorHandler::LogError("  - " + module.first + " (installed: v" + module.second + ")", 
					StreamUP::ErrorHandler::Category::Plugin);
			}
		}
		
		missing_modules.clear();
		version_mismatch_modules.clear();
		StreamUP::ErrorHandler::LogError("Required plugin check FAILED - returning false", StreamUP::ErrorHandler::Category::Plugin);
		return false;
	} else {
		// Log success without showing UI
		StreamUP::ErrorHandler::LogInfo("All required plugins are up to date - returning true", StreamUP::ErrorHandler::Category::Plugin);
		return true;
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
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("PluginName")) + "</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("InstalledVersion")) + "</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("CurrentVersion")) + "</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("DownloadLink")) + "</td>"
					  "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("WebsiteLink")) + "</td>"
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
						  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + direct_download_link + "\" style=\"color: #60a5fa;\">" + std::string(obs_module_text("Download")) + "</a></td>"
						  "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + forum_link + "\" style=\"color: #60a5fa;\">" + std::string(obs_module_text("Website")) + "</a></td>"
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
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("PluginName")) + "</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("Status")) + "</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("CurrentVersion")) + "</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("DownloadLink")) + "</td>"
					   "<td style=\"padding: 10px; border: 1px solid #718096;\">" + std::string(obs_module_text("WebsiteLink")) + "</td>"
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
						   "<td style=\"padding: 8px; border: 1px solid #718096; color: #fc8181;\">" + std::string(obs_module_text("MISSING")) + "</td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096; color: #68d391;\">v" + required_version + "</td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + direct_download_link + "\" style=\"color: #60a5fa;\">" + std::string(obs_module_text("Download")) + "</a></td>"
						   "<td style=\"padding: 8px; border: 1px solid #718096;\"><a href=\"" + forum_link + "\" style=\"color: #60a5fa;\">" + std::string(obs_module_text("Website")) + "</a></td>"
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
	StreamUP::ErrorHandler::LogInfo("Searching for plugin version in file", StreamUP::ErrorHandler::Category::Plugin);
	StreamUP::ErrorHandler::LogInfo("Search path: " + std::string(path ? path : "NULL"), StreamUP::ErrorHandler::Category::Plugin);
	StreamUP::ErrorHandler::LogInfo("Search string: \"" + std::string(search ? search : "NULL") + "\"", StreamUP::ErrorHandler::Category::Plugin);
	
	QString filepath = PathUtils::GetMostRecentFile(QString::fromLocal8Bit(path));
	StreamUP::ErrorHandler::LogInfo("Most recent file found: " + filepath.toStdString(), StreamUP::ErrorHandler::Category::Plugin);
	
	if (filepath.isEmpty()) {
		StreamUP::ErrorHandler::LogWarning("No recent file found at path", StreamUP::ErrorHandler::Category::Plugin);
		return "";
	}
	
	FILE *file = fopen(filepath.toLocal8Bit().constData(), "r+");
	if (!file) {
		StreamUP::ErrorHandler::LogWarning("Failed to open file: " + filepath.toStdString(), StreamUP::ErrorHandler::Category::Plugin);
		return "";
	}
	
	char line[256];
	std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	std::regex version_regex_double("[0-9]+\\.[0-9]+");

	int line_count = 0;
	bool found_search_string = false;
	
	while (fgets(line, sizeof(line), file)) {
		line_count++;
		char *found_ptr = strstr(line, search);
		if (found_ptr) {
			found_search_string = true;
			StreamUP::ErrorHandler::LogInfo("Found search string at line " + std::to_string(line_count) + ": \"" + std::string(line).substr(0, 100) + "...\"", StreamUP::ErrorHandler::Category::Plugin);
			
			std::string remaining_line = std::string(found_ptr + strlen(search));
			StreamUP::ErrorHandler::LogInfo("Text after search string: \"" + remaining_line.substr(0, 50) + "...\"", StreamUP::ErrorHandler::Category::Plugin);
			
			std::smatch match;

			if (std::regex_search(remaining_line, match, version_regex_triple) && match.size() > 0) {
				std::string version = match.str(0);
				StreamUP::ErrorHandler::LogInfo("Found triple version number: v" + version, StreamUP::ErrorHandler::Category::Plugin);
				fclose(file);
				return version;
			}

			if (std::regex_search(remaining_line, match, version_regex_double) && match.size() > 0) {
				std::string version = match.str(0);
				StreamUP::ErrorHandler::LogInfo("Found double version number: v" + version, StreamUP::ErrorHandler::Category::Plugin);
				fclose(file);
				return version;
			}
			
			StreamUP::ErrorHandler::LogWarning("Found search string but no version number in remaining text", StreamUP::ErrorHandler::Category::Plugin);
		}
	}

	StreamUP::ErrorHandler::LogInfo("Finished scanning " + std::to_string(line_count) + " lines", StreamUP::ErrorHandler::Category::Plugin);
	if (!found_search_string) {
		StreamUP::ErrorHandler::LogWarning("Search string \"" + std::string(search) + "\" not found in file", StreamUP::ErrorHandler::Category::Plugin);
	}

	fclose(file);
	return "";
}

std::vector<std::pair<std::string, std::string>> GetInstalledPlugins()
{
	StreamUP::ErrorHandler::LogInfo("Starting GetInstalledPlugins scan", StreamUP::ErrorHandler::Category::Plugin);
	
	std::vector<std::pair<std::string, std::string>> installedPlugins;
	char *filepath = GetFilePath();
	StreamUP::ErrorHandler::LogInfo("OBS log file path: " + std::string(filepath ? filepath : "NULL"), StreamUP::ErrorHandler::Category::Plugin);
	
	if (filepath == NULL) {
		StreamUP::ErrorHandler::LogError("Failed to get OBS log file path for plugin scan", StreamUP::ErrorHandler::Category::Plugin);
		return installedPlugins;
	}

	const auto& allPlugins = StreamUP::GetAllPlugins();
	StreamUP::ErrorHandler::LogInfo("Scanning " + std::to_string(allPlugins.size()) + " plugins for installation status", StreamUP::ErrorHandler::Category::Plugin);
	
	for (const auto &module : allPlugins) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &search_string = plugin_info.searchString;

		StreamUP::ErrorHandler::LogInfo("Scanning for plugin: \"" + plugin_name + "\"", StreamUP::ErrorHandler::Category::Plugin);

		std::string installed_version = SearchStringInFileForVersion(filepath, search_string.c_str());

		if (!installed_version.empty()) {
			StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" FOUND - v" + installed_version, StreamUP::ErrorHandler::Category::Plugin);
			installedPlugins.emplace_back(plugin_name, installed_version);
		} else {
			StreamUP::ErrorHandler::LogInfo("Plugin \"" + plugin_name + "\" NOT FOUND", StreamUP::ErrorHandler::Category::Plugin);
		}
	}

	bfree(filepath);

	StreamUP::ErrorHandler::LogInfo("GetInstalledPlugins complete - found " + std::to_string(installedPlugins.size()) + " installed plugins", StreamUP::ErrorHandler::Category::Plugin);
	return installedPlugins;
}

} // namespace PluginManager
} // namespace StreamUP
