// qtractorClip.h
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

#ifndef __qtractorClip_h
#define __qtractorClip_h

#include "qtractorTrack.h"


// Forward declarations.
class QWidget;


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

	// Filename properties accessors.
	void setFilename(const QString&  sFilename);
	const QString& filename() const;

	// Clip label accessors.
	void setClipName(const QString& sClipName);
	const QString& clipName() const;

	// Clip start frame accessors.
	void setClipStart(unsigned long iClipStart);
	unsigned long clipStart() const;

	// Clip frame length accessors.
	void setClipLength(unsigned long iClipLength);
	unsigned long clipLength() const;

	// Clip offset frame accessors.
	void setClipOffset(unsigned long iClipOffset);
	unsigned long clipOffset() const;

	// Clip selection accessors.
	void setClipSelected(bool bClipSelected);
	bool isClipSelected() const;

	void setClipSelect(unsigned long iSelectStart, unsigned long iSelectEnd);
	unsigned long clipSelectStart() const;
	unsigned long clipSelectEnd() const;

	// Clip loop point accessors.
	void setClipLoop(unsigned long iLoopStart, unsigned long iLoopEnd);
	unsigned long clipLoopStart() const;
	unsigned long clipLoopEnd() const;

	// Fade types.
	enum FadeType { Linear, Quadratic, Cubic };

	// Clip fade-in accessors
	void setFadeInType(FadeType fadeType);
	FadeType fadeInType() const;

	void setFadeInLength(unsigned long iFadeInLength);
	unsigned long fadeInLength() const;
	
	// Clip fade-out accessors
	void setFadeOutType(FadeType fadeType);
	FadeType fadeOutType() const;

	void setFadeOutLength(unsigned long iFadeOutLength);
	unsigned long fadeOutLength() const;

	// Clip time reference settler method.
	void updateClipTime();

	// Clip (re)open method.
	virtual void open() = 0;

	// Intra-clip frame positioning.
	virtual void seek(unsigned long iFrame) = 0;

	// Reset clip state position.
	virtual void reset(bool bLooping) = 0;

	// Clip loop-point methods.
	virtual void set_loop(unsigned long iLoopStart, unsigned long iLoopEnd) = 0;

	// Clip close-commit (record specific)
	virtual void close(bool bForce) = 0;

	// Clip special process cycle executive.
	virtual void process(unsigned long iFrameStart, unsigned long iFrameEnd) = 0;

	// Clip paint method.
	virtual void drawClip(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset);

	// Clip editor method.
	virtual bool startEditor(QWidget *pParent);
	virtual bool queryEditor();
	virtual void updateEditor();

	// Clip tool-tip.
	virtual QString toolTip() const;

	// Local dirty flag.
	void setDirty(bool bDirty);
	bool isDirty() const;

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

protected:

	// Compute clip gain, given current fade-in/out slopes.
	float gain(unsigned long iOffset) const;

	// Virtual document element methods.
	virtual bool loadClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;
	virtual bool saveClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;

private:

	qtractorTrack *m_pTrack;        // Track reference.

	QString       m_sFilename;      // Clip filename (complete path).
	QString       m_sClipName;      // Clip label.

	unsigned long m_iClipStart;     // Clip frame start.
	unsigned long m_iClipLength;    // Clip frame length.
	unsigned long m_iClipOffset;    // Clip frame offset.

	unsigned long m_iClipStartTime; // Clip time (tick) start.
	unsigned long m_iClipLengthTime;// Clip time (tick) length.

	unsigned long m_iSelectStart;   // Clip loop start frame-offset.
	unsigned long m_iSelectEnd;     // Clip loop end frame-offset.

	unsigned long m_iLoopStart;     // Clip loop start frame-offset.
	unsigned long m_iLoopEnd;       // Clip loop end frame-offset.

	// Fade-in/out stuff.
	unsigned long m_iFadeInLength;  // Fade-in length (in frames).
	unsigned long m_iFadeOutLength; // Fade-out length (in frames).

	// Aproximations to exponential fade interpolation.
	struct FadeMode
	{
		// Constructor.
		FadeMode() : fadeType(Quadratic),
			c3(0.0f), c2(0.0f), c1(0.0f), c0(0.0f) {}
		// Interpolation coefficients settler.
		void setFadeCoeffs(float a, float b);
		// Fade-type descriminator.
		FadeType fadeType;
		// Fade coefficient members.
		float c3, c2, c1, c0;
	};

	FadeMode m_fadeIn;
	FadeMode m_fadeOut;

	// Local dirty flag.
	bool m_bDirty;
};

#endif  // __qtractorClip_h


// end of qtractorClip.h
