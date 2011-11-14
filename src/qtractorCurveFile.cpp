// qtractorCurveFile.cpp
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
#include "qtractorCurveFile.h"

#include "qtractorDocument.h"
#include "qtractorTimeScale.h"
#include "qtractorMidiFile.h"

#include "qtractorMidiControl.h"

#include <QDomDocument>
#include <QDir>


//----------------------------------------------------------------------
// class qtractorCurveFile -- Automation curve file interface impl.
//

// Curve item list serialization methods.
void qtractorCurveFile::load ( QDomElement *pElement )
{
	clear();

	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull(); nChild = nChild.nextSibling()) {
		// Convert node to element, if any.
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;
		// Check for child item...
		if (eChild.tagName() == "filename")
			m_sFilename = eChild.text();
		else
		if (eChild.tagName() == "current")
			m_iCurrentCurve = eChild.text().toInt();
		else
		if (eChild.tagName() == "curve-items") {
			for (QDomNode nItem = eChild.firstChild();
					!nItem.isNull(); nItem = nItem.nextSibling()) {
				// Convert node to element, if any.
				QDomElement eItem = nItem.toElement();
				if (eItem.isNull())
					continue;
				// Check for controller item...
				if (eItem.tagName() == "curve-item") {
					Item *pItem = new Item;
					pItem->name  = eItem.attribute("name");
					pItem->index = eItem.attribute("index").toULong();
					pItem->mode  = modeFromText(eItem.attribute("mode"));
					pItem->process = false; // Defaults.
					pItem->capture = false;
					pItem->locked  = false;
					pItem->logarithmic = false;
					for (QDomNode nProp = eItem.firstChild();
							!nProp.isNull(); nProp = nProp.nextSibling()) {
						// Convert node to element, if any.
						QDomElement eProp = nProp.toElement();
						if (eProp.isNull())
							continue;
						// Check for property item...
						if (eProp.tagName() == "type")
							pItem->ctype = qtractorMidiControl::typeFromText(eProp.text());
						if (eProp.tagName() == "channel")
							pItem->channel = eProp.text().toUShort();
						else
						if (eProp.tagName() == "param")
							pItem->param = eProp.text().toUShort();
						else
						if (eProp.tagName() == "process")
							pItem->process = qtractorDocument::boolFromText(eProp.text());
						else
						if (eProp.tagName() == "capture")
							pItem->capture = qtractorDocument::boolFromText(eProp.text());
						else
						if (eProp.tagName() == "locked")
							pItem->locked = qtractorDocument::boolFromText(eProp.text());
						else
						if (eProp.tagName() == "logarithmic")
							pItem->logarithmic = qtractorDocument::boolFromText(eProp.text());
						else
						if (eProp.tagName() == "color")
							pItem->color.setNamedColor(eProp.text());
					}
					pItem->subject = NULL;
					addItem(pItem);
				}
			}
		}
	}
}


void qtractorCurveFile::save ( qtractorDocument *pDocument,
	QDomElement *pElement, qtractorTimeScale *pTimeScale ) const
{
	if (m_pCurveList == NULL)
		return;

	unsigned short iSeq;
	unsigned short iSeqs = m_items.count();
	if (iSeqs < 1)
		return;

	qtractorMidiFile file;
	if (!file.open(m_sFilename, qtractorMidiFile::Write))
		return;

	unsigned short iTicksPerBeat = pTimeScale->ticksPerBeat();
	qtractorMidiSequence **ppSeqs = new qtractorMidiSequence * [iSeqs];
	for (iSeq = 0; iSeq < iSeqs; ++iSeq)
		ppSeqs[iSeq] = new qtractorMidiSequence(QString(), 0, iTicksPerBeat);

	iSeq = 0;

	int iCurrent = -1;
	QDomElement eItems = pDocument->document()->createElement("curve-items");
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		qtractorCurve *pCurve = (pItem->subject)->curve();
		if (pCurve && !pCurve->isEmpty()) {
			qtractorMidiSequence *pSeq = ppSeqs[iSeq];
			pCurve->writeMidiSequence(pSeq,
				pItem->ctype,
				pItem->channel,
				pItem->param,
				pTimeScale);
			QDomElement eItem
				= pDocument->document()->createElement("curve-item");
			eItem.setAttribute("name", pItem->name);
			eItem.setAttribute("index", QString::number(pItem->index));
			eItem.setAttribute("mode", textFromMode(pItem->mode));
			pDocument->saveTextElement("type",
				qtractorMidiControl::textFromType(pItem->ctype), &eItem);
			pDocument->saveTextElement("channel",
				QString::number(pItem->channel), &eItem);
			pDocument->saveTextElement("param",
				QString::number(pItem->param), &eItem);
			pDocument->saveTextElement("mode",
				textFromMode(pItem->mode), &eItem);
			pDocument->saveTextElement("process",
				pDocument->textFromBool(pItem->process), &eItem);
			pDocument->saveTextElement("capture",
				pDocument->textFromBool(pItem->capture), &eItem);
			pDocument->saveTextElement("locked",
				pDocument->textFromBool(pItem->locked), &eItem);
			pDocument->saveTextElement("logarithmic",
				pDocument->textFromBool(pItem->logarithmic), &eItem);
			pDocument->saveTextElement("color",
				pItem->color.name(), &eItem);
			if (m_pCurveList->currentCurve() == pCurve)
				iCurrent = pItem->index;
			eItems.appendChild(eItem);
		}
		++iSeq;
	}

	pElement->appendChild(eItems);
	
	file.writeHeader(1, iSeqs, iTicksPerBeat);
	file.writeTracks(ppSeqs, iSeqs);
	file.close();

	for (iSeq = 0; iSeq < iSeqs; ++iSeq)
		delete ppSeqs[iSeq];
	delete [] ppSeqs;

	const QString& sFilename
		= QDir(m_sBaseDir).relativeFilePath(m_sFilename);
	pDocument->saveTextElement("filename",
		pDocument->addFile(sFilename), pElement);

	if (iCurrent >= 0) {
		pDocument->saveTextElement("current",
			QString::number(iCurrent), pElement);
	}
}


void qtractorCurveFile::apply ( qtractorTimeScale *pTimeScale )
{
	if (m_pCurveList == NULL)
		return;

	const QString& sFilename = QDir(m_sBaseDir).absoluteFilePath(m_sFilename);

	qtractorMidiFile file;
	if (!file.open(sFilename, qtractorMidiFile::Read))
		return;

	unsigned short iSeq = 0;
	unsigned short iTicksPerBeat = pTimeScale->ticksPerBeat();

	qtractorCurve *pCurrentCurve = NULL;

	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		qtractorCurve *pCurve = (pItem->subject)->curve();
		if (pCurve == NULL)
			pCurve = new qtractorCurve(m_pCurveList, pItem->subject, pItem->mode);
		if (m_iCurrentCurve == int(pItem->index))
			pCurrentCurve = pCurve;
		qtractorMidiSequence seq(QString(), pItem->channel, iTicksPerBeat);
		if (file.readTrack(&seq, iSeq)) {
			pCurve->readMidiSequence(&seq,
				pItem->ctype,
				pItem->channel,
				pItem->param,
				pTimeScale);
		}
		pCurve->setProcess(pItem->process);
		pCurve->setCapture(pItem->capture);
		pCurve->setLocked(pItem->locked);
		pCurve->setLogarithmic(pItem->logarithmic);
		pCurve->setColor(pItem->color);
		++iSeq;
	}

	file.close();

	if (pCurrentCurve)
		m_pCurveList->setCurrentCurve(pCurrentCurve);

	clear();
}


// Text/curve-mode converters...
qtractorCurve::Mode qtractorCurveFile::modeFromText ( const QString& sText )
{
	qtractorCurve::Mode mode = qtractorCurve::Hold;

	if (sText == "Spline")
		mode = qtractorCurve::Spline;
	else
	if (sText == "Linear")
		mode = qtractorCurve::Linear;

	return mode;
}


QString qtractorCurveFile::textFromMode ( qtractorCurve::Mode mode )
{
	QString sText;

	switch (mode) {
	case qtractorCurve::Spline:
		sText = "Spline";
		break;
	case qtractorCurve::Linear:
		sText = "Linear";
		break;
	case qtractorCurve::Hold:
	default:
		sText = "Hold";
		break;
	}

	return sText;
}


// end of qtractorCurveFile.cpp

