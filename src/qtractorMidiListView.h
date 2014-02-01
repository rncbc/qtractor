// qtractorMidiListView.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiListView_h
#define __qtractorMidiListView_h

#include "qtractorFileListView.h"

// Forward declarations.
class qtractorMidiFile;
class qtractorMidiListView;


//----------------------------------------------------------------------
// class qtractorMidiFileItem -- MIDI file list view item.
//

class qtractorMidiFileItem : public qtractorFileListItem
{
public:

	// Constructor.
	qtractorMidiFileItem(const QString& sPath, qtractorMidiFile *pFile);

protected:

	// Virtual tooltip renderer.
	QString toolTip() const;
};


//----------------------------------------------------------------------
// class qtractorMidiChannelItem -- MIDI channel list view item.
//

class qtractorMidiChannelItem : public qtractorFileChannelItem
{
public:

	// Constructors.
	qtractorMidiChannelItem(qtractorMidiFileItem *pFileItem,
		const QString& sName, unsigned short iChannel);

protected:

	// Virtual tooltip renderer.
	QString toolTip() const;
};


//----------------------------------------------------------------------------
// qtractorMidiListView -- Group/File list view, supporting drag-n-drop.
//

class qtractorMidiListView : public qtractorFileListView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiListView(QWidget *pParent = 0);

	// QListView::addColumn() ids.
	enum ItemColumn {
		Name        = 0,
		Format      = 1,
		Tracks      = 2,
		Resolution  = 3,
		Path        = 4,
		LastColumn  = 5
	};

protected:

	// Which column is the complete file path?
	int pathColumn() const { return Path; }

	// File item factory method.
	qtractorFileListItem *createFileItem(const QString& sPath);

	// Prompt for proper file list open.
	QStringList getOpenFileNames();
};


#endif  // __qtractorMidiListView_h

// end of qtractorMidiListView.h
