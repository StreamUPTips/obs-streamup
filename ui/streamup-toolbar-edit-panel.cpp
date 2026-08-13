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
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/ios-checkbox.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/tree-widget.hpp>
#include <streamup/ui/window-chrome.hpp>

namespace StreamUP {

namespace {

using UIStyles::S;

constexpr int kDescriptorRole = Qt::UserRole + 1;

// The palette is drag-only: a row is a template, never a destination, so the
// tree's own internal-move handling is switched off entirely.
class PaletteTree : public UIStyles::TreeWidget {
public:
	explicit PaletteTree(QWidget *parent = nullptr) : UIStyles::TreeWidget(parent)
	{
		setDragEnabled(true);
		setAcceptDrops(false);
		setDragDropMode(QAbstractItemView::DragOnly);
		setDefaultDropAction(Qt::CopyAction);
		setSelectionMode(QAbstractItemView::SingleSelection);
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
	QFont f = group->font(0);
	f.setBold(true);
	group->setFont(0, f);
	group->setForeground(0, QColor(UIStyles::Colors::TEXT_MUTED));
	group->setExpanded(true);
	return group;
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

	auto *card = new UIStyles::RoundedContainer(UIStyles::Sizes::RADIUS_CARD, this,
						    QColor(UIStyles::Colors::BG_DARKEST));
	outer->addWidget(card);

	auto *root = new QVBoxLayout(card);
	root->setContentsMargins(S(14), S(12), S(14), S(12));
	root->setSpacing(S(8));

	// 1. Heading and guidance.
	auto *heading = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Heading")), card);
	heading->setFont(UIStyles::displayFont(16, 700));
	heading->setStyleSheet(QString("QLabel{color:%1;background:transparent;}").arg(UIStyles::Colors::PRIMARY_COLOR));
	// The heading doubles as the drag handle, since a frameless tool window has
	// no titlebar to grab.
	heading->installEventFilter(new UIStyles::DragFilter(heading));
	heading->setCursor(Qt::SizeAllCursor);
	root->addWidget(heading);

	auto *guidance = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.Guidance")), card);
	guidance->setStyleSheet(UIStyles::dimLabelStyle());
	guidance->setWordWrap(true);
	root->addWidget(guidance);

	// 2. Palette.
	palette_ = new PaletteTree(card);
	palette_->setColumnCount(1);
	palette_->setMinimumHeight(S(180));
	palette_->setIconSize(QSize(S(16), S(16)));
	root->addWidget(palette_, 1);

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

	// 3. Hotkey button.
	auto *hotkeyButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.AddHotkey")), "outline", card);
	connect(hotkeyButton, &QPushButton::clicked, this, &ToolbarEditPanel::showHotkeyDialog);

	// 4. WebSocket button. Same shape as the hotkey one: it cannot be described
	// as a palette descriptor because it only exists once the request, its
	// arguments and an icon have been chosen.
	auto *websocketButton = new UIStyles::PillButton(
		QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.AddWebSocket")), "outline", card);
	connect(websocketButton, &QPushButton::clicked, this, &ToolbarEditPanel::showWebSocketDialog);
	root->addWidget(hotkeyButton);
	root->addWidget(websocketButton);

	// 4. Properties for the selection.
	properties_ = new UIStyles::RoundedContainer(UIStyles::Sizes::RADIUS_INPUT, card,
						     QColor(UIStyles::Colors::BG_PRIMARY));
	propertiesLayout_ = new QVBoxLayout(properties_);
	propertiesLayout_->setContentsMargins(S(10), S(10), S(10), S(10));
	propertiesLayout_->setSpacing(S(8));
	root->addWidget(properties_);

	auto *titleRow = new QHBoxLayout();
	titleRow->setSpacing(S(8));
	propertiesIcon_ = new QLabel(properties_);
	propertiesIcon_->setFixedSize(S(18), S(18));
	propertiesIcon_->setStyleSheet("QLabel{background:transparent;}");
	titleRow->addWidget(propertiesIcon_);
	propertiesTitle_ = UIStyles::makeLabel(QString(), UIStyles::Sizes::FONT_SIZE_HEADING,
					       UIStyles::Sizes::FONT_WEIGHT_BOLD);
	propertiesTitle_->setParent(properties_);
	titleRow->addWidget(propertiesTitle_, 1);
	propertiesLayout_->addLayout(titleRow);

	propertiesHint_ = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.NoSelection")),
				     properties_);
	propertiesHint_->setStyleSheet(UIStyles::dimLabelStyle());
	propertiesHint_->setWordWrap(true);
	propertiesLayout_->addWidget(propertiesHint_);

	propertiesBody_ = new QWidget(properties_);
	auto *body = new QVBoxLayout(propertiesBody_);
	body->setContentsMargins(0, 0, 0, 0);
	body->setSpacing(S(8));
	propertiesLayout_->addWidget(propertiesBody_);

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
	auto *sizeLabel = new QLabel(QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Panel.SpacerSize")), sizeRow_);
	sizeLabel->setStyleSheet(UIStyles::labelStyle());
	sizeLayout->addWidget(sizeLabel, 1);
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

	// 5. Footer.
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

	// Compact enough to sit beside a vertical toolbar without covering it.
	setMinimumWidth(S(260));
	resize(S(280), S(470));
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
		propertiesIcon_->setPixmap(icon.isNull() ? QPixmap() : icon.pixmap(QSize(S(18), S(18))));
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
	if (spacer) {
		QSignalBlocker blocker(sizeSpin_);
		sizeSpin_->setValue(spacer->size);
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
