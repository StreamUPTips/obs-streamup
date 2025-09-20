#pragma once

#include <QDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QMetaType>
#include <QMenu>
#include <QAction>
#include "streamup-toolbar-config.hpp"

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

// Forward declarations
class StreamUPToolbar;

// Register the custom types with Qt's meta-object system
Q_DECLARE_METATYPE(std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem>)

namespace StreamUP {

class DraggableListWidget;

class ToolbarConfigurator : public QDialog {
    Q_OBJECT

public:
    explicit ToolbarConfigurator(QWidget *parent = nullptr);
    ~ToolbarConfigurator();

private slots:
    void onAddBuiltinButton();
    void onBuiltinItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddDockButton();
    void onDockItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddHotkeyButton();
    void onAddSeparator();
    void onAddCustomSpacer();
    void onAddGroup();
    void onRemoveItem();
    void onMoveUp();
    void onMoveDown();
    void onItemSelectionChanged();
    void onItemClicked(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onResetToDefault();
    void onSave();
    void onCancel();
    void onSpacerSettingsChanged();
    void onItemContextMenu(const QPoint& pos);
    void onMoveToGroup();

private:
    void setupUI();
    void populateBuiltinButtonsList();
    void populateDockButtonsList();
    void populateCurrentConfiguration();
    void updateButtonStates();
    void clearSpacerForm();
    void createExpandIndicator(QTreeWidget* treeWidget, QTreeWidgetItem* item);
    
    QListWidgetItem* createConfigurationItem(std::shared_ptr<ToolbarConfig::ToolbarItem> item, int indentLevel = 0);
    std::shared_ptr<ToolbarConfig::ToolbarItem> createItemFromSelection();
    void addItemToList(std::shared_ptr<ToolbarConfig::ToolbarItem> item, int indentLevel, std::shared_ptr<ToolbarConfig::GroupItem> parentGroup = nullptr, int positionInGroup = -1);
    
    // UI Components
    QSplitter* mainSplitter;
    
    // Left panel - Available items
    QWidget* leftPanel;
    QVBoxLayout* leftLayout;
    QTabWidget* itemTabWidget;
    
    QTreeWidget* builtinButtonsList;
    QPushButton* addBuiltinButton;
    
    QTreeWidget* dockButtonsList;
    QPushButton* addDockButton;
    
    QPushButton* addHotkeyButton;
    
    QSpinBox* spacerSizeSpinBox;
    QPushButton* addCustomSpacerButton;
    
    QPushButton* addSeparatorButton;
    
    QLineEdit* groupNameLineEdit;
    QPushButton* addGroupButton;
    
    // Right panel - Current configuration
    QWidget* rightPanel;
    QVBoxLayout* rightLayout;
    
    QLabel* configLabel;
    DraggableListWidget* currentConfigList;
    
    QHBoxLayout* configButtonsLayout;
    QPushButton* removeButton;
    QPushButton* moveUpButton;
    QPushButton* moveDownButton;
    QPushButton* resetButton;
    
    // Bottom buttons
    QHBoxLayout* bottomButtonsLayout;
    QPushButton* saveButton;
    QPushButton* cancelButton;
    
    // Data
    ToolbarConfig::ToolbarConfiguration config;
};

// Custom list widget that supports drag and drop reordering
class DraggableListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit DraggableListWidget(QWidget* parent = nullptr);

signals:
    void itemMoved(int from, int to);
    void itemMovedToGroup(int from, int groupIndex);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;
    void paintEvent(QPaintEvent* event) override;

private:
    int dragStartIndex;
    int dropIndicatorIndex;
    int groupDropIndex; // Index of group being hovered over (-1 if none)
    bool isGroupDrop; // True if we're dropping INTO a group
};

} // namespace StreamUP