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
		VersionBlock block;
		block.versionLabel = headerLine.mid(2).trimmed();
		block.bodyHtml = MarkdownBodyToHtml(body);
		blocks.append(block);
	}

	return blocks;
}

// Build one collapsible card: clickable version header + hideable body label.
// `expanded` controls the initial state.
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

	// Header button
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
		"QPushButton:hover { background: %2; }"
		"QPushButton:checked { background: %2; }")
		.arg(UIStyles::Colors::TEXT_PRIMARY)
		.arg(UIStyles::Colors::BG_TERTIARY));
	cardLay->addWidget(header);

	// Body
	QLabel *body = new QLabel(block.bodyHtml);
	body->setTextFormat(Qt::RichText);
	body->setWordWrap(true);
	body->setOpenExternalLinks(true);
	body->setStyleSheet(QString("QLabel { background: transparent; color: %1; padding: 4px 14px 12px 14px; }")
		.arg(UIStyles::Colors::TEXT_SECONDARY));
	body->setVisible(expanded);
	cardLay->addWidget(body);

	QObject::connect(header, &QPushButton::toggled, body, [header, body, &block](bool checked) {
		Q_UNUSED(block)
		body->setVisible(checked);
		// Re-render arrow
		QString label = header->text();
		int idx = label.indexOf(' ', 2);
		if (idx > 0) {
			QString tail = label.mid(idx);
			header->setText(QString("  %1%2").arg(checked ? QChar(0x25BC) : QChar(0x25B6)).arg(tail));
		}
	});

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

		// Useful links section
		contentLayout->addWidget(UIStyles::CreateSectionHeader("Useful Links"));
		QHBoxLayout *buttonsLayout = new QHBoxLayout();
		buttonsLayout->setSpacing(UIStyles::Sizes::SPACING_SMALL);

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

		QPushButton *websiteBtn = UIStyles::CreateStyledButton("Website", "neutral");
		QObject::connect(websiteBtn, &QPushButton::clicked, []() {
			QDesktopServices::openUrl(QUrl("https://streamup.tips"));
		});

		buttonsLayout->addWidget(docsBtn);
		buttonsLayout->addWidget(discordBtn);
		buttonsLayout->addWidget(websiteBtn);
		buttonsLayout->addStretch();
		contentLayout->addLayout(buttonsLayout);

		contentLayout->addStretch();

		scrollArea->setWidget(contentWidget);
		scrollArea->setWidgetResizable(true);
		scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		mainLayout->addWidget(scrollArea);

		// Footer close button
		QVBoxLayout *footerLayout = UIStyles::GetDialogFooterLayout(dialog);
		QHBoxLayout *footerBtnLay = new QHBoxLayout();
		QPushButton *closeButton = UIStyles::CreateStyledButton("Close", "neutral");
		QObject::connect(closeButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
		footerBtnLay->addStretch();
		footerBtnLay->addWidget(closeButton);
		footerBtnLay->addStretch();
		footerLayout->addLayout(footerBtnLay);

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
