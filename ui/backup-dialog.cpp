#include "backup-dialog.hpp"

#include "../core/backup-manager.hpp"
#include <streamup/debug-logger.hpp>
#include "backup-ui-common.hpp"
#include "version.h"

#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/switch-button.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

using namespace StreamUP::UIStyles;
using namespace StreamUP::BackupUI;
namespace su = StreamUP::UIStyles;

namespace StreamUP {
namespace Backup {

namespace {

/** One labelled toggle row, title and help text on the left, switch on the right. */
su::SwitchButton *toggleRow(QVBoxLayout *layout, const QString &title, const QString &help, bool initial)
{
	auto *row = new QWidget();
	auto *rowLayout = new QHBoxLayout(row);
	rowLayout->setContentsMargins(0, S(4), 0, S(4));
	rowLayout->setSpacing(S(18));

	auto *textColumn = new QWidget();
	auto *textLayout = new QVBoxLayout(textColumn);
	textLayout->setContentsMargins(0, 0, 0, 0);
	textLayout->setSpacing(S(1));
	cardText(textLayout, title, Colors::TEXT_PRIMARY, CardText::kTitle, true);
	cardText(textLayout, help, Colors::TEXT_SECONDARY, CardText::kCaption);

	rowLayout->addWidget(textColumn, 1);
	// Top-aligned, not centred: the help text can run to two or three lines and
	// a centred switch then drifts away from the title it belongs to.
	su::SwitchButton *toggle = su::CreateStyledSwitch("", initial);
	rowLayout->addWidget(toggle, 0, Qt::AlignTop);

	layout->addWidget(row);
	return toggle;
}

/**
 * The "it worked" window. Deliberately not a one-line message box: a backup
 * that quietly left things out is the failure mode that matters, so what was
 * saved, what was skipped and why all get their own section, with the actual
 * file lists rather than counts alone.
 */
void showResult(QWidget *parent, const Result &result, const QString &archivePath)
{
	su::WindowShell shell = su::makeWindow(obs_module_text("Backup.Result.Title"), "v" PROJECT_VERSION, parent,
					       /*brandFooter=*/true, "StreamUP");
	QDialog *dialog = shell.dialog;
	QVBoxLayout *layout = shell.content;
	layout->setContentsMargins(S(20), S(16), S(20), S(16));
	layout->setSpacing(cardColumnSpacing());

	cardText(layout, QFileInfo(archivePath).fileName(), Colors::TEXT_PRIMARY, CardText::kTitle, true);
	cardText(layout, QFileInfo(archivePath).absolutePath(), Colors::TEXT_SECONDARY, CardText::kCaption);

	// Two columns: the short summary blocks on the left, the long file lists on
	// the right where there is room for them to breathe.
	// 3:7 and a wider window: the left column only holds short fact tables,
	// while the right holds file paths that are unreadable when squeezed.
	auto columns = cardColumns(layout, 3, 7);
	QVBoxLayout *left = columns.first;
	QVBoxLayout *right = columns.second;

	// What went in, broken down by area.
	QVBoxLayout *saved = sectionCard(left, obs_module_text("Backup.Result.Section.Saved"));
	QGridLayout *grid = cardFacts(saved);
	int row = 0;
	for (const auto &area : result.areaCounts) {
		if (area.second > 0)
			cardFact(grid, row++, area.first, QString::number(area.second));
	}
	if (result.mediaCollected > 0)
		cardFact(grid, row++, obs_module_text("Backup.Result.Fact.Media"), QString::number(result.mediaCollected));
	cardFact(grid, row++, obs_module_text("Backup.Result.Fact.Total"),
	     QStringLiteral("%1 files, %2").arg(result.fileCount).arg(formatBytes(result.archiveBytes)));

	// Credentials: always stated, never assumed.
	QVBoxLayout *security = sectionCard(left, obs_module_text("Backup.Result.Section.Sharing"));
	cardText(security,
		 result.credentialsIncluded ? obs_module_text("Backup.Result.WithCredentials")
					    : obs_module_text("Backup.Result.NoCredentials"),
		 result.credentialsIncluded ? Colors::COLOR_WARNING : Colors::TEXT_SECONDARY, CardText::kBody,
		 result.credentialsIncluded);

	// Missing media, with the actual paths and which collection wanted them.
	if (!result.missingMedia.isEmpty()) {
		QVBoxLayout *missing = sectionCard(right, obs_module_text("Backup.Result.Section.Missing"));
		cardText(missing, QString(obs_module_text("Backup.Result.MissingExplain")).arg(result.mediaMissing),
			       Colors::COLOR_WARNING, 12);
		QList<FileRow> rows;
		for (const MediaReference &ref : result.missingMedia)
			rows.append({ref.path, ref.collection});
		missing->addWidget(cardFileTable(rows, obs_module_text("Backup.Result.Column.UsedBy"), 260));

		// Export, so the list can be worked through outside OBS rather than
		// copied off the screen by hand.
		auto *exportRow = new QHBoxLayout();
		exportRow->setContentsMargins(0, S(4), 0, 0);
		exportRow->addStretch();
		auto *exportButton = new su::PillButton(obs_module_text("Backup.Result.ExportMissing"), "neutral");
		exportRow->addWidget(exportButton);
		missing->addLayout(exportRow);

		// One button, two formats. A menu rather than two buttons: the choice
		// is between file types, not two different actions.
		QObject::connect(exportButton, &QPushButton::clicked, dialog, [dialog, rows, exportButton]() {
			auto *menu = new QMenu(dialog);
			menu->setAttribute(Qt::WA_DeleteOnClose);

			auto run = [dialog, rows](FileExportFormat format) {
				const QString stamp =
					QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd"));
				const QString written = exportFileRows(
					dialog, rows, obs_module_text("Backup.Result.ExportTitle"),
					obs_module_text("Backup.Result.Column.UsedBy"),
					QStringLiteral("streamup-missing-files-%1").arg(stamp), format);
				if (!written.isEmpty())
					su::info(dialog, obs_module_text("Backup.Result.ExportDone.Title"),
						 QString(obs_module_text("Backup.Result.ExportDone.Message"))
							 .arg(rows.size())
							 .arg(QFileInfo(written).fileName()));
			};

			QObject::connect(menu->addAction(obs_module_text("Backup.Result.Export.Text")),
					 &QAction::triggered, dialog,
					 [run]() { run(FileExportFormat::Text); });
			QObject::connect(menu->addAction(obs_module_text("Backup.Result.Export.Csv")),
					 &QAction::triggered, dialog, [run]() { run(FileExportFormat::Csv); });

			menu->popup(exportButton->mapToGlobal(QPoint(0, exportButton->height())));
		});
	}

	// Oversized files left out on purpose.
	if (!result.skippedLargeFiles.isEmpty()) {
		QVBoxLayout *skipped = sectionCard(right, obs_module_text("Backup.Result.Section.Skipped"));
		cardText(skipped,
			       QString(obs_module_text("Backup.Result.SkippedExplain"))
				       .arg(result.skippedLargeFiles.size())
				       .arg(formatBytes(result.skippedBytes)),
			       Colors::TEXT_SECONDARY, 12);
		QList<FileRow> rows;
		for (const SkippedFile &file : result.skippedLargeFiles)
			rows.append({file.path, formatBytes(file.size)});
		skipped->addWidget(cardFileTable(rows, obs_module_text("Backup.Result.Column.Size"), 120));
	}

	left->addStretch();
	right->addStretch();

	auto *okButton = new su::PillButton(obs_module_text("UI.Button.OK"), "primary");
	shell.footerButtons->addWidget(okButton);
	QObject::connect(okButton, &QPushButton::clicked, dialog, &QDialog::close);

	// Wider so the file lists have a column of their own rather than
	// squeezing full paths into a narrow window.
	// activate() first: sizeHint() is stale until the layout has run, so
	// measuring before it leaves the window taller than its contents.
	dialog->layout()->activate();
	dialog->resize(S(1080) + 2 * S(su::ShadowDialog::kShadowMargin), dialog->sizeHint().height());
	// Shrink to the content's real height once it has been laid out. See
	// fitWindowToContent: no size hint matches what actually gets drawn here.
	QTimer::singleShot(0, dialog, [dialog, layout]() { fitWindowToContent(dialog, layout); });

	dialog->show();
}

} // namespace

void ShowCreateBackupDialog()
{
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());

	su::WindowShell shell = su::makeWindow(obs_module_text("Backup.Dialog.Title"), "v" PROJECT_VERSION, parent,
					       /*brandFooter=*/true, "StreamUP");
	QDialog *dialog = shell.dialog;
	QVBoxLayout *layout = shell.content;
	layout->setContentsMargins(S(20), S(16), S(20), S(16));
	layout->setSpacing(cardColumnSpacing());

	// ── What a backup covers ───────────────────────────────────────────
	QVBoxLayout *whatCard = sectionCard(layout, obs_module_text("Backup.Section.Includes"));
	cardText(whatCard, obs_module_text("Backup.Includes.List"), Colors::TEXT_PRIMARY, CardText::kBody);
	cardText(whatCard, obs_module_text("Backup.Includes.Excluded"), Colors::TEXT_SECONDARY, CardText::kCaption);

	// ── What this will produce, measured now ───────────────────────────
	Options estimateOptions;
	const Estimate estimate = EstimateBackup(estimateOptions);
	const Locations loc = ResolveLocations();
	const QList<MediaReference> media = ScanMediaReferences(loc);
	int missingCount = 0;
	for (const MediaReference &ref : media) {
		if (!ref.exists)
			missingCount++;
	}

	QVBoxLayout *sizeCard = sectionCard(layout, obs_module_text("Backup.Section.YourSetup"));
	QGridLayout *grid = cardFacts(sizeCard);
	int row = 0;
	cardFact(grid, row++, obs_module_text("Backup.Fact.Files"),
	     QStringLiteral("%1  (%2)").arg(estimate.fileCount).arg(formatBytes(estimate.totalBytes)));
	cardFact(grid, row++, obs_module_text("Backup.Fact.Mode"),
	     loc.portable ? obs_module_text("Restore.Info.Portable") : obs_module_text("Restore.Info.Installed"));
	if (!media.isEmpty())
		cardFact(grid, row++, obs_module_text("Backup.Fact.Referenced"),
		     missingCount > 0 ? QString(obs_module_text("Backup.Fact.ReferencedMissing"))
						.arg(media.size())
						.arg(missingCount)
				      : QString::number(media.size()),
		     missingCount > 0 ? Colors::COLOR_WARNING : Colors::TEXT_PRIMARY);
	if (estimate.largeFileCount > 0)
		cardFact(grid, row++, obs_module_text("Backup.Fact.LargeSkipped"),
		     QStringLiteral("%1  (%2)").arg(estimate.largeFileCount).arg(formatBytes(estimate.largeFileBytes)),
		     Colors::COLOR_WARNING);

	// ── Options ────────────────────────────────────────────────────────
	QVBoxLayout *optionsCard = sectionCard(layout, obs_module_text("Backup.Section.Options"));
	su::SwitchButton *credentialsToggle = toggleRow(optionsCard, obs_module_text("Backup.Option.Credentials"),
							obs_module_text("Backup.Option.CredentialsDesc"), false);
	su::SwitchButton *mediaToggle = toggleRow(optionsCard, obs_module_text("Backup.Option.CollectMedia"),
						  obs_module_text("Backup.Option.CollectMediaDesc"), false);

	// Spare height belongs here, below the last card, so the cards themselves
	// stay sized to their contents.
	layout->addStretch();

	// Progress lives in its own container so that while hidden it takes no
	// space at all: the window opens at the height of its content, then grows
	// by exactly this container when a backup starts.
	auto *progressArea = new QWidget();
	auto *progressLayout = new QVBoxLayout(progressArea);
	progressLayout->setContentsMargins(0, S(4), 0, 0);
	progressLayout->setSpacing(S(4));

	auto *progress = new QProgressBar();
	progress->setTextVisible(false);
	progressLayout->addWidget(progress);
	QLabel *status = cardText(progressLayout, QString(), Colors::TEXT_SECONDARY, CardText::kCaption);

	progressArea->setVisible(false);
	layout->addWidget(progressArea);

	auto *cancelButton = new su::PillButton(obs_module_text("UI.Button.Cancel"), "outline");
	auto *backupButton = new su::PillButton(obs_module_text("Backup.Button.Create"), "primary");
	shell.footerButtons->addWidget(cancelButton);
	shell.footerButtons->addWidget(backupButton);

	QObject::connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::close);

	QObject::connect(backupButton, &QPushButton::clicked, [=]() {
		// Same folder as the automatic backups by default, so backups are in
		// one place. Pruning only ever touches streamup-auto-*.zip, so a manual
		// backup saved here is never deleted for you.
		QString defaultDir = ResolveBackupFolder();
		if (defaultDir.isEmpty())
			defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
		QDir().mkpath(defaultDir);
		const QString path = QFileDialog::getSaveFileName(
			dialog, obs_module_text("Backup.Dialog.SaveTitle"),
			QDir(defaultDir).filePath(SuggestedFileName()), QStringLiteral("Zip archive (*.zip)"));
		if (path.isEmpty())
			return;

		Options options;
		options.includeCredentials = credentialsToggle->isChecked();
		options.collectMedia = mediaToggle->isChecked();

		backupButton->setEnabled(false);
		cancelButton->setEnabled(false);

		// Grow to make room, rather than reserving the space up front.
		const int grow = progressArea->sizeHint().height() + layout->spacing();
		progressArea->setVisible(true);
		dialog->resize(dialog->width(), dialog->height() + grow);

		const Result result = CreateBackup(path, options, [=](const QString &file, int done, int total) {
			progress->setMaximum(total);
			progress->setValue(done);
			status->setText(QString(obs_module_text("Backup.Status.Working")).arg(file));
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			return true;
		});

		progressArea->setVisible(false);
		dialog->resize(dialog->width(), qMax(0, dialog->height() - grow));
		backupButton->setEnabled(true);
		cancelButton->setEnabled(true);

		if (!result.success) {
			su::info(parent, obs_module_text("Backup.Result.Title"),
				 QString(obs_module_text("Backup.Status.Failed")).arg(result.error));
			return;
		}

		dialog->close();
		showResult(parent, result, path);
	});

	// activate() first: sizeHint() is stale until the layout has run, so
	// measuring before it leaves the window taller than its contents.
	dialog->layout()->activate();
	dialog->resize(S(560) + 2 * S(su::ShadowDialog::kShadowMargin), dialog->sizeHint().height());
	// Shrink to the content's real height once it has been laid out. See
	// fitWindowToContent: no size hint matches what actually gets drawn here.
	QTimer::singleShot(0, dialog, [dialog, layout]() { fitWindowToContent(dialog, layout); });

	dialog->show();
}

} // namespace Backup
} // namespace StreamUP
