// qtractorFiles.cpp
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

#include "qtractorAbout.h"
#include "qtractorFiles.h"

#include <qtabwidget.h>


//-------------------------------------------------------------------------
// qtractorFiles - File/Groups dockable window.
//

// Constructor.
qtractorFiles::qtractorFiles ( QWidget *pParent, const char *pszName )
	: QDockWindow(pParent, pszName)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	if (pszName == 0)
		QDockWindow::setName("qtractorFiles");

	// Create file type selection tab widget.
	m_pTabWidget = new QTabWidget(this);
	m_pTabWidget->setTabPosition(QTabWidget::Bottom);

	// Create local tabs.
	m_pAudioListView = new qtractorAudioListView(this);
	m_pMidiListView  = new qtractorMidiListView(this);
	// Add respective...
	m_pTabWidget->insertTab(m_pAudioListView, tr("Audio"));
	m_pTabWidget->setTabIconSet(m_pAudioListView,
		QIconSet(QPixmap::fromMimeSource("trackAudio.png")));
	m_pTabWidget->insertTab(m_pMidiListView, tr("MIDI"));
	m_pTabWidget->setTabIconSet(m_pMidiListView,
		QIconSet(QPixmap::fromMimeSource("trackMidi.png")));

	// Prepare the dockable window stuff.
	QDockWindow::setWidget(m_pTabWidget);
	QDockWindow::setOrientation(Qt::Vertical);
	QDockWindow::setCloseMode(QDockWindow::Always);
	QDockWindow::setResizeEnabled(true);
	// Some specialties to this kind of dock window...
	QDockWindow::setFixedExtentWidth(160);

	// Finally set the default caption and tooltip.
	QString sCaption = tr("Files");
	QToolTip::add(this, sCaption);
	QDockWindow::setCaption(sCaption);
	QDockWindow::setIcon(QPixmap::fromMimeSource("viewFiles.png"));
}


// Destructor.
qtractorFiles::~qtractorFiles (void)
{
	// No need to delete child widgets, Qt does it all for us.
}


// Audio file list view accessor.
qtractorAudioListView *qtractorFiles::audioListView (void) const
{
	return m_pAudioListView;
}


// MIDI file list view accessor.
qtractorMidiListView *qtractorFiles::midiListView (void) const
{
	return m_pMidiListView;
}


// Clear evrything on sight.
void qtractorFiles::clear (void)
{
	m_pAudioListView->clear();
	m_pMidiListView->clear();
}


// Audio file addition convenience method.
void qtractorFiles::addAudioFile  ( const QString& sFilename )
{
	m_pTabWidget->setCurrentPage(qtractorFiles::Audio);
	m_pAudioListView->addFileItem(sFilename);
}


// MIDI file addition convenience method.
void qtractorFiles::addMidiFile  ( const QString& sFilename )
{
	m_pTabWidget->setCurrentPage(qtractorFiles::Midi);
	m_pMidiListView->addFileItem(sFilename);
}


// Audio file selection convenience method.
void qtractorFiles::selectAudioFile  ( const QString& sFilename )
{
	m_pTabWidget->setCurrentPage(qtractorFiles::Audio);
	m_pAudioListView->selectFileItem(sFilename);
}


// MIDI file selection convenience method.
void qtractorFiles::selectMidiFile  ( const QString& sFilename,
	int iTrackChannel )
{
	m_pTabWidget->setCurrentPage(qtractorFiles::Midi);
	m_pMidiListView->selectFileItem(sFilename, iTrackChannel);
}


// end of qtractorFiles.cpp
