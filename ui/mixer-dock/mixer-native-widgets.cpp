// Ports of OBS's mixer widgets (frontend/components VolumeMeter, VolumeName,
// VolumeSlider and AbsoluteSlider, GPL-2.0, by the OBS Project contributors and
// Taylor Giampaolo). Behaviour and drawing are kept as close to the originals
// as possible so the StreamUP Mixer looks and reacts like the native one; the
// differences are only where the originals reached into OBS internals.

#include "mixer-native-widgets.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <QApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>
#include <QStyleOptionSlider>
#include <QStylePainter>
#include <QTextDocument>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

#include "moc_mixer-native-widgets.cpp"

namespace {

constexpr int INDICATOR_THICKNESS = 3;
constexpr int CLIP_FLASH_DURATION_MS = 1000;
constexpr int TICK_SIZE = 2;
constexpr int TICK_DB_INTERVAL = 6;
constexpr const char *TICK_LABEL_TOKEN = "-88";
constexpr float TICK_LABEL_HEIGHT_SCALE_FACTOR = 0.8f;

QColor color_from_int(long long val)
{
	QColor color(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
	color.setAlpha(255);
	return color;
}

// OBS's accessibility setting can override the meter colours; honour it the
// same way the native meter does.
QColor accessibleOverride(const QColor &themeColour, const char *key)
{
	config_t *config = obs_frontend_get_user_config();
	if (config && config_get_bool(config, "Accessibility", "OverrideColors"))
		return color_from_int(config_get_int(config, "Accessibility", key));
	return themeColour;
}

int convertToInt(float number)
{
	constexpr int min = std::numeric_limits<int>::min();
	constexpr int max = std::numeric_limits<int>::max();
	if (number >= (float)max)
		return max;
	if (number < min)
		return min;
	return int(number);
}

// One shared redraw timer for every meter, as in OBS.
QTimer *meterTimer()
{
	static QPointer<QTimer> timer;
	if (!timer) {
		timer = new QTimer(qApp);
		timer->setTimerType(Qt::PreciseTimer);
		timer->start(16);
	}
	return timer;
}

QString plainText(const QString &text)
{
	if (Qt::mightBeRichText(text)) {
		QTextDocument doc;
		doc.setHtml(text);
		return doc.toPlainText();
	}
	return text;
}

} // namespace

// ============================================================================
// VolumeMeter
// ============================================================================

void VolumeMeter::setBackgroundNominalColor(QColor c)
{
	p_backgroundNominalColor = c;
	backgroundNominalColor = accessibleOverride(c, "MixerGreen");
}

void VolumeMeter::setBackgroundWarningColor(QColor c)
{
	p_backgroundWarningColor = c;
	backgroundWarningColor = accessibleOverride(c, "MixerYellow");
}

void VolumeMeter::setBackgroundErrorColor(QColor c)
{
	p_backgroundErrorColor = c;
	backgroundErrorColor = accessibleOverride(c, "MixerRed");
}

void VolumeMeter::setForegroundNominalColor(QColor c)
{
	p_foregroundNominalColor = c;
	foregroundNominalColor = accessibleOverride(c, "MixerGreenActive");
}

void VolumeMeter::setForegroundWarningColor(QColor c)
{
	p_foregroundWarningColor = c;
	foregroundWarningColor = accessibleOverride(c, "MixerYellowActive");
}

void VolumeMeter::setForegroundErrorColor(QColor c)
{
	p_foregroundErrorColor = c;
	foregroundErrorColor = accessibleOverride(c, "MixerRedActive");
}

VolumeMeter::VolumeMeter(obs_volmeter_t *volmeter_, QWidget *parent) : QWidget(parent), volmeter(volmeter_)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	// Clicks fall through to the strip, which is what starts a drag.
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setFocusPolicy(Qt::NoFocus);

	// Defaults, only seen when no stylesheet sets them (as in OBS).
	setBackgroundNominalColor(QColor(0x26, 0x7f, 0x26));
	setBackgroundWarningColor(QColor(0x7f, 0x7f, 0x26));
	setBackgroundErrorColor(QColor(0x7f, 0x26, 0x26));
	setForegroundNominalColor(QColor(0x4c, 0xff, 0x4c));
	setForegroundWarningColor(QColor(0xff, 0xff, 0x4c));
	setForegroundErrorColor(QColor(0xff, 0x4c, 0x4c));
	backgroundNominalColorDisabled.setRgb(90, 90, 90);
	backgroundWarningColorDisabled.setRgb(117, 117, 117);
	backgroundErrorColorDisabled.setRgb(65, 65, 65);
	foregroundNominalColorDisabled.setRgb(163, 163, 163);
	foregroundWarningColorDisabled.setRgb(217, 217, 217);
	foregroundErrorColorDisabled.setRgb(113, 113, 113);
	clipColor.setRgb(0xff, 0xff, 0xff);
	magnitudeColor.setRgb(0x00, 0x00, 0x00);
	majorTickColor.setRgb(0x00, 0x00, 0x00);
	minorTickColor.setRgb(0x32, 0x32, 0x32);

	audio_t *audio = obs_get_audio();
	channels = audio ? (int)audio_output_get_channels(audio) : 2;

	resetLevels();

	connect(meterTimer(), &QTimer::timeout, this, [this]() {
		if (!isVisible())
			return;
		if (needLayoutChange()) {
			doLayout();
			update();
		} else {
			update(getBarRect());
		}
	});

	style()->polish(this);
	updateTickLabelTokenSize();
	doLayout();
}

void VolumeMeter::refreshStyle()
{
	style()->unpolish(this);
	style()->polish(this);
	updateTickLabelTokenSize();
	updateBackgroundCache(true);
	doLayout();
	update();
}

void VolumeMeter::refreshColors()
{
	setBackgroundNominalColor(getBackgroundNominalColor());
	setBackgroundWarningColor(getBackgroundWarningColor());
	setBackgroundErrorColor(getBackgroundErrorColor());
	setForegroundNominalColor(getForegroundNominalColor());
	setForegroundWarningColor(getForegroundWarningColor());
	setForegroundErrorColor(getForegroundErrorColor());
	updateBackgroundCache(true);
	update();
}

void VolumeMeter::setLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS],
			    const float inputPeak[MAX_AUDIO_CHANNELS])
{
	uint64_t ts = os_gettime_ns();
	QMutexLocker locker(&dataMutex);

	currentLastUpdateTime = ts;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = magnitude[channelNr];
		currentPeak[channelNr] = peak[channelNr];
		currentInputPeak[channelNr] = inputPeak[channelNr];
	}

	locker.unlock();
	calculateBallistics(ts);
}

void VolumeMeter::resetLevels()
{
	currentLastUpdateTime = 0;
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++) {
		currentMagnitude[channelNr] = -M_INFINITE;
		currentPeak[channelNr] = -M_INFINITE;
		currentInputPeak[channelNr] = -M_INFINITE;

		displayMagnitude[channelNr] = -M_INFINITE;
		displayPeak[channelNr] = -M_INFINITE;
		displayPeakHold[channelNr] = -M_INFINITE;
		displayPeakHoldLastUpdateTime[channelNr] = 0;
		displayInputPeakHold[channelNr] = -M_INFINITE;
		displayInputPeakHoldLastUpdateTime[channelNr] = 0;
	}
}

bool VolumeMeter::needLayoutChange()
{
	int current = volmeter ? obs_volmeter_get_nr_channels(volmeter) : 0;
	if (!current) {
		struct obs_audio_info oai;
		obs_get_audio_info(&oai);
		current = (oai.speakers == SPEAKERS_MONO) ? 1 : 2;
	}
	if (displayNrAudioChannels != current) {
		displayNrAudioChannels = current;
		return true;
	}
	return false;
}

void VolumeMeter::setVertical(bool vertical_)
{
	if (vertical == vertical_)
		return;
	vertical = vertical_;
	doLayout();
}

void VolumeMeter::setUseDisabledColors(bool enable)
{
	if (useDisabledColors == enable)
		return;
	useDisabledColors = enable;
	updateBackgroundCache(true);
}

void VolumeMeter::setMuted(bool mute)
{
	muted = mute;
}

void VolumeMeter::setPeakMeterType(enum obs_peak_meter_type type)
{
	if (volmeter)
		obs_volmeter_set_peak_meter_type(volmeter, type);
	if (type == TRUE_PEAK_METER) {
		errorLevel = -2.0;
		warningLevel = -13.0;
	} else {
		errorLevel = -9.0;
		warningLevel = -20.0;
	}
	updateBackgroundCache(true);
}

QRect VolumeMeter::getBarRect() const
{
	QRect barRect = rect();
	if (vertical)
		barRect.setWidth(displayNrAudioChannels * (meterThickness + 1) - 1);
	else
		barRect.setHeight(displayNrAudioChannels * (meterThickness + 1) - 1);
	return barRect;
}

void VolumeMeter::doLayout()
{
	QMutexLocker locker(&dataMutex);

	if (displayNrAudioChannels) {
		int meterSize = (int)std::floor(22 / displayNrAudioChannels);
		meterThickness = std::clamp(meterSize, 3, 6);
	}

	locker.unlock();
	updateBackgroundCache();
	locker.relock();
	resetLevels();
	locker.unlock();

	updateGeometry();
}

bool VolumeMeter::detectIdle(uint64_t ts)
{
	double secondsSinceLastUpdate = (ts - currentLastUpdateTime) * 0.000000001;
	if (secondsSinceLastUpdate > 0.5) {
		resetLevels();
		return true;
	}
	return false;
}

void VolumeMeter::calculateBallisticsForChannel(int channelNr, uint64_t ts, qreal timeSinceLastRedraw)
{
	if (currentPeak[channelNr] >= displayPeak[channelNr] || std::isnan(displayPeak[channelNr])) {
		displayPeak[channelNr] = currentPeak[channelNr];
	} else {
		float decay = float(peakDecayRate * timeSinceLastRedraw);
		displayPeak[channelNr] =
			std::clamp(displayPeak[channelNr] - decay, std::min(currentPeak[channelNr], 0.f), 0.f);
	}

	if (currentPeak[channelNr] >= displayPeakHold[channelNr] || !std::isfinite(displayPeakHold[channelNr])) {
		displayPeakHold[channelNr] = currentPeak[channelNr];
		displayPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak = (uint64_t)(ts - displayPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > peakHoldDuration) {
			displayPeakHold[channelNr] = currentPeak[channelNr];
			displayPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (currentInputPeak[channelNr] >= displayInputPeakHold[channelNr] ||
	    !std::isfinite(displayInputPeakHold[channelNr])) {
		displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
		displayInputPeakHoldLastUpdateTime[channelNr] = ts;
	} else {
		qreal timeSinceLastPeak = (uint64_t)(ts - displayInputPeakHoldLastUpdateTime[channelNr]) * 0.000000001;
		if (timeSinceLastPeak > inputPeakHoldDuration) {
			displayInputPeakHold[channelNr] = currentInputPeak[channelNr];
			displayInputPeakHoldLastUpdateTime[channelNr] = ts;
		}
	}

	if (!std::isfinite(displayMagnitude[channelNr])) {
		displayMagnitude[channelNr] = currentMagnitude[channelNr];
	} else {
		float attack = float((currentMagnitude[channelNr] - displayMagnitude[channelNr]) *
				     (timeSinceLastRedraw / magnitudeIntegrationTime) * 0.99);
		displayMagnitude[channelNr] =
			std::clamp(displayMagnitude[channelNr] + attack, (float)minimumLevel, 0.f);
	}
}

void VolumeMeter::calculateBallistics(uint64_t ts, qreal timeSinceLastRedraw)
{
	QMutexLocker locker(&dataMutex);
	for (int channelNr = 0; channelNr < MAX_AUDIO_CHANNELS; channelNr++)
		calculateBallisticsForChannel(channelNr, ts, timeSinceLastRedraw);
}

QColor VolumeMeter::getPeakColor(float peakHold)
{
	if (peakHold < minimumInputLevel)
		return backgroundNominalColor;
	if (peakHold < warningLevel)
		return foregroundNominalColor;
	if (peakHold < errorLevel)
		return foregroundWarningColor;
	if (peakHold < clipLevel)
		return foregroundErrorColor;
	return clipColor;
}

void VolumeMeter::paintHTicks(QPainter &painter, int x, int y, int width)
{
	qreal scale = width / minimumLevel;

	painter.setFont(font());
	QFontMetrics metrics(font());
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= TICK_DB_INTERVAL) {
		int position = int(x + width - (i * scale) - 1);
		QString str = QString::number(i);

		QRect textBounds = metrics.boundingRect(str);
		int pos;
		if (i == 0) {
			pos = position - textBounds.width();
		} else {
			pos = position - (textBounds.width() / 2);
			if (pos < 0)
				pos = 0;
		}
		painter.drawText(pos, y + 4 + metrics.capHeight(), str);
		painter.drawLine(position, y, position, y + TICK_SIZE);
	}
}

void VolumeMeter::paintVTicks(QPainter &painter, int x, int y, int height)
{
	qreal scale = height / minimumLevel;

	painter.setFont(font());
	QFontMetrics metrics(font());
	painter.setPen(majorTickColor);

	for (int i = 0; i >= minimumLevel; i -= TICK_DB_INTERVAL) {
		int position = y + int(i * scale);
		QString str = QString::number(i);

		if (i == 0)
			painter.drawText(x + 10, position + metrics.capHeight(), str);
		else
			painter.drawText(x + 8, position + (metrics.capHeight() / 2), str);

		painter.drawLine(x, position, x + TICK_SIZE, position);
	}
}

void VolumeMeter::updateTickLabelTokenSize()
{
	QFontMetrics metrics(font());
	tickTextTokenRect = metrics.size(Qt::TextSingleLine, TICK_LABEL_TOKEN);
}

void VolumeMeter::updateBackgroundCache(bool force)
{
	if (!force && size().isEmpty())
		return;
	if (!force && backgroundCache.size() == size() && !backgroundCache.isNull())
		return;
	if (!force && displayNrAudioChannels <= 0)
		return;

	QColor backgroundColor = palette().color(QPalette::Window);

	backgroundCache = QPixmap(size() * devicePixelRatioF());
	backgroundCache.setDevicePixelRatio(devicePixelRatioF());
	backgroundCache.fill(backgroundColor);

	QPainter bg{&backgroundCache};
	QRect widgetRect = rect();

	if (vertical) {
		paintVTicks(bg, displayNrAudioChannels * (meterThickness + 1) - 1, 0,
			    widgetRect.height() - (INDICATOR_THICKNESS + 3));
	} else {
		paintHTicks(bg, INDICATOR_THICKNESS + 3, displayNrAudioChannels * (meterThickness + 1) - 1,
			    widgetRect.width() - (INDICATOR_THICKNESS + 3));
	}

	bool disabledColors = muted || useDisabledColors;
	QColor nominal = disabledColors ? backgroundNominalColorDisabled : backgroundNominalColor;
	QColor warning = disabledColors ? backgroundWarningColorDisabled : backgroundWarningColor;
	QColor error = disabledColors ? backgroundErrorColorDisabled : backgroundErrorColor;

	int meterStart = INDICATOR_THICKNESS + 2;
	int meterLength = vertical ? rect().height() - (INDICATOR_THICKNESS + 2)
				   : rect().width() - (INDICATOR_THICKNESS + 2);

	qreal scale = meterLength / minimumLevel;

	int warningPosition = meterLength - convertToInt(float(warningLevel * scale));
	int errorPosition = meterLength - convertToInt(float(errorLevel * scale));

	int nominalLength = warningPosition;
	int warningLength = nominalLength + (errorPosition - warningPosition);

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
		int channelOffset = channelNr * (meterThickness + 1);

		if (vertical) {
			bg.fillRect(channelOffset, meterLength, meterThickness, -meterLength, error);
			bg.fillRect(channelOffset, meterLength, meterThickness, -warningLength, warning);
			bg.fillRect(channelOffset, meterLength, meterThickness, -nominalLength, nominal);
		} else {
			bg.fillRect(meterStart, channelOffset, meterLength, meterThickness, error);
			bg.fillRect(meterStart, channelOffset, warningLength, meterThickness, warning);
			bg.fillRect(meterStart, channelOffset, nominalLength, meterThickness, nominal);
		}
	}
}

void VolumeMeter::paintEvent(QPaintEvent *)
{
	uint64_t ts = os_gettime_ns();
	qreal timeSinceLastRedraw = (ts - lastRedrawTime) * 0.000000001;
	calculateBallistics(ts, timeSinceLastRedraw);
	bool idle = detectIdle(ts);

	QPainter painter(this);

	bool disabledColors = muted || useDisabledColors;
	QColor nominal = disabledColors ? foregroundNominalColorDisabled : foregroundNominalColor;
	QColor warning = disabledColors ? foregroundWarningColorDisabled : foregroundWarningColor;
	QColor error = disabledColors ? foregroundErrorColorDisabled : foregroundErrorColor;

	int meterStart = INDICATOR_THICKNESS + 2;
	int meterLength = vertical ? rect().height() - (INDICATOR_THICKNESS + 2)
				   : rect().width() - (INDICATOR_THICKNESS + 2);

	const qreal scale = meterLength / minimumLevel;

	painter.drawPixmap(0, 0, backgroundCache);

	int warningPosition = meterLength - convertToInt(float(warningLevel * scale));
	int errorPosition = meterLength - convertToInt(float(errorLevel * scale));
	int clipPosition = meterLength - convertToInt(float(clipLevel * scale));

	int nominalLength = warningPosition;
	int warningLength = nominalLength + (errorPosition - warningPosition);

	for (int channelNr = 0; channelNr < displayNrAudioChannels; channelNr++) {
		int channelNrFixed = (displayNrAudioChannels == 1 && channels > 2) ? 2 : channelNr;

		QMutexLocker locker(&dataMutex);
		float peak = displayPeak[channelNrFixed];
		float peakHold = displayPeakHold[channelNrFixed];
		float magnitude = displayMagnitude[channelNrFixed];

		int peakPosition = meterLength - convertToInt(float(peak * scale));
		int peakHoldPosition = meterLength - convertToInt(float(peakHold * scale));
		int magnitudePosition = meterLength - convertToInt(float(magnitude * scale));
		locker.unlock();

		if (clipping)
			peakPosition = meterLength;

		auto fill = [&](int pos, int length, const QColor &color) {
			if (vertical)
				painter.fillRect(pos, meterLength, meterThickness, -length, color);
			else
				painter.fillRect(meterStart, pos, length, meterThickness, color);
		};

		int channelOffset = channelNr * (meterThickness + 1);

		if (peakPosition >= clipPosition) {
			if (!clipping) {
				QTimer::singleShot(CLIP_FLASH_DURATION_MS, this, [this]() { clipping = false; });
				clipping = true;
			}
			fill(channelOffset, meterLength, error);
		} else {
			if (peakPosition > errorPosition)
				fill(channelOffset, std::min(peakPosition, meterLength), error);
			if (peakPosition > warningPosition)
				fill(channelOffset, std::min(peakPosition, warningLength), warning);
			if (peakPosition > meterStart)
				fill(channelOffset, std::min(peakPosition, nominalLength), nominal);
		}

		QColor peakHoldColor = nominal;
		if (peakHoldPosition >= errorPosition)
			peakHoldColor = error;
		else if (peakHoldPosition >= warningPosition)
			peakHoldColor = warning;

		if (peakHoldPosition - 3 > 0) {
			if (vertical)
				painter.fillRect(channelOffset, meterLength - peakHoldPosition - 3, meterThickness, 3,
						 peakHoldColor);
			else
				painter.fillRect(meterStart + peakHoldPosition - 3, channelOffset, 3, meterThickness,
						 peakHoldColor);
		}

		if (magnitudePosition - 3 >= 0) {
			if (vertical)
				painter.fillRect(channelOffset, meterLength - magnitudePosition - 3, meterThickness, 3,
						 magnitudeColor);
			else
				painter.fillRect(meterStart + magnitudePosition - 3, channelOffset, 3, meterThickness,
						 magnitudeColor);
		}

		if (idle)
			continue;

		if (vertical)
			painter.fillRect(channelOffset, rect().height(), meterThickness, -INDICATOR_THICKNESS,
					 getPeakColor(displayInputPeakHold[channelNrFixed]));
		else
			painter.fillRect(0, channelOffset, INDICATOR_THICKNESS, meterThickness,
					 getPeakColor(displayInputPeakHold[channelNrFixed]));
	}

	lastRedrawTime = ts;
}

void VolumeMeter::resizeEvent(QResizeEvent *event)
{
	updateBackgroundCache();
	QWidget::resizeEvent(event);
}

QSize VolumeMeter::minimumSizeHint() const
{
	return sizeHint();
}

QSize VolumeMeter::sizeHint() const
{
	QRect meterRect = getBarRect();
	int labelTotal = (int)std::abs(minimumLevel / TICK_DB_INTERVAL) + 1;

	if (vertical) {
		int width = meterRect.width() + tickTextTokenRect.width() + TICK_SIZE + 10;
		int height = int(labelTotal * (tickTextTokenRect.height() * TICK_LABEL_HEIGHT_SCALE_FACTOR)) +
			     INDICATOR_THICKNESS;
		return QSize(width, height);
	}
	int width = (labelTotal * tickTextTokenRect.width()) + INDICATOR_THICKNESS;
	int height = meterRect.height() + tickTextTokenRect.height();
	return QSize(width, height);
}

// ============================================================================
// VolumeName
// ============================================================================

VolumeName::VolumeName(QWidget *parent)
	: QAbstractButton(parent),
	  indicatorWidth(style()->pixelMetric(QStyle::PM_MenuButtonIndicator, nullptr, this))
{
	auto *layout = new QHBoxLayout();
	setLayout(layout);

	label = new QLabel(this);
	label->setIndent(0);
	label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	layout->addWidget(label);
	layout->setContentsMargins(0, 0, indicatorWidth, 0);
}

void VolumeName::setFullText(const QString &text)
{
	fullText = text;
	QAbstractButton::setText(text);
	updateGeometry();
	updateLabelText();
}

QSize VolumeName::minimumSizeHint() const
{
	QStyleOptionButton opt;
	opt.initFrom(this);

	QFontMetrics metrics(label->font());
	QSize textSize = metrics.size(Qt::TextSingleLine, plainText(fullText));

	int height = textSize.height();
	if (!opt.icon.isNull())
		height = std::max(height, indicatorWidth);

	QSize contentsSize = style()->sizeFromContents(QStyle::CT_PushButton, &opt, QSize(0, height), this);
	contentsSize.rwidth() += indicatorWidth;
	return contentsSize;
}

QSize VolumeName::sizeHint() const
{
	QFontMetrics metrics(label->font());
	int width = metrics.horizontalAdvance(plainText(fullText)) + indicatorWidth;
	QMargins margins = label->contentsMargins();
	width += margins.left() + margins.right();
	int height = metrics.height() + margins.top() + margins.bottom();
	return QSize(width, height);
}

void VolumeName::resizeEvent(QResizeEvent *event)
{
	updateLabelText();
	QAbstractButton::resizeEvent(event);
}

void VolumeName::paintEvent(QPaintEvent *)
{
	QStylePainter painter(this);
	QStyleOptionButton opt;
	opt.initFrom(this);

	painter.drawControl(QStyle::CE_PushButtonBevel, opt);

	QRect contentRect = style()->subElementRect(QStyle::SE_PushButtonContents, &opt, this);
	int paddingRight = opt.rect.right() - contentRect.right();
	int arrowWidth = style()->pixelMetric(QStyle::PM_MenuButtonIndicator, &opt, this);

	QStyleOption arrowOpt = opt;
	arrowOpt.rect = QRect(opt.rect.right() - arrowWidth - paddingRight / 2, opt.rect.center().y() - arrowWidth / 2,
			      arrowWidth, arrowWidth);
	painter.drawPrimitive(QStyle::PE_IndicatorArrowDown, arrowOpt);
}

void VolumeName::updateLabelText()
{
	if (!label)
		return;
	const QString plain = plainText(fullText);
	QFontMetrics metrics(label->font());

	int availableWidth = label->contentsRect().width();
	if (availableWidth <= 0) {
		label->clear();
		return;
	}
	if (availableWidth > metrics.horizontalAdvance(plain)) {
		label->setText(fullText);
		return;
	}
	label->setText(metrics.elidedText(plain, Qt::ElideMiddle, availableWidth));
}

// ============================================================================
// MixerSlider
// ============================================================================

namespace StreamUP {
namespace Mixer {

MixerSlider::MixerSlider(obs_fader_t *fader, Qt::Orientation orientation, QWidget *parent)
	: QSlider(orientation, parent),
	  m_fader(fader)
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
}

void MixerSlider::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}

void MixerSlider::keyPressEvent(QKeyEvent *event)
{
	// OBS's slider leaves Up and Down to scrolling the mixer.
	if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
		event->ignore();
		return;
	}
	QSlider::keyPressEvent(event);
}

int MixerSlider::posToRangeValue(QMouseEvent *event)
{
	QStyleOptionSlider opt;
	initStyleOption(&opt);

	const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	int pos, sliderMin, sliderMax;
	const QPoint p = event->position().toPoint();
	if (orientation() == Qt::Horizontal) {
		pos = p.x();
		sliderMin = groove.left() + (handle.width() / 2);
		sliderMax = groove.right() - (handle.width() / 2) + 1;
	} else {
		pos = p.y();
		sliderMin = groove.top() + (handle.height() / 2);
		sliderMax = groove.bottom() - (handle.height() / 2) + 1;
	}
	return style()->sliderValueFromPosition(minimum(), maximum(), pos - sliderMin, sliderMax - sliderMin,
						opt.upsideDown);
}

void MixerSlider::mousePressEvent(QMouseEvent *event)
{
	m_dragging = (event->buttons() & Qt::LeftButton) || (event->buttons() & Qt::MiddleButton);
	if (m_dragging) {
		setSliderDown(true);
		setValue(posToRangeValue(event));
		emit sliderMoved(posToRangeValue(event));
	}
	event->accept();
}

void MixerSlider::mouseReleaseEvent(QMouseEvent *event)
{
	m_dragging = false;
	setSliderDown(false);
	event->accept();
}

void MixerSlider::mouseMoveEvent(QMouseEvent *event)
{
	if (m_dragging) {
		setValue(posToRangeValue(event));
		emit sliderMoved(posToRangeValue(event));
	}
	QSlider::mouseMoveEvent(event);
	event->accept();
}

void MixerSlider::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	// The native slider hard codes this tick colour too.
	const QColor tickColor(91, 98, 115, 255);

	obs_fader_conversion_t dbToDef = obs_fader_db_to_def(m_fader);

	QStyleOptionSlider opt;
	initStyleOption(&opt);
	QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
	QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

	if (orientation() == Qt::Horizontal) {
		const int sliderWidth = groove.width() - handle.width();
		float tickLength = groove.height() * 1.5f;
		tickLength = float(std::max((int)tickLength + groove.height(), 8 + groove.height()));
		float yPos = groove.center().y() - (tickLength / 2) + 1;

		for (int db = -10; db >= -90; db -= 10) {
			float tickValue = dbToDef(float(db));
			float xPos = groove.left() + (tickValue * sliderWidth) + (handle.width() / 2);
			painter.fillRect(QRectF(xPos, yPos, 1, tickLength), tickColor);
		}
	} else {
		const int sliderHeight = groove.height() - handle.height();
		float tickLength = groove.width() * 1.5f;
		tickLength = float(std::max((int)tickLength + groove.width(), 8 + groove.width()));
		float xPos = groove.center().x() - (tickLength / 2) + 1;

		for (int db = -10; db >= -96; db -= 10) {
			float tickValue = dbToDef(float(db));
			float yPos = groove.height() + groove.top() - (tickValue * sliderHeight) - (handle.height() / 2);
			painter.fillRect(QRectF(xPos, yPos, tickLength, 1), tickColor);
		}
	}
	painter.end();

	QSlider::paintEvent(event);
}

// ============================================================================
// StyleClass (idian equivalents)
// ============================================================================

namespace StyleClass {

void repolish(QWidget *widget)
{
	if (!widget->testAttribute(Qt::WA_WState_Polished))
		return;
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

bool add(QWidget *widget, const QString &name)
{
	QStringList list = widget->property("class").toString().split(' ', Qt::SkipEmptyParts);
	if (list.contains(name))
		return false;
	list.append(name);
	widget->setProperty("class", list.join(' '));
	repolish(widget);
	return true;
}

bool remove(QWidget *widget, const QString &name)
{
	QStringList list = widget->property("class").toString().split(' ', Qt::SkipEmptyParts);
	if (!list.contains(name))
		return false;
	list.removeAll(name);
	widget->setProperty("class", list.join(' '));
	repolish(widget);
	return true;
}

bool toggle(QWidget *widget, const QString &name, bool on)
{
	return on ? add(widget, name) : remove(widget, name);
}

void tintIcon(QAbstractButton *button)
{
	if (!button || button->icon().isNull())
		return;
	QStyleOptionButton opt;
	opt.initFrom(button);
	const QColor color = opt.palette.color(QPalette::ButtonText);
	QImage img = button->icon().pixmap(button->iconSize(), button->devicePixelRatioF(), QIcon::Normal).toImage();
	QPainter p(&img);
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.fillRect(img.rect(), color);
	p.end();
	QIcon tinted;
	tinted.addPixmap(QPixmap::fromImage(img), QIcon::Normal);
	tinted.addPixmap(QPixmap::fromImage(img), QIcon::Disabled);
	button->setIcon(tinted);
}

namespace {

class StateFilter : public QObject {
public:
	explicit StateFilter(QWidget *target) : QObject(target), m_target(target)
	{
		if (auto *button = qobject_cast<QAbstractButton *>(target)) {
			QObject::connect(button, &QAbstractButton::toggled, this,
					 [this](bool checked) { toggle(m_target, "checked", checked); });
		}
	}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override
	{
		auto *widget = qobject_cast<QWidget *>(obj);
		if (!widget)
			return QObject::eventFilter(obj, event);

		bool recolour = true;
		switch (event->type()) {
		case QEvent::FocusIn:
			toggle(widget, "focus", true);
			break;
		case QEvent::FocusOut:
			toggle(widget, "focus", false);
			break;
		case QEvent::HoverEnter:
			if (widget->isEnabled())
				toggle(widget, "hover", true);
			break;
		case QEvent::HoverLeave:
			toggle(widget, "hover", false);
			break;
		case QEvent::EnabledChange:
			toggle(widget, "disabled", !widget->isEnabled());
			break;
		case QEvent::StyleChange:
		case QEvent::PaletteChange:
			break;
		default:
			recolour = false;
			break;
		}
		if (recolour) {
			QPointer<QWidget> guard(widget);
			QTimer::singleShot(0, this, [guard]() {
				if (guard)
					tintIcon(qobject_cast<QAbstractButton *>(guard.data()));
			});
		}
		return QObject::eventFilter(obj, event);
	}

private:
	QWidget *m_target;
};

} // namespace

void installStateFilter(QWidget *widget)
{
	widget->setAttribute(Qt::WA_Hover, true);
	widget->installEventFilter(new StateFilter(widget));
}

} // namespace StyleClass

} // namespace Mixer
} // namespace StreamUP
