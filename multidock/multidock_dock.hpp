#ifndef STREAMUP_MULTIDOCK_DOCK_HPP
#define STREAMUP_MULTIDOCK_DOCK_HPP

#include <QFrame>

class QVBoxLayout;
class QToolBar;
class QAction;
class QLabel;
class QCheckBox;

namespace StreamUP {
namespace MultiDock {

class InnerDockHost;

/**
 * @brief The main MultiDock widget registered with OBS
 * 
 * This is the widget that appears in OBS's View->Docks menu and can be
 * docked alongside other OBS docks. It contains an InnerDockHost that
 * provides the inner docking functionality.
 */
class MultiDockDock : public QFrame
{
    Q_OBJECT

public:
    explicit MultiDockDock(const QString& id, const QString& name, QWidget* parent = nullptr);
    ~MultiDockDock();

    /**
     * @brief Get the MultiDock ID
     * @return Unique identifier for this MultiDock
     */
    QString GetId() const { return m_id; }

    /**
     * @brief Get the MultiDock name
     * @return Display name for this MultiDock
     */
    QString GetName() const { return m_name; }

    /**
     * @brief Get the inner dock host
     * @return Pointer to the inner docking area
     */
    InnerDockHost* GetInnerHost() const { return m_innerHost; }

    /**
     * @brief Set the MultiDock name and update display
     * @param name New name for the MultiDock
     */
    void SetName(const QString& name);

    /**
     * @brief Save the current state to persistent storage
     */
    void SaveState();

    /**
     * @brief Load and restore state from persistent storage
     */
    void LoadState();

    /**
     * @brief Update the toolbar state based on current docks
     */
    void UpdateToolbarState();

    // No slots needed - we save on OBS shutdown

private:
    void SetupUi();
    void CreateBottomToolbar(QVBoxLayout* layout);

    QString m_id;
    QString m_name;
    InnerDockHost* m_innerHost;
    
    // Toolbar references for status updates
    QLabel* m_statusLabel;
    QAction* m_addDockAction;
    QAction* m_returnDockAction;
    QAction* m_closeDockAction;
    QCheckBox* m_lockCheckbox;
    bool m_docksLocked;
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_DOCK_HPP