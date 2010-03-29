// qtractorConnections.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorConnections_h
#define __qtractorConnections_h

#include "qtractorEngine.h"

#include <QWidget>


// Forward declarations.
class qtractorConnectForm;


//-------------------------------------------------------------------------
// qtractorConnections - Connections dockable window.
//

class qtractorConnections : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorConnections(QWidget *pParent, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorConnections();

	// Connect form accessor.
	qtractorConnectForm *connectForm() const;

	// Main bus mode switching.
	void showBus(qtractorBus *pBus, qtractorBus::BusMode busMode);

	// Complete connections refreshment.
	void refresh();

	// Complete connections recycle.
	void clear();

	// Conditional connections recycle.
	void reset();

protected:

	// Notify the main application widget that we're closing.
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

	// Just about to notify main-window that we're closing.
	void closeEvent(QCloseEvent *);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *);

private:

	// Instance variables.
	qtractorConnectForm *m_pConnectForm;
};


#endif  // __qtractorConnections_h


// end of qtractorConnections.h
