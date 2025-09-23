#include "patch-notes-window.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp" 
#include "splash-screen.hpp"
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

namespace StreamUP {
namespace PatchNotesWindow {

// Patch notes dialog is now managed by DialogManager in ui-helpers

void CreatePatchNotesDialog()
{
    UIHelpers::ShowSingletonDialogOnUIThread("patch-notes", []() -> QDialog* {
        QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog("StreamUP - Patch Notes");

        // Start with compact size - will expand based on content
        dialog->resize(700, 700);

        QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        // Modern unified content area with scroll - everything inside
        QScrollArea *scrollArea = StreamUP::UIStyles::CreateStyledScrollArea();

        QWidget *contentWidget = new QWidget();
        contentWidget->setStyleSheet(QString("background: %1;").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
        contentLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL,
                          StreamUP::UIStyles::Sizes::PADDING_XL, StreamUP::UIStyles::Sizes::PADDING_XL);
        contentLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);

        // Modern header inside scrollable area
        QWidget *headerSection = new QWidget();
        QVBoxLayout *headerLayout = new QVBoxLayout(headerSection);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);

        // Title with modern styling
        QLabel *titleLabel = StreamUP::UIStyles::CreateStyledTitle("📋 Patch Notes");
        titleLabel->setAlignment(Qt::AlignCenter);

        // Description with modern styling
        QString versionText = QString("Latest updates and improvements in v%1").arg(PROJECT_VERSION);
        QLabel *subtitleLabel = StreamUP::UIStyles::CreateStyledDescription(versionText);
        subtitleLabel->setAlignment(Qt::AlignCenter);

        headerLayout->addWidget(titleLabel);
        headerLayout->addWidget(subtitleLabel);
        
        contentLayout->addWidget(headerSection);
        contentLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

        // Load patch notes content from splash screen module
        std::string patchNotesContent = StreamUP::SplashScreen::LoadLocalPatchNotes();
        QString patchNotesText = QString::fromStdString(patchNotesContent);
        
        if (patchNotesText.isEmpty()) {
            patchNotesText = QString(R"(
<div style="color: %1; line-height: 1.6; font-size: 14px;">
    <h2 style="color: %2; margin: 0 0 15px 0; font-size: 20px; font-weight: 700;">🚀 StreamUP v%3 - Complete Redesign</h2>
    <p style="margin: 8px 0; color: %4; font-size: 13px;">⚠️ Unable to load patch notes from local file</p>
    <div style="margin: 15px 0;">
        <h3 style="color: %5; font-size: 16px; font-weight: 600; margin: 12px 0 8px 0;">✨ Key Features</h3>
        <ul style="margin: 8px 0; padding-left: 20px; line-height: 1.8;">
            <li><strong>Modular Architecture:</strong> Complete restructuring for better performance</li>
            <li><strong>Enhanced WebSocket API:</strong> Modern PascalCase commands</li>
            <li><strong>Improved UI/UX:</strong> Beautiful interfaces with consistent design</li>
            <li><strong>Advanced Settings:</strong> Comprehensive configuration options</li>
            <li><strong>Better Notifications:</strong> Enhanced user feedback system</li>
        </ul>
    </div>
</div>
            )")
                .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
                .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR)
                .arg(PROJECT_VERSION)
                .arg(StreamUP::UIStyles::Colors::WARNING)
                .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        }
        
        // Create main patch notes group box
        QGroupBox* patchNotesGroup = StreamUP::UIStyles::CreateStyledGroupBox("📋 Latest Updates", "success");
        QVBoxLayout* patchNotesLayout = new QVBoxLayout(patchNotesGroup);
        patchNotesLayout->setContentsMargins(8, 8, 8, 8);
        
        // Add patch notes content to group box
        QLabel* patchNotesLabel = UIHelpers::CreateRichTextLabel(patchNotesText, false, true, Qt::Alignment(), true);
        patchNotesLayout->addWidget(patchNotesLabel);
        
        contentLayout->addWidget(patchNotesGroup);
        
        // Add version and build information section
        QGroupBox* versionGroup = StreamUP::UIStyles::CreateStyledGroupBox("ℹ️ Version Information", "warning");
        QVBoxLayout* versionLayout = new QVBoxLayout(versionGroup);
        versionLayout->setContentsMargins(8, 8, 8, 8);
        
        QString versionInfo = QString(R"(
<div style="color: %1; line-height: 1.5; font-size: 13px;">
    <p style="margin: 4px 0;"><strong>Version:</strong> %2</p>
    <p style="margin: 4px 0;"><strong>Build:</strong> Release</p>
    <p style="margin: 4px 0;"><strong>Platform:</strong> Windows x64</p>
    <p style="margin: 4px 0; color: %3;">For the latest updates and community support, check out our links below!</p>
</div>
        )")
            .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
            .arg(PROJECT_VERSION)
            .arg(StreamUP::UIStyles::Colors::TEXT_MUTED);
        
        QLabel* versionLabel = UIHelpers::CreateRichTextLabel(versionInfo, false, true, Qt::Alignment(), true);
        versionLayout->addWidget(versionLabel);
        contentLayout->addWidget(versionGroup);
        
        // Create links section with modern card styling
        QGroupBox* linksGroup = StreamUP::UIStyles::CreateStyledGroupBox("🔗 Useful Links", "info");
        QVBoxLayout* linksGroupLayout = new QVBoxLayout(linksGroup);
        linksGroupLayout->setContentsMargins(8, 8, 8, 8);
        
        // Link buttons with improved layout
        QHBoxLayout* buttonsLayout = new QHBoxLayout();
        buttonsLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_SMALL);
        
        QPushButton* docsBtn = StreamUP::UIStyles::CreateStyledButton("📖 Documentation", "success");
        docsBtn->setToolTip("View comprehensive documentation and guides");
        QObject::connect(docsBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.doras.click/docs"));
        });
        
        QPushButton* discordBtn = StreamUP::UIStyles::CreateStyledButton("💬 Discord", "info");
        discordBtn->setIcon(QIcon(":images/icons/social/discord.svg"));
        discordBtn->setIconSize(QSize(16, 16));
        discordBtn->setToolTip("Join our community Discord server");
        QObject::connect(discordBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://discord.com/invite/RnDKRaVCEu"));
        });
        
        QPushButton* websiteBtn = StreamUP::UIStyles::CreateStyledButton("🌐 Website", "neutral");
        websiteBtn->setToolTip("Visit the official StreamUP website");
        QObject::connect(websiteBtn, &QPushButton::clicked, []() {
            QDesktopServices::openUrl(QUrl("https://streamup.tips"));
        });
        
        buttonsLayout->addWidget(docsBtn);
        buttonsLayout->addWidget(discordBtn);
        buttonsLayout->addWidget(websiteBtn);
        buttonsLayout->addStretch();
        
        linksGroupLayout->addLayout(buttonsLayout);
        contentLayout->addWidget(linksGroup);

        // Add stretch to push content to top
        contentLayout->addStretch();

        // Add close button at the bottom of the content area (inside scroll)
        contentLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_XL);
        
        // Modern button section inside scrollable content
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        buttonLayout->setContentsMargins(0, 0, 0, 0);
        
        QPushButton *closeButton = StreamUP::UIStyles::CreateStyledButton("Close", "neutral");
        QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
        
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeButton);
        buttonLayout->addStretch();
        
        contentLayout->addLayout(buttonLayout);

        // Set up scroll area
        scrollArea->setWidget(contentWidget);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        mainLayout->addWidget(scrollArea);
        dialog->setLayout(mainLayout);

        // Apply consistent sizing similar to WebSocket window
        StreamUP::UIStyles::ApplyAutoSizing(dialog, 700, 900, 700, 800);
        
        // Center the dialog
        StreamUP::UIHelpers::CenterDialog(dialog);
        return dialog;
    });
}

void ShowPatchNotesWindow()
{
    CreatePatchNotesDialog();
    
    ErrorHandler::LogInfo("Patch notes window shown", ErrorHandler::Category::UI);
}

bool IsPatchNotesWindowOpen()
{
    return UIHelpers::DialogManager::IsSingletonDialogOpen("patch-notes");
}

} // namespace PatchNotesWindow
} // namespace StreamUP
