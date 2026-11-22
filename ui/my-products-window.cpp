#include "my-products-window.hpp"

#include "../core/file-manager.hpp"
#include "../core/overlay-controls.hpp"
#include "../core/product-registry.hpp"
#include "overlay-controls-window.hpp"
#include "overlay-ui-common.hpp"
#include "ui-helpers.hpp"
#include "version.h"

#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/gallery-style.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/search-field.hpp>
#include <streamup/ui/section-card.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
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

// A category card's header: the range's name and count as the house section
// header (a real sectionHeader() label, so it matches every other StreamUP
// window), with a painted chevron that shows whether the card is open.
class SectionHeader : public QAbstractButton {
public:
	explicit SectionHeader(QWidget *parent = nullptr) : QAbstractButton(parent)
	{
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setFocusPolicy(Qt::TabFocus);
		auto *h = new QHBoxLayout(this);
		h->setContentsMargins(0, 0, su::S(28), 0);
		h->setSpacing(su::S(8));
		m_title = su::sectionHeader(QString());
		m_title->setContentsMargins(0, 0, 0, 0);
		m_title->setWordWrap(false);
		m_count = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 700, su::Colors::TEXT_MUTED);
		for (QLabel *l : {m_title, m_count})
			l->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		h->addWidget(m_title);
		h->addWidget(m_count, 0, Qt::AlignVCenter);
		h->addStretch(1);
		setFixedHeight(su::S(34));
	}
	void setLabel(const QString &label, int count)
	{
		m_title->setText(label.toUpper());
		m_count->setText(QString::number(count));
		setAccessibleName(QString("%1 %2").arg(label).arg(count));
	}
	void setExpanded(bool on)
	{
		m_expanded = on;
		update();
	}
	void setCollapsible(bool on)
	{
		m_collapsible = on;
		setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
		setEnabled(on);
		update();
	}
	QSize sizeHint() const override { return QSize(su::S(200), su::S(34)); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		if (!m_collapsible)
			return;
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		const QRectF r = QRectF(rect());
		QPen pen(QColor(underMouse() ? su::Colors::TEXT_PRIMARY : su::Colors::TEXT_SECONDARY),
			 std::max(1.3, su::S(2) * 0.8));
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.save();
		p.translate(QPointF(r.right() - su::S(12), r.center().y()));
		p.rotate(m_expanded ? 0.0 : -90.0); // down open, right closed
		su::detail::drawChevron(p, QPointF(0, 0), su::S(8), /*up=*/false);
		p.restore();
	}

private:
	QLabel *m_title = nullptr;
	QLabel *m_count = nullptr;
	bool m_expanded = true;
	bool m_collapsible = true;
};

// The sidebar's rows: the name on the left, its count on the right.
class CountDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
	{
		QStyledItemDelegate::paint(p, option, index);
		const QVariant count = index.data(Qt::UserRole + 1);
		if (!count.isValid())
			return;
		p->save();
		QFont f = option.font;
		f.setPixelSize(su::S(12));
		f.setBold(true);
		p->setFont(f);
		const bool selected = option.state & QStyle::State_Selected;
		p->setPen(QColor(selected ? "#ffffff" : su::Colors::TEXT_MUTED));
		p->drawText(option.rect.adjusted(0, 0, -su::S(16), 0), Qt::AlignVCenter | Qt::AlignRight,
			    QString::number(count.toInt()));
		p->restore();
	}
};

// Which categories are folded away, kept between sessions.
QString ViewStatePath()
{
	char *path = obs_module_config_path("my-products.json");
	const QString p = path ? QString::fromUtf8(path) : QString();
	bfree(path);
	return p;
}

QSet<QString> LoadCollapsed()
{
	QSet<QString> out;
	QFile f(ViewStatePath());
	if (f.open(QIODevice::ReadOnly))
		for (const QJsonValue v : QJsonDocument::fromJson(f.readAll()).object().value("collapsed").toArray())
			out.insert(v.toString());
	return out;
}

void SaveCollapsed(const QSet<QString> &collapsed)
{
	const QString path = ViewStatePath();
	if (path.isEmpty())
		return;
	QDir().mkpath(QFileInfo(path).absolutePath());
	QJsonArray arr;
	for (const QString &c : collapsed)
		arr.append(c);
	QFile f(path);
	if (f.open(QIODevice::WriteOnly))
		f.write(QJsonDocument(QJsonObject{{"collapsed", arr}}).toJson(QJsonDocument::Compact));
}

class MyProductsDialog : public su::ShadowDialog {
public:
	explicit MyProductsDialog(QWidget *parent) : su::ShadowDialog(parent)
	{
		setAttribute(Qt::WA_DeleteOnClose, true);
		m_collapsed = LoadCollapsed();
		auto shell = su::applyChrome(this, T("MyProducts.Title"), "v" PROJECT_VERSION, /*brandFooter=*/true,
					     "StreamUP");

		// Sidebar + content, the layout of StreamUP Settings and the controls window.
		auto *body = new QWidget();
		body->setObjectName("mpBody");
		body->setStyleSheet(QString("QWidget#mpBody{background:%1;}").arg(su::Colors::BG_DARKEST));
		auto *row = new QHBoxLayout(body);
		row->setContentsMargins(su::S(16), su::S(16), 0, 0);
		row->setSpacing(0);

		m_rail = new QFrame();
		m_rail->setObjectName("mpRail");
		m_rail->setFixedWidth(su::S(180));
		m_rail->setStyleSheet(su::scale_qss(
			QString("QFrame#mpRail{background:%1;border-radius:22px;}").arg(su::Colors::BG_PRIMARY)));
		auto *rl = new QVBoxLayout(m_rail);
		rl->setContentsMargins(su::S(6), su::S(6), su::S(6), su::S(6));
		m_nav = new QListWidget();
		m_nav->setStyleSheet(OverlayUI::NavListStyle());
		m_nav->setItemDelegate(new CountDelegate(m_nav));
		m_nav->setAccessibleName(T("MyProducts.Categories"));
		rl->addWidget(m_nav);
		m_railBox = new QWidget();
		auto *railTop = new QVBoxLayout(m_railBox);
		railTop->setContentsMargins(0, 0, 0, su::S(16));
		railTop->addWidget(m_rail);
		row->addWidget(m_railBox);

		auto *right = new QVBoxLayout();
		right->setContentsMargins(0, 0, 0, 0);
		right->setSpacing(0);
		row->addLayout(right, 1);

		auto *bar = new QWidget();
		auto *barRow = new QHBoxLayout(bar);
		barRow->setContentsMargins(su::S(16), 0, su::S(16), su::S(4));
		barRow->setSpacing(su::S(10));
		m_search = new su::SearchField();
		m_search->setPlaceholderText(T("MyProducts.Search"));
		m_search->setAccessibleName(T("MyProducts.Search"));
		m_search->setFixedHeight(su::S(30));
		barRow->addWidget(m_search, 1);
		auto *attach = new su::PillButton(T("MyProducts.AddControlsFromFile"), "neutral");
		attach->setToolTip(T("MyProducts.AddControlsFromFileTip"));
		barRow->addWidget(attach);
		right->addWidget(bar);
		QObject::connect(attach, &QPushButton::clicked, this,
				 []() { StreamUP::OverlayControlsWindow::AttachFromFile(); });

		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll->setStyleSheet("QScrollArea{background:transparent;border:none;}");
		su::useScrollBars(scroll);
		m_scroll = scroll;
		auto *page = new QWidget();
		page->setObjectName("mpPage");
		// Row dividers and cards live here, switched per row by a property,
		// so filtering never restyles widgets one by one.
		page->setStyleSheet(su::scale_qss(QString("QWidget#mpPage{background:%1;}"
							  "QFrame#mpCard{background:%2;border-radius:18px;}"
							  "QWidget#mpRow[divider=\"true\"]{border-top:1px solid %3;}")
							  .arg(su::Colors::BG_DARKEST, su::Colors::BG_PRIMARY, su::Colors::SEPARATOR)));
		m_list = new QVBoxLayout(page);
		m_list->setContentsMargins(su::S(16), su::S(8), su::S(16), su::S(16));
		m_list->setSpacing(su::S(12));
		scroll->setWidget(page);
		right->addWidget(scroll, 1);
		shell.content->addWidget(body, 1);

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
		QObject::connect(m_search, &QLineEdit::textChanged, this, [this]() {
			// Search always runs across everything.
			if (!m_search->text().trimmed().isEmpty() && m_nav->currentRow() > 0) {
				const QSignalBlocker block(m_nav);
				m_nav->setCurrentRow(0);
				m_category.clear();
			}
			applyFilter();
		});
		// Screen readers select a row without making it current; follow them.
		QObject::connect(m_nav, &QListWidget::itemSelectionChanged, this, [this]() {
			const QList<QListWidgetItem *> sel = m_nav->selectedItems();
			if (!sel.isEmpty() && m_nav->row(sel.first()) != m_nav->currentRow())
				m_nav->setCurrentRow(m_nav->row(sel.first()));
		});
		QObject::connect(m_nav, &QListWidget::currentRowChanged, this, [this](int r) {
			if (r < 0)
				return;
			m_category = m_nav->item(r)->data(Qt::UserRole + 2).toString();
			if (!m_category.isEmpty() && !m_search->text().isEmpty()) {
				const QSignalBlocker block(m_search);
				m_search->clear();
			}
			applyFilter();
			m_scroll->verticalScrollBar()->setValue(0);
		});
		obs_frontend_add_event_callback(OnEvent, this);

		rebuild();
		const int m = su::S(su::ShadowDialog::kShadowMargin);
		resize(su::S(m_railBox->isHidden() ? 660 : 860) + 2 * m, su::S(660) + 2 * m);
	}

	~MyProductsDialog() override { obs_frontend_remove_event_callback(OnEvent, this); }

private:
	struct RowRef {
		QWidget *row;
		QString haystack; // name, range and scenes, lower case
	};
	struct Section {
		QString key;
		QString label;
		QFrame *card;
		SectionHeader *header;
		QWidget *body;
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

		// Sidebar: All, then each category in display order.
		{
			const QSignalBlocker block(m_nav);
			m_nav->clear();
			auto *all = new QListWidgetItem(T("MyProducts.All"));
			all->setData(Qt::UserRole + 1, m_total);
			all->setData(Qt::UserRole + 2, QString());
			m_nav->addItem(all);
			for (const QString &key : pr::CategoryOrder()) {
				int count = 0;
				for (const pr::ProductEntry &p : products)
					count += p.category == key;
				if (!count)
					continue;
				auto *item = new QListWidgetItem(pr::CategoryLabel(key));
				item->setData(Qt::UserRole + 1, count);
				item->setData(Qt::UserRole + 2, key);
				m_nav->addItem(item);
			}
			int select = 0;
			for (int i = 0; i < m_nav->count(); ++i)
				if (m_nav->item(i)->data(Qt::UserRole + 2).toString() == m_category)
					select = i;
			m_nav->setCurrentRow(select);
			m_category = m_nav->item(select)->data(Qt::UserRole + 2).toString();
		}
		// One category needs no sidebar.
		m_railBox->setVisible(m_nav->count() > 2);

		if (products.isEmpty()) {
			buildEmptyState();
			m_search->setEnabled(false);
			return;
		}
		m_search->setEnabled(true);

		Section *section = nullptr;
		for (const pr::ProductEntry &p : products) {
			if (!section || section->key != p.category) {
				Section s;
				s.key = p.category;
				s.label = pr::CategoryLabel(p.category);
				s.card = new QFrame();
				s.card->setObjectName("mpCard");
				s.card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
				auto *cardLay = new QVBoxLayout(s.card);
				cardLay->setContentsMargins(su::S(16), su::S(8), su::S(16), su::S(10));
				cardLay->setSpacing(0);
				s.header = new SectionHeader();
				cardLay->addWidget(s.header);
				s.body = new QWidget();
				auto *bodyLay = new QVBoxLayout(s.body);
				bodyLay->setContentsMargins(0, 0, 0, 0);
				bodyLay->setSpacing(0);
				cardLay->addWidget(s.body);
				m_list->addWidget(s.card);
				const QString key = s.key;
				QObject::connect(s.header, &QAbstractButton::clicked, this, [this, key]() { toggle(key); });
				m_sections.append(s);
				section = &m_sections.last();
			}
			QWidget *rowWidget = buildRow(p);
			section->body->layout()->addWidget(rowWidget);
			const QString hay = p.name + " " + pr::CategoryLabel(p.category) + " " + p.scenes.join(" ");
			section->rows.append(RowRef{rowWidget, hay.toLower()});
		}
		m_emptyNote = su::makeLabel(QString(), su::CardText::kBody, 400, su::Colors::TEXT_SECONDARY);
		m_emptyNote->setAlignment(Qt::AlignCenter);
		m_emptyNote->setWordWrap(true);
		m_emptyNote->hide();
		m_list->addWidget(m_emptyNote);
		m_list->addStretch(1);
		applyFilter();
	}

	void toggle(const QString &key)
	{
		if (m_collapsed.contains(key))
			m_collapsed.remove(key);
		else
			m_collapsed.insert(key);
		SaveCollapsed(m_collapsed);
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
		meta->setToolTip(p.overlayId.isEmpty() && p.shippedControls ? T("MyProducts.NoControls") : p.scenes.join("\n"));
		text->addWidget(meta);
		h->addLayout(text, 1);

		if (!p.overlayId.isEmpty()) {
			auto *adjust = new su::PillButton(T("MyProducts.Adjust"), "primary");
			adjust->setAccessibleName(T("MyProducts.AdjustAccessible").arg(p.name));
			const QString id = p.overlayId;
			QObject::connect(adjust, &QPushButton::clicked, this,
					 [id]() { StreamUP::OverlayControlsWindow::Show(id); });
			h->addWidget(adjust, 0, Qt::AlignVCenter);
			// Everything the old Overlays menu offered, per product.
			auto *more = new OverlayUI::GlyphButton(OverlayUI::GlyphButton::Glyph::More, T("MyProducts.More"),
								T("MyProducts.MoreAccessible").arg(p.name), /*filled=*/true);
			const pr::ProductEntry product = p;
			QObject::connect(more, &QToolButton::clicked, this, [this, more, product]() { showMenu(more, product); });
			h->addWidget(more, 0, Qt::AlignVCenter);
		} else if (p.shippedControls) {
			auto *attach = new su::PillButton(T("MyProducts.AddControls"), "neutral");
			attach->setToolTip(T("MyProducts.NoControls"));
			QObject::connect(attach, &QPushButton::clicked, this,
					 []() { StreamUP::OverlayControlsWindow::AttachFromFile(); });
			h->addWidget(attach, 0, Qt::AlignVCenter);
		}
		return row;
	}

	void showMenu(QWidget *anchor, const pr::ProductEntry &p)
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
		menu.exec(anchor->mapToGlobal(QPoint(0, anchor->height() + su::S(4))));
	}

	// One pass for search, the chosen category and folded cards.
	void applyFilter()
	{
		const QString needle = m_search->text().trimmed().toLower();
		const bool searching = !needle.isEmpty();
		const bool single = !m_category.isEmpty();
		int shown = 0;
		for (Section &s : m_sections) {
			const bool inView = !single || s.key == m_category;
			int inSection = 0;
			for (RowRef &r : s.rows) {
				const bool match = inView && (!searching || r.haystack.contains(needle));
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
			// A category page and search results are always open; on All,
			// a card can be folded away and stays that way.
			const bool collapsible = !single && !searching;
			const bool expanded = !collapsible || !m_collapsed.contains(s.key);
			s.header->setLabel(s.label, inSection);
			s.header->setCollapsible(collapsible);
			s.header->setExpanded(expanded);
			s.body->setVisible(expanded);
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
	QScrollArea *m_scroll = nullptr;
	QFrame *m_rail = nullptr;
	QWidget *m_railBox = nullptr;
	QListWidget *m_nav = nullptr;
	su::SearchField *m_search = nullptr;
	QLabel *m_status = nullptr;
	QLabel *m_emptyNote = nullptr;
	QTimer *m_rebuild = nullptr;
	QList<Section> m_sections;
	QSet<QString> m_collapsed;
	QString m_category; // empty: All
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
