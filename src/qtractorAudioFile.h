// qtractorAudioFile.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioFile_h
#define __qtractorAudioFile_h

#include <QStringList>
#include <QHash>


//----------------------------------------------------------------------
// class qtractorAudioFile -- Abstract audio file mockup.
//

class qtractorAudioFile
{
public:

	// Virtual destructor.
	virtual ~qtractorAudioFile() {}

	// Basic file open mode.
	enum { None = 0, Read = 1, Write = 2 };

	// Pure virtual method mockups.
	virtual bool open  (const QString& sFilename, int iMode = Read) = 0;
	virtual int  read  (float **ppFrames, unsigned int iFrames) = 0;
	virtual int  write (float **ppFrames, unsigned int iFrames) = 0;
	virtual bool seek  (unsigned long iOffset) = 0;
	virtual void close () = 0;

	// Pure virtual accessor mockups.
	virtual int mode() const = 0;

	// These shall give us a clue on the size
	// of the ring buffer size (in frames).
	virtual unsigned short channels() const = 0;
	virtual unsigned long frames() const = 0;

	// Other special informational methods.
	virtual unsigned int sampleRate() const = 0;
};


//----------------------------------------------------------------------
// class qtractorAudioFileFactory -- Audio file factory (singleton).
//

class qtractorAudioFileFactory
{
public:

	// Supported file types.
	enum FileType { SndFile, VorbisFile, MadFile };

	// Factory methods.
	static qtractorAudioFile *createAudioFile (
		const QString& sFilename, unsigned short iChannels = 0,
		unsigned int iSampleRate = 0, unsigned int iBufferSize = 0);
	static qtractorAudioFile *createAudioFile (
		FileType type, unsigned short iChannels = 0,
		unsigned int iSampleRate = 0, unsigned int iBufferSize = 0);

	// Audio file format descriptor.
	struct FileFormat
	{
		FileType type;
		QString  name;
		QString  ext;
		int      data;
	};

	typedef QList<FileFormat *> FileFormats;

	// File type/format global accessors.
	static const FileFormats& formats();

	// Retrieve supported filters (suitable for QFileDialog usage).
	static QString filters();

	// Default audio file format accessors
	// (specific to capture/recording)
	static void setDefaultType(const QString& sExt, int iType,
		int iFormat = 0, int iQuality = 4);

	static QString defaultExt();
	static int defaultFormat();
	static int defaultQuality();

	// Check whether given file type/format is valid.
	static bool isValidFormat(const FileFormat *pFormat, int iFormat);

	// Singleton destroyer.
	static void Destroy();

protected:

	// Constructor.
	qtractorAudioFileFactory();

	// Singleton instance accessor.
	static qtractorAudioFileFactory& getInstance();

	// Instance factory methods.
	qtractorAudioFile *newAudioFile (
		const QString& sFilename, unsigned short iChannels,
		unsigned int iSampleRate, unsigned int iBufferSize);
	qtractorAudioFile *newAudioFile (
		FileType type, unsigned short iChannels,
		unsigned int iSampleRate, unsigned int iBufferSize);

	// Translate format index into libsndfile specific...
	static int format(const FileFormat *pFormat, int iFormat);

private:

	// The singleton instance.
	static qtractorAudioFileFactory* g_pInstance;

	// The supported file types/extension map.
	typedef QHash<QString, FileFormat *> FileTypes;

	FileFormats m_formats;
	FileTypes   m_types;

	// Supported filter strings.
	QStringList m_filters;
	
	// Default file format/type (for capture/record)
	FileFormat *m_pDefaultFormat;
	int m_iDefaultFormat;
	int m_iDefaultQuality;
};


#endif  // __qtractorAudioFile_h


// end of qtractorAudioFile.h
