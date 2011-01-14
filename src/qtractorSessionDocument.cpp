// qtractorSessionDocument.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorSessionDocument.h"

#include "qtractorSession.h"


//-------------------------------------------------------------------------
// qtractorSessionDocument -- Session file import/export helper class.
//

// Constructor.
qtractorSessionDocument::qtractorSessionDocument ( QDomDocument *pDocument,
	qtractorSession *pSession, qtractorFiles *pFiles )
	: qtractorDocument(pDocument, "session")
{
	m_pSession = pSession;
	m_pFiles   = pFiles;
}


// Default destructor.
qtractorSessionDocument::~qtractorSessionDocument (void)
{
}


// Session accessor.
qtractorSession *qtractorSessionDocument::session (void) const
{
	return m_pSession;
}


// File list accessor.
qtractorFiles *qtractorSessionDocument::files (void) const
{
	return m_pFiles;
}


// The elemental loader implementation.
bool qtractorSessionDocument::loadElement ( QDomElement *pElement )
{
	return m_pSession->loadElement(this, pElement);
}


// The elemental saver implementation.
bool qtractorSessionDocument::saveElement ( QDomElement *pElement )
{
	return m_pSession->saveElement(this, pElement);
}


// end of qtractorSessionDocument.cpp
