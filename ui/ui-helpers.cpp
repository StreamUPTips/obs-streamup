#include "ui-helpers.hpp"
#include <obs-module.h>
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
	QDialog *dialog = new QDialog();
	dialog->setWindowTitle(obs_module_text(windowTitle));
	dialog->setWindowFlags(Qt::Window);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
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

		QDialog *dialog = CreateDialogWindow(titleTextChar);
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(20, 15, 20, 10);

		dialogLayout->addLayout(AddIconAndText(iconType, infoText1));
		dialogLayout->addSpacing(10);

		QLabel *info2 = CreateRichTextLabel(infoText2Str, false, true, Qt::AlignTop);
		dialogLayout->addWidget(info2, 0, Qt::AlignTop);
		dialogLayout->addSpacing(10);

		QLabel *info3 = CreateRichTextLabel(infoText3Str, false, true, Qt::AlignTop);
		dialogLayout->addWidget(info3, 0, Qt::AlignTop);
		dialogLayout->addSpacing(10);

		QHBoxLayout *buttonLayout = new QHBoxLayout();
		CreateButton(buttonLayout, obs_module_text("OK"), [dialog]() { dialog->close(); });

		dialogLayout->addLayout(buttonLayout);
		dialog->setLayout(dialogLayout);
		dialog->show();
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