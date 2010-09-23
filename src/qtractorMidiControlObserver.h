// qtractorMidiControlObserver.h
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

#ifndef __qtractorMidiControlObserver_h
#define __qtractorMidiControlObserver_h

#include "qtractorObserver.h"

#include "qtractorMidiControl.h"


//----------------------------------------------------------------------
// class qtractorMidiControlObserver -- MIDI controller observers.
//

class qtractorMidiControlObserver : public qtractorObserver
{
public:

	// Constructor.
	qtractorMidiControlObserver(qtractorSubject *pSubject);

	// Key accessors.
	void setType(qtractorMidiControl::ControlType ctype)
		{ m_ctype = ctype; }
	qtractorMidiControl::ControlType type() const
		{ return m_ctype; }

	void setChannel(unsigned short iChannel)
		{ m_iChannel = iChannel; }
	unsigned short channel() const
		{ return m_iChannel; }

	void setParam(unsigned short iParam)
		{ m_iParam = iParam; }
	unsigned short param() const
		{ return m_iParam; }

	// Properties accessors.
	void setLogarithmic(bool bLogarithmic)
		{ m_bLogarithmic = bLogarithmic; }
	bool isLogarithmic() const
		{ return m_bLogarithmic; }

	void setFeedback(bool bFeedback)
		{ m_bFeedback = bFeedback; }
	bool isFeedback() const
		{ return m_bFeedback; }

	// Value mapping limits.
	void setMaxValue(float fMaxValue)
		{ m_fMaxValue = fMaxValue; }
	float maxValue() const
		{ return m_fMaxValue; }

	void setMinValue(float fMinValue)
		{ m_fMinValue = fMinValue; }
	float minValue() const
		{ return m_fMinValue; }

	// Normalized scale accessors.
	void setScaleValue(float fScale)
		{ setValue(valueFromScale(fScale, m_bLogarithmic)); }
	float scaleValue() const
		{ return scaleFromValue(value(), m_bLogarithmic); }

	// MIDI mapped value converters.
	void setMidiValue(unsigned short iMidiValue);
	unsigned short midiValue() const;

	// Normalized scale convertors.
	float valueFromScale(float fScale, bool bCubic) const;
	float scaleFromValue(float fValue, bool bCubic) const;

protected:

	// Updater.
	virtual void update();

private:

	// Key members.
	qtractorMidiControl::ControlType m_ctype;
	unsigned short m_iChannel;
	unsigned short m_iParam;

	// Property members.
	bool m_bLogarithmic;
	bool m_bFeedback;

	// Value mapping limits.
	float m_fMinValue;
	float m_fMaxValue;
};


#endif  // __qtractorMidiControlObserver_h


// end of qtractorMidiControlObserver.h
