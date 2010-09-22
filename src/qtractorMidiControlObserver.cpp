// qtractorMidiControlObserver.cpp
//
/****************************************************************************
   Copyright (C) 2010, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiControlObserver.h"


//----------------------------------------------------------------------
// class qtractorMidiControlObserver -- MIDI controller observers.
//


// Constructor.
qtractorMidiControlObserver::qtractorMidiControlObserver (
	qtractorSubject *pSubject ) : qtractorObserver(pSubject),
		m_ctype(qtractorMidiEvent::CONTROLLER),
		m_iChannel(0), m_iParam(0), m_bFeedback(false),
		m_fMinValue(0.0f), m_fMaxValue(1.0f)
{
}


// MIDI mapped value converters.
void qtractorMidiControlObserver::setMidiValue ( unsigned short iValue )
{
	setValue(m_fMinValue + float(iValue) * (m_fMaxValue - m_fMinValue) / 127.0f);
}

unsigned short qtractorMidiControlObserver::midiValue (void) const
{
	return 127.0f * (value() - m_fMinValue) / (m_fMaxValue - m_fMinValue);
}


// Updater (feedback).
void qtractorMidiControlObserver::update (void)
{
	if (m_bFeedback) {
		qtractorMidiControl *pMidiControl
			= qtractorMidiControl::getInstance();
		if (pMidiControl && pMidiControl->isMidiObserverMapped(this)) {
			pMidiControl->sendController(
				m_ctype, m_iChannel, m_iParam, midiValue());
		}
	}
}


// end of qtractorMidiControlObserver.cpp
