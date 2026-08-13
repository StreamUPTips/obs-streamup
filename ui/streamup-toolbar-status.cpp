#include "streamup-toolbar-status.hpp"
#include "../streamup.hpp"
#include "ui-helpers.hpp"

#include <obs-module.h>

#include <QBoxLayout>
#include <QDateTime>
#include <QEvent>
#include <QIcon>
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

QString kindIconName(Kind kind)
{
	switch (kind) {
	case Kind::Cpu:
		return QStringLiteral("status-cpu");
	case Kind::Fps:
		return QStringLiteral("status-fps");
	case Kind::FramesDropped:
		return QStringLiteral("status-dropped");
	case Kind::RecordTime:
		return QStringLiteral("status-record-time");
	case Kind::StreamTime:
		return QStringLiteral("status-stream-time");
	case Kind::StreamBitrate:
		return QStringLiteral("status-stream-bitrate");
	case Kind::RecordBitrate:
		return QStringLiteral("status-record-bitrate");
	case Kind::Message:
		return QStringLiteral("status-message");
	}
	return QStringLiteral("status-message");
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

QString Monitor::text(Kind kind, bool compact, bool showHours) const
{
	switch (kind) {
	case Kind::Cpu:
		return QStringLiteral("%1%").arg(cpuPercent_, 0, 'f', compact ? 0 : 1);
	case Kind::Fps:
		return QString::number(fps_, 'f', compact ? 0 : 2);
	case Kind::FramesDropped: {
		// Rendering lag, which is what OBS's "missed frames" readout counts.
		const double percent = totalFrames_ > 0
					       ? (static_cast<double>(laggedFrames_) / static_cast<double>(totalFrames_)) * 100.0
					       : 0.0;
		return compact ? QStringLiteral("%1").arg(laggedFrames_)
			       : QStringLiteral("%1 (%2%)").arg(laggedFrames_).arg(percent, 0, 'f', 1);
	}
	case Kind::RecordTime:
		return durationText(recordTime_.ms(), showHours);
	case Kind::StreamTime:
		return durationText(streamTime_.ms(), showHours);
	case Kind::StreamBitrate:
		return bitrateText(streamRate_.kbps, compact);
	case Kind::RecordBitrate:
		return bitrateText(recordRate_.kbps, compact);
	case Kind::Message:
		return messageText(message_, compact);
	}
	return QString();
}

QString Monitor::widestText(Kind kind, bool compact, bool showHours) const
{
	Q_UNUSED(showHours)

	switch (kind) {
	case Kind::Cpu:
		return compact ? QStringLiteral("100%") : QStringLiteral("100.0%");
	case Kind::Fps:
		return compact ? QStringLiteral("240") : QStringLiteral("240.00");
	case Kind::FramesDropped:
		// A realistic reading, not the worst case. Reserving room for 88888
		// frames at 100% left a hole in the bar that nobody's numbers ever
		// filled. Anything bigger widens the slot when it turns up.
		return compact ? QStringLiteral("888") : QStringLiteral("888 (8.8%)");
	case Kind::RecordTime:
	case Kind::StreamTime:
		// Reserving the hours form from the start costs a wide, mostly empty
		// box all session. Reserving the minutes form and growing once, at the
		// hour mark, costs a single reflow per stream. The width only ever
		// grows (see StatusWidget::applyPinnedSize), so it cannot oscillate at
		// the boundary. Asking for hours pins it wide from the start, for
		// anyone who would rather it never moved.
		return showHours ? QStringLiteral("88:88:88") : QStringLiteral("88:88");
	case Kind::StreamBitrate:
	case Kind::RecordBitrate:
		// Four digits covers the bitrates people actually stream and record at.
		// A higher one widens the slot once and stays there.
		return compact ? QStringLiteral("8888k") : QStringLiteral("8888 kb/s");
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
	return QString();
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

namespace {

// Slack on the measured text width, so a string measured to the pixel does not
// clip on a heavier weight or a wider font. Kept small: this is anti-clipping
// insurance, not spacing. It used to be wide enough to double as padding, which
// left every value floating in its slot. Space between items is the item's own
// margin below, where it is even on both sides.
constexpr int kValueSlackPx = 3;

// Even margin either side of an item, so a separator dropped between two of
// them sits in the middle of a gap rather than up against an icon.
constexpr int kItemMarginPx = 6;
constexpr int kItemMarginVerticalPx = 3;

// The icon sits at the toolbar's own icon size where it can, but a status item
// is text-height furniture, so it is capped to something that reads next to a
// number rather than dwarfing it.
constexpr int kIconPx = 14;

} // namespace

StatusWidget::StatusWidget(Kind kind, bool vertical, bool showIcon, bool showHours, bool preview, QWidget *parent)
	: QWidget(parent),
	  kind_(kind),
	  vertical_(vertical),
	  showIcon_(showIcon),
	  showHours_(showHours),
	  preview_(preview)
{
	// Styled by the theme through the class property, like every other item
	// on the bar. Nothing here hardcodes a colour.
	setProperty("class", "streamup-toolbar-status");
	setProperty("statusKind", kindKey(kind));
	setProperty("alerting", false);
	setFocusPolicy(Qt::NoFocus);

	// With an icon instead of a word, this is the only thing that says what
	// the number is.
	setToolTip(kindDisplayName(kind));

	// Stacked when the bar runs down the side, in a row when it runs across.
	auto *layout = new QBoxLayout(vertical_ ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight, this);
	// Even on both sides. Uneven margins are what made a separator dropped
	// between two readouts sit hard against the icon on one side.
	if (vertical_)
		layout->setContentsMargins(0, kItemMarginVerticalPx, 0, kItemMarginVerticalPx);
	else
		layout->setContentsMargins(kItemMarginPx, 0, kItemMarginPx, 0);
	layout->setSpacing(vertical_ ? 0 : 4);

	icon_ = new QLabel(this);
	icon_->setAlignment(Qt::AlignCenter);
	icon_->setFixedSize(kIconPx, kIconPx);
	icon_->setPixmap(QIcon(UIHelpers::GetThemedIconPath(kindIconName(kind))).pixmap(kIconPx, kIconPx));
	icon_->setVisible(showIcon_);
	layout->addWidget(icon_, 0, Qt::AlignCenter);

	value_ = new QLabel(this);
	// Along the bar the value reads as a pair with its icon, so it sits against
	// it and any reserved slack falls to the right. Centring it inside a box
	// sized for the widest possible reading opens a gap on both sides and the
	// number drifts away from the icon that names it. Stacked, the value is
	// under its icon and centring is what lines the column up.
	value_->setAlignment(vertical_ ? Qt::AlignCenter : (Qt::AlignLeft | Qt::AlignVCenter));
	value_->setTextInteractionFlags(Qt::NoTextInteraction);
	layout->addWidget(value_, 0, vertical_ ? Qt::AlignCenter : Qt::AlignVCenter);

	Monitor::instance().addObserver();
	connect(&Monitor::instance(), &Monitor::updated, this, &StatusWidget::refresh);

	applyPinnedSize();
	refresh();
}

StatusWidget::~StatusWidget()
{
	Monitor::instance().removeObserver();
}

void StatusWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
	// The theme's font is only certain once the widget has been polished, and
	// the width is measured from it.
	applyPinnedSize();
}

void StatusWidget::changeEvent(QEvent *event)
{
	QWidget::changeEvent(event);
	if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
		applyPinnedSize();
}

void StatusWidget::applyPinnedSize()
{
	if (!value_)
		return;

	const QString widest = Monitor::instance().widestText(kind_, vertical_, showHours_);

	// A message item with nothing to say would pin itself to zero and never
	// grow, so an empty widest reading keeps the widget flexible instead.
	if (widest.isEmpty()) {
		value_->setMinimumWidth(0);
		value_->setMaximumWidth(QWIDGETSIZE_MAX);
		return;
	}

	// Measured on the label's own font, not the parent's: the theme styles the
	// label, and measuring the wrong font is how text ends up clipped.
	const QFontMetrics metrics = value_->fontMetrics();
	int width = metrics.horizontalAdvance(widest) + kValueSlackPx;

	// The reserved reading is what a value normally fits inside, not a promise.
	// A recording passing an hour, or a frame counter going further than anyone
	// expected, must widen the slot rather than lose the end of the number.
	width = qMax(width, metrics.horizontalAdvance(value_->text()) + kValueSlackPx);

	// Grow only. Shrinking back the moment a value gets shorter is the jitter
	// this whole mechanism exists to prevent.
	width = qMax(width, pinnedWidth_);

	if (width == pinnedWidth_)
		return;

	pinnedWidth_ = width;
	value_->setFixedWidth(width);
}

void StatusWidget::refresh()
{
	QString text = Monitor::instance().text(kind_, vertical_, showHours_);

	// In the editor an empty message would be an invisible item: nothing to
	// click, nothing to drag, no way to remove it. It shows a sample of what it
	// will look like instead, which is the point of a preview.
	if (preview_ && kind_ == Kind::Message && text.isEmpty())
		text = moduleText(vertical_ ? "StreamUP.Toolbar.Status.Msg.RecordingStarted.Short"
					    : "StreamUP.Toolbar.Status.Msg.RecordingStarted");

	value_->setText(text);

	// Messages are transient. Every other readout always has a value, but this
	// one is empty most of the time, and an icon sitting over nothing is dead
	// space on a bar where space is the whole problem. It takes its slot when
	// there is something to say and gives it back afterwards.
	if (kind_ == Kind::Message && !preview_)
		setVisible(!text.isEmpty());

	// Catches the value outgrowing its slot, which for a clock happens exactly
	// once, when it passes an hour.
	applyPinnedSize();

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
