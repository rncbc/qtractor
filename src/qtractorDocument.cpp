// qtractorDocument.cpp
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

#include "qtractorDocument.h"

#include "qtractorZipFile.h"

#include <QDomDocument>

#include <QFileInfo>
#include <QTextStream>
#include <QDir>


//-------------------------------------------------------------------------
// qtractorDocument -- Session file import/export helper class.
//

// Default document type suffixes (file name extensions).
QString qtractorDocument::g_sDefaultExt  = "qts";
QString qtractorDocument::g_sTemplateExt = "qtt";
QString qtractorDocument::g_sArchiveExt  = "qtz";


// Constructor.
qtractorDocument::qtractorDocument ( QDomDocument *pDocument,
	const QString& sTagName, Flags flags )
	: m_pDocument(pDocument), m_sTagName(sTagName), m_flags(flags),
		m_pZipFile(NULL)
{
}

// Default destructor.
qtractorDocument::~qtractorDocument (void)
{
	if (m_pZipFile) delete m_pZipFile;
}


//-------------------------------------------------------------------------
// qtractorDocument -- accessors.
//

QDomDocument *qtractorDocument::document (void) const
{
	return m_pDocument;
}

// Base document name (derived from filename).
const QString& qtractorDocument::name (void) const
{
	return m_sName;
}


// Regular text element factory method.
void qtractorDocument::saveTextElement ( const QString& sTagName,
	const QString& sText, QDomElement *pElem )
{
    QDomElement eTag = m_pDocument->createElement(sTagName);
    eTag.appendChild(m_pDocument->createTextNode(sText));
	pElem->appendChild(eTag);
}


// Document flags property.
void qtractorDocument::setFlags ( Flags flags )
{
	m_flags = flags;
}

qtractorDocument::Flags qtractorDocument::flags (void) const
{
	return m_flags;
}

bool qtractorDocument::isTemplate (void) const
{
	return (m_flags & Template);
}

bool qtractorDocument::isArchive (void) const
{
	return (m_flags & Archive);
}


//-------------------------------------------------------------------------
// qtractorDocument -- loaders.
//

// External storage simple load method.
bool qtractorDocument::load ( const QString& sFilename, Flags flags )
{
	// Hold template mode.
	setFlags(flags);

	// Was it an archive previously?
	if (m_pZipFile) {
		delete m_pZipFile;
		m_pZipFile = NULL;
	}

	// Is it an archive about to stuff?
	const QFileInfo info(sFilename);
	m_sName = info.completeBaseName();
	QString sDocname = info.filePath();
	QIODevice::OpenMode mode = QIODevice::ReadOnly;
	if (isArchive()) {
		QDir::setCurrent(info.path());
		m_pZipFile = new qtractorZipFile(sDocname, mode);
		sDocname = m_sName + '.' + g_sDefaultExt;
		m_pZipFile->extractAll();
		m_pZipFile->close();
		delete m_pZipFile;
		m_pZipFile = NULL;
		// ATTN: Archived sub-directory must exist!
		QDir::setCurrent(m_sName);
	}

	// Open file...
	QFile file(sDocname);
	if (!file.open(mode))
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

bool qtractorDocument::save ( const QString& sFilename, Flags flags )
{
	// Hold template mode.
	setFlags(flags);

	// We must have a valid tag name...
	if (m_sTagName.isEmpty())
		return false;

	// Was it an archive previously?
	if (m_pZipFile) {
		delete m_pZipFile;
		m_pZipFile = NULL;
	}

	// Is it an archive about to stuff?
	const QFileInfo info(sFilename);
	m_sName = info.completeBaseName();
	QString sDocname = info.filePath();
	QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Truncate;
	if (isArchive()) {
		m_pZipFile = new qtractorZipFile(sDocname, mode);
		sDocname = m_sName + '.' + g_sDefaultExt;
	}

	// Save spec...
	QDomElement elem = m_pDocument->createElement(m_sTagName);
	if (!saveElement(&elem))
		return false;
	m_pDocument->appendChild(elem);

	// Finally, we're ready to save to external file.
	QFile file(sDocname);
	if (!file.open(mode))
		return false;
	QTextStream ts(&file);
	ts << m_pDocument->toString() << endl;
	file.close();

	// Commit to archive.
	if (m_pZipFile) {
		m_pZipFile->addFile(sDocname, m_sName + '/' + sDocname);
		m_pZipFile->processAll();
		m_pZipFile->close();
		delete m_pZipFile;
		m_pZipFile = NULL;
		// Kill temporary.
		file.remove();
	}

	return true;
}


// Archive filename filter.
QString qtractorDocument::addFile ( const QString& sFilename ) const
{
	if (isArchive() && m_pZipFile) {
		const QString sAlias = m_pZipFile->alias(sFilename);
		m_pZipFile->addFile(sFilename, m_sName + '/' + sAlias);
		return sAlias;
	}

	return sFilename;
}


//-------------------------------------------------------------------------
// qtractorDocument -- helpers.
//

bool qtractorDocument::boolFromText ( const QString& sText )
{
	return (sText == "true" || sText == "on" || sText == "yes" || sText == "1");
}

QString qtractorDocument::textFromBool ( bool bBool )
{
	return QString::number(bBool ? 1 : 0);
}


//-------------------------------------------------------------------------
// qtractorDocument -- filename extensions (suffix) accessors.
//

void qtractorDocument::setDefaultExt ( const QString& sDefaultExt )
{
	g_sDefaultExt = sDefaultExt;
}

void qtractorDocument::setTemplateExt ( const QString& sTemplateExt )
{
	g_sTemplateExt = sTemplateExt;
}

void qtractorDocument::setArchiveExt ( const QString& sArchiveExt )
{
	g_sArchiveExt = sArchiveExt;
}


const QString& qtractorDocument::defaultExt (void)
{
	return g_sDefaultExt;
}

const QString& qtractorDocument::templateExt (void)
{
	return g_sTemplateExt;
}

const QString& qtractorDocument::archiveExt (void)
{
	return g_sArchiveExt;
}


// end of qtractorDocument.cpp
