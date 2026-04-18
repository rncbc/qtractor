// qtractorCoreMidiEngine.h
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2024, Nebula Audio. All rights reserved.

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

#ifndef __qtractorCoreMidiEngine_h
#define __qtractorCoreMidiEngine_h

#include "qtractorEngine.h"
#include "qtractorTimeScale.h"
#include "qtractorMmcEvent.h"
#include "qtractorCtlEvent.h"

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#endif

#include <QMultiHash>
#include <QMutex>
#include <QObject>

// Forward declarations.
class qtractorMidiBus;
class qtractorMidiClip;
class qtractorMidiEvent;
class qtractorMidiSequence;
class qtractorMidiMonitor;
class qtractorMidiSysexList;
class qtractorMidiInputBuffer;
class qtractorMidiPlayer;
class qtractorPluginList;
class qtractorCurveList;


//----------------------------------------------------------------------
// class qtractorCoreMidiEngineProxy -- CoreMIDI engine event proxy object.
//

class qtractorCoreMidiEngineProxy : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorCoreMidiEngineProxy(QObject *pParent = nullptr)
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
// class qtractorCoreMidiEngine -- CoreMIDI client instance (singleton).
//

class qtractorCoreMidiEngine : public qtractorEngine
{
public:

	// Constructor.
	qtractorCoreMidiEngine(qtractorSession *pSession);

	// Engine initialization.
	bool init();

	// Special event notifier proxy object.
	qtractorCoreMidiEngineProxy *proxy();

#ifdef __APPLE__
	// CoreMIDI client descriptor accessor.
	MIDIClientRef coreMidiClient() const;
#endif

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

	// MIDI event capture method.
#ifdef __APPLE__
	void capture(const MIDIPacket *pPacket);
#endif

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
	void sendSppCommand(int iCmdType, unsigned int iSongPos = 0) const;

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

#ifdef __APPLE__
	// CoreMIDI read callback.
	static void midiReadCallback(
		const MIDIPacketList *pktlist,
		void *readProcRefCon,
		void *srcConnRefCon);
#endif

	// Special event notifier proxy object.
	qtractorCoreMidiEngineProxy m_proxy;

#ifdef __APPLE__
	// CoreMIDI device instance variables.
	MIDIClientRef m_coreMidiClient;
	MIDIPortRef   m_inputPort;
	MIDIPortRef   m_outputPort;
#endif

	// The number of frames to read-ahead.
	unsigned int m_iReadAhead;

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


#endif  // __qtractorCoreMidiEngine_h


// end of qtractorCoreMidiEngine.h

