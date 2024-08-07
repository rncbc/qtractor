// qtractorMidiManager.h
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiManager_h
#define __qtractorMidiManager_h

#include "qtractorAbout.h"
#include "qtractorMidiBuffer.h"

#ifdef CONFIG_VST2
#include "qtractorVst2Plugin.h"
#ifndef CONFIG_MIDI_PARSER
#define CONFIG_MIDI_PARSER 1
#endif
#endif

#ifdef CONFIG_VST3
#include "qtractorVst3Plugin.h"
#ifndef CONFIG_MIDI_PARSER
#define CONFIG_MIDI_PARSER 1
#endif
#endif

#ifdef CONFIG_CLAP
#include "qtractorClapPlugin.h"
#ifndef CONFIG_MIDI_PARSER
#define CONFIG_MIDI_PARSER 1
#endif
#endif

#ifdef CONFIG_LV2
#if defined(CONFIG_LV2_EVENT) || defined(CONFIG_LV2_ATOM)
#include "qtractorLv2Plugin.h"
#ifndef CONFIG_MIDI_PARSER
#define CONFIG_MIDI_PARSER 1
#endif
#endif
#endif

#include "qtractorInstrument.h"


// Forward declarations.
class qtractorTimeScale;
class qtractorPluginList;
class qtractorPlugin;
class qtractorAudioMonitor;
class qtractorAudioOutputMonitor;
class qtractorAudioBus;
class qtractorMidiBus;
class qtractorSubject;

class qtractorMidiSyncThread;


//----------------------------------------------------------------------
// class qtractorMidiSyncItem -- MIDI sync item decl.
//

class qtractorMidiSyncItem
{
public:

	// Constructor.
	qtractorMidiSyncItem();

	// Destructor.
	virtual ~qtractorMidiSyncItem();

	// Sync thread state flags accessors.
	void setWaitSync(bool bWaitSync);
	bool isWaitSync() const;

	// Process item (in asynchronous controller thread).
	virtual void processSync() = 0;

	// Post/schedule item for process sync.
	static void syncItem(qtractorMidiSyncItem *pSyncItem);

private:

	// Instance mmembers.
	volatile bool m_bWaitSync;

	// Async manager thread (singleton)
	static qtractorMidiSyncThread *g_pSyncThread;
	static unsigned int g_iSyncThreadRefCount;
};


//----------------------------------------------------------------------
// class qtractorMidiInputBuffer -- MIDI input buffer decl.
//

class qtractorMidiInputBuffer : public qtractorMidiBuffer
{
public:

	// Constructor.
	qtractorMidiInputBuffer(
		unsigned int iBufferSize = qtractorMidiBuffer::MinBufferSize)
		: qtractorMidiBuffer(iBufferSize),
			m_pDryGainSubject(nullptr), m_pWetGainSubject(nullptr) {}

	// Velocity/gain accessors.
	void setDryGainSubject(qtractorSubject *pDryGainSubject)
		{ m_pDryGainSubject = pDryGainSubject; }
	qtractorSubject *dryGainSubject() const
		{ return m_pDryGainSubject; }

	void setWetGainSubject(qtractorSubject *pWetGainSubject)
		{ m_pWetGainSubject = pWetGainSubject; }
	qtractorSubject *wetGainSubject() const
		{ return m_pWetGainSubject; }

	// Input event enqueuer.
	bool enqueue(snd_seq_event_t *pEv, unsigned long iTime = 0);

private:

	// Instance mmembers.
	qtractorSubject *m_pDryGainSubject;
	qtractorSubject *m_pWetGainSubject;
};


//----------------------------------------------------------------------
// class qtractorMidiOutputBuffer -- MIDI output buffer decl.
//

class qtractorMidiOutputBuffer : public qtractorMidiSyncItem
{
public:

	// Constructor.
	qtractorMidiOutputBuffer(qtractorMidiBus *pMidiBus,
		unsigned int iBufferSize = qtractorMidiBuffer::MinBufferSize)
		: qtractorMidiSyncItem(), m_pMidiBus(pMidiBus),
			m_outputBuffer(iBufferSize), m_pGainSubject(nullptr) {}

	// Velocity/gain accessors.
	void setGainSubject(qtractorSubject *pGainSubject)
		{ m_pGainSubject = pGainSubject; }
	qtractorSubject *gaiSubject() const
		{ return m_pGainSubject; }

	// Event enqueuer.
	bool enqueue(snd_seq_event_t *pEv, unsigned long iTime)
		{ return m_outputBuffer.push(pEv, iTime); }

	// Buffer reset.
	void clear() { m_outputBuffer.clear(); }

	// Process buffer (in asynchronous thread).
	void processSync();

private:

	// Instance mmembers.
	qtractorMidiBus   *m_pMidiBus;
	qtractorMidiBuffer m_outputBuffer;
	qtractorSubject   *m_pGainSubject;
};


//----------------------------------------------------------------------
// class qtractorMidiManager -- MIDI internal plugin list manager.
//

class qtractorMidiManager : public qtractorList<qtractorMidiManager>::Link
{
public:

	// Constructor.
	qtractorMidiManager(qtractorPluginList *pPluginList,
		unsigned int iBufferSize = qtractorMidiBuffer::MinBufferSize);

	// Destructor.
	~qtractorMidiManager();

	// Implementation properties.
	qtractorPluginList *pluginList() const
		{ return m_pPluginList; }

	unsigned int bufferSize() const
		{ return m_queuedBuffer.bufferSize(); }

	// Clears buffers for processing.
	void clear();

	// Event buffers accessors.
	qtractorMidiBuffer *buffer_in() const
		{ return m_ppEventBuffers[m_iEventBuffer & 1]; }
	qtractorMidiBuffer *buffer_out() const
		{ return m_ppEventBuffers[(m_iEventBuffer + 1) & 1]; }

	// Direct buffering.
	bool direct(snd_seq_event_t *pEvent);

	// Queued buffering.
	bool queued(snd_seq_event_t *pEvent,
		unsigned long iTime, unsigned long iTimeOff = 0);

	// Process buffers.
	void process(unsigned long iTimeStart, unsigned long iTimeEnd);

	// Process buffers (in asynchronous controller thread).
	void processSync();

	// Resets all buffering.
	void reset();

	// Sync thread state flags accessors.
	void setWaitSync(bool bWaitSync);
	bool isWaitSync() const;

	// Factory (proxy) methods.
	static qtractorMidiManager *createMidiManager(
		qtractorPluginList *pPluginList);
	static void deleteMidiManager(
		qtractorMidiManager *pMidiManager);

	// Some default factory options.
	static void setDefaultAudioOutputBus(bool bAudioOutputBus);
	static bool isDefaultAudioOutputBus();

	static void setDefaultAudioOutputAutoConnect(bool bAudioOutputAutoConnect);
	static bool isDefaultAudioOutputAutoConnect();

#ifdef CONFIG_DSSI
	// DSSI event buffer accessors...
	snd_seq_event_t *dssi_events() const
		{ return m_pDssiEvents; }
	unsigned int dssi_count() const
		{ return m_iDssiEvents; }
#endif

#ifdef CONFIG_VST2
	// VST2 event buffer accessors...
	VstEvents *vst2_events_in() const
		{ return (VstEvents *) m_ppVst2Buffers[m_iEventBuffer & 1]; }
	VstEvents *vst2_events_out() const
		{ return (VstEvents *) m_ppVst2Buffers[(m_iEventBuffer + 1) & 1]; }
	// Copy VST2 event buffer (output)...
	void vst2_events_copy(VstEvents *pVst2Buffer);
	// Swap VST2 event buffers...
	void vst2_events_swap();
#endif

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	// LV2 event buffer accessors...
	LV2_Event_Buffer *lv2_events_in() const
		{ return m_ppLv2EventBuffers[m_iEventBuffer & 1]; }
	LV2_Event_Buffer *lv2_events_out() const
		{ return m_ppLv2EventBuffers[(m_iEventBuffer + 1) & 1]; }
	// Swap LV2 event buffers...
	void lv2_events_swap();
#endif
#ifdef CONFIG_LV2_ATOM
	// LV2 atom buffer accessors...
	LV2_Atom_Buffer *lv2_atom_buffer_in() const
		{ return m_ppLv2AtomBuffers[m_iEventBuffer & 1]; }
	LV2_Atom_Buffer *lv2_atom_buffer_out() const
		{ return m_ppLv2AtomBuffers[(m_iEventBuffer + 1) & 1]; }
	// Swap LV2 atom buffers...
	void lv2_atom_buffer_swap();
	// Resize LV2 atom buffers if necessary.
	void lv2_atom_buffer_resize(unsigned int iMinBufferSize);
#endif
#endif

#ifdef CONFIG_MIDI_PARSER
	// Parse MIDI output and swap event buffers.
	// (esp. used by VST3 and CLAP)
	void swapOutputBuffers();
#endif

	// Audio output bus mode accessors.
	void setAudioOutputBus(bool bAudioOutputBus);
	bool isAudioOutputBus() const
		{ return m_bAudioOutputBus; }
	void setAudioOutputBusName(const QString& sAudioOutputBusName)
		{ m_sAudioOutputBusName = sAudioOutputBusName; }
	const QString& audioOutputBusName() const
		{ return m_sAudioOutputBusName; }
	qtractorAudioBus *audioOutputBus() const
		{ return m_pAudioOutputBus; }
	void resetAudioOutputBus();

	// Audio output bus defaults accessors.
	void setAudioOutputAutoConnect(bool bAudioOutputAutoConnect)
		{ m_bAudioOutputAutoConnect = bAudioOutputAutoConnect; }
	bool isAudioOutputAutoConnect() const
		{ return m_bAudioOutputAutoConnect; }

	// Audio output bus monitor accessors.
	void setAudioOutputMonitor(bool bAudioOutputMonitor);
	void setAudioOutputMonitorEx(bool bAudioOutputMonitor);
	bool isAudioOutputMonitor() const
		{ return m_bAudioOutputMonitor; }
	qtractorAudioOutputMonitor *audioOutputMonitor() const
		{ return m_pAudioOutputMonitor; }

	// Current bank selection accessors.
	void setCurrentBank(int iBank)
		{ m_iCurrentBank = iBank; }
	int currentBank() const
		{ return m_iCurrentBank; }

	// Current program selection accessors.
	void setCurrentProg(int iProg)
		{ m_iCurrentProg = iProg; }
	int currentProg() const
		{ return m_iCurrentProg; }

	// Instrument map builder.
	void updateInstruments();

	// Instrument map accessor.
	const qtractorInstrumentList& instruments() const
		{ return m_instruments; }

	// Direct MIDI controller helper.
	void setController(unsigned short iChannel, int iController, int iValue);

	// Shut-off MIDI channel (panic)...
	void shutOff(unsigned short iChannel);

	// Process specific MIDI buffer (merge).
	void processInputBuffer(
		qtractorMidiInputBuffer *pMidiInputBuffer, unsigned long t0 = 0);

	// Reset event buffers (input/output only)
	void resetInputBuffers();
	void resetOutputBuffers();

protected:

	// Audio output (de)activation methods.
	void createAudioOutputBus();
	void deleteAudioOutputBus();

	// Process/decode into other/plugin event buffers...
	void processEventBuffers();

	// Swap event buffers (in for out and vice-versa)
	void swapEventBuffers();

private:

	// MIDI process sync item class.
	//
	class SyncItem : public qtractorMidiSyncItem
	{
	public:
		// Constructor.
		SyncItem(qtractorMidiManager *pMidiManager)
			: qtractorMidiSyncItem(), m_pMidiManager(pMidiManager) {}
		// Processor sync method.
		void processSync() { m_pMidiManager->processSync(); }
		//
	private:
		// Instance member.
		qtractorMidiManager *m_pMidiManager;
	};

	SyncItem *m_pSyncItem;

	// Instance variables
	qtractorPluginList *m_pPluginList;

	qtractorMidiBuffer  m_directBuffer;
	qtractorMidiBuffer  m_queuedBuffer;
	qtractorMidiBuffer  m_postedBuffer;

	qtractorMidiBuffer  m_controllerBuffer;

	qtractorMidiBuffer  *m_ppEventBuffers[2];

	unsigned short      m_iEventBuffer;

#ifdef CONFIG_MIDI_PARSER
	snd_midi_event_t   *m_pMidiParser;
#endif

#ifdef CONFIG_DSSI
	snd_seq_event_t    *m_pDssiEvents;
	unsigned int        m_iDssiEvents;
#endif

#ifdef CONFIG_VST2
	VstMidiEvent       *m_ppVst2MidiBuffers[2];
	unsigned char      *m_ppVst2Buffers[2];
#endif

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer   *m_ppLv2EventBuffers[2];
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer    *m_ppLv2AtomBuffers[2];
	unsigned int        m_iLv2AtomBufferSize;
#endif
#endif

	bool                m_bAudioOutputBus;
	QString             m_sAudioOutputBusName;
	qtractorAudioBus   *m_pAudioOutputBus;
	bool                m_bAudioOutputAutoConnect;
	bool                m_bAudioOutputMonitor;

	qtractorAudioOutputMonitor *m_pAudioOutputMonitor;

	int m_iCurrentBank;
	int m_iCurrentProg;

	int m_iPendingBankMSB;
	int m_iPendingBankLSB;
	int m_iPendingProg;

	qtractorInstrumentList m_instruments;

	// Global factory options.
	static bool g_bAudioOutputBus;
	static bool g_bAudioOutputAutoConnect;
	static bool g_bAudioOutputMonitor;
};


#endif  // __qtractorMidiManager_h

// end of qtractorMidiManager.h
