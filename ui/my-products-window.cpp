#include "my-products-window.hpp"

#include "../core/file-manager.hpp"
#include "../core/overlay-controls.hpp"
#include "../core/product-registry.hpp"
#include "overlay-controls-window.hpp"
#include "overlay-ui-common.hpp"
#include "ui-helpers.hpp"
#include "version.h"

#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/search-field.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QRegularExpression>
#include <QImageReader>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace su = StreamUP::UIStyles;
namespace oc = StreamUP::OverlayControls;
namespace pr = StreamUP::Products;
using StreamUP::OverlayUI::T;

namespace StreamUP {
namespace MyProductsWindow {

namespace {

// Every row's image is this 16:9 box (design px), cropped to it, never stretched.
constexpr int kTileW = 64;
constexpr int kTileH = 36;

QWidget *MainWindow()
{
	return static_cast<QWidget *>(obs_frontend_get_main_window());
}

// Catppuccin accents, one per range, so a list of tiles reads at a glance.
QColor CategoryColour(const QString &category)
{
	static const QHash<QString, QColor> colours = {
		{"overlays", QColor("#89b4fa")}, {"alerts", QColor("#f38ba8")}, {"transitions", QColor("#cba6f7")},
		{"effects", QColor("#fab387")},  {"widgets", QColor("#a6e3a1")}, {"games", QColor("#f9e2af")},
		{"tools", QColor("#94e2d5")},
	};
	return colours.value(category, QColor("#9399b2"));
}

QString Initials(const QString &name)
{
	QString out;
	for (const QString &word : name.split(QRegularExpression("[\\s\\-_]+"), Qt::SkipEmptyParts)) {
		const QChar c = word.at(0);
		if (c.isLetterOrNumber())
			out.append(c.toUpper());
		if (out.size() == 2)
			break;
	}
	return out.isEmpty() ? QStringLiteral("?") : out;
}

// The product's own image when it shipped one, else its initials on its
// range's colour. Same 16:9 box and corner for both. An image of any other
// shape (an old square icon) is centre-cropped to fill the box.
QPixmap Tile(const pr::ProductEntry &p, qreal dpr)
{
	const int w = int(su::S(kTileW) * dpr);
	const int h = int(su::S(kTileH) * dpr);
	QPixmap pm(w, h);
	pm.fill(Qt::transparent);
	QPainter g(&pm);
	g.setRenderHint(QPainter::Antialiasing, true);
	g.setRenderHint(QPainter::SmoothPixmapTransform, true);
	const qreal radius = su::S(6) * dpr;
	QPainterPath clip;
	clip.addRoundedRect(QRectF(0, 0, w, h), radius, radius);
	g.setClipPath(clip);

	QImage img;
	if (!p.iconPath.isEmpty()) {
		QImageReader reader(p.iconPath);
		const QSize full = reader.size();
		if (full.isValid()) // cover: fill the box, crop the overflow evenly
			reader.setScaledSize(full.scaled(w, h, Qt::KeepAspectRatioByExpanding));
		img = reader.read();
	}
	if (!img.isNull()) {
		g.fillRect(QRect(0, 0, w, h), QColor(su::Colors::BG_SECONDARY));
		g.drawImage(QPointF((w - img.width()) / 2.0, (h - img.height()) / 2.0), img);
	} else {
		g.fillRect(QRect(0, 0, w, h), CategoryColour(p.category));
		QFont f = qApp->font();
		f.setPixelSize(int(h * 0.42));
		f.setBold(true);
		g.setFont(f);
		g.setPen(QColor("#11111b"));
		g.drawText(QRect(0, 0, w, h), Qt::AlignCenter, Initials(p.name));
	}
	g.end();
	pm.setDevicePixelRatio(dpr);
	return pm;
}

class MyProductsDialog : public su::ShadowDialog {
public:
	explicit MyProductsDialog(QWidget *parent) : su::ShadowDialog(parent)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		auto shell = su::applyChrome(this, T("MyProducts.Title"), "v" PROJECT_VERSION, /*brandFooter=*/true,
					     "StreamUP");

		// Search and the file-based extras sit above the list.
		auto *bar = new QWidget();
		bar->setObjectName("mpBar");
		bar->setStyleSheet(QString("QWidget#mpBar{background:%1;}").arg(su::Colors::BG_DARKEST));
		auto *barRow = new QHBoxLayout(bar);
		barRow->setContentsMargins(su::S(16), su::S(14), su::S(16), su::S(2));
		barRow->setSpacing(su::S(10));
		m_search = new su::SearchField();
		m_search->setPlaceholderText(T("MyProducts.Search"));
		m_search->setAccessibleName(T("MyProducts.Search"));
		m_search->setFixedHeight(su::S(30));
		barRow->addWidget(m_search, 1);
		auto *attach = new su::PillButton(T("MyProducts.AddControlsFromFile"), "neutral");
		attach->setToolTip(T("MyProducts.AddControlsFromFileTip"));
		barRow->addWidget(attach);
		shell.content->addWidget(bar);
		QObject::connect(attach, &QPushButton::clicked, this,
				 []() { StreamUP::OverlayControlsWindow::AttachFromFile(); });

		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
		su::useScrollBars(scroll);
		auto *page = new QWidget();
		page->setObjectName("mpPage");
		// Row dividers live here, switched per row by a property, so filtering
		// never restyles widgets one by one.
		page->setStyleSheet(QString("QWidget#mpPage{background:%1;}"
					    "QWidget#mpRow[divider=\"true\"]{border-top:1px solid %2;}")
					    .arg(su::Colors::BG_DARKEST, su::Colors::SEPARATOR));
		m_list = new QVBoxLayout(page);
		m_list->setContentsMargins(su::S(16), su::S(10), su::S(16), su::S(16));
		m_list->setSpacing(su::S(12));
		scroll->setWidget(page);
		shell.content->addWidget(scroll, 1);

		m_status = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
		m_status->setWordWrap(false);
		shell.footer->insertWidget(0, m_status);
		m_status->hide();

#ifdef _WIN32
		auto *install = new su::PillButton(T("MyProducts.Install"), "outline");
		shell.footerButtons->addWidget(install);
		QObject::connect(install, &QPushButton::clicked, this,
				 []() { StreamUP::FileManager::LoadStreamupFileWithWarning(); });
#endif
		auto *done = new su::PillButton(T("OverlayControls.Button.Done"), "primary");
		shell.footerButtons->addWidget(done);
		QObject::connect(done, &QPushButton::clicked, this, &QDialog::close);

		// Installs, removals and collection switches all change the list.
		// Several can arrive at once, so they share one rebuild.
		m_rebuild = new QTimer(this);
		m_rebuild->setSingleShot(true);
		m_rebuild->setInterval(150);
		QObject::connect(m_rebuild, &QTimer::timeout, this, [this]() { rebuild(); });
		QObject::connect(oc::GetNotifier(), &oc::Notifier::overlaysChanged, this, [this]() { m_rebuild->start(); });
		QObject::connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
		obs_frontend_add_event_callback(OnEvent, this);

		rebuild();
		const int m = su::S(su::ShadowDialog::kShadowMargin);
		resize(su::S(620) + 2 * m, su::S(640) + 2 * m);
	}

	~MyProductsDialog() override { obs_frontend_remove_event_callback(OnEvent, this); }

private:
	struct RowRef {
		QWidget *row;
		QString haystack; // name, range and scenes, lower case
	};
	struct Section {
		QWidget *card;
		QLabel *heading;
		QString label;
		QList<RowRef> rows;
	};

	static void OnEvent(enum obs_frontend_event event, void *param)
	{
		if (event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED && event != OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED)
			return;
		auto *self = static_cast<MyProductsDialog *>(param);
		QMetaObject::invokeMethod(self->m_rebuild, qOverload<>(&QTimer::start), Qt::QueuedConnection);
	}

	void clearList()
	{
		m_sections.clear();
		m_emptyNote = nullptr;
		while (QLayoutItem *item = m_list->takeAt(0)) {
			if (QWidget *w = item->widget())
				w->deleteLater();
			delete item;
		}
	}

	void rebuild()
	{
		clearList();
		const QList<pr::ProductEntry> products = pr::ListProducts();
		m_total = int(products.size());

		if (products.isEmpty()) {
			buildEmptyState();
			m_search->setEnabled(false);
			return;
		}
		m_search->setEnabled(true);

		QString current;
		Section *section = nullptr;
		QVBoxLayout *card = nullptr;
		for (const pr::ProductEntry &p : products) {
			if (p.category != current) {
				current = p.category;
				int count = 0;
				for (const pr::ProductEntry &q : products)
					count += q.category == current;
				card = su::sectionCard(m_list, QString("%1  %2").arg(pr::CategoryLabel(current)).arg(count));
				card->setSpacing(0);
				QLabel *heading = card->count() ? qobject_cast<QLabel *>(card->itemAt(0)->widget()) : nullptr;
				m_sections.append(Section{card->parentWidget(), heading, pr::CategoryLabel(current), {}});
				section = &m_sections.last();
			}
			QWidget *row = buildRow(p);
			card->addWidget(row);
			QString hay = p.name + " " + pr::CategoryLabel(p.category) + " " + p.scenes.join(" ");
			section->rows.append(RowRef{row, hay.toLower()});
		}
		m_emptyNote = su::makeLabel(QString(), su::CardText::kBody, 400, su::Colors::TEXT_SECONDARY);
		m_emptyNote->setAlignment(Qt::AlignCenter);
		m_emptyNote->setWordWrap(true);
		m_emptyNote->hide();
		m_list->addWidget(m_emptyNote);
		m_list->addStretch(1);
		applyFilter();
	}

	void buildEmptyState()
	{
		m_list->addStretch(1);
		QLabel *title = su::makeLabel(T("MyProducts.EmptyTitle"), su::CardText::kTitle, 700, su::Colors::TEXT_PRIMARY);
		title->setAlignment(Qt::AlignCenter);
		m_list->addWidget(title);
		QLabel *body = su::makeLabel(T("MyProducts.Empty"), su::CardText::kBody, 400, su::Colors::TEXT_SECONDARY);
		body->setAlignment(Qt::AlignCenter);
		body->setWordWrap(true);
		m_list->addWidget(body);
		m_list->addStretch(2);
	}

	QWidget *buildRow(const pr::ProductEntry &p)
	{
		auto *row = new QWidget();
		row->setObjectName("mpRow");
		row->setAttribute(Qt::WA_StyledBackground, true);
		auto *h = new QHBoxLayout(row);
		h->setContentsMargins(0, su::S(8), 0, su::S(8));
		h->setSpacing(su::S(12));

		auto *tile = new QLabel();
		tile->setFixedSize(su::S(kTileW), su::S(kTileH));
		tile->setPixmap(Tile(p, devicePixelRatioF()));
		tile->setAccessibleName(p.name);
		h->addWidget(tile, 0, Qt::AlignVCenter);

		auto *text = new QVBoxLayout();
		text->setContentsMargins(0, 0, 0, 0);
		text->setSpacing(su::S(1));
		QLabel *name = su::makeLabel(p.name, su::Sizes::FONT_SIZE_SMALL, 600, su::Colors::TEXT_PRIMARY);
		name->setWordWrap(false);
		text->addWidget(name);
		QStringList facts;
		if (!p.version.isEmpty())
			facts.append("v" + p.version);
		facts.append(p.scenes.size() == 1 ? T("MyProducts.OneScene") : T("MyProducts.Scenes").arg(p.scenes.size()));
		QString metaText = facts.join(QString::fromUtf8("  ·  "));
		const char *metaColour = su::Colors::TEXT_MUTED;
		if (p.overlayId.isEmpty() && p.shippedControls) {
			metaText += QString::fromUtf8("  ·  ") + T("MyProducts.NoControlsShort");
			metaColour = su::Colors::COLOR_WARNING;
		}
		QLabel *meta = su::makeLabel(metaText, su::Sizes::FONT_SIZE_TINY, 500, metaColour);
		meta->setWordWrap(false);
		meta->setToolTip(p.overlayId.isEmpty() && p.shippedControls ?T("MyProducts.NoControls") : p.scenes.join("\n"));
		text->addWidget(meta);
		h->addLayout(text, 1);

		if (!p.overlayId.isEmpty()) {
			auto *adjust = new su::PillButton(T("MyProducts.Adjust"), "primary");
			adjust->setAccessibleName(T("MyProducts.AdjustAccessible").arg(p.name));
			const QString id = p.overlayId;
			QObject::connect(adjust, &QPushButton::clicked, this,
					 [id]() { StreamUP::OverlayControlsWindow::Show(id); });
			h->addWidget(adjust, 0, Qt::AlignVCenter);
		} else if (p.shippedControls) {
			auto *attach = new su::PillButton(T("MyProducts.AddControls"), "neutral");
			attach->setToolTip(T("MyProducts.NoControls"));
			QObject::connect(attach, &QPushButton::clicked, this,
					 []() { StreamUP::OverlayControlsWindow::AttachFromFile(); });
			h->addWidget(attach, 0, Qt::AlignVCenter);
		}

		// Everything the old Overlays menu offered, per product.
		if (!p.overlayId.isEmpty()) {
			QToolButton *more = OverlayUI::IconButton(QString::fromUtf8("⋯"), T("MyProducts.More"),
								  T("MyProducts.MoreAccessible").arg(p.name));
			const pr::ProductEntry product = p;
			QObject::connect(more, &QToolButton::clicked, this, [this, more, product]() { showMenu(more, product); });
			h->addWidget(more, 0, Qt::AlignVCenter);
		}
		return row;
	}

	void showMenu(QToolButton *anchor, const pr::ProductEntry &p)
	{
		QMenu menu(this);
		menu.setStyleSheet(su::menuStyle());
		const QString overlay = p.overlayId;
		menu.addAction(T("MyProducts.Adjust"), this, [overlay]() { StreamUP::OverlayControlsWindow::Show(overlay); });
		if (p.isTransition) {
			const QString action = p.transitionAction;
			menu.addAction(T("OverlayControls.Action.Use"), this, [this, overlay, action]() {
				QString message;
				QString error;
				if (oc::RunAction(overlay, action, &message, &error))
					showStatus(message);
				else
					su::info(this, T("OverlayControls.Group.Transition"), error);
			});
		}
		menu.addSeparator();
		menu.addAction(T("Menu.Overlays.Remove"), this,
			       [overlay]() { StreamUP::OverlayControlsWindow::ConfirmRemove(overlay); });
		menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height())));
	}

	void applyFilter()
	{
		const QString needle = m_search->text().trimmed().toLower();
		int shown = 0;
		for (Section &s : m_sections) {
			int inSection = 0;
			for (RowRef &r : s.rows) {
				const bool match = needle.isEmpty() || r.haystack.contains(needle);
				r.row->setVisible(match);
				// A hairline between visible rows, none above the first.
				const bool divider = match && inSection > 0;
				if (r.row->property("divider").toBool() != divider) {
					r.row->setProperty("divider", divider);
					r.row->style()->unpolish(r.row);
					r.row->style()->polish(r.row);
				}
				inSection += match;
			}
			s.card->setVisible(inSection > 0);
			if (s.heading)
				s.heading->setText(QString("%1  %2").arg(s.label.toUpper()).arg(inSection)); // as sectionHeader() shows it
			shown += inSection;
		}
		if (m_emptyNote) {
			m_emptyNote->setText(T("MyProducts.NoMatch").arg(m_search->text().trimmed().toHtmlEscaped()));
			m_emptyNote->setVisible(m_total > 0 && shown == 0);
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

	QVBoxLayout *m_list = nullptr;
	su::SearchField *m_search = nullptr;
	QLabel *m_status = nullptr;
	QLabel *m_emptyNote = nullptr;
	QTimer *m_rebuild = nullptr;
	QList<Section> m_sections;
	int m_total = 0;
};

QPointer<MyProductsDialog> &Open()
{
	static QPointer<MyProductsDialog> dialog;
	return dialog;
}

void ShowOnUiThread()
{
	QPointer<MyProductsDialog> &existing = Open();
	if (existing) {
		existing->showNormal();
		existing->raise();
		existing->activateWindow();
		return;
	}
	auto *dlg = new MyProductsDialog(MainWindow());
	existing = dlg;
	StreamUP::UIHelpers::PlaceOverMainWindow(dlg);
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
}

} // namespace

void Show()
{
	QMetaObject::invokeMethod(qApp, []() { ShowOnUiThread(); }, Qt::QueuedConnection);
}

} // namespace MyProductsWindow
} // namespace StreamUP
