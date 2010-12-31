// qtractorConnections.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorConnections.h"

#include "qtractorSession.h"

#include "qtractorOptions.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"
#include "qtractorConnectForm.h"

#include <QTabWidget>
#include <QComboBox>

#include <QKeyEvent>


//-------------------------------------------------------------------------
// qtractorConnections - Connections dockable window.
//

// Constructor.
qtractorConnections::qtractorConnections (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	QWidget::setObjectName("qtractorConnections");

	// Create main inner widget.
	m_pConnectForm = new qtractorConnectForm(this);
	// Set proper tab widget icons...
	QTabWidget *pTabWidget = m_pConnectForm->connectTabWidget();
	pTabWidget->setTabIcon(0, QIcon(":/images/trackAudio.png"));
	pTabWidget->setTabIcon(1, QIcon(":/images/trackMidi.png"));

	// Prepare the layout stuff.
	QHBoxLayout *pLayout = new QHBoxLayout();
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pConnectForm);
	QWidget::setLayout(pLayout);

	// Some specialties to this kind of dock window...
	QWidget::setMinimumWidth(480);
	QWidget::setMinimumHeight(240);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Connections") + " - " QTRACTOR_TITLE;
	QWidget::setWindowTitle(sCaption);
	QWidget::setWindowIcon(QIcon(":/images/viewConnections.png"));
	QWidget::setToolTip(sCaption);

	// Get previously saved splitter sizes,
	// (with fair default...)
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<int> sizes;
		sizes.append(180);
		sizes.append(60);
		sizes.append(180);
		pOptions->loadSplitterSizes(
			m_pConnectForm->audioConnectSplitter(), sizes);
		pOptions->loadSplitterSizes(
			m_pConnectForm->midiConnectSplitter(), sizes);
	}
}


// Destructor.
qtractorConnections::~qtractorConnections (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		// Get previously saved splitter sizes...
		pOptions->saveSplitterSizes(
			m_pConnectForm->audioConnectSplitter());
		pOptions->saveSplitterSizes(
			m_pConnectForm->midiConnectSplitter());
	}

	// No need to delete child widgets, Qt does it all for us.
	delete m_pConnectForm;
}


// Notify the main application widget that we're emerging.
void qtractorConnections::showEvent ( QShowEvent *pShowEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnections::showEvent()");
#endif

    qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
    if (pMainForm)
        pMainForm->stabilizeForm();

    QWidget::showEvent(pShowEvent);
}

// Notify the main application widget that we're closing.
void qtractorConnections::hideEvent ( QHideEvent *pHideEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnections::hideEvent()");
#endif

    QWidget::hideEvent(pHideEvent);

    qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
    if (pMainForm)
        pMainForm->stabilizeForm();
}


// Just about to notify main-window that we're closing.
void qtractorConnections::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnections::closeEvent()");
#endif

	QWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Connect form accessor.
qtractorConnectForm *qtractorConnections::connectForm (void) const
{
	return m_pConnectForm;
}


// Main bus mode switching.
void qtractorConnections::showBus ( qtractorBus *pBus,
	qtractorBus::BusMode busMode )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QString sSuffix = ".*";

	switch (pBus->busType()) {
	case qtractorTrack::Audio:
	{	// Show exclusive Audio engine connections...
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			m_pConnectForm->audioClear();
			m_pConnectForm->connectTabWidget()->setCurrentIndex(0);
			if (busMode & qtractorBus::Input) {
				m_pConnectForm->audioOClientsComboBox()->setCurrentIndex(0);
				m_pConnectForm->audioIClientsComboBox()->setCurrentIndex(
					m_pConnectForm->audioIClientsComboBox()->findText(
						pSession->audioEngine()->clientName()));
				m_pConnectForm->audioIListView()->setPortName(
					pAudioBus->busName() + sSuffix);
			} else {
				m_pConnectForm->audioIClientsComboBox()->setCurrentIndex(0);
				m_pConnectForm->audioOClientsComboBox()->setCurrentIndex(
					m_pConnectForm->audioOClientsComboBox()->findText(
						pSession->audioEngine()->clientName()));
				m_pConnectForm->audioOListView()->setPortName(
					pAudioBus->busName() + sSuffix);
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
			m_pConnectForm->midiClear();
			m_pConnectForm->connectTabWidget()->setCurrentIndex(1);
			if (busMode & qtractorBus::Input) {
				m_pConnectForm->midiOClientsComboBox()->setCurrentIndex(0);
				m_pConnectForm->midiIClientsComboBox()->setCurrentIndex(
					m_pConnectForm->midiIClientsComboBox()->findText(
						QString::number(pSession->midiEngine()->alsaClient())
						+ ':'+ pSession->midiEngine()->clientName()));
				m_pConnectForm->midiIListView()->setPortName(
					QString::number(pMidiBus->alsaPort())
					+ ':' + pMidiBus->busName() + sSuffix);
			} else {
				m_pConnectForm->midiIClientsComboBox()->setCurrentIndex(0);
				m_pConnectForm->midiOClientsComboBox()->setCurrentIndex(
					m_pConnectForm->midiOClientsComboBox()->findText(
						QString::number(pSession->midiEngine()->alsaClient())
						+ ':' + pSession->midiEngine()->clientName()));
				m_pConnectForm->midiOListView()->setPortName(
					QString::number(pMidiBus->alsaPort())
					+ ':' + pMidiBus->busName() + sSuffix);
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
	activateWindow();
}


// Complete connections refreshment.
void qtractorConnections::refresh (void)
{
	m_pConnectForm->audioRefresh();
	m_pConnectForm->midiRefresh();
}


// Complete connections recycle.
void qtractorConnections::clear (void)
{
	m_pConnectForm->audioClear();
	m_pConnectForm->midiClear();
}


// Conditional connections recycle.
void qtractorConnections::reset (void)
{
	m_pConnectForm->audioReset();
	m_pConnectForm->midiReset();
}


// Keyboard event handler.
void qtractorConnections::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnections::keyPressEvent(%d)", pKeyEvent->key());
#endif
	int iKey = pKeyEvent->key();
	switch (iKey) {
	case Qt::Key_Escape:
		close();
		break;
	default:
		QWidget::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorConnections.cpp
