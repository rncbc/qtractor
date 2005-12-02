// qtractorTrack.h
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

#ifndef __qtractorTrack_h
#define __qtractorTrack_h

#include "qtractorList.h"

#include <qcolor.h>


// Forward declarations.
class qtractorSession;
class qtractorSessionDocument;
class qtractorInstrumentList;
class qtractorClip;
class qtractorBus;

// Special forward declarations.
class QDomElement;
class QPainter;
class QRect;


//-------------------------------------------------------------------------
// qtractorTrack -- Track container.

class qtractorTrack : public qtractorListNode<qtractorTrack>
{
public:

	// Track type symbology.
	enum TrackType { None = 0, Audio, Midi };

	// Constructor.
	qtractorTrack(qtractorSession *pSession, TrackType trackType = None);
	// Default constructor.
	~qtractorTrack();

	// Reset track.
	void clear();

	// Track open/close methods.
	bool open();
	void close();

	// Session accessor.
	qtractorSession *session() const;

	// Track name accessors.
	void setTrackName(const QString& sTrackName);
	const QString& trackName() const;

	// Track type accessors.
	void setTrackType(TrackType trackType);
	TrackType trackType() const;

	// Record status accessors.
	void setRecord(bool bRecord);
	bool isRecord() const;

	// Mute status accessors.
	void setMute(bool bMute);
	bool isMute() const;

	// Solo status accessors.
	void setSolo(bool bSolo);
	bool isSolo() const;

	// MIDI specific: track-tag accessors.
	void setMidiTag(unsigned short iMidiTag);
	unsigned short midiTag() const;

	// MIDI specific: channel acessors.
	void setMidiChannel(unsigned short iMidiChannel);
	unsigned short midiChannel() const;

	// MIDI specific: bank acessors (optional).
	void setMidiBank(int iMidiBank);
	int midiBank() const;

	// MIDI specific: program acessors (optional).
	void setMidiProgram(int iMidiProgram);
	int midiProgram() const;

	// Assigned bus name accessors.
	void setBusName(const QString& sBusName);
	const QString& busName() const;

	// Assigned bus accessors.
	qtractorBus *bus() const;

	// Normalized view height accessors.
	void setHeight(unsigned short iHeight);
	unsigned short height() const;

	// Clip list management methods.
	qtractorList<qtractorClip>& clips();

	void addClip(qtractorClip *pClip);
	void unlinkClip(qtractorClip *pClip);
	void removeClip(qtractorClip *pClip);

	// Background color accessors.
	void setBackground(const QColor& bg);
	const QColor& background() const;

	// Foreground color accessors.
	void setForeground(const QColor& fg);
	const QColor& foreground() const;

	// Generate a default track color.
	static QColor trackColor(int iTrack);

	// Track special process cycle executive.
	void process(qtractorClip *pClip,
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Track paint method.
	void drawTrack(QPainter *pPainter, const QRect& trackRect,
		unsigned long iTrackStart, unsigned long iTrackEnd,
		qtractorClip *pClip = NULL);

	// MIDI track instrument patching.
	void setMidiPatch(qtractorInstrumentList *pInstruments);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

private:

	qtractorSession *m_pSession;    // Session reference.

	QString   m_sTrackName;         // Track title name.
	TrackType m_trackType;          // Track type

	bool m_bMute;                   // Mute status.
	bool m_bRecord;                 // Record status.
	bool m_bSolo;                   // Solo status.

	unsigned short m_iMidiTag;      // MIDI specific: track-tag;
	unsigned short m_iMidiChannel;  // MIDI specific: channel;

	int m_iMidiBank;                // MIDI specific: bank (optional);
	int m_iMidiProgram;             // MIDI specific: program (optional);

	QString      m_sBusName;        // Assigned bus name.
	qtractorBus *m_pBus;            // Track assigned bus.

	int m_iHeight;                  // View height (normalized).

	QColor m_colorBackground;       // Clip background color.
	QColor m_colorForeground;       // Clip foreground color.

	qtractorList<qtractorClip> m_clips; // List of clips.
};


#endif  // __qtractorTrack_h


// end of qtractorTrack.h
