// qtractorMidiControlObserver.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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
		m_bLogarithmic(false), m_bFeedback(false), m_bInvert(false), m_bHook(false),
		m_bMidiValueInit(false), m_bMidiValueSync(false), m_fMidiValue(0.0f),
		m_pCurveList(NULL)
{
}


// Destructor (virtual).
qtractorMidiControlObserver::~qtractorMidiControlObserver (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl && pMidiControl->isMidiObserverMapped(this))
		pMidiControl->unmapMidiObserver(this);
}


// MIDI scale type (7bit vs. 14bit).
unsigned short qtractorMidiControlObserver::midiScale (void) const
{
	if (m_ctype == qtractorMidiEvent::PITCHBEND ||
		m_ctype == qtractorMidiEvent::CONTROL14 ||
		m_ctype == qtractorMidiEvent::REGPARAM  ||
		m_ctype == qtractorMidiEvent::NONREGPARAM)
		return 0x3fff;
	else
		return 0x7f;
}


// MIDI mapped value converters.
void qtractorMidiControlObserver::setMidiValue ( unsigned short iValue )
{
	const unsigned short iScale = midiScale();
//	setScaleValue(float(iValue) / float(iScale));
	if (iValue > iScale)
		iValue = iScale;

	const float fScale
		= float(m_bInvert ? iScale - iValue : iValue) / float(iScale);
	const float fValue
		= valueFromScale(fScale, m_bLogarithmic);

	if (m_bHook)
		m_bMidiValueSync = true;

	if (m_bMidiValueInit && !m_bMidiValueSync) {
		const float v0 = m_fMidiValue;
		const float v1 = qtractorObserver::value();
		if ((fValue > v0 && v1 >= v0 && fValue >= v1) ||
			(fValue < v0 && v0 >= v1 && v1 >= fValue))
			 m_bMidiValueSync = true;
	}

	if (m_bMidiValueSync)
		subject()->setValue(fValue);
	else
		m_bMidiValueInit = true;

	m_fMidiValue = fValue;
}

unsigned short qtractorMidiControlObserver::midiValue (void) const
{
	const unsigned short iScale = midiScale();
	unsigned short iValue = float(iScale) * scaleValue();
	if (iValue > iScale)
		iValue = iScale;
	return (m_bInvert ? iScale - iValue : iValue);
}


// Normalized scale converters.
float qtractorMidiControlObserver::valueFromScale (
	float fScale, bool bLogarithmic ) const
{
	if (bLogarithmic) {
		const float fMaxValue = qtractorObserver::maxValue();
		const float fMinValue = qtractorObserver::minValue();
		if (fMinValue < 0.0f && fMaxValue > 0.0f) {
			const float fMidScale = fMinValue / (fMinValue - fMaxValue);
			if (fScale > fMidScale) {
				const float fMaxScale = (1.0f - fMidScale);
				fScale = (fScale - fMidScale) / fMaxScale;
				fScale = fMidScale + fMaxScale * ::cubef2(fScale); 
			} else {
				fScale = (fMidScale - fScale) / fMidScale;
				fScale = fMidScale - fMidScale * ::cubef2(fScale); 
			}
		}
		else fScale = ::cubef2(fScale);
	}

	return qtractorObserver::valueFromScale(fScale);
}


float qtractorMidiControlObserver::scaleFromValue (
	float fValue, bool bLogarithmic ) const
{
	float fScale = qtractorObserver::scaleFromValue(fValue);

	if (bLogarithmic) {
		const float fMaxValue = qtractorObserver::maxValue();
		const float fMinValue = qtractorObserver::minValue();
		if (fMinValue < 0.0f && fMaxValue > 0.0f) {
			const float fMidScale = fMinValue / (fMinValue - fMaxValue);
			if (fScale > fMidScale) {
				const float fMaxScale = (1.0f - fMidScale);
				fScale = (fScale - fMidScale) / fMaxScale;
				fScale = fMidScale + fMaxScale * ::cbrtf2(fScale); 
			} else {
				fScale = (fMidScale - fScale) / fMidScale;
				fScale = fMidScale - fMidScale * ::cbrtf2(fScale); 
			}
		}
		else fScale = ::cbrtf2(fScale);
	}

	return fScale;
}


// Updater (feedback).
void qtractorMidiControlObserver::update ( bool bUpdate )
{
	if (bUpdate && m_bFeedback) {
		qtractorMidiControl *pMidiControl
			= qtractorMidiControl::getInstance();
		if (pMidiControl && pMidiControl->isMidiObserverMapped(this)) {
			pMidiControl->sendController(
				m_ctype, m_iChannel, m_iParam, midiValue());
		}
	}

	m_bMidiValueSync = false;
}


// end of qtractorMidiControlObserver.cpp
