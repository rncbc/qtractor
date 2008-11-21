// qtractorMidiEngine.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSessionCursor.h"
#include "qtractorSessionDocument.h"
#include "qtractorAudioEngine.h"

#include "qtractorMidiSequence.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiBuffer.h"

#include "qtractorPlugin.h"

#include <QApplication>

#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <QSocketNotifier>


// Specific controller definitions
#define BANK_SELECT_MSB		0x00
#define BANK_SELECT_LSB		0x20

#define ALL_SOUND_OFF		0x78
#define ALL_CONTROLLERS_OFF	0x79
#define ALL_NOTES_OFF		0x7b

#define CHANNEL_VOLUME		0x07
#define CHANNEL_PANNING		0x0a


//----------------------------------------------------------------------
// class qtractorMidiInputThread -- MIDI input thread (singleton).
//

class qtractorMidiInputThread : public QThread
{
public:

	// Constructor.
	qtractorMidiInputThread(qtractorSession *pSession);

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
	qtractorSession *m_pSession;

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
	qtractorMidiOutputThread(qtractorSession *pSession,
		unsigned int iReadAhead = 0);

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
	void trackClipSync(qtractorTrack *pTrack,
		unsigned long iFrameStart, unsigned long iFrameEnd);

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
	qtractorSession *m_pSession;

	// The number of frames to read-ahead.
	unsigned int m_iReadAhead;

	// Whether the thread is logically running.
	bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;
};


//----------------------------------------------------------------------
// class qtractorMidiInputThread -- MIDI input thread (singleton).
//

// Constructor.
qtractorMidiInputThread::qtractorMidiInputThread (
	qtractorSession *pSession ) : QThread()
{
	m_pSession  = pSession;
	m_bRunState = false;
}


// Destructor.
qtractorMidiInputThread::~qtractorMidiInputThread (void)
{
	// Try to terminate executive thread,
	// but give it a bit of time to cleanup...
	if (isRunning()) do {
		setRunState(false);
	//	terminate();
	} while (wait(100));
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
	snd_seq_t *pAlsaSeq = m_pSession->midiEngine()->alsaSeq();
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

	m_bRunState = true;

	int iPoll = 0;
	while (m_bRunState && iPoll >= 0) {
		// Wait for events...
		iPoll = poll(pfds, nfds, 200);
		while (iPoll > 0) {
			snd_seq_event_t *pEv = NULL;
			snd_seq_event_input(pAlsaSeq, &pEv);
			// Process input event - ...
			// - enqueue to input track mapping;
			m_pSession->midiEngine()->capture(pEv);
		//	snd_seq_free_event(pEv);
			iPoll = snd_seq_event_input_pending(pAlsaSeq, 0);
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
	qtractorSession *pSession, unsigned int iReadAhead ) : QThread()
{
	if (iReadAhead < 1)
		iReadAhead = pSession->sampleRate();

	m_pSession   = pSession;
	m_bRunState  = false;
	m_iReadAhead = iReadAhead;
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
		= m_pSession->audioEngine()->sessionCursor();
	if (pAudioCursor == NULL)
		return NULL;

	// And to our slave MIDI engine too...
	qtractorSessionCursor *pMidiCursor
		= m_pSession->midiEngine()->sessionCursor();
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
		if (m_pSession->isPlaying()) {
			m_mutex.unlock();
			process();
			m_mutex.lock();
		}
	}
	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::run(): stopped.", this);
#endif
}


// MIDI output process cycle iteration.
void qtractorMidiOutputThread::process (void)
{
	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	// Get a handle on our slave MIDI engine...
	qtractorSessionCursor *pMidiCursor = midiCursorSync();
	// Isn't MIDI slightly behind audio?
	if (pMidiCursor == NULL)
		return;

	// Now for the next readahead bunch...
	unsigned long iFrameStart = pMidiCursor->frame();
	unsigned long iFrameEnd   = iFrameStart + m_iReadAhead;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::process(%lu, %lu)",
		this, iFrameStart, iFrameEnd);
#endif

	// Split processing, in case we're looping...
	if (m_pSession->isLooping() && iFrameStart < m_pSession->loopEnd()) {
		// Loop-length might be shorter than the read-ahead...
		while (iFrameEnd >= m_pSession->loopEnd()) {
			// Process the remaining until end-of-loop...
			m_pSession->process(pMidiCursor, iFrameStart, m_pSession->loopEnd());
			// Process metronome clicks...
			pMidiEngine->processMetro(iFrameStart, m_pSession->loopEnd());
			// Reset to start-of-loop...
			iFrameStart = m_pSession->loopStart();
			iFrameEnd   = iFrameStart + (iFrameEnd - m_pSession->loopEnd());
			pMidiCursor->seek(iFrameStart);
			// This is really a must...
			pMidiEngine->restartLoop();
		}
	}

	// Regular range...
	m_pSession->process(pMidiCursor, iFrameStart, iFrameEnd);
	// Process metronome clicks...
	pMidiEngine->processMetro(iFrameStart, iFrameEnd);

	// Sync with loop boundaries (unlikely?)...
	if (m_pSession->isLooping() && iFrameStart < m_pSession->loopEnd()
		&& iFrameEnd >= m_pSession->loopEnd()) {
		iFrameEnd = m_pSession->loopStart()
			+ (iFrameEnd - m_pSession->loopEnd());
	}

	// Sync to the next bunch, also critical for Audio-MIDI sync...
	pMidiCursor->seek(iFrameEnd);
	pMidiCursor->process(m_iReadAhead);

	// Flush the MIDI engine output queue...
	pMidiEngine->flush();
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

	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	// Pick our actual MIDI sequencer cursor...
	qtractorSessionCursor *pMidiCursor = pMidiEngine->sessionCursor();
	if (pMidiCursor == NULL)
		return;

	// This is the last framestamp to be trown out...
	unsigned long iFrameEnd = pMidiCursor->frame();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::trackSync(%p, %lu, %lu)",
		this, pTrack, iFrameStart, iFrameEnd);
#endif

	// Split processing, in case we've been caught looping...
	if (m_pSession->isLooping() && iFrameEnd < iFrameStart) {
		unsigned long ls = m_pSession->loopStart();
		unsigned long le = m_pSession->loopEnd();
		if (iFrameStart < le) {
			long iTimeStart = pMidiEngine->timeStart();
			pMidiEngine->setTimeStart(
				iTimeStart + m_pSession->tickFromFrame(le - ls));
			trackClipSync(pTrack, iFrameStart, le);
			pMidiEngine->setTimeStart(iTimeStart);
			iFrameStart = ls;
		}
	}

	// Do normal sequence...
	trackClipSync(pTrack, iFrameStart, iFrameEnd);

	// Surely must realize the output queue...
	pMidiEngine->flush();
}


// MIDI track output process resync.
void qtractorMidiOutputThread::trackClipSync ( qtractorTrack *pTrack,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Locate the immediate nearest clip in track
	// and render them all thereafter, immediately...
	qtractorClip *pClip = pTrack->clips().first();
	while (pClip && pClip->clipStart() < iFrameEnd) {
		if (iFrameStart < pClip->clipStart() + pClip->clipLength())
			pClip->process(iFrameStart, iFrameEnd);
		pClip = pClip->next();
	}
}


// MIDI metronome output process resync.
void qtractorMidiOutputThread::metroSync ( unsigned long iFrameStart )
{
	QMutexLocker locker(&m_mutex);

	qtractorMidiEngine *pMidiEngine = m_pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	// Pick our actual MIDI sequencer cursor...
	qtractorSessionCursor *pMidiCursor = pMidiEngine->sessionCursor();
	if (pMidiCursor == NULL)
		return;

	// This is the last framestamp to be trown out...
	unsigned long iFrameEnd = pMidiCursor->frame();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiOutputThread[%p]::metroSync(%lu, %lu)",
		this, iFrameStart, iFrameEnd);
#endif

	// (Re)process the metronome stuff...
	pMidiEngine->processMetro(iFrameStart, iFrameEnd);

	// Surely must realize the output queue...
	pMidiEngine->flush();
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
// class qtractorMidiEngine -- ALSA sequencer client instance (singleton).
//

// Constructor.
qtractorMidiEngine::qtractorMidiEngine ( qtractorSession *pSession )
	: qtractorEngine(pSession, qtractorTrack::Midi)
{
	m_pAlsaSeq       = NULL;
	m_iAlsaClient    = -1;
	m_iAlsaQueue     = -1;

	m_pAlsaSubsSeq   = NULL;
	m_iAlsaSubsPort  = -1;
	m_pAlsaNotifier  = NULL;

	m_pInputThread   = NULL;
	m_pOutputThread  = NULL;

	m_iTimeStart     = 0;
	m_iTimeDelta     = 0;
	m_iTimeDrift     = 0;

	m_pNotifyObject  = NULL;
	m_eNotifyMmcType = QEvent::None;
	m_eNotifyCtlType = QEvent::None;

	m_bControlBus    = false;
	m_pIControlBus   = NULL;
	m_pOControlBus   = NULL;

	m_bMetronome         = false;
	m_bMetroBus          = false;
	m_pMetroBus          = NULL;
	m_iMetroChannel      = 9;	// GM Drums channel (10)
	m_iMetroBarNote      = 76;	// GM High-wood stick
	m_iMetroBarVelocity  = 96;
	m_iMetroBarDuration  = 24;
	m_iMetroBeatNote     = 77;	// GM Low-wood stick
	m_iMetroBeatVelocity = 64;
	m_iMetroBeatDuration = 16;

	// No input/capture quantization (default).
	m_iCaptureQuantize = 0;
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

	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Set queue tempo...
	snd_seq_queue_tempo_t *tempo;
	snd_seq_queue_tempo_alloca(&tempo);
	// Fill tempo struct with current tempo info.
	snd_seq_get_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, tempo);
	// Set the new intended ones...
	snd_seq_queue_tempo_set_ppq(tempo, (int) pSession->ticksPerBeat());
	snd_seq_queue_tempo_set_tempo(tempo,
		(unsigned int) (60000000.0f / pSession->tempo()));
	// Give tempo struct to the queue.
	snd_seq_set_queue_tempo(m_pAlsaSeq, m_iAlsaQueue, tempo);
}


// Reset all MIDI monitoring...
void qtractorMidiEngine::resetAllMonitors (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

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


// Reset all MIDI controllers...
void qtractorMidiEngine::resetAllControllers (void)
{
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
			qtractorMidiMonitor *pInputMonitor  = pMidiBus->midiMonitor_in();
			qtractorMidiMonitor *pOutputMonitor = pMidiBus->midiMonitor_out();
			if (pInputMonitor) {
				if (pOutputMonitor == NULL) {
					pMidiBus->setMasterVolume(pInputMonitor->gain());
					pMidiBus->setMasterPanning(pInputMonitor->panning());
				}
			}
			if (pOutputMonitor) {
				pMidiBus->setMasterVolume(pOutputMonitor->gain());
				pMidiBus->setMasterPanning(pOutputMonitor->panning());
			}
		}
	}

	// Reset all MIDI track channel controllers...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi) {
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
}


// MIDI event capture method.
void qtractorMidiEngine::capture ( snd_seq_event_t *pEv )
{
	qtractorMidiEvent::EventType type;

	unsigned short iChannel = 0;
	unsigned char  data1    = 0;
	unsigned char  data2    = 0;
	unsigned long  duration = 0;

	unsigned char *pSysex   = NULL;
	unsigned short iSysex   = 0;

	int iAlsaPort = pEv->dest.port;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// - capture quantization...
	if (m_iCaptureQuantize > 0) {
		unsigned long q = pSession->ticksPerBeat() / m_iCaptureQuantize;
		pEv->time.tick = q * ((pEv->time.tick + (q >> 1)) / q);
	}

#ifdef CONFIG_DEBUG_0
	// - show event for debug purposes...
	fprintf(stderr, "MIDI In  %05d 0x%02x", pEv->time.tick, pEv->type);
	if (pEv->type == SND_SEQ_EVENT_SYSEX) {
		fprintf(stderr, " sysex {");
		unsigned char *data = (unsigned char *) pEv->data.ext.ptr;
		for (unsigned int i = 0; i < pEv->data.ext.len; i++)
			fprintf(stderr, " %02x", data[i]);
		fprintf(stderr, " }\n");
	} else {
		for (unsigned int i = 0; i < sizeof(pEv->data.raw8.d); i++)
			fprintf(stderr, " %3d", pEv->data.raw8.d[i]);
		fprintf(stderr, "\n");
	}
#endif

	switch (pEv->type) {
	case SND_SEQ_EVENT_NOTE:
	case SND_SEQ_EVENT_NOTEON:
		type     = qtractorMidiEvent::NOTEON;
		iChannel = pEv->data.note.channel;
		data1    = pEv->data.note.note;
		data2    = pEv->data.note.velocity;
		duration = pEv->data.note.duration;
		if (data2 == 0) {
			pEv->type = SND_SEQ_EVENT_NOTEOFF;
			type = qtractorMidiEvent::NOTEOFF;
		}
		break;
	case SND_SEQ_EVENT_NOTEOFF:
		type     = qtractorMidiEvent::NOTEOFF;
		iChannel = pEv->data.note.channel;
		data1    = pEv->data.note.note;
		data2    = pEv->data.note.velocity;
		duration = pEv->data.note.duration;
		break;
	case SND_SEQ_EVENT_KEYPRESS:
		type     = qtractorMidiEvent::KEYPRESS;
		iChannel = pEv->data.control.channel;
		data1    = pEv->data.control.param;
		data2    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_CONTROLLER:
		type     = qtractorMidiEvent::CONTROLLER;
		iChannel = pEv->data.control.channel;
		data1    = pEv->data.control.param;
		data2    = pEv->data.control.value;
		// Trap controller commands...
		if (m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
			// FIXME: Avoid some estraneous events...
			if (data1 > 0x7f || data2 > 0x7f)
				return;
			// Post the stuffed event...
			if (m_pNotifyObject) {
				QApplication::postEvent(m_pNotifyObject,
					new qtractorMidiControlEvent(m_eNotifyCtlType,
						iChannel, data1, data2));
			}
		}
		break;
	case SND_SEQ_EVENT_PGMCHANGE:
		type     = qtractorMidiEvent::PGMCHANGE;
		iChannel = pEv->data.control.channel;
		data1    = 0;
		data2    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_CHANPRESS:
		type     = qtractorMidiEvent::CHANPRESS;
		iChannel = pEv->data.control.channel;
		data1    = 0;
		data2    = pEv->data.control.value;
		break;
	case SND_SEQ_EVENT_PITCHBEND:
		type     = qtractorMidiEvent::PITCHBEND;
		iChannel = pEv->data.control.channel;
		iSysex   = (unsigned short) (0x2000 + pEv->data.control.value); // aux.
		data1    = (iSysex & 0x007f);
		data2    = (iSysex & 0x3f80) >> 7;
		break;
	case SND_SEQ_EVENT_SYSEX:
		type     = qtractorMidiEvent::SYSEX;
		pSysex   = (unsigned char *) pEv->data.ext.ptr;
		iSysex   = (unsigned short)  pEv->data.ext.len;
		// Trap MMC commands...
		if (pSysex[1] == 0x7f && pSysex[3] == 0x06
			// check if it was intended to our input control bus!
			&& m_pIControlBus && m_pIControlBus->alsaPort() == iAlsaPort) {
			// Post the stuffed event...
			if (m_pNotifyObject) {
				QApplication::postEvent(m_pNotifyObject,
					new qtractorMmcEvent(m_eNotifyMmcType, pSysex));
			}
			// Bail out, right now!
			return;
		}
		break;
	default:
		// Not handled here...
		return;
	}

	// Now check which bus and track we're into...
	int iDrainOutput = 0;
	bool bRecording = (pSession->isRecording() && isPlaying());
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// Must be a MIDI track in capture/passthru
		// mode and for the intended channel...
		if (pTrack->trackType() == qtractorTrack::Midi
			&& (pTrack->isRecord() || pTrack->isMonitor())
		//	&& !pTrack->isMute() && (!pSession->soloTracks() || pTrack->isSolo())
			&& (pTrack->isMidiOmni() || pTrack->midiChannel() == iChannel)) {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->inputBus());
			if (pMidiBus && pMidiBus->alsaPort() == iAlsaPort) {
				// Is it actually recording?...
				if (pTrack->isRecord() && bRecording) {
					qtractorMidiClip *pMidiClip
						= static_cast<qtractorMidiClip *> (pTrack->clipRecord());
					if (pMidiClip && (!pSession->isPunching()
						|| (pEv->time.tick >= pSession->punchInTime()
						&&  pEv->time.tick <  pSession->punchOutTime()))) {
						// Yep, we got a new MIDI event...
						qtractorMidiEvent *pEvent = new qtractorMidiEvent(
							pEv->time.tick, type, data1, data2, duration);
						if (pSysex)
							pEvent->setSysex(pSysex, iSysex);
						pMidiClip->sequence()->addEvent(pEvent);
					}
				}
				// Track input monitoring...
				qtractorMidiMonitor *pMidiMonitor
					= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
				if (pMidiMonitor)
					pMidiMonitor->enqueue(type, data2);
				// Output monitoring on record...
				if (pTrack->isMonitor()) {
					pMidiBus = static_cast<qtractorMidiBus *> (pTrack->outputBus());
					if (pMidiBus && pMidiBus->midiMonitor_out()) {
						// FIXME: MIDI-thru channel filtering prolog... 
						unsigned short iOldChannel = pEv->data.note.channel;
						pEv->data.note.channel = pTrack->midiChannel();
						// MIDI-thru: same event redirected...
						snd_seq_ev_set_source(pEv, pMidiBus->alsaPort());
						snd_seq_ev_set_subs(pEv);
						snd_seq_ev_set_direct(pEv);
						snd_seq_event_output(m_pAlsaSeq, pEv);
					//	snd_seq_drain_output(m_pAlsaSeq);
						iDrainOutput++;
						// Done with MIDI-thru.
						pMidiBus->midiMonitor_out()->enqueue(type, data2);
						// Do it for the MIDI plugins too...
						if ((pTrack->pluginList())->midiManager())
							(pTrack->pluginList())->midiManager()->direct(pEv);
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
				pMidiBus->midiMonitor_in()->enqueue(type, data2);
			// Do it for the MIDI input plugins too...
			if (pMidiBus->pluginList_in()
				&& (pMidiBus->pluginList_in())->midiManager())
				((pMidiBus->pluginList_in())->midiManager())->direct(pEv);
			// Output monitoring on passthru...
			if (pMidiBus->isPassthru()) {
				// Do it for the MIDI output plugins too...
				if (pMidiBus->pluginList_out()
					&& (pMidiBus->pluginList_out())->midiManager())
					((pMidiBus->pluginList_out())->midiManager())->direct(pEv);
				if (pMidiBus->midiMonitor_out()) {
					// MIDI-thru: same event redirected...
					snd_seq_ev_set_source(pEv, pMidiBus->alsaPort());
					snd_seq_ev_set_subs(pEv);
					snd_seq_ev_set_direct(pEv);
					snd_seq_event_output(m_pAlsaSeq, pEv);
				//	snd_seq_drain_output(m_pAlsaSeq);
					iDrainOutput++;
					// Done with MIDI-thru.
					pMidiBus->midiMonitor_out()->enqueue(type, data2);
				}
			}
		}
	}

	if (iDrainOutput > 0)
		snd_seq_drain_output(m_pAlsaSeq);
}


// MIDI event enqueue method.
void qtractorMidiEngine::enqueue ( qtractorTrack *pTrack,
	qtractorMidiEvent *pEvent, unsigned long iTime, float fGain )
{
	// Target MIDI bus...
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus == NULL)
		return;
#if 1
	// HACK: Ignore our own mixer-monitor supplied controllers...
	if (pEvent->type() == qtractorMidiEvent::CONTROLLER) {
		if (pEvent->controller() == CHANNEL_VOLUME ||
			pEvent->controller() == CHANNEL_PANNING)
			return;
	}
#endif
	// Scheduled delivery: take into account
	// the time playback/queue started...
	unsigned long tick
		= ((long) iTime > m_iTimeStart ? iTime - m_iTimeStart : 0);

#ifdef CONFIG_DEBUG_0
	// - show event for debug purposes...
	fprintf(stderr, "MIDI Out %05lu 0x%02x", tick,
		(int) pEvent->type() | pTrack->midiChannel());
	if (pEvent->type() == qtractorMidiEvent::SYSEX) {
		fprintf(stderr, " sysex {");
		unsigned char *data = (unsigned char *) pEvent->sysex();
		for (unsigned int i = 0; i < pEvent->sysex_len(); i++)
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
			ev.data.note.channel    = pTrack->midiChannel();
			ev.data.note.note       = pEvent->note();
			ev.data.note.velocity   = int(fGain * float(pEvent->value())) & 0x7f;
			ev.data.note.duration   = pEvent->duration();
			break;
		case qtractorMidiEvent::KEYPRESS:
			ev.type = SND_SEQ_EVENT_KEYPRESS;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.param   = pEvent->note();
			ev.data.control.value   = pEvent->value();
			break;
		case qtractorMidiEvent::CONTROLLER:
			ev.type = SND_SEQ_EVENT_CONTROLLER;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.param   = pEvent->controller();
			ev.data.control.value   = pEvent->value();
			break;
		case qtractorMidiEvent::PGMCHANGE:
			ev.type = SND_SEQ_EVENT_PGMCHANGE;
			ev.data.control.channel = pTrack->midiChannel();
			ev.data.control.value   = pEvent->value();
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
		pMidiBus->midiMonitor_out()->enqueue(pEvent->type(), pEvent->value(), tick);

	// Do it for the MIDI track plugins too...
	if ((pTrack->pluginList())->midiManager())
		(pTrack->pluginList())->midiManager()->queued(&ev);

	// And for the MIDI output plugins as well...
	if (pMidiBus->pluginList_out()
		&& (pMidiBus->pluginList_out())->midiManager())
		((pMidiBus->pluginList_out())->midiManager())->queued(&ev);

}


// Flush ouput queue (if necessary)...
void qtractorMidiEngine::flush (void)
{
	// Really flush MIDI output...
	snd_seq_drain_output(m_pAlsaSeq);

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;
//	if (pSession->isRecording())
//		return;

	// Time to have some corrective approach...?
	snd_seq_queue_status_t *pQueueStatus;
	snd_seq_queue_status_alloca(&pQueueStatus);
	if (snd_seq_get_queue_status(
			m_pAlsaSeq, m_iAlsaQueue, pQueueStatus) >= 0) {
		unsigned long iMidiTime
			= snd_seq_queue_status_get_tick_time(pQueueStatus);
		unsigned long iAudioTime = pSession->tickFromFrame(
			pSession->audioEngine()->sessionCursor()->frameTime());
		m_iTimeDelta = (iAudioTime - iMidiTime) - m_iTimeDrift;
		if (m_iTimeDelta && iAudioTime > 0 && iMidiTime > 0) {
		//	m_iTimeStart += m_iTimeDelta;
			m_iTimeDrift += m_iTimeDelta;
#ifdef CONFIG_DEBUG
			qDebug("qtractorMidiEngine::flush(): "
				"iAudioTime=%lu iMidiTime=%lu iTimeDelta=%ld (%ld)",
				iAudioTime, iMidiTime, m_iTimeDelta, m_iTimeDrift);
#endif
		}
	}
}


// Device engine initialization method.
bool qtractorMidiEngine::init ( const QString& sClientName )
{
	// Try open a new client...
	if (snd_seq_open(&m_pAlsaSeq, "hw", SND_SEQ_OPEN_DUPLEX, 0) < 0)
		return false;
	if (m_pAlsaSeq == NULL)
		return false;

	// Fix client name.
	snd_seq_set_client_name(m_pAlsaSeq, sClientName.toUtf8().constData());

	m_iAlsaClient = snd_seq_client_id(m_pAlsaSeq);
	m_iAlsaQueue  = snd_seq_alloc_queue(m_pAlsaSeq);

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

	// Open control/metronome buses, at least try...
	openControlBus();
	openMetroBus();

	return true;
}


// Device engine activation method.
bool qtractorMidiEngine::activate (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Create and start our own MIDI input queue thread...
	m_pInputThread = new qtractorMidiInputThread(pSession);
	m_pInputThread->start(QThread::TimeCriticalPriority);

	// Create and start our own MIDI output queue thread...
	m_pOutputThread = new qtractorMidiOutputThread(pSession);
	m_pOutputThread->start(QThread::HighPriority);

	m_iTimeStart = 0;
	m_iTimeDelta = 0;
	m_iTimeDrift = 0;

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

	// Initial output thread bumping...
	qtractorSessionCursor *pMidiCursor
		= m_pOutputThread->midiCursorSync(true);
	if (pMidiCursor == NULL)
		return false;

	// Reset all dependables...
	resetTempo();
	resetAllMonitors();

	// Start queue timer...
	m_iTimeStart = (long) pSession->tickFromFrame(pMidiCursor->frame());
	m_iTimeDelta = 0;
	m_iTimeDrift = 0;

	// Effectively start sequencer queue timer...
	snd_seq_start_queue(m_pAlsaSeq, m_iAlsaQueue, NULL);

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

	// Delete output thread...
	if (m_pOutputThread) {
		// Make it nicely...
		if (m_pOutputThread->isRunning()) {
		//	m_pOutputThread->terminate();
			m_pOutputThread->wait();
		}
		delete m_pOutputThread;
		m_pOutputThread = NULL;
		m_iTimeStart = 0;
		m_iTimeDelta = 0;
		m_iTimeDrift = 0;
	}

	// Last but not least, delete input thread...
	if (m_pInputThread) {
		// Make it nicely...
		if (m_pInputThread->isRunning()) {
		//	m_pInputThread->terminate();
			m_pInputThread->wait();
		}
		delete m_pInputThread;
		m_pInputThread = NULL;
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
}


// Special rewind method, for queue loop.
void qtractorMidiEngine::restartLoop (void)
{
	qtractorSession *pSession = session();
	if (pSession && pSession->isLooping()) {
		m_iTimeStart -= (long) pSession->tickFromFrame(
			pSession->loopEnd() - pSession->loopStart());
		m_iTimeStart += m_iTimeDelta; // Drift correction...
		m_iTimeDelta  = 0;
	}
}


// The delta-time accessors.
void qtractorMidiEngine::setTimeStart ( long iTimeStart )
{
	m_iTimeStart = iTimeStart;
}

long qtractorMidiEngine::timeStart (void) const
{
	return m_iTimeStart;
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
		// Reset track monitor...
		qtractorMidiMonitor *pMidiMonitor
			= static_cast<qtractorMidiMonitor *> (pTrack->monitor());
		if (pMidiMonitor)
			pMidiMonitor->reset();
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


// Event notifier widget settings.
void qtractorMidiEngine::setNotifyObject ( QObject *pNotifyObject )
{
	m_pNotifyObject = pNotifyObject;
}

void qtractorMidiEngine::setNotifyMmcType ( QEvent::Type eNotifyMmcType )
{
	m_eNotifyMmcType = eNotifyMmcType;
}

void qtractorMidiEngine::setNotifyCtlType ( QEvent::Type eNotifyCtlType )
{
	m_eNotifyCtlType = eNotifyCtlType;
}


QObject *qtractorMidiEngine::notifyObject (void) const
{
	return m_pNotifyObject;
}

QEvent::Type qtractorMidiEngine::notifyMmcType (void) const
{
	return m_eNotifyMmcType;
}

QEvent::Type qtractorMidiEngine::notifyCtlType (void) const
{
	return m_eNotifyCtlType;
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
		m_pOControlBus = new qtractorMidiBus(this, "Control");
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
	if (m_bControlBus)
		m_pOControlBus->open();

	return true;
}


// Close MIDI control stuff.
void qtractorMidiEngine::closeControlBus (void)
{
	if (m_pOControlBus && m_bControlBus)
		m_pOControlBus->close();
}


// Destroy MIDI control stuff.
void qtractorMidiEngine::deleteControlBus (void)
{
	// When owned, both input and output
	// bus are the one and the same...
	if (m_pOControlBus && m_bControlBus) {
		m_pOControlBus->close();
		delete m_pOControlBus;
	}

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
	pSysex[iSysex++] = 0x7f;				// All-caller-id.
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
	delete pSysex;
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

	// Whether metronome bus is here owned, or...
	if (m_bMetroBus) {
		m_pMetroBus = new qtractorMidiBus(this,
			"Metronome", qtractorBus::Output);
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

	// Is there any?
	if (m_pMetroBus == NULL)
		createMetroBus();
	if (m_pMetroBus == NULL)
		return false;

	// This is it, when dedicated...
	if (m_bMetroBus)
		m_pMetroBus->open();

	return true;
}


// Close MIDI metronome stuff.
void qtractorMidiEngine::closeMetroBus (void)
{
	if (m_pMetroBus && m_bMetroBus)
		m_pMetroBus->close();
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
	if (!m_bMetronome)
		return;

	if (m_pMetroBus == NULL)
		return;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Register the next metronome slot.
	unsigned long  iTimeStart = pSession->tickFromFrame(iFrameStart);
	unsigned long  iTimeEnd   = pSession->tickFromFrame(iFrameEnd);

	unsigned short iTicksPerBeat = pSession->ticksPerBeat();
	unsigned int   iBeat = (iTimeStart + (iTicksPerBeat >> 1)) / iTicksPerBeat;
	unsigned long  iTime = iBeat * iTicksPerBeat;

	// Intialize outbound event...
	snd_seq_event_t ev;
	snd_seq_ev_clear(&ev);
	// Addressing...
	snd_seq_ev_set_source(&ev, m_pMetroBus->alsaPort());
	snd_seq_ev_set_subs(&ev);
	// Set common event data...
	ev.tag = (unsigned char) 0xff;
	ev.type = SND_SEQ_EVENT_NOTE;
	ev.data.note.channel  = m_iMetroChannel;

	while (iTime < iTimeEnd) {
		// Scheduled delivery: take into account
		// the time playback/queue started...
		unsigned long tick
			= ((long) iTime > m_iTimeStart ? iTime - m_iTimeStart : 0);
		snd_seq_ev_schedule_tick(&ev, m_iAlsaQueue, 0, tick);
		// Set proper event data...
		if (pSession->beatIsBar(iBeat)) {
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
		if (m_pOControlBus->midiMonitor_out()) {
			m_pOControlBus->midiMonitor_out()->enqueue(
				qtractorMidiEvent::NOTEON, ev.data.note.velocity, tick);
		}
		// Go for next beat...
		iTime += iTicksPerBeat;
		iBeat++;
	}
}


// Document element methods.
bool qtractorMidiEngine::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
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

		if (eChild.tagName() == "midi-bus") {
			QString sBusName = eChild.attribute("name");
			qtractorMidiBus::BusMode busMode
				= pDocument->loadBusMode(eChild.attribute("mode"));
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


bool qtractorMidiEngine::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
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

	// Contrrol bus (input) connects...
	if (m_bControlBus && m_pIControlBus) {
		QDomElement eInputs
			= pDocument->document()->createElement("control-inputs");
		qtractorBus::ConnectList inputs;
		m_pIControlBus->updateConnects(qtractorBus::Input, inputs);
		m_pIControlBus->saveConnects(inputs, pDocument, &eInputs);
		pElement->appendChild(eInputs);
	}

	// Contrrol bus (output) connects...
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
	unsigned long iExportStart, unsigned long iExportEnd,
	qtractorMidiBus *pExportBus )
{
	// No simultaneous or foul exports...
	if (!isActivated() || isPlaying())
		return false;

	// Make sure we have an actual session cursor...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Cannot have exports longer than current session.
	if (iExportStart >= iExportEnd)
		iExportEnd = pSession->sessionLength();
	if (iExportStart >= iExportEnd)
		return false;

	// We'll grab the first bus around, if none is given...
	if (pExportBus == NULL)
		pExportBus = static_cast<qtractorMidiBus *> (buses().first());
	if (pExportBus == NULL)
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
		if (pMidiBus->alsaPort() != pExportBus->alsaPort())
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
			iTracks++;
			pSeq = new qtractorMidiSequence(
				pTrack->trackName(), iTracks, iTicksPerBeat);
			pSeq->setChannel(pTrack->midiChannel());
			seqs.append(pSeq);
		}
		// Make this track setup...
		if (pSeq->bank() < 0)
			pSeq->setBank(pTrack->midiBank());
		if (pSeq->program() < 0)
			pSeq->setProgram(pTrack->midiProgram());
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
				while (pEvent && iTimeClip
					+ pEvent->time() < iTimeStart)
					pEvent = pEvent->next();
				while (pEvent && iTimeClip
					+ pEvent->time() + pEvent->duration() < iTimeEnd) {
					qtractorMidiEvent *pNewEvent
						= new qtractorMidiEvent(*pEvent);
					pNewEvent->setTime(iTimeOffset + pEvent->time());
					if (pNewEvent->type() == qtractorMidiEvent::NOTEON) {
						float fGain = pMidiClip->gain(
							pSession->frameFromTick(pEvent->time()));
						pNewEvent->setVelocity((unsigned char)
							(fGain * float(pEvent->velocity())) & 0x7f);
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
	iTracks++;

	// Special on SMF Format 1...
	if (ppSeqs == NULL) {
		// Sanity check...
		if (iTracks < 1)
			return false;
		// Number of actual track sequences...
		iSeqs  = iTracks;
		ppSeqs = new qtractorMidiSequence * [iSeqs];
		QListIterator<qtractorMidiSequence *> iter(seqs);
		ppSeqs[0] = NULL;	// META info track...
		for (iSeq = 1; iSeq < iSeqs && iter.hasNext(); ++iSeq)
			ppSeqs[iSeq] = iter.next();
		// May clear it now.
		seqs.clear();
	}

	// Prepare file for writing...
	qtractorMidiFile file;
	// File ready for export?
	bool bResult = file.open(sExportPath, qtractorMidiFile::Write);
	if (bResult) {
		file.setTempo(pSession->tempo());
		file.setBeatsPerBar(pSession->beatsPerBar());
		file.writeHeader(iFormat, iTracks, iTicksPerBeat);
		file.writeTracks(ppSeqs, iSeqs);
		file.close();
	}

	// Free locally allocated track/sequence array.
	for (iSeq = 0; iSeq < iSeqs; ++iSeq)
		delete ppSeqs[iSeq];
	delete [] ppSeqs;

	// Done successfully.
	return bResult;
}


// Retrieve/restore all connections, on all MIDI buses.
// return the total number of effective (re)connection attempts...
int qtractorMidiEngine::updateConnects (void)
{
	// It must be activated, sure...
	if (!isActivated())
		return 0;

	// Do it first on all standard owned dependable buses...
	int iUpdate = qtractorEngine::updateConnects();

	// Control bus inputs...
	if (m_bControlBus && m_pIControlBus) {
		iUpdate += m_pIControlBus->updateConnects(
				qtractorBus::Input, m_pIControlBus->inputs(), true);
	}

	// Control bus outputs...
	if (m_bControlBus && m_pOControlBus) {
		iUpdate += m_pOControlBus->updateConnects(
				qtractorBus::Output, m_pOControlBus->outputs(), true);
	}

	// Metronome bus outputs...
	if (m_bMetroBus && m_pMetroBus) {
		iUpdate += m_pMetroBus->updateConnects(
				qtractorBus::Output, m_pMetroBus->outputs(), true);
	}

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


//----------------------------------------------------------------------
// class qtractorMidiBus -- Managed ALSA sequencer port set
//

// Constructor.
qtractorMidiBus::qtractorMidiBus ( qtractorMidiEngine *pMidiEngine,
	const QString& sBusName, BusMode busMode, bool bPassthru )
	: qtractorBus(pMidiEngine, sBusName, busMode, bPassthru)
{
	m_iAlsaPort = -1;

	if (busMode & qtractorBus::Input) {
		m_pIMidiMonitor = new qtractorMidiMonitor(pMidiEngine->session());
		m_pIPluginList  = createPluginList();
	} else {
		m_pIMidiMonitor = NULL;
		m_pIPluginList  = NULL;
	}

	if (busMode & qtractorBus::Output) {
		m_pOMidiMonitor = new qtractorMidiMonitor(pMidiEngine->session());
		m_pOPluginList  = createPluginList();
	} else {
		m_pOMidiMonitor = NULL;
		m_pOPluginList  = NULL;
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
	if (m_pIPluginList) {
		updatePluginList(m_pIPluginList);
		m_pIPluginList->setName(QObject::tr("%1 In").arg(busName()));
	}
	if (m_pOPluginList) {
		updatePluginList(m_pOPluginList);
		m_pOPluginList->setName(QObject::tr("%1 Out").arg(busName()));
	}

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
	// Have a new/old input monitor?
	if (busMode() & qtractorBus::Input) {
		if (m_pIMidiMonitor == NULL)
			m_pIMidiMonitor = new qtractorMidiMonitor(engine()->session());
		if (m_pIPluginList == NULL)
			m_pIPluginList = createPluginList();
	} else {
		if (m_pIMidiMonitor) {
			delete m_pIMidiMonitor;
			m_pIMidiMonitor = NULL;
		}
		if (m_pIPluginList) {
			delete m_pIPluginList;
			m_pIPluginList = NULL;
		}
	}

	// Have a new/old output monitor?
	if (busMode() & qtractorBus::Output) {
		if (m_pOMidiMonitor == NULL)
			m_pOMidiMonitor = new qtractorMidiMonitor(engine()->session());
		if (m_pOPluginList == NULL)
			m_pOPluginList = createPluginList();
	} else {
		if (m_pOMidiMonitor) {
			delete m_pOMidiMonitor;
			m_pOMidiMonitor = NULL;
		}
		if (m_pOPluginList) {
			delete m_pOPluginList;
			m_pOPluginList = NULL;
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

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiBus[%p]::shutOff(%d)", this, int(bClose));
#endif

	QHash<unsigned short, Patch>::ConstIterator iter;
	for (iter = m_patches.constBegin(); iter != m_patches.constEnd(); ++iter) {
		unsigned short iChannel = iter.key();
		setControllerEx(iChannel, ALL_SOUND_OFF);
		setControllerEx(iChannel, ALL_NOTES_OFF);
		if (bClose)
			setControllerEx(iChannel, ALL_CONTROLLERS_OFF);
	}
}


// Direct MIDI bank/program selection helper.
void qtractorMidiBus::setPatch ( unsigned short iChannel,
	const QString& sInstrumentName, int iBankSelMethod,
	int iBank, int iProg, qtractorTrack *pTrack )
{
	// Sanity check.
	if (iProg < 0)
		return;

	// We always need our MIDI engine reference...
	qtractorMidiEngine *pMidiEngine
		= static_cast<qtractorMidiEngine *> (engine());
	if (pMidiEngine == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiBus[%p]::setPatch(%d, \"%s\", %d, %d, %d)",
		this, iChannel, sInstrumentName.toUtf8().constData(),
		iBankSelMethod, iBank, iProg);
#endif

	// Update patch mapping...
	if (!sInstrumentName.isEmpty()) {
		Patch& patch = m_patches[iChannel & 0x0f];
		patch.instrumentName = sInstrumentName;
		patch.bankSelMethod  = iBankSelMethod;
		patch.bank = iBank;
		patch.prog = iProg;
	}

	// Don't do anything else if engine
	// has not been activated...
	if (pMidiEngine->alsaSeq() == NULL)
		return;

	// Do it for the MIDI plugins if applicable...
	qtractorMidiManager *pMidiManager = NULL;
	if (pTrack)
		pMidiManager = (pTrack->pluginList())->midiManager();

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
		snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);
		if (pMidiManager) pMidiManager->direct(&ev);
	}

	// Select Bank LSB.
	if (iBank >= 0 && (iBankSelMethod == 0 || iBankSelMethod == 2)) {
		ev.type = SND_SEQ_EVENT_CONTROLLER;
		ev.data.control.channel = iChannel;
		ev.data.control.param   = BANK_SELECT_LSB;
		ev.data.control.value   = (iBank & 0x007f);
		snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);
		if (pMidiManager) pMidiManager->direct(&ev);
	}

	// Program change...
	ev.type = SND_SEQ_EVENT_PGMCHANGE;
	ev.data.control.channel = iChannel;
	ev.data.control.value   = iProg;
	snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);
	if (pMidiManager) pMidiManager->direct(&ev);

	pMidiEngine->flush();
}


// Direct MIDI controller helper.
void qtractorMidiBus::setController ( qtractorTrack *pTrack,
	int iController, int iValue ) const
{
	setControllerEx(pTrack->midiChannel(), iController, iValue, pTrack);
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
	snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);

	// Do it for the MIDI plugins too...
	if (pTrack && (pTrack->pluginList())->midiManager())
		(pTrack->pluginList())->midiManager()->direct(&ev);

	pMidiEngine->flush();
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
	snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);

	// Do it for the MIDI plugins too...
	if ((pTrack->pluginList())->midiManager())
		(pTrack->pluginList())->midiManager()->direct(&ev);

	pMidiEngine->flush();

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


// Direct SysEx helper.
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
	fprintf(stderr, "qtractorMidiBus::sendSysEx(%p, %u)", pSysex, iSysex);
	fprintf(stderr, " sysex {");
	for (unsigned int i = 0; i < iSysex; i++)
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
	snd_seq_event_output(pMidiEngine->alsaSeq(), &ev);

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
qtractorPluginList *qtractorMidiBus::createPluginList (void) const
{
	qtractorSession *pSession = engine()->session();
	if (pSession == NULL)
		return NULL;

	// Get audio bus as for the plugin list...
	// Output bus gets to be the first available output bus...
	qtractorAudioBus    *pAudioBus    = NULL;
	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine) {
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
		return new qtractorPluginList(0, 0, 0, true);

	// Create plugin-list alright...
	return new qtractorPluginList(pAudioBus->channels(),
		pAudioEngine->bufferSize(), pAudioEngine->sampleRate(), true);
}


// Update plugin-list buffers properly.
void qtractorMidiBus::updatePluginList ( qtractorPluginList *pPluginList )
{
	// Sanity checks...
	if (pPluginList == NULL)
		return;

	qtractorSession *pSession = engine()->session();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;
	
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
		pAudioEngine->bufferSize(), pAudioEngine->sampleRate(), true);
}


// Retrieve all current ALSA connections for a given bus mode interface;
// return the effective number of connection attempts...
int qtractorMidiBus::updateConnects ( qtractorBus::BusMode busMode,
	ConnectList& connects, bool bConnect )
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
		if (item.client > 0) {
			item.clientName = snd_seq_client_info_get_name(pClientInfo);
			snd_seq_port_info_set_client(pPortInfo, item.client);
			snd_seq_port_info_set_port(pPortInfo, -1);
			while (snd_seq_query_next_port(
					pMidiEngine->alsaSeq(), pPortInfo) >= 0) {
				unsigned int iPortCapability
					= snd_seq_port_info_get_capability(pPortInfo);
				if (((iPortCapability & iPortFlags) == iPortFlags) &&
					((iPortCapability & SND_SEQ_PORT_CAP_NO_EXPORT) == 0)) {
					item.port = snd_seq_port_info_get_port(pPortInfo);
					item.portName = snd_seq_port_info_get_name(pPortInfo);
					pItem = connects.findItem(item);
					if (pItem) {
						pItem->port = item.port;
						pItem->client = item.client;
					}
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
		item.clientName = snd_seq_client_info_get_name(pClientInfo);
		snd_seq_get_any_port_info(
			pMidiEngine->alsaSeq(), seq_addr.client, seq_addr.port, pPortInfo);
		item.port = seq_addr.port;
		item.portName = snd_seq_port_info_get_name(pPortInfo);
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
				iUpdate++;
			}
		}
	}

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
	unsigned char pan = (int(63.0f * (1.0f + fPanning)) + 1) & 0x7f;
	setController(pTrack, CHANNEL_PANNING, pan);
}


// Document element methods.
bool qtractorMidiBus::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	for (QDomNode nProp = pElement->firstChild();
			!nProp.isNull();
				nProp = nProp.nextSibling()) {

		// Convert node to element...
		QDomElement eProp = nProp.toElement();
		if (eProp.isNull())
			continue;

		// Load map elements (non-critical)...
		if (eProp.tagName() == "pass-through" ||
			eProp.tagName() == "midi-thru") { // Legacy compat.
			qtractorMidiBus::setPassthru(
				pDocument->boolFromText(eProp.text()));
		} else if (eProp.tagName() == "midi-map") {
			qtractorMidiBus::loadMidiMap(pDocument, &eProp);
		} else if (eProp.tagName() == "input-gain") {
			if (qtractorMidiBus::monitor_in())
				qtractorMidiBus::monitor_in()->setGain(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-panning") {
			if (qtractorMidiBus::monitor_in())
				qtractorMidiBus::monitor_in()->setPanning(
					eProp.text().toFloat());
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
		} else if (eProp.tagName() == "output-connects") {
			qtractorMidiBus::loadConnects(
				qtractorMidiBus::outputs(), pDocument, &eProp);
		}
	}

	return true;
}


bool qtractorMidiBus::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	pElement->setAttribute("name",
		qtractorMidiBus::busName());
	pElement->setAttribute("mode",
		pDocument->saveBusMode(qtractorMidiBus::busMode()));

	pDocument->saveTextElement("pass-through",
		pDocument->textFromBool(qtractorMidiBus::isPassthru()), pElement);

	if (qtractorMidiBus::busMode() & qtractorBus::Input) {
		pDocument->saveTextElement("input-gain",
			QString::number(qtractorMidiBus::monitor_in()->gain()),
				pElement);
		pDocument->saveTextElement("input-panning",
			QString::number(qtractorMidiBus::monitor_in()->panning()),
				pElement);
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
		QDomElement eMidiOutputs
			= pDocument->document()->createElement("output-connects");
		qtractorBus::ConnectList outputs;
		qtractorMidiBus::updateConnects(qtractorBus::Output, outputs);
		qtractorMidiBus::saveConnects(outputs, pDocument, &eMidiOutputs);
		pElement->appendChild(eMidiOutputs);
	}

	// Create the map element...
	QDomElement eMidiMap
		= pDocument->document()->createElement("midi-map");
	qtractorMidiBus::saveMidiMap(pDocument, &eMidiMap);
	pElement->appendChild(eMidiMap);

	return true;
}


// Document instrument map methods.
bool qtractorMidiBus::loadMidiMap ( qtractorSessionDocument * /*pDocument*/,
	QDomElement *pElement )
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


bool qtractorMidiBus::saveMidiMap ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	// Save map items...
	QHash<unsigned short, Patch>::ConstIterator iter;
	for (iter = m_patches.constBegin(); iter != m_patches.constEnd(); ++iter) {
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


// end of qtractorMidiEngine.cpp
