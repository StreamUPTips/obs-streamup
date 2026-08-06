#ifndef STREAMUP_ZIP_WRITER_HPP
#define STREAMUP_ZIP_WRITER_HPP

#include <QByteArray>
#include <QFile>
#include <QString>
#include <cstdint>
#include <functional>
#include <vector>

namespace StreamUP {
namespace Zip {

/**
 * Minimal streaming ZIP writer (deflate, with ZIP64 when an archive or member
 * needs it). Written rather than pulled in as a dependency because the only
 * thing needed is "write files into a .zip", zlib already ships with libobs,
 * and QuaZip/QZipWriter are not available to us (QZipWriter is Qt-private).
 *
 * Entries are written one at a time and streamed from disk, so memory use stays
 * flat regardless of how big a collected media file is.
 */
class Writer {
public:
	Writer() = default;
	~Writer();

	/** Create/truncate the archive. Returns false if it cannot be opened. */
	bool open(const QString &archivePath);

	/** True once open() has succeeded and close() has not yet run. */
	bool isOpen() const { return file.isOpen(); }

	/**
	 * Called as each chunk of a file is compressed, with bytes done and the
	 * file's total. Lets the caller keep the UI alive during a single large
	 * file, which per-file progress alone cannot do. Return false to abort.
	 */
	using ChunkCallback = std::function<bool(qint64, qint64)>;

	/**
	 * Add a file from disk. archiveName uses forward slashes and must be
	 * relative. Returns false on read or write failure.
	 */
	bool addFile(const QString &sourcePath, const QString &archiveName, ChunkCallback onChunk = nullptr);

	/** Add an in-memory blob (used for the manifest). */
	bool addData(const QByteArray &data, const QString &archiveName);

	/** Write the central directory. Called by the destructor if not called. */
	bool close();

	/** Human-readable reason for the last failure, for logging and the UI. */
	QString lastError() const { return error; }

	/** Bytes written to the archive so far. */
	qint64 bytesWritten() const { return file.pos(); }

private:
	struct Entry {
		QString name;
		quint32 crc = 0;
		quint64 compressedSize = 0;
		quint64 uncompressedSize = 0;
		quint64 localHeaderOffset = 0;
		quint16 method = 0;
		quint32 dosTime = 0;
	};

	bool writeLocalHeader(Entry &entry);
	bool writeCentralDirectory();
	bool fail(const QString &reason);

	QFile file;
	std::vector<Entry> entries;
	QString error;
};

/**
 * Re-open a written archive and check it is structurally sound: the end of
 * central directory record is present, and it claims the number of entries we
 * think we wrote. Cheap, and it turns "the file exists" into "the file is a
 * valid archive containing what we intended", which is the difference between
 * a backup and a false sense of security.
 */
bool VerifyArchive(const QString &archivePath, int expectedEntries, QString *error);

} // namespace Zip
} // namespace StreamUP

#endif // STREAMUP_ZIP_WRITER_HPP
