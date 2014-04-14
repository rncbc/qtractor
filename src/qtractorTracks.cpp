// qtractorTracks.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTracks.h"
#include "qtractorTrackView.h"
#include "qtractorTrackList.h"
#include "qtractorTrackTime.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorSessionCommand.h"
#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"
#include "qtractorCurveCommand.h"
#include "qtractorMidiEditCommand.h"
#include "qtractorTimeScaleCommand.h"

#include "qtractorTrackButton.h"

#include "qtractorAudioEngine.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioClip.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"

#include "qtractorClipSelect.h"

#include "qtractorOptions.h"

#include "qtractorMainForm.h"
#include "qtractorTrackForm.h"
#include "qtractorClipForm.h"

#include "qtractorPasteRepeatForm.h"
#include "qtractorTempoAdjustForm.h"

#include "qtractorEditRangeForm.h"

#include "qtractorMidiEditorForm.h"

#include "qtractorMidiToolsForm.h"
#include "qtractorMidiEditSelect.h"

#include "qtractorFileList.h"

#include <QVBoxLayout>
#include <QProgressBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDate>
#include <QUrl>

#include <QHeaderView>


//----------------------------------------------------------------------------
// qtractorTracks -- The main session track listview widget.

// Constructor.
qtractorTracks::qtractorTracks ( QWidget *pParent )
	: QSplitter(Qt::Horizontal, pParent)
{
	// Surely a name is crucial (e.g. for storing geometry settings)
	QSplitter::setObjectName("qtractorTracks");

	// Create child widgets...
	m_pTrackList = new qtractorTrackList(this, this);
	QWidget *pVBox = new QWidget(this);
	m_pTrackTime = new qtractorTrackTime(this, pVBox);
	m_pTrackView = new qtractorTrackView(this, pVBox);

	// Zoom mode flag.
	m_iZoomMode = ZoomAll;

	// Create child box layouts...
	QVBoxLayout *pVBoxLayout = new QVBoxLayout(pVBox);
	pVBoxLayout->setMargin(0);
	pVBoxLayout->setSpacing(0);
	pVBoxLayout->addWidget(m_pTrackTime);
	pVBoxLayout->addWidget(m_pTrackView);
	pVBox->setLayout(pVBoxLayout);

//	QSplitter::setOpaqueResize(false);
	QSplitter::setStretchFactor(QSplitter::indexOf(m_pTrackList), 0);
	QSplitter::setHandleWidth(2);

	QSplitter::setWindowTitle(tr("Tracks"));
	QSplitter::setWindowIcon(QIcon(":/images/qtractorTracks.png"));

	// Get previously saved splitter sizes,
	// (with some fair default...)
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<int> sizes;
		sizes.append(160);
		sizes.append(480);
		pOptions->loadSplitterSizes(this, sizes);
	}

	// Early track list stabilization.
	m_pTrackTime->setFixedHeight(
		(m_pTrackList->header())->sizeHint().height());

	// To have all views in positional sync.
	QObject::connect(m_pTrackList, SIGNAL(contentsMoving(int,int)),
		m_pTrackView, SLOT(contentsYMovingSlot(int,int)));
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackList, SLOT(contentsYMovingSlot(int,int)));
}


// Destructor.
qtractorTracks::~qtractorTracks (void)
{
	// Save splitter sizes...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->saveSplitterSizes(this);
}


// Child widgets accessors.
qtractorTrackList *qtractorTracks::trackList (void) const
{
	return m_pTrackList;
}

qtractorTrackTime *qtractorTracks::trackTime (void) const
{
	return m_pTrackTime;
}

qtractorTrackView *qtractorTracks::trackView (void) const
{
	return m_pTrackView;
}


// Horizontal zoom factor.
void qtractorTracks::horizontalZoomStep ( int iZoomStep )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
		
	int iHorizontalZoom = pSession->horizontalZoom() + iZoomStep;
	if (iHorizontalZoom < ZoomMin)
		iHorizontalZoom = ZoomMin;
	else
	if (iHorizontalZoom > ZoomMax)
		iHorizontalZoom = ZoomMax;
	if (iHorizontalZoom == pSession->horizontalZoom())
		return;

	// Fix the ssession time scale zoom determinant.
	pSession->setHorizontalZoom(iHorizontalZoom);
	pSession->updateTimeScale();
	pSession->updateSession();
}


// Vertical zoom factor.
void qtractorTracks::verticalZoomStep ( int iZoomStep )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
		
	int iVerticalZoom = pSession->verticalZoom() + iZoomStep;
	if (iVerticalZoom < ZoomMin)
		iVerticalZoom = ZoomMin;
	else
	if (iVerticalZoom > ZoomMax)
		iVerticalZoom = ZoomMax;
	if (iVerticalZoom == pSession->verticalZoom())
		return;

	// Fix the session vertical view zoom.
	pSession->setVerticalZoom(iVerticalZoom);
}


// Zoom (view) mode.
void qtractorTracks::setZoomMode ( int iZoomMode )
{
	m_iZoomMode = iZoomMode;
}

int qtractorTracks::zoomMode (void) const
{
	return m_iZoomMode;
}


// Zoom view actuators.
void qtractorTracks::zoomIn (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	if (m_iZoomMode & ZoomHorizontal)
		horizontalZoomStep(+ ZoomStep);
	if (m_iZoomMode & ZoomVertical)
		verticalZoomStep(+ ZoomStep);

	zoomCenterPost(zc);
}

void qtractorTracks::zoomOut (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	if (m_iZoomMode & ZoomHorizontal)
		horizontalZoomStep(- ZoomStep);
	if (m_iZoomMode & ZoomVertical)
		verticalZoomStep(- ZoomStep);

	zoomCenterPost(zc);
}


void qtractorTracks::zoomReset (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	ZoomCenter zc;
	zoomCenterPre(zc);

	if (m_iZoomMode & ZoomHorizontal)
		horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
	if (m_iZoomMode & ZoomVertical)
		verticalZoomStep(ZoomBase - pSession->verticalZoom());

	zoomCenterPost(zc);
}


void qtractorTracks::horizontalZoomInSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(+ ZoomStep);
	zoomCenterPost(zc);
}

void qtractorTracks::horizontalZoomOutSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(- ZoomStep);
	zoomCenterPost(zc);
}

void qtractorTracks::verticalZoomInSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(+ ZoomStep);
	zoomCenterPost(zc);
}

void qtractorTracks::verticalZoomOutSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(- ZoomStep);
	zoomCenterPost(zc);
}

void qtractorTracks::viewZoomResetSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	ZoomCenter zc;
	zoomCenterPre(zc);

	// All zoom base are belong to us :)
	verticalZoomStep(ZoomBase - pSession->verticalZoom());
	horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
	zoomCenterPost(zc);
}


// Zoom centering prepare method.
// (usually before zoom change)
void qtractorTracks::zoomCenterPre ( ZoomCenter& zc ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	QWidget *pViewport = m_pTrackView->viewport();
	const QRect& rect = pViewport->rect();
	const QPoint& pos = pViewport->mapFromGlobal(QCursor::pos());
	if (rect.contains(pos)) {
		zc.x = pos.x();
		zc.y = 0; // pos.y();
	} else {
	#if 0
		zc.x = 0;
		zc.y = 0;
	#else
		zc.x = (rect.width() >> 1);
		zc.y = 0; // (rect.height() >> 1);
	#endif
	}

	int cx = m_pTrackView->contentsX();
	zc.frame = pSession->frameFromPixel(cx + zc.x);
}


// Zoom centering post methods.
// (usually after zoom change)
void qtractorTracks::zoomCenterPost ( const ZoomCenter& zc )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	int cx = pSession->pixelFromFrame(zc.frame);
	int cy = m_pTrackView->contentsY();

	if (cx > zc.x) cx -= zc.x; else cx = 0;
	if (cy > zc.y) cy -= zc.y; else cy = 0;

	// Update the dependant views...
	m_pTrackList->updateContentsHeight();
	m_pTrackView->updateContentsWidth();
	m_pTrackView->setContentsPos(cx, cy);
	m_pTrackView->updateContents();

	// Make its due...
	selectionChangeNotify();
}


// Update/sync integral contents from session tracks.
void qtractorTracks::updateContents ( bool bRefresh )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTracks::updateContents(%d)\n", int(bRefresh));
#endif

	// Update/sync from session tracks.
	int iRefresh = 0;
	if (bRefresh)
		++iRefresh;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		// Check if item is already on list
		if (m_pTrackList->trackRow(pTrack) < 0) {
			m_pTrackList->insertTrack(iTrack, pTrack);
			++iRefresh;
		}
		pTrack = pTrack->next();
		++iTrack;
	}

	// Update dependant views.
	if (iRefresh > 0) {
		m_pTrackView->updateContentsWidth();
		m_pTrackList->updateContentsHeight();
	//	m_pTrackView->setFocus();
	}
}


// Retrieves current (selected) track reference.
qtractorTrack *qtractorTracks::currentTrack (void) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

	qtractorTrack *pTrack = m_pTrackList->currentTrack();
	if (pTrack == NULL) {
		qtractorClip *pClip = m_pTrackView->currentClip();
		if (pClip)
			pTrack = pClip->track();
	}

	return pTrack;
}


// Retrieves current selected clip reference.
qtractorClip *qtractorTracks::currentClip (void) const
{
	return m_pTrackView->currentClip();
}


// Edit/create a brand new clip.
bool qtractorTracks::newClip (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Create on current track, or take the first...
	qtractorTrack *pTrack = currentTrack();
	if (pTrack == NULL)
		pTrack = pSession->tracks().first();
	if (pTrack == NULL)
		return false;

	// Create the clip prototype...
	qtractorClip *pClip = NULL;
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		pClip = new qtractorAudioClip(pTrack);
		break;
	case qtractorTrack::Midi:
		pClip = new qtractorMidiClip(pTrack);
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Correct so far?
	if (pClip == NULL)
		return false;

	// Set initial default clip parameters...
	const unsigned long iClipStart = pSession->editHead();
	pClip->setClipStart(iClipStart);
	m_pTrackView->ensureVisibleFrame(iClipStart);

	// Special for MIDI clips, which already have it's own editor,
	// we'll add and start a blank one right-away...
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Set initial clip length...
		if (pSession->editTail() > pSession->editHead()) {
			pClip->setClipLength(pSession->editTail() - pSession->editHead());
		} else {
			pClip->setClipLength(pSession->frameFromTick(
				pSession->ticksPerBeat() * pSession->beatsPerBar()));
		}
		// Proceed to setup the MDII clip properly...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			// Create a clip filename from scratch...
			const QString& sFilename
				= pSession->createFilePath(pTrack->trackName(), "mid");
			// Create the SMF for good...
			if (pMidiClip->createMidiFile(sFilename)) {
				// Add that to regular files...
				pMainForm->addMidiFile(pClip->filename());
				// Insert the clip right away...
				qtractorClipCommand *pClipCommand
					= new qtractorClipCommand(tr("new clip"));
				pClipCommand->addClip(pClip, pTrack);
				pSession->execute(pClipCommand);
				// Just start the MIDI editor on it...
				return pClip->startEditor(pMainForm);
			}
		}
	}

	// Then ask user to refine clip properties...
	qtractorClipForm clipForm(pMainForm);
	clipForm.setClip(pClip, true);
	if (!clipForm.exec()) {
		delete pClip;
		return false;
	}

	// Done.
	return true;
}


// Edit given(current) clip.
bool qtractorTracks::editClip ( qtractorClip *pClip )
{
	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// All else hasn't fail.
	return pClip->startEditor(pMainForm);
}


// Unlink given(current) clip.
bool qtractorTracks::unlinkClip ( qtractorClip *pClip )
{
	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorMidiClip *pMidiClip
		= static_cast<qtractorMidiClip *> (pClip);
	if (pMidiClip == NULL)
		return false;

	if (!pMidiClip->isHashLinked())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Have a new filename revision...
	const QString& sFilename
		= pMidiClip->createFilePathRevision(true);

	// Save/replace the clip track...
	qtractorMidiFile::saveCopyFile(sFilename,
		pMidiClip->filename(),
		pMidiClip->trackChannel(),
		pMidiClip->format(),
		pMidiClip->sequence(),
		pSession->timeScale(),
		pSession->tickFromFrame(pMidiClip->clipStart()));

	// Now, we avoid the linked/ref-counted instances...
	pSession->files()->removeClipItem(qtractorFileList::Midi, pMidiClip);
	pMidiClip->setFilename(sFilename);
	pMidiClip->setDirty(false);
	pMidiClip->unlinkHashData();
	pMidiClip->updateEditor(true);
	pSession->files()->addClipItem(qtractorFileList::Midi, pMidiClip, true);

	// HACK: This operation is so important that
	// it surely deserves being in the front page...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->addMidiFile(sFilename);

	return true;
}


// Split given(current) clip.
bool qtractorTracks::splitClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	const unsigned long iPlayHead  = pSession->playHead();

	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL) {
		qtractorTrack *pTrack = m_pTrackList->currentTrack();
		if (pTrack) {
			pClip = pTrack->clips().first();
			while (pClip && iPlayHead > pClip->clipStart() + pClip->clipLength())
				pClip = pClip->next();
		}
	}

	if (pClip == NULL)
		return false;

	if (!pClip->queryEditor())
		return false;

	const unsigned long iClipStart = pClip->clipStart();
	const unsigned long iClipEnd   = iClipStart + pClip->clipLength();
	if (iClipStart >= iPlayHead || iPlayHead >= iClipEnd)
		return false;

	m_pTrackView->ensureVisibleFrame(iPlayHead);

	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("split clip"));

	// Shorten old right...
	const unsigned long iClipOffset = pClip->clipOffset();
	pClipCommand->resizeClip(pClip,
		iClipStart, iClipOffset, iPlayHead - iClipStart);
	// Add left clone...
	qtractorClip *pNewClip = m_pTrackView->cloneClip(pClip);
	if (pNewClip) {
		pNewClip->setClipStart(iPlayHead);
		pNewClip->setClipOffset(iClipOffset + iPlayHead - iClipStart);
		pNewClip->setClipLength(iClipEnd - iPlayHead);
		pNewClip->setFadeOutLength(pClip->fadeOutLength());
		pClipCommand->addClip(pNewClip, pNewClip->track());
	}

	// That's it...
	return pSession->execute(pClipCommand);
}


// Audio clip normalize callback.
struct audioClipNormalizeData
{	// Ctor.
	audioClipNormalizeData(unsigned short iChannels)
		: count(0), channels(iChannels), max(0.0f) {};
	// Members.
	unsigned int count;
	unsigned short channels;
	float max;
};

static void audioClipNormalize (
	float **ppFrames, unsigned int iFrames, void *pvArg )
{
	audioClipNormalizeData *pData
		= static_cast<audioClipNormalizeData *> (pvArg);

	for (unsigned short i = 0; i < pData->channels; ++i) {
		float *pFrames = ppFrames[i];
		for (unsigned int n = 0; n < iFrames; ++n) {
			float fSample = *pFrames++;
			if (fSample < 0.0f) // Take absolute value...
				fSample = -(fSample);
			if (pData->max < fSample)
				pData->max = fSample;
		}
	}

	if (++(pData->count) > 100) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			QProgressBar *pProgressBar = pMainForm->progressBar();
			pProgressBar->setValue(pProgressBar->value() + iFrames);
		}
		qtractorSession::stabilize();
		pData->count = 0;
	}
}


// MIDI clip normalize callback.
static void midiClipNormalize (
	qtractorMidiSequence *pSeq, void *pvArg )
{
	unsigned char *pMax = (unsigned char *) (pvArg);
	for (qtractorMidiEvent *pEvent = pSeq->events().first();
			pEvent; pEvent = pEvent->next()) {
		if (pEvent->type() == qtractorMidiEvent::NOTEON &&
			*pMax < pEvent->velocity()) {
			*pMax = pEvent->velocity();
		}
	}
}


// Normalize given(current) clip.
bool qtractorTracks::normalizeClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Make it as an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip normalize"));

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Multiple clip selection...
	if (isClipSelected()) {
		qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
		const qtractorClipSelect::ItemList& items = pClipSelect->items();
		qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
		const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
		for ( ; iter != iter_end; ++iter) {
			// Make sure it's legal selection...
			pClip = iter.key();
			if (pClip->track() && pClip->isClipSelected())
				normalizeClipCommand(pClipCommand, pClip);
		}
	}	// Single, current clip instead?
	else normalizeClipCommand(pClipCommand, pClip);

	QApplication::restoreOverrideCursor();

	// Check if valid...
	if (pClipCommand->isEmpty()) {
		delete pClipCommand;
		return false;
	}

	// That's it...
	return pSession->execute(pClipCommand);

}


bool qtractorTracks::normalizeClipCommand (
	qtractorClipCommand *pClipCommand, qtractorClip *pClip )
{
	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();

	unsigned long iOffset = 0;
	unsigned long iLength = pClip->clipLength();

	if (pClip->isClipSelected()) {
		iOffset = pClip->clipSelectStart() - pClip->clipStart();
		iLength = pClip->clipSelectEnd() - pClip->clipSelectStart();
	}

	// Default non-normalized setting...
	float fGain = pClip->clipGain();

	if (pTrack->trackType() == qtractorTrack::Audio) {
		// Normalize audio clip...
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (pClip);
		if (pAudioClip == NULL)
			return false;
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pTrack->outputBus());
		if (pAudioBus == NULL)
			return false;
		QProgressBar *pProgressBar = NULL;
		if (pMainForm)
			pProgressBar = pMainForm->progressBar();
		if (pProgressBar) {
			pProgressBar->setRange(0, iLength / 100);
			pProgressBar->reset();
			pProgressBar->show();
		}
		audioClipNormalizeData data(pAudioBus->channels());
		pAudioClip->clipExport(audioClipNormalize, &data, iOffset, iLength);
		if (data.max > 0.01f && data.max < 1.1f)
			fGain /= data.max;
		if (pProgressBar)
			pProgressBar->hide();
	}
	else
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Normalize MIDI clip...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip == NULL)
			return false;
		unsigned char max = 0;
		pMidiClip->clipExport(midiClipNormalize, &max, iOffset, iLength);
		if (max > 0x0c && max < 0x7f)
			fGain *= (127.0f / float(max));
	}

	// Make it as an undoable command...
	pClipCommand->gainClip(pClip, fGain);

	// That's it...
	return true;
}


// Execute tool on a given(current) MIDI clip.
bool qtractorTracks::executeClipTool ( int iTool, qtractorClip *pClip )
{
	qtractorMidiToolsForm toolsForm(this);
	toolsForm.setToolIndex(iTool);
	if (!toolsForm.exec())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;
	
	// Make it as an undoable named command...
	QString sTool;
	switch (iTool) {
	case 0:	sTool = tr("quantize");  break;
	case 1:	sTool = tr("transpose"); break;
	case 2:	sTool = tr("normalize"); break;
	case 3:	sTool = tr("randomize"); break;
	case 4:	sTool = tr("resize");    break;
	case 5:	sTool = tr("rescale");   break;
	case 6:	sTool = tr("timeshift"); break;
	}

	qtractorClipToolCommand *pClipToolCommand
		= new qtractorClipToolCommand(sTool);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Multiple clip selection...
	if (isClipSelected()) {
		qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
		const qtractorClipSelect::ItemList& items = pClipSelect->items();
		qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
		const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
		for ( ; iter != iter_end; ++iter) {
			// Make sure it's legal selection...
			qtractorClip *pClip = iter.key();
			if (pClip->track() && pClip->isClipSelected())
				executeClipToolCommand(pClipToolCommand, pClip, &toolsForm);
		}
	}	// Single, current clip instead?
	else executeClipToolCommand(pClipToolCommand, pClip, &toolsForm);

	QApplication::restoreOverrideCursor();

	// Check if valid...
	if (pClipToolCommand->isEmpty()) {
		delete pClipToolCommand;
		return false;
	}

	// That's it...
	return pSession->execute(pClipToolCommand);
}


bool qtractorTracks::executeClipToolCommand (
	qtractorClipToolCommand *pClipToolCommand, qtractorClip *pClip,
	qtractorMidiToolsForm *pMidiToolsForm )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	qtractorTrack *pTrack = pClip->track();
	if (pTrack == NULL)
		return false;
	if (pTrack->trackType() != qtractorTrack::Midi)
		return false;

	qtractorMidiClip *pMidiClip = static_cast<qtractorMidiClip *> (pClip);
	if (pMidiClip == NULL)
		return false;

	unsigned long iOffset = 0;
	unsigned long iLength = pClip->clipLength();

	if (pClip->isClipSelected()) {
		iOffset = pClip->clipSelectStart() - pClip->clipStart();
		iLength = pClip->clipSelectEnd() - pClip->clipSelectStart();
	}

	qtractorMidiSequence *pSeq = pMidiClip->sequence();
	const unsigned long iTimeOffset = pSeq->timeOffset();

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pClip->clipStart());
	const unsigned long t0 = pNode->tickFromFrame(pClip->clipStart());

	unsigned long f1 = pClip->clipStart() + pClip->clipOffset() + iOffset;
	pNode = cursor.seekFrame(f1);
	const unsigned long t1 = pNode->tickFromFrame(f1);
	unsigned long iTimeStart = t1 - t0;
	iTimeStart = (iTimeStart > iTimeOffset ? iTimeStart - iTimeOffset : 0);
	pNode = cursor.seekFrame(f1 += iLength);
	const unsigned long iTimeEnd = iTimeStart + pNode->tickFromFrame(f1) - t1;

	// Emulate an user-made selection...
	qtractorMidiEditSelect select;
	const QRect rect; // Dummy event rectangle.	
	for (qtractorMidiEvent *pEvent = pSeq->events().first();
			pEvent; pEvent = pEvent->next()) {
		const unsigned long iTime = pEvent->time();
		if (iTime >= iTimeStart && iTime < iTimeEnd) {
			select.addItem(pEvent, rect, rect);
		}
	}

	// Add new edit command from tool...
	pClipToolCommand->addMidiEditCommand(
		pMidiToolsForm->editCommand(pMidiClip, &select,
			pSession->tickFromFrame(pClip->clipStart())));
			
	// Must be brand new revision...
	pMidiClip->setRevision(0);

	// That's it...
	return true;
}


// Import (audio) clip(s) into current track...
bool qtractorTracks::importClips (
	QStringList files, unsigned long iClipStart )
{
	// Have we some?
	if (files.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Create on current track, or take the first...
	qtractorTrack *pTrack = currentTrack();
	if (pTrack == NULL)
		pTrack = pSession->tracks().first();
	if (pTrack == NULL) // || pTrack->trackType() != qtractorTrack::Audio)
		return addAudioTracks(files, iClipStart);

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	qtractorClipCommand *pImportClipCommand
		= new qtractorClipCommand(tr("clip import"));

	// For each one of those files...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		switch (pTrack->trackType()) {
		case qtractorTrack::Audio: {
			// Add the audio clip at once...
			qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
			pAudioClip->setFilename(sPath);
			pAudioClip->setClipStart(iClipStart);
			// Redundant but necessary for multi-clip
			// concatenation, as we only know the actual
			// audio clip length after opening it...
			if (iter.hasNext()) {
				pAudioClip->open();
				iClipStart += pAudioClip->clipLength();
			}
			// Will add the new clip into session on command execute...
			pImportClipCommand->addClip(pAudioClip, pTrack);
			// Don't forget to add this one to local repository.
			if (pMainForm) {
				pMainForm->addAudioFile(sPath);
				// Log this successful import operation...
				sDescription += tr("Audio file import \"%1\" on %2 %3.\n")
					.arg(QFileInfo(sPath).fileName())
					.arg(QDate::currentDate().toString("MMM dd yyyy"))
					.arg(QTime::currentTime().toString("hh:mm:ss"));
				pMainForm->appendMessages(
					tr("Audio file import: \"%1\".").arg(sPath));
			}
			break;
		}
		case qtractorTrack::Midi: {
			// We'll be careful and pre-open the SMF header here...
			qtractorMidiFile file;
			if (file.open(sPath)) {
				// Depending of SMF format...
				int iTrackChannel = pTrack->midiChannel();
				if (file.format() == 1)
					++iTrackChannel;
				// Add the MIDI clip at once...
				qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
				pMidiClip->setFilename(sPath);
				pMidiClip->setTrackChannel(iTrackChannel);
				pMidiClip->setClipStart(iClipStart);
				// Redundant but necessary for multi-clip
				// concatenation, as we only know the actual
				// MIDI clip length after opening it...
				if (iter.hasNext()) {
					pMidiClip->open();
					iClipStart += pMidiClip->clipLength();
				}
				// Will add the new clip into session on command execute...
				pImportClipCommand->addClip(pMidiClip, pTrack);
				// Don't forget to add this one to local repository.
				if (pMainForm) {
					pMainForm->addMidiFile(sPath);
					// Log this successful import operation...
					sDescription += tr("MIDI file import \"%1\""
						" track-channel %2 on %3 %4.\n")
						.arg(QFileInfo(sPath).fileName()).arg(iTrackChannel)
						.arg(QDate::currentDate().toString("MMM dd yyyy"))
						.arg(QTime::currentTime().toString("hh:mm:ss"));
					pMainForm->appendMessages(
						tr("MIDI file import: \"%1\", track-channel: %2.")
						.arg(sPath).arg(iTrackChannel));
				}
			}
			break;
		}
		default:
			break;
		}
	}

	// Log to session (undoable by import-track command)...
	pSession->setDescription(sDescription);

	// Done.
	return pSession->execute(pImportClipCommand);
}


// Export selected clips.
bool qtractorTracks::exportClips (void)
{
	return mergeExportClips(NULL);
}


// Merge selected clips.
bool qtractorTracks::mergeClips (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Make it as an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip merge"));

	const bool bResult = mergeExportClips(pClipCommand);
	
	// Have we failed the command prospect?
	if (!bResult || pClipCommand->isEmpty()) {
		delete pClipCommand;
		return false;
	}

	return pSession->execute(pClipCommand);
}


// Merge/export selected clips.
bool qtractorTracks::mergeExportClips ( qtractorClipCommand *pClipCommand )
{
	// Multiple clip selection:
	// - make sure we have at least 2 clips to merge...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	if (pClipSelect->items().count() < 1)
		return false;

	// Should be one single track...
	qtractorTrack *pTrack = pClipSelect->singleTrack();
	if (pTrack == NULL)
		return false;

	// Dispatch to specialized method...
	bool bResult = false;
	switch (pTrack->trackType()) {
	case qtractorTrack::Audio:
		bResult = mergeExportAudioClips(pClipCommand);
		break;
	case qtractorTrack::Midi:
		bResult = mergeExportMidiClips(pClipCommand);
		break;
	case qtractorTrack::None:
	default:
		break;
	}

	// Done most.
	return bResult;
}


// Audio clip buffer merge/export item.
struct audioClipBufferItem
{
	// Constructor.
	audioClipBufferItem(qtractorClip *pClip,
		qtractorAudioBufferThread *pSyncThread,
		unsigned short iChannels,
		unsigned int iSampleRate)
		: clip(static_cast<qtractorAudioClip *> (pClip))
	{
		buff = new qtractorAudioBuffer(pSyncThread, iChannels, iSampleRate);
		buff->setOffset(clip->clipOffset());
		buff->setLength(clip->clipLength());
		buff->setTimeStretch(clip->timeStretch());
		buff->setPitchShift(clip->pitchShift());
		buff->open(clip->filename());
		buff->syncExport();
	}

	// Destructor.
	~audioClipBufferItem()
		{ if (buff) delete buff; }

	// Members.
	qtractorAudioClip   *clip;
	qtractorAudioBuffer *buff;
};


// Merge/export selected(audio) clips.
bool qtractorTracks::mergeExportAudioClips ( qtractorClipCommand *pClipCommand )
{
	// Should be one single MIDI track...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	qtractorTrack *pTrack = pClipSelect->singleTrack();
	if (pTrack == NULL)
		return false;
	if (pTrack->trackType() != qtractorTrack::Audio)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (pTrack->outputBus());
	if (pAudioBus == NULL)
		return false;

	const QString& sExt = qtractorAudioFileFactory::defaultExt();
	const QString& sTitle  = tr("Merge/Export Audio Clip") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Audio files (*.%1)").arg(sExt); 
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to save...
	QFileDialog::Options options = 0;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	QString sFilename = QFileDialog::getSaveFileName(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), sExt), sFilter, NULL, options);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), sExt), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sAudioDir));
		fileDialog.setSidebarUrls(urls);
		if (pOptions->bDontUseNativeDialogs)
			fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	}
	// Show dialog...
	if (!fileDialog.exec())
		return false;
	QString sFilename = fileDialog.selectedFiles().first();
#endif
	if (sFilename.isEmpty())
		return false;
	if (QFileInfo(sFilename).suffix().isEmpty())
		sFilename += '.' + sExt;

	// Should take sometime...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	qtractorAudioFile *pAudioFile
		= qtractorAudioFileFactory::createAudioFile(sFilename,
			pAudioBus->channels(), pSession->sampleRate());
	if (pAudioFile == NULL) {
		QApplication::restoreOverrideCursor();
		return false;
	}

	// Open the file for writing...
	if (!pAudioFile->open(sFilename, qtractorAudioFile::Write)) {
		QApplication::restoreOverrideCursor();
		delete pAudioFile;
		return false;
	}

	// Start logging...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->appendMessages(
			tr("Audio clip merge/export: \"%1\" started...")
			.arg(sFilename));
	}

	// Multiple clip selection...
	const qtractorClipSelect::ItemList& items = pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();

	const unsigned short iChannels = pAudioBus->channels();
	const unsigned int iSampleRate = pSession->sampleRate();

	// Multi-selection extents (in frames)...
	QList<audioClipBufferItem *> list;
	unsigned long iSelectStart = pSession->sessionEnd();
	unsigned long iSelectEnd = pSession->sessionStart();
	for ( ; iter != iter_end; ++iter) {
		qtractorClip *pClip = iter.key();
		qtractorTrack *pTrack = pClip->track();
		// Make sure it's a legal selection...
		if (pTrack && pClip->isClipSelected()) {
			qtractorAudioBufferThread *pSyncThread = pTrack->syncThread();
			if (iSelectStart > pClip->clipSelectStart())
				iSelectStart = pClip->clipSelectStart();
			if (iSelectEnd < pClip->clipSelectEnd())
				iSelectEnd = pClip->clipSelectEnd();
			list.append(new audioClipBufferItem(
				pClip, pSyncThread, iChannels, iSampleRate));
		}
	}

	// A progress indication might be friendly...
	QProgressBar *pProgressBar = NULL;
	if (pMainForm)
		pProgressBar = pMainForm->progressBar();
	if (pProgressBar) {
		pProgressBar->setRange(0, (iSelectEnd - iSelectStart) / 100);
		pProgressBar->reset();
		pProgressBar->show();
	}

	// Allocate merge audio scratch buffer...
	const unsigned int iBufferSize = pSession->audioEngine()->bufferSize();
	unsigned short i;
	float **ppFrames = new float * [iChannels];
	for (i = 0; i < iChannels; ++i)
		ppFrames[i] = new float[iBufferSize];

	// Setup clip buffers...
	QListIterator<audioClipBufferItem *> it(list);
	while (it.hasNext()) {
		audioClipBufferItem *pItem = it.next();
		qtractorAudioClip   *pClip = pItem->clip;
		qtractorAudioBuffer *pBuff = pItem->buff;
		// Almost similar to qtractorAudioClip::process(0)...
		const unsigned long iClipStart = pClip->clipStart();
		const unsigned long iClipEnd   = iClipStart + pClip->clipLength();
		unsigned long iOffset = 0;
		if (iSelectStart > iClipStart && iSelectStart < iClipEnd)
			iOffset = iSelectStart - iClipStart;
		// Make it initially filled...
		pBuff->seek(iOffset);
	//	pBuff->syncExport();
	}

	// Loop-merge audio clips...
	unsigned long iFrameStart = iSelectStart;
	unsigned long iFrameEnd = iFrameStart + iBufferSize;
	int count = 0;

	// Loop until EOF...
	while (iFrameStart < iSelectEnd && iFrameEnd > iSelectStart) {
		// Zero-silence on scratch buffers...
		for (i = 0; i < iChannels; ++i)
			::memset(ppFrames[i], 0, iBufferSize * sizeof(float));
		// Merge clips in window...
		it.toFront();
		while (it.hasNext()) {
			audioClipBufferItem *pItem = it.next();
			qtractorAudioClip   *pClip = pItem->clip;
			qtractorAudioBuffer *pBuff = pItem->buff;
			// Should force sync now and then...
			if ((count % 33) == 0) pBuff->syncExport();
			// Quite similar to qtractorAudioClip::process()...
			const unsigned long iClipStart = pClip->clipStart();
			const unsigned long iClipEnd   = iClipStart + pClip->clipLength();;
			if (iFrameStart < iClipStart && iFrameEnd > iClipStart) {
				const unsigned long iOffset = iFrameEnd - iClipStart;
				while (!pBuff->inSync(0, 0))
					pBuff->syncExport();
				pBuff->readMix(ppFrames, iOffset,
					iChannels, iClipStart - iFrameStart, pClip->gain(iOffset));
			}
			else
			if (iFrameStart >= iClipStart && iFrameStart < iClipEnd) {
				const unsigned long iFrame = iFrameStart - iClipStart;
				while (!pBuff->inSync(iFrame, iFrame))
					pBuff->syncExport();
				pBuff->readMix(ppFrames, iBufferSize,
					iChannels, 0, pClip->gain(iFrameEnd - iClipStart));
			}
		}
		// Actually write to merge audio file;
		// - check for last incomplete block...
		if (iFrameEnd > iSelectEnd)
			pAudioFile->write(ppFrames, iBufferSize - (iFrameEnd - iSelectEnd));
		else
			pAudioFile->write(ppFrames, iBufferSize);
		// Advance to next buffer...
		iFrameStart = iFrameEnd;
		iFrameEnd = iFrameStart + iBufferSize;
		if (++count > 100 && pProgressBar) {
			pProgressBar->setValue(pProgressBar->value() + iBufferSize);
			qtractorSession::stabilize();
			count = 0;
		}
	}

	for (i = 0; i < iChannels; ++i)
		delete ppFrames[i];
	delete [] ppFrames;

	qDeleteAll(list);
	list.clear();

	// Close and free it up...
	pAudioFile->close();
	delete pAudioFile;

	if (pProgressBar)
		pProgressBar->hide();

	// Stop logging...
	if (pMainForm) {
		pMainForm->addAudioFile(sFilename);
		pMainForm->appendMessages(
			tr("Audio clip merge/export: \"%1\" complete.")
			.arg(sFilename));
	}

	// The resulting merge comands, if any...
	if (pClipCommand) {
		iter = items.constBegin();
		for ( ; iter != iter_end; ++iter) {
			qtractorClip *pClip = iter.key();
			// Clip parameters.
			const unsigned long iClipStart  = pClip->clipStart();
			const unsigned long iClipOffset = pClip->clipOffset();
			const unsigned long iClipLength = pClip->clipLength();
			const unsigned long iClipEnd    = iClipStart + iClipLength;
			// Determine and keep clip regions...
			if (iSelectStart > iClipStart) {
				// -- Left clip...
				pClipCommand->resizeClip(pClip,
					iClipStart,
					iClipOffset,
					iSelectStart - iClipStart);
				// Done, left clip.
			}
			else
			if (iSelectEnd < iClipEnd) {
				// -- Right clip...
				pClipCommand->resizeClip(pClip,
					iSelectEnd,
					iClipOffset + (iSelectEnd - iClipStart),
					iClipEnd - iSelectEnd);
				// Done, right clip.
			} else {
				// -- Inner clip...
				pClipCommand->removeClip(pClip);
				// Done, inner clip.
			}
		}
		// Set the resulting clip command...
		qtractorAudioClip *pNewClip = new qtractorAudioClip(pTrack);
		pNewClip->setClipStart(iSelectStart);
		pNewClip->setClipLength(iSelectEnd - iSelectStart);
		pNewClip->setFilename(sFilename);
		pClipCommand->addClip(pNewClip, pTrack);
	}

	// Almost done with it...
	QApplication::restoreOverrideCursor();

	// That's it...
	return true;
}


// Merge/export selected(MIDI) clips.
bool qtractorTracks::mergeExportMidiClips ( qtractorClipCommand *pClipCommand )
{
	// Should be one single MIDI track...
	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	qtractorTrack *pTrack = pClipSelect->singleTrack();
	if (pTrack == NULL)
		return false;
	if (pTrack->trackType() != qtractorTrack::Midi)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Merge MIDI Clip filename requester...
	const QString  sExt("mid");
	const QString& sTitle  = tr("Merge/Export MIDI Clip") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt); 
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to save...
	QFileDialog::Options options = 0;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	QString sFilename = QFileDialog::getSaveFileName(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), sExt), sFilter, NULL, options);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(this, sTitle,
		pSession->createFilePath(pTrack->trackName(), sExt), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sMidiDir));
		fileDialog.setSidebarUrls(urls);
		if (pOptions->bDontUseNativeDialogs)
			fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	}
	// Show dialog...
	if (!fileDialog.exec())
		return false;
	QString sFilename = fileDialog.selectedFiles().first();
#endif
	if (sFilename.isEmpty())
		return false;
	if (QFileInfo(sFilename).suffix().isEmpty())
		sFilename += '.' + sExt;

	// Should take sometime...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Create SMF...
	qtractorMidiFile file;
	if (!file.open(sFilename, qtractorMidiFile::Write)) {
		QApplication::restoreOverrideCursor();
		return false;
	}

	// Write SMF header...
	const unsigned short iTicksPerBeat = pSession->ticksPerBeat();
	const unsigned short iFormat = qtractorMidiClip::defaultFormat();
	const unsigned short iTracks = (iFormat == 0 ? 1 : 2);
	if (!file.writeHeader(iFormat, iTracks, iTicksPerBeat)) {
		QApplication::restoreOverrideCursor();
		return false;
	}

	// Start logging...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->appendMessages(
			tr("MIDI clip merge/export: \"%1\" started...")
			.arg(sFilename));
	}

	// Multiple clip selection...
	const qtractorClipSelect::ItemList& items = pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();

	// Multi-selection extents (in frames)...
	unsigned long iSelectStart = pSession->sessionEnd();
	unsigned long iSelectEnd = pSession->sessionStart();
	for ( ; iter != iter_end; ++iter) {
		qtractorClip *pClip = iter.key();
		// Make sure it's a legal selection...
		if (pClip->track() && pClip->isClipSelected()) {
			if (iSelectStart > pClip->clipSelectStart())
				iSelectStart = pClip->clipSelectStart();
			if (iSelectEnd < pClip->clipSelectEnd())
				iSelectEnd = pClip->clipSelectEnd();
		}
	}

	// Multi-selection extents (in ticks)...
	const unsigned long iTimeStart = pSession->tickFromFrame(iSelectStart);
	const unsigned long iTimeEnd   = pSession->tickFromFrame(iSelectEnd);

	// Set proper tempo map...
	if (file.tempoMap()) {
		file.tempoMap()->fromTimeScale(
			pSession->timeScale(),
			pSession->tickFromFrame(iTimeStart));
	}

	// Setup track (SMF format 1).
	if (iFormat == 1)
		file.writeTrack(NULL);

	// Setup merge sequence...
	qtractorMidiSequence seq(pTrack->trackName(), 1, iTicksPerBeat);
	seq.setChannel(pTrack->midiChannel());
	seq.setBank(pTrack->midiBank());
	seq.setProg(pTrack->midiProg());

	// The merge...
	iter = items.constBegin();
	for ( ; iter != iter_end; ++iter) {
		qtractorClip *pClip = iter.key();
		// Make sure it's a legal selection...
		if (pClip->track() && pClip->isClipSelected()) {
			// Clip parameters.
			const unsigned long iClipStart  = pClip->clipStart();
			const unsigned long iClipOffset = pClip->clipOffset();
			const unsigned long iClipLength = pClip->clipLength();
			const unsigned long iClipEnd    = iClipStart + iClipLength;
			// Determine and keep clip regions...
			if (pClipCommand) {
				if (iSelectStart > iClipStart) {
					// -- Left clip...
					pClipCommand->resizeClip(pClip,
						iClipStart,
						iClipOffset,
						iSelectStart - iClipStart);
					// Done, left clip.
				}
				else
				if (iSelectEnd < iClipEnd) {
					// -- Right clip...
					pClipCommand->resizeClip(pClip,
						iSelectEnd,
						iClipOffset + (iSelectEnd - iClipStart),
						iClipEnd - iSelectEnd);
					// Done, right clip.
				} else {
					// -- Inner clip...
					pClipCommand->removeClip(pClip);
					// Done, inner clip.
				}
			}
			// Do the MIDI merge, itself...
			qtractorMidiClip *pMidiClip
				= static_cast<qtractorMidiClip *> (pClip);
			if (pMidiClip) {
				const unsigned long iTimeClip
					= pSession->tickFromFrame(pClip->clipStart());
				const unsigned long iTimeOffset = iTimeClip - iTimeStart;
				// For each event...
				qtractorMidiEvent *pEvent
					= pMidiClip->sequence()->events().first();
				while (pEvent && iTimeClip + pEvent->time() < iTimeStart)
					pEvent = pEvent->next();
				while (pEvent && iTimeClip + pEvent->time() < iTimeEnd) {
					qtractorMidiEvent *pNewEvent
						= new qtractorMidiEvent(*pEvent);
					pNewEvent->setTime(iTimeOffset + pEvent->time());
					if (pNewEvent->type() == qtractorMidiEvent::NOTEON) {
						const unsigned long iTimeEvent = iTimeClip + pEvent->time();
						const float fGain = pMidiClip->gain(
							pSession->frameFromTick(iTimeEvent)
							- pClip->clipStart());
						pNewEvent->setVelocity((unsigned char)
							(fGain * float(pEvent->velocity())) & 0x7f);
						if (iTimeEvent + pEvent->duration() > iTimeEnd)
							pNewEvent->setDuration(iTimeEnd - iTimeEvent);
					}
					seq.insertEvent(pNewEvent);
					pEvent = pEvent->next();
				}
			}
		}
	}

	// Write the track and close SMF...
	file.writeTrack(&seq);
	file.close();

	// Stop logging...
	if (pMainForm) {
		pMainForm->addMidiFile(sFilename);
		pMainForm->appendMessages(
			tr("MIDI clip merge/export: \"%1\" complete.")
			.arg(sFilename));
	}

	// Set the resulting clip command...
	if (pClipCommand) {
		qtractorMidiClip *pNewClip = new qtractorMidiClip(pTrack);
		pNewClip->setClipStart(iSelectStart);
		pNewClip->setClipLength(iSelectEnd - iSelectStart);
		pNewClip->setFilename(sFilename);
		pNewClip->setTrackChannel(1);
		pClipCommand->addClip(pNewClip, pTrack);
	}

	// Almost done with it...
	QApplication::restoreOverrideCursor();

	// That's it...
	return true;
}


// Edit/loop-range from current clip settlers.
bool qtractorTracks::rangeClip ( qtractorClip *pClip )
{
	return rangeClipEx(pClip, false);
}

bool qtractorTracks::loopClip ( qtractorClip *pClip )
{
	return rangeClipEx(pClip, true);
}

bool qtractorTracks::rangeClipEx ( qtractorClip *pClip, bool bLoopSet )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	unsigned long iEditHead = 0;
	unsigned long iEditTail = 0;

	// Multiple clip selection...
	if (pClip == NULL && isClipSelected()) {
		// Multi-selection extents (in frames)...
		iEditHead = pSession->sessionEnd();
		iEditTail = pSession->sessionStart();
		qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
		const qtractorClipSelect::ItemList& items = pClipSelect->items();
		qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
		const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();
		for ( ; iter != iter_end; ++iter) {
			pClip = iter.key();
			// Make sure it's a legal selection...
			if (pClip->isClipSelected()) {
				const unsigned long iClipStart = pClip->clipStart();
				const unsigned long iClipEnd   = iClipStart + pClip->clipLength();
				if (iEditHead > iClipStart)
					iEditHead = iClipStart;
				if (iEditTail < iClipEnd)
					iEditTail = iClipEnd;
			}
		}
	} else {
		if (pClip == NULL)
			pClip = m_pTrackView->currentClip();
		if (pClip) {
			iEditHead = pClip->clipStart();
			iEditTail = iEditHead + pClip->clipLength();
		}
	}

	pSession->setEditHead(iEditHead);
	pSession->setEditTail(iEditTail);

	if (bLoopSet) {
		return pSession->execute(
			new qtractorSessionLoopCommand(pSession, iEditHead, iEditTail));
	}

	selectionChangeNotify();
	return true;
}


// Adjust current tempo from clip selection or interactive tapping...
bool qtractorTracks::tempoClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	unsigned long iRangeStart  = pSession->editHead();
	unsigned long iRangeLength = pSession->editTail() - iRangeStart;

	if (pClip == NULL)
		pClip = currentClip();
	if (pClip) {
		if (pClip->isClipSelected()) {
			iRangeStart  = pClip->clipSelectStart();
			iRangeLength = pClip->clipSelectEnd() - iRangeStart;
		} else {
			iRangeStart  = pClip->clipStart();
			iRangeLength = pClip->clipLength();
		}
	}

	qtractorTempoAdjustForm form(this);
	form.setRangeStart(iRangeStart);
	form.setRangeLength(iRangeLength);
	if (!form.exec())
		return false;

	// Avoid automatic time stretching option for audio clips...
	const bool bAutoTimeStretch = pSession->isAutoTimeStretch();
	pSession->setAutoTimeStretch(false);

	// Find appropriate node...
	qtractorTimeScale *pTimeScale = pSession->timeScale();
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(form.rangeStart());

	// Now, express the change as a undoable command...
	pSession->execute(
		new qtractorTimeScaleUpdateNodeCommand(pTimeScale, pNode->frame,
			form.tempo(), 2, form.beatsPerBar(), form.beatDivisor()));

	// Done.
	pSession->setAutoTimeStretch(bAutoTimeStretch);	
	pSession->setEditTail(pSession->editHead() + form.rangeLength());

	selectionChangeNotify();
	return true;
}


// Whether there's anything currently selected.
bool qtractorTracks::isSelected (void) const
{
	return isClipSelected() || isCurveSelected();
}


// Whether there's any clip currently selected.
bool qtractorTracks::isClipSelected (void) const
{
	return m_pTrackView->isClipSelected();
}


// Whether there's any curve/automation currently selected.
bool qtractorTracks::isCurveSelected (void) const
{
	return m_pTrackView->isCurveSelected();
}


// Whether there's a single track selection.
qtractorTrack *qtractorTracks::singleTrackSelected (void)
{
	return m_pTrackView->singleTrackSelected();
}


// Retrieve actual clip selection range.
void qtractorTracks::clipSelectedRange (
	unsigned long& iSelectStart,
	unsigned long& iSelectEnd ) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	iSelectStart = pSession->sessionEnd();
	iSelectEnd = pSession->sessionStart();

	qtractorClipSelect *pClipSelect = m_pTrackView->clipSelect();
	const qtractorClipSelect::ItemList& items = pClipSelect->items();
	qtractorClipSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorClipSelect::ItemList::ConstIterator& iter_end = items.constEnd();

	for ( ; iter != iter_end; ++iter) {
		qtractorClip *pClip = iter.key();
		qtractorTrack *pTrack = pClip->track();
		// Make sure it's a legal selection...
		if (pTrack && pClip->isClipSelected()) {
			if (iSelectStart > pClip->clipSelectStart())
				iSelectStart = pClip->clipSelectStart();
			if (iSelectEnd < pClip->clipSelectEnd())
				iSelectEnd = pClip->clipSelectEnd();
		}
	}
}


// Clipboard methods.
void qtractorTracks::cutClipboard (void)
{
	if (m_pTrackView->isCurveEdit())
		m_pTrackView->executeCurveSelect(qtractorTrackView::Cut);
	else
		m_pTrackView->executeClipSelect(qtractorTrackView::Cut);
}

void qtractorTracks::copyClipboard (void)
{
	if (m_pTrackView->isCurveEdit())
		m_pTrackView->executeCurveSelect(qtractorTrackView::Copy);
	else
		m_pTrackView->executeClipSelect(qtractorTrackView::Copy);
}

void qtractorTracks::pasteClipboard (void)
{
	m_pTrackView->pasteClipboard();
}


// Special paste/repeat prompt.
void qtractorTracks::pasteRepeatClipboard (void)
{
	qtractorPasteRepeatForm pasteForm(this);
	pasteForm.setRepeatPeriod(m_pTrackView->pastePeriod());
	if (pasteForm.exec()) {
		m_pTrackView->pasteClipboard(
			pasteForm.repeatCount(),
			pasteForm.repeatPeriod());
	}
}


// Delete current selection.
void qtractorTracks::deleteSelect (void)
{
	if (m_pTrackView->isCurveEdit())
		m_pTrackView->executeCurveSelect(qtractorTrackView::Delete);
	else
		m_pTrackView->executeClipSelect(qtractorTrackView::Delete);
}


// Split selection method.
void qtractorTracks::splitSelect (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Split);
}


// Select range interval between edit head and tail.
void qtractorTracks::selectEditRange ( bool bReset )
{
	if (m_pTrackView->isCurveEdit())
		m_pTrackView->selectCurveTrackRange(NULL, bReset);
	else
		m_pTrackView->selectClipTrackRange(NULL, bReset);
}


// Select all clips on current track.
void qtractorTracks::selectCurrentTrack ( bool bReset )
{
	qtractorTrack *pTrack = currentTrack();
	if (pTrack == NULL)
		return;

	if (m_pTrackView->isCurveEdit())
		m_pTrackView->selectCurveTrack(pTrack, bReset);
	else
		m_pTrackView->selectClipTrack(pTrack, bReset);
}


// Select all clips on current track range.
void qtractorTracks::selectCurrentTrackRange ( bool bReset )
{
	qtractorTrack *pTrack = currentTrack();
	if (pTrack == NULL)
		return;

	if (m_pTrackView->isCurveEdit())
		m_pTrackView->selectCurveTrackRange(pTrack, bReset);
	else
		m_pTrackView->selectClipTrackRange(pTrack, bReset);
}


// Select everything on all tracks.
void qtractorTracks::selectAll (void)
{
	if (m_pTrackView->isCurveEdit())
		m_pTrackView->selectCurveAll();
	else
		m_pTrackView->selectClipAll();
}


// Select nothing on all tracks.
void qtractorTracks::selectNone (void)
{
	m_pTrackView->clearSelect();

	selectionChangeNotify();
}


// Invert selection on all tracks and clips.
void qtractorTracks::selectInvert (void)
{
	if (m_pTrackView->isCurveEdit())
		m_pTrackView->selectCurveInvert();
	else
		m_pTrackView->selectClipInvert();
}


// Insertion method.
bool qtractorTracks::insertEditRange ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return false;

	unsigned long iInsertStart = pSession->editHead();
	unsigned long iInsertEnd   = pSession->editTail();

	if (iInsertStart >= iInsertEnd) {
		unsigned short iBar = pTimeScale->barFromFrame(iInsertStart);
		iInsertEnd = pTimeScale->frameFromBar(iBar + 1);
	}

	unsigned int iInsertOptions = qtractorEditRangeForm::None;
	iInsertOptions |= qtractorEditRangeForm::Clips;
	iInsertOptions |= qtractorEditRangeForm::Automation;

	if (pTrack == NULL) {
		qtractorEditRangeForm rangeForm(this);
		rangeForm.setWindowTitle(tr("Insert Range") + " - " QTRACTOR_TITLE);
		if (isClipSelected())
			clipSelectedRange(iInsertStart, iInsertEnd);
		rangeForm.setSelectionRange(iInsertStart, iInsertEnd);
		if (!rangeForm.exec())
			return false;
		iInsertStart = rangeForm.rangeStart();
		iInsertEnd = rangeForm.rangeEnd();
		iInsertOptions = rangeForm.rangeOptions();
	}

	if (iInsertStart >= iInsertEnd)
		return false;

	const unsigned long iInsertLength = iInsertEnd - iInsertStart;

	int iUpdate = 0;

	qtractorClipRangeCommand *pClipRangeCommand
		= new qtractorClipRangeCommand(pTrack == NULL
			? tr("insert range")
			: tr("insert track range"));

	if (pTrack) {
		iUpdate += insertEditRangeTrack(pClipRangeCommand,
			pTrack, iInsertStart, iInsertEnd, iInsertOptions);
	} else {
		// Clips & Automation...
		pTrack = pSession->tracks().first();
		while (pTrack) {
			iUpdate += insertEditRangeTrack(pClipRangeCommand,
				pTrack, iInsertStart, iInsertEnd, iInsertOptions);
			pTrack = pTrack->next();
		}
		// Loop...
		if (iInsertOptions & qtractorEditRangeForm::Loop) {
			unsigned long iLoopStart = pSession->loopStart();
			unsigned long iLoopEnd = pSession->loopEnd();
			if (iLoopStart < iLoopEnd) {
				int iLoopUpdate = 0;
				if (iLoopStart  > iInsertStart) {
					iLoopStart += iInsertLength;
					++iLoopUpdate;
				}
				if (iLoopEnd  > iInsertStart) {
					iLoopEnd += iInsertLength;
					++iLoopUpdate;
				}
				if (iLoopUpdate > 0) {
					pClipRangeCommand->addSessionCommand(
						new qtractorSessionLoopCommand(pSession,
							iLoopStart, iLoopEnd));
					++iUpdate;
				}
			}
		}
		// Punch In/Out...
		if (iInsertOptions & qtractorEditRangeForm::Punch) {
			unsigned long iPunchIn = pSession->punchOut();
			unsigned long iPunchOut = pSession->punchIn();
			if (iPunchIn < iPunchOut) {
				int iPunchUpdate = 0;
				if (iPunchIn  > iInsertStart) {
					iPunchIn += iInsertLength;
					++iPunchUpdate;
				}
				if (iPunchOut  > iInsertStart) {
					iPunchOut += iInsertLength;
					++iPunchUpdate;
				}
				if (iPunchUpdate > 0) {
					pClipRangeCommand->addSessionCommand(
						new qtractorSessionPunchCommand(pSession,
							iPunchIn, iPunchOut));
					++iUpdate;
				}
			}
		}
		// Markers...
		if (iInsertOptions & qtractorEditRangeForm::Markers) {
			qtractorTimeScale::Marker *pMarker
				= pTimeScale->markers().last();
			while (pMarker && pMarker->frame > iInsertStart) {
				pClipRangeCommand->addTimeScaleMarkerCommand(
					new qtractorTimeScaleMoveMarkerCommand(pTimeScale,
						pMarker, pMarker->frame + iInsertLength));
				pMarker = pMarker->prev();
				++iUpdate;
			}
		}
		// Tempo-map...
		if (iInsertOptions & qtractorEditRangeForm::TempoMap) {
			qtractorTimeScale::Cursor cursor(pTimeScale);
			qtractorTimeScale::Node *pNode
				= cursor.seekFrame(pSession->sessionEnd());
			while (pNode && pNode->frame > iInsertStart) {
				pClipRangeCommand->addTimeScaleNodeCommand(
					new qtractorTimeScaleMoveNodeCommand(pTimeScale,
						pNode, pNode->frame + iInsertLength));
				pNode = pNode->prev();
				++iUpdate;
			}
		}
	}

	if (iUpdate < 1) {
		delete pClipRangeCommand;
		return false;
	}

	return pSession->execute(pClipRangeCommand);
}


// Insertion method (track).
int qtractorTracks::insertEditRangeTrack (
	qtractorClipRangeCommand *pClipRangeCommand, qtractorTrack *pTrack,
	unsigned long iInsertStart, unsigned long iInsertEnd,
	unsigned int iInsertOptions ) const
{
	const unsigned long iInsertLength = iInsertEnd - iInsertStart;

	int iUpdate = 0;

    if (iInsertOptions & qtractorEditRangeForm::Clips) {
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip) {
			const unsigned long iClipStart  = pClip->clipStart();
			const unsigned long iClipOffset = pClip->clipOffset();
			const unsigned long iClipLength = pClip->clipLength();
			const unsigned long iClipEnd    = iClipStart + iClipLength;
			if (iClipEnd > iInsertStart) {
				// Slip/move clip...
				if (iClipStart < iInsertStart) {
					// Left-clip...
					pClipRangeCommand->resizeClip(pClip,
						iClipStart,
						iClipOffset,
						iInsertStart - iClipStart);
					// Right-clip...
					qtractorClip *pClipEx = m_pTrackView->cloneClip(pClip);
					if (pClipEx) {
						const unsigned long iClipOffset2
							= iClipOffset + iInsertStart - iClipStart;
						pClipEx->setClipStart(iInsertEnd);
						pClipEx->setClipOffset(iClipOffset2);
						pClipEx->setClipLength(iClipEnd - iInsertStart);
						pClipEx->setFadeOutLength(pClip->fadeOutLength());
						pClipRangeCommand->addClip(pClipEx, pTrack);
					}
				} else {
					// Whole-clip...
					pClipRangeCommand->moveClip(pClip, pTrack,
						iClipStart + iInsertLength,
						iClipOffset,
						iClipLength);
				}
				++iUpdate;
			}
			pClip = pClip->next();
		}
	}

    if (iInsertOptions & qtractorEditRangeForm::Automation) {
		qtractorCurveList *pCurveList = pTrack->curveList();
		if (pCurveList) {
			qtractorCurve *pCurve = pCurveList->first();
			while (pCurve) {
				int iCurveEditUpdate = 0;
				qtractorCurveEditCommand *pCurveEditCommand
					= new qtractorCurveEditCommand(QString(), pCurve);
				qtractorCurve::Node *pNode = pCurve->seek(iInsertStart);
				while (pNode) {
					pCurveEditCommand->moveNode(pNode,
						pNode->frame + iInsertLength);
					++iCurveEditUpdate;
					pNode = pNode->next();
				}
				if (iCurveEditUpdate > 0) {
					pClipRangeCommand->addCurveEditCommand(pCurveEditCommand);
					iUpdate += iCurveEditUpdate;
				}
				else delete pCurveEditCommand;
				pCurve = pCurve->next();
			}
		}
	}

	return iUpdate;
}


// Removal method.
bool qtractorTracks::removeEditRange ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorTimeScale *pTimeScale = pSession->timeScale();
	if (pTimeScale == NULL)
		return false;

	unsigned long iRemoveStart = pSession->editHead();
	unsigned long iRemoveEnd   = pSession->editTail();

	if (iRemoveStart >= iRemoveEnd) {
		unsigned short iBar = pTimeScale->barFromFrame(iRemoveStart);
		iRemoveEnd = pTimeScale->frameFromBar(iBar + 1);
	}

	unsigned int iRemoveOptions = qtractorEditRangeForm::None;
	iRemoveOptions |= qtractorEditRangeForm::Clips;
	iRemoveOptions |= qtractorEditRangeForm::Automation;

	if (pTrack == NULL) {
		qtractorEditRangeForm rangeForm(this);
		rangeForm.setWindowTitle(tr("Remove Range") + " - " QTRACTOR_TITLE);
		if (isClipSelected())
			clipSelectedRange(iRemoveStart, iRemoveEnd);
		rangeForm.setSelectionRange(iRemoveStart, iRemoveEnd);
		if (!rangeForm.exec())
			return false;
		iRemoveStart = rangeForm.rangeStart();
		iRemoveEnd = rangeForm.rangeEnd();
		iRemoveOptions = rangeForm.rangeOptions();
	}

	if (iRemoveStart >= iRemoveEnd)
		return false;

	const unsigned long iRemoveLength = iRemoveEnd - iRemoveStart;

	int iUpdate = 0;

	qtractorClipRangeCommand *pClipRangeCommand
		= new qtractorClipRangeCommand(pTrack == NULL
			? tr("remove range")
			: tr("remove track range"));

	if (pTrack) {
		iUpdate += removeEditRangeTrack(pClipRangeCommand,
			pTrack, iRemoveStart, iRemoveEnd, iRemoveOptions);
	} else {
		// Clips & Automation...
		pTrack = pSession->tracks().first();
		while (pTrack) {
			iUpdate += removeEditRangeTrack(pClipRangeCommand,
				pTrack, iRemoveStart, iRemoveEnd, iRemoveOptions);
			pTrack = pTrack->next();
		}
		// Loop...
		if (iRemoveOptions & qtractorEditRangeForm::Loop) {
			unsigned long iLoopStart = pSession->loopStart();
			unsigned long iLoopEnd = pSession->loopEnd();
			if (iLoopStart < iLoopEnd) {
				int iLoopUpdate = 0;
				if (iLoopStart > iRemoveStart) {
					if (iLoopStart  > iRemoveEnd)
						iLoopStart  = iRemoveStart;
					else
						iLoopStart -= iRemoveLength;
					++iLoopUpdate;
				}
				if (iLoopEnd > iRemoveStart) {
					if (iLoopEnd  > iRemoveEnd)
						iLoopEnd -= iRemoveLength;
					else
						iLoopEnd  = iRemoveEnd;
					++iLoopUpdate;
				}
				if (iLoopUpdate > 0) {
					pClipRangeCommand->addSessionCommand(
						new qtractorSessionLoopCommand(pSession,
							iLoopStart, iLoopEnd));
					++iUpdate;
				}
			}
		}
		// Punch In/Out...
		if (iRemoveOptions & qtractorEditRangeForm::Punch) {
			unsigned long iPunchIn = pSession->punchOut();
			unsigned long iPunchOut = pSession->punchIn();
			if (iPunchIn < iPunchOut) {
				int iPunchUpdate = 0;
				if (iPunchIn > iRemoveStart) {
					if (iPunchIn  > iRemoveEnd)
						iPunchIn  = iRemoveStart;
					else
						iPunchIn -= iRemoveLength;
					++iPunchUpdate;
				}
				if (iPunchOut > iRemoveStart) {
					if (iPunchOut  > iRemoveEnd)
						iPunchOut -= iRemoveLength;
					else
						iPunchOut  = iRemoveEnd;
					++iPunchUpdate;
				}
				if (iPunchUpdate > 0) {
					pClipRangeCommand->addSessionCommand(
						new qtractorSessionPunchCommand(pSession,
							iPunchIn, iPunchOut));
					++iUpdate;
				}
			}
		}
		// Markers...
		if (iRemoveOptions & qtractorEditRangeForm::Markers) {
			qtractorTimeScale::Marker *pMarker
				= pTimeScale->markers().seekFrame(iRemoveStart);
			while (pMarker) {
				if (pMarker->frame > iRemoveStart) {
					if (pMarker->frame < iRemoveEnd) {
						pClipRangeCommand->addTimeScaleMarkerCommand(
							new qtractorTimeScaleRemoveMarkerCommand(
								pTimeScale, pMarker));
						++iUpdate;
					}
					else
					if (pMarker->frame > iRemoveEnd) {
						pClipRangeCommand->addTimeScaleMarkerCommand(
							new qtractorTimeScaleMoveMarkerCommand(pTimeScale,
								pMarker, pMarker->frame - iRemoveLength));
						++iUpdate;
					}
				}
				pMarker = pMarker->next();
			}
		}
		// Tempo-map...
		if (iRemoveOptions & qtractorEditRangeForm::TempoMap) {
			qtractorTimeScale::Cursor cursor(pTimeScale);
			qtractorTimeScale::Node *pNode
				= cursor.seekFrame(iRemoveStart);
			while (pNode) {
				if (pNode->frame > iRemoveStart) {
					if (pNode->frame < iRemoveEnd) {
						pClipRangeCommand->addTimeScaleNodeCommand(
							new qtractorTimeScaleRemoveNodeCommand(
								pTimeScale, pNode));
						++iUpdate;
					}
					else
					if (pNode->frame > iRemoveEnd) {
						pClipRangeCommand->addTimeScaleNodeCommand(
							new qtractorTimeScaleMoveNodeCommand(pTimeScale,
								pNode, pNode->frame - iRemoveLength));
						++iUpdate;
					}
				}
				pNode = pNode->next();
			}
		}
	}

	if (iUpdate < 1) {
		delete pClipRangeCommand;
		return false;
	}

	clearSelect();

	return pSession->execute(pClipRangeCommand);
}


// Removal method (track).
int qtractorTracks::removeEditRangeTrack (
	qtractorClipRangeCommand *pClipRangeCommand, qtractorTrack *pTrack,
	unsigned long iRemoveStart, unsigned long iRemoveEnd,
	unsigned int iRemoveOptions ) const
{
	const unsigned long iRemoveLength = iRemoveEnd - iRemoveStart;

	int iUpdate = 0;

	if (iRemoveOptions & qtractorEditRangeForm::Clips) {
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip) {
			const unsigned long iClipStart  = pClip->clipStart();
			const unsigned long iClipOffset = pClip->clipOffset();
			const unsigned long iClipLength = pClip->clipLength();
			const unsigned long iClipEnd    = iClipStart + iClipLength;
			if (iClipEnd > iRemoveStart) {
				// Slip/move clip...
				if (iClipStart < iRemoveEnd) {
					// Left-clip...
					if (iClipStart < iRemoveStart) {
						pClipRangeCommand->resizeClip(pClip,
								iClipStart,
								iClipOffset,
								iRemoveStart - iClipStart);
						// Right-clip...
						if (iClipEnd > iRemoveEnd) {
							qtractorClip *pClipEx = m_pTrackView->cloneClip(pClip);
							if (pClipEx) {
								const unsigned long iClipOffset2
									= iClipOffset + iRemoveEnd - iClipStart;
								pClipEx->setClipStart(iRemoveStart);
								pClipEx->setClipOffset(iClipOffset2);
								pClipEx->setClipLength(iClipEnd - iRemoveEnd);
								pClipEx->setFadeOutLength(pClip->fadeOutLength());
								pClipRangeCommand->addClip(pClipEx, pTrack);
							}
						}
					}
					else
					if (iClipEnd > iRemoveEnd) {
						pClipRangeCommand->resizeClip(pClip,
							iRemoveStart,
							iClipOffset + iRemoveEnd - iClipStart,
							iClipEnd - iRemoveEnd);
					} else {
						pClipRangeCommand->removeClip(pClip);
					}
				} else {
					// Whole-clip...
					pClipRangeCommand->moveClip(pClip, pTrack,
						iClipStart - iRemoveLength,
						iClipOffset,
						iClipLength);
				}
				++iUpdate;
			}
			pClip = pClip->next();
		}
	}

	if (iRemoveOptions & qtractorEditRangeForm::Automation) {
		qtractorCurveList *pCurveList = pTrack->curveList();
		if (pCurveList) {
			qtractorCurve *pCurve = pCurveList->first();
			while (pCurve) {
				int iCurveEditUpdate = 0;
				qtractorCurveEditCommand *pCurveEditCommand
					= new qtractorCurveEditCommand(QString(), pCurve);
				qtractorCurve::Node *pNode = pCurve->seek(iRemoveStart);
				while (pNode) {
					if (pNode->frame < iRemoveEnd)
						pCurveEditCommand->removeNode(pNode);
					else
						pCurveEditCommand->moveNode(pNode,
							pNode->frame - iRemoveLength);
					++iCurveEditUpdate;
					pNode = pNode->next();
				}
				if (iCurveEditUpdate > 0) {
					pClipRangeCommand->addCurveEditCommand(pCurveEditCommand);
					iUpdate += iCurveEditUpdate;
				}
				else delete pCurveEditCommand;
				pCurve = pCurve->next();
			}
		}
	}

	return iUpdate;
}


// Adds a new track into session.
bool qtractorTracks::addTrack (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Create a new track right away...
	const int iTrack = pSession->tracks().count() + 1;
	const QColor& color = qtractorTrack::trackColor(iTrack);
	qtractorTrack *pTrack = new qtractorTrack(pSession);
	pTrack->setTrackName(tr("Track %1").arg(iTrack));
	pTrack->setMidiChannel(pSession->midiTag() % 16);
	pTrack->setBackground(color);
	pTrack->setForeground(color.darker());

	// Open dialog for settings...
	qtractorTrackForm trackForm(pMainForm);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec()) {
		delete pTrack;
		return false;
	}

	// Take care of user supplied properties...
	pTrack->properties() = trackForm.properties();

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorAddTrackCommand(pTrack, currentTrack()));
}


// Remove given(current) track from session.
bool qtractorTracks::removeTrack ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Get the list view item reference of the intended track...
	if (pTrack == NULL)
		pTrack = currentTrack();
	if (pTrack == NULL)
		return false;

	// Don't remove tracks engaged in recording...
	if (pTrack->isRecord() && pSession->isRecording() && pSession->isPlaying())
		return false;

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove track:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(pTrack->trackName()),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel)
			return false;
	}

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorRemoveTrackCommand(pTrack));
}


// Edit given(current) track properties.
bool qtractorTracks::editTrack ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	// Get the list view item reference of the intended track...
	if (pTrack == NULL)
		pTrack = currentTrack();
	if (pTrack == NULL)
		return false;

	// Don't edit tracks engaged in recording...
	if (pTrack->isRecord() && pSession->isRecording() && pSession->isPlaying())
		return false;

	// Open dialog for settings...
	qtractorTrackForm trackForm(pMainForm);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec())
		return false;

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorEditTrackCommand(pTrack, trackForm.properties()));
}


// Import Audio files into new tracks...
bool qtractorTracks::addAudioTracks (
	QStringList files, unsigned long iClipStart )
{
	// Have we some?
	if (files.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

//	pSession->lock();

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand(currentTrack());

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// Needed whether we'll span to one single track
	// or will have each clip intto several tracks...
	const bool bDropSpan = m_pTrackView->isDropSpan();
	qtractorTrack *pTrack = NULL;
	int iTrackClip = 0;

	// For each one of those files...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		// Create a new track right away...
		if (pTrack == NULL || !bDropSpan) {
			const QColor& color = qtractorTrack::trackColor(++iTrack);
			pTrack = new qtractorTrack(pSession, qtractorTrack::Audio);
			pTrack->setBackground(color);
			pTrack->setForeground(color.darker());
			pImportTrackCommand->addTrack(pTrack);
			iTrackClip = 0;
		}
		// Add the clip at once...
		qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
		pAudioClip->setFilename(sPath);
		pAudioClip->setClipStart(iClipStart);
		// Time to add the new track/clip into session;
		// actuallly, this is when the given audio file gets open...
		pTrack->addClip(pAudioClip);
		if (iTrackClip == 0)
			pTrack->setTrackName(pAudioClip->clipName());
		++iTrackClip;
		// Add the new track to composite command...
		if (bDropSpan)
			iClipStart += pAudioClip->clipLength();
		++iUpdate;
		// Don't forget to add this one to local repository.
		if (pMainForm) {
			pMainForm->addAudioFile(sPath);
			// Log this successful import operation...
			sDescription += tr("Audio file import \"%1\" on %2 %3.\n")
				.arg(QFileInfo(sPath).fileName())
				.arg(QDate::currentDate().toString("MMM dd yyyy"))
				.arg(QTime::currentTime().toString("hh:mm:ss"));
			pMainForm->appendMessages(
				tr("Audio file import: \"%1\".").arg(sPath));
		}
	}

	// Have we changed anything?
	bool bResult = false;
	if (iUpdate > 0) {
		// Log to session (undoable by import-track command)...
		pSession->setDescription(sDescription);
		// Put it in the form of an undoable command...
		bResult = pSession->execute(pImportTrackCommand);
	} else {
		delete pImportTrackCommand;
	}

//	pSession->unlock();

	return bResult;
}


// Import MIDI files into new tracks...
bool qtractorTracks::addMidiTracks (
	QStringList files, unsigned long iClipStart )
{
	// Have we some?
	if (files.isEmpty())
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

//	pSession->lock();

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand(currentTrack());

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// Needed to help on setting whole session properties
	// from the first imported MIDI file...
	int iImport = iTrack;

	const unsigned long iTimeStart = pSession->tickFromFrame(iClipStart);

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// For each one of those files...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		// We'll be careful and pre-open the SMF header here...
		qtractorMidiFile file;
		if (!file.open(sPath))
			continue;
		// It all depends on the format...
		int iTracks = (file.format() == 1 ? file.tracks() : 16);
		for (int iTrackChannel = 0; iTrackChannel < iTracks; ++iTrackChannel) {
			// Create a new track right away...
			const QColor& color = qtractorTrack::trackColor(++iTrack);
			qtractorTrack *pTrack
				= new qtractorTrack(pSession, qtractorTrack::Midi);
		//	pTrack->setTrackName(QFileInfo(sPath).baseName());
			pTrack->setBackground(color);
			pTrack->setForeground(color.darker());
			// Add the clip at once...
			qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
			pMidiClip->setFilename(sPath);
			pMidiClip->setTrackChannel(iTrackChannel);
			pMidiClip->setClipStart(iClipStart);
			if (iTrackChannel == 0 && iImport == 0)
				pMidiClip->setSessionFlag(true);
			// Time to add the new track/clip into session;
			// actuallly, this is when the given MIDI file and
			// track-channel gets open and read into the clip!
			pTrack->addClip(pMidiClip);
			// As far the standards goes,from which we'll strictly follow,
			// only the first track/channel has some tempo/time signature...
			if (iTrackChannel == 0) {
				// Some adjustment required...
				++iImport;
				iClipStart = pSession->frameFromTick(iTimeStart);
				pMidiClip->setClipStart(iClipStart);
			}
			// Time to check whether there is actual data on track...
			if (pMidiClip->clipLength() > 0) {
				// Add the new track to composite command...
				pTrack->setTrackName(pMidiClip->clipName());
				pTrack->setMidiChannel(pMidiClip->channel());
				pImportTrackCommand->addTrack(pTrack);
				++iUpdate;
				// Don't forget to add this one to local repository.
				if (pMainForm)
					pMainForm->addMidiFile(sPath);
			} else {
				// Get rid of these, now...
				pTrack->unlinkClip(pMidiClip);
				delete pMidiClip;
				delete pTrack;
			}
		}
		// Log this successful import operation...
		if (iUpdate > 0 && pMainForm) {
			sDescription += tr("MIDI file import \"%1\" on %2 %3.\n")
				.arg(QFileInfo(sPath).fileName())
				.arg(QDate::currentDate().toString("MMM dd yyyy"))
				.arg(QTime::currentTime().toString("hh:mm:ss"));
			pMainForm->appendMessages(
				tr("MIDI file import: \"%1\".").arg(sPath));
		}
	}

	// Have we changed anything?
	bool bResult = false;
	if (iUpdate > 0) {
		// Log to session (undoable by import-track command)...
		pSession->setDescription(sDescription);	
		// Put it in the form of an undoable command...
		bResult = pSession->execute(pImportTrackCommand);
	} else {
		delete pImportTrackCommand;
	}

//	pSession->unlock();

	return bResult;
}


// Import MIDI file track-channel into new track...
bool qtractorTracks::addMidiTrackChannel ( const QString& sPath,
	int iTrackChannel, unsigned long iClipStart )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand(currentTrack());

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// Create a new track right away...
	const QColor& color = qtractorTrack::trackColor(++iTrack);
	qtractorTrack *pTrack
		= new qtractorTrack(pSession, qtractorTrack::Midi);
//	pTrack->setTrackName(QFileInfo(sPath).baseName());
	pTrack->setBackground(color);
	pTrack->setForeground(color.darker());
	// Add the clip at once...
	qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
	pMidiClip->setFilename(sPath);
	pMidiClip->setTrackChannel(iTrackChannel);
	pMidiClip->setClipStart(iClipStart);
	// Time to add the new track/clip into session...
	pTrack->addClip(pMidiClip);
	pTrack->setTrackName(pMidiClip->clipName());
	pTrack->setMidiChannel(pMidiClip->channel());
	// Add the new track to composite command...
	pImportTrackCommand->addTrack(pTrack);

	// Don't forget to add this one to local repository.
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->addMidiFile(sPath);
		// Log this successful import operation...
		sDescription += tr("MIDI file import \"%1\""
			" track-channel %2 on %3 %4.\n")
			.arg(QFileInfo(sPath).fileName()).arg(iTrackChannel)
			.arg(QDate::currentDate().toString("MMM dd yyyy"))
			.arg(QTime::currentTime().toString("hh:mm:ss"));
		pMainForm->appendMessages(
			tr("MIDI file import: \"%1\", track-channel: %2.")
			.arg(sPath).arg(iTrackChannel));
	}

	// Log to session (undoable by import-track command)...
	pSession->setDescription(sDescription);

	// Put it in the form of an undoable command...
	return pSession->execute(pImportTrackCommand);
}


// Track-list active maintenance update.
void qtractorTracks::updateTrack ( qtractorTrack *pTrack )
{
	m_pTrackList->updateTrack(pTrack);

	if (pTrack->trackType() == qtractorTrack::Midi)
		updateMidiTrack(pTrack);
}


// MIDI track/bus/channel alias active maintenance method.
void qtractorTracks::updateMidiTrack ( qtractorTrack *pMidiTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QString& sBusName = pMidiTrack->outputBusName();
	const unsigned short iChannel = pMidiTrack->midiChannel();

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		// If same channel, force same bank/program stuff...
		if (pTrack != pMidiTrack
			&& pTrack->trackType() == qtractorTrack::Midi
			&& pTrack->outputBusName() == sBusName
			&& pTrack->midiChannel() == iChannel) {
			// Make else tracks MIDI attributes the same....
			pTrack->setMidiBankSelMethod(pMidiTrack->midiBankSelMethod());
			pTrack->setMidiBank(pMidiTrack->midiBank());
			pTrack->setMidiProg(pMidiTrack->midiProg());
			// Update the track list view, immediately...
			m_pTrackList->updateTrack(pTrack);
		}
	}

#if 0
	// Re-open all MIDI clips (channel might have changed?)...
	qtractorClip *pClip = pMidiTrack->clips().first();
	for ( ; pClip; pClip = pClip->next())
		pClip->open();
#endif

	// Update MIDI bus patch...
	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pMidiTrack->outputBus());
	if (pMidiBus == NULL)
		return;

	const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
	pMidiBus->setPatch(iChannel, patch.instrumentName,
		pMidiTrack->midiBankSelMethod(),
		pMidiTrack->midiBank(),
		pMidiTrack->midiProg(),
		pMidiTrack);
}


// Simple main-form stabilizer redirector.
void qtractorTracks::selectionChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->selectionNotifySlot(NULL);
}


// Simple main-form dirty-flag redirectors.
void qtractorTracks::contentsChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->changeNotifySlot(NULL);
}

void qtractorTracks::dirtyChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->dirtyNotifySlot();
}


// Track-list update (current track only).
void qtractorTracks::updateTrackList (void)
{
	m_pTrackList->updateTrack(currentTrack());
}


// Track-view update (obviously a slot).
void qtractorTracks::updateTrackView (void)
{
	m_pTrackView->update();
}


// Overall selection reset.
void qtractorTracks::clearSelect (void)
{
	m_pTrackView->clearSelect();
}


// Overall contents reset.
void qtractorTracks::clear (void)
{
	m_pTrackList->clear();
	m_pTrackView->clear();

	updateContents(true);
}


// end of qtractorTracks.cpp
