#pragma once

// The readouts OBS shows in its own status bar, as toolbar items.
//
// None of this reads OBS's status bar widget. That widget is private UI in the
// main window, it can be renamed or restructured in any OBS release, and it
// would break silently when it was. Everything here is derived from public
// frontend events and public counters instead, which means the wording is ours
// and will not be character-identical to OBS's. That is a deliberate trade and
// the documentation says so.
//
// One Monitor serves every status item on the bar: one CPU query object, one
// timer, one sample of each counter per tick. Items are views onto it.

#include <QElapsedTimer>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

namespace StreamUP {
namespace ToolbarStatus {

enum class Kind {
	Cpu,
	Fps,
	FramesDropped,
	RecordTime,
	StreamTime,
	StreamBitrate,
	RecordBitrate,
	Message
};

// Kinds are stored by name rather than by index, so the enum can be reordered
// without rewriting anyone's saved toolbar.
QString kindKey(Kind kind);
bool kindFromKey(const QString &key, Kind &out);
QStringList allKindKeys();

// Localised name for the palette, the editor and the item's own label.
QString kindDisplayName(Kind kind);

// Whether this kind offers the "show hours under an hour" setting.
bool kindIsDuration(Kind kind);

// Samples everything the status items show, once a second, for all of them.
class Monitor : public QObject {
	Q_OBJECT

public:
	static Monitor &instance();

	// Items call these so the timer and the CPU query only exist while
	// something is actually on the bar to show them.
	void addObserver();
	void removeObserver();

	// The current reading. compact is the side-docked form, which has to fit
	// a bar only as wide as a button.
	QString text(Kind kind, bool compact, bool showLabel, bool showHours) const;

	// The widest reading this kind can produce, used to pin the item's width.
	// Without this every item reflows the whole run once a second and the bar
	// visibly jitters while you stream.
	QString widestText(Kind kind, bool compact, bool showLabel, bool showHours) const;

	// True when the value is one OBS would colour red. The item turns this
	// into a Qt property for the theme rather than painting a colour itself.
	bool isAlerting(Kind kind) const;

signals:
	void updated();

private:
	Monitor();
	~Monitor() override;

	void tick();
	void syncToRunningOutputs();
	void handleFrontendEvent(enum obs_frontend_event event);
	static void OnFrontendEvent(enum obs_frontend_event event, void *data);

	QString durationText(qint64 ms, bool showHours) const;
	QString bitrateText(double kbps, bool compact) const;

	// Elapsed milliseconds for an output that may have been paused.
	struct Elapsed {
		bool running = false;
		bool paused = false;
		qint64 accumulatedMs = 0;
		QElapsedTimer since;

		void start();
		void stop();
		void pause();
		void resume();
		qint64 ms() const;
	};

	// A rate needs two samples, so each output carries its own last reading.
	struct RateSample {
		uint64_t lastBytes = 0;
		qint64 lastAtMs = 0;
		double kbps = 0.0;
		bool primed = false;

		void reset();
		void feed(uint64_t bytes, qint64 nowMs);
	};

	// Messages are an enum, not a string, so the compact form is a real
	// shorter wording rather than an elided sentence.
	enum class MessageId {
		None,
		RecordingStarted,
		RecordingStopped,
		RecordingPaused,
		StreamingStarted,
		StreamingStopped,
		EncodingOverloaded
	};

	void setMessage(MessageId id);
	QString messageText(MessageId id, bool compact) const;

	int observers_ = 0;
	QTimer timer_;
	os_cpu_usage_info_t *cpuInfo_ = nullptr;
	bool eventCallbackAdded_ = false;

	double cpuPercent_ = 0.0;
	double fps_ = 0.0;
	uint32_t laggedFrames_ = 0;
	uint32_t totalFrames_ = 0;
	uint32_t skippedFrames_ = 0;
	uint32_t encodedFrames_ = 0;
	// The window the overload warning is judged over, so one bad second on a
	// scene change does not light it up.
	uint32_t skippedAtWindowStart_ = 0;
	uint32_t encodedAtWindowStart_ = 0;
	int windowTicks_ = 0;
	bool overloaded_ = false;

	Elapsed recordTime_;
	Elapsed streamTime_;
	RateSample streamRate_;
	RateSample recordRate_;

	MessageId message_ = MessageId::None;
	QElapsedTimer messageAge_;
};

// One readout on the toolbar. Subscribes to the Monitor, keeps a pinned width,
// and goes quiet while the toolbar is being edited.
class StatusWidget : public QLabel {
	Q_OBJECT

public:
	StatusWidget(Kind kind, bool compact, bool showLabel, bool showHours, QWidget *parent = nullptr);
	~StatusWidget() override;

	void refresh();

private:
	void applyFixedWidth();

	Kind kind_;
	bool compact_;
	bool showLabel_;
	bool showHours_;
	bool alerting_ = false;
};

} // namespace ToolbarStatus
} // namespace StreamUP
