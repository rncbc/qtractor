// qtractorActionControl.h
//
/****************************************************************************
   Copyright (C) 2005-2015, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorActionControl_h
#define __qtractorActionControl_h

#include "qtractorMidiControlObserver.h"

#include <QAction>


//----------------------------------------------------------------------
// class qtractorActionControl -- (QAction) MIDI observers map.
//

class qtractorActionControl : public QObject
{
	Q_OBJECT

public:

	// ctor.
	qtractorActionControl(QObject *pParent = NULL);

	// dtor.
	~qtractorActionControl();

	// MIDI observers interface.
	class MidiObserver : public qtractorMidiControlObserver
	{
	public:

		MidiObserver(QAction *pAction)
			: qtractorMidiControlObserver(NULL), m_pAction(pAction)
		{
			m_subject.setName(pAction->text());
			m_subject.setToggled(pAction->isChecked());

			qtractorMidiControlObserver::setSubject(&m_subject);
			qtractorMidiControlObserver::setHook(true);
		}

	protected:

		void update(bool bUpdate)
		{
			m_pAction->activate(QAction::Trigger);

			qtractorMidiControlObserver::update(bUpdate);
		}

	private:

		QAction *m_pAction;
		qtractorSubject m_subject;
	};

	// MIDI observers map methods.
	MidiObserver *getMidiObserver(QAction *pAction);
	MidiObserver *addMidiObserver(QAction *pAction);
	void removeMidiObserver(QAction *pAction);

	// Pseudo-singleton instance accessor.
	static qtractorActionControl *getInstance();

protected slots:

	// MIDI observer trigger slot.
	void triggeredSlot(bool bOn);

protected:

	// MIDI observers map cleaner.
	void clear();

private:

	// MIDI observers map.
	QHash<QAction *, MidiObserver *> m_midiObservers;

	// Pseudo-singleton instance.
	static qtractorActionControl *g_pActionControl;
};


#endif	// __qtractorActionControl_h

// end of qtractorActionControl.h
