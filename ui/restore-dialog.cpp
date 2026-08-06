#include "restore-dialog.hpp"

#include "../core/backup-manager.hpp"
#include "../core/restore-manager.hpp"
#include "../utilities/debug-logger.hpp"
#include "backup-ui-common.hpp"
#include <streamup/ui/section-card.hpp>
#include "version.h"

#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/segmented-control.hpp>
#include <streamup/ui/switch-button.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/window-chrome.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QProgressBar>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

using namespace StreamUP::UIStyles;
namespace su = StreamUP::UIStyles;

namespace StreamUP {
namespace Restore {

namespace {

QString formatCreated(const QString &iso)
{
	const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
	if (!dt.isValid())
		return iso;
	return dt.toLocalTime().toString(QStringLiteral("d MMM yyyy 'at' HH:mm"));
}

} // namespace

void ShowAppliedReportIfAny()
{
	const AppliedReport report = ReadAppliedReport();
	if (!report.present || report.reported)
		return;

	// Mark it seen straight away: this should appear once after the restore,
	// not on every launch from here on.
	MarkReportSeen();

	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	const bool clean = (report.failed == 0);

	su::WindowShell shell = su::makeWindow(clean ? obs_module_text("Restore.Report.TitleOk")
						     : obs_module_text("Restore.Report.TitleProblem"),
					       "v" PROJECT_VERSION, parent, /*brandFooter=*/true, "StreamUP");
	QDialog *dialog = shell.dialog;
	QVBoxLayout *layout = shell.content;
	layout->setContentsMargins(S(20), S(16), S(20), S(16));
	layout->setSpacing(S(12));

	cardText(layout,
		clean ? obs_module_text("Restore.Report.HeadlineOk") : obs_module_text("Restore.Report.HeadlineProblem"),
		clean ? Colors::TEXT_PRIMARY : Colors::COLOR_WARNING, CardText::kTitle, true);

	QVBoxLayout *summary = sectionCard(layout, obs_module_text("Restore.Report.Section.Summary"));
	QGridLayout *grid = cardFacts(summary);
	int row = 0;
	cardFact(grid, row++, obs_module_text("Restore.Report.Fact.Restored"),
		QStringLiteral("%1 / %2").arg(report.applied + report.alreadyInPlace).arg(report.total),
		Colors::TEXT_PRIMARY);
	if (report.alreadyInPlace > 0)
		cardFact(grid, row++, obs_module_text("Restore.Report.Fact.AlreadyCorrect"),
			QString::number(report.alreadyInPlace), Colors::TEXT_SECONDARY);
	cardFact(grid, row++, obs_module_text("Restore.Report.Fact.Failed"), QString::number(report.failed),
		report.failed > 0 ? Colors::COLOR_WARNING : Colors::TEXT_SECONDARY);
	if (!report.sourceArchive.isEmpty())
		cardFact(grid, row++, obs_module_text("Restore.Report.Fact.From"),
			QFileInfo(report.sourceArchive).fileName(), Colors::TEXT_SECONDARY);

	if (!report.failures.isEmpty()) {
		QVBoxLayout *failed = sectionCard(layout, obs_module_text("Restore.Report.Section.Failures"));
		cardText(failed, obs_module_text("Restore.Report.FailureExplain"), Colors::TEXT_SECONDARY, CardText::kBody);
		failed->addWidget(cardList(report.failures, 140));
	}

	if (!report.safetyBackup.isEmpty()) {
		QVBoxLayout *safety = sectionCard(layout, obs_module_text("Restore.Report.Section.Safety"));
		cardText(safety, QString(obs_module_text("Restore.Report.SafetyExplain"))
					.arg(QFileInfo(report.safetyBackup).fileName()),
			Colors::TEXT_SECONDARY, CardText::kBody);
	}

	layout->addStretch();

	auto *okButton = new su::PillButton(obs_module_text("UI.Button.OK"), "primary");
	shell.footerButtons->addWidget(okButton);
	QObject::connect(okButton, &QPushButton::clicked, dialog, &QDialog::close);

	// activate() first: sizeHint() is stale until the layout has run, so
	// measuring before it leaves the window taller than its contents.
	dialog->layout()->activate();
	dialog->resize(S(560) + 2 * S(su::ShadowDialog::kShadowMargin), dialog->sizeHint().height());
	// Shrink to the content's real height once it has been laid out. See
	// fitWindowToContent: no size hint matches what actually gets drawn here.
	QTimer::singleShot(0, dialog, [dialog, layout]() { fitWindowToContent(dialog, layout); });

	dialog->show();
}

void ShowRestoreDialog()
{
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());

	// Start where the backups actually are. Automatic backups live in the
	// configured folder, and opening in Documents made them hard to find.
	QString startDir = Backup::ResolveBackupFolder();
	if (startDir.isEmpty() || !QDir(startDir).exists())
		startDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	const QString archivePath =
		QFileDialog::getOpenFileName(parent, obs_module_text("Restore.Dialog.PickTitle"), startDir,
					     QStringLiteral("StreamUP backup (*.zip)"));
	if (archivePath.isEmpty())
		return;

	const Inspection info = Inspect(archivePath);
	if (!info.valid) {
		su::info(parent, obs_module_text("Restore.Error.Title"), info.error);
		return;
	}

	su::WindowShell shell = su::makeWindow(obs_module_text("Restore.Dialog.Title"), "v" PROJECT_VERSION, parent,
					       /*brandFooter=*/true, "StreamUP");
	QDialog *dialog = shell.dialog;
	QVBoxLayout *layout = shell.content;
	layout->setContentsMargins(S(20), S(16), S(20), S(16));
	layout->setSpacing(cardColumnSpacing());

	// ── The backup itself ──────────────────────────────────────────────
	cardText(layout, QFileInfo(archivePath).fileName(), Colors::TEXT_PRIMARY, CardText::kTitle, true);

	// Short blocks left, the browsable contents right: the contents list is the
	// tall element, so giving it its own column keeps the window from running
	// down the screen.
	// 5:5 with a floor on the left: at 4:6 the contents list claimed so much
	// width that the facts on the left wrapped mid-value ("OBS 32.2.1," on one
	// line) and the steps were cut off.
	auto columns = cardColumns(layout, 5, 5);
	QVBoxLayout *leftColumn = columns.first;
	QVBoxLayout *rightColumn = columns.second;
	leftColumn->addStrut(S(300));

	QVBoxLayout *aboutCard = sectionCard(leftColumn, obs_module_text("Restore.Section.About"));
	QGridLayout *facts = cardFacts(aboutCard);
	int row = 0;
	cardFact(facts, row++, obs_module_text("Restore.Fact.Taken"), formatCreated(info.created),
		Colors::TEXT_PRIMARY);
	cardFact(facts, row++, obs_module_text("Restore.Fact.From"),
		QStringLiteral("OBS %1, %2")
			.arg(info.obsVersion, info.portable ? obs_module_text("Restore.Info.Portable")
							    : obs_module_text("Restore.Info.Installed")),
		Colors::TEXT_PRIMARY);
	cardFact(facts, row++, obs_module_text("Restore.Fact.StreamKey"),
		info.credentialsIncluded ? obs_module_text("Restore.Fact.StreamKeyYes")
					 : obs_module_text("Restore.Fact.StreamKeyNo"),
		info.credentialsIncluded ? Colors::TEXT_PRIMARY : Colors::COLOR_WARNING);

	// ── What is inside, one page per area ──────────────────────────────
	// SegmentedControl + QStackedWidget rather than a QTabWidget: the
	// segmented control is the house picker (STANDARD.md section 5), and a
	// QTabWidget truncates its own labels once there are five of them, which
	// is exactly what it did here.
	QVBoxLayout *contentsCard = sectionCard(rightColumn, obs_module_text("Restore.Section.Contents"));

	cardText(contentsCard, obs_module_text("Restore.Section.ContentsHint"), Colors::TEXT_SECONDARY,
		 CardText::kCaption);

	// Each area gets a switch, so a restore can be narrowed to just what was
	// lost. Scene collections additionally get a checkable list, because
	// "restore one collection" is the case people actually hit.
	auto *selection = new Selection();
	QStringList pageLabels;
	QList<QWidget *> pageWidgets;

	auto *collectionList = new QListWidget();
	collectionList->setStyleSheet(listStyle());
	collectionList->setSelectionMode(QAbstractItemView::NoSelection);
	collectionList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	useScrollBars(collectionList);
	for (const QString &name : info.sceneCollectionNames) {
		auto *item = new QListWidgetItem(name, collectionList);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(Qt::Checked);
	}
	QObject::connect(collectionList, &QListWidget::itemChanged, dialog, [collectionList, selection](QListWidgetItem *) {
		selection->onlyCollections.clear();
		int checked = 0;
		for (int i = 0; i < collectionList->count(); ++i) {
			if (collectionList->item(i)->checkState() == Qt::Checked) {
				checked++;
				selection->onlyCollections << collectionList->item(i)->text();
			}
		}
		// All ticked means "no filter", which keeps the journal honest about
		// whether this was a partial restore.
		if (checked == collectionList->count())
			selection->onlyCollections.clear();
		selection->sceneCollections = (checked > 0);
	});

	// Select all / none. Restoring a single collection out of thirty means
	// unticking twenty-nine by hand otherwise, which is the exact case this
	// feature exists for.
	auto *collectionPage = new QWidget();
	auto *collectionLayout = new QVBoxLayout(collectionPage);
	collectionLayout->setContentsMargins(0, S(6), 0, 0);
	collectionLayout->setSpacing(S(6));

	auto *bulkRow = new QHBoxLayout();
	bulkRow->setContentsMargins(0, 0, 0, 0);
	bulkRow->setSpacing(S(8));
	auto *allButton = new su::PillButton(obs_module_text("Restore.Select.All"), "outline");
	auto *noneButton = new su::PillButton(obs_module_text("Restore.Select.None"), "outline");
	bulkRow->addWidget(allButton);
	bulkRow->addWidget(noneButton);
	bulkRow->addStretch();
	collectionLayout->addLayout(bulkRow);
	collectionLayout->addWidget(collectionList, 1);

	auto setAllChecked = [collectionList](bool checked) {
		for (int i = 0; i < collectionList->count(); ++i)
			collectionList->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	};
	QObject::connect(allButton, &QPushButton::clicked, dialog, [setAllChecked]() { setAllChecked(true); });
	QObject::connect(noneButton, &QPushButton::clicked, dialog, [setAllChecked]() { setAllChecked(false); });

	auto wrapPage = [](QWidget *inner) {
		auto *page = new QWidget();
		auto *layout = new QVBoxLayout(page);
		layout->setContentsMargins(0, S(6), 0, 0);
		layout->addWidget(inner);
		page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
		return page;
	};

	pageLabels << QStringLiteral("%1 (%2)").arg(obs_module_text("Restore.Tab.Scenes")).arg(info.sceneCollections);
	collectionPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	pageWidgets << collectionPage;

	auto addSimplePage = [&](const QString &label, const QStringList &items, int count) {
		pageLabels << QStringLiteral("%1 (%2)").arg(label).arg(count);
		pageWidgets << wrapPage(cardList(items.isEmpty()
							 ? QStringList{obs_module_text("Restore.Tab.Empty")}
							 : items,
						 300));
	};

	addSimplePage(obs_module_text("Restore.Tab.Profiles"), info.profileNames, info.profiles);
	addSimplePage(obs_module_text("Restore.Tab.Plugins"), info.pluginConfigNames, info.pluginConfigFiles);
	addSimplePage(obs_module_text("Restore.Tab.Themes"), info.themeNames, info.themeFiles);
	if (info.mediaFiles > 0)
		addSimplePage(obs_module_text("Restore.Tab.Media"), QStringList(), info.mediaFiles);

	auto *picker = new su::SegmentedControl(pageLabels);
	contentsCard->addWidget(picker);

	auto *pages = new QStackedWidget();
	for (QWidget *page : pageWidgets)
		pages->addWidget(page);
	pages->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	contentsCard->addWidget(pages, 1);
	picker->onChanged([pages](int index) { pages->setCurrentIndex(index); });

	// Let this card grow to the height of the column beside it. Cards hug their
	// content by default, which is right for a fact table but wastes the space
	// next to a taller left column when the content is a scrollable list.
	if (QWidget *contentsFrame = contentsCard->parentWidget())
		contentsFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	// ── What to put back ─────────────────────────────────────────────
	QVBoxLayout *whatCard = sectionCard(leftColumn, obs_module_text("Restore.Section.WhatToRestore"));
	auto areaToggle = [&](const QString &label, bool *flag, bool enabled) {
		auto *row = new QWidget();
		auto *rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		auto *text = new QLabel(label);
		text->setStyleSheet(scale_qss(QString("QLabel{color:%1;font-size:%2px;background:transparent;}")
						      .arg(QString(enabled ? Colors::TEXT_PRIMARY : Colors::TEXT_MUTED))
						      .arg(CardText::kBody)));
		rowLayout->addWidget(text, 1);
		auto *toggle = su::CreateStyledSwitch("", enabled);
		toggle->setEnabled(enabled);
		QObject::connect(toggle, &su::SwitchButton::toggled, dialog, [flag](bool on) { *flag = on; });
		rowLayout->addWidget(toggle, 0, Qt::AlignVCenter);
		whatCard->addWidget(row);
	};

	areaToggle(obs_module_text("Restore.Tab.Scenes"), &selection->sceneCollections, info.sceneCollections > 0);
	areaToggle(obs_module_text("Restore.Tab.Profiles"), &selection->profiles, info.profiles > 0);
	areaToggle(obs_module_text("Restore.Tab.Plugins"), &selection->pluginSettings, info.pluginConfigFiles > 0);
	areaToggle(obs_module_text("Restore.Tab.Themes"), &selection->themes, info.themeFiles > 0);
	areaToggle(obs_module_text("Restore.Area.ObsSettings"), &selection->obsSettings, true);
	if (info.mediaFiles > 0)
		areaToggle(obs_module_text("Restore.Tab.Media"), &selection->media, true);

	// ── Anything worth knowing before committing ───────────────────────
	const bool hasWarnings = !info.pluginGaps.isEmpty() || info.layoutDiffers;
	if (hasWarnings) {
		QVBoxLayout *warnCard = sectionCard(leftColumn, obs_module_text("Restore.Section.CheckFirst"));

		if (!info.pluginGaps.isEmpty()) {
			QStringList names;
			for (const PluginGap &gap : info.pluginGaps)
				names << gap.name;
			cardText(warnCard, QString(obs_module_text("Restore.Warn.MissingPluginsShort")).arg(names.size()),
				Colors::COLOR_WARNING, CardText::kBody, true);

			warnCard->addWidget(cardList(names, 90));
		}

		if (info.layoutDiffers)
			cardText(warnCard, obs_module_text("Restore.Warn.LayoutDiffers"), Colors::COLOR_WARNING, CardText::kBody);
	}

	// ── What is about to happen ────────────────────────────────────────
	QVBoxLayout *processCard = sectionCard(leftColumn, obs_module_text("Restore.Section.WhatHappens"));
	cardText(processCard, obs_module_text("Restore.Step.Safety"), Colors::TEXT_SECONDARY, CardText::kBody);
	cardText(processCard, obs_module_text("Restore.Step.Prepare"), Colors::TEXT_SECONDARY, CardText::kBody);
	cardText(processCard, obs_module_text("Restore.Step.Apply"), Colors::TEXT_SECONDARY, CardText::kBody);

	// Only the left column pools its slack: the right column's card expands.
	leftColumn->addStretch();

	// Progress lives in its own container so it takes up no space at all while
	// hidden. The window opens at the height of its content and grows by
	// exactly this container when the restore starts.
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
	auto *restoreButton = new su::PillButton(obs_module_text("Restore.Button.Restore"), "danger");
	shell.footerButtons->addWidget(cancelButton);
	shell.footerButtons->addWidget(restoreButton);

	QObject::connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::close);

	QObject::connect(restoreButton, &QPushButton::clicked, [=]() {
		restoreButton->setEnabled(false);
		cancelButton->setEnabled(false);

		const int grow = progressArea->sizeHint().height() + layout->spacing();
		progressArea->setVisible(true);
		dialog->resize(dialog->width(), dialog->height() + grow);
		progress->setRange(0, 0); // indeterminate until the file count is known
		status->setText(obs_module_text("Restore.Status.SafetyBackup"));
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		QString error;
		QString safetyPath;
		const bool staged = Stage(archivePath, &error, &safetyPath,
					  [=](const QString &stage, int done, int total) {
						  if (total > 0) {
							  progress->setRange(0, total);
							  progress->setValue(done);
						  }
						  status->setText(stage);
						  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
						  return true;
					  },
					  *selection);

		progressArea->setVisible(false);
		dialog->resize(dialog->width(), qMax(0, dialog->height() - grow));
		restoreButton->setEnabled(true);
		cancelButton->setEnabled(true);

		if (!staged) {
			su::info(parent, obs_module_text("Restore.Error.Title"), error);
			return;
		}

		dialog->close();
		su::info(parent, obs_module_text("Restore.Staged.Title"),
			 QString(obs_module_text("Restore.Staged.Message")).arg(QFileInfo(safetyPath).fileName()));
	});

	// Double width: the tabbed lists need room, and the fact table reads far
	// better in two columns than stacked.
	// activate() first: sizeHint() is stale until the layout has run, so
	// measuring before it leaves the window taller than its contents.
	dialog->layout()->activate();
	dialog->resize(S(1000) + 2 * S(su::ShadowDialog::kShadowMargin), dialog->sizeHint().height());
	// Shrink to the content's real height once it has been laid out. See
	// fitWindowToContent: no size hint matches what actually gets drawn here.
	QTimer::singleShot(0, dialog, [dialog, layout]() { fitWindowToContent(dialog, layout); });

	dialog->show();
}

} // namespace Restore
} // namespace StreamUP
