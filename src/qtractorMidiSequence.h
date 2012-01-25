// qtractorMidiSequence.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiSequence_h
#define __qtractorMidiSequence_h

#include "qtractorMidiEvent.h"

#include <QString>
#include <QMultiHash>

// typedef unsigned long long uint64_t;
#include <stdint.h>


//----------------------------------------------------------------------
// class qtractorMidiSequence -- The generic MIDI event sequence buffer.
//

class qtractorMidiSequence
{
public:

	// Constructor.
	qtractorMidiSequence(const QString& sName = QString(),
		unsigned short iChannel = 0, unsigned short iTicksPerBeat = 960);

	// Destructor.
	~qtractorMidiSequence();

	// Sequencer reset method.
	void clear();

	// Sequence/track name accessors.
	void setName(const QString& sName) { m_sName = sName; }
	const QString& name() const { return m_sName; }

	// Sequence/track channel accessors.
	void setChannel(unsigned short iChannel) { m_iChannel = (iChannel & 0x0f); }
	unsigned short channel() const { return m_iChannel; }

	// Sequence/track bank accessors (optional).
	void setBank(int iBank) { m_iBank = iBank; }
	int bank() const { return m_iBank; }

	// Sequence/track prog accessors (optional).
	void setProgram(int iProgram) { m_iProgram = iProgram; }
	int program() const { return m_iProgram; }

	// Sequence/track resolution accessors.
	void setTicksPerBeat(unsigned short iTicksPerBeat)
		{ m_iTicksPerBeat = iTicksPerBeat; }
	unsigned short ticksPerBeat() const { return m_iTicksPerBeat; }

	// Sequence time-offset parameter accessors.
	void setTimeOffset(unsigned long iTimeOffset)
		{ m_iTimeOffset = iTimeOffset; }
	unsigned long timeOffset() const { return m_iTimeOffset; }

	// Sequence duration parameter accessors.
	void setTimeLength(unsigned long iTimeLength)
		{ m_iTimeLength = iTimeLength; }
	unsigned long timeLength() const { return m_iTimeLength; }

	void setDuration(unsigned long iDuration)
		{ m_duration = iDuration; }
	unsigned long duration() const { return m_duration; }

	// Statiscal helper accessors.
	void setNoteMin(unsigned char note)
		{ if (m_noteMin > note || m_noteMin == 0) m_noteMin = note; }
	void setNoteMax(unsigned char note)
		{ if (m_noteMax < note || m_noteMax == 0) m_noteMax = note; }

	unsigned char noteMin() const { return m_noteMin;  }
	unsigned char noteMax() const { return m_noteMax;  }

	// Event list accessor.
	const qtractorList<qtractorMidiEvent>& events() const { return m_events; }

	// Event list management methods.
	void addEvent    (qtractorMidiEvent *pEvent);
	void insertEvent (qtractorMidiEvent *pEvent);
	void unlinkEvent (qtractorMidiEvent *pEvent);
	void removeEvent (qtractorMidiEvent *pEvent);

	// Adjust time resolutions (64bit).
	unsigned long timep(unsigned long iTime, unsigned short p) const
		{ return uint64_t(iTime) * p / m_iTicksPerBeat; }
	unsigned long timeq(unsigned long iTime, unsigned short q) const
		{ return uint64_t(iTime) * m_iTicksPerBeat / q; }

	// Replace events from another sequence in given range.
	void replaceEvents(qtractorMidiSequence *pSeq,
		unsigned long iTimeOffset = 0, unsigned long iTimeLength = 0);

	// Clopy all events from another sequence (raw-copy).
	void copyEvents(qtractorMidiSequence *pSeq);

	// Sequence closure method.
	void close();

	// Typed hash table to track note-ons.
	typedef QMultiHash<unsigned char, qtractorMidiEvent *> NoteMap;

private:

	// Sequence/track properties.
	QString        m_sName;
	unsigned short m_iChannel;
	unsigned short m_iTicksPerBeat;

	// Sequence time-offset/duration parameters.
	unsigned long  m_iTimeOffset;
	unsigned long  m_iTimeLength;

	// Sequence/track optional properties.
	int            m_iBank;
	int            m_iProgram;

	// Statistical helper variables.
	unsigned char  m_noteMin;
	unsigned char  m_noteMax;
	unsigned long  m_duration;

	// Sequence instance event list (all same MIDI channel).
	qtractorList<qtractorMidiEvent> m_events;

	// Local hash table to track note-ons.
	NoteMap m_notes;
};


#endif  // __qtractorMidiSequence_h


// end of qtractorMidiSequence.h
