#include "hotkey-button-config-dialog.hpp"
#include "hotkey-selector-dialog.hpp"
#include "icon-selector-dialog.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include <QMessageBox>
#include <QUuid>
#include <QFileInfo>
#include <obs-module.h>

namespace StreamUP {

HotkeyButtonConfigDialog::HotkeyButtonConfigDialog(QWidget* parent)
    : QDialog(parent)
    , isEditMode(false)
{
    setWindowTitle(obs_module_text("HotkeyButton.Dialog.AddTitle"));
    setModal(true);
    resize(500, 400);

    // Apply StreamUP dialog styling
    setStyleSheet(UIStyles::GetDialogStyle());

    setupUI();
    validateInput();
}

HotkeyButtonConfigDialog::HotkeyButtonConfigDialog(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> existingItem, QWidget* parent)
    : QDialog(parent)
    , isEditMode(true)
{
    setWindowTitle(obs_module_text("HotkeyButton.Dialog.EditTitle"));
    setModal(true);
    resize(500, 400);

    // Apply StreamUP dialog styling
    setStyleSheet(UIStyles::GetDialogStyle());

    setupUI();
    setExistingItem(existingItem);
    validateInput();
}

void HotkeyButtonConfigDialog::setupUI() {
    mainLayout = new QVBoxLayout(this);
    
    // Hotkey selection section
    hotkeyGroup = new QGroupBox(obs_module_text("HotkeyButton.Group.Hotkey"), this);
    hotkeyGroup->setStyleSheet(UIStyles::GetGroupBoxStyle("", ""));
    hotkeyFormLayout = new QFormLayout(hotkeyGroup);
    
    selectedHotkeyLabel = new QLabel(obs_module_text("HotkeyButton.Message.NoSelection"), hotkeyGroup);
    selectedHotkeyLabel->setStyleSheet("color: gray; font-style: italic;");
    
    hotkeyDescriptionLabel = new QLabel("", hotkeyGroup);
    hotkeyDescriptionLabel->setWordWrap(true);
    hotkeyDescriptionLabel->setStyleSheet("color: white;");
    
    selectHotkeyButton = new QPushButton(obs_module_text("HotkeyButton.Button.SelectHotkey"), hotkeyGroup);
    selectHotkeyButton->setStyleSheet(UIStyles::GetButtonStyle());
    
    hotkeyFormLayout->addRow(QString(obs_module_text("HotkeyButton.Label.Selected")), selectedHotkeyLabel);
    hotkeyFormLayout->addRow(QString(obs_module_text("HotkeyButton.Label.Description")), hotkeyDescriptionLabel);
    hotkeyFormLayout->addRow("", selectHotkeyButton);
    
    mainLayout->addWidget(hotkeyGroup);
    
    connect(selectHotkeyButton, &QPushButton::clicked, this, &HotkeyButtonConfigDialog::onSelectHotkeyClicked);
    
    // Icon selection section
    iconGroup = new QGroupBox(obs_module_text("HotkeyButton.Group.Icon"), this);
    iconGroup->setStyleSheet(UIStyles::GetGroupBoxStyle("", ""));
    iconLayout = new QVBoxLayout(iconGroup);

    // Icon preview section
    iconPreviewLayout = new QHBoxLayout();
    iconPreviewLabel = new QLabel(obs_module_text("HotkeyButton.Label.Preview"), iconGroup);
    iconPreview = new QLabel(iconGroup);
    iconPreview->setFixedSize(32, 32);
    iconPreview->setStyleSheet("border: 1px solid gray;");
    iconPreview->setAlignment(Qt::AlignCenter);
    iconPreview->setScaledContents(true);

    selectIconButton = new QPushButton(obs_module_text("HotkeyButton.Button.SelectIcon"), iconGroup);
    selectIconButton->setStyleSheet(UIStyles::GetButtonStyle());

    iconPreviewLayout->addWidget(iconPreviewLabel);
    iconPreviewLayout->addWidget(iconPreview);
    iconPreviewLayout->addWidget(selectIconButton);
    iconPreviewLayout->addStretch();

    iconLayout->addLayout(iconPreviewLayout);

    mainLayout->addWidget(iconGroup);

    connect(selectIconButton, &QPushButton::clicked, this, &HotkeyButtonConfigDialog::onSelectIconClicked);
    
    // Button customization section
    customizationGroup = new QGroupBox(obs_module_text("HotkeyButton.Group.Customization"), this);
    customizationGroup->setStyleSheet(UIStyles::GetGroupBoxStyle("", ""));
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

    okButton->setStyleSheet(UIStyles::GetButtonStyle());
    cancelButton->setStyleSheet(UIStyles::GetButtonStyle());

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
    
    // Set icon info - now just use whichever icon path is set
    if (!item->customIconPath.isEmpty()) {
        selectedIconPath = item->customIconPath;
    } else {
        selectedIconPath = item->iconPath;
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
    // Determine if current icon is custom (absolute path) or built-in (relative)
    bool isCustomIcon = QFileInfo(selectedIconPath).isAbsolute();

    IconSelectorDialog dialog(
        isCustomIcon ? QString() : selectedIconPath,  // currentIcon
        isCustomIcon ? selectedIconPath : QString(),  // currentCustomIcon
        isCustomIcon,                                  // useCustomIcon
        this
    );

    if (dialog.exec() == QDialog::Accepted) {
        QString newIconPath = dialog.getSelectedIcon();
        if (!newIconPath.isEmpty()) {
            selectedIconPath = newIconPath;
            updateIconDisplay();
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
        selectedHotkeyLabel->setStyleSheet("color: white; font-weight: bold;");
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
    iconPreview->clear();

    if (!selectedIconPath.isEmpty()) {
        // Try to load the selected icon
        QPixmap pixmap;

        // Check if it's a custom file path (absolute path) or built-in icon
        if (QFileInfo(selectedIconPath).isAbsolute() && QFileInfo(selectedIconPath).exists()) {
            // It's a custom icon file
            pixmap.load(selectedIconPath);
        } else {
            // It's a built-in icon, get the themed version
            QString themedPath = StreamUP::UIHelpers::GetThemedIconPath(selectedIconPath);
            pixmap.load(themedPath);
        }

        if (!pixmap.isNull()) {
            iconPreview->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            iconPreview->setText(obs_module_text("HotkeyButton.Message.Invalid"));
        }
    } else if (!selectedHotkey.name.isEmpty()) {
        // No icon selected, try to use default for this hotkey
        QString defaultIcon = StreamUP::OBSHotkeyManager::getDefaultHotkeyIcon(selectedHotkey.name);
        if (!defaultIcon.isEmpty()) {
            QString themedPath = StreamUP::UIHelpers::GetThemedIconPath(defaultIcon);
            QPixmap pixmap(themedPath);
            if (!pixmap.isNull()) {
                iconPreview->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                selectedIconPath = defaultIcon; // Update to use this default
            } else {
                iconPreview->setText(obs_module_text("HotkeyButton.Message.Default"));
            }
        } else {
            iconPreview->setText(obs_module_text("HotkeyButton.Message.NoIcon"));
        }
    } else {
        // No hotkey and no icon selected
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
    
    // Set icon configuration - determine if it's custom or built-in based on path
    if (QFileInfo(selectedIconPath).isAbsolute() && QFileInfo(selectedIconPath).exists()) {
        // It's a custom icon file
        item->useCustomIcon = true;
        item->customIconPath = selectedIconPath;
        item->iconPath = "";
    } else {
        // It's a built-in icon
        item->useCustomIcon = false;
        item->iconPath = selectedIconPath;
        item->customIconPath = "";
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