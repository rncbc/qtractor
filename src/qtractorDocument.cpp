// qtractorDocument.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorDocument.h"

#include <qfileinfo.h>
#include <qtextstream.h>


//-------------------------------------------------------------------------
// qtractorDocument -- Session file import/export helper class.
//

// Constructor.
qtractorDocument::qtractorDocument ( QDomDocument *pDocument,
	const QString& sTagName )
{
	m_pDocument = pDocument;
	m_sTagName  = sTagName;
}

// Default destructor.
qtractorDocument::~qtractorDocument (void)
{
}


//-------------------------------------------------------------------------
// qtractorDocument -- accessors.
//

QDomDocument *qtractorDocument::document (void) const
{
	return m_pDocument;
}


//-------------------------------------------------------------------------
// qtractorDocument -- helpers.
//

bool qtractorDocument::boolFromText ( const QString& s ) const
{
	return (s == "true" || s == "on" || s == "yes" || s == "1");
}

QString qtractorDocument::textFromBool ( bool b ) const
{
	return QString::number(b ? 1 : 0);
}


void qtractorDocument::saveTextElement ( const QString& sTagName,
	const QString& sText, QDomElement *pElem )
{
    QDomElement eTag = m_pDocument->createElement(sTagName);
    eTag.appendChild(m_pDocument->createTextNode(sText));
	pElem->appendChild(eTag);
}


//-------------------------------------------------------------------------
// qtractorDocument -- loaders.
//

// External storage simple load method.
bool qtractorDocument::load ( const QString& sFilename )
{
	// Open file...
	QFile file(sFilename);
	if (!file.open(IO_ReadOnly))
		return false;
	// Parse it a-la-DOM :-)
	if (!m_pDocument->setContent(&file)) {
		file.close();
		return false;
	}
	file.close();

	// Get root element and check for proper taqg name.
	QDomElement elem = m_pDocument->documentElement();
	if (elem.tagName() != m_sTagName)
	    return false;

	return loadElement(&elem);
}


//-------------------------------------------------------------------------
// qtractorDocument -- savers.
//

bool qtractorDocument::save ( const QString& sFilename )
{
	// We must have a valid tag name...
	if (m_sTagName.isEmpty())
		return false;

	// Save spec...
	QDomElement elem = m_pDocument->createElement(m_sTagName);
	if (!saveElement(&elem))
		return false;
	m_pDocument->appendChild(elem);

	// Finally, we're ready to save to external file.
	QFile file(sFilename);
	if (!file.open(IO_WriteOnly | IO_Truncate))
		return false;
	QTextStream ts(&file);
	ts << m_pDocument->toString() << endl;
	file.close();

	return true;
}


// end of qtractorDocument.cpp
