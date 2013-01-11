// qtractorMidiFile.h
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

#ifndef __qtractorMidiFile_h
#define __qtractorMidiFile_h

#include "qtractorMidiSequence.h"

#include "qtractorMidiFileTempo.h"

class qtractorTimeScale;


//----------------------------------------------------------------------
// class qtractorMidiFile -- A SMF (Standard MIDI File) class.
//

class qtractorMidiFile
{
public:

	// Constructor.
	qtractorMidiFile();
	// Destructor.
	~qtractorMidiFile();

	// Basic file open mode.
	enum { None = 0, Read = 1, Write = 2 };

	// Open file methods.
	bool open(const QString& sFilename, int iMode = Read);
	void close();

	// Open file property accessors.
	const QString& filename() const { return m_sFilename; }
	int mode() const { return m_iMode; }

	// Header accessors.
	unsigned short format() const { return m_iFormat; }
	unsigned short tracks() const { return m_iTracks; }
	unsigned short ticksPerBeat() const { return m_iTicksPerBeat; }

	// Tempo/time-signature map accessor.
	qtractorMidiFileTempo *tempoMap() const { return m_pTempoMap; }

	// Sequence/track readers.
	bool readTracks(qtractorMidiSequence **ppSeqs, unsigned short iSeqs,
		unsigned short iTrackChannel = 0);
	bool readTrack(qtractorMidiSequence *pSeq,
		unsigned short iTrackChannel);

	// Header writer.
	bool writeHeader(unsigned short iFormat,
		unsigned short iTracks, unsigned short iTicksPerBeat);

	// Sequence/track writers.
	bool writeTracks(qtractorMidiSequence **ppSeqs, unsigned short iSeqs);
	bool writeTrack (qtractorMidiSequence *pSeq);

	// All-in-one SMF file writer/creator method.
	static bool saveCopyFile(const QString& sNewFilename,
		const QString& sOldFilename, unsigned short iTrackChannel,
		unsigned short iFormat, qtractorMidiSequence *pSeq,
		qtractorTimeScale *pTimeScale = NULL, unsigned long iTimeOffset = 0);

	// Create filename revision.
	static QString createFilePathRevision(
		const QString& sFilename, int iRevision = 0);

protected:

	// Read methods.
	int readInt   (unsigned short n = 0);
	int readData  (unsigned char *pData, unsigned short n);

	// Write methods.
	int writeInt  (int val, unsigned short n = 0);
	int writeData (unsigned char *pData, unsigned short n);

	// Write tempo-time-signature node.
	void writeNode(
		qtractorMidiFileTempo::Node *pNode, unsigned long iLastTime);

	// Write location marker.
	void writeMarker(
		qtractorMidiFileTempo::Marker *pMarker, unsigned long iLastTime);

private:

	// SMF instance variables.
	QString        m_sFilename;
	int            m_iMode;
	FILE          *m_pFile;
	unsigned long  m_iOffset;

	// Header informational data.
	unsigned short m_iFormat;
	unsigned short m_iTracks;
	unsigned short m_iTicksPerBeat;

	// Track info map.
	struct TrackInfo {
		unsigned int  length;
		unsigned long offset;
	} *m_pTrackInfo;

	// Special tempo/time-signature map.
	qtractorMidiFileTempo *m_pTempoMap;
};


#endif  // __qtractorMidiFile_h


// end of qtractorMidiFile.h
