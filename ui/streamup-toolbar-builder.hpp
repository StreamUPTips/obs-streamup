#pragma once

// Turns a toolbar configuration into a laid-out run of widgets.
//
// One function, one set of rules, both orientations. Sizing is expressed in
// axis terms (see streamup-toolbar-geometry.hpp) so nothing here has to know
// whether it is running left to right or top to bottom.
//
// The two rules that used to be broken, stated once, here:
//   - A spacer pins its length along the flow and fills across.
//   - A separator is one pixel thick along the flow and fills across.
// Both were previously pinned on one axis with the other left to a zero size
// hint, which is why they rendered as nothing on a side-docked toolbar.

#include <QBoxLayout>
#include <QFrame>
#include <QIcon>
#include <QList>
#include <QString>
#include <QWidget>
#include <functional>
#include <memory>

#include "streamup-toolbar-config.hpp"
#include "streamup-toolbar-geometry.hpp"
#include "settings-manager.hpp"

namespace StreamUP {
namespace ToolbarBuild {

// The configured length of a spacer, stashed on the widget so a rebuild never
// has to recover it from rendered geometry.
extern const char *const kSpacerSizeProperty;
// The item id a widget was built for. Every widget in a run carries one, so hit
// testing is a lookup rather than a guess at running order.
extern const char *const kItemIdProperty;

struct Options {
	ToolbarGeom::Axis axis{false};
	SettingsManager::ToolbarAlignment alignment = SettingsManager::ToolbarAlignment::Start;
	int spacing = 0;
};

// Supplies the widget(s) for one configured button. Returning more than one
// covers a button that carries a companion, such as record bringing pause.
// Return an empty list to leave the item out.
using WidgetFactory = std::function<QList<QWidget *>(const std::shared_ptr<ToolbarConfig::ToolbarItem> &)>;

struct Result {
	QWidget *container = nullptr;
	QBoxLayout *layout = nullptr;
	// Every laid-out widget, in running order, paired with the item it came
	// from. Editing surfaces walk this rather than re-deriving the order.
	QList<QPair<QString, QWidget *>> placed;
};

Result build(const ToolbarConfig::ToolbarConfiguration &config, const Options &opts, QWidget *parent,
	     const WidgetFactory &makeWidgets);

// Size a spacer: pinned along the flow, filling across.
void applySpacerSize(QWidget *spacer, int size, const ToolbarGeom::Axis &axis);

QWidget *createSpacer(const QString &id, int size, const ToolbarGeom::Axis &axis, QWidget *parent);
QFrame *createSeparator(const QString &id, const ToolbarGeom::Axis &axis, QWidget *parent);

// Shared so nothing can disagree about what an item looks like or is called.
QIcon iconForItem(const std::shared_ptr<ToolbarConfig::ToolbarItem> &item);
QString labelForItem(const std::shared_ptr<ToolbarConfig::ToolbarItem> &item);

} // namespace ToolbarBuild
} // namespace StreamUP
