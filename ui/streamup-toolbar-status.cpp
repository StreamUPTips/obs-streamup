#include "streamup-toolbar-status.hpp"
#include "../streamup.hpp"

#include <obs-module.h>

#include <QDateTime>
#include <QFontMetrics>
#include <QStyle>

namespace StreamUP {
namespace ToolbarStatus {

namespace {

// OBS's status bar runs at 1Hz and that is plenty for all of these.
constexpr int kTickMs = 1000;

// A message stays up for this long and then clears itself, the same way OBS's
// own transient messages do.
constexpr qint64 kMessageHoldMs = 8000;

// Overload is judged over a window rather than a single tick, so one bad second
// during a scene change does not light the warning.
constexpr int kOverloadWindowTicks = 5;
constexpr double kOverloadSkipRatio = 0.05;

QString moduleText(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace

QString kindKey(Kind kind)
{
	switch (kind) {
	case Kind::Cpu:
		return QStringLiteral("cpu");
	case Kind::Fps:
		return QStringLiteral("fps");
	case Kind::FramesDropped:
		return QStringLiteral("frames_dropped");
	case Kind::RecordTime:
		return QStringLiteral("record_time");
	case Kind::StreamTime:
		return QStringLiteral("stream_time");
	case Kind::StreamBitrate:
		return QStringLiteral("stream_bitrate");
	case Kind::RecordBitrate:
		return QStringLiteral("record_bitrate");
	case Kind::Message:
		return QStringLiteral("message");
	}
	return QStringLiteral("cpu");
}

bool kindFromKey(const QString &key, Kind &out)
{
	for (int i = 0; i <= static_cast<int>(Kind::Message); ++i) {
		const Kind kind = static_cast<Kind>(i);
		if (kindKey(kind) != key)
			continue;
		out = kind;
		return true;
	}
	return false;
}

QStringList allKindKeys()
{
	QStringList keys;
	for (int i = 0; i <= static_cast<int>(Kind::Message); ++i)
		keys.append(kindKey(static_cast<Kind>(i)));
	return keys;
}

QString kindDisplayName(Kind kind)
{
	switch (kind) {
	case Kind::Cpu:
		return moduleText("StreamUP.Toolbar.Status.Cpu");
	case Kind::Fps:
		return moduleText("StreamUP.Toolbar.Status.Fps");
	case Kind::FramesDropped:
		return moduleText("StreamUP.Toolbar.Status.FramesDropped");
	case Kind::RecordTime:
		return moduleText("StreamUP.Toolbar.Status.RecordTime");
	case Kind::StreamTime:
		return moduleText("StreamUP.Toolbar.Status.StreamTime");
	case Kind::StreamBitrate:
		return moduleText("StreamUP.Toolbar.Status.StreamBitrate");
	case Kind::RecordBitrate:
		return moduleText("StreamUP.Toolbar.Status.RecordBitrate");
	case Kind::Message:
		return moduleText("StreamUP.Toolbar.Status.Message");
	}
	return QString();
}

bool kindIsDuration(Kind kind)
{
	return kind == Kind::RecordTime || kind == Kind::StreamTime;
}

// ---------------------------------------------------------------------------
// Elapsed

void Monitor::Elapsed::start()
{
	running = true;
	paused = false;
	accumulatedMs = 0;
	since.restart();
}

void Monitor::Elapsed::stop()
{
	running = false;
	paused = false;
	accumulatedMs = 0;
}

void Monitor::Elapsed::pause()
{
	if (!running || paused)
		return;
	accumulatedMs += since.isValid() ? since.elapsed() : 0;
	paused = true;
}

void Monitor::Elapsed::resume()
{
	if (!running || !paused)
		return;
	paused = false;
	since.restart();
}

qint64 Monitor::Elapsed::ms() const
{
	if (!running)
		return 0;
	if (paused)
		return accumulatedMs;
	return accumulatedMs + (since.isValid() ? since.elapsed() : 0);
}

// ---------------------------------------------------------------------------
// RateSample

void Monitor::RateSample::reset()
{
	lastBytes = 0;
	lastAtMs = 0;
	kbps = 0.0;
	primed = false;
}

void Monitor::RateSample::feed(uint64_t bytes, qint64 nowMs)
{
	// The first sample only establishes a baseline. Reporting it as a rate
	// would show the whole stream so far as one second's worth of traffic.
	if (!primed) {
		lastBytes = bytes;
		lastAtMs = nowMs;
		primed = true;
		kbps = 0.0;
		return;
	}

	const qint64 elapsed = nowMs - lastAtMs;
	if (elapsed <= 0)
		return;

	// An output that restarted counts from zero again, so a negative delta is
	// a new output rather than a real reading.
	const uint64_t delta = bytes >= lastBytes ? bytes - lastBytes : 0;
	kbps = (static_cast<double>(delta) * 8.0) / static_cast<double>(elapsed);

	lastBytes = bytes;
	lastAtMs = nowMs;
}

// ---------------------------------------------------------------------------
// Monitor

Monitor &Monitor::instance()
{
	static Monitor monitor;
	return monitor;
}

Monitor::Monitor()
{
	timer_.setInterval(kTickMs);
	connect(&timer_, &QTimer::timeout, this, &Monitor::tick);
}

Monitor::~Monitor()
{
	if (eventCallbackAdded_)
		obs_frontend_remove_event_callback(OnFrontendEvent, this);
	if (cpuInfo_)
		os_cpu_usage_info_destroy(cpuInfo_);
}

void Monitor::addObserver()
{
	if (++observers_ > 1)
		return;

	if (!cpuInfo_)
		cpuInfo_ = os_cpu_usage_info_start();

	// Registering a frontend callback during module load is the supported
	// pattern and is safe. Asking the frontend anything is not: the toolbar is
	// built from inside obs_init_module, where the frontend API does not exist
	// yet. Everything else here waits for OBS to say it has finished loading.
	if (!eventCallbackAdded_) {
		obs_frontend_add_event_callback(OnFrontendEvent, this);
		eventCallbackAdded_ = true;
	}

	timer_.start();

	// A status item created after startup has already missed FINISHED_LOADING,
	// so it catches up here instead of waiting for an event that has been and
	// gone.
	if (ObsFinishedLoading())
		syncToRunningOutputs();
}

void Monitor::removeObserver()
{
	if (observers_ > 0)
		--observers_;
	if (observers_ > 0)
		return;

	timer_.stop();

	// The CPU query is kept: destroying and restarting it loses the baseline
	// it needs, so the first reading after re-adding an item would be wrong.
	streamRate_.reset();
	recordRate_.reset();
}

void Monitor::OnFrontendEvent(enum obs_frontend_event event, void *data)
{
	static_cast<Monitor *>(data)->handleFrontendEvent(event);
}

void Monitor::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		// The first moment it is legal to ask the frontend anything. Items
		// built during module load have been showing their startup values
		// until now.
		syncToRunningOutputs();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		recordTime_.start();
		recordRate_.reset();
		setMessage(MessageId::RecordingStarted);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		recordTime_.stop();
		recordRate_.reset();
		setMessage(MessageId::RecordingStopped);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		recordTime_.pause();
		setMessage(MessageId::RecordingPaused);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		recordTime_.resume();
		setMessage(MessageId::None);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		streamTime_.start();
		streamRate_.reset();
		// The overload window starts fresh with the stream, since skipped
		// frames from before it began say nothing about it.
		windowTicks_ = 0;
		overloaded_ = false;
		setMessage(MessageId::StreamingStarted);
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		streamTime_.stop();
		streamRate_.reset();
		overloaded_ = false;
		setMessage(MessageId::StreamingStopped);
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		// Nothing may touch libobs after this. The Monitor is a static, so
		// its destructor runs during process teardown, by which point
		// obs_frontend_remove_event_callback would be reaching into a
		// shut-down frontend. Everything that talks to OBS is released here
		// instead, while there is still an OBS to release it to. Removing a
		// callback from inside its own dispatch is supported.
		timer_.stop();
		obs_frontend_remove_event_callback(OnFrontendEvent, this);
		eventCallbackAdded_ = false;
		recordTime_.stop();
		streamTime_.stop();
		streamRate_.reset();
		recordRate_.reset();
		break;
	default:
		return;
	}

	emit updated();
}

void Monitor::setMessage(MessageId id)
{
	message_ = id;
	messageAge_.restart();
}

// Adopts whatever OBS is already doing, so an item added part way through a
// recording does not read zero until the next start event, which may never come.
void Monitor::syncToRunningOutputs()
{
	if (obs_frontend_recording_active() && !recordTime_.running) {
		recordTime_.start();
		if (obs_frontend_recording_paused())
			recordTime_.pause();
	}
	if (obs_frontend_streaming_active() && !streamTime_.running)
		streamTime_.start();

	tick();
}

void Monitor::tick()
{
	// The timer cannot fire before the Qt event loop runs, but OBS pumps events
	// during startup, so this is not a guarantee on its own.
	if (!ObsFinishedLoading())
		return;

	const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

	if (cpuInfo_)
		cpuPercent_ = os_cpu_usage_info_query(cpuInfo_);

	fps_ = obs_get_active_fps();
	laggedFrames_ = obs_get_lagged_frames();
	totalFrames_ = obs_get_total_frames();

	if (video_t *video = obs_get_video()) {
		skippedFrames_ = video_output_get_skipped_frames(video);
		encodedFrames_ = video_output_get_total_frames(video);
	}

	if (obs_output_t *output = obs_frontend_get_streaming_output()) {
		streamRate_.feed(obs_output_get_total_bytes(output), nowMs);
		obs_output_release(output);
	} else {
		streamRate_.reset();
	}

	if (obs_output_t *output = obs_frontend_get_recording_output()) {
		recordRate_.feed(obs_output_get_total_bytes(output), nowMs);
		obs_output_release(output);
	} else {
		recordRate_.reset();
	}

	// Encoding overload, judged the way OBS judges it: the share of frames the
	// encoder could not keep up with over a window, not since OBS started.
	if (++windowTicks_ >= kOverloadWindowTicks) {
		const uint32_t skipped = skippedFrames_ >= skippedAtWindowStart_
						 ? skippedFrames_ - skippedAtWindowStart_
						 : 0;
		const uint32_t encoded = encodedFrames_ >= encodedAtWindowStart_
						 ? encodedFrames_ - encodedAtWindowStart_
						 : 0;

		const bool nowOverloaded =
			encoded > 0 && (static_cast<double>(skipped) / static_cast<double>(encoded)) > kOverloadSkipRatio;

		// Only announce the transition. Re-announcing every window would keep
		// resetting the message hold and nothing else could ever be seen.
		if (nowOverloaded && !overloaded_)
			setMessage(MessageId::EncodingOverloaded);

		overloaded_ = nowOverloaded;
		skippedAtWindowStart_ = skippedFrames_;
		encodedAtWindowStart_ = encodedFrames_;
		windowTicks_ = 0;
	}

	if (message_ != MessageId::None && messageAge_.isValid() && messageAge_.elapsed() > kMessageHoldMs)
		message_ = MessageId::None;

	emit updated();
}

QString Monitor::durationText(qint64 ms, bool showHours) const
{
	const qint64 totalSeconds = ms / 1000;
	const qint64 hours = totalSeconds / 3600;
	const qint64 minutes = (totalSeconds % 3600) / 60;
	const qint64 seconds = totalSeconds % 60;

	if (hours > 0 || showHours) {
		return QStringLiteral("%1:%2:%3")
			.arg(hours, 2, 10, QLatin1Char('0'))
			.arg(minutes, 2, 10, QLatin1Char('0'))
			.arg(seconds, 2, 10, QLatin1Char('0'));
	}

	return QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0'));
}

QString Monitor::bitrateText(double kbps, bool compact) const
{
	if (compact)
		return QStringLiteral("%1k").arg(static_cast<int>(kbps));
	return QStringLiteral("%1 kb/s").arg(static_cast<int>(kbps));
}

QString Monitor::messageText(MessageId id, bool compact) const
{
	switch (id) {
	case MessageId::None:
		return QString();
	case MessageId::RecordingStarted:
		return moduleText(compact ? "StreamUP.Toolbar.Status.Msg.RecordingStarted.Short"
				  : "StreamUP.Toolbar.Status.Msg.RecordingStarted");
	case MessageId::RecordingStopped:
		return moduleText(compact ? "StreamUP.Toolbar.Status.Msg.RecordingStopped.Short"
				  : "StreamUP.Toolbar.Status.Msg.RecordingStopped");
	case MessageId::RecordingPaused:
		return moduleText(compact ? "StreamUP.Toolbar.Status.Msg.RecordingPaused.Short"
				  : "StreamUP.Toolbar.Status.Msg.RecordingPaused");
	case MessageId::StreamingStarted:
		return moduleText(compact ? "StreamUP.Toolbar.Status.Msg.StreamingStarted.Short"
				  : "StreamUP.Toolbar.Status.Msg.StreamingStarted");
	case MessageId::StreamingStopped:
		return moduleText(compact ? "StreamUP.Toolbar.Status.Msg.StreamingStopped.Short"
				  : "StreamUP.Toolbar.Status.Msg.StreamingStopped");
	case MessageId::EncodingOverloaded:
		return moduleText(compact ? "StreamUP.Toolbar.Status.Msg.Overloaded.Short"
				  : "StreamUP.Toolbar.Status.Msg.Overloaded");
	}
	return QString();
}

QString Monitor::text(Kind kind, bool compact, bool showLabel, bool showHours) const
{
	QString label;
	QString value;

	switch (kind) {
	case Kind::Cpu:
		label = moduleText("StreamUP.Toolbar.Status.Cpu.Prefix");
		value = QStringLiteral("%1%").arg(cpuPercent_, 0, 'f', compact ? 0 : 1);
		break;
	case Kind::Fps:
		label = moduleText("StreamUP.Toolbar.Status.Fps.Prefix");
		value = compact ? QString::number(fps_, 'f', 0) : QStringLiteral("%1 FPS").arg(fps_, 0, 'f', 2);
		break;
	case Kind::FramesDropped: {
		// Rendering lag, which is what OBS's "missed frames" readout counts.
		const double percent = totalFrames_ > 0
					       ? (static_cast<double>(laggedFrames_) / static_cast<double>(totalFrames_)) * 100.0
					       : 0.0;
		label = moduleText("StreamUP.Toolbar.Status.FramesDropped.Prefix");
		value = compact ? QStringLiteral("%1").arg(laggedFrames_)
				: QStringLiteral("%1 (%2%)").arg(laggedFrames_).arg(percent, 0, 'f', 1);
		break;
	}
	case Kind::RecordTime:
		label = moduleText("StreamUP.Toolbar.Status.RecordTime.Prefix");
		value = durationText(recordTime_.ms(), showHours);
		break;
	case Kind::StreamTime:
		label = moduleText("StreamUP.Toolbar.Status.StreamTime.Prefix");
		value = durationText(streamTime_.ms(), showHours);
		break;
	case Kind::StreamBitrate:
		label = moduleText("StreamUP.Toolbar.Status.StreamBitrate.Prefix");
		value = bitrateText(streamRate_.kbps, compact);
		break;
	case Kind::RecordBitrate:
		label = moduleText("StreamUP.Toolbar.Status.RecordBitrate.Prefix");
		value = bitrateText(recordRate_.kbps, compact);
		break;
	case Kind::Message:
		// A message carries its own wording, so a prefix in front of it would
		// read as "Status: Recording stopped".
		return messageText(message_, compact);
	}

	if (!showLabel || label.isEmpty())
		return value;
	return QStringLiteral("%1 %2").arg(label, value);
}

QString Monitor::widestText(Kind kind, bool compact, bool showLabel, bool showHours) const
{
	QString label;
	QString value;

	switch (kind) {
	case Kind::Cpu:
		label = moduleText("StreamUP.Toolbar.Status.Cpu.Prefix");
		value = compact ? QStringLiteral("100%") : QStringLiteral("100.0%");
		break;
	case Kind::Fps:
		label = moduleText("StreamUP.Toolbar.Status.Fps.Prefix");
		value = compact ? QStringLiteral("240") : QStringLiteral("240.00 FPS");
		break;
	case Kind::FramesDropped:
		label = moduleText("StreamUP.Toolbar.Status.FramesDropped.Prefix");
		value = compact ? QStringLiteral("88888") : QStringLiteral("88888 (100.0%)");
		break;
	case Kind::RecordTime:
	case Kind::StreamTime:
		label = kind == Kind::RecordTime ? moduleText("StreamUP.Toolbar.Status.RecordTime.Prefix")
						 : moduleText("StreamUP.Toolbar.Status.StreamTime.Prefix");
		// Always measured at the hours form. A stream that ticks over an hour
		// must not widen the bar and shove everything along at that moment.
		value = QStringLiteral("88:88:88");
		break;
	case Kind::StreamBitrate:
	case Kind::RecordBitrate:
		label = kind == Kind::StreamBitrate ? moduleText("StreamUP.Toolbar.Status.StreamBitrate.Prefix")
						    : moduleText("StreamUP.Toolbar.Status.RecordBitrate.Prefix");
		value = compact ? QStringLiteral("88888k") : QStringLiteral("88888 kb/s");
		break;
	case Kind::Message: {
		// The longest of the set, since any of them can appear.
		QString widest;
		for (int i = 0; i <= static_cast<int>(MessageId::EncodingOverloaded); ++i) {
			const QString candidate = messageText(static_cast<MessageId>(i), compact);
			if (candidate.length() > widest.length())
				widest = candidate;
		}
		return widest;
	}
	}

	Q_UNUSED(showHours)

	if (!showLabel || label.isEmpty())
		return value;
	return QStringLiteral("%1 %2").arg(label, value);
}

bool Monitor::isAlerting(Kind kind) const
{
	switch (kind) {
	case Kind::Cpu:
		return cpuPercent_ >= 90.0;
	case Kind::FramesDropped:
		return totalFrames_ > 0 &&
		       (static_cast<double>(laggedFrames_) / static_cast<double>(totalFrames_)) > 0.05;
	case Kind::Message:
		return message_ == MessageId::EncodingOverloaded;
	default:
		return false;
	}
}

// ---------------------------------------------------------------------------
// StatusWidget

StatusWidget::StatusWidget(Kind kind, bool compact, bool showLabel, bool showHours, QWidget *parent)
	: QLabel(parent),
	  kind_(kind),
	  compact_(compact),
	  showLabel_(showLabel),
	  showHours_(showHours)
{
	// Styled by the theme through the class property, like every other item
	// on the bar. Nothing here hardcodes a colour.
	setProperty("class", "streamup-toolbar-status");
	setProperty("statusKind", kindKey(kind));
	setProperty("alerting", false);
	setAlignment(Qt::AlignCenter);
	setTextInteractionFlags(Qt::NoTextInteraction);
	setFocusPolicy(Qt::NoFocus);

	Monitor::instance().addObserver();
	connect(&Monitor::instance(), &Monitor::updated, this, &StatusWidget::refresh);

	applyFixedWidth();
	refresh();
}

StatusWidget::~StatusWidget()
{
	Monitor::instance().removeObserver();
}

void StatusWidget::applyFixedWidth()
{
	const QString widest = Monitor::instance().widestText(kind_, compact_, showLabel_, showHours_);

	// A message item with nothing to say would pin itself to zero and never
	// grow, so an empty widest reading keeps the widget flexible instead.
	if (widest.isEmpty()) {
		setMinimumWidth(0);
		setMaximumWidth(QWIDGETSIZE_MAX);
		return;
	}

	setFixedWidth(fontMetrics().horizontalAdvance(widest) + fontMetrics().averageCharWidth());
}

void StatusWidget::refresh()
{
	setText(Monitor::instance().text(kind_, compact_, showLabel_, showHours_));

	const bool alerting = Monitor::instance().isAlerting(kind_);
	if (alerting == alerting_)
		return;

	// The theme decides what alerting looks like. Repolishing is what makes a
	// [alerting="true"] selector take effect on an already-shown widget.
	alerting_ = alerting;
	setProperty("alerting", alerting);
	style()->unpolish(this);
	style()->polish(this);
}

} // namespace ToolbarStatus
} // namespace StreamUP
