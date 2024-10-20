// qtractorClipCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorClipCommand.h"
#include "qtractorTrackCommand.h"

#include "qtractorMainForm.h"

#include "qtractorSession.h"
#include "qtractorAudioClip.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"
#include "qtractorTracks.h"
#include "qtractorFiles.h"

#include "qtractorMidiEditCommand.h"
#include "qtractorTimeScaleCommand.h"
#include "qtractorSessionCommand.h"
#include "qtractorCurveCommand.h"


//----------------------------------------------------------------------
// class qtractorClipCommand - declaration.
//

// Constructor.
qtractorClipCommand::qtractorClipCommand ( const QString& sName )
	: qtractorCommand(sName)
{
}

// Destructor.
qtractorClipCommand::~qtractorClipCommand (void)
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pItem->takeInfo)
			pItem->takeInfo->releaseRef();
		if (pItem->editCommand)
			delete pItem->editCommand;
		if (pItem->autoDelete)
			delete pItem->clip;
	}

	qDeleteAll(m_items);
	m_items.clear();

	qDeleteAll(m_trackCommands);
	m_trackCommands.clear();

	m_clips.clear();
}


// Primitive command methods.
void qtractorClipCommand::addClip ( qtractorClip *pClip,
	qtractorTrack *pTrack )
{
	pClip->setTrack(pTrack);
	pClip->open();

	m_items.append(new Item(AddClip, pClip, pTrack));

//	setClearSelectReset(true);
}


void qtractorClipCommand::removeClip ( qtractorClip *pClip )
{
	m_items.append(new Item(RemoveClip, pClip, pClip->track()));

//	setClearSelectReset(true);
}


// Edit command methods.
void qtractorClipCommand::fileClip ( qtractorClip *pClip,
	const QString& sFilename, unsigned short iTrackChannel )
{
	Item *pItem = new Item(FileClip, pClip, pClip->track());
	pItem->filename = sFilename;
	pItem->trackChannel = iTrackChannel;
	m_items.append(pItem);

	if (isAudioClip(pClip))
		reopenClip(pClip, true);
}


void qtractorClipCommand::renameClip (
	qtractorClip *pClip, const QString& sClipName )
{
	Item *pItem = new Item(RenameClip, pClip, pClip->track());
	pItem->clipName = sClipName;
	m_items.append(pItem);
}


void qtractorClipCommand::moveClip ( qtractorClip *pClip,
	qtractorTrack *pTrack, unsigned long iClipStart,
	unsigned long iClipOffset, unsigned long iClipLength )
{
	Item *pItem = new Item(MoveClip, pClip, pTrack);

	// HACK: convert/override MIDI clip-offset times
	// across potential tempo/time-sig changes...
	qtractorSession *pSession = pTrack->session();
	if (pSession && pTrack->trackType() == qtractorTrack::Midi) {
		const unsigned long iOldClipStart = pClip->clipStart();
		const unsigned long iClipStartTime
			= pSession->tickFromFrame(iClipStart);
		const unsigned long iClipOffsetTime
			= pSession->tickFromFrameRange(
				iOldClipStart, iOldClipStart + iClipOffset, true);
		const unsigned long iClipLengthTime
			= pSession->tickFromFrameRange(
				iOldClipStart, iOldClipStart + iClipLength);
		iClipOffset = pSession->frameFromTickRange(
			iClipStartTime, iClipStartTime + iClipOffsetTime, true);
		iClipLength = pSession->frameFromTickRange(
			iClipStartTime, iClipStartTime + iClipLengthTime);
	}

	pItem->clipStart  = iClipStart;
	pItem->clipOffset = iClipOffset;
	pItem->clipLength = iClipLength;

	if (iClipOffset == pClip->clipOffset())
		pItem->fadeInLength = pClip->fadeInLength();
	if (iClipOffset + iClipLength == pClip->clipOffset() + pClip->clipLength())
		pItem->fadeOutLength = pClip->fadeOutLength();
	m_items.append(pItem);

	// ATTN: when moving across tracks:
	// reset all take(record) descriptors...
	if (pTrack != pClip->track()) {
		qtractorClip::TakeInfo *pTakeInfo = pClip->takeInfo();
		if (pTakeInfo) {
			pClip = pTakeInfo->clipPart(qtractorClip::TakeInfo::ClipHead);
			if (pClip)
				takeInfoClip(pClip, nullptr);
			pClip = pTakeInfo->clipPart(qtractorClip::TakeInfo::ClipTake);
			if (pClip)
				takeInfoClip(pClip, nullptr);
		}
	}

//	setClearSelect(true);
}


void qtractorClipCommand::resizeClip ( qtractorClip *pClip,
	unsigned long iClipStart, unsigned long iClipOffset,
	unsigned long iClipLength, float fTimeStretch, float fPitchShift )
{
	Item *pItem = new Item(ResizeClip, pClip, pClip->track());

	// FIXME: convert/override MIDI clip-offset times
	// across potential tempo/time-sig changes...
	qtractorTrack *pTrack = pClip->track();
	qtractorSession *pSession = pTrack->session();
	if (pSession && pTrack->trackType() == qtractorTrack::Midi) {
		const unsigned long iOldClipStart = pClip->clipStart();
		const unsigned long iClipOffsetTime
			= pSession->tickFromFrameRange(
				iOldClipStart, iOldClipStart + iClipOffset, true);
		const unsigned long iClipStartTime
			= pSession->tickFromFrame(iClipStart);
		iClipOffset = pSession->frameFromTickRange(
			iClipStartTime, iClipStartTime + iClipOffsetTime, true);
	}

	pItem->clipStart  = iClipStart;
	pItem->clipOffset = iClipOffset;
	pItem->clipLength = iClipLength;

	long iFadeInLength = long(pClip->fadeInLength());
	if (iFadeInLength > 0) {
		if (fTimeStretch > 0.0f)
			iFadeInLength = long(fTimeStretch * float(iFadeInLength));
		else
			iFadeInLength += long(pClip->clipStart()) - long(iClipStart);
		if (iFadeInLength > 0)
			pItem->fadeInLength = iFadeInLength;
	}

	long iFadeOutLength = long(pClip->fadeOutLength());
	if (iFadeOutLength > 0) {
		if (fTimeStretch > 0.0f)
			iFadeOutLength = long(fTimeStretch * float(iFadeOutLength));
		else
			iFadeOutLength += long(iClipStart + iClipLength)
				- long(pClip->clipStart() + pClip->clipLength());
		if (iFadeOutLength > 0)
			pItem->fadeOutLength = iFadeOutLength;
	}

	if (fTimeStretch > 0.0f) {
		pItem->timeStretch = fTimeStretch;
		switch ((pClip->track())->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (pClip);
			const float fRatio = fTimeStretch / pAudioClip->timeStretch();
			pItem->clipOffset = (unsigned long) (fRatio * float(iClipOffset));
			break;
		}
		case qtractorTrack::Midi: {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			pItem->editCommand = createMidiEditCommand(pMidiClip, fTimeStretch);
			break;
		}
		default:
			break;
		}
	}

	if (fPitchShift > 0.0f)
		pItem->pitchShift = fPitchShift;
	m_items.append(pItem);

	if (isAudioClip(pClip) || pItem->editCommand == nullptr)
		reopenClip(pClip, fTimeStretch > 0.0f);
//	else
//		setClearSelect(true);
}


void qtractorClipCommand::gainClip ( qtractorClip *pClip, float fGain )
{
	Item *pItem = new Item(GainClip, pClip, pClip->track());
	pItem->clipGain = fGain;
	m_items.append(pItem);

	if (isAudioClip(pClip))
		reopenClip(pClip);
}


void qtractorClipCommand::panningClip ( qtractorClip *pClip, float fPanning )
{
	Item *pItem = new Item(PanningClip, pClip, pClip->track());
	pItem->clipPanning = fPanning;
	m_items.append(pItem);

	if (isAudioClip(pClip))
		reopenClip(pClip);
}


void qtractorClipCommand::muteClip ( qtractorClip *pClip, bool bMute )
{
	Item *pItem = new Item(MuteClip, pClip, pClip->track());
	pItem->clipMute = bMute;
	m_items.append(pItem);
}


void qtractorClipCommand::fadeInClip ( qtractorClip *pClip,
	unsigned long iFadeInLength, qtractorClip::FadeType fadeInType )
{
	Item *pItem = new Item(FadeInClip, pClip, pClip->track());
	pItem->fadeInLength = iFadeInLength;
	pItem->fadeInType = fadeInType;
	m_items.append(pItem);
}


void qtractorClipCommand::fadeOutClip ( qtractorClip *pClip,
	unsigned long iFadeOutLength, qtractorClip::FadeType fadeOutType )
{
	Item *pItem = new Item(FadeOutClip, pClip, pClip->track());
	pItem->fadeOutLength = iFadeOutLength;
	pItem->fadeOutType = fadeOutType;
	m_items.append(pItem);
}


void qtractorClipCommand::timeStretchClip (
	qtractorClip *pClip, float fTimeStretch )
{
	Item *pItem = new Item(TimeStretchClip, pClip, pClip->track());
	pItem->timeStretch = fTimeStretch;
	m_items.append(pItem);

	if (isAudioClip(pClip))
		reopenClip(pClip, true);
}


void qtractorClipCommand::pitchShiftClip (
	qtractorClip *pClip, float fPitchShift )
{
	Item *pItem = new Item(PitchShiftClip, pClip, pClip->track());
	pItem->pitchShift = fPitchShift;
	m_items.append(pItem);

	if (isAudioClip(pClip))
		reopenClip(pClip);
}


void qtractorClipCommand::takeInfoClip (
	qtractorClip *pClip, qtractorClip::TakeInfo *pTakeInfo )
{
	Item *pItem = new Item(TakeInfoClip, pClip, pClip->track());
	pItem->takeInfo = pTakeInfo;
	m_items.append(pItem);

	if (pTakeInfo)
		pTakeInfo->addRef();
}


void qtractorClipCommand::resetClip ( qtractorClip *pClip )
{
	Item *pItem = new Item(ResetClip, pClip, pClip->track());
	pItem->clipOffset = pClip->clipOffset();
	pItem->clipLength = pClip->clipLength();
	m_items.append(pItem);

//	setClearSelect(true);
}


void qtractorClipCommand::wsolaClip ( qtractorClip *pClip,
	bool bWsolaTimeStretch, bool bWsolaQuickSeek )
{
	Item *pItem = new Item(WsolaClip, pClip, pClip->track());
	pItem->wsolaTimeStretch = bWsolaTimeStretch;
	pItem->wsolaQuickSeek = bWsolaQuickSeek;
	m_items.append(pItem);

	if (isAudioClip(pClip))
		reopenClip(pClip, true);
}


// Conveniency helper: whether a clip is certain type.
bool qtractorClipCommand::isAudioClip ( qtractorClip *pClip ) const
{
	if (pClip->track())
		return ((pClip->track())->trackType() == qtractorTrack::Audio);
	else
		return false;
}


// When clips need to get reopenned.
void qtractorClipCommand::reopenClip ( qtractorClip *pClip, bool bClose )
{
	QHash<qtractorClip *, bool>::ConstIterator iter
		= m_clips.constFind(pClip);
	if (iter == m_clips.constEnd())
		m_clips.insert(pClip, bClose);
//	setClearSelect(true);
}


// Special clip record nethods.
bool qtractorClipCommand::addClipRecord (
	qtractorTrack *pTrack, unsigned long iFrameTime )
{
	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	qtractorClip *pClip = pTrack->clipRecord();
	if (pClip == nullptr)
		return false;

	// Recording clip extents...
	const unsigned long iClipEnd = pTrack->clipRecordEnd(iFrameTime);
	const unsigned long iClipStart = pClip->clipStart();
	if (iClipStart >= iClipEnd)
		return false;

	const qtractorTrack::TrackType trackType = pTrack->trackType();

	// HACK: Exclusive MIDI clip recording/overdub...
	if (pTrack->isClipRecordEx()) {
		if (trackType == qtractorTrack::Midi) {
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				// HACK: Make all final adjustments...
				qtractorMidiEditCommand(pMidiClip, name()).adjust();
				// Have a new filename revision...
				const QString& sFilename
					= pMidiClip->createFilePathRevision(true);
				// Save/replace the overdubbed clip...
				pMidiClip->saveCopyFile(sFilename, false);
				// Just change filename/track-channel...
				fileClip(pMidiClip, sFilename, pMidiClip->trackChannel());
				// Post-commit dirty changes...
				pSession->files()->addClipItem(qtractorFileList::Midi, sFilename, true);
			}
		}
		// Can get rid of the recorded clip.
		pTrack->setClipRecord(nullptr);
		return true;
	}

	// Recorded clip length fixup...
	pClip->setClipLength(iClipEnd - iClipStart);

	// Time to close the clip...
	pClip->close();

	// Actual clip length might have changed on close.
	const unsigned long iClipLength = pClip->clipLength();
	if (iClipLength < 1)
		return false;

	// Recorded clip offset...
	unsigned long iClipOffset = pClip->clipOffset();
	// Audio clips may need some record latency compensation...
	if (trackType == qtractorTrack::Audio) {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine)
			iClipOffset += pAudioEngine->transportLatency();
	}

	// Check whether in loop-recording/takes mode...
	const unsigned long iLoopStart = pSession->loopStart();
	const unsigned long iLoopEnd = pSession->loopEnd();

	if (iLoopStart < iLoopEnd
		&& pSession->loopRecordingMode() > 0
		&& iClipStart < iLoopEnd && iClipEnd > iLoopEnd) {
		// HACK: Take care of punch-in/out...
		unsigned long iTakeStart = iLoopStart;
		unsigned long iTakeEnd = iLoopEnd;
		unsigned long iTakeGap = 0;
		if (pSession->isPunching()) {
			const unsigned long iPunchIn = pSession->punchIn();
			const unsigned long iPunchOut = pSession->punchOut();
			if (iTakeStart < iPunchIn && iPunchIn < iTakeEnd) {
				iTakeGap += (iPunchIn - iTakeStart);
				iTakeStart = iPunchIn;
			}
			if (iTakeStart < iPunchOut && iPunchOut < iTakeEnd) {
				iTakeGap += (iTakeEnd - iPunchOut);
				iTakeEnd = iPunchOut;
			}
		}
		qtractorClip::TakeInfo *pTakeInfo
			= new qtractorClip::TakeInfo(
				iClipStart, iClipOffset, iClipLength,
				iTakeStart, iTakeEnd, iTakeGap);
		const int iTake = (pSession->loopRecordingMode() == 1 ? 0 : -1);
		pTakeInfo->setCurrentTake(pTakeInfo->select(this, pTrack, iTake));
	}
	else
	addClipRecordTake(pTrack, pClip, iClipStart, iClipOffset, iClipLength);

	// Make sure this brand new filename is not reused!
	// (see qtractroSession::createFilePath()...)
	pSession->releaseFilePath(pClip->filename());

	// Can get rid of the recorded clip.
	pTrack->setClipRecord(nullptr);

	// Done.
	return true;
}


bool qtractorClipCommand::addClipRecordTake ( qtractorTrack *pTrack,
	qtractorClip *pClip, unsigned long iClipStart, unsigned long iClipOffset,
	unsigned long iClipLength, qtractorClip::TakePart *pTakePart )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClipCommand[%p]::addClipRecordTake(%p, %p, %lu, %lu, %lu, %p)",
		this, pTrack, pClip, iClipStart, iClipOffset, iClipLength, pTakePart);
#endif

	// Now, its imperative to make a proper copy of those clips...
	// -- formerly a cloning clip factory method.

	// Reference for immediate file addition...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();

	switch (pTrack->trackType()) {
	case qtractorTrack::Audio: {
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (pClip);
		if (pAudioClip) {
			pAudioClip = new qtractorAudioClip(*pAudioClip);
			pAudioClip->setClipStart(iClipStart);
			pAudioClip->setClipOffset(iClipOffset);
			pAudioClip->setClipLength(iClipLength);
			addClip(pAudioClip, pTrack);
			if (pTakePart) {
				takeInfoClip(pAudioClip, pTakePart->takeInfo());
				pTakePart->setClip(pAudioClip);
			}
			if (pMainForm) {
				pMainForm->addAudioFile(pAudioClip->filename());
				qtractorTracks *pTracks = pMainForm->tracks();
				if (pTracks)
					pTracks->setCurrentClip(pAudioClip);
			}
		}
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			pMidiClip = new qtractorMidiClip(*pMidiClip);
			pMidiClip->setClipStart(iClipStart);
			pMidiClip->setClipOffset(iClipOffset);
			pMidiClip->setClipLength(iClipLength);
			addClip(pMidiClip, pTrack);
			if (pTakePart) {
				takeInfoClip(pMidiClip, pTakePart->takeInfo());
				pTakePart->setClip(pMidiClip);
			}
			if (pMainForm)
				pMainForm->addMidiFile(pMidiClip->filename());
				qtractorTracks *pTracks = pMainForm->tracks();
				if (pTracks)
					pTracks->setCurrentClip(pMidiClip);
		}
		break;
	}
	default:
		return false;
	}

	return true;
}


// When new tracks are needed.
void qtractorClipCommand::addTrack ( qtractorTrack *pTrack )
{
	m_trackCommands.append(
		new qtractorAddTrackCommand(pTrack));
}


// When MIDI clips are stretched.
qtractorMidiEditCommand *qtractorClipCommand::createMidiEditCommand (
	qtractorMidiClip *pMidiClip, float fTimeStretch )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return nullptr;

	// Make it like an undoable command...
	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiClip, name());

	qtractorMidiSequence *pSeq = pMidiClip->sequence();
	const unsigned long iTimeOffset = pSeq->timeOffset();

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pMidiClip->clipStart());
	const unsigned long t0 = pNode->tickFromFrame(pMidiClip->clipStart());

	unsigned long f1 = pMidiClip->clipStart() + pMidiClip->clipOffset();
	pNode = cursor.seekFrame(f1);
	const unsigned long t1 = pNode->tickFromFrame(f1);
	unsigned long iTimeStart = t1 - t0;
	iTimeStart = (iTimeStart > iTimeOffset ? iTimeStart - iTimeOffset : 0);
	pNode = cursor.seekFrame(f1 += pMidiClip->clipLength());
	const unsigned long iTimeEnd = iTimeStart + pNode->tickFromFrame(f1) - t1;

	for (qtractorMidiEvent *pEvent = pSeq->events().first();
			pEvent; pEvent = pEvent->next()) {
		unsigned long iTime = pEvent->time();
		if (iTime >= iTimeStart && iTime < iTimeEnd) {
			iTime = qtractorTimeScale::uroundf(
				fTimeStretch * float(iTime));
			unsigned long iDuration = pEvent->duration();
			if (pEvent->type() == qtractorMidiEvent::NOTEON) {
				iDuration = qtractorTimeScale::uroundf(
					fTimeStretch * float(iDuration));
			}
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
	}

	// Must have brand new revision...
	pMidiClip->setRevision(0);

	// That's it...
	return pEditCommand;
}


// Composite predicate.
bool qtractorClipCommand::isEmpty (void) const
{
	return m_trackCommands.isEmpty() && m_items.isEmpty();
}


// Common executive method.
bool qtractorClipCommand::execute ( bool bRedo )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	pSession->lock();

	QListIterator<qtractorTrackCommand *> track(m_trackCommands);
	while (track.hasNext()) {
	    qtractorTrackCommand *pTrackCommand = track.next();
		if (bRedo)
			pTrackCommand->redo();
		else
			pTrackCommand->undo();
	}

	// Pre-close needed clips once...
	QHash<qtractorClip *, bool>::ConstIterator clip = m_clips.constBegin();
	const QHash<qtractorClip *, bool>::ConstIterator& clip_end = m_clips.constEnd();
	for ( ; clip != clip_end; ++clip) {
		if (clip.value()) // Scrap peak file (audio).
			clip.key()->close();
	}

	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		qtractorClip  *pClip  = pItem->clip;
		qtractorTrack *pTrack = pItem->track;
		// Execute the command item...
		switch (pItem->command) {
		case AddClip: {
			if (bRedo)
				pTrack->addClip(pClip);
			else
				pTrack->removeClip(pClip);
			pItem->autoDelete = !bRedo;
			pSession->updateTrack(pTrack);
			setClearSelectReset(!bRedo);
			break;
		}
		case RemoveClip: {
			if (bRedo)
				pTrack->removeClip(pClip);
			else
				pTrack->addClip(pClip);
			pItem->autoDelete = bRedo;
			pSession->updateTrack(pTrack);
			setClearSelectReset(bRedo);
			break;
		}
		case FileClip: {
			const QString sOldFilename = pClip->filename();
			unsigned short iOldTrackChannel = 0;
			pClip->setFilename(pItem->filename);
			qtractorMidiClip *pMidiClip = nullptr;
			if (pTrack->trackType() == qtractorTrack::Midi)
				pMidiClip = static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				iOldTrackChannel = pMidiClip->trackChannel();
				pMidiClip->setTrackChannel(pItem->trackChannel);
			}
			pItem->filename = sOldFilename;
			if (pMidiClip)
				pItem->trackChannel = iOldTrackChannel;
			//pSession->updateTrack(pTrack);
			break;
		}
		case RenameClip: {
			const QString sOldName = pClip->clipName();
			pClip->setClipName(pItem->clipName);
			pItem->clipName = sOldName;
			break;
		}
		case MoveClip: {
			qtractorTrack *pOldTrack = pClip->track();
			const unsigned long iOldStart = pClip->clipStart();
			const unsigned long iOldOffset = pClip->clipOffset();
			const unsigned long iOldLength = pClip->clipLength();
			const unsigned long iOldFadeIn = pClip->fadeInLength();
			const unsigned long iOldFadeOut = pClip->fadeOutLength();
			pOldTrack->removeClipEx(pClip);
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pClip->setFadeInLength(pItem->fadeInLength);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pTrack->addClipEx(pClip);
			pItem->track = pOldTrack;
			pItem->clipStart = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeOutLength = iOldFadeOut;
			if (pOldTrack != pTrack)
				pSession->updateTrack(pOldTrack);
			pSession->updateTrack(pTrack);
			setClearSelect(true);
			break;
		}
		case ResizeClip: {
			const unsigned long iOldStart = pClip->clipStart();
			const unsigned long iOldOffset = pClip->clipOffset();
			const unsigned long iOldLength = pClip->clipLength();
			const unsigned long iOldFadeIn = pClip->fadeInLength();
			const unsigned long iOldFadeOut = pClip->fadeOutLength();
			float fOldTimeStretch = 0.0f;
			float fOldPitchShift  = 0.0f;
			qtractorAudioClip *pAudioClip = nullptr;
			if (pTrack->trackType() == qtractorTrack::Audio) {
				pAudioClip = static_cast<qtractorAudioClip *> (pClip);
				if (pAudioClip) {
					if (pItem->timeStretch > 0.0f) {
						fOldTimeStretch = pAudioClip->timeStretch();
					//--pAudioClip->close(); // Scrap peak file.
					}
					if (pItem->pitchShift > 0.0f)
						fOldPitchShift = pAudioClip->pitchShift();
				}
			}
			if (iOldStart != pItem->clipStart)
				pTrack->removeClip(pClip);
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pClip->setFadeInLength(pItem->fadeInLength);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			if (pAudioClip) {
				if (pItem->timeStretch > 0.0f) {
					pAudioClip->setTimeStretch(pItem->timeStretch);
					pItem->timeStretch = fOldTimeStretch;
				}
				if (pItem->pitchShift > 0.0f) {
					pAudioClip->setPitchShift(pItem->pitchShift);
					pItem->pitchShift = fOldPitchShift;
				}
			}
			if (pItem->editCommand) {
				if (bRedo)
					(pItem->editCommand)->redo();
				else
					(pItem->editCommand)->undo();
			}
			if (iOldStart != pItem->clipStart)
				pTrack->insertClip(pClip);
			pItem->clipStart = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeOutLength = iOldFadeOut;
			pSession->updateTrack(pTrack);
			break;
		}
		case GainClip: {
			const float fOldGain = pClip->clipGain();
			pClip->setClipGain(pItem->clipGain);
			pItem->clipGain = fOldGain;
			break;
		}
		case PanningClip: {
			const float fOldPanning = pClip->clipPanning();
			pClip->setClipPanning(pItem->clipPanning);
			pItem->clipPanning = fOldPanning;
			break;
		}
		case MuteClip: {
			const bool bOldMute = pClip->isClipMute();
			pClip->setClipMute(pItem->clipMute);
			pItem->clipMute = bOldMute;
			break;
		}
		case FadeInClip: {
			const unsigned long iOldFadeIn = pClip->fadeInLength();
			qtractorClip::FadeType oldFadeInType = pClip->fadeInType();
			pClip->setFadeInType(pItem->fadeInType);
			pClip->setFadeInLength(pItem->fadeInLength);
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeInType = oldFadeInType;
			break;
		}
		case FadeOutClip: {
			const unsigned long iOldFadeOut = pClip->fadeOutLength();
			qtractorClip::FadeType oldFadeOutType = pClip->fadeOutType();
			pClip->setFadeOutType(pItem->fadeOutType);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pItem->fadeOutLength = iOldFadeOut;
			pItem->fadeOutType = oldFadeOutType;
			break;
		}
		case TimeStretchClip: {
			qtractorAudioClip *pAudioClip = nullptr;
			if (pTrack->trackType() == qtractorTrack::Audio)
				pAudioClip = static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				const float fOldTimeStretch = pAudioClip->timeStretch();
				pAudioClip->setTimeStretch(pItem->timeStretch);
				pAudioClip->updateClipTime();	// Care of tempo change.
				pItem->timeStretch = fOldTimeStretch;
			}
			break;
		}
		case PitchShiftClip: {
			qtractorAudioClip *pAudioClip = nullptr;
			if (pTrack->trackType() == qtractorTrack::Audio)
				pAudioClip = static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				const float fOldPitchShift = pAudioClip->pitchShift();
				pAudioClip->setPitchShift(pItem->pitchShift);
				pItem->pitchShift = fOldPitchShift;
			}
			break;
		}
		case TakeInfoClip: {
			qtractorClip::TakeInfo *pTakeInfo = pClip->takeInfo();
			if (pTakeInfo)
				pTakeInfo->addRef();
			pClip->setTakeInfo(pItem->takeInfo);
			if (pItem->takeInfo)
				pItem->takeInfo->releaseRef();
			pItem->takeInfo = pTakeInfo;
			break;
		}
		case ResetClip: {
			const unsigned long iClipStartTime  = pClip->clipStartTime();
			const unsigned long iClipLengthTime = pClip->clipLengthTime();
			pItem->clipLength =	pSession->frameFromTickRange(
				iClipStartTime, iClipStartTime + iClipLengthTime);
			pClip->setClipLength(pItem->clipLength);
		#if 1// FIXUP: Don't quantize to MIDI metronomic time-scale...
			const unsigned long iClipOffset = pClip->clipOffset();
			pClip->setClipOffset(pItem->clipOffset);
			pItem->clipOffset =	iClipOffset;
		#else
			const unsigned long iClipOffsetTime = pClip->clipOffsetTime();
			pClip->setClipOffset(pItem->clipOffset);
			pItem->clipOffset =	pSession->frameFromTickRange(
				iClipStartTime, iClipStartTime + iClipOffsetTime, true);
		#endif
			break;
		}
		case WsolaClip: {
			qtractorAudioClip *pAudioClip = nullptr;
			if (pTrack->trackType() == qtractorTrack::Audio)
				pAudioClip = static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				const bool bOldWsolaTimeStretch = pAudioClip->isWsolaTimeStretch();
				const bool bOldWsolaQuickSeek = pAudioClip->isWsolaQuickSeek();
				pAudioClip->setWsolaTimeStretch(pItem->wsolaTimeStretch);
				pAudioClip->setWsolaQuickSeek(pItem->wsolaQuickSeek);
				pItem->wsolaTimeStretch = bOldWsolaTimeStretch;
				pItem->wsolaQuickSeek = bOldWsolaQuickSeek;
			}
			break;
		}
		default:
			break;
		}
	}

	// Re-open needed clips, just once...
	for (clip = m_clips.constBegin(); clip != clip_end; ++clip)
		clip.key()->open();

	pSession->unlock();

	return true;
}


// Virtual command methods.
bool qtractorClipCommand::redo (void)
{
	return execute(true);
}

bool qtractorClipCommand::undo (void)
{
	return execute(false);
}


//----------------------------------------------------------------------
// class qtractorClipTakeCommand - declaration.
//

// Constructor.
qtractorClipTakeCommand::qtractorClipTakeCommand (
	qtractorClip::TakeInfo *pTakeInfo, qtractorTrack *pTrack, int iCurrentTake )
	: qtractorClipCommand(QString()),
		m_pTakeInfo(pTakeInfo), m_iCurrentTake(iCurrentTake)
{
	if (pTrack)
		qtractorCommand::setName(QObject::tr("take %1").arg(iCurrentTake + 1));
	else
		qtractorCommand::setName(QObject::tr("reset takes"));

	if (pTrack && iCurrentTake >= 0)
		m_pTakeInfo->select(this, pTrack, iCurrentTake);
	else
		m_pTakeInfo->reset(this, pTrack == nullptr);
}


// Executive override.
bool qtractorClipTakeCommand::execute ( bool bRedo )
{
	const int iCurrentTake = m_pTakeInfo->currentTake();
	m_pTakeInfo->setCurrentTake(m_iCurrentTake);
	m_iCurrentTake = iCurrentTake;;

	return qtractorClipCommand::execute(bRedo);
}


//----------------------------------------------------------------------
// class qtractorClipRangeCommand - declaration.
//

// Constructor.
qtractorClipRangeCommand::qtractorClipRangeCommand ( const QString& sName )
	: qtractorClipCommand(sName)
{
}


// Destructor.
qtractorClipRangeCommand::~qtractorClipRangeCommand (void)
{
	qDeleteAll(m_sessionCommands);
	m_sessionCommands.clear();

	qDeleteAll(m_curveEditCommands);
	m_curveEditCommands.clear();

	qDeleteAll(m_timeScaleMarkerCommands);
	m_timeScaleMarkerCommands.clear();

	qDeleteAll(m_timeScaleNodeCommands);
	m_timeScaleNodeCommands.clear();
}


// When Loop/Punch changes are needed.
void qtractorClipRangeCommand::addSessionCommand (
	qtractorSessionCommand *pSessionCommand )
{
	m_sessionCommands.append(pSessionCommand);
}


// When automation curves are needed.
void qtractorClipRangeCommand::addCurveEditCommand (
	qtractorCurveEditCommand *pCurveEditCommand )
{
	m_curveEditCommands.append(pCurveEditCommand);
}


// When location markers are needed.
void qtractorClipRangeCommand::addTimeScaleMarkerCommand (
	qtractorTimeScaleMarkerCommand *pTimeScaleMarkerCommand )
{
	m_timeScaleMarkerCommands.append(pTimeScaleMarkerCommand);
}


// When tempo-map/time-sig nodes are needed.
void qtractorClipRangeCommand::addTimeScaleNodeCommand (
	qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand )
{
	m_timeScaleNodeCommands.append(pTimeScaleNodeCommand);
}


// Executive override.
bool qtractorClipRangeCommand::execute ( bool bRedo )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	// Loop/Punch...
	if (!m_sessionCommands.isEmpty()) {
		QListIterator<qtractorSessionCommand *> iter(m_sessionCommands);
		while (iter.hasNext()) {
			qtractorSessionCommand *pSessionCommand	= iter.next();
			if (bRedo)
				pSessionCommand->redo();
			else
				pSessionCommand->undo();
		}
	}

	// Automation...
	if (!m_curveEditCommands.isEmpty()) {
		QListIterator<qtractorCurveEditCommand *>
			curve_iter(m_curveEditCommands);
		if (bRedo) {
			while (curve_iter.hasNext()) {
				qtractorCurveEditCommand *pCurveEditCommand
					= curve_iter.next();
				pCurveEditCommand->redo();
				pCurveEditCommand->curve()->update();
			}
		} else {
			curve_iter.toBack();
			while (curve_iter.hasPrevious()) {
				qtractorCurveEditCommand *pCurveEditCommand
					= curve_iter.previous();
				pCurveEditCommand->undo();
				pCurveEditCommand->curve()->update();
			}
		}
	}

	// Markers...
	if (!m_timeScaleMarkerCommands.isEmpty()) {
		QListIterator<qtractorTimeScaleMarkerCommand *>
			marker_iter(m_timeScaleMarkerCommands);
		if (bRedo) {
			while (marker_iter.hasNext()) {
				qtractorTimeScaleMarkerCommand *pTimeScaleMarkerCommand
					= marker_iter.next();
				pTimeScaleMarkerCommand->redo();
			}
		} else {
			marker_iter.toBack();
			while (marker_iter.hasPrevious()) {
				qtractorTimeScaleMarkerCommand *pTimeScaleMarkerCommand
					= marker_iter.previous();
				pTimeScaleMarkerCommand->undo();
			}
		}
	}

	// Time-map...
	if (!m_timeScaleNodeCommands.isEmpty()) {
		// If currently playing, we need to do a stop and go...
		const bool bPlaying = pSession->isPlaying();
		if (bPlaying)
			pSession->lock();
		QListIterator<qtractorTimeScaleNodeCommand *>
			node_iter(m_timeScaleNodeCommands);
		if (bRedo) {
			while (node_iter.hasNext()) {
				qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand
					= node_iter.next();
				pTimeScaleNodeCommand->redo();
			}
		} else {
			node_iter.toBack();
			while (node_iter.hasPrevious()) {
				qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand
					= node_iter.previous();
				pTimeScaleNodeCommand->undo();
			}
		}
		// Restore playback state, if needed...
		if (bPlaying) {
			// The Audio engine too...
			if (pSession->audioEngine())
				pSession->audioEngine()->resetMetro();
			// The MIDI engine queue needs a reset...
			if (pSession->midiEngine())
				pSession->midiEngine()->resetTempo();
			pSession->unlock();
		}
	}

	// Clips...
	return qtractorClipCommand::execute(bRedo);
}


//----------------------------------------------------------------------
// class qtractorClipContextCommand - declaration. (virtual class)
//

// Constructor.
qtractorClipContextCommand::qtractorClipContextCommand (
	const QString& sName ) : qtractorCommand(sName), m_iRedoCount(0)
{
}


// Destructor.
qtractorClipContextCommand::~qtractorClipContextCommand (void)
{
}


// Composite command methods.
void qtractorClipContextCommand::addMidiClipContext (
	qtractorMidiClip *pMidiClip )
{
	MidiClipCtx& mctx = m_midiClipCtxs[pMidiClip];
	mctx.filename = pMidiClip->filename();
	mctx.offset = pMidiClip->clipOffset();
	mctx.length = pMidiClip->clipLength();
}


// Composite predicate.
bool qtractorClipContextCommand::isEmpty (void) const
{
	return m_midiClipCtxs.isEmpty();
}


// Main executive method.
bool qtractorClipContextCommand::execute ( bool bRedo )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	++m_iRedoCount;

	MidiClipCtxs::Iterator iter = m_midiClipCtxs.begin();
	const MidiClipCtxs::Iterator& iter_end = m_midiClipCtxs.end();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiClip *pMidiClip = iter.key();
		MidiClipCtx& mctx = iter.value();
		const QString filename = pMidiClip->filename();
		const unsigned long offset = pMidiClip->clipOffset();
		const unsigned long length = pMidiClip->clipLength();
		if (m_iRedoCount == 1 && bRedo) {
			const QString& sFilename
				= pMidiClip->createFilePathRevision(true);
			if (!pMidiClip->saveCopyFile(sFilename, false))
				continue;
			pMidiClip->setDirty(false);
			mctx.filename = sFilename;
		}
		pSession->files()->removeClipItem(qtractorFileList::Midi, pMidiClip);
		if (executeMidiClipContext(pMidiClip, mctx, bRedo)) {
			mctx.filename = filename;
			mctx.offset = offset;
			mctx.length = length;
		}
		pSession->files()->addClipItem(qtractorFileList::Midi, pMidiClip, true);
	}

	return true;
}


// Virtual command methods.
bool qtractorClipContextCommand::redo (void)
{
	return execute(true);
}


bool qtractorClipContextCommand::undo (void)
{
	return execute(false);
}


//----------------------------------------------------------------------
// class qtractorClipSaveFileCommand - declaration.
//

// Constructor.
qtractorClipSaveFileCommand::qtractorClipSaveFileCommand (void)
	: qtractorClipContextCommand(QObject::tr("clip save"))
{
}


// Context (visitor) executive method.
bool qtractorClipSaveFileCommand::executeMidiClipContext (
	qtractorMidiClip *pMidiClip, const MidiClipCtx& mctx, bool /*bRedo*/ )
{
	pMidiClip->setClipLength(mctx.length);
	pMidiClip->setClipOffset(mctx.offset);
	pMidiClip->setFilenameEx(mctx.filename, true);

	return true;
}


//----------------------------------------------------------------------
// class qtractorClipUnlinkCommand - declaration.
//

// Constructor.
qtractorClipUnlinkCommand::qtractorClipUnlinkCommand (void)
	: qtractorClipContextCommand(QObject::tr("clip unlink"))
{
}


// Context (visitor) executive method.
bool qtractorClipUnlinkCommand::executeMidiClipContext (
	qtractorMidiClip *pMidiClip, const MidiClipCtx& mctx, bool bRedo )
{
	pMidiClip->setClipLength(mctx.length);
	pMidiClip->setClipOffset(mctx.offset);
	pMidiClip->setFilename(mctx.filename);

	if (bRedo)
		pMidiClip->unlinkHashData();
	else
		pMidiClip->relinkHashData();

	return true;
}


//----------------------------------------------------------------------
// class qtractorClipToolCommand - declaration.
//

// Constructor.
qtractorClipToolCommand::qtractorClipToolCommand ( const QString& sName )
	: qtractorCommand(QObject::tr("clip tool %1").arg(sName)),
		m_iRedoCount(0), m_pClipSaveFileCommand(nullptr)
{
}


// Destructor.
qtractorClipToolCommand::~qtractorClipToolCommand (void)
{
	qDeleteAll(m_midiEditCommands);
	m_midiEditCommands.clear();

	qDeleteAll(m_timeScaleNodeCommands);
	m_timeScaleNodeCommands.clear();

	if (m_pClipSaveFileCommand)
		delete m_pClipSaveFileCommand;
}


// Check if a clip is already part of the iting set.
bool qtractorClipToolCommand::isLinkedMidiClip ( qtractorMidiClip *pMidiClip ) const
{
	QListIterator<qtractorMidiEditCommand *> iter(m_midiEditCommands);
	while (iter.hasNext()) {
		qtractorMidiClip *pMidiClipIter = iter.next()->midiClip();
		if (pMidiClipIter && pMidiClipIter->isLinkedClip(pMidiClip))
			return true;
	}
	return false;
}


// Composite command methods.
void qtractorClipToolCommand::addMidiEditCommand (
	qtractorMidiEditCommand *pMidiEditCommand )
{
	m_midiEditCommands.append(pMidiEditCommand);
}


// When additional tempo-map/time-sig nodes are needed.
void qtractorClipToolCommand::addTimeScaleNodeCommand (
	qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand )
{
	m_timeScaleNodeCommands.append(pTimeScaleNodeCommand);
}


// Composite predicate.
bool qtractorClipToolCommand::isEmpty (void) const
{
	return m_midiEditCommands.isEmpty();
}


// Virtual command methods.
bool qtractorClipToolCommand::redo (void)
{
	++m_iRedoCount;

	executeTimeScaleNodeCommands(true);

	QListIterator<qtractorMidiEditCommand *> iter(m_midiEditCommands);
	while (iter.hasNext()) {
		qtractorMidiEditCommand *pMidiEditCommand = iter.next();
		if (pMidiEditCommand) {
			qtractorMidiClip *pMidiClip = pMidiEditCommand->midiClip();
			if (pMidiClip) {
				// Save if dirty...
				if (m_iRedoCount == 1/* && pMidiClip->isDirty()*/) {
					if (m_pClipSaveFileCommand == nullptr)
						m_pClipSaveFileCommand = new qtractorClipSaveFileCommand();
					m_pClipSaveFileCommand->addMidiClipContext(pMidiClip);
				}
				// Redo as you told...
				pMidiEditCommand->redo();
			}
		}
	}

	if (m_pClipSaveFileCommand)
		m_pClipSaveFileCommand->redo();

	return true;
}


bool qtractorClipToolCommand::undo (void)
{
	if (m_pClipSaveFileCommand)
		m_pClipSaveFileCommand->undo();

	QListIterator<qtractorMidiEditCommand *> iter(m_midiEditCommands);
	iter.toBack();
	while (iter.hasPrevious()) {
		qtractorMidiEditCommand *pMidiEditCommand = iter.previous();
		if (pMidiEditCommand) {
			qtractorMidiClip *pMidiClip = pMidiEditCommand->midiClip();
			if (pMidiClip) {
				// Undo as you told...
				pMidiEditCommand->undo();
			}
		}
	}

	executeTimeScaleNodeCommands(false);

	return (m_iRedoCount > 0);
}


// Execute tempo-map/time-sig commands.
void qtractorClipToolCommand::executeTimeScaleNodeCommands ( bool bRedo )
{
	int iTimeScaleUpdate = 0;
	QListIterator<qtractorTimeScaleNodeCommand *> iter(m_timeScaleNodeCommands);
	if (bRedo)
		iter.toFront();
	else
		iter.toBack();
	while (bRedo ? iter.hasNext() : iter.hasPrevious()) {
		qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand
			= (bRedo ? iter.next() : iter.previous());
		if (bRedo)
			pTimeScaleNodeCommand->redo();
		else
			pTimeScaleNodeCommand->undo();
		++iTimeScaleUpdate;
	}

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && iTimeScaleUpdate > 0)
		pMainForm->updateNotifySlot(qtractorCommand::ClearSelect);
}


//----------------------------------------------------------------------
// class qtractorClipRecordExCommand - implementation.
//

// Constructor.
qtractorClipRecordExCommand::qtractorClipRecordExCommand (
	qtractorClip *pClipRecordEx, bool bClipRecordEx )
	: qtractorCommand(QObject::tr("clip record"))
{
	m_pTrack = pClipRecordEx->track();

	m_pClipRecordEx = pClipRecordEx;
	m_bClipRecordEx = bClipRecordEx;

	setRefresh(true);
}


// Clip-record command method.
bool qtractorClipRecordExCommand::redo (void)
{
	if (m_pTrack == nullptr)
		return false;

	// Carry on...
	qtractorClip *pClipRecordEx = m_pTrack->clipRecord();
	const bool bClipRecordEx = m_pTrack->isClipRecordEx();

	m_pTrack->setRecord(m_bClipRecordEx);
	m_pTrack->setClipRecord(m_bClipRecordEx ? m_pClipRecordEx : nullptr);
	m_pTrack->setClipRecordEx(m_bClipRecordEx);

	if (m_bClipRecordEx && m_pClipRecordEx
		&& m_pTrack->trackType() == qtractorTrack::Midi) {
		qtractorSession *pSession = m_pTrack->session();
		qtractorMidiClip *pMidiClip = static_cast<qtractorMidiClip *> (m_pClipRecordEx);
		if (pMidiClip && pSession) {
			pMidiClip->setStepInputHead(pSession->playHead());
			pMidiClip->updateStepInput();
		}
	}

	// Reset for undo.
	m_bClipRecordEx = bClipRecordEx;
	m_pClipRecordEx = pClipRecordEx;

	return true;
}

bool qtractorClipRecordExCommand::undo (void)
{
	return redo();
}


// end of qtractorClipCommand.cpp
