// qtractorTracks.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTracks.h"
#include "qtractorTrackView.h"
#include "qtractorTrackList.h"
#include "qtractorTrackTime.h"
#include "qtractorSession.h"
#include "qtractorSessionCursor.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiFile.h"

#include "qtractorOptions.h"

#include "qtractorMainForm.h"
#include "qtractorTrackForm.h"

#include <qmessagebox.h>
#include <qheader.h>
#include <qvbox.h>


// Zoom factor constants.
#define QTRACTOR_ZOOM_MIN		10
#define QTRACTOR_ZOOM_BASE		100
#define QTRACTOR_ZOOM_MAX		1000
#define QTRACTOR_ZOOM_STEP		10


//----------------------------------------------------------------------------
// qtractorTracks -- The main session track listview widget.

// Constructor.
qtractorTracks::qtractorTracks ( qtractorMainForm *pMainForm,
	QWidget *pParent, const char *pszName )
	: QSplitter(Qt::Horizontal, pParent, pszName)
{
	m_pMainForm = pMainForm;

	// To avoid contents sync moving recursion.
	m_iContentsMoving = 0;

	// Create child widgets...
	m_pTrackList = new qtractorTrackList(this, this);
	QVBox *pVBox = new QVBox(this);
	m_pTrackTime = new qtractorTrackTime(this, pVBox);
	m_pTrackView = new qtractorTrackView(this, pVBox);

	QSplitter::setResizeMode(m_pTrackList, QSplitter::KeepSize);
#if QT_VERSION >= 0x030200
	QSplitter::setHandleWidth(2);
#endif
	QWidget::setIcon(QPixmap::fromMimeSource("qtractorTracks.png"));
	QWidget::setCaption(tr("Tracks"));

	// Resize the left pane track list as last remembered.
	qtractorOptions *pOptions = m_pMainForm->options();
	if (pOptions) {
		QSize size = m_pTrackList->size();
		size.setWidth(pOptions->iTrackListWidth);
		m_pTrackList->resize(size);
	}

	// Hook the early polishing notification...
	// (needed for time scale height stabilization)
	QObject::connect(m_pTrackList, SIGNAL(polishNotifySignal()),
		this, SLOT(trackListPolishSlot()));

	// To have all views in positional sync.
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackTime, SLOT(contentsMovingSlot(int,int)));
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackList, SLOT(contentsMovingSlot(int,int)));
	QObject::connect(m_pTrackList, SIGNAL(contentsMoving(int,int)),
		m_pTrackView, SLOT(contentsMovingSlot(int,int)));
}


// Destructor.
qtractorTracks::~qtractorTracks (void)
{
}


// Close event override to emit respective signal.
void qtractorTracks::closeEvent ( QCloseEvent *pCloseEvent )
{
	emit closeNotifySignal();

	QWidget::closeEvent(pCloseEvent);
}


// Main application form accessors.
qtractorMainForm *qtractorTracks::mainForm (void) const
{
	return m_pMainForm;
}


// Session accessor.
qtractorSession *qtractorTracks::session (void) const
{
	return m_pMainForm->session();
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
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTracks::horizontalZoomStep(%d)\n", iZoomStep);
#endif

	qtractorSession *pSession = m_pMainForm->session();
	if (pSession == NULL)
		return;
		
	int iHorizontalZoom = pSession->horizontalZoom() + iZoomStep;
	if (iHorizontalZoom < QTRACTOR_ZOOM_MIN)
		iHorizontalZoom = QTRACTOR_ZOOM_MIN;
	if (iHorizontalZoom > QTRACTOR_ZOOM_MAX)
		iHorizontalZoom = QTRACTOR_ZOOM_MAX;
	if (iHorizontalZoom == pSession->horizontalZoom())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, " => iHorizontalZoom=%d\n", iHorizontalZoom);
#endif

	// Save current session frame location...
	unsigned long iFrame = m_pTrackView->sessionCursor()->frame();

	// Fix the ssession time scale zoom determinant.
	pSession->setHorizontalZoom(iHorizontalZoom);
	pSession->updateTimeScale();
	pSession->updateSessionLength();

	// Update the dependant views...
	m_pTrackView->updateContentsWidth();
	m_pTrackView->setContentsPos(
		pSession->pixelFromFrame(iFrame), m_pTrackView->contentsY());
	m_pTrackView->updateContents();

	// Notify who's watching...
	contentsChangeNotify();
}


// Vertical zoom factor.
void qtractorTracks::verticalZoomStep ( int iZoomStep )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTracks::verticalZoomStep(%d)\n", iZoomStep);
#endif

	qtractorSession *pSession = m_pMainForm->session();
	if (pSession == NULL)
		return;
		
	int iVerticalZoom = pSession->verticalZoom() + iZoomStep;
	if (iVerticalZoom < QTRACTOR_ZOOM_MIN)
		iVerticalZoom = QTRACTOR_ZOOM_MIN;
	if (iVerticalZoom > QTRACTOR_ZOOM_MAX)
		iVerticalZoom = QTRACTOR_ZOOM_MAX;
	if (iVerticalZoom == pSession->verticalZoom())
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, " => iVerticalZoom=%d\n", iVerticalZoom);
#endif

	// Fix the session vertical view zoom.
	pSession->setVerticalZoom(iVerticalZoom);
	// Update the dependant views...
	m_pTrackList->zoomItemHeight(pSession->verticalZoom());

	// Notify who's watching...
	contentsChangeNotify();
}


// Early track list stabilization.
void qtractorTracks::trackListPolishSlot (void)
{
//	int iHeaderHeight = m_pTrackList->header()->sizeHint().height();
	int iHeaderHeight = m_pTrackList->header()->height();
	m_pTrackTime->setMaximumHeight(iHeaderHeight);
	m_pTrackTime->setMinimumHeight(iHeaderHeight);
}

// Zoom view slots.
void qtractorTracks::horizontalZoomInSlot (void)
{
	horizontalZoomStep(+ QTRACTOR_ZOOM_STEP);
}

void qtractorTracks::horizontalZoomOutSlot (void)
{
	horizontalZoomStep(- QTRACTOR_ZOOM_STEP);
}

void qtractorTracks::verticalZoomInSlot (void)
{
	verticalZoomStep(+ QTRACTOR_ZOOM_STEP);
}

void qtractorTracks::verticalZoomOutSlot (void)
{
	verticalZoomStep(- QTRACTOR_ZOOM_STEP);
}

void qtractorTracks::viewZoomToolSlot (void)
{
	qtractorSession *pSession = m_pMainForm->session();
	if (pSession == NULL)
		return;

	// All zoom base are belong to us :)
	verticalZoomStep(QTRACTOR_ZOOM_BASE - pSession->verticalZoom());
}


// Common scroll view positional sync method.
void qtractorTracks::setContentsPos ( QScrollView *pScrollView, int cx, int cy )
{
	// Avoid recursion...
	if (m_iContentsMoving > 0)
		return;

	m_iContentsMoving++;
	pScrollView->setContentsPos(cx, cy);
	m_iContentsMoving--;
}


// Update/sync integral contents from session tracks.
void qtractorTracks::updateContents ( bool bRefresh )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;
	//
	// TODO: Update/sync from session tracks.
	//
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTracks::updateContents(bRefresh=%d)\n", (int) bRefresh);
#endif

	int iRefresh = 0;
	if (bRefresh)
		iRefresh++;
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack;
				pTrack = pTrack->next()) {
		// Check if item is already on list
		qtractorTrackListItem *pTrackItem = m_pTrackList->trackItem(pTrack);
		if (pTrackItem == NULL) {
			new qtractorTrackListItem(m_pTrackList, pTrack);
			iRefresh++;
		}
	}

	// Update dependant views.
	if (iRefresh > 0) {
		m_pTrackView->updateContentsHeight();
		m_pTrackView->updateContentsWidth();
		m_pTrackView->updateContents();
	}
}


// Retrieves current track reference.
qtractorTrack *qtractorTracks::currentTrack (void) const
{
	qtractorTrackListItem *pTrackItem =
		static_cast<qtractorTrackListItem *> (m_pTrackList->currentItem());
	return (pTrackItem ? pTrackItem->track() : NULL);
}


// Whether there's any clip currently selected.
bool qtractorTracks::isClipSelected (void) const
{
	return m_pTrackView->isClipSelected();
}


// Delete current selection.
void qtractorTracks::deleteClipSelect (void)
{
	m_pTrackView->deleteClipSelect();
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
qtractorTrack *qtractorTracks::addTrack (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return NULL;

	//
	// FIXME: Initial track name and type?
	//

	// Create a new track right away...
	const int iTrack = pSession->tracks().count() + 1;
	const QColor color = qtractorTrack::trackColor(iTrack);
	qtractorTrack *pTrack = new qtractorTrack(pSession, qtractorTrack::Audio);
	pTrack->setTrackName(QString("Track %1").arg(iTrack));
	pTrack->setMidiChannel(pSession->midiTag() % 16);
	pTrack->setBackground(color);
	pTrack->setForeground(color.dark());

	// Open dialog for settings...
	qtractorTrackForm trackForm(this);
	trackForm.setInstruments(m_pMainForm->instruments());
	trackForm.setTrack(pTrack);
	if (!trackForm.exec()) {
		delete pTrack;
		return NULL;
	}

	pSession->addTrack(pTrack);
	// And the new track list view item too...
	qtractorTrackListItem *pTrackItem =
		new qtractorTrackListItem(m_pTrackList, pTrack);
	// Make it the current item...
	m_pTrackList->setCurrentItem(pTrackItem);
	// Refresh view.
	updateContents(true);

	// Notify who's watching...
	contentsChangeNotify();

	// Done.
	return pTrack;
}


// Remove given(current) track from session.
bool qtractorTracks::removeTrack ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Get the list view item reference of the intended track...
	qtractorTrackListItem *pTrackItem = NULL;;
	if (pTrack)
		pTrackItem = m_pTrackList->trackItem(pTrack);
	if (pTrackItem == NULL)
		pTrackItem = static_cast<qtractorTrackListItem *> (m_pTrackList->currentItem());
	if (pTrackItem == NULL)
		return false;
	// Enforce which track to remove...
	pTrack = pTrackItem->track();

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = m_pMainForm->options();
	if (pOptions && pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove track:\n\n"
			"%1\n\n"
			"Are you sure?")
			.arg(pTrack->trackName()),
			tr("OK"), tr("Cancel")) > 0)
			return false;
	}

	// First, make it unselected, avoiding excessive signals.
	pTrackItem->setSelected(false);
	// Second, remove from session...
	pSession->removeTrack(pTrack);
	// Third, renumbering of all other remaining items.
	m_pTrackList->renumberTrackItems(pTrackItem);
	// OK. Finally remove track from list view...
	delete pTrackItem;
	// Refresh all.
	updateContents(true);

	// Notify who's watching...
	contentsChangeNotify();

	// Done.
	return true;
}


// Edit given(current) track properties.
bool qtractorTracks::editTrack ( qtractorTrack *pTrack )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Get the list view item reference of the intended track...
	qtractorTrackListItem *pTrackItem = NULL;;
	if (pTrack)
		pTrackItem = m_pTrackList->trackItem(pTrack);
	if (pTrackItem == NULL)
		pTrackItem = static_cast<qtractorTrackListItem *> (m_pTrackList->currentItem());
	if (pTrackItem == NULL)
		return false;
	// Enforce which track to edit...
	pTrack = pTrackItem->track();

	// Open dialog for settings...
	qtractorTrackForm trackForm(this);
	trackForm.setInstruments(m_pMainForm->instruments());
	trackForm.setTrack(pTrack);
	if (!trackForm.exec())
		return false;

	// Reopen to assign a probable new bus...
	if (!pTrack->open()) {
		m_pMainForm->appendMessagesError(
			tr("Track assignment failed:\n\n"
			"Track: \"%1\" Bus: \"%2\"")
			.arg(pTrack->trackName())
			.arg(pTrack->busName()));
	}

	// Refresh track item, at least the names...
	pTrackItem->setText(qtractorTrackList::Name, pTrack->trackName());
	pTrackItem->setText(qtractorTrackList::Bus,  pTrack->busName());
	// Refresh view.
	updateContents(true);

	// Notify who's watching...
	contentsChangeNotify();

	// Done.
	return true;
}


// Import Audio files into new tracks...
bool qtractorTracks::addAudioTracks ( QStringList files )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Account for actual updates...
	int iUpdate = 0;

	// For each one of those files, if any...
	for (QStringList::Iterator iter = files.begin();
			iter != files.end(); ++iter) {
		// This is one of the selected filenames....
		const QString& sPath = *iter;
		// Create a new track right away...
		const int iTrack = pSession->tracks().count() + 1;
		const QColor color = qtractorTrack::trackColor(iTrack);
		qtractorTrack *pTrack
			= new qtractorTrack(pSession, qtractorTrack::Audio);
	//	pTrack->setTrackName(QFileInfo(sPath).baseName());
		pTrack->setBackground(color);
		pTrack->setForeground(color.dark());
		// Add the clip at once...
		qtractorAudioClip *pAudioClip
			= new qtractorAudioClip(pTrack);
		pAudioClip->setClipStart(0);
		// Time for truth...
		if (pAudioClip->open(sPath)) {
			// Time to add the new track/clip into session...
			pTrack->setTrackName(pAudioClip->clipName());
			pTrack->addClip(pAudioClip);
			pSession->addTrack(pTrack);
			// And the new track list view item too...
			qtractorTrackListItem *pTrackItem =
				new qtractorTrackListItem(m_pTrackList, pTrack);
			// Make it the current item...
			m_pTrackList->setCurrentItem(pTrackItem);
			// Don't forget to add this one to local repository.
			mainForm()->addAudioFile(sPath);
			iUpdate++;
			// Log this successful import operation...
			mainForm()->appendMessages(
				tr("Audio file import succeeded: \"%1\".").arg(sPath));
		} else {
			// Bummer. Do cleanup...
			delete pAudioClip;
			delete pTrack;
			// But tell everyone that things failed.
			mainForm()->appendMessagesError(
				tr("Audio file import failure:\n\n\"%1\".").arg(sPath));
		}
		// Make things temporarily stable...
		qtractorSession::stabilize();
	}

	// Have we changed anything?
	if (iUpdate < 1)
	    return false;

	// Maybe, just maybe, we've made things larger...
	pSession->updateSessionLength();
	// Refresh view.
	updateContents(true);
	// Notify who's watching...
	contentsChangeNotify();

	// Done.
	return true;
}


// Import MIDI files into new tracks...
bool qtractorTracks::addMidiTracks ( QStringList files )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return false;

	// Account for actual updates...
	int iUpdate = 0;

	// Needed to help on setting whole session properties
	// from the first imported MIDI file...
	int iImport = pSession->tracks().count();

	// For each one of those files, if any...
	for (QStringList::Iterator iter = files.begin();
			iter != files.end(); ++iter) {
		// This is one of the selected filenames....
		const QString& sPath = *iter;
		// We'll be careful and pre-open the SMF here...
		qtractorMidiFile file;
		if (!file.open(sPath)) {
			// And tell everyone that things failed here.
			mainForm()->appendMessagesError(
				tr("MIDI file import failure:\n\n\"%1\".").arg(sPath));
		    continue;
		}
		// It all depends on the format...
		int iTracks = (file.format() == 1 ? file.tracks() : 16);
		for (int iTrackChannel = 0; iTrackChannel < iTracks; iTrackChannel++) {
			// Create a new track right away...
			const int iTrack = pSession->tracks().count() + 1;
			const QColor color = qtractorTrack::trackColor(iTrack);
			qtractorTrack *pTrack
				= new qtractorTrack(pSession, qtractorTrack::Midi);
		//	pTrack->setTrackName(QFileInfo(sPath).baseName());
			pTrack->setBackground(color);
			pTrack->setForeground(color.dark());
			// Add the clip at once...
			qtractorMidiClip *pMidiClip
				= new qtractorMidiClip(pTrack);
			pMidiClip->setClipStart(0);
			// Time for truth...
			if (pMidiClip->open(&file, iTrackChannel, iImport == 0)) {
				// Time to add the new track/clip into session...
				pTrack->setTrackName(pMidiClip->clipName());
				pTrack->setMidiChannel(pMidiClip->channel());
				pTrack->setMidiBank(pMidiClip->bank());
				pTrack->setMidiProgram(pMidiClip->program());
				pTrack->addClip(pMidiClip);
				pSession->addTrack(pTrack);
				// And the new track list view item too...
				qtractorTrackListItem *pTrackItem =
					new qtractorTrackListItem(m_pTrackList, pTrack);
				// Make it the current item...
				m_pTrackList->setCurrentItem(pTrackItem);
				// Don't forget to add this one to local repository.
				mainForm()->addMidiFile(sPath);
				iUpdate++;
			} else {
				// Bummer. Do cleanup...
				delete pMidiClip;
				delete pTrack;
			}
			// As far as the standard goes and that we'll strictly follow,
			// only the first track/channel has some tempo/time signature...
			if (iTrackChannel == 0)
				iImport++;
		}
		// Log this successful import operation...
		mainForm()->appendMessages(
			tr("MIDI file import succeeded: \"%1\".").arg(sPath));
		// Make things temporarily stable...
		qtractorSession::stabilize();
	}

	// Have we changed anything?
	if (iUpdate < 1)
	    return false;

	// Maybe, just maybe, we've made things larger...
	pSession->updateSessionLength();
	// Refresh view.
	updateContents(true);
	// Notify who's watching...
	contentsChangeNotify();

	// Done.
	return true;
}


// Simple selectionChangeSignal redirector.
void qtractorTracks::selectionChangeNotify (void)
{
	emit selectionChangeSignal();
}


// Simple contentsChangeSignal redirector.
void qtractorTracks::contentsChangeNotify (void)
{
	emit contentsChangeSignal();
}


// end of qtractorTracks.cpp
