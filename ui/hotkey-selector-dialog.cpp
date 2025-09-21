#include "hotkey-selector-dialog.hpp"
#include "ui-styles.hpp"
#include <QHeaderView>
#include <QApplication>
#include <QDebug>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <obs-module.h>

namespace StreamUP {

HotkeySelectorDialog::HotkeySelectorDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(obs_module_text("HotkeySelector.Dialog.Title"));
    setModal(true);
    resize(800, 600);

    // Apply StreamUP dialog styling
    setStyleSheet(UIStyles::GetDialogStyle());

    setupUI();
    populateHotkeys();
}

void HotkeySelectorDialog::setupUI() {
    mainLayout = new QVBoxLayout(this);
    
    // Main splitter
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplitter);
    
    // Left panel - hotkey tree
    leftPanel = new QWidget();
    leftLayout = new QVBoxLayout(leftPanel);
    
    // Search box
    searchBox = new QLineEdit(leftPanel);
    searchBox->setPlaceholderText(obs_module_text("HotkeySelector.Placeholder.Search"));
    leftLayout->addWidget(searchBox);
    connect(searchBox, &QLineEdit::textChanged, this, &HotkeySelectorDialog::onSearchTextChanged);
    
    // Hotkey tree
    hotkeyTree = new QTreeWidget(leftPanel);
    hotkeyTree->setHeaderLabels({obs_module_text("HotkeySelector.Column.Hotkey"), obs_module_text("HotkeySelector.Column.Keys")});
    hotkeyTree->setColumnWidth(0, 300);
    hotkeyTree->setRootIsDecorated(true);
    hotkeyTree->setSortingEnabled(false);
    hotkeyTree->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(hotkeyTree);
    
    connect(hotkeyTree, &QTreeWidget::itemSelectionChanged, this, &HotkeySelectorDialog::onHotkeySelectionChanged);
    connect(hotkeyTree, &QTreeWidget::itemDoubleClicked, this, &HotkeySelectorDialog::onHotkeyItemDoubleClicked);
    
    mainSplitter->addWidget(leftPanel);
    
    // Right panel - details
    rightPanel = new QWidget();
    rightLayout = new QVBoxLayout(rightPanel);
    
    detailsGroup = new QGroupBox(obs_module_text("HotkeySelector.Group.Details"), rightPanel);
    detailsGroup->setStyleSheet(UIStyles::GetGroupBoxStyle("", ""));
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);
    
    selectedHotkeyName = new QLabel(obs_module_text("HotkeySelector.Message.NoSelection"), detailsGroup);
    selectedHotkeyName->setStyleSheet("font-weight: bold; font-size: 14px;");
    detailsLayout->addWidget(selectedHotkeyName);
    
    selectedHotkeyDescription = new QLabel("", detailsGroup);
    selectedHotkeyDescription->setWordWrap(true);
    detailsLayout->addWidget(selectedHotkeyDescription);
    
    selectedHotkeyKeys = new QLabel("", detailsGroup);
    selectedHotkeyKeys->setStyleSheet("color: blue; font-family: monospace;");
    detailsLayout->addWidget(selectedHotkeyKeys);
    
    selectedHotkeyHelp = new QTextEdit(detailsGroup);
    selectedHotkeyHelp->setMaximumHeight(100);
    selectedHotkeyHelp->setReadOnly(true);
    selectedHotkeyHelp->setPlainText(obs_module_text("HotkeySelector.Help.SelectHotkey"));
    detailsLayout->addWidget(selectedHotkeyHelp);
    
    rightLayout->addWidget(detailsGroup);
    rightLayout->addStretch();
    
    mainSplitter->addWidget(rightPanel);
    
    // Set splitter proportions
    mainSplitter->setStretchFactor(0, 2); // Left panel gets more space
    mainSplitter->setStretchFactor(1, 1); // Right panel gets less space
    
    // Dialog buttons
    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    okButton = new QPushButton(obs_module_text("HotkeySelector.Button.Add"), this);
    cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), this);

    okButton->setStyleSheet(UIStyles::GetButtonStyle());
    cancelButton->setStyleSheet(UIStyles::GetButtonStyle());

    okButton->setEnabled(false); // Disabled until selection is made
    
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void HotkeySelectorDialog::populateHotkeys() {
    categorizedHotkeys = StreamUP::OBSHotkeyManager::getCategorizedHotkeys();
    
    // Clear existing items
    hotkeyTree->clear();
    itemHotkeyMap.clear();
    allHotkeyItems.clear();
    
    // Build hierarchical tree structure
    QMap<QString, QTreeWidgetItem*> categoryMap;
    
    // Add categories and hotkeys
    for (auto it = categorizedHotkeys.constBegin(); it != categorizedHotkeys.constEnd(); ++it) {
        const QString& fullCategoryPath = it.key();
        const QList<StreamUP::HotkeyInfo>& hotkeys = it.value();
        
        if (hotkeys.isEmpty()) continue;
        
        // Parse hierarchical category path (e.g., "Sources › Audio › Microphone")
        QStringList categoryParts = fullCategoryPath.split(" › ");
        
        QTreeWidgetItem* currentParent = nullptr;
        QString currentPath = "";
        
        // Create hierarchy of category items
        for (int i = 0; i < categoryParts.size() - 1; ++i) { // -1 because last part is handled separately
            if (!currentPath.isEmpty()) {
                currentPath += " › ";
            }
            currentPath += categoryParts[i];
            
            if (!categoryMap.contains(currentPath)) {
                QTreeWidgetItem* categoryItem = createCategoryItem(categoryParts[i]);
                categoryMap[currentPath] = categoryItem;
                
                if (currentParent) {
                    currentParent->addChild(categoryItem);
                } else {
                    hotkeyTree->addTopLevelItem(categoryItem);
                }
                
                // Expand important top-level categories by default
                if (i == 0 && (categoryParts[i] == "General" || categoryParts[i] == "Sources")) {
                    categoryItem->setExpanded(true);
                }
            }
            
            currentParent = categoryMap[currentPath];
        }
        
        // Handle the final level (individual source or subcategory)
        if (categoryParts.size() > 1) {
            // Create final category item for the specific source/subcategory
            QString finalCategory = categoryParts.last();
            QTreeWidgetItem* finalCategoryItem = createCategoryItem(finalCategory);
            
            if (currentParent) {
                currentParent->addChild(finalCategoryItem);
            } else {
                hotkeyTree->addTopLevelItem(finalCategoryItem);
            }
            
            // Add hotkeys to the final category
            for (const auto& hotkey : hotkeys) {
                QTreeWidgetItem* hotkeyItem = createHotkeyItem(hotkey, finalCategoryItem);
                finalCategoryItem->addChild(hotkeyItem);
                allHotkeyItems.append(hotkeyItem);
            }
        } else {
            // Single-level category
            if (!categoryMap.contains(fullCategoryPath)) {
                QTreeWidgetItem* categoryItem = createCategoryItem(categoryParts[0]);
                categoryMap[fullCategoryPath] = categoryItem;
                hotkeyTree->addTopLevelItem(categoryItem);
                
                if (categoryParts[0] == "General" || categoryParts[0] == "Sources") {
                    categoryItem->setExpanded(true);
                }
            }
            
            QTreeWidgetItem* categoryItem = categoryMap[fullCategoryPath];
            for (const auto& hotkey : hotkeys) {
                QTreeWidgetItem* hotkeyItem = createHotkeyItem(hotkey, categoryItem);
                categoryItem->addChild(hotkeyItem);
                allHotkeyItems.append(hotkeyItem);
            }
        }
    }
    
    hotkeyTree->sortItems(0, Qt::AscendingOrder);
}

QTreeWidgetItem* HotkeySelectorDialog::createCategoryItem(const QString& categoryName) {
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, categoryName);
    item->setFlags(Qt::ItemIsEnabled); // Not selectable

    // Use theme-appropriate text color (no background color)
    item->setForeground(0, QApplication::palette().text().color());
    item->setForeground(1, QApplication::palette().text().color());

    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);
    item->setFont(1, font);

    return item;
}

QTreeWidgetItem* HotkeySelectorDialog::createHotkeyItem(const StreamUP::HotkeyInfo& hotkey, QTreeWidgetItem* parent) {
    Q_UNUSED(parent);
    
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, hotkey.description);
    item->setText(1, getHotkeyKeybinding(hotkey.name));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    // Ensure text is visible (use theme-appropriate colors)
    item->setForeground(0, QApplication::palette().text().color());
    item->setForeground(1, QApplication::palette().text().color());

    // Store hotkey info
    itemHotkeyMap[item] = hotkey;
    
    // Add tooltip with more details
    QString tooltip = QString(obs_module_text("HotkeySelector.Tooltip.HotkeyInfo"))
                      .arg(hotkey.name)
                      .arg(hotkey.description)
                      .arg(hotkey.registererType == OBS_HOTKEY_REGISTERER_FRONTEND ? obs_module_text("HotkeySelector.Type.Frontend") : obs_module_text("HotkeySelector.Type.Other"));
    item->setToolTip(0, tooltip);
    item->setToolTip(1, tooltip);
    
    return item;
}

QString HotkeySelectorDialog::getHotkeyKeybinding(const QString& hotkeyName) {
    (void)hotkeyName;
    // This is a simplified implementation - in practice you might want to
    // query OBS for actual key bindings, but that's quite complex
    return obs_module_text("HotkeySelector.Keys.NotBound");
}

void HotkeySelectorDialog::onSearchTextChanged(const QString& text) {
    filterHotkeys(text);
}

void HotkeySelectorDialog::filterHotkeys(const QString& searchText) {
    if (searchText.isEmpty()) {
        // Show all items
        for (QTreeWidgetItem* item : allHotkeyItems) {
            item->setHidden(false);
        }
        
        // Show/hide categories based on whether they have visible children
        for (int i = 0; i < hotkeyTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* categoryItem = hotkeyTree->topLevelItem(i);
            bool hasVisibleChildren = false;
            
            for (int j = 0; j < categoryItem->childCount(); ++j) {
                if (!categoryItem->child(j)->isHidden()) {
                    hasVisibleChildren = true;
                    break;
                }
            }
            
            categoryItem->setHidden(!hasVisibleChildren);
            if (hasVisibleChildren) {
                categoryItem->setExpanded(true);
            }
        }
    } else {
        QString lowerSearch = searchText.toLower();
        
        // Filter hotkey items
        for (QTreeWidgetItem* item : allHotkeyItems) {
            bool matches = false;
            
            if (itemHotkeyMap.contains(item)) {
                const StreamUP::HotkeyInfo& hotkey = itemHotkeyMap[item];
                matches = hotkey.name.toLower().contains(lowerSearch) ||
                         hotkey.description.toLower().contains(lowerSearch);
            }
            
            item->setHidden(!matches);
        }
        
        // Show/hide categories based on whether they have visible children
        for (int i = 0; i < hotkeyTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* categoryItem = hotkeyTree->topLevelItem(i);
            bool hasVisibleChildren = false;
            
            for (int j = 0; j < categoryItem->childCount(); ++j) {
                if (!categoryItem->child(j)->isHidden()) {
                    hasVisibleChildren = true;
                    break;
                }
            }
            
            categoryItem->setHidden(!hasVisibleChildren);
            if (hasVisibleChildren) {
                categoryItem->setExpanded(true);
            }
        }
    }
}

void HotkeySelectorDialog::onHotkeySelectionChanged() {
    QList<QTreeWidgetItem*> selectedItems = hotkeyTree->selectedItems();
    
    if (selectedItems.isEmpty() || !itemHotkeyMap.contains(selectedItems.first())) {
        selectedHotkey = StreamUP::HotkeyInfo(); // Clear selection
        okButton->setEnabled(false);
        selectedHotkeyName->setText(obs_module_text("HotkeySelector.Message.NoSelection"));
        selectedHotkeyDescription->clear();
        selectedHotkeyKeys->clear();
        selectedHotkeyHelp->setPlainText(obs_module_text("HotkeySelector.Help.SelectHotkey"));
        return;
    }
    
    selectedHotkey = itemHotkeyMap[selectedItems.first()];
    okButton->setEnabled(true);
    updateHotkeyDetails();
}

void HotkeySelectorDialog::updateHotkeyDetails() {
    selectedHotkeyName->setText(selectedHotkey.description);
    selectedHotkeyDescription->setText(QString(obs_module_text("HotkeySelector.Info.InternalName")).arg(selectedHotkey.name));
    selectedHotkeyKeys->setText(QString(obs_module_text("HotkeySelector.Info.Keys")).arg(getHotkeyKeybinding(selectedHotkey.name)));
    
    // Generate help text based on hotkey type
    QString helpText;
    if (selectedHotkey.name.contains("Stream")) {
        helpText = QString(obs_module_text("HotkeySelector.Help.Streaming"));
    } else if (selectedHotkey.name.contains("Record")) {
        helpText = QString(obs_module_text("HotkeySelector.Help.Recording"));
    } else if (selectedHotkey.name.contains("Replay")) {
        helpText = QString(obs_module_text("HotkeySelector.Help.ReplayBuffer"));
    } else if (selectedHotkey.name.contains("Virtual")) {
        helpText = QString(obs_module_text("HotkeySelector.Help.VirtualCamera"));
    } else if (selectedHotkey.name.contains("Scene") || selectedHotkey.name.contains("Transition")) {
        helpText = QString(obs_module_text("HotkeySelector.Help.Scenes"));
    } else {
        helpText = QString(obs_module_text("HotkeySelector.Help.General"));
    }
    
    selectedHotkeyHelp->setPlainText(helpText);
}

void HotkeySelectorDialog::onHotkeyItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    if (itemHotkeyMap.contains(item)) {
        accept(); // Double-click selects and accepts
    }
}

} // namespace StreamUP

#include "hotkey-selector-dialog.moc"