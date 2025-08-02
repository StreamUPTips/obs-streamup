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
        .arg(Sizes::SPACING_SMALL);
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
        .arg(Sizes::SPACING_SMALL);
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

QString GetButtonStyle(const QString& baseColor, const QString& hoverColor) {
    return QString(
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
        "color: white;"
        "border: none;"
        "padding: %3px %4px;"
        "font-size: %5px;"
        "font-weight: bold;"
        "border-radius: %6px;"
        "min-width: 100px;"
        "}"
        "QPushButton:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %7, stop:1 %1);"
        "}")
        .arg(baseColor)
        .arg(hoverColor)
        .arg(Sizes::PADDING_MEDIUM)
        .arg(Sizes::PADDING_XL + 4)
        .arg(Sizes::FONT_SIZE_NORMAL)
        .arg(6)
        .arg(hoverColor);
}

QString GetScrollAreaStyle() {
    return QString(
        "QScrollArea {"
        "border: none;"
        "background: %1;"
        "}"
        "QScrollBar:vertical {"
        "background: %2;"
        "width: 12px;"
        "border-radius: 6px;"
        "}"
        "QScrollBar::handle:vertical {"
        "background: %3;"
        "border-radius: 6px;"
        "min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "background: %4;"
        "}")
        .arg(Colors::BACKGROUND_DARK)
        .arg(Colors::BACKGROUND_HOVER)
        .arg(Colors::TEXT_DISABLED)
        .arg(Colors::TEXT_MUTED);
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

QDialog* CreateStyledDialog(const QString& title) {
    QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow(title.toUtf8().constData());
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

QPushButton* CreateStyledButton(const QString& text, const QString& type) {
    QPushButton* button = new QPushButton(text);
    
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
    
    button->setStyleSheet(GetButtonStyle(baseColor, hoverColor));
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

} // namespace UIStyles
} // namespace StreamUP