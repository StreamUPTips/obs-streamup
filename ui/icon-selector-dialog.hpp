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
#include <QGroupBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QDir>
#include <QIcon>

namespace StreamUP {

class IconSelectorDialog : public QDialog {
    Q_OBJECT

public:
    explicit IconSelectorDialog(const QString& currentIcon = QString(), 
                               const QString& currentCustomIcon = QString(),
                               bool useCustomIcon = false,
                               QWidget* parent = nullptr);
    
    // Get selected icon information
    QString getSelectedIcon() const { return selectedIcon; }
    QString getSelectedCustomIcon() const { return selectedCustomIcon; }
    bool shouldUseCustomIcon() const { return useCustomIcon; }
    
private slots:
    void onIconButtonClicked();
    void onBrowseCustomIcon();
    void onIconTypeChanged();

private:
    void setupUI();
    void setupIconTabs();
    void populateOBSIcons();
    void populateDefaultHotkeyIcons();
    void createIconButton(const QString& iconPath, const QString& iconName, QGridLayout* layout, int& row, int& col, const QString& category);
    QString getIconDisplayName(const QString& iconPath);
    QIcon loadPreviewIcon(const QString& iconPath);
    QString getOBSThemeIconPath(const QString& iconName);
    
    // UI Components
    QVBoxLayout* mainLayout;
    QTabWidget* iconTabs;
    QScrollArea* obsScrollArea;
    QScrollArea* defaultScrollArea;
    QWidget* obsIconsWidget;
    QWidget* defaultIconsWidget;
    QGridLayout* obsIconsLayout;
    QGridLayout* defaultIconsLayout;
    
    QGroupBox* customIconGroup;
    QRadioButton* useIconRadio;
    QRadioButton* useCustomRadio;
    QLabel* customIconPreview;
    QPushButton* browseCustomButton;
    QLineEdit* customIconPath;
    
    QHBoxLayout* buttonLayout;
    QPushButton* okButton;
    QPushButton* cancelButton;
    
    QButtonGroup* iconButtonGroup;
    
    // Selected values
    QString selectedIcon;
    QString selectedCustomIcon;
    bool useCustomIcon;
    
    // Icon collections
    QList<QPair<QString, QString>> obsIcons;        // (path, name) pairs
    QList<QPair<QString, QString>> defaultIcons;    // (path, name) pairs
    
    // Constants
    static const int ICON_SIZE = 32;
    static const int BUTTON_SIZE = 48;
    static const int GRID_COLUMNS = 8;
};

} // namespace StreamUP