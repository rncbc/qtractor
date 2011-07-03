// qtractorAudioMonitor.cpp
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

#include "qtractorAudioMonitor.h"

#include <math.h>


#if defined(__SSE__)

#include <xmmintrin.h>

// SSE detection.
static inline bool sse_enabled (void)
{
#if defined(__GNUC__)
	unsigned int eax, ebx, ecx, edx;
#if defined(__x86_64__) || (!defined(PIC) && !defined(__PIC__))
	__asm__ __volatile__ (
		"cpuid\n\t" \
		: "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx) \
		: "a" (1) : "cc");
#else
	__asm__ __volatile__ (
		"push %%ebx\n\t" \
		"cpuid\n\t" \
		"movl %%ebx,%1\n\t" \
		"pop %%ebx\n\t" \
		: "=a" (eax), "=r" (ebx), "=c" (ecx), "=d" (edx) \
		: "a" (1) : "cc");
#endif
	return (edx & (1 << 25));
#else
	return false;
#endif
}


// SSE enabled processor versions.
static inline void sse_process (
	float *pFrames, unsigned int iFrames, float fGain, float *pfValue )
{
	__m128 v0 = _mm_load_ps1(&fGain);
	__m128 v1 = _mm_load_ps1(pfValue);
	__m128 v2;

	for (; (long(pFrames) & 15) && (iFrames > 0); --iFrames)
		*pFrames++ *= fGain;
	
	for (; iFrames >= 4; iFrames -= 4) {
		v2 = _mm_mul_ps(_mm_loadu_ps(pFrames), v0);
		v1 = _mm_max_ps(v2, v1);
		_mm_store_ps(pFrames, v2);
		pFrames += 4;
	}
	
	for (; iFrames > 0; --iFrames)
		*pFrames++ *= fGain;

	*pfValue = *(float *) &v1; // CHEAT: take 1st of 4 possible values.
}

static inline void sse_process_meter (
	float *pFrames, unsigned int iFrames, float *pfValue )
{
	__m128 v1 = _mm_load_ps1(pfValue);

	for (; (long(pFrames) & 15) && (iFrames > 0); --iFrames)
		++pFrames;
	
	for (; iFrames >= 4; iFrames -= 4) {
		v1 = _mm_max_ps(_mm_loadu_ps(pFrames), v1);
		pFrames += 4;
	}
	
	*pfValue = *(float *) &v1; // CHEAT: take 1st of 4 possible values.
}

#endif


// Standard processor versions.
static inline void std_process (
	float *pFrames, unsigned int iFrames, float fGain, float *pfValue )
{
	for (unsigned int n = 0; n < iFrames; ++n) {
		pFrames[n] *= fGain;
		if (*pfValue < pFrames[n])
			*pfValue = pFrames[n];
	}
}

static inline void std_process_meter (
	float *pFrames, unsigned int iFrames, float *pfValue )
{
	for (unsigned int n = 0; n < iFrames; ++n) {
		if (*pfValue < pFrames[n])
			*pfValue = pFrames[n];
	}
}


//----------------------------------------------------------------------------
// qtractorAudioMonitor -- Audio monitor bridge value processor.

// Constructor.
qtractorAudioMonitor::qtractorAudioMonitor ( unsigned short iChannels,
	float fGain, float fPanning ) : qtractorMonitor(fGain, fPanning),
	m_iChannels(0), m_pfValues(0), m_pfGains(0)
{
#if defined(__SSE__)
	if (sse_enabled()) {
		m_pfnProcess = sse_process;
		m_pfnProcessMeter = sse_process_meter;
	} else {
#endif
	m_pfnProcess = std_process;
	m_pfnProcessMeter = std_process_meter;
#if defined(__SSE__)
	}
#endif

	setChannels(iChannels);
}


// Destructor.
qtractorAudioMonitor::~qtractorAudioMonitor (void)
{
	setChannels(0);
}


// Channel property accessors.
void qtractorAudioMonitor::setChannels ( unsigned short iChannels )
{
	// Check if channels will really change...
	if (m_iChannels == iChannels)
		return;

	// Delete old value holders...
	if (m_pfValues) {
		delete [] m_pfValues;
		m_pfValues = 0;
	}

	// Delete old panning-gains holders...
	if (m_pfGains) {
		delete [] m_pfGains;
		m_pfGains = 0;
	}

	// Set new value holders...
	m_iChannels = iChannels;
	if (m_iChannels > 0) {
		m_pfValues = new float [m_iChannels];
		for (unsigned short i = 0; i < m_iChannels; ++i)
			m_pfValues[i] = 0.0f;
		m_pfGains = new float [m_iChannels];
		update();
	}
}

unsigned short qtractorAudioMonitor::channels (void) const
{
	return m_iChannels;
}


// Value holder accessor.
float qtractorAudioMonitor::value ( unsigned short iChannel ) const
{
	float fValue = m_pfValues[iChannel];
	m_pfValues[iChannel] = 0.0f;
	return fValue;
}


void qtractorAudioMonitor::process (
	float **ppFrames, unsigned int iFrames, unsigned short iChannels )
{
	if (iChannels < 1)
		iChannels = m_iChannels;

	if (iChannels == m_iChannels) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			(*m_pfnProcess)(ppFrames[i], iFrames, m_pfGains[i], &m_pfValues[i]);
	}
	else if (iChannels > m_iChannels) {
		unsigned short j = 0;
		for (unsigned short i = 0; i < iChannels; ++i) {
			(*m_pfnProcess)(ppFrames[i], iFrames, m_pfGains[j], &m_pfValues[j]);
			if (++j >= m_iChannels)
				j = 0;
		}
	}
	else { // (iChannels < m_iChannels)
		unsigned short i = 0;
		for (unsigned short j = 0; j < m_iChannels; ++j) {
			(*m_pfnProcess)(ppFrames[i], iFrames, m_pfGains[j], &m_pfValues[j]);
			if (++i >= iChannels)
				i = 0;
		}
	}
}

void qtractorAudioMonitor::process_meter (
	float **ppFrames, unsigned int iFrames, unsigned short iChannels )
{
	if (iChannels < 1)
		iChannels = m_iChannels;

	if (iChannels == m_iChannels) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			(*m_pfnProcessMeter)(ppFrames[i], iFrames, &m_pfValues[i]);
	}
	else if (iChannels > m_iChannels) {
		unsigned short j = 0;
		for (unsigned short i = 0; i < iChannels; ++i) {
			(*m_pfnProcessMeter)(ppFrames[i], iFrames, &m_pfValues[j]);
			if (++j >= m_iChannels)
				j = 0;
		}
	}
	else { // (iChannels < m_iChannels)
		unsigned short i = 0;
		for (unsigned short j = 0; j < m_iChannels; ++j) {
			(*m_pfnProcessMeter)(ppFrames[i], iFrames, &m_pfValues[j]);
			if (++i >= iChannels)
				i = 0;
		}
	}
}


// Rebuild the whole panning-gain array...
void qtractorAudioMonitor::update (void)
{
	// (Re)compute equal-power stereo-panning gains...
	const float fPan = 0.5f * (1.0f + panning());
	float afGains[2] = { gain(), gain() };
	if (panning() < -0.001f || panning() > +0.001f) {
#ifdef QTRACTOR_MONITOR_PANNING_SQRT
		afGains[0] *= M_SQRT2 * ::sqrtf(1.0f - fPan);
		afGains[1] *= M_SQRT2 * ::sqrtf(fPan);
#else
		afGains[0] *= M_SQRT2 * ::cosf(fPan * M_PI_2);
		afGains[1] *= M_SQRT2 * ::sinf(fPan * M_PI_2);
#endif
	}
	// Apply to multi-channel gain array (paired fashion)...
	unsigned short i;
	unsigned short iChannels = (m_iChannels - (m_iChannels % 2));
	for (i = 0; i < iChannels; ++i)
		m_pfGains[i] = afGains[i % 2];
	while (i < m_iChannels)
		m_pfGains[i++] = gain();
}


// end of qtractorAudioMonitor.cpp
