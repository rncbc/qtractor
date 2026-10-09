// qtractorEditor.cpp
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
#include "qtractorEditor.h"

#include "qtractorTimeScale.h"

#include "qtractorScrollView.h"

#include "qtractorSession.h"
#include "qtractorRubberBand.h"

#include "qtractorClipCommand.h"

#include "qtractorOptions.h"

#include <QApplication>
#include <QToolTip>
#include <QPainter>


// Follow-playhead: maximum iterations on hold.
#define QTRACTOR_SYNC_VIEW_HOLD  46

// Minimum event width (default).
#define QTRACTOR_MIN_EVENT_WIDTH 5


//----------------------------------------------------------------------------
// qtractorEditor -- Base clip editor widget.


// Constructor.
qtractorEditor::qtractorEditor ( QWidget *pParent )
	: QSplitter(Qt::Vertical, pParent)
{
	// Event fore/background colors.
	m_foreground = Qt::darkBlue;
	m_background = Qt::blue;

	// Zoom mode flag.
	m_iZoomMode = ZoomAll;

	// Snap-to-beat/bar grid/zebra mode.
	m_bSnapZebra = false;
	m_bSnapGrid  = false;

	// Floating tool-tips view mode.
	m_bToolTips = true;

	// Fade in/out control and view view mode.
	m_bFadeInOut = false;

	// Local time-scale.
	m_pTimeScale = new qtractorTimeScale();

	// The local time-scale offset/length.
	m_iOffset = 0;
	m_iLength = 0;

	// Local edit-head/tail positioning.
	m_iEditHeadX = 0;
	m_iEditTailX = 0;

	// Local play-head positioning.
	m_iPlayHeadX = 0;
	m_bSyncView  = false;

	// Temporary sync-view/follow-playhead hold state.
	m_bSyncViewHold = false;
	m_iSyncViewHold = 0;

	// Common drag state.
	m_dragFadeState  = DragFadeNone;
	m_dragFadeCursor = DragFadeNone;

	m_iDragFadeX = 0;

	m_pRubberBand = nullptr;
}


// Destructor.
qtractorEditor::~qtractorEditor (void)
{
	// Release local instances.
	delete m_pTimeScale;
}


// Event foreground (outline) color.
void qtractorEditor::setForeground ( const QColor& fore )
{
	m_foreground = fore;
}

const QColor& qtractorEditor::foreground (void) const
{
	return m_foreground;
}


// Event background (fill) color.
void qtractorEditor::setBackground ( const QColor& back )
{
	m_background = back;
}

const QColor& qtractorEditor::background (void) const
{
	return m_background;
}


// Zoom (view) mode.
void qtractorEditor::setZoomMode ( int iZoomMode )
{
	m_iZoomMode = iZoomMode;
}

int qtractorEditor::zoomMode (void) const
{
	return m_iZoomMode;
}


// Zoom ratio accessors.
void qtractorEditor::setHorizontalZoom ( unsigned short iHorizontalZoom )
{
	m_pTimeScale->setHorizontalZoom(iHorizontalZoom);
	m_pTimeScale->updateScale();
}

unsigned short qtractorEditor::horizontalZoom (void) const
{
	return m_pTimeScale->horizontalZoom();
}


void qtractorEditor::setVerticalZoom ( unsigned short iVerticalZoom )
{
	m_pTimeScale->setVerticalZoom(iVerticalZoom);
}

unsigned short qtractorEditor::verticalZoom (void) const
{
	return m_pTimeScale->verticalZoom();
}


// Snap-to-bar zebra mode.
void qtractorEditor::setSnapZebra ( bool bSnapZebra )
{
	m_bSnapZebra = bSnapZebra;
}

bool qtractorEditor::isSnapZebra (void) const
{
	return m_bSnapZebra;
}


// Snap-to-beat grid mode.
void qtractorEditor::setSnapGrid ( bool bSnapGrid )
{
	m_bSnapGrid = bSnapGrid;
}

bool qtractorEditor::isSnapGrid (void) const
{
	return m_bSnapGrid;
}


// Floating tool-tips mode.
void qtractorEditor::setToolTips ( bool bToolTips )
{
	m_bToolTips = bToolTips;
}

bool qtractorEditor::isToolTips (void) const
{
	return m_bToolTips;
}


// Fade in/out control and view view mode.
void qtractorEditor::setFadeInOut ( bool bFadeInOut )
{
	m_bFadeInOut = bFadeInOut;
}

bool qtractorEditor::isFadeInOut (void) const
{
	return m_bFadeInOut;
}


// Local time scale accessor.
qtractorTimeScale *qtractorEditor::timeScale (void) const
{
	return m_pTimeScale;
}


// Time-scale offset (in frames) accessors.
void qtractorEditor::setOffset ( unsigned long iOffset )
{
	m_iOffset = iOffset;
}

unsigned long qtractorEditor::offset (void) const
{
	return m_iOffset;
}


// Time-scale length (in frames) accessors.
void qtractorEditor::setLength ( unsigned long iLength )
{
	m_iLength = iLength;
}

unsigned long qtractorEditor::length (void) const
{
	return m_iLength;
}


// Edit-head/tail positioning.
void qtractorEditor::setEditHead (
	unsigned long iEditHead, bool bSyncView )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	if (iEditHead > pSession->editTail())
		setEditTail(iEditHead, bSyncView);
	else
		setSyncViewHoldOn(true);

	if (bSyncView)
		pSession->setEditHead(iEditHead);

	const int iEditHeadX
		= m_pTimeScale->pixelFromFrame(iEditHead)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iEditHeadX, iEditHeadX, bSyncView);
}

int qtractorEditor::editHeadX (void) const
{
	return m_iEditHeadX;
}


void qtractorEditor::setEditTail (
	unsigned long iEditTail, bool bSyncView )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	if (iEditTail < pSession->editHead())
		setEditHead(iEditTail, bSyncView);
	else
		setSyncViewHoldOn(true);

	if (bSyncView)
		pSession->setEditTail(iEditTail);

	const int iEditTailX
		= m_pTimeScale->pixelFromFrame(iEditTail)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iEditTailX, iEditTailX, bSyncView);
}

int qtractorEditor::editTailX (void) const
{
	return m_iEditTailX;
}


// Play-head positioning.
void qtractorEditor::setPlayHead (
	unsigned long iPlayHead, bool bSyncView )
{
	if (bSyncView)
		bSyncView = m_bSyncView;

	const int iPlayHeadX
		= m_pTimeScale->pixelFromFrame(iPlayHead)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iPlayHeadX, iPlayHeadX, bSyncView);
}

int qtractorEditor::playHeadX (void) const
{
	return m_iPlayHeadX;
}


// Play-head follow-ness.
void qtractorEditor::setSyncView ( bool bSyncView )
{
	m_bSyncView = bSyncView;
	m_iSyncViewHold = 0;
}

bool qtractorEditor::isSyncView (void) const
{
	return m_bSyncView;
}


// Horizontal zoom factor.
void qtractorEditor::horizontalZoomStep ( int iZoomStep )
{
	int iHorizontalZoom = horizontalZoom() + iZoomStep;
	if (iHorizontalZoom < ZoomMin)
		iHorizontalZoom = ZoomMin;
	else if (iHorizontalZoom > ZoomMax)
		iHorizontalZoom = ZoomMax;
	if (iHorizontalZoom == horizontalZoom())
		return;

	// Fix the local horizontal view zoom.
	setHorizontalZoom(iHorizontalZoom);
}


// Vertical zoom factor.
void qtractorEditor::verticalZoomStep ( int iZoomStep )
{
	int iVerticalZoom = verticalZoom() + iZoomStep;
	if (iVerticalZoom < ZoomMin)
		iVerticalZoom = ZoomMin;
	else if (iVerticalZoom > ZoomMax)
		iVerticalZoom = ZoomMax;
	if (iVerticalZoom == verticalZoom())
		return;

	// Fix the local vertical view zoom.
	setVerticalZoom(iVerticalZoom);
}


// Zoom step evaluator.
int qtractorEditor::zoomStep (void) const
{
	const Qt::KeyboardModifiers& modifiers
		= QApplication::keyboardModifiers();

	if (modifiers & Qt::ControlModifier)
		return ZoomMax;
	if (modifiers & Qt::ShiftModifier)
		return ZoomBase >> 1;

	return ZoomStep;
}


// Temporary sync-view/follow-playhead hold state.
void qtractorEditor::setSyncViewHoldOn ( bool bOn )
{
	m_iSyncViewHold = (m_bSyncViewHold && bOn ? QTRACTOR_SYNC_VIEW_HOLD : 0);
}


void qtractorEditor::setSyncViewHold ( bool bSyncViewHold )
{
	m_bSyncViewHold = bSyncViewHold;
	setSyncViewHoldOn(bSyncViewHold);
}


bool qtractorEditor::isSyncViewHold (void) const
{
	return (m_bSyncViewHold && m_iSyncViewHold > 0);
}


// Return either snapped pixel or the passed one if [Alt] key is pressed.
unsigned int qtractorEditor::pixelSnap ( unsigned int x ) const
{
	if (QApplication::keyboardModifiers() & Qt::AltModifier)
		return x;
	else
		return (m_pTimeScale ? m_pTimeScale->pixelSnap(x) : x);
}


// Return either snapped frame or the passed one if [Alt] key is pressed.
unsigned long qtractorEditor::frameSnap ( unsigned long iFrame ) const
{
	if (QApplication::keyboardModifiers() & Qt::AltModifier)
		return iFrame;
	else
		return (m_pTimeScale ? m_pTimeScale->frameSnap(iFrame) : iFrame);
}


// Show selection tooltip...
void qtractorEditor::showToolTip (
	qtractorScrollView *pScrollView, const QRect& rect ) const
{
	if (!isToolTips())
		return;

	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	const unsigned long f0 = offset();
	const unsigned long iFrameStart = frameSnap(
		pTimeScale->frameFromPixel(qMax(0, rect.left())) + f0);
	const unsigned long iFrameEnd = frameSnap(
		pTimeScale->frameFromPixel(qMax(0, rect.right())) + f0);

	QToolTip::showText(
		QCursor::pos(),
		tr("Start:\t%1\nEnd:\t%2\nLength:\t%3")
			.arg(pTimeScale->textFromFrame(iFrameStart))
			.arg(pTimeScale->textFromFrame(iFrameEnd))
			.arg(pTimeScale->textFromFrame(iFrameStart, true, iFrameEnd - iFrameStart)),
		pScrollView->viewport());
}


// Make given frame position visible in view.
void qtractorEditor::ensureVisibleFrame (
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


// Command executioner...
bool qtractorEditor::execute ( qtractorCommand *pCommand )
{
	qtractorCommandList *pCommands = commands();
	return (pCommands ? pCommands->exec(pCommand) : false);
}


// Draw the fade in/out slopes and handles.
void qtractorEditor::drawFadeInOut (
	QPainter& painter, int dx, const QRect& clipRect )
{
	if (!isFadeInOut())
		return;

	qtractorClip *pClip = clip();
	if (pClip == nullptr)
		return;

	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	// Fade in/out handle color...
	const QColor& rgbFore = foreground();
	const QColor& rgbBack = background();
	const bool bDark = (rgbBack.value() < 0xcc);
	QColor rgbFade = (bDark
		? rgbFore.lighter(240)
		: rgbFore.darker(120));
	QColor rgbHand = rgbFade;
	rgbFade.setAlpha(bDark ? 120 :  80);
	rgbHand.setAlpha(bDark ? 240 : 160);
	painter.setPen(rgbFade);
	painter.setBrush(rgbFade);

	// Fade-in slope...
	const int y = clipRect.top();
	const int h = clipRect.bottom();

	const int x0 = pTimeScale->pixelFromFrame(offset()) - dx;
	int x = clipRect.left() + x0;
	int w = pTimeScale->pixelFromFrame(pClip->fadeInLength());
	const QRect rectFadeIn(x + w, y, 10, 10);
	if (w > 0 && x + w > clipRect.left()) {
		pClip->drawFadeInOut(painter,
			qtractorClip::FadeIn, QRect(x, y, w, h));
	}

	// Fade-out slope...
	x = clipRect.left() + pTimeScale->pixelFromFrame(length()) + x0;
	w = pTimeScale->pixelFromFrame(pClip->fadeOutLength());
	const QRect rectFadeOut(x - w - 10, y, 10, 10);
	if (w > 0 && x - w < clipRect.right()) {
		pClip->drawFadeInOut(painter,
			qtractorClip::FadeOut, QRect(x, y, w, h));
	}

	// Fade in/out handles...
	if (rectFadeIn.intersects(clipRect))
		painter.fillRect(rectFadeIn, rgbHand);
	if (rectFadeOut.intersects(clipRect))
		painter.fillRect(rectFadeOut, rgbHand);
}


// Start drag-move-selecting...
bool qtractorEditor::dragMoveStart (
	qtractorScrollView *pScrollView, const QPoint& pos,
	const Qt::KeyboardModifiers& modifiers )
{
	const bool bResult
		= dragFadeInOutStart(pScrollView, pos);

	// Remember what and where we'll be dragging/selecting...
	if (bResult) {
		m_dragFadeState = DragFadeStart;
		m_posFadeStart  = pos;
	}

	return bResult;
}


// Update drag-move-selection...
bool qtractorEditor::dragMoveUpdate (
	qtractorScrollView *pScrollView, const QPoint& pos,
	const Qt::KeyboardModifiers& modifiers )
{
	bool bResult = false;

	switch (m_dragFadeState) {
	case DragFadeStart:
		// Did we moved enough around?
		bResult = true;
		if ((pos - m_posFadeStart).manhattanLength()
			< QApplication::startDragDistance())
			break;
		if (dragFadeInOutStart(pScrollView, m_posFadeStart)) {
			m_dragFadeState = m_dragFadeCursor;
			pScrollView->viewport()->setCursor(QCursor(Qt::SizeHorCursor));
		}
		pScrollView->viewport()->update();
		break;
	case DragFadeIn:
	case DragFadeOut:
		dragFadeInOutMove(pScrollView, pos);
		bResult = true;
		break;
	case DragFadeNone:
		// Try to catch mouse over the fade-in/out handles...
		bResult = dragFadeInOutStart(pScrollView, pos);
		// Fall thru...
	default:
		break;
	}

	return bResult;
}


// Commit drag-move-selection...
bool qtractorEditor::dragMoveCommit (
	qtractorScrollView *pScrollView, const QPoint& pos,
	const Qt::KeyboardModifiers& modifiers )
{
	bool bResult = false;

	switch (m_dragFadeState) {
	case DragFadeIn:
	case DragFadeOut:
		dragFadeInOutDrop(pScrollView, pos);
		bResult = true;
		break;
	case DragFadeStart:
	case DragFadeNone:
	default:
		break;
	}

	return bResult;
}


// Visualize the current drag/select/move state.
void qtractorEditor::paintDragState (
	qtractorScrollView *pScrollView, QPainter& painter )
{
	// Show/hide a moving clip fade in/out slope lines...
	if (m_dragFadeState == DragFadeIn || m_dragFadeState == DragFadeOut) {
		QRect rectHandle(m_rectFadeHandle);
		// Horizontal adjust...
		rectHandle.translate(m_iDragFadeX, 0);
		// Convert rectangle into view coordinates...
		rectHandle.moveTopLeft(
			pScrollView->contentsToViewport(rectHandle.topLeft()));
		// Draw envelope line...
		QPoint vpos;
		QPen pen(Qt::DotLine);
		pen.setColor(Qt::blue);
		painter.setPen(pen);
		if (m_dragFadeState == DragFadeIn) {
			vpos = pScrollView->contentsToViewport(m_rectFadeClip.bottomLeft());
			painter.drawLine(
				vpos.x(), vpos.y(), rectHandle.left(), rectHandle.top());
		}
		else
		if (m_dragFadeState == DragFadeOut) {
			vpos = pScrollView->contentsToViewport(m_rectFadeClip.bottomRight());
			painter.drawLine(
				rectHandle.right(), rectHandle.top(), vpos.x(), vpos.y());
		}
	}
}


// Reset drag/select/move state.
void qtractorEditor::resetDragState ( qtractorScrollView *pScrollView )
{
	if (m_pRubberBand) {
		m_pRubberBand->hide();
		delete m_pRubberBand;
		m_pRubberBand = nullptr;
	}

	if (pScrollView) {
		if (m_dragFadeState != DragFadeNone) {
			m_dragFadeCursor = DragFadeNone;
			pScrollView->viewport()->unsetCursor();
		}
		if (m_dragFadeState == DragFadeIn  ||
			m_dragFadeState == DragFadeOut) {
			pScrollView->viewport()->update();
		}
	}

	m_dragFadeState = DragFadeNone;
}


// Check whether we're up to drag a clip fade-in/out or resize handles.
bool qtractorEditor::dragFadeInOutStart (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	if (!isFadeInOut())
		return false;

	qtractorClip *pClip = clip();
	if (pClip == nullptr)
		return false;

	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return false;

	QWidget *pViewport = pScrollView->viewport();
	const int y = pScrollView->contentsY();
	const int w = pTimeScale->pixelFromFrame(length());
	const int h = pViewport->height();
	const QRect rectClip(0, y, w, h);

	// Fade-in handle check...
	m_rectFadeHandle.setRect(rectClip.left()
		+ pTimeScale->pixelFromFrame(pClip->fadeInLength()),
			rectClip.top() + 1, 10, 10);
	if (m_rectFadeHandle.contains(pos)) {
		m_dragFadeCursor = DragFadeIn;
		pViewport->setCursor(QCursor(Qt::PointingHandCursor));
		m_rectFadeClip = rectClip;
		return true;
	}

	// Fade-out handle check...
	m_rectFadeHandle.setRect(rectClip.right() - 10
		- pTimeScale->pixelFromFrame(pClip->fadeOutLength()),
			rectClip.top() + 1, 10, 10);
	if (m_rectFadeHandle.contains(pos)) {
		m_dragFadeCursor = DragFadeOut;
		pViewport->setCursor(QCursor(Qt::PointingHandCursor));
		m_rectFadeClip = rectClip;
		return true;
	}

	// Reset cursor if any persist around.
	if (m_dragFadeCursor != DragFadeNone) {
		m_dragFadeCursor  = DragFadeNone;
		pViewport->unsetCursor();
	}

	return false;
}


// Clip fade-in/out handle drag-moving parts.
void qtractorEditor::dragFadeInOutMove (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	// Always change horizontally wise...
	int dx = pos.x() - m_posFadeStart.x();
	if (m_rectFadeHandle.left() + dx < m_rectFadeClip.left())
		dx = m_rectFadeClip.left() - m_rectFadeHandle.left();
	else
	if (m_rectFadeHandle.right() + dx > m_rectFadeClip.right())
		dx = m_rectFadeClip.right() - m_rectFadeHandle.right();

	int x0 = 0;
	if (m_dragFadeState == DragFadeIn)
		x0 = m_rectFadeHandle.left();
	else
	if (m_dragFadeState == DragFadeOut)
		x0 = m_rectFadeHandle.right();

	dx += x0;

	m_iDragFadeX = (dx >= 0 ? pixelSnap(dx) : -pixelSnap(-dx)) - x0;

	moveRubberBand(pScrollView, m_rectFadeHandle);
	pScrollView->ensureVisible(pos.x(), m_rectFadeHandle.top() + 1, 24, 0);

	// Prepare to update the whole view area...
	pScrollView->viewport()->update();

	// Show fade-in/out tooltip..
	QRect rect(m_rectFadeClip);
	if (m_dragFadeState == DragFadeIn)
		rect.setRight(x0 + m_iDragFadeX);
	else
	if (m_dragFadeState == DragFadeOut)
		rect.setLeft(x0 + m_iDragFadeX);
	showToolTip(pScrollView, rect);
}


// Clip fade-in/out handle settler.
void qtractorEditor::dragFadeInOutDrop (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	dragFadeInOutMove(pScrollView, pos);

	qtractorClip *pClip = clip();
	if (pClip == nullptr)
		return;

	qtractorTimeScale *pTimeScale = timeScale();
	if (pTimeScale == nullptr)
		return;

	// We'll build a command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("clip %1").arg(
			m_dragFadeState == DragFadeIn
				? tr("fade-in") : tr("fade-out")));

	if (m_dragFadeState == DragFadeIn) {
		pClipCommand->fadeInClip(clip(),
			pTimeScale->frameFromPixel(
				m_rectFadeHandle.left() + m_iDragFadeX - m_rectFadeClip.left()),
				qtractorClip::FadeType(pClip->fadeInType()));
	}
	else
	if (m_dragFadeState == DragFadeOut) {
		pClipCommand->fadeOutClip(clip(),
			pTimeScale->frameFromPixel(
				m_rectFadeClip.right() - m_iDragFadeX - m_rectFadeHandle.right()),
				qtractorClip::FadeType(pClip->fadeOutType()));
	}

	// Reset state for proper redrawing...
	m_dragFadeState = DragFadeNone;

	// Put it in the form of an undoable command...
	execute(pClipCommand);
}


// Show and move rubber-band item.
void qtractorEditor::moveRubberBand (
	qtractorScrollView *pScrollView, const QRect& rectDrag, int thick )
{
	QRect rect(rectDrag.normalized());

	QWidget *pViewport = pScrollView->viewport();
	const int w = pViewport->width();

	// Horizontal adjust (on fade-in/out moves only)...
	if (m_dragFadeState == DragFadeIn || m_dragFadeState == DragFadeOut) {
		rect.translate(m_iDragFadeX, 0);
		// Convert rectangle into view coordinates...
		rect.moveTopLeft(pScrollView->contentsToViewport(rect.topLeft()));
	}

	// Make sure the rectangle doesn't get too off view,
	// which it would make it sluggish :)
	if (rect.left() < 0)
		rect.setLeft(-8);
	if (rect.right() > w)
		rect.setRight(w + 8);

	// Create the rubber-band if there's none...
	if (m_pRubberBand == nullptr) {
		m_pRubberBand = new qtractorRubberBand(
			QRubberBand::Rectangle, pViewport, thick);
	#if 0
		QPalette pal(m_pRubberBand->palette());
		pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
		m_pRubberBand->setPalette(pal);
		m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	#endif
	}

	// Just move it
	m_pRubberBand->setGeometry(rect);

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Command execution notification slot.
void qtractorEditor::updateNotifySlot ( unsigned int flags )
{
	if (flags & qtractorCommand::Refresh)
		updateContents();

	if (flags & qtractorCommand::Reset)
		emit changeNotifySignal(nullptr);
	else
		emit changeNotifySignal(this);
}


// Emit selection/changes.
void qtractorEditor::selectionChangeNotify (void)
{
	setSyncViewHoldOn(true);

	emit selectNotifySignal(this);
}


// end of qtractorEditor.cpp
