// qtractorMidiControlObserver.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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


#include <math.h>

// Possible cube root optimization.
// (borrowed from metamerist.com)
static inline float cbrtf2 ( float x )
{
#ifdef CONFIG_FLOAT32
	// Avoid strict-aliasing optimization (gcc -O2).
	union { float f; int i; } u;
	u.f = x;
	u.i = (u.i / 3) + 710235478;
	return u.f;
#else
	return cbrtf(x);
#endif
}

static inline float cubef2 ( float x )
{
	return x * x * x;
}


//----------------------------------------------------------------------
// class qtractorMidiControlObserver -- MIDI controller observers.
//


// Constructor.
qtractorMidiControlObserver::qtractorMidiControlObserver (
	qtractorSubject *pSubject ) : qtractorObserver(pSubject),
		m_ctype(qtractorMidiEvent::CONTROLLER), m_iChannel(0), m_iParam(0),
		m_bLogarithmic(false), m_bFeedback(false), m_bInvert(false)
{
}


// Destructor (virtual).
qtractorMidiControlObserver::~qtractorMidiControlObserver (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl && pMidiControl->isMidiObserverMapped(this))
		pMidiControl->unmapMidiObserver(this);
}


// MIDI mapped value converters.
void qtractorMidiControlObserver::setMidiValue ( unsigned short iValue )
{
	const unsigned short iRatio
		= (m_ctype == qtractorMidiEvent::PITCHBEND ? 0x3fff : 0x7f);
//	setScaleValue(float(iValue) / float(iRatio));
	float fScale = float(m_bInvert ? iRatio - iValue : iValue) / float(iRatio);
	subject()->setValue(valueFromScale(fScale, m_bLogarithmic));
}

unsigned short qtractorMidiControlObserver::midiValue (void) const
{
	const unsigned short iRatio
		= float(m_ctype == qtractorMidiEvent::PITCHBEND ? 0x3fff : 0x7f);
	unsigned short iValue = float(iRatio) * scaleValue();
	return (m_bInvert ? iRatio - iValue : iValue);
}


// Normalized scale coneverters.
float qtractorMidiControlObserver::valueFromScale ( float fScale, bool bCubic ) const
{
	if (bCubic)
		fScale = ::cubef2(fScale);

	return minValue() + fScale * (maxValue() - minValue());
}

float qtractorMidiControlObserver::scaleFromValue ( float fValue, bool bCubic ) const
{
	float fScale = 0.0f;

	float fDelta = (maxValue() - minValue());
	if (fDelta > +1E-6f || fDelta < -1E-6f)
		fScale = (fValue - minValue()) / fDelta;

	if (bCubic)
		fScale = ::cbrtf2(fScale);

	return fScale;
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
