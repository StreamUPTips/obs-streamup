#include "multistream-enhancements.hpp"
#include "theme-enhancements.hpp"
#include "vertical-canvas-enhancements.hpp"
#include <streamup/debug-logger.hpp>

#include <obs-frontend-api.h>

#include <QBoxLayout>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include <QCoreApplication>
#include <util/c99defs.h>
#include <streamup/ui/gallery-style.hpp>

namespace StreamUP {
namespace MultistreamEnhancements {

// The names and roles the theme keys on. Kept in one place so the two cannot
// drift.
static const char *kToolbarObjectName = "StreamUPMultistreamToolbar";
static const char *kRoleProperty = "streamupRole";
static const char *kRoleCanvas = "canvas";
static const char *kRoleCanvasTitle = "canvasTitle";
static const char *kRoleOutput = "output";
static const char *kRoleOutputName = "outputName";

// Aitum's dock class, matched by class name.
static const char *kDockClass = "MultistreamDock";

// Each output's stream button. Aitum reuses the Vertical Canvas object name.
static const char *kStreamButton = "canvasStream";

// What Aitum had before we touched it, kept on the widget so cleanup can put it
// back.
static const char *kOriginalSheetProperty = "streamup_original_sheet";
static const char *kFullTextProperty = "streamup_full_text";
static const char *kOriginalMarginsProperty = "streamup_original_margins";
static const char *kOriginalSpacingProperty = "streamup_original_spacing";

// The helpers below carry no Q_OBJECT, so findChild cannot look them up by type;
// they are found by these names instead.
static const char *kEliderName = "StreamUPMultistreamElider";
static const char *kWatcherName = "StreamUPMultistreamWatcher";

// The platform icon size on a card, and the card's own margins. Aitum leaves the
// card layouts at Qt's default margins, which a theme cannot reach.
static const int kIconSize = 24;
static const char *kOriginalSizeProperty = "streamup_original_size";
static const char *kCompactedProperty = "streamup_compacted";

static void Enhance(QWidget *dock);

static void CompactCard(QGroupBox *output)
{
	QLayout *outer = output->layout();
	if (!outer || output->property(kCompactedProperty).isValid()) {
		return;
	}

	const QMargins m = outer->contentsMargins();
	output->setProperty(kCompactedProperty, QVariantList{m.left(), m.top(), m.right(), m.bottom()});
	outer->setContentsMargins(StreamUP::UIStyles::S(8), StreamUP::UIStyles::S(4), StreamUP::UIStyles::S(6),
				  StreamUP::UIStyles::S(4));

	// The icon, name and button row is a layout inside the card's own. Its
	// items fill the row's height by default, and the stream button is taller
	// than the icon and name, so it sat low. Each is centred on the row instead.
	for (int i = 0; i < outer->count(); ++i) {
		QLayout *row = outer->itemAt(i)->layout();
		if (!row) {
			continue;
		}
		row->setSpacing(StreamUP::UIStyles::S(8));
		for (int j = 0; j < row->count(); ++j) {
			QLayoutItem *item = row->itemAt(j);
			if (QWidget *widget = item->widget()) {
				row->setAlignment(widget, item->alignment() | Qt::AlignVCenter);
				if (auto *label = qobject_cast<QLabel *>(widget)) {
					label->setAlignment((label->alignment() & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter);
				}
			}
		}
	}
}

static void Repolish(QWidget *widget)
{
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
	widget->update();
}

// Takes Aitum's stylesheet off a widget, keeping the original for cleanup, and
// gives the widget a role for the theme to find it by.
static void TakeOver(QWidget *widget, const char *role)
{
	if (!widget) {
		return;
	}

	if (!widget->property(kOriginalSheetProperty).isValid()) {
		widget->setProperty(kOriginalSheetProperty, widget->styleSheet());
	}

	const bool roleChanged = role && widget->property(kRoleProperty).toString() != QLatin1String(role);
	if (roleChanged) {
		widget->setProperty(kRoleProperty, QString::fromUtf8(role));
	}

	if (!widget->styleSheet().isEmpty()) {
		widget->setStyleSheet(QString()); // repolishes on its own
	} else if (roleChanged) {
		Repolish(widget);
	}
}

static void GiveBack(QWidget *widget)
{
	if (!widget) {
		return;
	}

	const QVariant original = widget->property(kOriginalSheetProperty);
	widget->setProperty(kRoleProperty, QVariant());
	widget->setProperty(kOriginalSheetProperty, QVariant());
	if (original.isValid()) {
		widget->setStyleSheet(original.toString());
	}
	Repolish(widget);
}

// Output names sit beside the stream button with nothing to stop them growing.
// A QLabel never shortens itself, so one long name made the whole list wider
// than the dock and the stream buttons were cut off at the edge, with no
// scrollbar to reach them. The label is told to take only the space it is
// given, and shows as much of the name as fits, the rest in its tooltip.
class LabelElider : public QObject {
public:
	explicit LabelElider(QLabel *label) : QObject(label), m_label(label)
	{
		setObjectName(kEliderName);
		label->setProperty(kFullTextProperty, label->text());
		label->setToolTip(label->text());
		label->setSizePolicy(QSizePolicy::Ignored, label->sizePolicy().verticalPolicy());
		label->installEventFilter(this);
		Elide();
	}

	void Restore()
	{
		if (!m_label) {
			return;
		}
		m_label->removeEventFilter(this);
		m_label->setText(m_label->property(kFullTextProperty).toString());
		m_label->setProperty(kFullTextProperty, QVariant());
		m_label->setToolTip(QString());
		m_label->setSizePolicy(QSizePolicy::Preferred, m_label->sizePolicy().verticalPolicy());
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (watched == m_label && (event->type() == QEvent::Resize || event->type() == QEvent::FontChange ||
					   event->type() == QEvent::StyleChange)) {
			Elide();
		}
		return QObject::eventFilter(watched, event);
	}

private:
	void Elide()
	{
		if (!m_label) {
			return;
		}
		const QString full = m_label->property(kFullTextProperty).toString();
		const int width = m_label->contentsRect().width();
		m_label->setText(m_label->fontMetrics().elidedText(full, Qt::ElideRight, qMax(width, 0)));
	}

	QPointer<QLabel> m_label;
};

// Names the list's parts and clears the stylesheets Aitum puts on them. Run
// again whenever Aitum rebuilds the outputs, which it does on a profile change
// and after its settings are saved.
static void StyleList(QWidget *dock)
{
	QScrollArea *scroll = dock->findChild<QScrollArea *>();
	QWidget *content = scroll ? scroll->widget() : nullptr;
	if (!content) {
		return;
	}

	// Aitum's "padding: 0px; margin: 0px" on the list. With no selector it
	// reaches every widget inside, which is what kept the theme's spacing out.
	TakeOver(content, nullptr);

	for (QGroupBox *canvas : content->findChildren<QGroupBox *>(Qt::FindDirectChildrenOnly)) {
		TakeOver(canvas, kRoleCanvas);

		// The canvas title is the group's own label. Labels inside an output
		// card, or in the no-vertical warning, belong to a child widget.
		for (QLabel *title : canvas->findChildren<QLabel *>(Qt::FindDirectChildrenOnly)) {
			TakeOver(title, kRoleCanvasTitle);
		}

		for (QGroupBox *output : canvas->findChildren<QGroupBox *>(Qt::FindDirectChildrenOnly)) {
			if (!output->findChild<QPushButton *>(kStreamButton)) {
				continue;
			}
			TakeOver(output, kRoleOutput);
			CompactCard(output);

			// The platform icon is a label too, told apart by its pixmap.
			for (QLabel *name : output->findChildren<QLabel *>(Qt::FindDirectChildrenOnly)) {
				if (!name->pixmap().isNull()) {
					// Aitum draws it at 36px, which is most of what made each
					// card so tall. Scaled into a fixed square instead of
					// replaced, since Aitum sets a new pixmap whenever the
					// stream service changes.
					if (!name->property(kOriginalSizeProperty).isValid()) {
						name->setProperty(kOriginalSizeProperty,
								  QVariantList{name->minimumSize(), name->maximumSize()});
					}
					name->setScaledContents(true);
					name->setFixedSize(StreamUP::UIStyles::S(kIconSize), StreamUP::UIStyles::S(kIconSize));
					continue;
				}
				TakeOver(name, kRoleOutputName);
				if (!name->findChild<QObject *>(kEliderName, Qt::FindDirectChildrenOnly)) {
					new LabelElider(name);
				}
			}
		}
	}
}

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
		if (item->widget() == widget || (item->widget() && item->widget()->isAncestorOf(widget))) {
			return true;
		}
		if (LayoutContains(item->layout(), widget)) {
			return true;
		}
	}

	return false;
}

// The settings button, found by the class Aitum gives it: it has no object name.
static QPushButton *FindGear(QWidget *dock)
{
	QScrollArea *scroll = dock->findChild<QScrollArea *>();
	for (QPushButton *button : dock->findChildren<QPushButton *>()) {
		if (button->property("class").toString() == QStringLiteral("icon-gear") &&
		    !(scroll && scroll->isAncestorOf(button))) {
			return button;
		}
	}
	return nullptr;
}

// The settings, heart and logo row is a bare layout at the bottom of the dock,
// and Qt cannot paint a layout. Hung on a named widget, the theme can give it
// the same toolbar look as the Vertical Canvas row. Nothing is moved or
// replaced; a stretch after the gear sends the heart and logo to the far end,
// where they read as a pair rather than as 2 wide buttons.
static void WrapButtonRow(QWidget *dock)
{
	if (dock->findChild<QWidget *>(kToolbarObjectName)) {
		return;
	}

	QPushButton *gear = FindGear(dock);
	QBoxLayout *dockLayout = qobject_cast<QBoxLayout *>(dock->layout());
	if (!gear || !dockLayout) {
		return;
	}

	for (int i = 0; i < dockLayout->count(); ++i) {
		QLayoutItem *item = dockLayout->itemAt(i);
		QBoxLayout *row = item ? qobject_cast<QBoxLayout *>(item->layout()) : nullptr;
		if (!row || !LayoutContains(row, gear)) {
			continue;
		}

		dockLayout->takeAt(i);

		QWidget *bar = new QWidget(dock);
		bar->setObjectName(kToolbarObjectName);

		const QMargins m = row->contentsMargins();
		bar->setProperty(kOriginalMarginsProperty, QVariantList{m.left(), m.top(), m.right(), m.bottom()});
		bar->setProperty(kOriginalSpacingProperty, row->spacing());

		bar->setLayout(row);
		// A stylesheet's padding does not move a layout, so the row's own
		// margins are what keep the end icons off the dock edges.
		row->setContentsMargins(StreamUP::UIStyles::S(10), StreamUP::UIStyles::S(2), StreamUP::UIStyles::S(10),
					StreamUP::UIStyles::S(2));
		// The buttons carry their own small margin from the theme, as on the
		// Vertical Canvas row, so the heart and logo sit together as a pair.
		row->setSpacing(0);

		// Settings on the left, the heart and logo together on the right,
		// the way the Vertical Canvas row ends with them.
		row->insertStretch(row->indexOf(gear) + 1, 1);

		dockLayout->insertWidget(i, bar);

		StreamUP::DebugLogger::LogInfo("Multistream", "Wrapped the Multistream button row so the theme can paint it");
		return;
	}
}

// Hides the partner blocks and their dismiss button. Aitum inserts them as
// layouts between the list and the button row, so only sub-layouts in that span
// are touched; the list itself is a widget item and is never hidden.
static void ApplyPartnerBlockVisibility(QWidget *dock)
{
	QBoxLayout *dockLayout = dock ? qobject_cast<QBoxLayout *>(dock->layout()) : nullptr;
	QWidget *bar = dock ? dock->findChild<QWidget *>(kToolbarObjectName) : nullptr;
	if (!dockLayout || !bar) {
		return;
	}

	const int barIndex = dockLayout->indexOf(bar);
	if (barIndex < 0) {
		return;
	}

	const bool visible = !StreamUP::VerticalCanvasEnhancements::ShouldHidePartnerBlocks();

	for (int i = 0; i < barIndex; ++i) {
		QLayoutItem *item = dockLayout->itemAt(i);
		QLayout *blockLayout = item ? item->layout() : nullptr;
		if (!blockLayout) {
			continue;
		}
		for (int j = 0; j < blockLayout->count(); ++j) {
			if (QWidget *widget = blockLayout->itemAt(j)->widget()) {
				widget->setVisible(visible);
			}
		}
	}
}

// Watches the dock for 2 things, as on the Vertical Canvas dock.
//
// One: Aitum adds the outputs once OBS has finished loading, rebuilds them on a
// profile change or a settings save, and adds the partner blocks when its API
// answers. None of that exists on the first pass, so each new child queues
// another.
//
// Two: shift and right click offers to hide or show the partner blocks. The
// setting is shared with the Vertical Canvas dock, so either dock's menu
// changes both.
class DockWatcher : public QObject {
public:
	explicit DockWatcher(QWidget *dock) : QObject(dock), m_dock(dock) { setObjectName(kWatcherName); }

	void WatchAll()
	{
		if (!m_dock) {
			return;
		}
		m_dock->removeEventFilter(this);
		m_dock->installEventFilter(this);
		for (QWidget *child : m_dock->findChildren<QWidget *>()) {
			child->removeEventFilter(this);
			child->installEventFilter(this);
		}
	}

	void UnwatchAll()
	{
		if (!m_dock) {
			return;
		}
		m_dock->removeEventFilter(this);
		for (QWidget *child : m_dock->findChildren<QWidget *>()) {
			child->removeEventFilter(this);
		}
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (event->type() == QEvent::ChildAdded && m_dock && !m_pending) {
			// Queued, because the child is not finished being set up when
			// this fires, and coalesced, because a rebuild adds dozens.
			m_pending = true;
			QTimer::singleShot(0, this, [this]() {
				m_pending = false;
				if (m_dock && StreamUP::ThemeEnhancements::IsTweakActive(
						      StreamUP::ThemeEnhancements::Tweak::Multistream)) {
					Enhance(m_dock);
				}
			});
		}

		if (event->type() == QEvent::MouseButtonPress && m_dock) {
			auto *mouse = static_cast<QMouseEvent *>(event);
			if (mouse->button() == Qt::RightButton && (mouse->modifiers() & Qt::ShiftModifier)) {
				QMenu menu(m_dock);
				menu.addAction(StreamUP::VerticalCanvasEnhancements::ShouldHidePartnerBlocks()
						       ? QStringLiteral("Show Aitum's promotions")
						       : QStringLiteral("Hide Aitum's promotions"),
					       []() { StreamUP::VerticalCanvasEnhancements::TogglePartnerBlocks(); });
				menu.exec(mouse->globalPosition().toPoint());
				return true;
			}
		}

		return QObject::eventFilter(watched, event);
	}

private:
	QPointer<QWidget> m_dock;
	bool m_pending = false;
};

static void Enhance(QWidget *dock)
{
	WrapButtonRow(dock);
	StyleList(dock);
	ApplyPartnerBlockVisibility(dock);

	auto *watcher = static_cast<DockWatcher *>(dock->findChild<QObject *>(kWatcherName, Qt::FindDirectChildrenOnly));
	if (!watcher) {
		watcher = new DockWatcher(dock);
	}
	watcher->WatchAll();
}

static QWidget *FindDock()
{
	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow) {
		return nullptr;
	}

	for (QWidget *widget : mainWindow->findChildren<QWidget *>()) {
		if (qstrcmp(widget->metaObject()->className(), kDockClass) == 0) {
			return widget;
		}
	}
	return nullptr;
}

void ApplyMultistreamEnhancements()
{
	// Off, or on another theme: undo whatever an earlier pass did, so the
	// switch takes effect straight away rather than on the next start.
	if (!StreamUP::ThemeEnhancements::IsTweakActive(StreamUP::ThemeEnhancements::Tweak::Multistream)) {
		CleanupMultistreamEnhancements();
		return;
	}

	if (QWidget *dock = FindDock()) {
		Enhance(dock);
	}

	// TEMP-DEBUG-GRAB
	static bool grabArmed = false;
	if (!grabArmed && qEnvironmentVariableIsSet("SU_GRAB")) {
		grabArmed = true;
		QTimer::singleShot(15000, []() {
			QWidget *dock = FindDock();
			if (!dock) return;
			QWidget *host = dock->parentWidget();
			const QSize size(qEnvironmentVariableIntValue("SU_GRAB_W"), qEnvironmentVariableIntValue("SU_GRAB_H"));
			dock->setParent(nullptr);
			dock->setAttribute(Qt::WA_DontShowOnScreen, true);
			dock->resize(size);
			dock->show();
			QCoreApplication::processEvents();
			dock->resize(size);
			QCoreApplication::processEvents();
			dock->grab().save(qEnvironmentVariable("SU_GRAB"));
			UNUSED_PARAMETER(host);
		});
	}
}

void RefreshPartnerBlocks()
{
	if (QWidget *dock = FindDock()) {
		ApplyPartnerBlockVisibility(dock);
	}
}

void CleanupMultistreamEnhancements()
{
	QWidget *dock = FindDock();
	if (!dock) {
		return;
	}

	if (auto *watcher = static_cast<DockWatcher *>(dock->findChild<QObject *>(kWatcherName, Qt::FindDirectChildrenOnly))) {
		watcher->UnwatchAll();
		watcher->deleteLater();
	}

	for (QLabel *label : dock->findChildren<QLabel *>()) {
		if (auto *elider = static_cast<LabelElider *>(label->findChild<QObject *>(kEliderName, Qt::FindDirectChildrenOnly))) {
			elider->Restore();
			elider->deleteLater();
		}
	}

	for (QWidget *widget : dock->findChildren<QWidget *>()) {
		if (widget->property(kOriginalSheetProperty).isValid()) {
			GiveBack(widget);
		}

		const QVariantList margins = widget->property(kCompactedProperty).toList();
		if (margins.size() == 4 && widget->layout()) {
			widget->layout()->setContentsMargins(margins[0].toInt(), margins[1].toInt(), margins[2].toInt(),
							     margins[3].toInt());
			widget->setProperty(kCompactedProperty, QVariant());
		}

		const QVariantList sizes = widget->property(kOriginalSizeProperty).toList();
		if (sizes.size() == 2) {
			if (auto *label = qobject_cast<QLabel *>(widget)) {
				label->setScaledContents(false);
			}
			widget->setMinimumSize(sizes[0].toSize());
			widget->setMaximumSize(sizes[1].toSize());
			widget->setProperty(kOriginalSizeProperty, QVariant());
		}
	}

	// Partner blocks back on screen, then the button row back into the dock.
	QBoxLayout *dockLayout = qobject_cast<QBoxLayout *>(dock->layout());
	QWidget *bar = dock->findChild<QWidget *>(kToolbarObjectName);
	if (!dockLayout || !bar) {
		return;
	}

	const int index = dockLayout->indexOf(bar);
	QBoxLayout *row = qobject_cast<QBoxLayout *>(bar->layout());
	if (index < 0 || !row) {
		return;
	}

	for (int i = 0; i < index; ++i) {
		QLayout *blockLayout = dockLayout->itemAt(i) ? dockLayout->itemAt(i)->layout() : nullptr;
		for (int j = 0; blockLayout && j < blockLayout->count(); ++j) {
			if (QWidget *widget = blockLayout->itemAt(j)->widget()) {
				widget->setVisible(true);
			}
		}
	}

	for (int i = row->count() - 1; i >= 0; --i) {
		if (row->itemAt(i)->spacerItem()) {
			delete row->takeAt(i);
		}
	}

	const QVariantList m = bar->property(kOriginalMarginsProperty).toList();
	if (m.size() == 4) {
		row->setContentsMargins(m[0].toInt(), m[1].toInt(), m[2].toInt(), m[3].toInt());
	}
	row->setSpacing(bar->property(kOriginalSpacingProperty).toInt());

	dockLayout->takeAt(index);
	bar->setLayout(nullptr);
	dockLayout->insertLayout(index, row);

	bar->setParent(nullptr);
	bar->deleteLater();
}

} // namespace MultistreamEnhancements
} // namespace StreamUP
