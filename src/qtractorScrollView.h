// qtractorScrollView.h
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

#ifndef __qtractorScrollView_h
#define __qtractorScrollView_h

#include <QAbstractScrollArea>
#include <QRect>

// Forward declarations.
class QResizeEvent;
class QPaintEvent;


//----------------------------------------------------------------------------
// qtractorScrollView -- abstract scroll view widget.

class qtractorScrollView : public QAbstractScrollArea
{
	Q_OBJECT

public:

	// Constructor.
	qtractorScrollView(QWidget *pParent);
	// Destructor.
	virtual ~qtractorScrollView();

	// Virtual contents extent accessors.
	int contentsX()      const { return m_rectContents.x(); }
	int contentsY()      const { return m_rectContents.y(); }
	int contentsHeight() const { return m_rectContents.height(); }
	int contentsWidth()  const { return m_rectContents.width(); }

	// Virtual contents methods.
	void setContentsPos(int cx, int cy);
	void resizeContents(int cw, int ch);

	// Scrolls contents so that given point is visible.
	void ensureVisible(int cx, int cy, int mx = 50, int my = 50);

	// Viewport/contents position converters.
	QPoint viewportToContents(const QPoint& pos) const;
	QPoint contentsToViewport(const QPoint& pos) const;

signals:

	// Contents moving slot.
	void contentsMoving(int cx, int cy);

protected:

	// Scrollbar stabilization.
	void updateScrollBars();

	// Scroll area updater.
	void scrollContentsBy(int dx, int dy);

	// Specialized event handlers.
	void resizeEvent(QResizeEvent *pResizeEvent);
	void paintEvent(QPaintEvent *pPaintEvent);
	void wheelEvent(QWheelEvent *pWheelEvent);

	// Draw the virtual contents.
	virtual void drawContents(QPainter *pPainter, const QRect& rect) = 0;

	// Rectangular contents update.
	virtual void updateContents(const QRect& rect);
	// Overall contents update.
	virtual void updateContents();

private:

	// The virtual contents coordinates.
	QRect m_rectContents;
};


#endif  // __qtractorScrollView_h


// end of qtractorScrollView.h
