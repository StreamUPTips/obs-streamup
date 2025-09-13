#include "hotkey-button-config-dialog.hpp"
#include "hotkey-selector-dialog.hpp"
#include "icon-selector-dialog.hpp"
#include "ui-helpers.hpp"
#include <QMessageBox>
#include <QUuid>
#include <obs-module.h>

namespace StreamUP {

HotkeyButtonConfigDialog::HotkeyButtonConfigDialog(QWidget* parent)
    : QDialog(parent)
    , useCustomIcon(false)
    , isEditMode(false)
{
    setWindowTitle(obs_module_text("HotkeyButton.Dialog.AddTitle"));
    setModal(true);
    resize(500, 400);
    
    setupUI();
    validateInput();
}

HotkeyButtonConfigDialog::HotkeyButtonConfigDialog(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> existingItem, QWidget* parent)
    : QDialog(parent)
    , useCustomIcon(false)
    , isEditMode(true)
{
    setWindowTitle(obs_module_text("HotkeyButton.Dialog.EditTitle"));
    setModal(true);
    resize(500, 400);
    
    setupUI();
    setExistingItem(existingItem);
    validateInput();
}

void HotkeyButtonConfigDialog::setupUI() {
    mainLayout = new QVBoxLayout(this);
    
    // Hotkey selection section
    hotkeyGroup = new QGroupBox(obs_module_text("HotkeyButton.Group.Hotkey"), this);
    hotkeyFormLayout = new QFormLayout(hotkeyGroup);
    
    selectedHotkeyLabel = new QLabel(obs_module_text("HotkeyButton.Message.NoSelection"), hotkeyGroup);
    selectedHotkeyLabel->setStyleSheet("color: gray; font-style: italic;");
    
    hotkeyDescriptionLabel = new QLabel("", hotkeyGroup);
    hotkeyDescriptionLabel->setWordWrap(true);
    hotkeyDescriptionLabel->setStyleSheet("color: blue;");
    
    selectHotkeyButton = new QPushButton(obs_module_text("HotkeyButton.Button.SelectHotkey"), hotkeyGroup);
    
    hotkeyFormLayout->addRow(QString(obs_module_text("HotkeyButton.Label.Selected")), selectedHotkeyLabel);
    hotkeyFormLayout->addRow(QString(obs_module_text("HotkeyButton.Label.Description")), hotkeyDescriptionLabel);
    hotkeyFormLayout->addRow("", selectHotkeyButton);
    
    mainLayout->addWidget(hotkeyGroup);
    
    connect(selectHotkeyButton, &QPushButton::clicked, this, &HotkeyButtonConfigDialog::onSelectHotkeyClicked);
    
    // Icon selection section
    iconGroup = new QGroupBox(obs_module_text("HotkeyButton.Group.Icon"), this);
    iconLayout = new QVBoxLayout(iconGroup);
    
    iconTypeGroup = new QButtonGroup(this);
    
    useDefaultIconRadio = new QRadioButton(obs_module_text("HotkeyButton.Radio.DefaultIcon"), iconGroup);
    useCustomIconRadio = new QRadioButton(obs_module_text("HotkeyButton.Radio.CustomIcon"), iconGroup);
    
    iconTypeGroup->addButton(useDefaultIconRadio, 0);
    iconTypeGroup->addButton(useCustomIconRadio, 1);
    
    useDefaultIconRadio->setChecked(true); // Default selection
    
    iconLayout->addWidget(useDefaultIconRadio);
    iconLayout->addWidget(useCustomIconRadio);
    
    // Icon preview section
    iconPreviewLayout = new QHBoxLayout();
    iconPreviewLabel = new QLabel(obs_module_text("HotkeyButton.Label.Preview"), iconGroup);
    iconPreview = new QLabel(iconGroup);
    iconPreview->setFixedSize(32, 32);
    iconPreview->setStyleSheet("border: 1px solid gray; background: white;");
    iconPreview->setAlignment(Qt::AlignCenter);
    iconPreview->setScaledContents(true);
    
    selectIconButton = new QPushButton(obs_module_text("HotkeyButton.Button.SelectIcon"), iconGroup);
    selectIconButton->setEnabled(false); // Initially disabled
    
    iconPreviewLayout->addWidget(iconPreviewLabel);
    iconPreviewLayout->addWidget(iconPreview);
    iconPreviewLayout->addWidget(selectIconButton);
    iconPreviewLayout->addStretch();
    
    iconLayout->addLayout(iconPreviewLayout);
    
    mainLayout->addWidget(iconGroup);
    
    connect(useDefaultIconRadio, &QRadioButton::toggled, this, &HotkeyButtonConfigDialog::onUseDefaultIconToggled);
    connect(selectIconButton, &QPushButton::clicked, this, &HotkeyButtonConfigDialog::onSelectIconClicked);
    
    // Button customization section
    customizationGroup = new QGroupBox(obs_module_text("HotkeyButton.Group.Customization"), this);
    customizationLayout = new QFormLayout(customizationGroup);
    
    buttonTextEdit = new QLineEdit(customizationGroup);
    buttonTextEdit->setPlaceholderText(obs_module_text("HotkeyButton.Placeholder.ButtonText"));
    
    tooltipEdit = new QLineEdit(customizationGroup);
    tooltipEdit->setPlaceholderText(obs_module_text("HotkeyButton.Placeholder.Tooltip"));
    
    customizationLayout->addRow(obs_module_text("HotkeyButton.Label.ButtonText"), buttonTextEdit);
    customizationLayout->addRow(obs_module_text("HotkeyButton.Label.Tooltip"), tooltipEdit);
    
    mainLayout->addWidget(customizationGroup);
    
    // Dialog buttons
    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    okButton = new QPushButton(isEditMode ? obs_module_text("HotkeyButton.Button.Update") : obs_module_text("HotkeyButton.Button.Add"), this);
    cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), this);
    
    okButton->setDefault(true);
    okButton->setEnabled(false); // Initially disabled until hotkey is selected
    
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // Initial validation
    validateInput();
}

void HotkeyButtonConfigDialog::setExistingItem(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> item) {
    if (!item) return;
    
    originalItemId = item->id;
    
    // Set hotkey info
    selectedHotkey = StreamUP::HotkeyInfo();
    selectedHotkey.name = item->hotkeyName;
    selectedHotkey.description = item->displayName;
    
    updateHotkeyDisplay();
    
    // Set icon info
    useCustomIcon = item->useCustomIcon;
    selectedIconPath = item->iconPath;
    selectedCustomIconPath = item->customIconPath;
    
    if (useCustomIcon) {
        useCustomIconRadio->setChecked(true);
    } else {
        useDefaultIconRadio->setChecked(true);
    }
    
    updateIconDisplay();
    
    // Set customization info
    buttonTextEdit->setText(item->displayName);
    tooltipEdit->setText(item->tooltip);
}

void HotkeyButtonConfigDialog::onSelectHotkeyClicked() {
    HotkeySelectorDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted && dialog.hasSelection()) {
        selectedHotkey = dialog.getSelectedHotkey();
        updateHotkeyDisplay();
        updateIconDisplay(); // Update default icon preview if using default
        validateInput();
    }
}

void HotkeyButtonConfigDialog::onSelectIconClicked() {
    IconSelectorDialog dialog(selectedIconPath, selectedCustomIconPath, useCustomIcon, this);
    if (dialog.exec() == QDialog::Accepted) {
        selectedIconPath = dialog.getSelectedIcon();
        selectedCustomIconPath = dialog.getSelectedCustomIcon();
        useCustomIcon = dialog.shouldUseCustomIcon();
        
        // Update radio buttons based on selection
        if (useCustomIcon) {
            useCustomIconRadio->setChecked(true);
        } else {
            useDefaultIconRadio->setChecked(true);
        }
        
        updateIconDisplay();
    }
}

void HotkeyButtonConfigDialog::onUseDefaultIconToggled(bool checked) {
    selectIconButton->setEnabled(!checked);
    
    if (checked) {
        useCustomIcon = false;
        // Show default icon for selected hotkey
        updateIconDisplay();
    } else {
        useCustomIcon = true;
        if (selectedCustomIconPath.isEmpty() && selectedIconPath.isEmpty()) {
            // No custom icon selected yet, show placeholder
            iconPreview->clear();
            iconPreview->setText(obs_module_text("HotkeyButton.Message.NoIcon"));
        }
    }
}

void HotkeyButtonConfigDialog::updateHotkeyDisplay() {
    if (selectedHotkey.name.isEmpty()) {
        selectedHotkeyLabel->setText(obs_module_text("HotkeyButton.Message.NoSelection"));
        selectedHotkeyLabel->setStyleSheet("color: gray; font-style: italic;");
        hotkeyDescriptionLabel->clear();
    } else {
        selectedHotkeyLabel->setText(selectedHotkey.name);
        selectedHotkeyLabel->setStyleSheet("color: black; font-weight: bold;");
        hotkeyDescriptionLabel->setText(selectedHotkey.description);
        
        // Auto-fill button text and tooltip if empty
        if (buttonTextEdit->text().isEmpty()) {
            buttonTextEdit->setText(selectedHotkey.description);
        }
        if (tooltipEdit->text().isEmpty()) {
            tooltipEdit->setText(selectedHotkey.description);
        }
    }
    
    // Validate after updating display
    validateInput();
}

void HotkeyButtonConfigDialog::updateIconDisplay() {
    if (useCustomIcon && !selectedCustomIconPath.isEmpty()) {
        // Show custom icon
        QPixmap pixmap(selectedCustomIconPath);
        if (!pixmap.isNull()) {
            iconPreview->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            iconPreview->setText(obs_module_text("HotkeyButton.Message.Invalid"));
        }
    } else if (!useCustomIcon && !selectedIconPath.isEmpty()) {
        // Show selected built-in icon
        QString themedPath = StreamUP::UIHelpers::GetThemedIconPath(selectedIconPath);
        QPixmap pixmap(themedPath);
        if (!pixmap.isNull()) {
            iconPreview->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            iconPreview->setText(obs_module_text("HotkeyButton.Message.NotFound"));
        }
    } else if (!useCustomIcon && !selectedHotkey.name.isEmpty()) {
        // Show default icon for this hotkey
        QString defaultIcon = StreamUP::OBSHotkeyManager::getDefaultHotkeyIcon(selectedHotkey.name);
        QString themedPath = StreamUP::UIHelpers::GetThemedIconPath(defaultIcon);
        QPixmap pixmap(themedPath);
        if (!pixmap.isNull()) {
            iconPreview->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            iconPreview->setText(obs_module_text("HotkeyButton.Message.Default"));
        }
        // Update selectedIconPath to the default
        selectedIconPath = defaultIcon;
    } else {
        // No icon to show
        iconPreview->clear();
        iconPreview->setText(obs_module_text("HotkeyButton.Message.NoIcon"));
    }
}

void HotkeyButtonConfigDialog::validateInput() {
    bool hasHotkey = !selectedHotkey.name.isEmpty();
    okButton->setEnabled(hasHotkey);
    
    if (!hasHotkey) {
        okButton->setToolTip(obs_module_text("HotkeyButton.Tooltip.SelectHotkeyFirst"));
    } else {
        okButton->setToolTip("");
    }
}

std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> HotkeyButtonConfigDialog::getHotkeyButtonItem() const {
    if (selectedHotkey.name.isEmpty()) {
        return nullptr;
    }
    
    // Generate unique ID for new items
    QString itemId = isEditMode ? originalItemId : QString("hotkey_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    
    auto item = std::make_shared<StreamUP::ToolbarConfig::HotkeyButtonItem>(
        itemId, 
        selectedHotkey.name, 
        selectedHotkey.description
    );
    
    // Set icon configuration
    item->useCustomIcon = useCustomIcon;
    if (useCustomIcon) {
        item->customIconPath = selectedCustomIconPath;
        item->iconPath = ""; // Clear built-in icon path
    } else {
        item->iconPath = selectedIconPath;
        item->customIconPath = ""; // Clear custom icon path
    }
    
    // Set customization
    QString buttonText = buttonTextEdit->text().trimmed();
    QString tooltip = tooltipEdit->text().trimmed();
    
    item->displayName = buttonText.isEmpty() ? selectedHotkey.description : buttonText;
    item->tooltip = tooltip.isEmpty() ? selectedHotkey.description : tooltip;
    
    return item;
}

} // namespace StreamUP

#include "hotkey-button-config-dialog.moc"