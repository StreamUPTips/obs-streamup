#include "plugin-manager.hpp"
#include "../utilities/debug-logger.hpp"
#include "streamup-common.hpp"
#include "plugin-state.hpp"
#include "string-utils.hpp"
#include "version-utils.hpp"
#include "path-utils.hpp"
#include "http-client.hpp"
#include "../ui/ui-helpers.hpp"
#include "../ui/ui-styles.hpp"
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
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QUrl>
#include <QGroupBox>
#include <QScrollArea>
#include <QPushButton>
#include <QObject>
#include <QTimer>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <fstream>
#include <iostream>
#include <regex>
#include <functional>
#include <algorithm>
#include <unordered_set>
#include <cctype>
#include <util/platform.h>
#include <obs-frontend-api.h>

#ifdef _WIN32
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

// UI functions now accessed through StreamUP::UIHelpers namespace

namespace StreamUP {
namespace PluginManager {

//-------------------TABLE WIDGET HELPERS-------------------
QString ExtractDomainFromUrl(const QString& url) {
	QUrl qurl(url);
	QString host = qurl.host();
	
	// Remove www. prefix if present
	if (host.startsWith("www.")) {
		host = host.mid(4);
	}
	
	// If no host found, try to extract manually
	if (host.isEmpty()) {
		QString cleanUrl = url;
		if (cleanUrl.contains("://")) {
			cleanUrl = cleanUrl.split("://").last();
		}
		if (cleanUrl.contains("/")) {
			cleanUrl = cleanUrl.split("/").first();
		}
		if (cleanUrl.startsWith("www.")) {
			cleanUrl = cleanUrl.mid(4);
		}
		return cleanUrl;
	}
	
	return host;
}
QTableWidget* CreateMissingPluginsTable(const std::map<std::string, std::string>& missing_modules) {
	QStringList headers = {
		obs_module_text("UI.Label.PluginName"),
		obs_module_text("UI.Label.Status"), 
		obs_module_text("UI.Label.CurrentVersion"),
		obs_module_text("UI.Label.DownloadLink"),
		obs_module_text("UI.Label.WebsiteLink")
	};
	
	QTableWidget* table = StreamUP::UIStyles::CreateStyledTable(headers);
	table->setRowCount(static_cast<int>(missing_modules.size()));
	
	int row = 0;
	const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
	
	for (const auto& module : missing_modules) {
		const std::string& moduleName = module.first;
		const StreamUP::PluginInfo& pluginInfo = requiredPlugins.at(moduleName);
		const std::string& required_version = pluginInfo.version;
		const std::string& forum_link = pluginInfo.generalURL;
		const std::string& direct_download_link = StringUtils::GetPlatformURL(
			QString::fromStdString(pluginInfo.windowsURL),
			QString::fromStdString(pluginInfo.macURL),
			QString::fromStdString(pluginInfo.linuxURL),
			QString::fromStdString(pluginInfo.generalURL)).toStdString();
		
		// Plugin Name column
		table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(moduleName)));
		
		// Status column - Missing
		QTableWidgetItem* statusItem = new QTableWidgetItem("❌ " + QString(obs_module_text("UI.Message.MISSING")));
		statusItem->setForeground(QColor("#ef4444"));
		table->setItem(row, 1, statusItem);
		
		// Version column
		QTableWidgetItem* versionItem = new QTableWidgetItem("v" + QString::fromStdString(required_version));
		versionItem->setForeground(QColor("#22c55e"));
		table->setItem(row, 2, versionItem);
		
		// Download Link column
		QTableWidgetItem* downloadItem = new QTableWidgetItem(obs_module_text("UI.Button.Download"));
		downloadItem->setForeground(QColor("#3b82f6"));
		downloadItem->setData(Qt::UserRole, QString::fromStdString(direct_download_link));
		table->setItem(row, 3, downloadItem);
		
		// Website Link column - show domain name
		QString domainName = ExtractDomainFromUrl(QString::fromStdString(forum_link));
		QTableWidgetItem* websiteItem = new QTableWidgetItem(domainName);
		websiteItem->setForeground(QColor("#3b82f6"));
		websiteItem->setData(Qt::UserRole, QString::fromStdString(forum_link));
		table->setItem(row, 4, websiteItem);
		
		row++;
	}
	
	StreamUP::UIStyles::AutoResizeTableColumns(table);
	return table;
}

QTableWidget* CreateUpdatesTable(const std::map<std::string, std::string>& version_mismatch_modules) {
	QStringList headers = {
		obs_module_text("UI.Label.PluginName"),
		obs_module_text("UI.Label.InstalledVersion"),
		obs_module_text("UI.Label.CurrentVersion"), 
		obs_module_text("UI.Label.DownloadLink"),
		obs_module_text("UI.Label.WebsiteLink")
	};
	
	QTableWidget* table = StreamUP::UIStyles::CreateStyledTable(headers);
	table->setRowCount(static_cast<int>(version_mismatch_modules.size()));
	
	int row = 0;
	const auto& allPlugins = StreamUP::GetAllPlugins();
	
	for (const auto& module : version_mismatch_modules) {
		const std::string& moduleName = module.first;
		const std::string& installed_version = module.second;
		const StreamUP::PluginInfo& plugin_info = allPlugins.at(moduleName);
		const std::string& required_version = plugin_info.version;
		const std::string& forum_link = plugin_info.generalURL;
		const std::string& direct_download_link = StringUtils::GetPlatformURL(
			QString::fromStdString(plugin_info.windowsURL),
			QString::fromStdString(plugin_info.macURL),
			QString::fromStdString(plugin_info.linuxURL),
			QString::fromStdString(plugin_info.generalURL)).toStdString();
		
		// Plugin Name column
		table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(moduleName)));
		
		// Installed Version column
		QTableWidgetItem* installedItem = new QTableWidgetItem("v" + QString::fromStdString(installed_version));
		installedItem->setForeground(QColor("#ef4444"));
		table->setItem(row, 1, installedItem);
		
		// Current Version column
		QTableWidgetItem* currentItem = new QTableWidgetItem("v" + QString::fromStdString(required_version));
		currentItem->setForeground(QColor("#22c55e"));
		table->setItem(row, 2, currentItem);
		
		// Download Link column
		QTableWidgetItem* downloadItem = new QTableWidgetItem(obs_module_text("UI.Button.Download"));
		downloadItem->setForeground(QColor("#3b82f6"));
		downloadItem->setData(Qt::UserRole, QString::fromStdString(direct_download_link));
		table->setItem(row, 3, downloadItem);
		
		// Website Link column - show domain name
		QString domainName = ExtractDomainFromUrl(QString::fromStdString(forum_link));
		QTableWidgetItem* websiteItem = new QTableWidgetItem(domainName);
		websiteItem->setForeground(QColor("#3b82f6"));
		websiteItem->setData(Qt::UserRole, QString::fromStdString(forum_link));
		table->setItem(row, 4, websiteItem);
		
		row++;
	}
	
	StreamUP::UIStyles::AutoResizeTableColumns(table);
	return table;
}

//-------------------ERROR HANDLING FUNCTIONS-------------------
void ErrorDialog(const QString &errorMessage)
{
	std::string message = errorMessage.isEmpty() ? obs_module_text("UI.Message.UnknownError") : errorMessage.toStdString();
	StreamUP::ErrorHandler::ShowErrorDialog("Plugin Error", message);
}

void PluginsUpToDateOutput(bool manuallyTriggered)
{
	if (manuallyTriggered) {
		StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
			QDialog *toast = new QDialog();
			toast->setWindowTitle(obs_module_text("App.Name"));
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
			
			QLabel *messageLabel = new QLabel(obs_module_text("Plugin.Status.AllUpToDate"));
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
			
			QLabel *countdownLabel = new QLabel(obs_module_text("Plugin.Message.AutoClosing3"));
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
					countdownLabel->setText(QString(obs_module_text("Plugin.Message.AutoClosingN")).arg(*remainingSeconds));
				} else {
					countdownTimer->stop();
					toast->close();
				}
			});
			
			countdownTimer->start(1000); // Update every second
		});
	}
}

void PluginsHaveIssue(const std::map<std::string, std::string>& missing_modules, const std::map<std::string, std::string>& version_mismatch_modules, std::function<void()> continueCallback)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([missing_modules, version_mismatch_modules, continueCallback]() {
		// Create styled dialog with dynamic title
		bool hasMissing = !missing_modules.empty();
		bool hasUpdates = !version_mismatch_modules.empty();
		
		QString titleText;
		if (hasMissing && hasUpdates) {
			titleText = obs_module_text("Plugin.Status.MissingAndUpdatesAvailable");
		} else if (hasMissing) {
			titleText = obs_module_text("Plugin.Status.MissingRequired");
		} else if (hasUpdates) {
			titleText = obs_module_text("Plugin.Status.UpdatesAvailable");
		}

		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleText);
		
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
			descText = obs_module_text("Plugin.Status.SomeMissingAndNeedUpdates");
		} else if (hasMissing) {
			descText = obs_module_text("Plugin.Message.RequiredNotInstalled");
		} else if (hasUpdates) {
			descText = obs_module_text("Plugin.Message.UpdatesAvailable");
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
			// Create expandable GroupBox with table
			QGroupBox *missingGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("Plugin.Dialog.MissingGroup"), "error");
			missingGroup->setMinimumWidth(500);
			missingGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
			
			QVBoxLayout *missingLayout = new QVBoxLayout(missingGroup);
			missingLayout->setContentsMargins(8, 8, 8, 8); // Reduced top margin from 20 to 8
			missingLayout->setSpacing(0); // Remove spacing around table
			
			// Create modern table widget
			QTableWidget *missingTable = CreateMissingPluginsTable(missing_modules);
			
			// Dynamic height calculation: max 10 rows, auto-size otherwise
			int rowCount = missingTable->rowCount();
			int maxVisibleRows = std::min(rowCount, 10);
			int headerHeight = 35; // Standard header height
			int rowHeight = 30; // Standard row height for consistency
			int tableHeight = headerHeight + (rowHeight * maxVisibleRows) + 6; // +6 for borders
			
			missingTable->setFixedHeight(tableHeight);
			if (rowCount > 10) {
				missingTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			} else {
				missingTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			}
			
			// Remove table border and background to blend with group box
			missingTable->setStyleSheet(
				missingTable->styleSheet() +
				"QTableWidget { "
				"border: none; "
				"background: transparent; "
				"border-radius: 8px; "
				"} "
				"QTableWidget::item { "
				"border-bottom: 1px solid #374151; "
				"} "
				"QTableWidget::item:last { "
				"border-bottom: none; "
				"} "
				"QHeaderView::section:first { "
				"border-top-left-radius: 8px; "
				"} "
				"QHeaderView::section:last { "
				"border-top-right-radius: 8px; "
				"}"
			);
			
			// Connect click handler for website/download links
			QObject::connect(missingTable, &QTableWidget::cellClicked, 
							[missingTable](int row, int column) {
				StreamUP::UIStyles::HandleTableCellClick(missingTable, row, column);
			});
			
			missingLayout->addWidget(missingTable);
			contentLayout->addWidget(missingGroup);
		}

		if (hasUpdates) {
			// Create expandable GroupBox with table
			QGroupBox *updateGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("Plugin.Dialog.UpdateGroup"), "warning");
			updateGroup->setMinimumWidth(500);
			updateGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
			
			QVBoxLayout *updateLayout = new QVBoxLayout(updateGroup);
			updateLayout->setContentsMargins(8, 8, 8, 8); // Reduced top margin from 20 to 8
			updateLayout->setSpacing(0); // Remove spacing around table
			
			// Create modern table widget
			QTableWidget *updateTable = CreateUpdatesTable(version_mismatch_modules);
			
			// Dynamic height calculation: max 10 rows, auto-size otherwise
			int rowCount = updateTable->rowCount();
			int maxVisibleRows = std::min(rowCount, 10);
			int headerHeight = 35; // Standard header height
			int rowHeight = 30; // Standard row height for consistency
			int tableHeight = headerHeight + (rowHeight * maxVisibleRows) + 6; // +6 for borders
			
			updateTable->setFixedHeight(tableHeight);
			if (rowCount > 10) {
				updateTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			} else {
				updateTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			}
			
			// Remove table border and background to blend with group box
			updateTable->setStyleSheet(
				updateTable->styleSheet() +
				"QTableWidget { "
				"border: none; "
				"background: transparent; "
				"border-radius: 8px; "
				"} "
				"QTableWidget::item { "
				"border-bottom: 1px solid #374151; "
				"} "
				"QTableWidget::item:last { "
				"border-bottom: none; "
				"} "
				"QHeaderView::section:first { "
				"border-top-left-radius: 8px; "
				"} "
				"QHeaderView::section:last { "
				"border-top-right-radius: 8px; "
				"}"
			);
			
			// Connect click handler for website/download links
			QObject::connect(updateTable, &QTableWidget::cellClicked, 
							[updateTable](int row, int column) {
				StreamUP::UIStyles::HandleTableCellClick(updateTable, row, column);
			});
			
			updateLayout->addWidget(updateTable);
			contentLayout->addWidget(updateGroup);
		}

		// Content is now directly in the dialog layout

		// Add warning message above buttons if there's a continue callback (meaning this is for install product)
		if (continueCallback) {
			// Add spacing before warning
			dialogLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
			
			QLabel *warningLabel = new QLabel("⚠️ " + QString(obs_module_text("Plugin.Dialog.WarningContinue")));
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
			QPushButton *continueButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Message.ContinueAnyway"), "warning");
			QObject::connect(continueButton, &QPushButton::clicked, [dialog, continueCallback]() {
				dialog->close();
				continueCallback();
			});
			buttonLayout->addWidget(continueButton);
		}
		
		QPushButton *okButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.OK"), "neutral", 30, 100);
		QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		buttonLayout->addWidget(okButton);

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		
		// Apply auto-sizing with larger defaults for plugin tables
		StreamUP::UIStyles::ApplyAutoSizing(dialog, 700, 1000, 150, 800);
	});
}


//-------------------PLUGIN UPDATE FUNCTIONS-------------------
void CheckAllPluginsForUpdates(bool manuallyTriggered)
{
	const auto& allPlugins = StreamUP::GetAllPlugins();
	if (allPlugins.empty()) {
		ErrorDialog(obs_module_text("Plugin.Error.LoadIssue"));
		return;
	}

	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgUpdate = "";
	std::vector<std::pair<std::string, std::string>> installedPlugins = GetInstalledPlugins();

	for (const auto &plugin : installedPlugins) {
		const std::string &plugin_name = plugin.first;
		const std::string &installed_version = plugin.second;
		
		auto plugin_it = allPlugins.find(plugin_name);
		if (plugin_it == allPlugins.end()) {
			continue;
		}
		
		const StreamUP::PluginInfo &plugin_info = plugin_it->second;
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
	}
	
	if (!version_mismatch_modules.empty()) {
		std::map<std::string, std::string> empty_missing_modules;
		PluginsHaveIssue(empty_missing_modules, version_mismatch_modules, nullptr);
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
		return;
	}

	if (api_response.find("Error:") != std::string::npos) {
		StreamUP::ErrorHandler::ShowErrorDialog("API Error", api_response);
		return;
	}

	if (api_response.empty()) {
		StreamUP::ErrorHandler::ShowErrorDialog("Plugin Load Error", obs_module_text("Plugin.Error.LoadIssue"));
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
		info.required = OBSWrappers::GetBoolProperty(plugin.get(), "required");

		StreamUP::PluginState::Instance().AddPlugin(name, info);

		if (info.required) {
			StreamUP::PluginState::Instance().AddRequiredPlugin(name, info);
		}
	}
}

bool CheckrequiredOBSPluginsWithoutUI(bool isLoadStreamUpFile)
{
	UNUSED_PARAMETER(isLoadStreamUpFile);
	const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
	if (requiredPlugins.empty()) {
		return false;
	}

	std::map<std::string, std::string> missing_modules;
	std::map<std::string, std::string> version_mismatch_modules;
	char *filepath = StreamUP::PathUtils::GetOBSLogPath();
	if (filepath == nullptr) {
		return false;
	}

	for (const auto &module : requiredPlugins) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &required_version = plugin_info.version;
		const std::string &search_string = plugin_info.searchString;

		if (search_string.find("[ignore]") != std::string::npos) {
			continue;
		}

		std::string installed_version;
		// Check if this is a theme entry (special handling)
		if (search_string.find("[THEME_CHECK]") != std::string::npos) {
			// For theme checking, use the theme-specific search function
			installed_version = SearchThemeFileForVersion("Version:");
		} else {
			// Regular plugin checking in log files
			installed_version = SearchStringInFileForVersion(filepath, search_string.c_str());
		}

		if (installed_version.empty() && plugin_info.required) {
			missing_modules.emplace(plugin_name, required_version);
		} else if (!installed_version.empty() && installed_version != required_version && plugin_info.required &&
			   VersionUtils::IsVersionLessThan(installed_version, required_version)) {
			version_mismatch_modules.emplace(plugin_name, installed_version);
		}
	}

	bfree(filepath);

	return missing_modules.empty() && version_mismatch_modules.empty();
}

bool CheckrequiredOBSPlugins(bool isLoadStreamUpFile)
{
	const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
	if (requiredPlugins.empty()) {
		ErrorDialog(obs_module_text("Plugin.Error.LoadIssue"));
		return false;
	}

	std::map<std::string, std::string> missing_modules;
	std::map<std::string, std::string> version_mismatch_modules;
	std::string errorMsgMissing = "";
	std::string errorMsgUpdate = "";
	char *filepath = StreamUP::PathUtils::GetOBSLogPath();
	if (filepath == nullptr) {
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

		std::string installed_version;
		// Check if this is a theme entry (special handling)
		if (search_string.find("[THEME_CHECK]") != std::string::npos) {
			// For theme checking, use the theme-specific search function
			installed_version = SearchThemeFileForVersion("Version:");
		} else {
			// Regular plugin checking in log files
			installed_version = SearchStringInFileForVersion(filepath, search_string.c_str());
		}

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
		PluginsHaveIssue(missing_modules, version_mismatch_modules, nullptr);

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
	static QString cached_filepath;
	static QString cached_path;
	
	// Cache the filepath lookup to avoid repeated filesystem operations
	QString current_path = QString::fromLocal8Bit(path);
	if (current_path != cached_path) {
		cached_path = current_path;
		cached_filepath = PathUtils::GetMostRecentFile(current_path);
	}
	
	if (cached_filepath.isEmpty()) {
		return "";
	}
	
	FILE *file = fopen(cached_filepath.toLocal8Bit().constData(), "r");
	if (!file) {
		return "";
	}
	
	char line[1024]; // Further increased buffer size
	static const std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	static const std::regex version_regex_double("[0-9]+\\.[0-9]+");
	const size_t search_len = strlen(search);
	
	while (fgets(line, sizeof(line), file)) {
		char *found_ptr = strstr(line, search);
		if (found_ptr) {
			std::string remaining_line(found_ptr + search_len);
			std::smatch match;

			// First try to find semantic version (x.y.z format)
			if (std::regex_search(remaining_line, match, version_regex_triple)) {
				std::string version = match.str(0);
				// Check if this looks like a git hash (contains only hex chars and is long)
				bool is_git_hash = version.length() >= 7 && std::all_of(version.begin(), version.end(), 
					[](char c) { return std::isxdigit(c) || c == '.'; }) && 
					std::count(version.begin(), version.end(), '.') >= 2;
				
				if (!is_git_hash) {
					fclose(file);
					return version;
				}
			}

			// If triple version was a git hash or not found, try double version
			if (std::regex_search(remaining_line, match, version_regex_double)) {
				std::string version = match.str(0);
				// Check if this looks like a git hash (contains only hex chars and is long)
				bool is_git_hash = version.length() >= 7 && std::all_of(version.begin(), version.end(), 
					[](char c) { return std::isxdigit(c) || c == '.'; }) && 
					std::count(version.begin(), version.end(), '.') >= 1;
				
				if (!is_git_hash) {
					fclose(file);
					return version;
				}
			}
		}
	}

	fclose(file);
	return "";
}

std::string SearchThemeFileForVersion(const char *search)
{
	// Get theme directory paths similar to how GetFilePath() gets log paths
	QStringList themePaths;
	
	// Use OBS module config path to find themes directory (similar to logs)
	char *config_path = nullptr;
	char *theme_path_abs = nullptr;
	
	if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
		// Try multiple relative paths to find the actual data/themes directory
		QStringList relativePaths = {
			"../../../../data/obs-studio/themes/",  // From config back to build data
			"../../data/obs-studio/themes/",       // Original attempt
			"../../../themes/",                    // User themes in config
			"../../../../../build_x64/rundir/RelWithDebInfo/data/obs-studio/themes/"  // Development build specific
		};
		
		for (const QString& relPath : relativePaths) {
			config_path = obs_module_config_path(relPath.toLocal8Bit().constData());
			theme_path_abs = os_get_abs_path_ptr(config_path);
			
			if (theme_path_abs) {
				QString themeDirPath = QString::fromLocal8Bit(theme_path_abs);
				QString fullThemePath = themeDirPath + "StreamUP.obt";
				themePaths << fullThemePath;
			}
			
			bfree(config_path);
			bfree(theme_path_abs);
			config_path = nullptr;
			theme_path_abs = nullptr;
		}
	} else {
		// macOS/Linux paths
		config_path = obs_module_config_path("../../data/obs-studio/themes/");
		theme_path_abs = os_get_abs_path_ptr(config_path);
		
		if (theme_path_abs) {
			QString themeDirPath = QString::fromLocal8Bit(theme_path_abs);
			themePaths << themeDirPath + "StreamUP.obt";
		}
	}
	
	if (config_path) bfree(config_path);
	if (theme_path_abs) bfree(theme_path_abs);
	
	// Also try direct path construction based on module location
	char *module_config_path = obs_module_config_path("");
	if (module_config_path) {
		QString configBase = QString::fromLocal8Bit(module_config_path);
		// Remove trailing slash and plugin-specific part to get base config
		configBase = configBase.left(configBase.lastIndexOf("plugins"));
		
		// Build path to data directory themes
		QString directDataPath = configBase + "../data/obs-studio/themes/StreamUP.obt";
		themePaths << directDataPath;
		
		bfree(module_config_path);
	}
	
	static const std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	static const std::regex version_regex_double("[0-9]+\\.[0-9]+");
	const size_t search_len = strlen(search);
	
	// Try each potential theme file location
	for (const QString& themePath : themePaths) {
		FILE *file = fopen(themePath.toLocal8Bit().constData(), "r");
		if (!file) {
			continue; // Try next path
		}
		
		char line[1024];
		while (fgets(line, sizeof(line), file)) {
			char *found_ptr = strstr(line, search);
			if (found_ptr) {
				std::string remaining_line(found_ptr + search_len);
				std::smatch match;

				if (std::regex_search(remaining_line, match, version_regex_triple)) {
					fclose(file);
					return match.str(0);
				}

				if (std::regex_search(remaining_line, match, version_regex_double)) {
					fclose(file);
					return match.str(0);
				}
			}
		}
		fclose(file);
	}
	
	return ""; // Theme file not found or version not found
}

std::vector<std::pair<std::string, std::string>> GetInstalledPlugins()
{
	std::vector<std::pair<std::string, std::string>> installedPlugins;
	char *filepath = StreamUP::PathUtils::GetOBSLogPath();
	if (filepath == nullptr) {
		return installedPlugins;
	}

	// Read the log file once and cache its content
	QString logfile = PathUtils::GetMostRecentFile(QString::fromLocal8Bit(filepath));
	if (logfile.isEmpty()) {
		bfree(filepath);
		return installedPlugins;
	}
	
	FILE *file = fopen(logfile.toLocal8Bit().constData(), "r");
	if (!file) {
		bfree(filepath);
		return installedPlugins;
	}
	
	// Read entire file content once
	std::string file_content;
	char buffer[4096];
	while (fgets(buffer, sizeof(buffer), file)) {
		file_content += buffer;
	}
	fclose(file);

	const auto& allPlugins = StreamUP::GetAllPlugins();
	installedPlugins.reserve(allPlugins.size()); // Reserve capacity for performance
	
	static const std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	static const std::regex version_regex_double("[0-9]+\\.[0-9]+");
	
	for (const auto &module : allPlugins) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &search_string = plugin_info.searchString;

		std::string installed_version;
		// Check if this is a theme entry (special handling)
		if (search_string.find("[THEME_CHECK]") != std::string::npos) {
			// For theme checking, use the theme-specific search function
			installed_version = SearchThemeFileForVersion("Version:");
		} else {
			// Search in log file content for regular plugins
			size_t found_pos = file_content.find(search_string);
			if (found_pos != std::string::npos) {
				std::string remaining = file_content.substr(found_pos + search_string.length());
				std::smatch match;
				
				// First try to find semantic version (x.y.z format)
				if (std::regex_search(remaining, match, version_regex_triple)) {
					std::string version = match.str(0);
					// Check if this looks like a git hash (contains only hex chars and is long)
					bool is_git_hash = version.length() >= 7 && std::all_of(version.begin(), version.end(), 
						[](char c) { return std::isxdigit(c) || c == '.'; }) && 
						std::count(version.begin(), version.end(), '.') >= 2;
					
					if (!is_git_hash) {
						installed_version = version;
					}
				}
				
				// If triple version was a git hash or not found, try double version
				if (installed_version.empty() && std::regex_search(remaining, match, version_regex_double)) {
					std::string version = match.str(0);
					// Check if this looks like a git hash (contains only hex chars and is long)
					bool is_git_hash = version.length() >= 7 && std::all_of(version.begin(), version.end(), 
						[](char c) { return std::isxdigit(c) || c == '.'; }) && 
						std::count(version.begin(), version.end(), '.') >= 1;
					
					if (!is_git_hash) {
						installed_version = version;
					}
				}
			}
		}

		// Add to installed plugins list if version was found
		if (!installed_version.empty()) {
			installedPlugins.emplace_back(plugin_name, installed_version);
		}
	}

	bfree(filepath);
	return installedPlugins;
}

//-------------------EFFICIENT CACHING FUNCTIONS-------------------
void PerformPluginCheckAndCache(bool checkAllPlugins)
{
	const auto& pluginsToCheck = checkAllPlugins ? StreamUP::GetAllPlugins() : StreamUP::GetRequiredPlugins();
	if (pluginsToCheck.empty()) {
		return;
	}

	std::map<std::string, std::string> missing_modules;
	std::map<std::string, std::string> version_mismatch_modules;
	char *filepath = StreamUP::PathUtils::GetOBSLogPath();
	if (filepath == nullptr) {
		return;
	}

	// Read the log file once and cache its content for efficient checking
	QString logfile = PathUtils::GetMostRecentFile(QString::fromLocal8Bit(filepath));
	if (logfile.isEmpty()) {
		bfree(filepath);
		return;
	}
	
	FILE *file = fopen(logfile.toLocal8Bit().constData(), "r");
	if (!file) {
		bfree(filepath);
		return;
	}
	
	// Read entire file content once
	std::string file_content;
	char buffer[4096];
	while (fgets(buffer, sizeof(buffer), file)) {
		file_content += buffer;
	}
	fclose(file);
	bfree(filepath);
	
	static const std::regex version_regex_triple("[0-9]+\\.[0-9]+\\.[0-9]+");
	static const std::regex version_regex_double("[0-9]+\\.[0-9]+");

	// Check plugins based on parameter (all plugins or just required ones)
	for (const auto &module : pluginsToCheck) {
		const std::string &plugin_name = module.first;
		const StreamUP::PluginInfo &plugin_info = module.second;
		const std::string &required_version = plugin_info.version;
		const std::string &search_string = plugin_info.searchString;

		if (search_string.find("[ignore]") != std::string::npos) {
			continue;
		}

		// Search for version
		std::string installed_version;
		// Check if this is a theme entry (special handling)
		if (search_string.find("[THEME_CHECK]") != std::string::npos) {
			// For theme checking, use the theme-specific search function
			installed_version = SearchThemeFileForVersion("Version:");
		} else {
			// Search in cached log file content for regular plugins
			size_t found_pos = file_content.find(search_string);
			if (found_pos != std::string::npos) {
				std::string remaining = file_content.substr(found_pos + search_string.length());
				std::smatch match;
				
				// First try to find semantic version (x.y.z format)
				if (std::regex_search(remaining, match, version_regex_triple)) {
					std::string version = match.str(0);
					// Check if this looks like a git hash (contains only hex chars and is long)
					bool is_git_hash = version.length() >= 7 && std::all_of(version.begin(), version.end(), 
						[](char c) { return std::isxdigit(c) || c == '.'; }) && 
						std::count(version.begin(), version.end(), '.') >= 2;
					
					if (!is_git_hash) {
						installed_version = version;
					}
				}
				
				// If triple version was a git hash or not found, try double version
				if (installed_version.empty() && std::regex_search(remaining, match, version_regex_double)) {
					std::string version = match.str(0);
					// Check if this looks like a git hash (contains only hex chars and is long)
					bool is_git_hash = version.length() >= 7 && std::all_of(version.begin(), version.end(), 
						[](char c) { return std::isxdigit(c) || c == '.'; }) && 
						std::count(version.begin(), version.end(), '.') >= 1;
					
					if (!is_git_hash) {
						installed_version = version;
					}
				}
			}
		}

		// For missing plugins, only add to missing list if it's a required plugin
		if (installed_version.empty() && plugin_info.required) {
			missing_modules.emplace(plugin_name, required_version);
		}
		// For version mismatches, check ALL plugins (required and non-required)
		else if (!installed_version.empty() && installed_version != required_version &&
			   VersionUtils::IsVersionLessThan(installed_version, required_version)) {
			version_mismatch_modules.emplace(plugin_name, installed_version);
		}
	}

	// Get all installed plugins for cache
	std::vector<std::pair<std::string, std::string>> installedPlugins = GetInstalledPlugins();

	// Cache the results
	StreamUP::PluginState::PluginCheckResults results;
	results.missingPlugins = std::move(missing_modules);
	results.outdatedPlugins = std::move(version_mismatch_modules);
	results.installedPlugins = std::move(installedPlugins);
	results.allRequiredUpToDate = results.missingPlugins.empty() && results.outdatedPlugins.empty();

	StreamUP::PluginState::Instance().SetPluginStatus(results);
}

bool IsAllPluginsUpToDateCached()
{
	if (!StreamUP::PluginState::Instance().IsPluginStatusCached()) {
		PerformPluginCheckAndCache(false); // Check only required plugins for "up to date" status
	}

	const auto& status = StreamUP::PluginState::Instance().GetCachedPluginStatus();
	return status.allRequiredUpToDate;
}

void ShowCachedPluginIssuesDialog(std::function<void()> continueCallback)
{
	if (!StreamUP::PluginState::Instance().IsPluginStatusCached()) {
		PerformPluginCheckAndCache(false); // Only check required plugins
	}

	const auto& status = StreamUP::PluginState::Instance().GetCachedPluginStatus();
	const auto& requiredPlugins = StreamUP::GetRequiredPlugins();
	
	// Filter cached results to only show required plugins
	std::map<std::string, std::string> filteredMissing;
	std::map<std::string, std::string> filteredOutdated;
	
	// Copy only required plugins from cached missing plugins
	for (const auto& plugin : status.missingPlugins) {
		if (requiredPlugins.find(plugin.first) != requiredPlugins.end()) {
			filteredMissing[plugin.first] = plugin.second;
		}
	}
	
	// Copy only required plugins from cached outdated plugins
	for (const auto& plugin : status.outdatedPlugins) {
		if (requiredPlugins.find(plugin.first) != requiredPlugins.end()) {
			filteredOutdated[plugin.first] = plugin.second;
		}
	}
	
	if (filteredMissing.empty() && filteredOutdated.empty()) {
		PluginsUpToDateOutput(true);
		return;
	}

	// Use modern table display for filtered results (only required plugins)
	PluginsHaveIssue(filteredMissing, filteredOutdated, continueCallback);
}

void ShowCachedPluginUpdatesDialog()
{
	if (!StreamUP::PluginState::Instance().IsPluginStatusCached()) {
		PerformPluginCheckAndCache(true); // Check all plugins
	}

	const auto& status = StreamUP::PluginState::Instance().GetCachedPluginStatus();
	
	// Write outdated plugins list to file
	QString appDataPath = PathUtils::GetLocalAppDataPath();
	QString filePath = appDataPath + "/StreamUP-OutdatedPluginsList.txt";
	
	std::ofstream outFile(filePath.toStdString(), std::ios::out);
	if (outFile.is_open()) {
		for (const auto &module : status.outdatedPlugins) {
			outFile << module.first << "\n";
		}
		outFile.close();
	}
	
	if (status.outdatedPlugins.empty()) {
		PluginsUpToDateOutput(true);
		return;
	}

	// Use modern table display for cached results
	std::map<std::string, std::string> empty_missing_modules;
	PluginsHaveIssue(empty_missing_modules, status.outdatedPlugins, nullptr);
}

void ShowCachedPluginUpdatesDialogSilent()
{
	if (!StreamUP::PluginState::Instance().IsPluginStatusCached()) {
		PerformPluginCheckAndCache(true); // Check all plugins
	}

	const auto& status = StreamUP::PluginState::Instance().GetCachedPluginStatus();
	
	// Always write outdated plugins list to file, even if silent
	QString appDataPath = PathUtils::GetLocalAppDataPath();
	QString filePath = appDataPath + "/StreamUP-OutdatedPluginsList.txt";
	
	std::ofstream outFile(filePath.toStdString(), std::ios::out);
	if (outFile.is_open()) {
		for (const auto &module : status.outdatedPlugins) {
			outFile << module.first << "\n";
		}
		outFile.close();
	}
	
	if (status.outdatedPlugins.empty()) {
		return; // Silent success - don't show any dialog
	}

	// Use modern table display for cached results
	std::map<std::string, std::string> empty_missing_modules;
	PluginsHaveIssue(empty_missing_modules, status.outdatedPlugins, nullptr);
}

void InvalidatePluginCache()
{
	StreamUP::PluginState::Instance().InvalidatePluginStatus();
}

std::vector<std::pair<std::string, std::string>> GetInstalledPluginsCached()
{
	if (!StreamUP::PluginState::Instance().IsPluginStatusCached()) {
		PerformPluginCheckAndCache(true); // Check all plugins to get complete installed list
	}

	const auto& status = StreamUP::PluginState::Instance().GetCachedPluginStatus();
	return status.installedPlugins;
}

//-------------------UI HELPER FUNCTIONS-------------------
QString GetPluginForumLink(const std::string &pluginName)
{
	const auto& allPlugins = StreamUP::GetAllPlugins();
	if (allPlugins.find(pluginName) != allPlugins.end()) {
		const StreamUP::PluginInfo &pluginInfo = allPlugins.at(pluginName);
		return QString::fromStdString(pluginInfo.generalURL);
	}
	return QString();
}

QString GetPluginPlatformURL(const std::string &pluginName)
{
	const auto& allPlugins = StreamUP::GetAllPlugins();
	if (allPlugins.find(pluginName) == allPlugins.end()) {
		return QString();
	}

	const StreamUP::PluginInfo &pluginInfo = allPlugins.at(pluginName);
	std::string url;
	if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
		url = pluginInfo.windowsURL;
	} else if (strcmp(STREAMUP_PLATFORM_NAME, "macos") == 0) {
		url = pluginInfo.macURL;
	} else if (strcmp(STREAMUP_PLATFORM_NAME, "linux") == 0) {
		url = pluginInfo.linuxURL;
	} else {
		url = pluginInfo.windowsURL;
	}
	return QString::fromStdString(url);
}

std::vector<std::string> SearchLoadedModulesInLogFile(const char *logPath)
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

	std::string filepath = StreamUP::PathUtils::GetMostRecentTxtFile(logPath);
	FILE *file = fopen(filepath.c_str(), "r");
	std::vector<std::string> collected_modules;
	std::regex timestamp_regex("^[0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3}:");

	if (file) {
		char line[256];
		bool in_section = false;

		while (fgets(line, 256, file) != NULL) {
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
		StreamUP::DebugLogger::LogErrorFormat("PluginManager", "Failed to open log file: %s", filepath.c_str());
	}

	std::sort(collected_modules.begin(), collected_modules.end(), [](const std::string &a, const std::string &b) {
		return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](char char1, char char2) {
			return std::tolower(char1) < std::tolower(char2);
		});
	});

	return collected_modules;
}

} // namespace PluginManager
} // namespace StreamUP
