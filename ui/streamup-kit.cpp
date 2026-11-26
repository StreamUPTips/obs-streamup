#include "streamup-kit.hpp"

#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>

#include <obs-module.h>

#include <QApplication>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
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

} // namespace

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


} // namespace Kit
} // namespace StreamUP
