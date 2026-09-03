#pragma once

#include <QFrame>
#include <QTreeView>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QCheckBox>
#include <QTabBar>
#include <QStackedWidget>
#include <QListWidget>
#include <QTreeWidget>
#include <map>
#include <functional>
#include <obs.h>
#include <obs-frontend-api.h>
#include "../settings-manager.hpp"

namespace StreamUP {
namespace SceneOrganiser {

class SceneTreeModel;
class SceneTreeView;
class SceneOrganiserDock;
class CustomColorDelegate;

// Which canvas a dock is bound to. Normal is OBS's main canvas; Vertical is the
// canvas Aitum's Vertical Canvas plugin registers with the frontend (OBS 32.2+).
// The dock is otherwise identical for both, everything canvas-specific goes
// through scene-canvas.hpp.
enum class CanvasType {
    Normal,
    Vertical
};

// What a tab in the dock's tab bar is. Scenes is the organiser tree and is
// always present; Favourites and Recent are built-in lists that can each be
// switched off on their own; Custom is a list the user made and filled.
enum class QuickTabKind {
    Scenes,
    Favourites,
    Recent,
    Custom
};

// One entry in a tab's tree: either a folder (with children of its own) or a
// reference to a scene by name. A tab holds a list of these, so a tab can be
// organised with folders exactly like the Scenes tree - and independently of it,
// since the same scene may sit in a different folder on every tab.
//
// A scene is held by NAME rather than by a weak source: a tab is a saved
// arrangement, and a scene that disappears should leave the arrangement intact
// for when it comes back, not tear a hole in it.
struct TabNode {
    bool isFolder = false;
    QString name;
    QVector<TabNode> children;
};

// A user-made tab: a name and the tree they built in it.
struct CustomSceneTab {
    QString name;
    QVector<TabNode> nodes;
};

class SceneOrganiserDock : public QFrame {
    Q_OBJECT

public:
    explicit SceneOrganiserDock(CanvasType canvasType, QWidget *parent = nullptr);
    ~SceneOrganiserDock();

    CanvasType GetCanvasType() const { return m_canvasType; }

    static void NotifyAllDocksSettingsChanged();
    static void NotifySceneOrganiserIconsChanged();

    void SaveConfiguration();
    // Tab trees are saved as JSON, since they are nested. The old flat text
    // format is still read once, so tabs made before this survive the upgrade.
    void saveQuickTabs(const QString &configDir, const QString &sceneCollectionName);
    void loadQuickTabs(const QString &configDir, const QString &sceneCollectionName);
    bool loadLegacyQuickTabs(const QString &path);
    void LoadConfiguration();

private slots:
    void onSceneSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onItemClicked(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);
    void onCustomContextMenuRequested(const QPoint &pos);
    void onAddFolderClicked();
    void onCreateSceneClicked();
    void onFiltersClicked();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onToggleIconsClicked();
    void onExpandCollapseAllClicked();
    void updateExpandCollapseButtonState();
    void onSettingsChanged();
    void onIconsChanged();
    void onSettingsClicked();
    void onRenameSceneClicked();
    void onRenameFolderClicked();
    void onDuplicateSceneClicked();
    void onDeleteSceneClicked();
    void onCopyFiltersClicked();
    void onPasteFiltersClicked();
    void onSceneFiltersClicked();
    void onScreenshotSceneClicked();
    void onShowInMultiviewClicked();
    void onOpenProjectorClicked();
    void onOpenProjectorWindowClicked();
    void onOpenProjectorOnMonitorClicked();
    void onSceneMoveUpClicked();
    void onSceneMoveDownClicked();
    void onSceneMoveToTopClicked();
    void onSceneMoveToBottomClicked();
    void onHideSceneClicked();
    void onShowSceneClicked();
    void applySceneVisibility();
    void applySceneVisibilityRecursive(QStandardItem *parent);
    void updateHiddenScenesStyling();
    void updateHiddenScenesStylingRecursive(QStandardItem *parent);

public slots:
    static void onFrontendEvent(enum obs_frontend_event event, void *private_data);
    void onRemoveClicked();

public:
    // Public methods for keyboard shortcuts
    void triggerRename();
    void triggerRemove();
    // Activate (go-live/transition to) the currently selected scene — bound to
    // the Enter/Return key in the tree view.
    void triggerActivateSelectedScene();
    // Takes the given scene item live (studio mode aware). Ignores folders.
    void activateSceneItem(QStandardItem *item);
    // Enter in the search box: go live with the first match still visible.
    void activateFirstSearchMatch();
    // Focuses the search box of the dock bound to the given canvas. Static so a
    // frontend hotkey can reach it without holding a dock pointer.
    static void FocusSearchBox(CanvasType canvasType);
    // Runs the one-time load (config, folder tree, scenes, colours). Normally
    // driven by FINISHED_LOADING; public so a dock created after that event has
    // fired can be kicked directly. Idempotent.
    void performInitialLoad();

private:
    void setupUI();
    void setupContextMenu();
    void setupObsSignals();
    void setupSearchBar();
    void onSearchTextChanged(const QString &text);
    void onClearSearch();
    void saveExpansionState();
    void restoreExpansionState();
    void saveFolderExpansionState();
    void restoreFolderExpansionState();
    void createBottomToolbar();
    // Takes the height of OBS's own Sources toolbar so the two bars line up.
    void matchObsToolbarHeight();
    void updateDragEnabled();
    void updateToolbarState();
    void refreshSceneList();
    void applySortingIfEnabled();
    void sortManually(StreamUP::SettingsManager::SceneSortMethod method, QStandardItem *parent = nullptr);
    void updateFromObsScenes();
    void showFolderContextMenu(const QPoint &pos, const QModelIndex &index);
    void showSceneContextMenu(const QPoint &pos, const QModelIndex &index);
    void showBackgroundContextMenu(const QPoint &pos);
    void updateAllItemIcons(QStandardItem *parent);
    // Folder icons follow whether the row is open, which only the view knows.
    void setFolderExpandedState(const QModelIndex &proxyIndex, bool expanded);
    void syncFolderIcons(QStandardItem *parent = nullptr);
    void updateToggleIconsState();
    void updateLockActionStates();
    void updateActiveSceneHighlight();
    void updateActiveSceneHighlightRecursive(QStandardItem *parent, const QString &activeSceneName, const QString &previewSceneName = QString());
    // Moves the tree selection (preview/blue indicator) to a scene by name.
    void selectSceneByName(const QString &sceneName);
    void onSetCustomColorClicked();
    void onClearCustomColorClicked();
    // "Set Colour" submenu, mimicking OBS' native Sources menu: Clear, Custom
    // Colour, then the same eight preset swatches OBS offers.
    QMenu *createColorSubmenu();
    // "Set Icon" submenu: the OBS theme icons plus a custom image and a reset.
    // Shared by the folder and scene menus, like the colour one.
    QMenu *createIconSubmenu();
    void refreshIconMenuState();
    void applyIconSpec(const QString &spec);
    void onSetCustomIconImageClicked();
    // Icon tint, kept apart from the icon itself so either can change alone.
    void applyIconColor(const QColor &color);
    void refreshIconColorMenuState();
    void onSetCustomIconColorClicked();
    void refreshColorMenuState();
    void applyPresetColor(int presetIndex);
    void updateTreeViewStylesheet();
    void onToggleLockClicked();
    void setLocked(bool locked);
    void updateUIEnabledState();
    void populateProjectorMenu();
    // Rebuilt every time the menu opens: the transition list and the scene's
    // stored override can both change between one right click and the next.
    void populateTransitionOverrideMenu(obs_source_t *sceneSource);
    // Vertical dock only. Rebuilt every time the menu opens: the main scene
    // list and the links stored on it can both change between right clicks.
    void populateLinkedScenesMenu(obs_source_t *sceneSource);

public:
    // Color helper methods (public for CustomColorDelegate access)
    QColor getContrastTextColor(const QColor &backgroundColor);
    QColor getDefaultThemeTextColor();
    QColor adjustColorBrightness(const QColor &color, float factor);
    QColor ensureRowContrast(const QColor &bgColor);
    QColor getSelectionColor(const QColor &baseColor);
    QColor getHoverColor(const QColor &baseColor);

    // Layout undo: capture before a mutation, push after it.
    QString captureLayout();
    void pushLayoutUndo(const QString &name, const QString &before);
    static void ApplyLayoutSnapshot(const char *data);
    // True while an undo/redo is being applied, so the restore does not record
    // an undo entry of its own.
    bool m_applyingLayoutSnapshot = false;
    // Layout captured when an inline folder rename was started; consumed when
    // the edit lands. Empty when no rename is in flight.
    QString m_renameLayoutBefore;

    // Color application methods (public for SceneTreeModel access during drag & drop)
    void applyCustomColorToItem(QStandardItem *item, const QColor &color);
    void clearCustomColorFromItem(QStandardItem *item);
    void applyAllCustomColors(QStandardItem *parent = nullptr);

    // The row height every tab uses, from the settings (19-48, default 24).
    int currentRowHeight() const;

    // Force tree view repaint (used after drag and drop)
    void forceTreeViewRepaint();

    // Data members
    CanvasType m_canvasType;
    // Vertical dock only: the canvas we are mirroring, held weakly, plus the
    // signal wiring. Canvas scenes never raise the frontend's SCENE_LIST_CHANGED
    // event, so the vertical dock listens to the canvas itself instead.
    obs_weak_canvas_t *m_weakCanvas = nullptr;

    // Aitum drives its switching through its own transition, which sits on the
    // canvas channel and never changes, so the canvas raises no channel_change
    // when the live scene changes. Nothing tells us, so the vertical dock asks.
    // Vertical dock only; the main canvas has real frontend events.
    QTimer *m_verticalSceneWatch = nullptr;
    QString m_lastVerticalScene;
    void watchVerticalCurrentScene();
    void connectCanvasSignals();
    void disconnectCanvasSignals();
    static void OnCanvasSourceAdded(void *data, calldata_t *cd);
    static void OnCanvasSourceRemoved(void *data, calldata_t *cd);
    static void OnCanvasSourceRenamed(void *data, calldata_t *cd);
    // Fires for every source rename in OBS, whoever did the renaming. The three
    // things this dock stores by name have to follow, or they quietly lose the
    // scene: see renameStoredScene().
    static void OnSourceRenamed(void *data, calldata_t *cd);
    // Rewrites a scene's name everywhere this dock keeps one: hidden scenes,
    // recents, favourites and every custom tab.
    void renameStoredScene(const QString &oldName, const QString &newName);
    static void renameSceneInNodes(QVector<TabNode> &nodes, const QString &oldName, const QString &newName);
    static void OnCanvasChannelChanged(void *data, calldata_t *cd);
    QVBoxLayout *m_mainLayout;
    SceneTreeView *m_treeView;
    SceneTreeModel *m_model;
    QSortFilterProxyModel *m_proxyModel;

    // Search functionality
    QWidget *m_searchWidget = nullptr;
    QHBoxLayout *m_searchLayout;
    QLineEdit *m_searchEdit = nullptr;
    QMap<QPersistentModelIndex, bool> m_savedExpansionState;

    // Toolbar and buttons
    QToolBar *m_toolbar = nullptr;
    QAction *m_addFolderAction;
    QAction *m_removeAction;
    QAction *m_filtersAction;
    QAction *m_moveUpAction;
    QAction *m_moveDownAction;

    // Button references for state management
    QToolButton *m_addButton = nullptr;
    QToolButton *m_removeButton = nullptr;
    QToolButton *m_filtersButton = nullptr;
    QToolButton *m_moveUpButton = nullptr;
    QToolButton *m_moveDownButton = nullptr;
    QCheckBox *m_expandCollapseButton = nullptr;
    QCheckBox *m_lockButton = nullptr;
    QToolButton *m_settingsButton = nullptr;

    // Context menus
    QMenu *m_folderContextMenu;
    QMenu *m_sceneContextMenu;
    QMenu *m_backgroundContextMenu;
    QMenu *m_sceneOrderMenu;
    QMenu *m_sceneProjectorMenu;
    QMenu *m_sceneTransitionMenu = nullptr;
    QMenu *m_sceneLinkedScenesMenu = nullptr;

    // "Set Colour" submenu, shared by the folder and scene context menus (both
    // act on m_currentContextItem, so one instance serves both).
    QMenu *m_colorMenu = nullptr;
    QAction *m_colorClearAction = nullptr;
    QAction *m_colorCustomAction = nullptr;
    QList<QPushButton *> m_colorSwatchButtons;

    QMenu *m_iconMenu = nullptr;
    QAction *m_iconDefaultAction = nullptr;
    QAction *m_iconCustomAction = nullptr;
    QHash<QString, QAction *> m_iconThemeActions;
    QMenu *m_iconColorMenu = nullptr;
    QAction *m_iconColorClearAction = nullptr;
    QAction *m_iconColorCustomAction = nullptr;
    QList<QPushButton *> m_iconColorSwatchButtons;

    // Toggle actions (for checkmarks)
    QAction *m_folderToggleIconsAction;
    QAction *m_sceneToggleIconsAction;
    QAction *m_backgroundToggleIconsAction;

    // Context menu actions for lock state management
    QAction *m_deleteFolderAction;
    QAction *m_deleteSceneAction;
    QAction *m_sceneMoveUpAction;
    QAction *m_sceneMoveDownAction;
    QAction *m_sceneMoveToTopAction;
    QAction *m_sceneMoveToBottomAction;
    QAction *m_hideSceneAction;
    QAction *m_showSceneAction;

    // Lock/unlock actions in context menus
    QAction *m_folderLockAction;
    QAction *m_sceneLockAction;
    QAction *m_backgroundLockAction;

    // Configuration
    QString m_configKey;
    QTimer *m_saveTimer;

    // Clipboard for filters
    obs_weak_source_t *m_copyFiltersSource;

    // Color management
    QStandardItem *m_currentContextItem;

    // Lock state management
    bool m_isLocked;

    // Initial load state - prevents saving until first load completes
    bool m_initialLoadComplete;
    // Whether performInitialLoad() has already been kicked off, so a dock that
    // receives both a direct kick and FINISHED_LOADING only loads once.
    bool m_initialLoadStarted = false;

    // Expand/collapse state management
    bool m_allExpanded;

    // Scene visibility management
    QSet<QString> m_hiddenScenes;

    // The tab bar and the single list widget the flat tabs share. Which tabs
    // exist is decided by rebuildTabBar() from the settings plus m_customTabs.
    QTabBar *m_quickTabs = nullptr;
    QStackedWidget *m_viewStack = nullptr;
    // One tree serves every non-Scenes tab; it is repopulated on each switch.
    // The same view class as the Scenes tree, so every bit of its painting -
    // the cleared indent column, the folder guides, the chevron - applies here
    // too. A plain QTreeWidget looked like a different dock.
    SceneTreeView *m_quickTree = nullptr;
    QStandardItemModel *m_quickModel = nullptr;
    QSortFilterProxyModel *m_quickProxy = nullptr;
    // True while the tab tree is being filled, so the model signals that fire
    // during population are not mistaken for the user rearranging it.
    bool m_populatingQuickTree = false;
    // Whether the tab trees are collapsed. Remembered because a tab tree is
    // rebuilt on every refresh and would otherwise spring back open.
    bool m_quickTreeCollapsed = false;

    // Favourites is a tree like the custom tabs: starring a scene drops it in at
    // the top level, and it can then be filed into folders. Recent stays a plain
    // list - it is maintained by what you go live with, not arranged by hand.
    QVector<TabNode> m_favouriteNodes;
    QStringList m_recentScenes;
    // User-made tabs, also per scene collection.
    QVector<CustomSceneTab> m_customTabs;

    // The tab currently selected, held as kind + index rather than a bar
    // position, so it survives tabs being switched on and off around it.
    QuickTabKind m_currentKind = QuickTabKind::Scenes;
    int m_currentCustomTab = -1;

    QAction *m_favouriteToggleAction = nullptr;
    QMenu *m_addToTabMenu = nullptr;

    void setupQuickTabs();
    // Rebuilds the bar from scratch: the tree, whichever built-in tabs the
    // settings allow, then every custom tab. Restores the previous selection if
    // that tab still exists, otherwise falls back to the tree.
    void rebuildTabBar();
    // Repopulates the list behind the current tab.
    void refreshQuickList();
    void onQuickTreeActivated(const QModelIndex &index);
    void onQuickTreeContextMenu(const QPoint &pos);
    // Writes the tree widget's current shape back to the tab it belongs to,
    // after a drag or any other edit made in the widget itself.
    void commitQuickTreeToTab();
    // The nodes behind the current tab, when it is one the user arranges.
    QVector<TabNode> *editableNodesForCurrentTab();

    // Node helpers. All of them work on a tab's node list, not on OBS.
    static bool nodesContainScene(const QVector<TabNode> &nodes, const QString &sceneName);
    static bool removeSceneFromNodes(QVector<TabNode> &nodes, const QString &sceneName);
    static void collectSceneNames(const QVector<TabNode> &nodes, QStringList &out);
    static QString uniqueFolderName(const QVector<TabNode> &nodes, const QString &base);

    // Folder actions on a tab tree.
    void onAddTabFolderClicked();
    void onRenameTabFolderClicked(QStandardItem *item);
    void onRemoveFromTabClicked(QStandardItem *item);
    void onQuickTabChanged(int index);
    void onTabMoved(int from, int to);
    // Identity of the selected tab, for restoring it across a rebuild.
    QString currentTabToken() const;
    // The tab bar's order, by identity. Saved, so a tab switched off in the
    // settings returns to its old place rather than to the end.
    QStringList m_tabOrder;
    // The tab that was selected when the config was written, held as a token
    // until the tabs it may name have finished loading.
    QString m_currentKindToken;
    void onQuickTabsContextMenu(const QPoint &pos);
    void onToggleFavouriteClicked();

    // Custom tab management
    // Shared by create and rename: asks for a name and re-asks on a clash.
    void promptForTabName(const QString &title, const QString &fieldLabel, const QString &initial,
                          int skipIndex, std::function<void(const QString &)> onAccept);
    void onCreateCustomTabClicked();
    void onRenameCustomTabClicked(int customIndex);
    void onDeleteCustomTabClicked(int customIndex);
    void addSceneToCustomTab(const QString &sceneName, int customIndex);
    void removeSceneFromCustomTab(const QString &sceneName, int customIndex);
    // Rebuilt every time the scene menu opens: the custom tabs can change
    // between one right click and the next.
    void populateAddToTabMenu();
    int customTabIndexByName(const QString &name) const;

    // Pushes a scene to the front of the recents list. No-op if it is already
    // the most recent, so re-selecting the live scene does not churn the list.
    void noteRecentScene(const QString &sceneName);
    bool isFavourite(const QString &sceneName) const;
    // Tree-only controls are meaningless on the flat tabs, so they are hidden
    // there rather than left on screen doing nothing.
    void updateControlsForTab();
    // The scene selected on whichever tab is showing.
    QString selectedSceneOnCurrentTab() const;
    void showAddToTabMenu();
    void moveWithinCurrentTab(int direction);
    // Applies the tree's row height, icon size and font to the flat lists.
    void applyRowMetricsToQuickList();
    // How many recents to keep.
    static constexpr int kMaxRecentScenes = 20;

    // Click tracking for rename functionality
    QPersistentModelIndex m_lastClickedIndex;
    // Action references removed - using direct button approach for right-side buttons

    // Performance optimization members
    QTimer *m_updateBatchTimer;
    bool m_updatesPending;
    void scheduleOptimizedUpdate();
    void processBatchedUpdates();

    // Cache management
    static void clearIconCaches();
    static void onThemeChanged();

    // Mirrors the active theme's own list-row rules onto our tree views, so
    // hover and selection read the same here as in the Scenes/Sources docks.
    void applyThemeRowStyling();

    bool currentThemeIsDark;

    // OBS integration
    static QList<SceneOrganiserDock*> s_dockInstances;
};

// Custom tree model for scene organization
class SceneTreeModel : public QStandardItemModel {
    Q_OBJECT

public:
    explicit SceneTreeModel(CanvasType canvasType, QObject *parent = nullptr);
    ~SceneTreeModel();

    // Drag & drop support
    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;

    // Editing support
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                     int row, int column, const QModelIndex &parent) override;

    // Scene management
    void updateTree(const QModelIndex &selectedIndex = QModelIndex());
    void saveSceneTree();
    void loadSceneTree();
    QStandardItem *findSceneItem(obs_weak_source_t *weak_source);
    QStandardItem *findFolderItem(const QString &folderName);
    QStandardItem *createFolderItem(const QString &folderName);
    QStandardItem *createSceneItem(const QString &sceneName, obs_weak_source_t *weak_source);
    void moveSceneToFolder(obs_weak_source_t *weak_source, QStandardItem *folderItem);

    // Configuration (now handled by DigitOtter approach)
    // saveToConfig and loadFromConfig removed - using saveSceneTree/loadSceneTree instead

    // Migration from original obs_scene_tree_view plugin
    bool migrateFromOriginalPlugin(const QString &originalConfigPath);
    static bool checkMigrationAvailable(const QString &sceneCollectionName, QString &outConfigPath);
    bool migrateCurrentCollection();
    static obs_data_array_t* convertSceneTreeViewFormat(obs_data_array_t *original_array);

    // Cleanup
    QStandardItem *findItemByName(const QString &name, int itemType, QStandardItem *parent);
    QStandardItem *findSceneItemByName(const QString &name);
    QStandardItem *findFolderItemByName(const QString &name);
    // Whole-layout snapshot / restore, used to back undo and redo.
    QString serialiseLayout();
    void restoreLayout(const QString &json);
    // Removes scene rows the tracking map does not know about, which is what a
    // move that left its original behind produces. Returns how many went.
    int removeUntrackedSceneDuplicates(QStandardItem *parent = nullptr);
    void cleanupEmptyItems();
    void removeSceneFromTracking(obs_weak_source_t *weak_source);

private:
    void setupRootItem();
    bool isValidSceneForCanvas(obs_scene_t *scene);
    void cleanupEmptyItemsRecursive(QStandardItem *parent);
    bool isChildOf(QStandardItem *potentialChild, QStandardItem *potentialParent);
    void moveSceneItem(QStandardItem *item, int row, QStandardItem *parentItem);
    void moveSceneFolder(QStandardItem *item, int row, QStandardItem *parentItem);
    QString createUniqueFolderName(const QString &baseName, QStandardItem *parentItem);
    void cleanupSceneTree();
    bool isManagedScene(obs_source_t *source);
    obs_data_array_t *createFolderArray(QStandardItem &parent);
    void loadFolderArray(obs_data_array_t *folder_array, QStandardItem &parent);

    // Migration helpers
    void loadOriginalFolderArray(obs_data_array_t *folder_array, QStandardItem &parent);

    // DigitOtter-style scene tracking
    using source_map_t = std::map<obs_weak_source_t*, QStandardItem*>;
    source_map_t m_scenesInTree;

    CanvasType m_canvasType;

signals:
    void modelChanged();
};

// Custom tree view with enhanced drag & drop
class SceneTreeView : public QTreeView {
    Q_OBJECT

public:
    explicit SceneTreeView(QWidget *parent = nullptr);

protected:
    // Draws the vertical guides that show which folder a row belongs to, then
    // lets the base class put the expand/collapse chevron on top.
    void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;
    // Prunes a selection that holds both a folder and its contents before the
    // drag begins, so the view and the drag payload describe the same rows.
    void startDrag(Qt::DropActions supportedActions) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    // The view fills the whole row with the theme's selection colour before the
    // delegate runs. Rows the delegate colours itself opt out of that fill.
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;

private:
    void setupView();
};

// Standard item types for the tree
class SceneFolderItem : public QStandardItem {
public:
    explicit SceneFolderItem(const QString &folderName);

    int type() const override { return UserType + 1; }
    bool isFolder() const { return true; }

    // Timestamp management
    qint64 getCreationTimestamp() const;
    void setCreationTimestamp(qint64 timestamp);

public:
    void updateIcon();

private:
    void setupFolderItem();
};

class SceneTreeItem : public QStandardItem {
public:
    explicit SceneTreeItem(const QString &sceneName, obs_weak_source_t *weak_source);
    ~SceneTreeItem();

    int type() const override { return UserType + 2; }
    bool isScene() const { return true; }

    obs_weak_source_t* getWeakSource() const { return m_weakSource; }
    void updateIcon();

    // Timestamp management
    qint64 getCreationTimestamp() const;
    void setCreationTimestamp(qint64 timestamp);

private:
    void setupSceneItem();
    void updateFromObs();

    obs_weak_source_t* m_weakSource;
};

// Paints the flat Favourites / Recent rows exactly as CustomColorDelegate paints
// the tree: theme-owned selection and hover on a plain row, the same rounded
// pill for a hand-set colour, the same row height. The two delegates cannot be one class because they
// read from different models (a tree behind a proxy vs a plain list), but they
// share every colour and metric decision through the dock.
class QuickListDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit QuickListDelegate(SceneOrganiserDock *dock, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    SceneOrganiserDock *m_dock;
};

// Custom delegate to paint background colors (overrides theme stylesheet)
class CustomColorDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    // The proxy/model pair is passed in so one delegate class can serve both the
    // Scenes tree and a tab's tree; passing none means the dock's Scenes pair.
    explicit CustomColorDelegate(SceneOrganiserDock *dock, QObject *parent = nullptr,
                                 QSortFilterProxyModel *proxy = nullptr,
                                 QStandardItemModel *model = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    SceneOrganiserDock *m_dock;
    QSortFilterProxyModel *m_proxy;
    QStandardItemModel *m_model;
};

} // namespace SceneOrganiser
} // namespace StreamUP
