#include "source-tree-enhancements.hpp"
#include "mixer-enhancements.hpp"
#include "settings-manager.hpp"

#include <obs-module.h>

#include <QAbstractItemModel>
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPointer>
#include <QSpacerItem>
#include <QTimer>
#include <QWidget>

namespace StreamUP {
namespace SourceTreeEnhancements {

// OBS's Sources dock and Aitum Vertical's copy of it are both a class named
// SourceTree, and both build their rows the same way: a SourceTreeItem whose
// layout starts with a 3px spacer (plain row), a 16px spacer (a group's child)
// or the group's expand checkbox, then the source icon and the name.
static const char *kTreeClass = "SourceTree";

// The gap OBS puts in front of a plain row's icon. The theme gives the group
// arrow the same, so the arrow sits in the icon column.
static constexpr int kIconLead = 3;

// Half the 16px source icon: where the guide line runs, under the arrow.
static constexpr int kIconHalf = 8;

// How far short of a child's icon the guide's turn into the last child stops.
static constexpr int kElbowGap = 5;

// The width OBS gives a child's spacer, put back when the theme changes away.
static constexpr int kObsChildSpacer = 16;

// Set on a group row's icon label: the open state its pixmap was last set for,
// and the pixmap OBS gave it, so it can be restored.
static const char *kFolderStateProp = "streamup_folder_open";
static const char *kOriginalPixmapProp = "streamup_original_pixmap";

static SourceTreeFilter *g_filter = nullptr;
static QList<QPointer<QListView>> g_trees;

enum class RowType { Plain, Group, Child };

struct Row {
	RowType type = RowType::Plain;
	QWidget *item = nullptr;
	QCheckBox *expand = nullptr;  // Group
	QWidget *first = nullptr;     // Group: the widget after the arrow (icon, or name if icons are off)
	QSpacerItem *spacer = nullptr; // Child
};

static Row ClassifyRow(QListView *tree, int row)
{
	Row r;
	QAbstractItemModel *model = tree->model();
	r.item = tree->indexWidget(model->index(row, 0));
	if (!r.item) {
		return r;
	}

	auto *box = qobject_cast<QHBoxLayout *>(r.item->layout());
	if (!box || box->count() < 2) {
		return r;
	}

	QLayoutItem *lead = box->itemAt(0);
	if (auto *check = qobject_cast<QCheckBox *>(lead->widget())) {
		r.type = RowType::Group;
		r.expand = check;
		r.first = box->itemAt(1)->widget();
	} else if (QSpacerItem *spacer = lead->spacerItem()) {
		// A plain row's spacer is 3px; anything wider is a child, whether OBS's
		// 16px or the width set here.
		if (spacer->sizeHint().width() > 3) {
			r.type = RowType::Child;
			r.spacer = spacer;
		}
	}

	return r;
}

static bool IsActive()
{
	return StreamUP::MixerEnhancements::IsUsingStreamUPTheme();
}

// The folder pixmap for a group row, tinted to the icon's own text colour the
// way the Scene Organiser tints its folders.
static QPixmap FolderPixmap(bool open, QLabel *label)
{
	const char *fileName = open ? "icons/folder-open.svg" : "icons/folder-closed.svg";

	QIcon icon;
	if (char *path = obs_module_file(fileName)) {
		icon = QIcon(QString::fromUtf8(path));
		bfree(path);
	}
	if (icon.isNull()) {
		return QPixmap();
	}

	QPixmap pixmap = icon.pixmap(QSize(16, 16), label->devicePixelRatioF());
	QPainter painter(&pixmap);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.fillRect(pixmap.rect(), label->palette().color(QPalette::WindowText));
	painter.end();
	return pixmap;
}

// Where a group's guide line runs (under the arrow) and where its children's
// icons start (level with the group's folder icon), in the group row's own
// coordinates.
static int GuideX(const Row &group)
{
	return group.expand->x() + kIconLead + kIconHalf;
}

static int ChildIconX(const Row &group)
{
	return group.first->x();
}

// Where a plain row's name starts, in its own coordinates: past the 3px lead,
// the icon, its gap and OBS's 2px spacing. The group arrow is given exactly
// that much room, so the arrow takes a source icon's slot and the folder icon
// starts where a plain row's name does. -1 when the tree has no plain row with
// an icon to measure (icons off, or only groups), leaving the theme's size.
static int PlainNameX(QListView *tree)
{
	const int rows = tree->model()->rowCount();
	for (int i = 0; i < rows; ++i) {
		const Row r = ClassifyRow(tree, i);
		if (r.type != RowType::Plain || !r.item) {
			continue;
		}
		auto *box = qobject_cast<QHBoxLayout *>(r.item->layout());
		if (!box) {
			continue;
		}
		bool pastIcon = false;
		for (int j = 0; j < box->count(); ++j) {
			QWidget *w = box->itemAt(j)->widget();
			if (!w) {
				continue;
			}
			if (pastIcon) {
				return w->x();
			}
			if (!w->property("class").toString().contains(QStringLiteral("source-icon"))) {
				break;
			}
			pastIcon = true;
		}
	}
	return -1;
}

// The gap the row's layout puts between its widgets. OBS's own rows set it to
// 0; Aitum Vertical's leave Qt's default, which lands between the arrow and the
// folder icon. Qt adds none beside a spacer, so a child row's lead needs none.
static int RowSpacing(const Row &r)
{
	return qMax(0, r.item->layout()->spacing());
}

// The fixed width wanted for a group's arrow, or 0 for none (Qt's own size).
static int ArrowWidth(const Row &group, int plainNameX, bool active)
{
	if (!active || plainNameX < 0) {
		return 0;
	}
	const int width = plainNameX - RowSpacing(group) - group.expand->x();
	return width > 0 ? width : 0;
}

static int CurrentArrowWidth(QCheckBox *expand)
{
	return expand->minimumWidth() == expand->maximumWidth() ? expand->minimumWidth() : 0;
}

static void SetArrowWidth(QCheckBox *expand, int width)
{
	if (width > 0) {
		expand->setFixedWidth(width);
	} else {
		expand->setMinimumWidth(0);
		expand->setMaximumWidth(QWIDGETSIZE_MAX);
	}
}

// Aitum Vertical's rows leave their layout at Qt's default spacing, where
// OBS's own set it to 0, so every icon, arrow and folder there sat further
// apart than in the Sources dock. Zeroed while the theme is active so the two
// docks match, and put back after.
static const char *kOriginalSpacingProp = "streamup_original_spacing";

static bool SpacingNeedsSync(QWidget *item, bool active)
{
	QLayout *layout = item->layout();
	if (!layout) {
		return false;
	}
	return active ? layout->spacing() != 0 : item->property(kOriginalSpacingProp).isValid();
}

static void SyncSpacing(QWidget *item, bool active)
{
	QLayout *layout = item->layout();
	if (!layout) {
		return;
	}
	if (active && layout->spacing() != 0) {
		item->setProperty(kOriginalSpacingProp, layout->spacing());
		layout->setSpacing(0);
	} else if (!active && item->property(kOriginalSpacingProp).isValid()) {
		layout->setSpacing(item->property(kOriginalSpacingProp).toInt());
		item->setProperty(kOriginalSpacingProp, QVariant());
	}
}

// Brings the rows' layout and folder icons in line with the current theme.
// Run outside the paint, since both change widget geometry or pixmaps.
static void SyncTree(QListView *tree)
{
	if (!tree || !tree->model()) {
		return;
	}

	const bool active = IsActive();
	const int rows = tree->model()->rowCount();
	const int plainNameX = PlainNameX(tree);
	const Row *group = nullptr;
	Row current;
	Row lastGroup;

	for (int i = 0; i < rows; ++i) {
		current = ClassifyRow(tree, i);
		if (current.item) {
			SyncSpacing(current.item, active);
		}

		if (current.type == RowType::Group) {
			lastGroup = current;
			group = &lastGroup;

			const int arrow = ArrowWidth(current, plainNameX, active);
			if (CurrentArrowWidth(current.expand) != arrow) {
				SetArrowWidth(current.expand, arrow);
			}

			auto *icon = qobject_cast<QLabel *>(current.first);
			if (!icon || !icon->property("class").toString().contains(QStringLiteral("source-icon"))) {
				continue;
			}

			if (!icon->property(kOriginalPixmapProp).isValid()) {
				icon->setProperty(kOriginalPixmapProp, QVariant::fromValue(icon->pixmap()));
			}

			if (active) {
				const bool open = !current.expand->isChecked();
				const QVariant state = icon->property(kFolderStateProp);
				if (!state.isValid() || state.toBool() != open) {
					const QPixmap folder = FolderPixmap(open, icon);
					if (!folder.isNull()) {
						icon->setPixmap(folder);
						icon->setProperty(kFolderStateProp, open);
					}
				}
			} else if (icon->property(kFolderStateProp).isValid()) {
				icon->setPixmap(icon->property(kOriginalPixmapProp).value<QPixmap>());
				icon->setProperty(kFolderStateProp, QVariant());
			}
		} else if (current.type == RowType::Child) {
			int width = kObsChildSpacer;
			if (active && group && group->first) {
				width = ChildIconX(*group) - current.spacer->geometry().x();
			}
			if (width > 3 && current.spacer->sizeHint().width() != width) {
				current.spacer->changeSize(width, 1);
				current.item->layout()->invalidate();
			}
		} else {
			group = nullptr;
		}
	}
}

static bool NeedsSync(QListView *tree)
{
	const bool active = IsActive();
	const int rows = tree->model()->rowCount();
	const int plainNameX = PlainNameX(tree);
	Row lastGroup;
	bool inGroup = false;

	for (int i = 0; i < rows; ++i) {
		const Row r = ClassifyRow(tree, i);
		if (r.item && SpacingNeedsSync(r.item, active)) {
			return true;
		}
		if (r.type == RowType::Group) {
			lastGroup = r;
			inGroup = r.first != nullptr;
			if (CurrentArrowWidth(r.expand) != ArrowWidth(r, plainNameX, active)) {
				return true;
			}
			auto *icon = qobject_cast<QLabel *>(r.first);
			if (icon && icon->property("class").toString().contains(QStringLiteral("source-icon"))) {
				const QVariant state = icon->property(kFolderStateProp);
				if (active ? (!state.isValid() || state.toBool() == r.expand->isChecked()) : state.isValid()) {
					return true;
				}
			}
		} else if (r.type == RowType::Child) {
			const int want = (active && inGroup) ? ChildIconX(lastGroup) - r.spacer->geometry().x()
							     : kObsChildSpacer;
			if (want > 3 && r.spacer->sizeHint().width() != want) {
				return true;
			}
		} else {
			inGroup = false;
		}
	}
	return false;
}

static void ScheduleSync(QListView *tree)
{
	if (tree->property("streamup_sync_pending").toBool()) {
		return;
	}
	tree->setProperty("streamup_sync_pending", true);

	QPointer<QListView> guard(tree);
	QTimer::singleShot(0, tree, [guard]() {
		if (!guard) {
			return;
		}
		guard->setProperty("streamup_sync_pending", false);
		SyncTree(guard);
	});
}

// Guide lines under each group's arrow, drawn the way the Scene
// Organiser draws them: a straight run beside each child, turning into the last
// one. They go on the viewport, under the row widgets, so nothing in a row is
// ever drawn over.
static void PaintGuides(QListView *tree)
{
	if (!StreamUP::SettingsManager::GetCurrentSettings().sceneOrganiserShowIndentGuides) {
		return;
	}

	QWidget *viewport = tree->viewport();
	QAbstractItemModel *model = tree->model();
	const int rows = model->rowCount();

	QColor guide = tree->palette().color(QPalette::Text);
	guide.setAlpha(60);

	QPainter painter(viewport);
	painter.setPen(QPen(guide, 1));

	Row lastGroup;
	bool inGroup = false;

	for (int i = 0; i < rows; ++i) {
		const Row r = ClassifyRow(tree, i);
		if (r.type == RowType::Group) {
			lastGroup = r;
			inGroup = r.first != nullptr;
			continue;
		}
		if (r.type != RowType::Child || !inGroup) {
			inGroup = false;
			continue;
		}

		const QRect rect = tree->visualRect(model->index(i, 0));
		if (!rect.isValid() || !rect.intersects(viewport->rect())) {
			continue;
		}

		const bool last = (i + 1 >= rows) || ClassifyRow(tree, i + 1).type != RowType::Child;
		const int x = lastGroup.item->x() + GuideX(lastGroup);
		const int top = rect.top();
		const int bottom = rect.bottom() + tree->spacing() + 1;
		const int middle = rect.top() + (rect.height() / 2);

		if (last) {
			painter.drawLine(x, top, x, middle);
			painter.drawLine(x, middle, lastGroup.item->x() + ChildIconX(lastGroup) - kElbowGap, middle);
		} else {
			painter.drawLine(x, top, x, bottom);
		}
	}
}

SourceTreeFilter::SourceTreeFilter(QObject *parent) : QObject(parent) {}

static void Hook(QListView *tree)
{
	if (!tree || tree->property("streamup_tree_hooked").toBool()) {
		return;
	}
	tree->setProperty("streamup_tree_hooked", true);
	g_trees.append(tree);
	tree->viewport()->update();
}

bool SourceTreeFilter::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::Show && watched->inherits(kTreeClass)) {
		Hook(qobject_cast<QListView *>(watched));
		return false;
	}

	if (event->type() != QEvent::Paint) {
		return false;
	}

	auto *viewport = qobject_cast<QWidget *>(watched);
	auto *tree = viewport ? qobject_cast<QListView *>(viewport->parentWidget()) : nullptr;
	if (!tree || !tree->property("streamup_tree_hooked").toBool() || !tree->model()) {
		return false;
	}

	// The list paints first (row backgrounds, selection), then the guides go
	// on top of that and under the row widgets. Re-sending the event with this
	// flag set lets it through to the list's own handler.
	if (viewport->property("streamup_in_paint").toBool()) {
		return false;
	}
	viewport->setProperty("streamup_in_paint", true);
	QCoreApplication::sendEvent(viewport, event);
	viewport->setProperty("streamup_in_paint", false);

	if (IsActive()) {
		PaintGuides(tree);
	}
	if (NeedsSync(tree)) {
		ScheduleSync(tree);
	}
	return true;
}

void ApplySourceTreeEnhancements()
{
	if (!g_filter) {
		g_filter = new SourceTreeFilter();
		// Application wide, so it sees each tree's viewport paint and also
		// trees created later, such as Vertical's when its canvas is added
		// after this runs.
		QApplication::instance()->installEventFilter(g_filter);
	}

	for (QWidget *widget : QApplication::allWidgets()) {
		if (widget->inherits(kTreeClass)) {
			Hook(qobject_cast<QListView *>(widget));
		}
	}
}

void CleanupSourceTreeEnhancements()
{
	if (!g_filter) {
		return;
	}

	QApplication::instance()->removeEventFilter(g_filter);
	for (const QPointer<QListView> &tree : g_trees) {
		if (tree) {
			tree->setProperty("streamup_tree_hooked", false);
			tree->viewport()->update();
		}
	}
	g_trees.clear();

	delete g_filter;
	g_filter = nullptr;
}

} // namespace SourceTreeEnhancements
} // namespace StreamUP
