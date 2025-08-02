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
		dialog->setStyleSheet("QDialog { background: #13171f; }");
		dialog->resize(500, 400);
		dialog->setMinimumSize(450, 350);
		
		QVBoxLayout *dialogLayout = new QVBoxLayout(dialog);
		dialogLayout->setContentsMargins(25, 20, 25, 20);
		dialogLayout->setSpacing(20);

		// Header section
		QLabel *titleLabel = new QLabel(titleStr);
		titleLabel->setStyleSheet(
			"QLabel {"
			"color: white;"
			"font-size: 20px;"
			"font-weight: bold;"
			"margin: 0px 0px 15px 0px;"
			"padding: 0px;"
			"}");
		titleLabel->setAlignment(Qt::AlignCenter);
		dialogLayout->addWidget(titleLabel);

		// Message section with blue border for info
		QGroupBox *messageGroup = new QGroupBox();
		messageGroup->setStyleSheet(
			"QGroupBox {"
			"border: 2px solid #3b82f6;"
			"border-radius: 10px;"
			"padding-top: 15px;"
			"background: #1f2937;"
			"}");
		
		QVBoxLayout *messageLayout = new QVBoxLayout(messageGroup);
		messageLayout->setContentsMargins(15, 15, 15, 15);
		messageLayout->setSpacing(15);
		
		// First info text (main message)
		QLabel *info1 = CreateRichTextLabel(infoText1Str, false, true, Qt::AlignCenter);
		info1->setStyleSheet(
			"QLabel {"
			"color: #f3f4f6;"
			"font-size: 14px;"
			"font-weight: bold;"
			"background: transparent;"
			"border: none;"
			"}");
		messageLayout->addWidget(info1);

		// Second info text
		QLabel *info2 = CreateRichTextLabel(infoText2Str, false, true, Qt::AlignCenter);
		info2->setStyleSheet(
			"QLabel {"
			"color: #d1d5db;"
			"font-size: 13px;"
			"background: transparent;"
			"border: none;"
			"}");
		messageLayout->addWidget(info2);

		// Third info text
		QLabel *info3 = CreateRichTextLabel(infoText3Str, false, true, Qt::AlignCenter);
		info3->setStyleSheet(
			"QLabel {"
			"color: #d1d5db;"
			"font-size: 13px;"
			"background: transparent;"
			"border: none;"
			"}");
		messageLayout->addWidget(info3);
		
		dialogLayout->addWidget(messageGroup);

		// Button section
		QHBoxLayout *buttonLayout = new QHBoxLayout();
		buttonLayout->addStretch();
		
		QPushButton *okButton = new QPushButton(obs_module_text("OK"));
		okButton->setStyleSheet(
			"QPushButton {"
			"background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #3b82f6, stop:1 #2563eb);"
			"color: white;"
			"border: none;"
			"padding: 12px 24px;"
			"font-size: 14px;"
			"font-weight: bold;"
			"border-radius: 6px;"
			"min-width: 100px;"
			"}"
			"QPushButton:hover {"
			"background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #60a5fa, stop:1 #3b82f6);"
			"}");
		QObject::connect(okButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		buttonLayout->addWidget(okButton);
		buttonLayout->addStretch();

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