// qtractorSession.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSessionCursor.h"
#include "qtractorSessionDocument.h"

#include "qtractorAudioEngine.h"
#include "qtractorAudioPeak.h"
#include "qtractorAudioClip.h"
#include "qtractorAudioBuffer.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"

#include "qtractorPlugin.h"

#include <QApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QRegExp>
#include <QDir>

#include <math.h>


//-------------------------------------------------------------------------
// qtractorSession::Properties -- Session properties structure.

// Helper copy method.
qtractorSession::Properties& qtractorSession::Properties::copy (
	const Properties& props )
{
	sessionDir     = props.sessionDir;
	sessionName    = props.sessionName;
	description    = props.description;
	sampleRate     = props.sampleRate;
	tempo          = props.tempo;
	ticksPerBeat   = props.ticksPerBeat;
	beatsPerBar    = props.beatsPerBar;
	pixelsPerBeat  = props.pixelsPerBeat;
	horizontalZoom = props.horizontalZoom;
	verticalZoom   = props.verticalZoom;
	snapPerBeat    = props.snapPerBeat;
	editHead       = props.editHead;
	editTail       = props.editTail;
	editHeadTime   = props.editHeadTime;
	editTailTime   = props.editTailTime;
	
	return *this;
}

// Helper clear/reset method.
void qtractorSession::Properties::clear (void)
{
	sessionDir     = QDir().absolutePath();
	sessionName    = QString::null;
	description    = QString::null;
	sampleRate     = 44100;
	tempo          = 120.0f;
	ticksPerBeat   = 120;
	beatsPerBar    = 4;
	pixelsPerBeat  = 32;
	horizontalZoom = 100;
	verticalZoom   = 100;
	snapPerBeat    = 4;
	editHead       = 0;
	editTail       = 0;
	editHeadTime   = 0;
	editTailTime   = 0;
}


//-------------------------------------------------------------------------
// qtractorSession -- Session container.

// Constructor.
qtractorSession::qtractorSession (void)
{
	m_tracks.setAutoDelete(true);
	m_cursors.setAutoDelete(false);

	// The dubious permanency of the crucial device engines.
	m_pMidiEngine       = new qtractorMidiEngine(this);
	m_pAudioEngine      = new qtractorAudioEngine(this);
	m_pAudioPeakFactory = new qtractorAudioPeakFactory();

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
}


// Open session engine(s).
bool qtractorSession::open ( const QString& sClientName )
{
	// A default MIDI master bus is always in order...
	if (m_pMidiEngine->buses().count() == 0)
		m_pMidiEngine->addBus(new qtractorMidiBus(m_pMidiEngine, "Master"));

	// Get over the stereo playback default master bus...
	if (m_pAudioEngine->buses().count() == 0)
		m_pAudioEngine->addBus(new qtractorAudioBus(m_pAudioEngine, "Master"));

	//  Actually open session device engines...
	if (!m_pAudioEngine->open(sClientName) ||
		!m_pMidiEngine->open(sClientName)) {
		close();
		return false;
	}

	// Open all tracks (assign buses)...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		if (!pTrack->open()) {
			close();
			return false;
		}
	}

	// Done.
	return true;
}


// Close session engine(s).
void qtractorSession::close (void)
{
	m_pAudioEngine->close();
	m_pMidiEngine->close();

	// Close all tracks (unassign buses)...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		pTrack->close();
	}

//	clear();
}


// Reset session.
void qtractorSession::clear (void)
{
	ATOMIC_SET(&m_locks, 0);
	ATOMIC_SET(&m_mutex, 0);

	m_tracks.clear();
	m_cursors.clear();

	m_props.clear();

	m_midiTags.clear();

	m_pAudioEngine->clear();
	m_pMidiEngine->clear();

	m_iSessionLength = 0;

	m_iRecordTracks  = 0;
	m_iMuteTracks    = 0;
	m_iSoloTracks    = 0;

	m_iMidiTag       = 0;

	m_iLoopStart     = 0;
	m_iLoopEnd       = 0;
	m_iLoopStartTime = 0;
	m_iLoopEndTime   = 0;

	m_bRecording     = false;

	updateTimeScale();

	m_pAudioEngine->sessionCursor()->reset();
	m_pAudioEngine->sessionCursor()->seek(0);
	m_cursors.append(m_pAudioEngine->sessionCursor());

	m_pMidiEngine->sessionCursor()->reset();
	m_pMidiEngine->sessionCursor()->seek(0);
	m_cursors.append(m_pMidiEngine->sessionCursor());
}


// Session directory path accessors.
void qtractorSession::setSessionDir ( const QString& sSessionDir )
{
	m_props.sessionDir = sSessionDir;
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
bool qtractorSession::updateSessionLength ( unsigned long iSessionLength )
{
	// Find the last and longest clip frame position...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		qtractorClip *pClip = pTrack->clips().last();
		if (pClip && pClip->clipStart() + pClip->clipLength() > iSessionLength)
			iSessionLength = pClip->clipStart() + pClip->clipLength();
	}
	// Just return if we've not changed.
	if (iSessionLength == m_iSessionLength)
		return false;
	// Set new one.
	m_iSessionLength = iSessionLength;
	// Done.
	return true;
}


// Session length accessors.
unsigned long qtractorSession::sessionLength (void) const
{
	return m_iSessionLength;
}


// Sample rate accessors.
void qtractorSession::setSampleRate ( unsigned int iSampleRate )
{
	m_props.sampleRate = iSampleRate;
}

unsigned int qtractorSession::sampleRate (void) const
{
	return m_props.sampleRate;
}


// Session tempo accessors.
void qtractorSession::setTempo ( float fTempo )
{
	m_props.tempo = fTempo;
}

float qtractorSession::tempo (void) const
{
	return m_props.tempo;
}


// Resolution accessors.
void qtractorSession::setTicksPerBeat ( unsigned short iTicksPerBeat )
{
	m_props.ticksPerBeat = iTicksPerBeat;
}

unsigned short qtractorSession::ticksPerBeat (void) const
{
	return m_props.ticksPerBeat;
}


// Beats/Bar(measure) accessors.
void qtractorSession::setBeatsPerBar ( unsigned short iBeatsPerBar )
{
	m_props.beatsPerBar = iBeatsPerBar;
}


unsigned short qtractorSession::beatsPerBar (void) const
{
	return m_props.beatsPerBar;
}


bool qtractorSession::beatIsBar ( unsigned int iBeat ) const
{
	return ((iBeat % m_props.beatsPerBar) == 0);
}


// Pixels per beat (width).	
void qtractorSession::setPixelsPerBeat ( unsigned short iPixelsPerBeat )
{
	m_props.pixelsPerBeat = iPixelsPerBeat;
}

unsigned short qtractorSession::pixelsPerBeat (void) const
{ 
	return m_props.pixelsPerBeat;
}


// Horizontal zoom factor.
void qtractorSession::setHorizontalZoom ( unsigned short iHorizontalZoom )
{ 
	m_props.horizontalZoom = iHorizontalZoom;
}

unsigned short qtractorSession::horizontalZoom (void) const
{
	return m_props.horizontalZoom;
}


// Vertical zoom factor.
void qtractorSession::setVerticalZoom ( unsigned short iVerticalZoom )
{ 
	m_props.verticalZoom = iVerticalZoom;
}

unsigned short qtractorSession::verticalZoom (void) const
{ 
	return m_props.verticalZoom;
}


// Beat divisor (snap) accessors.
void qtractorSession::setSnapPerBeat ( unsigned short iSnapPerBeat )
{
	m_props.snapPerBeat = iSnapPerBeat;
}

unsigned short qtractorSession::snapPerBeat (void) const
{
	return m_props.snapPerBeat;
}


// Beat divisor (snap index) accessors.
unsigned short qtractorSession::snapFromIndex ( int iSnap )
{
	unsigned short iSnapPerBeat = 0;
	if (iSnap > 0)
		iSnapPerBeat++;
	for (int i = 1; i < iSnap; i++)
		iSnapPerBeat <<= 1;
	return iSnapPerBeat;
}


int qtractorSession::indexFromSnap ( unsigned short iSnapPerBeat )
{
	int iSnap = 0;
	for (unsigned short n = 1; n <= iSnapPerBeat; n <<= 1)
		++iSnap;
	return iSnap;
}


// Edit-head frame accessors.
void qtractorSession::setEditHead ( unsigned long iEditHead )
{
	m_props.editHead     = iEditHead;
	m_props.editHeadTime = tickFromFrame(iEditHead);
}

unsigned long qtractorSession::editHead (void) const
{
	return m_props.editHead;
}


void qtractorSession::setEditTail ( unsigned long iEditTail )
{
	m_props.editTail     = iEditTail;
	m_props.editTailTime = tickFromFrame(iEditTail);
}

unsigned long qtractorSession::editTail (void) const
{
	return m_props.editTail;
}


// Pixel/Beat number conversion.
unsigned int qtractorSession::beatFromPixel ( unsigned int x ) const
{
	return ((100 * x) / m_iScale_a);
}

unsigned int qtractorSession::pixelFromBeat ( unsigned int iBeat ) const
{ 
	return ((iBeat * m_iScale_a) / 100);
}


// Pixel/Tick number conversion.
unsigned long qtractorSession::tickFromPixel ( unsigned int x ) const
{
	return (unsigned long) ::lroundf((m_fScale_d * x) / m_fScale_b);
}

unsigned int qtractorSession::pixelFromTick ( unsigned long iTick ) const
{
	return (unsigned int) ::lroundf((m_fScale_b * iTick) / m_fScale_d);
}


// Pixel/Frame number conversion.
unsigned long qtractorSession::frameFromPixel ( unsigned int x ) const
{
	return (unsigned long) ::lroundf((m_fScale_c * x) / m_fScale_b);
}

unsigned int qtractorSession::pixelFromFrame ( unsigned long iFrame ) const
{ 
	return (unsigned int) ::lroundf((m_fScale_b * iFrame) / m_fScale_c);
}


// Beat/frame conversion.
unsigned long qtractorSession::frameFromBeat ( unsigned int iBeat ) const
{
	return (unsigned long) ::lroundf((m_fScale_c * iBeat) / m_props.tempo);
}

unsigned int qtractorSession::beatFromFrame ( unsigned long iFrame ) const
{
	return (unsigned int) ::lroundf((m_props.tempo * iFrame) / m_fScale_c);
}


// Tick/Frame number conversion.
unsigned long qtractorSession::frameFromTick ( unsigned long iTick ) const
{
	return (unsigned long) ::lroundf((m_fScale_c * iTick) / m_fScale_d);
}

unsigned long qtractorSession::tickFromFrame ( unsigned long iFrame ) const
{
	return (unsigned long) ::lroundf((m_fScale_d * iFrame) / m_fScale_c);
}


// Beat/frame snap filters.
unsigned long qtractorSession::tickSnap ( unsigned long iTick ) const
{
	unsigned long iTickSnap = iTick;
	if (m_props.snapPerBeat > 0) {
		unsigned long q = m_props.ticksPerBeat / m_props.snapPerBeat;
		iTickSnap = q * ((iTickSnap + (q >> 1)) / q);
	}
	return iTickSnap;
}

unsigned long qtractorSession::frameSnap ( unsigned long iFrame ) const
{
	return frameFromTick(tickSnap(tickFromFrame(iFrame)));
}

unsigned int qtractorSession::pixelSnap ( unsigned int x ) const
{
	return pixelFromTick(tickSnap(tickFromPixel(x)));
}


// Frame/locate (SMPTE) conversion.
unsigned long qtractorSession::frameFromLocate ( unsigned long iLocate ) const
{
	return (iLocate * m_props.sampleRate) / 30;
}

unsigned long qtractorSession::locateFromFrame ( unsigned long iFrame ) const
{ 
	return (30 * iFrame) / m_props.sampleRate;
}


// Convert frame to time string.
QString qtractorSession::timeFromFrame ( unsigned long iFrame, bool bBBT ) const
{
	QString sTime;

	if (bBBT) {
		// Time frame code in bars.beats.ticks ...
		unsigned int bars, beats;
		unsigned long ticks = tickFromFrame(iFrame);
		bars = beats = 0;
		if (ticks >= (unsigned long) m_props.ticksPerBeat) {
			beats  = (unsigned int)  (ticks / m_props.ticksPerBeat);
			ticks -= (unsigned long) (beats * m_props.ticksPerBeat);
		}
		if (beats >= (unsigned int) m_props.beatsPerBar) {
			bars   = (unsigned int) (beats / m_props.beatsPerBar);
			beats -= (unsigned int) (bars  * m_props.beatsPerBar);
		}
		sTime.sprintf("%u.%02u.%03lu", bars + 1, beats + 1, ticks);
	} else {
		// Time frame code in hh:mm:ss.zzz ...
		unsigned int hh, mm, ss, zzz;
		float secs = (float) iFrame / (float) m_props.sampleRate;
		hh = mm = ss = 0;
		if (secs >= 3600.0f) {
			hh = (unsigned int) (secs / 3600.0f);
			secs -= (float) hh * 3600.0f;
		}
		if (secs >= 60.0f) {
			mm = (unsigned int) (secs / 60.0f);
			secs -= (float) mm * 60.0f;
		}
		if (secs >= 0.0f) {
			ss = (unsigned int) secs;
			secs -= (float) ss;
		}
		zzz = (unsigned int) (secs * 1000.0f);
		sTime.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
	}

	return sTime;
}


// Update scale divisor factors.
void qtractorSession::updateTimeScale (void) 
{
	// Recompute scale divisor factors...
	m_iScale_a = (unsigned int) (m_props.horizontalZoom * m_props.pixelsPerBeat);
	m_fScale_b = (float) (0.01f * m_props.tempo * m_iScale_a);
	m_fScale_c = (float) (60.0f * m_props.sampleRate);
	m_fScale_d = (float) (m_props.tempo * m_props.ticksPerBeat);

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

	// Do not forget those edit points too...
	m_props.editHead = frameFromTick(m_props.editHeadTime);
	m_props.editTail = frameFromTick(m_props.editTailTime);
}


// Update from disparate sample-rate.
void qtractorSession::updateSampleRate ( unsigned int iOldSampleRate ) 
{
	if (iOldSampleRate < 8000 || iOldSampleRate == m_props.sampleRate)
		return;

	// Unfortunatelly we must close all clips first,
	// so let at least all audio peaks be refreshned...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
			pClip->close(true);
		}
	}

	// Give it some room to just that...
	stabilize();
	updateTimeScale();
	stabilize();

	// Set the conversion ratio...
	float fRatio = float(m_props.sampleRate) / float(iOldSampleRate);

	// Adjust all tracks and clips (reopening all those...)
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		for (qtractorClip *pClip = pTrack->clips().first();
				pClip; pClip = pClip->next()) {
		//	pClip->setClipStart(
		//		::lroundf(fRatio * float(pClip->clipStart())));
			pClip->setClipOffset(
				::lroundf(fRatio * float(pClip->clipOffset())));
		//	pClip->setClipLength(
		//		::lroundf(fRatio * float(pClip->clipLength())));
			pClip->setFadeInLength(
				::lroundf(fRatio * float(pClip->fadeInLength())));
			pClip->setFadeOutLength(
				::lroundf(fRatio * float(pClip->fadeOutLength())));
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

	qtractorSessionCursor *pSessionCursor = m_cursors.first();
	while (pSessionCursor) {
		pSessionCursor->addTrack(pTrack);
		pSessionCursor = pSessionCursor->next();
	}

	pTrack->setLoop(m_iLoopStart, m_iLoopEnd);
	pTrack->open();

//	unlock();
}


void qtractorSession::moveTrack ( qtractorTrack *pTrack,
	qtractorTrack *pNextTrack )
{
//	lock();

	m_tracks.unlink(pTrack);
	m_tracks.insertBefore(pTrack, pNextTrack);

	reset();

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
		m_iRecordTracks++;
	} else if (m_iRecordTracks > 0) {
		m_iRecordTracks--;
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
		m_iMuteTracks++;
	} else if (m_iMuteTracks > 0) {
		m_iMuteTracks--;
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
		m_iSoloTracks++;
	} else if (m_iSoloTracks > 0) {
		m_iSoloTracks--;
	}
}

unsigned int qtractorSession::soloTracks (void) const
{
	return m_iSoloTracks;
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


// Reset all linked session cursors.
void qtractorSession::reset (void)
{
	qtractorSessionCursor *pSessionCursor = m_cursors.first();
	while (pSessionCursor) {
		pSessionCursor->reset();
		pSessionCursor = pSessionCursor->next();
	}
}


// Reset (reactivate) all plugin chains...
void qtractorSession::resetAllPlugins (void)
{
	// All tracks...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->pluginList())
			pTrack->pluginList()->resetBuffer();
	}
	
	// All audio buses...
	for (qtractorBus *pBus = m_pAudioEngine->buses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			if (pAudioBus->pluginList_in())
				pAudioBus->pluginList_in()->resetBuffer();
			if (pAudioBus->pluginList_out())
				pAudioBus->pluginList_out()->resetBuffer();
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
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorSession::stabilize(%d)\n", msecs);
#endif
	// Wait a litle bit before continue...
	QTime t;
	t.start();
	while (t.elapsed() < msecs)
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
		unsigned long iClipStart = m_pAudioEngine->sessionCursor()->frame();
		for (qtractorTrack *pTrack = m_tracks.first();
				pTrack; pTrack = pTrack->next()) {
			qtractorClip *pClipRecord = pTrack->clipRecord();
			if (pClipRecord)
				pClipRecord->setClipStart(iClipStart);
		}
	}

	// Do it.
	m_pAudioEngine->setPlaying(bPlaying);
	m_pMidiEngine->setPlaying(bPlaying);
}

bool qtractorSession::isPlaying() const
{
	return (m_pAudioEngine->isPlaying() && m_pMidiEngine->isPlaying());
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


// Playhead positioning.
void qtractorSession::setPlayHead ( unsigned long iFrame )
{
	bool bPlaying = isPlaying();
	if (bPlaying && isRecording())
		return;

	lock();
	setPlaying(false);

	if (m_pAudioEngine->jackClient())
		jack_transport_locate(m_pAudioEngine->jackClient(), iFrame);

	seek(iFrame, true);

	setPlaying(bPlaying);
	unlock();
}

unsigned long qtractorSession::playHead (void) const
{
	return m_pAudioEngine->sessionCursor()->frame();
}


// Session loop points accessors.
void qtractorSession::setLoop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
	bool bPlaying = isPlaying();
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
	unsigned long iFrame = playHead();

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


// Sanitize a given name.
QString qtractorSession::sanitize ( const QString& s ) 
{
	return QString(s).replace(QRegExp("[\\s|\\.|\\-]+"), "_");
}


// Create a brand new filename (absolute file path).
QString qtractorSession::createFilePath ( const QString& sTrackName,
	int iClipNo, const QString& sExt ) 
{
	QString sFilename = qtractorSession::sanitize(m_props.sessionName)
		+ '-' + qtractorSession::sanitize(sTrackName) + "-%1." + sExt;

	if (iClipNo < 1)
		iClipNo++;

	QFileInfo fi(m_props.sessionDir, sFilename.arg(iClipNo));
	while (fi.exists())
		fi.setFile(m_props.sessionDir, sFilename.arg(++iClipNo));

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorSession::createFilePath(\"%s\")\n",
		fi.absoluteFilePath().toUtf8().constData());
#endif

	return fi.absoluteFilePath();
}


// Consolidated session record state.
void qtractorSession::setRecording ( bool bRecording )
{
	m_bRecording = bRecording;

	// For all armed tracks...
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->isRecord())
			trackRecord(pTrack, bRecording);
	}
}

bool qtractorSession::isRecording() const
{
	return m_bRecording;
}


// Immediate track record-arming.
void qtractorSession::trackRecord ( qtractorTrack *pTrack, bool bRecord )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorSession::trackRecord(\"%s\", %d)\n",
		pTrack->trackName().toUtf8().constData(), (int) bRecord);
#endif

	// Just ditch the in-record clip...
	if (!bRecord) {
		pTrack->setClipRecord(NULL);
		return;
	}

	// Here's the place to create and set the capture clip...
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
	{
		qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
		pAudioClip->setClipStart(playHead());
		pAudioClip->openAudioFile(
			createFilePath(pTrack->trackName(), 0,
				qtractorAudioFileFactory::defaultExt()),
			qtractorAudioFile::Write);
		pTrack->setClipRecord(pAudioClip);
		break;
	}
	case qtractorTrack::Midi:
	{
		qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
		pMidiClip->setClipStart(playHead());
		pMidiClip->openMidiFile(
			createFilePath(pTrack->trackName(), 0, "mid"),
			1, // iTrackChannel: Single-track SMF format 1 (2 tracks)
			qtractorMidiFile::Write);
		pTrack->setClipRecord(pMidiClip);
		break;
	}
	default:
		break;
	}
}

// Immediate track mute (engine indirection).
void qtractorSession::trackMute ( qtractorTrack *pTrack, bool bMute )
{
	// For the time being, only needed for ALSA sequencer...
	if (pTrack->trackType() == qtractorTrack::Midi)
		m_pMidiEngine->trackMute(pTrack, bMute);

	// But not, plugins must also be muted (sort of)...
	if (bMute && pTrack->pluginList())
		pTrack->pluginList()->resetBuffer();
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
		if (pTrack == pTrackMute || pTrackMute->isMute())
			continue;
		// (Un)mute each other track...
		trackMute(pTrackMute, bSolo);
	}
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
	unsigned short iMidiTag = pTrack->midiTag();
	if (iMidiTag > 0) {
		m_midiTags.push_back(iMidiTag);
		pTrack->setMidiTag(0);
	}
}


// MIDI session/tracks instrument patching.
void qtractorSession::setMidiPatch ( qtractorInstrumentList *pInstruments )
{
	for (qtractorTrack *pTrack = m_tracks.first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Midi)
			pTrack->setMidiPatch(pInstruments);
	}
}


// Session special process cycle executive.
void qtractorSession::process ( qtractorSessionCursor *pSessionCursor,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Now, for every Audio track...
	int iTrack = 0;
	qtractorTrack *pTrack = m_tracks.first();
	while (pTrack) {
		if (pTrack->trackType() == pSessionCursor->syncType()) {
			pTrack->process(pSessionCursor->clip(iTrack),
				iFrameStart, iFrameEnd);
		}
		pTrack = pTrack->next();
		iTrack++;
	}
}


// Document element methods.
bool qtractorSession::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	qtractorSession::clear();
	qtractorSession::setSessionName(pElement->attribute("name"));

	// Session state should be postponed...
	unsigned long iLoopStart = 0;
	unsigned long iLoopEnd   = 0;

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
				else if (eProp.tagName() == "sample-rate")
					qtractorSession::setSampleRate(eProp.text().toUInt());
				else if (eProp.tagName() == "tempo")
					qtractorSession::setTempo(eProp.text().toFloat());
				else if (eProp.tagName() == "ticks-per-beat")
					qtractorSession::setTicksPerBeat(eProp.text().toUShort());
				else if (eProp.tagName() == "beats-per-bar")
					qtractorSession::setBeatsPerBar(eProp.text().toUShort());
			}
			// We need to make this permanent, right now.
			qtractorSession::updateTimeScale();
		}
		else
		if (eChild.tagName() == "state") {
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
			}
		}
		else
		// Load file lists...
		if (eChild.tagName() == "files") {
			for (QDomNode nList = eChild.firstChild();
					!nList.isNull();
						nList = nList.nextSibling()) {
				// Convert filelist node to element...
				QDomElement eList = nList.toElement();
				if (eList.isNull())
					continue;
				if (eList.tagName() == "audio-list") {
					qtractorAudioListView *pAudioList = NULL;
					if (pDocument->files())
						pAudioList = pDocument->files()->audioListView();
					if (pAudioList == NULL)
						return false;
					if (!pAudioList->loadElement(pDocument, &eList))
						return false;
				}
				else
				if (eList.tagName() == "midi-list") {
					qtractorMidiListView *pMidiList = NULL;
					if (pDocument->files())
						pMidiList = pDocument->files()->midiListView();
					if (pMidiList == NULL)
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
						else if (eView.tagName() == "edit-head")
							qtractorSession::setEditHead(eView.text().toULong());
						else if (eView.tagName() == "edit-tail")
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
	qtractorSession::updateSessionLength();

	// Check whether some deferred state needs to be set...
	if (iLoopStart < iLoopEnd)
		qtractorSession::setLoop(iLoopStart, iLoopEnd);

	return true;
}


bool qtractorSession::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	pElement->setAttribute("name", qtractorSession::sessionName());
	pElement->setAttribute("version", QTRACTOR_VERSION);

	// Save session properties...
	QDomElement eProps = pDocument->document()->createElement("properties");
	pDocument->saveTextElement("directory",
		qtractorSession::sessionDir(), &eProps);
	pDocument->saveTextElement("description",
		qtractorSession::description(), &eProps);
	pDocument->saveTextElement("sample-rate",
		QString::number(qtractorSession::sampleRate()), &eProps);
	pDocument->saveTextElement("tempo",
		QString::number(qtractorSession::tempo()), &eProps);
	pDocument->saveTextElement("ticks-per-beat",
		QString::number(qtractorSession::ticksPerBeat()), &eProps);
	pDocument->saveTextElement("beats-per-bar",
		QString::number(qtractorSession::beatsPerBar()), &eProps);
	pElement->appendChild(eProps);

	// Save session state...
	QDomElement eState = pDocument->document()->createElement("state");
	pDocument->saveTextElement("loop-start",
		QString::number(qtractorSession::loopStart()), &eState);
	pDocument->saveTextElement("loop-end",
		QString::number(qtractorSession::loopEnd()), &eState);
	pElement->appendChild(eState);

	// Save file lists...
	QDomElement eFiles = pDocument->document()->createElement("files");
	// Audio files...
	QDomElement eAudioList = pDocument->document()->createElement("audio-list");
	qtractorAudioListView *pAudioList = NULL;
	if (pDocument->files())
		pAudioList = pDocument->files()->audioListView();
	if (pAudioList == NULL)
		return false;
	if (!pAudioList->saveElement(pDocument, &eAudioList))
		return false;
	eFiles.appendChild(eAudioList);
	// MIDI files...
	QDomElement eMidiList = pDocument->document()->createElement("midi-list");
	qtractorMidiListView *pMidiList = NULL;
	if (pDocument->files())
		pMidiList = pDocument->files()->midiListView();
	if (pMidiList == NULL)
		return false;
	if (!pMidiList->saveElement(pDocument, &eMidiList))
		return false;
	eFiles.appendChild(eMidiList);
	pElement->appendChild(eFiles);

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
	pDocument->saveTextElement("edit-head",
		QString::number(qtractorSession::editHead()), &eView);
	pDocument->saveTextElement("edit-tail",
		QString::number(qtractorSession::editTail()), &eView);
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


// end of qtractorSession.cpp
