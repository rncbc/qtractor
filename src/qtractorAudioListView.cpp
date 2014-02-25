// qtractorAudioListView.cpp
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
#include "qtractorAudioListView.h"
#include "qtractorAudioFile.h"

#include "qtractorOptions.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QUrl>


//----------------------------------------------------------------------
// class qtractorAudioFileItem -- audio file list view item.
//

// Constructors.
qtractorAudioFileItem::qtractorAudioFileItem (
	const QString& sPath, qtractorAudioFile *pFile )
	: qtractorFileListItem(sPath)
{
	QTreeWidgetItem::setTextAlignment(
		qtractorAudioListView::Channels, Qt::AlignRight);
	QTreeWidgetItem::setTextAlignment(
		qtractorAudioListView::Frames, Qt::AlignRight);
	QTreeWidgetItem::setTextAlignment(
		qtractorAudioListView::Rate, Qt::AlignRight);

	QTreeWidgetItem::setIcon(qtractorAudioListView::Name,
		QIcon(":/images/itemAudioFile.png"));

	QTreeWidgetItem::setText(qtractorAudioListView::Channels,
		QString::number(pFile->channels()));
	QTreeWidgetItem::setText(qtractorAudioListView::Frames,
		QString::number(pFile->frames()));
	QTreeWidgetItem::setText(qtractorAudioListView::Rate,
		QString::number(pFile->sampleRate()));

	QString sTime;
	unsigned int hh, mm, ss, zzz;
	float secs = (float) pFile->frames() / (float) pFile->sampleRate();
	hh = mm = ss = 0;
	if (secs >= 3600.0f) {
		hh = (unsigned int) (secs / 3600.0f);
		secs -= (float) hh * 3600.0f;
	}
	if (secs >= 60.0f) {
		mm = (unsigned int) (secs / 60.0f);
		secs -= (float) mm * 60.0f;
	}
	if (secs >= 0.0f) {
		ss = (unsigned int) secs;
		secs -= (float) ss;
	}
	zzz = (unsigned int) (secs * 1000.0f);
	sTime.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
	QTreeWidgetItem::setText(qtractorAudioListView::Time, sTime);

	QTreeWidgetItem::setText(qtractorAudioListView::Path, sPath);
}


// Tooltip renderer.
QString qtractorAudioFileItem::toolTip (void) const
{
	return QObject::tr(
		"%1 (%2)\n%3 channels, %4 frames, %5 Hz\n%6")
		.arg(QTreeWidgetItem::text(qtractorAudioListView::Name))
		.arg(QTreeWidgetItem::text(qtractorAudioListView::Time))
		.arg(QTreeWidgetItem::text(qtractorAudioListView::Channels))
		.arg(QTreeWidgetItem::text(qtractorAudioListView::Frames))
		.arg(QTreeWidgetItem::text(qtractorAudioListView::Rate))
		.arg(QTreeWidgetItem::text(qtractorAudioListView::Path));
}


//----------------------------------------------------------------------------
// qtractorAudioListView -- Group/File list view, supporting drag-n-drop.
//

// Constructor.
qtractorAudioListView::qtractorAudioListView ( QWidget *pParent )
	: qtractorFileListView(qtractorFileList::Audio, pParent)
{
	QTreeWidget::setColumnCount(qtractorAudioListView::LastColumn + 1);
	
	QTreeWidgetItem *pHeaderItem = QTreeWidget::headerItem();
	pHeaderItem->setText(qtractorAudioListView::Name, tr("Name"));
	pHeaderItem->setText(qtractorAudioListView::Channels, tr("Ch"));
	pHeaderItem->setText(qtractorAudioListView::Frames,	tr("Frames"));
	pHeaderItem->setText(qtractorAudioListView::Rate, tr("Rate"));
	pHeaderItem->setText(qtractorAudioListView::Time, tr("Time"));
	pHeaderItem->setText(qtractorAudioListView::Path, tr("Path"));
	pHeaderItem->setText(qtractorAudioListView::LastColumn, QString::null);

	pHeaderItem->setTextAlignment(
		qtractorAudioListView::Channels, Qt::AlignRight);
	pHeaderItem->setTextAlignment(
		qtractorAudioListView::Frames, Qt::AlignRight);
	pHeaderItem->setTextAlignment(
		qtractorAudioListView::Rate, Qt::AlignRight);

	QHeaderView *pHeader = QTreeWidget::header();
	pHeader->resizeSection(qtractorAudioListView::Name, 160);
	QTreeWidget::resizeColumnToContents(qtractorAudioListView::Channels);
	QTreeWidget::resizeColumnToContents(qtractorAudioListView::Frames);
	QTreeWidget::resizeColumnToContents(qtractorAudioListView::Rate);
	QTreeWidget::resizeColumnToContents(qtractorAudioListView::Time);
	pHeader->resizeSection(qtractorAudioListView::Path, 160);
}


// Prompt for proper file list open.
QStringList qtractorAudioListView::getOpenFileNames (void)
{
	QStringList files;

	const QString& sTitle = tr("Open Audio Files") + " - " QTRACTOR_TITLE;
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	files = QFileDialog::getOpenFileNames(this,
		sTitle, recentDir(), qtractorAudioFileFactory::filters(), NULL, options);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, recentDir(), qtractorAudioFileFactory::filters());
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	fileDialog.setDefaultSuffix(qtractorAudioFileFactory::defaultExt());
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sAudioDir));
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


// File item factory method.
qtractorFileListItem *qtractorAudioListView::createFileItem (
	const QString& sPath )
{
	qtractorFileListItem *pFileItem = NULL;

	qtractorAudioFile *pFile =
		qtractorAudioFileFactory::createAudioFile(sPath);
	if (pFile == NULL)
		return NULL;

	if (pFile->open(sPath)) {
		pFileItem = new qtractorAudioFileItem(sPath, pFile);
		pFile->close();
	}

	delete pFile;

	return pFileItem;
}


// end of qtractorAudioListView.cpp

