// qtractorConnectForm.cpp
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

#include "qtractorConnectForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include <QPixmap>


//----------------------------------------------------------------------------
// qtractorConnectForm -- UI wrapper form.

// Constructor.
qtractorConnectForm::qtractorConnectForm (
	QWidget *pParent, Qt::WindowFlags wflags ) : QWidget(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pAudioConnect = new qtractorAudioConnect(
		m_ui.AudioOListView, m_ui.AudioIListView, m_ui.AudioConnectorView);
	m_pAudioConnect->setBezierLines(true);

	m_pMidiConnect = new qtractorMidiConnect(
		m_ui.MidiOListView, m_ui.MidiIListView, m_ui.MidiConnectorView);
	m_pMidiConnect->setBezierLines(true);

	// UI signal/slot connections...
	QObject::connect(m_ui.AudioIClientsComboBox,
		SIGNAL(activated(int)),
		SLOT(audioIClientChanged()));
	QObject::connect(m_ui.AudioOClientsComboBox,
		SIGNAL(activated(int)),
		SLOT(audioOClientChanged()));
	QObject::connect(m_ui.AudioConnectPushButton,
		SIGNAL(clicked()),
		SLOT(audioConnectSelected()));
	QObject::connect(m_ui.AudioDisconnectPushButton,
		SIGNAL(clicked()),
		SLOT(audioDisconnectSelected()));
	QObject::connect(m_ui.AudioDisconnectAllPushButton,
		SIGNAL(clicked()),
		SLOT(audioDisconnectAll()));
	QObject::connect(m_ui.AudioRefreshPushButton,
		SIGNAL(clicked()),
		SLOT(audioRefresh()));
	QObject::connect(m_ui.MidiIClientsComboBox,
		SIGNAL(activated(int)),
		SLOT(midiIClientChanged()));
	QObject::connect(m_ui.MidiOClientsComboBox,
		SIGNAL(activated(int)),
		SLOT(midiOClientChanged()));
	QObject::connect(m_ui.MidiConnectPushButton,
		SIGNAL(clicked()),
		SLOT(midiConnectSelected()));
	QObject::connect(m_ui.MidiDisconnectPushButton,
		SIGNAL(clicked()),
		SLOT(midiDisconnectSelected()));
	QObject::connect(m_ui.MidiDisconnectAllPushButton,
		SIGNAL(clicked()),
		SLOT(midiDisconnectAll()));
	QObject::connect(m_ui.MidiRefreshPushButton,
		SIGNAL(clicked()),
		SLOT(midiRefresh()));

	// Connect it to some UI feedback slots.
	QObject::connect(m_ui.AudioOListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(audioStabilize()));
	QObject::connect(m_ui.AudioIListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(audioStabilize()));
	QObject::connect(m_ui.MidiOListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(midiStabilize()));
	QObject::connect(m_ui.MidiIListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(midiStabilize()));

	QObject::connect(m_pAudioConnect,
		SIGNAL(connectChanged()),
		SLOT(audioConnectChanged()));
	QObject::connect(m_pMidiConnect,
		SIGNAL(connectChanged()),
		SLOT(midiConnectChanged()));
}


// Destructor.
qtractorConnectForm::~qtractorConnectForm (void)
{
	delete m_pAudioConnect;
	delete m_pMidiConnect;
}


// Audio client name change slots.
void qtractorConnectForm::audioIClientChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioIClientChanged()");
#endif

	// Reset any port name pattern...
	m_ui.AudioIListView->setPortName(QString::null);
	audioRefresh();
}

void qtractorConnectForm::audioOClientChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioOClientChanged()");
#endif

	// Reset any port name pattern...
	m_ui.AudioOListView->setPortName(QString::null);
	audioRefresh();
}


// Connect current selected ports.
void qtractorConnectForm::audioConnectSelected (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioConnectSelected()");
#endif

	m_pAudioConnect->connectSelected();
}


// Disconnect current selected ports.
void qtractorConnectForm::audioDisconnectSelected (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioDisconnectSelected()");
#endif

	m_pAudioConnect->disconnectSelected();
}


// Disconnect all connected ports.
void qtractorConnectForm::audioDisconnectAll (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioDisconnectAll()");
#endif

	m_pAudioConnect->disconnectAll();
}


// Refresh complete form by notifying the parent form.
void qtractorConnectForm::audioUpdate ( bool bClear )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioUpdate(%d)", int(bClear));
#endif

	m_ui.AudioOListView->setClientName(
		m_ui.AudioOClientsComboBox->currentIndex() > 0 && !bClear
			? m_ui.AudioOClientsComboBox->currentText() : QString::null);
	m_ui.AudioIListView->setClientName(
		m_ui.AudioIClientsComboBox->currentIndex() > 0 && !bClear
			? m_ui.AudioIClientsComboBox->currentText() : QString::null);

	m_pAudioConnect->updateContents(bClear);

	updateClientsComboBox(m_ui.AudioOClientsComboBox, m_ui.AudioOListView,
		m_pAudioConnect->icon(qtractorAudioConnect::ClientOut));
	updateClientsComboBox(m_ui.AudioIClientsComboBox, m_ui.AudioIListView,
		m_pAudioConnect->icon(qtractorAudioConnect::ClientIn));

	audioStabilize();
}


// Refresh complete form conditionally.
void qtractorConnectForm::audioReset (void)
{
	audioUpdate(
		m_ui.AudioOClientsComboBox->currentIndex() > 0 ||
		m_ui.AudioIClientsComboBox->currentIndex() > 0);
}


// A helper connection change slot.
void qtractorConnectForm::audioConnectChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioConectChanged()");
#endif

	audioStabilize();

	emit connectChanged();
}


// A helper stabilization slot.
void qtractorConnectForm::audioStabilize (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::audioStabilize()");
#endif

	m_ui.AudioConnectPushButton->setEnabled(
		m_pAudioConnect->canConnectSelected());
	m_ui.AudioDisconnectPushButton->setEnabled(
		m_pAudioConnect->canDisconnectSelected());
	m_ui.AudioDisconnectAllPushButton->setEnabled(
		m_pAudioConnect->canDisconnectAll());
}


// MIDI client name change slots.
void qtractorConnectForm::midiIClientChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiIClientChanged()");
#endif

	// Reset any port name pattern...
	m_ui.MidiIListView->setPortName(QString::null);
	midiRefresh();
}

void qtractorConnectForm::midiOClientChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiOClientChanged()");
#endif

	// Reset any port name pattern...
	m_ui.MidiOListView->setPortName(QString::null);
	midiRefresh();
}


// Connect current selected ports.
void qtractorConnectForm::midiConnectSelected (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiConnectSelected()");
#endif

	m_pMidiConnect->connectSelected();
}


// Disconnect current selected ports.
void qtractorConnectForm::midiDisconnectSelected (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiDisconnectSelected()");
#endif

	m_pMidiConnect->disconnectSelected();
}


// Disconnect all connected ports.
void qtractorConnectForm::midiDisconnectAll (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiDisconnectAll()");
#endif

	m_pMidiConnect->disconnectAll();
}


// Refresh complete form by notifying the parent form.
void qtractorConnectForm::midiUpdate ( bool bClear )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiUpdate(%d)", int(bClear));
#endif

	m_ui.MidiOListView->setClientName(
		m_ui.MidiOClientsComboBox->currentIndex() > 0 && !bClear
			? m_ui.MidiOClientsComboBox->currentText() : QString::null);
	m_ui.MidiIListView->setClientName(
		m_ui.MidiIClientsComboBox->currentIndex() > 0 && !bClear
			? m_ui.MidiIClientsComboBox->currentText() : QString::null);

	m_pMidiConnect->updateContents(bClear);
	
	updateClientsComboBox(m_ui.MidiOClientsComboBox, m_ui.MidiOListView,
		m_pMidiConnect->icon(qtractorMidiConnect::ClientOut));
	updateClientsComboBox(m_ui.MidiIClientsComboBox, m_ui.MidiIListView,
		m_pMidiConnect->icon(qtractorMidiConnect::ClientIn));

	midiStabilize();
}


// Refresh complete form conditionally.
void qtractorConnectForm::midiReset (void)
{
	midiUpdate(
		m_ui.MidiOClientsComboBox->currentIndex() > 0 ||
		m_ui.MidiIClientsComboBox->currentIndex() > 0);
}


// A helper connection change slot.
void qtractorConnectForm::midiConnectChanged (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiConectChanged()");
#endif

	midiStabilize();

	emit connectChanged();
}


// A helper stabilization slot.
void qtractorConnectForm::midiStabilize (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorConnectForm::midiStabilize()");
#endif

	m_ui.MidiConnectPushButton->setEnabled(
		m_pMidiConnect->canConnectSelected());
	m_ui.MidiDisconnectPushButton->setEnabled(
		m_pMidiConnect->canDisconnectSelected());
	m_ui.MidiDisconnectAllPushButton->setEnabled(
		m_pMidiConnect->canDisconnectAll());
}


// Refresh given combo-box with client names.
void qtractorConnectForm::updateClientsComboBox ( QComboBox *pComboBox,
	qtractorClientListView *pClientListView, const QIcon& icon )
{
	// Refresh client names combo box contents...
	pComboBox->clear();
	pComboBox->addItem(tr("(All)"));
	const QStringList& clientNames = pClientListView->clientNames();
	QStringListIterator iter(clientNames);
	while (iter.hasNext())
		pComboBox->addItem(icon, iter.next());

	// Update current item/selection...
	const QString& sClientName = pClientListView->clientName();
	if (sClientName.isEmpty()) {
		pComboBox->setCurrentIndex(0);
	} else {
		// Select and expand all else...
		pComboBox->setCurrentIndex(pComboBox->findText(sClientName));
		// Select and expand all else...
		qtractorClientListItem *pClientItem
			= pClientListView->findClientItem(sClientName);
		if (pClientItem)
			pClientListView->setCurrentItem(pClientItem);
		pClientListView->setOpenAll(true);
	}
}


// end of qtractorConnectForm.cpp
