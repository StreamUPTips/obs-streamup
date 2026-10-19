#pragma once

// Widgets that carry the class names OBS's own audio mixer uses.
//
// OBS themes style the native mixer with Qt stylesheet *type* selectors
// (VolumeControl, VolumeMeter, VolumeName, AudioMixer) plus object names and
// "class" properties. A type selector matches a widget whose class, or any
// base class, has exactly that name, and a class inside a namespace never
// does. So these few classes live in the global namespace with OBS's names,
// and the StreamUP Mixer builds its strips from them in the same arrangement
// as OBS's VolumeControl. Every theme, Yami and its variants, System,
// StreamUP or a third party one, then styles them exactly as it styles the
// native mixer, with nothing hard coded here.
//
// They are separate types from OBS's (OBS's are internal to the frontend and
// are not exported), and they are kept out of the plugin's exported symbols so
// they can never be confused with them on any platform. Nothing in OBS looks
// these names up except the accessibility factory for "VolumeSlider", which
// is why the slider below has a different name.
//
// VolumeMeter, VolumeName and MixerSlider are ports of OBS's VolumeMeter,
// VolumeName, VolumeSlider and AbsoluteSlider (frontend/components, GPL-2.0,
// by the OBS Project contributors and Taylor Giampaolo), trimmed to what the
// dock needs and moved onto the public libobs and frontend APIs.

#include <obs.h>

#include <QAbstractButton>
#include <QColor>
#include <QFrame>
#include <QMutex>
#include <QPixmap>
#include <QPointer>
#include <QSlider>
#include <QString>
#include <QWidget>

class QLabel;

#if defined(__GNUC__) || defined(__clang__)
#define STREAMUP_MIXER_LOCAL __attribute__((visibility("hidden")))
#else
#define STREAMUP_MIXER_LOCAL
#endif

// Matches the native fader resolution.
#define STREAMUP_FADER_PRECISION 4096.0

// The dock body. Themes scope the mixer toolbar as "AudioMixer QToolBar".
class STREAMUP_MIXER_LOCAL AudioMixer : public QFrame {
	Q_OBJECT

public:
	explicit AudioMixer(QWidget *parent = nullptr) : QFrame(parent) {}
};

// One channel strip. The StreamUP strip derives from this for its type name.
class STREAMUP_MIXER_LOCAL VolumeControl : public QFrame {
	Q_OBJECT

public:
	explicit VolumeControl(QWidget *parent = nullptr) : QFrame(parent) {}
};

class STREAMUP_MIXER_LOCAL VolumeMeter : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QColor backgroundNominalColor READ getBackgroundNominalColor WRITE setBackgroundNominalColor
			   DESIGNABLE true)
	Q_PROPERTY(QColor backgroundWarningColor READ getBackgroundWarningColor WRITE setBackgroundWarningColor
			   DESIGNABLE true)
	Q_PROPERTY(
		QColor backgroundErrorColor READ getBackgroundErrorColor WRITE setBackgroundErrorColor DESIGNABLE true)
	Q_PROPERTY(QColor foregroundNominalColor READ getForegroundNominalColor WRITE setForegroundNominalColor
			   DESIGNABLE true)
	Q_PROPERTY(QColor foregroundWarningColor READ getForegroundWarningColor WRITE setForegroundWarningColor
			   DESIGNABLE true)
	Q_PROPERTY(
		QColor foregroundErrorColor READ getForegroundErrorColor WRITE setForegroundErrorColor DESIGNABLE true)
	Q_PROPERTY(QColor backgroundNominalColorDisabled READ getBackgroundNominalColorDisabled WRITE
			   setBackgroundNominalColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor backgroundWarningColorDisabled READ getBackgroundWarningColorDisabled WRITE
			   setBackgroundWarningColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor backgroundErrorColorDisabled READ getBackgroundErrorColorDisabled WRITE
			   setBackgroundErrorColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor foregroundNominalColorDisabled READ getForegroundNominalColorDisabled WRITE
			   setForegroundNominalColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor foregroundWarningColorDisabled READ getForegroundWarningColorDisabled WRITE
			   setForegroundWarningColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor foregroundErrorColorDisabled READ getForegroundErrorColorDisabled WRITE
			   setForegroundErrorColorDisabled DESIGNABLE true)
	Q_PROPERTY(QColor magnitudeColor READ getMagnitudeColor WRITE setMagnitudeColor DESIGNABLE true)
	Q_PROPERTY(QColor majorTickColor READ getMajorTickColor WRITE setMajorTickColor DESIGNABLE true)
	Q_PROPERTY(QColor minorTickColor READ getMinorTickColor WRITE setMinorTickColor DESIGNABLE true)

public:
	// The meter does not own the volmeter: the strip creates it, feeds the
	// levels in and destroys it. The meter only asks it for its channel count.
	explicit VolumeMeter(obs_volmeter_t *volmeter, QWidget *parent = nullptr);

	void setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
		       const float inputPeak[MAX_AUDIO_CHANNELS]);
	void setVertical(bool vertical);
	void setUseDisabledColors(bool enable);
	void setMuted(bool mute);
	void setPeakDecayRate(qreal v) { peakDecayRate = v; }
	void setPeakMeterType(enum obs_peak_meter_type type);
	void refreshStyle();
	QRect getBarRect() const;

	QColor getBackgroundNominalColor() const { return p_backgroundNominalColor; }
	void setBackgroundNominalColor(QColor c);
	QColor getBackgroundWarningColor() const { return p_backgroundWarningColor; }
	void setBackgroundWarningColor(QColor c);
	QColor getBackgroundErrorColor() const { return p_backgroundErrorColor; }
	void setBackgroundErrorColor(QColor c);
	QColor getForegroundNominalColor() const { return p_foregroundNominalColor; }
	void setForegroundNominalColor(QColor c);
	QColor getForegroundWarningColor() const { return p_foregroundWarningColor; }
	void setForegroundWarningColor(QColor c);
	QColor getForegroundErrorColor() const { return p_foregroundErrorColor; }
	void setForegroundErrorColor(QColor c);
	QColor getBackgroundNominalColorDisabled() const { return backgroundNominalColorDisabled; }
	void setBackgroundNominalColorDisabled(QColor c) { backgroundNominalColorDisabled = c; }
	QColor getBackgroundWarningColorDisabled() const { return backgroundWarningColorDisabled; }
	void setBackgroundWarningColorDisabled(QColor c) { backgroundWarningColorDisabled = c; }
	QColor getBackgroundErrorColorDisabled() const { return backgroundErrorColorDisabled; }
	void setBackgroundErrorColorDisabled(QColor c) { backgroundErrorColorDisabled = c; }
	QColor getForegroundNominalColorDisabled() const { return foregroundNominalColorDisabled; }
	void setForegroundNominalColorDisabled(QColor c) { foregroundNominalColorDisabled = c; }
	QColor getForegroundWarningColorDisabled() const { return foregroundWarningColorDisabled; }
	void setForegroundWarningColorDisabled(QColor c) { foregroundWarningColorDisabled = c; }
	QColor getForegroundErrorColorDisabled() const { return foregroundErrorColorDisabled; }
	void setForegroundErrorColorDisabled(QColor c) { foregroundErrorColorDisabled = c; }
	QColor getMagnitudeColor() const { return magnitudeColor; }
	void setMagnitudeColor(QColor c) { magnitudeColor = c; }
	QColor getMajorTickColor() const { return majorTickColor; }
	void setMajorTickColor(QColor c) { majorTickColor = c; }
	QColor getMinorTickColor() const { return minorTickColor; }
	void setMinorTickColor(QColor c) { minorTickColor = c; }

	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	bool needLayoutChange();
	void resetLevels();
	void doLayout();
	bool detectIdle(uint64_t ts);
	void calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw = 0.0);
	void calculateBallisticsForChannel(int channelNr, uint64_t ts, qreal timeSinceLastRedraw);
	QColor getPeakColor(float peakHold);
	void paintHTicks(QPainter &painter, int x, int y, int width);
	void paintVTicks(QPainter &painter, int x, int y, int height);
	void updateTickLabelTokenSize();
	void updateBackgroundCache(bool force = false);

	QMutex dataMutex;
	uint64_t currentLastUpdateTime{0};
	float currentMagnitude[MAX_AUDIO_CHANNELS];
	float currentPeak[MAX_AUDIO_CHANNELS];
	float currentInputPeak[MAX_AUDIO_CHANNELS];

	obs_volmeter_t *volmeter = nullptr;
	int displayNrAudioChannels{0};
	float displayMagnitude[MAX_AUDIO_CHANNELS];
	float displayPeak[MAX_AUDIO_CHANNELS];
	float displayPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];
	float displayInputPeakHold[MAX_AUDIO_CHANNELS];
	uint64_t displayInputPeakHoldLastUpdateTime[MAX_AUDIO_CHANNELS];

	QPixmap backgroundCache;
	QSize tickTextTokenRect;

	QColor backgroundNominalColor, backgroundWarningColor, backgroundErrorColor;
	QColor foregroundNominalColor, foregroundWarningColor, foregroundErrorColor;
	QColor p_backgroundNominalColor, p_backgroundWarningColor, p_backgroundErrorColor;
	QColor p_foregroundNominalColor, p_foregroundWarningColor, p_foregroundErrorColor;
	QColor backgroundNominalColorDisabled, backgroundWarningColorDisabled, backgroundErrorColorDisabled;
	QColor foregroundNominalColorDisabled, foregroundWarningColorDisabled, foregroundErrorColorDisabled;
	QColor clipColor, magnitudeColor, majorTickColor, minorTickColor;

	int meterThickness = 3;
	qreal minimumLevel = -60.0;
	qreal warningLevel = -20.0;
	qreal errorLevel = -9.0;
	qreal clipLevel = 0.0;
	qreal minimumInputLevel = -50.0;
	qreal peakDecayRate = 11.76;
	qreal magnitudeIntegrationTime = 0.3;
	qreal peakHoldDuration = 20.0;
	qreal inputPeakHoldDuration = 1.0;

	uint64_t lastRedrawTime{0};
	int channels{0};
	bool clipping{false};
	bool vertical{false};
	bool muted{false};
	bool useDisabledColors{false};
};

// The source name, as a flat button the theme styles like the native one.
class STREAMUP_MIXER_LOCAL VolumeName : public QAbstractButton {
	Q_OBJECT

public:
	explicit VolumeName(QWidget *parent = nullptr);

	void setFullText(const QString &text);
	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void updateLabelText();

	QPointer<QLabel> label;
	int indicatorWidth = 0;
	QString fullText;
};

namespace StreamUP {
namespace Mixer {

// Mirrors OBS's VolumeSlider: jumps straight to the clicked position, ignores
// the wheel unless focused, and draws dB ticks behind the groove. Named
// differently on purpose (see the note at the top of this file).
class MixerSlider : public QSlider {
	Q_OBJECT

public:
	MixerSlider(obs_fader_t *fader, Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	int posToRangeValue(QMouseEvent *event);

	obs_fader_t *m_fader = nullptr;
	bool m_dragging = false;
};

// The same "class" helpers OBS's widgets use (idian), so state classes such
// as "checked" and "hover" reach the theme the same way.
namespace StyleClass {
// Each returns true when the widget's classes actually changed. A widget
// that has not been polished yet is left alone: its first polish (when it is
// first shown) applies every rule anyway, and polishing it early is what made
// building a strip expensive.
bool add(QWidget *widget, const QString &name);
bool remove(QWidget *widget, const QString &name);
bool toggle(QWidget *widget, const QString &name, bool on);
void repolish(QWidget *widget);
// Tints a button's icon to its palette's button text colour, as idian does.
void tintIcon(QAbstractButton *button);
// Keeps "hover", "focus", "disabled" and "checked" classes in step with the
// widget's state, and re-tints its icon when they change.
void installStateFilter(QWidget *widget);
} // namespace StyleClass

} // namespace Mixer
} // namespace StreamUP
