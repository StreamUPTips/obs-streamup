#include "streamup-kit.hpp"

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/svg-icon.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace StreamUP {
namespace Kit {

namespace su = StreamUP::UIStyles;
using su::S;

namespace {

QString Tr(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

struct Look {
	QColor fill;
	QColor ink;
	QString glyph;
};

Look LookFor(Status s)
{
	switch (s) {
	case Status::Ok:
		return {QColor(101, 196, 102, 40), QColor("#a6e3a1"), QStringLiteral("✓")};
	case Status::Warn:
		return {QColor(250, 179, 135, 40), QColor("#fab387"), QStringLiteral("!")};
	case Status::Bad:
		return {QColor(227, 93, 85, 46), QColor("#f38ba8"), QStringLiteral("✕")};
	case Status::Info:
		return {QColor(137, 180, 250, 38), QColor("#89b4fa"), QStringLiteral("i")};
	case Status::Neutral:
	default:
		return {QColor(255, 255, 255, 18), QColor(su::Colors::TEXT_SECONDARY), QString()};
	}
}

class Pill : public QWidget {
public:
	Pill(Status s, const QString &text, QWidget *parent) : QWidget(parent), m_look(LookFor(s)), m_text(text)
	{
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		m_font = qApp->font();
		m_font.setPixelSize(S(12));
		m_font.setWeight(QFont::DemiBold);
	}
	QSize sizeHint() const override
	{
		const QFontMetrics fm(m_font);
		const int glyph = m_look.glyph.isEmpty() ? 0 : S(16);
		return QSize(fm.horizontalAdvance(m_text) + glyph + S(20), S(22));
	}
	QSize minimumSizeHint() const override { return sizeHint(); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect());
		p.setPen(Qt::NoPen);
		p.setBrush(m_look.fill);
		p.drawRoundedRect(r, r.height() / 2, r.height() / 2);
		p.setFont(m_font);
		p.setPen(m_look.ink);
		int x = S(10);
		if (!m_look.glyph.isEmpty()) {
			QFont g = m_font;
			g.setWeight(QFont::Black);
			p.setFont(g);
			p.drawText(QRect(x, 0, S(12), height()), Qt::AlignCenter, m_look.glyph);
			p.setFont(m_font);
			x += S(16);
		}
		p.drawText(QRect(x, 0, width() - x - S(8), height()), Qt::AlignVCenter | Qt::AlignLeft, m_text);
	}

private:
	Look m_look;
	QString m_text;
	QFont m_font;
};

QIcon ThemedIcon(const QString &path)
{
	return path.isEmpty() ? QIcon() : QIcon(path);
}

QString DataFile(const QString &folder, const QString &id, const char *ext)
{
	const QByteArray rel = QString("%1/%2.%3").arg(folder, id, QString::fromLatin1(ext)).toUtf8();
	char *path = obs_module_file(rel.constData());
	const QString p = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	return QFileInfo::exists(p) ? p : QString();
}

} // namespace

QWidget *StatusPill(Status status, const QString &text, QWidget *parent)
{
	return new Pill(status, text, parent);
}

QLabel *HelpDot(const QString &help, QWidget *parent)
{
	if (help.isEmpty())
		return nullptr;
	auto *dot = new QLabel(QStringLiteral("?"), parent);
	dot->setToolTip(help);
	dot->setAlignment(Qt::AlignCenter);
	dot->setFixedSize(S(16), S(16));
	dot->setCursor(Qt::WhatsThisCursor);
	dot->setStyleSheet(su::scale_qss(QString("QLabel{background:rgba(255,255,255,0.08);color:%1;border-radius:8px;"
						 "font-size:10px;font-weight:700;}"
						 "QLabel:hover{background:rgba(255,255,255,0.16);color:%2;}")
						 .arg(su::Colors::TEXT_MUTED, su::Colors::TEXT_PRIMARY)));
	dot->setAccessibleName(help);
	return dot;
}

QWidget *SettingRow(const QString &label, QWidget *control, const QString &help, const QString &iconPath)
{
	auto *row = new QWidget();
	row->setAttribute(Qt::WA_StyledBackground, false);
	auto *h = new QHBoxLayout(row);
	h->setContentsMargins(0, S(3), 0, S(3));
	h->setSpacing(S(8));
	if (!iconPath.isEmpty()) {
		auto *icon = new QLabel();
		icon->setPixmap(ThemedIcon(iconPath).pixmap(S(18), S(18)));
		icon->setFixedSize(S(20), S(20));
		h->addWidget(icon);
	}
	QLabel *name = su::makeLabel(label, su::Sizes::FONT_SIZE_NORMAL, 600, su::Colors::TEXT_PRIMARY);
	name->setWordWrap(false);
	h->addWidget(name);
	if (QLabel *dot = HelpDot(help))
		h->addWidget(dot);
	h->addStretch(1);
	if (control) {
		h->addWidget(control, 0, Qt::AlignVCenter);
		if (control->accessibleName().isEmpty())
			control->setAccessibleName(label);
	}
	return row;
}

// ---------------------------------------------------------------------------

MoreOptions::MoreOptions(const QString &key, const QString &label, QWidget *parent)
	: QWidget(parent),
	  m_key(key),
	  m_label(label.isEmpty() ? Tr("Kit.MoreOptions") : label)
{
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->setSpacing(S(4));
	m_header = new QPushButton(this);
	m_header->setCursor(Qt::PointingHandCursor);
	m_header->setFlat(true);
	m_header->setStyleSheet(su::scale_qss(QString("QPushButton{color:%1;background:transparent;border:none;text-align:left;"
						      "font-size:13px;font-weight:600;padding:4px 2px;}"
						      "QPushButton:hover{color:%2;}")
						      .arg(su::Colors::TAG_COLOR, su::Colors::TEXT_PRIMARY)));
	v->addWidget(m_header);
	m_body = new QWidget(this);
	m_bodyLayout = new QVBoxLayout(m_body);
	m_bodyLayout->setContentsMargins(0, 0, 0, 0);
	m_bodyLayout->setSpacing(S(6));
	v->addWidget(m_body);

	bool open = false;
	if (config_t *cfg = obs_frontend_get_user_config())
		open = config_get_bool(cfg, "StreamUP", ("More." + m_key).toUtf8().constData());
	setOpen(open, false);
	QObject::connect(m_header, &QPushButton::clicked, this, [this]() { setOpen(!m_open, true); });
}

void MoreOptions::setCountHint(int count)
{
	m_count = count;
	setOpen(m_open, false);
}

void MoreOptions::setOpen(bool open, bool save)
{
	m_open = open;
	m_body->setVisible(open);
	const QString arrow = open ? QStringLiteral("▾") : QStringLiteral("▸");
	m_header->setText(m_count >= 0 ? QString("%1  %2 (%3)").arg(arrow, m_label).arg(m_count)
				       : QString("%1  %2").arg(arrow, m_label));
	if (save) {
		if (config_t *cfg = obs_frontend_get_user_config())
			config_set_bool(cfg, "StreamUP", ("More." + m_key).toUtf8().constData(), open);
	}
}

// ---------------------------------------------------------------------------

QWidget *EmptyState(const QString &iconPath, const QString &line, const QString &button, std::function<void()> onClick,
		    QWidget *extra)
{
	auto *w = new QWidget();
	auto *v = new QVBoxLayout(w);
	v->setContentsMargins(S(16), S(28), S(16), S(28));
	v->setSpacing(S(10));
	v->addStretch(1);
	if (!iconPath.isEmpty()) {
		auto *icon = new QLabel();
		icon->setPixmap(ThemedIcon(iconPath).pixmap(S(40), S(40)));
		icon->setAlignment(Qt::AlignCenter);
		icon->setStyleSheet("background:transparent;");
		v->addWidget(icon);
	}
	QLabel *text = su::makeLabel(line, su::Sizes::FONT_SIZE_NORMAL, 600, su::Colors::TEXT_SECONDARY);
	text->setAlignment(Qt::AlignCenter);
	text->setWordWrap(true);
	v->addWidget(text);
	if (!button.isEmpty()) {
		auto *b = new su::PillButton(button, "primary");
		if (onClick)
			QObject::connect(b, &QPushButton::clicked, w, onClick);
		auto *h = new QHBoxLayout();
		h->addStretch(1);
		h->addWidget(b);
		h->addStretch(1);
		v->addLayout(h);
	}
	if (extra) {
		v->addSpacing(S(10));
		v->addWidget(extra, 0, Qt::AlignHCenter);
	}
	v->addStretch(1);
	return w;
}

// ---------------------------------------------------------------------------

namespace {

class ToastWidget : public QWidget {
public:
	ToastWidget(const QString &text, Status s, const QString &iconPath, QWidget *anchor)
		: QWidget(anchor, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus),
		  m_look(LookFor(s))
	{
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_ShowWithoutActivating);
		setAttribute(Qt::WA_DeleteOnClose);
		setAccessibleName(text);
		auto *h = new QHBoxLayout(this);
		h->setContentsMargins(S(16), S(10), S(18), S(10));
		h->setSpacing(S(10));
		auto *icon = new QLabel(this);
		if (!iconPath.isEmpty()) {
			icon->setPixmap(QIcon(iconPath).pixmap(S(18), S(18)));
		} else {
			icon->setText(m_look.glyph.isEmpty() ? QStringLiteral("•") : m_look.glyph);
			icon->setStyleSheet(QString("color:%1;font-weight:900;font-size:%2px;background:transparent;")
						    .arg(m_look.ink.name())
						    .arg(S(15)));
		}
		h->addWidget(icon);
		QLabel *label = su::makeLabel(text, su::Sizes::FONT_SIZE_NORMAL, 600, su::Colors::TEXT_PRIMARY);
		label->setStyleSheet(label->styleSheet() + "background:transparent;");
		label->setWordWrap(false);
		h->addWidget(label);
		adjustSize();
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		QPainterPath path;
		path.addRoundedRect(r, S(12), S(12));
		p.fillPath(path, QColor(su::Colors::POPUP_BG));
		p.setPen(QPen(QColor(255, 255, 255, 30), 1));
		p.drawPath(path);
		p.setClipPath(path);
		p.fillRect(QRectF(r.left(), r.top(), S(4), r.height()), m_look.ink);
	}

private:
	Look m_look;
};

QPointer<ToastWidget> &CurrentToast()
{
	static QPointer<ToastWidget> t;
	return t;
}

void ShowToastNow(const QString &text, Status s, const QString &iconPath)
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (QPointer<ToastWidget> &old = CurrentToast(); old)
		old->close();
	auto *t = new ToastWidget(text, s, iconPath, main);
	CurrentToast() = t;
	QRect area = main ? main->geometry() : QGuiApplication::primaryScreen()->availableGeometry();
	if (main && main->isMinimized())
		area = main->screen()->availableGeometry();
	t->move(area.right() - t->width() - S(24), area.bottom() - t->height() - S(56));
	t->show();
	QPointer<ToastWidget> guard(t);
	QTimer::singleShot(2400, t, [guard]() {
		if (guard)
			guard->close();
	});
}

} // namespace

void Toast(const QString &text, Status status, const QString &iconPath)
{
	QMetaObject::invokeMethod(
		qApp, [text, status, iconPath]() { ShowToastNow(text, status, iconPath); }, Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------

bool IsMember()
{
	// No sign-in exists in the plugin yet, so there is nothing to check.
	// When there is, members see a thank-you instead of the pitch.
	return false;
}

QString MembershipUrl()
{
	return QStringLiteral("https://streamup.tips/premium");
}

QString SupportUrl()
{
	return QStringLiteral("https://paypal.me/andilippi");
}

QWidget *MembershipCard(MemberStyle style, const QString &line)
{
	if (IsMember())
		return nullptr;
	auto *card = new QFrame();
	card->setObjectName("suMember");
	card->setStyleSheet(su::scale_qss(QString("QFrame#suMember{background:%1;border:1px solid rgba(137,180,250,0.22);"
						  "border-radius:%2px;}")
						  .arg(su::Colors::BG_PRIMARY)
						  .arg(style == MemberStyle::Card ? 16 : 12)));
	card->setAccessibleName(Tr("Kit.Membership.Name"));
	const QString text = line.isEmpty() ? Tr("Kit.Membership.Line") : line;
	auto *button = new su::PillButton(Tr("Kit.Membership.Button"), style == MemberStyle::Card ? "primary" : "outline");
	QObject::connect(button, &QPushButton::clicked, card, []() { QDesktopServices::openUrl(QUrl(MembershipUrl())); });
	auto *logo = new QLabel();
	logo->setStyleSheet("background:transparent;border:none;");

	if (style == MemberStyle::Strip) {
		auto *h = new QHBoxLayout(card);
		h->setContentsMargins(S(12), S(8), S(10), S(8));
		h->setSpacing(S(10));
		logo->setPixmap(QIcon(":images/icons/social/streamup-logo-button.svg").pixmap(S(20), S(20)));
		h->addWidget(logo);
		QLabel *t = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 600, su::Colors::TEXT_SECONDARY);
		t->setStyleSheet(t->styleSheet() + "background:transparent;border:none;");
		h->addWidget(t, 1);
		h->addWidget(button);
		return card;
	}

	auto *v = new QVBoxLayout(card);
	v->setContentsMargins(S(18), S(16), S(18), S(16));
	v->setSpacing(S(8));
	auto *top = new QHBoxLayout();
	top->setSpacing(S(10));
	logo->setPixmap(QIcon(":images/icons/social/streamup-logo-button.svg").pixmap(S(28), S(28)));
	top->addWidget(logo);
	QLabel *title = su::makeLabel(Tr("Kit.Membership.Name"), su::Sizes::FONT_SIZE_HEADING, 700, su::Colors::TEXT_PRIMARY);
	title->setStyleSheet(title->styleSheet() + "background:transparent;border:none;");
	top->addWidget(title, 1);
	v->addLayout(top);
	QLabel *t = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 500, su::Colors::TEXT_SECONDARY);
	t->setWordWrap(true);
	t->setStyleSheet(t->styleSheet() + "background:transparent;border:none;");
	v->addWidget(t);
	auto *h = new QHBoxLayout();
	h->addWidget(button);
	h->addStretch(1);
	v->addLayout(h);
	return card;
}

// ---------------------------------------------------------------------------

PreviewSlot::PreviewSlot(const QString &folder, const QString &id, int height, const QString &placeholder, QWidget *parent)
	: QWidget(parent),
	  m_placeholder(placeholder.isEmpty() ? Tr("Kit.Preview.Soon") : placeholder)
{
	setFixedHeight(S(height));
	QString anim = DataFile(folder, id, "webp");
	if (anim.isEmpty())
		anim = DataFile(folder, id, "gif");
	if (!anim.isEmpty()) {
		m_movie = new QMovie(anim, QByteArray(), this);
		m_movie->setCacheMode(QMovie::CacheNone);
		QObject::connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
		m_movie->jumpToFrame(0);
	}
	const QString still = DataFile(folder, id, "png");
	if (!still.isEmpty())
		m_still.load(still);
}

bool PreviewSlot::hasPicture() const
{
	return m_movie || !m_still.isNull();
}

void PreviewSlot::setPlaying(bool on)
{
	if (!m_movie)
		return;
	if (on && m_movie->state() != QMovie::Running)
		m_movie->start();
	else if (!on && m_movie->state() == QMovie::Running)
		m_movie->setPaused(true);
}

void PreviewSlot::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::SmoothPixmapTransform, true);
	const QRectF r = QRectF(rect());
	QPainterPath clip;
	clip.addRoundedRect(r, S(12), S(12));
	p.setClipPath(clip);
	p.fillRect(r, QColor(su::Colors::BG_SECONDARY));
	QImage frame = m_movie ? m_movie->currentImage() : m_still;
	if (frame.isNull() && m_movie)
		frame = m_still;
	if (!frame.isNull()) {
		const QSizeF s = QSizeF(frame.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
		p.drawImage(QRectF(r.center().x() - s.width() / 2, r.center().y() - s.height() / 2, s.width(), s.height()),
			    frame);
		return;
	}
	p.setPen(QColor(su::Colors::TEXT_MUTED));
	QFont f = qApp->font();
	f.setPixelSize(S(12));
	p.setFont(f);
	p.drawText(r, Qt::AlignCenter, m_placeholder);
}

} // namespace Kit
} // namespace StreamUP
