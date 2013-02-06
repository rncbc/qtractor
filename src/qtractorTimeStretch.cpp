// qtractorTimeStretch.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

   Adapted and refactored from the SoundTouch library (L)GPL,
   Copyright (C) 2001-2012, Olli Parviainen.

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

#include "qtractorTimeStretch.h"

#include <math.h>


// Cross-correlation value calculation over the overlap period.
//

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


// SSE enabled version.
static inline float sse_cross_corr (
	const float *pV1, const float *pV2, unsigned int iOverlapLength )
{
	__m128 vCorr, vNorm, vTemp, *pVec2;

	// Note. It means a major slow-down if the routine needs to tolerate
	// unaligned __m128 memory accesses. It's way faster if we can skip
	// unaligned slots and use _mm_load_ps instruction instead of _mm_loadu_ps.
	// This can mean up to ~ 10-fold difference (incl. part of which is
	// due to skipping every second round for stereo sound though).
	//
	// Little cheating allowed, return valid correlation only for
	// aligned locations, meaning every second round for stereo sound.
	//if (((unsigned long) pV1) & 15) return -1e38f; // Skip unaligned locations.
	// No cheating allowed, use unaligned load & take the resulting
	// performance hit. -- use _mm_loadu_ps() instead of _mm_load_ps();

	// Ensure overlapLength is divisible by 8
	// assert((m_iOverlapLength % 8) == 0);
	iOverlapLength >>= 4;

	// Calculates the cross-correlation value between 'pV1' and 'pV2' vectors
	// Note: pV2 _must_ be aligned to 16-bit boundary, pV1 need not.
	pVec2 = (__m128 *) pV2;
	vCorr = _mm_setzero_ps();
	vNorm = _mm_setzero_ps();

	// Unroll the loop by factor of 4 * 4 operations
	for (unsigned int i = 0; i < iOverlapLength; ++i) {
		// vCorr += pV1[0..3] * pV2[0..3]
		vTemp = _mm_loadu_ps(pV1);
		vCorr = _mm_add_ps(vCorr, _mm_mul_ps(vTemp, pVec2[0]));
		vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));
		// vCorr += pV1[4..7] * pV2[4..7]
		vTemp = _mm_loadu_ps(pV1 + 4);
		vCorr = _mm_add_ps(vCorr, _mm_mul_ps(vTemp, pVec2[1]));
		vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));
		// vCorr += pV1[8..11] * pV2[8..11]
		vTemp = _mm_loadu_ps(pV1 + 8);
		vCorr = _mm_add_ps(vCorr, _mm_mul_ps(vTemp, pVec2[2]));
		vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));
		// vCorr += pV1[12..15] * pV2[12..15]
		vTemp = _mm_loadu_ps(pV1 + 12);
		vCorr = _mm_add_ps(vCorr, _mm_mul_ps(vTemp, pVec2[3]));
		vNorm = _mm_add_ps(vNorm, _mm_mul_ps(vTemp, vTemp));
		pV1 += 16;
		pVec2 += 4;
	}

	float *pvNorm = (float *) &vNorm;
	float fNorm = (pvNorm[0] + pvNorm[1] + pvNorm[2] + pvNorm[3]);

	if (fNorm < 1e-9f) fNorm = 1.0f; // avoid div by zero

	float *pvCorr = (float *) &vCorr;
	return (pvCorr[0] + pvCorr[1] + pvCorr[2] + pvCorr[3]) / ::sqrtf(fNorm);
}

#endif


// Standard (slow) version.
static inline float std_cross_corr (
	const float *pV1, const float *pV2, unsigned int iOverlapLength )
{
	float fCorr = 0.0f;
	float fNorm = 0.0f;

	for (unsigned int i = 0; i < iOverlapLength; ++i) {
		fCorr += pV1[i] * pV2[i];
		fNorm += pV1[i] * pV1[i];
	}

	if (fNorm < 1e-9f) fNorm = 1.0f; // avoid div by zero

	return fCorr / ::sqrtf(fNorm);
}


//---------------------------------------------------------------------------
// qtractorTimeStretch - Time-stretch (tempo change) effect for processed sound.
//

// Constructor.
qtractorTimeStretch::qtractorTimeStretch (
	unsigned short iChannels, unsigned int iSampleRate )
	: m_iChannels(0)
{
	setChannels(iChannels);

	m_fTempo = 1.0f;
	m_bQuickSeek = false;

	m_bMidBufferDirty = false;
	m_ppMidBuffer = NULL;
	m_ppRefMidBuffer = NULL;
	m_ppRefMidBufferUnaligned = NULL;
	m_ppFrames = NULL;

	m_iOverlapLength = 0;

#if defined(__SSE__)
	if (sse_enabled())
		m_pfnCrossCorr = sse_cross_corr;
	else
#endif
	m_pfnCrossCorr = std_cross_corr;

	setParameters(iSampleRate);
}


// Destructor.
qtractorTimeStretch::~qtractorTimeStretch (void)
{
	if (m_ppFrames) {
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			delete [] m_ppMidBuffer[i];
			delete [] m_ppRefMidBufferUnaligned[i];
		}
		delete [] m_ppMidBuffer;
		delete [] m_ppRefMidBufferUnaligned;
		delete [] m_ppRefMidBuffer;
		delete [] m_ppFrames;
	}
}



// Sets the number of channels, 1=mono, 2=stereo.
void qtractorTimeStretch::setChannels ( unsigned short iChannels )
{
	if (m_iChannels == iChannels)
		return;

	m_iChannels = iChannels;
	m_inputBuffer.setChannels(m_iChannels);
	m_outputBuffer.setChannels(m_iChannels);
}


// Get the assigne number of channels, 1=mono, 2=stereo.
unsigned short qtractorTimeStretch::channels (void) const
{
	return m_iChannels;
}


// Sets new target tempo; less than 1.0 values represent
// slower tempo, greater than 1.0 represents faster tempo.
void qtractorTimeStretch::setTempo ( float fTempo )
{
	// Set new is tempo scaling.
	m_fTempo = fTempo;

	calcSeekWindowLength();
	calcOverlapLength();

	// Calculate ideal skip length (according to tempo value) 
	m_fNominalSkip = m_fTempo * (m_iSeekWindowLength - m_iOverlapLength);
	m_fSkipFract = 0;

	// Calculate how many samples are needed in the input buffer 
	// to process another batch of samples.
	m_iFramesReq = (unsigned int) (m_fNominalSkip + 0.5f) + m_iOverlapLength;
	if (m_iFramesReq < m_iSeekWindowLength)
		m_iFramesReq = m_iSeekWindowLength;
	m_iFramesReq += m_iSeekLength;

	clear();

	// These will be enough for most purposes, and
	// shoudl avoid in-the-fly buffer re-allocations...
	m_inputBuffer.ensureCapacity(m_iFramesReq);
	m_outputBuffer.ensureCapacity(m_iFramesReq);
}

// Get assigned target tempo.
float qtractorTimeStretch::tempo (void) const
{
	return m_fTempo;
}


// Set quick-seek mode (hierachical search).
void qtractorTimeStretch::setQuickSeek ( bool bQuickSeek )
{
	m_bQuickSeek = bQuickSeek;
}

// Get quick-seek mode.
bool qtractorTimeStretch::isQuickSeek (void) const
{
	return m_bQuickSeek;
}


// Sets routine control parameters.
// These control are certain time constants defining
// how the sound is stretched to the desired duration.
//
// iSampleRate = sample rate of the sound.
// iSequenceMs = one processing sequence length in milliseconds.
// iSeekWindowMs = seeking window length for scanning the best
//      overlapping position.
// iOverlapMs = overlapping length.
void qtractorTimeStretch::setParameters (
	unsigned int iSampleRate,
	unsigned int iSequenceMs,
	unsigned int iSeekWindowMs,
	unsigned int iOverlapMs )
{
	m_iSampleRate = iSampleRate;
	m_iSequenceMs = iSequenceMs;
	m_iSeekWindowMs = iSeekWindowMs;
	m_iOverlapMs = iOverlapMs;

	m_bAutoSequenceMs = (iSequenceMs < 1);
	m_bAutoSeekWindowMs = (iSeekWindowMs < 1);

	// Set tempo to recalculate required frames...
	setTempo(m_fTempo);
}



// Get routine control parameters, see setParameters() function.
// Any of the parameters to this function can be NULL, in such case
// corresponding parameter value isn't returned.
void qtractorTimeStretch::getParameters (
	unsigned int *piSampleRate,
	unsigned int *piSequenceMs,
	unsigned int *piSeekWindowMs,
	unsigned int *piOverlapMs )
{
	if (piSampleRate)
		*piSampleRate = m_iSampleRate;

	if (piSequenceMs)
		*piSequenceMs = m_iSequenceMs;

	if (piSeekWindowMs)
		*piSeekWindowMs = m_iSeekWindowMs;

	if (piOverlapMs)
		*piOverlapMs = m_iOverlapMs;
}


// Clears mid sample frame buffer.
void qtractorTimeStretch::clearMidBuffer (void)
{
	if (m_bMidBufferDirty) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			::memset(m_ppMidBuffer[i], 0, 2 * m_iOverlapLength * sizeof(float));
		m_bMidBufferDirty = false;
	}
}


// Clears input and mid sample frame buffers.
void qtractorTimeStretch::clearInput (void)
{
	m_inputBuffer.clear();
	clearMidBuffer();
}


// Clears all sample frame buffers.
void qtractorTimeStretch::clear (void)
{
	m_outputBuffer.clear();
	m_inputBuffer.clear();

	clearMidBuffer();
}



// Seeks for the optimal overlap-mixing position.
//
// The best position is determined as the position where
// the two overlapped sample sequences are 'most alike',
// in terms of the highest cross-correlation value over
// the overlapping period.
unsigned int qtractorTimeStretch::seekBestOverlapPosition (void) 
{
	float fBestCorr, fCorr;
	unsigned int iBestOffs, iPrevBestOffs;
	unsigned short i, iStep;
	int iOffs, j, k;
	
	// Slopes the amplitude of the 'midBuffer' samples
	calcCrossCorrReference();

	fBestCorr = -1e38f; // A reasonable lower limit.

	// Scans for the best correlation value by testing each
	// possible position over the permitted range.
	if (m_bQuickSeek) {
		// Hierachical search...
		iPrevBestOffs = (m_iSeekLength + 1) >> 1;
		iOffs = iBestOffs = iPrevBestOffs; 
		for (iStep = 64; iStep > 0; iStep >>= 2) {
			for (k = -1; k <= 1; k += 2) {
				for (j = 1; j < 4 || iStep == 64; ++j) {
					iOffs = iPrevBestOffs + k * j * iStep;
					if (iOffs < 0 || iOffs >= (int) m_iSeekLength)
						break;
					for (i = 0; i < m_iChannels; ++i) {
						// Calculates correlation value for the mixing
						// position corresponding to iOffs.
						fCorr = (*m_pfnCrossCorr)(
							m_inputBuffer.ptrBegin(i) + iOffs,
							m_ppRefMidBuffer[i],
							m_iOverlapLength);
						// Checks for the highest correlation value.
						if (fCorr > fBestCorr) {
							fBestCorr = fCorr;
							iBestOffs = iOffs;
						}
					}
				}
			}
			iPrevBestOffs = iBestOffs;
		}
	} else {
		// Linear search...
		iBestOffs = 0;
		for (iOffs = 0; iOffs < (int) m_iSeekLength; ++iOffs) {
			for (i = 0; i < m_iChannels; ++i) {
				// Calculates correlation value for the mixing
				// position corresponding to iOffs.
				fCorr = (*m_pfnCrossCorr)(
					m_inputBuffer.ptrBegin(i) + iOffs,
					m_ppRefMidBuffer[i], m_iOverlapLength);
				// Checks for the highest correlation value.
				if (fCorr > fBestCorr) {
					fBestCorr = fCorr;
					iBestOffs = iOffs;
				}
			}
		}
	}

	return iBestOffs;
}


// Processes as many processing frames of the samples
// from input-buffer, store the result into output-buffer.
void qtractorTimeStretch::processFrames (void)
{
	unsigned short i;
	unsigned int j, k;
	float *pInput, *pOutput;
	unsigned int iSkip, iOffset;
	int iTemp;

	// If mid-buffer is empty, move the first
	// frames of the input stream into it...
	if (!m_bMidBufferDirty) {
		// Wait until we've got overlapLength samples
		if (m_inputBuffer.frames() < m_iOverlapLength)
			return;
		m_inputBuffer.receiveFrames(m_ppMidBuffer, m_iOverlapLength);
		m_bMidBufferDirty = true;
	}

	// Process frames as long as there are enough in
	// input-buffer to form a processing block.
	while (m_inputBuffer.frames() >= m_iFramesReq) {
	
		// If tempo differs from the nominal,
		// scan for the best overlapping position...
		iOffset = seekBestOverlapPosition();

		// Mix the frames in the input-buffer at position of iOffset
		// with the samples in mid-buffer using sliding overlapping;
		// first partially overlap with the end of the previous
		// sequence (that's in the mid-buffer).
		m_outputBuffer.ensureCapacity(m_iOverlapLength);
		// Overlap...
		for (i = 0; i < m_iChannels; ++i) {
			pInput = m_inputBuffer.ptrBegin(i);
			pOutput = m_outputBuffer.ptrEnd(i);
			for (j = 0; j < m_iOverlapLength ; ++j) {
				k = m_iOverlapLength - j;
				pOutput[j] = (pInput[j + iOffset] * j
					+ m_ppMidBuffer[i][j] * k) / m_iOverlapLength;
			}
		}
		// Commit...
		m_outputBuffer.putFrames(m_iOverlapLength);

		// Then copy sequence samples from input-buffer to output...
		iTemp = (m_iSeekWindowLength - 2 * m_iOverlapLength);
		if (iTemp > 0) {
			// Temporary mapping...
			for (i = 0; i < m_iChannels; ++i) {
				m_ppFrames[i] = m_inputBuffer.ptrBegin(i)
					+ (iOffset + m_iOverlapLength);
			}
			m_outputBuffer.putFrames(m_ppFrames, iTemp);
		}

		// Copies the end of the current sequence from input-buffer to 
		// mid-buffer for being mixed with the beginning of the next 
		// processing sequence and so on
		// assert(iOffset + m_iSeekWindowLength <= m_inputBuffer.frames());
		m_inputBuffer.readFrames(m_ppMidBuffer, m_iOverlapLength,
			(iOffset + m_iSeekWindowLength - m_iOverlapLength));
		m_bMidBufferDirty = true;

		// Remove the processed samples from the input-buffer. Update
		// the difference between integer & nominal skip step to skip-fract
		// in order to prevent the error from accumulating over time.
		m_fSkipFract += m_fNominalSkip; // real skip size
		iSkip = (int) m_fSkipFract;     // rounded to integer skip
		// Maintain the fraction part, i.e. real vs. integer skip
		m_fSkipFract -= iSkip;       
		m_inputBuffer.receiveFrames(iSkip);
	}
}


// Adds frames of samples into the input of the object.
void qtractorTimeStretch::putFrames ( float **ppFrames, unsigned int iFrames )
{
	// Add the frames into the input buffer.
	m_inputBuffer.putFrames(ppFrames, iFrames);
	// Process the samples in input buffer.
	processFrames();
}


// Output frames from beginning of the sample buffer.
// Copies requested frames output buffer and removes them
// from the sample buffer. If there are less than frames()
// samples in the buffer, returns all that available.
unsigned int qtractorTimeStretch::receiveFrames ( float **ppFrames, unsigned int iFrames )
{
	return m_outputBuffer.receiveFrames(ppFrames, iFrames);
}


// Returns number of frames currently available.
unsigned int qtractorTimeStretch::frames() const
{
	return m_outputBuffer.frames();
}


// Flush any last samples that are hiding in the internal processing pipeline.
void qtractorTimeStretch::flushInput (void)
{
	if (m_bMidBufferDirty) {
		// Prepare a dummy empty buffer...
		unsigned short i;
		float dummy[256];
		::memset(&dummy[0], 0, sizeof(dummy));
		for (i = 0; i < m_iChannels; ++i)
			m_ppFrames[i] = &dummy[0];
		// Push the last active frames out from the pipeline
		// by feeding blank samples into processing until
		// new samples appear in the output...
		unsigned int iFrames = frames();
		for (i = 0; i < 128; ++i) {
			putFrames(m_ppFrames, 256);
			// Any new samples appeared in the output?
			if (frames() > iFrames)
				break;
		}
	}

	clearInput();
}


//---------------------------------------------------------------------------
// Floating point arithmetics specific algorithm implementations.
//

// Slopes the amplitude of the mid-buffer samples
// so that cross correlation is faster to calculate
void qtractorTimeStretch::calcCrossCorrReference (void)
{
	for (unsigned int j = 0 ; j < m_iOverlapLength ; ++j) {
		float fTemp = (float) j * (float) (m_iOverlapLength - j);
		for (unsigned short i = 0; i < m_iChannels; ++i)
			m_ppRefMidBuffer[i][j] = (float) (m_ppMidBuffer[i][j] * fTemp);
	}
}


// Calculates overlap period length in frames and reallocate ref-mid-buffer.
void qtractorTimeStretch::calcOverlapLength (void)
{
	// Must be divisible by 8...
	unsigned int iNewOverlapLength = (m_iSampleRate * m_iOverlapMs) / 1000;
	if (iNewOverlapLength < 16)
		iNewOverlapLength = 16;
	iNewOverlapLength -= (iNewOverlapLength % 8);

	unsigned int iOldOverlapLength = m_iOverlapLength;
	m_iOverlapLength = iNewOverlapLength;
	if (m_iOverlapLength > iOldOverlapLength) {
		unsigned short i;
		if (m_ppFrames) {
			for (i = 0; i < m_iChannels; ++i) {
				delete [] m_ppMidBuffer;
				delete [] m_ppRefMidBufferUnaligned;
			}
			delete [] m_ppMidBuffer;
			delete [] m_ppRefMidBufferUnaligned;
			delete [] m_ppRefMidBuffer;
			delete [] m_ppFrames;
		}
		m_ppFrames = new float * [m_iChannels];
		m_ppMidBuffer = new float * [m_iChannels];
		m_ppRefMidBufferUnaligned = new float * [m_iChannels];
		m_ppRefMidBuffer = new float * [m_iChannels];
		for (i = 0; i < m_iChannels; ++i) {
			m_ppMidBuffer[i] = new float [2 * m_iOverlapLength];
			m_ppRefMidBufferUnaligned[i]
				= new float[2 * m_iOverlapLength + 16 / sizeof(float)];
			// Ensure that ref-mid-buffer is aligned
			// to 16 byte boundary for efficiency
			m_ppRefMidBuffer[i] = (float *)
				((((unsigned long) m_ppRefMidBufferUnaligned[i]) + 15) & -16);
		}
		m_bMidBufferDirty = true;
		clearMidBuffer();
	}
}


// Calculates processing sequence length according to tempo setting.
void qtractorTimeStretch::calcSeekWindowLength (void)
{
	// Adjust tempo param according to tempo,
	// so that variating processing sequence length is used...
	// at varius tempo settings, between the given low...top limits
	#define AUTO_TEMPO_MIN	0.5f	// auto setting low tempo range (-50%)
	#define AUTO_TEMPO_MAX	2.0f	// auto setting top tempo range (+100%)
	#define AUTO_TEMPO_DIFF (AUTO_TEMPO_MAX - AUTO_TEMPO_MIN)

	// iSequenceMs setting values at above low & top tempo.
	#define AUTO_SEQ_MIN	40.0f
	#define AUTO_SEQ_MAX	125.0f
	#define AUTO_SEQ_DIFF	(AUTO_SEQ_MAX - AUTO_SEQ_MIN)
	#define AUTO_SEQ_K		(AUTO_SEQ_DIFF / AUTO_TEMPO_DIFF)
	#define AUTO_SEQ_C		(AUTO_SEQ_MIN - (AUTO_SEQ_K * AUTO_TEMPO_MIN))

	// iSeekWindowMs setting values at above low & top tempo.
	#define AUTO_SEEK_MIN	15.0f
	#define AUTO_SEEK_MAX	25.0f
	#define AUTO_SEEK_DIFF	(AUTO_SEEK_MAX - AUTO_SEEK_MIN)
	#define AUTO_SEEK_K		(AUTO_SEEK_DIFF / AUTO_TEMPO_DIFF)
	#define AUTO_SEEK_C		(AUTO_SEEK_MIN - (AUTO_SEEK_K * AUTO_TEMPO_MIN))

	#define AUTO_LIMITS(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

	if (m_bAutoSequenceMs) {
		float fSeq = AUTO_SEQ_C + AUTO_SEQ_K * m_fTempo;
		fSeq = AUTO_LIMITS(fSeq, AUTO_SEQ_MIN, AUTO_SEQ_MAX);
		m_iSequenceMs = (unsigned int) (fSeq + 0.5f);
	}

	if (m_bAutoSeekWindowMs) {
		float fSeek = AUTO_SEEK_C + AUTO_SEEK_K * m_fTempo;
		fSeek = AUTO_LIMITS(fSeek, AUTO_SEEK_MIN, AUTO_SEEK_MAX);
		m_iSeekWindowMs = (unsigned int) (fSeek + 0.5f);
	}

	// Update seek window lengths.
	m_iSeekLength = (m_iSampleRate * m_iSeekWindowMs) / 1000;
	m_iSeekWindowLength = (m_iSampleRate * m_iSequenceMs) / 1000;
}


// end of qtractorTimeStretch.cpp
