// qtractorMonitor.h
//
/****************************************************************************
   Copyright (C) 2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorMonitor_h
#define __qtractorMonitor_h


//----------------------------------------------------------------------------
// qtractorMonitor -- Monitor bridge value widget.

class qtractorMonitor
{
public:

	// Constructor.
	qtractorMonitor(unsigned short iChannels)
		: m_iChannels(0), m_fGain(1.0f), m_pfValues(0)
		{ setChannels(iChannels); }

	// Destructor.
	~qtractorMonitor()
		{ setChannels(0); }

	// Gain accessors.
	float gain() const
		{ return m_fGain; }
	void setGain(float fGain)
		{ m_fGain = fGain; }

	// Channel property accessors.
	unsigned short channels() const
		{ return m_iChannels; }

	void setChannels(unsigned short iChannels)
	{
		// Delete old value holders...
		if (m_pfValues) {
			delete [] m_pfValues;
			m_pfValues = NULL;
		}	
		// Set new value holders...
		m_iChannels = iChannels;
		if (m_iChannels > 0) {
			m_pfValues = new float [m_iChannels];
			for (unsigned short i = 0; i < m_iChannels; i++)
				m_pfValues[i] = 0.0f;
		}
	}		

	// Value holder accessors.
	float value(unsigned short iChannel) const
	{
		float fValue = m_pfValues[iChannel];
		m_pfValues[iChannel] = 0.0f;
		return fValue;
	}

	void setValue(unsigned short iChannel, float fValue)
	{
		if (m_pfValues[iChannel] < fValue)
			m_pfValues[iChannel] = fValue;
	}

	// Batch processor.
	void process(float **ppFrames, unsigned int iFrames)
	{
		for (unsigned short i = 0; i < m_iChannels; i++) {
			for (unsigned int n = 0; n < iFrames; n++)
				setValue(i, ppFrames[i][n] *= m_fGain);
		}
	}

private:

	// Instance variables.
	unsigned short m_iChannels;
	float          m_fGain;
	float         *m_pfValues;
};


#endif  // __qtractorMonitor_h

// end of qtractorMonitor.h
