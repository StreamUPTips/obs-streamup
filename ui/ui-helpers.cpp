#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "../core/streamup-common.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QApplication>
#include <QClipboard>
#include <QMetaObject>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include <QGridLayout>
#include <QObject>
#include <QScreen>
#include <QTimer>
#include <QIcon>


// Forward declarations for functions from main streamup.cpp
// Notification functionality moved to StreamUP::NotificationManager module

namespace StreamUP {
namespace UIHelpers {

//-------------------DIALOG MANAGEMENT-------------------
// Static member definition
std::unordered_map<std::string, QPointer<QDialog>> DialogManager::s_dialogs;

bool DialogManager::ShowSingletonDialog(const std::string& dialogId, 
                                       const std::function<QDialog*()>& createFunction,
                                       bool bringToFront)
{
    auto it = s_dialogs.find(dialogId);
    
    // Check if dialog already exists and is visible
    if (it != s_dialogs.end() && !it->second.isNull() && it->second->isVisible()) {
        if (bringToFront) {
            it->second->raise();
            it->second->activateWindow();
        }
        return false; // Existing dialog found
    }
    
    // Clean up any null pointers
    if (it != s_dialogs.end() && it->second.isNull()) {
        s_dialogs.erase(it);
    }
    
    // Create new dialog
    QDialog* dialog = createFunction();
    if (dialog) {
        s_dialogs[dialogId] = dialog;
        return true; // New dialog created
    }
    
    return false;
}

void DialogManager::CloseSingletonDialog(const std::string& dialogId)
{
    auto it = s_dialogs.find(dialogId);
    if (it != s_dialogs.end() && !it->second.isNull()) {
        it->second->close();
        s_dialogs.erase(it);
    }
}

bool DialogManager::IsSingletonDialogOpen(const std::string& dialogId)
{
    auto it = s_dialogs.find(dialogId);
    return it != s_dialogs.end() && !it->second.isNull() && it->second->isVisible();
}

//-------------------DIALOG CREATION FUNCTIONS-------------------
void ShowDialogOnUIThread(const std::function<void()> &dialogFunction)
{
	QMetaObject::invokeMethod(qApp, dialogFunction, Qt::QueuedConnection);
}

void ShowSingletonDialogOnUIThread(const std::string& dialogId,
                                  const std::function<QDialog*()>& createFunction,
                                  bool bringToFront)
{
    // First check if dialog already exists (can be done safely on any thread)
    if (DialogManager::IsSingletonDialogOpen(dialogId)) {
        if (bringToFront) {
            ShowDialogOnUIThread([dialogId]() {
                // Use public method to bring dialog to front
                DialogManager::ShowSingletonDialog(dialogId, []() -> QDialog* { return nullptr; }, true);
            });
        }
        return;
    }
    
    // Create dialog on UI thread
    ShowDialogOnUIThread([dialogId, createFunction, bringToFront]() {
        DialogManager::ShowSingletonDialog(dialogId, createFunction, bringToFront);
    });
}

QDialog *CreateDialogWindow(const char *windowTitle, QWidget *parentWidget)
{
	// Use provided parent or fallback to OBS main window
	QWidget *parent = parentWidget ? parentWidget : static_cast<QWidget *>(obs_frontend_get_main_window());
	QDialog *dialog = new QDialog(parent);
	dialog->setWindowTitle(obs_module_text(windowTitle));
	dialog->setWindowFlags(Qt::Dialog); // Use Dialog instead of Window for better positioning
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	
	// Note: Positioning is deferred to CenterDialog() after dialog is properly sized
	
	return dialog;
}

void CenterDialog(QDialog *dialog, QWidget *parentWidget)
{
	if (!dialog) return;
	
	// Use provided parent, dialog's parent, or OBS main window as fallback
	QWidget *parent = parentWidget;
	if (!parent) {
		parent = dialog->parentWidget();
	}
	if (!parent) {
		parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	}
	
	if (parent && parent->isVisible()) {
		// Center on parent widget
		QRect parentGeometry = parent->geometry();
		QRect dialogGeometry = dialog->geometry();
		
		int x = parentGeometry.x() + (parentGeometry.width() - dialogGeometry.width()) / 2;
		int y = parentGeometry.y() + (parentGeometry.height() - dialogGeometry.height()) / 2;
		
		dialog->move(x, y);
	} else {
		// Center on screen if no valid parent
		QScreen *screen = QApplication::primaryScreen();
		if (screen) {
			QRect screenGeometry = screen->geometry();
			QRect dialogGeometry = dialog->geometry();
			
			int x = screenGeometry.x() + (screenGeometry.width() - dialogGeometry.width()) / 2;
			int y = screenGeometry.y() + (screenGeometry.height() - dialogGeometry.height()) / 2;
			
			dialog->move(x, y);
		}
	}
}

void ShowDialogCentered(QDialog *dialog, QWidget *parentWidget)
{
	if (!dialog) return;
	
	// Show the dialog first
	dialog->show();
	
	// Use a small delay to ensure the dialog is properly sized before centering
	QTimer::singleShot(10, [dialog, parentWidget]() {
		CenterDialog(dialog, parentWidget);
	});
}

void CreateToolDialog(const char *infoText1, const char *infoText2, const char *infoText3, const QString &titleText,
		      const QStyle::StandardPixmap &iconType)
{
	(void)iconType; // Suppress unused parameter warning
	ShowDialogOnUIThread([infoText1, infoText2, infoText3, titleText]() {
		const char *titleTextChar = titleText.toUtf8().constData();
		QString titleStr = obs_module_text(titleTextChar);
		QString infoText1Str = obs_module_text(infoText1);
		QString infoText2Str = obs_module_text(infoText2);
		QString infoText3Str = obs_module_text(infoText3);

		QDialog *dialog = StreamUP::UIStyles::CreateStyledDialog(titleStr);
		
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_XL, 
			StreamUP::UIStyles::Sizes::PADDING_LARGE, 
			StreamUP::UIStyles::Sizes::PADDING_XL, 
			StreamUP::UIStyles::Sizes::PADDING_LARGE);
		dialogLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_LARGE);

		// Add styled title
		QLabel *titleLabel = StreamUP::UIStyles::CreateStyledTitle(titleStr);
		dialogLayout->addWidget(titleLabel);

		// Create styled message group
		QGroupBox *messageGroup = StreamUP::UIStyles::CreateStyledGroupBox("", "info");
		QVBoxLayout *messageLayout = new QVBoxLayout(messageGroup);
		messageLayout->setContentsMargins(StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
			StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
			StreamUP::UIStyles::Sizes::PADDING_MEDIUM, 
			StreamUP::UIStyles::Sizes::PADDING_MEDIUM);
		messageLayout->setSpacing(StreamUP::UIStyles::Sizes::SPACING_MEDIUM);
		
		// First info text (main message)
		QLabel *info1 = CreateRichTextLabel(infoText1Str, false, true, Qt::AlignCenter);
		info1->setStyleSheet(QString(
			"QLabel {"
			"color: %1;"
			"font-size: %2px;"
			"font-weight: bold;"
			"background: transparent;"
			"border: none;"
			"}").arg(StreamUP::UIStyles::Colors::TEXT_SECONDARY)
			   .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_NORMAL));
		messageLayout->addWidget(info1);

		// Second info text
		QLabel *info2 = CreateRichTextLabel(infoText2Str, false, true, Qt::AlignCenter);
		info2->setStyleSheet(QString(
			"QLabel {"
			"color: %1;"
			"font-size: %2px;"
			"background: transparent;"
			"border: none;"
			"}").arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
			   .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
		messageLayout->addWidget(info2);

		// Third info text
		QLabel *info3 = CreateRichTextLabel(infoText3Str, false, true, Qt::AlignCenter);
		info3->setStyleSheet(QString(
			"QLabel {"
			"color: %1;"
			"font-size: %2px;"
			"background: transparent;"
			"border: none;"
			"}").arg(StreamUP::UIStyles::Colors::TEXT_MUTED)
			   .arg(StreamUP::UIStyles::Sizes::FONT_SIZE_SMALL));
		messageLayout->addWidget(info3);
		
		dialogLayout->addWidget(messageGroup);

		// Add styled button
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->addStretch();
		
		QPushButton *okButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("OK"), "info");
		QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		buttonLayout->addWidget(okButton);
		buttonLayout->addStretch();

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		
		// Apply auto-sizing
		StreamUP::UIStyles::ApplyAutoSizing(dialog, 450, 600, 350, 500);
	});
}

//-------------------LABEL CREATION FUNCTIONS-------------------
QLabel *CreateRichTextLabel(const QString &text, bool bold, bool wrap, Qt::Alignment alignment, bool roundedBackground)
{
	QLabel *label = new QLabel;
	label->setText(text);
	label->setTextFormat(Qt::RichText);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	
	// Build style sheet with optional rounded background
	QString styleSheet;
	if (roundedBackground) {
		styleSheet = QString(
			"QLabel {"
			"background-color: %1;"
			"border-radius: %2px;"
			"padding: %3px;"
			"border: 1px solid %4;"
		).arg(StreamUP::UIStyles::Colors::BG_SECONDARY)
		 .arg(StreamUP::UIStyles::Sizes::RADIUS_MD)
		 .arg(StreamUP::UIStyles::Sizes::PADDING_MEDIUM)
		 .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE);
	} else {
		styleSheet = "QLabel {";
	}
	
	if (bold) {
		styleSheet += "font-weight: bold; font-size: 14px;";
	}
	
	styleSheet += "}";
	label->setStyleSheet(styleSheet);
	
	if (wrap) {
		label->setWordWrap(true);
	}
	if (alignment != Qt::Alignment()) {
		label->setAlignment(alignment);
	}
	return label;
}

QLabel *CreateIconLabel(const QStyle::StandardPixmap &iconName)
{
	QLabel *icon = new QLabel();
	int pixmapSize = (strcmp(STREAMUP_PLATFORM_NAME, "macos") == 0) ? 16 : 64;
	icon->setPixmap(QApplication::style()->standardIcon(iconName).pixmap(pixmapSize, pixmapSize));
	icon->setStyleSheet("padding-top: 3px;");
	return icon;
}

//-------------------LAYOUT CREATION FUNCTIONS-------------------
QHBoxLayout *AddIconAndText(const QStyle::StandardPixmap &iconText, const char *labelText)
{
	QLabel *icon = CreateIconLabel(iconText);
	QLabel *text = CreateRichTextLabel(obs_module_text(labelText), false, true, Qt::AlignTop);

	QHBoxLayout *iconTextLayout = new QHBoxLayout();
	iconTextLayout->addWidget(icon, 0, Qt::AlignTop);
	iconTextLayout->addSpacing(10);
	text->setWordWrap(true);
	iconTextLayout->addWidget(text, 1);

	return iconTextLayout;
}

QVBoxLayout *CreateVBoxLayout(QWidget *parent)
{
	QVBoxLayout *layout = new QVBoxLayout(parent);
	layout->setContentsMargins(20, 15, 20, 10);
	return layout;
}

//-------------------CONTROL CREATION FUNCTIONS-------------------
void CreateLabelWithLink(QLayout *layout, const QString &text, const QString &url, int row, int column)
{
	QLabel *label = CreateRichTextLabel(text, false, false, Qt::AlignCenter);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	QObject::connect(label, &QLabel::linkActivated, [url]() { QDesktopServices::openUrl(QUrl(url)); });
	static_cast<QGridLayout *>(layout)->addWidget(label, row, column);
}

void CreateButton(QLayout *layout, const QString &text, const std::function<void()> &onClick)
{
	QPushButton *button = StreamUP::UIStyles::CreateStyledButton(text, "neutral");
	QObject::connect(button, &QPushButton::clicked, onClick);
	layout->addWidget(button);
}

//-------------------UTILITY FUNCTIONS-------------------
void CopyToClipboard(const QString &text)
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(text);
}

QString GetThemedIconPath(const QString &iconName)
{
	// Check if we're using a dark theme (requires OBS 29.0.0+)
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	bool isDarkTheme = obs_frontend_is_theme_dark();
#else
	bool isDarkTheme = false; // Fallback to light theme for older versions
#endif
	
	// Use appropriate suffix based on theme
	QString themeSuffix = isDarkTheme ? "-dark" : "-light";
	QString iconPath = QString(":images/icons/ui/%1%2.svg").arg(iconName).arg(themeSuffix);
	
	return iconPath;
}

QIcon CreateThemedIcon(const QString &baseName)
{
	// Create a QIcon that can handle theme changes automatically
	// This approach is more Qt-standard and handles theme switching better
	QIcon icon;
	
	// Add both light and dark variants to the icon
	// Qt and OBS can then automatically choose the appropriate one
	QString lightIconPath = QString(":images/icons/ui/%1-light.svg").arg(baseName);
	QString darkIconPath = QString(":images/icons/ui/%1-dark.svg").arg(baseName);
	
	// Add the current theme's icon as the default
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	bool isDarkTheme = obs_frontend_is_theme_dark();
#else
	bool isDarkTheme = false; // Fallback to light theme for older versions
#endif
	QString currentIconPath = isDarkTheme ? darkIconPath : lightIconPath;
	
	icon.addFile(currentIconPath);
	
	return icon;
}

bool IsOBSThemeDark()
{
	// obs_frontend_is_theme_dark was introduced in OBS 29.0.0
	// Check if we're running on a compatible version
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
	return obs_frontend_is_theme_dark();
#else
	// Fallback for older OBS versions - assume light theme
	return false;
#endif
}

QStandardItem* FindItemRecursive(QStandardItem* parent, const QString& text, int itemType)
{
	if (!parent) {
		return nullptr;
	}

	// Check current item
	if (parent->text() == text && parent->type() == itemType) {
		return parent;
	}

	// Search children recursively
	for (int i = 0; i < parent->rowCount(); ++i) {
		QStandardItem* child = parent->child(i);
		if (child) {
			QStandardItem* found = FindItemRecursive(child, text, itemType);
			if (found) {
				return found;
			}
		}
	}

	return nullptr;
}

} // namespace UIHelpers
} // namespace StreamUP