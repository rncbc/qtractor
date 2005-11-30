// qtractorMidiEngine.h
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

#ifndef __qtractorMidiEngine_h
#define __qtractorMidiEngine_h

#include "qtractorEngine.h"

#include <qmap.h>

#include <alsa/asoundlib.h>

// Forward declarations.
class qtractorMidiBus;
class qtractorMidiEvent;
class qtractorMidiOutputThread;


//----------------------------------------------------------------------
// class qtractorMidiEngine -- ALSA sequencer client instance (singleton).
//

class qtractorMidiEngine : public qtractorEngine
{
public:

	// Constructor.
	qtractorMidiEngine(qtractorSession *pSession);

	// ALSA client descriptor accessor.
	snd_seq_t *alsaSeq() const;
	int alsaClient() const;
	int alsaQueue() const;

	// Special slave sync method.
	void sync();

	// MIDI event enqueue method.
	void enqueue(qtractorTrack *pTrack, qtractorMidiEvent *pEvent,
		unsigned long iTime, float fGain = 1.0);

	// Flush ouput queue (if necessary)...
	void flush();

	// Special track-immediate methods.
	void trackMute(qtractorTrack *pTrack, bool bMute);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

protected:

	// Concrete device (de)activation methods.
	bool init(const QString& sClientName);
	bool activate();
	bool start();
	void stop();
	void deactivate();
	void clean();

private:

	// Device instance variables.
	snd_seq_t *m_pAlsaSeq;
	int        m_iAlsaClient;
	int        m_iAlsaQueue;
	
	// Name says it all.
	qtractorMidiOutputThread *m_pOutputThread;
	// The time when playback started.
	unsigned long m_iTimeStart;
};


//----------------------------------------------------------------------
// class qtractorMidiBus -- Managed ALSA sequencer port set
//

class qtractorMidiBus : public qtractorBus
{
public:

	// Constructor.
	qtractorMidiBus(const QString& sBusName, BusMode mode = Duplex);

	// ALSA sequencer port accessor.
	int alsaPort() const;

	// Activation methods.
	bool open();
	void close();

	// Channel map payload.
	struct Patch
	{
		// Default payload constructor.
		Patch() : bank(-1), prog(-1) {}
		// Payload members.
		QString name;
		int     bank;
		int     prog;
	};

	// Channel map accessor.
	Patch& patch(unsigned short iChannel)
		{ return m_map[iChannel & 0x0f]; }

	// Direct MIDI bank/program selection helper.
	void setPatch(unsigned short iChannel, const QString& sInstrumentName,
		int iBank, int iProg, int iBanklSelMethod = 0);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

private:

	// Instance variables.
	int m_iAlsaPort;

	// Channel mapper.
 	QMap<int, Patch> m_map;
};


#endif  // __qtractorMidiEngine_h


// end of qtractorMidiEngine.h
