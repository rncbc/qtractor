// qtractorCoreMidiEngine.cpp
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
#include "qtractorCoreMidiEngine.h"
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

#include <cmath>


// Specific controller definitions
#define BANK_SELECT_MSB         0x00
#define BANK_SELECT_LSB         0x20

#define ALL_SOUND_OFF           0x78
#define ALL_CONTROLLERS_OFF     0x79
#define ALL_NOTES_OFF           0x7b

#define CHANNEL_VOLUME          0x07
#define CHANNEL_PANNING         0x0a


//----------------------------------------------------------------------
// class qtractorCoreMidiEngine -- CoreMIDI client instance (singleton).
//

// Constructor.
qtractorCoreMidiEngine::qtractorCoreMidiEngine(qtractorSession *pSession)
	: qtractorEngine(pSession, qtractorTrack::Midi),
#ifdef __APPLE__
	m_coreMidiClient(0),
	m_inputPort(0),
	m_outputPort(0),
#endif
	m_iReadAhead(0),
	m_bDriftCorrect(true),
	m_iDriftCheck(DRIFT_CHECK),
	m_iDriftCount(0),
	m_iTimeDrift(0),
	m_iFrameDrift(0),
	m_iTimeStart(0),
	m_iFrameStart(0),
	m_iTimeStartEx(0),
	m_iAudioFrameStart(0),
	m_bControlBus(false),
	m_pIControlBus(nullptr),
	m_pOControlBus(nullptr),
	m_bMetronome(false),
	m_bMetroBus(false),
	m_pMetroBus(nullptr),
	m_iMetroChannel(0),
	m_iMetroBarNote(60),
	m_iMetroBarVelocity(100),
	m_iMetroBarDuration(480),
	m_iMetroBeatNote(60),
	m_iMetroBeatVelocity(80),
	m_iMetroBeatDuration(480),
	m_iMetroOffset(0),
	m_bMetroEnabled(false),
	m_pMetroCursor(nullptr),
	m_fMetroTempo(0.0f),
	m_bCountIn(false),
	m_countInMode(CountInNone),
	m_iCountInBeats(0),
	m_iCountIn(0),
	m_iCountInFrame(0),
	m_iCountInFrameStart(0),
	m_iCountInFrameEnd(0),
	m_iCountInTimeStart(0),
	m_bPlayerBus(false),
	m_pPlayerBus(nullptr),
	m_pPlayer(nullptr),
	m_iCaptureQuantize(0),
	m_iResetAllControllersPending(0),
	m_mmcDevice(0x7f),
	m_mmcMode(qtractorBus::None),
	m_sppMode(qtractorBus::None),
	m_clockMode(qtractorBus::None),
	m_bResetAllControllers(false),
	m_iClockCount(0),
	m_fClockTempo(0.0f)
{
}


// Engine initialization.
bool qtractorCoreMidiEngine::init()
{
#ifdef __APPLE__
	CFStringRef name = CFStringCreateWithCString(
		kCFAllocatorDefault, "NebulaQuasar", kCFStringEncodingUTF8);

	OSStatus status = MIDIClientCreate(name, nullptr, nullptr, &m_coreMidiClient);
	if (status != noErr) {
		qWarning("qtractorCoreMidiEngine::init(): Cannot create MIDI client");
		CFRelease(name);
		return false;
	}

	status = MIDIInputPortCreate(m_coreMidiClient,
		CFSTR("Input Port"), midiReadCallback, this, &m_inputPort);
	if (status != noErr) {
		qWarning("qtractorCoreMidiEngine::init(): Cannot create MIDI input port");
		CFRelease(name);
		return false;
	}

	status = MIDIOutputPortCreate(m_coreMidiClient,
		CFSTR("Output Port"), &m_outputPort);
	if (status != noErr) {
		qWarning("qtractorCoreMidiEngine::init(): Cannot create MIDI output port");
		CFRelease(name);
		return false;
	}

	CFRelease(name);

	// Enumerate and connect to available MIDI devices
	ItemCount numSources = MIDIGetNumberOfSources();
	for (ItemCount i = 0; i < numSources; ++i) {
		MIDIEndpointRef source = MIDIGetSource(i);
		if (source) {
			MIDIPortConnectSource(m_inputPort, source, nullptr);
		}
	}
#endif

	return true;
}


// Special event notifier proxy object.
qtractorCoreMidiEngineProxy *qtractorCoreMidiEngine::proxy()
{
	return &m_proxy;
}


#ifdef __APPLE__
// CoreMIDI client descriptor accessor.
MIDIClientRef qtractorCoreMidiEngine::coreMidiClient() const
{
	return m_coreMidiClient;
}
#endif


#ifdef __APPLE__
// CoreMIDI read callback.
void qtractorCoreMidiEngine::midiReadCallback(
	const MIDIPacketList *pktlist,
	void *readProcRefCon,
	void *srcConnRefCon)
{
	qtractorCoreMidiEngine *pEngine = static_cast<qtractorCoreMidiEngine *>(readProcRefCon);
	if (!pEngine) return;

	const MIDIPacket *packet = pktlist->packet;
	for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
		pEngine->capture(packet);
		packet = MIDIPacketNext(packet);
	}
}
#endif


// Read ahead frames configuration.
void qtractorCoreMidiEngine::setReadAhead(unsigned int iReadAhead)
{
	m_iReadAhead = iReadAhead;
}

unsigned int qtractorCoreMidiEngine::readAhead() const
{
	return m_iReadAhead;
}


// MIDI output process cycle iteration.
void qtractorCoreMidiEngine::process()
{
	qtractorSession *pSession = session();
	if (!pSession) return;

	// Process all MIDI tracks
	pSession->processMidi();
}


// Special slave sync method.
void qtractorCoreMidiEngine::sync()
{
	// Sync implementation
}


// Reset queue time.
void qtractorCoreMidiEngine::resetTime()
{
	m_iTimeStart = 0;
	m_iFrameStart = 0;
	m_iTimeStartEx = 0;
}

void qtractorCoreMidiEngine::resetSync()
{
	resetTime();
}


// Reset queue tempo.
void qtractorCoreMidiEngine::resetTempo()
{
	m_fMetroTempo = 0.0f;
}


// Reset all MIDI monitoring...
void qtractorCoreMidiEngine::resetAllMonitors()
{
	// Reset all monitors
}


// Reset all MIDI controllers...
void qtractorCoreMidiEngine::resetAllControllers(bool bForceImmediate)
{
	++m_iResetAllControllersPending;
}

bool qtractorCoreMidiEngine::isResetAllControllersPending() const
{
	return (m_iResetAllControllersPending > 0);
}


// Shut-off all MIDI buses (stop)...
void qtractorCoreMidiEngine::shutOffAllBuses(bool bClose)
{
	// Shut off all buses
}


// Shut-off all MIDI tracks (panic)...
void qtractorCoreMidiEngine::shutOffAllTracks()
{
	// Panic - shut off all notes
}


#ifdef __APPLE__
// MIDI event capture method.
void qtractorCoreMidiEngine::capture(const MIDIPacket *pPacket)
{
	if (!pPacket) return;

	const unsigned char *data = pPacket->data;
	UInt32 length = pPacket->length;

	// Parse MIDI data and process events
	for (UInt32 i = 0; i < length; ) {
		unsigned char status = data[i] & 0xF0;
		unsigned char channel = data[i] & 0x0F;
		
		switch (status) {
		case 0x80: // Note Off
		case 0x90: // Note On
		case 0xA0: // Polyphonic Aftertouch
		case 0xB0: // Control Change
		case 0xE0: // Pitch Bend
			if (i + 2 < length) {
				// Process 3-byte message
				i += 3;
			} else {
				return;
			}
			break;
		case 0xC0: // Program Change
		case 0xD0: // Channel Aftertouch
			if (i + 1 < length) {
				// Process 2-byte message
				i += 2;
			} else {
				return;
			}
			break;
		case 0xF0: // System messages
			// Handle system messages
			++i;
			break;
		default:
			// Running status or data byte
			++i;
			break;
		}
	}
}
#endif


// MIDI event enqueue method.
void qtractorCoreMidiEngine::enqueue(qtractorTrack *pTrack,
	qtractorMidiEvent *pEvent, unsigned long iTime, float fGain)
{
	// Enqueue MIDI event for playback
}


// Flush ouput queue (if necessary)...
void qtractorCoreMidiEngine::flush()
{
	// Flush output queue
}


// The delta-time/frame accessors.
long qtractorCoreMidiEngine::timeStart() const
{
	return m_iTimeStart;
}


// The absolute-time/frame accessors.
unsigned long qtractorCoreMidiEngine::timeStartEx() const
{
	return m_iTimeStartEx;
}


// Special track-immediate methods.
void qtractorCoreMidiEngine::trackMute(qtractorTrack *pTrack, bool bMute)
{
	// Mute/unmute track
}


// Special metronome-immediate methods.
void qtractorCoreMidiEngine::metroMute(bool bMute)
{
	// Mute/unmute metronome
}


// Metronome switching.
void qtractorCoreMidiEngine::setMetronome(bool bMetronome)
{
	m_bMetronome = bMetronome;
}

bool qtractorCoreMidiEngine::isMetronome() const
{
	return m_bMetronome;
}


// Metronome enabled accessors.
void qtractorCoreMidiEngine::setMetroEnabled(bool bMetroEnabled)
{
	m_bMetroEnabled = bMetroEnabled;
}

bool qtractorCoreMidiEngine::isMetroEnabled() const
{
	return m_bMetroEnabled;
}


// Concrete device (de)activation methods.
bool qtractorCoreMidiEngine::activate()
{
#ifdef __APPLE__
	if (!m_coreMidiClient) return false;
#endif
	return true;
}

bool qtractorCoreMidiEngine::start()
{
	return true;
}

void qtractorCoreMidiEngine::stop()
{
}

void qtractorCoreMidiEngine::deactivate()
{
}

void qtractorCoreMidiEngine::clean()
{
}


// Document element methods.
bool qtractorCoreMidiEngine::loadElement(
	qtractorDocument *pDocument, QDomElement *pElement)
{
	return qtractorEngine::loadElement(pDocument, pElement);
}

bool qtractorCoreMidiEngine::saveElement(
	qtractorDocument *pDocument, QDomElement *pElement) const
{
	return qtractorEngine::saveElement(pDocument, pElement);
}


// end of qtractorCoreMidiEngine.cpp
