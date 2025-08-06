#include "settings-manager.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "switch-button.hpp"
#include "plugin-manager.hpp"
#include "plugin-state.hpp"
#include "hotkey-manager.hpp"
#include "hotkey-widget.hpp"
#include "dock/streamup-dock.hpp"
#include <obs-module.h>
#include <obs-properties.h>
#include <obs-frontend-api.h>
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
#include <memory>
#include <util/platform.h>

// Forward declarations for functions that may need to be moved from streamup.cpp
extern QString GetForumLink(const std::string &pluginName);
extern void SetLabelWithSortedModules(QLabel *label, const std::vector<std::string> &moduleNames);
extern std::vector<std::string> SearchModulesInFile(const char *path);
extern char *GetFilePath();

namespace StreamUP {
namespace SettingsManager {

// Global settings state
static bool notificationsMuted = false;

// Helper function to extract domain from URL
QString ExtractDomain(const QString& url) {
    QUrl qurl(url);
    QString host = qurl.host();
    if (host.startsWith("www.")) {
        host = host.mid(4); // Remove "www."
    }
    return host.isEmpty() ? url : host;
}


// Helper function to create styled plugin table
QTableWidget* CreatePluginTable() {
    QTableWidget* table = StreamUP::UIStyles::CreateStyledTableWidget();
    
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
void AddCompatiblePluginRow(QTableWidget* table, const std::string& pluginName, const std::string& version) {
    const auto& allPlugins = StreamUP::GetAllPlugins();
    auto it = allPlugins.find(pluginName);
    
    int row = table->rowCount();
    table->insertRow(row);
    
    // Status column - Compatible
    QTableWidgetItem* statusItem = new QTableWidgetItem("✅ Compatible");
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
    QString forumLink = GetForumLink(pluginName);
    QString domainName = ExtractDomain(forumLink);
    QTableWidgetItem* websiteItem = new QTableWidgetItem(domainName);
    websiteItem->setForeground(QColor(StreamUP::UIStyles::Colors::INFO));
    websiteItem->setData(Qt::UserRole, forumLink);
    table->setItem(row, 4, websiteItem);
}

// Helper function to add incompatible plugin row  
void AddIncompatiblePluginRow(QTableWidget* table, const std::string& moduleName) {
    int row = table->rowCount();
    table->insertRow(row);
    
    // Status column - Incompatible
    QTableWidgetItem* statusItem = new QTableWidgetItem("❌ Incompatible");
    statusItem->setForeground(QColor(StreamUP::UIStyles::Colors::ERROR));
    table->setItem(row, 0, statusItem);
    
    // Plugin Name column - N/A for incompatible
    QTableWidgetItem* nameItem = new QTableWidgetItem("N/A");
    nameItem->setForeground(QColor(StreamUP::UIStyles::Colors::TEXT_MUTED));
    table->setItem(row, 1, nameItem);
    
    // Module Name column
    table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(moduleName)));
    
    // Version column - N/A for incompatible
    QTableWidgetItem* versionItem = new QTableWidgetItem("N/A");
    versionItem->setForeground(QColor(StreamUP::UIStyles::Colors::TEXT_MUTED));
    table->setItem(row, 3, versionItem);
    
    // Website column - N/A for incompatible
    QTableWidgetItem* websiteItem = new QTableWidgetItem("N/A");
    websiteItem->setForeground(QColor(StreamUP::UIStyles::Colors::TEXT_MUTED));
    table->setItem(row, 4, websiteItem);
}


// Static pointer to track open settings dialog
static QPointer<QDialog> settingsDialog;

obs_data_t* LoadSettings()
{
    char* configPath = obs_module_config_path("configs.json");
    obs_data_t* data = obs_data_create_from_json_file(configPath);
    
    if (!data) {
        blog(LOG_INFO, "[StreamUP] Settings not found. Creating default settings...");
        char* config_path = obs_module_config_path("");
        if (config_path) {
            os_mkdirs(config_path);
            bfree(config_path);
        }
        
        data = obs_data_create();
        obs_data_set_bool(data, "run_at_startup", true);
        obs_data_set_bool(data, "notifications_mute", false);
        obs_data_set_bool(data, "show_cph_integration", true);
        
        // Set default dock tool settings
        obs_data_t* dockData = obs_data_create();
        obs_data_set_bool(dockData, "show_lock_all_sources", true);
        obs_data_set_bool(dockData, "show_lock_current_sources", true);
        obs_data_set_bool(dockData, "show_refresh_browser_sources", true);
        obs_data_set_bool(dockData, "show_refresh_audio_monitoring", true);
        obs_data_set_bool(dockData, "show_video_capture_options", true);
        obs_data_set_obj(data, "dock_tools", dockData);
        obs_data_release(dockData);
        
        if (obs_data_save_json(data, configPath)) {
            blog(LOG_INFO, "[StreamUP] Default settings saved to %s", configPath);
        } else {
            blog(LOG_WARNING, "[StreamUP] Failed to save default settings to file.");
        }
    } else {
        blog(LOG_INFO, "[StreamUP] Settings loaded successfully from %s", configPath);
    }
    
    bfree(configPath);
    return data;
}

bool SaveSettings(obs_data_t* settings)
{
    char* configPath = obs_module_config_path("configs.json");
    bool success = false;
    
    if (obs_data_save_json(settings, configPath)) {
        blog(LOG_INFO, "[StreamUP] Settings saved to %s", configPath);
        success = true;
    } else {
        blog(LOG_WARNING, "[StreamUP] Failed to save settings to file.");
    }
    
    bfree(configPath);
    return success;
}

PluginSettings GetCurrentSettings()
{
    PluginSettings settings;
    obs_data_t* data = LoadSettings();
    
    if (data) {
        settings.runAtStartup = obs_data_get_bool(data, "run_at_startup");
        settings.notificationsMute = obs_data_get_bool(data, "notifications_mute");
        settings.showCPHIntegration = obs_data_get_bool(data, "show_cph_integration");
        
        // Load dock tool settings
        obs_data_t* dockData = obs_data_get_obj(data, "dock_tools");
        if (dockData) {
            settings.dockTools.showLockAllSources = obs_data_get_bool(dockData, "show_lock_all_sources");
            settings.dockTools.showLockCurrentSources = obs_data_get_bool(dockData, "show_lock_current_sources");
            settings.dockTools.showRefreshBrowserSources = obs_data_get_bool(dockData, "show_refresh_browser_sources");
            settings.dockTools.showRefreshAudioMonitoring = obs_data_get_bool(dockData, "show_refresh_audio_monitoring");
            settings.dockTools.showVideoCaptureOptions = obs_data_get_bool(dockData, "show_video_capture_options");
            
            obs_data_release(dockData);
        } else {
            blog(LOG_INFO, "[StreamUP Settings] No dock_tools data found, using defaults");
        }
        
        obs_data_release(data);
    }
    
    return settings;
}

void UpdateSettings(const PluginSettings& settings)
{
    obs_data_t* data = obs_data_create();
    obs_data_set_bool(data, "run_at_startup", settings.runAtStartup);
    obs_data_set_bool(data, "notifications_mute", settings.notificationsMute);
    obs_data_set_bool(data, "show_cph_integration", settings.showCPHIntegration);
    
    // Save dock tool settings
    obs_data_t* dockData = obs_data_create();
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
    obs_data_t* settings = LoadSettings();
    
    if (settings) {
        bool runAtStartup = obs_data_get_bool(settings, "run_at_startup");
        blog(LOG_INFO, "[StreamUP] Run at startup setting: %s", runAtStartup ? "true" : "false");
        
        notificationsMuted = obs_data_get_bool(settings, "notifications_mute");
        blog(LOG_INFO, "[StreamUP] Notifications mute setting: %s", notificationsMuted ? "true" : "false");
        
        obs_data_release(settings);
    } else {
        blog(LOG_WARNING, "[StreamUP] Failed to load settings in initialization.");
    }
}

void ShowSettingsDialog()
{
    // Check if settings dialog is already open
    if (!settingsDialog.isNull() && settingsDialog->isVisible()) {
        // Bring existing dialog to front
        settingsDialog->raise();
        settingsDialog->activateWindow();
        return;
    }

    StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
        obs_data_t* settings = LoadSettings();
        if (!settings) {
            return;
        }

        // Create standardized dialog using template system
        auto components = StreamUP::UIStyles::CreateStandardDialog(
            obs_module_text("WindowSettingsTitle"),
            obs_module_text("WindowSettingsMainTitle"),
            obs_module_text("WindowSettingsMainDescription")
        );
        
        QDialog* dialog = components.dialog;
        QScrollArea* scrollArea = components.scrollArea;
        QWidget* contentWidget = components.contentWidget;
        QVBoxLayout* contentLayout = components.contentLayout;

        obs_properties_t* props = obs_properties_create();

        // General Settings Group
        QGroupBox* generalGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsGeneralGroup"), "info");
        
        QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);
        generalLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        // Run at startup setting
        obs_property_t* runAtStartupProp =
            obs_properties_add_bool(props, "run_at_startup", obs_module_text("WindowSettingsRunOnStartup"));

        // Create horizontal layout for run at startup setting
        QHBoxLayout* runAtStartupLayout = new QHBoxLayout();
        
        QLabel* runAtStartupLabel = new QLabel(obs_module_text("WindowSettingsRunOnStartup"));
        runAtStartupLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
        
        StreamUP::UIStyles::SwitchButton* runAtStartupSwitch = StreamUP::UIStyles::CreateStyledSwitch(
            "", obs_data_get_bool(settings, obs_property_name(runAtStartupProp))
        );
        
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
        obs_property_t* notificationsMuteProp =
            obs_properties_add_bool(props, "notifications_mute", obs_module_text("WindowSettingsNotificationsMute"));

        // Create horizontal layout for notifications setting
        QHBoxLayout* notificationsLayout = new QHBoxLayout();
        
        QLabel* notificationsLabel = new QLabel(obs_module_text("WindowSettingsNotificationsMute"));
        notificationsLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
        notificationsLabel->setToolTip(obs_module_text("WindowSettingsNotificationsMuteTooltip"));
        
        StreamUP::UIStyles::SwitchButton* notificationsMuteSwitch = StreamUP::UIStyles::CreateStyledSwitch(
            "", obs_data_get_bool(settings, obs_property_name(notificationsMuteProp))
        );
        notificationsMuteSwitch->setToolTip(obs_module_text("WindowSettingsNotificationsMuteTooltip"));
        
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
        obs_property_t* cphIntegrationProp =
            obs_properties_add_bool(props, "show_cph_integration", obs_module_text("WindowSettingsCPHIntegration"));

        // Create horizontal layout for CPH integration setting
        QHBoxLayout* cphLayout = new QHBoxLayout();
        
        QLabel* cphLabel = new QLabel(obs_module_text("WindowSettingsCPHIntegration"));
        cphLabel->setStyleSheet(QString("color: %1; font-size: %2px; background: transparent;")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
        cphLabel->setToolTip(obs_module_text("WindowSettingsCPHIntegrationTooltip"));
        
        StreamUP::UIStyles::SwitchButton* cphIntegrationSwitch = StreamUP::UIStyles::CreateStyledSwitch(
            "", obs_data_get_bool(settings, obs_property_name(cphIntegrationProp))
        );
        cphIntegrationSwitch->setToolTip(obs_module_text("WindowSettingsCPHIntegrationTooltip"));
        
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
        contentLayout->addWidget(generalGroup);

        // Plugin Management Group
        QGroupBox* pluginGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsPluginManagementGroup"), "info");
        
        QVBoxLayout* pluginLayout = new QVBoxLayout(pluginGroup);
        pluginLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        QPushButton* pluginButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsViewInstalledPlugins"), "info");
        
        // Connect plugin button to show plugins inline with template system
        QObject::connect(pluginButton, &QPushButton::clicked, [components]() {
            StreamUP::SettingsManager::ShowInstalledPluginsInline(components);
        });
        
        QHBoxLayout* pluginButtonLayout = new QHBoxLayout();
        pluginButtonLayout->addStretch();
        pluginButtonLayout->addWidget(pluginButton);
        pluginButtonLayout->addStretch();
        pluginLayout->addLayout(pluginButtonLayout);
        
        contentLayout->addWidget(pluginGroup);

        // Hotkeys Group
        QGroupBox* hotkeysGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsHotkeysGroup"), "info");
        
        QVBoxLayout* hotkeysLayout = new QVBoxLayout(hotkeysGroup);
        hotkeysLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        QPushButton* hotkeysButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsManageHotkeys"), "info");
        
        // Connect hotkeys button to show hotkeys inline with template system
        QObject::connect(hotkeysButton, &QPushButton::clicked, [components]() {
            StreamUP::SettingsManager::ShowHotkeysInline(components);
        });
        
        QHBoxLayout* hotkeysButtonLayout = new QHBoxLayout();
        hotkeysButtonLayout->addStretch();
        hotkeysButtonLayout->addWidget(hotkeysButton);
        hotkeysButtonLayout->addStretch();
        hotkeysLayout->addLayout(hotkeysButtonLayout);
        
        contentLayout->addWidget(hotkeysGroup);

        // Dock Configuration Group
        QGroupBox* dockConfigGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsDockConfigGroup"), "info");
        
        QVBoxLayout* dockConfigLayout = new QVBoxLayout(dockConfigGroup);
        dockConfigLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        QPushButton* dockConfigButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsManageDockConfig"), "info");
        
        // Connect dock config button to show dock config inline with template system
        QObject::connect(dockConfigButton, &QPushButton::clicked, [components]() {
            StreamUP::SettingsManager::ShowDockConfigInline(components);
        });
        
        QHBoxLayout* dockConfigButtonLayout = new QHBoxLayout();
        dockConfigButtonLayout->addStretch();
        dockConfigButtonLayout->addWidget(dockConfigButton);
        dockConfigButtonLayout->addStretch();
        dockConfigLayout->addLayout(dockConfigButtonLayout);
        
        contentLayout->addWidget(dockConfigGroup);
        contentLayout->addStretch();
        
        // Create a timer to periodically update button text based on current page
        QTimer* buttonUpdateTimer = new QTimer(dialog);
        buttonUpdateTimer->setInterval(100); // Check every 100ms
        
        QPointer<QWidget> originalContentWidget = contentWidget; // Use QPointer for safe reference tracking
        
        // Capture specific pointers safely to avoid reference issues
        QPointer<QScrollArea> scrollAreaPtr = components.scrollArea;
        QPointer<QPushButton> mainButtonPtr = components.mainButton;
        QPointer<QDialog> dialogPtr = components.dialog;
        
        QObject::connect(buttonUpdateTimer, &QTimer::timeout, [scrollAreaPtr, mainButtonPtr, originalContentWidget]() {
            if (scrollAreaPtr.isNull() || mainButtonPtr.isNull()) {
                return;
            }
            
            QWidget* currentWidget = scrollAreaPtr->widget();
            if (currentWidget != originalContentWidget.data()) {
                // We're on a sub-page, show Back button
                if (mainButtonPtr->text() != obs_module_text("WindowSettingsBackButton")) {
                    mainButtonPtr->setText(obs_module_text("WindowSettingsBackButton"));
                }
            } else {
                // We're on main page, show Close button
                if (mainButtonPtr->text() != obs_module_text("Close")) {
                    mainButtonPtr->setText(obs_module_text("Close"));
                }
            }
        });
        buttonUpdateTimer->start();
        
        // Setup dialog navigation using template system
        StreamUP::UIStyles::SetupDialogNavigation(components, [scrollAreaPtr, dialogPtr, originalContentWidget]() {
            // Add null pointer checks to prevent crashes
            if (scrollAreaPtr.isNull() || dialogPtr.isNull()) {
                return;
            }
            
            // Navigate back to main page if we're on a sub-page
            QWidget* currentWidget = scrollAreaPtr->widget();
            if (currentWidget != originalContentWidget.data()) {
                // We're on a sub-page, go back to main content
                QWidget* widgetToRemove = scrollAreaPtr->takeWidget();
                if (!originalContentWidget.isNull()) {
                    scrollAreaPtr->setWidget(originalContentWidget.data());
                }
                
                // Properly delete the widget that was removed to prevent memory leaks and crashes
                if (widgetToRemove && widgetToRemove != originalContentWidget.data()) {
                    widgetToRemove->deleteLater();
                }
                
                // Main header stays unchanged - "StreamUP Settings" throughout
            } else {
                // We're on main page, just close
                dialogPtr->close();
            }
        });
        
        // Stop timer when dialog is closed
        QObject::connect(dialog, &QDialog::finished, [buttonUpdateTimer]() {
            buttonUpdateTimer->stop();
        });

        QObject::connect(dialog, &QDialog::finished, [=](int) {
            obs_data_release(settings);
            obs_properties_destroy(props);
        });

        // Store the dialog reference
        settingsDialog = dialog;
        
        // Apply consistent sizing that fits content without scrolling - use very generous height
        StreamUP::UIStyles::ApplyConsistentSizing(dialog, 650, 1000, 300, 1200);
        dialog->show();
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

void ShowInstalledPluginsInline(const StreamUP::UIStyles::StandardDialogComponents& components)
{
    // Store the current widget temporarily
    QWidget* currentWidget = components.scrollArea->takeWidget();
    
    // Keep the main header unchanged - only replace content below it
    
    // Create replacement content widget with sub-page header
    QWidget* pluginsWidget = new QWidget();
    pluginsWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    pluginsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout* pluginsLayout = new QVBoxLayout(pluginsWidget);
    pluginsLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_SMALL, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_SMALL, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    pluginsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Create separate header section with same spacing as main header
    QWidget* headerSection = new QWidget();
    QVBoxLayout* headerSectionLayout = new QVBoxLayout(headerSection);
    headerSectionLayout->setContentsMargins(0, 0, 0, 0);
    headerSectionLayout->setSpacing(0); // Same as main header - no base spacing
    
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsInstalledPluginsTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    headerSectionLayout->addWidget(titleLabel);
    
    // Add small spacing between title and description for readability
    //headerSectionLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);
    
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WindowSettingsInstalledPluginsDesc"));
    descLabel->setAlignment(Qt::AlignCenter);
    headerSectionLayout->addWidget(descLabel);
    
    pluginsLayout->addWidget(headerSection);
    
    // Info section - constrain width properly
    QLabel* infoLabel = new QLabel(obs_module_text("WindowSettingsInstalledPluginsInfo"));
    infoLabel->setStyleSheet(QString(
        "QLabel {"
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
    QTableWidget* pluginTable = CreatePluginTable();
    
    // Add compatible plugins
    auto installedPlugins = StreamUP::PluginManager::GetInstalledPluginsCached();
    for (const auto& plugin : installedPlugins) {
        AddCompatiblePluginRow(pluginTable, plugin.first, plugin.second);
    }
    
    // Add incompatible plugins
    char* filePath = GetFilePath();
    std::vector<std::string> incompatibleModules = SearchModulesInFile(filePath);
    bfree(filePath);
    
    for (const std::string& moduleName : incompatibleModules) {
        AddIncompatiblePluginRow(pluginTable, moduleName);
    }
    
    // Set appropriate height based on content
    int totalRows = pluginTable->rowCount();
    if (totalRows == 0) {
        // Show empty state
        QLabel* emptyLabel = new QLabel(obs_module_text("WindowSettingsInstalledPlugins"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QString(
            "QLabel {"
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
        QObject::connect(pluginTable, &QTableWidget::cellClicked, 
                        [pluginTable](int row, int column) {
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

void ShowInstalledPluginsPage(QWidget* parentWidget)
{
    StreamUP::UIHelpers::ShowDialogOnUIThread([parentWidget]() {
        auto installedPlugins = StreamUP::PluginManager::GetInstalledPluginsCached();

        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("WindowSettingsInstalledPlugins"), parentWidget);
        
        // Start smaller - will be resized based on content
        dialog->resize(600, 500);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section with title
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px; }")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL));
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsInstalledPluginsTitle"));
        headerLayout->addWidget(titleLabel);
        
        QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WindowSettingsInstalledPluginsDesc"));
        headerLayout->addWidget(subtitleLabel);
        
        mainLayout->addWidget(headerWidget);

        // Content area - direct layout without scrolling  
        QVBoxLayout* contentLayout = new QVBoxLayout();
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        mainLayout->addLayout(contentLayout);

        // Info section - fit to UI width
        QLabel* infoLabel = new QLabel(obs_module_text("WindowSettingsInstalledPluginsInfo"));
        infoLabel->setStyleSheet(QString(
            "QLabel {"
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
        QTableWidget* pluginTable = CreatePluginTable();
        
        // Add compatible plugins
        for (const auto& plugin : installedPlugins) {
            AddCompatiblePluginRow(pluginTable, plugin.first, plugin.second);
        }
        
        // Add incompatible plugins
        char* filePath = GetFilePath();
        std::vector<std::string> incompatibleModules = SearchModulesInFile(filePath);
        bfree(filePath);
        
        for (const std::string& moduleName : incompatibleModules) {
            AddIncompatiblePluginRow(pluginTable, moduleName);
        }
        
        // Configure table for dialog - larger size and better layout
        int totalRows = pluginTable->rowCount();
        if (totalRows == 0) {
            // Show empty state
            QLabel* emptyLabel = new QLabel(obs_module_text("WindowSettingsInstalledPlugins"));
            emptyLabel->setAlignment(Qt::AlignCenter);
            emptyLabel->setStyleSheet(QString(
                "QLabel {"
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
            QObject::connect(pluginTable, &QTableWidget::cellClicked, 
                            [pluginTable](int row, int column) {
                StreamUP::UIStyles::HandleTableCellClick(pluginTable, row, column);
            });
            
            // Adjust dialog size to exactly fit table content
            int tableWidth = pluginTable->minimumWidth();
            int dialogWidth = std::max(tableWidth + 80, 600); // Minimal padding, lower minimum
            dialog->resize(dialogWidth, 650);
            
            contentLayout->addWidget(pluginTable);
        }

        // Bottom button area
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet(QString("background: %1; padding: %2px;")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0);

        QPushButton* closeButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Close"), "neutral");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addWidget(buttonWidget);

        dialog->setLayout(mainLayout);
        
        // Apply consistent sizing that adjusts to actual content size (also centers dialog after sizing)
        StreamUP::UIStyles::ApplyConsistentSizing(dialog, 650, 1000, 400, 800);
        dialog->show();
    });
}

void ShowHotkeysInline(const StreamUP::UIStyles::StandardDialogComponents& components)
{
    // Store the current widget temporarily
    QWidget* currentWidget = components.scrollArea->takeWidget();
    
    // Keep the main header unchanged - only replace content below it
    
    // Create replacement content widget with sub-page header
    QWidget* hotkeysWidget = new QWidget();
    hotkeysWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    QVBoxLayout* hotkeysLayout = new QVBoxLayout(hotkeysWidget);
    hotkeysLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    hotkeysLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Create separate header section with same spacing as main header
    QWidget* headerSection = new QWidget();
    QVBoxLayout* headerSectionLayout = new QVBoxLayout(headerSection);
    headerSectionLayout->setContentsMargins(0, 0, 0, 0);
    headerSectionLayout->setSpacing(0); // Same as main header - no base spacing
    
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsHotkeysTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    headerSectionLayout->addWidget(titleLabel);
    
    // Add small spacing between title and description for readability
    // headerSectionLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);
    
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WindowSettingsHotkeysDesc"));
    descLabel->setAlignment(Qt::AlignCenter);
    headerSectionLayout->addWidget(descLabel);
    
    hotkeysLayout->addWidget(headerSection);
    
    // Info section
    QLabel* infoLabel = new QLabel(obs_module_text("WindowSettingsHotkeysInfo"));
    infoLabel->setStyleSheet(QString(
        "QLabel {"
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
    QGroupBox* hotkeysGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsHotkeysGroupTitle"), "info");
    
    QVBoxLayout* hotkeysGroupLayout = new QVBoxLayout(hotkeysGroup);
    hotkeysGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
    // Define hotkey information structure
    struct HotkeyInfo {
        QString name;
        QString description;
        QString obsHotkeyName;
    };
    
    // List of all StreamUP hotkeys
    std::vector<HotkeyInfo> streamupHotkeys = {
        {obs_module_text("HotkeyRefreshBrowserSources"), obs_module_text("HotkeyRefreshBrowserSourcesDesc"), "streamup_refresh_browser_sources"},
        {obs_module_text("HotkeyRefreshAudioMonitoring"), obs_module_text("HotkeyRefreshAudioMonitoringDesc"), "streamup_refresh_audio_monitoring"},
        {obs_module_text("HotkeyLockAllSources"), obs_module_text("HotkeyLockAllSourcesDesc"), "streamup_lock_all_sources"},
        {obs_module_text("HotkeyLockCurrentSources"), obs_module_text("HotkeyLockCurrentSourcesDesc"), "streamup_lock_current_sources"},
        {obs_module_text("HotkeyOpenSourceProperties"), obs_module_text("HotkeyOpenSourcePropertiesDesc"), "streamup_open_source_properties"},
        {obs_module_text("HotkeyOpenSourceFilters"), obs_module_text("HotkeyOpenSourceFiltersDesc"), "streamup_open_source_filters"},
        {obs_module_text("HotkeyOpenSourceInteract"), obs_module_text("HotkeyOpenSourceInteractDesc"), "streamup_open_source_interact"},
        {obs_module_text("HotkeyOpenSceneFilters"), obs_module_text("HotkeyOpenSceneFiltersDesc"), "streamup_open_scene_filters"},
        {obs_module_text("HotkeyActivateVideoCaptureDevices"), obs_module_text("HotkeyActivateVideoCaptureDevicesDesc"), "streamup_activate_video_capture_devices"},
        {obs_module_text("HotkeyDeactivateVideoCaptureDevices"), obs_module_text("HotkeyDeactivateVideoCaptureDevicesDesc"), "streamup_deactivate_video_capture_devices"},
        {obs_module_text("HotkeyRefreshVideoCaptureDevices"), obs_module_text("HotkeyRefreshVideoCaptureDevicesDesc"), "streamup_refresh_video_capture_devices"}
    };
    
    // Create direct layout for hotkeys (no scrolling, fit to content)
    QVBoxLayout* hotkeyContentLayout = new QVBoxLayout();
    hotkeyContentLayout->setSpacing(0); // No spacing, separators will handle it
    hotkeyContentLayout->setContentsMargins(0, 0, 0, 0); // No padding inside the box (like WebSocket UI)
    
    // Add each hotkey as a row with actual hotkey widget (WebSocket UI style)
    for (int i = 0; i < streamupHotkeys.size(); ++i) {
        const auto& hotkey = streamupHotkeys[i];
        
        QWidget* hotkeyRow = new QWidget();
        // Use transparent background with no border (like WebSocket UI)
        hotkeyRow->setStyleSheet(QString(
            "QWidget {"
            "background: transparent;"
            "border: none;"
            "padding: 0px;"
            "}"));
        
        QHBoxLayout* hotkeyRowLayout = new QHBoxLayout(hotkeyRow);
        // Match WebSocket UI margins: tight vertical padding, medium horizontal spacing
        hotkeyRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
        hotkeyRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Text section - vertical layout with tight spacing (like WebSocket UI)
        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(2); // Very tight spacing between name and description
        textLayout->setContentsMargins(0, 0, 0, 0);
        
        // Hotkey name - use same styling as WebSocket UI
        QLabel* nameLabel = new QLabel(hotkey.name);
        nameLabel->setStyleSheet(QString(
            "QLabel {"
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
        QLabel* descLabel = new QLabel(hotkey.description);
        descLabel->setStyleSheet(QString(
            "QLabel {"
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
        QWidget* textWrapper = new QWidget();
        QVBoxLayout* wrapperLayout = new QVBoxLayout(textWrapper);
        wrapperLayout->setContentsMargins(0, 0, 0, 0);
        wrapperLayout->addStretch(); // Add stretch above
        wrapperLayout->addLayout(textLayout);
        wrapperLayout->addStretch(); // Add stretch below
        
        hotkeyRowLayout->addWidget(textWrapper, 1);
        
        // Hotkey configuration widget section - also center vertically
        QVBoxLayout* hotkeyWrapperLayout = new QVBoxLayout();
        hotkeyWrapperLayout->setContentsMargins(0, 0, 0, 0);
        hotkeyWrapperLayout->addStretch(); // Add stretch above
        
        StreamUP::UI::HotkeyWidget* hotkeyWidget = new StreamUP::UI::HotkeyWidget(hotkey.obsHotkeyName, hotkeyRow);
        
        // Load current hotkey binding
        obs_data_array_t* currentBinding = StreamUP::HotkeyManager::GetHotkeyBinding(hotkey.obsHotkeyName.toUtf8().constData());
        if (currentBinding) {
            hotkeyWidget->SetHotkey(currentBinding);
            obs_data_array_release(currentBinding);
        }
        
        // Connect to save changes immediately
        QObject::connect(hotkeyWidget, &StreamUP::UI::HotkeyWidget::HotkeyChanged, 
            [](const QString& hotkeyName, obs_data_array_t* hotkeyData) {
                if (hotkeyData) {
                    StreamUP::HotkeyManager::SetHotkeyBinding(hotkeyName.toUtf8().constData(), hotkeyData);
                } else {
                    // Clear the hotkey
                    obs_data_array_t* emptyArray = obs_data_array_create();
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
            QFrame* separator = new QFrame();
            separator->setFrameShape(QFrame::HLine);
            separator->setFrameShadow(QFrame::Plain);
            separator->setStyleSheet(QString(
                "QFrame {"
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
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
    QPushButton* resetButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsResetAllHotkeys"), "error");
    
    // Store all hotkey widgets so we can refresh them after reset
    QList<StreamUP::UI::HotkeyWidget*> allHotkeyWidgets;
    
    // Collect all hotkey widgets from the layout
    for (int i = 0; i < hotkeyContentLayout->count(); i++) {
        QLayoutItem* item = hotkeyContentLayout->itemAt(i);
        if (item && item->widget()) {
            QWidget* widget = item->widget();
            // Skip separators (QFrame), only process hotkey rows
            if (QString(widget->metaObject()->className()) != "QFrame") {
                StreamUP::UI::HotkeyWidget* hotkeyWidget = widget->findChild<StreamUP::UI::HotkeyWidget*>();
                if (hotkeyWidget) {
                    allHotkeyWidgets.append(hotkeyWidget);
                }
            }
        }
    }
    
    QObject::connect(resetButton, &QPushButton::clicked, [allHotkeyWidgets]() {
        // Show confirmation dialog for reset
        StreamUP::UIHelpers::ShowDialogOnUIThread([allHotkeyWidgets]() {
            QDialog* confirmDialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("WindowSettingsResetHotkeysTitle"));
            confirmDialog->resize(400, 200);
            
            QVBoxLayout* layout = new QVBoxLayout(confirmDialog);
            
            QLabel* warningLabel = new QLabel(obs_module_text("WindowSettingsResetHotkeysWarning"));
            warningLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: %3px;")
                .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
                .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
                .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
            warningLabel->setWordWrap(true);
            warningLabel->setAlignment(Qt::AlignCenter);
            
            layout->addWidget(warningLabel);
            
            QHBoxLayout* buttonLayout = new QHBoxLayout();
            
            QPushButton* cancelBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Cancel"), "neutral");
            QPushButton* resetBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsResetHotkeysButton"), "error");
            
            QObject::connect(cancelBtn, &QPushButton::clicked, confirmDialog, &QDialog::close);
            QObject::connect(resetBtn, &QPushButton::clicked, [confirmDialog, allHotkeyWidgets]() {
                // Clear all StreamUP hotkeys
                StreamUP::HotkeyManager::ResetAllHotkeys();
                
                // Refresh all hotkey widgets in the UI
                for (StreamUP::UI::HotkeyWidget* widget : allHotkeyWidgets) {
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

void UpdateDockToolSettings(const DockToolSettings& dockSettings)
{
    PluginSettings settings = GetCurrentSettings();
    settings.dockTools = dockSettings;
    UpdateSettings(settings);
    
    // Notify all dock instances to update their visibility
    StreamUPDock::NotifyAllDocksSettingsChanged();
}

void ShowDockConfigInline(const StreamUP::UIStyles::StandardDialogComponents& components)
{
    // Store the current widget temporarily
    QWidget* currentWidget = components.scrollArea->takeWidget();
    
    // Keep the main header unchanged - only replace content below it
    
    // Create replacement content widget with sub-page header
    QWidget* dockConfigWidget = new QWidget();
    dockConfigWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    QVBoxLayout* dockConfigLayout = new QVBoxLayout(dockConfigWidget);
    dockConfigLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    dockConfigLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Create separate header section with same spacing as main header
    QWidget* headerSection = new QWidget();
    QVBoxLayout* headerSectionLayout = new QVBoxLayout(headerSection);
    headerSectionLayout->setContentsMargins(0, 0, 0, 0);
    headerSectionLayout->setSpacing(0); // Same as main header - no base spacing
    
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsDockConfigTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    headerSectionLayout->addWidget(titleLabel);
    
    // Add small spacing between title and description for readability
    // headerSectionLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);
    
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WindowSettingsDockConfigDesc"));
    descLabel->setAlignment(Qt::AlignCenter);
    headerSectionLayout->addWidget(descLabel);
    
    dockConfigLayout->addWidget(headerSection);
    
    // Info section
    QLabel* infoLabel = new QLabel(obs_module_text("WindowSettingsDockConfigInfo"));
    infoLabel->setStyleSheet(QString(
        "QLabel {"
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
    QGroupBox* toolsGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsDockToolsGroupTitle"), "info");
    
    QVBoxLayout* toolsGroupLayout = new QVBoxLayout(toolsGroup);
    toolsGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        0, // No top padding
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        0); // No bottom padding
    toolsGroupLayout->setSpacing(0); // No spacing since we handle it in the widget padding
    
    // Get current dock settings
    DockToolSettings currentSettings = GetDockToolSettings();
    
    // Define tool information structure
    struct ToolInfo {
        QString name;
        QString description;
        bool* settingPtr;
        int toolIndex; // Add index to identify which tool this is
    };
    
    // List of all dock tools with pointers to their settings
    std::vector<ToolInfo> dockTools = {
        {obs_module_text("DockToolLockAllSources"), obs_module_text("DockToolLockAllSourcesDesc"), &currentSettings.showLockAllSources, 0},
        {obs_module_text("DockToolLockCurrentSources"), obs_module_text("DockToolLockCurrentSourcesDesc"), &currentSettings.showLockCurrentSources, 1},
        {obs_module_text("DockToolRefreshBrowserSources"), obs_module_text("DockToolRefreshBrowserSourcesDesc"), &currentSettings.showRefreshBrowserSources, 2},
        {obs_module_text("DockToolRefreshAudioMonitoring"), obs_module_text("DockToolRefreshAudioMonitoringDesc"), &currentSettings.showRefreshAudioMonitoring, 3},
        {obs_module_text("DockToolVideoCaptureOptions"), obs_module_text("DockToolVideoCaptureOptionsDesc"), &currentSettings.showVideoCaptureOptions, 4}
    };
    
    // Create tool rows matching WebSocket/hotkeys UI pattern
    for (int i = 0; i < dockTools.size(); ++i) {
        const auto& tool = dockTools[i];
        
        QWidget* toolRow = new QWidget();
        toolRow->setStyleSheet(QString(
            "QWidget {"
            "background: transparent;"
            "border: none;"
            "padding: 0px;"
            "}"));
        
        QHBoxLayout* toolRowLayout = new QHBoxLayout(toolRow);
        toolRowLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
        toolRowLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Text section - vertical layout with tight spacing (like WebSocket/hotkeys UI)
        QVBoxLayout* textLayout = new QVBoxLayout();
        textLayout->setSpacing(2); // Very tight spacing between name and description
        textLayout->setContentsMargins(0, 0, 0, 0);
        
        // Tool name - use same styling as WebSocket/hotkeys UI
        QLabel* nameLabel = new QLabel(tool.name);
        nameLabel->setStyleSheet(QString(
            "QLabel {"
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
        QLabel* descLabel = new QLabel(tool.description);
        descLabel->setStyleSheet(QString(
            "QLabel {"
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
        QWidget* textWrapper = new QWidget();
        QVBoxLayout* wrapperLayout = new QVBoxLayout(textWrapper);
        wrapperLayout->setContentsMargins(0, 0, 0, 0);
        wrapperLayout->addStretch(); // Add stretch above
        wrapperLayout->addLayout(textLayout);
        wrapperLayout->addStretch(); // Add stretch below
        
        toolRowLayout->addWidget(textWrapper, 1);
        
        // Switch section - also center vertically
        QVBoxLayout* switchWrapperLayout = new QVBoxLayout();
        switchWrapperLayout->setContentsMargins(0, 0, 0, 0);
        switchWrapperLayout->addStretch(); // Add stretch above
        
        // Force fresh settings load every time to ensure we have the latest values
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
        
        // Create switch with explicit initial state
        StreamUP::UIStyles::SwitchButton* toolSwitch = StreamUP::UIStyles::CreateStyledSwitch("", currentValue);
        
        // Force a visual refresh after the UI is fully constructed
        QTimer::singleShot(0, [toolSwitch, currentValue]() {
            toolSwitch->setChecked(currentValue);
        });
        
        // Update settings immediately when switch changes  
        QObject::connect(toolSwitch, &StreamUP::UIStyles::SwitchButton::toggled, [tool, toolSwitch](bool checked) {
                // Get current settings from persistent storage
                DockToolSettings settings = GetDockToolSettings();
                
                // Update the specific setting based on tool index
                switch (tool.toolIndex) {
                    case 0: settings.showLockAllSources = checked; break;
                    case 1: settings.showLockCurrentSources = checked; break;
                    case 2: settings.showRefreshBrowserSources = checked; break;
                    case 3: settings.showRefreshAudioMonitoring = checked; break;
                    case 4: settings.showVideoCaptureOptions = checked; break;
                }
                
                // Update the local pointer too for UI consistency
                *tool.settingPtr = checked;
                
                // Save the updated settings immediately (this calls NotifyAllDocksSettingsChanged)
                UpdateDockToolSettings(settings);
            });
        
        switchWrapperLayout->addWidget(toolSwitch);
        switchWrapperLayout->addStretch(); // Add stretch below
        
        toolRowLayout->addLayout(switchWrapperLayout);
        
        toolsGroupLayout->addWidget(toolRow);
        
        // Add separator line between tools (but not after the last one) - like WebSocket/hotkeys UI
        if (i < dockTools.size() - 1) {
            QFrame* separator = new QFrame();
            separator->setFrameShape(QFrame::HLine);
            separator->setFrameShadow(QFrame::Plain);
            separator->setStyleSheet(QString(
                "QFrame {"
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
    QHBoxLayout* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    actionLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3);
    
    QPushButton* resetButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsResetDockConfig"), "error");
    
    // Store all switches so we can update them after reset
    QList<StreamUP::UIStyles::SwitchButton*> allSwitches;
    for (int i = 0; i < toolsGroupLayout->count(); i++) {
        QLayoutItem* item = toolsGroupLayout->itemAt(i);
        if (item && item->widget()) {
            QWidget* widget = item->widget();
            if (QString(widget->metaObject()->className()) != "QFrame") {
                StreamUP::UIStyles::SwitchButton* switchButton = widget->findChild<StreamUP::UIStyles::SwitchButton*>();
                if (switchButton) {
                    allSwitches.append(switchButton);
                }
            }
        }
    }
    
    QObject::connect(resetButton, &QPushButton::clicked, [allSwitches, dockTools]() {
        // Show confirmation dialog for reset (matching hotkeys pattern)
        StreamUP::UIHelpers::ShowDialogOnUIThread([allSwitches, dockTools]() {
            QDialog* confirmDialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("WindowSettingsResetDockConfigTitle"));
            confirmDialog->resize(400, 200);
            
            QVBoxLayout* layout = new QVBoxLayout(confirmDialog);
            
            QLabel* warningLabel = new QLabel(obs_module_text("WindowSettingsResetDockConfigWarning"));
            warningLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: %3px;")
                .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
                .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL)
                .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
            warningLabel->setWordWrap(true);
            warningLabel->setAlignment(Qt::AlignCenter);
            
            layout->addWidget(warningLabel);
            
            QHBoxLayout* buttonLayout = new QHBoxLayout();
            
            QPushButton* cancelBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Cancel"), "neutral");
            QPushButton* resetBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsResetDockConfigButton"), "error");
            
            QObject::connect(cancelBtn, &QPushButton::clicked, confirmDialog, &QDialog::close);
            QObject::connect(resetBtn, &QPushButton::clicked, [confirmDialog, allSwitches, dockTools]() {
                // Reset all dock tools to default (visible)
                DockToolSettings defaultSettings;
                UpdateDockToolSettings(defaultSettings);
                
                // Update all switches to checked state
                for (StreamUP::UIStyles::SwitchButton* switchButton : allSwitches) {
                    if (switchButton) {
                        switchButton->setChecked(true);
                    }
                }
                
                // Update the tool setting pointers
                for (const auto& tool : dockTools) {
                    *tool.settingPtr = true;
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


} // namespace SettingsManager
} // namespace StreamUP
