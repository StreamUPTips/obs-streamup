#include "ui-helpers.hpp"
#include "ui-styles.hpp"
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
// QSystemTrayIcon now handled by NotificationManager module

#if defined(_WIN32)
#define PLATFORM_NAME "windows"
#elif defined(__APPLE__)
#define PLATFORM_NAME "macos"
#elif defined(__linux__)
#define PLATFORM_NAME "linux"
#else
#define PLATFORM_NAME "unknown"
#endif

// Forward declarations for functions from main streamup.cpp
// Notification functionality moved to StreamUP::NotificationManager module

namespace StreamUP {
namespace UIHelpers {

//-------------------DIALOG CREATION FUNCTIONS-------------------
void ShowDialogOnUIThread(const std::function<void()> &dialogFunction)
{
	QMetaObject::invokeMethod(qApp, dialogFunction, Qt::QueuedConnection);
}

QDialog *CreateDialogWindow(const char *windowTitle)
{
	// Get OBS main window as parent to ensure dialog opens on same monitor
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	QDialog *dialog = new QDialog(parent);
	dialog->setWindowTitle(obs_module_text(windowTitle));
	dialog->setWindowFlags(Qt::Dialog); // Use Dialog instead of Window for better positioning
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	
	// Center the dialog relative to OBS main window
	if (parent) {
		dialog->move(parent->geometry().center() - dialog->rect().center());
	}
	
	return dialog;
}

void CreateToolDialog(const char *infoText1, const char *infoText2, const char *infoText3, const QString &titleText,
		      const QStyle::StandardPixmap &iconType)
{
	ShowDialogOnUIThread([infoText1, infoText2, infoText3, titleText, iconType]() {
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
QLabel *CreateRichTextLabel(const QString &text, bool bold, bool wrap, Qt::Alignment alignment)
{
	QLabel *label = new QLabel;
	label->setText(text);
	label->setTextFormat(Qt::RichText);
	label->setTextInteractionFlags(Qt::TextBrowserInteraction);
	label->setOpenExternalLinks(true);
	if (bold) {
		label->setStyleSheet("font-weight: bold; font-size: 14px;");
	}
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
	int pixmapSize = (strcmp(PLATFORM_NAME, "macos") == 0) ? 16 : 64;
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
	QPushButton *button = new QPushButton(text);
	QObject::connect(button, &QPushButton::clicked, onClick);
	layout->addWidget(button);
}

//-------------------UTILITY FUNCTIONS-------------------
void CopyToClipboard(const QString &text)
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(text);
}

} // namespace UIHelpers
} // namespace StreamUP