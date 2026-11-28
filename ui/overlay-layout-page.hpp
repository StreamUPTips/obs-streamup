#pragma once

// The Layout page of the Overlay Controls window: a live preview of one of the
// overlay's scenes with its parts outlined. Drag a part to move it (Shift keeps
// it on one axis), use the arrow keys to nudge, or type exact values below.

#include "../core/overlay-controls.hpp"

#include <QJsonObject>
#include <QWidget>

#include <obs.h>

#include <functional>

class QComboBox;
class QListWidget;
class QLabel;
class QCheckBox;
class QPushButton;

namespace StreamUP {
namespace OverlayUI {

class ScenePreview;
class PanelDoubleSpin;
class PanelSlider;

class LayoutPage : public QWidget {
public:
	LayoutPage(const QString &overlayId, const OverlayControls::OverlayInfo &info, QWidget *parent = nullptr);
	~LayoutPage() override;

	// New values from the window (another page, a preset, obs-websocket).
	void refresh(const OverlayControls::OverlayInfo &info);

	std::function<void(const QString &)> onStatus;

protected:
	void showEvent(QShowEvent *e) override;
	void hideEvent(QHideEvent *e) override;

private:
	void fillScenes();
	void fillParts();
	void selectPart(const QString &id);
	void showSelected();
	void write(const QJsonObject &partial);
	bool flushWrite();
	void showScene(bool show);
	const OverlayControls::ControlInfo *part(const QString &id) const;
	QList<const OverlayControls::ControlInfo *> partsInScene(const QString &scene) const;

	QString m_overlay;
	OverlayControls::OverlayInfo m_info;
	QString m_scene;
	QString m_selected;
	bool m_updating = false;

	QComboBox *m_scenes = nullptr;
	QListWidget *m_parts = nullptr;
	ScenePreview *m_preview = nullptr;
	QLabel *m_partTitle = nullptr;
	PanelDoubleSpin *m_x = nullptr;
	PanelDoubleSpin *m_y = nullptr;
	PanelSlider *m_scale = nullptr;
	QLabel *m_scaleValue = nullptr;
	QCheckBox *m_visible = nullptr;
	QPushButton *m_reset = nullptr;
	QWidget *m_editor = nullptr;

	// The previewed scene, held "showing" for as long as it is on screen
	// rather than toggled every frame (each toggle fires show/hide on every
	// source in it).
	obs_source_t *m_shown = nullptr;

	// Drag, nudge, spin and scale writes, throttled like the controls page.
	QJsonObject m_pending;
	class QTimer *m_flushTimer = nullptr;
};

} // namespace OverlayUI
} // namespace StreamUP
