#include "add_dock_dialog.hpp"
#include "../utilities/debug-logger.hpp"
#include "multidock_utils.hpp"
#include "../ui/ui-styles.hpp"
#include <obs-module.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <algorithm>

namespace StreamUP {
namespace MultiDock {

AddDockDialog::AddDockDialog(const QString& multiDockId, QWidget* parent)
    : QDialog(parent)
    , m_multiDockId(multiDockId)
    , m_dockList(nullptr)
    , m_addButton(nullptr)
    , m_cancelButton(nullptr)
{
    setWindowTitle(obs_module_text("MultiDock.Dialog.AddDockTitle"));
    setModal(true);
    resize(400, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Header
    layout->addWidget(new QLabel(obs_module_text("MultiDock.Label.SelectDock")));
    
    // Dock list
    m_dockList = new QListWidget(this);
    m_dockList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dockList->setStyleSheet(StreamUP::UIStyles::GetListWidgetStyle());
    layout->addWidget(m_dockList);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_addButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("MultiDock.Button.Add"), "info");
    m_cancelButton = StreamUP::UIStyles::CreateStyledButton(obs_module_text("UI.Button.Cancel"), "neutral");
    
    m_addButton->setEnabled(false);
    m_addButton->setDefault(true);
    
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_cancelButton);
    layout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_dockList, &QListWidget::itemSelectionChanged, this, &AddDockDialog::OnSelectionChanged);
    connect(m_dockList, &QListWidget::itemDoubleClicked, this, &AddDockDialog::OnItemDoubleClicked);
    connect(m_addButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    PopulateAvailableDocks();
}

QDockWidget* AddDockDialog::GetSelectedDock() const
{
    QListWidgetItem* current = m_dockList->currentItem();
    if (!current) {
        return nullptr;
    }
    
    int index = m_dockList->row(current);
    if (index >= 0 && index < m_availableDocks.size()) {
        return m_availableDocks[index];
    }
    
    return nullptr;
}

void AddDockDialog::OnSelectionChanged()
{
    m_addButton->setEnabled(m_dockList->currentItem() != nullptr);
}

void AddDockDialog::OnItemDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item)
    if (m_addButton->isEnabled()) {
        accept();
    }
}

void AddDockDialog::PopulateAvailableDocks()
{
    m_dockList->clear();
    m_availableDocks.clear();
    
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        m_dockList->addItem(obs_module_text("MultiDock.Message.UnableToAccessMainWindow"));
        return;
    }
    
    QList<QDockWidget*> allDocks = FindAllObsDocks(mainWindow);
    
    // Collect available docks with their display names
    QList<QPair<QString, QDockWidget*>> docksWithNames;
    
    for (QDockWidget* dock : allDocks) {
        if (IsDockAvailable(dock)) {
            QString displayName = dock->windowTitle();
            
            if (displayName.isEmpty()) {
                displayName = obs_module_text("MultiDock.Message.UnnamedDock");
            }
            
            // Only show the dock name, not the identifier
            docksWithNames.append({displayName, dock});
        }
    }
    
    // Sort alphabetically by display name
    std::sort(docksWithNames.begin(), docksWithNames.end(), 
              [](const QPair<QString, QDockWidget*>& a, const QPair<QString, QDockWidget*>& b) {
                  return a.first.toLower() < b.first.toLower();
              });
    
    // Add to list and available docks array
    for (const auto& pair : docksWithNames) {
        m_availableDocks.append(pair.second);
        m_dockList->addItem(pair.first);
    }
    
    if (m_availableDocks.isEmpty()) {
        m_dockList->addItem(obs_module_text("MultiDock.Message.NoAvailableDocksFound"));
    }
    
    StreamUP::DebugLogger::LogDebugFormat("MultiDock", "Dialog", "AddDockDialog: Found %d available docks",
         m_availableDocks.size());
}

bool AddDockDialog::IsDockAvailable(QDockWidget* dock) const
{
    if (!dock) {
        return false;
    }
    
    // Don't allow capturing MultiDock containers
    if (IsMultiDockContainer(dock)) {
        return false;
    }
    
    // Check if dock is already captured by any MultiDock
    // For MVP, we'll do a simple check by seeing if the dock's parent is not the main window
    QMainWindow* mainWindow = GetObsMainWindow();
    if (!mainWindow) {
        return false;
    }
    
    // If the dock's parent is not the main window, it's likely captured elsewhere
    QWidget* dockParent = dock->parentWidget();
    while (dockParent && dockParent != mainWindow) {
        // Check if parent is a MultiDock InnerDockHost by checking for the class name
        if (qobject_cast<QMainWindow*>(dockParent) && dockParent != mainWindow) {
            // If it's a QMainWindow that's not the main OBS window, likely a MultiDock host
            QString className = dockParent->metaObject()->className();
            if (className.contains("InnerDockHost")) {
                return false; // Captured by a MultiDock
            }
        }
        dockParent = dockParent->parentWidget();
    }
    
    return dockParent == mainWindow;
}

} // namespace MultiDock
} // namespace StreamUP

#include "add_dock_dialog.moc"