#include "streamup-kit.hpp"

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/svg-icon.hpp>
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/ui-scrollbar.hpp>

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
#include <QVariantAnimation>
#include <QScrollArea>
#include <QGraphicsOpacityEffect>
#include <algorithm>
#include <cmath>
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>

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
			g.setWeight(QFont::DemiBold);
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
						 "font-size: 12px;font-weight: 600;}"
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
	QLabel *name = su::makeLabel(label, su::Type::Body, su::Type::Regular, su::Colors::TEXT_PRIMARY);
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

RowList::RowList(QWidget *parent) : QFrame(parent)
{
	setObjectName("suRowList");
	setStyleSheet(su::scale_qss(QString("QFrame#suRowList{background:%1;border:1px solid " + QString(StreamUP::UIStyles::Colors::BORDER_SUBTLE) + ";border-radius:12px;}").arg(su::Colors::BG_PRIMARY)));
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	m_v = new QVBoxLayout(this);
	m_v->setContentsMargins(S(Space::CardX), S(Space::CardY), S(Space::CardX), S(Space::CardY));
	m_v->setSpacing(0);
}

QWidget *RowList::addRow(const QString &title, const QString &detail, const QList<QWidget *> &trailing, QWidget *leading,
			 const QString &detailTip)
{
	if (m_rows > 0) {
		auto *line = new QFrame(this);
		line->setFixedHeight(1);
		line->setStyleSheet(QString("background:%1;border:none;").arg(su::Colors::SEPARATOR));
		m_v->addWidget(line);
	}
	auto *row = new QWidget(this);
	row->setAttribute(Qt::WA_StyledBackground, false);
	auto *h = new QHBoxLayout(row);
	h->setContentsMargins(0, S(Space::Row), 0, S(Space::Row));
	h->setSpacing(S(Space::Gap + 2));
	if (leading)
		h->addWidget(leading, 0, Qt::AlignVCenter);
	auto *text = new QVBoxLayout();
	text->setContentsMargins(0, 0, 0, 0);
	text->setSpacing(S(2));
	QLabel *t = su::makeLabel(title, su::Sizes::FONT_SIZE_NORMAL, 700, su::Colors::TEXT_PRIMARY);
	t->setWordWrap(false);
	t->setStyleSheet(t->styleSheet() + "background:transparent;");
	text->addWidget(t);
	if (!detail.isEmpty()) {
		QLabel *d = su::makeLabel(detail, su::Sizes::FONT_SIZE_SMALL, 500, su::Colors::TEXT_MUTED);
		d->setWordWrap(true);
		d->setStyleSheet(d->styleSheet() + "background:transparent;");
		if (!detailTip.isEmpty())
			d->setToolTip(detailTip);
		text->addWidget(d);
	}
	h->addLayout(text, 1);
	for (QWidget *w : trailing)
		if (w)
			h->addWidget(w, 0, Qt::AlignVCenter);
	row->setAccessibleName(title);
	m_v->addWidget(row);
	++m_rows;
	return row;
}

QWidget *Note(Status status, const QString &text, QWidget *parent)
{
	const Look look = LookFor(status);
	auto *box = new QFrame(parent);
	box->setObjectName("suNote");
	const QColor fill = look.fill;
	box->setStyleSheet(su::scale_qss(QString("QFrame#suNote{background:rgba(%1,%2,%3,%4);border-radius:12px;}")
						 .arg(fill.red())
						 .arg(fill.green())
						 .arg(fill.blue())
						 .arg(fill.alpha())));
	box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	auto *h = new QHBoxLayout(box);
	h->setContentsMargins(S(14), S(10), S(14), S(10));
	h->setSpacing(S(10));
	auto *icon = new QLabel(box);
	icon->setStyleSheet("background:transparent;");
	const char *glyph = status == Status::Ok     ? "circle-check"
			    : status == Status::Warn ? "alert-triangle"
			    : status == Status::Bad  ? "circle-x"
						     : "info-circle";
	icon->setPixmap(su::svgIcon(QString::fromLatin1(glyph), look.ink, 18, qApp->devicePixelRatio()));
	h->addWidget(icon, 0, Qt::AlignTop);
	QLabel *l = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 500, su::Colors::TEXT_PRIMARY);
	l->setWordWrap(true);
	l->setStyleSheet(l->styleSheet() + "background:transparent;");
	h->addWidget(l, 1);
	box->setAccessibleName(text);
	return box;
}

namespace {

// Reports the height of its contents for the width it is given, so the window
// it sits in can be sized to fit and only scrolls when it has to.
class FitScrollArea : public QScrollArea {
public:
	using QScrollArea::QScrollArea;
	bool hasHeightForWidth() const override { return widget() != nullptr; }
	int heightForWidth(int w) const override
	{
		QWidget *c = widget();
		if (!c)
			return QScrollArea::heightForWidth(w);
		return c->hasHeightForWidth() ? c->heightForWidth(w) : c->sizeHint().height();
	}
	QSize sizeHint() const override
	{
		QWidget *c = widget();
		if (!c)
			return QScrollArea::sizeHint();
		const int w = std::max(width(), c->sizeHint().width());
		return QSize(c->sizeHint().width(), heightForWidth(w));
	}
	QSize minimumSizeHint() const override { return QSize(S(200), S(80)); }
};

std::function<void()> &OpenFeaturesHook()
{
	static std::function<void()> fn;
	return fn;
}

} // namespace

void SetOpenFeaturesHook(std::function<void()> fn)
{
	OpenFeaturesHook() = std::move(fn);
}

QVBoxLayout *ScrollingBody(QVBoxLayout *shellContent)
{
	auto *area = new FitScrollArea();
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	area->setStyleSheet("QScrollArea{background:transparent;border:none;}");
	area->viewport()->setAutoFillBackground(false);
	su::useScrollBars(area);
	QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
	sp.setHeightForWidth(true);
	area->setSizePolicy(sp);
	auto *inner = new QWidget();
	inner->setObjectName("suScrollBody");
	inner->setStyleSheet("QWidget#suScrollBody{background:transparent;}");
	auto *v = new QVBoxLayout(inner);
	v->setContentsMargins(S(Space::Page), S(Space::PageTop), S(Space::Page), S(Space::PageTop));
	v->setSpacing(S(Space::Block));
	area->setWidget(inner);
	shellContent->addWidget(area, 1);
	return v;
}

void FitToContent(QWidget *window, int designWidth)
{
	if (!window)
		return;
	const int shadow = 2 * S(su::ShadowDialog::kShadowMargin);
	const int w = S(designWidth) + shadow;
	QScreen *scr = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
	const int maxHeight = scr ? int(scr->availableGeometry().height() * 0.85) : S(800);
	// Already on screen with a scrolling body: grow or shrink by exactly the
	// difference between what the body holds now and what it shows. The
	// layouts' cached size hints are not trusted for this.
	if (window->isVisible() && window->width() == w) {
		for (QScrollArea *area : window->findChildren<QScrollArea *>()) {
			QWidget *inner = area->widget();
			if (!inner || inner->objectName() != QLatin1String("suScrollBody") || !inner->layout())
				continue;
			QLayout *il = inner->layout();
			il->invalidate();
			const int vw = area->viewport()->width();
			const int want = il->hasHeightForWidth() ? il->totalHeightForWidth(vw) : il->totalSizeHint().height();
			const int h = window->height() + (want - area->viewport()->height());
			window->resize(w, std::clamp(h, window->minimumHeight(), maxHeight));
			return;
		}
	}
	window->resize(w, window->height());
	// The layouts cache their children's sizes: after something inside a
	// scrolling body changed (a fold opened, a row appeared) those numbers
	// are stale until told otherwise.
	for (QScrollArea *area : window->findChildren<QScrollArea *>()) {
		if (QWidget *inner = area->widget()) {
			if (inner->layout())
				inner->layout()->invalidate();
			inner->updateGeometry();
		}
		area->updateGeometry();
	}
	int h = window->sizeHint().height();
	if (QLayout *l = window->layout()) {
		l->invalidate();
		l->activate();
		h = l->hasHeightForWidth() ? l->totalHeightForWidth(w) : l->totalSizeHint().height();
	}
	QScreen *screen = window->screen() ? window->screen() : QGuiApplication::primaryScreen();
	const int maxH = screen ? int(screen->availableGeometry().height() * 0.85) : S(800);
	window->resize(w, std::clamp(h, window->minimumHeight(), maxH));
}

QWidget *SwitchedOffBanner(const QString &featureName)
{
	auto *box = new QFrame();
	box->setObjectName("suOffBanner");
	box->setStyleSheet(su::scale_qss(QString("QFrame#suOffBanner{background:%1;border:1px solid " + QString(StreamUP::UIStyles::Colors::BORDER_SUBTLE) + ";"
						 "border-radius:12px;}")
						 .arg(su::Colors::BG_PRIMARY)));
	box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	auto *h = new QHBoxLayout(box);
	h->setContentsMargins(S(14), S(10), S(10), S(10));
	h->setSpacing(S(10));
	auto *icon = new QLabel(box);
	icon->setStyleSheet("background:transparent;border:none;");
	icon->setPixmap(su::svgIcon("eye-off", QColor(su::Colors::TEXT_MUTED), 18, qApp->devicePixelRatio()));
	h->addWidget(icon);
	QLabel *l = su::makeLabel(Tr("Kit.SwitchedOff").arg(featureName), su::Sizes::FONT_SIZE_SMALL, 600,
				  su::Colors::TEXT_SECONDARY);
	l->setWordWrap(true);
	l->setStyleSheet(l->styleSheet() + "background:transparent;border:none;");
	h->addWidget(l, 1);
	auto *open = new su::PillButton(Tr("Kit.SwitchedOff.Open"), "neutral");
	QObject::connect(open, &QPushButton::clicked, box, []() {
		if (OpenFeaturesHook())
			OpenFeaturesHook()();
	});
	h->addWidget(open);
	return box;
}

namespace {

// A veil over switched-off settings. Painted on top rather than with an
// opacity effect: an effect renders the text into an offscreen picture first,
// which loses ClearType and makes it soft.
class DimVeil : public QWidget {
public:
	explicit DimVeil(QWidget *over) : QWidget(over)
	{
		setObjectName("suDimVeil");
		over->installEventFilter(this);
		setGeometry(over->rect());
		raise();
		show();
	}

protected:
	bool eventFilter(QObject *o, QEvent *e) override
	{
		if (o == parent() && (e->type() == QEvent::Resize || e->type() == QEvent::ChildAdded)) {
			setGeometry(parentWidget()->rect());
			raise();
		}
		return QWidget::eventFilter(o, e);
	}
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		QColor c(su::Colors::BG_DARKEST);
		c.setAlpha(150);
		p.fillRect(rect(), c);
	}
};

} // namespace

void DimForSwitchedOff(QWidget *content, bool off)
{
	if (!content)
		return;
	content->setEnabled(!off);
	QWidget *veil = content->findChild<QWidget *>("suDimVeil", Qt::FindDirectChildrenOnly);
	if (off && !veil)
		new DimVeil(content);
	else if (!off && veil)
		veil->deleteLater();
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
						      "font-size: 13px;font-weight:600;padding:4px 2px;}"
						      "QPushButton:hover{color:%2;}")
						      .arg(su::Colors::TEXT_SECONDARY, su::Colors::TEXT_PRIMARY)));
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
		if (m_onToggled)
			m_onToggled(open);
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

const char *ToastGlyph(Status s)
{
	switch (s) {
	case Status::Ok:
		return "circle-check";
	case Status::Warn:
		return "alert-triangle";
	case Status::Bad:
		return "circle-x";
	case Status::Info:
	case Status::Neutral:
	default:
		return "info-circle";
	}
}

// A rounded pill: icon, one short line. Fades and rises a few pixels in, fades
// out. Never takes focus.
class ToastWidget : public QWidget {
public:
	ToastWidget(const QString &text, Status s, const QString &iconPath, QWidget *anchor)
		: QWidget(anchor, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
	{
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_ShowWithoutActivating);
		setAttribute(Qt::WA_DeleteOnClose);
		setAccessibleName(text);
		auto *h = new QHBoxLayout(this);
		h->setContentsMargins(S(14), S(9), S(18), S(9));
		h->setSpacing(S(9));
		auto *icon = new QLabel(this);
		icon->setStyleSheet("background:transparent;");
		if (!iconPath.isEmpty())
			icon->setPixmap(QIcon(iconPath).pixmap(S(18), S(18)));
		else
			icon->setPixmap(su::svgIcon(QString::fromLatin1(ToastGlyph(s)), LookFor(s).ink, 18,
						    qApp->devicePixelRatio()));
		h->addWidget(icon);
		QLabel *label = su::makeLabel(text, su::Sizes::FONT_SIZE_NORMAL, 600, su::Colors::TEXT_PRIMARY);
		label->setStyleSheet(label->styleSheet() + "background:transparent;");
		label->setWordWrap(false);
		h->addWidget(label);
		adjustSize();
	}

	// Fade in while rising S(8) into place.
	void showAt(const QPoint &home)
	{
		m_home = home;
		setWindowOpacity(0.0);
		move(home + QPoint(0, S(8)));
		show();
		animate(true);
	}

	void dismiss()
	{
		if (m_leaving)
			return;
		m_leaving = true;
		animate(false);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
		QPainterPath path;
		path.addRoundedRect(r, r.height() / 2, r.height() / 2);
		p.fillPath(path, QColor(su::Colors::POPUP_BG));
		p.setPen(QPen(QColor(255, 255, 255, 28), 1));
		p.drawPath(path);
	}

private:
	void animate(bool in)
	{
		auto *a = new QVariantAnimation(this);
		a->setDuration(in ? 180 : 160);
		a->setStartValue(in ? 0.0 : 1.0);
		a->setEndValue(in ? 1.0 : 0.0);
		a->setEasingCurve(in ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
		QObject::connect(a, &QVariantAnimation::valueChanged, this, [this, in](const QVariant &v) {
			const double t = v.toDouble();
			setWindowOpacity(t);
			if (in)
				move(m_home + QPoint(0, qRound((1.0 - t) * S(8))));
		});
		if (!in)
			QObject::connect(a, &QVariantAnimation::finished, this, [this]() { close(); });
		a->start(QAbstractAnimation::DeleteWhenStopped);
	}

	QPoint m_home;
	bool m_leaving = false;
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
	t->showAt(QPoint(area.right() - t->width() - S(24), area.bottom() - t->height() - S(56)));
	QPointer<ToastWidget> guard(t);
	QTimer::singleShot(2600, t, [guard]() {
		if (guard)
			guard->dismiss();
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

QString NavListQss()
{
	return su::scale_qss(QString("QListWidget{background-color:transparent;border:none;font-size:13px;outline:none;}"
				     "QListWidget::item{padding:9px 14px;margin:1px 0;border:none;border-radius:10px;"
				     "color:%1;background:transparent;}"
				     "QListWidget::item:selected{background-color:rgba(255,255,255,0.09);color:%2;}"
				     "QListWidget::item:hover:!selected{background-color:rgba(255,255,255,0.04);color:%2;}")
				     .arg(su::Colors::TEXT_SECONDARY)
				     .arg(su::Colors::TEXT_PRIMARY));
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
	card->setStyleSheet(su::scale_qss(QString("QFrame#suMember{background:%1;border:1px solid " + QString(StreamUP::UIStyles::Colors::BORDER_SUBTLE) + ";"
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
		logo->setFixedSize(S(22), S(22));
		logo->setPixmap(QIcon(":images/icons/social/streamup-logo-button.svg").pixmap(QSize(S(22), S(22)), qApp->devicePixelRatio()));
		h->addWidget(logo, 0, Qt::AlignVCenter);
		QLabel *t = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 600, su::Colors::TEXT_SECONDARY);
		t->setWordWrap(false);
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
	logo->setFixedSize(S(30), S(30));
	logo->setPixmap(QIcon(":images/icons/social/streamup-logo-button.svg").pixmap(QSize(S(30), S(30)), qApp->devicePixelRatio()));
	top->addWidget(logo, 0, Qt::AlignVCenter);
	QLabel *title = su::makeLabel(Tr("Kit.Membership.Name"), su::Sizes::FONT_SIZE_HEADING, 700, su::Colors::TEXT_PRIMARY);
	title->setStyleSheet(title->styleSheet() + "background:transparent;border:none;");
	top->addWidget(title, 1);
	v->addLayout(top);
	QLabel *t = su::makeLabel(text, su::Sizes::FONT_SIZE_SMALL, 500, su::Colors::TEXT_SECONDARY);
	// One short line: no wrapping, so the card is always wide enough for it
	// (a wrapped label in a centred card was measured too short and clipped).
	t->setWordWrap(false);
	t->setStyleSheet(t->styleSheet() + "background:transparent;border:none;");
	v->addWidget(t);
	card->setMinimumWidth(S(340));
	auto *h = new QHBoxLayout();
	h->addWidget(button);
	h->addStretch(1);
	v->addLayout(h);
	return card;
}

// ---------------------------------------------------------------------------

PreviewSlot::PreviewSlot(const QString &folder, const QString &id, int height, const QString &placeholder, QWidget *parent)
	: QWidget(parent),
	  m_placeholder(placeholder.isEmpty() ? Tr("Kit.Preview.Soon") : placeholder),
	  m_folder(folder),
	  m_id(id)
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
	QImage frame = m_movie ? m_movie->currentImage() : m_still;
	if (frame.isNull() && m_movie)
		frame = m_still;
	// A framed slot gets its own surface; an unframed picture (the Welcome
	// hero, drawn with a transparent background) sits on the window itself,
	// so it matches whichever theme colours are on.
	if (m_outline || frame.isNull())
		p.fillRect(r, QColor(su::Colors::BG_SECONDARY));
	if (!frame.isNull()) {
		const QSizeF s = QSizeF(frame.size()).scaled(r.size(), m_fit ? Qt::KeepAspectRatio
									    : Qt::KeepAspectRatioByExpanding);
		// Scale once to the exact device pixels with a proper (area-averaged)
		// filter, then draw 1:1. Drawing a 2x picture straight into a small
		// rect only gets bilinear filtering, which looks rough and shimmers.
		const qreal dpr = devicePixelRatioF();
		const QSize px(qRound(s.width() * dpr), qRound(s.height() * dpr));
		const qint64 key = frame.cacheKey();
		if (m_scaled.isNull() || m_scaledKey != key || m_scaled.size() != px) {
			m_scaled = frame.size() == px ? frame : frame.scaled(px, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			m_scaled.setDevicePixelRatio(dpr);
			m_scaledKey = key;
		}
		p.drawImage(QPointF(std::round(r.center().x() - s.width() / 2), std::round(r.center().y() - s.height() / 2)),
			    m_scaled);
		if (m_outline) {
			p.setClipping(false);
			p.setPen(QPen(QColor(255, 255, 255, m_zoom && underMouse() ? 80 : 30), 1));
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), S(12), S(12));
		}
		return;
	}
	p.setPen(QColor(su::Colors::TEXT_MUTED));
	QFont f = qApp->font();
	f.setPixelSize(S(12));
	p.setFont(f);
	p.drawText(r, Qt::AlignCenter, m_placeholder);
}

void PreviewSlot::setNaturalAspect()
{
	m_natural = true;
	m_fit = true;
	setMinimumHeight(0);
	setMaximumHeight(QWIDGETSIZE_MAX);
	QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
	sp.setHeightForWidth(true);
	setSizePolicy(sp);
	updateGeometry();
}

int PreviewSlot::heightForWidth(int w) const
{
	QSize size = m_still.size();
	if (m_movie && !m_movie->currentImage().isNull())
		size = m_movie->currentImage().size();
	if (size.isEmpty())
		return S(240);
	return w * size.height() / size.width();
}

void PreviewSlot::setZoom(const QString &title, const QString &line)
{
	m_zoom = hasPicture();
	setAttribute(Qt::WA_Hover, m_zoom);
	m_zoomTitle = title;
	m_zoomLine = line;
	setCursor(m_zoom ? Qt::PointingHandCursor : Qt::ArrowCursor);
	if (m_zoom)
		setToolTip(Tr("Kit.Preview.Zoom"));
}

void PreviewSlot::mouseReleaseEvent(QMouseEvent *e)
{
	if (m_zoom && e->button() == Qt::LeftButton && rect().contains(e->pos())) {
		Lightbox(window(), m_folder, m_id, m_zoomTitle, m_zoomLine);
		return;
	}
	QWidget::mouseReleaseEvent(e);
}

namespace {

class LightboxOverlay : public QWidget {
public:
	LightboxOverlay(QWidget *window, const QString &folder, const QString &id, const QString &title,
			const QString &line, const QList<QWidget *> &buttons)
		: QWidget(window),
		  m_window(window)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		setFocusPolicy(Qt::StrongFocus);
		setAccessibleName(title);
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(S(40), S(28), S(40), S(28));
		v->setSpacing(S(12));
		auto *top = new QHBoxLayout();
		top->addStretch(1);
		auto *x = new su::CloseButton(this);
		x->setAccessibleName(Tr("Kit.Lightbox.Close"));
		QObject::connect(x, &QAbstractButton::clicked, this, [this]() { dismiss(); });
		top->addWidget(x);
		v->addLayout(top);
		v->addStretch(1);
		m_pic = new PreviewSlot(folder, id, 200, QString(), this);
		m_pic->setFit(true);
		m_pic->setOutline(false);
		v->addWidget(m_pic, 0, Qt::AlignHCenter);
		if (!title.isEmpty()) {
			QLabel *t = su::makeLabel(title, su::Sizes::FONT_SIZE_LARGE, 700, su::Colors::TEXT_PRIMARY);
			t->setAlignment(Qt::AlignHCenter);
			t->setStyleSheet(t->styleSheet() + "background:transparent;");
			v->addWidget(t);
		}
		if (!line.isEmpty()) {
			QLabel *l = su::makeLabel(line, su::Sizes::FONT_SIZE_NORMAL, 500, su::Colors::TEXT_SECONDARY);
			l->setAlignment(Qt::AlignHCenter);
			l->setWordWrap(true);
			l->setStyleSheet(l->styleSheet() + "background:transparent;");
			v->addWidget(l);
		}
		if (!buttons.isEmpty()) {
			auto *row = new QHBoxLayout();
			row->setSpacing(S(10));
			row->addStretch(1);
			for (QWidget *b : buttons) {
				b->setParent(this);
				row->addWidget(b);
				if (auto *pb = qobject_cast<QPushButton *>(b))
					QObject::connect(pb, &QPushButton::clicked, this, [this]() { dismiss(); });
			}
			row->addStretch(1);
			v->addLayout(row);
		}
		v->addStretch(1);
		window->installEventFilter(this);
		place();
		m_pic->setPlaying(true);

		fade(true);
		show();
		raise();
		setFocus();
	}

	void dismiss()
	{
		if (m_leaving)
			return;
		m_leaving = true;
		m_pic->setPlaying(false);
		fade(false);
	}

protected:
	bool eventFilter(QObject *o, QEvent *e) override
	{
		if (o == m_window && e->type() == QEvent::Resize)
			place();
		return QWidget::eventFilter(o, e);
	}
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		QPainterPath path;
		path.addRoundedRect(QRectF(rect()), S(18), S(18));
		p.fillPath(path, QColor(10, 10, 16, 238));
	}
	void mousePressEvent(QMouseEvent *e) override
	{
		if (!m_pic->geometry().contains(e->pos()))
			dismiss();
		e->accept();
	}
	void keyPressEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Escape) {
			dismiss();
			return;
		}
		QWidget::keyPressEvent(e);
	}

private:
	void place()
	{
		QRect r = m_window->rect();
		if (dynamic_cast<su::ShadowDialog *>(m_window)) {
			const int m = S(su::ShadowDialog::kShadowMargin);
			r.adjust(m, m, -m, -m);
		}
		setGeometry(r);
		// The picture as big as fits, 16:9, leaving room for the words.
		const int maxW = std::max(S(200), r.width() - S(80));
		const int maxH = std::max(S(120), r.height() - S(220));
		int w = std::min(maxW, S(1100));
		int h = w * 9 / 16;
		if (h > maxH) {
			h = maxH;
			w = h * 16 / 9;
		}
		m_pic->setFixedSize(w, h);
	}
	void fade(bool in)
	{
		auto *fx = new QGraphicsOpacityEffect(this);
		fx->setOpacity(in ? 0.0 : 1.0);
		setGraphicsEffect(fx);
		QPointer<QGraphicsOpacityEffect> guard(fx);
		auto *a = new QVariantAnimation(this);
		a->setDuration(in ? 170 : 140);
		a->setStartValue(in ? 0.0 : 1.0);
		a->setEndValue(in ? 1.0 : 0.0);
		a->setEasingCurve(QEasingCurve::OutCubic);
		QObject::connect(a, &QVariantAnimation::valueChanged, this, [guard](const QVariant &v) {
			if (guard)
				guard->setOpacity(v.toDouble());
		});
		QObject::connect(a, &QVariantAnimation::finished, this, [this, in]() {
			if (in)
				setGraphicsEffect(nullptr); // paint the clip at full quality
			else
				close();
		});
		a->start(QAbstractAnimation::DeleteWhenStopped);
	}

	QWidget *m_window = nullptr;
	PreviewSlot *m_pic = nullptr;
	bool m_leaving = false;
};

} // namespace

QWidget *Lightbox(QWidget *window, const QString &folder, const QString &id, const QString &title, const QString &line,
		  const QList<QWidget *> &buttons)
{
	if (!window)
		return nullptr;
	return new LightboxOverlay(window, folder, id, title, line, buttons);
}

// ---------------------------------------------------------------------------
// Theme tokens

namespace Theme {
namespace {

using Vars = QHash<QString, QString>;

struct ThemeFile {
	QString id;
	QString extends;
	Vars vars;
};

// The StreamUP base theme plus The Andi variant (StreamUP_TheAndi.ovt), for
// when the active theme is not a StreamUP one (or its files cannot be read).
Vars BuiltIn()
{
	return {
		{"bg_darkest", "#0E0E0E"},
		{"bg_primary", "#1A1A1A"},
		{"bg_secondary", "#222222"},
		{"bg_tertiary", "#2E2E2E"},
		{"primary_color", "#D4A843"},
		{"primary_hover", "#E0B85A"},
		{"primary_light", "#EDD07E"},
		{"secondary_color", "#EDD07E"},
		{"text_primary", "#ffffff"},
		{"text_secondary", "rgba(255, 255, 255, 0.65)"},
		{"text_disabled", "rgba(255, 255, 255, 0.35)"},
		{"text_on_primary", "#1a1a1a"},
		{"color_warning", "#ff9f0a"},
		{"color_danger", "#ff453a"},
		{"border_subtle", "rgba(255, 255, 255, 0.07)"},
		{"border_soft", "rgba(255, 255, 255, 0.10)"},
		{"surface_subtle", "rgba(212, 168, 67, 0.08)"},
		{"menu_overlay", "rgba(16, 16, 16, 0.96)"},
		{"selection_bg", "rgba(212, 168, 67, 0.28)"},
	};
}

ThemeFile ReadThemeFile(const QString &path)
{
	ThemeFile t;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return t;
	QString text = QString::fromUtf8(f.readAll());
	text.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"), QRegularExpression::DotMatchesEverythingOption));
	static const QRegularExpression idRe(QStringLiteral("\\bid:\\s*'([^']+)'"));
	static const QRegularExpression extRe(QStringLiteral("\\bextends:\\s*'([^']+)'"));
	static const QRegularExpression varsRe(QStringLiteral("@OBSThemeVars\\s*\\{([^}]*)\\}"));
	static const QRegularExpression varRe(QStringLiteral("--([A-Za-z0-9_]+)\\s*:\\s*([^;]+);"));
	if (auto m = idRe.match(text); m.hasMatch())
		t.id = m.captured(1);
	if (auto m = extRe.match(text); m.hasMatch())
		t.extends = m.captured(1);
	auto blocks = varsRe.globalMatch(text);
	while (blocks.hasNext()) {
		auto vars = varRe.globalMatch(blocks.next().captured(1));
		while (vars.hasNext()) {
			auto v = vars.next();
			QString value = v.captured(2).trimmed();
			if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
				value = value.mid(1, value.size() - 2);
			t.vars.insert(v.captured(1), value);
		}
	}
	return t;
}

QStringList ThemeDirs()
{
	QStringList dirs;
	const QString app = QCoreApplication::applicationDirPath();
	dirs << QDir(app).absoluteFilePath(QStringLiteral("../../data/obs-studio/themes"))
	     << QDir(app).absoluteFilePath(QStringLiteral("../Resources/data/obs-studio/themes"))
	     << QDir(app).absoluteFilePath(QStringLiteral("../share/obs/obs-studio/themes"))
	     << QStringLiteral("/usr/share/obs/obs-studio/themes");
	// User themes live beside plugin_config in OBS's own config folder.
	if (char *cfg = obs_module_config_path("")) {
		dirs << QDir(QString::fromUtf8(cfg)).absoluteFilePath(QStringLiteral("../../themes"));
		bfree(cfg);
	}
	return dirs;
}

QColor ParseColour(const QString &value)
{
	static const QRegularExpression rgbaRe(QStringLiteral(
		"rgba?\\(\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*(?:,\\s*([\\d.]+)\\s*)?\\)"));
	if (auto m = rgbaRe.match(value); m.hasMatch()) {
		QColor c(int(m.captured(1).toDouble()), int(m.captured(2).toDouble()), int(m.captured(3).toDouble()));
		if (!m.captured(4).isEmpty())
			c.setAlphaF(float(m.captured(4).toDouble()));
		return c;
	}
	return QColor::fromString(value.trimmed());
}

QString Resolve(const Vars &vars, const QString &key, int depth = 0)
{
	const QString v = vars.value(key).trimmed();
	static const QRegularExpression ref(QStringLiteral("^var\\(--([A-Za-z0-9_]+)\\)$"));
	if (depth < 8)
		if (auto m = ref.match(v); m.hasMatch())
			return Resolve(vars, m.captured(1), depth + 1);
	return v;
}

// A translucent colour flattened onto a surface, as a solid #rrggbb.
QString Solid(const QColor &c, const QColor &over)
{
	const double a = c.alphaF();
	return QColor(int(std::lround(c.red() * a + over.red() * (1 - a))),
		      int(std::lround(c.green() * a + over.green() * (1 - a))),
		      int(std::lround(c.blue() * a + over.blue() * (1 - a))))
		.name();
}

QString Rgba(const QColor &c)
{
	return QStringLiteral("rgba(%1,%2,%3,%4)")
		.arg(c.red())
		.arg(c.green())
		.arg(c.blue())
		.arg(QString::number(c.alphaF(), 'f', 2));
}

QString g_source = QStringLiteral("built-in");
// The strings the shared tokens point at; kept alive for the session.
QHash<QString, QByteArray> g_store;

const char *Keep(const char *name, const QString &value)
{
	QByteArray &slot = g_store[QString::fromLatin1(name)];
	slot = value.toUtf8();
	return slot.constData();
}

} // namespace

QString TokenSource()
{
	return g_source;
}

void LoadTokens()
{
	Vars vars = BuiltIn();
	g_source = QStringLiteral("built-in");

	QString active;
	if (config_t *cfg = obs_frontend_get_user_config())
		active = QString::fromUtf8(config_get_string(cfg, "Appearance", "Theme"));

	if (active.startsWith(QStringLiteral("com.streamup."))) {
		QHash<QString, ThemeFile> themes;
		for (const QString &dir : ThemeDirs()) {
			const QDir d(dir);
			if (!d.exists())
				continue;
			for (const QString &name : d.entryList({QStringLiteral("*.obt"), QStringLiteral("*.ovt")}, QDir::Files)) {
				ThemeFile t = ReadThemeFile(d.absoluteFilePath(name));
				if (t.id.startsWith(QStringLiteral("com.streamup.")))
					themes.insert(t.id, t);
			}
		}
		// Walk from the variant up to the base, then apply the base first.
		QStringList chain;
		QString id = active;
		while (themes.contains(id) && chain.size() < 6 && !chain.contains(id)) {
			chain.prepend(id);
			id = themes.value(id).extends;
		}
		if (!chain.isEmpty()) {
			Vars merged;
			for (const QString &tid : chain) {
				const Vars &tv = themes[tid].vars;
				for (auto it = tv.cbegin(); it != tv.cend(); ++it)
					merged.insert(it.key(), it.value());
			}
			// Only take a variant whose surfaces and accent resolve.
			if (ParseColour(Resolve(merged, QStringLiteral("bg_primary"))).isValid() &&
			    ParseColour(Resolve(merged, QStringLiteral("primary_color"))).isValid()) {
				for (auto it = merged.cbegin(); it != merged.cend(); ++it)
					vars.insert(it.key(), it.value());
				g_source = active;
			}
		}
	}

	auto colour = [&vars](const char *key) {
		return ParseColour(Resolve(vars, QString::fromLatin1(key)));
	};
	const QColor darkest = colour("bg_darkest");
	const QColor primaryBg = colour("bg_primary");
	const QColor tertiary = colour("bg_tertiary");
	const QColor accent = colour("primary_color");
	QColor text = colour("text_primary");
	if (!text.isValid())
		text = QColor(Qt::white);

	namespace C = su::Colors;
	C::BG_DARKEST = Keep("BG_DARKEST", darkest.name());
	C::BG_PRIMARY = Keep("BG_PRIMARY", primaryBg.name());
	C::BG_SECONDARY = Keep("BG_SECONDARY", darkest.name());
	C::BG_TERTIARY = Keep("BG_TERTIARY", tertiary.isValid() ? tertiary.name() : primaryBg.lighter(130).name());
	C::SIDEBAR_BG = Keep("SIDEBAR_BG", darkest.name());
	C::INPUT_ON_BASE = Keep("INPUT_ON_BASE", primaryBg.name());
	C::INPUT_ON_CARD = Keep("INPUT_ON_CARD", darkest.name());

	C::PRIMARY_COLOR = Keep("PRIMARY_COLOR", accent.name());
	const QColor hover = colour("primary_hover");
	C::PRIMARY_HOVER = Keep("PRIMARY_HOVER", (hover.isValid() ? hover : accent.lighter(112)).name());
	C::PRIMARY_PRESSED = Keep("PRIMARY_PRESSED", accent.darker(115).name());
	const QColor light = colour("primary_light");
	C::PRIMARY_LIGHT = Keep("PRIMARY_LIGHT", (light.isValid() ? light : accent.lighter(130)).name());
	C::TAG_COLOR = C::PRIMARY_LIGHT;
	const QColor onAccent = colour("text_on_primary");
	C::TEXT_ON_PRIMARY = Keep("TEXT_ON_PRIMARY", (onAccent.isValid() ? onAccent : QColor(Qt::white)).name());

	if (const QColor w = colour("color_warning"); w.isValid())
		C::COLOR_WARNING = Keep("COLOR_WARNING", w.name());
	if (const QColor d = colour("color_danger"); d.isValid())
		C::COLOR_DANGER = Keep("COLOR_DANGER", d.name());

	C::TEXT_PRIMARY = Keep("TEXT_PRIMARY", text.name());
	QColor secondary = colour("text_secondary");
	if (!secondary.isValid()) {
		secondary = text;
		secondary.setAlphaF(0.65f);
	}
	C::TEXT_SECONDARY = Keep("TEXT_SECONDARY", Solid(secondary, primaryBg));
	QColor muted = text;
	muted.setAlphaF(0.45f);
	C::TEXT_MUTED = Keep("TEXT_MUTED", Solid(muted, primaryBg));
	QColor placeholder = colour("text_disabled");
	if (!placeholder.isValid()) {
		placeholder = text;
		placeholder.setAlphaF(0.35f);
	}
	C::TEXT_PLACEHOLDER = Keep("TEXT_PLACEHOLDER", Rgba(placeholder));

	QColor border = colour("border_subtle");
	if (!border.isValid())
		border = QColor(255, 255, 255, 20);
	C::BORDER_SUBTLE = Keep("BORDER_SUBTLE", Rgba(border));
	C::SEPARATOR = Keep("SEPARATOR", Solid(border, primaryBg));
	const QColor soft = colour("border_soft");
	C::POPUP_BORDER = Keep("POPUP_BORDER", Rgba(soft.isValid() ? soft : border));
	const QColor menu = colour("menu_overlay");
	C::POPUP_BG = Keep("POPUP_BG", menu.isValid() ? Solid(menu, darkest) : primaryBg.name());
	const QColor surface = colour("surface_subtle");
	C::HOVER_ROW = Keep("HOVER_ROW", Rgba(surface.isValid() ? surface : QColor(255, 255, 255, 13)));
	const QColor selection = colour("selection_bg");
	C::SELECTION = Keep("SELECTION", Rgba(selection.isValid() ? selection : accent));

	blog(LOG_INFO, "[StreamUP] UI colours from %s (accent %s, surface %s)", g_source.toUtf8().constData(),
	     C::PRIMARY_COLOR, C::BG_PRIMARY);
}

} // namespace Theme

} // namespace Kit
} // namespace StreamUP
