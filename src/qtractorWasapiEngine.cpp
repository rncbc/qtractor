// qtractorWasapiEngine.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2024, Nebula Audio. All rights reserved.

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
#include "qtractorWasapiEngine.h"
#include "qtractorAudioMonitor.h"
#include "qtractorAudioBuffer.h"

#include "qtractorSession.h"

#include "qtractorDocument.h"

#include "qtractorMonitor.h"
#include "qtractorSessionCursor.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiManager.h"
#include "qtractorPlugin.h"

#include "qtractorMainForm.h"

#ifdef CONFIG_VST3
#include "qtractorVst3Plugin.h"
#endif

#ifdef CONFIG_CLAP
#include "qtractorClapPlugin.h"
#endif

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_TIME
#include "qtractorLv2Plugin.h"
#endif
#endif

#include "qtractorInsertPlugin.h"

#include <QApplication>
#include <QProgressBar>
#include <QDomDocument>


// Sensible defaults.
#define SAMPLE_RATE 44100
#define BUFFER_SIZE 1024
#define BLOCK_SIZE  64


// Global processing flag (thread-local).
static thread_local bool g_bProcessing = false;


//----------------------------------------------------------------------
// class qtractorWasapiEngine -- WASAPI client instance (singleton).
//

// Constructor.
qtractorWasapiEngine::qtractorWasapiEngine(qtractorSession *pSession)
	: qtractorEngine(pSession, qtractorTrack::Audio),
#ifdef _WIN32
	m_pWasapiDevice(nullptr),
	m_pAudioClient(nullptr),
	m_pRenderClient(nullptr),
	m_hRenderEvent(nullptr),
	m_hRenderThread(nullptr),
	m_pWaveFormat(nullptr),
	m_bufferSizeFrames(0),
#endif
	m_iSampleRate(SAMPLE_RATE),
	m_iBufferSize(BUFFER_SIZE),
	m_iBufferSizeEx(BUFFER_SIZE * 4),
	m_iBufferOffset(0),
	m_iBlockSize(BLOCK_SIZE),
	m_bMasterAutoConnect(false),
	m_bFreewheel(false),
	m_pSyncThread(nullptr),
	m_bExporting(false),
	m_pExportFile(nullptr),
	m_iExportOffset(0),
	m_iExportStart(0),
	m_iExportEnd(0),
	m_bExportDone(false),
	m_pExportBuses(nullptr),
	m_pExportBuffer(nullptr),
	m_bMetronome(false),
	m_bMetroBus(false),
	m_pMetroBus(nullptr),
	m_bMetroAutoConnect(false),
	m_pMetroBarBuff(nullptr),
	m_fMetroBarGain(1.0f),
	m_pMetroBeatBuff(nullptr),
	m_fMetroBeatGain(1.0f),
	m_iMetroOffset(0),
	m_iMetroBeatStart(0),
	m_iMetroBeat(0),
	m_bMetroEnabled(false),
	m_bCountIn(false),
	m_countInMode(CountInNone),
	m_iCountInBeats(0),
	m_iCountIn(0),
	m_iCountInBeat(0),
	m_iCountInBeatStart(0),
	m_iCountInFrame(0),
	m_iCountInFrameEnd(0),
	m_bPlayerOpen(false),
	m_bPlayerBus(false),
	m_pPlayerBus(nullptr),
	m_bPlayerAutoConnect(false),
	m_pPlayerBuff(nullptr),
	m_iPlayerFrame(0),
	m_transportMode(qtractorBus::None),
	m_iTransportLatency(0),
	m_bTimebase(false),
	m_iTimebase(0),
	m_captureLatencyMode(Auto),
	m_iCaptureLatency(0)
{
}


// Engine initialization.
bool qtractorWasapiEngine::init()
{
#ifdef _WIN32
	HRESULT hr;

	// Initialize COM
	hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
		qWarning("qtractorWasapiEngine::init(): Cannot initialize COM");
		return false;
	}

	// Get default audio endpoint
	IMMDeviceEnumerator *pEnumerator = nullptr;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&pEnumerator));
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::init(): Cannot create device enumerator");
		CoUninitialize();
		return false;
	}

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pWasapiDevice);
	pEnumerator->Release();
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::init(): Cannot get default audio endpoint");
		CoUninitialize();
		return false;
	}

	// Activate audio client
	hr = m_pWasapiDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
		nullptr, reinterpret_cast<void**>(&m_pAudioClient));
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::init(): Cannot activate audio client");
		m_pWasapiDevice->Release();
		m_pWasapiDevice = nullptr;
		CoUninitialize();
		return false;
	}

	// Get mix format
	WAVEFORMATEX *pMixFormat = nullptr;
	hr = m_pAudioClient->GetMixFormat(&pMixFormat);
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::init(): Cannot get mix format");
		m_pAudioClient->Release();
		m_pAudioClient = nullptr;
		m_pWasapiDevice->Release();
		m_pWasapiDevice = nullptr;
		CoUninitialize();
		return false;
	}

	m_iSampleRate = pMixFormat->nSamplesPerSec;
	CoTaskMemFree(pMixFormat);

	// Create render event
	m_hRenderEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_hRenderEvent) {
		qWarning("qtractorWasapiEngine::init(): Cannot create render event");
		m_pAudioClient->Release();
		m_pAudioClient = nullptr;
		m_pWasapiDevice->Release();
		m_pWasapiDevice = nullptr;
		CoUninitialize();
		return false;
	}

#endif // _WIN32

	return true;
}


// Special event notifier proxy object.
const qtractorWasapiEngineProxy *qtractorWasapiEngine::proxy() const
{
	return &m_proxy;
}


// Event notifications.
void qtractorWasapiEngine::notifyShutEvent()
{
	m_proxy.notifyShutEvent();
}

void qtractorWasapiEngine::notifyXrunEvent()
{
	m_proxy.notifyXrunEvent();
}

void qtractorWasapiEngine::notifyPortEvent()
{
	m_proxy.notifyPortEvent();
}

void qtractorWasapiEngine::notifyBuffEvent(unsigned int iBufferSize)
{
	m_proxy.notifyBuffEvent(iBufferSize);
}

void qtractorWasapiEngine::notifySessEvent(void *pvSessionArg)
{
	m_proxy.notifySessEvent(pvSessionArg);
}

void qtractorWasapiEngine::notifySyncEvent(unsigned long iPlayHead, bool bPlaying)
{
	m_proxy.notifySyncEvent(iPlayHead, bPlaying);
}

void qtractorWasapiEngine::notifyPropEvent()
{
	m_proxy.notifyPropEvent();
}

void qtractorWasapiEngine::notifySelfEvent()
{
	m_proxy.notifySelfEvent();
}


#ifdef _WIN32
// WASAPI device descriptor accessor.
IMMDevice *qtractorWasapiEngine::wasapiDevice() const
{
	return m_pWasapiDevice;
}

IAudioClient *qtractorWasapiEngine::audioClient() const
{
	return m_pAudioClient;
}

IAudioRenderClient *qtractorWasapiEngine::renderClient() const
{
	return m_pRenderClient;
}
#endif


// Process cycle executive.
int qtractorWasapiEngine::process(unsigned int nframes)
{
	g_bProcessing = true;

	qtractorSession *pSession = session();
	if (!pSession) {
		g_bProcessing = false;
		return 0;
	}

	// Update time info
	updateTimeInfo(pSession->playHead());

	// Process all tracks
	pSession->process(nframes);

	g_bProcessing = false;
	return 0;
}


// Concrete device (de)activation methods.
bool qtractorWasapiEngine::activate()
{
#ifdef _WIN32
	if (!m_pAudioClient) return false;

	HRESULT hr;

	// Get buffer size
	hr = m_pAudioClient->GetDevicePeriod(nullptr, nullptr);
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::activate(): Cannot get device period");
		return false;
	}

	// Initialize audio client
	hr = m_pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		0, 0, nullptr, nullptr);
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::activate(): Cannot initialize audio client");
		return false;
	}

	// Get render client
	hr = m_pAudioClient->GetService(__uuidof(IAudioRenderClient),
		reinterpret_cast<void**>(&m_pRenderClient));
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::activate(): Cannot get render client");
		return false;
	}

	// Start rendering
	hr = m_pAudioClient->Start();
	if (FAILED(hr)) {
		qWarning("qtractorWasapiEngine::activate(): Cannot start audio client");
		m_pRenderClient->Release();
		m_pRenderClient = nullptr;
		return false;
	}
#endif

	return true;
}

bool qtractorWasapiEngine::start()
{
	return true;
}

void qtractorWasapiEngine::stop()
{
}

void qtractorWasapiEngine::deactivate()
{
#ifdef _WIN32
	if (m_pAudioClient) {
		m_pAudioClient->Stop();
	}
	if (m_pRenderClient) {
		m_pRenderClient->Release();
		m_pRenderClient = nullptr;
	}
#endif
}

void qtractorWasapiEngine::clean()
{
}


// Document element methods.
bool qtractorWasapiEngine::loadElement(
	qtractorDocument *pDocument, QDomElement *pElement)
{
	return qtractorEngine::loadElement(pDocument, pElement);
}

bool qtractorWasapiEngine::saveElement(
	qtractorDocument *pDocument, QDomElement *pElement) const
{
	return qtractorEngine::saveElement(pDocument, pElement);
}


// Session UUID accessors.
void qtractorWasapiEngine::setSessionId(const QString& sSessionId)
{
	m_sSessionId = sSessionId;
}

const QString& qtractorWasapiEngine::sessionId() const
{
	return m_sSessionId;
}


// Internal sample-rate accessor.
unsigned int qtractorWasapiEngine::sampleRate() const
{
	return m_iSampleRate;
}


// Buffer size accessors.
unsigned int qtractorWasapiEngine::bufferSize() const
{
	return m_iBufferSize;
}

unsigned int qtractorWasapiEngine::bufferSizeEx() const
{
	return m_iBufferSizeEx;
}


// Buffer offset accessor.
unsigned int qtractorWasapiEngine::bufferOffset() const
{
	return m_iBufferOffset;
}


// Block-stride size (in frames) accessor.
unsigned int qtractorWasapiEngine::blockSize() const
{
	return m_iBlockSize;
}


// Audio (Master) bus defaults accessors.
void qtractorWasapiEngine::setMasterAutoConnect(bool bMasterAutoConnect)
{
	m_bMasterAutoConnect = bMasterAutoConnect;
}

bool qtractorWasapiEngine::isMasterAutoConnect() const
{
	return m_bMasterAutoConnect;
}


// Audio-export freewheeling (internal) state.
void qtractorWasapiEngine::setFreewheel(bool bFreewheel)
{
	m_bFreewheel = bFreewheel;
}

bool qtractorWasapiEngine::isFreewheel() const
{
	return m_bFreewheel;
}


// Audio-export active state.
void qtractorWasapiEngine::setExporting(bool bExporting)
{
	m_bExporting = bExporting;
}

bool qtractorWasapiEngine::isExporting() const
{
	return m_bExporting;
}


// Last known export extents accessors.
unsigned long qtractorWasapiEngine::exportStart() const
{
	return m_iExportStart;
}

unsigned long qtractorWasapiEngine::exportOffset() const
{
	return m_iExportOffset;
}

unsigned long qtractorWasapiEngine::exportLength() const
{
	return (m_iExportEnd > m_iExportStart ? m_iExportEnd - m_iExportStart : 0);
}


// Whether we're in the audio/real-time thread...
bool qtractorWasapiEngine::isProcessing()
{
	return g_bProcessing;
}


// end of qtractorWasapiEngine.cpp
