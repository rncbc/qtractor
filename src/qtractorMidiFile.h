// qtractorMidiFile.h
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

#ifndef __qtractorMidiFile_h
#define __qtractorMidiFile_h

#include "qtractorMidiSequence.h"


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
	bool open(const char *pszName, int iMode = Read);
	void close();

	// Open file property accessors.
	const QString& filename() const { return m_sFilename; }
	int mode() const { return m_iMode; }

	// Header accessors.
	unsigned short format() const { return m_iFormat; }
	unsigned short tracks() const { return m_iTracks; }
	unsigned short ticksPerBeat() const { return m_iTicksPerBeat; }

	// Sequence/track tempo (BPM) accessors.
	void setTempo(float fTempo)
		{ m_fTempo = fTempo; }
	float tempo() const { return m_fTempo; }

	// Sequence/track beats per bar accessors.
	void setBeatsPerBar(unsigned short iBeatsPerBar)
		{ m_iBeatsPerBar = iBeatsPerBar; }
	unsigned short beatsPerBar() const { return m_iBeatsPerBar; }

	// Sequence/track reader.
	bool readTrack (qtractorMidiSequence *pSeq,
		unsigned short iTrackChannel);		

	// Sequence/track writers.
	bool writeTrack (qtractorMidiSequence *pSeq);

	// Header writer.
	bool writeHeader(unsigned short iFormat,
		unsigned short iTracks, unsigned short iTicksPerBeat);

protected:

	// Read methods.
	int readInt   (unsigned short n = 0);
	int readData  (unsigned char *pData, unsigned short n);

	// Write methods.
	int writeInt  (int val, unsigned short n = 0);
	int writeData (unsigned char *pData, unsigned short n);

	int sizeInt   (int val);
	
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

	// Special uniform track properties.
	float          m_fTempo;
	unsigned short m_iBeatsPerBar;

	// Track info map.
	struct TrackInfo {
		unsigned int  length;
		unsigned long offset;
	} *m_pTrackInfo;
};


#endif  // __qtractorMidiFile_h


// end of qtractorMidiFile.h