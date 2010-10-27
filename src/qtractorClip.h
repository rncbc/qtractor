// qtractorClip.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

	QString relativeFilename(qtractorSessionDocument *pDocument) const;

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

	// Clip gain/volume accessors.
	void setClipGain(float fGain);
	float clipGain() const;

	// Fade modes.
	enum FadeMode {
		FadeIn = 0,
		FadeOut
	};

	// Fade types.
	enum FadeType {
		Linear = 0,
		InQuad,
		OutQuad,
		InOutQuad,
		InCubic,
		OutCubic,
		InOutCubic
	};

	// Clip fade-in type accessors
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

	// Clip paint method.
	void drawClip(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset);

	// Clip (re)open method.
	virtual void open() = 0;

	// Intra-clip frame positioning.
	virtual void seek(unsigned long iFrame) = 0;

	// Reset clip state position.
	virtual void reset(bool bLooping) = 0;

	// Clip loop-point methods.
	virtual void set_loop(unsigned long iLoopStart, unsigned long iLoopEnd) = 0;

	// Clip close-commit (record specific)
	virtual void close() = 0;

	// Clip special process cycle executive.
	virtual void process(unsigned long iFrameStart, unsigned long iFrameEnd) = 0;

	// Clip paint method.
	virtual void draw(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset) = 0;

	// Clip editor method.
	virtual bool startEditor(QWidget *pParent);
	virtual void resetEditor(bool bSelectClear);
	virtual void updateEditor(bool bSelectClear);
	virtual bool queryEditor();

	// Clip tool-tip.
	virtual QString toolTip() const;

	// Local dirty flag.
	void setDirty(bool bDirty);
	bool isDirty() const;

	// Compute clip gain, given current fade-in/out slopes.
	float gain(unsigned long iOffset) const;

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

	// Clip fade type textual helper methods.
	static FadeType fadeInTypeFromText(const QString& sText);
	static FadeType fadeOutTypeFromText(const QString& sText);
	static QString textFromFadeType(FadeType fadeType);

protected:

	// Fade functor (pure abstract) class.
	//
	class FadeFunctor
	{
	public:

		virtual float operator() (float t) const = 0;
	};

	// Fade functor factory method.
	//
	static FadeFunctor *createFadeFunctor(
		FadeMode fadeMode, FadeType fadeType);

	// Virtual document element methods.
	virtual bool loadClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;
	virtual bool saveClipElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;

	// Gain fractionalizer(tm)...
	struct { int num, den; } m_fractGain;

private:

	qtractorTrack *m_pTrack;        // Track reference.

	QString       m_sFilename;      // Clip filename (complete path).
	QString       m_sClipName;      // Clip label.

	unsigned long m_iClipStart;     // Clip frame start.
	unsigned long m_iClipLength;    // Clip frame length.
	unsigned long m_iClipOffset;    // Clip frame offset.

	unsigned long m_iClipStartTime; // Clip time (tick) start.
	unsigned long m_iClipOffsetTime;// Clip time (tick) offset.
	unsigned long m_iClipLengthTime;// Clip time (tick) length.

	unsigned long m_iSelectStart;   // Clip loop start frame-offset.
	unsigned long m_iSelectEnd;     // Clip loop end frame-offset.

	unsigned long m_iLoopStart;     // Clip loop start frame-offset.
	unsigned long m_iLoopEnd;       // Clip loop end frame-offset.

	// Clip gain/volume.
	float m_fGain;

	// Fade-in/out stuff.
	unsigned long m_iFadeInLength;  // Fade-in length (in frames).
	unsigned long m_iFadeOutLength; // Fade-out length (in frames).

	unsigned long m_iFadeInTime;    // Fade-in length (in ticks).
	unsigned long m_iFadeOutTime;   // Fade-out length (in ticks).

	FadeType m_fadeInType;          // Fade-in curve type.
	FadeType m_fadeOutType;         // Fade-out curve type.

	// Aproximations to exponential fade interpolation.
	FadeFunctor *m_pFadeInFunctor;
	FadeFunctor *m_pFadeOutFunctor;

	// Local dirty flag.
	bool m_bDirty;
};

#endif  // __qtractorClip_h


// end of qtractorClip.h
