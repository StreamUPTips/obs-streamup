#include "splash-screen.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "patch-notes-window.hpp"
#include "settings-manager.hpp"
#include "../utilities/error-handler.hpp"
#include "../utilities/http-client.hpp"
#include "../core/plugin-manager.hpp"
#include "../version.h"
#include <obs-module.h>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include <QGridLayout>
#include <QPixmap>
#include <QStyleOption>
#include <QStyle>
#include <QScrollBar>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QFrame>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QEnterEvent>
#include <QDialog>
#include <QKeyEvent>
#include <QScreen>
#include <util/platform.h>
#include <sstream>
#include <algorithm>

namespace StreamUP {
namespace SplashScreen {

// Splash dialog is now managed by DialogManager in ui-helpers

// Base carousel widget for images
class ImageCarousel : public QWidget
{
    Q_OBJECT

public:
    explicit ImageCarousel(QWidget* parent = nullptr) : QWidget(parent), currentIndex(0)
    {
        setFixedHeight(300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setupUI();
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

protected:
    // Pure virtual function - subclasses must implement
    virtual void loadImages() = 0;

    void setupUI()
    {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        // Image container
        imageContainer = new QFrame();
        imageContainer->setFixedHeight(250);
        imageContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        imageContainer->setStyleSheet("QFrame { background: transparent; border: none; }");

        QHBoxLayout* containerLayout = new QHBoxLayout(imageContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // Image label centered
        imageLabel = new QLabel();
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(false);
        imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        imageLabel->setCursor(Qt::PointingHandCursor);

        // Connect click event for zooming
        imageLabel->installEventFilter(this);

        containerLayout->addWidget(imageLabel, 1, Qt::AlignCenter);

        // Dots indicator - centered
        dotsContainer = new QHBoxLayout();
        dotsContainer->setContentsMargins(0, 0, 0, 0);
        dotsContainer->setSpacing(8);
        dotsContainer->addStretch();

        layout->addWidget(imageContainer);
        layout->addLayout(dotsContainer);
        layout->addStretch();
    }

    void createDotsForImages()
    {
        // Create dots for each image
        for (int i = 0; i < images.size(); ++i) {
            QPushButton* dot = new QPushButton();
            dot->setText("");
            dot->setFixedSize(10, 10);
            dot->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            dot->setFlat(true);
            dot->setStyleSheet(QString(
                "QPushButton {"
                "    background-color: %1;"
                "    border: none;"
                "    border-radius: 5px;"
                "}"
                "QPushButton:hover {"
                "    background-color: %2;"
                "}"
            ).arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
             .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY));

            connect(dot, &QPushButton::clicked, this, [this, i]() {
                goToImage(i);
            });

            dots.append(dot);
            dotsContainer->addWidget(dot);
        }

        dotsContainer->addStretch();

        if (!images.isEmpty()) {
            updateImage();
            updateDots();
        }
    }

    void updateImage()
    {
        if (currentIndex < images.size()) {
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
                "}"
                "QPushButton:hover {"
                "    background-color: %2;"
                "}"
            ).arg(i == currentIndex ? StreamUP::UIStyles::Colors::PRIMARY_COLOR : StreamUP::UIStyles::Colors::TEXT_MUTED)
             .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY);

            dots[i]->setStyleSheet(style);
        }
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        if (!images.isEmpty()) {
            updateImage();
        }
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

    void showZoomedImage()
    {
        if (currentIndex >= images.size()) return;

        // Create modal zoom dialog
        QDialog* zoomDialog = new QDialog(this->window());
        zoomDialog->setWindowTitle("Image Preview - Full Size");
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

    // Member variables
    QList<QPixmap> images;
    QList<QPushButton*> dots;
    QLabel* imageLabel;
    QFrame* imageContainer;
    QHBoxLayout* dotsContainer;
    QDialog* currentZoomDialog = nullptr;
    int currentIndex;
};

// Theme carousel - loads specific theme images
class ThemeImageCarousel : public ImageCarousel
{
    Q_OBJECT

public:
    explicit ThemeImageCarousel(QWidget* parent = nullptr) : ImageCarousel(parent)
    {
        loadImages();
    }

protected:
    void loadImages() override
    {
        // Load theme images
        QStringList imageFiles = {"obs-theme-1.png", "obs-theme-2.png", "obs-theme-3.png", "obs-theme-4.png"};

        for (const QString& fileName : imageFiles) {
            QString imagePath = QString(":/images/misc/%1").arg(fileName);
            QPixmap pixmap(imagePath);

            if (!pixmap.isNull()) {
                images.append(pixmap);
            }
        }

        createDotsForImages();
    }
};

// Source Explorer carousel - dynamically loads numbered images
class SourceExplorerCarousel : public ImageCarousel
{
    Q_OBJECT

public:
    explicit SourceExplorerCarousel(QWidget* parent = nullptr) : ImageCarousel(parent)
    {
        loadImages();
    }

protected:
    void loadImages() override
    {
        // Dynamically load source-explorer-1.png, source-explorer-2.png, etc.
        // Keep loading until we don't find the next numbered image
        int imageNumber = 1;
        bool foundImage = true;

        while (foundImage) {
            QString imagePath = QString(":/images/misc/source-explorer-%1.png").arg(imageNumber);
            QPixmap pixmap(imagePath);

            if (!pixmap.isNull()) {
                images.append(pixmap);
                imageNumber++;
            } else {
                foundImage = false;
            }
        }

        // If no images found, show placeholder
        if (images.isEmpty()) {
            // Create a placeholder image
            QPixmap placeholder(700, 250);
            placeholder.fill(QColor(StreamUP::UIStyles::Colors::BG_SECONDARY));
            images.append(placeholder);
        }

        createDotsForImages();
    }
};

// Supporter data structures
struct Supporter {
    QString userId;
    QString displayName;
    
    bool operator<(const Supporter& other) const {
        return displayName.toLower() < other.displayName.toLower();
    }
};

struct SupportersData {
    std::vector<Supporter> andiSupporters;
    std::vector<Supporter> streamupSupporters;
    bool loaded = false;
    QString errorMessage;
};

static SupportersData supportersData;

// Monthly supporters list (you'll want to update this regularly)
// Note: Currently using dynamic API loading instead of static list
[[maybe_unused]] static const char* MONTHLY_SUPPORTERS = R"(
<h3>💝 Thank You to Our Monthly Supporters!</h3>
<p><i>Your support makes StreamUP possible and helps us continue developing amazing features!</i></p>

<h4>🌟 Diamond Supporters ($25+)</h4>
<ul>
<li>StreamerName1</li>
<li>ContentCreator2</li>
<li>TechEnthusiast3</li>
</ul>

<h4>💎 Gold Supporters ($10+)</h4>
<ul>
<li>Supporter1</li>
<li>Supporter2</li>
<li>Supporter3</li>
<li>Supporter4</li>
<li>Supporter5</li>
</ul>

<h4>⭐ Silver Supporters ($5+)</h4>
<ul>
<li>Fan1</li>
<li>Fan2</li>
<li>Fan3</li>
<li>Fan4</li>
<li>Fan5</li>
<li>Fan6</li>
<li>Fan7</li>
<li>Fan8</li>
</ul>

<p><i>And many more amazing supporters who make this project possible! ❤️</i></p>
)";

std::string ProcessInlineFormatting(const std::string& text)
{
    std::string result = text;
    
    // Convert **bold** text to <b>
    size_t pos = 0;
    while ((pos = result.find("**", pos)) != std::string::npos) {
        result.replace(pos, 2, "<b>");
        size_t endPos = result.find("**", pos + 3);
        if (endPos != std::string::npos) {
            result.replace(endPos, 2, "</b>");
            pos = endPos + 4;
        } else {
            pos += 3;
        }
    }
    
    // Convert *italic* text to <i> (but skip already processed bold)
    pos = 0;
    while ((pos = result.find("*", pos)) != std::string::npos) {
        // Skip if it's part of <b> or </b> tags
        if (pos >= 1 && result.substr(pos-1, 3) == "<b>") {
            pos++;
            continue;
        }
        if (pos + 4 <= result.length() && result.substr(pos, 4) == "</b>") {
            pos += 4;
            continue;
        }
        
        result.replace(pos, 1, "<i>");
        size_t endPos = result.find("*", pos + 3);
        if (endPos != std::string::npos) {
            result.replace(endPos, 1, "</i>");
            pos = endPos + 4;
        } else {
            pos += 3;
        }
    }
    
    // Convert `code` blocks to <code>
    pos = 0;
    while ((pos = result.find("`", pos)) != std::string::npos) {
        result.replace(pos, 1, "<code style=\"background: #313244; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #cdd6f4;\">");
        size_t endPos = result.find("`", pos + 10);
        if (endPos != std::string::npos) {
            result.replace(endPos, 1, "</code>");
            pos = endPos + 7;
        } else {
            pos += 10;
        }
    }
    
    // Convert [link text](url) to <a href="url">link text</a>
    pos = 0;
    while ((pos = result.find("[", pos)) != std::string::npos) {
        size_t closePos = result.find("]", pos);
        size_t parenPos = result.find("(", closePos);
        size_t closeParenPos = result.find(")", parenPos);
        
        if (closePos != std::string::npos && parenPos != std::string::npos && 
            closeParenPos != std::string::npos && parenPos == closePos + 1) {
            
            std::string linkText = result.substr(pos + 1, closePos - pos - 1);
            std::string linkUrl = result.substr(parenPos + 1, closeParenPos - parenPos - 1);
            std::string replacement = "<a href=\"" + linkUrl + "\" style=\"color: #89b4fa; text-decoration: underline;\">" + linkText + "</a>";
            
            result.replace(pos, closeParenPos - pos + 1, replacement);
            pos += replacement.length();
        } else {
            pos++;
        }
    }
    
    return result;
}

void LoadSupportersData() 
{
    StreamUP::HttpClient::MakeAsyncGetRequest("https://streamup.tips/api/supporters", 
        [](const std::string& /* url */, const std::string& response, bool success) {
            if (!success) {
                supportersData.errorMessage = "Failed to fetch supporters data";
                supportersData.loaded = true;
                ErrorHandler::LogWarning("Failed to fetch supporters API: " + response, ErrorHandler::Category::Network);
                return;
            }

            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(response.c_str(), &parseError);
            
            if (parseError.error != QJsonParseError::NoError) {
                supportersData.errorMessage = "Failed to parse supporters JSON: " + parseError.errorString();
                supportersData.loaded = true;
                ErrorHandler::LogWarning("Failed to parse supporters JSON: " + parseError.errorString().toStdString(), ErrorHandler::Category::Network);
                return;
            }

            QJsonObject rootObj = doc.object();
            
            // Parse Andi supporters
            if (rootObj.contains("andilippi") && rootObj["andilippi"].isObject()) {
                QJsonObject andiObj = rootObj["andilippi"].toObject();
                if (andiObj.contains("supporters") && andiObj["supporters"].isArray()) {
                    QJsonArray andiSupportersArray = andiObj["supporters"].toArray();
                    for (const QJsonValue value : andiSupportersArray) {
                        if (value.isObject()) {
                            QJsonObject supporterObj = value.toObject();
                            Supporter supporter;
                            supporter.userId = supporterObj["userId"].toString();
                            supporter.displayName = supporterObj["displayName"].toString();
                            supportersData.andiSupporters.push_back(supporter);
                        }
                    }
                }
            }

            // Parse StreamUP supporters
            if (rootObj.contains("streamUP") && rootObj["streamUP"].isObject()) {
                QJsonObject streamupObj = rootObj["streamUP"].toObject();
                if (streamupObj.contains("supporters") && streamupObj["supporters"].isArray()) {
                    QJsonArray streamupSupportersArray = streamupObj["supporters"].toArray();
                    for (const QJsonValue value : streamupSupportersArray) {
                        if (value.isObject()) {
                            QJsonObject supporterObj = value.toObject();
                            Supporter supporter;
                            supporter.userId = supporterObj["userId"].toString();
                            supporter.displayName = supporterObj["displayName"].toString();
                            supportersData.streamupSupporters.push_back(supporter);
                        }
                    }
                }
            }

            // Sort supporters alphabetically
            std::sort(supportersData.andiSupporters.begin(), supportersData.andiSupporters.end());
            std::sort(supportersData.streamupSupporters.begin(), supportersData.streamupSupporters.end());

            supportersData.loaded = true;
            
            ErrorHandler::LogInfo(QString("Loaded %1 Andi supporters and %2 StreamUP supporters")
                .arg(supportersData.andiSupporters.size())
                .arg(supportersData.streamupSupporters.size()).toStdString(), ErrorHandler::Category::Network);
        });
}

QString GenerateSupportersHTML() 
{
    if (!supportersData.loaded) {
        return R"(
<div style="color: #e9d5ff; line-height: 1.4; font-size: 13px;">
    <p style="margin: 0; color: #d8b4fe;">Loading supporters...</p>
</div>
        )";
    }

    if (!supportersData.errorMessage.isEmpty()) {
        return QString(R"(
<div style="color: #e9d5ff; line-height: 1.4; font-size: 13px;">
    <p style="margin: 0; color: #fca5a5;">Unable to load supporters at this time.</p>
</div>
        )");
    }

    QString html = R"(<div style="color: #e9d5ff; line-height: 1.4; font-size: 13px;">)";

    // Andi's supporters section
    if (!supportersData.andiSupporters.empty()) {
        html += R"(<h4 style="color: #fbbf24; margin: 12px 0 8px 0; font-size: 14px;">💛 Andi's Supporters</h4>)";
        html += R"(<p style="margin: 8px 0; line-height: 2.0;">)";

        for (size_t i = 0; i < supportersData.andiSupporters.size(); ++i) {
            const auto& supporter = supportersData.andiSupporters[i];
            html += QString(R"(<span style="background-color: rgba(251, 191, 36, 0.2); color: #fde68a; padding: 3px 8px; border-radius: 4px; font-size: 12px; font-weight: 500; white-space: nowrap;">%1</span>)")
                .arg(supporter.displayName.toHtmlEscaped());

            // Add space between badges
            if (i < supportersData.andiSupporters.size() - 1) {
                html += " &nbsp; ";
            }
        }
        html += R"(</p>)";
    }

    // StreamUP supporters section
    if (!supportersData.streamupSupporters.empty()) {
        html += R"(<h4 style="color: #a855f7; margin: 12px 0 8px 0; font-size: 14px;">💜 StreamUP Supporters</h4>)";
        html += R"(<p style="margin: 8px 0; line-height: 2.0;">)";

        for (size_t i = 0; i < supportersData.streamupSupporters.size(); ++i) {
            const auto& supporter = supportersData.streamupSupporters[i];
            html += QString(R"(<span style="background-color: rgba(168, 85, 247, 0.2); color: #e9d5ff; padding: 3px 8px; border-radius: 4px; font-size: 12px; font-weight: 500; white-space: nowrap;">%1</span>)")
                .arg(supporter.displayName.toHtmlEscaped());

            // Add space between badges
            if (i < supportersData.streamupSupporters.size() - 1) {
                html += " &nbsp; ";
            }
        }
        html += R"(</p>)";
    }

    // If both lists are empty
    if (supportersData.andiSupporters.empty() && supportersData.streamupSupporters.empty()) {
        html += R"(<p style="margin: 0; color: #d8b4fe; font-style: italic;">No public supporters to display at this time.</p>)";
    }

    // Opt-in message
    html += R"(<div style="margin: 16px 0 0 0; padding: 12px; background: rgba(168, 85, 247, 0.1); border-left: 3px solid #a855f7; border-radius: 4px;">)";
    html += R"(<p style="margin: 0; color: #e9d5ff; font-size: 12px;">)";
    html += R"(<strong>If you're a supporter and your name is not here:</strong><br>)";
    html += R"(This is an opt-in feature which you can enable in your account settings.<br>)";
    html += R"(You can opt-in right now and choose exactly how your name will appear<br>)";
    html += R"(👉 <a href="https://streamup.tips/Identity/Account/Manage" style="color: #c4b5fd; text-decoration: underline;">https://streamup.tips/Identity/Account/Manage</a>)";
    html += R"(</p></div>)";

    html += R"(</div>)";
    return html;
}

std::string LoadLocalSupportInfo()
{
    QFile file(":support-info.md");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ErrorHandler::LogWarning("Failed to open local support info file", ErrorHandler::Category::UI);
        return "";
    }

    QTextStream in(&file);
    QString markdownContent = in.readAll();
    file.close();

    if (markdownContent.isEmpty()) {
        ErrorHandler::LogWarning("Local support info file is empty", ErrorHandler::Category::UI);
        return "";
    }

    // Convert markdown to HTML - line by line processing for better control
    std::string result;
    std::istringstream stream(markdownContent.toStdString());
    std::string line;
    bool inList = false;
    
    while (std::getline(stream, line)) {
        // Skip empty lines to reduce spacing
        if (line.empty()) {
            continue;
        }
        
        // Convert horizontal rules (--- or ***)
        if (line == "---" || line == "***") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            result += "<hr style=\"border: none; border-top: 1px solid #313244; margin: 16px 0;\">\n";
            continue;
        }
        
        // Convert headers with custom colors for support section
        if (line.length() >= 4 && line.substr(0, 4) == "### ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(4);
            result += "<h4 style=\"color: #f3e8ff; margin: 12px 0 6px 0; font-size: 14px;\">" + headerText + "</h4>\n";
        }
        else if (line.length() >= 3 && line.substr(0, 3) == "## ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(3);
            // Use different colors for "What Your Support" vs "Monthly Supporter Benefits"
            if (headerText.find("What Your Support") != std::string::npos) {
                result += "<h3 style=\"color: #fbbf24; margin: 16px 0 8px 0; font-size: 16px;\">" + headerText + "</h3>\n";
            } else if (headerText.find("Monthly Supporter") != std::string::npos) {
                result += "<h3 style=\"color: #a78bfa; margin: 16px 0 8px 0; font-size: 16px;\">" + headerText + "</h3>\n";
            } else {
                result += "<h3 style=\"color: #a855f7; margin: 16px 0 8px 0; font-size: 16px;\">" + headerText + "</h3>\n";
            }
        }
        else if (line.length() >= 2 && line.substr(0, 2) == "# ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(2);
            result += "<h2 style=\"color: #cdd6f4; margin: 18px 0 10px 0; font-size: 18px; font-weight: 600;\">" + headerText + "</h2>\n";
        }
        // Convert list items
        else if (line.length() >= 2 && line.substr(0, 2) == "- ") {
            if (!inList) {
                result += "<ul style=\"margin: 8px 0; padding-left: 20px;\">\n";
                inList = true;
            }
            std::string content = line.substr(2);
            
            // Process inline formatting in list content
            content = ProcessInlineFormatting(content);
            
            result += "<li style=\"margin: 3px 0;\">" + content + "</li>\n";
        }
        // Regular paragraph
        else {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            
            // Process inline formatting
            std::string processedLine = ProcessInlineFormatting(line);
            result += "<p style=\"margin: 8px 0;\">" + processedLine + "</p>\n";
        }
    }
    
    // Close any open list
    if (inList) {
        result += "</ul>\n";
    }
    
    std::string formattedNotes = "<div style=\"color: #dbeafe; line-height: 1.4; font-size: 12px;\">\n";
    formattedNotes += result;
    formattedNotes += "</div>";
    
    ErrorHandler::LogInfo("Successfully loaded local support info", ErrorHandler::Category::UI);
    return formattedNotes;
}

std::string LoadLocalPluginInfo()
{
    QFile file(":plugin-info.md");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ErrorHandler::LogWarning("Failed to open local plugin info file", ErrorHandler::Category::UI);
        return "";
    }

    QTextStream in(&file);
    QString markdownContent = in.readAll();
    file.close();

    if (markdownContent.isEmpty()) {
        ErrorHandler::LogWarning("Local plugin info file is empty", ErrorHandler::Category::UI);
        return "";
    }

    // Convert markdown to HTML - line by line processing for better control
    std::string result;
    std::istringstream stream(markdownContent.toStdString());
    std::string line;
    bool inList = false;
    
    while (std::getline(stream, line)) {
        // Skip empty lines to reduce spacing
        if (line.empty()) {
            continue;
        }
        
        // Convert horizontal rules (--- or ***)
        if (line == "---" || line == "***") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            result += "<hr style=\"border: none; border-top: 1px solid #313244; margin: 16px 0;\">\n";
            continue;
        }
        
        // Convert headers
        if (line.length() >= 4 && line.substr(0, 4) == "### ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(4);
            result += "<h4 style=\"color: #f3e8ff; margin: 12px 0 6px 0; font-size: 14px;\">" + headerText + "</h4>\n";
        }
        else if (line.length() >= 3 && line.substr(0, 3) == "## ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(3);
            result += "<h3 style=\"color: #a855f7; margin: 16px 0 8px 0; font-size: 16px;\">" + headerText + "</h3>\n";
        }
        else if (line.length() >= 2 && line.substr(0, 2) == "# ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(2);
            result += "<h2 style=\"color: #89b4fa; margin: 18px 0 10px 0; font-size: 18px; font-weight: 600;\">" + headerText + "</h2>\n";
        }
        // Convert list items
        else if (line.length() >= 2 && line.substr(0, 2) == "- ") {
            if (!inList) {
                result += "<ul style=\"margin: 8px 0; padding-left: 20px;\">\n";
                inList = true;
            }
            std::string content = line.substr(2);
            
            // Process inline formatting in list content
            content = ProcessInlineFormatting(content);
            
            result += "<li style=\"margin: 3px 0;\">" + content + "</li>\n";
        }
        // Regular paragraph
        else {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            
            // Process inline formatting
            std::string processedLine = ProcessInlineFormatting(line);
            result += "<p style=\"margin: 8px 0;\">" + processedLine + "</p>\n";
        }
    }
    
    // Close any open list
    if (inList) {
        result += "</ul>\n";
    }
    
    std::string formattedNotes = "<div style=\"color: #cdd6f4; line-height: 1.4; font-size: 12px;\">\n";
    formattedNotes += result;
    formattedNotes += "</div>";
    
    ErrorHandler::LogInfo("Successfully loaded local plugin info", ErrorHandler::Category::UI);
    return formattedNotes;
}

std::string GetWelcomeMessage()
{
    return std::string(R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <h2 style="color: #89b4fa; margin: 18px 0 12px 0; font-size: 20px; font-weight: 600;">🎉 Welcome to StreamUP!</h2>

    <p style="margin: 12px 0; color: #cdd6f4; font-size: 15px;">
        <strong>Thank you for installing StreamUP!</strong> You've just unlocked a powerful toolkit designed to supercharge your OBS Studio experience.
    </p>

    <h3 style="color: #a855f7; margin: 16px 0 8px 0; font-size: 16px;">✨ What is StreamUP?</h3>
    <p style="margin: 8px 0;">
        StreamUP is an advanced plugin that provides essential tools for content creators, streamers, and anyone using OBS Studio. From source management and plugin updates to WebSocket API control and automated workflows - StreamUP streamlines your creative process.
    </p>

    <h3 style="color: #a855f7; margin: 16px 0 8px 0; font-size: 16px;">📚 Getting Started</h3>
    <p style="margin: 8px 0;">
        Ready to dive in? Check out our comprehensive documentation to learn about all the amazing features at your fingertips:
    </p>
    <p style="margin: 8px 0;">
        <a href="https://streamup.doras.click/docs" style="color: #89b4fa; text-decoration: underline; font-weight: 500;">📖 Read the Documentation</a>
    </p>

    <h3 style="color: #fbbf24; margin: 16px 0 8px 0; font-size: 16px;">💖 Support Our Development</h3>
    <p style="margin: 8px 0;">
        StreamUP is a passion project created with love for the OBS community. If you find it useful, please consider supporting our development efforts. Your support helps us continue adding new features and maintaining compatibility with the latest OBS versions.
    </p>
    <p style="margin: 8px 0;">
        Every contribution, no matter how small, makes a real difference and keeps this project thriving!
    </p>

    <div style="margin: 16px 0; padding: 12px; border-left: 4px solid #a855f7; border-radius: 4px;">
        <p style="margin: 0; color: #e9d5ff; font-style: italic;">
            💜 Love from <strong>Andi (Andilippi)</strong><br>
            <a href="https://doras.to/andi" style="color: #c4b5fd; text-decoration: underline;">https://doras.to/andi</a>
        </p>
    </div>
</div>
    )");
}

std::string LoadLocalPatchNotes()
{
    QFile file(":patch-notes-summary.md");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ErrorHandler::LogWarning("Failed to open local patch notes file", ErrorHandler::Category::UI);
        return "";
    }

    QTextStream in(&file);
    QString markdownContent = in.readAll();
    file.close();

    if (markdownContent.isEmpty()) {
        ErrorHandler::LogWarning("Local patch notes file is empty", ErrorHandler::Category::UI);
        return "";
    }

    // Convert markdown to HTML - line by line processing for better control
    std::string result;
    std::istringstream stream(markdownContent.toStdString());
    std::string line;
    bool inList = false;
    
    while (std::getline(stream, line)) {
        // Skip empty lines to reduce spacing
        if (line.empty()) {
            continue;
        }
        
        // Convert horizontal rules (--- or ***)
        if (line == "---" || line == "***") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            result += "<hr style=\"border: none; border-top: 1px solid #313244; margin: 16px 0;\">\n";
            continue;
        }
        
        // Convert headers
        if (line.length() >= 4 && line.substr(0, 4) == "### ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(4);
            result += "<h4 style=\"color: #f3e8ff; margin: 12px 0 6px 0; font-size: 14px;\">" + headerText + "</h4>\n";
        }
        else if (line.length() >= 3 && line.substr(0, 3) == "## ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(3);
            result += "<h3 style=\"color: #a855f7; margin: 16px 0 8px 0; font-size: 16px;\">" + headerText + "</h3>\n";
        }
        else if (line.length() >= 2 && line.substr(0, 2) == "# ") {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            std::string headerText = line.substr(2);
            result += "<h2 style=\"color: #89b4fa; margin: 18px 0 10px 0; font-size: 18px; font-weight: 600;\">" + headerText + "</h2>\n";
        }
        // Convert list items
        else if (line.length() >= 2 && line.substr(0, 2) == "- ") {
            if (!inList) {
                result += "<ul style=\"margin: 8px 0; padding-left: 20px;\">\n";
                inList = true;
            }
            std::string content = line.substr(2);
            
            // Process inline formatting in list content
            content = ProcessInlineFormatting(content);
            
            result += "<li style=\"margin: 3px 0;\">" + content + "</li>\n";
        }
        // Regular paragraph
        else {
            if (inList) {
                result += "</ul>\n";
                inList = false;
            }
            
            // Process inline formatting
            std::string processedLine = ProcessInlineFormatting(line);
            result += "<p style=\"margin: 8px 0;\">" + processedLine + "</p>\n";
        }
    }
    
    // Close any open list
    if (inList) {
        result += "</ul>\n";
    }
    
    std::string formattedNotes = "<div style=\"color: #cdd6f4; line-height: 1.4; font-size: 12px;\">\n";
    formattedNotes += result;
    formattedNotes += "</div>";
    
    ErrorHandler::LogInfo("Successfully loaded local patch notes", ErrorHandler::Category::UI);
    return formattedNotes;
}

std::string GetPatchNotes()
{
    // Load patch notes from local markdown file
    std::string localNotes = LoadLocalPatchNotes();
    if (!localNotes.empty()) {
        return localNotes;
    }
    
    // Fallback to static notes if local file fails
    ErrorHandler::LogWarning("Using fallback static patch notes", ErrorHandler::Category::UI);
    
    return std::string(R"(
<div style="color: #cdd6f4; line-height: 1.3; font-size: 12px;">
    <h2 style="color: #89b4fa; margin: 12px 0 8px 0;">What's New in )") + PROJECT_VERSION + R"(</h2>
    <p style="margin: 0 0 8px 0; color: #fbbf24; font-style: italic;">⚠️ Unable to load patch notes from local file</p>
    <p style="margin: 0;"><b>🚀 Recent Features:</b> WebSocket API, Plugin Manager, Notifications, Settings UI</p>
</div>
    )";
}

ShowCondition CheckSplashCondition()
{
    obs_data_t* settings = SettingsManager::LoadSettings();
    if (!settings) {
        return ShowCondition::FirstInstall;
    }

    // Check if this is first install
    if (!obs_data_has_user_value(settings, "last_version_shown")) {
        obs_data_release(settings);
        return ShowCondition::FirstInstall;
    }

    // Check if version has changed
    const char* lastVersion = obs_data_get_string(settings, "last_version_shown");
    if (strcmp(lastVersion, PROJECT_VERSION) != 0) {
        obs_data_release(settings);
        return ShowCondition::VersionUpdate;
    }

    obs_data_release(settings);
    return ShowCondition::Never;
}

void UpdateVersionTracking()
{
    obs_data_t* settings = SettingsManager::LoadSettings();
    if (!settings) {
        settings = obs_data_create();
    }

    obs_data_set_string(settings, "last_version_shown", PROJECT_VERSION);
    SettingsManager::SaveSettings(settings);
    obs_data_release(settings);
}

void CreateSplashDialog(ShowCondition condition)
{
    (void)condition; // Suppress unused parameter warning

    // Use DialogManager for singleton pattern, but condition logic still needed
    if (UIHelpers::DialogManager::IsSingletonDialogOpen("splash")) {
        return; // Dialog already handled by DialogManager
    }

    // Start loading supporters data if not already loaded or in progress
    if (!supportersData.loaded && supportersData.errorMessage.isEmpty()) {
        LoadSupportersData();
    }

    UIHelpers::ShowSingletonDialogOnUIThread("splash", []() -> QDialog* {
        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("StreamUP.SplashScreen.Title"));
        dialog->setModal(false);
        dialog->setFixedSize(800, 600);
        
        // Ensure version tracking is updated when dialog closes (any way)
        QObject::connect(dialog, &QDialog::finished, []() {
            UpdateVersionTracking();
        });
        
        QVBoxLayout* mainLayout = StreamUP::UIStyles::GetDialogContentLayout(dialog);

        // Early Access Banner - Clickable banner at the very top
        // Define the clickable banner class first
        class ClickableBanner : public QWidget {
        public:
            ClickableBanner(QWidget* parent = nullptr) : QWidget(parent) {}
        protected:
            void mousePressEvent(QMouseEvent* event) override {
                if (event->button() == Qt::LeftButton) {
                    // Show early access features dialog
                    ShowEarlyAccessDialog();
                }
                QWidget::mousePressEvent(event);
            }

            static void ShowEarlyAccessDialog() {
                StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
                    QDialog* earlyAccessDialog = StreamUP::UIStyles::CreateStyledDialog("StreamUP \xe2\x80\xa2 Early Access");
                    earlyAccessDialog->setModal(true);
                    earlyAccessDialog->setFixedSize(800, 600);

                    QVBoxLayout* dialogLayout = StreamUP::UIStyles::GetDialogContentLayout(earlyAccessDialog);

                    // Create scroll area that fills the whole dialog
                    QScrollArea* scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
                    QWidget* scrollContent = new QWidget();
                    scrollContent->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
                    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
                    contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_LARGE,
                                                     StreamUP::UIStyles::Sizes::SPACING_MEDIUM,
                                                     StreamUP::UIStyles::Sizes::PADDING_LARGE,
                                                     StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
                    contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

                    // Header - using HTML like the about window
                    QString headerHTML = R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <h2 style="color: #fbbf24; margin: 18px 0 12px 0; font-size: 20px; font-weight: 600; text-align: center;">🌟 Get Exclusive Early Access to New Features!</h2>
    <p style="margin: 12px 0; color: #cdd6f4; font-size: 15px; text-align: center;">
        Support StreamUP development and unlock cutting-edge features before they're released to the public:
    </p>
</div>
                    )";
                    QLabel* headerLabel = UIHelpers::CreateRichTextLabel(headerHTML, false, true, Qt::Alignment(), true);
                    contentLayout->addWidget(headerLabel);

                    // Source Explorer Section — flat
                    contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Source Explorer (Early Access)"));
                    QVBoxLayout* sourceExplorerLayout = new QVBoxLayout();
                    sourceExplorerLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

                    // Source Explorer carousel with navigation buttons
                    QHBoxLayout* explorerCarouselControlLayout = new QHBoxLayout();
                    explorerCarouselControlLayout->setContentsMargins(0, 0, 0, 0);
                    explorerCarouselControlLayout->setSpacing(0);

                    // Create carousel
                    SourceExplorerCarousel* explorerCarousel = new SourceExplorerCarousel();

                    // Create external navigation buttons
                    QPushButton* explorerPrevButton = StreamUP::UIStyles::CreateStyledButton("❮", "neutral", 40);
                    explorerPrevButton->setFixedSize(40, 40);
                    QPushButton* explorerNextButton = StreamUP::UIStyles::CreateStyledButton("❯", "neutral", 40);
                    explorerNextButton->setFixedSize(40, 40);

                    // Connect buttons to carousel
                    QObject::connect(explorerPrevButton, &QPushButton::clicked, explorerCarousel, &SourceExplorerCarousel::previousImage);
                    QObject::connect(explorerNextButton, &QPushButton::clicked, explorerCarousel, &SourceExplorerCarousel::nextImage);

                    // Layout: button - carousel - button
                    explorerCarouselControlLayout->addWidget(explorerPrevButton);
                    explorerCarouselControlLayout->addWidget(explorerCarousel, 1);
                    explorerCarouselControlLayout->addWidget(explorerNextButton);

                    sourceExplorerLayout->addLayout(explorerCarouselControlLayout);

                    QString sourceExplorerFeatures = R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <p style="margin: 8px 0;">
        A powerful new tool for advanced source management and exploration:
    </p>
    <ul style="margin: 8px 0; padding-left: 20px;">
        <li style="margin: 4px 0;">All source controls in one easy to use dock. Access and edit Source information, transform, properties, filters and more, no more need to open sub menus etc.</li>
        <li style="margin: 4px 0;">Open and edit multiple filters at the same time!</li>
        <li style="margin: 4px 0;">Edit sources on a different scene without having to use Studio mode.</li>
        <li style="margin: 4px 0;">Easily copy settings as websocket or CPH (Streamer.Bot) commands.</li>
        <li style="margin: 4px 0;">Easy access to buttons for flipping, rotating and more transform for sources.</li>
        <li style="margin: 4px 0;">Split each tab into its own dock if needed.</li>
        <li style="margin: 4px 0;">Easy button to screenshot selected source.</li>
    </ul>
</div>
                    )";
                    QLabel* sourceExplorerFeaturesLabel = StreamUP::UIHelpers::CreateRichTextLabel(sourceExplorerFeatures, false, true, Qt::Alignment(), true);
                    sourceExplorerLayout->addWidget(sourceExplorerFeaturesLabel);
                    contentLayout->addLayout(sourceExplorerLayout);

                    // Theme Section — flat
                    contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Exclusive Supporter Theme"));
                    QVBoxLayout* themeLayout = new QVBoxLayout();
                    themeLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

                    // Theme carousel with navigation buttons
                    QHBoxLayout* carouselControlLayout = new QHBoxLayout();
                    carouselControlLayout->setContentsMargins(0, 0, 0, 0);
                    carouselControlLayout->setSpacing(0);

                    // Create carousel
                    ThemeImageCarousel* carousel = new ThemeImageCarousel();

                    // Create external navigation buttons
                    QPushButton* prevButton = StreamUP::UIStyles::CreateStyledButton("❮", "neutral", 40);
                    prevButton->setFixedSize(40, 40);
                    QPushButton* nextButton = StreamUP::UIStyles::CreateStyledButton("❯", "neutral", 40);
                    nextButton->setFixedSize(40, 40);

                    // Connect buttons to carousel
                    QObject::connect(prevButton, &QPushButton::clicked, carousel, &ThemeImageCarousel::previousImage);
                    QObject::connect(nextButton, &QPushButton::clicked, carousel, &ThemeImageCarousel::nextImage);

                    // Layout: button - carousel - button
                    carouselControlLayout->addWidget(prevButton);
                    carouselControlLayout->addWidget(carousel, 1);
                    carouselControlLayout->addWidget(nextButton);

                    themeLayout->addLayout(carouselControlLayout);

                    QString themeDescription = R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <p style="margin: 8px 0;">
        Customize your OBS experience with our exclusive supporter theme. A more modern feel featuring refined colors, enhanced contrast, and beautiful styling designed to make your workflow even more enjoyable.
    </p>
</div>
                    )";
                    QLabel* themeDescLabel = StreamUP::UIHelpers::CreateRichTextLabel(themeDescription, false, true, Qt::Alignment(), true);
                    themeLayout->addWidget(themeDescLabel);
                    contentLayout->addLayout(themeLayout);

                    // Widgets Section — flat
                    contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Access to StreamUP.tips Premium Content"));
                    QVBoxLayout* widgetsLayout = new QVBoxLayout();

                    QString widgetsHTML = R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <p style="margin: 8px 0;">
        Get full access to all our paid widgets, tools, alerts, effects and more on <a href="https://streamup.tips" style="color: #89b4fa; text-decoration: underline;">StreamUP.tips</a>
    </p>
    <p style="margin: 8px 0;">
        Premium stream widgets, professional alerts, custom effects, and powerful tools to enhance your streaming experience.
    </p>
</div>
                    )";
                    QLabel* widgetsLabel = StreamUP::UIHelpers::CreateRichTextLabel(widgetsHTML, false, true, Qt::Alignment(), true);
                    widgetsLayout->addWidget(widgetsLabel);
                    contentLayout->addLayout(widgetsLayout);

                    // Community message — flat
                    contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Your Support Makes a Difference"));
                    QVBoxLayout* communityLayout = new QVBoxLayout();

                    QString communityHTML = R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <p style="margin: 8px 0;">
        Every contribution helps us continue developing amazing features for the entire OBS community. You're not just getting early access—you're investing in the future of StreamUP!
    </p>
</div>
                    )";
                    QLabel* communityLabel = StreamUP::UIHelpers::CreateRichTextLabel(communityHTML, false, true, Qt::Alignment(), true);
                    communityLayout->addWidget(communityLabel);
                    contentLayout->addLayout(communityLayout);

                    scrollArea->setWidget(scrollContent);
                    dialogLayout->addWidget(scrollArea);

                    // Footer button area
                    QVBoxLayout* footerLayout = StreamUP::UIStyles::GetDialogFooterLayout(earlyAccessDialog);

                    // Support Buttons
                    QHBoxLayout* supportButtonLayout = new QHBoxLayout();
                    supportButtonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

                    QPushButton* kofiBtn = StreamUP::UIStyles::CreateStyledButton("Ko-Fi", "warning");
                    kofiBtn->setIcon(QIcon(":images/icons/social/kofi.svg"));
                    kofiBtn->setIconSize(QSize(16, 16));
                    kofiBtn->setToolTip("Support us on Ko-Fi");
                    QObject::connect(kofiBtn, &QPushButton::clicked, []() {
                        QDesktopServices::openUrl(QUrl("https://ko-fi.com/streamup"));
                    });

                    QPushButton* patreonBtn = StreamUP::UIStyles::CreateStyledButton("Patreon", "warning");
                    patreonBtn->setIcon(QIcon(":images/icons/social/patreon.svg"));
                    patreonBtn->setIconSize(QSize(16, 16));
                    patreonBtn->setToolTip("Support us on Patreon");
                    QObject::connect(patreonBtn, &QPushButton::clicked, []() {
                        QDesktopServices::openUrl(QUrl("https://www.patreon.com/streamup"));
                    });

                    QPushButton* andiBtn = StreamUP::UIStyles::CreateStyledButton("Support Andi", "info");
                    andiBtn->setIcon(QIcon(":images/icons/social/doras.svg"));
                    andiBtn->setIconSize(QSize(16, 16));
                    andiBtn->setToolTip("Support Andi Personally");
                    QObject::connect(andiBtn, &QPushButton::clicked, []() {
                        QDesktopServices::openUrl(QUrl("https://andilippi.co.uk/en-gbp"));
                    });

                    supportButtonLayout->addStretch();
                    supportButtonLayout->addWidget(kofiBtn);
                    supportButtonLayout->addWidget(patreonBtn);
                    supportButtonLayout->addWidget(andiBtn);
                    supportButtonLayout->addStretch();

                    footerLayout->addLayout(supportButtonLayout);

                    // Close button
                    QHBoxLayout* closeButtonLayout = new QHBoxLayout();
                    QPushButton* closeBtn = StreamUP::UIStyles::CreateStyledButton("Close", "neutral");
                    QObject::connect(closeBtn, &QPushButton::clicked, [earlyAccessDialog]() {
                        earlyAccessDialog->close();
                    });
                    closeButtonLayout->addStretch();
                    closeButtonLayout->addWidget(closeBtn);
                    closeButtonLayout->addStretch();

                    footerLayout->addLayout(closeButtonLayout);
                    earlyAccessDialog->show();
                    StreamUP::UIHelpers::CenterDialog(earlyAccessDialog);
                });
            }
        };

        // Create the clickable banner
        ClickableBanner* earlyAccessBanner = new ClickableBanner();
        earlyAccessBanner->setObjectName("earlyAccessBanner");
        earlyAccessBanner->setCursor(Qt::PointingHandCursor);
        earlyAccessBanner->setStyleSheet(QString(
            "QWidget#earlyAccessBanner {"
            "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "        stop:0 #f59e0b, stop:0.5 #fbbf24, stop:1 #f59e0b);"
            "    padding: %1px %2px;"
            "    border-bottom: 2px solid #d97706;"
            "}"
            "QWidget#earlyAccessBanner:hover {"
            "    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "        stop:0 #d97706, stop:0.5 #f59e0b, stop:1 #d97706);"
            "}"
        ).arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
         .arg(StreamUP::UIStyles::Sizes::PADDING_LARGE));

        QHBoxLayout* bannerLayout = new QHBoxLayout(earlyAccessBanner);
        bannerLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
                                         StreamUP::UIStyles::Sizes::PADDING_SMALL,
                                         StreamUP::UIStyles::Sizes::PADDING_MEDIUM,
                                         StreamUP::UIStyles::Sizes::PADDING_SMALL);
        bannerLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);

        // Text content - two lines centered
        QLabel* bannerText = new QLabel(
            "<div style='text-align: center;'>"
            "<b>🌟 Early Access Features Available! 🌟</b><br>"
            "Unlock Source Explorer, exclusive OBS theme, Stream widgets and more. Click to find out more."
            "</div>",
            earlyAccessBanner);
        bannerText->setStyleSheet(QString(
            "color: #78350f; font-size: %1px; font-weight: 400; background: transparent;"
        ).arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
        bannerText->setWordWrap(true);
        bannerText->setAlignment(Qt::AlignCenter);

        bannerLayout->addWidget(bannerText, 1);

        mainLayout->addWidget(earlyAccessBanner);

        // Header section in scrollable area
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: transparent; padding: %1px; }")
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL)); // Even padding all around
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);
        headerLayout->setAlignment(Qt::AlignCenter);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        // StreamUP text logo (clickable)
        class ClickableLabel : public QLabel {
        public:
            ClickableLabel(QWidget* parent = nullptr) : QLabel(parent) {
                setCursor(Qt::PointingHandCursor);
            }
        protected:
            void mousePressEvent(QMouseEvent* event) override {
                if (event->button() == Qt::LeftButton) {
                    QDesktopServices::openUrl(QUrl("https://streamup.tips"));
                }
                QLabel::mousePressEvent(event);
            }
        };
        
        ClickableLabel* textLogoLabel = new ClickableLabel();
        textLogoLabel->setObjectName("textLogoLabel");
        QPixmap textLogoPixmap;
        
        QStringList textLogoPaths = {
            ":/images/icons/social/streamup-logo-text.svg",
            ":/media/logo.png",
            "images/icons/social/streamup-logo-text.svg",
            "media/logo.png"
        };
        
        bool textLogoLoaded = false;
        for (const QString& path : textLogoPaths) {
            textLogoPixmap = QPixmap(path);
            if (!textLogoPixmap.isNull()) {
                // Scale the text logo to fit nicely in header - static size
                QPixmap scaledTextLogo = textLogoPixmap.scaled(250, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                textLogoLabel->setPixmap(scaledTextLogo);
                textLogoLoaded = true;
                break;
            }
        }
        
        if (!textLogoLoaded) {
            // Fallback to text if logo fails to load - static size
            textLogoLabel->setText("StreamUP");
            textLogoLabel->setStyleSheet(QString(
                "QLabel {"
                "font-size: %1px;"
                "font-weight: bold;"
                "color: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                "    stop:0 #f472b6, stop:0.25 #a855f7, stop:0.5 #89b4fa, stop:1 #06b6d4);"
                "margin: 0;"
                "}")
                .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_LARGE));
        }
        textLogoLabel->setAlignment(Qt::AlignCenter);
        
        QString versionText = QString(obs_module_text("StreamUP.SplashScreen.VersionText")).arg(PROJECT_VERSION);
        QLabel* versionLabel = StreamUP::UIStyles::CreateStyledDescription(versionText);
        versionLabel->setObjectName("versionLabel");
        
        headerLayout->addWidget(textLogoLabel);
        headerLayout->addWidget(versionLabel);
        
        // Social media icons - icon only buttons
        QHBoxLayout* socialIconsLayout = new QHBoxLayout();
        socialIconsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL + 2);
        socialIconsLayout->setContentsMargins(0, 0, 0, 0);
        
        // Create standardized squircle social media icon buttons (same size as dock)
        QPushButton* twitterIcon = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
        twitterIcon->setIcon(QIcon(":images/icons/social/twitter.svg"));
        twitterIcon->setIconSize(QSize(16, 16));
        twitterIcon->setCursor(Qt::PointingHandCursor);
        // Additional styling for transparency - keep borders for pill shape visibility
        twitterIcon->setStyleSheet(twitterIcon->styleSheet() + 
            QString("QPushButton { background: transparent; color: %1; }").arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY));
        QObject::connect(twitterIcon, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://twitter.com/StreamUPTips"));
        });
        
        QPushButton* blueskyIcon = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
        blueskyIcon->setIcon(QIcon(":images/icons/social/bluesky.svg"));
        blueskyIcon->setIconSize(QSize(16, 16));
        blueskyIcon->setCursor(Qt::PointingHandCursor);
        // Additional styling for transparency - keep borders for pill shape visibility
        blueskyIcon->setStyleSheet(blueskyIcon->styleSheet() + 
            QString("QPushButton { background: transparent; color: %1; }").arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY));
        QObject::connect(blueskyIcon, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://bsky.app/profile/streamup.tips"));
        });
        
        QPushButton* dorasIcon = StreamUP::UIStyles::CreateStyledSquircleButton("", "neutral", 28);
        dorasIcon->setIcon(QIcon(":images/icons/social/doras.svg"));
        dorasIcon->setIconSize(QSize(16, 16));
        dorasIcon->setCursor(Qt::PointingHandCursor);
        // Additional styling for transparency - keep borders for pill shape visibility
        dorasIcon->setStyleSheet(dorasIcon->styleSheet() + 
            QString("QPushButton { background: transparent; color: %1; }").arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY));
        QObject::connect(dorasIcon, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://doras.to/streamup"));
        });
        
        socialIconsLayout->addStretch();
        socialIconsLayout->addWidget(twitterIcon);
        socialIconsLayout->addWidget(blueskyIcon);
        socialIconsLayout->addWidget(dorasIcon);
        socialIconsLayout->addStretch();
        
        headerLayout->addLayout(socialIconsLayout);

        // Content area with modern StreamUP scrollbar styling
        QScrollArea* scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();
        
        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_LARGE, 
            StreamUP::UIStyles::Sizes::SPACING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_LARGE, 
            StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

        // Add header to the scrollable content
        contentLayout->addWidget(headerWidget);

        // Action buttons section near the top
        QWidget* actionButtonsCard = new QWidget();
        actionButtonsCard->setStyleSheet("QWidget { background: transparent; border: none; padding: 0px; }");
        QVBoxLayout* actionButtonsLayout = new QVBoxLayout(actionButtonsCard);
        actionButtonsLayout->setContentsMargins(StreamUP::UIStyles::Sizes::SPACING_MEDIUM, 
            StreamUP::UIStyles::Sizes::SPACING_MEDIUM, 
            StreamUP::UIStyles::Sizes::SPACING_MEDIUM, 
            StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        QHBoxLayout* actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
        actionLayout->setContentsMargins(0, 0, 0, 0);
        
        QPushButton* patchNotesBtn = StreamUP::UIStyles::CreateStyledButton("📋 View Patch Notes", "success");
        QObject::connect(patchNotesBtn, &QPushButton::clicked, []() {
            StreamUP::PatchNotesWindow::ShowPatchNotesWindow();
        });
        
        actionLayout->addStretch();
        actionLayout->addWidget(patchNotesBtn);
        actionLayout->addStretch();
        
        actionButtonsLayout->addLayout(actionLayout);
        contentLayout->addWidget(actionButtonsCard);

        // Welcome section — flat
        contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Welcome to StreamUP"));
        std::string welcomeMessage = GetWelcomeMessage();
        QLabel* mainContentLabel = UIHelpers::CreateRichTextLabel(QString::fromStdString(welcomeMessage), false, true, Qt::Alignment(), true);
        contentLayout->addWidget(mainContentLabel);

        // Support section — flat
        contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Support StreamUP"));
        std::string supportContent = LoadLocalSupportInfo();
        QLabel* supportLabel = UIHelpers::CreateRichTextLabel(QString::fromStdString(supportContent), false, true, Qt::Alignment(), true);
        contentLayout->addWidget(supportLabel);
        
        // More compact donation buttons
        QHBoxLayout* donationLayout = new QHBoxLayout();
        donationLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
        donationLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::SPACING_SMALL, 0, 0);
        
            
        QPushButton* patreonBtn = StreamUP::UIStyles::CreateStyledButton("Patreon", "info");
        patreonBtn->setIcon(QIcon(":images/icons/social/patreon.svg"));
        patreonBtn->setIconSize(QSize(16, 16));
        QObject::connect(patreonBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://www.patreon.com/streamup"));
        });
        
        QPushButton* kofiBtn = StreamUP::UIStyles::CreateStyledButton("Ko-Fi", "info");
        kofiBtn->setIcon(QIcon(":images/icons/social/kofi.svg"));
        kofiBtn->setIconSize(QSize(16, 16));
        QObject::connect(kofiBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://ko-fi.com/streamup"));
        });
        
        QPushButton* beerBtn = StreamUP::UIStyles::CreateStyledButton("Buy a Beer", "warning");
        beerBtn->setIcon(QIcon(":images/icons/social/beer.svg"));
        beerBtn->setIconSize(QSize(16, 16));
        QObject::connect(beerBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.lemonsqueezy.com/buy/15f64c2f-8b8c-443e-bd6c-e8bf49a0fc97"));
        });
        
        QPushButton* githubBtn = StreamUP::UIStyles::CreateStyledButton("Star Project", "neutral");
        githubBtn->setIcon(QIcon(":images/icons/social/github.svg"));
        githubBtn->setIconSize(QSize(16, 16));
        QObject::connect(githubBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://github.com/StreamUPTips/obs-streamup"));
        });
        
        donationLayout->addStretch();
        donationLayout->addWidget(patreonBtn);
        donationLayout->addWidget(kofiBtn);
        donationLayout->addWidget(beerBtn);
        donationLayout->addWidget(githubBtn);
        donationLayout->addStretch();
        
        contentLayout->addLayout(donationLayout);

        // Supporters section — flat
        contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Thank You to Our Supporters!"));
        QLabel* supportersLabel = UIHelpers::CreateRichTextLabel(GenerateSupportersHTML(), false, true, Qt::Alignment(), true);
        supportersLabel->setObjectName("supportersLabel");
        contentLayout->addWidget(supportersLabel);
        
        // Set up a timer to refresh the supporters display every 2 seconds until loaded
        QTimer* refreshTimer = new QTimer(dialog);
        QObject::connect(refreshTimer, &QTimer::timeout, [supportersLabel, refreshTimer]() {
            supportersLabel->setText(GenerateSupportersHTML());
            if (supportersData.loaded) {
                refreshTimer->stop(); // Stop refreshing once data is loaded
            }
        });
        refreshTimer->start(2000); // Refresh every 2 seconds

        // Links section — flat
        contentLayout->addWidget(StreamUP::UIStyles::CreateSectionHeader("Useful Links"));
        QHBoxLayout* linksButtonLayout = new QHBoxLayout();
        linksButtonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
        linksButtonLayout->setContentsMargins(0, 0, 0, 0);
        
        QPushButton* docsBtn = StreamUP::UIStyles::CreateStyledButton("📖 Documentation", "success");
        QObject::connect(docsBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.doras.click/docs"));
        });
        
        QPushButton* discordBtn = StreamUP::UIStyles::CreateStyledButton("Discord", "info");
        discordBtn->setIcon(QIcon(":images/icons/social/discord.svg"));
        discordBtn->setIconSize(QSize(16, 16));
        QObject::connect(discordBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://discord.com/invite/RnDKRaVCEu"));
        });
        
        QPushButton* websiteBtn = StreamUP::UIStyles::CreateStyledButton("🌐 Website", "error");
        QObject::connect(websiteBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips"));
        });
        
        QPushButton* twitterBtn = StreamUP::UIStyles::CreateStyledButton("Twitter", "neutral");
        twitterBtn->setIcon(QIcon(":images/icons/social/twitter.svg"));
        twitterBtn->setIconSize(QSize(16, 16));
        QObject::connect(twitterBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://twitter.com/StreamUPTips"));
        });
        
        QPushButton* blueskyBtn = StreamUP::UIStyles::CreateStyledButton("Bluesky", "neutral");
        blueskyBtn->setIcon(QIcon(":images/icons/social/bluesky.svg"));
        blueskyBtn->setIconSize(QSize(16, 16));
        QObject::connect(blueskyBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://bsky.app/profile/streamup.tips"));
        });
        
        QPushButton* dorasBtn = StreamUP::UIStyles::CreateStyledButton("Doras", "neutral");
        dorasBtn->setIcon(QIcon(":images/icons/social/doras.svg"));
        dorasBtn->setIconSize(QSize(16, 16));
        QObject::connect(dorasBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://doras.to/streamup"));
        });

        linksButtonLayout->addStretch();
        linksButtonLayout->addWidget(docsBtn);
        linksButtonLayout->addWidget(discordBtn);
        linksButtonLayout->addWidget(websiteBtn);
        linksButtonLayout->addStretch();
        
        // Second row for social media buttons
        QHBoxLayout* socialLinksLayout = new QHBoxLayout();
        socialLinksLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
        socialLinksLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::SPACING_SMALL, 0, 0);
        
        socialLinksLayout->addStretch();
        socialLinksLayout->addWidget(twitterBtn);
        socialLinksLayout->addWidget(blueskyBtn);
        socialLinksLayout->addWidget(dorasBtn);
        socialLinksLayout->addStretch();
        
        contentLayout->addLayout(linksButtonLayout);
        contentLayout->addLayout(socialLinksLayout);

        // Bottom button area with two buttons
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet(QString("background: transparent; padding: %1px;")
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL));
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Check for Plugin Update button
        QPushButton* updateBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("StreamUP.SplashScreen.CheckForUpdate"), "info");
        QObject::connect(updateBtn, &QPushButton::clicked, []() {
            StreamUP::PluginManager::ShowCachedPluginUpdatesDialog();
        });
        
        // Close button
        QPushButton* closeBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("Close"), "neutral");
        closeBtn->setDefault(true);
        QObject::connect(closeBtn, &QPushButton::clicked, [dialog]() {
            dialog->close();
        });
        
        buttonLayout->addStretch();
        buttonLayout->addWidget(updateBtn);
        buttonLayout->addWidget(closeBtn);
        buttonLayout->addStretch();
        
        contentLayout->addWidget(buttonWidget);

        scrollArea->setWidget(contentWidget);
        
        mainLayout->addWidget(scrollArea);

        // Dialog will be managed by DialogManager

        dialog->show();
        StreamUP::UIHelpers::CenterDialog(dialog);
        return dialog;
    });
}

void ShowSplashScreenIfNeeded()
{
    ShowCondition condition = CheckSplashCondition();
    if (condition == ShowCondition::Never) {
        return;
    }

    // Check if user disabled splash screen
    obs_data_t* settings = SettingsManager::LoadSettings();
    if (settings && obs_data_get_bool(settings, "splash_disabled")) {
        // Still update version tracking even if splash is disabled
        UpdateVersionTracking();
        if (settings) obs_data_release(settings);
        return;
    }
    if (settings) obs_data_release(settings);

    if (condition == ShowCondition::VersionUpdate) {
        // For version updates, show patch notes directly instead of the welcome splash
        UpdateVersionTracking();
        PatchNotesWindow::ShowPatchNotesWindow();

        ErrorHandler::LogInfo("Patch notes shown for version update to " + std::string(PROJECT_VERSION),
                             ErrorHandler::Category::UI);
    } else {
        // For first install, show the full welcome splash screen
        CreateSplashDialog(condition);

        ErrorHandler::LogInfo("Welcome splash shown for first install of version " + std::string(PROJECT_VERSION),
                             ErrorHandler::Category::UI);
    }
}

void ShowSplashScreen()
{
    CreateSplashDialog(ShowCondition::VersionUpdate);
}

bool IsSplashScreenOpen()
{
    return UIHelpers::DialogManager::IsSingletonDialogOpen("splash");
}

} // namespace SplashScreen
} // namespace StreamUP

// Include moc file for Q_OBJECT class defined in this cpp file
#include "splash-screen.moc"
