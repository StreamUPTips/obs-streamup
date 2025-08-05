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
#include <QTimer>
#include <QPointer>

namespace StreamUP {
namespace WebSocketWindow {

// Static pointers to track open dialogs
static QPointer<QDialog> normalWebSocketDialog;
static QPointer<QDialog> internalWebSocketDialog;

struct WebSocketCommand {
    QString name;
    QString description;
    QString category;
    bool isInternalTool = false; // Mark commands that are internal tools
};

// Define all available WebSocket commands organized by category
static const QList<WebSocketCommand> websocketCommands = {
    // Utility Commands
    {"GetStreamBitrate", "WebSocket.Command.GetStreamBitrate.Description", "WebSocket.Category.Utility", false},
    {"GetPluginVersion", "WebSocket.Command.GetPluginVersion.Description", "WebSocket.Category.Utility", true}, // Internal tool
    
    // Plugin Management
    {"CheckRequiredPlugins", "WebSocket.Command.CheckRequiredPlugins.Description", "WebSocket.Category.PluginManagement", true}, // Internal tool
    
    // Source Management
    {"ToggleLockAllSources", "WebSocket.Command.ToggleLockAllSources.Description", "WebSocket.Category.SourceManagement", false},
    {"ToggleLockCurrentSceneSources", "WebSocket.Command.ToggleLockCurrentSceneSources.Description", "WebSocket.Category.SourceManagement", false},
    {"RefreshAudioMonitoring", "WebSocket.Command.RefreshAudioMonitoring.Description", "WebSocket.Category.SourceManagement", false},
    {"RefreshBrowserSources", "WebSocket.Command.RefreshBrowserSources.Description", "WebSocket.Category.SourceManagement", false},
    {"GetSelectedSource", "WebSocket.Command.GetSelectedSource.Description", "WebSocket.Category.SourceManagement", false},
    
    // Transition Management
    {"GetShowTransition", "WebSocket.Command.GetShowTransition.Description", "WebSocket.Category.TransitionManagement", false},
    {"GetHideTransition", "WebSocket.Command.GetHideTransition.Description", "WebSocket.Category.TransitionManagement", false},
    {"SetShowTransition", "WebSocket.Command.SetShowTransition.Description", "WebSocket.Category.TransitionManagement", false},
    {"SetHideTransition", "WebSocket.Command.SetHideTransition.Description", "WebSocket.Category.TransitionManagement", false},
    
    // Source Properties
    {"GetBlendingMethod", "WebSocket.Command.GetBlendingMethod.Description", "WebSocket.Category.SourceProperties", false},
    {"SetBlendingMethod", "WebSocket.Command.SetBlendingMethod.Description", "WebSocket.Category.SourceProperties", false},
    {"GetDeinterlacing", "WebSocket.Command.GetDeinterlacing.Description", "WebSocket.Category.SourceProperties", false},
    {"SetDeinterlacing", "WebSocket.Command.SetDeinterlacing.Description", "WebSocket.Category.SourceProperties", false},
    {"GetScaleFiltering", "WebSocket.Command.GetScaleFiltering.Description", "WebSocket.Category.SourceProperties", false},
    {"SetScaleFiltering", "WebSocket.Command.SetScaleFiltering.Description", "WebSocket.Category.SourceProperties", false},
    {"GetDownmixMono", "WebSocket.Command.GetDownmixMono.Description", "WebSocket.Category.SourceProperties", false},
    {"SetDownmixMono", "WebSocket.Command.SetDownmixMono.Description", "WebSocket.Category.SourceProperties", false},
    
    // File Management
    {"GetRecordingOutputPath", "WebSocket.Command.GetRecordingOutputPath.Description", "WebSocket.Category.FileManagement", true}, // Internal tool
    {"GetVLCCurrentFile", "WebSocket.Command.GetVLCCurrentFile.Description", "WebSocket.Category.FileManagement", false},
    {"LoadStreamUpFile", "WebSocket.Command.LoadStreamUpFile.Description", "WebSocket.Category.FileManagement", true}, // Internal tool
    
    // UI Interaction
    {"OpenSourceProperties", "WebSocket.Command.OpenSourceProperties.Description", "WebSocket.Category.UIInteraction", false},
    {"OpenSourceFilters", "WebSocket.Command.OpenSourceFilters.Description", "WebSocket.Category.UIInteraction", false},
    {"OpenSourceInteraction", "WebSocket.Command.OpenSourceInteraction.Description", "WebSocket.Category.UIInteraction", false},
    {"OpenSceneFilters", "WebSocket.Command.OpenSceneFilters.Description", "WebSocket.Category.UIInteraction", false},
    
    // Video Capture Device Management
    {"ActivateAllVideoCaptureDevices", "WebSocket.Command.ActivateAllVideoCaptureDevices.Description", "WebSocket.Category.VideoDeviceManagement", false},
    {"DeactivateAllVideoCaptureDevices", "WebSocket.Command.DeactivateAllVideoCaptureDevices.Description", "WebSocket.Category.VideoDeviceManagement", false},
    {"RefreshAllVideoCaptureDevices", "WebSocket.Command.RefreshAllVideoCaptureDevices.Description", "WebSocket.Category.VideoDeviceManagement", false}
};

void ShowWebSocketWindow(bool showInternalTools)
{
    // Check if the appropriate dialog is already open
    QPointer<QDialog>& dialogRef = showInternalTools ? internalWebSocketDialog : normalWebSocketDialog;
    
    if (!dialogRef.isNull() && dialogRef->isVisible()) {
        // Bring existing dialog to front
        dialogRef->raise();
        dialogRef->activateWindow();
        return;
    }

    StreamUP::UIHelpers::ShowDialogOnUIThread([showInternalTools, &dialogRef]() {
        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog(obs_module_text("WebSocket.Window.Title"));
        
        // Start with compact size - will expand based on content
        dialog->resize(700, 500);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet(QString("QWidget#headerWidget { background: %1; padding: %2px %3px %4px %3px; }")
            .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL + StreamUP::UIStyles::Sizes::PADDING_MEDIUM) // More padding at top
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL)
            .arg(StreamUP::UIStyles::Sizes::PADDING_XL)); // Standard padding at bottom
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle(obs_module_text("WebSocket.Window.Header"));
        titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(titleLabel);
        
        // Add reduced spacing between title and description
        headerLayout->addSpacing(-StreamUP::UIStyles::Sizes::SPACING_SMALL);
        
        QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(obs_module_text("WebSocket.Window.Description"));
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
            QString translatedCategory = obs_module_text(cmd.category.toUtf8().constData());
            commandsByCategory[translatedCategory].append(cmd);
        }

        // Create sections for each category
        QStringList categoryOrder = {obs_module_text("WebSocket.Category.Utility"), obs_module_text("WebSocket.Category.PluginManagement"), obs_module_text("WebSocket.Category.SourceManagement"), obs_module_text("WebSocket.Category.SourceProperties"), 
                                   obs_module_text("WebSocket.Category.TransitionManagement"), obs_module_text("WebSocket.Category.FileManagement"), obs_module_text("WebSocket.Category.UIInteraction"), obs_module_text("WebSocket.Category.VideoDeviceManagement")};
        
        for (const QString& category : categoryOrder) {
            if (!commandsByCategory.contains(category) || commandsByCategory[category].isEmpty()) continue;
            
            // Category icon mapping
            QString categoryIcon;
            QString categoryStyle;
            if (category == obs_module_text("WebSocket.Category.Utility")) {
                categoryIcon = "⚙️";
                categoryStyle = "info";
            } else if (category == obs_module_text("WebSocket.Category.PluginManagement")) {
                categoryIcon = "🔌";
                categoryStyle = "success";
            } else if (category == obs_module_text("WebSocket.Category.SourceManagement")) {
                categoryIcon = "🎭";
                categoryStyle = "info";
            } else if (category == obs_module_text("WebSocket.Category.SourceProperties")) {
                categoryIcon = "🎨";
                categoryStyle = "warning";
            } else if (category == obs_module_text("WebSocket.Category.TransitionManagement")) {
                categoryIcon = "🔄";
                categoryStyle = "info";
            } else if (category == obs_module_text("WebSocket.Category.FileManagement")) {
                categoryIcon = "📁";
                categoryStyle = "success";
            } else if (category == obs_module_text("WebSocket.Category.UIInteraction")) {
                categoryIcon = "🖱️";
                categoryStyle = "warning";
            } else if (category == obs_module_text("WebSocket.Category.VideoDeviceManagement")) {
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
                QString translatedDescription = obs_module_text(cmd.description.toUtf8().constData());
                QWidget* commandWidget = CreateCommandWidget(cmd.name, translatedDescription);
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

        // Bottom button area - no background bar, just padded button
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet("background: transparent;"); // Remove background bar
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM); // Add padding all around

        QPushButton* closeButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("WebSocket.Button.Close"), "neutral");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        buttonLayout->addStretch(); // Center the button
        
        mainLayout->addWidget(buttonWidget);

        dialog->setLayout(mainLayout);
        
        // Store the dialog reference
        dialogRef = dialog;
        
        // Apply consistent sizing for websocket window
        StreamUP::UIStyles::ApplyConsistentSizing(dialog, 700, 1100, 500, 800);
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
    QPushButton* obsRawBtn = StreamUP::UIStyles::CreateStyledButton("OBS Raw", "info", 28, 80);
    obsRawBtn->setFixedSize(80, 28); // Set static size - bigger for "OBS Raw" text
    obsRawBtn->setToolTip(obs_module_text("WebSocket.Button.OBSRaw.Tooltip"));
    QObject::connect(obsRawBtn, &QPushButton::clicked, [obsRawBtn, obsRawJson]() {
        QApplication::clipboard()->setText(obsRawJson);
        
        // Visual feedback: briefly change button text and style
        QString originalText = obsRawBtn->text();
        obsRawBtn->setText(obs_module_text("WebSocket.Button.Copied"));
        obsRawBtn->setEnabled(false);
        
        // Change to success style temporarily
        obsRawBtn->setStyleSheet(StreamUP::UIStyles::GetButtonStyle(
            StreamUP::UIStyles::Colors::SUCCESS, StreamUP::UIStyles::Colors::SUCCESS_HOVER, 28));
        
        // Restore button after 1 second
        QTimer::singleShot(1000, [obsRawBtn, originalText]() {
            obsRawBtn->setText(originalText);
            obsRawBtn->setEnabled(true);
            // Restore original blue style
            obsRawBtn->setStyleSheet(StreamUP::UIStyles::GetButtonStyle(
                StreamUP::UIStyles::Colors::INFO, StreamUP::UIStyles::Colors::INFO_HOVER, 28));
        });
    });
    
    // CPH copy button  
    QString cphCommand = QString(R"(CPH.ObsSendRaw("CallVendorRequest", "{\"vendorName\":\"streamup\",\"requestType\":\"%1\",\"requestData\":{}}", 0);)").arg(command);
    QPushButton* cphBtn = StreamUP::UIStyles::CreateStyledButton("CPH", "info", 28, 70);
    cphBtn->setFixedSize(70, 28); // Set static size
    cphBtn->setToolTip(obs_module_text("WebSocket.Button.CPH.Tooltip"));
    QObject::connect(cphBtn, &QPushButton::clicked, [cphBtn, cphCommand]() {
        QApplication::clipboard()->setText(cphCommand);
        
        // Visual feedback: briefly change button text and style
        QString originalText = cphBtn->text();
        cphBtn->setText(obs_module_text("WebSocket.Button.Copied"));
        cphBtn->setEnabled(false);
        
        // Change to success style temporarily
        cphBtn->setStyleSheet(StreamUP::UIStyles::GetButtonStyle(
            StreamUP::UIStyles::Colors::SUCCESS, StreamUP::UIStyles::Colors::SUCCESS_HOVER, 28));
        
        // Restore button after 1 second
        QTimer::singleShot(1000, [cphBtn, originalText]() {
            cphBtn->setText(originalText);
            cphBtn->setEnabled(true);
            // Restore original blue style
            cphBtn->setStyleSheet(StreamUP::UIStyles::GetButtonStyle(
                StreamUP::UIStyles::Colors::INFO, StreamUP::UIStyles::Colors::INFO_HOVER, 28));
        });
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