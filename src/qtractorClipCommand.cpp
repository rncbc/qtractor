// qtractorClipCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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
	m_items.append(new Item(AddClip, pClip, pTrack));

	setClearSelect(true);
}


void qtractorClipCommand::removeClip ( qtractorClip *pClip )
{
	m_items.append(new Item(RemoveClip, pClip, pClip->track()));

	setClearSelect(true);
}


// Edit command methods.
void qtractorClipCommand::fileClip ( qtractorClip *pClip,
	const QString& sFilename, unsigned short iTrackChannel )
{
	Item *pItem = new Item(FileClip, pClip, pClip->track());
	pItem->filename = sFilename;
	pItem->trackChannel = iTrackChannel;
	m_items.append(pItem);

	reopenClip(pClip, true);
}


void qtractorClipCommand::renameClip ( qtractorClip *pClip,
	const QString& sClipName )
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
				takeInfoClip(pClip, NULL);
			pClip = pTakeInfo->clipPart(qtractorClip::TakeInfo::ClipTake);
			if (pClip)
				takeInfoClip(pClip, NULL);
		}
	}

	setClearSelect(true);
}


void qtractorClipCommand::resizeClip ( qtractorClip *pClip,
	unsigned long iClipStart, unsigned long iClipOffset,
	unsigned long iClipLength, float fTimeStretch, float fPitchShift )
{
	Item *pItem = new Item(ResizeClip, pClip, pClip->track());
	pItem->clipStart  = iClipStart;
	pItem->clipOffset = iClipOffset;
	pItem->clipLength = iClipLength;
	long iFadeInLength = long(pClip->fadeInLength());
	if (iFadeInLength > 0) {
		iFadeInLength += long(pClip->clipOffset()) - long(iClipOffset);
		if (iFadeInLength > 0)
			pItem->fadeInLength = iFadeInLength;
	}
	long iFadeOutLength = long(pClip->fadeOutLength());
	if (iFadeOutLength > 0) {
		iFadeOutLength += long(iClipOffset + iClipLength)
			- long(pClip->clipOffset() + pClip->clipLength());
		if (iFadeOutLength > 0)
			pItem->fadeOutLength = iFadeOutLength;
	}
	if (fTimeStretch > 0.0f) {
		pItem->timeStretch = fTimeStretch;
		switch ((pClip->track())->trackType()) {
		case qtractorTrack::Audio: {
			qtractorAudioClip *pAudioClip
				= static_cast<qtractorAudioClip *> (pClip);
			float fRatio = fTimeStretch / pAudioClip->timeStretch();
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

	if (pItem->editCommand == NULL)
		reopenClip(pClip, fTimeStretch > 0.0f);
	else
		setClearSelect(true);
}


void qtractorClipCommand::gainClip ( qtractorClip *pClip, float fGain )
{
	Item *pItem = new Item(GainClip, pClip, pClip->track());
	pItem->clipGain = fGain;
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


void qtractorClipCommand::timeStretchClip ( qtractorClip *pClip,
	float fTimeStretch )
{
	Item *pItem = new Item(TimeStretchClip, pClip, pClip->track());
	pItem->timeStretch = fTimeStretch;
	m_items.append(pItem);

	reopenClip(pClip, true);
}


void qtractorClipCommand::pitchShiftClip ( qtractorClip *pClip,
	float fPitchShift )
{
	Item *pItem = new Item(PitchShiftClip, pClip, pClip->track());
	pItem->pitchShift = fPitchShift;
	m_items.append(pItem);

	reopenClip(pClip);
}


void qtractorClipCommand::takeInfoClip ( qtractorClip *pClip,
	qtractorClip::TakeInfo *pTakeInfo )
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

	setClearSelect(true);
}


void qtractorClipCommand::reopenClip ( qtractorClip *pClip, bool bClose )
{
	QHash<qtractorClip *, bool>::ConstIterator iter
		= m_clips.constFind(pClip);
	if (iter == m_clips.constEnd())
		m_clips.insert(pClip, bClose);

	setClearSelect(true);
}


// Special clip record nethods.
bool qtractorClipCommand::addClipRecord (
	qtractorTrack *pTrack, unsigned long iFrameTime )
{
	qtractorClip *pClip = pTrack->clipRecord();
	if (pClip == NULL)
		return false;

	unsigned long iClipEnd = pTrack->clipRecordEnd(iFrameTime);
	unsigned long iClipStart = pClip->clipStart();
	if (iClipStart >= iClipEnd)
		return false;

	// Time to close the clip...
	pClip->close();

	// Actual clip length might have changed on close.
	if (pClip->clipLength() < 1)
		return false;

	// Recorded clip length and offset...
	unsigned long iClipLength = iClipEnd - iClipStart;
	unsigned long iClipOffset = 0;

	// Attend to audio clip record latency compensation...
	if (pTrack->trackType() == qtractorTrack::Audio) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pTrack->inputBus());
		if (pAudioBus)
			iClipOffset += pAudioBus->latency_in();
	}

	// Check whether in loop-recording/takes mode....
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && pSession->isLoopRecording()
		&& iClipEnd > pSession->loopEnd()) {
		qtractorClip::TakeInfo *pTakeInfo
			= new qtractorClip::TakeInfo(
				iClipStart, iClipOffset, iClipLength,
				pSession->loopStart(),
				pSession->loopEnd());
		int iTake = (pSession->loopRecordingMode() == 1 ? 0 : -1);
		iTake = pTakeInfo->select(this, pTrack, iTake);
		pTakeInfo->setCurrentTake(iTake);
	}
	else
	addClipRecordTake(pTrack, pClip, iClipStart, iClipOffset, iClipLength);

	// Can get rid of the recorded clip.
	pTrack->setClipRecord(NULL);

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
			if (pMainForm)	
				pMainForm->addAudioFile(pAudioClip->filename());
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
	if (pSession == NULL)
		return NULL;

	// Make it like an undoable command...
	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiClip, name());

	qtractorMidiSequence *pSeq = pMidiClip->sequence();
	unsigned long iTimeOffset = pSeq->timeOffset();

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pMidiClip->clipStart());
	unsigned long t0 = pNode->tickFromFrame(pMidiClip->clipStart());

	unsigned long f1 = pMidiClip->clipStart() + pMidiClip->clipOffset();
	pNode = cursor.seekFrame(f1);
	unsigned long t1 = pNode->tickFromFrame(f1);
	unsigned long iTimeStart = t1 - t0;
	iTimeStart = (iTimeStart > iTimeOffset ? iTimeStart - iTimeOffset : 0);
	pNode = cursor.seekFrame(f1 += pMidiClip->clipLength());
	unsigned long iTimeEnd = iTimeStart + pNode->tickFromFrame(f1) - t1;

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
	if (pSession == NULL)
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
			break;
		}
		case RemoveClip: {
			if (bRedo)
				pTrack->removeClip(pClip);
			else
				pTrack->addClip(pClip);
			pItem->autoDelete = bRedo;
			pSession->updateTrack(pTrack);
			break;
		}
		case FileClip: {
			QString sOldFilename = pClip->filename();
			unsigned short iOldTrackChannel = 0;
			pClip->setFilename(pItem->filename);
			qtractorMidiClip *pMidiClip = NULL;
			if (pTrack->trackType() == qtractorTrack::Midi)
				pMidiClip = static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				iOldTrackChannel = pMidiClip->trackChannel();
				pMidiClip->setTrackChannel(pItem->trackChannel);
			}
		//--pClip->close();	// Scrap peak file (audio).
		//--pClip->open();
			pItem->filename = sOldFilename;
			if (pMidiClip)
				pItem->trackChannel = iOldTrackChannel;
			pSession->updateTrack(pTrack);
			break;
		}
		case RenameClip: {
			QString sOldName = pClip->clipName();
			pClip->setClipName(pItem->clipName);
			pItem->clipName = sOldName;
			break;
		}
		case MoveClip: {
			qtractorTrack *pOldTrack = pClip->track();
			unsigned long  iOldStart = pClip->clipStart();
			unsigned long iOldOffset = pClip->clipOffset();
			unsigned long iOldLength = pClip->clipLength();
			unsigned long iOldFadeIn = pClip->fadeInLength();
			unsigned long iOldFadeOut = pClip->fadeOutLength();
			pOldTrack->removeClip(pClip);
			pClip->setClipStart(pItem->clipStart);
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pClip->setFadeInLength(pItem->fadeInLength);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pTrack->addClip(pClip);
			pItem->track      = pOldTrack;
			pItem->clipStart  = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeOutLength = iOldFadeOut;
			if (pOldTrack != pTrack)
				pSession->updateTrack(pOldTrack);
			pSession->updateTrack(pTrack);
			break;
		}
		case ResizeClip: {
			unsigned long iOldStart  = pClip->clipStart();
			unsigned long iOldOffset = pClip->clipOffset();
			unsigned long iOldLength = pClip->clipLength();
			unsigned long iOldFadeIn = pClip->fadeInLength();
			unsigned long iOldFadeOut = pClip->fadeOutLength();
			float fOldTimeStretch = 0.0f;
			float fOldPitchShift  = 0.0f;
			qtractorAudioClip *pAudioClip = NULL;
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
		//--else
		//--if (pItem->editCommand == NULL)
		//--	pClip->close();
			if (iOldStart != pItem->clipStart)
				pTrack->unlinkClip(pClip);
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
		//--else pClip->open();
			if (iOldStart != pItem->clipStart)
				pTrack->insertClip(pClip);
			pItem->clipStart  = iOldStart;
			pItem->clipOffset = iOldOffset;
			pItem->clipLength = iOldLength;
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeOutLength = iOldFadeOut;
			pSession->updateTrack(pTrack);
			break;
		}
		case GainClip: {
			float fOldGain = pClip->clipGain();
			pClip->setClipGain(pItem->clipGain);
			pItem->clipGain = fOldGain;
			break;
		}
		case FadeInClip: {
			unsigned long iOldFadeIn = pClip->fadeInLength();
			qtractorClip::FadeType oldFadeInType = pClip->fadeInType();
			pClip->setFadeInType(pItem->fadeInType);
			pClip->setFadeInLength(pItem->fadeInLength);
			pItem->fadeInLength = iOldFadeIn;
			pItem->fadeInType = oldFadeInType;
			break;
		}
		case FadeOutClip: {
			unsigned long iOldFadeOut = pClip->fadeOutLength();
			qtractorClip::FadeType oldFadeOutType = pClip->fadeOutType();
			pClip->setFadeOutType(pItem->fadeOutType);
			pClip->setFadeOutLength(pItem->fadeOutLength);
			pItem->fadeOutLength = iOldFadeOut;
			pItem->fadeOutType = oldFadeOutType;
			break;
		}
		case TimeStretchClip: {
			qtractorAudioClip *pAudioClip = NULL;
			if (pTrack->trackType() == qtractorTrack::Audio)
				pAudioClip = static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				float fOldTimeStretch = pAudioClip->timeStretch();
				pAudioClip->setTimeStretch(pItem->timeStretch);
			//--pAudioClip->close();			// Scrap peak file.
				pAudioClip->updateClipTime();	// Care of tempo change.
			//--pAudioClip->open();
				pItem->timeStretch = fOldTimeStretch;
			}
			break;
		}
		case PitchShiftClip: {
			qtractorAudioClip *pAudioClip = NULL;
			if (pTrack->trackType() == qtractorTrack::Audio)
				pAudioClip = static_cast<qtractorAudioClip *> (pClip);
			if (pAudioClip) {
				float fOldPitchShift = pAudioClip->pitchShift();
				pAudioClip->setPitchShift(pItem->pitchShift);
			//--pAudioClip->open();
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
			unsigned long iClipStartTime  = pClip->clipStartTime();
			unsigned long iClipOffsetTime = pClip->clipOffsetTime();
			unsigned long iClipLengthTime = pClip->clipLengthTime();
			pClip->setClipOffset(pItem->clipOffset);
			pClip->setClipLength(pItem->clipLength);
			pItem->clipOffset =	pSession->frameFromTickRange(
				iClipStartTime, iClipStartTime + iClipOffsetTime);
			pItem->clipLength =	pSession->frameFromTickRange(
				iClipStartTime, iClipStartTime + iClipLengthTime);
			break;
		}
		default:
			break;
		}
	}

	// Re-open needed clips, just once...
	for (clip = m_clips.constBegin(); clip != m_clips.constEnd(); ++clip)
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
		m_pTakeInfo->reset(this, pTrack == NULL);
}


// Executive override.
bool qtractorClipTakeCommand::execute ( bool bRedo )
{
	int iCurrentTake = m_pTakeInfo->currentTake();
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
	if (pSession == NULL)
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
		bool bPlaying = pSession->isPlaying();
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
// class qtractorClipToolCommand - declaration.
//

// Constructor.
qtractorClipToolCommand::qtractorClipToolCommand ( const QString& sName )
	: qtractorCommand(QObject::tr("clip tool %1").arg(sName)), m_iRedoCount(0)
{
}


// Destructor.
qtractorClipToolCommand::~qtractorClipToolCommand (void)
{
	qDeleteAll(m_midiEditCommands);
	m_midiEditCommands.clear();
}


// Composite command methods.
void qtractorClipToolCommand::addMidiEditCommand (
	qtractorMidiEditCommand *pMidiEditCommand )
{
	m_midiEditCommands.append(pMidiEditCommand);
}


// Composite predicate.
bool qtractorClipToolCommand::isEmpty (void) const
{
	return m_midiEditCommands.isEmpty();
}


// Virtual command methods.
bool qtractorClipToolCommand::redo (void)
{
	if (++m_iRedoCount > 1)
		return undo();

	QListIterator<qtractorMidiEditCommand *> iter(m_midiEditCommands);
	while (iter.hasNext()) {
		qtractorMidiEditCommand *pMidiEditCommand = iter.next();
		if (pMidiEditCommand) {
			qtractorMidiClip *pMidiClip = pMidiEditCommand->midiClip();
			if (pMidiClip) {
				// Save if dirty...
				if (pMidiClip->isDirty())
					pMidiClip->saveCopyFile(false);
				// Filename change one-time transaction...
				MidiClipCtx mctx;
				mctx.pre.filename = pMidiClip->filename();
				mctx.pre.length = pMidiClip->clipLength();
				if (!pMidiEditCommand->redo())
					return false;
				pMidiClip->setRevision(0);
				if (pMidiClip->saveCopyFile(true)) {
					mctx.post.filename = pMidiClip->filename();
					mctx.post.length = pMidiClip->clipLength();
					m_midiClipCtxs.insert(pMidiClip, mctx);
				}
			}
		}
	}

	return true;
}

bool qtractorClipToolCommand::undo (void)
{
	QListIterator<qtractorMidiEditCommand *> iter(m_midiEditCommands);
	iter.toBack();
	while (iter.hasPrevious()) {
		qtractorMidiEditCommand *pMidiEditCommand = iter.previous();
		if (pMidiEditCommand) {
			qtractorMidiClip *pMidiClip = pMidiEditCommand->midiClip();
			if (pMidiClip) {
				// Filename swap transaction...
				MidiClipCtx& mctx = m_midiClipCtxs[pMidiClip];
				if (mctx.pre.filename.isEmpty() || mctx.post.filename.isEmpty())
					return false;
				const unsigned long iPreLength = mctx.pre.length;
				const QString sPreFilename = mctx.pre.filename;
				const unsigned long iPostLength = mctx.post.length;
				const QString sPostFilename = mctx.post.filename;
			//	pMidiClip->close();
				pMidiClip->setClipLength(iPreLength);
				pMidiClip->setFilenameEx(sPreFilename, true);
				pMidiClip->open();
				mctx.post.filename = sPreFilename;
				mctx.post.length = iPreLength;
				mctx.pre.filename = sPostFilename;
				mctx.pre.length = iPostLength;
			}
		}
	}

	return (m_iRedoCount > 0);
}


// end of qtractorClipCommand.cpp
