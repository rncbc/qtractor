// qtractorClip.h
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

#ifndef __qtractorClip_h
#define __qtractorClip_h

#include "qtractorTrack.h"


//-------------------------------------------------------------------------
// qtractorClip -- Track clip capsule.

class qtractorClip : public qtractorList<qtractorClip>::Link
{
public:

	// Constructor.
	qtractorClip(qtractorTrack *pTrack);
	// Default constructor.
	virtual ~qtractorClip();

	// Clear clip.
	void clear();

	// Track accessor.
	void setTrack(qtractorTrack *pTrack);
	qtractorTrack *track() const;

	// Clip label accessors.
	void setClipName(const QString& sClipName);
	const QString& clipName() const;

	// Clip start frame accessors.
	void setClipStart(unsigned long iClipStart);
	unsigned long clipStart() const;

	// Clip frame length accessors.
	void setClipLength(unsigned long iClipLength);
	unsigned long clipLength() const;

	// Clip selection accessors.
	void setClipSelected(bool bClipSelect);
	bool isClipSelected() const;

	// Clip loop point accessors.
	void setClipLoop(unsigned long iLoopStart, unsigned long iLoopEnd);
	unsigned long clipLoopStart() const;
	unsigned long clipLoopEnd() const;

	// Intra-clip frame positioning.
	virtual void seek(unsigned long iOffset) = 0;

	// Reset clip state.
	virtual void reset(bool bLooping) = 0;

	// Clip loop point accessors.
	virtual void loop(unsigned long iLoopStart, unsigned long iLoopEnd) = 0;

	// Clip close-commit (record specific)
	virtual void close() = 0;

	// Clip special process cycle executive.
	virtual void process(float fGain,
		unsigned long iFrameStart, unsigned long iFrameEnd) = 0;

	// Clip paint method.
	virtual void drawClip(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset) = 0;

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

protected:

	// Virtual document element methods.
	virtual bool loadClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;
	virtual bool saveClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;

private:

	qtractorTrack *m_pTrack;    	// Track reference.

	QString m_sClipName;            // Clip label.

	unsigned long m_iClipStart;     // Clip frame start.
	unsigned long m_iClipLength;    // Clip frame length.
	
	bool m_bClipSelect;             // Clip selection flag.

	unsigned long m_iLoopStart;     // Clip loop start frame-offset.
	unsigned long m_iLoopEnd;       // Clip loop end frame-offset.
};

#endif  // __qtractorClip_h


// end of qtractorClip.h
