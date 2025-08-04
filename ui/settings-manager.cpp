#include "settings-manager.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "plugin-manager.hpp"
#include "hotkey-manager.hpp"
#include "hotkey-widget.hpp"
#include <obs-module.h>
#include <obs-properties.h>
#include <obs-frontend-api.h>
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
#include <QTimer>
#include <QSizePolicy>
#include <QPointer>
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

        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("WindowSettingsTitle"));
        
        // Start with compact initial size - will expand based on content
        dialog->resize(500, 400);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px; }")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL));
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setProperty("isMainHeaderLayout", true); // Mark for later reference
        
        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsMainTitle"));
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setProperty("isMainTitle", true);
        headerLayout->addWidget(titleLabel);
        
        mainLayout->addWidget(headerWidget);

        // Content area with scroll
        QScrollArea* scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
            StreamUP::UIStyles::Sizes::PADDING_XL, 
            StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
            StreamUP::UIStyles::Sizes::PADDING_XL);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

        obs_properties_t* props = obs_properties_create();

        // General Settings Group
        QGroupBox* generalGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsGeneralGroup"), "info");
        
        QVBoxLayout* generalLayout = new QVBoxLayout(generalGroup);
        generalLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        // Run at startup setting
        obs_property_t* runAtStartupProp =
            obs_properties_add_bool(props, "run_at_startup", obs_module_text("WindowSettingsRunOnStartup"));

        QString checkboxStyle = QString(
            "QCheckBox {"
            "color: %1;"
            "font-size: %2px;"
            "spacing: %3px;"
            "}"
            "QCheckBox::indicator {"
            "width: 18px;"
            "height: 18px;"
            "border: 2px solid %4;"
            "border-radius: 3px;"
            "background: %5;"
            "}"
            "QCheckBox::indicator:checked {"
            "background: %6;"
            "border: 2px solid %6;"
            "}"
            "QCheckBox::indicator:checked:hover {"
            "background: %7;"
            "}")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
            .arg(StreamUP::UIStyles::Sizes::SPACING_SMALL)
            .arg(StreamUP::UIStyles::Colors::BORDER_LIGHT)
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_INPUT)
            .arg(StreamUP::UIStyles::Colors::INFO)
            .arg(StreamUP::UIStyles::Colors::INFO_HOVER);
            
        QCheckBox* runAtStartupCheckBox = new QCheckBox(obs_module_text("WindowSettingsRunOnStartup"));
        runAtStartupCheckBox->setChecked(obs_data_get_bool(settings, obs_property_name(runAtStartupProp)));
        runAtStartupCheckBox->setStyleSheet(checkboxStyle);

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
        notificationsMuteCheckBox->setStyleSheet(checkboxStyle);

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
        QGroupBox* pluginGroup = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsPluginManagementGroup"), "info");
        
        QVBoxLayout* pluginLayout = new QVBoxLayout(pluginGroup);
        pluginLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        QPushButton* pluginButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsViewInstalledPlugins"), "info");
        
        // Connect plugin button to show plugins inline with dynamic sizing
        QObject::connect(pluginButton, &QPushButton::clicked, [dialog, scrollArea, contentWidget]() {
            StreamUP::SettingsManager::ShowInstalledPluginsInline(scrollArea, contentWidget, dialog);
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
        
        // Connect hotkeys button to show hotkeys inline with dynamic sizing
        QObject::connect(hotkeysButton, &QPushButton::clicked, [dialog, scrollArea, contentWidget]() {
            StreamUP::SettingsManager::ShowHotkeysInline(scrollArea, contentWidget, dialog);
        });
        
        QHBoxLayout* hotkeysButtonLayout = new QHBoxLayout();
        hotkeysButtonLayout->addStretch();
        hotkeysButtonLayout->addWidget(hotkeysButton);
        hotkeysButtonLayout->addStretch();
        hotkeysLayout->addLayout(hotkeysButtonLayout);
        
        contentLayout->addWidget(hotkeysGroup);
        contentLayout->addStretch();
        
        scrollArea->setWidget(contentWidget);
        mainLayout->addWidget(scrollArea);

        // Bottom button area
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet(QString("background: %1; padding: %2px;")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL));
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 0, 
            StreamUP::UIStyles::Sizes::PADDING_XL, 0);

        QPushButton* cancelButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Cancel"), "neutral");
        QObject::connect(cancelButton, &QPushButton::clicked, [dialog, settings]() {
            obs_data_release(settings);
            dialog->close();
        });

        QPushButton* saveButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Save"), "success");
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

        // Store the dialog reference
        settingsDialog = dialog;
        
        // Apply dynamic sizing that hugs content (this will handle showing the dialog)
        StreamUP::UIStyles::ApplyDynamicSizing(dialog, 500, 900, 400, 650);
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

void ShowInstalledPluginsInline(QScrollArea* scrollArea, QWidget* originalContent, QDialog* parentDialog)
{
    // Store the current widget temporarily
    QWidget* currentWidget = scrollArea->takeWidget();
    
    // Find and update the main header with back button
    if (parentDialog) {
        QLabel* mainTitle = parentDialog->findChild<QLabel*>();
        if (mainTitle && mainTitle->property("isMainTitle").toBool()) {
            
            // Get the header widget
            QWidget* headerWidget = qobject_cast<QWidget*>(mainTitle->parent());
            if (headerWidget) {
                QVBoxLayout* headerLayout = qobject_cast<QVBoxLayout*>(headerWidget->layout());
                if (headerLayout && headerLayout->property("isMainHeaderLayout").toBool()) {
                    
                    // Create back button with compact sizing
                    QPushButton* backButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsBackButton"), "neutral");
                    backButton->setParent(headerWidget);
                    backButton->setProperty("isBackButton", true);
                    
                    // Set explicit size to prevent stretching
                    backButton->setFixedSize(80, 30);
                    backButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                    
                    backButton->show();
                    
                    // Position back button absolutely on the left
                    backButton->move(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
                                   (headerWidget->height() - backButton->height()) / 2);
                    
                    QObject::connect(backButton, &QPushButton::clicked, [scrollArea, originalContent, parentDialog, backButton, headerLayout]() {
                        // Remove back button
                        if (backButton) {
                            backButton->hide();
                            backButton->deleteLater();
                        }
                        
                        // No subtitle cleanup needed since we don't add one
                        
                        // Restore original content
                        QWidget* currentWidget = scrollArea->takeWidget();
                        if (currentWidget) {
                            currentWidget->deleteLater();
                        }
                        scrollArea->setWidget(originalContent);
                        
                        // Resize dialog back to compact settings size
                        if (parentDialog && parentDialog->property("dynamicSizing").toBool()) {
                            StreamUP::UIStyles::ApplyDynamicSizing(parentDialog, 500, 900, 400, 650);
                        }
                    });
                    
                    // Don't add subtitle to header - it will be in the content area instead
                    
                    // Ensure back button is positioned correctly after layout updates
                    QTimer::singleShot(10, [backButton, headerWidget]() {
                        if (backButton && headerWidget) {
                            backButton->move(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
                                           (headerWidget->height() - backButton->height()) / 2);
                        }
                    });
                }
            }
        }
    }
    
    // Create replacement content widget  
    QWidget* pluginsWidget = new QWidget();
    pluginsWidget->setStyleSheet(QString("background: transparent;")); // Remove dark background
    QVBoxLayout* pluginsLayout = new QVBoxLayout(pluginsWidget);
    pluginsLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    pluginsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Title and description - closer together
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsInstalledPluginsTitle"));
    pluginsLayout->addWidget(titleLabel);
    
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WindowSettingsInstalledPluginsDesc"));
    pluginsLayout->addWidget(descLabel);
    
    // Reduce spacing after header
    pluginsLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
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
    pluginsLayout->addWidget(infoLabel);

    auto installedPlugins = StreamUP::PluginManager::GetInstalledPlugins();
    
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
                                     "</a> <span style=\"color: #9ca3af;\">("+ QString::fromStdString(pluginVersion) + ")</span><br>";
        }
        if (compatiblePluginsString.endsWith("<br>")) {
            compatiblePluginsString.chop(4);
        }
    }

    QLabel* compatiblePluginsList = new QLabel(compatiblePluginsString);
    compatiblePluginsList->setOpenExternalLinks(true);
    compatiblePluginsList->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "line-height: 1.4;"
        "padding: %3px;"
        "background: transparent;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
        .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2));
    compatiblePluginsList->setWordWrap(true);

    // Create GroupBox with prioritized width and internal scrolling
    QGroupBox* compatiblePluginsBox = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsCompatiblePlugins"), "success");
    compatiblePluginsBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    compatiblePluginsBox->setMinimumWidth(300);
    compatiblePluginsBox->setMaximumHeight(300);
    
    QVBoxLayout* compatiblePluginsBoxLayout = new QVBoxLayout(compatiblePluginsBox);
    compatiblePluginsBoxLayout->setContentsMargins(8, 20, 8, 8);
    
    // ScrollArea goes INSIDE the GroupBox
    QScrollArea* compatibleScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
    compatibleScrollArea->setWidget(compatiblePluginsList);
    compatibleScrollArea->setStyleSheet(compatibleScrollArea->styleSheet() + 
        "QScrollArea { background: transparent; border: none; }");
    
    compatiblePluginsBoxLayout->addWidget(compatibleScrollArea);

    QLabel* incompatiblePluginsList = new QLabel;
    char* filePath = GetFilePath();
    SetLabelWithSortedModules(incompatiblePluginsList, SearchModulesInFile(filePath));
    bfree(filePath);
    incompatiblePluginsList->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "line-height: 1.4;"
        "padding: %3px;"
        "background: transparent;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
        .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2));
    incompatiblePluginsList->setWordWrap(true);

    // Create GroupBox with smaller fixed width for incompatible
    QGroupBox* incompatiblePluginsBox = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsIncompatiblePlugins"), "error");
    incompatiblePluginsBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    incompatiblePluginsBox->setFixedWidth(200);
    incompatiblePluginsBox->setMaximumHeight(300);
    
    QVBoxLayout* incompatiblePluginsBoxLayout = new QVBoxLayout(incompatiblePluginsBox);
    incompatiblePluginsBoxLayout->setContentsMargins(8, 20, 8, 8);
    
    // ScrollArea goes INSIDE the GroupBox
    QScrollArea* incompatibleScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
    incompatibleScrollArea->setWidget(incompatiblePluginsList);
    incompatibleScrollArea->setStyleSheet(incompatibleScrollArea->styleSheet() + 
        "QScrollArea { background: transparent; border: none; }");
    
    incompatiblePluginsBoxLayout->addWidget(incompatibleScrollArea);

    QHBoxLayout* pluginBoxesLayout = new QHBoxLayout();
    pluginBoxesLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    pluginBoxesLayout->addWidget(compatiblePluginsBox, 3); // Give compatible box more space
    pluginBoxesLayout->addWidget(incompatiblePluginsBox, 1); // Give incompatible box less space

    pluginsLayout->addLayout(pluginBoxesLayout);
    pluginsLayout->addStretch();
    
    // Replace the content in the scroll area
    scrollArea->setWidget(pluginsWidget);
    
    // Make scroll area fit content height to prevent dual scrollbars
    scrollArea->setWidgetResizable(true);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Expand dialog significantly to accommodate plugin content without main scrolling
    if (parentDialog && parentDialog->property("dynamicSizing").toBool()) {
        StreamUP::UIStyles::ApplyDynamicSizing(parentDialog, 800, 1200, 600, 900);
    }
}

void ShowInstalledPluginsPage()
{
    StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
        auto installedPlugins = StreamUP::PluginManager::GetInstalledPlugins();

        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("WindowSettingsInstalledPlugins"));
        
        // Start compact and let it grow based on content
        dialog->resize(650, 400);
        
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
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
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
        compatiblePluginsList->setStyleSheet(QString(
            "QLabel {"
            "color: %1;"
            "font-size: %2px;"
            "line-height: 1.4;"
            "padding: %3px;"
            "background: transparent;"
            "}")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
            .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2));
        compatiblePluginsList->setWordWrap(true);

        // Create GroupBox with prioritized width and internal scrolling
        QGroupBox* compatiblePluginsBox = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsCompatiblePlugins"), "success");
        compatiblePluginsBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        compatiblePluginsBox->setMinimumWidth(300);
        compatiblePluginsBox->setMaximumHeight(300);
        
        QVBoxLayout* compatiblePluginsBoxLayout = new QVBoxLayout(compatiblePluginsBox);
        compatiblePluginsBoxLayout->setContentsMargins(8, 20, 8, 8);
        
        // ScrollArea goes INSIDE the GroupBox
        QScrollArea* compatibleScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
        compatibleScrollArea->setWidget(compatiblePluginsList);
        compatibleScrollArea->setStyleSheet(compatibleScrollArea->styleSheet() + 
            "QScrollArea { background: transparent; border: none; }");
        
        compatiblePluginsBoxLayout->addWidget(compatibleScrollArea);

        QLabel* incompatiblePluginsList = new QLabel;
        char* filePath = GetFilePath();
        SetLabelWithSortedModules(incompatiblePluginsList, SearchModulesInFile(filePath));
        bfree(filePath);
        incompatiblePluginsList->setStyleSheet(QString(
            "QLabel {"
            "color: %1;"
            "font-size: %2px;"
            "line-height: 1.4;"
            "padding: %3px;"
            "background: transparent;"
            "}")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
            .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY)
            .arg(StreamUP::UIStyles::Sizes::PADDING_SMALL + 2));
        incompatiblePluginsList->setWordWrap(true);

        // Create GroupBox with smaller fixed width for incompatible
        QGroupBox* incompatiblePluginsBox = StreamUP::UIStyles::CreateStyledGroupBox(obs_module_text("WindowSettingsIncompatiblePlugins"), "error");
        incompatiblePluginsBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        incompatiblePluginsBox->setFixedWidth(200);
        incompatiblePluginsBox->setMaximumHeight(300);
        
        QVBoxLayout* incompatiblePluginsBoxLayout = new QVBoxLayout(incompatiblePluginsBox);
        incompatiblePluginsBoxLayout->setContentsMargins(8, 20, 8, 8);
        
        // ScrollArea goes INSIDE the GroupBox
        QScrollArea* incompatibleScrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
        incompatibleScrollArea->setWidget(incompatiblePluginsList);
        incompatibleScrollArea->setStyleSheet(incompatibleScrollArea->styleSheet() + 
            "QScrollArea { background: transparent; border: none; }");
        
        incompatiblePluginsBoxLayout->addWidget(incompatibleScrollArea);

        QHBoxLayout* pluginBoxesLayout = new QHBoxLayout();
        pluginBoxesLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        pluginBoxesLayout->addWidget(compatiblePluginsBox, 3); // Give compatible box more space
        pluginBoxesLayout->addWidget(incompatiblePluginsBox, 1); // Give incompatible box less space

        contentLayout->addLayout(pluginBoxesLayout);

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
        
        // Apply dynamic sizing that adjusts to actual content size
        StreamUP::UIStyles::ApplyDynamicSizing(dialog, 650, 1000, 400, 800);
        dialog->show();
    });
}

void ShowHotkeysInline(QScrollArea* scrollArea, QWidget* originalContent, QDialog* parentDialog)
{
    // Store the current widget temporarily
    QWidget* currentWidget = scrollArea->takeWidget();
    
    // Find and update the main header with back button
    if (parentDialog) {
        QLabel* mainTitle = parentDialog->findChild<QLabel*>();
        if (mainTitle && mainTitle->property("isMainTitle").toBool()) {
            
            // Get the header widget
            QWidget* headerWidget = qobject_cast<QWidget*>(mainTitle->parent());
            if (headerWidget) {
                QVBoxLayout* headerLayout = qobject_cast<QVBoxLayout*>(headerWidget->layout());
                if (headerLayout && headerLayout->property("isMainHeaderLayout").toBool()) {
                    
                    // Create back button with compact sizing
                    QPushButton* backButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WindowSettingsBackButton"), "neutral");
                    backButton->setParent(headerWidget);
                    backButton->setProperty("isBackButton", true);
                    
                    // Set explicit size to prevent stretching
                    backButton->setFixedSize(80, 30);
                    backButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                    
                    backButton->show();
                    
                    // Position back button absolutely on the left
                    backButton->move(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
                                   (headerWidget->height() - backButton->height()) / 2);
                    
                    QObject::connect(backButton, &QPushButton::clicked, [scrollArea, originalContent, parentDialog, backButton, headerLayout]() {
                        // Remove back button
                        if (backButton) {
                            backButton->hide();
                            backButton->deleteLater();
                        }
                        
                        // Restore original content
                        QWidget* currentWidget = scrollArea->takeWidget();
                        if (currentWidget) {
                            currentWidget->deleteLater();
                        }
                        scrollArea->setWidget(originalContent);
                        
                        // Resize dialog back to compact settings size
                        if (parentDialog && parentDialog->property("dynamicSizing").toBool()) {
                            StreamUP::UIStyles::ApplyDynamicSizing(parentDialog, 500, 900, 400, 650);
                        }
                    });
                    
                    // Ensure back button is positioned correctly after layout updates
                    QTimer::singleShot(10, [backButton, headerWidget]() {
                        if (backButton && headerWidget) {
                            backButton->move(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
                                           (headerWidget->height() - backButton->height()) / 2);
                        }
                    });
                }
            }
        }
    }
    
    // Create replacement content widget
    QWidget* hotkeysWidget = new QWidget();
    hotkeysWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    QVBoxLayout* hotkeysLayout = new QVBoxLayout(hotkeysWidget);
    hotkeysLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    hotkeysLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Title and description
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WindowSettingsHotkeysTitle"));
    hotkeysLayout->addWidget(titleLabel);
    
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WindowSettingsHotkeysDesc"));
    hotkeysLayout->addWidget(descLabel);
    
    // Reduce spacing after header
    hotkeysLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
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
        });
    });
    
    actionLayout->addStretch();
    actionLayout->addWidget(resetButton);
    
    hotkeysGroupLayout->addLayout(actionLayout);
    hotkeysLayout->addWidget(hotkeysGroup);
    hotkeysLayout->addStretch();
    
    // Replace the content in the scroll area
    scrollArea->setWidget(hotkeysWidget);
    
    // Expand dialog to accommodate hotkeys content
    if (parentDialog && parentDialog->property("dynamicSizing").toBool()) {
        StreamUP::UIStyles::ApplyDynamicSizing(parentDialog, 700, 1000, 500, 750);
    }
}

} // namespace SettingsManager
} // namespace StreamUP
