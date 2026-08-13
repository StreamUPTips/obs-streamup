#include "zip-writer.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <zlib.h>

namespace StreamUP {
namespace Zip {

namespace {

constexpr quint32 kLocalHeaderSig = 0x04034b50;
constexpr quint32 kCentralHeaderSig = 0x02014b50;
constexpr quint32 kEndOfCentralDirSig = 0x06054b50;
constexpr quint32 kZip64EndSig = 0x06064b50;
constexpr quint32 kZip64LocatorSig = 0x07064b50;

// Version 4.5 = the minimum that understands ZIP64 records.
constexpr quint16 kVersionZip64 = 45;
constexpr quint16 kVersionDefault = 20;

// Anything at or above this cannot be expressed in the classic 32-bit fields.
constexpr quint64 kZip64Threshold = 0xFFFFFFFFull;

constexpr int kChunkSize = 128 * 1024;

void putU16(QByteArray &out, quint16 v)
{
	out.append(static_cast<char>(v & 0xFF));
	out.append(static_cast<char>((v >> 8) & 0xFF));
}

void putU32(QByteArray &out, quint32 v)
{
	for (int i = 0; i < 4; ++i)
		out.append(static_cast<char>((v >> (8 * i)) & 0xFF));
}

void putU64(QByteArray &out, quint64 v)
{
	for (int i = 0; i < 8; ++i)
		out.append(static_cast<char>((v >> (8 * i)) & 0xFF));
}

// MS-DOS date/time, which is what the zip format stores.
quint32 toDosTime(const QDateTime &dt)
{
	const QDate d = dt.date();
	const QTime t = dt.time();
	// DOS epoch starts at 1980; clamp rather than write a negative year.
	const int year = d.year() < 1980 ? 1980 : d.year();
	const quint32 date = static_cast<quint32>(((year - 1980) << 9) | (d.month() << 5) | d.day());
	const quint32 time = static_cast<quint32>((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
	return (date << 16) | time;
}

} // namespace

Writer::~Writer()
{
	if (file.isOpen())
		close();
}

bool Writer::fail(const QString &reason)
{
	error = reason;
	return false;
}

bool Writer::open(const QString &archivePath)
{
	entries.clear();
	error.clear();
	file.setFileName(archivePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return fail(QStringLiteral("Could not create %1: %2").arg(archivePath, file.errorString()));
	return true;
}

bool Writer::writeLocalHeader(Entry &entry)
{
	const QByteArray name = entry.name.toUtf8();
	const bool needsZip64 = entry.uncompressedSize >= kZip64Threshold || entry.compressedSize >= kZip64Threshold;

	QByteArray header;
	putU32(header, kLocalHeaderSig);
	putU16(header, needsZip64 ? kVersionZip64 : kVersionDefault);
	putU16(header, 0x0800); // UTF-8 names
	putU16(header, entry.method);
	putU32(header, entry.dosTime);
	putU32(header, entry.crc);
	if (needsZip64) {
		putU32(header, 0xFFFFFFFF);
		putU32(header, 0xFFFFFFFF);
	} else {
		putU32(header, static_cast<quint32>(entry.compressedSize));
		putU32(header, static_cast<quint32>(entry.uncompressedSize));
	}
	putU16(header, static_cast<quint16>(name.size()));
	putU16(header, needsZip64 ? 20 : 0); // extra field length
	header.append(name);
	if (needsZip64) {
		putU16(header, 0x0001); // ZIP64 extended information
		putU16(header, 16);
		putU64(header, entry.uncompressedSize);
		putU64(header, entry.compressedSize);
	}

	return file.write(header) == header.size();
}

bool Writer::addFile(const QString &sourcePath, const QString &archiveName, ChunkCallback onChunk)
{
	if (!file.isOpen())
		return fail(QStringLiteral("Archive is not open"));

	QFile in(sourcePath);
	if (!in.open(QIODevice::ReadOnly))
		return fail(QStringLiteral("Could not read %1: %2").arg(sourcePath, in.errorString()));

	Entry entry;
	entry.name = archiveName;
	entry.method = Z_DEFLATED;
	entry.dosTime = toDosTime(QFileInfo(sourcePath).lastModified());
	entry.localHeaderOffset = static_cast<quint64>(file.pos());
	entry.uncompressedSize = static_cast<quint64>(in.size());

	// The header is written first with placeholder sizes, then rewritten once
	// the sizes are known. Data descriptors would avoid the seek, but plenty of
	// tools handle them badly, and we always have a seekable file here.
	if (!writeLocalHeader(entry))
		return fail(QStringLiteral("Could not write header for %1").arg(archiveName));

	const qint64 dataStart = file.pos();

	z_stream stream{};
	if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
		return fail(QStringLiteral("Could not start compression for %1").arg(archiveName));

	QByteArray inBuf(kChunkSize, Qt::Uninitialized);
	QByteArray outBuf(kChunkSize, Qt::Uninitialized);
	quint32 crc = crc32(0, nullptr, 0);
	quint64 compressed = 0;
	qint64 consumed = 0;
	bool ok = true;

	forever {
		if (onChunk && !onChunk(consumed, static_cast<qint64>(entry.uncompressedSize))) {
			deflateEnd(&stream);
			return fail(QStringLiteral("Cancelled"));
		}

		const qint64 read = in.read(inBuf.data(), inBuf.size());
		if (read < 0) {
			ok = fail(QStringLiteral("Read failed on %1").arg(sourcePath));
			break;
		}

		const bool done = (read == 0);
		if (read > 0) {
			crc = crc32(crc, reinterpret_cast<const Bytef *>(inBuf.constData()), static_cast<uInt>(read));
			consumed += read;
		}

		stream.next_in = reinterpret_cast<Bytef *>(inBuf.data());
		stream.avail_in = static_cast<uInt>(read);

		do {
			stream.next_out = reinterpret_cast<Bytef *>(outBuf.data());
			stream.avail_out = static_cast<uInt>(outBuf.size());
			const int ret = deflate(&stream, done ? Z_FINISH : Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR) {
				ok = fail(QStringLiteral("Compression failed on %1").arg(archiveName));
				break;
			}
			const qint64 produced = outBuf.size() - static_cast<qint64>(stream.avail_out);
			if (produced > 0) {
				if (file.write(outBuf.constData(), produced) != produced) {
					ok = fail(QStringLiteral("Write failed while adding %1").arg(archiveName));
					break;
				}
				compressed += static_cast<quint64>(produced);
			}
		} while (stream.avail_out == 0 && ok);

		if (!ok || done)
			break;
	}

	deflateEnd(&stream);
	in.close();

	if (!ok)
		return false;

	entry.crc = crc;
	entry.compressedSize = compressed;

	// Rewrite the header now the real sizes and CRC are known.
	const qint64 endOfData = file.pos();
	if (!file.seek(static_cast<qint64>(entry.localHeaderOffset)))
		return fail(QStringLiteral("Could not rewind to patch header for %1").arg(archiveName));
	if (!writeLocalHeader(entry))
		return fail(QStringLiteral("Could not patch header for %1").arg(archiveName));
	if (!file.seek(endOfData))
		return fail(QStringLiteral("Could not resume after patching %1").arg(archiveName));

	Q_UNUSED(dataStart);
	entries.push_back(entry);
	return true;
}

bool Writer::addData(const QByteArray &data, const QString &archiveName)
{
	if (!file.isOpen())
		return fail(QStringLiteral("Archive is not open"));

	// Small blobs (the manifest) are stored rather than deflated: simpler, and
	// the size difference is irrelevant.
	Entry entry;
	entry.name = archiveName;
	entry.method = 0; // stored
	entry.dosTime = toDosTime(QDateTime::currentDateTime());
	entry.localHeaderOffset = static_cast<quint64>(file.pos());
	entry.uncompressedSize = static_cast<quint64>(data.size());
	entry.compressedSize = entry.uncompressedSize;
	entry.crc = crc32(crc32(0, nullptr, 0), reinterpret_cast<const Bytef *>(data.constData()),
			  static_cast<uInt>(data.size()));

	if (!writeLocalHeader(entry))
		return fail(QStringLiteral("Could not write header for %1").arg(archiveName));
	if (file.write(data) != data.size())
		return fail(QStringLiteral("Could not write %1").arg(archiveName));

	entries.push_back(entry);
	return true;
}

bool Writer::writeCentralDirectory()
{
	const quint64 dirStart = static_cast<quint64>(file.pos());

	for (const Entry &entry : entries) {
		const QByteArray name = entry.name.toUtf8();
		const bool bigSizes = entry.uncompressedSize >= kZip64Threshold ||
				      entry.compressedSize >= kZip64Threshold;
		const bool bigOffset = entry.localHeaderOffset >= kZip64Threshold;
		const bool needsZip64 = bigSizes || bigOffset;

		QByteArray extra;
		if (needsZip64) {
			putU16(extra, 0x0001);
			putU16(extra, 0); // length patched below
			if (bigSizes) {
				putU64(extra, entry.uncompressedSize);
				putU64(extra, entry.compressedSize);
			}
			if (bigOffset)
				putU64(extra, entry.localHeaderOffset);
			const quint16 payload = static_cast<quint16>(extra.size() - 4);
			extra[2] = static_cast<char>(payload & 0xFF);
			extra[3] = static_cast<char>((payload >> 8) & 0xFF);
		}

		QByteArray header;
		putU32(header, kCentralHeaderSig);
		putU16(header, needsZip64 ? kVersionZip64 : kVersionDefault); // version made by
		putU16(header, needsZip64 ? kVersionZip64 : kVersionDefault); // version needed
		putU16(header, 0x0800);
		putU16(header, entry.method);
		putU32(header, entry.dosTime);
		putU32(header, entry.crc);
		putU32(header, bigSizes ? 0xFFFFFFFF : static_cast<quint32>(entry.compressedSize));
		putU32(header, bigSizes ? 0xFFFFFFFF : static_cast<quint32>(entry.uncompressedSize));
		putU16(header, static_cast<quint16>(name.size()));
		putU16(header, static_cast<quint16>(extra.size()));
		putU16(header, 0); // comment length
		putU16(header, 0); // disk number
		putU16(header, 0); // internal attrs
		putU32(header, 0); // external attrs
		putU32(header, bigOffset ? 0xFFFFFFFF : static_cast<quint32>(entry.localHeaderOffset));
		header.append(name);
		header.append(extra);

		if (file.write(header) != header.size())
			return fail(QStringLiteral("Could not write central directory"));
	}

	const quint64 dirSize = static_cast<quint64>(file.pos()) - dirStart;
	const bool needsZip64 = entries.size() >= 0xFFFF || dirStart >= kZip64Threshold ||
				dirSize >= kZip64Threshold;

	QByteArray tail;
	if (needsZip64) {
		const quint64 zip64Start = static_cast<quint64>(file.pos());
		putU32(tail, kZip64EndSig);
		putU64(tail, 44); // size of this record minus 12
		putU16(tail, kVersionZip64);
		putU16(tail, kVersionZip64);
		putU32(tail, 0);
		putU32(tail, 0);
		putU64(tail, entries.size());
		putU64(tail, entries.size());
		putU64(tail, dirSize);
		putU64(tail, dirStart);

		putU32(tail, kZip64LocatorSig);
		putU32(tail, 0);
		putU64(tail, zip64Start);
		putU32(tail, 1);
	}

	putU32(tail, kEndOfCentralDirSig);
	putU16(tail, 0);
	putU16(tail, 0);
	putU16(tail, entries.size() >= 0xFFFF ? 0xFFFF : static_cast<quint16>(entries.size()));
	putU16(tail, entries.size() >= 0xFFFF ? 0xFFFF : static_cast<quint16>(entries.size()));
	putU32(tail, dirSize >= kZip64Threshold ? 0xFFFFFFFF : static_cast<quint32>(dirSize));
	putU32(tail, dirStart >= kZip64Threshold ? 0xFFFFFFFF : static_cast<quint32>(dirStart));
	putU16(tail, 0); // no archive comment

	return file.write(tail) == tail.size();
}

bool Writer::close()
{
	if (!file.isOpen())
		return true;

	const bool ok = writeCentralDirectory();
	file.close();
	return ok;
}

bool VerifyArchive(const QString &archivePath, int expectedEntries, QString *error)
{
	auto reportError = [error](const QString &reason) {
		if (error)
			*error = reason;
		return false;
	};

	QFile in(archivePath);
	if (!in.open(QIODevice::ReadOnly))
		return reportError(QStringLiteral("Could not reopen the archive: %1").arg(in.errorString()));

	// The end of central directory record lives in the last 64KB (22 bytes plus
	// up to 64KB of comment, and we never write a comment).
	const qint64 size = in.size();
	const qint64 tailSize = qMin<qint64>(size, 66000);
	if (!in.seek(size - tailSize))
		return reportError(QStringLiteral("Could not read the end of the archive"));
	const QByteArray tail = in.read(tailSize);
	in.close();

	int eocd = -1;
	for (int i = tail.size() - 22; i >= 0; --i) {
		if (static_cast<quint8>(tail[i]) == 0x50 && static_cast<quint8>(tail[i + 1]) == 0x4b &&
		    static_cast<quint8>(tail[i + 2]) == 0x05 && static_cast<quint8>(tail[i + 3]) == 0x06) {
			eocd = i;
			break;
		}
	}
	if (eocd < 0)
		return reportError(QStringLiteral("Archive is missing its central directory"));

	auto readU16 = [&tail](int offset) {
		return static_cast<quint16>(static_cast<quint8>(tail[offset]) |
					    (static_cast<quint8>(tail[offset + 1]) << 8));
	};

	quint64 count = readU16(eocd + 10);

	// ZIP64: the classic field saturates at 0xFFFF and the real count lives in
	// the ZIP64 end of central directory record.
	if (count == 0xFFFF) {
		int zip64 = -1;
		for (int i = eocd - 56; i >= 0; --i) {
			if (static_cast<quint8>(tail[i]) == 0x50 && static_cast<quint8>(tail[i + 1]) == 0x4b &&
			    static_cast<quint8>(tail[i + 2]) == 0x06 && static_cast<quint8>(tail[i + 3]) == 0x06) {
				zip64 = i;
				break;
			}
		}
		if (zip64 >= 0) {
			count = 0;
			for (int b = 0; b < 8; ++b)
				count |= static_cast<quint64>(static_cast<quint8>(tail[zip64 + 32 + b])) << (8 * b);
		}
	}

	if (static_cast<int>(count) != expectedEntries)
		return reportError(QStringLiteral("Archive lists %1 entries but %2 were written")
					   .arg(count)
					   .arg(expectedEntries));

	return true;
}

} // namespace Zip
} // namespace StreamUP
