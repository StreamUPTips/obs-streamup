#ifndef STREAMUP_VERSION_UTILS_HPP
#define STREAMUP_VERSION_UTILS_HPP

#include <string>
#include <vector>

namespace StreamUP {
namespace VersionUtils {

/**
 * Compare two version strings to determine if version1 is less than version2
 * @param version1 First version string (e.g., "1.2.3")
 * @param version2 Second version string (e.g., "1.2.4")
 * @return bool true if version1 < version2
 */
bool IsVersionLessThan(const std::string &version1, const std::string &version2);

/**
 * Parse a version string into its numeric components
 * @param version Version string to parse (e.g., "1.2.3")
 * @return std::vector<int> containing version components
 */
std::vector<int> ParseVersion(const std::string &version);

} // namespace VersionUtils
} // namespace StreamUP

#endif // STREAMUP_VERSION_UTILS_HPP
