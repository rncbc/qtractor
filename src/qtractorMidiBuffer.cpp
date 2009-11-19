// qtractorMidiBuffer.cpp
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

#include "qtractorMidiBuffer.h"

#include "qtractorSession.h"
#include "qtractorPlugin.h"

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
	qtractorMidiManagerThread(qtractorMidiManager *pMidiManager);

	// Destructor.
	~qtractorMidiManagerThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync();

protected:

	// The main thread executive.
	void run();

private:

	// The thread launcher engine.
	qtractorMidiManager *m_pMidiManager;

	// Whether the thread is logically running.
	bool m_bRunState;

	// Thread synchronization objects.
	QMutex         m_mutex;
	QWaitCondition m_cond;
};


//----------------------------------------------------------------------
// class qtractorMidiManagerThread -- MIDI output thread (singleton).
//

// Constructor.
qtractorMidiManagerThread::qtractorMidiManagerThread (
	qtractorMidiManager *pMidiManager ) : QThread(),
		m_pMidiManager(pMidiManager), m_bRunState(false)
{
}


// Destructor.
qtractorMidiManagerThread::~qtractorMidiManagerThread (void)
{
	// Try to wake and terminate executive thread,
	// but give it a bit of time to cleanup...
	if (isRunning()) do {
		setRunState(false);
	//	terminate();
		sync();
	} while (!wait(100));
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

	m_bRunState = true;

	m_mutex.lock();
	while (m_bRunState) {
		// Wait for sync...
		m_cond.wait(&m_mutex);
#ifdef CONFIG_DEBUG_0
		qDebug("qtractorMidiManagerThread[%p]::run(): waked.", this);
#endif
		// Call control process cycle.
		//m_mutex.unlock();
		m_pMidiManager->processSync();
		//m_mutex.lock();
	}
	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiManagerThread[%p]::run(): stopped.", this);
#endif
}


// Wake from executive wait condition.
void qtractorMidiManagerThread::sync (void)
{
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
bool qtractorMidiManager::g_bAudioOutputBus = false;

// Constructor.
qtractorMidiManager::qtractorMidiManager ( qtractorSession *pSession,
	qtractorPluginList *pPluginList, unsigned int iBufferSize ) :
	m_pSession(pSession),
	m_pPluginList(pPluginList),
	m_directBuffer(iBufferSize >> 2),
	m_queuedBuffer(iBufferSize),
	m_postedBuffer(iBufferSize >> 1),
	m_controllerBuffer(iBufferSize >> 2),
	m_pBuffer(NULL), m_iBuffer(0),
#ifdef CONFIG_MIDI_PARSER
	m_pMidiParser(NULL),
#endif
#ifdef CONFIG_VST
	m_pVstMidiBuffer(NULL),
	m_pVstBuffer(NULL),
#endif
#ifdef CONFIG_LV2_EVENT
	m_pLv2Buffer(NULL),
#endif
	m_bAudioOutputBus(g_bAudioOutputBus),
	m_pAudioOutputBus(NULL),
	m_iCurrentBank(-1),
	m_iCurrentProg(-1),
	m_iPendingBankMSB(-1),
	m_iPendingBankLSB(-1),
	m_iPendingProg(-1)
{
	m_pBuffer = new snd_seq_event_t [bufferSize() << 1];

#ifdef CONFIG_MIDI_PARSER
	if (snd_midi_event_new(4, &m_pMidiParser) == 0) {
		snd_midi_event_no_status(m_pMidiParser, 1);
		const unsigned int MaxMidiEvents = (bufferSize() << 1);
	#ifdef CONFIG_VST
		const unsigned int VstBufferSize = sizeof(VstEvents)
			+ MaxMidiEvents * sizeof(VstMidiEvent *);
		m_pVstBuffer = new unsigned char [VstBufferSize];
		m_pVstMidiBuffer = new VstMidiEvent [MaxMidiEvents];
	#endif
	#ifdef CONFIG_LV2_EVENT
		const unsigned int Lv2BufferSize = (MaxMidiEvents << 2);
		m_pLv2Buffer = lv2_event_buffer_new(Lv2BufferSize,
			LV2_EVENT_AUDIO_STAMP);
	#endif
	}
#endif

	m_pSyncThread = new qtractorMidiManagerThread(this);
	m_pSyncThread->start();
	
	createAudioOutputBus();
}

// Destructor.
qtractorMidiManager::~qtractorMidiManager (void)
{
	deleteAudioOutputBus();

	if (m_pSyncThread)
		delete m_pSyncThread;

#ifdef CONFIG_MIDI_PARSER
#ifdef CONFIG_VST
	if (m_pVstMidiBuffer) {
		delete [] m_pVstMidiBuffer;
		m_pVstMidiBuffer = NULL;
	}
	if (m_pVstBuffer) {
		delete [] m_pVstBuffer;
		m_pVstBuffer = NULL;
	}
#endif
#ifdef CONFIG_LV2_EVENT
	if (m_pLv2Buffer) {
		::free(m_pLv2Buffer);
		m_pLv2Buffer = NULL;
	}
#endif
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
bool qtractorMidiManager::queued ( snd_seq_event_t *pEvent )
{
	unsigned long iTick = m_pSession->frameFromTick(pEvent->time.tick);

	if (pEvent->type == SND_SEQ_EVENT_NOTE) {
		snd_seq_event_t ev = *pEvent;
		ev.type = SND_SEQ_EVENT_NOTEON;
		if (!m_queuedBuffer.insert(&ev, iTick))
			return false;
		iTick += m_pSession->frameFromTick(ev.data.note.duration - 1);
		ev.type = SND_SEQ_EVENT_NOTEOFF;
		ev.data.note.velocity = 0;
		ev.data.note.duration = 0;
		return m_postedBuffer.insert(&ev, iTick);
	}

	return m_queuedBuffer.push(pEvent, iTick);
}


// Clears buffers for processing.
void qtractorMidiManager::clear (void)
{
	m_iBuffer = 0;

#ifdef CONFIG_VST
	if (m_pVstBuffer)
		::memset(m_pVstBuffer, 0, sizeof(VstEvents));
#endif

#ifdef CONFIG_LV2_EVENT
	if (m_pLv2Buffer) {
		lv2_event_buffer_reset(m_pLv2Buffer, LV2_EVENT_AUDIO_STAMP,
			(unsigned char *) (m_pLv2Buffer + 1));
	}
#endif
}


// Process buffers (merge).
void qtractorMidiManager::process (
	unsigned long iTimeStart, unsigned long iTimeEnd )
{
	clear();

	// Check for programn change...
	if (m_pSyncThread
		&& (m_iPendingProg >= 0 || !m_controllerBuffer.isEmpty()))
		m_pSyncThread->sync();

	// Merge events in buffer for plugin processing...
	snd_seq_event_t *pEv0 = m_directBuffer.peek();
	snd_seq_event_t *pEv1 = m_postedBuffer.peek();
	snd_seq_event_t *pEv2 = m_queuedBuffer.peek();

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
			pEv1 = m_postedBuffer.next();
		}
		while (pEv2 && pEv2->time.tick < iTimeEnd
			&& ((pEv1 && pEv1->time.tick > pEv2->time.tick) || !pEv1)) {
			m_pBuffer[m_iBuffer] = *pEv2;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv2->time.tick > iTimeStart
					? pEv2->time.tick - iTimeStart : 0);
			pEv2 = m_queuedBuffer.next();
		}
	}

#ifdef CONFIG_DEBUG_0
	for (unsigned int i = 0; i < m_iBuffer; ++i) {
		snd_seq_event_t *pEv = &m_pBuffer[i];
		// - show event for debug purposes...
		unsigned long iTime = iTimeStart + pEv->time.tick;
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
		VstEvents *pVstEvents = (VstEvents *) m_pVstBuffer;
	#endif
	#ifdef CONFIG_LV2_EVENT
		LV2_Event_Iterator iter;
		lv2_event_begin(&iter, m_pLv2Buffer);
	#endif
		const unsigned int MaxMidiEvents = (bufferSize() << 1);
		unsigned int iMidiEvents = 0;
		unsigned char *pMidiData;
		long iMidiData;
		for (unsigned int i = 0; i < m_iBuffer; ++i) {
			snd_seq_event_t *pEv = &m_pBuffer[i];
		#ifdef CONFIG_VST
			VstMidiEvent *pVstMidiEvent = &m_pVstMidiBuffer[iMidiEvents];
			::memset(pVstMidiEvent, 0, sizeof(VstMidiEvent));
			pVstMidiEvent->type = kVstMidiType;
			pVstMidiEvent->byteSize = sizeof(VstMidiEvent);
			pVstMidiEvent->deltaFrames = pEv->time.tick;
			pMidiData = (unsigned char *) &pVstMidiEvent->midiData[0];
			iMidiData = 4;
		#endif
		#ifdef CONFIG_LV2_EVENT
			unsigned char data[4];
			pMidiData = &data[0];
			iMidiData = sizeof(data);
		#endif
			iMidiData = snd_midi_event_decode(m_pMidiParser,
				pMidiData, iMidiData, pEv);
			if (iMidiData < 0)
				break;
		#ifdef CONFIG_DEBUG_0
			// - show event for debug purposes...
			unsigned long iTime = pEv->time.tick;
			fprintf(stderr, "MIDI Raw %06lu {", iTime);
			for (unsigned int i = 0; i < iMidiData; ++i)
				fprintf(stderr, " %02x", pMidiData[i]);
			fprintf(stderr, " }\n");
		#endif
		#ifdef CONFIG_VST
			pVstEvents->events[iMidiEvents] = (VstEvent *) pVstMidiEvent;
		#endif
		#ifdef CONFIG_LV2_EVENT
			lv2_event_write(&iter, pEv->time.tick, 0,
				QTRACTOR_LV2_MIDI_EVENT_ID, iMidiData, pMidiData);
		#endif
			if (++iMidiEvents >= MaxMidiEvents)
				break;
		}
	#ifdef CONFIG_VST
		pVstEvents->numEvents = iMidiEvents;
	//	pVstEvents->reserved = 0;
	#endif
	}
#endif

	// Now's time to process the plugins as usual...
	if (m_pAudioOutputBus) {
		unsigned int nframes = iTimeEnd - iTimeStart;
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
}


// Resets all buffering.
void qtractorMidiManager::reset (void)
{
	m_pSession->lock();

	m_directBuffer.clear();
	m_queuedBuffer.clear();
	m_postedBuffer.reset(); // formerly .clear();

	clear();

#ifdef CONFIG_MIDI_PARSER
	if (m_pMidiParser)
		snd_midi_event_reset_decode(m_pMidiParser);
#endif

	m_pPluginList->resetBuffer();

	m_iPendingBankMSB = -1;
	m_iPendingBankLSB = -1;
	m_iPendingProg    = -1;

	m_pSession->unlock();
}


// Factory (proxy) methods.
qtractorMidiManager *qtractorMidiManager::createMidiManager (
	qtractorPluginList *pPluginList )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	qtractorMidiManager *pMidiManager
		= new qtractorMidiManager(pSession, pPluginList);
	pSession->addMidiManager(pMidiManager);

#ifdef CONFIG_DEBUG
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

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiManager::deleteMidiManager(%p)", pMidiManager);
#endif

	pSession->removeMidiManager(pMidiManager);
	delete pMidiManager;
}


// Some default factory options.
void qtractorMidiManager::setDefaultAudioOutputBus ( bool bAudioOutputBus )
{
	g_bAudioOutputBus = bAudioOutputBus;
}

bool qtractorMidiManager::isDefaultAudioOutputBus (void)
{
	return g_bAudioOutputBus;
}


// Output bus mode accessors.
void qtractorMidiManager::setAudioOutputBus ( bool bAudioOutputBus )
{
	m_pSession->lock();
	deleteAudioOutputBus();

	m_bAudioOutputBus = bAudioOutputBus;

	createAudioOutputBus();
	m_pSession->unlock();
}

void qtractorMidiManager::resetAudioOutputBus (void)
{
	m_pSession->lock();
	createAudioOutputBus();
	m_pSession->unlock();
}


// Create audio output stuff...
void qtractorMidiManager::createAudioOutputBus (void)
{
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	deleteAudioOutputBus();

	// Whether audio output bus is here owned, or...
	if (m_bAudioOutputBus) {
		// Owned, not part of audio engine...
		m_pAudioOutputBus = new qtractorAudioBus(pAudioEngine,
			m_pPluginList->name(),
			qtractorBus::Output, false,
			m_pPluginList->channels());
		if (pAudioEngine->isActivated()) {
			if (m_pAudioOutputBus->open())
				m_pAudioOutputBus->autoConnect();
		}
	}
	else {
		// Output bus gets to be the first available output bus...
		for (qtractorBus *pBus = (pAudioEngine->buses()).first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				m_pAudioOutputBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}
	}
}


// Destroy audio Outputnome stuff.
void qtractorMidiManager::deleteAudioOutputBus (void)
{
	// Owned, not part of audio engine...
	if (m_bAudioOutputBus && m_pAudioOutputBus)
		delete m_pAudioOutputBus;

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
