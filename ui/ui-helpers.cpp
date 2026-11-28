#include "ui-helpers.hpp"
#include <streamup/ui/kit.hpp>
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include "version.h"
#include "../core/streamup-common.hpp"
#include <streamup/debug-logger.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QApplication>
#include <QClipboard>
#include <QMetaObject>
#include <QPushButton>
#include <QAbstractButton>
#include <QDesktopServices>
#include <QUrl>
#include <QGridLayout>
#include <QObject>
#include <QScreen>
#include <QGuiApplication>
#include <algorithm>
#include <QTimer>
#include <QIcon>
#include <QStyle>
#include <QPalette>
#include <QColor>
#include <QVariant>

using namespace StreamUP::UIStyles;
namespace su = StreamUP::UIStyles;


// Forward declarations for functions from main streamup.cpp
// Notification functionality moved to StreamUP::NotificationManager module

namespace StreamUP {
namespace UIHelpers {

//-------------------DIALOG MANAGEMENT-------------------
// Static member definitions
std::mutex DialogManager::s_dialogsMutex;
std::unordered_map<std::string, QPointer<QDialog>> DialogManager::s_dialogs;

bool DialogManager::ShowSingletonDialog(const std::string& dialogId,
                                       const std::function<QDialog*()>& createFunction,
                                       bool bringToFront)
{
    std::lock_guard<std::mutex> lock(s_dialogsMutex);
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

bool DialogManager::IsSingletonDialogOpen(const std::string& dialogId)
{
    std::lock_guard<std::mutex> lock(s_dialogsMutex);
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

		// Branded custom window (secondary popup → brandFooter=false). Replaces the
		// legacy CreateStyledDialog factory + group-box message block.
		QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
		su::WindowShell shell = su::makeWindow(titleStr, "v" PROJECT_VERSION, parent,
						       /*brandFooter=*/false, "StreamUP");
		shell.content->setContentsMargins(S(20), S(16), S(20), S(16));
		shell.content->setSpacing(S(12));

		// First info text (main message) — emphasised
		QLabel *info1 = CreateRichTextLabel(infoText1Str, false, true, Qt::AlignCenter);
		info1->setStyleSheet(scale_qss(QString(
			"QLabel{color:%1;font-size:%2px;font-weight: 600;background:transparent;border:none;}")
			.arg(Colors::TEXT_SECONDARY)
			.arg(Sizes::FONT_SIZE_NORMAL)));
		shell.content->addWidget(info1);

		// Second info text
		QLabel *info2 = CreateRichTextLabel(infoText2Str, false, true, Qt::AlignCenter);
		info2->setStyleSheet(scale_qss(QString(
			"QLabel{color:%1;font-size:%2px; font-weight: 400;background:transparent;border:none;}")
			.arg(Colors::TEXT_MUTED)
			.arg(Sizes::FONT_SIZE_SMALL)));
		shell.content->addWidget(info2);

		// Third info text
		QLabel *info3 = CreateRichTextLabel(infoText3Str, false, true, Qt::AlignCenter);
		info3->setStyleSheet(scale_qss(QString(
			"QLabel{color:%1;font-size:%2px; font-weight: 400;background:transparent;border:none;}")
			.arg(Colors::TEXT_MUTED)
			.arg(Sizes::FONT_SIZE_SMALL)));
		shell.content->addWidget(info3);

		// OK button — right-anchored action slot in the footer.
		auto *okButton = new su::PillButton(obs_module_text("OK"), "primary");
		QObject::connect(okButton, &QPushButton::clicked, shell.dialog, &QDialog::close);
		shell.footerButtons->addWidget(okButton);

		shell.dialog->resize(S(450), S(360));
		shell.dialog->show();
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
		).arg(Colors::BG_SECONDARY)
		 .arg(S(10))
		 .arg(S(12))
		 .arg(Colors::BORDER_SUBTLE);
	} else {
		styleSheet = "QLabel {";
	}
	
	if (bold) {
		styleSheet += "font-weight: 600; font-size: 14px;";
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
#ifdef __APPLE__
	int pixmapSize = 16;
#else
	int pixmapSize = 64;
#endif
	icon->setPixmap(QApplication::style()->standardIcon(iconName).pixmap(StreamUP::UIStyles::S(pixmapSize), StreamUP::UIStyles::S(pixmapSize)));
	icon->setStyleSheet(StreamUP::UIStyles::scale_qss("padding-top: 3px;"));
	return icon;
}

//-------------------LAYOUT CREATION FUNCTIONS-------------------
QHBoxLayout *AddIconAndText(const QStyle::StandardPixmap &iconText, const char *labelText)
{
	QLabel *icon = CreateIconLabel(iconText);
	QLabel *text = CreateRichTextLabel(obs_module_text(labelText), false, true, Qt::AlignTop);

	QHBoxLayout *iconTextLayout = new QHBoxLayout();
	iconTextLayout->addWidget(icon, 0, Qt::AlignTop);
	iconTextLayout->addSpacing(StreamUP::UIStyles::S(10));
	text->setWordWrap(true);
	iconTextLayout->addWidget(text, 1);

	return iconTextLayout;
}

//-------------------UTILITY FUNCTIONS-------------------
void CopyToClipboard(const QString &text)
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(text);
}

QString GetThemedIconPath(const QString &iconName)
{
	// One resolver for every StreamUP icon: the shared kit's Tabler set first,
	// then this plugin's own :images/icons/ui files.
	return StreamUP::Kit::IconPath(iconName);
}

bool IsOBSThemeDark()
{
	return obs_frontend_is_theme_dark();
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

void PlaceOverMainWindow(QWidget *window, bool onlyWhenElsewhere)
{
	QWidget *main = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (!window || !main)
		return;
	QScreen *mainScreen = main->screen() ? main->screen() : QGuiApplication::primaryScreen();
	if (!mainScreen)
		return;
	const QRect area = mainScreen->availableGeometry();
	const QRect frame = window->frameGeometry();
	if (onlyWhenElsewhere) {
		QScreen *at = QGuiApplication::screenAt(frame.center());
		if (at == mainScreen && area.contains(frame.center()))
			return;
	}
	// Centre over OBS itself, or over its screen when OBS is minimised.
	const QRect over = main->isMinimized() ? area : main->frameGeometry().intersected(area);
	QPoint pos = (over.isValid() ? over : area).center() - QPoint(frame.width() / 2, frame.height() / 2);
	// Keep it on that screen, title bar first when it is too tall to fit.
	pos.setX(std::clamp(pos.x(), area.left(), std::max(area.left(), area.right() - frame.width())));
	pos.setY(std::clamp(pos.y(), area.top(), std::max(area.top(), area.bottom() - frame.height())));
	window->move(pos);
}

} // namespace UIHelpers
} // namespace StreamUP
