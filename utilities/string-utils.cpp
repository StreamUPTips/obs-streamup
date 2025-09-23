#include "string-utils.hpp"
#include "path-utils.hpp"
#include <obs-module.h>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <sstream>
#include <regex>
#include <fstream>

namespace StreamUP {
namespace StringUtils {

std::vector<std::string> SplitString(const std::string &str, const std::string &delimiter)
{
	std::vector<std::string> parts;
	if (delimiter.length() == 1) {
		// Use faster single-character delimiter approach
		std::istringstream stream(str);
		std::string part;
		char delim = delimiter[0];
		
		while (std::getline(stream, part, delim)) {
			parts.push_back(part);
		}
	} else {
		// Multi-character delimiter
		size_t start = 0;
		size_t end = str.find(delimiter);
		
		while (end != std::string::npos) {
			parts.push_back(str.substr(start, end - start));
			start = end + delimiter.length();
			end = str.find(delimiter, start);
		}
		parts.push_back(str.substr(start));
	}
	
	return parts;
}

bool SearchStringInFile(const QString &filePath, const QString &searchString)
{
	QString mostRecentFile;
	
	// If filePath is a directory, get the most recent file
	QFileInfo fileInfo(filePath);
	if (fileInfo.isDir()) {
		mostRecentFile = PathUtils::GetMostRecentFile(filePath);
		if (mostRecentFile.isEmpty()) {
			return false;
		}
	} else {
		mostRecentFile = filePath;
	}
	
	QFile file(mostRecentFile);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return false;
	}
	
	QTextStream stream(&file);
	while (!stream.atEnd()) {
		QString line = stream.readLine();
		if (line.contains(searchString)) {
			return true;
		}
	}
	
	return false;
}

QString GetPlatformURL(const QString &windowsURL, const QString &macURL, 
                      const QString &linuxURL, const QString &generalURL)
{
#ifdef _WIN32
	(void)macURL;
	(void)linuxURL;
	return windowsURL.isEmpty() ? generalURL : windowsURL;
#elif defined(__APPLE__)
	(void)windowsURL;
	(void)linuxURL;
	return macURL.isEmpty() ? generalURL : macURL;
#elif defined(__linux__)
	(void)windowsURL;
	(void)macURL;
	return linuxURL.isEmpty() ? generalURL : linuxURL;
#else
	(void)windowsURL;
	(void)macURL;
	(void)linuxURL;
	return generalURL;
#endif
}

std::string QStringToStdString(const QString &qstr)
{
	return qstr.toStdString();
}

QString StdStringToQString(const std::string &str)
{
	return QString::fromStdString(str);
}

} // namespace StringUtils
} // namespace StreamUP
