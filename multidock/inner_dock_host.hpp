#ifndef STREAMUP_MULTIDOCK_INNER_DOCK_HOST_HPP
#define STREAMUP_MULTIDOCK_INNER_DOCK_HOST_HPP

#include "multidock_utils.hpp"
#include <QMainWindow>
#include <QToolBar>
#include <QAction>
#include <QTimer>
#include <QHash>

class QLabel;

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
     * @brief Get the currently selected/focused dock
     * @return Currently focused dock, or nullptr if none
     */
    QDockWidget* GetCurrentDock() const;

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

protected:
    /**
     * @brief Event filter to track dock focus events
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
    /**
     * @brief Show the Add Dock dialog
     */
    void ShowAddDockDialog();

    /**
     * @brief Return the currently selected dock to main window
     */
    void ReturnCurrentDock();

    /**
     * @brief Close (hide) the currently selected dock
     */
    void CloseCurrentDock();

signals:
    /**
     * @brief Emitted when the layout changes and should be saved
     */
    void LayoutChanged();

private slots:
    void OnDockLocationChanged(Qt::DockWidgetArea area);
    void OnTabifiedDockActivated(QDockWidget* dock);
    void OnDockVisibilityChanged(bool visible);
    void TriggerAutoSave();

private:
    void SetupToolBar();
    void SetupDockOptions();
    void ConnectDockSignals(QDockWidget* dock);
    void DisconnectDockSignals(QDockWidget* dock);
    void UpdateToolBarState();

    QString m_multiDockId;
    QToolBar* m_toolBar;
    QAction* m_addDockAction;
    QAction* m_returnDockAction;
    QAction* m_closeDockAction;
    QLabel* m_statusLabel;
    
    QHash<DockId, CapturedDock> m_capturedDocks;
    QPointer<QDockWidget> m_currentDock;
    
    QTimer* m_autoSaveTimer;
    static const int AUTO_SAVE_DELAY_MS = 500;
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_INNER_DOCK_HOST_HPP