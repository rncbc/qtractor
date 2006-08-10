// qtractorConnections.cpp
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
#include "qtractorConnections.h"

#include "qtractorOptions.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"
#include "qtractorConnectForm.h"

#include <qtabwidget.h>
#include <qcombobox.h>


//-------------------------------------------------------------------------
// qtractorConnections - Connections dockable window.
//

// Constructor.
qtractorConnections::qtractorConnections ( qtractorMainForm *pParent )
	: QDockWindow(pParent, "qtractorConnections")
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	// QDockWindow::setName("qtractorConnections");

	// Create main inner widget.
	m_pConnectForm = new qtractorConnectForm(this);
	// Set proper tab widget icons...
	m_pConnectForm->ConnectTabWidget->setTabIconSet(
		m_pConnectForm->ConnectTabWidget->page(0),
		QIconSet(QPixmap::fromMimeSource("trackAudio.png")));
	m_pConnectForm->ConnectTabWidget->setTabIconSet(
		m_pConnectForm->ConnectTabWidget->page(1),
		QIconSet(QPixmap::fromMimeSource("trackMidi.png")));

	// Prepare the dockable window stuff.
	QDockWindow::setWidget(m_pConnectForm);
	QDockWindow::setOrientation(Qt::Horizontal);
	QDockWindow::setCloseMode(QDockWindow::Always);
	QDockWindow::setResizeEnabled(true);
	// Some specialties to this kind of dock window...
	QDockWindow::setFixedExtentHeight(240);
	QDockWindow::setFixedExtentWidth(480);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Connections");
	QToolTip::add(this, sCaption);
	QDockWindow::setCaption(sCaption);
	QDockWindow::setIcon(QPixmap::fromMimeSource("viewConnections.png"));

	// Get previously saved splitter sizes,
	// (with fair default...)
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			QValueList<int> sizes;
			sizes.append(180);
			sizes.append(60);
			sizes.append(180);
			pOptions->loadSplitterSizes(
				m_pConnectForm->AudioConnectSplitter, sizes);
			pOptions->loadSplitterSizes(
				m_pConnectForm->MidiConnectSplitter, sizes);
		}
	}
}


// Destructor.
qtractorConnections::~qtractorConnections (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			// Get previously saved splitter sizes...
			pOptions->saveSplitterSizes(
				m_pConnectForm->AudioConnectSplitter);
			pOptions->saveSplitterSizes(
				m_pConnectForm->MidiConnectSplitter);
		}
	}

	// No need to delete child widgets, Qt does it all for us.
	delete m_pConnectForm;
}


// Connect form accessor.
qtractorConnectForm *qtractorConnections::connectForm (void) const
{
	return m_pConnectForm;
}


// Session accessor.
qtractorSession *qtractorConnections::session (void) const
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	return (pMainForm ? pMainForm->session() : NULL);
}


// Main bus mode switching.
void qtractorConnections::showBus ( qtractorBus *pBus,
	qtractorBus::BusMode busMode )
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	const QString sSuffix = ".*";

	switch (pBus->busType()) {
	case qtractorTrack::Audio:
	{	// Show exclusive Audio engine connections...
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			m_pConnectForm->ConnectTabWidget->setCurrentPage(0);
			if (busMode & qtractorBus::Input) {
				m_pConnectForm->AudioOClientsComboBox->setCurrentItem(0);
				m_pConnectForm->AudioIClientsComboBox->setCurrentText(
					pSession->audioEngine()->clientName());
				m_pConnectForm->AudioIListView->setPortName(
					pAudioBus->busName() + sSuffix);
			} else {
				m_pConnectForm->AudioOClientsComboBox->setCurrentText(
					pSession->audioEngine()->clientName());
				m_pConnectForm->AudioOListView->setPortName(
					pAudioBus->busName() + sSuffix);
				m_pConnectForm->AudioIClientsComboBox->setCurrentItem(0);
			}
			m_pConnectForm->audioRefresh();
		}
		break;
	}
	case qtractorTrack::Midi:
	{	// Show exclusive MIDI engine connections...
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus) {
			m_pConnectForm->ConnectTabWidget->setCurrentPage(1);
			if (busMode & qtractorBus::Input) {
				m_pConnectForm->MidiOClientsComboBox->setCurrentItem(0);
				m_pConnectForm->MidiIClientsComboBox->setCurrentText(
					QString::number(pSession->midiEngine()->alsaClient())
					+ ':'+ pSession->midiEngine()->clientName());
				m_pConnectForm->MidiIListView->setPortName(
					QString::number(pMidiBus->alsaPort())
					+ ':' + pMidiBus->busName() + sSuffix);
			} else {
				m_pConnectForm->MidiOClientsComboBox->setCurrentText(
					QString::number(pSession->midiEngine()->alsaClient())
					+ ':' + pSession->midiEngine()->clientName());
				m_pConnectForm->MidiOListView->setPortName(
					QString::number(pMidiBus->alsaPort())
					+ ':' + pMidiBus->busName() + sSuffix);
				m_pConnectForm->MidiIClientsComboBox->setCurrentItem(0);
			}
			m_pConnectForm->midiRefresh();
		}
		break;
	}
	default:
		break;
	}
		
	// Make it stand out, sure...
	show();
	raise();
	setActiveWindow();
}


// Complete connections refreshment.
void qtractorConnections::refresh (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		m_pConnectForm->setSession(pMainForm->session());
}


// Complete connections recycle.
void qtractorConnections::clear (void)
{
	m_pConnectForm->setSession(NULL);
}


// end of qtractorConnections.cpp
