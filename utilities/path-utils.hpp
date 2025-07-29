#ifndef STREAMUP_PATH_UTILS_HPP
#define STREAMUP_PATH_UTILS_HPP

#include <QString>
#include <string>

namespace StreamUP {
namespace PathUtils {

/**
 * Get the local application data path for the current platform
 * @return QString containing the local app data path
 */
QString GetLocalAppDataPath();

/**
 * Get the most recent file from a directory matching a pattern
 * @param directoryPath Path to search in
 * @param filePattern Pattern to match files against
 * @return QString containing the most recent file path, or empty if none found
 */
QString GetMostRecentFile(const QString &directoryPath, const QString &filePattern = "*");

/**
 * Check if a file or directory exists
 * @param path Path to check
 * @return bool true if path exists
 */
bool PathExists(const QString &path);

/**
 * Convert a relative path to absolute path
 * @param relativePath The relative path to convert
 * @return QString containing the absolute path
 */
QString ToAbsolutePath(const QString &relativePath);

} // namespace PathUtils
} // namespace StreamUP

#endif // STREAMUP_PATH_UTILS_HPP