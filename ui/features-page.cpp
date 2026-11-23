#include "features-page.hpp"

#include "../core/features.hpp"
#include "overlay-ui-common.hpp"
#include "patch-notes-window.hpp"
#include "settings-manager.hpp"
#include "splash-screen.hpp"
#include "streamup-kit.hpp"
#include "theme-window.hpp"
#include "ui-helpers.hpp"
#include "version.h"

#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/svg-icon.hpp>
#include <streamup/ui/switch-button.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProcess>
#include <QScrollArea>
#include <QScrollBar>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <set>
#include <utility>

namespace su = StreamUP::UIStyles;
namespace ft = StreamUP::Features;
namespace kit = StreamUP::Kit;
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

QString Name(const ft::Feature &f)
{
	return T(f.nameKey.toUtf8().constData());
}

// Restarts OBS the way OBS does after a settings change that needs it: OBS
// closes normally (and saves), then starts again with the same arguments. A
// small hidden helper waits for this OBS to exit before starting the new one,
// and gives up after two minutes if OBS was not closed after all.
void RestartObs()
{
	const QString exe = QCoreApplication::applicationFilePath();
	const QString dir = QCoreApplication::applicationDirPath();
	QStringList args = QCoreApplication::arguments();
	if (!args.isEmpty())
		args.removeFirst();
	const qint64 pid = QCoreApplication::applicationPid();
#ifdef _WIN32
	auto quote = [](QString v) { return "'" + v.replace("'", "''") + "'"; };
	QStringList quoted;
	for (const QString &a : args)
		quoted << quote(a);
	QString script = QString("Wait-Process -Id %1 -Timeout 120 -ErrorAction SilentlyContinue; "
				 "if (-not (Get-Process -Id %1 -ErrorAction SilentlyContinue)) { "
				 "Start-Process -FilePath %2 -WorkingDirectory %3")
				 .arg(pid)
				 .arg(quote(QDir::toNativeSeparators(exe)), quote(QDir::toNativeSeparators(dir)));
	if (!quoted.isEmpty())
		script += " -ArgumentList " + quoted.join(',');
	script += " }";
	QProcess::startDetached("powershell.exe",
				{"-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-Command", script});
#else
	QString cmd = QString("while kill -0 %1 2>/dev/null; do sleep 0.3; done; cd \"%2\" && exec \"%3\"")
			      .arg(pid)
			      .arg(dir, exe);
	for (const QString &a : args)
		cmd += " '" + QString(a).replace("'", "'\\''") + "'";
	QProcess::startDetached("/bin/sh", {"-c", cmd});
#endif
	if (QWidget *main = MainWindow())
		QMetaObject::invokeMethod(main, "close", Qt::QueuedConnection);
}

struct Card {
	QString id;
	QFrame *frame = nullptr;
	kit::PreviewSlot *demo = nullptr;
	su::SwitchButton *toggle = nullptr;
	QLabel *note = nullptr; // "applies after restart"
};

// A page in the stack: its scroll area, so demos can play only while seen.
QScrollArea *MakeScroll(QWidget *content)
{
	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
	scroll->viewport()->setAutoFillBackground(false);
	su::useScrollBars(scroll);
	scroll->setWidget(content);
	return scroll;
}

class Page : public su::ShadowDialog {
public:
	explicit Page(QWidget *parent) : su::ShadowDialog(parent)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		m_shell = su::applyChrome(this, T("Features.Title"), "v" PROJECT_VERSION, /*brandFooter=*/true, "StreamUP");
		m_stack = new QStackedWidget();
		m_stack->setStyleSheet("QStackedWidget{background:transparent;}");
		m_shell.content->addWidget(m_stack, 1);

		m_status = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
		m_status->setWordWrap(false);
		m_shell.footer->insertWidget(0, m_status);
		m_status->hide();

		// Demos play only while their card is on screen.
		m_visibility = new QTimer(this);
		m_visibility->setSingleShot(true);
		m_visibility->setInterval(80);
		QObject::connect(m_visibility, &QTimer::timeout, this, [this]() { updatePlaying(); });

		const int m = su::S(su::ShadowDialog::kShadowMargin);
		resize(su::S(3 * kCardW + 2 * 14 + 2 * 20 + 24) + 2 * m, su::S(760) + 2 * m);

		// Restart parts already pending before this window opened do not
		// count towards the prompt when it closes.
		for (const ft::Feature &f : ft::All())
			if (!f.live && ft::IsOn(f.id) != ft::IsActive(f.id))
				m_pendingAtOpen.insert(f.id);
	}

	std::function<void()> onClosed;

	void showMode(Mode mode)
	{
		m_mode = mode;
		QWidget *page = nullptr;
		QString title;
		switch (mode) {
		case Mode::Welcome:
			page = m_stack->widget(indexOf(Mode::Welcome));
			title = T("Welcome.Title");
			break;
		case Mode::WhatsNew:
			page = m_stack->widget(indexOf(Mode::WhatsNew));
			title = T("WhatsNew.Title");
			break;
		case Mode::Features:
			page = m_stack->widget(indexOf(Mode::Features));
			title = T("Features.Title");
			// The page counts as seen once it has been opened.
			StreamUP::SettingsManager::PluginSettings s = StreamUP::SettingsManager::GetCurrentSettings();
			s.featuresVersionSeen = PROJECT_VERSION;
			StreamUP::SettingsManager::UpdateSettings(s);
			break;
		}
		m_stack->setCurrentWidget(page);
		setTitle(title);
		rebuildFooter();
		m_visibility->start();
	}

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
		su::ShadowDialog::closeEvent(e);
		askToRestart();
		if (onClosed) {
			auto cb = std::move(onClosed);
			onClosed = nullptr;
			QTimer::singleShot(0, qApp, cb);
		}
	}

private:
	// Pages are built on first use, so the window opens fast whichever page
	// it opens on.
	int indexOf(Mode mode)
	{
		int &slot = mode == Mode::Welcome ? m_welcomeIndex : mode == Mode::WhatsNew ? m_whatsNewIndex : m_featuresIndex;
		if (slot < 0) {
			QScrollArea *scroll = nullptr;
			if (mode == Mode::Welcome)
				scroll = MakeScroll(buildWelcome());
			else if (mode == Mode::WhatsNew)
				scroll = MakeScroll(PatchNotesWindow::BuildWhatsNewContent());
			else
				scroll = MakeScroll(buildFeatures());
			QObject::connect(scroll->verticalScrollBar(), &QScrollBar::valueChanged, m_visibility,
					 qOverload<>(&QTimer::start));
			if (mode == Mode::Features)
				m_featuresScroll = scroll;
			slot = m_stack->addWidget(scroll);
		}
		return slot;
	}

	void setTitle(const QString &title)
	{
		setWindowTitle(title);
		QString name = title;
		if (name.startsWith(QStringLiteral("StreamUP ")))
			name = name.mid(9);
		if (auto *label = findChild<QLabel *>("suTitle"))
			label->setText(QStringLiteral("StreamUP • ") + name + QStringLiteral(" (v" PROJECT_VERSION ")"));
	}

	void rebuildFooter()
	{
		while (QLayoutItem *item = m_shell.footerButtons->takeAt(0)) {
			if (QWidget *w = item->widget())
				w->deleteLater();
			delete item;
		}
		auto add = [this](const QString &text, const char *kind, std::function<void()> fn) {
			auto *b = new su::PillButton(text, kind);
			QObject::connect(b, &QPushButton::clicked, this, fn);
			m_shell.footerButtons->addWidget(b);
			return b;
		};
		switch (m_mode) {
		case Mode::Welcome:
			add(T("Welcome.PickFeatures"), "primary", [this]() { showMode(Mode::Features); });
			break;
		case Mode::WhatsNew:
			add(T("WhatsNew.Docs"), "neutral",
			    []() { QDesktopServices::openUrl(QUrl("https://streamup.doras.click/docs")); });
			add(T("WhatsNew.SeeFeatures"), "outline", [this]() { showMode(Mode::Features); });
			add(T("UI.Button.Done"), "primary", [this]() { close(); });
			break;
		case Mode::Features:
			add(T("OverlayControls.Button.Done"), "primary", [this]() { close(); });
			break;
		}
	}

	// First install: thank you, what StreamUP is in three lines, the
	// membership card, then on to picking features.
	QWidget *buildWelcome()
	{
		auto *page = new QWidget();
		page->setObjectName("wlPage");
		page->setStyleSheet("QWidget#wlPage{background:transparent;}");
		auto *v = new QVBoxLayout(page);
		v->setContentsMargins(su::S(28), su::S(22), su::S(28), su::S(22));
		v->setSpacing(su::S(14));

		QLabel *title = su::makeLabel(T("Welcome.Heading"), su::Type::Title, su::Type::Semibold, su::Colors::TEXT_PRIMARY);
		title->setAlignment(Qt::AlignHCenter);
		v->addWidget(title);
		QLabel *line = su::makeLabel(T("Welcome.Line"), su::Sizes::FONT_SIZE_NORMAL, 500, su::Colors::TEXT_SECONDARY);
		line->setAlignment(Qt::AlignHCenter);
		line->setWordWrap(true);
		v->addWidget(line);

		// One full picture, nothing cut off: drawn whole at the page width.
		auto *hero = new kit::PreviewSlot(QStringLiteral("welcome"), QStringLiteral("hero"), 260);
		hero->setNaturalAspect();
		hero->setOutline(false); // drawn on the window's own background: no frame
		hero->setPlaying(true);
		v->addWidget(hero);

		// Three things StreamUP does, each an icon and a few words.
		auto *row = new QHBoxLayout();
		row->setSpacing(su::S(12));
		struct Pillar {
			const char *icon;
			const char *nameKey;
			const char *lineKey;
		};
		const Pillar pillars[] = {{"package", "Welcome.Products", "Welcome.ProductsLine"},
					  {"tools", "Welcome.Tools", "Welcome.ToolsLine"},
					  {"sparkles", "Welcome.YourWay", "Welcome.YourWayLine"}};
		for (const Pillar &p : pillars) {
			auto *box = new QFrame();
			box->setObjectName("wlPillar");
			box->setStyleSheet(su::scale_qss(QString("QFrame#wlPillar{background:%1;border:1px solid rgba(255,255,255,0.06);"
								 "border-radius:12px;}")
								 .arg(su::Colors::BG_PRIMARY)));
			auto *bv = new QVBoxLayout(box);
			bv->setContentsMargins(su::S(14), su::S(12), su::S(14), su::S(12));
			bv->setSpacing(su::S(6));
			auto *icon = new QLabel();
			icon->setStyleSheet("background:transparent;border:none;");
			icon->setPixmap(su::svgIcon(QString::fromLatin1(p.icon), QColor(su::Colors::PRIMARY_LIGHT), 22,
						    devicePixelRatioF()));
			bv->addWidget(icon);
			QLabel *n = su::makeLabel(T(p.nameKey), su::Sizes::FONT_SIZE_NORMAL, 700, su::Colors::TEXT_PRIMARY);
			n->setStyleSheet(n->styleSheet() + "border:none;");
			bv->addWidget(n);
			QLabel *l = su::makeLabel(T(p.lineKey), su::Sizes::FONT_SIZE_SMALL, 400, su::Colors::TEXT_SECONDARY);
			l->setWordWrap(true);
			l->setStyleSheet(l->styleSheet() + "border:none;");
			bv->addWidget(l);
			bv->addStretch(1);
			row->addWidget(box, 1);
		}
		v->addLayout(row);

		if (QWidget *member = kit::MembershipCard(kit::MemberStyle::Strip))
			v->addWidget(member);
		v->addStretch(1);
		return page;
	}

	QWidget *buildFeatures()
	{
		auto *page = new QWidget();
		page->setObjectName("ftPage");
		page->setStyleSheet(su::scale_qss(QString("QWidget#ftPage{background:transparent;}"
							  "QFrame#ftCard{background:%1;border:1px solid rgba(255,255,255,0.06);"
							  "border-radius:12px;}")
							  .arg(su::Colors::BG_PRIMARY)));
		auto *v = new QVBoxLayout(page);
		v->setContentsMargins(su::S(20), su::S(16), su::S(20), su::S(20));
		v->setSpacing(su::S(10));

		QLabel *intro = su::makeLabel(T("Features.Intro"), su::CardText::kBody, 400, su::Colors::TEXT_SECONDARY);
		intro->setWordWrap(true);
		v->addWidget(intro);

		const QStringList fresh = ft::NewSince(ft::LastSeenVersion());
		for (ft::Group g : {ft::Group::Products, ft::Group::Docks, ft::Group::Mixer, ft::Group::Sources,
				    ft::Group::Hotkeys, ft::Group::Safety, ft::Group::Theme}) {
			QList<const ft::Feature *> features;
			for (const ft::Feature &f : ft::All())
				if (f.group == g)
					features.append(&f);
			if (features.isEmpty())
				continue;
			v->addWidget(su::sectionHeader(ft::GroupLabel(g)));
			if (g == ft::Group::Theme) {
				// What these need, said once, while the theme is not in use.
				if (!ft::ThemeActive())
					v->addWidget(themeNeededCard());
				else
					v->addWidget(su::makeLabel(T("Features.Theme.Active"), su::CardText::kBody, 400,
								   su::Colors::TEXT_SECONDARY));
			}
			auto *grid = new QGridLayout();
			grid->setHorizontalSpacing(su::S(14));
			grid->setVerticalSpacing(su::S(14));
			for (int i = 0; i < features.size(); ++i)
				grid->addWidget(buildCard(*features[i], fresh.contains(features[i]->id)), i / 3, i % 3,
						Qt::AlignTop | Qt::AlignLeft);
			grid->setColumnStretch(3, 1);
			v->addLayout(grid);
		}
		// Membership, once, at the foot of the list.
		if (QWidget *member = kit::MembershipCard(kit::MemberStyle::Strip)) {
			v->addSpacing(su::S(6));
			v->addWidget(member);
		}
		v->addStretch(1);
		return page;
	}

	QWidget *themeNeededCard()
	{
		auto *box = new QFrame();
		box->setObjectName("ftTheme");
		box->setStyleSheet(su::scale_qss(QString("QFrame#ftTheme{background:%1;border:1px solid rgba(255,255,255,0.08);"
							 "border-radius:12px;}")
							 .arg(su::Colors::BG_PRIMARY)));
		auto *h = new QHBoxLayout(box);
		h->setContentsMargins(su::S(14), su::S(12), su::S(12), su::S(12));
		h->setSpacing(su::S(12));
		auto *icon = new QLabel();
		icon->setStyleSheet("background:transparent;border:none;");
		icon->setPixmap(su::svgIcon("palette", QColor(su::Colors::PRIMARY_LIGHT), 22, devicePixelRatioF()));
		h->addWidget(icon, 0, Qt::AlignTop);
		auto *text = new QVBoxLayout();
		text->setSpacing(su::S(2));
		QLabel *t = su::makeLabel(T("Features.Theme.Needed"), su::Sizes::FONT_SIZE_NORMAL, 700, su::Colors::TEXT_PRIMARY);
		t->setStyleSheet(t->styleSheet() + "border:none;");
		text->addWidget(t);
		QLabel *l = su::makeLabel(T("Features.Theme.NeededLine"), su::Sizes::FONT_SIZE_SMALL, 400,
					  su::Colors::TEXT_SECONDARY);
		l->setWordWrap(true);
		l->setStyleSheet(l->styleSheet() + "border:none;");
		text->addWidget(l);
		h->addLayout(text, 1);
		auto *see = new su::PillButton(T("Features.Theme.See"), "neutral");
		QObject::connect(see, &QPushButton::clicked, this, []() { StreamUP::ThemeWindow::ShowThemeWindow(); });
		h->addWidget(see, 0, Qt::AlignVCenter);
		auto *member = new su::PillButton(T("Kit.Membership.Button"), "primary");
		QObject::connect(member, &QPushButton::clicked, this,
				 []() { QDesktopServices::openUrl(QUrl(kit::MembershipUrl())); });
		h->addWidget(member, 0, Qt::AlignVCenter);
		return box;
	}

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

		card.demo = new kit::PreviewSlot(QStringLiteral("demos"), f.id, kDemoH);
		card.demo->setZoom(Name(f), T(f.howKey.toUtf8().constData()));
		v->addWidget(card.demo);

		// A fixed-height row, so a card with a badge lines up with one without.
		auto *titleBox = new QWidget();
		titleBox->setFixedHeight(su::S(24));
		auto *titleRow = new QHBoxLayout(titleBox);
		titleRow->setContentsMargins(0, 0, 0, 0);
		titleRow->setSpacing(su::S(8));
		QLabel *name = su::makeLabel(Name(f), su::CardText::kTitle, 700, su::Colors::TEXT_PRIMARY);
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
		QObject::connect(show, &QPushButton::clicked, this, [this, id]() { showMe(id); });
		actions->addWidget(show);
		actions->addStretch(1);
		if (!f.core) {
			card.toggle = new su::SwitchButton();
			card.toggle->setChecked(ft::IsOn(f.id));
			card.toggle->setAccessibleName(name->text());
			actions->addWidget(card.toggle, 0, Qt::AlignVCenter);
			QObject::connect(card.toggle, &su::SwitchButton::toggled, this, [this, id](bool on) { flip(id, on); });
		}
		v->addLayout(actions);

		m_cards.append(card);
		refreshNote(m_cards.last());
		return card.frame;
	}

	// Show me: the clip big, one line on how to turn it on, and the way to the
	// place in OBS it lives (plus the video, when there is one).
	void showMe(const QString &id)
	{
		const ft::Feature *f = ft::Find(id);
		if (!f)
			return;
		QList<QWidget *> buttons;
		auto *where = new su::PillButton(T("Features.ShowWhere"), "primary");
		where->setEnabled(ft::IsActive(id));
		if (!where->isEnabled())
			where->setToolTip(T("Features.ShowWhereOff"));
		QObject::connect(where, &QPushButton::clicked, this, [id]() {
			QTimer::singleShot(160, qApp, [id]() { ft::ShowMe(id); });
		});
		buttons << where;
		if (!f->video.isEmpty()) {
			auto *video = new su::PillButton(T("Features.WatchVideo"), "neutral");
			const QString url = f->video;
			QObject::connect(video, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
			buttons << video;
		}
		kit::Lightbox(this, QStringLiteral("demos"), id, Name(*f), T(f->howKey.toUtf8().constData()), buttons);
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
		showStatus(on ? T("Features.TurnedOn").arg(Name(*ft::Find(id))) : T("Features.TurnedOff").arg(Name(*ft::Find(id))));
	}

	// A restart part changed here and now differs from what is running: offer
	// to restart OBS now, like OBS's own settings do. Never while live.
	void askToRestart()
	{
		QStringList changed;
		for (const ft::Feature &f : ft::All())
			if (!f.live && ft::IsOn(f.id) != ft::IsActive(f.id) && !m_pendingAtOpen.count(f.id))
				changed << Name(f);
		if (changed.isEmpty())
			return;
		if (obs_frontend_streaming_active() || obs_frontend_recording_active()) {
			kit::Toast(T("Features.Restart.Later"), kit::Status::Info);
			return;
		}
		QWidget *main = MainWindow();
		su::ShadowDialog *ask = su::confirm(main, T("Features.Restart.Title"),
						    T("Features.Restart.Line").arg(changed.join(", ")), T("Features.Restart.Now"),
						    "primary", []() { RestartObs(); });
		const QString now = T("Features.Restart.Now");
		for (auto *b : ask->findChildren<QPushButton *>())
			if (b->text() != now)
				b->setText(T("Features.Restart.LaterButton"));
		StreamUP::UIHelpers::PlaceOverMainWindow(ask);
	}

	void updatePlaying()
	{
		if (!isVisible())
			return;
		QScrollArea *scroll = qobject_cast<QScrollArea *>(m_stack->currentWidget());
		const bool onFeatures = scroll && scroll == m_featuresScroll;
		const QRect view = scroll ? QRect(scroll->viewport()->mapToGlobal(QPoint(0, 0)), scroll->viewport()->size())
					  : QRect();
		for (Card &c : m_cards) {
			if (!c.demo->animated())
				continue;
			const QRect r(c.demo->mapToGlobal(QPoint(0, 0)), c.demo->size());
			c.demo->setPlaying(onFeatures && view.intersects(r));
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

	su::WindowShell m_shell{};
	QStackedWidget *m_stack = nullptr;
	QScrollArea *m_featuresScroll = nullptr;
	QLabel *m_status = nullptr;
	QTimer *m_visibility = nullptr;
	QList<Card> m_cards;
	Mode m_mode = Mode::Features;
	int m_welcomeIndex = -1;
	int m_whatsNewIndex = -1;
	int m_featuresIndex = -1;
	std::set<QString> m_pendingAtOpen;
};

QPointer<Page> &Open()
{
	static QPointer<Page> page;
	return page;
}

std::vector<std::function<void()>> &AfterClose()
{
	static std::vector<std::function<void()>> list;
	return list;
}

void ShowOnUiThread(Mode mode)
{
	QPointer<Page> &existing = Open();
	if (existing) {
		existing->showMode(mode);
		existing->showNormal();
		existing->raise();
		existing->activateWindow();
		return;
	}
	auto *page = new Page(MainWindow());
	page->onClosed = []() {
		auto list = std::move(AfterClose());
		AfterClose().clear();
		for (auto &fn : list)
			if (fn)
				fn();
	};
	existing = page;
	page->showMode(mode);
	StreamUP::UIHelpers::PlaceOverMainWindow(page);
	page->show();
	page->raise();
	page->activateWindow();
}

} // namespace

void Show()
{
	QMetaObject::invokeMethod(qApp, []() { ShowOnUiThread(Mode::Features); }, Qt::QueuedConnection);
}

void ShowWhatsNew()
{
	QMetaObject::invokeMethod(qApp, []() { ShowOnUiThread(Mode::WhatsNew); }, Qt::QueuedConnection);
}

bool IsOpen()
{
	return !Open().isNull();
}

void WhenClosed(std::function<void()> fn)
{
	if (!IsOpen()) {
		if (fn)
			fn();
		return;
	}
	AfterClose().push_back(std::move(fn));
}

void ShowAtStartup()
{
	using SC = StreamUP::SplashScreen::ShowCondition;
	const SC condition = StreamUP::SplashScreen::CheckSplashCondition();
	bool quiet = false;
	if (obs_data_t *settings = StreamUP::SettingsManager::LoadSettings()) {
		quiet = obs_data_get_bool(settings, "splash_disabled");
		obs_data_release(settings);
	}
	const QString seen = ft::LastSeenVersion();
	const bool newParts = !seen.isEmpty() && !ft::NewSince(seen).isEmpty();
	if (condition != SC::Never)
		StreamUP::SplashScreen::UpdateVersionTracking();

	// One window, whatever the reason: a first install gets the welcome page,
	// an update gets what's new, and either leads on to the features.
	if (condition == SC::FirstInstall || seen.isEmpty()) {
		ShowOnUiThread(Mode::Welcome);
	} else if (condition == SC::VersionUpdate && !quiet) {
		ShowOnUiThread(Mode::WhatsNew);
	} else if (newParts) {
		ShowOnUiThread(Mode::Features);
	}
}

} // namespace FeaturesPage
} // namespace StreamUP
