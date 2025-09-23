#include "streamup-toolbar-configurator.hpp"
#include "../utilities/debug-logger.hpp"
#include "hotkey-button-config-dialog.hpp"
#include "settings-manager.hpp"
#include "ui-styles.hpp"
#include <obs-module.h>
#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardPaths>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>
#include <QDateTime>
#include <QPainter>
#include <QPolygon>
#include <QStyleOptionViewItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <obs-frontend-api.h>

namespace StreamUP {

ToolbarConfigurator::ToolbarConfigurator(QWidget *parent)
    : QDialog(parent)
{
    // Register custom types for QVariant
    qRegisterMetaType<std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem>>();
    
    setWindowTitle(obs_module_text("StreamUP.Toolbar.Configurator.Title"));
    setModal(true);
    resize(900, 650);
    
    // Apply StreamUP dialog styling
    setStyleSheet(UIStyles::GetDialogStyle());
    
    setupUI();
    
    // Load current configuration
    config.loadFromSettings();
    
    populateBuiltinButtonsList();
    populateDockButtonsList();
    populateCurrentConfiguration();
    updateButtonStates();
}

ToolbarConfigurator::~ToolbarConfigurator() = default;

void ToolbarConfigurator::setupUI()
{
    // Main layout with 12px margin
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);
    
    // Create splitter for left and right panels
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    // Style the splitter handle to use darkest color
    mainSplitter->setStyleSheet(
        "QSplitter::handle { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "    width: 2px; "
        "}"
    );
    mainLayout->addWidget(mainSplitter);
    
    // === LEFT PANEL: Available items ===
    leftPanel = new QWidget();
    leftPanel->setStyleSheet("QWidget { background-color: " + QString(StreamUP::UIStyles::Colors::BG_PRIMARY) + "; border: none; border-radius: 24px;}");
    leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 12, 12, 12); // Standard margins
    leftLayout->setSpacing(12);
    
    // Add heading to left panel
    QLabel* leftHeading = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AvailableItems")));
    leftHeading->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    leftLayout->addWidget(leftHeading);
    
    // Create tab widget
    itemTabWidget = new QTabWidget();
    // Style the tab widget with pill-shaped tabs and darkest color
    itemTabWidget->setStyleSheet(
        "QTabWidget::pane { "
        "    border: none; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_PRIMARY) + "; "
        "} "
        "QTabWidget::tab-bar { "
        "    left: 0px; "
        "} "
        "QTabBar::tab { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    border: none; "
        "    padding: 4px 8px; "
        "    margin-right: 8px; "
        "    margin-bottom: 0px; "
        "    border-radius: 10px; "
        "    min-width: 30px; "
        "} "
        "QTabBar::tab:selected { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "} "
        "QTabBar::tab:hover:!selected { "
				 "    background-color: " +
				 QString(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30) +
				 "; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "}"
    );
    leftLayout->addWidget(itemTabWidget);
    
    // === TAB 1: Built-in Buttons ===
    QWidget* builtinTab = new QWidget();
    QVBoxLayout* builtinTabLayout = new QVBoxLayout(builtinTab);
    builtinTabLayout->setContentsMargins(12, 12, 12, 12); // Standard margins
    builtinTabLayout->setSpacing(12);
    
    builtinButtonsList = new QTreeWidget();
    builtinButtonsList->setHeaderHidden(true);
    builtinButtonsList->setRootIsDecorated(true);
    builtinButtonsList->setIndentation(0); // No indentation - we'll handle it with styling
    builtinButtonsList->setStyleSheet(
        "QTreeWidget { "
        "    border: none; "
        "    border-radius: 12px; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    selection-background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    selection-color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    outline: none; "
        "    padding: 8px; "
        "    show-decoration-selected: 0; "
        "} "
        "QTreeWidget::item { "
        "    padding: 4px 8px; "
        "    border-radius: 6px; "
        "    margin: 1px; "
        "} "
        "QTreeWidget::item:hover { "
				      "    background-color: " +
				      QString(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30) +
				      "; "
        "} "
        "QTreeWidget::item:selected { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border: none; "
        "} "
        "QTreeWidget::item:selected:active { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border: none; "
        "} "
        "QTreeWidget::item:selected:!active { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border: none; "
        "} "
        "QTreeWidget::indicator { "
        "    width: 0px; "
        "    height: 0px; "
        "    border: none; "
        "    background: transparent; "
        "} "
        "QTreeWidget::indicator:checked, QTreeWidget::indicator:unchecked { "
        "    width: 0px; "
        "    height: 0px; "
        "    border: none; "
        "    background: transparent; "
        "} "
        "QTreeWidget::branch { "
        "    background: transparent; "
        "    width: 0px; "
        "    height: 0px; "
        "} "
        "QScrollBar:vertical { "
        "    background: " + QString(StreamUP::UIStyles::Colors::BG_SECONDARY) + "; "
        "    width: 6px; "
        "    border-radius: 3px; "
        "} "
        "QScrollBar::handle:vertical { "
        "    background: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border-radius: 3px; "
        "    min-height: 20px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "    background: " + QString(StreamUP::UIStyles::Colors::PRIMARY_HOVER) + "; "
        "} "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "    height: 0px; "
        "}"
    );
    builtinTabLayout->addWidget(builtinButtonsList, 1); // Give tree widget stretch priority
    
    // Add spacer to push button to bottom
    builtinTabLayout->addStretch(0);
    
    addBuiltinButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddSelectedButton")));
    addBuiltinButton->setEnabled(false);
    addBuiltinButton->setStyleSheet(UIStyles::GetButtonStyle());
    builtinTabLayout->addWidget(addBuiltinButton, 0); // Don't stretch button
    
    itemTabWidget->addTab(builtinTab, "OBS");
    
    // === TAB 2: StreamUP Dock Buttons ===
    QWidget* dockTab = new QWidget();
    QVBoxLayout* dockTabLayout = new QVBoxLayout(dockTab);
    dockTabLayout->setContentsMargins(12, 12, 12, 12); // Standard margins
    dockTabLayout->setSpacing(12);
    
    dockButtonsList = new QTreeWidget();
    dockButtonsList->setHeaderHidden(true);
    dockButtonsList->setRootIsDecorated(true);
    dockButtonsList->setIndentation(0); // No indentation - we'll handle it with styling
    dockButtonsList->setStyleSheet(
        "QTreeWidget { "
        "    border: none; "
        "    border-radius: 12px; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    selection-background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    selection-color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    outline: none; "
        "    padding: 8px; "
        "    show-decoration-selected: 0; "
        "} "
        "QTreeWidget::item { "
        "    padding: 4px 8px; "
        "    border-radius: 6px; "
        "    margin: 1px; "
        "} "
        "QTreeWidget::item:hover { "
				   "    background-color: " +
				   QString(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30) +
				   "; "
        "} "
        "QTreeWidget::item:selected { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border: none; "
        "} "
        "QTreeWidget::item:selected:active { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border: none; "
        "} "
        "QTreeWidget::item:selected:!active { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border: none; "
        "} "
        "QTreeWidget::indicator { "
        "    width: 0px; "
        "    height: 0px; "
        "    border: none; "
        "    background: transparent; "
        "} "
        "QTreeWidget::indicator:checked, QTreeWidget::indicator:unchecked { "
        "    width: 0px; "
        "    height: 0px; "
        "    border: none; "
        "    background: transparent; "
        "} "
        "QTreeWidget::branch { "
        "    background: transparent; "
        "    width: 0px; "
        "    height: 0px; "
        "} "
        "QScrollBar:vertical { "
        "    background: " + QString(StreamUP::UIStyles::Colors::BG_SECONDARY) + "; "
        "    width: 6px; "
        "    border-radius: 3px; "
        "} "
        "QScrollBar::handle:vertical { "
        "    background: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border-radius: 3px; "
        "    min-height: 20px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "    background: " + QString(StreamUP::UIStyles::Colors::PRIMARY_HOVER) + "; "
        "} "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "    height: 0px; "
        "}"
    );
    dockTabLayout->addWidget(dockButtonsList, 1); // Give tree widget stretch priority
    
    // Add spacer to push button to bottom
    dockTabLayout->addStretch(0);
    
    addDockButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddSelectedDockButton")));
    addDockButton->setEnabled(false);
    addDockButton->setStyleSheet(UIStyles::GetButtonStyle());
    dockTabLayout->addWidget(addDockButton, 0); // Don't stretch button
    
    itemTabWidget->addTab(dockTab, QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.StreamUPTab")));
    
    // === TAB 3: Hotkey Buttons ===
    QWidget* hotkeyTab = new QWidget();
    QVBoxLayout* hotkeyTabLayout = new QVBoxLayout(hotkeyTab);
    hotkeyTabLayout->setContentsMargins(12, 12, 12, 12); // Standard margins
    hotkeyTabLayout->setSpacing(12);
    
    // Create background container to match other tabs
    QWidget* hotkeyContainer = new QWidget();
    hotkeyContainer->setStyleSheet(
        "QWidget { "
        "    border: none; "
        "    border-radius: 12px; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "}"
    );
    QVBoxLayout* hotkeyContainerLayout = new QVBoxLayout(hotkeyContainer);
    hotkeyContainerLayout->setContentsMargins(8, 8, 8, 8);
    hotkeyContainerLayout->setSpacing(6);
    
    // Hotkey button section
    QLabel* hotkeyLabel = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.HotkeyButtons")));
    hotkeyLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    hotkeyContainerLayout->addWidget(hotkeyLabel);
    
    QLabel* hotkeyDescription = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.HotkeyDescription")));
    hotkeyDescription->setWordWrap(true);
    hotkeyDescription->setStyleSheet("QLabel { color: " + QString(StreamUP::UIStyles::Colors::TEXT_SECONDARY) + "; font-size: 12px; }");
    hotkeyContainerLayout->addWidget(hotkeyDescription);
    
    addHotkeyButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddHotkeyButton")));
    addHotkeyButton->setStyleSheet(UIStyles::GetButtonStyle());
    hotkeyContainerLayout->addWidget(addHotkeyButton);
    
    hotkeyContainerLayout->addStretch(); // Push content to top
    hotkeyTabLayout->addWidget(hotkeyContainer);
    hotkeyTabLayout->addStretch(); // Push content to top
    
    itemTabWidget->addTab(hotkeyTab, "Hotkeys");
    
    // === TAB 4: Spacers & Separators ===
    QWidget* spacerTab = new QWidget();
    QVBoxLayout* spacerTabLayout = new QVBoxLayout(spacerTab);
    spacerTabLayout->setContentsMargins(12, 12, 12, 12); // Standard margins  
    spacerTabLayout->setSpacing(0);
    
    // Create background container to match tree widgets
    QWidget* spacerContainer = new QWidget();
    spacerContainer->setStyleSheet(
        "QWidget { "
        "    border: none; "
        "    border-radius: 12px; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "}"
    );
    QVBoxLayout* spacerContainerLayout = new QVBoxLayout(spacerContainer);
    spacerContainerLayout->setContentsMargins(8, 8, 8, 8);
    spacerContainerLayout->setSpacing(6);
    
    // Custom spacer section
    QLabel* spacerLabel = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.CustomSpacer")));
    spacerLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    spacerContainerLayout->addWidget(spacerLabel);
    
    QHBoxLayout* sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.SizeLabel"))));
    spacerSizeSpinBox = new QSpinBox();
    spacerSizeSpinBox->setRange(5, 200);
    spacerSizeSpinBox->setValue(20);
    spacerSizeSpinBox->setStyleSheet(UIStyles::GetSpinBoxStyle());
    sizeLayout->addWidget(spacerSizeSpinBox);
    sizeLayout->addStretch();
    spacerContainerLayout->addLayout(sizeLayout);
    
    addCustomSpacerButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddCustomSpacer")));
    addCustomSpacerButton->setStyleSheet(UIStyles::GetButtonStyle());
    spacerContainerLayout->addWidget(addCustomSpacerButton);
    
    // Separator section
    QLabel* separatorLabel = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.Separator")));
    separatorLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    spacerContainerLayout->addWidget(separatorLabel);
    
    addSeparatorButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddSeparator")));
    addSeparatorButton->setStyleSheet(UIStyles::GetButtonStyle());
    spacerContainerLayout->addWidget(addSeparatorButton);
    
    // Group section
    QLabel* groupLabel = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.Group")));
    groupLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    spacerContainerLayout->addWidget(groupLabel);
    
    QHBoxLayout* groupLayout = new QHBoxLayout();
    groupLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.GroupName"))));
    groupNameLineEdit = new QLineEdit();
    groupNameLineEdit->setPlaceholderText("Enter group name");
    groupNameLineEdit->setStyleSheet(UIStyles::GetLineEditStyle());
    groupLayout->addWidget(groupNameLineEdit);
    spacerContainerLayout->addLayout(groupLayout);
    
    addGroupButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddGroup")));
    addGroupButton->setStyleSheet(UIStyles::GetButtonStyle());
    spacerContainerLayout->addWidget(addGroupButton);
    
    spacerTabLayout->addWidget(spacerContainer);
    spacerTabLayout->addStretch(); // Push content to top
    itemTabWidget->addTab(spacerTab, "Spacing");
    
    mainSplitter->addWidget(leftPanel);
    
    // === RIGHT PANEL: Current configuration ===
    rightPanel = new QWidget();
    rightPanel->setStyleSheet("QWidget { background-color: " + QString(StreamUP::UIStyles::Colors::BG_PRIMARY) + "; border: none; border-radius: 24px;}");
    rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(12, 12, 12, 12);
    rightLayout->setSpacing(12);
    
    configLabel = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.CurrentConfiguration")));
    configLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle() + "font-weight: bold;");
    rightLayout->addWidget(configLabel);
    
    currentConfigList = new DraggableListWidget();
    currentConfigList->setStyleSheet(
        "QListWidget { "
        "    border: none; "
        "    border-radius: 12px; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_DARKEST) + "; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    selection-background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    selection-color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "    outline: none; "
        "    padding: 8px; "
        "} "
        "QListWidget::item { "
        "    padding: 4px 8px; "
        "    margin: 1px; "
        "    border-radius: 6px; "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::BG_SECONDARY) + "; "
        "} "
        "QListWidget::item:selected { "
        "    background-color: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "} "
        "QListWidget::item:hover { "
				     "    background-color: " +
				     QString(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30) +
				     "; "
        "} "
        "QScrollBar:vertical { "
        "    background: " + QString(StreamUP::UIStyles::Colors::BG_SECONDARY) + "; "
        "    width: 6px; "
        "    border-radius: 3px; "
        "} "
        "QScrollBar::handle:vertical { "
        "    background: " + QString(StreamUP::UIStyles::Colors::PRIMARY_COLOR) + "; "
        "    border-radius: 3px; "
        "    min-height: 20px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "    background: " + QString(StreamUP::UIStyles::Colors::PRIMARY_HOVER) + "; "
        "} "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "    height: 0px; "
        "}"
    );
    rightLayout->addWidget(currentConfigList, 1); // Give list widget stretch priority
    
    // Add spacer to push buttons to bottom - matching left panel
    rightLayout->addStretch(0);
    
    // Configuration control buttons
    configButtonsLayout = new QHBoxLayout();
    
    removeButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.Remove")));
    removeButton->setEnabled(false);
    removeButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(removeButton);
    
    moveUpButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.MoveUp")));
    moveUpButton->setEnabled(false);
    moveUpButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(moveUpButton);
    
    moveDownButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.MoveDown")));
    moveDownButton->setEnabled(false);
    moveDownButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(moveDownButton);
    
    configButtonsLayout->addStretch();
    
    resetButton = new QPushButton(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.ResetToDefault")));
    resetButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(resetButton);
    
    rightLayout->addLayout(configButtonsLayout);
    mainSplitter->addWidget(rightPanel);
    
    // Set splitter proportions (narrower tab boxes, wider right panel)  
    mainSplitter->setSizes({280, 420});
    mainSplitter->setHandleWidth(12); // Add 12px gap between panels
    
    // === BOTTOM BUTTONS ===
    bottomButtonsLayout = new QHBoxLayout();
    bottomButtonsLayout->addStretch();
    
    saveButton = new QPushButton(QString::fromUtf8(obs_module_text("UI.Button.Save")));
    saveButton->setDefault(true);
    saveButton->setStyleSheet(UIStyles::GetButtonStyle());
    bottomButtonsLayout->addWidget(saveButton);
    
    cancelButton = new QPushButton(QString::fromUtf8(obs_module_text("UI.Button.Cancel")));
    cancelButton->setStyleSheet(UIStyles::GetButtonStyle());
    bottomButtonsLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(bottomButtonsLayout);
    
    // Connect signals
    connect(builtinButtonsList, &QTreeWidget::itemSelectionChanged, this, &ToolbarConfigurator::updateButtonStates);
    connect(builtinButtonsList, &QTreeWidget::itemDoubleClicked, this, &ToolbarConfigurator::onBuiltinItemDoubleClicked);
    connect(dockButtonsList, &QTreeWidget::itemSelectionChanged, this, &ToolbarConfigurator::updateButtonStates);
    connect(dockButtonsList, &QTreeWidget::itemDoubleClicked, this, &ToolbarConfigurator::onDockItemDoubleClicked);
    connect(currentConfigList, &QListWidget::itemSelectionChanged, this, &ToolbarConfigurator::onItemSelectionChanged);
    connect(currentConfigList, &QListWidget::itemDoubleClicked, this, &ToolbarConfigurator::onItemDoubleClicked);
    connect(currentConfigList, &QListWidget::itemClicked, this, &ToolbarConfigurator::onItemClicked);
    
    // Set up context menu for the configuration list
    currentConfigList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(currentConfigList, &QWidget::customContextMenuRequested, this, &ToolbarConfigurator::onItemContextMenu);
    
    // Connect drag-and-drop with proper ID-based indexing
    connect(currentConfigList, &DraggableListWidget::itemMoved, [this](int fromUIIndex, int toUIIndex) {
        StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "=== DRAG AND DROP START ===");
        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "From UI Index: %d, To UI Index: %d (already adjusted by DraggableListWidget)", fromUIIndex, toUIIndex);
        
        // Get the item being moved using the original fromUIIndex before any moves
        if (fromUIIndex < 0 || fromUIIndex >= currentConfigList->count()) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "Drag & Drop: Invalid fromUIIndex, aborting");
            return;
        }
        
        QListWidgetItem* draggedUIItem = currentConfigList->item(fromUIIndex);
        if (!draggedUIItem) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "Drag & Drop: No dragged UI item found, aborting");
            return;
        }
        
        auto draggedItem = draggedUIItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
        if (!draggedItem) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "Drag & Drop: No dragged item data found, aborting");
            return;
        }
        
        QString draggedItemId = draggedItem->id;
        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Dragged item: %s, Type: %d", draggedItemId.toUtf8().constData(), (int)draggedItem->type);
        
        // Find the actual config index of the dragged item
        int fromConfigIndex = config.getItemIndex(draggedItemId);
        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Main list config index: %d", fromConfigIndex);
        
        // If not found in main list, it might be inside a group
        bool draggedFromGroup = false;
        std::shared_ptr<ToolbarConfig::GroupItem> sourceGroup = nullptr;
        if (fromConfigIndex < 0) {
            StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Item not in main list, searching in groups...");
            // Search for the item in groups
            for (const auto& item : config.items) {
                if (item->type == ToolbarConfig::ItemType::Group) {
                    auto group = std::static_pointer_cast<ToolbarConfig::GroupItem>(item);
                    if (group->findChild(draggedItemId)) {
                        draggedFromGroup = true;
                        sourceGroup = group;
                        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Found item in group: %s", group->name.toUtf8().constData());
                        break;
                    }
                }
            }
            
            if (!draggedFromGroup) {
                StreamUP::DebugLogger::LogWarning("Toolbar", "Drag & Drop: Item not found anywhere, aborting");
                return; // Item not found anywhere
            }
        } else {
            StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Item found in main list at index: %d", fromConfigIndex);
        }
        
        // Handle special case: if target is a group, move the item into that group
        if (toUIIndex < currentConfigList->count()) {
            QListWidgetItem* targetUIItem = currentConfigList->item(toUIIndex);
            if (targetUIItem) {
                auto targetItem = targetUIItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
                if (targetItem && targetItem->type == ToolbarConfig::ItemType::Group) {
                    StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "TARGET IS GROUP - but check if we're dropping between group children instead");
                    
                    // Check if we're actually dropping onto a child item within the group
                    // by looking at the next item in the UI list
                    bool droppedOnChildItem = false;
                    int insertPosition = -1;
                    
                    if (toUIIndex + 1 < currentConfigList->count()) {
                        QListWidgetItem* nextUIItem = currentConfigList->item(toUIIndex + 1);
                        if (nextUIItem) {
                            auto nextParentGroup = nextUIItem->data(Qt::UserRole + 1).value<std::shared_ptr<ToolbarConfig::GroupItem>>();
                            if (nextParentGroup && nextParentGroup->id == targetItem->id) {
                                // The next item is a child of this group, so we're dropping at the beginning of the group
                                insertPosition = 0;
                                droppedOnChildItem = true;
                                StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Dropping at beginning of group");
                            }
                        }
                    }
                    
                    if (!droppedOnChildItem) {
                        StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Dropping into group (at end)");
                        // Normal drop into group - add at end
                        insertPosition = -1;
                    }
                    
                    auto targetGroup = std::static_pointer_cast<ToolbarConfig::GroupItem>(targetItem);
                    
                    // Remove the dragged item from its current location
                    if (draggedFromGroup) {
                        sourceGroup->removeChild(draggedItemId);
                    } else {
                        config.items.removeAt(fromConfigIndex);
                    }
                    
                    // Prevent groups from being placed within other groups (would cause crash)
                    if (draggedItem->type == ToolbarConfig::ItemType::Group) {
                        StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Cannot place group within another group - operation blocked");
                        return;
                    }

                    // Add the item to the target group at the appropriate position
                    if (insertPosition >= 0) {
                        targetGroup->childItems.insert(insertPosition, draggedItem);
                        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Inserted at position %d in group", insertPosition);
                    } else {
                        targetGroup->addChild(draggedItem);
                        StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Added to end of group");
                    }
                    
                    // Refresh the display and return early
                    populateCurrentConfiguration();
                    
                    // Expand the target group to show the newly added item
                    targetGroup->expanded = true;
                    populateCurrentConfiguration();
                    
                    StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Drop into group completed, returning early");
                    return;
                }
            }
        }
        
        // Calculate target config index only for main list operations
        int toConfigIndex = -1;
        if (!draggedFromGroup && toUIIndex < currentConfigList->count()) {
            // Only needed for main list to main list moves
            QListWidgetItem* targetUIItem = currentConfigList->item(toUIIndex);
            if (targetUIItem) {
                auto targetItem = targetUIItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
                if (targetItem) {
                    toConfigIndex = config.getItemIndex(targetItem->id);
                }
            }
        }
        
        // Get target item metadata to determine if we're moving within a group
        QListWidgetItem* targetUIItem = nullptr;
        if (toUIIndex < currentConfigList->count()) {
            targetUIItem = currentConfigList->item(toUIIndex);
            StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Target UI item found at index: %d", toUIIndex);
        } else {
            StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Target is beyond list end (dropping at end)");
        }
        
        std::shared_ptr<ToolbarConfig::GroupItem> targetParentGroup = nullptr;
        int targetPositionInGroup = -1;
        
        if (targetUIItem) {
            targetParentGroup = targetUIItem->data(Qt::UserRole + 1).value<std::shared_ptr<ToolbarConfig::GroupItem>>();
            targetPositionInGroup = targetUIItem->data(Qt::UserRole + 2).toInt();
            
            if (targetParentGroup) {
                StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Target is child of group: %s, position: %d",
                     targetParentGroup->name.toUtf8().constData(), targetPositionInGroup);
            } else {
                StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Target is in main list (no parent group)");
            }
            
            auto targetItemData = targetUIItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
            if (targetItemData) {
                StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Target item: %s, Type: %d",
                     targetItemData->id.toUtf8().constData(), (int)targetItemData->type);
            }
        }
        
        // Handle the move based on source and destination
        if (draggedFromGroup) {
            StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Source is from group: %s", sourceGroup->name.toUtf8().constData());
            
            // Check if we're moving within the same group
            if (targetParentGroup && targetParentGroup->id == sourceGroup->id) {
                StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "WITHIN-GROUP REORDERING detected");
                // Within-group reordering
                int sourcePositionInGroup = sourceGroup->getChildIndex(draggedItemId);
                StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Source position in group: %d, Target position: %d",
                     sourcePositionInGroup, targetPositionInGroup);
                
                if (sourcePositionInGroup >= 0 && targetPositionInGroup >= 0 && targetPositionInGroup != sourcePositionInGroup) {
                    // For within-group moves, we need to be careful about position calculation
                    // The target position from UI represents where we want to DROP the item
                    // But Qt's move logic may need adjustment depending on implementation
                    
                    int finalTargetPosition = targetPositionInGroup;
                    StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Raw positions - Source: %d, Target: %d", sourcePositionInGroup, targetPositionInGroup);
                    
                    // Don't adjust - let moveChild handle the logic correctly
                    // The UI position should be the final desired position
                    
                    StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Executing moveChild(%d, %d)", sourcePositionInGroup, finalTargetPosition);
                    sourceGroup->moveChild(sourcePositionInGroup, finalTargetPosition);
                    populateCurrentConfiguration();
                } else {
                    StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Within-group move conditions not met or same position");
                }
            } else {
                StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "CROSS-GROUP or GROUP-TO-MAIN move detected");
                // Moving out of group to main list or different group
                sourceGroup->removeChild(draggedItemId);
                
                if (targetParentGroup && targetParentGroup != sourceGroup) {
                    StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Moving to different group: %s", targetParentGroup->name.toUtf8().constData());
                    // Moving to a different group
                    if (targetPositionInGroup >= 0) {
                        targetParentGroup->childItems.insert(targetPositionInGroup, draggedItem);
                    } else {
                        targetParentGroup->addChild(draggedItem);
                    }
                } else {
                    StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "Moving to main list at config index: %d", toConfigIndex);
                    // Moving to main list
                    if (toUIIndex >= currentConfigList->count()) {
                        // Add to end of main list
                        config.items.append(draggedItem);
                    } else {
                        // Insert at the specified position in main list
                        config.items.insert(toConfigIndex, draggedItem);
                    }
                }
                
                populateCurrentConfiguration();
            }
        } else {
            StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Source is from MAIN LIST");
            
            // Check if we're moving from main list to a group
            if (targetParentGroup) {
                StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "MAIN-TO-GROUP move detected, target group: %s", targetParentGroup->name.toUtf8().constData());

                // Prevent groups from being placed within other groups (would cause crash)
                if (draggedItem->type == ToolbarConfig::ItemType::Group) {
                    StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Cannot place group within another group - operation blocked");
                    return;
                }

                // Moving from main list to a group
                config.items.removeAt(fromConfigIndex);
                if (targetPositionInGroup >= 0) {
                    targetParentGroup->childItems.insert(targetPositionInGroup, draggedItem);
                } else {
                    targetParentGroup->addChild(draggedItem);
                }
                populateCurrentConfiguration();
            } else {
                StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Drag & Drop", "MAIN LIST REORDERING from %d to %d", fromConfigIndex, toConfigIndex);
                // Normal move within main list
                if (fromConfigIndex != toConfigIndex) {
                    config.moveItem(fromConfigIndex, toConfigIndex);
                    populateCurrentConfiguration();
                } else {
                    StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "Same position, no move needed");
                }
            }
        }
        
        StreamUP::DebugLogger::LogDebug("Toolbar", "Drag & Drop", "=== DRAG AND DROP END ===");
        
        // Restore selection to moved item
        for (int i = 0; i < currentConfigList->count(); ++i) {
            QListWidgetItem* listItem = currentConfigList->item(i);
            auto listItemData = listItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
            if (listItemData && listItemData->id == draggedItemId) {
                currentConfigList->setCurrentRow(i);
                break;
            }
        }
    });

    // Handle dropping items INTO groups
    connect(currentConfigList, &DraggableListWidget::itemMovedToGroup, [this](int fromUIIndex, int groupUIIndex) {
        StreamUP::DebugLogger::LogDebug("Toolbar", "GroupDrop", "=== GROUP DROP START ===");
        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "GroupDrop", "From UI Index: %d, Group UI Index: %d", fromUIIndex, groupUIIndex);

        // Get the item being moved
        if (fromUIIndex < 0 || fromUIIndex >= currentConfigList->count()) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "GroupDrop: Invalid fromUIIndex, aborting");
            return;
        }

        QListWidgetItem* draggedUIItem = currentConfigList->item(fromUIIndex);
        if (!draggedUIItem) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "GroupDrop: No dragged UI item found, aborting");
            return;
        }

        auto draggedItem = draggedUIItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
        if (!draggedItem) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "GroupDrop: No dragged item data found, aborting");
            return;
        }

        // Get the target group
        if (groupUIIndex < 0 || groupUIIndex >= currentConfigList->count()) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "GroupDrop: Invalid groupUIIndex, aborting");
            return;
        }

        QListWidgetItem* groupUIItem = currentConfigList->item(groupUIIndex);
        if (!groupUIItem) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "GroupDrop: No group UI item found, aborting");
            return;
        }

        auto groupItem = groupUIItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
        if (!groupItem || groupItem->type != ToolbarConfig::ItemType::Group) {
            StreamUP::DebugLogger::LogWarning("Toolbar", "GroupDrop: Target is not a group, aborting");
            return;
        }

        auto group = std::static_pointer_cast<ToolbarConfig::GroupItem>(groupItem);

        StreamUP::DebugLogger::LogDebugFormat("Toolbar", "GroupDrop", "Moving item '%s' into group '%s'",
            draggedItem->id.toUtf8().constData(), group->name.toUtf8().constData());

        // Prevent groups from being placed within other groups (would cause crash)
        if (draggedItem->type == ToolbarConfig::ItemType::Group) {
            StreamUP::DebugLogger::LogDebug("Toolbar", "GroupDrop", "Cannot place group within another group - operation blocked");
            return;
        }

        // Remove item from its current location
        QString draggedItemId = draggedItem->id;
        config.removeItem(draggedItemId);

        // Add item to the group
        group->addChild(draggedItem);
        group->expanded = true; // Expand group to show the new item

        // Refresh the display
        populateCurrentConfiguration();

        StreamUP::DebugLogger::LogDebug("Toolbar", "GroupDrop", "=== GROUP DROP COMPLETE ===");
    });

    connect(addBuiltinButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddBuiltinButton);
    connect(addDockButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddDockButton);
    connect(addHotkeyButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddHotkeyButton);
    connect(addSeparatorButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddSeparator);
    connect(addCustomSpacerButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddCustomSpacer);
    connect(addGroupButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddGroup);
    
    connect(removeButton, &QPushButton::clicked, this, &ToolbarConfigurator::onRemoveItem);
    connect(moveUpButton, &QPushButton::clicked, this, &ToolbarConfigurator::onMoveUp);
    connect(moveDownButton, &QPushButton::clicked, this, &ToolbarConfigurator::onMoveDown);
    connect(resetButton, &QPushButton::clicked, this, &ToolbarConfigurator::onResetToDefault);
    
    connect(saveButton, &QPushButton::clicked, this, &ToolbarConfigurator::onSave);
    connect(cancelButton, &QPushButton::clicked, this, &ToolbarConfigurator::onCancel);
    
    // Spacer settings change tracking
    connect(spacerSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ToolbarConfigurator::onSpacerSettingsChanged);
}

void ToolbarConfigurator::createExpandIndicator(QTreeWidget* treeWidget, QTreeWidgetItem* item)
{
    // Create a container widget with horizontal layout
    QWidget* containerWidget = new QWidget();
    containerWidget->setStyleSheet("QWidget { background: transparent; }");
    
    QHBoxLayout* layout = new QHBoxLayout(containerWidget);
    layout->setContentsMargins(0, 0, 0, 0); // No margins needed with proper indentation
    layout->setSpacing(6); // Space between icon and text
    
    // Create the expand/collapse indicator
    QCheckBox* expandIndicator = new QCheckBox();
    expandIndicator->setProperty("class", "checkbox-icon indicator-expand");
    expandIndicator->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    expandIndicator->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    expandIndicator->setChecked(!item->isExpanded()); // Inverted: checked = collapsed
    expandIndicator->setStyleSheet(
        "QCheckBox { background: transparent; border: none; } "
        "QCheckBox:hover { background: transparent; }" // Disable hover effect
    );
    
    // Create a label for the text
    QLabel* textLabel = new QLabel(item->text(0));
    textLabel->setStyleSheet(
        "QLabel { "
        "    background: transparent; "
        "    color: " + QString(StreamUP::UIStyles::Colors::TEXT_PRIMARY) + "; "
        "}"
    );
    
    // Add widgets to layout
    layout->addWidget(expandIndicator);
    layout->addWidget(textLabel);
    layout->addStretch(); // Push everything to the left
    
    // Connect the checkbox to expand/collapse the item
    connect(expandIndicator, &QCheckBox::toggled, [item](bool checked) {
        item->setExpanded(!checked); // Inverted logic: checked = collapsed
    });
    
    // Also connect the item's expansion to update the checkbox
    connect(treeWidget, &QTreeWidget::itemExpanded, [expandIndicator, item](QTreeWidgetItem* expandedItem) {
        if (expandedItem == item) {
            expandIndicator->blockSignals(true);
            expandIndicator->setChecked(false); // Expanded = unchecked
            expandIndicator->blockSignals(false);
        }
    });
    
    connect(treeWidget, &QTreeWidget::itemCollapsed, [expandIndicator, item](QTreeWidgetItem* collapsedItem) {
        if (collapsedItem == item) {
            expandIndicator->blockSignals(true);
            expandIndicator->setChecked(true); // Collapsed = checked
            expandIndicator->blockSignals(false);
        }
    });
    
    // Clear the original text since we're using a custom widget
    item->setText(0, "");
    treeWidget->setItemWidget(item, 0, containerWidget);
}

void ToolbarConfigurator::populateBuiltinButtonsList()
{
    builtinButtonsList->clear();
    
    // Create Controls Dock category
    QTreeWidgetItem* controlsCategory = new QTreeWidgetItem(builtinButtonsList);
    controlsCategory->setText(0, "Controls Dock");
    controlsCategory->setExpanded(true);
    controlsCategory->setFlags(Qt::ItemIsEnabled); // Make category non-selectable
    
    // Create expand/collapse indicator for the category
    createExpandIndicator(builtinButtonsList, controlsCategory);
    
    auto buttons = ToolbarConfig::ButtonRegistry::getBuiltinButtons();
    for (const auto& button : buttons) {
        QTreeWidgetItem* item = new QTreeWidgetItem(controlsCategory);
        item->setText(0, "    " + button.displayName); // Add manual indentation
        item->setData(0, Qt::UserRole, button.type);
        item->setToolTip(0, button.defaultTooltip);
    }
}

void ToolbarConfigurator::populateDockButtonsList()
{
    dockButtonsList->clear();
    
    // Create StreamUP Tools category
    QTreeWidgetItem* toolsCategory = new QTreeWidgetItem(dockButtonsList);
    toolsCategory->setText(0, "StreamUP Tools");
    toolsCategory->setExpanded(true);
    toolsCategory->setFlags(Qt::ItemIsEnabled); // Make category non-selectable
    
    // Create StreamUP Settings category
    QTreeWidgetItem* settingsCategory = new QTreeWidgetItem(dockButtonsList);
    settingsCategory->setText(0, "StreamUP Settings");
    settingsCategory->setExpanded(true);
    settingsCategory->setFlags(Qt::ItemIsEnabled); // Make category non-selectable
    
    // Create expand/collapse indicators for both categories
    createExpandIndicator(dockButtonsList, toolsCategory);
    createExpandIndicator(dockButtonsList, settingsCategory);
    
    auto buttons = ToolbarConfig::ToolbarConfiguration::getAvailableDockButtons();
    for (const auto& button : buttons) {
        // Determine which category this button belongs to
        QTreeWidgetItem* parentCategory = toolsCategory;
        if (button.dockButtonType.contains("settings") || button.dockButtonType.contains("config")) {
            parentCategory = settingsCategory;
        }
        
        QTreeWidgetItem* item = new QTreeWidgetItem(parentCategory);
        item->setText(0, "    " + button.name); // Add manual indentation
        item->setData(0, Qt::UserRole, button.dockButtonType);
        item->setToolTip(0, button.tooltip);
    }
}

void ToolbarConfigurator::populateCurrentConfiguration()
{
    currentConfigList->clear();
    
    for (const auto& configItem : config.items) {
        addItemToList(configItem, 0); // Start with no indentation, no parent group
    }
    
    updateButtonStates();
}

void ToolbarConfigurator::addItemToList(std::shared_ptr<ToolbarConfig::ToolbarItem> item, int indentLevel, std::shared_ptr<ToolbarConfig::GroupItem> parentGroup, int positionInGroup)
{
    QListWidgetItem* listItem = createConfigurationItem(item, indentLevel);
    if (listItem) {
        // Store hierarchical metadata in the list item
        listItem->setData(Qt::UserRole + 1, QVariant::fromValue(parentGroup));
        listItem->setData(Qt::UserRole + 2, positionInGroup);
        
        // Debug logging for metadata storage
        if (parentGroup) {
            StreamUP::DebugLogger::LogDebugFormat("Toolbar", "Configuration", "Storing metadata for item %s: parent=%s, position=%d",
                 item->id.toUtf8().constData(), parentGroup->name.toUtf8().constData(), positionInGroup);
        }
        
        currentConfigList->addItem(listItem);
        
        // If this is a group and it's expanded, add its children
        if (item->type == ToolbarConfig::ItemType::Group) {
            auto groupItem = std::static_pointer_cast<ToolbarConfig::GroupItem>(item);
            if (groupItem->expanded) {
                for (int i = 0; i < groupItem->childItems.size(); ++i) {
                    addItemToList(groupItem->childItems[i], indentLevel + 1, groupItem, i);
                }
            }
        }
    }
}

QListWidgetItem* ToolbarConfigurator::createConfigurationItem(std::shared_ptr<ToolbarConfig::ToolbarItem> item, int indentLevel)
{
    // Skip items that shouldn't be displayed
    if (!item) {
        return nullptr;
    }
    
    QListWidgetItem* listItem = new QListWidgetItem();
    listItem->setData(Qt::UserRole, QVariant::fromValue(item));
    
    QString displayText;
    QString iconPath;
    
    // Add indentation for child items
    QString indent = QString("    ").repeated(indentLevel);
    
    // Add clickable blue dot for enable/disable
    QString enabledDot = item->visible ? "🔵" : "⚫";
    
    switch (item->type) {
    case ToolbarConfig::ItemType::Button: {
        auto buttonItem = std::static_pointer_cast<ToolbarConfig::ButtonItem>(item);
        auto buttonInfo = ToolbarConfig::ButtonRegistry::getButtonInfo(buttonItem->buttonType);
        
        // Handle case where button info is not found or display name is empty
        QString buttonDisplayName = buttonInfo.displayName;
        if (buttonDisplayName.isEmpty()) {
            // If it's a StreamUP Settings button that was removed from built-in buttons, hide it
            if (buttonItem->buttonType == "streamup_settings") {
                return nullptr; // Hide this item completely
            } 
            // Also hide pause and save_replay buttons that were removed
            else if (buttonItem->buttonType == "pause" || buttonItem->buttonType == "save_replay") {
                return nullptr; // Hide these items completely
            } else {
                // For other unknown buttons, use the button type
                buttonDisplayName = buttonItem->buttonType;
            }
        }
        
        displayText = QString("%1%2 %3").arg(indent).arg(enabledDot).arg(buttonDisplayName);
        break;
    }
    case ToolbarConfig::ItemType::Separator: {
        displayText = QString("%1%2 ━━━ Separator ━━━").arg(indent).arg(enabledDot);
        break;
    }
    case ToolbarConfig::ItemType::CustomSpacer: {
        auto spacerItem = std::static_pointer_cast<ToolbarConfig::CustomSpacerItem>(item);
        displayText = QString("%1%2 ↔️ Spacer (%3px) ↔️").arg(indent).arg(enabledDot).arg(spacerItem->size);
        break;
    }
    case ToolbarConfig::ItemType::DockButton: {
        auto dockItem = std::static_pointer_cast<ToolbarConfig::DockButtonItem>(item);
        // Special handling for StreamUP Settings - show without "(Dock)" suffix
        if (dockItem->dockButtonType == "streamup_settings") {
            displayText = QString("%1%2 %3").arg(indent).arg(enabledDot).arg(dockItem->name);
        } else {
            displayText = QString("%1%2 %3 (Dock)").arg(indent).arg(enabledDot).arg(dockItem->name);
        }
        break;
    }
    case ToolbarConfig::ItemType::Group: {
        auto groupItem = std::static_pointer_cast<ToolbarConfig::GroupItem>(item);
        QString expandIcon = groupItem->expanded ? "📂" : "📁";
        displayText = QString("%1%2 %3 (%4 items)")
                        .arg(indent)
                        .arg(expandIcon)
                        .arg(groupItem->name)
                        .arg(groupItem->childItems.size());
        break;
    }
    case ToolbarConfig::ItemType::HotkeyButton: {
        auto hotkeyItem = std::static_pointer_cast<ToolbarConfig::HotkeyButtonItem>(item);
        displayText = QString("%1%2 %3 (Hotkey)").arg(indent).arg(enabledDot).arg(hotkeyItem->displayName);
        break;
    }
    }
    
    listItem->setText(displayText);
    listItem->setFlags(listItem->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    
    return listItem;
}

void ToolbarConfigurator::updateButtonStates()
{
    // Update add buttons based on selections - only enable if a child item (not category) is selected
    QTreeWidgetItem* builtinSelected = builtinButtonsList->currentItem();
    addBuiltinButton->setEnabled(builtinSelected != nullptr && builtinSelected->parent() != nullptr);
    
    QTreeWidgetItem* dockSelected = dockButtonsList->currentItem();
    addDockButton->setEnabled(dockSelected != nullptr && dockSelected->parent() != nullptr);
    
    // Custom spacer button is always enabled (no validation needed)
    addCustomSpacerButton->setEnabled(true);
    
    // Update configuration manipulation buttons
    QListWidgetItem* selectedItem = currentConfigList->currentItem();
    bool hasSelection = selectedItem != nullptr;
    int selectedRow = currentConfigList->currentRow();
    
    removeButton->setEnabled(hasSelection);
    moveUpButton->setEnabled(hasSelection && selectedRow > 0);
    moveDownButton->setEnabled(hasSelection && selectedRow < currentConfigList->count() - 1);
}

void ToolbarConfigurator::onAddBuiltinButton()
{
    QTreeWidgetItem* selected = builtinButtonsList->currentItem();
    if (!selected || selected->parent() == nullptr) return; // Only allow child items, not categories
    
    QString buttonType = selected->data(0, Qt::UserRole).toString();
    auto buttonInfo = ToolbarConfig::ButtonRegistry::getButtonInfo(buttonType);
    
    // Generate unique ID
    QString id = QString("builtin_%1_%2").arg(buttonType).arg(QDateTime::currentMSecsSinceEpoch());
    
    auto buttonItem = std::make_shared<ToolbarConfig::ButtonItem>(id, buttonType);
    buttonItem->iconPath = buttonInfo.defaultIcon;
    buttonItem->tooltip = buttonInfo.defaultTooltip;
    buttonItem->checkable = buttonInfo.checkable;
    
    config.addItem(buttonItem);
    populateCurrentConfiguration();
}

void ToolbarConfigurator::onBuiltinItemDoubleClicked(QTreeWidgetItem* treeItem, int column)
{
    Q_UNUSED(column);
    if (!treeItem || treeItem->parent() == nullptr) return; // Only allow child items, not categories
    
    // Simulate the add builtin button click
    builtinButtonsList->setCurrentItem(treeItem);
    onAddBuiltinButton();
}

void ToolbarConfigurator::onAddDockButton()
{
    QTreeWidgetItem* selected = dockButtonsList->currentItem();
    if (!selected || selected->parent() == nullptr) return; // Only allow child items, not categories
    
    QString dockButtonType = selected->data(0, Qt::UserRole).toString();
    
    // Find the dock button info
    auto availableButtons = ToolbarConfig::ToolbarConfiguration::getAvailableDockButtons();
    for (const auto& button : availableButtons) {
        if (button.dockButtonType == dockButtonType) {
            // Generate unique ID
            QString id = QString("dock_%1_%2").arg(dockButtonType).arg(QDateTime::currentMSecsSinceEpoch());
            
            auto dockItem = std::make_shared<ToolbarConfig::DockButtonItem>(id, button.dockButtonType, button.name);
            dockItem->iconPath = button.iconPath;
            dockItem->tooltip = button.tooltip;
            
            config.addItem(dockItem);
            populateCurrentConfiguration();
            break;
        }
    }
}

void ToolbarConfigurator::onDockItemDoubleClicked(QTreeWidgetItem* treeItem, int column)
{
    Q_UNUSED(column);
    if (!treeItem || treeItem->parent() == nullptr) return; // Only allow child items, not categories
    
    // Simulate the add dock button click
    dockButtonsList->setCurrentItem(treeItem);
    onAddDockButton();
}

void ToolbarConfigurator::onAddSeparator()
{
    QString id = QString("sep_%1").arg(QDateTime::currentMSecsSinceEpoch());
    auto separatorItem = std::make_shared<ToolbarConfig::SeparatorItem>(id);
    
    config.addItem(separatorItem);
    populateCurrentConfiguration();
}

void ToolbarConfigurator::onAddCustomSpacer()
{
    QString id = QString("spacer_%1").arg(QDateTime::currentMSecsSinceEpoch());
    
    auto spacerItem = std::make_shared<ToolbarConfig::CustomSpacerItem>(id, spacerSizeSpinBox->value());
    spacerItem->isStretch = false; // Always false now
    
    config.addItem(spacerItem);
    populateCurrentConfiguration();
    
    // Reset form to defaults
    clearSpacerForm();
}

void ToolbarConfigurator::onAddGroup()
{
    QString groupName = groupNameLineEdit->text().trimmed();

    // If the line edit is empty (likely called from context menu), show input dialog
    if (groupName.isEmpty()) {
        bool ok;
        groupName = QInputDialog::getText(this,
                                          QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddGroup")),
                                          QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.GroupName")),
                                          QLineEdit::Normal,
                                          "",
                                          &ok);

        if (!ok || groupName.trimmed().isEmpty()) {
            return; // User cancelled or entered empty name
        }

        groupName = groupName.trimmed();
    }

    QString id = QString("group_%1").arg(QDateTime::currentMSecsSinceEpoch());
    auto groupItem = std::make_shared<ToolbarConfig::GroupItem>(id, groupName);

    config.addItem(groupItem);
    populateCurrentConfiguration();

    // Clear the form if it was used
    if (!groupNameLineEdit->text().isEmpty()) {
        groupNameLineEdit->clear();
    }
}

void ToolbarConfigurator::onRemoveItem()
{
    QListWidgetItem* selected = currentConfigList->currentItem();
    if (!selected) return;
    
    auto item = selected->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
    if (item) {
        config.removeItem(item->id);
        populateCurrentConfiguration();
    }
}

void ToolbarConfigurator::onMoveUp()
{
    QListWidgetItem* selectedItem = currentConfigList->currentItem();
    if (!selectedItem) return;
    
    // Get the item from the selected UI item
    auto item = selectedItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
    if (!item) return;
    
    QString itemId = item->id;
    
    // Find the actual config index of this item (not the UI row!)
    int configIndex = config.getItemIndex(itemId);
    if (configIndex <= 0) return; // Can't move up if it's the first item or not found
    
    config.moveItem(configIndex, configIndex - 1);
    populateCurrentConfiguration();
    
    // Find and select the moved item by ID
    for (int i = 0; i < currentConfigList->count(); ++i) {
        QListWidgetItem* listItem = currentConfigList->item(i);
        auto listItemData = listItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
        if (listItemData && listItemData->id == itemId) {
            currentConfigList->setCurrentRow(i);
            break;
        }
    }
}

void ToolbarConfigurator::onMoveDown()
{
    QListWidgetItem* selectedItem = currentConfigList->currentItem();
    if (!selectedItem) return;
    
    // Get the item from the selected UI item
    auto item = selectedItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
    if (!item) return;
    
    QString itemId = item->id;
    
    // Find the actual config index of this item (not the UI row!)
    int configIndex = config.getItemIndex(itemId);
    if (configIndex < 0 || configIndex >= config.items.size() - 1) return; // Can't move down if it's the last item or not found
    
    config.moveItem(configIndex, configIndex + 1);
    populateCurrentConfiguration();
    
    // Find and select the moved item by ID
    for (int i = 0; i < currentConfigList->count(); ++i) {
        QListWidgetItem* listItem = currentConfigList->item(i);
        auto listItemData = listItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
        if (listItemData && listItemData->id == itemId) {
            currentConfigList->setCurrentRow(i);
            break;
        }
    }
}

void ToolbarConfigurator::onItemSelectionChanged()
{
    updateButtonStates();
}

void ToolbarConfigurator::onItemClicked(QListWidgetItem* listItem)
{
    if (!listItem) return;
    
    auto item = listItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
    if (!item) return;
    
    // Get the click position relative to the item
    QPoint clickPos = currentConfigList->mapFromGlobal(QCursor::pos());
    QRect itemRect = currentConfigList->visualItemRect(listItem);
    
    // Check if this is a group and click was on the folder icon area (first ~50 pixels for groups)
    if (item->type == ToolbarConfig::ItemType::Group && 
        clickPos.x() - itemRect.x() <= 50) {
        auto groupItem = std::static_pointer_cast<ToolbarConfig::GroupItem>(item);
        // Toggle expanded state
        groupItem->expanded = !groupItem->expanded;
        
        // Refresh the display to show/hide child items
        populateCurrentConfiguration();
        
        // Restore selection to the group item
        for (int i = 0; i < currentConfigList->count(); ++i) {
            QListWidgetItem* checkItem = currentConfigList->item(i);
            auto checkData = checkItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
            if (checkData && checkData->id == item->id) {
                currentConfigList->setCurrentRow(i);
                break;
            }
        }
    }
    // Check if click was in the blue dot area (first ~25 pixels) for non-group items
    else if (item->type != ToolbarConfig::ItemType::Group && clickPos.x() - itemRect.x() <= 25) {
        // Toggle visibility
        item->visible = !item->visible;
        
        // Update the configuration and refresh the display
        populateCurrentConfiguration(); // Refresh the display to show new state
    }
}

void ToolbarConfigurator::onItemDoubleClicked(QListWidgetItem* listItem)
{
    if (!listItem) return;
    
    auto item = listItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
    if (!item) return;
    
    // Toggle visibility
    item->visible = !item->visible;
    
    // Update the configuration and refresh the display
    config.saveToSettings(); // Save the change immediately
    populateCurrentConfiguration(); // Refresh the display to show new state
}

void ToolbarConfigurator::onResetToDefault()
{
    int ret = QMessageBox::question(this, QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.ResetConfirmation")),
                                    QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.ResetMessage")),
                                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        config.setDefaultConfiguration();
        populateCurrentConfiguration();
    }
}


void ToolbarConfigurator::onSave()
{
    config.saveToSettings();
    accept(); // Close dialog with accepted result
}

void ToolbarConfigurator::onCancel()
{
    reject(); // Close dialog with rejected result
}



void ToolbarConfigurator::clearSpacerForm()
{
    spacerSizeSpinBox->setValue(20);
}

void ToolbarConfigurator::onSpacerSettingsChanged()
{
    // No validation needed for spacer settings - all values are valid
}

void ToolbarConfigurator::onItemContextMenu(const QPoint& pos)
{
    QMenu contextMenu(this);

    QListWidgetItem* item = currentConfigList->itemAt(pos);
    std::shared_ptr<ToolbarConfig::ToolbarItem> configItem = nullptr;

    if (item) {
        configItem = item->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
    }

    // Always add "Add Group" option
    QAction* addGroupAction = contextMenu.addAction(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.AddGroup")));
    connect(addGroupAction, &QAction::triggered, this, &ToolbarConfigurator::onAddGroup);

    // Only add item-specific options if we have a valid item
    if (configItem && configItem->type != ToolbarConfig::ItemType::Group) {
        // Check if there are any groups to move to
        QList<std::shared_ptr<ToolbarConfig::ToolbarItem>> availableGroups;
        for (const auto& item : config.items) {
            if (item->type == ToolbarConfig::ItemType::Group) {
                availableGroups.append(item);
            }
        }

        // Add separator before item-specific options
        if (!availableGroups.isEmpty()) {
            contextMenu.addSeparator();

            // Add "Move to Group" submenu
            QMenu* moveToGroupMenu = contextMenu.addMenu(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.MoveToGroup")));
            for (const auto& group : availableGroups) {
                auto groupItem = std::static_pointer_cast<ToolbarConfig::GroupItem>(group);
                QAction* action = moveToGroupMenu->addAction(groupItem->name);
                action->setData(groupItem->id);
                connect(action, &QAction::triggered, [this, configItem, groupItem]() {
            // Find and remove item from current location (could be in main list or another group)
            bool foundInMain = false;
            for (int i = 0; i < config.items.size(); ++i) {
                if (config.items[i]->id == configItem->id) {
                    config.items.removeAt(i);
                    foundInMain = true;
                    break;
                }
            }
            
            if (!foundInMain) {
                // Item must be in a group, find and remove it
                for (const auto& item : config.items) {
                    if (item->type == ToolbarConfig::ItemType::Group) {
                        auto group = std::static_pointer_cast<ToolbarConfig::GroupItem>(item);
                        if (group->findChild(configItem->id)) {
                            group->removeChild(configItem->id);
                            break;
                        }
                    }
                }
            }
            
            // Add to the target group
            groupItem->addChild(configItem);
            // Refresh the display
            populateCurrentConfiguration();
        });
            }
        }

        // Add "Move out of Group" if item is currently in a group
        bool itemInGroup = false;
        for (const auto& item : config.items) {
            if (item->type == ToolbarConfig::ItemType::Group) {
                auto group = std::static_pointer_cast<ToolbarConfig::GroupItem>(item);
                if (group->findChild(configItem->id)) {
                    itemInGroup = true;
                    break;
                }
            }
        }

        if (itemInGroup) {
            QAction* moveOutAction = contextMenu.addAction(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Configurator.MoveOutOfGroup")));
            connect(moveOutAction, &QAction::triggered, [this, configItem]() {
                config.moveItemOutOfGroup(configItem->id);
                populateCurrentConfiguration();
            });
        }
    }

    contextMenu.exec(currentConfigList->mapToGlobal(pos));
}

void ToolbarConfigurator::onAddHotkeyButton()
{
    // Open the hotkey button configuration dialog
    HotkeyButtonConfigDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto hotkeyItem = dialog.getHotkeyButtonItem();
        if (hotkeyItem) {
            config.addItem(hotkeyItem);
            populateCurrentConfiguration();
        }
    }
}

void ToolbarConfigurator::onMoveToGroup()
{
    // This method is called by context menu actions - implementation is inline above
}

// DraggableListWidget implementation

DraggableListWidget::DraggableListWidget(QWidget* parent)
    : QListWidget(parent), dragStartIndex(-1), dropIndicatorIndex(-1), groupDropIndex(-1), isGroupDrop(false)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDropIndicatorShown(true);
}

void DraggableListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->source() == this && event->mimeData()->hasFormat("application/x-streamup-toolbaritem")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->source() == this && event->mimeData()->hasFormat("application/x-streamup-toolbaritem")) {
        // Calculate drop position and update indicator
        QPoint pos = event->position().toPoint();
        QModelIndex index = indexAt(pos);
        
        // Reset group drop state
        groupDropIndex = -1;
        isGroupDrop = false;
        
        if (index.isValid()) {
            QRect rect = visualRect(index);
            QListWidgetItem* targetItem = item(index.row());
            
            if (targetItem) {
                auto itemData = targetItem->data(Qt::UserRole).value<std::shared_ptr<StreamUP::ToolbarConfig::ToolbarItem>>();
                
                // Check if we're hovering over a group
                if (itemData && itemData->type == StreamUP::ToolbarConfig::ItemType::Group) {
                    // Check if we're in the middle area of the group (not top/bottom edges)
                    int topEdge = rect.y() + 8;  // 8px from top
                    int bottomEdge = rect.y() + rect.height() - 8;  // 8px from bottom
                    
                    if (pos.y() > topEdge && pos.y() < bottomEdge) {
                        // We're dropping INTO the group
                        groupDropIndex = index.row();
                        isGroupDrop = true;
                        dropIndicatorIndex = -1; // Don't show line indicator
                    } else {
                        // We're dropping above/below the group
                        if (pos.y() > rect.center().y()) {
                            dropIndicatorIndex = index.row() + 1;
                        } else {
                            dropIndicatorIndex = index.row();
                        }
                    }
                } else {
                    // Normal item - use standard line indicator
                    if (pos.y() > rect.center().y()) {
                        dropIndicatorIndex = index.row() + 1;
                    } else {
                        dropIndicatorIndex = index.row();
                    }
                }
            }
        } else {
            // Dropping at the end
            dropIndicatorIndex = count();
        }
        
        // Trigger repaint to show drop indicator
        viewport()->update();
        
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    dropIndicatorIndex = -1;
    groupDropIndex = -1;
    isGroupDrop = false;
    viewport()->update(); // Clear drop indicator
    QListWidget::dragLeaveEvent(event);
}

void DraggableListWidget::dropEvent(QDropEvent* event)
{
    if (event->source() != this) {
        event->ignore();
        return;
    }

    if (dragStartIndex >= 0) {
        if (isGroupDrop && groupDropIndex >= 0) {
            // Dropping INTO a group
            StreamUP::DebugLogger::LogDebug("Toolbar", "DragDrop",
                QString("Dropping item %1 INTO group at index %2")
                .arg(dragStartIndex).arg(groupDropIndex).toUtf8().constData());

            emit itemMovedToGroup(dragStartIndex, groupDropIndex);
        } else {
            // Standard drop (not into a group)
            int dropIndex = dropIndicatorIndex;
            if (dropIndex < 0) {
                dropIndex = count();
            }

            if (dragStartIndex != dropIndex) {
                // Adjust drop index if dropping below the dragged item
                if (dropIndex > dragStartIndex) {
                    dropIndex--;
                }

                StreamUP::DebugLogger::LogDebug("Toolbar", "DragDrop",
                    QString("Standard drop: moving item %1 to position %2")
                    .arg(dragStartIndex).arg(dropIndex).toUtf8().constData());

                emit itemMoved(dragStartIndex, dropIndex);
            }
        }
    }

    dragStartIndex = -1;
    dropIndicatorIndex = -1;
    groupDropIndex = -1;
    isGroupDrop = false;
    viewport()->update(); // Clear drop indicator
    event->acceptProposedAction();
}

void DraggableListWidget::startDrag(Qt::DropActions supportedActions)
{
    QListWidgetItem* item = currentItem();
    if (!item) return;
    
    dragStartIndex = row(item);
    
    // Create a custom drag with transparent pixmap
    QDrag* drag = new QDrag(this);
    QMimeData* mimeData = new QMimeData();
    
    // Set up custom mime data to avoid QVariant serialization issues
    mimeData->setData("application/x-streamup-toolbaritem", QByteArray::number(dragStartIndex));
    
    // Create a transparent version of the item
    QRect itemRect = visualItemRect(item);
    QPixmap pixmap(itemRect.size());
    pixmap.fill(Qt::transparent);
    
    // Render the item onto the pixmap
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw the item with the widget's style
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, itemRect.width(), itemRect.height());
    option.state = QStyle::State_Selected;
    option.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    option.decorationAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    option.decorationSize = iconSize();
    option.font = font();
    option.fontMetrics = fontMetrics();
    option.palette = palette();
    option.text = item->text();
    option.icon = item->icon();
    
    // Draw the item normally first
    style()->drawControl(QStyle::CE_ItemViewItem, &option, &painter, this);
    
    // Apply transparency to the entire pixmap
    QPixmap transparentPixmap(pixmap.size());
    transparentPixmap.fill(Qt::transparent);
    QPainter transparentPainter(&transparentPixmap);
    transparentPainter.setOpacity(0.5); // 50% transparency
    transparentPainter.drawPixmap(0, 0, pixmap);
    
    drag->setMimeData(mimeData);
    drag->setPixmap(transparentPixmap);
    drag->setHotSpot(QPoint(transparentPixmap.width() / 2, transparentPixmap.height() / 2));
    
    // Start the drag operation
    drag->exec(supportedActions, Qt::MoveAction);
}

void DraggableListWidget::paintEvent(QPaintEvent* event)
{
    QListWidget::paintEvent(event);
    
    // Draw group drop indicator if we're dropping into a group
    if (isGroupDrop && groupDropIndex >= 0 && dragStartIndex >= 0) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        
        QRect groupRect = visualRect(model()->index(groupDropIndex, 0));
        
        // Draw a highlight box around the group
        painter.setPen(QPen(QColor("#0076df"), 3));
        painter.setBrush(QBrush(QColor("#0076df"), Qt::Dense6Pattern)); // Semi-transparent fill
        
        // Draw rounded rectangle around the group
        QRectF highlightRect = groupRect.adjusted(2, 2, -2, -2);
        painter.drawRoundedRect(highlightRect, 4, 4);
        
        // Draw "DROP INTO GROUP" text
        painter.setPen(QPen(QColor("#0076df"), 2));
        painter.setFont(QFont(font().family(), 8, QFont::Bold));
        QRect textRect = groupRect.adjusted(10, 0, -10, 0);
        painter.drawText(textRect, Qt::AlignCenter, "DROP INTO GROUP");
    }
    // Draw normal line drop indicator if we're not doing a group drop
    else if (dropIndicatorIndex >= 0 && dragStartIndex >= 0) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Use StreamUP primary color for the drop indicator
        painter.setPen(QPen(QColor("#0076df"), 2));
        painter.setBrush(QBrush(QColor("#0076df")));
        
        int y;
        if (dropIndicatorIndex == count()) {
            // Dropping at the end
            if (count() > 0) {
                QRect lastRect = visualRect(model()->index(count() - 1, 0));
                y = lastRect.bottom() + 1;
            } else {
                y = 1;
            }
        } else {
            // Dropping between items
            QRect rect = visualRect(model()->index(dropIndicatorIndex, 0));
            y = rect.top() - 1;
        }
        
        // Draw horizontal line as drop indicator
        painter.drawLine(5, y, width() - 10, y);
        
        // Draw small triangular indicators at both ends
        QPolygon leftArrow;
        leftArrow << QPoint(2, y) << QPoint(8, y - 3) << QPoint(8, y + 3);
        painter.drawPolygon(leftArrow);
        
        QPolygon rightArrow;
        rightArrow << QPoint(width() - 2, y) << QPoint(width() - 8, y - 3) << QPoint(width() - 8, y + 3);
        painter.drawPolygon(rightArrow);
    }
}

} // namespace StreamUP

#include "streamup-toolbar-configurator.moc"
