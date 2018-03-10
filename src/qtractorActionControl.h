// qtractorActionControl.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

		MidiObserver(QAction *pAction);

	protected:

		void update(bool bUpdate);

	private:

		QAction *m_pAction;
		qtractorSubject m_subject;
	};

	// MIDI observers map accessor.
	typedef QHash<QAction *, MidiObserver *> MidiObservers;

	const MidiObservers& midiObservers() const
		{ return m_midiObservers; }

	// MIDI observers map methods.
	MidiObserver *getMidiObserver(QAction *pAction);
	MidiObserver *addMidiObserver(QAction *pAction);
	void removeMidiObserver(QAction *pAction);

	// MIDI observers map cleaner.
	void clear();

	// Pseudo-singleton instance accessor.
	static qtractorActionControl *getInstance();

	// Complete action text, from associated menus.
	static QString menuActionText(QAction *pAction, const QString& sText);

protected slots:

	// MIDI observer trigger slot.
	void triggeredSlot(bool bOn);

private:

	// MIDI observers map.
	MidiObservers m_midiObservers;

	// Pseudo-singleton instance.
	static qtractorActionControl *g_pActionControl;
};


#endif	// __qtractorActionControl_h

// end of qtractorActionControl.h
