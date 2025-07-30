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
        QFormLayout* dialogLayout = new QFormLayout(dialog);
        dialogLayout->setContentsMargins(20, 15, 20, 10);

        QLabel* titleLabel = StreamUP::UIHelpers::CreateRichTextLabel("General", true, false);
        dialogLayout->addRow(titleLabel);

        obs_properties_t* props = obs_properties_create();

        // Run at startup setting
        obs_property_t* runAtStartupProp =
            obs_properties_add_bool(props, "run_at_startup", obs_module_text("WindowSettingsRunOnStartup"));

        QCheckBox* runAtStartupCheckBox = new QCheckBox(obs_module_text("WindowSettingsRunOnStartup"));
        runAtStartupCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(runAtStartupProp)));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(runAtStartupCheckBox, &QCheckBox::checkStateChanged, [=](int state) {
#else
        QObject::connect(runAtStartupCheckBox, &QCheckBox::stateChanged, [=](int state) {
#endif
            obs_data_set_bool(settings, obs_property_name(runAtStartupProp), state == Qt::Checked);
        });

        dialogLayout->addWidget(runAtStartupCheckBox);

        // Notifications mute setting
        obs_property_t* notificationsMuteProp =
            obs_properties_add_bool(props, "notifications_mute", obs_module_text("WindowSettingsNotificationsMute"));

        QCheckBox* notificationsMuteCheckBox = new QCheckBox(obs_module_text("WindowSettingsNotificationsMute"));
        notificationsMuteCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(notificationsMuteProp)));
        notificationsMuteCheckBox->setToolTip(obs_module_text("WindowSettingsNotificationsMuteTooltip"));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        QObject::connect(notificationsMuteCheckBox, &QCheckBox::checkStateChanged, [=](int state) {
#else
        QObject::connect(notificationsMuteCheckBox, &QCheckBox::stateChanged, [=](int state) {
#endif
            bool isChecked = (state == Qt::Checked);
            obs_data_set_bool(settings, obs_property_name(notificationsMuteProp), isChecked);
            notificationsMuted = isChecked;
        });

        dialogLayout->addWidget(notificationsMuteCheckBox);

        // Spacer
        dialogLayout->addItem(new QSpacerItem(0, 5));

        // Plugin management
        QLabel* pluginLabel = StreamUP::UIHelpers::CreateRichTextLabel(obs_module_text("WindowSettingsPluginManagement"), true, false);
        QPushButton* pluginButton = new QPushButton(obs_module_text("WindowSettingsViewInstalledPlugins"));
        QObject::connect(pluginButton, &QPushButton::clicked, ShowInstalledPluginsDialog);

        dialogLayout->addRow(pluginLabel);
        dialogLayout->addRow(pluginButton);

        // Buttons
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("Cancel"), [dialog, settings]() {
            obs_data_release(settings);
            dialog->close();
        });

        StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("Save"), [=]() {
            SaveSettings(settings);
            dialog->close();
        });

        QWidget* buttonWidget = new QWidget();
        buttonWidget->setLayout(buttonLayout);
        dialogLayout->addRow(buttonWidget);

        dialog->setLayout(dialogLayout);

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
        QVBoxLayout* dialogLayout = new QVBoxLayout(dialog);
        dialogLayout->setContentsMargins(20, 15, 20, 10);

        dialogLayout->addLayout(StreamUP::UIHelpers::AddIconAndText(QStyle::SP_MessageBoxInformation, "WindowSettingsInstalledPluginsInfo1"));
        dialogLayout->addSpacing(5);

        dialogLayout->addWidget(
            StreamUP::UIHelpers::CreateRichTextLabel(obs_module_text("WindowSettingsInstalledPluginsInfo2"), false, true, Qt::AlignTop));
        dialogLayout->addWidget(
            StreamUP::UIHelpers::CreateRichTextLabel(obs_module_text("WindowSettingsInstalledPluginsInfo3"), false, true, Qt::AlignTop));

        QString compatiblePluginsString;
        if (installedPlugins.empty()) {
            compatiblePluginsString = obs_module_text("WindowSettingsInstalledPlugins");
        } else {
            for (const auto& plugin : installedPlugins) {
                const auto& pluginName = plugin.first;
                const auto& pluginVersion = plugin.second;
                const QString forumLink = GetForumLink(pluginName);

                compatiblePluginsString += "<a href=\"" + forumLink + "\">" + QString::fromStdString(pluginName) +
                                         "</a> (" + QString::fromStdString(pluginVersion) + ")<br>";
            }
            if (compatiblePluginsString.endsWith("<br>")) {
                compatiblePluginsString.chop(4);
            }
        }

        QLabel* compatiblePluginsList = StreamUP::UIHelpers::CreateRichTextLabel(compatiblePluginsString, false, false);
        QGroupBox* compatiblePluginsBox = new QGroupBox(obs_module_text("WindowSettingsUpdaterCompatible"));
        QVBoxLayout* compatiblePluginsBoxLayout = StreamUP::UIHelpers::CreateVBoxLayout(compatiblePluginsBox);
        compatiblePluginsBoxLayout->addWidget(compatiblePluginsList);

        QScrollArea* compatibleScrollArea = new QScrollArea;
        compatibleScrollArea->setWidgetResizable(true);
        compatibleScrollArea->setWidget(compatiblePluginsBox);
        compatibleScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        compatibleScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        compatibleScrollArea->setMinimumWidth(200);

        QGroupBox* incompatiblePluginsBox = new QGroupBox(obs_module_text("WindowSettingsUpdaterIncompatible"));
        QVBoxLayout* incompatiblePluginsBoxLayout = StreamUP::UIHelpers::CreateVBoxLayout(incompatiblePluginsBox);
        QLabel* incompatiblePluginsList = new QLabel;
        char* filePath = GetFilePath();
        SetLabelWithSortedModules(incompatiblePluginsList, SearchModulesInFile(filePath));
        bfree(filePath);
        incompatiblePluginsBoxLayout->addWidget(incompatiblePluginsList);

        QScrollArea* incompatibleScrollArea = new QScrollArea;
        incompatibleScrollArea->setWidgetResizable(true);
        incompatibleScrollArea->setWidget(incompatiblePluginsBox);
        incompatibleScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        incompatibleScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        incompatibleScrollArea->setMinimumWidth(200);

        QHBoxLayout* pluginBoxesLayout = new QHBoxLayout();
        pluginBoxesLayout->addWidget(compatibleScrollArea);
        pluginBoxesLayout->addWidget(incompatibleScrollArea);

        QHBoxLayout* buttonLayout = new QHBoxLayout();
        StreamUP::UIHelpers::CreateButton(buttonLayout, obs_module_text("Close"), [dialog]() { dialog->close(); });

        pluginBoxesLayout->setAlignment(Qt::AlignHCenter);
        dialogLayout->addLayout(pluginBoxesLayout);
        dialogLayout->addLayout(buttonLayout);

        dialog->setLayout(dialogLayout);
        dialog->show();
    });
}

} // namespace SettingsManager
} // namespace StreamUP