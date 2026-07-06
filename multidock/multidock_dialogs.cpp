#include "multidock_dialogs.hpp"
#include "../utilities/debug-logger.hpp"
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

void ShowManageMultiDocksDialog()
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return;
    }
    
    // Stack ShadowDialog + chrome; keep modal exec(). brandFooter=false.
    ShadowDialog dialog(mainWindow);
    WindowShell sh = applyChrome(&dialog, obs_module_text("MultiDock.Dialog.ManageTitle"), "v" PROJECT_VERSION,
                                 /*brandFooter=*/false, "StreamUP");
    sh.content->setContentsMargins(S(20), S(16), S(20), S(16));
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.resize(S(440) + 2 * S(ShadowDialog::kShadowMargin), S(340) + 2 * S(ShadowDialog::kShadowMargin));

    QVBoxLayout* layout = sh.content;

    // List widget
    layout->addWidget(new QLabel(obs_module_text("MultiDock.Label.ExistingDocks")));
    QListWidget* listWidget = new QListWidget(&dialog);
    listWidget->setStyleSheet(listStyle());
    useScrollBars(listWidget);

    layout->addWidget(listWidget);
    
    // Populate with existing MultiDocks
    MultiDockManager* manager = MultiDockManager::Instance();
    if (manager) {
        QList<MultiDockInfo> multiDocks = manager->GetMultiDockInfoList();
        for (const MultiDockInfo& info : multiDocks) {
            QListWidgetItem* item = new QListWidgetItem(info.name);
            item->setData(Qt::UserRole, info.id);
            listWidget->addItem(item);
        }
        
        if (multiDocks.isEmpty()) {
            listWidget->addItem(obs_module_text("MultiDock.Message.NoMultiDocksCreated"));
        }
    } else {
        listWidget->addItem(obs_module_text("MultiDock.Message.SystemNotInitialized"));
    }
    
    // Buttons. New = Plus icon button; Delete = Trash icon button (the SoT now
    // has Glyph::Trash) — the two icon-only actions pair cleanly on the left.
    // Open/Rename/Close = labelled pills on the right. Placed in the footer's
    // extra-rows slot (sh.footer) as one full-width row to preserve the split.
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(S(10));

    IconButton* newButton = new IconButton(IconButton::Glyph::Plus, &dialog);
    newButton->setToolTip(obs_module_text("MultiDock.Button.New"));

    IconButton* deleteButton = new IconButton(IconButton::Glyph::Trash, &dialog);
    deleteButton->setToolTip(obs_module_text("MultiDock.Button.Delete"));

    PillButton* openButton = new PillButton(obs_module_text("UI.Button.Open"), "primary");
    openButton->setParent(&dialog);
    PillButton* renameButton = new PillButton(obs_module_text("MultiDock.Button.Rename"), "neutral");
    renameButton->setParent(&dialog);
    PillButton* closeButton = new PillButton(obs_module_text("UI.Button.Close"), "outline");
    closeButton->setParent(&dialog);

    // Enable/disable buttons based on selection
    bool hasSelection = listWidget->currentItem() != nullptr &&
                       !listWidget->currentItem()->data(Qt::UserRole).toString().isEmpty();
    openButton->setEnabled(hasSelection);
    renameButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);

    // Left side: New, Delete
    buttonLayout->addWidget(newButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    // Right side: Open, Rename, Close
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(renameButton);
    buttonLayout->addWidget(closeButton);
    sh.footer->addLayout(buttonLayout);
    
    // Connect signals - use explicit captures instead of [&]
    QObject::connect(listWidget, &QListWidget::itemSelectionChanged, [listWidget, openButton, renameButton, deleteButton]() {
        bool hasValidSelection = listWidget->currentItem() != nullptr && 
                               !listWidget->currentItem()->data(Qt::UserRole).toString().isEmpty();
        openButton->setEnabled(hasValidSelection);
        renameButton->setEnabled(hasValidSelection);
        deleteButton->setEnabled(hasValidSelection);
    });
    
    QObject::connect(openButton, &QPushButton::clicked, [listWidget, manager, &dialog]() {
        QListWidgetItem* item = listWidget->currentItem();
        if (item) {
            QString id = item->data(Qt::UserRole).toString();
            if (!id.isEmpty() && manager) {
                MultiDockDock* multiDock = manager->GetMultiDock(id);
                if (multiDock) {
                    // Find and show the actual OBS dock widget
                    QMainWindow* mainWindow = GetObsMainWindow();
                    if (mainWindow) {
                        QDockWidget* obsDocWidget = mainWindow->findChild<QDockWidget*>(id);
                        if (obsDocWidget) {
                            obsDocWidget->show();
                            obsDocWidget->raise();
                            obsDocWidget->activateWindow();
                            dialog.accept();
                        } else {
                            StreamUP::DebugLogger::LogWarning("MultiDock", "Dialog: Could not find OBS dock widget to open");
                        }
                    }
                }
            }
        }
    });
    
    QObject::connect(deleteButton, &QPushButton::clicked, [listWidget, manager, &dialog]() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item || !manager) {
            return;
        }

        // Copy id/name into values the modeless confirm callback owns; QPointer-
        // guard the list (the outer modal dialog owns it and stays alive, but the
        // callback runs after the synchronous click handler returns).
        const QString id = item->data(Qt::UserRole).toString();
        const QString name = item->text();
        if (id.isEmpty()) {
            return;
        }

        QPointer<QListWidget> listGuard(listWidget);
        const QString message = QString(obs_module_text("MultiDock.Confirm.Delete")).arg(name) +
                                "\n\n" + obs_module_text("MultiDock.Info.DeleteRestore");

        su::confirm(&dialog, obs_module_text("MultiDock.Dialog.DeleteTitle"), message,
                    obs_module_text("MultiDock.Button.Delete"), "danger",
                    [manager, id, name, listGuard, dlg = &dialog]() {
            // Simple immediate deletion
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "Attempting to remove MultiDock '%s'", name.toUtf8().constData());

            bool success = manager->RemoveMultiDock(id);
            if (success) {
                // Remove from the UI list (re-find by id — the row may have shifted).
                if (listGuard) {
                    for (int i = 0; i < listGuard->count(); ++i) {
                        QListWidgetItem* it = listGuard->item(i);
                        if (it && it->data(Qt::UserRole).toString() == id) {
                            delete listGuard->takeItem(i);
                            break;
                        }
                    }
                }
                StreamUP::DebugLogger::LogDebug("MultiDock", "Dialog", "Successfully removed MultiDock from UI");
            } else {
                StreamUP::DebugLogger::LogWarning("MultiDock", "Dialog: Failed to remove MultiDock");
                su::info(dlg, obs_module_text("StreamUP.MultiDock.Error"), obs_module_text("StreamUP.MultiDock.FailedToDelete"));
            }
        });
    });
    
    QObject::connect(newButton, &QPushButton::clicked, [listWidget, &dialog]() {
        ShowCreateMultiDockDialog(&dialog, listWidget);
    });
    
    QObject::connect(renameButton, &QPushButton::clicked, [listWidget, manager, &dialog]() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item || !manager) {
            return;
        }
        
        QString id = item->data(Qt::UserRole).toString();
        QString currentName = item->text();
        if (id.isEmpty()) {
            return;
        }
        
        // Show rename dialog — stack ShadowDialog + chrome, modal exec.
        ShadowDialog renameDialog(&dialog);
        WindowShell rsh = applyChrome(&renameDialog, obs_module_text("MultiDock.Dialog.RenameTitle"), "v" PROJECT_VERSION,
                                      /*brandFooter=*/false, "StreamUP");
        rsh.content->setContentsMargins(S(20), S(16), S(20), S(16));
        renameDialog.setWindowModality(Qt::ApplicationModal);
        renameDialog.resize(S(360) + 2 * S(ShadowDialog::kShadowMargin), S(150) + 2 * S(ShadowDialog::kShadowMargin));

        QVBoxLayout* renameLayout = rsh.content;

        // Name input
        renameLayout->addWidget(new QLabel(obs_module_text("MultiDock.Label.NewName")));
        QLineEdit* nameEdit = new QLineEdit(&renameDialog);
        nameEdit->setText(currentName);
        nameEdit->selectAll();
        nameEdit->setStyleSheet(lineEditStyle());
        nameEdit->setFixedHeight(S(28));
        renameLayout->addWidget(nameEdit);

        // Buttons — right-anchored in the footer (Cancel outline, Save primary).
        PillButton* saveButton = new PillButton(obs_module_text("UI.Button.Save"), "primary");
        PillButton* cancelButton = new PillButton(obs_module_text("UI.Button.Cancel"), "outline");
        rsh.footerButtons->addWidget(cancelButton);
        rsh.footerButtons->addWidget(saveButton);

        // Connect signals
        QObject::connect(cancelButton, &QPushButton::clicked, &renameDialog, &QDialog::reject);
        QObject::connect(saveButton, &QPushButton::clicked, [&renameDialog, nameEdit, manager, item, id, currentName]() {
            QString newName = nameEdit->text().trimmed();
            if (newName.isEmpty()) {
                su::info(&renameDialog, obs_module_text("Plugin.Error.InvalidName"), obs_module_text("MultiDock.Error.InvalidName"));
                return;
            }

            if (newName == currentName) {
                // No change needed
                renameDialog.accept();
                return;
            }

            bool success = manager->RenameMultiDock(id, newName);
            if (success) {
                // Update the UI list
                item->setText(newName);
                StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "Successfully renamed MultiDock to '%s'", newName.toUtf8().constData());
                renameDialog.accept();
            } else {
                su::info(&renameDialog, obs_module_text("StreamUP.MultiDock.Error"), obs_module_text("StreamUP.MultiDock.FailedToRename"));
            }
        });

        // Make save button default
        saveButton->setDefault(true);
        nameEdit->setFocus();

        renameDialog.exec();
    });
    
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    
    StreamUP::DebugLogger::LogDebug("MultiDock", "Dialog", "Showing manage dialog");
    
    dialog.exec();
}

} // namespace MultiDock
} // namespace StreamUP
