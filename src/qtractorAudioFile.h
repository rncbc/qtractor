// qtractorAudioFile.h
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorAudioFile_h
#define __qtractorAudioFile_h

#include "qtractorAbout.h"
#include "qtractorRingBuffer.h"

#include <qstringlist.h>
#include <qmap.h>


// Forward declarations.
class qtractorAudioFileFactory;

// Declare audio buffer file type.
// typedef qtractorRingBufferFile<float> qtractorAudioFile;
class qtractorAudioFile : public qtractorRingBufferFile<float> {};


//----------------------------------------------------------------------
// class qtractorAudioBuffer -- A special case of an audio ring-buffer audio.
//

class qtractorAudioBuffer : public qtractorRingBuffer<float>
{
public:

	// Overriden open method.
	bool open(const char *pszName, int iMode = qtractorAudioFile::Read);
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
		const QString& sFilename, unsigned int iBufferSize = 0);
	static qtractorAudioFile *createAudioFile (
		FileType type, unsigned int iBufferSize = 0);

	// Retrieve supported filters (suitable for QFileDialog usage).
	static QString filters();

	// Singleton destroyer.
	static void Destroy();

protected:

	// Constructor.
	qtractorAudioFileFactory();

	// Singleton instance accessor.
	static qtractorAudioFileFactory& Instance();

	// Instance factory methods.
	qtractorAudioFile *newAudioFile (
		const QString& sFilename, unsigned int iBufferSize);
	qtractorAudioFile *newAudioFile (
		FileType type, unsigned int iBufferSize);

private:

	// The singleton instance.
	static qtractorAudioFileFactory* g_pInstance;

	// The supported file types/extension map.
	typedef QMap<QString, FileType> FileTypes;
	FileTypes m_types;

	// Supported filter strings.
	QStringList m_filters;
};


#endif  // __qtractorAudioFile_h


// end of qtractorAudioFile.h
