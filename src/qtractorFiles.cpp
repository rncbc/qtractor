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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorFiles.h"

#include "qtractorMainForm.h"

#include <QTabWidget>


//-------------------------------------------------------------------------
// qtractorFiles - File/Groups dockable window.
//

// Constructor.
qtractorFiles::qtractorFiles ( QWidget *pParent )
	: QDockWidget(pParent)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	QDockWidget::setObjectName("qtractorFiles");

	// Create file type selection tab widget.
	m_pTabWidget = new QTabWidget(this);
	m_pTabWidget->setTabPosition(QTabWidget::South);

	// Create local tabs.
	m_pAudioListView = new qtractorAudioListView();
	m_pMidiListView  = new qtractorMidiListView();
	// Add respective...
	m_pTabWidget->addTab(m_pAudioListView, tr("Audio"));
	m_pTabWidget->addTab(m_pMidiListView, tr("MIDI"));
	// Icons...
	m_pTabWidget->setTabIcon(qtractorFiles::Audio, QIcon(":/icons/trackAudio.png"));
	m_pTabWidget->setTabIcon(qtractorFiles::Midi,  QIcon(":/icons/trackMidi.png"));

	// Prepare the dockable window stuff.
	QDockWidget::setWidget(m_pTabWidget);
	QDockWidget::setFeatures(QDockWidget::AllDockWidgetFeatures);
	QDockWidget::setAllowedAreas(
		Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	// Some specialties to this kind of dock window...
	QDockWidget::setMinimumWidth(120);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Files");
	QDockWidget::setWindowTitle(sCaption);
	QDockWidget::setWindowIcon(QIcon(":/icons/viewFiles.png"));
	QDockWidget::setToolTip(sCaption);
}


// Destructor.
qtractorFiles::~qtractorFiles (void)
{
	// No need to delete child widgets, Qt does it all for us.
}


// Just about to notify main-window that we're closing.
void qtractorFiles::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorFiles::closeEvent()\n");
#endif

	QDockWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
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
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
	m_pAudioListView->addFileItem(sFilename);
}


// MIDI file addition convenience method.
void qtractorFiles::addMidiFile  ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
	m_pMidiListView->addFileItem(sFilename);
}


// Audio file selection convenience method.
void qtractorFiles::selectAudioFile  ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
	m_pAudioListView->selectFileItem(sFilename);
}


// MIDI file selection convenience method.
void qtractorFiles::selectMidiFile  ( const QString& sFilename,
	int iTrackChannel )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
	m_pMidiListView->selectFileItem(sFilename, iTrackChannel);
}


// end of qtractorFiles.cpp
