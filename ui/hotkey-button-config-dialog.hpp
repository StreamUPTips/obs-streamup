#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include "streamup-toolbar-config.hpp"
#include "obs-hotkey-manager.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>

namespace StreamUP {

class HotkeyButtonConfigDialog : public StreamUP::UIStyles::ShadowDialog {
    Q_OBJECT

public:
    explicit HotkeyButtonConfigDialog(QWidget* parent = nullptr);
    explicit HotkeyButtonConfigDialog(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> existingItem, QWidget* parent = nullptr);
    
    // Get the configured hotkey button item
    std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> getHotkeyButtonItem() const;
    
private slots:
    void onSelectHotkeyClicked();
    void onSelectIconClicked();
    void validateInput();

private:
    void setupUI();
    void updateHotkeyDisplay();
    void updateIconDisplay();
    void setExistingItem(std::shared_ptr<StreamUP::ToolbarConfig::HotkeyButtonItem> item);
    
    // UI Components
    QVBoxLayout* mainLayout;        // chrome.content
    QHBoxLayout* footerButtons;     // chrome.footerButtons (right-anchored action slot)
    
    // Hotkey selection section
    QWidget* hotkeyGroup;
    QFormLayout* hotkeyFormLayout;
    QLabel* selectedHotkeyLabel;
    QLabel* hotkeyDescriptionLabel;
    StreamUP::UIStyles::PillButton* selectHotkeyButton;

    // Icon selection section
    QWidget* iconGroup;
    QVBoxLayout* iconLayout;

    QHBoxLayout* iconPreviewLayout;
    QLabel* iconPreviewLabel;
    QLabel* iconPreview;
    StreamUP::UIStyles::PillButton* selectIconButton;

    // Button customization section
    QWidget* customizationGroup;
    QFormLayout* customizationLayout;
    QLineEdit* buttonTextEdit;
    QLineEdit* tooltipEdit;

    // Dialog buttons
    QHBoxLayout* buttonLayout;
    StreamUP::UIStyles::PillButton* okButton;
    StreamUP::UIStyles::PillButton* cancelButton;
    
    // Data
    StreamUP::HotkeyInfo selectedHotkey;
    QString selectedIconPath;
    bool isEditMode;
    QString originalItemId; // For edit mode
};

} // namespace StreamUP
