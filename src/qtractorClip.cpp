// qtractorClip.cpp
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

#include "qtractorAbout.h"
#include "qtractorClip.h"

#include "qtractorSessionDocument.h"


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
	m_sClipName    = QString::null;
	
	m_iClipStart   = 0;
	m_iClipLength  = 0;
	m_iClipOffset  = 0;
	
	m_iClipTime    = 0;

	m_iSelectStart = 0;
	m_iSelectEnd   = 0;

	m_iLoopStart   = 0;
	m_iLoopEnd     = 0;
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
	if (m_pTrack && m_pTrack->session())
		m_iClipTime = m_pTrack->session()->tickFromFrame(iClipStart);

	m_iClipStart = iClipStart;
}


// Clip frame length accessors.
unsigned long qtractorClip::clipLength (void) const
{
	return m_iClipLength;
}

void qtractorClip::setClipLength ( unsigned long iClipLength )
{
	m_iClipLength = iClipLength;
	
	set_length(iClipLength);
}


// Clip frame offset accessors.
unsigned long qtractorClip::clipOffset (void) const
{
	return m_iClipOffset;
}

void qtractorClip::setClipOffset ( unsigned long iClipOffset )
{
	m_iClipLength = iClipOffset;
	
	set_offset(iClipOffset);
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


// Clip time reference settler method.
void qtractorClip::updateClipTime (void)
{
	if (m_pTrack && m_pTrack->session())
		m_iClipStart = m_pTrack->session()->frameFromTick(m_iClipTime);
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
				if (eProp.tagName() == "start")
					qtractorClip::setClipStart(eProp.text().toULong());
				else if (eProp.tagName() == "length")
					qtractorClip::setClipLength(eProp.text().toULong());
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
	pDocument->saveTextElement("start",
		QString::number(qtractorClip::clipStart()), &eProps);
	pDocument->saveTextElement("length",
		QString::number(qtractorClip::clipLength()), &eProps);
	pElement->appendChild(eProps);

	// Save clip derivative properties...
	return saveClipElement(pDocument, pElement);
}


// end of qtractorClip.h
