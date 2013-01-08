// qtractorZipFile.h
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

/* Most of this code was originally borrowed, stirred, mangled
 * and finally adapted from the Qt 4.6 source code (LGPL).
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(ies).
 * All rights reserved.
 * Contact: Nokia Corporation (qt-info@nokia.com)
 */

#ifndef __qtractorZipFile_h
#define __qtractorZipFile_h

#include <QFile>


//----------------------------------------------------------------------------
// qtractorZipFile  -- Custom ZIP file archive class.
//

class qtractorZipDevice;

class qtractorZipFile
{
public:

	// Constructors.
	qtractorZipFile(const QString& sFilename,
		QIODevice::OpenMode mode = QIODevice::ReadOnly);

	explicit qtractorZipFile(QIODevice *pDdevice);

	// Destructor.
	~qtractorZipFile();

	enum Status {
		NoError = 0,
		FileOpenError,
		FileReadError,
		FileWriteError,
		FilePermissionsError,
		FileError
	};

	Status status() const;

	bool isReadable() const;
	bool isWritable() const;

	bool exists() const;

	bool extractFile(const QString& sFilename);
	bool extractAll();

	QString alias(const QString& sFilename,
		const QString& sPrefix = QString()) const;

	bool addFile(const QString& sFilename,
		const QString& sAlias = QString());
	bool addDirectory(const QString& sDirectory);

	bool processAll();

	void close();

	unsigned int totalUncompressed() const;
	unsigned int totalCompressed() const;
	unsigned int totalProcessed() const;

private:

	// Disable copy constructor.
	qtractorZipFile(const qtractorZipFile&);

	// Implementation device.
	qtractorZipDevice *m_pZip;
};


#endif // __qtractorZipFile_h

// end of qtractorZipFile.h
