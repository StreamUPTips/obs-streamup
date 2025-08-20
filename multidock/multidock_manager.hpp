#ifndef STREAMUP_MULTIDOCK_MANAGER_HPP
#define STREAMUP_MULTIDOCK_MANAGER_HPP

#include "persistence.hpp"
#include <QObject>
#include <QHash>

namespace StreamUP {
namespace MultiDock {

class MultiDockDock;

/**
 * @brief Central manager for all MultiDock instances
 * 
 * Handles creation, deletion, and lifecycle management of MultiDocks.
 * Integrates with OBS Frontend API to register docks properly.
 */
class MultiDockManager : public QObject
{
    Q_OBJECT

public:
    static MultiDockManager* Instance();
    static void Initialize();
    static void Shutdown();

    /**
     * @brief Create a new MultiDock
     * @param name Display name for the MultiDock
     * @return Unique ID of the created MultiDock, or empty string on failure
     */
    QString CreateMultiDock(const QString& name);

    /**
     * @brief Remove a MultiDock
     * @param id ID of the MultiDock to remove
     * @return True if removed successfully
     */
    bool RemoveMultiDock(const QString& id);

    /**
     * @brief Rename a MultiDock
     * @param id ID of the MultiDock to rename
     * @param newName New name for the MultiDock
     * @return True if renamed successfully
     */
    bool RenameMultiDock(const QString& id, const QString& newName);

    /**
     * @brief Get a MultiDock by ID
     * @param id ID of the MultiDock
     * @return Pointer to MultiDock, or nullptr if not found
     */
    MultiDockDock* GetMultiDock(const QString& id) const;

    /**
     * @brief Get all MultiDocks
     * @return List of all MultiDock instances
     */
    QList<MultiDockDock*> GetAllMultiDocks() const;

    /**
     * @brief Get MultiDock information list
     * @return List of MultiDock info (ID and name)
     */
    QList<MultiDockInfo> GetMultiDockInfoList() const;

    /**
     * @brief Load all MultiDocks from persistent storage
     */
    void LoadAllMultiDocks();

    /**
     * @brief Save all MultiDocks to persistent storage
     */
    void SaveAllMultiDocks();

private slots:
    void OnMultiDockDestroyed(QObject* obj);

private:
    explicit MultiDockManager(QObject* parent = nullptr);
    ~MultiDockManager();

    QString GenerateUniqueId() const;
    void RegisterWithObs(MultiDockDock* multiDock);
    void UnregisterFromObs(MultiDockDock* multiDock);

    QHash<QString, MultiDockDock*> m_multiDocks;
    
    static MultiDockManager* s_instance;
};

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_MANAGER_HPP