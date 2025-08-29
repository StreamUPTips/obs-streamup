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
    void onAddSeparator();
    void onAddCustomSpacer();
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

private:
    void setupUI();
    void populateBuiltinButtonsList();
    void populateDockButtonsList();
    void populateCurrentConfiguration();
    void updateButtonStates();
    void clearSpacerForm();
    
    QListWidgetItem* createConfigurationItem(std::shared_ptr<ToolbarConfig::ToolbarItem> item);
    std::shared_ptr<ToolbarConfig::ToolbarItem> createItemFromSelection();
    
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
    
    QSpinBox* spacerSizeSpinBox;
    QPushButton* addCustomSpacerButton;
    
    QPushButton* addSeparatorButton;
    
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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    int dragStartIndex;
};

} // namespace StreamUP