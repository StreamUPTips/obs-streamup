#include "settings-manager.hpp"
#include "ui-helpers.hpp"
#include "plugin-manager.hpp"
#include <obs-module.h>
#include <obs-properties.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpacerItem>
#include <QGroupBox>
#include <QScrollArea>
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
        obs_data_release(data);
    }
    
    return settings;
}

void UpdateSettings(const PluginSettings& settings)
{
    obs_data_t* data = obs_data_create();
    obs_data_set_bool(data, "run_at_startup", settings.runAtStartup);
    obs_data_set_bool(data, "notifications_mute", settings.notificationsMute);
    
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
    StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
        obs_data_t* settings = LoadSettings();
        if (!settings) {
            return;
        }

        QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowSettingsTitle");
        dialog->setStyleSheet("QDialog { background: #13171f; }");
        dialog->setFixedSize(600, 400);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section with title
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet("QWidget#headerWidget { background: #13171f; padding: 20px; }");
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* titleLabel = new QLabel("⚙️ Settings");
        titleLabel->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 24px;
                font-weight: bold;
                margin: 0px;
                padding: 0px;
            }
        )");
        titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(titleLabel);
        
        mainLayout->addWidget(headerWidget);

        // Content area
        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet("background: #13171f;");
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(30, 20, 30, 20);
        contentLayout->setSpacing(20);

        obs_properties_t* props = obs_properties_create();

        // General Settings Group
        QGroupBox* generalGroup = new QGroupBox("General Settings");
        generalGroup->setStyleSheet(R"(
            QGroupBox {
                color: white;
                font-size: 14px;
                font-weight: bold;
                border: 2px solid #2d3748;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
                background: #1a202c;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 8px 0 8px;
                color: #60a5fa;
            }
        )");
        
        QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);
        generalLayout->setSpacing(15);

        // Run at startup setting
        obs_property_t* runAtStartupProp =
            obs_properties_add_bool(props, "run_at_startup", obs_module_text("WindowSettingsRunOnStartup"));

        QCheckBox* runAtStartupCheckBox = new QCheckBox(obs_module_text("WindowSettingsRunOnStartup"));
        runAtStartupCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(runAtStartupProp)));
        runAtStartupCheckBox->setStyleSheet(R"(
            QCheckBox {
                color: white;
                font-size: 12px;
                spacing: 8px;
            }
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border: 2px solid #4a5568;
                border-radius: 3px;
                background: #2d3748;
            }
            QCheckBox::indicator:checked {
                background: #3182ce;
                border: 2px solid #3182ce;
            }
            QCheckBox::indicator:checked:hover {
                background: #2c5aa0;
            }
        )");

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(runAtStartupCheckBox, &QCheckBox::checkStateChanged, [=](int state) {
#else
        QObject::connect(runAtStartupCheckBox, &QCheckBox::stateChanged, [=](int state) {
#endif
            obs_data_set_bool(settings, obs_property_name(runAtStartupProp), state == Qt::Checked);
        });

        generalLayout->addWidget(runAtStartupCheckBox);

        // Notifications mute setting
        obs_property_t* notificationsMuteProp =
            obs_properties_add_bool(props, "notifications_mute", obs_module_text("WindowSettingsNotificationsMute"));

        QCheckBox* notificationsMuteCheckBox = new QCheckBox(obs_module_text("WindowSettingsNotificationsMute"));
        notificationsMuteCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(notificationsMuteProp)));
        notificationsMuteCheckBox->setToolTip(obs_module_text("WindowSettingsNotificationsMuteTooltip"));
        notificationsMuteCheckBox->setStyleSheet(R"(
            QCheckBox {
                color: white;
                font-size: 12px;
                spacing: 8px;
            }
            QCheckBox::indicator {
                width: 18px;
                height: 18px;
                border: 2px solid #4a5568;
                border-radius: 3px;
                background: #2d3748;
            }
            QCheckBox::indicator:checked {
                background: #3182ce;
                border: 2px solid #3182ce;
            }
            QCheckBox::indicator:checked:hover {
                background: #2c5aa0;
            }
        )");

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(notificationsMuteCheckBox, &QCheckBox::checkStateChanged, [=](int state) {
#else
        QObject::connect(notificationsMuteCheckBox, &QCheckBox::stateChanged, [=](int state) {
#endif
            bool isChecked = (state == Qt::Checked);
            obs_data_set_bool(settings, obs_property_name(notificationsMuteProp), isChecked);
            notificationsMuted = isChecked;
        });

        generalLayout->addWidget(notificationsMuteCheckBox);
        contentLayout->addWidget(generalGroup);

        // Plugin Management Group
        QGroupBox* pluginGroup = new QGroupBox("Plugin Management");
        pluginGroup->setStyleSheet(R"(
            QGroupBox {
                color: white;
                font-size: 14px;
                font-weight: bold;
                border: 2px solid #2d3748;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
                background: #1a202c;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 8px 0 8px;
                color: #60a5fa;
            }
        )");
        
        QVBoxLayout* pluginLayout = new QVBoxLayout(pluginGroup);
        pluginLayout->setSpacing(15);

        QPushButton* pluginButton = new QPushButton(obs_module_text("WindowSettingsViewInstalledPlugins"));
        pluginButton->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4a90e2, stop:1 #357abd);
                color: white;
                border: none;
                padding: 12px 24px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 6px;
                min-width: 200px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #5ba0f2, stop:1 #4682cd);
            }
            QPushButton:pressed {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3680d2, stop:1 #2968ad);
            }
        )");
        QObject::connect(pluginButton, &QPushButton::clicked, ShowInstalledPluginsDialog);
        
        QHBoxLayout* pluginButtonLayout = new QHBoxLayout();
        pluginButtonLayout->addStretch();
        pluginButtonLayout->addWidget(pluginButton);
        pluginButtonLayout->addStretch();
        pluginLayout->addLayout(pluginButtonLayout);
        
        contentLayout->addWidget(pluginGroup);
        contentLayout->addStretch();
        
        mainLayout->addWidget(contentWidget);

        // Bottom button area
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet("background: #13171f; padding: 20px;");
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(20, 0, 20, 0);

        QPushButton* cancelButton = new QPushButton(obs_module_text("Cancel"));
        cancelButton->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);
                color: white;
                border: none;
                padding: 12px 24px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 6px;
                min-width: 100px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);
            }
        )");
        QObject::connect(cancelButton, &QPushButton::clicked, [dialog, settings]() {
            obs_data_release(settings);
            dialog->close();
        });

        QPushButton* saveButton = new QPushButton(obs_module_text("Save"));
        saveButton->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22c55e, stop:1 #16a34a);
                color: white;
                border: none;
                padding: 12px 24px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 6px;
                min-width: 100px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #32d56e, stop:1 #26b35a);
            }
        )");
        QObject::connect(saveButton, &QPushButton::clicked, [=]() {
            SaveSettings(settings);
            dialog->close();
        });

        buttonLayout->addStretch();
        buttonLayout->addWidget(cancelButton);
        buttonLayout->addSpacing(10);
        buttonLayout->addWidget(saveButton);
        
        mainLayout->addWidget(buttonWidget);

        dialog->setLayout(mainLayout);

        QObject::connect(dialog, &QDialog::finished, [=](int) {
            obs_data_release(settings);
            obs_properties_destroy(props);
        });

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

void ShowInstalledPluginsDialog()
{
    StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
        auto installedPlugins = StreamUP::PluginManager::GetInstalledPlugins();

        QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow("WindowSettingsInstalledPlugins");
        dialog->setStyleSheet("QDialog { background: #13171f; }");
        dialog->resize(750, 550);
        dialog->setMinimumSize(650, 450);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section with title
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet("QWidget#headerWidget { background: #13171f; padding: 20px; }");
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* titleLabel = new QLabel("🔌 Installed Plugins");
        titleLabel->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 24px;
                font-weight: bold;
                margin: 0px;
                padding: 0px;
            }
        )");
        titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(titleLabel);
        
        QLabel* subtitleLabel = new QLabel("Overview of plugins detected by StreamUP's update checker");
        subtitleLabel->setStyleSheet(R"(
            QLabel {
                color: #9ca3af;
                font-size: 14px;
                margin: 5px 0px 0px 0px;
                padding: 0px;
            }
        )");
        subtitleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(subtitleLabel);
        
        mainLayout->addWidget(headerWidget);

        // Content area
        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet("background: #13171f;");
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(25, 15, 25, 15);
        contentLayout->setSpacing(15);

        // Info section - more compact
        QLabel* infoLabel = new QLabel("Plugins are tracked for updates and categorized by compatibility with our service.");
        infoLabel->setStyleSheet(R"(
            QLabel {
                color: #d1d5db;
                font-size: 12px;
                line-height: 1.3;
                padding: 12px;
                background: #1f2937;
                border: 1px solid #374151;
                border-radius: 6px;
            }
        )");
        infoLabel->setWordWrap(true);
        contentLayout->addWidget(infoLabel);

        QString compatiblePluginsString;
        if (installedPlugins.empty()) {
            compatiblePluginsString = obs_module_text("WindowSettingsInstalledPlugins");
        } else {
            for (const auto& plugin : installedPlugins) {
                const auto& pluginName = plugin.first;
                const auto& pluginVersion = plugin.second;
                const QString forumLink = GetForumLink(pluginName);

                compatiblePluginsString += "<a href=\"" + forumLink + "\" style=\"color: #60a5fa; text-decoration: none;\">" + 
                                         QString::fromStdString(pluginName) +
                                         "</a> <span style=\"color: #9ca3af;\">(" + QString::fromStdString(pluginVersion) + ")</span><br>";
            }
            if (compatiblePluginsString.endsWith("<br>")) {
                compatiblePluginsString.chop(4);
            }
        }

        QLabel* compatiblePluginsList = new QLabel(compatiblePluginsString);
        compatiblePluginsList->setOpenExternalLinks(true);
        compatiblePluginsList->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 12px;
                line-height: 1.4;
                padding: 12px;
                background: #1a202c;
            }
        )");
        compatiblePluginsList->setWordWrap(true);

        QGroupBox* compatiblePluginsBox = new QGroupBox("✅ Compatible Plugins");
        compatiblePluginsBox->setStyleSheet(R"(
            QGroupBox {
                color: white;
                font-size: 13px;
                font-weight: bold;
                border: 2px solid #059669;
                border-radius: 8px;
                margin-top: 8px;
                padding-top: 8px;
                background: #1a202c;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 6px 0 6px;
                color: #10b981;
            }
        )");
        
        QVBoxLayout* compatiblePluginsBoxLayout = new QVBoxLayout(compatiblePluginsBox);
        compatiblePluginsBoxLayout->setContentsMargins(0, 12, 0, 8);
        compatiblePluginsBoxLayout->addWidget(compatiblePluginsList);

        QScrollArea* compatibleScrollArea = new QScrollArea;
        compatibleScrollArea->setWidgetResizable(true);
        compatibleScrollArea->setWidget(compatiblePluginsBox);
        compatibleScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        compatibleScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        compatibleScrollArea->setMinimumWidth(300);
        compatibleScrollArea->setMaximumHeight(280);
        compatibleScrollArea->setStyleSheet(R"(
            QScrollArea {
                border: none;
                background: transparent;
            }
            QScrollBar:vertical {
                background: #374151;
                width: 12px;
                border-radius: 6px;
            }
            QScrollBar::handle:vertical {
                background: #6b7280;
                border-radius: 6px;
                min-height: 20px;
            }
            QScrollBar::handle:vertical:hover {
                background: #9ca3af;
            }
        )");

        QLabel* incompatiblePluginsList = new QLabel;
        char* filePath = GetFilePath();
        SetLabelWithSortedModules(incompatiblePluginsList, SearchModulesInFile(filePath));
        bfree(filePath);
        incompatiblePluginsList->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 12px;
                line-height: 1.4;
                padding: 12px;
                background: #1a202c;
            }
        )");
        incompatiblePluginsList->setWordWrap(true);

        QGroupBox* incompatiblePluginsBox = new QGroupBox("⚠️ Incompatible Plugins");
        incompatiblePluginsBox->setStyleSheet(R"(
            QGroupBox {
                color: white;
                font-size: 13px;
                font-weight: bold;
                border: 2px solid #dc2626;
                border-radius: 8px;
                margin-top: 8px;
                padding-top: 8px;
                background: #1a202c;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 6px 0 6px;
                color: #ef4444;
            }
        )");
        
        QVBoxLayout* incompatiblePluginsBoxLayout = new QVBoxLayout(incompatiblePluginsBox);
        incompatiblePluginsBoxLayout->setContentsMargins(0, 12, 0, 8);
        incompatiblePluginsBoxLayout->addWidget(incompatiblePluginsList);

        QScrollArea* incompatibleScrollArea = new QScrollArea;
        incompatibleScrollArea->setWidgetResizable(true);
        incompatibleScrollArea->setWidget(incompatiblePluginsBox);
        incompatibleScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        incompatibleScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        incompatibleScrollArea->setMinimumWidth(300);
        incompatibleScrollArea->setMaximumHeight(280);
        incompatibleScrollArea->setStyleSheet(R"(
            QScrollArea {
                border: none;
                background: transparent;
            }
            QScrollBar:vertical {
                background: #374151;
                width: 12px;
                border-radius: 6px;
            }
            QScrollBar::handle:vertical {
                background: #6b7280;
                border-radius: 6px;
                min-height: 20px;
            }
            QScrollBar::handle:vertical:hover {
                background: #9ca3af;
            }
        )");

        QHBoxLayout* pluginBoxesLayout = new QHBoxLayout();
        pluginBoxesLayout->setSpacing(15);
        pluginBoxesLayout->addWidget(compatibleScrollArea);
        pluginBoxesLayout->addWidget(incompatibleScrollArea);

        contentLayout->addLayout(pluginBoxesLayout);
        mainLayout->addWidget(contentWidget);

        // Bottom button area
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet("background: #13171f; padding: 15px;");
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(15, 0, 15, 0);

        QPushButton* closeButton = new QPushButton(obs_module_text("Close"));
        closeButton->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);
                color: white;
                border: none;
                padding: 12px 24px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 6px;
                min-width: 100px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);
            }
        )");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addWidget(buttonWidget);

        dialog->setLayout(mainLayout);
        dialog->show();
    });
}

} // namespace SettingsManager
} // namespace StreamUP