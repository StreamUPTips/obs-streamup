#pragma once

// Animated popups for combo dropdowns and menus (StreamUP standard).
//
// The popups render OPAQUE (POPUP_BG is a solid hex — see comboStyle()/
// menuStyle()); there is no transparency or acrylic, and no Windows DWM
// dependency. What we keep from the old "glass" helpers is purely the open
// animation: a short slide-down + fade-in when the popup appears. That's just a
// window move + opacity tween — it needs neither translucency nor the
// compositor. We also strip the popup's frame so the QSS-rounded item view shows
// cleanly.

#include <QWidget>
#include <QEvent>
#include <QObject>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFrame>
#include <QMenu>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QPoint>
#include <QEasingCurve>
#include <QApplication>

#include "gallery-style.hpp"

namespace StreamUP {
namespace UIStyles {

// Slide-down + fade-in when a popup opens. Native popups (combo lists, QMenus)
// are torn down synchronously on close, so a clean close animation isn't
// reliable — this animates the OPEN only: the popup window starts a few px high
// and transparent, then eases down into its anchored position. Re-runs on every
// show (Qt recreates the native popup window between openings).
inline void animatePopupIn(QWidget *win)
{
	if (!win)
		return;
	const QPoint end = win->pos();
	const int dy = S(8);
	const QPoint start(end.x(), end.y() - dy);
	win->move(start);
	win->setWindowOpacity(0.0);

	auto *grp = new QParallelAnimationGroup(win);
	auto *pos = new QPropertyAnimation(win, "pos");
	pos->setDuration(140);
	pos->setEasingCurve(QEasingCurve::OutCubic);
	pos->setStartValue(start);
	pos->setEndValue(end);
	auto *op = new QPropertyAnimation(win, "windowOpacity");
	op->setDuration(140);
	op->setStartValue(0.0);
	op->setEndValue(1.0);
	grp->addAnimation(pos);
	grp->addAnimation(op);
	grp->start(QAbstractAnimation::DeleteWhenStopped);
}

// Runs the open animation when the watched popup is shown. A single popup open
// can deliver MORE THAN ONE Show event to the watched view — Qt re-shows /
// repositions the popup window while it lays out, and the duplicate Show may
// even resolve to a different window object. Without coalescing, each Show
// restarts the tween, so the popup fades in, snaps back to transparent +
// offset, and fades in again (the "opens twice" flash). We debounce on this
// filter (a stable per-popup object, unlike the recreated window): the first
// Show animates; further Shows within a short window are ignored. A genuine
// re-open is always far enough apart to animate again. No transparency/DWM.
class PopupAnimFilter : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *obj, QEvent *e) override
	{
		// Animate once per open: the first Show after each Hide. One open can
		// deliver several Show events as the popup lays out — only the first
		// runs the tween; the rest are ignored until the popup closes.
		if (e->type() == QEvent::Show) {
			if (auto *w = qobject_cast<QWidget *>(obj)) {
				if (!m_open) {
					m_open = true;
					animatePopupIn(w->window());
				}
			}
		} else if (e->type() == QEvent::Hide) {
			m_open = false;
		}
		return QObject::eventFilter(obj, e);
	}

private:
	bool m_open = false;
};

// We provide our own slide+fade for popups (animatePopupIn). Qt ALSO animates
// combo/menu popups itself (the UI_AnimateCombo / UI_*Menu GUI effects, run
// inside QComboBox::showPopup / QMenu::popup). Left on, the native effect plays
// on top of ours — the popup slides+fades in, then fades AGAIN once it's fully
// open. Turn the native popup effects off so only our animation plays. Global
// + idempotent; safe to call from every makeComboAnimated/makeMenuAnimated.
inline void disableNativePopupEffects()
{
	static bool done = false;
	if (done)
		return;
	done = true;
	QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
	QApplication::setEffectEnabled(Qt::UI_AnimateMenu, false);
	QApplication::setEffectEnabled(Qt::UI_FadeMenu, false);
}

// Give a combo box's dropdown the slide+fade open animation, and remove the
// popup frame so the QSS-rounded item view shows cleanly. Opaque (no glass).
// Idempotent: guarded by a dynamic property so re-calling won't double-install
// the event filter.
inline void makeComboAnimated(QComboBox *combo)
{
	if (!combo)
		return;
	disableNativePopupEffects();
	QAbstractItemView *v = combo->view();
	if (auto *frame = qobject_cast<QFrame *>(v))
		frame->setFrameShape(QFrame::NoFrame);
	if (auto *container = qobject_cast<QFrame *>(v->parentWidget())) {
		container->setFrameShape(QFrame::NoFrame);
		container->setContentsMargins(0, 0, 0, 0);
	}
	if (v->property("suPopupAnimated").toBool())
		return;
	v->setProperty("suPopupAnimated", true);
	v->installEventFilter(new PopupAnimFilter(combo));
}

// Give a QMenu the slide+fade open animation. Opaque (no glass). Idempotent.
inline void makeMenuAnimated(QMenu *menu)
{
	if (!menu)
		return;
	disableNativePopupEffects();
	if (menu->property("suPopupAnimated").toBool())
		return;
	menu->setProperty("suPopupAnimated", true);
	menu->installEventFilter(new PopupAnimFilter(menu));
}

} // namespace UIStyles
} // namespace StreamUP
