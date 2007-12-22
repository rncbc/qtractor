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
#include <QHBoxLayout>
#include <QToolButton>


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
#if QT_VERSION >= 0x040201
	m_pTabWidget->setUsesScrollButtons(false);
#endif

	// Player button (initially disabled)...
	m_pPlayWidget = new QWidget(m_pTabWidget);
	m_pPlayLayout = new QHBoxLayout(/*m_pPlayWidget*/);
	m_pPlayLayout->setMargin(2);
	m_pPlayLayout->setSpacing(2);
	m_pPlayWidget->setLayout(m_pPlayLayout);

	m_iPlayUpdate = 0;
	m_pPlayButton = new QToolButton(m_pPlayWidget);
	m_pPlayButton->setIcon(QIcon(":/icons/transportPlay.png"));
	m_pPlayButton->setToolTip(tr("Play file"));
	m_pPlayButton->setCheckable(true);
	m_pPlayButton->setEnabled(false);
	m_pPlayLayout->addWidget(m_pPlayButton);
	m_pTabWidget->setCornerWidget(m_pPlayWidget, Qt::BottomRightCorner);

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

	// Child widgets signal/slots... 
	QObject::connect(m_pTabWidget,
		SIGNAL(currentChanged(int)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pAudioListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pMidiListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pPlayButton,
		SIGNAL(toggled(bool)),
		SLOT(playSlot(bool)));
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


// Clear everything on sight.
void qtractorFiles::clear (void)
{
	m_pAudioListView->clear();
	m_pMidiListView->clear();
}


// Audio file addition convenience method.
void qtractorFiles::addAudioFile ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
	m_pAudioListView->addFileItem(sFilename);
}


// MIDI file addition convenience method.
void qtractorFiles::addMidiFile ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
	m_pMidiListView->addFileItem(sFilename);
}


// Audio file selection convenience method.
void qtractorFiles::selectAudioFile ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
	m_pAudioListView->selectFileItem(sFilename);
}


// MIDI file selection convenience method.
void qtractorFiles::selectMidiFile ( const QString& sFilename,
	int iTrackChannel )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
	m_pMidiListView->selectFileItem(sFilename, iTrackChannel);
}


// Audition/pre-listening player methods.
void qtractorFiles::setPlayButton ( bool bOn )
{
	m_iPlayUpdate++;
	m_pPlayButton->setChecked(bOn);
	m_iPlayUpdate--;
}

bool qtractorFiles::isPlayButton (void) const
{
	return m_pPlayButton->isChecked();
}


// Audition/pre-listening player slot.
void qtractorFiles::playSlot ( bool bOn )
{
	if (m_iPlayUpdate > 0)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	QString sFilename;
	if (bOn && m_pTabWidget->currentIndex() == qtractorFiles::Audio) {
		qtractorFileListItem *pFileItem = m_pAudioListView->currentFileItem();
		if (pFileItem)
			sFilename = pFileItem->path();
	}

	pMainForm->activateAudioFile(sFilename);
}


// Current item change slots.
void qtractorFiles::stabilizeSlot (void)
{
	switch (m_pTabWidget->currentIndex()) {
	case qtractorFiles::Audio:
		m_pPlayButton->setEnabled(m_pAudioListView->currentFileItem() != NULL);
		break;
	case qtractorFiles::Midi:
	default:
		m_pPlayButton->setEnabled(false);
		break;
	}
}


// end of qtractorFiles.cpp
