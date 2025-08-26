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
            "professional OBS Studio experience. This theme perfectly matches the StreamUP plugin's "
            "design language, creating a seamless and cohesive interface throughout OBS Studio. "
            "Featuring modern design elements, improved readability, and streamlined interfaces - "
            "it's the perfect companion to your streaming setup."
        );
        descriptionText->setWordWrap(true);
        descriptionLayout->addWidget(descriptionText);
        
        mainLayout->addWidget(descriptionGroup);
        
        
        // Preview section with placeholder images
        QGroupBox* previewGroup = StreamUP::UIStyles::CreateStyledGroupBox("Theme Preview", "info");
        QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
        previewLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_12);
        
        QStringList imagePaths = {
            ":/images/misc/obs-theme-1.png",
            ":/images/misc/obs-theme-2.png", 
            ":/images/misc/obs-theme-3.png",
            ":/images/misc/obs-theme-4.png"
        };
        
        // Create a flexible layout that adapts to different image sizes
        QVBoxLayout* imagesLayout = new QVBoxLayout();
        imagesLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_12);
        
        for (int i = 0; i < imagePaths.size(); i++) {
            QLabel* imageLabel = new QLabel();
            QPixmap pixmap(imagePaths[i]);
            
            if (!pixmap.isNull()) {
                // Calculate optimal display size based on original image
                QSize originalSize = pixmap.size();
                int maxWidth = 500;  // Maximum width for display
                int maxHeight = 350; // Maximum height for display
                
                // Scale maintaining aspect ratio, but ensure reasonable viewing size
                QPixmap scaledPixmap = pixmap.scaled(maxWidth, maxHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->setPixmap(scaledPixmap);
                imageLabel->setAlignment(Qt::AlignCenter);
                
                // Add subtle styling
                imageLabel->setStyleSheet(QString(
                    "QLabel {"
                    "    border-radius: %1px;"
                    "    border: 1px solid %2;"
                    "    background-color: %3;"
                    "    padding: 4px;"
                    "}"
                ).arg(StreamUP::UIStyles::Sizes::RADIUS_SM)
                 .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE)
                 .arg(StreamUP::UIStyles::Colors::BG_SECONDARY));
                
            } else {
                // Fallback if image not found
                imageLabel->setMinimumSize(400, 250);
                imageLabel->setStyleSheet(QString(
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
                imageLabel->setText(QString("Theme Preview %1\n(Image not found)").arg(i + 1));
                imageLabel->setAlignment(Qt::AlignCenter);
            }
            
            imagesLayout->addWidget(imageLabel);
        }
        
        previewLayout->addLayout(imagesLayout);
        
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
            QDesktopServices::openUrl(QUrl("https://streamup.tips/premium"));
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