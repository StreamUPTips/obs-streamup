#include "overlay-controls-window.hpp"
#include "streamup-kit.hpp"

#include "../core/overlay-controls.hpp"
#include "../core/product-transitions.hpp"
#include "../core/product-registry.hpp"
#include "overlay-layout-page.hpp"
#include "overlay-platforms-editor.hpp"
#include "overlay-ui-common.hpp"
#include "font-picker.hpp"
#include "ui-helpers.hpp"
#include <streamup/debug-logger.hpp>
#include "version.h"

#include <streamup/ui/color-picker.hpp>
#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/glass.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/switch-button.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QFileDialog>
#include <QDesktopServices>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QImageReader>
#include <QGridLayout>
#include <QHash>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QSet>
#include <QTimer>

#include <cmath>
#include <memory>
#include <utility>

namespace su = StreamUP::UIStyles;
namespace oc = StreamUP::OverlayControls;
using StreamUP::OverlayUI::T;

namespace StreamUP {
namespace OverlayControlsWindow {

namespace {

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

QString SwatchStyle(const QColor &c)
{
	// Dark ink on light colours, white on dark, judged on perceived brightness.
	const double luma = (0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF()) * c.alphaF() +
			    (1.0 - c.alphaF()) * 0.15;
	const QString ink = luma > 0.55 ? QStringLiteral("#11111b") : QStringLiteral("#ffffff");
	return su::scale_qss(
		QString("QPushButton{background:%1;color:%2;border:1px solid rgba(255,255,255,0.14);"
			"border-radius:8px;min-height:26px;padding:0 12px;font-size: 12px;font-weight: 600;"
			"}"
			"QPushButton:hover{border:1px solid %3;}")
			.arg(c.name(QColor::HexArgb), ink, su::Colors::PRIMARY_LIGHT));
}

QColor ParseHex(const QString &text)
{
	const QString s = text.trimmed();
	if (s.length() == 9) // #RRGGBBAA
		return QColor(QStringLiteral("#%1%2").arg(s.mid(7, 2), s.mid(1, 6)));
	return QColor(s);
}

QString HexText(const QColor &c)
{
	return c.alpha() < 255 ? QStringLiteral("#%1%2")
					 .arg(c.name(QColor::HexRgb).mid(1))
					 .arg(c.alpha(), 2, 16, QLatin1Char('0'))
					 .toUpper()
			       : c.name(QColor::HexRgb).toUpper();
}

struct Row {
	oc::ControlInfo info;
	QLabel *drift = nullptr;
	OverlayUI::PanelDoubleSpin *sliderSpin = nullptr; // a slider's typed value
	OverlayUI::GlyphButton *reset = nullptr;
	OverlayUI::PanelSlider *slider = nullptr;
	OverlayUI::PanelDoubleSpin *number = nullptr;
	QPushButton *swatch = nullptr;
	su::SwitchButton *toggle = nullptr;
	QLineEdit *text = nullptr;
	QPlainTextEdit *area = nullptr;
	QComboBox *combo = nullptr;
	FontPicker::FontButton *font = nullptr;
	OverlayUI::PlatformsEditor *platforms = nullptr;
	QLineEdit *imagePath = nullptr;
	QLabel *thumb = nullptr;
};

// Whether a value is anything other than the control's default, so its reset
// button has something to do. Numbers compare within a hair of each other.
bool DiffersFromDefault(const oc::ControlInfo &c, const QJsonValue &value)
{
	const QJsonValue &d = c.defaultValue;
	if (value.isDouble() || d.isDouble())
		return std::fabs(value.toDouble() - d.toDouble()) > 1e-9 * std::max(1.0, std::fabs(d.toDouble()));
	if (value.isString() && d.isString())
		return value.toString().compare(d.toString(), c.type == oc::ControlType::Colour ? Qt::CaseInsensitive
													 : Qt::CaseSensitive) != 0;
	return value != d;
}

// One of the product's actions, as a row with a button.
struct ActionRow {
	oc::ActionInfo info;
	QLabel *state = nullptr;
	su::PillButton *button = nullptr;
};

// Image files an OBS image source can show.
QString ImageFilter()
{
	return T("OverlayControls.Image.Filter") + " (*.png *.jpg *.jpeg *.gif *.bmp *.tga *.webp *.psd *.jxr)";
}

void ShowThumb(QLabel *thumb, const QString &path)
{
	const QSize box = thumb->size() * thumb->devicePixelRatioF();
	QImageReader reader(path);
	QImage img;
	if (!path.isEmpty() && reader.canRead()) {
		const QSize full = reader.size();
		if (full.isValid())
			reader.setScaledSize(full.scaled(box, Qt::KeepAspectRatio));
		img = reader.read();
	}
	if (img.isNull()) {
		thumb->setPixmap(QPixmap());
		thumb->setText(QString());
		return;
	}
	QPixmap pm = QPixmap::fromImage(img);
	pm.setDevicePixelRatio(thumb->devicePixelRatioF());
	thumb->setText(QString());
	thumb->setPixmap(pm);
}

class OverlayPanel : public su::ShadowDialog {
public:
	explicit OverlayPanel(const QString &overlayId, QWidget *parent) : su::ShadowDialog(parent), m_id(overlayId)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		oc::GetOverlay(m_id, m_info);

		auto shell = su::applyChrome(this, m_info.name + " " + T("OverlayControls.Window.TitleSuffix"),
					     "v" PROJECT_VERSION, /*brandFooter=*/true, "StreamUP");

		// Sidebar + pages, the StreamUP Settings layout.
		auto *body = new QWidget();
		body->setObjectName("ocBody");
		body->setStyleSheet(QString("QWidget#ocBody{background:%1;}").arg(su::Colors::BG_DARKEST));
		auto *row = new QHBoxLayout(body);
		row->setContentsMargins(su::S(16), su::S(16), 0, 0);
		row->setSpacing(0);

		auto *rail = new QFrame();
		rail->setObjectName("ocRail");
		rail->setFixedWidth(su::S(180));
		rail->setStyleSheet(su::scale_qss(
			QString("QFrame#ocRail{background:%1;border-radius:22px;}").arg(su::Colors::BG_PRIMARY)));
		auto *rl = new QVBoxLayout(rail);
		rl->setContentsMargins(su::S(6), su::S(6), su::S(6), su::S(6));
		m_nav = new QListWidget();
		m_nav->setStyleSheet(OverlayUI::NavListStyle());
		m_nav->setAccessibleName(T("OverlayControls.Window.Sections"));
		rl->addWidget(m_nav);
		auto *railTop = new QVBoxLayout();
		railTop->setContentsMargins(0, 0, 0, su::S(16));
		railTop->addWidget(rail);
		row->addLayout(railTop);

		m_pages = new QStackedWidget();
		row->addWidget(m_pages, 1);
		shell.content->addWidget(body, 1);

		buildPages();
		QObject::connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
			ensurePage(row);
			m_pages->setCurrentIndex(row);
		});
		// Screen readers select a row without making it current, so the
		// page follows the selection too.
		QObject::connect(m_nav, &QListWidget::itemSelectionChanged, this, [this]() {
			const QList<QListWidgetItem *> sel = m_nav->selectedItems();
			if (!sel.isEmpty() && m_nav->row(sel.first()) != m_nav->currentRow())
				m_nav->setCurrentRow(m_nav->row(sel.first()));
		});
		m_nav->setCurrentRow(0);

		m_status = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
		m_status->setWordWrap(false);
		shell.footer->insertWidget(0, m_status);
		m_status->hide();

		auto *resetAll = new su::PillButton(T("OverlayControls.Button.ResetAll"), "outline");
		auto *savePreset = new su::PillButton(T("OverlayControls.Button.SavePreset"), "neutral");
		auto *done = new su::PillButton(T("OverlayControls.Button.Done"), "primary");
		shell.footerButtons->addWidget(resetAll);
		shell.footerButtons->addWidget(savePreset);
		shell.footerButtons->addWidget(done);
		// A product that is all actions has nothing a preset could hold.
		savePreset->setVisible(m_info.hasControls);

		QObject::connect(done, &QPushButton::clicked, this, &QDialog::close);
		QObject::connect(resetAll, &QPushButton::clicked, this, [this]() {
			QPointer<OverlayPanel> self(this);
			su::confirm(this, T("OverlayControls.ResetAll.Title"),
				    T("OverlayControls.ResetAll.Message").arg(m_info.name.toHtmlEscaped()),
				    T("OverlayControls.Button.ResetAll"), "danger", [self]() {
					    if (!self)
						    return;
					    oc::ResetAll(self->m_id);
					    self->showStatus(T("OverlayControls.Status.Reset"));
				    });
		});
		QObject::connect(savePreset, &QPushButton::clicked, this, [this]() {
			QPointer<OverlayPanel> self(this);
			su::prompt(this, T("OverlayControls.SavePreset.Title"), T("OverlayControls.SavePreset.Field"),
				   QString(), [self](const QString &name) {
					   if (!self)
						   return;
					   QString error;
					   if (oc::SavePreset(self->m_id, name, &error))
						   self->showStatus(T("OverlayControls.Status.PresetSaved").arg(name.trimmed()));
					   else
						   su::info(self, T("OverlayControls.SavePreset.Title"), error);
				   });
		});

		// A slider drag or a layout drag fires a change per step. Writing every
		// step to every linked filter, then rebuilding the panel's view of the
		// whole overlay after each one, is what made the panel lag. Writes are
		// throttled to ~30 a second (first and last always land), and the
		// refreshes that follow them are coalesced into one.
		m_flushTimer = new QTimer(this);
		m_flushTimer->setSingleShot(true);
		m_flushTimer->setInterval(33);
		QObject::connect(m_flushTimer, &QTimer::timeout, this, [this]() {
			if (flushWrites())
				m_flushTimer->start();
		});
		m_refreshTimer = new QTimer(this);
		m_refreshTimer->setSingleShot(true);
		m_refreshTimer->setInterval(90);
		QObject::connect(m_refreshTimer, &QTimer::timeout, this, [this]() { runRefresh(); });

		oc::Notifier *n = oc::GetNotifier();
		QObject::connect(n, &oc::Notifier::controlChanged, this,
				 [this](const QString &overlayId, const QString &controlId) {
					 if (overlayId != m_id)
						 return;
					 if (!m_writing)
						 m_changedControls.insert(controlId);
					 m_refreshTimer->start();
				 });
		QObject::connect(n, &oc::Notifier::overlayReset, this, [this](const QString &overlayId) {
			if (overlayId == m_id)
				refreshAll(true);
		});
		QObject::connect(n, &oc::Notifier::sourceReset, this, [this](const QString &overlayId, const QString &) {
			if (overlayId == m_id)
				refreshAll(true);
		});
		QObject::connect(n, &oc::Notifier::overlaysChanged, this, [this]() {
			oc::OverlayInfo info;
			if (!oc::GetOverlay(m_id, info)) {
				close(); // removed, or the scene collection changed
				return;
			}
			refreshAll(false);
		});

		// Someone may change a linked filter by hand while this is open. A
		// slow poll is plenty for a hint dot and costs a few lookups.
		auto *driftTimer = new QTimer(this);
		driftTimer->setInterval(1500);
		QObject::connect(driftTimer, &QTimer::timeout, this, [this]() {
			// Nothing on the Layout page shows drift, and the poll reads every
			// linked setting, so leave it alone while that page is up.
			if (m_layout && m_layout->isVisible())
				return;
			refreshDrift();
			refreshActions();
		});
		driftTimer->start();

		const int m = su::S(su::ShadowDialog::kShadowMargin);
		resize(su::S(1120) + 2 * m, su::S(800) + 2 * m);
	}

	~OverlayPanel() override
	{
		// The last step of a drag may still be waiting on the throttle.
		flushWrites();
	}

	QString overlayId() const { return m_id; }

	// A cancelled colour picker reverts straight to the old value, so any
	// step of it still queued here must not land on top of the revert.
	void dropPending(const QString &controlId) { m_pending.remove(controlId); }

private:
	// ------------------------------------------------------------ pages

	void buildPages()
	{
		bool first = true;
		bool anyCurated = false;
		for (const oc::GroupInfo &g : m_info.groups)
			for (const oc::ControlInfo &c : g.controls)
				anyCurated = anyCurated || c.type != oc::ControlType::Layout;
		if (!anyCurated && !m_info.actions.isEmpty()) {
			// A product with actions and nothing else to tune still opens
			// on a page that explains it and offers them.
			m_nav->addItem(T("OverlayControls.Tab.Setup"));
			m_pages->addWidget(buildCuratedPage(QString(), true));
			first = false;
		}
		for (const oc::TabInfo &tab : m_info.tabs) {
			bool any = false;
			for (const oc::GroupInfo &g : m_info.groups)
				for (const oc::ControlInfo &c : g.controls)
					any = any || (c.tab == tab.id && c.type != oc::ControlType::Layout);
			if (!any)
				continue;
			m_nav->addItem(tab.label);
			if (first) {
				m_pages->addWidget(buildCuratedPage(tab.id, true));
			} else {
				// Built the first time it is shown: a page of rows is the
				// slow part of opening the panel, and most visits only
				// look at the first tab.
				m_lazyTabs.insert(m_pages->count(), tab.id);
				m_pages->addWidget(new QWidget());
			}
			first = false;
		}
		bool hasLayout = false;
		for (const oc::GroupInfo &g : m_info.groups)
			for (const oc::ControlInfo &c : g.controls)
				hasLayout = hasLayout || c.type == oc::ControlType::Layout;
		if (hasLayout) {
			m_layout = new OverlayUI::LayoutPage(m_id, m_info);
			m_layout->onStatus = [this](const QString &s) { showStatus(s); };
			m_nav->addItem(T("OverlayControls.Tab.Layout"));
			m_pages->addWidget(m_layout);
		}
		refreshDrift();
	}

	void ensurePage(int index)
	{
		auto it = m_lazyTabs.find(index);
		if (it == m_lazyTabs.end())
			return;
		const QString tab = it.value();
		m_lazyTabs.erase(it);
		QWidget *placeholder = m_pages->widget(index);
		QWidget *page = buildCuratedPage(tab, false);
		const QSignalBlocker block(m_pages);
		m_pages->insertWidget(index, page);
		m_pages->removeWidget(placeholder);
		placeholder->deleteLater();
		refreshDrift();
	}

	QWidget *buildCuratedPage(const QString &tab, bool first)
	{
		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
		su::useScrollBars(scroll);

		auto *page = new QWidget();
		page->setObjectName("ocPage");
		page->setStyleSheet(QString("QWidget#ocPage{background:%1;}").arg(su::Colors::BG_DARKEST) + su::sliderStyle());
		auto *v = new QVBoxLayout(page);
		v->setContentsMargins(su::S(16), 0, su::S(16), su::S(16));
		v->setSpacing(su::S(12));
		scroll->setWidget(page);

		if (first) {
			if (QLabel *dot = StreamUP::Kit::HelpDot(m_info.description)) {
				auto *about = new QHBoxLayout();
				about->setSpacing(su::S(6));
				about->addWidget(su::makeLabel(T("OverlayControls.HowItWorks"), su::Sizes::FONT_SIZE_SMALL, 600,
							       su::Colors::TEXT_MUTED));
				about->addWidget(dot);
				about->addStretch(1);
				v->addLayout(about);
			}
			m_missingNote = su::makeLabel(QString(), su::Sizes::FONT_SIZE_SMALL, 500, su::Colors::COLOR_WARNING);
			v->addWidget(m_missingNote);
			updateMissingNote();
			buildActionsCard(v);
			if (!tab.isEmpty())
				buildPresetCard(v);
		}

		StreamUP::Kit::MoreOptions *more = nullptr;
		for (const oc::GroupInfo &g : m_info.groups) {
			QList<const oc::ControlInfo *> controls;
			QList<const oc::ControlInfo *> rare;
			for (const oc::ControlInfo &c : g.controls)
				if (c.tab == tab && c.type != oc::ControlType::Layout)
					(c.advanced ? rare : controls).append(&c);
			if (!rare.isEmpty()) {
				if (!more)
					more = new StreamUP::Kit::MoreOptions(QStringLiteral("controls.") + m_info.id + "." + tab);
				QVBoxLayout *rareCard = su::sectionCard(more->body(), g.label);
				auto *rareGrid = new QGridLayout();
				rareGrid->setContentsMargins(0, su::S(6), 0, 0);
				rareGrid->setHorizontalSpacing(su::S(12));
				rareGrid->setVerticalSpacing(su::S(10));
				rareGrid->setColumnStretch(1, 1);
				rareCard->addLayout(rareGrid);
				int rr = 0;
				for (const oc::ControlInfo *c : rare)
					rr = addRow(rareGrid, rr, *c);
			}
			if (controls.isEmpty())
				continue;
			QVBoxLayout *card = su::sectionCard(v, g.label);
			auto *grid = new QGridLayout();
			grid->setContentsMargins(0, su::S(6), 0, 0);
			grid->setHorizontalSpacing(su::S(12));
			grid->setVerticalSpacing(su::S(10));
			grid->setColumnStretch(1, 1);
			card->addLayout(grid);
			int r = 0;
			for (const oc::ControlInfo *c : controls)
				r = addRow(grid, r, *c);
		}
		if (more)
			v->addWidget(more);
		v->addStretch(1);
		return scroll;
	}

	void buildActionsCard(QVBoxLayout *v)
	{
		if (m_info.actions.isEmpty())
			return;
		QVBoxLayout *card = su::sectionCard(v, T("OverlayControls.Group.Transition"));
		for (const oc::ActionInfo &a : m_info.actions) {
			auto *row = new QHBoxLayout();
			row->setContentsMargins(0, su::S(6), 0, 0);
			row->setSpacing(su::S(16));
			auto *text = new QVBoxLayout();
			text->setContentsMargins(0, 0, 0, 0);
			text->setSpacing(su::S(3));
			const QString title = a.label.isEmpty() ? T("OverlayControls.Action.UseAsTransition") : a.label;
			text->addWidget(su::makeLabel(title, su::CardText::kTitle, 600, su::Colors::TEXT_PRIMARY));
			const QString help = a.help.isEmpty()
						     ? T("OverlayControls.Action.UseAsTransition.Help").arg(a.scene.toHtmlEscaped())
						     : a.help;
			if (QLabel *dot = StreamUP::Kit::HelpDot(help)) {
				// The title row carries the detail as a "?".
				QLayoutItem *titleItem = text->itemAt(text->count() - 1);
				if (QWidget *titleWidget = titleItem ? titleItem->widget() : nullptr) {
					text->removeWidget(titleWidget);
					auto *titleRow = new QHBoxLayout();
					titleRow->setSpacing(su::S(6));
					titleRow->addWidget(titleWidget);
					titleRow->addWidget(dot);
					titleRow->addStretch(1);
					text->addLayout(titleRow);
				}
			}
			ActionRow ar;
			ar.info = a;
			ar.state = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 600, su::Colors::TEXT_MUTED);
			ar.state->setWordWrap(false);
			text->addWidget(ar.state);
			row->addLayout(text, 1);
			ar.button = new su::PillButton(T("OverlayControls.Action.Use"), "primary");
			ar.button->setAccessibleName(title);
			row->addWidget(ar.button, 0, Qt::AlignVCenter);
			card->addLayout(row);

			const QString id = a.id;
			QObject::connect(ar.button, &QPushButton::clicked, this, [this, id]() {
				QString message;
				QString error;
				if (oc::RunAction(m_id, id, &message, &error))
					showStatus(message);
				else
					su::info(this, T("OverlayControls.Group.Transition"), error);
				refreshActions();
			});
			m_actionRows.append(ar);
		}
		refreshActions();
	}

	// Cheap: one look at OBS's transition list per action, so it runs with
	// the drift poll and catches a transition renamed, removed or picked in
	// the Transitions dock.
	void refreshActions()
	{
		// A reinstall can rename an action's transition.
		for (ActionRow &ar : m_actionRows)
			for (const oc::ActionInfo &a : m_info.actions)
				if (a.id == ar.info.id)
					ar.info = a;
		for (ActionRow &ar : m_actionRows) {
			const QString name = ar.info.transition;
			obs_source_t *t = StreamUP::ProductTransitions::Find(name);
			const bool exists = t != nullptr;
			obs_source_release(t);
			const bool current = exists && StreamUP::ProductTransitions::IsCurrent(name);
			QString state;
			const char *colour = su::Colors::TEXT_MUTED;
			if (!exists) {
				state = T("OverlayControls.Action.State.Missing");
			} else if (current) {
				state = T("OverlayControls.Action.State.Picked").arg(name);
				colour = su::Colors::COLOR_SUCCESS;
			} else {
				state = T("OverlayControls.Action.State.NotPicked").arg(name);
			}
			if (ar.state->text() != state) {
				ar.state->setText(state);
				ar.state->setStyleSheet(su::scale_qss(QString("QLabel{color:%1;font-size:%2px;font-weight:600;"
										 "background:transparent;}")
										 .arg(colour)
										 .arg(su::Sizes::FONT_SIZE_TINY)));
			}
			ar.button->setText(exists ? T("OverlayControls.Action.Update") : T("OverlayControls.Action.Use"));
			ar.button->updateGeometry();
		}
	}

	void buildPresetCard(QVBoxLayout *v)
	{
		QVBoxLayout *card = su::sectionCard(v, T("OverlayControls.Group.Presets"));
		auto *row = new QHBoxLayout();
		row->setContentsMargins(0, su::S(6), 0, 0);
		row->setSpacing(su::S(10));

		m_presets = new su::MacComboBox();
		m_presets->setOnCard(true);
		m_presets->setAccessibleName(T("OverlayControls.Group.Presets"));
		m_presets->setStyleSheet(su::comboStyle(true));
		su::makeComboAnimated(m_presets);
		row->addWidget(m_presets, 1);

		m_deletePreset = new QToolButton();
		m_deletePreset->setText(T("OverlayControls.Button.DeletePreset"));
		m_deletePreset->setCursor(Qt::PointingHandCursor);
		m_deletePreset->setStyleSheet(su::scale_qss(
			QString("QToolButton{color:%1;background:transparent;border:none;font-size: 12px;"
				"font-weight: 600;padding:0 4px;}QToolButton:hover{color:%2;}")
				.arg(su::Colors::TEXT_MUTED, su::Colors::COLOR_DANGER)));
		// The stylesheet font only applies once polished; size from that,
		// or the button keeps its pre-style hint and elides to "...".
		m_deletePreset->ensurePolished();
		m_deletePreset->setMinimumWidth(m_deletePreset->fontMetrics().horizontalAdvance(m_deletePreset->text()) +
						su::S(12));
		row->addWidget(m_deletePreset, 0);
		card->addLayout(row);

		fillPresets();

		// currentIndexChanged rather than activated, so choosing with the
		// keyboard or a screen reader applies the preset too. Programmatic
		// fills block signals, and the placeholder row carries no name.
		QObject::connect(m_presets, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
			const QString name = m_presets->itemData(index).toString();
			if (name.isEmpty())
				return;
			QString error;
			if (oc::ApplyPreset(m_id, name, &error))
				showStatus(T("OverlayControls.Status.PresetApplied").arg(name));
			else
				su::info(this, T("OverlayControls.Group.Presets"), error);
			m_presets->setCurrentIndex(0);
		});
		QObject::connect(m_deletePreset, &QToolButton::clicked, this, [this]() {
			QMenu menu(this);
			menu.setStyleSheet(su::menuStyle());
			for (const oc::PresetInfo &p : m_info.presets) {
				if (p.builtIn)
					continue;
				const QString name = p.name;
				menu.addAction(name, this, [this, name]() {
					if (oc::DeletePreset(m_id, name))
						showStatus(T("OverlayControls.Status.PresetDeleted").arg(name));
				});
			}
			menu.exec(m_deletePreset->mapToGlobal(QPoint(0, m_deletePreset->height())));
		});
	}

	void fillPresets()
	{
		if (!m_presets)
			return;
		const QSignalBlocker block(m_presets);
		m_presets->clear();
		m_presets->addItem(T("OverlayControls.Preset.Placeholder"), QString());
		bool anyUser = false;
		bool anyBuiltIn = false;
		for (const oc::PresetInfo &p : m_info.presets) {
			if (p.builtIn) {
				m_presets->addItem(p.name, p.name);
				anyBuiltIn = true;
			}
		}
		for (const oc::PresetInfo &p : m_info.presets) {
			if (!p.builtIn) {
				if (!anyUser && anyBuiltIn)
					m_presets->insertSeparator(m_presets->count());
				m_presets->addItem(p.name, p.name);
				anyUser = true;
			}
		}
		m_presets->setCurrentIndex(0);
		m_presets->setEnabled(m_presets->count() > 1);
		m_deletePreset->setVisible(anyUser);
	}

	// ------------------------------------------------------------ rows

	// Returns the next free grid row.
	int addRow(QGridLayout *grid, int r, const oc::ControlInfo &c)
	{
		Row row;
		row.info = c;
		const QString id = c.id;

		auto *labelBox = new QWidget();
		auto *lb = new QHBoxLayout(labelBox);
		lb->setContentsMargins(0, 0, 0, 0);
		lb->setSpacing(su::S(6));
		lb->addWidget(OverlayUI::RowLabel(c.label, c.help));
		row.drift = new QLabel(QString::fromUtf8("●"));
		row.drift->setStyleSheet(su::scale_qss(
			QString("QLabel{color:%1;font-size: 12px; font-weight: 400;background:transparent;}").arg(su::Colors::COLOR_WARNING)));
		row.drift->setToolTip(T("OverlayControls.Drift.Tooltip"));
		row.drift->hide();
		lb->addWidget(row.drift);
		lb->addStretch();
		labelBox->setMinimumWidth(su::S(170));

		row.reset = new OverlayUI::GlyphButton(OverlayUI::GlyphButton::Glyph::Reset, T("OverlayControls.Reset.Tooltip"),
						       T("OverlayControls.Reset.Accessible").arg(c.label));
		// Shown only while the value differs from the default; its column
		// keeps its width so the rows never shift.
		QSizePolicy keep = row.reset->sizePolicy();
		keep.setRetainSizeWhenHidden(true);
		row.reset->setSizePolicy(keep);
		QObject::connect(row.reset, &QToolButton::clicked, this, [this, id]() {
			oc::ResetControl(m_id, id);
			refreshControl(id);
		});

		int next = r + 1;
		switch (c.type) {
		case oc::ControlType::Colour: {
			row.swatch = new QPushButton();
			row.swatch->setCursor(Qt::PointingHandCursor);
			row.swatch->setFixedWidth(su::S(132));
			row.swatch->setToolTip(c.help);
			row.swatch->setAccessibleName(c.label);
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.swatch, r, 1, Qt::AlignLeft);
			QObject::connect(row.swatch, &QPushButton::clicked, this, [this, id]() { pickColour(id); });
			break;
		}
		case oc::ControlType::Slider: {
			row.slider = new OverlayUI::PanelSlider(/*ownStyle=*/false);
			const int steps = std::max(1, int(std::lround((c.max - c.min) / c.step)));
			row.slider->setRange(0, steps);
			row.slider->setPageStep(std::max(1, steps / 10));
			row.slider->setToolTip(c.help);
			row.slider->setAccessibleName(c.label);
			// The value beside it can be typed: Enter or leaving the field
			// applies it, the arrows and the wheel (when focused) step it.
			row.sliderSpin = new OverlayUI::PanelDoubleSpin();
			row.sliderSpin->setRange(c.min, c.max);
			row.sliderSpin->setDecimals(c.decimals);
			row.sliderSpin->setSingleStep(c.step);
			row.sliderSpin->setSuffix(c.unit);
			row.sliderSpin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			// Wide enough for the longest value this control can hold, unit
			// included, plus the field's padding and its arrow column.
			{
				// Measured in the box's own 13 px font: the style sheet sets
				// it, and font() can still be the default size here, which is
				// how "2.00" ended up cut to "2.0(".
				QFont boxFont = row.sliderSpin->font();
				boxFont.setPixelSize(su::S(13));
				const QFontMetrics fm(boxFont);
				int text = 0;
				for (double v : {c.min, c.max})
					text = std::max(text, fm.horizontalAdvance(
								      QString::number(v, 'f', c.decimals) + c.unit + "0"));
				// One width for every value box in a panel unless a value
				// needs more, so the boxes and reset arrows line up.
				row.sliderSpin->setFixedWidth(std::max(text + su::S(10 + 28 + 20), su::S(124)));
			}
			row.sliderSpin->setAccessibleName(T("OverlayControls.Slider.Value").arg(c.label));
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.slider, r, 1);
			grid->addWidget(row.sliderSpin, r, 2, Qt::AlignVCenter);
			QObject::connect(row.slider, &QSlider::valueChanged, this, [this, id](int pos) {
				if (m_updating)
					return;
				Row &rw = m_rows[id];
				const double v = rw.info.min + pos * rw.info.step;
				{
					const QSignalBlocker block(rw.sliderSpin);
					rw.sliderSpin->setValue(v);
				}
				write(id, v);
			});
			QObject::connect(row.sliderSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
					 [this, id](double v) {
						 if (m_updating)
							 return;
						 Row &rw = m_rows[id];
						 {
							 const QSignalBlocker block(rw.slider);
							 rw.slider->setValue(int(std::lround((v - rw.info.min) / rw.info.step)));
						 }
						 write(id, v);
					 });
			break;
		}
		case oc::ControlType::Number: {
			row.number = new OverlayUI::PanelDoubleSpin();
			row.number->setRange(c.min, c.max);
			row.number->setDecimals(c.decimals);
			row.number->setSingleStep(c.step);
			row.number->setSuffix(c.unit.isEmpty() ? QString() : " " + c.unit);
			row.number->setFixedWidth(su::S(160));
			row.number->setToolTip(c.help);
			row.number->setAccessibleName(c.label);
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.number, r, 1, Qt::AlignLeft);
			QObject::connect(row.number, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
					 [this, id](double v) {
						 if (!m_updating)
							 write(id, v);
					 });
			break;
		}
		case oc::ControlType::Toggle: {
			row.toggle = new su::SwitchButton();
			row.toggle->setToolTip(c.help);
			row.toggle->setAccessibleName(c.label);
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.toggle, r, 1, Qt::AlignLeft);
			QObject::connect(row.toggle, &su::SwitchButton::toggled, this, [this, id](bool on) {
				if (!m_updating)
					write(id, on);
			});
			break;
		}
		case oc::ControlType::Text: {
			row.text = new QLineEdit();
			row.text->setStyleSheet(su::lineEditStyle(true));
			row.text->setFixedHeight(su::S(28));
			row.text->setToolTip(c.help);
			row.text->setAccessibleName(c.label);
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.text, r, 1);
			auto *timer = new QTimer(row.text);
			timer->setSingleShot(true);
			timer->setInterval(400);
			QLineEdit *edit = row.text;
			QObject::connect(edit, &QLineEdit::textEdited, timer, [timer]() { timer->start(); });
			QObject::connect(timer, &QTimer::timeout, this, [this, id, edit]() { write(id, edit->text()); });
			QObject::connect(edit, &QLineEdit::editingFinished, this, [this, id, edit, timer]() {
				if (timer->isActive()) {
					timer->stop();
					write(id, edit->text());
				}
			});
			break;
		}
		case oc::ControlType::TextArea: {
			row.area = new QPlainTextEdit();
			row.area->setStyleSheet(su::plainTextStyle(true));
			row.area->setFixedHeight(su::S(88));
			row.area->setToolTip(c.help);
			row.area->setAccessibleName(c.label);
			grid->addWidget(labelBox, r, 0, Qt::AlignTop);
			grid->addWidget(row.area, r, 1);
			auto *timer = new QTimer(row.area);
			timer->setSingleShot(true);
			timer->setInterval(450);
			QPlainTextEdit *edit = row.area;
			QObject::connect(edit, &QPlainTextEdit::textChanged, timer, [this, timer]() {
				if (!m_updating)
					timer->start();
			});
			QObject::connect(timer, &QTimer::timeout, this, [this, id, edit]() { write(id, edit->toPlainText()); });
			break;
		}
		case oc::ControlType::Font: {
			row.font = new FontPicker::FontButton();
			row.font->setToolTip(c.help);
			row.font->setAccessibleName(c.label);
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.font, r, 1);
			QStringList suggested;
			for (const oc::SelectOption &o : c.options)
				suggested.append(o.value);
			QObject::connect(row.font, &QPushButton::clicked, this, [this, id, suggested]() {
				QPointer<OverlayPanel> self(this);
				const QStringList kinds = oc::BoundSourceKinds(m_id, id);
				FontPicker::Open(this, m_rows[id].font->family(), suggested, kinds,
						 [self, id, kinds](const FontPicker::Picked &p) {
							 if (!self || !self->m_rows.contains(id))
								 return;
							 self->m_rows[id].font->setFamily(p.family);
							 self->write(id, p.family);
							 if (!p.installed)
								 return;
							 if (kinds.contains("text_ft2_source")) {
								 // FreeType 2 only reads fonts installed for
								 // everyone: show the file so they can.
								 self->showStatus(T("FontPicker.InstalledFt2").arg(p.family));
								 QDesktopServices::openUrl(
									 QUrl::fromLocalFile(QFileInfo(p.file).absolutePath()));
							 } else if (kinds.contains("text_gdiplus")) {
								 self->showStatus(T("FontPicker.InstalledRestart").arg(p.family));
							 } else {
								 self->showStatus(T("FontPicker.Installed").arg(p.family));
							 }
						 });
			});
			break;
		}
		case oc::ControlType::Select: {
			QComboBox *combo = nullptr;
			if (c.editable) {
				combo = OverlayUI::EditableCombo(true);
			} else {
				auto *mac = new su::MacComboBox();
				mac->setOnCard(true);
				mac->setStyleSheet(su::comboStyle(true));
				combo = mac;
			}
			su::makeComboAnimated(combo);
			combo->setToolTip(c.help);
			combo->setAccessibleName(c.label);
			for (const oc::SelectOption &o : c.options)
				combo->addItem(o.label, o.value);
			combo->setMaxVisibleItems(16);
			row.combo = combo;
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(combo, r, 1);
			QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, id, combo](int i) {
				if (m_updating || i < 0)
					return;
				write(id, combo->itemData(i).toString());
			});
			if (c.editable) {
				auto *timer = new QTimer(combo);
				timer->setSingleShot(true);
				timer->setInterval(500);
				QObject::connect(combo, &QComboBox::editTextChanged, timer, [this, timer]() {
					if (!m_updating)
						timer->start();
				});
				QObject::connect(timer, &QTimer::timeout, this, [this, id, combo]() {
					const int i = combo->findText(combo->currentText());
					write(id, i >= 0 ? combo->itemData(i).toString() : combo->currentText());
				});
			}
			break;
		}
		case oc::ControlType::Image: {
			auto *box = new QWidget();
			auto *h = new QHBoxLayout(box);
			h->setContentsMargins(0, 0, 0, 0);
			h->setSpacing(su::S(8));
			row.thumb = new QLabel();
			row.thumb->setFixedSize(su::S(96), su::S(54));
			row.thumb->setAlignment(Qt::AlignCenter);
			row.thumb->setStyleSheet(su::scale_qss(
				QString("QLabel{background:%1;border-radius:6px;color:%2;font-size: 12px;}")
					.arg(su::Colors::INPUT_ON_CARD, su::Colors::TEXT_MUTED)));
			row.imagePath = new QLineEdit();
			row.imagePath->setStyleSheet(su::lineEditStyle(true));
			row.imagePath->setFixedHeight(su::S(28));
			row.imagePath->setPlaceholderText(T("OverlayControls.Image.Placeholder"));
			row.imagePath->setToolTip(c.help);
			row.imagePath->setAccessibleName(c.label);
			auto *browse = new su::PillButton(T("OverlayControls.Image.Change"), "neutral");
			browse->setAccessibleName(T("OverlayControls.Image.Pick").arg(c.label));
			h->addWidget(row.thumb);
			h->addWidget(browse);
			h->addStretch(1);
			h->addWidget(row.imagePath);
			// The file's path is not something to read: it is the tooltip.
			row.imagePath->hide();
			row.thumb->setToolTip(c.value.toString());
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(box, r, 1);
			QLineEdit *edit = row.imagePath;
			QLabel *thumb = row.thumb;
			QObject::connect(edit, &QLineEdit::editingFinished, this, [this, id, edit, thumb]() {
				if (m_rows.contains(id) && edit->text() != m_rows[id].info.value.toString()) {
					write(id, edit->text());
					ShowThumb(thumb, edit->text());
				}
			});
			QObject::connect(browse, &QPushButton::clicked, this, [this, id, edit, thumb]() {
				const QString current = edit->text();
				const QString start = current.isEmpty() ? QString() : QFileInfo(current).absolutePath();
				const QString path = QFileDialog::getOpenFileName(this, T("OverlayControls.Image.Pick").arg(m_rows[id].info.label),
										  start, ImageFilter());
				if (path.isEmpty())
					return;
				edit->setText(path);
				write(id, path);
				ShowThumb(thumb, path);
				thumb->setToolTip(path);
			});
			break;
		}
		case oc::ControlType::Platforms: {
			// A list needs the full width: label and reset on their own
			// line above it.
			grid->addWidget(labelBox, r, 0, Qt::AlignVCenter);
			grid->addWidget(row.reset, r, 3, Qt::AlignVCenter | Qt::AlignRight);
			row.platforms = new OverlayUI::PlatformsEditor(c);
			row.platforms->onChanged = [this, id](const QJsonArray &rows) { write(id, rows); };
			grid->addWidget(row.platforms, r + 1, 0, 1, 4);
			next = r + 2;
			break;
		}
		case oc::ControlType::Layout:
			break;
		}
		if (c.type != oc::ControlType::Platforms)
			grid->addWidget(row.reset, r, 3, Qt::AlignVCenter | Qt::AlignRight);

		m_rows.insert(id, row);
		showValue(m_rows[id]);
		return next;
	}

	// Pushes a row's stored value into its widgets without writing anything back.
	void showValue(Row &row)
	{
		m_updating = true;
		const oc::ControlInfo &c = row.info;
		switch (c.type) {
		case oc::ControlType::Colour:
			row.swatch->setText(c.value.toString());
			row.swatch->setStyleSheet(SwatchStyle(ParseHex(c.value.toString())));
			break;
		case oc::ControlType::Slider:
			row.slider->setValue(int(std::lround((c.value.toDouble() - c.min) / c.step)));
			if (!row.sliderSpin->hasFocus())
				row.sliderSpin->setValue(c.value.toDouble());
			break;
		case oc::ControlType::Number:
			row.number->setValue(c.value.toDouble());
			break;
		case oc::ControlType::Toggle:
			row.toggle->setChecked(c.value.toBool());
			break;
		case oc::ControlType::Text:
			// Never under the user's cursor: their typing wins until it lands.
			if (!row.text->hasFocus() && row.text->text() != c.value.toString())
				row.text->setText(c.value.toString());
			break;
		case oc::ControlType::TextArea:
			if (!row.area->hasFocus() && row.area->toPlainText() != c.value.toString())
				row.area->setPlainText(c.value.toString());
			break;
		case oc::ControlType::Font:
			row.font->setFamily(c.value.toString());
			break;
		case oc::ControlType::Select: {
			const QString v = c.value.toString();
			const int i = row.combo->findData(v);
			if (i >= 0)
				row.combo->setCurrentIndex(i);
			else if (row.combo->isEditable() && !row.combo->hasFocus())
				row.combo->setEditText(v);
			break;
		}
		case oc::ControlType::Platforms:
			row.platforms->setValue(c.value.toArray());
			break;
		case oc::ControlType::Image:
			if (!row.imagePath->hasFocus() && row.imagePath->text() != c.value.toString()) {
				row.imagePath->setText(c.value.toString());
				ShowThumb(row.thumb, c.value.toString());
			} else if (row.thumb->pixmap().isNull() && !c.value.toString().isEmpty()) {
				ShowThumb(row.thumb, c.value.toString());
			} else if (c.value.toString().isEmpty()) {
				ShowThumb(row.thumb, QString());
			}
			break;
		case oc::ControlType::Layout:
			break;
		}
		row.reset->setVisible(c.applied && DiffersFromDefault(c, c.value));
		m_updating = false;
	}

	// Leading edge goes straight through so a click feels instant; anything
	// arriving inside the throttle window waits for the timer, latest value
	// wins.
	void write(const QString &controlId, const QJsonValue &value)
	{
		m_pending.insert(controlId, value);
		if (!m_flushTimer->isActive()) {
			flushWrites();
			m_flushTimer->start();
		}
	}

	// Returns whether anything was written.
	bool flushWrites()
	{
		if (m_pending.isEmpty())
			return false;
		const QHash<QString, QJsonValue> pending = std::exchange(m_pending, {});
		for (auto it = pending.cbegin(); it != pending.cend(); ++it)
			apply(it.key(), it.value());
		return true;
	}

	void apply(const QString &controlId, const QJsonValue &value)
	{
		m_writing = true;
		QString error;
		const bool ok = oc::SetControlValue(m_id, controlId, value, &error);
		m_writing = false;
		if (!ok) {
			showStatus(error);
			return;
		}
		if (m_rows.contains(controlId)) {
			Row &row = m_rows[controlId];
			row.info.value = value;
			row.info.applied = true;
			row.reset->setVisible(DiffersFromDefault(row.info, value));
			row.drift->hide();
		}
	}

	void pickColour(const QString &controlId)
	{
		Row &row = m_rows[controlId];
		const QString before = row.info.value.toString();
		const bool wasApplied = row.info.applied;

		// Parented to the main window, not this panel: a child would be torn
		// down inside the panel's destructor, where the revert below could
		// still reach half-destroyed rows.
		auto sh = su::makeWindow(row.info.label, QString(), MainWindow(), /*brandFooter=*/false, "StreamUP", {},
					 /*popup=*/true);
		auto *cp = new su::ColorPicker();
		cp->setColor(ParseHex(before));
		sh.content->setContentsMargins(su::S(16), su::S(16), su::S(16), su::S(8));
		sh.content->addWidget(cp);

		auto *cancel = new su::PillButton(T("OverlayControls.Button.Cancel"), "outline");
		auto *ok = new su::PillButton(T("OverlayControls.Button.Select"), "primary");
		sh.footerButtons->addWidget(cancel);
		sh.footerButtons->addWidget(ok);

		// Live preview: the overlay follows the picker. Cancel, or closing
		// the picker any other way, puts back what was there before.
		auto accepted = std::make_shared<bool>(false);
		QPointer<OverlayPanel> self(this);
		cp->onChanged = [self, controlId](const QColor &c) {
			if (!self)
				return;
			self->write(controlId, HexText(c));
			if (self->m_rows.contains(controlId)) {
				Row &row = self->m_rows[controlId];
				row.info.value = HexText(c);
				self->showValue(row);
			}
		};
		QObject::connect(cancel, &QPushButton::clicked, sh.dialog, &QDialog::close);
		QObject::connect(ok, &QPushButton::clicked, sh.dialog, [accepted, dlg = sh.dialog]() {
			*accepted = true;
			dlg->close();
		});
		// The revert runs even when the panel has gone, so a cancelled
		// colour never sticks. An open panel hears about it through the
		// notifier like any other change.
		const QString overlay = m_id;
		QObject::connect(sh.dialog, &QObject::destroyed, qApp, [self, overlay, controlId, before, wasApplied, accepted]() {
			if (*accepted)
				return;
			if (self)
				self->dropPending(controlId);
			if (wasApplied)
				oc::SetControlValue(overlay, controlId, before);
			else
				oc::ResetControl(overlay, controlId);
		});
		// Closing the panel closes its picker, which reverts through the above.
		QPointer<su::ShadowDialog> picker(sh.dialog);
		QObject::connect(this, &QObject::destroyed, qApp, [picker]() {
			if (picker)
				picker->close();
		});

		sh.dialog->resize(su::S(360), su::S(430));
		sh.dialog->show();
	}

	// ------------------------------------------------------------ refresh

	void refreshControl(const QString &controlId)
	{
		oc::OverlayInfo info;
		if (!oc::GetOverlay(m_id, info))
			return;
		m_info = info;
		for (const oc::GroupInfo &g : info.groups) {
			for (const oc::ControlInfo &c : g.controls) {
				if (c.id != controlId || !m_rows.contains(c.id))
					continue;
				Row &row = m_rows[c.id];
				row.info = c;
				showValue(row);
			}
		}
		refreshSiblings();
		updateMissingNote();
	}

	// One pass for everything that changed since the last one: the rows that
	// were changed from outside this panel (the Layout page, websocket, a
	// preset), then the other pages.
	void runRefresh()
	{
		const QSet<QString> changed = std::exchange(m_changedControls, {});
		oc::OverlayInfo info;
		if (!oc::GetOverlay(m_id, info))
			return;
		m_info = info;
		for (const oc::GroupInfo &g : info.groups) {
			for (const oc::ControlInfo &c : g.controls) {
				if (!changed.contains(c.id) || !m_rows.contains(c.id))
					continue;
				Row &row = m_rows[c.id];
				row.info = c;
				showValue(row);
			}
		}
		if (m_layout)
			m_layout->refresh(info);
		if (!changed.isEmpty())
			updateMissingNote();
	}

	// Pages other than the one that made a change still need the new state.
	void refreshSiblings()
	{
		oc::OverlayInfo info;
		if (!oc::GetOverlay(m_id, info))
			return;
		m_info = info;
		if (m_layout)
			m_layout->refresh(info);
	}

	void refreshAll(bool sourcesChanged)
	{
		oc::OverlayInfo info;
		if (!oc::GetOverlay(m_id, info))
			return;
		m_info = info;
		for (const oc::GroupInfo &g : info.groups) {
			for (const oc::ControlInfo &c : g.controls) {
				if (!m_rows.contains(c.id))
					continue;
				Row &row = m_rows[c.id];
				row.info = c;
				showValue(row);
			}
		}
		fillPresets();
		updateMissingNote();
		refreshDrift();
		refreshActions();
		if (m_layout)
			m_layout->refresh(info);
	}

	void refreshDrift()
	{
		const QStringList drifted = oc::DriftedControls(m_id);
		for (auto it = m_rows.begin(); it != m_rows.end(); ++it)
			it->drift->setVisible(drifted.contains(it.key()));
	}

	void updateMissingNote()
	{
		if (!m_missingNote)
			return;
		if (m_info.missing > 0) {
			m_missingNote->setText(T("OverlayControls.Missing").arg(m_info.missing).arg(m_info.links));
			m_missingNote->show();
		} else {
			m_missingNote->hide();
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

	QString m_id;
	oc::OverlayInfo m_info;
	QListWidget *m_nav = nullptr;
	QStackedWidget *m_pages = nullptr;
	OverlayUI::LayoutPage *m_layout = nullptr;
	QHash<int, QString> m_lazyTabs; // page index -> tab id, built when first shown
	QLabel *m_missingNote = nullptr;
	QLabel *m_status = nullptr;
	su::MacComboBox *m_presets = nullptr;
	QToolButton *m_deletePreset = nullptr;
	QHash<QString, Row> m_rows;
	QList<ActionRow> m_actionRows;
	bool m_updating = false;
	bool m_writing = false;
	QTimer *m_flushTimer = nullptr;
	QTimer *m_refreshTimer = nullptr;
	QHash<QString, QJsonValue> m_pending;
	QSet<QString> m_changedControls;
};

QHash<QString, QPointer<OverlayPanel>> &OpenPanels()
{
	static QHash<QString, QPointer<OverlayPanel>> panels;
	return panels;
}

void ShowOnUiThread(const QString &overlayId)
{
	oc::OverlayInfo info;
	if (!oc::GetOverlay(overlayId, info)) {
		su::info(MainWindow(), T("OverlayControls.Window.TitleSuffix"), T("OverlayControls.NotFound"));
		return;
	}
	QPointer<OverlayPanel> &existing = OpenPanels()[overlayId];
	if (existing) {
		existing->showNormal();
		existing->raise();
		existing->activateWindow();
		return;
	}
	QElapsedTimer opening;
	opening.start();
	auto *panel = new OverlayPanel(overlayId, MainWindow());
	existing = panel;
	StreamUP::UIHelpers::PlaceOverMainWindow(panel);
	panel->show();
	panel->raise();
	panel->activateWindow();
	// Once the event loop is free again the panel is on screen and usable.
	const QString name = info.name;
	QTimer::singleShot(0, qApp, [opening, name]() {
		StreamUP::DebugLogger::LogInfoFormat("OverlayControls", "Opened the controls for '%s' in %lld ms",
						      name.toUtf8().constData(), (long long)opening.elapsed());
	});
}

} // namespace

void Show(const QString &overlayId)
{
	QMetaObject::invokeMethod(qApp, [overlayId]() { ShowOnUiThread(overlayId); }, Qt::QueuedConnection);
}

void OfferAfterInstall(const QString &overlayId)
{
	QMetaObject::invokeMethod(
		qApp,
		[overlayId]() {
			oc::OverlayInfo info;
			if (!oc::GetOverlay(overlayId, info))
				return;
			su::confirm(MainWindow(), T("OverlayControls.Installed.Title"),
				    T("OverlayControls.Installed.Message").arg(info.name.toHtmlEscaped()),
				    T("OverlayControls.Installed.Open"), "primary",
				    [overlayId]() { ShowOnUiThread(overlayId); });
		},
		Qt::QueuedConnection);
}

void AttachFromFile()
{
	const QString path = QFileDialog::getOpenFileName(MainWindow(), T("OverlayControls.Attach.Title"), QString(),
							  "StreamUP File (*.streamup)");
	if (path.isEmpty())
		return;
	QString id;
	QString error;
	if (!oc::RegisterFromFile(path, &id, &error)) {
		su::info(MainWindow(), T("OverlayControls.Attach.Title"), error);
		return;
	}
	StreamUP::Products::CacheIconFromFile(path);
	ShowOnUiThread(id);
}

void ConfirmRemove(const QString &overlayId)
{
	oc::OverlayInfo info;
	if (!oc::GetOverlay(overlayId, info))
		return;
	su::confirm(MainWindow(), T("OverlayControls.Remove.Title"),
		    T("OverlayControls.Remove.Message").arg(info.name.toHtmlEscaped()), T("OverlayControls.Remove.Button"),
		    "danger", [overlayId]() { oc::RemoveOverlay(overlayId); });
}

} // namespace OverlayControlsWindow
} // namespace StreamUP
