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

// Static pointer to track open patch notes dialog
static QPointer<QDialog> patchNotesDialog;

void CreatePatchNotesDialog()
{
    // Check if patch notes dialog is already open
    if (!patchNotesDialog.isNull() && patchNotesDialog->isVisible()) {
        // Bring existing dialog to front
        patchNotesDialog->raise();
        patchNotesDialog->activateWindow();
        return;
    }

    UIHelpers::ShowDialogOnUIThread([]() {
        // Create standardized dialog using modern template system
        QString versionText = QString("Latest updates and improvements in v%1").arg(PROJECT_VERSION);
        auto components = StreamUP::UIStyles::CreateStandardDialog(
            "StreamUP - Patch Notes",
            "📋 Patch Notes", 
            versionText
        );
        
        components.dialog->setModal(false);
        
        // Load patch notes content from splash screen module
        std::string patchNotesContent = StreamUP::SplashScreen::LoadLocalPatchNotes();
        QString patchNotesText = QString::fromStdString(patchNotesContent);
        
        if (patchNotesText.isEmpty()) {
            patchNotesText = QString(R"(
<div style="color: %1; line-height: 1.6; font-size: 14px; font-family: -apple-system, system-ui;">
    <div style="background: %2; padding: 20px; border-radius: 10px; margin: 10px 0;">
        <h2 style="color: %3; margin: 0 0 15px 0; font-size: 20px; font-weight: 700;">🚀 StreamUP v%4 - Complete Redesign</h2>
        <p style="margin: 8px 0; color: %5; font-size: 13px;">⚠️ Unable to load patch notes from local file</p>
        <div style="margin: 15px 0;">
            <h3 style="color: %6; font-size: 16px; font-weight: 600; margin: 12px 0 8px 0;">✨ Key Features</h3>
            <ul style="margin: 8px 0; padding-left: 20px; line-height: 1.8;">
                <li><strong>Modular Architecture:</strong> Complete restructuring for better performance</li>
                <li><strong>Enhanced WebSocket API:</strong> Modern PascalCase commands</li>
                <li><strong>Improved UI/UX:</strong> Beautiful interfaces with consistent design</li>
                <li><strong>Advanced Settings:</strong> Comprehensive configuration options</li>
                <li><strong>Better Notifications:</strong> Enhanced user feedback system</li>
            </ul>
        </div>
    </div>
</div>
            )")
                .arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
                .arg(StreamUP::UIStyles::Colors::BACKGROUND_CARD)
                .arg(StreamUP::UIStyles::Colors::INFO)
                .arg(PROJECT_VERSION)
                .arg(StreamUP::UIStyles::Colors::WARNING)
                .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY);
        }
        
        // Add patch notes content to scrollable area
        QLabel* patchNotesLabel = UIHelpers::CreateRichTextLabel(patchNotesText, false, true);
        components.contentLayout->addWidget(patchNotesLabel);
        
        // Add some spacing
        components.contentLayout->addSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);
        
        // Create links section with modern card styling
        QGroupBox* linksGroup = StreamUP::UIStyles::CreateStyledGroupBox("🔗 Useful Links", "info");
        QVBoxLayout* linksLayout = new QVBoxLayout();
        linksLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
        
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
        
        linksLayout->addLayout(buttonsLayout);
        linksGroup->setLayout(linksLayout);
        components.contentLayout->addWidget(static_cast<QWidget*>(linksGroup));
        
        // Setup dialog navigation with close button
        QDialog* dialog = components.dialog; // Capture dialog pointer safely
        StreamUP::UIStyles::SetupDialogNavigation(components, [dialog]() {
            dialog->close();
        });
        
        // Update main button text
        components.mainButton->setText("Close");
        components.mainButton->setDefault(true);
        
        // Apply modern sizing
        StreamUP::UIStyles::ApplyConsistentSizing(components.dialog, 720, 1000, 500, 700);
        
        // Store the dialog reference
        patchNotesDialog = components.dialog;
        
        components.dialog->show();
        StreamUP::UIHelpers::CenterDialog(components.dialog);
    });
}

void ShowPatchNotesWindow()
{
    CreatePatchNotesDialog();
    
    ErrorHandler::LogInfo("Patch notes window shown", ErrorHandler::Category::UI);
}

bool IsPatchNotesWindowOpen()
{
    return !patchNotesDialog.isNull() && patchNotesDialog->isVisible();
}

} // namespace PatchNotesWindow
} // namespace StreamUP