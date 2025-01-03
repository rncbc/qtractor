// qtractorMidiEngine.h
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTimeScale.h"
#include "qtractorMmcEvent.h"
#include "qtractorCtlEvent.h"

#include <alsa/asoundlib.h>

#include <QMultiHash>
#include <QMutex>
#include <QObject>

// Forward declarations.
class qtractorMidiBus;
class qtractorMidiClip;
class qtractorMidiEvent;
class qtractorMidiSequence;
class qtractorMidiInputThread;
class qtractorMidiOutputThread;
class qtractorMidiMonitor;
class qtractorMidiSysexList;
class qtractorMidiInputBuffer;
class qtractorMidiPlayer;
class qtractorPluginList;
class qtractorCurveList;

class QSocketNotifier;


//----------------------------------------------------------------------
// class qtractorMidiEngineProxy -- MIDI engine event proxy object.
//

class qtractorMidiEngineProxy : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEngineProxy(QObject *pParent = nullptr)
		: QObject(pParent) {}

	// Event notifications.
	void notifyMmcEvent(const qtractorMmcEvent& mmce)
		{ emit mmcEvent(mmce); }
	void notifyCtlEvent(const qtractorCtlEvent& ctle)
		{ emit ctlEvent(ctle); }
	void notifySppEvent(int iSppCmd, unsigned short iSongPos)
		{ emit sppEvent(iSppCmd, iSongPos); }
	void notifyClkEvent(float fTempo)
		{ emit clkEvent(fTempo); }
	void notifyInpEvent(unsigned short flags)
		{ emit inpEvent(flags); }

signals:
	
	// Event signals.
	void mmcEvent(const qtractorMmcEvent& mmce);
	void ctlEvent(const qtractorCtlEvent& ctle);
	void sppEvent(int iSppCmd, unsigned short iSongPos);
	void clkEvent(float fTempo);
	void inpEvent(unsigned short flags);
};


//----------------------------------------------------------------------
// class qtractorMidiEngine -- ALSA sequencer client instance (singleton).
//

class qtractorMidiEngine : public qtractorEngine
{
public:

	// Constructor.
	qtractorMidiEngine(qtractorSession *pSession);

	// Engine initialization.
	bool init();

	// Special event notifier proxy object.
	qtractorMidiEngineProxy *proxy();

	// ALSA client descriptor accessor.
	snd_seq_t *alsaSeq() const;
	int alsaClient() const;
	int alsaQueue() const;

	// ALSA queue time accessor.
	unsigned long queueTime() const;

	// ALSA subscription port notifier.
	QSocketNotifier *alsaNotifier() const;
	void alsaNotifyAck();

	// Read ahead frames configuration.
	void setReadAhead(unsigned int iReadAhead);
	unsigned int readAhead() const;

	// MIDI output process cycle iteration.
	void process();

	// Special slave sync method.
	void sync();

	// Reset queue time.
	void resetTime();
	void resetSync();

	// Reset queue tempo.
	void resetTempo();

	// Reset all MIDI monitoring...
	void resetAllMonitors();

	// Reset all MIDI controllers...
	void resetAllControllers(bool bForceImmediate);
	bool isResetAllControllersPending() const;

	// Shut-off all MIDI buses (stop)...
	void shutOffAllBuses(bool bClose = false);

	// Shut-off all MIDI tracks (panic)...
	void shutOffAllTracks();

	// ALSA port input registry methods.
	void addInputBus(qtractorMidiBus *pMidiBus);
	void removeInputBus(qtractorMidiBus *pMidiBus);

	void addInputBuffer(int iAlsaPort,
		qtractorMidiInputBuffer *pMidiInputBuffer);
	void removeInputBuffer(int iAlsaPort);

	// MIDI event capture method.
	void capture(snd_seq_event_t *pEv);

	// MIDI event enqueue method.
	void enqueue(qtractorTrack *pTrack, qtractorMidiEvent *pEvent,
		unsigned long iTime, float fGain = 1.0f);

	// Flush ouput queue (if necessary)...
	void flush();

	// The delta-time/frame accessors.
	long timeStart() const;

	// The absolute-time/frame accessors.
	unsigned long timeStartEx() const;

	// Special track-immediate methods.
	void trackMute(qtractorTrack *pTrack, bool bMute);

	// Special metronome-immediate methods.
	void metroMute(bool bMute);

	// Metronome switching.
	void setMetronome(bool bMetronome);
	bool isMetronome() const;

	// Metronome enabled accessors.
	void setMetroEnabled(bool bMetroEnabled);
	bool isMetroEnabled() const;

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

	// Metronome latency offset (in ticks).
	void setMetroOffset(unsigned long iMetroOffset);
	unsigned long metroOffset() const;

	// Process metronome clicks.
	void processMetro(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Access to current tempo/time-signature cursor.
	qtractorTimeScale::Cursor *metroCursor() const;

	// Metronome count-in switching.
	void setCountIn(bool bCountIn);
	bool isCountIn() const;

	// Metronome count-in mode.
	enum CountInMode { CountInNone = 0, CountInPlayback, CountInRecording };

	void setCountInMode(CountInMode countInMode);
	CountInMode countInMode() const;

	// Metronome count-in number of beats.
	void setCountInBeats(unsigned short iCountInBeats);
	unsigned short countInBeats() const;

	// Metronome count-in status.
	unsigned short countIn(unsigned int nframes);
	unsigned short countIn() const;

	// Control bus accessors.
	void setControlBus(bool bControlBus);
	bool isControlBus() const;
	void resetControlBus();

	// Control buses accessors.
	qtractorMidiBus *controlBus_in() const;
	qtractorMidiBus *controlBus_out() const;

	// Audition/pre-listening bus mode accessors.
	void setPlayerBus(bool bPlayerBus);
	bool isPlayerBus() const;

	bool isPlayerOpen() const;
	bool openPlayer(const QString& sFilename, int iTrackChannel = -1);
	void closePlayer();

	// MMC dispatch special commands.
	void sendMmcLocate(unsigned long iLocate) const;
	void sendMmcMaskedWrite(qtractorMmcEvent::SubCommand scmd,
		int iTrack, bool bOn) const;
	void sendMmcCommand(qtractorMmcEvent::Command cmd,
		unsigned char *pMmcData = nullptr, unsigned short iMmcData = 0) const;

	// SPP dispatch special command.
	void sendSppCommand(int iCmdType, unsigned short iSongPos = 0) const;

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement) const;

	// MIDI-export method.
	bool fileExport(const QString& sExportPath,
		const QList<qtractorMidiBus *>& exportBuses,
		unsigned long iExportStart, unsigned long iExportEnd,
		int iExportFormat = -1);

	// Retrieve/restore all connections, on all buses;
	// return the effective number of connection attempts.
	int updateConnects();

	// Capture (record) quantization accessors.
	void setCaptureQuantize(unsigned short iCaptureQuantize);
	unsigned short captureQuantize() const;

	// ALSA device queue timer.
	void setAlsaTimer(int iAlsaTimer);
	int alsaTimer() const;

	// Drift check/correction accessors.
	void setDriftCorrect(bool bDriftCorrect);
	bool isDriftCorrect() const;

	// MMC device-id accessors.
	void setMmcDevice(unsigned char mmcDevice);
	unsigned char mmcDevice() const;

	// MMC mode accessors.
	void setMmcMode(qtractorBus::BusMode mmcMode);
	qtractorBus::BusMode mmcMode() const;

	// SPP mode accessors.
	void setSppMode(qtractorBus::BusMode sppMode);
	qtractorBus::BusMode sppMode() const;

	// MIDI Clock mode accessors.
	void setClockMode(qtractorBus::BusMode clockMode);
	qtractorBus::BusMode clockMode() const;

	// Whether to reset all MIDI controllers (on playback start).
	void setResetAllControllers(bool bResetAllControllers);
	bool isResetAllControllers() const;

	// Reset ouput queue drift stats (audio vs. MIDI)...
	void resetDrift();

	// Step-input event flags...
	enum InpFlag { InpNone = 0, InpReset = 1, InpEvent = 2 };

	// Process pending step-input events...
	void processInpEvents();

protected:

	// Concrete device (de)activation methods.
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

	// Player (de)activation methods.
	void createPlayerBus();
	bool openPlayerBus();
	void closePlayerBus();
	void deletePlayerBus();

	// MIDI/Audio sync-check predicate.
	qtractorSessionCursor *midiCursorSync(bool bStart = false);

	// Process metronome count-ins.
	void processCountIn(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Do ouput queue drift stats (audio vs. MIDI)...
	void driftCheck();

private:

	// Special event notifier proxy object.
	qtractorMidiEngineProxy m_proxy;

	// Device instance variables.
	snd_seq_t *m_pAlsaSeq;
	int        m_iAlsaClient;
	int        m_iAlsaQueue;
	int        m_iAlsaTimer;

	// Subscription notification stuff.
	snd_seq_t       *m_pAlsaSubsSeq;
	int              m_iAlsaSubsPort;
	QSocketNotifier *m_pAlsaNotifier;

	// The number of frames to read-ahead.
	unsigned int m_iReadAhead;

	// Name says it all.
	qtractorMidiInputThread  *m_pInputThread;
	qtractorMidiOutputThread *m_pOutputThread;

	// ALSA port input registries.
	QHash<int, qtractorMidiBus *> m_inputBuses;
	QHash<int, qtractorMidiInputBuffer *> m_inputBuffers;

	// Whether to check for time drift.
	bool m_bDriftCorrect;

	// The number of times we check for time drift.
	unsigned int m_iDriftCheck;
	unsigned int m_iDriftCount;

	long m_iTimeDrift;
	long m_iFrameDrift;

	// The delta-time/frame when playback started.
	long m_iTimeStart;
	long m_iFrameStart;

	// The absolute-time/frame when playback started.
	unsigned long m_iTimeStartEx;

	unsigned long m_iAudioFrameStart;

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
	unsigned long    m_iMetroOffset;
	bool             m_bMetroEnabled;

	// Time-scale cursor (tempo/time-signature map)
	qtractorTimeScale::Cursor *m_pMetroCursor;

	// Track down tempo changes.
	float m_fMetroTempo;

	// Count-in stuff.
	bool             m_bCountIn;
	CountInMode      m_countInMode;
	unsigned short   m_iCountInBeats;
	unsigned short   m_iCountIn;
	unsigned long    m_iCountInFrame;
	unsigned long    m_iCountInFrameStart;
	unsigned long    m_iCountInFrameEnd;
	long             m_iCountInTimeStart;

	// SMF player enablement.
	bool             m_bPlayerBus;
	qtractorMidiBus *m_pPlayerBus;
	qtractorMidiPlayer *m_pPlayer;

	// Input quantization (aka. record snap-per-beat).
	unsigned short m_iCaptureQuantize;

	// Controller update pending flagger.
	int m_iResetAllControllersPending;

	// MMC Device ID.
	unsigned char m_mmcDevice;

	// MMC/SPP modes.
	qtractorBus::BusMode m_mmcMode;
	qtractorBus::BusMode m_sppMode;

	// MIDI Clock mode.
	qtractorBus::BusMode m_clockMode;

	// Whether to reset all MIDI controllers (on playback start).
	bool m_bResetAllControllers;

	// MIDI Clock tempo tracking.
	unsigned short m_iClockCount;
	float          m_fClockTempo;

	// Step-input event data...
	typedef QMultiHash<qtractorMidiClip *, qtractorMidiEvent *> InpEvents;

	InpEvents m_inpEvents;
	QMutex    m_inpMutex;
};


//----------------------------------------------------------------------
// class qtractorMidiBus -- Managed ALSA sequencer port set
//

class qtractorMidiBus : public qtractorBus
{
public:

	// Constructor.
	qtractorMidiBus(qtractorMidiEngine *pMidiEngine,
		const QString& sBusName, BusMode busMode, bool bMonitor = false);

	// Destructor.
	~qtractorMidiBus();

	// ALSA sequencer port accessor.
	int alsaPort() const;

	// Activation methods.
	bool open();
	void close();

	// Shut-off everything out there.
	void shutOff(bool bClose = false);

	// SysEx setup list accessors.
	qtractorMidiSysexList *sysexList() const;

	// Default instrument name accessors.
	void setInstrumentName(const QString& sInstrumentName);
	const QString& instrumentName() const;

	// Channel map payload.
	struct Patch
	{
		// Default payload constructor.
		Patch() : bankSelMethod(-1), bank(-1), prog(-1) {}
		// Validate members.
		bool isValid() const
			{ return (bankSelMethod >= 0 || bank >= 0 || prog >= 0); }
		// Payload members.
		QString instrumentName;
		int     bankSelMethod;
		int     bank;
		int     prog;
	};

	// Channel patch map accessors.
	const Patch& patch(unsigned iChannel)
		{ return m_patches[iChannel & 0x0f]; }

	// Direct MIDI bank/program selection helper.
	void setPatch(unsigned short iChannel,
		const QString& sInstrumentName,
		int iBankSelMethod, int iBank, int iProg,
		qtractorTrack *pTrack);

	// Direct MIDI controller helpers.
	void setController(qtractorTrack *pTrack,
		int iController, int iValue = 0) const;
	void setController(unsigned short iChannel,
		int iController, int iValue = 0) const;

	// Direct MIDI channel event helper.
	void sendEvent(qtractorMidiEvent::EventType etype,
		unsigned short iChannel, unsigned short iParam,
		unsigned short iValue) const;

	// Direct MIDI note helper.
	void sendNote(qtractorTrack *pTrack,
		int iNote, int iVelocity, bool bForce) const;

	// Direct SysEx helpers.
	void sendSysex(unsigned char *pSysex, unsigned int iSysex) const;
	void sendSysexList() const;

	// Import/export SysEx setup from/into event sequence.
	bool importSysexList(qtractorMidiSequence *pSeq);
	bool exportSysexList(qtractorMidiSequence *pSeq);

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
		ConnectList& connects, bool bConnect = false) const;

	// MIDI master volume.
	void setMasterVolume(float fVolume);
	// MIDI master panning (balance).
	void setMasterPanning(float fPanning);

	// MIDI channel volume.
	void setVolume(qtractorTrack *pTrack, float fVolume);
	// MIDI channel stereo panning.
	void setPanning(qtractorTrack *pTrack, float fPanning);

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Pending note-off processing methods.
	void enqueueNoteOff(snd_seq_event_t *pEv,
		unsigned long iTimeOn, unsigned long iTimeOff);
	void dequeueNoteOffs(unsigned long iQueueTime);

	// Update all aux-sends to this very bus...
	void updateMidiAuxSends(const QString& sMidiBusName);

protected:

	// Direct MIDI controller common helper.
	void setControllerEx(unsigned short iChannel, int iController,
		int iValue = 0, qtractorTrack *pTrack = nullptr) const;

	// Bus mode/name change events.
	void updateBusMode();
	void updateBusName();

	// Create plugin-list properly.
	qtractorPluginList *createPluginList(int iFlags) const;

	// Set plugin-list title name properly.
	void updatePluginListName(
		qtractorPluginList *pPluginList, int iFlags) const;

	// Set plugin-list buffers properly.
	void updatePluginList(qtractorPluginList *pPluginList, int iFlags);

	// Document instrument map methods.
	bool loadMidiMap(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveMidiMap(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Document SysEx setup list methods.
	bool loadSysexList(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveSysexList(qtractorDocument *pDocument, QDomElement *pElement) const;

private:

	// Instance variables.
	int m_iAlsaPort;

	// Specific monitor instances.
	qtractorMidiMonitor *m_pIMidiMonitor;
	qtractorMidiMonitor *m_pOMidiMonitor;

	// Plugin-chain instances.
	qtractorPluginList *m_pIPluginList;
	qtractorPluginList *m_pOPluginList;

	// SysEx setup list.
	qtractorMidiSysexList *m_pSysexList;

	// Default instrument name.
	QString m_sInstrumentName;

	// Channel patch mapper.
	QHash<unsigned short, Patch> m_patches;

	// Pending note-off processing data structure.
	//
	struct NoteOff
	{
		NoteOff(unsigned long on, unsigned long off)
			: time_on(on), time_off(off) {}
		NoteOff(const NoteOff& other)
			: time_on(other.time_on), time_off(other.time_off) {}

		unsigned long time_on;
		unsigned long time_off;
	};

	typedef QHash<unsigned short, NoteOff> NoteOffs;

	NoteOffs m_noteOffs;
};


#endif  // __qtractorMidiEngine_h


// end of qtractorMidiEngine.h
