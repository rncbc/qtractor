// qtractorMidiFile.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiFile.h"

#include "qtractorTimeScale.h"

#include "qtractorMidiRpn.h"

#include <QFileInfo>
#include <QDir>
#include <QRegExp>


// Symbolic header markers.
#define SMF_MTHD "MThd"
#define SMF_MTRK "MTrk"


// - Bank-select (controller) types...
#define BANK_MSB  0x00
#define BANK_LSB  0x20

// - Extra-ordinary (controller) types...
#define RPN_MSB   0x65
#define RPN_LSB   0x64

#define NRPN_MSB  0x63
#define NRPN_LSB  0x62

#define DATA_MSB  0x06
#define DATA_LSB  0x26

//----------------------------------------------------------------------
// class qtractorMidiFileRpn -- MIDI RPN/NRPN file parser.
//
class qtractorMidiFileRpn : public qtractorMidiRpn
{
public:

	// Constructor.
	qtractorMidiFileRpn() : qtractorMidiRpn() {}

	// Encoder.
	bool process ( unsigned long time, int port,
		unsigned char status, unsigned short param, unsigned short value )
	{
		qtractorMidiRpn::Event event;

		event.time   = time;
		event.port   = port;
		event.status = status;
		event.param  = param;
		event.value  = value;

		return qtractorMidiRpn::process(event);
	}

	// Decoder.
	void dequeue ( qtractorMidiSequence *pSeq )
	{
		while (qtractorMidiRpn::isPending()) {
			qtractorMidiRpn::Event event;
			if (qtractorMidiRpn::dequeue(event)) {
				qtractorMidiEvent::EventType type;
				switch (qtractorMidiRpn::Type(event.status & 0xf0)) {
				case qtractorMidiRpn::CC:
					type = qtractorMidiEvent::CONTROLLER;
					break;
				case qtractorMidiRpn::RPN:
					type = qtractorMidiEvent::REGPARAM;
					break;
				case qtractorMidiRpn::NRPN:
					type = qtractorMidiEvent::NONREGPARAM;
					break;
				case qtractorMidiRpn::CC14:
					type = qtractorMidiEvent::CONTROL14;
					break;
				default:
					continue;
				}
				qtractorMidiEvent *pEvent = new qtractorMidiEvent(
					event.time, type, event.param, event.value);
				pSeq->addEvent(pEvent);
				pSeq->setChannel(event.status & 0x0f);
			}
		}
	}
};



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

	m_pTrackInfo    = NULL;

	// Special tempo/time-signature map.
	m_pTempoMap     = NULL;
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
	char header[5];
	readData((unsigned char *) &header[0], 4); header[4] = (char) 0;
	if (::strcmp(header, SMF_MTHD)) {
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
		++m_iOffset;
		--iMThdLength;
	}

	// Allocate the track map.
	m_pTrackInfo = new TrackInfo [m_iTracks];
	for (int iTrack = 0; iTrack < m_iTracks; ++iTrack) {
		// Must be a track header "MTrk"...
		readData((unsigned char *) &header[0], 4); header[4] = (char) 0;
		if (::strcmp(header, SMF_MTRK)) {
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

	// Special tempo/time-signature map.
	m_pTempoMap = new qtractorMidiFileTempo(this);

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

	if (m_pTempoMap) {
		delete m_pTempoMap;
		m_pTempoMap = NULL;
	}
}


// Sequence/track/channel readers.
bool qtractorMidiFile::readTracks ( qtractorMidiSequence **ppSeqs,
	unsigned short iSeqs, unsigned short iTrackChannel )
{
	if (m_pFile == NULL)
		return false;
	if (m_pTempoMap == NULL)
		return false;
	if (m_iMode != Read)
		return false;

	// Expedite RPN/NRPN controllers processor...
	qtractorMidiFileRpn xrpn;

	// So, how many tracks are we reading in a row?...
	unsigned short iSeqTracks = (iSeqs > 1 ? m_iTracks : 1);

	// Go fetch them...
	for (unsigned short iSeqTrack = 0; iSeqTrack < iSeqTracks; ++iSeqTrack) {

		// If under a format 0 file, we'll filter for one single channel.
		if (iSeqTracks > 1)
			iTrackChannel = iSeqTrack;
		unsigned short iTrack = (m_iFormat == 1 ? iTrackChannel : 0);
		if (iTrack >= m_iTracks)
			return false;
		unsigned short iChannelFilter = iTrackChannel;
		if (m_iFormat == 1 || iSeqs > 1)
			iChannelFilter = 0xf0;

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
		unsigned long iTimeout    = 0;

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
				--m_iOffset;
				iStatus = iLastStatus;
			} else {
				iLastStatus = iStatus;
			}

			qtractorMidiEvent *pEvent;
			unsigned short iChannel = (iStatus & 0x0f);
			qtractorMidiEvent::EventType type
				= qtractorMidiEvent::EventType(iStatus & 0xf0);
			if (iStatus == qtractorMidiEvent::META)
				type = qtractorMidiEvent::META;

			// Make proper sequence reference...
			unsigned short iSeq = 0;
			if (iSeqs > 1)
				iSeq = (m_iFormat == 0 ? iChannel : iTrack);
			qtractorMidiSequence *pSeq = ppSeqs[iSeq];

			// Event time converted to sequence resolution...
			unsigned long iTime = pSeq->timeq(iTrackTime, m_iTicksPerBeat);

			// Check for sequence time length, if any...
			if (pSeq->timeLength() > 0
				&& iTime >= pSeq->timeOffset() + pSeq->timeLength()) {
				xrpn.flush();
				xrpn.dequeue(pSeq);
				break;
			}

			// Flush/timeout RPN/NRPN stuff...
			if (iTimeout < iTime || type != qtractorMidiEvent::CONTROLLER) {
				iTimeout = iTime + (pSeq->ticksPerBeat() >> 2);
				xrpn.flush();
			}

			// Check whether it won't be channel filtered...
			bool bChannelEvent = (iTime >= pSeq->timeOffset()
				&& ((iChannelFilter & 0xf0) || (iChannelFilter == iChannel)));

			unsigned char *data, data1, data2;
			unsigned int len, meta, bank;

			switch (type) {
			case qtractorMidiEvent::NOTEOFF:
			case qtractorMidiEvent::NOTEON:
				data1 = readInt(1);
				data2 = readInt(1);
				// Check if its channel filtered...
				if (bChannelEvent) {
					if (data2 == 0 && type == qtractorMidiEvent::NOTEON)
						type = qtractorMidiEvent::NOTEOFF;
					pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
					pSeq->addEvent(pEvent);
					pSeq->setChannel(iChannel);
				}
				break;
			case qtractorMidiEvent::KEYPRESS:
				data1 = readInt(1);
				data2 = readInt(1);
				// Check if its channel filtered...
				if (bChannelEvent) {
					// Create the new event...
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
					// Check for RPN/NRPN stuff...
					if (xrpn.process(iTime, iSeqTrack,
						(qtractorMidiRpn::CC | iChannel), data1, data2)) {
						iTimeout = iTime + (pSeq->ticksPerBeat() >> 2);
						break;
					}
					// Create the new event...
					pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
					pSeq->addEvent(pEvent);
					pSeq->setChannel(iChannel);
					// Set the primordial bank patch...
					switch (data1) {
					case BANK_MSB:
						// Bank MSB...
						bank = (pSeq->bank() < 0 ? 0 : (pSeq->bank() & 0x007f));
						pSeq->setBank(bank | (data2 << 7));
						break;
					case BANK_LSB:
						// Bank LSB...
						bank = (pSeq->bank() < 0 ? 0 : (pSeq->bank() & 0x3f80));
						pSeq->setBank(bank | data2);
						break;
					default:
						break;
					}
				}
				break;
			case qtractorMidiEvent::PGMCHANGE:
				data1 = 0;
				data2 = readInt(1);
				// Check if its channel filtered...
				if (bChannelEvent) {
					// Create the new event...
					pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
					pSeq->addEvent(pEvent);
					pSeq->setChannel(iChannel);
					// Set the primordial program patch...
					if (pSeq->prog() < 0)
						pSeq->setProg(data2);
				}
				break;
			case qtractorMidiEvent::CHANPRESS:
				data1 = 0;
				data2 = readInt(1);
				// Check if its channel filtered...
				if (bChannelEvent) {
					// Create the new event...
					pEvent = new qtractorMidiEvent(iTime, type, data1, data2);
					pSeq->addEvent(pEvent);
					pSeq->setChannel(iChannel);
				}
				break;
			case qtractorMidiEvent::PITCHBEND:
				data1 = readInt(1);
				data2 = readInt(1);
				// Check if its channel filtered...
				if (bChannelEvent) {
					const unsigned short value = (data2 << 7) | data1;
					// Create the new event...
					pEvent = new qtractorMidiEvent(iTime, type, 0, value);
					pSeq->addEvent(pEvent);
					pSeq->setChannel(iChannel);
				}
				break;
			case qtractorMidiEvent::SYSEX:
				len = readInt();
				if ((int) len < 1) {
					m_iOffset = iTrackEnd; // Force EoT!
					break;
				}
				data = new unsigned char [1 + len];
				data[0] = (unsigned char) type;	// Skip 0xf0 head.
				if (readData(&data[1], len) < (int) len) {
					delete [] data;
					return false;
				}
				// Check if its channel filtered...
				if (bChannelEvent) {
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
				if ((int) len < 1) {
				//	m_iOffset = iTrackEnd; // Force EoT!
					break;
				}
				if (meta == qtractorMidiEvent::TEMPO) {
					m_pTempoMap->addNodeTempo(iTime,
						qtractorTimeScale::uroundf(
							60000000.0f / float(readInt(len))));
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
						if ((unsigned short) data[0] > 0) {
							m_pTempoMap->addNodeTime(iTime,
								(unsigned short) data[0],
								(unsigned short) data[1]);
						}
						break;
					case qtractorMidiEvent::MARKER:
						m_pTempoMap->addMarker(iTime,
							QString((const char *) data).simplified());
						break;
					default:
						// Ignore all others...
						break;
					}
					delete [] data;
				}
				// Fall thru...
			default:
				break;
			}

			// Flush/pending RPN/NRPN stuff...
			xrpn.dequeue(pSeq);
		}
	}

	// FIXME: Commit the sequence(s) length...
	for (unsigned short iSeq = 0; iSeq < iSeqs; ++iSeq)
		ppSeqs[iSeq]->close();

#ifdef CONFIG_DEBUG_0
	for (unsigned short iSeq = 0; iSeq < iSeqs; ++iSeq) {
		qtractorMidiSequence *pSeq = ppSeqs[iSeq];
		qDebug("qtractorMidiFile::readTrack([%u]%p,%u,%u)"
			" name=\"%s\" events=%d duration=%lu",
			iSeq, pSeq, iSeqs, iTrackChannel,
			pSeq->name().toUtf8().constData(),
			pSeq->events().count(), pSeq->duration());
	}
#endif

	return true;
}

bool qtractorMidiFile::readTrack ( qtractorMidiSequence *pSeq,
	unsigned short iTrackChannel )
{
	return readTracks(&pSeq, 1, iTrackChannel);
}



// Header writer.
bool qtractorMidiFile::writeHeader ( unsigned short iFormat,
	unsigned short iTracks, unsigned short iTicksPerBeat )
{
	if (m_pFile == NULL)
		return false;
	if (m_pTempoMap)
		return false;
	if (m_iMode != Write)
		return false;

	// SMF format 0 sanitization...
	if (iFormat == 0)
		iTracks = 1;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiFile::writeHeader(%u,%u,%u)",
		iFormat, iTracks, iTicksPerBeat);
#endif

	// First word must identify the file as a SMF;
	// must be literal "MThd"
	writeData((unsigned char *) SMF_MTHD, 4);

	// Second word should be the total header chunk length...
	writeInt(6, 4);

	// Write header data (6 bytes)...
	writeInt(m_iFormat = iFormat, 2);
	writeInt(m_iTracks = iTracks, 2);
	writeInt(m_iTicksPerBeat = iTicksPerBeat, 2);

	// Special tempo/time-signature map.
	m_pTempoMap = new qtractorMidiFileTempo(this);

	// Assume all is fine.
	return true;
}


// Sequence/track writers.
bool qtractorMidiFile::writeTracks ( qtractorMidiSequence **ppSeqs,
	unsigned short iSeqs )
{
	if (m_pFile == NULL)
		return false;
	if (m_pTempoMap == NULL)
		return false;
	if (m_iMode != Write)
		return false;

#ifdef CONFIG_DEBUG_0
	if (ppSeqs == NULL)
		qDebug("qtractorMidiFile::writeTrack(NULL,%u)", iSeqs);
	for (unsigned short iSeq = 0; ppSeqs && iSeq < iSeqs; ++iSeq) {
		qtractorMidiSequence *pSeq = ppSeqs[iSeq];
		qDebug("qtractorMidiFile::writeTrack([%u]%p,%u)"
			" name=\"%s\" events=%d duration=%lu",
			iSeq, pSeq, iSeqs,
			pSeq->name().toUtf8().constData(),
			pSeq->events().count(), pSeq->duration());
	}
#endif

	// So, how many tracks are we reading in a row?...
	unsigned short iTracks = (iSeqs > 1 ? m_iTracks : 1);

	// Go fetch them...
	for (unsigned short iTrack = 0; iTrack < iTracks; ++iTrack) {

		// Make proper initial sequence reference...
		unsigned short iSeq = 0;
		if (iSeqs > 1 && m_iFormat == 1)
			iSeq = iTrack;
		// Which is just good for track labeling...
		qtractorMidiSequence *pSeq = NULL; 
		if (ppSeqs)
			pSeq = ppSeqs[iSeq];

		// Must be a track header "MTrk"...
		writeData((unsigned char *) SMF_MTRK, 4);

		// Write a dummy track length (we'll overwrite it later)...
		unsigned long iMTrkOffset = m_iOffset;
		writeInt(0, 4);

		// Track name...
		QString sTrackName
			= (pSeq ? pSeq->name() : QFileInfo(m_sFilename).baseName());
		if (!sTrackName.isEmpty()) {
			writeInt(0); // delta-time=0
			writeInt(qtractorMidiEvent::META, 1);
			writeInt(qtractorMidiEvent::TRACKNAME, 1);
			writeInt(sTrackName.length());
			QByteArray aTrackName = sTrackName.toUtf8();
			writeData((unsigned char *) aTrackName.constData(), aTrackName.length());
		}

		// Tempo/time-signature map and location markers...
		qtractorMidiFileTempo::Node *pNode = m_pTempoMap->nodes().first();
		qtractorMidiFileTempo::Marker *pMarker = m_pTempoMap->markers().first();

		// Tempo/time-signature map (track 0)
		// - applicable to SMF format 1 files...
		if (pSeq == NULL) {
			unsigned long iNodeTime = 0;
			while (pNode) {
				while (pMarker && pMarker->tick < pNode->tick) {
					writeMarker(pMarker, iNodeTime);
					iNodeTime = pMarker->tick;
					pMarker = pMarker->next();
				}
				writeNode(pNode, iNodeTime);
				iNodeTime = pNode->tick;
				pNode = pNode->next();
			}
			while (pMarker) {
				writeMarker(pMarker, iNodeTime);
				iNodeTime = pMarker->tick;
				pMarker = pMarker->next();
			}
		}

		// Now's time for proper events being written down...
		//
		if (pSeq) {

			// Provisional data structures for sequence merging...
			struct EventItem
			{
				EventItem(qtractorMidiSequence *pSeq)
					: seq(pSeq), event(pSeq->events().first())
					{ notesOff.setAutoDelete(true); }
				qtractorMidiSequence *seq;
				qtractorMidiEvent *event;
				qtractorList<qtractorMidiEvent> notesOff;
			};

			EventItem *pItem;
			unsigned short iItem;
			unsigned short iItems = (m_iFormat == 0 ? iSeqs : 1);

			// Prolog...
			EventItem **ppItems = new EventItem * [iItems];
			for (iItem = 0; iItem < iItems; ++iItem) {
				iSeq = (m_iFormat == 0 ? iItem : iTrack);
				ppItems[iItem] = new EventItem(ppSeqs[iSeq]);
			}

			// Write the whole sequence out...
			unsigned int   iChannel;
			unsigned int   iStatus;
			unsigned int   iLastStatus = 0;
			unsigned long  iLastTime   = 0;
			unsigned long  iTime;
			unsigned long  iTimeOff;
			unsigned char *data;
			unsigned int   len;

			EventItem *pEventItem;
			EventItem *pNoteOffItem;
			qtractorMidiEvent *pEvent;
			qtractorMidiEvent *pNoteFirst;
			qtractorMidiEvent *pNoteAfter;
			qtractorMidiEvent *pNoteOff;

			unsigned long iEventTime = 0;

			// Track/channel bank-select...
			if (pSeq->bank() >= 0) {
				iChannel = pSeq->channel();
				iStatus  = (qtractorMidiEvent::CONTROLLER | iChannel) & 0xff;
				writeInt(0); // delta-time=0
				writeInt(iStatus, 1);
				writeInt(BANK_MSB, 1);	// Bank MSB.
				writeInt((pSeq->bank() & 0x3f80) >> 7, 1);
				writeInt(0); // delta-time=0
			//	writeInt(iStatus, 1);
				writeInt(BANK_LSB, 1);	// Bank LSB.
				writeInt((pSeq->bank() & 0x007f), 1);
			}

			// Track/channel program change...
			if (pSeq->prog() >= 0) {
				iChannel = pSeq->channel();
				iStatus  = (qtractorMidiEvent::PGMCHANGE | iChannel) & 0xff;
				writeInt(0); // delta-time=0
				writeInt(iStatus, 1);
				writeInt((pSeq->prog() & 0x007f), 1);
			}

			// Lets-a go, down to event merge loop...
			//
			for (;;) {

				// Find which will be next event to write out,
				// keeping account onto  sequence merging...
				pSeq = NULL;
				pEvent = NULL;
				pEventItem = NULL;

				for (iItem = 0; iItem < iItems; ++iItem) {
					pItem = ppItems[iItem];
					if (pItem->event &&	(pEventItem == NULL
						|| iEventTime >= (pItem->event)->time())) {
						iEventTime = (pItem->event)->time();
						pEventItem = pItem;
					}
				}

				// So we'll have another event ready?
				if (pEventItem) {
					pSeq = pEventItem->seq;
					pEvent = pEventItem->event;
					pEventItem->event = pEvent->next();
					if (pEventItem->event)
						iEventTime = (pEventItem->event)->time();
				}

				// Maybe we reached the (partial) end...
				if (pEvent == NULL)
					break;

				// Event (absolute) time converted to file resolution...
				iTime = pSeq->timep(pEvent->time(), m_iTicksPerBeat);

				// Check for pending note-offs...
				for (;;) {

					// Get any a note-off event pending...
					iTimeOff = iTime;
					pNoteOff = NULL;
					pNoteOffItem = NULL;

					for (iItem = 0; iItem < iItems; ++iItem) {
						pItem = ppItems[iItem];
						pNoteFirst = pItem->notesOff.first();
						if (pNoteFirst && iTimeOff >= pNoteFirst->time()) {
							iTimeOff = pNoteFirst->time();
							pNoteOff = pNoteFirst;
							pNoteOffItem = pItem;
						}
					}

					// Was there any?
					if (pNoteOff == NULL)
						break;

					// - Delta time...
					writeInt(iTimeOff > iLastTime ? iTimeOff - iLastTime : 0);
					iLastTime = iTimeOff;
					// - Status byte...
					iChannel = (pNoteOffItem->seq)->channel();
					iStatus  = (pNoteOff->type() | iChannel) & 0xff;
					// - Running status...
					if (iStatus != iLastStatus) {
						writeInt(iStatus, 1);
						iLastStatus = iStatus;
					}
					// - Data bytes...
					writeInt(pNoteOff->note(), 1);
					writeInt(pNoteOff->velocity(), 1);

					// Remove from note-off list and continue...
					pNoteOffItem->notesOff.remove(pNoteOff);
				}

				// Tempo/time-signature map (interleaved)
				// - applicable to SMF format 0 files...
				if (iSeqs > 1 && iTrack == 0) {
					while (pNode && iTime >= pNode->tick) {
						while (pMarker && pMarker->tick < pNode->tick) {
							writeMarker(pMarker, iLastTime);
							iLastTime = pMarker->tick;
							pMarker = pMarker->next();
						}
						writeNode(pNode, iLastTime);
						iLastTime = pNode->tick;
						iLastStatus = 0;
						pNode = pNode->next();
					}
					while (pMarker && iTime >= pMarker->tick) {
						writeMarker(pMarker, iLastTime);
						iLastTime = pMarker->tick;
						iLastStatus = 0;
						pMarker = pMarker->next();
					}
				}

				// OK. Let's get back to actual event...

				// - Delta time...
				writeInt(iTime > iLastTime ? iTime - iLastTime : 0);
				iLastTime = iTime;

				// - Status byte...
				iChannel = pSeq->channel();

				// - Extra-ordinary (controller) types...
				const qtractorMidiEvent::EventType etype = pEvent->type();
				if (etype == qtractorMidiEvent::REGPARAM    ||
					etype == qtractorMidiEvent::NONREGPARAM ||
					etype == qtractorMidiEvent::CONTROL14) {
					iStatus = (qtractorMidiEvent::CONTROLLER | iChannel) & 0xff;
				} else {
					iStatus = (etype | iChannel) & 0xff;
				}

				// - Running status?
				if (iStatus != iLastStatus) {
					writeInt(iStatus, 1);
					iLastStatus = iStatus;
				}

				// - Data bytes...
				switch (etype) {
				case qtractorMidiEvent::NOTEON:
					writeInt(pEvent->note(), 1);
					writeInt(pEvent->velocity(), 1);
					iTimeOff = (pEvent->time() + pEvent->duration());
					pNoteOff = new qtractorMidiEvent(
						pSeq->timep(iTimeOff, m_iTicksPerBeat),
						qtractorMidiEvent::NOTEOFF,
						pEvent->note());
					// Find the proper position in notes-off list ...
					pNoteAfter = pEventItem->notesOff.last();
					while (pNoteAfter && pNoteAfter->time() > pNoteOff->time())
						pNoteAfter = pNoteAfter->prev();
					if (pNoteAfter)
						pEventItem->notesOff.insertAfter(pNoteOff, pNoteAfter);
					else
						pEventItem->notesOff.prepend(pNoteOff);
					break;
				case qtractorMidiEvent::KEYPRESS:
					writeInt(pEvent->note(), 1);
					writeInt(pEvent->velocity(), 1);
					break;
				case qtractorMidiEvent::CONTROLLER:
					writeInt(pEvent->controller(), 1);
					writeInt(pEvent->value(), 1);
					break;
				case qtractorMidiEvent::CONTROL14:
					writeInt((pEvent->controller() & 0x1f), 1);
					writeInt((pEvent->value() & 0x3f80) >> 7, 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt((pEvent->controller() + 0x20) & 0x3f, 1);
					writeInt((pEvent->value() & 0x007f), 1);
					break;
				case qtractorMidiEvent::REGPARAM:
					writeInt(RPN_MSB, 1);
					writeInt((pEvent->param() & 0x3f80) >> 7, 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt(RPN_LSB, 1);
					writeInt((pEvent->param() & 0x007f), 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt(DATA_MSB, 1);
					writeInt((pEvent->value() & 0x3f80) >> 7, 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt(DATA_LSB, 1);
					writeInt((pEvent->value() & 0x007f), 1);
					break;
				case qtractorMidiEvent::NONREGPARAM:
					writeInt(NRPN_MSB, 1);
					writeInt((pEvent->param() & 0x3f80) >> 7, 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt(NRPN_LSB, 1);
					writeInt((pEvent->param() & 0x007f), 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt(DATA_MSB, 1);
					writeInt((pEvent->value() & 0x3f80) >> 7, 1);
					writeInt(0); // delta-time=0
				//	writeInt(iStatus, 1);
					writeInt(DATA_LSB, 1);
					writeInt((pEvent->value() & 0x007f), 1);
					break;
				case qtractorMidiEvent::PGMCHANGE:
				case qtractorMidiEvent::CHANPRESS:
					writeInt(pEvent->value(), 1);
					break;
				case qtractorMidiEvent::PITCHBEND:
					writeInt((pEvent->value() & 0x007f), 1);
					writeInt((pEvent->value() & 0x3f80) >> 7, 1);
					break;
				case qtractorMidiEvent::SYSEX:
					if (pEvent->sysex() && pEvent->sysex_len() > 1) {
						data = pEvent->sysex() + 1; // Skip 0xf0 head.
						len  = pEvent->sysex_len() - 1;
						writeInt(len);
						writeData(data, len);
					}
					// Fall thru...
				default:
					break;
				}
			}

			// Merge all remaining note-offs...
			for (iItem = 0; iItem < iItems; ++iItem) {
				pItem = ppItems[iItem];
				pItem->event = pItem->notesOff.first();
			}

			iTimeOff = 0;

			for (;;) {

				// Find which note-off will be next to write out,
				// always accounting for sequence merging...
				pNoteOff = NULL;
				pNoteOffItem = NULL;

				for (iItem = 0; iItem < iItems; ++iItem) {
					pItem = ppItems[iItem];
					if (pItem->event &&	(pNoteOffItem == NULL
						|| iTimeOff >= (pItem->event)->time())) {
						iTimeOff = (pItem->event)->time();
						pNoteOffItem = pItem;
					}
				}

				// So we'll have another event ready?
				if (pNoteOffItem) {
					pNoteOff = pNoteOffItem->event;
					pNoteOffItem->event = pNoteOff->next();
					if (pNoteOffItem->event)
						iTimeOff = (pNoteOffItem->event)->time();
				}

				// Maybe we reached the (whole) end...
				if (pNoteOff == NULL)
					break;

				// - Delta time...
				iTime = pNoteOff->time();
				writeInt(iTime > iLastTime ? iTime - iLastTime : 0);
				iLastTime = iTime;
				// - Status byte...
				iChannel = (pNoteOffItem->seq)->channel();
				iStatus  = (pNoteOff->type() | iChannel) & 0xff;
				// - Running status...
				if (iStatus != iLastStatus) {
					writeInt(iStatus, 1);
					iLastStatus = iStatus;
				}
				// - Data bytes...
				writeInt(pNoteOff->note(), 1);
				writeInt(pNoteOff->velocity(), 1);
				// Done with this note-off...
			}

			// Epilog...
			for (iItem = 0; iItem < iItems; ++iItem)
				delete ppItems[iItem];
			delete [] ppItems;
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
		if (::fseek(m_pFile, m_iOffset, SEEK_SET))
			return false;
	}
	
	// Success.
	return true;
}

bool qtractorMidiFile::writeTrack ( qtractorMidiSequence *pSeq )
{
	return writeTracks((pSeq ? &pSeq : NULL), 1);
}



// Integer read method.
int qtractorMidiFile::readInt ( unsigned short n )
{
	int c, val = 0;
	
	if (n > 0) {
		// Fixed length (n bytes) integer read.
		for (int i = 0; i < n; ++i) {
			val <<= 8;
			c = ::fgetc(m_pFile);
			if (c < 0)
				return -1;
			val |= c;
			++m_iOffset;
		}
	} else {
		// Variable length integer read.
		do {
			c = ::fgetc(m_pFile);
			if (c < 0)
				return -1;
			val <<= 7;
			val |= (c & 0x7f);
			++m_iOffset;
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
	unsigned int c;

	if (n > 0) {
		// Fixed length (n bytes) integer write.
		for (int i = (n - 1) * 8; i >= 0; i -= 8) {
			c = (val & (0xff << i)) >> i;
			if (::fputc(c & 0xff, m_pFile) < 0)
				return -1;
			++m_iOffset;
		}
	} else {
		// Variable length integer write.
		n = 0;
		c = val & 0x7f;
		while ((val >>= 7) > 0) {
			c <<= 8;
			c |= (val & 0x7f) | 0x80;
		}
		while (true) {
			if (::fputc(c & 0xff, m_pFile) < 0)
				return -1;
			++n;
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


// Write tempo-time-signature node.
void qtractorMidiFile::writeNode (
	qtractorMidiFileTempo::Node *pNode, unsigned long iLastTime )
{
	unsigned long iDeltaTime
		= (pNode->tick > iLastTime ? pNode->tick - iLastTime : 0);
	qtractorMidiFileTempo::Node *pPrev = pNode->prev();

	if (pPrev == NULL || (pPrev->tempo != pNode->tempo)) {
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorMidiFile::writeNode(%lu) time=%lu TEMPO (%g)",
			iLastTime, iDeltaTime, pNode->tempo);
	#endif
		// Tempo change...
		writeInt(iDeltaTime);
		writeInt(qtractorMidiEvent::META, 1);
		writeInt(qtractorMidiEvent::TEMPO, 1);
		writeInt(3);
		writeInt(qtractorTimeScale::uroundf(60000000.0f / pNode->tempo), 3);
		iDeltaTime = 0;
	}

	if (pPrev == NULL ||
		pPrev->beatsPerBar != pNode->beatsPerBar ||
		pPrev->beatDivisor != pNode->beatDivisor) {
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorMidiFile::writeNode(%lu) time=%lu TIME (%u/%u)",
			iLastTime, iDeltaTime, pNode->beatsPerBar, 1 << pNode->beatDivisor);
	#endif
		// Time signature change...
		writeInt(iDeltaTime);
		writeInt(qtractorMidiEvent::META, 1);
		writeInt(qtractorMidiEvent::TIME, 1);
		writeInt(4);
		writeInt(pNode->beatsPerBar, 1);    // Numerator.
		writeInt(pNode->beatDivisor, 1);    // Denominator.
		writeInt(32, 1);                    // MIDI clocks per metronome click.
		writeInt(4, 1);                     // 32nd notes per quarter.
	//	iDeltaTime = 0;
	}
}


// Write location marker.
void qtractorMidiFile::writeMarker (
	qtractorMidiFileTempo::Marker *pMarker, unsigned long iLastTime )
{
	unsigned long iDeltaTime
		= (pMarker->tick > iLastTime ? pMarker->tick - iLastTime : 0);

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiFile::writeMarker(%lu) time=%lu (\"%s\")",
		iLastTime, iDeltaTime, pMarker->text.toUft8().constData());
#endif

	writeInt(iDeltaTime);
	writeInt(qtractorMidiEvent::META, 1);
	writeInt(qtractorMidiEvent::MARKER, 1);
	writeInt(pMarker->text.length());
	const QByteArray aMarker = pMarker->text.toUtf8();
	writeData((unsigned char *) aMarker.constData(), aMarker.length());
}


// Create filename revision (name says it all).
QString qtractorMidiFile::createFilePathRevision (
	const QString& sFilename, int iRevision )
{
	QFileInfo fi(sFilename);
	QDir adir(fi.absoluteDir());

	QRegExp rxRevision("(.+)\\-(\\d+)$");
	QString sBasename = fi.baseName();
	if (rxRevision.exactMatch(sBasename)) {
		sBasename = rxRevision.cap(1);
		iRevision = rxRevision.cap(2).toInt();
	}

	sBasename += "-%1." + fi.completeSuffix();

	if (iRevision < 1)
		++iRevision;

	fi.setFile(adir, sBasename.arg(iRevision));
	while (fi.exists())
		fi.setFile(adir, sBasename.arg(++iRevision));

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiFile::createFilePathRevision(\"%s\")",
		fi.absoluteFilePath().toUtf8().constData());
#endif

	return fi.absoluteFilePath();
}


// All-in-one SMF file writer/creator method.
bool qtractorMidiFile::saveCopyFile ( const QString& sNewFilename,
	const QString& sOldFilename, unsigned short iTrackChannel,
	unsigned short iFormat, qtractorMidiSequence *pSeq,
	qtractorTimeScale *pTimeScale, unsigned long iTimeOffset )
{
	qtractorMidiFile file;
	qtractorTimeScale ts;
	unsigned short iTracks;
	unsigned short iSeq, iSeqs = 0;
	qtractorMidiSequence **ppSeqs = NULL;
	const QString sTrackName = QObject::tr("Track %1");

	if (pSeq == NULL)
		return false;

	if (pTimeScale)
		ts.copy(*pTimeScale);

	// Open and load the whole source file...
	if (file.open(sOldFilename)) {
		ts.setTicksPerBeat(file.ticksPerBeat());
		iFormat = file.format();
		iSeqs = (iFormat == 1 ? file.tracks() : 16);
		ppSeqs = new qtractorMidiSequence * [iSeqs];
		for (iSeq = 0; iSeq < iSeqs; ++iSeq) {
			ppSeqs[iSeq] = new qtractorMidiSequence(
				sTrackName.arg(iSeq + 1), iSeq, ts.ticksPerBeat());
		}
		if (file.readTracks(ppSeqs, iSeqs) && file.tempoMap())
			file.tempoMap()->intoTimeScale(&ts, iTimeOffset);
		file.close();
	}

	// Open and save the whole target file...
	if (!file.open(sNewFilename, qtractorMidiFile::Write))
		return false;

	if (ppSeqs == NULL)
		iSeqs = (iFormat == 0 ? 1 : 2);

	// Write SMF header...
	iTracks = (iFormat == 0 ? 1 : iSeqs);
	if (!file.writeHeader(iFormat, iTracks, ts.ticksPerBeat())) {
		file.close();
		return false;
	}

	// Set (initial) tempo/time-signature node.
	if (file.tempoMap())
		file.tempoMap()->fromTimeScale(&ts, iTimeOffset);

	// Write SMF tracks(s)...
	if (ppSeqs) {
		// Replace the target track-channel events... 
		ppSeqs[iTrackChannel]->replaceEvents(pSeq);
		// Write the whole new tracks...
		file.writeTracks(ppSeqs, iSeqs);
	} else {
		// Most probabley this is a brand new file...
		if (iFormat == 1)
			file.writeTrack(NULL);
		file.writeTrack(pSeq);
	}

	file.close();

	// Free locally allocated track/sequence array.
	if (ppSeqs) {
		for (iSeq = 0; iSeq < iSeqs; ++iSeq)
			delete ppSeqs[iSeq];
		delete [] ppSeqs;
	}

	return true;
}


// end of qtractorMidiFile.cpp
