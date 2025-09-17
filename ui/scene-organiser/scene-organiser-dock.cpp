#include "scene-organiser-dock.hpp"
#include "../ui-styles.hpp"
#include "../ui-helpers.hpp"
#include "../settings-manager.hpp"
#include "../../utilities/debug-logger.hpp"
#include "../../utilities/obs-data-helpers.hpp"
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
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QColor>
#include <QtSvg/QSvgRenderer>
#include <QSortFilterProxyModel>
#include <QDir>
#include <util/platform.h>

namespace StreamUP {
namespace SceneOrganiser {

// Helper function to get theme icon from OBS main window properties
static QIcon GetThemeIcon(const QString& propertyName)
{
    // Get the main OBS window to access theme properties
    QWidget *mainWindow = nullptr;

    // Find the main OBS window
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == "OBSBasic") {
            mainWindow = widget;
            break;
        }
    }

    if (mainWindow) {
        // Get the icon from the main window's theme properties
        QVariant iconProperty = mainWindow->property(propertyName.toUtf8().constData());
        if (iconProperty.isValid()) {
            return iconProperty.value<QIcon>();
        }
    }

    // Fallback to empty icon if property not found
    return QIcon();
}

// Helper function to create colored icons from SVG resources (copied from multidock)
static QIcon CreateColoredIcon(const QString& svgPath, const QColor& color, const QSize& size = QSize(16, 16))
{
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

    return QIcon(pixmap);
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
    , m_folderContextMenu(nullptr)
    , m_sceneContextMenu(nullptr)
    , m_backgroundContextMenu(nullptr)
    , m_saveTimer(new QTimer(this))
{
    s_dockInstances.append(this);

    // Set configuration key based on canvas type
    m_configKey = (m_canvasType == CanvasType::Vertical) ? "scene_organiser_vertical" : "scene_organiser_normal";

    setupUI();
    setupContextMenu();
    setupObsSignals();

    // Setup auto-save timer
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1000); // Save 1 second after last change
    connect(m_saveTimer, &QTimer::timeout, this, &SceneOrganiserDock::SaveConfiguration);

    // Load configuration after setup
    QTimer::singleShot(100, this, &SceneOrganiserDock::LoadConfiguration);

    // Initialize toggle icons state in context menus
    updateToggleIconsState();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Initialize",
        QString("Scene Organiser Dock created for %1 canvas")
        .arg(m_canvasType == CanvasType::Vertical ? "vertical" : "normal").toUtf8().constData());
}

SceneOrganiserDock::~SceneOrganiserDock()
{
    s_dockInstances.removeAll(this);
    SaveConfiguration();
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup", "Scene Organiser Dock destroyed");
}

bool SceneOrganiserDock::IsVerticalPluginDetected()
{
    // Check if Aitum Vertical plugin is installed using StreamUP's plugin detection system
    auto installedPlugins = StreamUP::PluginManager::GetInstalledPluginsCached();

    for (const auto& plugin : installedPlugins) {
        if (plugin.first.find("aitum-vertical") != std::string::npos ||
            plugin.first.find("Aitum Vertical") != std::string::npos) {
            return true;
        }
    }

    return false;
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
    setObjectName(QString("StreamUPSceneOrganiser%1").arg(m_canvasType == CanvasType::Vertical ? "Vertical" : "Normal"));

    // Use zero margins and spacing to match OBS dock style
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Create model and view
    m_model = new SceneTreeModel(m_canvasType, this);
    m_treeView = new SceneTreeView(this);
    m_treeView->setModel(m_model);

    // Configure tree view to match OBS scenes dock exactly
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(true);
    m_treeView->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeView->setDefaultDropAction(Qt::MoveAction);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setIndentation(20); // Match OBS indentation

    // Connect signals
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SceneOrganiserDock::onSceneSelectionChanged);
    connect(m_treeView, &QAbstractItemView::doubleClicked,
            this, &SceneOrganiserDock::onItemDoubleClicked);
    connect(m_treeView, &QWidget::customContextMenuRequested,
            this, &SceneOrganiserDock::onCustomContextMenuRequested);
    connect(m_model, &SceneTreeModel::modelChanged,
            [this]() { m_saveTimer->start(); });

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

    // Create a proper QToolBar like in the multidock
    m_toolbar = new QToolBar("Scene Organiser Controls", this);
    m_toolbar->setObjectName("SceneOrganiserBottomToolbar");
    m_toolbar->setMovable(false);
    m_toolbar->setFloatable(false);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setOrientation(Qt::Horizontal);

    // Match native OBS scenes dock styling exactly
    m_toolbar->setIconSize(QSize(14, 14));

    // Apply exact OBS scenes dock toolbar styling (with proper padding and spacing)
    m_toolbar->setStyleSheet(
        "QToolBar {"
        "    background-color: #161617;"
        "    min-height: 24px;"
        "    max-height: 24px;"
        "    border: none;"
        "    padding: 2px 4px 2px 4px;"
        "    spacing: 2px;"
        "}"
        "QToolButton {"
        "    background: transparent;"
        "    border: none;"
        "    border-radius: 4px;"
        "    margin: 0px 1px 0px 1px;"
        "    min-width: 20px;"
        "    max-width: 20px;"
        "    min-height: 20px;"
        "    max-height: 20px;"
        "    padding: 1px;"
        "}"
        "QToolButton:hover {"
        "    background-color: #0f7bcf;"
        "    border-radius: 4px;"
        "}"
        "QToolButton:pressed {"
        "    background-color: #0a5a9c;"
        "    border-radius: 4px;"
        "}"
        "QToolButton:disabled {"
        "    background: transparent;"
        "}"
        "QToolButton::menu-indicator {"
        "    image: none;"
        "    width: 0px;"
        "    height: 0px;"
        "}"
        "QToolButton::menu-button {"
        "    border: none;"
        "    background: transparent;"
        "    width: 0px;"
        "}"
    );

    // Create QToolButtons and add them to toolbar (to properly support themeID)
    // This approach allows themeID properties to work while maintaining proper toolbar styling

    // Add button with dropdown menu
    QToolButton *addButton = new QToolButton(this);
    addButton->setProperty("themeID", "addIconSmall");
    addButton->setProperty("class", "icon-plus");
    addButton->setToolTip("Add folder or create scene");
    addButton->setPopupMode(QToolButton::InstantPopup);

    // Create a menu for the add button
    QMenu *addMenu = new QMenu(this);
    addMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddFolderClicked);
    addMenu->addAction(obs_module_text("SceneOrganiser.Action.CreateScene"), this, &SceneOrganiserDock::onCreateSceneClicked);
    addButton->setMenu(addMenu);

    // Don't set a default action to avoid icon overlap - just connect the clicked signal
    connect(addButton, &QToolButton::clicked, this, &SceneOrganiserDock::onAddFolderClicked);

    m_toolbar->addWidget(addButton);
    m_addFolderAction = nullptr; // No separate action needed for dropdown button

    // Remove button
    QToolButton *removeButton = new QToolButton(this);
    removeButton->setProperty("themeID", "removeIconSmall");
    removeButton->setProperty("class", "icon-trash");
    removeButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Remove"));
    removeButton->setEnabled(false);
    connect(removeButton, &QToolButton::clicked, this, &SceneOrganiserDock::onRemoveClicked);

    m_toolbar->addWidget(removeButton);
    m_removeAction = nullptr; // Direct button connection

    // Filters button (using theme info: .icon-filter uses url(theme:Dark/filter.svg))
    QToolButton *filtersButton = new QToolButton(this);
    filtersButton->setProperty("class", "icon-filter");
    filtersButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.Filters"));
    filtersButton->setEnabled(false);
    connect(filtersButton, &QToolButton::clicked, this, &SceneOrganiserDock::onFiltersClicked);

    m_toolbar->addWidget(filtersButton);
    m_filtersAction = nullptr; // Direct button connection

    // Add separator
    m_toolbar->addSeparator();

    // Move up button
    QToolButton *moveUpButton = new QToolButton(this);
    moveUpButton->setProperty("themeID", "upArrowIconSmall");
    moveUpButton->setProperty("class", "icon-up");
    moveUpButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.MoveUp"));
    moveUpButton->setEnabled(false);
    connect(moveUpButton, &QToolButton::clicked, this, &SceneOrganiserDock::onMoveUpClicked);

    m_toolbar->addWidget(moveUpButton);
    m_moveUpAction = nullptr; // Direct button connection

    // Move down button
    QToolButton *moveDownButton = new QToolButton(this);
    moveDownButton->setProperty("themeID", "downArrowIconSmall");
    moveDownButton->setProperty("class", "icon-down");
    moveDownButton->setToolTip(obs_module_text("SceneOrganiser.Tooltip.MoveDown"));
    moveDownButton->setEnabled(false);
    connect(moveDownButton, &QToolButton::clicked, this, &SceneOrganiserDock::onMoveDownClicked);

    m_toolbar->addWidget(moveDownButton);
    m_moveDownAction = nullptr; // Direct button connection

    // Store button references for additional state management if needed
    m_addButton = addButton;
    m_removeButton = removeButton;
    m_filtersButton = filtersButton;
    m_moveUpButton = moveUpButton;
    m_moveDownButton = moveDownButton;

    // Add toolbar to the bottom of the layout
    m_mainLayout->addWidget(m_toolbar, 0); // 0 means don't stretch

    // Initialize toolbar button states
    updateToolbarState();
}

void SceneOrganiserDock::setupContextMenu()
{
    // Folder context menu
    m_folderContextMenu = new QMenu(this);
    m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.RenameFolder"), [this]() {
        // Implement rename folder functionality
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            auto item = m_model->itemFromIndex(selectedIndexes.first());
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                bool ok;
                QString newName = QInputDialog::getText(this,
                    obs_module_text("SceneOrganiser.Dialog.RenameFolder.Title"),
                    obs_module_text("SceneOrganiser.Dialog.RenameFolder.Text"),
                    QLineEdit::Normal, item->text(), &ok);
                if (ok && !newName.isEmpty()) {
                    item->setText(newName);
                    m_saveTimer->start();
                }
            }
        }
    });

    m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.DeleteFolder"), [this]() {
        // Implement delete folder functionality
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            auto item = m_model->itemFromIndex(selectedIndexes.first());
            if (item && item->type() == SceneFolderItem::UserType + 1) {
                // Move children to root before deleting folder
                while (item->hasChildren()) {
                    auto child = item->takeChild(0);
                    m_model->invisibleRootItem()->appendRow(child);
                }
                m_model->removeRow(item->row(), item->parent() ? item->parent()->index() : QModelIndex());
                m_saveTimer->start();
            }
        }
    });

    m_folderContextMenu->addSeparator();
    m_folderToggleIconsAction = m_folderContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_folderToggleIconsAction->setCheckable(true);

    // Scene context menu
    m_sceneContextMenu = new QMenu(this);
    m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.SwitchToScene"), [this]() {
        auto selectedIndexes = m_treeView->selectionModel()->selectedIndexes();
        if (!selectedIndexes.isEmpty()) {
            auto item = m_model->itemFromIndex(selectedIndexes.first());
            if (item && item->type() == SceneTreeItem::UserType + 2) {
                obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
                if (source) {
                    obs_frontend_set_current_scene(source);
                    obs_source_release(source);
                }
            }
        }
    });

    m_sceneContextMenu->addSeparator();
    m_sceneToggleIconsAction = m_sceneContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_sceneToggleIconsAction->setCheckable(true);

    // Background context menu
    m_backgroundContextMenu = new QMenu(this);
    m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.AddFolder"), this, &SceneOrganiserDock::onAddFolderClicked);
    m_backgroundContextMenu->addSeparator();
    m_backgroundToggleIconsAction = m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.ToggleIcons"), this, &SceneOrganiserDock::onToggleIconsClicked);
    m_backgroundToggleIconsAction->setCheckable(true);
    m_backgroundContextMenu->addAction(obs_module_text("SceneOrganiser.Action.Refresh"), this, &SceneOrganiserDock::onRefreshClicked);
}

void SceneOrganiserDock::setupObsSignals()
{
    obs_frontend_add_event_callback(onFrontendEvent, this);
}

void SceneOrganiserDock::refreshSceneList()
{
    m_model->updateTree();
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
    bool canMoveUp = false;
    bool canMoveDown = false;

    if (hasSelection) {
        QModelIndex index = selected.indexes().first();
        QStandardItem *item = m_model->itemFromIndex(index);

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

    // Update toolbar button states (direct button control)
    if (m_removeButton) m_removeButton->setEnabled(hasSelection);
    if (m_filtersButton) m_filtersButton->setEnabled(isScene);
    if (m_moveUpButton) m_moveUpButton->setEnabled(hasSelection && canMoveUp);
    if (m_moveDownButton) m_moveDownButton->setEnabled(hasSelection && canMoveDown);
}

void SceneOrganiserDock::updateToolbarState()
{
    if (!m_toolbar) {
        return;
    }

    // This method can be used for additional toolbar state updates if needed
    // Currently, the selection-based state updates are handled in onSceneSelectionChanged
}

void SceneOrganiserDock::onItemDoubleClicked(const QModelIndex &index)
{
    auto item = m_model->itemFromIndex(index);
    if (item && item->type() == SceneTreeItem::UserType + 2) {
        // Switch to scene on double-click
        obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
        if (source) {
            obs_frontend_set_current_scene(source);
            obs_source_release(source);
        }
    }
}

void SceneOrganiserDock::onCustomContextMenuRequested(const QPoint &pos)
{
    QModelIndex index = m_treeView->indexAt(pos);

    if (index.isValid()) {
        auto item = m_model->itemFromIndex(index);
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
    Q_UNUSED(index)
    m_folderContextMenu->exec(pos);
}

void SceneOrganiserDock::showSceneContextMenu(const QPoint &pos, const QModelIndex &index)
{
    Q_UNUSED(index)
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

            // Optionally switch to the new scene
            obs_frontend_set_current_scene(scene_source);

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

    QModelIndex index = selected.first();
    QStandardItem *item = m_model->itemFromIndex(index);
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
                obs_source_remove(source);
                obs_source_release(source);

                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Scene Removal",
                    QString("Deleted scene from OBS: %1").arg(itemName).toUtf8().constData());
            }
        }

        // Remove from tree model (this will be handled by OBS events for scenes)
        if (isFolder) {
            QStandardItem *parent = item->parent();
            if (!parent) parent = m_model->invisibleRootItem();
            parent->removeRow(item->row());
            m_saveTimer->start();
        }
        // For scenes, the tree will be updated automatically by OBS events
    }
}

void SceneOrganiserDock::onFiltersClicked()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex index = selected.first();
    QStandardItem *item = m_model->itemFromIndex(index);
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

    QModelIndex index = selected.first();
    QStandardItem *item = m_model->itemFromIndex(index);
    if (!item) return;

    QStandardItem *parent = item->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = item->row();
    if (currentRow > 0) {
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        parent->insertRow(currentRow - 1, items);

        // Restore selection
        QModelIndex newIndex = m_model->indexFromItem(items.first());
        m_treeView->selectionModel()->select(newIndex, QItemSelectionModel::ClearAndSelect);

        m_saveTimer->start();
    }
}

void SceneOrganiserDock::onMoveDownClicked()
{
    QModelIndexList selected = m_treeView->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) return;

    QModelIndex index = selected.first();
    QStandardItem *item = m_model->itemFromIndex(index);
    if (!item) return;

    QStandardItem *parent = item->parent();
    if (!parent) parent = m_model->invisibleRootItem();

    int currentRow = item->row();
    if (currentRow < parent->rowCount() - 1) {
        QList<QStandardItem*> items = parent->takeRow(currentRow);
        parent->insertRow(currentRow + 1, items);

        // Restore selection
        QModelIndex newIndex = m_model->indexFromItem(items.first());
        m_treeView->selectionModel()->select(newIndex, QItemSelectionModel::ClearAndSelect);

        m_saveTimer->start();
    }
}

void SceneOrganiserDock::onRefreshClicked()
{
    refreshSceneList();
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

void SceneOrganiserDock::onSettingsChanged()
{
    // Handle settings changes if needed
    LoadConfiguration();
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
            dock->m_model->loadSceneTree();
            dock->m_model->updateTree();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->m_model->updateTree();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
        // Save current scene tree before switching
        dock->m_model->saveSceneTree();
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->m_model->loadSceneTree();
            dock->m_model->updateTree();
        });
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED:
        QTimer::singleShot(100, dock, [dock]() {
            dock->m_model->saveSceneTree();
            dock->m_model->updateTree();
        });
        break;
    default:
        break;
    }
}

void SceneOrganiserDock::SaveConfiguration()
{
    char *configPath = obs_module_get_config_path(obs_current_module(), "scene_organiser_configs");
    if (!configPath) return;

    QString configDir = QString::fromUtf8(configPath);
    QString configFile = configDir + "/" + m_configKey + ".json";
    bfree(configPath);

    // Ensure the directory exists
    QDir dir;
    if (!dir.mkpath(configDir)) {
        StreamUP::DebugLogger::LogInfo("SceneOrganiser",
            QString("Failed to create config directory: %1").arg(configDir).toUtf8().constData());
        return;
    }

    // Now using DigitOtter approach - save is handled by saveSceneTree()
    // which is called automatically on OBS frontend events
    m_model->saveSceneTree();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        "Configuration saved using DigitOtter approach");
}

void SceneOrganiserDock::LoadConfiguration()
{
    // Now using the DigitOtter approach - this is handled by frontend events
    // No need to manually load configuration here
    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
        "Configuration loading handled by frontend events");
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

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
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

    QStandardItem *parentItem = itemFromIndex(parent);
    if (!parentItem) {
        parentItem = invisibleRootItem();
    } else if (parentItem->type() == SceneTreeItem::UserType + 2) {
        // Can't drop on scene items
        return false;
    }

    if (row < 0)
        row = 0;

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
            // Move scene item
            moveSceneItem(originalItem, row, parentItem);
        } else if (originalItem->type() == SceneFolderItem::UserType + 1) {
            // Move folder item
            moveSceneFolder(originalItem, row, parentItem);
        }

        ++row; // Increment for next item
    }

    emit modelChanged();
    return true;
}

void SceneTreeModel::updateTree(const QModelIndex &selectedIndex)
{
    // Get all scenes from OBS
    struct obs_frontend_source_list scene_list = {0};
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

        // Check if scene already in tree
        scene_it = m_scenesInTree.find(weak);
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
    // For now, show all scenes. In the future, we could filter based on canvas resolution
    // or other properties to determine if a scene belongs to vertical vs normal canvas
    Q_UNUSED(scene)

    // TODO: Implement canvas-specific filtering based on scene properties
    // This could check scene resolution, special naming conventions, or metadata

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

// OLD CONFIG METHODS - REPLACED BY DIGITOTTER APPROACH
/*
void SceneTreeModel::saveToConfig(obs_data_t *config)
{
    obs_data_array_t *rootArray = obs_data_array_create();

    // Save root level items
    for (int i = 0; i < invisibleRootItem()->rowCount(); ++i) {
        QStandardItem *item = invisibleRootItem()->child(i);
        if (item) {
            obs_data_t *itemData = obs_data_create();
            obs_data_set_string(itemData, "name", item->text().toUtf8().constData());
            obs_data_set_int(itemData, "type", item->type());

            if (item->type() == SceneFolderItem::UserType + 1) {
                // Save folder children
                obs_data_array_t *childrenArray = obs_data_array_create();
                for (int j = 0; j < item->rowCount(); ++j) {
                    QStandardItem *child = item->child(j);
                    if (child) {
                        obs_data_t *childData = obs_data_create();
                        obs_data_set_string(childData, "name", child->text().toUtf8().constData());
                        obs_data_set_int(childData, "type", child->type());
                        obs_data_array_push_back(childrenArray, childData);
                        obs_data_release(childData);
                    }
                }
                obs_data_set_array(itemData, "children", childrenArray);
                obs_data_array_release(childrenArray);
            }

            obs_data_array_push_back(rootArray, itemData);
            obs_data_release(itemData);
        }
    }

    obs_data_set_array(config, "tree_structure", rootArray);
    obs_data_array_release(rootArray);
}

void SceneTreeModel::loadFromConfig(obs_data_t *config)
{
    obs_data_array_t *rootArray = obs_data_get_array(config, "tree_structure");
    if (!rootArray) return;

    // First pass: create all folders
    QMap<QString, QStandardItem*> folderMap;

    size_t count = obs_data_array_count(rootArray);
    for (size_t i = 0; i < count; i++) {
        obs_data_t *itemData = obs_data_array_item(rootArray, i);
        if (!itemData) continue;

        const char *name = obs_data_get_string(itemData, "name");
        int type = (int)obs_data_get_int(itemData, "type");

        // Validate folder name is not empty
        if (!name || strlen(name) == 0) {
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                "Skipping item with empty name in config");
            continue;
        }

        QString itemName = QString::fromUtf8(name);
        if (itemName.isEmpty() || itemName.trimmed().isEmpty()) {
            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                QString("Skipping item with invalid name: '%1'").arg(itemName).toUtf8().constData());
            continue;
        }

        if (type == SceneFolderItem::UserType + 1) {
            QStandardItem *folderItem = createFolderItem(itemName);
            if (folderItem && !folderItem->text().isEmpty()) {
                invisibleRootItem()->appendRow(folderItem);
                folderMap[itemName] = folderItem;
                StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                    QString("Created folder: '%1'").arg(itemName).toUtf8().constData());
            }
        }

        obs_data_release(itemData);
    }

    // Second pass: organize scenes
    for (size_t i = 0; i < count; i++) {
        obs_data_t *itemData = obs_data_array_item(rootArray, i);
        if (!itemData) continue;

        const char *name = obs_data_get_string(itemData, "name");
        int type = (int)obs_data_get_int(itemData, "type");

        if (type == SceneFolderItem::UserType + 1) {
            // Handle folder children
            QStandardItem *folderItem = folderMap[QString::fromUtf8(name)];
            obs_data_array_t *childrenArray = obs_data_get_array(itemData, "children");
            if (childrenArray && folderItem) {
                size_t childCount = obs_data_array_count(childrenArray);
                for (size_t j = 0; j < childCount; j++) {
                    obs_data_t *childData = obs_data_array_item(childrenArray, j);
                    if (childData) {
                        const char *childName = obs_data_get_string(childData, "name");
                        int childType = (int)obs_data_get_int(childData, "type");

                        // Validate child name is not empty
                        if (!childName || strlen(childName) == 0) {
                            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                                "Skipping child with empty name in config");
                            continue;
                        }

                        QString childSceneName = QString::fromUtf8(childName);
                        if (childSceneName.isEmpty() || childSceneName.trimmed().isEmpty()) {
                            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                                QString("Skipping child with invalid name: '%1'").arg(childSceneName).toUtf8().constData());
                            continue;
                        }

                        if (childType == SceneTreeItem::UserType + 2) {
                            // Scene handling is now done by updateTree - skip here
                            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                                QString("Scene '%1' handling delegated to updateTree").arg(childSceneName).toUtf8().constData());
                            if (sceneItem && !sceneItem->text().isEmpty()) {
                                QStandardItem *oldParent = sceneItem->parent();
                                if (!oldParent) oldParent = invisibleRootItem();

                                QStandardItem *movedItem = oldParent->takeChild(sceneItem->row());
                                if (movedItem && !movedItem->text().isEmpty()) {
                                    folderItem->appendRow(movedItem);
                                    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                                        QString("Moved scene '%1' to folder '%2'").arg(childSceneName, folderItem->text()).toUtf8().constData());
                                }
                            }
                        } else if (childType == SceneTreeItem::UserType + 2) {
                            StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Config",
                                QString("Scene '%1' no longer exists in OBS, skipping").arg(childSceneName).toUtf8().constData());
                        }
                        obs_data_release(childData);
                    }
                }
                obs_data_array_release(childrenArray);
            }
        }

        obs_data_release(itemData);
    }

    obs_data_array_release(rootArray);
}
*/
// END OLD CONFIG METHODS

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
    if (!item || item->type() != SceneTreeItem::UserType + 2) {
        return;
    }

    SceneTreeItem *sceneItem = static_cast<SceneTreeItem*>(item);
    QString sceneName = item->text();
    obs_weak_source_t *weak = sceneItem->getWeakSource();

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
        QString("Moving scene '%1'").arg(sceneName).toUtf8().constData());

    // Add reference to weak source for new item
    obs_weak_source_addref(weak);

    // Create new scene item with same weak reference
    QStandardItem *newItem = new SceneTreeItem(sceneName, weak);
    if (newItem) {
        parentItem->insertRow(row, newItem);

        // Update our tracking map
        m_scenesInTree[weak] = newItem;

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Move",
            QString("Moved scene '%1' to row %2").arg(sceneName).arg(row).toUtf8().constData());
    }

    // Original item will be removed by Qt's drag/drop system
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
    // Release all weak references and clear the tree
    for (auto &scene_pair : m_scenesInTree) {
        obs_weak_source_release(scene_pair.first);
    }
    m_scenesInTree.clear();

    // Clear the model
    clear();
    setupRootItem();
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
    QString configFile = configDir + "/scene_tree_" +
        (m_canvasType == CanvasType::Vertical ? "vertical" : "normal") + ".json";

    bfree(configPath);

    // Create root data
    obs_data_t *root_data = obs_data_create();
    obs_data_array_t *folder_array = createFolderArray(*invisibleRootItem());
    obs_data_set_array(root_data, scene_collection, folder_array);
    obs_data_array_release(folder_array);

    // Save to file
    obs_data_save_json(root_data, configFile.toUtf8().constData());
    obs_data_release(root_data);

    bfree(scene_collection);

    StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Save",
        QString("Saved scene tree to: %1").arg(configFile).toUtf8().constData());
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
    QString configFile = configDir + "/scene_tree_" +
        (m_canvasType == CanvasType::Vertical ? "vertical" : "normal") + ".json";

    bfree(configPath);

    // Clean up previous tree
    cleanupSceneTree();

    // Load from file
    obs_data_t *root_data = obs_data_create_from_json_file(configFile.toUtf8().constData());
    if (root_data) {
        obs_data_array_t *folder_array = obs_data_get_array(root_data, scene_collection);
        if (folder_array) {
            loadFolderArray(folder_array, *invisibleRootItem());
            obs_data_array_release(folder_array);
        }
        obs_data_release(root_data);

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Load",
            QString("Loaded scene tree from: %1").arg(configFile).toUtf8().constData());
    }

    bfree(scene_collection);
}

void SceneTreeModel::removeSceneFromTracking(obs_weak_source_t *weak_source)
{
    auto it = m_scenesInTree.find(weak_source);
    if (it != m_scenesInTree.end()) {
        m_scenesInTree.erase(it);
        obs_weak_source_release(weak_source);

        StreamUP::DebugLogger::LogDebug("SceneOrganiser", "Cleanup",
            "Removed scene from tracking and released weak reference");
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

            // Recursively save children
            obs_data_array_t *children = createFolderArray(*child);
            obs_data_set_array(item_data, "children", children);
            obs_data_array_release(children);

        } else if (child->type() == SceneTreeItem::UserType + 2) {
            // This is a scene
            obs_data_set_string(item_data, "name", child->text().toUtf8().constData());
            obs_data_set_string(item_data, "type", "scene");
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
    setHeaderHidden(true);
    setRootIsDecorated(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setExpandsOnDoubleClick(false);

    // Enable better drop indicators for easier targeting
    setDropIndicatorShown(true);

    // Remove custom styling - let it inherit from OBS theme
    // This ensures it matches the native OBS scenes dock exactly
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

//==============================================================================
// Item Classes Implementation
//==============================================================================

SceneFolderItem::SceneFolderItem(const QString &folderName)
    : QStandardItem(folderName)
{
    setupFolderItem();
}

void SceneFolderItem::setupFolderItem()
{
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

SceneTreeItem::SceneTreeItem(const QString &sceneName, obs_weak_source_t *weak_source)
    : QStandardItem(sceneName), m_weakSource(weak_source)
{
    setupSceneItem();
    updateFromObs();
}

SceneTreeItem::~SceneTreeItem()
{
    // Don't release weak source here - it's managed by the model
    // The model will release all weak sources in its destructor
}

void SceneTreeItem::setupSceneItem()
{
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