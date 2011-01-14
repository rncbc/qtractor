// qtractorSessionDocument.h
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

#ifndef __qtractorSessionDocument_h
#define __qtractorSessionDocument_h

#include "qtractorDocument.h"

// Forward declarations.
class qtractorSession;
class qtractorFiles;


//-------------------------------------------------------------------------
// qtractorSessionDocument -- Session file import/export helper class.
//

class qtractorSessionDocument : public qtractorDocument
{
public:

	// Constructor.
	qtractorSessionDocument(QDomDocument *pDocument,
		qtractorSession *pSession, qtractorFiles *pFiles);
	// Default destructor.
	~qtractorSessionDocument();

	// Property accessors.
	qtractorSession *session() const;
	qtractorFiles   *files() const;

	// Elemental loader/savers...
	bool loadElement(QDomElement *pElement);
	bool saveElement(QDomElement *pElement);

private:

	// Instance variables.
	qtractorSession *m_pSession;
	qtractorFiles   *m_pFiles;
};


#endif  // __qtractorSessionDocument_h

// end of qtractorSessionDocument.h
