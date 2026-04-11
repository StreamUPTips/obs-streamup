#include "patch-notes-window.hpp"
#include "ui-helpers.hpp"
#include "ui-styles.hpp"
#include "splash-screen.hpp"
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
#include <QPropertyAnimation>
#include <QEasingCurve>

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

// Convert a single version's markdown body (no h1) to inline-styled HTML.
static QString MarkdownBodyToHtml(const QString &md)
{
	QString out;
	bool inList = false;
	const QStringList lines = md.split('\n');
	for (const QString &raw : lines) {
		QString line = raw;
		if (line.trimmed().isEmpty()) continue;

		auto closeList = [&]() {
			if (inList) {
				out += "</ul>\n";
				inList = false;
			}
		};

		if (line.startsWith("### ")) {
			closeList();
			out += QString("<h4 style=\"color: %1; margin: 10px 0 4px 0; font-size: 13px; font-weight: 600;\">%2</h4>\n")
				.arg(UIStyles::Colors::TEXT_PRIMARY, line.mid(4));
		} else if (line.startsWith("## ")) {
			closeList();
			out += QString("<h3 style=\"color: %1; margin: 12px 0 6px 0; font-size: 14px; font-weight: 700;\">%2</h3>\n")
				.arg(UIStyles::Colors::PRIMARY_LIGHT, line.mid(3));
		} else if (line.startsWith("- ")) {
			if (!inList) {
				out += "<ul style=\"margin: 4px 0 8px 0; padding-left: 18px;\">\n";
				inList = true;
			}
			out += QString("<li style=\"margin: 2px 0;\">%1</li>\n")
				.arg(ApplyInlineFormatting(line.mid(2)));
		} else {
			closeList();
			out += QString("<p style=\"margin: 6px 0;\">%1</p>\n").arg(ApplyInlineFormatting(line));
		}
	}
	if (inList) out += "</ul>\n";

	return QString("<div style=\"color: %1; line-height: 1.45; font-size: 12px;\">%2</div>")
		.arg(UIStyles::Colors::TEXT_SECONDARY, out);
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

// Build one collapsible card: clickable version header + animated body container.
// The body lives inside a wrapper QWidget whose maximumHeight is animated between
// 0 and the body's heightForWidth so the curve follows the actual rendered text
// height (a wordwrap'd rich-text label needs heightForWidth, sizeHint lies).
static QWidget *BuildVersionCard(const VersionBlock &block, bool expanded)
{
	QFrame *card = new QFrame();
	card->setObjectName("patchNotesCard");
	card->setStyleSheet(QString(
		"QFrame#patchNotesCard { background: %1; border: 1px solid %2; border-radius: %3px; }")
		.arg(UIStyles::Colors::BG_PRIMARY)
		.arg(UIStyles::Colors::BORDER_SUBTLE)
		.arg(UIStyles::Sizes::RADIUS_MD));

	QVBoxLayout *cardLay = new QVBoxLayout(card);
	cardLay->setContentsMargins(0, 0, 0, 0);
	cardLay->setSpacing(0);

	// Header button — fully transparent (no hover/checked bg) so the rounded
	// card outline isn't fighting a square button background. Indication of
	// state is the chevron rotation only.
	QPushButton *header = new QPushButton();
	header->setCheckable(true);
	header->setChecked(expanded);
	header->setCursor(Qt::PointingHandCursor);
	header->setText(QString("  %1  %2").arg(expanded ? QChar(0x25BC) : QChar(0x25B6)).arg(block.versionLabel));
	header->setStyleSheet(QString(
		"QPushButton {"
		"  text-align: left;"
		"  background: transparent;"
		"  border: none;"
		"  color: %1;"
		"  font-size: 14px;"
		"  font-weight: 700;"
		"  padding: 12px 14px;"
		"}"
		"QPushButton:hover { color: %2; }")
		.arg(UIStyles::Colors::TEXT_PRIMARY)
		.arg(UIStyles::Colors::PRIMARY_LIGHT));
	cardLay->addWidget(header);

	// Body wrapper. Animate maximumHeight on toggle.
	QWidget *bodyWrapper = new QWidget();
	bodyWrapper->setStyleSheet("background: transparent;");
	QVBoxLayout *bodyWrapperLay = new QVBoxLayout(bodyWrapper);
	bodyWrapperLay->setContentsMargins(0, 0, 0, 0);
	bodyWrapperLay->setSpacing(0);

	QLabel *body = new QLabel(block.bodyHtml);
	body->setTextFormat(Qt::RichText);
	body->setWordWrap(true);
	body->setOpenExternalLinks(true);
	body->setStyleSheet(QString("QLabel { background: transparent; color: %1; padding: 4px 14px 12px 14px; }")
		.arg(UIStyles::Colors::TEXT_SECONDARY));
	bodyWrapperLay->addWidget(body);

	bodyWrapper->setMaximumHeight(expanded ? QWIDGETSIZE_MAX : 0);

	QObject::connect(header, &QPushButton::toggled, bodyWrapper, [header, body, bodyWrapper](bool checked) {
		// heightForWidth gives the actual rendered height for a wordwrap label.
		// Fall back to sizeHint if width isn't laid out yet.
		int width = body->width() > 0 ? body->width() : bodyWrapper->width();
		int targetHeight = width > 0 ? body->heightForWidth(width) : body->sizeHint().height();
		if (targetHeight <= 0) targetHeight = body->sizeHint().height();

		auto *anim = new QPropertyAnimation(bodyWrapper, "maximumHeight");
		anim->setDuration(220);
		anim->setEasingCurve(QEasingCurve::OutCubic);
		anim->setStartValue(checked ? 0 : bodyWrapper->height());
		anim->setEndValue(checked ? targetHeight : 0);
		QObject::connect(anim, &QPropertyAnimation::finished, bodyWrapper, [bodyWrapper, checked]() {
			if (checked) bodyWrapper->setMaximumHeight(QWIDGETSIZE_MAX);
		});
		anim->start(QAbstractAnimation::DeleteWhenStopped);

		// Update the chevron text immediately.
		QString label = header->text();
		int idx = label.indexOf(' ', 2);
		if (idx > 0) {
			QString tail = label.mid(idx);
			header->setText(QString("  %1%2").arg(checked ? QChar(0x25BC) : QChar(0x25B6)).arg(tail));
		}
	});

	cardLay->addWidget(bodyWrapper);
	return card;
}

} // namespace

void CreatePatchNotesDialog()
{
	UIHelpers::ShowSingletonDialogOnUIThread("patch-notes", []() -> QDialog * {
		QDialog *dialog = UIStyles::CreateStyledDialog("StreamUP \xe2\x80\xa2 Patch Notes");
		dialog->resize(720, 720);

		QVBoxLayout *mainLayout = UIStyles::GetDialogContentLayout(dialog);

		// Scrollable content
		QScrollArea *scrollArea = UIStyles::CreateStyledScrollArea();
		QWidget *contentWidget = new QWidget();
		contentWidget->setStyleSheet(QString("background: %1;").arg(UIStyles::Colors::BG_DARKEST));
		QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
		contentLayout->setContentsMargins(UIStyles::Sizes::PADDING_LARGE, UIStyles::Sizes::PADDING_LARGE,
						  UIStyles::Sizes::PADDING_LARGE, UIStyles::Sizes::PADDING_LARGE);
		contentLayout->setSpacing(UIStyles::Sizes::SPACING_MEDIUM);

		// Subtitle
		QString subtitle = QString("Latest updates and improvements in v%1").arg(PROJECT_VERSION);
		contentLayout->addWidget(UIStyles::CreateStyledDescription(subtitle));

		// Per-version collapsible cards — latest expanded, rest collapsed.
		QVector<VersionBlock> blocks = LoadVersionBlocks();
		if (blocks.isEmpty()) {
			QLabel *fallback = new QLabel("Unable to load patch notes.");
			fallback->setStyleSheet(QString("QLabel { color: %1; }").arg(UIStyles::Colors::WARNING));
			contentLayout->addWidget(fallback);
		} else {
			for (int i = 0; i < blocks.size(); ++i) {
				contentLayout->addWidget(BuildVersionCard(blocks[i], i == 0));
			}
		}

		contentLayout->addStretch();

		scrollArea->setWidget(contentWidget);
		scrollArea->setWidgetResizable(true);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		mainLayout->addWidget(scrollArea);

		// Footer: support row + links row + close, all pinned to the bottom.
		QVBoxLayout *footerLayout = UIStyles::GetDialogFooterLayout(dialog);

		// Support This Project row
		QHBoxLayout *supportLay = new QHBoxLayout();
		supportLay->setSpacing(UIStyles::Sizes::SPACING_SMALL);
		supportLay->addStretch();

		QPushButton *patreonBtn = UIStyles::CreateStyledButton("Patreon", "warning");
		patreonBtn->setIcon(QIcon(":images/icons/social/patreon.svg"));
		patreonBtn->setIconSize(QSize(16, 16));
		QObject::connect(patreonBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://www.patreon.com/streamup"));
		});

		QPushButton *kofiBtn = UIStyles::CreateStyledButton("Ko-Fi", "warning");
		kofiBtn->setIcon(QIcon(":images/icons/social/kofi.svg"));
		kofiBtn->setIconSize(QSize(16, 16));
		QObject::connect(kofiBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://ko-fi.com/streamup"));
		});

		QPushButton *beerBtn = UIStyles::CreateStyledButton("Buy a Beer", "warning");
		beerBtn->setIcon(QIcon(":images/icons/social/beer.svg"));
		beerBtn->setIconSize(QSize(16, 16));
		QObject::connect(beerBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://paypal.me/andilippi"));
		});

		supportLay->addWidget(patreonBtn);
		supportLay->addWidget(kofiBtn);
		supportLay->addWidget(beerBtn);
		supportLay->addStretch();
		footerLayout->addLayout(supportLay);

		// Links + close row
		QHBoxLayout *linksLay = new QHBoxLayout();
		linksLay->setSpacing(UIStyles::Sizes::SPACING_SMALL);
		linksLay->addStretch();

		QPushButton *docsBtn = UIStyles::CreateStyledButton("Documentation", "success");
		QObject::connect(docsBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://streamup.doras.click/docs"));
		});

		QPushButton *discordBtn = UIStyles::CreateStyledButton("Discord", "info");
		discordBtn->setIcon(QIcon(":images/icons/social/discord.svg"));
		discordBtn->setIconSize(QSize(16, 16));
		QObject::connect(discordBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://discord.com/invite/RnDKRaVCEu"));
		});

		QPushButton *websiteBtn = UIStyles::CreateStyledButton("Website", "primary-outline");
		QObject::connect(websiteBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://streamup.tips"));
		});

		QPushButton *closeButton = UIStyles::CreateStyledButton("Close", "neutral");
		QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });

		linksLay->addWidget(docsBtn);
		linksLay->addWidget(discordBtn);
		linksLay->addWidget(websiteBtn);
		linksLay->addSpacing(UIStyles::Sizes::SPACING_LARGE);
		linksLay->addWidget(closeButton);
		linksLay->addStretch();
		footerLayout->addLayout(linksLay);

		UIStyles::ApplyAutoSizing(dialog, 720, 900, 720, 820);
		UIHelpers::CenterDialog(dialog);
		return dialog;
	});
}

void ShowPatchNotesWindow()
{
	CreatePatchNotesDialog();
	ErrorHandler::LogInfo("Patch notes window shown", ErrorHandler::Category::UI);
}

bool IsPatchNotesWindowOpen()
{
	return UIHelpers::DialogManager::IsSingletonDialogOpen("patch-notes");
}

} // namespace PatchNotesWindow
} // namespace StreamUP
