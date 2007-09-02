// qtractorMidiEngine.h
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

#ifndef __qtractorMidiEngine_h
#define __qtractorMidiEngine_h

#include "qtractorEngine.h"
#include "qtractorMmcEvent.h"

#include <QMap>

#include <alsa/asoundlib.h>

// Forward declarations.
class qtractorMidiBus;
class qtractorMidiEvent;
class qtractorMidiInputThread;
class qtractorMidiOutputThread;
class qtractorMidiMonitor;

class QSocketNotifier;


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

	// ALSA subscription port notifier.
	QSocketNotifier *alsaNotifier() const;
	void alsaNotifyAck();

	// Special slave sync method.
	void sync();

	// Read ahead frames configuration.
	void setReadAhead(unsigned int iReadAhead);
	unsigned int readAhead() const;

	// Reset queue tempo.
	void resetTempo();

	// Reset all MIDI monitoring...
	void resetAllMonitors();

	// Reset control buses.
	void resetControlBus(qtractorBus::BusMode busMode);

	// MIDI event capture method.
	void capture(snd_seq_event_t *pEv);

	// MIDI event enqueue method.
	void enqueue(qtractorTrack *pTrack, qtractorMidiEvent *pEvent,
		unsigned long iTime, float fGain = 1.0f);

	// Flush ouput queue (if necessary)...
	void flush();

	// Special rewind method, on queue loop.
	void restartLoop();

	// Special track-immediate methods.
	void trackMute(qtractorTrack *pTrack, bool bMute);

	// Event notifier widget settings.
	void setNotifyWidget  (QWidget *pNotifyWidget);
	void setNotifyMmcType (QEvent::Type eNotifyMmcType);

	QWidget     *notifyWidget() const;
	QEvent::Type notifyMmcType() const;

	// Control buses accessors.
	qtractorMidiBus *controlBus_in() const;
	qtractorMidiBus *controlBus_out() const;

	// MMC dispatch special commands.
	void sendMmcLocate(unsigned long iLocate) const;
	void sendMmcMaskedWrite(qtractorMmcEvent::SubCommand scmd,
		int iTrack, bool bOn) const;
	void sendMmcCommand(qtractorMmcEvent::Command cmd,
		unsigned char *pMmcData = NULL, unsigned short iMmcData = 0) const;

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

	// MIDI-export method.
	bool fileExport(const QString& sExportPath,
		unsigned long iExportStart = 0, unsigned long iExportEnd = 0,
		qtractorMidiBus *pExportBus = NULL);

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

	// Subscription notification stuff.
	snd_seq_t       *m_pAlsaSubsSeq;
	int              m_iAlsaSubsPort;
	QSocketNotifier *m_pAlsaNotifier;

	// Name says it all.
	qtractorMidiInputThread  *m_pInputThread;
	qtractorMidiOutputThread *m_pOutputThread;

	// The delta-time when playback started .
	long m_iTimeStart;
	long m_iTimeDelta;

	// The event notifier widget.
	QWidget      *m_pNotifyWidget;
	QEvent::Type  m_eNotifyMmcType;

	// The assigned control buses.
	qtractorMidiBus *m_pIControlBus;
	qtractorMidiBus *m_pOControlBus;
};


//----------------------------------------------------------------------
// class qtractorMidiBus -- Managed ALSA sequencer port set
//

class qtractorMidiBus : public qtractorBus
{
public:

	// Constructor.
	qtractorMidiBus(qtractorMidiEngine *pMidiEngine,
		const QString& sBusName, BusMode mode = Duplex);

	// Destructor.
	~qtractorMidiBus();

	// ALSA sequencer port accessor.
	int alsaPort() const;

	// Activation methods.
	bool open();
	void close();

	// Shut-off everything out there.
	void shutOff(bool bClose = false) const;

	// Channel map payload.
	struct Patch
	{
		// Default payload constructor.
		Patch() : bankSelMethod(-1), bank(-1), prog(-1) {}
		// Payload members.
		QString instrumentName;
		int     bankSelMethod;
		int     bank;
		int     prog;
	};

	// Channel patch map accessor.
	Patch& patch(unsigned short iChannel)
		{ return m_patches[iChannel & 0x0f]; }

	// Direct MIDI bank/program selection helper.
	void setPatch(unsigned short iChannel, const QString& sInstrumentName,
		int iBankSelMethod, int iBank, int iProg);

	// Direct MIDI controller helper.
	void setController(unsigned short iChannel,
		int iController, int iValue = 0) const;

	// Direct MIDI note helper.
	void sendNote(unsigned short iChannel,
		int iNote, int iVelocity = 0) const;

	// Direct SysEx helper.
	void sendSysex(unsigned char *pSysex, unsigned int iSysex) const;

	// Virtual I/O bus-monitor accessors.
	qtractorMonitor *monitor_in()  const;
	qtractorMonitor *monitor_out() const;

	// MIDI I/O bus-monitor accessors.
	qtractorMidiMonitor *midiMonitor_in()  const;
	qtractorMidiMonitor *midiMonitor_out() const;

	// Retrieve/restore client:port connections.
	// return the effective number of connection attempts.
	int updateConnects(BusMode busMode,
		ConnectList& connects, bool bConnect = false);

	// MIDI master volume.
	void setMasterVolume(float fVolume);
	// MIDI channel volume.
	void setVolume(unsigned short iChannel, float fVolume);
	// MIDI channel stereo panning.
	void setPanning(unsigned short iChannel, float fPanning);

	// Document element methods.
	bool loadMidiMap(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveMidiMap(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

protected:

	// Bus mode change event.
	void updateBusMode();

private:

	// Instance variables.
	int m_iAlsaPort;

	// Specific monitor instances.
	qtractorMidiMonitor *m_pIMidiMonitor;
	qtractorMidiMonitor *m_pOMidiMonitor;

	// Channel patch mapper.
	QMap<unsigned short, Patch> m_patches;
};


#endif  // __qtractorMidiEngine_h


// end of qtractorMidiEngine.h
