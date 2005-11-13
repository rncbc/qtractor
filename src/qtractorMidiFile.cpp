// qtractorMidiFile.cpp
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

#include "qtractorMidiFile.h"

#include <qmap.h>


//----------------------------------------------------------------------
// class qtractorMidiFile -- A SMF (Standard MIDI File) class.
//

// Constructor.
qtractorMidiFile::qtractorMidiFile (void)
{
	// SMF instance variables.
	m_iMode         = None;
	m_pFile         = NULL;
	m_iOffset       = 0;

	// Header informational data.
	m_iFormat       = 0;
	m_iTracks       = 0;
	m_iTicksPerBeat = 0;

	m_fTempo        = 120.0;
	m_iBeatsPerBar  = 4;

	m_pTrackInfo    = NULL;
}


// Destructor.
qtractorMidiFile::~qtractorMidiFile (void)
{
	close();
}


// Open file method.
bool qtractorMidiFile::open ( const char *pszName, int iMode )
{
	close();

	if (iMode == None)
		iMode = Read;

	m_pFile = ::fopen(pszName, iMode == Write ? "w+b" : "rb");
	if (m_pFile == NULL)
		return false;

	m_sFilename = pszName;
	m_iMode     = iMode;
	m_iOffset   = 0;

	// Bail out of here, if in write mode...
	if (m_iMode == Write)
		return true;

	// First word must identify the file as a SMF;
	// must be literal "MThd"
	if (readInt(4) != 0x4d546864) {
		close();
		return false;
	}

	// Second word should be the total header chunk length...
	int iMThdLength = readInt(4);
	if (iMThdLength < 6) {
		close();
		return false;
	}

	// Read header data...
	m_iFormat = (unsigned short) readInt(2);
	m_iTracks = (unsigned short) readInt(2);
	m_iTicksPerBeat = (unsigned short) readInt(2);
	// Should skip any extra bytes...
	while (iMThdLength > 6) {
		if (::fgetc(m_pFile) < 0) {
			close();
			return false;
		}
		m_iOffset++;
		iMThdLength--;
	}

	// Allocate the track map.
	m_pTrackInfo = new TrackInfo [m_iTracks];
	for (int iTrack = 0; iTrack < m_iTracks; iTrack++) {
		// Must be a track header "MTrk"...
		if (readInt(4) != 0x4d54726b) {
			close();
			return false;
		}
		// Check track chunk length...
		int iMTrkLength = readInt(4);
		if (iMTrkLength < 0) {
			close();
			return false;
		}
		// Set this one track info.
		m_pTrackInfo[iTrack].length = iMTrkLength;
		m_pTrackInfo[iTrack].offset = m_iOffset;
		// Set next track offset...
		m_iOffset += iMTrkLength;
		// Advance to next one...
		if (::fseek(m_pFile, m_iOffset, SEEK_SET)) {
			close();
			return false;
		}
	}

	// We're in business...
	return true;
}


// Close file method.
void qtractorMidiFile::close (void)
{
	if (m_pFile) {
		::fclose(m_pFile);
		m_pFile = NULL;
	}

	if (m_pTrackInfo) {
		delete [] m_pTrackInfo;
		m_pTrackInfo = NULL;
	}
}


// Sequence/track/channel reader.
bool qtractorMidiFile::readTrack ( qtractorMidiSequence *pSeq,
	unsigned short iTrackChannel )
{
	if (m_pFile == NULL)
		return false;
	if (m_iMode != Read)
		return false;

	// If under a format 0 file, we'll filter for one single channel.
	unsigned short iTrack = (m_iFormat == 1 ? iTrackChannel : 0);
	if (iTrack >= m_iTracks)
		return false;
	unsigned short iChannelFilter = (m_iFormat == 1 ? 0xf0 : iTrackChannel);

	// Locate the desired track stuff...
	unsigned long iTrackStart = m_pTrackInfo[iTrack].offset;
	if (iTrackStart != m_iOffset) {
		if (::fseek(m_pFile, iTrackStart, SEEK_SET))
			return false;
		m_iOffset = iTrackStart;
	}

	// Track the note-ons.
	typedef QMap<unsigned char, qtractorMidiEvent *> NoteMap;
	NoteMap notes;
	
	// Now we're going into business...
	unsigned long iTrackTime = 0;
	unsigned long iTrackEnd = m_iOffset + m_pTrackInfo[iTrack].length;
	int iLastStatus = 0;
	
	// While this track lasts...
	while (m_iOffset < iTrackEnd) {

		// Read delta timestamp...
		iTrackTime += readInt();

		// Read probable status byte...
		int iStatus = readInt(1);
		// Maybe a running status byte?
		if ((iStatus & 0x80) == 0) {
			// Go back one byte...
			::ungetc(iStatus, m_pFile);
			m_iOffset--;
			iStatus = iLastStatus;
		} else {
			iLastStatus = iStatus;
		}

		unsigned short iChannel = (iStatus & 0x0f);
		qtractorMidiEvent::EventType type
			= qtractorMidiEvent::EventType(iStatus & 0xf0);
		if (iStatus == qtractorMidiEvent::META)
			type = qtractorMidiEvent::META;

		// Event time converted to sequence resolution...
		unsigned long time = (iTrackTime * pSeq->ticksPerBeat())
			/ m_iTicksPerBeat;

		qtractorMidiEvent *pEvent;
		unsigned char *data, data1, data2;
		unsigned int len, meta;

		switch (type) {
		case qtractorMidiEvent::NOTEOFF:
		case qtractorMidiEvent::NOTEON:
			data1 = readInt(1);
			data2 = readInt(1);
			// Check if its channel filtered...
			if ((iChannelFilter & 0xf0) || (iChannelFilter == iChannel)) {
				if (data2 == 0 && type == qtractorMidiEvent::NOTEON)
					type = qtractorMidiEvent::NOTEOFF;
				// Find previous note event and compute duration...
				NoteMap::Iterator iter = notes.find(data1);
				if (iter != notes.end()) {
					pSeq->setEventDuration(*iter, time - (*iter)->time());
					notes.erase(iter);
				}
				// Create the new event (only for NOTEONs)...
				if (type == qtractorMidiEvent::NOTEON) {
					pEvent = new qtractorMidiEvent(time, type, data1, data2);
					pSeq->addEvent(pEvent);
					pSeq->setChannel(iChannel);
					// Add to lingering notes...
					notes[data1] = pEvent;
				}
			}
			break;
		case qtractorMidiEvent::KEYPRESS:
		case qtractorMidiEvent::CONTROLLER:
		case qtractorMidiEvent::PITCHBEND:
			data1 = readInt(1);
			data2 = readInt(1);
			// Check if its channel filtered...
			if ((iChannelFilter & 0xf0) || (iChannelFilter == iChannel)) {
				// Create the new event...
				pEvent = new qtractorMidiEvent(time, type, data1, data2);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(iChannel);
				if (type == qtractorMidiEvent::NOTEON)
					notes[data1] = pEvent;
			}
			break;
		case qtractorMidiEvent::PGMCHANGE:
		case qtractorMidiEvent::CHANPRESS:
			data1 = 0;
			data2 = readInt(1);
			// Check if its channel filtered...
			if ((iChannelFilter & 0xf0) || (iChannelFilter == iChannel)) {
				// Create the new event...
				pEvent = new qtractorMidiEvent(time, type, data1);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(iChannel);
			}
			break;
		case qtractorMidiEvent::SYSEX:
			len = readInt();
			data = new unsigned char [len + 1];
			if (readData(data, len) < (int) len) {
				delete [] data;
				return false;
			}
			data[len] = (unsigned char) 0;
			// Check if its channel filtered...
			if ((iChannelFilter & 0xf0) || (iChannelFilter == iChannel)) {
				pEvent = new qtractorMidiEvent(time, type);
				pEvent->setSysex(data, len);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(iChannel);
			}
			delete [] data;
			break;
		case qtractorMidiEvent::META:
			meta = qtractorMidiEvent::MetaType(readInt(1));
			// Get the meta data...
			len = readInt();
			if (len > 0) {
				if (meta == qtractorMidiEvent::TEMPO) {
					m_fTempo = (60000000.0 / float(readInt(len)));
				} else {
					data = new unsigned char [len + 1];
					if (readData(data, len) < (int) len) {
						delete [] data;
						return false;
					}
					data[len] = (unsigned char) 0;
					// Now, we'll deal only with some...
					switch (meta) {
					case qtractorMidiEvent::TRACKNAME:
						pSeq->setName((const char *) data);
						break;
					case qtractorMidiEvent::TIME:
						// Beats per bar is the numerator of time signature...
						m_iBeatsPerBar = (unsigned short) data[0];
						break;
					default:
						// Ignore all others...
						break;
					}
					delete [] data;
					break;
				}
			}
			break;
		default:
			break;
		}
	}

	// Last known time converted to sequence resolution...
	unsigned long lasttime = (iTrackTime * pSeq->ticksPerBeat())
		/ m_iTicksPerBeat;
	// Finish all pending notes...
	for (NoteMap::Iterator iter = notes.begin();
			iter != notes.end(); ++iter) {
		pSeq->setEventDuration(*iter, lasttime - (*iter)->time());
	}

	return true;
}


// Sequence/track writer.
bool qtractorMidiFile::writeTrack ( qtractorMidiSequence *pSeq )
{
	if (m_pFile == NULL)
		return false;
	if (m_iMode != Write)
		return false;

	return true;
}



// Integer read method.
int qtractorMidiFile::readInt ( unsigned short n )
{
	int c, val = 0;
	
	if (n > 0) {
		// Fixed length (n bytes) integer read.
		for (int i = 0; i < n; i++) {
			val <<= 8;
			c = ::fgetc(m_pFile);
			if (c < 0)
				return -1;
			val |= c;
			m_iOffset++;
		}
	} else {
		// Variable length integer read.
		do {
			c = ::fgetc(m_pFile);
			if (c < 0)
				return -1;
			val <<= 7;
			val |= (c & 0x7f);
			m_iOffset++;
		}
		while ((c & 0x80) == 0x80);
	}

	return val;
}


// Raw data read method.
int qtractorMidiFile::readData ( unsigned char *pData, unsigned short n )
{
	int nread = ::fread(pData, sizeof(unsigned char), n, m_pFile);
	if (nread > 0)
		m_iOffset += nread;
	return nread;
}


// Integer write method.
int qtractorMidiFile::writeInt ( int val, unsigned short n )
{
	int c;

	if (n > 0) {
		// Fixed length (n bytes) integer write.
		for (int i = n; i > 0; i--) {
			c = val & (0xff << (i * 8));
			if (--i > 0)
				c >>= (i * 8);
			if (::fputc(c & 0xff, m_pFile) < 0)
				return -1;
			m_iOffset++;
		}
	} else {
		// Variable length integer write.
		n = 0;
		c = val & 0x7f;
		while (val >>= 7) {
			c <<= 8;
			c |= ((val & 0x7f) | 0x80);
		}
		if (::fputc(c & 0xff, m_pFile) < 0)
			return -1;
		n++;
		while (c & 0x80) {
			c >>= 8;
			if (::fputc(c & 0xff, m_pFile) < 0)
				return -1;
			n++;
		}
		m_iOffset += n;
	}

	return n;
}


// Raw data write method.
int qtractorMidiFile::writeData ( unsigned char *pData, unsigned short n )
{
	int nwrite = ::fwrite(pData, sizeof(unsigned char), n, m_pFile);
	if (nwrite > 0)
		m_iOffset += nwrite;
	return nwrite;
}


// end of qtractorMidiFile.h
