// qtractorComboBox.h
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

#ifndef __qtractorComboBox_h
#define __qtractorComboBox_h

#include <QComboBox>


//-------------------------------------------------------------------------
// qtractorAudioFileTypeComboBox - A simple combo-box custom widget.

class qtractorAudioFileTypeComboBox : public QComboBox
{
public:

	// Constructor.
	qtractorAudioFileTypeComboBox(QWidget *pParent = nullptr);

	// Current type accessors.
	void setCurrentType(const QString& sExt, int iType = 0);
	const void *currentHandle() const;
	int currentType(const void *handle = nullptr) const;
	QString currentExt(const void *handle = nullptr) const;

	// Indexed-type accessors.
	int indexOf(const QString& sExt, int iType = 0) const;
	const void *handleOf(int iIndex) const;
};


//-------------------------------------------------------------------------
// qtractorAudioFileFormatComboBox - A simple combo-box custom widget.

class qtractorAudioFileFormatComboBox : public QComboBox
{
public:

	// Constructor.
	qtractorAudioFileFormatComboBox(QWidget *pParent = nullptr);
};


//-------------------------------------------------------------------------
// qtractorMidiFileFormatComboBox - A simple combo-box custom widget.

class qtractorMidiFileFormatComboBox : public QComboBox
{
public:

	// Constructor.
	qtractorMidiFileFormatComboBox(QWidget *pParent = nullptr);
};


#endif  // __qtractorComboBox_h


// end of qtractorComboBox.h
