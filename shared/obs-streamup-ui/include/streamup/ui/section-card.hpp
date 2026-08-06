#pragma once

// Grouped content blocks: a titled card, a label/value fact table, and a
// read-only scrollable list.
//
// Any window that presents more than a couple of facts turns into a wall of
// paragraphs without these. Grouping the same information into cards gives the
// eye somewhere to rest and makes each block skimmable. Shared here rather than
// living in one plugin so every StreamUP window groups things the same way.
//
// Usage:
// Cards stack in a column with cardColumnSpacing() between them, or sit in two
// columns via cardColumns() when one section is a long list and the rest are
// short: a tall list next to short blocks wastes half the window otherwise.
//
//   auto *body = sectionCard(layout, "What gets saved");
//   cardText(body, "Scene collections, profiles, themes.");
//   auto *grid = cardFacts(body);
//   cardFact(grid, 0, "Files", "1101");
//   body->addWidget(cardList(names));

#include "gallery-style.hpp"
#include "labels.hpp"
#include "ui-scrollbar.hpp"
#include "window-chrome.hpp" // RoundedContainer

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>
#include <QHeaderView>
#include <QSizePolicy>
#include <QTableWidget>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPair>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace StreamUP {
namespace UIStyles {

// A titled card. Returns the layout to fill; the card itself is a QFrame#card
// so it picks up the shared cardStyle() fill and radius.
inline QVBoxLayout *sectionCard(QVBoxLayout *parent, const QString &title = QString())
{
	auto *frame = new QFrame();
	frame->setObjectName("card");
	frame->setStyleSheet(cardStyle());
	// Hug the content vertically. Without this the parent layout hands any
	// spare window height to the cards, which then stretch and spread their
	// own rows apart: the card ends up sized by the window rather than by what
	// is in it, and the internal spacing looks arbitrary. Slack belongs in a
	// trailing stretch, not inside a card.
	frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	auto *layout = new QVBoxLayout(frame);
	layout->setContentsMargins(S(16), S(12), S(16), S(12));
	layout->setSpacing(S(4));

	if (!title.isEmpty()) {
		// sectionHeader() is the house section title (STANDARD.md section 6):
		// accent colour, display font, uppercase. A card must not invent its
		// own quieter caption, or windows built from cards stop matching every
		// other StreamUP window.
		QLabel *heading = sectionHeader(title);
		// Its default margins assume a bare section on a page; inside a card
		// the card's own padding already provides that space.
		heading->setContentsMargins(0, 0, 0, 0);
		heading->setFixedHeight(heading->fontMetrics().height());
		layout->addWidget(heading);
	}

	parent->addWidget(frame);
	return layout;
}

// Typography roles for card content, expressed in the house tokens rather than
// in numbers of their own. Two sizes only: hierarchy below the title comes from
// colour (TEXT_PRIMARY against TEXT_SECONDARY), not from a third font size.
namespace CardText {
constexpr int kTitle = Sizes::FONT_SIZE_NORMAL;  // a title within a card, e.g. a toggle's name
constexpr int kBody = Sizes::FONT_SIZE_SMALL;    // body copy and fact values
constexpr int kCaption = Sizes::FONT_SIZE_SMALL; // secondary help; quieter by colour
} // namespace CardText

// Body text inside a card.
inline QLabel *cardText(QVBoxLayout *layout, const QString &content,
			const char *colour = Colors::TEXT_SECONDARY, int size = CardText::kBody,
			bool bold = false)
{
	auto *label = new QLabel(content);
	label->setWordWrap(true);
	// No line-height here: Qt's stylesheet subset ignores it on QLabel, so
	// spacing has to come from the layout instead of pretending CSS applies.
	label->setStyleSheet(
		scale_qss(QString("QLabel{color:%1;font-size:%2px;font-weight:%3;background:transparent;}")
				  .arg(QString(colour))
				  .arg(size)
				  .arg(bold ? 700 : 400)));
	label->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(label);
	return label;
}

// A two-column grid for label/value pairs.
inline QGridLayout *cardFacts(QVBoxLayout *parent)
{
	auto *grid = new QGridLayout();
	grid->setContentsMargins(0, S(1), 0, 0);
	grid->setHorizontalSpacing(S(18));
	grid->setVerticalSpacing(S(2));
	grid->setColumnStretch(1, 1);
	parent->addLayout(grid);
	return grid;
}

// One label/value row. Values carry the emphasis, labels stay quiet.
inline void cardFact(QGridLayout *grid, int row, const QString &label, const QString &value,
		     const char *valueColour = Colors::TEXT_PRIMARY)
{
	auto *key = new QLabel(label);
	key->setStyleSheet(scale_qss(QString("QLabel{color:%1;font-size:%2px;background:transparent;}")
					     .arg(QString(Colors::TEXT_SECONDARY))
					     .arg(CardText::kBody)));
	auto *val = new QLabel(value);
	val->setWordWrap(true);
	val->setStyleSheet(
		scale_qss(QString("QLabel{color:%1;font-size:%2px;font-weight:600;background:transparent;}")
				  .arg(QString(valueColour))
				  .arg(CardText::kBody)));
	grid->addWidget(key, row, 0, Qt::AlignTop);
	grid->addWidget(val, row, 1, Qt::AlignTop);
}

// The gap between stacked cards. One value, so every window paces the same.
inline int cardColumnSpacing()
{
	return S(8);
}

// Split the content into two columns of cards. Returns {left, right}; add
// cards to either. The right column takes the extra width, since that is where
// a long list belongs.
inline QPair<QVBoxLayout *, QVBoxLayout *> cardColumns(QVBoxLayout *parent, int leftStretch = 1,
						       int rightStretch = 1)
{
	auto *row = new QHBoxLayout();
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(cardColumnSpacing());

	auto *left = new QVBoxLayout();
	left->setContentsMargins(0, 0, 0, 0);
	left->setSpacing(cardColumnSpacing());

	auto *right = new QVBoxLayout();
	right->setContentsMargins(0, 0, 0, 0);
	right->setSpacing(cardColumnSpacing());

	row->addLayout(left, leftStretch);
	row->addLayout(right, rightStretch);
	parent->addLayout(row);
	return {left, right};
}

// A read-only scrollable list, wrapped so its corners actually clip.
//
// Returns the container to add to a layout, not the list: per section 6, a bare
// QSS border-radius leaves square bottom corners as soon as a row reaches the
// edge, so the list goes inside a RoundedContainer. Sits on the darker base so
// it reads as inset against the card around it.
inline QWidget *cardList(const QStringList &items, int maxHeight = 150)
{
	auto *widget = new QListWidget();
	widget->addItems(items);
	widget->setStyleSheet(scale_qss(
		QString("QListWidget{background:transparent;border:none;color:%1;font-size:%2px;padding:4px;}"
			"QListWidget::item{padding:4px 6px;}")
			.arg(QString(Colors::TEXT_PRIMARY))
			.arg(CardText::kBody)));
	widget->setSelectionMode(QAbstractItemView::NoSelection);
	widget->setFocusPolicy(Qt::NoFocus);
	widget->setWordWrap(true);
	widget->setFrameShape(QFrame::NoFrame);
	useScrollBars(widget);

	auto *container = new RoundedContainer(8, nullptr, QColor(Colors::BG_SECONDARY));
	auto *layout = new QVBoxLayout(container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(widget);
	// A list is the one thing in a card that should take spare height: it is
	// scrollable, so extra room shows more rows rather than more empty space.
	container->setMaximumHeight(S(maxHeight));
	container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	return container;
}

// A row for cardFileTable: a file, plus an optional tag naming where it is
// used. Full path is kept for the tooltip.
struct FileRow {
	QString path;
	QString tag;
};

// A table of file paths, for when a list of paths is the content.
//
// Paths in a plain list are unreadable: they wrap over two or three lines, the
// name you are looking for sits at the end of a long folder chain, and the
// widget grows a horizontal scrollbar. This splits each path into name and
// folder columns, elides the folder from the left so the meaningful end stays
// visible, and puts the full path in the tooltip.
inline QWidget *cardFileTable(const QList<FileRow> &rows, const QString &tagHeader = QString(),
			      int maxHeight = 220)
{
	const bool hasTag = !tagHeader.isEmpty();
	auto *table = new QTableWidget(rows.size(), hasTag ? 3 : 2);
	QStringList headers{QStringLiteral("File"), QStringLiteral("Folder")};
	if (hasTag)
		headers << tagHeader;
	table->setHorizontalHeaderLabels(headers);

	table->verticalHeader()->setVisible(false);
	table->setShowGrid(false);
	table->setSelectionMode(QAbstractItemView::NoSelection);
	table->setFocusPolicy(Qt::NoFocus);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setWordWrap(false);
	// Middle elision suits both columns: a file name keeps its start and
	// extension, a folder keeps its drive and its last directory.
	table->setTextElideMode(Qt::ElideMiddle);
	// No horizontal scrolling. A table you have to scroll sideways to read is
	// the thing this replaced; columns share the width instead.
	table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	table->horizontalHeader()->setStretchLastSection(false);
	// A floor per column, so text is elided by choice rather than crushed to
	// "blurr....jpeg" when the window is narrow.
	table->horizontalHeader()->setMinimumSectionSize(S(130));
	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	if (hasTag)
		table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	table->verticalHeader()->setDefaultSectionSize(S(22));
	table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

	for (int i = 0; i < rows.size(); ++i) {
		const QFileInfo info(rows[i].path);

		auto *name = new QTableWidgetItem(info.fileName());
		name->setToolTip(rows[i].path);
		table->setItem(i, 0, name);

		auto *folder = new QTableWidgetItem(info.absolutePath());
		folder->setToolTip(rows[i].path);
		folder->setForeground(QColor(Colors::TEXT_SECONDARY));
		table->setItem(i, 1, folder);

		if (hasTag) {
			auto *tag = new QTableWidgetItem(rows[i].tag);
			tag->setForeground(QColor(Colors::TEXT_SECONDARY));
			table->setItem(i, 2, tag);
		}
	}

	// Right-click to copy. A table of paths is only useful if you can get the
	// path out of it: without this the user is retyping what is on screen.
	table->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(table, &QTableWidget::customContextMenuRequested, table,
			 [table, rows, hasTag, tagHeader](const QPoint &pos) {
				 const int row = table->rowAt(pos.y());
				 if (row < 0 || row >= rows.size())
					 return;

				 const FileRow &entry = rows[row];
				 const QFileInfo info(entry.path);

				 auto *menu = new QMenu(table);
				 menu->setAttribute(Qt::WA_DeleteOnClose);
				 auto copy = [](const QString &text) { QApplication::clipboard()->setText(text); };

				 QObject::connect(menu->addAction(QObject::tr("Copy file name")), &QAction::triggered,
						  table, [copy, info]() { copy(info.fileName()); });
				 QObject::connect(menu->addAction(QObject::tr("Copy full path")), &QAction::triggered,
						  table, [copy, entry]() { copy(entry.path); });
				 QObject::connect(menu->addAction(QObject::tr("Copy folder path")), &QAction::triggered,
						  table, [copy, info]() { copy(info.absolutePath()); });
				 if (hasTag && !entry.tag.isEmpty()) {
					 QObject::connect(menu->addAction(QObject::tr("Copy %1").arg(tagHeader.toLower())),
							  &QAction::triggered, table,
							  [copy, entry]() { copy(entry.tag); });
				 }

				 menu->addSeparator();
				 QObject::connect(menu->addAction(QObject::tr("Copy all rows")), &QAction::triggered,
						  table, [copy, rows, hasTag]() {
							  QStringList lines;
							  for (const FileRow &r : rows)
								  lines << (hasTag && !r.tag.isEmpty()
										    ? QStringLiteral("%1\t%2").arg(r.path, r.tag)
										    : r.path);
							  copy(lines.join(QLatin1Char('\n')));
						  });

				 // Only offer to reveal a file that is actually there: these
				 // tables often list files that have gone missing.
				 if (info.exists()) {
					 menu->addSeparator();
					 QObject::connect(menu->addAction(QObject::tr("Open containing folder")),
							  &QAction::triggered, table, [info]() {
								  QDesktopServices::openUrl(
									  QUrl::fromLocalFile(info.absolutePath()));
							  });
				 }

				 menu->popup(table->viewport()->mapToGlobal(pos));
			 });

	auto *card = makeTableCard(table, 8);
	card->setMaximumHeight(S(maxHeight));
	card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	return card;
}

// What an exported list should be written as.
enum class FileExportFormat {
	Text, // readable report: title, date, count, one path per entry
	Csv,  // spreadsheet: one row per file, columns for name, folder, path
};

// Write a list of files out to a document the user can keep, so a list of
// missing or skipped files can be worked through outside OBS. Returns the path
// written, or empty if cancelled or failed.
//
// The caller picks the format rather than it being inferred from whatever
// extension the user happens to type: "export to CSV" should produce a CSV.
inline QString exportFileRows(QWidget *parent, const QList<FileRow> &rows, const QString &title,
			      const QString &tagHeader, const QString &suggestedName,
			      FileExportFormat format = FileExportFormat::Text)
{
	const bool csv = (format == FileExportFormat::Csv);
	const QString extension = csv ? QStringLiteral(".csv") : QStringLiteral(".txt");

	QString defaultName = suggestedName;
	if (!defaultName.endsWith(extension, Qt::CaseInsensitive)) {
		const int dot = defaultName.lastIndexOf(QLatin1Char('.'));
		defaultName = (dot > 0 ? defaultName.left(dot) : defaultName) + extension;
	}

	QString chosen = QFileDialog::getSaveFileName(
		parent, csv ? QObject::tr("Export list as CSV") : QObject::tr("Export list"),
		QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(defaultName),
		csv ? QObject::tr("CSV spreadsheet (*.csv)") : QObject::tr("Text file (*.txt)"));
	if (chosen.isEmpty())
		return {};

	// Honour the chosen format even if the extension was edited away, so the
	// file's contents always match what was asked for.
	if (!chosen.endsWith(extension, Qt::CaseInsensitive))
		chosen += extension;

	QFile out(chosen);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		return {};

	QTextStream stream(&out);
	const bool hasTag = !tagHeader.isEmpty();

	if (csv) {
		auto quote = [](const QString &value) {
			QString escaped = value;
			escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
			return QStringLiteral("\"%1\"").arg(escaped);
		};
		stream << quote(QObject::tr("File name")) << ',' << quote(QObject::tr("Folder")) << ','
		       << quote(QObject::tr("Full path"));
		if (hasTag)
			stream << ',' << quote(tagHeader);
		stream << '\n';
		for (const FileRow &row : rows) {
			const QFileInfo info(row.path);
			stream << quote(info.fileName()) << ',' << quote(info.absolutePath()) << ','
			       << quote(row.path);
			if (hasTag)
				stream << ',' << quote(row.tag);
			stream << '\n';
		}
	} else {
		stream << title << '\n';
		stream << QObject::tr("Generated %1")
				  .arg(QDateTime::currentDateTime().toString(QStringLiteral("d MMMM yyyy 'at' HH:mm")))
		       << '\n';
		stream << QObject::tr("%1 files").arg(rows.size()) << '\n';
		stream << QString(60, QLatin1Char('-')) << '\n';
		for (const FileRow &row : rows) {
			stream << row.path << '\n';
			if (hasTag && !row.tag.isEmpty())
				stream << QStringLiteral("    %1: %2").arg(tagHeader, row.tag) << '\n';
		}
	}

	out.close();
	return chosen;
}

// Shrink a window to the height its content actually occupies.
//
// Call once, queued, after show(). Every hint-based approach fails here:
// sizeHint(), totalSizeHint() and adjustSize() all agreed on a height ~87px
// taller than anything drawn, because a card's hint is computed from unwrapped
// label text and grid hints, not from the geometry the layout finally gives it.
// The only number that matches the pixels is where the last visible child
// actually ends, so that is what this measures.
//
// Only ever shrinks: a window that needs more room keeps it.
inline void fitWindowToContent(QWidget *window, QVBoxLayout *content)
{
	QWidget *host = content->parentWidget();
	if (!window || !host)
		return;

	int bottom = 0;
	for (int i = 0; i < content->count(); ++i) {
		QLayoutItem *item = content->itemAt(i);
		if (QWidget *child = item->widget()) {
			if (child->isVisible())
				bottom = qMax(bottom, child->geometry().bottom() + 1);
		} else if (QLayout *nested = item->layout()) {
			bottom = qMax(bottom, nested->geometry().bottom() + 1);
		}
	}
	if (bottom <= 0)
		return;

	// Whatever sits below the content area (footer, shadow margin) keeps its
	// height; we only reclaim the slack inside the content itself.
	const int contentBottomInWindow = host->mapTo(window, QPoint(0, host->height())).y();
	const int belowContent = window->height() - contentBottomInWindow;
	const int wanted = host->mapTo(window, QPoint(0, bottom)).y() + content->contentsMargins().bottom() +
			   belowContent;

	if (wanted > 0 && wanted < window->height())
		window->resize(window->width(), wanted);
}

// Bytes as something a person can read. Here because every window that reports
// file sizes was otherwise rolling its own.
inline QString formatBytes(qint64 bytes)
{
	if (bytes >= 1024LL * 1024 * 1024)
		return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 1);
	if (bytes >= 1024 * 1024)
		return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 1);
	if (bytes >= 1024)
		return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 0);
	return QStringLiteral("%1 bytes").arg(bytes);
}

} // namespace UIStyles
} // namespace StreamUP
