// qtractorMidiFile.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiFile.h"

#include <QFileInfo>

// Symbolic header markers.
#define SMF_MTHD 0x4d546864
#define SMF_MTRK 0x4d54726b


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

	m_fTempo        = 120.0f;
	m_iBeatsPerBar  = 4;

	m_pTrackInfo    = NULL;
}


// Destructor.
qtractorMidiFile::~qtractorMidiFile (void)
{
	close();
}


// Open file method.
bool qtractorMidiFile::open ( const QString& sFilename, int iMode )
{
	close();

	if (iMode == None)
		iMode = Read;

	QByteArray aFilename = sFilename.toUtf8();
	m_pFile = ::fopen(aFilename.constData(), iMode == Write ? "w+b" : "rb");
	if (m_pFile == NULL)
		return false;

	m_sFilename = sFilename;
	m_iMode     = iMode;
	m_iOffset   = 0;

	// Bail out of here, if in write mode...
	if (m_iMode == Write)
		return true;

	// First word must identify the file as a SMF;
	// must be literal "MThd"
	if (readInt(4) != SMF_MTHD) {
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
		if (readInt(4) != SMF_MTRK) {
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

	// Now we're going into business...
	unsigned long iTrackTime  = 0;
	unsigned long iTrackEnd   = m_iOffset + m_pTrackInfo[iTrack].length;
	unsigned int  iLastStatus = 0;
	
	// While this track lasts...
	while (m_iOffset < iTrackEnd) {

		// Read delta timestamp...
		iTrackTime += readInt();

		// Read probable status byte...
		unsigned int iStatus = readInt(1);
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
		unsigned long iTime = (iTrackTime * pSeq->ticksPerBeat())
			/ m_iTicksPerBeat;

		// Check for sequence time length, if any...
		if (pSeq->timeLength() > 0
			&& iTime > pSeq->timeOffset() + pSeq->timeLength())
			break;

		// Check whether it won't be channel filtered...
		bool bChannelEvent = (iTime >= pSeq->timeOffset()
			&& ((iChannelFilter & 0xf0) || (iChannelFilter == iChannel)));

		qtractorMidiEvent *pEvent;
		unsigned char *data, data1, data2;
		unsigned int len, meta, bank;

		switch (type) {
		case qtractorMidiEvent::NOTEOFF:
		case qtractorMidiEvent::NOTEON:
			data1 = readInt(1);
			data2 = readInt(1);
			// Check if its channel filtered...
			if (bChannelEvent) {
				iTime -= pSeq->timeOffset();
				if (data2 == 0 && type == qtractorMidiEvent::NOTEON)
					type = qtractorMidiEvent::NOTEOFF;
				pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(iChannel);
			}
			break;
		case qtractorMidiEvent::CONTROLLER:
			data1 = readInt(1);
			data2 = readInt(1);
			// Check if its channel filtered...
			if (bChannelEvent) {
				iTime -= pSeq->timeOffset();
				// We don't sequence bank select events here,
				// just set the primordial bank patch...
				switch (data1) {
				case 0x00:
					// Bank MSB...
					bank = (pSeq->bank() < 0 ? 0 : (pSeq->bank() & 0x007f));
					pSeq->setBank(bank | (data2 << 7));
					break;
				case 0x20:
					// Bank LSB...
					bank = (pSeq->bank() < 0 ? 0 : (pSeq->bank() & 0x3f80));
					pSeq->setBank(bank | data2);
					break;
				default:
					// Create the new event...
					pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
					pSeq->addEvent(pEvent);
					break;
				}
				pSeq->setChannel(iChannel);
			}
			break;
		case qtractorMidiEvent::KEYPRESS:
		case qtractorMidiEvent::PITCHBEND:
			data1 = readInt(1);
			data2 = readInt(1);
			// Check if its channel filtered...
			if (bChannelEvent) {
				iTime -= pSeq->timeOffset();
				// Create the new event...
				pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(iChannel);
			}
			break;
		case qtractorMidiEvent::PGMCHANGE:
			data1 = 0;
			data2 = readInt(1);
			// Check if its channel filtered...
			if (bChannelEvent) {
				iTime -= pSeq->timeOffset();
				// We don't sequence prog change events here,
				// just set the primordial program patch...
				if (pSeq->program() < 0)
					pSeq->setProgram(data2);
				pSeq->setChannel(iChannel);
			}
			break;
		case qtractorMidiEvent::CHANPRESS:
			data1 = 0;
			data2 = readInt(1);
			// Check if its channel filtered...
			if (bChannelEvent) {
				iTime -= pSeq->timeOffset();
				// Create the new event...
				pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(iChannel);
			}
			break;
		case qtractorMidiEvent::SYSEX:
			len = readInt();
			data = new unsigned char [1 + len];
			data[0] = (unsigned char) type;	// Skip 0xf0 head.
			if (readData(&data[1], len) < (int) len) {
				delete [] data;
				return false;
			}
			// Check if its channel filtered...
			if (bChannelEvent) {
				iTime -= pSeq->timeOffset();
				pEvent = new qtractorMidiEvent(iTime, type);
				pEvent->setSysex(data, 1 + len);
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
					m_fTempo = (60000000.0f / float(readInt(len)));
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
						pSeq->setName(QString((const char *) data).simplified());
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

	// FIXME: Commit the sequence length...
	pSeq->close();

	return true;
}


// Header writer.
bool qtractorMidiFile::writeHeader ( unsigned short iFormat,
	unsigned short iTracks, unsigned short iTicksPerBeat )
{
	if (m_pFile == NULL)
		return false;
	if (m_iMode != Write)
		return false;

	// First word must identify the file as a SMF;
	// must be literal "MThd"
	writeInt(SMF_MTHD, 4);

	// Second word should be the total header chunk length...
	writeInt(6, 4);

	// Write header data (6 bytes)...
	writeInt(m_iFormat = iFormat, 2);
	writeInt(m_iTracks = iTracks, 2);
	writeInt(m_iTicksPerBeat = iTicksPerBeat, 2);
	
	// Assume all is fine.
	return true;
}


// Sequence/track writer.
bool qtractorMidiFile::writeTrack ( qtractorMidiSequence *pSeq )
{
	if (m_pFile == NULL)
		return false;
	if (m_iMode != Write)
		return false;

	// Must be a track header "MTrk"...
	writeInt(SMF_MTRK, 4);

	// Write a dummy track length (we'll overwrite it later)...
	unsigned long iMTrkOffset = m_iOffset;
	writeInt(0, 4);

	// Track name...
	QString sTrackName
		= (pSeq ? pSeq->name() : QFileInfo(m_sFilename).baseName());
	writeInt(0); // delta-time=0
	writeInt(qtractorMidiEvent::META, 1);
	writeInt(qtractorMidiEvent::TRACKNAME, 1);
	writeInt(sTrackName.length());
	QByteArray aTrackName = sTrackName.toUtf8();
	writeData((unsigned char *) aTrackName.constData(), aTrackName.length());

	// Write basic META data...
	if (m_iFormat == 0 || pSeq == NULL) {
		// Tempo...
		writeInt(0); // delta-delta=0
		writeInt(qtractorMidiEvent::META, 1);
		writeInt(qtractorMidiEvent::TEMPO, 1);
		writeInt(3);
		writeInt(int(60000000.0f / m_fTempo), 3);
		// Time signature...
		writeInt(0); // delta-time=0
		writeInt(qtractorMidiEvent::META, 1);
		writeInt(qtractorMidiEvent::TIME, 1);
		writeInt(4);
		writeInt(m_iBeatsPerBar, 1);    // Numerator.
		writeInt(2, 1);                 // Denominator.
		writeInt(32, 1);                // MIDI clocks per metronome click.
		writeInt(4, 1);                 // 32nd notes per quarter.
	}

	// SMF format 1 files must have events
	// which should be just writen down... 
	if (pSeq) {
		// Write the whole sequence out...
		unsigned int  iStatus;
		unsigned int  iLastStatus = 0;
		unsigned long iLastTime = 0;
		unsigned long iTimeOff;
		unsigned char *data;
		unsigned int len;
		qtractorMidiEvent *pNoteAfter;
		qtractorMidiEvent *pNoteOff;
		qtractorList<qtractorMidiEvent> notesOff;
		notesOff.setAutoDelete(true);
		for (qtractorMidiEvent *pEvent = pSeq->events().first();
				pEvent; pEvent = pEvent->next()) {
			// Event (absolute) time converted to file resolution...
			unsigned long iTime = (pEvent->time() * m_iTicksPerBeat)
				/ pSeq->ticksPerBeat();
			// Check for pending note-offs...
			while ((pNoteOff = notesOff.first()) != NULL
					&& iTime >= pNoteOff->time()) {
				// - Delta time...
				iTimeOff = pNoteOff->time();
				writeInt(iTimeOff > iLastTime ? iTimeOff - iLastTime : 0);
				iLastTime = iTimeOff;
				// - Status byte...
				iStatus = (pNoteOff->type() | pSeq->channel()) & 0xff;
				// - Running status...
				if (iStatus != iLastStatus) {
					writeInt(iStatus, 1);
					iLastStatus = iStatus;
				}
				// - Data bytes...
				writeInt(pNoteOff->note(), 1);
				writeInt(pNoteOff->velocity(), 1);
				// Remove from note-off list and continue...
				notesOff.remove(pNoteOff);
			}
			// Let's get back to actual event...
			// - Delta time...
			writeInt(iTime > iLastTime ? iTime - iLastTime : 0);
			iLastTime = iTime;
			// - Status byte...
			iStatus = (pEvent->type() | pSeq->channel()) & 0xff;
			// - Running status?
			if (iStatus != iLastStatus) {
				writeInt(iStatus, 1);
				iLastStatus = iStatus;
			}
			// - Data bytes...
			switch (pEvent->type()) {
			case qtractorMidiEvent::NOTEON:
				writeInt(pEvent->note(), 1);
				writeInt(pEvent->velocity(), 1);
				iTimeOff = (pEvent->time() + pEvent->duration());
				pNoteOff = new qtractorMidiEvent(
					(iTimeOff * m_iTicksPerBeat) / pSeq->ticksPerBeat(),
					qtractorMidiEvent::NOTEOFF,
					pEvent->note());
				// Find the proper position in notes-off list ...
				pNoteAfter = notesOff.last();
				while (pNoteAfter && pNoteAfter->time() > pNoteOff->time())
					pNoteAfter = pNoteAfter->prev();
				if (pNoteAfter)
					notesOff.insertAfter(pNoteOff, pNoteAfter);
				else
					notesOff.prepend(pNoteOff);
				break;
			case qtractorMidiEvent::CONTROLLER:
				writeInt(pEvent->controller(), 1);
				writeInt(pEvent->value(), 1);
				break;
			case qtractorMidiEvent::KEYPRESS:
			case qtractorMidiEvent::PITCHBEND:
				writeInt(pEvent->note(), 1);
				writeInt(pEvent->value(), 1);
				break;
			case qtractorMidiEvent::PGMCHANGE:
			case qtractorMidiEvent::CHANPRESS:
				writeInt(pEvent->value(), 1);
				break;
			case qtractorMidiEvent::SYSEX:
				data = pEvent->sysex() + 1;	// Skip 0xf0 head.
				len  = pEvent->sysex_len() - 1;
				writeInt(len);
				writeData(data, len);
				break;
			default:
				break;
			}
		}
		// And all remaining note-offs...
		while ((pNoteOff = notesOff.first()) != NULL) {
			// - Delta time...
			iTimeOff = pNoteOff->time();
			writeInt(iTimeOff > iLastTime ? iTimeOff - iLastTime : 0);
			iLastTime = iTimeOff;
			// - Status byte...
			iStatus = (pNoteOff->type() | pSeq->channel()) & 0xff;
			// - Running status...
			if (iStatus != iLastStatus) {
				writeInt(iStatus, 1);
				iLastStatus = iStatus;
			}
			// - Data bytes...
			writeInt(pNoteOff->note(), 1);
			writeInt(pNoteOff->velocity(), 1);
			// Remove from note-off list and continue...
			notesOff.remove(pNoteOff);
		}
		// Done.
	}

	// End-of-track marker.
	writeInt(0); // delta-time=0
	writeInt(qtractorMidiEvent::META, 1);
	writeInt(qtractorMidiEvent::EOT, 1);
	writeInt(0); // length=0;

	// Time to overwrite the actual track length...
	if (::fseek(m_pFile, iMTrkOffset, SEEK_SET))
		return false;

	// Do it...
	writeInt(m_iOffset - (iMTrkOffset + 4), 4);
	m_iOffset -= 4;

	// Restore file position to end-of-file...	
	return (::fseek(m_pFile, m_iOffset, SEEK_SET) == 0);
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
		for (int i = (n - 1) * 8; i >= 0; i -= 8) {
			c = (val & (0xff << i)) >> i;
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
			c |= (val & 0x7f) | 0x80;
		}
		while (true) {
			if (::fputc(c & 0xff, m_pFile) < 0)
				return -1;
			n++;
			if ((c & 0x80) == 0)
				break;
			c >>= 8;
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
