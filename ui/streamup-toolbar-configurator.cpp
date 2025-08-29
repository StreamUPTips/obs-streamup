#include "streamup-toolbar-configurator.hpp"
#include "settings-manager.hpp"
#include "ui-styles.hpp"
#include <QApplication>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QStandardPaths>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>
#include <QDateTime>
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
    
    setWindowTitle("Configure StreamUP Toolbar");
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
    builtinTabLayout->setContentsMargins(12, 12, 12, 12);
    
    builtinButtonsList = new QTreeWidget();
    builtinButtonsList->setHeaderHidden(true);
    builtinButtonsList->setRootIsDecorated(true);
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
        "QTreeWidget::branch { "
        "    background: transparent; "
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
    builtinTabLayout->addWidget(builtinButtonsList);
    
    addBuiltinButton = new QPushButton("Add Selected Button");
    addBuiltinButton->setEnabled(false);
    addBuiltinButton->setStyleSheet(UIStyles::GetButtonStyle());
    builtinTabLayout->addWidget(addBuiltinButton);
    
    itemTabWidget->addTab(builtinTab, "OBS");
    
    // === TAB 2: StreamUP Dock Buttons ===
    QWidget* dockTab = new QWidget();
    QVBoxLayout* dockTabLayout = new QVBoxLayout(dockTab);
    dockTabLayout->setContentsMargins(12, 12, 12, 12);
    
    dockButtonsList = new QTreeWidget();
    dockButtonsList->setHeaderHidden(true);
    dockButtonsList->setRootIsDecorated(true);
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
        "QTreeWidget::branch { "
        "    background: transparent; "
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
    dockTabLayout->addWidget(dockButtonsList);
    
    addDockButton = new QPushButton("Add Selected Dock Button");
    addDockButton->setEnabled(false);
    addDockButton->setStyleSheet(UIStyles::GetButtonStyle());
    dockTabLayout->addWidget(addDockButton);
    
    itemTabWidget->addTab(dockTab, "StreamUP");
    
    // === TAB 3: Spacers & Separators ===
    QWidget* spacerTab = new QWidget();
    QVBoxLayout* spacerTabLayout = new QVBoxLayout(spacerTab);
    spacerTabLayout->setContentsMargins(12, 12, 12, 12);
    
    // Custom spacer section
    QLabel* spacerLabel = new QLabel("Custom Spacer");
    spacerLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    spacerTabLayout->addWidget(spacerLabel);
    
    QHBoxLayout* sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel("Size (px):"));
    spacerSizeSpinBox = new QSpinBox();
    spacerSizeSpinBox->setRange(5, 200);
    spacerSizeSpinBox->setValue(20);
    spacerSizeSpinBox->setStyleSheet(UIStyles::GetSpinBoxStyle());
    sizeLayout->addWidget(spacerSizeSpinBox);
    sizeLayout->addStretch();
    spacerTabLayout->addLayout(sizeLayout);
    
    addCustomSpacerButton = new QPushButton("Add Custom Spacer");
    addCustomSpacerButton->setStyleSheet(UIStyles::GetButtonStyle());
    spacerTabLayout->addWidget(addCustomSpacerButton);
    
    spacerTabLayout->addSpacing(20);
    
    // Separator section
    QLabel* separatorLabel = new QLabel("Separator");
    separatorLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
    spacerTabLayout->addWidget(separatorLabel);
    
    addSeparatorButton = new QPushButton("Add Separator");
    addSeparatorButton->setStyleSheet(UIStyles::GetButtonStyle());
    spacerTabLayout->addWidget(addSeparatorButton);
    
    spacerTabLayout->addStretch();
    itemTabWidget->addTab(spacerTab, "Spacing");
    
    mainSplitter->addWidget(leftPanel);
    
    // === RIGHT PANEL: Current configuration ===
    rightPanel = new QWidget();
    rightPanel->setStyleSheet("QWidget { background-color: " + QString(StreamUP::UIStyles::Colors::BG_PRIMARY) + "; border: none; border-radius: 24px;}");
    rightLayout = new QVBoxLayout(rightPanel);
    
    configLabel = new QLabel("Current Toolbar Configuration:");
    configLabel->setStyleSheet(UIStyles::GetDescriptionLabelStyle());
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
    rightLayout->addWidget(currentConfigList);
    
    // Configuration control buttons
    configButtonsLayout = new QHBoxLayout();
    
    removeButton = new QPushButton("Remove");
    removeButton->setEnabled(false);
    removeButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(removeButton);
    
    moveUpButton = new QPushButton("Move Up");
    moveUpButton->setEnabled(false);
    moveUpButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(moveUpButton);
    
    moveDownButton = new QPushButton("Move Down");
    moveDownButton->setEnabled(false);
    moveDownButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(moveDownButton);
    
    configButtonsLayout->addStretch();
    
    resetButton = new QPushButton("Reset to Default");
    resetButton->setStyleSheet(UIStyles::GetButtonStyle());
    configButtonsLayout->addWidget(resetButton);
    
    rightLayout->addLayout(configButtonsLayout);
    mainSplitter->addWidget(rightPanel);
    
    // Set splitter proportions (narrower tab boxes, wider right panel)
    mainSplitter->setSizes({280, 420});
    
    // === BOTTOM BUTTONS ===
    bottomButtonsLayout = new QHBoxLayout();
    bottomButtonsLayout->addStretch();
    
    saveButton = new QPushButton("Save");
    saveButton->setDefault(true);
    saveButton->setStyleSheet(UIStyles::GetButtonStyle());
    bottomButtonsLayout->addWidget(saveButton);
    
    cancelButton = new QPushButton("Cancel");
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
    connect(currentConfigList, &DraggableListWidget::itemMoved, [this](int from, int to) {
        config.moveItem(from, to);
        populateCurrentConfiguration();
    });
    
    connect(addBuiltinButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddBuiltinButton);
    connect(addDockButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddDockButton);
    connect(addSeparatorButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddSeparator);
    connect(addCustomSpacerButton, &QPushButton::clicked, this, &ToolbarConfigurator::onAddCustomSpacer);
    
    connect(removeButton, &QPushButton::clicked, this, &ToolbarConfigurator::onRemoveItem);
    connect(moveUpButton, &QPushButton::clicked, this, &ToolbarConfigurator::onMoveUp);
    connect(moveDownButton, &QPushButton::clicked, this, &ToolbarConfigurator::onMoveDown);
    connect(resetButton, &QPushButton::clicked, this, &ToolbarConfigurator::onResetToDefault);
    
    connect(saveButton, &QPushButton::clicked, this, &ToolbarConfigurator::onSave);
    connect(cancelButton, &QPushButton::clicked, this, &ToolbarConfigurator::onCancel);
    
    // Spacer settings change tracking
    connect(spacerSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ToolbarConfigurator::onSpacerSettingsChanged);
}

void ToolbarConfigurator::populateBuiltinButtonsList()
{
    builtinButtonsList->clear();
    
    // Create Controls Dock category
    QTreeWidgetItem* controlsCategory = new QTreeWidgetItem(builtinButtonsList);
    controlsCategory->setText(0, "Controls Dock");
    controlsCategory->setExpanded(true);
    controlsCategory->setFlags(Qt::ItemIsEnabled); // Make category non-selectable
    
    auto buttons = ToolbarConfig::ButtonRegistry::getBuiltinButtons();
    for (const auto& button : buttons) {
        QTreeWidgetItem* item = new QTreeWidgetItem(controlsCategory);
        item->setText(0, button.displayName);
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
    
    auto buttons = ToolbarConfig::ToolbarConfiguration::getAvailableDockButtons();
    for (const auto& button : buttons) {
        // Determine which category this button belongs to
        QTreeWidgetItem* parentCategory = toolsCategory;
        if (button.dockButtonType.contains("settings") || button.dockButtonType.contains("config")) {
            parentCategory = settingsCategory;
        }
        
        QTreeWidgetItem* item = new QTreeWidgetItem(parentCategory);
        item->setText(0, button.name);
        item->setData(0, Qt::UserRole, button.dockButtonType);
        item->setToolTip(0, button.tooltip);
    }
}

void ToolbarConfigurator::populateCurrentConfiguration()
{
    currentConfigList->clear();
    
    for (const auto& configItem : config.items) {
        QListWidgetItem* item = createConfigurationItem(configItem);
        if (item) { // Only add if item was successfully created
            currentConfigList->addItem(item);
        }
    }
    
    updateButtonStates();
}

QListWidgetItem* ToolbarConfigurator::createConfigurationItem(std::shared_ptr<ToolbarConfig::ToolbarItem> item)
{
    // Skip items that shouldn't be displayed
    if (!item) {
        return nullptr;
    }
    
    QListWidgetItem* listItem = new QListWidgetItem();
    listItem->setData(Qt::UserRole, QVariant::fromValue(item));
    
    QString displayText;
    QString iconPath;
    
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
        
        displayText = QString("%1 %2").arg(enabledDot).arg(buttonDisplayName);
        break;
    }
    case ToolbarConfig::ItemType::Separator: {
        displayText = QString("%1 ━━━ Separator ━━━").arg(enabledDot);
        break;
    }
    case ToolbarConfig::ItemType::CustomSpacer: {
        auto spacerItem = std::static_pointer_cast<ToolbarConfig::CustomSpacerItem>(item);
        displayText = QString("%1 ↔️ Spacer (%2px) ↔️").arg(enabledDot).arg(spacerItem->size);
        break;
    }
    case ToolbarConfig::ItemType::DockButton: {
        auto dockItem = std::static_pointer_cast<ToolbarConfig::DockButtonItem>(item);
        // Special handling for StreamUP Settings - show without "(Dock)" suffix
        if (dockItem->dockButtonType == "streamup_settings") {
            displayText = QString("%1 %2").arg(enabledDot).arg(dockItem->name);
        } else {
            displayText = QString("%1 %2 (Dock)").arg(enabledDot).arg(dockItem->name);
        }
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
    int currentRow = currentConfigList->currentRow();
    if (currentRow > 0) {
        config.moveItem(currentRow, currentRow - 1);
        populateCurrentConfiguration();
        currentConfigList->setCurrentRow(currentRow - 1);
    }
}

void ToolbarConfigurator::onMoveDown()
{
    int currentRow = currentConfigList->currentRow();
    if (currentRow >= 0 && currentRow < currentConfigList->count() - 1) {
        config.moveItem(currentRow, currentRow + 1);
        populateCurrentConfiguration();
        currentConfigList->setCurrentRow(currentRow + 1);
    }
}

void ToolbarConfigurator::onItemSelectionChanged()
{
    updateButtonStates();
}

void ToolbarConfigurator::onItemClicked(QListWidgetItem* listItem)
{
    if (!listItem) return;
    
    // Get the click position relative to the item
    QPoint clickPos = currentConfigList->mapFromGlobal(QCursor::pos());
    QRect itemRect = currentConfigList->visualItemRect(listItem);
    
    // Check if click was in the blue dot area (first ~25 pixels)
    if (clickPos.x() - itemRect.x() <= 25) {
        auto item = listItem->data(Qt::UserRole).value<std::shared_ptr<ToolbarConfig::ToolbarItem>>();
        if (!item) return;
        
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
    int ret = QMessageBox::question(this, "Reset Configuration",
                                    "Are you sure you want to reset the toolbar to its default configuration? This will remove all custom buttons and separators.",
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

// DraggableListWidget implementation

DraggableListWidget::DraggableListWidget(QWidget* parent)
    : QListWidget(parent), dragStartIndex(-1)
{
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);
}

void DraggableListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->source() == this && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->source() == this && event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void DraggableListWidget::dropEvent(QDropEvent* event)
{
    if (event->source() != this) {
        event->ignore();
        return;
    }
    
    int dropIndex = indexAt(event->position().toPoint()).row();
    if (dropIndex < 0) {
        dropIndex = count();
    }
    
    if (dragStartIndex >= 0 && dragStartIndex != dropIndex) {
        // Adjust drop index if dropping below the dragged item
        if (dropIndex > dragStartIndex) {
            dropIndex--;
        }
        
        emit itemMoved(dragStartIndex, dropIndex);
    }
    
    dragStartIndex = -1;
    event->acceptProposedAction();
}

void DraggableListWidget::startDrag(Qt::DropActions supportedActions)
{
    QListWidgetItem* item = currentItem();
    if (item) {
        dragStartIndex = row(item);
        QListWidget::startDrag(supportedActions);
    }
}

} // namespace StreamUP

#include "streamup-toolbar-configurator.moc"
