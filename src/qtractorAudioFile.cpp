// qtractorAudioFile.cpp
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAudioFile.h"
#include "qtractorAudioSndFile.h"
#include "qtractorAudioVorbisFile.h"
#include "qtractorAudioMadFile.h"

#include <QFileInfo>
#include <QRegExp>


//----------------------------------------------------------------------
// class qtractorAudioFileFactory -- Audio file factory (singleton).
//

// Initialize singleton instance pointer.
qtractorAudioFileFactory *qtractorAudioFileFactory::g_pInstance = NULL;


// Singleton instance accessor.
qtractorAudioFileFactory& qtractorAudioFileFactory::getInstance (void)
{
	// Create the singleton instance, if not already...
	if (g_pInstance == NULL) {
		g_pInstance = new qtractorAudioFileFactory();
		std::atexit(Destroy);
	}

	return *g_pInstance;
}


// Singleton instance destroyer.
void qtractorAudioFileFactory::Destroy (void)
{
	// OK. We're done with ourselves.
	if (g_pInstance) {
		qDeleteAll(g_pInstance->m_formats);
		g_pInstance->m_formats.clear();
		g_pInstance->m_filters.clear();
		g_pInstance->m_types.clear();
		delete g_pInstance;
		g_pInstance = NULL;
	}
}


// Constructor.
qtractorAudioFileFactory::qtractorAudioFileFactory (void)
{
	// First for libsndfile stuff...
	FileFormat *pFormat;
	const QString sExtMask("*.%1");
	const QString sFilterMask("%1 (%2)");
	SF_FORMAT_INFO sffinfo;
	int iCount = 0;
	::sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &iCount, sizeof(int));
	for (int i = 0 ; i < iCount; i++) {
		sffinfo.format = i;
		::sf_command(NULL, SFC_GET_FORMAT_MAJOR, &sffinfo, sizeof(sffinfo));
		pFormat = new FileFormat;
		pFormat->type = SndFile;
		pFormat->name = QString(sffinfo.name)
			.replace('/', '-')	// Replace some illegal characters.
			.replace('(', QString::null)
			.replace(')', QString::null);
		pFormat->ext  = sffinfo.extension;
		pFormat->data = sffinfo.format;
		m_formats.append(pFormat);
		// Add for the extension map (should be unique)...
		QString sExt = pFormat->ext;
		QString sExts(sExtMask.arg(sExt));
		if (m_types.find(sExt) == m_types.end()) {
			m_types[sExt] = pFormat;
			// Take care of some old 8.3 convention,
			// specially regarding filename extensions...
			if (sExt.length() > 3) {
				sExt = sExt.left(3);
				if (m_types.find(sExt) == m_types.end()) {
					sExts = sExtMask.arg(sExt) + ' ' + sExts;
					m_types[sExt] = pFormat;
				}
			}
		}
		// What we see on dialog is some excerpt...
		m_filters.append(
			sFilterMask.arg(pFormat->name).arg(sExts));
	}

#ifdef CONFIG_LIBVORBIS
	// Add for libvorbis...
	pFormat = new FileFormat;
	pFormat->type = VorbisFile;
	pFormat->name = "OGG Vorbis";
	pFormat->ext  = "ogg";
	pFormat->data = 0;
	m_formats.append(pFormat);
	m_types[pFormat->ext] = pFormat;
	m_filters.append(
		sFilterMask.arg(pFormat->name).arg(sExtMask.arg(pFormat->ext)));
#endif

#ifdef CONFIG_LIBMAD
	// Add for libmad (mp3 read-only)...
	pFormat = new FileFormat;
	pFormat->type = MadFile;
	pFormat->name = "MPEG Layer III";
	pFormat->ext  = "mp3";
	pFormat->data = 0;
	m_formats.append(pFormat);
	m_types[pFormat->ext] = pFormat;
	m_filters.append(
		sFilterMask.arg(pFormat->name).arg(sExtMask.arg(pFormat->ext)));
#endif

	// Finally, simply build the all supported files entry.
	QRegExp rx("^(aif(|f)|fla(|c)|mp3|ogg|w(av|64))", Qt::CaseInsensitive);
	QStringList exts;
	for (FileTypes::ConstIterator iter = m_types.begin();
			iter != m_types.end(); ++iter) {
		if (rx.exactMatch(iter.key()))
			exts.append(sExtMask.arg(iter.key()));
	}
	m_filters.prepend(QObject::tr("Audio Files (%1)").arg(exts.join(" ")));
	m_filters.append(QObject::tr("All Files (*.*)"));
}


// Factory methods.
qtractorAudioFile *qtractorAudioFileFactory::createAudioFile (
	const QString& sFilename, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	return getInstance().newAudioFile(
		sFilename, iChannels, iSampleRate, iBufferSize);
}

qtractorAudioFile *qtractorAudioFileFactory::createAudioFile (
	FileType type, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	return getInstance().newAudioFile(type, iChannels, iSampleRate, iBufferSize);
}


// Internal factory methods.
qtractorAudioFile *qtractorAudioFileFactory::newAudioFile (
	const QString& sFilename, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	const QString sExt = QFileInfo(sFilename).suffix().toLower();
	
	FileTypes::ConstIterator iter = m_types.find(sExt);
	if (iter == m_types.end())
		return NULL;

	return newAudioFile(iter.value()->type, iChannels, iSampleRate, iBufferSize);
}

qtractorAudioFile *qtractorAudioFileFactory::newAudioFile (
	FileType type, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	switch (type) {
	case SndFile:
		return new qtractorAudioSndFile(iChannels, iSampleRate, iBufferSize);
	case VorbisFile:
		return new qtractorAudioVorbisFile(iChannels, iSampleRate, iBufferSize);
	case MadFile:
		return new qtractorAudioMadFile(iBufferSize);
	default:
		return NULL;
	}
}


const qtractorAudioFileFactory::FileFormats& qtractorAudioFileFactory::formats (void)
{
	return getInstance().m_formats;
}


// Retrieve supported filters (suitable for QFileDialog usage).
QString qtractorAudioFileFactory::filters (void)
{
	return getInstance().m_filters.join(";;");
}


// end of qtractorAudioFile.cpp
