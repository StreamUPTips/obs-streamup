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
#include <map>
#include <obs.h>
#include <obs-frontend-api.h>

namespace StreamUP {
namespace SceneOrganiser {

class SceneTreeModel;
class SceneTreeView;

enum class CanvasType {
    Normal,
    Vertical
};

class SceneOrganiserDock : public QFrame {
    Q_OBJECT

public:
    explicit SceneOrganiserDock(CanvasType canvasType, QWidget *parent = nullptr);
    ~SceneOrganiserDock();

    CanvasType GetCanvasType() const { return m_canvasType; }

    static bool IsVerticalPluginDetected();
    static void NotifyAllDocksSettingsChanged();

    void SaveConfiguration();
    void LoadConfiguration();

private slots:
    void onSceneSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void onItemDoubleClicked(const QModelIndex &index);
    void onCustomContextMenuRequested(const QPoint &pos);
    void onAddFolderClicked();
    void onRefreshClicked();
    void onSettingsChanged();

public slots:
    static void onFrontendEvent(enum obs_frontend_event event, void *private_data);

private:
    void setupUI();
    void setupContextMenu();
    void setupObsSignals();
    void refreshSceneList();
    void updateFromObsScenes();
    void showFolderContextMenu(const QPoint &pos, const QModelIndex &index);
    void showSceneContextMenu(const QPoint &pos, const QModelIndex &index);
    void showBackgroundContextMenu(const QPoint &pos);

    // Data members
    CanvasType m_canvasType;
    QVBoxLayout *m_mainLayout;
    SceneTreeView *m_treeView;
    SceneTreeModel *m_model;
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_addFolderButton;
    QPushButton *m_refreshButton;

    // Context menus
    QMenu *m_folderContextMenu;
    QMenu *m_sceneContextMenu;
    QMenu *m_backgroundContextMenu;

    // Configuration
    QString m_configKey;
    QTimer *m_saveTimer;

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

    // Cleanup
    void cleanupEmptyItems();

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
    void removeSceneFromTracking(obs_weak_source_t *weak_source);

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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void setupView();
};

// Standard item types for the tree
class SceneFolderItem : public QStandardItem {
public:
    explicit SceneFolderItem(const QString &folderName);

    int type() const override { return UserType + 1; }
    bool isFolder() const { return true; }

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

private:
    void setupSceneItem();
    void updateFromObs();

    obs_weak_source_t* m_weakSource;
};

} // namespace SceneOrganiser
} // namespace StreamUP