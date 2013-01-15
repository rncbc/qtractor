// qtractorDocument.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

class qtractorZipFile;


//-------------------------------------------------------------------------
// qtractorDocument -- Document file import/export abstract class.
//

class qtractorDocument
{
public:

	// Document flags.
	enum Flags { Default = 0, Template = 1, Archive = 2, Temporary = 4 };

	// Constructor.
	qtractorDocument(QDomDocument *pDocument,
		const QString& sTagName = QString(), Flags = Default);
	// Default destructor.
	virtual ~qtractorDocument();

	// Accessors.
	QDomDocument *document() const;
	const QString& name() const;

	// Regular text element factory method.
	void saveTextElement (const QString& sTagName, const QString& sText,
		QDomElement *pElement);

	// Document flags property.
	void setFlags(Flags flags);
	Flags flags() const;

	bool isTemplate() const;
	bool isArchive() const;
	bool isTemporary() const;

	// Archive filename filter.
	QString addFile (const QString& sFilename) const;

	// External storage simple methods.
	bool load (const QString& sFilename, Flags flags = Default);
	bool save (const QString& sFilename, Flags flags = Default);

	// External storage element pure virtual methods.
	virtual bool loadElement (QDomElement *pElement) = 0;
	virtual bool saveElement (QDomElement *pElement) = 0;

	// Helper methods.
	static bool    boolFromText (const QString& sText);
	static QString textFromBool (bool bBool);

	// Filename extensions (suffix) accessors.
	static void setDefaultExt  (const QString& sDefaultExt);
	static void setTemplateExt (const QString& sTemplateExt);
	static void setArchiveExt  (const QString& sArchiveExt);

	static const QString& defaultExt();
	static const QString& templateExt();
	static const QString& archiveExt();

	// Extracted archive paths simple management.
	static const QStringList& extractedArchives();
	static void clearExtractedArchives(bool bRemove = false);

	// Extra-ordinary archive files management.
	static QString addArchiveFile(const QString& sFilename);

private:

	// Instance variables.
	QDomDocument *m_pDocument;
	QString m_sTagName;
	Flags m_flags;

	// Base document name (derived from filename).
	QString m_sName;

	// Archive stuff.
	qtractorZipFile *m_pZipFile;

	// Filename extensions (file suffixes).
	static QString g_sDefaultExt;
	static QString g_sTemplateExt;
	static QString g_sArchiveExt;

	// Extracted archive paths.
	static QStringList g_extractedArchives;

	// Extra-ordinary archive files.
	static qtractorDocument *g_pArchive;
};


#endif  // __qtractorDocument_h

// end of qtractorDocument.h
