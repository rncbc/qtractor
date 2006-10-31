// qtractorDocument.h
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorDocument_h
#define __qtractorDocument_h

#include <QDomDocument>


//-------------------------------------------------------------------------
// qtractorDocument -- Document file import/export abstract class.
//

class qtractorDocument
{
public:

	// Constructor.
	qtractorDocument(QDomDocument *pDocument,
		const QString& sTagName = QString::null);
	// Default destructor.
	virtual ~qtractorDocument();

	// Accessors.
	QDomDocument *document() const;

	// Helper methods.
	bool    boolFromText (const QString& s) const;
	QString textFromBool (bool b) const;

	void saveTextElement (const QString& sTagName, const QString& sText,
		QDomElement *pElement);

	// External storage simple methods.
	bool load (const QString& sFilename);
	bool save (const QString& sFilename);

	// External storage element pure virtual methods.
	virtual bool loadElement (QDomElement *pElement) = 0;
	virtual bool saveElement (QDomElement *pElement) = 0;

private:

	// Instance variables.
	QDomDocument *m_pDocument;
	QString m_sTagName;
};


#endif  // __qtractorDocument_h

// end of qtractorDocument.h
