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
        // Create modern unified dialog
        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog("StreamUP - Theme");
        dialog->setModal(false);
        dialog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        
        // Store dialog reference
        themeDialog = dialog;
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Modern unified content area with scroll - everything inside
        QScrollArea* scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
                                          StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

        // Modern header inside scrollable area
        QWidget* headerSection = new QWidget();
        QVBoxLayout* headerLayout = new QVBoxLayout(headerSection);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_8);

        // Title with modern styling
        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle("StreamUP OBS Theme");
        titleLabel->setAlignment(Qt::AlignCenter);

        // Description with modern styling
        QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription("The cleanest OBS theme out there - available to all supporters of any tier");
        subtitleLabel->setAlignment(Qt::AlignCenter);

        headerLayout->addWidget(titleLabel);
        headerLayout->addWidget(subtitleLabel);
        
        contentLayout->addWidget(headerSection);
        contentLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);
        
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
        // Ensure text has transparent background
        descriptionText->setStyleSheet(descriptionText->styleSheet() + " QLabel { background: transparent; }");
        descriptionLayout->addWidget(descriptionText);
        
        contentLayout->addWidget(descriptionGroup);
        
        
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
        
        // Create carousel container with transparent background
        QWidget* carouselContainer = new QWidget();
        carouselContainer->setStyleSheet("QWidget { background: transparent; }");
        QVBoxLayout* carouselLayout = new QVBoxLayout(carouselContainer);
        carouselLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_16);
        carouselLayout->setContentsMargins(0, 0, 0, 0);
        carouselLayout->setAlignment(Qt::AlignCenter);
        
        // Main image display (focused/current image) - using button for click functionality
        QPushButton* mainImageButton = new QPushButton();
        mainImageButton->setFlat(true);
        mainImageButton->setCursor(Qt::PointingHandCursor);
        mainImageButton->setFixedSize(508, 358); // Fixed size to match scaled image without border
        mainImageButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        mainImageButton->setStyleSheet(QString(
            "QPushButton {"
            "    border: none;"
            "    background: transparent;"
            "    padding: 4px;"
            "}"
            "QPushButton:hover {"
            "    background: rgba(255, 255, 255, 0.05);"
            "}"
            "QPushButton:pressed {"
            "    background: rgba(255, 255, 255, 0.1);"
            "}"
        ));
        
        // Thumbnail navigation with transparent background
        QWidget* thumbnailsContainer = new QWidget();
        thumbnailsContainer->setStyleSheet("QWidget { background: transparent; }");
        QHBoxLayout* thumbnailsLayout = new QHBoxLayout(thumbnailsContainer);
        thumbnailsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_8);
        thumbnailsLayout->setContentsMargins(0, 0, 0, 0);
        
        // Store images and create carousel logic
        QVector<QPixmap>* originalImages = new QVector<QPixmap>();
        QVector<QPushButton*>* thumbnailButtons = new QVector<QPushButton*>();
        int* currentImageIndex = new int(0);
        
        // Load images and create thumbnails
        for (int i = 0; i < imagePaths.size(); i++) {
            QPixmap pixmap(imagePaths[i]);
            
            if (!pixmap.isNull()) {
                originalImages->append(pixmap);
                
                // Create thumbnail
                QPixmap thumbnail = pixmap.scaled(80, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                QPushButton* thumbButton = new QPushButton();
                thumbButton->setIcon(QIcon(thumbnail));
                thumbButton->setIconSize(thumbnail.size());
                thumbButton->setFlat(true);
                thumbButton->setFixedSize(88, 68);
                thumbButton->setCursor(Qt::PointingHandCursor);
                thumbButton->setProperty("imageIndex", i);
                
                // Style thumbnail button
                thumbButton->setStyleSheet(QString(
                    "QPushButton {"
                    "    border-radius: %1px;"
                    "    border: 2px solid %2;"
                    "    background: transparent;"
                    "    padding: 2px;"
                    "}"
                    "QPushButton:hover {"
                    "    border: 2px solid %3;"
                    "}"
                    "QPushButton[selected=\"true\"] {"
                    "    border: 2px solid %3;"
                    "}"
                ).arg(StreamUP::UIStyles::Sizes::RADIUS_SM)
                 .arg(StreamUP::UIStyles::Colors::BORDER_MEDIUM)
                 .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR));
                
                thumbnailButtons->append(thumbButton);
                thumbnailsLayout->addWidget(thumbButton);
            } else {
                // Create placeholder for missing images
                originalImages->append(QPixmap());
                QPushButton* placeholderButton = new QPushButton("N/A");
                placeholderButton->setFixedSize(88, 68);
                placeholderButton->setEnabled(false);
                thumbnailButtons->append(placeholderButton);
                thumbnailsLayout->addWidget(placeholderButton);
            }
        }
        
        // Center thumbnails
        thumbnailsLayout->insertStretch(0);
        thumbnailsLayout->addStretch();
        
        // Function to update main image
        auto updateMainImage = [mainImageButton, originalImages, thumbnailButtons, currentImageIndex]() {
            if (*currentImageIndex < originalImages->size() && !originalImages->at(*currentImageIndex).isNull()) {
                QPixmap scaledMain = originalImages->at(*currentImageIndex).scaled(500, 350, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                mainImageButton->setIcon(QIcon(scaledMain));
                mainImageButton->setIconSize(scaledMain.size());
                
                // Update thumbnail selection
                for (int i = 0; i < thumbnailButtons->size(); i++) {
                    thumbnailButtons->at(i)->setProperty("selected", i == *currentImageIndex);
                    thumbnailButtons->at(i)->style()->unpolish(thumbnailButtons->at(i));
                    thumbnailButtons->at(i)->style()->polish(thumbnailButtons->at(i));
                }
            }
        };
        
        // Connect thumbnail clicks
        for (int i = 0; i < thumbnailButtons->size(); i++) {
            QObject::connect(thumbnailButtons->at(i), &QPushButton::clicked, [i, currentImageIndex, updateMainImage]() {
                *currentImageIndex = i;
                updateMainImage();
            });
        }
        
        // Main image click-to-zoom
        QObject::connect(mainImageButton, &QPushButton::clicked, [originalImages, currentImageIndex]() {
            if (*currentImageIndex < originalImages->size() && !originalImages->at(*currentImageIndex).isNull()) {
                QPixmap originalPixmap = originalImages->at(*currentImageIndex);
                
                // Create full-size dialog
                QDialog* zoomDialog = new QDialog();
                zoomDialog->setWindowTitle(QString("Theme Preview %1").arg(*currentImageIndex + 1));
                zoomDialog->setModal(true);
                zoomDialog->setAttribute(Qt::WA_DeleteOnClose);
                
                QVBoxLayout* zoomLayout = new QVBoxLayout(zoomDialog);
                zoomLayout->setContentsMargins(10, 10, 10, 10);
                
                QScrollArea* zoomScrollArea = new QScrollArea();
                zoomScrollArea->setWidgetResizable(true);
                zoomScrollArea->setAlignment(Qt::AlignCenter);
                
                QLabel* fullImageLabel = new QLabel();
                fullImageLabel->setPixmap(originalPixmap);
                fullImageLabel->setAlignment(Qt::AlignCenter);
                
                zoomScrollArea->setWidget(fullImageLabel);
                zoomLayout->addWidget(zoomScrollArea);
                
                // Size dialog based on image size
                QSize imageSize = originalPixmap.size();
                int dialogWidth = qMin(imageSize.width() + 50, 1200);
                int dialogHeight = qMin(imageSize.height() + 100, 800);
                zoomDialog->resize(dialogWidth, dialogHeight);
                
                zoomDialog->exec();
            }
        });
        
        // Initialize with first image
        updateMainImage();
        
        // Add components to carousel layout
        carouselLayout->addWidget(mainImageButton);
        carouselLayout->addWidget(thumbnailsContainer);
        
        // Center the carousel within the preview group box
        previewLayout->addStretch();
        previewLayout->addWidget(carouselContainer);
        previewLayout->addStretch();
        
        contentLayout->addWidget(previewGroup);
        
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
        // Ensure text has transparent background
        accessText->setStyleSheet(accessText->styleSheet() + " QLabel { background: transparent; }");
        accessLayout->addWidget(accessText);
        
        // Support button
        QPushButton* supportButton = StreamUP::UIStyles::CreateStyledButton("Become a Supporter", "primary", 0, 150);
        QObject::connect(supportButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips/premium"));
        });
        accessLayout->addWidget(supportButton);
        
        contentLayout->addWidget(accessGroup);
        
        // Add close button at the bottom of the content area (inside scroll)
        contentLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
        
        // Modern button section inside scrollable content
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        
        QPushButton* visitButton = StreamUP::UIStyles::CreateStyledButton("Visit StreamUP.tips", "primary");
        QObject::connect(visitButton, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips/"));
        });
        
        QPushButton* closeButton = StreamUP::UIStyles::CreateStyledButton("Close", "neutral");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

        buttonLayout->addStretch();
        buttonLayout->addWidget(visitButton);
        buttonLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        buttonLayout->addWidget(closeButton);
        buttonLayout->addStretch();
        
        contentLayout->addLayout(buttonLayout);
        contentLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        scrollArea->setWidget(contentWidget);
        mainLayout->addWidget(scrollArea);
        
        // Apply flexible sizing that fits content
        StreamUP::UIStyles::ApplyDynamicSizing(dialog, 650, 850, 600, 750);
        
        // Show dialog
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
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