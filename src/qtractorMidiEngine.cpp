// qtractorMidiEngine.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"
#include "qtractorMidiEvent.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorDocument.h"

#include "qtractorAudioEngine.h"

#include "qtractorMidiClip.h"
#include "qtractorMidiManager.h"
#include "qtractorMidiControl.h"
#include "qtractorMidiTimer.h"
#include "qtractorMidiSysex.h"
#include "qtractorMidiRpn.h"

#include "qtractorPlugin.h"

#include "qtractorInsertPlugin.h"

#include "qtractorMidiEditCommand.h"

#include <QApplication>
#include <QFileInfo>

#include <QDomDocument>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <QSocketNotifier>

#include <QElapsedTimer>

#include <cmath>


// Specific controller definitions
#define BANK_SELECT_MSB		0x00
#define BANK_SELECT_LSB		0x20

#define ALL_SOUND_OFF		0x78
#define ALL_CONTROLLERS_OFF	0x79
#define ALL_NOTES_OFF		0x7b

#define CHANNEL_VOLUME		0x07
#define CHANNEL_PANNING		0x0a


// Audio vs. MIDI time drift cycle
#define DRIFT_CHECK         8

#define DRIFT_CHECK_MIN     (DRIFT_CHECK >> 2)
#define DRIFT_CHECK_MAX     (DRIFT_CHECK << 1)


//----------------------------------------------------------------------
// class qtractorMidiInputRpn -- MIDI RPN/NRPN input parser (singleton).
//
class qtractorMidiInputRpn : public qtractorMidiRpn
{
public:

	// Constructor.
	qtractorMidiInputRpn();

	// Encoder.
	bool process (const snd_seq_event_t *ev);

	// Decoder.
	bool dequeue (snd_seq_event_t *ev);
};


//----------------------------------------------------------------------
// class qtractorMidiInputThread -- MIDI input thread (singleton).
//

class qtractorMidiInputThread : public QThread
{
public:

	// Constructor.
	qtractorMidiInputThread(qtractorMidiEngine *pMidiEngine);

	// Destructor.
	~qtractorMidiInputThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

protected:

	// The main thread executive.
	void run();

private:

	// The thread launcher engine.
	qtractorMidiEngine *m_pMidiEngine;

	// Whether the thread is logically running.
	bool m_bRunState;
};


//----------------------------------------------------------------------
// class qtractorMidiOutputThread -- MIDI output thread (singleton).
//

class qtractorMidiOutputThread : public QThread
{
public:

	// Constructor.
	qtractorMidiOutputThread(qtractorMidiEngine *pMidiEngine);

	// Destructor.
	~qtractorMidiOutputThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// MIDI track output process resync.
	void trackSync(qtractorTrack *pTrack, unsigned long iFrameStart);

	// MIDI metronome output process resync.
	void metroSync(unsigned long iFrameStart);

	// MIDI output process cycle iteration (locked).
	void processSync();

	// MIDI output flush/drain (locked).
	void flushSync();

	// MIDI queue reset (locked).
	void resetSync();

	// Wake from executive wait condition.
	void sync();

protected:

	// The main thread executive.
	void run();

private:

	// The thread launcher engine.
	qtractorMidiEngine *m_pMidiEngine;

	// Whether the thread is logically running.
	bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;
};


//----------------------------------------------------------------------
// class qtractorMidiPlayerThread -- MIDI player thread.
//

class qtractorMidiPlayer;

class qtractorMidiPlayerThread : public QThread
{
public:

	// Constructor.
	qtractorMidiPlayerThread(qtractorMidiPlayer *pMidiPlayer);

	// Destructor.
	~qtractorMidiPlayerThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

protected:

	// The main thread executive.
	void run();

private:

	// Instance variables.
	qtractorMidiPlayer *m_pMidiPlayer;

	bool m_bRunState;
};


//----------------------------------------------------------------------
// class qtractorMidiPlayer -- Simple MIDI player.
//

class qtractorMidiPlayer
{
public:

	// Constructor.
	qtractorMidiPlayer(qtractorMidiBus *pMidiBus);

	// Destructor.
	~qtractorMidiPlayer();

	// Open and start playing.
	bool open(const QString& sFilename, int iTrackChannel = -1);

	// Close and stop playing.
	void close();

	// Process playing executive.
	bool process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Open status predicate.
	bool isOpen() const;

	// Sample-rate accessor.
	unsigned int sampleRate() const;

	// Queue time (ticks) accessor.
	unsigned long queueTime() const;

	// Queue time (frames) accessor.
	unsigned long queueFrame() const;

protected:

	// Enqueue process event.
	void enqueue(unsigned short iMidiChannel,
		qtractorMidiEvent *pEvent, unsigned long iTime);

private:

	// Instance variables.
	qtractorMidiBus           *m_pMidiBus;
	qtractorMidiEngine        *m_pMidiEngine;

	int                        m_iPlayerQueue;

	unsigned short             m_iSeqs;
	qtractorMidiSequence     **m_ppSeqs;
	qtractorMidiCursor       **m_ppSeqCursors;
	qtractorTimeScale         *m_pTimeScale;

	qtractorTimeScale::Cursor *m_pCursor;
	float                      m_fTempo;

	qtractorMidiPlayerThread  *m_pPlayerThread;
};


//----------------------------------------------------------------------
// class qtractorMidiInputRpn -- MIDI RPN/NRPN input parser.
//

// Constructor.
qtractorMidiInputRpn::qtractorMidiInputRpn (void) : qtractorMidiRpn()
{
}


// Encoder.
bool qtractorMidiInputRpn::process ( const snd_seq_event_t *ev )
{
	if (ev->type != SND_SEQ_EVENT_CONTROLLER) {
		qtractorMidiRpn::flush();
		return false;
	}

	qtractorMidiRpn::Event event;

	event.time   = ev->time.tick;
	event.port   = ev->dest.port;
	event.status = qtractorMidiRpn::CC | (ev->data.control.channel & 0x0f);
	event.param  = ev->data.control.param;
	event.value  = ev->data.control.value;

	return qtractorMidiRpn::process(event);
}


// Decoder.
bool qtractorMidiInputRpn::dequeue ( snd_seq_event_t *ev )
{
	qtractorMidiRpn::Event event;

	if (!qtractorMidiRpn::dequeue(event))
		return false;

	snd_seq_ev_clear(ev);
	snd_seq_ev_schedule_tick(ev, 0, 0, event.time);
	snd_seq_ev_set_dest(ev, 0, event.port);
	snd_seq_ev_set_fixed(ev);

	switch (qtractorMidiRpn::Type(event.status & 0x70)) {
	case qtractorMidiRpn::CC:	// 0x10
		ev->type = SND_SEQ_EVENT_CONTROLLER;
		break;
	case qtractorMidiRpn::RPN:	// 0x20
		ev->type = SND_SEQ_EVENT_REGPARAM;
		break;
	case qtractorMidiRpn::NRPN:	// 0x30
		ev->type = SND_SEQ_EVENT_NONREGPARAM;
		break;
	case qtractorMidiRpn::CC14:	// 0x40
		ev->type = SND_SEQ_EVENT_CONTROL14;
		break;
	default:
		return false;
	}

	ev->data.control.channel = event.status & 0x0f;
	ev->data.control.param   = event.param;
	ev->data.control.value   = event.value;

	return true;
}


//----------------------------------------------------------------------
// class qtractorMidiInputThread -- MIDI input thread (singleton).
//

// Constructor.
qtractorMidiInputThread::qtractorMidiInputThread (
	qtractorMidiEngine *pMidiEngine ) : QThread()
{
	m_pMidiEngine = pMidiEngine;
	m_bRunState   = false;
}


// Destructor.
qtractorMidiInputThread::~qtractorMidiInputThread (void)
{
	// Try to terminate executive thread,
	// but give it a bit of time to cleanup...
	if (isRunning()) do {
		setRunState(false);
	//	terminate();
	} while (!wait(100));
}


// Thread run state accessors.
void qtractorMidiInputThread::setRunState ( bool bRunState )
{
	m_bRunState = bRunState;
}

bool qtractorMidiInputThread::runState (void) const
{
	return m_bRunState;
}


// The main thread executive.
void qtractorMidiInputThread::run (void)
{
	snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiInputThread[%p]::run(%p): started...", this);
#endif

	int nfds;
	struct pollfd *pfds;

	nfds = snd_seq_poll_descriptors_count(pAlsaSeq, POLLIN);
	pfds = (struct pollfd *) alloca(nfds * sizeof(struct pollfd));
	snd_seq_poll_descriptors(pAlsaSeq, pfds, nfds, POLLIN);

	qtractorMidiInputRpn xrpn;

	m_bRunState = true;

	int iPoll = 0;
	while (m_bRunState && iPoll >= 0) {
		// Wait for events...
		iPoll = poll(pfds, nfds, 200);
		// Timeout?
		if (iPoll == 0)
			xrpn.flush();
		while (iPoll > 0) {
			snd_seq_event_t *pEv = nullptr;
			snd_seq_event_input(pAlsaSeq, &pEv);
			// Process input event - ...
			// - enqueue to input track mapping;
			if (!xrpn.process(pEv))
				m_pMidiEngine->capture(pEv);
		//	snd_seq_free_event(pEv);
			iPoll = snd_seq_event_input_pending(pAlsaSeq, 0);
		}
		// Process pending events...
		while (xrpn.isPending()) {
			snd_seq_event_t ev;
			if (xrpn.dequeue(&ev))
				m_pMidiEngine->capture(&ev);
		}
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiInputThread[%p]::run(): stopped.", this);
#endif
}


//----------------------------------------------------------------------
// class qtractorMidiOutputThread -- MIDI output thread (singleton).
//

// Constructor.
qtractorMidiOutputThread::qtractorMidiOutputThread (
	qtractorMidiEngine *pMidiEngine ) : QThread()
{
	m_pMidiEngine = pMidiEngine;
	m_bRunState   = false;
}


// Destructor.
qtractorMidiOutputThread::~qtractorMidiOutputThread (void)
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
void qtractorMidiOutputThread::setRunState ( bool bRunState )
{
	QMutexLocker locker(&m_mutex);

	m_bRunState = bRunState;
}

bool qtractorMidiOutputThread::runState (void) const
{
	return m_bRunState;
}


// The main thread executive.
void qtractorMidiOutputThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::run(): started...", this);
#endif

	m_bRunState = true;

	m_mutex.lock();

	while (m_bRunState) {
		// Wait for sync...
		m_cond.wait(&m_mutex);
#ifdef CONFIG_DEBUG_0
		qDebug("qtractorMidiOutputThread[%p]::run(): waked.", this);
#endif
		// Only if playing, the output process cycle.
		if (m_pMidiEngine->isPlaying())
			m_pMidiEngine->process();
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::run(): stopped.", this);
#endif
}


// MIDI output process cycle iteration (locked).
void qtractorMidiOutputThread::processSync (void)
{
	QMutexLocker locker(&m_mutex);
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::processSync()", this);
#endif
	m_pMidiEngine->process();
}


// MIDI output flush/drain (locked).
void qtractorMidiOutputThread::flushSync (void)
{
	QMutexLocker locker(&m_mutex);
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::flushSync()", this);
#endif
	snd_seq_drain_output(m_pMidiEngine->alsaSeq());
}


// MIDI queue reset time (locked).
void qtractorMidiOutputThread::resetSync (void)
{
	QMutexLocker locker(&m_mutex);
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::resetSync()", this);
#endif
	m_pMidiEngine->resetSync();
}


// MIDI track output process resync.
void qtractorMidiOutputThread::trackSync (
	qtractorTrack *pTrack, unsigned long iFrameStart )
{
	QMutexLocker locker(&m_mutex);

	// Must have a valid session...
	qtractorSession *pSession = m_pMidiEngine->session();
	if (pSession == nullptr)
		return;
	
	// Pick our actual MIDI sequencer cursor...
	qtractorSessionCursor *pMidiCursor = m_pMidiEngine->sessionCursor();
	if (pMidiCursor == nullptr)
		return;

	// This is the last framestamp to be trown out...
	const unsigned long iFrameEnd = pMidiCursor->frame();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::trackSync(%p, %lu, %lu)",
		this, pTrack, iFrameStart, iFrameEnd);
#endif

	// Split processing, in case we've been caught looping...
	if (pSession->isLooping()
		&& iFrameStart > iFrameEnd
		&& iFrameStart < pSession->loopEnd())
		iFrameStart = pSession->loopStart();

	// Locate the immediate nearest clip in track
	// and render them all thereafter, immediately...
	qtractorClip *pClip = pTrack->clips().first();
	while (pClip && pClip->clipStart() < iFrameEnd) {
		if (iFrameStart < pClip->clipStart() + pClip->clipLength())
			pClip->process(iFrameStart, iFrameEnd);
		pClip = pClip->next();
	}

	// Surely must realize the output queue...
	snd_seq_drain_output(m_pMidiEngine->alsaSeq());
}


// MIDI metronome output process resync.
void qtractorMidiOutputThread::metroSync ( unsigned long iFrameStart )
{
	QMutexLocker locker(&m_mutex);

	// Pick our actual MIDI sequencer cursor...
	qtractorSessionCursor *pMidiCursor = m_pMidiEngine->sessionCursor();
	if (pMidiCursor == nullptr)
		return;

	// This is the last framestamp to be trown out...
	const unsigned long iFrameEnd = pMidiCursor->frame();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::metroSync(%lu, %lu)",
		this, iFrameStart, iFrameEnd);
#endif

	// (Re)process the metronome stuff...
	m_pMidiEngine->processMetro(iFrameStart, iFrameEnd);

	// Surely must realize the output queue...
	snd_seq_drain_output(m_pMidiEngine->alsaSeq());
}


// Wake from executive wait condition.
void qtractorMidiOutputThread::sync (void)
{
	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else qDebug("qtractorMidiOutputThread[%p]::sync(): tryLock() failed.", this);
#endif
}


//----------------------------------------------------------------------
// class qtractorMidiPlayerThread -- MIDI player thread.
//

// Constructor.
qtractorMidiPlayerThread::qtractorMidiPlayerThread (
	qtractorMidiPlayer *pMidiPlayer ) : QThread(),
		m_pMidiPlayer(pMidiPlayer), m_bRunState(false)
{
}


// Destructor.
qtractorMidiPlayerThread::~qtractorMidiPlayerThread (void)
{
	if (isRunning()) do {
		setRunState(false);
	//	terminate();
	} while (!wait(100));
}


// Thread run state accessors.
void qtractorMidiPlayerThread::setRunState ( bool bRunState )
{
	m_bRunState = bRunState;
}

bool qtractorMidiPlayerThread::runState (void) const
{
	return m_bRunState;
}


// The main thread executive.
void qtractorMidiPlayerThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiPlayerThread[%p]::run(): started...", this);
#endif

	const unsigned int iSampleRate = m_pMidiPlayer->sampleRate();
	const unsigned long iReadAhead = (iSampleRate >> 2);

	unsigned long iFrameStart = 0;
	unsigned long iFrameEnd = 0;

	m_bRunState = true;

	while (m_bRunState) {
		iFrameEnd += iReadAhead;
		if (m_pMidiPlayer->process(iFrameStart, iFrameEnd)) {
			const unsigned long iQueueFrame = m_pMidiPlayer->queueFrame();
			const long iDelta = long(iFrameStart) - long(iQueueFrame);
		#ifdef CONFIG_DEBUG
			qDebug("qtractorMidiPlayer::process(%lu, %lu) iQueueFrame=%lu (%ld)",
				iFrameStart, iFrameEnd, iQueueFrame, iDelta);
		#endif
			unsigned long iSleep = iReadAhead;
			if (iSleep +  iDelta > 0)
				iSleep += iDelta;
			msleep((1000 * iSleep) / iSampleRate);
			iFrameStart = iFrameEnd;
		}
		else m_bRunState = false;
	}

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiPlayerThread[%p]::run(): stopped.", this);
#endif
}


//----------------------------------------------------------------------
// class qtractorMidiPlayer -- Simple MIDI player.
//

// Constructor.
qtractorMidiPlayer::qtractorMidiPlayer ( qtractorMidiBus *pMidiBus )
	: m_pMidiBus(pMidiBus),
		m_pMidiEngine(static_cast<qtractorMidiEngine *> (pMidiBus->engine())),
		m_iPlayerQueue(-1), m_iSeqs(0), m_ppSeqs(nullptr), m_ppSeqCursors(nullptr),
		m_pTimeScale(nullptr), m_pCursor(nullptr), m_fTempo(0.0f),
		m_pPlayerThread(nullptr)
{
}


// Destructor.
qtractorMidiPlayer::~qtractorMidiPlayer (void)
{
	close();
}


// Open and start playing.
bool qtractorMidiPlayer::open ( const QString& sFilename, int iTrackChannel )
{
	close();

	if (m_pMidiBus == nullptr)
		return false;

	qtractorSession *pSession = m_pMidiEngine->session();
	if (pSession == nullptr)
		return false;

	qtractorSessionCursor *pAudioCursor
		= pSession->audioEngine()->sessionCursor();
	if (pAudioCursor == nullptr)
		return false;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == nullptr)
		return false;

	snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return false;

	m_iPlayerQueue = snd_seq_alloc_queue(pAlsaSeq);
	if (m_iPlayerQueue < 0)
		return false;

	qtractorMidiFile file;
	if (!file.open(sFilename)) {
		snd_seq_free_queue(pAlsaSeq, m_iPlayerQueue);
		m_iPlayerQueue = -1;
		return false;
	}

	m_pTimeScale = new qtractorTimeScale(*pTimeScale);
	m_pTimeScale->setTicksPerBeat(file.ticksPerBeat());

	if (iTrackChannel < 0) {
		m_iSeqs = (file.format() == 1 ? file.tracks() : 16);
		iTrackChannel = 0;
	}
	else m_iSeqs = 1;

	m_ppSeqs = new qtractorMidiSequence * [m_iSeqs];
	m_ppSeqCursors = new qtractorMidiCursor * [m_iSeqs];
	for (unsigned short iSeq = 0; iSeq < m_iSeqs; ++iSeq) {
		m_ppSeqs[iSeq] = new qtractorMidiSequence(
			QString(), iSeq, m_pTimeScale->ticksPerBeat());
		m_ppSeqCursors[iSeq] = new qtractorMidiCursor();
	}

	if (file.readTracks(m_ppSeqs, m_iSeqs, iTrackChannel) && file.tempoMap())
		file.tempoMap()->intoTimeScale(m_pTimeScale);

	file.close();

	m_pCursor = new qtractorTimeScale::Cursor(m_pTimeScale);
	m_fTempo = m_pTimeScale->tempo();

	snd_seq_queue_tempo_t *pQueueTempo;
	snd_seq_queue_tempo_alloca(&pQueueTempo);
	snd_seq_get_queue_tempo(pAlsaSeq, m_iPlayerQueue, pQueueTempo);
	snd_seq_queue_tempo_set_ppq(pQueueTempo,
		qtractorTimeScale::TICKS_PER_BEAT_HRQ);
	snd_seq_queue_tempo_set_tempo(pQueueTempo,
		(unsigned int) (60000000.0f / m_fTempo));
	snd_seq_set_queue_tempo(pAlsaSeq, m_iPlayerQueue, pQueueTempo);

	snd_seq_start_queue(pAlsaSeq, m_iPlayerQueue, nullptr);
	snd_seq_drain_output(pAlsaSeq);

	pAudioCursor->reset();

	qtractorMidiMonitor::resetTime(m_pTimeScale, 0);

	if (m_pMidiBus->midiMonitor_out())
		m_pMidiBus->midiMonitor_out()->reset();

	m_pPlayerThread = new qtractorMidiPlayerThread(this);
	m_pPlayerThread->start(QThread::HighPriority);

	return true;
}


// Close and stop playing.
void qtractorMidiPlayer::close (void)
{
	if (m_pPlayerThread) {
		if (m_pPlayerThread->isRunning()) do {
			m_pPlayerThread->setRunState(false);
		//	m_pPayerThread->terminate();
		} while (!m_pPlayerThread->wait(100));
		delete m_pPlayerThread;
		m_pPlayerThread = nullptr;
	}

	if (m_ppSeqs && m_pMidiBus) {
		snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
		if (pAlsaSeq) {
			m_pMidiBus->dequeueNoteOffs(queueTime());
			snd_seq_drop_output(pAlsaSeq);
			if (m_iPlayerQueue >= 0) {
				snd_seq_stop_queue(pAlsaSeq, m_iPlayerQueue, nullptr);
				snd_seq_free_queue(pAlsaSeq, m_iPlayerQueue);
				m_iPlayerQueue = -1;
			}
			for (unsigned short iSeq = 0; iSeq < m_iSeqs; ++iSeq) {
				const unsigned short iChannel = m_ppSeqs[iSeq]->channel();
				m_pMidiBus->setController(iChannel, ALL_SOUND_OFF);
				m_pMidiBus->setController(iChannel, ALL_NOTES_OFF);
				m_pMidiBus->setController(iChannel, ALL_CONTROLLERS_OFF);
			}
			snd_seq_drain_output(pAlsaSeq);
		}
		if (m_pMidiBus->pluginList_out()
			&& (m_pMidiBus->pluginList_out())->midiManager())
			(m_pMidiBus->pluginList_out())->midiManager()->reset();
	}
	
	if (m_pCursor) {
		delete m_pCursor;
		m_pCursor = nullptr;
	}

	m_fTempo = 0.0f;

	for (unsigned short iSeq = 0; iSeq < m_iSeqs; ++iSeq) {
		if (m_ppSeqCursors && m_ppSeqCursors[iSeq])
			delete m_ppSeqCursors[iSeq];
		if (m_ppSeqs && m_ppSeqs[iSeq])
			delete m_ppSeqs[iSeq];
	}

	if (m_ppSeqCursors) {
		delete [] m_ppSeqCursors;
		m_ppSeqCursors = nullptr;
	}

	if (m_ppSeqs) {
		delete [] m_ppSeqs;
		m_ppSeqs = nullptr;
	}

	m_iSeqs = 0;

	if (m_pTimeScale) {
		delete m_pTimeScale;
		m_pTimeScale = nullptr;
	}
}


// Process playing executive.
bool qtractorMidiPlayer::process (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (m_pMidiBus == nullptr)
		return false;

	if (m_pCursor == nullptr)
		return false;

	snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return false;

	if (m_iPlayerQueue < 0)
		return false;

	qtractorTimeScale::Node *pNode
		= m_pCursor->seekFrame(iFrameEnd);
	if (pNode->tempo != m_fTempo) {
		const unsigned long iTime = (pNode->frame < iFrameStart
			? pNode->tickFromFrame(iFrameStart) : pNode->tick);
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_schedule_tick(&ev, m_iPlayerQueue, 0, m_pTimeScale->timep(iTime));
		ev.type = SND_SEQ_EVENT_TEMPO;
		ev.data.queue.queue = m_iPlayerQueue;
		ev.data.queue.param.value
			= (unsigned int) (60000000.0f / pNode->tempo);
		ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
		ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
		snd_seq_event_output(pAlsaSeq, &ev);
		m_fTempo = pNode->tempo;
		qtractorMidiMonitor::splitTime(
			m_pCursor->timeScale(), pNode->frame, iTime);
	}

	const unsigned long iTimeStart
		= m_pTimeScale->tickFromFrame(iFrameStart);
	const unsigned long iTimeEnd
		= m_pTimeScale->tickFromFrame(iFrameEnd);

	unsigned int iProcess = 0;
	for (unsigned short iSeq = 0; iSeq < m_iSeqs; ++iSeq) {
		qtractorMidiSequence *pSeq = m_ppSeqs[iSeq];
		qtractorMidiCursor *pSeqCursor = m_ppSeqCursors[iSeq];
		qtractorMidiEvent *pEvent = pSeqCursor->seek(pSeq, iTimeStart);
		while (pEvent) {
			const unsigned long iTime = pEvent->time();
			if (iTime >= iTimeEnd)
				break;
			if (iTime >= iTimeStart)
				enqueue(pSeq->channel(), pEvent, iTime);
			pEvent = pEvent->next();
		}
		if (iTimeEnd < pSeq->duration()) ++iProcess;
	}

	snd_seq_drain_output(pAlsaSeq);

	return (iProcess > 0);
}


// Enqueue process event.
void qtractorMidiPlayer::enqueue ( unsigned short iMidiChannel,
	qtractorMidiEvent *pEvent, unsigned long iTime )
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	snd_seq_ev_set_source(&ev, m_pMidiBus->alsaPort());
	snd_seq_ev_set_subs(&ev);

	snd_seq_ev_schedule_tick(&ev, m_iPlayerQueue, 0, m_pTimeScale->timep(iTime));

	unsigned long iDuration = 0;

	switch (pEvent->type()) {
	case qtractorMidiEvent::NOTEON:
		ev.type = SND_SEQ_EVENT_NOTE;
		ev.data.note.channel  = iMidiChannel;
		ev.data.note.note     = pEvent->note();
		ev.data.note.velocity = pEvent->value();
		iDuration = pEvent->duration();
		ev.data.note.duration = m_pTimeScale->timep(iDuration);
		break;
	case qtractorMidiEvent::KEYPRESS:
		ev.type = SND_SEQ_EVENT_KEYPRESS;
		ev.data.note.channel  = iMidiChannel;
		ev.data.note.note     = pEvent->note();
		ev.data.note.velocity = pEvent->velocity();
		ev.data.note.duration = 0;
		break;
	case qtractorMidiEvent::CONTROLLER:
		ev.type = SND_SEQ_EVENT_CONTROLLER;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.param   = pEvent->controller();
		ev.data.control.value   = pEvent->value();
		break;
	case qtractorMidiEvent::REGPARAM:
		ev.type = SND_SEQ_EVENT_REGPARAM;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.param   = pEvent->param();
		ev.data.control.value   = pEvent->value();
		break;
	case qtractorMidiEvent::NONREGPARAM:
		ev.type = SND_SEQ_EVENT_NONREGPARAM;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.param   = pEvent->param();
		ev.data.control.value   = pEvent->value();
		break;
	case qtractorMidiEvent::CONTROL14:
		ev.type = SND_SEQ_EVENT_CONTROL14;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.param   = pEvent->param();
		ev.data.control.value   = pEvent->value();
		break;
	case qtractorMidiEvent::PGMCHANGE:
		ev.type = SND_SEQ_EVENT_PGMCHANGE;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.value   = pEvent->param();
		break;
	case qtractorMidiEvent::CHANPRESS:
		ev.type = SND_SEQ_EVENT_CHANPRESS;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.value   = pEvent->value();
		break;
	case qtractorMidiEvent::PITCHBEND:
		ev.type = SND_SEQ_EVENT_PITCHBEND;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.value   = pEvent->pitchBend();
		break;
	case qtractorMidiEvent::SYSEX:
		ev.type = SND_SEQ_EVENT_SYSEX;
		snd_seq_ev_set_sysex(&ev, pEvent->sysex_len(), pEvent->sysex());
		break;
	default:
		break;
	}

	snd_seq_event_output(m_pMidiEngine->alsaSeq(), &ev);

	if (ev.type == SND_SEQ_EVENT_NOTE && iDuration > 0)
		m_pMidiBus->enqueueNoteOff(&ev, iTime, iTime + (iDuration - 1));

	if (m_pMidiBus->midiMonitor_out())
		m_pMidiBus->midiMonitor_out()->enqueue(
			pEvent->type(), pEvent->value(), iTime);

	if (m_pMidiBus->pluginList_out()) {
		qtractorMidiManager *pMidiManager
			= (m_pMidiBus->pluginList_out())->midiManager();
		if (pMidiManager) {
			qtractorTimeScale::Cursor& cursor = m_pTimeScale->cursor();
			qtractorTimeScale::Node *pNode = cursor.seekTick(iTime);
			const unsigned long t1 = pNode->frameFromTick(iTime);
			unsigned long t2 = t1;
			if (ev.type == SND_SEQ_EVENT_NOTE && iDuration > 0) {
				iTime += (iDuration - 1);
				pNode = cursor.seekTick(iTime);
				t2 += (pNode->frameFromTick(iTime) - t1);
			}
			pMidiManager->queued(&ev, t1, t2);
		}
	}
}


// Open status predicate.
bool qtractorMidiPlayer::isOpen (void) const
{
	return (m_pPlayerThread && m_pPlayerThread->isRunning());
}


// Sample-rate accessor.
unsigned int qtractorMidiPlayer::sampleRate (void) const
{
	return (m_pTimeScale ? m_pTimeScale->sampleRate() : 44100);
}


// Queue time (ticks) accessor.
unsigned long qtractorMidiPlayer::queueTime (void) const
{
	snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return 0;

	if (m_iPlayerQueue < 0)
		return 0;

	unsigned long iQueueTime = 0;
	
	snd_seq_queue_status_t *pQueueStatus;
	snd_seq_queue_status_alloca(&pQueueStatus);
	if (snd_seq_get_queue_status(
			pAlsaSeq, m_iPlayerQueue, pQueueStatus) >= 0) {
		iQueueTime = snd_seq_queue_status_get_tick_time(pQueueStatus);
	}

	return m_pTimeScale->timeq(iQueueTime);
}


// Queue time (frames) accessor.
unsigned long qtractorMidiPlayer::queueFrame (void) const
{
	return (m_pTimeScale ? m_pTimeScale->frameFromTick(queueTime()) : 0);
}


//----------------------------------------------------------------------
// class qtractorMidiEngine -- ALSA sequencer client instance (singleton).
//

// Constructor.
qtractorMidiEngine::qtractorMidiEngine ( qtractorSession *pSession )
	: qtractorEngine(pSession, qtractorTrack::Midi)
{
	m_pAlsaSeq      = nullptr;
	m_iAlsaClient   = -1;
	m_iAlsaQueue    = -1;
	m_iAlsaTimer    = 0;

	m_pAlsaSubsSeq  = nullptr;
	m_iAlsaSubsPort = -1;
	m_pAlsaNotifier = nullptr;

	m_iReadAhead    = 0;

	m_pInputThread  = nullptr;
	m_pOutputThread = nullptr;

	m_bDriftCorrect = true;

	m_iDriftCheck   = 0;
	m_iDriftCount   = DRIFT_CHECK;

	m_iTimeDrift    = 0;
	m_iFrameDrift   = 0;

	m_iTimeStart    = 0;
	m_iFrameStart   = 0;

	m_iTimeStartEx  = 0;

	m_iAudioFrameStart = 0;

	m_bControlBus   = false;
	m_pIControlBus  = nullptr;
	m_pOControlBus  = nullptr;

	// MIDI Metronome stuff.
	m_bMetronome         = false;
	m_bMetroBus          = false;
	m_pMetroBus          = nullptr;
	m_iMetroChannel      = 9;	// GM Drums channel (10)
	m_iMetroBarNote      = 76;	// GM High-wood stick
	m_iMetroBarVelocity  = 96;
	m_iMetroBarDuration  = 48;
	m_iMetroBeatNote     = 77;	// GM Low-wood stick
	m_iMetroBeatVelocity = 64;
	m_iMetroBeatDuration = 24;
	m_iMetroOffset       = 0;
	m_bMetroEnabled      = false;

	// Time-scale cursor (tempo/time-signature map)
	m_pMetroCursor = nullptr;

	// Track down tempo changes.
	m_fMetroTempo = 0.0f;

	// MIDI Metronome count-in stuff.
	m_bCountIn           = false;
	m_countInMode        = CountInNone;
	m_iCountInBeats      = 0;
	m_iCountIn           = 0;
	m_iCountInFrame      = 0;
	m_iCountInFrameStart = 0;
	m_iCountInFrameEnd   = 0;
	m_iCountInTimeStart  = 0;

	// SMF player stuff.
	m_bPlayerBus = false;
	m_pPlayerBus = nullptr;
	m_pPlayer    = nullptr;

	// No input/capture quantization (default).
	m_iCaptureQuantize = 0;

	// MIDI controller mapping flagger.
	m_iResetAllControllersPending = 0;

	// MIDI MMC/SPP modes.
	m_mmcDevice = 0x7f; // All-caller-id.
	m_mmcMode = qtractorBus::Duplex;
	m_sppMode = qtractorBus::Duplex;

	// MIDI Clock mode.
	m_clockMode = qtractorBus::None;

	// Whether to reset all MIDI controllers (on playback start).
	m_bResetAllControllers = false;

	// MIDI Clock tempo tracking.
	m_iClockCount = 0;
	m_fClockTempo = 120.0f;
}


// Special event notifier proxy object.
qtractorMidiEngineProxy *qtractorMidiEngine::proxy (void)
{
	return &m_proxy;
}


// ALSA sequencer client descriptor accessor.
snd_seq_t *qtractorMidiEngine::alsaSeq (void) const
{
	return m_pAlsaSeq;
}

int qtractorMidiEngine::alsaClient (void) const
{
	return m_iAlsaClient;
}

int qtractorMidiEngine::alsaQueue (void) const
{
	return m_iAlsaQueue;
}


// Current ALSA queue time accessor.
unsigned long qtractorMidiEngine::queueTime (void) const
{
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return 0;

	if (m_pAlsaSeq == nullptr)
		return 0;

	if (m_iAlsaQueue < 0)
		return 0;

	long iQueueTime = 0;

	snd_seq_queue_status_t *pQueueStatus;
	snd_seq_queue_status_alloca(&pQueueStatus);
	if (snd_seq_get_queue_status(
			m_pAlsaSeq, m_iAlsaQueue, pQueueStatus) >= 0) {
		iQueueTime = snd_seq_queue_status_get_tick_time(pQueueStatus);
	}

	iQueueTime = pSession->timeq(iQueueTime);

	const long iTimeStart = timeStart();
	if (iQueueTime > -iTimeStart)
		iQueueTime += iTimeStart;

	return iQueueTime;
}


// ALSA subscription port notifier.
QSocketNotifier *qtractorMidiEngine::alsaNotifier (void) const
{
	return m_pAlsaNotifier;
}


// ALSA subscription notifier acknowledgment.
void qtractorMidiEngine::alsaNotifyAck (void)
{
	if (m_pAlsaSubsSeq == nullptr)
		return;

	do {
		snd_seq_event_t *pAlsaEvent;
		snd_seq_event_input(m_pAlsaSubsSeq, &pAlsaEvent);
		snd_seq_free_event(pAlsaEvent);
	}
	while (snd_seq_event_input_pending(m_pAlsaSubsSeq, 0) > 0);
}


// Special slave sync method.
void qtractorMidiEngine::sync (void)
{
	// Pure conditional thread slave synchronization...
	if (m_pOutputThread && midiCursorSync())
		m_pOutputThread->sync();
}


// Read ahead frames configuration.
void qtractorMidiEngine::setReadAhead ( unsigned int iReadAhead )
{
	m_iReadAhead = iReadAhead;
}

unsigned int qtractorMidiEngine::readAhead (void) const
{
	return m_iReadAhead;
}


// Audio/MIDI sync-check and cursor predicate.
qtractorSessionCursor *qtractorMidiEngine::midiCursorSync ( bool bStart )
{
	// Must have a valid session...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return nullptr;

	// We'll need access to master audio engine...
	qtractorSessionCursor *pAudioCursor
		= pSession->audioEngine()->sessionCursor();
	if (pAudioCursor == nullptr)
		return nullptr;

	// And to our slave MIDI engine too...
	qtractorSessionCursor *pMidiCursor = sessionCursor();
	if (pMidiCursor == nullptr)
		return nullptr;

	// Can MIDI be ever behind audio?
	if (bStart) {
		pMidiCursor->seek(pAudioCursor->frame());
	//	pMidiCursor->setFrameTime(pAudioCursor->frameTime());
	}
	else // No, it cannot be behind more than the read-ahead period...
	if (pMidiCursor->frameTime() > pAudioCursor->frameTime() + m_iReadAhead)
		return nullptr;

	// Nope. OK.
	return pMidiCursor;
}


// MIDI output process cycle iteration.
void qtractorMidiEngine::process (void)
{
	// Must have a valid session...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	// Bail out if the audio-metronome is under count-in...
	if (pSession->audioEngine()->countIn() > 0)
		return;

	// Get a handle on our slave MIDI engine...
	qtractorSessionCursor *pMidiCursor = midiCursorSync();
	// Isn't MIDI slightly behind audio?
	if (pMidiCursor == nullptr)
		return;

	// Metronome/count-in stuff...
	if (m_iCountIn > 0) {
		if (m_iCountInFrameStart < m_iCountInFrameEnd) {
			unsigned long iCountInFrameEnd = m_iCountInFrameStart + m_iReadAhead;
			if (iCountInFrameEnd > m_iCountInFrameEnd)
				iCountInFrameEnd = m_iCountInFrameEnd;
			processCountIn(m_iCountInFrameStart, iCountInFrameEnd);
			m_iCountInFrameStart = iCountInFrameEnd;
		}
		// Bail out...
		return;
	}

	// Now for the next read-ahead bunch...
	unsigned long iFrameStart = pMidiCursor->frame();
	unsigned long iFrameEnd   = iFrameStart + m_iReadAhead;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEngine[%p]::process(%lu, %lu)",
		this, iFrameStart, iFrameEnd);
#endif

	// Split processing, in case we're looping...
	const bool bLooping = pSession->isLooping();
	const unsigned long le = pSession->loopEnd();
	if (bLooping && iFrameStart < le) {
		// Loop-length might be shorter than the read-ahead...
		while (iFrameEnd >= le) {
			// Process metronome clicks...
			processMetro(iFrameStart, le);
			// Process the remaining until end-of-loop...
			pSession->process(pMidiCursor, iFrameStart, le);
			// Reset to start-of-loop...
			iFrameStart = pSession->loopStart();
			iFrameEnd   = iFrameStart + (iFrameEnd - le);
			pMidiCursor->seek(iFrameStart);
			// This is really a must...
			m_iFrameStart -= pSession->loopEnd();
			m_iFrameStart += pSession->loopStart();
			m_iTimeStart  -= pSession->loopEndTime();
			m_iTimeStart  += pSession->loopStartTime();
		//	resetDrift(); -- Drift correction?
		}
	}

	// Process metronome clicks...
	processMetro(iFrameStart, iFrameEnd);
	// Regular range...
	pSession->process(pMidiCursor, iFrameStart, iFrameEnd);

	// Sync with loop boundaries (unlikely?)...
	if (bLooping && iFrameStart < le && iFrameEnd >= le)
		iFrameEnd = pSession->loopStart() + (iFrameEnd - le);

	// Sync to the next bunch, also critical for Audio-MIDI sync...
	pMidiCursor->seek(iFrameEnd);
	pMidiCursor->process(m_iReadAhead);

	// Flush the MIDI engine output queue...
	snd_seq_drain_output(m_pAlsaSeq);

	// Always do the queue drift stats
	// at the bottom of the pack...
	driftCheck();
}


// Reset queue time.
void qtractorMidiEngine::resetTime (void)
{
	if (m_pOutputThread)
		m_pOutputThread->resetSync();
}

void qtractorMidiEngine::resetSync (void)
{
	qtractorSession *pSession = session();
	if (pSession && m_pAlsaSeq) {
		snd_seq_stop_queue(m_pAlsaSeq, m_iAlsaQueue, nullptr);
		m_iAudioFrameStart = pSession->audioEngine()->jackFrameTime();
		snd_seq_start_queue(m_pAlsaSeq, m_iAlsaQueue, nullptr);
	}
}


// Reset queue tempo.
void qtractorMidiEngine::resetTempo (void)
{
	// It must be surely activated...
	if (!isActivated())
		return;

	// Needs a valid cursor...
	if (m_pMetroCursor == nullptr)
		return;

	// Reset tempo cursor.
	m_pMetroCursor->reset();

	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	// Recache tempo node...
	qtractorTimeScale::Node *pNode
		= m_pMetroCursor->seekFrame(pSession->playHead());

	snd_seq_queue_tempo_t *pQueueTempo;
	snd_seq_queue_tempo_alloca(&pQueueTempo);
	// Fill tempo struct with current tempo info.
	snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pQueueTempo);
	// Set the new intended ones...
	snd_seq_queue_tempo_set_ppq(pQueueTempo,
		qtractorTimeScale::TICKS_PER_BEAT_HRQ);
	snd_seq_queue_tempo_set_tempo(pQueueTempo,
		(unsigned int) (60000000.0f / pNode->tempo));
	// Give tempo struct to the queue.
	snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pQueueTempo);

	// Set queue tempo...
	if (m_bDriftCorrect && pSession->isPlaying()) {
		m_iFrameDrift = long(pNode->frameFromTick(queueTime()));
		m_iFrameDrift -= long(pSession->playHead());
	}

	// Recache tempo value...
	m_fMetroTempo = pNode->tempo;

	// MIDI Clock tempo tracking.
	m_iClockCount = 0;
	m_fClockTempo = pNode->tempo;
}


// Reset all MIDI monitoring...
void qtractorMidiEngine::resetAllMonitors (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	// Reset common MIDI monitor stuff...
	qtractorMidiMonitor::resetTime(
		pSession->timeScale(), pSession->playHead());

	// Reset all MIDI bus monitors...
	for (qtractorBus *pBus = buses().first();
			pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus) {
			if (pMidiBus->midiMonitor_in())
				pMidiBus->midiMonitor_in()->reset();
			if (pMidiBus->midiMonitor_out())
				pMidiBus->midiMonitor_out()->reset();
		}
	}

	// Reset all MIDI track channel monitors...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi) {
			qtractorMidiMonitor *pMidiMonitor
				= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
			if (pMidiMonitor)
				pMidiMonitor->reset();
		}
	}

	// HACK: Reset step-input...
	m_proxy.notifyInpEvent(InpReset);
}


// Reset all MIDI instrument/controllers...
void qtractorMidiEngine::resetAllControllers ( bool bForceImmediate )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::resetAllControllers(%d)", int(bForceImmediate));
#endif

	// Deferred processing?
	if (!bForceImmediate) {
		++m_iResetAllControllersPending;
		return;
	}

	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	// Reset all MIDI bus controllers...
	for (qtractorBus *pBus = buses().first();
			pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus) {
			qtractorMidiMonitor *pOutputMonitor = pMidiBus->midiMonitor_out();
			if (pOutputMonitor) {
				pMidiBus->sendSysexList(); // SysEx setup!
				pMidiBus->setMasterVolume(pOutputMonitor->gain());
				pMidiBus->setMasterPanning(pOutputMonitor->panning());
			} else {
				qtractorMidiMonitor *pInputMonitor = pMidiBus->midiMonitor_in();
				if (pInputMonitor) {
					pMidiBus->setMasterVolume(pInputMonitor->gain());
					pMidiBus->setMasterPanning(pInputMonitor->panning());
				}
			}
		}
	}

	// Reset all MIDI tracks channel bank/program and controllers...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi) {
			// MIDI track instrument patching (channel bank/program)...
			pTrack->setMidiPatch(pSession->instruments());
			// MIDI track channel controllers...
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->outputBus());
			if (pMidiBus) {
				pMidiBus->setVolume(pTrack, pTrack->gain());
				pMidiBus->setPanning(pTrack, pTrack->panning());
			}
		}
	}

	// Re-send all mapped feedback MIDI controllers...
	qtractorMidiControl *pMidiControl
		= qtractorMidiControl::getInstance();
	if (pMidiControl)
		pMidiControl->sendAllControllers();

	// Done.
	m_iResetAllControllersPending = 0;
}


// Whether is actually pending a reset of
// all the MIDI instrument/controllers...
bool qtractorMidiEngine::isResetAllControllersPending (void) const
{
	return (m_iResetAllControllersPending > 0);
}


// Shut-off all MIDI buses (stop)...
void qtractorMidiEngine::shutOffAllBuses ( bool bClose )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEngine::shutOffAllBuses(%d)", int(bClose));
#endif

	for (qtractorBus *pBus = qtractorEngine::buses().first();
			pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus)
			pMidiBus->shutOff(bClose);
	}
}


// Shut-off all MIDI tracks (panic)...
void qtractorMidiEngine::shutOffAllTracks (void)
{
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::shutOffAllTracks()");
#endif

	QHash<qtractorMidiBus *, unsigned short> channels;

	const unsigned long iQueueTime = queueTime();
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi) {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->outputBus());
			if (pMidiBus) {
				const unsigned short iChannel = pTrack->midiChannel();
				const unsigned short iChannelMask = (1 << iChannel);
				const unsigned short iChannelFlags = channels.value(pMidiBus, 0);
				if ((iChannelFlags & iChannelMask) == 0) {
					pMidiBus->dequeueNoteOffs(iQueueTime);
					pMidiBus->setController(pTrack, ALL_SOUND_OFF);
					pMidiBus->setController(pTrack, ALL_NOTES_OFF);
					pMidiBus->setController(pTrack, ALL_CONTROLLERS_OFF);
					channels.insert(pMidiBus, iChannelFlags | iChannelMask);
				}
			}
		}
	}

	resetAllControllers(true); // Force immediate!
}


// ALSA port input registry methods.
void qtractorMidiEngine::addInputBus ( qtractorMidiBus *pMidiBus )
{
	m_inputBuses.insert(pMidiBus->alsaPort(), pMidiBus);
}

void qtractorMidiEngine::removeInputBus ( qtractorMidiBus *pMidiBus )
{
	m_inputBuses.remove(pMidiBus->alsaPort());
}


void qtractorMidiEngine::addInputBuffer (
	int iAlsaPort, qtractorMidiInputBuffer *pMidiInputBuffer )
{
	m_inputBuffers.insert(iAlsaPort, pMidiInputBuffer);
}

void qtractorMidiEngine::removeInputBuffer ( int iAlsaPort )
{
	m_inputBuffers.remove(iAlsaPort);
}


// MIDI event capture method.
void qtractorMidiEngine::capture ( snd_seq_event_t *pEv )
{
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	const int iAlsaPort = pEv->dest.port;

	qtractorMidiEvent::EventType type;

	unsigned char  channel  = 0;
	unsigned short param    = 0;
	unsigned short value    = 0;
	unsigned long  duration = 0;

	unsigned char *pSysex   = nullptr;
	unsigned short iSysex   = 0;

	unsigned long tick = pSession->timeq(pEv->time.tick);

	// - capture quantization...
	if (m_iCaptureQuantize > 0) {
		const unsigned long q
			= pSession->ticksPerBeat()
			/ m_iCaptureQuantize;
		tick = q * ((tick + (q >> 1)) / q);
	}

#ifdef CONFIG_DEBUG_0
	// - show event for debug purposes...
	fprintf(stderr, "MIDI In %d: %06lu 0x%02x", iAlsaPort, tick, pEv->type);
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

	switch (pEv->type) {
//	case SND_SEQ_EVENT_NOTE: -- Unlikely real-time input.
	case SND_SEQ_EVENT_NOTEON:
		type     = qtractorMidiEvent::NOTEON;
		channel  = pEv->data.note.channel;
		param    = pEv->data.note.note;
		value    = pEv->data.note.velocity;
	//	duration = pEv->data.note.duration;
		if (value == 0) {
			pEv->type = SND_SEQ_EVENT_NOTEOFF;
			type = qtractorMidiEvent::NOTEOFF;
		}
		break;
	case SND_SEQ_EVENT_NOTEOFF:
		type     = qtractorMidiEvent::NOTEOFF;
		channel  = pEv->data.note.channel;
		param    = pEv->data.note.note;
		value    = pEv->data.note.velocity;
	//	duration = pEv->data.note.duration;
		break;
	case SND_SEQ_EVENT_KEYPRESS:
		type     = qtractorMidiEvent::KEYPRESS;
		channel  = pEv->data.note.channel;
		param    = pEv->data.note.note;
		value    = pEv->data.note.velocity;
		break;
	case SND_SEQ_EVENT_CONTROLLER:
		type     = qtractorMidiEvent::CONTROLLER;
		channel  = pEv->data.control.channel;
		param    = pEv->data.control.param;
		value    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_REGPARAM:
		type     = qtractorMidiEvent::REGPARAM;
		channel  = pEv->data.control.channel;
		param    = pEv->data.control.param;
		value    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_NONREGPARAM:
		type     = qtractorMidiEvent::NONREGPARAM;
		channel  = pEv->data.control.channel;
		param    = pEv->data.control.param;
		value    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_CONTROL14:
		type     = qtractorMidiEvent::CONTROL14;
		channel  = pEv->data.control.channel;
		param    = pEv->data.control.param;
		value    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_PGMCHANGE:
		type     = qtractorMidiEvent::PGMCHANGE;
		channel  = pEv->data.control.channel;
		param    = pEv->data.control.value;
		value    = 0x7f;
		break;
	case SND_SEQ_EVENT_CHANPRESS:
		type     = qtractorMidiEvent::CHANPRESS;
		channel  = pEv->data.control.channel;
	//	param    = 0;
		value    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_PITCHBEND:
		type     = qtractorMidiEvent::PITCHBEND;
		channel  = pEv->data.control.channel;
	//	param    = 0;
		value    = (unsigned short) (0x2000 + pEv->data.control.value);
		break;
	case SND_SEQ_EVENT_START:
	case SND_SEQ_EVENT_STOP:
	case SND_SEQ_EVENT_CONTINUE:
	case SND_SEQ_EVENT_SONGPOS:
		// Trap SPP commands...
		if ((m_sppMode & qtractorBus::Input)
			&& m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
			// Post the stuffed event...
			m_proxy.notifySppEvent(int(pEv->type), pEv->data.control.value);
		}
		// Not handled any longer.
		return;
	case SND_SEQ_EVENT_CLOCK:
		// Trap MIDI Clocks...
		if ((m_clockMode & qtractorBus::Input)
			&& m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
			static QElapsedTimer s_clockTimer;
			if (++m_iClockCount == 1)
				s_clockTimer.start();
			else
			if (m_iClockCount > 72) { // 3 beat averaging...
				m_iClockCount = 0;
				const float fClockTempo
					= ::rintf(180000.0f / float(s_clockTimer.elapsed()));
				if (qAbs(fClockTempo - m_fClockTempo) / m_fClockTempo > 0.01f) {
					m_fClockTempo = fClockTempo;
					// Post the stuffed event...
					m_proxy.notifyClkEvent(m_fClockTempo);
				}
			}
		}
		// Not handled any longer.
		return;
	case SND_SEQ_EVENT_SYSEX:
		type   = qtractorMidiEvent::SYSEX;
		pSysex = (unsigned char *) pEv->data.ext.ptr;
		iSysex = (unsigned short)  pEv->data.ext.len;
		// Trap MMC commands...
		if ((m_mmcMode & qtractorBus::Input)
			&& pSysex[1] == 0x7f && pSysex[3] == 0x06 // MMC command mode.
			&& m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
			// Post the stuffed event...
			m_proxy.notifyMmcEvent(qtractorMmcEvent(pSysex));
			// Bail out, right now!
			return;
		}
		break;
	default:
		// Not handled here...
		return;
	}

	unsigned long iTime = m_iTimeStartEx + tick;

	// Wrap in loop-range, if any...
	if (pSession->isLooping()) {
		const unsigned long iLoopEndTime = pSession->loopEndTime();
		if (iTime > iLoopEndTime) {
			const unsigned long iLoopStartTime = pSession->loopStartTime();
			iTime = iLoopStartTime
				+ (iTime - iLoopEndTime)
				% (iLoopEndTime - iLoopStartTime);
		}
	}

	// Take care of recording, if any...
	bool bRecording = (pSession->isRecording() && isPlaying());
	if (bRecording) {
		// Take care of punch-in/out-range...
		bRecording = (!pSession->isPunching() ||
			(iTime >= pSession->punchInTime() &&
			 iTime <  pSession->punchOutTime()));
	}

#if 0//-- Unlikely real-time input.
	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekTick(iTime);
	const unsigned long t0 = pNode->frameFromTick(iTime);
	const unsigned long f0 = m_iFrameStartEx;
	const unsigned long t1 = (t0 < f0 ? t0 : t0 - f0);
	unsigned long t2 = t1;
	if (type == qtractorMidiEvent::NOTEON && duration > 0) {
		const unsigned long iTimeOff = iTime + (duration - 1);
		pNode = cursor.seekTick(iTimeOff);
		t2 += (pNode->frameFromTick(iTimeOff) - t0);
	}
#endif

	qtractorMidiManager *pMidiManager;

	// Whether to notify any step input...
	unsigned short iInpEvents = 0;

	// Now check which bus and track we're into...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// Must be a MIDI track...
		if (pTrack->trackType() != qtractorTrack::Midi)
			continue;
		// Must be capture/passthru mode
		// and for the intended channel...
		const bool bRecord  = pTrack->isRecord();
		const bool bMonitor = pSession->isTrackMonitor(pTrack);
		if ((bRecord || bMonitor)
		//	&& !pTrack->isMute() && (!pSession->soloTracks() || pTrack->isSolo())
			&& pSession->isTrackMidiChannel(pTrack, channel)) {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->inputBus());
			if (pMidiBus && pMidiBus->alsaPort() == iAlsaPort) {
				// Is it actually recording?...
				if (bRecord) {
					qtractorMidiSequence *pSeq = nullptr;
					qtractorMidiClip *pMidiClip
						= static_cast<qtractorMidiClip *> (pTrack->clipRecord());
					if (pMidiClip)
						pSeq = pMidiClip->sequence();
					if (pMidiClip && pSeq && pTrack->isClipRecordEx()) {
						// Account for step-input recording...
						if (!isPlaying()) {
							// Check step-input auto-advance...
							if (type != qtractorMidiEvent::NOTEOFF) {
								pMidiClip->setStepInputLast(
									pSession->audioEngine()->jackFrameTime());
							}
							// Set quantized step-input event time...
							iTime = pMidiClip->stepInputHeadTime();
							if (type == qtractorMidiEvent::NOTEON)
								duration = pMidiClip->stepInputTailTime() - iTime;
							else
							if (type == qtractorMidiEvent::NOTEOFF)
								pSeq = nullptr; // ignore all note-offs...
						}
						// Make sure it falls inside the recording clip...
						const unsigned long iClipStartTime
							= pMidiClip->clipStartTime();
						const unsigned long iClipEndTime
							= iClipStartTime + pMidiClip->clipLengthTime();
						if (iTime >= iClipStartTime && iTime < iClipEndTime)
							tick = iTime - iClipStartTime + pMidiClip->clipOffsetTime();
						else
						if (type != qtractorMidiEvent::NOTEOFF)
							pSeq = nullptr;
					}
					else
					if (!isPlaying())
						pSeq = nullptr;
					// Yep, maybe we have a new MIDI event on record...
					if (pSeq) {
						qtractorMidiEvent *pEvent = new qtractorMidiEvent(
							tick, type, param, value, duration);
						if (pSysex)
							pEvent->setSysex(pSysex, iSysex);
						if (isPlaying()) {
							pSeq->addEvent(pEvent);
						} else {
							m_inpMutex.lock();
							m_inpEvents.insert(pMidiClip, pEvent);
							m_inpMutex.unlock();
							++iInpEvents;
						}
					}
				}
				// Track input monitoring...
				qtractorMidiMonitor *pMidiMonitor
					= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
				if (pMidiMonitor)
					pMidiMonitor->enqueue(type, value);
				// Output monitoring on record...
				if (bMonitor) {
					pMidiBus = static_cast<qtractorMidiBus *> (pTrack->outputBus());
					if (pMidiBus && pMidiBus->midiMonitor_out()) {
						// FIXME: MIDI-thru channel filtering prolog... 
						const unsigned short iOldChannel
							= pEv->data.note.channel;
						pEv->data.note.channel = pTrack->midiChannel();
						// MIDI-thru: same event redirected...
						snd_seq_ev_set_source(pEv, pMidiBus->alsaPort());
						snd_seq_ev_set_subs(pEv);
						snd_seq_ev_set_direct(pEv);
						snd_seq_event_output_direct(m_pAlsaSeq, pEv);
						// Done with MIDI-thru.
						pMidiBus->midiMonitor_out()->enqueue(type, value);
						// Do it for the MIDI plugins too...
						pMidiManager = (pTrack->pluginList())->midiManager();
						if (pMidiManager)
							pMidiManager->direct(pEv); //queued(pEv,t1[,t2]);
						if (!pMidiBus->isMonitor()
							&& pMidiBus->pluginList_out()) {
							pMidiManager = (pMidiBus->pluginList_out())->midiManager();
							if (pMidiManager)
								pMidiManager->direct(pEv); //queued(pEv,t1[,t2]);
						}
						// FIXME: MIDI-thru channel filtering epilog...
						pEv->data.note.channel = iOldChannel;
					}
				}
			}
		}
	}

	// MIDI Bus monitoring...
	qtractorMidiBus *pMidiBus = m_inputBuses.value(iAlsaPort, nullptr);
	if (pMidiBus) {
		// Input monitoring...
		if (pMidiBus->midiMonitor_in())
			pMidiBus->midiMonitor_in()->enqueue(type, value);
		// Do it for the MIDI input plugins too...
		if (pMidiBus->pluginList_in()) {
			pMidiManager = (pMidiBus->pluginList_in())->midiManager();
			if (pMidiManager)
				pMidiManager->direct(pEv); //queued(pEv,t1[,t2]);
		}
		// Output monitoring on passthru...
		if (pMidiBus->isMonitor()) {
			// Do it for the MIDI output plugins too...
			if (pMidiBus->pluginList_out()) {
				pMidiManager = (pMidiBus->pluginList_out())->midiManager();
				if (pMidiManager)
					pMidiManager->direct(pEv); //queued(pEv,t1[,t2]);
			}
			if (pMidiBus->midiMonitor_out()) {
				// MIDI-thru: same event redirected...
				snd_seq_ev_set_source(pEv, pMidiBus->alsaPort());
				snd_seq_ev_set_subs(pEv);
				snd_seq_ev_set_direct(pEv);
				snd_seq_event_output_direct(m_pAlsaSeq, pEv);
				// Done with MIDI-thru.
				pMidiBus->midiMonitor_out()->enqueue(type, value);
			}
		}
	} else {
		// Input buffers (eg. insert returns)...
		qtractorMidiInputBuffer *pMidiInputBuffer
			= m_inputBuffers.value(iAlsaPort, nullptr);
		if (pMidiInputBuffer)
			pMidiInputBuffer->enqueue(pEv);
	}

	// Trap controller commands...
	if (type == qtractorMidiEvent::SYSEX)
		return;

	if (m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
		// Post the stuffed event...
		m_proxy.notifyCtlEvent(
			qtractorCtlEvent(type, channel, param, value));
	}

	// Notify step-input events...
	if (iInpEvents > 0) {
		// Post the stuffed event(s)...
		m_proxy.notifyInpEvent(InpEvent);
	}
}


// MIDI event enqueue method.
void qtractorMidiEngine::enqueue ( qtractorTrack *pTrack,
	qtractorMidiEvent *pEvent, unsigned long iTime, float fGain )
{
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	// Target MIDI bus...
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == nullptr)
		return;
#if 0
	// HACK: Ignore our own mixer-monitor supplied controllers...
	if (pEvent->type() == qtractorMidiEvent::CONTROLLER) {
		if (pEvent->controller() == CHANNEL_VOLUME ||
			pEvent->controller() == CHANNEL_PANNING)
			return;
	}
#endif
	const int iAlsaPort = pMidiBus->alsaPort();

	// Scheduled delivery: take into account
	// the time playback/queue started...
	const unsigned long tick
		= (long(iTime) > m_iTimeStart ? iTime - m_iTimeStart : 0);

#ifdef CONFIG_DEBUG_0
	// - show event for debug purposes...
	fprintf(stderr, "MIDI Out %d: %06lu 0x%02x", iAlsaPort,
		tick, int(pEvent->type() | pTrack->midiChannel()));
	if (pEvent->type() == qtractorMidiEvent::SYSEX) {
		fprintf(stderr, " sysex {");
		unsigned char *data = (unsigned char *) pEvent->sysex();
		for (unsigned int i = 0; i < pEvent->sysex_len(); ++i)
			fprintf(stderr, " %02x", data[i]);
		fprintf(stderr, " }\n");
	} else {
		fprintf(stderr, " %3d %3d (duration=%lu)\n",
			pEvent->note(), pEvent->velocity(),
			pEvent->duration());
	}
#endif

	// Initialize outbound event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Set Event tag...
	ev.tag = (unsigned char) (pTrack->midiTag() & 0xff);

	// Addressing...
	snd_seq_ev_set_source(&ev, iAlsaPort);
	snd_seq_ev_set_subs(&ev);

	// Scheduled delivery...
	snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, pSession->timep(tick));

	unsigned long iDuration = 0;

	// Set proper event data...
	switch (pEvent->type()) {
		case qtractorMidiEvent::NOTEON:
			ev.type = SND_SEQ_EVENT_NOTE;
			ev.data.note.channel  = pTrack->midiChannel();
			ev.data.note.note     = pEvent->note();
			ev.data.note.velocity = int(fGain * float(pEvent->value())) & 0x7f;
			iDuration = pEvent->duration();
			if (pSession->isLooping()) {
				const unsigned long iLoopEndTime
					= pSession->tickFromFrame(pSession->loopEnd());
				if (iLoopEndTime > iTime && iLoopEndTime < iTime + pEvent->duration())
					iDuration = iLoopEndTime - iTime;
			}
			ev.data.note.duration = pSession->timep(iDuration);
			break;
		case qtractorMidiEvent::KEYPRESS:
			ev.type = SND_SEQ_EVENT_KEYPRESS;
			ev.data.note.channel  = pTrack->midiChannel();
			ev.data.note.note     = pEvent->note();
			ev.data.note.velocity = pEvent->velocity();
			ev.data.note.duration = 0;
			break;
		case qtractorMidiEvent::CONTROLLER:
			ev.type = SND_SEQ_EVENT_CONTROLLER;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.param   = pEvent->controller();
			// Track properties override...
			switch (pEvent->controller()) {
			case BANK_SELECT_MSB:
				if (pTrack->midiBank() >= 0)
					ev.data.control.value = (pTrack->midiBank() & 0x3f80) >> 7;
				else
					ev.data.control.value = pEvent->value();
				break;
			case BANK_SELECT_LSB:
				if (pTrack->midiBank() >= 0)
					ev.data.control.value = (pTrack->midiBank() & 0x7f);
				else
					ev.data.control.value = pEvent->value();
				break;
			case CHANNEL_VOLUME:
				ev.data.control.value = int(pTrack->gain() * float(pEvent->value())) & 0x7f;
				break;
			default:
				ev.data.control.value = pEvent->value();
				break;
			}
			break;
		case qtractorMidiEvent::REGPARAM:
			ev.type = SND_SEQ_EVENT_REGPARAM;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.param   = pEvent->param();
			ev.data.control.value   = pEvent->value();
			break;
		case qtractorMidiEvent::NONREGPARAM:
			ev.type = SND_SEQ_EVENT_NONREGPARAM;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.param   = pEvent->param();
			ev.data.control.value   = pEvent->value();
			break;
		case qtractorMidiEvent::CONTROL14:
			ev.type = SND_SEQ_EVENT_CONTROL14;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.param   = pEvent->param();
			ev.data.control.value   = pEvent->value();
			break;
		case qtractorMidiEvent::PGMCHANGE:
			ev.type = SND_SEQ_EVENT_PGMCHANGE;
			ev.data.control.channel = pTrack->midiChannel();
			// HACK: Track properties override...
			if (pTrack->midiProg() >= 0)
				ev.data.control.value = pTrack->midiProg();
			else
				ev.data.control.value = pEvent->param();
			break;
		case qtractorMidiEvent::CHANPRESS:
			ev.type = SND_SEQ_EVENT_CHANPRESS;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.value   = pEvent->value();
			break;
		case qtractorMidiEvent::PITCHBEND:
			ev.type = SND_SEQ_EVENT_PITCHBEND;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.value   = pEvent->pitchBend();
			break;
		case qtractorMidiEvent::SYSEX: {
			ev.type = SND_SEQ_EVENT_SYSEX;
			unsigned char *data = pEvent->sysex();
			unsigned short data_len = pEvent->sysex_len();
			if (pMidiBus->midiMonitor_out()) {
				// HACK: Master volume: make a copy
				// and update it while queued...
				if (data[1] == 0x7f &&
					data[2] == 0x7f &&
					data[3] == 0x04 &&
					data[4] == 0x01) {
					static unsigned char s_data[8];
					if (data_len > sizeof(s_data))
						data_len = sizeof(s_data);
					::memcpy(s_data, data, data_len);
					const float fGain = pMidiBus->midiMonitor_out()->gain();
					data = &s_data[0];
					data[5] = 0;
					data[6] = int(fGain * float(data[6])) & 0x7f;
				}
			}
			snd_seq_ev_set_sysex(&ev, data_len, data);
			break;
		}
		default:
			break;
	}

	// Pump it into the queue.
	snd_seq_event_output(m_pAlsaSeq, &ev);

	// MIDI track monitoring...
	qtractorMidiMonitor *pMidiMonitor
		= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
	if (pMidiMonitor)
		pMidiMonitor->enqueue(pEvent->type(), pEvent->value(), tick);
	// MIDI bus monitoring...
	if (pMidiBus->midiMonitor_out())
		pMidiBus->midiMonitor_out()->enqueue(
			pEvent->type(), pEvent->value(), tick);

	// Do it for the MIDI track plugins too...
	qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekTick(iTime);
	const long f0 = m_iFrameStart + (pTrack->pluginList())->latency();
	const unsigned long t0 = pNode->frameFromTick(iTime);
	const unsigned long t1 = (long(t0) < f0 ? t0 : t0 - f0);
	unsigned long t2 = t1;

	if (ev.type == SND_SEQ_EVENT_NOTE && iDuration > 0) {
		const unsigned long iTimeOff = iTime + (iDuration - 1);
		pMidiBus->enqueueNoteOff(&ev, iTime, iTimeOff);
		pNode = cursor.seekTick(iTimeOff);
		t2 += (pNode->frameFromTick(iTimeOff) - t0);
	}

	qtractorMidiManager *pMidiManager
		= (pTrack->pluginList())->midiManager();
	if (pMidiManager)
		pMidiManager->queued(&ev, t1, t2);

	// And for the MIDI output plugins as well...
	if (pMidiBus->pluginList_out()) {
		pMidiManager = (pMidiBus->pluginList_out())->midiManager();
		if (pMidiManager)
			pMidiManager->queued(&ev, t1, t2);
	}
}


// Reset ouput queue drift stats (audio vs. MIDI)...
void qtractorMidiEngine::resetDrift (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::resetDrift()");
#endif

//--DRIFT-SKEW-BEGIN--
	snd_seq_queue_tempo_t *pQueueTempo;
	snd_seq_queue_tempo_alloca(&pQueueTempo);
	snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pQueueTempo);
	snd_seq_queue_tempo_set_skew(pQueueTempo,
		snd_seq_queue_tempo_get_skew_base(pQueueTempo));
	snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pQueueTempo);
//--DRIFT-SKEW-END--

	m_iDriftCheck = 0;
	m_iDriftCount = DRIFT_CHECK;

	m_iTimeDrift  = 0;
	m_iFrameDrift = 0;
}


// Do ouput queue status (audio vs. MIDI)...
void qtractorMidiEngine::driftCheck (void)
{
	if (!m_bDriftCorrect)
		return;
	if (++m_iDriftCheck < m_iDriftCount)
		return;

	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;
//	if (pSession->isRecording())
//		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	if (m_pMetroCursor == nullptr)
		return;

	// Time to have some corrective approach...?
	const unsigned short iTicksPerBeat
		= pSession->ticksPerBeat();
	const long iAudioFrame = m_iFrameStart + m_iFrameDrift
		+ pAudioEngine->jackFrameTime() - m_iAudioFrameStart;
	qtractorTimeScale::Node *pNode = m_pMetroCursor->seekFrame(iAudioFrame);
	const long iAudioTime
		= long(pNode->tickFromFrame(iAudioFrame)) - m_iTimeStart;
	const long iMidiTime
		= long(queueTime());
	const long iMinDeltaTime
		= long(iTicksPerBeat >> 8) + 1;
	const long iMaxDeltaTime
		= long(iTicksPerBeat >> 4) + 1;
	const long iDeltaTime = (iAudioTime - iMidiTime);
	if (qAbs(iDeltaTime) < iMaxDeltaTime) {
	//--DRIFT-SKEW-BEGIN--
		const long iTimeDrift = m_iTimeDrift + (iDeltaTime << 1);
		snd_seq_queue_tempo_t *pQueueTempo;
		snd_seq_queue_tempo_alloca(&pQueueTempo);
		snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pQueueTempo);
		const unsigned int iSkewBase
			= snd_seq_queue_tempo_get_skew_base(pQueueTempo);
		const unsigned int iSkewPrev
			= snd_seq_queue_tempo_get_skew(pQueueTempo);
		const unsigned int iSkewNext = (unsigned int) (float(iSkewBase)
			* float(iAudioTime + iTimeDrift) / float(iAudioTime));
		if (iSkewNext != iSkewPrev) {
			snd_seq_queue_tempo_set_skew(pQueueTempo, iSkewNext);
			snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pQueueTempo);
		}
	#ifdef CONFIG_DEBUG//_0
		qDebug("qtractorMidiEngine::driftCheck(%u): "
			"iAudioTime=%ld iMidiTime=%ld (%ld) iTimeDrift=%ld (%.2g%%)",
			m_iDriftCount, iAudioTime, iMidiTime, iDeltaTime, iTimeDrift,
			((100.0f * float(iSkewNext)) / float(iSkewBase)) - 100.0f);
	#endif
		// Adaptive drift check... plan A.
		const bool bDecreased = (qAbs(m_iTimeDrift) > qAbs(iTimeDrift));
		const bool bOvershoot = ((m_iTimeDrift * iTimeDrift) < 0);
		m_iTimeDrift = iTimeDrift;
		if ((!bDecreased || bOvershoot)
			&& (qAbs(iDeltaTime) > iMinDeltaTime)
			&& (m_iDriftCheck > DRIFT_CHECK_MIN)) {
			m_iDriftCount >>= 1;
		//	m_iTimeDrift  <<= 1;
		}
		else
		// Adaptive drift check... plan B.
		if ((bDecreased || !bOvershoot)
		//	&& (qAbs(iDeltaTime) < iMinDeltaTime)
			&& (m_iDriftCheck < DRIFT_CHECK_MAX)) {
			m_iDriftCount <<= 1;
			m_iTimeDrift  >>= 1;
		}
	//--DRIFT-SKEW-END--
	}

	// Restart counting...
	m_iDriftCheck = 0;
}


// Flush ouput queue (if necessary)...
void qtractorMidiEngine::flush (void)
{
	// Really flush MIDI output...
	if (m_pOutputThread)
		m_pOutputThread->flushSync();
}


// Device engine initialization method.
bool qtractorMidiEngine::init (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return false;

	// Try open a new client...
	if (snd_seq_open(&m_pAlsaSeq, "default",
			SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0)
		return false;
	if (m_pAlsaSeq == nullptr)
		return false;

	// Fix client name.
	const QByteArray aClientName = pSession->clientName().toUtf8();
	snd_seq_set_client_name(m_pAlsaSeq, aClientName.constData());

	m_iAlsaClient = snd_seq_client_id(m_pAlsaSeq);
	m_iAlsaQueue  = snd_seq_alloc_queue(m_pAlsaSeq);

	// Set sequencer queue timer.
	if (qtractorMidiTimer().indexOf(m_iAlsaTimer) > 0) {
		qtractorMidiTimer::Key key(m_iAlsaTimer);
		snd_timer_id_t *pAlsaTimerId;
		snd_timer_id_alloca(&pAlsaTimerId);
		snd_timer_id_set_class(pAlsaTimerId, key.alsaTimerClass());
		snd_timer_id_set_card(pAlsaTimerId, key.alsaTimerCard());
		snd_timer_id_set_device(pAlsaTimerId, key.alsaTimerDevice());
		snd_timer_id_set_subdevice(pAlsaTimerId, key.alsaTimerSubDev());
		snd_seq_queue_timer_t *pAlsaTimer;
		snd_seq_queue_timer_alloca(&pAlsaTimer);
		snd_seq_queue_timer_set_type(pAlsaTimer, SND_SEQ_TIMER_ALSA);
		snd_seq_queue_timer_set_id(pAlsaTimer, pAlsaTimerId);
		snd_seq_set_queue_timer(m_pAlsaSeq, m_iAlsaQueue, pAlsaTimer);
	}

	// Setup subscriptions stuff...
	if (snd_seq_open(&m_pAlsaSubsSeq, "hw", SND_SEQ_OPEN_DUPLEX, 0) >= 0) {
		m_iAlsaSubsPort = snd_seq_create_simple_port(
			m_pAlsaSubsSeq, clientName().toUtf8().constData(),
			SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE |
			SND_SEQ_PORT_CAP_NO_EXPORT, SND_SEQ_PORT_TYPE_APPLICATION);
		if (m_iAlsaSubsPort >= 0) {
			struct pollfd pfd[1];
			snd_seq_addr_t seq_addr;
			snd_seq_port_subscribe_t *pAlsaSubs;
			snd_seq_port_subscribe_alloca(&pAlsaSubs);
			seq_addr.client = SND_SEQ_CLIENT_SYSTEM;
			seq_addr.port   = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
			snd_seq_port_subscribe_set_sender(pAlsaSubs, &seq_addr);
			seq_addr.client = snd_seq_client_id(m_pAlsaSubsSeq);
			seq_addr.port   = m_iAlsaSubsPort;
			snd_seq_port_subscribe_set_dest(pAlsaSubs, &seq_addr);
			snd_seq_subscribe_port(m_pAlsaSubsSeq, pAlsaSubs);
			snd_seq_poll_descriptors(m_pAlsaSubsSeq, pfd, 1, POLLIN);
			m_pAlsaNotifier = new QSocketNotifier(
				pfd[0].fd, QSocketNotifier::Read);
		}
	}

	// Time-scale cursor (tempo/time-signature map)
	m_pMetroCursor = new qtractorTimeScale::Cursor(pSession->timeScale());

	return true;
}


// Device engine activation method.
bool qtractorMidiEngine::activate (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return false;

	// Open SMF player to last...
	openPlayerBus();

	// Open control/metronome buses, at least try...
	openControlBus();
	openMetroBus();

	// Set the read-ahead in frames (0.5s)...
	m_iReadAhead = (pSession->sampleRate() >> 1);

	// Create and start our own MIDI input queue thread...
	m_pInputThread = new qtractorMidiInputThread(this);
	m_pInputThread->start(QThread::TimeCriticalPriority);

	// Create and start our own MIDI output queue thread...
	m_pOutputThread = new qtractorMidiOutputThread(this);
	m_pOutputThread->start(QThread::HighPriority);

	// Reset/zero tickers...
	m_iTimeStart  = 0;
	m_iFrameStart = 0;

	m_iTimeStartEx = m_iTimeStart;

	m_iAudioFrameStart = pSession->audioEngine()->jackFrameTime();

	// Reset output queue drift compensator...
	resetDrift();

	// Reset all dependable monitoring...
	resetAllMonitors();

	return true;
}


// Device engine start method.
bool qtractorMidiEngine::start (void)
{
	// It must be surely activated...
	if (!isActivated())
		return false;

	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return false;

	// Output thread must be around too...
	if (m_pOutputThread == nullptr)
		return false;

	// Close any SMF player out there...
	closePlayer();

	// Initial output thread bumping...
	qtractorSessionCursor *pMidiCursor = midiCursorSync(true);
	if (pMidiCursor == nullptr)
		return false;

	// Reset all dependables...
	resetTempo();
	resetAllMonitors();

	// Reset output queue drift compensator...
	resetDrift();

	// Start queue timer...
	m_iFrameStart = long(pMidiCursor->frame());
	m_iTimeStart  = long(pSession->tickFromFrame(m_iFrameStart));

	m_iTimeStartEx = m_iTimeStart;

	m_iAudioFrameStart = pSession->audioEngine()->jackFrameTime();

	// Start count-in stuff...
	unsigned short iCountInBeats = 0;
	if (m_bCountIn && (
		(m_countInMode == CountInPlayback) ||
		(m_countInMode == CountInRecording && pSession->isRecording())))
		iCountInBeats = m_iCountInBeats;
	if (iCountInBeats > 0) {
		m_iCountIn = iCountInBeats;
		m_iCountInFrame = m_iFrameStart;
		qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
		qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iCountInFrame);
		const unsigned short iCountInBeat
			= pNode->beatFromFrame(m_iCountInFrame);
		m_iCountInFrameStart = m_iCountInFrame;
		m_iCountInFrameEnd = m_iCountInFrameStart
			+ pNode->frameFromBeat(iCountInBeat + iCountInBeats)
			- pNode->frameFromBeat(iCountInBeat);
		m_iCountInTimeStart = m_iTimeStart;
		++m_iCountIn; // Give some slack to the end...
	} else {
		m_iCountIn = 0;
		m_iCountInFrame = 0;
		m_iCountInFrameStart = 0;
		m_iCountInFrameEnd = 0;
		m_iCountInTimeStart = 0;
	}

	// Effectively start sequencer queue timer...
	snd_seq_start_queue(m_pAlsaSeq, m_iAlsaQueue, nullptr);
	snd_seq_drain_output(m_pAlsaSeq);

	// Carry on...
	m_pOutputThread->processSync();

	return true;
}


// Device engine stop method.
void qtractorMidiEngine::stop (void)
{
	if (!isActivated())
		return;

	// Cleanup queues...
	snd_seq_drop_input(m_pAlsaSeq);
	snd_seq_drop_output(m_pAlsaSeq);

	// Stop queue timer...
	snd_seq_stop_queue(m_pAlsaSeq, m_iAlsaQueue, nullptr);

	flush();

	// Shut-off all MIDI buses...
	shutOffAllBuses();

	// Reset all monitors...
	resetAllMonitors();
}


// Device engine deactivation method.
void qtractorMidiEngine::deactivate (void)
{
	// We're stopping now...
	setPlaying(false);

	// Stop our queue threads...
	m_pInputThread->setRunState(false);
	m_pOutputThread->setRunState(false);
	m_pOutputThread->sync();
}


// Device engine cleanup method.
void qtractorMidiEngine::clean (void)
{
	// Clean any (pending?) step-input events...
	m_inpEvents.clear();

	// Clean control/metronome buses...
	deleteControlBus();
	deleteMetroBus();

	// Close SMF player last...
	deletePlayerBus();

	// Delete output thread...
	if (m_pOutputThread) {
		// Make it nicely...
		if (m_pOutputThread->isRunning()) do {
			m_pOutputThread->setRunState(false);
		//	m_pOutputThread->terminate();
			m_pOutputThread->sync();
		} while (!m_pOutputThread->wait(100));
		delete m_pOutputThread;
		m_pOutputThread = nullptr;
	}

	// Last but not least, delete input thread...
	if (m_pInputThread) {
		// Make it nicely...
		if (m_pInputThread->isRunning()) do {
			m_pInputThread->setRunState(false);
		//	m_pInputThread->terminate();
		} while (!m_pInputThread->wait(100));
		delete m_pInputThread;
		m_pInputThread = nullptr;
	}

	// Time-scale cursor (tempo/time-signature map)
	if (m_pMetroCursor) {
		delete m_pMetroCursor;
		m_pMetroCursor = nullptr;
	}

	// Drop subscription stuff.
	if (m_pAlsaSubsSeq) {
		if (m_pAlsaNotifier) {
			delete m_pAlsaNotifier;
			m_pAlsaNotifier = nullptr;
		}
		if (m_iAlsaSubsPort >= 0) {
			snd_seq_delete_simple_port(m_pAlsaSubsSeq, m_iAlsaSubsPort);
			m_iAlsaSubsPort = -1;
		}
		snd_seq_close(m_pAlsaSubsSeq);
		m_pAlsaSubsSeq = nullptr;
	}

	// Drop everything else, finally.
	if (m_pAlsaSeq) {
		// And now, the sequencer queue and handle...
		snd_seq_free_queue(m_pAlsaSeq, m_iAlsaQueue);
		snd_seq_close(m_pAlsaSeq);
		m_iAlsaQueue  = -1;
		m_iAlsaClient = -1;
		m_pAlsaSeq    = nullptr;
	}

	// And all other timing tracers.
	m_iTimeDrift  = 0;
	m_iFrameDrift = 0;

	m_iTimeStart  = 0;
	m_iFrameStart = 0;

	m_iTimeStartEx = 0;

	m_iAudioFrameStart = 0;
}


// The delta-time/frame accessors.
long qtractorMidiEngine::timeStart (void) const
{
	return m_iTimeStart;
}


// The absolute-time/frame accessors.
unsigned long qtractorMidiEngine::timeStartEx (void) const
{
	return m_iTimeStartEx;
}


// Immediate track mute.
void qtractorMidiEngine::trackMute ( qtractorTrack *pTrack, bool bMute )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::trackMute(%p, %d)", pTrack, bMute);
#endif

	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	const unsigned long iFrame = pSession->playHead();

	if (bMute) {
		// Remove all already enqueued events
		// for the given track and channel...
		snd_seq_remove_events_t *pre;
		snd_seq_remove_events_alloca(&pre);
		snd_seq_timestamp_t ts;
		const unsigned long iTime = pSession->tickFromFrame(iFrame);
		ts.tick = ((long) iTime > m_iTimeStart ? iTime - m_iTimeStart : 0);
		snd_seq_remove_events_set_time(pre, &ts);
		snd_seq_remove_events_set_tag(pre, pTrack->midiTag());
		snd_seq_remove_events_set_channel(pre, pTrack->midiChannel());
		snd_seq_remove_events_set_queue(pre, m_iAlsaQueue);
		snd_seq_remove_events_set_condition(pre, SND_SEQ_REMOVE_OUTPUT
			| SND_SEQ_REMOVE_TIME_AFTER | SND_SEQ_REMOVE_TIME_TICK
			| SND_SEQ_REMOVE_DEST_CHANNEL | SND_SEQ_REMOVE_IGNORE_OFF
			| SND_SEQ_REMOVE_TAG_MATCH);
		snd_seq_remove_events(m_pAlsaSeq, pre);
		// Immediate all current notes off.
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus)
			pMidiBus->setController(pTrack, ALL_NOTES_OFF);
		// Clear/reset track monitor...
		qtractorMidiMonitor *pMidiMonitor
			= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
		if (pMidiMonitor)
			pMidiMonitor->clear();
		// Reset track plugin buffers...
		if ((pTrack->pluginList())->midiManager())
			(pTrack->pluginList())->midiManager()->reset();
		// Done track mute.
	} else {
		// Must redirect to MIDI ouput thread:
		// the immediate re-enqueueing of MIDI events.
		m_pOutputThread->trackSync(pTrack, iFrame);
		// Done track unmute.
	}
}


// Immediate metronome mute.
void qtractorMidiEngine::metroMute ( bool bMute )
{
	if (!m_bMetroEnabled)
		return;

	if (!m_bMetronome)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::metroMute(%d)\n", int(bMute));
#endif

	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	const unsigned long iFrame = pSession->playHead();

	if (bMute) {
		// Remove all already enqueued events
		// for the given track and channel...
		snd_seq_remove_events_t *pre;
 		snd_seq_remove_events_alloca(&pre);
		snd_seq_timestamp_t ts;
		const unsigned long iTime = pSession->tickFromFrame(iFrame);
		ts.tick = ((long) iTime > m_iTimeStart ? iTime - m_iTimeStart : 0);
		snd_seq_remove_events_set_time(pre, &ts);
		snd_seq_remove_events_set_tag(pre, 0xff);
		snd_seq_remove_events_set_channel(pre, m_iMetroChannel);
		snd_seq_remove_events_set_queue(pre, m_iAlsaQueue);
		snd_seq_remove_events_set_condition(pre, SND_SEQ_REMOVE_OUTPUT
			| SND_SEQ_REMOVE_TIME_AFTER | SND_SEQ_REMOVE_TIME_TICK
			| SND_SEQ_REMOVE_DEST_CHANNEL | SND_SEQ_REMOVE_IGNORE_OFF
			| SND_SEQ_REMOVE_TAG_MATCH);
		snd_seq_remove_events(m_pAlsaSeq, pre);
		// Done metronome mute.
	} else {
		// Must redirect to MIDI ouput thread:
		// the immediate re-enqueueing of MIDI events.
		m_pOutputThread->metroSync(iFrame);
		// Done metronome unmute.
	}
}


// Control bus accessors.
void qtractorMidiEngine::setControlBus ( bool bControlBus )
{
	qtractorBus::ConnectList ins, outs;

	if (isActivated() && m_bControlBus && m_pIControlBus && m_pOControlBus) {
		m_pIControlBus->updateConnects(qtractorBus::Input, ins);
		m_pOControlBus->updateConnects(qtractorBus::Output, outs);
	}

	deleteControlBus();

	m_bControlBus = bControlBus;

	createControlBus();

	if (isActivated()) {
		openControlBus();
		if (m_bControlBus && m_pIControlBus && m_pOControlBus) {
			m_pIControlBus->updateConnects(qtractorBus::Input, ins, true);
			m_pOControlBus->updateConnects(qtractorBus::Output, outs, true);
		}
	}
}

bool qtractorMidiEngine::isControlBus (void) const
{
	return m_bControlBus;
}

void qtractorMidiEngine::resetControlBus (void)
{
	if (m_bControlBus && m_pOControlBus)
		return;

	createControlBus();
}


// Control bus simple management.
void qtractorMidiEngine::createControlBus (void)
{
	deleteControlBus();

	// Whether control bus is here owned, or...
	if (m_bControlBus) {
		m_pOControlBus = new qtractorMidiBus(this, "Control",
			qtractorBus::BusMode(qtractorBus::Duplex | qtractorBus::Ex));
		m_pIControlBus = m_pOControlBus;
	} else {
		// Find available control buses...
		for (qtractorBus *pBus = qtractorEngine::buses().first();
				pBus; pBus = pBus->next()) {
			if (m_pIControlBus == nullptr
				&& (pBus->busMode() & qtractorBus::Input))
				m_pIControlBus = static_cast<qtractorMidiBus *> (pBus);
			if (m_pOControlBus == nullptr
				&& (pBus->busMode() & qtractorBus::Output))
				m_pOControlBus = static_cast<qtractorMidiBus *> (pBus);
		}
	}
}

// Open MIDI control stuff...
bool qtractorMidiEngine::openControlBus (void)
{
	closeControlBus();

	// Is there any?
	if (m_pOControlBus == nullptr)
		createControlBus();
	if (m_pOControlBus == nullptr)
		return false;

	// This is it, when dedicated...
	if (m_bControlBus) {
		addBusEx(m_pOControlBus);
		m_pOControlBus->open();
	}

	return true;
}


// Close MIDI control stuff.
void qtractorMidiEngine::closeControlBus (void)
{
	if (m_pOControlBus && m_bControlBus) {
		m_pOControlBus->close();
		removeBusEx(m_pOControlBus);
	}
}


// Destroy MIDI control stuff.
void qtractorMidiEngine::deleteControlBus (void)
{
	closeControlBus();

	// When owned, both input and output
	// bus are the one and the same...
	if (m_pOControlBus && m_bControlBus)
		delete m_pOControlBus;

	// Reset both control buses...
	m_pIControlBus = nullptr;
	m_pOControlBus = nullptr;
}


// Control buses accessors.
qtractorMidiBus *qtractorMidiEngine::controlBus_in() const
{
	return m_pIControlBus;
}

qtractorMidiBus *qtractorMidiEngine::controlBus_out() const
{
	return m_pOControlBus;
}


// Player bus accessors.
void qtractorMidiEngine::setPlayerBus ( bool bPlayerBus )
{
	qtractorBus::ConnectList outs;

	if (isActivated() && m_bPlayerBus && m_pPlayerBus)
		m_pPlayerBus->updateConnects(qtractorBus::Output, outs);

	deletePlayerBus();

	m_bPlayerBus = bPlayerBus;

	createPlayerBus();

	if (isActivated()) {
		openPlayerBus();
		if (m_bPlayerBus && m_pPlayerBus)
			m_pPlayerBus->updateConnects(qtractorBus::Output, outs, true);
	}
}

bool qtractorMidiEngine::isPlayerBus (void) const
{
	return m_bPlayerBus;
}


// Player bus simple management.
void qtractorMidiEngine::createPlayerBus (void)
{
	deletePlayerBus();

	// Whether metronome bus is here owned, or...
	if (m_bPlayerBus) {
		m_pPlayerBus = new qtractorMidiBus(this, "Player",
			qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex));
	} else {
		// Find first available output buses...
		QListIterator<qtractorBus *> iter(buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
			if (pBus->busMode() & qtractorBus::Output) {
				m_pPlayerBus = static_cast<qtractorMidiBus *> (pBus);
				break;
			}
		}
	}
}


// Open MIDI player stuff...
bool qtractorMidiEngine::openPlayerBus (void)
{
	closePlayerBus();

	// Is there any?
	if (m_pPlayerBus == nullptr)
		createPlayerBus();
	if (m_pPlayerBus == nullptr)
		return false;

	// This is it, when dedicated...
	if (m_bPlayerBus) {
		addBusEx(m_pPlayerBus);
		m_pPlayerBus->open();
	}

	// Time to create our player...
	m_pPlayer = new qtractorMidiPlayer(m_pPlayerBus);

	return true;
}


// Close MIDI player stuff.
void qtractorMidiEngine::closePlayerBus (void)
{
	if (m_pPlayer)
		m_pPlayer->close();

	if (m_pPlayerBus && m_bPlayerBus) {
		m_pPlayerBus->close();
		removeBusEx(m_pPlayerBus);
	}
}


// Destroy MIDI player stuff.
void qtractorMidiEngine::deletePlayerBus (void)
{
	closePlayerBus();

	if (m_pPlayer) {
		delete m_pPlayer;
		m_pPlayer = nullptr;
	}

	if (m_pPlayerBus && m_bPlayerBus)
		delete m_pPlayerBus;

	m_pPlayerBus = nullptr;
}


// Tell whether audition/pre-listening is active...
bool qtractorMidiEngine::isPlayerOpen (void) const
{
	return (m_pPlayer ? m_pPlayer->isOpen() : false);
}


// Open and start audition/pre-listening...
bool qtractorMidiEngine::openPlayer (
	const QString& sFilename, int iTrackChannel )
{
	if (isPlaying())
		return false;

	m_iFrameStart = 0;
	m_iTimeStart  = 0;

	m_iTimeStartEx = m_iTimeStart;

	return (m_pPlayer ? m_pPlayer->open(sFilename, iTrackChannel) : false);
}


// Stop and close audition/pre-listening...
void qtractorMidiEngine::closePlayer (void)
{
	if (m_pPlayer) m_pPlayer->close();
}


// MMC dispatch special commands.
void qtractorMidiEngine::sendMmcLocate ( unsigned long iLocate ) const
{
	unsigned char data[6];

	data[0] = 0x01;
	data[1] = iLocate / (3600 * 30); iLocate -= (3600 * 30) * (int) data[1];
	data[2] = iLocate / (  60 * 30); iLocate -= (  60 * 30) * (int) data[2];
	data[3] = iLocate / (       30); iLocate -= (       30) * (int) data[3];
	data[4] = iLocate;
	data[5] = 0;

	sendMmcCommand(qtractorMmcEvent::LOCATE, data, sizeof(data));
}


void qtractorMidiEngine::sendMmcMaskedWrite (
	qtractorMmcEvent::SubCommand scmd, int iTrack, bool bOn ) const
{
	unsigned char data[4];
	const int iMask = (1 << (iTrack < 2 ? iTrack + 5 : (iTrack - 2) % 7));

	data[0] = scmd;
	data[1] = (unsigned char) (iTrack < 2 ? 0 : 1 + (iTrack - 2) / 7);
	data[2] = (unsigned char) iMask;
	data[3] = (unsigned char) (bOn ? iMask : 0);

	sendMmcCommand(qtractorMmcEvent::MASKED_WRITE, data, sizeof(data));
}


void qtractorMidiEngine::sendMmcCommand (
	qtractorMmcEvent::Command cmd,
	unsigned char *pMmcData, unsigned short iMmcData ) const
{
	// Do we have MMC output enabled?
	if ((m_mmcMode & qtractorBus::Output) == 0)
		return;

	// We surely need a output control bus...
	if (m_pOControlBus == nullptr)
		return;

	// Build up the MMC sysex message...
	unsigned char *pSysex;
	unsigned short iSysex;

	iSysex = 6;
	if (pMmcData && iMmcData > 0)
		iSysex += 1 + iMmcData;
	pSysex = new unsigned char [iSysex];
	iSysex = 0;

	pSysex[iSysex++] = 0xf0;				// Sysex header.
	pSysex[iSysex++] = 0x7f;				// Realtime sysex.
	pSysex[iSysex++] = m_mmcDevice;			// MMC device id.
	pSysex[iSysex++] = 0x06;				// MMC command mode.
	pSysex[iSysex++] = (unsigned char) cmd;	// MMC command code.
	if (pMmcData && iMmcData > 0) {
		pSysex[iSysex++] = iMmcData;
		::memcpy(&pSysex[iSysex], pMmcData, iMmcData);
		iSysex += iMmcData;
	}
	pSysex[iSysex++] = 0xf7;				// Sysex trailer.

	// Send it out, now.
	m_pOControlBus->sendSysex(pSysex, iSysex);

	// Done.
	delete [] pSysex;
}


// SPP dispatch special command.
void qtractorMidiEngine::sendSppCommand (
	int iCmdType, unsigned int iSongPos ) const
{
	// Do we have SPP output enabled?
	if ((m_sppMode & qtractorBus::Output) == 0)
		return;

	// We surely need a output control bus...
	if (m_pOControlBus == nullptr)
		return;

	// Initialize sequencer event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Addressing...
	snd_seq_ev_set_source(&ev, m_pOControlBus->alsaPort());
	snd_seq_ev_set_subs(&ev);

	// The event will be direct...
	snd_seq_ev_set_direct(&ev);

	// Set command parameters...
	// - SND_SEQ_EVENT_START
	// - SND_SEQ_EVENT_STOP
	// - SND_SEQ_EVENT_CONTINUE
	// - SND_SEQ_EVENT_SONGPOS
	ev.type = snd_seq_event_type(iCmdType);
	ev.data.control.value = iSongPos;

	// Bail out...
	snd_seq_event_output_direct(m_pAlsaSeq, &ev);
}


// Metronome switching.
void qtractorMidiEngine::setMetronome ( bool bMetronome )
{
	m_bMetronome = bMetronome;

	if (isPlaying())
		metroMute(!m_bMetronome);
}

bool qtractorMidiEngine::isMetronome (void) const
{
	return m_bMetronome;
}


// Metronome enabled accessors.
void qtractorMidiEngine::setMetroEnabled ( bool bMetroEnabled )
{
	m_bMetroEnabled = bMetroEnabled;
}

bool qtractorMidiEngine::isMetroEnabled (void) const
{
	return m_bMetroEnabled;
}


// Metronome bus accessors.
void qtractorMidiEngine::setMetroBus ( bool bMetroBus )
{
	qtractorBus::ConnectList outs;

	if (isActivated() && m_bMetroBus && m_pMetroBus)
		m_pMetroBus->updateConnects(qtractorBus::Output, outs);

	deleteMetroBus();

	m_bMetroBus = bMetroBus;

	createMetroBus();

	if (isActivated()) {
		openMetroBus();
		if (m_bMetroBus && m_pMetroBus)
			m_pMetroBus->updateConnects(qtractorBus::Output, outs, true);
	}
}

bool qtractorMidiEngine::isMetroBus (void) const
{
	return m_bMetroBus;
}

void qtractorMidiEngine::resetMetroBus (void)
{
	if (m_bMetroBus && m_pMetroBus)
		return;

	createMetroBus();
}


// Metronome bus simple management.
void qtractorMidiEngine::createMetroBus (void)
{
	deleteMetroBus();

	if (!m_bMetroEnabled)
		return;

	// Whether metronome bus is here owned, or...
	if (m_bMetroBus) {
		m_pMetroBus = new qtractorMidiBus(this, "Metronome",
			qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex));
	} else {
		// Find first available output buses...
		QListIterator<qtractorBus *> iter(buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
			if (pBus->busMode() & qtractorBus::Output) {
				m_pMetroBus = static_cast<qtractorMidiBus *> (pBus);
				break;
			}
		}
	}
}


// Open MIDI metronome stuff...
bool qtractorMidiEngine::openMetroBus (void)
{
	closeMetroBus();

	if (!m_bMetroEnabled)
		return false;

	// Is there any?
	if (m_pMetroBus == nullptr)
		createMetroBus();
	if (m_pMetroBus == nullptr)
		return false;

	// This is it, when dedicated...
	if (m_bMetroBus) {
		addBusEx(m_pMetroBus);
		m_pMetroBus->open();
	}

	return true;
}


// Close MIDI metronome stuff.
void qtractorMidiEngine::closeMetroBus (void)
{
	if (m_pMetroBus && m_bMetroBus) {
		m_pMetroBus->close();
		removeBusEx(m_pMetroBus);
	}
}


// Destroy MIDI metronome stuff.
void qtractorMidiEngine::deleteMetroBus (void)
{
	closeMetroBus();

	if (m_pMetroBus && m_bMetroBus)
		delete m_pMetroBus;

	m_pMetroBus = nullptr;
}


// Metronome channel accessors.
void qtractorMidiEngine::setMetroChannel ( unsigned short iChannel )
{
	m_iMetroChannel = iChannel;
}

unsigned short qtractorMidiEngine::metroChannel (void) const
{
	return m_iMetroChannel;
}

// Metronome bar parameters.
void qtractorMidiEngine::setMetroBar (
	int iNote, int iVelocity, unsigned long iDuration )
{
	m_iMetroBarNote     = iNote;
	m_iMetroBarVelocity = iVelocity;
	m_iMetroBarDuration = iDuration;
}

int qtractorMidiEngine::metroBarNote (void) const
{
	return m_iMetroBarNote;
}

int qtractorMidiEngine::metroBarVelocity (void) const
{
	return m_iMetroBarVelocity;
}

unsigned long qtractorMidiEngine::metroBarDuration (void) const
{
	return m_iMetroBarDuration;
}


// Metronome bar parameters.
void qtractorMidiEngine::setMetroBeat (
	int iNote, int iVelocity, unsigned long iDuration )
{
	m_iMetroBeatNote     = iNote;
	m_iMetroBeatVelocity = iVelocity;
	m_iMetroBeatDuration = iDuration;
}

int qtractorMidiEngine::metroBeatNote (void) const
{
	return m_iMetroBarNote;
}

int qtractorMidiEngine::metroBeatVelocity (void) const
{
	return m_iMetroBarVelocity;
}

unsigned long qtractorMidiEngine::metroBeatDuration (void) const
{
	return m_iMetroBeatDuration;
}


// Metronome latency offset (in ticks).
void qtractorMidiEngine::setMetroOffset ( unsigned long iMetroOffset )
{
	m_iMetroOffset = iMetroOffset;
}

unsigned long qtractorMidiEngine::metroOffset (void) const
{
	return m_iMetroOffset;
}


// Process metronome clicks.
void qtractorMidiEngine::processMetro (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (m_pMetroCursor == nullptr)
		return;

	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	qtractorTimeScale::Node *pNode = m_pMetroCursor->seekFrame(iFrameEnd);

	// Take this moment to check for tempo changes...
	if (pNode->tempo != m_fMetroTempo) {
		// New tempo node...
		const unsigned long iTime = (pNode->frame < iFrameStart
			? pNode->tickFromFrame(iFrameStart) : pNode->tick);
		// Enqueue tempo event...
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		// Scheduled delivery: take into account
		// the time playback/queue started...
		const unsigned long tick
			= (long(iTime) > m_iTimeStart ? iTime - m_iTimeStart : 0);
		snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, pSession->timep(tick));
		ev.type = SND_SEQ_EVENT_TEMPO;
		ev.data.queue.queue = m_iAlsaQueue;
		ev.data.queue.param.value
			= (unsigned int) (60000000.0f / pNode->tempo);
		ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
		ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
		// Pump it into the queue.
		snd_seq_event_output(m_pAlsaSeq, &ev);
		// Save for next change.
		m_fMetroTempo = pNode->tempo;
		// Update MIDI monitor slot stuff...
		qtractorMidiMonitor::splitTime(
			m_pMetroCursor->timeScale(), pNode->frame, tick);
	}

	// Get on with the actual metronome/count-in/clock stuff...
	if (!m_bMetronome && (m_clockMode & qtractorBus::Output) == 0)
		return;

	// Register the next metronome/clock beat slot.
	const unsigned long iTimeEnd = pNode->tickFromFrame(iFrameEnd);

	pNode = m_pMetroCursor->seekFrame(iFrameStart);
	const unsigned long iTimeStart = pNode->tickFromFrame(iFrameStart);
	unsigned int  iBeat = pNode->beatFromTick(iTimeStart);
	unsigned long iTime = pNode->tickFromBeat(iBeat);

	// Initialize outbound metronome event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	// Addressing...
	if (m_pMetroBus) {
		snd_seq_ev_set_source(&ev, m_pMetroBus->alsaPort());
		snd_seq_ev_set_subs(&ev);
	}
	// Set common event data...
	ev.tag = (unsigned char) 0xff;
	ev.type = SND_SEQ_EVENT_NOTE;
	ev.data.note.channel = m_iMetroChannel;

	// Initialize outbound clock event...
	snd_seq_event_t ev_clock;
	snd_seq_ev_clear(&ev_clock);
	// Addressing...
	if (m_pOControlBus) {
		snd_seq_ev_set_source(&ev_clock, m_pOControlBus->alsaPort());
		snd_seq_ev_set_subs(&ev_clock);
	}
	// Set common event data...
	ev_clock.tag = (unsigned char) 0xff;
	ev_clock.type = SND_SEQ_EVENT_CLOCK;

	while (iTime < iTimeEnd) {
		// Scheduled delivery: take into account
		// the time playback/queue started...
		if (m_clockMode & qtractorBus::Output) {
			unsigned long iTimeClock = iTime;
			const unsigned int iTicksPerClock = pNode->ticksPerBeat / 24;
			for (unsigned int iClock = 0; iClock < 24; ++iClock) {
				if (iTimeClock >= iTimeEnd)
					break;
				if (iTimeClock >= iTimeStart) {
					const unsigned long tick
						= (long(iTimeClock) > m_iTimeStart ? iTimeClock - m_iTimeStart : 0);
					snd_seq_ev_schedule_tick(&ev_clock, m_iAlsaQueue, 0, pSession->timep(tick));
					snd_seq_event_output(m_pAlsaSeq, &ev_clock);
				}
				iTimeClock += iTicksPerClock;
			}
		}
		if (m_bMetronome && iTime >= iTimeStart) {
			// Have some latency compensation...
			const unsigned long iTimeOffset = (m_iMetroOffset > 0
				&& iTime > m_iMetroOffset ? iTime - m_iMetroOffset : iTime);
			// Set proper event schedule time...
			const unsigned long tick
				= (long(iTimeOffset) > m_iTimeStart ? iTimeOffset - m_iTimeStart : 0);
			snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, pSession->timep(tick));
			// Set proper event data...
			if (pNode->beatIsBar(iBeat)) {
				ev.data.note.note     = m_iMetroBarNote;
				ev.data.note.velocity = m_iMetroBarVelocity;
				ev.data.note.duration = m_iMetroBarDuration;
			} else {
				ev.data.note.note     = m_iMetroBeatNote;
				ev.data.note.velocity = m_iMetroBeatVelocity;
				ev.data.note.duration = m_iMetroBeatDuration;
			}
			// Pump it into the queue.
			snd_seq_event_output(m_pAlsaSeq, &ev);
			// MIDI track monitoring...
			if (m_pMetroBus && m_pMetroBus->midiMonitor_out()) {
				m_pMetroBus->midiMonitor_out()->enqueue(
					qtractorMidiEvent::NOTEON, ev.data.note.velocity, tick);
			}
		}
		// Go for next beat...
		iTime += pNode->ticksPerBeat;
		pNode = m_pMetroCursor->seekBeat(++iBeat);
	}
}


// Access to current tempo/time-signature cursor.
qtractorTimeScale::Cursor *qtractorMidiEngine::metroCursor (void) const
{
	return m_pMetroCursor;
}


// Metronome count-in switching.
void qtractorMidiEngine::setCountIn ( bool bCountIn )
{
	m_bCountIn = bCountIn;
}

bool qtractorMidiEngine::isCountIn (void) const
{
	return m_bCountIn;
}


// Metronome count-in mode.
void qtractorMidiEngine::setCountInMode ( CountInMode countInMode )
{
	m_countInMode = countInMode;
}

qtractorMidiEngine::CountInMode qtractorMidiEngine::countInMode (void) const
{
	return m_countInMode;
}


// Metronome count-in number of beats.
void qtractorMidiEngine::setCountInBeats ( unsigned short iCountInBeats )
{
	m_iCountInBeats = iCountInBeats;
}

unsigned short qtractorMidiEngine::countInBeats (void) const
{
	return m_iCountInBeats;
}


// Metronome count-in status.
unsigned short qtractorMidiEngine::countIn ( unsigned int nframes )
{
	if (m_iCountIn > 0) {
		m_iCountInFrame += nframes;
		if (m_iCountInFrame >= m_iCountInFrameEnd) {
			m_iCountIn = 0;
			resetTime();
		} else {
			sync();
		}
	}

	return m_iCountIn;
}

unsigned short qtractorMidiEngine::countIn (void) const
{
	return m_iCountIn;
}


// Process metronome count-ins.
void qtractorMidiEngine::processCountIn (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (m_pMetroCursor == nullptr)
		return;

	// Get on with the actual metronome/count-in stuff...
	if (m_iCountIn < 1)
		return;

	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return;

	// Register the next metronome/clock beat slot.
	qtractorTimeScale::Node *pNode = m_pMetroCursor->seekFrame(iFrameEnd);
	const unsigned long iTimeEnd = pNode->tickFromFrame(iFrameEnd);
	pNode = m_pMetroCursor->seekFrame(iFrameStart);
	const unsigned long iTimeStart = pNode->tickFromFrame(iFrameStart);
	unsigned int  iBeat = pNode->beatFromTick(iTimeStart);
	unsigned long iTime = pNode->tickFromBeat(iBeat);

	// Initialize outbound metronome event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	// Addressing...
	if (m_pMetroBus) {
		snd_seq_ev_set_source(&ev, m_pMetroBus->alsaPort());
		snd_seq_ev_set_subs(&ev);
	}
	// Set common event data...
	ev.tag = (unsigned char) 0xff;
	ev.type = SND_SEQ_EVENT_NOTE;
	ev.data.note.channel = m_iMetroChannel;

	while (iTime < iTimeEnd) {
		// Scheduled delivery: take into account
		// the time playback/queue started...
		if (iTime >= iTimeStart) {
			// Have some latency compensation...
			const unsigned long iTimeOffset = (m_iMetroOffset > 0
				&& iTime > m_iMetroOffset ? iTime - m_iMetroOffset : iTime);
			// Set proper event schedule time...
			const unsigned long tick
				= (long(iTimeOffset) > m_iCountInTimeStart
				? iTimeOffset - m_iCountInTimeStart : 0);
			snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, pSession->timep(tick));
			// Set proper event data...
			if (pNode->beatIsBar(iBeat)) {
				ev.data.note.note     = m_iMetroBarNote;
				ev.data.note.velocity = m_iMetroBarVelocity;
				ev.data.note.duration = m_iMetroBarDuration;
			} else {
				ev.data.note.note     = m_iMetroBeatNote;
				ev.data.note.velocity = m_iMetroBeatVelocity;
				ev.data.note.duration = m_iMetroBeatDuration;
			}
			// Pump it into the queue.
			snd_seq_event_output(m_pAlsaSeq, &ev);
			// MIDI track monitoring...
			if (m_pMetroBus && m_pMetroBus->midiMonitor_out()) {
				m_pMetroBus->midiMonitor_out()->enqueue(
					qtractorMidiEvent::NOTEON, ev.data.note.velocity, tick);
			}
		//	--m_iCountIn; // Not really useful?
		}
		// Go for next beat...
		iTime += pNode->ticksPerBeat;
		pNode = m_pMetroCursor->seekBeat(++iBeat);
	}

	snd_seq_drain_output(m_pAlsaSeq);
}


// Document element methods.
bool qtractorMidiEngine::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	qtractorEngine::clear();

	createControlBus();
	createMetroBus();

	QStringList midi_buses2;

	// Load session children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		if (eChild.tagName() == "midi-control") {
			for (QDomNode nProp = eChild.firstChild();
					!nProp.isNull(); nProp = nProp.nextSibling()) {
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				if (eProp.tagName() == "mmc-mode") {
					qtractorMidiEngine::setMmcMode(
						qtractorBus::busModeFromText(eProp.text()));
				}
				else if (eProp.tagName() == "mmc-device") {
					qtractorMidiEngine::setMmcDevice(
						eProp.text().toInt() & 0x7f);
				}
				else if (eProp.tagName() == "spp-mode") {
					qtractorMidiEngine::setSppMode(
						qtractorBus::busModeFromText(eProp.text()));
				}
				else if (eProp.tagName() == "clock-mode") {
					qtractorMidiEngine::setClockMode(
						qtractorBus::busModeFromText(eProp.text()));
				}
			}
		}
		else if (eChild.tagName() == "midi-bus") {
			QString sBusName = eChild.attribute("name");
			qtractorMidiBus::BusMode busMode
				= qtractorBus::busModeFromText(eChild.attribute("mode"));
			qtractorMidiBus *pMidiBus
				= new qtractorMidiBus(this, sBusName, busMode);
			if (!pMidiBus->loadElement(pDocument, &eChild))
				return false;
			qtractorMidiEngine::addBus(pMidiBus);
		}
		else if (eChild.tagName() == "control-inputs") {
			if (m_bControlBus && m_pIControlBus) {
				m_pIControlBus->loadConnects(
					m_pIControlBus->inputs(), pDocument, &eChild);
			}
		}
		else if (eChild.tagName() == "control-outputs") {
			if (m_bControlBus && m_pOControlBus) {
				m_pOControlBus->loadConnects(
					m_pOControlBus->outputs(), pDocument, &eChild);
			}
		}
		else if (eChild.tagName() == "metronome-outputs") {
			if (m_bMetroBus && m_pMetroBus) {
				m_pMetroBus->loadConnects(
					m_pMetroBus->outputs(), pDocument, &eChild);
			}
		}
		else if (eChild.tagName() == "midi-buses2") {
			midi_buses2 = qtractorEngine::loadBuses2List(
				pDocument, &eChild, "midi-bus2");
		}
	}

	qtractorEngine::setBuses2List(midi_buses2);

	return true;
}


bool qtractorMidiEngine::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	// Save transport/control modes...
	QDomElement eControl
		= pDocument->document()->createElement("midi-control");
	pDocument->saveTextElement("mmc-mode",
		qtractorBus::textFromBusMode(
			qtractorMidiEngine::mmcMode()), &eControl);
	pDocument->saveTextElement("mmc-device",
		QString::number(int(qtractorMidiEngine::mmcDevice())), &eControl);
	pDocument->saveTextElement("spp-mode",
		qtractorBus::textFromBusMode(
			qtractorMidiEngine::sppMode()), &eControl);
	pDocument->saveTextElement("clock-mode",
		qtractorBus::textFromBusMode(
			qtractorMidiEngine::clockMode()), &eControl);
	pElement->appendChild(eControl);

	// Save MIDI buses...
	QListIterator<qtractorBus *> iter(qtractorEngine::buses2());
	while (iter.hasNext()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (iter.next());
		if (pMidiBus) {
			// Create the new MIDI bus element...
			QDomElement eMidiBus
				= pDocument->document()->createElement("midi-bus");
			pMidiBus->saveElement(pDocument, &eMidiBus);
			pElement->appendChild(eMidiBus);
		}
	}

	// Control bus (input) connects...
	if (m_bControlBus && m_pIControlBus) {
		QDomElement eInputs
			= pDocument->document()->createElement("control-inputs");
		qtractorBus::ConnectList inputs;
		m_pIControlBus->updateConnects(qtractorBus::Input, inputs);
		m_pIControlBus->saveConnects(inputs, pDocument, &eInputs);
		pElement->appendChild(eInputs);
	}

	// Control bus (output) connects...
	if (m_bControlBus && m_pOControlBus) {
		QDomElement eOutputs
			= pDocument->document()->createElement("control-outputs");
		qtractorBus::ConnectList outputs;
		m_pOControlBus->updateConnects(qtractorBus::Output, outputs);
		m_pOControlBus->saveConnects(outputs, pDocument, &eOutputs);
		pElement->appendChild(eOutputs);
	}

	// Metronome bus connects...
	if (m_bMetroBus && m_pMetroBus) {
		QDomElement eOutputs
			= pDocument->document()->createElement("metronome-outputs");
		qtractorBus::ConnectList outputs;
		m_pMetroBus->updateConnects(qtractorBus::Output, outputs);
		m_pMetroBus->saveConnects(outputs, pDocument, &eOutputs);
		pElement->appendChild(eOutputs);
	}

	const QStringList& midi_buses2 = qtractorEngine::buses2List();
	if (!midi_buses2.isEmpty()) {
		QDomElement eBuses2
			= pDocument->document()->createElement("midi-buses2");
		saveBuses2List(pDocument, &eBuses2, "midi-bus2", midi_buses2);
		pElement->appendChild(eBuses2);
	}

	return true;
}


// MIDI-export method.
bool qtractorMidiEngine::fileExport (
	const QString& sExportPath, const QList<qtractorMidiBus *>& exportBuses,
	unsigned long iExportStart, unsigned long iExportEnd, int iExportFormat )
{
	// No simultaneous or foul exports...
	if (isPlaying())
		return false;

	// Make sure we have an actual session cursor...
	qtractorSession *pSession = session();
	if (pSession == nullptr)
		return false;

	// Cannot have exports longer than current session.
	if (iExportStart >= iExportEnd)
		iExportEnd = pSession->sessionEnd();
	if (iExportStart >= iExportEnd)
		return false;

	const unsigned short iTicksPerBeat
		= pSession->ticksPerBeat();

	const unsigned long iTimeStart
		= pSession->tickFromFrame(iExportStart);
	const unsigned long iTimeEnd
		= pSession->tickFromFrame(iExportEnd);

	const unsigned short iFormat
		= (iExportFormat < 0
		? qtractorMidiClip::defaultFormat()
		: iExportFormat);

	unsigned short iSeq;
	unsigned short iSeqs = 0;
	QList<qtractorMidiSequence *> seqs;
	qtractorMidiSequence **ppSeqs = nullptr;
	if (iFormat == 0) {
		iSeqs  = 16;
		ppSeqs = new qtractorMidiSequence * [iSeqs];
		for (iSeq = 0; iSeq < iSeqs; ++iSeq) {
			ppSeqs[iSeq] = new qtractorMidiSequence(
				QString(), iSeq, iTicksPerBeat);
		}
	}

	// Do the real grunt work, get eaach elligigle track
	// and copy the events in range to be written out...
	QListIterator<qtractorMidiBus *> bus_iter(exportBuses);

	unsigned short iTracks = 0;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() != qtractorTrack::Midi)
			continue;
		if (pTrack->isMute() || (pSession->soloTracks() && !pTrack->isSolo()))
			continue;
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pTrack->outputBus());
		if (pMidiBus == nullptr)
			continue;
		// Check whether this track makes it
		// as one of the exported buses....
		qtractorMidiBus *pExportBus = nullptr;
		bus_iter.toFront();
		while (bus_iter.hasNext()) {
			pExportBus = bus_iter.next();
			if (pExportBus
				&& pExportBus->alsaPort() == pMidiBus->alsaPort())
				break;
			pExportBus = nullptr;
		}
		// Is it not?
		if (pExportBus == nullptr)
			continue;
		// We have a target sequence, maybe reused...
		qtractorMidiSequence *pSeq;
		if (ppSeqs) {
			// SMF Format 0
			pSeq = ppSeqs[pTrack->midiChannel() & 0x0f];
			QString sName = pSeq->name();
			if (!sName.isEmpty())
				sName += "; ";
			pSeq->setName(sName + pTrack->shortTrackName());
		} else {
			// SMF Format 1
			++iTracks;
			pSeq = new qtractorMidiSequence(
				pTrack->shortTrackName(), iTracks, iTicksPerBeat);
			pSeq->setChannel(pTrack->midiChannel());
			seqs.append(pSeq);
		}
		// Make this track setup...
		if (pSeq->bankSelMethod() < 0)
			pSeq->setBankSelMethod(pTrack->midiBankSelMethod());
		if (pSeq->bank() < 0)
			pSeq->setBank(pTrack->midiBank());
		if (pSeq->prog() < 0)
			pSeq->setProg(pTrack->midiProg());
		// Now, for every clip...
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip && pClip->clipStart()
			+ pClip->clipLength() < iExportStart)
			pClip = pClip->next();
		while (pClip && pClip->clipStart() < iExportEnd) {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				const unsigned long iTimeClip
					= pSession->tickFromFrame(pClip->clipStart());
				const unsigned long iTimeOffset
					= iTimeClip - iTimeStart;
				const float fGain = pMidiClip->clipGain();
				// For each event...
				qtractorMidiEvent *pEvent
					= pMidiClip->sequence()->events().first();
				while (pEvent && iTimeClip + pEvent->time() < iTimeStart)
					pEvent = pEvent->next();
				while (pEvent && iTimeClip + pEvent->time() < iTimeEnd) {
					qtractorMidiEvent *pNewEvent
						= new qtractorMidiEvent(*pEvent);
					pNewEvent->setTime(iTimeOffset + pEvent->time());
					if (pNewEvent->type() == qtractorMidiEvent::NOTEON) {
						const unsigned long iTimeEvent
							= iTimeClip + pEvent->time();
						const float fVolume = fGain
							* pMidiClip->fadeInOutGain(
								pSession->frameFromTick(iTimeEvent)
								- pClip->clipStart());
						pNewEvent->setVelocity((unsigned char)
							(fVolume * float(pEvent->velocity())) & 0x7f);
						if (iTimeEvent + pEvent->duration() > iTimeEnd)
							pNewEvent->setDuration(iTimeEnd - iTimeEvent);
					}
					pSeq->insertEvent(pNewEvent);
					pEvent = pEvent->next();
				}
			}
			pClip = pClip->next();
		}
		// Have a break...
		qtractorSession::stabilize();
	}

	// Account for the only or META info track...
	++iTracks;

	// Special on SMF Format 1...
	if (ppSeqs == nullptr) {
		// Sanity check...
		if (iTracks < 1)
			return false;
		// Number of actual track sequences...
		iSeqs  = iTracks;
		ppSeqs = new qtractorMidiSequence * [iSeqs];
		QListIterator<qtractorMidiSequence *> seq_iter(seqs);
		ppSeqs[0] = nullptr;	// META info track...
		for (iSeq = 1; iSeq < iSeqs && seq_iter.hasNext(); ++iSeq)
			ppSeqs[iSeq] = seq_iter.next();
		// May clear it now.
		seqs.clear();
	}

	// Prepare file for writing...
	qtractorMidiFile file;
	// File ready for export?
	const bool bResult
		= file.open(sExportPath, qtractorMidiFile::Write);
	if (bResult) {
		if (file.writeHeader(iFormat, iTracks, iTicksPerBeat)) {
			// Export SysEx setups...
			bus_iter.toFront();
			while (bus_iter.hasNext()) {
				qtractorMidiBus *pExportBus = bus_iter.next();
				qtractorMidiSysexList *pSysexList = pExportBus->sysexList();
				if (pSysexList && pSysexList->count() > 0) {
					if (ppSeqs[0] == nullptr) {
						ppSeqs[0] = new qtractorMidiSequence(
							QFileInfo(sExportPath).baseName(), 0, iTicksPerBeat);
					}
					pExportBus->exportSysexList(ppSeqs[0]);
				}
			}
			// Export tempo map as well...	
			if (file.tempoMap()) {
				file.tempoMap()->fromTimeScale(
					pSession->timeScale(), iTimeStart);
			}
			file.writeTracks(ppSeqs, iSeqs);
		}
		file.close();
	}

	// Free locally allocated track/sequence array.
	for (iSeq = 0; iSeq < iSeqs; ++iSeq) {
		if (ppSeqs[iSeq])
			delete ppSeqs[iSeq];
	}
	delete [] ppSeqs;

	// Done successfully.
	return bResult;
}


// Retrieve/restore all connections, on all MIDI buses.
// return the total number of effective (re)connection attempts...
int qtractorMidiEngine::updateConnects (void)
{
	// Do it as usual, on all standard owned dependable buses...
	const int iUpdate = qtractorEngine::updateConnects();

	// Reset all pending controllers, if any...
	if (m_iResetAllControllersPending > 0)
		resetAllControllers(true); // Force immediate!

	// Done.
	return iUpdate;
}


// Capture/input (record) quantization accessors.
// (value in snap-per-beat units)
void qtractorMidiEngine::setCaptureQuantize ( unsigned short iCaptureQuantize )
{
	m_iCaptureQuantize = iCaptureQuantize;
}

unsigned short qtractorMidiEngine::captureQuantize (void) const
{
	return m_iCaptureQuantize;
}


// ALSA device queue timer.
void qtractorMidiEngine::setAlsaTimer ( int iAlsaTimer )
{
	m_iAlsaTimer = iAlsaTimer;
}

int qtractorMidiEngine::alsaTimer (void) const
{
	return m_iAlsaTimer;
}


// Drift check/correction accessors.
void qtractorMidiEngine::setDriftCorrect ( bool bDriftCorrect )
{
	m_bDriftCorrect = bDriftCorrect;
}

bool qtractorMidiEngine::isDriftCorrect (void) const
{
	return m_bDriftCorrect;
}


// MMC device-id accessors.
void qtractorMidiEngine::setMmcDevice ( unsigned char mmcDevice )
{
	m_mmcDevice = mmcDevice;
}

unsigned char qtractorMidiEngine::mmcDevice (void) const
{
	return m_mmcDevice;
}


// MMC mode accessors.
void qtractorMidiEngine::setMmcMode ( qtractorBus::BusMode mmcMode )
{
	m_mmcMode = mmcMode;
}

qtractorBus::BusMode qtractorMidiEngine::mmcMode (void) const
{
	return m_mmcMode;
}


// SPP mode accessors.
void qtractorMidiEngine::setSppMode ( qtractorBus::BusMode sppMode )
{
	m_sppMode = sppMode;
}

qtractorBus::BusMode qtractorMidiEngine::sppMode (void) const
{
	return m_sppMode;
}


// MIDI Clock mode accessors.
void qtractorMidiEngine::setClockMode ( qtractorBus::BusMode clockMode )
{
	m_clockMode = clockMode;
}

qtractorBus::BusMode qtractorMidiEngine::clockMode (void) const
{
	return m_clockMode;
}


// Whether to reset all MIDI controllers (on playback start).
void qtractorMidiEngine::setResetAllControllers ( bool bResetAllControllers )
{
	m_bResetAllControllers = bResetAllControllers;
}

bool qtractorMidiEngine::isResetAllControllers (void) const
{
	return m_bResetAllControllers;
}


// Process pending step-input events...
void qtractorMidiEngine::processInpEvents (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	QMutexLocker locker(&m_inpMutex);

	InpEvents::ConstIterator iter = m_inpEvents.constBegin();
	const InpEvents::ConstIterator& iter_end = m_inpEvents.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiClip *pMidiClip = iter.key();
		qtractorMidiEditCommand *pMidiEditCommand
			= new qtractorMidiEditCommand(pMidiClip, "step input");
		unsigned short iInpEvents = 0;
		const QList<qtractorMidiEvent *>& events
			= m_inpEvents.values(pMidiClip);
		QListIterator<qtractorMidiEvent *> iter2(events);
		while (iter2.hasNext()) {
			qtractorMidiEvent *pEvent = iter2.next();
			if (pEvent->type() == qtractorMidiEvent::NOTEON
				&& pMidiClip->findStepInputEvent(pEvent)) {
				delete pEvent;
				pEvent = nullptr;
			}
			if (pEvent) {
				pMidiEditCommand->insertEvent(pEvent);
				++iInpEvents;
			}
		}
		if (iInpEvents > 0) {
			// Apply to MIDI clip *iif* its editor is there...
			// otherwise make it global to session.
			if (!pMidiClip->execute(pMidiEditCommand))
				pSession->execute(pMidiEditCommand);
		} else {
			// No events to apply...
			delete pMidiEditCommand;
		}
	}

	m_inpEvents.clear();
}


//----------------------------------------------------------------------
// class qtractorMidiBus -- Managed ALSA sequencer port set
//

// Constructor.
qtractorMidiBus::qtractorMidiBus ( qtractorMidiEngine *pMidiEngine,
	const QString& sBusName, BusMode busMode, bool bMonitor )
	: qtractorBus(pMidiEngine, sBusName, busMode, bMonitor)
{
	m_iAlsaPort = -1;

	if ((busMode & qtractorBus::Input) && !(busMode & qtractorBus::Ex)) {
		m_pIMidiMonitor = new qtractorMidiMonitor();
		m_pIPluginList  = createPluginList(qtractorPluginList::MidiInBus);
	} else {
		m_pIMidiMonitor = nullptr;
		m_pIPluginList  = nullptr;
	}

	if ((busMode & qtractorBus::Output) && !(busMode & qtractorBus::Ex)) {
		m_pOMidiMonitor = new qtractorMidiMonitor();
		m_pOPluginList  = createPluginList(qtractorPluginList::MidiOutBus);
		m_pSysexList    = new qtractorMidiSysexList();
	} else {
		m_pOMidiMonitor = nullptr;
		m_pOPluginList  = nullptr;
		m_pSysexList    = nullptr;
	}
}

// Destructor.
qtractorMidiBus::~qtractorMidiBus (void)
{
	close();

	if (m_pIMidiMonitor)
		delete m_pIMidiMonitor;
	if (m_pOMidiMonitor)
		delete m_pOMidiMonitor;

	if (m_pIPluginList)
		delete m_pIPluginList;
	if (m_pOPluginList)
		delete m_pOPluginList;

	if (m_pSysexList)
		delete m_pSysexList;
}


// ALSA sequencer port accessor.
int qtractorMidiBus::alsaPort (void) const
{
	return m_iAlsaPort;
}


// Register and pre-allocate bus port buffers.
bool qtractorMidiBus::open (void)
{
//	close();

	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return false;

	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return false;

	const qtractorBus::BusMode busMode
		= qtractorMidiBus::busMode();

	// The very same port might be used for input and output...
	unsigned int flags = 0;

	if (busMode & qtractorBus::Input)
		flags |= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
	if (busMode & qtractorBus::Output)
		flags |= SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;

	m_iAlsaPort = snd_seq_create_simple_port(
		pAlsaSeq, busName().toUtf8().constData(), flags,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

	if (m_iAlsaPort < 0)
		return false;

	// We want to know when the events get delivered to us...
	snd_seq_port_info_t *pinfo;
	snd_seq_port_info_alloca(&pinfo);

	if (snd_seq_get_port_info(pAlsaSeq, m_iAlsaPort, pinfo) < 0)
		return false;

	snd_seq_port_info_set_timestamping(pinfo, 1);
	snd_seq_port_info_set_timestamp_queue(pinfo, pMidiEngine->alsaQueue());
	snd_seq_port_info_set_timestamp_real(pinfo, 0);	// MIDI ticks.

	if (snd_seq_set_port_info(pAlsaSeq, m_iAlsaPort, pinfo) < 0)
		return false;

	// Update monitor subject names...
	qtractorMidiBus::updateBusName();

	// Plugin lists need some buffer (re)allocation too...
	if (m_pIPluginList)
		updatePluginList(m_pIPluginList, qtractorPluginList::MidiInBus);
	if (m_pOPluginList)
		updatePluginList(m_pOPluginList, qtractorPluginList::MidiOutBus);

	// Finally add this to the elligible input registry...
	if (m_pIMidiMonitor)
		pMidiEngine->addInputBus(this);

	// Done.
	return true;
}


// Unregister and post-free bus port buffers.
void qtractorMidiBus::close (void)
{
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	if (m_pIMidiMonitor)
		pMidiEngine->removeInputBus(this);

	shutOff(true);

	snd_seq_delete_simple_port(pAlsaSeq, m_iAlsaPort);

	m_iAlsaPort = -1;
}


// Bus mode change event.
void qtractorMidiBus::updateBusMode (void)
{
	const qtractorBus::BusMode busMode
		= qtractorMidiBus::busMode();

	// Have a new/old input monitor?
	if ((busMode & qtractorBus::Input) && !(busMode & qtractorBus::Ex)) {
		if (m_pIMidiMonitor == nullptr)
			m_pIMidiMonitor = new qtractorMidiMonitor();
		if (m_pIPluginList == nullptr)
			m_pIPluginList = createPluginList(qtractorPluginList::MidiInBus);
	} else {
		if (m_pIMidiMonitor) {
			delete m_pIMidiMonitor;
			m_pIMidiMonitor = nullptr;
		}
		if (m_pIPluginList) {
			delete m_pIPluginList;
			m_pIPluginList = nullptr;
		}
	}

	// Have a new/old output monitor?
	if ((busMode & qtractorBus::Output) && !(busMode & qtractorBus::Ex)) {
		if (m_pOMidiMonitor == nullptr)
			m_pOMidiMonitor = new qtractorMidiMonitor();
		if (m_pOPluginList == nullptr)
			m_pOPluginList = createPluginList(qtractorPluginList::MidiOutBus);
		if (m_pSysexList == nullptr)
			m_pSysexList = new qtractorMidiSysexList();
	} else {
		if (m_pOMidiMonitor) {
			delete m_pOMidiMonitor;
			m_pOMidiMonitor = nullptr;
		}
		if (m_pOPluginList) {
			delete m_pOPluginList;
			m_pOPluginList = nullptr;
		}
		if (m_pSysexList) {
			delete m_pSysexList;
			m_pSysexList = nullptr;
		}
	}
}


// Bus name change event.
void qtractorMidiBus::updateBusName (void)
{
	const QString& sBusName
		= qtractorMidiBus::busName();

	if (m_pIMidiMonitor) {
		const QString& sBusNameIn
			= QObject::tr("%1 In").arg(sBusName);
		m_pIMidiMonitor->gainSubject()->setName(
			QObject::tr("%1 Volume").arg(sBusNameIn));
		m_pIMidiMonitor->panningSubject()->setName(
			QObject::tr("%1 Pan").arg(sBusNameIn));
	}

	if (m_pOMidiMonitor) {
		const QString& sBusNameOut
			= QObject::tr("%1 Out").arg(sBusName);
		m_pOMidiMonitor->gainSubject()->setName(
			QObject::tr("%1 Volume").arg(sBusNameOut));
		m_pOMidiMonitor->panningSubject()->setName(
			QObject::tr("%1 Pan").arg(sBusNameOut));
	}

	qtractorBus::updateBusName();
}


// Shut-off everything out there.
void qtractorMidiBus::shutOff ( bool bClose )
{
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	if ((busMode() & qtractorBus::Output) == 0)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::shutOff(%d)", this, int(bClose));
#endif

	dequeueNoteOffs(pMidiEngine->queueTime());

	QHash<unsigned short, Patch>::ConstIterator iter
		= m_patches.constBegin();
	const QHash<unsigned short, Patch>::ConstIterator& iter_end
		= m_patches.constEnd();
	for ( ; iter != iter_end; ++iter) {
		const unsigned short iChannel = iter.key();
		setControllerEx(iChannel, ALL_SOUND_OFF);
		setControllerEx(iChannel, ALL_NOTES_OFF);
		if (bClose)
			setControllerEx(iChannel, ALL_CONTROLLERS_OFF);
	}
}


// Default instrument name accessors.
void qtractorMidiBus::setInstrumentName ( const QString& sInstrumentName )
{
	m_sInstrumentName = sInstrumentName;
}

const QString& qtractorMidiBus::instrumentName (void) const
{
	return m_sInstrumentName;
}


// SysEx setup list accessors.
qtractorMidiSysexList *qtractorMidiBus::sysexList (void) const
{
	return m_pSysexList;
}


// Direct MIDI bank/program selection helper.
void qtractorMidiBus::setPatch ( unsigned short iChannel,
	const QString& sInstrumentName, int iBankSelMethod,
	int iBank, int iProg, qtractorTrack *pTrack )
{
	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiBus[%p]::setPatch(%d, \"%s\", %d, %d, %d)",
		this, iChannel, sInstrumentName.toUtf8().constData(),
		iBankSelMethod, iBank, iProg);
#endif

	// Update patch mapping...
	Patch& patch = m_patches[iChannel & 0x0f];
	patch.instrumentName = sInstrumentName;
	patch.bankSelMethod  = iBankSelMethod;
	patch.bank = iBank;
	patch.prog = iProg;

	// Sanity check.
	if (!patch.isValid())
		m_patches.remove(iChannel & 0x0f);

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	// Do it for the MIDI plugins if applicable...
	qtractorMidiManager *pTrackMidiManager = nullptr;
	if (pTrack)
		pTrackMidiManager = (pTrack->pluginList())->midiManager();

	qtractorMidiManager *pBusMidiManager = nullptr;
	if (pluginList_out())
		pBusMidiManager = pluginList_out()->midiManager();

	// Initialize sequencer event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Addressing...
	snd_seq_ev_set_source(&ev, m_iAlsaPort);
	snd_seq_ev_set_subs(&ev);

	// The event will be direct...
	snd_seq_ev_set_direct(&ev);

	// Select Bank MSB.
	if (iBank >= 0 && (iBankSelMethod == 0 || iBankSelMethod == 1)) {
		ev.type = SND_SEQ_EVENT_CONTROLLER;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = BANK_SELECT_MSB;
		if (iBankSelMethod == 0)
			ev.data.control.value = (iBank & 0x3f80) >> 7;
		else
			ev.data.control.value = (iBank & 0x007f);
		snd_seq_event_output_direct(pAlsaSeq, &ev);
		if (pTrackMidiManager)
			pTrackMidiManager->direct(&ev);
		if (pBusMidiManager)
			pBusMidiManager->direct(&ev);
	}

	// Select Bank LSB.
	if (iBank >= 0 && (iBankSelMethod == 0 || iBankSelMethod == 2)) {
		ev.type = SND_SEQ_EVENT_CONTROLLER;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = BANK_SELECT_LSB;
		ev.data.control.value   = (iBank & 0x007f);
		snd_seq_event_output_direct(pAlsaSeq, &ev);
		if (pTrackMidiManager)
			pTrackMidiManager->direct(&ev);
		if (pBusMidiManager)
			pBusMidiManager->direct(&ev);
	}

	// Program change...
	if (iProg >= 0) {
		ev.type = SND_SEQ_EVENT_PGMCHANGE;
		ev.data.control.channel = iChannel;
		ev.data.control.value   = iProg;
		snd_seq_event_output_direct(pAlsaSeq, &ev);
		if (pTrackMidiManager)
			pTrackMidiManager->direct(&ev);
		if (pBusMidiManager)
			pBusMidiManager->direct(&ev);
	}

	// Bank reset to none...
	if (iBank < 0) {
		if (pTrackMidiManager)
			pTrackMidiManager->setCurrentBank(-1);
		if (pBusMidiManager)
			pBusMidiManager->setCurrentBank(-1);
	}

	// Program reset to none...
	if (iProg < 0) {
		if (pTrackMidiManager)
			pTrackMidiManager->setCurrentProg(-1);
		if (pBusMidiManager)
			pBusMidiManager->setCurrentProg(-1);
	}

//	pMidiEngine->flush();
}


// Direct MIDI controller helper.
void qtractorMidiBus::setController (
	qtractorTrack *pTrack, int iController, int iValue ) const
{
	setControllerEx(pTrack->midiChannel(), iController, iValue, pTrack);
}

void qtractorMidiBus::setController ( unsigned short iChannel,
	int iController, int iValue ) const
{
	setControllerEx(iChannel, iController, iValue, nullptr);
}


// Direct MIDI controller common helper.
void qtractorMidiBus::setControllerEx ( unsigned short iChannel,
	int iController, int iValue, qtractorTrack *pTrack ) const
{
	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::setControllerEx(%d, %d, %d, %p)",
		this, iChannel, iController, iValue, pTrack);
#endif

	// Initialize sequencer event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Addressing...
	snd_seq_ev_set_source(&ev, m_iAlsaPort);
	snd_seq_ev_set_subs(&ev);

	// The event will be direct...
	snd_seq_ev_set_direct(&ev);

	// Set controller parameters...
	ev.type = SND_SEQ_EVENT_CONTROLLER;
	ev.data.control.channel = iChannel;
	ev.data.control.param   = iController;
	ev.data.control.value   = iValue;
	snd_seq_event_output_direct(pAlsaSeq, &ev);

	// Do it for the MIDI plugins too...
	if (pTrack && (pTrack->pluginList())->midiManager())
		(pTrack->pluginList())->midiManager()->direct(&ev);
	if (pluginList_out() && pluginList_out()->midiManager())
		(pluginList_out()->midiManager())->direct(&ev);

//	pMidiEngine->flush();
}


// Direct MIDI channel event helper.
void qtractorMidiBus::sendEvent ( qtractorMidiEvent::EventType etype,
	unsigned short iChannel, unsigned short iParam, unsigned short iValue ) const
{
	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::sendEvent(0x%02x, %u, %u, %u)",
		this, int(etype), iChannel, iParam, iValue);
#endif

	// Initialize sequencer event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Addressing...
	snd_seq_ev_set_source(&ev, m_iAlsaPort);
	snd_seq_ev_set_subs(&ev);

	// The event will be direct...
	snd_seq_ev_set_direct(&ev);

	// Set controller parameters...
	switch (etype) {
	case qtractorMidiEvent::NOTEON:
		ev.type = SND_SEQ_EVENT_NOTEON;
		ev.data.note.channel  = iChannel;
		ev.data.note.note     = iParam;
		ev.data.note.velocity = iValue;
		break;
	case qtractorMidiEvent::NOTEOFF:
		ev.type = SND_SEQ_EVENT_NOTEOFF;
		ev.data.note.channel  = iChannel;
		ev.data.note.note     = iParam;
		ev.data.note.velocity = iValue;
		break;
	case qtractorMidiEvent::KEYPRESS:
		ev.type = SND_SEQ_EVENT_KEYPRESS;
		ev.data.note.channel  = iChannel;
		ev.data.note.note     = iParam;
		ev.data.note.velocity = iValue;
		break;
	case qtractorMidiEvent::CONTROLLER:
	default:
		ev.type = SND_SEQ_EVENT_CONTROLLER;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = iParam;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::REGPARAM:
		ev.type = SND_SEQ_EVENT_REGPARAM;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = iParam;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::NONREGPARAM:
		ev.type = SND_SEQ_EVENT_NONREGPARAM;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = iParam;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::CONTROL14:
		ev.type = SND_SEQ_EVENT_CONTROL14;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = iParam;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::PGMCHANGE:
		ev.type = SND_SEQ_EVENT_PGMCHANGE;
		ev.data.control.channel = iChannel;
	//	ev.data.control.param   = 0;
		ev.data.control.value   = iParam;
		break;
	case qtractorMidiEvent::CHANPRESS:
		ev.type = SND_SEQ_EVENT_CHANPRESS;
		ev.data.control.channel = iChannel;
	//	ev.data.control.param   = 0;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::PITCHBEND:
		ev.type = SND_SEQ_EVENT_PITCHBEND;
		ev.data.control.channel = iChannel;
	//	ev.data.control.param   = 0;
		ev.data.control.value   = int(iValue) - 0x2000;
		break;
	}

	snd_seq_event_output_direct(pAlsaSeq, &ev);
}


// Direct MIDI note on/off helper.
void qtractorMidiBus::sendNote (
	qtractorTrack *pTrack, int iNote, int iVelocity, bool bForce ) const
{
	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	const unsigned short iChannel = pTrack->midiChannel();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::sendNote(%d, %d, %d)",
		this, iChannel, iNote, iVelocity);
#endif

	// Initialize sequencer event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Addressing...
	snd_seq_ev_set_source(&ev, m_iAlsaPort);
	snd_seq_ev_set_subs(&ev);

	// The event will be direct...
	snd_seq_ev_set_direct(&ev);

	// Set controller parameters...
	ev.type = (iVelocity > 0 ? SND_SEQ_EVENT_NOTEON : SND_SEQ_EVENT_NOTEOFF);
	ev.data.note.channel  = iChannel;
	ev.data.note.note     = iNote;
	ev.data.note.velocity = iVelocity;
	snd_seq_event_output_direct(pAlsaSeq, &ev);

	// Do it for the MIDI plugins too...
	if ((pTrack->pluginList())->midiManager())
		(pTrack->pluginList())->midiManager()->direct(&ev);
	if (pluginList_out() && pluginList_out()->midiManager())
		(pluginList_out()->midiManager())->direct(&ev);

//	pMidiEngine->flush();

	// Bus/track output monitoring...
	if (iVelocity > 0) {
		// Bus output monitoring...
		if (m_pOMidiMonitor)
			m_pOMidiMonitor->enqueue(qtractorMidiEvent::NOTEON, iVelocity);
		// Track output monitoring...
		qtractorMidiMonitor *pMidiMonitor
			= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
		if (pMidiMonitor)
			pMidiMonitor->enqueue(qtractorMidiEvent::NOTEON, iVelocity);
	}

	// Attempt to capture the playing note as well...
	if (bForce && pTrack->isRecord()) {
		snd_seq_ev_set_dest(&ev,
			pMidiEngine->alsaClient(), m_iAlsaPort);
		snd_seq_ev_schedule_tick(&ev,
			pMidiEngine->alsaQueue(), 0, pMidiEngine->queueTime());
		pMidiEngine->capture(&ev);
	}
}


// Direct SysEx helpers.
void qtractorMidiBus::sendSysex (
	unsigned char *pSysex, unsigned int iSysex ) const
{
	// Yet again, we need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorMidiBus::sendSysex(%p, %u)", pSysex, iSysex);
	fprintf(stderr, " sysex {");
	for (unsigned int i = 0; i < iSysex; ++i)
		fprintf(stderr, " %02x", pSysex[i]);
	fprintf(stderr, " }\n");
#endif

	// Initialize sequencer event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Addressing...
	snd_seq_ev_set_source(&ev, m_iAlsaPort);
	snd_seq_ev_set_subs(&ev);

	// The event will be direct...
	snd_seq_ev_set_direct(&ev);

	// Just set SYSEX stuff and send it out..
	ev.type = SND_SEQ_EVENT_SYSEX;
	snd_seq_ev_set_sysex(&ev, iSysex, pSysex);
	snd_seq_event_output_direct(pAlsaSeq, &ev);

//	pMidiEngine->flush();
}


void qtractorMidiBus::sendSysexList (void) const
{
	// Check that we have some SysEx for setup...
	if (m_pSysexList == nullptr)
		return;
	if (m_pSysexList->count() < 1)
		return;

	// Yet again, we need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	QListIterator<qtractorMidiSysex *> iter(*m_pSysexList);
	while (iter.hasNext()) {
		qtractorMidiSysex *pSysex = iter.next();
	#ifdef CONFIG_DEBUG_0
		unsigned char *pData = pSysex->data();
		unsigned short iSize = pSysex->size();
		fprintf(stderr, "qtractorMidiBus::sendSysexList(%p, %u)", pData, iSize);
		fprintf(stderr, " sysex {");
		for (unsigned short i = 0; i < iSize; ++i)
			fprintf(stderr, " %02x", pData[i]);
		fprintf(stderr, " }\n");
	#endif
		// Initialize sequencer event...
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		// Addressing...
		snd_seq_ev_set_source(&ev, m_iAlsaPort);
		snd_seq_ev_set_subs(&ev);
		// The event will be direct...
		snd_seq_ev_set_direct(&ev);
		// Just set SYSEX stuff and send it out..
		ev.type = SND_SEQ_EVENT_SYSEX;
		snd_seq_ev_set_sysex(&ev, pSysex->size(), pSysex->data());
		snd_seq_event_output(pAlsaSeq, &ev);
		// AG: Do it for the MIDI plugins too...
		if (pluginList_out() && pluginList_out()->midiManager())
			(pluginList_out()->midiManager())->direct(&ev);
	}

	pMidiEngine->flush();
}


// Virtual I/O bus-monitor accessors.
qtractorMonitor *qtractorMidiBus::monitor_in (void) const
{
	return midiMonitor_in();
}

qtractorMonitor *qtractorMidiBus::monitor_out (void) const
{
	return midiMonitor_out();
}


// MIDI I/O bus-monitor accessors.
qtractorMidiMonitor *qtractorMidiBus::midiMonitor_in (void) const
{
	return m_pIMidiMonitor;
}

qtractorMidiMonitor *qtractorMidiBus::midiMonitor_out (void) const
{
	return m_pOMidiMonitor;
}


// Plugin-chain accessors.
qtractorPluginList *qtractorMidiBus::pluginList_in (void) const
{
	return m_pIPluginList;
}

qtractorPluginList *qtractorMidiBus::pluginList_out (void) const
{
	return m_pOPluginList;
}


// Create plugin-list properly.
qtractorPluginList *qtractorMidiBus::createPluginList ( int iFlags ) const
{
	// Create plugin-list alright...
	qtractorPluginList *pPluginList	= new qtractorPluginList(0, iFlags);

	// Set plugin-list title name...
	updatePluginListName(pPluginList, iFlags);

	return pPluginList;
}


// Update plugin-list title name...
void qtractorMidiBus::updatePluginListName (
	qtractorPluginList *pPluginList, int iFlags ) const
{
	pPluginList->setName((iFlags & qtractorPluginList::In ?
		QObject::tr("%1 In") : QObject::tr("%1 Out")).arg(busName()));
}


// Update plugin-list buffers properly.
void qtractorMidiBus::updatePluginList (
	qtractorPluginList *pPluginList, int iFlags )
{
	// Sanity checks...
	qtractorSession *pSession = engine()->session();
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	// Set plugin-list title name...
	updatePluginListName(pPluginList, iFlags);

	// Get audio bus as for the plugin list...
	qtractorAudioBus *pAudioBus = nullptr;
	if (pPluginList->midiManager())
		pAudioBus = (pPluginList->midiManager())->audioOutputBus();
	if (pAudioBus == nullptr) {
		// Output bus gets to be the first available output bus...
		QListIterator<qtractorBus *> iter(pAudioEngine->buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
			if (pBus->busMode() & qtractorBus::Output) {
				pAudioBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}
	}

	// Set plugin-list buffer alright...
	if (pAudioBus)
		pPluginList->setChannels(pAudioBus->channels(), iFlags);
}


// Retrieve all current ALSA connections for a given bus mode interface;
// return the effective number of connection attempts...
int qtractorMidiBus::updateConnects (
	qtractorBus::BusMode busMode, ConnectList& connects, bool bConnect ) const
{
	// Modes must match, at least...
	if ((busMode & qtractorMidiBus::busMode()) == 0)
		return 0;

	if (bConnect && connects.isEmpty())
		return 0;

	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return 0;

	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return 0;

	// Which kind of subscription?
	snd_seq_query_subs_type_t subs_type
		= (busMode == qtractorBus::Input ?
			SND_SEQ_QUERY_SUBS_WRITE : SND_SEQ_QUERY_SUBS_READ);

	snd_seq_query_subscribe_t *pAlsaSubs;
	snd_seq_addr_t seq_addr;
	
	snd_seq_query_subscribe_alloca(&pAlsaSubs);

	snd_seq_client_info_t *pClientInfo;
	snd_seq_port_info_t   *pPortInfo;

	snd_seq_client_info_alloca(&pClientInfo);
	snd_seq_port_info_alloca(&pPortInfo);

	ConnectItem item, *pItem;

	// Update current client/ports ids.
	unsigned int iPortFlags;
	if (busMode == qtractorBus::Input)
		iPortFlags = SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ;
	else
		iPortFlags = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;

	while (snd_seq_query_next_client(pAlsaSeq, pClientInfo) >= 0) {
		item.client = snd_seq_client_info_get_client(pClientInfo);
		item.clientName = QString::fromUtf8(
			snd_seq_client_info_get_name(pClientInfo));
		snd_seq_port_info_set_client(pPortInfo, item.client);
		snd_seq_port_info_set_port(pPortInfo, -1);
		while (snd_seq_query_next_port(pAlsaSeq, pPortInfo) >= 0) {
			const unsigned int iPortCapability
				= snd_seq_port_info_get_capability(pPortInfo);
			if (((iPortCapability & iPortFlags) == iPortFlags) &&
				((iPortCapability & SND_SEQ_PORT_CAP_NO_EXPORT) == 0)) {
				item.port = snd_seq_port_info_get_port(pPortInfo);
				item.portName = QString::fromUtf8(
					snd_seq_port_info_get_name(pPortInfo));
				pItem = connects.findItem(item);
				if (pItem) {
					pItem->port = item.port;
					pItem->client = item.client;
				}
			}
		}
	}

	// Get port connections...
	snd_seq_query_subscribe_set_type(pAlsaSubs, subs_type);
	snd_seq_query_subscribe_set_index(pAlsaSubs, 0);
	seq_addr.client = pMidiEngine->alsaClient();
	seq_addr.port   = m_iAlsaPort;
	snd_seq_query_subscribe_set_root(pAlsaSubs, &seq_addr);
	while (snd_seq_query_port_subscribers(pAlsaSeq, pAlsaSubs) >= 0) {
		seq_addr = *snd_seq_query_subscribe_get_addr(pAlsaSubs);
		snd_seq_get_any_client_info(pAlsaSeq,
			seq_addr.client, pClientInfo);
		item.client = seq_addr.client;
		item.clientName = QString::fromUtf8(
			snd_seq_client_info_get_name(pClientInfo));
		snd_seq_get_any_port_info(pAlsaSeq,
			seq_addr.client, seq_addr.port, pPortInfo);
		item.port = seq_addr.port;
		item.portName = QString::fromUtf8(
			snd_seq_port_info_get_name(pPortInfo));
		// Check if already in list/connected...
		pItem = connects.findItem(item);
		if (pItem && bConnect) {
			int iItem = connects.indexOf(pItem);
			if (iItem >= 0) {
				connects.removeAt(iItem);
				delete pItem;
			}
		} else if (!bConnect)
			connects.append(new ConnectItem(item));
		// Fetch next connection...
		snd_seq_query_subscribe_set_index(pAlsaSubs,
			snd_seq_query_subscribe_get_index(pAlsaSubs) + 1);
	}

	// Shall we proceed for actual connections?
	if (!bConnect)
		return 0;

	snd_seq_port_subscribe_t *pPortSubs;
	snd_seq_port_subscribe_alloca(&pPortSubs);

	// For each (remaining) connection, try...
	int iUpdate = 0;
	QListIterator<ConnectItem *> iter(connects);
	while (iter.hasNext()) {
		ConnectItem *pItem = iter.next();
		// Don't care of non-valid client/ports...
		if (pItem->client < 0 || pItem->port < 0)
			continue;
		// Mangle which is output and input...
		if (busMode == qtractorBus::Input) {
			seq_addr.client = pItem->client;
			seq_addr.port   = pItem->port;
			snd_seq_port_subscribe_set_sender(pPortSubs, &seq_addr);
			seq_addr.client = pMidiEngine->alsaClient();
			seq_addr.port   = m_iAlsaPort;
			snd_seq_port_subscribe_set_dest(pPortSubs, &seq_addr);
		} else {
			seq_addr.client = pMidiEngine->alsaClient();
			seq_addr.port   = m_iAlsaPort;
			snd_seq_port_subscribe_set_sender(pPortSubs, &seq_addr);
			seq_addr.client = pItem->client;
			seq_addr.port   = pItem->port;
			snd_seq_port_subscribe_set_dest(pPortSubs, &seq_addr);
		}
#ifdef CONFIG_DEBUG
		const QString sPortName	= QString::number(m_iAlsaPort) + ':' + busName();
		qDebug("qtractorMidiBus[%p]::updateConnects(%d): "
			"snd_seq_subscribe_port: [%d:%s] => [%d:%s]\n", this, int(busMode),
				pMidiEngine->alsaClient(), sPortName.toUtf8().constData(),
				pItem->client, pItem->portName.toUtf8().constData());
#endif
		if (snd_seq_subscribe_port(pAlsaSeq, pPortSubs) == 0) {
			const int iItem = connects.indexOf(pItem);
			if (iItem >= 0) {
				connects.removeAt(iItem);
				delete pItem;
				++iUpdate;
			}
		}
	}

	// Remember to resend all session/tracks control stuff,
	// iif we've changed any of the intended MIDI connections...
	if (iUpdate)
		pMidiEngine->resetAllControllers(false); // Deferred++

	// Done.
	return iUpdate;
}


// MIDI master volume.
void qtractorMidiBus::setMasterVolume ( float fVolume )
{
	const unsigned char vol
		= (unsigned char) (int(127.0f * fVolume) & 0x7f);
	// Build Universal SysEx and let it go...
	unsigned char aMasterVolSysex[]
		= { 0xf0, 0x7f, 0x7f, 0x04, 0x01, 0x00, 0x00, 0xf7 };
	// Set the course value right...
	if (fVolume >= +1.0f)
		aMasterVolSysex[5] = 0x7f;
	aMasterVolSysex[6] = vol;
	sendSysex(aMasterVolSysex, sizeof(aMasterVolSysex));
}


// MIDI master panning.
void qtractorMidiBus::setMasterPanning ( float fPanning )
{
	const unsigned char pan
		= (unsigned char) ((0x40 + int(63.0f * fPanning)) & 0x7f);
	// Build Universal SysEx and let it go...
	unsigned char aMasterPanSysex[]
		= { 0xf0, 0x7f, 0x7f, 0x04, 0x02, 0x00, 0x00, 0xf7 };
	// Set the course value right...
	// And fine special for hard right...
	if (fPanning >= +1.0f)
		aMasterPanSysex[5] = 0x7f;
	if (fPanning > -1.0f)
		aMasterPanSysex[6] = pan;
	sendSysex(aMasterPanSysex, sizeof(aMasterPanSysex));
}


// MIDI channel volume.
void qtractorMidiBus::setVolume ( qtractorTrack *pTrack, float fVolume )
{
	const unsigned char vol
		= (unsigned char) (int(127.0f * fVolume) & 0x7f);
	setController(pTrack, CHANNEL_VOLUME, vol);
}


// MIDI channel stereo panning.
void qtractorMidiBus::setPanning ( qtractorTrack *pTrack, float fPanning )
{
	const unsigned char pan
		= (unsigned char) ((0x40 + int(63.0f * fPanning)) & 0x7f);
	setController(pTrack, CHANNEL_PANNING, pan);
}


// Document element methods.
bool qtractorMidiBus::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	for (QDomNode nProp = pElement->firstChild();
			!nProp.isNull();
				nProp = nProp.nextSibling()) {

		// Convert node to element...
		QDomElement eProp = nProp.toElement();
		if (eProp.isNull())
			continue;

		// Load map elements (non-critical)...
		if (eProp.tagName() == "pass-through" || // Legacy compat.
			eProp.tagName() == "midi-thru"    ||
			eProp.tagName() == "monitor") {
			qtractorMidiBus::setMonitor(
				qtractorDocument::boolFromText(eProp.text()));
		} else if (eProp.tagName() == "midi-sysex-list") {
			qtractorMidiBus::loadSysexList(pDocument, &eProp);
		} else if (eProp.tagName() == "midi-map") {
			qtractorMidiBus::loadMidiMap(pDocument, &eProp);
		} else if (eProp.tagName() == "midi-instrument-name") {
			qtractorMidiBus::setInstrumentName(eProp.text());
		} else if (eProp.tagName() == "input-gain") {
			if (qtractorMidiBus::monitor_in())
				qtractorMidiBus::monitor_in()->setGain(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-panning") {
			if (qtractorMidiBus::monitor_in())
				qtractorMidiBus::monitor_in()->setPanning(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-controllers") {
			qtractorMidiBus::loadControllers(&eProp, qtractorBus::Input);
		} else if (eProp.tagName() == "input-plugins") {
			if (qtractorMidiBus::pluginList_in())
				qtractorMidiBus::pluginList_in()->loadElement(
					pDocument, &eProp);
		} else if (eProp.tagName() == "input-connects") {
			qtractorMidiBus::loadConnects(
				qtractorMidiBus::inputs(), pDocument, &eProp);
		} else if (eProp.tagName() == "output-gain") {
			if (qtractorMidiBus::monitor_out())
				qtractorMidiBus::monitor_out()->setGain(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "output-panning") {
			if (qtractorMidiBus::monitor_out())
				qtractorMidiBus::monitor_out()->setPanning(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "output-controllers") {
			qtractorMidiBus::loadControllers(&eProp, qtractorBus::Output);
		} else if (eProp.tagName() == "output-plugins") {
			if (qtractorMidiBus::pluginList_out())
				qtractorMidiBus::pluginList_out()->loadElement(
					pDocument, &eProp);
		} else if (eProp.tagName() == "output-connects") {
			qtractorMidiBus::loadConnects(
				qtractorMidiBus::outputs(), pDocument, &eProp);
		}
	}

	return true;
}


bool qtractorMidiBus::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	const qtractorBus::BusMode busMode
		= qtractorMidiBus::busMode();

	pElement->setAttribute("name",
		qtractorMidiBus::busName());
	pElement->setAttribute("mode",
		qtractorBus::textFromBusMode(busMode));

	pDocument->saveTextElement("monitor",
		qtractorDocument::textFromBool(
			qtractorMidiBus::isMonitor()), pElement);

	if (busMode & qtractorBus::Input) {
		pDocument->saveTextElement("input-gain",
			QString::number(qtractorMidiBus::monitor_in()->gain()),
				pElement);
		pDocument->saveTextElement("input-panning",
			QString::number(qtractorMidiBus::monitor_in()->panning()),
				pElement);
		// Save input bus controllers...
		QDomElement eInputControllers
			= pDocument->document()->createElement("input-controllers");
		qtractorMidiBus::saveControllers(pDocument,
			&eInputControllers, qtractorBus::Input);
		pElement->appendChild(eInputControllers);
		// Save input bus plugins...
		if (qtractorMidiBus::pluginList_in()) {
			QDomElement eInputPlugins
				= pDocument->document()->createElement("input-plugins");
			qtractorMidiBus::pluginList_in()->saveElement(
				pDocument, &eInputPlugins);
			pElement->appendChild(eInputPlugins);
		}
		// Save input bus connections...
		QDomElement eMidiInputs
			= pDocument->document()->createElement("input-connects");
		qtractorBus::ConnectList inputs;
		qtractorMidiBus::updateConnects(qtractorBus::Input, inputs);
		qtractorMidiBus::saveConnects(inputs, pDocument, &eMidiInputs);
		pElement->appendChild(eMidiInputs);
	}

	if (busMode & qtractorBus::Output) {
		pDocument->saveTextElement("output-gain",
			QString::number(qtractorMidiBus::monitor_out()->gain()),
				pElement);
		pDocument->saveTextElement("output-panning",
			QString::number(qtractorMidiBus::monitor_out()->panning()),
				pElement);
		// Save output bus controllers...
		QDomElement eOutputControllers
			= pDocument->document()->createElement("output-controllers");
		qtractorMidiBus::saveControllers(pDocument,
			&eOutputControllers, qtractorBus::Output);
		pElement->appendChild(eOutputControllers);
		// Save output bus plugins...
		if (qtractorMidiBus::pluginList_out()) {
			QDomElement eOutputPlugins
				= pDocument->document()->createElement("output-plugins");
			qtractorMidiBus::pluginList_out()->saveElement(
				pDocument, &eOutputPlugins);
			pElement->appendChild(eOutputPlugins);
		}
		// Save output bus connections...
		QDomElement eMidiOutputs
			= pDocument->document()->createElement("output-connects");
		qtractorBus::ConnectList outputs;
		qtractorMidiBus::updateConnects(qtractorBus::Output, outputs);
		qtractorMidiBus::saveConnects(outputs, pDocument, &eMidiOutputs);
		pElement->appendChild(eMidiOutputs);
	}

	// Save default instrument name, if any...
	if (!qtractorMidiBus::instrumentName().isEmpty()) {
		pDocument->saveTextElement("midi-instrument-name",
			qtractorMidiBus::instrumentName(), pElement);
	}

	// Create the sysex element...
	if (m_pSysexList && m_pSysexList->count() > 0) {
		QDomElement eSysexList
			= pDocument->document()->createElement("midi-sysex-list");
		qtractorMidiBus::saveSysexList(pDocument, &eSysexList);
		pElement->appendChild(eSysexList);
	}

	// Create the map element...
	if (m_patches.count() > 0) {
		QDomElement eMidiMap
			= pDocument->document()->createElement("midi-map");
		qtractorMidiBus::saveMidiMap(pDocument, &eMidiMap);
		pElement->appendChild(eMidiMap);
	}

	return true;
}


// Document instrument map methods.
bool qtractorMidiBus::loadMidiMap (
	qtractorDocument * /*pDocument*/, QDomElement *pElement )
{
	m_patches.clear();

	// Load map items...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Load map item...
		if (eChild.tagName() == "midi-patch") {
			const unsigned short iChannel
				= eChild.attribute("channel").toUShort();
			Patch& patch = m_patches[iChannel & 0x0f];
			for (QDomNode nPatch = eChild.firstChild();
					!nPatch.isNull();
						nPatch = nPatch.nextSibling()) {
				// Convert patch node to element...
				QDomElement ePatch = nPatch.toElement();
				if (ePatch.isNull())
					continue;
				// Add this one to map...
				if (ePatch.tagName() == "midi-instrument")
					patch.instrumentName = ePatch.text();
				else
				if (ePatch.tagName() == "midi-bank-sel-method")
					patch.bankSelMethod = ePatch.text().toInt();
				else
				if (ePatch.tagName() == "midi-bank")
					patch.bank = ePatch.text().toInt();
				else
				if (ePatch.tagName() == "midi-program")
					patch.prog = ePatch.text().toInt();
			}
			// Rollback if instrument-patch is invalid...
			if (!patch.isValid())
				m_patches.remove(iChannel & 0x0f);
		}
	}

	return true;
}


bool qtractorMidiBus::saveMidiMap (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	// Save map items...
	QHash<unsigned short, Patch>::ConstIterator iter
		= m_patches.constBegin();
	const QHash<unsigned short, Patch>::ConstIterator& iter_end
		= m_patches.constEnd();
	for ( ; iter != iter_end; ++iter) {
		const Patch& patch = iter.value();
		if (!patch.isValid())
			continue;
		QDomElement ePatch = pDocument->document()->createElement("midi-patch");
		ePatch.setAttribute("channel", QString::number(iter.key()));
		if (!patch.instrumentName.isEmpty()) {
			pDocument->saveTextElement("midi-instrument",
				patch.instrumentName, &ePatch);
		}
		if (patch.bankSelMethod >= 0) {
			pDocument->saveTextElement("midi-bank-sel-method",
				QString::number(patch.bankSelMethod), &ePatch);
		}
		if (patch.bank >= 0) {
			pDocument->saveTextElement("midi-bank",
				QString::number(patch.bank), &ePatch);
		}
		if (patch.prog >= 0) {
			pDocument->saveTextElement("midi-program",
				QString::number(patch.prog), &ePatch);
		}
		pElement->appendChild(ePatch);
	}

	return true;
}


// Document SysEx setup list methods.
bool qtractorMidiBus::loadSysexList (
	qtractorDocument * /*pDocument*/, QDomElement *pElement )
{
	// Must have one...
	if (m_pSysexList == nullptr)
		return false;

	// Crystal clear...
	m_pSysexList->clear();

	// Load map items...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Load map item...
		if (eChild.tagName() == "midi-sysex") {
			qtractorMidiSysex *pSysex
				= new qtractorMidiSysex(
					eChild.attribute("name"), eChild.text());
			if (pSysex->size() > 0)
				m_pSysexList->append(pSysex);
			else
				delete pSysex;
		}
	}

	return true;
}


bool qtractorMidiBus::saveSysexList (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	// Must have one...
	if (m_pSysexList == nullptr)
		return false;

	// Save map items...
	QListIterator<qtractorMidiSysex *> iter(*m_pSysexList);
	while (iter.hasNext()) {
		qtractorMidiSysex *pSysex = iter.next();
		QDomElement eSysex = pDocument->document()->createElement("midi-sysex");
		eSysex.setAttribute("name", pSysex->name());
		eSysex.appendChild(
			pDocument->document()->createTextNode(pSysex->text()));
		pElement->appendChild(eSysex);
	}

	return true;
}


// Import SysEx setup from event sequence.
bool qtractorMidiBus::importSysexList ( qtractorMidiSequence *pSeq )  
{
	if (m_pSysexList == nullptr)
		return false;

	m_pSysexList->clear();

	int iSysex = 0;
	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		if (pEvent->type() == qtractorMidiEvent::SYSEX) {
			m_pSysexList->append(
				new qtractorMidiSysex(pSeq->name()
					+ '-' + QString::number(++iSysex),
					pEvent->sysex(), pEvent->sysex_len())
			);
		}
		pEvent = pEvent->next();
	}

	return true;
}


// Export SysEx setup to event sequence.
bool qtractorMidiBus::exportSysexList ( qtractorMidiSequence *pSeq )
{
	if (m_pSysexList == nullptr)
		return false;

	QListIterator<qtractorMidiSysex *> iter(*m_pSysexList);
	while (iter.hasNext()) {
		qtractorMidiSysex *pSysex = iter.next();
		qtractorMidiEvent *pEvent
			= new qtractorMidiEvent(0, qtractorMidiEvent::SYSEX);
		pEvent->setSysex(pSysex->data(), pSysex->size());
		pSeq->addEvent(pEvent);
	}

	return true;
}


// Pending note-offs processing methods.
//
void qtractorMidiBus::enqueueNoteOff (
	snd_seq_event_t *pEv, unsigned long iTimeOn, unsigned long iTimeOff )
{
	const unsigned short key
		= (pEv->data.note.channel << 7) | (pEv->data.note.note & 0x7f);
	m_noteOffs.insert(key, NoteOff(iTimeOn, iTimeOff));
}


void qtractorMidiBus::dequeueNoteOffs ( unsigned long iQueueTime )
{
	// Whether we have anything pending...
	if (m_noteOffs.isEmpty())
		return;

	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == nullptr)
		return;

	// Don't do anything else if engine
	// has not been activated...
	snd_seq_t *pAlsaSeq = pMidiEngine->alsaSeq();
	if (pAlsaSeq == nullptr)
		return;

	NoteOffs::ConstIterator iter = m_noteOffs.constBegin();
	const NoteOffs::ConstIterator& iter_end = m_noteOffs.constEnd();
	for ( ; iter != iter_end; ++iter) {
		const unsigned short key = iter.key();
		const NoteOff& value = iter.value();
		if (value.time_on  <  iQueueTime &&
			value.time_off >= iQueueTime) {
			// Determina channel and note address...
			const unsigned short iChannel = (key >> 7);
			const unsigned short iNote = (key & 0x7f);
			// Send NOTE_OFF event...
			snd_seq_event_t ev;
			snd_seq_ev_clear(&ev);
			snd_seq_ev_set_source(&ev, m_iAlsaPort);
			snd_seq_ev_set_subs(&ev);
			snd_seq_ev_set_direct(&ev);
			ev.type = SND_SEQ_EVENT_NOTEOFF;
			ev.data.note.channel  = iChannel;
			ev.data.note.note     = iNote;
			ev.data.note.velocity = 0;
			snd_seq_event_output_direct(pAlsaSeq, &ev);
		#ifdef CONFIG_DEBUG_0
			qDebug("qtractorMidiBus[%p]::dequeueNoteOffs(%lu):"
				" channel=%u note=%u", this, iQueueTime, iChannel, iNote);
		#endif
		}
	}

	m_noteOffs.clear();
}


// Update all aux-sends to this very bus...
//
void qtractorMidiBus::updateMidiAuxSends ( const QString& sMidiBusName )
{
	if ((busMode() & qtractorBus::Output) == 0)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == nullptr)
		return;

	// Make it to all MIDI output buses...
	for (qtractorBus *pBus = pMidiEngine->buses().first();
			pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus = static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus == this)
			continue;
		if ((pMidiBus->busMode() & qtractorBus::Output) == 0)
			continue;
		qtractorPluginList *pPluginList = pMidiBus->pluginList_out();
		if (pPluginList == nullptr)
			continue;
		for (qtractorPlugin *pPlugin = pPluginList->first();
				pPlugin; pPlugin = pPlugin->next()) {
			qtractorPluginType *pType = pPlugin->type();
			if (pType && pType->typeHint() == qtractorPluginType::AuxSend
				&& pType->index() == 0) { // index == 0 => MIDI aux-send.
				qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
					= static_cast<qtractorMidiAuxSendPlugin *> (pPlugin);
				if (pMidiAuxSendPlugin
					&& pMidiAuxSendPlugin->midiBus() == this)
					pMidiAuxSendPlugin->setMidiBusName(sMidiBusName);
			}
		}
	}

	// Make it to MIDI tracks only...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() != qtractorTrack::Midi)
			continue;
		qtractorPluginList *pPluginList = pTrack->pluginList();
		if (pPluginList == nullptr)
			continue;
		for (qtractorPlugin *pPlugin = pPluginList->first();
				pPlugin; pPlugin = pPlugin->next()) {
			qtractorPluginType *pType = pPlugin->type();
			if (pType && pType->typeHint() != qtractorPluginType::AuxSend)
				continue;
			if (pType->index() == 0) { // index == 0 => MIDI aux-send.
				qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
					= static_cast<qtractorMidiAuxSendPlugin *> (pPlugin);
				if (pMidiAuxSendPlugin
					&& pMidiAuxSendPlugin->midiBus() == this)
					pMidiAuxSendPlugin->setMidiBusName(sMidiBusName);
			}
		}
	}
}


// end of qtractorMidiEngine.cpp
