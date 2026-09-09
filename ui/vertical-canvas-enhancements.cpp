#include "vertical-canvas-enhancements.hpp"
#include "mixer-enhancements.hpp"
#include <streamup/debug-logger.hpp>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QBoxLayout>
#include <QCheckBox>
#include <QLayout>
#include <QMainWindow>
#include <QFrame>
#include <QPushButton>
#include <streamup/ui/gallery-style.hpp>
#include <QRegularExpression>
#include <QWidget>
#include <QTimer>
#include <QPointer>
#include <QMouseEvent>
#include <QMenu>
#include <QEvent>

#include <algorithm>

namespace StreamUP {
namespace VerticalCanvasEnhancements {

// The name the theme paints. Kept in one place so the two cannot drift.
static const char *kToolbarObjectName = "StreamUPVerticalCanvasToolbar";

// Aitum's dock class, matched by class name rather than by object name: the
// object name carries a canvas id, the class does not.
static const char *kCanvasDockClass = "CanvasDock";

// One of the buttons in the row, used to find which layout the row actually is.
// Object names are Aitum's and are stable across their releases.
static const char *kRowMarkerButton = "canvasStream";

static const char *kSeparatorObjectName = "StreamUPVerticalCanvasSeparator";

// A toolbar groups what belongs together, so the row gets dividers around the
// groups. Named buttons are Aitum's; the gear has no name and is found by its
// class instead.
static const char *kSeparatorAfter[] = {"canvasRecord", "canvasVirtualCam"};
static const char *kSeparatorBefore[] = {"canvasRecord", "canvasVirtualCam"};

static void NormaliseButtonCorners(QWidget *bar);
static void ApplyPartnerBlockVisibility(QWidget *dock, QWidget *bar);
static bool ShouldHidePartnerBlocks();
static void WatchChildren(QWidget *dock, QObject *watcher);

// Does this layout contain the given widget, at any depth?
static bool LayoutContains(QLayout *layout, QWidget *widget)
{
	if (!layout || !widget) {
		return false;
	}

	for (int i = 0; i < layout->count(); ++i) {
		QLayoutItem *item = layout->itemAt(i);
		if (!item) {
			continue;
		}
		if (item->widget() == widget) {
			return true;
		}
		if (item->widget() && item->widget()->isAncestorOf(widget)) {
			return true;
		}
		if (LayoutContains(item->layout(), widget)) {
			return true;
		}
	}

	return false;
}

// The index of the row's own item that holds the given widget, or -1. A button
// may be nested inside a group widget or a sub-layout, so this looks through
// both rather than only at direct children.
static int RowIndexHolding(QBoxLayout *row, QWidget *widget)
{
	if (!row || !widget) {
		return -1;
	}

	for (int i = 0; i < row->count(); ++i) {
		QLayoutItem *item = row->itemAt(i);
		if (!item) {
			continue;
		}
		if (item->widget() == widget || (item->widget() && item->widget()->isAncestorOf(widget))) {
			return i;
		}
		if (LayoutContains(item->layout(), widget)) {
			return i;
		}
	}

	return -1;
}

static QFrame *MakeSeparator(QWidget *parent)
{
	QFrame *line = new QFrame(parent);
	line->setObjectName(kSeparatorObjectName);
	line->setFrameShape(QFrame::VLine);
	line->setFrameShadow(QFrame::Plain);
	return line;
}

// Drops dividers into the row so it reads in groups rather than as one long run
// of icons. Inserted back to front, so an earlier insert cannot shift a later
// index out from under us.
static void AddSeparators(QWidget *bar, QBoxLayout *row)
{
	if (!bar || !row) {
		return;
	}

	QList<int> positions;

	for (const char *name : kSeparatorAfter) {
		if (QPushButton *button = bar->findChild<QPushButton *>(QString::fromUtf8(name))) {
			const int index = RowIndexHolding(row, button);
			if (index >= 0) {
				positions.append(index + 1);
			}
		}
	}

	for (const char *name : kSeparatorBefore) {
		if (QPushButton *button = bar->findChild<QPushButton *>(QString::fromUtf8(name))) {
			const int index = RowIndexHolding(row, button);
			if (index >= 0) {
				positions.append(index);
			}
		}
	}

	// The settings button carries no object name, so it is matched on the class
	// property the plugin sets on it.
	for (QPushButton *button : bar->findChildren<QPushButton *>()) {
		if (button->property("class").toString() == QStringLiteral("icon-gear")) {
			const int index = RowIndexHolding(row, button);
			if (index >= 0) {
				positions.append(index + 1);
			}
			break;
		}
	}

	std::sort(positions.begin(), positions.end());
	for (int i = positions.size() - 1; i >= 0; --i) {
		row->insertWidget(positions.at(i), MakeSeparator(bar));
	}
}

// Backtrack-on holds a live QCheckBox rather than an icon, inside a 32px button.
// The layout holding it keeps Qt's default margins, which is 9px a side, leaving
// about 14px for a control that needs more than that, so it was being clipped
// away to nothing and read as a missing icon. A theme cannot reach layout
// margins, so they are cleared here.
static void FixBacktrackCheckbox(QWidget *bar)
{
	QPushButton *button = bar ? bar->findChild<QPushButton *>(QStringLiteral("canvasBacktrackEnable")) : nullptr;
	if (!button || !button->layout()) {
		return;
	}

	button->layout()->setContentsMargins(0, 0, 0, 0);
	button->layout()->setSpacing(0);

	if (QCheckBox *box = button->findChild<QCheckBox *>()) {
		box->setContentsMargins(0, 0, 0, 0);
	}

	// The checkbox wears the theme's toggle switch, which is wider than the 32px
	// the plugin pins this button to in its own stylesheet. That width is taken
	// out and a wider minimum set, so the switch has somewhere to be drawn rather
	// than being clipped to nothing.
	static const QRegularExpression widthRule(R"(width\s*:[^;}]*;?)",
						  QRegularExpression::CaseInsensitiveOption);

	QString sheet = button->styleSheet();
	if (!sheet.isEmpty()) {
		sheet.remove(widthRule);
		button->setStyleSheet(sheet);
	}

	button->setMinimumWidth(StreamUP::UIStyles::S(52));
}

// Aitum gives several of these buttons a stylesheet of their own, and a widget's
// own stylesheet beats the theme. Those stylesheets square off individual
// corners to glue buttons into groups, which leaves the highlight half pill and
// half square once the theme rounds the rest.
//
// The colours in them are meaningful and are left exactly as they are: red while
// recording, blue for backtrack, amber for the virtual camera. Only the corner
// radii are taken out, and a single even radius is put back, so a highlight is
// the same squircle everywhere in the row.
static void NormaliseButtonCorners(QWidget *bar)
{
	if (!bar) {
		return;
	}

	static const QRegularExpression radiusRule(
		QStringLiteral(R"(border-(top|bottom)-(left|right)-radius\s*:[^;}]*;?)"),
		QRegularExpression::CaseInsensitiveOption);

	// Only the toolbar row. Aitum squares off individual corners in there to glue
	// pairs of buttons into groups, which leaves a highlight half pill and half
	// square once the theme rounds the rest. The colours in those stylesheets are
	// meaningful and are left alone: red while recording, blue for backtrack,
	// amber for the virtual camera. Only the radii are replaced.
	//
	// Nothing outside the row is touched. The partner block above it is an
	// ordinary wide button and the theme already gives it the right shape.
	const QString evenCorners =
		QStringLiteral("QPushButton{border-radius: %1px;}").arg(StreamUP::UIStyles::S(6));

	for (QPushButton *button : bar->findChildren<QPushButton *>()) {
		QString sheet = button->styleSheet();
		if (sheet.isEmpty()) {
			continue;
		}

		sheet.remove(radiusRule);
		button->setStyleSheet(sheet + evenCorners);
	}
}

// Whether the partner blocks Aitum fetches into the dock should be hidden.
//
// Deliberately not in the settings window: this hides another plugin's promotion
// of its own product, which is their call to make, not a switch we advertise.
// Anyone who wants it can set it, in OBS' own user config:
//
//   [StreamUP]
//   HideVerticalCanvasPartnerBlocks=true
//
static bool ShouldHidePartnerBlocks()
{
	config_t *config = obs_frontend_get_user_config();
	if (!config) {
		return false;
	}

	return config_get_bool(config, "StreamUP", "HideVerticalCanvasPartnerBlocks");
}

// Hides the partner block and its dismiss button, leaving everything Aitum's
// dock actually does for you untouched. Only the widgets above the control row
// are considered, which is where the blocks are inserted; the row itself and the
// preview are never touched.
static void ApplyPartnerBlockVisibility(QWidget *dock, QWidget *bar)
{
	if (!dock || !bar) {
		return;
	}

	const bool visible = !ShouldHidePartnerBlocks();

	QBoxLayout *dockLayout = qobject_cast<QBoxLayout *>(dock->layout());
	if (!dockLayout) {
		return;
	}

	const int barIndex = dockLayout->indexOf(bar);
	if (barIndex < 0) {
		return;
	}

	// A partner block is a layout of its own sitting above the control row and
	// below the preview. The preview is a widget item, not a layout, so walking
	// the layout items and taking only the sub-layouts leaves it alone.
	int touched = 0;
	for (int i = 0; i < barIndex; ++i) {
		QLayoutItem *item = dockLayout->itemAt(i);
		QLayout *blockLayout = item ? item->layout() : nullptr;
		if (!blockLayout) {
			continue;
		}

		for (int j = 0; j < blockLayout->count(); ++j) {
			if (QWidget *widget = blockLayout->itemAt(j)->widget()) {
				widget->setVisible(visible);
				++touched;
			}
		}
	}

	if (touched > 0) {
		StreamUP::DebugLogger::LogInfo("VerticalCanvas",
			QString("%1 %2 partner block widget(s)")
				.arg(visible ? "Showed" : "Hid")
				.arg(touched)
				.toUtf8()
				.constData());
	}
}

// The filter goes on the dock and on everything inside it: a click lands on
// whichever child is under the pointer, and only a filter on that child sees it.
static void WatchChildren(QWidget *dock, QObject *watcher)
{
	if (!dock || !watcher) {
		return;
	}

	dock->removeEventFilter(watcher);
	dock->installEventFilter(watcher);

	for (QWidget *child : dock->findChildren<QWidget *>()) {
		child->removeEventFilter(watcher);
		child->installEventFilter(watcher);
	}
}

// Watches a Vertical Canvas dock for 2 things.
//
// One: the partner block above the control row is fetched from Aitum's API and
// added some seconds after the dock is built, so it does not exist when the row
// is first wrapped and would otherwise miss the corner pass and the visibility
// setting.
//
// Two: shift and right click anywhere in the dock offers to hide or show those
// blocks. Shift is what keeps it out of the way: a plain right click still gets
// whatever menu Aitum put there, and nothing about this appears in the settings
// window, the hotkey list, or the dock itself.
//
// No Q_OBJECT here on purpose: this needs nothing but the virtual eventFilter,
// so it costs no moc and no extra build wiring.
class DockWatcher : public QObject {
public:
	explicit DockWatcher(QWidget *dock) : QObject(dock), m_dock(dock) {}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (event->type() == QEvent::ChildAdded && m_dock) {
			// Queued, because the child is not finished being set up at
			// the moment this fires.
			QTimer::singleShot(0, m_dock, [this]() { Refresh(); });
		}

		if (event->type() == QEvent::MouseButtonPress && m_dock) {
			auto *mouse = static_cast<QMouseEvent *>(event);
			if (mouse->button() == Qt::RightButton && (mouse->modifiers() & Qt::ShiftModifier)) {
				ShowMenu(mouse->globalPosition().toPoint());
				return true; // swallowed, so Aitum's own menu stays put
			}
		}

		return QObject::eventFilter(watched, event);
	}

private:
	void Refresh()
	{
		if (!m_dock) {
			return;
		}

		QWidget *bar = m_dock->findChild<QWidget *>(kToolbarObjectName);
		NormaliseButtonCorners(bar);
		ApplyPartnerBlockVisibility(m_dock, bar);
		WatchChildren(m_dock, this);
	}

	void ShowMenu(const QPoint &globalPos)
	{
		QMenu menu(m_dock);
		menu.addAction(ShouldHidePartnerBlocks() ? QStringLiteral("Show Aitum's promotions")
							 : QStringLiteral("Hide Aitum's promotions"),
			       []() { TogglePartnerBlocks(); });
		menu.exec(globalPos);
	}

	QPointer<QWidget> m_dock;
};

// Wraps one dock's control row. Returns true if it did anything.
static bool WrapControlRow(QWidget *dock)
{
	if (!dock || dock->findChild<QWidget *>(kToolbarObjectName)) {
		return false; // already done
	}

	QPushButton *marker = dock->findChild<QPushButton *>(kRowMarkerButton);
	QBoxLayout *dockLayout = qobject_cast<QBoxLayout *>(dock->layout());
	if (!marker || !dockLayout) {
		return false;
	}

	// Find the row as a direct child of the dock's own layout, so the widget
	// ends up in the same place in the dock that the row occupied.
	for (int i = 0; i < dockLayout->count(); ++i) {
		QLayoutItem *item = dockLayout->itemAt(i);
		QLayout *rowLayout = item ? item->layout() : nullptr;
		if (!rowLayout || !LayoutContains(rowLayout, marker)) {
			continue;
		}

		// Take the row out, hang it on a widget, and put the widget back
		// where the row was. The buttons never move.
		dockLayout->takeAt(i);

		QWidget *bar = new QWidget(dock);
		bar->setObjectName(kToolbarObjectName);
		bar->setLayout(rowLayout);
		// A stylesheet's padding does not move a layout, only the box it paints,
		// so the gap at the ends of the row has to be set here.
		rowLayout->setContentsMargins(StreamUP::UIStyles::S(10), StreamUP::UIStyles::S(2),
					      StreamUP::UIStyles::S(10), StreamUP::UIStyles::S(2));

		dockLayout->insertWidget(i, bar);
		AddSeparators(bar, qobject_cast<QBoxLayout *>(rowLayout));
		FixBacktrackCheckbox(bar);
		NormaliseButtonCorners(bar);

		ApplyPartnerBlockVisibility(dock, bar);

		// Catches the partner block, which arrives later than this runs, and
		// carries the shift and right click menu.
		WatchChildren(dock, new DockWatcher(dock));

		StreamUP::DebugLogger::LogInfo("VerticalCanvas",
			"Wrapped the Vertical Canvas control row so the theme can paint it");
		return true;
	}

	return false;
}

// Every Vertical Canvas dock currently open, found by class name.
static QList<QWidget *> FindCanvasDocks()
{
	QList<QWidget *> docks;

	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		return docks;
	}

	for (QWidget *widget : mainWindow->findChildren<QWidget *>()) {
		if (widget->metaObject() && qstrcmp(widget->metaObject()->className(), kCanvasDockClass) == 0) {
			docks.append(widget);
		}
	}

	return docks;
}

void ApplyVerticalCanvasEnhancements()
{
	// Same gate as the mixer work: this is a StreamUP theme enhancement, and
	// on any other theme the dock is left exactly as its plugin built it.
	if (!StreamUP::MixerEnhancements::IsUsingStreamUPTheme()) {
		return;
	}

	for (QWidget *dock : FindCanvasDocks()) {
		WrapControlRow(dock);
	}
}

void CleanupVerticalCanvasEnhancements()
{
	for (QWidget *dock : FindCanvasDocks()) {
		QWidget *bar = dock->findChild<QWidget *>(kToolbarObjectName);
		QBoxLayout *dockLayout = bar ? qobject_cast<QBoxLayout *>(dock->layout()) : nullptr;
		if (!bar || !dockLayout) {
			continue;
		}

		// Put the row back where the wrapper sat, then drop the wrapper.
		const int index = dockLayout->indexOf(bar);
		QLayout *rowLayout = bar->layout();
		if (index < 0 || !rowLayout) {
			continue;
		}

		dockLayout->takeAt(index);
		bar->setLayout(nullptr);
		dockLayout->insertLayout(index, rowLayout);

		bar->setParent(nullptr);
		bar->deleteLater();
	}
}


void TogglePartnerBlocks()
{
	config_t *config = obs_frontend_get_user_config();
	if (!config) {
		return;
	}

	const bool hide = !config_get_bool(config, "StreamUP", "HideVerticalCanvasPartnerBlocks");
	config_set_bool(config, "StreamUP", "HideVerticalCanvasPartnerBlocks", hide);
	// Written now rather than at shutdown, so the answer survives a crash and
	// matches what is on screen.
	config_save_safe(config, "tmp", nullptr);

	for (QWidget *dock : FindCanvasDocks()) {
		ApplyPartnerBlockVisibility(dock, dock->findChild<QWidget *>(kToolbarObjectName));
	}

	StreamUP::DebugLogger::LogInfo("VerticalCanvas",
		hide ? "Partner blocks hidden" : "Partner blocks shown");
}

} // namespace VerticalCanvasEnhancements
} // namespace StreamUP
