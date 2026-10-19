#include "mixer-strip.hpp"

#include "../mixer-enhancements.hpp"
#include "../theme-enhancements.hpp"

#include <streamup/monitoring-compat.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QApplication>
#include <QBoxLayout>
#include <QContextMenuEvent>
#include <QCursor>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QSpacerItem>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

#include "moc_mixer-strip.cpp"


namespace StreamUP {
namespace Mixer {

namespace {

// OBS's own strings, so labels and tooltips read exactly as in its mixer and
// follow the user's OBS language.
QString obsText(const char *key)
{
	const char *text = obs_frontend_get_locale_string(key);
	return QString::fromUtf8(text ? text : key);
}

const QIcon &warningIcon()
{
	static const QIcon icon(":/res/images/unassigned.svg");
	return icon;
}

const QIcon &muteIcon()
{
	// OBS uses the same glyph for muted and unmuted; themes swap it.
	static const QIcon icon(":/settings/images/settings/audio.svg");
	return icon;
}

const QIcon &monitorOnIcon()
{
	static const QIcon icon(":/res/images/headphones.svg");
	return icon;
}

const QIcon &monitorOffIcon()
{
	static const QIcon icon(":/res/images/headphones-off.svg");
	return icon;
}

bool privateFlag(obs_source_t *source, const char *key)
{
	obs_data_t *priv = obs_source_get_private_settings(source);
	const bool value = obs_data_get_bool(priv, key);
	obs_data_release(priv);
	return value;
}

bool isGlobalSource(obs_source_t *source)
{
	for (uint32_t channel = 1; channel <= 6; channel++) {
		obs_source_t *global = obs_get_output_source(channel);
		const bool match = global == source;
		obs_source_release(global);
		if (match)
			return true;
	}
	return false;
}

} // namespace

MixerStrip::MixerStrip(obs_source_t *source, bool vertical, QWidget *parent)
	: ::VolumeControl(parent),
	  m_source(obs_source_get_ref(source)),
	  m_vertical(vertical)
{
	m_uuid = QString::fromUtf8(obs_source_get_uuid(m_source));
	// OBS names each channel after its source.
	setObjectName(QString::fromUtf8(obs_source_get_name(m_source)));

	m_fader = obs_fader_create(OBS_FADER_LOG);
	m_volmeter = obs_volmeter_create(OBS_FADER_LOG);

	m_category = new QLabel(QStringLiteral("Active"), this);
	m_category->setAlignment(Qt::AlignCenter);
	StyleClass::add(m_category, "mixer-category");
	StyleClass::add(m_category, "text-tiny");

	m_name = new ::VolumeName(this);
	m_name->setMaximumWidth(280);
	StyleClass::add(m_name, "text-small");
	StyleClass::add(m_name, "mixer-name");
	m_name->installEventFilter(this);

	m_mute = new QPushButton(this);
	m_mute->setCheckable(true);
	StyleClass::add(m_mute, "btn-mute");

	m_monitor = new QPushButton(this);
	m_monitor->setCheckable(true);
	StyleClass::add(m_monitor, "btn-monitor");

	m_volLabel = new QLabel(this);
	m_volLabel->setIndent(0);
	m_volLabel->setObjectName("volLabel");

	m_slider = new MixerSlider(m_fader, Qt::Horizontal, this);
	m_slider->setMinimum(0);
	m_slider->setMaximum(int(STREAMUP_FADER_PRECISION));

	StyleClass::installStateFilter(m_mute);
	StyleClass::installStateFilter(m_monitor);

	m_meter = new ::VolumeMeter(m_volmeter, this);

	if (vertical)
		buildVertical();
	else
		buildHorizontal();

	setToolTip(QString::fromUtf8(obs_module_text("Mixer.Tooltip.Strip")));

	applyMeterSettings();

	obs_fader_add_callback(m_fader, &MixerStrip::onFaderChanged, this);
	obs_volmeter_add_callback(m_volmeter, &MixerStrip::onVolmeterUpdated, this);
	obs_fader_attach_source(m_fader, m_source);
	obs_volmeter_attach_source(m_volmeter, m_source);

	signal_handler_t *sh = obs_source_get_signal_handler(m_source);
	for (const char *signal : {"mute", "audio_mixers", "activate", "deactivate", "audio_activate",
				   "audio_deactivate"})
		signal_handler_connect(sh, signal, &MixerStrip::onSourceSignal, this);
	signal_handler_connect(sh, "audio_monitoring", &MixerStrip::onMonitoringSignal, this);
	signal_handler_connect(sh, "rename", &MixerStrip::onRenameSignal, this);

	connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
		obs_fader_set_deflection(m_fader, float(value) / float(STREAMUP_FADER_PRECISION));
		updateText();
	});
	connect(m_mute, &QPushButton::clicked, this, &MixerStrip::handleMuteButton);
	connect(m_monitor, &QPushButton::clicked, this, &MixerStrip::handleMonitorButton);
	connect(m_name, &QAbstractButton::clicked, this, [this]() { emit menuRequested(this, QCursor::pos()); });

	m_name->setFullText(QString::fromUtf8(obs_source_get_name(m_source)));
	changeVolume();
	processMixerState();

	// The StreamUP theme's mixer tweak adjusts OBS's own channels; give these
	// the same treatment so they keep matching.
	if (StreamUP::MixerEnhancements::IsOBS32_1OrNewer() &&
	    StreamUP::ThemeEnhancements::IsTweakActive(StreamUP::ThemeEnhancements::Tweak::Mixer))
		StreamUP::MixerEnhancements::EnhanceVolumeControl(this);
}

MixerStrip::~MixerStrip()
{
	// Stop the callbacks before the widgets they write into go.
	signal_handler_t *sh = obs_source_get_signal_handler(m_source);
	for (const char *signal : {"mute", "audio_mixers", "activate", "deactivate", "audio_activate",
				   "audio_deactivate"})
		signal_handler_disconnect(sh, signal, &MixerStrip::onSourceSignal, this);
	signal_handler_disconnect(sh, "audio_monitoring", &MixerStrip::onMonitoringSignal, this);
	signal_handler_disconnect(sh, "rename", &MixerStrip::onRenameSignal, this);

	obs_fader_remove_callback(m_fader, &MixerStrip::onFaderChanged, this);
	obs_volmeter_remove_callback(m_volmeter, &MixerStrip::onVolmeterUpdated, this);
	obs_fader_destroy(m_fader);
	obs_volmeter_destroy(m_volmeter);

	obs_source_release(m_source);
}

// ---- Layout: the same tree as OBS's VolumeControl::setLayoutVertical ----

void MixerStrip::buildVertical()
{
	auto *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	setMaximumWidth(110);
	setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

	auto *categoryLayout = new QHBoxLayout;
	auto *nameLayout = new QHBoxLayout;
	auto *controlLayout = new QHBoxLayout;
	auto *volLayout = new QHBoxLayout;
	auto *meterFrame = new QFrame;
	auto *meterLayout = new QHBoxLayout;

	m_meter->setVertical(true);
	m_meter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

	m_slider->setOrientation(Qt::Vertical);
	m_slider->setLayoutDirection(Qt::LeftToRight);

	m_name->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
	m_category->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
	m_volLabel->setAlignment(Qt::AlignLeft);

	categoryLayout->setAlignment(Qt::AlignCenter);
	nameLayout->setAlignment(Qt::AlignCenter);
	meterLayout->setAlignment(Qt::AlignCenter);
	controlLayout->setAlignment(Qt::AlignCenter);
	volLayout->setAlignment(Qt::AlignCenter);

	meterFrame->setObjectName("volMeterFrame");

	categoryLayout->setContentsMargins(0, 0, 0, 0);
	categoryLayout->setSpacing(0);
	categoryLayout->addWidget(m_category);

	nameLayout->setContentsMargins(0, 0, 0, 0);
	nameLayout->setSpacing(0);
	nameLayout->addWidget(m_name);

	controlLayout->setContentsMargins(0, 0, 0, 0);
	controlLayout->setSpacing(0);
	controlLayout->addWidget(m_mute);
	controlLayout->addWidget(m_monitor);

	meterLayout->setContentsMargins(0, 0, 0, 0);
	meterLayout->setSpacing(0);
	meterLayout->addWidget(m_slider);
	meterLayout->addWidget(m_meter);
	meterFrame->setLayout(meterLayout);

	volLayout->setContentsMargins(0, 0, 0, 0);
	volLayout->setSpacing(0);
	volLayout->addWidget(m_volLabel);
	volLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum));

	layout->addItem(categoryLayout);
	layout->addItem(nameLayout);
	layout->addItem(volLayout);
	layout->addWidget(meterFrame);
	layout->addItem(controlLayout);

	layout->setStretch(0, 0);
	layout->setStretch(1, 0);
	layout->setStretch(2, 0);
	layout->setStretch(3, 1);
	layout->setStretch(4, 0);

	m_meter->setFocusProxy(m_slider);
	setLayout(layout);
}

void MixerStrip::buildHorizontal()
{
	auto *layout = new QBoxLayout(QBoxLayout::TopToBottom);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	setMaximumWidth(QWIDGETSIZE_MAX);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	auto *textLayout = new QHBoxLayout;
	auto *controlLayout = new QHBoxLayout;
	auto *meterFrame = new QFrame;
	auto *meterLayout = new QVBoxLayout;
	auto *buttonLayout = new QVBoxLayout;

	m_meter->setVertical(false);
	m_meter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

	m_slider->setOrientation(Qt::Horizontal);
	m_slider->setLayoutDirection(Qt::LeftToRight);

	m_name->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_category->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	m_volLabel->setAlignment(Qt::AlignRight);

	auto *textSubLayout = new QHBoxLayout;
	textSubLayout->setContentsMargins(0, 0, 0, 0);
	textLayout->setContentsMargins(0, 0, 0, 0);

	textLayout->addWidget(m_name);
	textLayout->addItem(textSubLayout);

	textSubLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));
	textSubLayout->addWidget(m_category);
	textSubLayout->addWidget(m_volLabel);

	meterFrame->setObjectName("volMeterFrame");
	meterFrame->setLayout(meterLayout);

	meterLayout->setContentsMargins(0, 0, 0, 0);
	meterLayout->setSpacing(0);
	meterLayout->addWidget(m_slider);
	meterLayout->addWidget(m_meter);
	meterLayout->setStretch(0, 2);
	meterLayout->setStretch(1, 2);

	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(0);
	buttonLayout->addWidget(m_mute);
	buttonLayout->addWidget(m_monitor);

	controlLayout->addItem(buttonLayout);
	controlLayout->addWidget(meterFrame);

	layout->addItem(textLayout);
	layout->addItem(controlLayout);
	layout->setStretch(0, 3);
	layout->setStretch(1, 6);

	m_meter->setFocusProxy(m_slider);
	setLayout(layout);
}

void MixerStrip::applyMeterSettings()
{
	config_t *profile = obs_frontend_get_profile_config();
	if (!profile)
		return;
	const double decay = config_get_double(profile, "Audio", "MeterDecayRate");
	if (decay > 0.0)
		m_meter->setPeakDecayRate(decay);
	const uint64_t peakType = config_get_uint(profile, "Audio", "PeakMeterType");
	m_meter->setPeakMeterType(peakType == 1 ? TRUE_PEAK_METER : SAMPLE_PEAK_METER);
}

// ---- OBS callbacks: any thread. Each hops to the UI thread; Qt drops the
// queued call if the strip has been deleted by then. ----

void MixerStrip::onFaderChanged(void *data, float)
{
	auto *strip = static_cast<MixerStrip *>(data);
	QMetaObject::invokeMethod(strip, [strip]() { strip->changeVolume(); }, Qt::QueuedConnection);
}

void MixerStrip::onVolmeterUpdated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	static_cast<MixerStrip *>(data)->m_meter->setLevels(magnitude, peak, inputPeak);
}

void MixerStrip::onSourceSignal(void *data, calldata_t *)
{
	auto *strip = static_cast<MixerStrip *>(data);
	QMetaObject::invokeMethod(strip, [strip]() { strip->processMixerState(); }, Qt::QueuedConnection);
}

void MixerStrip::onMonitoringSignal(void *data, calldata_t *cd)
{
	auto *strip = static_cast<MixerStrip *>(data);
	// libobs sends this signal before it stores the new type, so reading the
	// source straight away can still see the old one. The signal carries the
	// new type on OBS 32 and older; use it, and read the source again a moment
	// later either way (OBS 33 expresses the type through mute as well).
	long long type = -1;
	if (!StreamUP::MonitoringCompat::has_monitoring_switch())
		calldata_get_int(cd, "type", &type);
	QMetaObject::invokeMethod(
		strip,
		[strip, type]() {
			strip->processMixerState(type >= 0 ? static_cast<int>(type) : -1);
			QTimer::singleShot(100, strip, [strip]() { strip->processMixerState(); });
		},
		Qt::QueuedConnection);
}

void MixerStrip::onRenameSignal(void *data, calldata_t *)
{
	auto *strip = static_cast<MixerStrip *>(data);
	QMetaObject::invokeMethod(
		strip,
		[strip]() {
			const QString name = QString::fromUtf8(obs_source_get_name(strip->m_source));
			strip->setObjectName(name);
			strip->m_name->setFullText(name);
		},
		Qt::QueuedConnection);
}

// ---- State, as OBS's VolumeControl works it out ----

void MixerStrip::changeVolume()
{
	const QSignalBlocker blocker(m_slider);
	m_slider->setValue(int(obs_fader_get_deflection(m_fader) * STREAMUP_FADER_PRECISION));
	updateText();
}

void MixerStrip::updateText()
{
	const float db = obs_fader_get_db(m_fader);
	m_volLabel->setText(db < -96.0f ? QStringLiteral("-inf dB")
					: QString::number(db, 'f', 1).append(QStringLiteral(" dB")));
}

void MixerStrip::refreshState()
{
	processMixerState();
}

void MixerStrip::setPreviewed(bool previewed)
{
	if (m_previewed == previewed)
		return;
	m_previewed = previewed;
	processMixerState();
}

void MixerStrip::processMixerState(int knownMonitoringType)
{
	const bool unassigned = obs_source_get_audio_mixers(m_source) == 0;
	const bool active = obs_source_active(m_source) && obs_source_audio_active(m_source);
	const bool muted = obs_source_muted(m_source);
	const obs_monitoring_type monitoring = knownMonitoringType >= 0
						       ? static_cast<obs_monitoring_type>(knownMonitoringType)
						       : StreamUP::MonitoringCompat::get_type(m_source);

	const QSignalBlocker blockMute(m_mute);
	const QSignalBlocker blockMonitor(m_monitor);

	const bool showAsMuted = muted || monitoring == OBS_MONITORING_TYPE_MONITOR_ONLY;
	const bool showAsMonitored = monitoring != OBS_MONITORING_TYPE_NONE;
	const bool showAsUnassigned = !muted && unassigned;
	const bool showWarningIcon = showAsUnassigned || monitoring == OBS_MONITORING_TYPE_MONITOR_ONLY;

	m_meter->setMuted((showAsMuted || showAsUnassigned) && !showAsMonitored);
	m_meter->setUseDisabledColors(showAsMuted || !active);

	const int buttonState = (showAsMuted ? 1 : 0) | (showAsMonitored ? 2 : 0) | (showWarningIcon ? 4 : 0);
	if (buttonState != m_buttonState) {
		m_buttonState = buttonState;

		m_mute->setChecked(showAsMuted);
		m_monitor->setChecked(showAsMonitored);

		m_mute->setToolTip(obsText(showAsMuted ? "Unmute" : "Mute"));
		m_monitor->setToolTip(obsText(showAsMonitored ? "Basic.AudioMixer.Monitoring.Disable"
							      : "Basic.AudioMixer.Monitoring.Enable"));

		m_mute->setIcon(showWarningIcon ? warningIcon() : muteIcon());
		m_monitor->setIcon(showAsMonitored ? monitorOnIcon() : monitorOffIcon());

		// Qt cannot switch a button's icon on :checked from a stylesheet, so
		// OBS sets a "checked" class for themes to use instead. Repolishing
		// lets a theme's own icon for the new state replace ours.
		StyleClass::toggle(m_mute, "checked", showAsMuted);
		StyleClass::toggle(m_monitor, "checked", showAsMonitored);
		StyleClass::toggle(m_mute, "mute-warning", showWarningIcon);
		StyleClass::repolish(m_mute);
		StyleClass::repolish(m_monitor);
		StyleClass::tintIcon(m_mute);
		StyleClass::tintIcon(m_monitor);
	}

	// OBS's per-source volume lock.
	m_slider->setEnabled(!privateFlag(m_source, "volume_locked"));

	updateCategoryLabel(active, unassigned);
}

void MixerStrip::updateCategoryLabel(bool active, bool unassigned)
{
	const bool global = isGlobalSource(m_source);
	const bool pinned = privateFlag(m_source, "mixer_pinned");
	const bool hidden = privateFlag(m_source, "mixer_hidden");

	QString text = obsText("Basic.AudioMixer.Category.Active");
	if (unassigned)
		text = obsText("Basic.AudioMixer.Category.Unassigned");
	else if (global)
		text = obsText("Basic.AudioMixer.Category.Global");
	else if (pinned)
		text = obsText("Basic.AudioMixer.Category.Pinned");
	else if (hidden)
		text = obsText("Basic.AudioMixer.Category.Hidden");
	else if (!active)
		text = obsText(m_previewed ? "Basic.AudioMixer.Category.Preview" : "Basic.AudioMixer.Category.Inactive");

	const bool stylePinned = global || pinned;
	const bool styleInactive = !active;

	bool changed = false;
	changed |= StyleClass::toggle(this, "volume-pinned", stylePinned);
	changed |= StyleClass::toggle(this, "volume-inactive", styleInactive);
	changed |= StyleClass::toggle(this, "volume-preview", styleInactive && m_previewed);
	changed |= StyleClass::toggle(this, "volume-hidden", hidden && !stylePinned);
	changed |= StyleClass::toggle(this, "volume-unassigned", unassigned);

	if (m_category->text() != text)
		m_category->setText(text);

	// A class on the strip changes its children's rules (category colours,
	// muted names, greyed meters, dimmed buttons), so repolish them, but only
	// when a class really changed and the strip is already on screen.
	if (changed && testAttribute(Qt::WA_WState_Polished)) {
		StyleClass::repolish(m_category);
		StyleClass::repolish(m_name);
		StyleClass::repolish(m_mute);
		StyleClass::repolish(m_monitor);
		StyleClass::tintIcon(m_mute);
		StyleClass::tintIcon(m_monitor);
		m_meter->refreshStyle();
	}
}

void MixerStrip::handleMuteButton(bool mute)
{
	obs_source_set_muted(m_source, mute);
	// As in OBS: unmuting a Monitor Only source sends it to the stream again.
	if (!mute && StreamUP::MonitoringCompat::get_type(m_source) == OBS_MONITORING_TYPE_MONITOR_ONLY)
		StreamUP::MonitoringCompat::set_type(m_source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	processMixerState();
}

void MixerStrip::handleMonitorButton(bool enable)
{
	// Off or Monitor and Output, as OBS's button does. Through the compat
	// switch rather than the type so OBS 33 leaves the mute state alone.
	StreamUP::MonitoringCompat::set_enabled(m_source, enable);
	processMixerState();
}

// ---- Dragging: from the strip itself, or from its name ----

bool MixerStrip::pressStartsDrag(QMouseEvent *event)
{
	if (event->button() != Qt::LeftButton)
		return false;
	m_pressed = true;
	m_pressPos = event->globalPosition().toPoint();
	return true;
}

bool MixerStrip::moveStartsDrag(QMouseEvent *event)
{
	if (!m_pressed || !(event->buttons() & Qt::LeftButton))
		return false;
	if ((event->globalPosition().toPoint() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
		return false;
	m_pressed = false;
	// The dock runs the drag. It keeps this strip alive until the drag's
	// event loop has finished, so returning straight after is safe.
	emit dragRequested(this);
	return true;
}

bool MixerStrip::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == m_name) {
		if (event->type() == QEvent::MouseButtonPress) {
			pressStartsDrag(static_cast<QMouseEvent *>(event));
		} else if (event->type() == QEvent::MouseMove) {
			if (moveStartsDrag(static_cast<QMouseEvent *>(event))) {
				// A drag is not a click: leave the name button unpressed.
				m_name->setDown(false);
				return true;
			}
		} else if (event->type() == QEvent::MouseButtonRelease) {
			m_pressed = false;
		}
	}
	return ::VolumeControl::eventFilter(watched, event);
}

void MixerStrip::mousePressEvent(QMouseEvent *event)
{
	pressStartsDrag(event);
	::VolumeControl::mousePressEvent(event);
}

void MixerStrip::mouseMoveEvent(QMouseEvent *event)
{
	if (moveStartsDrag(event))
		return;
	::VolumeControl::mouseMoveEvent(event);
}

void MixerStrip::mouseReleaseEvent(QMouseEvent *event)
{
	m_pressed = false;
	::VolumeControl::mouseReleaseEvent(event);
}

void MixerStrip::contextMenuEvent(QContextMenuEvent *event)
{
	emit menuRequested(this, event->globalPos());
	event->accept();
}

} // namespace Mixer
} // namespace StreamUP
