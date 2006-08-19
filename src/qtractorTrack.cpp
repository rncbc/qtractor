// qtractorTrack.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorTrack.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"

#include "qtractorSessionDocument.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorAudioMonitor.h"
#include "qtractorMidiMonitor.h"
#include "qtractorInstrument.h"
#include "qtractorPlugin.h"

#include <qpainter.h>


//-------------------------------------------------------------------------
// qtractorTrack::Properties -- Track properties structure.

// Default constructor.
qtractorTrack::Properties::Properties (void)
{
	trackName   = QString::null;
	trackType   = None;
	record      = false;
	mute        = false;
	solo        = false;
	gain        = 1.0f;
	panning     = 0.0f;
	inputBusName  = QString::null;
	outputBusName = QString::null;
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
	trackName   = props.trackName;
	trackType   = props.trackType;
	record      = props.record;
	mute        = props.mute;
	solo        = props.solo;
	gain        = props.gain;
	panning     = props.panning;
	inputBusName  = props.inputBusName;
	outputBusName = props.outputBusName;
	midiChannel = props.midiChannel;
	midiBankSelMethod = props.midiBankSelMethod;
	midiBank    = props.midiBank;
	midiProgram = props.midiProgram;
	foreground  = props.foreground;
	background  = props.background;

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

	m_pMonitor = NULL;

	m_iMidiTag = 0;
	m_iHeight  = 48;

	m_clips.setAutoDelete(true);

	m_pClipRecord = NULL;

	m_pPluginList = new qtractorPluginList(0, 0, 0);

	clear();
}

// Default constructor.
qtractorTrack::~qtractorTrack (void)
{
	close();

	if (m_pPluginList)
		delete m_pPluginList;
	if (m_pMonitor)
		delete m_pMonitor;

	clear();
}


// Reset track.
void qtractorTrack::clear (void)
{
	setClipRecord(NULL);

	m_clips.clear();

	m_props.midiBankSelMethod = -1;
	m_props.midiBank          = -1;
	m_props.midiProgram       = -1;

	m_props.record  = false;
	m_props.mute    = false;
	m_props.solo    = false;
	m_props.gain    = 1.0f;
	m_props.panning = 0.0f;
}


// Track open method.
bool qtractorTrack::open (void)
{
//	close();

	if (m_pSession == NULL)
		return false;

	// Depending on track type...
	qtractorEngine *pEngine = NULL;
	switch (trackType()) {
	case qtractorTrack::Audio:
		pEngine = m_pSession->audioEngine();
		break;
	case qtractorTrack::Midi:
		pEngine = m_pSession->midiEngine();
		break;
	default:
		break;
	}

	// Got it?
	if (pEngine == NULL)
		return false;

	// (Re)assign the input bus to the track.
	m_pInputBus = pEngine->findBus(inputBusName());
	// Fallback to first usable one...
	if (m_pInputBus == NULL) {
		for (qtractorBus *pBus = pEngine->busses().first();
				pBus; pBus->next()) {
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
	m_pOutputBus = pEngine->findBus(outputBusName());
	// Fallback to first usable one...
	if (m_pOutputBus == NULL) {
		for (qtractorBus *pBus = pEngine->busses().first();
				pBus; pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				m_pOutputBus = pBus;
				break;
			}
		}
		// Set the bus name back...
		if (m_pOutputBus)
			setOutputBusName(m_pOutputBus->busName());
	}

	// Check proper bus assignment...
	if (m_pInputBus == NULL || m_pOutputBus == NULL)
		return false;

	// (Re)allocate (output) monitor...
	if (m_pMonitor) {
		delete m_pMonitor;
		m_pMonitor = NULL;
	}
	switch (trackType()) {
	case qtractorTrack::Audio: {
		qtractorAudioEngine *pAudioEngine
			= static_cast<qtractorAudioEngine *> (pEngine);
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (m_pOutputBus);
		if (pAudioBus) {
			m_pMonitor = new qtractorAudioMonitor(pAudioBus->channels(),
				m_props.gain, m_props.panning);
			m_pPluginList->setBuffer(pAudioBus->channels(),
				pAudioEngine->bufferSize(), pAudioEngine->sampleRate());
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (m_pOutputBus);
		if (pMidiBus) {
			m_pMonitor = new qtractorMidiMonitor(m_pSession,
				m_props.gain, m_props.panning);
			m_pPluginList->setBuffer(0, 0, 0);
		}
		break;
	}
	default:
		break;
	}

	// Ah, at least make new name feedback...
	m_pPluginList->setName(trackName());

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

	// Acquire a new midi-tag...
	if (m_props.trackType == qtractorTrack::Midi)
		m_pSession->acquireMidiTag(this);
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
	if ((m_props.mute && !bMute) || (!m_props.mute && bMute))
		m_pSession->setMuteTracks(bMute);

	m_props.mute = bMute;

	if (m_pSession->isPlaying())
		m_pSession->trackMute(this, bMute);
}


// Solo status accessors.
bool qtractorTrack::isSolo (void) const
{
	return m_props.solo;
}

void qtractorTrack::setSolo ( bool bSolo )
{
	if ((m_props.solo && !bSolo) || (!m_props.solo && bSolo))
		m_pSession->setSoloTracks(bSolo);

	m_props.solo = bSolo;

	if (m_pSession->isPlaying())
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
	m_iMidiTag = iMidiTag;
}

unsigned short qtractorTrack::midiTag (void) const
{
	return m_iMidiTag;
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
unsigned short qtractorTrack::height (void) const
{
	return m_iHeight;
}

void qtractorTrack::setHeight ( unsigned short iHeight )
{
	m_iHeight = iHeight;
}


// Clip list management methods.
qtractorList<qtractorClip>& qtractorTrack::clips (void)
{
	return m_clips;
}


// Insert a new clip in garanteed sorted fashion.
void qtractorTrack::addClip ( qtractorClip *pClip )
{
	pClip->setTrack(this);
	pClip->updateClipTime();

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
//	pClip->setTrack(NULL);
	m_clips.unlink(pClip);
}

void qtractorTrack::removeClip ( qtractorClip *pClip )
{
//	pClip->setTrack(NULL);
	m_clips.remove(pClip);
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
	qtractorAudioBus *pAudioBus = NULL;
	qtractorAudioMonitor *pAudioMonitor = NULL;
	if (m_props.trackType == qtractorTrack::Audio) {
		pAudioBus = static_cast<qtractorAudioBus *> (m_pOutputBus);
		pAudioMonitor = static_cast<qtractorAudioMonitor *> (m_pMonitor);
		if (pAudioBus == NULL || pAudioMonitor == NULL)
			return;
		pAudioBus->buffer_prepare(nframes);
	}

	// Now, for every clip...
	while (pClip && pClip->clipStart() < iFrameEnd) {
		pClip->process(iFrameStart, iFrameEnd);
		pClip = pClip->next();
	}

	// Audio buffers needs monitoring and commitment...
	if (pAudioMonitor) {
		// Commit output if it ain't muted, of course...
		if (pAudioBus
			&& !isMute() && (!m_pSession->soloTracks() || isSolo())) {
			pAudioMonitor->process(pAudioBus->buffer(), nframes);
			// Plugin chain post-processing...
			if (m_pPluginList->activated())
				m_pPluginList->process(pAudioBus->buffer(), nframes);
			// Done, send it out...
			pAudioBus->buffer_commit(nframes);
		}
		// Audio-recording?
		if (isRecord()) {
			// Input pre-monitoring...
			pAudioBus = static_cast<qtractorAudioBus *> (m_pInputBus);
			if (pAudioBus) {
				pAudioMonitor->process(
					pAudioBus->in(), nframes, pAudioBus->channels());
				// Effective audio-recording?
				qtractorAudioClip *pAudioClip
					= static_cast<qtractorAudioClip *> (m_pClipRecord);
				if (pAudioClip) {
					pAudioClip->write(
						pAudioBus->in(), nframes, pAudioBus->channels());
				}
			}
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
				x += session()->pixelFromFrame(iClipStart - iTrackStart);
			} else {
				iClipOffset = iTrackStart - iClipStart;
				x--;	// Give some clip left-border room.
			}
			if (iClipEnd < iTrackEnd) {
				w -= session()->pixelFromFrame(iTrackEnd - iClipEnd) + 1;
			} else {
				w++;	// Give some clip right-border room.
			}
			const QRect clipRect(x, y, w - x, h);
			pClip->drawClip(pPainter, clipRect, iClipOffset);
			if (pClip->isClipSelected()) {
				Qt::RasterOp rop = pPainter->rasterOp();
				pPainter->setRasterOp(Qt::NotROP);
				pPainter->fillRect(clipRect, Qt::gray);
				pPainter->setRasterOp(rop);
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
		iBankSelMethod, midiBank(), midiProgram());
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
					qtractorTrack::setHeight(eView.text().toUShort());
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
		if (eChild.tagName() == "clips") {
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

	// Save track plugins...
	QDomElement ePlugins = pDocument->document()->createElement("plugins");
	m_pPluginList->saveElement(pDocument, &ePlugins);
	pElement->appendChild(ePlugins);

	return true;
}


// end of qtractorTrack.h
