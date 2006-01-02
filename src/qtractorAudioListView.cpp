// qtractorAudioListView.cpp
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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAudioListView.h"
#include "qtractorAudioFile.h"

#include <qpixmap.h>
#include <qfiledialog.h>


//----------------------------------------------------------------------
// class qtractorAudioFileItem -- audio file list view item.
//

// Constructors.
qtractorAudioFileItem::qtractorAudioFileItem (
	qtractorAudioListView *pListView, const QString& sPath,
	qtractorAudioFile *pFile )
	: qtractorFileListItem(pListView, sPath)
{
	initAudioFileItem(sPath, pFile);
}

qtractorAudioFileItem::qtractorAudioFileItem (
	qtractorFileGroupItem *pGroupItem, const QString& sPath,
	qtractorAudioFile *pFile )
	: qtractorFileListItem(pGroupItem, sPath)
{
	initAudioFileItem(sPath, pFile);
}


// Common item initializer.
void qtractorAudioFileItem::initAudioFileItem ( const QString& sPath,
	qtractorAudioFile *pFile )
{
	QListViewItem::setPixmap(qtractorAudioListView::Name,
		QPixmap::fromMimeSource("itemFile.png"));

	QListViewItem::setText(qtractorAudioListView::Channels,
		QString::number(pFile->channels()));
	QListViewItem::setText(qtractorAudioListView::Frames,
		QString::number(pFile->frames()));
	QListViewItem::setText(qtractorAudioListView::Rate,
		QString::number(pFile->sampleRate()));

	QString sTime;
	unsigned int hh, mm, ss, ddd;
	float secs = (float) pFile->frames() / (float) pFile->sampleRate();
	hh = mm = ss = 0;
	if (secs >= 3600.0) {
		hh = (unsigned int) (secs / 3600.0);
		secs -= (float) hh * 3600.0;
	}
	if (secs >= 60.0) {
		mm = (unsigned int) (secs / 60.0);
		secs -= (float) mm * 60.0;
	}
	if (secs >= 0.0) {
		ss = (unsigned int) secs;
		secs -= (float) ss;
	}
	ddd = (unsigned int) (secs * 1000.0);
	sTime.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, ddd);
	QListViewItem::setText(qtractorAudioListView::Time, sTime);

	QListViewItem::setText(qtractorAudioListView::Path, sPath);
}


// Tooltip renderer.
QString qtractorAudioFileItem::toolTip (void) const
{
	return QObject::tr(
		"%1 (%5)\n"
		"%2 channels, %3 frames, %4 Hz\n"
		"%6")
		.arg(QListViewItem::text(qtractorAudioListView::Name))
		.arg(QListViewItem::text(qtractorAudioListView::Channels))
		.arg(QListViewItem::text(qtractorAudioListView::Frames))
		.arg(QListViewItem::text(qtractorAudioListView::Rate))
		.arg(QListViewItem::text(qtractorAudioListView::Time))
		.arg(QListViewItem::text(qtractorAudioListView::Path));
}


//----------------------------------------------------------------------------
// qtractorAudioListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorAudioListView::qtractorAudioListView (
	QWidget *pParent, const char *pszName )
	: qtractorFileListView(pParent, pszName)
{
	QListView::addColumn(tr("Name"));		// qtractorAudioListView::Name
	QListView::addColumn(tr("Ch"));			// qtractorAudioListView::Channels
	QListView::addColumn(tr("Frames"));		// qtractorAudioListView::Frames
	QListView::addColumn(tr("Rate"));		// qtractorAudioListView::Rate
	QListView::addColumn(tr("Time"));		// qtractorAudioListView::Time
	QListView::addColumn(tr("Path"));		// qtractorAudioListView::Path

	QListView::setColumnAlignment(qtractorAudioListView::Channels, Qt::AlignRight);
	QListView::setColumnAlignment(qtractorAudioListView::Frames, Qt::AlignRight);
	QListView::setColumnAlignment(qtractorAudioListView::Rate, Qt::AlignRight);

	QListView::setColumnWidth(qtractorAudioListView::Name, 120);
}


// Prompt for proper file list open.
QStringList qtractorAudioListView::getOpenFileNames (void)
{
	// Ask for the filename to open...
	return QFileDialog::getOpenFileNames(
		qtractorAudioFileFactory::filters(),// Filter files.
		recentDir(),                        // Start here.
		this, 0,                            // Parent and name (none)
		tr("Open Audio Files") // + " - " QTRACTOR_TITLE // Caption.
	);
}


// File item factory method.
qtractorFileListItem *qtractorAudioListView::createFileItem (
	const QString& sPath, qtractorFileGroupItem *pParentItem )
{
	qtractorFileListItem *pFileItem = NULL;

	qtractorAudioFile *pFile =
		qtractorAudioFileFactory::createAudioFile(sPath);
	if (pFile == NULL)
		return NULL;

	if (pFile->open(sPath)) {
		if (pParentItem)
			pFileItem = new qtractorAudioFileItem(pParentItem, sPath, pFile);
		else
			pFileItem = new qtractorAudioFileItem(this, sPath, pFile);
		pFile->close();
	}

	delete pFile;

	return pFileItem;
}


// end of qtractorAudioListView.cpp

