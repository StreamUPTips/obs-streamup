#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QFileDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QButtonGroup>
#include <QRadioButton>
#include <QDir>
#include <QIcon>
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>

namespace StreamUP {

class IconSelectorDialog : public StreamUP::UIStyles::ShadowDialog {
    Q_OBJECT

public:
    explicit IconSelectorDialog(const QString& currentIcon = QString(),
                               const QString& currentCustomIcon = QString(),
                               bool useCustomIcon = false,
                               QWidget* parent = nullptr);

    // Get selected icon information - now returns the path regardless of type
    QString getSelectedIcon() const;
    
private slots:
    void onIconButtonClicked();
    void onBrowseCustomIcon();
    void onCustomIconPathChanged();

private:
    void setupUI();
    void setupIconTabs();
    void populateOBSIcons();
    void populateCommonIcons();
    void populateCustomIcons();
    void saveCustomIcon(const QString& iconPath);
    void loadCustomIconHistory();
    void createIconButton(const QString& iconPath, const QString& iconName, QGridLayout* layout, int& row, int& col, const QString& category);
    QString getIconDisplayName(const QString& iconPath);
    QIcon loadPreviewIcon(const QString& iconPath);
    QString getOBSThemeIconPath(const QString& iconName);
    
    // UI Components
    QVBoxLayout* mainLayout;        // chrome.content
    QHBoxLayout* footerButtons;     // chrome.footerButtons (right-anchored action slot)
    QTabWidget* iconTabs;
    QScrollArea* obsScrollArea;
    QScrollArea* commonScrollArea;
    QScrollArea* customScrollArea;
    QWidget* obsIconsWidget;
    QWidget* commonIconsWidget;
    QWidget* customIconsWidget;
    QGridLayout* obsIconsLayout;
    QGridLayout* commonIconsLayout;
    QGridLayout* customIconsLayout;

    // Custom tab components
    StreamUP::UIStyles::PillButton* browseCustomButton;
    QLineEdit* customIconPath;

    QHBoxLayout* buttonLayout;
    StreamUP::UIStyles::PillButton* okButton;
    StreamUP::UIStyles::PillButton* cancelButton;
    
    QButtonGroup* iconButtonGroup;

    // Selected values
    QString selectedIconPath;  // The final selected path (any type)

    // Icon collections
    QList<QPair<QString, QString>> obsIcons;        // (path, name) pairs
    QList<QPair<QString, QString>> commonIcons;     // (path, name) pairs
    QStringList customIconHistory;                  // Previously used custom icon paths
    
    // Constants
    static const int ICON_SIZE = 32;
    static const int BUTTON_SIZE = 48;
    static const int GRID_COLUMNS = 8;
};

} // namespace StreamUP
