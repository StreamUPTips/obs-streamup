#ifndef STREAMUP_MULTIDOCK_PERSISTENCE_HPP
#define STREAMUP_MULTIDOCK_PERSISTENCE_HPP

#include <QString>
#include <QStringList>
#include <QByteArray>

namespace StreamUP {
namespace MultiDock {

struct MultiDockInfo {
    QString id;
    QString name;
};

/**
 * @brief Load the list of MultiDocks from persistent storage
 * @return List of MultiDock information
 */
QList<MultiDockInfo> LoadMultiDockList();

/**
 * @brief Save the list of MultiDocks to persistent storage
 * @param multiDocks List of MultiDock information to save
 */
void SaveMultiDockList(const QList<MultiDockInfo>& multiDocks);

/**
 * @brief Load the state of a specific MultiDock
 * @param id The MultiDock ID
 * @param capturedDocks Output list of captured dock IDs
 * @param layout Output layout data as Base64 encoded QByteArray
 * @return True if state was loaded successfully
 */
bool LoadMultiDockState(const QString& id, QStringList& capturedDocks, QByteArray& layout);

/**
 * @brief Save the state of a specific MultiDock
 * @param id The MultiDock ID
 * @param capturedDocks List of captured dock IDs
 * @param layout Layout data as QByteArray
 */
void SaveMultiDockState(const QString& id, const QStringList& capturedDocks, const QByteArray& layout);

/**
 * @brief Remove a MultiDock from persistent storage
 * @param id The MultiDock ID to remove
 */
void RemoveMultiDockState(const QString& id);

} // namespace MultiDock
} // namespace StreamUP

#endif // STREAMUP_MULTIDOCK_PERSISTENCE_HPP