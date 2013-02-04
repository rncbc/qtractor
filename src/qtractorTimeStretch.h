// qtractorTimeStretch.h
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

#ifndef __qtractorTimeStretch_h
#define __qtractorTimeStretch_h

#include "qtractorFifoBuffer.h"


//---------------------------------------------------------------------------
// qtractorTimeStretch - Time-stretch (tempo change) processed sound.
//

class qtractorTimeStretch
{
public:

	// Constructor.
	qtractorTimeStretch(
		unsigned short iChannels = 2,
		unsigned int iSampleRate = 44100);
	
	// Destructor.
	~qtractorTimeStretch();

	// Sets the number of channels, 1=mono, 2=stereo.
	void setChannels(unsigned short iChannels);

	// Get the assigned number of channels, 1=mono, 2=stereo.
	unsigned short channels() const;

	// Sets new target tempo; smaller values represent
	// slower tempo, larger faster tempo.
	void setTempo(float fTempo);

	// Get assigned target tempo.
	float tempo() const;

	// Set quick-seek mode (hierachical search).
	void setQuickSeek(bool bQuickSeek);

	// Get quick-seek mode.
	bool isQuickSeek() const;

	// Default values for sound processing parameters.
	enum {

		// Default length of a single processing sequence, in milliseconds.
		// This determines to how long sequences the original sound is
		// chopped in the time-stretch algorithm.
		//
		// The larger this value is, the lesser number of sequences are used
		// in processing. In principle a bigger value sounds better when
		// slowing down tempo, but worse when increasing tempo and vice versa.
		//
		// Increasing this value reduces computational burden and vice versa.
		// DEFAULT_SEQUENCE_MS = 40

		DEFAULT_SEQUENCE_MS = 0,

		// Seeking window default length in milliseconds for algorithm
		// that finds the best possible overlapping location. This determines
		// from how wide window the algorithm may look for an optimal joining
		// location when mixing the sound sequences back together. 
		//
		// The bigger this window setting is, the higher the possibility
		// to find a better mixing position will become, but at the same time
		// large values may cause a "drifting" artifact because consequent
		// sequences will be taken at more uneven intervals.
		//
		// If there's a disturbing artifact that sounds as if a constant
		// frequency was drifting around, try reducing this setting.
		//
		// Increasing this value increases computational burden and vice versa.
		// DEFAULT_SEEKWINDOW_MS = 15

		DEFAULT_SEEKWINDOW_MS = 0,

		// Overlap length in milliseconds. When the chopped sound sequences
		// are mixed back together, to form a continuous sound stream,
		// this parameter defines over how long period the two consecutive 
		// sequences are let to overlap each other. 
		//
		// This shouldn't be that critical parameter. If you reduce the
		// DEFAULT_SEQUENCE_MS setting by a large amount, you might wish
		// to try a smaller value on this.
		//
		// Increasing this value increases computational burden and vice versa.

		DEFAULT_OVERLAP_MS = 8
	};

	// Sets routine control parameters.
	// These control are certain time constants defining
	// how the sound is stretched to the desired duration.
	//
	// iSampleRate = sample rate of the sound.
	// iSequenceMS = one processing sequence length in milliseconds.
	// iSeekWindowMs = seking window length for scanning the best
	//      verlapping position.
	// iOverlapMs = oerlapping length.
	void setParameters(
			unsigned int iSampleRate,
			unsigned int iSequenceMs = DEFAULT_SEQUENCE_MS,
			unsigned int iSeekWindowMs = DEFAULT_SEEKWINDOW_MS,
			unsigned int iOverlapMs = DEFAULT_OVERLAP_MS);

	// Get routine control parameters, see setParameters() function.
	// Any of the parameters to this function can be NULL in such
	// case corresponding parameter value isn't returned.
	void getParameters(
			unsigned int *piSampleRate,
			unsigned int *piSequenceMs,
			unsigned int *piSeekWindowMs,
			unsigned int *piOverlapMs);

	// Adds frames of samples into the input buffer.
	void putFrames(float **ppFrames, unsigned int iFrames);

	// Output frames from beginning of the sample buffer.
	// Copies requested frames output buffer and removes them
	// from the sample buffer. If there are less than frames()
	// samples in the buffer, returns all that available.
	unsigned int receiveFrames(float **ppFrames, unsigned int iFrames);

	// Returns number of frames currently available.
	unsigned int frames() const;

	// Flush any last samples that are hiding in the internal processing pipeline.
	void flushInput();

	// Clears the input buffer
	void clearInput();

	// Clears all buffers.
	void clear();

protected:

	// Calculates processing sequence length according to tempo setting.
	void calcSeekWindowLength();

	// Calculates overlap period length in frames.
	void calcOverlapLength();

	// Seeks for the optimal overlap-mixing position.
	unsigned int seekBestOverlapPosition();

	// Slopes the amplitude of the mid-buffer samples.
	void calcCrossCorrReference();

	// Clears mid sample frame buffer.
	void clearMidBuffer();

	// Changes the tempo of the given sound sample frames.
	// Returns amount of framees returned in the output buffer.
	void processFrames();

private:

	unsigned short m_iChannels;

	float m_fTempo;
	bool  m_bQuickSeek;

	unsigned int m_iSampleRate;
	unsigned int m_iSequenceMs;
	unsigned int m_iSeekWindowMs;
	unsigned int m_iOverlapMs;

	bool m_bAutoSequenceMs;
	bool m_bAutoSeekWindowMs;

	unsigned int m_iFramesReq;
	float **m_ppMidBuffer;
	float **m_ppRefMidBuffer;
	float **m_ppRefMidBufferUnaligned;
	float **m_ppFrames;
	unsigned int m_iOverlapLength;
	unsigned int m_iSeekLength;
	unsigned int m_iSeekWindowLength;
	float m_fNominalSkip;
	float m_fSkipFract;
	qtractorFifoBuffer<float> m_outputBuffer;
	qtractorFifoBuffer<float> m_inputBuffer;
	bool m_bMidBufferDirty;

	// Calculates the cross-correlation value over the overlap period.
	float (*m_pfnCrossCorr)(const float *, const float *, unsigned int);
};


#endif  // __qtractorTimeStretch_h


// end of qtractorTimeStretch.h
