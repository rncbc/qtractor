// qtractorTracks.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"

#include "qtractorAudioEngine.h"
#include "qtractorAudioClip.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"

#include "qtractorOptions.h"

#include "qtractorMainForm.h"
#include "qtractorTrackForm.h"
#include "qtractorClipForm.h"

#include "qtractorPasteRepeatForm.h"

#include "qtractorMidiEditorForm.h"

#include <QVBoxLayout>
#include <QToolButton>
#include <QProgressBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDate>
#include <QUrl>

#include <QHeaderView>

#if QT_VERSION < 0x040300
#define lighter(x)	light(x)
#define darker(x)	dark(x)
#endif


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
	QSplitter::setWindowIcon(QIcon(":/icons/qtractorTracks.png"));

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
	pSession->updateSessionLength();
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


// Zoom view slots.
void qtractorTracks::zoomIn (void)
{
	horizontalZoomStep(+ ZoomStep);
	verticalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorTracks::zoomOut (void)
{
	horizontalZoomStep(- ZoomStep);
	verticalZoomStep(- ZoomStep);
	centerContents();
}


void qtractorTracks::zoomReset (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
	verticalZoomStep(ZoomBase - pSession->verticalZoom());
	centerContents();
}


void qtractorTracks::horizontalZoomInSlot (void)
{
	horizontalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorTracks::horizontalZoomOutSlot (void)
{
	horizontalZoomStep(- ZoomStep);
	centerContents();
}

void qtractorTracks::verticalZoomInSlot (void)
{
	verticalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorTracks::verticalZoomOutSlot (void)
{
	verticalZoomStep(- ZoomStep);
	centerContents();
}

void qtractorTracks::viewZoomResetSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// All zoom base are belong to us :)
	verticalZoomStep(ZoomBase - pSession->verticalZoom());
	horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
	centerContents();
}


// Try to center horizontally/vertically
// (usually after zoom change)
void qtractorTracks::centerContents (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
		
	// Get current session frame location...
	unsigned long iFrame = m_pTrackView->sessionCursor()->frame();

	// Update the dependant views...
	m_pTrackList->updateContentsHeight();
	m_pTrackView->updateContentsWidth();
	m_pTrackView->setContentsPos(
		pSession->pixelFromFrame(iFrame), m_pTrackView->contentsY());
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
		iRefresh++;
	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		// Check if item is already on list
		if (m_pTrackList->trackRow(pTrack) < 0) {
			m_pTrackList->insertTrack(iTrack, pTrack);
			iRefresh++;
		}
		pTrack = pTrack->next();
		iTrack++;
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
		if (pClip) {
			pTrack = pClip->track();
		} else {
			pTrack = pSession->tracks().first();
		}
	}

	return pTrack;
}


// Retrieves current selected track widget reference.
qtractorTrackItemWidget *qtractorTracks::currentTrackWidget (void) const
{
	return m_pTrackList->currentTrackWidget();
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
	unsigned long iClipStart = pSession->editHead();
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
		// Create a clip filename from scratch...
		const QString& sFilename = pSession->createFilePath(
			pTrack->trackName(), pTrack->clips().count(), "mid");
		pClip->setFilename(sFilename);
		pClip->setClipName(QFileInfo(sFilename).baseName());
		// Proceed to setup the MDII clip properly...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip) {
			// Initialize MIDI event container...
			qtractorMidiSequence *pSeq = pMidiClip->sequence();
			pSeq->setName(pMidiClip->clipName());
			pSeq->setChannel(pTrack->midiChannel());
			pSeq->setTicksPerBeat(pSession->ticksPerBeat());
			// Which SMF format?
			if (pMidiClip->format() == 0) {
				// SMF format 0 (1 track, 1 channel)
				pMidiClip->setTrackChannel(pTrack->midiChannel());
			} else {
				// SMF format 1 (2 tracks, 1 channel)
				pMidiClip->setTrackChannel(1);
			}
			// Make it a brand new revision...
			pMidiClip->setRevision(1);
			// Insert the clip right away...
			qtractorClipCommand *pClipCommand
				= new qtractorClipCommand(tr("new clip"));
			pClipCommand->addClip(pClip, pTrack);
			pSession->execute(pClipCommand);
			// Just start the MIDI editor on it...
			return pClip->startEditor(this);
		}
	}

	// Then ask user to refine clip properties...
	qtractorClipForm clipForm(this);
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

	// All else hasn't fail.
	return pClip->startEditor(this);
}


// Split given(current) clip.
bool qtractorTracks::splitClip ( qtractorClip *pClip )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	if (pClip == NULL)
		pClip = m_pTrackView->currentClip();
	if (pClip == NULL)
		return false;

	unsigned long iPlayHead  = pSession->playHead();
	unsigned long iClipStart = pClip->clipStart();
	unsigned long iClipEnd   = iClipStart + pClip->clipLength();
	if (iClipStart >= iPlayHead || iPlayHead >= iClipEnd)
		return false;

	m_pTrackView->ensureVisibleFrame(iPlayHead);

	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("split clip"));

	// Shorten old right...
	unsigned long iClipOffset = pClip->clipOffset();
	pClipCommand->moveClip(pClip, pClip->track(),
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
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
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
		if (data.max > 0.1f && data.max < 1.1f)
			fGain /= data.max;
		if (pProgressBar)
			pProgressBar->hide();
		QApplication::restoreOverrideCursor();
	}
	else
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Normalize MIDI clip...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip == NULL)
			return false;
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		unsigned char max = 0;
		pMidiClip->clipExport(midiClipNormalize, &max, iOffset, iLength);
		if (max > 0x0c && max < 0x7f)
			fGain *= (127.0f / float(max));
		QApplication::restoreOverrideCursor();
	}

	// Make it as an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip normalize"));
	pClipCommand->gainClip(pClip, fGain);

	// That's it...
	return pSession->execute(pClipCommand);
}


// Audio clip export calback.
struct audioClipExportData
{	// Ctor.
	audioClipExportData(qtractorAudioFile *pFile)
		: count(0), file(pFile) {};
	// Members.
	unsigned int count;
	qtractorAudioFile *file;
};

static void audioClipExport (
	float **ppFrames, unsigned int iFrames, void *pvArg )
{
	audioClipExportData *pData
		= static_cast<audioClipExportData *> (pvArg);

	(pData->file)->write(ppFrames, iFrames);

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


// MIDI clip export calback.
static void midiClipExport (
	qtractorMidiSequence *pSeq, void *pvArg )
{
	qtractorMidiFile *pMidiFile
		= static_cast<qtractorMidiFile *> (pvArg);
	pMidiFile->writeTrack(pSeq);
}


// Export given(current) clip.
bool qtractorTracks::exportClip ( qtractorClip *pClip )
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

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();

	unsigned long iOffset = 0;
	unsigned long iLength = pClip->clipLength();

	if (pClip->isClipSelected()) {
		iOffset = pClip->clipSelectStart() - pClip->clipStart();
		iLength = pClip->clipSelectEnd() - pClip->clipSelectStart();
	}

	if (pTrack->trackType() == qtractorTrack::Audio) {
		// Export audio clip...
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (pClip);
		if (pAudioClip == NULL)
			return false;
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pTrack->outputBus());
		if (pAudioBus == NULL)
			return false;
		const QString& sExt = qtractorAudioFileFactory::defaultExt();
		const QString& sTitle  = tr("Export Audio Clip") + " - " QTRACTOR_TITLE;
		const QString& sFilter = tr("Audio files (*.%1)").arg(sExt); 
	#if QT_VERSION < 0x040400
		// Ask for the filename to save...
		QString sFilename = QFileDialog::getSaveFileName(this, sTitle,
			pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
	#else
		// Construct save-file dialog...
		QFileDialog fileDialog(this, sTitle,
			pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
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
		}
		// Show dialog...
		if (!fileDialog.exec())
			return false;
		QString sFilename = fileDialog.selectedFiles().first();
	#endif
		if (sFilename.isEmpty())
			return false;
		if (QFileInfo(sFilename).suffix() != sExt)
			sFilename += '.' + sExt;
		qtractorAudioFile *pAudioFile
			= qtractorAudioFileFactory::createAudioFile(sFilename,
				pAudioBus->channels(), pSession->sampleRate());
		if (pAudioFile == NULL)
			return false;
		if (pMainForm) {
			pMainForm->appendMessages(
				tr("Audio clip export: \"%1\" started...")
				.arg(sFilename));
		}
		bool bResult = false;
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		QProgressBar *pProgressBar = NULL;
		if (pMainForm)
			pProgressBar = pMainForm->progressBar();
		if (pProgressBar) {
			pProgressBar->setRange(0, iLength / 100);
			pProgressBar->reset();
			pProgressBar->show();
		}
		if (pAudioFile->open(sFilename, qtractorAudioFile::Write)) {
			audioClipExportData data(pAudioFile);
			pAudioClip->clipExport(
				audioClipExport, &data, iOffset, iLength);
			pAudioFile->close();
			bResult = true;
		}
		delete pAudioFile;
		if (pProgressBar)
			pProgressBar->hide();
		QApplication::restoreOverrideCursor();
		if (pMainForm) {
			if (bResult) {
				pMainForm->addAudioFile(sFilename);
				pMainForm->appendMessages(
					tr("Audio clip export: \"%1\" complete.")
					.arg(sFilename));
			} else {
				pMainForm->appendMessagesError(
					tr("Audio clip export:\n\n\"%1\"\n\nfailed.")
					.arg(sFilename));
			}
		}
	}
	else
	if (pTrack->trackType() == qtractorTrack::Midi) {
		// Export MIDI clip...
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (pClip);
		if (pMidiClip == NULL)
			return false;
		const QString  sExt("mid");
		const QString& sTitle  = tr("Export MIDI Clip") + " - " QTRACTOR_TITLE;
		const QString& sFilter = tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt); 
	#if QT_VERSION < 0x040400
		// Ask for the filename to save...
		QString sFilename = QFileDialog::getSaveFileName(this, sTitle,
			pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
	#else
		// Construct save-file dialog...
		QFileDialog fileDialog(this, sTitle,
			pSession->createFilePath(pTrack->trackName(), 0, sExt), sFilter);
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
		}
		// Show dialog...
		if (!fileDialog.exec())
			return false;
		QString sFilename = fileDialog.selectedFiles().first();
	#endif
		if (sFilename.isEmpty())
			return false;
		if (QFileInfo(sFilename).suffix() != sExt)
			sFilename += '.' + sExt;
		if (pMainForm) {
			pMainForm->appendMessages(
				tr("MIDI clip export: \"%1\" started...")
				.arg(sFilename));
		}
		bool bResult = true;
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		qtractorMidiFile *pMidiFile = new qtractorMidiFile();
		if (pMidiFile->open(sFilename, qtractorMidiFile::Write)) {
			pMidiFile->setTempo(pSession->tempo());
			pMidiFile->setBeatsPerBar(pSession->beatsPerBar());
			unsigned short iFormat = qtractorMidiClip::defaultFormat();
			unsigned short iTracks = (iFormat == 0 ? 1 : 2);
			pMidiFile->writeHeader(iFormat, iTracks, pSession->ticksPerBeat());
			if (iFormat == 1)
				pMidiFile->writeTrack(NULL);  // Setup track (SMF format 1).
			pMidiClip->clipExport(
				midiClipExport, pMidiFile, iOffset, iLength);
			pMidiFile->close();
			bResult = true;
		}
		delete pMidiFile;
		QApplication::restoreOverrideCursor();
		if (pMainForm) {
			if (bResult) {
				pMainForm->addMidiFile(sFilename);
				pMainForm->appendMessages(
					tr("MIDI clip export: \"%1\" complete.")
					.arg(sFilename));
			} else {
				pMainForm->appendMessagesError(
					tr("MIDI clip export:\n\n\"%1\"\n\nfailed.")
					.arg(sFilename));
			}
		}
	}
	else return false; // WTF?

	// Done.
	return true;
}


// Whether there's any clip currently selected.
bool qtractorTracks::isClipSelected (void) const
{
	return m_pTrackView->isClipSelected();
}


// Clipboard methods.
void qtractorTracks::cutClipboard (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Cut);
}

void qtractorTracks::copyClipboard (void)
{
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
void qtractorTracks::deleteClipSelect (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Delete);
}


// Select range interval between edit head and tail.
void qtractorTracks::selectEditRange (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Get and select the whole rectangular area
	// between the edit head and tail points...
	QRect rect(0, 0, 0, m_pTrackView->contentsHeight());
	rect.setLeft(pSession->pixelFromFrame(pSession->editHead()));
	rect.setRight(pSession->pixelFromFrame(pSession->editTail()));

	// HACK: Make sure the snap goes straight...
	unsigned short iSnapPerBeat4 = (pSession->snapPerBeat() << 2);
	if (iSnapPerBeat4 > 0)
		rect.translate(pSession->pixelsPerBeat() / iSnapPerBeat4, 0);

	// Make the selection, but don't change edit head nor tail...
	m_pTrackView->selectRect(rect,
		qtractorTrackView::SelectRange,
		qtractorTrackView::EditNone);

	// Make its due...
	selectionChangeNotify();
}



// Select all clips on current track.
void qtractorTracks::selectCurrentTrack ( bool bReset )
{
	qtractorTrack *pTrack = currentTrack();
	if (pTrack)
		m_pTrackView->selectTrack(pTrack, bReset);
}


// Select all clips on all tracks.
void qtractorTracks::selectAll ( bool bSelect )
{
	m_pTrackView->selectAll(bSelect);
}


// Adds a new track into session.
bool qtractorTracks::addTrack (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Create a new track right away...
	const int iTrack = pSession->tracks().count() + 1;
	const QColor color = qtractorTrack::trackColor(iTrack);
	qtractorTrack *pTrack = new qtractorTrack(pSession);
	pTrack->setTrackName(QString("Track %1").arg(iTrack));
	pTrack->setMidiChannel(pSession->midiTag() % 16);
	pTrack->setBackground(color);
	pTrack->setForeground(color.darker());

	// Open dialog for settings...
	qtractorTrackForm trackForm(this);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec()) {
		delete pTrack;
		return false;
	}

	// Take care of user supplied properties...
	pTrack->properties() = trackForm.properties();

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorAddTrackCommand(pTrack));
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
			tr("OK"), tr("Cancel")) > 0)
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

	// Get the list view item reference of the intended track...
	if (pTrack == NULL)
		pTrack = currentTrack();
	if (pTrack == NULL)
		return false;

	// Don't edit tracks engaged in recording...
	if (pTrack->isRecord() && pSession->isRecording() && pSession->isPlaying())
		return false;

	// Open dialog for settings...
	qtractorTrackForm trackForm(this);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec())
		return false;

	// Put it in the form of an undoable command...
	return pSession->execute(
		new qtractorEditTrackCommand(pTrack, trackForm.properties()));
}


// Import Audio files into new tracks...
bool qtractorTracks::addAudioTracks ( QStringList files,
	unsigned long iClipStart )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand();

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// Needed whether we'll span to one single track
	// or will have each clip intto several tracks...
	bool bDropSpan = m_pTrackView->isDropSpan();
	qtractorTrack *pTrack = NULL;
	int iTrackClip = 0;

	// For each one of those files, if any...
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
		// actuallly, this is wheen the given audio file gets open...
		pTrack->addClip(pAudioClip);
		if (iTrackClip == 0)
			pTrack->setTrackName(pAudioClip->clipName());
		iTrackClip++;
		// Add the new track to composite command...
		if (bDropSpan)
			iClipStart += pAudioClip->clipLength();
		iUpdate++;
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
		// Make things temporarily stable...
		qtractorSession::stabilize();
	}

	// Have we changed anything?
	if (iUpdate < 1) {
		delete pImportTrackCommand;
		return false;
	}
	
	// Log to session (undoable by import-track command)...
	pSession->setDescription(sDescription);

	// Put it in the form of an undoable command...
	return pSession->execute(pImportTrackCommand);
}


// Import MIDI files into new tracks...
bool qtractorTracks::addMidiTracks ( QStringList files,
	unsigned long iClipStart )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand();

	// Increment this for suggestive track coloring...
	int iTrack = pSession->tracks().count();

	// Needed to help on setting whole session properties
	// from the first imported MIDI file...
	int iImport = iTrack;
	unsigned long iTimeStart = pSession->tickFromFrame(iClipStart);

	// To log this import into session description.
	QString sDescription = pSession->description().trimmed();
	if (!sDescription.isEmpty())
		sDescription += '\n';

	// For each one of those files, if any...
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
		for (int iTrackChannel = 0; iTrackChannel < iTracks; iTrackChannel++) {
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
			// actuallly, this is wheen the given MIDI file and
			// track-channel gets open and read into the clip!
			pTrack->addClip(pMidiClip);
			// As far the standards goes,from which we'll strictly follow,
			// only the first track/channel has some tempo/time signature...
			if (iTrackChannel == 0) {
				// Some adjustment required...
				iImport++;
				iClipStart = pSession->frameFromTick(iTimeStart);
				pMidiClip->setClipStart(iClipStart);
			}
			// Time to check whether there is actual data on track...
			if (pMidiClip->clipLength() > 0) {
				// Add the new track to composite command...
				pTrack->setTrackName(pMidiClip->clipName());
				pTrack->setMidiChannel(pMidiClip->channel());
				pImportTrackCommand->addTrack(pTrack);
				iUpdate++;
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
		// Make things temporarily stable...
		qtractorSession::stabilize();
	}

	// Have we changed anything?
	if (iUpdate < 1) {
		delete pImportTrackCommand;
		return false;
	}

	// Log to session (undoable by import-track command)...
	pSession->setDescription(sDescription);

	// Put it in the form of an undoable command...
	return pSession->execute(pImportTrackCommand);
}


// Import MIDI file track-channel into new track...
bool qtractorTracks::addMidiTrackChannel ( const QString& sPath,
	int iTrackChannel, unsigned long iClipStart )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand();

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
		// Make things temporarily stable...
		pMainForm->appendMessages(
			tr("MIDI file import: \"%1\""
				", track-channel: %2.").arg(sPath).arg(iTrackChannel));
	}

	qtractorSession::stabilize();

	// Put it in the form of an undoable command...
	return pSession->execute(pImportTrackCommand);
}


// MIDI track/bus/channel alias active maintenance method.
void qtractorTracks::updateMidiTrack ( qtractorTrack *pMidiTrack )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QString& sBusName = pMidiTrack->outputBusName();
	unsigned short iChannel = pMidiTrack->midiChannel();

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
			pTrack->setMidiProgram(pMidiTrack->midiProgram());
			// Update the track list view, immediately...
			m_pTrackList->updateTrack(pTrack);
		}
	}

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
		pMidiTrack->midiProgram(),
		pMidiTrack);
}


// Simple main-form stabilizer redirector.
void qtractorTracks::selectionChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->selectionNotifySlot(NULL);
}


// Simple main-form dirty-flag redirector.
void qtractorTracks::contentsChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->changeNotifySlot(NULL);
}


// Overall contents reset.
void qtractorTracks::clear(void)
{
	m_pTrackList->clear();
	m_pTrackView->clear();

	updateContents(true);
}


// Track button notification.
void qtractorTracks::trackButtonToggledSlot (
	qtractorTrackButton *pTrackButton, bool bOn )
{
	// Put it in the form of an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorTrackButtonCommand(pTrackButton, bOn));
}


// end of qtractorTracks.cpp
