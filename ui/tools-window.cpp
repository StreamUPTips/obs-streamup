#include "tools-window.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "source-manager.hpp"
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

namespace StreamUP {
namespace ToolsWindow {

void ShowToolsWindow()
{
    StreamUP::UIHelpers::ShowDialogOnUIThread([]() {
        QDialog* dialog = StreamUP::UIStyles::CreateStyledDialog("StreamUP Tools");
        
        // Start with compact size - will expand based on content
        dialog->resize(600, 450);
        
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
        
        QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle("🛠️ StreamUP Tools");
        headerLayout->addWidget(titleLabel);
        
        QLabel* subtitleLabel = StreamUP::UIStyles::CreateStyledDescription("Powerful tools to manage your OBS setup efficiently");
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

        // Source Management Section
        QGroupBox* sourceGroupBox = StreamUP::UIStyles::CreateStyledGroupBox("🎭 Source Management", "info");
        
        QVBoxLayout* sourceGroupLayout = new QVBoxLayout(sourceGroupBox);
        sourceGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_XL, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
        sourceGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Description
        QLabel* sourceDesc = StreamUP::UIStyles::CreateStyledContent("Tools for managing and controlling your OBS sources");
        sourceDesc->setAlignment(Qt::AlignLeft);
        sourceGroupLayout->addWidget(sourceDesc);
        
        // Source buttons in 2x1 grid
        QHBoxLayout* sourceRow1Layout = new QHBoxLayout();
        sourceRow1Layout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Lock Current Scene Sources button
        QPushButton* lockCurrentBtn = StreamUP::UIStyles::CreateStyledButton("Lock Current Scene Sources", "info");
        lockCurrentBtn->setMinimumHeight(70);
        QObject::connect(lockCurrentBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "LockAllCurrentSources", 
                "LockAllCurrentSourcesInfo1", "LockAllCurrentSourcesInfo2", "LockAllCurrentSourcesInfo3",
                []() { StreamUP::SourceManager::ToggleLockSourcesInCurrentScene(); },
                "LockAllCurrentSourcesHowTo1", "LockAllCurrentSourcesHowTo2", 
                "LockAllCurrentSourcesHowTo3", "LockAllCurrentSourcesHowTo4", "ToggleLockCurrentSceneSources");
        });
        
        // Lock All Sources button
        QPushButton* lockAllBtn = StreamUP::UIStyles::CreateStyledButton("Lock All Sources", "warning");
        lockAllBtn->setMinimumHeight(70);
        QObject::connect(lockAllBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "LockAllSources", 
                "LockAllSourcesInfo1", "LockAllSourcesInfo2", "LockAllSourcesInfo3",
                []() { StreamUP::SourceManager::ToggleLockAllSources(); },
                "LockAllSourcesHowTo1", "LockAllSourcesHowTo2", 
                "LockAllSourcesHowTo3", "LockAllSourcesHowTo4", "ToggleLockAllSources");
        });
        
        sourceRow1Layout->addWidget(lockCurrentBtn);
        sourceRow1Layout->addWidget(lockAllBtn);
        sourceGroupLayout->addLayout(sourceRow1Layout);
        
        contentLayout->addWidget(sourceGroupBox);

        // Audio & Video Section
        QGroupBox* avGroupBox = StreamUP::UIStyles::CreateStyledGroupBox("🔊 Audio & Video", "info");
        
        QVBoxLayout* avGroupLayout = new QVBoxLayout(avGroupBox);
        avGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_XL, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
            StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
        avGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Description
        QLabel* avDesc = StreamUP::UIStyles::CreateStyledContent("Manage audio monitoring and video capture devices");
        avDesc->setAlignment(Qt::AlignLeft);
        avGroupLayout->addWidget(avDesc);
        
        // Audio/Video buttons in 2x2 grid
        QHBoxLayout* avRow1Layout = new QHBoxLayout();
        avRow1Layout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Refresh Audio Monitoring button
        QPushButton* audioBtn = StreamUP::UIStyles::CreateStyledButton("Refresh Audio Monitoring", "success");
        audioBtn->setMinimumHeight(70);
        QObject::connect(audioBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "RefreshAudioMonitoring", 
                "RefreshAudioMonitoringInfo1", "RefreshAudioMonitoringInfo2", "RefreshAudioMonitoringInfo3",
                []() { obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr); },
                "RefreshAudioMonitoringHowTo1", "RefreshAudioMonitoringHowTo2", 
                "RefreshAudioMonitoringHowTo3", "RefreshAudioMonitoringHowTo4", "RefreshAudioMonitoring");
        });
        
        // Refresh Browser Sources button
        QPushButton* browserBtn = StreamUP::UIStyles::CreateStyledButton("Refresh Browser Sources", "error");
        browserBtn->setMinimumHeight(70);
        QObject::connect(browserBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "RefreshBrowserSources", 
                "RefreshBrowserSourcesInfo1", "RefreshBrowserSourcesInfo2", "RefreshBrowserSourcesInfo3",
                []() { obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr); },
                "RefreshBrowserSourcesHowTo1", "RefreshBrowserSourcesHowTo2", 
                "RefreshBrowserSourcesHowTo3", "RefreshBrowserSourcesHowTo4", "RefreshBrowserSources");
        });
        
        avRow1Layout->addWidget(audioBtn);
        avRow1Layout->addWidget(browserBtn);
        avGroupLayout->addLayout(avRow1Layout);
        
        // Second row for video device button (centered)
        QHBoxLayout* avRow2Layout = new QHBoxLayout();
        avRow2Layout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
        // Manage Video Capture Devices button
        QPushButton* videoBtn = StreamUP::UIStyles::CreateStyledButton("Manage Video Capture Devices", "info");
        videoBtn->setMinimumHeight(70);
        QObject::connect(videoBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowVideoDeviceOptionsInline(scrollArea, contentWidget);
        });
        
        // Add spacer, button, spacer to center the video button
        avRow2Layout->addStretch(1);
        avRow2Layout->addWidget(videoBtn, 2);
        avRow2Layout->addStretch(1);
        avGroupLayout->addLayout(avRow2Layout);
        
        contentLayout->addWidget(avGroupBox);

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
        
        // Apply dynamic sizing for tools window
        StreamUP::UIStyles::ApplyDynamicSizing(dialog, 600, 1000, 450, 700);
        dialog->show();
    });
}

QPushButton* CreateToolButton(const QString& title, const QString& description, std::function<void()> action)
{
    QPushButton* button = new QPushButton();
    button->setMinimumHeight(60);
    button->setMaximumHeight(60);
    
    // Set the button layout
    QVBoxLayout* buttonLayout = new QVBoxLayout(button);
    buttonLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_SMALL + 2, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_SMALL + 2);
    buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);
    
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "font-weight: bold;"
        "margin: 0px;"
        "padding: 0px;"
        "background: transparent;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL + 1));
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    
    QLabel* descLabel = new QLabel(description);
    descLabel->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "margin: 0px;"
        "padding: 0px;"
        "line-height: 1.3;"
        "background: transparent;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY + 1));
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    descLabel->setWordWrap(true);
    
    buttonLayout->addWidget(titleLabel);
    buttonLayout->addWidget(descLabel);
    buttonLayout->addStretch();
    
    button->setStyleSheet(QString(
        "QPushButton {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2);"
        "border: 1px solid %3;"
        "border-radius: %4px;"
        "text-align: left;"
        "padding: 0px;"
        "}"
        "QPushButton:hover {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %5, stop:1 %1);"
        "border: 1px solid %6;"
        "}"
        "QPushButton:pressed {"
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %2, stop:1 %7);"
        "}")
        .arg(StreamUP::UIStyles::Colors::BACKGROUND_HOVER)
        .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
        .arg(StreamUP::UIStyles::Colors::BORDER_LIGHT)
        .arg(15)
        .arg(StreamUP::UIStyles::Colors::BORDER_LIGHT)
        .arg(StreamUP::UIStyles::Colors::TEXT_DISABLED)
        .arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    
    QObject::connect(button, &QPushButton::clicked, [action]() {
        action();
    });
    
    return button;
}

void ShowVideoDeviceOptions(QDialog* parentDialog)
{
    // Create a new dialog within the existing window
    QDialog* optionsDialog = new QDialog(parentDialog);
    optionsDialog->setWindowTitle("Video Capture Device Management");
    optionsDialog->setStyleSheet(StreamUP::UIStyles::GetDialogStyle());
    optionsDialog->setFixedSize(500, 300);
    optionsDialog->setModal(true);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(optionsDialog);
    mainLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    mainLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Title
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle("🎥 Video Capture Device Management");
    mainLayout->addWidget(titleLabel);
    
    // Description
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription("Choose an action to perform on all video capture devices in your scenes:");
    mainLayout->addWidget(descLabel);
    
    // Action buttons
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
    // Activate button
    QPushButton* activateBtn = StreamUP::UIStyles::CreateStyledButton("Activate All Video Devices", "success");
    QObject::connect(activateBtn, &QPushButton::clicked, [optionsDialog]() {
        StreamUP::SourceManager::ActivateAllVideoCaptureDevicesDialog();
        optionsDialog->close();
    });
    
    // Deactivate button
    QPushButton* deactivateBtn = StreamUP::UIStyles::CreateStyledButton("Deactivate All Video Devices", "error");
    QObject::connect(deactivateBtn, &QPushButton::clicked, [optionsDialog]() {
        StreamUP::SourceManager::DeactivateAllVideoCaptureDevicesDialog();
        optionsDialog->close();
    });
    
    // Refresh button
    QPushButton* refreshBtn = StreamUP::UIStyles::CreateStyledButton("Refresh All Video Devices", "info");
    QObject::connect(refreshBtn, &QPushButton::clicked, [optionsDialog]() {
        StreamUP::SourceManager::RefreshAllVideoCaptureDevicesDialog();
        optionsDialog->close();
    });
    
    buttonLayout->addWidget(activateBtn);
    buttonLayout->addWidget(deactivateBtn);
    buttonLayout->addWidget(refreshBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // Close button
    QHBoxLayout* closeLayout = new QHBoxLayout();
    QPushButton* closeBtn = StreamUP::UIStyles::CreateStyledButton("Cancel", "neutral");
    QObject::connect(closeBtn, &QPushButton::clicked, [optionsDialog]() {
        optionsDialog->close();
    });
    
    closeLayout->addStretch();
    closeLayout->addWidget(closeBtn);
    mainLayout->addLayout(closeLayout);
    
    optionsDialog->exec();
}

void ShowVideoDeviceOptionsInline(QScrollArea* scrollArea, QWidget* originalContent)
{
    // Store the current widget temporarily
    QWidget* currentWidget = scrollArea->takeWidget();
    
    // Create replacement content widget
    QWidget* optionsWidget = new QWidget();
    optionsWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsWidget);
    optionsLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    optionsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Back button and title section
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QPushButton* backButton = StreamUP::UIStyles::CreateStyledButton("← Back to Tools", "neutral");
    QObject::connect(backButton, &QPushButton::clicked, [scrollArea, originalContent, optionsWidget]() {
        // Safely switch back to original content
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        optionsWidget->deleteLater(); // Schedule for deletion
    });
    
    headerLayout->addWidget(backButton);
    headerLayout->addStretch();
    
    optionsLayout->addLayout(headerLayout);
    
    // Title
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle("🎥 Video Capture Device Management");
    optionsLayout->addWidget(titleLabel);
    
    // Description
    QLabel* descLabel = StreamUP::UIStyles::CreateStyledDescription("Choose an action to perform on all video capture devices in your scenes:");
    optionsLayout->addWidget(descLabel);
    
    // Options group
    QGroupBox* optionsGroup = StreamUP::UIStyles::CreateStyledGroupBox("Device Actions", "info");
    
    QVBoxLayout* optionsGroupLayout = new QVBoxLayout(optionsGroup);
    optionsGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
    optionsGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
    // Action buttons with larger sizing
    QPushButton* activateBtn = StreamUP::UIStyles::CreateStyledButton("Activate All Video Devices", "success");
    activateBtn->setMinimumHeight(60);
    QObject::connect(activateBtn, &QPushButton::clicked, [scrollArea, originalContent, optionsWidget]() {
        StreamUP::SourceManager::ActivateAllVideoCaptureDevicesDialog();
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        optionsWidget->deleteLater(); // Schedule for deletion
    });
    
    QPushButton* deactivateBtn = StreamUP::UIStyles::CreateStyledButton("Deactivate All Video Devices", "error");
    deactivateBtn->setMinimumHeight(60);
    QObject::connect(deactivateBtn, &QPushButton::clicked, [scrollArea, originalContent, optionsWidget]() {
        StreamUP::SourceManager::DeactivateAllVideoCaptureDevicesDialog();
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        optionsWidget->deleteLater(); // Schedule for deletion
    });
    
    QPushButton* refreshBtn = StreamUP::UIStyles::CreateStyledButton("Refresh All Video Devices", "info");
    refreshBtn->setMinimumHeight(60);
    QObject::connect(refreshBtn, &QPushButton::clicked, [scrollArea, originalContent, optionsWidget]() {
        StreamUP::SourceManager::RefreshAllVideoCaptureDevicesDialog();
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        optionsWidget->deleteLater(); // Schedule for deletion
    });
    
    optionsGroupLayout->addWidget(activateBtn);
    optionsGroupLayout->addWidget(deactivateBtn);
    optionsGroupLayout->addWidget(refreshBtn);
    
    optionsLayout->addWidget(optionsGroup);
    optionsLayout->addStretch();
    
    // Replace the content in the scroll area
    scrollArea->setWidget(optionsWidget);
}

void ShowToolDetailInline(QScrollArea* scrollArea, QWidget* originalContent, const char* titleKey,
                         const char* info1Key, const char* info2Key, const char* info3Key,
                         std::function<void()> action, const char* howTo1Key, const char* howTo2Key,
                         const char* howTo3Key, const char* howTo4Key, const char* websocketCommand)
{
    // Store the current widget temporarily
    QWidget* currentWidget = scrollArea->takeWidget();
    
    // Create replacement content widget
    QWidget* detailWidget = new QWidget();
    detailWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BACKGROUND_DARK));
    QVBoxLayout* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_XL + 5, 
        StreamUP::UIStyles::Sizes::PADDING_XL);
    detailLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
    
    // Back button and title section
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QPushButton* backButton = StreamUP::UIStyles::CreateStyledButton("← Back to Tools", "neutral");
    QObject::connect(backButton, &QPushButton::clicked, [scrollArea, originalContent, detailWidget]() {
        // Safely switch back to original content
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        detailWidget->deleteLater(); // Schedule for deletion
    });
    
    headerLayout->addWidget(backButton);
    headerLayout->addStretch();
    
    detailLayout->addLayout(headerLayout);
    
    // Title
    QString titleStr = obs_module_text(titleKey);
    QLabel* titleLabel = StreamUP::UIStyles::CreateStyledTitle("🛠️ " + titleStr);
    detailLayout->addWidget(titleLabel);
    
    // Main info section
    QGroupBox* infoGroup = StreamUP::UIStyles::CreateStyledGroupBox("Tool Information", "info");
    
    QVBoxLayout* infoGroupLayout = new QVBoxLayout(infoGroup);
    infoGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
    infoGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
    
    // Info text 1
    QString info1Str = obs_module_text(info1Key);
    QLabel* info1Label = new QLabel(info1Str);
    info1Label->setStyleSheet(QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "font-weight: bold;"
        "margin: 0px;"
        "padding: 0px;"
        "background: transparent;"
        "border: none;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
    info1Label->setWordWrap(true);
    info1Label->setTextFormat(Qt::PlainText);
    infoGroupLayout->addWidget(info1Label);
    
    // Info text 2
    QString info2Str = obs_module_text(info2Key);
    QLabel* info2Label = StreamUP::UIStyles::CreateStyledContent(info2Str);
    info2Label->setAlignment(Qt::AlignLeft);
    info2Label->setTextFormat(Qt::PlainText);
    infoGroupLayout->addWidget(info2Label);
    
    // Info text 3
    QString info3Str = obs_module_text(info3Key);
    QLabel* info3Label = StreamUP::UIStyles::CreateStyledContent(info3Str);
    info3Label->setAlignment(Qt::AlignLeft);
    info3Label->setTextFormat(Qt::PlainText);
    infoGroupLayout->addWidget(info3Label);
    
    detailLayout->addWidget(infoGroup);
    
    // How to use section
    QGroupBox* howToGroup = StreamUP::UIStyles::CreateStyledGroupBox("How To Use", "info");
    
    QVBoxLayout* howToGroupLayout = new QVBoxLayout(howToGroup);
    howToGroupLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_XL, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
        StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
    howToGroupLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_TINY);
    
    // How-to steps
    QString howTo1Str = obs_module_text(howTo1Key);
    QString howTo2Str = obs_module_text(howTo2Key);
    QString howTo3Str = obs_module_text(howTo3Key);
    QString howTo4Str = obs_module_text(howTo4Key);
    
    QString stepStyle = QString(
        "QLabel {"
        "color: %1;"
        "font-size: %2px;"
        "margin: 0px;"
        "padding: 0px;"
        "background: transparent;"
        "border: none;"
        "}")
        .arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
        .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_TINY);
    
    QLabel* howTo1Label = new QLabel("1. " + howTo1Str);
    howTo1Label->setStyleSheet(stepStyle);
    howTo1Label->setWordWrap(true);
    howToGroupLayout->addWidget(howTo1Label);
    
    QLabel* howTo2Label = new QLabel("2. " + howTo2Str);
    howTo2Label->setStyleSheet(stepStyle);
    howTo2Label->setWordWrap(true);
    howToGroupLayout->addWidget(howTo2Label);
    
    QLabel* howTo3Label = new QLabel("3. " + howTo3Str);
    howTo3Label->setStyleSheet(stepStyle);
    howTo3Label->setWordWrap(true);
    howToGroupLayout->addWidget(howTo3Label);
    
    QLabel* howTo4Label = new QLabel("4. " + howTo4Str);
    howTo4Label->setStyleSheet(stepStyle);
    howTo4Label->setWordWrap(true);
    howToGroupLayout->addWidget(howTo4Label);
    
    detailLayout->addWidget(howToGroup);
    
    // Action buttons layout
    QHBoxLayout* actionButtonsLayout = new QHBoxLayout();
    actionButtonsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
    
    // WebSocket JSON copy button (if websocketCommand is provided)
    if (websocketCommand) {
        QPushButton* copyJsonBtn = StreamUP::UIStyles::CreateStyledButton(obs_module_text("CopyWebsocketJson"), "info");
        copyJsonBtn->setMinimumHeight(60);
        
        // Generate WebSocket JSON
        QString websocketJson = QString(R"({"requestType":"CallVendorRequest","requestData":{"vendorName":"streamup","requestType":"%1","requestData":{}}})").arg(websocketCommand);
        
        QObject::connect(copyJsonBtn, &QPushButton::clicked, [websocketJson]() {
            StreamUP::UIHelpers::CopyToClipboard(websocketJson);
            StreamUP::NotificationManager::SendInfoNotification("WebSocket JSON", "Copied to clipboard successfully");
        });
        
        actionButtonsLayout->addWidget(copyJsonBtn);
    }
    
    // Execute button
    QPushButton* executeBtn = StreamUP::UIStyles::CreateStyledButton("Execute " + titleStr, "success");
    executeBtn->setMinimumHeight(60);
    QObject::connect(executeBtn, &QPushButton::clicked, [scrollArea, originalContent, detailWidget, action, titleStr]() {
        action();
        StreamUP::NotificationManager::SendInfoNotification(titleStr, "Tool executed successfully");
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        detailWidget->deleteLater(); // Schedule for deletion
    });
    
    actionButtonsLayout->addWidget(executeBtn);
    detailLayout->addLayout(actionButtonsLayout);
    detailLayout->addStretch();
    
    // Replace the content in the scroll area
    scrollArea->setWidget(detailWidget);
}

} // namespace ToolsWindow
} // namespace StreamUP