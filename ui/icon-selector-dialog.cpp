#include "icon-selector-dialog.hpp"
#include "ui-helpers.hpp"
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
    , selectedIcon(currentIcon)
    , selectedCustomIcon(currentCustomIcon)
    , useCustomIcon(useCustomIconFlag)
    , iconButtonGroup(new QButtonGroup(this))
{
    setWindowTitle(obs_module_text("IconSelector.Dialog.Title"));
    setModal(true);
    resize(600, 500);
    
    setupUI();
    
    // Set initial selection
    if (useCustomIcon && !currentCustomIcon.isEmpty()) {
        useCustomRadio->setChecked(true);
        customIconPath->setText(currentCustomIcon);
        customIconPreview->setPixmap(QPixmap(currentCustomIcon).scaled(ICON_SIZE, ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (!currentIcon.isEmpty()) {
        useIconRadio->setChecked(true);
        // Find and select the current icon button
        for (QAbstractButton* button : iconButtonGroup->buttons()) {
            if (button->property("iconPath").toString() == currentIcon) {
                button->setChecked(true);
                break;
            }
        }
    } else {
        useIconRadio->setChecked(true);
    }
    
    onIconTypeChanged();
}

void IconSelectorDialog::setupUI() {
    mainLayout = new QVBoxLayout(this);
    
    
    // Icon selection type
    QGroupBox* selectionGroup = new QGroupBox(obs_module_text("IconSelector.Group.Source"), this);
    QVBoxLayout* selectionLayout = new QVBoxLayout(selectionGroup);
    
    useIconRadio = new QRadioButton(obs_module_text("IconSelector.Radio.BuiltIn"), selectionGroup);
    useCustomRadio = new QRadioButton(obs_module_text("IconSelector.Radio.Custom"), selectionGroup);
    
    selectionLayout->addWidget(useIconRadio);
    selectionLayout->addWidget(useCustomRadio);
    
    mainLayout->addWidget(selectionGroup);
    
    connect(useIconRadio, &QRadioButton::toggled, this, &IconSelectorDialog::onIconTypeChanged);
    connect(useCustomRadio, &QRadioButton::toggled, this, &IconSelectorDialog::onIconTypeChanged);
    
    // Icon tabs (for built-in icons)
    setupIconTabs();
    mainLayout->addWidget(iconTabs);
    
    // Custom icon section
    customIconGroup = new QGroupBox(obs_module_text("IconSelector.Group.Custom"), this);
    QVBoxLayout* customLayout = new QVBoxLayout(customIconGroup);
    
    QHBoxLayout* customPathLayout = new QHBoxLayout();
    customIconPath = new QLineEdit(customIconGroup);
    customIconPath->setPlaceholderText(obs_module_text("IconSelector.Placeholder.Path"));
    customIconPath->setReadOnly(true);
    browseCustomButton = new QPushButton(obs_module_text("UI.Button.Browse"), customIconGroup);
    
    customPathLayout->addWidget(customIconPath);
    customPathLayout->addWidget(browseCustomButton);
    customLayout->addLayout(customPathLayout);
    
    // Custom icon preview
    QHBoxLayout* previewLayout = new QHBoxLayout();
    QLabel* previewLabel = new QLabel(obs_module_text("IconSelector.Label.Preview"), customIconGroup);
    customIconPreview = new QLabel(customIconGroup);
    customIconPreview->setFixedSize(ICON_SIZE + 8, ICON_SIZE + 8);
    customIconPreview->setStyleSheet("border: 1px solid gray; background: white;");
    customIconPreview->setAlignment(Qt::AlignCenter);
    customIconPreview->setText(obs_module_text("IconSelector.Message.NoIcon"));
    
    previewLayout->addWidget(previewLabel);
    previewLayout->addWidget(customIconPreview);
    previewLayout->addStretch();
    customLayout->addLayout(previewLayout);
    
    mainLayout->addWidget(customIconGroup);
    
    connect(browseCustomButton, &QPushButton::clicked, this, &IconSelectorDialog::onBrowseCustomIcon);
    
    // Dialog buttons
    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    okButton = new QPushButton(obs_module_text("UI.Button.OK"), this);
    cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), this);
    
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
    iconTabs->addTab(obsScrollArea, obs_module_text("IconSelector.Tab.OBS"));
    
    
    // Default Hotkey Icons Tab
    defaultScrollArea = new QScrollArea(iconTabs);
    defaultIconsWidget = new QWidget();
    defaultIconsLayout = new QGridLayout(defaultIconsWidget);
    defaultScrollArea->setWidget(defaultIconsWidget);
    defaultScrollArea->setWidgetResizable(true);
    iconTabs->addTab(defaultScrollArea, obs_module_text("IconSelector.Tab.Common"));
    
    // Populate icons
    populateOBSIcons();
    populateDefaultHotkeyIcons();
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


void IconSelectorDialog::populateDefaultHotkeyIcons() {
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
        defaultIcons.append({iconMapping.name, iconMapping.displayName});
        
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
        button->setProperty("category", "default");
        
        iconButtonGroup->addButton(button);
        connect(button, &QToolButton::clicked, this, &IconSelectorDialog::onIconButtonClicked);
        
        defaultIconsLayout->addWidget(button, row, col);
        
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
            
            defaultIcons.append({resourcePath, displayName});
            createIconButton(resourcePath, displayName, defaultIconsLayout, row, col, "streamup");
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
            
            defaultIcons.append({resourcePath, displayName});
            createIconButton(resourcePath, displayName, defaultIconsLayout, row, col, "streamup");
        }
    }
    
    
    // Add some spacing at the end
    defaultIconsLayout->setRowStretch(row + 1, 1);
}

void IconSelectorDialog::createIconButton(const QString& iconPath, const QString& iconName, QGridLayout* layout, int& row, int& col, const QString& category) {
    QToolButton* button = new QToolButton();
    button->setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
    button->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    
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
        selectedIcon = button->property("iconPath").toString();
        useIconRadio->setChecked(true);
        useCustomIcon = false;
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
        selectedCustomIcon = fileName;
        
        // Show preview
        QPixmap pixmap(fileName);
        if (!pixmap.isNull()) {
            customIconPreview->setPixmap(pixmap.scaled(ICON_SIZE, ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            useCustomRadio->setChecked(true);
            useCustomIcon = true;
        } else {
            QMessageBox::warning(this, obs_module_text("IconSelector.Error.InvalidImageTitle"), obs_module_text("IconSelector.Error.InvalidImage"));
            customIconPreview->setText(obs_module_text("IconSelector.Message.Invalid"));
        }
    }
}

void IconSelectorDialog::onIconTypeChanged() {
    bool useBuiltIn = useIconRadio->isChecked();
    
    iconTabs->setEnabled(useBuiltIn);
    customIconGroup->setEnabled(!useBuiltIn);
    
    // Clear icon button selection if switching to custom
    if (!useBuiltIn) {
        iconButtonGroup->setExclusive(false);
        for (QAbstractButton* button : iconButtonGroup->buttons()) {
            button->setChecked(false);
        }
        iconButtonGroup->setExclusive(true);
    }
    
    useCustomIcon = !useBuiltIn;
}


} // namespace StreamUP

#include "icon-selector-dialog.moc"