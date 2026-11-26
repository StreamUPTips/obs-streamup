#include "hotkey-button-config-dialog.hpp"
#include "hotkey-selector-dialog.hpp"
#include "icon-selector-dialog.hpp"
#include "ui-helpers.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/section-card.hpp>
#include "version.h"
#include <QMessageBox>
#include <QUuid>
#include <QFileInfo>
#include <obs-module.h>

using namespace StreamUP::UIStyles;

namespace StreamUP {

HotkeyButtonConfigDialog::HotkeyButtonConfigDialog(QWidget* parent)
    : ShadowDialog(parent)
    , isEditMode(false)
{
    WindowShell chrome = applyChrome(this, obs_module_text("HotkeyButton.Dialog.AddTitle"),
                                     "v" PROJECT_VERSION, /*brandFooter=*/false, "StreamUP");
    chrome.content->setContentsMargins(S(20), S(16), S(20), S(16));
    mainLayout = chrome.content;
    footerButtons = chrome.footerButtons;
    setModal(true);
    resize(S(540) + 2 * S(ShadowDialog::kShadowMargin), S(470) + 2 * S(ShadowDialog::kShadowMargin));

    setupUI();
    validateInput();
}

HotkeyButtonConfigDialog::HotkeyButtonConfigDialog(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> existingItem, QWidget* parent)
    : ShadowDialog(parent)
    , isEditMode(true)
{
    WindowShell chrome = applyChrome(this, obs_module_text("HotkeyButton.Dialog.EditTitle"),
                                     "v" PROJECT_VERSION, /*brandFooter=*/false, "StreamUP");
    chrome.content->setContentsMargins(S(20), S(16), S(20), S(16));
    mainLayout = chrome.content;
    footerButtons = chrome.footerButtons;
    setModal(true);
    resize(S(540) + 2 * S(ShadowDialog::kShadowMargin), S(470) + 2 * S(ShadowDialog::kShadowMargin));

    setupUI();
    setExistingItem(existingItem);
    validateInput();
}

void HotkeyButtonConfigDialog::setupUI() {
    mainLayout->setSpacing(S(12));

    // 1. Which hotkey: what is chosen, with the way to choose it.
    QVBoxLayout *hotkeyCard = sectionCard(mainLayout, obs_module_text("HotkeyButton.Group.Hotkey"));
    hotkeyGroup = new QWidget(this);
    auto *hotkeyRow = new QHBoxLayout(hotkeyGroup);
    hotkeyRow->setContentsMargins(0, S(4), 0, S(2));
    hotkeyRow->setSpacing(S(12));
    auto *hotkeyText = new QVBoxLayout();
    hotkeyText->setSpacing(S(2));
    selectedHotkeyLabel = makeLabel(obs_module_text("HotkeyButton.Message.NoSelection"), Type::Body, Type::Semibold,
                                    Colors::TEXT_PRIMARY);
    hotkeyDescriptionLabel = makeLabel(QString(), Type::Small, Type::Regular, Colors::TEXT_SECONDARY);
    hotkeyDescriptionLabel->setWordWrap(true);
    hotkeyText->addWidget(selectedHotkeyLabel);
    hotkeyText->addWidget(hotkeyDescriptionLabel);
    hotkeyRow->addLayout(hotkeyText, 1);
    selectHotkeyButton = new PillButton(obs_module_text("HotkeyButton.Button.SelectHotkey"), "neutral");
    hotkeyRow->addWidget(selectHotkeyButton, 0, Qt::AlignVCenter);
    hotkeyCard->addWidget(hotkeyGroup);
    connect(selectHotkeyButton, &QPushButton::clicked, this, &HotkeyButtonConfigDialog::onSelectHotkeyClicked);

    // 2. The button: a live preview, its icon, its caption and its tooltip.
    QVBoxLayout *buttonCard = sectionCard(mainLayout, obs_module_text("HotkeyButton.Group.Button"));
    iconGroup = new QWidget(this);
    auto *previewRow = new QHBoxLayout(iconGroup);
    previewRow->setContentsMargins(0, S(6), 0, S(6));
    previewRow->setSpacing(S(12));
    m_preview = new StreamUP::Kit::ButtonPreview(iconGroup);
    previewRow->addWidget(m_preview, 1);
    selectIconButton = new PillButton(obs_module_text("HotkeyButton.Button.SelectIcon"), "neutral");
    previewRow->addWidget(selectIconButton, 0, Qt::AlignVCenter);
    buttonCard->addWidget(iconGroup);
    connect(selectIconButton, &QPushButton::clicked, this, &HotkeyButtonConfigDialog::onSelectIconClicked);
    // Kept for the code that still reports to it; the preview tile shows the icon.
    iconPreview = new QLabel(this);
    iconPreview->hide();

    buttonCard->addWidget(StreamUP::Kit::FieldRow(obs_module_text("HotkeyButton.Label.ButtonText"),
                                                  obs_module_text("HotkeyButton.Placeholder.ButtonText"), &buttonTextEdit));
    buttonCard->addWidget(StreamUP::Kit::FieldRow(obs_module_text("HotkeyButton.Label.Tooltip"),
                                                  obs_module_text("HotkeyButton.Placeholder.Tooltip"), &tooltipEdit));
    connect(buttonTextEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    connect(tooltipEdit, &QLineEdit::textChanged, this, [this]() { refreshPreview(); });
    mainLayout->addStretch(1);

    // Dialog buttons in footer (right-anchored action slot)
    okButton = new PillButton(
        isEditMode ? obs_module_text("HotkeyButton.Button.Update") : obs_module_text("HotkeyButton.Button.Add"),
        "primary");
    cancelButton = new PillButton(obs_module_text("UI.Button.Cancel"), "neutral");

    okButton->setDefault(true);
    okButton->setEnabled(false);

    footerButtons->addWidget(cancelButton);
    footerButtons->addWidget(okButton);

    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    refreshPreview();
    validateInput();
}

// The preview shows exactly what Add will make: the typed caption and tooltip,
// or what they fall back to when left empty.
void HotkeyButtonConfigDialog::refreshPreview() {
    if (!m_preview)
        return;
    const QString fallback = selectedHotkey.name.isEmpty() ? QString()
                                                           : StreamUP::OBSHotkeyManager::displayLabel(selectedHotkey);
    const QString caption = buttonTextEdit && !buttonTextEdit->text().trimmed().isEmpty()
                                ? buttonTextEdit->text().trimmed() : fallback;
    const QString tip = tooltipEdit && !tooltipEdit->text().trimmed().isEmpty() ? tooltipEdit->text().trimmed() : fallback;
    m_preview->setText(caption, tip);
    m_preview->setIconPath(selectedIconPath);
}

void HotkeyButtonConfigDialog::setExistingItem(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> item) {
    if (!item) return;
    
    originalItemId = item->id;
    
    // Set hotkey info
    selectedHotkey = StreamUP::HotkeyInfo();
    selectedHotkey.name = item->hotkeyName;
    selectedHotkey.description = item->displayName;
    selectedHotkey.context = item->hotkeyContext;
    
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
        updateIconDisplay();
        validateInput();
    }
}

void HotkeyButtonConfigDialog::onSelectIconClicked() {
    bool isCustomIcon = QFileInfo(selectedIconPath).isAbsolute();

    IconSelectorDialog dialog(
        isCustomIcon ? QString() : selectedIconPath,
        isCustomIcon ? selectedIconPath : QString(),
        isCustomIcon,
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
        hotkeyDescriptionLabel->setText(obs_module_text("HotkeyButton.Message.NoSelectionLine"));
    } else {
        // What it does, in words, first; the raw hotkey name under it.
        selectedHotkeyLabel->setText(StreamUP::OBSHotkeyManager::displayLabel(selectedHotkey));
        hotkeyDescriptionLabel->setText(selectedHotkey.name);

        // Auto-fill button text and tooltip if empty. The caption wants to be
        // short, so it takes the source name where there is one ("Main Cam"
        // beats "Switch to scene"); the tooltip spells out both.
        if (buttonTextEdit->text().isEmpty()) {
            buttonTextEdit->setText(selectedHotkey.context.isEmpty() ? selectedHotkey.description
                                                                     : selectedHotkey.context);
        }
        if (tooltipEdit->text().isEmpty()) {
            tooltipEdit->setText(StreamUP::OBSHotkeyManager::displayLabel(selectedHotkey));
        }
    }
    
    // Validate after updating display
    refreshPreview();
    validateInput();
}

void HotkeyButtonConfigDialog::updateIconDisplay() {
    // A hotkey with a default icon gets it until another is chosen.
    if (selectedIconPath.isEmpty() && !selectedHotkey.name.isEmpty())
        selectedIconPath = StreamUP::OBSHotkeyManager::getDefaultHotkeyIcon(selectedHotkey.name);
    refreshPreview();
    iconPreview->clear();
    iconPreview->setContentsMargins(S(6), 0, S(6), 0);

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
            iconPreview->setContentsMargins(0, 0, 0, 0);
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
                iconPreview->setContentsMargins(0, 0, 0, 0);
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
    item->hotkeyContext = selectedHotkey.context;
    
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
    
    const QString fallbackLabel = StreamUP::OBSHotkeyManager::displayLabel(selectedHotkey);
    item->displayName = buttonText.isEmpty() ? fallbackLabel : buttonText;
    item->tooltip = tooltip.isEmpty() ? fallbackLabel : tooltip;
    
    return item;
}

} // namespace StreamUP

#include "hotkey-button-config-dialog.moc"
