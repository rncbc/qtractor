// qtractorTracks.cpp
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

#include "qtractorAudioClip.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"
#include "qtractorMidiFile.h"

#include "qtractorOptions.h"

#include "qtractorMainForm.h"
#include "qtractorTrackForm.h"

#include <QVBoxLayout>
#include <QToolButton>
#include <QMessageBox>
#include <QFileInfo>
#include <QDate>

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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			QList<int> sizes;
			sizes.append(160);
			sizes.append(480);
			pOptions->loadSplitterSizes(this, sizes);
		}
	}

	// Early track list stabilization.
	m_pTrackTime->setFixedHeight(
		m_pTrackList->horizontalHeader()->sizeHint().height());

	// To have all views in positional sync.
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pTrackView, SIGNAL(contentsMoving(int,int)),
		m_pTrackList, SLOT(contentsYMovingSlot(int,int)));
}


// Destructor.
qtractorTracks::~qtractorTracks (void)
{
	// Save splitter sizes...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions)
			pOptions->saveSplitterSizes(this);
	}
}


// Session helper accessor.
qtractorSession *qtractorTracks::session (void) const
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	return (pMainForm ? pMainForm->session() : NULL);
}


// Instrument list accessor.
qtractorInstrumentList *qtractorTracks::instruments (void) const
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	return (pMainForm ? pMainForm->instruments() : NULL);
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
	qtractorSession *pSession = session();
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

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTracks::horizontalZoomStep(%d)"
		" => iHorizontalZoom=%d\n", iZoomStep, iHorizontalZoom);
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
	qtractorSession *pSession = session();
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

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTracks::verticalZoomStep(%d)"
		" => iVerticalZoom=%d\n", iZoomStep, iVerticalZoom);
#endif

	// Fix the session vertical view zoom.
	pSession->setVerticalZoom(iVerticalZoom);

	// Update the dependant views...
	m_pTrackList->updateZoomHeight();

	// Notify who's watching...
	contentsChangeNotify();
}


// Zoom view slots.
void qtractorTracks::horizontalZoomInSlot (void)
{
	horizontalZoomStep(+ ZoomStep);
}

void qtractorTracks::horizontalZoomOutSlot (void)
{
	horizontalZoomStep(- ZoomStep);
}

void qtractorTracks::verticalZoomInSlot (void)
{
	verticalZoomStep(+ ZoomStep);
}

void qtractorTracks::verticalZoomOutSlot (void)
{
	verticalZoomStep(- ZoomStep);
}

void qtractorTracks::viewZoomResetSlot (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	// All zoom base are belong to us :)
	verticalZoomStep(ZoomBase - pSession->verticalZoom());
	horizontalZoomStep(ZoomBase - pSession->horizontalZoom());
}


// Update/sync integral contents from session tracks.
void qtractorTracks::updateContents ( bool bRefresh )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorTracks::updateContents(bRefresh=%d)\n", (int) bRefresh);
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
		m_pTrackView->updateContentsHeight();
		m_pTrackView->updateContentsWidth();
		m_pTrackView->updateContents();
	}
}


// Retrieves current selected track reference.
qtractorTrack *qtractorTracks::currentTrack (void) const
{
	return m_pTrackList->currentTrack();
}


// Whether there's any clip currently selected.
bool qtractorTracks::isClipSelected (void) const
{
	return m_pTrackView->isClipSelected();
}


// Whether there's any clip on clipboard.
bool qtractorTracks::isClipboardEmpty (void) const
{
	return m_pTrackView->isClipboardEmpty();
}


// Clipboard methods.
void qtractorTracks::cutClipSelect (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Cut);
}

void qtractorTracks::copyClipSelect (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Copy);
}

void qtractorTracks::pasteClipSelect (void)
{
	m_pTrackView->pasteClipSelect();
}


// Delete current selection.
void qtractorTracks::deleteClipSelect (void)
{
	m_pTrackView->executeClipSelect(qtractorTrackView::Delete);
}


// Select range interval between edit head and tail.
void qtractorTracks::selectEditRange (void)
{
	qtractorSession *pSession = session();
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

	// Nothing has really changed,
	// but we'll mark the session dirty anyway...
	contentsChangeNotify();
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Create a new track right away...
	const int iTrack = pSession->tracks().count() + 1;
	const QColor color = qtractorTrack::trackColor(iTrack);
	qtractorTrack *pTrack = new qtractorTrack(pSession);
	pTrack->setTrackName(QString("Track %1").arg(iTrack));
	pTrack->setMidiChannel(pSession->midiTag() % 16);
	pTrack->setBackground(color);
	pTrack->setForeground(color.dark());

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
	return pMainForm->commands()->exec(
		new qtractorAddTrackCommand(pMainForm, pTrack));
}


// Remove given(current) track from session.
bool qtractorTracks::removeTrack ( qtractorTrack *pTrack )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Get the list view item reference of the intended track...
	int iTrack = m_pTrackList->trackRow(pTrack);
	if (iTrack < 0)
		iTrack = m_pTrackList->currentIndex().row();
	if (iTrack < 0)
		return false;
	// Enforce which track to remove...
	pTrack = m_pTrackList->track(iTrack);

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = pMainForm->options();
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

	// Put it in the form of an undoable command...
	return pMainForm->commands()->exec(
		new qtractorRemoveTrackCommand(pMainForm, pTrack));
}


// Edit given(current) track properties.
bool qtractorTracks::editTrack ( qtractorTrack *pTrack )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Get the list view item reference of the intended track...
	int iTrack = m_pTrackList->trackRow(pTrack);
	if (iTrack < 0)
		iTrack = m_pTrackList->currentIndex().row();
	if (iTrack < 0)
		return false;
	// Enforce which track to remove...
	pTrack = m_pTrackList->track(iTrack);

	// Open dialog for settings...
	qtractorTrackForm trackForm(this);
	trackForm.setTrack(pTrack);
	if (!trackForm.exec())
		return false;

	// Put it in the form of an undoable command...
	return pMainForm->commands()->exec(
		new qtractorEditTrackCommand(pMainForm, pTrack,
			trackForm.properties()));
}


// Import Audio files into new tracks...
bool qtractorTracks::addAudioTracks ( QStringList files,
	unsigned long iClipStart )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand(pMainForm);

	// Increment this for suggestive track coloring...
	int iTrackCount = pSession->tracks().count();

	// To log this import into session description.
	QString sDescription = pSession->description() + "--\n";

	// For each one of those files, if any...
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// This is one of the selected filenames....
		const QString& sPath = iter.next();
		// Create a new track right away...
		const QColor color = qtractorTrack::trackColor(++iTrackCount);
		qtractorTrack *pTrack
			= new qtractorTrack(pSession, qtractorTrack::Audio);
	//	pTrack->setTrackName(QFileInfo(sPath).baseName());
		pTrack->setBackground(color);
		pTrack->setForeground(color.dark());
		// Add the clip at once...
		qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
		pAudioClip->setFilename(sPath);
		pAudioClip->setClipStart(iClipStart);
		// Time to add the new track/clip into session...
		pTrack->setTrackName(pAudioClip->clipName());
		pTrack->addClip(pAudioClip);
		// Add the new track to composite command...
		pImportTrackCommand->addTrack(pTrack);
		// Don't forget to add this one to local repository.
		pMainForm->addAudioFile(sPath);
		iUpdate++;
		// Log this successful import operation...
		sDescription += tr("Audio file import \"%1\" on %2 %3.\n")
			.arg(QFileInfo(sPath).fileName())
			.arg(QDate::currentDate().toString("MMM dd yyyy"))
			.arg(QTime::currentTime().toString("hh:mm:ss"));
		pMainForm->appendMessages(
			tr("Audio file import: \"%1\".").arg(sPath));
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
	return pMainForm->commands()->exec(pImportTrackCommand);
}


// Import MIDI files into new tracks...
bool qtractorTracks::addMidiTracks ( QStringList files,
	unsigned long iClipStart )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// Account for actual updates...
	int iUpdate = 0;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand(pMainForm);

	// Increment this for suggestive track coloring...
	int iTrackCount = pSession->tracks().count();

	// Needed to help on setting whole session properties
	// from the first imported MIDI file...
	int iImport = iTrackCount;
	unsigned long iTimeStart = pSession->tickFromFrame(iClipStart);

	// To log this import into session description.
	QString sDescription = pSession->description() + "--\n";

	// For each one of those files, if any...
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
			const QColor color = qtractorTrack::trackColor(++iTrackCount);
			qtractorTrack *pTrack
				= new qtractorTrack(pSession, qtractorTrack::Midi);
		//	pTrack->setTrackName(QFileInfo(sPath).baseName());
			pTrack->setBackground(color);
			pTrack->setForeground(color.dark());
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
				pImportTrackCommand->addTrack(pTrack);
				// Don't forget to add this one to local repository.
				pMainForm->addMidiFile(sPath);
				iUpdate++;
			} else {
				// Get rid of these, now...
				pTrack->unlinkClip(pMidiClip);
				delete pMidiClip;
				delete pTrack;
			}
		}
		// Log this successful import operation...
		if (iUpdate > 0) {
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
	return pMainForm->commands()->exec(pImportTrackCommand);
}


// Import MIDI file track-channel into new track...
bool qtractorTracks::addMidiTrackChannel ( const QString& sPath,
	int iTrackChannel, unsigned long iClipStart )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return false;

	// We'll build a composite command...
	qtractorImportTrackCommand *pImportTrackCommand
		= new qtractorImportTrackCommand(pMainForm);

	// Increment this for suggestive track coloring...
	int iTrackCount = pSession->tracks().count();

	// Create a new track right away...
	const QColor color = qtractorTrack::trackColor(++iTrackCount);
	qtractorTrack *pTrack
		= new qtractorTrack(pSession, qtractorTrack::Midi);
//	pTrack->setTrackName(QFileInfo(sPath).baseName());
	pTrack->setBackground(color);
	pTrack->setForeground(color.dark());
	// Add the clip at once...
	qtractorMidiClip *pMidiClip = new qtractorMidiClip(pTrack);
	pMidiClip->setFilename(sPath);
	pMidiClip->setTrackChannel(iTrackChannel);
	pMidiClip->setClipStart(iClipStart);
	// Time to add the new track/clip into session...
	pTrack->addClip(pMidiClip);
	pTrack->setTrackName(pMidiClip->clipName());
	// Add the new track to composite command...
	pImportTrackCommand->addTrack(pTrack);
	// Don't forget to add this one to local repository.
	pMainForm->addMidiFile(sPath);

	// Make things temporarily stable...
	pMainForm->appendMessages(
		tr("MIDI file import: \"%1\""
			", track-channel: %2.").arg(sPath).arg(iTrackChannel));
	qtractorSession::stabilize();

	// Put it in the form of an undoable command...
	return pMainForm->commands()->exec(pImportTrackCommand);
}


// MIDI track/bus/channel alias active maintenance method.
void qtractorTracks::updateMidiTrack ( qtractorTrack *pMidiTrack )
{
	qtractorSession *pSession = session();
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
		pMidiTrack->midiProgram());
}


// Simple main-form stabilizer redirector.
void qtractorTracks::selectionChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Simple main-form dirty-flag redirector.
void qtractorTracks::contentsChangeNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->contentsChanged();
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorTrackButtonCommand(pMainForm, pTrackButton, bOn));
}


// end of qtractorTracks.cpp
