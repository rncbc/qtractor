// qtractorTimeScale.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTimeScale.h"


//----------------------------------------------------------------------
// class qtractorTimeScale -- Time scale conversion helper class.
//

// (Re)nitializer method.
void qtractorTimeScale::clear (void)
{
	m_iSampleRate     = 44100;
	m_fTempo          = 120.0f;
	m_iTicksPerBeat   = 96;
	m_iBeatsPerBar    = 4;
	m_iPixelsPerBeat  = 32;
	m_iSnapPerBeat    = 4;
	m_iHorizontalZoom = 100;
	m_iVerticalZoom   = 100;

//	m_displayFormat   = Frames;

	updateScale();
}

// Copy method.
qtractorTimeScale& qtractorTimeScale::copy ( const qtractorTimeScale& ts )
{
	if (&ts != this) {

		m_iSampleRate     = ts.m_iSampleRate;
		m_fTempo          = ts.m_fTempo;
		m_iTicksPerBeat   = ts.m_iTicksPerBeat;
		m_iBeatsPerBar    = ts.m_iBeatsPerBar;
		m_iPixelsPerBeat  = ts.m_iPixelsPerBeat;
		m_iSnapPerBeat    = ts.m_iSnapPerBeat;
		m_iHorizontalZoom = ts.m_iHorizontalZoom;
		m_iVerticalZoom   = ts.m_iVerticalZoom;

		m_displayFormat   = ts.m_displayFormat;

		updateScale();
	}

	return *this;
}


// Update scale divisor factors.
void qtractorTimeScale::updateScale (void)
{
	m_iScale_a = (unsigned int) (m_iHorizontalZoom * m_iPixelsPerBeat);
	m_fScale_b = (float) (0.01f * m_fTempo * m_iScale_a);
	m_fScale_c = (float) (60.0f * m_iSampleRate);
	m_fScale_d = (float) (m_fTempo * m_iTicksPerBeat);
}


// Beat/frame snap filters.
unsigned long qtractorTimeScale::tickSnap ( unsigned long iTick ) const
{
	unsigned long iTickSnap = iTick;
	if (m_iSnapPerBeat > 0) {
		unsigned long q = m_iTicksPerBeat / m_iSnapPerBeat;
		iTickSnap = q * ((iTickSnap + (q >> 1)) / q);
	}
	return iTickSnap;
}


// Value/text format converters.
unsigned long qtractorTimeScale::frameFromText ( const QString& sText ) const
{
	unsigned long iFrame = 0;

	switch (m_displayFormat) {

		case BBT:
		{
			// Time frame code in bars.beats.ticks ...
			unsigned int  bars  = sText.section('.', 0, 0).toUInt();
			unsigned int  beats = sText.section('.', 1, 1).toUInt();
			unsigned long ticks = sText.section('.', 2).toULong();
			if (bars > 0)
				bars--;
			if (beats > 0)
				beats--;
			beats += bars  * m_iBeatsPerBar;
			ticks += beats * m_iTicksPerBeat;
			iFrame = frameFromTick(ticks);
			break;
		}

		case Time:
		{
			// Time frame code in hh:mm:ss.zzz ...
			unsigned int hh = sText.section(':', 0, 0).toUInt();
			unsigned int mm = sText.section(':', 1, 1).toUInt();
			float secs = sText.section(':', 2).toFloat();
			mm   += 60 * hh;
			secs += 60.f * (float) mm;
			iFrame = (unsigned long) ::lroundf(secs * (float) m_iSampleRate);
			break;
		}

		case Frames:
		default:
		{
			iFrame = sText.toULong();
			break;
		}
	}

	return iFrame;
}


QString qtractorTimeScale::textFromFrame ( unsigned long iFrame ) const
{
	QString sText;

	switch (m_displayFormat) {

		case BBT:
		{
			// Time frame code in bars.beats.ticks ...
			unsigned int bars, beats;
			unsigned long ticks = tickFromFrame(iFrame);
			bars = beats = 0;
			if (ticks >= (unsigned long) m_iTicksPerBeat) {
				beats  = (unsigned int)  (ticks / m_iTicksPerBeat);
				ticks -= (unsigned long) (beats * m_iTicksPerBeat);
			}
			if (beats >= (unsigned int) m_iBeatsPerBar) {
				bars   = (unsigned int) (beats / m_iBeatsPerBar);
				beats -= (unsigned int) (bars  * m_iBeatsPerBar);
			}
			sText.sprintf("%u.%02u.%03lu", bars + 1, beats + 1, ticks);
			break;
		}

		case Time:
		{
			// Time frame code in hh:mm:ss.zzz ...
			unsigned int hh, mm, ss, zzz;
			float secs = (float) iFrame / (float) m_iSampleRate;
			hh = mm = ss = 0;
			if (secs >= 3600.0f) {
				hh = (unsigned int) (secs / 3600.0f);
				secs -= (float) hh * 3600.0f;
			}
			if (secs >= 60.0f) {
				mm = (unsigned int) (secs / 60.0f);
				secs -= (float) mm * 60.0f;
			}
			if (secs >= 0.0f) {
				ss = (unsigned int) secs;
				secs -= (float) ss;
			}
			zzz = (unsigned int) (secs * 1000.0f);
			sText.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
			break;
		}

		case Frames:
		default:
		{
			sText = QString::number(iFrame);
			break;
		}
	}

	return sText;
}


// Beat divisor (snap index) accessors.
unsigned short qtractorTimeScale::snapFromIndex ( int iSnap )
{
	unsigned short iSnapPerBeat = 0;
	if (iSnap > 0)
		iSnapPerBeat++;
	for (int i = 1; i < iSnap; i++)
		iSnapPerBeat <<= 1;
	return iSnapPerBeat;
}

int qtractorTimeScale::indexFromSnap ( unsigned short iSnapPerBeat )
{
	int iSnap = 0;
	for (unsigned short n = 1; n <= iSnapPerBeat; n <<= 1)
		++iSnap;
	return iSnap;
}


// end of qtractorTimeScale.cpp
