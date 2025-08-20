#include "multidock_dialogs.hpp"
#include "multidock_utils.hpp"
#include "multidock_manager.hpp"
#include "multidock_dock.hpp"
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
    dialog.setWindowTitle("New MultiDock");
    dialog.setModal(true);
    dialog.resize(300, 120);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // Name input
    layout->addWidget(new QLabel("MultiDock Name:"));
    QLineEdit* nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText("Enter MultiDock name...");
    layout->addWidget(nameEdit);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* createButton = new QPushButton("Create", &dialog);
    QPushButton* cancelButton = new QPushButton("Cancel", &dialog);
    
    buttonLayout->addWidget(createButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // Connect signals
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(createButton, &QPushButton::clicked, [&dialog, nameEdit]() {
        QString name = nameEdit->text().trimmed();
        if (name.isEmpty()) {
            QMessageBox::warning(&dialog, "Invalid Name", "Please enter a valid MultiDock name.");
            return;
        }
        
        // Create the MultiDock using the manager
        MultiDockManager* manager = MultiDockManager::Instance();
        if (!manager) {
            QMessageBox::critical(&dialog, "Error", "MultiDock system not initialized.");
            return;
        }
        
        QString id = manager->CreateMultiDock(name);
        if (id.isEmpty()) {
            QMessageBox::critical(&dialog, "Error", "Failed to create MultiDock.");
            return;
        }
        
        blog(LOG_INFO, "[StreamUP MultiDock] Created MultiDock: '%s' with ID '%s'", 
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
    dialog.setWindowTitle("Manage MultiDocks");
    dialog.setModal(true);
    dialog.resize(400, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    // List widget
    layout->addWidget(new QLabel("Existing MultiDocks:"));
    QListWidget* listWidget = new QListWidget(&dialog);
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
            listWidget->addItem("(No MultiDocks created yet)");
        }
    } else {
        listWidget->addItem("(MultiDock system not initialized)");
    }
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* openButton = new QPushButton("Open", &dialog);
    QPushButton* renameButton = new QPushButton("Rename", &dialog);
    QPushButton* deleteButton = new QPushButton("Delete", &dialog);
    QPushButton* closeButton = new QPushButton("Close", &dialog);
    
    // Enable/disable buttons based on selection
    bool hasSelection = listWidget->currentItem() != nullptr && 
                       !listWidget->currentItem()->data(Qt::UserRole).toString().isEmpty();
    openButton->setEnabled(hasSelection);
    renameButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(renameButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
    
    // Connect signals
    QObject::connect(listWidget, &QListWidget::itemSelectionChanged, [&]() {
        bool hasValidSelection = listWidget->currentItem() != nullptr && 
                               !listWidget->currentItem()->data(Qt::UserRole).toString().isEmpty();
        openButton->setEnabled(hasValidSelection);
        renameButton->setEnabled(hasValidSelection);
        deleteButton->setEnabled(hasValidSelection);
    });
    
    QObject::connect(openButton, &QPushButton::clicked, [&]() {
        QListWidgetItem* item = listWidget->currentItem();
        if (item) {
            QString id = item->data(Qt::UserRole).toString();
            if (!id.isEmpty() && manager) {
                MultiDockDock* multiDock = manager->GetMultiDock(id);
                if (multiDock) {
                    multiDock->show();
                    multiDock->raise();
                    multiDock->activateWindow();
                    dialog.accept();
                }
            }
        }
    });
    
    QObject::connect(deleteButton, &QPushButton::clicked, [&]() {
        QListWidgetItem* item = listWidget->currentItem();
        if (item && manager) {
            QString id = item->data(Qt::UserRole).toString();
            QString name = item->text();
            if (!id.isEmpty()) {
                int result = QMessageBox::question(&dialog, "Delete MultiDock", 
                    QString("Are you sure you want to delete the MultiDock '%1'?\n\nThis will return all captured docks to the main window.").arg(name),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                
                if (result == QMessageBox::Yes) {
                    if (manager->RemoveMultiDock(id)) {
                        delete item;
                    }
                }
            }
        }
    });
    
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    
    blog(LOG_INFO, "[StreamUP MultiDock] Showing manage dialog");
    
    dialog.exec();
}

} // namespace MultiDock
} // namespace StreamUP