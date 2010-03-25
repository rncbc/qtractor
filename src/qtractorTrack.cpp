// qtractorTrack.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTrack.h"

#include "qtractorSession.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"

#include "qtractorSessionDocument.h"
#include "qtractorAudioEngine.h"
#include "qtractorAudioMonitor.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"
#include "qtractorMidiBuffer.h"
#include "qtractorInstrument.h"
#include "qtractorPlugin.h"
#include "qtractorMixer.h"

#include "qtractorMainForm.h"

#include <QPainter>


//-------------------------------------------------------------------------
// qtractorTrack::Properties -- Track properties structure.

// Helper clear/reset method.
void qtractorTrack::Properties::clear (void)
{
	trackName.clear();
	trackType   = None;
	monitor     = false;
	record      = false;
	mute        = false;
	solo        = false;
	gain        = 1.0f;
	panning     = 0.0f;
	inputBusName.clear();
	outputBusName.clear();
	midiOmni    = false;
	midiChannel = 0;
	midiBankSelMethod = -1;
	midiBank    = -1;
	midiProgram = -1;
	foreground  = Qt::yellow;
	background  = Qt::darkBlue;
}

// Helper copy method.
qtractorTrack::Properties& qtractorTrack::Properties::copy (
	const Properties& props )
{
	if (&props != this) {
		trackName   = props.trackName;
		trackType   = props.trackType;
		monitor     = props.monitor;
		record      = props.record;
		mute        = props.mute;
		solo        = props.solo;
		gain        = props.gain;
		panning     = props.panning;
		inputBusName  = props.inputBusName;
		outputBusName = props.outputBusName;
		midiOmni    = props.midiOmni;
		midiChannel = props.midiChannel;
		midiBankSelMethod = props.midiBankSelMethod;
		midiBank    = props.midiBank;
		midiProgram = props.midiProgram;
		foreground  = props.foreground;
		background  = props.background;
	}
	return *this;
}


//-------------------------------------------------------------------------
// qtractorTrack -- Track container.

// Constructor.
qtractorTrack::qtractorTrack ( qtractorSession *pSession, TrackType trackType )
{
	m_pSession = pSession;

	m_props.trackType = trackType;

	m_pInputBus  = NULL;
	m_pOutputBus = NULL;
	m_pMonitor   = NULL;
	m_iMidiTag   = 0;

	m_pClipRecord = NULL;

	m_clips.setAutoDelete(true);

	unsigned int iFlags = qtractorPluginList::Track;
	if (trackType == qtractorTrack::Midi)
		iFlags |= qtractorPluginList::Midi;

	m_pPluginList = new qtractorPluginList(0, 0, pSession->sampleRate(), iFlags);

	setHeight(HeightBase);	// Default track height.
	clear();
}

// Default constructor.
qtractorTrack::~qtractorTrack (void)
{
	close();
	clear();

	if (m_pPluginList)
		delete m_pPluginList;
	if (m_pMonitor)
		delete m_pMonitor;
}


// Reset track.
void qtractorTrack::clear (void)
{
	setClipRecord(NULL);

	m_clips.clear();

	m_props.midiBankSelMethod = -1;
	m_props.midiBank          = -1;
	m_props.midiProgram       = -1;

	m_props.monitor = false;
	m_props.record  = false;
	m_props.mute    = false;
	m_props.solo    = false;
	m_props.gain    = 1.0f;
	m_props.panning = 0.0f;
}


// Track open method.
bool qtractorTrack::open (void)
{
	close();

	if (m_pSession == NULL)
		return false;

	// Depending on track type...
	qtractorEngine *pEngine = NULL;
	qtractorAudioEngine *pAudioEngine = m_pSession->audioEngine();
	qtractorMidiEngine  *pMidiEngine  = m_pSession->midiEngine();
	switch (m_props.trackType) {
	case qtractorTrack::Audio:
		pEngine = pAudioEngine;
		break;
	case qtractorTrack::Midi:
		pEngine = pMidiEngine;
		break;
	default:
		break;
	}

	// Got it?
	if (pEngine == NULL)
		return false;

	// (Re)assign the input bus to the track.
	m_pInputBus = pEngine->findInputBus(inputBusName());
	// Fallback to first usable one...
	if (m_pInputBus == NULL) {
		for (qtractorBus *pBus = pEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Input) {
				m_pInputBus = pBus;
				break;
			}
		}
		// Set the bus name back...
		if (m_pInputBus)
			setInputBusName(m_pInputBus->busName());
	}

	// (Re)assign the output bus to the track.
	m_pOutputBus = pEngine->findOutputBus(outputBusName());
	// Fallback to first usable one...
	if (m_pOutputBus == NULL) {
		for (qtractorBus *pBus = pEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				m_pOutputBus = pBus;
				break;
			}
		}
		// Set the bus name back...
		if (m_pOutputBus)
			setOutputBusName(m_pOutputBus->busName());
	}

#if 0
	// Check proper bus assignment...
	if (m_pInputBus == NULL || m_pOutputBus == NULL)
		return false;
#endif

	// Remember current (output) monitor, for later deletion...
	qtractorMonitor *pMonitor = m_pMonitor;
	m_pMonitor = NULL;

	// (Re)allocate (output) monitor...
	switch (m_props.trackType) {
	case qtractorTrack::Audio: {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (m_pOutputBus);
		if (pAudioBus) {
			m_pMonitor = new qtractorAudioMonitor(pAudioBus->channels(),
				m_props.gain, m_props.panning);
			m_pPluginList->setBuffer(pAudioBus->channels(),
				pAudioEngine->bufferSize(), m_pSession->sampleRate(),
				qtractorPluginList::AudioTrack);
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (m_pOutputBus);
		if (pMidiBus) {
			m_pMonitor = new qtractorMidiMonitor(
				m_props.gain, m_props.panning);
		}
		// Get audio bus as for the plugin list...
		qtractorAudioBus *pAudioBus = NULL;
		qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
		if (pMidiManager)
			pAudioBus = pMidiManager->audioOutputBus();
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
		// Set plugin-list buffer alright...
		if (pAudioBus) {
			m_pPluginList->setBuffer(pAudioBus->channels(),
				pAudioEngine->bufferSize(), m_pSession->sampleRate(),
				qtractorPluginList::MidiTrack);
		}
		break;
	}
	default:
		break;
	}

	// Ah, at least make new name feedback...
	m_pPluginList->setName(trackName());

	// Mixer turn, before we get rid of old monitor...
	if (pMonitor) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorMixer *pMixer = pMainForm->mixer();
			if (pMixer) {
				qtractorMixerStrip *pStrip
					= pMixer->trackRack()->findStrip(pMonitor);
				if (pStrip)
					pStrip->setTrack(this);
			}
		}
		// That's it...
		delete pMonitor;
	}

	// Done.
	return (m_pMonitor != NULL);
}


// Track close method.
void qtractorTrack::close (void)
{
#if 0
	if (m_pMonitor) {
		delete m_pMonitor;
		m_pMonitor = NULL;
	}
#endif

	m_pInputBus  = NULL;
	m_pOutputBus = NULL;

	setClipRecord(NULL);
}


// Session accessor.
qtractorSession *qtractorTrack::session (void) const
{
	return m_pSession;
}


// Track name accessors.
const QString& qtractorTrack::trackName (void) const
{
	return m_props.trackName;
}

void qtractorTrack::setTrackName ( const QString& sTrackName )
{
	m_props.trackName = sTrackName;
}


// Track type accessors.
qtractorTrack::TrackType qtractorTrack::trackType (void) const
{
	return m_props.trackType;
}

void qtractorTrack::setTrackType ( qtractorTrack::TrackType trackType )
{
	// Don't change anything if we're already the same type...
	if (m_props.trackType == trackType)
		return;

	// Acquire a new midi-tag...
	if (m_props.trackType == qtractorTrack::Midi)
		m_pSession->releaseMidiTag(this);

	// Set new track type, now...
	m_props.trackType = trackType;

	// Acquire a new midi-tag and setup new plugin-list flags...
	unsigned int iFlags = qtractorPluginList::Track;
	if (m_props.trackType == qtractorTrack::Midi) {
		m_pSession->acquireMidiTag(this);
		iFlags |= qtractorPluginList::Midi;
	}

	// (Re)set plugin-list...
	m_pPluginList->setBuffer(0, 0, m_pSession->sampleRate(), iFlags);
}


// Record monitoring status accessors.
bool qtractorTrack::isMonitor (void) const
{
	return m_props.monitor;
}

void qtractorTrack::setMonitor ( bool bMonitor )
{
	m_props.monitor = bMonitor;
}


// Record status accessors.
bool qtractorTrack::isRecord (void) const
{
	return m_props.record;
}

void qtractorTrack::setRecord ( bool bRecord )
{
	if ((m_props.record && !bRecord) || (!m_props.record && bRecord))
		m_pSession->setRecordTracks(bRecord);

	m_props.record = bRecord;

	if (m_pSession->isRecording())
		m_pSession->trackRecord(this, bRecord);
}


// Mute status accessors.
bool qtractorTrack::isMute (void) const
{
	return m_props.mute;
}

void qtractorTrack::setMute ( bool bMute )
{
	if (m_pSession->isPlaying() && bMute)
		m_pSession->trackMute(this, bMute);

	if ((m_props.mute && !bMute) || (!m_props.mute && bMute))
		m_pSession->setMuteTracks(bMute);

	m_props.mute = bMute;

	if (m_pSession->isPlaying() && !bMute)
		m_pSession->trackMute(this, bMute);
}


// Solo status accessors.
bool qtractorTrack::isSolo (void) const
{
	return m_props.solo;
}

void qtractorTrack::setSolo ( bool bSolo )
{
	if (m_pSession->isPlaying() && bSolo)
		m_pSession->trackSolo(this, bSolo);

	if ((m_props.solo && !bSolo) || (!m_props.solo && bSolo))
		m_pSession->setSoloTracks(bSolo);

	m_props.solo = bSolo;

	if (m_pSession->isPlaying() && !bSolo)
		m_pSession->trackSolo(this, bSolo);
}


// Track gain (volume) accessor.
void qtractorTrack::setGain ( float fGain )
{
	m_props.gain = fGain;

	if (m_pMonitor)
		m_pMonitor->setGain(fGain);
}

float qtractorTrack::gain (void) const
{
	return (m_pMonitor ? m_pMonitor->gain() : m_props.gain);
}


// Track stereo-panning accessor.
void qtractorTrack::setPanning ( float fPanning )
{
	m_props.panning = fPanning;

	if (m_pMonitor)
		m_pMonitor->setPanning(fPanning);
}

float qtractorTrack::panning (void) const
{
	return (m_pMonitor ? m_pMonitor->panning() : m_props.panning);
}


// MIDI specific: track-tag accessors.
void qtractorTrack::setMidiTag ( unsigned short iMidiTag )
{
	m_iMidiTag = (iMidiTag % 0xff);
}

unsigned short qtractorTrack::midiTag (void) const
{
	return m_iMidiTag;
}


// MIDI specific: omni (capture) mode acessors.
void qtractorTrack::setMidiOmni ( bool bMidiOmni )
{
	m_props.midiOmni = bMidiOmni;
}

bool qtractorTrack::isMidiOmni (void) const
{
	return m_props.midiOmni;
}


// MIDI specific: channel acessors.
void qtractorTrack::setMidiChannel ( unsigned short iMidiChannel )
{
	m_props.midiChannel = iMidiChannel;
}

unsigned short qtractorTrack::midiChannel (void) const
{
	return m_props.midiChannel;
}


// MIDI specific: bank accessors.
void qtractorTrack::setMidiBankSelMethod ( int iMidiBankSelMethod )
{
	m_props.midiBankSelMethod = iMidiBankSelMethod;
}

int qtractorTrack::midiBankSelMethod (void) const
{
	return m_props.midiBankSelMethod;
}


// MIDI specific: bank accessors.
void qtractorTrack::setMidiBank ( int iMidiBank )
{
	m_props.midiBank = iMidiBank;
}

int qtractorTrack::midiBank (void) const
{
	return m_props.midiBank;
}


// MIDI specific: program accessors.
void qtractorTrack::setMidiProgram ( int iMidiProgram )
{
	m_props.midiProgram = iMidiProgram;
}

int qtractorTrack::midiProgram (void) const
{
	return m_props.midiProgram;
}


// Assigned input bus name accessors.
void qtractorTrack::setInputBusName ( const QString& sBusName )
{
	m_props.inputBusName = sBusName;
}

const QString& qtractorTrack::inputBusName (void) const
{
	return m_props.inputBusName;
}


// Assigned output bus name accessors.
void qtractorTrack::setOutputBusName ( const QString& sBusName )
{
	m_props.outputBusName = sBusName;
}

const QString& qtractorTrack::outputBusName (void) const
{
	return m_props.outputBusName;
}


// Assigned audio bus accessors.
qtractorBus *qtractorTrack::inputBus (void) const
{
	return m_pInputBus;
}

qtractorBus *qtractorTrack::outputBus (void) const
{
	return m_pOutputBus;
}


// Track monitor accessors.
qtractorMonitor *qtractorTrack::monitor (void) const
{
	return m_pMonitor;
}


// Track plugin-chain accessors.
qtractorPluginList *qtractorTrack::pluginList (void) const
{
	return m_pPluginList;
}


// Normalized view height accessors.
int qtractorTrack::height (void) const
{
	return m_iHeight;
}

void qtractorTrack::setHeight ( int iHeight )
{
	m_iHeight = iHeight;
	if (m_iHeight < HeightMin)
		m_iHeight = HeightMin;

	updateZoomHeight();
}

void qtractorTrack::updateHeight (void)
{
	if (m_pSession) {
		m_iHeight = (100 * m_iZoomHeight) / m_pSession->verticalZoom();
		if (m_iHeight < HeightMin)
			m_iHeight = HeightMin;
	}
}


// Zoomed view height accessors.
int qtractorTrack::zoomHeight (void) const
{
	return m_iZoomHeight;
}

void qtractorTrack::setZoomHeight ( int iZoomHeight )
{
	m_iZoomHeight = iZoomHeight;
	if (m_iZoomHeight < HeightMin)
		m_iZoomHeight = HeightMin;

	updateHeight();
}

void qtractorTrack::updateZoomHeight (void)
{
	if (m_pSession) {
		m_iZoomHeight = (m_iHeight * m_pSession->verticalZoom()) / 100;
		if (m_iZoomHeight < HeightMin)
			m_iZoomHeight = HeightMin;
	}
}


// Clip list management methods.
const qtractorList<qtractorClip>& qtractorTrack::clips (void) const
{
	return m_clips;
}


// Insert a new clip in garanteed sorted fashion.
void qtractorTrack::addClip ( qtractorClip *pClip )
{
	// Preliminary settings...
	pClip->setTrack(this);
	pClip->open();

	// Special case for initial MIDI tracks...
	if (m_props.trackType == qtractorTrack::Midi) {
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			if (midiBank() < 0)
				setMidiBank(pMidiClip->bank());
			if (midiProgram() < 0)
				setMidiProgram(pMidiClip->program());
		}
	}

	// Now do insert the clip in proper place in track...
	insertClip(pClip);
}

void qtractorTrack::insertClip ( qtractorClip *pClip )
{
	qtractorClip *pNextClip = m_clips.first();
	while (pNextClip && pNextClip->clipStart() < pClip->clipStart())
		pNextClip = pNextClip->next();
	if (pNextClip)
		m_clips.insertBefore(pClip, pNextClip);
	else
		m_clips.append(pClip);
}


void qtractorTrack::unlinkClip ( qtractorClip *pClip )
{
	m_clips.unlink(pClip);
}

void qtractorTrack::removeClip ( qtractorClip *pClip )
{
//	pClip->setTrack(NULL);
	pClip->close(false);

	unlinkClip(pClip);
}


// Current clip on record (capture).
void qtractorTrack::setClipRecord ( qtractorClip *pClipRecord )
{
	if (m_pClipRecord)
		delete m_pClipRecord;

	m_pClipRecord = pClipRecord;
}

qtractorClip *qtractorTrack::clipRecord (void) const
{
	return m_pClipRecord;
}


// Background color accessors.
void qtractorTrack::setBackground ( const QColor& bg )
{
	m_props.background = bg;
	m_props.background.setAlpha(192);
}

const QColor& qtractorTrack::background (void) const
{
	return m_props.background;
}


// Foreground color accessors.
void qtractorTrack::setForeground ( const QColor& fg )
{
	m_props.foreground = fg;
}

const QColor& qtractorTrack::foreground (void) const
{
	return m_props.foreground;
}


// Generate a default track color.
QColor qtractorTrack::trackColor ( int iTrack )
{
	int c[3] = { 0xff, 0xcc, 0x99 };

	return QColor(c[iTrack % 3], c[(iTrack / 3) % 3], c[(iTrack / 9) % 3]);
}


// Alternate properties accessor.
qtractorTrack::Properties& qtractorTrack::properties (void)
{
	return m_props;
}


// Track special process cycle executive.
void qtractorTrack::process ( qtractorClip *pClip,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Audio-buffers needs some preparation...
	unsigned int nframes = iFrameEnd - iFrameStart;
	qtractorAudioMonitor *pAudioMonitor = NULL;
	qtractorAudioBus *pOutputBus = NULL;
	if (m_props.trackType == qtractorTrack::Audio) {
		pAudioMonitor = static_cast<qtractorAudioMonitor *> (m_pMonitor);
		pOutputBus = static_cast<qtractorAudioBus *> (m_pOutputBus);
		// Prepare this track buffer...
		if (pOutputBus) {
			qtractorAudioBus *pInputBus = (isMonitor()
				? static_cast<qtractorAudioBus *> (m_pInputBus) : NULL);
			pOutputBus->buffer_prepare(nframes, pInputBus);
		}
	}

	// Playback...
	if (!isMute() && (!m_pSession->soloTracks() || isSolo())) {
		// Now, for every clip...
		while (pClip && pClip->clipStart() < iFrameEnd) {
			if (iFrameStart < pClip->clipStart() + pClip->clipLength())
				pClip->process(iFrameStart, iFrameEnd);
			pClip = pClip->next();
		}
		// Audio buffers needs monitoring and commitment...
		if (pAudioMonitor && pOutputBus) {
			// Plugin chain post-processing...
			if (m_pPluginList->activated() > 0)
				m_pPluginList->process(pOutputBus->buffer(), nframes);
			// Monitor passthru...
			pAudioMonitor->process(pOutputBus->buffer(), nframes);
			// Actually render it...
			pOutputBus->buffer_commit(nframes);
		}
	}
}


// Track special process record executive (audio recording only).
void qtractorTrack::process_record (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Audio track-recording?
	qtractorAudioBus *pInputBus
		= static_cast<qtractorAudioBus *> (m_pInputBus);
	qtractorAudioClip *pAudioClip
		= static_cast<qtractorAudioClip *> (m_pClipRecord);
	if (pAudioClip && pInputBus) {
		// Clip recording...
		unsigned int nframes = iFrameEnd - iFrameStart;
		if (m_pSession->isPunching()) {
			// Punch-in/out recording...
			unsigned long iPunchIn  = m_pSession->punchIn();
			unsigned long iPunchOut = m_pSession->punchOut();
			if (iPunchIn < iFrameEnd && iPunchOut > iFrameStart) {
				unsigned int offs = 0;
				// Punch-out (unlikely...)
				if (iPunchIn >= iFrameStart) {
					offs += (iPunchIn - iFrameStart);
					if (iPunchOut < iFrameEnd)
						nframes = (iPunchOut - iPunchIn);
					else
						nframes = (iFrameEnd - iPunchIn);
				}
				else
				// Punch-out (likely...)
				if (iPunchOut < iFrameEnd)
					nframes = (iPunchOut - iFrameStart);
				// Punch-in/out recording...
				pAudioClip->write(
					pInputBus->in(), nframes, pInputBus->channels(), offs);
			}
		} else {
			// Regular full-length recording...
			pAudioClip->write(
				pInputBus->in(), nframes, pInputBus->channels());
		}
		// Record non-passthru metering...
		qtractorAudioMonitor *pAudioMonitor
			= static_cast<qtractorAudioMonitor *> (m_pMonitor);
		if (pAudioMonitor) {
			pAudioMonitor->process_meter(
				pInputBus->in(), nframes, pInputBus->channels());
		}
	}
}


// Track paint method.
void qtractorTrack::drawTrack ( QPainter *pPainter, const QRect& trackRect,
	unsigned long iTrackStart, unsigned long iTrackEnd, qtractorClip *pClip )
{
	int y = trackRect.y();
	int h = trackRect.height();

	if (pClip == NULL)
		pClip = m_clips.first();

	while (pClip) {
		unsigned long iClipStart = pClip->clipStart();
		if (iClipStart > iTrackEnd)
			break;
		unsigned long iClipEnd = iClipStart + pClip->clipLength();
		if (iClipStart < iTrackEnd && iClipEnd > iTrackStart) {
			unsigned long iClipOffset = 0;
			int x = trackRect.x();
			int w = trackRect.width();
			if (iClipStart >= iTrackStart) {
				x += m_pSession->pixelFromFrame(iClipStart - iTrackStart);
			} else {
				iClipOffset = iTrackStart - iClipStart;
				x--;	// Give some clip left-border room.
			}
			if (iClipEnd < iTrackEnd) {
				w -= m_pSession->pixelFromFrame(iTrackEnd - iClipEnd);// + 1;
			} else {
				w++;	// Give some clip right-border room.
			}
			QRect rect(x, y, w - x, h);
			pClip->drawClip(pPainter, rect, iClipOffset);
			// Draw the clip selection...
			if (pClip->isClipSelected()) {
				unsigned long iSelectStart = pClip->clipSelectStart();
				unsigned long iSelectEnd   = pClip->clipSelectEnd();
				x = trackRect.x();
				w = trackRect.width();
				if (iSelectStart >= iTrackStart) {
					x += m_pSession->pixelFromFrame(iSelectStart - iTrackStart);
				} else {
					x--;	// Give selection some left-border room.
				}
				if (iSelectEnd < iTrackEnd) {
					w -= m_pSession->pixelFromFrame(iTrackEnd - iSelectEnd);
				} else {
					w++;	// Give selection some right-border room.
				}
				rect.setRect(x, y, w - x, h);
				pPainter->fillRect(rect, QColor(0, 0, 255, 120));
			}
		}
		pClip = pClip->next();
	}
}


// Track loop point setler.
void qtractorTrack::setLoop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
	qtractorClip *pClip = m_clips.first();
	while (pClip) {
		// Convert loop-points from session to clip...
		unsigned long iClipStart = pClip->clipStart();
		unsigned long iClipEnd   = iClipStart + pClip->clipLength();
		if (iLoopStart < iClipEnd && iLoopEnd > iClipStart) {
			// Set clip inner-loop...
			pClip->setClipLoop(
				(iLoopStart > iClipStart ? iLoopStart - iClipStart : 0),
				(iLoopEnd < iClipEnd ? iLoopEnd : iClipEnd) - iClipStart) ;
		} else {
			// Clear/reaet clip-loop...
			pClip->setClipLoop(0, 0);
		}
		pClip = pClip->next();
	}
}


// MIDI track instrument patching.
void qtractorTrack::setMidiPatch ( qtractorInstrumentList *pInstruments )
{
	if (midiProgram() < 0)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pOutputBus);
	if (pMidiBus == NULL)
		return;

	const qtractorMidiBus::Patch& patch = pMidiBus->patch(midiChannel());
	int iBankSelMethod = patch.bankSelMethod;
	if (!patch.instrumentName.isEmpty() && iBankSelMethod < 0)
		iBankSelMethod = (*pInstruments)[patch.instrumentName].bankSelMethod();

	pMidiBus->setPatch(midiChannel(), patch.instrumentName,
		iBankSelMethod, midiBank(), midiProgram(), this);
}


// Update all clips editors.
void qtractorTrack::updateClipEditors (void)
{
	qtractorClip *pClip = m_clips.first();
	while (pClip) {
		pClip->updateEditor(true);
		pClip = pClip->next();
	}
}


// Document element methods.
bool qtractorTrack::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	qtractorTrack::setTrackName(pElement->attribute("name"));
	qtractorTrack::setTrackType(
		pDocument->loadTrackType(pElement->attribute("type")));

	// Load track children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Load (other) track properties..
		if (eChild.tagName() == "properties") {
			for (QDomNode nProp = eChild.firstChild();
					!nProp.isNull();
						nProp = nProp.nextSibling()) {
				// Convert property node to element...
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				if (eProp.tagName() == "input-bus")
					qtractorTrack::setInputBusName(eProp.text());
				else if (eProp.tagName() == "output-bus")
					qtractorTrack::setOutputBusName(eProp.text());
				else if (eProp.tagName() == "midi-omni")
					qtractorTrack::setMidiOmni(pDocument->boolFromText(eProp.text()));
				else if (eProp.tagName() == "midi-channel")
					qtractorTrack::setMidiChannel(eProp.text().toUShort());
				else if (eProp.tagName() == "midi-bank-sel-method")
					qtractorTrack::setMidiBankSelMethod(eProp.text().toInt());
				else if (eProp.tagName() == "midi-bank")
					qtractorTrack::setMidiBank(eProp.text().toInt());
				else if (eProp.tagName() == "midi-program")
					qtractorTrack::setMidiProgram(eProp.text().toInt());
			}
		}
		else
		// Load track state..
		if (eChild.tagName() == "state") {
			for (QDomNode nState = eChild.firstChild();
					!nState.isNull();
						nState = nState.nextSibling()) {
				// Convert state node to element...
				QDomElement eState = nState.toElement();
				if (eState.isNull())
					continue;
				if (eState.tagName() == "mute")
					qtractorTrack::setMute(pDocument->boolFromText(eState.text()));
				else if (eState.tagName() == "solo")
					qtractorTrack::setSolo(pDocument->boolFromText(eState.text()));
				else if (eState.tagName() == "record")
					qtractorTrack::setRecord(pDocument->boolFromText(eState.text()));
				else if (eState.tagName() == "monitor")
					qtractorTrack::setMonitor(pDocument->boolFromText(eState.text()));
				else if (eState.tagName() == "gain")
					qtractorTrack::setGain(eState.text().toFloat());
				else if (eState.tagName() == "panning")
					qtractorTrack::setPanning(eState.text().toFloat());
			}
		}
		else
		if (eChild.tagName() == "view") {
			for (QDomNode nView = eChild.firstChild();
					!nView.isNull();
						nView = nView.nextSibling()) {
				// Convert view node to element...
				QDomElement eView = nView.toElement();
				if (eView.isNull())
					continue;
				if (eView.tagName() == "height") {
					qtractorTrack::setHeight(eView.text().toInt());
				} else if (eView.tagName() == "background-color") {
					QColor bg; bg.setNamedColor(eView.text());
					qtractorTrack::setBackground(bg);
				} else if (eView.tagName() == "foreground-color") {
					QColor fg; fg.setNamedColor(eView.text());
					qtractorTrack::setForeground(fg);
				}
			}
		}
		else
		// Load clips...
		if (eChild.tagName() == "clips" && !pDocument->isTemplate()) {
			for (QDomNode nClip = eChild.firstChild();
					!nClip.isNull();
						nClip = nClip.nextSibling()) {
				// Convert clip node to element...
				QDomElement eClip = nClip.toElement();
				if (eClip.isNull())
					continue;
				if (eClip.tagName() == "clip") {
					qtractorClip *pClip = NULL;
					switch (qtractorTrack::trackType()) {
						case qtractorTrack::Audio:
							pClip = new qtractorAudioClip(this);
							break;
						case qtractorTrack::Midi:
							pClip = new qtractorMidiClip(this);
							break;
						case qtractorTrack::None:
						default:
							break;
					}
					if (pClip == NULL)
						return false;
					if (!pClip->loadElement(pDocument, &eClip))
						return false;
					qtractorTrack::addClip(pClip);
				}
			}
		}
		else
		// Load plugins...
		if (eChild.tagName() == "plugins")
			m_pPluginList->loadElement(pDocument, &eChild);
	}

	return true;
}


bool qtractorTrack::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	pElement->setAttribute("name", qtractorTrack::trackName());
	pElement->setAttribute("type",
		pDocument->saveTrackType(qtractorTrack::trackType()));

	// Save track properties...
	QDomElement eProps = pDocument->document()->createElement("properties");
	pDocument->saveTextElement("input-bus",
		qtractorTrack::inputBusName(), &eProps);
	pDocument->saveTextElement("output-bus",
		qtractorTrack::outputBusName(), &eProps);
	if (qtractorTrack::trackType() == qtractorTrack::Midi) {
		pDocument->saveTextElement("midi-omni",
			pDocument->textFromBool(qtractorTrack::isMidiOmni()), &eProps);
		pDocument->saveTextElement("midi-channel",
			QString::number(qtractorTrack::midiChannel()), &eProps);
		if (qtractorTrack::midiBankSelMethod() >= 0) {
			pDocument->saveTextElement("midi-bank-sel-method",
				QString::number(qtractorTrack::midiBankSelMethod()), &eProps);
		}
		if (qtractorTrack::midiBank() >= 0) {
			pDocument->saveTextElement("midi-bank",
				QString::number(qtractorTrack::midiBank()), &eProps);
		}
		if (qtractorTrack::midiProgram() >= 0) {
			pDocument->saveTextElement("midi-program",
				QString::number(qtractorTrack::midiProgram()), &eProps);
		}
	}
	pElement->appendChild(eProps);

	// Save track state...
	QDomElement eState = pDocument->document()->createElement("state");
	pDocument->saveTextElement("mute",
		pDocument->textFromBool(qtractorTrack::isMute()), &eState);
	pDocument->saveTextElement("solo",
		pDocument->textFromBool(qtractorTrack::isSolo()), &eState);
	pDocument->saveTextElement("record",
		pDocument->textFromBool(qtractorTrack::isRecord()), &eState);
	pDocument->saveTextElement("monitor",
		pDocument->textFromBool(qtractorTrack::isMonitor()), &eState);
	pDocument->saveTextElement("gain",
		QString::number(qtractorTrack::gain()), &eState);
	pDocument->saveTextElement("panning",
		QString::number(qtractorTrack::panning()), &eState);
	pElement->appendChild(eState);

	// Save track view attributes...
	QDomElement eView = pDocument->document()->createElement("view");
	pDocument->saveTextElement("height",
		QString::number(qtractorTrack::height()), &eView);
	pDocument->saveTextElement("background-color",
		qtractorTrack::background().name(), &eView);
	pDocument->saveTextElement("foreground-color",
		qtractorTrack::foreground().name(), &eView);
	pElement->appendChild(eView);

	// Clips are not saved when in template mode...
	if (!pDocument->isTemplate()) {
		// Save track clips...
		QDomElement eClips = pDocument->document()->createElement("clips");
		for (qtractorClip *pClip = qtractorTrack::clips().first();
				pClip; pClip = pClip->next()) {
			// Create the new clip element...
			QDomElement eClip = pDocument->document()->createElement("clip");
			if (!pClip->saveElement(pDocument, &eClip))
				return false;
			// Add this clip...
			eClips.appendChild(eClip);
		}
		pElement->appendChild(eClips);
	}

	// Save track plugins...
	QDomElement ePlugins = pDocument->document()->createElement("plugins");
	m_pPluginList->saveElement(pDocument, &ePlugins);
	pElement->appendChild(ePlugins);

	return true;
}


// end of qtractorTrack.h
