// qtractorClip.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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

// Constructor.
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
	m_sClipName   = QString::null;
	m_iClipStart  = 0;
	m_iClipLength = 0;
	m_bClipSelect = false;
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
}


// Clip selection accessors.
void qtractorClip::setClipSelected ( bool bClipSelect )
{
	m_bClipSelect = bClipSelect;
}

bool qtractorClip::isClipSelected (void) const
{
	return m_bClipSelect;
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
