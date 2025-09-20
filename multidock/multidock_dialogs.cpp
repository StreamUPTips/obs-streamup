#include "multidock_dialogs.hpp"
#include "../utilities/debug-logger.hpp"
#include "multidock_utils.hpp"
#include "multidock_manager.hpp"
#include "multidock_dock.hpp"
#include "../ui/ui-styles.hpp"
#include <obs-module.h>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>

namespace StreamUP {
namespace MultiDock {

void ShowNewMultiDockDialog()
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return;
    }
    
    QDialog dialog(mainWindow);
    dialog.setWindowTitle(obs_module_text("MultiDock.Dialog.NewTitle"));
    dialog.setModal(true);
    dialog.resize(300, 120);
    
    // Set dialog background to BG_DARKEST
    dialog.setStyleSheet(QString("QDialog { background-color: %1; }").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Name input
    layout->addWidget(new QLabel(obs_module_text("MultiDock.Label.Name")));
    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(obs_module_text("MultiDock.Placeholder.Name"));
    
    // Style the input box
    nameEdit->setStyleSheet(StreamUP::UIStyles::GetLineEditStyle());
    
    layout->addWidget(nameEdit);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* createButton = new QPushButton(obs_module_text("MultiDock.Button.Create"), &dialog);
    QPushButton* cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), &dialog);
    
    // Style buttons
    createButton->setStyleSheet(StreamUP::UIStyles::GetButtonStyle());
    cancelButton->setStyleSheet(StreamUP::UIStyles::GetButtonStyle());
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // Connect signals
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(createButton, &QPushButton::clicked, [&dialog, nameEdit]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dialog, obs_module_text("Plugin.Error.InvalidName"), obs_module_text("MultiDock.Error.InvalidName"));
            return;
        }
        
        // Create the MultiDock using the manager
        MultiDockManager* manager = MultiDockManager::Instance();
        if (!manager) {
            QMessageBox::critical(&dialog, obs_module_text("Plugin.Error.Title"), obs_module_text("MultiDock.Error.SystemNotInitialized"));
            return;
        }
        
        QString id = manager->CreateMultiDock(name);
        if (id.isEmpty()) {
            QMessageBox::critical(&dialog, obs_module_text("Plugin.Error.Title"), obs_module_text("MultiDock.Error.CreationFailed"));
            return;
        }
        
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "Created MultiDock: '%s' with ID '%s'",
             name.toUtf8().constData(), id.toUtf8().constData());
        
        dialog.accept();
    });
    
    // Make create button default
    createButton->setDefault(true);
    nameEdit->setFocus();
    
    dialog.exec();
}

void ShowManageMultiDocksDialog()
{
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return;
    }
    
    QDialog dialog(mainWindow);
    dialog.setWindowTitle(obs_module_text("MultiDock.Dialog.ManageTitle"));
    dialog.setModal(true);
    dialog.resize(400, 300);
    
    // Set dialog background to BG_DARKEST
    dialog.setStyleSheet(QString("QDialog { background-color: %1; }").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // List widget
    layout->addWidget(new QLabel(obs_module_text("MultiDock.Label.ExistingDocks")));
    QListWidget* listWidget = new QListWidget(&dialog);
    
    // Style the list widget with rounded corners and proper background
    listWidget->setStyleSheet(QString(
        "QListWidget {"
        "    background-color: %1;"
        "    border: 0px solid %2;"
        "    border-radius: %3px;"
        "    padding: 4px;"
        "    color: %4;"
        "}"
        "QListWidget::item {"
        "    padding: 4px 8px;"
        "    border-radius: %5px;"
        "    margin: 1px;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: %6;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: %7;"
        "}")
        .arg(StreamUP::UIStyles::Colors::BG_SECONDARY)
        .arg(StreamUP::UIStyles::Colors::BORDER_SUBTLE)
	.arg(StreamUP::UIStyles::Sizes::RADIUS_DOCK)
        .arg(StreamUP::UIStyles::Colors::TEXT_PRIMARY)
        .arg(StreamUP::UIStyles::Sizes::RADIUS_LG)
        .arg(StreamUP::UIStyles::Colors::PRIMARY_COLOR)
	.arg(StreamUP::UIStyles::Colors::PRIMARY_HOVER));
    
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
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* newButton = new QPushButton(&dialog);
    newButton->setProperty("class", "icon-plus");
    newButton->setToolTip(obs_module_text("MultiDock.Button.New"));
    newButton->setStyleSheet(StreamUP::UIStyles::GetSquircleButtonStyle("", "", 28));
    newButton->setFixedSize(28, 28);
    QPushButton* openButton = new QPushButton(obs_module_text("UI.Button.Open"), &dialog);
    QPushButton* renameButton = new QPushButton(obs_module_text("MultiDock.Button.Rename"), &dialog);
    QPushButton* deleteButton = new QPushButton(&dialog);
    deleteButton->setProperty("class", "icon-trash");
    deleteButton->setToolTip(obs_module_text("MultiDock.Button.Delete"));
    deleteButton->setStyleSheet(StreamUP::UIStyles::GetSquircleButtonStyle("", "", 28));
    deleteButton->setFixedSize(28, 28);
    QPushButton* closeButton = new QPushButton(obs_module_text("UI.Button.Close"), &dialog);
    
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
    layout->addLayout(buttonLayout);
    
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
    
    QObject::connect(deleteButton, &QPushButton::clicked, [listWidget, manager, &dialog, openButton, renameButton, deleteButton]() {
        QListWidgetItem* item = listWidget->currentItem();
        if (!item || !manager) {
            return;
        }
        
        QString id = item->data(Qt::UserRole).toString();
        QString name = item->text();
        if (id.isEmpty()) {
            return;
        }
        
        // Create a separate dialog for confirmation to avoid any lifecycle issues
        QMessageBox confirmDialog(&dialog);
        confirmDialog.setWindowTitle(obs_module_text("MultiDock.Dialog.DeleteTitle"));
        confirmDialog.setText(QString(obs_module_text("MultiDock.Confirm.Delete")).arg(name));
        confirmDialog.setInformativeText(obs_module_text("MultiDock.Info.DeleteRestore"));
        confirmDialog.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        confirmDialog.setDefaultButton(QMessageBox::No);
        confirmDialog.setIcon(QMessageBox::Question);
        
        int result = confirmDialog.exec();
        if (result != QMessageBox::Yes) {
            return;
        }
        
        // Simple immediate deletion
        StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "Attempting to remove MultiDock '%s'", name.toUtf8().constData());
        
        bool success = manager->RemoveMultiDock(id);
        if (success) {
            // Remove from the UI list
            int row = listWidget->row(item);
            if (row >= 0) {
                delete listWidget->takeItem(row);
            }
            StreamUP::DebugLogger::LogDebug("MultiDock", "Dialog", "Successfully removed MultiDock from UI");
        } else {
            StreamUP::DebugLogger::LogWarning("MultiDock", "Dialog: Failed to remove MultiDock");
            QMessageBox::warning(&dialog, obs_module_text("StreamUP.MultiDock.Error"), obs_module_text("StreamUP.MultiDock.FailedToDelete"));
        }
    });
    
    QObject::connect(newButton, &QPushButton::clicked, [listWidget, manager, &dialog]() {
        // Show the new MultiDock dialog
        QDialog newDialog(&dialog);
        newDialog.setWindowTitle(obs_module_text("MultiDock.Dialog.NewTitle"));
        newDialog.setModal(true);
        newDialog.resize(300, 120);
        
        // Set dialog background to BG_DARKEST
        newDialog.setStyleSheet(QString("QDialog { background-color: %1; }").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        
        QVBoxLayout* newLayout = new QVBoxLayout(&newDialog);
        
        // Name input
        newLayout->addWidget(new QLabel(obs_module_text("MultiDock.Label.Name")));
        QLineEdit* nameEdit = new QLineEdit(&newDialog);
        nameEdit->setPlaceholderText(obs_module_text("MultiDock.Placeholder.Name"));
        
        // Style the input box
        nameEdit->setStyleSheet(StreamUP::UIStyles::GetLineEditStyle());
        
        newLayout->addWidget(nameEdit);
        
        // Buttons
        QHBoxLayout* newButtonLayout = new QHBoxLayout();
        QPushButton* createButton = new QPushButton(obs_module_text("MultiDock.Button.Create"), &newDialog);
        QPushButton* cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), &newDialog);
        
        // Style buttons
        createButton->setStyleSheet(StreamUP::UIStyles::GetButtonStyle());
        cancelButton->setStyleSheet(StreamUP::UIStyles::GetButtonStyle());
        
        newButtonLayout->addStretch();
        newButtonLayout->addWidget(createButton);
        newButtonLayout->addWidget(cancelButton);
        newLayout->addLayout(newButtonLayout);
        
        // Connect signals
        QObject::connect(cancelButton, &QPushButton::clicked, &newDialog, &QDialog::reject);
        QObject::connect(createButton, &QPushButton::clicked, [&newDialog, nameEdit, manager, listWidget]() {
            QString name = nameEdit->text().trimmed();
            if (name.isEmpty()) {
                QMessageBox::warning(&newDialog, obs_module_text("Plugin.Error.InvalidName"), obs_module_text("MultiDock.Error.InvalidName"));
                return;
            }
            
            if (!manager) {
                QMessageBox::critical(&newDialog, obs_module_text("Plugin.Error.Title"), obs_module_text("MultiDock.Error.SystemNotInitialized"));
                return;
            }
            
            QString id = manager->CreateMultiDock(name);
            if (id.isEmpty()) {
                QMessageBox::critical(&newDialog, obs_module_text("Plugin.Error.Title"), obs_module_text("MultiDock.Error.CreationFailed"));
                return;
            }
            
            // Add the new MultiDock to the list
            QListWidgetItem* item = new QListWidgetItem(name);
            item->setData(Qt::UserRole, id);
            
            // Remove placeholder text if it exists
            if (listWidget->count() == 1) {
                QListWidgetItem* firstItem = listWidget->item(0);
                if (firstItem && firstItem->data(Qt::UserRole).toString().isEmpty()) {
                    delete listWidget->takeItem(0);
                }
            }
            
            listWidget->addItem(item);
            listWidget->setCurrentItem(item);
            
            StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "Created MultiDock: '%s' with ID '%s'",
                 name.toUtf8().constData(), id.toUtf8().constData());
            
            newDialog.accept();
        });
        
        // Make create button default
        createButton->setDefault(true);
        nameEdit->setFocus();
        
        newDialog.exec();
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
        
        // Show rename dialog
        QDialog renameDialog(&dialog);
        renameDialog.setWindowTitle(obs_module_text("MultiDock.Dialog.RenameTitle"));
        renameDialog.setModal(true);
        renameDialog.resize(300, 120);
        
        // Set dialog background to BG_DARKEST
        renameDialog.setStyleSheet(QString("QDialog { background-color: %1; }").arg(StreamUP::UIStyles::Colors::BG_DARKEST));
        
        QVBoxLayout* renameLayout = new QVBoxLayout(&renameDialog);
        
        // Name input
        renameLayout->addWidget(new QLabel(obs_module_text("MultiDock.Label.NewName")));
        QLineEdit* nameEdit = new QLineEdit(&renameDialog);
        nameEdit->setText(currentName);
        nameEdit->selectAll();
        
        // Style the input box
        nameEdit->setStyleSheet(StreamUP::UIStyles::GetLineEditStyle());
        
        renameLayout->addWidget(nameEdit);
        
        // Buttons
        QHBoxLayout* renameButtonLayout = new QHBoxLayout();
        QPushButton* saveButton = new QPushButton(obs_module_text("UI.Button.Save"), &renameDialog);
        QPushButton* cancelButton = new QPushButton(obs_module_text("UI.Button.Cancel"), &renameDialog);
        
        // Style buttons
        saveButton->setStyleSheet(StreamUP::UIStyles::GetButtonStyle());
        cancelButton->setStyleSheet(StreamUP::UIStyles::GetButtonStyle());
        
        renameButtonLayout->addStretch();
        renameButtonLayout->addWidget(saveButton);
        renameButtonLayout->addWidget(cancelButton);
        renameLayout->addLayout(renameButtonLayout);
        
        // Connect signals
        QObject::connect(cancelButton, &QPushButton::clicked, &renameDialog, &QDialog::reject);
        QObject::connect(saveButton, &QPushButton::clicked, [&renameDialog, nameEdit, manager, item, id, currentName]() {
            QString newName = nameEdit->text().trimmed();
            if (newName.isEmpty()) {
                QMessageBox::warning(&renameDialog, obs_module_text("Plugin.Error.InvalidName"), obs_module_text("MultiDock.Error.InvalidName"));
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
                QMessageBox::warning(&renameDialog, obs_module_text("StreamUP.MultiDock.Error"), obs_module_text("StreamUP.MultiDock.FailedToRename"));
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
