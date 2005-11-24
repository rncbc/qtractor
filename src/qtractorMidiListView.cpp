// qtractorMidiListView.cpp
//
/****************************************************************************
   Copyright (C) 2003-2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorMidiListView.h"
#include "qtractorMidiFile.h"

#include <qpixmap.h>
#include <qfiledialog.h>


//----------------------------------------------------------------------
// class qtractorMidiFileItem -- audio file list view item.
//

// Constructors.
qtractorMidiFileItem::qtractorMidiFileItem (
	qtractorMidiListView *pListView, const QString& sPath,
	qtractorMidiFile *pFile )
	: qtractorFileListItem(pListView, sPath)
{
	initMidiFileItem(sPath, pFile);
}

qtractorMidiFileItem::qtractorMidiFileItem (
	qtractorFileGroupItem *pGroupItem, const QString& sPath,
	qtractorMidiFile *pFile )
	: qtractorFileListItem(pGroupItem, sPath)
{
	initMidiFileItem(sPath, pFile);
}


// Common item initializer.
void qtractorMidiFileItem::initMidiFileItem ( const QString& sPath,
	qtractorMidiFile *pFile )
{
	QListViewItem::setPixmap(qtractorMidiListView::Name,
		QPixmap::fromMimeSource("itemFile.png"));

	QListViewItem::setText(qtractorMidiListView::Format,
		QString::number(pFile->format()));
	QListViewItem::setText(qtractorMidiListView::Tracks,
		QString::number(pFile->tracks()));
	QListViewItem::setText(qtractorMidiListView::Resolution,
		QString::number(pFile->ticksPerBeat()));

	QListViewItem::setText(qtractorMidiListView::Path, sPath);

	if (pFile->format() == 1) {
		// Add track sub-items...
		for (int iTrack = pFile->tracks() - 1; iTrack >= 0; --iTrack) {
			new qtractorMidiChannelItem(this,
				QObject::tr("Track %1").arg(iTrack), iTrack);
		}
	} else {
		// Add channel sub-items...
		for (int iChannel = 15; iChannel >= 0; --iChannel) {
			new qtractorMidiChannelItem(this,
				QObject::tr("Channel %1").arg(iChannel), iChannel);
		}
	}
}


// Tooltip renderer.
QString qtractorMidiFileItem::toolTip (void) const
{
	return QObject::tr("%1 (format %2)\n%3 tracks, %4 tpqn\n%5")
		.arg(QListViewItem::text(qtractorMidiListView::Name))
		.arg(QListViewItem::text(qtractorMidiListView::Format))
		.arg(QListViewItem::text(qtractorMidiListView::Tracks))
		.arg(QListViewItem::text(qtractorMidiListView::Resolution))
		.arg(QListViewItem::text(qtractorMidiListView::Path));
}


//----------------------------------------------------------------------
// class qtractorMidiChannelItem -- MIDI track/channel view item.
//

// Constructor.
qtractorMidiChannelItem::qtractorMidiChannelItem (
	qtractorMidiFileItem *pFileItem, const QString& sName,
	unsigned short iChannel )
	: qtractorFileChannelItem(pFileItem, sName, iChannel)
{
	QListViewItem::setPixmap(qtractorMidiListView::Name,
		QPixmap::fromMimeSource("itemChannel.png"));
}


// Tooltip renderer.
QString qtractorMidiChannelItem::toolTip (void) const
{
	qtractorMidiFileItem *pFileItem
	    = static_cast<qtractorMidiFileItem *> (QListViewItem::parent());
	if (pFileItem == NULL)
	    return qtractorFileChannelItem::toolTip();

	return QObject::tr("%1 (format %2)\n%3")
		.arg(pFileItem->text(qtractorMidiListView::Name))
		.arg(pFileItem->text(qtractorMidiListView::Format))
		.arg(QListViewItem::text(qtractorMidiListView::Name));
}


//----------------------------------------------------------------------------
// qtractorMidiListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorMidiListView::qtractorMidiListView (
	QWidget *pParent, const char *pszName )
	: qtractorFileListView(pParent, pszName)
{
	QListView::addColumn(tr("Name"));		// qtractorMidiListView::Name
	QListView::addColumn(tr("Fmt"));		// qtractorMidiListView::Format
	QListView::addColumn(tr("Tracks"));		// qtractorMidiListView::Tracks
	QListView::addColumn(tr("TPQN"));		// qtractorMidiListView::Resolution
	QListView::addColumn(tr("Path"));		// qtractorMidiListView::Path

	QListView::setColumnAlignment(qtractorMidiListView::Format, Qt::AlignRight);
	QListView::setColumnAlignment(qtractorMidiListView::Tracks, Qt::AlignRight);
	QListView::setColumnAlignment(qtractorMidiListView::Resolution, Qt::AlignRight);

	QListView::setColumnWidth(qtractorMidiListView::Name, 120);
}


// File item factory method.
qtractorFileListItem *qtractorMidiListView::createFileItem (
	const QString& sPath, qtractorFileGroupItem *pParentItem )
{
	qtractorFileListItem *pFileItem = NULL;
	qtractorMidiFile file;
	if (file.open(sPath)) {
		if (pParentItem)
			pFileItem = new qtractorMidiFileItem(pParentItem, sPath, &file);
		else
			pFileItem = new qtractorMidiFileItem(this, sPath, &file);
		file.close();
	}
	return pFileItem;
}


// Prompt for proper file list open.
QStringList qtractorMidiListView::getOpenFileNames (void)
{
	// Ask for the filename to open...
	return QFileDialog::getOpenFileNames(
		tr("MIDI Files (*.mid)"),   // Filter files.
		recentDir(),                // Start here.
		this, 0,                    // Parent and name (none)
		tr("Open MIDI Files") // + " - " QTRACTOR_TITLE // Caption.
	);
}


// end of qtractorMidiListView.cpp

