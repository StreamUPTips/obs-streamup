#pragma once

// The All Settings page of the Overlay Controls window: every scene, source and
// filter in the overlay, with every setting OBS knows for each, drawn in the
// StreamUP style from OBS's own property definitions. Nothing is out of reach,
// and each source can be put back as it was installed.

#include "../core/overlay-controls.hpp"

#include <obs.h>

#include <QPointer>
#include <QWidget>

#include <functional>

class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class QScrollArea;
class QGridLayout;

namespace StreamUP {
namespace OverlayUI {

// A settings form for one source or filter, built from obs_source_properties.
// Edits go straight to the source; a property whose change reshapes the list
// (OBS's "modified" callback) rebuilds the form in place.
class PropertiesForm : public QWidget {
public:
	explicit PropertiesForm(obs_source_t *source, QWidget *parent = nullptr);
	~PropertiesForm() override;

	void rebuild();

private:
	void build(QVBoxLayout *into, obs_properties_t *props, int depth);
	void addProperty(QGridLayout *grid, int row, obs_property_t *p, QVBoxLayout *into, int depth);
	void changed(const QString &name);
	obs_data_t *settings() const;

	obs_source_t *m_source = nullptr; // owned reference
	obs_properties_t *m_props = nullptr;
	QVBoxLayout *m_root = nullptr;
	bool m_rebuildQueued = false;
};

class AllSettingsPage : public QWidget {
public:
	AllSettingsPage(const QString &overlayId, const OverlayControls::OverlayInfo &info, QWidget *parent = nullptr);

	// Rebuilds the open source's forms after changes made elsewhere.
	void refresh();

	std::function<void(const QString &)> onStatus;

protected:
	void showEvent(QShowEvent *e) override;

private:
	void fillTree();
	void showItem(QTreeWidgetItem *item);

	QString m_overlay;
	OverlayControls::OverlayInfo m_info;
	QTreeWidget *m_tree = nullptr;
	QScrollArea *m_scroll = nullptr;
	QString m_scene;
	QString m_source;
	qint64 m_itemId = 0;
	bool m_dirty = true;
};

} // namespace OverlayUI
} // namespace StreamUP
