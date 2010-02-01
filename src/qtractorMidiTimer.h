// qtractorMidiTimer.h
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

#ifndef __qtractorMidiTimer_h
#define __qtractorMidiTimer_h

#include <QStringList>


//----------------------------------------------------------------------
// class qtractorMidiTimer -- ALSA sequencer timer stuff (singleton).
//

class qtractorMidiTimer
{
public:

	// Constructor.
	qtractorMidiTimer();

	// Destructor.
	~qtractorMidiTimer();

	// Returns the number of available timers.
	int count() const;

	// Index of the a timer key.
	int indexOf(int iKey) const;

	// Key from index.
	int key(int iIndex) const;

	// Name from index.
	const QString& name(int iIndex) const;

	// ALSA sequencer timer key stuff.
	class Key
	{
	public:

		// Constructors.
		Key(int iAlsaTimer = 0)
			{ setAlsaTimer(iAlsaTimer); }
		Key(int iClass, int iCard, int iDevice, int iSubDev)
			{ setAlsaTimer(iClass, iCard, iDevice, iSubDev); }
		Key(const Key& key)
			{ setAlsaTimer(key.alsaTimer()); }

		// Getters.
		int alsaTimer() const
			{ return m_iAlsaTimer; }

		int alsaTimerClass() const
			{ return int((m_iAlsaTimer & 0x7f000000) >> 24); }
		int alsaTimerCard() const
			{ return int((m_iAlsaTimer & 0x007f0000) >> 16); }
		int alsaTimerDevice() const
			{ return int((m_iAlsaTimer & 0x00007f00) >> 8); }
		int alsaTimerSubDev() const
			{ return int(m_iAlsaTimer & 0x0000007f); }

	protected:

		// Setters.
		void setAlsaTimer(int iAlsaTimer);
		void setAlsaTimer(int iClass, int iCard, int iDevice, int iSubDev);

	private:

		// Queue timer
		int m_iAlsaTimer;
	};

private:

	// Instance variables;
	QStringList m_names;
	QList<int>  m_keys;
};


#endif  // __qtractorMidiTimer_h


// end of qtractorMidiTimer.h
