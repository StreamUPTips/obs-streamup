#include "splash-screen.hpp"
#include "ui-helpers.hpp"
#include "settings-manager.hpp"
#include "../utilities/error-handler.hpp"
#include "../utilities/http-client.hpp"
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
#include <QCheckBox>
#include <QScrollBar>
#include <util/platform.h>
#include <sstream>

namespace StreamUP {
namespace SplashScreen {

// Patch notes for current version
static const char* PATCH_NOTES = R"(
<h3>Unable to Load Patch Notes</h3>
)";

// Monthly supporters list (you'll want to update this regularly)
static const char* MONTHLY_SUPPORTERS = R"(
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

// GitHub API configuration
static const char* GITHUB_API_URL = "https://api.github.com/repos/StreamUPTips/obs-streamup/releases/latest";
static const char* GITHUB_RELEASES_API_URL = "https://api.github.com/repos/StreamUPTips/obs-streamup/releases";

std::string FetchLatestReleaseNotes()
{
    std::string response;
    if (!HttpClient::MakeGetRequest(GITHUB_API_URL, response)) {
        ErrorHandler::LogWarning("Failed to fetch GitHub release data", ErrorHandler::Category::Network);
        return "";
    }

    // Log the first 200 characters of response for debugging
    std::string responsePreview = response.length() > 200 ? response.substr(0, 200) + "..." : response;
    ErrorHandler::LogInfo("GitHub API response preview: " + responsePreview, ErrorHandler::Category::Network);

    // Check if response looks like JSON
    if (response.empty() || (response[0] != '{' && response[0] != '[')) {
        ErrorHandler::LogWarning("GitHub API returned non-JSON response: " + responsePreview, ErrorHandler::Category::Network);
        return "";
    }

    // Parse JSON response using OBS built-in JSON functions
    obs_data_t* releaseData = obs_data_create_from_json(response.c_str());
    if (!releaseData) {
        ErrorHandler::LogWarning("Failed to parse GitHub release JSON: " + responsePreview, ErrorHandler::Category::Network);
        return "";
    }

    const char* tagName = obs_data_get_string(releaseData, "tag_name");
    const char* releaseName = obs_data_get_string(releaseData, "name");
    const char* body = obs_data_get_string(releaseData, "body");
    const char* publishedAt = obs_data_get_string(releaseData, "published_at");

    if (!tagName || !body) {
        obs_data_release(releaseData);
        return "";
    }

    // Format the release notes as HTML with comprehensive markdown parsing
    std::string formattedNotes = "<h3 style=\"font-size: 14px; font-weight: 600; color: #f9fafb; margin: 0 0 6px 0;\">📋 What's New & Recent Features</h3>\n";
    formattedNotes += "<h4>🎉 Latest Release: " + std::string(releaseName ? releaseName : tagName) + "</h4>\n";
    
    // Convert markdown-style formatting to HTML
    std::string bodyStr = body;
    
    // Convert **bold** text to <b>
    size_t pos = 0;
    while ((pos = bodyStr.find("**", pos)) != std::string::npos) {
        bodyStr.replace(pos, 2, "<b>");
        size_t endPos = bodyStr.find("**", pos + 3);
        if (endPos != std::string::npos) {
            bodyStr.replace(endPos, 2, "</b>");
        }
        pos += 3;
    }
    
    // Convert *italic* text to <i>
    pos = 0;
    while ((pos = bodyStr.find("*", pos)) != std::string::npos) {
        // Skip if it's part of **bold** (already converted)
        if (pos >= 1 && pos + 2 < bodyStr.length()) {
            std::string check = bodyStr.substr(pos-1, 3);
            if (check == "<b>") {
                pos++;
                continue;
            }
        }
        if (pos + 4 <= bodyStr.length()) {
            std::string check = bodyStr.substr(pos, 4);
            if (check == "</b>") {
                pos += 4;
                continue;
            }
        }
        
        bodyStr.replace(pos, 1, "<i>");
        size_t endPos = bodyStr.find("*", pos + 3);
        if (endPos != std::string::npos) {
            bodyStr.replace(endPos, 1, "</i>");
        }
        pos += 3;
    }
    
    // Convert `code` blocks to <code>
    pos = 0;
    while ((pos = bodyStr.find("`", pos)) != std::string::npos) {
        bodyStr.replace(pos, 1, "<code style=\"background: #374151; padding: 2px 4px; border-radius: 3px; font-family: monospace;\">");
        size_t endPos = bodyStr.find("`", pos + 10);
        if (endPos != std::string::npos) {
            bodyStr.replace(endPos, 1, "</code>");
        }
        pos += 10;
    }
    
    // Convert ### headers to <h4>
    pos = 0;
    while ((pos = bodyStr.find("### ", pos)) != std::string::npos) {
        bodyStr.replace(pos, 4, "<h4 style=\"color: #f3e8ff; margin: 8px 0 4px 0;\">");
        size_t endPos = bodyStr.find('\n', pos);
        if (endPos != std::string::npos) {
            bodyStr.insert(endPos, "</h4>");
        }
        pos += 4;
    }
    
    // Convert ## headers to <h3>
    pos = 0;
    while ((pos = bodyStr.find("## ", pos)) != std::string::npos) {
        bodyStr.replace(pos, 3, "<h3 style=\"color: #a855f7; margin: 10px 0 6px 0;\">");
        size_t endPos = bodyStr.find('\n', pos);
        if (endPos != std::string::npos) {
            bodyStr.insert(endPos, "</h3>");
        }
        pos += 4;
    }
    
    // Convert # headers to <h2>
    pos = 0;
    while ((pos = bodyStr.find("# ", pos)) != std::string::npos) {
        // Skip if it's already part of a header tag
        if (pos >= 4) {
            std::string before = bodyStr.substr(pos-4, 4);
            if (before == "<h3>" || before == "<h4>") {
                pos += 2;
                continue;
            }
        }
        bodyStr.replace(pos, 2, "<h2 style=\"color: #3b82f6; margin: 12px 0 8px 0;\">");
        size_t endPos = bodyStr.find('\n', pos);
        if (endPos != std::string::npos) {
            bodyStr.insert(endPos, "</h2>");
        }
        pos += 4;
    }
    
    // Convert [link text](url) to <a href="url">link text</a>
    pos = 0;
    while ((pos = bodyStr.find("[", pos)) != std::string::npos) {
        size_t closePos = bodyStr.find("]", pos);
        size_t parenPos = bodyStr.find("(", closePos);
        size_t closeParenPos = bodyStr.find(")", parenPos);
        
        if (closePos != std::string::npos && parenPos != std::string::npos && 
            closeParenPos != std::string::npos && parenPos == closePos + 1) {
            
            std::string linkText = bodyStr.substr(pos + 1, closePos - pos - 1);
            std::string linkUrl = bodyStr.substr(parenPos + 1, closeParenPos - parenPos - 1);
            std::string replacement = "<a href=\"" + linkUrl + "\" style=\"color: #60a5fa;\">" + linkText + "</a>";
            
            bodyStr.replace(pos, closeParenPos - pos + 1, replacement);
            pos += replacement.length();
        } else {
            pos++;
        }
    }
    
    // Convert line breaks to <br> for better formatting
    pos = 0;
    while ((pos = bodyStr.find("\n\n", pos)) != std::string::npos) {
        bodyStr.replace(pos, 2, "<br><br>");
        pos += 8;
    }
    
    // Convert - list items to <li>
    pos = 0;
    bool inList = false;
    std::string result;
    std::istringstream stream(bodyStr);
    std::string line;
    
    while (std::getline(stream, line)) {
        if (line.length() >= 2 && line.substr(0, 2) == "- ") {
            if (!inList) {
                result += "<ul style=\"margin: 4px 0; padding-left: 20px;\">\n";
                inList = true;
            }
            std::string content = line.length() > 2 ? line.substr(2) : "";
            result += "<li style=\"margin: 2px 0;\">" + content + "</li>\n";
        } else if (line.length() >= 3 && line.substr(0, 3) == "  -") {
            if (!inList) {
                result += "<ul style=\"margin: 4px 0; padding-left: 20px;\">\n";
                inList = true;
            }
            std::string content = line.length() > 3 ? line.substr(3) : "";
            result += "<li style=\"margin: 2px 0; margin-left: 15px;\">" + content + "</li>\n";
        } else {
            if (inList && !line.empty()) {
                result += "</ul>\n";
                inList = false;
            }
            result += line + "\n";
        }
    }
    
    if (inList) {
        result += "</ul>\n";
    }
    
    formattedNotes += "<div style=\"color: #d1d5db; line-height: 1.4; font-size: 12px;\">\n";
    formattedNotes += result;
    formattedNotes += "\n</div>";

    obs_data_release(releaseData);
    
    ErrorHandler::LogInfo("Successfully fetched release notes for " + std::string(tagName), ErrorHandler::Category::Network);
    return formattedNotes;
}

std::string GetPatchNotes()
{
    // Try to fetch from GitHub first
    std::string githubNotes = FetchLatestReleaseNotes();
    if (!githubNotes.empty()) {
        return githubNotes;
    }
    
    // Fallback to static notes if GitHub fetch fails
    ErrorHandler::LogInfo("Using fallback static patch notes", ErrorHandler::Category::UI);
    
    return R"(
<div style="color: #d1d5db; line-height: 1.3; font-size: 12px;">
    <h3 style="font-size: 14px; font-weight: 600; color: #f9fafb; margin: 0 0 6px 0;">📋 What's New & Recent Features</h3>
    <h4>🎉 What's New in v1.7.1</h4>
    <p style="margin: 0 0 4px 0;"><b>🔧 Enhanced Source Management:</b> Improved locking functionality</p>
    <p style="margin: 0 0 4px 0;"><b>🎵 Audio Monitoring:</b> Better refresh capabilities</p>
    <p style="margin: 0 0 4px 0;"><b>🌐 Browser Sources:</b> Enhanced refresh functionality</p>
    <p style="margin: 0 0 8px 0;"><b>🐛 Bug Fixes:</b> Stability improvements</p>
    <p style="margin: 0;"><b>🚀 Recent:</b> WebSocket API, Plugin Manager, Notifications, Settings UI</p>
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

void CreateSplashDialog()
{
    UIHelpers::ShowDialogOnUIThread([]() {
        QDialog* dialog = UIHelpers::CreateDialogWindow("StreamUP");
        dialog->setModal(false);
        dialog->setFixedSize(800, 600);
        dialog->setStyleSheet("QDialog { background: #1f2937; }");
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section in scrollable area
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet("QWidget#headerWidget { background: #1f2937; padding: 20px; }"); // Even padding all around
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setSpacing(4);
        headerLayout->setAlignment(Qt::AlignCenter);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        // StreamUP text logo
        QLabel* textLogoLabel = new QLabel();
        textLogoLabel->setObjectName("textLogoLabel");
        QPixmap textLogoPixmap;
        
        QStringList textLogoPaths = {
            ":/images/text_logo.png",
            "images/text_logo.png",
            "../images/text_logo.png",
            "./images/text_logo.png"
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
            textLogoLabel->setStyleSheet(R"(
                font-size: 24px; 
                font-weight: bold; 
                color: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #f472b6, stop:0.25 #a855f7, stop:0.5 #3b82f6, stop:1 #06b6d4); 
                margin: 0;
            )");
        }
        textLogoLabel->setAlignment(Qt::AlignCenter);
        
        QString versionText = QString("Advanced Toolkit for OBS Studio • Version %1").arg(PROJECT_VERSION);
        QLabel* versionLabel = new QLabel(versionText);
        versionLabel->setObjectName("versionLabel");
        versionLabel->setStyleSheet("font-size: 14px; color: #9ca3af; margin: 0;"); // Static size
        versionLabel->setAlignment(Qt::AlignCenter);
        
        headerLayout->addWidget(textLogoLabel);
        headerLayout->addWidget(versionLabel);

        // Content area with dark mode scrolling and overlay scrollbar
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setStyleSheet(R"(
            QScrollArea {
                background: #1f2937;
                border: none;
            }
            QScrollArea::corner {
                background: transparent;
            }
            QScrollBar:vertical {
                background: rgba(55, 65, 81, 0.8);
                width: 12px;
                border-radius: 12px;
                margin: 0px;
                border: none;
                position: absolute;
                right: 2px;
            }
            QScrollBar::handle:vertical {
                background: rgba(107, 114, 128, 0.9);
                border-radius: 12px;
                min-height: 20px;
                margin: 2px;
            }
            QScrollBar::handle:vertical:hover {
                background: rgba(156, 163, 175, 0.9);
            }
            QScrollBar::add-line:vertical,
            QScrollBar::sub-line:vertical {
                border: none;
                background: none;
                height: 0px;
            }
            QScrollBar::add-page:vertical,
            QScrollBar::sub-page:vertical {
                background: transparent;
            }
        )");
        
        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet("background: #1f2937;");
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(0, 10, 0, 10); // Add 10px top margin
        contentLayout->setSpacing(10);

        // Add header to the scrollable content
        contentLayout->addWidget(headerWidget);

        // Patch Notes Section - Much more compact
        QWidget* patchNotesCard = new QWidget();
        patchNotesCard->setStyleSheet(R"(
            QWidget {
                background: #374151;
                border: none;
                border-radius: 0px;
                padding: 0px;
            }
        )");
        QVBoxLayout* patchNotesLayout = new QVBoxLayout(patchNotesCard);
        patchNotesLayout->setContentsMargins(10, 10, 10, 10);
        
        // Fetch dynamic patch notes from GitHub
        std::string dynamicPatchNotes = GetPatchNotes();
        QString modernPatchNotes = QString::fromStdString(dynamicPatchNotes);
        
        QLabel* patchNotesLabel = UIHelpers::CreateRichTextLabel(modernPatchNotes, false, true);
        patchNotesLayout->addWidget(patchNotesLabel);
        contentLayout->addWidget(patchNotesCard);

        // Support Section - Much more compact

        QWidget* supportCard = new QWidget();
        supportCard->setStyleSheet(R"(
            QWidget {
                background: #1e3a8a;
                border: none;
                border-radius: 0px;
                padding: 0px;
            }
        )");
        QVBoxLayout* supportLayout = new QVBoxLayout(supportCard);
        supportLayout->setContentsMargins(10, 10, 10, 10);
        
        QString supportText = R"(
<div style="color: #dbeafe; line-height: 1.3; font-size: 12px;">
    <h3 style="font-size: 14px; font-weight: 600; color: #f9fafb; margin: 0 0 6px 0;">💖 Support StreamUP Development</h3>
    <p style="margin: 0;">StreamUP is developed by independent developers. Your support helps us continue!</p>
</div>
        )";
        
        QLabel* supportLabel = UIHelpers::CreateRichTextLabel(supportText, false, true);
        supportLayout->addWidget(supportLabel);
        
        // More compact donation buttons
        QHBoxLayout* donationLayout = new QHBoxLayout();
        donationLayout->setSpacing(6);
        donationLayout->setContentsMargins(0, 6, 0, 0);
        
        QPushButton* patreonBtn = new QPushButton("💝 Support on Patreon");
        patreonBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #f472b6, stop:1 #a855f7);
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 20px;
                font-weight: 500;
                font-size: 12px;
                min-height: 20px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #e879f9, stop:1 #9333ea);
            }
        )");
        QObject::connect(patreonBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://www.patreon.com/streamup"));
        });
        
        QPushButton* kofiBtn = new QPushButton("☕ Buy us a Coffee");
        kofiBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #3b82f6, stop:1 #06b6d4);
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 20px;
                font-weight: 500;
                font-size: 12px;
                min-height: 20px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #2563eb, stop:1 #0891b2);
            }
        )");
        QObject::connect(kofiBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://ko-fi.com/streamup"));
        });
        
        QPushButton* githubBtn = new QPushButton("⭐ Star on GitHub");
        githubBtn->setStyleSheet(R"(
            QPushButton {
                background: #1f2937;
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 20px;
                font-weight: 500;
                font-size: 12px;
                min-height: 20px;
            }
            QPushButton:hover {
                background: #111827;
            }
        )");
        QObject::connect(githubBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://github.com/streamup-plugins/streamup"));
        });
        
        donationLayout->addWidget(patreonBtn);
        donationLayout->addWidget(kofiBtn);
        donationLayout->addWidget(githubBtn);
        donationLayout->addStretch();
        
        supportLayout->addLayout(donationLayout);
        contentLayout->addWidget(supportCard);

        // Supporters Section - Much more compact
        QWidget* supportersCard = new QWidget();
        supportersCard->setStyleSheet(R"(
            QWidget {
                background: #581c87;
                border: none;
                border-radius: 0px;
                padding: 0px;
            }
        )");
        QVBoxLayout* supportersLayout = new QVBoxLayout(supportersCard);
        supportersLayout->setContentsMargins(10, 10, 10, 10);
        
        QString modernSupporters = R"(
<div style="color: #e9d5ff; line-height: 1.4; font-size: 13px;">
    <h3 style="font-size: 16px; font-weight: 600; color: #f9fafb; margin: 0 0 10px 0;">🙏 Thank You to Our Supporters!</h3>
    <p style="margin: 0 0 8px 0; font-style: italic;">Your support makes StreamUP possible!</p>
    
    <p style="margin: 0 0 6px 0;"><b style="color: #f3e8ff;">🌟 Diamond:</b> <span style="color: #d8b4fe;">StreamerName1, ContentCreator2, TechEnthusiast3</span></p>
    <p style="margin: 0 0 6px 0;"><b style="color: #f3e8ff;">💎 Gold:</b> <span style="color: #d8b4fe;">Supporter1, Supporter2, Supporter3, Supporter4</span></p>
    <p style="margin: 0 0 8px 0;"><b style="color: #f3e8ff;">⭐ Silver:</b> <span style="color: #d8b4fe;">Fan1, Fan2, Fan3, Fan4, Fan5, Fan6</span></p>
    
    <div style="text-align: center; margin-top: 10px; padding: 8px; background: rgba(139, 92, 246, 0.2); border-radius: 6px;">
        <p style="margin: 0; color: #f3e8ff; font-weight: 600; font-size: 12px;">Want to see your name here? <a href="https://www.patreon.com/streamup" style="color: #f3e8ff;">Join our supporters!</a></p>
    </div>
</div>
        )";
        
        QLabel* supportersLabel = UIHelpers::CreateRichTextLabel(modernSupporters, false, true);
        supportersLayout->addWidget(supportersLabel);
        contentLayout->addWidget(supportersCard);

        // Additional links - More compact
        QWidget* linksCard = new QWidget();
        linksCard->setStyleSheet(R"(
            QWidget {
                background: #374151;
                border: none;
                border-radius: 0px;
                padding: 0px;
            }
        )");
        QVBoxLayout* linksCardLayout = new QVBoxLayout(linksCard);
        linksCardLayout->setContentsMargins(10, 10, 10, 10);
        
        QString linksText = R"(
<div style="color: #d1d5db; line-height: 1.3; font-size: 12px;">
    <h3 style="font-size: 16px; font-weight: 600; color: #f9fafb; margin: 0 0 10px 0;">🔗 Useful Links</h3>
</div>
        )";
        
        QLabel* linksLabel = UIHelpers::CreateRichTextLabel(linksText, false, true);
        linksCardLayout->addWidget(linksLabel);
        
        QHBoxLayout* linksLayout = new QHBoxLayout();
        linksLayout->setSpacing(8);
        linksLayout->setContentsMargins(0, 6, 0, 0);
        
        QPushButton* docsBtn = new QPushButton("📖 Documentation");
        docsBtn->setStyleSheet(R"(
            QPushButton {
                background: #059669;
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 20px;
                font-weight: 500;
                font-size: 12px;
                min-height: 20px;
            }
            QPushButton:hover { background: #047857; }
        )");
        QObject::connect(docsBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://docs.streamup.tips"));
        });
        
        QPushButton* discordBtn = new QPushButton("💬 Discord Community");
        discordBtn->setStyleSheet(R"(
            QPushButton {
                background: #5865f2;
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 20px;
                font-weight: 500;
                font-size: 12px;
                min-height: 20px;
            }
            QPushButton:hover { background: #4752c4; }
        )");
        QObject::connect(discordBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://discord.gg/streamup"));
        });
        
        QPushButton* websiteBtn = new QPushButton("🌐 Website");
        websiteBtn->setStyleSheet(R"(
            QPushButton {
                background: #dc2626;
                color: white;
                border: none;
                padding: 10px 16px;
                border-radius: 20px;
                font-weight: 500;
                font-size: 12px;
                min-height: 20px;
            }
            QPushButton:hover { background: #b91c1c; }
        )");
        QObject::connect(websiteBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips"));
        });
        
        linksLayout->addWidget(docsBtn);
        linksLayout->addWidget(discordBtn);
        linksLayout->addWidget(websiteBtn);
        linksLayout->addStretch();
        
        linksCardLayout->addLayout(linksLayout);
        contentLayout->addWidget(linksCard);

        // Add Get Started button to the scrollable content at the bottom
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet("background: #1f2937; padding: 20px;");
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        
        QPushButton* closeBtn = new QPushButton("Get Started! 🚀");
        closeBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #3b82f6, stop:1 #1d4ed8);
                color: white;
                border: none;
                padding: 12px 32px;
                border-radius: 25px;
                font-weight: 600;
                font-size: 16px;
                min-height: 26px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0, 
                    stop:0 #2563eb, stop:1 #1e40af);
            }
        )");
        closeBtn->setDefault(true);
        
        QObject::connect(closeBtn, &QPushButton::clicked, [dialog]() {
            UpdateVersionTracking();
            dialog->close();
        });
        
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeBtn);
        buttonLayout->addStretch();
        
        contentLayout->addWidget(buttonWidget);

        scrollArea->setWidget(contentWidget);
        
        mainLayout->addWidget(scrollArea);

        dialog->setLayout(mainLayout);
        dialog->show();
    });
}

void ShowSplashScreenIfNeeded()
{
    // TESTING: Always show splash screen for development/testing
    CreateSplashDialog();
    ErrorHandler::LogInfo("Splash screen shown for testing (version " + std::string(PROJECT_VERSION) + ")", 
                         ErrorHandler::Category::UI);
    
    /* PRODUCTION CODE (commented out for testing):
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

    CreateSplashDialog();
    
    ErrorHandler::LogInfo("Splash screen shown for version " + std::string(PROJECT_VERSION), 
                         ErrorHandler::Category::UI);
    */
}

void ShowSplashScreen()
{
    CreateSplashDialog();
}

} // namespace SplashScreen
} // namespace StreamUP
