// qtractorZipFile.cpp
//
/****************************************************************************
   Copyright (C) 2010-2013, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAbout.h"

#ifdef CONFIG_LIBZ

/* Most of this code was originally borrowed, stirred, mangled
 * and finally adapted from the Qt 4.6 source code (LGPL).
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(ies).
 * All rights reserved.
 * Contact: Nokia Corporation (qt-info@nokia.com)
 */

#include "qtractorZipFile.h"

#define QTRACTOR_PROGRESS_BAR
#ifdef  QTRACTOR_PROGRESS_BAR
#include "qtractorMainForm.h"
#include <QProgressBar>
#endif

#include <QDateTime>
#include <QDir>
#include <QHash>

#include <zlib.h>

#include <sys/stat.h>

#if defined(Q_OS_WIN)
#  undef  S_IFREG
#  undef  S_ISDIR
#  undef  S_ISREG
#  undef  S_IRUSR
#  undef  S_IWUSR
#  undef  S_IXUSR
#  define S_IFREG 0100000
#  define S_ISDIR(x) ((x) & 0040000) > 0
#  define S_ISREG(x) ((x) & 0170000) == S_IFREG
#  define S_IFLNK 020000
#  define S_ISLNK(x) ((x) & S_IFLNK) > 0
#  define S_IRUSR 0400
#  define S_IWUSR 0200
#  define S_IXUSR 0100
#  define S_IRGRP 0040
#  define S_IWGRP 0020
#  define S_IXGRP 0010
#  define S_IROTH 0004
#  define S_IWOTH 0002
#  define S_IXOTH 0001
#endif

#include <utime.h>

#define BUFF_SIZE 16384


static inline unsigned int read_uint ( const unsigned char *data )
{
	return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
}

static inline unsigned short read_ushort ( const unsigned char *data )
{
	return data[0] + (data[1] << 8);
}

static inline void write_uint ( unsigned char *data, unsigned int i )
{
	data[0] = i & 0xff;
	data[1] = (i >>  8) & 0xff;
	data[2] = (i >> 16) & 0xff;
	data[3] = (i >> 24) & 0xff;
}

static inline void write_ushort ( unsigned char *data, ushort i )
{
	data[0] = i & 0xff;
	data[1] = (i >> 8) & 0xff;
}

static inline void copy_uint ( unsigned char *dest, const unsigned char *src )
{
	dest[0] = src[0];
	dest[1] = src[1];
	dest[2] = src[2];
	dest[3] = src[3];
}

static inline void copy_ushort ( unsigned char *dest, const unsigned char *src )
{
	dest[0] = src[0];
	dest[1] = src[1];
}

static void write_msdos_date ( unsigned char *data, const QDateTime& dt )
{
	if (dt.isValid()) {
		unsigned short time = (dt.time().hour() << 11) // 5 bit hour
			| (dt.time().minute() << 5)  // 6 bit minute
			| (dt.time().second() >> 1); // 5 bit double seconds
		data[0] = time & 0xff;
		data[1] = time >> 8;
		unsigned short date = ((dt.date().year() - 1980) << 9) // 7 bit year 1980-based
			| (dt.date().month() << 5)   // 4 bit month
			| (dt.date().day());         // 5 bit day
		data[2] = date & 0xff;
		data[3] = date >> 8;
	} else {
		data[0] = 0;
		data[1] = 0;
		data[2] = 0;
		data[3] = 0;
	}
}

static QDateTime read_msdos_date ( unsigned char *data )
{
	unsigned short time = data[0] + (data[1] << 8);
	unsigned short date = data[2] + (data[3] << 8);

	unsigned int year = (date >> 9) + 1980; // 7 bit year 1980-based.
	unsigned int mon  = (date >> 5) & 0x0f; // 4 bit month.
	unsigned int day  = (date & 0x1f);      // 5 bit day.
	unsigned int hh   = (time >> 11);       // 5 bit hour
	unsigned int mm   = (time >> 5) & 0x3f; // 6 bit minute
	unsigned int ss   = (time & 0x1f) << 1; // 5 bit double (m)seconds.

	return QDateTime(QDate(year, mon, day), QTime(hh, mm, ss));
}

static unsigned int mode_from_permissions ( QFile::Permissions perms )
{
	unsigned int mode = 0;

	if (perms & QFile::ReadOwner)
		mode |= S_IRUSR;
	if (perms & QFile::WriteOwner)
		mode |= S_IWUSR;
	if (perms & QFile::ExeOwner)
		mode |= S_IXUSR;
	if (perms & QFile::ReadUser)
		mode |= S_IRUSR;
	if (perms & QFile::WriteUser)
		mode |= S_IWUSR;
	if (perms & QFile::ExeUser)
		mode |= S_IXUSR;
	if (perms & QFile::ReadGroup)
		mode |= S_IRGRP;
	if (perms & QFile::WriteGroup)
		mode |= S_IWGRP;
	if (perms & QFile::ExeGroup)
		mode |= S_IXGRP;
	if (perms & QFile::ReadOther)
		mode |= S_IROTH;
	if (perms & QFile::WriteOther)
		mode |= S_IWOTH;
	if (perms & QFile::ExeOther)
		mode |= S_IXOTH;

	return mode;
}

static QFile::Permissions permissions_from_mode ( unsigned int mode )
{
	QFile::Permissions perms;

	if (mode & S_IRUSR)
		perms |= QFile::ReadOwner;
	if (mode & S_IWUSR)
		perms |= QFile::WriteOwner;
	if (mode & S_IXUSR)
		perms |= QFile::ExeOwner;
	if (mode & S_IRUSR)
		perms |= QFile::ReadUser;
	if (mode & S_IWUSR)
		perms |= QFile::WriteUser;
	if (mode & S_IXUSR)
		perms |= QFile::ExeUser;
	if (mode & S_IRGRP)
		perms |= QFile::ReadGroup;
	if (mode & S_IWGRP)
		perms |= QFile::WriteGroup;
	if (mode & S_IXGRP)
		perms |= QFile::ExeGroup;
	if (mode & S_IROTH)
		perms |= QFile::ReadOther;
	if (mode & S_IWOTH)
		perms |= QFile::WriteOther;
	if (mode & S_IXOTH)
		perms |= QFile::ExeOther;

	return perms;
}


struct LocalFileHeader
{
	unsigned char signature[4]; //  0x04034b50
	unsigned char version_needed[2];
	unsigned char general_purpose_bits[2];
	unsigned char compression_method[2];
	unsigned char last_mod_file[4];
	unsigned char crc_32[4];
	unsigned char compressed_size[4];
	unsigned char uncompressed_size[4];
	unsigned char file_name_length[2];
	unsigned char extra_field_length[2];
};

struct CentralFileHeader
{
	unsigned char signature[4]; // 0x02014b50
	unsigned char version_made[2];
	unsigned char version_needed[2];
	unsigned char general_purpose_bits[2];
	unsigned char compression_method[2];
	unsigned char last_mod_file[4];
	unsigned char crc_32[4];
	unsigned char compressed_size[4];
	unsigned char uncompressed_size[4];
	unsigned char file_name_length[2];
	unsigned char extra_field_length[2];
	unsigned char file_comment_length[2];
	unsigned char disk_start[2];
	unsigned char internal_file_attributes[2];
	unsigned char external_file_attributes[4];
	unsigned char offset_local_header[4];
};

struct EndOfDirectory
{
	unsigned char signature[4]; // 0x06054b50
	unsigned char this_disk[2];
	unsigned char start_of_directory_disk[2];
	unsigned char num_dir_entries_this_disk[2];
	unsigned char num_dir_entries[2];
	unsigned char directory_size[4];
	unsigned char dir_start_offset[4];
	unsigned char comment_length[2];
};


struct FileHeader
{
	CentralFileHeader h;
	QByteArray file_name;
	QByteArray extra_field;
	QByteArray file_comment;
};


static void copy_header ( LocalFileHeader& lfh, const CentralFileHeader& h )
{
	write_uint(lfh.signature, 0x04034b50);
	copy_ushort(lfh.version_needed, h.version_needed);
	copy_ushort(lfh.general_purpose_bits, h.general_purpose_bits);
	copy_ushort(lfh.compression_method, h.compression_method);
	copy_uint(lfh.last_mod_file, h.last_mod_file);
	copy_uint(lfh.crc_32, h.crc_32);
	copy_uint(lfh.compressed_size, h.compressed_size);
	copy_uint(lfh.uncompressed_size, h.uncompressed_size);
	copy_ushort(lfh.file_name_length, h.file_name_length);
	copy_ushort(lfh.extra_field_length, h.extra_field_length);
}


//----------------------------------------------------------------------------
// qtractorZipDevice  -- Common ZIP I/O device class.
//

class qtractorZipDevice
{
public:

	qtractorZipDevice (QIODevice *pDevice, bool bOwnDevice)
		: device(pDevice), own_device(bOwnDevice),
			status(qtractorZipFile::NoError),
			dirty_contents(true),
			total_uncompressed(0),
			total_compressed(0),
			total_processed(0),
			buff_read(new unsigned char [BUFF_SIZE]),
			buff_write(new unsigned char [BUFF_SIZE]),
			write_offset(0)
	{
	#ifdef QTRACTOR_PROGRESS_BAR
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		progress_bar = (pMainForm ? pMainForm->progressBar() : NULL);
	#endif
	}

	~qtractorZipDevice()
	{
		delete [] buff_read;
		delete [] buff_write;
		if (own_device) delete device;
	}

	void scanFiles();

	bool extractEntry(const QString& sFilename, const FileHeader& fh);
	bool extractAll();

	QString alias(const QString& sFilename,
		const QString& sPrefix = QString()) const;

	enum EntryType { File = 0, Directory, SymLink };
	bool addEntry(EntryType type, const QString& sFilename,
		const QString& sAlias = QString());

	bool processEntry(const QString& sFilename, FileHeader& fh);
	bool processAll();

	QIODevice *device;
	bool own_device;
	qtractorZipFile::Status status;
	bool dirty_contents;
	QHash<QString, FileHeader> file_headers;
	QHash<QString, int> file_aliases;
	QByteArray comment;
	unsigned int total_uncompressed;
	unsigned int total_compressed;
	unsigned int total_processed;
	unsigned char *buff_read;
	unsigned char *buff_write;
	unsigned int write_offset;
#ifdef QTRACTOR_PROGRESS_BAR
	QProgressBar *progress_bar;
#endif
};


//----------------------------------------------------------------------------
// qtractorZipDevice -- Common ZIP I/O device class.
//

// Scans and reads zip archive entry directory (read-only).
void qtractorZipDevice::scanFiles (void)
{
	if (!dirty_contents)
		return;

	if (!(device->isOpen() || device->open(QIODevice::ReadOnly))) {
		status = qtractorZipFile::FileOpenError;
		return;
	}

	if (!(device->openMode() & QIODevice::ReadOnly)) {
		status = qtractorZipFile::FileReadError;
		return;
	}

	total_uncompressed = 0;
	total_compressed = 0;
	total_processed = 0;

	file_headers.clear();

	dirty_contents = false;
	unsigned char tmp[4];
	device->read((char *) tmp, 4);
	if (read_uint(tmp) != 0x04034b50) {
		qWarning("qtractorZipDevice::scanFiles: not a zip file.");
		return;
	}

	// Find EndOfDirectory header...
	int i = 0;
	int dir_start_offset = -1;
	int num_dir_entries = 0;
	EndOfDirectory eod;
	while (dir_start_offset == -1) {
		int pos = device->size() - sizeof(EndOfDirectory) - i;
		if (pos < 0 || i > 65535) {
			qWarning("qtractorZipDevice::scanFiles: "
				"end-of-directory not found.");
			return;
		}

		device->seek(pos);
		device->read((char *) &eod, sizeof(EndOfDirectory));
		if (read_uint(eod.signature) == 0x06054b50)
			break;
		++i;
	}

	// Have the eod...
	dir_start_offset = read_uint(eod.dir_start_offset);
	num_dir_entries = read_ushort(eod.num_dir_entries);

	int comment_length = read_ushort(eod.comment_length);
	if (comment_length != i)
		qWarning("qtractorZipDevice::scanFiles: failed to parse zip file.");
	comment = device->read(qMin(comment_length, i));

	device->seek(dir_start_offset);
	for (i = 0; i < num_dir_entries; ++i) {
		FileHeader fh;
		int read = device->read((char *) &fh.h, sizeof(CentralFileHeader));
		if (read < (int) sizeof(CentralFileHeader)) {
			qWarning("qtractorZipDevice::scanFiles: "
				"failed to read complete header, "
				"index may be incomplete.");
			break;
		}
		if (read_uint(fh.h.signature) != 0x02014b50) {
			qWarning("qtractorZipDevice::scanFiles: "
				"invalid header signature, "
				"zip index may be incomplete.");
			break;
		}
		int l = read_ushort(fh.h.file_name_length);
		fh.file_name = device->read(l);
		if (fh.file_name.length() != l) {
			qWarning("qtractorZipDevice::scanFiles: "
				"failed to read filename, "
				"zip index may be incomplete.");
			break;
		}
		l = read_ushort(fh.h.extra_field_length);
		fh.extra_field = device->read(l);
		if (fh.extra_field.length() != l) {
			qWarning("qtractorZipDevice::scanFiles: "
				"failed to read extra field, skipping, "
				"zip index may be incomplete");
			break;
		}
		l = read_ushort(fh.h.file_comment_length);
		fh.file_comment = device->read(l);
		if (fh.file_comment.length() != l) {
			qWarning("qtractorZipDevice::scanFiles: "
				"failed to read file comment, "
				"index may be incomplete");
			break;
		}
		total_uncompressed += read_uint(fh.h.uncompressed_size);
		total_compressed += read_uint(fh.h.compressed_size);
		file_headers.insert(QString::fromLocal8Bit(fh.file_name), fh);
	}
}


// Extract contents of a zip archive file entry (read-only).
bool qtractorZipDevice::extractEntry (
	const QString& sFilename, const FileHeader& fh )
{
	if (!(device->isOpen() || device->open(QIODevice::ReadOnly))) {
		status = qtractorZipFile::FileOpenError;
		return false;
	}

	if (!(device->openMode() & QIODevice::ReadOnly)) {
		status = qtractorZipFile::FileReadError;
		return false;
	}

	QFileInfo info(sFilename);
	if (!info.dir().exists())
		QDir().mkpath(info.dir().path());

	QFile *pFile = NULL;
	const unsigned int mode = read_uint(fh.h.external_file_attributes) >> 16;
	if (S_ISREG(mode)) {
		pFile = new QFile(info.filePath());
		if (!pFile->open(QIODevice::WriteOnly)) {
			status = qtractorZipFile::FileError;
			delete pFile;
			return false;
		}
	}
	else
	if (S_ISDIR(mode))
		return QDir().mkpath(info.filePath());
	else
	if (pFile == NULL)
		return false;
	
	unsigned int uncompressed_size = read_uint(fh.h.uncompressed_size);
	unsigned int compressed_size = read_uint(fh.h.compressed_size);

	if (uncompressed_size == 0 || compressed_size == 0)
		return false;

	device->seek(read_uint(fh.h.offset_local_header));

	LocalFileHeader lfh;
	device->read((char *) &lfh, sizeof(LocalFileHeader));
	unsigned int skip = read_ushort(lfh.file_name_length)
		+ read_ushort(lfh.extra_field_length);
	device->seek(device->pos() + skip);

	ushort compression_method = read_ushort(lfh.compression_method);

	if (compression_method == 8) {
		unsigned int n_file_read  = 0;
		unsigned int n_file_write = 0;
		z_stream zstream;
		memset(&zstream, 0, sizeof(zstream));
		unsigned int crc_32 = ::crc32(0, 0, 0);
		int zrc = ::inflateInit2(&zstream, -MAX_WBITS);
		while (zrc != Z_STREAM_END) {
			unsigned int n_buff_read = BUFF_SIZE;
			if (n_file_read + BUFF_SIZE > compressed_size)
				n_buff_read = compressed_size - n_file_read;
			device->read((char *) buff_read, n_buff_read);
			n_file_read += n_buff_read;
			zstream.next_in  = (uchar *) buff_read;
			zstream.avail_in = (uint)  n_buff_read;
			do {
				unsigned int n_buff_write = BUFF_SIZE;
				zstream.next_out  = (uchar *) buff_write;
				zstream.avail_out = (uint)  n_buff_write;
				zrc = ::inflate(&zstream, Z_NO_FLUSH);
				if (zrc != Z_STREAM_ERROR) {
					n_buff_write -= zstream.avail_out;
					if (n_buff_write > 0) {
						pFile->write((const char *) buff_write, n_buff_write);
						crc_32 = ::crc32(crc_32,
							(const uchar *) buff_write,
							(ulong) n_buff_write);
						n_file_write += n_buff_write;
						total_processed += n_buff_write;
					}
				}
			}
			while (zstream.avail_out == 0);
		#ifdef QTRACTOR_PROGRESS_BAR
			if (progress_bar) progress_bar->setValue(
				(100.0f * float(total_processed)) / float(total_uncompressed));
		#endif
		}
	//	uncompressed_size = n_file_write;
		::inflateEnd(&zstream);
		if (crc_32 != read_uint(lfh.crc_32))
			qWarning("qtractorZipDevice::extractEntry: bad CRC32!");
	} else {
		// No compression...
		QByteArray data = device->read(compressed_size);
		data.truncate(uncompressed_size);
		pFile->write(data);
	}

	pFile->setPermissions(permissions_from_mode(S_IRUSR | S_IWUSR | mode));
	pFile->close();
	delete pFile;

	// Set file time using utime...
	struct utimbuf utb;
	long tse = read_msdos_date(lfh.last_mod_file).toTime_t();
	utb.actime = tse;
	utb.modtime = tse;
	if (::utime(fh.file_name.data(), &utb))
		qWarning("qtractorZipDevice::extractEntry: failed to set file time.");

#ifdef CONFIG_DEBUG
	qDebug("qtractorZipDevice::inflate(%3.0f%%) %s",
		(100.0f * float(total_processed)) / float(total_uncompressed),
		fh.file_name.data());
#endif

	return true;
}


// Extract the full contents of the zip file (read-only).
bool qtractorZipDevice::extractAll (void)
{
	scanFiles();

#ifdef QTRACTOR_PROGRESS_BAR
	if (progress_bar) {
		progress_bar->setRange(0, 100);
		progress_bar->reset();
		progress_bar->show();
	}
#endif

	int iExtracted = 0;

	QHash<QString, FileHeader>::ConstIterator iter
		= file_headers.constBegin();
	const QHash<QString, FileHeader>::ConstIterator& iter_end
		= file_headers.constEnd();
	for ( ; iter != iter_end; ++iter) {
		if (extractEntry(iter.key(), iter.value()))
			++iExtracted;
	}

#ifdef QTRACTOR_PROGRESS_BAR
	if (progress_bar)
		progress_bar->hide();
#endif

	return (iExtracted == file_headers.count());
}


// Returns an zip archive entry alias name avoiding duplicates (write-only).
QString qtractorZipDevice::alias (
	const QString& sFilename, const QString& sPrefix ) const
{
	QFileInfo info(sFilename);

	QString sAliasPrefix = sPrefix;
	if (!sAliasPrefix.isEmpty() && !sAliasPrefix.endsWith('/'))
		sAliasPrefix.append('/');
	sAliasPrefix.append(info.baseName());
	QString sAliasExt;
	const QString& sAliasSuffix = info.completeSuffix();
	if (!sAliasSuffix.isEmpty())
		sAliasExt = '.' + sAliasSuffix;

	QString sAlias = sAliasPrefix + sAliasExt;

	if (!file_headers.contains(info.canonicalFilePath())) {
		int i = 0;
		while (file_aliases.contains(sAlias)) {
			if (i == 0) sAliasPrefix.remove(QRegExp("\\-[0-9]+$"));
			sAlias = sAliasPrefix + '-' + QString::number(++i) + sAliasExt;
		}
	}

	return sAlias;
}


// Add an entry to a zip archive (write-only).
bool qtractorZipDevice::addEntry ( EntryType type,
	const QString& sFilename, const QString& sAlias )
{
	QFileInfo info(sFilename);
	if (info.isDir())
		type = Directory;
	else
	if (info.isSymLink())
		type = File; /* OVERRIDE */

	if (type == File && !info.exists())
		return false;

	const QString& sFilepath = info.canonicalFilePath();
	if (file_headers.contains(sFilepath))
		return true;

	FileHeader fh;
	memset(&fh.h, 0, sizeof(CentralFileHeader));
	write_uint(fh.h.signature, 0x02014b50);

	write_ushort(fh.h.version_needed, 0x14);
	write_uint(fh.h.uncompressed_size, info.size());
	write_msdos_date(fh.h.last_mod_file, info.lastModified());

	total_uncompressed += info.size();

	QString sFakename = sAlias;
	if (sFakename.isEmpty()) {
		if (info.isAbsolute()) {
			sFakename = sFilepath;
			sFakename.remove(QRegExp('^' + QDir::rootPath()));
		} else {
			sFakename = info.filePath();
			sFakename.remove(QRegExp("^[\\./]+"));
		}
	}

	fh.file_name = sFakename.toLocal8Bit();
	write_ushort(fh.h.file_name_length, fh.file_name.length());
	write_ushort(fh.h.version_made, 3 << 8);

	unsigned int mode = mode_from_permissions(info.permissions());
	switch (type) {
		case File:      mode |= S_IFREG; break;
		case Directory: mode |= S_IFDIR; break;
		case SymLink:   mode |= S_IFLNK; break;
	}
	write_uint(fh.h.external_file_attributes, mode << 16);
	write_uint(fh.h.offset_local_header, 0);  /* DEFERRED (write_offset) */
	write_ushort(fh.h.compression_method, 0); /* DEFERRED */

	file_headers.insert(sFilepath, fh);
	file_aliases.insert(sFakename, file_aliases.count());
	dirty_contents = true;

	return true;
}


// Process contents of zip archive entry (write-only).
bool qtractorZipDevice::processEntry ( const QString& sFilename, FileHeader& fh )
{
	if (!(device->isOpen() || device->open(QIODevice::WriteOnly))) {
		status = qtractorZipFile::FileOpenError;
		return false;
	}

	if (!(device->openMode() & QIODevice::WriteOnly)) {
		status = qtractorZipFile::FileWriteError;
		return false;
	}

	QFile *pFile = NULL;
	const unsigned int mode = read_uint(fh.h.external_file_attributes) >> 16;
	if (S_ISREG(mode)) {
		pFile = new QFile(sFilename);
		if (!pFile->open(QIODevice::ReadOnly)) {
			status = qtractorZipFile::FileError;
			delete pFile;
			return false;
		}
	}

	unsigned int uncompressed_size = read_uint(fh.h.uncompressed_size);
	unsigned int compressed_size = 0;

	device->seek(write_offset);

	LocalFileHeader lfh;
	copy_header(lfh, fh.h);
	device->write((char *) &lfh, sizeof(LocalFileHeader));
	device->write(fh.file_name);

	unsigned int crc_32 = ::crc32(0, 0, 0);

	if (pFile) {
		write_ushort(fh.h.compression_method, 8); /* DEFERRED */
		unsigned int n_file_read  = 0;
		unsigned int n_file_write = 0;
		z_stream zstream;
		memset(&zstream, 0, sizeof(zstream));
		int zrc = ::deflateInit2(&zstream,
			Z_DEFAULT_COMPRESSION,
			Z_DEFLATED, -MAX_WBITS, 8,
			Z_DEFAULT_STRATEGY);
		while (zrc != Z_STREAM_END) {
			unsigned int n_buff_read = BUFF_SIZE;
			if (n_file_read + BUFF_SIZE > uncompressed_size)
				n_buff_read =uncompressed_size - n_file_read;
			int zflush = (n_buff_read < BUFF_SIZE ? Z_FINISH : Z_NO_FLUSH);
			pFile->read((char *) buff_read, n_buff_read);
			crc_32 = ::crc32(crc_32,
				(const uchar *) buff_read,
				(ulong) n_buff_read);
			n_file_read += n_buff_read;
			total_processed += n_buff_read;
			zstream.next_in  = (uchar *) buff_read;
			zstream.avail_in = (uint)  n_buff_read;
			do {
				unsigned int n_buff_write = BUFF_SIZE;
				zstream.next_out  = (uchar *) buff_write;
				zstream.avail_out = (uint)  n_buff_write;
				zrc = ::deflate(&zstream, zflush);
				if (zrc != Z_STREAM_ERROR) {
					n_buff_write -= zstream.avail_out;
					if (n_buff_write > 0) {
						device->write((const char *) buff_write, n_buff_write);
						n_file_write += n_buff_write;
					}
				}
			}
			while (zstream.avail_out == 0);
		#ifdef QTRACTOR_PROGRESS_BAR
			if (progress_bar) progress_bar->setValue(
				(100.0f * float(total_processed)) / float(total_uncompressed));
		#endif
		}
		compressed_size = n_file_write;
		::deflateEnd(&zstream);
		pFile->close();
		delete pFile;
	}

	unsigned int last_offset = device->pos();

	// Rewrite updated header...
	total_compressed += compressed_size;
	write_uint(fh.h.compressed_size, compressed_size);
	write_uint(fh.h.crc_32, crc_32);

	device->seek(write_offset);
	copy_header(lfh, fh.h);
	device->write((char *) &lfh, sizeof(LocalFileHeader));

	write_uint(fh.h.offset_local_header, write_offset); /* DEFERRED */

	// Done for next item so far...
	write_offset = last_offset;

#ifdef CONFIG_DEBUG
	qDebug("qtractorZipDevice::deflate(%3.0f%%) %s.",
		(100.0f * float(total_processed)) / float(total_uncompressed),
		fh.file_name.data());
#endif

	return true;
}


// Process the full contents of the zip file (write-only).
bool qtractorZipDevice::processAll (void)
{
#ifdef QTRACTOR_PROGRESS_BAR
	if (progress_bar) {
		progress_bar->setRange(0, 100);
		progress_bar->reset();
		progress_bar->show();
	}
#endif

	int iProcessed = 0;

	QHash<QString, FileHeader>::Iterator iter = file_headers.begin();
	const QHash<QString, FileHeader>::Iterator& iter_end = file_headers.end();
	for ( ; iter != iter_end; ++iter) {
		if (!processEntry(iter.key(), iter.value()))
			break;
		++iProcessed;
	}

#ifdef QTRACTOR_PROGRESS_BAR
	if (progress_bar)
		progress_bar->hide();
#endif

	return (iProcessed == file_headers.count());
}


//----------------------------------------------------------------------------
// qtractorZipFile  -- Custom ZIP file archive class.
//

// Constructors.
qtractorZipFile::qtractorZipFile (
	const QString& sFilename, QIODevice::OpenMode mode )
{
	QFile *pFile = new QFile(sFilename);
	pFile->open(mode);
	qtractorZipFile::Status status;
	if (pFile->error() == QFile::NoError)
		status = NoError;
	else if (pFile->error() == QFile::ReadError)
		status = FileReadError;
	else if (pFile->error() == QFile::WriteError)
		status = FileWriteError;
	else if (pFile->error() == QFile::OpenError)
		status = FileOpenError;
	else if (pFile->error() == QFile::PermissionsError)
		status = FilePermissionsError;
	else
		status = FileError;

	m_pZip = new qtractorZipDevice(pFile, /*bOwnDevice=*/true);
	m_pZip->status = status;
}

qtractorZipFile::qtractorZipFile ( QIODevice *pDevice )
	: m_pZip(new qtractorZipDevice(pDevice, /*bOwnDevice=*/false))
{
	Q_ASSERT(pDevice);
}


// Desctructor
qtractorZipFile::~qtractorZipFile (void)
{
	close();
	delete m_pZip;
}


// Returns a status code indicating the first error met.
qtractorZipFile::Status qtractorZipFile::status (void) const
{
	return m_pZip->status;
}


// Returns true if the user can read the file;
// otherwise returns false.
bool qtractorZipFile::isReadable (void) const
{
	return (m_pZip->status == NoError) && m_pZip->device->isReadable();
}


// Returns true if the user can write the file;
// otherwise returns false.
bool qtractorZipFile::isWritable (void) const
{
	return (m_pZip->status == NoError) && m_pZip->device->isWritable();
}


// Returns true if the file exists; otherwise returns false.
bool qtractorZipFile::exists (void) const
{
	QFile *pFile = qobject_cast<QFile *> (m_pZip->device);
	return (pFile ? pFile->exists() : false);
}


// Extract file contents from the zip archive (read-only).
bool qtractorZipFile::extractFile ( const QString& sFilename )
{
	m_pZip->scanFiles();

	if (!m_pZip->file_headers.contains(sFilename))
		return false;

	return m_pZip->extractEntry(sFilename,
		m_pZip->file_headers.value(sFilename));
}

// Extracts the full contents of the zip archive (read-only).
bool qtractorZipFile::extractAll (void)
{
	return m_pZip->extractAll();
}


// Returns the possible alias name avoiding file entry duplicates (write-only).
QString qtractorZipFile::alias ( const QString& sFilename, const QString& sPrefix ) const
{
	return m_pZip->alias(sFilename, sPrefix);
}


// Add a file in the zip archive (write-only).
bool qtractorZipFile::addFile ( const QString& sFilename, const QString& sAlias )
{
	return m_pZip->addEntry(qtractorZipDevice::File, sFilename, sAlias);
}


// Add a directory in the zip archive (write-only).
bool qtractorZipFile::addDirectory ( const QString& sDirectory )
{
	QString sFilename = sDirectory;
	if (!sFilename.endsWith('/'))
		sFilename.append('/');
	return m_pZip->addEntry(qtractorZipDevice::Directory, sFilename);
}


// Process the full contents of the zip archive (write-only).
bool qtractorZipFile::processAll (void)
{
	return m_pZip->processAll();
}


// Closes the zip file.
void qtractorZipFile::close (void)
{
	if (!(m_pZip->device->openMode() & QIODevice::WriteOnly)) {
		m_pZip->device->close();
		return;
	}

	m_pZip->device->seek(m_pZip->write_offset);

	// Write new directory...
	QHash<QString, FileHeader>::ConstIterator iter
		= m_pZip->file_headers.constBegin();
	const QHash<QString, FileHeader>::ConstIterator iter_end
		= m_pZip->file_headers.constEnd();
	for ( ; iter != iter_end; ++iter) {
		const FileHeader& fh = iter.value();
		m_pZip->device->write((const char *) &fh.h, sizeof(CentralFileHeader));
		m_pZip->device->write(fh.file_name);
		m_pZip->device->write(fh.extra_field);
		m_pZip->device->write(fh.file_comment);
	}

	int dir_size = m_pZip->device->pos() - m_pZip->write_offset;

	// Write end of directory...
	EndOfDirectory eod;
	memset(&eod, 0, sizeof(EndOfDirectory));
	write_uint(eod.signature, 0x06054b50);
	write_ushort(eod.num_dir_entries_this_disk, m_pZip->file_headers.size());
	write_ushort(eod.num_dir_entries, m_pZip->file_headers.size());
	write_uint(eod.directory_size, dir_size);
	write_uint(eod.dir_start_offset, m_pZip->write_offset);
	write_ushort(eod.comment_length, m_pZip->comment.length());

	m_pZip->device->write((const char *) &eod, sizeof(EndOfDirectory));
	m_pZip->device->write(m_pZip->comment);
	m_pZip->device->close();
}


// Statistical accessors.
unsigned int qtractorZipFile::totalUncompressed (void) const
{
	return m_pZip->total_uncompressed;
}

unsigned int qtractorZipFile::totalCompressed (void) const
{
	return m_pZip->total_compressed;
}

unsigned int qtractorZipFile::totalProcessed (void) const
{
	return m_pZip->total_processed;
}


#endif	// CONFIG_LIBZ

// end of qtractorZipFile.cpp
