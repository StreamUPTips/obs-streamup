#include "multidock_dialogs.hpp"
#include <streamup/debug-logger.hpp>
#include "multidock_utils.hpp"
#include "multidock_manager.hpp"
#include "multidock_dock.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/dialogs.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include <streamup/ui/svg-icon.hpp>
#include "version.h"
#include "../ui/streamup-kit.hpp"
#include "../ui/ui-helpers.hpp"
#include "inner_dock_host.hpp"
#include <QTimer>
#include <QDockWidget>
#include <functional>
#include <obs-module.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QPointer>

using namespace StreamUP::UIStyles;
namespace su = StreamUP::UIStyles;

namespace StreamUP {
namespace MultiDock {

// Shared helper: shows the "create new MultiDock" dialog and returns the new ID (empty on cancel/failure).
// Optionally updates a QListWidget if provided.
static QString ShowCreateMultiDockDialog(QWidget* parent, QListWidget* listWidget = nullptr)
{
    // Stack ShadowDialog + chrome; keep modal exec() semantics so the caller can
    // read the created ID synchronously. brandFooter=false → secondary window.
    ShadowDialog dialog(parent);
    WindowShell sh = applyChrome(&dialog, obs_module_text("MultiDock.Dialog.NewTitle"), "v" PROJECT_VERSION,
                                 /*brandFooter=*/false, "StreamUP");
    sh.content->setContentsMargins(S(20), S(16), S(20), S(16));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.resize(S(360) + 2 * S(ShadowDialog::kShadowMargin), S(150) + 2 * S(ShadowDialog::kShadowMargin));

    QVBoxLayout* layout = sh.content;

    // Name input
    layout->addWidget(new QLabel(obs_module_text("MultiDock.Label.Name")));
    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(obs_module_text("MultiDock.Placeholder.Name"));
    nameEdit->setStyleSheet(lineEditStyle());
    nameEdit->setFixedHeight(S(28));
    layout->addWidget(nameEdit);

    // Collect any spare height at the bottom so the label and field stay
    // together at the top instead of drifting apart when the window is taller
    // than its content.
    layout->addStretch();

    // Buttons — right-anchored in the footer (Cancel outline left, primary right).
    PillButton* createButton = new PillButton(obs_module_text("MultiDock.Button.Create"), "primary");
    PillButton* cancelButton = new PillButton(obs_module_text("UI.Button.Cancel"), "outline");
    sh.footerButtons->addWidget(cancelButton);
    sh.footerButtons->addWidget(createButton);

    QString createdId;

    // Connect signals
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(createButton, &QPushButton::clicked, [&dialog, nameEdit, listWidget, &createdId]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            su::info(&dialog, obs_module_text("Plugin.Error.InvalidName"), obs_module_text("MultiDock.Error.InvalidName"));
            return;
        }

        MultiDockManager* manager = MultiDockManager::Instance();
        if (!manager) {
            su::info(&dialog, obs_module_text("Plugin.Error.Title"), obs_module_text("MultiDock.Error.SystemNotInitialized"));
            return;
        }

        QString id = manager->CreateMultiDock(name);
        if (id.isEmpty()) {
            su::info(&dialog, obs_module_text("Plugin.Error.Title"), obs_module_text("MultiDock.Error.CreationFailed"));
            return;
        }

        createdId = id;

        // Optionally update a list widget
        if (listWidget) {
            // Remove placeholder text if it exists
            if (listWidget->count() == 1) {
                QListWidgetItem* firstItem = listWidget->item(0);
                if (firstItem && firstItem->data(Qt::UserRole).toString().isEmpty()) {
                    delete listWidget->takeItem(0);
                }
            }

            QListWidgetItem* item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, id);
            listWidget->addItem(item);
            listWidget->setCurrentItem(item);
        }

        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "Created MultiDock: '%s' with ID '%s'",
             name.toUtf8().constData(), id.toUtf8().constData());

        dialog.accept();
    });

    // Make create button default
    createButton->setDefault(true);
    nameEdit->setFocus();

    dialog.exec();
    return createdId;
}

void ShowNewMultiDockDialog()
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return;
    }

    ShowCreateMultiDockDialog(mainWindow);
}

// Asks for a new name for a Multi-Dock. Returns true when it was renamed.
static bool RenameDialog(QWidget* parent, MultiDockManager* manager, const QString& id, const QString& currentName)
{
    ShadowDialog renameDialog(parent);
    WindowShell rsh = applyChrome(&renameDialog, obs_module_text("MultiDock.Dialog.RenameTitle"), "v" PROJECT_VERSION,
                                  /*brandFooter=*/false, "StreamUP");
    rsh.content->setContentsMargins(S(20), S(16), S(20), S(16));
    renameDialog.setWindowModality(Qt::ApplicationModal);
    renameDialog.resize(S(360) + 2 * S(ShadowDialog::kShadowMargin), S(150) + 2 * S(ShadowDialog::kShadowMargin));

    QLineEdit* nameEdit = new QLineEdit(&renameDialog);
    nameEdit->setText(currentName);
    nameEdit->selectAll();
    nameEdit->setStyleSheet(lineEditStyle());
    nameEdit->setFixedHeight(S(28));
    rsh.content->addWidget(new QLabel(obs_module_text("MultiDock.Label.NewName")));
    rsh.content->addWidget(nameEdit);
    rsh.content->addStretch();

    PillButton* saveButton = new PillButton(obs_module_text("UI.Button.Save"), "primary");
    PillButton* cancelButton = new PillButton(obs_module_text("UI.Button.Cancel"), "outline");
    rsh.footerButtons->addWidget(cancelButton);
    rsh.footerButtons->addWidget(saveButton);

    bool renamed = false;
    QObject::connect(cancelButton, &QPushButton::clicked, &renameDialog, &QDialog::reject);
    QObject::connect(saveButton, &QPushButton::clicked, [&renameDialog, nameEdit, manager, id, currentName, &renamed]() {
        const QString newName = nameEdit->text().trimmed();
        if (newName.isEmpty()) {
            su::info(&renameDialog, obs_module_text("Plugin.Error.InvalidName"), obs_module_text("MultiDock.Error.InvalidName"));
            return;
        }
        if (newName == currentName) {
            renameDialog.accept();
            return;
        }
        if (manager->RenameMultiDock(id, newName)) {
            renamed = true;
            renameDialog.accept();
        } else {
            su::info(&renameDialog, obs_module_text("StreamUP.MultiDock.Error"), obs_module_text("StreamUP.MultiDock.FailedToRename"));
        }
    });
    saveButton->setDefault(true);
    nameEdit->setFocus();
    renameDialog.exec();
    return renamed;
}

// Manage Multi-Docks: one row per Multi-Dock in the same row style as the
// plugin and product windows (name, how many docks it holds, then Open,
// Rename and a bin), and New Multi-Dock in the footer.
void ShowManageMultiDocksDialog()
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return;
    }
    namespace kit = StreamUP::Kit;

    ShadowDialog dialog(mainWindow);
    WindowShell sh = applyChrome(&dialog, obs_module_text("MultiDock.Dialog.ManageTitle"), "v" PROJECT_VERSION,
                                 /*brandFooter=*/false, "StreamUP");
    dialog.setWindowModality(Qt::ApplicationModal);
    QVBoxLayout* body = kit::ScrollingBody(sh.content);
    auto* listHost = new QVBoxLayout();
    listHost->setContentsMargins(0, 0, 0, 0);
    body->addLayout(listHost);
    body->addStretch(1);

    MultiDockManager* manager = MultiDockManager::Instance();
    std::function<void()> rebuild;
    rebuild = [&]() {
        while (QLayoutItem* old = listHost->takeAt(0)) {
            if (QWidget* w = old->widget())
                w->deleteLater();
            delete old;
        }
        const QList<MultiDockInfo> docks = manager ? manager->GetMultiDockInfoList() : QList<MultiDockInfo>();
        if (docks.isEmpty()) {
            listHost->addWidget(kit::EmptyState(QString(),
                manager ? obs_module_text("MultiDock.Message.NoMultiDocksCreated")
                        : obs_module_text("MultiDock.Message.SystemNotInitialized")));
            return;
        }
        auto* list = new kit::RowList();
        for (const MultiDockInfo& info : docks) {
            const QString id = info.id;
            const QString name = info.name;
            int inside = 0;
            if (MultiDockDock* md = manager->GetMultiDock(id))
                if (md->GetInnerHost())
                    inside = md->GetInnerHost()->GetAllDocks().size();
            auto* open = new PillButton(obs_module_text("UI.Button.Open"), "primary");
            QObject::connect(open, &QPushButton::clicked, &dialog, [id, &dialog]() {
                if (QMainWindow* main = GetObsMainWindow()) {
                    if (QDockWidget* dock = main->findChild<QDockWidget*>(id)) {
                        dock->show();
                        dock->raise();
                        dock->activateWindow();
                        dialog.accept();
                    }
                }
            });
            auto* rename = new PillButton(obs_module_text("MultiDock.Button.Rename"), "neutral");
            QObject::connect(rename, &QPushButton::clicked, &dialog, [&dialog, manager, id, name, &rebuild]() {
                if (RenameDialog(&dialog, manager, id, name))
                    QTimer::singleShot(0, &dialog, [&rebuild]() { rebuild(); });
            });
            auto* remove = new IconButton(IconButton::Glyph::Trash);
            remove->setToolTip(obs_module_text("MultiDock.Button.Delete"));
            remove->setAccessibleName(QString("%1 %2").arg(obs_module_text("MultiDock.Button.Delete"), name));
            QObject::connect(remove, &QPushButton::clicked, &dialog, [&dialog, manager, id, name, &rebuild]() {
                const QString message = QString(obs_module_text("MultiDock.Confirm.Delete")).arg(name) +
                                        "\n\n" + obs_module_text("MultiDock.Info.DeleteRestore");
                QPointer<ShadowDialog> guard(&dialog);
                su::confirm(&dialog, obs_module_text("MultiDock.Dialog.DeleteTitle"), message,
                            obs_module_text("MultiDock.Button.Delete"), "danger", [guard, manager, id, &rebuild]() {
                    if (!manager->RemoveMultiDock(id)) {
                        if (guard)
                            su::info(guard, obs_module_text("StreamUP.MultiDock.Error"),
                                     obs_module_text("StreamUP.MultiDock.FailedToDelete"));
                        return;
                    }
                    if (guard)
                        QTimer::singleShot(0, guard, [&rebuild]() { rebuild(); });
                });
            });
            list->addRow(name, inside == 1 ? QString(obs_module_text("MultiDock.Detail.OneDock"))
                                           : QString(obs_module_text("MultiDock.Detail.Docks")).arg(inside),
                         {open, rename, remove});
        }
        listHost->addWidget(list);
    };
    rebuild();

    auto* newButton = new PillButton(obs_module_text("MultiDock.Button.New"), "primary");
    auto* closeButton = new PillButton(obs_module_text("UI.Button.Close"), "outline");
    sh.footerButtons->addWidget(closeButton);
    sh.footerButtons->addWidget(newButton);
    QObject::connect(newButton, &QPushButton::clicked, &dialog, [&dialog, &rebuild]() {
        if (!ShowCreateMultiDockDialog(&dialog).isEmpty())
            QTimer::singleShot(0, &dialog, [&rebuild]() { rebuild(); });
    });
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    kit::FitToContent(&dialog, 480);
    StreamUP::UIHelpers::PlaceOverMainWindow(&dialog);
    StreamUP::DebugLogger::LogDebug("MultiDock", "Dialog", "Showing manage dialog");
    dialog.exec();
}

} // namespace MultiDock
} // namespace StreamUP
