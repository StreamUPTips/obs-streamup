#include "streamup-toolbar-editor.hpp"
#include "streamup-toolbar-builder.hpp"

#include <algorithm>

#include <QBoxLayout>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStringList>
#include <QToolButton>

#include <streamup/ui/gallery-style.hpp>

namespace StreamUP {

const char *const kToolbarPaletteMimeType = "application/x-streamup-toolbar-item";

namespace {

// Travel before a press becomes a drag.
constexpr int kDragThreshold = 5;
// Grab band on the far end of a spacer.
constexpr int kSpacerGripBand = 6;
// Smallest comfortable target along the flow. A separator is one pixel wide,
// which is not something anyone can reasonably be asked to click.
constexpr int kMinGrabThickness = 14;
constexpr int kSpacerMin = 5;
constexpr int kSpacerMax = 200;
constexpr int kDefaultSpacerSize = 20;

const char *const kDescriptorSeparator = "separator";
const char *const kDescriptorSpacer = "spacer";

} // namespace

ToolbarEditor::ToolbarEditor(QWidget *parent) : QWidget(parent)
{
	setObjectName("StreamUPToolbarEditor");
	setAcceptDrops(true);
	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);

	// The run lives in a layout rather than being positioned by hand, so the
	// editor reports a real size hint. Dropped straight into a QToolBar with
	// no hint it would collapse to nothing.
	hostLayout_ = new QVBoxLayout(this);
	hostLayout_->setContentsMargins(0, 0, 0, 0);
	hostLayout_->setSpacing(0);
}

void ToolbarEditor::setConfiguration(ToolbarConfig::ToolbarConfiguration *config)
{
	config_ = config;
	rebuild();
}

void ToolbarEditor::setAxis(const ToolbarGeom::Axis &axis)
{
	if (axis_.vertical() == axis.vertical())
		return;
	axis_ = axis;
	rebuild();
}

void ToolbarEditor::setAlignment(SettingsManager::ToolbarAlignment alignment)
{
	if (alignment_ == alignment)
		return;
	alignment_ = alignment;
	rebuild();
}

void ToolbarEditor::setSelectedItemId(const QString &id)
{
	if (selectedId_ == id)
		return;
	selectedId_ = id;
	update();
}

std::shared_ptr<ToolbarConfig::ToolbarItem> ToolbarEditor::selectedItem() const
{
	if (!config_ || selectedId_.isEmpty())
		return nullptr;
	return config_->findItem(selectedId_);
}

void ToolbarEditor::rebuild()
{
	if (content_) {
		content_->setParent(nullptr);
		content_->deleteLater();
		content_ = nullptr;
	}
	slots_.clear();

	if (!config_) {
		update();
		return;
	}

	ToolbarBuild::Options opts;
	opts.axis = axis_;
	opts.alignment = alignment_;
	opts.spacing = UIStyles::S(1);

	// Inert lookalikes. No connections, no checkable state, nothing that can
	// fire. The editor takes every mouse event before a child could see one,
	// but a button with no slot attached cannot misbehave even if that failed.
	auto makeWidgets = [this](const std::shared_ptr<ToolbarConfig::ToolbarItem> &item) -> QList<QWidget *> {
		QToolButton *button = new QToolButton(this);
		button->setProperty("class", "streamup-toolbar-button");
		button->setProperty("buttonType", "streamup-button");

		// A hotkey or WebSocket button with no icon chosen would otherwise be
		// an empty square you cannot find, let alone drag. Fall back to the
		// label so every slot is visible while editing.
		const QIcon icon = ToolbarBuild::iconForItem(item);
		if (icon.isNull()) {
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			const QString label = ToolbarBuild::labelForItem(item);
			button->setText(label.isEmpty() ? QStringLiteral("?") : label);
		} else {
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			button->setIcon(icon);
		}
		button->setFocusPolicy(Qt::NoFocus);
		button->setAttribute(Qt::WA_TransparentForMouseEvents);
		return {button};
	};

	auto built = ToolbarBuild::build(*config_, opts, this, makeWidgets);
	content_ = built.container;
	content_->setAttribute(Qt::WA_TransparentForMouseEvents);
	for (QWidget *child : content_->findChildren<QWidget *>())
		child->setAttribute(Qt::WA_TransparentForMouseEvents);

	hostLayout_->addWidget(content_);
	content_->show();

	// Geometry is not settled until the layout has run.
	QMetaObject::invokeMethod(this, [this]() { captureSlots(); }, Qt::QueuedConnection);
	update();
}

void ToolbarEditor::captureSlots()
{
	slots_.clear();
	if (!config_ || !content_)
		return;

	if (content_->layout())
		content_->layout()->activate();

	// Every widget carries its item id, so a slot is matched by name. Running
	// order is not assumed anywhere: the builder moves the StreamUP button to
	// the end, and name matching handles that for free.
	for (QWidget *child : content_->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		const QString id = child->property(ToolbarBuild::kItemIdProperty).toString();
		if (id.isEmpty())
			continue;
		auto item = config_->findItem(id);
		if (!item)
			continue;

		Slot slot;
		slot.id = id;
		slot.type = item->type;
		// Child geometry is relative to the run, hit testing happens in the
		// editor's own coordinates, so shift by wherever the run sits.
		slot.rect = child->geometry().translated(content_->pos());

		// Widen anything too thin to click into a usable target, centred on
		// what is drawn so the padding grows evenly on both sides.
		slot.hitRect = slot.rect;
		const int thickness = axis_.alongLength(slot.rect);
		if (thickness < kMinGrabThickness) {
			const int grow = kMinGrabThickness - thickness;
			const int along = axis_.alongStart(slot.rect) - (grow / 2);
			slot.hitRect = axis_.rect(along, kMinGrabThickness, axis_.acrossStart(slot.rect),
						  axis_.acrossLength(slot.rect));
		}
		slots_.append(slot);
	}

	// Laid-out order, so gap arithmetic and the insertion marker agree with
	// what is on screen rather than with the order children happen to be in.
	std::sort(slots_.begin(), slots_.end(), [this](const Slot &a, const Slot &b) {
		return axis_.alongStart(a.rect) < axis_.alongStart(b.rect);
	});

	update();
}

void ToolbarEditor::resizeSpacerLive(const QString &id, int size)
{
	if (!content_)
		return;
	QWidget *widget = content_->findChild<QWidget *>(id, Qt::FindDirectChildrenOnly);
	if (!widget)
		return;

	// Resize the one widget rather than rebuilding the run, or the widget
	// being dragged is destroyed mid-drag.
	ToolbarBuild::applySpacerSize(widget, size, axis_);
	if (content_->layout())
		content_->layout()->activate();
	captureSlots();
	update();
}

int ToolbarEditor::slotAt(const QPoint &pos) const
{
	// Exact hits win, so a widened hairline never steals a click from the
	// button sitting next to it.
	for (int i = 0; i < slots_.count(); ++i) {
		if (slots_[i].rect.contains(pos))
			return i;
	}
	for (int i = 0; i < slots_.count(); ++i) {
		if (slots_[i].hitRect.contains(pos))
			return i;
	}
	return -1;
}

int ToolbarEditor::insertionIndexAt(const QPoint &pos) const
{
	const int p = axis_.along(pos);
	for (int i = 0; i < slots_.count(); ++i) {
		if (p < axis_.alongCentre(slots_[i].rect))
			return i;
	}
	return slots_.count();
}

int ToolbarEditor::configIndexForGap(int slotGap) const
{
	if (!config_)
		return 0;
	if (slotGap >= 0 && slotGap < slots_.count()) {
		const int idx = config_->getItemIndex(slots_[slotGap].id);
		if (idx >= 0)
			return idx;
	}
	return config_->items.count();
}

QRect ToolbarEditor::insertionMarkerRect(int index) const
{
	constexpr int kMarkerThickness = 3;
	const int acrossPad = 4;
	const int acrossLen = axis_.acrossLength(size()) - (acrossPad * 2);

	if (slots_.isEmpty())
		return axis_.rect(2, kMarkerThickness, acrossPad, acrossLen);

	const int clamped = qBound(0, index, slots_.count());
	int along = 0;
	if (clamped == slots_.count()) {
		along = axis_.alongEnd(slots_.last().rect) + 1;
	} else {
		along = axis_.alongStart(slots_[clamped].rect) - kMarkerThickness;
	}

	return axis_.rect(along, kMarkerThickness, acrossPad, acrossLen);
}

bool ToolbarEditor::isSpacerGrip(int slotIndex, const QPoint &pos) const
{
	if (slotIndex < 0 || slotIndex >= slots_.count())
		return false;
	if (slots_[slotIndex].type != ToolbarConfig::ItemType::CustomSpacer)
		return false;

	const int far = axis_.alongEnd(slots_[slotIndex].rect);
	return qAbs(axis_.along(pos) - far) <= kSpacerGripBand;
}

void ToolbarEditor::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	// The layout has repositioned the run, so the slot rects have moved with it.
	captureSlots();
}

void ToolbarEditor::mousePressEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		event->accept();
		return;
	}

	pressPos_ = event->pos();
	pressSlot_ = slotAt(pressPos_);
	pressed_ = true;
	dragging_ = false;
	draggingOut_ = false;
	dropGap_ = -1;

	if (isSpacerGrip(pressSlot_, pressPos_) && config_) {
		auto item = config_->findItem(slots_[pressSlot_].id);
		if (auto spacer = std::dynamic_pointer_cast<ToolbarConfig::CustomSpacerItem>(item)) {
			resizingSpacer_ = true;
			resizeId_ = slots_[pressSlot_].id;
			resizeStartSize_ = spacer->size;
		}
	}

	const QString id = pressSlot_ >= 0 ? slots_[pressSlot_].id : QString();
	if (id != selectedId_) {
		selectedId_ = id;
		emit itemSelected(selectedId_);
	}
	event->accept();
	update();
}

void ToolbarEditor::mouseMoveEvent(QMouseEvent *event)
{
	const QPoint pos = event->pos();

	if (resizingSpacer_ && config_ && !resizeId_.isEmpty()) {
		const int delta = axis_.along(pos) - axis_.along(pressPos_);
		auto item = config_->findItem(resizeId_);
		if (auto spacer = std::dynamic_pointer_cast<ToolbarConfig::CustomSpacerItem>(item)) {
			const int wanted = qBound(kSpacerMin, resizeStartSize_ + delta, kSpacerMax);
			if (wanted != spacer->size) {
				spacer->size = wanted;
				resizeSpacerLive(resizeId_, wanted);
				// Live, so the panel's size field tracks the drag.
				emit itemSelected(resizeId_);
			}
		}
		event->accept();
		return;
	}

	if (pressed_ && !dragging_ && pressSlot_ >= 0 && (pos - pressPos_).manhattanLength() >= kDragThreshold)
		dragging_ = true;

	if (dragging_) {
		draggingOut_ = !rect().contains(pos);
		dropGap_ = draggingOut_ ? -1 : insertionIndexAt(pos);
		setCursor(draggingOut_ ? Qt::ForbiddenCursor : Qt::ClosedHandCursor);
		update();
		event->accept();
		return;
	}

	const int slot = slotAt(pos);
	if (slot != hoverSlot_) {
		hoverSlot_ = slot;
		update();
	}
	if (isSpacerGrip(slot, pos)) {
		setCursor(axis_.vertical() ? Qt::SizeVerCursor : Qt::SizeHorCursor);
	} else {
		setCursor(slot >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
	}
	event->accept();
}

void ToolbarEditor::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton) {
		event->accept();
		return;
	}

	const bool wasResizing = resizingSpacer_;
	const bool wasDragging = dragging_;
	const bool wasOut = draggingOut_;
	const int gap = dropGap_;
	const int from = pressSlot_;

	pressed_ = false;
	dragging_ = false;
	draggingOut_ = false;
	resizingSpacer_ = false;
	resizeId_.clear();
	dropGap_ = -1;
	setCursor(Qt::ArrowCursor);
	event->accept();

	if (wasResizing) {
		// The size rode along with the drag. Announced once, here.
		emit configurationChanged();
		update();
		return;
	}

	if (!wasDragging || from < 0 || from >= slots_.count() || !config_) {
		update();
		return;
	}

	const QString movingId = slots_[from].id;

	if (wasOut) {
		// Dragged off the bar removes it, the same gesture as pulling a button
		// off a browser toolbar.
		removeItem(movingId);
		return;
	}

	const int fromConfig = config_->getItemIndex(movingId);
	if (fromConfig < 0) {
		update();
		return;
	}

	// Translate the gap between laid-out slots into a config index, then
	// account for the moving item being lifted out before it is put back.
	int toConfig = configIndexForGap(gap);
	if (fromConfig < toConfig)
		toConfig -= 1;

	if (toConfig != fromConfig) {
		config_->moveItem(fromConfig, toConfig);
		rebuild();
		emit configurationChanged();
	} else {
		update();
	}
}

void ToolbarEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
	// Swallowed, so a quick double click cannot reach anything underneath.
	event->accept();
}

void ToolbarEditor::leaveEvent(QEvent *event)
{
	if (!dragging_) {
		hoverSlot_ = -1;
		update();
	}
	QWidget::leaveEvent(event);
}

void ToolbarEditor::removeItem(const QString &id)
{
	if (!config_ || id.isEmpty())
		return;
	config_->removeItem(id);
	if (selectedId_ == id) {
		selectedId_.clear();
		emit itemSelected(QString());
	}
	rebuild();
	emit configurationChanged();
}

QStringList ToolbarEditor::paletteDescriptors()
{
	QStringList out;
	for (const auto &button : ToolbarConfig::ButtonRegistry::getBuiltinButtons())
		out.append(QStringLiteral("builtin:") + button.type);
	for (const auto &dock : ToolbarConfig::ToolbarConfiguration::getAvailableDockButtons())
		out.append(QStringLiteral("dock:") + dock.dockButtonType);
	out.append(QString::fromLatin1(kDescriptorSeparator));
	out.append(QString::fromLatin1(kDescriptorSpacer));
	return out;
}

std::shared_ptr<ToolbarConfig::ToolbarItem> ToolbarEditor::createItemFromDescriptor(const QString &descriptor)
{
	// Ids stay in the historic shape so a migrated configuration and a freshly
	// added item are indistinguishable.
	const qint64 stamp = QDateTime::currentMSecsSinceEpoch();

	if (descriptor == QLatin1String(kDescriptorSeparator))
		return std::make_shared<ToolbarConfig::SeparatorItem>(QStringLiteral("sep_%1").arg(stamp));

	if (descriptor == QLatin1String(kDescriptorSpacer))
		return std::make_shared<ToolbarConfig::CustomSpacerItem>(QStringLiteral("spacer_%1").arg(stamp),
									 kDefaultSpacerSize);

	if (descriptor.startsWith(QLatin1String("builtin:"))) {
		const QString type = descriptor.mid(8);
		const auto info = ToolbarConfig::ButtonRegistry::getButtonInfo(type);
		auto item = std::make_shared<ToolbarConfig::ButtonItem>(
			QStringLiteral("builtin_%1_%2").arg(type).arg(stamp), type);
		item->iconPath = info.defaultIcon;
		item->tooltip = info.defaultTooltip;
		item->checkable = info.checkable;
		return item;
	}

	if (descriptor.startsWith(QLatin1String("dock:"))) {
		const QString type = descriptor.mid(5);
		for (const auto &dock : ToolbarConfig::ToolbarConfiguration::getAvailableDockButtons()) {
			if (dock.dockButtonType != type)
				continue;
			auto item = std::make_shared<ToolbarConfig::DockButtonItem>(
				QStringLiteral("dock_%1_%2").arg(type).arg(stamp), dock.dockButtonType, dock.name);
			item->iconPath = dock.iconPath;
			item->tooltip = dock.tooltip;
			return item;
		}
	}

	return nullptr;
}

QString ToolbarEditor::insertFromDescriptor(const QString &descriptor, int configIndex)
{
	if (!config_)
		return QString();

	auto item = createItemFromDescriptor(descriptor);
	if (!item)
		return QString();

	if (configIndex < 0 || configIndex > config_->items.count()) {
		config_->addItem(item);
	} else {
		config_->items.insert(configIndex, item);
	}

	rebuild();
	selectedId_ = item->id;
	emit itemSelected(selectedId_);
	emit configurationChanged();
	return item->id;
}

void ToolbarEditor::dragEnterEvent(QDragEnterEvent *event)
{
	if (!event->mimeData()->hasFormat(kToolbarPaletteMimeType)) {
		event->ignore();
		return;
	}
	paletteDragActive_ = true;
	dropGap_ = insertionIndexAt(event->position().toPoint());
	event->acceptProposedAction();
	update();
}

void ToolbarEditor::dragMoveEvent(QDragMoveEvent *event)
{
	if (!event->mimeData()->hasFormat(kToolbarPaletteMimeType)) {
		event->ignore();
		return;
	}
	dropGap_ = insertionIndexAt(event->position().toPoint());
	event->acceptProposedAction();
	update();
}

void ToolbarEditor::dragLeaveEvent(QDragLeaveEvent *event)
{
	paletteDragActive_ = false;
	dropGap_ = -1;
	update();
	QWidget::dragLeaveEvent(event);
}

void ToolbarEditor::dropEvent(QDropEvent *event)
{
	if (!event->mimeData()->hasFormat(kToolbarPaletteMimeType)) {
		event->ignore();
		return;
	}

	const QString descriptor = QString::fromUtf8(event->mimeData()->data(kToolbarPaletteMimeType));
	const int configIndex = configIndexForGap(insertionIndexAt(event->position().toPoint()));

	paletteDragActive_ = false;
	dropGap_ = -1;
	event->acceptProposedAction();

	insertFromDescriptor(descriptor, configIndex);
}

void ToolbarEditor::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing, true);

	const QColor accent(UIStyles::Colors::PRIMARY_COLOR);

	// A wash, so it reads as a surface being edited rather than a live bar.
	QColor wash = accent;
	wash.setAlpha(24);
	painter.fillRect(rect(), wash);

	// Spacers and separators are nearly invisible by nature, so give them a
	// body while editing or there is nothing to take hold of.
	for (const Slot &slot : slots_) {
		const bool isSpacer = slot.type == ToolbarConfig::ItemType::CustomSpacer;
		const bool isSeparator = slot.type == ToolbarConfig::ItemType::Separator;
		if (!isSpacer && !isSeparator)
			continue;

		QColor fill = accent;
		fill.setAlpha(isSpacer ? 70 : 130);
		painter.setPen(Qt::NoPen);
		painter.setBrush(fill);

		// Draw the target, not the hairline, so what you can click is what you
		// can see. The line itself stays solid inside it.
		if (isSeparator) {
			QColor pad = accent;
			pad.setAlpha(45);
			painter.setBrush(pad);
			painter.drawRoundedRect(slot.hitRect, 3, 3);
			painter.setBrush(fill);
		}
		painter.drawRect(slot.rect);
	}

	if (hoverSlot_ >= 0 && hoverSlot_ < slots_.count() && !dragging_) {
		QColor hover = accent;
		hover.setAlpha(70);
		painter.setPen(Qt::NoPen);
		painter.setBrush(hover);
		painter.drawRoundedRect(slots_[hoverSlot_].hitRect.adjusted(-2, -2, 2, 2), 6, 6);
	}

	if (!selectedId_.isEmpty()) {
		for (const Slot &slot : slots_) {
			if (slot.id != selectedId_)
				continue;
			QPen pen(accent);
			pen.setWidth(2);
			painter.setPen(pen);
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(slot.hitRect.adjusted(-2, -2, 2, 2), 6, 6);
			break;
		}
	}

	if ((dragging_ && !draggingOut_ && dropGap_ >= 0) || (paletteDragActive_ && dropGap_ >= 0)) {
		painter.setPen(Qt::NoPen);
		painter.setBrush(accent);
		painter.drawRoundedRect(insertionMarkerRect(dropGap_), 2, 2);
	}

	if (draggingOut_) {
		QPen warn(QColor(220, 80, 80));
		warn.setWidth(2);
		warn.setStyle(Qt::DashLine);
		painter.setPen(warn);
		painter.setBrush(Qt::NoBrush);
		painter.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 8, 8);
	}
}

} // namespace StreamUP
