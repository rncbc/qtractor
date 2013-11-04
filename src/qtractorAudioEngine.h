// qtractorAudioEngine.h
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

#ifndef __qtractorAudioEngine_h
#define __qtractorAudioEngine_h

#include "qtractorAtomic.h"
#include "qtractorEngine.h"

#include <jack/jack.h>

#include <QObject>


// Forward declarations.
class qtractorAudioBus;
class qtractorAudioBuffer;
class qtractorAudioMonitor;
class qtractorAudioFile;
class qtractorAudioExportBuffer;
class qtractorPluginList;
class qtractorCurveList;


//----------------------------------------------------------------------
// class qtractorAudioEngineProxy -- Audio engine event proxy object.
//

class qtractorAudioEngineProxy : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioEngineProxy(QObject *pParent = NULL)
		: QObject(pParent) {}

	// Event notifications.
	void notifyShutEvent()
		{ emit shutEvent(); }
	void notifyXrunEvent()
		{ emit xrunEvent(); }
	void notifyPortEvent()
		{ emit portEvent(); }
	void notifyBuffEvent()
		{ emit buffEvent(); }
	void notifySessEvent(void *pvSessionArg)
		{ emit sessEvent(pvSessionArg); }
	void notifySyncEvent(unsigned long iPlayHead)
		{ emit syncEvent(iPlayHead); }

signals:
	
	// Event signals.
	void shutEvent();
	void xrunEvent();
	void portEvent();
	void buffEvent();
	void sessEvent(void *pvSessionArg);
	void syncEvent(unsigned long iPlayHead);
};	


//----------------------------------------------------------------------
// class qtractorAudioEngine -- JACK client instance (singleton).
//

class qtractorAudioEngine : public qtractorEngine
{
public:

	// Constructor.
	qtractorAudioEngine(qtractorSession *pSession);

	// Engine initialization.
	bool init();

	// Special event notifier proxy object.
	const qtractorAudioEngineProxy *proxy() const;

	// Event notifications.
	void notifyShutEvent();
	void notifyXrunEvent();
	void notifyPortEvent();
	void notifyBuffEvent();
	void notifySessEvent(void *pvSessionArg);
	void notifySyncEvent(unsigned long iPlayHead);

	// JACK client descriptor accessor.
	jack_client_t *jackClient() const;

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
	// Buffer size accessor.
	unsigned int bufferSize() const;
	// Buffer offset accessor.
	unsigned int bufferOffset() const;

	// Audio (Master) bus defaults accessors.
	void setMasterAutoConnect(bool bMasterAutoConnect);
	bool isMasterAutoConnect() const;

	// Audio-export freewheeling (internal) state.
	void setFreewheel(bool bFreewheel);
	bool isFreewheel() const;

	// Audio-export active state.
	void setExporting(bool bExporting);
	bool isExporting() const;

	// Audio-export method.
	bool fileExport(const QString& sExportPath,
		const QList<qtractorAudioBus *>& exportBuses,
		unsigned long iExportStart = 0, unsigned long iExportEnd = 0);

	// Direct sync method (needed for export)
	void syncExport(unsigned long iFrameStart, unsigned long iFrameEnd);

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

	void resetMetro();

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

	// JACK Transport mode accessors.
	void setTransportMode(qtractorBus::BusMode transportMode);
	qtractorBus::BusMode transportMode() const;

	// Absolute number of frames elapsed since engine start.
	unsigned long jackFrame() const;

    // Reset all audio monitoring...
    void resetAllMonitors();

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

private:

	// Special event notifier proxy object.
	qtractorAudioEngineProxy m_proxy;

	// Audio device instance variables.
	jack_client_t *m_pJackClient;

	// JACK Session UUID.
	QString m_sSessionId;

	// Initial sample-rate and buffer size.
	unsigned int m_iSampleRate;
	unsigned int m_iBufferSize;

	// Audio (Master) bus defaults.
	bool m_bMasterAutoConnect;

	// Partial buffer offset state;
	// careful for proper loop concatenation.
	unsigned int m_iBufferOffset;

	// Audio-export freewheeling (internal) state.
	bool m_bFreewheel;

	// Common audio buffer sync thread.
	qtractorAudioBufferThread *m_pSyncThread;

	// Audio-export (in)active state.
	volatile bool        m_bExporting;
	qtractorAudioFile   *m_pExportFile;
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
	qtractorAudioBuffer *m_pMetroBeatBuff;
	QString              m_sMetroBarFilename;
	QString              m_sMetroBeatFilename;
	float                m_fMetroBarGain;
	float                m_fMetroBeatGain;
	unsigned long        m_iMetroBeatStart;
	unsigned int         m_iMetroBeat;
	bool                 m_bMetroEnabled;

	// Audition/pre-listening player stuff.
	qtractorAtomic       m_playerLock;
	bool                 m_bPlayerOpen;
	bool                 m_bPlayerBus;
	qtractorAudioBus    *m_pPlayerBus;
	bool                 m_bPlayerAutoConnect;
	qtractorAudioBuffer *m_pPlayerBuff;
	unsigned long        m_iPlayerFrame;

	// JACK Transport mode.
	qtractorBus::BusMode m_transportMode;
};


//----------------------------------------------------------------------
// class qtractorAudioBus -- Managed JACK port set
//

class qtractorAudioBus : public qtractorBus
{
public:

	// Constructor.
	qtractorAudioBus(qtractorAudioEngine *pAudioEngine,
		const QString& sBusName, BusMode busMode, bool bMonitor = false,
		unsigned short iChannels = 2);

	// Destructor.
	~qtractorAudioBus();

	// Channel number property accessor.
	void setChannels(unsigned short iChannels);
	unsigned short channels() const;

	// Auto-connection predicate.
	void setAutoConnect(bool bAutoConnect);
	bool isAutoConnect() const;

	// Concrete activation methods.
	bool open();
	void close();

	// Auto-connect to physical ports.
	void autoConnect();

	// Process cycle (preparator only).
	void process_prepare(unsigned int nframes);
	void process_monitor(unsigned int nframes);
	void process_commit(unsigned int nframes);

	// Bus-buffering methods.
	void buffer_prepare(unsigned int nframes,
		qtractorAudioBus *pInputBus = NULL);
	void buffer_commit(unsigned int nframes);

	float **buffer() const;

	// Frame buffer accessors.
	float **in()  const;
	float **out() const;

	// Virtual I/O bus-monitor accessors.
	qtractorMonitor *monitor_in()  const;
	qtractorMonitor *monitor_out() const;

	// Audio I/O bus-monitor accessors.
	qtractorAudioMonitor *audioMonitor_in()  const;
	qtractorAudioMonitor *audioMonitor_out() const;

	// Plugin-chain accessors.
	qtractorPluginList *pluginList_in()  const;
	qtractorPluginList *pluginList_out() const;

	// Automation curve list accessors.
	qtractorCurveList *curveList_in()  const;
	qtractorCurveList *curveList_out() const;

	// Automation curve serializer accessors.
	qtractorCurveFile *curveFile_in()  const;
	qtractorCurveFile *curveFile_out() const;

	// Audio I/O port latency accessors.
	unsigned int latency_in()  const;
	unsigned int latency_out() const;

	// Retrieve/restore client:port connections;
	// return the effective number of connection attempts...
	int updateConnects(BusMode busMode,
		ConnectList& connects, bool bConnect = false) const;

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement) const;

protected:

	// Bus mode change event.
	void updateBusMode();

	// Create plugin-list properly.
	qtractorPluginList *createPluginList(int iFlags) const;

	// Set plugin-list title name.
	void updatePluginListName(
		qtractorPluginList *pPluginList, int iFlags) const;

	// Set plugin-list buffers properly.
	void updatePluginList(
		qtractorPluginList *pPluginList, int iFlags);

private:

	// Instance variables.
	unsigned short m_iChannels;
	bool           m_bAutoConnect;

	// Specific monitor instances.
	qtractorAudioMonitor *m_pIAudioMonitor;
	qtractorAudioMonitor *m_pOAudioMonitor;

	// Plugin-chain instances.
	qtractorPluginList *m_pIPluginList;
	qtractorPluginList *m_pOPluginList;

	// Automation curve serializer instances.
	qtractorCurveFile  *m_pICurveFile;
	qtractorCurveFile  *m_pOCurveFile;

	// Specific JACK ports stuff.
	jack_port_t **m_ppIPorts;
	jack_port_t **m_ppOPorts;
	float       **m_ppIBuffer;
	float       **m_ppOBuffer;
	float       **m_ppXBuffer;
	float       **m_ppYBuffer;

	// Special under-work flag...
	// (r/w access should be atomic)
	bool m_bEnabled;

	// Buffer mix-down processor.
	void (*m_pfnBufferAdd)(float **, float **, unsigned int,
		unsigned short, unsigned short, unsigned int);
};


#endif  // __qtractorAudioEngine_h


// end of qtractorAudioEngine.h
