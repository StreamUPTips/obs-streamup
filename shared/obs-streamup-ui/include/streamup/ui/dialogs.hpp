#pragma once

// StreamUP branded message/prompt dialogs — the replacements for the native
// QMessageBox / QInputDialog, which are banned in a branded flow (standard
// §11/§17.10). Built from the same makeWindow chrome as every other custom
// window: a message (or single field) in the content, a Cancel/confirm button
// row in the footer.
//
// These were previously trapped as file-static helpers inside the gallery's
// obs-ui-gallery.cpp; they live here so the shared obs-streamup-ui library owns
// the one true version. Modeless + callback-based (no nested event loop — safe
// to call from any thread already marshalled onto the UI thread). Header-only:
// only lambdas + virtuals already provided by ShadowDialog, so no extra moc.

#include "gallery-style.hpp"
#include "window-chrome.hpp"
#include "pill-button.hpp"
#include "labels.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>

namespace StreamUP {
namespace UIStyles {

// Cancel (outline) + an accent action, added to the shell's right-anchored
// footer button slot. Shared by both dialogs below.
namespace detail {
inline void addDialogButtons(QHBoxLayout *footerButtons, PillButton *&cancelOut,
			     PillButton *&acceptOut, const QString &acceptText,
			     const QString &acceptVariant)
{
	cancelOut = new PillButton("Cancel", "outline");
	acceptOut = new PillButton(acceptText, acceptVariant);
	footerButtons->addWidget(cancelOut);
	footerButtons->addWidget(acceptOut);
}
} // namespace detail

// Branded confirm dialog — replaces QMessageBox. Modeless; invokes onAccept
// when the accept button is pressed. acceptVariant: primary|danger|success|
// outline|neutral. Returns the dialog so the caller can resize/position it.
inline ShadowDialog *confirm(QWidget *parent, const QString &title, const QString &message,
			     const QString &acceptText = "Confirm",
			     const QString &acceptVariant = "primary",
			     std::function<void()> onAccept = {})
{
	// brandFooter=false → secondary window: no "Send us a Beer!" brand line,
	// just the button row.
	auto shell = makeWindow(title, "", parent, /*brandFooter=*/false, "", {}, /*popup=*/true);

	auto *msg = new QLabel(message);
	msg->setWordWrap(true);
	msg->setStyleSheet(scale_qss(
		QString("QLabel{color:%1;font-size:13px;background:transparent;padding:18px 20px;}")
			.arg(Colors::TEXT_SECONDARY)));
	shell.content->addWidget(msg);

	PillButton *cancel = nullptr, *accept = nullptr;
	detail::addDialogButtons(shell.footerButtons, cancel, accept, acceptText, acceptVariant);

	QObject::connect(cancel, &QPushButton::clicked, shell.dialog, &QDialog::close);
	QObject::connect(accept, &QPushButton::clicked, shell.dialog, [dlg = shell.dialog, onAccept]() {
		if (onAccept)
			onAccept();
		dlg->close();
	});

	shell.dialog->resize(S(380), S(190));
	shell.dialog->show();
	return shell.dialog;
}

// Branded notification — replaces an OK-only QMessageBox (information/warning
// with no choice to make). Single dismiss button, modeless.
inline ShadowDialog *info(QWidget *parent, const QString &title, const QString &message,
			  const QString &dismissText = "OK")
{
	auto shell = makeWindow(title, "", parent, /*brandFooter=*/false, "", {}, /*popup=*/true);

	auto *msg = new QLabel(message);
	msg->setWordWrap(true);
	msg->setStyleSheet(scale_qss(
		QString("QLabel{color:%1;font-size:13px;background:transparent;padding:18px 20px;}")
			.arg(Colors::TEXT_SECONDARY)));
	shell.content->addWidget(msg);

	auto *ok = new PillButton(dismissText, "primary");
	shell.footerButtons->addWidget(ok);
	QObject::connect(ok, &QPushButton::clicked, shell.dialog, &QDialog::close);

	shell.dialog->resize(S(380), S(180));
	shell.dialog->show();
	return shell.dialog;
}

// Branded single-field prompt — replaces QInputDialog. Modeless; invokes
// onAccept with the field text when the accept button is pressed.
inline ShadowDialog *prompt(QWidget *parent, const QString &title, const QString &fieldLabel,
			    const QString &initial, std::function<void(const QString &)> onAccept,
			    const QString &acceptText = "Save")
{
	auto shell = makeWindow(title, "", parent, /*brandFooter=*/false, "", {}, /*popup=*/true);

	auto *body = new QWidget();
	auto *bl = new QVBoxLayout(body);
	bl->setContentsMargins(S(20), S(16), S(20), S(8));
	bl->setSpacing(S(8));
	bl->addWidget(makeLabel(fieldLabel, Sizes::FONT_SIZE_TINY, Sizes::FONT_WEIGHT_NORMAL,
				Colors::TEXT_MUTED));
	auto *field = new QLineEdit(initial);
	field->setStyleSheet(lineEditStyle(false));
	field->setFixedHeight(S(28));
	field->selectAll();
	bl->addWidget(field);
	shell.content->addWidget(body);

	PillButton *cancel = nullptr, *accept = nullptr;
	detail::addDialogButtons(shell.footerButtons, cancel, accept, acceptText, "primary");

	QObject::connect(cancel, &QPushButton::clicked, shell.dialog, &QDialog::close);
	auto submit = [dlg = shell.dialog, field, onAccept]() {
		if (onAccept)
			onAccept(field->text());
		dlg->close();
	};
	QObject::connect(accept, &QPushButton::clicked, shell.dialog, submit);
	QObject::connect(field, &QLineEdit::returnPressed, shell.dialog, submit);

	shell.dialog->resize(S(360), S(190));
	shell.dialog->show();
	field->setFocus();
	return shell.dialog;
}

// Branded "unsaved changes on close" dialog — the apply-or-discard flow a plugin
// triggers from its window close handler when the editor is dirty. Two footer
// actions: `closeText` (neutral, left) discards and closes; `applyText` (primary,
// right) applies then closes. The dialog's own CloseButton / Escape just cancels
// (stays open, no callback) so the user can return to editing. Modeless +
// WA_DeleteOnClose like the others; QPointer/value-capture safe.
inline ShadowDialog *confirmDiscard(QWidget *parent, const QString &title, const QString &message,
				    std::function<void()> onApplyClose, std::function<void()> onClose,
				    const QString &applyText = "Apply & Close",
				    const QString &closeText = "Close")
{
	auto shell = makeWindow(title, "", parent, /*brandFooter=*/false, "", {}, /*popup=*/true);

	auto *msg = new QLabel(message);
	msg->setWordWrap(true);
	msg->setStyleSheet(scale_qss(
		QString("QLabel{color:%1;font-size:13px;background:transparent;padding:18px 20px;}")
			.arg(Colors::TEXT_SECONDARY)));
	shell.content->addWidget(msg);

	// closeText (neutral) on the LEFT, applyText (primary) on the RIGHT.
	auto *close = new PillButton(closeText, "neutral");
	auto *apply = new PillButton(applyText, "primary");
	shell.footerButtons->addWidget(close);
	shell.footerButtons->addWidget(apply);

	QObject::connect(apply, &QPushButton::clicked, shell.dialog,
			 [dlg = shell.dialog, onApplyClose]() {
				 if (onApplyClose)
					 onApplyClose();
				 dlg->close();
			 });
	QObject::connect(close, &QPushButton::clicked, shell.dialog,
			 [dlg = shell.dialog, onClose]() {
				 if (onClose)
					 onClose();
				 dlg->close();
			 });

	shell.dialog->resize(S(420), S(200));
	shell.dialog->show();
	return shell.dialog;
}

} // namespace UIStyles
} // namespace StreamUP
