// qtractorConnectForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
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
#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"


// Kind of constructor.
void qtractorConnectForm::init (void)
{
	m_pSession = NULL;
	m_pOptions = NULL;

	m_pAudioConnect = new qtractorAudioConnect(
		AudioOListView, AudioIListView,AudioConnectorView);
	m_pAudioConnect->setBezierLines(true);

	m_pMidiConnect = new qtractorMidiConnect(
		MidiOListView, MidiIListView, MidiConnectorView);
	m_pMidiConnect->setBezierLines(true);

	// Connect it to some UI feedback slots.
	QObject::connect(AudioOListView, SIGNAL(selectionChanged()),
		this, SLOT(audioStabilize()));
	QObject::connect(AudioIListView, SIGNAL(selectionChanged()),
		this, SLOT(audioStabilize()));
	QObject::connect(MidiOListView, SIGNAL(selectionChanged()),
		this, SLOT(midiStabilize()));
	QObject::connect(MidiIListView, SIGNAL(selectionChanged()),
		this, SLOT(midiStabilize()));

	QObject::connect(m_pAudioConnect, SIGNAL(connectChanged()),
		this, SLOT(audioStabilize()));
	QObject::connect(m_pMidiConnect, SIGNAL(connectChanged()),
		this, SLOT(midiStabilize()));
}


// Kind of destructor.
void qtractorConnectForm::destroy (void)
{
	delete m_pAudioConnect;
	delete m_pMidiConnect;
}


// Session accessors.
void qtractorConnectForm::setSession (
	qtractorSession *pSession )
{
	m_pSession = pSession;

	if (m_pSession) {
		m_pAudioConnect->setJackClient(m_pSession->audioEngine()->jackClient());
		m_pMidiConnect->setAlsaSeq(m_pSession->midiEngine()->alsaSeq());
	} else {
		m_pAudioConnect->setJackClient(NULL);
		m_pMidiConnect->setAlsaSeq(NULL);
	}

	audioRefresh();
	midiRefresh();
}


qtractorSession *qtractorConnectForm::session (void)
{
	return m_pSession;
}


// General options accessors.
void qtractorConnectForm::setOptions (
	qtractorOptions *pOptions )
{
	m_pOptions = pOptions;
}


qtractorOptions *qtractorConnectForm::options (void)
{
	return m_pOptions;
}


// Connect current selected ports.
void qtractorConnectForm::audioConnectSelected (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::audioConnectSelected()\n");
#endif

	if (m_pAudioConnect->connectSelected())
	/*	audioRefresh()	*/;
}


// Disconnect current selected ports.
void qtractorConnectForm::audioDisconnectSelected (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::audioDisconnectSelected()\n");
#endif

	if (m_pAudioConnect->disconnectSelected())
	/*	audioRefresh()	*/;
}


// Disconnect all connected ports.
void qtractorConnectForm::audioDisconnectAll (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::audioDisconnectAll()\n");
#endif

	if (m_pAudioConnect->disconnectAll())
	/*	audioRefresh()	*/;
}


// Refresh complete form by notifying the parent form.
void qtractorConnectForm::audioRefresh (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::audioRefresh()\n");
#endif

	AudioOListView->setClientName(AudioOClientsComboBox->currentItem() > 0 ?
		AudioOClientsComboBox->currentText() : QString::null);
	AudioIListView->setClientName(AudioIClientsComboBox->currentItem() > 0 ?
		AudioIClientsComboBox->currentText() : QString::null);

	m_pAudioConnect->refresh();

	updateClientsComboBox(AudioOClientsComboBox, AudioOListView,
		m_pAudioConnect->pixmap(QTRACTOR_AUDIO_CLIENT_OUT));
	updateClientsComboBox(AudioIClientsComboBox, AudioIListView,
		m_pAudioConnect->pixmap(QTRACTOR_AUDIO_CLIENT_IN));

	audioStabilize();
}


// A helper stabilization slot.
void qtractorConnectForm::audioStabilize (void)
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorConnectForm::audioStabilize()\n");
#endif

	AudioConnectPushButton->setEnabled(
		m_pAudioConnect->canConnectSelected());
	AudioDisconnectPushButton->setEnabled(
		m_pAudioConnect->canDisconnectSelected());
	AudioDisconnectAllPushButton->setEnabled(
		m_pAudioConnect->canDisconnectAll());
}


// Connect current selected ports.
void qtractorConnectForm::midiConnectSelected (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::midiConnectSelected()\n");
#endif

	if (m_pMidiConnect->connectSelected())
	/*	midiRefresh()	*/;
}


// Disconnect current selected ports.
void qtractorConnectForm::midiDisconnectSelected (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::midiDisconnectSelected()\n");
#endif

	if (m_pMidiConnect->disconnectSelected())
	/*	midiRefresh()	*/;
}


// Disconnect all connected ports.
void qtractorConnectForm::midiDisconnectAll (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::midiDisconnectAll()\n");
#endif

	if (m_pMidiConnect->disconnectAll())
	/*	midiRefresh()	*/;
}


// Refresh complete form by notifying the parent form.
void qtractorConnectForm::midiRefresh (void)
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorConnectForm::midiRefresh()\n");
#endif

	MidiOListView->setClientName(MidiOClientsComboBox->currentItem() > 0 ?
		MidiOClientsComboBox->currentText() : QString::null);
	MidiIListView->setClientName(MidiIClientsComboBox->currentItem() > 0 ?
		MidiIClientsComboBox->currentText() : QString::null);

	m_pMidiConnect->refresh();
	
	updateClientsComboBox(MidiOClientsComboBox, MidiOListView,
		m_pMidiConnect->pixmap(QTRACTOR_MIDI_CLIENT_OUT));
	updateClientsComboBox(MidiIClientsComboBox, MidiIListView,
		m_pMidiConnect->pixmap(QTRACTOR_MIDI_CLIENT_IN));

	midiStabilize();
}


// A helper stabilization slot.
void qtractorConnectForm::midiStabilize (void)
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorConnectForm::midiStabilize()\n");
#endif

	MidiConnectPushButton->setEnabled(
		m_pMidiConnect->canConnectSelected());
	MidiDisconnectPushButton->setEnabled(
		m_pMidiConnect->canDisconnectSelected());
	MidiDisconnectAllPushButton->setEnabled(
		m_pMidiConnect->canDisconnectAll());
}


// Refresh given combo-box with client names.
void qtractorConnectForm::updateClientsComboBox ( QComboBox *pComboBox,
	qtractorClientListView *pClientListView, const QPixmap& pixmap )
{
	// Refresh client names combo box contents...
	pComboBox->clear();
	pComboBox->insertItem(tr("(All)"));
	const QStringList& clientNames = pClientListView->clientNames();
	QStringList::ConstIterator iter = clientNames.begin();
	while (iter != clientNames.end())
		pComboBox->insertItem(pixmap, *iter++);

	// Update current item/selection...
	const QString& sClientName = pClientListView->clientName();
	if (sClientName.isEmpty()) {
		pComboBox->setCurrentItem(0);
	} else {
		// Select and expand all else...
		pComboBox->setCurrentText(sClientName);
		// Select and expand all else...
		qtractorClientListItem *pClientItem
			= pClientListView->findClientItem(sClientName);
		if (pClientItem)
			pClientListView->setSelected(pClientItem, true);
		pClientListView->setOpenAll(true);
	}
}


// end of qtractorConnectForm.ui.h
