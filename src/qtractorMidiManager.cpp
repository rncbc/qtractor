// qtractorMidiManager.cpp
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

#include "qtractorMidiManager.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"
#include "qtractorPlugin.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"
#include "qtractorAudioEngine.h"
#include "qtractorAudioMonitor.h"

#include "qtractorMainForm.h"
#include "qtractorTracks.h"
#include "qtractorMixer.h"

#include <QThread>
#include <QMutex>
#include <QWaitCondition>


// Specific controller definitions
#define BANK_SELECT_MSB		0x00
#define BANK_SELECT_LSB		0x20

#define ALL_SOUND_OFF		0x78
#define ALL_CONTROLLERS_OFF	0x79
#define ALL_NOTES_OFF		0x7b


//----------------------------------------------------------------------
// class qtractorMidiSyncThread -- MIDI sync thread decl.
//

class qtractorMidiSyncThread : public QThread
{
public:

	// Constructor.
	qtractorMidiSyncThread(unsigned int iSyncSize = 128);

	// Destructor.
	~qtractorMidiSyncThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync(qtractorMidiSyncItem *pSyncItem = nullptr);

protected:

	// The main thread executive.
	void run();

private:

	// The thread launcher queue instance reference.
	unsigned int m_iSyncSize;
	unsigned int m_iSyncMask;

	qtractorMidiSyncItem **m_ppSyncItems;

	volatile unsigned int  m_iSyncRead;
	volatile unsigned int  m_iSyncWrite;

	// Whether the thread is logically running.
	volatile bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;
};


//----------------------------------------------------------------------
// class qtractorMidiSyncThread -- MIDI sync thread decl.
//

// Constructor.
qtractorMidiSyncThread::qtractorMidiSyncThread ( unsigned int iSyncSize )
{
	m_iSyncSize = (1 << 7);
	while (m_iSyncSize < iSyncSize)
		m_iSyncSize <<= 1;
	m_iSyncMask = (m_iSyncSize - 1);
	m_ppSyncItems = new qtractorMidiSyncItem * [m_iSyncSize];
	m_iSyncRead   = 0;
	m_iSyncWrite  = 0;

	::memset(m_ppSyncItems, 0, m_iSyncSize * sizeof(qtractorMidiSyncItem *));

	m_bRunState = false;
}


// Destructor.
qtractorMidiSyncThread::~qtractorMidiSyncThread (void)
{
	delete [] m_ppSyncItems;
}


// Thread run state accessors.
void qtractorMidiSyncThread::setRunState ( bool bRunState )
{
	QMutexLocker locker(&m_mutex);

	m_bRunState = bRunState;
}

bool qtractorMidiSyncThread::runState (void) const
{
	return m_bRunState;
}


// The main thread executive.
void qtractorMidiSyncThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiSyncThread[%p]::run(): started...", this);
#endif

	m_mutex.lock();

	m_bRunState = true;

	while (m_bRunState) {
		// Wait for sync...
		m_cond.wait(&m_mutex);
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorMidiSyncThread[%p]::run(): waked.", this);
	#endif
		// Call control process cycle.
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			qtractorMidiSyncItem *pSyncItem = m_ppSyncItems[r];
			if (pSyncItem->isWaitSync()) {
				pSyncItem->processSync();
				pSyncItem->setWaitSync(false);
			}
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiSyncThread[%p]::run(): stopped.", this);
#endif
}


// Wake from executive wait condition.
void qtractorMidiSyncThread::sync ( qtractorMidiSyncItem *pSyncItem )
{
	if (pSyncItem == nullptr) {
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			qtractorMidiSyncItem *pSyncItem = m_ppSyncItems[r];
			if (pSyncItem)
				pSyncItem->setWaitSync(false);
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
	} else {
		// !pSyncItem->isWaitSync()
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
			pSyncItem->setWaitSync(true);
			m_ppSyncItems[w] = pSyncItem;
			m_iSyncWrite = (w + 1) & m_iSyncMask;
		}
	}

	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else qDebug("qtractorMidiSyncThread[%p]::sync(): tryLock() failed.", this);
#endif
}

//----------------------------------------------------------------------
// class qtractorMidiSyncItem -- MIDI sync item impl.
//

qtractorMidiSyncThread *qtractorMidiSyncItem::g_pSyncThread         = nullptr;
unsigned int            qtractorMidiSyncItem::g_iSyncThreadRefCount = 0;

// Constructor.
qtractorMidiSyncItem::qtractorMidiSyncItem (void)
	: m_bWaitSync(false)
{
	if (++g_iSyncThreadRefCount == 1 && g_pSyncThread == nullptr) {
		g_pSyncThread = new qtractorMidiSyncThread();
		g_pSyncThread->start(QThread::HighestPriority);
	}
}


// Destructor.
qtractorMidiSyncItem::~qtractorMidiSyncItem (void)
{
	if (--g_iSyncThreadRefCount == 0 && g_pSyncThread != nullptr) {
		// Try to wake and terminate executive thread,
		// but give it a bit of time to cleanup...
		if (g_pSyncThread->isRunning()) do {
			g_pSyncThread->setRunState(false);
		//	g_pSyncThread->terminate();
			g_pSyncThread->sync();
		} while (!g_pSyncThread->wait(100));
		delete g_pSyncThread;
		g_pSyncThread = nullptr;
	}
}


// Sync thread state flags accessors.
void qtractorMidiSyncItem::setWaitSync ( bool bWaitSync )
{
	m_bWaitSync = bWaitSync;
}

bool qtractorMidiSyncItem::isWaitSync (void) const
{
	return m_bWaitSync;
}


// Post/schedule item for process sync. (static)
void qtractorMidiSyncItem::syncItem ( qtractorMidiSyncItem *pSyncItem )
{
	if (g_pSyncThread)
		g_pSyncThread->sync(pSyncItem);
}


//----------------------------------------------------------------------
// class qtractorMidiInputBuffer -- MIDI input buffer impl.
//

// Input event enqueuer.
bool qtractorMidiInputBuffer::enqueue (
	snd_seq_event_t *pEv, unsigned long iTime )
{
	if (pEv->type == SND_SEQ_EVENT_NOTE || // Unlikely real-time input...
		pEv->type == SND_SEQ_EVENT_NOTEON) {
		if (m_pWetGainSubject) {
			const float fWetGain = m_pWetGainSubject->value();
			int val = int(fWetGain * float(pEv->data.note.velocity));
			if (val < 1)
				val = 1;
			else
			if (val > 127)
				val = 127;
			pEv->data.note.velocity = val;
		}
	}

	return qtractorMidiBuffer::push(pEv, iTime);
}


//----------------------------------------------------------------------
// class qtractorMidiOutputBuffer -- MIDI output buffer impl.
//

// Process buffer (in asynchronous controller thread).
void qtractorMidiOutputBuffer::processSync (void)
{
	if (m_pMidiBus == nullptr)
		return;

	if (!(m_pMidiBus->busMode() & qtractorBus::Output))
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	const unsigned long iTimeStart = pMidiEngine->timeStartEx();
	qtractorTimeScale::Cursor cursor(pSession->timeScale());

	qtractorMidiManager *pMidiManager = nullptr;
	if (m_pMidiBus->pluginList_out())
		pMidiManager = (m_pMidiBus->pluginList_out())->midiManager();
	qtractorMidiMonitor *pMidiMonitor = m_pMidiBus->midiMonitor_out();

	snd_seq_event_t *pEv = m_outputBuffer.peek();
	while (pEv) {
		qtractorTimeScale::Node *pNode = cursor.seekFrame(pEv->time.tick);
		const unsigned long iTime = pNode->tickFromFrame(pEv->time.tick);
		const unsigned long tick = (iTime > iTimeStart ? iTime - iTimeStart : 0);
		qtractorMidiEvent::EventType type = qtractorMidiEvent::EventType(0);
		unsigned short val = 0;
		switch (pEv->type) {
		case SND_SEQ_EVENT_NOTE:
		case SND_SEQ_EVENT_NOTEON:
			type = qtractorMidiEvent::NOTEON;
			val  = pEv->data.note.velocity;
			if (m_pGainSubject) {
				val = (unsigned short) (m_pGainSubject->value() * float(val));
				if (val < 1)
					val = 1;
				else
				if (val > 127)
					val = 127;
				pEv->data.note.velocity = val;
			}
			// Fall thru...
		default:
			break;
		}
	#ifdef CONFIG_DEBUG_0
		// - show event for debug purposes...
		fprintf(stderr, "MIDI Out %06lu 0x%02x", tick, pEv->type);
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
	#endif
		// Schedule into sends/output bus...
		snd_seq_ev_set_source(pEv, m_pMidiBus->alsaPort());
		snd_seq_ev_set_subs(pEv);
		snd_seq_ev_schedule_tick(pEv, pMidiEngine->alsaQueue(), 0, tick);
		snd_seq_event_output(pMidiEngine->alsaSeq(), pEv);
		if (pMidiManager)
			pMidiManager->queued(pEv, pEv->time.tick);
		if (pMidiMonitor)
			pMidiMonitor->enqueue(type, val, tick);
		// And next...
		pEv = m_outputBuffer.next();
	}

	pMidiEngine->flush();
}


//----------------------------------------------------------------------
// class qtractorMidiManager -- MIDI internal plugin list manager.
//

bool qtractorMidiManager::g_bAudioOutputBus = false;
bool qtractorMidiManager::g_bAudioOutputAutoConnect = true;

// AG: Buffer size large enough to hold some sysex events.
const long c_iMaxMidiData = 512;

// Constructor.
qtractorMidiManager::qtractorMidiManager (
	qtractorPluginList *pPluginList, unsigned int iBufferSize ) :
	m_pSyncItem(new SyncItem(this)),
	m_pPluginList(pPluginList),
	m_directBuffer(iBufferSize >> 1),
	m_queuedBuffer(iBufferSize),
	m_postedBuffer(iBufferSize),
	m_controllerBuffer(iBufferSize >> 2),
	m_iEventBuffer(0),
#ifdef CONFIG_MIDI_PARSER
	m_pMidiParser(nullptr),
#endif
	m_bAudioOutputBus(pPluginList->isAudioOutputBus()),
	m_sAudioOutputBusName(pPluginList->audioOutputBusName()),
	m_pAudioOutputBus(nullptr),
	m_bAudioOutputAutoConnect(pPluginList->isAudioOutputAutoConnect()),
	m_bAudioOutputMonitor(pPluginList->isAudioOutputMonitor()),
	m_pAudioOutputMonitor(nullptr),
	m_iCurrentBank(pPluginList->midiBank()),
	m_iCurrentProg(pPluginList->midiProg()),
	m_iPendingBankMSB(-1),
	m_iPendingBankLSB(-1),
	m_iPendingProg(-1)
{
	const unsigned int MaxMidiEvents = bufferSize();

#ifdef CONFIG_MIDI_PARSER
	if (snd_midi_event_new(c_iMaxMidiData, &m_pMidiParser) == 0)
		snd_midi_event_no_status(m_pMidiParser, 1);
#endif

	// Create_event buffers...
#ifdef CONFIG_DSSI
	m_pDssiEvents = new snd_seq_event_t [MaxMidiEvents];
	m_iDssiEvents = 0;
#endif
#ifdef CONFIG_VST2
	const unsigned int Vst2BufferSize = sizeof(VstEvents)
		+ MaxMidiEvents * sizeof(VstMidiEvent *);
#endif
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	const unsigned int Lv2EventBufferSize
		= (sizeof(LV2_Event) + 4) * MaxMidiEvents;
#endif
#ifdef CONFIG_LV2_ATOM
	m_iLv2AtomBufferSize = (sizeof(LV2_Atom_Event) + 4) * MaxMidiEvents;
#endif
#endif
	for (unsigned short i = 0; i < 2; ++i) {
		m_ppEventBuffers[i] = new qtractorMidiBuffer(MaxMidiEvents);
	#ifdef CONFIG_VST2
		m_ppVst2Buffers[i] = new unsigned char [Vst2BufferSize];
		m_ppVst2MidiBuffers[i] = new VstMidiEvent [MaxMidiEvents];
	#endif
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_EVENT
		m_ppLv2EventBuffers[i] = lv2_event_buffer_new(Lv2EventBufferSize,
			LV2_EVENT_AUDIO_STAMP);
	#endif
	#ifdef CONFIG_LV2_ATOM
		m_ppLv2AtomBuffers[i] = lv2_atom_buffer_new(m_iLv2AtomBufferSize,
			qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Chunk),
			qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Sequence), (i & 1) == 0);
	#endif
	#endif
	}

	createAudioOutputBus();
}


// Destructor.
qtractorMidiManager::~qtractorMidiManager (void)
{
	deleteAudioOutputBus();

	if (m_pAudioOutputMonitor)
		delete m_pAudioOutputMonitor;

	// Destroy event_buffers...
	for (unsigned short i = 0; i < 2; ++i) {
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_free(m_ppLv2AtomBuffers[i]);
	#endif
	#ifdef CONFIG_LV2_EVENT
		::free(m_ppLv2EventBuffers[i]);
	#endif
	#endif
	#ifdef CONFIG_VST2
		delete [] m_ppVst2MidiBuffers[i];
		delete [] m_ppVst2Buffers[i];
	#endif
		delete m_ppEventBuffers[i];
	}

#ifdef CONFIG_DSSI
	delete [] m_pDssiEvents;
	m_pDssiEvents = nullptr;
	m_iDssiEvents = 0;
#endif

#ifdef CONFIG_MIDI_PARSER
	if (m_pMidiParser) {
		snd_midi_event_free(m_pMidiParser);
		m_pMidiParser = nullptr;
	}
#endif

	delete m_pSyncItem;
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
	else
	if (pEvent->type == SND_SEQ_EVENT_PGMCHANGE)
		m_iPendingProg = pEvent->data.control.value;

	return m_directBuffer.push(pEvent);
}


// Queued buffering.
bool qtractorMidiManager::queued (
	snd_seq_event_t *pEvent, unsigned long iTime, unsigned long iTimeOff )
{
	if (pEvent->type == SND_SEQ_EVENT_NOTE && iTime < iTimeOff) {
		snd_seq_event_t ev = *pEvent;
		ev.type = SND_SEQ_EVENT_NOTEON;
		if (!m_queuedBuffer.insert(&ev, iTime))
			return false;
		ev.type = SND_SEQ_EVENT_NOTEOFF;
		ev.data.note.velocity = 0;
		ev.data.note.duration = 0;
		return m_postedBuffer.insert(&ev, iTimeOff);
	}

	if (pEvent->type == SND_SEQ_EVENT_NOTEOFF)
		return m_postedBuffer.insert(pEvent, iTime);
	else
		return m_queuedBuffer.insert(pEvent, iTime);
}


// Clears buffers for processing.
void qtractorMidiManager::clear (void)
{
	// Reset event buffers...
	for (unsigned short i = 0; i < 2; ++i) {
		m_ppEventBuffers[i]->clear();
	#ifdef CONFIG_VST2
		::memset(m_ppVst2Buffers[i], 0, sizeof(VstEvents));
	#endif
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_EVENT
		LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[i];
		lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
			(unsigned char *) (pLv2EventBuffer + 1));
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_reset(m_ppLv2AtomBuffers[i], (i & 1) == 0);
	#endif
	#endif
	}

	m_iEventBuffer = 0;
}


// Process buffers (merge).
void qtractorMidiManager::process (
	unsigned long iTimeStart, unsigned long iTimeEnd )
{
	clear();

	// Address the MIDI input buffer this way...
	const unsigned short iInputBuffer = m_iEventBuffer & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iInputBuffer];

	// Check for program changes and controller messages...
	if (m_iPendingProg >= 0 || !m_controllerBuffer.isEmpty())
		qtractorMidiSyncItem::syncItem(m_pSyncItem);

	// Merge events in buffer for plugin processing...
	snd_seq_event_t *pEv0 = m_directBuffer.peek();
	snd_seq_event_t *pEv1 = m_queuedBuffer.peek();
	snd_seq_event_t *pEv2 = m_postedBuffer.peek();

	// Direct events...
	while (pEv0) {
		pEventBuffer->push(pEv0, pEv0->time.tick);
		pEv0 = m_directBuffer.next();
	}

	// Queued/posted events...
	while ((pEv1 && pEv1->time.tick < iTimeEnd)
		|| (pEv2 && pEv2->time.tick < iTimeEnd)) {
		while (pEv1 && pEv1->time.tick < iTimeEnd
			&& ((pEv2 && pEv2->time.tick >= pEv1->time.tick) || !pEv2)) {
			pEventBuffer->push(pEv1,
				(pEv1->time.tick > iTimeStart
					? pEv1->time.tick - iTimeStart : 0));
			pEv1 = m_queuedBuffer.next();
		}
		while (pEv2 && pEv2->time.tick < iTimeEnd
			&& ((pEv1 && pEv1->time.tick > pEv2->time.tick) || !pEv1)) {
			pEventBuffer->push(pEv2,
				(pEv2->time.tick > iTimeStart
					? pEv2->time.tick - iTimeStart : 0));
			pEv2 = m_postedBuffer.next();
		}
	}

#ifdef CONFIG_DEBUG_0
	const unsigned int iEventCount = pEventBuffer->count();
	for (unsigned int i = 0; i < iEventCount; ++i) {
		snd_seq_event_t *pEv = pEventBuffer->at(i);
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

	// Process/decode into other/plugin event buffers...
	processEventBuffers();

	// Now's time to process the plugins as usual...
	if (m_pAudioOutputBus) {
		const unsigned int nframes = iTimeEnd - iTimeStart;
		if (m_bAudioOutputBus) {
			m_pAudioOutputBus->process_prepare(nframes);
			m_pPluginList->process(m_pAudioOutputBus->out(), nframes);
			if (m_bAudioOutputMonitor)
				m_pAudioOutputMonitor->process_meter(
					m_pAudioOutputBus->out(), nframes);
			m_pAudioOutputBus->process_commit(nframes);
		} else {
			m_pAudioOutputBus->buffer_prepare(nframes);
			m_pPluginList->process(m_pAudioOutputBus->buffer(), nframes);
			if (m_bAudioOutputMonitor)
				m_pAudioOutputMonitor->process_meter(
					m_pAudioOutputBus->buffer(), nframes);
			m_pAudioOutputBus->buffer_commit(nframes);
		}
	}
}


// Process buffers (in asynchronous controller thread).
void qtractorMidiManager::processSync (void)
{
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
}


// Resets all buffering.
void qtractorMidiManager::reset (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
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

	m_pPluginList->resetLatency();
	m_pPluginList->resetBuffers();

	m_controllerBuffer.clear();

	m_iPendingBankMSB = -1;
	m_iPendingBankLSB = -1;
	m_iPendingProg    = -1;

	pSession->unlock();
}


// Direct MIDI controller helper.
void qtractorMidiManager::setController (
	unsigned short iChannel, int iController, int iValue )
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	ev.type = SND_SEQ_EVENT_CONTROLLER;
	ev.data.control.channel = iChannel;
	ev.data.control.param   = iController;
	ev.data.control.value   = iValue;

	direct(&ev);
}


// Shut-off MIDI channel (panic)...
void qtractorMidiManager::shutOff ( unsigned short iChannel )
{
	setController(iChannel, ALL_SOUND_OFF, 0);
	setController(iChannel, ALL_NOTES_OFF, 0);
	setController(iChannel, ALL_CONTROLLERS_OFF, 0);
}


// Factory (proxy) methods.
qtractorMidiManager *qtractorMidiManager::createMidiManager (
	qtractorPluginList *pPluginList )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return nullptr;

	qtractorMidiManager *pMidiManager
		= new qtractorMidiManager(pPluginList);
	pSession->addMidiManager(pMidiManager);

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManager::createMidiManager(%p)", pMidiManager);
#endif

	return pMidiManager;
}


void qtractorMidiManager::deleteMidiManager (
	qtractorMidiManager *pMidiManager )
{
	if (pMidiManager == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManager::deleteMidiManager(%p)", pMidiManager);
#endif

	pSession->removeMidiManager(pMidiManager);

	delete pMidiManager;
}

// Process specific MIDI input buffer (eg. insert/merge).
void qtractorMidiManager::processInputBuffer (
	qtractorMidiInputBuffer *pMidiInputBuffer, unsigned long t0 )
{
	snd_seq_event_t *pEv;

	qtractorSubject *pDryGainSubject = pMidiInputBuffer->dryGainSubject();

	// Address the MIDI input buffer this way...
	const unsigned short iInputBuffer = m_iEventBuffer & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iInputBuffer];
	const unsigned int iEventCount = pEventBuffer->count();

	for (unsigned int i = 0; i < iEventCount; ++i) {
		pEv = pEventBuffer->at(i);
		// Apply gain (through/dry)...
		if (pDryGainSubject
			&& (pEv->type == SND_SEQ_EVENT_NOTE || // Unlikely real-time input...
				pEv->type == SND_SEQ_EVENT_NOTEON)) {
			const float fDryGain = pDryGainSubject->value();
			int val = int(fDryGain * float(pEv->data.note.velocity));
			if (val < 1)
				val = 1;
			else
			if (val > 127)
				val = 127;
			pEv->data.note.velocity = val;
		}
		// Merge input through...
		if (!pMidiInputBuffer->insert(pEv, t0 + pEv->time.tick))
			break;
	}

	resetInputBuffers();

	pEv = pMidiInputBuffer->peek();
	while (pEv) {
		const unsigned long t1 = pEv->time.tick;
		pEv->time.tick = (t1 > t0 ? t1 - t0 : 0);
		pEventBuffer->push(pEv, pEv->time.tick);
		pEv = pMidiInputBuffer->next();
	}

	processEventBuffers();
}


// Process/decode into other/plugin event buffers...
void qtractorMidiManager::processEventBuffers (void)
{
#ifdef CONFIG_MIDI_PARSER

	if (m_pMidiParser == nullptr)
		return;

	const unsigned short iInputBuffer = m_iEventBuffer & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iInputBuffer];
	const unsigned int iEventCount = pEventBuffer->count();
#ifdef CONFIG_VST2
	VstEvents *pVst2Events = (VstEvents *) m_ppVst2Buffers[iInputBuffer];
#endif
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iInputBuffer];
	LV2_Event_Iterator eiter;
	lv2_event_begin(&eiter, pLv2EventBuffer);
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iInputBuffer];
	LV2_Atom_Buffer_Iterator aiter;
	lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
#endif
#endif
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	unsigned int iMidiEvents = 0;
#ifdef CONFIG_DSSI
	m_iDssiEvents = 0;
#endif
	// AG: Untangle treatment of VST2 and LV2 plugins,
	// so that we can use a larger buffer for the latter...
#ifdef CONFIG_VST2
	unsigned int iVst2MidiEvents = 0;
#endif
	for (unsigned int i = 0; i < iEventCount; ++i) {
		snd_seq_event_t *pEv = pEventBuffer->at(i);
		unsigned char midiData[c_iMaxMidiData];
		unsigned char *pMidiData = &midiData[0];
		long iMidiData = sizeof(midiData);
		iMidiData = snd_midi_event_decode(m_pMidiParser,
			pMidiData, iMidiData, pEv);
		if (iMidiData < 0)
			break;
	#ifdef CONFIG_DEBUG_0
		// - show event for debug purposes...
		fprintf(stderr, "MIDI Raw %06u {", pEv->time.tick);
		for (long i = 0; i < iMidiData; ++i)
			fprintf(stderr, " %02x", pMidiData[i]);
		fprintf(stderr, " }\n");
	#endif
	#ifdef CONFIG_DSSI
		m_pDssiEvents[m_iDssiEvents++] = *pEv;
	#endif
	#ifdef CONFIG_VST2
		VstMidiEvent *pVst2MidiBuffer = m_ppVst2MidiBuffers[iInputBuffer];
		VstMidiEvent *pVst2MidiEvent = &pVst2MidiBuffer[iVst2MidiEvents];
		if (iMidiData < long(sizeof(pVst2MidiEvent->midiData))) {
			::memset(pVst2MidiEvent, 0, sizeof(VstMidiEvent));
			pVst2MidiEvent->type = kVstMidiType;
			pVst2MidiEvent->byteSize = sizeof(VstMidiEvent);
			pVst2MidiEvent->deltaFrames = pEv->time.tick;
			::memcpy(&pVst2MidiEvent->midiData[0], pMidiData, iMidiData);
			pVst2Events->events[iVst2MidiEvents++] = (VstEvent *) pVst2MidiEvent;
		}
	#endif
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_EVENT
		lv2_event_write(&eiter, pEv->time.tick, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_write(&aiter, pEv->time.tick, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#endif
		if (++iMidiEvents >= MaxMidiEvents)
			break;
	}
#ifdef CONFIG_VST2
	pVst2Events->numEvents = iVst2MidiEvents;
//	pVst2Events->reserved = 0;
#endif

#endif	// CONFIG_MIDI_PARSER
}


// Reset event buffers (input only)
void qtractorMidiManager::resetInputBuffers (void)
{
	const unsigned short iInputBuffer = m_iEventBuffer & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iInputBuffer];

	pEventBuffer->reset();

#ifdef CONFIG_DSSI
	m_iDssiEvents = 0;
#endif
#ifdef CONFIG_VST2
	::memset(m_ppVst2Buffers[iInputBuffer], 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iInputBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
#endif
#ifdef CONFIG_LV2_ATOM
	lv2_atom_buffer_reset(m_ppLv2AtomBuffers[iInputBuffer], true);
#endif
#endif
}


// Reset event buffers (output only)
void qtractorMidiManager::resetOutputBuffers (void)
{
	const unsigned short iOutputBuffer = (m_iEventBuffer + 1) & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iOutputBuffer];

	pEventBuffer->reset();

#ifdef CONFIG_VST2
	::memset(m_ppVst2Buffers[iOutputBuffer], 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iOutputBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
#endif
#ifdef CONFIG_LV2_ATOM
	lv2_atom_buffer_reset(m_ppLv2AtomBuffers[iOutputBuffer], false);
#endif
#endif
}


// Swap event buffers (in for out and vice-versa)
void qtractorMidiManager::swapEventBuffers (void)
{
	const unsigned short iInputBuffer = m_iEventBuffer & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iInputBuffer];

	pEventBuffer->reset();

#ifdef CONFIG_DSSI
	m_iDssiEvents = 0;
#endif
#ifdef CONFIG_VST2
	::memset(m_ppVst2Buffers[iInputBuffer], 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iInputBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
#endif
#ifdef CONFIG_LV2_ATOM
	lv2_atom_buffer_reset(m_ppLv2AtomBuffers[iInputBuffer], false);
#endif
#endif

	++m_iEventBuffer;
}


#ifdef CONFIG_VST2

// Copy VST2 event buffer (output)...
void qtractorMidiManager::vst2_events_copy ( VstEvents *pVst2Buffer )
{
	const unsigned short iOutputBuffer = (m_iEventBuffer + 1) & 1;
	VstMidiEvent *pVst2MidiBuffer = m_ppVst2MidiBuffers[iOutputBuffer];
	VstEvents *pVst2Events = (VstEvents *) m_ppVst2Buffers[iOutputBuffer];
	::memset(pVst2Events, 0, sizeof(VstEvents));
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	unsigned int iMidiEvents = pVst2Buffer->numEvents;
	if (iMidiEvents > MaxMidiEvents)
		iMidiEvents = MaxMidiEvents;
	for (unsigned int i = 0; i < iMidiEvents; ++i) {
		VstMidiEvent *pOldMidiEvent = (VstMidiEvent *) pVst2Buffer->events[i];
		VstMidiEvent *pVst2MidiEvent = &pVst2MidiBuffer[i];
		::memcpy(pVst2MidiEvent, pOldMidiEvent, sizeof(VstMidiEvent));
		pVst2Events->events[i] = (VstEvent *) pVst2MidiEvent;
	}
	pVst2Events->numEvents = iMidiEvents;
}


// Swap VST2 event buffers...
void qtractorMidiManager::vst2_events_swap (void)
{
	const unsigned short iOutputBuffer = (m_iEventBuffer + 1) & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iOutputBuffer];
	VstMidiEvent *pVst2MidiBuffer = m_ppVst2MidiBuffers[iOutputBuffer];
	VstEvents *pVst2Events = (VstEvents *) m_ppVst2Buffers[iOutputBuffer];
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iOutputBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
	LV2_Event_Iterator eiter;
	lv2_event_begin(&eiter, pLv2EventBuffer);
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iOutputBuffer];
	lv2_atom_buffer_reset(pLv2AtomBuffer, true);
	LV2_Atom_Buffer_Iterator aiter;
	lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
#endif
#endif
	unsigned int iMidiEvents = 0;
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	while (iMidiEvents < MaxMidiEvents
		   && int(iMidiEvents) < pVst2Events->numEvents) {
		VstMidiEvent *pVst2MidiEvent = &pVst2MidiBuffer[iMidiEvents];
		unsigned char *pMidiData = (unsigned char *) &pVst2MidiEvent->midiData[0];
		long iMidiData = sizeof(pVst2MidiEvent->midiData);
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
	#ifdef CONFIG_MIDI_PARSER
		if (m_pMidiParser) {
			iMidiData = snd_midi_event_encode(m_pMidiParser,
				pMidiData, iMidiData, &ev);
			if (iMidiData < 1 || ev.type == SND_SEQ_EVENT_NONE)
				break;
			ev.time.tick = pVst2MidiEvent->deltaFrames;
		}
	#endif
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_EVENT
		lv2_event_write(&eiter, pVst2MidiEvent->deltaFrames, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_write(&aiter, pVst2MidiEvent->deltaFrames, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#endif
		pEventBuffer->push(&ev, ev.time.tick);
		++iMidiEvents;
	}
	swapEventBuffers();
}

#endif	// CONFIG_VST2


#ifdef CONFIG_MIDI_PARSER

// Parse MIDI output and swap event buffers.
// (esp. used by VST3 and CLAP)
void qtractorMidiManager::swapOutputBuffers (void)
{
	if (m_pMidiParser == nullptr) {
		swapEventBuffers();
		return;
	}

	const unsigned short iOutputBuffer = (m_iEventBuffer + 1) & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iOutputBuffer];
	const unsigned int iEventCount = pEventBuffer->count();
#ifdef CONFIG_VST2
	VstMidiEvent *pVst2MidiBuffer = m_ppVst2MidiBuffers[iOutputBuffer];
	VstEvents *pVst2Events = (VstEvents *) m_ppVst2Buffers[iOutputBuffer];
	::memset(pVst2Events, 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iOutputBuffer];
	lv2_event_buffer_reset(pLv2EventBuffer, LV2_EVENT_AUDIO_STAMP,
		(unsigned char *) (pLv2EventBuffer + 1));
	LV2_Event_Iterator eiter;
	lv2_event_begin(&eiter, pLv2EventBuffer);
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iOutputBuffer];
	lv2_atom_buffer_reset(pLv2AtomBuffer, true);
	LV2_Atom_Buffer_Iterator aiter;
	lv2_atom_buffer_begin(&aiter, pLv2AtomBuffer);
#endif
#endif
	unsigned int iMidiEvents = 0;
	const unsigned int MaxMidiEvents = (bufferSize() << 1);
	for (unsigned int i = 0; i < iEventCount; ++i) {
		snd_seq_event_t *pEv = pEventBuffer->at(i);
		unsigned char midiData[4];
		unsigned char *pMidiData = &midiData[0];
		long iMidiData = sizeof(midiData);
		iMidiData = snd_midi_event_decode(m_pMidiParser,
			pMidiData, iMidiData, pEv);
		if (iMidiData < 1)
			break;
	#ifdef CONFIG_VST2
		VstMidiEvent *pVst2MidiEvent = &pVst2MidiBuffer[iMidiEvents];
		if (iMidiData >= long(sizeof(pVst2MidiEvent->midiData)))
			break;
		::memset(pVst2MidiEvent, 0, sizeof(VstMidiEvent));
		pVst2MidiEvent->type = kVstMidiType;
		pVst2MidiEvent->byteSize = sizeof(VstMidiEvent);
		pVst2MidiEvent->deltaFrames = pEv->time.tick;
		::memcpy(&pVst2MidiEvent->midiData[0], pMidiData, iMidiData);
		pVst2Events->events[iMidiEvents] = (VstEvent *) pVst2MidiEvent;
	#endif
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_EVENT
		lv2_event_write(&eiter, pEv->time.tick, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#ifdef CONFIG_LV2_ATOM
		lv2_atom_buffer_write(&aiter, pEv->time.tick, 0,
			QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
	#endif
	#endif
		if (++iMidiEvents >= MaxMidiEvents)
			break;
	}
#ifdef CONFIG_VST2
	pVst2Events->numEvents = iMidiEvents;
#endif

	swapEventBuffers();
}

#endif	// CONFIG_MIDI_PARSER


#ifdef CONFIG_LV2

#ifdef CONFIG_LV2_EVENT

// Swap LV2 event buffers...
void qtractorMidiManager::lv2_events_swap (void)
{
	const unsigned short iOutputBuffer = (m_iEventBuffer + 1) & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iOutputBuffer];

	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iOutputBuffer];
#ifdef CONFIG_VST2
	VstMidiEvent *pVst2MidiBuffer = m_ppVst2MidiBuffers[iOutputBuffer];
	VstEvents *pVst2Events = (VstEvents *) m_ppVst2Buffers[iOutputBuffer];
	::memset(pVst2Events, 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iOutputBuffer];
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
		if (pLv2Event == nullptr)
			break;
		if (pLv2Event->type == QTRACTOR_LV2_MIDI_EVENT_ID) {
			long iMidiData = pLv2Event->size;
			if (iMidiData < 1)
				break;
		#ifdef CONFIG_VST2
			VstMidiEvent *pVst2MidiEvent = &pVst2MidiBuffer[iMidiEvents];
			if (iMidiData >= long(sizeof(pVst2MidiEvent->midiData)))
				break;
		#endif
			snd_seq_event_t ev;
			snd_seq_ev_clear(&ev);
		#ifdef CONFIG_MIDI_PARSER
			if (m_pMidiParser) {
				iMidiData = snd_midi_event_encode(m_pMidiParser,
					pMidiData, iMidiData, &ev);
				if (iMidiData < 1 || ev.type == SND_SEQ_EVENT_NONE)
					break;
				ev.time.tick = pLv2Event->frames;
			}
		#endif
		#ifdef CONFIG_VST2
			::memset(pVst2MidiEvent, 0, sizeof(VstMidiEvent));
			pVst2MidiEvent->type = kVstMidiType;
			pVst2MidiEvent->byteSize = sizeof(VstMidiEvent);
			pVst2MidiEvent->deltaFrames = pLv2Event->frames;
			::memcpy(&pVst2MidiEvent->midiData[0], pMidiData, iMidiData);
			pVst2Events->events[iMidiEvents] = (VstEvent *) pVst2MidiEvent;
		#endif
		#ifdef CONFIG_LV2_ATOM
			lv2_atom_buffer_write(&aiter, pLv2Event->frames, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
			pEventBuffer->push(&ev, ev.time.tick);
			++iMidiEvents;
		}
		lv2_event_increment(&eiter);
	}
#ifdef CONFIG_VST2
	pVst2Events->numEvents = iMidiEvents;
#endif

	swapEventBuffers();
}

#endif	// CONFIG_LV2_EVENT


#ifdef CONFIG_LV2_ATOM

// Swap LV2 atom buffers...
void qtractorMidiManager::lv2_atom_buffer_swap (void)
{
	const unsigned short iOutputBuffer = (m_iEventBuffer + 1) & 1;
	qtractorMidiBuffer *pEventBuffer = m_ppEventBuffers[iOutputBuffer];

	LV2_Atom_Buffer *pLv2AtomBuffer = m_ppLv2AtomBuffers[iOutputBuffer];
#ifdef CONFIG_VST2
	VstMidiEvent *pVst2MidiBuffer = m_ppVst2MidiBuffers[iOutputBuffer];
	VstEvents *pVst2Events = (VstEvents *) m_ppVst2Buffers[iOutputBuffer];
	::memset(pVst2Events, 0, sizeof(VstEvents));
#endif
#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer *pLv2EventBuffer = m_ppLv2EventBuffers[iOutputBuffer];
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
		if (pLv2AtomEvent == nullptr)
			break;
		if (pLv2AtomEvent->body.type == QTRACTOR_LV2_MIDI_EVENT_ID) {
			long iMidiData = pLv2AtomEvent->body.size;
			if (iMidiData < 1)
				break;
		#ifdef CONFIG_VST2
			VstMidiEvent *pVst2MidiEvent = &pVst2MidiBuffer[iMidiEvents];
			if (iMidiData >= long(sizeof(pVst2MidiEvent->midiData)))
				break;
		#endif
			snd_seq_event_t ev;
			snd_seq_ev_clear(&ev);
		#ifdef CONFIG_MIDI_PARSER
			if (m_pMidiParser) {
			//	snd_seq_ev_clear(pEv);
				iMidiData = snd_midi_event_encode(m_pMidiParser,
					pMidiData, iMidiData, &ev);
				if (iMidiData < 1 || ev.type == SND_SEQ_EVENT_NONE)
					break;
				ev.time.tick = pLv2AtomEvent->time.frames;
			}
		#endif
		#ifdef CONFIG_VST2
			::memset(pVst2MidiEvent, 0, sizeof(VstMidiEvent));
			pVst2MidiEvent->type = kVstMidiType;
			pVst2MidiEvent->byteSize = sizeof(VstMidiEvent);
			pVst2MidiEvent->deltaFrames = pLv2AtomEvent->time.frames;
			::memcpy(&pVst2MidiEvent->midiData[0], pMidiData, iMidiData);
			pVst2Events->events[iMidiEvents] = (VstEvent *) pVst2MidiEvent;
		#endif
		#ifdef CONFIG_LV2_EVENT
			lv2_event_write(&eiter, pLv2AtomEvent->time.frames, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
			pEventBuffer->push(&ev, ev.time.tick);
			++iMidiEvents;
		}
		lv2_atom_buffer_increment(&aiter);
	}
#ifdef CONFIG_VST2
	pVst2Events->numEvents = iMidiEvents;
#endif
	swapEventBuffers();
}


// Resize LV2 atom buffers if necessary.
void qtractorMidiManager::lv2_atom_buffer_resize ( unsigned int iMinBufferSize )
{
	if (iMinBufferSize < m_iLv2AtomBufferSize)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
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

#endif	// CONFIG_LV2


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
	if (pSession == nullptr)
		return;

	pSession->lock();

	deleteAudioOutputBus();

	m_bAudioOutputBus = bAudioOutputBus;

	createAudioOutputBus();

	if (m_pAudioOutputBus) {
		const unsigned short iChannels
			= m_pAudioOutputBus->channels();
		m_pPluginList->setChannelsEx(iChannels);
		setAudioOutputMonitorEx(
			m_pPluginList->resetChannels(iChannels, true));
	}

	pSession->unlock();
}


void qtractorMidiManager::resetAudioOutputBus (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
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
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	deleteAudioOutputBus();

	// Whether audio output bus is here owned, or...
	if (m_bAudioOutputBus) {
		// Owned, not part of audio engine...
		m_pAudioOutputBus
			= new qtractorAudioBus(pAudioEngine, m_pPluginList->name(),
				qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex),
				false, 2); // FIXME: Make it always stereo (2ch).
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
		if (m_pAudioOutputBus == nullptr) {
			for (qtractorBus *pBus = pAudioEngine->buses().first();
					pBus; pBus = pBus->next()) {
				if (pBus->busMode() & qtractorBus::Output) {
					m_pAudioOutputBus = static_cast<qtractorAudioBus *> (pBus);
					break;
				}
			}
		}
	}

	// Whether audio output bus monitoring is on...
	if (m_bAudioOutputMonitor && m_pAudioOutputBus) {
		// Owned, not part of audio engine...
		if (m_pAudioOutputMonitor) {
			m_pAudioOutputMonitor->setChannels(m_pAudioOutputBus->channels());
		} else {
			m_pAudioOutputMonitor
				= new qtractorAudioOutputMonitor(m_pAudioOutputBus->channels());
		}
	}
}


// Destroy audio Outputnome stuff.
void qtractorMidiManager::deleteAudioOutputBus (void)
{
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
	m_pAudioOutputBus = nullptr;
}


// Output monitor mode accessors.
void qtractorMidiManager::setAudioOutputMonitor ( bool bAudioOutputMonitor )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	pSession->lock();

	if (m_pAudioOutputMonitor && !bAudioOutputMonitor)
		m_pAudioOutputMonitor->setChannels(0);

	m_bAudioOutputMonitor = bAudioOutputMonitor;

	if (m_bAudioOutputMonitor && m_pAudioOutputBus) {
		// Owned, not part of audio engine...
		if (m_pAudioOutputMonitor) {
			m_pAudioOutputMonitor->setChannels(m_pAudioOutputBus->channels());
		} else {
			m_pAudioOutputMonitor
				= new qtractorAudioOutputMonitor(m_pAudioOutputBus->channels());
		}
	}

	pSession->unlock();
}


void qtractorMidiManager::setAudioOutputMonitorEx ( bool bAudioOutputMonitor )
{
	if (( m_bAudioOutputMonitor && !bAudioOutputMonitor) ||
		(!m_bAudioOutputMonitor &&  bAudioOutputMonitor)) {
		// Only do this if really necessary...
		setAudioOutputMonitor(bAudioOutputMonitor);
	}

	// Update all tracks anyway...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		// Meters on tracks list...
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks)
			pTracks->updateMidiTrackItem(this);
		// Meters on mixer strips...
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer)
			pMixer->updateMidiManagerStrip(this);
	}
}


// Instrument map builder.
void qtractorMidiManager::updateInstruments (void)
{
	m_instruments.clearAll();

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		int iIndex = 0;
		int iBanks = 0;
		qtractorPlugin::Program program;
		const QString& sInstrumentName = pPlugin->title();
		qtractorInstrument& instr = m_instruments[sInstrumentName];
		instr.setInstrumentName(sInstrumentName);
		while (pPlugin->getProgram(iIndex++, program)) {
			QString sBankName = instr.bankName(program.bank);
			if (sBankName.isEmpty()) {
				sBankName = QObject::tr("%1 - Bank %2")
					.arg(program.bank)
					.arg(iBanks++);
				instr.setBankName(program.bank, sBankName);
			}
			instr.setProgName(
				program.bank,
				program.prog,
				program.name.simplified());
		}
		iIndex = 0;
		qtractorPlugin::NoteName note;
		while (pPlugin->getNoteName(iIndex++, note)) {
			qtractorInstrumentData& notes
				= instr.notes(note.bank, note.prog);
			notes[note.note] = note.name;
		}
	}
}


// end of qtractorMidiManager.cpp
