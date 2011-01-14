// qtractorClip.cpp
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

#include "qtractorAbout.h"
#include "qtractorClip.h"

#include "qtractorSession.h"

#include "qtractorDocument.h"

#include "qtractorClipForm.h"

#include <QFileInfo>
#include <QPainter>
#include <QPolygon>
#include <QDir>

#include <QDomDocument>

#ifdef CONFIG_GRADIENT
#include <QLinearGradient>
#endif

#if QT_VERSION < 0x040300
#define lighter(x)	light(x)
#define darker(x)	dark(x)
#endif


//-------------------------------------------------------------------------
// qtractorClip -- Track clip capsule.

// Constructorz.
qtractorClip::qtractorClip ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;

	m_pFadeInFunctor  = NULL;
	m_pFadeOutFunctor = NULL;

	clear();
}


// Default constructor.
qtractorClip::~qtractorClip (void)
{
	if (m_pFadeInFunctor)
		delete m_pFadeInFunctor;
	if (m_pFadeOutFunctor)
		delete m_pFadeOutFunctor;
}


// Reset clip.
void qtractorClip::clear (void)
{
	m_sClipName.clear();

	m_iClipStart      = 0;
	m_iClipLength     = 0;
	m_iClipOffset     = 0;

	m_iClipStartTime  = 0;
	m_iClipOffsetTime = 0;
	m_iClipLengthTime = 0;

	m_iSelectStart    = 0;
	m_iSelectEnd      = 0;

	m_iLoopStart      = 0;
	m_iLoopEnd        = 0;

	m_fGain           = 1.0f;

	// Gain fractionalizer(tm)...
	m_fractGain.num   = 1;
	m_fractGain.den   = 8;

	m_iFadeInLength   = 0;
	m_iFadeOutLength  = 0;

	m_iFadeInTime     = 0;
	m_iFadeOutTime    = 0;

	setFadeInType(InQuad);
	setFadeOutType(OutQuad);

	m_bDirty = false;
}


// Clip filename properties accessors.
void qtractorClip::setFilename ( const QString& sFilename )
{
	QDir dir;

	if (m_pTrack && m_pTrack->session())
		dir.setPath(m_pTrack->session()->sessionDir());

	m_sFilename = QDir::cleanPath(dir.absoluteFilePath(sFilename));
}

const QString& qtractorClip::filename (void) const
{
	return m_sFilename;
}

QString qtractorClip::relativeFilename ( qtractorDocument *pDocument ) const
{
	if (pDocument && pDocument->isArchive())
		return pDocument->addFile(m_sFilename);

	QDir dir;

	if (m_pTrack && m_pTrack->session())
		dir.setPath(m_pTrack->session()->sessionDir());

	return dir.relativeFilePath(m_sFilename);
}


// Clip start frame accessor.
void qtractorClip::setClipStart ( unsigned long iClipStart )
{
	m_iClipStart = iClipStart;

	if (m_pTrack && m_pTrack->session())
		m_iClipStartTime = m_pTrack->session()->tickFromFrame(iClipStart);
}


// Clip frame length accessor.
void qtractorClip::setClipLength ( unsigned long iClipLength )
{
	m_iClipLength = iClipLength;

	if (m_pTrack && m_pTrack->session())
		m_iClipLengthTime = m_pTrack->session()->tickFromFrame(iClipLength);
}


// Clip frame offset accessor.
void qtractorClip::setClipOffset ( unsigned long iClipOffset )
{
	m_iClipOffset = iClipOffset;

	if (m_pTrack && m_pTrack->session())
		m_iClipOffsetTime = m_pTrack->session()->tickFromFrame(iClipOffset);
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


// Clip-loop points accessors.
void qtractorClip::setClipGain ( float fGain )
{
	m_fGain = fGain;

	// Gain fractionalizer(tm)...
	m_fractGain.den = 8;
	while(fGain != int(fGain) && m_fractGain.den < 20) {
		m_fractGain.den += 2;
		fGain *= 4.0f;
	}
	m_fractGain.num = int(fGain);
}


// Clip fade-in accessors
void qtractorClip::setFadeInType ( qtractorClip::FadeType fadeType )
{
	if (m_pFadeInFunctor)
		delete m_pFadeInFunctor;

	m_fadeInType = fadeType;
	m_pFadeInFunctor = createFadeFunctor(FadeIn, fadeType);
}


void qtractorClip::setFadeInLength ( unsigned long iFadeInLength )
{
	if (iFadeInLength > m_iClipLength)
		iFadeInLength = m_iClipLength;
	
	m_iFadeInLength = iFadeInLength;

	if (m_pTrack && m_pTrack->session())
		m_iFadeInTime = m_pTrack->session()->tickFromFrame(iFadeInLength);
}


// Clip fade-out accessors
void qtractorClip::setFadeOutType ( qtractorClip::FadeType fadeType )
{
	if (m_pFadeOutFunctor)
		delete m_pFadeOutFunctor;

	m_fadeOutType = fadeType;
	m_pFadeOutFunctor = createFadeFunctor(FadeOut, fadeType);
}


void qtractorClip::setFadeOutLength ( unsigned long iFadeOutLength )
{
	if (iFadeOutLength > m_iClipLength)
		iFadeOutLength = m_iClipLength;

	m_iFadeOutLength = iFadeOutLength;

	if (m_pTrack && m_pTrack->session())
		m_iFadeOutTime = m_pTrack->session()->tickFromFrame(iFadeOutLength);
}


// Compute clip gain, given current fade-in/out slopes.
float qtractorClip::gain ( unsigned long iOffset ) const
{
	if (iOffset >= m_iClipLength)
		return 0.0f;

	float fGain = m_fGain;

	if (m_iFadeInLength > 0 && iOffset < m_iFadeInLength) {
		fGain *= (*m_pFadeInFunctor)(
			float(iOffset) / float(m_iFadeInLength));
	}

	if (m_iFadeOutLength > 0 && iOffset > m_iClipLength - m_iFadeOutLength) {
		fGain *= (*m_pFadeOutFunctor)(
			float(iOffset - (m_iClipLength - m_iFadeOutLength))
				/ float(m_iFadeOutLength));
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
	m_iClipOffset = pSession->frameFromTick(m_iClipOffsetTime);
	m_iClipLength = pSession->frameFromTick(m_iClipLengthTime);

	m_iFadeInLength  = pSession->frameFromTick(m_iFadeInTime);
	m_iFadeOutLength = pSession->frameFromTick(m_iFadeOutTime);
}


// Base clip drawing method.
void qtractorClip::drawClip ( QPainter *pPainter, const QRect& clipRect,
	unsigned long iClipOffset )
{
	// Fill clip background...
	pPainter->setPen(m_pTrack->background().darker());
#ifdef CONFIG_GRADIENT
	QLinearGradient grad(0, clipRect.top(), 0, clipRect.bottom());
	grad.setColorAt(0.4, m_pTrack->background());
	grad.setColorAt(1.0, m_pTrack->background().darker(130));
	pPainter->setBrush(grad);
#else
	pPainter->setBrush(m_pTrack->background());
#endif
	pPainter->drawRect(clipRect);

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	// Adjust the clip rectangle left origin... 
	QRect rect(clipRect);
	if (iClipOffset > 0)
		rect.setLeft(rect.left() - pSession->pixelFromFrame(iClipOffset) + 1);

	// Draw clip name label...
	pPainter->drawText(rect,
		Qt::AlignLeft | Qt::AlignBottom | Qt::TextSingleLine, m_sClipName);

	// Draw clip contents (virtual)
	draw(pPainter, clipRect, iClipOffset);

	// Avoid drawing fade in/out handles
	// on still empty clips (eg. while recording)
	if (m_iClipLength < 1)
		return;

	// Fade in/out handle color...
	QColor rgbFade(m_pTrack->foreground());
	rgbFade.setAlpha(120);
	pPainter->setPen(rgbFade);
	pPainter->setBrush(rgbFade);

	// Fade-in slope...
	int y = rect.top();
	int x = rect.left();
	int w = pSession->pixelFromFrame(m_iFadeInLength);
	int h = rect.bottom();
	QRect rectFadeIn(x + w, y, 8, 8);
	if (w > 0 && x + w > clipRect.left()) {
	#if 0
		QPolygon polyg(3);
		polyg.setPoint(0, x, y);
		polyg.setPoint(1, x, h);
		polyg.setPoint(2, x + w, y);
		pPainter->drawPolygon(polyg);
	#else
		const int w2 = (w >> 1);
		const int w4 = (w >> 2);
		QPolygon polyg(5);
		polyg.setPoint(0, x, y);
		polyg.setPoint(1, x, h);
		switch (m_fadeInType) {
		case Linear:
			polyg.setPoint(2, x,          h);
			polyg.setPoint(3, x,          h);
			break;
		case InQuad:
			polyg.setPoint(2, x + w2,     h);
			polyg.setPoint(3, x + w,      y);
			break;
		case OutQuad:
			polyg.setPoint(2, x + w2,     y);
			polyg.setPoint(3, x + w,      y);
			break;
		case InOutQuad:
			polyg.setPoint(2, x + w2,     h);
			polyg.setPoint(3, x + w2,     y);
			break;
		case InCubic:
			polyg.setPoint(2, x + w - w4, h);
			polyg.setPoint(3, x + w,      y);
			break;
		case OutCubic:
			polyg.setPoint(2, x + w2,     y);
			polyg.setPoint(3, x + w - w4, y);
			break;
		case InOutCubic:
			polyg.setPoint(2, x + w - w4, h);
			polyg.setPoint(3, x + w4,     y);
			break;
		}
		polyg.setPoint(4, x + w, y);
		QPainterPath path;
		path.moveTo(polyg.at(0));
		path.lineTo(polyg.at(1));
		path.cubicTo(polyg.at(2), polyg.at(3), polyg.at(4));
		path.lineTo(polyg.at(0));
		pPainter->drawPath(path);
	#endif
	}

	// Fade-out slope...
	x = rect.left() + pSession->pixelFromFrame(m_iClipLength);
	w = pSession->pixelFromFrame(m_iFadeOutLength);
	QRect rectFadeOut(x - w - 8, y, 8, 8);
	if (w > 0 && x - w < clipRect.right()) {
	#if 0
		QPolygon polyg(3);
		polyg.setPoint(0, x, y);
		polyg.setPoint(1, x, h);
		polyg.setPoint(2, x - w, y);
		pPainter->drawPolygon(polyg);
	#else
		const int w2 = (w >> 1);
		const int w4 = (w >> 2);
		QPolygon polyg(5);
		polyg.setPoint(0, x, y);
		polyg.setPoint(1, x, h);
		switch (m_fadeOutType) {
		case Linear:
			polyg.setPoint(2, x,          h);
			polyg.setPoint(3, x,          h);
			break;
		case InQuad:
			polyg.setPoint(2, x - w2,     y);
			polyg.setPoint(3, x - w,      y);
			break;
		case OutQuad:
			polyg.setPoint(2, x - w2,     h);
			polyg.setPoint(3, x - w,      y);
			break;
		case InOutQuad:
			polyg.setPoint(2, x - w2,     h);
			polyg.setPoint(3, x - w2,     y);
			break;
		case InCubic:
			polyg.setPoint(2, x - w2,     y);
			polyg.setPoint(3, x - w + w4, y);
			break;
		case OutCubic:
			polyg.setPoint(2, x - w + w4, h);
			polyg.setPoint(3, x - w,      y);
			break;
		case InOutCubic:
			polyg.setPoint(2, x - w + w4, h);
			polyg.setPoint(3, x - w4,     y);
			break;
		}
		polyg.setPoint(4, x - w, y);
		QPainterPath path;
		path.moveTo(polyg.at(0));
		path.lineTo(polyg.at(1));
		path.cubicTo(polyg.at(2), polyg.at(3), polyg.at(4));
		path.lineTo(polyg.at(0));
		pPainter->drawPath(path);
	#endif
	}

	// Fade in/out handles...
	if (rectFadeIn.intersects(clipRect))
		pPainter->fillRect(rectFadeIn, rgbFade.darker(120));
	if (rectFadeOut.intersects(clipRect))
		pPainter->fillRect(rectFadeOut, rgbFade.darker(120));
}


// Clip editor method.
bool qtractorClip::startEditor ( QWidget *pParent )
{
	// Make sure the regular clip-form is started modal...
	qtractorClipForm clipForm(pParent);
	clipForm.setClip(this);
	return clipForm.exec();
}

// Clip editor reset.
void qtractorClip::resetEditor ( bool /*bSelectClear*/ )
{
	// Do nothing here.
}

// Clip editor update.
void qtractorClip::updateEditor ( bool /*bSelectClear*/ )
{
	// Do nothing here.
}

// Clip query-close method (return true if editing is done).
bool qtractorClip::queryEditor (void)
{
	return !isDirty();
}


// Clip tool-tip.
QString qtractorClip::toolTip (void) const
{
	QString sToolTip = QObject::tr("Name:\t%1").arg(m_sClipName);

	qtractorSession *pSession = NULL;
	if (m_pTrack)
		pSession = m_pTrack->session();
	if (pSession) {
		sToolTip += '\n';
		qtractorTimeScale *pTimeScale = pSession->timeScale();
		sToolTip += QObject::tr("Start / End:\t%1 / %2\nOffs. / Length:\t%3 / %4")
			.arg(pTimeScale->textFromFrame(m_iClipStart))
			.arg(pTimeScale->textFromFrame(m_iClipStart + m_iClipLength))
			.arg(pTimeScale->textFromFrame(m_iClipStart, true, m_iClipOffset))
			.arg(pTimeScale->textFromFrame(m_iClipStart, true, m_iClipLength));
	}

	sToolTip += '\n';
	sToolTip += QObject::tr("File:\t%1").arg(QFileInfo(m_sFilename).fileName());

	return sToolTip;
}


// Document element methods.
bool qtractorClip::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
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
				else if (eProp.tagName() == "gain")
					qtractorClip::setClipGain(eProp.text().toFloat());
				else if (eProp.tagName() == "fade-in") {
					qtractorClip::setFadeInType(
						qtractorClip::fadeInTypeFromText(eProp.attribute("type")));
					qtractorClip::setFadeInLength(eProp.text().toULong());
				}
				else if (eProp.tagName() == "fade-out") {
					qtractorClip::setFadeOutType(
						qtractorClip::fadeOutTypeFromText(eProp.attribute("type")));
					qtractorClip::setFadeOutLength(eProp.text().toULong());
				}
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


bool qtractorClip::saveElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
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
	pDocument->saveTextElement("gain",
		QString::number(qtractorClip::clipGain()), &eProps);

    QDomElement eFadeIn = pDocument->document()->createElement("fade-in");
	eFadeIn.setAttribute("type",
		qtractorClip::textFromFadeType(qtractorClip::fadeInType()));
    eFadeIn.appendChild(pDocument->document()->createTextNode(
		QString::number(qtractorClip::fadeInLength())));
	eProps.appendChild(eFadeIn);

    QDomElement eFadeOut = pDocument->document()->createElement("fade-out");
	eFadeOut.setAttribute("type",
		qtractorClip::textFromFadeType(qtractorClip::fadeOutType()));
    eFadeOut.appendChild(pDocument->document()->createTextNode(
		QString::number(qtractorClip::fadeOutLength())));
	eProps.appendChild(eFadeOut);

	pElement->appendChild(eProps);

	// Save clip derivative properties...
	return saveClipElement(pDocument, pElement);
}


// Clip fade type textual helper methods.
qtractorClip::FadeType qtractorClip::fadeInTypeFromText ( const QString& sText )
{
	FadeType fadeType = InQuad;

	if (sText == "Linear" || sText == "linear")
		fadeType = Linear;
	else 
	if (sText == "InCubic" || sText == "cubic")
		fadeType = InCubic;
	else 
	if (sText == "OutQuad")
		fadeType = OutQuad;
	else 
	if (sText == "InOutQuad")
		fadeType = InOutQuad;
	else 
	if (sText == "OutCubic")
		fadeType = OutCubic;
	else 
	if (sText == "InOutCubic")
		fadeType = InOutCubic;

	return fadeType;
}

qtractorClip::FadeType qtractorClip::fadeOutTypeFromText ( const QString& sText )
{
	FadeType fadeType = OutQuad;

	if (sText == "Linear" || sText == "linear")
		fadeType = Linear;
	else 
	if (sText == "OutCubic" || sText == "cubic")
		fadeType = OutCubic;
	else 
	if (sText == "InQuad")
		fadeType = InQuad;
	else 
	if (sText == "InOutQuad")
		fadeType = InOutQuad;
	else 
	if (sText == "InCubic")
		fadeType = InCubic;
	else 
	if (sText == "InOutCubic")
		fadeType = InOutCubic;

	return fadeType;
}

QString qtractorClip::textFromFadeType ( FadeType fadeType )
{
	QString sText;

	switch (fadeType) {
	case Linear:
		sText = "Linear";
		break;
	case InQuad:
		sText = "InQuad";
		break;
	case OutQuad:
		sText = "OutQuad";
		break;
	case InOutQuad:
		sText = "InOutQuad";
		break;
	case InCubic:
		sText = "InCubic";
		break;
	case OutCubic:
		sText = "OutCubic";
		break;
	case InOutCubic:
		sText = "InOutCubic";
		break;
	}

	return sText;
}


// end of qtractorClip.h
