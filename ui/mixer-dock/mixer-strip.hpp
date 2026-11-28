#pragma once

// One channel in the StreamUP Mixer.
//
// Built to match OBS's own mixer channel (VolumeControl) piece for piece: the
// same widget tree, object names, "class" properties and state classes, on top
// of widgets carrying OBS's class names (mixer-native-widgets.hpp). The active
// theme therefore styles it exactly as it styles the native mixer.
//
// The strip holds a strong reference to its source for as long as it exists,
// so the source's signal handler is always alive when the strip disconnects
// from it. The dock deletes strips as soon as a source is removed and before
// OBS clears a collection, so the reference never keeps a source alive past
// the point OBS expects it gone.

#include "mixer-native-widgets.hpp"

#include <obs.h>

#include <QPoint>
#include <QPushButton>
#include <QString>

class QLabel;

namespace StreamUP {
namespace Mixer {

class MixerStrip : public ::VolumeControl {
	Q_OBJECT

public:
	MixerStrip(obs_source_t *source, bool vertical, QWidget *parent = nullptr);
	~MixerStrip() override;

	const QString &uuid() const { return m_uuid; }
	obs_source_t *source() const { return m_source; }

	// Re-read everything the theme or OBS's mixer flags can change.
	void refreshState();
	void setPreviewed(bool previewed);

	// The mixer status OBS's own mixer works with, as of the last update.
	bool isActive() const { return m_active; }
	bool isHiddenInMixer() const { return m_hidden; }
	bool isPinnedInMixer() const { return m_pinned; }
	bool isGlobal() const { return m_global; }
	bool isVolumeLocked() const { return m_locked; }
	bool isPreviewed() const { return m_previewed; }

	// Profile meter settings (decay rate, peak meter type) and the
	// accessibility colour overrides, re-read when OBS reports a change.
	void applyMeterSettings();
	void refreshMeterColours();

signals:
	void dragRequested(StreamUP::Mixer::MixerStrip *strip);
	void menuRequested(StreamUP::Mixer::MixerStrip *strip, const QPoint &globalPos);
	// Active, hidden, pinned, global or previewed changed: whether the strip
	// shows, and where, may be different now.
	void mixerStateChanged(StreamUP::Mixer::MixerStrip *strip);

protected:
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	static void onFaderChanged(void *data, float db);
	static void onVolmeterUpdated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				      const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS]);
	static void onSourceSignal(void *data, calldata_t *cd);
	static void onMonitoringSignal(void *data, calldata_t *cd);
	static void onRenameSignal(void *data, calldata_t *cd);

	void buildVertical();
	void buildHorizontal();
	void changeVolume();
	void updateText();
	void processMixerState(int knownMonitoringType = -1);
	void updateCategoryLabel();
	void updateAccessibleNames();
	void handleMuteButton(bool mute);
	void handleMonitorButton(bool enable);
	bool pressStartsDrag(QMouseEvent *event);
	bool moveStartsDrag(QMouseEvent *event);

	obs_source_t *m_source = nullptr;
	obs_fader_t *m_fader = nullptr;
	obs_volmeter_t *m_volmeter = nullptr;
	QString m_uuid;
	bool m_vertical = true;
	bool m_previewed = false;
	bool m_active = false;
	bool m_hidden = false;
	bool m_pinned = false;
	bool m_global = false;
	bool m_locked = false;
	bool m_unassigned = false;
	bool m_stateKnown = false;
	// The button look last applied (mute, monitor and warning bits), so an
	// update that changes nothing restyles nothing.
	int m_buttonState = -1;

	QLabel *m_category = nullptr;
	::VolumeName *m_name = nullptr;
	QPushButton *m_mute = nullptr;
	QPushButton *m_monitor = nullptr;
	QLabel *m_volLabel = nullptr;
	MixerSlider *m_slider = nullptr;
	::VolumeMeter *m_meter = nullptr;

	QPoint m_pressPos;
	bool m_pressed = false;
};

} // namespace Mixer
} // namespace StreamUP
