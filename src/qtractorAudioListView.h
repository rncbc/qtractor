// qtractorAudioListView.h
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

#ifndef __qtractorAudioListView_h
#define __qtractorAudioListView_h

#include "qtractorFileListView.h"

// Forward declarations.
class qtractorAudioFile;
class qtractorAudioListView;


//----------------------------------------------------------------------
// class qtractorAudioFileItem -- MIDI file list view item.
//

class qtractorAudioFileItem : public qtractorFileListItem
{
public:

	// Constructor.
	qtractorAudioFileItem(const QString& sPath, qtractorAudioFile *pFile);

protected:

	// Virtual tooltip renderer.
	QString toolTip() const;
};


//----------------------------------------------------------------------------
// qtractorAudioListView -- Group/File list view, supporting drag-n-drop.
//

class qtractorAudioListView : public qtractorFileListView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioListView(QWidget *pParent = 0);

	// QListView::addColumn() ids.
	enum ItemColumn {
		Name        = 0,
		Channels    = 1,
		Frames      = 2,
		Rate        = 3,
		Time        = 4,
		Path        = 5,
		LastColumn  = 6
	};

protected:

	// Which column is the complete file path?
	int pathColumn() const { return Path; }

	// File item factory method.
	qtractorFileListItem *createFileItem(const QString& sPath);

	// Prompt for proper file list open.
	QStringList getOpenFileNames();
};


#endif  // __qtractorAudioListView_h

// end of qtractorAudioListView.h
