// qtractorConnections.h
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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorConnections_h
#define __qtractorConnections_h

#include "qtractorEngine.h"

#include <qdockwindow.h>

// Forward declarations.
class qtractorMainForm;
class qtractorConnectForm;


//-------------------------------------------------------------------------
// qtractorConnections - Connections dockable window.
//

class qtractorConnections : public QDockWindow
{
	Q_OBJECT

public:

	// Constructor.
	qtractorConnections(qtractorMainForm *pMainForm);
	// Destructor.
	~qtractorConnections();

	// Main application form accessors.
	qtractorMainForm *mainForm() const;

	// Connect form accessor.
	qtractorConnectForm *connectForm() const;

	// Session accessors.
	qtractorSession *session() const;

	// Main bus mode switching.
	void showBus(qtractorBus *pBus, qtractorBus::BusMode busMode);

	// Complete connections refreshment.
	void refresh();

	// Complete connections reset.
	void clear();

private:

	// Instance variables.
	qtractorMainForm    *m_pMainForm;
	qtractorConnectForm *m_pConnectForm;
};


#endif  // __qtractorConnections_h


// end of qtractorConnections.h
