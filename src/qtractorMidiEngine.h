// qtractorMidiEngine.h
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QHash>

#include <alsa/asoundlib.h>

// Forward declarations.
class qtractorMidiBus;
class qtractorMidiEvent;
class qtractorMidiInputThread;
class qtractorMidiOutputThread;
class qtractorMidiMonitor;
class qtractorPluginList;

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

	// Reset all MIDI controllers...
	void resetAllControllers();

	// MIDI event capture method.
	void capture(snd_seq_event_t *pEv);

	// MIDI event enqueue method.
	void enqueue(qtractorTrack *pTrack, qtractorMidiEvent *pEvent,
		unsigned long iTime, float fGain = 1.0f);

	// Flush ouput queue (if necessary)...
	void flush();

	// Special rewind method, on queue loop.
	void restartLoop();

	// The delta-time accessors.
	void setTimeStart(long iTimeStart);
	long timeStart() const;

	// Special track-immediate methods.
	void trackMute(qtractorTrack *pTrack, bool bMute);

	// Special metronome-immediate methods.
	void metroMute(bool bMute);

	// Metronome switching.
	void setMetronome(bool bMetronome);
	bool isMetronome() const;

	// Metronome bus accessors.
	void setMetroBus(bool bMetroBus);
	bool isMetroBus() const;
	void resetMetroBus();

	// Metronome parameters.
	void setMetroChannel (unsigned short iChannel);
	unsigned short metroChannel() const;

	void setMetroBar(int iNote, int iVelocity, unsigned long iDuration);
	int metroBarNote() const;
	int metroBarVelocity() const;
	unsigned long metroBarDuration() const;

	void setMetroBeat(int iNote, int iVelocity, unsigned long iDuration);
	int metroBeatNote() const;
	int metroBeatVelocity() const;
	unsigned long metroBeatDuration() const;

	// Process metronome clicks.
	void processMetro(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Event notifier widget settings.
	void setNotifyObject  (QObject *pNotifyObject);
	void setNotifyMmcType (QEvent::Type eNotifyMmcType);
	void setNotifyCtlType (QEvent::Type eNotifyCtlType);

	QObject     *notifyObject() const;
	QEvent::Type notifyMmcType() const;
	QEvent::Type notifyCtlType() const;

	// Control bus accessors.
	void setControlBus(bool bControlBus);
	bool isControlBus() const;
	void resetControlBus();

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

	// Retrieve/restore all connections, on all buses;
	// return the effective number of connection attempts.
	int updateConnects();

	// Capture (record) quantization accessors.
	void setCaptureQuantize(unsigned short iCaptureQuantize);
	unsigned short captureQuantize() const;

protected:

	// Concrete device (de)activation methods.
	bool init(const QString& sClientName);
	bool activate();
	bool start();
	void stop();
	void deactivate();
	void clean();

	// Metronome (de)activation methods.
	void createMetroBus();
	bool openMetroBus();
	void closeMetroBus();
	void deleteMetroBus();

	// Control (de)activation methods.
	void createControlBus();
	bool openControlBus();
	void closeControlBus();
	void deleteControlBus();

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
	long m_iTimeDrift;

	// The event notifier widget.
	QObject      *m_pNotifyObject;
	QEvent::Type  m_eNotifyMmcType;
	QEvent::Type  m_eNotifyCtlType;

	// The assigned control buses.
	bool             m_bControlBus;
	qtractorMidiBus *m_pIControlBus;
	qtractorMidiBus *m_pOControlBus;

	// Metronome enablement.
	bool             m_bMetronome;
	bool             m_bMetroBus;
	qtractorMidiBus *m_pMetroBus;
	unsigned short   m_iMetroChannel;
	int              m_iMetroBarNote;
	int              m_iMetroBarVelocity;
	unsigned long    m_iMetroBarDuration;
	int              m_iMetroBeatNote;
	int              m_iMetroBeatVelocity;
	unsigned long    m_iMetroBeatDuration;

	// Time-scale cursor (tempo/time-signature map)
	qtractorTimeScale::Cursor *m_pMetroCursor;

	// Input quantization (aka. record snap-per-beat).
	unsigned short   m_iCaptureQuantize;

	// Track down tempo changes.
	float m_fTempo;
};


//----------------------------------------------------------------------
// class qtractorMidiBus -- Managed ALSA sequencer port set
//

class qtractorMidiBus : public qtractorBus
{
public:

	// Constructor.
	qtractorMidiBus(qtractorMidiEngine *pMidiEngine,
		const QString& sBusName, BusMode mode = Duplex,
		bool bPassthru = false);

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
	void setPatch(unsigned short iChannel,
		const QString& sInstrumentName,
		int iBankSelMethod, int iBank, int iProg,
		qtractorTrack *pTrack);

	// Direct MIDI controller helpers.
	void setController(qtractorTrack *pTrack,
		int iController, int iValue = 0) const;

	// Direct MIDI note helper.
	void sendNote(qtractorTrack *pTrack,
		int iNote, int iVelocity = 0) const;

	// Direct SysEx helper.
	void sendSysex(unsigned char *pSysex, unsigned int iSysex) const;

	// Virtual I/O bus-monitor accessors.
	qtractorMonitor *monitor_in()  const;
	qtractorMonitor *monitor_out() const;

	// MIDI I/O bus-monitor accessors.
	qtractorMidiMonitor *midiMonitor_in()  const;
	qtractorMidiMonitor *midiMonitor_out() const;

	// Plugin-chain accessors.
	qtractorPluginList *pluginList_in()  const;
	qtractorPluginList *pluginList_out() const;

	// Retrieve/restore client:port connections.
	// return the effective number of connection attempts.
	int updateConnects(BusMode busMode,
		ConnectList& connects, bool bConnect = false);

	// MIDI master volume.
	void setMasterVolume(float fVolume);
	// MIDI master panning (balance).
	void setMasterPanning(float fPanning);

	// MIDI channel volume.
	void setVolume(qtractorTrack *pTrack, float fVolume);
	// MIDI channel stereo panning.
	void setPanning(qtractorTrack *pTrack, float fPanning);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

protected:

	// Direct MIDI controller common helper.
	void setControllerEx(unsigned short iChannel, int iController,
		int iValue = 0, qtractorTrack *pTrack = NULL) const;

	// Bus mode change event.
	void updateBusMode();

	// Create plugin-list properly.
	qtractorPluginList *createPluginList (void) const;

	// Set plugin-list buffers properly.
	void updatePluginList(qtractorPluginList *pPluginList);

	// Document instrument map methods.
	bool loadMidiMap(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveMidiMap(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

private:

	// Instance variables.
	int m_iAlsaPort;

	// Specific monitor instances.
	qtractorMidiMonitor *m_pIMidiMonitor;
	qtractorMidiMonitor *m_pOMidiMonitor;

	// Plugin-chain instances.
	qtractorPluginList *m_pIPluginList;
	qtractorPluginList *m_pOPluginList;

	// Channel patch mapper.
	QHash<unsigned short, Patch> m_patches;
};


//----------------------------------------------------------------------
// qtractorMidiControlEvent - MIDI Control custom event.
//

class qtractorMidiControlEvent : public QEvent
{
public:

	// Contructor.
	qtractorMidiControlEvent(QEvent::Type eType, unsigned short iChannel,
		unsigned char controller, unsigned char value)
		: QEvent(eType), m_channel(iChannel),
			m_controller(controller), m_value(value) {}

	// Accessors.
	unsigned short channel()    const { return m_channel; }
	unsigned short controller() const { return m_controller; }
	unsigned short value()      const { return m_value; }

private:

	// Instance variables.
	unsigned short m_channel;
	unsigned short m_controller;
	unsigned short m_value;
};


#endif  // __qtractorMidiEngine_h


// end of qtractorMidiEngine.h
