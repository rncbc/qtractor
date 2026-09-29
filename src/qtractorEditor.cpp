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

#include "qtractorSession.h"

#include <QApplication>


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

	// Floating tool-tips mode.
	m_bToolTips = true;

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


// Return either snapped pixel, or the passed one if [Alt] key is pressed.
unsigned int qtractorEditor::pixelSnap ( unsigned int x ) const
{
	if (QApplication::keyboardModifiers() & Qt::AltModifier)
		return x;
	else
		return (m_pTimeScale ? m_pTimeScale->pixelSnap(x) : x);
}


// Return either snapped frame, or the passed one if [Alt] key is pressed.
unsigned long qtractorEditor::frameSnap ( unsigned long iFrame ) const
{
	if (QApplication::keyboardModifiers() & Qt::AltModifier)
		return iFrame;
	else
		return (m_pTimeScale ? m_pTimeScale->frameSnap(iFrame) : iFrame);
}


// end of qtractorEditor.cpp
