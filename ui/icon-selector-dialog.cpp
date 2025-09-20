#include "icon-selector-dialog.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QStyle>
#include <QPixmap>
#include <QStandardPaths>
#include <QMessageBox>
#include <QImageReader>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QUrl>
#include <QDebug>
#include <QFileInfo>
#include <QLabel>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

namespace StreamUP {

IconSelectorDialog::IconSelectorDialog(const QString& currentIcon,
                                      const QString& currentCustomIcon,
                                      bool useCustomIconFlag,
                                      QWidget* parent)
    : QDialog(parent)
    , iconButtonGroup(new QButtonGroup(this))
{
    setWindowTitle(obs_module_text("IconSelector.Dialog.Title"));
    setModal(true);
    resize(700, 600);

    // Apply StreamUP dialog styling
    setStyleSheet(UIStyles::GetDialogStyle());

    // Determine the initial selected path
    if (useCustomIconFlag && !currentCustomIcon.isEmpty()) {
        selectedIconPath = currentCustomIcon;
    } else if (!currentIcon.isEmpty()) {
        selectedIconPath = currentIcon;
    }

    loadCustomIconHistory();
    setupUI();

    // Set initial selection after UI is set up
    if (!selectedIconPath.isEmpty()) {
        // Find and select the current icon button
        for (QAbstractButton* button : iconButtonGroup->buttons()) {
            if (button->property("iconPath").toString() == selectedIconPath) {
                button->setChecked(true);
                break;
            }
        }

        // If it's a custom icon, also set it in the custom tab
        if (useCustomIconFlag && !currentCustomIcon.isEmpty()) {
            customIconPath->setText(currentCustomIcon);
        }
    }
}

void IconSelectorDialog::setupUI() {
    mainLayout = new QVBoxLayout(this);

    // Icon tabs
    setupIconTabs();
    mainLayout->addWidget(iconTabs);

    // Dialog buttons
    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    okButton = new QPushButton(obs_module_text("UI.Button.OK"), this);
    cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), this);

    okButton->setStyleSheet(UIStyles::GetButtonStyle());
    cancelButton->setStyleSheet(UIStyles::GetButtonStyle());
    
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void IconSelectorDialog::setupIconTabs() {
    iconTabs = new QTabWidget(this);

    // OBS Icons Tab
    obsScrollArea = new QScrollArea(iconTabs);
    obsIconsWidget = new QWidget();
    obsIconsLayout = new QGridLayout(obsIconsWidget);
    obsScrollArea->setWidget(obsIconsWidget);
    obsScrollArea->setWidgetResizable(true);
    iconTabs->addTab(obsScrollArea, "OBS Icons");

    // Common Icons Tab
    commonScrollArea = new QScrollArea(iconTabs);
    commonIconsWidget = new QWidget();
    commonIconsLayout = new QGridLayout(commonIconsWidget);
    commonScrollArea->setWidget(commonIconsWidget);
    commonScrollArea->setWidgetResizable(true);
    iconTabs->addTab(commonScrollArea, "Common Icons");

    // Custom Icons Tab
    customScrollArea = new QScrollArea(iconTabs);
    customIconsWidget = new QWidget();
    QVBoxLayout* customTabLayout = new QVBoxLayout(customIconsWidget);

    // Custom icon path selection
    QHBoxLayout* customPathLayout = new QHBoxLayout();
    customIconPath = new QLineEdit(customIconsWidget);
    customIconPath->setPlaceholderText(obs_module_text("IconSelector.Placeholder.Path"));
    browseCustomButton = new QPushButton(obs_module_text("UI.Button.Browse"), customIconsWidget);
    browseCustomButton->setStyleSheet(UIStyles::GetButtonStyle());

    customPathLayout->addWidget(customIconPath);
    customPathLayout->addWidget(browseCustomButton);
    customTabLayout->addLayout(customPathLayout);

    // Custom icon history grid
    customIconsLayout = new QGridLayout();
    customTabLayout->addLayout(customIconsLayout);
    customTabLayout->addStretch();

    customScrollArea->setWidget(customIconsWidget);
    customScrollArea->setWidgetResizable(true);
    iconTabs->addTab(customScrollArea, "Custom Icons");

    // Connect signals
    connect(browseCustomButton, &QPushButton::clicked, this, &IconSelectorDialog::onBrowseCustomIcon);
    connect(customIconPath, &QLineEdit::textChanged, this, &IconSelectorDialog::onCustomIconPathChanged);

    // Populate icons
    populateOBSIcons();
    populateCommonIcons();
    populateCustomIcons();
}

void IconSelectorDialog::populateOBSIcons() {
    // Try multiple methods to find OBS data directory
    QStringList possibleObsPaths;
    
    // Method 1: Try to find OBS installation directory from the running process
    QString obsExePath = QCoreApplication::applicationFilePath();
    QDir obsDir(QFileInfo(obsExePath).absoluteDir());
    if (obsDir.exists("data/obs-studio/themes")) {
        possibleObsPaths.append(obsDir.absoluteFilePath("data/obs-studio/themes"));
    }
    
    // Method 2: Try common OBS installation paths on Windows
    QStringList commonPaths = {
        "C:/Program Files/obs-studio/data/obs-studio/themes",
        "C:/Program Files (x86)/obs-studio/data/obs-studio/themes",
        QDir::homePath() + "/AppData/Local/obs-studio/data/obs-studio/themes",
        obsDir.absolutePath() + "/../data/obs-studio/themes",
        obsDir.absolutePath() + "/data/obs-studio/themes"
    };
    
    for (const QString& path : commonPaths) {
        if (QDir(path).exists()) {
            possibleObsPaths.append(path);
        }
    }
    
    // Method 3: Try to find it relative to the current executable
    QDir currentDir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 3; ++i) { // Go up max 3 levels
        if (currentDir.exists("data/obs-studio/themes")) {
            possibleObsPaths.append(currentDir.absoluteFilePath("data/obs-studio/themes"));
            break;
        }
        if (!currentDir.cdUp()) break;
    }
    
    // Use the first valid path we found
    QString themesPath;
    for (const QString& path : possibleObsPaths) {
        if (QDir(path).exists()) {
            themesPath = path;
            break;
        }
    }
    
    if (themesPath.isEmpty()) {
        // Add fallback message
        QLabel* noIconsLabel = new QLabel(obs_module_text("IconSelector.Error.NoThemeDir"));
        noIconsLabel->setAlignment(Qt::AlignCenter);
        noIconsLabel->setStyleSheet("color: gray; font-style: italic;");
        obsIconsLayout->addWidget(noIconsLabel, 0, 0, 1, GRID_COLUMNS);
        return;
    }
    
    // Determine current theme
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
    bool isDark = obs_frontend_is_theme_dark();
    QString currentTheme = isDark ? "Dark" : "Light";
#else
    QString currentTheme = "Dark"; // Fallback
#endif
    
    // Load icons from current theme only to avoid duplicates and errors
    QString themeIconPath = themesPath + "/" + currentTheme;
    QDir themeDir(themeIconPath);
    
    if (!themeDir.exists()) {
        QLabel* noIconsLabel = new QLabel(obs_module_text("IconSelector.Error.NoThemeDir"));
        noIconsLabel->setAlignment(Qt::AlignCenter);
        noIconsLabel->setStyleSheet("color: gray; font-style: italic;");
        obsIconsLayout->addWidget(noIconsLabel, 0, 0, 1, GRID_COLUMNS);
        return;
    }
    
    QStringList iconFilters;
    iconFilters << "*.svg" << "*.png" << "*.jpg" << "*.jpeg" << "*.ico" << "*.bmp";
    
    int row = 0, col = 0;
    QSet<QString> processedIcons; // Avoid duplicates
    
    // Helper function to process icons from a directory
    auto processIconsFromDir = [&](const QDir& dir, const QString& prefix = QString()) {
        QStringList iconFiles = dir.entryList(iconFilters, QDir::Files, QDir::Name);
        
        for (const QString& file : iconFiles) {
            QString fullPath = dir.absoluteFilePath(file);
            QString iconName = QFileInfo(file).baseName();
            
            // Skip if we've already processed this icon name
            if (processedIcons.contains(iconName)) continue;
            processedIcons.insert(iconName);
            
            // Only include actual files that exist
            if (QFile::exists(fullPath)) {
                QString displayName = prefix.isEmpty() ? iconName : QString("%1/%2").arg(prefix).arg(iconName);
                
                obsIcons.append({fullPath, displayName});
                createIconButton(fullPath, displayName, obsIconsLayout, row, col, "obs");
            }
        }
    };
    
    // Process main theme directory
    processIconsFromDir(themeDir);
    
    // Process subdirectories (like settings/, sources/, media/)
    QStringList subDirs = themeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subDir : subDirs) {
        QDir subDirectory(themeIconPath + "/" + subDir);
        if (subDirectory.exists()) {
            processIconsFromDir(subDirectory, subDir);
        }
    }
    
    if (row == 0 && col == 0) {
        // No icons found, add a placeholder
        QLabel* noIconsLabel = new QLabel(obs_module_text("IconSelector.Error.NoValidIcons"));
        noIconsLabel->setAlignment(Qt::AlignCenter);
        noIconsLabel->setStyleSheet("color: gray; font-style: italic;");
        obsIconsLayout->addWidget(noIconsLabel, 0, 0, 1, GRID_COLUMNS);
    } else {
        // Add some spacing at the end
        obsIconsLayout->setRowStretch(row + 1, 1);
    }
}


void IconSelectorDialog::populateCommonIcons() {
    // Use standard Qt icons that are guaranteed to exist
    struct IconMapping {
        QStyle::StandardPixmap pixmap;
        QString name;
        QString displayName;
    };
    
    QList<IconMapping> standardIcons = {
        // Media controls
        {QStyle::SP_MediaPlay, "play", "Play"},
        {QStyle::SP_MediaStop, "stop", "Stop"},
        {QStyle::SP_MediaPause, "pause", "Pause"},
        {QStyle::SP_MediaSkipForward, "skip-forward", "Skip Forward"},
        {QStyle::SP_MediaSkipBackward, "skip-backward", "Skip Backward"},
        {QStyle::SP_MediaSeekForward, "seek-forward", "Seek Forward"},
        {QStyle::SP_MediaSeekBackward, "seek-backward", "Seek Backward"},
        {QStyle::SP_MediaVolume, "volume", "Volume"},
        {QStyle::SP_MediaVolumeMuted, "volume-muted", "Volume Muted"},
        
        // Dialog buttons
        {QStyle::SP_DialogSaveButton, "save", "Save"},
        {QStyle::SP_DialogOpenButton, "open", "Open"},
        {QStyle::SP_DialogCloseButton, "close", "Close"},
        {QStyle::SP_DialogOkButton, "ok", "OK"},
        {QStyle::SP_DialogCancelButton, "cancel", "Cancel"},
        {QStyle::SP_DialogApplyButton, "apply", "Apply"},
        {QStyle::SP_DialogResetButton, "reset", "Reset"},
        {QStyle::SP_DialogHelpButton, "help", "Help"},
        
        // Navigation arrows
        {QStyle::SP_ArrowUp, "up", "Up"},
        {QStyle::SP_ArrowDown, "down", "Down"},
        {QStyle::SP_ArrowLeft, "left", "Left"},
        {QStyle::SP_ArrowRight, "right", "Right"},
        
        // File/Folder icons
        {QStyle::SP_ComputerIcon, "computer", "Computer"},
        {QStyle::SP_DirIcon, "folder", "Folder"},
        {QStyle::SP_FileIcon, "file", "File"},
        {QStyle::SP_DirClosedIcon, "folder-closed", "Folder Closed"},
        {QStyle::SP_DirOpenIcon, "folder-open", "Folder Open"},
        {QStyle::SP_DriveHDIcon, "drive", "Hard Drive"},
        {QStyle::SP_DriveCDIcon, "cd", "CD Drive"},
        {QStyle::SP_DriveNetIcon, "network", "Network Drive"},
        
        // System icons
        {QStyle::SP_BrowserReload, "refresh", "Refresh"},
        {QStyle::SP_TrashIcon, "trash", "Trash"},
        {QStyle::SP_DesktopIcon, "desktop", "Desktop"},
        {QStyle::SP_TitleBarMenuButton, "menu", "Menu"},
        {QStyle::SP_TitleBarMinButton, "minimize", "Minimize"},
        {QStyle::SP_TitleBarMaxButton, "maximize", "Maximize"},
        {QStyle::SP_TitleBarCloseButton, "window-close", "Close Window"},
        
        // Message icons
        {QStyle::SP_MessageBoxInformation, "info", "Information"},
        {QStyle::SP_MessageBoxWarning, "warning", "Warning"},
        {QStyle::SP_MessageBoxCritical, "error", "Error"},
        {QStyle::SP_MessageBoxQuestion, "question", "Question"}
    };
    
    int row = 0, col = 0;
    
    // Add Qt standard icons
    for (const IconMapping& iconMapping : standardIcons) {
        commonIcons.append({iconMapping.name, iconMapping.displayName});

        QToolButton* button = new QToolButton();
        button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);

        QIcon icon = QApplication::style()->standardIcon(iconMapping.pixmap);
        button->setIcon(icon);
        button->setToolTip(iconMapping.displayName);

        button->setProperty("iconPath", iconMapping.name);
        button->setProperty("category", "common");

        // Add styling to make selection more visible
        button->setStyleSheet(
            "QToolButton { border: 2px solid transparent; border-radius: 4px; }"
            "QToolButton:checked { border: 2px solid #007ACC; background-color: rgba(0, 122, 204, 0.2); }"
            "QToolButton:hover { border: 2px solid #005A9E; background-color: rgba(0, 90, 158, 0.1); }"
        );

        iconButtonGroup->addButton(button);
        connect(button, &QToolButton::clicked, this, &IconSelectorDialog::onIconButtonClicked);

        commonIconsLayout->addWidget(button, row, col);
        
        col++;
        if (col >= GRID_COLUMNS) {
            col = 0;
            row++;
        }
    }
    
    // Add StreamUP plugin icons from Qt resources
    // StreamUP icons are embedded as Qt resources using ":images/icons/..." paths
    
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
    bool isDark = obs_frontend_is_theme_dark();
    QString themeSuffix = isDark ? "-dark" : "-light";
#else
    QString themeSuffix = "-dark"; // Fallback
#endif
    
    // StreamUP UI icons (theme-aware)
    QStringList uiIconNames = {
        "streaming-inactive", "streaming", 
        "record-off", "record-on", "pause",
        "replay-buffer-off", "replay-buffer-on", "save-replay",
        "virtual-camera", "virtual-camera-settings",
        "studio-mode", "settings", "camera",
        "all-scene-source-locked", "all-scene-source-unlocked",
        "current-scene-source-locked", "current-scene-source-unlocked",
        "video-capture-device-activate", "video-capture-device-deactivate", 
        "video-capture-device-refresh", "refresh-browser-sources", 
        "refresh-audio-monitoring"
    };
    
    for (const QString& iconName : uiIconNames) {
        QString resourcePath = QString(":images/icons/ui/%1%2.svg").arg(iconName).arg(themeSuffix);
        
        // Check if the resource exists
        if (QFile::exists(resourcePath)) {
            QString displayName = getIconDisplayName(iconName);
            
            obsIcons.append({resourcePath, displayName});
            createIconButton(resourcePath, displayName, obsIconsLayout, row, col, "streamup");
        }
    }
    
    // StreamUP social icons
    QStringList socialIconNames = {
        "patreon", "kofi", "beer", "github", "twitter", 
        "bluesky", "doras", "discord", "streamup-logo-button", 
        "streamup-logo-stacked", "streamup-logo-text"
    };
    
    for (const QString& iconName : socialIconNames) {
        QString resourcePath = QString(":images/icons/social/%1.svg").arg(iconName);
        
        // Check if the resource exists
        if (QFile::exists(resourcePath)) {
            QString displayName = getIconDisplayName(iconName);
            
            obsIcons.append({resourcePath, displayName});
            createIconButton(resourcePath, displayName, obsIconsLayout, row, col, "streamup");
        }
    }
    
    
    // Add some spacing at the end
    obsIconsLayout->setRowStretch(row + 1, 1);
}

void IconSelectorDialog::createIconButton(const QString& iconPath, const QString& iconName, QGridLayout* layout, int& row, int& col, const QString& category) {
    QToolButton* button = new QToolButton();
    button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
    button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);

    // Add styling to make selection more visible
    button->setStyleSheet(
        "QToolButton { border: 2px solid transparent; border-radius: 4px; }"
        "QToolButton:checked { border: 2px solid #007ACC; background-color: rgba(0, 122, 204, 0.2); }"
        "QToolButton:hover { border: 2px solid #005A9E; background-color: rgba(0, 90, 158, 0.1); }"
    );

    // Load icon
    QIcon icon = loadPreviewIcon(iconPath);
    button->setIcon(icon);
    button->setToolTip(iconName); // Move text to tooltip instead

    // Store icon path in button properties
    button->setProperty("iconPath", iconPath);
    button->setProperty("category", category);

    // Add to button group
    iconButtonGroup->addButton(button);
    
    connect(button, &QToolButton::clicked, this, &IconSelectorDialog::onIconButtonClicked);
    
    layout->addWidget(button, row, col);
    
    col++;
    if (col >= GRID_COLUMNS) {
        col = 0;
        row++;
    }
}

QString IconSelectorDialog::getIconDisplayName(const QString& iconPath) {
    QString name = iconPath;
    
    // Remove common suffixes
    name.remove("-dark").remove("-light").remove("-inactive").remove("-off").remove("-on");
    
    // Convert to title case
    name.replace('-', ' ');
    name.replace('_', ' ');
    
    QStringList words = name.split(' ', Qt::SkipEmptyParts);
    for (QString& word : words) {
        if (!word.isEmpty()) {
            word[0] = word[0].toUpper();
        }
    }
    
    return words.join(' ');
}

QIcon IconSelectorDialog::loadPreviewIcon(const QString& iconPath) {
    // Try direct file path first (for full paths from OBS icons)
    QIcon directIcon(iconPath);
    if (!directIcon.isNull()) {
        return directIcon;
    }
    
    // Try to load using StreamUP's themed icon system (for icon names)
    QString themedPath = StreamUP::UIHelpers::GetThemedIconPath(iconPath);
    QIcon icon(themedPath);
    if (!icon.isNull()) {
        return icon;
    }
    
    // Try as OBS theme icon name (for icon names without full path)
    QString obsThemeIconPath = getOBSThemeIconPath(iconPath);
    if (!obsThemeIconPath.isEmpty()) {
        QIcon obsIcon(obsThemeIconPath);
        if (!obsIcon.isNull()) {
            return obsIcon;
        }
    }
    
    // Fallback to default application icon
    return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
}

QString IconSelectorDialog::getOBSThemeIconPath(const QString& iconName) {
    // Get OBS installation directory from registry or environment
    const char* obsDataPath = obs_get_module_data_path(obs_current_module());
    if (!obsDataPath) return QString();
    
    QDir obsDir(obsDataPath);
    // Navigate to OBS themes directory
    // Go up from module data path to OBS data directory
    obsDir.cdUp(); // up from obs-plugins
    obsDir.cdUp(); // up from data  
    obsDir.cd("data");
    obsDir.cd("obs-studio");
    obsDir.cd("themes");
    
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
    bool isDark = obs_frontend_is_theme_dark();
    QString themeName = isDark ? "Dark" : "Light";
#else
    QString themeName = "Dark"; // Fallback
#endif
    
    obsDir.cd(themeName);
    
    // Try different file extensions
    QStringList extensions = {".svg", ".png", ".jpg", ".jpeg"};
    for (const QString& ext : extensions) {
        QString fullPath = obsDir.absoluteFilePath(iconName + ext);
        if (QFile::exists(fullPath)) {
            return fullPath;
        }
    }
    
    return QString();
}

void IconSelectorDialog::onIconButtonClicked() {
    QToolButton* button = qobject_cast<QToolButton*>(sender());
    if (button) {
        selectedIconPath = button->property("iconPath").toString();

        // If it's a custom icon, also update the custom tab
        QString category = button->property("category").toString();
        if (category == "custom") {
            customIconPath->setText(selectedIconPath);
        }
    }
}

void IconSelectorDialog::onBrowseCustomIcon() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        obs_module_text("IconSelector.Dialog.SelectCustom"),
        QString(),
        obs_module_text("IconSelector.FileFilter.Images")
    );
    
    if (!fileName.isEmpty()) {
        customIconPath->setText(fileName);
        selectedIconPath = fileName;

        // Validate the image and save to history
        QPixmap pixmap(fileName);
        if (!pixmap.isNull()) {
            saveCustomIcon(fileName); // Save to history
        } else {
            QMessageBox::warning(this, obs_module_text("IconSelector.Error.InvalidImageTitle"), obs_module_text("IconSelector.Error.InvalidImage"));
        }
    }
}

void IconSelectorDialog::onCustomIconPathChanged() {
    QString path = customIconPath->text();
    if (!path.isEmpty()) {
        selectedIconPath = path;
    } else {
        selectedIconPath.clear();
    }
}

void IconSelectorDialog::populateCustomIcons() {
    // Clear existing custom icon buttons
    QLayoutItem* item;
    while ((item = customIconsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    int row = 0, col = 0;

    // Add custom icons from history
    for (const QString& iconPath : customIconHistory) {
        if (!QFileInfo(iconPath).exists()) {
            continue; // Skip non-existent files
        }

        QToolButton* button = new QToolButton();
        button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
        button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);

        QPixmap pixmap(iconPath);
        if (!pixmap.isNull()) {
            button->setIcon(QIcon(pixmap));
            button->setToolTip(QFileInfo(iconPath).fileName());

            button->setProperty("iconPath", iconPath);
            button->setProperty("category", "custom");

            // Add styling to make selection more visible
            button->setStyleSheet(
                "QToolButton { border: 2px solid transparent; border-radius: 4px; }"
                "QToolButton:checked { border: 2px solid #007ACC; background-color: rgba(0, 122, 204, 0.2); }"
                "QToolButton:hover { border: 2px solid #005A9E; background-color: rgba(0, 90, 158, 0.1); }"
            );

            iconButtonGroup->addButton(button);
            connect(button, &QToolButton::clicked, this, &IconSelectorDialog::onIconButtonClicked);

            customIconsLayout->addWidget(button, row, col);

            col++;
            if (col >= GRID_COLUMNS) {
                col = 0;
                row++;
            }
        }
    }
}

void IconSelectorDialog::saveCustomIcon(const QString& iconPath) {
    if (!iconPath.isEmpty() && !customIconHistory.contains(iconPath)) {
        customIconHistory.prepend(iconPath); // Add to beginning

        // Limit history size to 50 items
        while (customIconHistory.size() > 50) {
            customIconHistory.removeLast();
        }

        // Save to settings
        obs_data_t* settings = obs_data_create();
        obs_data_array_t* historyArray = obs_data_array_create();

        for (const QString& path : customIconHistory) {
            obs_data_t* item = obs_data_create();
            obs_data_set_string(item, "path", path.toUtf8().constData());
            obs_data_array_push_back(historyArray, item);
            obs_data_release(item);
        }

        obs_data_set_array(settings, "custom_icon_history", historyArray);

        // Save to module config
        char* configPath = obs_module_config_path("streamup_custom_icons.json");
        if (configPath) {
            obs_data_save_json(settings, configPath);
            bfree(configPath);
        }

        obs_data_array_release(historyArray);
        obs_data_release(settings);

        // Refresh the custom icons display
        populateCustomIcons();
    }
}

void IconSelectorDialog::loadCustomIconHistory() {
    char* configPath = obs_module_config_path("streamup_custom_icons.json");
    if (!configPath) {
        return;
    }

    obs_data_t* settings = obs_data_create_from_json_file(configPath);
    if (settings) {
        obs_data_array_t* historyArray = obs_data_get_array(settings, "custom_icon_history");
        if (historyArray) {
            size_t count = obs_data_array_count(historyArray);
            for (size_t i = 0; i < count; i++) {
                obs_data_t* item = obs_data_array_item(historyArray, i);
                const char* path = obs_data_get_string(item, "path");
                if (path && strlen(path) > 0) {
                    customIconHistory.append(QString::fromUtf8(path));
                }
                obs_data_release(item);
            }
            obs_data_array_release(historyArray);
        }
        obs_data_release(settings);
    }

    bfree(configPath);
}

QString IconSelectorDialog::getSelectedIcon() const {
    return selectedIconPath;
}


} // namespace StreamUP

#include "icon-selector-dialog.moc"