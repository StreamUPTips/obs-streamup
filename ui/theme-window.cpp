#include "theme-window.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp" 
#include "../utilities/error-handler.hpp"
#include "../version.h"
#include <obs-module.h>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QGroupBox>
#include <QPixmap>

namespace StreamUP {
namespace ThemeWindow {

// Static pointer to track open theme dialog
static QPointer<QDialog> themeDialog;

void CreateThemeDialog()
{
    // Check if theme dialog is already open
    if (!themeDialog.isNull() && themeDialog->isVisible()) {
        // Bring existing dialog to front
        themeDialog->raise();
        themeDialog->activateWindow();
        return;
    }

    UIHelpers::ShowDialogOnUIThread([]() {
        // Create standardized dialog using modern template system
        auto components = StreamUP::UIStyles::CreateStandardDialog(
            "StreamUP - Theme",
            "🎨 StreamUP OBS Theme", 
            "The cleanest OBS theme out there - available to all supporters of any tier"
        );
        
        components.dialog->setModal(false);
        
        // Store dialog reference
        themeDialog = components.dialog;
        
        // Create main content area
        QVBoxLayout* mainLayout = new QVBoxLayout();
        mainLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_16);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        
        // Description section
        QGroupBox* descriptionGroup = StreamUP::UIStyles::CreateStyledGroupBox("About the StreamUP Theme", "info");
        QVBoxLayout* descriptionLayout = new QVBoxLayout(descriptionGroup);
        descriptionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_12);
        
        QLabel* descriptionText = StreamUP::UIStyles::CreateStyledContent(
            "The StreamUP OBS Theme is carefully crafted to provide the cleanest and most "
            "professional OBS Studio experience. Featuring modern design elements, improved "
            "readability, and streamlined interfaces - it's the perfect companion to your "
            "streaming setup."
        );
        descriptionText->setWordWrap(true);
        descriptionLayout->addWidget(descriptionText);
        
        mainLayout->addWidget(descriptionGroup);
        
        // Features section
        QGroupBox* featuresGroup = StreamUP::UIStyles::CreateStyledGroupBox("Theme Features", "info");
        QVBoxLayout* featuresLayout = new QVBoxLayout(featuresGroup);
        featuresLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_8);
        
        QStringList features = {
            "• Clean, modern interface design",
            "• Improved color contrast and readability",
            "• Streamlined control layouts",
            "• Professional dark theme aesthetic",
            "• Optimized for streaming workflows",
            "• Regular updates and improvements"
        };
        
        for (const QString& feature : features) {
            QLabel* featureLabel = StreamUP::UIStyles::CreateStyledContent(feature);
            featuresLayout->addWidget(featureLabel);
        }
        
        mainLayout->addWidget(featuresGroup);
        
        // Preview section with placeholder images
        QGroupBox* previewGroup = StreamUP::UIStyles::CreateStyledGroupBox("Theme Preview", "info");
        QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
        previewLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_12);
        
        // Create placeholder image areas
        for (int i = 1; i <= 3; i++) {
            QLabel* placeholderLabel = new QLabel();
            placeholderLabel->setMinimumSize(400, 200);
            placeholderLabel->setMaximumSize(500, 250);
            placeholderLabel->setStyleSheet(QString(
                "QLabel {"
                "    background-color: %1;"
                "    border: 2px dashed %2;"
                "    border-radius: %3px;"
                "    color: %4;"
                "    font-size: 14px;"
                "    font-weight: 500;"
                "}"
            ).arg(StreamUP::UIStyles::Colors::BG_SECONDARY)
             .arg(StreamUP::UIStyles::Colors::BORDER_MEDIUM)
             .arg(StreamUP::UIStyles::Sizes::RADIUS_SM)
             .arg(StreamUP::UIStyles::Colors::TEXT_MUTED));
            
            placeholderLabel->setText(QString("Theme Preview Image %1\n(Placeholder)").arg(i));
            placeholderLabel->setAlignment(Qt::AlignCenter);
            placeholderLabel->setScaledContents(true);
            
            previewLayout->addWidget(placeholderLabel);
        }
        
        mainLayout->addWidget(previewGroup);
        
        // Access information section
        QGroupBox* accessGroup = StreamUP::UIStyles::CreateStyledGroupBox("How to Get the Theme", "success");
        QVBoxLayout* accessLayout = new QVBoxLayout(accessGroup);
        accessLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_12);
        
        QLabel* accessText = StreamUP::UIStyles::CreateStyledContent(
            "The StreamUP OBS Theme is available to all StreamUP supporters of any tier. "
            "Simply become a supporter and you'll gain access to download and use this "
            "professional theme for your OBS Studio setup."
        );
        accessText->setWordWrap(true);
        accessLayout->addWidget(accessText);
        
        // Support button
        QPushButton* supportButton = StreamUP::UIStyles::CreateStyledButton("Become a Supporter", "primary", 0, 150);
        QObject::connect(supportButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamip.tips/Premium"));
        });
        accessLayout->addWidget(supportButton);
        
        mainLayout->addWidget(accessGroup);
        
        // Add content to scroll area
        components.contentLayout->addLayout(mainLayout);
        
        // Update main button
        components.mainButton->setText("Visit StreamUP.tips");
        QObject::connect(components.mainButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips/"));
        });
        
        // Apply consistent sizing
        StreamUP::UIStyles::ApplyConsistentSizing(components.dialog, 600, 900, 500, 750);
        
        // Show dialog
        components.dialog->show();
        components.dialog->raise();
        components.dialog->activateWindow();
    });
}

void ShowThemeWindow()
{
    try {
        CreateThemeDialog();
    } catch (const std::exception& e) {
        StreamUP::ErrorHandler::LogError("Failed to show theme window: " + std::string(e.what()), StreamUP::ErrorHandler::Category::UI);
    }
}

bool IsThemeWindowOpen()
{
    return !themeDialog.isNull() && themeDialog->isVisible();
}

} // namespace ThemeWindow
} // namespace StreamUP