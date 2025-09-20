#include "path-utils.hpp"
#include "debug-logger.hpp"
#include <filesystem>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <cstdlib>
#include <cstring>
#include <obs-module.h>
#include <util/platform.h>

#ifdef _WIN32
#include <windows.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

namespace StreamUP {
namespace PathUtils {

QString GetLocalAppDataPath()
{
#ifdef _WIN32
	char *buf = nullptr;
	size_t sz = 0;
	if (_dupenv_s(&buf, &sz, "LOCALAPPDATA") == 0 && buf != nullptr) {
		QString path = QString::fromLocal8Bit(buf);
		free(buf);
		return path;
	}
	return QString();
#else
	return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
}

QString GetMostRecentFile(const QString &directoryPath, const QString &filePattern)
{
	QDir dir(directoryPath);
	if (!dir.exists()) {
		return QString();
	}

	QStringList nameFilters;
	if (filePattern.isEmpty()) {
		nameFilters << "*.txt"; // Default to .txt files like original implementation
	} else {
		nameFilters << filePattern;
	}

	QFileInfoList fileList = dir.entryInfoList(nameFilters, QDir::Files, QDir::Time);
	
	if (!fileList.isEmpty()) {
		return fileList.first().absoluteFilePath();
	}
	
	return QString();
}

bool PathExists(const QString &path)
{
	return QFileInfo::exists(path);
}

QString ToAbsolutePath(const QString &relativePath)
{
	QFileInfo fileInfo(relativePath);
	return fileInfo.absoluteFilePath();
}

std::string GetMostRecentTxtFile(const std::string &directoryPath)
{
	auto it = std::filesystem::directory_iterator(directoryPath);
	auto last_write_time = std::filesystem::file_time_type::min();
	std::filesystem::directory_entry newest_file;

	for (const auto &entry : it) {
		if (entry.is_regular_file() && entry.path().extension() == ".txt") {
			auto current_write_time = entry.last_write_time();
			if (current_write_time > last_write_time) {
				last_write_time = current_write_time;
				newest_file = entry;
			}
		}
	}

	return newest_file.path().string();
}

char* GetOBSLogPath()
{
	char *path = nullptr;
	char *path_abs = nullptr;

#ifdef _WIN32
	constexpr const char* STREAMUP_PLATFORM_NAME = "windows";
#else
	constexpr const char* STREAMUP_PLATFORM_NAME = "other";
#endif

	if (strcmp(STREAMUP_PLATFORM_NAME, "windows") == 0) {
		path = obs_module_config_path("../../logs/");
		path_abs = os_get_abs_path_ptr(path);

		if (path_abs && (path_abs[strlen(path_abs) - 1] != '/' && path_abs[strlen(path_abs) - 1] != '\\')) {
			// Create a new string with appended "/"
			size_t new_path_abs_size = strlen(path_abs) + 2;
			char *newPathAbs = (char *)bmalloc(new_path_abs_size);
			strcpy(newPathAbs, path_abs);
			strcat(newPathAbs, "/");

			// Free the old path_abs and reassign it
			bfree(path_abs);
			path_abs = newPathAbs;
		}
	} else {
		path = obs_module_config_path("");

		std::string path_str(path);
		std::string to_search = "/plugin_config/streamup/";
		std::string replace_str = "/logs/";

		size_t pos = path_str.find(to_search);

		// If found then replace it
		if (pos != std::string::npos) {
			path_str.replace(pos, to_search.size(), replace_str);
		}

		size_t path_abs_size = path_str.size() + 1;
		path_abs = (char *)bmalloc(path_abs_size);
		std::strcpy(path_abs, path_str.c_str());
	}

	if (path) {
		bfree(path);
	}

	if (path_abs) {
		StreamUP::DebugLogger::LogDebugFormat("PathUtils", "Log Path Discovery", "Path: %s", path_abs);

		// Use std::filesystem to check if the path exists
		std::string path_abs_str(path_abs);
		bool path_exists = std::filesystem::exists(path_abs_str);
		if (path_exists) {
			std::filesystem::directory_iterator dir(path_abs_str);
			if (dir == std::filesystem::directory_iterator{}) {
				// The directory is empty
				StreamUP::DebugLogger::LogDebug("PathUtils", "Log Path Discovery", "OBS doesn't have files in the install directory");
				bfree(path_abs);
				return nullptr;
			} else {
				// The directory contains files
				return path_abs;
			}
		} else {
			// The directory does not exist
			StreamUP::DebugLogger::LogDebug("PathUtils", "Log Path Discovery", "OBS log file folder does not exist in the install directory");
			bfree(path_abs);
			return nullptr;
		}
	}

	return nullptr;
}

char* GetOBSConfigPath(const char* relativePath)
{
	return obs_module_config_path(relativePath);
}

} // namespace PathUtils
} // namespace StreamUP