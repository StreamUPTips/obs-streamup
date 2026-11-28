#include "streamup-toolbar-edit-panel.hpp"

#include "hotkey-button-config-dialog.hpp"
#include "websocket-button-config-dialog.hpp"
#include "streamup-toolbar-builder.hpp"
#include "streamup-toolbar-editor.hpp"
#include "streamup-toolbar-status.hpp"

#include <obs-module.h>

#include <QDrag>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/ios-checkbox.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/svg-icon.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/tree-widget.hpp>
#include <streamup/ui/window-chrome.hpp>

namespace StreamUP {

namespace {

using UIStyles::S;

constexpr int kDescriptorRole = Qt::UserRole + 1;

// Draws the palette as the theme's lists do: a quiet grey label for each
// section, then roomy rows with a drag grip, the item's icon and its name, and
// a soft rounded highlight on hover.
class PaletteDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		const bool section = !index.parent().isValid();
		return QSize(option.rect.width(), S(section ? (index.row() == 0 ? 26 : 38) : 34));
	}

	void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		p->save();
		p->setRenderHint(QPainter::Antialiasing, true);
		const QRect r = option.rect;
		const QString text = index.data(Qt::DisplayRole).toString();
		QFont f = option.font;
		if (!index.parent().isValid()) {
			f.setPixelSize(S(12));
			f.setWeight(QFont::DemiBold);
			p->setFont(f);
			p->setPen(QColor(UIStyles::Colors::TEXT_MUTED));
			p->drawText(r.adjusted(S(10), 0, -S(10), -S(6)), Qt::AlignLeft | Qt::AlignBottom, text);
			p->restore();
			return;
		}
		const bool hover = option.state & QStyle::State_MouseOver;
		const bool selected = option.state & QStyle::State_Selected;
		const QRectF pill = QRectF(r).adjusted(S(2), S(1), -S(2), -S(1));
		if (hover || selected) {
			p->setPen(Qt::NoPen);
			p->setBrush(QColor(255, 255, 255, selected ? 26 : 16));
			p->drawRoundedRect(pill, S(8), S(8));
		}
		// Grip: two columns of three dots, brighter on hover.
		p->setPen(Qt::NoPen);
		p->setBrush(QColor(255, 255, 255, hover ? 150 : 70));
		const qreal gx = r.left() + S(12), gy = r.center().y();
		for (int col = 0; col < 2; ++col)
			for (int row = -1; row <= 1; ++row)
				p->drawEllipse(QPointF(gx + col * S(4), gy + row * S(4)), S(1) * 1.1, S(1) * 1.1);
		const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
		const int is = S(18);
		const QRect iconRect(r.left() + S(28), r.center().y() - is / 2, is, is);
		if (!icon.isNull())
			icon.paint(p, iconRect);
		f.setPixelSize(S(13));
		f.setWeight(QFont::Normal);
		p->setFont(f);
		p->setPen(QColor(UIStyles::Colors::TEXT_PRIMARY));
		p->drawText(QRect(iconRect.right() + S(12), r.top(), r.right() - iconRect.right() - S(20), r.height()),
			    Qt::AlignLeft | Qt::AlignVCenter, text);
		p->restore();
	}
};

// The palette is drag-only: a row is a template, never a destination, so the
// tree's own internal-move handling is switched off entirely.
class PaletteTree : public QTreeWidget {
public:
	explicit PaletteTree(QWidget *parent = nullptr) : QTreeWidget(parent)
	{
		setDragEnabled(true);
		setAcceptDrops(false);
		setDragDropMode(QAbstractItemView::DragOnly);
		setDefaultDropAction(Qt::CopyAction);
		setSelectionMode(QAbstractItemView::SingleSelection);
		setHeaderHidden(true);
		setIndentation(0);
		setRootIsDecorated(false);
		setItemsExpandable(false);
		setFrameShape(QFrame::NoFrame);
		setMouseTracking(true);
		setFocusPolicy(Qt::NoFocus);
		setItemDelegate(new PaletteDelegate(this));
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		setStyleSheet(QStringLiteral("QTreeWidget{background:transparent;border:none;outline:none;}"
					     "QTreeWidget::item{background:transparent;border:none;}"));
		viewport()->setAutoFillBackground(false);
		UIStyles::useScrollBars(this);
		setCursor(Qt::OpenHandCursor);
	}

protected:
	Qt::DropActions supportedDropActions() const override { return Qt::CopyAction; }

	QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
	{
		if (items.isEmpty())
			return nullptr;

		const QString descriptor = items.first()->data(0, kDescriptorRole).toString();
		if (descriptor.isEmpty())
			return nullptr; // a group heading, not something you can place

		auto *mime = new QMimeData();
		mime->setData(QString::fromLatin1(kToolbarPaletteMimeType), descriptor.toUtf8());
		return mime;
	}
};

QTreeWidgetItem *makeGroup(QTreeWidget *tree, const QString &title)
{
	auto *group = new QTreeWidgetItem(tree, {title});
	group->setFlags(Qt::ItemIsEnabled);
	group->setExpanded(true);
	return group;
}

// A rounded card on the panel, the theme's settings-group look.
QFrame *makeCard(QWidget *parent, const char *name)
{
	auto *card = new QFrame(parent);
	card->setObjectName(QString::fromLatin1(name));
	card->setStyleSheet(UIStyles::scale_qss(QString("QFrame#%1{background:%2;border:1px solid %3;border-radius:12px;}")
						     .arg(QString::fromLatin1(name), UIStyles::Colors::BG_PRIMARY,
							  UIStyles::Colors::BORDER_SUBTLE)));
	return card;
}

QLabel *sectionLabel(const char *key, QWidget *parent)
{
	auto *l = new QLabel(QString::fromUtf8(obs_module_text(key)), parent);
	l->setStyleSheet(UIStyles::scale_qss(QString("QLabel{color:%1;font-size:12px;font-weight:600;background:transparent;}")
						  .arg(UIStyles::Colors::TEXT_MUTED)));
	return l;
}

} // namespace

ToolbarEditPanel::ToolbarEditPanel(ToolbarEditor *editor, QWidget *parent)
	: QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
	  editor_(editor)
{
	// Qt::Tool keeps the panel above OBS, and not taking focus keeps the toolbar
	// underneath the live editing surface while a palette drag starts.
	setAttribute(Qt::WA_TranslucentBackground, true);
	setAttribute(Qt::WA_DeleteOnClose, false);
	setWindowTitle(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Title")));

	buildUi();
	populatePalette();

	if (editor_) {
		connect(editor_, &ToolbarEditor::itemSelected, this, &ToolbarEditPanel::onSelectionChanged);
		onSelectionChanged(editor_->selectedItemId());
	} else {
		onSelectionChanged(QString());
	}
}

void ToolbarEditPanel::buildUi()
{
	// The shared window chrome is built for ShadowDialog, so a Qt::Tool window
	// borrows only the rounded card from it and keeps the rest hand-rolled.
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *card = new UIStyles::RoundedContainer(16, this, QColor(UIStyles::Colors::BG_DARKEST));
	card->setObjectName("tbEditCard");
	card->setStyleSheet(UIStyles::scale_qss(
		QString("QWidget#tbEditCard{border:1px solid %1;border-radius:16px;}").arg(UIStyles::Colors::BORDER_SUBTLE)));
	outer->addWidget(card);

	auto *root = new QVBoxLayout(card);
	root->setContentsMargins(S(20), S(16), S(20), S(16));
	root->setSpacing(S(14));

	// 1. Header: a grip and the title, both a handle to move the window.
	auto *header = new QWidget(card);
	header->setCursor(Qt::SizeAllCursor);
	header->installEventFilter(new UIStyles::DragFilter(header));
	auto *headerRow = new QHBoxLayout(header);
	headerRow->setContentsMargins(0, 0, 0, 0);
	headerRow->setSpacing(S(10));
	auto *grip = new QLabel(header);
	grip->setPixmap(UIStyles::svgIcon(QStringLiteral("dots"), QColor(UIStyles::Colors::TEXT_MUTED), 18,
					  devicePixelRatioF()));
	grip->setStyleSheet("QLabel{background:transparent;}");
	headerRow->addWidget(grip);
	auto *titles = new QVBoxLayout();
	titles->setSpacing(S(2));
	auto *heading = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Heading")), header);
	heading->setStyleSheet(UIStyles::scale_qss(QString("QLabel{color:%1;font-size:18px;font-weight:600;background:transparent;}")
							.arg(UIStyles::Colors::TEXT_PRIMARY)));
	titles->addWidget(heading);
	auto *guidance = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Guidance")), header);
	guidance->setStyleSheet(UIStyles::scale_qss(QString("QLabel{color:%1;font-size:13px;background:transparent;}")
							 .arg(UIStyles::Colors::TEXT_SECONDARY)));
	guidance->setWordWrap(true);
	titles->addWidget(guidance);
	headerRow->addLayout(titles, 1);
	root->addWidget(header);

	// 2. Two columns: what you can add, and the selected item's settings.
	auto *columns = new QHBoxLayout();
	columns->setSpacing(S(16));
	root->addLayout(columns, 1);

	auto *left = new QVBoxLayout();
	left->setSpacing(S(8));
	left->addWidget(sectionLabel("StreamUP.Toolbar.Panel.AddSection", card));
	auto *paletteCard = makeCard(card, "tbPaletteCard");
	auto *paletteLayout = new QVBoxLayout(paletteCard);
	paletteLayout->setContentsMargins(S(6), S(4), S(6), S(6));
	palette_ = new PaletteTree(paletteCard);
	palette_->setColumnCount(1);
	palette_->setMinimumHeight(S(300));
	palette_->setIconSize(QSize(S(18), S(18)));
	paletteLayout->addWidget(palette_);
	left->addWidget(paletteCard, 1);

	connect(palette_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
		if (!editor_ || !item)
			return;
		const QString descriptor = item->data(0, kDescriptorRole).toString();
		if (descriptor.isEmpty())
			return;
		// Append, for anyone who would rather not drag.
		if (!editor_->insertFromDescriptor(descriptor, -1).isEmpty())
			emit configurationChanged();
	});

	// Hotkey and WebSocket buttons are made in a dialog, so they sit under the
	// list as two small secondary actions.
	auto *makeRow = new QHBoxLayout();
	makeRow->setSpacing(S(8));
	const QIcon plus(UIStyles::svgIcon(QStringLiteral("plus"), QColor(UIStyles::Colors::TEXT_PRIMARY), 14,
					   devicePixelRatioF()));
	auto *hotkeyButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.AddHotkey")), "neutral", card);
	hotkeyButton->setLeadingIcon(plus, 14);
	connect(hotkeyButton, &QPushButton::clicked, this, &ToolbarEditPanel::showHotkeyDialog);
	auto *websocketButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.AddWebSocket")), "neutral", card);
	websocketButton->setLeadingIcon(plus, 14);
	connect(websocketButton, &QPushButton::clicked, this, &ToolbarEditPanel::showWebSocketDialog);
	makeRow->addWidget(hotkeyButton);
	makeRow->addWidget(websocketButton);
	makeRow->addStretch(1);
	left->addLayout(makeRow);
	columns->addLayout(left, 3);

	// 3. Settings for the selection.
	auto *right = new QVBoxLayout();
	right->setSpacing(S(8));
	right->addWidget(sectionLabel("StreamUP.Toolbar.Panel.SelectedSection", card));
	properties_ = makeCard(card, "tbPropsCard");
	propertiesLayout_ = new QVBoxLayout(properties_);
	propertiesLayout_->setContentsMargins(S(16), S(14), S(16), S(14));
	propertiesLayout_->setSpacing(S(12));
	right->addWidget(properties_, 1);
	columns->addLayout(right, 2);

	auto *titleRow = new QHBoxLayout();
	titleRow->setSpacing(S(10));
	propertiesIcon_ = new QLabel(properties_);
	propertiesIcon_->setFixedSize(S(32), S(32));
	propertiesIcon_->setAlignment(Qt::AlignCenter);
	propertiesIcon_->setStyleSheet(UIStyles::scale_qss(
		QString("QLabel{background:%1;border-radius:8px;}").arg(UIStyles::Colors::BG_TERTIARY)));
	titleRow->addWidget(propertiesIcon_);
	propertiesTitle_ = UIStyles::makeLabel(QString(), UIStyles::Sizes::FONT_SIZE_HEADING,
					       UIStyles::Sizes::FONT_WEIGHT_BOLD);
	propertiesTitle_->setParent(properties_);
	propertiesTitle_->setWordWrap(true);
	titleRow->addWidget(propertiesTitle_, 1);
	propertiesLayout_->addLayout(titleRow);

	propertiesHint_ = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.NoSelection")),
				     properties_);
	propertiesHint_->setStyleSheet(UIStyles::scale_qss(QString("QLabel{color:%1;font-size:13px;background:transparent;}")
								.arg(UIStyles::Colors::TEXT_SECONDARY)));
	propertiesHint_->setWordWrap(true);
	propertiesHint_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
	propertiesLayout_->addWidget(propertiesHint_);

	propertiesBody_ = new QWidget(properties_);
	auto *body = new QVBoxLayout(propertiesBody_);
	body->setContentsMargins(0, 0, 0, 0);
	body->setSpacing(S(10));
	propertiesLayout_->addWidget(propertiesBody_);
	propertiesLayout_->addStretch(1);

	visibleCheck_ = new UIStyles::IOSCheckBox(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Visible")), propertiesBody_);
	body->addWidget(visibleCheck_);
	connect(visibleCheck_, &QCheckBox::toggled, this, [this](bool on) {
		if (!editor_)
			return;
		auto item = editor_->selectedItem();
		if (!item || item->visible == on)
			return;
		item->visible = on;
		editor_->rebuild();
		emit configurationChanged();
	});

	sizeRow_ = new QWidget(propertiesBody_);
	auto *sizeLayout = new QHBoxLayout(sizeRow_);
	sizeLayout->setContentsMargins(0, 0, 0, 0);
	sizeLayout->setSpacing(S(8));
	sizeLabel_ = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.SpacerSize")), sizeRow_);
	sizeLabel_->setStyleSheet(UIStyles::labelStyle());
	sizeLayout->addWidget(sizeLabel_, 1);
	sizeSpin_ = new UIStyles::MacSpinBox(sizeRow_);
	sizeSpin_->setOnCard(true);
	sizeSpin_->setRange(5, 4000);
	sizeSpin_->setSuffix(QStringLiteral(" px"));
	sizeLayout->addWidget(sizeSpin_);
	body->addWidget(sizeRow_);
	connect(sizeSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
		if (!editor_)
			return;
		auto item = std::dynamic_pointer_cast<ToolbarConfig::CustomSpacerItem>(editor_->selectedItem());
		if (!item || item->size == value)
			return;
		item->size = value;
		editor_->rebuild();
		emit configurationChanged();
	});

	flexibleCheck_ = new UIStyles::IOSCheckBox(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.SpacerFlexible")), propertiesBody_);
	body->addWidget(flexibleCheck_);
	connect(flexibleCheck_, &QCheckBox::toggled, this, [this](bool on) {
		if (!editor_)
			return;
		auto item = std::dynamic_pointer_cast<ToolbarConfig::CustomSpacerItem>(editor_->selectedItem());
		if (!item || item->flexible == on)
			return;
		item->flexible = on;
		// The size field becomes a minimum rather than a length, so the label
		// alongside it has to change with the tick.
		editor_->rebuild();
		onSelectionChanged(currentId_);
		emit configurationChanged();
	});

	// Status readouts carry their own two settings. Both rebuild the item
	// rather than poking the live widget, because the pinned width has to be
	// recalculated from the new wording or the bar jitters again.
	showIconCheck_ = new UIStyles::IOSCheckBox(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.StatusShowIcon")), propertiesBody_);
	body->addWidget(showIconCheck_);
	connect(showIconCheck_, &QCheckBox::toggled, this, [this](bool on) {
		if (!editor_)
			return;
		auto item = std::dynamic_pointer_cast<ToolbarConfig::StatusItem>(editor_->selectedItem());
		if (!item || item->showIcon == on)
			return;
		item->showIcon = on;
		editor_->rebuild();
		emit configurationChanged();
	});

	showHoursCheck_ = new UIStyles::IOSCheckBox(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.StatusShowHours")), propertiesBody_);
	body->addWidget(showHoursCheck_);
	connect(showHoursCheck_, &QCheckBox::toggled, this, [this](bool on) {
		if (!editor_)
			return;
		auto item = std::dynamic_pointer_cast<ToolbarConfig::StatusItem>(editor_->selectedItem());
		if (!item || item->showHours == on)
			return;
		item->showHours = on;
		editor_->rebuild();
		emit configurationChanged();
	});

	removeRow_ = new QWidget(propertiesBody_);
	auto *removeLayout = new QHBoxLayout(removeRow_);
	removeLayout->setContentsMargins(0, 0, 0, 0);
	removeLayout->addStretch(1);
	auto *removeButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Remove")), "danger", removeRow_);
	removeLayout->addWidget(removeButton);
	body->addWidget(removeRow_);
	connect(removeButton, &QPushButton::clicked, this, [this]() {
		if (!editor_)
			return;
		const QString id = editor_->selectedItemId();
		if (id.isEmpty())
			return;
		editor_->removeItem(id);
		emit configurationChanged();
	});

	// 4. Footer.
	auto *footer = new QHBoxLayout();
	footer->setSpacing(S(8));
	auto *resetButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Reset")), "neutral", card);
	connect(resetButton, &QPushButton::clicked, this, &ToolbarEditPanel::resetRequested);
	footer->addWidget(resetButton);
	footer->addStretch(1);
	auto *doneButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Done")), "primary", card);
	connect(doneButton, &QPushButton::clicked, this, &ToolbarEditPanel::doneRequested);
	footer->addWidget(doneButton);
	root->addLayout(footer);

	setMinimumSize(S(600), S(480));
	resize(S(640), S(520));
}

void ToolbarEditPanel::populatePalette()
{
	palette_->clear();

	QTreeWidgetItem *buttons = makeGroup(palette_,
					     QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.GroupButtons")));
	QTreeWidgetItem *tools = makeGroup(palette_,
					   QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.GroupTools")));
	QTreeWidgetItem *status = makeGroup(palette_,
					    QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.GroupStatus")));
	QTreeWidgetItem *layout = makeGroup(palette_,
					    QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.GroupLayout")));

	for (const QString &descriptor : ToolbarEditor::paletteDescriptors()) {
		auto item = ToolbarEditor::createItemFromDescriptor(descriptor);
		if (!item)
			continue; // a descriptor the builder no longer understands

		QTreeWidgetItem *parent = layout;
		if (descriptor.startsWith(QStringLiteral("builtin:")))
			parent = buttons;
		else if (descriptor.startsWith(QStringLiteral("dock:")))
			parent = tools;
		else if (descriptor.startsWith(QStringLiteral("status:")))
			parent = status;

		auto *row = new QTreeWidgetItem(parent, {ToolbarBuild::labelForItem(item)});
		row->setIcon(0, ToolbarBuild::iconForItem(item));
		row->setData(0, kDescriptorRole, descriptor);
		row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
	}

	// An empty group reads as a fault rather than as "nothing here".
	for (QTreeWidgetItem *group : {buttons, tools, status, layout}) {
		group->setHidden(group->childCount() == 0);
		group->setExpanded(true);
	}
}

void ToolbarEditPanel::onSelectionChanged(const QString &itemId)
{
	auto item = editor_ ? editor_->selectedItem() : nullptr;

	if (!item || itemId.isEmpty()) {
		currentId_.clear();
		propertiesIcon_->clear();
		propertiesTitle_->setText(QString());
		propertiesTitle_->setVisible(false);
		propertiesIcon_->setVisible(false);
		propertiesHint_->setVisible(true);
		propertiesBody_->setVisible(false);
		return;
	}

	const bool sameItem = (itemId == currentId_);
	currentId_ = itemId;

	propertiesHint_->setVisible(false);
	propertiesBody_->setVisible(true);
	propertiesTitle_->setVisible(true);
	propertiesIcon_->setVisible(true);

	if (!sameItem) {
		propertiesTitle_->setText(ToolbarBuild::labelForItem(item));
		const QIcon icon = ToolbarBuild::iconForItem(item);
		propertiesIcon_->setPixmap(icon.isNull() ? QPixmap() : icon.pixmap(QSize(S(18), S(18)), devicePixelRatioF()));
	}

	// The editor re-emits itemSelected on every step of a spacer resize drag, so
	// the values are re-read here and pushed in with signals blocked: writing
	// them back through the valueChanged handler would fight the drag.
	{
		QSignalBlocker blocker(visibleCheck_);
		visibleCheck_->setChecked(item->visible);
	}

	auto spacer = std::dynamic_pointer_cast<ToolbarConfig::CustomSpacerItem>(item);
	sizeRow_->setVisible(spacer != nullptr);
	flexibleCheck_->setVisible(spacer != nullptr);
	if (spacer) {
		QSignalBlocker blocker(sizeSpin_);
		sizeSpin_->setValue(spacer->size);
		{
			QSignalBlocker flexBlocker(flexibleCheck_);
			flexibleCheck_->setChecked(spacer->flexible);
		}
		// A flexible spacer has no length of its own, so the number beside it
		// is the least it will ever be rather than the size it will be.
		sizeLabel_->setText(QString::fromUtf8(obs_module_text(
			spacer->flexible ? "StreamUP.Toolbar.Panel.SpacerMinSize"
					 : "StreamUP.Toolbar.Panel.SpacerSize")));
	}

	auto status = std::dynamic_pointer_cast<ToolbarConfig::StatusItem>(item);
	showIconCheck_->setVisible(status != nullptr);
	if (status) {
		QSignalBlocker blocker(showIconCheck_);
		showIconCheck_->setChecked(status->showIcon);
	}

	// Hours only mean anything on a duration. A CPU readout offering it would
	// be a setting that does nothing.
	ToolbarStatus::Kind kind = ToolbarStatus::Kind::Cpu;
	const bool isDuration = status && ToolbarStatus::kindFromKey(status->kind, kind) &&
				ToolbarStatus::kindIsDuration(kind);
	showHoursCheck_->setVisible(isDuration);
	if (isDuration) {
		QSignalBlocker blocker(showHoursCheck_);
		showHoursCheck_->setChecked(status->showHours);
	}
}

void ToolbarEditPanel::showHotkeyDialog()
{
	HotkeyButtonConfigDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted)
		return;

	auto item = dialog.getHotkeyButtonItem();
	if (!item)
		return;

	// See the header: the panel cannot insert this itself, so the owner does.
	emit itemCreated(std::static_pointer_cast<ToolbarConfig::ToolbarItem>(item));
}

void ToolbarEditPanel::showWebSocketDialog()
{
	WebSocketButtonConfigDialog dialog(this);
	if (dialog.exec() != QDialog::Accepted)
		return;

	auto item = dialog.getWebSocketButtonItem();
	if (!item)
		return;

	// Same route as a hotkey button, for the same reason.
	emit itemCreated(std::static_pointer_cast<ToolbarConfig::ToolbarItem>(item));
}

} // namespace StreamUP
