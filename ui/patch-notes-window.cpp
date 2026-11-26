#include "patch-notes-window.hpp"
#include "ui-helpers.hpp"
#include "streamup-kit.hpp"
#include <QGridLayout>
#include <streamup/ui/window-chrome.hpp> // ShadowDialog, makeWindow, WindowShell
#include <streamup/ui/pill-button.hpp>   // PillButton
#include <streamup/ui/labels.hpp>        // makeLabel
#include <streamup/ui/svg-icon.hpp>
#include <streamup/ui/ui-scrollbar.hpp>  // useScrollBars
#include "splash-screen.hpp"
#include "features-page.hpp"
#include "../utilities/error-handler.hpp"
#include "../version.h"
#include <obs-module.h>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QFrame>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>

namespace StreamUP {
namespace PatchNotesWindow {

namespace {

struct VersionBlock {
	QString versionLabel; // e.g. "StreamUP v2.1.8 - Patch Update"
	QString bodyHtml;     // rendered body (excludes the h1)
};

// Apply **bold** / *italic* inline formatting.
static QString ApplyInlineFormatting(QString line)
{
	// **bold**
	while (true) {
		int open = line.indexOf("**");
		if (open < 0) break;
		int close = line.indexOf("**", open + 2);
		if (close < 0) break;
		QString inner = line.mid(open + 2, close - open - 2);
		line.replace(open, close - open + 2, QString("<b>%1</b>").arg(inner));
	}
	return line;
}

// A colour flattened onto the card surface, as #rrggbb: rich text has no
// alpha, so the tinted section labels are mixed here.
static QString Tint(const QColor &c, double amount)
{
	const QColor bg(UIStyles::Colors::BG_PRIMARY);
	return QColor(int(c.red() * amount + bg.red() * (1 - amount)), int(c.green() * amount + bg.green() * (1 - amount)),
		      int(c.blue() * amount + bg.blue() * (1 - amount)))
		.name();
}

// A section heading as a small coloured label: New in the accent, Improved
// in green, Fixed in amber, so a version's notes can be scanned by kind.
static QString SectionLabel(const QString &heading)
{
	const QString h = heading.toLower();
	QString text = UIStyles::sentenceCase(heading);
	QColor colour(UIStyles::Colors::PRIMARY_COLOR);
	if (h.contains("fix")) {
		text = QStringLiteral("Fixed");
		colour = QColor(UIStyles::Colors::COLOR_WARNING);
	} else if (h.contains("improve")) {
		text = QStringLiteral("Improved");
		colour = QColor(UIStyles::Colors::COLOR_SUCCESS);
	} else if (h.contains("new")) {
		text = QStringLiteral("New");
	} else if (h.contains("headline")) {
		text = QStringLiteral("Highlights");
	}
	return QString("<table cellspacing=\"0\" cellpadding=\"4\" style=\"margin: 20px 0 6px 0;\"><tr>"
		       "<td style=\"background-color: %1; color: %2; font-size: 11px; font-weight: 600;\">&nbsp;%3&nbsp;</td>"
		       "</tr></table>\n")
		.arg(Tint(colour, 0.18), colour.name(), text.toHtmlEscaped());
}

// One entry, indented under its section: an accent dot, the title on its own
// line and the detail under it, with room between lines.
static QString EntryHtml(const QString &title, const QString &detail)
{
	QString text;
	if (!title.isEmpty())
		text += QString("<p style=\"margin: 0; color: %1; font-size: 13px; font-weight: 600; line-height: 140%;\">%2</p>")
				.arg(UIStyles::Colors::TEXT_PRIMARY, ApplyInlineFormatting(UIStyles::sentenceCase(title)));
	if (!detail.isEmpty())
		text += QString("<p style=\"margin: %1 0 0 0; color: %2; font-size: 12px; line-height: 150%;\">%3</p>")
				.arg(title.isEmpty() ? "0" : "2px", UIStyles::Colors::TEXT_SECONDARY, ApplyInlineFormatting(detail));
	return QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" style=\"margin: 0 0 12px 0;\"><tr>"
		       "<td width=\"14\"></td>"
		       "<td width=\"16\" valign=\"top\" style=\"color: %1; font-size: 13px; font-weight: 600;\">&#8226;</td>"
		       "<td valign=\"top\">%2</td></tr></table>\n")
		.arg(UIStyles::Colors::PRIMARY_COLOR, text);
}

// Convert a single version's markdown body (no h1) to inline-styled HTML.
static QString MarkdownBodyToHtml(const QString &md)
{
	QString out;
	const QStringList lines = md.split('\n');
	for (const QString &raw : lines) {
		const QString line = raw.trimmed();
		if (line.isEmpty())
			continue;
		if (line.startsWith("## ") || line.startsWith("### ")) {
			out += SectionLabel(line.mid(line.indexOf(' ') + 1));
		} else if (line.startsWith("- ")) {
			// An entry is written as `- **Title** Body`: the title goes on its
			// own line with the detail under it, so the list skims by title.
			const QString entry = line.mid(2).trimmed();
			QString title;
			QString detail = entry;
			if (entry.startsWith("**")) {
				const int close = entry.indexOf("**", 2);
				if (close > 2) {
					title = entry.mid(2, close - 2).trimmed();
					detail = entry.mid(close + 2).trimmed();
				}
			}
			out += EntryHtml(title, detail);
		} else {
			out += QString("<p style=\"margin: 6px 0; line-height: 150%;\">%1</p>\n").arg(ApplyInlineFormatting(line));
		}
	}
	return QString("<div style=\"color: %1; font-size: 12px;\">%2</div>").arg(UIStyles::Colors::TEXT_SECONDARY, out);
}

// Read the raw patch notes markdown and split into per-version blocks.
static QVector<VersionBlock> LoadVersionBlocks()
{
	QVector<VersionBlock> blocks;

	QFile file(":patch-notes-summary.md");
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		ErrorHandler::LogWarning("Failed to open local patch notes file", ErrorHandler::Category::UI);
		return blocks;
	}
	QTextStream in(&file);
	const QString md = in.readAll();
	file.close();
	if (md.isEmpty()) return blocks;

	// Split on the `---` separator. Each chunk should start with `# StreamUP vX.Y.Z ...`.
	const QStringList chunks = md.split(QRegularExpression("\\n-{3,}\\n"));
	for (const QString &chunk : chunks) {
		QString trimmed = chunk.trimmed();
		if (trimmed.isEmpty()) continue;

		int firstNewline = trimmed.indexOf('\n');
		QString headerLine = firstNewline < 0 ? trimmed : trimmed.left(firstNewline);
		QString body = firstNewline < 0 ? QString() : trimmed.mid(firstNewline + 1);

		if (!headerLine.startsWith("# ")) continue;
		QString label = headerLine.mid(2).trimmed();
		// Only keep real version blocks. Other H1s in the markdown
		// (e.g. "Support This Project") are rendered elsewhere or skipped.
		if (!label.startsWith("StreamUP v")) continue;
		VersionBlock block;
		block.versionLabel = label;
		block.bodyHtml = MarkdownBodyToHtml(body);
		blocks.append(block);
	}

	return blocks;
}

// Build one collapsible card: clickable version header + body label that
// toggles via setVisible. We deliberately do NOT animate maximumHeight here:
// animating it on a QScrollArea-hosted wordwrap QLabel re-runs the entire
// layout pass each frame (heightForWidth is expensive on rich text), which
// pegs the CPU and looks worse than an instant toggle.
static QWidget *BuildVersionCard(const VersionBlock &block, bool expanded)
{
	QFrame *card = new QFrame();
	card->setObjectName("patchNotesCard");
	card->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
		"QFrame#patchNotesCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
		.arg(UIStyles::Colors::BG_PRIMARY)
		.arg(UIStyles::Colors::BORDER_SUBTLE)
		.arg(12)));

	QVBoxLayout *cardLay = new QVBoxLayout(card);
	cardLay->setContentsMargins(0, 0, 0, 0);
	cardLay->setSpacing(0);

	// Header button — fully transparent so the rounded card outline isn't
	// fighting a square button background. Hover indication = chevron colour.
	QPushButton *header = new QPushButton();
	header->setCheckable(true);
	header->setChecked(expanded);
	header->setCursor(Qt::PointingHandCursor);
	header->setText(QString("  %1  %2").arg(expanded ? QChar(0x2212) : QChar(0x002B)).arg(block.versionLabel));
	header->setStyleSheet(StreamUP::UIStyles::scale_qss(QString(
		"QPushButton {"
		"  text-align: left;"
		"  background: transparent;"
		"  border: none;"
		"  color: %1;"
		"  font-size: 14px;"
		"  font-weight: 600;"
		"  padding: 12px 14px;"
		"}"
		"QPushButton:hover { color: %2; }")
		.arg(UIStyles::Colors::PRIMARY_COLOR)
		.arg(UIStyles::Colors::PRIMARY_LIGHT)));
	cardLay->addWidget(header);

	QLabel *body = new QLabel(block.bodyHtml);
	body->setTextFormat(Qt::RichText);
	body->setWordWrap(true);
	body->setOpenExternalLinks(true);
	body->setStyleSheet(StreamUP::UIStyles::scale_qss(QString("QLabel { background: transparent; color: %1; padding: 0px 18px 10px 18px; }")
		.arg(UIStyles::Colors::TEXT_SECONDARY)));
	body->setVisible(expanded);
	cardLay->addWidget(body);

	QObject::connect(header, &QPushButton::toggled, body, [header, body](bool checked) {
		body->setVisible(checked);
		// Swap chevron without rebuilding the rest of the label.
		QString label = header->text();
		int idx = label.indexOf(' ', 2);
		if (idx > 0) {
			QString tail = label.mid(idx);
			header->setText(QString("  %1%2").arg(checked ? QChar(0x2212) : QChar(0x002B)).arg(tail));
		}
	});

	return card;
}

} // namespace

// What's new: the four biggest changes as picture cards, two smaller ones as
// icon rows, then a button for every change. Pictures are in
// data/whatsnew/<id>.webp|.gif|.png at 2x.
struct Headline {
	const char *id;
	const char *nameKey;
	const char *lineKey;
	const char *icon; // for the small rows
};

static const Headline kPictures[] = {
	{"features", "WhatsNew.Features.Name", "WhatsNew.Features.Line", nullptr},
	{"my-products", "WhatsNew.MyProducts.Name", "WhatsNew.MyProducts.Line", nullptr},
	{"mixer", "WhatsNew.Mixer.Name", "WhatsNew.Mixer.Line", nullptr},
	{"font-picker", "WhatsNew.FontPicker.Name", "WhatsNew.FontPicker.Line", nullptr},
};
static const Headline kRows[] = {
	{"product-check", "WhatsNew.ProductCheck.Name", "WhatsNew.ProductCheck.Line", "package"},
	{"cleaner", "WhatsNew.Cleaner.Name", "WhatsNew.Cleaner.Line", "sparkles"},
	{"restart", "WhatsNew.Restart.Name", "WhatsNew.Restart.Line", "refresh"},
};

static QString Tx(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QWidget *BuildWhatsNewContent()
{
	namespace su = StreamUP::UIStyles;
	QWidget *contentWidget = new QWidget();
	contentWidget->setObjectName("wnPage");
	contentWidget->setStyleSheet(su::scale_qss(QString("QWidget#wnPage{background:transparent;}"
							   "QFrame#wnCard{background:%1;border:1px solid " + QString(StreamUP::UIStyles::Colors::BORDER_SUBTLE) + ";"
							   "border-radius:12px;}")
							   .arg(su::Colors::BG_PRIMARY)));
	QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
	contentLayout->setContentsMargins(su::S(24), su::S(20), su::S(24), su::S(20));
	contentLayout->setSpacing(su::S(14));

	QLabel *heading = su::makeLabel(QString(Tx("WhatsNew.Heading")).arg(PROJECT_VERSION_MAJOR), su::Type::Title,
					su::Type::Semibold, su::Colors::TEXT_PRIMARY);
	contentLayout->addWidget(heading);

	// Picture cards, two to a row. A click on a picture shows it big.
	auto *grid = new QGridLayout();
	grid->setHorizontalSpacing(su::S(12));
	grid->setVerticalSpacing(su::S(12));
	int n = 0;
	for (const Headline &h : kPictures) {
		auto *card = new QFrame();
		card->setObjectName("wnCard");
		auto *v = new QVBoxLayout(card);
		v->setContentsMargins(su::S(12), su::S(12), su::S(12), su::S(14));
		v->setSpacing(su::S(6));
		auto *pic = new StreamUP::Kit::PreviewSlot(QStringLiteral("whatsnew"), QString::fromUtf8(h.id), 140);
		pic->setNaturalAspect(); // the whole picture, nothing cropped
		pic->setZoom(Tx(h.nameKey), Tx(h.lineKey));
		pic->setPlaying(true);
		v->addWidget(pic);
		v->addSpacing(su::S(4));
		v->addWidget(su::makeLabel(Tx(h.nameKey), su::Type::Body, su::Type::Semibold, su::Colors::TEXT_PRIMARY));
		QLabel *line = su::makeLabel(Tx(h.lineKey), su::Type::Small, su::Type::Regular, su::Colors::TEXT_SECONDARY);
		line->setWordWrap(true);
		v->addWidget(line);
		v->addStretch(1);
		grid->addWidget(card, n / 2, n % 2);
		++n;
	}
	contentLayout->addLayout(grid);

	// Smaller changes: an icon and one line each, in one card.
	auto *list = new StreamUP::Kit::RowList();
	for (const Headline &h : kRows) {
		auto *icon = new QLabel();
		icon->setStyleSheet("background:transparent;border:none;");
		icon->setPixmap(su::svgIcon(QString::fromLatin1(h.icon), QColor(su::Colors::TEXT_SECONDARY), 20,
					    qApp->devicePixelRatio()));
		list->addRow(Tx(h.nameKey), Tx(h.lineKey), {}, icon);
	}
	contentLayout->addWidget(list);

	// Every change, version by version, behind a button.
	auto *allBox = new QWidget();
	auto *allLay = new QVBoxLayout(allBox);
	allLay->setContentsMargins(0, 0, 0, 0);
	allLay->setSpacing(su::S(8));
	QVector<VersionBlock> blocks = LoadVersionBlocks();
	if (blocks.isEmpty()) {
		allLay->addWidget(su::makeLabel(Tx("WhatsNew.LoadFailed"), su::Type::Body, su::Type::Regular, su::Colors::COLOR_WARNING));
	} else {
		for (int i = 0; i < blocks.size(); ++i)
			allLay->addWidget(BuildVersionCard(blocks[i], i == 0));
	}
	allBox->setVisible(false);
	auto *allRow = new QHBoxLayout();
	auto *allButton = new su::PillButton(Tx("WhatsNew.AllChanges"), "outline");
	allButton->setAccessibleName(Tx("WhatsNew.AllChanges"));
	QObject::connect(allButton, &QPushButton::clicked, allBox, [allBox, allButton]() {
		const bool show = !allBox->isVisible();
		allBox->setVisible(show);
		allButton->setText(Tx(show ? "WhatsNew.HideChanges" : "WhatsNew.AllChanges"));
		allButton->updateGeometry();
	});
	allRow->addWidget(allButton);
	allRow->addStretch(1);
	contentLayout->addLayout(allRow);
	contentLayout->addWidget(allBox);

	// Membership once, at the foot.
	if (QWidget *member = StreamUP::Kit::MembershipCard(StreamUP::Kit::MemberStyle::Strip))
		contentLayout->addWidget(member);
	contentLayout->addStretch();
	return contentWidget;
}

void CreatePatchNotesDialog()
{
	FeaturesPage::ShowWhatsNew();
}

void ShowPatchNotesWindow()
{
	FeaturesPage::ShowWhatsNew();
	ErrorHandler::LogInfo("Patch notes window shown", ErrorHandler::Category::UI);
}

bool IsPatchNotesWindowOpen()
{
	return FeaturesPage::IsOpen();
}

} // namespace PatchNotesWindow
} // namespace StreamUP
