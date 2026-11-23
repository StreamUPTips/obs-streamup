#include "font-picker.hpp"

#include "../core/google-fonts.hpp"
#include "overlay-ui-common.hpp"

#include <streamup/ui/labels.hpp>
#include <streamup/ui/mac-inputs.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/search-field.hpp>
#include <streamup/ui/segmented-control.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-module.h>

#include <QApplication>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHash>
#include <QListWidget>
#include <QPainter>
#include <QPointer>
#include <QRunnable>
#include <QScrollBar>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <memory>

namespace su = StreamUP::UIStyles;
namespace gf = StreamUP::GoogleFonts;
using StreamUP::OverlayUI::T;

namespace StreamUP {
namespace FontPicker {

namespace {

constexpr int kRoleFamily = Qt::UserRole + 1;
constexpr int kRoleGoogle = Qt::UserRole + 2;  // true for a Google row
constexpr int kRoleHeader = Qt::UserRole + 3;  // section header row
constexpr int kRoleLoaded = Qt::UserRole + 4;  // drawn in its own face

// Draws a row's name in its own face when the face can render Latin text
// (symbol fonts would show boxes and spam DirectWrite warnings).
void ApplyFace(QListWidgetItem *item, const QString &face)
{
	if (!QFontDatabase::writingSystems(face).contains(QFontDatabase::Latin))
		return;
	QFont f(face);
	f.setPixelSize(su::S(17));
	item->setFont(f);
	item->setData(kRoleLoaded, true);
}

QListWidgetItem *Header(const QString &text)
{
	auto *item = new QListWidgetItem(text.toUpper());
	item->setFlags(Qt::NoItemFlags);
	item->setData(kRoleHeader, true);
	QFont f = qApp->font();
	f.setPixelSize(su::S(11));
	f.setBold(true);
	f.setLetterSpacing(QFont::AbsoluteSpacing, 0.6);
	item->setFont(f);
	item->setForeground(QColor(su::Colors::PRIMARY_COLOR));
	item->setSizeHint(QSize(0, su::S(32)));
	return item;
}

QListWidgetItem *Row(const QString &family, bool google)
{
	auto *item = new QListWidgetItem(family);
	item->setData(kRoleFamily, family);
	item->setData(kRoleGoogle, google);
	item->setSizeHint(QSize(0, su::S(36)));
	QFont f = qApp->font();
	f.setPixelSize(su::S(15));
	item->setFont(f);
	if (google)
		item->setToolTip(T("FontPicker.GoogleTip"));
	return item;
}

// Shared between the dialog and its background downloads, which may finish
// after the dialog has closed.
struct State {
	QPointer<QListWidget> list;
	QHash<QString, QListWidgetItem *> googleRows; // family -> row (valid while list is)
	QSet<QString> queued;
	std::vector<gf::Entry> catalog;
	bool catalogReady = false;
};

class Job : public QRunnable {
public:
	explicit Job(std::function<void()> fn) : m_fn(std::move(fn)) { setAutoDelete(true); }
	void run() override { m_fn(); }

private:
	std::function<void()> m_fn;
};

void Post(std::function<void()> fn)
{
	QMetaObject::invokeMethod(qApp, std::move(fn), Qt::QueuedConnection);
}

} // namespace

// ------------------------------------------------------------------ FontButton

FontButton::FontButton()
{
	setCursor(Qt::PointingHandCursor);
	setAttribute(Qt::WA_Hover, true);
	setFixedHeight(su::S(30));
	setMinimumWidth(su::S(200));
	setStyleSheet("QPushButton{border:none;background:transparent;}");
}

void FontButton::setFamily(const QString &family)
{
	m_family = family;
	setAccessibleDescription(family);
	update();
}

QSize FontButton::sizeHint() const
{
	return QSize(su::S(260), su::S(30));
}

void FontButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
	p.setPen(underMouse() || hasFocus() ? QPen(QColor(0, 118, 223, 160), 1.0) : QPen(Qt::NoPen));
	p.setBrush(QColor(su::Colors::INPUT_ON_CARD));
	p.drawRoundedRect(r, su::S(8), su::S(8));

	QFont f(m_family.isEmpty() ? qApp->font().family() : m_family);
	if (!QFontDatabase::writingSystems(m_family).contains(QFontDatabase::Latin))
		f = qApp->font();
	f.setPixelSize(su::S(14));
	p.setFont(f);
	p.setPen(QColor(m_family.isEmpty() ? su::Colors::TEXT_MUTED : su::Colors::TEXT_PRIMARY));
	const QRectF textRect = r.adjusted(su::S(10), 0, -su::S(30), 0);
	p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
		   QFontMetrics(f).elidedText(m_family.isEmpty() ? T("FontPicker.Choose") : m_family, Qt::ElideRight,
					      int(textRect.width())));

	QPen cp(QColor(su::Colors::TEXT_SECONDARY), std::max(1.3, su::S(2) * 0.8));
	cp.setCapStyle(Qt::RoundCap);
	cp.setJoinStyle(Qt::RoundJoin);
	p.setPen(cp);
	p.setBrush(Qt::NoBrush);
	su::detail::drawChevron(p, QPointF(r.right() - su::S(15), r.center().y()), su::S(8), /*up=*/false);
}

// ------------------------------------------------------------------ the picker

void Open(QWidget *parent, const QString &current, const QStringList &suggested, const QStringList &sourceKinds,
	  std::function<void(const Picked &)> onPicked)
{
	auto shell = su::makeWindow(T("FontPicker.Title"), QString(), parent, /*brandFooter=*/false, "StreamUP", {},
				    /*popup=*/true);
	su::ShadowDialog *dlg = shell.dialog;
	dlg->setAttribute(Qt::WA_DeleteOnClose, true);
	shell.content->setContentsMargins(su::S(16), su::S(14), su::S(16), su::S(8));
	shell.content->setSpacing(su::S(10));

	auto *bar = new QHBoxLayout();
	bar->setSpacing(su::S(10));
	auto *search = new su::SearchField();
	search->setPlaceholderText(T("FontPicker.Search"));
	search->setAccessibleName(T("FontPicker.Search"));
	search->setFixedHeight(su::S(30));
	bar->addWidget(search, 1);
	auto *source = new su::SegmentedControl({T("FontPicker.All"), T("FontPicker.System"), T("FontPicker.Google")});
	source->setFixedWidth(su::S(230));
	bar->addWidget(source);
	shell.content->addLayout(bar);

	auto *list = new QListWidget();
	list->setUniformItemSizes(false);
	list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	list->setStyleSheet(su::scale_qss(
		QString("QListWidget{background:%1;border:none;border-radius:12px;outline:none;padding:6px;color:%2;}"
			"QListWidget::item{padding:0 12px;border:none;border-radius:8px;}"
			"QListWidget::item:hover{background:%3;}"
			"QListWidget::item:selected{background:%4;color:#ffffff;}")
			.arg(su::Colors::BG_SECONDARY, su::Colors::TEXT_PRIMARY, su::Colors::HOVER_ROW, su::Colors::PRIMARY_COLOR)));
	su::useScrollBars(list);
	shell.content->addWidget(list, 1);

	QLabel *status = su::makeLabel(QString(), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::TEXT_MUTED);
	status->setWordWrap(true);
	shell.content->addWidget(status);

	// What a newly installed font needs before OBS's own text shows it:
	// FreeType 2 only reads fonts installed for all users, GDI+ only fonts
	// that were there when OBS started. ProText uses it straight away.
	const char *noteKey = sourceKinds.contains("text_ft2_source") ? "FontPicker.Ft2Note"
			      : sourceKinds.contains("text_gdiplus")  ? "FontPicker.GdiNote"
								      : nullptr;
	if (noteKey) {
		QLabel *note = su::makeLabel(T(noteKey), su::Sizes::FONT_SIZE_TINY, 500, su::Colors::COLOR_WARNING);
		note->setWordWrap(true);
		shell.content->addWidget(note);
	}

	auto *cancel = new su::PillButton(T("OverlayControls.Button.Cancel"), "outline");
	auto *use = new su::PillButton(T("FontPicker.Use"), "primary");
	shell.footerButtons->addWidget(cancel);
	shell.footerButtons->addWidget(use);
	QObject::connect(cancel, &QPushButton::clicked, dlg, &QDialog::close);

	auto st = std::make_shared<State>();
	st->list = list;

	// Installed fonts: suggestions first, then everything on the system.
	// Fonts a preview loaded into Qt are not installed for OBS: they stay
	// in the Google list until they really are.
	QStringList installed;
	for (const QString &f : QFontDatabase::families())
		if (!gf::IsPreviewOnly(f))
			installed.append(f);
	QSet<QString> installedSet;
	for (const QString &f : installed)
		installedSet.insert(f.toLower());
	if (!suggested.isEmpty()) {
		list->addItem(Header(T("FontPicker.Suggested")));
		for (const QString &f : suggested) {
			QListWidgetItem *item = Row(f, !installedSet.contains(f.toLower()));
			if (installedSet.contains(f.toLower()))
				ApplyFace(item, f);
			list->addItem(item);
		}
	}
	list->addItem(Header(T("FontPicker.SystemFonts")));
	for (const QString &f : installed) {
		if (f.startsWith('@') || QFontDatabase::isPrivateFamily(f))
			continue;
		QListWidgetItem *item = Row(f, false);
		ApplyFace(item, f);
		list->addItem(item);
	}
	QListWidgetItem *googleHeader = Header(T("FontPicker.GoogleFonts"));
	list->addItem(googleHeader);
	QListWidgetItem *loading = new QListWidgetItem(T("FontPicker.Loading"));
	loading->setFlags(Qt::NoItemFlags);
	loading->setForeground(QColor(su::Colors::TEXT_MUTED));
	list->addItem(loading);

	// Select the current family.
	auto selectFamily = [list](const QString &family) {
		for (int i = 0; i < list->count(); ++i) {
			QListWidgetItem *item = list->item(i);
			if (item->data(kRoleFamily).toString().compare(family, Qt::CaseInsensitive) == 0) {
				list->setCurrentItem(item);
				list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
				return;
			}
		}
	};
	selectFamily(current);

	// Previews for the Google rows on screen: download in the background,
	// a few at a time, and draw each in its face as it lands.
	auto *previewTimer = new QTimer(dlg);
	previewTimer->setSingleShot(true);
	previewTimer->setInterval(150);
	QObject::connect(previewTimer, &QTimer::timeout, dlg, [st]() {
		QListWidget *l = st->list;
		if (!l)
			return;
		const QRect view = l->viewport()->rect();
		QListWidgetItem *top = l->itemAt(view.topLeft() + QPoint(4, 4));
		int start = top ? l->row(top) : 0;
		for (int i = start; i < l->count(); ++i) {
			QListWidgetItem *item = l->item(i);
			if (l->visualItemRect(item).top() > view.bottom())
				break;
			if (item->isHidden() || !item->data(kRoleGoogle).toBool() || item->data(kRoleLoaded).toBool())
				continue;
			const QString family = item->data(kRoleFamily).toString();
			if (st->queued.contains(family))
				continue;
			st->queued.insert(family);
			gf::Pool()->start(new Job([st, family]() {
				const QString file = gf::Download(family, nullptr, 10);
				const QString face = gf::LoadForPreview(file);
				Post([st, family, face]() {
					if (!st->list || face.isEmpty())
						return;
					if (QListWidgetItem *row = st->googleRows.value(family))
						ApplyFace(row, face);
				});
			}));
		}
	});
	QObject::connect(list->verticalScrollBar(), &QScrollBar::valueChanged, previewTimer,
			 [previewTimer]() { previewTimer->start(); });

	// Filter by text and source.
	auto applyFilter = [list, search, source, previewTimer]() {
		const QString needle = search->text().trimmed();
		const int which = source->currentIndex(); // 0 all, 1 system, 2 google
		QListWidgetItem *header = nullptr;
		bool headerHasRows = false;
		auto closeHeader = [&]() {
			if (header)
				header->setHidden(!headerHasRows);
		};
		for (int i = 0; i < list->count(); ++i) {
			QListWidgetItem *item = list->item(i);
			if (item->data(kRoleHeader).toBool()) {
				closeHeader();
				header = item;
				headerHasRows = false;
				continue;
			}
			const QString family = item->data(kRoleFamily).toString();
			if (family.isEmpty()) { // the loading line
				item->setHidden(which == 1 || !needle.isEmpty());
				headerHasRows = headerHasRows || !item->isHidden();
				continue;
			}
			const bool google = item->data(kRoleGoogle).toBool();
			const bool show = (which == 0 || (which == 1) == !google) &&
					  (needle.isEmpty() || family.contains(needle, Qt::CaseInsensitive));
			item->setHidden(!show);
			headerHasRows = headerHasRows || show;
		}
		closeHeader();
		previewTimer->start();
	};
	QObject::connect(search, &QLineEdit::textChanged, dlg, [applyFilter]() { applyFilter(); });
	source->onChanged([applyFilter](int) { applyFilter(); });

	// The catalogue loads in the background (network the first time a week).
	gf::Pool()->start(new Job([st, installedSet, current, applyFilter, loading, list]() {
		std::vector<gf::Entry> catalog = gf::Catalog();
		Post([st, catalog, installedSet, current, applyFilter, loading]() {
			QListWidget *l = st->list;
			if (!l)
				return;
			const int at = l->row(loading);
			delete l->takeItem(at);
			int row = at;
			for (const gf::Entry &e : catalog) {
				if (installedSet.contains(e.family.toLower()))
					continue; // already listed under system fonts
				QListWidgetItem *item = Row(e.family, true);
				const QString cached = gf::CachedFile(e.family);
				if (!cached.isEmpty()) {
					const QString face = gf::LoadForPreview(cached);
					if (!face.isEmpty())
						ApplyFace(item, face);
				}
				l->insertItem(row++, item);
				st->googleRows.insert(e.family, item);
			}
			st->catalogReady = true;
			applyFilter();
			if (!l->currentItem()) {
				for (int i = 0; i < l->count(); ++i) {
					if (l->item(i)->data(kRoleFamily).toString().compare(current, Qt::CaseInsensitive) == 0) {
						l->setCurrentItem(l->item(i));
						l->scrollToItem(l->item(i), QAbstractItemView::PositionAtCenter);
						break;
					}
				}
			}
		});
		(void)list;
	}));

	// Use: a Google font is downloaded and installed first.
	auto busy = std::make_shared<bool>(false);
	// Only OBS's own text sources need the font installed in Windows. A
	// control that only drives ProText reads the shared font cache, so the
	// download is enough and nothing is installed.
	const bool installNeeded = sourceKinds.isEmpty() || sourceKinds.contains("text_gdiplus") ||
				   sourceKinds.contains("text_ft2_source");
	auto choose = [dlg, list, status, use, cancel, busy, onPicked, installNeeded]() {
		QListWidgetItem *item = list->currentItem();
		if (*busy || !item || item->data(kRoleFamily).toString().isEmpty())
			return;
		const QString family = item->data(kRoleFamily).toString();
		if (!item->data(kRoleGoogle).toBool() || gf::IsInstalled(family)) {
			Picked p;
			p.family = family;
			onPicked(p);
			dlg->close();
			return;
		}
		*busy = true;
		use->setEnabled(false);
		list->setEnabled(false);
		status->setText(T("FontPicker.Installing").arg(family));
		QPointer<su::ShadowDialog> guard(dlg);
		gf::Pool()->start(new Job([guard, family, status, use, list, busy, onPicked, installNeeded]() {
			QString error;
			const QString file = gf::Download(family, &error, 30);
			gf::InstallResult r;
			if (file.isEmpty()) {
				r.error = error;
			} else if (installNeeded) {
				r = gf::InstallForUser(family, file);
			} else {
				r.ok = true;
				r.family = gf::LoadForPreview(file);
				if (r.family.isEmpty())
					r.family = family;
			}
			Post([guard, r, status, use, list, busy, onPicked]() {
				if (r.ok) {
					Picked p;
					p.family = r.family;
					p.installed = !r.file.isEmpty();
					p.file = r.file;
					onPicked(p); // the panel stays, so this runs even if the picker closed
					if (guard)
						guard->close();
					return;
				}
				if (!guard)
					return;
				*busy = false;
				use->setEnabled(true);
				list->setEnabled(true);
				status->setText(r.error);
			});
		}));
		(void)cancel;
	};
	QObject::connect(use, &QPushButton::clicked, dlg, choose);
	QObject::connect(list, &QListWidget::itemDoubleClicked, dlg, [choose](QListWidgetItem *) { choose(); });
	// Screen readers select a row without making it current; follow them.
	QObject::connect(list, &QListWidget::itemSelectionChanged, dlg, [list]() {
		const QList<QListWidgetItem *> sel = list->selectedItems();
		if (!sel.isEmpty() && sel.first() != list->currentItem())
			list->setCurrentItem(sel.first());
	});
	QObject::connect(list, &QListWidget::currentItemChanged, dlg, [use, status](QListWidgetItem *now, QListWidgetItem *) {
		const bool google = now && now->data(kRoleGoogle).toBool() && !gf::IsInstalled(now->data(kRoleFamily).toString());
		use->setText(google ? T("FontPicker.InstallAndUse") : T("FontPicker.Use"));
		use->updateGeometry();
		status->setText(google ? T("FontPicker.WillInstall").arg(now->data(kRoleFamily).toString()) : QString());
	});

	const int m = su::S(su::ShadowDialog::kShadowMargin);
	dlg->resize(su::S(560) + 2 * m, su::S(620) + 2 * m);
	dlg->show();
	previewTimer->start();
}

} // namespace FontPicker
} // namespace StreamUP
