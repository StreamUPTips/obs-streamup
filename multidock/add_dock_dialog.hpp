#ifndef STREAMUP_MULTIDOCK_ADD_DOCK_DIALOG_HPP
#define STREAMUP_MULTIDOCK_ADD_DOCK_DIALOG_HPP

#include <QDialog>
#include <QListWidget>
#include <QDockWidget>
#include <QPushButton>

namespace StreamUP {
namespace MultiDock {

/**
 * @brief Dialog for selecting an available dock to add to a MultiDock
 */
class AddDockDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDockDialog(const QString& multiDockId, QWidget* parent = nullptr);

    /**
     * @brief Get the dock selected by the user
     * @return Selected dock widget, or nullptr if none selected
     */
    QDockWidget* GetSelectedDock() const;

private slots:
    void OnSelectionChanged();
    void OnItemDoubleClicked(QListWidgetItem* item);

private:
    void PopulateAvailableDocks();
    bool IsDockAvailable(QDockWidget* dock) const;

    QString m_multiDockId;
    QListWidget* m_dockList;
    QPushButton* m_addButton;
    QPushButton* m_cancelButton;
    
    QList<QDockWidget*> m_availableDocks;
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_ADD_DOCK_DIALOG_HPP
