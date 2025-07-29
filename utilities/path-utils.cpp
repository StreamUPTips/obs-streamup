#include "path-utils.hpp"
#include <filesystem>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
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

} // namespace PathUtils
} // namespace StreamUP