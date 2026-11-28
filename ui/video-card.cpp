#include "video-card.hpp"
#include "ui-helpers.hpp"
#include <streamup/ui/gallery-style.hpp>

#include <obs-module.h>

#include <QDesktopServices>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

namespace StreamUP {

namespace su = UIStyles;

namespace {

const QColor kYouTubeRed(0xFF, 0x00, 0x33);

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QFont Font(const QFont &base, int px, int weight)
{
	QFont f = base;
	f.setPixelSize(su::S(px));
	f.setWeight(QFont::Weight(weight));
	return f;
}

// The line under a video's title: where it lives and how long it is.
QString Detail(const Videos::Video &v)
{
	QString where = T(v.chapter ? "Videos.Chapter" : "Videos.OnYouTube");
	if (!v.length.isEmpty())
		where += QStringLiteral(" · ") + v.length;
	return where;
}

} // namespace

namespace VideoPaint {
void PlayBadge(QPainter &p, const QPointF &c, qreal w)
{
	const qreal h = w * 0.7;
	QPainterPath badge;
	badge.addRoundedRect(QRectF(c.x() - w / 2, c.y() - h / 2, w, h), h * 0.28, h * 0.28);
	p.fillPath(badge, kYouTubeRed);
	const qreal t = h * 0.36;
	QPainterPath tri;
	tri.moveTo(c.x() - t * 0.42, c.y() - t * 0.5);
	tri.lineTo(c.x() + t * 0.58, c.y());
	tri.lineTo(c.x() - t * 0.42, c.y() + t * 0.5);
	tri.closeSubpath();
	p.fillPath(tri, Qt::white);
}
} // namespace VideoPaint

// ---------------------------------------------------------------- VideoCard

VideoCard::VideoCard(const Videos::Video &video, Style style, QWidget *parent)
	: QPushButton(parent),
	  m_video(video),
	  m_style(style)
{
	setCursor(Qt::PointingHandCursor);
	setAttribute(Qt::WA_Hover, true);
	setFlat(true);
	setAccessibleName(T("Videos.Accessible").arg(video.title));
	setToolTip(video.url);
	if (style == Style::Hero) {
		// Its height follows its width (resizeEvent).
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		pinHeight(heightForWidth(su::S(640)));
	} else {
		setFixedWidth(sizeHint().width());
		pinHeight(sizeHint().height());
	}
	connect(this, &QPushButton::clicked, this, [this]() { open(); });
	Videos::OnChanged(this, [this]() { update(); });
}

QSize VideoCard::sizeHint() const
{
	if (m_style == Style::Compact)
		return QSize(su::S(420), su::S(96));
	return QSize(su::S(640), heightForWidth(su::S(640)));
}

QSize VideoCard::minimumSizeHint() const
{
	if (m_style == Style::Compact)
		return sizeHint();
	return QSize(su::S(360), heightForWidth(su::S(360)));
}

int VideoCard::heightForWidth(int w) const
{
	const int pad = su::S(12);
	const int thumbW = (w - pad * 2) * 52 / 100;
	return thumbW * 9 / 16 + pad * 2;
}

void VideoCard::resizeEvent(QResizeEvent *e)
{
	QPushButton::resizeEvent(e);
	if (m_style == Style::Hero) {
		const int h = heightForWidth(width());
		if (h != height())
			pinHeight(h);
	}
}

// OBS themes give QPushButton a height and max-height in their style sheet,
// which beats setFixedHeight, so the height is pinned in the style sheet too.
void VideoCard::pinHeight(int h)
{
	setStyleSheet(QStringLiteral("QPushButton{border:none;background:transparent;margin:0px;padding:0px;"
				     "min-height:%1px;max-height:%1px;height:%1px;}")
			      .arg(h));
	setFixedHeight(h);
}

QRect VideoCard::thumbRect() const
{
	const int pad = m_style == Style::Compact ? su::S(8) : su::S(12);
	const int h = height() - pad * 2;
	const int w = h * 16 / 9;
	return QRect(pad, pad, w, h);
}

void VideoCard::open()
{
	if (!m_video.url.isEmpty())
		QDesktopServices::openUrl(QUrl(m_video.url));
}

void VideoCard::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	const bool hot = underMouse() || hasFocus();

	// The card.
	QPainterPath card;
	card.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), su::S(12), su::S(12));
	p.fillPath(card, QColor(hot ? su::Colors::BG_TERTIARY : su::Colors::BG_PRIMARY));
	p.setPen(QPen(QColor(255, 255, 255, hot ? 40 : 18), 1));
	p.drawPath(card);

	// The thumbnail, or a plain panel until it has downloaded.
	const QRect tr = thumbRect();
	QPainterPath thumb;
	thumb.addRoundedRect(QRectF(tr), su::S(8), su::S(8));
	p.save();
	p.setClipPath(thumb);
	const QImage img = Videos::Thumbnail(m_video.videoId);
	if (img.isNull())
		p.fillRect(tr, QColor(su::Colors::BG_SECONDARY));
	else
		p.drawImage(tr, img);
	p.restore();
	VideoPaint::PlayBadge(p, QRectF(tr).center(), m_style == Style::Hero ? su::S(64) : su::S(40));
	if (!m_video.length.isEmpty()) {
		const QFont f = Font(font(), 11, 600);
		const QFontMetrics fm(f);
		const int w = fm.horizontalAdvance(m_video.length) + su::S(10);
		const int h = fm.height() + su::S(2);
		const QRect chip(tr.right() - w - su::S(6), tr.bottom() - h - su::S(6), w, h);
		QPainterPath cp;
		cp.addRoundedRect(QRectF(chip), su::S(4), su::S(4));
		p.fillPath(cp, QColor(0, 0, 0, 210));
		p.setFont(f);
		p.setPen(Qt::white);
		p.drawText(chip, Qt::AlignCenter, m_video.length);
	}

	// The words beside it.
	const int gap = m_style == Style::Compact ? su::S(14) : su::S(20);
	QRect text(tr.right() + gap, tr.top(), width() - tr.right() - gap - su::S(14), tr.height());
	if (m_style == Style::Compact) {
		const QFont tf = Font(font(), 13, 600);
		const QFont df = Font(font(), 12, 400);
		const QFontMetrics tfm(tf), dfm(df);
		int y = text.top() + (text.height() - tfm.height() - dfm.height() * 2 - su::S(8)) / 2;
		p.setFont(tf);
		p.setPen(QColor(su::Colors::TEXT_PRIMARY));
		p.drawText(QRect(text.left(), y, text.width(), tfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
			   tfm.elidedText(m_video.title, Qt::ElideRight, text.width()));
		y += tfm.height() + su::S(2);
		p.setFont(df);
		p.setPen(QColor(su::Colors::TEXT_SECONDARY));
		p.drawText(QRect(text.left(), y, text.width(), dfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
			   dfm.elidedText(Detail(m_video), Qt::ElideRight, text.width()));
		y += dfm.height() + su::S(6);
		QFont wf = Font(font(), 12, 600);
		p.setFont(wf);
		p.setPen(QColor(hot ? su::Colors::TEXT_PRIMARY : su::Colors::TEXT_SECONDARY));
		p.drawText(QRect(text.left(), y, text.width(), dfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
			   T("Videos.WatchOnYouTube") + QStringLiteral(" ↗"));
		return;
	}

	// Hero: a heading, a line, then a red Watch on YouTube pill.
	const QFont hf = Font(font(), 16, 700);
	const QFont lf = Font(font(), 13, 400);
	const QFont bf = Font(font(), 13, 600);
	const QFontMetrics hfm(hf), lfm(lf), bfm(bf);
	const QString line = T("WhatsNew.Video.Line");
	const QRect lineBox = lfm.boundingRect(QRect(0, 0, text.width(), 10000), Qt::TextWordWrap, line);
	const int pillH = su::S(34);
	const int blockH = hfm.height() + su::S(6) + lineBox.height() + su::S(14) + pillH;
	int y = text.top() + std::max(0, (text.height() - blockH) / 2);
	p.setFont(hf);
	p.setPen(QColor(su::Colors::TEXT_PRIMARY));
	p.drawText(QRect(text.left(), y, text.width(), hfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
		   hfm.elidedText(T("WhatsNew.Video.Heading"), Qt::ElideRight, text.width()));
	y += hfm.height() + su::S(6);
	p.setFont(lf);
	p.setPen(QColor(su::Colors::TEXT_SECONDARY));
	p.drawText(QRect(text.left(), y, text.width(), lineBox.height()), Qt::TextWordWrap, line);
	y += lineBox.height() + su::S(14);
	const QString label = T("Videos.WatchOnYouTube");
	const int pillW = bfm.horizontalAdvance(label) + su::S(48);
	const QRectF pill(text.left(), y, pillW, pillH);
	QPainterPath pp;
	pp.addRoundedRect(pill, pillH / 2.0, pillH / 2.0);
	p.fillPath(pp, hot ? kYouTubeRed.lighter(112) : kYouTubeRed);
	const qreal tx = pill.left() + su::S(16);
	const qreal ty = pill.center().y();
	const qreal ts = su::S(10);
	QPainterPath tri;
	tri.moveTo(tx, ty - ts / 2);
	tri.lineTo(tx + ts * 0.85, ty);
	tri.lineTo(tx, ty + ts / 2);
	tri.closeSubpath();
	p.fillPath(tri, Qt::white);
	p.setFont(bf);
	p.setPen(Qt::white);
	p.drawText(pill.adjusted(su::S(32), 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, label);
}

// ---------------------------------------------------------------- WatchLink

WatchLink::WatchLink(const Videos::Video &video, QWidget *parent) : QPushButton(parent), m_video(video)
{
	setCursor(Qt::PointingHandCursor);
	setAttribute(Qt::WA_Hover, true);
	setFlat(true);
	setStyleSheet(QStringLiteral("QPushButton{border:none;background:transparent;margin:0px;padding:0px;"
				     "min-height:%1px;max-height:%1px;height:%1px;}")
			      .arg(sizeHint().height()));
	setAccessibleName(T("Videos.Accessible").arg(video.title));
	setToolTip(video.url);
	setFixedSize(sizeHint());
	connect(this, &QPushButton::clicked, this, [this]() {
		if (!m_video.url.isEmpty())
			QDesktopServices::openUrl(QUrl(m_video.url));
	});
}

QSize WatchLink::sizeHint() const
{
	QFont f = font();
	f.setPixelSize(su::S(12));
	f.setWeight(QFont::DemiBold);
	QString text = T("WhatsNew.Watch");
	if (!m_video.length.isEmpty())
		text += QStringLiteral(" · ") + m_video.length;
	return QSize(su::S(26) + QFontMetrics(f).horizontalAdvance(text) + su::S(4), su::S(22));
}

void WatchLink::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	const bool hot = underMouse() || hasFocus();
	VideoPaint::PlayBadge(p, QPointF(su::S(10), height() / 2.0), su::S(18));
	QFont f = font();
	f.setPixelSize(su::S(12));
	f.setWeight(QFont::DemiBold);
	f.setUnderline(hot);
	p.setFont(f);
	p.setPen(QColor(hot ? su::Colors::TEXT_PRIMARY : su::Colors::TEXT_SECONDARY));
	QString text = T("WhatsNew.Watch");
	if (!m_video.length.isEmpty())
		text += QStringLiteral(" · ") + m_video.length;
	p.drawText(rect().adjusted(su::S(26), 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
}

} // namespace StreamUP
