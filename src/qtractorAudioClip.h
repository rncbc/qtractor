// qtractorAudioClip.h
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioClip_h
#define __qtractorAudioClip_h

#include "qtractorClip.h"
#include "qtractorAudioFile.h"

// Forward declarations.
class qtractorAudioBuffer;
class qtractorAudioPeak;


//----------------------------------------------------------------------
// class qtractorAudioClip -- Audio file/buffer clip.
//

class qtractorAudioClip : public qtractorClip
{
public:

	// Constructor.
	qtractorAudioClip(qtractorTrack *pTrack);
	// Copy constructor.
	qtractorAudioClip(const qtractorAudioClip& clip);

	// Destructor.
	~qtractorAudioClip();

	// Time-stretching.
	void setTimeStretch(float fTimeStretch);
	float timeStretch() const;

	// Pitch-shifting.
	void setPitchShift(float fPitchShift);
	float pitchShift() const;

	// Clip (re)open method.
	void open();

	// The main use method.
	bool openAudioFile(const QString& sFilename, int iMode = qtractorAudioFile::Read);

	// Direct write method.
	void write(float **ppBuffer, unsigned int iFrames,
		unsigned short iChannels = 0, unsigned int iOffset = 0);

	// Export-mode sync method.
	void syncExport();

	// Intra-clip frame positioning.
	void seek(unsigned long iFrame);

	// Reset clip state.
	void reset(bool bLooping);

	// Loop positioning.
	void set_loop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Clip close-commit (record specific)
	void close(bool bForce);

	// Audio clip special process cycle executive.
	void process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void drawClip(QPainter *pPainter, const QRect& rect,
	    unsigned long iClipOffset);

	// Audio clip tool-tip.
	QString toolTip() const;

protected:

	// Virtual document element methods.
	bool loadClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

private:

	// Instance variables.
	qtractorAudioBuffer *m_pBuff;
	qtractorAudioPeak   *m_pPeak;

	float m_fTimeStretch;
	float m_fPitchShift;
};


#endif  // __qtractorAudioClip_h


// end of qtractorAudioClip.h
