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

#include <qobject.h>
#include <qfileinfo.h>
#include <qregexp.h>


//----------------------------------------------------------------------
// class qtractorAudioFileFactory -- Audio file factory (singleton).
//

// Initialize singleton instance pointer.
qtractorAudioFileFactory *qtractorAudioFileFactory::g_pInstance = NULL;


// Singleton instance accessor.
qtractorAudioFileFactory& qtractorAudioFileFactory::Instance (void)
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
		delete g_pInstance;
		g_pInstance = NULL;
	}
}


// Constructor.
qtractorAudioFileFactory::qtractorAudioFileFactory (void)
{
	// First for libsndfile stuff...
	const QString sMask("*.%1");
	SF_FORMAT_INFO sffinfo;
	int iCount = 0;
	::sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &iCount, sizeof(int));
	for (int i = 0 ; i < iCount; i++) {
		sffinfo.format = i;
		::sf_command(NULL, SFC_GET_FORMAT_MAJOR, &sffinfo, sizeof(sffinfo));
		const QString sName = QString(sffinfo.name)
			.replace('/', '-')	// Replace some illegal characters.
			.replace('(', QString::null)
			.replace(')', QString::null);
		QString sExt(sffinfo.extension);
		QString sExts(sMask.arg(sExt));
		m_types[sExt] = SndFile;
		// Take care of some old 8.3 convention,
		// specially regarding filename extensions...
		if (sExt.length() > 3) {
			sExt = sExt.left(3);
			sExts = sMask.arg(sExt) + ' ' + sExts;
			m_types[sExt] = SndFile;
		}
		// What we see on dialog is some excerpt...
		m_filters.append(QString("%1 (%2)").arg(sName).arg(sExts));
	}

#ifdef CONFIG_LIBVORBIS
	// Add for libvorbisfile (read-only)...
	m_types["ogg"] = VorbisFile;
	m_filters.append("OGG Vorbis (*.ogg)");
#endif

#ifdef CONFIG_LIBMAD
	// Add for libmad (mp3 read-only)...
	m_types["mp3"] = MadFile;
	m_filters.append("MPEG Layer III (*.mp3)");
#endif

	// Finally, simply build the all supported files entry.
	QRegExp rx("^(aif(|f)|fla(|c)|mp3|ogg|w(av|64))", false);
	QStringList exts;
	for (FileTypes::ConstIterator iter = m_types.begin();
			iter != m_types.end(); ++iter) {
		if (rx.exactMatch(iter.key()))
			exts.append(sMask.arg(iter.key()));
	}
	m_filters.prepend(QObject::tr("Audio Files (%1)").arg(exts.join(" ")));
	m_filters.append(QObject::tr("All Files (*.*)"));
}


// Factory methods.
qtractorAudioFile *qtractorAudioFileFactory::createAudioFile (
	const QString& sFilename, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	return Instance().newAudioFile(
		sFilename, iChannels, iSampleRate, iBufferSize);
}

qtractorAudioFile *qtractorAudioFileFactory::createAudioFile (
	FileType type, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	return Instance().newAudioFile(type, iChannels, iSampleRate, iBufferSize);
}


// Internal factory methods.
qtractorAudioFile *qtractorAudioFileFactory::newAudioFile (
	const QString& sFilename, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	const QString sExtension = QFileInfo(sFilename).extension(false).lower();
	
	FileTypes::ConstIterator iter = m_types.find(sExtension);
	if (iter == m_types.end())
		return NULL;

	return newAudioFile(iter.data(), iChannels, iSampleRate, iBufferSize);
}

qtractorAudioFile *qtractorAudioFileFactory::newAudioFile (
	FileType type, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	switch (type) {
	case SndFile:
		return new qtractorAudioSndFile(iChannels, iSampleRate, iBufferSize);
	case VorbisFile:
		return new qtractorAudioVorbisFile(iBufferSize);
	case MadFile:
		return new qtractorAudioMadFile(iBufferSize);
	default:
		return NULL;
	}
}

// Retrieve supported filters (suitable for QFileDialog usage).
QString qtractorAudioFileFactory::filters (void)
{
	return Instance().m_filters.join(";;");
}


// end of qtractorAudioFile.cpp
