// qtractorAudioMonitor.h
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioMonitor_h
#define __qtractorAudioMonitor_h

#include "qtractorMonitor.h"

// Forward decls.
class qtractorAudioMeter;


//----------------------------------------------------------------------------
// qtractorAudioMonitor -- Audio monitor bridge value processor.

class qtractorAudioMonitor : public qtractorMonitor
{
public:

	// Constructor.
	qtractorAudioMonitor(unsigned short iChannels,
		float fGain = 1.0f, float fPanning = 0.0f);

	// Destructor.
	~qtractorAudioMonitor();

	// Channel property accessors.
	void setChannels(unsigned short iChannels);
	unsigned short channels() const;

	// Value holder accessor.
	float value_stamp(unsigned short iChannel, unsigned long iStamp) const;

	// Batch processors.
	void process(float **ppFrames,
		unsigned int iFrames, unsigned short iChannels = 0);
	void process_meter(float **ppFrames,
		unsigned int iFrames, unsigned short iChannels = 0);

	// Reset channel gain trackers.
	void reset();

protected:

	// Rebuild the whole panning-gain array...
	void update();

private:

	// Instance variables.
	unsigned short m_iChannels;
	unsigned long *m_piStamps;
	float         *m_pfValues;
	float         *m_pfPrevValues;
	float         *m_pfGains;
	float         *m_pfPrevGains;
	volatile int   m_iProcessRamp;

	// Monitoring evaluator processor.
	void (*m_pfnProcess)(float *, unsigned int, float, float *);
	void (*m_pfnProcessRamp)(float *, unsigned int, float, float, float *);
	void (*m_pfnProcessMeter)(float *, unsigned int, float *);
};


//----------------------------------------------------------------------------
// qtractorAudioOutputMonitor -- Audio-output monitor bridge value processor.

class qtractorAudioOutputMonitor : public qtractorAudioMonitor
{
public:

	// Constructor.
	qtractorAudioOutputMonitor(unsigned short iChannels,
		float fGain = 1.0f, float fPanning = 0.0f);

	// Channel property accessors.
	void setChannels(unsigned short iChannels);

	// Associated meters (kinda observers) managament methods.
	void addAudioMeter(qtractorAudioMeter *pAudioMeter);
	void removeAudioMeter(qtractorAudioMeter *pAudioMeter);

private:

	// Instance variables.
	QList<qtractorAudioMeter *> m_meters;
};


#endif  // __qtractorAudioMonitor_h

// end of qtractorAudioMonitor.h
