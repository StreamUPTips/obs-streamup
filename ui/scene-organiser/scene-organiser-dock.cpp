#include "scene-organiser-dock.hpp"
#include "../ui-styles.hpp"
#include "../ui-helpers.hpp"
#include "../settings-manager.hpp"
#include "../../utilities/debug-logger.hpp"
#include "../../utilities/obs-data-helpers.hpp"
#include "../../utilities/path-utils.hpp"
#include "../../core/plugin-manager.hpp"
#include <obs-module.h>
#include <QHeaderView>
#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QInputDialog>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QMessageBox>
#include <QKeyEvent>
#include <QColorDialog>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QColor>
#include <QStyle>
#include <QFile>
#include <QTextStream>
#include <QtSvg/QSvgRenderer>
#include <QSortFilterProxyModel>
#include <QScreen>
#include <QGuiApplication>
#include <QDir>
#include <QDateTime>
#include <util/platform.h>

namespace StreamUP {
namespace SceneOrganiser {

// Optimized theme icon cache
static QHash<QString, QIcon> s_themeIconCache;
static QWidget* s_cachedMainWindow = nullptr;

// Helper function to get theme icon from OBS main window properties (cached)
static QIcon GetThemeIcon(const QString& propertyName)
{
    // Check cache first
    auto it = s_themeIconCache.find(propertyName);
    if (it != s_themeIconCache.end()) {
        return it.value();
    }

    // Find main window only once and cache it
    if (!s_cachedMainWindow) {
        for (QWidget* widget : QApplication::topLevelWidgets()) {
            if (widget->objectName() == "OBSBasic") {
                s_cachedMainWindow = widget;
                break;
            }
        }
    }

    QIcon icon;
    if (s_cachedMainWindow) {
        // Get the icon from the main window's theme properties
        QVariant iconProperty = s_cachedMainWindow->property(propertyName.toUtf8().constData());
        if (iconProperty.isValid()) {
            icon = iconProperty.value<QIcon>();
        }
    }

    // Cache the result (even if empty)
    s_themeIconCache.insert(propertyName, icon);
    return icon;
}

// Optimized colored icon cache
static QHash<QString, QIcon> s_coloredIconCache;

// Helper function to create colored icons from SVG resources (optimized with caching)
static QIcon CreateColoredIcon(const QString& svgPath, const QColor& color, const QSize& size = QSize(16, 16))
{
    // Create cache key from path, color, and size
    QString cacheKey = QString("%1_%2_%3x%4").arg(svgPath, color.name(), QString::number(size.width()), QString::number(size.height()));

    // Check cache first
    auto it = s_coloredIconCache.find(cacheKey);
    if (it != s_coloredIconCache.end()) {
        return it.value();
    }

    // Create the icon only if not cached
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QSvgRenderer renderer(svgPath);
    if (renderer.isValid()) {
        renderer.render(&painter);

        // Apply color overlay
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
    }

    QIcon icon(pixmap);

    // Cache the result for future use
    s_coloredIconCache.insert(cacheKey, icon);

    return icon;
}

// Cache management functions
static void ClearIconCaches()
{
    s_themeIconCache.clear();
    s_coloredIconCache.clear();
    s_cachedMainWindow = nullptr;
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cache", "Cleared icon caches");
}

// Theme change handler (call this when theme changes)
static void OnThemeChanged()
{
    // Clear theme-dependent caches
    s_themeIconCache.clear();
    s_coloredIconCache.clear(); // Color icons may also be theme-dependent
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Theme", "Cleared caches for theme change");
}

// Static member initialization
QList<SceneOrganiserDock*> SceneOrganiserDock::s_dockInstances;

//==============================================================================
// SceneOrganiserDock Implementation
//==============================================================================

SceneOrganiserDock::SceneOrganiserDock(CanvasType canvasType, QWidget *parent)
    : QFrame(parent)
    , m_canvasType(canvasType)
    , m_mainLayout(nullptr)
    , m_treeView(nullptr)
    , m_model(nullptr)
    , m_proxyModel(nullptr)
    , m_searchWidget(nullptr)
    , m_searchLayout(nullptr)
    , m_searchEdit(nullptr)
    , m_toolbar(nullptr)
    , m_addFolderAction(nullptr)
    , m_removeAction(nullptr)
    , m_filtersAction(nullptr)
    , m_moveUpAction(nullptr)
    , m_moveDownAction(nullptr)
    , m_addButton(nullptr)
    , m_removeButton(nullptr)
    , m_filtersButton(nullptr)
    , m_moveUpButton(nullptr)
    , m_moveDownButton(nullptr)
    , m_expandCollapseButton(nullptr)
    , m_lockButton(nullptr)
    , m_settingsButton(nullptr)
    , m_folderContextMenu(nullptr)
    , m_sceneContextMenu(nullptr)
    , m_backgroundContextMenu(nullptr)
    , m_sceneOrderMenu(nullptr)
    , m_sceneProjectorMenu(nullptr)
    , m_folderLockAction(nullptr)
    , m_sceneLockAction(nullptr)
    , m_backgroundLockAction(nullptr)
    , m_saveTimer(new QTimer(this))
    , m_copyFiltersSource(nullptr)
    , m_currentContextItem(nullptr)
    , m_isLocked(false)
    , m_initialLoadComplete(false)
    , m_allExpanded(false)
    , m_hideSceneAction(nullptr)
    , m_showSceneAction(nullptr)
    , m_updateBatchTimer(new QTimer(this))
    , m_updatesPending(false)
    , currentThemeIsDark(false)
{
    s_dockInstances.append(this);

    // Set configuration key
    m_configKey = "scene_organiser_normal";

    // Initialize optimized update system
    m_updateBatchTimer->setSingleShot(true);
    m_updateBatchTimer->setInterval(30); // 30ms batching delay for smooth updates
    connect(m_updateBatchTimer, &QTimer::timeout, this, &SceneOrganiserDock::processBatchedUpdates);

    setupSearchBar();
    setupUI();
    setupContextMenu();
    setupObsSignals();

    // Setup auto-save timer
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(300); // Save 300ms after last change for responsive saving
    connect(m_saveTimer, &QTimer::timeout, this, &SceneOrganiserDock::SaveConfiguration);


    // Load configuration after setup
    QTimer::singleShot(100, this, &SceneOrganiserDock::LoadConfiguration);

    // Initialize current theme state
    currentThemeIsDark = StreamUP::UIHelpers::IsOBSThemeDark();


    // Initialize toggle icons state in context menus
    updateToggleIconsState();

    // Initialize lock action states in context menus
    updateLockActionStates();

    // Initialize active scene highlighting
    QTimer::singleShot(200, this, &SceneOrganiserDock::updateActiveSceneHighlight);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Initialize",
        "Scene Organiser Dock created for normal canvas");
}

SceneOrganiserDock::~SceneOrganiserDock()
{
    // Remove frontend event callback to prevent use-after-free
    obs_frontend_remove_event_callback(onFrontendEvent, this);

    // Clean up copy filters source
    if (m_copyFiltersSource) {
        obs_weak_source_release(m_copyFiltersSource);
        m_copyFiltersSource = nullptr;
    }

    s_dockInstances.removeAll(this);
    SaveConfiguration();
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup", "Scene Organiser Dock destroyed");
}


void SceneOrganiserDock::NotifyAllDocksSettingsChanged()
{
    for (auto dock : s_dockInstances) {
        dock->onSettingsChanged();
    }
}

void SceneOrganiserDock::NotifySceneOrganiserIconsChanged()
{
    for (auto dock : s_dockInstances) {
        dock->onIconsChanged();
    }
}


void SceneOrganiserDock::setupUI()
{
    setObjectName("StreamUPSceneOrganiserNormal");

    // Use zero margins and spacing to match OBS dock style
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create model, proxy model, and view
    m_model = new SceneTreeModel(m_canvasType, this);

    // Create proxy model for search/filtering
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setRecursiveFilteringEnabled(true);

    // Create tree view - SceneTreeView only for event handling, no custom styling
    m_treeView = new SceneTreeView(this);
    m_treeView->setModel(m_proxyModel);

    // Install custom delegate to handle color painting (overrides theme stylesheet)
    CustomColorDelegate *colorDelegate = new CustomColorDelegate(this, m_treeView);
    m_treeView->setItemDelegate(colorDelegate);

    // Set object name to match OBS scenes list for proper theme styling
    // NOTE: Using "scenes" will apply OBS theme styling for the native scenes dock
    // Try changing this to see if it affects the double-selection issue
    m_treeView->setObjectName("streamupSceneOrganiser");  // Try: "sceneorganiser", "streamupScenes", or ""

    // DEBUG: Log the current stylesheet to see what's being applied
    QString currentStyleSheet = m_treeView->styleSheet();
    if (!currentStyleSheet.isEmpty()) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "TreeViewStyle",
            QString("TreeView has custom stylesheet: %1").arg(currentStyleSheet).toUtf8().constData());
    }

    // DEBUG: Log the palette to see selection colors
    QPalette pal = m_treeView->palette();
    QColor highlight = pal.color(QPalette::Highlight);
    QColor highlightText = pal.color(QPalette::HighlightedText);
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "TreeViewPalette",
        QString("Highlight color: %1, HighlightedText: %2")
        .arg(highlight.name()).arg(highlightText.name()).toUtf8().constData());

    // Configure tree view with minimal settings - match OBS scenes dock
    m_treeView->setHeaderHidden(true);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeView->setDefaultDropAction(Qt::MoveAction);
    m_treeView->setDropIndicatorShown(true);
    m_treeView->setIndentation(20);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setExpandsOnDoubleClick(false);

    // Apply initial item height setting
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    int iconSize = (32 * settings.sceneOrganiserItemHeight) / 100;
    m_treeView->setIconSize(QSize(iconSize, iconSize));

    // Apply initial font size based on height setting
    int fontSize = std::max(8, (11 * settings.sceneOrganiserItemHeight) / 100);
    QFont font = m_treeView->font();
    font.setPointSize(fontSize);
    m_treeView->setFont(font);

    // Connect signals
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SceneOrganiserDock::onSceneSelectionChanged);
    connect(m_treeView, &QAbstractItemView::clicked,
            this, &SceneOrganiserDock::onItemClicked);
    connect(m_treeView, &QAbstractItemView::doubleClicked,
            this, &SceneOrganiserDock::onItemDoubleClicked);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &SceneOrganiserDock::onCustomContextMenuRequested);

    // Connect expansion signals to save folder state and update button
    connect(m_treeView, &QTreeView::expanded, this, [this](const QModelIndex &) {
        m_saveTimer->start();
        updateExpandCollapseButtonState();
    });
    connect(m_treeView, &QTreeView::collapsed, this, [this](const QModelIndex &) {
        m_saveTimer->start();
        updateExpandCollapseButtonState();
    });

    connect(m_model, &SceneTreeModel::modelChanged,
            [this]() {
                applySortingIfEnabled();
                m_saveTimer->start();
                // Use optimized batched update instead of immediate repaints
                scheduleOptimizedUpdate();
            });

    // Tree view takes up most of the space (like OBS scenes dock)
    m_mainLayout->addWidget(m_treeView, 1);

    // Create and add the toolbar at the bottom
    createBottomToolbar();
}

void SceneOrganiserDock::createBottomToolbar()
{
    if (!m_mainLayout) {
        return;
    }

    // Add search bar above the toolbar
    if (m_searchWidget) {
        m_mainLayout->addWidget(m_searchWidget);
    }

    // Create a proper QToolBar like in the multidock
    m_toolbar = new QToolBar("Scene Organiser Controls", this);
    m_toolbar->setObjectName("SceneOrganiserBottomToolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setOrientation(Qt::Horizontal);

    // Match native OBS scenes dock styling exactly
    m_toolbar->setIconSize(QSize(16, 16));  // Standard OBS toolbar icon size

    // Let OBS theme handle all toolbar styling - no custom stylesheets
    // The toolbar will inherit appearance from the active OBS theme

    // Create QToolButtons and add them to toolbar (to properly support themeID)
    // This approach allows themeID properties to work while maintaining proper toolbar styling

    // Add button - appears as normal button without dropdown arrow
    QToolButton *addButton = new QToolButton(this);
    addButton->setObjectName("SceneOrganiserAddButton");
    addButton->setProperty("themeID", "addIconSmall");
    addButton->setProperty("class", "icon-plus");
    addButton->setToolTip("Add folder or create scene");

    // Create a menu for the add button (shown on click, no arrow indicator)
    QMenu *addMenu = new QMenu(this);
    addMenu->setObjectName("SceneOrganiserAddMenu");
    addMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddFolderClicked);
    addMenu->addAction(obs_module_text("SceneOrganiser.Action.CreateScene"), this, &SceneOrganiserDock::onCreateSceneClicked);

    // Show menu on click without dropdown arrow
    connect(addButton, &QToolButton::clicked, [this, addButton, addMenu]() {
        QPoint pos = addButton->mapToGlobal(QPoint(0, addButton->height()));
        addMenu->exec(pos);
    });

    m_toolbar->addWidget(addButton);
    m_addFolderAction = nullptr; // No separate action needed

    // Remove button
    QToolButton *removeButton = new QToolButton(this);
    removeButton->setObjectName("SceneOrganiserRemoveButton");
    removeButton->setProperty("themeID", "removeIconSmall");
    removeButton->setProperty("class", "icon-trash");
    removeButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Remove"));
    removeButton->setEnabled(false);
    connect(removeButton, &QToolButton::clicked, this, &SceneOrganiserDock::onRemoveClicked);

    m_toolbar->addWidget(removeButton);
    m_removeAction = nullptr; // Direct button connection

    // Separator after remove button
    m_toolbar->addSeparator();

    // Filters button (using theme info: .icon-filter uses url(theme:Dark/filter.svg))
    QToolButton *filtersButton = new QToolButton(this);
    filtersButton->setObjectName("SceneOrganiserFiltersButton");
    filtersButton->setProperty("class", "icon-filter");
    filtersButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Filters"));
    filtersButton->setEnabled(false);
    connect(filtersButton, &QToolButton::clicked, this, &SceneOrganiserDock::onFiltersClicked);

    m_toolbar->addWidget(filtersButton);
    m_filtersAction = nullptr; // Direct button connection

    // Separator after filters button
    m_toolbar->addSeparator();

    // Move up button
    QToolButton *moveUpButton = new QToolButton(this);
    moveUpButton->setObjectName("SceneOrganiserMoveUpButton");
    moveUpButton->setProperty("themeID", "upArrowIconSmall");
    moveUpButton->setProperty("class", "icon-up");
    moveUpButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.MoveUp"));
    moveUpButton->setEnabled(false);
    connect(moveUpButton, &QToolButton::clicked, this, &SceneOrganiserDock::onMoveUpClicked);

    m_toolbar->addWidget(moveUpButton);
    m_moveUpAction = nullptr; // Direct button connection

    // Move down button
    QToolButton *moveDownButton = new QToolButton(this);
    moveDownButton->setObjectName("SceneOrganiserMoveDownButton");
    moveDownButton->setProperty("themeID", "downArrowIconSmall");
    moveDownButton->setProperty("class", "icon-down");
    moveDownButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.MoveDown"));
    moveDownButton->setEnabled(false);
    connect(moveDownButton, &QToolButton::clicked, this, &SceneOrganiserDock::onMoveDownClicked);

    m_toolbar->addWidget(moveDownButton);
    m_moveDownAction = nullptr; // Direct button connection

    // Add spacer to push remaining buttons to the right
    QWidget *spacer = new QWidget(this);
    spacer->setObjectName("SceneOrganiserToolbarSpacer");
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    // Create container for right-aligned buttons with consistent spacing
    QWidget *rightButtonsContainer = new QWidget(this);
    rightButtonsContainer->setObjectName("SceneOrganiserRightButtonsContainer");
    rightButtonsContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *rightButtonsLayout = new QHBoxLayout(rightButtonsContainer);
    rightButtonsLayout->setContentsMargins(0, 0, 0, 0);
    rightButtonsLayout->setSpacing(2); // Match toolbar button spacing

    // Lock checkbox - using exact same approach as OBS source dock
    QCheckBox *lockCheckbox = new QCheckBox(rightButtonsContainer);
    lockCheckbox->setObjectName("SceneOrganiserLockCheckbox");
    lockCheckbox->setProperty("class", "checkbox-icon indicator-lock");
    lockCheckbox->setChecked(false); // Start unlocked
    lockCheckbox->setToolTip("Scene organizer is unlocked (click to lock)");
    lockCheckbox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(lockCheckbox, &QCheckBox::toggled, this, &SceneOrganiserDock::onToggleLockClicked);
    rightButtonsLayout->addWidget(lockCheckbox);

    // Expand/Collapse All button - using QCheckBox like OBS does for expand indicators
    QCheckBox *expandCollapseButton = new QCheckBox(rightButtonsContainer);
    expandCollapseButton->setObjectName("SceneOrganiserExpandCollapseButton");
    expandCollapseButton->setProperty("class", "checkbox-icon indicator-expand");
    expandCollapseButton->setChecked(false); // Unchecked = expanded, checked = collapsed
    expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
    expandCollapseButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(expandCollapseButton, &QCheckBox::toggled, this, &SceneOrganiserDock::onExpandCollapseAllClicked);
    rightButtonsLayout->addWidget(expandCollapseButton);

    // Add the container to the toolbar
    m_toolbar->addWidget(rightButtonsContainer);

    // Add separator before settings button (using proper toolbar separator)
    m_toolbar->addSeparator();

    // Settings button - using OBS theming (added directly to toolbar, not in container)
    QToolButton *settingsButton = new QToolButton(this);
    settingsButton->setObjectName("SceneOrganiserSettingsButton");
    settingsButton->setProperty("themeID", "configIconSmall");
    settingsButton->setProperty("class", "icon-gear");
    settingsButton->setToolTip("Open StreamUP settings for Scene Organiser");
    connect(settingsButton, &QToolButton::clicked, this, &SceneOrganiserDock::onSettingsClicked);
    m_toolbar->addWidget(settingsButton);

    // Store button references for additional state management if needed
    m_addButton = addButton;
    m_removeButton = removeButton;
    m_filtersButton = filtersButton;
    m_moveUpButton = moveUpButton;
    m_moveDownButton = moveDownButton;
    m_expandCollapseButton = expandCollapseButton;
    m_settingsButton = settingsButton;
    m_lockButton = lockCheckbox;

    // Add toolbar to the bottom of the layout
    m_mainLayout->addWidget(m_toolbar, 0); // 0 means don't stretch

    // Initialize toolbar button states
    updateToolbarState();
}

void SceneOrganiserDock::setupContextMenu()
{
    // Folder context menu
    m_folderContextMenu = new QMenu(this);
    QAction *renameFolderAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.RenameFolder"), this, &SceneOrganiserDock::onRenameFolderClicked);
    renameFolderAction->setShortcut(QKeySequence(Qt::Key_F2));

    m_deleteFolderAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.DeleteFolder"), [this]() {
        // Implement delete folder functionality
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                // Move children to root before deleting folder
                while (item->hasChildren()) {
                    auto child = item->takeChild(0);
                    m_model->invisibleRootItem()->appendRow(child);
                }
                m_model->removeRow(item->row(), item->parent() ? item->parent()->index() : QModelIndex());
                SaveConfiguration(); // Immediate save on folder deletion
            }
        }
    });
    m_deleteFolderAction->setShortcut(QKeySequence(Qt::Key_Delete));

    m_folderContextMenu->addSeparator();
    m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.SetColor"), this, &SceneOrganiserDock::onSetCustomColorClicked);
    m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ClearColor"), this, &SceneOrganiserDock::onClearCustomColorClicked);
    m_folderContextMenu->addSeparator();
    m_folderToggleIconsAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_folderToggleIconsAction->setCheckable(true);

    // Add lock/unlock option
    m_folderContextMenu->addSeparator();
    m_folderLockAction = m_folderContextMenu->addAction("", this, &SceneOrganiserDock::onToggleLockClicked);
    m_folderLockAction->setCheckable(true);

    // Sort submenu for folders
    m_folderContextMenu->addSeparator();
    QMenu *folderSortMenu = new QMenu(obs_module_text("SceneOrganiser.Action.Sort"), this);
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalAZ"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ, item);
            }
        }
    });
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalZA"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA, item);
            }
        }
    });
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.NewestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::NewestFirst, item);
            }
        }
    });
    folderSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.OldestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortManually(StreamUP::SettingsManager::SceneSortMethod::OldestFirst, item);
            }
        }
    });
    m_folderContextMenu->addMenu(folderSortMenu);

    // Scene context menu - matching OBS standard functionality
    m_sceneContextMenu = new QMenu(this);

    // Main actions
    QAction *renameSceneAction = m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename"), -1), this, &SceneOrganiserDock::onRenameSceneClicked);
    renameSceneAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Duplicate"), -1), this, &SceneOrganiserDock::onDuplicateSceneClicked);

    // Filter actions
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Copy.Filters"), -1), this, &SceneOrganiserDock::onCopyFiltersClicked);
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Paste.Filters"), -1), this, &SceneOrganiserDock::onPasteFiltersClicked);

    // Delete action
    m_sceneContextMenu->addSeparator();
    m_deleteSceneAction = m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Remove"), -1), this, &SceneOrganiserDock::onDeleteSceneClicked);
    m_deleteSceneAction->setShortcut(QKeySequence(Qt::Key_Delete));

    // Scene visibility actions
    m_hideSceneAction = m_sceneContextMenu->addAction("Hide Scene", this, &SceneOrganiserDock::onHideSceneClicked);
    m_showSceneAction = m_sceneContextMenu->addAction("Show Scene", this, &SceneOrganiserDock::onShowSceneClicked);

    // Order submenu
    m_sceneContextMenu->addSeparator();
    m_sceneOrderMenu = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order"), -1), this);
    m_sceneMoveUpAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveUp"), -1), this, &SceneOrganiserDock::onSceneMoveUpClicked);
    m_sceneMoveDownAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveDown"), -1), this, &SceneOrganiserDock::onSceneMoveDownClicked);
    m_sceneOrderMenu->addSeparator();
    m_sceneMoveToTopAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToTop"), -1), this, &SceneOrganiserDock::onSceneMoveToTopClicked);
    m_sceneMoveToBottomAction = m_sceneOrderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToBottom"), -1), this, &SceneOrganiserDock::onSceneMoveToBottomClicked);
    m_sceneContextMenu->addMenu(m_sceneOrderMenu);

    // Projector submenu
    m_sceneProjectorMenu = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Open.Scene"), -1), this);
    // We'll populate projector options dynamically when the menu is shown
    m_sceneProjectorMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Window"), -1), this, &SceneOrganiserDock::onOpenProjectorWindowClicked);
    m_sceneContextMenu->addMenu(m_sceneProjectorMenu);

    // Additional OBS actions
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Screenshot.Scene"), -1), this, &SceneOrganiserDock::onScreenshotSceneClicked);
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Filters"), -1), this, &SceneOrganiserDock::onSceneFiltersClicked);

    // Show in multiview toggle
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ShowInMultiview"), -1), this, &SceneOrganiserDock::onShowInMultiviewClicked);

    // Custom actions
    m_sceneContextMenu->addSeparator();
    m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.SetColor"), this, &SceneOrganiserDock::onSetCustomColorClicked);
    m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ClearColor"), this, &SceneOrganiserDock::onClearCustomColorClicked);
    m_sceneContextMenu->addSeparator();
    m_sceneToggleIconsAction = m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_sceneToggleIconsAction->setCheckable(true);

    // Add lock/unlock option
    m_sceneContextMenu->addSeparator();
    m_sceneLockAction = m_sceneContextMenu->addAction("", this, &SceneOrganiserDock::onToggleLockClicked);
    m_sceneLockAction->setCheckable(true);

    // Sort submenu for scenes (sorts parent folder)
    m_sceneContextMenu->addSeparator();
    QMenu *sceneSortMenu = new QMenu(obs_module_text("SceneOrganiser.Action.Sort"), this);
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalAZ"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                // Sort the parent folder (or root if no parent)
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ, parent);
            }
        }
    });
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalZA"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA, parent);
            }
        }
    });
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.NewestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::NewestFirst, parent);
            }
        }
    });
    sceneSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.OldestFirst"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
            auto item = m_model->itemFromIndex(sourceIndex);
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                QStandardItem *parent = item->parent();
                sortManually(StreamUP::SettingsManager::SceneSortMethod::OldestFirst, parent);
            }
        }
    });
    m_sceneContextMenu->addMenu(sceneSortMenu);

    // Background context menu
    m_backgroundContextMenu = new QMenu(this);
    m_backgroundContextMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("AddScene"), -1), this, &SceneOrganiserDock::onCreateSceneClicked);
    m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddFolderClicked);
    m_backgroundContextMenu->addSeparator();
    m_backgroundToggleIconsAction = m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_backgroundToggleIconsAction->setCheckable(true);

    // Add lock/unlock option
    m_backgroundContextMenu->addSeparator();
    m_backgroundLockAction = m_backgroundContextMenu->addAction("", this, &SceneOrganiserDock::onToggleLockClicked);
    m_backgroundLockAction->setCheckable(true);

    // Sort submenu for background (sorts entire tree)
    m_backgroundContextMenu->addSeparator();
    QMenu *backgroundSortMenu = new QMenu(obs_module_text("SceneOrganiser.Action.Sort"), this);
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalAZ"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ);
    });
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.AlphabeticalZA"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA);
    });
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.NewestFirst"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::NewestFirst);
    });
    backgroundSortMenu->addAction(obs_module_text("SceneOrganiser.Action.Sort.OldestFirst"), [this]() {
        sortManually(StreamUP::SettingsManager::SceneSortMethod::OldestFirst);
    });
    m_backgroundContextMenu->addMenu(backgroundSortMenu);
}

void SceneOrganiserDock::setupObsSignals()
{
    obs_frontend_add_event_callback(onFrontendEvent, this);
}

void SceneOrganiserDock::setupSearchBar()
{
    // Create search widget container
    m_searchWidget = new QWidget(this);
    m_searchLayout = new QHBoxLayout(m_searchWidget);
    m_searchLayout->setContentsMargins(4, 2, 4, 2);
    m_searchLayout->setSpacing(4);

    // Create search input
    m_searchEdit = new QLineEdit(m_searchWidget);
    m_searchEdit->setPlaceholderText(obs_module_text("SceneOrganiser.Search.Placeholder"));
    m_searchEdit->setClearButtonEnabled(true);

    // Layout search elements
    m_searchLayout->addWidget(m_searchEdit);

    // Connect search functionality
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SceneOrganiserDock::onSearchTextChanged);

    // Add Escape key shortcut to clear search
    QAction *escapeAction = new QAction(this);
    escapeAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(escapeAction, &QAction::triggered, this, &SceneOrganiserDock::onClearSearch);
    m_searchEdit->addAction(escapeAction);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search", "Search bar initialized");
}

void SceneOrganiserDock::refreshSceneList()
{
    m_model->updateTree();
    updateActiveSceneHighlight();
    updateHiddenScenesStyling();
    applySceneVisibility();
    applySortingIfEnabled();
    updateExpandCollapseButtonState();
}

void SceneOrganiserDock::applySortingIfEnabled()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

    // Don't sort if method is None
    if (settings.sceneOrganiserSortMethod == StreamUP::SettingsManager::SceneSortMethod::None) {
        return;
    }

    QStandardItem *root = m_model->invisibleRootItem();
    if (!root) return;

    bool groupFolders = settings.sceneOrganiserGroupFolders;
    auto sortMethod = settings.sceneOrganiserSortMethod;

    // Recursively sort items within each parent
    std::function<void(QStandardItem*)> sortItemsRecursive = [&](QStandardItem *parent) {
        if (!parent) return;

        int rowCount = parent->rowCount();
        if (rowCount <= 1) return;

        // Simple bubble sort to swap rows into correct position
        bool swapped;
        do {
            swapped = false;
            for (int i = 0; i < parent->rowCount() - 1; ++i) {
                QStandardItem *item1 = parent->child(i);
                QStandardItem *item2 = parent->child(i + 1);

                if (!item1 || !item2) continue;

                bool item1IsFolder = (item1->type() == SceneFolderItem::UserType + 1);
                bool item2IsFolder = (item2->type() == SceneFolderItem::UserType + 1);

                bool shouldSwap = false;

                if (groupFolders) {
                    // Folders should come before scenes
                    if (!item1IsFolder && item2IsFolder) {
                        shouldSwap = true;
                    }
                    // Within same type, sort by selected method
                    else if (item1IsFolder == item2IsFolder) {
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                            sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                            int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                                shouldSwap = (comparison > 0);
                            } else {
                                shouldSwap = (comparison < 0);
                            }
                        } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                                   sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                            // Get timestamps
                            qint64 timestamp1 = 0, timestamp2 = 0;
                            if (item1IsFolder) {
                                SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                                SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                                timestamp1 = folder1->getCreationTimestamp();
                                timestamp2 = folder2->getCreationTimestamp();
                            } else {
                                SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                                SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                                timestamp1 = scene1->getCreationTimestamp();
                                timestamp2 = scene2->getCreationTimestamp();
                            }

                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                                // Newest first: higher timestamp comes first
                                shouldSwap = (timestamp1 < timestamp2);
                            } else {
                                // Oldest first: lower timestamp comes first
                                shouldSwap = (timestamp1 > timestamp2);
                            }
                        }
                    }
                } else {
                    // Mix folders and scenes, sort by selected method
                    if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                        sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                        int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                            shouldSwap = (comparison > 0);
                        } else {
                            shouldSwap = (comparison < 0);
                        }
                    } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                               sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                        // Get timestamps (works for both folders and scenes)
                        qint64 timestamp1 = 0, timestamp2 = 0;
                        if (item1->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                            timestamp1 = folder1->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                            timestamp1 = scene1->getCreationTimestamp();
                        }

                        if (item2->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                            timestamp2 = folder2->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                            timestamp2 = scene2->getCreationTimestamp();
                        }

                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                            shouldSwap = (timestamp1 < timestamp2);
                        } else {
                            shouldSwap = (timestamp1 > timestamp2);
                        }
                    }
                }

                if (shouldSwap) {
                    // Swap the two rows
                    QList<QStandardItem*> row1 = parent->takeRow(i);
                    QList<QStandardItem*> row2 = parent->takeRow(i); // i because we just removed row i
                    parent->insertRow(i, row2);
                    parent->insertRow(i + 1, row1);
                    swapped = true;
                }
            }
        } while (swapped);

        // Recursively sort all folders
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem *item = parent->child(i);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortItemsRecursive(item);
            }
        }
    };

    sortItemsRecursive(root);
    m_model->saveSceneTree();
}

void SceneOrganiserDock::sortManually(StreamUP::SettingsManager::SceneSortMethod method, QStandardItem *parent)
{
    // If no parent specified, sort the root
    if (!parent) {
        parent = m_model->invisibleRootItem();
    }

    if (!parent) return;

    // Get grouping setting
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    bool groupFolders = settings.sceneOrganiserGroupFolders;

    // Use the same sorting logic as applySortingIfEnabled
    std::function<void(QStandardItem*, StreamUP::SettingsManager::SceneSortMethod)> sortItemsRecursive =
        [&](QStandardItem *currentParent, StreamUP::SettingsManager::SceneSortMethod sortMethod) {
        if (!currentParent) return;

        int rowCount = currentParent->rowCount();
        if (rowCount <= 1) return;

        // Simple bubble sort to swap rows into correct position
        bool swapped;
        do {
            swapped = false;
            for (int i = 0; i < currentParent->rowCount() - 1; ++i) {
                QStandardItem *item1 = currentParent->child(i);
                QStandardItem *item2 = currentParent->child(i + 1);

                if (!item1 || !item2) continue;

                bool item1IsFolder = (item1->type() == SceneFolderItem::UserType + 1);
                bool item2IsFolder = (item2->type() == SceneFolderItem::UserType + 1);

                bool shouldSwap = false;

                if (groupFolders) {
                    // Folders should come before scenes
                    if (!item1IsFolder && item2IsFolder) {
                        shouldSwap = true;
                    }
                    // Within same type, sort by selected method
                    else if (item1IsFolder == item2IsFolder) {
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                            sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                            int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                                shouldSwap = (comparison > 0);
                            } else {
                                shouldSwap = (comparison < 0);
                            }
                        } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                                   sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                            // Get timestamps
                            qint64 timestamp1 = 0, timestamp2 = 0;
                            if (item1IsFolder) {
                                SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                                SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                                timestamp1 = folder1->getCreationTimestamp();
                                timestamp2 = folder2->getCreationTimestamp();
                            } else {
                                SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                                SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                                timestamp1 = scene1->getCreationTimestamp();
                                timestamp2 = scene2->getCreationTimestamp();
                            }

                            if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                                shouldSwap = (timestamp1 < timestamp2);
                            } else {
                                shouldSwap = (timestamp1 > timestamp2);
                            }
                        }
                    }
                } else {
                    // Mix folders and scenes, sort by selected method
                    if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ ||
                        sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalZA) {
                        int comparison = QString::compare(item1->text(), item2->text(), Qt::CaseInsensitive);
                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::AlphabeticalAZ) {
                            shouldSwap = (comparison > 0);
                        } else {
                            shouldSwap = (comparison < 0);
                        }
                    } else if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst ||
                               sortMethod == StreamUP::SettingsManager::SceneSortMethod::OldestFirst) {
                        // Get timestamps (works for both folders and scenes)
                        qint64 timestamp1 = 0, timestamp2 = 0;
                        if (item1->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder1 = static_cast<SceneFolderItem*>(item1);
                            timestamp1 = folder1->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene1 = static_cast<SceneTreeItem*>(item1);
                            timestamp1 = scene1->getCreationTimestamp();
                        }

                        if (item2->type() == SceneFolderItem::UserType + 1) {
                            SceneFolderItem *folder2 = static_cast<SceneFolderItem*>(item2);
                            timestamp2 = folder2->getCreationTimestamp();
                        } else {
                            SceneTreeItem *scene2 = static_cast<SceneTreeItem*>(item2);
                            timestamp2 = scene2->getCreationTimestamp();
                        }

                        if (sortMethod == StreamUP::SettingsManager::SceneSortMethod::NewestFirst) {
                            shouldSwap = (timestamp1 < timestamp2);
                        } else {
                            shouldSwap = (timestamp1 > timestamp2);
                        }
                    }
                }

                if (shouldSwap) {
                    // Swap the two rows
                    QList<QStandardItem*> row1 = currentParent->takeRow(i);
                    QList<QStandardItem*> row2 = currentParent->takeRow(i);
                    currentParent->insertRow(i, row2);
                    currentParent->insertRow(i + 1, row1);
                    swapped = true;
                }
            }
        } while (swapped);

        // Recursively sort all folders
        for (int i = 0; i < currentParent->rowCount(); ++i) {
            QStandardItem *item = currentParent->child(i);
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                sortItemsRecursive(item, sortMethod);
            }
        }
    };

    sortItemsRecursive(parent, method);
    m_model->saveSceneTree();
}

void SceneOrganiserDock::updateFromObsScenes()
{
    refreshSceneList();
}

void SceneOrganiserDock::onSceneSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected)

    bool hasSelection = !selected.indexes().isEmpty();
    bool isScene = false;
    bool isFolder = false;
    (void)isFolder; // Suppress unused variable warning
    bool canMoveUp = false;
    bool canMoveDown = false;

    if (hasSelection) {
        QModelIndex index = selected.indexes().first();
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item) {
            isScene = (item->type() == SceneTreeItem::UserType + 2);
            isFolder = (item->type() == SceneFolderItem::UserType + 1);

            // Check if item can move up/down
            QStandardItem *parent = item->parent();
            if (!parent) parent = m_model->invisibleRootItem();

            int itemRow = item->row();
            canMoveUp = (itemRow > 0);
            canMoveDown = (itemRow < parent->rowCount() - 1);

            if (isScene) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Selection",
                    QString("Scene selected: %1").arg(item->text()).toUtf8().constData());
            }
        }
    }

    // Update toolbar button states (direct button control) - but respect lock state
    if (!m_isLocked) {
        if (m_removeButton) m_removeButton->setEnabled(hasSelection);
        if (m_filtersButton) m_filtersButton->setEnabled(isScene);
        if (m_moveUpButton) m_moveUpButton->setEnabled(hasSelection && canMoveUp);
        if (m_moveDownButton) m_moveDownButton->setEnabled(hasSelection && canMoveDown);
    }
}

void SceneOrganiserDock::updateToolbarState()
{
    if (!m_toolbar) {
        return;
    }

    // Update expand/collapse button state to reflect current folder states
    updateExpandCollapseButtonState();

    // This method can be used for additional toolbar state updates if needed
    // Currently, the selection-based state updates are handled in onSceneSelectionChanged
}

void SceneOrganiserDock::onItemClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item) {
        m_lastClickedIndex = index;
        return;
    }

    // Get current settings for later use
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

    // Check if studio mode is active - it overrides normal click behavior
    if (obs_frontend_preview_program_mode_active() && item->type() == SceneTreeItem::UserType + 2) {
        // Check if preview switching is disabled in studio mode
        if (settings.sceneOrganiserDisablePreviewSwitchingInStudioMode) {
            // Preview switching is disabled in studio mode, do nothing
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                QString("Single-click: Preview switching disabled in studio mode").toUtf8().constData());
        } else {
            // Studio mode: single-click always sets preview scene
            obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
            if (source) {
                obs_frontend_set_current_preview_scene(source);
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                    QString("Single-click: Set preview scene to '%1'").arg(item->text()).toUtf8().constData());
                obs_source_release(source);
            }
        }
    } else {
        // Normal mode: use settings to determine behavior
        // Handle single-click scene switching if enabled
        if (settings.sceneOrganiserSwitchMode == StreamUP::SettingsManager::SceneSwitchMode::SingleClick &&
            item->type() == SceneTreeItem::UserType + 2) {
            // Switch to scene on single-click
            obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
            if (source) {
                obs_frontend_set_current_scene(source);
                obs_source_release(source);
            }
        }
    }

    // Check if this is a second click on the same already-selected item for renaming
    if (m_lastClickedIndex.isValid() && m_lastClickedIndex == index) {
        if (item->type() == SceneTreeItem::UserType + 2 || item->type() == SceneFolderItem::UserType + 1) {
            // Start inline editing (but only if we're not doing single-click switching for scenes)
            // In studio mode, never allow rename on second click since single-click has special meaning
            if (!obs_frontend_preview_program_mode_active() &&
                !(settings.sceneOrganiserSwitchMode == StreamUP::SettingsManager::SceneSwitchMode::SingleClick &&
                  item->type() == SceneTreeItem::UserType + 2)) {
                m_treeView->edit(index);
            }
        }
    }

    // Update the last clicked index
    m_lastClickedIndex = index;
}

void SceneOrganiserDock::onItemDoubleClicked(const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    // Check if studio mode is active - it overrides normal double-click behavior
    if (obs_frontend_preview_program_mode_active() && item->type() == SceneTreeItem::UserType + 2) {
        // Check if scene switching is disabled in studio mode
        StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
        if (settings.sceneOrganiserDisableTransitionInStudioMode) {
            // Transition is disabled in studio mode, do nothing
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                QString("Double-click: Transition disabled in studio mode").toUtf8().constData());
        } else {
            // Studio mode: double-click transitions preview to program (goes live)
            obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
            if (source) {
                // First set as preview, then trigger transition
                obs_frontend_set_current_preview_scene(source);
                obs_frontend_preview_program_trigger_transition();
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "StudioMode",
                    QString("Double-click: Transitioned scene '%1' to program").arg(item->text()).toUtf8().constData());
                obs_source_release(source);
            }
        }
    } else {
        // Normal mode: use settings to determine behavior
        StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

        // Only switch to scene on double-click if double-click mode is enabled
        if (settings.sceneOrganiserSwitchMode == StreamUP::SettingsManager::SceneSwitchMode::DoubleClick) {
            // Switch to scene on double-click
            obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
            if (source) {
                obs_frontend_set_current_scene(source);
                obs_source_release(source);
            }
        }
    }
}

void SceneOrganiserDock::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = m_treeView->indexAt(pos);

    if (index.isValid()) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        auto item = m_model->itemFromIndex(sourceIndex);
        if (item) {
            if (item->type() == SceneFolderItem::UserType + 1) {
                showFolderContextMenu(m_treeView->mapToGlobal(pos), index);
            } else if (item->type() == SceneTreeItem::UserType + 2) {
                showSceneContextMenu(m_treeView->mapToGlobal(pos), index);
            }
        }
    } else {
        showBackgroundContextMenu(m_treeView->mapToGlobal(pos));
    }
}

void SceneOrganiserDock::showFolderContextMenu(const QPoint &pos, const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    m_currentContextItem = m_model->itemFromIndex(sourceIndex);
    m_folderContextMenu->exec(pos);
}

void SceneOrganiserDock::showSceneContextMenu(const QPoint &pos, const QModelIndex &index)
{
    QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
    m_currentContextItem = m_model->itemFromIndex(sourceIndex);

    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    // Update dynamic menu states
    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());

    // Populate projector menu with current monitors
    populateProjectorMenu();

    // Enable/disable "Paste Filters" based on whether we have copied filters
    QList<QAction*> actions = m_sceneContextMenu->actions();
    for (QAction *action : actions) {
        QString actionText = action->text();
        if (actionText.contains("Paste") && actionText.contains("Filter")) {
            action->setEnabled(m_copyFiltersSource && !obs_weak_source_expired(m_copyFiltersSource));
        }
        else if (actionText.contains("Copy") && actionText.contains("Filter")) {
            // Enable "Copy Filters" only if the scene has filters
            action->setEnabled(source && obs_source_filter_count(source) > 0);
        }
    }

    // Update "Show in Multiview" checkable state
    if (source) {
        obs_data_t *privateSettings = obs_source_get_private_settings(source);
        obs_data_set_default_bool(privateSettings, "show_in_multiview", true);
        bool showInMultiview = obs_data_get_bool(privateSettings, "show_in_multiview");

        for (QAction *action : actions) {
            if (action->text().contains("Multiview")) {
                action->setCheckable(true);
                action->setChecked(showInMultiview);
                break;
            }
        }

        obs_data_release(privateSettings);
        obs_source_release(source);
    }

    // Update hide/show scene action visibility based on current scene state
    bool sceneIsHidden = m_hiddenScenes.contains(sceneName);
    if (m_hideSceneAction) m_hideSceneAction->setVisible(!sceneIsHidden);
    if (m_showSceneAction) m_showSceneAction->setVisible(sceneIsHidden);

    // Only enable hide/show actions when unlocked
    if (m_hideSceneAction) m_hideSceneAction->setEnabled(!m_isLocked);
    if (m_showSceneAction) m_showSceneAction->setEnabled(!m_isLocked);

    m_sceneContextMenu->exec(pos);
}

void SceneOrganiserDock::showBackgroundContextMenu(const QPoint &pos)
{
    m_backgroundContextMenu->exec(pos);
}

void SceneOrganiserDock::onAddFolderClicked()
{
    bool ok;
    QString folderName = QInputDialog::getText(this,
        obs_module_text("SceneOrganiser.Dialog.AddFolder.Title"),
        obs_module_text("SceneOrganiser.Dialog.AddFolder.Text"),
        QLineEdit::Normal, QString(), &ok);

    if (ok && !folderName.isEmpty()) {
        auto folderItem = m_model->createFolderItem(folderName);
        m_model->invisibleRootItem()->appendRow(folderItem);
        m_treeView->expand(folderItem->index());
        applySortingIfEnabled();
        m_saveTimer->start();
    }
}

void SceneOrganiserDock::onCreateSceneClicked()
{
    bool ok;
    QString sceneName = QInputDialog::getText(this,
        obs_module_text("SceneOrganiser.Dialog.CreateScene.Title"),
        obs_module_text("SceneOrganiser.Dialog.CreateScene.Text"),
        QLineEdit::Normal, QString(), &ok);

    if (ok && !sceneName.isEmpty()) {
        // Create a new scene in OBS
        obs_scene_t *scene = obs_scene_create(sceneName.toUtf8().constData());
        if (scene) {
            obs_source_t *scene_source = obs_scene_get_source(scene);

            // The scene will automatically appear in our tree view due to the OBS event system
            // We don't need to manually add it here

            StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
            if (settings.sceneOrganiserSwitchToNewScene) {
                // In studio mode, only ever set the preview — never push the new scene to program
                if (obs_frontend_preview_program_mode_active()) {
                    obs_frontend_set_current_preview_scene(scene_source);
                } else {
                    obs_frontend_set_current_scene(scene_source);
                }
            }

            obs_scene_release(scene);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Creation",
                QString("Created new scene: %1").arg(sceneName).toUtf8().constData());
        }
    }
}

void SceneOrganiserDock::onRemoveClicked()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item) return;

    QString itemName = item->text();
    bool isFolder = (item->type() == SceneFolderItem::UserType + 1);
    bool isScene = (item->type() == SceneTreeItem::UserType + 2);
    QString itemType = isFolder ? "folder" : "scene";

    int ret = QMessageBox::question(this,
        obs_module_text("SceneOrganiser.Dialog.Remove.Title"),
        QString(obs_module_text("SceneOrganiser.Dialog.Remove.Text")).arg(itemType, itemName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        if (isScene) {
            // Delete the actual scene from OBS
            obs_source_t *source = obs_get_source_by_name(itemName.toUtf8().constData());
            if (source) {
                // Before removing from OBS, clean up our tracking and UI
                if (item->type() == SceneTreeItem::UserType + 2) {
                    SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
                    obs_weak_source_t *weak = sceneItem->getWeakSource();

                    // Remove from our tracking map
                    m_model->removeSceneFromTracking(weak);
                }

                // Clear selection first
                m_treeView->selectionModel()->clearSelection();

                // Remove the item from the tree view immediately
                QStandardItem *parent = item->parent();
                if (!parent) parent = m_model->invisibleRootItem();
                parent->removeRow(item->row());

                // Now remove from OBS
                obs_source_remove(source);
                obs_source_release(source);

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Removal",
                    QString("Deleted scene from OBS and removed from dock: %1").arg(itemName).toUtf8().constData());

                // Clean up any empty folders and save immediately
                m_model->cleanupEmptyItems();
                SaveConfiguration(); // Immediate save on deletion
            }
        }

        // Remove from tree model (this will be handled by OBS events for scenes)
        if (isFolder) {
            QStandardItem *parent = item->parent();
            if (!parent) parent = m_model->invisibleRootItem();
            parent->removeRow(item->row());
            SaveConfiguration(); // Immediate save on folder deletion
        }
        // For scenes, the tree will be updated automatically by OBS events
    }
}

void SceneOrganiserDock::onFiltersClicked()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = item->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (source) {
        // Open scene filters using OBS frontend API
        obs_frontend_open_source_filters(source);
        obs_source_release(source);
    }
}

void SceneOrganiserDock::onMoveUpClicked()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item) return;

    QStandardItem *parent = item->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = item->row();
    if (currentRow > 0) {
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        parent->insertRow(currentRow - 1, items);

        // Restore selection
        QModelIndex newIndex = m_model->indexFromItem(items.first());
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(newIndex);
        m_treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect);

        m_saveTimer->start();
    }
}

void SceneOrganiserDock::onMoveDownClicked()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
    QStandardItem *item = m_model->itemFromIndex(sourceIndex);
    if (!item) return;

    QStandardItem *parent = item->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = item->row();
    if (currentRow < parent->rowCount() - 1) {
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        parent->insertRow(currentRow + 1, items);

        // Restore selection
        QModelIndex newIndex = m_model->indexFromItem(items.first());
        QModelIndex proxyIndex = m_proxyModel->mapFromSource(newIndex);
        m_treeView->selectionModel()->select(proxyIndex, QItemSelectionModel::ClearAndSelect);

        m_saveTimer->start();
    }
}

void SceneOrganiserDock::onExpandCollapseAllClicked()
{
    if (!m_treeView || !m_expandCollapseButton) return;

    // Note: In OBS convention, checked = collapsed, unchecked = expanded
    // The checkbox state has already been toggled by Qt, so we read the current state
    bool isCollapsed = m_expandCollapseButton->isChecked();

    if (isCollapsed) {
        // Collapse all folders
        m_treeView->collapseAll();
        m_allExpanded = false;
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "ExpandCollapse", "Collapsed all folders");
    } else {
        // Expand all folders
        m_treeView->expandAll();
        m_allExpanded = true;
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.CollapseAll"));
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "ExpandCollapse", "Expanded all folders");
    }
}

void SceneOrganiserDock::updateExpandCollapseButtonState()
{
    if (!m_treeView || !m_expandCollapseButton || !m_proxyModel) return;

    // Count expanded and collapsed folders
    int totalFolders = 0;
    int expandedFolders = 0;

    std::function<void(const QModelIndex&)> countRecursive = [&](const QModelIndex& proxyIndex) {
        if (!proxyIndex.isValid()) return;

        // Convert to source index to check if it's a folder
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item && item->type() == QStandardItem::UserType + 1) { // Folder item
            totalFolders++;
            if (m_treeView->isExpanded(proxyIndex)) {
                expandedFolders++;
            }
        }

        // Check children
        int rowCount = m_proxyModel->rowCount(proxyIndex);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, proxyIndex);
            countRecursive(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        countRecursive(rootIndex);
    }

    // Block signals to prevent triggering the clicked handler
    m_expandCollapseButton->blockSignals(true);

    if (totalFolders == 0 || expandedFolders == 0) {
        // No folders or all folders collapsed - show collapsed state (expand icon)
        m_expandCollapseButton->setChecked(true);
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.ExpandAll"));
        m_allExpanded = false;
    } else {
        // At least one folder is expanded - show expanded state (collapse icon)
        m_expandCollapseButton->setChecked(false);
        m_expandCollapseButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.CollapseAll"));
        m_allExpanded = true;
    }

    m_expandCollapseButton->blockSignals(false);
}

void SceneOrganiserDock::onToggleIconsClicked()
{
    // Toggle the icons setting
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    settings.sceneOrganiserShowIcons = !settings.sceneOrganiserShowIcons;
    StreamUP::SettingsManager::UpdateSettings(settings);

    // Update all docks' icons immediately
    NotifySceneOrganiserIconsChanged();
}

void SceneOrganiserDock::onSetCustomColorClicked()
{
    if (!m_currentContextItem) {
        return;
    }

    // Get current color if any
    QVariant colorData = m_currentContextItem->data(Qt::UserRole + 1);
    QColor currentColor = colorData.isValid() ? colorData.value<QColor>() : QColor();

    // Open color picker dialog
    QColor selectedColor = QColorDialog::getColor(currentColor, this, "Choose Color");

    if (selectedColor.isValid()) {
        // Store the color in the item
        m_currentContextItem->setData(selectedColor, Qt::UserRole + 1);

        // Apply the color immediately
        applyCustomColorToItem(m_currentContextItem, selectedColor);

        // Save configuration
        m_saveTimer->start();

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CustomColor",
            QString("Set custom color for item '%1': %2")
            .arg(m_currentContextItem->text(), selectedColor.name()).toUtf8().constData());
    }
}

void SceneOrganiserDock::onClearCustomColorClicked()
{
    if (!m_currentContextItem) {
        return;
    }

    // Remove the custom color
    m_currentContextItem->setData(QVariant(), Qt::UserRole + 1);

    // Clear the color styling
    clearCustomColorFromItem(m_currentContextItem);

    // Save configuration
    m_saveTimer->start();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CustomColor",
        QString("Cleared custom color for item '%1'").arg(m_currentContextItem->text()).toUtf8().constData());
}

void SceneOrganiserDock::applyCustomColorToItem(QStandardItem *item, const QColor &color)
{
    if (!item || !color.isValid()) {
        return;
    }

    // Set background color
    item->setData(QBrush(color), Qt::BackgroundRole);

    // Calculate and set contrasting text color
    QColor textColor = getContrastTextColor(color);
    item->setData(QBrush(textColor), Qt::ForegroundRole);
}

void SceneOrganiserDock::clearCustomColorFromItem(QStandardItem *item)
{
    if (!item) {
        return;
    }

    // Clear background and foreground colors to use default theme colors
    item->setData(QVariant(), Qt::BackgroundRole);
    item->setData(QVariant(), Qt::ForegroundRole);
}

void SceneOrganiserDock::applyAllCustomColors(QStandardItem *parent)
{
    // If no parent provided, start from root
    if (!parent) {
        parent = m_model->invisibleRootItem();
    }

    if (!parent) {
        return;
    }

    // Apply colors recursively to all items in the tree
    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) continue;

        // Check if this item has a custom color stored
        QVariant colorData = item->data(Qt::UserRole + 1);
        if (colorData.isValid()) {
            QColor color = colorData.value<QColor>();
            if (color.isValid()) {
                applyCustomColorToItem(item, color);
            }
        }

        // Recursively apply to children
        if (item->hasChildren()) {
            applyAllCustomColors(item);
        }
    }
}

QColor SceneOrganiserDock::getContrastTextColor(const QColor &backgroundColor)
{
    // Calculate luminance using the standard formula
    double r = backgroundColor.redF();
    double g = backgroundColor.greenF();
    double b = backgroundColor.blueF();

    // Apply gamma correction
    r = (r <= 0.03928) ? r / 12.92 : qPow((r + 0.055) / 1.055, 2.4);
    g = (g <= 0.03928) ? g / 12.92 : qPow((g + 0.055) / 1.055, 2.4);
    b = (b <= 0.03928) ? b / 12.92 : qPow((b + 0.055) / 1.055, 2.4);

    // Calculate relative luminance
    double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;

    // Return white for dark backgrounds, black for light backgrounds
    return (luminance > 0.5) ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor SceneOrganiserDock::getDefaultThemeTextColor()
{
    if (m_treeView) {
        return m_treeView->palette().color(QPalette::Text);
    }
    // Fallback to a reasonable default if tree view is not available
    return QColor(255, 255, 255); // White text for dark themes (common in OBS)
}

QColor SceneOrganiserDock::adjustColorBrightness(const QColor &color, float factor)
{
    if (!color.isValid()) {
        return color;
    }

    // Convert to HSV to maintain hue and saturation while adjusting brightness
    int h, s, v;
    color.getHsv(&h, &s, &v);

    // Adjust the value (brightness) component
    v = qBound(0, static_cast<int>(v * factor), 255);

    return QColor::fromHsv(h, s, v);
}

QColor SceneOrganiserDock::getSelectionColor(const QColor &baseColor)
{
    if (!baseColor.isValid()) {
        // No custom color, use default theme selection
        return m_treeView ? m_treeView->palette().color(QPalette::Highlight) : QColor(26, 127, 207);
    }

    // For custom colors, brighten them for selection
    return adjustColorBrightness(baseColor, 1.3f); // 30% brighter
}

QColor SceneOrganiserDock::getHoverColor(const QColor &baseColor)
{
    if (!baseColor.isValid()) {
        // No custom color, use default theme hover (slightly dimmed selection)
        QColor highlight = m_treeView ? m_treeView->palette().color(QPalette::Highlight) : QColor(26, 127, 207);
        return adjustColorBrightness(highlight, 0.7f);
    }

    // For custom colors, slightly brighten them for hover
    return adjustColorBrightness(baseColor, 1.1f); // 10% brighter
}

void SceneOrganiserDock::onToggleLockClicked()
{
    setLocked(!m_isLocked);
}

void SceneOrganiserDock::onSettingsClicked()
{
    // Open StreamUP settings dialog on Scene Organiser page (tab index 2)
    StreamUP::SettingsManager::ShowSettingsDialog(2);
}

void SceneOrganiserDock::onRenameSceneClicked()
{
    auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneTreeItem::UserType + 2) return;

    // Start inline editing
    m_treeView->edit(selectedIndexes.first());
}

void SceneOrganiserDock::onRenameFolderClicked()
{
    auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
    if (selectedIndexes.isEmpty()) return;

    QModelIndex sourceIndex = m_proxyModel->mapToSource(selectedIndexes.first());
    auto item = m_model->itemFromIndex(sourceIndex);
    if (!item || item->type() != SceneFolderItem::UserType + 1) return;

    // Start inline editing
    m_treeView->edit(selectedIndexes.first());
}

void SceneOrganiserDock::setLocked(bool locked)
{
    m_isLocked = locked;

    // Update lock checkbox state and tooltip (matching OBS source dock)
    if (m_lockButton) {
        // Block signals to prevent infinite recursion when updating checkbox state
        m_lockButton->blockSignals(true);
        m_lockButton->setChecked(m_isLocked);
        m_lockButton->blockSignals(false);
        m_lockButton->setToolTip(m_isLocked ? "Scene organizer is locked (click to unlock)" : "Scene organizer is unlocked (click to lock)");
    }

    // Update UI enabled state
    updateUIEnabledState();

    // Apply scene visibility based on lock state
    applySceneVisibility();

    // Update lock action states in context menus
    updateLockActionStates();

    // Save lock state
    m_saveTimer->start();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "LockState",
        QString("Scene organizer %1").arg(m_isLocked ? "locked" : "unlocked").toUtf8().constData());
}

void SceneOrganiserDock::updateUIEnabledState()
{
    bool unlocked = !m_isLocked;

    // Check current selection for selective enabling
    bool hasSelection = false;
    bool isScene = false;
    if (m_treeView && m_treeView->selectionModel()) {
        auto selected = m_treeView->selectionModel()->selectedIndexes();
        hasSelection = !selected.isEmpty();
        if (hasSelection) {
            QModelIndex sourceIndex = m_proxyModel->mapToSource(selected.first());
            QStandardItem *item = m_model->itemFromIndex(sourceIndex);
            isScene = (item && item->type() == SceneTreeItem::UserType + 2);
        }
    }

    // NON-DESTRUCTIVE operations - always allowed when unlocked
    if (m_addButton) m_addButton->setEnabled(true); // Add scene/folder is always allowed
    if (m_filtersButton) m_filtersButton->setEnabled(isScene); // Scene filters are non-destructive

    // DESTRUCTIVE operations - only allowed when unlocked
    if (m_removeButton) m_removeButton->setEnabled(unlocked && hasSelection);
    if (m_moveUpButton) m_moveUpButton->setEnabled(unlocked && hasSelection);
    if (m_moveDownButton) m_moveDownButton->setEnabled(unlocked && hasSelection);

    // Tree view drag & drop - only allow when unlocked (moving is destructive)
    if (m_treeView) {
        m_treeView->setDragEnabled(unlocked);
        m_treeView->setAcceptDrops(unlocked);
        m_treeView->setDragDropMode(unlocked ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    }

    // Context menus - enable but control individual destructive actions
    if (m_folderContextMenu) {
        m_folderContextMenu->setEnabled(true);
    }
    if (m_sceneContextMenu) {
        m_sceneContextMenu->setEnabled(true);
    }
    if (m_backgroundContextMenu) {
        m_backgroundContextMenu->setEnabled(true);
    }

    // Control individual destructive context menu actions
    if (m_deleteFolderAction) {
        m_deleteFolderAction->setEnabled(unlocked);
    }
    if (m_deleteSceneAction) {
        m_deleteSceneAction->setEnabled(unlocked);
    }
    // Scene visibility actions - only allowed when unlocked
    if (m_hideSceneAction) {
        m_hideSceneAction->setEnabled(unlocked);
    }
    if (m_showSceneAction) {
        m_showSceneAction->setEnabled(unlocked);
    }
    if (m_sceneMoveUpAction) {
        m_sceneMoveUpAction->setEnabled(unlocked);
    }
    if (m_sceneMoveDownAction) {
        m_sceneMoveDownAction->setEnabled(unlocked);
    }
    if (m_sceneMoveToTopAction) {
        m_sceneMoveToTopAction->setEnabled(unlocked);
    }
    if (m_sceneMoveToBottomAction) {
        m_sceneMoveToBottomAction->setEnabled(unlocked);
    }
}

void SceneOrganiserDock::onSettingsChanged()
{
    // Handle settings changes if needed
    LoadConfiguration();
    applySortingIfEnabled();

    // Apply the item height setting to the tree view
    if (m_treeView) {
        StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();

        // Calculate icon size based on height percentage (50-200% range)
        // Base icon size is 32px at 100%
        int iconSize = (32 * settings.sceneOrganiserItemHeight) / 100;
        m_treeView->setIconSize(QSize(iconSize, iconSize));

        // Calculate and apply font size based on height percentage
        // Base font size is 11px at 100%, with a minimum of 8px
        int fontSize = std::max(8, (11 * settings.sceneOrganiserItemHeight) / 100);
        QFont font = m_treeView->font();
        font.setPointSize(fontSize);
        m_treeView->setFont(font);

        // Force a viewport update to reflect height changes
        m_treeView->viewport()->update();
    }
}

void SceneOrganiserDock::onIconsChanged()
{
    // Update all existing items to show/hide icons based on current setting
    if (m_model) {
        updateAllItemIcons(m_model->invisibleRootItem());
    }

    // Update checkmarks in context menus
    updateToggleIconsState();
}

void SceneOrganiserDock::updateToggleIconsState()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    bool iconsEnabled = settings.sceneOrganiserShowIcons;

    if (m_folderToggleIconsAction) {
        m_folderToggleIconsAction->setChecked(iconsEnabled);
    }
    if (m_sceneToggleIconsAction) {
        m_sceneToggleIconsAction->setChecked(iconsEnabled);
    }
    if (m_backgroundToggleIconsAction) {
        m_backgroundToggleIconsAction->setChecked(iconsEnabled);
    }
}

void SceneOrganiserDock::updateLockActionStates()
{
    QString lockText = m_isLocked ? "Unlock Scene Organiser" : "Lock Scene Organiser";

    if (m_folderLockAction) {
        m_folderLockAction->setText(lockText);
        m_folderLockAction->setChecked(m_isLocked);
    }
    if (m_sceneLockAction) {
        m_sceneLockAction->setText(lockText);
        m_sceneLockAction->setChecked(m_isLocked);
    }
    if (m_backgroundLockAction) {
        m_backgroundLockAction->setText(lockText);
        m_backgroundLockAction->setChecked(m_isLocked);
    }
}

void SceneOrganiserDock::updateActiveSceneHighlight()
{
    if (!m_model || !m_treeView) {
        return;
    }

    QString current_scene_name;
    QString preview_scene_name;

    // Get the current active (program) scene from OBS
    obs_source_t *current_scene = obs_frontend_get_current_scene();
    if (current_scene) {
        current_scene_name = QString::fromUtf8(obs_source_get_name(current_scene));
        obs_source_release(current_scene);
    }

    // If in studio mode, also get the preview scene
    if (obs_frontend_preview_program_mode_active()) {
        obs_source_t *preview_scene = obs_frontend_get_current_preview_scene();
        if (preview_scene) {
            preview_scene_name = QString::fromUtf8(obs_source_get_name(preview_scene));
            obs_source_release(preview_scene);
        }
    }

    // Update all scene items in the tree
    updateActiveSceneHighlightRecursive(m_model->invisibleRootItem(), current_scene_name, preview_scene_name);

    QString debugMsg = QString("Updated active scene highlight - Program: %1").arg(current_scene_name);
    if (!preview_scene_name.isEmpty()) {
        debugMsg += QString(", Preview: %1").arg(preview_scene_name);
    }
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "ActiveScene", debugMsg.toUtf8().constData());
}

void SceneOrganiserDock::updateActiveSceneHighlightRecursive(QStandardItem *parent, const QString &activeSceneName, const QString &previewSceneName)
{
    // DISABLED - No custom styling at all
    // Just let OBS theme handle everything
    Q_UNUSED(parent);
    Q_UNUSED(activeSceneName);
    Q_UNUSED(previewSceneName);
}

void SceneOrganiserDock::updateAllItemIcons(QStandardItem *parent)
{
    if (!parent) return;

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *item = parent->child(i);
        if (!item) continue;

        // Update icon for this item
        if (item->type() == SceneFolderItem::UserType + 1) {
            // Folder item
            SceneFolderItem *folderItem = static_cast<SceneFolderItem*>(item);
            folderItem->updateIcon();
        } else if (item->type() == SceneTreeItem::UserType + 2) {
            // Scene item
            SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
            sceneItem->updateIcon();
        }

        // Recursively update children
        updateAllItemIcons(item);
    }
}

void SceneOrganiserDock::onFrontendEvent(enum obs_frontend_event event, void *private_data)
{
    auto dock = static_cast<SceneOrganiserDock*>(private_data);

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        QTimer::singleShot(100, dock, [dock]() {
            dock->LoadConfiguration();
            dock->m_model->loadSceneTree();
            dock->refreshSceneList();
            dock->applyAllCustomColors();

            // Restore folder expansion state after tree is fully loaded
            // and mark initial load as complete to allow saves
            QTimer::singleShot(500, dock, [dock]() {
                StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
                if (settings.sceneOrganiserRememberFolderState) {
                    dock->restoreFolderExpansionState();
                }
                // Mark initial load as complete - saves are now allowed
                dock->m_initialLoadComplete = true;
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Init",
                    "Initial load complete - saves now enabled");
            });
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
        // Skip during scene collection changes to avoid accessing stale scene data
        if (!dock->m_initialLoadComplete)
            break;
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Event", "Scene list changed event received");
        QTimer::singleShot(50, dock, [dock]() {
            if (!dock->m_initialLoadComplete)
                return;
            dock->refreshSceneList();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
        // Save current scene tree and settings before switching (only if initial load completed)
        if (dock->m_initialLoadComplete) {
            dock->m_model->saveSceneTree();
            dock->SaveConfiguration();
        }
        // Disable saves during collection switch to prevent race conditions
        dock->m_initialLoadComplete = false;
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->LoadConfiguration();
            dock->m_model->loadSceneTree();
            dock->refreshSceneList();
            dock->applyAllCustomColors();

            // Restore folder expansion state after tree is fully loaded
            // and re-enable saves after load completes
            QTimer::singleShot(500, dock, [dock]() {
                StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
                if (settings.sceneOrganiserRememberFolderState) {
                    dock->restoreFolderExpansionState();
                }
                // Re-enable saves after collection load completes
                dock->m_initialLoadComplete = true;
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "CollectionChange",
                    "Scene collection load complete - saves now enabled");
            });
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->m_model->saveSceneTree();
            dock->refreshSceneList();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
        if (!dock->m_initialLoadComplete)
            break;
        QTimer::singleShot(50, dock, [dock]() {
            if (!dock->m_initialLoadComplete)
                return;
            dock->updateActiveSceneHighlight();
        });
        break;
    case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
        // Update highlighting when preview scene changes in studio mode
        if (!dock->m_initialLoadComplete)
            break;
        QTimer::singleShot(50, dock, [dock]() {
            if (!dock->m_initialLoadComplete)
                return;
            dock->updateActiveSceneHighlight();
        });
        break;
    case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
    case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
        // Update highlighting when studio mode is toggled
        QTimer::singleShot(50, dock, [dock]() {
            dock->updateActiveSceneHighlight();
        });
        break;
    case OBS_FRONTEND_EVENT_THEME_CHANGED:
        // Handle theme changes - clear caches and update icons
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Theme", "Theme changed event received");
        QTimer::singleShot(50, dock, [dock]() {
            dock->onThemeChanged();
        });
        break;
    case OBS_FRONTEND_EVENT_EXIT:
        // Save configuration before OBS exits
        if (dock->m_initialLoadComplete) {
            dock->m_model->saveSceneTree();
            dock->SaveConfiguration();
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Exit",
                "Saved scene tree on OBS exit");
        }
        break;
    default:
        break;
    }
}

void SceneOrganiserDock::SaveConfiguration()
{
    // Prevent saving before initial load completes to avoid overwriting valid data
    if (!m_initialLoadComplete) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Save",
            "Skipping save - initial load not yet complete");
        return;
    }

    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Ensure the directory exists
    QDir dir;
    if (!dir.mkpath(configDir)) {
        StreamUP::DebugLogger::LogInfo("SceneOrganiser",
            QString("Failed to create config directory: %1").arg(configDir).toUtf8().constData());
        return;
    }

    // Save lock state per scene collection
    QString lockStateFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_lock_state.txt";
    QFile file(lockStateFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << (m_isLocked ? "locked" : "unlocked");
        file.close();
    }

    // Save hidden scenes per scene collection
    QString hiddenScenesFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_hidden_scenes.txt";
    QFile hiddenFile(hiddenScenesFile);
    if (hiddenFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&hiddenFile);
        QStringList hiddenScenesList = QStringList(m_hiddenScenes.begin(), m_hiddenScenes.end());
        out << hiddenScenesList.join("\n");
        hiddenFile.close();
    }

    // Now using DigitOtter approach - save is handled by saveSceneTree()
    // which is called automatically on OBS frontend events
    m_model->saveSceneTree();

    // Save folder expansion state if setting is enabled
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (settings.sceneOrganiserRememberFolderState) {
        saveFolderExpansionState();
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        QString("Configuration saved for scene collection '%1' (lock state: %2)")
        .arg(sceneCollectionName)
        .arg(m_isLocked ? "locked" : "unlocked").toUtf8().constData());
}

void SceneOrganiserDock::LoadConfiguration()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Load lock state per scene collection
    QString lockStateFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_lock_state.txt";
    QFile file(lockStateFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QString lockState = in.readAll().trimmed();
        bool shouldBeLocked = (lockState == "locked");
        setLocked(shouldBeLocked);
        file.close();
    } else {
        // Default to unlocked if no saved state
        setLocked(false);
    }

    // Load hidden scenes per scene collection
    QString hiddenScenesFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_hidden_scenes.txt";
    QFile hiddenFile(hiddenScenesFile);
    if (hiddenFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&hiddenFile);
        QString hiddenScenesText = in.readAll().trimmed();
        if (!hiddenScenesText.isEmpty()) {
            QStringList hiddenScenesList = hiddenScenesText.split("\n", Qt::SkipEmptyParts);
            m_hiddenScenes = QSet<QString>(hiddenScenesList.begin(), hiddenScenesList.end());

            // Apply scene visibility after loading hidden scenes
            // Use longer delay to ensure scene tree is fully populated
            QTimer::singleShot(500, this, [this]() {
                updateHiddenScenesStyling();
                applySceneVisibility();
            });
        }
        hiddenFile.close();
    } else {
        // Clear hidden scenes if no saved state for this collection
        m_hiddenScenes.clear();
    }

    // Check if we need to prompt for migration
    // Check if migration is available even if lock file exists, to handle cases where
    // user previously used the plugin but hasn't migrated from obs_scene_tree_view yet
    // Check persistent migration history file to avoid re-prompting
    QString migrationHistoryFile = configDir + "/migration_prompted_collections.txt";
    QFile historyFile(migrationHistoryFile);
    QSet<QString> promptedCollections;

    // Load previously prompted collections from file
    if (historyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&historyFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                promptedCollections.insert(line);
            }
        }
        historyFile.close();
    }

    // Static set to track which collections are currently being prompted (to prevent race condition)
    // This prevents multiple dock instances from showing the import prompt simultaneously
    static QSet<QString> currentlyPrompting;

    // Check for migration availability if we haven't prompted yet for this collection
    // Also check if we're not already prompting for this collection (prevents duplicate prompts)
    if (m_model && !promptedCollections.contains(sceneCollectionName) && !currentlyPrompting.contains(sceneCollectionName)) {
        QString migrationConfigPath;
        if (SceneTreeModel::checkMigrationAvailable(sceneCollectionName, migrationConfigPath)) {
            // Mark this collection as currently being prompted (prevents other dock instances from showing the prompt)
            currentlyPrompting.insert(sceneCollectionName);

            // Mark this collection as prompted by saving to file immediately
            promptedCollections.insert(sceneCollectionName);
            if (historyFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&historyFile);
                for (const QString &collection : promptedCollections) {
                    out << collection << "\n";
                }
                historyFile.close();
            }

            // Use QTimer to ensure prompt appears after UI is fully loaded
            QTimer::singleShot(500, this, [this, sceneCollectionName]() {
                QMessageBox msgBox(QMessageBox::Question,
                    "Import Scene Organization?",
                    QString("Settings from the SceneTree plugin were found for '%1'.\n\n"
                            "Would you like to import your scene organization?\n\n"
                            "You can access the Scene Organizer from View → Docks → Scene Organizer.").arg(sceneCollectionName),
                    QMessageBox::Yes | QMessageBox::No,
                    this);

                msgBox.setDefaultButton(QMessageBox::Yes);

                if (msgBox.exec() == QMessageBox::Yes) {
                    if (m_model->migrateCurrentCollection()) {
                        // Reload configuration and tree for ALL dock instances
                        for (auto dock : SceneOrganiserDock::s_dockInstances) {
                            if (dock && dock->m_model) {
                                dock->LoadConfiguration();
                                dock->m_model->loadSceneTree();
                                dock->m_model->updateTree();
                                dock->applyAllCustomColors();
                            }
                        }

                        QMessageBox::information(this,
                            "Import Successful",
                            QString("Successfully imported scene organization for '%1'!\n\n"
                                    "The Scene Organizer can be accessed from View → Docks → Scene Organizer.").arg(sceneCollectionName));
                    } else {
                        QMessageBox::warning(this,
                            "Import Failed",
                            "Failed to import settings. Please check the log for details.");
                    }
                }

                // Remove from currently prompting set after dialog is dismissed
                currentlyPrompting.remove(sceneCollectionName);
            });
        }
    }

    // Now using the DigitOtter approach - this is handled by frontend events
    // No need to manually load configuration here
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        QString("Configuration loaded for scene collection '%1' (lock state: %2)")
        .arg(sceneCollectionName)
        .arg(m_isLocked ? "locked" : "unlocked").toUtf8().constData());

    // Apply scene visibility after full initialization
    // Note: Folder expansion state is restored in the frontend event handlers
    // (FINISHED_LOADING and SCENE_COLLECTION_CHANGED) after the tree is fully refreshed
    QTimer::singleShot(1000, this, [this]() {
        updateHiddenScenesStyling();
        applySceneVisibility();
    });
}

// Search functionality implementation
void SceneOrganiserDock::onSearchTextChanged(const QString &text)
{
    if (!m_proxyModel) return;

    // Check if we have search text for expansion logic
    bool hasText = !text.isEmpty();
    bool hadText = !m_proxyModel->filterRegularExpression().pattern().isEmpty();

    // Save expansion state when starting a search
    if (hasText && !hadText) {
        saveExpansionState();
    }

    // Apply filter to proxy model
    m_proxyModel->setFilterWildcard(text);

    // Expand all items when searching to show matches
    if (hasText) {
        m_treeView->expandAll();
    } else {
        // Restore previous expansion state when search is cleared
        restoreExpansionState();
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search",
        QString("Search filter applied: '%1'").arg(text).toUtf8().constData());
}

void SceneOrganiserDock::onClearSearch()
{
    if (!m_searchEdit) return;

    m_searchEdit->clear();
    m_searchEdit->clearFocus();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search", "Search cleared");
}

void SceneOrganiserDock::saveExpansionState()
{
    if (!m_treeView || !m_proxyModel) return;

    m_savedExpansionState.clear();

    // Recursively save expansion state for all items
    std::function<void(const QModelIndex&)> saveRecursive = [&](const QModelIndex& proxyIndex) {
        if (!proxyIndex.isValid()) return;

        // Convert proxy index to source index for stable storage
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        if (!sourceIndex.isValid()) return;

        // Save expansion state using source model index
        bool isExpanded = m_treeView->isExpanded(proxyIndex);
        m_savedExpansionState[QPersistentModelIndex(sourceIndex)] = isExpanded;

        // Recursively save children
        int rowCount = m_proxyModel->rowCount(proxyIndex);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, proxyIndex);
            saveRecursive(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        saveRecursive(rootIndex);
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search",
        QString("Saved expansion state for %1 items").arg(m_savedExpansionState.size()).toUtf8().constData());
}

void SceneOrganiserDock::restoreExpansionState()
{
    if (!m_treeView || !m_proxyModel || m_savedExpansionState.isEmpty()) return;

    // Restore expansion state for all saved items
    for (auto it = m_savedExpansionState.begin(); it != m_savedExpansionState.end(); ++it) {
        QPersistentModelIndex sourceIndex = it.key();
        bool wasExpanded = it.value();

        if (sourceIndex.isValid()) {
            // Convert source index back to proxy index for tree view
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            if (proxyIndex.isValid()) {
                m_treeView->setExpanded(proxyIndex, wasExpanded);
            }
        }
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Search",
        QString("Restored expansion state for %1 items").arg(m_savedExpansionState.size()).toUtf8().constData());

    // Clear the saved state after restoring
    m_savedExpansionState.clear();
}

void SceneOrganiserDock::saveFolderExpansionState()
{
    if (!m_treeView || !m_model) return;

    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Collect expanded folder names
    QStringList expandedFolders;
    std::function<void(const QModelIndex&)> collectExpanded = [&](const QModelIndex& index) {
        if (!index.isValid()) return;

        // Map proxy index to source index
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item && item->type() == SceneFolderItem::UserType + 1) {
            if (m_treeView->isExpanded(index)) {
                expandedFolders.append(item->text());
            }
        }

        // Recursively check children
        int rowCount = m_proxyModel->rowCount(index);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, index);
            collectExpanded(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        collectExpanded(rootIndex);
    }

    // Save to file
    QString expansionFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_expansion_state.txt";
    QFile file(expansionFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << expandedFolders.join("\n");
        file.close();

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
            QString("Saved expansion state for %1 folders").arg(expandedFolders.size()).toUtf8().constData());
    }
}

void SceneOrganiserDock::restoreFolderExpansionState()
{
    if (!m_treeView || !m_model) return;

    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString sceneCollectionName = QString::fromUtf8(scene_collection);
    bfree(configPath);
    bfree(scene_collection);

    // Load from file
    QString expansionFile = configDir + "/" + m_configKey + "_" + sceneCollectionName + "_expansion_state.txt";
    QFile file(expansionFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return; // No saved state, nothing to restore
    }

    QTextStream in(&file);
    QString expandedFoldersText = in.readAll().trimmed();
    file.close();

    if (expandedFoldersText.isEmpty()) {
        return;
    }

    QStringList expandedFoldersList = expandedFoldersText.split("\n", Qt::SkipEmptyParts);
    QSet<QString> expandedFolders = QSet<QString>(expandedFoldersList.begin(), expandedFoldersList.end());

    // Restore expansion state
    std::function<void(const QModelIndex&)> restoreExpanded = [&](const QModelIndex& index) {
        if (!index.isValid()) return;

        // Map proxy index to source index
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);

        if (item && item->type() == SceneFolderItem::UserType + 1) {
            if (expandedFolders.contains(item->text())) {
                m_treeView->setExpanded(index, true);
            }
        }

        // Recursively restore children
        int rowCount = m_proxyModel->rowCount(index);
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex childIndex = m_proxyModel->index(i, 0, index);
            restoreExpanded(childIndex);
        }
    };

    // Start from root items
    int rootRowCount = m_proxyModel->rowCount();
    for (int i = 0; i < rootRowCount; ++i) {
        QModelIndex rootIndex = m_proxyModel->index(i, 0);
        restoreExpanded(rootIndex);
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        QString("Restored expansion state for %1 folders").arg(expandedFolders.size()).toUtf8().constData());

    // Update button state to reflect the restored expansion state
    updateExpandCollapseButtonState();
}

// Public methods for keyboard shortcuts
void SceneOrganiserDock::triggerRename()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (!selected.isEmpty()) {
        QModelIndex proxyIndex = selected.first();
        QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        QStandardItem *item = m_model->itemFromIndex(sourceIndex);
        if (item) {
            if (item->type() == SceneTreeItem::UserType + 2) {
                // Scene item - trigger scene rename
                onRenameSceneClicked();
            } else if (item->type() == SceneFolderItem::UserType + 1) {
                // Folder item - trigger folder rename
                onRenameFolderClicked();
            }
        }
    }
}

void SceneOrganiserDock::triggerRemove()
{
    onRemoveClicked();
}

// New OBS-compatible context menu actions

void SceneOrganiserDock::onDuplicateSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString currentSceneName = m_currentContextItem->text();
    obs_source_t *currentSource = obs_get_source_by_name(currentSceneName.toUtf8().constData());
    if (!currentSource) return;

    obs_scene_t *currentScene = obs_scene_from_source(currentSource);
    if (!currentScene) {
        obs_source_release(currentSource);
        return;
    }

    // Generate unique name for duplicated scene
    QString format = currentSceneName + " %1";
    int i = 2;
    QString newName = format.arg(i);
    obs_source_t *existing = nullptr;
    while ((existing = obs_get_source_by_name(newName.toUtf8().constData())) != nullptr) {
        obs_source_release(existing);
        newName = format.arg(++i);
    }

    // Duplicate the scene
    obs_scene_t *duplicatedScene = obs_scene_duplicate(currentScene, newName.toUtf8().constData(), OBS_SCENE_DUP_REFS);
    if (duplicatedScene) {
        obs_source_t *newSource = obs_scene_get_source(duplicatedScene);
        obs_frontend_set_current_scene(newSource);

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Duplication",
            QString("Duplicated scene '%1' to '%2'").arg(currentSceneName, newName).toUtf8().constData());
        obs_scene_release(duplicatedScene);
    }

    obs_source_release(currentSource);
}

void SceneOrganiserDock::onDeleteSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (!source) return;

    // Confirm deletion
    int ret = QMessageBox::question(this,
        QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title"), -1),
        QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text"), -1).arg(sceneName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        obs_source_remove(source);
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Deletion",
            QString("Deleted scene: %1").arg(sceneName).toUtf8().constData());
    }

    obs_source_release(source);
}

void SceneOrganiserDock::onHideSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();

    // Add scene to hidden list
    m_hiddenScenes.insert(sceneName);

    // Apply visual styling when unlocked (grayed-out color to indicate it will be hidden when locked)
    QColor grayColor = getDefaultThemeTextColor();
    grayColor.setAlpha(100); // Make it semi-transparent for a grayed-out effect
    m_currentContextItem->setForeground(QBrush(grayColor));

    // If dock is locked, immediately hide the scene from the tree
    if (m_isLocked) {
        applySceneVisibility();
    }

    // Save configuration immediately to persist hidden scenes
    m_saveTimer->start(500);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Visibility",
        QString("Hidden scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onShowSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();

    // Remove scene from hidden list
    m_hiddenScenes.remove(sceneName);

    // Reset to default theme text color
    m_currentContextItem->setForeground(QBrush(getDefaultThemeTextColor()));

    // If dock is locked, immediately show the scene in the tree
    if (m_isLocked) {
        applySceneVisibility();
    }

    // Save configuration immediately to persist visible scenes
    m_saveTimer->start(500);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Visibility",
        QString("Shown scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::applySceneVisibility()
{
    if (!m_model || !m_treeView) return;

    // Recursively apply visibility to all scene items in the tree
    applySceneVisibilityRecursive(m_model->invisibleRootItem());

    // Force update of the proxy model to refresh the view
    if (m_proxyModel) {
        m_proxyModel->invalidate();
    }
}

void SceneOrganiserDock::applySceneVisibilityRecursive(QStandardItem *parent)
{
    if (!parent) return;

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (!child) continue;

        if (child->type() == SceneTreeItem::UserType + 2) { // Scene item
            QString sceneName = child->text();
            bool shouldHide = m_isLocked && m_hiddenScenes.contains(sceneName);

            // Get the model index for this item
            QModelIndex index = child->index();
            if (m_treeView) {
                // Map to proxy model index
                QModelIndex proxyIndex = m_proxyModel->mapFromSource(index);
                // Hide/show the row in the tree view
                m_treeView->setRowHidden(proxyIndex.row(), proxyIndex.parent(), shouldHide);
            }
        } else if (child->hasChildren()) {
            // Recursively check folder contents
            applySceneVisibilityRecursive(child);
        }
    }
}

void SceneOrganiserDock::updateHiddenScenesStyling()
{
    if (!m_model) return;
    updateHiddenScenesStylingRecursive(m_model->invisibleRootItem());
}

void SceneOrganiserDock::updateHiddenScenesStylingRecursive(QStandardItem *parent)
{
    // DISABLED - No custom styling
    Q_UNUSED(parent);
}

void SceneOrganiserDock::onCopyFiltersClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (!source) return;

    // Release previous copy source if any
    if (m_copyFiltersSource) {
        obs_weak_source_release(m_copyFiltersSource);
    }

    // Store weak reference to source for filter copying
    m_copyFiltersSource = obs_source_get_weak_source(source);
    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Filter Copy",
        QString("Copied filters from scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onPasteFiltersClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;
    if (!m_copyFiltersSource || obs_weak_source_expired(m_copyFiltersSource)) return;

    QString targetSceneName = m_currentContextItem->text();
    obs_source_t *targetSource = obs_get_source_by_name(targetSceneName.toUtf8().constData());
    obs_source_t *sourceSource = obs_weak_source_get_source(m_copyFiltersSource);

    if (!targetSource || !sourceSource) {
        if (targetSource) obs_source_release(targetSource);
        if (sourceSource) obs_source_release(sourceSource);
        return;
    }

    // Copy all filters from source to target
    obs_source_copy_filters(targetSource, sourceSource);

    obs_source_release(targetSource);
    obs_source_release(sourceSource);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Filter Paste",
        QString("Pasted filters to scene: %1").arg(targetSceneName).toUtf8().constData());
}

void SceneOrganiserDock::onSceneFiltersClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (!source) return;

    obs_frontend_open_source_filters(source);
    obs_source_release(source);
}

void SceneOrganiserDock::onScreenshotSceneClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (!source) return;

    obs_frontend_take_source_screenshot(source);
    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Screenshot",
        QString("Taking screenshot of scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onShowInMultiviewClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (!source) return;

    obs_data_t *privateSettings = obs_source_get_private_settings(source);
    obs_data_set_default_bool(privateSettings, "show_in_multiview", true);
    bool showInMultiview = obs_data_get_bool(privateSettings, "show_in_multiview");
    obs_data_set_bool(privateSettings, "show_in_multiview", !showInMultiview);
    obs_data_release(privateSettings);

    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Multiview Toggle",
        QString("Toggled multiview for scene: %1 (now %2)").arg(sceneName, !showInMultiview ? "visible" : "hidden").toUtf8().constData());
}

void SceneOrganiserDock::onOpenProjectorClicked()
{
    // This would be called by monitor-specific projector actions
    // Implementation would involve creating projectors for specific monitors
}

void SceneOrganiserDock::onOpenProjectorOnMonitorClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) return;

    int monitorIndex = action->property("monitor").toInt();
    QString sceneName = m_currentContextItem->text();

    obs_frontend_open_projector("Scene", monitorIndex, nullptr, sceneName.toUtf8().constData());

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Projector Monitor",
        QString("Opened projector for scene '%1' on monitor %2").arg(sceneName).arg(monitorIndex).toUtf8().constData());
}

void SceneOrganiserDock::onOpenProjectorWindowClicked()
{
    if (!m_currentContextItem || m_currentContextItem->type() != SceneTreeItem::UserType + 2) return;

    QString sceneName = m_currentContextItem->text();
    obs_source_t *source = obs_get_source_by_name(sceneName.toUtf8().constData());
    if (!source) return;

    obs_frontend_open_projector("Scene", -1, nullptr, obs_source_get_name(source));
    obs_source_release(source);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Projector Window",
        QString("Opened projector window for scene: %1").arg(sceneName).toUtf8().constData());
}

void SceneOrganiserDock::onSceneMoveUpClicked()
{
    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    if (currentRow > 0) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(currentRow - 1, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }
}

void SceneOrganiserDock::onSceneMoveDownClicked()
{
    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    if (currentRow < parent->rowCount() - 1) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(currentRow + 1, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }
}

void SceneOrganiserDock::onSceneMoveToTopClicked()
{
    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    if (currentRow > 0) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(0, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }
}

void SceneOrganiserDock::onSceneMoveToBottomClicked()
{
    if (!m_currentContextItem) return;

    QStandardItem *parent = m_currentContextItem->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = m_currentContextItem->row();
    int maxRow = parent->rowCount() - 1;
    if (currentRow < maxRow) {
        // Take the entire row (this includes all columns)
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        if (!items.isEmpty()) {
            parent->insertRow(maxRow, items);
            QModelIndex sourceIndex = items[0]->index();
            QModelIndex proxyIndex = m_proxyModel->mapFromSource(sourceIndex);
            m_treeView->setCurrentIndex(proxyIndex);
            m_saveTimer->start();
        }
    }
}

void SceneOrganiserDock::populateProjectorMenu()
{
    if (!m_sceneProjectorMenu) return;

    // Clear existing monitor actions (but keep the window projector action)
    QList<QAction*> actions = m_sceneProjectorMenu->actions();
    for (QAction *action : actions) {
        if (action->property("monitor").isValid()) {
            m_sceneProjectorMenu->removeAction(action);
            action->deleteLater();
        }
    }

    // Add monitor actions at the beginning
    QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens[i];
        QString monitorText;

        if (screens.size() > 1) {
            // Multiple monitors - show monitor number and name
            monitorText = QString("Monitor %1").arg(i + 1);
            if (!screen->name().isEmpty()) {
                monitorText += QString(" (%1)").arg(screen->name());
            }
        } else {
            // Single monitor - just show "Fullscreen"
            monitorText = "Fullscreen";
        }

        QAction *monitorAction = new QAction(monitorText, m_sceneProjectorMenu);
        monitorAction->setProperty("monitor", i);
        connect(monitorAction, &QAction::triggered, this, &SceneOrganiserDock::onOpenProjectorOnMonitorClicked);

        // Insert before the separator (which should be before "Windowed")
        QList<QAction*> currentActions = m_sceneProjectorMenu->actions();
        if (!currentActions.isEmpty() && currentActions.first()->isSeparator()) {
            m_sceneProjectorMenu->insertAction(currentActions.first(), monitorAction);
        } else {
            // If no separator found, add at the beginning
            if (!currentActions.isEmpty()) {
                m_sceneProjectorMenu->insertAction(currentActions.first(), monitorAction);
            } else {
                m_sceneProjectorMenu->addAction(monitorAction);
            }
        }
    }
}

//==============================================================================
// SceneTreeModel Implementation
//==============================================================================

SceneTreeModel::SceneTreeModel(CanvasType canvasType, QObject *parent)
    : QStandardItemModel(parent)
    , m_canvasType(canvasType)
{
    setupRootItem();
    // Don't call refreshFromObs() here - let the dock load configuration first
}

SceneTreeModel::~SceneTreeModel()
{
    // Clean up all weak source references
    cleanupSceneTree();
}

void SceneTreeModel::setupRootItem()
{
    setHorizontalHeaderLabels(QStringList() << "Scenes");
}

Qt::DropActions SceneTreeModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

Qt::ItemFlags SceneTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;

    // Allow editing for scenes and folders
    QStandardItem *item = itemFromIndex(index);
    if (item && (item->type() == SceneTreeItem::UserType + 2 || item->type() == SceneFolderItem::UserType + 1)) {
        flags |= Qt::ItemIsEditable;
    }

    return flags;
}

bool SceneTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid())
        return false;

    QStandardItem *item = itemFromIndex(index);
    if (!item)
        return false;

    QString newName = value.toString().trimmed();
    QString oldName = item->text();

    // Validate the new name
    if (newName.isEmpty() || newName == oldName)
        return false;

    if (item->type() == SceneTreeItem::UserType + 2) {
        // Scene item - rename in OBS
        obs_source_t *source = obs_get_source_by_name(oldName.toUtf8().constData());
        if (source) {
            obs_source_set_name(source, newName.toUtf8().constData());
            obs_source_release(source);

            // Update the item text
            item->setText(newName);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Inline Rename",
                QString("Renamed scene from '%1' to '%2'").arg(oldName, newName).toUtf8().constData());

            emit modelChanged();

            // Immediately save after rename to ensure changes persist
            saveSceneTree();

            return true;
        }
    } else if (item->type() == SceneFolderItem::UserType + 1) {
        // Folder item - simple rename
        item->setText(newName);

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Inline Rename",
            QString("Renamed folder from '%1' to '%2'").arg(oldName, newName).toUtf8().constData());

        emit modelChanged();

        // Immediately save after rename to ensure changes persist
        saveSceneTree();

        return true;
    }

    return false;
}

QStringList SceneTreeModel::mimeTypes() const
{
    return QStringList() << "application/x-streamup-sceneorganiser";
}

QMimeData *SceneTreeModel::mimeData(const QModelIndexList &indexes) const
{
    if (indexes.isEmpty())
        return nullptr;

    QMimeData *mimeData = new QMimeData();

    // Use binary data format like DigitOtter plugin
    QByteArray mimeDataBytes;
    const int numIndexes = indexes.size();

    mimeDataBytes.append(reinterpret_cast<const char*>(&numIndexes), sizeof(int));

    for (const QModelIndex &index : indexes) {
        if (index.isValid()) {
            QStandardItem *item = itemFromIndex(index);
            if (item) {
                // Store item pointer directly
                mimeDataBytes.append(reinterpret_cast<const char*>(&item), sizeof(QStandardItem*));
            }
        }
    }

    mimeData->setData("application/x-streamup-sceneorganiser", mimeDataBytes);
    return mimeData;
}

bool SceneTreeModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
{
    Q_UNUSED(action)
    Q_UNUSED(column)

    if (!data->hasFormat("application/x-streamup-sceneorganiser"))
        return false;

    // Get parent item - use root if none specified
    QStandardItem *parentItem = itemFromIndex(parent);
    if (!parentItem) {
        parentItem = invisibleRootItem();
    }

    // Prevent dropping scenes into scenes (DigitOtter approach)
    if (parentItem->type() == SceneTreeItem::UserType + 2) {
        // Dropping on a scene - place it beside the scene instead
        QStandardItem *sceneParent = parentItem->parent();
        if (!sceneParent) {
            sceneParent = invisibleRootItem();
        }
        row = parentItem->row() + 1;
        parentItem = sceneParent;
    }

    // Ensure valid row
    if (row < 0) {
        row = parentItem->rowCount();
    }

    QByteArray mimeDataBytes = data->data("application/x-streamup-sceneorganiser");
    if (mimeDataBytes.size() < static_cast<int>(sizeof(int))) {
        return false;
    }

    const char *dat = mimeDataBytes.constData();
    const int numIndexes = *reinterpret_cast<const int*>(dat);
    dat += sizeof(int);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
        QString("Dropping %1 items at row %2 into '%3'")
        .arg(numIndexes).arg(row)
        .arg(parentItem == invisibleRootItem() ? "root" : parentItem->text()).toUtf8().constData());

    for (int i = 0; i < numIndexes; ++i) {
        if (dat + sizeof(QStandardItem*) > mimeDataBytes.constData() + mimeDataBytes.size()) {
            break; // Safety check
        }

        QStandardItem *originalItem = *reinterpret_cast<QStandardItem* const*>(dat);
        dat += sizeof(QStandardItem*);

        if (!originalItem) {
            continue;
        }

        if (originalItem->type() == SceneTreeItem::UserType + 2) {
            // Move scene item - create new item and update tracking
            SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(originalItem);
            obs_weak_source_t *weak_source = sceneItem->getWeakSource();

            // Add reference for the new item (since the old item will release its reference when destroyed)
            obs_weak_source_addref(weak_source);
            QStandardItem *newItem = new SceneTreeItem(originalItem->text(), weak_source);

            // Copy custom color from original item
            QVariant customColor = originalItem->data(Qt::UserRole + 1);
            if (customColor.isValid()) {
                newItem->setData(customColor, Qt::UserRole + 1);
                // Apply the color visually
                QColor color = customColor.value<QColor>();
                if (color.isValid()) {
                    // Get the dock instance to use its color helper methods
                    for (auto dock : SceneOrganiserDock::s_dockInstances) {
                        if (dock && dock->m_model == this) {
                            dock->applyCustomColorToItem(newItem, color);
                            break;
                        }
                    }
                }
            }

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                QString("Inserting scene '%1' at row %2 (parent has %3 children)")
                .arg(newItem->text()).arg(row).arg(parentItem->rowCount()).toUtf8().constData());

            parentItem->insertRow(row, newItem);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                QString("After insert: scene '%1' is now at row %2 (parent has %3 children)")
                .arg(newItem->text()).arg(newItem->row()).arg(parentItem->rowCount()).toUtf8().constData());

            // Update tracking map with new item
            m_scenesInTree[weak_source] = newItem;

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                QString("Updated tracking for scene '%1' - new item at %2")
                .arg(newItem->text()).arg(reinterpret_cast<uintptr_t>(newItem)).toUtf8().constData());

            // Check if this is the active scene and mark it for immediate update
            obs_source_t *current_scene = obs_frontend_get_current_scene();
            if (current_scene) {
                QString current_scene_name = QString::fromUtf8(obs_source_get_name(current_scene));
                if (newItem->text() == current_scene_name) {
                    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "DragDrop",
                        QString("Moved scene '%1' is the active scene - forcing immediate repaint")
                        .arg(newItem->text()).toUtf8().constData());

                    // Force immediate repaint of this specific item
                    QModelIndex newIndex = indexFromItem(newItem);
                    if (newIndex.isValid()) {
                        emit dataChanged(newIndex, newIndex);
                    }
                }
                obs_source_release(current_scene);
            }
        } else if (originalItem->type() == SceneFolderItem::UserType + 1) {
            // Move folder item
            moveSceneFolder(originalItem, row, parentItem);
        }

        ++row; // Increment for next item
    }

    emit modelChanged();

    // Immediately save after drag & drop to ensure changes persist
    saveSceneTree();

    return true;
}

void SceneTreeModel::updateTree(const QModelIndex &selectedIndex)
{
    // Get all scenes from OBS
    struct obs_frontend_source_list scene_list = {};
    obs_frontend_get_scenes(&scene_list);

    source_map_t new_scene_tree;

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
        QString("Processing %1 scenes from OBS").arg(scene_list.sources.num).toUtf8().constData());

    for (size_t i = 0; i < scene_list.sources.num; i++) {
        obs_source_t *source = scene_list.sources.array[i];
        if (!source) continue;

        obs_scene_t *scene = obs_scene_from_source(source);
        if (!scene) continue;

        if (!isManagedScene(source)) continue;

        obs_weak_source_t *weak = obs_source_get_weak_source(source);
        source_map_t::iterator scene_it;

        // Check if scene already in tree by comparing actual sources, not weak pointers
        // Different weak references can point to the same source
        scene_it = m_scenesInTree.end();
        for (auto it = m_scenesInTree.begin(); it != m_scenesInTree.end(); ++it) {
            obs_source_t *existing_source = obs_weak_source_get_source(it->first);
            bool same_source = (existing_source == source);
            obs_source_release(existing_source);

            if (same_source) {
                scene_it = it;
                break;
            }
        }

        if (scene_it != m_scenesInTree.end()) {
            // Move existing scene to new tree
            auto new_scene_it = new_scene_tree.emplace(scene_it->first, scene_it->second).first;
            m_scenesInTree.erase(scene_it);
            scene_it = new_scene_it;
            obs_weak_source_release(weak); // Release the duplicate weak reference
        } else {
            // Add new scene
            scene_it = new_scene_tree.emplace(weak, nullptr).first;
        }

        if (!scene_it->second) {
            // Scene not yet in tree, add it
            const char *name = obs_source_get_name(source);
            if (name) {
                QString sceneName = QString::fromUtf8(name);
                QStandardItem *newSceneItem = new SceneTreeItem(sceneName, scene_it->first);

                // Determine where to add the scene
                QStandardItem *parent = invisibleRootItem();
                QStandardItem *selected = itemFromIndex(selectedIndex);
                if (selected) {
                    if (selected->type() == SceneFolderItem::UserType + 1) {
                        parent = selected;
                    } else if (selected->parent()) {
                        parent = selected->parent();
                    }
                }

                parent->appendRow(newSceneItem);
                scene_it->second = newSceneItem;

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
                    QString("Added new scene: %1").arg(sceneName).toUtf8().constData());
            }
        } else {
            // Update existing scene name
            const char *name = obs_source_get_name(source);
            if (name) {
                scene_it->second->setText(QString::fromUtf8(name));
            }
        }
    }

    // Remove scenes that are no longer in OBS
    for (auto &old_scene : m_scenesInTree) {
        if (old_scene.second) {
            QStandardItem *parent = old_scene.second->parent();
            if (!parent) parent = invisibleRootItem();

            QString sceneName = old_scene.second->text();
            int row = old_scene.second->row();
            parent->removeRow(row);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
                QString("Removed scene: %1").arg(sceneName).toUtf8().constData());
        }
        // Release the weak source reference
        obs_weak_source_release(old_scene.first);
    }

    // Update our scene tree
    m_scenesInTree = std::move(new_scene_tree);

    obs_frontend_source_list_free(&scene_list);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "UpdateTree",
        QString("Tree updated - %1 scenes tracked").arg(m_scenesInTree.size()).toUtf8().constData());

    emit modelChanged();
}

bool SceneTreeModel::isValidSceneForCanvas(obs_scene_t *scene)
{
    if (!scene) return false;

    obs_source_t *source = obs_scene_get_source(scene);
    if (!source) return false;

    const char *scene_name = obs_source_get_name(source);
    if (!scene_name) return false;

    // All scenes are valid for normal canvas
    return true;
}


QStandardItem *SceneTreeModel::findSceneItem(obs_weak_source_t *weak_source)
{
    auto it = m_scenesInTree.find(weak_source);
    return (it != m_scenesInTree.end()) ? it->second : nullptr;
}

QStandardItem *SceneTreeModel::findFolderItem(const QString &folderName)
{
    return StreamUP::UIHelpers::FindItemRecursive(invisibleRootItem(), folderName, SceneFolderItem::UserType + 1);
}

QStandardItem *SceneTreeModel::createFolderItem(const QString &folderName)
{
    if (folderName.isEmpty() || folderName.trimmed().isEmpty()) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Warning",
            "Attempted to create folder item with empty name");
        return nullptr;
    }
    return new SceneFolderItem(folderName);
}

QStandardItem *SceneTreeModel::createSceneItem(const QString &sceneName, obs_weak_source_t *weak_source)
{
    if (sceneName.isEmpty() || sceneName.trimmed().isEmpty()) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Warning",
            "Attempted to create scene item with empty name");
        return nullptr;
    }
    return new SceneTreeItem(sceneName, weak_source);
}

void SceneTreeModel::moveSceneToFolder(obs_weak_source_t *weak_source, QStandardItem *folderItem)
{
    QStandardItem *sceneItem = findSceneItem(weak_source);
    if (sceneItem && folderItem) {
        QStandardItem *oldParent = sceneItem->parent();
        if (!oldParent) oldParent = invisibleRootItem();

        QStandardItem *movedItem = oldParent->takeChild(sceneItem->row());
        if (movedItem) {
            folderItem->appendRow(movedItem);
            emit modelChanged();
        }
    }
}

void SceneTreeModel::cleanupEmptyItems()
{
    cleanupEmptyItemsRecursive(invisibleRootItem());
}

void SceneTreeModel::cleanupEmptyItemsRecursive(QStandardItem *parent)
{
    if (!parent) return;

    // Iterate backwards to avoid index issues when removing items
    for (int i = parent->rowCount() - 1; i >= 0; --i) {
        QStandardItem *child = parent->child(i);
        if (!child) {
            // Remove null items
            parent->removeRow(i);
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup", "Removed null item");
            continue;
        }

        // Check if item has empty or whitespace-only text
        QString itemText = child->text();
        if (itemText.isEmpty() || itemText.trimmed().isEmpty()) {
            parent->removeRow(i);
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
                QString("Removed empty item at row %1").arg(i).toUtf8().constData());
            continue;
        }

        // For scene items, they are managed by updateTree now
        if (child->type() == SceneTreeItem::UserType + 2) {
            // Scene validation is handled in updateTree - skip here
            continue;
        }

        // Recursively clean up folders
        if (child->type() == SceneFolderItem::UserType + 1) {
            cleanupEmptyItemsRecursive(child);

            // After cleaning up children, check if folder is now empty and remove it if so
            if (child->rowCount() == 0) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
                    QString("Folder '%1' is empty after cleanup - considering for removal").arg(itemText).toUtf8().constData());
                // Note: We don't auto-remove empty folders as user might want to keep them
            }
        } else if (child->type() != SceneTreeItem::UserType + 2) {
            // Handle items with unknown/invalid types
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
                QString("Found item '%1' with unknown type %2, removing").arg(itemText).arg(child->type()).toUtf8().constData());
            parent->removeRow(i);
        }
    }
}

bool SceneTreeModel::isChildOf(QStandardItem *potentialChild, QStandardItem *potentialParent)
{
    if (!potentialChild || !potentialParent) {
        return false;
    }

    // Check if potentialParent is an ancestor of potentialChild
    QStandardItem *current = potentialChild->parent();
    while (current) {
        if (current == potentialParent) {
            return true;
        }
        current = current->parent();
    }
    return false;
}

void SceneTreeModel::moveSceneItem(QStandardItem *item, int row, QStandardItem *parentItem)
{
    // This function is now only used for internal moves, not drag & drop
    // Drag & drop is handled directly in dropMimeData
    if (!item || item->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
    QString sceneName = item->text();
    obs_weak_source_t *weak = sceneItem->getWeakSource();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moving scene '%1'").arg(sceneName).toUtf8().constData());

    // For internal moves, we can safely move the item directly
    QStandardItem *oldParent = item->parent();
    if (!oldParent) oldParent = invisibleRootItem();

    QStandardItem *takenItem = oldParent->takeChild(item->row());
    if (takenItem) {
        parentItem->insertRow(row, takenItem);
        m_scenesInTree[weak] = takenItem;

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
            QString("Moved scene '%1' to row %2").arg(sceneName).arg(row).toUtf8().constData());
    }
}

void SceneTreeModel::moveSceneFolder(QStandardItem *item, int row, QStandardItem *parentItem)
{
    if (!item || item->type() != SceneFolderItem::UserType + 1) {
        return;
    }

    QString folderName = item->text();
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moving folder '%1'").arg(folderName).toUtf8().constData());

    // Check if we're moving to the same parent - if so, keep original name
    QString uniqueName;
    QStandardItem *originalParent = item->parent();
    if (!originalParent) originalParent = invisibleRootItem();

    if (originalParent == parentItem) {
        // Moving within same parent - keep original name
        uniqueName = folderName;
    } else {
        // Moving to different parent - check for conflicts
        uniqueName = createUniqueFolderName(folderName, parentItem);
    }

    // Create new folder item
    QStandardItem *newFolder = createFolderItem(uniqueName);
    if (!newFolder) {
        return;
    }

    // Copy custom color from original folder
    QVariant customColor = item->data(Qt::UserRole + 1);
    if (customColor.isValid()) {
        newFolder->setData(customColor, Qt::UserRole + 1);
        // Apply the color visually
        QColor color = customColor.value<QColor>();
        if (color.isValid()) {
            // Get the dock instance to use its color helper methods
            for (auto dock : SceneOrganiserDock::s_dockInstances) {
                if (dock && dock->m_model == this) {
                    dock->applyCustomColorToItem(newFolder, color);
                    break;
                }
            }
        }
    }

    parentItem->insertRow(row, newFolder);

    // Recursively move child items
    for (int childRow = 0; childRow < item->rowCount(); ++childRow) {
        QStandardItem *childItem = item->child(childRow);
        if (!childItem) continue;

        if (childItem->type() == SceneFolderItem::UserType + 1) {
            moveSceneFolder(childItem, childRow, newFolder);
        } else if (childItem->type() == SceneTreeItem::UserType + 2) {
            moveSceneItem(childItem, childRow, newFolder);
        }
    }

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moved folder '%1' to row %2 with %3 children")
        .arg(uniqueName).arg(row).arg(item->rowCount()).toUtf8().constData());
}

QString SceneTreeModel::createUniqueFolderName(const QString &baseName, QStandardItem *parentItem)
{
    QString uniqueName = baseName;
    int suffix = 1;

    // Check if name already exists in parent
    while (true) {
        bool nameExists = false;
        for (int i = 0; i < parentItem->rowCount(); ++i) {
            QStandardItem *child = parentItem->child(i);
            if (child && child->type() == SceneFolderItem::UserType + 1 &&
                child->text() == uniqueName) {
                nameExists = true;
                break;
            }
        }

        if (!nameExists) {
            break;
        }

        uniqueName = QString("%1 (%2)").arg(baseName).arg(suffix++);
    }

    return uniqueName;
}

bool SceneTreeModel::isManagedScene(obs_source_t *source)
{
    if (!source) return false;

    obs_scene_t *scene = obs_scene_from_source(source);
    if (!scene) return false;

    return isValidSceneForCanvas(scene);
}

void SceneTreeModel::cleanupSceneTree()
{
    // Clear the model first - this will trigger SceneTreeItem destructors
    // which will release their own weak source references
    clear();
    setupRootItem();

    // Clear the tracking map (weak sources already released by item destructors)
    m_scenesInTree.clear();
}

void SceneTreeModel::saveSceneTree()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/scene_tree_normal.json";
    QString sceneCollectionName = QString::fromUtf8(scene_collection);

    bfree(configPath);

    // Load existing data first to preserve other scene collections
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (!root_data) {
        // File doesn't exist yet, create new data
        root_data = obs_data_create();
    }

    // Update only the current scene collection's data
    obs_data_array_t *folder_array = createFolderArray(*invisibleRootItem());
    obs_data_set_array(root_data, scene_collection, folder_array);
    obs_data_array_release(folder_array);

    // Save to file (now preserves other collections)
    obs_data_save_json(root_data, configFile.toUtf8().constData());
    obs_data_release(root_data);

    bfree(scene_collection);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Save",
        QString("Saved scene tree for collection '%1' to: %2").arg(sceneCollectionName, configFile).toUtf8().constData());
}

void SceneTreeModel::loadSceneTree()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) return;

    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        bfree(scene_collection);
        return;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/scene_tree_normal.json";
    QString sceneCollectionName = QString::fromUtf8(scene_collection);

    bfree(configPath);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
        QString("Loading scene tree for collection: '%1'").arg(sceneCollectionName).toUtf8().constData());

    // Clean up previous tree
    cleanupSceneTree();

    // Load from file
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (root_data) {
        obs_data_array_t *folder_array = obs_data_get_array(root_data, scene_collection);
        if (folder_array) {
            size_t itemCount = obs_data_array_count(folder_array);
            loadFolderArray(folder_array, *invisibleRootItem());
            obs_data_array_release(folder_array);

            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
                QString("Loaded %1 items from scene tree for collection '%2' from: %3")
                .arg(itemCount).arg(sceneCollectionName, configFile).toUtf8().constData());
        } else {
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
                QString("No saved scene tree found for collection '%1' (key not in JSON)")
                .arg(sceneCollectionName).toUtf8().constData());
        }
        obs_data_release(root_data);
    } else {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
            QString("Config file does not exist or is invalid: %1").arg(configFile).toUtf8().constData());
    }

    bfree(scene_collection);
}

bool SceneTreeModel::migrateFromOriginalPlugin(const QString &originalConfigPath)
{
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
        QString("Starting migration from: %1").arg(originalConfigPath).toUtf8().constData());

    // Load the original plugin's config file
    obs_data_t *original_data = obs_data_create_from_json_file(originalConfigPath.toUtf8().constData());
    if (!original_data) {
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
            "Failed to load original config file");
        return false;
    }

    // Get our config path
    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) {
        obs_data_release(original_data);
        return false;
    }

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/scene_tree_normal.json";
    bfree(configPath);

    // Load existing data (if any) to preserve what we have
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (!root_data) {
        root_data = obs_data_create();
    }

    int migratedCollections = 0;

    // Iterate through all scene collections in the original file
    obs_data_item_t *item = obs_data_first(original_data);
    while (item) {
        const char *collection_name = obs_data_item_get_name(item);
        obs_data_type type = obs_data_item_gettype(item);

        if (type == OBS_DATA_ARRAY) {
            obs_data_array_t *original_array = obs_data_item_get_array(item);
            if (original_array) {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
                    QString("Migrating collection: %1").arg(collection_name).toUtf8().constData());

                // Check if we already have data for this collection
                obs_data_array_t *existing_array = obs_data_get_array(root_data, collection_name);
                if (existing_array) {
                    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
                        QString("Collection '%1' already exists, skipping").arg(collection_name).toUtf8().constData());
                    obs_data_array_release(existing_array);
                } else {
                    // Convert and migrate this collection
                    obs_data_set_array(root_data, collection_name, original_array);
                    migratedCollections++;
                }

                obs_data_array_release(original_array);
            }
        }

        obs_data_item_next(&item);
    }

    // Save the migrated data
    obs_data_save_json(root_data, configFile.toUtf8().constData());
    obs_data_release(root_data);
    obs_data_release(original_data);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Migration",
        QString("Migration complete. Migrated %1 scene collections.").arg(migratedCollections).toUtf8().constData());

    return migratedCollections > 0;
}

bool SceneTreeModel::checkMigrationAvailable(const QString &sceneCollectionName, QString &outConfigPath)
{
    // Get the streamup plugin config path to navigate to obs_scene_tree_view
    char *streamup_config_path = obs_module_get_config_path(obs_current_module(), "");
    if (!streamup_config_path) {
        return false;
    }

    QDir streamupConfigDir(QString::fromUtf8(streamup_config_path));
    streamupConfigDir.cdUp(); // Go to plugin_config directory

    QString sceneTreeConfigPath = streamupConfigDir.filePath("obs_scene_tree_view/scene_tree.json");
    bfree(streamup_config_path);

    // Check if the file exists
    if (!QFile::exists(sceneTreeConfigPath)) {
        return false;
    }

    // Load the file and check if this scene collection exists in it
    obs_data_t *original_data = obs_data_create_from_json_file(sceneTreeConfigPath.toUtf8().constData());
    if (!original_data) {
        return false;
    }

    // Check if this scene collection has data
    obs_data_array_t *collection_array = obs_data_get_array(original_data, sceneCollectionName.toUtf8().constData());
    bool hasData = (collection_array != nullptr && obs_data_array_count(collection_array) > 0);

    if (collection_array) {
        obs_data_array_release(collection_array);
    }
    obs_data_release(original_data);

    if (hasData) {
        outConfigPath = sceneTreeConfigPath;
    }

    return hasData;
}

// Convert obs_scene_tree_view format to StreamUP format
// obs_scene_tree_view: {"name": "X", "folder": [...], "is_expanded": bool} for folders, {"name": "X"} for scenes
// StreamUP: {"name": "X", "type": "folder", "children": [...]} for folders, {"name": "X", "type": "scene"} for scenes
obs_data_array_t* SceneTreeModel::convertSceneTreeViewFormat(obs_data_array_t *original_array)
{
    if (!original_array) return nullptr;

    obs_data_array_t *converted_array = obs_data_array_create();
    size_t count = obs_data_array_count(original_array);

    for (size_t i = 0; i < count; i++) {
        obs_data_t *original_item = obs_data_array_item(original_array, i);
        if (!original_item) continue;

        obs_data_t *converted_item = obs_data_create();
        const char *name = obs_data_get_string(original_item, "name");
        obs_data_set_string(converted_item, "name", name);

        // Check if this is a folder (has "folder" array) or a scene
        obs_data_array_t *folder_contents = obs_data_get_array(original_item, "folder");
        if (folder_contents) {
            // This is a folder
            obs_data_set_string(converted_item, "type", "folder");
            bool is_expanded = obs_data_get_bool(original_item, "is_expanded");
            obs_data_set_bool(converted_item, "expanded", is_expanded);

            // Recursively convert folder contents
            obs_data_array_t *converted_children = convertSceneTreeViewFormat(folder_contents);
            obs_data_set_array(converted_item, "children", converted_children);
            obs_data_array_release(converted_children);
            obs_data_array_release(folder_contents);
        } else {
            // This is a scene
            obs_data_set_string(converted_item, "type", "scene");
        }

        obs_data_array_push_back(converted_array, converted_item);
        obs_data_release(converted_item);
        obs_data_release(original_item);
    }

    return converted_array;
}

bool SceneTreeModel::migrateCurrentCollection()
{
    char *scene_collection = obs_frontend_get_current_scene_collection();
    if (!scene_collection) {
        return false;
    }

    QString collectionName = QString::fromUtf8(scene_collection);
    bfree(scene_collection);

    QString configPath;
    if (!checkMigrationAvailable(collectionName, configPath)) {
        return false;
    }

    // Load the original plugin's config file
    obs_data_t *original_data = obs_data_create_from_json_file(configPath.toUtf8().constData());
    if (!original_data) {
        return false;
    }

    // Get the data for this specific collection
    obs_data_array_t *collection_array = obs_data_get_array(original_data, collectionName.toUtf8().constData());
    if (!collection_array) {
        obs_data_release(original_data);
        return false;
    }

    // Convert from obs_scene_tree_view format to StreamUP format
    obs_data_array_t *converted_array = convertSceneTreeViewFormat(collection_array);
    if (!converted_array) {
        obs_data_array_release(collection_array);
        obs_data_release(original_data);
        return false;
    }

    // Get our config path
    char *our_config_path = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!our_config_path) {
        obs_data_array_release(converted_array);
        obs_data_array_release(collection_array);
        obs_data_release(original_data);
        return false;
    }

    QString configDir = QString::fromUtf8(our_config_path);
    QString configFile = configDir + "/scene_tree_normal.json";
    bfree(our_config_path);

    // Load existing data to preserve other collections
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (!root_data) {
        root_data = obs_data_create();
    }

    // Set only this collection's converted data
    obs_data_set_array(root_data, collectionName.toUtf8().constData(), converted_array);

    // Save
    obs_data_save_json(root_data, configFile.toUtf8().constData());

    obs_data_array_release(converted_array);
    obs_data_array_release(collection_array);
    obs_data_release(root_data);
    obs_data_release(original_data);

    return true;
}

void SceneTreeModel::loadOriginalFolderArray(obs_data_array_t *folder_array, QStandardItem &parent)
{
    // This loads the original plugin's format which is already compatible
    // The format is the same, so we can use the existing loadFolderArray
    loadFolderArray(folder_array, parent);
}

void SceneTreeModel::removeSceneFromTracking(obs_weak_source_t *weak_source)
{
    auto it = m_scenesInTree.find(weak_source);
    if (it != m_scenesInTree.end()) {
        m_scenesInTree.erase(it);
        // Note: Don't release weak_source here - it will be released by the SceneTreeItem destructor

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
            "Removed scene from tracking (weak reference managed by item)");
    }
}

obs_data_array_t *SceneTreeModel::createFolderArray(QStandardItem &parent)
{
    obs_data_array_t *folder_array = obs_data_array_create();

    for (int i = 0; i < parent.rowCount(); ++i) {
        QStandardItem *child = parent.child(i);
        if (!child) continue;

        obs_data_t *item_data = obs_data_create();

        if (child->type() == SceneFolderItem::UserType + 1) {
            // This is a folder
            obs_data_set_string(item_data, "name", child->text().toUtf8().constData());
            obs_data_set_string(item_data, "type", "folder");
            obs_data_set_bool(item_data, "expanded", true); // TODO: Track expansion state

            // Save custom color if set
            QVariant colorData = child->data(Qt::UserRole + 1);
            if (colorData.isValid()) {
                QColor color = colorData.value<QColor>();
                obs_data_set_string(item_data, "custom_color", color.name().toUtf8().constData());
            }

            // Recursively save children
            obs_data_array_t *children = createFolderArray(*child);
            obs_data_set_array(item_data, "children", children);
            obs_data_array_release(children);

        } else if (child->type() == SceneTreeItem::UserType + 2) {
            // This is a scene
            obs_data_set_string(item_data, "name", child->text().toUtf8().constData());
            obs_data_set_string(item_data, "type", "scene");

            // Save custom color if set
            QVariant colorData = child->data(Qt::UserRole + 1);
            if (colorData.isValid()) {
                QColor color = colorData.value<QColor>();
                obs_data_set_string(item_data, "custom_color", color.name().toUtf8().constData());
            }
        }

        obs_data_array_push_back(folder_array, item_data);
        obs_data_release(item_data);
    }

    return folder_array;
}

void SceneTreeModel::loadFolderArray(obs_data_array_t *folder_array, QStandardItem &parent)
{
    size_t count = obs_data_array_count(folder_array);
    for (size_t i = 0; i < count; i++) {
        obs_data_t *item_data = obs_data_array_item(folder_array, i);
        if (!item_data) continue;

        const char *name = obs_data_get_string(item_data, "name");
        const char *type = obs_data_get_string(item_data, "type");

        if (!name || !type) {
            obs_data_release(item_data);
            continue;
        }

        QString itemName = QString::fromUtf8(name);
        QString itemType = QString::fromUtf8(type);

        if (itemType == "folder") {
            // Create folder
            QStandardItem *folderItem = createFolderItem(itemName);
            if (folderItem) {
                parent.appendRow(folderItem);

                // Load custom color if present
                const char *colorName = obs_data_get_string(item_data, "custom_color");
                if (colorName && strlen(colorName) > 0) {
                    QColor color(colorName);
                    if (color.isValid()) {
                        folderItem->setData(color, Qt::UserRole + 1);
                    }
                }

                // Load children
                obs_data_array_t *children = obs_data_get_array(item_data, "children");
                if (children) {
                    loadFolderArray(children, *folderItem);
                    obs_data_array_release(children);
                }
            }
        } else if (itemType == "scene") {
            // Create placeholder for scene - will be filled by updateTree
            // Find the actual scene source
            obs_source_t *source = obs_get_source_by_name(name);
            if (source) {
                obs_weak_source_t *weak = obs_source_get_weak_source(source);
                QStandardItem *sceneItem = new SceneTreeItem(itemName, weak);
                parent.appendRow(sceneItem);

                // Load custom color if present
                const char *colorName = obs_data_get_string(item_data, "custom_color");
                if (colorName && strlen(colorName) > 0) {
                    QColor color(colorName);
                    if (color.isValid()) {
                        sceneItem->setData(color, Qt::UserRole + 1);
                    }
                }

                // Add to our tracking map
                m_scenesInTree[weak] = sceneItem;

                obs_source_release(source);

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "LoadConfig",
                    QString("Loaded scene '%1' from config").arg(itemName).toUtf8().constData());
            } else {
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "LoadConfig",
                    QString("Scene '%1' not found in OBS, skipping").arg(itemName).toUtf8().constData());
            }
        }

        obs_data_release(item_data);
    }
}


//==============================================================================
// SceneTreeView Implementation
//==============================================================================

SceneTreeView::SceneTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setupView();
}

void SceneTreeView::setupView()
{
    // Do nothing - let the parent dock configure all settings
    // This ensures we don't interfere with OBS theme styling
}

void SceneTreeView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-streamup-sceneorganiser")) {
        event->acceptProposedAction();
    }
    QTreeView::dragEnterEvent(event);
}

void SceneTreeView::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-streamup-sceneorganiser")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
    QTreeView::dragMoveEvent(event);
}

void SceneTreeView::dropEvent(QDropEvent *event)
{
    QTreeView::dropEvent(event);
}

void SceneTreeView::contextMenuEvent(QContextMenuEvent *event)
{
    // This is handled by the custom context menu signal
    event->accept();
}

void SceneTreeView::keyPressEvent(QKeyEvent *event)
{
    // Find the parent dock for all hotkey actions
    QWidget *parentWidget = this->parentWidget();
    SceneOrganiserDock *dock = nullptr;
    while (parentWidget && !dock) {
        dock = qobject_cast<SceneOrganiserDock*>(parentWidget);
        parentWidget = parentWidget->parentWidget();
    }

    if (!dock) {
        QTreeView::keyPressEvent(event);
        return;
    }

    // Handle hotkeys
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        // Delete/Remove hotkey
        dock->triggerRemove();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_F2) {
        // Rename hotkey (F2)
        dock->triggerRename();
        event->accept();
        return;
    }

    // Let the parent handle other keys
    QTreeView::keyPressEvent(event);
}

//==============================================================================
// Item Classes Implementation
//==============================================================================

SceneFolderItem::SceneFolderItem(const QString &folderName)
    : QStandardItem(folderName)
{
    setupFolderItem();
    // Set creation timestamp to current time
    setCreationTimestamp(QDateTime::currentMSecsSinceEpoch());
}

void SceneFolderItem::setupFolderItem()
{
    // NO custom styling - pure OBS theme
    updateIcon();
    setDropEnabled(true);
    setDragEnabled(true);
}

void SceneFolderItem::updateIcon()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (settings.sceneOrganiserShowIcons) {
        // Use OBS group icon for folders from theme properties
        QIcon groupIcon = GetThemeIcon("groupIcon");
        setIcon(groupIcon);
    } else {
        setIcon(QIcon());
    }
}

qint64 SceneFolderItem::getCreationTimestamp() const
{
    return data(Qt::UserRole + 100).toLongLong();
}

void SceneFolderItem::setCreationTimestamp(qint64 timestamp)
{
    setData(timestamp, Qt::UserRole + 100);
}

SceneTreeItem::SceneTreeItem(const QString &sceneName, obs_weak_source_t *weak_source)
    : QStandardItem(sceneName), m_weakSource(weak_source)
{
    setupSceneItem();
    updateFromObs();
    // Set creation timestamp to current time
    setCreationTimestamp(QDateTime::currentMSecsSinceEpoch());
}

SceneTreeItem::~SceneTreeItem()
{
    // Release the weak source reference when this item is destroyed
    if (m_weakSource) {
        obs_weak_source_release(m_weakSource);
        m_weakSource = nullptr;
    }
}

void SceneTreeItem::setupSceneItem()
{
    // NO custom styling - pure OBS theme
    updateIcon();
    setDropEnabled(false);
    setDragEnabled(true);
}

void SceneTreeItem::updateIcon()
{
    StreamUP::SettingsManager::PluginSettings settings = StreamUP::SettingsManager::GetCurrentSettings();
    if (settings.sceneOrganiserShowIcons) {
        // Use scene icon from theme properties
        QIcon sceneIcon = GetThemeIcon("sceneIcon");
        setIcon(sceneIcon);

        // Could add special styling for current scene in the future
        // obs_source_t *current_scene = obs_frontend_get_current_scene();
        // obs_source_t *this_scene = obs_weak_source_get_source(m_weakSource);
        // if (current_scene && this_scene && obs_source_get_ref(current_scene) == obs_source_get_ref(this_scene)) {
        //     // Current scene - could use different color or styling
        // }
        // if (current_scene) obs_source_release(current_scene);
        // if (this_scene) obs_source_release(this_scene);
    } else {
        setIcon(QIcon());
    }
}

qint64 SceneTreeItem::getCreationTimestamp() const
{
    return data(Qt::UserRole + 100).toLongLong();
}

void SceneTreeItem::setCreationTimestamp(qint64 timestamp)
{
    setData(timestamp, Qt::UserRole + 100);
}

void SceneTreeItem::updateFromObs()
{
    // Update icon in case current scene changed
    updateIcon();

    // Get scene info from OBS and update display
    obs_source_t *source = obs_get_source_by_name(text().toUtf8().constData());
    if (source) {
        // Could show additional info like scene item count, etc.
        obs_source_release(source);
    }
}

} // namespace SceneOrganiser
} // namespace StreamUP

//==============================================================================
// Tree View Helper Methods
//==============================================================================

void StreamUP::SceneOrganiser::SceneOrganiserDock::forceTreeViewRepaint()
{
    if (m_treeView) {
        m_treeView->viewport()->repaint();
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Repaint",
            "Forced tree view repaint after drag and drop");
    }
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::scheduleOptimizedUpdate()
{
    if (!m_updatesPending) {
        m_updatesPending = true;
        m_updateBatchTimer->start();
    }
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::processBatchedUpdates()
{
    if (!m_updatesPending) {
        return;
    }

    m_updatesPending = false;

    // Perform efficient batched updates
    if (m_treeView) {
        // Update the viewport instead of forcing a full repaint
        m_treeView->viewport()->update();
        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Batch Update", "Processed batched tree view update");
    }
}

void StreamUP::SceneOrganiser::SceneOrganiserDock::clearIconCaches()
{
    ClearIconCaches();
}


void StreamUP::SceneOrganiser::SceneOrganiserDock::onThemeChanged()
{
    // Clear caches and notify all dock instances to refresh
    OnThemeChanged();

    // Update all dock instances efficiently
    for (auto* dock : s_dockInstances) {
        if (dock && dock->m_model) {
            // Update theme state
            dock->currentThemeIsDark = StreamUP::UIHelpers::IsOBSThemeDark();
            // Update all icons with the new theme
            dock->updateAllItemIcons(dock->m_model->invisibleRootItem());
            // Schedule viewport repaint
            dock->scheduleOptimizedUpdate();
        }
    }
}

//==============================================================================
// CustomColorDelegate Implementation
//==============================================================================

StreamUP::SceneOrganiser::CustomColorDelegate::CustomColorDelegate(SceneOrganiserDock *dock, QObject *parent)
    : QStyledItemDelegate(parent), m_dock(dock)
{
}

void StreamUP::SceneOrganiser::CustomColorDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid() || !m_dock) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Get the item to check for custom color
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(m_dock->m_model);
    if (!model) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Map from proxy model to source model if needed
    QModelIndex sourceIndex = index;
    if (m_dock->m_proxyModel) {
        sourceIndex = m_dock->m_proxyModel->mapToSource(index);
    }

    QStandardItem *item = model->itemFromIndex(sourceIndex);
    if (!item) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Check if this item has a custom color
    QVariant colorData = item->data(Qt::UserRole + 1);
    if (!colorData.isValid()) {
        // No custom color, use default painting
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QColor customColor = colorData.value<QColor>();
    if (!customColor.isValid()) {
        // Invalid color, use default painting
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    // Calculate the appropriate background color based on state
    QColor bgColor = customColor;
    if (option.state & QStyle::State_Selected) {
        bgColor = m_dock->getSelectionColor(customColor);
    } else if (option.state & QStyle::State_MouseOver) {
        bgColor = m_dock->getHoverColor(customColor);
    }

    // Calculate contrasting text color
    QColor textColor = m_dock->getContrastTextColor(bgColor);

    // Save painter state
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Draw rounded rectangle background with custom color
    QRect rect = option.rect;
    // Add some padding to match theme styling
    rect.adjust(2, 1, -2, -1);

    // Draw rounded rectangle (radius 4 matches most OBS themes)
    painter->setPen(Qt::NoPen);
    painter->setBrush(bgColor);
    painter->drawRoundedRect(rect, 4, 4);

    painter->restore();

    // Now let the base class paint the content (icon, text) with custom text color
    QStyleOptionViewItem modifiedOption = option;
    modifiedOption.palette.setColor(QPalette::Text, textColor);
    modifiedOption.palette.setColor(QPalette::HighlightedText, textColor);

    // Tell the style not to draw the background (we already did it)
    modifiedOption.backgroundBrush = QBrush(Qt::NoBrush);

    // Draw without focus rect to avoid visual artifacts
    modifiedOption.state &= ~QStyle::State_HasFocus;

    QStyledItemDelegate::paint(painter, modifiedOption, index);
}

#include "scene-organiser-dock.moc"
