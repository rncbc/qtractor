// qtractorClip.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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
class qtractorClipCommand;

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
	void setTrack(qtractorTrack *pTrack)
		{ m_pTrack = pTrack; }
	qtractorTrack *track() const
		{ return m_pTrack; }

	// Filename properties accessors.
	void setFilename(const QString&  sFilename);
	const QString& filename() const;

	QString relativeFilename(qtractorDocument *pDocument) const;

	// Clip label accessors.
	void setClipName(const QString& sClipName)
		{ m_sClipName = sClipName; }
	const QString& clipName() const
		{ return m_sClipName; }

	QString shortClipName(const QString& sClipName) const;

	QString clipTitle() const;

	// Clip start frame accessors.
	void setClipStart0(unsigned long iClipStart0);
	unsigned long clipStart0() const
		{ return m_iClipStart0; }
	void setClipStart(unsigned long iClipStart);
	unsigned long clipStart() const
		{ return m_iClipStart; }
	unsigned long clipStartTime() const
		{ return m_iClipStartTime; }

	// Clip frame length accessors.
	void setClipLength0(unsigned long iClipLength0);
	unsigned long clipLength0() const
		{ return m_iClipLength0; }
	void setClipLength(unsigned long iClipLength);
	unsigned long clipLength() const
		{ return m_iClipLength; }
	unsigned long clipLengthTime() const
		{ return m_iClipLengthTime; }

	// Clip offset frame accessors.
	void setClipOffset0(unsigned long iClipOffset0);
	unsigned long clipOffset0() const
		{ return m_iClipOffset0; }
	void setClipOffset(unsigned long iClipOffset);
	unsigned long clipOffset() const
		{ return m_iClipOffset; }
	unsigned long clipOffsetTime() const
		{ return m_iClipOffsetTime; }

	// Clip selection accessors.
	void setClipSelected(bool bClipSelected);
	bool isClipSelected() const;

	void setClipSelect(unsigned long iSelectStart, unsigned long iSelectEnd);
	unsigned long clipSelectStart() const
		{ return m_iSelectStart; }
	unsigned long clipSelectEnd() const
		{ return m_iSelectEnd; }

	// Clip gain/panning accessors.
	void setClipGain(float fGain)
		{ m_fGain = fGain; }
	float clipGain() const
		{ return m_fGain; }

	void setClipPanning(float fPanning)
		{ m_fPanning = fPanning; }
	float clipPanning() const
		{ return m_fPanning; }

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
	FadeType fadeInType() const
		{ return m_fadeInType; }

	void setFadeInLength(unsigned long iFadeInLength);
	unsigned long fadeInLength() const
		{ return m_iFadeInLength; }

	// Clip fade-out accessors
	void setFadeOutType(FadeType fadeType);
	FadeType fadeOutType() const
		{ return m_fadeOutType; }

	void setFadeOutLength(unsigned long iFadeOutLength);
	unsigned long fadeOutLength() const
		{ return m_iFadeOutLength; }

	// Compute clip gain, given current fade-in/out slopes.
	float fadeInOutGain(unsigned long iOffset) const;

	// Clip time reference settler method.
	void updateClipTime(long iFramesDiff = 0);

	// Clip paint methods.
	void drawClip(QPainter *pPainter,
		const QRect& clipRect, unsigned long iClipOffset);
	void drawClipRecord(QPainter *pPainter,
		const QRect& clipRect, unsigned long iClipOffset);

	// Clip (re)open method.
	virtual void open() = 0;

	// Intra-clip frame positioning.
	virtual void seek(unsigned long iFrame) = 0;

	// Reset clip state position.
	virtual void reset(bool bLooping) = 0;

	// Clip loop-point methods.
	virtual void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd) = 0;

	// Clip close-commit (record specific)
	virtual void close() = 0;

	// Clip special process cycle executive.
	virtual void process(
		unsigned long iFrameStart, unsigned long iFrameEnd) = 0;

	// Clip freewheeling process cycle executive (needed for export).
	virtual void process_export(
		unsigned long iFrameStart, unsigned long iFrameEnd) = 0;

	// Clip paint method.
	virtual void draw(QPainter *pPainter,
		const QRect& clipRect, unsigned long iClipOffset) = 0;

	// Clip update method.
	virtual void update() = 0;

	// Clip editor methods.
	virtual bool startEditor(QWidget *pParent = NULL);
	virtual void updateEditor(bool bSelectClear);
	virtual void updateEditorContents();
	virtual bool queryEditor();

	// Clip tool-tip.
	virtual QString toolTip() const;

	// Local dirty flag.
	void setDirty(bool bDirty)
		{ m_bDirty = bDirty; }
	bool isDirty() const
		{ return m_bDirty; }

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement);

	// Clip fade type textual helper methods.
	static FadeType fadeInTypeFromText(const QString& sText);
	static FadeType fadeOutTypeFromText(const QString& sText);
	static QString textFromFadeType(FadeType fadeType);

	// Take(record) descriptor.
	//
	class TakeInfo
	{
	public:

		// Constructor.
		TakeInfo(
			unsigned long iClipStart,
			unsigned long iClipOffset,
			unsigned long iClipLength,
			unsigned long iTakeStart,
			unsigned long iTakeEnd,
			unsigned long iTakeGap)
		:	m_iClipStart(iClipStart),
			m_iClipOffset(iClipOffset),
			m_iClipLength(iClipLength),
			m_iTakeStart(iTakeStart),
			m_iTakeEnd(iTakeEnd),
			m_iTakeGap(iTakeGap),
			m_iCurrentTake(-1),
			m_iRefCount(0)
			{ m_apClipParts[ClipHead] = NULL; m_apClipParts[ClipTake] = NULL; }

		// Brainless accessors (maybe useless).
		unsigned long clipStart() const
			{ return m_iClipStart; }
		unsigned long clipOffset() const
			{ return m_iClipOffset; }
		unsigned long clipLength() const
			{ return m_iClipLength; }

		unsigned long takeStart() const
			{ return m_iTakeStart; }
		unsigned long takeEnd() const
			{ return m_iTakeEnd; }
		unsigned long takeGap() const
			{ return m_iTakeGap; }

		void setCurrentTake(int iCurrentTake)
			{ m_iCurrentTake = iCurrentTake; }
		int currentTake() const
			{ return m_iCurrentTake; }

		// Estimate number of takes.
		int takeCount() const;

		// Select current take set.
		int select(qtractorClipCommand *pClipCommand,
			qtractorTrack *pTrack, int iTake = -1);

		// Reset(unfold) whole take set.
		void reset(qtractorClipCommand *pClipCommand, bool bClear = false);

		// Reference counting methods.
		void addRef() { ++m_iRefCount; }
		void releaseRef() { if (--m_iRefCount < 1) delete this; }

		// Sub-clip take parts.
		enum ClipPart { ClipHead = 0, ClipTake = 1, ClipParts };

		void setClipPart(ClipPart cpart, qtractorClip *pClip)
			{ m_apClipParts[cpart] = pClip; }
		qtractorClip *clipPart(ClipPart cpart) const
			{ return m_apClipParts[cpart]; }
		ClipPart partClip(const qtractorClip *pClip) const
			{ return (m_apClipParts[ClipHead] == pClip ? ClipHead : ClipTake); }

	protected:

		// Sub-brainfull method.
		void selectClipPart(qtractorClipCommand *pClipCommand,
			qtractorTrack *pTrack, ClipPart cpart, unsigned long iClipStart,
			unsigned long iClipOffset, unsigned long iClipLength);

	private:

		// Instance variables.
		unsigned long m_iClipStart;
		unsigned long m_iClipOffset;
		unsigned long m_iClipLength;

		unsigned long m_iTakeStart;
		unsigned long m_iTakeEnd;
		unsigned long m_iTakeGap;

		int m_iCurrentTake;
		int m_iRefCount;

		qtractorClip *m_apClipParts[ClipParts];
	};

	// Take(record) descriptor accessors.
	void setTakeInfo(TakeInfo *pTakeInfo);
	TakeInfo *takeInfo() const;

	// Take(record) part clip-descriptor.
	//
	class TakePart
	{
	public:

		// Constructor.
		TakePart(TakeInfo *pTakeInfo, TakeInfo::ClipPart cpart)
			: m_pTakeInfo(pTakeInfo), m_cpart(cpart) {}

		// Direct accessors.
		TakeInfo *takeInfo() const
			{ return m_pTakeInfo; }
		TakeInfo::ClipPart cpart() const
			{ return m_cpart; }

		// Delegated accessors.
		void setClip(qtractorClip *pClip)
			{ m_pTakeInfo->setClipPart(m_cpart, pClip); }
		qtractorClip *clip() const
			{ return m_pTakeInfo->clipPart(m_cpart); }
		
	private:

		// Member variables.
		TakeInfo *m_pTakeInfo;
		TakeInfo::ClipPart m_cpart;
	};

	// Fade functor (pure abstract) class.
	//
	class FadeFunctor
	{
	public:

		virtual ~FadeFunctor() {}
		virtual float operator() (float t) const = 0;
	};

protected:

	// Fade functor factory method.
	//
	static FadeFunctor *createFadeFunctor(
		FadeMode fadeMode, FadeType fadeType);

	// Virtual document element methods.
	virtual bool loadClipElement(
		qtractorDocument *pDocument, QDomElement *pElement) = 0;
	virtual bool saveClipElement(
		qtractorDocument *pDocument, QDomElement *pElement) = 0;

private:

	qtractorTrack *m_pTrack;            // Track reference.

	QString       m_sFilename;          // Clip filename (complete path).
	QString       m_sClipName;          // Clip label.

	unsigned long m_iClipStart;         // Clip frame start.
	unsigned long m_iClipLength;        // Clip frame length.
	unsigned long m_iClipOffset;        // Clip frame offset.
	unsigned long m_iClipStart0;
	unsigned long m_iClipOffset0;
	unsigned long m_iClipLength0;

	unsigned long m_iClipStartTime;     // Clip time (tick) start.
	unsigned long m_iClipLengthTime;    // Clip time (tick) length.
	unsigned long m_iClipOffsetTime;    // Clip time (tick) offset.

	unsigned long m_iSelectStart;       // Clip loop start frame-offset.
	unsigned long m_iSelectEnd;         // Clip loop end frame-offset.

	float m_fGain;                      // Clip gain/volume;
	float m_fPanning;                   // Clip panning;

	// Take(record) descriptor.
	TakeInfo *m_pTakeInfo;

	// Fade-in/out stuff.
	unsigned long m_iFadeInLength;      // Fade-in length (in frames).
	unsigned long m_iFadeOutLength;     // Fade-out length (in frames).

	unsigned long m_iFadeInTime;        // Fade-in length (in ticks).
	unsigned long m_iFadeOutTime;       // Fade-out length (in ticks).

	FadeType m_fadeInType;              // Fade-in curve type.
	FadeType m_fadeOutType;             // Fade-out curve type.

	// Aproximations to exponential fade interpolation.
	FadeFunctor *m_pFadeInFunctor;
	FadeFunctor *m_pFadeOutFunctor;

	// Local dirty flag.
	bool m_bDirty;
};


// Stub declarations.
class qtractorTrack::TakeInfo : public qtractorClip::TakeInfo {};


#endif  // __qtractorClip_h


// end of qtractorClip.h
