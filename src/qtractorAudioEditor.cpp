// qtractorAudioEditor.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEditor.h"

#include "qtractorAudioEditTime.h"
#include "qtractorAudioEditView.h"

#include "qtractorAudioEngine.h"
#include "qtractorAudioClip.h"
#include "qtractorAudioPeak.h"

#include "qtractorTimeScale.h"

#include "qtractorTimeScaleCommand.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QPixmap>
#include <QFrame>
#include <QIcon>
#include <QPainter>

#include <QFileInfo>
#include <QDir>

#include <QComboBox>
#include <QToolTip>

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#include <QScreen>
#endif

// Follow-playhead: maximum iterations on hold.
#define QTRACTOR_SYNC_VIEW_HOLD  46

// Minimum event width (default).
#define QTRACTOR_MIN_EVENT_WIDTH 5


// An double-dash string reference (formerly empty blank).
static QString g_sDashes = "--";


//----------------------------------------------------------------------------
// qtractorAudioEdit::DragTimeScale - Specialized drag/time-scale (draft)...

struct qtractorAudioEditor::DragTimeScale
{
	DragTimeScale(qtractorTimeScale *ts, unsigned long offset)
		: cursor(ts)
	{
		node = cursor.seekFrame(offset);
		t0 = node->tickFromFrame(offset);
		x0 = ts->pixelFromFrame(offset);
	}

	qtractorTimeScale::Cursor cursor;
	qtractorTimeScale::Node *node;
	unsigned long t0;
	int x0;
};


//----------------------------------------------------------------------------
// qtractorAudioEditor -- The main Audio sequence editor widget.


// Constructor.
qtractorAudioEditor::qtractorAudioEditor ( QWidget *pParent )
	: qtractorEditor(pParent)
{
	// Initialize instance variables...
	m_pAudioClip = nullptr;
	m_pAudioPeak = nullptr;

	// The main widget splitters.
	m_pHSplitter = new QSplitter(Qt::Horizontal, this);
	m_pHSplitter->setObjectName("qtractorAudioEditor::HSplitter");

	m_pVSplitter = this;
	m_pVSplitter->setObjectName("qtractorAudioEditor::VSplitter");

	// Create child frame widgets...
	QWidget *pVBoxLeft  = new QWidget(m_pHSplitter);
	QWidget *pVBoxRight = new QWidget(m_pHSplitter);

	// Create child view widgets...
	m_pEditViewHeader = new QFrame(pVBoxLeft);
	m_pEditViewHeader->setFixedHeight(20);
	m_pEditTime  = new qtractorAudioEditTime(this, pVBoxRight);
	m_pEditTime->setFixedHeight(20);
	m_pEditView  = new qtractorAudioEditView(this, pVBoxRight);
	m_pEditViewScale = new qtractorAudioEditViewScale(this, pVBoxLeft);

	// Create child box layouts...
	QVBoxLayout *pVBoxLeftLayout = new QVBoxLayout(pVBoxLeft);
	pVBoxLeftLayout->setContentsMargins(0, 0, 0, 0);
	pVBoxLeftLayout->setSpacing(0);
	pVBoxLeftLayout->addWidget(m_pEditViewHeader);
	pVBoxLeftLayout->addWidget(m_pEditViewScale);
	pVBoxLeft->setLayout(pVBoxLeftLayout);

	QVBoxLayout *pVBoxRightLayout = new QVBoxLayout(pVBoxRight);
	pVBoxRightLayout->setContentsMargins(0, 0, 0, 0);
	pVBoxRightLayout->setSpacing(0);
	pVBoxRightLayout->addWidget(m_pEditTime);
	pVBoxRightLayout->addWidget(m_pEditView);
	pVBoxRight->setLayout(pVBoxRightLayout);

//	m_pHSplitter->setOpaqueResize(false);
	m_pHSplitter->setStretchFactor(m_pHSplitter->indexOf(pVBoxLeft), 0);
	m_pHSplitter->setHandleWidth(2);

	m_pVSplitter->setHandleWidth(0);

	m_pVSplitter->setWindowIcon(QIcon::fromTheme("qtractorAudioEditor"));
	m_pVSplitter->setWindowTitle(tr("Audio Editor"));

	QSplitter::setWindowIcon(QIcon::fromTheme("qtractorAudioEditor"));
	QSplitter::setWindowTitle(tr("Audio Editor"));

	// To have all views in positional sync.
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditTime, SLOT(contentsXMovingSlot(int,int)));

	// Initial splitter sizes.
	QList<int> sizes;
	// Initial horizontal splitter sizes...
	sizes.append(24);
	sizes.append(776);
	m_pHSplitter->setSizes(sizes);
}


// Destructor.
qtractorAudioEditor::~qtractorAudioEditor (void)
{
	resetDragState(nullptr);

	deleteAudioPeak();
}


// Editing clip accessor.
void qtractorAudioEditor::setAudioClip ( qtractorAudioClip *pAudioClip )
{
	// So, this is the brand new object to edit...
	m_pAudioClip = pAudioClip;

	if (m_pAudioClip) {
		// Open audio-peak generator...
		createAudioPeak();
		// Now set the editing audio range alright...
		setOffset(m_pAudioClip->clipStart());
		setLength(m_pAudioClip->clipLength());
		// Set its most outstanding properties...
		qtractorTrack *pTrack = m_pAudioClip->track();
		if (pTrack) {
			setForeground(pTrack->foreground());
			setBackground(pTrack->background());
		}
		// Set zoom ratios...
		const unsigned short iHorizontalZoom
			= pAudioClip->editorHorizontalZoom();
		if (iHorizontalZoom != 100)
			setHorizontalZoom(iHorizontalZoom);
		const unsigned short iVerticalZoom
			= pAudioClip->editorVerticalZoom();
		if (iVerticalZoom != 100)
			setVerticalZoom(iVerticalZoom);
		// Connect to clip's command (undo/redo) sinal-slot stack...
		qtractorCommandList *pCommands = pAudioClip->commands();
		if (pCommands) {
			QObject::connect(pCommands,
				 SIGNAL(updateNotifySignal(unsigned int)),
				 SLOT(updateNotifySlot(unsigned int)));
		}
		// Got clip!
	} else {
		// Close audio-peak generator, if any...
		deleteAudioPeak();
		// Reset those little things too..
		setOffset(0);
		setLength(0);
	}
}


qtractorAudioClip *qtractorAudioEditor::audioClip (void) const
{
	return m_pAudioClip;
}

qtractorAudioPeak *qtractorAudioEditor::audioPeak (void) const
{
	return m_pAudioPeak;
}


// Audio clip property accessors.
const QString& qtractorAudioEditor::filename (void) const
{
	return m_pAudioClip->filename();
}


unsigned short qtractorAudioEditor::channels (void) const
{
	return (m_pAudioPeak ? m_pAudioPeak->channels() : 0);
}


// Zoom ratio accessors.
void qtractorAudioEditor::setHorizontalZoom ( unsigned short iHorizontalZoom )
{
	qtractorEditor::setHorizontalZoom(iHorizontalZoom);

	if (m_pAudioClip)
		m_pAudioClip->setEditorHorizontalZoom(iHorizontalZoom);
}


void qtractorAudioEditor::setVerticalZoom ( unsigned short iVerticalZoom )
{
	qtractorEditor::setVerticalZoom(iVerticalZoom);

	if (m_pAudioClip)
		m_pAudioClip->setEditorVerticalZoom(iVerticalZoom);
}

// Audio-peak live-cycle methods.
void qtractorAudioEditor::createAudioPeak (void)
{
	deleteAudioPeak();

	if (m_pAudioClip == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	const QString& sFilename = m_pAudioClip->filename();
	const float fTimeStretch = m_pAudioClip->timeStretch();

	qtractorAudioPeakFactory *pPeakFactory
		= pSession->audioPeakFactory();
	if (pPeakFactory) {
		m_pAudioPeak = pPeakFactory->createPeak(
			sFilename, fTimeStretch);
	}
}


void qtractorAudioEditor::deleteAudioPeak (void)
{
	if (m_pAudioPeak) {
		delete m_pAudioPeak;
		m_pAudioPeak = nullptr;
	}
}


// Update time-scale to master session.
void qtractorAudioEditor::updateTimeScale (void)
{
	if (m_pAudioClip == nullptr)
		return;

	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	pTimeScale->sync(*pSession->timeScale());

	setOffset(m_pAudioClip->clipStart());
	setLength(m_pAudioClip->clipLength());

	setPlayHead(pSession->playHead(), false);
	setEditHead(pSession->editHead(), false);
	setEditTail(pSession->editTail(), false);
}


// Vertical line position drawing.
void qtractorAudioEditor::drawPositionX ( int& iPositionX, int x, bool bSyncView )
{
	// Update track-view position...
	const int x0 = m_pEditView->contentsX();
	const int w  = m_pEditView->width();
	const int h  = m_pEditView->height();
	const int wm = (w >> 3);

	// Time-line header extents...
	const int h0 = m_pEditTime->height();
	const int d0 = (h0 >> 1);

	// Restore old position...
	int x1 = iPositionX - x0;
	if (iPositionX != x && x1 >= 0 && x1 < w + d0) {
		// Override old view line...
		(m_pEditView->viewport())->update(QRect(x1, 0, 1, h));
		(m_pEditTime->viewport())->update(QRect(x1 - d0, d0, h0, d0));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && (x < x0 || x > x0 + w - wm)
	//	&& QApplication::mouseButtons() == Qt::NoButton
		&& --m_iSyncViewHold < 0) {
		// Move it...
		m_pEditView->setContentsPos(x - wm, m_pEditView->contentsY());
		m_iSyncViewHold = 0;
	} else {
		// Draw the line, by updating the new region...
		x1 = x - x0;
		if (x1 >= 0 && x1 < w + d0) {
			(m_pEditView->viewport())->update(QRect(x1, 0, 1, h));
			(m_pEditTime->viewport())->update(QRect(x1 - d0, d0, h0, d0));
		}
	}
}


// Child widgets accessors.
qtractorAudioEditTime *qtractorAudioEditor::editTime (void) const
{
	return m_pEditTime;
}

qtractorAudioEditView *qtractorAudioEditor::editView (void) const
{
	return m_pEditView;
}


// Zoom view slots.
void qtractorAudioEditor::zoomIn (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	const int iZoomMode = zoomMode();
	if (iZoomMode & ZoomHorizontal)
		horizontalZoomStep(+ ZoomStep);
	if (iZoomMode & ZoomVertical)
		verticalZoomStep(+ ZoomStep);

	zoomCenterPost(zc);
}

void qtractorAudioEditor::zoomOut (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	const int iZoomMode = zoomMode();
	if (iZoomMode & ZoomHorizontal)
		horizontalZoomStep(- ZoomStep);
	if (iZoomMode & ZoomVertical)
		verticalZoomStep(- ZoomStep);

	zoomCenterPost(zc);
}


void qtractorAudioEditor::zoomReset (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	ZoomCenter zc;
	zoomCenterPre(zc);

	const int iZoomMode = zoomMode();
	if (iZoomMode & ZoomHorizontal)
		horizontalZoomStep(ZoomBase - pTimeScale->horizontalZoom());
	if (iZoomMode & ZoomVertical)
		verticalZoomStep(ZoomBase - pTimeScale->verticalZoom());

	zoomCenterPost(zc);
}


void qtractorAudioEditor::horizontalZoomInSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(+ zoomStep());
	zoomCenterPost(zc);
}


void qtractorAudioEditor::horizontalZoomOutSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(- zoomStep());
	zoomCenterPost(zc);
}


void qtractorAudioEditor::verticalZoomInSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(+ zoomStep());
	zoomCenterPost(zc);
}


void qtractorAudioEditor::verticalZoomOutSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(- zoomStep());
	zoomCenterPost(zc);
}


void qtractorAudioEditor::horizontalZoomResetSlot (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(ZoomBase - pTimeScale->horizontalZoom());
	zoomCenterPost(zc);
}


void qtractorAudioEditor::verticalZoomResetSlot (void)
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(ZoomBase - pTimeScale->verticalZoom());
	zoomCenterPost(zc);
}




// Tell whether we can undo last command...
bool qtractorAudioEditor::canUndo (void) const
{
	qtractorCommandList *pCommands = commands();
	return (pCommands ? pCommands->lastCommand() != nullptr : false);
}

// Tell whether we can redo last command...
bool qtractorAudioEditor::canRedo (void) const
{
	qtractorCommandList *pCommands = commands();
	return (pCommands ? pCommands->nextCommand() != nullptr : false);
}


// Undo last edit command.
void qtractorAudioEditor::undoCommand (void)
{
	qtractorCommandList *pCommands = commands();
	if (pCommands)
		pCommands->undo();
}


// Redo last edit command.
void qtractorAudioEditor::redoCommand (void)
{
	qtractorCommandList *pCommands = commands();
	if (pCommands)
		pCommands->redo();
}


// Whether there's any item currently selected.
bool qtractorAudioEditor::isSelected (void) const
{
	// TODO: ?...
	return false;
}


// Whether there's any item on the clipboard.
bool qtractorAudioEditor::isClipboard (void)
{
	// TODO: ?...
	return false;
}


// Cut current selection to clipboard.
void qtractorAudioEditor::cutClipboard (void)
{
	if (m_pAudioClip == nullptr)
		return;

	if (!isSelected())
		return;

	// TODO: ?...
	//
}


// Copy current selection to clipboard.
void qtractorAudioEditor::copyClipboard (void)
{
	if (m_pAudioClip == nullptr)
		return;

	if (!isSelected())
		return;

	// TODO: ?...
	//
}


// Retrieve current paste period.
// (as from current clipboard width)
unsigned long qtractorAudioEditor::pastePeriod (void) const
{
	// TODO: ?...
	//
	return 0;
}


// Paste from clipboard.
void qtractorAudioEditor::pasteClipboard (
	unsigned short iPasteCount, unsigned long iPastePeriod )
{
	if (m_pAudioClip == nullptr)
		return;

	if (!isClipboard())
		return;

	// Reset any current selection, whatsoever...
	clearSelect();
	resetDragState(nullptr);

	// Multi-paste period...
	if (iPastePeriod < 1)
		iPastePeriod = pastePeriod();

	// TODO: ?...
	//
}


// Execute event removal.
void qtractorAudioEditor::deleteSelect (void)
{
	if (m_pAudioClip == nullptr)
		return;

	if (!isSelected())
		return;

	// TODO: ?...
	//
}


// Select all/none contents.
void qtractorAudioEditor::selectAll (
	qtractorScrollView *pScrollView, bool bSelect, bool bToggle )
{
	// Select all/none view contents.
	if (bSelect) {
		const QRect rect(0, 0,
			pScrollView->contentsWidth(),
			pScrollView->contentsHeight());
		selectRect(pScrollView,	rect, bToggle, true);
	} else {
		clearSelect();
		resetDragState(pScrollView);
		selectionChangeNotify();
	}

	// Make sure main view keeps focus...
	QWidget::activateWindow();
	pScrollView->setFocus();
}


// Select range view contents.
void qtractorAudioEditor::selectRange (
	qtractorScrollView *pScrollView, bool bToggle, bool bCommit )
{
	const int x = editHeadX();
	const int y = 0;
	const int w = editTailX() - editHeadX();
	const int h = pScrollView->contentsHeight();

	selectRect(pScrollView, QRect(x, y, w, h), bToggle, bCommit);
}


// Select everything between a given view rectangle.
void qtractorAudioEditor::selectRect ( qtractorScrollView *pScrollView,
	const QRect& rect, bool bToggle, bool bCommit )
{
	int flags = SelectNone;
	if (bToggle)
		flags |= SelectToggle;
	if (bCommit)
		flags |= SelectCommit;
	updateDragSelect(pScrollView, rect.normalized(), flags);
	resetDragState(pScrollView);
	selectionChangeNotify();
}


// Update the event selection list.
void qtractorAudioEditor::updateDragSelect (
	qtractorScrollView *pScrollView, const QRect& rectSelect, int flags )
{
	if (m_pAudioClip == nullptr)
		return;

	// TODO: ?...
	//
}


// Ensure point visibility depending on view.
void qtractorAudioEditor::ensureVisible (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	const int w = pScrollView->width();
	const int wm = (w >> 3);
	if (pos.x() > w - wm)
		m_pEditView->updateContentsWidth(pos.x() + wm);

	pScrollView->ensureVisible(pos.x(), pos.y(), 16, 16);
}


// Make given frame position visible in view.
void qtractorAudioEditor::ensureVisibleFrame (
	qtractorScrollView *pScrollView, unsigned long iFrame )
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	const int x0 = pScrollView->contentsX();
	const int y  = pScrollView->contentsY();
	const int w  = pScrollView->viewport()->width();
	const int w3 = w - (w >> 3);
	int x = pTimeScale->pixelFromFrame(iFrame)
		  - pTimeScale->pixelFromFrame(offset());
	if (x < x0)
		x -= w3;
	else if (x > x0 + w3)
		x += w3;
	pScrollView->ensureVisible(x, y, 0, 0);
//	pScrollView->setFocus();
}


// Clear all selection.
void qtractorAudioEditor::clearSelect (void)
{
	// TODO: ?...
	//
}


// Update all selection rectangular areas.
void qtractorAudioEditor::updateSelect ( bool bSelectReset )
{
	// TODO: ?...
	//
}


// Whether there's any selected range (edit-head/tail).
bool qtractorAudioEditor::isSelectable (void) const
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	return (pSession->editHead() < pSession->editTail());
}


// Update/sync integral contents.
void qtractorAudioEditor::updateContents (void)
{
	// Update dependent views.
	m_pEditView->updateContentsWidth();

	updateSelect(false);

	// Trigger a complete view update...
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
}


// Zoom centering prepare method.
// (usually before zoom change)
void qtractorAudioEditor::zoomCenterPre ( ZoomCenter& zc ) const
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	const int x0 = pTimeScale->pixelFromFrame(offset());
	const int cx = m_pEditView->contentsX();
	const int cy = m_pEditView->contentsY();

	QWidget *pViewport = m_pEditView->viewport();
	const QRect& rect = pViewport->rect();
	const QPoint& pos = pViewport->mapFromGlobal(QCursor::pos());

	zc.x = 0;
	zc.y = 0;

	const int iZoomMode = zoomMode();
	if (rect.contains(pos)) {
		if (iZoomMode & ZoomHorizontal)
			zc.x = pos.x();
		if (iZoomMode & ZoomVertical)
			zc.y = pos.y();
	} else {
		if (iZoomMode & ZoomHorizontal) {
			const int w2 = (rect.width() >> 1);
			if (cx > w2) zc.x = w2;
		}
		if (iZoomMode & ZoomVertical) {
			const int h2 = (rect.height() >> 1);
			if (cy > h2) zc.y = h2;
		}
	}

	zc.frame = pTimeScale->frameFromPixel(cx + zc.x + x0);
}


// Zoom centering post methods.
// (usually after zoom change)
void qtractorAudioEditor::zoomCenterPost ( const ZoomCenter& zc )
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	const int x0 = pTimeScale->pixelFromFrame(offset());
	int cx = pTimeScale->pixelFromFrame(zc.frame) - x0;

	// Update dependent views.
	m_pEditView->updateContentsWidth();

	updateSelect(true);

	if (zoomMode() & ZoomHorizontal) {
		if (cx > zc.x) cx -= zc.x; else cx = 0;
	}

	// Do the centering...
	m_pEditView->setContentsPos(cx, m_pEditView->contentsY());

	// Update visual cursors anyway...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		setPlayHead(pSession->playHead(), false);
		setEditHead(pSession->editHead(), false);
		setEditTail(pSession->editTail(), false);
	}

	// Trigger a complete view update...
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
}


// Visualize the event selection drag-move.
void qtractorAudioEditor::paintDragState (
	qtractorScrollView *pScrollView, QPainter *pPainter )
{
	// TODO: ?...
	//
}


// Reset drag/select/move state.
void qtractorAudioEditor::resetDragState ( qtractorScrollView *pScrollView )
{
	// TODO: ?...
	//
}


// Command list accessor.
qtractorCommandList *qtractorAudioEditor::commands (void) const
{
	return (m_pAudioClip ? m_pAudioClip->commands() : nullptr);
}


// Command executioner...
bool qtractorAudioEditor::execute ( qtractorCommand *pCommand )
{
	qtractorCommandList *pCommands = commands();
	return (pCommands ? pCommands->exec(pCommand) : false);
}


// Command execution notification slot.
void qtractorAudioEditor::updateNotifySlot ( unsigned int flags )
{
	if (flags & qtractorCommand::Refresh)
		updateContents();

	if (flags & qtractorCommand::Reset)
		emit changeNotifySignal(nullptr);
	else
		emit changeNotifySignal(this);
}


// Emit selection/changes.
void qtractorAudioEditor::selectionChangeNotify (void)
{
	setSyncViewHoldOn(true);

	emit selectNotifySignal(this);
}


// Keyboard event handler (common).
bool qtractorAudioEditor::keyPress ( qtractorScrollView *pScrollView,
	int iKey, const Qt::KeyboardModifiers& modifiers )
{
	switch (iKey) {
	case Qt::Key_Insert: // Aha, joking :)
	case Qt::Key_Return:
	case Qt::Key_Escape:
		resetDragState(pScrollView);
		break;
	case Qt::Key_Home:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(0, 0);
		} else {
			pScrollView->setContentsPos(0, pScrollView->contentsY());
		}
		break;
	case Qt::Key_End:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsWidth()  - pScrollView->width(),
				pScrollView->contentsHeight() - pScrollView->height());
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsWidth()  - pScrollView->width(),
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Left:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() - pScrollView->width(),
				pScrollView->contentsY());
		} else if (!keyStep(pScrollView, iKey, modifiers)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() - 16,
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Right:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() + pScrollView->width(),
				pScrollView->contentsY());
		} else if (!keyStep(pScrollView, iKey, modifiers)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() + 16,
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Up:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - pScrollView->height());
		} else if (!keyStep(pScrollView, iKey, modifiers)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - 16);
		}
		break;
	case Qt::Key_Down:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + pScrollView->height());
		} else if (!keyStep(pScrollView, iKey, modifiers)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + 16);
		}
		break;
	case Qt::Key_PageUp:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(), 16);
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - pScrollView->height());
		}
		break;
	case Qt::Key_PageDown:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsHeight() - pScrollView->height());
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + pScrollView->height());
		}
		break;
	default:
		// Not handled here.
		return false;
	}

	// Done.
	return true;
}


// Keyboard step handler.
bool qtractorAudioEditor::keyStep ( qtractorScrollView *pScrollView,
	int iKey, const Qt::KeyboardModifiers& modifiers )
{
	// Only applicable if something is selected...
	// TODO: ?...
	return false;

}


// end of qtractorAudioEditor.cpp
