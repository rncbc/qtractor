// qtractorMidiBuffer.cpp
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

#include "qtractorMidiBuffer.h"

#include "qtractorSession.h"
#include "qtractorPlugin.h"

#include "qtractorMidiEngine.h"
#include "qtractorAudioEngine.h"

#include "qtractorMixer.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>


// Specific controller definitions
#define BANK_SELECT_MSB		0x00
#define BANK_SELECT_LSB		0x20


//----------------------------------------------------------------------
// class qtractorMidiManagerThread -- MIDI controller thread.
//

class qtractorMidiManagerThread : public QThread
{
public:

	// Constructor.
	qtractorMidiManagerThread(unsigned int iSyncSize = 128);

	// Destructor.
	~qtractorMidiManagerThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync(qtractorMidiManager *pMidiManager = NULL);

protected:

	// The main thread executive.
	void run();

private:

	// The thread launcher queue instance reference.
	unsigned int           m_iSyncSize;
	unsigned int           m_iSyncMask;
	qtractorMidiManager  **m_ppSyncItems;

	volatile unsigned int  m_iSyncRead;
	volatile unsigned int  m_iSyncWrite;

	// Whether the thread is logically running.
	volatile bool m_bRunState;

	// Thread synchronization objects.
	QMutex         m_mutex;
	QWaitCondition m_cond;
};


//----------------------------------------------------------------------
// class qtractorMidiManagerThread -- MIDI output thread (singleton).
//

// Constructor.
qtractorMidiManagerThread::qtractorMidiManagerThread ( unsigned int iSyncSize )
{
	m_iSyncSize = (64 << 1);
	while (m_iSyncSize < iSyncSize)
		m_iSyncSize <<= 1;
	m_iSyncMask = (m_iSyncSize - 1);
	m_ppSyncItems = new qtractorMidiManager * [m_iSyncSize];
	m_iSyncRead   = 0;
	m_iSyncWrite  = 0;

	::memset(m_ppSyncItems, 0, m_iSyncSize * sizeof(qtractorMidiManager *));

	m_bRunState = false;	
}


// Destructor.
qtractorMidiManagerThread::~qtractorMidiManagerThread (void)
{
	delete [] m_ppSyncItems;
}


// Thread run state accessors.
void qtractorMidiManagerThread::setRunState ( bool bRunState )
{
	QMutexLocker locker(&m_mutex);

	m_bRunState = bRunState;
}

bool qtractorMidiManagerThread::runState (void) const
{
	return m_bRunState;
}


// The main thread executive.
void qtractorMidiManagerThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManagerThread[%p]::run(): started...", this);
#endif

	m_mutex.lock();

	m_bRunState = true;

	while (m_bRunState) {
		// Wait for sync...
		m_cond.wait(&m_mutex);
#ifdef CONFIG_DEBUG_0
		qDebug("qtractorMidiManagerThread[%p]::run(): waked.", this);
#endif
		// Call control process cycle.
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			m_ppSyncItems[r]->processSync();
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManagerThread[%p]::run(): stopped.", this);
#endif
}


// Wake from executive wait condition.
void qtractorMidiManagerThread::sync ( qtractorMidiManager *pMidiManager )
{
	if (pMidiManager == NULL) {
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			qtractorMidiManager *pSyncItem = m_ppSyncItems[r];
			if (pSyncItem)
				pSyncItem->setWaitSync(false);
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
	} else {
		// !pMidiManager->isWaitSync()
		unsigned int n;
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		if (w > r) {
			n = ((r - w + m_iSyncSize) & m_iSyncMask) - 1;
		} else if (r > w) {
			n = (r - w) - 1;
		} else {
			n = m_iSyncSize - 1;
		}
		if (n > 0) {
			pMidiManager->setWaitSync(true);
			m_ppSyncItems[w] = pMidiManager;
			m_iSyncWrite = (w + 1) & m_iSyncMask;
		}
	}

	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else qDebug("qtractorMidiManagerThread[%p]::sync(): tryLock() failed.", this);
#endif
}


//----------------------------------------------------------------------
// class qtractorMidiManager -- MIDI internal plugin list manager.
//
qtractorMidiManagerThread *qtractorMidiManager::g_pSyncThread = NULL;
unsigned int               qtractorMidiManager::g_iSyncThreadRefCount = 0;

bool qtractorMidiManager::g_bAudioOutputBus = false;
bool qtractorMidiManager::g_bAudioOutputAutoConnect = true;

// AG: Buffer size large enough to hold some sysex events.
const long c_iMaxMidiData = 512;

// Constructor.
qtractorMidiManager::qtractorMidiManager (
	qtractorPluginList *pPluginList, unsigned int iBufferSize ) :
	m_pPluginList(pPluginList),
	m_directBuffer(iBufferSize >> 1),
	m_queuedBuffer(iBufferSize),
	m_postedBuffer(iBufferSize),
	m_controllerBuffer(iBufferSize >> 2),
	m_pBuffer(NULL), m_iBuffer(0),
#ifdef CONFIG_MIDI_PARSER
	m_pMidiParser(NULL),
#endif
	m_iEventBuffer(0),
	m_bAudioOutputBus(g_bAudioOutputBus),
	m_bAudioOutputAutoConnect(g_bAudioOutputAutoConnect),
	m_pAudioOutputBus(NULL),
	m_iCurrentBank(-1),
	m_iCurrentProg(-1),
	m_iPendingBankMSB(-1),
	m_iPendingBankLSB(-1),
	m_iPendingProg(-1)
{
	const unsigned int MaxMidiEvents = (bufferSize() << 1);

	m_pBuffer = new snd_seq_event_t [MaxMidiEvents];

#ifdef CONFIG_MIDI_PARSER
	if (snd_midi_event_new(c_iMaxMidiData, &m_pMidiParser) == 0)
		snd_midi_event_no_status(m_pMidiParser, 1);
#endif

	// Create_event buffers...
#ifdef CONFIG_VST
	const unsigned int VstBufferSize = sizeof(VstEvents)
		+ MaxMidiEvents * sizeof(VstMidiEvent *);
#endif
#ifdef CONFIG_LV2_EVENT
	const unsigned int Lv2EventBufferSize
		= (sizeof(LV2_Event) + 4) * MaxMidiEvents;
#endif
#ifdef CONFIG_LV2_ATOM
	m_iLv2AtomBufferSize = (sizeof(LV2_Atom_Event) + 4) * MaxMidiEvents;
#endif
	for (unsigned short i = 0; i < 2; ++i) {
	#ifdef CONFIG_VST
		m_ppVstBuffers[i] = new unsigned char [VstBufferSize];
		m_ppVstMidiBuffers[i] = new VstMidiEvent [MaxMidiEvents];
	#endif
	#ifdef CONFIG_LV2_EVENT
		m_ppLv2EventBuffers[i] = lv2_event_buffer_new(Lv2EventBufferSize,
			LV2_EVENT_AUDIO_STAMP);
	#endif
	#ifdef CONFIG_LV2_ATOM
		m_ppLv2AtomBuffers[i] = lv2_atom_buffer_new(m_iLv2AtomBufferSize,
			qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Chunk),
			qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Sequence), (i & 1) == 0);
	#endif
	}

	m_bWaitSync = false;

	if (++g_iSyncThreadRefCount == 1 && g_pSyncThread == NULL) {
		g_pSyncThread = new qtractorMidiManagerThread();
		g_pSyncThread->start();
	}
	
	createAudioOutputBus();
}

// Destructor.
qtractorMidiManager::~qtractorMidiManager (void)
{
	deleteAudioOutputBus();

	if (--g_iSyncThreadRefCount == 0 && g_pSyncThread != NULL) {
		// Try to wake and terminate executive thread,
		// but give it a bit of time to cleanup...
		if (g_pSyncThread->isRunning()) do {
			g_pSyncThread->setRunState(false);
		//	g_pSyncThread->terminate();
			g_pSyncThread->sync();
		} while (!g_pSyncThread->wait(100));
		delete g_pSyncThread;
		g_pSyncThread = NULL;
	}

	// Destroy event_buffers...
	for (unsigned short i = 0; i < 2; ++i) {
	#ifdef CONFIG_VST
		delete [] m_ppVstMidiBuffers[i];
		delete [] m_ppVstBuffers[i];
	#endif
	#ifdef CONFIG_LV2_EVENT
		::free(m_ppLv2EventBuffers[i]);
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_free(m_ppLv2AtomBuffers[i]);
	#endif
	}

#ifdef CONFIG_MIDI_PARSER
	if (m_pMidiParser) {
		snd_midi_event_free(m_pMidiParser);
		m_pMidiParser = NULL;
	}
#endif

	if (m_pBuffer)
		delete [] m_pBuffer;
}


// Direct buffering.
bool qtractorMidiManager::direct ( snd_seq_event_t *pEvent )
{
	if (pEvent->type == SND_SEQ_EVENT_CONTROLLER) {
		switch (pEvent->data.control.param) {
		case BANK_SELECT_MSB:
			m_iPendingBankMSB = pEvent->data.control.value;
			break;
		case BANK_SELECT_LSB:
			m_iPendingBankLSB = pEvent->data.control.value;
			break;
		default:
			m_controllerBuffer.push(pEvent);
			break;
		}
	}
	else if (pEvent->type == SND_SEQ_EVENT_PGMCHANGE)
		m_iPendingProg = pEvent->data.control.value;

	return m_directBuffer.push(pEvent);
}


// Queued buffering.
bool qtractorMidiManager::queued ( qtractorTimeScale *pTimeScale,
	snd_seq_event_t *pEvent, unsigned long iTime, long iFrameStart )
{
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekTick(iTime);
	const unsigned long t0 = pNode->frameFromTick(iTime);
	unsigned long t1 = (long(t0) < iFrameStart ? t0 : t0 - iFrameStart);

	if (pEvent->type == SND_SEQ_EVENT_NOTE) {
		snd_seq_event_t ev = *pEvent;
		ev.type = SND_SEQ_EVENT_NOTEON;
		if (!m_queuedBuffer.insert(&ev, t1))
			return false;
		if (ev.data.note.duration > 0) {
			iTime += (ev.data.note.duration - 1);
			pNode = cursor.seekTick(iTime);
			t1 += (pNode->frameFromTick(iTime) - t0);
		}
		ev.type = SND_SEQ_EVENT_NOTEOFF;
		ev.data.note.velocity = 0;
		ev.data.note.duration = 0;
		return m_postedBuffer.insert(&ev, t1);
	}

	if (pEvent->type == SND_SEQ_EVENT_NOTEOFF)
		return m_postedBuffer.insert(pEvent, t1);
	else
		return m_queuedBuffer.insert(pEvent, t1);
}


// Clears buffers for processing.
void qtractorMidiManager::clear (void)
{
	m_iBuffer = 0;

	// Reset event buffers...
	for (unsigned short i = 0; i < 2; ++i) {
	#ifdef CONFIG_VST
		VstEvents *pVstEvents = (VstEvents *) m_ppVstBuffers[i];
		::memset(pVstEvents, 0, sizeof(VstEvents));
	#endif
	#ifdef CONFIG_LV2_EVENT
		LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[i];
		lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
			(unsigned char *) (pLv2EventBuffer + 1));
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_reset(m_ppLv2AtomBuffers[i], (i & 1) == 0);
	#endif
	}

	m_iEventBuffer = 0;
}


// Process buffers (merge).
void qtractorMidiManager::process (
	unsigned long iTimeStart, unsigned long iTimeEnd )
{
	clear();

	// Check for program changes and controller messages...
	if (g_pSyncThread
		&& (m_iPendingProg >= 0 || !m_controllerBuffer.isEmpty()))
		g_pSyncThread->sync(this);

	// Merge events in buffer for plugin processing...
	snd_seq_event_t *pEv0 = m_directBuffer.peek();
	snd_seq_event_t *pEv1 = m_queuedBuffer.peek();
	snd_seq_event_t *pEv2 = m_postedBuffer.peek();

	// Direct events...
	while (pEv0) {
		m_pBuffer[m_iBuffer++] = *pEv0;
		pEv0 = m_directBuffer.next();
	}

	// Queued/posted events...
	while ((pEv1 && pEv1->time.tick < iTimeEnd)
		|| (pEv2 && pEv2->time.tick < iTimeEnd)) {
		while (pEv1 && pEv1->time.tick < iTimeEnd
			&& ((pEv2 && pEv2->time.tick >= pEv1->time.tick) || !pEv2)) {
			m_pBuffer[m_iBuffer] = *pEv1;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv1->time.tick > iTimeStart
					? pEv1->time.tick - iTimeStart : 0);
			pEv1 = m_queuedBuffer.next();
		}
		while (pEv2 && pEv2->time.tick < iTimeEnd
			&& ((pEv1 && pEv1->time.tick > pEv2->time.tick) || !pEv1)) {
			m_pBuffer[m_iBuffer] = *pEv2;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv2->time.tick > iTimeStart
					? pEv2->time.tick - iTimeStart : 0);
			pEv2 = m_postedBuffer.next();
		}
	}

#ifdef CONFIG_DEBUG_0
	for (unsigned int i = 0; i < m_iBuffer; ++i) {
		snd_seq_event_t *pEv = &m_pBuffer[i];
		// - show event for debug purposes...
		const unsigned long iTime = iTimeStart + pEv->time.tick;
		fprintf(stderr, "MIDI Seq %06lu 0x%02x", iTime, pEv->type);
		if (pEv->type == SND_SEQ_EVENT_SYSEX) {
			fprintf(stderr, " sysex {");
			unsigned char *data = (unsigned char *) pEv->data.ext.ptr;
			for (unsigned int i = 0; i < pEv->data.ext.len; ++i)
				fprintf(stderr, " %02x", data[i]);
			fprintf(stderr, " }\n");
		} else {
			for (unsigned int i = 0; i < sizeof(pEv->data.raw8.d); ++i)
				fprintf(stderr, " %3d", pEv->data.raw8.d[i]);
			fprintf(stderr, "\n");
		}
	}
#endif

#ifdef CONFIG_MIDI_PARSER
	if (m_pMidiParser) {
	#ifdef CONFIG_VST
		VstEvents *pVstEvents = (VstEvents *) m_ppVstBuffers[m_iEventBuffer & 1];
	#endif
	#ifdef CONFIG_LV2_EVENT
		LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[m_iEventBuffer & 1];
		LV2_Event_Iterator eiter;
		lv2_event_begin(&eiter, pLv2EventBuffer);
	#endif
	#ifdef CONFIG_LV2_ATOM
		LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[m_iEventBuffer & 1];
		LV2_Atom_Buffer_Iterator aiter;
		lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
	#endif
		const unsigned int MaxMidiEvents = (bufferSize() << 1);
		unsigned int iMidiEvents = 0;
		// AG: Untangle treatment of VST and LV2 plugins,
		// so that we can use a larger buffer for the latter...
	#ifdef CONFIG_VST
		unsigned int iVstMidiEvents = 0;
	#endif
		unsigned char *pMidiData;
		long iMidiData;
		for (unsigned int i = 0; i < m_iBuffer; ++i) {
			snd_seq_event_t *pEv = &m_pBuffer[i];
			unsigned char midiData[c_iMaxMidiData];
			pMidiData = &midiData[0];
			iMidiData = sizeof(midiData);
			iMidiData = snd_midi_event_decode(m_pMidiParser,
				pMidiData, iMidiData, pEv);
			if (iMidiData < 0)
				break;
		#ifdef CONFIG_DEBUG_0
			// - show event for debug purposes...
			unsigned long iTime = pEv->time.tick;
			fprintf(stderr, "MIDI Raw %06lu {", iTime);
			for (long i = 0; i < iMidiData; ++i)
				fprintf(stderr, " %02x", pMidiData[i]);
			fprintf(stderr, " }\n");
		#endif
		#ifdef CONFIG_LV2_EVENT
			lv2_event_write(&eiter, pEv->time.tick, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
		#ifdef CONFIG_LV2_ATOM
			lv2_atom_buffer_write(&aiter, pEv->time.tick, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
		#ifdef CONFIG_VST
			VstMidiEvent *pVstMidiBuffer = m_ppVstMidiBuffers[m_iEventBuffer & 1];
			VstMidiEvent *pVstMidiEvent = &pVstMidiBuffer[iVstMidiEvents];
			if (iMidiData < long(pVstMidiEvent->midiData)) {
				::memset(pVstMidiEvent, 0, sizeof(VstMidiEvent));
				pVstMidiEvent->type = kVstMidiType;
				pVstMidiEvent->byteSize = sizeof(VstMidiEvent);
				pVstMidiEvent->deltaFrames = pEv->time.tick;
				::memcpy(&pVstMidiEvent->midiData[0], pMidiData, iMidiData);
				pVstEvents->events[iVstMidiEvents++] = (VstEvent *) pVstMidiEvent;
			}
		#endif
			if (++iMidiEvents >= MaxMidiEvents)
				break;
		}
	#ifdef CONFIG_VST
		pVstEvents->numEvents = iVstMidiEvents;
	//	pVstEvents->reserved = 0;
	#endif
	}
#endif

	// Now's time to process the plugins as usual...
	if (m_pAudioOutputBus) {
		const unsigned int nframes = iTimeEnd - iTimeStart;
		if (m_bAudioOutputBus) {
			m_pAudioOutputBus->process_prepare(nframes);
			m_pPluginList->process(m_pAudioOutputBus->out(), nframes);
			m_pAudioOutputBus->process_commit(nframes);
		} else {
			m_pAudioOutputBus->buffer_prepare(nframes);
			m_pPluginList->process(m_pAudioOutputBus->buffer(), nframes);
			m_pAudioOutputBus->buffer_commit(nframes);
		}
	}
}


// Process buffers (in asynchronous controller thread).
void qtractorMidiManager::processSync (void)
{
	if (!m_bWaitSync)
		return;

	// Check for programn change...
	if (m_iPendingProg >= 0) {
		m_iCurrentBank = 0;
		m_iCurrentProg = m_iPendingProg;
		if (m_iPendingBankLSB >= 0) {
			if (m_iPendingBankMSB >= 0)
				m_iCurrentBank = (m_iPendingBankMSB << 7) + m_iPendingBankLSB;
			else
				m_iCurrentBank = m_iPendingBankLSB;
		}
		else if (m_iPendingBankMSB >= 0)
			m_iCurrentBank = m_iPendingBankMSB;
		// Make the change (should be RT safe...)
		qtractorPlugin *pPlugin = m_pPluginList->first();
		while (pPlugin) {
			pPlugin->selectProgram(m_iCurrentBank, m_iCurrentProg);
			pPlugin = pPlugin->next();
		}
		// Reset pending status.
		m_iPendingBankMSB = -1;
		m_iPendingBankLSB = -1;
		m_iPendingProg    = -1;
	}

	// Have all controller events sent to plugin(s),
	// mostly for mere GUI update purposes...
	snd_seq_event_t *pEv = m_controllerBuffer.peek();
	while (pEv) {
		if (pEv->type == SND_SEQ_EVENT_CONTROLLER) {
			qtractorPlugin *pPlugin = m_pPluginList->first();
			while (pPlugin) {
				pPlugin->setController(
					pEv->data.control.param,
					pEv->data.control.value);
				pPlugin = pPlugin->next();
			}
		}
		pEv = m_controllerBuffer.next();
	}

	m_controllerBuffer.clear();

	m_bWaitSync = false;
}


// Resets all buffering.
void qtractorMidiManager::reset (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->lock();

	m_directBuffer.clear();
	m_queuedBuffer.clear();
	m_postedBuffer.reset(); // formerly .clear();

	clear();

#ifdef CONFIG_MIDI_PARSER
	if (m_pMidiParser) {
		snd_midi_event_reset_decode(m_pMidiParser);
		snd_midi_event_reset_encode(m_pMidiParser);
	}
#endif

	m_pPluginList->resetBuffer();

	m_controllerBuffer.clear();

	m_iPendingBankMSB = -1;
	m_iPendingBankLSB = -1;
	m_iPendingProg    = -1;

	pSession->unlock();
}


// Sync thread state flags accessors.
void qtractorMidiManager::setWaitSync ( bool bWaitSync )
{
	m_bWaitSync = bWaitSync;
}

bool qtractorMidiManager::isWaitSync (void) const
{
	return m_bWaitSync;
}


// Factory (proxy) methods.
qtractorMidiManager *qtractorMidiManager::createMidiManager (
	qtractorPluginList *pPluginList )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	qtractorMidiManager *pMidiManager
		= new qtractorMidiManager(pPluginList);
	pSession->addMidiManager(pMidiManager);

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManager::createMidiManager(%p)", pMidiManager);
#endif

	return pMidiManager;
}


void qtractorMidiManager::deleteMidiManager ( qtractorMidiManager *pMidiManager )
{
	if (pMidiManager == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManager::deleteMidiManager(%p)", pMidiManager);
#endif

	pSession->removeMidiManager(pMidiManager);

	delete pMidiManager;
}


// Swap event buffers (in for out and vice-versa)
void qtractorMidiManager::swapEventBuffers (void)
{
#ifdef CONFIG_VST
	::memset(m_ppVstBuffers[m_iEventBuffer & 1], 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[m_iEventBuffer & 1];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
#endif
#ifdef CONFIG_LV2_ATOM
	lv2_atom_buffer_reset(m_ppLv2AtomBuffers[m_iEventBuffer & 1], false);
#endif

	++m_iEventBuffer;
}


#ifdef CONFIG_VST

// Copy VST event buffer (output)...
void qtractorMidiManager::vst_events_copy ( VstEvents *pVstBuffer )
{
	const unsigned short iEventBuffer = (m_iEventBuffer + 1) & 1;
	VstMidiEvent *pVstMidiBuffer = m_ppVstMidiBuffers[iEventBuffer];
	VstEvents *pVstEvents = (VstEvents *) m_ppVstBuffers[iEventBuffer];
	::memset(pVstEvents, 0, sizeof(VstEvents));
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	unsigned int iMidiEvents = pVstBuffer->numEvents;
	if (iMidiEvents > MaxMidiEvents)
		iMidiEvents = MaxMidiEvents;
	for (unsigned int i = 0; i < iMidiEvents; ++i) {
		VstMidiEvent *pOldMidiEvent = (VstMidiEvent *) pVstBuffer->events[i];
		VstMidiEvent *pVstMidiEvent = &pVstMidiBuffer[i];
		::memcpy(pVstMidiEvent, pOldMidiEvent, sizeof(VstMidiEvent));
		pVstEvents->events[i] = (VstEvent *) pVstMidiEvent;
	}
	pVstEvents->numEvents = iMidiEvents;
}


// Swap VST event buffers...
void qtractorMidiManager::vst_events_swap (void)
{
	const unsigned short iEventBuffer = (m_iEventBuffer + 1) & 1;
	VstMidiEvent *pVstMidiBuffer = m_ppVstMidiBuffers[iEventBuffer];
	VstEvents *pVstEvents = (VstEvents *) m_ppVstBuffers[iEventBuffer];
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iEventBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
	LV2_Event_Iterator eiter;
	lv2_event_begin(&eiter, pLv2EventBuffer);
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iEventBuffer];
	lv2_atom_buffer_reset(pLv2AtomBuffer, true);
	LV2_Atom_Buffer_Iterator aiter;
	lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
#endif
	unsigned int iMidiEvents = 0;
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	while (iMidiEvents < MaxMidiEvents
		   && int(iMidiEvents) < pVstEvents->numEvents) {
		VstMidiEvent *pVstMidiEvent = &pVstMidiBuffer[iMidiEvents];
		unsigned char *pMidiData = (unsigned char *) &pVstMidiEvent->midiData[0];
		long iMidiData = sizeof(pVstMidiEvent->midiData);
	#ifdef CONFIG_MIDI_PARSER
		if (m_pMidiParser) {
			snd_seq_event_t *pEv = &m_pBuffer[iMidiEvents];
		//	snd_seq_ev_clear(pEv);
			iMidiData = snd_midi_event_encode(m_pMidiParser,
				pMidiData, iMidiData, pEv);
			if (iMidiData < 1 || pEv->type == SND_SEQ_EVENT_NONE)
				break;
			pEv->time.tick = pVstMidiEvent->deltaFrames;
		}
	#endif
	#ifdef CONFIG_LV2_EVENT
		lv2_event_write(&eiter, pVstMidiEvent->deltaFrames, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_write(&aiter, pVstMidiEvent->deltaFrames, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
		++iMidiEvents;
	}
	m_iBuffer = iMidiEvents;
	swapEventBuffers();
}

#endif	// CONFIG_VST


#ifdef CONFIG_LV2_EVENT

// Swap LV2 event buffers...
void qtractorMidiManager::lv2_events_swap (void)
{
	const unsigned short iEventBuffer = (m_iEventBuffer + 1) & 1;
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iEventBuffer];
#ifdef CONFIG_VST
	VstMidiEvent *pVstMidiBuffer = m_ppVstMidiBuffers[iEventBuffer];
	VstEvents *pVstEvents = (VstEvents *) m_ppVstBuffers[iEventBuffer];
	::memset(pVstEvents, 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iEventBuffer];
	lv2_atom_buffer_reset(pLv2AtomBuffer, true);
	LV2_Atom_Buffer_Iterator aiter;
	lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
#endif
	LV2_Event_Iterator eiter;
	lv2_event_begin(&eiter, pLv2EventBuffer);
	unsigned int iMidiEvents = 0;
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	while (iMidiEvents < MaxMidiEvents
			&& lv2_event_is_valid(&eiter)) {
		unsigned char *pMidiData;
		LV2_Event *pLv2Event = lv2_event_get(&eiter, &pMidiData);
		if (pLv2Event == NULL)
			break;
		if (pLv2Event->type == QTRACTOR_LV2_MIDI_EVENT_ID) {
			long iMidiData = pLv2Event->size;
			if (iMidiData < 1)
				break;
		#ifdef CONFIG_VST
			VstMidiEvent *pVstMidiEvent = &pVstMidiBuffer[iMidiEvents];
			if (iMidiData >= long(sizeof(pVstMidiEvent->midiData)))
				break;
		#endif
		#ifdef CONFIG_MIDI_PARSER
			if (m_pMidiParser) {
				snd_seq_event_t *pEv = &m_pBuffer[iMidiEvents];
			//	snd_seq_ev_clear(pEv);
				iMidiData = snd_midi_event_encode(m_pMidiParser,
					pMidiData, iMidiData, pEv);
				if (iMidiData < 1 || pEv->type == SND_SEQ_EVENT_NONE)
					break;
				pEv->time.tick = pLv2Event->frames;
			}
		#endif
		#ifdef CONFIG_VST
			::memset(pVstMidiEvent, 0, sizeof(VstMidiEvent));
			pVstMidiEvent->type = kVstMidiType;
			pVstMidiEvent->byteSize = sizeof(VstMidiEvent);
			pVstMidiEvent->deltaFrames = pLv2Event->frames;
			::memcpy(&pVstMidiEvent->midiData[0], pMidiData, iMidiData);
			pVstEvents->events[iMidiEvents] = (VstEvent *) pVstMidiEvent;
		#endif
		#ifdef CONFIG_LV2_ATOM
			lv2_atom_buffer_write(&aiter, pLv2Event->frames, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
			++iMidiEvents;
		}
		lv2_event_increment(&eiter);
	}
#ifdef CONFIG_VST
	pVstEvents->numEvents = iMidiEvents;
#endif
	m_iBuffer = iMidiEvents;
	swapEventBuffers();
}

#endif	// CONFIG_LV2_EVENT


#ifdef CONFIG_LV2_ATOM

// Swap LV2 atom buffers...
void qtractorMidiManager::lv2_atom_buffer_swap (void)
{
	const unsigned short iEventBuffer = (m_iEventBuffer + 1) & 1;
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iEventBuffer];
#ifdef CONFIG_VST
	VstMidiEvent *pVstMidiBuffer = m_ppVstMidiBuffers[iEventBuffer];
	VstEvents *pVstEvents = (VstEvents *) m_ppVstBuffers[iEventBuffer];
	::memset(pVstEvents, 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iEventBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
	LV2_Event_Iterator eiter;
	lv2_event_begin(&eiter, pLv2EventBuffer);
#endif
	LV2_Atom_Buffer_Iterator aiter;
	lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
	unsigned int iMidiEvents = 0;
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	while (iMidiEvents < MaxMidiEvents) {
		unsigned char *pMidiData;
		LV2_Atom_Event *pLv2AtomEvent = lv2_atom_buffer_get(&aiter, &pMidiData);
		if (pLv2AtomEvent == NULL)
			break;
		if (pLv2AtomEvent->body.type == QTRACTOR_LV2_MIDI_EVENT_ID) {
			long iMidiData = pLv2AtomEvent->body.size;
			if (iMidiData < 1)
				break;
		#ifdef CONFIG_VST
			VstMidiEvent *pVstMidiEvent = &pVstMidiBuffer[iMidiEvents];
			if (iMidiData >= long(sizeof(pVstMidiEvent->midiData)))
				break;
		#endif
		#ifdef CONFIG_MIDI_PARSER
			if (m_pMidiParser) {
				snd_seq_event_t *pEv = &m_pBuffer[iMidiEvents];
			//	snd_seq_ev_clear(pEv);
				iMidiData = snd_midi_event_encode(m_pMidiParser,
					pMidiData, iMidiData, pEv);
				if (iMidiData < 1 || pEv->type == SND_SEQ_EVENT_NONE)
					break;
				pEv->time.tick = pLv2AtomEvent->time.frames;
			}
		#endif
		#ifdef CONFIG_VST
			::memset(pVstMidiEvent, 0, sizeof(VstMidiEvent));
			pVstMidiEvent->type = kVstMidiType;
			pVstMidiEvent->byteSize = sizeof(VstMidiEvent);
			pVstMidiEvent->deltaFrames = pLv2AtomEvent->time.frames;
			::memcpy(&pVstMidiEvent->midiData[0], pMidiData, iMidiData);
			pVstEvents->events[iMidiEvents] = (VstEvent *) pVstMidiEvent;
		#endif
		#ifdef CONFIG_LV2_EVENT
			lv2_event_write(&eiter, pLv2AtomEvent->time.frames, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
			++iMidiEvents;
		}
		lv2_atom_buffer_increment(&aiter);
	}
#ifdef CONFIG_VST
	pVstEvents->numEvents = iMidiEvents;
#endif
	m_iBuffer = iMidiEvents;
	swapEventBuffers();
}


// Resize LV2 atom buffers if necessary.
void qtractorMidiManager::lv2_atom_buffer_resize ( unsigned int iMinBufferSize )
{
	if (iMinBufferSize < m_iLv2AtomBufferSize)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const bool bPlaying = pSession->isPlaying();
	if (bPlaying)
		pSession->lock();

	m_iLv2AtomBufferSize += iMinBufferSize;

	for (unsigned short i = 0; i < 2; ++i) {
		if (m_ppLv2AtomBuffers[i])
			lv2_atom_buffer_free(m_ppLv2AtomBuffers[i]);
		m_ppLv2AtomBuffers[i] = lv2_atom_buffer_new(m_iLv2AtomBufferSize,
			qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Chunk),
			qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Sequence), (i & 1) == 0);
	}

	if (bPlaying)
		pSession->unlock();
}

#endif	// CONFIG_LV2_ATOM


// Some default factory options.
void qtractorMidiManager::setDefaultAudioOutputBus ( bool bAudioOutputBus )
{
	g_bAudioOutputBus = bAudioOutputBus;
}

bool qtractorMidiManager::isDefaultAudioOutputBus (void)
{
	return g_bAudioOutputBus;
}


void qtractorMidiManager::setDefaultAudioOutputAutoConnect (
	bool bAudioOutputAutoConnect )
{
	g_bAudioOutputAutoConnect = bAudioOutputAutoConnect;
}

bool qtractorMidiManager::isDefaultAudioOutputAutoConnect (void)
{
	return g_bAudioOutputAutoConnect;
}


// Output bus mode accessors.
void qtractorMidiManager::setAudioOutputBus ( bool bAudioOutputBus )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->lock();
	deleteAudioOutputBus();

	m_bAudioOutputBus = bAudioOutputBus;

	createAudioOutputBus();
	pSession->unlock();
}


void qtractorMidiManager::resetAudioOutputBus (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->lock();

	qtractorBus::ConnectList outputs;

	if (m_bAudioOutputBus && m_pAudioOutputBus)
		outputs.copy(m_pAudioOutputBus->outputs());

	createAudioOutputBus();

	if (m_bAudioOutputBus && m_pAudioOutputBus)
		m_pAudioOutputBus->outputs().copy(outputs);

	pSession->unlock();
}


// Create audio output stuff...
void qtractorMidiManager::createAudioOutputBus (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	deleteAudioOutputBus();

	// Whether audio output bus is here owned, or...
	if (m_bAudioOutputBus) {
		// Owned, not part of audio engine...
		m_pAudioOutputBus
			= new qtractorAudioBus(pAudioEngine, m_pPluginList->name(),
				qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex),
				false, m_pPluginList->channels());
		m_pAudioOutputBus->setAutoConnect(m_bAudioOutputAutoConnect);
		if (pAudioEngine->isActivated()) {
			pAudioEngine->addBusEx(m_pAudioOutputBus);
			if (m_pAudioOutputBus->open())
				m_pAudioOutputBus->autoConnect();
		}
	} else {
		// Find named audio output bus, if any...
		if (!m_sAudioOutputBusName.isEmpty())
			m_pAudioOutputBus = static_cast<qtractorAudioBus *> (
				pAudioEngine->findOutputBus(m_sAudioOutputBusName));
		// Otherwise bus gets to be the first available output bus...
		if (m_pAudioOutputBus == NULL) {
			for (qtractorBus *pBus = (pAudioEngine->buses()).first();
					pBus; pBus = pBus->next()) {
				if (pBus->busMode() & qtractorBus::Output) {
					m_pAudioOutputBus = static_cast<qtractorAudioBus *> (pBus);
					break;
				}
			}
		}
	}
}


// Destroy audio Outputnome stuff.
void qtractorMidiManager::deleteAudioOutputBus (void)
{
	// Owned, not part of audio engine...
	if (m_bAudioOutputBus && m_pAudioOutputBus) {
		m_pAudioOutputBus->close();
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
			if (pAudioEngine)
				pAudioEngine->removeBusEx(m_pAudioOutputBus);
		}
		delete m_pAudioOutputBus;
	}

	// Done.
	m_pAudioOutputBus = NULL;
}


// Instrument map builder.
void qtractorMidiManager::updateInstruments (void)
{
	m_instruments.clear();

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		int iIndex = 0;
		qtractorPlugin::Program program;
		Banks& banks = m_instruments[(pPlugin->type())->name()];
		while (pPlugin->getProgram(iIndex++, program)) {
			Bank& bank = banks[program.bank];
			if (bank.name.isEmpty()) {
				bank.name = QObject::tr("%1 - Bank %2")
					.arg(program.bank)
					.arg(banks.count() - 1);
			}
			bank.progs[program.prog] = program.name.simplified();
		}
	}
}


// end of qtractorMidiBuffer.cpp
