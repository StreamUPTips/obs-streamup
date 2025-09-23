#ifndef STREAMUP_STRING_UTILS_HPP
#define STREAMUP_STRING_UTILS_HPP

#include <QString>
#include <QStringList>
#include <string>
#include <vector>

namespace StreamUP {
namespace StringUtils {

/**
 * Split a string by a delimiter
 * @param str The string to split
 * @param delimiter The delimiter to split by
 * @return std::vector<std::string> containing the split parts
 */
std::vector<std::string> SplitString(const std::string &str, const std::string &delimiter);

/**
 * Search for a string pattern in a file
 * @param filePath Path to the file to search
 * @param searchString The string to search for
 * @return bool true if string is found in file
 */
bool SearchStringInFile(const QString &filePath, const QString &searchString);

/**
 * Get platform-specific URL from a set of URLs
 * @param windowsURL URL for Windows platform
 * @param macURL URL for macOS platform  
 * @param linuxURL URL for Linux platform
 * @param generalURL Fallback URL for any platform
 * @return QString containing the appropriate URL for current platform
 */
QString GetPlatformURL(const QString &windowsURL, const QString &macURL, 
                      const QString &linuxURL, const QString &generalURL);

/**
 * Convert QString to UTF-8 std::string
 * @param qstr The QString to convert
 * @return std::string UTF-8 encoded string
 */
std::string QStringToStdString(const QString &qstr);

/**
 * Convert std::string to QString
 * @param str The std::string to convert
 * @return QString converted string
 */
QString StdStringToQString(const std::string &str);

} // namespace StringUtils
} // namespace StreamUP

#endif // STREAMUP_STRING_UTILS_HPP
