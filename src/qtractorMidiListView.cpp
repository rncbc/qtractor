// qtractorMidiListView.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiListView.h"
#include "qtractorMidiFile.h"

#include "qtractorOptions.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QUrl>


//----------------------------------------------------------------------
// class qtractorMidiFileItem -- audio file list view item.
//

// Constructors.
qtractorMidiFileItem::qtractorMidiFileItem (
	const QString& sPath, qtractorMidiFile *pFile )
	: qtractorFileListItem(sPath)
{
	QTreeWidgetItem::setTextAlignment(
		qtractorMidiListView::Format, Qt::AlignRight);
	QTreeWidgetItem::setTextAlignment(
		qtractorMidiListView::Tracks, Qt::AlignRight);
	QTreeWidgetItem::setTextAlignment(
		qtractorMidiListView::Resolution, Qt::AlignRight);

	QTreeWidgetItem::setIcon(qtractorMidiListView::Name,
		QIcon(":/images/itemMidiFile.png"));

	QTreeWidgetItem::setText(qtractorMidiListView::Format,
		QString::number(pFile->format()));
	QTreeWidgetItem::setText(qtractorMidiListView::Tracks,
		QString::number(pFile->tracks()));
	QTreeWidgetItem::setText(qtractorMidiListView::Resolution,
		QString::number(pFile->ticksPerBeat()));

	QTreeWidgetItem::setText(qtractorMidiListView::Path, sPath);

	if (pFile->format() == 1) {
		// Add track sub-items...
		for (int iTrack = 0; iTrack < pFile->tracks(); ++iTrack) {
			new qtractorMidiChannelItem(this,
				QObject::tr("Track %1").arg(iTrack), iTrack);
		}
	} else {
		// Add channel sub-items...
		for (int iChannel = 0; iChannel < 16; ++iChannel) {
			new qtractorMidiChannelItem(this,
				QObject::tr("Channel %1").arg(iChannel + 1), iChannel);
		}
	}
}


// Tooltip renderer.
QString qtractorMidiFileItem::toolTip (void) const
{
	return QObject::tr("%1 (format %2)\n%3 tracks, %4 tpqn\n%5")
		.arg(QTreeWidgetItem::text(qtractorMidiListView::Name))
		.arg(QTreeWidgetItem::text(qtractorMidiListView::Format))
		.arg(QTreeWidgetItem::text(qtractorMidiListView::Tracks))
		.arg(QTreeWidgetItem::text(qtractorMidiListView::Resolution))
		.arg(QTreeWidgetItem::text(qtractorMidiListView::Path));
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
	QTreeWidgetItem::setIcon(qtractorMidiListView::Name,
		QIcon(":/images/itemChannel.png"));
}


// Tooltip renderer.
QString qtractorMidiChannelItem::toolTip (void) const
{
	qtractorMidiFileItem *pFileItem
	    = static_cast<qtractorMidiFileItem *> (QTreeWidgetItem::parent());
	if (pFileItem == NULL)
	    return qtractorFileChannelItem::toolTip();

	return QObject::tr("%1 (format %2)\n%3")
		.arg(pFileItem->text(qtractorMidiListView::Name))
		.arg(pFileItem->text(qtractorMidiListView::Format))
		.arg(QTreeWidgetItem::text(qtractorMidiListView::Name));
}


//----------------------------------------------------------------------------
// qtractorMidiListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorMidiListView::qtractorMidiListView ( QWidget *pParent )
	: qtractorFileListView(qtractorFileList::Midi, pParent)
{
	QTreeWidget::setColumnCount(qtractorMidiListView::LastColumn + 1);

	QTreeWidgetItem *pHeaderItem = QTreeWidget::headerItem();
	pHeaderItem->setText(qtractorMidiListView::Name, tr("Name"));	
	pHeaderItem->setText(qtractorMidiListView::Format, tr("Fmt"));	
	pHeaderItem->setText(qtractorMidiListView::Tracks, tr("Tracks"));	
	pHeaderItem->setText(qtractorMidiListView::Resolution, tr("tpqn"));
	pHeaderItem->setText(qtractorMidiListView::Path, tr("Path"));	
	pHeaderItem->setText(qtractorMidiListView::LastColumn, QString::null);

	pHeaderItem->setTextAlignment(
		qtractorMidiListView::Format, Qt::AlignRight);
	pHeaderItem->setTextAlignment(
		qtractorMidiListView::Tracks, Qt::AlignRight);
	pHeaderItem->setTextAlignment(
		qtractorMidiListView::Resolution, Qt::AlignRight);

	QHeaderView *pHeader = QTreeWidget::header();
	pHeader->resizeSection(qtractorMidiListView::Name, 160);
	QTreeWidget::resizeColumnToContents(qtractorMidiListView::Format);
	QTreeWidget::resizeColumnToContents(qtractorMidiListView::Tracks);
	QTreeWidget::resizeColumnToContents(qtractorMidiListView::Resolution);
	pHeader->resizeSection(qtractorMidiListView::Path, 160);
}


// File item factory method.
qtractorFileListItem *qtractorMidiListView::createFileItem (
	const QString& sPath )
{
	qtractorFileListItem *pFileItem = NULL;

	qtractorMidiFile file;
	if (file.open(sPath)) {
		pFileItem = new qtractorMidiFileItem(sPath, &file);
		file.close();
	}

	return pFileItem;
}


// Prompt for proper file list open.
QStringList qtractorMidiListView::getOpenFileNames (void)
{
	QStringList files;

	const QString  sExt("mid");
	const QString& sTitle  = tr("Open MIDI Files") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt);
#if 1//QT_VERSION < 0x040400
	// Ask for the filenames to open...
	QFileDialog::Options options = 0;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	files = QFileDialog::getOpenFileNames(this,
		sTitle, recentDir(), sFilter, NULL, options);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, recentDir(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sMidiDir));
		fileDialog.setSidebarUrls(urls);
		if (pOptions->bDontUseNativeDialogs)
			fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	}
	// Show dialog...
	if (fileDialog.exec())
		files = fileDialog.selectedFiles();
#endif

	return files;
}


// end of qtractorMidiListView.cpp

