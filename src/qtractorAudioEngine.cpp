// qtractorAudioEngine.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSession.h"

#include "qtractorDocument.h"

#include "qtractorMonitor.h"
#include "qtractorSessionCursor.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiManager.h"
#include "qtractorPlugin.h"
#include "qtractorClip.h"

#include "qtractorCurveFile.h"

#include "qtractorMainForm.h"

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_TIME
#include "qtractorLv2Plugin.h"
#endif
#endif

#ifdef CONFIG_JACK_SESSION
#include <jack/session.h>
#endif

#ifdef CONFIG_JACK_METADATA
#include <jack/metadata.h>
#endif

#include <QApplication>
#include <QProgressBar>
#include <QDomDocument>

#if defined(__SSE__)

#include <xmmintrin.h>

// SSE detection.
static inline bool sse_enabled (void)
{
#if defined(__GNUC__)
	unsigned int eax, ebx, ecx, edx;
#if defined(__x86_64__) || (!defined(PIC) && !defined(__PIC__))
	__asm__ __volatile__ (
		"cpuid\n\t" \
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) \
		: "a" (1) : "cc");
#else
	__asm__ __volatile__ (
		"push %%ebx\n\t" \
		"cpuid\n\t" \
		"movl %%ebx,%1\n\t" \
		"pop %%ebx\n\t" \
		: "=a" (eax), "=r" (ebx), "=c" (ecx), "=d" (edx) \
		: "a" (1) : "cc");
#endif
	return (edx & (1 << 25));
#else
	return false;
#endif
}


// SSE enabled mix-down processor version.
static inline void sse_buffer_add (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iBuffers, unsigned short iChannels, unsigned int iOffset )
{
	unsigned short j = 0;

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[j] + iOffset;
		float *pFrames = ppFrames[i] + iOffset;
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

#endif // __SSE__


#if defined(__ARM_NEON__)

#include "arm_neon.h"

// NEON enabled mix-down processor version.
static inline void neon_buffer_add (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iBuffers, unsigned short iChannels, unsigned int iOffset )
{
	unsigned short j = 0;

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[j] + iOffset;
		float *pFrames = ppFrames[i] + iOffset;
		unsigned int nframes = iFrames;
		for (; (long(pBuffer) & 15) && (nframes > 0); --nframes)
			*pBuffer++ += *pFrames++;
		for (; nframes >= 4; nframes -= 4) {
			vst1q_f32(pBuffer,
				vaddq_f32(
					vld1q_f32(pBuffer),
					vld1q_f32(pFrames)));
			pFrames += 4;
			pBuffer += 4;
		}
		for (; nframes > 0; --nframes)
			*pBuffer++ += *pFrames++;
		if (++j >= iBuffers)
			j = 0;
	}
}

#endif // __ARM_NEON__


// Standard mix-down processor version.
static inline void std_buffer_add (
	float **ppBuffer, float **ppFrames, unsigned int iFrames,
	unsigned short iBuffers, unsigned short iChannels, unsigned int iOffset )
{
	unsigned short j = 0;

	for (unsigned short i = 0; i < iChannels; ++i) {
		float *pBuffer = ppBuffer[j] + iOffset;
		float *pFrames = ppFrames[i] + iOffset;
		for (unsigned int n = 0; n < iFrames; ++n)
			*pBuffer++ += *pFrames++;
		if (++j >= iBuffers)
			j = 0;
	}
}


//----------------------------------------------------------------------
// qtractorAudioExportBuffer -- name tells all: audio export buffer.
//

class qtractorAudioExportBuffer
{
public:

	// Constructor
	qtractorAudioExportBuffer(
		unsigned short iChannels, unsigned int iBufferSize )
	{
		m_iChannels = iChannels;
		m_iBufferSize = iBufferSize;

		m_ppBuffer = new float * [m_iChannels];

		for (unsigned short i = 0; i < m_iChannels; ++i)
			m_ppBuffer[i] = new float [iBufferSize];

	#if defined(__SSE__)
		if (sse_enabled())
			m_pfnBufferAdd = sse_buffer_add;
		else
	#endif
	#if defined(__ARM_NEON__)
		m_pfnBufferAdd = neon_buffer_add;
		if (false)
	#endif
			m_pfnBufferAdd = std_buffer_add;
	}

	// Destructor.
	~qtractorAudioExportBuffer()
	{
		for (unsigned short i = 0; i < m_iChannels; ++i)
			delete [] m_ppBuffer[i];

		delete [] m_ppBuffer;
	}

	// Mix-down buffer accessors.
	unsigned short channels() const
		{ return m_iChannels; }

	unsigned int bufferSize() const
		{ return m_iBufferSize; }

	float **buffer() const
		{ return m_ppBuffer; }

	// Prepare mix-down buffer.
	void process_prepare(unsigned int nframes)
	{
		for (unsigned short i = 0; i < m_iChannels; ++i)
			::memset(m_ppBuffer[i], 0, nframes * sizeof(float));
	}

	// Incremental mix-down buffer.
	void process_add (qtractorAudioBus *pAudioBus,
		unsigned int nframes, unsigned int offset = 0)
	{
		(*m_pfnBufferAdd)(m_ppBuffer, pAudioBus->out(),
			nframes, m_iChannels, pAudioBus->channels(), offset);
	}

private:

	unsigned short m_iChannels;
	unsigned int m_iBufferSize;

	// Mix-down buffer.
	float **m_ppBuffer;

	// Mix-down buffer processor.
	void (*m_pfnBufferAdd)(float **, float **, unsigned int,
		unsigned short, unsigned short, unsigned int);
};


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
	jack_nframes_t, jack_position_t *pPos, int iNewPos, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->timebase(pPos, iNewPos);
}


//----------------------------------------------------------------------
// qtractorAudioEngine_shutdown -- JACK client shutdown callback.
//

static void qtractorAudioEngine_shutdown ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyShutEvent();
}


//----------------------------------------------------------------------
// qtractorAudioEngine_xrun -- JACK client XRUN callback.
//

static int qtractorAudioEngine_xrun ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyXrunEvent();

	return 0;
}


//----------------------------------------------------------------------
// qtractorAudioEngine_graph_order -- JACK graph change callback.
//

static int qtractorAudioEngine_graph_order ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyPortEvent();

	return 0;
}


//----------------------------------------------------------------------
// qtractorAudioEngine_client_registration -- JACK client reg. callback.
//

static void qtractorAudioEngine_client_registration (
	const char *, int, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyPortEvent();
}


//----------------------------------------------------------------------
// qtractorAudioEngine_port_registration -- JACK port reg. callback.
//

static void qtractorAudioEngine_port_registration (
	jack_port_id_t, int, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyPortEvent();
}


//----------------------------------------------------------------------
// qtractorAudioEngine_port_connect -- JACK port conn. callback.
//

static void qtractorAudioEngine_port_connect (
	jack_port_id_t, jack_port_id_t, int, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyPortEvent();
}


#ifdef CONFIG_JACK_PORT_RENAME

//----------------------------------------------------------------------
// qtractorAudioEngine_port_rename -- JACK port rename callback.
//

static void qtractorAudioEngine_port_rename (
	jack_port_id_t, const char *, const char *, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifyPortEvent();
}

#endif


//----------------------------------------------------------------------
// qtractorAudioEngine_buffer_size -- JACK buffer-size change callback.
//

static int qtractorAudioEngine_buffer_size ( jack_nframes_t nframes, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->bufferSize() < (unsigned int) nframes)
		pAudioEngine->notifyBuffEvent(nframes);

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


#ifdef CONFIG_JACK_SESSION

//----------------------------------------------------------------------
// qtractorAudioEngine_session_event -- JACK session event callabck
//

static void qtractorAudioEngine_session_event (
	jack_session_event_t *pSessionEvent, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	pAudioEngine->notifySessEvent(pSessionEvent);
}

#endif


//----------------------------------------------------------------------
// qtractorAudioEngine_sync -- JACK transport sync event callabck
//

static int qtractorAudioEngine_sync (
	jack_transport_state_t /*state*/, jack_position_t *pos, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->isFreewheel())
		return 0;

	const long iDeltaFrames
		= long(pos->frame) - long(pAudioEngine->sessionCursor()->frame());
	const unsigned int iBufferSize = pAudioEngine->bufferSize();
	if (labs(iDeltaFrames) > long(iBufferSize << 1)) {
		unsigned long iPlayHead = pos->frame;
		if (pAudioEngine->isPlaying())
			iPlayHead += iBufferSize;
		pAudioEngine->notifySyncEvent(iPlayHead);
	}

	return 1;
}


#ifdef CONFIG_JACK_METADATA
//----------------------------------------------------------------------
// qtractorAudioEngine_property_change -- JACK property change callabck
//

static void qtractorAudioEngine_property_change (
	jack_uuid_t, const char *key, jack_property_change_t, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	// PRETTY_NAME is the only metadata we are currently interested in...
	if (key && (::strcmp(key, JACK_METADATA_PRETTY_NAME) == 0))
		pAudioEngine->notifyPropEvent();
}
#endif


//----------------------------------------------------------------------
// class qtractorAudioEngine -- JACK client instance (singleton).
//

// Constructor.
qtractorAudioEngine::qtractorAudioEngine ( qtractorSession *pSession )
	: qtractorEngine(pSession, qtractorTrack::Audio)
{
	m_pJackClient = NULL;

	m_iSampleRate = 44100;	// A sensible default, always.
	m_iBufferSize = 0;

	m_iBufferOffset = 0;

	m_bMasterAutoConnect = true;

	// Audio-export freewheeling (internal) state.
	m_bFreewheel = false;

	// Common audio buffer sync thread.
	m_pSyncThread = NULL;

	// Audio-export (in)active state.
	m_bExporting   = false;
	m_pExportFile  = NULL;
	m_pExportBuses = NULL;
	m_pExportBuffer = NULL;
	m_iExportOffset = 0;
	m_iExportStart = 0;
	m_iExportEnd   = 0;
	m_bExportDone  = true;

	// Audio metronome stuff.
	m_bMetronome        = false;
	m_bMetroBus         = false;
	m_pMetroBus         = NULL;
	m_bMetroAutoConnect = true;
	m_pMetroBarBuff     = NULL;
	m_fMetroBarGain     = 1.0f;
	m_pMetroBeatBuff    = NULL;
	m_fMetroBeatGain    = 1.0f;
	m_iMetroOffset      = 0;
	m_iMetroBeatStart   = 0;
	m_iMetroBeat        = 0;
	m_bMetroEnabled     = false;

	// Audition/pre-listening player stuff.
	ATOMIC_SET(&m_playerLock, 0);
	m_bPlayerOpen  = false;
	m_bPlayerBus   = false;
	m_bPlayerAutoConnect = true;
	m_pPlayerBus   = NULL;
	m_pPlayerBuff  = NULL;
	m_iPlayerFrame = 0;

	// JACK transport mode.
	m_transportMode = qtractorBus::Duplex;

	// JACK timebase mode control.
	m_bTimebase = true;
	m_iTimebase = 0;
}


// Special event notifier proxy object.
const qtractorAudioEngineProxy *qtractorAudioEngine::proxy (void) const
{
	return &m_proxy;
}


// Event notifications.
void qtractorAudioEngine::notifyShutEvent (void)
{
	m_proxy.notifyShutEvent();
}

void qtractorAudioEngine::notifyXrunEvent (void)
{
	m_proxy.notifyXrunEvent();
}

void qtractorAudioEngine::notifyPortEvent (void)
{
	m_proxy.notifyPortEvent();
}

void qtractorAudioEngine::notifyBuffEvent ( unsigned int iBufferSize )
{
	m_proxy.notifyBuffEvent(iBufferSize);
}

void qtractorAudioEngine::notifySessEvent ( void *pvSessionArg )
{
	m_proxy.notifySessEvent(pvSessionArg);
}

void qtractorAudioEngine::notifySyncEvent ( unsigned long iPlayHead )
{
	m_proxy.notifySyncEvent(iPlayHead);
}

void qtractorAudioEngine::notifyPropEvent (void)
{
	m_proxy.notifyPropEvent();
}


// JACK client descriptor accessor.
jack_client_t *qtractorAudioEngine::jackClient (void) const
{
	return m_pJackClient;
}


// Internal sample-rate accessor.
unsigned int qtractorAudioEngine::sampleRate (void) const
{
	return m_iSampleRate;
}

// Buffer size accessor.
unsigned int qtractorAudioEngine::bufferSize (void) const
{
	return m_iBufferSize;
}


// Buffer offset accessor.
unsigned int qtractorAudioEngine::bufferOffset (void) const
{
	return m_iBufferOffset;
}


// Audio (Master) bus defaults accessors.
void qtractorAudioEngine::setMasterAutoConnect ( bool bMasterAutoConnect )
{
	m_bMasterAutoConnect = bMasterAutoConnect;
}

bool qtractorAudioEngine::isMasterAutoConnect (void) const
{
	return m_bMasterAutoConnect;
}


// Device engine initialization method.
bool qtractorAudioEngine::init (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Try open a new client...
	const QByteArray aClientName = pSession->clientName().toUtf8();
	int opts = JackNullOption;
#ifdef CONFIG_XUNIQUE
	opts |= JackUseExactName;
#endif
#ifdef CONFIG_JACK_SESSION
	if (!m_sSessionId.isEmpty()) {
		opts |= JackSessionID;
		const QByteArray aSessionId = m_sSessionId.toLocal8Bit();
		m_pJackClient = jack_client_open(
			aClientName.constData(),
			jack_options_t(opts), NULL,
			aSessionId.constData());
		// Reset JACK session UUID.
		m_sSessionId.clear();
	}
	else
#endif
	m_pJackClient = jack_client_open(
		aClientName.constData(),
		jack_options_t(opts), NULL);

	if (m_pJackClient == NULL)
		return false;

	// ATTN: First thing to remember is initial sample-rate and buffer size.
	m_iSampleRate = jack_get_sample_rate(m_pJackClient);
	m_iBufferSize = jack_get_buffer_size(m_pJackClient);

	// ATTN: Second is setting proper session client name.
	pSession->setClientName(
		QString::fromUtf8(jack_get_client_name(m_pJackClient)));

	// ATTN: Third is setting session sample rate.
	pSession->setSampleRate(m_iSampleRate);

	// Our dedicated audio buffer thread...
	m_pSyncThread = new qtractorAudioBufferThread();
	m_pSyncThread->start(QThread::HighPriority);

	return true;
}


// Device engine activation method.
bool qtractorAudioEngine::activate (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Let remaining buses get a life...
	openPlayerBus();
	openMetroBus();

	// MIDI plugin managers output buses...
	qtractorMidiManager *pMidiManager
		= pSession->midiManagers().first();
	while (pMidiManager) {
		qtractorAudioBus *pAudioBus = pMidiManager->audioOutputBus();
		if (pAudioBus && pMidiManager->isAudioOutputBus())
			pAudioBus->open();
		pMidiManager = pMidiManager->next();
	}

	// Ensure (not) freewheeling state...
	jack_set_freewheel(m_pJackClient, 0);

	// Set our main engine processor callbacks.
	jack_set_process_callback(m_pJackClient,
			qtractorAudioEngine_process, this);

	// Transport timebase callback...
	resetTimebase();

	// And some other event callbacks...
	jack_set_xrun_callback(m_pJackClient,
		qtractorAudioEngine_xrun, this);
	jack_on_shutdown(m_pJackClient,
		qtractorAudioEngine_shutdown, this);
	jack_set_graph_order_callback(m_pJackClient,
		qtractorAudioEngine_graph_order, this);
	jack_set_client_registration_callback(m_pJackClient,
		qtractorAudioEngine_client_registration, this);
	jack_set_port_registration_callback(m_pJackClient,
		qtractorAudioEngine_port_registration, this);
	jack_set_port_connect_callback(m_pJackClient,
		qtractorAudioEngine_port_connect, this);
#ifdef CONFIG_JACK_PORT_RENAME
	jack_set_port_rename_callback(m_pJackClient,
		qtractorAudioEngine_port_rename, this);
#endif
	jack_set_buffer_size_callback(m_pJackClient,
		qtractorAudioEngine_buffer_size, this);

	// Set audio export processor callback.
	jack_set_freewheel_callback(m_pJackClient,
		qtractorAudioEngine_freewheel, this);

#ifdef CONFIG_JACK_SESSION
	// Set JACK session event callback.
	if (jack_set_session_callback) {
		jack_set_session_callback(m_pJackClient,
			qtractorAudioEngine_session_event, this);
	}
#endif

	// Set JACK transport sync callback.
	if (m_transportMode & qtractorBus::Input) {
		jack_set_sync_callback(m_pJackClient,
			qtractorAudioEngine_sync, this);
	}

#ifdef CONFIG_JACK_METADATA
	// Set JACK property change callback.
	jack_set_property_change_callback(m_pJackClient,
		qtractorAudioEngine_property_change, this);
#endif

	// Reset all dependable monitoring...
	resetAllMonitors();

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

	// MIDI plugin managers output buses...
	pMidiManager = session()->midiManagers().first();
	while (pMidiManager) {
		qtractorAudioBus *pAudioBus = pMidiManager->audioOutputBus();
		if (pAudioBus && pMidiManager->isAudioOutputBus())
			pAudioBus->autoConnect();
		pMidiManager = pMidiManager->next();
	}

	// We're now ready and running...
	return true;
}


// Device engine start method.
bool qtractorAudioEngine::start (void)
{
	if (!isActivated())
		return false;

	// Reset all dependables...
	resetAllMonitors();

	// Make sure we have an actual session cursor...
	resetMetro();

	// Start transport rolling...
	if (m_transportMode & qtractorBus::Output)
		jack_transport_start(m_pJackClient);

	// We're now ready and running...
	return true;
}


// Device engine stop method.
void qtractorAudioEngine::stop (void)
{
	if (!isActivated())
		return;

	if (m_transportMode & qtractorBus::Output) {
		jack_transport_stop(m_pJackClient);
		jack_transport_locate(m_pJackClient, sessionCursor()->frame());
	}

	// MIDI plugin managers reset...
	qtractorMidiManager *pMidiManager
		= session()->midiManagers().first();
	while (pMidiManager) {
		pMidiManager->reset();
		pMidiManager = pMidiManager->next();
	}
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
	// Audio master bus auto-connection option...
	qtractorAudioBus *pMasterBus
		= static_cast<qtractorAudioBus *> (buses().first());
	if (pMasterBus) m_bMasterAutoConnect = pMasterBus->isAutoConnect();

	// Clean player/metronome buses...
	deletePlayerBus();
	deleteMetroBus();

	// Terminate common player/metro sync thread...
	if (m_pSyncThread) {
		if (m_pSyncThread->isRunning()) do {
			m_pSyncThread->setRunState(false);
		//	m_pSyncThread->terminate();
			m_pSyncThread->sync();
		} while (!m_pSyncThread->wait(100));
		delete m_pSyncThread;
		m_pSyncThread = NULL;
	}

	// Audio-export stilll around? weird...
	if (m_pExportBuffer) {
		delete m_pExportBuffer;
		m_pExportBuffer = NULL;
	}

	if (m_pExportBuses) {
		delete m_pExportBuses;
		m_pExportBuses = NULL;
	}

	if (m_pExportFile) {
		delete m_pExportFile;
		m_pExportFile = NULL;
	}

	// Close the JACK client, finally.
	if (m_pJackClient) {
		jack_client_close(m_pJackClient);
		m_pJackClient = NULL;
	}

	// Null sample-rate/period.
	// m_iSampleRate = 0;
	// m_iBufferSize = 0;
}


// Process cycle executive.
int qtractorAudioEngine::process ( unsigned int nframes )
{
	// Don't bother with a thing, if not running.
	if (!isActivated())
		return 0;

	// Reset buffer offset.
	m_iBufferOffset = 0;

	// Are we actually freewheeling for export?...
	// notice that freewheeling has no RT requirements.
	if (m_bFreewheel) {
		process_export(nframes);
		return 0;
	}

	// Must have a valid session...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return 0;

	// Make sure we have an actual session cursor...
	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
		return 0;

	// Session RT-safeness lock...
	if (!pSession->acquire())
		return 0;

	// Track whether audio output buses
	// buses needs monitoring while idle...
	int iOutputBus = 0;

	qtractorBus *pBus;
	qtractorAudioBus *pAudioBus;

	// Prepare all current audio buses...
	for (pBus = buses().first(); pBus; pBus = pBus->next()) {
		pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process_prepare(nframes);
	}

	// Prepare all extra audio buses...
	for (pBus = busesEx().first(); pBus; pBus = pBus->next()) {
		pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process_prepare(nframes);
	}

	// Monitor all current audio buses...
	for (pBus = buses().first(); pBus; pBus = pBus->next()) {
		pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process_monitor(nframes);
	}

	// The owned buses too, if any...
	if (m_bMetroBus && m_pMetroBus)
		m_pMetroBus->process_prepare(nframes);
	if (m_bPlayerBus && m_pPlayerBus)
		m_pPlayerBus->process_prepare(nframes);

	// Process audition/pre-listening...
	if (m_bPlayerOpen && ATOMIC_TAS(&m_playerLock)) {
		m_pPlayerBuff->readMix(m_pPlayerBus->out(), nframes,
			m_pPlayerBus->channels(), 0, 1.0f);
		m_bPlayerOpen = (m_iPlayerFrame < m_pPlayerBuff->length());
		m_iPlayerFrame += nframes;
		if (m_bPlayerBus && m_pPlayerBus)
			m_pPlayerBus->process_commit(nframes);
		else
			++iOutputBus;
		ATOMIC_SET(&m_playerLock, 0);
	}

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_TIME
	qtractorLv2Plugin::updateTime(this);
#endif
#endif

	// MIDI plugin manager processing...
	qtractorMidiManager *pMidiManager
		= pSession->midiManagers().first();
	if (pMidiManager) {
		const unsigned long iFrameTimeStart = pAudioCursor->frameTime();
		const unsigned long iFrameTimeEnd   = iFrameTimeStart + nframes;
		while (pMidiManager) {
			pMidiManager->process(iFrameTimeStart, iFrameTimeEnd);
			if (!pMidiManager->isAudioOutputBus())
				++iOutputBus;
			pMidiManager = pMidiManager->next();
		}
	}

	// Don't go any further, if not playing.
	if (!isPlaying()) {
		// Do the idle processing...
		for (qtractorTrack *pTrack = pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			// Audio-buffers needs some preparation...
			if (pTrack->trackType() == qtractorTrack::Audio) {
				qtractorAudioBus *pInputBus
					= static_cast<qtractorAudioBus *> (pTrack->inputBus());
				qtractorAudioMonitor *pAudioMonitor
					= static_cast<qtractorAudioMonitor *> (pTrack->monitor());
				// Pre-monitoring...
				if (pAudioMonitor == NULL)
					continue;
				// Record non-passthru metering...
				if (pTrack->isRecord() && pInputBus) {
					pAudioMonitor->process_meter(
						pInputBus->in(), nframes, pInputBus->channels());
				}
				// Monitor passthru/insert processing...
				qtractorPluginList *pPluginList = pTrack->pluginList();
				if (pSession->isTrackMonitor(pTrack)
					|| pPluginList->isAudioInsertActivated()) {
					// Plugin-chain processing...
					qtractorAudioBus *pOutputBus
						= static_cast<qtractorAudioBus *> (pTrack->outputBus());
					if (pOutputBus) {
						pOutputBus->buffer_prepare(nframes, pInputBus);
						pPluginList->process(pOutputBus->buffer(), nframes);
						pAudioMonitor->process(pOutputBus->buffer(), nframes);
						pOutputBus->buffer_commit(nframes);
						++iOutputBus;
					}
				}
			}
		}
		// Process audition/pre-listening bus...
		if (m_bPlayerBus && m_pPlayerBus)
			m_pPlayerBus->process_commit(nframes);
		// Pass-thru current audio buses...
		for (pBus = buses().first(); pBus; pBus = pBus->next()) {
			pAudioBus = static_cast<qtractorAudioBus *> (pBus);
			if (pAudioBus && (iOutputBus > 0 || pAudioBus->isMonitor()))
				pAudioBus->process_commit(nframes);
		}
		// Done as idle...
		pAudioCursor->process(nframes);
		pSession->release();
		return 0;
	}

	// This the legal process cycle frame range...
	unsigned long iFrameStart = pAudioCursor->frame();
	unsigned long iFrameEnd   = iFrameStart + nframes;

	// Metronome stuff...
	if (m_bMetronome && m_pMetroBus && iFrameEnd > m_iMetroBeatStart) {
		qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
		qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrameStart);
		qtractorAudioBuffer *pMetroBuff = NULL;
		if (pNode->beatIsBar(m_iMetroBeat))
			pMetroBuff = m_pMetroBarBuff;
		else
			pMetroBuff = m_pMetroBeatBuff;
		if (iFrameStart < m_iMetroBeatStart) {
			pMetroBuff->readMix(m_pMetroBus->out(),
				iFrameEnd - m_iMetroBeatStart, m_pMetroBus->channels(),
				m_iMetroBeatStart - iFrameStart, 1.0f);
		} else if (iFrameStart < m_iMetroBeatStart + pMetroBuff->length()) {
			pMetroBuff->readMix(m_pMetroBus->out(),
				nframes, m_pMetroBus->channels(), 0, 1.0f);
		} else {
			m_iMetroBeatStart = metro_offset(pNode->frameFromBeat(++m_iMetroBeat));
			pMetroBuff->reset(false);
		}
		if (m_bMetroBus && m_pMetroBus)
			m_pMetroBus->process_commit(nframes);
	}

	// Split processing, in case we're looping...
	if (pSession->isLooping()) {
		const unsigned long iLoopEnd = pSession->loopEnd();
		if (iFrameStart < iLoopEnd) {
			// Loop-length might be shorter than the buffer-period...
			while (iFrameEnd >= iLoopEnd + nframes) {
				// Process the remaining until end-of-loop...
				pSession->process(pAudioCursor, iFrameStart, iLoopEnd);
				m_iBufferOffset += (iLoopEnd - iFrameStart);
				// Reset to start-of-loop...
				iFrameStart = pSession->loopStart();
				iFrameEnd   = iFrameStart + (iFrameEnd - iLoopEnd);
				// Set to new transport location...
				if (m_transportMode & qtractorBus::Output)
					jack_transport_locate(m_pJackClient, iFrameStart);
				pAudioCursor->seek(iFrameStart);
			}
		}
	}

	// Regular range playback...
	pSession->process(pAudioCursor, iFrameStart, iFrameEnd);
	m_iBufferOffset += (iFrameEnd - iFrameStart);

	// Commit current audio buses...
	for (pBus = buses().first(); pBus; pBus = pBus->next()) {
		pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process_commit(nframes);
	}

	// Regular range recording (if and when applicable)...
	if (pSession->isRecording())
		pSession->process_record(iFrameStart, iFrameEnd);

	// Sync with loop boundaries (unlikely?)
	if (pSession->isLooping() && iFrameStart < pSession->loopEnd()
		&& iFrameEnd >= pSession->loopEnd()) {
		iFrameEnd = pSession->loopStart()
			+ (iFrameEnd - pSession->loopEnd());
		// Set to new transport location...
		if (m_transportMode & qtractorBus::Output)
			jack_transport_locate(m_pJackClient, iFrameEnd);
		// Take special care on metronome too...
		if (m_bMetronome) {
			m_iMetroBeat = pSession->beatFromFrame(iFrameEnd);
			m_iMetroBeatStart = metro_offset(pSession->frameFromBeat(m_iMetroBeat));
		}
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


// Freewheeling process cycle executive (needed for export).
void qtractorAudioEngine::process_export ( unsigned int nframes )
{
	if (m_bExportDone)
		return;
	if (m_pExportBuses  == NULL ||
		m_pExportFile   == NULL ||
		m_pExportBuffer == NULL)
		return;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
		return;

	// Make sure we're in a valid state...
	QListIterator<qtractorAudioBus *> iter(*m_pExportBuses);
	// Prepare the output buses first...
	while (iter.hasNext())
		iter.next()->process_prepare(nframes);
	// Prepare all extra audio buses...
	for (qtractorBus *pBusEx = busesEx().first();
			pBusEx; pBusEx = pBusEx->next()) {
		qtractorAudioBus *pAudioBusEx
			= static_cast<qtractorAudioBus *> (pBusEx);
		if (pAudioBusEx)
			pAudioBusEx->process_prepare(nframes);
	}

	// This the legal process cycle frame range...
	const unsigned long iFrameStart = pAudioCursor->frame();
	const unsigned long iFrameEnd   = iFrameStart + nframes;

	// Write output bus buffers to export audio file...
	if (iFrameStart < m_iExportEnd) {
		// Prepare mix-down buffer...
		m_pExportBuffer->process_prepare(nframes);
		// Force/sync every audio clip approaching...
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_TIME
		qtractorLv2Plugin::updateTime(this);
	#endif
	#endif
		// MIDI plugin manager processing...
		qtractorMidiManager *pMidiManager
			= pSession->midiManagers().first();
		while (pMidiManager) {
			pMidiManager->process(iFrameStart, iFrameEnd);
			pMidiManager = pMidiManager->next();
		}
		// Perform all tracks processing...
		int iTrack = 0;
		for (qtractorTrack *pTrack = pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			pTrack->process_export(pAudioCursor->clip(iTrack),
				iFrameStart, iFrameEnd);
			++iTrack;
		}
		// Prepare advance for next cycle...
		pAudioCursor->seek(iFrameEnd);
		// Check end-of-export...
		if (iFrameEnd > m_iExportEnd)
			nframes -= (iFrameEnd - m_iExportEnd);
		// Commit the output buses...
		iter.toFront();
		while (iter.hasNext()) {
			qtractorAudioBus *pExportBus = iter.next();
			pExportBus->process_commit(nframes);
			m_pExportBuffer->process_add(pExportBus, nframes);
		}
		// Write to export file...
		m_pExportFile->write(m_pExportBuffer->buffer(), nframes);
		// HACK! Freewheeling observers update (non RT safe!)...
		qtractorSubject::flushQueue(false);
	} else {
		// Are we trough?
		m_bExportDone = true;
		// HACK! Reset all observers...
		qtractorSubject::resetQueue();
		// HACK: Reset all MIDI plugin buffers...
		qtractorMidiManager *pMidiManager
			= pSession->midiManagers().first();
		while (pMidiManager) {
			pMidiManager->reset();
			pMidiManager = pMidiManager->next();
		}
		// HACK: Shut-off (panic) all MIDI tracks...
		QHash<qtractorMidiBus *, unsigned short> channels;
		for (qtractorTrack *pTrack = pSession->tracks().first();
				pTrack; pTrack = pTrack->next()) {
			if (pTrack->trackType() != qtractorTrack::Midi)
				continue;
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (pTrack->outputBus());
			if (pMidiBus == NULL)
				continue;
			const unsigned short iChannel = pTrack->midiChannel();
			pMidiManager = (pTrack->pluginList())->midiManager();
			if (pMidiManager)
				pMidiManager->shutOff(iChannel);
			const unsigned short iChannelMask = (1 << iChannel);
			const unsigned short iChannelFlags = channels.value(pMidiBus, 0);
			if ((iChannelFlags & iChannelMask) == 0) {
				pMidiManager = (pMidiBus->pluginList_out())->midiManager();
				if (pMidiManager)
					pMidiManager->shutOff(iChannel);
				channels.insert(pMidiBus, iChannelFlags | iChannelMask);
			}
		}
		// HACK: Reset all audio monitors...
		resetAllMonitors();
	}
}


// JACK timebase master callback.
void qtractorAudioEngine::timebase ( jack_position_t *pPos, int iNewPos )
{
	qtractorSession *pSession = session();
	qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pPos->frame);
	unsigned short bars  = 0;
	unsigned int   beats = 0;
	unsigned long  ticks = pNode->tickFromFrame(pPos->frame) - pNode->tick;
	if (ticks >= (unsigned long) pNode->ticksPerBeat) {
		beats  = (unsigned int) (ticks / pNode->ticksPerBeat);
		ticks -= (unsigned long) (beats * pNode->ticksPerBeat);
	}
	if (beats >= (unsigned int) pNode->beatsPerBar) {
		bars   = (unsigned short) (beats / pNode->beatsPerBar);
		beats -= (unsigned int) (bars * pNode->beatsPerBar);
	}
	// Time frame code in bars.beats.ticks ...
	pPos->valid = JackPositionBBT;
	pPos->bar   = pNode->bar + bars + 1;
	pPos->beat  = beats + 1;
	pPos->tick  = ticks;
	// Keep current tempo (BPM)...
	pPos->beats_per_bar    = pNode->beatsPerBar;
	pPos->ticks_per_beat   = pNode->ticksPerBeat;
	pPos->beats_per_minute = pNode->tempo;
	pPos->beat_type        = float(1 << pNode->beatDivisor);

	// Tell that we've been here...
	if (iNewPos) ++m_iTimebase;
}


// Document element methods.
bool qtractorAudioEngine::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
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

		if (eChild.tagName() == "audio-control") {
			for (QDomNode nProp = eChild.firstChild();
					!nProp.isNull(); nProp = nProp.nextSibling()) {
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				if (eProp.tagName() == "transport-mode") {
					qtractorAudioEngine::setTransportMode(
						qtractorBus::busModeFromText(eProp.text()));
				}
				else
				if (eProp.tagName() == "timebase") {
					qtractorAudioEngine::setTimebase(
						qtractorDocument::boolFromText(eProp.text()));
				}
			}
		}
		else if (eChild.tagName() == "audio-bus") {
			QString sBusName = eChild.attribute("name");
			qtractorBus::BusMode busMode
				= qtractorBus::busModeFromText(eChild.attribute("mode"));
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


bool qtractorAudioEngine::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	// Save transport/control modes...
	QDomElement eControl
		= pDocument->document()->createElement("audio-control");
	pDocument->saveTextElement("transport-mode",
		qtractorBus::textFromBusMode(
			qtractorAudioEngine::transportMode()), &eControl);
	pDocument->saveTextElement("timebase",
		qtractorDocument::textFromBool(
			qtractorAudioEngine::isTimebase()), &eControl);
	pElement->appendChild(eControl);

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


// JACK Session UUID accessors.
void qtractorAudioEngine::setSessionId ( const QString& sSessionId )
{
	m_sSessionId = sSessionId;
}

const QString& qtractorAudioEngine::sessionId (void) const
{
	return m_sSessionId;
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


// Last known export accessors.
unsigned long qtractorAudioEngine::exportStart (void) const
{
	return m_iExportStart;
}

unsigned long qtractorAudioEngine::exportOffset (void) const
{
	return m_iExportOffset;
}

unsigned long qtractorAudioEngine::exportLength (void) const
{
	return (m_iExportEnd > m_iExportStart ? m_iExportEnd - m_iExportStart : 0);
}



// Audio-export method.
bool qtractorAudioEngine::fileExport (
	const QString& sExportPath, const QList<qtractorAudioBus *>& exportBuses,
	unsigned long iExportStart, unsigned long iExportEnd )
{
	// No simultaneous or foul exports...
	if (!isActivated() || isPlaying() || isExporting())
		return false;

	// Make sure we have an actual session cursor...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// About to show some progress bar...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	QProgressBar *pProgressBar = pMainForm->progressBar();
	if (pProgressBar == NULL)
		return false;

	// Cannot have exports longer than current session.
	if (iExportStart >= iExportEnd)
		iExportEnd = pSession->sessionEnd();
	if (iExportStart >= iExportEnd)
		return false;

	// We'll grab the first bus around, as reference...
	qtractorAudioBus *pExportBus
		= static_cast<qtractorAudioBus *> (buses().first());
	if (pExportBus == NULL)
		return false;

	// Get proper file type class...
	const unsigned int iChannels = pExportBus->channels();
	qtractorAudioFile *pExportFile
		= qtractorAudioFileFactory::createAudioFile(
			sExportPath, iChannels, sampleRate());
	// No file ready for export?
	if (pExportFile == NULL)
		return false;

	// Go open it, for writeing of course...
	if (!pExportFile->open(sExportPath, qtractorAudioFile::Write)) {
		delete pExportFile;
		return false;
	}

	// We'll be busy...
	pSession->lock();

	// HACK! reset subject/observers queue...
	qtractorSubject::resetQueue();

	// Start with fixing the export range...
	m_bExporting   = true;
	m_pExportBuses = new QList<qtractorAudioBus *> (exportBuses);
	m_pExportFile  = pExportFile;
	m_pExportBuffer = new qtractorAudioExportBuffer(iChannels, bufferSize());
	m_iExportStart = iExportStart;
	m_iExportEnd   = iExportEnd;
	m_bExportDone  = false;

	// Prepare and show some progress...
	pProgressBar->setRange(iExportStart, iExportEnd);
	pProgressBar->reset();
	pProgressBar->show();

	// We'll have to save some session parameters...
	const unsigned long iPlayHead  = pSession->playHead();
	const unsigned long iLoopStart = pSession->loopStart();
	const unsigned long iLoopEnd   = pSession->loopEnd();

	QHash<qtractorAudioBus *, bool> exportMonitors;
	QListIterator<qtractorAudioBus *> bus_iter(exportBuses);
	while (bus_iter.hasNext()) {
		qtractorAudioBus *pAudioBus = bus_iter.next();
		exportMonitors.insert(pAudioBus, pAudioBus->isMonitor());
		pAudioBus->setMonitor(false);
	}

	// Make sure all track latencies are reset and
	// maximum track latency is acquainted as offset...
	m_iExportOffset = 0;

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (!pTrack->isMute() && (!pSession->soloTracks() || pTrack->isSolo())) {
			qtractorPluginList *pPluginList = pTrack->pluginList();
			if (pPluginList) {
				pPluginList->resetLatency();
				const unsigned long iLatency = pPluginList->latency();
				if (m_iExportOffset < iLatency)
					m_iExportOffset = iLatency;
			}
		}
	}

	if (m_iExportOffset > m_iExportStart)
		m_iExportOffset = m_iExportStart;

	// Because we'll have to set the export conditions...
	pSession->setLoop(0, 0);
	pSession->setPlayHead(m_iExportStart - m_iExportOffset);

	// Special initialization.
	m_iBufferOffset = 0;

	// Start export (freewheeling)...
	jack_set_freewheel(m_pJackClient, 1);

	// Wait for the export to end.
	struct timespec ts;
	ts.tv_sec  = 0;
	ts.tv_nsec = 20000000L; // 20msec.

	while (m_bExporting && !m_bExportDone) {
		qtractorSession::stabilize(200);
	#ifdef CONFIG_LV2
	#ifdef CONFIG_LV2_TIME
		qtractorLv2Plugin::updateTimePost();
	#endif
	#endif
		::nanosleep(&ts, NULL); // Ain't that enough?
		pProgressBar->setValue(pSession->playHead());
	}

	// Stop export (freewheeling)...
	jack_set_freewheel(m_pJackClient, 0);

	// May close the file...
	m_pExportFile->close();

	// Restore session at ease...
	pSession->setLoop(iLoopStart, iLoopEnd);
	pSession->setPlayHead(iPlayHead);

	bus_iter.toFront();
	while (bus_iter.hasNext()) {
		qtractorAudioBus *pAudioBus = bus_iter.next();
		pAudioBus->setMonitor(exportMonitors.value(pAudioBus, false));
	}

	// Check user cancellation...
	const bool bResult = m_bExporting;

	// Free up things here.
	delete m_pExportBuffer;
	delete m_pExportBuses;
	delete m_pExportFile;

	// Made some progress...
	pProgressBar->hide();

	m_bExporting   = false;
	m_pExportBuses = NULL;
	m_pExportFile  = NULL;
	m_pExportBuffer = NULL;
//	m_iExportStart = 0;
//	m_iExportEnd   = 0;
	m_bExportDone  = true;

	// Back to business..
	pSession->unlock();

	// Done whether successfully.
	return bResult;
}


// Special track-immediate methods.
void qtractorAudioEngine::trackMute ( qtractorTrack *pTrack, bool bMute )
{
	if (bMute) (pTrack->pluginList())->resetBuffers();

	sessionCursor()->updateTrackClip(pTrack);
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


// Metronome enabled accessors.
void qtractorAudioEngine::setMetroEnabled ( bool bMetroEnabled )
{
	m_bMetroEnabled = bMetroEnabled;
}

bool qtractorAudioEngine::isMetroEnabled (void) const
{
	return m_bMetroEnabled;
}


// Metronome bus mode accessors.
void qtractorAudioEngine::setMetroBus ( bool bMetroBus )
{
	qtractorBus::ConnectList outs;

	if (isActivated() && m_bMetroBus && m_pMetroBus)
		m_pMetroBus->updateConnects(qtractorBus::Output, outs);

	deleteMetroBus();

	m_bMetroBus = bMetroBus;

	createMetroBus();

	if (isActivated()) {
		openMetroBus();
		if (m_bMetroBus && m_pMetroBus) {
			if (!outs.isEmpty())
				m_pMetroBus->updateConnects(qtractorBus::Output, outs, true);
			else
				m_pMetroBus->autoConnect();
		}
	}
}

bool qtractorAudioEngine::isMetroBus (void) const
{
	return m_bMetroBus;
}

void qtractorAudioEngine::resetMetroBus (void)
{
	if (m_bMetronome && m_bMetroBus && m_pMetroBus)
		return;

	createMetroBus();

	if (isActivated())
		openMetroBus();
}


// Metronome bus defaults accessors.
void qtractorAudioEngine::setMetroAutoConnect ( bool bMetroAutoConnect )
{
	m_bMetroAutoConnect = bMetroAutoConnect;
}

bool qtractorAudioEngine::isMetroAutoConnect (void) const
{
	return m_bMetroAutoConnect;
}



// Metronome bar audio sample.
void qtractorAudioEngine::setMetroBarFilename ( const QString& sFilename )
{
	m_sMetroBarFilename = sFilename;
}

const QString& qtractorAudioEngine::metroBarFilename (void) const
{
	return m_sMetroBarFilename;
}


// Metronome bar audio sample gain.
void qtractorAudioEngine::setMetroBarGain ( float fGain )
{
	m_fMetroBarGain = fGain;
}

float qtractorAudioEngine::metroBarGain (void) const
{
	return m_fMetroBarGain;
}


// Metronome beat audio sample.
void qtractorAudioEngine::setMetroBeatFilename ( const QString& sFilename )
{
	m_sMetroBeatFilename = sFilename;
}

const QString& qtractorAudioEngine::metroBeatFilename() const
{
	return m_sMetroBeatFilename;
}


// Metronome beat audio sample gain.
void qtractorAudioEngine::setMetroBeatGain ( float fGain )
{
	m_fMetroBeatGain = fGain;
}

float qtractorAudioEngine::metroBeatGain (void) const
{
	return m_fMetroBeatGain;
}


// Metronome latency offset (in frames).
void qtractorAudioEngine::setMetroOffset ( unsigned long iMetroOffset )
{
	m_iMetroOffset = iMetroOffset;
}

unsigned long qtractorAudioEngine::metroOffset (void) const
{
	return m_iMetroOffset;
}


// Metronome latency offset compensation.
unsigned long qtractorAudioEngine::metro_offset ( unsigned long iFrame ) const
{
	const unsigned long iOffset = m_iMetroOffset
		+ (m_pMetroBus ? m_pMetroBus->latency_out() : 0);
	return (iFrame > iOffset ? iFrame - iOffset : iFrame);
}


// Create audio metronome stuff...
void qtractorAudioEngine::createMetroBus (void)
{
	deleteMetroBus();

	if (!m_bMetroEnabled)
		return;

	// Whether metronome bus is here owned, or...
	if (m_bMetroBus) {
		m_pMetroBus = new qtractorAudioBus(this, "Metronome",
			qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex));
		m_pMetroBus->setAutoConnect(m_bMetroAutoConnect);
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

	// Enough number of channels?...
	const unsigned short iChannels = m_pMetroBus->channels();
	if (iChannels < 1) {
		closeMetroBus();
		return false;
	}

	// We got it...
	m_pMetroBarBuff = new qtractorAudioBuffer(m_pSyncThread, iChannels);
	m_pMetroBarBuff->setGain(m_fMetroBarGain);
	m_pMetroBarBuff->open(m_sMetroBarFilename);

	m_pMetroBeatBuff = new qtractorAudioBuffer(m_pSyncThread, iChannels);
	m_pMetroBeatBuff->setGain(m_fMetroBeatGain);
	m_pMetroBeatBuff->open(m_sMetroBeatFilename);

	return true;
}


// Close audio metronome stuff.
void qtractorAudioEngine::closeMetroBus (void)
{
	if (m_pMetroBeatBuff) {
		m_pMetroBeatBuff->close();
		delete m_pMetroBeatBuff;
		m_pMetroBeatBuff = NULL;
	}

	if (m_pMetroBarBuff) {
		m_pMetroBarBuff->close();
		delete m_pMetroBarBuff;
		m_pMetroBarBuff = NULL;
	}

	if (m_bMetroBus && m_pMetroBus) {
		m_pMetroBus->close();
		removeBusEx(m_pMetroBus);
	}

	m_iMetroBeatStart = 0;
	m_iMetroBeat = 0;
}


// Destroy audio metronome stuff.
void qtractorAudioEngine::deleteMetroBus (void)
{
	closeMetroBus();

	if (m_bMetroBus && m_pMetroBus)
		delete m_pMetroBus;

	m_pMetroBus = NULL;
}


// Reset Audio metronome.
void qtractorAudioEngine::resetMetro (void)
{
	if (!m_bMetroEnabled)
		return;

	if (!m_bMetronome)
		return;

	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
		return;

	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Reset to the next beat position...
	unsigned long iFrame = pAudioCursor->frame();
	qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);

	// FIXME: Each sample buffer must be bounded properly...
	unsigned long iMaxLength = 0;
	const unsigned short iNextBeat = pNode->beatFromFrame(iFrame);
	if (iNextBeat > 0) {
		m_iMetroBeat = iNextBeat;
		m_iMetroBeatStart = metro_offset(pNode->frameFromBeat(m_iMetroBeat));
		iMaxLength = (m_iMetroBeatStart / m_iMetroBeat);
	} else {
		m_iMetroBeat = 0;
		m_iMetroBeatStart = 0;
		iMaxLength = pNode->frameFromBeat(1);
	}

	if (m_pMetroBarBuff) {
		const unsigned long iMetroBarLength
			= m_pMetroBarBuff->frames();
		m_pMetroBarBuff->setLength(
			iMetroBarLength > iMaxLength ? iMaxLength : iMetroBarLength);
		m_pMetroBarBuff->reset(false);
	}

	if (m_pMetroBeatBuff) {
		const unsigned long iMetroBeatLength
			= m_pMetroBeatBuff->frames();
		m_pMetroBeatBuff->setLength(
			iMetroBeatLength > iMaxLength ? iMaxLength : iMetroBeatLength);
		m_pMetroBeatBuff->reset(false);
	}
}


// Audition/pre-listening bus mode accessors.
void qtractorAudioEngine::setPlayerBus ( bool bPlayerBus )
{
	qtractorBus::ConnectList outs;

	if (isActivated() && m_bPlayerBus && m_pPlayerBus)
		m_pPlayerBus->updateConnects(qtractorBus::Output, outs);

	deletePlayerBus();

	m_bPlayerBus = bPlayerBus;

	createPlayerBus();

	if (isActivated()) {
		openPlayerBus();
		if (m_bPlayerBus && m_pPlayerBus) {
			if (!outs.isEmpty())
				m_pPlayerBus->updateConnects(qtractorBus::Output, outs, true);
			else
				m_pPlayerBus->autoConnect();
		}
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

	if (isActivated())
		openPlayerBus();
}


// Audition/pre-listening bus defaults accessors.
void qtractorAudioEngine::setPlayerAutoConnect ( bool bPlayerAutoConnect )
{
	m_bPlayerAutoConnect = bPlayerAutoConnect;
}

bool qtractorAudioEngine::isPlayerAutoConnect (void) const
{
	return m_bPlayerAutoConnect;
}


// Create audition/pre-listening stuff...
void qtractorAudioEngine::createPlayerBus (void)
{
	deletePlayerBus();

	// Whether audition/pre-listening bus is here owned, or...
	if (m_bPlayerBus) {
		m_pPlayerBus = new qtractorAudioBus(this, "Player",
			qtractorBus::BusMode(qtractorBus::Output | qtractorBus::Ex));
		m_pPlayerBus->setAutoConnect(m_bPlayerAutoConnect);
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

	if (m_bPlayerBus) {
		addBusEx(m_pPlayerBus);
		m_pPlayerBus->open();
	}

	// Enough number of channels?...
	const unsigned short iChannels = m_pPlayerBus->channels();
	if (iChannels < 1) {
		closePlayerBus();
		return false;
	}

	// We got it...
	m_pPlayerBuff = new qtractorAudioBuffer(m_pSyncThread, iChannels);

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

	if (m_bPlayerBus && m_pPlayerBus) {
		m_pPlayerBus->close();
		removeBusEx(m_pPlayerBus);
	}
}


// Destroy audition/pre-listening stuff...
void qtractorAudioEngine::deletePlayerBus (void)
{
	closePlayerBus();

	if (m_bPlayerBus && m_pPlayerBus)
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
	// Do it as usual, on all standard owned dependable buses...
	return qtractorEngine::updateConnects();
}


// JACK Transport mode accessors.
void qtractorAudioEngine::setTransportMode (
	qtractorBus::BusMode transportMode )
{
	m_transportMode = transportMode;
}

qtractorBus::BusMode qtractorAudioEngine::transportMode (void) const
{
	return m_transportMode;
}


// JACK Timebase mode accessors.
void qtractorAudioEngine::setTimebase ( bool bTimebase )
{
	m_bTimebase = bTimebase;
}

bool qtractorAudioEngine::isTimebase (void) const
{
	return m_bTimebase;
}


// JACK Timebase reset method.
void qtractorAudioEngine::resetTimebase (void)
{
	if (m_pJackClient == NULL)
		return;

	if (m_iTimebase > 0) {
		// Release being a timebase master, if any... 
		jack_release_timebase(m_pJackClient);
		m_iTimebase = 0;
	}

	if (m_bTimebase) {
		// Just force the timebase callback, maybe once again... 
		jack_set_timebase_callback(m_pJackClient,
			0 /* FIXME: un-conditional! */,
			qtractorAudioEngine_timebase, this);
	}
}


// Absolute number of frames elapsed since engine start.
unsigned long qtractorAudioEngine::jackFrameTime (void) const
{
	return (m_pJackClient ? jack_frame_time(m_pJackClient) : 0);
}


// Reset all audio monitoring...
void qtractorAudioEngine::resetAllMonitors (void)
{
	// There must a session reference...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// Reset all audio bus monitors...
	for (qtractorBus *pBus = buses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
		//	if (pAudioBus->audioMonitor_in())
		//		pAudioBus->audioMonitor_in()->reset();
			if (pAudioBus->audioMonitor_out())
				pAudioBus->audioMonitor_out()->reset();
		}
	}
	
	// Reset all audio track channel monitors...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Audio) {
			qtractorAudioMonitor *pAudioMonitor
				= static_cast<qtractorAudioMonitor *> (pTrack->monitor());
			if (pAudioMonitor)
				pAudioMonitor->reset();
			qtractorPluginList *pPluginList = pTrack->pluginList();
			if (pPluginList)
				pPluginList->resetLatency();
		}
	}
}


//----------------------------------------------------------------------
// class qtractorAudioBus -- Managed JACK port set
//

// Constructor.
qtractorAudioBus::qtractorAudioBus (
	qtractorAudioEngine *pAudioEngine, const QString& sBusName,
	BusMode busMode, bool bMonitor, unsigned short iChannels )
	: qtractorBus(pAudioEngine, sBusName, busMode, bMonitor)
{
	m_iChannels = iChannels;

	if ((busMode & qtractorBus::Input) && !(busMode & qtractorBus::Ex)) {
		m_pIAudioMonitor = new qtractorAudioMonitor(iChannels);
		m_pIPluginList   = createPluginList(qtractorPluginList::AudioInBus);
	} else {
		m_pIAudioMonitor = NULL;
		m_pIPluginList   = NULL;
	}

	if ((busMode & qtractorBus::Output) && !(busMode & qtractorBus::Ex)) {
		m_pOAudioMonitor = new qtractorAudioMonitor(iChannels);
		m_pOPluginList   = createPluginList(qtractorPluginList::AudioOutBus);
	} else {
		m_pOAudioMonitor = NULL;
		m_pOPluginList   = NULL;
	}

	m_bAutoConnect = false;

	m_ppIPorts  = NULL;
	m_ppOPorts  = NULL;

	m_ppIBuffer = NULL;
	m_ppOBuffer = NULL;

	m_ppXBuffer = NULL;
	m_ppYBuffer = NULL;

	m_bEnabled  = false;

#if defined(__SSE__)
	if (sse_enabled())
		m_pfnBufferAdd = sse_buffer_add;
	else
#endif
#if defined(__ARM_NEON__)
	m_pfnBufferAdd = neon_buffer_add;
	if (false)
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

#if 0
	if (m_pIPluginList)
		updatePluginList(m_pIPluginList, qtractorPluginList::AudioInBus);
	if (m_pOPluginList)
		updatePluginList(m_pOPluginList, qtractorPluginList::AudioOutBus);
#endif
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

	jack_client_t *pJackClient = pAudioEngine->jackClient();
	if (pJackClient == NULL)
		return false;

	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();
	const unsigned int iBufferSize
		= pAudioEngine->bufferSize();

	unsigned short i;
	unsigned short iDisabled = 0;

	if (busMode & qtractorBus::Input) {
		// Register and allocate input port buffers...
		m_ppIPorts  = new jack_port_t * [m_iChannels];
		m_ppIBuffer = new float * [m_iChannels];
		const QString sIPortName(busName() + "/in_%1");
		for (i = 0; i < m_iChannels; ++i) {
			m_ppIPorts[i] = jack_port_register(pJackClient,
				sIPortName.arg(i + 1).toUtf8().constData(),
				JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsInput, 0);
			m_ppIBuffer[i] = NULL;
			if (m_ppIPorts[i] == NULL) ++iDisabled;
		}
	}

	if (busMode & qtractorBus::Output) {
		// Register and allocate output port buffers...
		m_ppOPorts  = new jack_port_t * [m_iChannels];
		m_ppOBuffer = new float * [m_iChannels];
		const QString sOPortName(busName() + "/out_%1");
		for (i = 0; i < m_iChannels; ++i) {
			m_ppOPorts[i] = jack_port_register(pJackClient,
				sOPortName.arg(i + 1).toUtf8().constData(),
				JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsOutput, 0);
			m_ppOBuffer[i] = NULL;
			if (m_ppOPorts[i] == NULL) ++iDisabled;
		}
	}

	// Allocate internal working bus buffers...
	m_ppXBuffer = new float * [m_iChannels];
	m_ppYBuffer = new float * [m_iChannels];
	for (i = 0; i < m_iChannels; ++i) {
		m_ppXBuffer[i] = new float [iBufferSize];
		m_ppYBuffer[i] = NULL;
	}

	// Update monitor subject names...
	qtractorAudioBus::updateBusName();

	// Plugin lists need some buffer (re)allocation too...
	if (m_pIPluginList)
		updatePluginList(m_pIPluginList, qtractorPluginList::AudioInBus);
	if (m_pOPluginList)
		updatePluginList(m_pOPluginList, qtractorPluginList::AudioOutBus);

	// Finally, open for biz...
	m_bEnabled = (iDisabled == 0);

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

	jack_client_t *pJackClient = pAudioEngine->jackClient();

	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();

	unsigned short i;

	if (busMode & qtractorBus::Input) {
		// Unregister and free input ports,
		// if we're not shutdown...
		if (m_ppIPorts && pJackClient) {
			for (i = 0; i < m_iChannels; ++i) {
				if (m_ppIPorts[i]) {
					jack_port_unregister(pJackClient, m_ppIPorts[i]);
					m_ppIPorts[i] = NULL;
				}
			}
		}
		// Free input ports.
		if (m_ppIPorts)
			delete [] m_ppIPorts;
		m_ppIPorts = NULL;
		// Free input buffers.
		if (m_ppIBuffer)
			delete [] m_ppIBuffer;
		m_ppIBuffer = NULL;
	}

	if (busMode & qtractorBus::Output) {
		// Unregister and free output ports,
		// if we're not shutdown...
		if (m_ppOPorts && pJackClient) {
			for (i = 0; i < m_iChannels; ++i) {
				if (m_ppOPorts[i]) {
					jack_port_unregister(pJackClient, m_ppOPorts[i]);
					m_ppOPorts[i] = NULL;
				}
			}
		}
		// Free output ports.
		if (m_ppOPorts)
			delete [] m_ppOPorts;
		m_ppOPorts = NULL;
		// Free output buffers.
		if (m_ppOBuffer)
			delete [] m_ppOBuffer;
		m_ppOBuffer = NULL;
	}

	// Free internal buffers.
	if (m_ppXBuffer) {
		for (i = 0; i < m_iChannels; ++i)
			delete [] m_ppXBuffer[i];
		delete [] m_ppXBuffer;
		m_ppXBuffer = NULL;
	}

	if (m_ppYBuffer) {
		delete [] m_ppYBuffer;
		m_ppYBuffer = NULL;
	}
}


// Auto-connect to physical ports.
void qtractorAudioBus::autoConnect (void)
{
	if (!m_bAutoConnect)
		return;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	jack_client_t *pJackClient = pAudioEngine->jackClient();
	if (pJackClient == NULL)
		return;

	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();

	unsigned short i;

	if ((busMode & qtractorBus::Input) && inputs().isEmpty()) {
		const char **ppszOPorts
			= jack_get_ports(pJackClient,
				0, JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsOutput | JackPortIsPhysical);
		if (ppszOPorts) {
			const QString sIPortName = pAudioEngine->clientName()
				+ ':' + busName() + "/in_%1";
			for (i = 0; i < m_iChannels && ppszOPorts[i]; ++i) {
				jack_connect(pJackClient,
					ppszOPorts[i], sIPortName.arg(i + 1).toUtf8().constData());
			}
			::free(ppszOPorts);
		}
	}

	if ((busMode & qtractorBus::Output) && outputs().isEmpty()) {
		const char **ppszIPorts
			= jack_get_ports(pJackClient,
				0, JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsInput | JackPortIsPhysical);
		if (ppszIPorts) {
			const QString sOPortName = pAudioEngine->clientName()
				+ ':' + busName() + "/out_%1";
			for (i = 0; i < m_iChannels && ppszIPorts[i]; ++i) {
				jack_connect(pJackClient,
					sOPortName.arg(i + 1).toUtf8().constData(), ppszIPorts[i]);
			}
			::free(ppszIPorts);
		}
	}
}


// Bus mode change event.
void qtractorAudioBus::updateBusMode (void)
{
	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();

	// Have a new/old input monitor?
	if ((busMode & qtractorBus::Input) && !(busMode & qtractorBus::Ex)) {
		if (m_pIAudioMonitor == NULL)
			m_pIAudioMonitor = new qtractorAudioMonitor(m_iChannels);
		if (m_pIPluginList == NULL)
			m_pIPluginList = createPluginList(qtractorPluginList::AudioInBus);
	} else {
		if (m_pIAudioMonitor) {
			delete m_pIAudioMonitor;
			m_pIAudioMonitor = NULL;
		}
		if (m_pIPluginList) {
			delete m_pIPluginList;
			m_pIPluginList = NULL;
		}
	}

	// Have a new/old output monitor?
	if ((busMode & qtractorBus::Output) && !(busMode & qtractorBus::Ex)) {
		if (m_pOAudioMonitor == NULL)
			m_pOAudioMonitor = new qtractorAudioMonitor(m_iChannels);
		if (m_pOPluginList == NULL)
			m_pOPluginList = createPluginList(qtractorPluginList::AudioOutBus);
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


// Bus name change event.
void qtractorAudioBus::updateBusName (void)
{
	const QString& sBusName
		= qtractorAudioBus::busName();

	if (m_pIAudioMonitor) {
		const QString& sBusNameIn
			= QObject::tr("%1 In").arg(sBusName);
		m_pIAudioMonitor->gainSubject()->setName(
			QObject::tr("%1 Gain").arg(sBusNameIn));
		m_pIAudioMonitor->panningSubject()->setName(
			QObject::tr("%1 Pan").arg(sBusNameIn));
	}

	if (m_pOAudioMonitor) {
		const QString& sBusNameOut
			= QObject::tr("%1 Out").arg(sBusName);
		m_pOAudioMonitor->gainSubject()->setName(
			QObject::tr("%1 Gain").arg(sBusNameOut));
		m_pOAudioMonitor->panningSubject()->setName(
			QObject::tr("%1 Pan").arg(sBusNameOut));
	}

	qtractorBus::updateBusName();
}


// Process cycle preparator.
void qtractorAudioBus::process_prepare ( unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();

	unsigned short i;

	if (busMode & qtractorBus::Input) {
		for (i = 0; i < m_iChannels; ++i) {
			m_ppIBuffer[i] = static_cast<float *>
				(jack_port_get_buffer(m_ppIPorts[i], nframes));
		}
	}

	if (busMode & qtractorBus::Output) {
		for (i = 0; i < m_iChannels; ++i) {
			m_ppOBuffer[i] = static_cast<float *>
				(jack_port_get_buffer(m_ppOPorts[i], nframes));
			// Zero-out output buffer...
			::memset(m_ppOBuffer[i], 0, nframes * sizeof(float));
		}
	}
}


// Process cycle monitor.
void qtractorAudioBus::process_monitor ( unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();

	if (busMode & qtractorBus::Input) {
		if (m_pIPluginList)
			m_pIPluginList->process(m_ppIBuffer, nframes);
		if (m_pIAudioMonitor)
			m_pIAudioMonitor->process(m_ppIBuffer, nframes);
		if (isMonitor() && (busMode & qtractorBus::Output)) {
			(*m_pfnBufferAdd)(m_ppOBuffer, m_ppIBuffer,
				nframes, m_iChannels, m_iChannels, 0);
		}
	}
}


// Process cycle commitment.
void qtractorAudioBus::process_commit ( unsigned int nframes )
{
	if (!m_bEnabled)
		return;

	if (m_pOPluginList)
		m_pOPluginList->process(m_ppOBuffer, nframes);
	if (m_pOAudioMonitor)
		m_pOAudioMonitor->process(m_ppOBuffer, nframes);
}


// Bus-buffering methods.
void qtractorAudioBus::buffer_prepare (
	unsigned int nframes, qtractorAudioBus *pInputBus )
{
	if (!m_bEnabled)
		return;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	const unsigned int offset = pAudioEngine->bufferOffset();
	const unsigned int nbytes = nframes * sizeof(float);

	if (pInputBus == NULL) {
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			m_ppYBuffer[i] = m_ppXBuffer[i] + offset;
			::memset(m_ppYBuffer[i], 0, nbytes);
		}
		return;
	}

	const unsigned short iBuffers = pInputBus->channels();
	float **ppBuffer = pInputBus->in();

	if (m_iChannels == iBuffers) {
		// Exact buffer copy...
		for (unsigned short i = 0; i < iBuffers; ++i) {
			m_ppYBuffer[i] = m_ppXBuffer[i] + offset;
			::memcpy(m_ppYBuffer[i], ppBuffer[i] + offset, nbytes);
		}
	} else {
		// Buffer merge/multiplex...
		unsigned short i;
		for (i = 0; i < m_iChannels; ++i) {
			m_ppYBuffer[i] = m_ppXBuffer[i] + offset;
			::memset(m_ppYBuffer[i], 0, nbytes);
		}
		if (m_iChannels > iBuffers) {
			unsigned short j = 0;
			for (i = 0; i < m_iChannels; ++i) {
				::memcpy(m_ppYBuffer[i], ppBuffer[j] + offset, nbytes);
				if (++j >= iBuffers)
					j = 0;
			}
		} else { // (m_iChannels < iBuffers)
			(*m_pfnBufferAdd)(m_ppXBuffer, ppBuffer,
				nframes, m_iChannels, iBuffers, offset);
		}
	}
}

void qtractorAudioBus::buffer_commit ( unsigned int nframes )
{
	if (!m_bEnabled || (busMode() & qtractorBus::Output) == 0)
		return;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;

	(*m_pfnBufferAdd)(m_ppOBuffer, m_ppXBuffer,
		nframes, m_iChannels, m_iChannels, pAudioEngine->bufferOffset());
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


// Audio I/O port latency accessors.
unsigned int qtractorAudioBus::latency_in (void) const
{
	if (m_ppIPorts == NULL)
		return 0;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return 0;

	unsigned int iLatencyIn = pAudioEngine->bufferSize();

#ifdef CONFIG_JACK_LATENCY
	jack_nframes_t range_min = 0;
	jack_latency_range_t range;
	for (unsigned int i = 0; i < m_iChannels; ++i) {
		if (m_ppIPorts[i] == NULL)
			continue;
		jack_port_get_latency_range(m_ppIPorts[i], JackCaptureLatency, &range);
		if (range_min > range.min || i == 0)
			range_min = range.min;
	}
	iLatencyIn += range_min;
#else
	jack_nframes_t lat, lat_min = 0;
	for (unsigned int i = 0; i < m_iChannels; ++i) {
		if (m_ppIPorts[i] == NULL)
			continue;
		lat = jack_port_get_latency(m_ppIPorts[i]);
		if (lat_min > lat || i == 0)
			lat_min = lat;
	}
	iLatencyIn += lat_min;
#endif

	return iLatencyIn;
}

unsigned int qtractorAudioBus::latency_out (void) const
{
	if (m_ppOPorts == NULL)
		return 0;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return 0;

	unsigned int iLatencyOut = pAudioEngine->bufferSize();

#ifdef CONFIG_JACK_LATENCY
	jack_nframes_t range_min = 0;
	jack_latency_range_t range;
	for (unsigned int i = 0; i < m_iChannels; ++i) {
		if (m_ppOPorts[i] == NULL)
			continue;
		jack_port_get_latency_range(m_ppOPorts[i], JackPlaybackLatency, &range);
		if (range_min > range.min || i == 0)
			range_min = range.min;
	}
	iLatencyOut += range_min;
#else
	jack_nframes_t lat, lat_min = 0;
	for (unsigned int i = 0; i < m_iChannels; ++i) {
		if (m_ppOPorts[i] == NULL)
			continue;
		lat = jack_port_get_latency(m_ppOPorts[i]);
		if (lat_min > lat || i == 0)
			lat_min = lat;
	}
	iLatencyOut += lat_min;
#endif

	return iLatencyOut;
}


// Create plugin-list properly.
qtractorPluginList *qtractorAudioBus::createPluginList ( int iFlags ) const
{
	// Create plugin-list alright...
	qtractorPluginList *pPluginList	= new qtractorPluginList(0, iFlags);

	// Set plugin-list title name...
	updatePluginListName(pPluginList, iFlags);

	return pPluginList;
}


// Update plugin-list title name...
void qtractorAudioBus::updatePluginListName (
	qtractorPluginList *pPluginList, int iFlags ) const
{
	pPluginList->setName((iFlags & qtractorPluginList::In ?
		QObject::tr("%1 In") : QObject::tr("%1 Out")).arg(busName()));
}


// Update plugin-list name/buffers properly.
void qtractorAudioBus::updatePluginList (
	qtractorPluginList *pPluginList, int iFlags )
{
	// Set plugin-list title name...
	updatePluginListName(pPluginList, iFlags);

	// Set plugin-list buffer alright...
	pPluginList->setChannels(m_iChannels, iFlags);
}


// Retrieve all current JACK connections for a given bus mode interface;
// return the effective number of connection attempts...
int qtractorAudioBus::updateConnects (
	qtractorBus::BusMode busMode, ConnectList& connects, bool bConnect ) const
{
	// Modes must match, at least...
	if ((busMode & qtractorAudioBus::busMode()) == 0)
		return 0;

	if (bConnect && connects.isEmpty())
		return 0;

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return 0;

	jack_client_t *pJackClient = pAudioEngine->jackClient();
	if (pJackClient == NULL)
		return 0;

	// Which kind of ports?
	jack_port_t **ppPorts
		= (busMode == qtractorBus::Input ? m_ppIPorts : m_ppOPorts);
	if (ppPorts == NULL)
		return 0;

	// For each channel...
	ConnectItem item;
	for (item.index = 0; item.index < m_iChannels; ++item.index) {
		// Get port connections...
		const char **ppszClientPorts
			= jack_port_get_all_connections(pJackClient, ppPorts[item.index]);
		if (ppszClientPorts) {
			// Now, for each port...
			int iClientPort = 0;
			while (ppszClientPorts[iClientPort]) {
				// Check if already in list/connected...
				const QString sClientPort
					= QString::fromUtf8(ppszClientPorts[iClientPort]);
				item.clientName = sClientPort.section(':', 0, 0);
				item.portName   = sClientPort.section(':', 1, 1);
				ConnectItem *pItem = connects.findItem(item);
				if (pItem && bConnect) {
					const int iItem = connects.indexOf(pItem);
					if (iItem >= 0) {
						connects.removeAt(iItem);
						delete pItem;
					}
				}
				else if (!bConnect)
					connects.append(new ConnectItem(item));
				++iClientPort;
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
		if (jack_connect(pJackClient,
				sOutputPort.toUtf8().constData(),
				sInputPort.toUtf8().constData()) == 0) {
			const int iItem = connects.indexOf(pItem);
			if (iItem >= 0) {
				connects.removeAt(iItem);
				delete pItem;
				++iUpdate;
			}
		}
	}

	// Done.
	return iUpdate;
}


// Document element methods.
bool qtractorAudioBus::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	for (QDomNode nProp = pElement->firstChild();
			!nProp.isNull();
				nProp = nProp.nextSibling()) {
		// Convert audio-bus property to element...
		QDomElement eProp = nProp.toElement();
		if (eProp.isNull())
			continue;
		if (eProp.tagName() == "pass-through" || // Legacy compat.
			eProp.tagName() == "audio-thru"   ||
			eProp.tagName() == "monitor") {
			qtractorAudioBus::setMonitor(
				qtractorDocument::boolFromText(eProp.text()));
		} else if (eProp.tagName() == "channels") {
			qtractorAudioBus::setChannels(eProp.text().toUShort());
		} else if (eProp.tagName() == "auto-connect") {
			qtractorAudioBus::setAutoConnect(
				qtractorDocument::boolFromText(eProp.text()));
		} else if (eProp.tagName() == "input-gain") {
			if (qtractorAudioBus::monitor_in())
				qtractorAudioBus::monitor_in()->setGain(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-panning") {
			if (qtractorAudioBus::monitor_in())
				qtractorAudioBus::monitor_in()->setPanning(
					eProp.text().toFloat());
		} else if (eProp.tagName() == "input-controllers") {
			qtractorAudioBus::loadControllers(&eProp, qtractorBus::Input);
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
		} else if (eProp.tagName() == "output-controllers") {
			qtractorAudioBus::loadControllers(&eProp, qtractorBus::Output);
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


bool qtractorAudioBus::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	const qtractorBus::BusMode busMode
		= qtractorAudioBus::busMode();

	pElement->setAttribute("name",
		qtractorAudioBus::busName());
	pElement->setAttribute("mode",
		qtractorBus::textFromBusMode(busMode));

	pDocument->saveTextElement("monitor",
		qtractorDocument::textFromBool(
			qtractorAudioBus::isMonitor()), pElement);
	pDocument->saveTextElement("channels",
		QString::number(qtractorAudioBus::channels()), pElement);
	pDocument->saveTextElement("auto-connect",
		qtractorDocument::textFromBool(
			qtractorAudioBus::isAutoConnect()), pElement);

	if (busMode & qtractorBus::Input) {
		if (qtractorAudioBus::monitor_in()) {
			pDocument->saveTextElement("input-gain",
				QString::number(qtractorAudioBus::monitor_in()->gain()),
					pElement);
			pDocument->saveTextElement("input-panning",
				QString::number(qtractorAudioBus::monitor_in()->panning()),
					pElement);
		}
		// Save input bus controllers...
		QDomElement eInputControllers
			= pDocument->document()->createElement("input-controllers");
		qtractorAudioBus::saveControllers(pDocument,
			&eInputControllers, qtractorBus::Input);
		pElement->appendChild(eInputControllers);
		// Save input bus plugins...
		if (qtractorAudioBus::pluginList_in()) {
			QDomElement eInputPlugins
				= pDocument->document()->createElement("input-plugins");
			qtractorAudioBus::pluginList_in()->saveElement(
				pDocument, &eInputPlugins);
			pElement->appendChild(eInputPlugins);
		}
		// Save input bus connections...
		QDomElement eAudioInputs
			= pDocument->document()->createElement("input-connects");
		qtractorBus::ConnectList inputs;
		qtractorAudioBus::updateConnects(qtractorBus::Input, inputs);
		qtractorAudioBus::saveConnects(inputs, pDocument, &eAudioInputs);
		pElement->appendChild(eAudioInputs);
	}

	if (busMode & qtractorBus::Output) {
		if (qtractorAudioBus::monitor_out()) {
			pDocument->saveTextElement("output-gain",
				QString::number(qtractorAudioBus::monitor_out()->gain()),
					pElement);
			pDocument->saveTextElement("output-panning",
				QString::number(qtractorAudioBus::monitor_out()->panning()),
					pElement);
		}
		// Save output bus controllers...
		QDomElement eOutputControllers
			= pDocument->document()->createElement("output-controllers");
		qtractorAudioBus::saveControllers(pDocument,
			&eOutputControllers, qtractorBus::Output);
		pElement->appendChild(eOutputControllers);
		// Save output bus plugins...
		if (qtractorAudioBus::pluginList_out()) {
			QDomElement eOutputPlugins
				= pDocument->document()->createElement("output-plugins");
			qtractorAudioBus::pluginList_out()->saveElement(
				pDocument, &eOutputPlugins);
			pElement->appendChild(eOutputPlugins);
		}
		// Save output bus connections...
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
