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
