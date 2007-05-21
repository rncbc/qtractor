// qtractorClip.cpp
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

#include "qtractorAbout.h"
#include "qtractorClip.h"

#include "qtractorSessionDocument.h"

#include <QFileInfo>
#include <QPainter>
#include <QPolygon>


//-------------------------------------------------------------------------
// qtractorClip -- Track clip capsule.

// Constructorz.
qtractorClip::qtractorClip ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;

	clear();
}


// Default constructor.
qtractorClip::~qtractorClip (void)
{
}


// Reset clip.
void qtractorClip::clear (void)
{
	m_sClipName       = QString::null;
	
	m_iClipStart      = 0;
	m_iClipLength     = 0;
	m_iClipOffset     = 0;

    m_iClipStartTime  = 0;
	m_iClipLengthTime = 0;

	m_iSelectStart    = 0;
	m_iSelectEnd      = 0;

	m_iLoopStart      = 0;
	m_iLoopEnd        = 0;

	m_iFadeInLength   = 0;
	m_iFadeOutLength  = 0;
#if 0
	// This seems a need trade-off between speed and effect.
	if (m_pTrack && m_pTrack->trackType() == qtractorTrack::Audio) {
		m_fadeIn.fadeType  = Quadratic;
		m_fadeOut.fadeType = Quadratic;
	}
#endif
}


// Track accessors.
void qtractorClip::setTrack ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;
}

qtractorTrack *qtractorClip::track (void) const
{
	return m_pTrack;
}


// Clip filename properties accessors.
void qtractorClip::setFilename ( const QString& sFilename )
{
	m_sFilename = sFilename;
	
	if (clipName().isEmpty())
		setClipName(QFileInfo(m_sFilename).baseName());
}

const QString& qtractorClip::filename (void) const
{
	return m_sFilename;
}


// Clip label accessors.
const QString& qtractorClip::clipName (void) const
{
	return m_sClipName;
}

void qtractorClip::setClipName ( const QString& sClipName )
{
	m_sClipName = sClipName;
}


// Clip start frame accessors.
unsigned long qtractorClip::clipStart (void) const
{
	return m_iClipStart;
}

void qtractorClip::setClipStart ( unsigned long iClipStart )
{
	m_iClipStart = iClipStart;

	if (m_pTrack && m_pTrack->session())
		m_iClipStartTime = m_pTrack->session()->tickFromFrame(iClipStart);
}


// Clip frame length accessors.
unsigned long qtractorClip::clipLength (void) const
{
	return m_iClipLength;
}

void qtractorClip::setClipLength ( unsigned long iClipLength )
{
	m_iClipLength = iClipLength;

	if (m_pTrack && m_pTrack->session())
		m_iClipLengthTime = m_pTrack->session()->tickFromFrame(iClipLength);
}


// Clip frame offset accessors.
unsigned long qtractorClip::clipOffset (void) const
{
	return m_iClipOffset;
}

void qtractorClip::setClipOffset ( unsigned long iClipOffset )
{
	m_iClipOffset = iClipOffset;
}


// Clip selection accessors.
void qtractorClip::setClipSelected ( bool bClipSelected )
{
	if (bClipSelected) {
		setClipSelect(m_iClipStart, m_iClipStart + m_iClipLength);
	} else {
		setClipSelect(0, 0);
	}
}

bool qtractorClip::isClipSelected (void) const
{
	return (m_iSelectStart < m_iSelectEnd);
}


// Clip-selection points accessors.
void qtractorClip::setClipSelect ( unsigned long iSelectStart,
	unsigned long iSelectEnd )
{
	if (iSelectStart < m_iClipStart)
		iSelectStart = m_iClipStart;
	if (iSelectEnd > m_iClipStart + m_iClipLength)
		iSelectEnd = m_iClipStart + m_iClipLength;

	if (iSelectStart < iSelectEnd) {
		m_iSelectStart = iSelectStart;
		m_iSelectEnd   = iSelectEnd;
	} else {
		m_iSelectStart = 0;
		m_iSelectEnd   = 0;
	}
}

unsigned long qtractorClip::clipSelectStart (void) const
{
	return m_iSelectStart;
}

unsigned long qtractorClip::clipSelectEnd (void) const
{
	return m_iSelectEnd;
}


// Clip-loop points accessors.
void qtractorClip::setClipLoop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
	if (iLoopStart < iLoopEnd) {
		m_iLoopStart = iLoopStart;
		m_iLoopEnd   = iLoopEnd;
	} else {
		m_iLoopStart = 0;
		m_iLoopEnd   = 0;
	}

	set_loop(m_iLoopStart, m_iLoopEnd);
}

unsigned long qtractorClip::clipLoopStart (void) const
{
	return m_iLoopStart;
}

unsigned long qtractorClip::clipLoopEnd (void) const
{
	return m_iLoopEnd;
}


// Clip fade-in type accessors
void qtractorClip::setFadeInType( qtractorClip::FadeType fadeType )
{
	m_fadeIn.fadeType = fadeType;
}

qtractorClip::FadeType qtractorClip::fadeInType (void) const
{
	return m_fadeIn.fadeType;
}


// Clip fade-in length accessors
unsigned long qtractorClip::fadeInLength (void) const
{
	return m_iFadeInLength;
}

void qtractorClip::setFadeInLength ( unsigned long iFadeInLength )
{
	if (iFadeInLength > m_iClipLength)
		iFadeInLength = m_iClipLength;
	
	if (iFadeInLength > 0) {
		float a = 1.0f / float(iFadeInLength);
		float b = 0.0f;
		m_fadeIn.setFadeCoeffs(a, b);
	}

	m_iFadeInLength = iFadeInLength;
}


// Clip fade-out type accessors
void qtractorClip::setFadeOutType( qtractorClip::FadeType fadeType )
{
	m_fadeOut.fadeType = fadeType;
}

qtractorClip::FadeType qtractorClip::fadeOutType (void) const
{
	return m_fadeOut.fadeType;
}


// Clip fade-out length accessors
unsigned long qtractorClip::fadeOutLength (void) const
{
	return m_iFadeOutLength;
}

void qtractorClip::setFadeOutLength ( unsigned long iFadeOutLength )
{
	if (iFadeOutLength > m_iClipLength)
		iFadeOutLength = m_iClipLength;

	if (iFadeOutLength > 0) {
		float a = -1.0f / float(iFadeOutLength);
		float b = float(m_iClipLength) / float(iFadeOutLength);
		m_fadeOut.setFadeCoeffs(a, b);
	}

	m_iFadeOutLength = iFadeOutLength;
}


// Fade in/ou interpolation coefficients settler.
void qtractorClip::FadeMode::setFadeCoeffs ( float a, float b )
{
	switch (fadeType) {
	case Linear:
		c1 = a;
		c0 = b;
		break;
	case Quadratic:
		c2 = a * a;
		c1 = 2.0f * a * b;
		c0 = b * b;
		break;
	case Cubic:
		float a2 = a * a;
		float b2 = b * b;
		c3 = a * a2;
		c2 = 3.0f * a2 * b;
		c1 = 3.0f * a * b2;
		c0 = b * b2;
		break;
	}
}

// Compute clip gain, given current fade-in/out slopes.
float qtractorClip::gain ( unsigned long iOffset ) const
{ 
	float fGain = 1.0f;

	if (m_iFadeInLength > 0 && iOffset < m_iFadeInLength) {
		float f  = float(iOffset);
		float f2 = f * f;
		switch (m_fadeIn.fadeType) {
		case Linear:
			fGain *= m_fadeIn.c1 * f + m_fadeIn.c0;
			break;
		case Quadratic:
			fGain *= m_fadeIn.c2 * f2 + m_fadeIn.c1 * f + m_fadeIn.c0;
			break;
		case Cubic:
			fGain *= m_fadeIn.c3 * f2 * f + m_fadeIn.c2 * f2
				+ m_fadeIn.c1 * f + m_fadeIn.c0;
			break;
		}
	}

	if (m_iFadeOutLength > 0 && iOffset > m_iClipLength - m_iFadeOutLength) {
		float f  = float(iOffset);
		float f2 = f * f;
		switch (m_fadeOut.fadeType) {
		case Linear:
			fGain *= m_fadeOut.c1 * f + m_fadeOut.c0;
			break;
		case Quadratic:
			fGain *= m_fadeOut.c2 * f2 + m_fadeOut.c1 * f + m_fadeOut.c0;
			break;
		case Cubic:
			fGain *= m_fadeOut.c3 * f2 * f + m_fadeOut.c2 * f2
				+ m_fadeOut.c1 * f + m_fadeOut.c0;
			break;
		}
	}

	return fGain;
}


// Clip time reference settler method.
void qtractorClip::updateClipTime (void)
{
	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	m_iClipStart  = pSession->frameFromTick(m_iClipStartTime);
	m_iClipLength = pSession->frameFromTick(m_iClipLengthTime);
}


// Base clip drawing method.
void qtractorClip::drawClip ( QPainter *pPainter, const QRect& clipRect,
	unsigned long iClipOffset )
{
	// Fill clip background...
	pPainter->setPen(m_pTrack->background().dark());
	pPainter->setBrush(m_pTrack->background());
	pPainter->drawRect(clipRect);

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	// Adjust the clip rectangle left origin... 
	QRect rect(clipRect);
	if (iClipOffset > 0)
		rect.setLeft(rect.left() - pSession->pixelFromFrame(iClipOffset) + 1);
	rect.setRight(rect.left() + pSession->pixelFromFrame(m_iClipLength));

	// Draw clip name label...
	pPainter->drawText(rect,
		Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine, clipName());

	// Fade in/out handle color...
	const QColor& rgbFade = track()->foreground().light(160);
	pPainter->setPen(rgbFade);
	pPainter->setBrush(rgbFade);

	// Fade-in slope...
	int y = rect.top()  + 1;
	int x = rect.left() + 1;
	int w = pSession->pixelFromFrame(m_iFadeInLength);
	QRect rectFadeIn(x + w, y, 8, 8);
	if (w > 0 && x + w > clipRect.left()) {
		QPolygon polyg(3);
		polyg.setPoint(0, x, y);
		polyg.setPoint(1, x, rect.bottom() - 1);
		polyg.setPoint(2, x + w, y);
		pPainter->drawPolygon(polyg);
	}

	// Fade-out slope...
	x = rect.right() - 1;
	w = pSession->pixelFromFrame(m_iFadeOutLength);
	QRect rectFadeOut(x - w - 8, y, 8, 8);
	if (w > 0 && x - w < clipRect.right()) {
		QPolygon polyg(3);
		polyg.setPoint(0, x, y);
		polyg.setPoint(1, x, rect.bottom() - 1);
		polyg.setPoint(2, x - w, y);
		pPainter->drawPolygon(polyg);
	}

	// Fade in/out handles...
	if (rectFadeIn.intersects(clipRect))
		pPainter->fillRect(rectFadeIn, rgbFade.dark(120));
	if (rectFadeOut.intersects(clipRect))
		pPainter->fillRect(rectFadeOut, rgbFade.dark(120));
}


// Document element methods.
bool qtractorClip::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	qtractorClip::setClipName(pElement->attribute("name"));

	// Load clip children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Load clip properties...
		if (eChild.tagName() == "properties") {
			for (QDomNode nProp = eChild.firstChild();
					!nProp.isNull();
						nProp = nProp.nextSibling()) {
				// Convert property node to element...
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				if (eProp.tagName() == "name")
					qtractorClip::setClipName(eProp.text());
				else if (eProp.tagName() == "start")
					qtractorClip::setClipStart(eProp.text().toULong());
				else if (eProp.tagName() == "offset")
					qtractorClip::setClipOffset(eProp.text().toULong());
				else if (eProp.tagName() == "length")
					qtractorClip::setClipLength(eProp.text().toULong());
				else if (eProp.tagName() == "fade-in")
					qtractorClip::setFadeInLength(eProp.text().toULong());
				else if (eProp.tagName() == "fade-out")
					qtractorClip::setFadeOutLength(eProp.text().toULong());
			}
		}
		else
		// Load clip derivative properties...
		if (eChild.tagName() == "audio-clip" ||
			eChild.tagName() == "midi-clip") {
			if (!loadClipElement(pDocument, &eChild))
				return false;
		}
	}

	return true;
}


bool qtractorClip::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	pElement->setAttribute("name", qtractorClip::clipName());

	// Save clip properties...
	QDomElement eProps = pDocument->document()->createElement("properties");
	pDocument->saveTextElement("name", qtractorClip::clipName(), &eProps);
	pDocument->saveTextElement("start",
		QString::number(qtractorClip::clipStart()), &eProps);
	pDocument->saveTextElement("offset",
		QString::number(qtractorClip::clipOffset()), &eProps);
	pDocument->saveTextElement("length",
		QString::number(qtractorClip::clipLength()), &eProps);
	pDocument->saveTextElement("fade-in",
		QString::number(qtractorClip::fadeInLength()), &eProps);
	pDocument->saveTextElement("fade-out",
		QString::number(qtractorClip::fadeOutLength()), &eProps);
	pElement->appendChild(eProps);

	// Save clip derivative properties...
	return saveClipElement(pDocument, pElement);
}


// end of qtractorClip.h
