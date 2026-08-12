#include "streamup-toolbar-builder.hpp"
#include "ui-helpers.hpp"
#include "obs-hotkey-manager.hpp"

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

	// Pinned along, filling across. Both axes are always written.
	if (axis.vertical()) {
		spacer->setFixedHeight(size);
	} else {
		spacer->setFixedWidth(size);
	}
	spacer->setSizePolicy(axis.policy(QSizePolicy::Fixed, QSizePolicy::Expanding));
}

QWidget *createSpacer(const QString &id, int size, const ToolbarGeom::Axis &axis, QWidget *parent)
{
	QWidget *spacer = new QWidget(parent);
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
	default:
		return QString();
	}
}

} // namespace ToolbarBuild
} // namespace StreamUP
