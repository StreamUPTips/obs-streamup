#include "streamup-toolbar-builder.hpp"
#include "ui-helpers.hpp"
#include "obs-hotkey-manager.hpp"
#include "streamup-toolbar-status.hpp"

#include <obs-module.h>

#include <QBoxLayout>
#include <QFile>

namespace StreamUP {
namespace ToolbarBuild {

const char *const kSpacerSizeProperty = "streamupSpacerSize";
const char *const kItemIdProperty = "streamupItemId";

namespace {

// A separator is a hairline: one pixel along the flow, full width across.
constexpr int kSeparatorThickness = 1;

bool isStreamUPSettingsItem(const std::shared_ptr<ToolbarConfig::ToolbarItem> &item)
{
	if (!item || item->type != ToolbarConfig::ItemType::Button)
		return false;
	auto button = std::dynamic_pointer_cast<ToolbarConfig::ButtonItem>(item);
	return button && button->buttonType == "streamup_settings";
}

// Furniture fills the toolbar across the flow. Buttons keep their natural size
// and sit centred. This distinction is made here and nowhere else, so the two
// cannot drift apart.
bool fillsAcross(ToolbarConfig::ItemType type)
{
	return type == ToolbarConfig::ItemType::Separator || type == ToolbarConfig::ItemType::CustomSpacer;
}

// A spacer asks for its configured length and settles for less.
//
// A plain QWidget has no size hint, so a layout has nothing to ask for and a
// policy of Maximum would resolve its preferred length to zero. The hint is the
// whole mechanism: the layout hands the spacer its full size while there is
// room, and takes the difference back out of it when there is not, before it
// starts clipping anything that cannot shrink.
class SpacerWidget : public QWidget {
public:
	SpacerWidget(int size, const ToolbarGeom::Axis &axis, QWidget *parent)
		: QWidget(parent), size_(size), axis_(axis)
	{
	}

	void setSpacerSize(int size, const ToolbarGeom::Axis &axis)
	{
		size_ = size;
		axis_ = axis;
		updateGeometry();
	}

	QSize sizeHint() const override { return axis_.size(size_, 0); }
	QSize minimumSizeHint() const override { return QSize(0, 0); }

private:
	int size_ = 0;
	ToolbarGeom::Axis axis_{false};
};

} // namespace

void applySpacerSize(QWidget *spacer, int size, const ToolbarGeom::Axis &axis)
{
	if (!spacer)
		return;

	spacer->setProperty(kSpacerSizeProperty, size);

	// Clear whatever a previous orientation pinned, or the old axis survives
	// and the spacer comes out square.
	spacer->setMinimumSize(0, 0);
	spacer->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

	// The requested length along the flow, and no more. What it is NOT is a
	// floor: a spacer that could not give ground was the only thing standing
	// between a bar that had run out of room and one that clipped its last
	// item. A readout that grows, a narrowed window or a bigger font now takes
	// the space it needs out of the empty gap, which is what the gap is for,
	// and the items keep their size.
	//
	// Filling across is unchanged, and is the rule that stops a spacer
	// vanishing on a side-docked bar.
	if (axis.vertical()) {
		spacer->setMaximumHeight(size);
	} else {
		spacer->setMaximumWidth(size);
	}
	// Preferred, not Fixed and not Maximum. Fixed cannot give ground, which is
	// the bug. Maximum treats the hint as a ceiling and gave the spacer nothing
	// at all here. Preferred asks for the hint, keeps it while there is room,
	// and shrinks toward zero when there is not. Growing past the hint is
	// prevented by the cap set above, and by the alignment stretches, which are
	// Expanding and take any genuine slack first.
	spacer->setSizePolicy(axis.policy(QSizePolicy::Preferred, QSizePolicy::Expanding));

	// The hint is what the layout actually asks for, so it has to move with the
	// size. Without this a spacer resized mid-drag keeps asking for its old
	// length and only the cap changes.
	if (auto *sized = dynamic_cast<SpacerWidget *>(spacer))
		sized->setSpacerSize(size, axis);
}

QWidget *createSpacer(const QString &id, int size, const ToolbarGeom::Axis &axis, QWidget *parent)
{
	QWidget *spacer = new SpacerWidget(size, axis, parent);
	spacer->setProperty("class", "toolbar-spacer");
	spacer->setObjectName(id);
	spacer->setProperty(kItemIdProperty, id);
	applySpacerSize(spacer, size, axis);
	return spacer;
}

QFrame *createSeparator(const QString &id, const ToolbarGeom::Axis &axis, QWidget *parent)
{
	QFrame *separator = new QFrame(parent);
	separator->setProperty("class", "toolbar-separator");
	separator->setObjectName(id);
	separator->setProperty(kItemIdProperty, id);
	separator->setFrameShape(QFrame::NoFrame);

	// One pixel along, filling across. The old code pinned the thickness and
	// left a comment claiming the other axis would stretch, but it was added
	// to the layout with a centring alignment flag, so it took its size hint
	// instead, which is zero. That is why separators have never been visible
	// on a side-docked toolbar.
	if (axis.vertical()) {
		separator->setFixedHeight(kSeparatorThickness);
	} else {
		separator->setFixedWidth(kSeparatorThickness);
	}
	separator->setSizePolicy(axis.policy(QSizePolicy::Fixed, QSizePolicy::Expanding));
	return separator;
}

Result build(const ToolbarConfig::ToolbarConfiguration &config, const Options &opts, QWidget *parent,
	     const WidgetFactory &makeWidgets)
{
	Result result;
	const ToolbarGeom::Axis &axis = opts.axis;

	result.container = new QWidget(parent);
	result.container->setObjectName("StreamUPToolbarCentralWidget");

	result.layout = new QBoxLayout(axis.layoutDirection(), result.container);
	result.layout->setContentsMargins(0, 0, 0, 0);
	result.layout->setSpacing(opts.spacing);

	// Furniture must not be given an alignment flag: alignment on the cross
	// axis collapses a widget to its size hint, which is the bug this rewrite
	// exists to kill. Buttons do want centring.
	const auto place = [&result, &axis](const QString &id, QWidget *w, ToolbarConfig::ItemType type) {
		if (!w)
			return;
		if (fillsAcross(type)) {
			result.layout->addWidget(w);
		} else {
			// Everything that is not furniture is pinned at its natural size.
			//
			// A QToolButton will shrink without being asked, so when the run
			// was longer than the bar Qt squeezed the buttons rather than the
			// empty gap: icons collapsed to slivers you could not aim at, and
			// the StreamUP button, which is placed last, was pushed off the end
			// entirely. Pinning them leaves the spacers as the only thing on the
			// bar that can give up space, which is the whole point of a spacer.
			//
			// This matters most in the editor, where a bar you cannot hit is a
			// bar you cannot rearrange or add to.
			//
			// Minimum, not Fixed. Fixed pins the widget at the size hint it has
			// when the run is laid out, and the run is built before the theme
			// has styled the buttons, so every one of them locked at its
			// unstyled hint and came out a hairline. Minimum says the hint is a
			// floor: it can still grow into its real styled size, it just
			// cannot be squeezed below it.
			//
			// The policy is not enough on its own. The OBS theme sets
			// min-width: 4px on toolbar buttons, and a stylesheet minimum wins
			// over one implied by a size policy, so Qt was still free to squash
			// a button to 4px and did. pinNaturalMinimums() below writes an
			// explicit minimum once the theme has had its say.
			w->setSizePolicy(axis.policy(QSizePolicy::Minimum, QSizePolicy::Preferred));
			result.layout->addWidget(w, 0, axis.vertical() ? Qt::AlignHCenter : Qt::AlignVCenter);
		}
		result.placed.append({id, w});
	};

	const auto items = config.getFlattenedItems();

	// Leading stretch pushes the run off the near edge for centre and end
	// alignment. Start alignment leaves it where it is.
	if (opts.alignment != SettingsManager::ToolbarAlignment::Start)
		result.layout->addStretch();

	// The StreamUP button is pinned to the far end, so it is held back until
	// after the trailing stretch.
	QList<std::shared_ptr<ToolbarConfig::ToolbarItem>> deferred;

	const auto placeFromFactory = [&](const std::shared_ptr<ToolbarConfig::ToolbarItem> &item) {
		if (!makeWidgets)
			return;
		for (QWidget *w : makeWidgets(item)) {
			if (!w)
				continue;
			w->setProperty(kItemIdProperty, item->id);
			place(item->id, w, item->type);
		}
	};

	for (const auto &item : items) {
		if (!item || !item->visible)
			continue;
		if (isStreamUPSettingsItem(item)) {
			deferred.append(item);
			continue;
		}

		switch (item->type) {
		case ToolbarConfig::ItemType::Separator:
			place(item->id, createSeparator(item->id, axis, result.container), item->type);
			break;
		case ToolbarConfig::ItemType::CustomSpacer: {
			auto spacerItem = std::static_pointer_cast<ToolbarConfig::CustomSpacerItem>(item);
			place(item->id, createSpacer(item->id, spacerItem->size, axis, result.container), item->type);
			break;
		}
		default:
			placeFromFactory(item);
			break;
		}
	}

	// Trailing stretch holds the run against the near edge. End alignment is
	// already against the far end and does not want one.
	if (opts.alignment != SettingsManager::ToolbarAlignment::End)
		result.layout->addStretch();

	for (const auto &item : deferred)
		placeFromFactory(item);

	return result;
}

void pinNaturalMinimums(const Result &result, const ToolbarGeom::Axis &axis)
{
	// Called once the theme has styled the run, because a button's size hint is
	// next to nothing before that and pinning early is how you get a bar of
	// hairlines.
	//
	// Why this exists at all: the theme sets min-width: 4px on toolbar buttons.
	// A stylesheet minimum outranks the one a size policy implies, so when the
	// run was longer than the bar Qt happily squashed every button to 4px and
	// took only part of the slack out of the spacers. An explicit minimum is
	// the one thing the stylesheet does not override, so the spacers are left
	// as the only thing that can give.
	for (const auto &entry : result.placed) {
		QWidget *w = entry.second;
		if (!w)
			continue;

		// Furniture is what we want to be squeezable, so it is skipped: a
		// spacer giving up its space is the entire mechanism.
		if (w->property(kSpacerSizeProperty).isValid())
			continue;
		if (qobject_cast<QFrame *>(w) && w->property("class").toString() == QLatin1String("toolbar-separator"))
			continue;

		const QSize hint = w->sizeHint();
		if (!hint.isValid())
			continue;

		if (axis.vertical())
			w->setMinimumHeight(hint.height());
		else
			w->setMinimumWidth(hint.width());
	}
}

QIcon iconForItem(const std::shared_ptr<ToolbarConfig::ToolbarItem> &item)
{
	if (!item)
		return QIcon();

	const auto themed = [](const QString &name) { return QIcon(UIHelpers::GetThemedIconPath(name)); };

	switch (item->type) {
	case ToolbarConfig::ItemType::Button: {
		auto button = std::static_pointer_cast<ToolbarConfig::ButtonItem>(item);
		// The StreamUP logo is a brand mark, not a themed glyph.
		if (button->buttonType == "streamup_settings")
			return QIcon(":images/icons/social/streamup-logo-button.svg");

		QString iconPath = button->iconPath;
		if (iconPath.isEmpty())
			iconPath = ToolbarConfig::ButtonRegistry::getButtonInfo(button->buttonType).defaultIcon;
		if (iconPath.isEmpty())
			iconPath = "settings";
		return themed(iconPath);
	}
	case ToolbarConfig::ItemType::DockButton: {
		auto dock = std::static_pointer_cast<ToolbarConfig::DockButtonItem>(item);
		if (dock->dockButtonType == "toggle_visibility_selected_sources")
			return themed("visible");
		if (!dock->iconPath.isEmpty())
			return themed(dock->iconPath);
		return themed("settings");
	}
	case ToolbarConfig::ItemType::WebSocketButton: {
		auto ws = std::static_pointer_cast<ToolbarConfig::WebSocketButtonItem>(item);
		if (ws->useCustomIcon && !ws->customIconPath.isEmpty())
			return QIcon(ws->customIconPath);
		if (!ws->iconPath.isEmpty()) {
			if (QFile::exists(ws->iconPath))
				return QIcon(ws->iconPath);
			return themed(ws->iconPath);
		}
		return themed("settings");
	}
	case ToolbarConfig::ItemType::HotkeyButton: {
		auto hotkey = std::static_pointer_cast<ToolbarConfig::HotkeyButtonItem>(item);
		if (hotkey->useCustomIcon && !hotkey->customIconPath.isEmpty())
			return QIcon(hotkey->customIconPath);
		if (!hotkey->iconPath.isEmpty()) {
			// An OBS icon arrives as a full path, a StreamUP one as a name.
			if (QFile::exists(hotkey->iconPath))
				return QIcon(hotkey->iconPath);
			return themed(hotkey->iconPath);
		}
		return themed(OBSHotkeyManager::getDefaultHotkeyIcon(hotkey->hotkeyName));
	}
	default:
		return QIcon();
	}
}

QString labelForItem(const std::shared_ptr<ToolbarConfig::ToolbarItem> &item)
{
	if (!item)
		return QString();

	switch (item->type) {
	case ToolbarConfig::ItemType::Button: {
		auto button = std::static_pointer_cast<ToolbarConfig::ButtonItem>(item);
		if (button->buttonType == "streamup_settings")
			return QStringLiteral("StreamUP");
		const auto info = ToolbarConfig::ButtonRegistry::getButtonInfo(button->buttonType);
		return info.displayName.isEmpty() ? button->buttonType : info.displayName;
	}
	case ToolbarConfig::ItemType::DockButton:
		return std::static_pointer_cast<ToolbarConfig::DockButtonItem>(item)->name;
	case ToolbarConfig::ItemType::HotkeyButton:
		return std::static_pointer_cast<ToolbarConfig::HotkeyButtonItem>(item)->displayName;
	case ToolbarConfig::ItemType::WebSocketButton: {
		auto ws = std::static_pointer_cast<ToolbarConfig::WebSocketButtonItem>(item);
		return ws->displayName.isEmpty() ? ws->requestType : ws->displayName;
	}
	case ToolbarConfig::ItemType::Separator:
		return QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Item.Separator"));
	case ToolbarConfig::ItemType::CustomSpacer: {
		auto spacer = std::static_pointer_cast<ToolbarConfig::CustomSpacerItem>(item);
		return QString::fromUtf8(obs_module_text("StreamUP.Toolbar.Item.Spacer")).arg(spacer->size);
	}
	case ToolbarConfig::ItemType::StatusItem: {
		auto status = std::static_pointer_cast<ToolbarConfig::StatusItem>(item);
		ToolbarStatus::Kind kind;
		if (!ToolbarStatus::kindFromKey(status->kind, kind))
			return QString();
		return ToolbarStatus::kindDisplayName(kind);
	}
	default:
		return QString();
	}
}

} // namespace ToolbarBuild
} // namespace StreamUP
