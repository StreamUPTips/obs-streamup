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

// Re-runs the open animation every time the watched popup is shown (Qt may
// recreate the native popup window between openings). No transparency/DWM.
class PopupAnimFilter : public QObject {
public:
	using QObject::QObject;

protected:
	bool eventFilter(QObject *obj, QEvent *e) override
	{
		if (e->type() == QEvent::Show) {
			if (auto *w = qobject_cast<QWidget *>(obj))
				animatePopupIn(w->window());
		}
		return QObject::eventFilter(obj, e);
	}
};

// Give a combo box's dropdown the slide+fade open animation, and remove the
// popup frame so the QSS-rounded item view shows cleanly. Opaque (no glass).
// Idempotent: guarded by a dynamic property so re-calling won't double-install
// the event filter.
inline void makeComboAnimated(QComboBox *combo)
{
	if (!combo)
		return;
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
	if (menu->property("suPopupAnimated").toBool())
		return;
	menu->setProperty("suPopupAnimated", true);
	menu->installEventFilter(new PopupAnimFilter(menu));
}

} // namespace UIStyles
} // namespace StreamUP
