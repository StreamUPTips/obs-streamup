#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include "streamup-toolbar-config.hpp"
#include "obs-hotkey-manager.hpp"

namespace StreamUP {

class HotkeyButtonConfigDialog : public QDialog {
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
    QVBoxLayout* mainLayout;
    
    // Hotkey selection section
    QGroupBox* hotkeyGroup;
    QFormLayout* hotkeyFormLayout;
    QLabel* selectedHotkeyLabel;
    QLabel* hotkeyDescriptionLabel;
    QPushButton* selectHotkeyButton;
    
    // Icon selection section
    QGroupBox* iconGroup;
    QVBoxLayout* iconLayout;

    QHBoxLayout* iconPreviewLayout;
    QLabel* iconPreviewLabel;
    QLabel* iconPreview;
    QPushButton* selectIconButton;
    
    // Button customization section
    QGroupBox* customizationGroup;
    QFormLayout* customizationLayout;
    QLineEdit* buttonTextEdit;
    QLineEdit* tooltipEdit;
    
    // Dialog buttons
    QHBoxLayout* buttonLayout;
    QPushButton* okButton;
    QPushButton* cancelButton;
    
    // Data
    StreamUP::HotkeyInfo selectedHotkey;
    QString selectedIconPath;
    bool isEditMode;
    QString originalItemId; // For edit mode
};

} // namespace StreamUP