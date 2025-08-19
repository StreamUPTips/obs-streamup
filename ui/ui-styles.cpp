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
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <functional>
#include <obs-module.h>

namespace StreamUP {
namespace UIStyles {

QString GetDialogStyle() {
    // Let the StreamUP theme handle dialog styling
    return "";
}

QString GetTitleLabelStyle() {
    // Minimal styling - let theme handle most of it
    return "QLabel { font-weight: bold; }";
}

QString GetDescriptionLabelStyle() {
    // Minimal styling - let theme handle colors and fonts
    return "";
}

QString GetGroupBoxStyle(const QString& borderColor, const QString& titleColor) {
    // Let the StreamUP theme handle group box styling
    Q_UNUSED(borderColor)
    Q_UNUSED(titleColor)
    return "";
}

QString GetContentLabelStyle() {
    // No custom styling - inherit from theme
    return "";
}

QString GetButtonStyle(const QString& baseColor, const QString& hoverColor, int height) {
    // Apply rounded pill shape with visible border and solid hover effect
    Q_UNUSED(baseColor)
    Q_UNUSED(hoverColor)
    
    // Use standardized height for perfect pill shape
    int buttonHeight = 24; // Reduced height for better pill proportions
    int borderWidth = 2;
    int pillRadius = (buttonHeight / 2) + borderWidth; // Perfect pill: radius accounts for border
    
    return QString(
        "QPushButton {"
        "    border-radius: %1px;"
        "    border: 2px solid %5;"
        "    padding: 0px %2px;"
        "    min-height: %3px;"
        "    max-height: %3px;"
        "    height: %3px;"
        "    background: transparent;"
        "}"
        "QPushButton:hover {"
        "    background: %4 !important;"
        "    border-color: %4 !important;"
        "    opacity: 1.0;"
        "}"
    ).arg(pillRadius).arg(Sizes::SPACE_12).arg(buttonHeight).arg(Colors::PRIMARY_COLOR).arg(Colors::PRIMARY_COLOR);
}

QString GetSquircleButtonStyle(const QString& baseColor, const QString& hoverColor, int size) {
    // Apply rounded pill shape for icon buttons with visible border and solid hover effect
    Q_UNUSED(baseColor)
    Q_UNUSED(hoverColor)
    
    int buttonSize = size > 0 ? size : Sizes::ICON_BUTTON_SIZE;
    int borderWidth = 2;
    // For squircle: use larger radius for more rounded appearance
    int squircleRadius = 12 + borderWidth; // Fixed 10px radius creates more rounded squircle
    
    return QString(
        "QPushButton {"
        "    border-radius: %1px;"
        "    border: 2px solid %4;"
        "    min-width: %2px !important;"
        "    max-width: %2px !important;"
        "    min-height: %2px !important;"
        "    max-height: %2px !important;"
        "    width: %2px !important;"
        "    height: %2px !important;"
        "    padding: 0px !important;"
        "    margin: 0px !important;"
        "    background: transparent;"
        "}"
        "QPushButton:hover {"
        "    background: %3 !important;"
        "    border-color: %3 !important;"
        "    opacity: 1.0;"
        "}"
    ).arg(squircleRadius).arg(buttonSize).arg(Colors::PRIMARY_COLOR).arg(Colors::PRIMARY_COLOR);
}

QString GetScrollAreaStyle() {
    // Let StreamUP theme handle scrollbar styling
    return "";
}

QString GetTableStyle() {
    // Let StreamUP theme handle table styling
    return "";
}

QDialog* CreateStyledDialog(const QString& title, QWidget* parentWidget) {
    QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.toUtf8().constData(), parentWidget);
    // No custom styling - inherit from StreamUP theme
    return dialog;
}

QLabel* CreateStyledTitle(const QString& text) {
    QLabel* label = new QLabel(text);
    label->setStyleSheet(GetTitleLabelStyle());
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel* CreateStyledDescription(const QString& text) {
    QLabel* label = new QLabel(text);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel* CreateStyledContent(const QString& text) {
    QLabel* label = new QLabel(text);
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
    
    // No custom styling - let StreamUP theme handle group box appearance
    Q_UNUSED(type)
    
    return groupBox;
}

QScrollArea* CreateStyledScrollArea() {
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // No custom styling - let StreamUP theme handle scrollbar appearance
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
    
    // No custom styling - let StreamUP theme handle table appearance
    
    // Add context menu support for copying
    table->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(table, &QWidget::customContextMenuRequested, [table](const QPoint& pos) {
        QTableWidgetItem* item = table->itemAt(pos);
        if (!item) return;

        QMenu contextMenu(table);
        
        // Copy cell content
        QAction* copyAction = contextMenu.addAction("Copy");
        QObject::connect(copyAction, &QAction::triggered, [table, item]() {
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
    // Set initial size and show
    dialog->show();
    
    // Calculate proper size after layout is complete
    QTimer::singleShot(10, [dialog, maxWidth, maxHeight, minWidth, minHeight]() {
        // Force layout calculation multiple times to ensure accurate sizing
        dialog->adjustSize();
        QSize sizeHint = dialog->sizeHint();
        
        // Calculate appropriate size based on content with minimal padding
        int width = qMax(minWidth, qMin(sizeHint.width() + 40, maxWidth));
        int height = qMax(minHeight, qMin(sizeHint.height() + 20, maxHeight));
        
        dialog->resize(width, height);
        dialog->setMaximumSize(maxWidth, maxHeight);
        dialog->setMinimumSize(minWidth, minHeight);
        
        // Center the dialog after resizing
        StreamUP::UIHelpers::CenterDialog(dialog);
    });
}

void ApplyContentBasedSizing(QDialog* dialog) {
    // For dialogs that should size exactly to their content
    dialog->show();
    
    QTimer::singleShot(50, [dialog]() {
        // Force layout calculation first
        dialog->layout()->activate();
        dialog->adjustSize();
        QSize sizeHint = dialog->sizeHint();
        
        // Calculate size based on content with reasonable constraints
        int width = qMax(650, qMin(sizeHint.width() + 60, 1000));
        int height = qMax(400, qMin(sizeHint.height() + 40, 800));
        
        dialog->resize(width, height);
        
        // Set constraints but allow some flexibility
        dialog->setMinimumSize(650, 400);
        dialog->setMaximumSize(1000, 800);
        
        // Center the dialog after resizing
        StreamUP::UIHelpers::CenterDialog(dialog);
    });
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

} // namespace UIStyles
} // namespace StreamUP
