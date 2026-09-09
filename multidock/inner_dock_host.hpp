#ifndef STREAMUP_MULTIDOCK_INNER_DOCK_HOST_HPP
#define STREAMUP_MULTIDOCK_INNER_DOCK_HOST_HPP

#include "multidock_utils.hpp"
#include <QMainWindow>
#include <QHash>
#include <QMap>

namespace StreamUP {
namespace MultiDock {

/**
 * @brief QMainWindow subclass that hosts captured docks inside a MultiDock
 * 
 * This class provides the inner docking area where captured docks are placed.
 * It supports native Qt docking behavior (tabs, splits, etc.) and persists
 * the layout automatically.
 */
class InnerDockHost : public QMainWindow
{
    Q_OBJECT

public:
    explicit InnerDockHost(const QString& multiDockId, QWidget* parent = nullptr);
    ~InnerDockHost();

    /**
     * @brief Add a dock widget to this host
     * @param dock The dock widget to add
     * @param area The preferred docking area
     */
    void AddDock(QDockWidget* dock, Qt::DockWidgetArea area = Qt::RightDockWidgetArea);

    /**
     * @brief Remove a dock widget from this host
     * @param dock The dock widget to remove
     */
    void RemoveDock(QDockWidget* dock);


    /**
     * @brief Get all captured docks
     * @return List of all captured dock widgets
     */
    QList<QDockWidget*> GetAllDocks() const;

    /**
     * @brief Hand every captured dock back to the main window it came from
     *
     * A captured dock is reparented into this host, but OBS still owns it: it
     * keeps a shared_ptr to every dock a plugin registered, and deletes them
     * all from ~OBSBasic. If a dock is still a child of this host when the
     * host dies, Qt deletes it as a child and OBS then deletes it a second
     * time - a pure virtual call on a freed QObject, which aborts the process
     * on the way out. Releasing them first is what keeps the two owners from
     * colliding.
     */
    void ReleaseAllDocks();

    /**
     * @brief Restore the layout from saved state
     * @param layout The layout data to restore
     */
    void RestoreLayout(const QByteArray& layout);

    /**
     * @brief Get the current layout state
     * @return Current layout as QByteArray
     */
    QByteArray SaveLayout() const;

    /**
     * @brief Get the list of captured dock IDs
     * @return List of dock IDs currently captured
     */
    QStringList GetCapturedDockIds() const;


public slots:
    /**
     * @brief Show the Add Dock dialog
     */
    void ShowAddDockDialog();


    /**
     * @brief Update the toolbar state (make public for restoration)
     */
    void UpdateToolBarState();
    
    /**
     * @brief Reapply dock features to all captured docks
     */
    void ReapplyDockFeatures();
    
    /**
     * @brief Set whether docks are locked (prevents moving/resizing)
     * @param locked True to lock docks, false to unlock
     */
    void SetDocksLocked(bool locked);

    // No signals needed - we save on OBS shutdown

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:

private:
    void SetupDockOptions();

        /**
     * @brief Lift the layout size constraint on a captured dock's content
     *
     * A dock cannot be dragged below the minimum its content's layout imposes.
     * Lifting it lets the layout squeeze rather than the dock refuse, so a wide
     * dock (the Vertical Canvas) stops setting the floor for the whole
     * MultiDock. Records what was there so it can be put back exactly.
     * @param captured The capture record to relax (modified in place)
     */
    void RelaxContentConstraints(CapturedDock& captured);

    /**
     * @brief Undo RelaxContentConstraints before a dock goes back to OBS
     * @param captured The capture record to restore
     */
    void RestoreContentConstraints(const CapturedDock& captured);

    void ConnectDockSignals(QDockWidget* dock);
    void DisconnectDockSignals(QDockWidget* dock);
    void applyDockFeatures(bool locked);

    QString m_multiDockId;

    QHash<DockId, CapturedDock> m_capturedDocks;
    QMap<QDockWidget*, QMetaObject::Connection> m_visibilityConnections;
    bool m_docksLocked;
    
    // Removed timers - we save on OBS shutdown instead
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_INNER_DOCK_HOST_HPP
