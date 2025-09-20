#include "ui-styles.hpp"
#include "ui-helpers.hpp"
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QObject>
#include <QSizePolicy>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QGuiApplication>
#include <QScreen>
#include <QTableWidget>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QAbstractItemView>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QScreen>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QPalette>
#include <QWidget>
#include <QStyle>
#include <functional>
#include <obs-module.h>
#include <obs-frontend-api.h>

namespace StreamUP {
namespace UIStyles {

QString GetDialogStyle() {
    // Modern StreamUP dialog styling - prefer GetOBSCompliantDialogStyle() for new code
    // This version maintains custom StreamUP theme colors for existing custom dialogs
    return QString(
        "QDialog {"
        "    background-color: %1;"
        "    border-radius: %2px;"
        "    padding: %3px;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
        "QDialog * {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
    ).arg(Colors::BG_DARKEST)
     .arg(Sizes::RADIUS_DOCK)
     .arg(Sizes::PADDING_LARGE); // 20px padding around edges
}

QString GetTitleLabelStyle() {
    // Modern title styling with enhanced typography
    return QString(
        "QLabel {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 24px;"
        "    font-weight: 700;"
        "    color: %1;"
        "    letter-spacing: -0.5px;"
        "    line-height: 1.2;"
        "    margin: 0px;"
        "    padding: 0px 0px 8px 0px;"
        "}"
    ).arg(Colors::TEXT_PRIMARY);
}

QString GetDescriptionLabelStyle() {
    // Modern description styling with improved readability
    return QString(
        "QLabel {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 14px;"
        "    font-weight: 400;"
        "    color: %1;"
        "    line-height: 1.4;"
        "    letter-spacing: 0px;"
        "    margin: 0px;"
        "    padding: 0px 0px 4px 0px;"
        "    opacity: 0.85;"
        "}"
    ).arg(Colors::TEXT_SECONDARY);
}

QString GetGroupBoxStyle(const QString& borderColor, const QString& titleColor) {
    // Modern StreamUP group box styling matching OBSBasicSettings
    Q_UNUSED(borderColor)
    Q_UNUSED(titleColor)
    
    return QString(
        "QGroupBox {"
        "    padding: %1px;"
        "    padding-top: %2px;"
        "    margin-right: %3px;"
        "    margin-top: %4px;"
        "    margin-bottom: %1px;"
        "    outline: none;"
        "    border-radius: %5px;"
        "    background-color: %6;"
        "}"
        "QGroupBox::title {"
        "    background-color: %6;"
        "    color: %7;"
        "    padding: %8px %9px %9px %9px;"
        "    margin: %3px 0 %3px 0;"
        "    border-top-left-radius: %5px;"
        "    border-top-right-radius: %10px;"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top left;"
        "    top: -%11px;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-weight: 800;"
        "    font-size: 14px;"
        "}"
    ).arg(Sizes::PADDING_LARGE)        // 1. General padding (20px)
     .arg(Sizes::PADDING_LARGE + 10)   // 2. Top padding (30px)
     .arg(Sizes::SPACE_10)             // 3. Margin right (10px)
     .arg(Sizes::SPACE_14 + 1)         // 4. Margin top (15px)
     .arg(Sizes::RADIUS_GROUPBOX)      // 5. Border radius (32px) - Increased for more rounded corners
     .arg(Colors::BG_PRIMARY)          // 6. Background color - Match main toolbar configurator
     .arg(Colors::TEXT_PRIMARY)        // 7. Title text color
     .arg(Sizes::SPACE_4)              // 8. Title padding vertical (4px)
     .arg(Sizes::SPACE_16)             // 9. Title padding horizontal (16px)
     .arg(Sizes::RADIUS_LG)            // 10. Title top-right radius (14px)
     .arg(Sizes::SPACE_8);             // 11. Title top offset (8px)
}

QString GetContentLabelStyle() {
    // Modern content label styling with enhanced readability
    return QString(
        "QLabel {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 15px;"
        "    font-weight: 400;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    letter-spacing: 0.1px;"
        "    margin: 0px;"
        "    padding: 8px 0px;"
        "}"
    ).arg(Colors::TEXT_PRIMARY);
}

QString GetButtonStyle(const QString& baseColor, const QString& hoverColor, int height) {
    // Updated to match StreamUP theme button styles
    Q_UNUSED(baseColor)
    Q_UNUSED(hoverColor)
    Q_UNUSED(height)
    
    return QString(
        "QPushButton {"
        "    background-color: transparent;"
        "    color: %1;"
        "    border: %2px solid %3;"
        "    min-height: %4px;"
        "    max-height: %4px;"
        "    border-radius: %5px;"
        "    margin: %6px;"
        "    padding: %6px %7px;"
        "    outline: none;"
        "    font-weight: 800;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %3;"
        "    color: %1;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %8;"
        "    color: %1;"
        "}"
        "QPushButton:disabled {"
        "    color: %9;"
        "    border-color: %10;"
        "    background-color: transparent;"
        "}"
        "QPushButton:disabled:hover,"
        "QPushButton:disabled:pressed {"
        "    background-color: transparent;"
        "    border-color: %10;"
        "    color: %9;"
        "}"
    ).arg(Colors::TEXT_PRIMARY)
     .arg(Sizes::BUTTON_BORDER_SIZE)
     .arg(Colors::PRIMARY_COLOR)
     .arg(Sizes::BUTTON_HEIGHT)
     .arg(Sizes::RADIUS_LG)
     .arg(Sizes::SPACE_2)
     .arg(Sizes::SPACE_12)
     .arg(Colors::PRIMARY_HOVER)
     .arg(Colors::TEXT_DISABLED)
     .arg(Colors::BORDER_DISABLED);
}

QString GetSquircleButtonStyle(const QString& baseColor, const QString& hoverColor, int size) {
    // Updated to match StreamUP theme button styles for icon buttons
    Q_UNUSED(baseColor)
    Q_UNUSED(hoverColor)
    
    int buttonSize = size > 0 ? size : Sizes::ICON_BUTTON_SIZE;
    
    return QString(
        "QPushButton {"
        "    background-color: transparent;"
        "    color: %2;"
        "    border: %3px solid %4;"
        "    border-radius: %5px;"
        "    min-width: %1px !important;"
        "    max-width: %1px !important;"
        "    min-height: %1px !important;"
        "    max-height: %1px !important;"
        "    width: %1px !important;"
        "    height: %1px !important;"
        "    padding: 0px !important;"
        "    margin: %6px !important;"
        "    outline: none;"
        "    font-weight: 800;"
        "}"
        "QPushButton:hover {"
        "    background-color: %4;"
        "    color: %2;"
        "}"
        "QPushButton:pressed {"
        "    background-color: %7;"
        "    color: %2;"
        "}"
        "QPushButton:disabled {"
        "    color: %8;"
        "    border-color: %9;"
        "    background-color: transparent;"
        "}"
    ).arg(buttonSize)
     .arg(Colors::TEXT_PRIMARY)
     .arg(Sizes::BUTTON_BORDER_SIZE)
     .arg(Colors::PRIMARY_COLOR)
     .arg(Sizes::RADIUS_LG)
     .arg(Sizes::SPACE_2)
     .arg(Colors::PRIMARY_HOVER)
     .arg(Colors::TEXT_DISABLED)
     .arg(Colors::BORDER_DISABLED);
}

QString GetScrollAreaStyle() {
    // Modern StreamUP scrollbar styling
    return QString(
        "QScrollBar {"
        "    background-color: %1;"
        "    margin: 0px;"
        "    border-radius: 3px;"
        "}"
        "QScrollArea::corner {"
        "    background-color: %1;"
        "    border: none;"
        "}"
        "QScrollBar:vertical {"
        "    width: 6px;"
        "}"
        "QScrollBar::add-line:vertical,"
        "QScrollBar::sub-line:vertical {"
        "    border: none;"
        "    background: none;"
        "    height: 0px;"
        "}"
        "QScrollBar::up-arrow:vertical,"
        "QScrollBar::down-arrow:vertical,"
        "QScrollBar::add-page:vertical,"
        "QScrollBar::sub-page:vertical {"
        "    border: none;"
        "    background: none;"
        "    color: none;"
        "}"
        "QScrollBar:horizontal {"
        "    height: 6px;"
        "}"
        "QScrollBar::add-line:horizontal,"
        "QScrollBar::sub-line:horizontal {"
        "    border: none;"
        "    background: none;"
        "    width: 0px;"
        "}"
        "QScrollBar::left-arrow:horizontal,"
        "QScrollBar::right-arrow:horizontal,"
        "QScrollBar::add-page:horizontal,"
        "QScrollBar::sub-page:horizontal {"
        "    border: none;"
        "    background: none;"
        "    color: none;"
        "}"
        "QScrollBar::handle {"
        "    background-color: %2;"
        "    border-radius: 3px;"
        "}"
        "QScrollBar::handle:hover {"
        "    background-color: %3;"
        "}"
    ).arg(Colors::BG_SECONDARY)
     .arg(Colors::PRIMARY_COLOR)  
     .arg(Colors::PRIMARY_HOVER);
}

QString GetTableStyle() {
    // Apply modern table styling matching StreamUP theme
    return GetListWidgetStyle() + GetEnhancedScrollAreaStyle();
}

QDialog* CreateStyledDialog(const QString& title, QWidget* parentWidget) {
    QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.toUtf8().constData(), parentWidget);
    // Apply modern StreamUP dialog styling
    dialog->setStyleSheet(GetDialogStyle());
    return dialog;
}

QLabel* CreateStyledTitle(const QString& text) {
    QLabel* label = new QLabel(text);
    
    // Modern title styling with enhanced typography
    QString modernTitleStyle = QString(
        "QLabel {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 24px;"
        "    font-weight: 700;"
        "    color: %1;"
        "    letter-spacing: -0.5px;"
        "    line-height: 1.2;"
        "    margin: 0px;"
        "    padding: 0px 0px 8px 0px;"
        "}"
    ).arg(Colors::TEXT_PRIMARY);
    
    label->setStyleSheet(modernTitleStyle);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel* CreateStyledDescription(const QString& text) {
    QLabel* label = new QLabel(text);
    
    // Modern description styling with improved readability
    QString modernDescStyle = QString(
        "QLabel {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 14px;"
        "    font-weight: 400;"
        "    color: %1;"
        "    line-height: 1.4;"
        "    letter-spacing: 0px;"
        "    margin: 0px;"
        "    padding: 0px 0px 4px 0px;"
        "    opacity: 0.85;"
        "}"
    ).arg(Colors::TEXT_SECONDARY);
    
    label->setStyleSheet(modernDescStyle);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel* CreateStyledContent(const QString& text) {
    QLabel* label = new QLabel(text);
    
    // Modern content styling with enhanced readability
    QString modernContentStyle = QString(
        "QLabel {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 15px;"
        "    font-weight: 400;"
        "    color: %1;"
        "    line-height: 1.5;"
        "    letter-spacing: 0.1px;"
        "    margin: 0px;"
        "    padding: 8px 0px;"
        "}"
        "QLabel a {"
        "    color: %2;"
        "    text-decoration: none;"
        "    font-weight: 500;"
        "}"
        "QLabel a:hover {"
        "    color: %3;"
        "    text-decoration: underline;"
        "}"
    ).arg(Colors::TEXT_PRIMARY)
     .arg(Colors::INFO)
     .arg(Colors::PRIMARY_HOVER);
    
    label->setStyleSheet(modernContentStyle);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setOpenExternalLinks(true);
    return label;
}

QPushButton* CreateStyledButton(const QString& text, const QString& type, int height, int minWidth) {
    QPushButton* button = new QPushButton(text);
    
    // Basic size policy
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    
    // Set minimum width if specified
    if (minWidth > 0) {
        button->setMinimumWidth(minWidth);
    }
    
    // Apply pill shape styling - ignore height parameter to ensure consistency
    button->setStyleSheet(GetButtonStyle("", "", 0));
    
    Q_UNUSED(type)
    Q_UNUSED(height) // Ignored for consistency
    
    return button;
}

QPushButton* CreateStyledSquircleButton(const QString& text, const QString& type, int size) {
    QPushButton* button = new QPushButton(text);
    
    // Set fixed size for icon buttons
    button->setFixedSize(size, size);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    // Apply pill/circular shape styling for square icon buttons
    button->setStyleSheet(GetSquircleButtonStyle("", "", size));
    
    Q_UNUSED(type)
    
    return button;
}

QGroupBox* CreateStyledGroupBox(const QString& title, const QString& type) {
    QGroupBox* groupBox = new QGroupBox(title);
    
    // Apply modern StreamUP group box styling
    groupBox->setStyleSheet(GetGroupBoxStyle("", ""));
    Q_UNUSED(type)
    
    return groupBox;
}

QScrollArea* CreateStyledScrollArea() {
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // Apply modern StreamUP scrollbar styling
    scrollArea->setStyleSheet(GetScrollAreaStyle());
    return scrollArea;
}

// Table utility functions
QTableWidget* CreateStyledTableWidget(QWidget* parent) {
    QTableWidget* table = new QTableWidget(parent);
    
    // Configure table behavior (not styling)
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSortingEnabled(false);
    table->setShowGrid(false);
    
    // Header configuration
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    
    // Apply modern scrollbar styling to table
    table->setStyleSheet(GetTableStyle());
    
    // Add context menu support for copying
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(table, &QWidget::customContextMenuRequested, [table](const QPoint& pos) {
        QTableWidgetItem* item = table->itemAt(pos);
        if (!item) return;

        QMenu contextMenu(table);
        
        // Copy cell content
        QAction* copyAction = contextMenu.addAction("Copy");
        QObject::connect(copyAction, &QAction::triggered, [item]() {
            QApplication::clipboard()->setText(item->text());
        });
        
        // Copy entire row
        QAction* copyRowAction = contextMenu.addAction("Copy Row");
        QObject::connect(copyRowAction, &QAction::triggered, [table, item]() {
            int row = item->row();
            QStringList rowData;
            for (int col = 0; col < table->columnCount(); ++col) {
                QTableWidgetItem* cellItem = table->item(row, col);
                rowData << (cellItem ? cellItem->text() : "");
            }
            QApplication::clipboard()->setText(rowData.join("\t"));
        });

        contextMenu.exec(table->mapToGlobal(pos));
    });
    
    return table;
}

QTableWidget* CreateStyledTable(const QStringList& headers, QWidget* parent) {
    QTableWidget* table = CreateStyledTableWidget(parent);
    
    // Set headers if provided
    if (!headers.isEmpty()) {
        table->setColumnCount(headers.count());
        table->setHorizontalHeaderLabels(headers);
        
        // Configure last column to stretch by default
        if (headers.count() > 0) {
            table->horizontalHeader()->setSectionResizeMode(headers.count() - 1, QHeaderView::Stretch);
        }
    }
    
    return table;
}

void AutoResizeTableColumns(QTableWidget* table) {
    // First, resize columns to contents tightly
    table->resizeColumnsToContents();
    
    // Add minimal padding only for fixed columns (0 to n-1), skip last column as it stretches
    for (int col = 0; col < table->columnCount() - 1; ++col) {
        int currentWidth = table->columnWidth(col);
        // Add minimal padding (8px) for readability, no artificial minimums
        int finalWidth = currentWidth + 16; // 8px padding each side
        table->setColumnWidth(col, finalWidth);
    }
    
    // Calculate minimum width needed for fixed columns only
    int fixedColumnsWidth = 0;
    for (int col = 0; col < table->columnCount() - 1; ++col) {
        fixedColumnsWidth += table->columnWidth(col);
    }
    
    // Add minimum width for the stretch column and borders
    int minStretchWidth = 120; // Minimum width for last column
    int totalMinWidth = fixedColumnsWidth + minStretchWidth + 10; // Include border space
    
    // Set the table's minimum width - last column will stretch to fill any extra space
    table->setMinimumWidth(totalMinWidth);
}

void HandleTableCellClick(QTableWidget* table, int row, int column) {
    // Default implementation - can be overridden for specific table needs
    QTableWidgetItem* item = table->item(row, column);
    if (!item) return;
    
    // Check if item has URL data for clickable links
    QVariant urlData = item->data(Qt::UserRole);
    if (urlData.isValid()) {
        QDesktopServices::openUrl(QUrl(urlData.toString()));
    }
}

void ApplyAutoSizing(QDialog* dialog, int minWidth, int maxWidth, int minHeight, int maxHeight) {
    // Ensure we have reasonable screen-aware maximum sizes
    QScreen* screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        // Use 80% of screen width/height as safe maximum, but respect passed parameters
        maxWidth = qMin(maxWidth, static_cast<int>(screenGeometry.width() * 0.8));
        maxHeight = qMin(maxHeight, static_cast<int>(screenGeometry.height() * 0.8));
    }
    
    // Set minimum constraints first
    dialog->setMinimumSize(minWidth, minHeight);
    
    // Calculate proper size before showing
    dialog->adjustSize();
    QSize sizeHint = dialog->sizeHint();
    
    // Calculate appropriate size based on content with padding
    int width = qMax(minWidth, qMin(sizeHint.width() + 40, maxWidth));
    int height = qMax(minHeight, qMin(sizeHint.height() + 20, maxHeight));
    
    // Set the calculated size before showing
    dialog->resize(width, height);
    
    // Set maximum constraints after sizing but before showing
    dialog->setMaximumSize(maxWidth, maxHeight);
    
    // Now show the dialog
    dialog->show();
    
    // Center the dialog after showing
    StreamUP::UIHelpers::CenterDialog(dialog);
}

void ApplyContentBasedSizing(QDialog* dialog) {
    // For dialogs that should size exactly to their content
    // Force layout calculation first
    dialog->layout()->activate();
    dialog->adjustSize();
    
    // Show the dialog after sizing
    dialog->show();
    
    // Center the dialog after showing
    StreamUP::UIHelpers::CenterDialog(dialog);
}

void ApplyScrollAreaContentSizing(QScrollArea* scrollArea, int maxHeight) {
    QTimer::singleShot(10, [scrollArea, maxHeight]() {
        if (scrollArea->widget()) {
            QWidget* widget = scrollArea->widget();
            widget->adjustSize();
            
            // Calculate required height based on content
            int contentHeight = widget->sizeHint().height();
            int newHeight = qMin(contentHeight + 20, maxHeight);
            
            scrollArea->setMaximumHeight(newHeight);
            scrollArea->setMinimumHeight(qMin(newHeight, 100));
        }
    });
}

void ApplyDynamicSizing(QDialog* dialog, int minWidth, int maxWidth, int minHeight, int maxHeight) {
    // Set initial constraints
    dialog->setMinimumSize(minWidth, minHeight);
    dialog->setMaximumSize(maxWidth, maxHeight);
    
    // Store original resize behavior
    dialog->setProperty("dynamicSizing", true);
    
    // Hide dialog initially to prevent flashing
    bool wasVisible = dialog->isVisible();
    if (wasVisible) {
        dialog->hide();
    }
    
    // Calculate optimal size with multiple passes for accuracy
    QTimer::singleShot(1, [dialog, minWidth, maxWidth, minHeight, maxHeight, wasVisible]() {
        // Force multiple layout calculations to ensure accurate sizing
        dialog->layout()->activate();
        dialog->adjustSize();
        
        // Second pass for better accuracy
        QTimer::singleShot(5, [dialog, minWidth, maxWidth, minHeight, maxHeight, wasVisible]() {
            dialog->layout()->activate();
            dialog->adjustSize();
            QSize sizeHint = dialog->sizeHint();
            
            // Get the actual required size from layout with better padding calculation
            int contentWidth = sizeHint.width();
            int contentHeight = sizeHint.height();
            
            // Add minimal padding to prevent content from being cut off or requiring scroll
            int targetWidth = qMax(minWidth, qMin(contentWidth + 40, maxWidth));
            int targetHeight = qMax(minHeight, qMin(contentHeight + 20, maxHeight));
            
            // Ensure the dialog fits its content without needing scroll
            dialog->resize(targetWidth, targetHeight);
            
            // Final adjustment to ensure no scrolling is needed
            QTimer::singleShot(10, [dialog, targetWidth, targetHeight, wasVisible]() {
                // Double-check that content fits without scrolling
                QSize finalSizeHint = dialog->sizeHint();
                if (finalSizeHint.height() > targetHeight || finalSizeHint.width() > targetWidth) {
                    int adjustedWidth = qMax(targetWidth, finalSizeHint.width() + 20);
                    int adjustedHeight = qMax(targetHeight, finalSizeHint.height() + 10);
                    dialog->resize(adjustedWidth, adjustedHeight);
                }
                
                // Center the dialog using our standardized function
                StreamUP::UIHelpers::CenterDialog(dialog);
                
                // Show the dialog after sizing is complete
                if (wasVisible) {
                    dialog->show();
                }
            });
        });
    });
}

void ApplyConsistentSizing(QDialog* dialog, int preferredWidth, int maxWidth, int minHeight, int maxHeight) {
    // Set initial constraints
    dialog->setMinimumSize(preferredWidth, minHeight);
    dialog->setMaximumSize(maxWidth, maxHeight);
    
    // Hide dialog initially to prevent flashing
    bool wasVisible = dialog->isVisible();
    if (wasVisible) {
        dialog->hide();
    }
    
    // Calculate optimal size with consistent width approach
    QTimer::singleShot(1, [dialog, preferredWidth, maxWidth, minHeight, maxHeight, wasVisible]() {
        // Force layout calculation
        dialog->layout()->activate();
        dialog->adjustSize();
        QSize sizeHint = dialog->sizeHint();
        
        // Use preferred width as the standard, but expand more generously for content that needs it
        int targetWidth = preferredWidth;
        if (sizeHint.width() > preferredWidth) {
            // Give more margin for complex UI elements like hotkey controls
            targetWidth = qMin(sizeHint.width() + 60, maxWidth);
        }
        
        // Calculate height to fit content without scrolling - be more generous
        int targetHeight = qMax(minHeight, qMin(sizeHint.height() + 80, maxHeight));
        
        // Apply the calculated size
        dialog->resize(targetWidth, targetHeight);
        
        // Final validation pass
        QTimer::singleShot(10, [dialog, targetWidth, targetHeight, wasVisible]() {
            QSize finalSizeHint = dialog->sizeHint();
            if (finalSizeHint.height() > targetHeight) {
                // Only adjust height if content doesn't fit - be more generous
                int adjustedHeight = qMin(finalSizeHint.height() + 60, dialog->maximumHeight());
                dialog->resize(targetWidth, adjustedHeight);
            }
            
            // Center the dialog
            StreamUP::UIHelpers::CenterDialog(dialog);
            
            // Show the dialog after sizing is complete
            if (wasVisible) {
                dialog->show();
            }
        });
    });
}

StandardDialogComponents CreateStandardDialog(const QString& windowTitle, const QString& headerTitle, const QString& headerDescription, QWidget* parentWidget)
{
    StandardDialogComponents components;
    
    // Create main dialog
    components.dialog = CreateStyledDialog(windowTitle, parentWidget);
    components.dialog->resize(700, 500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(components.dialog);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header section - no custom styling, inherit from theme
    components.headerWidget = new QWidget();
    components.headerWidget->setObjectName("headerWidget");
    // No custom styling - let StreamUP theme handle header appearance
    
    QVBoxLayout* headerLayout = new QVBoxLayout(components.headerWidget);
    headerLayout->setContentsMargins(20, 20, 20, 20);
    
    components.titleLabel = CreateStyledTitle(headerTitle);
    components.titleLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(components.titleLabel);
    
    headerLayout->addSpacing(5);
    
    components.subtitleLabel = CreateStyledDescription(headerDescription);
    headerLayout->addWidget(components.subtitleLabel);
    
    mainLayout->addWidget(components.headerWidget);

    // Content area with scroll
    components.scrollArea = CreateStyledScrollArea();

    components.contentWidget = new QWidget();
    // No custom styling - inherit from theme
    components.contentLayout = new QVBoxLayout(components.contentWidget);
    components.contentLayout->setContentsMargins(20, 20, 20, 20);
    components.contentLayout->setSpacing(20);

    components.scrollArea->setWidget(components.contentWidget);
    mainLayout->addWidget(components.scrollArea);

    // Bottom button area
    QWidget* buttonWidget = new QWidget();
    // No custom styling - inherit from theme
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(20, 10, 20, 10);

    components.mainButton = CreateStyledButton("Close", "neutral");
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(components.mainButton);
    buttonLayout->addStretch();
    
    mainLayout->addWidget(buttonWidget);
    
    return components;
}

void UpdateDialogHeader(StandardDialogComponents& components, const QString& title, const QString& description)
{
    components.titleLabel->setText(title);
    components.subtitleLabel->setText(description);
}

void SetupDialogNavigation(StandardDialogComponents& components, std::function<void()> onMainButton)
{
    QObject::connect(components.mainButton, &QPushButton::clicked, [onMainButton]() {
        onMainButton();
    });
}

// New style functions to match StreamUP theme

QString GetLineEditStyle() {
    return QString(
        "QLineEdit, QTextEdit, QPlainTextEdit {"
        "    background-color: %1;"
        "    border: none;"
        "    border-radius: %2px;"
        "    padding: %3px %4px;"
        "    color: %5;"
        "    font-weight: 500;"
        "    font-size: 13px;"
        "    min-height: %6px;"
        "    selection-background-color: %7;"
        "    selection-color: %5;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
        "QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {"
        "    border: %8px solid %9;"
        "    outline: none;"
        "}"
    ).arg(Colors::BG_PRIMARY)
     .arg(Sizes::RADIUS_SM)
     .arg(Sizes::INPUT_PAD_Y)
     .arg(Sizes::INPUT_PAD_X)
     .arg(Colors::TEXT_PRIMARY)
     .arg(Sizes::INPUT_HEIGHT)
     .arg(Colors::PRIMARY_ALPHA_30)
     .arg(Sizes::SPACE_2)
     .arg(Colors::PRIMARY_COLOR);
}

QString GetCheckBoxStyle() {
    return QString(
        "QCheckBox {"
        "    color: %1;"
        "    font-weight: 500;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "    width: 44px;"
        "    height: 22px;"
        "    background-color: %2;"
        "    border: 1px solid %3;"
        "    border-radius: 11px;"
        "}"
        "QCheckBox::indicator:checked {"
        "    width: 44px;"
        "    height: 22px;"
        "    background-color: %4;"
        "    border: 1px solid %4;"
        "    border-radius: 11px;"
        "}"
    ).arg(Colors::TEXT_PRIMARY)
     .arg(Colors::BG_SECONDARY)
     .arg(Colors::BORDER_MEDIUM)
     .arg(Colors::PRIMARY_COLOR);
}

QString GetComboBoxStyle() {
    return QString(
        "QComboBox {"
        "    background-color: %1;"
        "    border: none;"
        "    border-radius: %2px;"
        "    padding: %3px %4px %3px %5px;"
        "    margin: %3px;"
        "    color: %6;"
        "    font-weight: 500;"
        "    font-size: 13px;"
        "    min-height: %7px;"
        "    max-height: %7px;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
        "QComboBox:hover, QComboBox:focus {"
        "    background-color: %8;"
        "    outline: none;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: %7px;"
        "    background-color: %9;"
        "    border-top-right-radius: %2px;"
        "    border-bottom-right-radius: %2px;"
        "}"
        "QComboBox QAbstractItemView {"
        "    background-color: %10;"
        "    border: 0px solid %11;"
        "    border-radius: %12px;"
        "    selection-background-color: %13;"
        "    color: %6;"
        "    padding: %14px;"
        "    font-weight: 600;"
        "    outline: none;"
        "    show-decoration-selected: 0;"
        "}"
        "QComboBox QAbstractItemView::item {"
        "    padding: %3px %15px;"
        "    border-radius: %16px;"
        "    margin: %17px %3px;"
        "    border: none;"
        "    outline: none;"
        "}"
        "QComboBox QAbstractItemView::item:selected {"
        "    background-color: %9;"
        "    color: %6;"
        "    outline: none;"
        "}"
    ).arg(Colors::BG_DARKEST)         // %1 - background
     .arg(Sizes::RADIUS_SM)           // %2 - border radius
     .arg(Sizes::SPACE_2)             // %3 - padding/margin
     .arg(Sizes::INPUT_PAD_X_WITH_ICON) // %4 - right padding
     .arg(Sizes::SPACE_12)            // %5 - left padding
     .arg(Colors::TEXT_PRIMARY)       // %6 - text color
     .arg(Sizes::BOX_HEIGHT)          // %7 - height
     .arg(Colors::HOVER_OVERLAY)      // %8 - hover background
     .arg(Colors::PRIMARY_COLOR)      // %9 - dropdown/selection background
     .arg(Colors::MENU_OVERLAY)       // %10 - menu background
     .arg(Colors::BORDER_MEDIUM)      // %11 - border color
     .arg(Sizes::RADIUS_LG)           // %12 - menu border radius
     .arg(Colors::PRIMARY_ALPHA_30)   // %13 - selection background
     .arg(Sizes::SPACE_8)             // %14 - menu padding
     .arg(Sizes::SPACE_10)            // %15 - item padding
     .arg(Sizes::MENU_ITEM_PILL_RADIUS) // %16 - item border radius
     .arg(Sizes::SPACE_1);            // %17 - item margin
}

QString GetSpinBoxStyle() {
    return QString(
        "QSpinBox, QDoubleSpinBox {"
        "    background-color: %1;"
        "    border: none;"
        "    border-radius: %2px;"
        "    padding: %3px %4px %3px %5px;"
        "    margin: %3px;"
        "    color: %6;"
        "    font-weight: 500;"
        "    font-size: 13px;"
        "    min-height: %7px;"
        "    max-height: %7px;"
        "    selection-background-color: %8;"
        "    selection-color: %6;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
        "QSpinBox:hover, QDoubleSpinBox:hover, QSpinBox:focus, QDoubleSpinBox:focus {"
        "    background-color: %9;"
        "    outline: none;"
        "}"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "    border: none;"
        "    background-color: %10;"
        "    width: %7px;"
        "}"
        "QSpinBox::up-button, QDoubleSpinBox::up-button {"
        "    border-top-right-radius: %2px;"
        "}"
        "QSpinBox::down-button, QDoubleSpinBox::down-button {"
        "    border-bottom-right-radius: %2px;"
        "}"
    ).arg(Colors::BG_PRIMARY)
     .arg(Sizes::RADIUS_SM)
     .arg(Sizes::SPACE_2)
     .arg(Sizes::INPUT_PAD_X_WITH_ICON)
     .arg(Sizes::SPACE_12)
     .arg(Colors::TEXT_PRIMARY)
     .arg(Sizes::BOX_HEIGHT)
     .arg(Colors::PRIMARY_ALPHA_30)
     .arg(Colors::HOVER_OVERLAY)
     .arg(Colors::PRIMARY_COLOR);
}

QString GetSliderStyle() {
    return QString(
        "QSlider::groove:horizontal {"
        "    height: %1px;"
        "    background: %2;"
        "    border: none;"
        "    border-radius: %3px;"
        "    margin: 0;"
        "}"
        "QSlider::groove:vertical {"
        "    width: %1px;"
        "    background: %2;"
        "    border: none;"
        "    border-radius: %3px;"
        "    margin: 0;"
        "}"
        "QSlider::sub-page:horizontal {"
        "    background: %4;"
        "    height: %1px;"
        "    border: none;"
        "    border-radius: %3px;"
        "    margin: 0;"
        "}"
        "QSlider::add-page:horizontal {"
        "    background: %5;"
        "    border: none;"
        "    border-radius: %3px;"
        "    margin: 0;"
        "}"
        "QSlider::handle:horizontal, QSlider::handle:vertical {"
        "    width: 12px;"
        "    height: 12px;"
        "    margin: -4px 0;"
        "    background: %6;"
        "    border: 1px solid %7;"
        "    border-radius: 6px;"
        "}"
        "QSlider::handle:hover {"
        "    background: #f0f0f0;"
        "    border: 1px solid %4;"
        "}"
    ).arg(Sizes::SLIDER_GROOVE_SIZE)
     .arg(Colors::SURFACE_SUBTLE)
     .arg(Sizes::SPACE_2)
     .arg(Colors::PRIMARY_COLOR)
     .arg(Colors::BORDER_MEDIUM)
     .arg(Colors::TEXT_PRIMARY)
     .arg(Colors::BG_SECONDARY);
}

QString GetListWidgetStyle() {
    return QString(
        "QTreeView, QListView, QListWidget, QTreeWidget {"
        "    font-weight: 500;"
        "    background-color: %1;"
        "    border: 0;"
        "    outline: none;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "    font-size: 14px;"
        "    color: %2;"
        "}"
        "QListWidget::item, QTreeWidget::item, QTreeView::item {"
        "    outline: none;"
        "    border: none;"
        "    min-height: 24px;"
        "    max-height: 24px;"
        "    padding: 0 6px 0 6px;"
        "}"
        "QListWidget::item:hover, QTreeWidget::item:hover, QTreeView::item:hover {"
        "    background-color: %3;"
        "    color: %2;"
        "    border-radius: %4px;"
        "}"
        "QListWidget::item:selected, QTreeWidget::item:selected, QTreeView::item:selected {"
        "    background-color: %5;"
        "    color: %2;"
        "    border-radius: %4px;"
        "}"
    ).arg(Colors::BG_SECONDARY)
     .arg(Colors::TEXT_PRIMARY)
     .arg(Colors::PRIMARY_ALPHA_30)
     .arg(Sizes::RADIUS_MD)
     .arg(Colors::PRIMARY_COLOR);
}

QString GetMenuStyle() {
    return QString(
        "QMenu {"
        "    background-color: %1;"
        "    border: 1px solid %2;"
        "    border-radius: 12px;"
        "    padding: %3px %4px;"
        "    color: %5;"
        "    font-weight: 600;"
        "    font-size: 13px;"
        "    margin: %6px;"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
        "QMenu::item {"
        "    padding: %7px %8px;"
        "    border-radius: %9px;"
        "    margin: %10px;"
        "    min-height: 16px;"
        "    background-color: transparent;"
        "}"
        "QMenu::item:selected {"
        "    background-color: %11;"
        "    color: %5;"
        "}"
        "QMenu::item:disabled {"
        "    color: %12;"
        "    background-color: transparent;"
        "}"
        "QMenu::separator {"
        "    height: 1px;"
        "    background-color: %2;"
        "    margin: %13px %14px;"
        "}"
    ).arg(Colors::MENU_OVERLAY)
     .arg(Colors::BORDER_SUBTLE)
     .arg(Sizes::MENU_PAD_Y)
     .arg(Sizes::MENU_PAD_X)
     .arg(Colors::TEXT_PRIMARY)
     .arg(Sizes::SPACE_4)
     .arg(Sizes::MENU_ITEM_PAD_Y)
     .arg(Sizes::MENU_ITEM_PAD_X)
     .arg(Sizes::MENU_ITEM_PILL_RADIUS)
     .arg(Sizes::SPACE_1)
     .arg(Colors::PRIMARY_COLOR)
     .arg(Colors::TEXT_DISABLED)
     .arg(Sizes::SPACE_6)
     .arg(Sizes::SPACE_12);
}

// Enhanced scrollbar style to match exact theme
QString GetEnhancedScrollAreaStyle() {
    return QString(
        "QScrollBar {"
        "    background-color: %1;"
        "    margin: 0px;"
        "    border-radius: 3px;"
        "}"
        "QScrollBar:vertical {"
        "    width: 6px;"
        "}"
        "QScrollBar:horizontal {"
        "    height: 6px;"
        "}"
        "QScrollBar::add-line, QScrollBar::sub-line {"
        "    border: none;"
        "    background: none;"
        "}"
        "QScrollBar::add-page, QScrollBar::sub-page {"
        "    border: none;"
        "    background: none;"
        "}"
        "QScrollBar::handle {"
        "    background-color: %2;"
        "    border-radius: 3px;"
        "    min-height: 6px;"
        "    min-width: 6px;"
        "}"
        "QScrollBar::handle:hover {"
        "    background-color: %3;"
        "}"
    ).arg(Colors::BG_SECONDARY)
     .arg(Colors::PRIMARY_COLOR)
     .arg(Colors::PRIMARY_HOVER);
}

// Apply StreamUP theme styles to specific widget types
void ApplyStreamUPThemeStyles(QWidget* widget) {
    if (!widget) return;
    
    // Apply StreamUP font family and enhanced scrollbar styling globally
    QString globalStyle = QString(
        "* {"
        "    font-family: Roboto, 'Open Sans', '.AppleSystemUIFont', Helvetica, Arial, 'MS Shell Dlg', sans-serif;"
        "}"
    ) + GetEnhancedScrollAreaStyle();
    
    widget->setStyleSheet(globalStyle);
}

//-------------------OBS/QT-COMPLIANT STYLING FUNCTIONS-------------------
// These functions use OBS theme system and Qt palette for better integration

QString GetOBSCompliantDialogStyle() {
    // Minimal styling that respects OBS theme and Qt palette
    return QString(
        "QDialog {"
        "    background-color: palette(window);"
        "    color: palette(window-text);"
        "    border: 1px solid palette(mid);"
        "}"
    );
}

QString GetOBSCompliantButtonStyle(const QString& variant) {
    // Use Qt palette colors with minimal custom styling
    if (variant == "primary") {
        return QString(
            "QPushButton {"
            "    background-color: palette(highlight);"
            "    color: palette(highlighted-text);"
            "    border: 1px solid palette(highlight);"
            "    border-radius: 4px;"
            "    padding: 4px 16px;"
            "}"
            "QPushButton:hover {"
            "    background-color: palette(highlight);"
            "    opacity: 0.9;"
            "}"
            "QPushButton:pressed {"
            "    background-color: palette(dark);"
            "}"
            "QPushButton:disabled {"
            "    background-color: palette(mid);"
            "    color: palette(disabled, window-text);"
            "}"
        );
    }
    
    // Default button style using palette
    return QString(
        "QPushButton {"
        "    background-color: palette(button);"
        "    color: palette(button-text);"
        "    border: 1px solid palette(mid);"
        "    border-radius: 4px;"
        "    padding: 4px 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: palette(light);"
        "}"
        "QPushButton:pressed {"
        "    background-color: palette(dark);"
        "}"
        "QPushButton:disabled {"
        "    background-color: palette(mid);"
        "    color: palette(disabled, button-text);"
        "}"
    );
}

QString GetOBSCompliantGroupBoxStyle() {
    // Simple group box that respects Qt/OBS theming
    return QString(
        "QGroupBox {"
        "    background-color: palette(base);"
        "    border: 1px solid palette(mid);"
        "    border-radius: 12px;"
        "    margin-top: 10px;"
        "    padding-top: 4px;"
        "}"
        "QGroupBox::title {"
        "    color: palette(window-text);"
        "    subcontrol-origin: margin;"
        "    subcontrol-position: top left;"
        "    padding: 0 5px;"
        "    background-color: palette(window);"
        "}"
    );
}

QString GetMinimalNativeStyle() {
    // Absolutely minimal styling - let Qt/OBS handle everything
    return QString();
}

void ApplyOBSNativeTheming(QWidget* widget) {
    if (!widget) return;
    
    // Clear any custom stylesheets and let Qt handle theming
    widget->setStyleSheet(GetMinimalNativeStyle());
    
    // Ensure the widget uses the system palette
    widget->setAutoFillBackground(true);
    
    // Use the current application palette (which OBS sets up)
    widget->setPalette(QApplication::palette());
}

bool IsOBSThemeDark() {
    // obs_frontend_is_theme_dark was introduced in OBS 29.0.0
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
    return obs_frontend_is_theme_dark();
#else
    // Fallback for older OBS versions - assume light theme
    return false;
#endif
}

} // namespace UIStyles
} // namespace StreamUP
