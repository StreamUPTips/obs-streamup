#include "websocket-window.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "notification-manager.hpp"
#include "../version.h"
#include <obs-module.h>
#include <QApplication>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QSpacerItem>
#include <QSizePolicy>
#include <QClipboard>
#include <QFrame>

namespace StreamUP {
namespace WebSocketWindow {

struct WebSocketCommand {
    QString name;
    QString description;
    QString category;
    bool isInternalTool = false; // Mark commands that are internal tools
};

// Define all available WebSocket commands organized by category
static const QList<WebSocketCommand> websocketCommands = {
    // Utility Commands
    {"GetStreamBitrate", "Get current stream bitrate in kbits/sec", "Utility", false},
    {"GetPluginVersion", "Get StreamUP plugin version", "Utility", true}, // Internal tool
    
    // Plugin Management
    {"CheckRequiredPlugins", "Check if required OBS plugins are installed and up to date", "Plugin Management", true}, // Internal tool
    
    // Source Management
    {"ToggleLockAllSources", "Toggle lock state for all sources across all scenes", "Source Management", false},
    {"ToggleLockCurrentSceneSources", "Toggle lock state for sources in current scene only", "Source Management", false},
    {"RefreshAudioMonitoring", "Refresh audio monitoring settings for all audio sources", "Source Management", false},
    {"RefreshBrowserSources", "Refresh all browser sources to reload their content", "Source Management", false},
    {"GetSelectedSource", "Get the name of currently selected source in current scene", "Source Management", false},
    
    // Transition Management
    {"GetShowTransition", "Get the show transition settings for scene items", "Transition Management", false},
    {"GetHideTransition", "Get the hide transition settings for scene items", "Transition Management", false},
    {"SetShowTransition", "Set the show transition settings for scene items", "Transition Management", false},
    {"SetHideTransition", "Set the hide transition settings for scene items", "Transition Management", false},
    
    // Source Properties
    {"GetBlendingMethod", "Get blending method for a specific source in scene", "Source Properties", false},
    {"SetBlendingMethod", "Set blending method for a specific source in scene", "Source Properties", false},
    {"GetDeinterlacing", "Get deinterlacing settings for a source", "Source Properties", false},
    {"SetDeinterlacing", "Set deinterlacing settings for a source", "Source Properties", false},
    {"GetScaleFiltering", "Get scale filtering method for a source in scene", "Source Properties", false},
    {"SetScaleFiltering", "Set scale filtering method for a source in scene", "Source Properties", false},
    {"GetDownmixMono", "Get mono downmix setting for an audio source", "Source Properties", false},
    {"SetDownmixMono", "Set mono downmix setting for an audio source", "Source Properties", false},
    
    // File Management
    {"GetRecordingOutputPath", "Get current recording output file path", "File Management", true}, // Internal tool
    {"GetVLCCurrentFile", "Get current file/title from VLC source", "File Management", false},
    {"LoadStreamUpFile", "Load a .streamup file from specified path", "File Management", true}, // Internal tool
    
    // UI Interaction
    {"OpenSourceProperties", "Open properties dialog for currently selected source", "UI Interaction", false},
    {"OpenSourceFilters", "Open filters dialog for currently selected source", "UI Interaction", false},
    {"OpenSourceInteraction", "Open interaction window for currently selected source", "UI Interaction", false},
    {"OpenSceneFilters", "Open filters dialog for current scene", "UI Interaction", false},
    
    // Video Capture Device Management
    {"ActivateAllVideoCaptureDevices", "Activate all video capture devices in all scenes", "Video Device Management", false},
    {"DeactivateAllVideoCaptureDevices", "Deactivate all video capture devices in all scenes", "Video Device Management", false},
    {"RefreshAllVideoCaptureDevices", "Refresh all video capture devices in all scenes", "Video Device Management", false}
};

void ShowWebSocketWindow(bool showInternalTools)
{
    StreamUP::UIHelpers::ShowDialogOnUIThread([showInternalTools]() {
        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog("StreamUP WebSocket Commands");
        
        // Start with compact size - will expand based on content
        dialog->resize(700, 500);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px; }")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL));
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle("🌐 WebSocket Commands");
        titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(titleLabel);
        
        QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription("Complete list of available WebSocket commands for external control");
        headerLayout->addWidget(subtitleLabel);
        
        mainLayout->addWidget(headerWidget);

        // Content area with scroll
        QScrollArea* scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
            StreamUP::UIStyles::Sizes::PADDING_XL, 
            StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
            StreamUP::UIStyles::Sizes::PADDING_XL);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

        // Group commands by category, filtering out internal tools if Shift is not held
        QMap<QString, QList<WebSocketCommand>> commandsByCategory;
        for (const auto& cmd : websocketCommands) {
            // Skip internal tools unless Shift key was held
            if (cmd.isInternalTool && !showInternalTools) {
                continue;
            }
            commandsByCategory[cmd.category].append(cmd);
        }

        // Create sections for each category
        QStringList categoryOrder = {"Utility", "Plugin Management", "Source Management", "Source Properties", 
                                   "Transition Management", "File Management", "UI Interaction", "Video Device Management"};
        
        for (const QString& category : categoryOrder) {
            if (!commandsByCategory.contains(category) || commandsByCategory[category].isEmpty()) continue;
            
            // Category icon mapping
            QString categoryIcon;
            QString categoryStyle;
            if (category == "Utility") {
                categoryIcon = "⚙️";
                categoryStyle = "info";
            } else if (category == "Plugin Management") {
                categoryIcon = "🔌";
                categoryStyle = "success";
            } else if (category == "Source Management") {
                categoryIcon = "🎭";
                categoryStyle = "info";
            } else if (category == "Source Properties") {
                categoryIcon = "🎨";
                categoryStyle = "warning";
            } else if (category == "Transition Management") {
                categoryIcon = "🔄";
                categoryStyle = "info";
            } else if (category == "File Management") {
                categoryIcon = "📁";
                categoryStyle = "success";
            } else if (category == "UI Interaction") {
                categoryIcon = "🖱️";
                categoryStyle = "warning";
            } else if (category == "Video Device Management") {
                categoryIcon = "🎥";
                categoryStyle = "error";
            }
            
            QGroupBox* categoryGroupBox = StreamUP::UIStyles::CreateStyledGroupBox(categoryIcon + " " + category, categoryStyle);
            
            QVBoxLayout* categoryLayout = new QVBoxLayout(categoryGroupBox);
            categoryLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
                0, // No top padding
                StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
                0); // No bottom padding
            categoryLayout->setSpacing(0); // No spacing since we handle it in the widget padding
            
            // Add commands for this category
            const auto& commands = commandsByCategory[category];
            for (int i = 0; i < commands.size(); ++i) {
                const auto& cmd = commands[i];
                QWidget* commandWidget = CreateCommandWidget(cmd.name, cmd.description);
                categoryLayout->addWidget(commandWidget);
                
                // Add separator line between commands (but not after the last one)
                if (i < commands.size() - 1) {
                    QFrame* separator = new QFrame();
                    separator->setFrameShape(QFrame::HLine);
                    separator->setFrameShadow(QFrame::Plain);
                    separator->setStyleSheet(QString(
                        "QFrame {"
                        "color: rgba(113, 128, 150, 0.3);"
                        "background-color: rgba(113, 128, 150, 0.3);"
                        "border: none;"
                        "margin: 0px;"
                        "max-height: 1px;"
                        "}"));
                    categoryLayout->addWidget(separator);
                }
            }
            
            contentLayout->addWidget(categoryGroupBox);
        }

        // Add stretch to push content to top
        contentLayout->addStretch();

        scrollArea->setWidget(contentWidget);
        mainLayout->addWidget(scrollArea);

        // Bottom button area
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet(QString("background: %1; padding: %2px;")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM));
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 0);

        QPushButton* closeButton = StreamUP::UIStyles::CreateStyledButton("Close", "neutral");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addWidget(buttonWidget);

        dialog->setLayout(mainLayout);
        
        // Apply dynamic sizing for websocket window
        StreamUP::UIStyles::ApplyDynamicSizing(dialog, 700, 1100, 500, 800);
        dialog->show();
    });
}

QWidget* CreateCommandWidget(const QString& command, const QString& description)
{
    QWidget* widget = new QWidget();
    widget->setStyleSheet(QString(
        "QWidget {"
        "background: transparent;"
        "border: none;"
        "padding: 0px;" // No padding on the widget itself
        "}"));
    
    QHBoxLayout* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3, 0, StreamUP::UIStyles::Sizes::PADDING_SMALL + 3); // Add margin to layout instead
    layout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
    // Command info section - use vertical layout but center it
    QVBoxLayout* infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2); // Tight spacing between name and description
    infoLayout->setContentsMargins(0, 0, 0, 0);
    
    // Command name
    QLabel* nameLabel = new QLabel(command);
    nameLabel->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "font-weight: bold;"
        "background: transparent;"
        "border: none;"
        "margin: 0px;"
        "padding: 0px;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
    
    // Command description
    QLabel* descLabel = new QLabel(description);
    descLabel->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "background: transparent;"
        "border: none;"
        "margin: 0px;"
        "padding: 0px;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
    descLabel->setWordWrap(true);
    
    infoLayout->addWidget(nameLabel);
    infoLayout->addWidget(descLabel);
    
    // Create a wrapper widget for info that centers the content vertically
    QWidget* infoWrapper = new QWidget();
    QVBoxLayout* wrapperLayout = new QVBoxLayout(infoWrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    wrapperLayout->addStretch(); // Add stretch above
    wrapperLayout->addLayout(infoLayout);
    wrapperLayout->addStretch(); // Add stretch below
    
    // Button section - also center vertically
    QVBoxLayout* buttonWrapperLayout = new QVBoxLayout();
    buttonWrapperLayout->setContentsMargins(0, 0, 0, 0);
    buttonWrapperLayout->addStretch(); // Add stretch above
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    
    // OBS Raw copy button
    QString obsRawJson = QString(R"({"requestType":"CallVendorRequest","requestData":{"vendorName":"streamup","requestType":"%1","requestData":{}}})").arg(command);
    QPushButton* obsRawBtn = StreamUP::UIStyles::CreateStyledButton("OBS Raw", "info", 28, 70);
    QObject::connect(obsRawBtn, &QPushButton::clicked, [obsRawJson]() {
        QApplication::clipboard()->setText(obsRawJson);
        StreamUP::NotificationManager::SendInfoNotification("WebSocket Command", "OBS Raw command copied to clipboard");
    });
    
    // CPH copy button  
    QString cphCommand = QString(R"(CPH.ObsSendRaw("CallVendorRequest", "{\"vendorName\":\"streamup\",\"requestType\":\"%1\",\"requestData\":{}}", 0);)").arg(command);
    QPushButton* cphBtn = StreamUP::UIStyles::CreateStyledButton("CPH", "info", 28, 70);
    QObject::connect(cphBtn, &QPushButton::clicked, [cphCommand]() {
        QApplication::clipboard()->setText(cphCommand);
        StreamUP::NotificationManager::SendInfoNotification("WebSocket Command", "CPH command copied to clipboard");
    });
    
    buttonLayout->addWidget(obsRawBtn);
    buttonLayout->addWidget(cphBtn);
    
    buttonWrapperLayout->addLayout(buttonLayout);
    buttonWrapperLayout->addStretch(); // Add stretch below
    
    // Add layouts to main layout
    layout->addWidget(infoWrapper, 1); // Give info section more space
    layout->addLayout(buttonWrapperLayout, 0); // Buttons take minimal space
    
    return widget;
}


} // namespace WebSocketWindow
} // namespace StreamUP