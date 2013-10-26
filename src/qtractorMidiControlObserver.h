// qtractorMidiControlObserver.h
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

#ifndef __qtractorMidiControlObserver_h
#define __qtractorMidiControlObserver_h

#include "qtractorObserver.h"

#include "qtractorMidiControl.h"


// Forward declarations.
class qtractorCurveList;


//----------------------------------------------------------------------
// class qtractorMidiControlObserver -- MIDI controller observers.
//

class qtractorMidiControlObserver : public qtractorObserver
{
public:

	// Constructor.
	qtractorMidiControlObserver(qtractorSubject *pSubject);

	// Destructor.
	virtual ~qtractorMidiControlObserver();

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

	void setInvert(bool bInvert)
		{ m_bInvert = bInvert; }
	bool isInvert() const
		{ return m_bInvert; }

	void setHook(bool bHook)
		{ m_bHook = bHook; }
	bool isHook() const
		{ return m_bHook; }

	// Normalized scale accessors.
	void setScaleValue(float fScale)
		{ setValue(valueFromScale(fScale, m_bLogarithmic)); }
	float scaleValue() const
		{ return scaleFromValue(value(), m_bLogarithmic); }

	// MIDI mapped value converters.
	void setMidiValue(unsigned short iMidiValue);
	unsigned short midiValue() const;

	// Normalized scale convertors.
	float valueFromScale(float fScale, bool bLogarithmic) const;
	float scaleFromValue(float fValue, bool bLogarithmic) const;

	// Special indirect automation relatives accessors.
	void setCurveList(qtractorCurveList *pCurveList)
		{ m_pCurveList = pCurveList; }
	qtractorCurveList *curveList() const
		{ return m_pCurveList; }

protected:

	// Updater.
	virtual void update(bool bUpdate);

	// MIDI scale type (7bit vs. 14bit).
	unsigned short midiScale() const;

private:

	// Key members.
	qtractorMidiControl::ControlType m_ctype;
	unsigned short m_iChannel;
	unsigned short m_iParam;

	// Property members.
	bool m_bLogarithmic;
	bool m_bFeedback;
	bool m_bInvert;
	bool m_bHook;

	// Tracking/catch-up members.
	bool  m_bMidiValueInit;
	bool  m_bMidiValueSync;
	float m_fMidiValue;
	
	// Special indirect automation relatives.
	qtractorCurveList *m_pCurveList;
};


#endif  // __qtractorMidiControlObserver_h


// end of qtractorMidiControlObserver.h
