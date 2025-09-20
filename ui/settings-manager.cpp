#include "settings-manager.hpp"
#include "../utilities/debug-logger.hpp"
#include "../utilities/path-utils.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "../utilities/obs-data-helpers.hpp"
#include "switch-button.hpp"
#include "plugin-manager.hpp"
#include "plugin-state.hpp"
#include "hotkey-manager.hpp"
#include "hotkey-widget.hpp"
#include "dock/streamup-dock.hpp"
#include "scene-organiser/scene-organiser-dock.hpp"
#include "streamup-toolbar.hpp"
#include "streamup-toolbar-configurator.hpp"
#include <obs-module.h>
#include <obs-data.h>
#include <obs-properties.h>
#include <obs-frontend-api.h>
#include <QMetaType>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpacerItem>
#include <QGroupBox>
#include <QScrollArea>
#include <QTimer>
#include <QSizePolicy>
#include <QPointer>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QComboBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QDesktopServices>
#include <memory>
#include <mutex>
#include <util/platform.h>

// UI settings management implementation

// Register obs_data_array_t* as opaque pointer with Qt's metatype system
Q_DECLARE_OPAQUE_POINTER(obs_data_array_t*)

namespace StreamUP {
namespace SettingsManager {

// Global settings state
static bool notificationsMuted = false;

// Enhanced cache for settings to avoid repeated file loading
static obs_data_t* cachedSettings = nullptr;
static bool settingsLoadLogged = false;
static std::mutex settingsCacheMutex; // Thread safety for settings cache

// Helper function to extract domain from URL
QString ExtractDomain(const QString &url)
{
	QUrl qurl(url);
	QString host = qurl.host();
	if (host.startsWith("www.")) {
		host = host.mid(4); // Remove "www."
	}
	return host.isEmpty() ? url : host;
}

// Helper function to create styled plugin table
QTableWidget *CreatePluginTable()
{
	QTableWidget *table = StreamUP::UIStyles::CreateStyledTableWidget();

	// Set column headers
	QStringList headers;
	headers << "Status"
		<< "Plugin Name"
		<< "Module Name"
		<< "Version"
		<< "Website";
	table->setColumnCount(5);
	table->setHorizontalHeaderLabels(headers);

	// Configure last column to stretch (Website column)
	table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

	return table;
}

// Helper function to add compatible plugin row
void AddCompatiblePluginRow(QTableWidget *table, const std::string &pluginName, const std::string &version)
{
	const auto &allPlugins = StreamUP::GetAllPlugins();
	auto it = allPlugins.find(pluginName);

	int row = table->rowCount();
	table->insertRow(row);

	// Status column - Compatible
	QTableWidgetItem *statusItem = new QTableWidgetItem("✅ Compatible");
	statusItem->setForeground(QColor(StreamUP::UIStyles::Colors::SUCCESS));
	table->setItem(row, 0, statusItem);

	// Plugin Name column
	table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(pluginName)));

	// Module Name column
	QString moduleName = "N/A";
	if (it != allPlugins.end() && !it->second.moduleName.empty()) {
		moduleName = QString::fromStdString(it->second.moduleName);
	}
	table->setItem(row, 2, new QTableWidgetItem(moduleName));

	// Version column
	table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(version)));

	// Website column - Show domain name
	QString forumLink = StreamUP::PluginManager::GetPluginForumLink(pluginName);
	QString domainName = ExtractDomain(forumLink);
	QTableWidgetItem *websiteItem = new QTableWidgetItem(domainName);
	websiteItem->setForeground(QColor(StreamUP::UIStyles::Colors::INFO));
	websiteItem->setData(Qt::UserRole, forumLink);
	table->setItem(row, 4, websiteItem);
}

// Helper function to add incompatible plugin row
void AddIncompatiblePluginRow(QTableWidget *table, const std::string &moduleName)
{
	int row = table->rowCount();
	table->insertRow(row);

	// Status column - Incompatible
	QTableWidgetItem *statusItem = new QTableWidgetItem("❌ Incompatible");
	statusItem->setForeground(QColor(StreamUP::UIStyles::Colors::ERROR));
	table->setItem(row, 0, statusItem);

	// Plugin Name column - N/A for incompatible
	QTableWidgetItem *nameItem = new QTableWidgetItem("N/A");
	nameItem->setForeground(QColor(StreamUP::UIStyles::Colors::TEXT_MUTED));
	table->setItem(row, 1, nameItem);

	// Module Name column
	table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(moduleName)));

	// Version column - N/A for incompatible
	QTableWidgetItem *versionItem = new QTableWidgetItem("N/A");
	versionItem->setForeground(QColor(StreamUP::UIStyles::Colors::TEXT_MUTED));
	table->setItem(row, 3, versionItem);

	// Website column - N/A for incompatible
	QTableWidgetItem *websiteItem = new QTableWidgetItem("N/A");
	websiteItem->setForeground(QColor(StreamUP::UIStyles::Colors::TEXT_MUTED));
	table->setItem(row, 4, websiteItem);
}

// Settings dialog is now managed by DialogManager in ui-helpers

obs_data_t *LoadSettings()
{
	std::lock_guard<std::mutex> lock(settingsCacheMutex);

	// Return cached settings if available
	if (cachedSettings) {
		obs_data_addref(cachedSettings);
		return cachedSettings;
	}

	char *configPath = StreamUP::PathUtils::GetOBSConfigPath("configs.json");
	obs_data_t *data = obs_data_create_from_json_file(configPath);

	if (!data) {
		StreamUP::DebugLogger::LogDebug("Settings", "Initialize", "Settings not found. Creating default settings...");
		char *config_path = StreamUP::PathUtils::GetOBSConfigPath("");
		if (config_path) {
			os_mkdirs(config_path);
			bfree(config_path);
		}

		data = obs_data_create();
		obs_data_set_bool(data, "run_at_startup", true);
		obs_data_set_bool(data, "notifications_mute", false);
		obs_data_set_bool(data, "show_cph_integration", true);
		obs_data_set_bool(data, "show_toolbar", true);
		obs_data_set_bool(data, "debug_logging_enabled", false);
		obs_data_set_string(data, "toolbar_position", "top");

		// Set default dock tool settings
		obs_data_t *dockData = obs_data_create();
		obs_data_set_bool(dockData, "show_lock_all_sources", true);
		obs_data_set_bool(dockData, "show_lock_current_sources", true);
		obs_data_set_bool(dockData, "show_refresh_browser_sources", true);
		obs_data_set_bool(dockData, "show_refresh_audio_monitoring", true);
		obs_data_set_bool(dockData, "show_video_capture_options", true);
		obs_data_set_obj(data, "dock_tools", dockData);
		obs_data_release(dockData);

		if (obs_data_save_json(data, configPath)) {
		} else {
			StreamUP::DebugLogger::LogWarning("Settings", "Failed to save default settings to file");
		}
	} else {
		// Only log success message once
		if (!settingsLoadLogged) {
			settingsLoadLogged = true;
		}
	}

	// Cache the settings for future use
	cachedSettings = data;
	obs_data_addref(cachedSettings);

	bfree(configPath);
	return data;
}

bool SaveSettings(obs_data_t *settings)
{
	std::lock_guard<std::mutex> lock(settingsCacheMutex);

	char *configPath = StreamUP::PathUtils::GetOBSConfigPath("configs.json");
	bool success = false;

	if (obs_data_save_json(settings, configPath)) {
		success = true;

		// Invalidate cache so next load picks up the updated settings
		if (cachedSettings) {
			obs_data_release(cachedSettings);
			cachedSettings = nullptr;
		}
	} else {
		StreamUP::DebugLogger::LogWarning("Settings", "Failed to save settings to file");
	}

	bfree(configPath);
	return success;
}

PluginSettings GetCurrentSettings()
{
	PluginSettings settings;
	obs_data_t *data = LoadSettings();

	if (data) {
		settings.runAtStartup = StreamUP::OBSDataHelpers::GetBoolWithDefault(data, "run_at_startup", true);
		settings.notificationsMute = StreamUP::OBSDataHelpers::GetBoolWithDefault(data, "notifications_mute", false);
		settings.showCPHIntegration = StreamUP::OBSDataHelpers::GetBoolWithDefault(data, "show_cph_integration", true);
		settings.showToolbar = StreamUP::OBSDataHelpers::GetBoolWithDefault(data, "show_toolbar", true);
		settings.debugLoggingEnabled = StreamUP::OBSDataHelpers::GetBoolWithDefault(data, "debug_logging_enabled", false);
		settings.sceneOrganiserShowIcons = StreamUP::OBSDataHelpers::GetBoolWithDefault(data, "scene_organiser_show_icons", true);

		// Load scene switch mode setting (default to single-click if not set)
		const char *switchModeStr = StreamUP::OBSDataHelpers::GetStringWithDefault(data, "scene_organiser_switch_mode", "single_click");
		if (switchModeStr && strlen(switchModeStr) > 0) {
			if (strcmp(switchModeStr, "double_click") == 0) {
				settings.sceneOrganiserSwitchMode = SceneSwitchMode::DoubleClick;
			} else {
				settings.sceneOrganiserSwitchMode = SceneSwitchMode::SingleClick;
			}
		} else {
			settings.sceneOrganiserSwitchMode = SceneSwitchMode::SingleClick;
		}

		// Load toolbar position setting (default to top if not set)
		const char *positionStr = StreamUP::OBSDataHelpers::GetStringWithDefault(data, "toolbar_position", "top");
		if (positionStr && strlen(positionStr) > 0) {
			if (strcmp(positionStr, "bottom") == 0) {
				settings.toolbarPosition = ToolbarPosition::Bottom;
			} else if (strcmp(positionStr, "left") == 0) {
				settings.toolbarPosition = ToolbarPosition::Left;
			} else if (strcmp(positionStr, "right") == 0) {
				settings.toolbarPosition = ToolbarPosition::Right;
			} else {
				settings.toolbarPosition = ToolbarPosition::Top;
			}
		} else {
			settings.toolbarPosition = ToolbarPosition::Top;
		}

		// Load dock tool settings
		obs_data_t *dockData = obs_data_get_obj(data, "dock_tools");
		if (dockData) {
			settings.dockTools.showLockAllSources = StreamUP::OBSDataHelpers::GetBoolWithDefault(dockData, "show_lock_all_sources", true);
			settings.dockTools.showLockCurrentSources = StreamUP::OBSDataHelpers::GetBoolWithDefault(dockData, "show_lock_current_sources", true);
			settings.dockTools.showRefreshBrowserSources = StreamUP::OBSDataHelpers::GetBoolWithDefault(dockData, "show_refresh_browser_sources", true);
			settings.dockTools.showRefreshAudioMonitoring = StreamUP::OBSDataHelpers::GetBoolWithDefault(dockData, "show_refresh_audio_monitoring", true);
			settings.dockTools.showVideoCaptureOptions = StreamUP::OBSDataHelpers::GetBoolWithDefault(dockData, "show_video_capture_options", true);

			obs_data_release(dockData);
		} else {
		}

		obs_data_release(data);
	}

	return settings;
}

void UpdateSettings(const PluginSettings &settings)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "run_at_startup", settings.runAtStartup);
	obs_data_set_bool(data, "notifications_mute", settings.notificationsMute);
	obs_data_set_bool(data, "show_cph_integration", settings.showCPHIntegration);
	obs_data_set_bool(data, "show_toolbar", settings.showToolbar);
	obs_data_set_bool(data, "debug_logging_enabled", settings.debugLoggingEnabled);
	obs_data_set_bool(data, "scene_organiser_show_icons", settings.sceneOrganiserShowIcons);

	// Save scene switch mode setting
	const char *switchModeStr;
	switch (settings.sceneOrganiserSwitchMode) {
	case SceneSwitchMode::DoubleClick:
		switchModeStr = "double_click";
		break;
	case SceneSwitchMode::SingleClick:
	default:
		switchModeStr = "single_click";
		break;
	}
	obs_data_set_string(data, "scene_organiser_switch_mode", switchModeStr);

	// Save toolbar position setting
	const char *positionStr;
	switch (settings.toolbarPosition) {
	case ToolbarPosition::Bottom:
		positionStr = "bottom";
		break;
	case ToolbarPosition::Left:
		positionStr = "left";
		break;
	case ToolbarPosition::Right:
		positionStr = "right";
		break;
	default:
	case ToolbarPosition::Top:
		positionStr = "top";
		break;
	}
	obs_data_set_string(data, "toolbar_position", positionStr);

	// Save dock tool settings
	obs_data_t *dockData = obs_data_create();
	obs_data_set_bool(dockData, "show_lock_all_sources", settings.dockTools.showLockAllSources);
	obs_data_set_bool(dockData, "show_lock_current_sources", settings.dockTools.showLockCurrentSources);
	obs_data_set_bool(dockData, "show_refresh_browser_sources", settings.dockTools.showRefreshBrowserSources);
	obs_data_set_bool(dockData, "show_refresh_audio_monitoring", settings.dockTools.showRefreshAudioMonitoring);
	obs_data_set_bool(dockData, "show_video_capture_options", settings.dockTools.showVideoCaptureOptions);

	obs_data_set_obj(data, "dock_tools", dockData);
	obs_data_release(dockData);

	SaveSettings(data);

	// Update global state
	notificationsMuted = settings.notificationsMute;

	obs_data_release(data);
}

void InitializeSettingsSystem()
{
	obs_data_t *settings = LoadSettings();

	if (settings) {
		bool runAtStartup = obs_data_get_bool(settings, "run_at_startup");
		StreamUP::DebugLogger::LogDebugFormat("Settings", "Startup", "Run at startup setting: %s", runAtStartup ? "true" : "false");

		notificationsMuted = obs_data_get_bool(settings, "notifications_mute");
		StreamUP::DebugLogger::LogDebugFormat("Settings", "Notifications", "Notifications mute setting: %s", notificationsMuted ? "true" : "false");

		obs_data_release(settings);
	} else {
		StreamUP::DebugLogger::LogWarning("Settings", "Failed to load settings in initialization");
	}
}

void ShowSettingsDialog()
{
	ShowSettingsDialog(0); // Default to General Settings tab
}

void ShowSettingsDialog(int tabIndex)
{
	StreamUP::UIHelpers::ShowSingletonDialogOnUIThread("settings", [tabIndex]() -> QDialog* {
		obs_data_t *settings = LoadSettings();
		if (!settings) {
			return nullptr;
		}

		// Create modern unified dialog with darkest background
		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Window.Title"));
		dialog->resize(900, 600);
		dialog->setStyleSheet(QString("QDialog { background-color: %1; }")
				      .arg(StreamUP::UIStyles::Colors::BG_DARKEST));

		QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
		mainLayout->setContentsMargins(0, 0, 0, 0);
		mainLayout->setSpacing(0);

		// Create OBS-style horizontal layout with sidebar and content
		QHBoxLayout *contentLayout = new QHBoxLayout();
		contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
						  StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		contentLayout->setSpacing(StreamUP::UIStyles::Sizes::PADDING_XL);

		// Left sidebar container with rounded corners (like OBS)
		QWidget *sidebarContainer = new QWidget();
		sidebarContainer->setFixedWidth(200);
		sidebarContainer->setStyleSheet(QString(
			"QWidget {"
			"    background-color: %1;"
			"    border-radius: %2px;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));

		QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarContainer);
		sidebarLayout->setContentsMargins(0, 0, 0, 0);
		sidebarLayout->setSpacing(0);

		QListWidget *categoryList = new QListWidget();
		categoryList->setStyleSheet(QString(
			"QListWidget {"
			"    background-color: transparent;"
			"    border: none;"
			"    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
			"    font-size: 14px;"
			"    outline: none;"
			"    border-radius: %5px;"
			"}"
			"QListWidget::item {"
			"    padding: 12px 16px;"
			"    border: none;"
			"    color: %1;"
			"    background-color: transparent;"
			"}"
			"QListWidget::item:selected {"
			"    background-color: %2;"
			"    color: %3;"
			"    border: none;"
			"    border-radius: %6px;"
			"}"
			"QListWidget::item:hover:!selected {"
			"    background-color: %4;"
			"    color: %3;"
			"    border-radius: %6px;"
			"}"
			"QListWidget::item:first {"
			"    border-top-left-radius: %5px;"
			"    border-top-right-radius: %5px;"
			"}"
			"QListWidget::item:last {"
			"    border-bottom-left-radius: %5px;"
			"    border-bottom-right-radius: %5px;"
			"}"
		).arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
		 .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR)
		 .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
		 .arg(StreamUP::UIStyles::Colors::HOVER_OVERLAY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_XL));

		sidebarLayout->addWidget(categoryList);

		// Add categories to sidebar
		QStringList categories = {
			obs_module_text("Settings.Group.General"),
			obs_module_text("Settings.Group.Toolbar"),
			obs_module_text("SceneOrganiser.Settings.Title"),
			obs_module_text("Settings.Group.PluginManagement"),
			obs_module_text("Settings.Group.Hotkeys"),
			obs_module_text("Settings.Group.DockConfig")
		};

		for (const QString &category : categories) {
			categoryList->addItem(category);
		}
		categoryList->setCurrentRow(tabIndex);

		// Right content area
		// Note: No scroll area needed with stacked widget approach
		
		// Stack widget to hold different category pages
		QStackedWidget *stackedWidget = new QStackedWidget();
		stackedWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
		stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

		obs_properties_t *props = obs_properties_create();

		// Create individual category pages
		
		// 1. General Settings Page
		QWidget *generalPage = new QWidget();
		QVBoxLayout *generalPageLayout = new QVBoxLayout(generalPage);
		generalPageLayout->setContentsMargins(0, 0, 0, 0);
		generalPageLayout->setSpacing(0);
		
		// Create scrollable container with dialog box styling
		QScrollArea *generalScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
		generalScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		generalScrollArea->setStyleSheet(generalScrollArea->styleSheet() + QString(
			"QScrollArea {"
			"    background-color: %1;"
			"    border: none;"
			"    border-radius: %2px;"
			"}"
			"QScrollArea > QWidget > QWidget {"
			"    background: transparent;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));
		
		// Create content container with dialog box background
		QWidget *generalContentContainer = new QWidget();
		generalContentContainer->setStyleSheet(QString(
			"QWidget {"
			"    background: transparent;"
			"}"
		));
		
		QVBoxLayout *generalContentLayout = new QVBoxLayout(generalContentContainer);
		generalContentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
							 StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		generalContentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		// Remove the GroupBox wrapper - settings go directly in the content container
		QWidget *generalSettingsWidget = new QWidget();

		QVBoxLayout *generalLayout = new QVBoxLayout(generalSettingsWidget);
		generalLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		// Run at startup setting
		obs_property_t *runAtStartupProp =
			obs_properties_add_bool(props, "run_at_startup", obs_module_text("Settings.Plugin.RunOnStartup"));

		// Create horizontal layout for run at startup setting
		QHBoxLayout *runAtStartupLayout = new QHBoxLayout();

		QLabel *runAtStartupLabel = new QLabel(obs_module_text("Settings.Plugin.RunOnStartup"));
		runAtStartupLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							 .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));

		StreamUP::UIStyles::SwitchButton *runAtStartupSwitch = StreamUP::UIStyles::CreateStyledSwitch(
			"", obs_data_get_bool(settings, obs_property_name(runAtStartupProp)));

		QObject::connect(runAtStartupSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [](bool checked) {
			// Use modern settings system to avoid conflicts with dock settings
			PluginSettings currentSettings = GetCurrentSettings();
			currentSettings.runAtStartup = checked;
			UpdateSettings(currentSettings);
		});

		runAtStartupLayout->addWidget(runAtStartupLabel);
		runAtStartupLayout->addStretch();
		runAtStartupLayout->addWidget(runAtStartupSwitch);
		generalLayout->addLayout(runAtStartupLayout);

		// Notifications mute setting
		obs_property_t *notificationsMuteProp =
			obs_properties_add_bool(props, "notifications_mute", obs_module_text("Settings.Notifications.Mute"));

		// Create horizontal layout for notifications setting
		QHBoxLayout *notificationsLayout = new QHBoxLayout();

		QLabel *notificationsLabel = new QLabel(obs_module_text("Settings.Notifications.Mute"));
		notificationsLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							  .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							  .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		notificationsLabel->setToolTip(obs_module_text("Settings.Notifications.MuteTooltip"));

		StreamUP::UIStyles::SwitchButton *notificationsMuteSwitch = StreamUP::UIStyles::CreateStyledSwitch(
			"", obs_data_get_bool(settings, obs_property_name(notificationsMuteProp)));
		notificationsMuteSwitch->setToolTip(obs_module_text("Settings.Notifications.MuteTooltip"));

		QObject::connect(notificationsMuteSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [](bool checked) {
			// Use modern settings system to avoid conflicts with dock settings
			PluginSettings currentSettings = GetCurrentSettings();
			currentSettings.notificationsMute = checked;
			UpdateSettings(currentSettings);
		});

		notificationsLayout->addWidget(notificationsLabel);
		notificationsLayout->addStretch();
		notificationsLayout->addWidget(notificationsMuteSwitch);
		generalLayout->addLayout(notificationsLayout);

		// CPH Integration setting
		obs_property_t *cphIntegrationProp =
			obs_properties_add_bool(props, "show_cph_integration", obs_module_text("Settings.CPH.Integration"));

		// Create horizontal layout for CPH integration setting
		QHBoxLayout *cphLayout = new QHBoxLayout();

		QLabel *cphLabel = new QLabel(obs_module_text("Settings.CPH.Integration"));
		cphLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
						.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
						.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		cphLabel->setToolTip(obs_module_text("Settings.CPH.IntegrationTooltip"));

		StreamUP::UIStyles::SwitchButton *cphIntegrationSwitch = StreamUP::UIStyles::CreateStyledSwitch(
			"", obs_data_get_bool(settings, obs_property_name(cphIntegrationProp)));
		cphIntegrationSwitch->setToolTip(obs_module_text("Settings.CPH.IntegrationTooltip"));

		QObject::connect(cphIntegrationSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [](bool checked) {
			// Use modern settings system to avoid conflicts with dock settings
			PluginSettings currentSettings = GetCurrentSettings();
			currentSettings.showCPHIntegration = checked;
			UpdateSettings(currentSettings);
		});

		cphLayout->addWidget(cphLabel);
		cphLayout->addStretch();
		cphLayout->addWidget(cphIntegrationSwitch);
		generalLayout->addLayout(cphLayout);

		// Debug Logging setting
		obs_property_t *debugLoggingProp =
			obs_properties_add_bool(props, "debug_logging_enabled", obs_module_text("Settings.Debug.Logging"));

		// Create horizontal layout for debug logging setting
		QHBoxLayout *debugLoggingLayout = new QHBoxLayout();

		QLabel *debugLoggingLabel = new QLabel(obs_module_text("Settings.Debug.Logging"));
		debugLoggingLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							  .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							  .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		debugLoggingLabel->setToolTip(obs_module_text("Settings.Debug.LoggingTooltip"));

		StreamUP::UIStyles::SwitchButton *debugLoggingSwitch = StreamUP::UIStyles::CreateStyledSwitch(
			"", obs_data_get_bool(settings, obs_property_name(debugLoggingProp)));
		debugLoggingSwitch->setToolTip(obs_module_text("Settings.Debug.LoggingTooltip"));

		QObject::connect(debugLoggingSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [](bool checked) {
			// Use modern settings system to avoid conflicts with dock settings
			PluginSettings currentSettings = GetCurrentSettings();
			currentSettings.debugLoggingEnabled = checked;
			UpdateSettings(currentSettings);
		});

		debugLoggingLayout->addWidget(debugLoggingLabel);
		debugLoggingLayout->addStretch();
		debugLoggingLayout->addWidget(debugLoggingSwitch);
		generalLayout->addLayout(debugLoggingLayout);

		generalContentLayout->addWidget(generalSettingsWidget);
		generalContentLayout->addStretch();
		
		generalScrollArea->setWidget(generalContentContainer);
		generalPageLayout->addWidget(generalScrollArea);
		stackedWidget->addWidget(generalPage);

		// 2. Toolbar Settings Page
		QWidget *toolbarPage = new QWidget();
		QVBoxLayout *toolbarPageLayout = new QVBoxLayout(toolbarPage);
		toolbarPageLayout->setContentsMargins(0, 0, 0, 0);
		toolbarPageLayout->setSpacing(0);
		
		// Create scrollable container with dialog box styling
		QScrollArea *toolbarScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
		toolbarScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		toolbarScrollArea->setStyleSheet(toolbarScrollArea->styleSheet() + QString(
			"QScrollArea {"
			"    background-color: %1;"
			"    border: none;"
			"    border-radius: %2px;"
			"}"
			"QScrollArea > QWidget > QWidget {"
			"    background: transparent;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));
		
		// Create content container with dialog box background
		QWidget *toolbarContentContainer = new QWidget();
		toolbarContentContainer->setStyleSheet(QString(
			"QWidget {"
			"    background: transparent;"
			"}"
		));
		
		QVBoxLayout *toolbarContentLayout = new QVBoxLayout(toolbarContentContainer);
		toolbarContentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
							 StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		toolbarContentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		QWidget *toolbarSettingsWidget = new QWidget();

		QVBoxLayout *toolbarLayout = new QVBoxLayout(toolbarSettingsWidget);
		toolbarLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		// Show toolbar setting
		obs_property_t *showToolbarProp = obs_properties_add_bool(props, "show_toolbar", obs_module_text("StreamUP.Settings.ShowToolbar"));

		// Create horizontal layout for show toolbar setting
		QHBoxLayout *showToolbarLayout = new QHBoxLayout();

		QLabel *showToolbarLabel = new QLabel(obs_module_text("StreamUP.Settings.ShowToolbar"));
		showToolbarLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		showToolbarLabel->setToolTip(obs_module_text("Toolbar.Tooltip.ShowHideToolbar"));

		StreamUP::UIStyles::SwitchButton *showToolbarSwitch =
			StreamUP::UIStyles::CreateStyledSwitch("", obs_data_get_bool(settings, obs_property_name(showToolbarProp)));
		showToolbarSwitch->setToolTip(obs_module_text("Toolbar.Tooltip.ShowHideToolbar"));

		QObject::connect(showToolbarSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [](bool checked) {
			// Use modern settings system
			PluginSettings currentSettings = GetCurrentSettings();
			currentSettings.showToolbar = checked;
			UpdateSettings(currentSettings);

			// Apply toolbar visibility
			extern void ApplyToolbarVisibility();
			ApplyToolbarVisibility();
		});

		showToolbarLayout->addWidget(showToolbarLabel);
		showToolbarLayout->addStretch();
		showToolbarLayout->addWidget(showToolbarSwitch);
		toolbarLayout->addLayout(showToolbarLayout);

		// Toolbar position setting with combobox
		QHBoxLayout *toolbarPositionLayout = new QHBoxLayout();

		QLabel *toolbarPositionLabel = new QLabel(obs_module_text("StreamUP.Settings.ToolbarPosition"));
		toolbarPositionLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							    .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							    .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		toolbarPositionLabel->setToolTip("Choose where to place the toolbar in OBS");

		// Get current toolbar position
		PluginSettings currentSettings = GetCurrentSettings();

		// Create combobox for position selection
		QComboBox *positionComboBox = new QComboBox();
		positionComboBox->addItem("Top", static_cast<int>(ToolbarPosition::Top));
		positionComboBox->addItem("Bottom", static_cast<int>(ToolbarPosition::Bottom));
		positionComboBox->addItem("Left", static_cast<int>(ToolbarPosition::Left));
		positionComboBox->addItem("Right", static_cast<int>(ToolbarPosition::Right));

		// Set current selection
		int currentIndex = static_cast<int>(currentSettings.toolbarPosition);
		positionComboBox->setCurrentIndex(currentIndex);

		// Set combobox styling and size
		positionComboBox->setStyleSheet(StreamUP::UIStyles::GetComboBoxStyle());
		positionComboBox->setMinimumWidth(100);
		positionComboBox->setMaximumWidth(150);
		positionComboBox->setToolTip("Choose toolbar position: Top, Bottom, Left, or Right");

		// Connect combobox selection change
		QObject::connect(positionComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
				 [](int index) {
					 if (index >= 0) {
						 PluginSettings currentSettings = GetCurrentSettings();
						 currentSettings.toolbarPosition = static_cast<ToolbarPosition>(index);
						 UpdateSettings(currentSettings);

						 // Apply toolbar position
						 extern void ApplyToolbarPosition();
						 ApplyToolbarPosition();
					 }
				 });

		toolbarPositionLayout->addWidget(toolbarPositionLabel);
		toolbarPositionLayout->addStretch();
		toolbarPositionLayout->addWidget(positionComboBox);
		toolbarLayout->addLayout(toolbarPositionLayout);

		// Add spacing between sections
		toolbarLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		// Add "Configure Toolbar" button
		QHBoxLayout *configureToolbarLayout = new QHBoxLayout();
		
		QLabel *configureToolbarLabel = new QLabel(obs_module_text("StreamUP.Settings.ToolbarConfiguration"));
		configureToolbarLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							    .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							    .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		configureToolbarLabel->setToolTip("Customize toolbar buttons and layout");

		QPushButton *configureToolbarButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("StreamUP.Settings.ConfigureToolbar"), "neutral");
		configureToolbarButton->setToolTip("Open toolbar configuration dialog");
		
		QObject::connect(configureToolbarButton, &QPushButton::clicked, [dialog]() {
			// Find the toolbar widget and open its configurator
			QWidget* mainWindow = static_cast<QWidget*>(obs_frontend_get_main_window());
			if (mainWindow) {
				StreamUPToolbar* toolbar = mainWindow->findChild<StreamUPToolbar*>();
				if (toolbar) {
					StreamUP::ToolbarConfigurator configurator(dialog);
					if (configurator.exec() == QDialog::Accepted) {
						// Refresh the toolbar with new configuration
						toolbar->refreshFromConfiguration();
					}
				}
			}
		});

		configureToolbarLayout->addWidget(configureToolbarLabel);
		configureToolbarLayout->addStretch();
		configureToolbarLayout->addWidget(configureToolbarButton);
		toolbarLayout->addLayout(configureToolbarLayout);

		toolbarContentLayout->addWidget(toolbarSettingsWidget);
		toolbarContentLayout->addStretch();
		
		toolbarScrollArea->setWidget(toolbarContentContainer);
		toolbarPageLayout->addWidget(toolbarScrollArea);
		stackedWidget->addWidget(toolbarPage);

		// 3. Scene Organiser Page
		QWidget *sceneOrganiserPage = new QWidget();
		QVBoxLayout *sceneOrganiserPageLayout = new QVBoxLayout(sceneOrganiserPage);
		sceneOrganiserPageLayout->setContentsMargins(0, 0, 0, 0);
		sceneOrganiserPageLayout->setSpacing(0);

		// Create scrollable container with dialog box styling
		QScrollArea *sceneOrganiserScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
		sceneOrganiserScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		sceneOrganiserScrollArea->setStyleSheet(sceneOrganiserScrollArea->styleSheet() + QString(
			"QScrollArea {"
			"    background-color: %1;"
			"    border: none;"
			"    border-radius: %2px;"
			"}"
			"QScrollArea > QWidget > QWidget {"
			"    background: transparent;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));

		// Create content container with dialog box background
		QWidget *sceneOrganiserContentContainer = new QWidget();
		sceneOrganiserContentContainer->setStyleSheet(QString(
			"QWidget {"
			"    background: transparent;"
			"}"
		));

		QVBoxLayout *sceneOrganiserContentLayout = new QVBoxLayout(sceneOrganiserContentContainer);
		sceneOrganiserContentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
							 StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		sceneOrganiserContentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		QWidget *sceneOrganiserSettingsWidget = new QWidget();
		QVBoxLayout *sceneOrganiserLayout = new QVBoxLayout(sceneOrganiserSettingsWidget);
		sceneOrganiserLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		// Description header
		QLabel *sceneOrganiserDescription = new QLabel(obs_module_text("SceneOrganiser.Settings.Description"));
		sceneOrganiserDescription->setStyleSheet(QString("color: %1; font-size: %2px; margin-bottom: 16px; background: transparent;")
							.arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
							.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		sceneOrganiserDescription->setWordWrap(true);
		sceneOrganiserLayout->addWidget(sceneOrganiserDescription);


		// Show Icons setting
		sceneOrganiserLayout->addSpacing(16);
		QHBoxLayout *showIconsLayout = new QHBoxLayout();

		QLabel *showIconsLabel = new QLabel(obs_module_text("SceneOrganiser.Settings.ShowIcons"));
		showIconsLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
						.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
						.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		showIconsLabel->setToolTip(obs_module_text("SceneOrganiser.Settings.ShowIconsDesc"));

		StreamUP::UIStyles::SwitchButton *showIconsSwitch =
			StreamUP::UIStyles::CreateStyledSwitch("", currentSettings.sceneOrganiserShowIcons);
		showIconsSwitch->setToolTip(obs_module_text("SceneOrganiser.Settings.ShowIconsDesc"));

		QObject::connect(showIconsSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [](bool checked) {
			PluginSettings settings = GetCurrentSettings();
			settings.sceneOrganiserShowIcons = checked;
			UpdateSettings(settings);

			// Notify all scene organiser docks to update their icon visibility
			StreamUP::SceneOrganiser::SceneOrganiserDock::NotifySceneOrganiserIconsChanged();
		});

		showIconsLayout->addWidget(showIconsLabel);
		showIconsLayout->addStretch();
		showIconsLayout->addWidget(showIconsSwitch);
		sceneOrganiserLayout->addLayout(showIconsLayout);

		// Scene switching mode setting
		QHBoxLayout *switchModeLayout = new QHBoxLayout();
		switchModeLayout->setContentsMargins(0, 0, 0, 0);

		QLabel *switchModeLabel = new QLabel(obs_module_text("SceneOrganiser.Settings.SwitchMode"));
		switchModeLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
							.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		switchModeLabel->setWordWrap(true);

		// Create combobox for switch mode selection
		QComboBox *switchModeComboBox = new QComboBox();
		switchModeComboBox->addItem(obs_module_text("SceneOrganiser.Settings.SwitchMode.SingleClick"), static_cast<int>(SceneSwitchMode::SingleClick));
		switchModeComboBox->addItem(obs_module_text("SceneOrganiser.Settings.SwitchMode.DoubleClick"), static_cast<int>(SceneSwitchMode::DoubleClick));

		// Set current selection
		int currentSwitchModeIndex = static_cast<int>(currentSettings.sceneOrganiserSwitchMode);
		switchModeComboBox->setCurrentIndex(switchModeComboBox->findData(currentSwitchModeIndex));

		// Connect change handler
		QObject::connect(switchModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
				 [switchModeComboBox](int index) {
					 if (index >= 0) {
						 QVariant data = switchModeComboBox->itemData(index);
						 if (data.isValid()) {
							 PluginSettings currentSettings = GetCurrentSettings();
							 currentSettings.sceneOrganiserSwitchMode = static_cast<SceneSwitchMode>(data.toInt());
							 UpdateSettings(currentSettings);
						 }
					 }
				 });

		switchModeLayout->addWidget(switchModeLabel);
		switchModeLayout->addStretch();
		switchModeLayout->addWidget(switchModeComboBox);
		sceneOrganiserLayout->addLayout(switchModeLayout);

		// Credit section
		sceneOrganiserLayout->addSpacing(20);

		QGroupBox *creditGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("SceneOrganiser.Settings.Credit"), "info");
		QVBoxLayout *creditLayout = new QVBoxLayout(creditGroup);
		creditLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		QLabel *creditText = new QLabel(obs_module_text("SceneOrganiser.Settings.CreditText"));
		creditText->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
						.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
						.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		creditText->setWordWrap(true);
		creditLayout->addWidget(creditText);

		QPushButton *creditButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("SceneOrganiser.Settings.CreditLink"), "neutral");
		creditButton->setToolTip("https://github.com/DigitOtter/obs_scene_tree_view");

		QObject::connect(creditButton, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://github.com/DigitOtter/obs_scene_tree_view"));
		});

		creditLayout->addWidget(creditButton);
		sceneOrganiserLayout->addWidget(creditGroup);

		sceneOrganiserContentLayout->addWidget(sceneOrganiserSettingsWidget);
		sceneOrganiserContentLayout->addStretch();

		sceneOrganiserScrollArea->setWidget(sceneOrganiserContentContainer);
		sceneOrganiserPageLayout->addWidget(sceneOrganiserScrollArea);
		stackedWidget->addWidget(sceneOrganiserPage);

		// 4. Plugin Management Page
		QWidget *pluginPage = new QWidget();
		QVBoxLayout *pluginPageLayout = new QVBoxLayout(pluginPage);
		pluginPageLayout->setContentsMargins(0, 0, 0, 0);
		pluginPageLayout->setSpacing(0);
		
		// Create scrollable container with dialog box styling
		QScrollArea *pluginScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
		pluginScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		pluginScrollArea->setStyleSheet(pluginScrollArea->styleSheet() + QString(
			"QScrollArea {"
			"    background-color: %1;"
			"    border: none;"
			"    border-radius: %2px;"
			"}"
			"QScrollArea > QWidget > QWidget {"
			"    background: transparent;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));
		
		// Create content container with dialog box background
		QWidget *pluginContentContainer = new QWidget();
		pluginContentContainer->setStyleSheet(QString(
			"QWidget {"
			"    background: transparent;"
			"}"
		));
		
		QVBoxLayout *pluginContentLayout = new QVBoxLayout(pluginContentContainer);
		pluginContentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
							StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		pluginContentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		QWidget *pluginSettingsWidget = new QWidget();

		QVBoxLayout *pluginLayout = new QVBoxLayout(pluginSettingsWidget);
		pluginLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		QPushButton *pluginButton =
			StreamUP::UIStyles::CreateStyledButton(obs_module_text("Settings.Plugin.ViewInstalled"), "info");

		// Connect plugin button to open plugins dialog
		QObject::connect(pluginButton, &QPushButton::clicked, []() {
			StreamUP::SettingsManager::ShowInstalledPluginsPage(nullptr);
		});

		QHBoxLayout *pluginButtonLayout = new QHBoxLayout();
		pluginButtonLayout->addStretch();
		pluginButtonLayout->addWidget(pluginButton);
		pluginButtonLayout->addStretch();
		pluginLayout->addLayout(pluginButtonLayout);

		pluginContentLayout->addWidget(pluginSettingsWidget);
		pluginContentLayout->addStretch();
		
		pluginScrollArea->setWidget(pluginContentContainer);
		pluginPageLayout->addWidget(pluginScrollArea);
		stackedWidget->addWidget(pluginPage);

		// 5. Hotkeys Page - embed the full hotkeys UI directly
		QWidget *hotkeysPage = new QWidget();
		QVBoxLayout *hotkeysPageLayout = new QVBoxLayout(hotkeysPage);
		hotkeysPageLayout->setContentsMargins(0, 0, 0, 0);
		hotkeysPageLayout->setSpacing(0);
		
		// Create scrollable container with dialog box styling
		QScrollArea *hotkeysScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
		hotkeysScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		hotkeysScrollArea->setStyleSheet(hotkeysScrollArea->styleSheet() + QString(
			"QScrollArea {"
			"    background-color: %1;"
			"    border: none;"
			"    border-radius: %2px;"
			"}"
			"QScrollArea > QWidget > QWidget {"
			"    background: transparent;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));
		
		// Create content container with dialog box background
		QWidget *hotkeysContentContainer = new QWidget();
		hotkeysContentContainer->setStyleSheet(QString(
			"QWidget {"
			"    background: transparent;"
			"}"
		));
		
		QVBoxLayout *hotkeysMainLayout = new QVBoxLayout(hotkeysContentContainer);
		hotkeysMainLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
						      StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		hotkeysMainLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);
		
		// Add hotkeys content directly
		QWidget *hotkeysContentWidget = new QWidget();
		QVBoxLayout *hotkeysContentLayout = new QVBoxLayout(hotkeysContentWidget);
		hotkeysContentLayout->setContentsMargins(0, 0, 0, 0);
		hotkeysContentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		// Define hotkey information structure
		struct HotkeyInfo {
			QString name;
			QString description;
			QString obsHotkeyName;
		};
		
		// Source Locking Hotkeys Section
		QGroupBox *lockingGroup = StreamUP::UIStyles::CreateStyledGroupBox("Source Locking", "info");
		QVBoxLayout *lockingLayout = new QVBoxLayout(lockingGroup);
		lockingLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		
		std::vector<HotkeyInfo> lockingHotkeys = {
			{obs_module_text("Hotkey.LockAllSources.Name"), obs_module_text("Hotkey.LockAllSources.Description"), "streamup_lock_all_sources"},
			{obs_module_text("Hotkey.LockCurrentSources.Name"), obs_module_text("Hotkey.LockCurrentSources.Description"), "streamup_lock_current_sources"}
		};
		
		// Refresh Operations Section
		QGroupBox *refreshGroup = StreamUP::UIStyles::CreateStyledGroupBox("Refresh Operations", "info");
		QVBoxLayout *refreshLayout = new QVBoxLayout(refreshGroup);
		refreshLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		
		std::vector<HotkeyInfo> refreshHotkeys = {
			{obs_module_text("Hotkey.RefreshBrowserSources.Name"), obs_module_text("Hotkey.RefreshBrowserSources.Description"), "streamup_refresh_browser_sources"},
			{obs_module_text("Hotkey.RefreshAudioMonitoring.Name"), obs_module_text("Hotkey.RefreshAudioMonitoring.Description"), "streamup_refresh_audio_monitoring"}
		};
		
		// Source Interaction Section
		QGroupBox *interactionGroup = StreamUP::UIStyles::CreateStyledGroupBox("Source Interaction", "info");
		QVBoxLayout *interactionLayout = new QVBoxLayout(interactionGroup);
		interactionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		
		std::vector<HotkeyInfo> interactionHotkeys = {
			{obs_module_text("Hotkey.OpenSourceProperties.Name"), obs_module_text("Hotkey.OpenSourceProperties.Description"), "streamup_open_source_properties"},
			{obs_module_text("Hotkey.OpenSourceFilters.Name"), obs_module_text("Hotkey.OpenSourceFilters.Description"), "streamup_open_source_filters"},
			{obs_module_text("Hotkey.OpenSourceInteract.Name"), obs_module_text("Hotkey.OpenSourceInteract.Description"), "streamup_open_source_interact"},
			{obs_module_text("Hotkey.OpenSceneFilters.Name"), obs_module_text("Hotkey.OpenSceneFilters.Description"), "streamup_open_scene_filters"}
		};
		
		// Video Capture Device Section
		QGroupBox *videoCaptureGroup = StreamUP::UIStyles::CreateStyledGroupBox("Video Capture Devices", "info");
		QVBoxLayout *videoCaptureLayout = new QVBoxLayout(videoCaptureGroup);
		videoCaptureLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		
		std::vector<HotkeyInfo> videoCaptureHotkeys = {
			{obs_module_text("Hotkey.ActivateVideoCaptureDevices.Name"), obs_module_text("Hotkey.ActivateVideoCaptureDevices.Description"), "streamup_activate_video_capture_devices"},
			{obs_module_text("Hotkey.DeactivateVideoCaptureDevices.Name"), obs_module_text("Hotkey.DeactivateVideoCaptureDevices.Description"), "streamup_deactivate_video_capture_devices"},
			{obs_module_text("Hotkey.RefreshVideoCaptureDevices.Name"), obs_module_text("Hotkey.RefreshVideoCaptureDevices.Description"), "streamup_refresh_video_capture_devices"}
		};
		
		// Helper function to build hotkey rows for each section
		auto buildHotkeySection = [](const std::vector<HotkeyInfo>& hotkeys, QVBoxLayout* parentLayout) {
			QVBoxLayout *sectionLayout = new QVBoxLayout();
			sectionLayout->setSpacing(0);
			sectionLayout->setContentsMargins(0, 0, 0, 0);
			
			for (size_t i = 0; i < hotkeys.size(); ++i) {
				const auto &hotkey = hotkeys[i];
				
				QWidget *hotkeyRow = new QWidget();
				hotkeyRow->setStyleSheet("QWidget { background: transparent; border: none; padding: 0px; }");
				
				QHBoxLayout *hotkeyRowLayout = new QHBoxLayout(hotkeyRow);
				hotkeyRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
				hotkeyRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
				
				// Text section
				QVBoxLayout *textLayout = new QVBoxLayout();
				textLayout->setSpacing(2);
				textLayout->setContentsMargins(0, 0, 0, 0);
				
				QLabel *nameLabel = new QLabel(hotkey.name);
				nameLabel->setStyleSheet(QString("QLabel { color: %1; font-size: %2px; font-weight: bold; background: transparent; }")
							.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
				
				QLabel *descLabel = new QLabel(hotkey.description);
				descLabel->setStyleSheet(QString("QLabel { color: %1; font-size: %2px; background: transparent; }")
							.arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
							.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
				descLabel->setWordWrap(true);
				
				textLayout->addWidget(nameLabel);
				textLayout->addWidget(descLabel);
				
				QWidget *textWrapper = new QWidget();
				QVBoxLayout *wrapperLayout = new QVBoxLayout(textWrapper);
				wrapperLayout->setContentsMargins(0, 0, 0, 0);
				wrapperLayout->addStretch();
				wrapperLayout->addLayout(textLayout);
				wrapperLayout->addStretch();
				
				hotkeyRowLayout->addWidget(textWrapper, 1);
				
				// Hotkey configuration widget
				StreamUP::UI::HotkeyWidget *hotkeyWidget = new StreamUP::UI::HotkeyWidget(hotkey.obsHotkeyName, hotkeyRow);
				
				// Load current hotkey binding
				obs_data_array_t *currentBinding = StreamUP::HotkeyManager::GetHotkeyBinding(hotkey.obsHotkeyName.toUtf8().constData());
				if (currentBinding) {
					hotkeyWidget->SetHotkey(currentBinding);
					obs_data_array_release(currentBinding);
				}
				
				// Connect to save changes
				QObject::connect(hotkeyWidget, &StreamUP::UI::HotkeyWidget::HotkeyChanged, 
					[](const QString &hotkeyName, obs_data_array_t *hotkeyData) {
						if (hotkeyData) {
							StreamUP::HotkeyManager::SetHotkeyBinding(hotkeyName.toUtf8().constData(), hotkeyData);
						} else {
							obs_data_array_t *emptyArray = obs_data_array_create();
							StreamUP::HotkeyManager::SetHotkeyBinding(hotkeyName.toUtf8().constData(), emptyArray);
							obs_data_array_release(emptyArray);
						}
					});
				
				hotkeyRowLayout->addWidget(hotkeyWidget);
				sectionLayout->addWidget(hotkeyRow);
				
				// Add separator line between hotkeys (but not after the last one)
				if (i < hotkeys.size() - 1) {
					QFrame *separator = new QFrame();
					separator->setFrameShape(QFrame::HLine);
					separator->setFrameShadow(QFrame::Plain);
					separator->setStyleSheet("QFrame { color: rgba(113, 128, 150, 0.3); background-color: rgba(113, 128, 150, 0.3); border: none; margin: 0px; max-height: 1px; }");
					sectionLayout->addWidget(separator);
				}
			}
			
			parentLayout->addLayout(sectionLayout);
		};
		
		// Build each hotkey section
		buildHotkeySection(lockingHotkeys, lockingLayout);
		buildHotkeySection(refreshHotkeys, refreshLayout);
		buildHotkeySection(interactionHotkeys, interactionLayout);
		buildHotkeySection(videoCaptureHotkeys, videoCaptureLayout);
		
		// Add all sections to main layout
		hotkeysContentLayout->addWidget(lockingGroup);
		hotkeysContentLayout->addWidget(refreshGroup);
		hotkeysContentLayout->addWidget(interactionGroup);
		hotkeysContentLayout->addWidget(videoCaptureGroup);
		
		hotkeysMainLayout->addWidget(hotkeysContentWidget);
		hotkeysMainLayout->addStretch();
		
		hotkeysScrollArea->setWidget(hotkeysContentContainer);
		hotkeysPageLayout->addWidget(hotkeysScrollArea);
		stackedWidget->addWidget(hotkeysPage);

		// 6. Dock Configuration Page - embed the full dock config UI directly
		QWidget *dockPage = new QWidget();
		QVBoxLayout *dockPageLayout = new QVBoxLayout(dockPage);
		dockPageLayout->setContentsMargins(0, 0, 0, 0);
		dockPageLayout->setSpacing(0);
		
		// Create scrollable container with dialog box styling
		QScrollArea *dockScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
		dockScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		dockScrollArea->setStyleSheet(dockScrollArea->styleSheet() + QString(
			"QScrollArea {"
			"    background-color: %1;"
			"    border: none;"
			"    border-radius: %2px;"
			"}"
			"QScrollArea > QWidget > QWidget {"
			"    background: transparent;"
			"}"
		).arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK));
		
		// Create content container with dialog box background
		QWidget *dockContentContainer = new QWidget();
		dockContentContainer->setStyleSheet(QString(
			"QWidget {"
			"    background: transparent;"
			"}"
		));
		
		QVBoxLayout *dockMainLayout = new QVBoxLayout(dockContentContainer);
		dockMainLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
						   StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
		dockMainLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);
		
		// Add dock config content directly
		QWidget *dockContentWidget = new QWidget();
		QVBoxLayout *dockContentLayout = new QVBoxLayout(dockContentWidget);
		dockContentLayout->setContentsMargins(0, 0, 0, 0);
		dockContentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		// Create GroupBox for dock tools using UI styles
		QGroupBox *toolsGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("Settings.Dock.ToolsGroupTitle"), "info");
		
		QVBoxLayout *toolsGroupLayout = new QVBoxLayout(toolsGroup);
		toolsGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0, StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0);
		toolsGroupLayout->setSpacing(0);
		
		// Get current dock settings
		DockToolSettings dockSettings = GetDockToolSettings();
		
		// Define tool information structure
		struct ToolInfo {
			QString name;
			QString description;
			bool *settingPtr;
			int toolIndex;
		};
		
		// List of all dock tools with pointers to their settings
		std::vector<ToolInfo> dockTools = {
			{obs_module_text("Dock.Tool.LockAllSources.Title"), obs_module_text("Dock.Tool.LockAllSources.Description"), &dockSettings.showLockAllSources, 0},
			{obs_module_text("Dock.Tool.LockCurrentSources.Title"), obs_module_text("Dock.Tool.LockCurrentSources.Description"), &dockSettings.showLockCurrentSources, 1},
			{obs_module_text("Dock.Tool.RefreshBrowserSources.Title"), obs_module_text("Dock.Tool.RefreshBrowserSources.Description"), &dockSettings.showRefreshBrowserSources, 2},
			{obs_module_text("Dock.Tool.RefreshAudioMonitoring.Title"), obs_module_text("Dock.Tool.RefreshAudioMonitoring.Description"), &dockSettings.showRefreshAudioMonitoring, 3},
			{obs_module_text("Dock.Tool.VideoCaptureOptions.Title"), obs_module_text("Dock.Tool.VideoCaptureOptions.Description"), &dockSettings.showVideoCaptureOptions, 4}
		};
		
		// Create tool rows
		for (size_t i = 0; i < dockTools.size(); ++i) {
			const auto &tool = dockTools[i];
			
			QWidget *toolRow = new QWidget();
			toolRow->setStyleSheet("QWidget { background: transparent; border: none; padding: 0px; }");
			
			QHBoxLayout *toolRowLayout = new QHBoxLayout(toolRow);
			toolRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
			toolRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
			
			// Text section
			QVBoxLayout *textLayout = new QVBoxLayout();
			textLayout->setSpacing(2);
			textLayout->setContentsMargins(0, 0, 0, 0);
			
			QLabel *nameLabel = new QLabel(tool.name);
			nameLabel->setStyleSheet(QString("QLabel { color: %1; font-size: %2px; font-weight: bold; background: transparent; }")
						.arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
						.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
			
			QLabel *descLabel = new QLabel(tool.description);
			descLabel->setStyleSheet(QString("QLabel { color: %1; font-size: %2px; background: transparent; }")
						.arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
						.arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
			descLabel->setWordWrap(true);
			
			textLayout->addWidget(nameLabel);
			textLayout->addWidget(descLabel);
			
			QWidget *textWrapper = new QWidget();
			QVBoxLayout *wrapperLayout = new QVBoxLayout(textWrapper);
			wrapperLayout->setContentsMargins(0, 0, 0, 0);
			wrapperLayout->addStretch();
			wrapperLayout->addLayout(textLayout);
			wrapperLayout->addStretch();
			
			toolRowLayout->addWidget(textWrapper, 1);
			
			// Switch section
			DockToolSettings freshSettings = GetDockToolSettings();
			bool currentValue = false;
			
			// Get the current value based on tool index
			switch (tool.toolIndex) {
				case 0: currentValue = freshSettings.showLockAllSources; break;
				case 1: currentValue = freshSettings.showLockCurrentSources; break;
				case 2: currentValue = freshSettings.showRefreshBrowserSources; break;
				case 3: currentValue = freshSettings.showRefreshAudioMonitoring; break;
				case 4: currentValue = freshSettings.showVideoCaptureOptions; break;
			}
			
			StreamUP::UIStyles::SwitchButton *toolSwitch = StreamUP::UIStyles::CreateStyledSwitch("", currentValue);
			
			// Update settings when switch changes
			QObject::connect(toolSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [toolIndex = tool.toolIndex](bool checked) {
				DockToolSettings settings = GetDockToolSettings();
				
				switch (toolIndex) {
					case 0: settings.showLockAllSources = checked; break;
					case 1: settings.showLockCurrentSources = checked; break;
					case 2: settings.showRefreshBrowserSources = checked; break;
					case 3: settings.showRefreshAudioMonitoring = checked; break;
					case 4: settings.showVideoCaptureOptions = checked; break;
				}
				
				UpdateDockToolSettings(settings);
			});
			
			toolRowLayout->addWidget(toolSwitch);
			toolsGroupLayout->addWidget(toolRow);
			
			// Add separator line between tools (but not after the last one)
			if (i < dockTools.size() - 1) {
				QFrame *separator = new QFrame();
				separator->setFrameShape(QFrame::HLine);
				separator->setFrameShadow(QFrame::Plain);
				separator->setStyleSheet("QFrame { color: rgba(113, 128, 150, 0.3); background-color: rgba(113, 128, 150, 0.3); border: none; margin: 0px; max-height: 1px; }");
				toolsGroupLayout->addWidget(separator);
			}
		}
		
		dockContentLayout->addWidget(toolsGroup);
		
		dockMainLayout->addWidget(dockContentWidget);
		dockMainLayout->addStretch();
		
		dockScrollArea->setWidget(dockContentContainer);
		dockPageLayout->addWidget(dockScrollArea);
		stackedWidget->addWidget(dockPage);
		
		// Connect category selection to page switching
		QObject::connect(categoryList, &QListWidget::currentRowChanged, [stackedWidget](int index) {
			stackedWidget->setCurrentIndex(index);
		});
		
		// Set the stacked widget to show the correct page for the selected tab
		stackedWidget->setCurrentIndex(tabIndex);

		// Set up the main layout
		contentLayout->addWidget(sidebarContainer);
		contentLayout->addWidget(stackedWidget, 1); // Give content area all the extra space

		QWidget *mainWidget = new QWidget();
		mainWidget->setLayout(contentLayout);
		mainLayout->addWidget(mainWidget);

		// Bottom button area with consistent background
		QWidget *buttonWidget = new QWidget();
		buttonWidget->setStyleSheet(QString("background: %1; padding: %2px;")
					    .arg(StreamUP::UIStyles::Colors::BG_DARKEST)
					    .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
		QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
		buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
						 StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
						 StreamUP::UIStyles::Sizes::PADDING_XL, 
						 StreamUP::UIStyles::Sizes::PADDING_MEDIUM);

		QPushButton *closeButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Close"), "neutral");
		QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

		buttonLayout->addStretch();
		buttonLayout->addWidget(closeButton);
		buttonLayout->addStretch();

		mainLayout->addWidget(buttonWidget);

		QObject::connect(dialog, &QDialog::finished, [=](int) {
			obs_data_release(settings);
			obs_properties_destroy(props);
		});

		// Apply consistent sizing that fits content without scrolling - use very generous height
		StreamUP::UIStyles::ApplyConsistentSizing(dialog, 650, 1000, 300, 1200);
		dialog->show();
		return dialog;
	});
}

bool AreNotificationsMuted()
{
	return notificationsMuted;
}

void SetNotificationsMuted(bool muted)
{
	notificationsMuted = muted;

	// Also update persistent settings
	PluginSettings settings = GetCurrentSettings();
	settings.notificationsMute = muted;
	UpdateSettings(settings);
}

bool IsCPHIntegrationEnabled()
{
	PluginSettings settings = GetCurrentSettings();
	return settings.showCPHIntegration;
}

void ShowInstalledPluginsInline(const StreamUP::UIStyles::StandardDialogComponents &components)
{
	// Store the current widget temporarily
	QWidget *currentWidget = components.scrollArea->takeWidget();
	(void)currentWidget; // Suppress unused variable warning

	// Keep the main header unchanged - only replace content below it

	// Create replacement content widget with sub-page header
	QWidget *pluginsWidget = new QWidget();
	pluginsWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
	pluginsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	QVBoxLayout *pluginsLayout = new QVBoxLayout(pluginsWidget);
	pluginsLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_SMALL, StreamUP::UIStyles::Sizes::PADDING_XL,
					  StreamUP::UIStyles::Sizes::PADDING_SMALL, StreamUP::UIStyles::Sizes::PADDING_XL);
	pluginsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

	// Create separate header section with same spacing as main header
	QWidget *headerSection = new QWidget();
	QVBoxLayout *headerSectionLayout = new QVBoxLayout(headerSection);
	headerSectionLayout->setContentsMargins(0, 0, 0, 0);
	headerSectionLayout->setSpacing(0); // Same as main header - no base spacing

	QLabel *titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("Settings.Plugin.InstalledPluginsTitle"));
	titleLabel->setAlignment(Qt::AlignCenter);
	headerSectionLayout->addWidget(titleLabel);

	// Add small spacing between title and description for readability
	//headerSectionLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);

	QLabel *descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("Settings.Plugin.InstalledPluginsDesc"));
	descLabel->setAlignment(Qt::AlignCenter);
	headerSectionLayout->addWidget(descLabel);

	pluginsLayout->addWidget(headerSection);

	// Info section - constrain width properly
	QLabel *infoLabel = new QLabel(obs_module_text("Settings.Plugin.InstalledPluginsInfo"));
	infoLabel->setStyleSheet(QString("QLabel {"
					 "color: %1;"
					 "font-size: %2px;"
					 "line-height: 1.3;"
					 "padding: %3px;"
					 "background: %4;"
					 "border: 1px solid %5;"
					 "border-radius: %6px;"
					 "}")
					 .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
					 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
					 .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2)
					 .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
					 .arg(StreamUP::UIStyles::Colors::BACKGROUND_HOVER)
					 .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
	infoLabel->setWordWrap(true);
	infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	pluginsLayout->addWidget(infoLabel);

	// Create plugin table
	QTableWidget *pluginTable = CreatePluginTable();

	// Add compatible plugins
	auto installedPlugins = StreamUP::PluginManager::GetInstalledPluginsCached();
	for (const auto &plugin : installedPlugins) {
		AddCompatiblePluginRow(pluginTable, plugin.first, plugin.second);
	}

	// Add incompatible plugins
	char *filePath = StreamUP::PathUtils::GetOBSLogPath();
	std::vector<std::string> incompatibleModules = StreamUP::PluginManager::SearchLoadedModulesInLogFile(filePath);
	bfree(filePath);

	for (const std::string &moduleName : incompatibleModules) {
		AddIncompatiblePluginRow(pluginTable, moduleName);
	}

	// Set appropriate height based on content
	int totalRows = pluginTable->rowCount();
	if (totalRows == 0) {
		// Show empty state
		QLabel *emptyLabel = new QLabel(obs_module_text("Settings.Plugin.InstalledPlugins"));
		emptyLabel->setAlignment(Qt::AlignCenter);
		emptyLabel->setStyleSheet(QString("QLabel {"
						  "color: %1;"
						  "font-size: %2px;"
						  "padding: 20px;"
						  "}")
						  .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
						  .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
		pluginsLayout->addWidget(emptyLabel);
	} else {
		// Auto-resize columns to fit content perfectly
		StreamUP::UIStyles::AutoResizeTableColumns(pluginTable);

		// Configure table height based on content
		int maxVisibleRows = std::min(totalRows, 8);
		int headerHeight = pluginTable->horizontalHeader()->height();
		int rowHeight = pluginTable->rowHeight(0);
		int tableHeight = headerHeight + (rowHeight * maxVisibleRows) + 10; // 10px padding

		pluginTable->setMinimumHeight(std::min(tableHeight, 300));
		pluginTable->setMaximumHeight(400);

		// Connect click handler for website links
		QObject::connect(pluginTable, &QTableWidget::cellClicked, [pluginTable](int row, int column) {
			StreamUP::UIStyles::HandleTableCellClick(pluginTable, row, column);
		});

		pluginsLayout->addWidget(pluginTable, 0, Qt::AlignTop);

		// Set container to accommodate table width plus minimal padding
		int tableWidth = pluginTable->minimumWidth();
		int containerWidth = tableWidth + (StreamUP::UIStyles::Sizes::PADDING_SMALL * 2);
		pluginsWidget->setMinimumWidth(containerWidth);
		// Remove maximum width constraint to allow UI expansion
	}
	pluginsLayout->addStretch();

	// Replace the content in the scroll area and configure for horizontal expansion
	components.scrollArea->setWidget(pluginsWidget);

	// Configure scroll area to expand horizontally to fit content
	components.scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	components.scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	components.scrollArea->setWidgetResizable(true);

	// Ensure the scroll area itself can expand and respect minimum sizes
	components.scrollArea->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

	// Fix header stretching issue by setting fixed size policy
	if (components.headerWidget) {
		components.headerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		components.headerWidget->setMaximumHeight(components.headerWidget->sizeHint().height());
	}

	// Adjust the main dialog size to accommodate the table
	if (components.dialog && pluginTable->rowCount() > 0) {
		int tableWidth = pluginTable->minimumWidth();
		int currentDialogWidth = components.dialog->width();
		int requiredWidth = tableWidth + 100; // Add padding for dialog margins

		if (requiredWidth > currentDialogWidth) {
			QSize currentSize = components.dialog->size();
			components.dialog->resize(requiredWidth, currentSize.height());
		}
	}

	// Force the scroll area to consider the widget's size hints
	components.scrollArea->updateGeometry();
}

void ShowInstalledPluginsPage(QWidget *parentWidget)
{
	StreamUP::UIHelpers::ShowDialogOnUIThread([parentWidget]() {
		auto installedPlugins = StreamUP::PluginManager::GetInstalledPluginsCached();

		QDialog *dialog =
			StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Plugin.InstalledPlugins"), parentWidget);

		// Start smaller - will be resized based on content
		dialog->resize(600, 500);

		QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
		mainLayout->setContentsMargins(0, 0, 0, 0);
		mainLayout->setSpacing(0);

		// Header section with title
		QWidget *headerWidget = new QWidget();
		headerWidget->setObjectName("headerWidget");
		headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px; }")
						    .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
						    .arg(StreamUP::UIStyles::Sizes::PADDING_XL));

		QVBoxLayout *headerLayout = new QVBoxLayout(headerWidget);
		headerLayout->setContentsMargins(0, 0, 0, 0);

		QLabel *titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("Settings.Plugin.InstalledPluginsTitle"));
		headerLayout->addWidget(titleLabel);

		QLabel *subtitleLabel =
			StreamUP::UIStyles::CreateStyledDescription(obs_module_text("Settings.Plugin.InstalledPluginsDesc"));
		headerLayout->addWidget(subtitleLabel);

		mainLayout->addWidget(headerWidget);

		// Content area - direct layout without scrolling
		QVBoxLayout *contentLayout = new QVBoxLayout();
		contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
						  StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
						  StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
						  StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
		contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		mainLayout->addLayout(contentLayout);

		// Info section - fit to UI width
		QLabel *infoLabel = new QLabel(obs_module_text("Settings.Plugin.InstalledPluginsInfo"));
		infoLabel->setStyleSheet(QString("QLabel {"
						 "color: %1;"
						 "font-size: %2px;"
						 "line-height: 1.3;"
						 "padding: %3px;"
						 "background: %4;"
						 "border: 1px solid %5;"
						 "border-radius: %6px;"
						 "}")
						 .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
						 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
						 .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2)
						 .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
						 .arg(StreamUP::UIStyles::Colors::BACKGROUND_HOVER)
						 .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
		infoLabel->setWordWrap(true);
		contentLayout->addWidget(infoLabel);

		// Create plugin table for the dialog
		QTableWidget *pluginTable = CreatePluginTable();

		// Add compatible plugins
		for (const auto &plugin : installedPlugins) {
			AddCompatiblePluginRow(pluginTable, plugin.first, plugin.second);
		}

		// Add incompatible plugins
		char *filePath = StreamUP::PathUtils::GetOBSLogPath();
		std::vector<std::string> incompatibleModules = StreamUP::PluginManager::SearchLoadedModulesInLogFile(filePath);
		bfree(filePath);

		for (const std::string &moduleName : incompatibleModules) {
			AddIncompatiblePluginRow(pluginTable, moduleName);
		}

		// Configure table for dialog - larger size and better layout
		int totalRows = pluginTable->rowCount();
		if (totalRows == 0) {
			// Show empty state
			QLabel *emptyLabel = new QLabel(obs_module_text("Settings.Plugin.InstalledPlugins"));
			emptyLabel->setAlignment(Qt::AlignCenter);
			emptyLabel->setStyleSheet(QString("QLabel {"
							  "color: %1;"
							  "font-size: %2px;"
							  "padding: 20px;"
							  "}")
							  .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
							  .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
			contentLayout->addWidget(emptyLabel);
		} else {
			// Auto-resize columns to fit content perfectly
			StreamUP::UIStyles::AutoResizeTableColumns(pluginTable);

			// Set appropriate height for dialog
			pluginTable->setMinimumHeight(300);
			pluginTable->setMaximumHeight(500);

			// Connect click handler for website links
			QObject::connect(pluginTable, &QTableWidget::cellClicked, [pluginTable](int row, int column) {
				StreamUP::UIStyles::HandleTableCellClick(pluginTable, row, column);
			});

			// Adjust dialog size to exactly fit table content
			int tableWidth = pluginTable->minimumWidth();
			int dialogWidth = std::max(tableWidth + 80, 600); // Minimal padding, lower minimum
			dialog->resize(dialogWidth, 650);

			contentLayout->addWidget(pluginTable);
		}

		// Bottom button area
		QWidget *buttonWidget = new QWidget();
		buttonWidget->setStyleSheet(QString("background: %1; padding: %2px;")
						    .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
						    .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
		QHBoxLayout *buttonLayout = new QHBoxLayout(buttonWidget);
		buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0,
						 StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0);

		QPushButton *updateButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("StreamUP.Settings.CheckForUpdate"), "info");
		QObject::connect(updateButton, &QPushButton::clicked, [dialog]() {
			StreamUP::PluginManager::ShowCachedPluginUpdatesDialog();
			dialog->close();
		});

		buttonLayout->addStretch();
		buttonLayout->addWidget(updateButton);

		mainLayout->addWidget(buttonWidget);

		dialog->setLayout(mainLayout);

		// Apply consistent sizing that adjusts to actual content size (also centers dialog after sizing)
		StreamUP::UIStyles::ApplyConsistentSizing(dialog, 650, 1000, 400, 800);
		dialog->show();
	});
}

void ShowHotkeysInline(const StreamUP::UIStyles::StandardDialogComponents &components)
{
	// Store the current widget temporarily
	QWidget *currentWidget = components.scrollArea->takeWidget();
	(void)currentWidget; // Suppress unused variable warning

	// Keep the main header unchanged - only replace content below it

	// Create replacement content widget with sub-page header
	QWidget *hotkeysWidget = new QWidget();
	hotkeysWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
	QVBoxLayout *hotkeysLayout = new QVBoxLayout(hotkeysWidget);
	hotkeysLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, StreamUP::UIStyles::Sizes::PADDING_XL,
					  StreamUP::UIStyles::Sizes::PADDING_XL + 5, StreamUP::UIStyles::Sizes::PADDING_XL);
	hotkeysLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

	// Create separate header section with same spacing as main header
	QWidget *headerSection = new QWidget();
	QVBoxLayout *headerSectionLayout = new QVBoxLayout(headerSection);
	headerSectionLayout->setContentsMargins(0, 0, 0, 0);
	headerSectionLayout->setSpacing(0); // Same as main header - no base spacing

	QLabel *titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("Settings.Hotkeys.Title"));
	titleLabel->setAlignment(Qt::AlignCenter);
	headerSectionLayout->addWidget(titleLabel);

	// Add small spacing between title and description for readability
	// headerSectionLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);

	QLabel *descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("Settings.Hotkeys.Description"));
	descLabel->setAlignment(Qt::AlignCenter);
	headerSectionLayout->addWidget(descLabel);

	hotkeysLayout->addWidget(headerSection);

	// Info section
	QLabel *infoLabel = new QLabel(obs_module_text("Settings.Hotkeys.Info"));
	infoLabel->setStyleSheet(QString("QLabel {"
					 "color: %1;"
					 "font-size: %2px;"
					 "line-height: 1.3;"
					 "padding: %3px;"
					 "background: %4;"
					 "border: 1px solid %5;"
					 "border-radius: %6px;"
					 "}")
					 .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
					 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
					 .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2)
					 .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
					 .arg(StreamUP::UIStyles::Colors::BACKGROUND_HOVER)
					 .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
	infoLabel->setWordWrap(true);
	hotkeysLayout->addWidget(infoLabel);

	// Create GroupBox for hotkeys
	QGroupBox *hotkeysGroup =
		StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("Settings.Hotkeys.GroupTitle"), "info");

	QVBoxLayout *hotkeysGroupLayout = new QVBoxLayout(hotkeysGroup);
	hotkeysGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

	// Define hotkey information structure
	struct HotkeyInfo {
		QString name;
		QString description;
		QString obsHotkeyName;
	};

	// List of all StreamUP hotkeys
	std::vector<HotkeyInfo> streamupHotkeys = {
		{obs_module_text("Hotkey.RefreshBrowserSources.Name"), obs_module_text("Hotkey.RefreshBrowserSources.Description"),
		 "streamup_refresh_browser_sources"},
		{obs_module_text("Hotkey.RefreshAudioMonitoring.Name"), obs_module_text("Hotkey.RefreshAudioMonitoring.Description"),
		 "streamup_refresh_audio_monitoring"},
		{obs_module_text("Hotkey.LockAllSources.Name"), obs_module_text("Hotkey.LockAllSources.Description"), "streamup_lock_all_sources"},
		{obs_module_text("Hotkey.LockCurrentSources.Name"), obs_module_text("Hotkey.LockCurrentSources.Description"),
		 "streamup_lock_current_sources"},
		{obs_module_text("Hotkey.OpenSourceProperties.Name"), obs_module_text("Hotkey.OpenSourceProperties.Description"),
		 "streamup_open_source_properties"},
		{obs_module_text("Hotkey.OpenSourceFilters.Name"), obs_module_text("Hotkey.OpenSourceFilters.Description"),
		 "streamup_open_source_filters"},
		{obs_module_text("Hotkey.OpenSourceInteract.Name"), obs_module_text("Hotkey.OpenSourceInteract.Description"),
		 "streamup_open_source_interact"},
		{obs_module_text("Hotkey.OpenSceneFilters.Name"), obs_module_text("Hotkey.OpenSceneFilters.Description"),
		 "streamup_open_scene_filters"},
		{obs_module_text("Hotkey.ActivateVideoCaptureDevices.Name"), obs_module_text("Hotkey.ActivateVideoCaptureDevices.Description"),
		 "streamup_activate_video_capture_devices"},
		{obs_module_text("Hotkey.DeactivateVideoCaptureDevices.Name"), obs_module_text("Hotkey.DeactivateVideoCaptureDevices.Description"),
		 "streamup_deactivate_video_capture_devices"},
		{obs_module_text("Hotkey.RefreshVideoCaptureDevices.Name"), obs_module_text("Hotkey.RefreshVideoCaptureDevices.Description"),
		 "streamup_refresh_video_capture_devices"}};

	// Create direct layout for hotkeys (no scrolling, fit to content)
	QVBoxLayout *hotkeyContentLayout = new QVBoxLayout();
	hotkeyContentLayout->setSpacing(0);                  // No spacing, separators will handle it
	hotkeyContentLayout->setContentsMargins(0, 0, 0, 0); // No padding inside the box (like WebSocket UI)

	// Add each hotkey as a row with actual hotkey widget (WebSocket UI style)
	for (size_t i = 0; i < streamupHotkeys.size(); ++i) {
		const auto &hotkey = streamupHotkeys[i];

		QWidget *hotkeyRow = new QWidget();
		// Use transparent background with no border (like WebSocket UI)
		hotkeyRow->setStyleSheet(QString("QWidget {"
						 "background: transparent;"
						 "border: none;"
						 "padding: 0px;"
						 "}"));

		QHBoxLayout *hotkeyRowLayout = new QHBoxLayout(hotkeyRow);
		// Match WebSocket UI margins: tight vertical padding, medium horizontal spacing
		hotkeyRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0,
						    StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
		hotkeyRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		// Text section - vertical layout with tight spacing (like WebSocket UI)
		QVBoxLayout *textLayout = new QVBoxLayout();
		textLayout->setSpacing(2); // Very tight spacing between name and description
		textLayout->setContentsMargins(0, 0, 0, 0);

		// Hotkey name - use same styling as WebSocket UI
		QLabel *nameLabel = new QLabel(hotkey.name);
		nameLabel->setStyleSheet(QString("QLabel {"
						 "color: %1;"
						 "font-size: %2px;"
						 "font-weight: bold;"
						 "background: transparent;"
						 "border: none;"
						 "margin: 0px;"
						 "padding: 0px;"
						 "}")
						 .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
						 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));

		// Hotkey description - use same styling as WebSocket UI
		QLabel *descLabel = new QLabel(hotkey.description);
		descLabel->setStyleSheet(QString("QLabel {"
						 "color: %1;"
						 "font-size: %2px;"
						 "background: transparent;"
						 "border: none;"
						 "margin: 0px;"
						 "padding: 0px;"
						 "}")
						 .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
						 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
		descLabel->setWordWrap(true);

		textLayout->addWidget(nameLabel);
		textLayout->addWidget(descLabel);

		// Create a wrapper widget for text info that centers the content vertically (like WebSocket UI)
		QWidget *textWrapper = new QWidget();
		QVBoxLayout *wrapperLayout = new QVBoxLayout(textWrapper);
		wrapperLayout->setContentsMargins(0, 0, 0, 0);
		wrapperLayout->addStretch(); // Add stretch above
		wrapperLayout->addLayout(textLayout);
		wrapperLayout->addStretch(); // Add stretch below

		hotkeyRowLayout->addWidget(textWrapper, 1);

		// Hotkey configuration widget section - also center vertically
		QVBoxLayout *hotkeyWrapperLayout = new QVBoxLayout();
		hotkeyWrapperLayout->setContentsMargins(0, 0, 0, 0);
		hotkeyWrapperLayout->addStretch(); // Add stretch above

		StreamUP::UI::HotkeyWidget *hotkeyWidget = new StreamUP::UI::HotkeyWidget(hotkey.obsHotkeyName, hotkeyRow);

		// Load current hotkey binding
		obs_data_array_t *currentBinding =
			StreamUP::HotkeyManager::GetHotkeyBinding(hotkey.obsHotkeyName.toUtf8().constData());
		if (currentBinding) {
			hotkeyWidget->SetHotkey(currentBinding);
			obs_data_array_release(currentBinding);
		}

		// Connect to save changes immediately
		QObject::connect(
			hotkeyWidget, &StreamUP::UI::HotkeyWidget::HotkeyChanged,
			[](const QString &hotkeyName, obs_data_array_t *hotkeyData) {
				if (hotkeyData) {
					StreamUP::HotkeyManager::SetHotkeyBinding(hotkeyName.toUtf8().constData(), hotkeyData);
				} else {
					// Clear the hotkey
					obs_data_array_t *emptyArray = obs_data_array_create();
					StreamUP::HotkeyManager::SetHotkeyBinding(hotkeyName.toUtf8().constData(), emptyArray);
					obs_data_array_release(emptyArray);
				}
			});

		hotkeyWrapperLayout->addWidget(hotkeyWidget);
		hotkeyWrapperLayout->addStretch(); // Add stretch below

		hotkeyRowLayout->addLayout(hotkeyWrapperLayout);

		hotkeyContentLayout->addWidget(hotkeyRow);

		// Add separator line between hotkeys (but not after the last one) - like WebSocket UI
		if (i < streamupHotkeys.size() - 1) {
			QFrame *separator = new QFrame();
			separator->setFrameShape(QFrame::HLine);
			separator->setFrameShadow(QFrame::Plain);
			separator->setStyleSheet(QString("QFrame {"
							 "color: rgba(113, 128, 150, 0.3);"
							 "background-color: rgba(113, 128, 150, 0.3);"
							 "border: none;"
							 "margin: 0px;"
							 "max-height: 1px;"
							 "}"));
			hotkeyContentLayout->addWidget(separator);
		}
	}

	hotkeysGroupLayout->addLayout(hotkeyContentLayout);

	// Action buttons section
	QHBoxLayout *actionLayout = new QHBoxLayout();
	actionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

	QPushButton *resetButton =
		StreamUP::UIStyles::CreateStyledButton(obs_module_text("Settings.Hotkeys.ResetAll"), "error");

	// Store all hotkey widgets so we can refresh them after reset
	QList<StreamUP::UI::HotkeyWidget *> allHotkeyWidgets;

	// Collect all hotkey widgets from the layout
	for (int i = 0; i < hotkeyContentLayout->count(); i++) {
		QLayoutItem *item = hotkeyContentLayout->itemAt(i);
		if (item && item->widget()) {
			QWidget *widget = item->widget();
			// Skip separators (QFrame), only process hotkey rows
			if (QString(widget->metaObject()->className()) != "QFrame") {
				StreamUP::UI::HotkeyWidget *hotkeyWidget = widget->findChild<StreamUP::UI::HotkeyWidget *>();
				if (hotkeyWidget) {
					allHotkeyWidgets.append(hotkeyWidget);
				}
			}
		}
	}

	QObject::connect(resetButton, &QPushButton::clicked, [allHotkeyWidgets]() {
		// Show confirmation dialog for reset
		StreamUP::UIHelpers::ShowDialogOnUIThread([allHotkeyWidgets]() {
			QDialog *confirmDialog =
				StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Hotkeys.ResetTitle"));
			confirmDialog->resize(400, 200);

			QVBoxLayout *layout = new QVBoxLayout(confirmDialog);

			QLabel *warningLabel = new QLabel(obs_module_text("Settings.Hotkeys.ResetWarning"));
			warningLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: %3px;")
							    .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							    .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
							    .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
			warningLabel->setWordWrap(true);
			warningLabel->setAlignment(Qt::AlignCenter);

			layout->addWidget(warningLabel);

			QHBoxLayout *buttonLayout = new QHBoxLayout();

			QPushButton *cancelBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Cancel"), "neutral");
			QPushButton *resetBtn = StreamUP::UIStyles::CreateStyledButton(
				obs_module_text("Settings.Hotkeys.ResetButton"), "error");

			QObject::connect(cancelBtn, &QPushButton::clicked, confirmDialog, &QDialog::close);
			QObject::connect(resetBtn, &QPushButton::clicked, [confirmDialog, allHotkeyWidgets]() {
				// Clear all StreamUP hotkeys
				StreamUP::HotkeyManager::ResetAllHotkeys();

				// Refresh all hotkey widgets in the UI
				for (StreamUP::UI::HotkeyWidget *widget : allHotkeyWidgets) {
					if (widget) {
						widget->ClearHotkey();
					}
				}

				confirmDialog->close();
			});

			buttonLayout->addStretch();
			buttonLayout->addWidget(cancelBtn);
			buttonLayout->addWidget(resetBtn);

			layout->addLayout(buttonLayout);

			confirmDialog->show();
			StreamUP::UIHelpers::CenterDialog(confirmDialog);
		});
	});

	actionLayout->addStretch();
	actionLayout->addWidget(resetButton);

	hotkeysGroupLayout->addLayout(actionLayout);
	hotkeysLayout->addWidget(hotkeysGroup);
	hotkeysLayout->addStretch();

	// Replace the content in the scroll area
	components.scrollArea->setWidget(hotkeysWidget);

	// Don't resize dialog when switching to sub-menus to maintain consistent header appearance
}

DockToolSettings GetDockToolSettings()
{
	PluginSettings settings = GetCurrentSettings();
	return settings.dockTools;
}

void UpdateDockToolSettings(const DockToolSettings &dockSettings)
{
	PluginSettings settings = GetCurrentSettings();
	settings.dockTools = dockSettings;
	UpdateSettings(settings);

	// Notify all dock instances to update their visibility
	StreamUPDock::NotifyAllDocksSettingsChanged();
}

void ShowDockConfigInline(const StreamUP::UIStyles::StandardDialogComponents &components)
{
	// Store the current widget temporarily
	QWidget *currentWidget = components.scrollArea->takeWidget();
	(void)currentWidget; // Suppress unused variable warning

	// Keep the main header unchanged - only replace content below it

	// Create replacement content widget with sub-page header
	QWidget *dockConfigWidget = new QWidget();
	dockConfigWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
	QVBoxLayout *dockConfigLayout = new QVBoxLayout(dockConfigWidget);
	dockConfigLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, StreamUP::UIStyles::Sizes::PADDING_XL,
					     StreamUP::UIStyles::Sizes::PADDING_XL + 5, StreamUP::UIStyles::Sizes::PADDING_XL);
	dockConfigLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

	// Create separate header section with same spacing as main header
	QWidget *headerSection = new QWidget();
	QVBoxLayout *headerSectionLayout = new QVBoxLayout(headerSection);
	headerSectionLayout->setContentsMargins(0, 0, 0, 0);
	headerSectionLayout->setSpacing(0); // Same as main header - no base spacing

	QLabel *titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("Settings.Dock.Title"));
	titleLabel->setAlignment(Qt::AlignCenter);
	headerSectionLayout->addWidget(titleLabel);

	// Add small spacing between title and description for readability
	// headerSectionLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);

	QLabel *descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("Settings.Dock.Description"));
	descLabel->setAlignment(Qt::AlignCenter);
	headerSectionLayout->addWidget(descLabel);

	dockConfigLayout->addWidget(headerSection);

	// Info section
	QLabel *infoLabel = new QLabel(obs_module_text("Settings.Dock.Info"));
	infoLabel->setStyleSheet(QString("QLabel {"
					 "color: %1;"
					 "font-size: %2px;"
					 "line-height: 1.3;"
					 "padding: %3px;"
					 "background: %4;"
					 "border: 1px solid %5;"
					 "border-radius: %6px;"
					 "}")
					 .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
					 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
					 .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2)
					 .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
					 .arg(StreamUP::UIStyles::Colors::BACKGROUND_HOVER)
					 .arg(StreamUP::UIStyles::Sizes::BORDER_RADIUS));
	infoLabel->setWordWrap(true);
	dockConfigLayout->addWidget(infoLabel);

	// Create GroupBox for dock tools configuration
	QGroupBox *toolsGroup =
		StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("Settings.Dock.ToolsGroupTitle"), "info");

	QVBoxLayout *toolsGroupLayout = new QVBoxLayout(toolsGroup);
	toolsGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
					     0, // No top padding
					     StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
					     0); // No bottom padding
	toolsGroupLayout->setSpacing(0);         // No spacing since we handle it in the widget padding

	// Get current dock settings
	DockToolSettings currentDockSettings = GetDockToolSettings();

	// Define tool information structure
	struct ToolInfo {
		QString name;
		QString description;
		bool *settingPtr;
		int toolIndex; // Add index to identify which tool this is
	};

	// List of all dock tools with pointers to their settings
	std::vector<ToolInfo> dockTools = {
		{obs_module_text("Dock.Tool.LockAllSources.Title"), obs_module_text("Dock.Tool.LockAllSources.Description"),
		 &currentDockSettings.showLockAllSources, 0},
		{obs_module_text("Dock.Tool.LockCurrentSources.Title"), obs_module_text("Dock.Tool.LockCurrentSources.Description"),
		 &currentDockSettings.showLockCurrentSources, 1},
		{obs_module_text("Dock.Tool.RefreshBrowserSources.Title"), obs_module_text("Dock.Tool.RefreshBrowserSources.Description"),
		 &currentDockSettings.showRefreshBrowserSources, 2},
		{obs_module_text("Dock.Tool.RefreshAudioMonitoring.Title"), obs_module_text("Dock.Tool.RefreshAudioMonitoring.Description"),
		 &currentDockSettings.showRefreshAudioMonitoring, 3},
		{obs_module_text("Dock.Tool.VideoCaptureOptions.Title"), obs_module_text("Dock.Tool.VideoCaptureOptions.Description"),
		 &currentDockSettings.showVideoCaptureOptions, 4}};

	// Create tool rows matching WebSocket/hotkeys UI pattern
	for (size_t i = 0; i < dockTools.size(); ++i) {
		const auto &tool = dockTools[i];

		QWidget *toolRow = new QWidget();
		toolRow->setStyleSheet(QString("QWidget {"
					       "background: transparent;"
					       "border: none;"
					       "padding: 0px;"
					       "}"));

		QHBoxLayout *toolRowLayout = new QHBoxLayout(toolRow);
		toolRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0,
						  StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
		toolRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

		// Text section - vertical layout with tight spacing (like WebSocket/hotkeys UI)
		QVBoxLayout *textLayout = new QVBoxLayout();
		textLayout->setSpacing(2); // Very tight spacing between name and description
		textLayout->setContentsMargins(0, 0, 0, 0);

		// Tool name - use same styling as WebSocket/hotkeys UI
		QLabel *nameLabel = new QLabel(tool.name);
		nameLabel->setStyleSheet(QString("QLabel {"
						 "color: %1;"
						 "font-size: %2px;"
						 "font-weight: bold;"
						 "background: transparent;"
						 "border: none;"
						 "margin: 0px;"
						 "padding: 0px;"
						 "}")
						 .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
						 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));

		// Tool description - use same styling as WebSocket/hotkeys UI
		QLabel *descLabel = new QLabel(tool.description);
		descLabel->setStyleSheet(QString("QLabel {"
						 "color: %1;"
						 "font-size: %2px;"
						 "background: transparent;"
						 "border: none;"
						 "margin: 0px;"
						 "padding: 0px;"
						 "}")
						 .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
						 .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
		descLabel->setWordWrap(true);

		textLayout->addWidget(nameLabel);
		textLayout->addWidget(descLabel);

		// Create a wrapper widget for text info that centers the content vertically (like WebSocket/hotkeys UI)
		QWidget *textWrapper = new QWidget();
		QVBoxLayout *wrapperLayout = new QVBoxLayout(textWrapper);
		wrapperLayout->setContentsMargins(0, 0, 0, 0);
		wrapperLayout->addStretch(); // Add stretch above
		wrapperLayout->addLayout(textLayout);
		wrapperLayout->addStretch(); // Add stretch below

		toolRowLayout->addWidget(textWrapper, 1);

		// Switch section - also center vertically
		QVBoxLayout *switchWrapperLayout = new QVBoxLayout();
		switchWrapperLayout->setContentsMargins(0, 0, 0, 0);
		switchWrapperLayout->addStretch(); // Add stretch above

		// Force fresh settings load every time to ensure we have the latest values
		DockToolSettings freshSettings = GetDockToolSettings();
		bool currentValue = false;

		// Get the current value based on tool index
		switch (tool.toolIndex) {
		case 0:
			currentValue = freshSettings.showLockAllSources;
			break;
		case 1:
			currentValue = freshSettings.showLockCurrentSources;
			break;
		case 2:
			currentValue = freshSettings.showRefreshBrowserSources;
			break;
		case 3:
			currentValue = freshSettings.showRefreshAudioMonitoring;
			break;
		case 4:
			currentValue = freshSettings.showVideoCaptureOptions;
			break;
		}

		// Create switch with explicit initial state
		StreamUP::UIStyles::SwitchButton *toolSwitch = StreamUP::UIStyles::CreateStyledSwitch("", currentValue);

		// Force a visual refresh after the UI is fully constructed
		QTimer::singleShot(0, [toolSwitch, currentValue]() { toolSwitch->setChecked(currentValue); });

		// Update settings immediately when switch changes
		QObject::connect(toolSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [toolIndex = tool.toolIndex](bool checked) {
			// Get current settings from persistent storage
			DockToolSettings settings = GetDockToolSettings();

			// Update the specific setting based on tool index
			switch (toolIndex) {
			case 0:
				settings.showLockAllSources = checked;
				break;
			case 1:
				settings.showLockCurrentSources = checked;
				break;
			case 2:
				settings.showRefreshBrowserSources = checked;
				break;
			case 3:
				settings.showRefreshAudioMonitoring = checked;
				break;
			case 4:
				settings.showVideoCaptureOptions = checked;
				break;
			}

			// Save the updated settings immediately (this calls NotifyAllDocksSettingsChanged)
			UpdateDockToolSettings(settings);
		});

		switchWrapperLayout->addWidget(toolSwitch);
		switchWrapperLayout->addStretch(); // Add stretch below

		toolRowLayout->addLayout(switchWrapperLayout);

		toolsGroupLayout->addWidget(toolRow);

		// Add separator line between tools (but not after the last one) - like WebSocket/hotkeys UI
		if (i < dockTools.size() - 1) {
			QFrame *separator = new QFrame();
			separator->setFrameShape(QFrame::HLine);
			separator->setFrameShadow(QFrame::Plain);
			separator->setStyleSheet(QString("QFrame {"
							 "color: rgba(113, 128, 150, 0.3);"
							 "background-color: rgba(113, 128, 150, 0.3);"
							 "border: none;"
							 "margin: 0px;"
							 "max-height: 1px;"
							 "}"));
			toolsGroupLayout->addWidget(separator);
		}
	}

	// Action buttons section - only reset button, no save (like hotkeys UI)
	QHBoxLayout *actionLayout = new QHBoxLayout();
	actionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
	actionLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0,
					 StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);

	QPushButton *resetButton =
		StreamUP::UIStyles::CreateStyledButton(obs_module_text("Settings.Dock.ResetConfig"), "error");

	// Store all switches so we can update them after reset
	QList<StreamUP::UIStyles::SwitchButton *> allSwitches;
	for (int i = 0; i < toolsGroupLayout->count(); i++) {
		QLayoutItem *item = toolsGroupLayout->itemAt(i);
		if (item && item->widget()) {
			QWidget *widget = item->widget();
			if (QString(widget->metaObject()->className()) != "QFrame") {
				StreamUP::UIStyles::SwitchButton *switchButton =
					widget->findChild<StreamUP::UIStyles::SwitchButton *>();
				if (switchButton) {
					allSwitches.append(switchButton);
				}
			}
		}
	}

	QObject::connect(resetButton, &QPushButton::clicked, [allSwitches]() {
		// Show confirmation dialog for reset (matching hotkeys pattern)
		StreamUP::UIHelpers::ShowDialogOnUIThread([allSwitches]() {
			QDialog *confirmDialog =
				StreamUP::UIStyles::CreateStyledDialog(obs_module_text("Settings.Dock.ResetTitle"));
			confirmDialog->resize(400, 200);

			QVBoxLayout *layout = new QVBoxLayout(confirmDialog);

			QLabel *warningLabel = new QLabel(obs_module_text("Settings.Dock.ResetWarning"));
			warningLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: %3px;")
							    .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
							    .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
							    .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
			warningLabel->setWordWrap(true);
			warningLabel->setAlignment(Qt::AlignCenter);

			layout->addWidget(warningLabel);

			QHBoxLayout *buttonLayout = new QHBoxLayout();

			QPushButton *cancelBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Cancel"), "neutral");
			QPushButton *resetBtn = StreamUP::UIStyles::CreateStyledButton(
				obs_module_text("Settings.Dock.ResetButton"), "error");

			QObject::connect(cancelBtn, &QPushButton::clicked, confirmDialog, &QDialog::close);
			QObject::connect(resetBtn, &QPushButton::clicked, [confirmDialog, allSwitches]() {
				// Reset all dock tools to default (visible)
				DockToolSettings defaultSettings;
				UpdateDockToolSettings(defaultSettings);

				// Update all switches to checked state
				for (StreamUP::UIStyles::SwitchButton *switchButton : allSwitches) {
					if (switchButton) {
						switchButton->setChecked(true);
					}
				}

				confirmDialog->close();
			});

			buttonLayout->addStretch();
			buttonLayout->addWidget(cancelBtn);
			buttonLayout->addWidget(resetBtn);

			layout->addLayout(buttonLayout);

			confirmDialog->show();
			StreamUP::UIHelpers::CenterDialog(confirmDialog);
		});
	});

	actionLayout->addStretch();
	actionLayout->addWidget(resetButton);

	toolsGroupLayout->addLayout(actionLayout);
	dockConfigLayout->addWidget(toolsGroup);
	dockConfigLayout->addStretch();

	// Replace the content in the scroll area
	components.scrollArea->setWidget(dockConfigWidget);

	// Don't resize dialog when switching to sub-menus to maintain consistent header appearance
}

bool IsDebugLoggingEnabled()
{
	PluginSettings settings = GetCurrentSettings();
	return settings.debugLoggingEnabled;
}

void SetDebugLoggingEnabled(bool enabled)
{
	PluginSettings settings = GetCurrentSettings();
	settings.debugLoggingEnabled = enabled;
	UpdateSettings(settings);
}

void InvalidateSettingsCache()
{
	std::lock_guard<std::mutex> lock(settingsCacheMutex);
	if (cachedSettings) {
		obs_data_release(cachedSettings);
		cachedSettings = nullptr;
	}
}

void CleanupSettingsCache()
{
	std::lock_guard<std::mutex> lock(settingsCacheMutex);
	// Release cached settings on plugin shutdown
	if (cachedSettings) {
		obs_data_release(cachedSettings);
		cachedSettings = nullptr;
	}
	settingsLoadLogged = false;
}

} // namespace SettingsManager
} // namespace StreamUP
