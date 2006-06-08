// qtractorConnections.cpp
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

#include "qtractorAbout.h"
#include "qtractorConnections.h"

#include "qtractorConnectForm.h"


//-------------------------------------------------------------------------
// qtractorConnections - Connections dockable window.
//

// Constructor.
qtractorConnections::qtractorConnections ( QWidget *pParent, const char *pszName )
	: QDockWindow(pParent, pszName)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	if (pszName == 0)
		QDockWindow::setName("qtractorConnections");

	// Create main inner widget.
	m_pConnectForm = new qtractorConnectForm(this);

	// Prepare the dockable window stuff.
	QDockWindow::setWidget(m_pConnectForm);
	QDockWindow::setOrientation(Qt::Horizontal);
	QDockWindow::setCloseMode(QDockWindow::Always);
	QDockWindow::setResizeEnabled(true);

	// Finally set the default caption and tooltip.
	QString sCaption = tr("Connections");
	QToolTip::add(this, sCaption);
	QDockWindow::setCaption(sCaption);
}


// Destructor.
qtractorConnections::~qtractorConnections (void)
{
	// No need to delete child widgets, Qt does it all for us.
	delete m_pConnectForm;
}


// Connect form accessor.
qtractorConnectForm *qtractorConnections::connectForm (void) const
{
	return m_pConnectForm;
}


// end of qtractorConnections.cpp
