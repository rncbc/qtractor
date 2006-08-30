// qtractorAudioClip.h
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

	// The main use method.
	bool open(const QString& sFilename, int iMode = qtractorAudioFile::Read);

	// Direct write method.
	void write(float **ppBuffer, unsigned int iFrames,
		unsigned short iChannels = 0);

	// Aduio file properties accessors.
	const QString& filename() const;

	// Intra-clip frame positioning.
	void seek(unsigned long iOffset);

	// Reset clip state.
	void reset(bool bLooping);

	// Implementation methods.
	void set_offset(unsigned long iOffset);
	void set_length(unsigned long iLength);

	// Loop positioning.
	void set_loop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Clip close-commit (record specific)
	void close();

	// Audio clip special process cycle executive.
	void process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void drawClip(QPainter *pPainter, const QRect& rect,
	    unsigned long iClipOffset);

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

	QString m_sFilename;
};


#endif  // __qtractorAudioClip_h


// end of qtractorAudioClip.h
