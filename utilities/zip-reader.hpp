#ifndef STREAMUP_ZIP_READER_HPP
#define STREAMUP_ZIP_READER_HPP

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QString>
#include <QStringList>

namespace StreamUP {
namespace Zip {

/**
 * Minimal ZIP reader, the counterpart to Zip::Writer. Reads the central
 * directory up front, then extracts entries on demand (stored or deflated,
 * with ZIP64 sizes and offsets).
 *
 * Entries are extracted straight to disk rather than through memory so a
 * restore of a backup with collected media does not need the file to fit in
 * RAM. Every extraction is CRC-checked: a restore is the moment a corrupt
 * archive must be caught, not the moment it is discovered.
 */
class Reader {
public:
	struct Entry {
		QString name;
		quint64 compressedSize = 0;
		quint64 uncompressedSize = 0;
		quint64 localHeaderOffset = 0;
		quint32 crc = 0;
		quint16 method = 0;
	};

	~Reader();

	/** Open and parse the central directory. */
	bool open(const QString &archivePath);
	void close();

	/** Every entry name in the archive, in central directory order. */
	QStringList entryNames() const;
	bool contains(const QString &name) const { return index.contains(name); }
	const Entry *entry(const QString &name) const;

	/** Read a whole entry into memory. Use for the manifest and other small files. */
	QByteArray readFile(const QString &name);

	/** Extract an entry to an absolute path, creating parent directories. */
	bool extractTo(const QString &name, const QString &destinationPath);

	QString lastError() const { return error; }

private:
	bool fail(const QString &reason);
	bool readCentralDirectory();

	QFile file;
	QList<Entry> entries;
	QHash<QString, int> index;
	QString error;
};

} // namespace Zip
} // namespace StreamUP

#endif // STREAMUP_ZIP_READER_HPP
