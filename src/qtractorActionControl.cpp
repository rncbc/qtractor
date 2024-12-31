// qtractorActionControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2022, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorActionControl.h"

#include <QAction>
#include <QMenu>


//----------------------------------------------------------------------
// class qtractorActionControl::MidiObserver -- impl.
//

// MIDI observer ctor.
qtractorActionControl::MidiObserver::MidiObserver ( QAction *pAction )
	: qtractorMidiControlObserver(nullptr), m_pAction(pAction)
{
	m_subject.setName(menuActionText(pAction, pAction->text()).remove('&'));
	m_subject.setInteger(true);
	m_subject.setToggled(pAction->isCheckable());

	qtractorMidiControlObserver::setSubject(&m_subject);
	qtractorMidiControlObserver::setHook(true);
	qtractorMidiControlObserver::setLatch(false);
	qtractorMidiControlObserver::setTriggered(true);
}


// MIDI observer updater.
void qtractorActionControl::MidiObserver::update ( bool bUpdate )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorActionControl::MidiObserver[%p]::update(%d)", this, int(bUpdate));
#endif
	qtractorActionControl *pActionControl
		= qtractorActionControl::getInstance();
	if (pActionControl && m_pAction->isEnabled()) {
		const bool bBlockSignals
			= pActionControl->blockSignals(true);
		m_pAction->activate(QAction::Trigger);
		pActionControl->blockSignals(bBlockSignals);
	}

	qtractorMidiControlObserver::update(bUpdate);
}


//----------------------------------------------------------------------
// class qtractorActionControl -- (QAction) MIDI observers map.
//

// Kind of singleton reference.
qtractorActionControl *qtractorActionControl::g_pActionControl = nullptr;


// ctor.
qtractorActionControl::qtractorActionControl ( QObject *pParent )
	: QObject(pParent)
{
	// Pseudo-singleton reference setup.
	g_pActionControl = this;
}


// dtor.
qtractorActionControl::~qtractorActionControl (void)
{
	// Pseudo-singleton reference shut-down.
	g_pActionControl = nullptr;

	clear();
}


// Kind of singleton reference.
qtractorActionControl *qtractorActionControl::getInstance (void)
{
	return g_pActionControl;
}


// MIDI observer map cleaner.
void qtractorActionControl::clear (void)
{
	qDeleteAll(m_midiObservers);
	m_midiObservers.clear();
};


// MIDI observer map methods.
qtractorActionControl::MidiObserver *qtractorActionControl::getMidiObserver (
	QAction *pAction )
{
	return m_midiObservers.value(pAction, nullptr);
}


qtractorActionControl::MidiObserver *qtractorActionControl::addMidiObserver (
	QAction *pAction )
{
	MidiObserver *pMidiObserver = getMidiObserver(pAction);
	if (pMidiObserver == nullptr) {
		pMidiObserver = new MidiObserver(pAction);
		m_midiObservers.insert(pAction, pMidiObserver);
	}

	pMidiObserver->setValue(pAction->isChecked()
		? pMidiObserver->maxValue()
		: pMidiObserver->minValue());

	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		this, SLOT(triggeredSlot(bool)));

	return pMidiObserver;
}


void qtractorActionControl::removeMidiObserver ( QAction *pAction )
{
	MidiObserver *pMidiObserver = getMidiObserver(pAction);
	if (pMidiObserver) {
		QObject::disconnect(
			pAction, SIGNAL(triggered(bool)),
			this, SLOT(triggeredSlot(bool)));
		m_midiObservers.remove(pAction);
		delete pMidiObserver;
	}
}


// MIDI observer trigger slot.
void qtractorActionControl::triggeredSlot ( bool bOn )
{
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		MidiObserver *pMidiObserver = getMidiObserver(pAction);
		if (pMidiObserver) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractorActionControl::triggeredSlot(%d)", int(bOn));
		#endif
			const float v0 = pMidiObserver->value();
			const float vmax = pMidiObserver->maxValue();
			const float vmin = pMidiObserver->minValue();
			const float vmid = 0.5f * (vmax + vmin);
			if (bOn && !pAction->isChecked())
				pMidiObserver->setValue(v0 > vmid ? vmin : vmax);
			else
				pMidiObserver->setValue(v0 > vmid ? vmax : vmin);
		}
	}
}


// Complete action text, from associated menus. [static]
QString qtractorActionControl::menuActionText (
	QAction *pAction, const QString& sText )
{
	QString sActionText = sText;
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 2)
	QListIterator<QObject *> iter(pAction->associatedObjects());
#else
	QListIterator<QWidget *> iter(pAction->associatedWidgets());
#endif
	while (iter.hasNext()) {
		QMenu *pMenu = qobject_cast<QMenu *> (iter.next());
		if (pMenu) {
			sActionText = pMenu->title() + '/' + sActionText;
			pAction = pMenu->menuAction();
			if (pAction)
				sActionText = menuActionText(pAction, sActionText);
		}
	}

	return sActionText;
}


// end of qtractorActionControl.cpp
