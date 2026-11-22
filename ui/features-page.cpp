#include "features-page.hpp"

#include "../core/features.hpp"
#include "overlay-ui-common.hpp"
#include "settings-manager.hpp"
#include "ui-helpers.hpp"
#include "version.h"

#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/switch-button.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <QApplication>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace su = StreamUP::UIStyles;
namespace ft = StreamUP::Features;
using StreamUP::OverlayUI::T;

namespace StreamUP {
namespace FeaturesPage {

namespace {

constexpr int kCardW = 296; // design px; three to a row
constexpr int kDemoH = 150; // a touch under 16:9 of the card's inner width

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

QString DemoFile(const QString &id, const char *ext)
{
	const QByteArray rel = QString("demos/%1.%2").arg(id, QString::fromLatin1(ext)).toUtf8();
	char *path = obs_module_file(rel.constData());
	const QString p = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	return QFileInfo::exists(p) ? p : QString();
}

// The demo at the top of a card: an animated WebP or GIF when there is one
// (OBS has no Qt Multimedia, so no WebM), a still PNG otherwise, a quiet
// placeholder when there is neither. Animations only run while the card is on
// screen; the page starts and stops them as you scroll.
class DemoView : public QWidget {
public:
	explicit DemoView(const QString &id) : m_id(id)
	{
		setFixedHeight(su::S(kDemoH));
		setAttribute(Qt::WA_OpaquePaintEvent, false);
		QString anim = DemoFile(id, "webp");
		if (anim.isEmpty())
			anim = DemoFile(id, "gif");
		if (!anim.isEmpty()) {
			m_movie = new QMovie(anim, QByteArray(), this);
			m_movie->setCacheMode(QMovie::CacheNone);
			QObject::connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
			m_movie->jumpToFrame(0);
		}
		const QString still = DemoFile(id, "png");
		if (!still.isEmpty())
			m_still.load(still);
	}

	void setPlaying(bool on)
	{
		if (!m_movie)
			return;
		if (on && m_movie->state() != QMovie::Running)
			m_movie->start();
		else if (!on && m_movie->state() == QMovie::Running)
			m_movie->setPaused(true);
	}
	bool animated() const { return m_movie != nullptr; }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setRenderHint(QPainter::SmoothPixmapTransform, true);
		const QRectF r = QRectF(rect());
		QPainterPath clip;
		clip.addRoundedRect(r, su::S(12), su::S(12));
		p.setClipPath(clip);
		p.fillRect(r, QColor(su::Colors::BG_SECONDARY));
		QImage frame = m_movie ? m_movie->currentImage() : m_still;
		if (frame.isNull() && m_movie)
			frame = m_still;
		if (!frame.isNull()) {
			// Cover: fill the box, crop the overflow evenly.
			const QSizeF s = QSizeF(frame.size()).scaled(r.size(), Qt::KeepAspectRatioByExpanding);
			p.drawImage(QRectF(r.center().x() - s.width() / 2, r.center().y() - s.height() / 2, s.width(), s.height()),
				    frame);
		} else {
			p.setPen(QColor(su::Colors::TEXT_MUTED));
			QFont f = qApp->font();
			f.setPixelSize(su::S(12));
			p.setFont(f);
			p.drawText(r, Qt::AlignCenter, T("Features.NoDemo"));
		}
	}

private:
	QString m_id;
	QMovie *m_movie = nullptr;
	QImage m_still;
};

struct Card {
	QString id;
	QFrame *frame = nullptr;
	DemoView *demo = nullptr;
	su::SwitchButton *toggle = nullptr;
	QLabel *note = nullptr; // "applies after restart"
};

class Page : public su::ShadowDialog {
public:
	explicit Page(QWidget *parent) : su::ShadowDialog(parent)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		auto shell = su::applyChrome(this, T("Features.Title"), "v" PROJECT_VERSION, /*brandFooter=*/true, "StreamUP");

		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
		su::useScrollBars(scroll);
		m_scroll = scroll;
		auto *page = new QWidget();
		page->setObjectName("ftPage");
		page->setStyleSheet(su::scale_qss(QString("QWidget#ftPage{background:%1;}"
							  "QFrame#ftCard{background:%2;border-radius:18px;}")
							  .arg(su::Colors::BG_DARKEST, su::Colors::BG_PRIMARY)));
		auto *v = new QVBoxLayout(page);
		v->setContentsMargins(su::S(20), su::S(16), su::S(20), su::S(20));
		v->setSpacing(su::S(10));
		scroll->setWidget(page);
		shell.content->addWidget(scroll, 1);

		QLabel *intro = su::makeLabel(T("Features.Intro"), su::CardText::kBody, 400, su::Colors::TEXT_SECONDARY);
		intro->setWordWrap(true);
		v->addWidget(intro);

		const QStringList fresh =
			ft::NewSince(ft::LastSeenVersion());
		for (ft::Group g : {ft::Group::Products, ft::Group::Docks, ft::Group::Mixer, ft::Group::Sources,
				    ft::Group::Hotkeys, ft::Group::Safety}) {
			QList<const ft::Feature *> features;
			for (const ft::Feature &f : ft::All())
				if (f.group == g)
					features.append(&f);
			if (features.isEmpty())
				continue;
			QLabel *heading = su::sectionHeader(ft::GroupLabel(g));
			v->addWidget(heading);
			auto *grid = new QGridLayout();
			grid->setHorizontalSpacing(su::S(14));
			grid->setVerticalSpacing(su::S(14));
			for (int i = 0; i < features.size(); ++i)
				grid->addWidget(buildCard(*features[i], fresh.contains(features[i]->id)), i / 3, i % 3,
						Qt::AlignTop | Qt::AlignLeft);
			grid->setColumnStretch(3, 1);
			v->addLayout(grid);
		}
		v->addStretch(1);

		m_status = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
		m_status->setWordWrap(false);
		shell.footer->insertWidget(0, m_status);
		m_status->hide();
		auto *done = new su::PillButton(T("OverlayControls.Button.Done"), "primary");
		shell.footerButtons->addWidget(done);
		QObject::connect(done, &QPushButton::clicked, this, &QDialog::close);

		// Demos play only while their card is on screen.
		m_visibility = new QTimer(this);
		m_visibility->setSingleShot(true);
		m_visibility->setInterval(80);
		QObject::connect(m_visibility, &QTimer::timeout, this, [this]() { updatePlaying(); });
		QObject::connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, m_visibility,
				 qOverload<>(&QTimer::start));

		const int m = su::S(su::ShadowDialog::kShadowMargin);
		resize(su::S(3 * kCardW + 2 * 14 + 2 * 20 + 24) + 2 * m, su::S(760) + 2 * m);

		// The page counts as seen once it has been opened.
		StreamUP::SettingsManager::PluginSettings s = StreamUP::SettingsManager::GetCurrentSettings();
		s.featuresVersionSeen = PROJECT_VERSION;
		StreamUP::SettingsManager::UpdateSettings(s);
	}

	std::function<void()> onClosed;

protected:
	void showEvent(QShowEvent *e) override
	{
		su::ShadowDialog::showEvent(e);
		m_visibility->start();
	}
	void hideEvent(QHideEvent *e) override
	{
		for (Card &c : m_cards)
			c.demo->setPlaying(false);
		su::ShadowDialog::hideEvent(e);
	}
	void closeEvent(QCloseEvent *e) override
	{
		if (onClosed) {
			auto cb = std::move(onClosed);
			onClosed = nullptr;
			QTimer::singleShot(0, qApp, cb);
		}
		su::ShadowDialog::closeEvent(e);
	}

private:
	QWidget *buildCard(const ft::Feature &f, bool isNew)
	{
		Card card;
		card.id = f.id;
		card.frame = new QFrame();
		card.frame->setObjectName("ftCard");
		card.frame->setFixedWidth(su::S(kCardW));
		auto *v = new QVBoxLayout(card.frame);
		v->setContentsMargins(su::S(12), su::S(12), su::S(12), su::S(12));
		v->setSpacing(su::S(8));

		card.demo = new DemoView(f.id);
		v->addWidget(card.demo);

		// A fixed-height row, so a card with a badge lines up with one without.
		auto *titleBox = new QWidget();
		titleBox->setFixedHeight(su::S(24));
		auto *titleRow = new QHBoxLayout(titleBox);
		titleRow->setContentsMargins(0, 0, 0, 0);
		titleRow->setSpacing(su::S(8));
		QLabel *name = su::makeLabel(T(f.nameKey.toUtf8().constData()), su::CardText::kTitle, 700, su::Colors::TEXT_PRIMARY);
		name->setWordWrap(false);
		titleRow->addWidget(name);
		if (isNew)
			titleRow->addWidget(new su::Badge(T("Features.New"), QColor(su::Colors::PRIMARY_COLOR), QColor("#ffffff")));
		if (f.core)
			titleRow->addWidget(new su::Badge(T("Features.Core"), QColor("#2a2a3a"), QColor(su::Colors::TEXT_SECONDARY)));
		titleRow->addStretch(1);
		v->addWidget(titleBox);

		QLabel *line = su::makeLabel(T(f.lineKey.toUtf8().constData()), su::CardText::kCaption, 400, su::Colors::TEXT_SECONDARY);
		line->setWordWrap(true);
		line->setMinimumHeight(QFontMetrics(line->font()).lineSpacing() * 2);
		line->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		v->addWidget(line);

		card.note = su::makeLabel(T("Features.AfterRestart"), su::Sizes::FONT_SIZE_TINY, 600, su::Colors::COLOR_WARNING);
		card.note->setWordWrap(true);
		v->addWidget(card.note);

		auto *actions = new QHBoxLayout();
		actions->setSpacing(su::S(10));
		auto *show = new su::PillButton(T("Features.ShowMe"), "neutral");
		show->setAccessibleName(T("Features.ShowMeAccessible").arg(name->text()));
		const QString id = f.id;
		QObject::connect(show, &QPushButton::clicked, this, [id]() { ft::ShowMe(id); });
		actions->addWidget(show);
		actions->addStretch(1);
		if (!f.core) {
			card.toggle = new su::SwitchButton();
			card.toggle->setChecked(ft::IsOn(f.id));
			card.toggle->setAccessibleName(name->text());
			actions->addWidget(card.toggle, 0, Qt::AlignVCenter);
			QObject::connect(card.toggle, &su::SwitchButton::toggled, this, [this, id](bool on) { flip(id, on); });
		}
		show->setEnabled(ft::IsActive(f.id));
		card.frame->setProperty("showButton", QVariant::fromValue<QObject *>(show));
		v->addLayout(actions);

		m_cards.append(card);
		refreshNote(m_cards.last());
		return card.frame;
	}

	Card *cardFor(const QString &id)
	{
		for (Card &c : m_cards)
			if (c.id == id)
				return &c;
		return nullptr;
	}

	void refreshNote(Card &c)
	{
		const ft::Feature *f = ft::Find(c.id);
		// A restart part whose switch no longer matches what is running.
		const bool pending = f && !f->live && ft::IsOn(c.id) != ft::IsActive(c.id);
		c.note->setText(ft::IsOn(c.id) ? T("Features.OnAfterRestart") : T("Features.OffAfterRestart"));
		c.note->setVisible(pending);
		if (auto *show = qobject_cast<QPushButton *>(c.frame->property("showButton").value<QObject *>()))
			show->setEnabled(ft::IsActive(c.id));
	}

	void flip(const QString &id, bool on)
	{
		Card *c = cardFor(id);
		if (!c)
			return;
		if (!on && id == "adjustment-layer") {
			// A source type: switching it off hides it in every scene that
			// uses it. Say so first, and never do it silently.
			const int scenes = ft::AdjustmentLayerSceneCount();
			if (scenes > 0) {
				QPointer<Page> self(this);
				const QSignalBlocker block(c->toggle);
				c->toggle->setChecked(true);
				su::confirm(this, T("Features.AdjustmentLayer.WarnTitle"),
					    (scenes == 1 ? T("Features.AdjustmentLayer.WarnOne")
							 : T("Features.AdjustmentLayer.WarnMany").arg(scenes)),
					    T("Features.TurnOff"), "danger", [self, id]() {
						    if (!self)
							    return;
						    ft::SetOn(id, false);
						    if (Card *cc = self->cardFor(id)) {
							    const QSignalBlocker b(cc->toggle);
							    cc->toggle->setChecked(false);
							    self->refreshNote(*cc);
						    }
					    });
				return;
			}
		}
		ft::SetOn(id, on);
		refreshNote(*c);
		showStatus(on ? T("Features.TurnedOn").arg(T(ft::Find(id)->nameKey.toUtf8().constData()))
			      : T("Features.TurnedOff").arg(T(ft::Find(id)->nameKey.toUtf8().constData())));
	}

	void updatePlaying()
	{
		if (!isVisible())
			return;
		const QRect view(m_scroll->viewport()->mapToGlobal(QPoint(0, 0)), m_scroll->viewport()->size());
		for (Card &c : m_cards) {
			if (!c.demo->animated())
				continue;
			const QRect r(c.demo->mapToGlobal(QPoint(0, 0)), c.demo->size());
			c.demo->setPlaying(view.intersects(r));
		}
	}

	void showStatus(const QString &text)
	{
		m_status->setText(text);
		m_status->show();
		QPointer<QLabel> label(m_status);
		QTimer::singleShot(4000, this, [label, text]() {
			if (label && label->text() == text)
				label->hide();
		});
	}

	QScrollArea *m_scroll = nullptr;
	QLabel *m_status = nullptr;
	QTimer *m_visibility = nullptr;
	QList<Card> m_cards;
};

QPointer<Page> &Open()
{
	static QPointer<Page> page;
	return page;
}

void ShowOnUiThread(std::function<void()> onDone)
{
	QPointer<Page> &existing = Open();
	if (existing) {
		existing->showNormal();
		existing->raise();
		existing->activateWindow();
		if (onDone)
			onDone();
		return;
	}
	auto *page = new Page(MainWindow());
	page->onClosed = std::move(onDone);
	existing = page;
	StreamUP::UIHelpers::PlaceOverMainWindow(page);
	page->show();
	page->raise();
	page->activateWindow();
}

} // namespace

void Show()
{
	QMetaObject::invokeMethod(qApp, []() { ShowOnUiThread(nullptr); }, Qt::QueuedConnection);
}

void ShowIfNeeded(std::function<void()> onDone)
{
	const QString seen = ft::LastSeenVersion();
	// New users (never seen it) and anyone this version gives a new part.
	if (seen.isEmpty() || !ft::NewSince(seen).isEmpty()) {
		ShowOnUiThread(std::move(onDone));
		return;
	}
	if (onDone)
		onDone();
}

} // namespace FeaturesPage
} // namespace StreamUP
