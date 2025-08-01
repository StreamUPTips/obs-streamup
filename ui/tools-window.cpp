#include "tools-window.hpp"
#include "ui-helpers.hpp"
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
        QDialog* dialog = StreamUP::UIHelpers::CreateDialogWindow("StreamUP Tools");
        dialog->setStyleSheet("QDialog { background: #13171f; }");
        dialog->resize(900, 650);
        dialog->setMinimumSize(800, 550);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Header section
        QWidget* headerWidget = new QWidget();
        headerWidget->setObjectName("headerWidget");
        headerWidget->setStyleSheet("QWidget#headerWidget { background: #13171f; padding: 20px; }");
        
        QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* titleLabel = new QLabel("🛠️ StreamUP Tools");
        titleLabel->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 24px;
                font-weight: bold;
                margin: 0px;
                padding: 0px;
            }
        )");
        titleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(titleLabel);
        
        QLabel* subtitleLabel = new QLabel("Powerful tools to manage your OBS setup efficiently");
        subtitleLabel->setStyleSheet(R"(
            QLabel {
                color: #9ca3af;
                font-size: 14px;
                margin: 5px 0px 0px 0px;
                padding: 0px;
            }
        )");
        subtitleLabel->setAlignment(Qt::AlignCenter);
        headerLayout->addWidget(subtitleLabel);
        
        mainLayout->addWidget(headerWidget);

        // Content area with scroll
        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setStyleSheet(R"(
            QScrollArea {
                border: none;
                background: #13171f;
            }
            QScrollBar:vertical {
                background: #374151;
                width: 12px;
                border-radius: 6px;
            }
            QScrollBar::handle:vertical {
                background: #6b7280;
                border-radius: 6px;
                min-height: 20px;
            }
            QScrollBar::handle:vertical:hover {
                background: #9ca3af;
            }
        )");

        QWidget* contentWidget = new QWidget();
        contentWidget->setStyleSheet("background: #13171f;");
        QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(25, 20, 25, 20);
        contentLayout->setSpacing(20);

        // Source Management Section
        QGroupBox* sourceGroupBox = new QGroupBox("🎭 Source Management");
        sourceGroupBox->setStyleSheet(R"(
            QGroupBox {
                color: white;
                font-size: 16px;
                font-weight: bold;
                border: 2px solid #4b5563;
                border-radius: 10px;
                margin-top: 12px;
                padding-top: 15px;
                background: #1f2937;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 15px;
                padding: 0 8px 0 8px;
                color: #60a5fa;
            }
        )");
        
        QVBoxLayout* sourceGroupLayout = new QVBoxLayout(sourceGroupBox);
        sourceGroupLayout->setContentsMargins(15, 20, 15, 15);
        sourceGroupLayout->setSpacing(15);
        
        // Description
        QLabel* sourceDesc = new QLabel("Tools for managing and controlling your OBS sources");
        sourceDesc->setStyleSheet(R"(
            QLabel {
                color: #9ca3af;
                font-size: 12px;
                font-weight: normal;
                margin: 0px;
                padding: 0px;
                background: transparent;
                border: none;
            }
        )");
        sourceDesc->setWordWrap(true);
        sourceDesc->setTextFormat(Qt::PlainText);
        sourceGroupLayout->addWidget(sourceDesc);
        
        // Lock Current Scene Sources button
        QPushButton* lockCurrentBtn = new QPushButton("Lock Current Scene Sources");
        lockCurrentBtn->setMinimumHeight(60);
        lockCurrentBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);
                color: white;
                border: none;
                padding: 15px 20px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 8px;
                text-align: center;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);
            }
        )");
        QObject::connect(lockCurrentBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "LockAllCurrentSources", 
                "LockAllCurrentSourcesInfo1", "LockAllCurrentSourcesInfo2", "LockAllCurrentSourcesInfo3",
                []() { StreamUP::SourceManager::ToggleLockSourcesInCurrentScene(); },
                "LockAllCurrentSourcesHowTo1", "LockAllCurrentSourcesHowTo2", 
                "LockAllCurrentSourcesHowTo3", "LockAllCurrentSourcesHowTo4");
        });
        
        // Lock All Sources button
        QPushButton* lockAllBtn = new QPushButton("Lock All Sources");
        lockAllBtn->setMinimumHeight(60);
        lockAllBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);
                color: white;
                border: none;
                padding: 15px 20px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 8px;
                text-align: center;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);
            }
        )");
        QObject::connect(lockAllBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "LockAllSources", 
                "LockAllSourcesInfo1", "LockAllSourcesInfo2", "LockAllSourcesInfo3",
                []() { StreamUP::SourceManager::ToggleLockAllSources(); },
                "LockAllSourcesHowTo1", "LockAllSourcesHowTo2", 
                "LockAllSourcesHowTo3", "LockAllSourcesHowTo4");
        });
        
        sourceGroupLayout->addWidget(lockCurrentBtn);
        sourceGroupLayout->addWidget(lockAllBtn);
        
        contentLayout->addWidget(sourceGroupBox);

        // Audio & Video Section
        QGroupBox* avGroupBox = new QGroupBox("🔊 Audio & Video");
        avGroupBox->setStyleSheet(R"(
            QGroupBox {
                color: white;
                font-size: 16px;
                font-weight: bold;
                border: 2px solid #4b5563;
                border-radius: 10px;
                margin-top: 12px;
                padding-top: 15px;
                background: #1f2937;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 15px;
                padding: 0 8px 0 8px;
                color: #60a5fa;
            }
        )");
        
        QVBoxLayout* avGroupLayout = new QVBoxLayout(avGroupBox);
        avGroupLayout->setContentsMargins(15, 20, 15, 15);
        avGroupLayout->setSpacing(15);
        
        // Description
        QLabel* avDesc = new QLabel("Manage audio monitoring and video capture devices");
        avDesc->setStyleSheet(R"(
            QLabel {
                color: #9ca3af;
                font-size: 12px;
                font-weight: normal;
                margin: 0px;
                padding: 0px;
                background: transparent;
                border: none;
            }
        )");
        avDesc->setWordWrap(true);
        avDesc->setTextFormat(Qt::PlainText);
        avGroupLayout->addWidget(avDesc);
        
        // Refresh Audio Monitoring button
        QPushButton* audioBtn = new QPushButton("Refresh Audio Monitoring");
        audioBtn->setMinimumHeight(60);
        audioBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22c55e, stop:1 #16a34a);
                color: white;
                border: none;
                padding: 15px 20px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 8px;
                text-align: center;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #32d56e, stop:1 #26b35a);
            }
        )");
        QObject::connect(audioBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "RefreshAudioMonitoring", 
                "RefreshAudioMonitoringInfo1", "RefreshAudioMonitoringInfo2", "RefreshAudioMonitoringInfo3",
                []() { obs_enum_sources(StreamUP::SourceManager::RefreshAudioMonitoring, nullptr); },
                "RefreshAudioMonitoringHowTo1", "RefreshAudioMonitoringHowTo2", 
                "RefreshAudioMonitoringHowTo3", "RefreshAudioMonitoringHowTo4");
        });
        
        // Refresh Browser Sources button
        QPushButton* browserBtn = new QPushButton("Refresh Browser Sources");
        browserBtn->setMinimumHeight(60);
        browserBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #dc2626);
                color: white;
                border: none;
                padding: 15px 20px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 8px;
                text-align: center;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f87171, stop:1 #ef4444);
            }
        )");
        QObject::connect(browserBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowToolDetailInline(scrollArea, contentWidget, "RefreshBrowserSources", 
                "RefreshBrowserSourcesInfo1", "RefreshBrowserSourcesInfo2", "RefreshBrowserSourcesInfo3",
                []() { obs_enum_sources(StreamUP::SourceManager::RefreshBrowserSources, nullptr); },
                "RefreshBrowserSourcesHowTo1", "RefreshBrowserSourcesHowTo2", 
                "RefreshBrowserSourcesHowTo3", "RefreshBrowserSourcesHowTo4");
        });
        
        // Manage Video Capture Devices button
        QPushButton* videoBtn = new QPushButton("Manage Video Capture Devices");
        videoBtn->setMinimumHeight(60);
        videoBtn->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);
                color: white;
                border: none;
                padding: 15px 20px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 8px;
                text-align: center;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);
            }
        )");
        QObject::connect(videoBtn, &QPushButton::clicked, [scrollArea, contentWidget]() { 
            ShowVideoDeviceOptionsInline(scrollArea, contentWidget);
        });
        
        avGroupLayout->addWidget(audioBtn);
        avGroupLayout->addWidget(browserBtn);
        avGroupLayout->addWidget(videoBtn);
        
        contentLayout->addWidget(avGroupBox);

        // Add stretch to push content to top
        contentLayout->addStretch();

        scrollArea->setWidget(contentWidget);
        mainLayout->addWidget(scrollArea);

        // Bottom button area
        QWidget* buttonWidget = new QWidget();
        buttonWidget->setStyleSheet("background: #13171f; padding: 15px;");
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonWidget);
        buttonLayout->setContentsMargins(15, 0, 15, 0);

        QPushButton* closeButton = new QPushButton("Close");
        closeButton->setStyleSheet(R"(
            QPushButton {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);
                color: white;
                border: none;
                padding: 12px 24px;
                font-size: 14px;
                font-weight: bold;
                border-radius: 6px;
                min-width: 100px;
            }
            QPushButton:hover {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);
            }
        )");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addWidget(buttonWidget);

        dialog->setLayout(mainLayout);
        dialog->show();
    });
}

QPushButton* CreateToolButton(const QString& title, const QString& description, std::function<void()> action)
{
    QPushButton* button = new QPushButton();
    button->setMinimumHeight(80);
    button->setMaximumHeight(80);
    
    // Set the button layout
    QVBoxLayout* buttonLayout = new QVBoxLayout(button);
    buttonLayout->setContentsMargins(15, 12, 15, 12);
    buttonLayout->setSpacing(5);
    
    QLabel* titleLabel = new QLabel(title);
    titleLabel->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 13px;
            font-weight: bold;
            margin: 0px;
            padding: 0px;
            background: transparent;
        }
    )");
    titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    
    QLabel* descLabel = new QLabel(description);
    descLabel->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-size: 11px;
            margin: 0px;
            padding: 0px;
            line-height: 1.3;
            background: transparent;
        }
    )");
    descLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    descLabel->setWordWrap(true);
    
    buttonLayout->addWidget(titleLabel);
    buttonLayout->addWidget(descLabel);
    buttonLayout->addStretch();
    
    button->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #374151, stop:1 #1f2937);
            border: 1px solid #4b5563;
            border-radius: 8px;
            text-align: left;
            padding: 0px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4b5563, stop:1 #374151);
            border: 1px solid #6b7280;
        }
        QPushButton:pressed {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #1f2937, stop:1 #111827);
        }
    )");
    
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
    optionsDialog->setStyleSheet("QDialog { background: #13171f; }");
    optionsDialog->setFixedSize(500, 300);
    optionsDialog->setModal(true);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(optionsDialog);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);
    
    // Title
    QLabel* titleLabel = new QLabel("🎥 Video Capture Device Management");
    titleLabel->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 18px;
            font-weight: bold;
            margin: 0px 0px 10px 0px;
            padding: 0px;
        }
    )");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Description
    QLabel* descLabel = new QLabel("Choose an action to perform on all video capture devices in your scenes:");
    descLabel->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-size: 13px;
            margin: 0px 0px 20px 0px;
            padding: 0px;
        }
    )");
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(descLabel);
    
    // Action buttons
    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(10);
    
    // Activate button
    QPushButton* activateBtn = new QPushButton("Activate All Video Devices");
    activateBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22c55e, stop:1 #16a34a);
            color: white;
            border: none;
            padding: 12px 20px;
            font-size: 14px;
            font-weight: bold;
            border-radius: 6px;
            text-align: left;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #32d56e, stop:1 #26b35a);
        }
    )");
    QObject::connect(activateBtn, &QPushButton::clicked, [optionsDialog]() {
        StreamUP::SourceManager::ActivateAllVideoCaptureDevicesDialog();
        optionsDialog->close();
    });
    
    // Deactivate button
    QPushButton* deactivateBtn = new QPushButton("Deactivate All Video Devices");
    deactivateBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #dc2626);
            color: white;
            border: none;
            padding: 12px 20px;
            font-size: 14px;
            font-weight: bold;
            border-radius: 6px;
            text-align: left;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f87171, stop:1 #ef4444);
        }
    )");
    QObject::connect(deactivateBtn, &QPushButton::clicked, [optionsDialog]() {
        StreamUP::SourceManager::DeactivateAllVideoCaptureDevicesDialog();
        optionsDialog->close();
    });
    
    // Refresh button
    QPushButton* refreshBtn = new QPushButton("Refresh All Video Devices");
    refreshBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);
            color: white;
            border: none;
            padding: 12px 20px;
            font-size: 14px;
            font-weight: bold;
            border-radius: 6px;
            text-align: left;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);
        }
    )");
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
    QPushButton* closeBtn = new QPushButton("Cancel");
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);
            color: white;
            border: none;
            padding: 8px 16px;
            font-size: 12px;
            font-weight: bold;
            border-radius: 4px;
            min-width: 80px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);
        }
    )");
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
    optionsWidget->setStyleSheet("background: #13171f;");
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsWidget);
    optionsLayout->setContentsMargins(25, 20, 25, 20);
    optionsLayout->setSpacing(20);
    
    // Back button and title section
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QPushButton* backButton = new QPushButton("← Back to Tools");
    backButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);
            color: white;
            border: none;
            padding: 8px 16px;
            font-size: 12px;
            font-weight: bold;
            border-radius: 4px;
            text-align: left;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);
        }
    )");
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
    QLabel* titleLabel = new QLabel("🎥 Video Capture Device Management");
    titleLabel->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 24px;
            font-weight: bold;
            margin: 20px 0px 10px 0px;
            padding: 0px;
        }
    )");
    titleLabel->setAlignment(Qt::AlignCenter);
    optionsLayout->addWidget(titleLabel);
    
    // Description
    QLabel* descLabel = new QLabel("Choose an action to perform on all video capture devices in your scenes:");
    descLabel->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-size: 14px;
            margin: 0px 0px 30px 0px;
            padding: 0px;
        }
    )");
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    optionsLayout->addWidget(descLabel);
    
    // Options group
    QGroupBox* optionsGroup = new QGroupBox("Device Actions");
    optionsGroup->setStyleSheet(R"(
        QGroupBox {
            color: white;
            font-size: 16px;
            font-weight: bold;
            border: 2px solid #4b5563;
            border-radius: 10px;
            margin-top: 12px;
            padding-top: 15px;
            background: #1f2937;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 8px 0 8px;
            color: #60a5fa;
        }
    )");
    
    QVBoxLayout* optionsGroupLayout = new QVBoxLayout(optionsGroup);
    optionsGroupLayout->setContentsMargins(15, 20, 15, 15);
    optionsGroupLayout->setSpacing(15);
    
    // Action buttons with larger sizing
    QPushButton* activateBtn = new QPushButton("Activate All Video Devices");
    activateBtn->setMinimumHeight(60);
    activateBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22c55e, stop:1 #16a34a);
            color: white;
            border: none;
            padding: 15px 20px;
            font-size: 14px;
            font-weight: bold;
            border-radius: 8px;
            text-align: center;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #32d56e, stop:1 #26b35a);
        }
    )");
    QObject::connect(activateBtn, &QPushButton::clicked, [scrollArea, originalContent, optionsWidget]() {
        StreamUP::SourceManager::ActivateAllVideoCaptureDevicesDialog();
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        optionsWidget->deleteLater(); // Schedule for deletion
    });
    
    QPushButton* deactivateBtn = new QPushButton("Deactivate All Video Devices");
    deactivateBtn->setMinimumHeight(60);
    deactivateBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #ef4444, stop:1 #dc2626);
            color: white;
            border: none;
            padding: 15px 20px;
            font-size: 14px;
            font-weight: bold;
            border-radius: 8px;
            text-align: center;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f87171, stop:1 #ef4444);
        }
    )");
    QObject::connect(deactivateBtn, &QPushButton::clicked, [scrollArea, originalContent, optionsWidget]() {
        StreamUP::SourceManager::DeactivateAllVideoCaptureDevicesDialog();
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        optionsWidget->deleteLater(); // Schedule for deletion
    });
    
    QPushButton* refreshBtn = new QPushButton("Refresh All Video Devices");
    refreshBtn->setMinimumHeight(60);
    refreshBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);
            color: white;
            border: none;
            padding: 15px 20px;
            font-size: 14px;
            font-weight: bold;
            border-radius: 8px;
            text-align: center;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);
        }
    )");
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
                         const char* howTo3Key, const char* howTo4Key)
{
    // Store the current widget temporarily
    QWidget* currentWidget = scrollArea->takeWidget();
    
    // Create replacement content widget
    QWidget* detailWidget = new QWidget();
    detailWidget->setStyleSheet("background: #13171f;");
    QVBoxLayout* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(25, 20, 25, 20);
    detailLayout->setSpacing(20);
    
    // Back button and title section
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QPushButton* backButton = new QPushButton("← Back to Tools");
    backButton->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #64748b, stop:1 #475569);
            color: white;
            border: none;
            padding: 8px 16px;
            font-size: 12px;
            font-weight: bold;
            border-radius: 4px;
            text-align: left;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #74859b, stop:1 #576579);
        }
    )");
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
    QLabel* titleLabel = new QLabel("🛠️ " + titleStr);
    titleLabel->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 24px;
            font-weight: bold;
            margin: 20px 0px 10px 0px;
            padding: 0px;
        }
    )");
    titleLabel->setAlignment(Qt::AlignCenter);
    detailLayout->addWidget(titleLabel);
    
    // Main info section
    QGroupBox* infoGroup = new QGroupBox("Tool Information");
    infoGroup->setStyleSheet(R"(
        QGroupBox {
            color: white;
            font-size: 16px;
            font-weight: bold;
            border: 2px solid #4b5563;
            border-radius: 10px;
            margin-top: 12px;
            padding-top: 15px;
            background: #1f2937;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 8px 0 8px;
            color: #60a5fa;
        }
    )");
    
    QVBoxLayout* infoGroupLayout = new QVBoxLayout(infoGroup);
    infoGroupLayout->setContentsMargins(15, 20, 15, 15);
    infoGroupLayout->setSpacing(8);
    
    // Info text 1
    QString info1Str = obs_module_text(info1Key);
    QLabel* info1Label = new QLabel(info1Str);
    info1Label->setStyleSheet(R"(
        QLabel {
            color: white;
            font-size: 14px;
            font-weight: bold;
            margin: 0px;
            padding: 0px;
            background: transparent;
            border: none;
        }
    )");
    info1Label->setWordWrap(true);
    info1Label->setTextFormat(Qt::PlainText);
    infoGroupLayout->addWidget(info1Label);
    
    // Info text 2
    QString info2Str = obs_module_text(info2Key);
    QLabel* info2Label = new QLabel(info2Str);
    info2Label->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-size: 13px;
            margin: 0px;
            padding: 0px;
            background: transparent;
            border: none;
        }
    )");
    info2Label->setWordWrap(true);
    info2Label->setTextFormat(Qt::PlainText);
    infoGroupLayout->addWidget(info2Label);
    
    // Info text 3
    QString info3Str = obs_module_text(info3Key);
    QLabel* info3Label = new QLabel(info3Str);
    info3Label->setStyleSheet(R"(
        QLabel {
            color: #9ca3af;
            font-size: 13px;
            margin: 0px;
            padding: 0px;
            background: transparent;
            border: none;
        }
    )");
    info3Label->setWordWrap(true);
    info3Label->setTextFormat(Qt::PlainText);
    infoGroupLayout->addWidget(info3Label);
    
    detailLayout->addWidget(infoGroup);
    
    // How to use section
    QGroupBox* howToGroup = new QGroupBox("How To Use");
    howToGroup->setStyleSheet(R"(
        QGroupBox {
            color: white;
            font-size: 16px;
            font-weight: bold;
            border: 2px solid #4b5563;
            border-radius: 10px;
            margin-top: 12px;
            padding-top: 15px;
            background: #1f2937;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 8px 0 8px;
            color: #60a5fa;
        }
    )");
    
    QVBoxLayout* howToGroupLayout = new QVBoxLayout(howToGroup);
    howToGroupLayout->setContentsMargins(15, 20, 15, 15);
    howToGroupLayout->setSpacing(5);
    
    // How-to steps
    QString howTo1Str = obs_module_text(howTo1Key);
    QString howTo2Str = obs_module_text(howTo2Key);
    QString howTo3Str = obs_module_text(howTo3Key);
    QString howTo4Str = obs_module_text(howTo4Key);
    
    QLabel* howTo1Label = new QLabel("1. " + howTo1Str);
    howTo1Label->setStyleSheet("color: #9ca3af; font-size: 12px; margin: 0px; padding: 0px; background: transparent; border: none;");
    howTo1Label->setWordWrap(true);
    howTo1Label->setTextFormat(Qt::PlainText);
    howToGroupLayout->addWidget(howTo1Label);
    
    QLabel* howTo2Label = new QLabel("2. " + howTo2Str);
    howTo2Label->setStyleSheet("color: #9ca3af; font-size: 12px; margin: 0px; padding: 0px; background: transparent; border: none;");
    howTo2Label->setWordWrap(true);
    howTo2Label->setTextFormat(Qt::PlainText);
    howToGroupLayout->addWidget(howTo2Label);
    
    QLabel* howTo3Label = new QLabel("3. " + howTo3Str);
    howTo3Label->setStyleSheet("color: #9ca3af; font-size: 12px; margin: 0px; padding: 0px; background: transparent; border: none;");
    howTo3Label->setWordWrap(true);
    howTo3Label->setTextFormat(Qt::PlainText);
    howToGroupLayout->addWidget(howTo3Label);
    
    QLabel* howTo4Label = new QLabel("4. " + howTo4Str);
    howTo4Label->setStyleSheet("color: #9ca3af; font-size: 12px; margin: 0px; padding: 0px; background: transparent; border: none;");
    howTo4Label->setWordWrap(true);
    howTo4Label->setTextFormat(Qt::PlainText);
    howToGroupLayout->addWidget(howTo4Label);
    
    detailLayout->addWidget(howToGroup);
    
    // Action button
    QPushButton* executeBtn = new QPushButton("Execute " + titleStr);
    executeBtn->setMinimumHeight(60);
    executeBtn->setStyleSheet(R"(
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #22c55e, stop:1 #16a34a);
            color: white;
            border: none;
            padding: 0px;
            font-size: 16px;
            font-weight: bold;
            border-radius: 8px;
            text-align: center;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #32d56e, stop:1 #26b35a);
        }
    )");
    QObject::connect(executeBtn, &QPushButton::clicked, [scrollArea, originalContent, detailWidget, action, titleStr]() {
        action();
        StreamUP::NotificationManager::SendInfoNotification(titleStr, "Tool executed successfully");
        scrollArea->takeWidget(); // Remove current widget
        scrollArea->setWidget(originalContent);
        detailWidget->deleteLater(); // Schedule for deletion
    });
    
    detailLayout->addWidget(executeBtn);
    detailLayout->addStretch();
    
    // Replace the content in the scroll area
    scrollArea->setWidget(detailWidget);
}

} // namespace ToolsWindow
} // namespace StreamUP