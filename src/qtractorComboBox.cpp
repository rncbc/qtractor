// qtractorComboBox.cpp
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
#include "qtractorComboBox.h"

#include "qtractorAudioFile.h"


//-------------------------------------------------------------------------
// qtractorAudioFileTypeComboBox - A simple combo-box custom widget.

// Constructor.
qtractorAudioFileTypeComboBox::qtractorAudioFileTypeComboBox (
	QWidget *pParent ) : QComboBox(pParent)
{
	int i = 0;
	const qtractorAudioFileFactory::FileFormats& formats
		= qtractorAudioFileFactory::formats();
	foreach (qtractorAudioFileFactory::FileFormat *pFormat, formats) {
		if (pFormat->type != qtractorAudioFileFactory::MadFile)
			QComboBox::addItem(pFormat->name, i);
		++i;
	}
}


// Current type accessors.
void qtractorAudioFileTypeComboBox::setCurrentType (
	const QString& sExt, int iType )
 {
	const bool bBlockSignals
		= QComboBox::blockSignals(true);
	QComboBox::setCurrentIndex(indexOf(sExt, iType));
	QComboBox::blockSignals(bBlockSignals);
}


const void *qtractorAudioFileTypeComboBox::currentHandle (void) const
{
	return handleOf(QComboBox::currentIndex());
}


int qtractorAudioFileTypeComboBox::currentType ( const void *handle ) const
{
	if (handle == nullptr)
		handle = currentHandle();
	const qtractorAudioFileFactory::FileFormat *pFormat
		= static_cast<const qtractorAudioFileFactory::FileFormat *> (handle);
	return (pFormat ? pFormat->data : 0);
}


QString qtractorAudioFileTypeComboBox::currentExt ( const void *handle ) const
{
	if (handle == nullptr)
		handle = currentHandle();
	const qtractorAudioFileFactory::FileFormat *pFormat
		= static_cast<const qtractorAudioFileFactory::FileFormat *> (handle);
	return (pFormat ? pFormat->ext : QString());
}


// Indexed-type accessors.
int qtractorAudioFileTypeComboBox::indexOf (
	const QString& sExt, int iType ) const
 {
	int i = 0;
	int iIndex = QComboBox::currentIndex();
	const qtractorAudioFileFactory::FileFormats& formats
		= qtractorAudioFileFactory::formats();
	foreach (qtractorAudioFileFactory::FileFormat *pFormat, formats) {
		if (sExt == pFormat->ext
			&& (iType == 0 || iType == pFormat->data)) {
			iIndex = QComboBox::findData(i);
			break;
		}
		++i;
	}

	return iIndex;
}


const void *qtractorAudioFileTypeComboBox::handleOf ( int iIndex ) const
{
	if (iIndex >= 0 && iIndex < QComboBox::count()) {
		const int i = QComboBox::itemData(iIndex).toInt();
		return qtractorAudioFileFactory::formats().at(i);
	} else {
		return nullptr;
	}
}


//-------------------------------------------------------------------------
// qtractorAudioFileFormatComboBox - A simple combo-box custom widget.

// Constructor.
qtractorAudioFileFormatComboBox::qtractorAudioFileFormatComboBox (
	QWidget *pParent ) : QComboBox(pParent)
{
	static QStringList s_formats;

	if (s_formats.isEmpty()) {
		s_formats.append(QObject::tr("Signed 16-Bit"));
		s_formats.append(QObject::tr("Signed 24-Bit"));
		s_formats.append(QObject::tr("Signed 32-Bit"));
		s_formats.append(QObject::tr("Float  32-Bit"));
		s_formats.append(QObject::tr("Float  64-Bit"));
	}

	QComboBox::addItems(s_formats);
}


//-------------------------------------------------------------------------
// qtractorMidiFileFormatComboBox - A simple combo-box custom widget.

// Constructor.
qtractorMidiFileFormatComboBox::qtractorMidiFileFormatComboBox (
	QWidget *pParent ) : QComboBox(pParent)
{
	static QStringList s_formats;

	if (s_formats.isEmpty()) {
		s_formats.append(QObject::tr("SMF Format 0"));
		s_formats.append(QObject::tr("SMF Format 1"));
	}

	QComboBox::addItems(s_formats);
}


// end of qtractorComboBox.cpp
