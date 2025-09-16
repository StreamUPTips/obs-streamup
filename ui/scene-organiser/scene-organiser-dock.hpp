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

    // Drag & drop support
    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                     int row, int column, const QModelIndex &parent) override;

    // Scene management
    void refreshFromObs();
    QStandardItem *findSceneItem(const QString &sceneName);
    QStandardItem *findFolderItem(const QString &folderName);
    QStandardItem *createFolderItem(const QString &folderName);
    QStandardItem *createSceneItem(const QString &sceneName);
    void moveSceneToFolder(const QString &sceneName, QStandardItem *folderItem);

    // Configuration
    void saveToConfig(obs_data_t *config);
    void loadFromConfig(obs_data_t *config);

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

    CanvasType m_canvasType;
    QStringList m_validSceneNames;

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
    explicit SceneTreeItem(const QString &sceneName);

    int type() const override { return UserType + 2; }
    bool isScene() const { return true; }

private:
    void setupSceneItem();
    void updateFromObs();
};

} // namespace SceneOrganiser
} // namespace StreamUP