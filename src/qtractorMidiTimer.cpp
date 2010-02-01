// qtractorMidiTimer.cpp
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

#include "qtractorMidiTimer.h"

#include <alsa/asoundlib.h>

#include <stdio.h>


//----------------------------------------------------------------------
// class qtractorMidiTimer::Key -- ALSA sequencer timer key stuff.
//

// Setters.
void qtractorMidiTimer::Key::setAlsaTimer ( int iAlsaTimer )
{
	if (iAlsaTimer == 0)
		iAlsaTimer = ((SND_TIMER_CLASS_GLOBAL & 0x7f) << 24);
	m_iAlsaTimer = iAlsaTimer;
}

void qtractorMidiTimer::Key::setAlsaTimer (
	int iClass, int iCard, int iDevice, int iSubDev )
{
	if (iClass < 0 || iClass == SND_TIMER_CLASS_NONE)
		iClass = SND_TIMER_CLASS_GLOBAL;
	setAlsaTimer((unsigned long)
		((iClass  & 0x7f) << 24) |
		((iCard   & 0x7f) << 16) |
		((iDevice & 0x7f) <<  8) |
		 (iSubDev & 0x7f));
}


//----------------------------------------------------------------------
// class qtractorMidiTimer -- ALSA sequencer timer stuff (singleton).
//

// Constructor.
qtractorMidiTimer::qtractorMidiTimer (void)
{
	m_keys.clear();
	m_names.clear();

	m_keys.append(0);
	m_names.append(QObject::tr("(default)"));

	snd_timer_query_t *pTimerQuery = NULL;
	if (snd_timer_query_open(&pTimerQuery, "hw", 0) >= 0) {
		snd_timer_id_t *pTimerID = NULL;
		snd_timer_id_alloca(&pTimerID);
		snd_timer_id_set_class(pTimerID, SND_TIMER_CLASS_NONE);
		while (snd_timer_query_next_device(pTimerQuery, pTimerID) >= 0) {
			int iClass = snd_timer_id_get_class(pTimerID);
			if (iClass < 0)
				break;
			int iSClass = snd_timer_id_get_sclass(pTimerID);
			if (iSClass < 0)
				iSClass = 0;
			int iCard = snd_timer_id_get_card(pTimerID);
			if (iCard < 0)
				iCard = 0;
			int iDevice = snd_timer_id_get_device(pTimerID);
			if (iDevice < 0)
				iDevice = 0;
			int iSubDev = snd_timer_id_get_subdevice(pTimerID);
			if (iSubDev < 0)
				iSubDev = 0;
			char szTimer[64];
			snprintf(szTimer, sizeof(szTimer) - 1,
				"hw:CLASS=%i,SCLASS=%i,CARD=%i,DEV=%i,SUBDEV=%i",
				iClass, iSClass, iCard, iDevice, iSubDev);
			snd_timer_t *pTimer = NULL;
			if (snd_timer_open(&pTimer, szTimer, SND_TIMER_OPEN_NONBLOCK) >= 0) {
				snd_timer_info_t *pTimerInfo;
				snd_timer_info_alloca(&pTimerInfo);
				if (snd_timer_info(pTimer, pTimerInfo) >= 0) {
					qtractorMidiTimer::Key key(iClass, iCard, iDevice, iSubDev);
					long iResol = snd_timer_info_get_resolution(pTimerInfo);
					QString sTimerName = QString::fromUtf8(
						snd_timer_info_get_name(pTimerInfo));
					QString sTimerText;
					if (!snd_timer_info_is_slave(pTimerInfo) && iResol > 0)
						sTimerText = QObject::tr("%1 Hz").arg(1000000000L / iResol); 
					else
						sTimerText = QObject::tr("slave");
					m_keys.append(key.alsaTimer());
					m_names.append(QObject::tr("%1 (%2)")
						.arg(sTimerName).arg(sTimerText));
				}
				snd_timer_close(pTimer);
			}
		}
		snd_timer_query_close(pTimerQuery);
	}
}


// Destructor.
qtractorMidiTimer::~qtractorMidiTimer (void)
{
	m_names.clear();	
	m_keys.clear();
}


// Returns the number of available timers.
int qtractorMidiTimer::count (void) const
{
	return m_keys.count();
}


// Index of the a timer key.
int qtractorMidiTimer::indexOf ( int iKey ) const
{
	int iIndex = m_keys.indexOf(iKey);
	if (iIndex < 0)
		iIndex = 0;
	return iIndex;
}


// Key from index.
int qtractorMidiTimer::key ( int iIndex ) const
{
	if (iIndex < 0 || iIndex >= m_names.count())
		iIndex = 0;

	return m_keys.at(iIndex);
}


// Name from index.
const QString& qtractorMidiTimer::name ( int iIndex ) const
{
	if (iIndex < 0 || iIndex >= m_names.count())
		iIndex = 0;

	return m_names.at(iIndex);
}


// end of qtractorMidiTimer.cpp
