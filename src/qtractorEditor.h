// qtractorEditor.h
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

#ifndef __qtractorEditor_h
#define __qtractorEditor_h

#include <QSplitter>


// Forward decls.
class qtractorTimeScale;
class qtractorScrollView;
class qtractorCommandList;


//----------------------------------------------------------------------------
// qtractorEditor -- Base clip editor widget.

class qtractorEditor : public QSplitter
{
public:

	// Constructor.
	qtractorEditor(QWidget *pParent);
	// Destructor.
	~qtractorEditor();

	// Event foreground (outline) color.
	void setForeground(const QColor& fore);
	const QColor& foreground() const;

	// Event background (fill) color.
	void setBackground(const QColor& back);
	const QColor& background() const;

	// Snap-to-bar zebra mode.
	void setSnapZebra(bool bSnapZebra);
	bool isSnapZebra() const;

	// Snap-to-beat grid mode.
	void setSnapGrid(bool bSnapGrid);
	bool isSnapGrid() const;

	// Floating tool-tips mode.
	void setToolTips(bool bToolTips);
	bool isToolTips() const;

	// Zoom (view) modes.
	enum { ZoomNone = 0, ZoomHorizontal = 1, ZoomVertical = 2, ZoomAll = 3 };

	void setZoomMode(int iZoomMode);
	int zoomMode() const;

	// Zoom ratio accessors.
	virtual void setHorizontalZoom(unsigned short iHorizontalZoom);
	unsigned short horizontalZoom() const;

	virtual void setVerticalZoom(unsigned short iVerticalZoom);
	unsigned short verticalZoom() const;

	// Local time scale accessors.
	qtractorTimeScale *timeScale() const;

	// Time-scale offset (in frames) accessors.
	void setOffset(unsigned long iOffset);
	unsigned long offset() const;

	// Time-scale length (in frames) accessors.
	void setLength(unsigned long iLength);
	unsigned long length() const;

	// Edit-head/tail accessors.
	void setEditHead(unsigned long iEditHead, bool bSyncView = true);
	int editHeadX() const;

	void setEditTail(unsigned long iEditTail, bool bSyncView = true);
	int editTailX() const;

	// Play-head positioning.
	void setPlayHead(unsigned long iPlayHead, bool bSyncView = true);
	int playHeadX() const;

	// Play-head follow-ness.
	void setSyncView(bool bSyncView);
	bool isSyncView() const;

	// Temporary sync-view/follow-playhead hold state.
	void setSyncViewHoldOn(bool bOn);

	void setSyncViewHold(bool bSyncViewHold);
	bool isSyncViewHold() const;

	// Return either snapped pixel/frame,
	// or the passed one if [Alt] key is pressed.
	unsigned int pixelSnap(unsigned int x) const;
	unsigned long frameSnap(unsigned long iFrame) const;

	// Show selection tooltip...
	void showToolTip(qtractorScrollView *pScrollView, const QRect& rect) const;

	// Update time-scale to master session.
	virtual void updateTimeScale() = 0;

	// Command list accessor.
	virtual qtractorCommandList *commands() const = 0;

protected:

	// Zoom factor constants.
	enum { ZoomMin = 10, ZoomBase = 100, ZoomMax = 1000, ZoomStep = 10 };

	// Zoom step evaluator.
	int zoomStep() const;

	// Common zoom factor settlers.
	void horizontalZoomStep(int iZoomStep);
	void verticalZoomStep(int iZoomStep);

	// Vertical line position drawing.
	virtual void drawPositionX(int& iPositionX, int x, bool bSyncView) = 0;

	// Clip fade-in/out accessors.
	//
	virtual int fadeInType() const = 0;
	virtual unsigned long fadeInLength() const = 0;

	virtual int fadeOutType() const = 0;
	virtual unsigned long fadeOutLength() const = 0;

	// Temporary sync-view/follow-playhead hold state.
	int m_iSyncViewHold;

private:

	// Event fore/background colors.
	QColor m_foreground;
	QColor m_background;

	// The local time scale.
	qtractorTimeScale *m_pTimeScale;

	// The local time-scale offset/length.
	unsigned long m_iOffset;
	unsigned long m_iLength;

	// Zoom mode flag.
	int m_iZoomMode;

	// Snap-to-beat/bar grid/zebra mode.
	bool m_bSnapZebra;
	bool m_bSnapGrid;

	// Floating tool-tips mode.
	bool m_bToolTips;

	// Local edit-head/tail positioning.
	int  m_iEditHeadX;
	int  m_iEditTailX;

	// Local playhead positioning.
	int  m_iPlayHeadX;
	bool m_bSyncView;

	// Temporary sync-view/follow-playhead hold state.
	bool m_bSyncViewHold;
};


#endif  // __qtractorEditor_h


// end of qtractorEditor.h
