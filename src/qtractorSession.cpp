// qtractorSession.cpp
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
#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorAudioEngine.h"
#include "qtractorAudioPeak.h"
#include "qtractorAudioClip.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiManager.h"

#include "qtractorPlugin.h"
#include "qtractorCurve.h"

#include "qtractorInstrument.h"
#include "qtractorCommand.h"

#include "qtractorFileList.h"
#include "qtractorFiles.h"

#include "qtractorMainForm.h"

#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

#include <QDomDocument>

#include <QElapsedTimer>

#include <stdlib.h>


//-------------------------------------------------------------------------
// qtractorSession::Properties -- Session properties structure.

// Helper clear/reset method.
void qtractorSession::Properties::clear (void)
{
	sessionDir = QDir().absolutePath();

	sessionName.clear();
	description.clear();
	timeScale.clear();
}

// Helper copy method.
qtractorSession::Properties& qtractorSession::Properties::copy (
	const Properties& props )
{
	if (&props != this) {
		sessionDir  = props.sessionDir;
		sessionName = props.sessionName;
		description = props.description;
		timeScale   = props.timeScale;
	}
	return *this;
}


//-------------------------------------------------------------------------
// qtractorSession -- Session container.

// pSeudo-singleton instance pointer.
qtractorSession *qtractorSession::g_pSession = nullptr;


// Pseudo-singleton instance accessor (static).
qtractorSession *qtractorSession::getInstance (void)
{
	return g_pSession;
}


// Constructor.
qtractorSession::qtractorSession (void)
{
	g_pSession = this;

	m_tracks.setAutoDelete(true);
	m_cursors.setAutoDelete(false);

	m_midiManagers.setAutoDelete(false);

	// Initial comon client name.
	m_sClientName = PROJECT_TITLE;

	// Singleton ownings.
	m_pFiles       = new qtractorFileList();
	m_pCommands    = new qtractorCommandList();
	m_pInstruments = new qtractorInstrumentList();

	// The dubious permanency of the crucial device engines.
	m_pMidiEngine       = new qtractorMidiEngine(this);
	m_pAudioEngine      = new qtractorAudioEngine(this);
	m_pAudioPeakFactory = new qtractorAudioPeakFactory();

	m_bAutoTimeStretch  = false;

	m_bAutoDeactivate   = false;

	m_iLoopRecordingMode = 0;

	clear();
}


// Default destructor.
qtractorSession::~qtractorSession (void)
{
	close();
	clear();

	delete m_pAudioPeakFactory;
	delete m_pAudioEngine;
	delete m_pMidiEngine;

	delete m_pInstruments;
	delete m_pCommands;

	delete m_pFiles;

	g_pSession = nullptr;
}


// Initialize session engine(s).
bool qtractorSession::init (void)
{
	// Lock it up...
	lock();

	//  Actually init session device engines...
	const bool bResult = (m_pAudioEngine->init() && m_pMidiEngine->init());

	// Done.
	unlock();

	// HACK: Make sure we'll wait some time (~200 msec)
	// for the JACK server start up and stabilizing...
	if (bResult) {
		setSampleRate(m_pAudioEngine->sampleRate());
		stabilize();
	}

	return bResult;
}


// Open session engine(s).
bool qtractorSession::open (void)
{
	// Lock it up...
	lock();

	// A default MIDI master bus is always in order...
	const QString sMaster("Master");
	if (m_pMidiEngine->buses().count() == 0) {
		qtractorMidiBus *pMidiMasterBus
			= new qtractorMidiBus(m_pMidiEngine, sMaster, qtractorBus::Duplex);
		m_pMidiEngine->addBus(pMidiMasterBus);
	}

	// Get over the stereo playback default master bus...
	if (m_pAudioEngine->buses().count() == 0) {
		qtractorAudioBus *pAudioMasterBus
			= new qtractorAudioBus(m_pAudioEngine, sMaster, qtractorBus::Duplex);
		pAudioMasterBus->setAutoConnect(m_pAudioEngine->isMasterAutoConnect());
		m_pAudioEngine->addBus(pAudioMasterBus);
	}

	//  Actually open session device engines...
	if (!m_pAudioEngine->open() || !m_pMidiEngine->open()) {
		unlock();
		close();
		return false;
	}

	// Open all tracks (assign buses)...
	qtractorTrack *pTrack = m_tracks.first();
	while (pTrack) {
		if (!pTrack->open()) {
			unlock();
			close();
			return false;
		}
		pTrack = pTrack->next();
	}

	// Done.
	unlock();

	return true;
}


// Close session engine(s).
void qtractorSession::close (void)
{
	// Lock it up...
	lock();

	m_pAudioEngine->close();
	m_pMidiEngine->close();

	// Close all tracks (unassign buses)...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		pTrack->close();
	}

	unlock();

	m_pFiles->cleanup(true);

//	clear();
}


// Reset session.
void qtractorSession::clear (void)
{
	ATOMIC_SET(&m_locks, 0);
	ATOMIC_SET(&m_mutex, 0);

	m_pAudioPeakFactory->sync();

	m_pCurrentTrack = nullptr;

	m_pCommands->clear();

	m_tracks.clear();
	m_cursors.clear();

	m_props.clear();

	m_midiTags.clear();
//	m_midiManagers.clear();

	m_pMidiEngine->clear();
	m_pAudioEngine->clear();

	m_pFiles->clear();

	m_filePaths.clear();
	m_trackNames.clear();

	qtractorAudioClip::clearHashTable();
	qtractorMidiClip::clearHashTable();

	m_iSessionStart  = 0;
	m_iSessionEnd    = 0;

	m_iRecordTracks  = 0;
	m_iMuteTracks    = 0;
	m_iSoloTracks    = 0;

	m_iAudioRecord   = 0;
	m_iMidiRecord    = 0;

	m_iMidiTag       = 0;

	m_iEditHead      = 0;
	m_iEditTail      = 0;
	m_iEditHeadTime  = 0;
	m_iEditTailTime  = 0;

	m_iLoopStart     = 0;
	m_iLoopEnd       = 0;
	m_iLoopStartTime = 0;
	m_iLoopEndTime   = 0;

	m_iPunchIn       = 0;
	m_iPunchOut      = 0;
	m_iPunchInTime   = 0;
	m_iPunchOutTime  = 0;

	m_iPlayHeadAutoBackward = 0;

	m_bRecording = false;

	updateTimeScale();

	qtractorSessionCursor *pAudioCursor = m_pAudioEngine->sessionCursor();
	if (pAudioCursor) {
		pAudioCursor->resetClips();
		pAudioCursor->reset();
		pAudioCursor->seek(0);
		m_cursors.append(pAudioCursor);
	}

	qtractorSessionCursor *pMidiCursor = m_pMidiEngine->sessionCursor();
	if (pMidiCursor) {
		pMidiCursor->resetClips();
		pMidiCursor->reset();
		pMidiCursor->seek(0);
		m_cursors.append(pMidiCursor);
	}

	m_pAudioPeakFactory->cleanup();

	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl)
		pMidiControl->clearControlMap();
}


// The global undoable command execuive.
bool qtractorSession::execute ( qtractorCommand *pCommand )
{
	return m_pCommands->exec(pCommand);
}


// The global undoable command list reference.
qtractorCommandList *qtractorSession::commands (void) const
{
	return m_pCommands;
}


// Session instruments repository.
qtractorInstrumentList *qtractorSession::instruments (void) const
{
	return m_pInstruments;
}


// Session directory path accessors.
void qtractorSession::setSessionDir ( const QString& sSessionDir )
{
	const QDir sdir(sSessionDir);
	if (sdir.exists()) {
		m_props.sessionDir = sdir.absolutePath();
		if (!QFileInfo(m_props.sessionDir).isWritable())
			m_props.sessionDir = QDir::homePath();
	}
}

const QString& qtractorSession::sessionDir (void) const
{
	return m_props.sessionDir;
}


// Session filename accessors.
void qtractorSession::setSessionName ( const QString& sSessionName )
{
	m_props.sessionName = sSessionName;
}

const QString& qtractorSession::sessionName (void) const
{
	return m_props.sessionName;
}


// Session description accessors.
void qtractorSession::setDescription ( const QString& sDescription )
{
	m_props.description = sDescription;
}

const QString& qtractorSession::description (void) const
{
	return m_props.description;
}


// Adjust session length to the latest and/or longer clip.
void qtractorSession::updateSession (
	unsigned long iSessionStart, unsigned long iSessionEnd )
{
	// Maybe we just don't need to know more...
	// (recording ongoing?)
	if (iSessionEnd > 0) {
		if (m_iSessionEnd < iSessionEnd)
			m_iSessionEnd = iSessionEnd;
		// Enough!
		return;
	}

	// Set initial one...
	m_iSessionStart = iSessionStart;
	m_iSessionEnd   = iSessionEnd;

	// Find the last and longest clip frame position...
	int i = 0;
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			const unsigned long iClipStart = pClip->clipStart();
			const unsigned long iClipEnd   = iClipStart + pClip->clipLength();
			if (m_iSessionStart > iClipStart || i == 0)
				m_iSessionStart = iClipStart;
			if (m_iSessionEnd < iClipEnd)
				m_iSessionEnd = iClipEnd;
			++i;
		}
	}

	// Account for the last marker
	qtractorTimeScale::Marker *pMarker
		= m_props.timeScale.markers().last();
	if (pMarker &&
		m_iSessionEnd < pMarker->frame)
		m_iSessionEnd = pMarker->frame;
}


// Session start/end accessors.
unsigned long qtractorSession::sessionStart (void) const
{
	return m_iSessionStart;
}


unsigned long qtractorSession::sessionEnd (void) const
{
	return m_iSessionEnd;
}


// Time-scale helper accessors.
qtractorTimeScale *qtractorSession::timeScale (void)
{
	return &(m_props.timeScale);
}


// Device engine common client name accessors.
void qtractorSession::setClientName ( const QString& sClientName )
{
	m_sClientName = sClientName;
}

const QString& qtractorSession::clientName (void) const
{
	return m_sClientName;
}


// Sample rate accessors.
void qtractorSession::setSampleRate ( unsigned int iSampleRate )
{
	m_props.timeScale.setSampleRate(iSampleRate);
}

unsigned int qtractorSession::sampleRate (void) const
{
	return m_props.timeScale.sampleRate();
}


// Session tempo accessors.
void qtractorSession::setTempo ( float fTempo )
{
	m_props.timeScale.setTempo(fTempo);
	m_props.timeScale.updateScale();
}

float qtractorSession::tempo (void) const
{
	return m_props.timeScale.tempo();
}


// Tempo beat type accessors.
void qtractorSession::setBeatType ( unsigned short iBeatType )
{
	m_props.timeScale.setBeatType(iBeatType);
}


unsigned short qtractorSession::beatType (void) const
{
	return m_props.timeScale.beatType();
}


// Resolution accessors.
void qtractorSession::setTicksPerBeat ( unsigned short iTicksPerBeat )
{
	if (iTicksPerBeat < qtractorTimeScale::TICKS_PER_BEAT_MIN)
		iTicksPerBeat = qtractorTimeScale::TICKS_PER_BEAT_MIN;
	else
	if (iTicksPerBeat > qtractorTimeScale::TICKS_PER_BEAT_MAX)
		iTicksPerBeat = qtractorTimeScale::TICKS_PER_BEAT_MAX;

	m_props.timeScale.setTicksPerBeat(iTicksPerBeat);
}

unsigned short qtractorSession::ticksPerBeat (void) const
{
	return m_props.timeScale.ticksPerBeat();
}


// Beats/Bar(measure) accessors.
void qtractorSession::setBeatsPerBar ( unsigned short iBeatsPerBar )
{
	m_props.timeScale.setBeatsPerBar(iBeatsPerBar);
}


unsigned short qtractorSession::beatsPerBar (void) const
{
	return m_props.timeScale.beatsPerBar();
}


// Time signature (denominator) accessors.
void qtractorSession::setBeatDivisor ( unsigned short iBeatDivisor )
{
	m_props.timeScale.setBeatDivisor(iBeatDivisor);
}


unsigned short qtractorSession::beatDivisor (void) const
{
	return m_props.timeScale.beatDivisor();
}


// Pixels per beat (width).
void qtractorSession::setPixelsPerBeat ( unsigned short iPixelsPerBeat )
{
	m_props.timeScale.setPixelsPerBeat(iPixelsPerBeat);
}

unsigned short qtractorSession::pixelsPerBeat (void) const
{
	return m_props.timeScale.pixelsPerBeat();
}


// Horizontal zoom factor.
void qtractorSession::setHorizontalZoom ( unsigned short iHorizontalZoom )
{
	m_props.timeScale.setHorizontalZoom(iHorizontalZoom);
}

unsigned short qtractorSession::horizontalZoom (void) const
{
	return m_props.timeScale.horizontalZoom();
}


// Vertical zoom factor.
void qtractorSession::setVerticalZoom ( unsigned short iVerticalZoom )
{
	m_props.timeScale.setVerticalZoom(iVerticalZoom);
}

unsigned short qtractorSession::verticalZoom (void) const
{
	return m_props.timeScale.verticalZoom();
}


// Beat divisor (snap) accessors.
void qtractorSession::setSnapPerBeat ( unsigned short iSnapPerBeat )
{
	m_props.timeScale.setSnapPerBeat(iSnapPerBeat);
}

unsigned short qtractorSession::snapPerBeat (void) const
{
	return m_props.timeScale.snapPerBeat();
}


// Edit-head frame accessors.
void qtractorSession::setEditHead ( unsigned long iEditHead )
{
	m_iEditHead     = iEditHead;
	m_iEditHeadTime = tickFromFrame(iEditHead);
}

unsigned long qtractorSession::editHead (void) const
{
	return m_iEditHead;
}


void qtractorSession::setEditTail ( unsigned long iEditTail )
{
	m_iEditTail     = iEditTail;
	m_iEditTailTime = tickFromFrame(iEditTail);
}

unsigned long qtractorSession::editTail (void) const
{
	return m_iEditTail;
}


// Pixel/Tick number conversion.
unsigned long qtractorSession::tickFromPixel ( unsigned int x )
{
	return m_props.timeScale.tickFromPixel(x);
}

unsigned int qtractorSession::pixelFromTick ( unsigned long iTick )
{
	return m_props.timeScale.pixelFromTick(iTick);
}


// Pixel/Frame number conversion.
unsigned long qtractorSession::frameFromPixel ( unsigned int x ) const
{
	return m_props.timeScale.frameFromPixel(x);
}

unsigned int qtractorSession::pixelFromFrame ( unsigned long iFrame ) const
{
	return m_props.timeScale.pixelFromFrame(iFrame);
}


// Beat/frame conversion.
unsigned long qtractorSession::frameFromBeat ( unsigned int iBeat )
{
	return m_props.timeScale.frameFromBeat(iBeat);
}

unsigned int qtractorSession::beatFromFrame ( unsigned long iFrame )
{
	return m_props.timeScale.beatFromFrame(iFrame);
}


// Tick/Frame number conversion.
unsigned long qtractorSession::frameFromTick ( unsigned long iTick )
{
	return m_props.timeScale.frameFromTick(iTick);
}

unsigned long qtractorSession::tickFromFrame ( unsigned long iFrame )
{
	return m_props.timeScale.tickFromFrame(iFrame);
}


// Tick/Frame range conversion (delta conversion).
unsigned long qtractorSession::frameFromTickRange (
	unsigned long iTickStart, unsigned long iTickEnd, bool bOffset )
{
	return m_props.timeScale.frameFromTickRange(iTickStart, iTickEnd, bOffset);
}

unsigned long qtractorSession::tickFromFrameRange (
	unsigned long iFrameStart, unsigned long iFrameEnd, bool bOffset )
{
	return m_props.timeScale.tickFromFrameRange(iFrameStart, iFrameEnd, bOffset);
}


// Beat/frame snap filters.
unsigned long qtractorSession::tickSnap ( unsigned long iTick )
{
	return m_props.timeScale.tickSnap(iTick);
}

unsigned long qtractorSession::frameSnap ( unsigned long iFrame )
{
	return m_props.timeScale.frameSnap(iFrame);
}

unsigned int qtractorSession::pixelSnap ( unsigned int x )
{
	return m_props.timeScale.pixelSnap(x);
}


// Frame/locate (SMPTE) conversion.
unsigned long qtractorSession::frameFromLocate ( unsigned long iLocate ) const
{
	return (iLocate * m_props.timeScale.sampleRate()) / 30;
}

unsigned long qtractorSession::locateFromFrame ( unsigned long iFrame ) const
{
	return (30 * iFrame) / m_props.timeScale.sampleRate();
}



// Song position pointer (SPP=MIDI beats) to frame converters.
unsigned long qtractorSession::frameFromSongPos ( unsigned short iSongPos )
{
	return frameFromTick((iSongPos * ticksPerBeat()) >> 2);
}

unsigned short qtractorSession::songPosFromFrame ( unsigned long iFrame )
{
	return ((tickFromFrame(iFrame) << 2) / ticksPerBeat());
}


// Update scale divisor factors.
void qtractorSession::updateTimeScale (void)
{
	// Recompute scale divisor factors...
	m_props.timeScale.updateScale();

	// Just (re)synchronize all clips to new tempo state, if any;
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			pClip->updateClipTime();
		}
	}

	// Update loop points...
	if (m_iLoopStart < m_iLoopEnd) {
		m_iLoopStart = frameFromTick(m_iLoopStartTime);
		m_iLoopEnd   = frameFromTick(m_iLoopEndTime);
		// Set proper loop points for every track, clip and buffer...
		qtractorTrack *pTrack = m_tracks.first();
		while (pTrack) {
			pTrack->setLoop(m_iLoopStart, m_iLoopEnd);
			pTrack = pTrack->next();
		}
	}

	// Update punch points...
	if (m_iPunchIn < m_iPunchOut) {
		m_iPunchIn  = frameFromTick(m_iPunchInTime);
		m_iPunchOut = frameFromTick(m_iPunchOutTime);
	}

	// Do not forget those edit points too...
	m_iEditHead = frameFromTick(m_iEditHeadTime);
	m_iEditTail = frameFromTick(m_iEditTailTime);
}


void qtractorSession::updateTimeScaleEx (void)
{
	updateTimeScale();

	if (m_pAudioEngine)
		m_pAudioEngine->resetTimebase();
}


// Update time resolution divisor factors.
void qtractorSession::updateTimeResolution (void)
{
	// Recompute scale divisor factors...
	m_props.timeScale.updateScale();

	// Gotta (re)synchronize all MIDI clips to new resolution...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			if (pTrack->trackType() == qtractorTrack::Midi)
				pClip->close();
			pClip->setClipStart(pClip->clipStart());
			pClip->setClipOffset(pClip->clipOffset());
			pClip->setClipLength(pClip->clipLength());
			if (pTrack->trackType() == qtractorTrack::Midi)
				pClip->open();
		}
	}

	// Update loop points...
	if (m_iLoopStart < m_iLoopEnd) {
		m_iLoopStartTime = tickFromFrame(m_iLoopStart);
		m_iLoopEndTime   = tickFromFrame(m_iLoopEnd);
	}

	// Update punch points...
	if (m_iPunchIn < m_iPunchOut) {
		m_iPunchInTime  = tickFromFrame(m_iPunchIn);
		m_iPunchOutTime = tickFromFrame(m_iPunchOut);
	}

	// Do not forget those edit points too...
	m_iEditHeadTime = tickFromFrame(m_iEditHead);
	m_iEditTailTime = tickFromFrame(m_iEditTail);
}


// Update from disparate sample-rate.
void qtractorSession::updateSampleRate ( unsigned int iSampleRate )
{
	if (iSampleRate == m_props.timeScale.sampleRate())
		return;
#if 0
	// Unfortunatelly we must close all clips first,
	// so let at least all audio peaks be refreshned...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			pClip->close();
		}
	}
#endif
	// Set the conversion ratio...
	const float fRatio = float(iSampleRate)
		/ float(m_props.timeScale.sampleRate());

	// Set actual sample-rate...
	m_props.timeScale.setSampleRate(iSampleRate);

	// Give it some room for just that...
	stabilize();
	updateTimeScale();
	stabilize();

	// Adjust all tracks and clips (reopening all those...)
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		// Update automation stuff...
		qtractorCurveList *pCurveList = pTrack->curveList();
		if (pCurveList) {
			for (qtractorCurve *pCurve = pCurveList->first();
					pCurve; pCurve = pCurve->next()) {
				for (qtractorCurve::Node *pNode = pCurve->nodes().first();
						pNode; pNode = pNode->next()) {
					pNode->frame = qtractorTimeScale::uroundf(
						fRatio * float(pNode->frame));
				}
				pCurve->update();
			}
		}
		// Update regular clip stuff...
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
		//	pClip->setClipStart(qtractorTimeScale::uroundf(
		//		fRatio * float(pClip->clipStart())));
		#if 1// FIXUP: Don't quantize to MIDI metronomic time-scale...
			pClip->setClipOffset(qtractorTimeScale::uroundf(
				fRatio * float(pClip->clipOffset())));
		#endif
		//	pClip->setClipLength(qtractorTimeScale::uroundf(
		//		fRatio * float(pClip->clipLength())));
			pClip->setFadeInLength(qtractorTimeScale::uroundf(
				fRatio * float(pClip->fadeInLength())));
			pClip->setFadeOutLength(qtractorTimeScale::uroundf(
				fRatio * float(pClip->fadeOutLength())));
			pClip->open();
		}
	}
}



// Alternate properties accessor.
qtractorSession::Properties& qtractorSession::properties (void)
{
	return m_props;
}


// Track list management methods.
const qtractorList<qtractorTrack>& qtractorSession::tracks (void) const
{
	return m_tracks;
}


void qtractorSession::addTrack ( qtractorTrack *pTrack )
{
	insertTrack(pTrack, m_tracks.last());
}


void qtractorSession::insertTrack ( qtractorTrack *pTrack,
	qtractorTrack *pPrevTrack )
{
//	lock();

	if (pTrack->trackType() == qtractorTrack::Midi)
		acquireMidiTag(pTrack);

	if (pPrevTrack) {
		m_tracks.insertAfter(pTrack, pPrevTrack);
	} else {
		m_tracks.prepend(pTrack);
	}

#if 0
	if (pTrack->isRecord())
		setRecordTracks(true);
	if (pTrack->isMute())
		setMuteTracks(true);
	if (pTrack->isSolo())
		setSoloTracks(true);
#endif

	// Acquire track-name for uniqueness...
	acquireTrackName(pTrack);

	// Associate track to its curve-list...
	acquireTrackCurveList(pTrack);

	qtractorSessionCursor *pSessionCursor = m_cursors.first();
	while (pSessionCursor) {
		pSessionCursor->addTrack(pTrack);
		pSessionCursor = pSessionCursor->next();
	}

	pTrack->setLoop(m_iLoopStart, m_iLoopEnd);
	pTrack->open();

//	unlock();
}


void qtractorSession::moveTrack (
	qtractorTrack *pTrack, qtractorTrack *pNextTrack )
{
//	lock();

	m_tracks.unlink(pTrack);
	if (pNextTrack)
		m_tracks.insertBefore(pTrack, pNextTrack);
	else
		m_tracks.append(pTrack);

	qtractorSessionCursor *pSessionCursor = m_cursors.first();
	while (pSessionCursor) {
		pSessionCursor->resetClips();
		pSessionCursor = pSessionCursor->next();
	}

//	unlock();
}


void qtractorSession::updateTrack ( qtractorTrack *pTrack )
{
//	lock();

	pTrack->setLoop(m_iLoopStart, m_iLoopEnd);

	qtractorSessionCursor *pSessionCursor = m_cursors.first();
	while (pSessionCursor) {
		pSessionCursor->updateTrack(pTrack);
		pSessionCursor = pSessionCursor->next();
	}

//	unlock();
}


void qtractorSession::unlinkTrack ( qtractorTrack *pTrack )
{
//	lock();

	pTrack->setLoop(0, 0);
	pTrack->close();

	qtractorSessionCursor *pSessionCursor = m_cursors.first();
	while (pSessionCursor) {
		pSessionCursor->removeTrack(pTrack);
		pSessionCursor = pSessionCursor->next();
	}

	if (pTrack->isRecord())
		setRecordTracks(false);
	if (pTrack->isMute())
		setMuteTracks(false);
	if (pTrack->isSolo())
		setSoloTracks(false);

	// Release track-name from uniqueness...
	releaseTrackName(pTrack);

	// Dessociate track from its curve-list...
	releaseTrackCurveList(pTrack);

	if (pTrack->trackType() == qtractorTrack::Midi)
		releaseMidiTag(pTrack);

	m_tracks.unlink(pTrack);

//	unlock();
}


qtractorTrack *qtractorSession::trackAt ( int iTrack ) const
{
	return m_tracks.at(iTrack);
}


// Current number of record-armed tracks.
void qtractorSession::setRecordTracks ( bool bRecord )
{
	if (bRecord) {
		++m_iRecordTracks;
	} else if (m_iRecordTracks > 0) {
		--m_iRecordTracks;
	}
}

unsigned int qtractorSession::recordTracks (void) const
{
	return m_iRecordTracks;
}


// Current number of muted tracks.
void qtractorSession::setMuteTracks ( bool bMute )
{
	if (bMute) {
		++m_iMuteTracks;
	} else if (m_iMuteTracks > 0) {
		--m_iMuteTracks;
	}
}

unsigned int qtractorSession::muteTracks (void) const
{
	return m_iMuteTracks;
}


// Current number of solo tracks.
void qtractorSession::setSoloTracks ( bool bSolo )
{
	if (bSolo) {
		++m_iSoloTracks;
	} else if (m_iSoloTracks > 0) {
		--m_iSoloTracks;
	}
}

unsigned int qtractorSession::soloTracks (void) const
{
	return m_iSoloTracks;
}


// Temporary current track accessors.
void qtractorSession::setCurrentTrack ( qtractorTrack *pTrack )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSession::setCurrentTrack(%p)", pTrack);
#endif
	qtractorTrack *pOldCurrentTrack = m_pCurrentTrack;

	m_pCurrentTrack = pTrack;

	// Notify auto-plugin-deactivate...
	if (m_pCurrentTrack != pOldCurrentTrack)
		autoDeactivatePlugins();

	// Current-track plugin editors (GUI) auto-focus...
	if (m_pCurrentTrack && m_pCurrentTrack->pluginList())
		m_pCurrentTrack->pluginList()->setEditorVisibleAll(true);
}

qtractorTrack *qtractorSession::currentTrack (void) const
{
	return m_pCurrentTrack;
}


// Temporary current track predicates.
bool qtractorSession::isTrackMonitor ( qtractorTrack *pTrack ) const
{
	return pTrack->isMonitor()
		|| pTrack == m_pCurrentTrack;
}

bool qtractorSession::isTrackMidiChannel (
	qtractorTrack *pTrack, unsigned short iChannel ) const
{
	return pTrack->isMidiOmni()
		|| pTrack->midiChannel() == iChannel
		|| pTrack == m_pCurrentTrack;
}


// Session cursor factory methods.
qtractorSessionCursor *qtractorSession::createSessionCursor (
	unsigned long iFrame, qtractorTrack::TrackType syncType )
{
	qtractorSessionCursor *pSessionCursor
		= new qtractorSessionCursor(this, iFrame, syncType);

	m_cursors.append(pSessionCursor);

	return pSessionCursor;
}

void qtractorSession::unlinkSessionCursor (
	qtractorSessionCursor *pSessionCursor )
{
	m_cursors.unlink(pSessionCursor);
}


// Reset (reactivate) all plugin chains...
void qtractorSession::resetAllPlugins (void)
{
	// All tracks...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		(pTrack->pluginList())->resetBuffers();
	}

	// All audio buses...
	for (qtractorBus *pBus = m_pAudioEngine->buses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			if (pAudioBus->pluginList_in())
				pAudioBus->pluginList_in()->resetBuffers();
			if (pAudioBus->pluginList_out())
				pAudioBus->pluginList_out()->resetBuffers();
		}
	}
}


// MIDI engine accessor.
qtractorMidiEngine *qtractorSession::midiEngine (void) const
{
	return m_pMidiEngine;
}


// Audio engine accessor.
qtractorAudioEngine *qtractorSession::audioEngine (void) const
{
	return m_pAudioEngine;
}


// Wait for application stabilization.
void qtractorSession::stabilize ( int msecs )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSession::stabilize(%d)", msecs);
#endif

	// Wait a litle bit before continue...
	QElapsedTimer timer;
	timer.start();

	while (timer.elapsed() < msecs) {
		QThread::yieldCurrentThread();
		QApplication::processEvents(/* QEventLoop::ExcludeUserInputEvents */);
	}
}


// Consolidated session engine activation status.
bool qtractorSession::isActivated (void) const
{
	return (m_pAudioEngine->isActivated() && m_pMidiEngine->isActivated());
}


// Consolidated session engine start status.
void qtractorSession::setPlaying ( bool bPlaying )
{
	// For all armed tracks...
	if (bPlaying && isRecording()) {
		// Take a snapshot on where recording
		// clips are about to start...
		const unsigned long iPlayHead  = playHead();
		unsigned long iClipStart = iPlayHead;
		if (isPunching()) {
			const unsigned long iPunchIn = punchIn();
			if (iClipStart < iPunchIn)
				iClipStart = iPunchIn;
		}
		// Of course, mark those clips alright...
		for (qtractorTrack *pTrack = m_tracks.first();
				pTrack; pTrack = pTrack->next()) {
			qtractorClip *pClipRecord = pTrack->clipRecord();
			if (pClipRecord && !pTrack->isClipRecordEx()) {
				pTrack->setClipRecordStart(iClipStart);
				pClipRecord->setClipStart(iClipStart);
				// MIDI adjust to playing queue start...
				if (pTrack->trackType() == qtractorTrack::Midi
					&& iClipStart > iPlayHead) {
					qtractorMidiClip *pMidiClip
						= static_cast<qtractorMidiClip *> (pClipRecord);
					if (pMidiClip) {
						pMidiClip->sequence()->setTimeOffset(
							tickFromFrame(iClipStart - iPlayHead));
					}
				}
			}
		}
	}

	// Have all MIDI instrument plugins be shut up
	// if start playing, otherwise do ramping down...
	if (bPlaying) {
		qtractorMidiManager *pMidiManager = m_midiManagers.first();
		while (pMidiManager) {
			pMidiManager->reset();
			pMidiManager = pMidiManager->next();
		}
	}

	// Do it.
	m_pAudioEngine->setPlaying(bPlaying);
	m_pMidiEngine->setPlaying(bPlaying);

	// notify auto-plugin-deactivate
	autoDeactivatePlugins();
}

bool qtractorSession::isPlaying() const
{
	return (m_pAudioEngine->isPlaying() && m_pMidiEngine->isPlaying());
}


// Shutdown procedure.
void qtractorSession::shutdown (void)
{
	m_pAudioEngine->setPlaying(false);
	m_pMidiEngine->setPlaying(false);

	close();
}


// (Hazardous) bi-directional locate method.
void qtractorSession::seek ( unsigned long iFrame, bool bSync )
{
	if (bSync)
		resetAllPlugins();

	m_pAudioEngine->sessionCursor()->seek(iFrame, bSync);
	m_pMidiEngine->sessionCursor()->seek(iFrame, bSync);
}


// Session RT-safe pseudo-locking primitives.
bool qtractorSession::acquire (void)
{
	// Are we in business?
	return ATOMIC_TAS(&m_mutex);
}

void qtractorSession::release (void)
{
	// We're not in business anymore.
	ATOMIC_SET(&m_mutex, 0);
}


void qtractorSession::lock (void)
{
	// Wind up as pending lock...
	if (ATOMIC_INC(&m_locks) == 1) {
		// Get lost for a while...
		while (!acquire())
			stabilize();
	}
}

void qtractorSession::unlock (void)
{
	// Unwind pending locks and force back to business...
	if (ATOMIC_DEC(&m_locks) < 1) {
		ATOMIC_SET(&m_locks, 0);
		release();
	}
}


// Re-entrancy check.
bool qtractorSession::isBusy (void) const
{
	return (ATOMIC_GET(&m_locks) > 0);
}


// Playhead positioning.
void qtractorSession::setPlayHead ( unsigned long iPlayHead )
{
	const bool bPlaying = isPlaying();
	if (bPlaying && isRecording())
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSession::setPlayHead(%lu)", iPlayHead);
#endif

	lock();
	setPlaying(false);

	if (m_pAudioEngine->transportMode() & qtractorBus::Output)
		m_pAudioEngine->transport_locate(iPlayHead);

	seek(iPlayHead, true);

	if (!bPlaying) {
		// Update time(base)/BBT info...
		m_pAudioEngine->updateTimeInfo(iPlayHead);
		// Reset step-input tracking...
		m_pMidiEngine->proxy()->notifyInpEvent(qtractorMidiEngine::InpReset);
		// Sync all track automation...
		process_curve(iPlayHead);
	}

	setPlaying(bPlaying);
	unlock();
}

void qtractorSession::setPlayHeadEx ( unsigned long iPlayHead )
{
	const bool bPlaying = isPlaying();
	if (bPlaying && isRecording())
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSession::setPlayHeadEx(%lu)", iPlayHead);
#endif

	lock();

	m_pMidiEngine->setPlaying(false);

	seek(iPlayHead, true);

	// Have all MIDI instrument plugins be shut up
	// if start playing, otherwise do ramping down...
	qtractorMidiManager *pMidiManager = m_midiManagers.first();
	while (pMidiManager) {
		pMidiManager->reset();
		pMidiManager = pMidiManager->next();
	}

	if (bPlaying) {
		// Reset all dependables...
		m_pAudioEngine->resetAllMonitors();
		// Make sure we have an actual session cursor...
		//m_pAudioEngine->resetMetro();
	} else {
		// Update time(base)/BBT info...
		m_pAudioEngine->updateTimeInfo(iPlayHead);
		// Reset step-input tracking...
		m_pMidiEngine->proxy()->notifyInpEvent(qtractorMidiEngine::InpReset);
		// Sync all track automation...
		process_curve(iPlayHead);
	}

	m_pMidiEngine->setPlaying(bPlaying);

	unlock();
}

unsigned long qtractorSession::playHead (void) const
{
	return m_pAudioEngine->sessionCursor()->frame();
}


// Auto-backward save play-head frame accessors.
void qtractorSession::setPlayHeadAutoBackward ( unsigned long iPlayHead )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSession::setPlayHeadAutoBackward(%lu)", iPlayHead);
#endif
	m_iPlayHeadAutoBackward = iPlayHead;
}

unsigned long qtractorSession::playHeadAutoBackward (void) const
{
	return m_iPlayHeadAutoBackward;
}


// Session loop points accessors.
void qtractorSession::setLoop (
	unsigned long iLoopStart, unsigned long iLoopEnd )
{
	const bool bPlaying = isPlaying();
	if (bPlaying && isRecording())
		return;

	lock();
	setPlaying(false);

	// Local prepare...
	if (iLoopStart >= iLoopEnd) {
		iLoopStart = 0;
		iLoopEnd   = 0;
	}

	// Save exact current play-head position...
	const unsigned long iFrame = playHead();

	// Set proper loop points for every track, clip and buffer...
	qtractorTrack *pTrack = m_tracks.first();
	while (pTrack) {
		pTrack->setLoop(iLoopStart, iLoopEnd);
		pTrack = pTrack->next();
	}

	// Local commit...
	m_iLoopStart = iLoopStart;
	m_iLoopEnd   = iLoopEnd;

	// Time-normalized references too...
	m_iLoopStartTime = tickFromFrame(iLoopStart);
	m_iLoopEndTime   = tickFromFrame(iLoopEnd);

	// Replace last known play-head...
	m_pAudioEngine->sessionCursor()->seek(iFrame, true);
	m_pMidiEngine->sessionCursor()->seek(iFrame, true);

	setPlaying(bPlaying);
	unlock();
}

unsigned long qtractorSession::loopStart (void) const
{
	return m_iLoopStart;
}

unsigned long qtractorSession::loopEnd (void) const
{
	return m_iLoopEnd;
}

bool qtractorSession::isLooping (void) const
{
	return (m_iLoopStart < m_iLoopEnd);
}


unsigned long qtractorSession::loopStartTime (void) const
{
	return m_iLoopStartTime;
}

unsigned long qtractorSession::loopEndTime (void) const
{
	return m_iLoopEndTime;
}


// Session punch points accessors.
void qtractorSession::setPunch (
	unsigned long iPunchIn, unsigned long iPunchOut )
{
	// Local prepare...
	if (iPunchIn >= iPunchOut) {
		iPunchIn  = 0;
		iPunchOut = 0;
	}

	// Local commit...
	m_iPunchIn  = iPunchIn;
	m_iPunchOut = iPunchOut;

	// Time-normalized references too...
	m_iPunchInTime  = tickFromFrame(iPunchIn);
	m_iPunchOutTime = tickFromFrame(iPunchOut);
}

unsigned long qtractorSession::punchIn (void) const
{
	return m_iPunchIn;
}

unsigned long qtractorSession::punchOut (void) const
{
	return m_iPunchOut;
}

bool qtractorSession::isPunching (void) const
{
	return (m_iPunchIn < m_iPunchOut);
}


unsigned long qtractorSession::punchInTime (void) const
{
	return m_iPunchInTime;
}

unsigned long qtractorSession::punchOutTime (void) const
{
	return m_iPunchOutTime;
}


unsigned long qtractorSession::frameTime (void) const
{
	return m_pAudioEngine->sessionCursor()->frameTime();
}

unsigned long qtractorSession::frameTimeEx (void) const
{
	return m_pAudioEngine->sessionCursor()->frameTimeEx();
}


// Sanitize a given name.
QString qtractorSession::sanitize ( const QString& s )
{
//	return s.simplified().replace(QRegularExpression("[\\s|\\.|\\-|/]+"), '_');
	const QChar space(' ');
	const QRegularExpression rx("[^\\w]+");
	return QString(s).replace(rx, space).simplified().replace(space, '_');
}


// Provide an unique track-name if applicable,
// append an incremental numerical suffix...
QString qtractorSession::uniqueTrackName ( const QString& sTrackName ) const
{
	QString sNewTrackName = sTrackName;
	const QString& sShortTrackName
		= qtractorTrack::shortTrackName(sTrackName);
	if (!findTrack(sShortTrackName))
		return sTrackName;

	QString sOldShortTrackName = sShortTrackName;
	QString sNewShortTrackName;
	QRegularExpression rxTrackNo("([0-9]+)$");
	QRegularExpressionMatch match = rxTrackNo.match(sOldShortTrackName);
	int iTrackNo = 0;
	if (match.hasMatch()) {
		iTrackNo = match.captured(1).toInt();
		sOldShortTrackName.remove(rxTrackNo);
	}
	else sOldShortTrackName += ' ';

	do { sNewShortTrackName = sOldShortTrackName + QString::number(++iTrackNo); }
	while (findTrack(sNewShortTrackName));

	return sNewTrackName.replace(sShortTrackName, sNewShortTrackName);
}


void qtractorSession::acquireTrackName ( qtractorTrack *pTrack )
{
	if (pTrack) m_trackNames.insert(pTrack->shortTrackName(), pTrack);
}

void qtractorSession::releaseTrackName ( qtractorTrack *pTrack )
{
	if (pTrack) m_trackNames.remove(pTrack->shortTrackName());
}


// Transient file-name registry methods as far to
// avoid duplicates across load/save/record cycles...
void qtractorSession::acquireFilePath ( const QString& sFilename )
{
	m_filePaths.append(sFilename);
}

void qtractorSession::releaseFilePath ( const QString& sFilename )
{
	m_filePaths.removeAll(sFilename);
}


// Create a brand new filename (absolute file path).
QString qtractorSession::createFilePath (
	const QString& sBaseName, const QString& sExt, bool bAcquire )
{
	QString sFilename = qtractorSession::sanitize(m_props.sessionName);
	if (!sFilename.isEmpty())
		sFilename += '-';
	sFilename += qtractorSession::sanitize(sBaseName) + "-%1." + sExt;

	// If there are any existing, similar filenames,
	// take the version suffix from the most recent...
	int iFileNo = 0;
	QDir dir(m_props.sessionDir);
	const QStringList filter(sFilename.arg('*'));
	const QStringList& files
		= dir.entryList(filter, QDir::Files, QDir::Time);
	if (!files.isEmpty()) {
		QRegularExpression rx(sFilename.arg("([\\d]+)"));
		QRegularExpressionMatchIterator iter = rx.globalMatch(files.first());
		if (iter.hasNext()) {
			QRegularExpressionMatch match = iter.next();
			while (iter.hasNext()) match = iter.next();
			iFileNo = match.captured(1).toInt();
		}
	}

	// Check whether it's not aquired as our own already,
	// otherwise increment version suffix until it is.
	if (iFileNo == 0 || !bAcquire) ++iFileNo;
	QFileInfo fi(m_props.sessionDir, sFilename.arg(iFileNo));
	if (!m_filePaths.contains(fi.absoluteFilePath())) {
		while (fi.exists() || m_filePaths.contains(fi.absoluteFilePath()))
			fi.setFile(m_props.sessionDir, sFilename.arg(++iFileNo));
		if (bAcquire)
			m_filePaths.append(fi.absoluteFilePath()); // register new name!
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorSession::createFilePath(\"%s\")",
		fi.absoluteFilePath().toUtf8().constData());
#endif

	return fi.absoluteFilePath();
}


// Session directory relative/absolute file path helpers.
QString qtractorSession::relativeFilePath ( const QString& sFilename ) const
{
	return QDir(sessionDir()).relativeFilePath(sFilename);
}


QString qtractorSession::absoluteFilePath ( const QString& sFilename ) const
{
	return QDir::cleanPath(QDir(sessionDir()).absoluteFilePath(sFilename));
}


// Consolidated session record state.
void qtractorSession::setRecording ( bool bRecording )
{
	m_bRecording = bRecording;

	// For all armed tracks...
	unsigned long iClipStart = playHead();

	if (isPunching()) {
		const unsigned long iPunchIn = punchIn();
		if (iClipStart < iPunchIn)
			iClipStart = iPunchIn;
	}

	const unsigned long iFrameTime = frameTimeEx();
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->isRecord())
			trackRecord(pTrack, bRecording, iClipStart, iFrameTime);
	}
}

bool qtractorSession::isRecording (void) const
{
	return m_bRecording;
}


// Loop-recording/take mode.
void qtractorSession::setLoopRecordingMode ( int iLoopRecordingMode )
{
	m_iLoopRecordingMode = iLoopRecordingMode;
}

int qtractorSession::loopRecordingMode (void) const
{
	return m_iLoopRecordingMode;
}


// Immediate track record-arming.
void qtractorSession::trackRecord (
	qtractorTrack *pTrack, bool bRecord,
	unsigned long iClipStart, unsigned long iFrameTime )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorSession::trackRecord(\"%s\", %d, %lu, %lu)",
		pTrack->shortTrackName().toUtf8().constData(), int(bRecord),
		iClipStart, iFrameTime);
#endif

	const qtractorTrack::TrackType trackType = pTrack->trackType();

	// Just ditch the in-record clip...
	if (!bRecord) {
		pTrack->setClipRecord(nullptr);
		// Check whether we set recording off...
		if (recordTracks() < 1)
			setRecording(false);
		// One-down current tracks in record mode.
		switch (trackType) {
		case qtractorTrack::Audio:
			--m_iAudioRecord;
			break;
		case qtractorTrack::Midi:
			--m_iMidiRecord;
			break;
		default:
			break;
		}
	#if 0
		// Re-sync as appropriate...
		if (isPlaying())
			trackMute(pTrack, false);
	#endif
		// Done.
		return;
	}

	// Are we set record exclusive?
	if (pTrack->isClipRecordEx()) {
		// One-up current tracks in record mode.
		switch (trackType) {
		case qtractorTrack::Audio:
			++m_iAudioRecord;
			break;
		case qtractorTrack::Midi:
			++m_iMidiRecord;
			break;
		default:
			break;
		}
		// Bail out.
		return;
	}

	const bool bPlaying = isPlaying();

	switch (trackType) {
	case qtractorTrack::Audio:
	{
		qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
		pAudioClip->setClipStart(iClipStart);
		pAudioClip->openAudioFile(
			createFilePath(pTrack->shortTrackName(),
				qtractorAudioFileFactory::defaultExt(), true),
			qtractorAudioFile::Write);
		pTrack->setClipRecord(pAudioClip);
		// Adjust for some input latency compensation already...
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pTrack->inputBus());
		if (pAudioBus)
			// This is currently just a workaround, because
			// we consider here only the output latency of
			// the current audio bus.
			// TODO: calculate the overall out latency somehow
			pAudioClip->setClipOffset(
				pAudioBus->latency_in()	+
				pAudioBus->latency_out());
		// One-up audio tracks in record mode.
		++m_iAudioRecord;
		break;
	}
	case qtractorTrack::Midi:
	{
		qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
		pMidiClip->setClipStart(iClipStart);
		pMidiClip->openMidiFile(
			createFilePath(pTrack->shortTrackName(), "mid", true),
			pTrack->midiChannel(),
			qtractorMidiFile::Write);
		pTrack->setClipRecord(pMidiClip);
		// MIDI adjust to playing queue start
		// iif armed while already playing ...
		if (bPlaying) {
			const unsigned long iTime = pMidiClip->clipStartTime();
			const unsigned long iTimeStart = m_pMidiEngine->timeStartEx();
			if (iTime > iTimeStart)
				pMidiClip->sequence()->setTimeOffset(iTime - iTimeStart);
		}
		// One-up MIDI tracks in record mode.
		++m_iMidiRecord;
		break;
	}
	default:
		break;
	}

	// Make sure the recording clip
	// starts on some exact location...
	pTrack->setClipRecordStart(bPlaying ? iFrameTime : iClipStart);

#if 0
	// Mute track as appropriate...
	if (bPlaying)
		trackMute(pTrack, true);
#endif
}


// Immediate track mute (engine indirection).
void qtractorSession::trackMute ( qtractorTrack *pTrack, bool bMute )
{
	// For the time being, only needed for ALSA sequencer...
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		m_pAudioEngine->trackMute(pTrack, bMute);
		break;
	case qtractorTrack::Midi:
		m_pMidiEngine->trackMute(pTrack, bMute);
		break;
	case qtractorTrack::None:
	default:
		break;
	}
}


// Immediate track solo (engine indirection).
void qtractorSession::trackSolo ( qtractorTrack *pTrack, bool bSolo )
{
	// Check if we're going to (un)mute all others,
	// due to soloing this one; if already soloing,
	// no need for anything ado...
	if ((bSolo && m_iSoloTracks > 1) || (!bSolo && m_iSoloTracks > 0)) {
		trackMute(pTrack, !bSolo);
		return;
	}

	for (qtractorTrack *pTrackMute = m_tracks.first();
			pTrackMute; pTrackMute = pTrackMute->next()) {
		// For all other track, but this one.
		if (pTrackMute == pTrack || pTrackMute->isMute())
			continue;
		// (Un)mute each other track...
		trackMute(pTrackMute, bSolo);
	}
}


// Auto plugin deactivation specifics
void qtractorSession::autoDeactivatePlugins ( bool bForce )
{
	// Enabled && not if busy (e.g loading session)
	if (m_bAutoDeactivate && !isBusy()) {
		for (qtractorTrack *pTrack = m_tracks.first();
				pTrack; pTrack = pTrack->next()) {
			pTrack->pluginList()->autoDeactivatePlugins(
				canTrackBeAutoDeactivated(pTrack), bForce);
		}
	}
}


void qtractorSession::undoAutoDeactivatePlugins (void)
{
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		pTrack->pluginList()->autoDeactivatePlugins(false);
	}
}


void qtractorSession::setAutoDeactivate ( bool bOn )
{
	m_bAutoDeactivate = bOn;

	if (bOn)
		autoDeactivatePlugins();
	else
		undoAutoDeactivatePlugins();
}


bool qtractorSession::isAutoDeactivate (void) const
{
	return m_bAutoDeactivate;
}


bool qtractorSession::canTrackBeAutoDeactivated ( qtractorTrack *pTrack ) const
{
	bool bCanBeDeactivated = false;

	// Check automation
	bool bAutomationActive = false;
	qtractorCurveList *pCurveList = pTrack->curveList();
	if (pCurveList)
		bAutomationActive = pCurveList->isProcess() || pCurveList->isCapture();

	// No Auto-plugin-deactivation when automation active
	// Note: freewheeling case is done the hard way by disabling
	// auto-deactivate as whole - see qtractorMainForm::trackExportAudio
	if (!bAutomationActive) {
		if (isPlaying()) {
			// Monitored and recording tracks cannot be deactivated
			if(!isTrackMonitor(pTrack) && !pTrack->isRecord()) {
				bool bMute = pTrack->isMute();
				if(!bMute)
					bMute = m_iSoloTracks > 0 && !pTrack->isSolo();
				// TBD: We know when clips start/end. So we 'just' need a
				// clever song pos synced call of autoPluginsDeactivate
				// For now only check if track contains clips.
				bool bHasClips = pTrack->clips().count() > 0;
				if (bMute || !bHasClips)
					bCanBeDeactivated = true;
			}
		} else {
			bCanBeDeactivated = !isTrackMonitor(pTrack);
		}
	}

	return bCanBeDeactivated;
}


// Track recording specifics.
unsigned short qtractorSession::audioRecord (void) const
{
	return m_iAudioRecord;
}

unsigned short qtractorSession::midiRecord (void) const
{
	return m_iMidiRecord;
}


// Audio peak factory accessor.
qtractorAudioPeakFactory *qtractorSession::audioPeakFactory (void) const
{
	return m_pAudioPeakFactory;
}


// MIDI track tagging specifics.
unsigned short qtractorSession::midiTag (void) const
{
	return m_iMidiTag;
}

void qtractorSession::acquireMidiTag ( qtractorTrack *pTrack )
{
	if (pTrack->midiTag() > 0)
		return;

	if (m_midiTags.isEmpty()) {
		pTrack->setMidiTag(++m_iMidiTag);
	} else {
		pTrack->setMidiTag(m_midiTags.front());
		m_midiTags.pop_front();
	}
}

void qtractorSession::releaseMidiTag ( qtractorTrack *pTrack )
{
	const unsigned short iMidiTag = pTrack->midiTag();
	if (iMidiTag > 0) {
		m_midiTags.push_back(iMidiTag);
		pTrack->setMidiTag(0);
	}
}


// MIDI session/tracks instrument/controller patching (conditional).
void qtractorSession::resetAllMidiControllers ( bool bForceImmediate )
{
	m_pMidiEngine->resetAllControllers(
		( bForceImmediate && m_pMidiEngine->isResetAllControllers()) ||
		(!bForceImmediate && m_pMidiEngine->isResetAllControllersPending()));
}


// MIDI manager list accessors.
void qtractorSession::addMidiManager ( qtractorMidiManager *pMidiManager )
{
	m_midiManagers.append(pMidiManager);
}

void qtractorSession::removeMidiManager ( qtractorMidiManager *pMidiManager )
{
	m_midiManagers.remove(pMidiManager);
}


const qtractorList<qtractorMidiManager>& qtractorSession::midiManagers (void) const
{
	return m_midiManagers;
}


// Auto time-stretching global flag (when tempo changes)
void qtractorSession::setAutoTimeStretch ( bool bAutoTimeStretch )
{
	m_bAutoTimeStretch = bAutoTimeStretch;
}

bool qtractorSession::isAutoTimeStretch (void) const
{
	return m_bAutoTimeStretch;
}


// Session special process cycle executive.
void qtractorSession::process (
	qtractorSessionCursor *pSessionCursor,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	const qtractorTrack::TrackType syncType = pSessionCursor->syncType();

	// Now, for every track...
	int iTrack = 0;
	qtractorTrack *pTrack = m_tracks.first();
	while (pTrack) {
		// Track automation processing...
		if (syncType == qtractorTrack::Audio)
			pTrack->process_curve(iFrameStart);
		// Track clip processing...
		if (syncType == pTrack->trackType()) {
			pTrack->process(pSessionCursor->clip(iTrack),
				iFrameStart, iFrameEnd);
		}
		pTrack = pTrack->next();
		++iTrack;
	}
}


// Session special process record executive (audio recording only).
void qtractorSession::process_record (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Now, for every Audio track...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Audio && pTrack->isRecord())
			pTrack->process_record(iFrameStart, iFrameEnd);
	}
}


// Session special process automation executive.
void qtractorSession::process_curve ( unsigned long iFrame )
{
	// Now, for every track...
	qtractorTrack *pTrack = m_tracks.first();
	while (pTrack) {
		pTrack->process_curve(iFrame);
		pTrack = pTrack->next();
	}
}


// Manage curve-lists to specific tracks.
void qtractorSession::acquireTrackCurveList ( qtractorTrack *pTrack )
{
	if (pTrack && pTrack->curveList())
		m_curves.insert(pTrack->curveList(), pTrack);
}

void qtractorSession::releaseTrackCurveList ( qtractorTrack *pTrack )
{
	if (pTrack && pTrack->curveList())
		m_curves.remove(pTrack->curveList());
}


// Find track of specific curve-list.
qtractorTrack *qtractorSession::findTrackCurveList (
	qtractorCurveList *pCurveList ) const
{
	return m_curves.value(pCurveList, nullptr);
}


// Find track of specific name.
qtractorTrack *qtractorSession::findTrack ( const QString& sTrackName ) const
{
	return m_trackNames.value(sTrackName, nullptr);
}


// Session files registry accessor.
qtractorFileList *qtractorSession::files (void) const
{
	return m_pFiles;
}


// Document element methods.
bool qtractorSession::loadElement (
	Document *pDocument, QDomElement *pElement )
{
	qtractorSession::clear();
	qtractorSession::lock();

	// Templates have no session name...
	const bool bTemplate = pDocument->isTemplate();
	if (!bTemplate)
		qtractorSession::setSessionName(pElement->attribute("name"));

	// Session state should be postponed...
	unsigned long iLoopStart = 0;
	unsigned long iLoopEnd   = 0;

	unsigned long iPunchIn   = 0;
	unsigned long iPunchOut  = 0;

	// Load session children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Load session properties...
		if (eChild.tagName() == "properties") {
			for (QDomNode nProp = eChild.firstChild();
					!nProp.isNull();
						nProp = nProp.nextSibling()) {
				// Convert property node to element...
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				if (eProp.tagName() == "directory")
					qtractorSession::setSessionDir(eProp.text());
				else if (eProp.tagName() == "description")
					qtractorSession::setDescription(eProp.text());
				else if (eProp.tagName() == "sample-rate" && !bTemplate)
					qtractorSession::setSampleRate(eProp.text().toUInt());
				else if (eProp.tagName() == "tempo")
					qtractorSession::setTempo(eProp.text().toFloat());
				else if (eProp.tagName() == "ticks-per-beat")
					qtractorSession::setTicksPerBeat(eProp.text().toUShort());
				else if (eProp.tagName() == "beats-per-bar")
					qtractorSession::setBeatsPerBar(eProp.text().toUShort());
				else if (eProp.tagName() == "beat-divisor")
					qtractorSession::setBeatDivisor(eProp.text().toUShort());
			}
			// We need to make this permanent, right now.
			qtractorSession::updateTimeScale();
		}
		else
		if (eChild.tagName() == "state" && !bTemplate) {
			for (QDomNode nState = eChild.firstChild();
					!nState.isNull();
						nState = nState.nextSibling()) {
				// Convert state node to element...
				QDomElement eState = nState.toElement();
				if (eState.isNull())
					continue;
				if (eState.tagName() == "loop-start")
					iLoopStart = eState.text().toULong();
				else if (eState.tagName() == "loop-end")
					iLoopEnd = eState.text().toULong();
				else if (eState.tagName() == "punch-in")
					iPunchIn = eState.text().toULong();
				else if (eState.tagName() == "punch-out")
					iPunchOut = eState.text().toULong();
			}
		}
		else
		// Load file lists...
		if (eChild.tagName() == "files" && !bTemplate) {
			for (QDomNode nList = eChild.firstChild();
					!nList.isNull();
						nList = nList.nextSibling()) {
				// Convert filelist node to element...
				QDomElement eList = nList.toElement();
				if (eList.isNull())
					continue;
				if (eList.tagName() == "audio-list") {
					qtractorAudioListView *pAudioList = nullptr;
					if (pDocument->files())
						pAudioList = pDocument->files()->audioListView();
					if (pAudioList == nullptr)
						return false;
					if (!pAudioList->loadElement(pDocument, &eList))
						return false;
				}
				else
				if (eList.tagName() == "midi-list") {
					qtractorMidiListView *pMidiList = nullptr;
					if (pDocument->files())
						pMidiList = pDocument->files()->midiListView();
					if (pMidiList == nullptr)
						return false;
					if (!pMidiList->loadElement(pDocument, &eList))
						return false;
				}
			}
			// Stabilize things a bit...
			stabilize();
		}
		else
		// Load device lists...
		if (eChild.tagName() == "devices") {
			for (QDomNode nDevice = eChild.firstChild();
					!nDevice.isNull();
						nDevice = nDevice.nextSibling()) {
				// Convert buses list node to element...
				QDomElement eDevice = nDevice.toElement();
				if (eDevice.isNull())
					continue;
				if (eDevice.tagName() == "audio-engine") {
					if (!qtractorSession::audioEngine()
							->loadElement(pDocument, &eDevice)) {
						return false;
					}
				}
				else
				if (eDevice.tagName() == "midi-engine") {
					if (!qtractorSession::midiEngine()
							->loadElement(pDocument, &eDevice)) {
						return false;
					}
				}
			}
			// Stabilize things a bit...
			stabilize();
		}
		else
		// Load tempo/time-signature map...
		if (eChild.tagName() == "tempo-map") {
			for (QDomNode nNode = eChild.firstChild();
					!nNode.isNull();
						nNode = nNode.nextSibling()) {
				// Convert tempo node to element...
				QDomElement eNode = nNode.toElement();
				if (eNode.isNull())
					continue;
				// Load tempo-map...
				if (eNode.tagName() == "tempo-node") {
					const unsigned long iFrame
						= eNode.attribute("frame").toULong();
					float fTempo = 120.0f;
					unsigned short iBeatType = 2;
					unsigned short iBeatsPerBar = 4;
					unsigned short iBeatDivisor = 2;
					for (QDomNode nItem = eNode.firstChild();
							!nItem.isNull();
								nItem = nItem.nextSibling()) {
						// Convert node to element...
						QDomElement eItem = nItem.toElement();
						if (eItem.isNull())
							continue;
						if (eItem.tagName() == "tempo")
							fTempo = eItem.text().toFloat();
						else if (eItem.tagName() == "beat-type")
							iBeatType = eItem.text().toUShort();
						else if (eItem.tagName() == "beats-per-bar")
							iBeatsPerBar = eItem.text().toUShort();
						else if (eItem.tagName() == "beat-divisor")
							iBeatDivisor = eItem.text().toUShort();
					}
					// Add new node to tempo/time-signature map...
					qtractorSession::timeScale()->addNode(iFrame,
						fTempo, iBeatType, iBeatsPerBar, iBeatDivisor);
				}
			}
			// Again, make view/time scaling factors permanent.
			qtractorSession::updateTimeScale();
		}
		else
		// Load location markers...
		if (eChild.tagName() == "markers") {
			for (QDomNode nMarker = eChild.firstChild();
					!nMarker.isNull();
						nMarker = nMarker.nextSibling()) {
				// Convert tempo node to element...
				QDomElement eMarker = nMarker.toElement();
				if (eMarker.isNull())
					continue;
				// Load markers/key-signatures...
				if (eMarker.tagName() == "marker") {
					const unsigned long iFrame
						= eMarker.attribute("frame").toULong();
					QString sText;
					QColor rgbColor = Qt::darkGray;
					int iAccidentals = qtractorTimeScale::MinAccidentals;
					int iMode = -1;
					for (QDomNode nItem = eMarker.firstChild();
							!nItem.isNull();
								nItem = nItem.nextSibling()) {
						// Convert node to element...
						QDomElement eItem = nItem.toElement();
						if (eItem.isNull())
							continue;
						if (eItem.tagName() == "text")
							sText = eItem.text();
						else if (eItem.tagName() == "color") {
						#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
							rgbColor = QColor::fromString(eItem.text());
						#else
							rgbColor = QColor(eItem.text());
						#endif
						}
						else if (eItem.tagName() == "accidentals")
							iAccidentals = eItem.text().toInt();
						else if (eItem.tagName() == "mode")
							iMode = eItem.text().toInt();
					}
					// Add new marker...
					if (!sText.isEmpty()) {
						qtractorSession::timeScale()->addMarker(
							iFrame, sText, rgbColor);
					}
					// Or/and key-signature...
					if (qtractorTimeScale::isKeySignature(iAccidentals, iMode)) {
						qtractorSession::timeScale()->addKeySignature(
							iFrame, iAccidentals, iMode);
					}
				}
			}
		}
		else
		// Load tracks...
		if (eChild.tagName() == "tracks") {
			for (QDomNode nTrack = eChild.firstChild();
					!nTrack.isNull();
						nTrack = nTrack.nextSibling()) {
				// Convert track node to element...
				QDomElement eTrack = nTrack.toElement();
				if (eTrack.isNull())
					continue;
				// Load track-view state...
				if (eTrack.tagName() == "view") {
					for (QDomNode nView = eTrack.firstChild();
							!nView.isNull();
								nView = nView.nextSibling()) {
						// Convert state node to element...
						QDomElement eView = nView.toElement();
						if (eView.isNull())
							continue;
						if (eView.tagName() == "pixels-per-beat")
							qtractorSession::setPixelsPerBeat(eView.text().toUShort());
						else if (eView.tagName() == "horizontal-zoom")
							qtractorSession::setHorizontalZoom(eView.text().toUShort());
						else if (eView.tagName() == "vertical-zoom")
							qtractorSession::setVerticalZoom(eView.text().toUShort());
						else if (eView.tagName() == "snap-per-beat")
							qtractorSession::setSnapPerBeat(eView.text().toUShort());
						else if (eView.tagName() == "edit-head" && !bTemplate)
							qtractorSession::setEditHead(eView.text().toULong());
						else if (eView.tagName() == "edit-tail" && !bTemplate)
							qtractorSession::setEditTail(eView.text().toULong());
					}
					// Again, make view/time scaling factors permanent.
					qtractorSession::updateTimeScale();
				}
				else
				// Load track...
				if (eTrack.tagName() == "track") {
					qtractorTrack *pTrack = new qtractorTrack(this);
					if (!pTrack->loadElement(pDocument, &eTrack))
						return false;
					qtractorSession::addTrack(pTrack);
				}
			}
			// Stabilize things a bit...
			stabilize();
		}
	}

	// Just stabilize things around.
	qtractorSession::updateSession();

	// Check whether some deferred state needs to be set...
	if (iLoopStart < iLoopEnd)
		qtractorSession::setLoop(iLoopStart, iLoopEnd);
	if (iPunchIn < iPunchOut)
		qtractorSession::setPunch(iPunchIn, iPunchOut);

	qtractorSession::unlock();

	return true;
}


bool qtractorSession::saveElement (
	Document *pDocument, QDomElement *pElement )
{
	// Save this program version (informational)...
	pElement->setAttribute("version", PROJECT_TITLE " " PROJECT_VERSION);

	// Templates should have no session name...
	const bool bTemplate = pDocument->isTemplate();
	if (!bTemplate)
		pElement->setAttribute("name", qtractorSession::sessionName());

	// Save session properties...
	QDomElement eProps = pDocument->document()->createElement("properties");
	if (!pDocument->isArchive()) {
		pDocument->saveTextElement("directory",
			qtractorSession::sessionDir(), &eProps);
	}
	pDocument->saveTextElement("description",
		qtractorSession::description(), &eProps);
	if (!bTemplate) {
		pDocument->saveTextElement("sample-rate",
			QString::number(qtractorSession::sampleRate()), &eProps);
	}
	pDocument->saveTextElement("tempo",
		QString::number(qtractorSession::tempo()), &eProps);
	pDocument->saveTextElement("ticks-per-beat",
		QString::number(qtractorSession::ticksPerBeat()), &eProps);
	pDocument->saveTextElement("beats-per-bar",
		QString::number(qtractorSession::beatsPerBar()), &eProps);
	pDocument->saveTextElement("beat-divisor",
		QString::number(qtractorSession::beatDivisor()), &eProps);
	pElement->appendChild(eProps);

	// State and files are not saved when in template mode...
	if (!bTemplate) {
		// Save session state...
		QDomElement eState = pDocument->document()->createElement("state");
		pDocument->saveTextElement("loop-start",
			QString::number(qtractorSession::loopStart()), &eState);
		pDocument->saveTextElement("loop-end",
			QString::number(qtractorSession::loopEnd()), &eState);
		pDocument->saveTextElement("punch-in",
			QString::number(qtractorSession::punchIn()), &eState);
		pDocument->saveTextElement("punch-out",
			QString::number(qtractorSession::punchOut()), &eState);
		pElement->appendChild(eState);
		// Save file lists...
		QDomElement eFiles = pDocument->document()->createElement("files");
		// Audio files...
		QDomElement eAudioList
			= pDocument->document()->createElement("audio-list");
		qtractorAudioListView *pAudioList = nullptr;
		if (pDocument->files())
			pAudioList = pDocument->files()->audioListView();
		if (pAudioList == nullptr)
			return false;
		if (!pAudioList->saveElement(pDocument, &eAudioList))
			return false;
		eFiles.appendChild(eAudioList);
		// MIDI files...
		QDomElement eMidiList
			= pDocument->document()->createElement("midi-list");
		qtractorMidiListView *pMidiList = nullptr;
		if (pDocument->files())
			pMidiList = pDocument->files()->midiListView();
		if (pMidiList == nullptr)
			return false;
		if (!pMidiList->saveElement(pDocument, &eMidiList))
			return false;
		eFiles.appendChild(eMidiList);
		pElement->appendChild(eFiles);
	}

	// Save device lists...
	QDomElement eDevices = pDocument->document()->createElement("devices");
	// Audio engine...
	QDomElement eAudioEngine
		= pDocument->document()->createElement("audio-engine");
	if (!qtractorSession::audioEngine()->saveElement(pDocument, &eAudioEngine))
		return false;
	eDevices.appendChild(eAudioEngine);
	// MIDI engine...
	QDomElement eMidiEngine
		= pDocument->document()->createElement("midi-engine");
	if (!qtractorSession::midiEngine()->saveElement(pDocument, &eMidiEngine))
		return false;
	eDevices.appendChild(eMidiEngine);
	pElement->appendChild(eDevices);

	// Save tempo/time-signature, if any...
	qtractorTimeScale::Node *pNode
		= qtractorSession::timeScale()->nodes().first();
	if (pNode) pNode = pNode->next(); // Skip first anchor node.
	if (pNode) {
		QDomElement eTempoMap = pDocument->document()->createElement("tempo-map");
		while (pNode) {
			QDomElement eNode = pDocument->document()->createElement("tempo-node");
			eNode.setAttribute("bar", QString::number(pNode->bar));
			eNode.setAttribute("frame", QString::number(pNode->frame));
			pDocument->saveTextElement("tempo",
				QString::number(pNode->tempo), &eNode);
			pDocument->saveTextElement("beat-type",
				QString::number(pNode->beatType), &eNode);
			pDocument->saveTextElement("beats-per-bar",
				QString::number(pNode->beatsPerBar), &eNode);
			pDocument->saveTextElement("beat-divisor",
				QString::number(pNode->beatDivisor), &eNode);
			eTempoMap.appendChild(eNode);
			pNode = pNode->next();
		}
		pElement->appendChild(eTempoMap);
	}

	// Save location markers/key-signatures, if any...
	qtractorTimeScale::Marker *pMarker
		= qtractorSession::timeScale()->markers().first();
	if (pMarker) {
		QDomElement eMarkers = pDocument->document()->createElement("markers");
		while (pMarker) {
			QDomElement eMarker = pDocument->document()->createElement("marker");
			eMarker.setAttribute("frame", QString::number(pMarker->frame));
			if (!pMarker->text.isEmpty()) {
				pDocument->saveTextElement("text", pMarker->text, &eMarker);
				pDocument->saveTextElement("color", pMarker->color.name(), &eMarker);
			}
			if (qtractorTimeScale::isKeySignature(
					pMarker->accidentals, pMarker->mode)) {
				pDocument->saveTextElement("accidentals",
					QString::number(pMarker->accidentals), &eMarker);
				pDocument->saveTextElement("mode",
					QString::number(pMarker->mode), &eMarker);
			}
			eMarkers.appendChild(eMarker);
			pMarker = pMarker->next();
		}
		pElement->appendChild(eMarkers);
	}

	// Save track view state...
	QDomElement eTracks = pDocument->document()->createElement("tracks");
	QDomElement eView   = pDocument->document()->createElement("view");
	pDocument->saveTextElement("pixels-per-beat",
		QString::number(qtractorSession::pixelsPerBeat()), &eView);
	pDocument->saveTextElement("horizontal-zoom",
		QString::number(qtractorSession::horizontalZoom()), &eView);
	pDocument->saveTextElement("vertical-zoom",
		QString::number(qtractorSession::verticalZoom()), &eView);
	pDocument->saveTextElement("snap-per-beat",
		QString::number(qtractorSession::snapPerBeat()), &eView);
	if (!bTemplate) {
		pDocument->saveTextElement("edit-head",
			QString::number(qtractorSession::editHead()), &eView);
		pDocument->saveTextElement("edit-tail",
			QString::number(qtractorSession::editTail()), &eView);
	}
	eTracks.appendChild(eView);
	// Save session tracks...
	for (qtractorTrack *pTrack = qtractorSession::tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// Create the new track element...
		QDomElement eTrack = pDocument->document()->createElement("track");
		if (!pTrack->saveElement(pDocument, &eTrack))
			return false;
		// Add this slot...
		eTracks.appendChild(eTrack);
	}
	pElement->appendChild(eTracks);

	return true;
}


// Rename session files...
void qtractorSession::renameSession (
	const QString& sOldName, const QString& sNewName )
{
	// Do nothing if names are obviously the same...
	if (sOldName == sNewName)
		return;

	qtractorFiles *pFiles = nullptr;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pFiles = pMainForm->files();
	if (pFiles == nullptr)
		return;

	// Lock it up...
	lock();

	const QRegularExpression rx('^' + sOldName);

	// For each and every track and clip...
	QStringList paths;

	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		// Refer to a proper files-view...
		qtractorFileListView *pFileListView = nullptr;
		qtractorFileList::Type iFileType = qtractorFileList::Audio;
		if (pTrack->trackType() == qtractorTrack::Midi) {
			pFileListView = pFiles->midiListView();
			iFileType = qtractorFileList::Midi;
		} else {
			pFileListView = pFiles->audioListView();
		//	iFileType = qtractorFileList::Audio;
		}
		// For each and every elligible clip...
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			// Rename clip filename prefix...
			const QFileInfo info1(pClip->filename());
			QString sFileName = info1.fileName();
			if (!sFileName.contains(rx))
				continue;
			sFileName.replace(rx, sNewName);
			QFileInfo info2(info1.dir(), sFileName);
			const QString& sOldFilePath
				= info1.absoluteFilePath();
			if (!paths.contains(sOldFilePath)) {
				paths.append(sOldFilePath);
				// Increment filename suffix if exists already...
				if (info2.exists()) {
					int iFileNo = 0;
					sFileName = info2.completeBaseName();
					QRegularExpression rxFileNo("([0-9]+)$");
					QRegularExpressionMatch match = rxFileNo.match(sFileName);
					if (match.hasMatch()) {
						iFileNo = match.captured(1).toInt();
						sFileName.remove(rxFileNo);
					}
					else sFileName += '-';
					sFileName += "%1." + info1.suffix();
					do { info2.setFile(info1.dir(), sFileName.arg(++iFileNo)); }
					while (info2.exists());
				}
			}
			const QString& sNewFilePath
				= info2.absoluteFilePath();
			if (!paths.contains(sNewFilePath)) {
				paths.append(sNewFilePath);
				if (!QFile::rename(sOldFilePath, sNewFilePath))
					continue;
			#ifdef CONFIG_DEBUG
				qDebug("qtractorSession::renameSession: \"%s\" -> \"%s\"",
					info1.fileName().toUtf8().constData(),
					info2.fileName().toUtf8().constData());
			#endif
			}
			// Re-hash clip filenames...
			if (iFileType == qtractorFileList::Midi) {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (pClip);
				if (pMidiClip)
					pMidiClip->setFilenameEx(sNewFilePath, true);
			} else {
				pClip->close();
				pClip->setFilename(sNewFilePath);
				pClip->open();
			}
			// Manage files-view item...
			if (pFileListView) {
				qtractorFileGroupItem *pGroupItem = nullptr;
				qtractorFileListItem *pFileItem
					= pFileListView->findFileItem(sOldFilePath);
				if (pFileItem) {
					pGroupItem = pFileItem->groupItem();
					m_pFiles->removeFileItem(iFileType, pFileItem->path());
					delete pFileItem;
				}
				pFileListView->addFileItem(sNewFilePath, pGroupItem);
			}
		}
	}

	// If session name has changed, we'll prompt
	// for correct filename when save is triggered...
	pMainForm->clearFilename();

	// Done.
	unlock();
}


//-------------------------------------------------------------------------
// qtractorSession::Document -- Session file import/export helper class.
//

// Constructor.
qtractorSession::Document::Document ( QDomDocument *pDocument,
	qtractorSession *pSession, qtractorFiles *pFiles )
	: qtractorDocument(pDocument, "session")
{
	m_pSession = pSession;
	m_pFiles   = pFiles;
}


// Default destructor.
qtractorSession::Document::~Document (void)
{
}


// Session accessor.
qtractorSession *qtractorSession::Document::session (void) const
{
	return m_pSession;
}


// File list accessor.
qtractorFiles *qtractorSession::Document::files (void) const
{
	return m_pFiles;
}


// The elemental loader implementation.
bool qtractorSession::Document::loadElement ( QDomElement *pElement )
{
	return m_pSession->loadElement(this, pElement);
}


// The elemental saver implementation.
bool qtractorSession::Document::saveElement ( QDomElement *pElement )
{
	return m_pSession->saveElement(this, pElement);
}


// end of qtractorSession.cpp
