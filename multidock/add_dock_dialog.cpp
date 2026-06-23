#include "add_dock_dialog.hpp"
#include "inner_dock_host.hpp"
#include "../utilities/debug-logger.hpp"
#include "multidock_utils.hpp"
#include <streamup/ui/window-chrome.hpp>
#include <streamup/ui/pill-button.hpp>
#include <streamup/ui/labels.hpp>
#include <streamup/ui/ui-scrollbar.hpp>
#include "version.h"
#include <obs-module.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <algorithm>

using namespace StreamUP::UIStyles;

namespace StreamUP {
namespace MultiDock {

AddDockDialog::AddDockDialog(const QString& multiDockId, QWidget* parent)
    : UIStyles::ShadowDialog(parent)
    , m_multiDockId(multiDockId)
    , m_dockList(nullptr)
    , m_addButton(nullptr)
    , m_cancelButton(nullptr)
{
    // brandFooter=false → secondary window (no brand line), "StreamUP".
    WindowShell chrome = applyChrome(this, obs_module_text("MultiDock.Dialog.AddDockTitle"), "v" PROJECT_VERSION,
                                     /*brandFooter=*/false, "StreamUP");
    chrome.content->setContentsMargins(S(20), S(16), S(20), S(16));
    setModal(true);
    resize(S(400) + 2 * S(ShadowDialog::kShadowMargin), S(300) + 2 * S(ShadowDialog::kShadowMargin));

    QVBoxLayout* layout = chrome.content;

    // Header label
    layout->addWidget(new QLabel(obs_module_text("MultiDock.Label.SelectDock")));

    // Dock list
    m_dockList = new QListWidget(this);
    m_dockList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dockList->setStyleSheet(listStyle());
    useScrollBars(m_dockList);
    layout->addWidget(m_dockList);

    // Action buttons go in the footer's right-anchored slot (Cancel outline left,
    // primary Add right).
    auto* addButton = new PillButton(obs_module_text("MultiDock.Button.Add"), "primary");
    auto* cancelButton = new PillButton(obs_module_text("UI.Button.Cancel"), "outline");
    m_addButton = addButton;
    m_cancelButton = cancelButton;

    m_addButton->setEnabled(false);
    m_addButton->setDefault(true);

    chrome.footerButtons->addWidget(m_cancelButton);
    chrome.footerButtons->addWidget(m_addButton);

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
    if (index >= 0 && static_cast<size_t>(index) < static_cast<size_t>(m_availableDocks.size())) {
        QDockWidget *dock = m_availableDocks[index];
        if (dock) return dock; // QPointer auto-nulls if deleted
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
        // Check if parent is a MultiDock InnerDockHost using type-safe cast
        if (qobject_cast<InnerDockHost*>(dockParent)) {
            return false; // Already captured in a MultiDock
        }
        dockParent = dockParent->parentWidget();
    }
    
    return dockParent == mainWindow;
}

} // namespace MultiDock
} // namespace StreamUP

#include "add_dock_dialog.moc"
