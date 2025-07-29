#include "version-utils.hpp"
#include "string-utils.hpp"
#include <algorithm>

namespace StreamUP {
namespace VersionUtils {

std::vector<int> ParseVersion(const std::string &version)
{
	std::vector<int> parts;
	std::vector<std::string> stringParts = StringUtils::SplitString(version, ".");
	
	for (const auto &part : stringParts) {
		try {
			parts.push_back(std::stoi(part));
		} catch (const std::exception &) {
			parts.push_back(0); // Default to 0 for invalid numbers
		}
	}
	
	return parts;
}

bool IsVersionLessThan(const std::string &version1, const std::string &version2)
{
	std::vector<int> parts1 = ParseVersion(version1);
	std::vector<int> parts2 = ParseVersion(version2);

	// Pad shorter version with zeros
	while (parts1.size() < parts2.size()) {
		parts1.push_back(0);
	}
	while (parts2.size() < parts1.size()) {
		parts2.push_back(0);
	}

	for (size_t i = 0; i < parts1.size(); ++i) {
		if (parts1[i] < parts2[i]) {
			return true;
		} else if (parts1[i] > parts2[i]) {
			return false;
		}
	}

	return false; // Versions are equal
}

bool AreVersionsEqual(const std::string &version1, const std::string &version2)
{
	return !IsVersionLessThan(version1, version2) && !IsVersionLessThan(version2, version1);
}

std::string GetNewerVersion(const std::string &version1, const std::string &version2)
{
	if (IsVersionLessThan(version1, version2)) {
		return version2;
	} else {
		return version1;
	}
}

} // namespace VersionUtils
} // namespace StreamUP