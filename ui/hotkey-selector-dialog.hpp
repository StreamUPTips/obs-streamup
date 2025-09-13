#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QSplitter>
#include "obs-hotkey-manager.hpp"

namespace StreamUP {

class HotkeySelectorDialog : public QDialog {
    Q_OBJECT

public:
    explicit HotkeySelectorDialog(QWidget* parent = nullptr);
    
    // Get selected hotkey information
    StreamUP::HotkeyInfo getSelectedHotkey() const { return selectedHotkey; }
    bool hasSelection() const { return !selectedHotkey.name.isEmpty(); }

private slots:
    void onSearchTextChanged(const QString& text);
    void onHotkeySelectionChanged();
    void onHotkeyItemDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void setupUI();
    void populateHotkeys();
    void filterHotkeys(const QString& searchText);
    void updateHotkeyDetails();
    QTreeWidgetItem* createCategoryItem(const QString& categoryName);
    QTreeWidgetItem* createHotkeyItem(const StreamUP::HotkeyInfo& hotkey, QTreeWidgetItem* parent);
    QString getHotkeyKeybinding(const QString& hotkeyName);
    
    // UI Components
    QVBoxLayout* mainLayout;
    QSplitter* mainSplitter;
    
    QWidget* leftPanel;
    QVBoxLayout* leftLayout;
    QLineEdit* searchBox;
    QTreeWidget* hotkeyTree;
    
    QWidget* rightPanel;
    QVBoxLayout* rightLayout;
    QGroupBox* detailsGroup;
    QLabel* selectedHotkeyName;
    QLabel* selectedHotkeyDescription;
    QLabel* selectedHotkeyKeys;
    QTextEdit* selectedHotkeyHelp;
    
    QHBoxLayout* buttonLayout;
    QPushButton* okButton;
    QPushButton* cancelButton;
    
    // Data
    StreamUP::HotkeyInfo selectedHotkey;
    QMap<QString, QList<StreamUP::HotkeyInfo>> categorizedHotkeys;
    QMap<QTreeWidgetItem*, StreamUP::HotkeyInfo> itemHotkeyMap;
    QList<QTreeWidgetItem*> allHotkeyItems; // For filtering
};

} // namespace StreamUP