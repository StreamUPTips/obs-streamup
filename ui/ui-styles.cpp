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
#include <functional>
#include <obs-module.h>

namespace StreamUP {
namespace UIStyles {

QString GetDialogStyle() {
    return QString("QDialog { background: %1; }").arg(Colors::BACKGROUND_DARK);
}

QString GetTitleLabelStyle() {
    return QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "font-weight: bold;"
        "margin: 0px 0px %3px 0px;"
        "padding: 0px;"
        "}")
        .arg(Colors::TEXT_PRIMARY)
        .arg(Sizes::FONT_SIZE_LARGE)
        .arg(Sizes::SPACING_TINY);
}

QString GetDescriptionLabelStyle() {
    return QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "margin: 0px 0px %3px 0px;"
        "padding: 0px;"
        "}")
        .arg(Colors::TEXT_MUTED)
        .arg(Sizes::FONT_SIZE_NORMAL)
        .arg(Sizes::SPACING_TINY);
}

QString GetGroupBoxStyle(const QString& borderColor, const QString& titleColor) {
    return QString(
        "QGroupBox {"
        "color: %1;"
        "font-size: %2px;"
        "font-weight: bold;"
        "border: %3px solid %4;"
        "border-radius: %5px;"
        "margin-top: %6px;"
        "padding-top: %7px;"
        "background: %8;"
        "}"
        "QGroupBox::title {"
        "subcontrol-origin: margin;"
        "left: %9px;"
        "padding: 0 %10px 0 %10px;"
        "color: %11;"
        "}")
        .arg(Colors::TEXT_PRIMARY)
        .arg(Sizes::FONT_SIZE_MEDIUM)
        .arg(Sizes::BORDER_WIDTH)
        .arg(borderColor)
        .arg(Sizes::BORDER_RADIUS_LARGE)
        .arg(Sizes::SPACING_MEDIUM)
        .arg(Sizes::SPACING_SMALL)
        .arg(Colors::BACKGROUND_CARD)
        .arg(Sizes::PADDING_MEDIUM)
        .arg(Sizes::PADDING_SMALL)
        .arg(titleColor);
}

QString GetContentLabelStyle() {
    return QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "background: transparent;"
        "border: none;"
        "text-align: center;"
        "}")
        .arg(Colors::TEXT_SECONDARY)
        .arg(Sizes::FONT_SIZE_SMALL);
}

QString GetButtonStyle(const QString& baseColor, const QString& hoverColor, int height) {
    int radius = height / 2;  // Perfect pill shape: radius = half height
    return QString(
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2) !important;"
        "color: white !important;"
        "border: none !important;"
        "padding: 0px 12px !important;"
        "font-size: 13px !important;"
        "font-weight: bold !important;"
        "border-radius: %4px !important;"
        "height: %5px !important;"
        "min-height: %5px !important;"
        "max-height: %5px !important;"
        "}"
        "QPushButton:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %3, stop:1 %1) !important;"
        "}")
        .arg(baseColor)
        .arg(hoverColor)
        .arg(hoverColor)
        .arg(radius)
        .arg(height);
}

QString GetSquircleButtonStyle(const QString& baseColor, const QString& hoverColor, int size) {
    // True squircle radius: approximately 40% of size for smooth, organic curves
    int radius = static_cast<int>(size * 0.4);
    return QString(
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2) !important;"
        "color: white !important;"
        "border: none !important;"
        "padding: 0px !important;"
        "margin: 0px !important;"
        "font-size: 13px !important;"
        "font-weight: bold !important;"
        "border-radius: %4px !important;"
        "width: %5px !important;"
        "height: %5px !important;"
        "min-width: %5px !important;"
        "max-width: %5px !important;"
        "min-height: %5px !important;"
        "max-height: %5px !important;"
        "qproperty-sizePolicy: Fixed Fixed !important;"
        "}"
        "QPushButton:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %3, stop:1 %1) !important;"
        "}")
        .arg(baseColor)
        .arg(hoverColor)
        .arg(hoverColor)
        .arg(radius)
        .arg(size);
}

QString GetScrollAreaStyle() {
    return QString(
        "QScrollArea {"
        "border: none;"
        "background: %1;"
        "}")
        .arg(Colors::BACKGROUND_DARK);
}


QString GetTableStyle() {
    return QString(
        "table {"
        "width: 100%;"
        "border: 1px solid %1;"
        "cellpadding: %2px;"
        "cellspacing: 0;"
        "border-collapse: collapse;"
        "background: %3;"
        "color: white;"
        "border-radius: %4px;"
        "overflow: hidden;"
        "}")
        .arg(Colors::BORDER_LIGHT)
        .arg(Sizes::PADDING_SMALL)
        .arg(Colors::BACKGROUND_INPUT)
        .arg(Sizes::BORDER_RADIUS);
}

QDialog* CreateStyledDialog(const QString& title, QWidget* parentWidget) {
    QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.toUtf8().constData(), parentWidget);
    dialog->setStyleSheet(GetDialogStyle());
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
    label->setStyleSheet(GetDescriptionLabelStyle());
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

QLabel* CreateStyledContent(const QString& text) {
    QLabel* label = new QLabel(text);
    label->setStyleSheet(GetContentLabelStyle());
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setOpenExternalLinks(true);
    return label;
}

QPushButton* CreateStyledButton(const QString& text, const QString& type, int height, int minWidth) {
    QPushButton* button = new QPushButton(text);
    
    // Set size policy to fit content width
    button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    
    // Set minimum width if specified
    if (minWidth > 0) {
        button->setMinimumWidth(minWidth);
    }
    
    QString baseColor, hoverColor;
    if (type == "success") {
        baseColor = Colors::SUCCESS;
        hoverColor = Colors::SUCCESS_HOVER;
    } else if (type == "error") {
        baseColor = Colors::ERROR;
        hoverColor = Colors::ERROR_HOVER;
    } else if (type == "warning") {
        baseColor = Colors::WARNING;
        hoverColor = Colors::WARNING_HOVER;
    } else if (type == "info") {
        baseColor = Colors::INFO;
        hoverColor = Colors::INFO_HOVER;
    } else { // neutral
        baseColor = Colors::NEUTRAL;
        hoverColor = Colors::NEUTRAL_HOVER;
    }
    
    button->setStyleSheet(GetButtonStyle(baseColor, hoverColor, height));
    return button;
}

QPushButton* CreateStyledSquircleButton(const QString& text, const QString& type, int size) {
    QPushButton* button = new QPushButton(text);
    
    // Enforce fixed square size
    button->setFixedSize(size, size);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    QString baseColor, hoverColor;
    if (type == "success") {
        baseColor = Colors::SUCCESS;
        hoverColor = Colors::SUCCESS_HOVER;
    } else if (type == "error") {
        baseColor = Colors::ERROR;
        hoverColor = Colors::ERROR_HOVER;
    } else if (type == "warning") {
        baseColor = Colors::WARNING;
        hoverColor = Colors::WARNING_HOVER;
    } else if (type == "info") {
        baseColor = Colors::INFO;
        hoverColor = Colors::INFO_HOVER;
    } else { // neutral
        baseColor = Colors::NEUTRAL;
        hoverColor = Colors::NEUTRAL_HOVER;
    }
    
    button->setStyleSheet(GetSquircleButtonStyle(baseColor, hoverColor, size));
    return button;
}

QGroupBox* CreateStyledGroupBox(const QString& title, const QString& type) {
    QGroupBox* groupBox = new QGroupBox(title);
    
    QString borderColor, titleColor;
    if (type == "success") {
        borderColor = titleColor = Colors::SUCCESS;
    } else if (type == "error") {
        borderColor = titleColor = Colors::ERROR;
    } else if (type == "warning") {
        borderColor = titleColor = Colors::WARNING;
    } else { // info
        borderColor = titleColor = Colors::INFO;
    }
    
    groupBox->setStyleSheet(GetGroupBoxStyle(borderColor, titleColor));
    return groupBox;
}

QScrollArea* CreateStyledScrollArea() {
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setStyleSheet(GetScrollAreaStyle());
    return scrollArea;
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

    // Header section - exact WebSocket UI pattern
    components.headerWidget = new QWidget();
    components.headerWidget->setObjectName("headerWidget");
    components.headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px %4px %3px; }")
        .arg(Colors::BACKGROUND_CARD)
        .arg(Sizes::PADDING_XL + Sizes::PADDING_MEDIUM) // More padding at top
        .arg(Sizes::PADDING_XL)
        .arg(Sizes::PADDING_XL)); // Standard padding at bottom
    
    QVBoxLayout* headerLayout = new QVBoxLayout(components.headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    components.titleLabel = CreateStyledTitle(headerTitle);
    components.titleLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(components.titleLabel);
    
    // Add reduced spacing between title and description
    headerLayout->addSpacing(-Sizes::SPACING_SMALL);
    
    components.subtitleLabel = CreateStyledDescription(headerDescription);
    headerLayout->addWidget(components.subtitleLabel);
    
    mainLayout->addWidget(components.headerWidget);

    // Content area with scroll
    components.scrollArea = CreateStyledScrollArea();

    components.contentWidget = new QWidget();
    components.contentWidget->setStyleSheet(QString("background: %1;").arg(Colors::BACKGROUND_DARK));
    components.contentLayout = new QVBoxLayout(components.contentWidget);
    components.contentLayout->setContentsMargins(Sizes::PADDING_XL + 5, 
        Sizes::PADDING_XL, 
        Sizes::PADDING_XL + 5, 
        Sizes::PADDING_XL);
    components.contentLayout->setSpacing(Sizes::SPACING_XL);

    components.scrollArea->setWidget(components.contentWidget);
    mainLayout->addWidget(components.scrollArea);

    // Bottom button area
    QWidget* buttonWidget = new QWidget();
    buttonWidget->setStyleSheet("background: transparent;");
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(Sizes::PADDING_XL, 
        Sizes::PADDING_MEDIUM, 
        Sizes::PADDING_XL, 
        Sizes::PADDING_MEDIUM);

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
