// qtractorDocument.h
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

#ifndef __qtractorDocument_h
#define __qtractorDocument_h

#include <QString>

// Forward declartions.
class QDomDocument;
class QDomElement;


//-------------------------------------------------------------------------
// qtractorDocument -- Document file import/export abstract class.
//

class qtractorDocument
{
public:

	// Constructor.
	qtractorDocument(QDomDocument *pDocument,
		const QString& sTagName = QString(), bool bTemplate = false);
	// Default destructor.
	virtual ~qtractorDocument();

	// Accessors.
	QDomDocument *document() const;

	// Template mode property.
	void setTemplate(bool bTemplate);
	bool isTemplate() const;

	void saveTextElement (const QString& sTagName, const QString& sText,
		QDomElement *pElement);

	// External storage simple methods.
	bool load (const QString& sFilename, bool bTemplate = false);
	bool save (const QString& sFilename, bool bTemplate = false);

	// External storage element pure virtual methods.
	virtual bool loadElement (QDomElement *pElement) = 0;
	virtual bool saveElement (QDomElement *pElement) = 0;

	// Helper methods.
	static bool    boolFromText (const QString& sText);
	static QString textFromBool (bool bBool);

private:

	// Instance variables.
	QDomDocument *m_pDocument;
	QString m_sTagName;
	bool m_bTemplate;
};


#endif  // __qtractorDocument_h

// end of qtractorDocument.h
