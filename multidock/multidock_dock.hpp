#ifndef STREAMUP_MULTIDOCK_DOCK_HPP
#define STREAMUP_MULTIDOCK_DOCK_HPP

#include <QDockWidget>

namespace StreamUP {
namespace MultiDock {

class InnerDockHost;

/**
 * @brief The outer QDockWidget container registered with OBS
 * 
 * This is the dock that appears in OBS's View->Docks menu and can be
 * docked alongside other OBS docks. It contains an InnerDockHost as
 * its central widget.
 */
class MultiDockDock : public QDockWidget
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

public slots:
    /**
     * @brief Save the current state to persistent storage
     */
    void SaveState();

    /**
     * @brief Load and restore state from persistent storage
     */
    void LoadState();

private slots:
    void OnLayoutChanged();

private:
    void SetupUi();

    QString m_id;
    QString m_name;
    InnerDockHost* m_innerHost;
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_DOCK_HPP