// qtractorDocument.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorDocument.h"

#ifdef CONFIG_LIBZ
#include "qtractorZipFile.h"
#if QT_VERSION >= 0x050000
#include <QTemporaryDir>
#endif
#endif

#include <QDomDocument>

#include <QFileInfo>
#include <QTextStream>
#include <QDir>


// Local prototypes.
static void remove_dir_list(const QList<QFileInfo>& list);
static void remove_dir(const QString& sDir);

// Remove specific file path.
static void remove_dir ( const QString& sDir )
{
	const QDir dir(sDir);

	remove_dir_list(
		dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot));

	QDir cwd = QDir::current();
	if (cwd.absolutePath() == dir.absolutePath()) {
		cwd.cdUp();
		QDir::setCurrent(cwd.path());
	}

	dir.rmdir(sDir);
}

static void remove_dir_list ( const QList<QFileInfo>& list )
{
	QListIterator<QFileInfo> iter(list);
	while (iter.hasNext()) {
		const QFileInfo& info = iter.next();
		const QString& sPath = info.absoluteFilePath();
		if (info.isDir()) {
			remove_dir(sPath);
		} else {
			QFile::remove(sPath);
		}
	}
}


//-------------------------------------------------------------------------
// qtractorDocument -- Session file import/export helper class.
//

// Default document type suffixes (file name extensions).
QString qtractorDocument::g_sDefaultExt  = "qts";
QString qtractorDocument::g_sTemplateExt = "qtt";
QString qtractorDocument::g_sArchiveExt  = "qtz";

// Extracted archive paths (static).
QStringList qtractorDocument::g_extractedArchives;

// Extra-ordinary archive files (static).
qtractorDocument *qtractorDocument::g_pDocument = NULL;


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
#ifdef CONFIG_LIBZ
	if (m_pZipFile) delete m_pZipFile;
#endif

	QStringListIterator iter(m_tempFiles);
	while (iter.hasNext()) {
		const QFileInfo temp(iter.next());
		QFile::remove(temp.absoluteFilePath());
		QDir::temp().rmpath(temp.absolutePath());
	}
}


//-------------------------------------------------------------------------
// qtractorDocument -- accessors.
//

QDomDocument *qtractorDocument::document (void) const
{
	return m_pDocument;
}

// Document root tag-name.
const QString& qtractorDocument::tagName (void) const
{
	return m_sTagName;
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

bool qtractorDocument::isTemporary (void) const
{
	return (m_flags & Temporary);
}

bool qtractorDocument::isSymLink (void) const
{
	return (m_flags & SymLink);
}


//-------------------------------------------------------------------------
// qtractorDocument -- loaders.
//

// External storage simple load method.
bool qtractorDocument::load ( const QString& sFilename, Flags flags )
{
	// Hold template mode.
	setFlags(flags);

#ifdef CONFIG_LIBZ
	// Was it an archive previously?
	if (m_pZipFile) {
		delete m_pZipFile;
		m_pZipFile = NULL;
	}
#endif

	// Is it an archive about to stuff?
	const QFileInfo info(sFilename);
	m_sName = info.completeBaseName();
	QString sDocname = info.filePath();

	const QIODevice::OpenMode mode
		= QIODevice::ReadOnly;

#ifdef CONFIG_LIBZ
	if (isArchive()) {
		// ATTN: Always move to session file's directory first...
		if (!info.isWritable() || isTemporary()) {
			// Read-only/temporary media?
			const QString& sPath
				= QDir::temp().path() + QDir::separator() + QTRACTOR_TITLE;
			QDir dir(sPath); if (!dir.exists()) dir.mkpath(sPath);
			QDir::setCurrent(sPath);
		}
		else QDir::setCurrent(info.path());
		m_pZipFile = new qtractorZipFile(sDocname, mode);
		if (!m_pZipFile->isReadable()) {
			delete m_pZipFile;
			m_pZipFile = NULL;
			return false;
		}
		m_pZipFile->setPrefix(m_sName);
		m_pZipFile->extractAll();
		m_pZipFile->close();
		delete m_pZipFile;
		m_pZipFile = NULL;
		// ATTN: Archived sub-directory must exist!
		if (!QDir(m_sName).exists()) {
			const QStringList& dirs
				= QDir().entryList(
					QStringList() << info.baseName() + '*',
					QDir::Dirs | QDir::NoDotAndDotDot,
					QDir::Time | QDir::Reversed);
			if (!dirs.isEmpty()) m_sName = dirs.first();
		}
		sDocname = m_sName + '.' + g_sDefaultExt;
		if (QDir::setCurrent(m_sName))
			g_extractedArchives.append(QDir::currentPath());
	}
	else
#endif
	QDir::setCurrent(info.absolutePath());

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

// External storage simple save method.
bool qtractorDocument::save ( const QString& sFilename, Flags flags )
{
	// Hold template mode.
	setFlags(flags);

	// We must have a valid tag name...
	if (m_sTagName.isEmpty())
		return false;

#ifdef CONFIG_LIBZ
	// Was it an archive previously?
	if (m_pZipFile) {
		delete m_pZipFile;
		m_pZipFile = NULL;
	}
#endif

	// Is it an archive about to stuff?
	const QFileInfo info(sFilename);
	m_sName = info.completeBaseName();
	QString sDocname = info.filePath();

	QDir cwd = QDir::current();
	QDir::setCurrent(info.absolutePath());

	const QIODevice::OpenMode mode
		= QIODevice::WriteOnly | QIODevice::Truncate;

#ifdef CONFIG_LIBZ
	if (isArchive()) {
		m_pZipFile = new qtractorZipFile(sDocname, mode);
		if (!m_pZipFile->isWritable()) {
			delete m_pZipFile;
			m_pZipFile = NULL;
			return false;
		}
		sDocname = m_sName + '.' + g_sDefaultExt;
		m_pZipFile->setPrefix(m_sName);
	}
#endif

	// Officially saving now...
	g_pDocument = this;

	// Save spec...
	QDomElement elem = m_pDocument->createElement(m_sTagName);
	if (!saveElement(&elem)) {
		g_pDocument = NULL;
		return false;
	}
	m_pDocument->appendChild(elem);

	// Not saving anymore...
	g_pDocument = NULL;

	// Finally, we're ready to save to external file.
	QFile file(sDocname);
#ifdef CONFIG_LIBZ
	const bool bRemove = !file.exists();
#endif
	if (!file.open(mode))
		return false;
	QTextStream ts(&file);
	ts << m_pDocument->toString() << endl;
	file.close();

#ifdef CONFIG_LIBZ
	// Commit to archive.
	if (m_pZipFile) {
		// The session document itself, at last...
		m_pZipFile->addFile(sDocname);
		m_pZipFile->processAll();
		m_pZipFile->close();
		delete m_pZipFile;
		m_pZipFile = NULL;
		// Kill temporary, if didn't exist...
		if (bRemove) file.remove();
	}
#endif

	QDir::setCurrent(cwd.absolutePath());

	return true;
}


QString qtractorDocument::addFile ( const QString& sFilename )
{
	if (!isArchive() && !isSymLink())
		return sFilename;

	const QDir& cwd = QDir::current();
	QString sAlias = cwd.relativeFilePath(sFilename);

	QFileInfo info(sFilename);
	QString sPath = info.absoluteFilePath();
	const QString sName = info.completeBaseName();
	const QString sSuffix = info.suffix().toLower();

	if (isSymLink() && info.absolutePath() != cwd.absolutePath()) {
		const QString& sLink = sName
			+ '-' + QString::number(qHash(sPath), 16)
			+ '.' + sSuffix;
		QFile(sPath).link(sLink);
		info.setFile(cwd, sLink);
		sPath = info.absoluteFilePath();
		sAlias = sLink;
	}
	else
	if (info.isSymLink()) {
		info.setFile(info.symLinkTarget());
		sPath = info.absoluteFilePath();
	}

#ifdef CONFIG_LIBZ
	if (isArchive() && m_pZipFile) {
		if (sSuffix == "sfz") {
			// SFZ archive conversion...
			sAlias = m_pZipFile->alias(sPath, sName, true);
			const QChar sep = QDir::separator();
		#if QT_VERSION >= 0x050000
			QTemporaryDir temp_dir(QDir::tempPath() + sep + m_sName + '.');
			temp_dir.setAutoRemove(false);
			const QFileInfo temp(temp_dir.path() + sep + sAlias);
		#else
			const QString& sTempDir
				= QDir::tempPath() + sep + m_sName + ".%1";
			unsigned int i = qHash(this) >> 7;
			QDir temp_dir(sTempDir.arg(i, 0, 16));
			while (temp_dir.exists())
				temp_dir.setPath(sTempDir.arg(++i, 0, 16));
			const QFileInfo temp(temp_dir, sAlias);
		#endif
			const QString& sTempname = temp.absoluteFilePath();
		#ifdef CONFIG_DEBUG
			qDebug("qtractorDocument::addFile(\"%s\") SFZ: sTempname=\"%s\"...",
				sPath.toUtf8().constData(), sTempname.toUtf8().constData());
		#endif
			// Check if temporary file already exists...
			if (!m_tempFiles.contains(sTempname) && !temp.exists()) {
				// Create the new temporary path...
				QDir::temp().mkpath(temp.absolutePath());
				// Prepare and open both file streams...
				QFile ifile(sFilename);
				QFile ofile(sTempname);
				if (ifile.open(QIODevice::ReadOnly) &&
					ofile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
					// Ready, set, go...
					const QFileInfo alias(sAlias);
					const QRegExp rxComment("//.*$");
					const QRegExp rxDefaultPath("default_path[\\s]*=(.+)$");
					const QRegExp rxSample("sample[\\s]*=(.+\\.[\\w]+)");
					// Care for default path...
					QDir dir(QFileInfo(ifile).absoluteDir());
					// Prepare the text streams...
					QTextStream its(&ifile);
					QTextStream ots(&ofile);
					while (!its.atEnd()) {
						// Read the line.
						QString sLine = its.readLine();
						QString sTemp = sLine.simplified();
						// Remove any comments...
						sTemp.remove(rxComment);
						// While not empty...
						while (!sTemp.isEmpty()) {
							if (rxDefaultPath.indexIn(sTemp) >= 0) {
								const QFileInfo fi(dir, rxDefaultPath.cap(1));
								const QString& sDefaultPath
									= fi.absoluteFilePath();
								dir.setPath(sDefaultPath);
								sTemp.remove(rxDefaultPath);
								sLine.remove(rxDefaultPath);
								sTemp = sTemp.simplified();
							}
							else
							if (rxSample.indexIn(sTemp) >= 0) {
								const QFileInfo fi(dir, rxSample.cap(1));
								const QString& sSamplePath
									= fi.absoluteFilePath();
								const QString& sSampleAlias
									= m_pZipFile->alias(sSamplePath, alias.path());
								m_pZipFile->addFile(sSamplePath, sSampleAlias);
								sTemp.remove(rxSample);
								sLine.replace(rxSample, "sample=" + fi.fileName());
								sTemp = sTemp.simplified();
							}
							else sTemp.clear();
						}
						// Write possibly altered line...
						ots << sLine << endl;
					}
					// Almost done.
					ots.flush();
					ofile.close();
					ifile.close();
					// Done.
					m_pZipFile->addFile(sTempname, sAlias);
				}
				// Cache the temporary file-path for removal...
				m_tempFiles.append(sTempname);
			}
		} else {
			// Regular file archiving...
			sAlias = m_pZipFile->alias(sPath);
			m_pZipFile->addFile(sPath, sAlias);
		}
	}
#endif	// CONFIG_LIBZ

	return sAlias;
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


//-------------------------------------------------------------------------
// qtractorDocument -- extracted archive paths simple management.
//

const QStringList& qtractorDocument::extractedArchives (void)
{
	return g_extractedArchives;
}

void qtractorDocument::clearExtractedArchives ( bool bRemove )
{
	if (bRemove) {
		QStringListIterator iter(g_extractedArchives);
		while (iter.hasNext())
			remove_dir(iter.next());
	}

	g_extractedArchives.clear();
}


//-------------------------------------------------------------------------
// qtractorDocument -- extra-ordinary archive files management.
//

QString qtractorDocument::addFile (
	const QString& sDir, const QString& sFilename )
{
	if (g_pDocument) {
		const QFileInfo info(QDir(sDir), sFilename);
		return g_pDocument->addFile(info.absoluteFilePath());
	}

	return sFilename;
}


// end of qtractorDocument.cpp
