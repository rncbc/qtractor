// qtractorCoreAudioEngine.cpp
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
#include "qtractorCoreAudioEngine.h"
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
// class qtractorCoreAudioEngine -- CoreAudio client instance (singleton).
//

// Constructor.
qtractorCoreAudioEngine::qtractorCoreAudioEngine(qtractorSession *pSession)
	: qtractorEngine(pSession, qtractorTrack::Audio),
#ifdef __APPLE__
	m_coreAudioDevice(kAudioObjectUnknown),
	m_audioUnit(nullptr),
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
#ifdef __APPLE__
	// Initialize stream format structure
	memset(&m_streamFormat, 0, sizeof(m_streamFormat));
#endif
}


// Engine initialization.
bool qtractorCoreAudioEngine::init()
{
#ifdef __APPLE__
	OSStatus status;
	
	// Get default audio device
	UInt32 size = sizeof(m_coreAudioDevice);
	status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
		&size, &m_coreAudioDevice);
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::init(): Cannot get default audio device");
		return false;
	}

	// Create output audio unit (HAL Output)
	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	if (!comp) {
		qWarning("qtractorCoreAudioEngine::init(): Cannot find HAL Output component");
		return false;
	}

	status = AudioComponentInstanceNew(comp, &m_audioUnit);
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::init(): Cannot create audio unit");
		return false;
	}

	// Set the device for the HAL output
	status = AudioUnitSetProperty(m_audioUnit,
		kAudioOutputUnitProperty_CurrentDevice,
		kAudioUnitScope_Global, 0,
		&m_coreAudioDevice, sizeof(m_coreAudioDevice));
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::init(): Cannot set audio device");
		return false;
	}

	// Get stream format from device
	size = sizeof(m_streamFormat);
	status = AudioUnitGetProperty(m_audioUnit,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Input, 0,
		&m_streamFormat, &size);
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::init(): Cannot get stream format");
		return false;
	}

	m_iSampleRate = static_cast<unsigned int>(m_streamFormat.mSampleRate);

	// Set render callback
	AURenderCallbackStruct callbackStruct;
	callbackStruct.inputProc = renderCallback;
	callbackStruct.inputProcRefCon = this;

	status = AudioUnitSetProperty(m_audioUnit,
		kAudioUnitProperty_SetRenderCallback,
		kAudioUnitScope_Global, 0,
		&callbackStruct, sizeof(callbackStruct));
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::init(): Cannot set render callback");
		return false;
	}

#endif // __APPLE__

	return true;
}


// Special event notifier proxy object.
const qtractorCoreAudioEngineProxy *qtractorCoreAudioEngine::proxy() const
{
	return &m_proxy;
}


// Event notifications.
void qtractorCoreAudioEngine::notifyShutEvent()
{
	m_proxy.notifyShutEvent();
}

void qtractorCoreAudioEngine::notifyXrunEvent()
{
	m_proxy.notifyXrunEvent();
}

void qtractorCoreAudioEngine::notifyPortEvent()
{
	m_proxy.notifyPortEvent();
}

void qtractorCoreAudioEngine::notifyBuffEvent(unsigned int iBufferSize)
{
	m_proxy.notifyBuffEvent(iBufferSize);
}

void qtractorCoreAudioEngine::notifySessEvent(void *pvSessionArg)
{
	m_proxy.notifySessEvent(pvSessionArg);
}

void qtractorCoreAudioEngine::notifySyncEvent(unsigned long iPlayHead, bool bPlaying)
{
	m_proxy.notifySyncEvent(iPlayHead, bPlaying);
}

void qtractorCoreAudioEngine::notifyPropEvent()
{
	m_proxy.notifyPropEvent();
}

void qtractorCoreAudioEngine::notifySelfEvent()
{
	m_proxy.notifySelfEvent();
}


#ifdef __APPLE__
// CoreAudio device descriptor accessor.
AudioDeviceID qtractorCoreAudioEngine::coreAudioDevice() const
{
	return m_coreAudioDevice;
}

AudioUnit qtractorCoreAudioEngine::audioUnit() const
{
	return m_audioUnit;
}
#endif


// Process cycle executive.
int qtractorCoreAudioEngine::process(unsigned int nframes)
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


#ifdef __APPLE__
// CoreAudio render callback.
OSStatus qtractorCoreAudioEngine::renderCallback(
	void *inRefCon,
	AudioUnitRenderActionFlags *ioActionFlags,
	const AudioTimeStamp *inTimeStamp,
	UInt32 inBusNumber,
	UInt32 inNumberFrames,
	AudioBufferList *ioData)
{
	qtractorCoreAudioEngine *pEngine = static_cast<qtractorCoreAudioEngine *>(inRefCon);
	if (!pEngine) return noErr;

	// Process audio
	pEngine->process(inNumberFrames);

	// Here we would normally fill the ioData buffers with processed audio
	// For now, return silence (this is a placeholder implementation)
	for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
		float *buffer = static_cast<float *>(ioData->mBuffers[i].mData);
		if (buffer) {
			memset(buffer, 0, ioData->mBuffers[i].mDataByteSize);
		}
	}

	return noErr;
}
#endif


// Concrete device (de)activation methods.
bool qtractorCoreAudioEngine::activate()
{
#ifdef __APPLE__
	if (!m_audioUnit) return false;

	OSStatus status = AudioUnitInitialize(m_audioUnit);
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::activate(): Cannot initialize audio unit");
		return false;
	}

	status = AudioOutputUnitStart(m_audioUnit);
	if (status != noErr) {
		qWarning("qtractorCoreAudioEngine::activate(): Cannot start audio output");
		AudioUnitUninitialize(m_audioUnit);
		return false;
	}
#endif

	return true;
}

bool qtractorCoreAudioEngine::start()
{
	return true;
}

void qtractorCoreAudioEngine::stop()
{
}

void qtractorCoreAudioEngine::deactivate()
{
#ifdef __APPLE__
	if (m_audioUnit) {
		AudioOutputUnitStop(m_audioUnit);
		AudioUnitUninitialize(m_audioUnit);
	}
#endif
}

void qtractorCoreAudioEngine::clean()
{
}


// Document element methods.
bool qtractorCoreAudioEngine::loadElement(
	qtractorDocument *pDocument, QDomElement *pElement)
{
	// Load engine-specific settings from document
	return qtractorEngine::loadElement(pDocument, pElement);
}

bool qtractorCoreAudioEngine::saveElement(
	qtractorDocument *pDocument, QDomElement *pElement) const
{
	// Save engine-specific settings to document
	return qtractorEngine::saveElement(pDocument, pElement);
}


// Session UUID accessors.
void qtractorCoreAudioEngine::setSessionId(const QString& sSessionId)
{
	m_sSessionId = sSessionId;
}

const QString& qtractorCoreAudioEngine::sessionId() const
{
	return m_sSessionId;
}


// Internal sample-rate accessor.
unsigned int qtractorCoreAudioEngine::sampleRate() const
{
	return m_iSampleRate;
}


// Buffer size accessors.
unsigned int qtractorCoreAudioEngine::bufferSize() const
{
	return m_iBufferSize;
}

unsigned int qtractorCoreAudioEngine::bufferSizeEx() const
{
	return m_iBufferSizeEx;
}


// Buffer offset accessor.
unsigned int qtractorCoreAudioEngine::bufferOffset() const
{
	return m_iBufferOffset;
}


// Block-stride size (in frames) accessor.
unsigned int qtractorCoreAudioEngine::blockSize() const
{
	return m_iBlockSize;
}


// Audio (Master) bus defaults accessors.
void qtractorCoreAudioEngine::setMasterAutoConnect(bool bMasterAutoConnect)
{
	m_bMasterAutoConnect = bMasterAutoConnect;
}

bool qtractorCoreAudioEngine::isMasterAutoConnect() const
{
	return m_bMasterAutoConnect;
}


// Audio-export freewheeling (internal) state.
void qtractorCoreAudioEngine::setFreewheel(bool bFreewheel)
{
	m_bFreewheel = bFreewheel;
}

bool qtractorCoreAudioEngine::isFreewheel() const
{
	return m_bFreewheel;
}


// Audio-export active state.
void qtractorCoreAudioEngine::setExporting(bool bExporting)
{
	m_bExporting = bExporting;
}

bool qtractorCoreAudioEngine::isExporting() const
{
	return m_bExporting;
}


// Last known export extents accessors.
unsigned long qtractorCoreAudioEngine::exportStart() const
{
	return m_iExportStart;
}

unsigned long qtractorCoreAudioEngine::exportOffset() const
{
	return m_iExportOffset;
}

unsigned long qtractorCoreAudioEngine::exportLength() const
{
	return (m_iExportEnd > m_iExportStart ? m_iExportEnd - m_iExportStart : 0);
}


// Whether we're in the audio/real-time thread...
bool qtractorCoreAudioEngine::isProcessing()
{
	return g_bProcessing;
}


// end of qtractorCoreAudioEngine.cpp
