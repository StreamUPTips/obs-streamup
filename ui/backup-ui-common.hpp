#ifndef STREAMUP_BACKUP_UI_COMMON_HPP
#define STREAMUP_BACKUP_UI_COMMON_HPP

// The backup and restore windows build their sections from the shared design
// system component (streamup/ui/section-card.hpp), so they group information
// exactly like every other StreamUP window. This header only exists to pull
// that in under a short alias.

#include <streamup/ui/section-card.hpp>
#include <streamup/ui/ui-scrollbar.hpp>

#include <QBoxLayout>
#include <QFrame>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace StreamUP {
namespace BackupUI {

using StreamUP::UIStyles::cardFact;
using StreamUP::UIStyles::cardFacts;
using StreamUP::UIStyles::cardFileTable;
using StreamUP::UIStyles::exportFileRows;
using StreamUP::UIStyles::FileExportFormat;
using StreamUP::UIStyles::cardList;
using StreamUP::UIStyles::FileRow;
using StreamUP::UIStyles::cardText;
using StreamUP::UIStyles::formatBytes;
using StreamUP::UIStyles::sectionCard;

// A window taller than the screen loses its footer, and the Restore button with
// it: restoring with media on a 1080p screen put the button out of reach and
// Tab could not get to it either. Call after fitWindowToContent. If the window
// still does not fit the screen it is on, the content area moves into a scroll
// area, so the header and the footer buttons always stay on screen. A window
// that fits is left exactly as it was.
inline bool fitWindowToScreen(QWidget *window, QVBoxLayout *content)
{
	QWidget *host = content ? content->parentWidget() : nullptr;
	QScreen *screen = window ? window->screen() : nullptr;
	if (!host || !screen)
		return false;
	const QRect avail = screen->availableGeometry();
	if (window->height() <= avail.height())
		return false;

	QWidget *card = host->parentWidget();
	auto *box = card ? qobject_cast<QBoxLayout *>(card->layout()) : nullptr;
	const int index = box ? box->indexOf(host) : -1;
	if (index < 0)
		return false;

	auto *scroll = new QScrollArea(card);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"));
	scroll->viewport()->setAutoFillBackground(false);
	StreamUP::UIStyles::useScrollBars(scroll);
	box->insertWidget(index, scroll, box->stretch(index));
	scroll->setWidget(host); // takes the content out of the card layout

	window->resize(window->width(), avail.height());
	window->move(window->x(), avail.top());
	return true;
}

// Room left below the window on its screen, so a window that grows (the restore
// progress bar) never pushes its own footer off the bottom.
inline int roomBelowWindow(QWidget *window)
{
	QScreen *screen = window ? window->screen() : nullptr;
	if (!screen)
		return 0;
	return qMax(0, screen->availableGeometry().bottom() - window->frameGeometry().bottom());
}

} // namespace BackupUI
} // namespace StreamUP

#endif // STREAMUP_BACKUP_UI_COMMON_HPP
