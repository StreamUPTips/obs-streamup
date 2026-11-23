#include "splash-screen.hpp"
#include <streamup/ui/svg-icon.hpp>
#include <QGridLayout>
#include "streamup-kit.hpp"
#include "ui-helpers.hpp"
#include <streamup/ui/window-chrome.hpp> // ShadowDialog, RoundedContainer, makeWindow, WindowShell
#include <streamup/ui/pill-button.hpp>   // PillButton
#include <streamup/ui/labels.hpp>        // makeLabel, sectionHeader
#include <streamup/ui/ui-scrollbar.hpp>  // useScrollBars
#include "patch-notes-window.hpp"
#include "settings-manager.hpp"
#include "../flow-layout.hpp"
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

using namespace StreamUP::UIStyles;

// Splash dialog is now managed by DialogManager in ui-helpers

// Base carousel widget for images
class ImageCarousel : public QWidget
{
    Q_OBJECT

public:
    explicit ImageCarousel(QWidget* parent = nullptr) : QWidget(parent), currentIndex(0)
    {
        setFixedHeight(StreamUP::UIStyles::S(300));
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
        layout->setSpacing(StreamUP::UIStyles::S(12));

        // Image container
        imageContainer = new QFrame();
        imageContainer->setFixedHeight(StreamUP::UIStyles::S(250));
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
        dotsContainer->setSpacing(StreamUP::UIStyles::S(8));
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
            dot->setFixedSize(StreamUP::UIStyles::S(10), StreamUP::UIStyles::S(10));
            dot->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            dot->setFlat(true);
            dot->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
                "QPushButton {"
                "    background-color: %1;"
                "    border: none;"
                "    border-radius: 5px;"
                "}"
                "QPushButton:hover {"
                "    background-color: %2;"
                "}"
            ).arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
             .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)));

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

            dots[i]->setStyleSheet(StreamUP::UIStyles::scale_qss(style));
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

        // Create modal zoom dialog (frameless rounded card + elevation shadow)
        const int sm = StreamUP::UIStyles::ShadowDialog::kShadowMargin;
        QDialog* zoomDialog = new StreamUP::UIStyles::ShadowDialog(this->window());
        zoomDialog->setWindowTitle(obs_module_text("StreamUP.SplashScreen.ImagePreviewTitle"));
        zoomDialog->setModal(true);
        zoomDialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
        zoomDialog->setAttribute(Qt::WA_TranslucentBackground);

        QVBoxLayout* outerLayout = new QVBoxLayout(zoomDialog);
        outerLayout->setContentsMargins(sm, sm, sm, sm);
        outerLayout->setSpacing(0);

        auto* zoomCard = new StreamUP::UIStyles::RoundedContainer(14);
        outerLayout->addWidget(zoomCard);

        QVBoxLayout* layout = new QVBoxLayout(zoomCard);
        layout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20));

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
        QLabel* instructionLabel = StreamUP::UIStyles::makeLabel("Click anywhere or press ESC to close", 14, 500, StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        instructionLabel->setWordWrap(true);
        instructionLabel->setAlignment(Qt::AlignCenter);

        layout->addWidget(fullImageLabel, 1);
        layout->addSpacing(StreamUP::UIStyles::S(10));
        layout->addWidget(instructionLabel);

        // Fixed dialog size (+ shadow margin)
        zoomDialog->setFixedSize(StreamUP::UIStyles::S(860) + 2 * sm, StreamUP::UIStyles::S(720) + 2 * sm);

        // Center on screen
        zoomDialog->move(
            (QApplication::primaryScreen()->geometry().width() - (StreamUP::UIStyles::S(860) + 2 * sm)) / 2,
            (QApplication::primaryScreen()->geometry().height() - (StreamUP::UIStyles::S(720) + 2 * sm)) / 2
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

// The last supporters list, kept on disk so About shows names straight away
// and swaps in the fresh list when it arrives.
static QString SupportersCachePath()
{
    char *path = obs_module_config_path("supporters-cache.json");
    const QString p = path ? QString::fromUtf8(path) : QString();
    bfree(path);
    return p;
}

static SupportersData ParseSupporters(const QJsonObject &root)
{
    SupportersData out;
    auto read = [&root](const char *key, std::vector<Supporter> &into) {
        const QJsonObject obj = root.value(key).toObject();
        for (const QJsonValue value : obj.value("supporters").toArray()) {
            const QJsonObject o = value.toObject();
            Supporter s;
            s.userId = o.value("userId").toString();
            s.displayName = o.value("displayName").toString();
            if (!s.displayName.isEmpty())
                into.push_back(s);
        }
        std::sort(into.begin(), into.end());
    };
    read("andilippi", out.andiSupporters);
    read("streamUP", out.streamupSupporters);
    return out;
}

// Fills the list from the cache when nothing has loaded yet. Not "loaded":
// the window keeps refreshing until the network answers.
static bool g_fromCache = false;
static void LoadCachedSupporters()
{
    if (supportersData.loaded || g_fromCache)
        return;
    QFile f(SupportersCachePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;
    SupportersData cached = ParseSupporters(doc.object());
    supportersData.andiSupporters = cached.andiSupporters;
    supportersData.streamupSupporters = cached.streamupSupporters;
    g_fromCache = true;
}

void LoadSupportersData()
{
    LoadCachedSupporters();
    StreamUP::HttpClient::MakeAsyncGetRequest("https://streamup.tips/api/supporters",
        [](const std::string& /* url */, const std::string& response, bool success) {
            QJsonParseError parseError;
            const QJsonDocument doc = success ? QJsonDocument::fromJson(response.c_str(), &parseError) : QJsonDocument();
            const bool ok = success && parseError.error == QJsonParseError::NoError && doc.isObject();
            if (!ok)
                ErrorHandler::LogWarning("Failed to load the supporters list", ErrorHandler::Category::Network);
            SupportersData fresh = ok ? ParseSupporters(doc.object()) : SupportersData();
            const QByteArray raw = ok ? QByteArray::fromStdString(response) : QByteArray();
            // Hand over on the UI thread: the About window reads this list.
            QMetaObject::invokeMethod(qApp, [fresh, raw, ok]() {
                if (ok) {
                    supportersData.andiSupporters = fresh.andiSupporters;
                    supportersData.streamupSupporters = fresh.streamupSupporters;
                    QFile f(SupportersCachePath());
                    if (f.open(QIODevice::WriteOnly))
                        f.write(raw);
                } else if (!g_fromCache) {
                    supportersData.errorMessage = "Failed to load supporters";
                }
                supportersData.loaded = true;
            }, Qt::QueuedConnection);
        });
}

QString GenerateSupportersHTML()
{
    // One calm palette: quiet grey group labels, neutral name chips, a hairline
    // opt-in card. No gold or purple, so it sits with the rest of the window.
    const QString wrap = R"(<div style="color: #cdd6f4; line-height: 1.4; font-size: 13px;">)";
    if (!supportersData.loaded)
        return wrap + R"(<p style="margin: 0; color: #a6adc8;">Loading supporters…</p></div>)";

    if (!supportersData.errorMessage.isEmpty())
        return wrap + R"(<p style="margin: 0; color: #a6adc8;">Supporters could not be loaded right now.</p></div>)";

    QString html = wrap;
    auto group = [&html](const char *title, const std::vector<Supporter> &list) {
        if (list.empty())
            return;
        html += QString(R"(<p style="color: #9399b2; margin: 16px 0 8px 0; font-size: 12px; font-weight: 600;">%1</p>)")
                    .arg(QString::fromUtf8(title));
        html += R"(<p style="margin: 0; line-height: 2.2;">)";
        for (size_t i = 0; i < list.size(); ++i) {
            html += QString(R"(<span style="background-color: #313244; color: #cdd6f4; font-size: 12px; white-space: nowrap;">&nbsp;&nbsp;%1&nbsp;&nbsp;</span>)")
                        .arg(list[i].displayName.toHtmlEscaped());
            if (i + 1 < list.size())
                html += " &nbsp;";
        }
        html += R"(</p>)";
    };
    group("Andi's supporters", supportersData.andiSupporters);
    group("StreamUP supporters", supportersData.streamupSupporters);

    if (supportersData.andiSupporters.empty() && supportersData.streamupSupporters.empty())
        html += R"(<p style="margin: 0; color: #a6adc8;">No public supporters to show yet.</p>)";

    html += R"(<table width="100%" cellpadding="14" cellspacing="0" style="margin-top: 18px; background-color: #24273a; border: 1px solid #45475a;"><tr><td>)";
    html += R"(<p style="margin: 0 0 4px 0; color: #cdd6f4; font-size: 13px; font-weight: 600;">Supporter but not listed?</p>)";
    html += R"(<p style="margin: 0; color: #a6adc8; font-size: 12px; line-height: 1.5;">Showing your name is opt-in. Turn it on and choose how it appears in your <a href="https://streamup.tips/Identity/Account/Manage" style="color: #89b4fa; text-decoration: none;">account settings</a>.</p>)";
    html += R"(</td></tr></table>)";

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
                result += "<h3 style=\"color: #fbbf24; margin: 16px 0 8px 0; font-size: 14px;\">" + headerText + "</h3>\n";
            } else if (headerText.find("Monthly Supporter") != std::string::npos) {
                result += "<h3 style=\"color: #a78bfa; margin: 16px 0 8px 0; font-size: 14px;\">" + headerText + "</h3>\n";
            } else {
                result += "<h3 style=\"color: #a855f7; margin: 16px 0 8px 0; font-size: 14px;\">" + headerText + "</h3>\n";
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
            result += "<h3 style=\"color: #a855f7; margin: 16px 0 8px 0; font-size: 14px;\">" + headerText + "</h3>\n";
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
    <h2 style="color: #89b4fa; margin: 18px 0 12px 0; font-size: 18px; font-weight: 600;">Welcome to StreamUP</h2>

    <p style="margin: 12px 0; color: #cdd6f4; font-size: 14px;">
        <strong>Thank you for installing StreamUP.</strong> You've just unlocked a powerful toolkit designed to supercharge your OBS Studio experience.
    </p>

    <h3 style="color: #a855f7; margin: 16px 0 8px 0; font-size: 14px;">What is StreamUP?</h3>
    <p style="margin: 8px 0;">
        StreamUP is an advanced plugin that provides essential tools for content creators, streamers, and anyone using OBS Studio. From source management and plugin updates to WebSocket API control and automated workflows — StreamUP streamlines your creative process.
    </p>

    <h3 style="color: #a855f7; margin: 16px 0 8px 0; font-size: 14px;">Getting Started</h3>
    <p style="margin: 8px 0;">
        Ready to dive in? Check out our comprehensive documentation to learn about all the features at your fingertips:
    </p>
    <p style="margin: 8px 0;">
        <a href="https://streamup.doras.click/docs" style="color: #89b4fa; text-decoration: underline; font-weight: 500;">Read the Documentation</a>
    </p>

    <h3 style="color: #fbbf24; margin: 16px 0 8px 0; font-size: 14px;">Support Our Development</h3>
    <p style="margin: 8px 0;">
        StreamUP is a passion project created for the OBS community. If you find it useful, please consider supporting our development efforts. Your support helps us continue adding new features and maintaining compatibility with the latest OBS versions.
    </p>
    <p style="margin: 8px 0;">
        Every contribution, no matter how small, makes a real difference and keeps this project thriving.
    </p>

    <div style="margin: 16px 0; padding: 12px; border-left: 4px solid #a855f7; border-radius: 4px;">
        <p style="margin: 0; color: #e9d5ff; font-style: italic;">
            Love from <strong>Andi (Andilippi)</strong><br>
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
            result += "<h3 style=\"color: #a855f7; margin: 16px 0 8px 0; font-size: 14px;\">" + headerText + "</h3>\n";
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
    <p style="margin: 0 0 8px 0; color: #fbbf24; font-style: italic;">Unable to load patch notes from local file.</p>
    <p style="margin: 0;"><b>Recent Features:</b> WebSocket API, Plugin Manager, Notifications, Settings UI</p>
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
        StreamUP::UIStyles::WindowShell shell = StreamUP::UIStyles::makeWindow(
            obs_module_text("StreamUP.SplashScreen.Title"), "v" PROJECT_VERSION,
            nullptr, /*brandFooter=*/true, "StreamUP");
        QDialog* dialog = shell.dialog;
        dialog->setModal(false);
        // A normal, resizable window: wide enough for the links on one row,
        // as tall as the screen allows up to its content.
        dialog->resize(
            StreamUP::UIStyles::S(720) + 2 * StreamUP::UIStyles::S(StreamUP::UIStyles::ShadowDialog::kShadowMargin),
            StreamUP::UIStyles::S(760) + 2 * StreamUP::UIStyles::S(StreamUP::UIStyles::ShadowDialog::kShadowMargin));

        // Ensure version tracking is updated when dialog closes (any way)
        QObject::connect(dialog, &QDialog::finished, []() {
            UpdateVersionTracking();
        });

        QVBoxLayout* mainLayout = shell.content;
        // Strip the chrome's content margins so the early access banner can sit
        // flush against the dialog header. The actual content area below the
        // banner gets its own padding via the scroll content widget.
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

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
                    StreamUP::UIStyles::WindowShell eaShell = StreamUP::UIStyles::makeWindow(
                        "StreamUP \xe2\x80\xa2 Early Access", "v" PROJECT_VERSION,
                        nullptr, /*brandFooter=*/true, "StreamUP");
                    QDialog* earlyAccessDialog = eaShell.dialog;
                    earlyAccessDialog->setModal(true);
                    earlyAccessDialog->setFixedSize(
                        StreamUP::UIStyles::S(800) + 2 * StreamUP::UIStyles::S(StreamUP::UIStyles::ShadowDialog::kShadowMargin),
                        StreamUP::UIStyles::S(600) + 2 * StreamUP::UIStyles::S(StreamUP::UIStyles::ShadowDialog::kShadowMargin));

                    QVBoxLayout* dialogLayout = eaShell.content;
                    dialogLayout->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(16),
                                                     StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(16));

                    // Create scroll area that fills the whole dialog
                    QScrollArea* scrollArea = new QScrollArea();
                    scrollArea->setWidgetResizable(true);
                    scrollArea->setFrameShape(QFrame::NoFrame);
                    StreamUP::UIStyles::useScrollBars(scrollArea);
                    QWidget* scrollContent = new QWidget();
                    scrollContent->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
                    QVBoxLayout* contentLayout = new QVBoxLayout(scrollContent);
                    contentLayout->setContentsMargins(StreamUP::UIStyles::S(16),
                                                     StreamUP::UIStyles::S(12),
                                                     StreamUP::UIStyles::S(16),
                                                     StreamUP::UIStyles::S(12));
                    contentLayout->setSpacing(StreamUP::UIStyles::S(16));

                    // Header - using HTML like the about window
                    QString headerHTML = R"(
<div style="color: #cdd6f4; line-height: 1.4; font-size: 14px;">
    <h2 style="color: #fbbf24; margin: 18px 0 12px 0; font-size: 18px; font-weight: 600; text-align: center;">Get Exclusive Early Access to New Features</h2>
    <p style="margin: 12px 0; color: #cdd6f4; font-size: 14px; text-align: center;">
        Support StreamUP development and unlock cutting-edge features before they're released to the public:
    </p>
</div>
                    )";
                    QLabel* headerLabel = UIHelpers::CreateRichTextLabel(headerHTML, false, true, Qt::Alignment(), true);
                    contentLayout->addWidget(headerLabel);

                    // Source Explorer Section — flat
                    contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("Source Explorer (Early Access)"));
                    QVBoxLayout* sourceExplorerLayout = new QVBoxLayout();
                    sourceExplorerLayout->setSpacing(StreamUP::UIStyles::S(12));

                    // Source Explorer carousel with navigation buttons
                    QHBoxLayout* explorerCarouselControlLayout = new QHBoxLayout();
                    explorerCarouselControlLayout->setContentsMargins(0, 0, 0, 0);
                    explorerCarouselControlLayout->setSpacing(0);

                    // Create carousel
                    SourceExplorerCarousel* explorerCarousel = new SourceExplorerCarousel();

                    // Create external navigation buttons
                    QPushButton* explorerPrevButton = new StreamUP::UIStyles::PillButton("❮", "neutral");
                    explorerPrevButton->setFixedSize(StreamUP::UIStyles::S(40), StreamUP::UIStyles::S(40));
                    QPushButton* explorerNextButton = new StreamUP::UIStyles::PillButton("❯", "neutral");
                    explorerNextButton->setFixedSize(StreamUP::UIStyles::S(40), StreamUP::UIStyles::S(40));

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
                    contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("Exclusive Supporter Theme"));
                    QVBoxLayout* themeLayout = new QVBoxLayout();
                    themeLayout->setSpacing(StreamUP::UIStyles::S(12));

                    // Theme carousel with navigation buttons
                    QHBoxLayout* carouselControlLayout = new QHBoxLayout();
                    carouselControlLayout->setContentsMargins(0, 0, 0, 0);
                    carouselControlLayout->setSpacing(0);

                    // Create carousel
                    ThemeImageCarousel* carousel = new ThemeImageCarousel();

                    // Create external navigation buttons
                    QPushButton* prevButton = new StreamUP::UIStyles::PillButton("❮", "neutral");
                    prevButton->setFixedSize(StreamUP::UIStyles::S(40), StreamUP::UIStyles::S(40));
                    QPushButton* nextButton = new StreamUP::UIStyles::PillButton("❯", "neutral");
                    nextButton->setFixedSize(StreamUP::UIStyles::S(40), StreamUP::UIStyles::S(40));

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
                    contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("Access to StreamUP.tips Premium Content"));
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
                    contentLayout->addWidget(StreamUP::UIStyles::sectionHeader("Your Support Makes a Difference"));
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
                    QVBoxLayout* footerLayout = eaShell.footer;

                    // Support Buttons
                    QHBoxLayout* supportButtonLayout = new QHBoxLayout();
                    supportButtonLayout->setSpacing(StreamUP::UIStyles::S(8));

                    auto* kofiBtn = new StreamUP::UIStyles::PillButton("Ko-Fi", "primary");
                    kofiBtn->setLeadingIcon(QIcon(":images/icons/social/kofi.svg"), 14);
                    kofiBtn->setToolTip(obs_module_text("StreamUP.SplashScreen.SupportKofiTooltip"));
                    QObject::connect(kofiBtn, &QPushButton::clicked, []() {
                        QDesktopServices::openUrl(QUrl("https://ko-fi.com/streamup"));
                    });

                    auto* patreonBtn = new StreamUP::UIStyles::PillButton("Patreon", "primary");
                    patreonBtn->setLeadingIcon(QIcon(":images/icons/social/patreon.svg"), 14);
                    patreonBtn->setToolTip(obs_module_text("StreamUP.SplashScreen.SupportPatreonTooltip"));
                    QObject::connect(patreonBtn, &QPushButton::clicked, []() {
                        QDesktopServices::openUrl(QUrl("https://www.patreon.com/streamup"));
                    });

                    auto* andiBtn = new StreamUP::UIStyles::PillButton("Support Andi", "primary");
                    andiBtn->setLeadingIcon(QIcon(":images/icons/social/doras.svg"), 14);
                    andiBtn->setToolTip(obs_module_text("StreamUP.SplashScreen.SupportAndiTooltip"));
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
                    auto* closeBtn = new StreamUP::UIStyles::PillButton("Close", "outline");
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
        earlyAccessBanner->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
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
        ).arg(StreamUP::UIStyles::S(12))
         .arg(StreamUP::UIStyles::S(16))));

        QHBoxLayout* bannerLayout = new QHBoxLayout(earlyAccessBanner);
        bannerLayout->setContentsMargins(StreamUP::UIStyles::S(12),
                                         StreamUP::UIStyles::S(8),
                                         StreamUP::UIStyles::S(12),
                                         StreamUP::UIStyles::S(8));
        bannerLayout->setSpacing(StreamUP::UIStyles::S(12));

        // Text content - two lines centered
        QLabel* bannerText = new QLabel(
            "<div style='text-align: center;'>"
            "<b>Early Access Features Available</b><br>"
            "Unlock Source Explorer, exclusive OBS theme, Stream widgets and more. Click to find out more."
            "</div>",
            earlyAccessBanner);
        bannerText->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
            "color: #78350f; font-size: %1px; font-weight: 400; background: transparent;"
        ).arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL)));
        bannerText->setWordWrap(true);
        bannerText->setAlignment(Qt::AlignCenter);

        bannerLayout->addWidget(bannerText, 1);

        // 3.0: the Early Access banner gave way to the membership card below.
        earlyAccessBanner->deleteLater();

        // Header section: 3-column row [left spacer | centered logo+version stack | patch notes].
        // The left spacer is sized to the button's width so the centered stack
        // remains visually centered against the dialog rather than offset by
        // the button on the right.
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet("QWidget#headerWidget { background: transparent; }");
        QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setSpacing(StreamUP::UIStyles::S(12));
        headerLayout->setContentsMargins(0, StreamUP::UIStyles::S(12), 0, StreamUP::UIStyles::S(12));
        
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
                QPixmap scaledTextLogo = textLogoPixmap.scaled(180, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                textLogoLabel->setPixmap(scaledTextLogo);
                textLogoLoaded = true;
                break;
            }
        }
        
        if (!textLogoLoaded) {
            // Fallback to text if logo fails to load - static size
            textLogoLabel->setText("StreamUP");
            textLogoLabel->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
                "QLabel {"
                "font-size: %1px;"
                "font-weight: 600;"
                "color: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                "    stop:0 #f472b6, stop:0.25 #a855f7, stop:0.5 #89b4fa, stop:1 #06b6d4);"
                "margin: 0;"
                "}")
                .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_LARGE)));
        }
        textLogoLabel->setAlignment(Qt::AlignCenter);
        textLogoLabel->setStyleSheet(textLogoLabel->styleSheet() + "QLabel { padding: 0; margin: 0; }");

        QString versionText = QString(obs_module_text("StreamUP.SplashScreen.VersionText")).arg(PROJECT_VERSION);
        QLabel* versionLabel = StreamUP::UIStyles::makeLabel(versionText, 13, 500, StreamUP::UIStyles::Colors::TEXT_SECONDARY);
        versionLabel->setObjectName("versionLabel");
        versionLabel->setAlignment(Qt::AlignCenter);
        versionLabel->setWordWrap(false); // single line under the logo
        versionLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        QPushButton* patchNotesBtn = new StreamUP::UIStyles::PillButton(obs_module_text("About.WhatsNew"), "neutral");
        QObject::connect(patchNotesBtn, &QPushButton::clicked, []() {
            StreamUP::PatchNotesWindow::ShowPatchNotesWindow();
        });

        // Center cell: logo + version stacked vertically
        QWidget* centerStack = new QWidget();
        centerStack->setStyleSheet("background: transparent;");
        QVBoxLayout* centerLay = new QVBoxLayout(centerStack);
        centerLay->setContentsMargins(0, 0, 0, 0);
        centerLay->setSpacing(StreamUP::UIStyles::S(4));
        centerLay->addWidget(textLogoLabel, 0, Qt::AlignHCenter);
        centerLay->addWidget(versionLabel, 0, Qt::AlignHCenter);

        // Left spacer matches the button's width so the center stack stays
        // visually centered against the dialog instead of shifted left by
        // the button on the right.
        const int balancerWidth = patchNotesBtn->sizeHint().width();
        QWidget* leftBalancer = new QWidget();
        leftBalancer->setFixedWidth(balancerWidth);
        leftBalancer->setStyleSheet("background: transparent;");

        headerLayout->addWidget(leftBalancer);
        headerLayout->addStretch();
        headerLayout->addWidget(centerStack);
        headerLayout->addStretch();
        headerLayout->addWidget(patchNotesBtn);

        // Content area with modern StreamUP scrollbar styling
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        StreamUP::UIStyles::useScrollBars(scrollArea);

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::S(16),
            StreamUP::UIStyles::S(12),
            StreamUP::UIStyles::S(16),
            StreamUP::UIStyles::S(12));
        contentLayout->setSpacing(StreamUP::UIStyles::S(12));

        // Header (logo + version + patch notes button) inline at the top
        contentLayout->addWidget(headerWidget);

        // One line about StreamUP, the membership card, and the long welcome
        // letter folded under "Learn more".
        QLabel *aboutLine = StreamUP::UIStyles::makeLabel(obs_module_text("About.Line"), StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL,
                                                          500, StreamUP::UIStyles::Colors::TEXT_SECONDARY);
        aboutLine->setAlignment(Qt::AlignCenter);
        aboutLine->setWordWrap(true);
        contentLayout->addWidget(aboutLine);
        if (QWidget *member = StreamUP::Kit::MembershipCard(StreamUP::Kit::MemberStyle::Card))
            contentLayout->addWidget(member);
        auto *learnMore = new StreamUP::Kit::MoreOptions(QStringLiteral("about.learnmore"), obs_module_text("About.LearnMore"));
        contentLayout->addWidget(learnMore);

        // Learn more: four small cards, each an icon, a few words and at most a
        // button, instead of the old letter.
        {
            auto *grid = new QGridLayout();
            grid->setHorizontalSpacing(StreamUP::UIStyles::S(10));
            grid->setVerticalSpacing(StreamUP::UIStyles::S(10));
            struct Chunk {
                const char *icon;
                const char *titleKey;
                const char *lineKey;
                QList<QPair<QString, QString>> links; // label, url
            };
            const QList<Chunk> chunks = {
                {"package", "About.Chunk.Products", "About.Chunk.ProductsLine",
                 {{obs_module_text("About.Chunk.Browse"), "https://streamup.tips/"}}},
                {"tools", "About.Chunk.Tools", "About.Chunk.ToolsLine",
                 {{obs_module_text("About.Docs"), "https://streamup.doras.click/docs"}}},
                {"heart", "About.Chunk.Support", "About.Chunk.SupportLine",
                 {{"Patreon", "https://www.patreon.com/streamup"},
                  {"Ko-fi", "https://ko-fi.com/streamup"},
                  {obs_module_text("About.Chunk.Beer"), "https://streamup.lemonsqueezy.com/buy/15f64c2f-8b8c-443e-bd6c-e8bf49a0fc97"}}},
                {"users", "About.Chunk.Community", "About.Chunk.CommunityLine",
                 {{"Discord", "https://discord.com/invite/RnDKRaVCEu"},
                  {"GitHub", "https://github.com/StreamUPTips/obs-streamup"}}},
            };
            int n = 0;
            for (const Chunk &c : chunks) {
                auto *box = new QFrame();
                box->setObjectName("aboutChunk");
                box->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
                    "QFrame#aboutChunk{background:%1;border:1px solid rgba(255,255,255,0.06);border-radius:12px;}"
                    "QFrame#aboutChunk QLabel{background:transparent;border:none;}")
                    .arg(StreamUP::UIStyles::Colors::BG_PRIMARY)));
                auto *v = new QVBoxLayout(box);
                v->setContentsMargins(StreamUP::UIStyles::S(14), StreamUP::UIStyles::S(12),
                                      StreamUP::UIStyles::S(14), StreamUP::UIStyles::S(12));
                v->setSpacing(StreamUP::UIStyles::S(6));
                auto *top = new QHBoxLayout();
                top->setSpacing(StreamUP::UIStyles::S(8));
                auto *icon = new QLabel();
                icon->setPixmap(StreamUP::UIStyles::svgIcon(QString::fromLatin1(c.icon),
                                QColor(StreamUP::UIStyles::Colors::PRIMARY_LIGHT), 20, dialog->devicePixelRatioF()));
                top->addWidget(icon);
                top->addWidget(StreamUP::UIStyles::makeLabel(obs_module_text(c.titleKey),
                               StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL, 700, StreamUP::UIStyles::Colors::TEXT_PRIMARY), 1);
                v->addLayout(top);
                QLabel *line = StreamUP::UIStyles::makeLabel(obs_module_text(c.lineKey), StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL,
                                                             400, StreamUP::UIStyles::Colors::TEXT_SECONDARY);
                line->setWordWrap(true);
                v->addWidget(line);
                auto *buttons = new QHBoxLayout();
                buttons->setSpacing(StreamUP::UIStyles::S(6));
                for (const auto &link : c.links) {
                    auto *b = new StreamUP::UIStyles::PillButton(link.first, "neutral");
                    const QString url = link.second;
                    QObject::connect(b, &QPushButton::clicked, [url]() { QDesktopServices::openUrl(QUrl(url)); });
                    buttons->addWidget(b);
                }
                buttons->addStretch(1);
                v->addStretch(1);
                v->addLayout(buttons);
                grid->addWidget(box, n / 2, n % 2);
                ++n;
            }
            learnMore->body()->addLayout(grid);
        }

        // === Supporters section — real widgets, FlowLayout, proper pills ===
        // Wrapper card so the section reads as a distinct panel.
        QFrame *supportersCard = new QFrame();
        supportersCard->setObjectName("supportersCard");
        supportersCard->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
            "QFrame#supportersCard { background: %1; border: 1px solid %2; border-radius: 14px; }")
            .arg(StreamUP::UIStyles::Colors::BG_PRIMARY)
            .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE)));
        QVBoxLayout *supportersCardLay = new QVBoxLayout(supportersCard);
        supportersCardLay->setContentsMargins(StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(18), StreamUP::UIStyles::S(20), StreamUP::UIStyles::S(20));
        supportersCardLay->setSpacing(StreamUP::UIStyles::S(12));

        QLabel *supportersTitle = new QLabel(obs_module_text("About.Supporters.Title"));
        supportersTitle->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: 14px; font-weight: 600; background: transparent;")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)));
        supportersCardLay->addWidget(supportersTitle);

        // Two named groups (Andi / StreamUP) live as inner sub-cards inside the
        // wrapper. Pre-build empty containers and let RebuildSupporters() repopulate
        // them so the refresh timer doesn't have to throw away widgets.
        struct SupporterGroup {
            QString title;
            QString accent;        // text/border accent
            QString badgeBg;       // pill background
            QString badgeFg;       // pill text
            QFrame *card;
            QVBoxLayout *cardLay;
            QWidget *flowHost;
            FlowLayout *flow;
        };

        auto buildGroup = [&](const QString &title, const QString &accent,
                              const QString &badgeBg, const QString &badgeFg) {
            SupporterGroup g{};
            g.title = title;
            g.accent = accent;
            g.badgeBg = badgeBg;
            g.badgeFg = badgeFg;
            g.card = new QFrame();
            g.card->setObjectName("supportersGroup");
            g.card->setStyleSheet(QString("QFrame#supportersGroup { background: transparent; border: none; }"));
            g.cardLay = new QVBoxLayout(g.card);
            g.cardLay->setContentsMargins(0, 0, 0, 0);
            g.cardLay->setSpacing(StreamUP::UIStyles::S(10));
            QLabel *t = new QLabel(title);
            t->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: 12px; font-weight: 600; background: transparent;").arg(accent)));
            g.cardLay->addWidget(t);
            // Scoped by object name so the chips' own rules never reach the
            // host: an unscoped rule here let one chip's fill paint the whole
            // flow area as a tinted band behind the names.
            g.flowHost = new QWidget();
            g.flowHost->setObjectName("supportersFlow");
            g.flowHost->setAttribute(Qt::WA_StyledBackground, true);
            g.flowHost->setAutoFillBackground(false);
            g.flowHost->setStyleSheet("QWidget#supportersFlow { background: transparent; }");
            g.flow = new FlowLayout(g.flowHost, 0, 8, 8);
            g.cardLay->addWidget(g.flowHost);
            supportersCardLay->addWidget(g.card);
            return g;
        };

        // Both groups share one calm look: a quiet grey label and neutral
        // hairline chips, so neither fights the rest of the window.
        const QString chipBg = "rgba(255, 255, 255, 0.06)";
        SupporterGroup andiGroup = buildGroup("Andi's supporters", StreamUP::UIStyles::Colors::TEXT_MUTED,
                                               chipBg, StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        SupporterGroup streamupGroup = buildGroup("StreamUP supporters", StreamUP::UIStyles::Colors::TEXT_MUTED,
                                                   chipBg, StreamUP::UIStyles::Colors::TEXT_PRIMARY);

        // While nothing has loaded: grey placeholder pills, the shape of the
        // list to come, instead of a "Loading" line.
        QWidget *skeleton = new QWidget();
        skeleton->setStyleSheet("background: transparent;");
        {
            auto *flow = new FlowLayout(skeleton, 0, 8, 8);
            const int widths[] = {84, 120, 68, 104, 92, 140, 76, 110, 96, 62, 128, 88};
            for (int w : widths) {
                auto *pill = new QFrame();
                pill->setFixedSize(StreamUP::UIStyles::S(w), StreamUP::UIStyles::S(28));
                pill->setStyleSheet(StreamUP::UIStyles::scale_qss("QFrame{background:rgba(255,255,255,0.06);border-radius:12px;}"));
                flow->addWidget(pill);
            }
        }
        supportersCardLay->addWidget(skeleton);
        QLabel *emptyLabel = new QLabel(obs_module_text("StreamUP.SplashScreen.NoSupporters"));
        emptyLabel->setStyleSheet(QString("color: %1; font-style: italic; background: transparent;")
            .arg(StreamUP::UIStyles::Colors::TEXT_MUTED));
        emptyLabel->hide();
        supportersCardLay->addWidget(emptyLabel);

        // Modernised opt-in card. Object-name scoped so the border doesn't
        // cascade onto child QLabels.
        QFrame *optInBox = new QFrame();
        optInBox->setObjectName("optInBox");
        optInBox->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
            "QFrame#optInBox { background: %1; border: 1px solid %2; border-radius: 12px; }"
            "QFrame#optInBox QLabel { background: transparent; border: none; }")
            .arg(StreamUP::UIStyles::Colors::BG_TERTIARY)
            .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE)));
        QVBoxLayout *optInLay = new QVBoxLayout(optInBox);
        optInLay->setContentsMargins(StreamUP::UIStyles::S(16), StreamUP::UIStyles::S(14), StreamUP::UIStyles::S(16), StreamUP::UIStyles::S(14));
        optInLay->setSpacing(StreamUP::UIStyles::S(6));
        QLabel *optInTitle = new QLabel(obs_module_text("About.OptIn.Title"));
        optInTitle->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: 13px; font-weight: 600;")
            .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)));
        QLabel *optInBody = new QLabel(obs_module_text("About.OptIn.Line"));
        optInBody->setWordWrap(true);
        optInBody->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("color: %1; font-size: 12px; font-weight: 400;")
            .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)));
        QLabel *optInLink = new QLabel(QString(R"(<a href="https://streamup.tips/Identity/Account/Manage" style="color: %1; text-decoration: underline;">https://streamup.tips/Identity/Account/Manage</a>)")
            .arg(StreamUP::UIStyles::Colors::PRIMARY_LIGHT));
        optInLink->setOpenExternalLinks(true);
        optInLink->setStyleSheet(StreamUP::UIStyles::scale_qss("font-size: 12px;"));
        optInLay->addWidget(optInTitle);
        optInLay->addWidget(optInBody);
        optInLay->addWidget(optInLink);
        supportersCardLay->addWidget(optInBox);

        contentLayout->addWidget(supportersCard);

        // Rebuild helper — clears each flow layout and re-adds badges.
        auto rebuildSupporters = [andiGroup, streamupGroup, emptyLabel, skeleton]() {
            auto clear = [](FlowLayout *flow) {
                while (QLayoutItem *item = flow->takeAt(0)) {
                    if (QWidget *w = item->widget()) w->deleteLater();
                    delete item;
                }
            };
            auto addBadges = [](const SupporterGroup &g, const std::vector<Supporter> &list) {
                const QString style = QString(
                    "QLabel#supporterChip { background: %1; color: %2; padding: 5px 12px;"
                    "  border: 1px solid rgba(255, 255, 255, 0.08);"
                    "  border-radius: 12px; font-size: 12px; font-weight: 400; }")
                    .arg(g.badgeBg, g.badgeFg);
                for (const auto &supporter : list) {
                    QLabel *badge = new QLabel(supporter.displayName);
                    badge->setObjectName("supporterChip");
                    badge->setStyleSheet(StreamUP::UIStyles::scale_qss(style));
                    g.flow->addWidget(badge);
                }
            };
            clear(andiGroup.flow);
            clear(streamupGroup.flow);
            addBadges(andiGroup, supportersData.andiSupporters);
            addBadges(streamupGroup, supportersData.streamupSupporters);
            const bool any = !supportersData.andiSupporters.empty()
                          || !supportersData.streamupSupporters.empty();
            andiGroup.card->setVisible(!supportersData.andiSupporters.empty());
            streamupGroup.card->setVisible(!supportersData.streamupSupporters.empty());
            skeleton->setVisible(!any && !supportersData.loaded);
            emptyLabel->setVisible(!any && supportersData.loaded);
        };

        // Initial population.
        rebuildSupporters();

        // Refresh until data is loaded.
        QTimer *refreshTimer = new QTimer(dialog);
        QObject::connect(refreshTimer, &QTimer::timeout, [refreshTimer, rebuildSupporters]() {
            rebuildSupporters();
            if (supportersData.loaded) {
                refreshTimer->stop();
            }
        });
        refreshTimer->start(500);

        scrollArea->setWidget(contentWidget);

        mainLayout->addWidget(scrollArea);

        // Links: one row in the content, under the header.

        QHBoxLayout* linksRow = new QHBoxLayout();
        linksRow->setSpacing(StreamUP::UIStyles::S(8));

        auto* docsBtn = new StreamUP::UIStyles::PillButton(obs_module_text("About.Docs"), "neutral");
        QObject::connect(docsBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.doras.click/docs"));
        });

        auto* discordBtn = new StreamUP::UIStyles::PillButton("Discord", "neutral");
        discordBtn->setLeadingIcon(QIcon(":images/icons/social/discord.svg"), 14);
        QObject::connect(discordBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://discord.com/invite/RnDKRaVCEu"));
        });

        auto* websiteBtn = new StreamUP::UIStyles::PillButton(obs_module_text("About.Website"), "neutral");
        QObject::connect(websiteBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips"));
        });

        // Round icon buttons for the socials, on the neutral pill surface.
        auto makeSocialButton = [](const QString &text, const QString &iconPath,
                                   const QString &bg, const QString &hover, const QString &fg) {
            QPushButton *btn = new QPushButton();
            btn->setToolTip(text);
            btn->setAccessibleName(text);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setIcon(QIcon(iconPath));
            btn->setIconSize(QSize(StreamUP::UIStyles::S(16), StreamUP::UIStyles::S(16)));
            // Same soft surface as the pills beside them, so the row reads as
            // one set rather than three brand blocks.
            Q_UNUSED(bg);
            Q_UNUSED(hover);
            btn->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
                "QPushButton { background: rgba(255,255,255,0.07); color: %1; border: none;"
                "  border-radius: 13px; padding: 0;"
                "  min-height: 26px; max-height: 26px; min-width: 26px; max-width: 26px; }"
                "QPushButton:hover { background: rgba(255,255,255,0.13); }"
                "QPushButton:pressed { background: rgba(255,255,255,0.05); }")
                .arg(fg)));
            return btn;
        };

        QPushButton *twitterBtn = makeSocialButton("Twitter", ":images/icons/social/twitter.svg",
                                                    "#000000", "#1d1d1d", "#ffffff");
        QObject::connect(twitterBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://twitter.com/StreamUPTips"));
        });

        QPushButton *blueskyBtn = makeSocialButton("Bluesky", ":images/icons/social/bluesky.svg",
                                                    "#1185fe", "#0a6fd9", "#ffffff");
        QObject::connect(blueskyBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://bsky.app/profile/streamup.tips"));
        });

        QPushButton *dorasBtn = makeSocialButton("Doras", ":images/icons/social/doras.svg",
                                                  "#ac4944", "#923a36", "#ffffff");
        QObject::connect(dorasBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://doras.to/streamup"));
        });

        linksRow->addStretch();
        linksRow->addWidget(docsBtn);
        linksRow->addWidget(discordBtn);
        linksRow->addWidget(websiteBtn);
        linksRow->addSpacing(StreamUP::UIStyles::S(12));
        linksRow->addWidget(twitterBtn);
        linksRow->addWidget(blueskyBtn);
        linksRow->addWidget(dorasBtn);
        linksRow->addStretch();
        contentLayout->insertLayout(2, linksRow);

        // Footer: just Done. Plugin updates are in the StreamUP menu.
        auto* closeBtn = new StreamUP::UIStyles::PillButton(obs_module_text("UI.Button.Done"), "primary");
        closeBtn->setDefault(true);
        QObject::connect(closeBtn, &QPushButton::clicked, [dialog]() {
            dialog->close();
        });
        shell.footerButtons->addWidget(closeBtn);

        // Dialog will be managed by DialogManager

        StreamUP::UIHelpers::PlaceOverMainWindow(dialog);
        dialog->show();
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
