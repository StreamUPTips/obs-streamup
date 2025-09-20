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
#include <util/platform.h>
#include <sstream>
#include <algorithm>

namespace StreamUP {
namespace SplashScreen {

// Splash dialog is now managed by DialogManager in ui-helpers

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
        result.replace(pos, 1, "<code style=\"background: #374151; padding: 2px 4px; border-radius: 3px; font-family: monospace; color: #e5e7eb;\">");
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
            std::string replacement = "<a href=\"" + linkUrl + "\" style=\"color: #60a5fa; text-decoration: underline;\">" + linkText + "</a>";
            
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
                    for (const QJsonValue& value : andiSupportersArray) {
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
                    for (const QJsonValue& value : streamupSupportersArray) {
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
        html += R"(<p style="margin: 8px 0; line-height: 1.8;">)";
        
        for (size_t i = 0; i < supportersData.andiSupporters.size(); ++i) {
            const auto& supporter = supportersData.andiSupporters[i];
            html += QString(R"(<span style="background-color: rgba(251, 191, 36, 0.2); color: #fde68a; padding: 3px 8px; border-radius: 4px; font-size: 12px; font-weight: 500;">%1</span>)")
                .arg(supporter.displayName.toHtmlEscaped());
            
            // Add spacing between names
            if (i < supportersData.andiSupporters.size() - 1) {
                html += "&nbsp;&nbsp;&nbsp;"; // Add some spaces between names
            }
        }
        html += R"(</p>)";
    }

    // StreamUP supporters section
    if (!supportersData.streamupSupporters.empty()) {
        html += R"(<h4 style="color: #a855f7; margin: 12px 0 8px 0; font-size: 14px;">💜 StreamUP Supporters</h4>)";
        html += R"(<p style="margin: 8px 0; line-height: 1.8;">)";
        
        for (size_t i = 0; i < supportersData.streamupSupporters.size(); ++i) {
            const auto& supporter = supportersData.streamupSupporters[i];
            html += QString(R"(<span style="background-color: rgba(168, 85, 247, 0.2); color: #e9d5ff; padding: 3px 8px; border-radius: 4px; font-size: 12px; font-weight: 500;">%1</span>)")
                .arg(supporter.displayName.toHtmlEscaped());
                
            // Add spacing between names
            if (i < supportersData.streamupSupporters.size() - 1) {
                html += "&nbsp;&nbsp;&nbsp;"; // Add some spaces between names
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
            result += "<hr style=\"border: none; border-top: 1px solid #374151; margin: 16px 0;\">\n";
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
            result += "<h2 style=\"color: #f9fafb; margin: 18px 0 10px 0; font-size: 18px; font-weight: 600;\">" + headerText + "</h2>\n";
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
            result += "<hr style=\"border: none; border-top: 1px solid #374151; margin: 16px 0;\">\n";
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
            result += "<h2 style=\"color: #3b82f6; margin: 18px 0 10px 0; font-size: 18px; font-weight: 600;\">" + headerText + "</h2>\n";
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
    
    std::string formattedNotes = "<div style=\"color: #d1d5db; line-height: 1.4; font-size: 12px;\">\n";
    formattedNotes += result;
    formattedNotes += "</div>";
    
    ErrorHandler::LogInfo("Successfully loaded local plugin info", ErrorHandler::Category::UI);
    return formattedNotes;
}

std::string GetWelcomeMessage()
{
    return std::string(R"(
<div style="color: #d1d5db; line-height: 1.4; font-size: 14px;">
    <h2 style="color: #3b82f6; margin: 18px 0 12px 0; font-size: 20px; font-weight: 600;">🎉 Welcome to StreamUP!</h2>
    
    <p style="margin: 12px 0; color: #f9fafb; font-size: 15px;">
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
        <a href="https://streamup.doras.click/docs" style="color: #60a5fa; text-decoration: underline; font-weight: 500;">📖 Read the Documentation</a>
    </p>
    
    <h3 style="color: #fbbf24; margin: 16px 0 8px 0; font-size: 16px;">💖 Support Our Development</h3>
    <p style="margin: 8px 0;">
        StreamUP is a passion project created with love for the OBS community. If you find it useful, please consider supporting our development efforts. Your support helps us continue adding new features and maintaining compatibility with the latest OBS versions.
    </p>
    <p style="margin: 8px 0;">
        Every contribution, no matter how small, makes a real difference and keeps this project thriving!
    </p>
    
    <div style="margin: 16px 0; padding: 12px; background: rgba(139, 92, 246, 0.1); border-left: 4px solid #a855f7; border-radius: 4px;">
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
            result += "<hr style=\"border: none; border-top: 1px solid #374151; margin: 16px 0;\">\n";
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
            result += "<h2 style=\"color: #3b82f6; margin: 18px 0 10px 0; font-size: 18px; font-weight: 600;\">" + headerText + "</h2>\n";
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
    
    std::string formattedNotes = "<div style=\"color: #d1d5db; line-height: 1.4; font-size: 12px;\">\n";
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
<div style="color: #d1d5db; line-height: 1.3; font-size: 12px;">
    <h2 style="color: #3b82f6; margin: 12px 0 8px 0;">What's New in )") + PROJECT_VERSION + R"(</h2>
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
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

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
                "    stop:0 #f472b6, stop:0.25 #a855f7, stop:0.5 #3b82f6, stop:1 #06b6d4);"
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

        // Welcome Section - Using modern group box
        QGroupBox* welcomeGroup = StreamUP::UIStyles::CreateStyledGroupBox("Welcome to StreamUP", "info");
        QVBoxLayout* welcomeLayout = new QVBoxLayout(welcomeGroup);
        welcomeLayout->setContentsMargins(0, 0, 0, 0);
        
        // Always show welcome message
        std::string welcomeMessage = GetWelcomeMessage();
        QString mainContent = QString::fromStdString(welcomeMessage);
        
        QLabel* mainContentLabel = UIHelpers::CreateRichTextLabel(mainContent, false, true, Qt::Alignment(), true);
        welcomeLayout->addWidget(mainContentLabel);
        contentLayout->addWidget(welcomeGroup);

        // Support Section - Using modern group box
        QGroupBox* supportGroup = StreamUP::UIStyles::CreateStyledGroupBox("Support StreamUP", "success");
        QVBoxLayout* supportLayout = new QVBoxLayout(supportGroup);
        supportLayout->setContentsMargins(0, 0, 0, 0);
        
        // Load support info from local markdown file
        std::string supportContent = LoadLocalSupportInfo();
        QString supportText = QString::fromStdString(supportContent);
        
        QLabel* supportLabel = UIHelpers::CreateRichTextLabel(supportText, false, true, Qt::Alignment(), true);
        supportLayout->addWidget(supportLabel);
        
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
        
        supportLayout->addLayout(donationLayout);
        contentLayout->addWidget(supportGroup);

        // Supporters Section - Using modern group box
        QGroupBox* supportersGroup = StreamUP::UIStyles::CreateStyledGroupBox("Thank You to Our Supporters!", "warning");
        QVBoxLayout* supportersLayout = new QVBoxLayout(supportersGroup);
        supportersLayout->setContentsMargins(0, 0, 0, 0);
        
        // Create supporters label that will be updated dynamically
        QLabel* supportersLabel = UIHelpers::CreateRichTextLabel(GenerateSupportersHTML(), false, true, Qt::Alignment(), true);
        supportersLabel->setObjectName("supportersLabel"); // Give it an object name for easy updates
        supportersLayout->addWidget(supportersLabel);
        contentLayout->addWidget(supportersGroup);
        
        // Set up a timer to refresh the supporters display every 2 seconds until loaded
        QTimer* refreshTimer = new QTimer(dialog);
        QObject::connect(refreshTimer, &QTimer::timeout, [supportersLabel, refreshTimer]() {
            supportersLabel->setText(GenerateSupportersHTML());
            if (supportersData.loaded) {
                refreshTimer->stop(); // Stop refreshing once data is loaded
            }
        });
        refreshTimer->start(2000); // Refresh every 2 seconds

        // Useful Links Section - Using modern group box
        QGroupBox* linksGroup = StreamUP::UIStyles::CreateStyledGroupBox("Useful Links", "info");
        QVBoxLayout* linksGroupLayout = new QVBoxLayout(linksGroup);
        linksGroupLayout->setContentsMargins(0, 0, 0, 0);
        
        QHBoxLayout* linksButtonLayout = new QHBoxLayout();
        linksButtonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
        linksButtonLayout->setContentsMargins(0, StreamUP::UIStyles::Sizes::SPACING_SMALL, 0, 0);
        
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
        
        linksGroupLayout->addLayout(linksButtonLayout);
        linksGroupLayout->addLayout(socialLinksLayout);
        contentLayout->addWidget(linksGroup);

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
        
        dialog->setLayout(mainLayout);
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

    CreateSplashDialog(condition);
    
    ErrorHandler::LogInfo("Splash screen shown for version " + std::string(PROJECT_VERSION), 
                         ErrorHandler::Category::UI);
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
