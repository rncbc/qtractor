// qtractorWasapiEngine.h
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

#ifndef __qtractorWasapiEngine_h
#define __qtractorWasapiEngine_h

#include "qtractorAtomic.h"
#include "qtractorEngine.h"

#include <QObject>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#endif

// Forward declarations.
class qtractorAudioBus;
class qtractorAudioBuffer;
class qtractorAudioMonitor;
class qtractorAudioFile;
class qtractorAudioExportBuffer;
class qtractorPluginList;
class qtractorCurveList;


//----------------------------------------------------------------------
// class qtractorWasapiEngineProxy -- WASAPI engine event proxy object.
//

class qtractorWasapiEngineProxy : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorWasapiEngineProxy(QObject *pParent = nullptr)
		: QObject(pParent) {}

	// Event notifications.
	void notifyShutEvent()
		{ emit shutEvent(); }
	void notifyXrunEvent()
		{ emit xrunEvent(); }
	void notifyPortEvent()
		{ emit portEvent(); }
	void notifyBuffEvent(unsigned int iBufferSize)
		{ emit buffEvent(iBufferSize); }
	void notifySessEvent(void *pvSessionArg)
		{ emit sessEvent(pvSessionArg); }
	void notifySyncEvent(unsigned long iPlayHead, bool bPlaying)
		{ emit syncEvent(iPlayHead, bPlaying); }
	void notifyPropEvent()
		{ emit propEvent(); }
	void notifySelfEvent()
		{ emit selfEvent(); }

signals:
	
	// Event signals.
	void shutEvent();
	void xrunEvent();
	void portEvent();
	void buffEvent(unsigned int iBufferSize);
	void sessEvent(void *pvSessionArg);
	void syncEvent(unsigned long iPlayHead, bool bPlaying);
	void propEvent();
	void selfEvent();
};


//----------------------------------------------------------------------
// class qtractorWasapiEngine -- WASAPI client instance (singleton).
//

class qtractorWasapiEngine : public qtractorEngine
{
public:

	// Constructor.
	qtractorWasapiEngine(qtractorSession *pSession);

	// Engine initialization.
	bool init();

	// Special event notifier proxy object.
	const qtractorWasapiEngineProxy *proxy() const;

	// Event notifications.
	void notifyShutEvent();
	void notifyXrunEvent();
	void notifyPortEvent();
	void notifyBuffEvent(unsigned int iBufferSize);
	void notifySessEvent(void *pvSessionArg);
	void notifySyncEvent(unsigned long iPlayHead, bool bPlaying);
	void notifyPropEvent();
	void notifySelfEvent();

#ifdef _WIN32
	// WASAPI device descriptor accessor.
	IMMDevice        *wasapiDevice() const;
	IAudioClient     *audioClient() const;
	IAudioRenderClient *renderClient() const;
#endif

	// Process cycle executive.
	int process(unsigned int nframes);

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Session UUID accessors.
	void setSessionId(const QString& sSessionId);
	const QString& sessionId() const;

	// Internal sample-rate accessor.
	unsigned int sampleRate() const;

	// Buffer size accessors.
	unsigned int bufferSize() const;
	unsigned int bufferSizeEx() const;

	// Buffer offset accessor.
	unsigned int bufferOffset() const;

	// Block-stride size (in frames) accessor.
	unsigned int blockSize() const;

	// Audio (Master) bus defaults accessors.
	void setMasterAutoConnect(bool bMasterAutoConnect);
	bool isMasterAutoConnect() const;

	// Audio-export freewheeling (internal) state.
	void setFreewheel(bool bFreewheel);
	bool isFreewheel() const;

	// Audio-export active state.
	void setExporting(bool bExporting);
	bool isExporting() const;

	// Last known export extents accessors.
	unsigned long exportStart() const;
	unsigned long exportOffset() const;
	unsigned long exportLength() const;

	// Audio-export method.
	bool fileExport(const QString& sExportPath,
		const QList<qtractorAudioBus *>& exportBuses,
		unsigned long iExportStart, unsigned long iExportEnd,
		int iExportFormat = -1);

	// Special track-immediate methods.
	void trackMute(qtractorTrack *pTrack, bool bMute);

	// Metronome switching.
	void setMetronome(bool bMetronome);
	bool isMetronome() const;

	// Metronome enabled accessors.
	void setMetroEnabled(bool bMetroEnabled);
	bool isMetroEnabled() const;

	// Metronome bus mode accessors.
	void setMetroBus(bool bMetroBus);
	bool isMetroBus() const;
	void resetMetroBus();

	// Metronome bus defaults accessors.
	void setMetroAutoConnect(bool bMetroAutoConnect);
	bool isMetroAutoConnect() const;

	// Metronome bar audio sample.
	void setMetroBarFilename(const QString& sFilename);
	const QString& metroBarFilename() const;

	// Metronome bar audio sample gain.
	void setMetroBarGain(float fGain);
	float metroBarGain() const;

	// Metronome beat audio sample.
	void setMetroBeatFilename(const QString& sFilename);
	const QString& metroBeatFilename() const;

	// Metronome beat audio sample gain.
	void setMetroBeatGain(float fGain);
	float metroBeatGain() const;

	// Metronome latency offset (in frames).
	void setMetroOffset(unsigned long iMetroOffset);
	unsigned long metroOffset() const;

	void resetMetro(bool bCountIn = false);

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
	unsigned short countIn() const;

	// Audition/pre-listening bus mode accessors.
	void setPlayerBus(bool bPlayerBus);
	bool isPlayerBus() const;

	void resetPlayerBus();

	// Audition/pre-listening bus defaults accessors.
	void setPlayerAutoConnect(bool bPlayerAutoConnect);
	bool isPlayerAutoConnect() const;

	bool isPlayerOpen() const;
	bool openPlayer(const QString& sFilename);
	void closePlayer();

	// Retrieve/restore all connections, on all audio buses;
	// return the effective number of connection attempts.
	int updateConnects();

	// Transport mode accessors.
	void setTransportMode(qtractorBus::BusMode transportMode);
	qtractorBus::BusMode transportMode() const;

	// Transport latency accessors.
	void setTransportLatency(unsigned int iTransportLatency);
	unsigned int transportLatency() const;

	// Timebase mode accessors.
	void setTimebase(bool bTimebase);
	bool isTimebase() const;
	bool isTimebaseEx() const;

	// Timebase reset methods.
	void resetTimebase();

	// Absolute number of frames elapsed since engine start.
	unsigned long frameTime() const;

	// Reset all audio monitoring...
	void resetAllMonitors();

	// Whether we're in the audio/real-time thread...
	static bool isProcessing();

	// Time(base)/BBT info.
	struct TimeInfo
	{
		unsigned long  frame;
		bool           playing;
		unsigned int   sampleRate;
		float          tempo;
		unsigned int   ticksPerBeat;
		unsigned short beatsPerBar;
		unsigned short beatType;
		float          beats;
		unsigned short bar;
		unsigned int   beat;
		unsigned int   tick;
		float          barBeats;
		unsigned long  barTicks;
	};

	const TimeInfo& timeInfo() const
		{ return m_timeInfo; }

	// Update time(base)/BBT info.
	void updateTimeInfo(unsigned long iFrame);

	// Auxiliary audio output bus sorting method...
	void resetAudioOutBus(
		qtractorAudioBus *pAudioBus,
		qtractorPluginList *pPluginList);

	QStringList cyclicAudioOutBuses(qtractorAudioBus *pAudioBus) const;

	// Transport locate/reposition (timebase aware)...
	void transport_locate(unsigned long iFrame);

	// Audio I/O latency callbacks.
	void updateLatency_in();
	void updateLatency_out();

	// Audio latency compensation stuff.
	enum LatencyMode { Auto = 0, Add, Fixed };

	void setCaptureLatencyMode(LatencyMode latencyMode);
	LatencyMode captureLatencyMode() const;

	void setCaptureLatency(unsigned int iCaptureLatency);
	unsigned int captureLatency() const;

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

	// Audition/pre-listening player (de)activation methods.
	void createPlayerBus();
	bool openPlayerBus();
	void closePlayerBus();
	void deletePlayerBus();

	// Freewheeling process cycle executive (needed for export).
	void process_export(unsigned int nframes);

	// Metronome latency offset compensation.
	unsigned long metro_offset(unsigned long iFrame) const;

private:

#ifdef _WIN32
	// WASAPI render callback.
	static DWORD WINAPI renderThread(LPVOID lpParameter);
#endif

	// Special event notifier proxy object.
	qtractorWasapiEngineProxy m_proxy;

#ifdef _WIN32
	// WASAPI device instance variables.
	IMMDevice        *m_pWasapiDevice;
	IAudioClient     *m_pAudioClient;
	IAudioRenderClient *m_pRenderClient;
	HANDLE            m_hRenderEvent;
	HANDLE            m_hRenderThread;
	WAVEFORMATEX     *m_pWaveFormat;
	UINT32            m_bufferSizeFrames;
#endif

	// Session UUID.
	QString m_sSessionId;

	// Initial sample-rate and buffer size.
	unsigned int m_iSampleRate;
	unsigned int m_iBufferSize;

	// Initial maximum safe buffer size.
	unsigned int m_iBufferSizeEx;

	// Partial buffer offset state;
	// careful for proper loop concatenation.
	unsigned int m_iBufferOffset;

	// Block-stride size (in frames).
	unsigned int m_iBlockSize;

	// Audio (Master) bus defaults.
	bool m_bMasterAutoConnect;

	// Audio-export freewheeling (internal) state.
	bool m_bFreewheel;

	// Common audio buffer sync thread.
	qtractorAudioBufferThread *m_pSyncThread;

	// Audio-export (in)active state.
	volatile bool        m_bExporting;
	qtractorAudioFile   *m_pExportFile;
	unsigned long        m_iExportOffset;
	unsigned long        m_iExportStart;
	unsigned long        m_iExportEnd;
	volatile bool        m_bExportDone;

	QList<qtractorAudioBus *> *m_pExportBuses;
	qtractorAudioExportBuffer *m_pExportBuffer;

	// Audio metronome stuff.
	bool                 m_bMetronome;
	bool                 m_bMetroBus;
	qtractorAudioBus    *m_pMetroBus;
	bool                 m_bMetroAutoConnect;
	qtractorAudioBuffer *m_pMetroBarBuff;
	QString              m_sMetroBarFilename;
	float                m_fMetroBarGain;
	qtractorAudioBuffer *m_pMetroBeatBuff;
	QString              m_sMetroBeatFilename;
	float                m_fMetroBeatGain;
	unsigned long        m_iMetroOffset;
	unsigned long        m_iMetroBeatStart;
	unsigned int         m_iMetroBeat;
	bool                 m_bMetroEnabled;

	// Count-in stuff.
	bool                 m_bCountIn;
	CountInMode          m_countInMode;
	unsigned short       m_iCountInBeats;
	unsigned short       m_iCountIn;
	unsigned int         m_iCountInBeat;
	unsigned long        m_iCountInBeatStart;
	unsigned long        m_iCountInFrame;
	unsigned long        m_iCountInFrameEnd;

	// Audition/pre-listening player stuff.
	qtractorAtomic       m_playerLock;
	bool                 m_bPlayerOpen;
	bool                 m_bPlayerBus;
	qtractorAudioBus    *m_pPlayerBus;
	bool                 m_bPlayerAutoConnect;
	qtractorAudioBuffer *m_pPlayerBuff;
	unsigned long        m_iPlayerFrame;

	// Transport mode.
	qtractorBus::BusMode m_transportMode;

	// Transport latency.
	unsigned int         m_iTransportLatency;

	// Timebase mode and control.
	bool                 m_bTimebase;
	unsigned int         m_iTimebase;

	// Time(base)/BBT time info.
	TimeInfo             m_timeInfo;

	// Audio latency compensation stuff.
	LatencyMode  m_captureLatencyMode;
	unsigned int m_iCaptureLatency;
};


#endif  // __qtractorWasapiEngine_h


// end of qtractorWasapiEngine.h

