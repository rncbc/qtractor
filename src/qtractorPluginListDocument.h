// qtractorPluginListDocument.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorPluginListDocument_h
#define __qtractorPluginListDocument_h

#include "qtractorDocument.h"

// Forward declarations.
class qtractorPluginList;

class qtractorPluginListDocument : public qtractorDocument
{
public:

	// Constructor.
	qtractorPluginListDocument(QDomDocument *pDocument, qtractorPluginList *pPluginList);
	// Destructor.
	~qtractorPluginListDocument();

	// External storage element overrides.
	virtual bool loadElement (QDomElement *pElement);
	virtual bool saveElement (QDomElement *pElement);

private:
	qtractorPluginList *m_pPluginList;
};

#endif // __qtractorPluginListDocument_h

// end of qtractorPluginListDocument.h
