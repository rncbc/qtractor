// qtractorAudioEngine.cpp
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
#include "qtractorAudioEngine.h"
#include "qtractorAudioMonitor.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioClip.h"

#include "qtractorMonitor.h"
#include "qtractorSessionCursor.h"
#include "qtractorSessionDocument.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiBuffer.h"
#include "qtractorPlugin.h"
#include "qtractorClip.h"

#include <QApplication>
#include <QEvent>


#if defined(__SSE__)

#include <xmmintrin.h>

// SSE detection.
static inline bool sse_enabled (void)
{
	bool bSSE = false;
#if defined(__GNUC__)
#if defined(__i386__) || defined(__x86_64__)
	unsigned int a, b, c, d;
	__asm__ __volatile__ (
		"cpuid": "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (1));
	bSSE = (d & (1 << 25));
#endif
#endif
	return bSSE;
}

// SSE enabled mix-down processor version.
static inline void sse_buffer_add (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iBuffers, unsigned short iChannels, unsigned int iOffset )
{
	unsigned short j = 0;

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[j] + iOffset;
		float *pFrames = ppFrames[i];
		unsigned int nframes = iFrames;
		for (; (long(pBuffer) & 15) && (nframes > 0); --nframes)
			*pBuffer++ += *pFrames++;
		for (; nframes >= 4; nframes -= 4) {
			_mm_store_ps(pBuffer,
				_mm_add_ps(
					_mm_loadu_ps(pBuffer),
					_mm_loadu_ps(pFrames)));
			pFrames += 4;
			pBuffer += 4;
		}
		for (; nframes > 0; --nframes)
			*pBuffer++ += *pFrames++;
		if (++j >= iBuffers)
			j = 0;
	}
}

#endif


// Standard mix-down processor version.
static inline void std_buffer_add (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iBuffers, unsigned short iChannels, unsigned int iOffset )
{
	unsigned short j = 0;

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[j] + iOffset;
		float *pFrames = ppFrames[i];
		for (unsigned int n = 0; n < iFrames; ++n)
			*pBuffer++ += *pFrames++;
		if (++j >= iBuffers)
			j = 0;
	}
}


//----------------------------------------------------------------------
// qtractorAudioEngine_process -- JACK client process callback.
//

static int qtractorAudioEngine_process ( jack_nframes_t nframes, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	return pAudioEngine->process(nframes);
}


//----------------------------------------------------------------------
// qtractorAudioEngine_timebase -- JACK timebase master callback.
//

static void qtractorAudioEngine_timebase ( jack_transport_state_t,
	jack_nframes_t, jack_position_t *pPos, int, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	qtractorSession *pSession = pAudioEngine->session();
	if (pSession) {
		unsigned short iTicksPerBeat = pSession->ticksPerBeat();
		unsigned short iBeatsPerBar  = pSession->beatsPerBar();
		unsigned int   bars  = 0;
		unsigned int   beats = 0;
		unsigned long  ticks = pSession->tickFromFrame(pPos->frame);
		if (ticks >= (unsigned long) iTicksPerBeat) {
			beats  = (unsigned int)  (ticks / iTicksPerBeat);
			ticks -= (unsigned long) (beats * iTicksPerBeat);
		}
		if (beats >= (unsigned int) iBeatsPerBar) {
			bars   = (unsigned int) (beats / iBeatsPerBar);
			beats -= (unsigned int) (bars  * iBeatsPerBar);
		}
		// Time frame code in bars.beats.ticks ...
		pPos->valid = JackPositionBBT;
		pPos->bar   = bars  + 1;
		pPos->beat  = beats + 1;
		pPos->tick  = ticks;
		// Keep current tempo (BPM)...
		pPos->beats_per_bar    = iBeatsPerBar;
		pPos->ticks_per_beat   = iTicksPerBeat;
		pPos->beats_per_minute = pSession->tempo();
	//	pPos->beat_type        = 4.0;	// Quarter note.
	}
}


//----------------------------------------------------------------------
// qtractorAudioEngine_shutdown -- JACK client shutdown callback.
//

static void qtractorAudioEngine_shutdown ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->shutdown();

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QEvent(pAudioEngine->notifyShutdownType()));
	}
}


//----------------------------------------------------------------------
// qtractorAudioEngine_xrun -- JACK client XRUN callback.
//

static int qtractorAudioEngine_xrun ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QEvent(pAudioEngine->notifyXrunType()));
	}

	return 0;
}


//----------------------------------------------------------------------
// qtractorAudioEngine_graph_order -- JACK graph change callback.
//

static int qtractorAudioEngine_graph_order ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QEvent(pAudioEngine->notifyPortType()));
	}

	return 0;
}


//----------------------------------------------------------------------
// qtractorAudioEngine_graph_port -- JACK port registration callback.
//

static void qtractorAudioEngine_graph_port ( jack_port_id_t, int, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QEvent(pAudioEngine->notifyPortType()));
	}
}


//----------------------------------------------------------------------
// qtractorAudioEngine_buffer_size -- JACK buffer-size change callback.
//

static int qtractorAudioEngine_buffer_size ( jack_nframes_t, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QEvent(pAudioEngine->notifyBufferType()));
	}

	return 0;
}


//----------------------------------------------------------------------
// qtractorAudioEngine_freewheel -- Audio export process callback.
//

static void qtractorAudioEngine_freewheel ( int iStarting, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->setFreewheel(bool(iStarting));
}


//----------------------------------------------------------------------
// class qtractorAudioEngine -- JACK client instance (singleton).
//

// Constructor.
qtractorAudioEngine::qtractorAudioEngine ( qtractorSession *pSession )
	: qtractorEngine(pSession, qtractorTrack::Audio)
{
	m_pJackClient = NULL;

	m_pNotifyWidget       = NULL;
	m_eNotifyShutdownType = QEvent::None;
	m_eNotifyXrunType     = QEvent::None;
	m_eNotifyPortType     = QEvent::None;
	m_eNotifyBufferType   = QEvent::None;

	m_iBufferOffset = 0;

	// Audio-export freewheeling (internal) state.
	m_bFreewheel = false;
	
	// Audio-export (in)active state.
	m_bExporting   = false;
	m_pExportBus   = NULL;
	m_pExportFile  = NULL;
	m_iExportStart = 0;
	m_iExportEnd   = 0;
	m_iExportSync  = 0;
	m_bExportDone  = true;

	// Audio metronome stuff.
	m_bMetronome      = false;
	m_bMetroBus       = false;
	m_pMetroBus       = NULL;
	m_pMetroBarBuff   = NULL;
	m_pMetroBeatBuff  = NULL;
	m_iMetroBeatStart = 0;
	m_iMetroBeat      = 0;

	// Audition/pre-listening player stuff. 
	ATOMIC_SET(&m_playerLock, 0);
	m_bPlayerOpen  = false;
	m_bPlayerBus   = false;
	m_pPlayerBus   = NULL;
	m_pPlayerBuff  = NULL;
	m_iPlayerFrame = 0;
}


// JACK client descriptor accessor.
jack_client_t *qtractorAudioEngine::jackClient (void) const
{
	return m_pJackClient;
}


// Event notifier widget settings.
void qtractorAudioEngine::setNotifyWidget ( QWidget *pNotifyWidget )
{
	m_pNotifyWidget = pNotifyWidget;
}

void qtractorAudioEngine::setNotifyShutdownType ( QEvent::Type eNotifyShutdownType )
{
	m_eNotifyShutdownType = eNotifyShutdownType;
}

void qtractorAudioEngine::setNotifyXrunType ( QEvent::Type eNotifyXrunType )
{
	m_eNotifyXrunType = eNotifyXrunType;
}

void qtractorAudioEngine::setNotifyPortType ( QEvent::Type eNotifyPortType )
{
	m_eNotifyPortType = eNotifyPortType;
}

void qtractorAudioEngine::setNotifyBufferType ( QEvent::Type eNotifyBufferType )
{
	m_eNotifyBufferType = eNotifyBufferType;
}


QWidget *qtractorAudioEngine::notifyWidget (void) const
{
	return m_pNotifyWidget;
}

QEvent::Type qtractorAudioEngine::notifyShutdownType (void) const
{
	return m_eNotifyShutdownType;
}

QEvent::Type qtractorAudioEngine::notifyXrunType (void) const
{
	return m_eNotifyXrunType;
}

QEvent::Type qtractorAudioEngine::notifyPortType (void) const
{
	return m_eNotifyPortType;
}

QEvent::Type qtractorAudioEngine::notifyBufferType (void) const
{
	return m_eNotifyBufferType;
}


// Internal sample-rate accessor.
unsigned int qtractorAudioEngine::sampleRate (void) const
{
	return (m_pJackClient ? jack_get_sample_rate(m_pJackClient) : 0);
}

// Buffer size accessor.
unsigned int qtractorAudioEngine::bufferSize (void) const
{
	return (m_pJackClient ? jack_get_buffer_size(m_pJackClient) : 0);
}


// Buffer offset accessor.
unsigned int qtractorAudioEngine::bufferOffset (void) const
{
	return m_iBufferOffset;
}


// Special disaster recovery method.
void qtractorAudioEngine::shutdown (void)
{
	m_pJackClient = NULL;
}


// Device engine initialization method.
bool qtractorAudioEngine::init ( const QString& sClientName )
{
	// Try open a new client...
	m_pJackClient = jack_client_open(
		sClientName.toUtf8().constData(), JackUseExactName, NULL);
	if (m_pJackClient == NULL)
		return false;

	// ATTN: First thing to remember to set session sample rate.
	session()->setSampleRate(sampleRate());

	// Open player/metronome buses, at least try...
	openPlayerBus();
	openMetroBus();

	return true;
}


// Device engine activation method.
bool qtractorAudioEngine::activate (void)
{
	// Set our main engine processor callbacks.
	jack_set_process_callback(m_pJackClient,
			qtractorAudioEngine_process, this);

	// Trnsport timebase callbacks...
	jack_set_timebase_callback(m_pJackClient, 0 /* FIXME: un-conditional! */,
		qtractorAudioEngine_timebase, this);

	// And some other event callbacks...
	jack_set_xrun_callback(m_pJackClient,
		qtractorAudioEngine_xrun, this);
	jack_on_shutdown(m_pJackClient,
		qtractorAudioEngine_shutdown, this);
	jack_set_graph_order_callback(m_pJackClient,
		qtractorAudioEngine_graph_order, this);
	jack_set_port_registration_callback(m_pJackClient,
		qtractorAudioEngine_graph_port, this);
    jack_set_buffer_size_callback(m_pJackClient,
		qtractorAudioEngine_buffer_size, this);

	// Set audio export processor callback.
	jack_set_freewheel_callback(m_pJackClient,
		qtractorAudioEngine_freewheel, this);

	// Time to activate ourselves...
	jack_activate(m_pJackClient);

	// Now, do all auto-connection stuff (if applicable...)
	if (m_bPlayerBus && m_pPlayerBus)
		m_pPlayerBus->autoConnect();

	if (m_bMetroBus && m_pMetroBus)
		m_pMetroBus->autoConnect();

	for (qtractorBus *pBus = buses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *>(pBus);
		if (pAudioBus)
			pAudioBus->autoConnect();
	}

	// We're now ready and running...
	return true;
}


// Device engine start method.
bool qtractorAudioEngine::start (void)
{
	if (!isActivated())
	    return false;

	// Make sure we have an actual session cursor...
	resetMetro();

	// Start trnsport rolling...
	jack_transport_start(m_pJackClient);

	// We're now ready and running...
	return true;
}


// Device engine stop method.
void qtractorAudioEngine::stop (void)
{
	if (!isActivated())
	    return;

	jack_transport_stop(m_pJackClient);
}


// Device engine deactivation method.
void qtractorAudioEngine::deactivate (void)
{
	// We're stopping now...
	// setPlaying(false);

	// Deactivate the JACK client first.
	if (m_pJackClient)
		jack_deactivate(m_pJackClient);
}


// Device engine cleanup method.
void qtractorAudioEngine::clean (void)
{
	// Clean player/metronome buses...
	deletePlayerBus();
	deleteMetroBus();

	// Audio-export stilll around? weird...
	if (m_pExportFile) {
		delete m_pExportFile;
		m_pExportFile = NULL;
	}

	// Close the JACK client, finally.
	if (m_pJackClient) {
		jack_client_close(m_pJackClient);
		m_pJackClient = NULL;
	}
}


// Process cycle executive.
int qtractorAudioEngine::process ( unsigned int nframes )
{
	// Don't bother with a thing, if not running.
	if (!isActivated())
		return 0;

	// Must have a valid session...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return 0;

	// Make sure we have an actual session cursor...
	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
		return 0;

	// Are we actually freewheeling for export?...
	// notice that freewheeling has no RT requirements.
	if (m_bFreewheel) {
		// Make sure we're in a valid state...
		if (m_pExportFile && m_pExportBus && !m_bExportDone) {
			// Force/sync every audio clip approaching...
			m_iExportSync += nframes;
			if (m_iExportSync > (sampleRate() >> 3)) {
				m_iExportSync = 0;
				syncExport();
			}
			// This the legal process cycle frame range...
			unsigned long iFrameStart = pAudioCursor->frame();
			unsigned long iFrameEnd   = iFrameStart + nframes;
			// Write output bus buffer to export audio file...
			if (iFrameStart < m_iExportEnd && iFrameEnd > m_iExportStart) {
				// Prepare the output bus only...
				m_pExportBus->process_prepare(nframes);
				// Export cycle...
				pSession->process(pAudioCursor, iFrameStart, iFrameEnd);
				// Commit the output bus only...
				m_pExportBus->process_commit(nframes);
				// Write to export file...
				if (iFrameEnd > m_iExportEnd)
					nframes -= (iFrameEnd - m_iExportEnd);
				m_pExportFile->write(m_pExportBus->out(), nframes);
				// Prepare advance for next cycle...
				pAudioCursor->seek(iFrameEnd);
			}	// Are we trough?
			else m_bExportDone = true;
		}
		// Done with this one export cycle...
		return 0;
	}

	// Prepare all current audio buses...
	for (qtractorBus *pBus = buses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process_prepare(nframes);
	}
	// The owned buses too, if any...
	if (m_bMetroBus)
		m_pMetroBus->process_prepare(nframes);
	if (m_bPlayerBus)
		m_pPlayerBus->process_prepare(nframes);

	// Session RT-safeness lock...
	if (!pSession->acquire())
		return 0;

	// Reset buffer offset.
	m_iBufferOffset = 0;

	// Process audition/pre-listening...
	if (m_bPlayerOpen && ATOMIC_TAS(&m_playerLock)) {
		m_pPlayerBuff->readMix(m_pPlayerBus->out(), nframes,
			m_pPlayerBus->channels(), 0, 1.0f);
		m_bPlayerOpen = (m_iPlayerFrame < m_pPlayerBuff->length());
		m_iPlayerFrame += nframes;
		if (m_bPlayerBus)
			m_pPlayerBus->process_commit(nframes);
		ATOMIC_SET(&m_playerLock, 0);
	}

	// MIDI plugin manager processing...
	qtractorMidiManager *pMidiManager
		= pSession->midiManagers().first();
	if (pMidiManager) {
		unsigned long iTimeStart = pAudioCursor->frameTime();
		unsigned long iTimeEnd   = iTimeStart + nframes;
		while (pMidiManager) {
			pMidiManager->process(iTimeStart, iTimeEnd);
			pMidiManager = pMidiManager->next();
		}
	}

	// Don't go any further, if not playing.
	if (!isPlaying()) {
		// Do the idle processing...
		bool bRecord = (pSession->recordTracks() > 0);
		if (bRecord) {
			for (qtractorTrack *pTrack = pSession->tracks().first();
					pTrack; pTrack = pTrack->next()) {
				// Audio-buffers needs some preparation...
				if (pTrack->isRecord()
					&& pTrack->trackType() == qtractorTrack::Audio) {
					qtractorAudioBus *pInputBus
						= static_cast<qtractorAudioBus *> (pTrack->inputBus());
					qtractorAudioMonitor *pAudioMonitor
						= static_cast<qtractorAudioMonitor *> (pTrack->monitor());
					// Pre-monitoring...
					if (pInputBus && pAudioMonitor) {
						pAudioMonitor->process(
							pInputBus->in(), nframes, pInputBus->channels());
						// Post-passthru monitoring...
						if (pTrack->isMonitor() && !pTrack->isMute()
							&& (!pSession->soloTracks() || pTrack->isSolo())) {
							qtractorAudioBus *pOutputBus
								= static_cast<qtractorAudioBus *> (pTrack->outputBus());
							if (pOutputBus) {
								// Plugin chain thru-processing...
								pOutputBus->buffer_prepare_in(pInputBus, nframes);
								qtractorPluginList *pPluginList = pTrack->pluginList();
								if (pPluginList && pPluginList->activated())
									pPluginList->process(pOutputBus->buffer(), nframes);
								pOutputBus->buffer_commit(nframes);
							}
						}
					}
				}
			}
		}
		// Process audition/pre-listening bus...
		if (m_pPlayerBus && !m_bPlayerBus && !m_pPlayerBus->isPassthru())
			m_pPlayerBus->process_commit(nframes);
		// Pass-thru current audio buses...
		for (qtractorBus *pBus = buses().first();
			pBus; pBus = pBus->next()) {
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (pBus);
			if (pAudioBus && (bRecord || pAudioBus->isPassthru()))
				pAudioBus->process_commit(nframes);
		}
		// Done as idle...
		pSession->release();
		return 0;
	}

	// This the legal process cycle frame range...
	unsigned long iFrameStart = pAudioCursor->frame();
	unsigned long iFrameEnd   = iFrameStart + nframes;

	// Metronome stuff...
	if (m_bMetronome && m_pMetroBus && iFrameEnd > m_iMetroBeatStart) {
		qtractorAudioBuffer *pMetroBuff
			= (pSession->beatIsBar(m_iMetroBeat)
				? m_pMetroBarBuff : m_pMetroBeatBuff);
		if (iFrameStart < m_iMetroBeatStart) {
			pMetroBuff->readMix(m_pMetroBus->out(),
				iFrameEnd - m_iMetroBeatStart, m_pMetroBus->channels(),
				m_iMetroBeatStart - iFrameStart, 1.0f);
		} else if (iFrameStart < m_iMetroBeatStart + pMetroBuff->length()) {
			pMetroBuff->readMix(m_pMetroBus->out(),
				nframes, m_pMetroBus->channels(), 0, 1.0f);
		} else {
			m_iMetroBeatStart = pSession->frameFromBeat(++m_iMetroBeat);
			pMetroBuff->reset(false);
		}
		if (m_bMetroBus)
			m_pMetroBus->process_commit(nframes);
	}

	// Split processing, in case we're looping...
	if (pSession->isLooping()) {
		unsigned long iLoopEnd = pSession->loopEnd();
		 if (iFrameStart < iLoopEnd) {
			// Loop-length might be shorter than the buffer-period...
			while (iFrameEnd >= iLoopEnd) {
				// Process the remaining until end-of-loop...
				pSession->process(pAudioCursor, iFrameStart, iLoopEnd);
				m_iBufferOffset += (iLoopEnd - iFrameStart);
				// Reset to start-of-loop...
				iFrameStart = pSession->loopStart();
				iFrameEnd   = iFrameStart + (iFrameEnd - iLoopEnd);
				// Set to new transport location...
				jack_transport_locate(m_pJackClient, iFrameStart);
				pAudioCursor->seek(iFrameStart);
				// Take special care on metronome too...
				if (m_bMetronome) {
					m_iMetroBeat = pSession->beatFromFrame(iFrameStart);
					m_iMetroBeatStart = pSession->frameFromBeat(m_iMetroBeat);
				}
			}
		}
	}

	// Regular range...
	pSession->process(pAudioCursor, iFrameStart, iFrameEnd);
	m_iBufferOffset += (iFrameEnd - iFrameStart);

	// Commit current audio buses...
	for (qtractorBus *pBus = buses().first();
		pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process_commit(nframes);
	}

	// Sync with loop boundaries (unlikely?)
	if (pSession->isLooping() && iFrameStart < pSession->loopEnd()
		&& iFrameEnd >= pSession->loopEnd()) {
		iFrameEnd = pSession->loopStart()
			+ (iFrameEnd - pSession->loopEnd());
		// Set to new transport location...
		jack_transport_locate(m_pJackClient, iFrameEnd);
	}

	// Prepare advance for next cycle...
	pAudioCursor->seek(iFrameEnd);
	pAudioCursor->process(nframes);

	// Always sync to MIDI output thread...
	// (sure we have a MIDI engine, no?)
	pSession->midiEngine()->sync();

	// Release RT-safeness lock...
	pSession->release();

	// Process session stuff...
	return 0;
}


// Document element methods.
bool qtractorAudioEngine::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	qtractorEngine::clear();

	createPlayerBus();
	createMetroBus();

	// Load session children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		if (eChild.tagName() == "audio-bus") {
			QString sBusName = eChild.attribute("name");
			qtractorBus::BusMode busMode
				= pDocument->loadBusMode(eChild.attribute("mode"));
			qtractorAudioBus *pAudioBus
				= new qtractorAudioBus(this, sBusName, busMode);
			if (!pAudioBus->loadElement(pDocument, &eChild))
				return false;
			qtractorEngine::addBus(pAudioBus);
		}
		else if (eChild.tagName() == "metronome-outputs") {
			if (m_bMetroBus && m_pMetroBus) {
				m_pMetroBus->loadConnects(
					m_pMetroBus->outputs(), pDocument, &eChild);
			}
		}
		else if (eChild.tagName() == "player-outputs") {
			if (m_bPlayerBus && m_pPlayerBus) {
				m_pPlayerBus->loadConnects(
					m_pPlayerBus->outputs(), pDocument, &eChild);
			}
		}
	}

	return true;
}


bool qtractorAudioEngine::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	// Save audio buses...
	for (qtractorBus *pBus = qtractorEngine::buses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			// Create the new audio bus element...
			QDomElement eAudioBus
				= pDocument->document()->createElement("audio-bus");
			pAudioBus->saveElement(pDocument, &eAudioBus);
			pElement->appendChild(eAudioBus);
		}
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

	// Audition/pre-listening player bus connects...
	if (m_bPlayerBus && m_pPlayerBus) {
		QDomElement eOutputs
			= pDocument->document()->createElement("player-outputs");
		qtractorBus::ConnectList outputs;
		m_pPlayerBus->updateConnects(qtractorBus::Output, outputs);
		m_pPlayerBus->saveConnects(outputs, pDocument, &eOutputs);
		pElement->appendChild(eOutputs);
	}

	return true;
}


// Audio-exporting freewheel (internal) state accessors.
void qtractorAudioEngine::setFreewheel ( bool bFreewheel )
{
	m_bFreewheel = bFreewheel;
}

bool qtractorAudioEngine::isFreewheel (void) const
{
	return m_bFreewheel;
}


// Audio-exporting (freewheeling) state accessors.
void qtractorAudioEngine::setExporting ( bool bExporting )
{
	m_bExporting = bExporting;
}

bool qtractorAudioEngine::isExporting (void) const
{
	return m_bExporting;
}


// Audio-export method.
bool qtractorAudioEngine::fileExport ( const QString& sExportPath,
	unsigned long iExportStart, unsigned long iExportEnd,
	qtractorAudioBus *pExportBus )
{
	// No similtaneous or foul exports...
	if (!isActivated() || isPlaying() || isExporting())
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
		pExportBus = static_cast<qtractorAudioBus *> (buses().first());
	if (pExportBus == NULL)
		return false;

	// Get proper file type class...
	qtractorAudioFile *pExportFile
		= qtractorAudioFileFactory::createAudioFile(
			sExportPath, pExportBus->channels(), sampleRate());
	// No file ready for export?
	if (pExportFile == NULL)
		return false;

	// Go open it, for writeing of course...
	if (!pExportFile->open(sExportPath, qtractorAudioFile::Write)) {
		delete pExportFile;
		return false;
	}

	// Start with fixing the export range...
	m_bExporting   = true;
	m_pExportBus   = pExportBus;
	m_pExportFile  = pExportFile;
	m_iExportStart = iExportStart;
	m_iExportEnd   = iExportEnd;
	m_iExportSync  = 0;
	m_bExportDone  = false;

	// We'll have to save some session parameters...
	unsigned long iPlayHead  = pSession->playHead();
	unsigned long iLoopStart = pSession->loopStart();
	unsigned long iLoopEnd   = pSession->loopEnd();
	bool bPassthru = pExportBus->isPassthru();

	// Because we'll have to set the export conditions...
	pSession->setLoop(0, 0);
	pSession->setPlayHead(m_iExportStart);
	pExportBus->setPassthru(false);

	// Force sync...
	syncExport();

	// Special initialization.
	pSession->resetAllPlugins();
    m_iBufferOffset = 0;

	// Start export (freewheeling)...
	jack_set_freewheel(m_pJackClient, 1);

	// Wait for the export to end.
	while (m_bExporting && !m_bExportDone)
		qtractorSession::stabilize(200);

	// Stop export (freewheeling)...
	jack_set_freewheel(m_pJackClient, 0);

	// May close the file...
	m_pExportFile->close();

	// Restore session at ease...
	pSession->setLoop(iLoopStart, iLoopEnd);
	pSession->setPlayHead(iPlayHead);
	pExportBus->setPassthru(bPassthru);

	// Check user cancellation...
	bool bResult = m_bExporting;

	// Free up things here.
	delete m_pExportFile;
	m_bExporting   = false;
	m_pExportBus   = NULL;
	m_pExportFile  = NULL;
	m_iExportStart = 0;
	m_iExportEnd   = 0;
	m_iExportSync  = 0;
	m_bExportDone  = true;

	// Done whether successfully.
	return bResult;
}


// Direct sync method (needed for export)
void qtractorAudioEngine::syncExport (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
		return;

	unsigned long iFrameSync = pAudioCursor->frame() + sampleRate();

	int iTrack = 0;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Audio) {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (
					pAudioCursor->clip(iTrack));
			while (pAudioClip && pAudioClip->clipStart() < iFrameSync) {
				pAudioClip->syncExport();
				pAudioClip = static_cast<qtractorAudioClip *> (
					pAudioClip->next());
			}
		}
		iTrack++;
	}
}


// Metronome switching.
void qtractorAudioEngine::setMetronome ( bool bMetronome )
{
	m_bMetronome = bMetronome;

	if (isPlaying())
		resetMetro();
}

bool qtractorAudioEngine::isMetronome (void) const
{
	return m_bMetronome;
}


// Metronome bus mode accessors.
void qtractorAudioEngine::setMetroBus ( bool bMetroBus )
{
	deleteMetroBus();

	m_bMetroBus = bMetroBus;

	createMetroBus();

	if (isActivated()) {
		openMetroBus();
		if (m_pMetroBus && m_bMetroBus)
			m_pMetroBus->autoConnect();
	}
}

bool qtractorAudioEngine::isMetroBus (void) const
{
	return m_bMetroBus;
}

void qtractorAudioEngine::resetMetroBus (void)
{
	if (m_bMetroBus && m_pMetroBus)
		return;

	createMetroBus();
}


// Metronome bar audio sample.
void qtractorAudioEngine::setMetroBarFilename ( const QString& sFilename )
{
	m_sMetroBarFilename = sFilename;

	if (m_pMetroBarBuff) {
		m_pMetroBarBuff->setLength(0);
		m_pMetroBarBuff->open(m_sMetroBarFilename);
	}
}

const QString& qtractorAudioEngine::metroBarFilename (void) const
{
	return m_sMetroBarFilename;
}


// Metronome beat audio sample.
void qtractorAudioEngine::setMetroBeatFilename ( const QString& sFilename )
{
	m_sMetroBeatFilename = sFilename;

	if (m_pMetroBeatBuff) {
		m_pMetroBeatBuff->setLength(0);
		m_pMetroBeatBuff->open(m_sMetroBeatFilename);
	}
}

const QString& qtractorAudioEngine::metroBeatFilename() const
{
	return m_sMetroBeatFilename;
}


// Create audio metronome stuff...
void qtractorAudioEngine::createMetroBus (void)
{
	deleteMetroBus();

	// Whether metronome bus is here owned, or...
	if (m_bMetroBus) {
		m_pMetroBus = new qtractorAudioBus(this,
			"Metronome", qtractorBus::Output, 2);
	} else {
		// Metronome bus gets to be the first available output bus...
		for (qtractorBus *pBus = qtractorEngine::buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				m_pMetroBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}
	}
}


// Open audio metronome stuff...
bool qtractorAudioEngine::openMetroBus (void)
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

	// Enough number of channels?...
	unsigned short iChannels = m_pMetroBus->channels();
	if (iChannels < 1)
		return false;

	// We got it...
	m_pMetroBarBuff = new qtractorAudioBuffer(iChannels, sampleRate());
	m_pMetroBarBuff->open(m_sMetroBarFilename);
	m_pMetroBeatBuff = new qtractorAudioBuffer(iChannels, sampleRate());
	m_pMetroBeatBuff->open(m_sMetroBeatFilename);

	return true;
}


// Close audio metronome stuff.
void qtractorAudioEngine::closeMetroBus (void)
{
	if (m_pMetroBarBuff) {
		m_pMetroBarBuff->close();
		delete m_pMetroBarBuff;
		m_pMetroBarBuff = NULL;
	}

	if (m_pMetroBeatBuff) {
		m_pMetroBeatBuff->close();
		delete m_pMetroBeatBuff;
		m_pMetroBeatBuff = NULL;
	}

	if (m_pMetroBus && m_bMetroBus)
		m_pMetroBus->close();

	m_iMetroBeatStart = 0;
	m_iMetroBeat = 0;
}


// Destroy audio metronome stuff.
void qtractorAudioEngine::deleteMetroBus (void)
{
	closeMetroBus();

	if (m_pMetroBus && m_bMetroBus)
		delete m_pMetroBus;

	m_pMetroBus = NULL;
}


// Reset Audio metronome.
void qtractorAudioEngine::resetMetro (void)
{
	if (!m_bMetronome)
		return;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
		return;

	// Reset the next beat position...
	m_iMetroBeat = pSession->beatFromFrame(pAudioCursor->frame()) + 1;
	m_iMetroBeatStart = pSession->frameFromBeat(m_iMetroBeat);

	// Now each sample buffer must be bounded properly...
	unsigned long iMaxLength = (m_iMetroBeatStart / m_iMetroBeat);

	if (m_pMetroBarBuff) {
		unsigned long iMetroBarLength = m_pMetroBarBuff->frames();
		m_pMetroBarBuff->setLength(
			iMetroBarLength > iMaxLength ? iMaxLength : iMetroBarLength);
		m_pMetroBarBuff->reset(false);
	}

	if (m_pMetroBeatBuff) {
		unsigned long iMetroBeatLength = m_pMetroBeatBuff->frames();
		m_pMetroBeatBuff->setLength(
			iMetroBeatLength > iMaxLength ? iMaxLength : iMetroBeatLength);
		m_pMetroBeatBuff->reset(false);
	}
}


// Audition/pre-listening bus mode accessors.
void qtractorAudioEngine::setPlayerBus ( bool bPlayerBus )
{
	deletePlayerBus();

	m_bPlayerBus = bPlayerBus;

	createPlayerBus();

	if (isActivated()) {
		openPlayerBus();
		if (m_pPlayerBus && m_bPlayerBus)
			m_pPlayerBus->autoConnect();
	}
}

bool qtractorAudioEngine::isPlayerBus (void) const
{
	return m_bPlayerBus;
}

void qtractorAudioEngine::resetPlayerBus (void)
{
	if (m_bPlayerBus && m_pPlayerBus)
		return;

	createPlayerBus();
}


// Create audition/pre-listening stuff...
void qtractorAudioEngine::createPlayerBus (void)
{
	deletePlayerBus();

	// Whether audition/pre-listening bus is here owned, or...
	if (m_bPlayerBus) {
		m_pPlayerBus = new qtractorAudioBus(this,
			"Player", qtractorBus::Output, 2);
	} else {
		// Audition/pre-listening bus gets to be
		// the first available output bus...
		for (qtractorBus *pBus = qtractorEngine::buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				m_pPlayerBus = static_cast<qtractorAudioBus *> (pBus);
				break;
			}
		}
	}
}


// Open audition/pre-listening player stuff...
bool qtractorAudioEngine::openPlayerBus (void)
{
	closePlayerBus();

	// Is there any?
	if (m_pPlayerBus == NULL)
		createPlayerBus();
	if (m_pPlayerBus == NULL)
		return false;

	if (m_bPlayerBus)
		m_pPlayerBus->open();

	// Enough number of channels?...
	unsigned short iChannels = m_pPlayerBus->channels();
	if (iChannels < 1)
		return false;

	// We got it...
	m_pPlayerBuff = new qtractorAudioBuffer(iChannels, sampleRate());

	return true;
}


// Close audition/pre-listening stuff...
void qtractorAudioEngine::closePlayerBus (void)
{
	if (m_pPlayerBuff) {
		m_pPlayerBuff->close();
		delete m_pPlayerBuff;
		m_pPlayerBuff = NULL;
	}

	if (m_pPlayerBus && m_bPlayerBus)
		m_pPlayerBus->close();
}


// Destroy audition/pre-listening stuff...
void qtractorAudioEngine::deletePlayerBus (void)
{
	closePlayerBus();

	if (m_pPlayerBus && m_bPlayerBus)
		delete m_pPlayerBus;

	m_pPlayerBus = NULL;
}


// Tell whether audition/pre-listening is active...
bool qtractorAudioEngine::isPlayerOpen (void) const
{
	return m_bPlayerOpen;
}


// Open and start audition/pre-listening...
bool qtractorAudioEngine::openPlayer ( const QString& sFilename )
{
	// Must have a valid session...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Acquire proper locking...
	while (!ATOMIC_TAS(&m_playerLock))
		pSession->stabilize();

	// May close it logically...
	m_bPlayerOpen = false;

	// Is there any?
	if (m_pPlayerBuff) {
		m_pPlayerBuff->close();
		m_pPlayerBuff->setLength(0);
		m_bPlayerOpen = m_pPlayerBuff->open(sFilename);
	}

	m_iPlayerFrame = 0;

	// Release player lock...
	ATOMIC_SET(&m_playerLock, 0);

	return m_bPlayerOpen;
}


// Stop and close audition/pre-listening...
void qtractorAudioEngine::closePlayer (void)
{
	// Must have a valid session...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Acquire proper locking...
	while (!ATOMIC_TAS(&m_playerLock))
		pSession->stabilize();

	m_bPlayerOpen = false;

	if (m_pPlayerBuff)
		m_pPlayerBuff->close();

	m_iPlayerFrame = 0;

	// Release player lock...
	ATOMIC_SET(&m_playerLock, 0);
}


// Retrieve/restore all connections, on all audio buses.
// return the total number of effective (re)connection attempts...
int qtractorAudioEngine::updateConnects (void)
{
	// It must be activated, sure...
	if (!isActivated())
		return 0;

	// Do it first on all standard owned dependable buses...
	int iUpdate = qtractorEngine::updateConnects();

	// Metronome bus outputs...
	if (m_bMetroBus && m_pMetroBus) {
		iUpdate += m_pMetroBus->updateConnects(
				qtractorBus::Output, m_pMetroBus->outputs(), true);
	}

	// Player bus outputs...
	if (m_bPlayerBus && m_pPlayerBus) {
		iUpdate += m_pPlayerBus->updateConnects(
				qtractorBus::Output, m_pPlayerBus->outputs(), true);
	}

	// Done.
	return iUpdate;
}


//----------------------------------------------------------------------
// class qtractorAudioBus -- Managed JACK port set
//

// Constructor.
qtractorAudioBus::qtractorAudioBus ( qtractorAudioEngine *pAudioEngine,
	const QString& sBusName, BusMode busMode, bool bPassthru,
	unsigned short iChannels, bool bAutoConnect )
	: qtractorBus(pAudioEngine, sBusName, busMode, bPassthru)
{
	m_iChannels = iChannels;

	if (busMode & qtractorBus::Input) {
		m_pIAudioMonitor = new qtractorAudioMonitor(iChannels);
		m_pIPluginList   = new qtractorPluginList(iChannels, 0, 0);
	} else {
		m_pIAudioMonitor = NULL;
		m_pIPluginList   = NULL;
	}

	if (busMode & qtractorBus::Output) {
		m_pOAudioMonitor = new qtractorAudioMonitor(iChannels);
		m_pOPluginList   = new qtractorPluginList(iChannels, 0, 0);
	} else {
		m_pOAudioMonitor = NULL;
		m_pOPluginList   = NULL;
	}

	m_bAutoConnect = bAutoConnect;

	m_ppIPorts  = NULL;
	m_ppOPorts  = NULL;

	m_ppIBuffer = NULL;
	m_ppOBuffer = NULL;
	m_ppXBuffer = NULL;

	m_bEnabled  = false;

#if defined(__SSE__)
	if (sse_enabled())
		m_pfnBufferAdd = sse_buffer_add;
	else
#endif
	m_pfnBufferAdd = std_buffer_add;
}


// Destructor.
qtractorAudioBus::~qtractorAudioBus (void)
{
	close();

	if (m_pIAudioMonitor)
		delete m_pIAudioMonitor;
	if (m_pOAudioMonitor)
		delete m_pOAudioMonitor;

	if (m_pIPluginList)
		delete m_pIPluginList;
	if (m_pOPluginList)
		delete m_pOPluginList;
}


// Channel number property accessor.
void qtractorAudioBus::setChannels ( unsigned short iChannels )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	m_iChannels = iChannels;

	if (m_pIAudioMonitor)
		m_pIAudioMonitor->setChannels(iChannels);
	if (m_pOAudioMonitor)
		m_pOAudioMonitor->setChannels(iChannels);

	if (m_pIPluginList)
		m_pIPluginList->setBuffer(iChannels, 0, 0);

	if (m_pOPluginList)
		m_pIPluginList->setBuffer(iChannels, 0, 0);
}

unsigned short qtractorAudioBus::channels (void) const
{
	return m_iChannels;
}


// Auto-connection predicate.
void qtractorAudioBus::setAutoConnect ( bool bAutoConnect )
{
	m_bAutoConnect = bAutoConnect;
}

bool qtractorAudioBus::isAutoConnect (void) const
{
	return m_bAutoConnect;
}


// Register and pre-allocate bus port buffers.
bool qtractorAudioBus::open (void)
{
//	close();

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return false;
	if (pAudioEngine->jackClient() == NULL)
		return false;

	unsigned short i;

	if (busMode() & qtractorBus::Input) {
		// Register and allocate input port buffers...
		m_ppIPorts  = new jack_port_t * [m_iChannels];
		m_ppIBuffer = new float * [m_iChannels];
		const QString sIPortName(busName() + "/in_%1");
		for (i = 0; i < m_iChannels; i++) {
			m_ppIPorts[i] = jack_port_register(
				pAudioEngine->jackClient(),
				sIPortName.arg(i + 1).toUtf8().constData(),
				JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsInput, 0);
			m_ppIBuffer[i] = NULL;
		}
	}

	if (busMode() & qtractorBus::Output) {
		// Register and allocate output port buffers...
		m_ppOPorts  = new jack_port_t * [m_iChannels];
		m_ppOBuffer = new float * [m_iChannels];
		const QString sOPortName(busName() + "/out_%1");
		for (i = 0; i < m_iChannels; i++) {
			m_ppOPorts[i] = jack_port_register(
				pAudioEngine->jackClient(),
				sOPortName.arg(i + 1).toUtf8().constData(),
				JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsOutput, 0);
			m_ppOBuffer[i] = NULL;
		}
	}

	// Allocate internal working bus buffers...
	unsigned int iBufferSize = pAudioEngine->bufferSize();
	m_ppXBuffer = new float * [m_iChannels];
	for (i = 0; i < m_iChannels; i++)
		m_ppXBuffer[i] = new float [iBufferSize];

	// Plugin lists need some buffer (re)allocation too...
	unsigned int iSampleRate = pAudioEngine->sampleRate(); 
	if (m_pIPluginList) {
		m_pIPluginList->setBuffer(m_iChannels, iBufferSize, iSampleRate);
		m_pIPluginList->setName(QObject::tr("%1 In").arg(busName()));
	}
	if (m_pOPluginList) {
		m_pOPluginList->setBuffer(m_iChannels, iBufferSize, iSampleRate);
		m_pOPluginList->setName(QObject::tr("%1 Out").arg(busName()));
	}

	// Finally, open for biz...
	m_bEnabled = true;

	return true;
}


// Unregister and post-free bus port buffers.
void qtractorAudioBus::close (void)
{
	// Close for biz, immediate...
	m_bEnabled = false;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	unsigned short i;

	if (busMode() & qtractorBus::Input) {
		// Free input ports.
		if (m_ppIPorts) {
			// Unregister, if we're not shutdown...
			if (pAudioEngine->jackClient()) {
				for (i = 0; i < m_iChannels; i++) {
					if (m_ppIPorts[i]) {
						jack_port_unregister(
							pAudioEngine->jackClient(), m_ppIPorts[i]);
						m_ppIPorts[i] = NULL;
					}
				}
			}
			delete [] m_ppIPorts;
			m_ppIPorts = NULL;
		}
		// Free input Buffers.
		if (m_ppIBuffer)
			delete [] m_ppIBuffer;
		m_ppIBuffer = NULL;
	}

	if (busMode() & qtractorBus::Output) {
		// Free output ports.
		if (m_ppOPorts) {
			// Unregister, if we're not shutdown...
			if (pAudioEngine->jackClient()) {
				for (i = 0; i < m_iChannels; i++) {
					if (m_ppOPorts[i]) {
						jack_port_unregister(
							pAudioEngine->jackClient(), m_ppOPorts[i]);
						m_ppOPorts[i] = NULL;
					}
				}
			}
			delete [] m_ppOPorts;
			m_ppOPorts = NULL;
		}
		// Free output Buffers.
		if (m_ppOBuffer)
			delete [] m_ppOBuffer;
		m_ppOBuffer = NULL;
	}

	// Free internal buffers.
	if (m_ppXBuffer) {
		for (i = 0; i < m_iChannels; i++)
			delete [] m_ppXBuffer[i];
		delete [] m_ppXBuffer;
		m_ppXBuffer = NULL;
	}
}


// Auto-connect to physical ports.
void qtractorAudioBus::autoConnect (void)
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;
	if (pAudioEngine->jackClient() == NULL)
		return;

	if (!m_bAutoConnect)
		return;

	unsigned short i;

	if ((busMode() & qtractorBus::Input) && inputs().isEmpty()) {
		const char **ppszOPorts
			= jack_get_ports(pAudioEngine->jackClient(),
				0, JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsOutput | JackPortIsPhysical);
		if (ppszOPorts) {
			const QString sIPortName = pAudioEngine->clientName()
				+ ':' + busName() + "/in_%1";
			for (i = 0; i < m_iChannels && ppszOPorts[i]; ++i) {
				jack_connect(pAudioEngine->jackClient(),
					ppszOPorts[i], sIPortName.arg(i + 1).toUtf8().constData());
			}
			::free(ppszOPorts);
		}
	}

	if ((busMode() & qtractorBus::Output) && outputs().isEmpty()) {
		const char **ppszIPorts
			= jack_get_ports(pAudioEngine->jackClient(),
				0, JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsInput | JackPortIsPhysical);
		if (ppszIPorts) {
			const QString sOPortName = pAudioEngine->clientName()
				+ ':' + busName() + "/out_%1";
			for (i = 0; i < m_iChannels && ppszIPorts[i]; ++i) {
				jack_connect(pAudioEngine->jackClient(),
					sOPortName.arg(i + 1).toUtf8().constData(), ppszIPorts[i]);
			}
			::free(ppszIPorts);
		}
	}
}


// Bus mode change event.
void qtractorAudioBus::updateBusMode (void)
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	// Have a new/old input monitor?
	if (busMode() & qtractorBus::Input) {
		if (m_pIAudioMonitor == NULL)
			m_pIAudioMonitor = new qtractorAudioMonitor(m_iChannels);
		if (m_pIPluginList == NULL)
			m_pIPluginList = new qtractorPluginList(m_iChannels,
				pAudioEngine->bufferSize(), pAudioEngine->sampleRate());
	} else {
		if (m_pIAudioMonitor) {
			delete m_pIAudioMonitor;
			m_pIAudioMonitor = NULL;
		}
		if (m_pOPluginList) {
			delete m_pOPluginList;
			m_pOPluginList = NULL;
		}
	}

	// Have a new/old output monitor?
	if (busMode() & qtractorBus::Output) {
		if (m_pOAudioMonitor == NULL)
			m_pOAudioMonitor = new qtractorAudioMonitor(m_iChannels);
		if (m_pOPluginList == NULL)
			m_pOPluginList = new qtractorPluginList(m_iChannels,
				pAudioEngine->bufferSize(), pAudioEngine->sampleRate());
	} else {
		if (m_pOAudioMonitor) {
			delete m_pOAudioMonitor;
			m_pOAudioMonitor = NULL;
		}
		if (m_pOPluginList) {
			delete m_pOPluginList;
			m_pOPluginList = NULL;
		}
	}
}


// Process cycle preparator.
void qtractorAudioBus::process_prepare ( unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	unsigned short i;

	if (busMode() & qtractorBus::Input) {
		for (i = 0; i < m_iChannels; ++i) {
			m_ppIBuffer[i] = static_cast<float *>
				(jack_port_get_buffer(m_ppIPorts[i], nframes));
		}
		if (m_pIAudioMonitor)
			m_pIAudioMonitor->process(m_ppIBuffer, nframes);
		if (m_pIPluginList && m_pIPluginList->activated())
			m_pIPluginList->process(m_ppIBuffer, nframes);
	}

	if (busMode() & qtractorBus::Output) {
		for (i = 0; i < m_iChannels; ++i) {
			m_ppOBuffer[i] = static_cast<float *>
				(jack_port_get_buffer(m_ppOPorts[i], nframes));
			if (isPassthru() && (busMode() & qtractorBus::Input)) {
				::memcpy(m_ppOBuffer[i], m_ppIBuffer[i], nframes * sizeof(float));
			} else {
				::memset(m_ppOBuffer[i], 0, nframes * sizeof(float));
			}
		}
	}
}


// Process cycle commitment.
void qtractorAudioBus::process_commit ( unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	if (m_pOAudioMonitor)
		m_pOAudioMonitor->process(m_ppOBuffer, nframes);
	if (m_pOPluginList && m_pOPluginList->activated())
		m_pOPluginList->process(m_ppOBuffer, nframes);
}


// Bus-buffering methods.
void qtractorAudioBus::buffer_prepare ( unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	unsigned int offset = pAudioEngine->bufferOffset();
	unsigned int nbytes = nframes * sizeof(float);

	for (unsigned short i = 0; i < m_iChannels; ++i)
		::memset(m_ppXBuffer[i] + offset, 0, nbytes);
}


void qtractorAudioBus::buffer_commit ( unsigned int nframes )
{
	if (!m_bEnabled || (busMode() & qtractorBus::Output) == 0)
		return;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	(*m_pfnBufferAdd)(m_ppOBuffer, m_ppXBuffer, nframes,
		m_iChannels, m_iChannels, pAudioEngine->bufferOffset());
}


// Input record/passthru bus-buffering method.
void qtractorAudioBus::buffer_prepare_in (
	qtractorAudioBus *pInputBus, unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	unsigned int offset = pAudioEngine->bufferOffset();
	unsigned int nbytes = nframes * sizeof(float);

	unsigned short iBuffers = pInputBus->channels();
	float **ppBuffer = pInputBus->in();

	if (m_iChannels == iBuffers) {
		// Exact buffer copy...
		for (unsigned short i = 0; i < iBuffers; ++i)
			::memcpy(m_ppXBuffer[i] + offset, ppBuffer[i], nbytes);
	} else {
		// Buffer merge/multiplex...
		unsigned short i;
		for (i = 0; i < m_iChannels; ++i)
			::memset(m_ppXBuffer[i] + offset, 0, nbytes);
		if (m_iChannels > iBuffers) {
			unsigned short j = 0;
			for (i = 0; i < m_iChannels; ++i) {
				::memcpy(m_ppXBuffer[i] + offset, ppBuffer[j], nbytes);
				if (++j >= iBuffers)
					j = 0;
			}
		} else { // (m_iChannels < iBuffers)
			(*m_pfnBufferAdd)(m_ppXBuffer, ppBuffer, nframes,
				m_iChannels, iBuffers, offset);
		}
	}
}


float **qtractorAudioBus::buffer (void) const
{
	return m_ppXBuffer;
}


// Frame buffer accessors.
float **qtractorAudioBus::in (void) const
{
	return m_ppIBuffer;
}

float **qtractorAudioBus::out (void) const
{
	return m_ppOBuffer;
}


// Virtual I/O bus-monitor accessors.
qtractorMonitor *qtractorAudioBus::monitor_in (void) const
{
	return audioMonitor_in();
}

qtractorMonitor *qtractorAudioBus::monitor_out (void) const
{
	return audioMonitor_out();
}


// Audio I/O bus-monitor accessors.
qtractorAudioMonitor *qtractorAudioBus::audioMonitor_in (void) const
{
	return m_pIAudioMonitor;
}

qtractorAudioMonitor *qtractorAudioBus::audioMonitor_out (void) const
{
	return m_pOAudioMonitor;
}


// Plugin-chain accessors.
qtractorPluginList *qtractorAudioBus::pluginList_in (void) const
{
	return m_pIPluginList;
}

qtractorPluginList *qtractorAudioBus::pluginList_out (void) const
{
	return m_pOPluginList;
}



// Retrieve all current JACK connections for a given bus mode interface;
// return the effective number of connection attempts...
int qtractorAudioBus::updateConnects ( qtractorBus::BusMode busMode,
	ConnectList& connects, bool bConnect )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return 0;
	if (pAudioEngine->jackClient() == NULL)
		return 0;

	// Modes must match, at least...
	if ((busMode & qtractorAudioBus::busMode()) == 0)
		return 0;
	if (bConnect && connects.isEmpty())
		return 0;

	// Which kind of ports?
	jack_port_t **ppPorts
		= (busMode == qtractorBus::Input ? m_ppIPorts : m_ppOPorts);
	if (ppPorts == NULL)
		return 0;

	// For each channel...
	ConnectItem item;
	for (item.index = 0; item.index < m_iChannels; item.index++) {
		// Get port connections...
		const char **ppszClientPorts = jack_port_get_all_connections(
			pAudioEngine->jackClient(), ppPorts[item.index]);
		if (ppszClientPorts) {
			// Now, for each port...
			int iClientPort = 0;
			while (ppszClientPorts[iClientPort]) {
				// Check if already in list/connected...
				const QString sClientPort = ppszClientPorts[iClientPort];
				item.clientName = sClientPort.section(':', 0, 0);
				item.portName   = sClientPort.section(':', 1, 1);
				ConnectItem *pItem = connects.findItem(item);
				if (pItem && bConnect) {
					int iItem = connects.indexOf(pItem);
					if (iItem >= 0) {
						connects.removeAt(iItem);
						delete pItem;
					}
				} 
				else if (!bConnect)
					connects.append(new ConnectItem(item));
				iClientPort++;
			}
			::free(ppszClientPorts);
		}
	}

	// Shall we proceed for actual connections?
	if (!bConnect)
		return 0;

	// Our client:port prefix template...
	QString sClientPort = pAudioEngine->clientName() + ':';
	sClientPort += busName() + '/';
	sClientPort += (busMode == qtractorBus::Input ? "in" : "out");
	sClientPort += "_%1";

	QString sOutputPort;
	QString sInputPort;

	// For each (remaining) connection, try...
	int iUpdate = 0;
	QListIterator<ConnectItem *> iter(connects);
	while (iter.hasNext()) {
		ConnectItem *pItem = iter.next();
		// Mangle which is output and input...
		if (busMode == qtractorBus::Input) {
			sOutputPort = pItem->clientName + ':' + pItem->portName;
			sInputPort  = sClientPort.arg(pItem->index + 1);
		} else {
			sOutputPort = sClientPort.arg(pItem->index + 1);
			sInputPort  = pItem->clientName + ':' + pItem->portName;
		}
#ifdef CONFIG_DEBUG
		qDebug("qtractorAudioBus[%p]::updateConnects(%d): "
			"jack_connect: [%s] => [%s]", this, (int) busMode,
				sOutputPort.toUtf8().constData(),
				sInputPort.toUtf8().constData());
#endif
		// Do it...
		if (jack_connect(pAudioEngine->jackClient(),
				sOutputPort.toUtf8().constData(),
				sInputPort.toUtf8().constData()) == 0) {
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


// Document element methods.
bool qtractorAudioBus::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	for (QDomNode nProp = pElement->firstChild();
			!nProp.isNull();
				nProp = nProp.nextSibling()) {
		// Convert audio-bus property to element...
		QDomElement eProp = nProp.toElement();
		if (eProp.isNull())
			continue;
		if (eProp.tagName() == "pass-through") {
			qtractorAudioBus::setPassthru(
				pDocument->boolFromText(eProp.text()));
		} else if (eProp.tagName() == "channels") {
			qtractorAudioBus::setChannels(eProp.text().toUShort());
		} else if (eProp.tagName() == "auto-connect") {
			qtractorAudioBus::setAutoConnect(
				pDocument->boolFromText(eProp.text()));
		} else if (eProp.tagName() == "input-gain") {
			if (qtractorAudioBus::monitor_in())
				qtractorAudioBus::monitor_in()->setGain(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-panning") {
			if (qtractorAudioBus::monitor_in())
				qtractorAudioBus::monitor_in()->setPanning(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-plugins") {
			if (qtractorAudioBus::pluginList_in())
				qtractorAudioBus::pluginList_in()->loadElement(
					pDocument, &eProp);
		} else if (eProp.tagName() == "input-connects") {
			qtractorAudioBus::loadConnects(
				qtractorAudioBus::inputs(), pDocument, &eProp);
		} else if (eProp.tagName() == "output-gain") {
			if (qtractorAudioBus::monitor_out())
				qtractorAudioBus::monitor_out()->setGain(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "output-panning") {
			if (qtractorAudioBus::monitor_out())
				qtractorAudioBus::monitor_out()->setPanning(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "output-plugins") {
			if (qtractorAudioBus::pluginList_out())
				qtractorAudioBus::pluginList_out()->loadElement(
					pDocument, &eProp);
		} else if (eProp.tagName() == "output-connects") {
			qtractorAudioBus::loadConnects(
				qtractorAudioBus::outputs(), pDocument, &eProp);
		}
	}

	return true;
}


bool qtractorAudioBus::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	pElement->setAttribute("name",
		qtractorAudioBus::busName());
	pElement->setAttribute("mode",
		pDocument->saveBusMode(qtractorAudioBus::busMode()));

	pDocument->saveTextElement("pass-through",
		pDocument->textFromBool(qtractorAudioBus::isPassthru()), pElement);
	pDocument->saveTextElement("channels",
		QString::number(qtractorAudioBus::channels()), pElement);
	pDocument->saveTextElement("auto-connect",
		pDocument->textFromBool(qtractorAudioBus::isAutoConnect()), pElement);

	if (qtractorAudioBus::busMode() & qtractorBus::Input) {
		if (qtractorAudioBus::monitor_in()) {
			pDocument->saveTextElement("input-gain",
				QString::number(qtractorAudioBus::monitor_in()->gain()),
					pElement);
			pDocument->saveTextElement("input-panning",
				QString::number(qtractorAudioBus::monitor_in()->panning()),
					pElement);
		}
		if (qtractorAudioBus::pluginList_in()) {
			QDomElement eInputPlugins
				= pDocument->document()->createElement("input-plugins");
			qtractorAudioBus::pluginList_in()->saveElement(
				pDocument, &eInputPlugins);
			pElement->appendChild(eInputPlugins);
		}
		QDomElement eAudioInputs
			= pDocument->document()->createElement("input-connects");
		qtractorBus::ConnectList inputs;
		qtractorAudioBus::updateConnects(qtractorBus::Input, inputs);
		qtractorAudioBus::saveConnects(inputs, pDocument, &eAudioInputs);
		pElement->appendChild(eAudioInputs);
	}

	if (qtractorAudioBus::busMode() & qtractorBus::Output) {
		if (qtractorAudioBus::monitor_out()) {
			pDocument->saveTextElement("output-gain",
				QString::number(qtractorAudioBus::monitor_out()->gain()),
					pElement);
			pDocument->saveTextElement("output-panning",
				QString::number(qtractorAudioBus::monitor_out()->panning()),
					pElement);
		}
		if (qtractorAudioBus::pluginList_out()) {
			QDomElement eOutputPlugins
				= pDocument->document()->createElement("output-plugins");
			qtractorAudioBus::pluginList_out()->saveElement(
				pDocument, &eOutputPlugins);
			pElement->appendChild(eOutputPlugins);
		}
		QDomElement eAudioOutputs
			= pDocument->document()->createElement("output-connects");
		qtractorBus::ConnectList outputs;
		qtractorAudioBus::updateConnects(qtractorBus::Output, outputs);
		qtractorAudioBus::saveConnects(outputs, pDocument, &eAudioOutputs);
		pElement->appendChild(eAudioOutputs);
	}

	return true;
}


// end of qtractorAudioEngine.cpp
