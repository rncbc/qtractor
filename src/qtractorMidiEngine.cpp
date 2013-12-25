// qtractorMidiEngine.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"
#include "qtractorMidiEvent.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorDocument.h"

#include "qtractorAudioEngine.h"

#include "qtractorMidiSequence.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiBuffer.h"
#include "qtractorMidiControl.h"
#include "qtractorMidiTimer.h"
#include "qtractorMidiSysex.h"
#include "qtractorMidiRpn.h"

#include "qtractorPlugin.h"

#include "qtractorCurveFile.h"

#include <QApplication>
#include <QFileInfo>

#include <QDomDocument>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <QSocketNotifier>

#include <QTime>

#include <math.h>


// Specific controller definitions
#define BANK_SELECT_MSB		0x00
#define BANK_SELECT_LSB		0x20

#define ALL_SOUND_OFF		0x78
#define ALL_CONTROLLERS_OFF	0x79
#define ALL_NOTES_OFF		0x7b

#define CHANNEL_VOLUME		0x07
#define CHANNEL_PANNING		0x0a


// Audio vs. MIDI time drift cycle
#define DRIFT_CHECK 8


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
	qtractorMidiOutputThread(
		qtractorMidiEngine *pMidiEngine, unsigned int iReadAhead);

	// Destructor.
	~qtractorMidiOutputThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Read ahead frames configuration.
	void setReadAhead(unsigned int iReadAhead);
	unsigned int readAhead() const;

	// MIDI/Audio sync-check predicate.
	qtractorSessionCursor *midiCursorSync(bool bStart = false);

	// MIDI track output process resync.
	void trackSync(qtractorTrack *pTrack, unsigned long iFrameStart);

	// MIDI metronome output process resync.
	void metroSync(unsigned long iFrameStart);

	// MIDI output process cycle iteration (locked).
	void processSync();

	// Wake from executive wait condition.
	void sync();

protected:

	// The main thread executive.
	void run();

	// MIDI output process cycle iteration.
	void process();

private:

	// The thread launcher engine.
	qtractorMidiEngine *m_pMidiEngine;

	// The number of frames to read-ahead.
	unsigned int m_iReadAhead;

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
		qtractorMidiEvent *pEvent, unsigned long iTime, float fGain);

private:

	// Instance variables.
	qtractorMidiBus           *m_pMidiBus;
	qtractorMidiEngine        *m_pMidiEngine;

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

	switch (qtractorMidiRpn::Type(event.status & 0xf0)) {
	case qtractorMidiRpn::RPN:	// 0x10
		ev->type = SND_SEQ_EVENT_REGPARAM;
		break;
	case qtractorMidiRpn::NRPN:	// 0x20
		ev->type = SND_SEQ_EVENT_NONREGPARAM;
		break;
	case qtractorMidiRpn::CC14:	// 0x30
		ev->type = SND_SEQ_EVENT_CONTROL14;
		break;
	case qtractorMidiRpn::CC:	// 0xb0
		ev->type = SND_SEQ_EVENT_CONTROLLER;
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
	if (pAlsaSeq == NULL)
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
			snd_seq_event_t *pEv = NULL;
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
	qtractorMidiEngine *pMidiEngine, unsigned int iReadAhead ) : QThread()
{
	m_pMidiEngine = pMidiEngine;
	m_bRunState   = false;
	m_iReadAhead  = iReadAhead;
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


// Read ahead frames configuration.
void qtractorMidiOutputThread::setReadAhead ( unsigned int iReadAhead )
{
	QMutexLocker locker(&m_mutex);

	m_iReadAhead = iReadAhead;
}

unsigned int qtractorMidiOutputThread::readAhead (void) const
{
	return m_iReadAhead;
}


// Audio/MIDI sync-check and cursor predicate.
qtractorSessionCursor *qtractorMidiOutputThread::midiCursorSync ( bool bStart )
{
	// We'll need access to master audio engine...
	qtractorSessionCursor *pAudioCursor
		= (m_pMidiEngine->session())->audioEngine()->sessionCursor();
	if (pAudioCursor == NULL)
		return NULL;

	// And to our slave MIDI engine too...
	qtractorSessionCursor *pMidiCursor = m_pMidiEngine->sessionCursor();
	if (pMidiCursor == NULL)
		return NULL;

	// Can MIDI be ever behind audio?
	if (bStart) {
		pMidiCursor->seek(pAudioCursor->frame());
	//	pMidiCursor->setFrameTime(pAudioCursor->frameTime());
	}
	else // No, it cannot be behind more than the read-ahead period...
	if (pMidiCursor->frameTime() > pAudioCursor->frameTime() + m_iReadAhead)
		return NULL;

	// Nope. OK.
	return pMidiCursor;
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
			process();
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::run(): stopped.", this);
#endif
}


// MIDI output process cycle iteration.
void qtractorMidiOutputThread::process (void)
{
	// Must have a valid session...
	qtractorSession *pSession = m_pMidiEngine->session();
	if (pSession == NULL)
		return;
	
	// Get a handle on our slave MIDI engine...
	qtractorSessionCursor *pMidiCursor = midiCursorSync();
	// Isn't MIDI slightly behind audio?
	if (pMidiCursor == NULL)
		return;

	// Free overriden SysEx queued events.
	m_pMidiEngine->clearSysexCache();

	// Now for the next readahead bunch...
	unsigned long iFrameStart = pMidiCursor->frame();
	unsigned long iFrameEnd   = iFrameStart + m_iReadAhead;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::process(%lu, %lu)",
		this, iFrameStart, iFrameEnd);
#endif

	// Split processing, in case we're looping...
	bool bLooping = pSession->isLooping();
	unsigned long le = pSession->loopEnd();
	if (bLooping && iFrameStart < le) {
		// Loop-length might be shorter than the read-ahead...
		while (iFrameEnd >= le) {
			// Process metronome clicks...
			m_pMidiEngine->processMetro(iFrameStart, le);
			// Process the remaining until end-of-loop...
			pSession->process(pMidiCursor, iFrameStart, le);
			// Reset to start-of-loop...
			iFrameStart = pSession->loopStart();
			iFrameEnd   = iFrameStart + (iFrameEnd - le);
			pMidiCursor->seek(iFrameStart);
			// This is really a must...
			m_pMidiEngine->restartLoop();
		}
	}

	// Process metronome clicks...
	m_pMidiEngine->processMetro(iFrameStart, iFrameEnd);
	// Regular range...
	pSession->process(pMidiCursor, iFrameStart, iFrameEnd);

	// Sync with loop boundaries (unlikely?)...
	if (bLooping && iFrameStart < le && iFrameEnd >= le)
		iFrameEnd = pSession->loopStart() + (iFrameEnd - le);

	// Sync to the next bunch, also critical for Audio-MIDI sync...
	pMidiCursor->seek(iFrameEnd);
	pMidiCursor->process(m_iReadAhead);

	// Flush the MIDI engine output queue...
	m_pMidiEngine->flush();

	// Always do the queue drift stats
	// at the bottom of the pack...
	m_pMidiEngine->drift();
}


// MIDI output process cycle iteration (locked).
void qtractorMidiOutputThread::processSync (void)
{
	QMutexLocker locker(&m_mutex);
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::processSync()", this);
#endif
	process();
}


// MIDI track output process resync.
void qtractorMidiOutputThread::trackSync ( qtractorTrack *pTrack,
	unsigned long iFrameStart )
{
	QMutexLocker locker(&m_mutex);

	// Must have a valid session...
	qtractorSession *pSession = m_pMidiEngine->session();
	if (pSession == NULL)
		return;
	
	// Pick our actual MIDI sequencer cursor...
	qtractorSessionCursor *pMidiCursor = m_pMidiEngine->sessionCursor();
	if (pMidiCursor == NULL)
		return;

	// This is the last framestamp to be trown out...
	unsigned long iFrameEnd = pMidiCursor->frame();

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
	m_pMidiEngine->flush();
}


// MIDI metronome output process resync.
void qtractorMidiOutputThread::metroSync ( unsigned long iFrameStart )
{
	QMutexLocker locker(&m_mutex);

	// Pick our actual MIDI sequencer cursor...
	qtractorSessionCursor *pMidiCursor = m_pMidiEngine->sessionCursor();
	if (pMidiCursor == NULL)
		return;

	// This is the last framestamp to be trown out...
	unsigned long iFrameEnd = pMidiCursor->frame();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::metroSync(%lu, %lu)",
		this, iFrameStart, iFrameEnd);
#endif

	// (Re)process the metronome stuff...
	m_pMidiEngine->processMetro(iFrameStart, iFrameEnd);

	// Surely must realize the output queue...
	m_pMidiEngine->flush();
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
qtractorMidiPlayer::qtractorMidiPlayer (qtractorMidiBus *pMidiBus )
	: m_pMidiBus(pMidiBus),
		m_pMidiEngine(static_cast<qtractorMidiEngine *> (pMidiBus->engine())),
		m_iSeqs(0), m_ppSeqs(NULL), m_ppSeqCursors(NULL), m_pTimeScale(NULL), 
		m_pCursor(NULL), m_fTempo(0.0f), m_pPlayerThread(NULL)
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

	if (m_pMidiBus == NULL)
		return false;

	qtractorSession *pSession = m_pMidiEngine->session();
	if (pSession == NULL)
		return false;

	qtractorSessionCursor *pAudioCursor
		= pSession->audioEngine()->sessionCursor();
	if (pAudioCursor == NULL)
		return false;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return false;

	snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
	if (pAlsaSeq == NULL)
		return false;

	qtractorMidiFile file;
	if (!file.open(sFilename))
		return false;

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

	int iAlsaQueue = m_pMidiEngine->alsaQueue();

	snd_seq_queue_tempo_t *tempo;
	snd_seq_queue_tempo_alloca(&tempo);
	snd_seq_get_queue_tempo(pAlsaSeq, iAlsaQueue, tempo);
	snd_seq_queue_tempo_set_ppq(tempo, (int) m_pTimeScale->ticksPerBeat());
	snd_seq_queue_tempo_set_tempo(tempo,
		(unsigned int) (60000000.0f / m_fTempo));
	snd_seq_set_queue_tempo(pAlsaSeq, iAlsaQueue, tempo);

	snd_seq_start_queue(pAlsaSeq, iAlsaQueue, NULL);
	snd_seq_drain_output(pAlsaSeq);

	pAudioCursor->setFrameTime(0);

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
		m_pPlayerThread = NULL;
	}

	if (m_ppSeqs && m_pMidiBus) {
		snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
		if (pAlsaSeq) {
			snd_seq_drop_output(pAlsaSeq);
			int iAlsaQueue = m_pMidiEngine->alsaQueue();
			if (iAlsaQueue >= 0)
				snd_seq_stop_queue(pAlsaSeq, iAlsaQueue, NULL);
			for (unsigned short iSeq = 0; iSeq < m_iSeqs; ++iSeq) {
				unsigned short iChannel = m_ppSeqs[iSeq]->channel();
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
		m_pCursor = NULL;
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
		m_ppSeqCursors = NULL;
	}

	if (m_ppSeqs) {
		delete [] m_ppSeqs;
		m_ppSeqs = NULL;
	}

	m_iSeqs = 0;

	if (m_pTimeScale) {
		delete m_pTimeScale;
		m_pTimeScale = NULL;
	}
}


// Process playing executive.
bool qtractorMidiPlayer::process (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (m_pMidiBus == NULL)
		return false;

	if (m_pCursor == NULL)
		return false;

	snd_seq_t *pAlsaSeq = m_pMidiEngine->alsaSeq();
	if (pAlsaSeq == NULL)
		return false;

	int iAlsaQueue = m_pMidiEngine->alsaQueue();
	if (iAlsaQueue < 0)
		return false;

	qtractorTimeScale::Node *pNode
		= m_pCursor->seekFrame(iFrameEnd);
	if (pNode->tempo != m_fTempo) {
		unsigned long iTime = (pNode->frame < iFrameStart
			? pNode->tickFromFrame(iFrameStart) : pNode->tick);
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		snd_seq_ev_schedule_tick(&ev, iAlsaQueue, 0, iTime);
		ev.type = SND_SEQ_EVENT_TEMPO;
		ev.data.queue.queue = iAlsaQueue;
		ev.data.queue.param.value
			= (unsigned int) (60000000.0f / pNode->tempo);
		ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
		ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
		snd_seq_event_output(pAlsaSeq, &ev);
		m_fTempo = pNode->tempo;
		qtractorMidiMonitor::splitTime(
			m_pCursor->timeScale(), pNode->frame, iTime);
	}

	unsigned long iTimeStart = m_pTimeScale->tickFromFrame(iFrameStart);
	unsigned long iTimeEnd   = m_pTimeScale->tickFromFrame(iFrameEnd);

	unsigned int iProcess = 0;
	for (unsigned short iSeq = 0; iSeq < m_iSeqs; ++iSeq) {
		qtractorMidiSequence *pSeq = m_ppSeqs[iSeq];
		qtractorMidiCursor *pSeqCursor = m_ppSeqCursors[iSeq];
		qtractorMidiEvent *pEvent = pSeqCursor->seek(pSeq, iTimeStart);
		while (pEvent) {
			unsigned long iTime = pEvent->time(); // + iTimeOffset?
			if (iTime >= iTimeEnd)
				break;
			if (iTime >= iTimeStart)
				enqueue(pSeq->channel(), pEvent, iTime, 1.0f);
			pEvent = pEvent->next();
		}
		if (iTimeEnd < pSeq->duration()) ++iProcess;
	}

	snd_seq_drain_output(pAlsaSeq);

	return (iProcess > 0);
}


// Enqueue process event.
void qtractorMidiPlayer::enqueue ( unsigned short iMidiChannel,
	qtractorMidiEvent *pEvent, unsigned long iTime, float fGain )
{
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	snd_seq_ev_set_source(&ev, m_pMidiBus->alsaPort());
	snd_seq_ev_set_subs(&ev);

	snd_seq_ev_schedule_tick(&ev, m_pMidiEngine->alsaQueue(), 0, iTime);

	switch (pEvent->type()) {
	case qtractorMidiEvent::NOTEON:
		ev.type = SND_SEQ_EVENT_NOTE;
		ev.data.note.channel  = iMidiChannel;
		ev.data.note.note     = pEvent->note();
		ev.data.note.velocity = int(fGain * float(pEvent->value())) & 0x7f;
		ev.data.note.duration = pEvent->duration();
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
	case qtractorMidiEvent::PGMCHANGE:
		ev.type = SND_SEQ_EVENT_PGMCHANGE;
		ev.data.control.channel = iMidiChannel;
		ev.data.control.value   = pEvent->value();
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

	if (m_pMidiBus->midiMonitor_out())
		m_pMidiBus->midiMonitor_out()->enqueue(
			pEvent->type(), pEvent->value(), iTime);

	if (m_pMidiBus->pluginList_out()
		&& (m_pMidiBus->pluginList_out())->midiManager())
		((m_pMidiBus->pluginList_out())->midiManager())->queued(
			m_pTimeScale, &ev, iTime, 0);
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
	if (pAlsaSeq == NULL)
		return 0;

	int iAlsaQueue = m_pMidiEngine->alsaQueue();
	if (iAlsaQueue < 0)
		return 0;

	unsigned long iQueueTime = 0;
	
	snd_seq_queue_status_t *pQueueStatus;
	snd_seq_queue_status_alloca(&pQueueStatus);
	if (snd_seq_get_queue_status(
			pAlsaSeq, iAlsaQueue, pQueueStatus) >= 0) {
		iQueueTime = snd_seq_queue_status_get_tick_time(pQueueStatus);
	}

	return iQueueTime;
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
	m_pAlsaSeq       = NULL;
	m_iAlsaClient    = -1;
	m_iAlsaQueue     = -1;
	m_iAlsaTimer     = 0;

	m_pAlsaSubsSeq   = NULL;
	m_iAlsaSubsPort  = -1;
	m_pAlsaNotifier  = NULL;

	m_pInputThread   = NULL;
	m_pOutputThread  = NULL;

	m_iDriftCheck    = 0;

	m_iTimeStart     = 0;
	m_iTimeDrift     = 0;
	m_iFrameStart    = 0;
	m_iFrameTime     = 0;

	m_bControlBus    = false;
	m_pIControlBus   = NULL;
	m_pOControlBus   = NULL;

	m_bMetronome         = false;
	m_bMetroBus          = false;
	m_pMetroBus          = NULL;
	m_iMetroChannel      = 9;	// GM Drums channel (10)
	m_iMetroBarNote      = 76;	// GM High-wood stick
	m_iMetroBarVelocity  = 96;
	m_iMetroBarDuration  = 48;
	m_iMetroBeatNote     = 77;	// GM Low-wood stick
	m_iMetroBeatVelocity = 64;
	m_iMetroBeatDuration = 24;
	m_bMetroEnabled      = false;

	// Time-scale cursor (tempo/time-signature map)
	m_pMetroCursor = NULL;

	// Track down tempo changes.
	m_fMetroTempo = 0.0f;

	// SMF player stuff.
	m_bPlayerBus = false;
	m_pPlayerBus = NULL;
	m_pPlayer    = NULL;

	// No input/capture quantization (default).
	m_iCaptureQuantize = 0;

	// MIDI controller mapping flagger.
	m_iResetAllControllers = 0;

	// MIDI MMC/SPP modes.
	m_mmcDevice = 0x7f; // All-caller-id.
	m_mmcMode = qtractorBus::Duplex;
	m_sppMode = qtractorBus::Duplex;

	// MIDI Clock mode.
	m_clockMode = qtractorBus::None;

	// MIDI Clock tempo tracking.
	m_iClockCount = 0;
	m_fClockTempo = 120.0f;
}


// Special event notifier proxy object.
const qtractorMidiEngineProxy *qtractorMidiEngine::proxy (void) const
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


// ALSA subscription port notifier.
QSocketNotifier *qtractorMidiEngine::alsaNotifier (void) const
{
	return m_pAlsaNotifier;
}


// ALSA subscription notifier acknowledgment.
void qtractorMidiEngine::alsaNotifyAck (void)
{
	if (m_pAlsaSubsSeq == NULL)
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
	// Pure conditional thread slave syncronization...
	if (m_pOutputThread && m_pOutputThread->midiCursorSync())
		m_pOutputThread->sync();
}


// Read ahead frames configuration.
void qtractorMidiEngine::setReadAhead ( unsigned int iReadAhead )
{
	if (m_pOutputThread)
		m_pOutputThread->setReadAhead(iReadAhead);
}

unsigned int qtractorMidiEngine::readAhead (void) const
{
	return (m_pOutputThread ? m_pOutputThread->readAhead() : 0);
}


// Reset queue tempo.
void qtractorMidiEngine::resetTempo (void)
{
	// It must be surely activated...
	if (!isActivated())
		return;

	// Needs a valid cursor...
	if (m_pMetroCursor == NULL)
		return;

	// Reset tempo cursor.
	m_pMetroCursor->reset();

	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Recache tempo node...
	qtractorTimeScale::Node *pNode
		= m_pMetroCursor->seekFrame(pSession->playHead());

	// Set queue tempo...
	snd_seq_queue_tempo_t *tempo;
	snd_seq_queue_tempo_alloca(&tempo);
	// Fill tempo struct with current tempo info.
	snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, tempo);
	// Set the new intended ones...
	snd_seq_queue_tempo_set_ppq(tempo, (int) pSession->ticksPerBeat());
	snd_seq_queue_tempo_set_tempo(tempo,
		(unsigned int) (60000000.0f / pNode->tempo));
	// Give tempo struct to the queue.
	snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, tempo);

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
	if (pSession == NULL)
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
}


// Reset all MIDI instrument/controllers...
void qtractorMidiEngine::resetAllControllers ( bool bForceImmediate )
{
	// Deferred processsing?
	if (!bForceImmediate) {
		++m_iResetAllControllers;
		return;
	}

	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
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
			qtractorMidiMonitor *pMidiMonitor
				= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
			if (pMidiBus && pMidiMonitor) {
				pMidiBus->setVolume(pTrack, pMidiMonitor->gain());
				pMidiBus->setPanning(pTrack, pMidiMonitor->panning());
			}
		}
	}

	// Re-send all mapped feedback MIDI controllers...
	qtractorMidiControl *pMidiControl
		= qtractorMidiControl::getInstance();
	if (pMidiControl)
		pMidiControl->sendAllControllers();

	// Done.
	m_iResetAllControllers = 0;
}


// Whether is actually pending a reset of
// all the MIDI instrument/controllers...
bool qtractorMidiEngine::isResetAllControllers (void) const
{
	return (m_iResetAllControllers > 0);
}


// Shut-off all MIDI tracks (panic)...
void qtractorMidiEngine::shutOffAllTracks (void) const
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::shutOffAllTracks()");
#endif

	QHash<qtractorMidiBus *, unsigned short> channels;

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi) {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->outputBus());
			if (pMidiBus) {
				unsigned short iChannel = pTrack->midiChannel();
				unsigned short iChannelMask = (1 << iChannel);
				unsigned short iChannelFlags = channels.value(pMidiBus, 0);
				if ((iChannelFlags & iChannelMask) == 0) {
					pMidiBus->setController(pTrack, ALL_SOUND_OFF);
					pMidiBus->setController(pTrack, ALL_NOTES_OFF);
					pMidiBus->setController(pTrack, ALL_CONTROLLERS_OFF);
					channels.insert(pMidiBus, iChannelFlags | iChannelMask);
				}
			}
		}
	}
}


// MIDI event capture method.
void qtractorMidiEngine::capture ( snd_seq_event_t *pEv )
{
	qtractorMidiEvent::EventType type;

	unsigned char  channel  = 0;
	unsigned short param    = 0;
	unsigned short value    = 0;
	unsigned long  duration = 0;

	unsigned char *pSysex   = NULL;
	unsigned short iSysex   = 0;

	const int iAlsaPort = pEv->dest.port;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// - capture quantization...
	if (m_iCaptureQuantize > 0) {
		const unsigned long q = pSession->ticksPerBeat() / m_iCaptureQuantize;
		pEv->time.tick = q * ((pEv->time.tick + (q >> 1)) / q);
	}

#ifdef CONFIG_DEBUG_0
	// - show event for debug purposes...
	fprintf(stderr, "MIDI In  %06lu 0x%02x", pEv->time.tick, pEv->type);
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
	case SND_SEQ_EVENT_NOTE:
	case SND_SEQ_EVENT_NOTEON:
		type     = qtractorMidiEvent::NOTEON;
		channel  = pEv->data.note.channel;
		param    = pEv->data.note.note;
		value    = pEv->data.note.velocity;
		duration = pEv->data.note.duration;
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
		duration = pEv->data.note.duration;
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
	//	param    = 0;
		value    = pEv->data.control.value;
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
			static QTime s_clockTime;
			if (++m_iClockCount == 1)
				s_clockTime.start();
			else
			if (m_iClockCount > 72) { // 3 beat averaging...
				m_iClockCount = 0;
				float fTempo = int(180000.0f / float(s_clockTime.elapsed()));
				if (::fabs(fTempo - m_fClockTempo) / m_fClockTempo > 0.01f) {
					m_fClockTempo = fTempo;
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

	// Now check which bus and track we're into...
	const bool bRecording = (pSession->isRecording() && isPlaying());
	const unsigned long iTime = m_iTimeStart + pEv->time.tick;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// Must be a MIDI track in capture/passthru
		// mode and for the intended channel...
		if (pTrack->trackType() == qtractorTrack::Midi
			&& (pTrack->isRecord() || pSession->isTrackMonitor(pTrack))
		//	&& !pTrack->isMute() && (!pSession->soloTracks() || pTrack->isSolo())
			&& pSession->isTrackMidiChannel(pTrack, channel)) {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->inputBus());
			if (pMidiBus && pMidiBus->alsaPort() == iAlsaPort) {
				// Is it actually recording?...
				if (pTrack->isRecord() && bRecording) {
					qtractorMidiClip *pMidiClip
						= static_cast<qtractorMidiClip *> (pTrack->clipRecord());
					if (pMidiClip // && tick >= pMidiClip->clipStartTime()
						&& (!pSession->isPunching()
							|| ((iTime >= pSession->punchInTime())
							&&  (iTime <  pSession->punchOutTime())))) {
						// Yep, we got a new MIDI event...
						qtractorMidiEvent *pEvent = new qtractorMidiEvent(
							pEv->time.tick, type, param, value, duration);
						if (pSysex)
							pEvent->setSysex(pSysex, iSysex);
						(pMidiClip->sequence())->addEvent(pEvent);
					}
				}
				// Track input monitoring...
				qtractorMidiMonitor *pMidiMonitor
					= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
				if (pMidiMonitor)
					pMidiMonitor->enqueue(type, value);
				// Output monitoring on record...
				if (pSession->isTrackMonitor(pTrack)) {
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
						if (!pMidiBus->isMonitor()
							&& pMidiBus->pluginList_out()
							&& (pMidiBus->pluginList_out())->midiManager())
							((pMidiBus->pluginList_out())->midiManager())->queued(
								pSession->timeScale(), pEv, iTime, m_iFrameStart);
						// Do it for the MIDI plugins too...
						if ((pTrack->pluginList())->midiManager())
							(pTrack->pluginList())->midiManager()->queued(
								pSession->timeScale(), pEv, iTime, m_iFrameStart);
						// FIXME: MIDI-thru channel filtering epilog...
						pEv->data.note.channel = iOldChannel;
					}
				}
			}
		}
	}

	// Bus monitoring...
	for (qtractorBus *pBus = buses().first(); pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus && pMidiBus->alsaPort() == iAlsaPort) {
			// Input monitoring...
			if (pMidiBus->midiMonitor_in())
				pMidiBus->midiMonitor_in()->enqueue(type, value);
			// Do it for the MIDI input plugins too...
			if (pMidiBus->pluginList_in()
				&& (pMidiBus->pluginList_in())->midiManager())
				((pMidiBus->pluginList_in())->midiManager())->queued(
					pSession->timeScale(), pEv, iTime, m_iFrameStart);
			// Output monitoring on passthru...
			if (pMidiBus->isMonitor()) {
				// Do it for the MIDI output plugins too...
				if (pMidiBus->pluginList_out()
					&& (pMidiBus->pluginList_out())->midiManager())
					((pMidiBus->pluginList_out())->midiManager())->queued(
						pSession->timeScale(), pEv, iTime, m_iFrameStart);
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
		}
	}

	// Trap controller commands...
	if (type == qtractorMidiEvent::SYSEX)
		return;

	if (m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
		// Post the stuffed event...
		m_proxy.notifyCtlEvent(
			qtractorCtlEvent(type, channel, param, value));
	}
}


// MIDI event enqueue method.
void qtractorMidiEngine::enqueue ( qtractorTrack *pTrack,
	qtractorMidiEvent *pEvent, unsigned long iTime, float fGain )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Target MIDI bus...
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == NULL)
		return;
#if 0
	// HACK: Ignore our own mixer-monitor supplied controllers...
	if (pEvent->type() == qtractorMidiEvent::CONTROLLER) {
		if (pEvent->controller() == CHANNEL_VOLUME ||
			pEvent->controller() == CHANNEL_PANNING)
			return;
	}
#endif
	// Scheduled delivery: take into account
	// the time playback/queue started...
	const unsigned long tick
		= (long(iTime) > m_iTimeStart ? iTime - m_iTimeStart : 0);

#ifdef CONFIG_DEBUG_0
	// - show event for debug purposes...
	fprintf(stderr, "MIDI Out %06lu 0x%02x", tick,
		(int) pEvent->type() | pTrack->midiChannel());
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

	// Intialize outbound event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);

	// Set Event tag...
	ev.tag = (unsigned char) (pTrack->midiTag() & 0xff);

	// Addressing...
	snd_seq_ev_set_source(&ev, pMidiBus->alsaPort());
	snd_seq_ev_set_subs(&ev);

	// Scheduled delivery...
	snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, tick);

	// Set proper event data...
	switch (pEvent->type()) {
		case qtractorMidiEvent::NOTEON:
			ev.type = SND_SEQ_EVENT_NOTE;
			ev.data.note.channel  = pTrack->midiChannel();
			ev.data.note.note     = pEvent->note();
			ev.data.note.velocity = int(fGain * float(pEvent->value())) & 0x7f;
			ev.data.note.duration = pEvent->duration();
			if (pSession->isLooping()) {
				unsigned long le = pSession->tickFromFrame(pSession->loopEnd());
				if (le < iTime + ev.data.note.duration)
					ev.data.note.duration = le - iTime;
			}
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
				break;
			case BANK_SELECT_LSB:
				if (pTrack->midiBank() >= 0)
					ev.data.control.value = (pTrack->midiBank() & 0x7f);
				break;
			case CHANNEL_VOLUME:
				ev.data.control.value = int(pTrack->gain() * float(pEvent->value())) & 0x7f;
				break;
			default:
				ev.data.control.value = pEvent->value();
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
			ev.data.control.value = pEvent->value();
			// HACK: Track properties override...
			if (pTrack->midiProg() >= 0)
				ev.data.control.value = pTrack->midiProg();
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
			if (pMidiBus->midiMonitor_out()) {
				// HACK: Master volume hack...
				unsigned char *data = pEvent->sysex();
				if (data[1] == 0x7f &&
					data[2] == 0x7f &&
					data[3] == 0x04 &&
					data[4] == 0x01) {
					// Make a copy, update and cache it while queued...
					pEvent = new qtractorMidiEvent(*pEvent);
					data = pEvent->sysex();
					data[5] = 0;
					data[6] = int(pMidiBus->midiMonitor_out()->gain() * float(data[6])) & 0x7f;
					m_sysexCache.append(pEvent);
				}
			}
			snd_seq_ev_set_sysex(&ev, pEvent->sysex_len(), pEvent->sysex());
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
	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if ((pTrack->pluginList())->midiManager())
		(pTrack->pluginList())->midiManager()->queued(
			pTimeScale, &ev, iTime, m_iFrameStart);
	// And for the MIDI output plugins as well...
	if (pMidiBus->pluginList_out()
		&& (pMidiBus->pluginList_out())->midiManager()) {
		((pMidiBus->pluginList_out())->midiManager())->queued(
			pTimeScale, &ev, iTime, m_iFrameStart);
	}
}


// Reset ouput queue drift stats (audio vs. MIDI)...
void qtractorMidiEngine::resetDrift (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiEngine::resetDrift()");
#endif

	m_iDriftCheck = 0;

//--DRIFT-SKEW-BEGIN--
	snd_seq_queue_tempo_t *pAlsaTempo;
	snd_seq_queue_tempo_alloca(&pAlsaTempo);
	snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pAlsaTempo);
	unsigned int iSkewBase = snd_seq_queue_tempo_get_skew_base(pAlsaTempo);
	snd_seq_queue_tempo_set_skew(pAlsaTempo, iSkewBase);
	snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pAlsaTempo);
//--DRIFT-SKEW-END--

	m_iTimeDrift = 0;
}


// Do ouput queue status (audio vs. MIDI)...
void qtractorMidiEngine::drift (void)
{
	if (++m_iDriftCheck < DRIFT_CHECK)
		return;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;
//	if (pSession->isRecording())
//		return;

	if (m_pMetroCursor == NULL)
		return;

	// Time to have some corrective approach...?
	snd_seq_queue_status_t *pQueueStatus;
	snd_seq_queue_status_alloca(&pQueueStatus);
	if (snd_seq_get_queue_status(
			m_pAlsaSeq, m_iAlsaQueue, pQueueStatus) >= 0) {
	//	unsigned long iAudioFrame = pSession->playHead();
		unsigned long iAudioFrame
			= pSession->audioEngine()->jackFrame() - m_iFrameTime;
		qtractorTimeScale::Node *pNode = m_pMetroCursor->seekFrame(iAudioFrame);
		long iAudioTime = long(pNode->tickFromFrame(iAudioFrame));
		long iMidiTime = m_iTimeStart
			+ long(snd_seq_queue_status_get_tick_time(pQueueStatus));
		long iDeltaTime = (iAudioTime - iMidiTime);
		if (iDeltaTime && iAudioTime > 0 && iMidiTime > m_iTimeDrift) {
		//--DRIFT-SKEW-BEGIN--
			snd_seq_queue_tempo_t *pAlsaTempo;
			snd_seq_queue_tempo_alloca(&pAlsaTempo);
			snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pAlsaTempo);
			unsigned int iSkewBase = snd_seq_queue_tempo_get_skew_base(pAlsaTempo);
			unsigned int iSkewPrev = snd_seq_queue_tempo_get_skew(pAlsaTempo);
			unsigned int iSkewNext = (unsigned int) (float(iSkewBase)
				* float(iAudioTime) / float(iMidiTime - m_iTimeDrift));
			if (iSkewNext != iSkewPrev) {
				snd_seq_queue_tempo_set_skew(pAlsaTempo, iSkewNext);
				snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, pAlsaTempo);
			}
		//--DRIFT-SKEW-END--
			m_iTimeDrift += iDeltaTime;
		//	m_iTimeDrift >>= 1; // Damp fast-average drift?
		#ifdef CONFIG_DEBUG_0
			qDebug("qtractorMidiEngine::drift(): "
				"iAudioTime=%ld iMidiTime=%ld (%ld) iTimeDrift=%ld (%.2g%%)",
				iAudioTime, iMidiTime, iDeltaTime, m_iTimeDrift,
				((100.0f * float(iSkewNext)) / float(iSkewBase)) - 100.0f);
		#endif
		}
	}

	// Restart counting...
	m_iDriftCheck = 0;
}


// Flush ouput queue (if necessary)...
void qtractorMidiEngine::flush (void)
{
	// Really flush MIDI output...
	snd_seq_drain_output(m_pAlsaSeq);
}


// Device engine initialization method.
bool qtractorMidiEngine::init (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Try open a new client...
	if (snd_seq_open(&m_pAlsaSeq, "default",
			SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0)
		return false;
	if (m_pAlsaSeq == NULL)
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
	// Open SMF player to last...
	openPlayerBus();

	// Open control/metronome buses, at least try...
	openControlBus();
	openMetroBus();

	// Create and start our own MIDI input queue thread...
	m_pInputThread = new qtractorMidiInputThread(this);
	m_pInputThread->start(QThread::TimeCriticalPriority);

	// Create and start our own MIDI output queue thread...
	unsigned int iReadAhead = (session()->sampleRate() >> 1);
	m_pOutputThread = new qtractorMidiOutputThread(this, iReadAhead);
	m_pOutputThread->start(QThread::HighPriority);

	// Reset/zero tickers...
	m_iTimeStart = 0;

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
	if (pSession == NULL)
		return false;

	// Output thread must be around too...
	if (m_pOutputThread == NULL)
		return false;

	// Close any SMF player out there...
	closePlayer();

	// Initial output thread bumping...
	qtractorSessionCursor *pMidiCursor
		= m_pOutputThread->midiCursorSync(true);
	if (pMidiCursor == NULL)
		return false;

	// Reset all dependables...
	resetTempo();
	resetAllMonitors();

	// Reset output queue drift compensator...
	resetDrift();

	// Start queue timer...
	m_iFrameStart = long(pMidiCursor->frame());
	m_iTimeStart  = long(pSession->tickFromFrame(m_iFrameStart));
	m_iFrameTime  = long(pSession->audioEngine()->jackFrame()) - m_iFrameStart;
	
	// Effectively start sequencer queue timer...
	snd_seq_start_queue(m_pAlsaSeq, m_iAlsaQueue, NULL);
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
	snd_seq_stop_queue(m_pAlsaSeq, m_iAlsaQueue, NULL);
	snd_seq_drain_output(m_pAlsaSeq);

	// Shut-off all MIDI buses...
	for (qtractorBus *pBus = qtractorEngine::buses().first();
			pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus)
			pMidiBus->shutOff();
	}
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
		m_pOutputThread = NULL;
		m_iTimeStart = 0;
		m_iTimeDrift = 0;
	}

	// Last but not least, delete input thread...
	if (m_pInputThread) {
		// Make it nicely...
		if (m_pInputThread->isRunning()) do {
			m_pInputThread->setRunState(false);
		//	m_pInputThread->terminate();
		} while (!m_pInputThread->wait(100));
		delete m_pInputThread;
		m_pInputThread = NULL;
	}

	// Time-scale cursor (tempo/time-signature map)
	if (m_pMetroCursor) {
		delete m_pMetroCursor;
		m_pMetroCursor = NULL;
	}

	// Drop subscription stuff.
	if (m_pAlsaSubsSeq) {
		if (m_pAlsaNotifier) {
			delete m_pAlsaNotifier;
			m_pAlsaNotifier = NULL;
		}
		if (m_iAlsaSubsPort >= 0) {
			snd_seq_delete_simple_port(m_pAlsaSubsSeq, m_iAlsaSubsPort);
			m_iAlsaSubsPort = -1;
		}
		snd_seq_close(m_pAlsaSubsSeq);
		m_pAlsaSubsSeq = NULL;
	}

	// Drop everything else, finally.
	if (m_pAlsaSeq) {
		// And now, the sequencer queue and handle...
		snd_seq_free_queue(m_pAlsaSeq, m_iAlsaQueue);
		snd_seq_close(m_pAlsaSeq);
		m_iAlsaQueue  = -1;
		m_iAlsaClient = -1;
		m_pAlsaSeq    = NULL;
	}

	// Clean any other left-overs...
	clearSysexCache();
}


// Special rewind method, for queue loop.
void qtractorMidiEngine::restartLoop (void)
{
	qtractorSession *pSession = session();
	if (pSession && pSession->isLooping()) {
		unsigned long iLoopStart = pSession->loopStart();
		unsigned long iLoopEnd = pSession->loopEnd();
		long iLoopLength = long(iLoopEnd - iLoopStart);
		m_iFrameTime  += iLoopLength;
		m_iFrameStart -= iLoopLength;
		m_iTimeStart  -= long(pSession->tickFromFrame(iLoopEnd)
			- pSession->tickFromFrame(iLoopStart));
	//	m_iTimeStart += m_iTimeDrift; -- Drift correction?
		resetDrift();
	}
}


// The delta-time/frame accessors.
long qtractorMidiEngine::timeStart (void) const
{
	return m_iTimeStart;
}


long qtractorMidiEngine::frameStart (void) const
{
	return m_iFrameStart;
}


// Immediate track mute.
void qtractorMidiEngine::trackMute ( qtractorTrack *pTrack, bool bMute )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEngine::trackMute(%p, %d)", pTrack, bMute);
#endif

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	unsigned long iFrame = pSession->playHead();

	if (bMute) {
		// Remove all already enqueued events
		// for the given track and channel...
		snd_seq_remove_events_t *pre;
		snd_seq_remove_events_alloca(&pre);
		snd_seq_timestamp_t ts;
		unsigned long iTime = pSession->tickFromFrame(iFrame);
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
	if (pSession == NULL)
		return;

	unsigned long iFrame = pSession->playHead();

	if (bMute) {
		// Remove all already enqueued events
		// for the given track and channel...
		snd_seq_remove_events_t *pre;
 		snd_seq_remove_events_alloca(&pre);
		snd_seq_timestamp_t ts;
		unsigned long iTime = pSession->tickFromFrame(iFrame);
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
	deleteControlBus();

	m_bControlBus = bControlBus;

	createControlBus();

	if (isActivated())
		openControlBus();
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
			if (m_pIControlBus == NULL
				&& (pBus->busMode() & qtractorBus::Input))
				m_pIControlBus = static_cast<qtractorMidiBus *> (pBus);
			if (m_pOControlBus == NULL
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
	if (m_pOControlBus == NULL)
		createControlBus();
	if (m_pOControlBus == NULL)
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
	m_pIControlBus = NULL;
	m_pOControlBus = NULL;
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
	deletePlayerBus();

	m_bPlayerBus = bPlayerBus;

	createPlayerBus();

	if (isActivated())
		openPlayerBus();
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
		for (qtractorBus *pBus = qtractorEngine::buses().first();
				pBus; pBus = pBus->next()) {
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
	if (m_pPlayerBus == NULL)
		createPlayerBus();
	if (m_pPlayerBus == NULL)
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
		m_pPlayer = NULL;
	}

	if (m_pPlayerBus && m_bPlayerBus)
		delete m_pPlayerBus;

	m_pPlayerBus = NULL;
}


// Tell whether audition/pre-listening is active...
bool qtractorMidiEngine::isPlayerOpen (void) const
{
	return (m_pPlayer ? m_pPlayer->isOpen() : false);
}


// Open and start audition/pre-listening...
bool qtractorMidiEngine::openPlayer ( const QString& sFilename, int iTrackChannel )
{
	if (isPlaying())
		return false;

	m_iFrameStart = 0;
	m_iTimeStart  = 0;

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

void qtractorMidiEngine::sendMmcMaskedWrite ( qtractorMmcEvent::SubCommand scmd,
	int iTrack,	bool bOn ) const
{
	unsigned char data[4];
	int iMask = (1 << (iTrack < 2 ? iTrack + 5 : (iTrack - 2) % 7));

	data[0] = scmd;
	data[1] = (unsigned char) (iTrack < 2 ? 0 : 1 + (iTrack - 2) / 7);
	data[2] = (unsigned char) iMask;
	data[3] = (unsigned char) (bOn ? iMask : 0);

	sendMmcCommand(qtractorMmcEvent::MASKED_WRITE, data, sizeof(data));
}

void qtractorMidiEngine::sendMmcCommand ( qtractorMmcEvent::Command cmd,
	unsigned char *pMmcData, unsigned short iMmcData ) const
{
	// Do we have MMC output enabled?
	if ((m_mmcMode & qtractorBus::Output) == 0)
		return;

	// We surely need a output control bus...
	if (m_pOControlBus == NULL)
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
void qtractorMidiEngine::sendSppCommand ( int iCmdType, unsigned short iSongPos ) const
{
	// Do we have SPP output enabled?
	if ((m_sppMode & qtractorBus::Output) == 0)
		return;

	// We surely need a output control bus...
	if (m_pOControlBus == NULL)
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
	deleteMetroBus();

	m_bMetroBus = bMetroBus;

	createMetroBus();

	if (isActivated())
		openMetroBus();
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
		for (qtractorBus *pBus = qtractorEngine::buses().first();
				pBus; pBus = pBus->next()) {
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
	if (m_pMetroBus == NULL)
		createMetroBus();
	if (m_pMetroBus == NULL)
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

	m_pMetroBus = NULL;
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


// Process metronome clicks.
void qtractorMidiEngine::processMetro (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (m_pMetroCursor == NULL)
		return;

	qtractorTimeScale::Node *pNode = m_pMetroCursor->seekFrame(iFrameEnd);

	// Take this moment to check for tempo changes...
	if (pNode->tempo != m_fMetroTempo) {
		// New tempo node...
		unsigned long iTime = (pNode->frame < iFrameStart
			? pNode->tickFromFrame(iFrameStart) : pNode->tick);
		// Enqueue tempo event...
		snd_seq_event_t ev;
		snd_seq_ev_clear(&ev);
		// Scheduled delivery: take into account
		// the time playback/queue started...
		unsigned long tick
			= ((long) iTime > m_iTimeStart ? iTime - m_iTimeStart : 0);
		snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, tick);
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

	// Get on with the actual metronome/clock stuff...
	if (!m_bMetronome && (m_clockMode & qtractorBus::Output) == 0)
		return;

	// Register the next metronome/clock beat slot.
	unsigned long iTimeEnd = pNode->tickFromFrame(iFrameEnd);

	pNode = m_pMetroCursor->seekFrame(iFrameStart);
	unsigned long iTimeStart = pNode->tickFromFrame(iFrameStart);
	unsigned int  iBeat = pNode->beatFromTick(iTimeStart);
	unsigned long iTime = pNode->tickFromBeat(iBeat);

	// Intialize outbound metronome event...
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

	// Intialize outbound clock event...
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
			unsigned int iTicksPerClock = pNode->ticksPerBeat / 24;
			for (unsigned int iClock = 0; iClock < 24; ++iClock) {
				if (iTimeClock >= iTimeEnd)
					break;
				if (iTimeClock >= iTimeStart) {
					unsigned long tick
						= (long(iTimeClock) > m_iTimeStart ? iTimeClock - m_iTimeStart : 0);
					snd_seq_ev_schedule_tick(&ev_clock, m_iAlsaQueue, 0, tick);
					snd_seq_event_output(m_pAlsaSeq, &ev_clock);
				}
				iTimeClock += iTicksPerClock;
			}
		}
		if (m_bMetronome && iTime >= iTimeStart) {
			unsigned long tick
				= (long(iTime) > m_iTimeStart ? iTime - m_iTimeStart : 0);
			snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, tick);
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
			if (m_pMetroBus->midiMonitor_out()) {
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


// Document element methods.
bool qtractorMidiEngine::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	qtractorEngine::clear();

	createControlBus();
	createMetroBus();

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
	}

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
	for (qtractorBus *pBus = qtractorEngine::buses().first();
			pBus; pBus = pBus->next()) {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
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

	return true;
}


// MIDI-export method.
bool qtractorMidiEngine::fileExport ( const QString& sExportPath,
	const QList<qtractorMidiBus *>& exportBuses,
	unsigned long iExportStart, unsigned long iExportEnd )
{
	// No simultaneous or foul exports...
	if (isPlaying())
		return false;

	// Make sure we have an actual session cursor...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Cannot have exports longer than current session.
	if (iExportStart >= iExportEnd)
		iExportEnd = pSession->sessionEnd();
	if (iExportStart >= iExportEnd)
		return false;

	unsigned short iTicksPerBeat = pSession->ticksPerBeat();

	unsigned long iTimeStart = pSession->tickFromFrame(iExportStart);
	unsigned long iTimeEnd   = pSession->tickFromFrame(iExportEnd);

	unsigned short iFormat = qtractorMidiClip::defaultFormat();

	unsigned short iSeq;
	unsigned short iSeqs = 0;
	QList<qtractorMidiSequence *> seqs;
	qtractorMidiSequence **ppSeqs = NULL;
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
		if (pMidiBus == NULL)
			continue;
		// Check whether this track makes it
		// as one of the exported buses....
		qtractorMidiBus *pExportBus = NULL;
		bus_iter.toFront();
		while (bus_iter.hasNext()) {
			pExportBus = bus_iter.next();
			if (pExportBus
				&& pExportBus->alsaPort() == pMidiBus->alsaPort())
				break;
			pExportBus = NULL;
		}
		// Is it not?
		if (pExportBus == NULL)
			continue;
		// We have a target sequence, maybe reused...
		qtractorMidiSequence *pSeq;
		if (ppSeqs) {
			// SMF Format 0
			pSeq = ppSeqs[pTrack->midiChannel() & 0x0f];
			QString sName = pSeq->name();
			if (!sName.isEmpty())
				sName += "; ";
			pSeq->setName(sName + pTrack->trackName());
		} else {
			// SMF Format 1
			++iTracks;
			pSeq = new qtractorMidiSequence(
				pTrack->trackName(), iTracks, iTicksPerBeat);
			pSeq->setChannel(pTrack->midiChannel());
			seqs.append(pSeq);
		}
		// Make this track setup...
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
				unsigned long iTimeClip
					= pSession->tickFromFrame(pClip->clipStart());
				unsigned long iTimeOffset = iTimeClip - iTimeStart;
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
						unsigned long iTimeEvent = iTimeClip + pEvent->time();
						float fGain = pMidiClip->gain(
							pSession->frameFromTick(iTimeEvent)
							- pClip->clipStart());
						pNewEvent->setVelocity((unsigned char)
							(fGain * float(pEvent->velocity())) & 0x7f);
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
	if (ppSeqs == NULL) {
		// Sanity check...
		if (iTracks < 1)
			return false;
		// Number of actual track sequences...
		iSeqs  = iTracks;
		ppSeqs = new qtractorMidiSequence * [iSeqs];
		QListIterator<qtractorMidiSequence *> seq_iter(seqs);
		ppSeqs[0] = NULL;	// META info track...
		for (iSeq = 1; iSeq < iSeqs && seq_iter.hasNext(); ++iSeq)
			ppSeqs[iSeq] = seq_iter.next();
		// May clear it now.
		seqs.clear();
	}

	// Prepare file for writing...
	qtractorMidiFile file;
	// File ready for export?
	bool bResult = file.open(sExportPath, qtractorMidiFile::Write);
	if (bResult) {
		if (file.writeHeader(iFormat, iTracks, iTicksPerBeat)) {
			// Export SysEx setups...
			bus_iter.toFront();
			while (bus_iter.hasNext()) {
				qtractorMidiBus *pExportBus = bus_iter.next();
				qtractorMidiSysexList *pSysexList = pExportBus->sysexList();
				if (pSysexList && pSysexList->count() > 0) {
					if (ppSeqs[0] == NULL) {
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
	int iUpdate = qtractorEngine::updateConnects();

	// Reset all pending controllers, if any...
	if (m_iResetAllControllers > 0)
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


// Free overriden SysEx queued events.
void qtractorMidiEngine::clearSysexCache (void)
{
	qDeleteAll(m_sysexCache);
	m_sysexCache.clear();
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
		m_pICurveFile   = new qtractorCurveFile(m_pIPluginList->curveList());
	} else {
		m_pIMidiMonitor = NULL;
		m_pIPluginList  = NULL;
		m_pICurveFile   = NULL;
	}

	if ((busMode & qtractorBus::Output) && !(busMode & qtractorBus::Ex)) {
		m_pOMidiMonitor = new qtractorMidiMonitor();
		m_pOPluginList  = createPluginList(qtractorPluginList::MidiOutBus);
		m_pOCurveFile   = new qtractorCurveFile(m_pOPluginList->curveList());
		m_pSysexList    = new qtractorMidiSysexList();
	} else {
		m_pOMidiMonitor = NULL;
		m_pOPluginList  = NULL;
		m_pOCurveFile   = NULL;
		m_pSysexList    = NULL;
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

	if (m_pICurveFile)
		delete m_pICurveFile;
	if (m_pOCurveFile)
		delete m_pOCurveFile;

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
	if (pMidiEngine == NULL)
		return false;
	if (pMidiEngine->alsaSeq() == NULL)
		return false;

	// The verry same port might be used for input and output...
	unsigned int flags = 0;

	if (busMode() & qtractorBus::Input)
		flags |= SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
	if (busMode() & qtractorBus::Output)
		flags |= SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;

	m_iAlsaPort = snd_seq_create_simple_port(
		pMidiEngine->alsaSeq(), busName().toUtf8().constData(), flags,
		SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

	if (m_iAlsaPort < 0)
		return false;

	// We want to know when the events get delivered to us...
	snd_seq_port_info_t *pinfo;
	snd_seq_port_info_alloca(&pinfo);

	if (snd_seq_get_port_info(pMidiEngine->alsaSeq(), m_iAlsaPort, pinfo) < 0)
		return false;

	snd_seq_port_info_set_timestamping(pinfo, 1);
	snd_seq_port_info_set_timestamp_queue(pinfo, pMidiEngine->alsaQueue());
	snd_seq_port_info_set_timestamp_real(pinfo, 0);	// MIDI ticks.

	if (snd_seq_set_port_info(pMidiEngine->alsaSeq(), m_iAlsaPort, pinfo) < 0)
		return false;

	// Plugin lists need some buffer (re)allocation too...
	if (m_pIPluginList)
		updatePluginList(m_pIPluginList, qtractorPluginList::MidiInBus);
	if (m_pOPluginList)
		updatePluginList(m_pOPluginList, qtractorPluginList::MidiOutBus);

	// Done.
	return true;
}


// Unregister and post-free bus port buffers.
void qtractorMidiBus::close (void)
{
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;
	if (pMidiEngine->alsaSeq() == NULL)
		return;

	shutOff(true);

	snd_seq_delete_simple_port(pMidiEngine->alsaSeq(), m_iAlsaPort);

	m_iAlsaPort = -1;
}


// Bus mode change event.
void qtractorMidiBus::updateBusMode (void)
{
	const qtractorBus::BusMode mode = busMode();

	// Have a new/old input monitor?
	if ((mode & qtractorBus::Input) && !(mode & qtractorBus::Ex)) {
		if (m_pIMidiMonitor == NULL)
			m_pIMidiMonitor = new qtractorMidiMonitor();
		if (m_pIPluginList == NULL)
			m_pIPluginList = createPluginList(qtractorPluginList::MidiInBus);
		if (m_pICurveFile == NULL)
			m_pICurveFile = new qtractorCurveFile(m_pIPluginList->curveList());
	} else {
		if (m_pIMidiMonitor) {
			delete m_pIMidiMonitor;
			m_pIMidiMonitor = NULL;
		}
		if (m_pICurveFile) {
			delete m_pICurveFile;
			m_pICurveFile = NULL;
		}
		if (m_pIPluginList) {
			delete m_pIPluginList;
			m_pIPluginList = NULL;
		}
	}

	// Have a new/old output monitor?
	if ((mode & qtractorBus::Output) && !(mode & qtractorBus::Ex)) {
		if (m_pOMidiMonitor == NULL)
			m_pOMidiMonitor = new qtractorMidiMonitor();
		if (m_pOPluginList == NULL)
			m_pOPluginList = createPluginList(qtractorPluginList::MidiOutBus);
		if (m_pOCurveFile == NULL)
			m_pOCurveFile = new qtractorCurveFile(m_pOPluginList->curveList());
		if (m_pSysexList == NULL)
			m_pSysexList = new qtractorMidiSysexList();
	} else {
		if (m_pOMidiMonitor) {
			delete m_pOMidiMonitor;
			m_pOMidiMonitor = NULL;
		}
		if (m_pOCurveFile) {
			delete m_pOCurveFile;
			m_pOCurveFile = NULL;
		}
		if (m_pOPluginList) {
			delete m_pOPluginList;
			m_pOPluginList = NULL;
		}
		if (m_pSysexList) {
			delete m_pSysexList;
			m_pSysexList = NULL;
		}
	}
}


// Shut-off everything out there.
void qtractorMidiBus::shutOff ( bool bClose ) const
{
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;
	if (pMidiEngine->alsaSeq() == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::shutOff(%d)", this, int(bClose));
#endif

	QHash<unsigned short, Patch>::ConstIterator iter
		= m_patches.constBegin();
	const QHash<unsigned short, Patch>::ConstIterator& iter_end
		= m_patches.constEnd();
	for ( ; iter != iter_end; ++iter) {
		unsigned short iChannel = iter.key();
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
	if (pMidiEngine == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiBus[%p]::setPatch(%d, \"%s\", %d, %d, %d)",
		this, iChannel, sInstrumentName.toUtf8().constData(),
		iBankSelMethod, iBank, iProg);
#endif

	// Sanity check.
	if (sInstrumentName.isEmpty())
		m_patches.remove(iChannel & 0x0f);

	if (iProg < 0)
		return;

	// Update patch mapping...
	Patch& patch = m_patches[iChannel & 0x0f];
	patch.instrumentName = sInstrumentName;
	patch.bankSelMethod  = iBankSelMethod;
	patch.bank = iBank;
	patch.prog = iProg;

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
		return;

	// Do it for the MIDI plugins if applicable...
	qtractorMidiManager *pTrackMidiManager = NULL;
	if (pTrack)
		pTrackMidiManager = (pTrack->pluginList())->midiManager();

	qtractorMidiManager *pBusMidiManager = NULL;
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
		snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);
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
		snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);
		if (pTrackMidiManager)
			pTrackMidiManager->direct(&ev);
		if (pBusMidiManager)
			pBusMidiManager->direct(&ev);
	}

	// Program change...
	ev.type = SND_SEQ_EVENT_PGMCHANGE;
	ev.data.control.channel = iChannel;
	ev.data.control.value   = iProg;
	snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);
	if (pTrackMidiManager)
		pTrackMidiManager->direct(&ev);
	if (pBusMidiManager)
		pBusMidiManager->direct(&ev);

//	pMidiEngine->flush();
}


// Direct MIDI controller helper.
void qtractorMidiBus::setController ( qtractorTrack *pTrack,
	int iController, int iValue ) const
{
	setControllerEx(pTrack->midiChannel(), iController, iValue, pTrack);
}

void qtractorMidiBus::setController ( unsigned short iChannel,
	int iController, int iValue ) const
{
	setControllerEx(iChannel, iController, iValue, NULL);
}


// Direct MIDI controller common helper.
void qtractorMidiBus::setControllerEx ( unsigned short iChannel,
	int iController, int iValue, qtractorTrack *pTrack ) const
{
	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::setController(%d, %d, %d)",
		this, iChannel, iController, iValue);
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
	snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);

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
	if (pMidiEngine == NULL)
		return;

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
		return;

#ifdef CONFIG_DEBUG
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
		ev.data.control.param   = 0;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::CHANPRESS:
		ev.type = SND_SEQ_EVENT_CHANPRESS;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = 0;
		ev.data.control.value   = iValue;
		break;
	case qtractorMidiEvent::PITCHBEND:
		ev.type = SND_SEQ_EVENT_PITCHBEND;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = 0;
		ev.data.control.value   = int(iValue) - 0x2000;
		break;
	}

	snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);
}


// Direct MIDI note on/off helper.
void qtractorMidiBus::sendNote ( qtractorTrack *pTrack,
	int iNote, int iVelocity ) const
{
	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
		return;

	unsigned short iChannel = pTrack->midiChannel();
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
	snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);

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
}


// Direct SysEx helpers.
void qtractorMidiBus::sendSysex ( unsigned char *pSysex, unsigned int iSysex ) const
{
	// Yet again, we need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
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
	snd_seq_event_output_direct(pMidiEngine->alsaSeq(), &ev);

//	pMidiEngine->flush();
}


void qtractorMidiBus::sendSysexList (void) const
{
	// Check that we have some SysEx for setup...
	if (m_pSysexList == NULL)
		return;
	if (m_pSysexList->count() < 1)
		return;

	// Yet again, we need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
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
		snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);
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
	qtractorSession *pSession = engine()->session();
	if (pSession == NULL)
		return NULL;

	// Create plugin-list alright...
	unsigned int iSampleRate = 0;
	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine)
		iSampleRate = pAudioEngine->sampleRate();

	qtractorPluginList *pPluginList
		= new qtractorPluginList(0, 0, iSampleRate, iFlags);

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
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	// Set plugin-list title name...
	updatePluginListName(pPluginList, iFlags);

	// Get audio bus as for the plugin list...
	qtractorAudioBus *pAudioBus = NULL;
	if (pPluginList->midiManager())
		pAudioBus = (pPluginList->midiManager())->audioOutputBus();
	if (pAudioBus == NULL) {
		// Output bus gets to be the first available output bus...
		for (qtractorBus *pBus = (pAudioEngine->buses()).first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				pAudioBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}
	}

	// Got it?
	if (pAudioBus == NULL)
		return;

	// Set plugin-list buffer alright...
	pPluginList->setBuffer(pAudioBus->channels(),
		pAudioEngine->bufferSize(), pAudioEngine->sampleRate(), iFlags);
}


// Automation curve list accessors.
qtractorCurveList *qtractorMidiBus::curveList_in (void) const
{
	return (m_pIPluginList ? m_pIPluginList->curveList() : NULL);
}

qtractorCurveList *qtractorMidiBus::curveList_out (void) const
{
	return (m_pOPluginList ? m_pOPluginList->curveList() : NULL);
}


// Automation curve serializer accessors.
qtractorCurveFile *qtractorMidiBus::curveFile_in (void) const
{
	return m_pICurveFile;
}

qtractorCurveFile *qtractorMidiBus::curveFile_out (void) const
{
	return m_pOCurveFile;
}


// Retrieve all current ALSA connections for a given bus mode interface;
// return the effective number of connection attempts...
int qtractorMidiBus::updateConnects (
	qtractorBus::BusMode busMode, ConnectList& connects, bool bConnect ) const
{
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return 0;
	if (pMidiEngine->alsaSeq() == NULL)
		return 0;

	// Modes must match, at least...
	if ((busMode & qtractorMidiBus::busMode()) == 0)
		return 0;
	if (bConnect && connects.isEmpty())
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

	while (snd_seq_query_next_client(
			pMidiEngine->alsaSeq(), pClientInfo) >= 0) {
		item.client = snd_seq_client_info_get_client(pClientInfo);
		item.clientName = QString::fromUtf8(
			snd_seq_client_info_get_name(pClientInfo));
		snd_seq_port_info_set_client(pPortInfo, item.client);
		snd_seq_port_info_set_port(pPortInfo, -1);
		while (snd_seq_query_next_port(
				pMidiEngine->alsaSeq(), pPortInfo) >= 0) {
			unsigned int iPortCapability
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
	while (snd_seq_query_port_subscribers(
			pMidiEngine->alsaSeq(), pAlsaSubs) >= 0) {
		seq_addr = *snd_seq_query_subscribe_get_addr(pAlsaSubs);
		snd_seq_get_any_client_info(
			pMidiEngine->alsaSeq(), seq_addr.client, pClientInfo);
		item.client = seq_addr.client;
		item.clientName = QString::fromUtf8(
			snd_seq_client_info_get_name(pClientInfo));
		snd_seq_get_any_port_info(
			pMidiEngine->alsaSeq(), seq_addr.client, seq_addr.port, pPortInfo);
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
		if (snd_seq_subscribe_port(pMidiEngine->alsaSeq(), pPortSubs) == 0) {
			int iItem = connects.indexOf(pItem);
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
	unsigned char vol = (unsigned char) (int(127.0f * fVolume) & 0x7f);
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
	unsigned char pan = (unsigned char) ((0x40 + int(63.0f * fPanning)) & 0x7f);
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
	unsigned char vol = (unsigned char) (int(127.0f * fVolume) & 0x7f);
	setController(pTrack, CHANNEL_VOLUME, vol);
}


// MIDI channel stereo panning.
void qtractorMidiBus::setPanning ( qtractorTrack *pTrack, float fPanning )
{
	unsigned char pan = (unsigned char) ((0x40 + int(63.0f * fPanning)) & 0x7f);
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
		} else if (eProp.tagName() == "input-curve-file") {
			qtractorMidiBus::loadCurveFile(&eProp, qtractorBus::Input,
				qtractorMidiBus::curveFile_in());
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
		} else if (eProp.tagName() == "output-curve-file") {
			qtractorMidiBus::loadCurveFile(&eProp, qtractorBus::Output,
				qtractorMidiBus::curveFile_out());
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
	pElement->setAttribute("name",
		qtractorMidiBus::busName());
	pElement->setAttribute("mode",
		qtractorBus::textFromBusMode(qtractorMidiBus::busMode()));

	pDocument->saveTextElement("monitor",
		qtractorDocument::textFromBool(
			qtractorMidiBus::isMonitor()), pElement);

	if (qtractorMidiBus::busMode() & qtractorBus::Input) {
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
		// Save input bus automation curves...
		qtractorCurveList *pInputCurveList = qtractorMidiBus::curveList_in();
		if (pInputCurveList && !pInputCurveList->isEmpty()) {
			qtractorCurveFile cfile(pInputCurveList);
			QDomElement eInputCurveFile
				= pDocument->document()->createElement("input-curve-file");
			qtractorMidiBus::saveCurveFile(pDocument,
				&eInputCurveFile, qtractorBus::Input, &cfile);
			pElement->appendChild(eInputCurveFile);
		}
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

	if (qtractorMidiBus::busMode() & qtractorBus::Output) {
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
		// Save output bus automation curves...
		qtractorCurveList *pOutputCurveList = qtractorMidiBus::curveList_out();
		if (pOutputCurveList && !pOutputCurveList->isEmpty()) {
			qtractorCurveFile cfile(pOutputCurveList);
			QDomElement eOutputCurveFile
				= pDocument->document()->createElement("output-curve-file");
			qtractorMidiBus::saveCurveFile(pDocument,
				&eOutputCurveFile, qtractorBus::Output, &cfile);
			pElement->appendChild(eOutputCurveFile);
		}
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

	// Save default intrument name, if any...
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
			unsigned short iChannel = eChild.attribute("channel").toUShort();
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
			if (patch.instrumentName.isEmpty())
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
	if (m_pSysexList == NULL)
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
	if (m_pSysexList == NULL)
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
	if (m_pSysexList == NULL)
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
	if (m_pSysexList == NULL)
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


// end of qtractorMidiEngine.cpp
