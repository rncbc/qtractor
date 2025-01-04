// qtractorTrack.cpp
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
#include "qtractorTrack.h"

#include "qtractorSession.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"

#include "qtractorDocument.h"
#include "qtractorAudioEngine.h"
#include "qtractorAudioMonitor.h"
#include "qtractorAudioBuffer.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiMonitor.h"
#include "qtractorMidiManager.h"
#include "qtractorInstrument.h"
#include "qtractorPlugin.h"
#include "qtractorMixer.h"
#include "qtractorMeter.h"
#include "qtractorCurveFile.h"

#include "qtractorFileList.h"

#include "qtractorTrackCommand.h"

#include "qtractorMainForm.h"
#include "qtractorTracks.h"
#include "qtractorTrackList.h"

#include <QPainter>

#include <QDomDocument>
#include <QFileInfo>



// MIDI specific controllers.
#define MIDI_CHANNEL_VOLUME		0x07
#define MIDI_CHANNEL_PANNING	0x0a

//------------------------------------------------------------------------
// qtractorTrack::StateObserver -- Local track state observer.

class qtractorTrack::StateObserver : public qtractorMidiControlObserver
{
public:

	// Constructor.
	StateObserver(qtractorTrack *pTrack, ToolType toolType,
		qtractorSubject *pSubject) : qtractorMidiControlObserver(pSubject),
			m_pTrack(pTrack), m_toolType(toolType) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
	{
		const bool bOn = (value() > 0.0f);
		switch (m_toolType) {
		case qtractorTrack::Record:
			m_pTrack->setRecord(bOn);
			break;
		case qtractorTrack::Mute:
			m_pTrack->setMute(bOn);
			break;
		case qtractorTrack::Solo:
			m_pTrack->setSolo(bOn);
			break;
		}
		qtractorMidiControlObserver::update(bUpdate);
		if (bUpdate) {
			qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
			if (pMainForm)
				pMainForm->tracks()->updateContents(true);
		}
	}

private:

	// Members.
	qtractorTrack *m_pTrack;
	ToolType m_toolType;
};


//----------------------------------------------------------------------------
// qtractorTrack::MidiVolumeObserver -- Local dedicated observer.

class qtractorTrack::MidiVolumeObserver : public qtractorObserver
{
public:

	// Constructor.
	MidiVolumeObserver(qtractorTrack *pTrack, qtractorSubject *pSubject)
		: qtractorObserver(pSubject), m_pTrack(pTrack) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
		{ m_pTrack->setMidiVolume(int(127.0f * value()) & 0x7f, bUpdate); }

private:

	// Members.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------------
// qtractorTrack::MidiPanningObserver -- Local dedicated observer.

class qtractorTrack::MidiPanningObserver : public qtractorObserver
{
public:

	// Constructor.
	MidiPanningObserver(qtractorTrack *pTrack, qtractorSubject *pSubject)
		: qtractorObserver(pSubject), m_pTrack(pTrack) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
		{ m_pTrack->setMidiPanning((0x40 + int(63.0f * value())) & 0x7f, bUpdate); }

private:

	// Members.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------------
// qtractorTrack::MidiProgramObserver -- Local dedicated observer.

class qtractorTrack::MidiProgramObserver : public qtractorObserver
{
public:

	// Constructor.
	MidiProgramObserver(qtractorTrack *pTrack, qtractorSubject *pSubject)
		: qtractorObserver(pSubject), m_pTrack(pTrack) {}

protected:

	// Update feedback.
	void update(bool bUpdate)
	{
		const int iValue = int(value());
		const int iBank = (iValue >> 7) & 0x3fff;
		const int iProg = (iValue & 0x7f);
		m_pTrack->setMidiBank(iBank);
		m_pTrack->setMidiProg(iProg);
		// Refresh track item, at least the names...
		if (bUpdate) m_pTrack->updateTrack();
	}

private:

	// Members.
	qtractorTrack *m_pTrack;
};


//-------------------------------------------------------------------------
// qtractorTrack::Properties -- Track properties structure.

// Helper clear/reset method.
void qtractorTrack::Properties::clear (void)
{
	trackName.clear();
	trackIcon.clear();
	trackType   = None;
	monitor     = false;
	record      = false;
	mute        = false;
	solo        = false;
	gain        = 1.0f;
	panning     = 0.0f;
	inputBusName.clear();
	outputBusName.clear();
	pluginListLatency = false;
	midiOmni    = false;
	midiChannel = 0;
	midiBankSelMethod = -1;
	midiBank    = -1;
	midiProg    = -1;
	midiDrums   = false;
	foreground  = Qt::yellow;
	background  = Qt::darkBlue;
}

// Helper copy method.
qtractorTrack::Properties& qtractorTrack::Properties::copy (
	const Properties& props )
{
	if (&props != this) {
		trackName   = props.trackName;
		trackIcon   = props.trackIcon;
		trackType   = props.trackType;
		monitor     = props.monitor;
		record      = props.record;
		mute        = props.mute;
		solo        = props.solo;
		gain        = props.gain;
		panning     = props.panning;
		inputBusName  = props.inputBusName;
		outputBusName = props.outputBusName;
		pluginListLatency = props.pluginListLatency;
		midiOmni    = props.midiOmni;
		midiChannel = props.midiChannel;
		midiBankSelMethod = props.midiBankSelMethod;
		midiBank    = props.midiBank;
		midiProg    = props.midiProg;
		midiDrums   = props.midiDrums;
		foreground  = props.foreground;
		background  = props.background;
	}
	return *this;
}


// Take(record) descriptor/id registry methods.
void qtractorTrack::clearTakeInfo (void) const
{
	m_idtakes.clear();
	m_takeids.clear();
}


// Retrieve take(record) descriptor/id from registry.
qtractorTrack::TakeInfo *qtractorTrack::takeInfo ( int iTakeID ) const
{
	return m_idtakes.value(iTakeID, nullptr);
}

int qtractorTrack::takeInfoId ( qtractorTrack::TakeInfo *pTakeInfo ) const
{
	return m_takeids.value(pTakeInfo, -1);
}


// Add/new take(record) descriptor/id to registry.
int qtractorTrack::takeInfoNew ( qtractorTrack::TakeInfo *pTakeInfo ) const
{
	QHash<TakeInfo *, int>::ConstIterator iter
		= m_takeids.constFind(pTakeInfo);
	if (iter != m_takeids.constEnd()) {
		return iter.value();
	} else {
		const int iTakeID = m_takeids.count();
		takeInfoAdd(iTakeID, pTakeInfo);
		return iTakeID;
	}
}

void qtractorTrack::takeInfoAdd (
	int iTakeID, qtractorTrack::TakeInfo *pTakeInfo ) const
{
	m_idtakes.insert(iTakeID, pTakeInfo);
	m_takeids.insert(pTakeInfo, iTakeID);
}


//-------------------------------------------------------------------------
// qtractorTrack -- Track container.

// Constructor.
qtractorTrack::qtractorTrack ( qtractorSession *pSession, TrackType trackType )
{
	m_pSession = pSession;

	m_props.trackType = trackType;

	m_pInputBus  = nullptr;
	m_pOutputBus = nullptr;
	m_pMonitor   = nullptr;

	m_iMidiTag = 0;

	m_midiNoteMin = 0;
	m_midiNoteMax = 0;

	m_midiVolume  = 0;
	m_midiPanning = 0x40;

	m_pClipRecord = nullptr;
	m_iClipRecordStart = 0;

	m_bClipRecordEx = false;

	m_clips.setAutoDelete(true);

	m_pSyncThread = nullptr;

	m_pMidiVolumeObserver  = nullptr;
	m_pMidiPanningObserver = nullptr;

	m_pMonitorSubject = new qtractorSubject();
	m_pMonitorSubject->setToggled(true);

	m_pRecordSubject  = new qtractorSubject();
	m_pMuteSubject    = new qtractorSubject();
	m_pSoloSubject    = new qtractorSubject();

	m_pRecordSubject->setToggled(true);
	m_pMuteSubject->setToggled(true);
	m_pSoloSubject->setToggled(true);

	m_pMonitorObserver = new qtractorMidiControlObserver(m_pMonitorSubject);

	m_pRecordObserver = new StateObserver(this, Record, m_pRecordSubject);
	m_pMuteObserver   = new StateObserver(this, Mute,   m_pMuteSubject);
	m_pSoloObserver   = new StateObserver(this, Solo,   m_pSoloSubject);

	unsigned int iFlags = qtractorPluginList::Track;
	if (trackType == qtractorTrack::Midi)
		iFlags |= qtractorPluginList::Midi;

	m_pPluginList = new qtractorPluginList(0, iFlags);

	m_pCurveFile = new qtractorCurveFile(m_pPluginList->curveList());

	m_pMidiProgramObserver = nullptr;

	setHeight(HeightBase);	// Default track height.
	clear();
}


// Default desstructor.
qtractorTrack::~qtractorTrack (void)
{
	close();
	clear();

	if (m_pSoloObserver)
		delete m_pSoloObserver;
	if (m_pMuteObserver)
		delete m_pMuteObserver;
	if (m_pRecordObserver)
		delete m_pRecordObserver;
	if (m_pMonitorObserver)
		delete m_pMonitorObserver;

	if (m_pSoloSubject)
		delete m_pSoloSubject;
	if (m_pMuteSubject)
		delete m_pMuteSubject;
	if (m_pRecordSubject)
		delete m_pRecordSubject;
	if (m_pMonitorSubject)
		delete m_pMonitorSubject;

	qDeleteAll(m_controllers);
	m_controllers.clear();

	if (m_pCurveFile)
		delete m_pCurveFile;
	if (m_pPluginList)
		delete m_pPluginList;
	if (m_pMonitor)
		delete m_pMonitor;
}


// Reset track.
void qtractorTrack::clear (void)
{
	setClipRecord(nullptr);

	clearTakeInfo();
	m_clips.clear();

	m_pPluginList->clear();
	m_pCurveFile->clear();

	m_iMidiTag = 0;

	m_midiNoteMin = 0;
	m_midiNoteMax = 0;

	m_midiVolume  = 0;
	m_midiPanning = 0x40;

	m_props.midiBankSelMethod = -1;
	m_props.midiBank = -1;
	m_props.midiProg = -1;

	m_props.monitor = false;
	m_props.record  = false;
	m_props.mute    = false;
	m_props.solo    = false;
	m_props.gain    = 1.0f;
	m_props.panning = 0.0f;

	if (m_pSyncThread) {
		if (m_pSyncThread->isRunning()) do {
			m_pSyncThread->setRunState(false);
		//	m_pSyncThread->terminate();
			m_pSyncThread->sync();
		} while (!m_pSyncThread->wait(100));
		delete m_pSyncThread;
		m_pSyncThread = nullptr;
	}
}


// Track open method.
bool qtractorTrack::open (void)
{
	close();

	if (m_pSession == nullptr)
		return false;

	// Depending on track type...
	qtractorEngine *pEngine = nullptr;
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
	if (pEngine == nullptr)
		return false;

	// (Re)assign the input bus to the track.
	m_pInputBus = pEngine->findInputBus(inputBusName());
	// Fallback to first usable one...
	if (m_pInputBus == nullptr) {
		QListIterator<qtractorBus *> iter(pEngine->buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
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
	if (m_pOutputBus == nullptr) {
		QListIterator<qtractorBus *> iter(pEngine->buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
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
	if (m_pInputBus == nullptr || m_pOutputBus == nullptr)
		return false;
#endif

	// Remember current (output) monitor, for later deletion...
	qtractorMonitor *pMonitor = m_pMonitor;
	m_pMonitor = nullptr;

	// (Re)allocate (output) monitor...
	switch (m_props.trackType) {
	case qtractorTrack::Audio: {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (m_pOutputBus);
		if (pAudioBus) {
			m_pMonitor = new qtractorAudioMonitor(
				pAudioBus->channels(), m_props.gain, m_props.panning);
			m_pPluginList->setChannels(pAudioBus->channels(),
				qtractorPluginList::AudioTrack);
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (m_pOutputBus);
		if (pMidiBus) {
			qtractorMidiMonitor *pMidiMonitor
				= static_cast<qtractorMidiMonitor *> (pMonitor);
			if (pMidiMonitor) {
				m_pMonitor = new qtractorMidiMonitor(*pMidiMonitor);
			} else {
				m_pMonitor = new qtractorMidiMonitor(
					m_props.gain, m_props.panning);
			}
			m_pMidiVolumeObserver = new MidiVolumeObserver(
				this, m_pMonitor->gainSubject());
			m_pMidiPanningObserver = new MidiPanningObserver(
				this, m_pMonitor->panningSubject());
		}
		// Get audio bus as for the plugin list...
		qtractorAudioBus *pAudioBus = nullptr;
		qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
		if (pMidiManager)
			pAudioBus = pMidiManager->audioOutputBus();
		if (pAudioBus == nullptr) {
			// Output bus gets to be the first available output bus...
			QListIterator<qtractorBus *> iter(pAudioEngine->buses2());
			while (iter.hasNext()) {
				qtractorBus *pBus = iter.next();
				if (pBus->busMode() & qtractorBus::Output) {
					pAudioBus = static_cast<qtractorAudioBus *> (pBus);
					break;
				}
			}
		}
		// Set plugin-list buffer alright...
		if (pAudioBus) {
			m_pPluginList->setChannels(pAudioBus->channels(),
				qtractorPluginList::MidiTrack);
		}
		// Set MIDI bank/program observer...
		if (m_pPluginList->midiProgramSubject()) {
			m_pMidiProgramObserver = new MidiProgramObserver(this,
				m_pPluginList->midiProgramSubject());
		}
		break;
	}
	default:
		break;
	}

	// Before we get rid of old monitor...
	if (pMonitor) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			// Update mixer strip...
			qtractorMixer *pMixer = pMainForm->mixer();
			if (pMixer) {
				qtractorMixerStrip *pStrip
					= pMixer->trackRack()->findStrip(pMonitor);
				if (pStrip)
					pStrip->setTrack(this);
			}
			// Update track-list as well...
			qtractorTrackList *pTrackList = pMainForm->tracks()->trackList();
			if (pTrackList)
				pTrackList->updateTrack(this);
		}
		// Update gain and panning curve new subjects...
	#if 0
		qtractorCurveList *pCurveList = m_pPluginList->curveList();
		if (pCurveList) {
			qtractorCurve *pCurve = pCurveList->first();
			while (pCurve) {
				qtractorSubject *pSubject = pCurve->subject();
				if (pSubject) {
					if (pSubject == pMonitor->gainSubject()) {
						pCurve->setSubject(m_pMonitor->gainSubject());
						m_pMonitor->gainSubject()->setCurve(pCurve);
					}
					else
					if (pSubject == pMonitor->panningSubject()) {
						pCurve->setSubject(m_pMonitor->panningSubject());
						m_pMonitor->panningSubject()->setCurve(pCurve);
					}
				}
				pCurve = pCurve->next();
			}
		}
	#else
		// Panning subject ownership transfer...
		qtractorCurve *pCurve = pMonitor->panningSubject()->curve();
		if (pCurve) {
			pCurve->setSubject(m_pMonitor->panningSubject());
			m_pMonitor->panningSubject()->setCurve(pCurve);
		}
		// Gain subject ownership transfer...
		pCurve = pMonitor->gainSubject()->curve();
		if (pCurve) {
			pCurve->setSubject(m_pMonitor->gainSubject());
			m_pMonitor->gainSubject()->setCurve(pCurve);
		}
	#endif
		// That's it...
		delete pMonitor;
	}

	// Set on plug-list latency compensation...
	m_pPluginList->setLatency(m_props.pluginListLatency);

	// Ah, at least make new name feedback...
	updateTrackName();

	// Done.
	return (m_pMonitor != nullptr);
}


// Track close method.
void qtractorTrack::close (void)
{
#if 0// Sure there's no subject automation going on?...
	qtractorSubject::resetQueue();
#endif

	if (m_pMidiVolumeObserver) {
		delete m_pMidiVolumeObserver;
		m_pMidiVolumeObserver = nullptr;
	}

	if (m_pMidiPanningObserver) {
		delete m_pMidiPanningObserver;
		m_pMidiPanningObserver = nullptr;
	}

	if (m_pMidiProgramObserver) {
		delete m_pMidiProgramObserver;
		m_pMidiProgramObserver = nullptr;
	}

#if 0
	if (m_pMonitor) {
		delete m_pMonitor;
		m_pMonitor = nullptr;
	}
#endif

	m_pInputBus  = nullptr;
	m_pOutputBus = nullptr;

	setClipRecord(nullptr);
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


QString qtractorTrack::shortTrackName (void) const
{
	return shortTrackName(m_props.trackName);
}


void qtractorTrack::updateTrackName (void)
{
	const int iMaxLength = 12;
	const QString sEllipsis(3, '.');

	QString sTrackName = shortTrackName();
	if (sTrackName.length() >= iMaxLength + sEllipsis.length())
		sTrackName = sTrackName.left(iMaxLength).trimmed() + sEllipsis;

	m_pMonitorSubject->setName(QObject::tr("%1 Monitor").arg(sTrackName));
	m_pRecordSubject->setName(QObject::tr("%1 Record").arg(sTrackName));
	m_pMuteSubject->setName(QObject::tr("%1 Mute").arg(sTrackName));
	m_pSoloSubject->setName(QObject::tr("%1 Solo").arg(sTrackName));

	if (m_pMonitor) {
		if (m_props.trackType == qtractorTrack::Midi) {
			m_pMonitor->gainSubject()->setName(
				QObject::tr("%1 Volume").arg(sTrackName));
		} else {
			m_pMonitor->gainSubject()->setName(
				QObject::tr("%1 Gain").arg(sTrackName));
		}
		m_pMonitor->panningSubject()->setName(
			QObject::tr("%1 Pan").arg(sTrackName));
	}

	m_pPluginList->setName(sTrackName);
}


QString qtractorTrack::shortTrackName ( const QString& sTrackName )
{
	return QString(sTrackName).remove(QRegularExpression("\n.+")).simplified();
}


// Track icon (filename) accessors.
const QString& qtractorTrack::trackIcon (void) const
{
	return m_props.trackIcon;
}

void qtractorTrack::setTrackIcon ( const QString& sTrackIcon )
{
	if (!QIcon::fromTheme(sTrackIcon).isNull() || QFileInfo(sTrackIcon).exists())
		m_props.trackIcon = sTrackIcon;
	else
		m_props.trackIcon.clear();
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

	// Acquire a new MIDI-tag and setup the plugin-list flags...
	unsigned int iFlags = qtractorPluginList::Track;
	// Get current audio output bus for the plugin list...
	if (m_props.trackType == qtractorTrack::Midi) {
		m_pSession->acquireMidiTag(this);
		iFlags |= qtractorPluginList::Midi;
	}

	// (Re)set plugin-list...
	m_pPluginList->setChannels(0, iFlags);
}


// Record monitoring status accessors.
bool qtractorTrack::isMonitor (void) const
{
	return (m_pMonitorSubject->value() > 0.0f);
}

void qtractorTrack::setMonitor ( bool bMonitor )
{
	m_props.monitor = bMonitor;

	m_pMonitorSubject->setValue(bMonitor ? 1.0f : 0.0f);

	m_pSession->autoDeactivatePlugins();
}


// Record status accessors.
void qtractorTrack::setRecord ( bool bRecord )
{
	const bool bOldRecord = m_props.record; // isRecord();
	if ((bOldRecord && !bRecord) || (!bOldRecord && bRecord))
		m_pSession->setRecordTracks(bRecord);

	m_props.record = bRecord;

	m_pRecordSubject->setValue(bRecord ? 1.0f : 0.0f);

	m_pSession->autoDeactivatePlugins();

	if (m_pSession->isRecording()) {
		unsigned long iClipStart = m_pSession->playHead();
		if (m_pSession->isPunching()) {
			const unsigned long iPunchIn = m_pSession->punchIn();
			if (iClipStart < iPunchIn)
				iClipStart = iPunchIn;
		}
		const unsigned long iFrameTime = m_pSession->frameTimeEx();
		m_pSession->trackRecord(this, bRecord, iClipStart, iFrameTime);
	}
}

bool qtractorTrack::isRecord (void) const
{
	return m_props.record; // (m_pRecordSubject->value() > 0.0f);
}


// Mute status accessors.
void qtractorTrack::setMute ( bool bMute )
{
	if (m_pSession->isPlaying() && bMute)
		m_pSession->trackMute(this, bMute);

	const bool bOldMute = m_props.mute; // isMute();
	if ((bOldMute && !bMute) || (!bOldMute && bMute))
		m_pSession->setMuteTracks(bMute);

	m_props.mute = bMute;

	m_pMuteSubject->setValue(bMute ? 1.0f : 0.0f);

	if (m_pSession->isPlaying() && !bMute)
		m_pSession->trackMute(this, bMute);

	m_pSession->autoDeactivatePlugins();
}

bool qtractorTrack::isMute (void) const
{
	return m_props.mute; // (m_pMuteSubject->value() > 0.0f);
}


// Solo status accessors.
void qtractorTrack::setSolo ( bool bSolo )
{
	if (m_pSession->isPlaying() && bSolo)
		m_pSession->trackSolo(this, bSolo);

	const bool bOldSolo = m_props.solo; // isSolo();
	if ((bOldSolo && !bSolo) || (!bOldSolo && bSolo))
		m_pSession->setSoloTracks(bSolo);

	m_props.solo = bSolo;

	m_pSoloSubject->setValue(bSolo ? 1.0f : 0.0f);

	if (m_pSession->isPlaying() && !bSolo)
		m_pSession->trackSolo(this, bSolo);

	m_pSession->autoDeactivatePlugins();
}

bool qtractorTrack::isSolo (void) const
{
	return m_props.solo; // (m_pSoloSubject->value() > 0.0f);
}


// Track gain (volume) accessor.
void qtractorTrack::setGain ( float fGain )
{
	m_props.gain = fGain;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer) {
			qtractorMixerStrip *pStrip
				= pMixer->trackRack()->findStrip(m_pMonitor);
			if (pStrip && pStrip->meter())
				pStrip->meter()->setGain(fGain);
		}
	}
}

float qtractorTrack::gain (void) const
{
	return (m_pMonitor ? m_pMonitor->gain() : m_props.gain);
}

float qtractorTrack::prevGain (void) const
{
	return (m_pMonitor ? m_pMonitor->prevGain() : 1.0f);
}

// Track stereo-panning accessor.
void qtractorTrack::setPanning ( float fPanning )
{
	m_props.panning = fPanning;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorMixer *pMixer = pMainForm->mixer();
		if (pMixer) {
			qtractorMixerStrip *pStrip
				= pMixer->trackRack()->findStrip(m_pMonitor);
			if (pStrip && pStrip->meter())
				pStrip->meter()->setPanning(fPanning);
		}
	}
}

float qtractorTrack::panning (void) const
{
	return (m_pMonitor ? m_pMonitor->panning() : m_props.panning);
}

float qtractorTrack::prevPanning (void) const
{
	return (m_pMonitor ? m_pMonitor->prevPanning() : 0.0f);
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
void qtractorTrack::setMidiProg ( int iMidiProg )
{
	m_props.midiProg = iMidiProg;
}

int qtractorTrack::midiProg (void) const
{
	return m_props.midiProg;
}


// MIDI drum mode (UI).
void qtractorTrack::setMidiDrums ( bool bMidiDrums )
{
	m_props.midiDrums = bMidiDrums;
}

bool qtractorTrack::isMidiDrums (void) const
{
	return m_props.midiDrums;
}


// MIDI specific: note minimum/maximum range.
void qtractorTrack::setMidiNoteMin ( unsigned char note )
{
	if (m_midiNoteMin > note || m_midiNoteMin == 0)
		m_midiNoteMin = note;
}

unsigned char qtractorTrack::midiNoteMin (void) const
{
	return m_midiNoteMin;
}


void qtractorTrack::setMidiNoteMax ( unsigned char note )
{
	if (m_midiNoteMax < note || m_midiNoteMax == 0)
		m_midiNoteMax = note;
}

unsigned char qtractorTrack::midiNoteMax (void) const
{
	return m_midiNoteMax;
}


// MIDI specific volume controller.
void qtractorTrack::setMidiVolume ( unsigned char vol, bool bUpdate )
{
	if (m_midiVolume == vol)
		return;

	m_midiVolume = vol;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pOutputBus);
	if (pMidiBus == nullptr)
		return;

	if (bUpdate) {
		pMidiBus->setController(this, MIDI_CHANNEL_VOLUME, vol);
		return;
	}

	qtractorMidiManager *pMidiManager = nullptr;
	if (m_pPluginList)
		pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager)
		pMidiManager->setController(midiChannel(), MIDI_CHANNEL_VOLUME, vol);
	if (pMidiBus->pluginList_out()) {
		pMidiManager = pMidiBus->pluginList_out()->midiManager();
		if (pMidiManager)
			pMidiManager->setController(midiChannel(), MIDI_CHANNEL_VOLUME, vol);
	}
}

unsigned char qtractorTrack::midiVolume (void) const
{
	return m_midiVolume;
}


// MIDI specific panning controller.
void qtractorTrack::setMidiPanning ( unsigned char pan, bool bUpdate )
{
	if (m_midiPanning == pan)
		return;

	m_midiPanning = pan;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pOutputBus);
	if (pMidiBus == nullptr)
		return;

	if (bUpdate) {
		pMidiBus->setController(this, MIDI_CHANNEL_PANNING, pan);
		return;
	}

	qtractorMidiManager *pMidiManager = nullptr;
	if (m_pPluginList)
		pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager)
		pMidiManager->setController(midiChannel(), MIDI_CHANNEL_PANNING, pan);
	if (pMidiBus->pluginList_out()) {
		pMidiManager = pMidiBus->pluginList_out()->midiManager();
		if (pMidiManager)
			pMidiManager->setController(midiChannel(), MIDI_CHANNEL_PANNING, pan);
	}
}

unsigned char qtractorTrack::midiPanning (void) const
{
	return m_midiPanning;
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


// Plugin latency compensation accessors.
void qtractorTrack::setPluginListLatency ( bool bPluginListLatency )
{
	m_props.pluginListLatency = bPluginListLatency;
}

bool qtractorTrack::isPluginListLatency (void) const
{
	return m_props.pluginListLatency;
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

	m_iHeightBase = m_iHeight;

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


// Base zoomed view height accessor.
int qtractorTrack::zoomHeightBase (void) const
{
	if (m_pSession)
		return (m_iHeightBase * m_pSession->verticalZoom()) / 100;
	else
		return m_iHeightBase;
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


// Insert a new clip in guaranteed sorted fashion.
void qtractorTrack::addClip ( qtractorClip *pClip )
{
	if (m_pSession == nullptr)
		return;

	if (m_props.trackType == qtractorTrack::Audio)
		m_pSession->files()->addClipItem(qtractorFileList::Audio, pClip);
	else
	if (m_props.trackType == qtractorTrack::Midi) {
		m_pSession->files()->addClipItem(qtractorFileList::Midi, pClip);
		// Special case for initial MIDI tracks...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			if (midiBankSelMethod() < 0)
				setMidiBankSelMethod(pMidiClip->bankSelMethod());
			if (midiBank() < 0)
				setMidiBank(pMidiClip->bank());
			if (midiProg() < 0)
				setMidiProg(pMidiClip->prog());
		}
	}

	// Now do insert the clip in proper place in track...
	insertClip(pClip);
}


void qtractorTrack::addClipEx ( qtractorClip *pClip )
{
	// Preliminary settings...
	pClip->setTrack(this);
	pClip->open();

	addClip(pClip);
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

	pClip->setActive(true);
}


void qtractorTrack::removeClipEx ( qtractorClip *pClip )
{
	// Preliminary settings...
	// pClip->setTrack(nullptr);
	pClip->close();

	removeClip(pClip);
}


void qtractorTrack::removeClip ( qtractorClip *pClip )
{
	if (m_pSession == nullptr)
		return;

	if (m_props.trackType == qtractorTrack::Audio)
		m_pSession->files()->removeClipItem(qtractorFileList::Audio, pClip);
	else
	if (m_props.trackType == qtractorTrack::Midi)
		m_pSession->files()->removeClipItem(qtractorFileList::Midi, pClip);

	pClip->setActive(false);

	m_clips.unlink(pClip);
}


// Current clip on record (capture).
void qtractorTrack::setClipRecord ( qtractorClip *pClipRecord )
{
	if (!m_bClipRecordEx && m_pClipRecord)
		delete m_pClipRecord;

	m_pClipRecord = pClipRecord;

	if (m_pClipRecord == nullptr) {
		m_iClipRecordStart = 0;
		if (m_bClipRecordEx) {
			m_bClipRecordEx = false;
			setRecord(false);
		}
	}
}

qtractorClip *qtractorTrack::clipRecord (void) const
{
	return m_pClipRecord;
}


// Current clip on record absolute start frame (capture).
void qtractorTrack::setClipRecordStart ( unsigned long iClipRecordStart )
{
	m_iClipRecordStart = iClipRecordStart;
}

unsigned long qtractorTrack::clipRecordStart (void) const
{
	return m_iClipRecordStart;
}


unsigned long qtractorTrack::clipRecordEnd ( unsigned long iFrameTime ) const
{
	// HACK: Care of loop-recording + punch-in/out...
	if (m_pSession && m_pSession->isPunching()) {
		const unsigned long iPlayHead = m_pSession->playHead();
		unsigned long iPunchIn = m_pSession->punchIn();
		unsigned long iPunchOut = m_pSession->punchOut();
		if (iFrameTime < iPunchIn)
			return iPunchIn; // Cancelled...
		const unsigned long iLoopStart = m_pSession->loopStart();
		const unsigned long iLoopEnd = m_pSession->loopEnd();
		if (iLoopStart < iLoopEnd
			&& m_pSession->loopRecordingMode() > 0
			&& iLoopEnd > iPlayHead && iLoopEnd < iFrameTime) {
			if (iPunchIn < iLoopStart)
				iPunchIn = iLoopStart;
			if (iPunchOut > iLoopEnd)
				iPunchOut = iLoopEnd;
			const unsigned long iLoopLength
				= iLoopEnd - iLoopStart;
			const unsigned int iLoopCount
				= (iFrameTime - iPunchIn) / iLoopLength;
			iPunchOut += iLoopCount * iLoopLength;
		}
		// Make sure it's really about to punch-out...
		if (iFrameTime > iPunchOut)
			return iPunchOut;
	}

	unsigned long iClipRecordEnd = iFrameTime;
	if (iClipRecordEnd  > m_iClipRecordStart)
		iClipRecordEnd -= m_iClipRecordStart;
	if (m_pClipRecord)
		iClipRecordEnd += m_pClipRecord->clipStart();

	return iClipRecordEnd;
}


// Set current clip on exclusive recording.
void qtractorTrack::setClipRecordEx ( bool bClipRecordEx )
{
	m_bClipRecordEx = bClipRecordEx;
}

bool qtractorTrack::isClipRecordEx (void) const
{
	return m_bClipRecordEx;
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


// Default track color saturation factor [0..500].
int qtractorTrack::g_iTrackColorSaturation = 100;

void qtractorTrack::setTrackColorSaturation ( int iTrackColorSaturation )
{
	g_iTrackColorSaturation = iTrackColorSaturation;
}

int qtractorTrack::trackColorSaturation (void)
{
	return g_iTrackColorSaturation;
}


// Generate a default track color.
QColor qtractorTrack::trackColor ( int iTrack )
{
	const int c[3] = { 0xff, 0xcc, 0x99 };

	QColor color(
		c[iTrack % 3],
		c[(iTrack / 3) % 3],
		c[(iTrack / 9) % 3]);

	int h, s, v;
	color.getHsv(&h, &s, &v);
	s += (s * (g_iTrackColorSaturation - 100)) / 100;
	if (s < 0) s = 0;
	else
	if (s > 255) s = 255;
	color.setHsv(h, s, v);

	return color;
}


// Alternate properties accessor.
void qtractorTrack::setProperties ( const qtractorTrack::Properties& props )
{
	m_props = props;
}


qtractorTrack::Properties& qtractorTrack::properties (void)
{
	return m_props;
}


// Reset state properties (as needed on copy/duplicate)
void qtractorTrack::resetProperties (void)
{
	const bool bMonitor = m_props.monitor;
	const bool bRecord = m_props.record;
	const bool bMute = m_props.mute;
	const bool bSolo = m_props.solo;

	m_props.monitor = false;
	m_props.record = false;;
	m_props.mute = false;
	m_props.solo = false;

	setMonitor(bMonitor);
	setRecord(bRecord);
	setMute(bMute);
	setSolo(bSolo);
}


// Track special process cycle executive.
void qtractorTrack::process ( qtractorClip *pClip,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Audio-buffers needs some preparation...
	const unsigned int nframes = iFrameEnd - iFrameStart;
	qtractorAudioMonitor *pAudioMonitor = nullptr;
	qtractorAudioBus *pOutputBus = nullptr;
	if (m_props.trackType == qtractorTrack::Audio) {
		pAudioMonitor = static_cast<qtractorAudioMonitor *> (m_pMonitor);
		pOutputBus = static_cast<qtractorAudioBus *> (m_pOutputBus);
		// Prepare this track buffer...
		if (pOutputBus) {
			qtractorAudioBus *pInputBus = (m_pSession->isTrackMonitor(this)
				? static_cast<qtractorAudioBus *> (m_pInputBus) : nullptr);
			pOutputBus->buffer_prepare(nframes, pInputBus);
		}
	}

	// Playback...
	if (!isMute() && (!m_pSession->soloTracks() || isSolo())) {
		const unsigned long iLatency = m_pPluginList->latency();
		const unsigned long iFrameStart2 = iFrameStart + iLatency;
		const unsigned long iFrameEnd2 = iFrameEnd + iLatency;
		// Now, for every clip...
		while (pClip && pClip->clipStart() < iFrameEnd2) {
			if (!pClip->isClipMute() &&
				iFrameStart2 < pClip->clipStart() + pClip->clipLength())
				pClip->process(iFrameStart2, iFrameEnd2);
			pClip = pClip->next();
		}
	}

	// Audio buffers needs monitoring and commitment...
	if (pAudioMonitor && pOutputBus) {
		// Plugin chain post-processing...
		m_pPluginList->process(pOutputBus->buffer(), nframes);
		// Monitor passthru...
		pAudioMonitor->process(pOutputBus->buffer(), nframes);
		// Actually render it...
		pOutputBus->buffer_commit(nframes);
	}
}


// Freewheeling process cycle executive (needed for export).
void qtractorTrack::process_export ( qtractorClip *pClip,
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Track automation processing...
	process_curve(iFrameStart);

	// Audio-buffers needs some preparation...
	const unsigned int nframes = iFrameEnd - iFrameStart;
	qtractorAudioMonitor *pAudioMonitor = nullptr;
	qtractorAudioBus *pOutputBus = nullptr;
	if (m_props.trackType == qtractorTrack::Audio) {
		pAudioMonitor = static_cast<qtractorAudioMonitor *> (m_pMonitor);
		pOutputBus = static_cast<qtractorAudioBus *> (m_pOutputBus);
		if (pOutputBus)
			pOutputBus->buffer_prepare(nframes);
	}

	// Playback...
	if (!isMute() && (!m_pSession->soloTracks() || isSolo())) {
		const unsigned long iLatency = m_pPluginList->latency();
		const unsigned long iFrameStart2 = iFrameStart + iLatency;
		const unsigned long iFrameEnd2 = iFrameEnd + iLatency;
		// Now, for every clip...
		while (pClip && pClip->clipStart() < iFrameEnd2) {
			if (iFrameStart2 < pClip->clipStart() + pClip->clipLength())
				pClip->process_export(iFrameStart2, iFrameEnd2);
			pClip = pClip->next();
		}
	}

	// Audio buffers needs monitoring and commitment...
	if (pAudioMonitor && pOutputBus) {
		// Plugin chain post-processing...
		m_pPluginList->process(pOutputBus->buffer(), nframes);
		// Monitor passthru...
		pAudioMonitor->process(pOutputBus->buffer(), nframes);
		// Actually render it...
		pOutputBus->buffer_commit(nframes);
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
		// Punch-in/out recording...
		if (m_pSession->isPunching()
			&& m_pSession->frameTimeEx() < iFrameEnd) {
			const unsigned long iPunchIn = m_pSession->punchIn();
			// Punch-in (likely...)
			if (iPunchIn < iFrameEnd) {
				unsigned int offset = 0;
				if (iPunchIn >= iFrameStart) {
					offset += (iPunchIn - iFrameStart);
					nframes = (iFrameEnd - iPunchIn);
				}
				// Punch-in recording...
				pAudioClip->write(
					pInputBus->in(), nframes, pInputBus->channels(), offset);
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


// Track special process automation executive.
void qtractorTrack::process_curve ( unsigned long iFrame )
{
	qtractorCurveList *pCurveList = curveList();
	if (pCurveList && pCurveList->isProcess())
		pCurveList->process(iFrame);
}


// Track paint method.
void qtractorTrack::drawTrack ( QPainter *pPainter, const QRect& trackRect,
	unsigned long iTrackStart, unsigned long iTrackEnd, qtractorClip *pClip )
{
	const int y = trackRect.y();
	const int h = trackRect.height();

	if (pClip == nullptr)
		pClip = m_clips.first();

	// Track/clip background...
	QColor bg = background();
	const QPen pen(bg.darker());
	bg.setAlpha(192); // translucency...
#ifdef CONFIG_GRADIENT
	QLinearGradient grad(0, y, 0, y + h);
	grad.setColorAt(0.4, bg);
	grad.setColorAt(1.0, bg.darker(130));
	const QBrush brush(grad);
#else
	const QBrush brush(bg);
#endif

	qtractorClip *pClipRecordEx = (m_bClipRecordEx ? m_pClipRecord : nullptr);
	const int x0 = m_pSession->pixelFromFrame(iTrackStart);
	while (pClip) {
		unsigned long iClipStart = pClip->clipStart();
		if (iClipStart > iTrackEnd)
			break;
		unsigned long iClipEnd = iClipStart + pClip->clipLength();
		if (iClipStart < iTrackEnd && iClipEnd > iTrackStart) {
			unsigned long iClipOffset = 0; // pClip->clipOffset();
			if (iClipStart < iTrackStart) {
				iClipOffset += (iTrackStart - iClipStart);
				iClipStart = iTrackStart;
			}
			if (iClipEnd > iTrackEnd) {
				iClipEnd = iTrackEnd;
			}
			const int x1 = m_pSession->pixelFromFrame(iClipStart) - x0;
			const int x2 = m_pSession->pixelFromFrame(iClipEnd) - x0;
			pPainter->setPen(pen);
			pPainter->setBrush(brush);
			// Draw the clip...
			const QRect clipRect(x1, y, x2 - x1, h);
			pClip->drawClip(pPainter, clipRect, iClipOffset);
			if (pClip == pClipRecordEx)
				pPainter->fillRect(clipRect, QColor(255, 0, 0, 60));
			else
			if (pClip->isClipMute())
				pPainter->fillRect(clipRect, QColor(0, 0, 0, 60));
		}
		pClip = pClip->next();
	}

	if (m_props.mute || (!m_props.solo && m_pSession->soloTracks()))
		pPainter->fillRect(trackRect, QColor(0, 0, 0, 60));
}


// Track loop point setler.
void qtractorTrack::setLoop (
	unsigned long iLoopStart, unsigned long iLoopEnd )
{
	qtractorClip *pClip = m_clips.first();
	while (pClip) {
		// Convert loop-points from session to clip...
		const unsigned long iClipStart = pClip->clipStart();
		const unsigned long iClipEnd   = iClipStart + pClip->clipLength();
		if (iLoopStart < iClipEnd && iLoopEnd > iClipStart) {
			// Set clip inner-loop...
			pClip->setLoop(
				(iLoopStart > iClipStart ? iLoopStart - iClipStart : 0),
				(iLoopEnd < iClipEnd ? iLoopEnd : iClipEnd) - iClipStart);
		} else {
			// Clear/reaet clip-loop...
			pClip->setLoop(0, 0);
		}
		pClip = pClip->next();
	}
}


// MIDI track instrument patching.
void qtractorTrack::setMidiPatch ( qtractorInstrumentList *pInstruments )
{
	const int iProg = midiProg();
	if (iProg < 0)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pOutputBus);
	if (pMidiBus == nullptr)
		return;

	const unsigned short iChannel = midiChannel();
	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
	const int iBank = midiBank();

	int iBankSelMethod = midiBankSelMethod();
	if (iBankSelMethod < 0) {
		const QString& sInstrumentName = patch.instrumentName;
		if (!sInstrumentName.isEmpty()
			&& pInstruments->contains(sInstrumentName)) {
			const qtractorInstrument& instr
				= pInstruments->value(sInstrumentName);
			iBankSelMethod = instr.bankSelMethod();
		}
		else if (iBank >= 0)
			iBankSelMethod = 0;
	}

	pMidiBus->setPatch(iChannel, patch.instrumentName,
		iBankSelMethod, iBank, iProg, this);
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


// Audio buffer ring-cache (playlist) methods.
qtractorAudioBufferThread *qtractorTrack::syncThread (void)
{
	if (m_pSyncThread == nullptr) {
		m_pSyncThread = new qtractorAudioBufferThread();
		m_pSyncThread->start(QThread::HighPriority);
	} else {
		m_pSyncThread->checkSyncSize(m_clips.count());
	}

	return m_pSyncThread;
}


// Track state (monitor record, mute, solo) button setup.
qtractorSubject *qtractorTrack::monitorSubject (void) const
{
	return m_pMonitorSubject;
}

qtractorSubject *qtractorTrack::recordSubject (void) const
{
	return m_pRecordSubject;
}

qtractorSubject *qtractorTrack::muteSubject (void) const
{
	return m_pMuteSubject;
}

qtractorSubject *qtractorTrack::soloSubject (void) const
{
	return m_pSoloSubject;
}


qtractorMidiControlObserver *qtractorTrack::monitorObserver (void) const
{
	return m_pMonitorObserver;
}

qtractorMidiControlObserver *qtractorTrack::recordObserver (void) const
{
	return static_cast<qtractorMidiControlObserver *> (m_pRecordObserver);
}

qtractorMidiControlObserver *qtractorTrack::muteObserver (void) const
{
	return static_cast<qtractorMidiControlObserver *> (m_pMuteObserver);
}

qtractorMidiControlObserver *qtractorTrack::soloObserver (void) const
{
	return static_cast<qtractorMidiControlObserver *> (m_pSoloObserver);
}


// Track state (monitor) notifier (proto-slot).
void qtractorTrack::monitorChangeNotify ( bool bOn )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTrack[%p]::monitorChangeNotify(%d)", this, int(bOn));
#endif

	// Put it in the form of an undoable command...
	if (m_pSession)
		m_pSession->execute(
			new qtractorTrackMonitorCommand(this, bOn));
}


// Track state (record, mute, solo) notifier (proto-slot).
void qtractorTrack::stateChangeNotify ( ToolType toolType, bool bOn )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTrack[%p]::stateChangeNotify(%d, %d)",
		this, int(toolType), int(bOn));
#endif

	// Put it in the form of an undoable command...
	if (m_pSession)
		m_pSession->execute(
			new qtractorTrackStateCommand(this, toolType, bOn));
}


// Document element methods.
bool qtractorTrack::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	if (m_pSession == nullptr)
		return false;

	qtractorTrack::setTrackName(pElement->attribute("name"));
	qtractorTrack::setTrackType(
		qtractorTrack::trackTypeFromText(pElement->attribute("type")));

	// Reset take(record) descriptor/id registry.
	clearTakeInfo();

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
				else if (eProp.tagName() == "plugin-list-latency")
					qtractorTrack::setPluginListLatency(
						qtractorDocument::boolFromText(eProp.text()));
				else if (eProp.tagName() == "midi-omni")
					qtractorTrack::setMidiOmni(
						qtractorDocument::boolFromText(eProp.text()));
				else if (eProp.tagName() == "midi-channel")
					qtractorTrack::setMidiChannel(eProp.text().toUShort());
				else if (eProp.tagName() == "midi-bank-sel-method")
					qtractorTrack::setMidiBankSelMethod(eProp.text().toInt());
				else if (eProp.tagName() == "midi-bank")
					qtractorTrack::setMidiBank(eProp.text().toInt());
				else if (eProp.tagName() == "midi-program")
					qtractorTrack::setMidiProg(eProp.text().toInt());
				else if (eProp.tagName() == "midi-drums")
					qtractorTrack::setMidiDrums(
						qtractorDocument::boolFromText(eProp.text()));
				else if (eProp.tagName() == "icon")
					qtractorTrack::setTrackIcon(eProp.text());
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
					qtractorTrack::setMute(
						qtractorDocument::boolFromText(eState.text()));
				else if (eState.tagName() == "solo")
					qtractorTrack::setSolo(
						qtractorDocument::boolFromText(eState.text()));
				else if (eState.tagName() == "record")
					qtractorTrack::setRecord(
						qtractorDocument::boolFromText(eState.text()));
				else if (eState.tagName() == "monitor")
					qtractorTrack::setMonitor(
						qtractorDocument::boolFromText(eState.text()));
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
				#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
					qtractorTrack::setBackground(QColor::fromString(eView.text()));
				#else
					qtractorTrack::setBackground(QColor(eView.text()));
				#endif
				} else if (eView.tagName() == "foreground-color") {
				#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
					qtractorTrack::setForeground(QColor::fromString(eView.text()));
				#else
					qtractorTrack::setForeground(QColor(eView.text()));
				#endif
				}
			}
		}
		else
		if (eChild.tagName() == "controllers") {
			// Load track controllers...
			qtractorTrack::loadControllers(&eChild);
		}
		else
		if (eChild.tagName() == "curve-file") {
			// Load track automation curves...
			qtractorTrack::loadCurveFile(&eChild, m_pCurveFile);
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
					qtractorClip *pClip = nullptr;
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
					if (pClip == nullptr)
						return false;
					if (!pClip->loadElement(pDocument, &eClip))
						return false;
					qtractorTrack::addClipEx(pClip);
				}
			}
		}
		else
		// Load plugins...
		if (eChild.tagName() == "plugins")
			m_pPluginList->loadElement(pDocument, &eChild);
	}

	// Reset take(record) descriptor/id registry.
	clearTakeInfo();

	return true;
}


bool qtractorTrack::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	pElement->setAttribute("name", qtractorTrack::trackName());
	pElement->setAttribute("type",
		qtractorTrack::textFromTrackType(qtractorTrack::trackType()));

	// Reset take(record) descriptor/id registry.
	clearTakeInfo();

	// Save track properties...
	QDomElement eProps = pDocument->document()->createElement("properties");
	const QString& sTrackIcon = qtractorTrack::trackIcon();
	if (!sTrackIcon.isEmpty())
		pDocument->saveTextElement("icon", sTrackIcon, &eProps);
	pDocument->saveTextElement("input-bus",
		qtractorTrack::inputBusName(), &eProps);
	pDocument->saveTextElement("output-bus",
		qtractorTrack::outputBusName(), &eProps);
	const bool bPluginListLatency = qtractorTrack::isPluginListLatency();
	if (bPluginListLatency) pDocument->saveTextElement("plugin-list-latency",
		qtractorDocument::textFromBool(bPluginListLatency), &eProps);
	if (qtractorTrack::trackType() == qtractorTrack::Midi) {
		pDocument->saveTextElement("midi-omni",
			qtractorDocument::textFromBool(qtractorTrack::isMidiOmni()), &eProps);
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
		if (qtractorTrack::midiProg() >= 0) {
			pDocument->saveTextElement("midi-program",
				QString::number(qtractorTrack::midiProg()), &eProps);
		}
		pDocument->saveTextElement("midi-drums",
			qtractorDocument::textFromBool(qtractorTrack::isMidiDrums()), &eProps);
	}
	pElement->appendChild(eProps);

	// Save track state...
	QDomElement eState = pDocument->document()->createElement("state");
	pDocument->saveTextElement("mute",
		qtractorDocument::textFromBool(qtractorTrack::isMute()), &eState);
	pDocument->saveTextElement("solo",
		qtractorDocument::textFromBool(qtractorTrack::isSolo()), &eState);
	pDocument->saveTextElement("record",
		qtractorDocument::textFromBool(qtractorTrack::isRecord()
			&& !qtractorTrack::isClipRecordEx()), &eState);
	pDocument->saveTextElement("monitor",
		qtractorDocument::textFromBool(qtractorTrack::isMonitor()), &eState);
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

	// Save track controllers...
	QDomElement eControllers
		= pDocument->document()->createElement("controllers");
	qtractorTrack::saveControllers(pDocument, &eControllers);
	pElement->appendChild(eControllers);

	// Save track automation...
	qtractorCurveList *pCurveList = qtractorTrack::curveList();
	if (pCurveList && !pCurveList->isEmpty()) {
		qtractorCurveFile cfile(pCurveList);
		QDomElement eCurveFile
			= pDocument->document()->createElement("curve-file");
		qtractorTrack::saveCurveFile(pDocument, &eCurveFile, &cfile);
		pElement->appendChild(eCurveFile);
	}

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

	// Reset take(record) descriptor/id registry.
	clearTakeInfo();

	return true;
}


// Track type textual helper methods.
qtractorTrack::TrackType qtractorTrack::trackTypeFromText (
	const QString& sText )
{
	TrackType trackType = None;
	if (sText == "audio")
		trackType = Audio;
	else if (sText == "midi")
		trackType = Midi;
	return trackType;
}

QString qtractorTrack::textFromTrackType ( TrackType trackType )
{
	QString sText;
	switch (trackType) {
	case Audio:
		sText = "audio";
		break;
	case Midi:
		sText = "midi";
		break;
	case None:
	default:
		sText = "none";
		break;
	}
	return sText;
}


// Load track state (record, mute, solo) controllers (MIDI).
void qtractorTrack::loadControllers ( QDomElement *pElement )
{
	qtractorMidiControl::loadControllers(pElement, m_controllers);
}


// Save track state (record, mute, solo) controllers (MIDI).
void qtractorTrack::saveControllers (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == nullptr)
		return;

	if (m_pMonitor == nullptr)
		return;

	qtractorMidiControl::Controllers controllers;

	if (pMidiControl->isMidiObserverMapped(m_pMonitorObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = m_pMonitorObserver->subject()->name();
		pController->index = 0; // 0=MonitorObserver
		pController->ctype = m_pMonitorObserver->type();
		pController->channel = m_pMonitorObserver->channel();
		pController->param = m_pMonitorObserver->param();
		pController->logarithmic = m_pMonitorObserver->isLogarithmic();
		pController->feedback = m_pMonitorObserver->isFeedback();
		pController->invert = m_pMonitorObserver->isInvert();
		pController->hook = m_pMonitorObserver->isHook();
		pController->latch = m_pMonitorObserver->isLatch();
		controllers.append(pController);
	}

	qtractorMidiControlObserver *pPanObserver = m_pMonitor->panningObserver();
	if (pMidiControl->isMidiObserverMapped(pPanObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = m_pMonitorObserver->subject()->name();
		pController->index = 1; // 1=PanObserver
		pController->ctype = pPanObserver->type();
		pController->channel = pPanObserver->channel();
		pController->param = pPanObserver->param();
		pController->logarithmic = pPanObserver->isLogarithmic();
		pController->feedback = pPanObserver->isFeedback();
		pController->invert = pPanObserver->isInvert();
		pController->hook = pPanObserver->isHook();
		pController->latch = pPanObserver->isLatch();
		controllers.append(pController);
	}

	qtractorMidiControlObserver *pGainObserver = m_pMonitor->gainObserver();
	if (pMidiControl->isMidiObserverMapped(pGainObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = pGainObserver->subject()->name();
		pController->index = 2; // 2=GainObserver
		pController->ctype = pGainObserver->type();
		pController->channel = pGainObserver->channel();
		pController->param = pGainObserver->param();
		pController->logarithmic = pGainObserver->isLogarithmic();
		pController->feedback = pGainObserver->isFeedback();
		pController->invert = pGainObserver->isInvert();
		pController->hook = pGainObserver->isHook();
		pController->latch = pGainObserver->isLatch();
		controllers.append(pController);
	}

	if (pMidiControl->isMidiObserverMapped(m_pRecordObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = m_pRecordObserver->subject()->name();
		pController->index = 3; // 3=RecordObserver
		pController->ctype = m_pRecordObserver->type();
		pController->channel = m_pRecordObserver->channel();
		pController->param = m_pRecordObserver->param();
		pController->logarithmic = m_pRecordObserver->isLogarithmic();
		pController->feedback = m_pRecordObserver->isFeedback();
		pController->invert = m_pRecordObserver->isInvert();
		pController->hook = m_pRecordObserver->isHook();
		pController->latch = m_pRecordObserver->isLatch();
		controllers.append(pController);
	}

	if (pMidiControl->isMidiObserverMapped(m_pMuteObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = m_pMuteObserver->subject()->name();
		pController->index = 4; // 4=MuteObserver
		pController->ctype = m_pMuteObserver->type();
		pController->channel = m_pMuteObserver->channel();
		pController->param = m_pMuteObserver->param();
		pController->logarithmic = m_pMuteObserver->isLogarithmic();
		pController->feedback = m_pMuteObserver->isFeedback();
		pController->invert = m_pMuteObserver->isInvert();
		pController->hook = m_pMuteObserver->isHook();
		pController->latch = m_pMuteObserver->isLatch();
		controllers.append(pController);
	}

	if (pMidiControl->isMidiObserverMapped(m_pSoloObserver)) {
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = m_pSoloObserver->subject()->name();
		pController->index = 5; // 5=SoloObserver
		pController->ctype = m_pSoloObserver->type();
		pController->channel = m_pSoloObserver->channel();
		pController->param = m_pSoloObserver->param();
		pController->logarithmic = m_pSoloObserver->isLogarithmic();
		pController->feedback = m_pSoloObserver->isFeedback();
		pController->invert = m_pSoloObserver->isInvert();
		pController->hook = m_pSoloObserver->isHook();
		pController->latch = m_pSoloObserver->isLatch();
		controllers.append(pController);
	}

	qtractorMidiControl::saveControllers(pDocument, pElement, controllers);

	qDeleteAll(controllers);
	controllers.clear();
}


// Map track state (monitor, gain, pan, record, mute, solo) controllers (MIDI).
void qtractorTrack::mapControllers (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == nullptr)
		return;

	if (m_pMonitor == nullptr)
		return;

	QListIterator<qtractorMidiControl::Controller *> iter(m_controllers);
	while (iter.hasNext()) {
		qtractorMidiControl::Controller *pController = iter.next();
		qtractorMidiControlObserver *pObserver = nullptr;
		switch (pController->index) {
		case 0: // 0=MonitorObserver
			pObserver = monitorObserver();
			break;
		case 1: // 1=PanObserver
			pObserver = m_pMonitor->panningObserver();
			break;
		case 2: // 2=GainObserver
			pObserver = m_pMonitor->gainObserver();
			break;
		case 3: // 3=RecordObserver
			pObserver = recordObserver();
			break;
		case 4: // 4=MuteObserver
			pObserver = muteObserver();
			break;
		case 5: // 5=SoloObserver
			pObserver = soloObserver();
			break;
		}
		if (pObserver) {
			pObserver->setType(pController->ctype);
			pObserver->setChannel(pController->channel);
			pObserver->setParam(pController->param);
			pObserver->setLogarithmic(pController->logarithmic);
			pObserver->setFeedback(pController->feedback);
			pObserver->setInvert(pController->invert);
			pObserver->setHook(pController->hook);
			pObserver->setLatch(pController->latch);
			pMidiControl->mapMidiObserver(pObserver);
		}
	}

	qDeleteAll(m_controllers);
	m_controllers.clear();
}


// Track automation curve list accessor.
qtractorCurveList *qtractorTrack::curveList (void) const
{
	return (m_pPluginList ? m_pPluginList->curveList() : nullptr);
}


// Track automation curve serializer accessor.
qtractorCurveFile *qtractorTrack::curveFile (void) const
{
	return m_pCurveFile;
}


// Track automation current curve accessors.
void qtractorTrack::setCurrentCurve ( qtractorCurve *pCurrentCurve )
{
	qtractorCurveList *pCurveList = curveList();
	if (pCurveList)
		pCurveList->setCurrentCurve(pCurrentCurve);
}


qtractorCurve *qtractorTrack::currentCurve (void) const
{
	qtractorCurveList *pCurveList = curveList();
	return (pCurveList ? pCurveList->currentCurve() : nullptr);
}


// Load track automation curves (monitor, gain, pan, record, mute, solo).
void qtractorTrack::loadCurveFile (
	QDomElement *pElement, qtractorCurveFile *pCurveFile )
{
	if (pCurveFile) pCurveFile->load(pElement);
}


// Save track automation curves (monitor, gain, pan, record, mute, solo).
void qtractorTrack::saveCurveFile ( qtractorDocument *pDocument,
	QDomElement *pElement, qtractorCurveFile *pCurveFile ) const
{
	if (m_pMonitor == nullptr)
		return;

	if (pCurveFile == nullptr)
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	pCurveFile->clear();
	pCurveFile->setBaseDir(pSession->sessionDir());

	qtractorCurve *pCurve;

	pCurve = monitorSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 0;	// 0=MonitorSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 80;	// 80=General Purpose Button 1 (on/off)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	pCurve = m_pMonitor->panningSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 1;	// 1=PanningSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 10;	// 10=Pan Position (coarse)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	pCurve = m_pMonitor->gainSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 2; // 2=GainSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 7;	// 7=Volume (coarse)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	pCurve = recordSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 3;	// 3=RecordSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 81;	// 81=General Purpose Button 2 (on/off)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	pCurve = muteSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 4;	// 4=MuteSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 82;	// 82=General Purpose Button 3 (on/off)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	pCurve = soloSubject()->curve();
	if (pCurve) {
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = 5;	// 5=SoloSubject
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		pCurveItem->param = 83;	// 83=General Purpose Button 4 (on/off)
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	if (pCurveFile->isEmpty())
		return;

	const QString sBaseName(shortTrackName() + "_curve");
	const bool bTemporary = pDocument->isTemporary();
	const QString& sFilename
		= pSession->createFilePath(sBaseName, "mid", !bTemporary);
	pSession->files()->addFileItem(qtractorFileList::Midi, sFilename, bTemporary);
	pCurveFile->setFilename(sFilename);

	pCurveFile->save(pDocument, pElement, pSession->timeScale());
}


// Apply track automation curves (monitor, gain, pan, record, mute, solo).
void qtractorTrack::applyCurveFile ( qtractorCurveFile *pCurveFile ) const
{
	if (m_pMonitor == nullptr)
		return;

	if (pCurveFile == nullptr)
		return;
	if (pCurveFile->items().isEmpty())
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	pCurveFile->setBaseDir(pSession->sessionDir());
	
	QListIterator<qtractorCurveFile::Item *> iter(pCurveFile->items());
	while (iter.hasNext()) {
		qtractorCurveFile::Item *pCurveItem = iter.next();
		switch (pCurveItem->index) {
		case 0: // 0=MonitorSubject
			pCurveItem->subject = monitorSubject();
			break;
		case 1: // 1=PanSubject
			pCurveItem->subject = m_pMonitor->panningSubject();
			break;
		case 2: // 2=GainSubject
			pCurveItem->subject = m_pMonitor->gainSubject();
			break;
		case 3: // 3=RecordSubject
			pCurveItem->subject = recordSubject();
			break;
		case 4: // 4=MuteSubject
			pCurveItem->subject = muteSubject();
			break;
		case 5: // 5=SoloSubject
			pCurveItem->subject = soloSubject();
			break;
		}
	}

	pCurveFile->apply(pSession->timeScale());
}


// Update tracks/list-view.
void qtractorTrack::updateTrack (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	pTracks->updateTrack(this);
}


void qtractorTrack::updateMidiTrack (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == nullptr)
		return;

	pTracks->updateMidiTrack(this);
}


void qtractorTrack::updateMidiClips (void)
{
	const int iMidiBankSelMethod = qtractorTrack::midiBankSelMethod();
	const int iMidiBank = qtractorTrack::midiBank();
	const int iMidiProg = qtractorTrack::midiProg();

	for (qtractorClip *pClip = qtractorTrack::clips().first();
			pClip; pClip = pClip->next()) {
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			// Check for MIDI track's bank/program changes...
			bool bDirty = pMidiClip->isDirty();
			qtractorMidiSequence *pSeq = pMidiClip->sequence();
			if (pSeq && (
				pSeq->bankSelMethod() != iMidiBankSelMethod ||
				pSeq->bank() != iMidiBank ||
				pSeq->prog() != iMidiProg)) {
				pSeq->setBankSelMethod(iMidiBankSelMethod);
				pSeq->setBank(iMidiBank);
				pSeq->setProg(iMidiProg);
				bDirty = true;
			}
			// Are any dirty changes pending commit?
			if (bDirty) {
				const QString& sFilename
					= pMidiClip->createFilePathRevision();
				pMidiClip->saveCopyFile(sFilename, true);
			}
			// Re-open the MIDI clip anyway...
			//pMidiClip->open();
		}
	}
}


// Update all plugin forms, if visible.
void qtractorTrack::refreshPluginForms (void)
{
	if (m_pPluginList == nullptr)
		return;

	qtractorPlugin *pPlugin = m_pPluginList->first();
	while (pPlugin) {
		pPlugin->refreshForm();
		pPlugin = pPlugin->next();
	}
}


// end of qtractorTrack.cpp
