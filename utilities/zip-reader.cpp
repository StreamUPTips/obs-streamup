#include "zip-reader.hpp"

#include <QDir>
#include <QFileInfo>
#include <zlib.h>

namespace StreamUP {
namespace Zip {

namespace {

constexpr quint32 kCentralHeaderSig = 0x02014b50;
constexpr quint32 kEndOfCentralDirSig = 0x06054b50;
constexpr quint32 kZip64EndSig = 0x06064b50;
constexpr quint64 kZip64Marker = 0xFFFFFFFFull;
constexpr int kChunkSize = 128 * 1024;

quint16 readU16(const QByteArray &data, int offset)
{
	return static_cast<quint16>(static_cast<quint8>(data[offset]) |
				    (static_cast<quint8>(data[offset + 1]) << 8));
}

quint32 readU32(const QByteArray &data, int offset)
{
	quint32 v = 0;
	for (int i = 0; i < 4; ++i)
		v |= static_cast<quint32>(static_cast<quint8>(data[offset + i])) << (8 * i);
	return v;
}

quint64 readU64(const QByteArray &data, int offset)
{
	quint64 v = 0;
	for (int i = 0; i < 8; ++i)
		v |= static_cast<quint64>(static_cast<quint8>(data[offset + i])) << (8 * i);
	return v;
}

} // namespace

Reader::~Reader()
{
	close();
}

bool Reader::fail(const QString &reason)
{
	error = reason;
	return false;
}

void Reader::close()
{
	if (file.isOpen())
		file.close();
	entries.clear();
	index.clear();
}

bool Reader::open(const QString &archivePath)
{
	close();
	error.clear();

	file.setFileName(archivePath);
	if (!file.open(QIODevice::ReadOnly))
		return fail(QStringLiteral("Could not open %1: %2").arg(archivePath, file.errorString()));

	if (!readCentralDirectory()) {
		file.close();
		return false;
	}
	return true;
}

bool Reader::readCentralDirectory()
{
	const qint64 size = file.size();
	if (size < 22)
		return fail(QStringLiteral("File is too small to be a zip archive"));

	// End of central directory lives in the last 64KB (comment is at most that).
	const qint64 tailSize = qMin<qint64>(size, 66000);
	if (!file.seek(size - tailSize))
		return fail(QStringLiteral("Could not seek to the end of the archive"));
	const QByteArray tail = file.read(tailSize);

	int eocd = -1;
	for (int i = tail.size() - 22; i >= 0; --i) {
		if (readU32(tail, i) == kEndOfCentralDirSig) {
			eocd = i;
			break;
		}
	}
	if (eocd < 0)
		return fail(QStringLiteral("Not a zip archive (no central directory found)"));

	quint64 entryCount = readU16(tail, eocd + 10);
	quint64 dirOffset = readU32(tail, eocd + 16);

	// ZIP64: saturated fields point at the ZIP64 record for the real values.
	if (entryCount == 0xFFFF || dirOffset == kZip64Marker) {
		int zip64 = -1;
		for (int i = eocd - 56; i >= 0; --i) {
			if (readU32(tail, i) == kZip64EndSig) {
				zip64 = i;
				break;
			}
		}
		if (zip64 < 0)
			return fail(QStringLiteral("Archive claims ZIP64 but has no ZIP64 record"));
		entryCount = readU64(tail, zip64 + 32);
		dirOffset = readU64(tail, zip64 + 48);
	}

	if (!file.seek(static_cast<qint64>(dirOffset)))
		return fail(QStringLiteral("Could not seek to the central directory"));

	for (quint64 i = 0; i < entryCount; ++i) {
		const QByteArray header = file.read(46);
		if (header.size() != 46 || readU32(header, 0) != kCentralHeaderSig)
			return fail(QStringLiteral("Central directory entry %1 is malformed").arg(i));

		Entry e;
		e.method = readU16(header, 10);
		e.crc = readU32(header, 16);
		e.compressedSize = readU32(header, 20);
		e.uncompressedSize = readU32(header, 24);
		const quint16 nameLen = readU16(header, 28);
		const quint16 extraLen = readU16(header, 30);
		const quint16 commentLen = readU16(header, 32);
		e.localHeaderOffset = readU32(header, 42);

		const QByteArray nameBytes = file.read(nameLen);
		e.name = QString::fromUtf8(nameBytes);
		const QByteArray extra = file.read(extraLen);
		file.skip(commentLen);

		// Pull the real sizes/offset out of the ZIP64 extra field, in the order
		// the spec defines: only the saturated fields are present.
		if (e.uncompressedSize == kZip64Marker || e.compressedSize == kZip64Marker ||
		    e.localHeaderOffset == kZip64Marker) {
			int pos = 0;
			while (pos + 4 <= extra.size()) {
				const quint16 tag = readU16(extra, pos);
				const quint16 len = readU16(extra, pos + 2);
				if (tag == 0x0001) {
					int field = pos + 4;
					if (e.uncompressedSize == kZip64Marker && field + 8 <= extra.size()) {
						e.uncompressedSize = readU64(extra, field);
						field += 8;
					}
					if (e.compressedSize == kZip64Marker && field + 8 <= extra.size()) {
						e.compressedSize = readU64(extra, field);
						field += 8;
					}
					if (e.localHeaderOffset == kZip64Marker && field + 8 <= extra.size())
						e.localHeaderOffset = readU64(extra, field);
					break;
				}
				pos += 4 + len;
			}
		}

		index.insert(e.name, entries.size());
		entries.append(e);
	}

	return true;
}

QStringList Reader::entryNames() const
{
	QStringList names;
	names.reserve(entries.size());
	for (const Entry &e : entries)
		names << e.name;
	return names;
}

const Reader::Entry *Reader::entry(const QString &name) const
{
	const auto it = index.constFind(name);
	if (it == index.constEnd())
		return nullptr;
	return &entries[it.value()];
}

bool Reader::extractTo(const QString &name, const QString &destinationPath)
{
	const Entry *e = entry(name);
	if (!e)
		return fail(QStringLiteral("%1 is not in the archive").arg(name));

	// The local header repeats the name and extra field, and its lengths are
	// what tell us where the data actually starts.
	if (!file.seek(static_cast<qint64>(e->localHeaderOffset)))
		return fail(QStringLiteral("Could not seek to %1").arg(name));
	const QByteArray local = file.read(30);
	if (local.size() != 30)
		return fail(QStringLiteral("Truncated local header for %1").arg(name));
	const quint16 nameLen = readU16(local, 26);
	const quint16 extraLen = readU16(local, 28);
	if (!file.seek(static_cast<qint64>(e->localHeaderOffset) + 30 + nameLen + extraLen))
		return fail(QStringLiteral("Could not seek to the data for %1").arg(name));

	QDir().mkpath(QFileInfo(destinationPath).absolutePath());
	QFile out(destinationPath);
	if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return fail(QStringLiteral("Could not write %1: %2").arg(destinationPath, out.errorString()));

	quint32 crc = crc32(0, nullptr, 0);
	quint64 remaining = e->compressedSize;
	bool ok = true;

	if (e->method == 0) {
		QByteArray buffer(kChunkSize, Qt::Uninitialized);
		while (remaining > 0) {
			const qint64 want = qMin<quint64>(remaining, kChunkSize);
			const qint64 got = file.read(buffer.data(), want);
			if (got <= 0) {
				ok = fail(QStringLiteral("Unexpected end of archive reading %1").arg(name));
				break;
			}
			crc = crc32(crc, reinterpret_cast<const Bytef *>(buffer.constData()), static_cast<uInt>(got));
			if (out.write(buffer.constData(), got) != got) {
				ok = fail(QStringLiteral("Write failed extracting %1").arg(name));
				break;
			}
			remaining -= static_cast<quint64>(got);
		}
	} else if (e->method == Z_DEFLATED) {
		z_stream stream{};
		if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
			out.close();
			return fail(QStringLiteral("Could not start decompression for %1").arg(name));
		}

		QByteArray inBuf(kChunkSize, Qt::Uninitialized);
		QByteArray outBuf(kChunkSize, Qt::Uninitialized);
		int ret = Z_OK;

		while (ret != Z_STREAM_END) {
			if (stream.avail_in == 0) {
				const qint64 want = qMin<quint64>(remaining, kChunkSize);
				if (want == 0)
					break;
				const qint64 got = file.read(inBuf.data(), want);
				if (got <= 0) {
					ok = fail(QStringLiteral("Unexpected end of archive reading %1").arg(name));
					break;
				}
				remaining -= static_cast<quint64>(got);
				stream.next_in = reinterpret_cast<Bytef *>(inBuf.data());
				stream.avail_in = static_cast<uInt>(got);
			}

			stream.next_out = reinterpret_cast<Bytef *>(outBuf.data());
			stream.avail_out = static_cast<uInt>(outBuf.size());
			ret = inflate(&stream, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
				ok = fail(QStringLiteral("Corrupt data in %1").arg(name));
				break;
			}

			const qint64 produced = outBuf.size() - static_cast<qint64>(stream.avail_out);
			if (produced > 0) {
				crc = crc32(crc, reinterpret_cast<const Bytef *>(outBuf.constData()),
					    static_cast<uInt>(produced));
				if (out.write(outBuf.constData(), produced) != produced) {
					ok = fail(QStringLiteral("Write failed extracting %1").arg(name));
					break;
				}
			}
		}
		inflateEnd(&stream);
	} else {
		ok = fail(QStringLiteral("%1 uses an unsupported compression method").arg(name));
	}

	out.close();

	if (ok && crc != e->crc) {
		QFile::remove(destinationPath);
		return fail(QStringLiteral("%1 failed its checksum, the archive is damaged").arg(name));
	}

	if (!ok)
		QFile::remove(destinationPath);

	return ok;
}

QByteArray Reader::readFile(const QString &name)
{
	// Small files only: goes via a temporary so the extract path (and its CRC
	// check) stays the single implementation.
	const QString tempPath = QDir::temp().filePath(
		QStringLiteral("streamup-zip-%1").arg(QFileInfo(name).fileName()));
	if (!extractTo(name, tempPath))
		return QByteArray();

	QFile in(tempPath);
	QByteArray data;
	if (in.open(QIODevice::ReadOnly)) {
		data = in.readAll();
		in.close();
	}
	QFile::remove(tempPath);
	return data;
}

} // namespace Zip
} // namespace StreamUP
