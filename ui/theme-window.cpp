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
#include <QFrame>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QEnterEvent>
#include <QDialog>
#include <QKeyEvent>
#include <QScreen>
#include <QMouseEvent>

namespace StreamUP {
namespace ThemeWindow {

// Theme dialog is now managed by DialogManager in ui-helpers

// Carousel widget for theme images
class ThemeImageCarousel : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeImageCarousel(QWidget* parent = nullptr) : QWidget(parent), currentIndex(0)
    {
        setFixedHeight(400);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setupUI();
        loadImages();
        setupTimer();
    }

public slots:
    void nextImage()
    {
        if (images.isEmpty()) return;
        
        currentIndex = (currentIndex + 1) % images.size();
        updateImage();
        updateDots();
    }

    void previousImage()
    {
        if (images.isEmpty()) return;
        
        currentIndex = (currentIndex - 1 + images.size()) % images.size();
        updateImage();
        updateDots();
    }

private slots:

    void goToImage(int index)
    {
        if (index >= 0 && index < images.size()) {
            currentIndex = index;
            updateImage();
            updateDots();
        }
    }

private:
    void setupUI()
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        // Image container - full width to place buttons at edges
        imageContainer = new QFrame();
        imageContainer->setFixedHeight(350);
        imageContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        imageContainer->setStyleSheet(QString(
            "QFrame {"
            "    background: transparent;"
            "    border: none;"
            "}"
        ));

        QHBoxLayout* containerLayout = new QHBoxLayout(imageContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // Navigation buttons (hidden - using external buttons instead)
        prevButton = StreamUP::UIStyles::CreateStyledButton("❮", "neutral", 40);
        prevButton->setFixedSize(40, 40);
        prevButton->setVisible(false);
        connect(prevButton, &QPushButton::clicked, this, &ThemeImageCarousel::previousImage);

        nextButton = StreamUP::UIStyles::CreateStyledButton("❯", "neutral", 40);
        nextButton->setFixedSize(40, 40);
        nextButton->setVisible(false);
        connect(nextButton, &QPushButton::clicked, this, &ThemeImageCarousel::nextImage);

        // Image label centered without button constraints
        imageLabel = new QLabel();
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(false);
        imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        imageLabel->setCursor(Qt::PointingHandCursor);
        
        // Connect click event for zooming
        imageLabel->installEventFilter(this);

        // Simple centered layout for image only
        containerLayout->addWidget(imageLabel, 1, Qt::AlignCenter);

        // Dots indicator - centered
        dotsContainer = new QHBoxLayout();
        dotsContainer->setContentsMargins(0, 0, 0, 0);
        dotsContainer->setSpacing(8);
        dotsContainer->addStretch(); // Add stretch before dots to center them

        layout->addWidget(imageContainer);
        layout->addLayout(dotsContainer);
        layout->addStretch();
    }

    void loadImages()
    {
        // Load theme images
        QStringList imageFiles = {"obs-theme-1.png", "obs-theme-2.png", "obs-theme-3.png", "obs-theme-4.png"};
        
        for (const QString& fileName : imageFiles) {
            QString imagePath = QString(":/images/misc/%1").arg(fileName);
            QPixmap pixmap(imagePath);
            
            if (!pixmap.isNull()) {
                // Load image at full quality - let the label handle scaling
                images.append(pixmap);
            }
        }

        // Create dots for each image - small circles
        for (int i = 0; i < images.size(); ++i) {
            QPushButton* dot = new QPushButton();
            dot->setText("");  // Ensure no text
            dot->setFixedSize(10, 10);
            dot->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            dot->setFlat(true);
            dot->setStyleSheet(QString(
                "QPushButton {"
                "    background-color: %1;"
                "    border: none;"
                "    border-radius: 5px;"
                "    padding: 0px;"
                "    margin: 0px;"
                "}"
                "QPushButton:hover {"
                "    background-color: %2;"
                "}"
                "QPushButton:pressed {"
                "    background-color: %3;"
                "}"
            ).arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE)
             .arg(StreamUP::UIStyles::Colors::PRIMARY_ALPHA_30)
             .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR));
            
            connect(dot, &QPushButton::clicked, [this, i]() { goToImage(i); });
            dots.append(dot);
            dotsContainer->addWidget(dot);
        }

        dotsContainer->addStretch(); // Add stretch after dots to center them

        if (!images.isEmpty()) {
            updateImage();
            updateDots();
        }
    }

    void updateImage()
    {
        if (currentIndex < images.size()) {
            // Scale image to fit the container while maintaining aspect ratio and quality
            int availableWidth = imageContainer->width();
            int availableHeight = imageContainer->height();
            
            QPixmap scaledPixmap = images[currentIndex].scaled(
                QSize(availableWidth, availableHeight), 
                Qt::KeepAspectRatio, 
                Qt::SmoothTransformation
            );
            imageLabel->setPixmap(scaledPixmap);
        }
    }

    void updateDots()
    {
        for (int i = 0; i < dots.size(); ++i) {
            QString style = QString(
                "QPushButton {"
                "    background-color: %1;"
                "    border: none;"
                "    border-radius: 5px;"
                "    padding: 0px;"
                "    margin: 0px;"
                "}"
                "QPushButton:hover {"
                "    background-color: %2;"
                "}"
                "QPushButton:pressed {"
                "    background-color: %3;"
                "}"
            ).arg(i == currentIndex ? StreamUP::UIStyles::Colors::PRIMARY_COLOR : StreamUP::UIStyles::Colors::BORDER_SUBTLE)
             .arg(StreamUP::UIStyles::Colors::PRIMARY_HOVER)
             .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR);
            
            dots[i]->setStyleSheet(style);
        }
    }

    void setupTimer()
    {
        autoAdvanceTimer = new QTimer(this);
        connect(autoAdvanceTimer, &QTimer::timeout, this, &ThemeImageCarousel::nextImage);
        autoAdvanceTimer->start(4000); // Auto-advance every 4 seconds
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        autoAdvanceTimer->stop();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        autoAdvanceTimer->start(4000);
        QWidget::leaveEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        // Update image scaling when widget is resized
        if (!images.isEmpty()) {
            updateImage();
        }
    }


private:
    void showZoomedImage()
    {
        if (currentIndex >= images.size()) return;

        // Create modal zoom dialog
        QDialog* zoomDialog = new QDialog(this->window());
        zoomDialog->setWindowTitle("Theme Preview - Full Size");
        zoomDialog->setModal(true);
        zoomDialog->setStyleSheet(QString(
            "QDialog {"
            "    background: %1;"
            "    color: white;"
            "}"
        ).arg(StreamUP::UIStyles::Colors::BG_DARKEST));

        QVBoxLayout* layout = new QVBoxLayout(zoomDialog);
        layout->setContentsMargins(20, 20, 20, 20);

        // Full-size image label with fixed container size
        QLabel* fullImageLabel = new QLabel();
        fullImageLabel->setAlignment(Qt::AlignCenter);
        fullImageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        
        // Scale image to fit in fixed dialog size while maintaining aspect ratio
        QPixmap scaledPixmap = images[currentIndex].scaled(
            QSize(800, 600), 
            Qt::KeepAspectRatio, 
            Qt::SmoothTransformation
        );
        fullImageLabel->setPixmap(scaledPixmap);

        // Close instruction
        QLabel* instructionLabel = StreamUP::UIStyles::CreateStyledContent("Click anywhere or press ESC to close");
        instructionLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(fullImageLabel, 1);
        layout->addSpacing(10);
        layout->addWidget(instructionLabel);

        // Fixed dialog size
        zoomDialog->setFixedSize(860, 720);

        // Center on screen
        zoomDialog->move(
            (QApplication::primaryScreen()->geometry().width() - 860) / 2,
            (QApplication::primaryScreen()->geometry().height() - 720) / 2
        );

        // Install event filter for closing on click
        zoomDialog->installEventFilter(this);
        fullImageLabel->installEventFilter(this);

        // Store reference for event handling
        currentZoomDialog = zoomDialog;

        zoomDialog->exec();
        delete zoomDialog;
        currentZoomDialog = nullptr;
    }

    bool eventFilter(QObject* obj, QEvent* event) override
    {
        // Handle zoom dialog events
        if (currentZoomDialog && (obj == currentZoomDialog || obj->parent() == currentZoomDialog)) {
            if (event->type() == QEvent::MouseButtonPress || 
                (event->type() == QEvent::KeyPress && 
                 static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)) {
                currentZoomDialog->accept();
                return true;
            }
        }
        
        // Handle image label click for zoom
        if (obj == imageLabel && event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                showZoomedImage();
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

private:
    QList<QPixmap> images;
    QList<QPushButton*> dots;
    QLabel* imageLabel;
    QFrame* imageContainer;
    QPushButton* prevButton;
    QPushButton* nextButton;
    QHBoxLayout* dotsContainer;
    QTimer* autoAdvanceTimer;
    QDialog* currentZoomDialog = nullptr;
    int currentIndex;
};

void CreateThemeDialog()
{
    UIHelpers::ShowSingletonDialogOnUIThread("theme", []() -> QDialog* {
        // Create modern unified dialog
        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog("StreamUP - Theme");
        dialog->setModal(false);
        dialog->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        
        // Dialog will be managed by DialogManager
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Modern unified content area with scroll - everything inside
        QScrollArea* scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
        scrollArea->setWidgetResizable(true); // Ensure this is set for proper alignment
        scrollArea->setAlignment(Qt::AlignTop | Qt::AlignHCenter); // Keep content top-aligned when shorter than viewport
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Disable horizontal scrolling

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        contentWidget->setMaximumWidth(900); // Match dialog width minus padding
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
        
        // Theme images carousel section
        QGroupBox* previewGroup = StreamUP::UIStyles::CreateStyledGroupBox("Theme Preview", "primary");
        QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
        previewLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACE_12);
        
        // Create horizontal layout for buttons at group box edges
        QHBoxLayout* carouselControlLayout = new QHBoxLayout();
        carouselControlLayout->setContentsMargins(0, 0, 0, 0);
        carouselControlLayout->setSpacing(0);
        
        // Create carousel without navigation buttons (we'll add them externally)
        ThemeImageCarousel* carousel = new ThemeImageCarousel();
        
        // Create external navigation buttons for group box edges
        QPushButton* externalPrevButton = StreamUP::UIStyles::CreateStyledButton("❮", "neutral", 40);
        externalPrevButton->setFixedSize(40, 40);
        QPushButton* externalNextButton = StreamUP::UIStyles::CreateStyledButton("❯", "neutral", 40);
        externalNextButton->setFixedSize(40, 40);
        
        // Connect external buttons to carousel methods
        QObject::connect(externalPrevButton, &QPushButton::clicked, carousel, &ThemeImageCarousel::previousImage);
        QObject::connect(externalNextButton, &QPushButton::clicked, carousel, &ThemeImageCarousel::nextImage);
        
        // Layout: button at edge - carousel - button at edge
        carouselControlLayout->addWidget(externalPrevButton);
        carouselControlLayout->addWidget(carousel, 1);
        carouselControlLayout->addWidget(externalNextButton);
        
        previewLayout->addLayout(carouselControlLayout);
        
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
        
        // Apply flexible sizing that fits content - made wider for carousel
        StreamUP::UIStyles::ApplyDynamicSizing(dialog, 900, 1100, 700, 850);
        
        // Show dialog
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
        return dialog;
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
    return UIHelpers::DialogManager::IsSingletonDialogOpen("theme");
}

} // namespace ThemeWindow
} // namespace StreamUP

#include "theme-window.moc"
