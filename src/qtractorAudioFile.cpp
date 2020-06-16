// qtractorAudioFile.cpp
//
/****************************************************************************
   Copyright (C) 2005-2020, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioFile.h"
#include "qtractorAudioSndFile.h"
#include "qtractorAudioVorbisFile.h"
#include "qtractorAudioMadFile.h"

#include <QRegularExpression>
#include <QFileInfo>


//----------------------------------------------------------------------
// class qtractorAudioFileFactory -- Audio file factory (singleton).
//

// Initialize singleton instance pointer.
qtractorAudioFileFactory *qtractorAudioFileFactory::g_pInstance = nullptr;

// Singleton instance accessor.
qtractorAudioFileFactory *qtractorAudioFileFactory::getInstance (void)
{
	return g_pInstance;
}


// Constructor.
qtractorAudioFileFactory::qtractorAudioFileFactory (void)
{
	// Default file format/type (for capture/record)
	m_pDefaultFormat  = nullptr;
	m_iDefaultFormat  = 0;
	m_iDefaultQuality = 4;

	// Second for libsndfile stuff...
	FileFormat *pFormat;
	const QString sExtMask("*.%1");
	const QString sFilterMask("%1 (%2)");
	SF_FORMAT_INFO sffinfo;
	int iCount = 0;
	::sf_command(nullptr, SFC_GET_FORMAT_MAJOR_COUNT, &iCount, sizeof(int));
	for (int i = 0 ; i < iCount; ++i) {
		sffinfo.format = i;
		::sf_command(nullptr, SFC_GET_FORMAT_MAJOR, &sffinfo, sizeof(sffinfo));
		pFormat = new FileFormat;
		pFormat->type = SndFile;
		pFormat->name = QString(sffinfo.name)
			.replace('/', '-')	// Replace some illegal characters.
			.replace('(', QString())
			.replace(')', QString());
		pFormat->ext  = sffinfo.extension;
		pFormat->data = sffinfo.format;
		m_formats.append(pFormat);
		// Add for the extension map (should be unique)...
		QString sExt = pFormat->ext;
		QString sExts(sExtMask.arg(sExt));
		if (!m_types.contains(sExt)) {
			m_types.insert(sExt, pFormat);
			// Take care of some old 8.3 convention,
			// specially regarding filename extensions...
			if (sExt.length() > 3) {
				sExt = sExt.left(3);
				if (!m_types.contains(sExt)) {
					sExts = sExtMask.arg(sExt) + ' ' + sExts;
					m_types.insert(sExt, pFormat);
				}
			}
			// Make a stance on the default format...
			if (sExt == "wav") m_pDefaultFormat = pFormat;
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
	m_types.insert(pFormat->ext, pFormat);
	m_filters.append(
		sFilterMask.arg(pFormat->name).arg(sExtMask.arg(pFormat->ext)));
	// Oh yeah, this will be the official default format...
	m_pDefaultFormat = pFormat;
#endif

#ifdef CONFIG_LIBMAD
	// Add for libmad (mp3 read-only)...
	pFormat = new FileFormat;
	pFormat->type = MadFile;
	pFormat->name = "MP3 MPEG-1 Audio Layer 3";
	pFormat->ext  = "mp3";
	pFormat->data = 0;
	m_formats.append(pFormat);
	m_types.insert(pFormat->ext, pFormat);
	m_filters.append(
		sFilterMask.arg(pFormat->name).arg(sExtMask.arg(pFormat->ext)));
#endif

	// Finally, simply build the all (most commonly) supported files entry.
	const QRegularExpression rx(
		"^(aif(|f)|fla(|c)|mp3|ogg|w(av|64))",
		QRegularExpression::CaseInsensitiveOption);
	QStringList exts;
	FileTypes::ConstIterator iter = m_types.constBegin();
	const FileTypes::ConstIterator& iter_end = m_types.constEnd();
	for ( ; iter != iter_end; ++iter) {
		const QString& sExt = iter.key();
		if (rx.match(sExt).hasMatch())
			exts.append(sExtMask.arg(sExt));
		m_exts.append(sExt);
	}
	m_filters.prepend(QObject::tr("Audio files (%1)").arg(exts.join(" ")));
	m_filters.append(QObject::tr("All files (*.*)"));

	g_pInstance = this;
}


// Destructor.
qtractorAudioFileFactory::~qtractorAudioFileFactory (void)
{
	g_pInstance = nullptr;

	qDeleteAll(m_formats);
	m_formats.clear();

	m_filters.clear();
	m_types.clear();
	m_exts.clear();
}


// Factory method. (static)
qtractorAudioFile *qtractorAudioFileFactory::createAudioFile (
	const QString& sFilename, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize, int iFormat )
{
	return g_pInstance->newAudioFile(
		sFilename, iChannels, iSampleRate, iBufferSize, iFormat);
}


// Internal factory method.
qtractorAudioFile *qtractorAudioFileFactory::newAudioFile (
	const QString& sFilename, unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize, int iFormat )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	const FileFormat *pFormat = m_types.value(sExt, nullptr);
	if (pFormat == nullptr)
		return nullptr;

	switch (pFormat->type) {
	case SndFile: {
		if (iFormat < 0)
			iFormat = defaultFormat();
		while (iFormat > 0 && // Retry down to PCM Signed 16-Bit...
			!qtractorAudioSndFile::isValidFormat(pFormat->data, iFormat))
			--iFormat;
		return new qtractorAudioSndFile(
			iChannels, iSampleRate, iBufferSize,
			qtractorAudioSndFile::format(pFormat->data, iFormat));
	}
	case VorbisFile: {
		if (iFormat < 0)
			iFormat = defaultQuality();
		return new qtractorAudioVorbisFile(
			iChannels, iSampleRate, iBufferSize,
			qtractorAudioVorbisFile::quality(iFormat));
	}
	case MadFile:
		return new qtractorAudioMadFile(iBufferSize);
	default:
		return nullptr;
	}
}


const qtractorAudioFileFactory::FileFormats& qtractorAudioFileFactory::formats (void)
{
	return g_pInstance->m_formats;
}


const qtractorAudioFileFactory::FileTypes& qtractorAudioFileFactory::types (void)
{
	return g_pInstance->m_types;
}


// The supported file types/names format lists.
const QStringList& qtractorAudioFileFactory::filters (void)
{
	return g_pInstance->m_filters;
}


const QStringList& qtractorAudioFileFactory::exts (void)
{
	return g_pInstance->m_exts;
}


// Default audio file format accessors
// (specific to capture/recording)
void qtractorAudioFileFactory::setDefaultType (
	const QString& sExt, int iType, int iFormat, int iQuality )
{
	// Reset for the obviusly trivial...
	g_pInstance->m_pDefaultFormat  = nullptr;
	g_pInstance->m_iDefaultFormat  = iFormat;
	g_pInstance->m_iDefaultQuality = iQuality;

	// Search for type-format...
	QListIterator<FileFormat *> iter(g_pInstance->m_formats);
	while (iter.hasNext()) {
		FileFormat *pFormat = iter.next();
		if (sExt == pFormat->ext && (iType == 0 || iType == pFormat->data)) {
			g_pInstance->m_pDefaultFormat = pFormat;
			break;
		}
	}
}


QString qtractorAudioFileFactory::defaultExt (void)
{
	FileFormat *pFormat = g_pInstance->m_pDefaultFormat;
	if (pFormat)
		return pFormat->ext;

#ifdef CONFIG_LIBVORBIS_0
	return "ogg";
#else
	return "wav";
#endif
}


int qtractorAudioFileFactory::defaultFormat (void)
{
	return g_pInstance->m_iDefaultFormat;
}


int qtractorAudioFileFactory::defaultQuality (void)
{
	return g_pInstance->m_iDefaultQuality;
}


// Check whether given file type/format is valid. (static)
bool qtractorAudioFileFactory::isValidFormat (
	const FileFormat *pFormat, int iFormat )
{
	if (pFormat == nullptr)
		return false;
	if (pFormat->type != SndFile)
		return true;

	return qtractorAudioSndFile::isValidFormat(pFormat->data, iFormat);
}


// end of qtractorAudioFile.cpp
