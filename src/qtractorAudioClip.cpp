// qtractorAudioClip.cpp
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
#include "qtractorAudioClip.h"
#include "qtractorAudioBuffer.h"
#include "qtractorAudioEngine.h"
#include "qtractorAudioPeak.h"

#include "qtractorSessionDocument.h"

#include <QFileInfo>
#include <QPainter>
#include <QPolygon>


//----------------------------------------------------------------------
// class qtractorAudioClip -- Audio file/buffer clip.
//

// Constructor.
qtractorAudioClip::qtractorAudioClip ( qtractorTrack *pTrack )
	: qtractorClip(pTrack)
{
	m_pPeak = NULL;
	m_pBuff = NULL;

	m_fTimeStretch = 1.0f;
}

// Copy constructor.
qtractorAudioClip::qtractorAudioClip ( const qtractorAudioClip& clip )
	: qtractorClip(clip.track())
{
	m_pPeak = NULL;
	m_pBuff = NULL;

	m_fTimeStretch = clip.timeStretch();

	setFilename(clip.filename());
}


// Destructor.
qtractorAudioClip::~qtractorAudioClip (void)
{
	if (m_pPeak)
		delete m_pPeak;
	if (m_pBuff)
		delete m_pBuff;
}


// Time-stretch factor.
void qtractorAudioClip::setTimeStretch ( float fTimeStretch )
{
	m_fTimeStretch = fTimeStretch;
}

float qtractorAudioClip::timeStretch (void) const
{
	return m_fTimeStretch;
}


// The main use method.
bool qtractorAudioClip::openAudioFile ( const QString& sFilename, int iMode )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorAudioClip::openAudioFile(\"%s\", %d)\n",
		sFilename.toUtf8().constData(), iMode);
#endif

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	if (m_pBuff) {
		delete m_pBuff;
		m_pBuff = NULL;
	}

	// Check file buffer number of channels...
	unsigned short iChannels = 0;
	bool bWrite = (iMode & qtractorAudioFile::Write);
	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (bWrite ?
			pTrack->inputBus() : pTrack->outputBus());
	if (pAudioBus)
		iChannels = pAudioBus->channels();

	// However, this number of channels is only useful
	// only when recording, otherwise we'll stick with
	// source audio file's one...
	if (bWrite && iChannels < 1)
		return false;

	m_pBuff = new qtractorAudioBuffer(iChannels, pSession->sampleRate());
	m_pBuff->setOffset(clipOffset());
	m_pBuff->setLength(clipLength());
	m_pBuff->setTimeStretch(m_fTimeStretch);

	if (!m_pBuff->open(sFilename, iMode)) {
		delete m_pBuff;
		m_pBuff = NULL;
		return false;
	}

	// FIXME: Peak files shouldn't be created on-the-fly?
	if (!bWrite
		&& (m_pPeak == NULL || sFilename != filename())
		&& pSession->audioPeakFactory()) {
		if (m_pPeak)
			delete m_pPeak;
		m_pPeak = pSession->audioPeakFactory()->createPeak(
			sFilename, pSession->sampleRate(), m_pBuff->timeStretch(), /*???*/
			pSession->sessionDir());
	}

	// Set local properties...
	setFilename(sFilename);
	setDirty(false);

	// Clip name should be clear about it all.
	if (clipName().isEmpty())
		setClipName(QFileInfo(filename()).baseName());

	// Default clip length will be the whole file length.
	if (clipLength() == 0)
		setClipLength(m_pBuff->length() - m_pBuff->offset());

	return true;
}


// Direct write method.
void qtractorAudioClip::write ( float **ppBuffer,
	unsigned int iFrames, unsigned short iChannels )
{
	if (m_pBuff) m_pBuff->write(ppBuffer, iFrames, iChannels);
}


// Direct sync method.
void qtractorAudioClip::syncExport (void)
{
	if (m_pBuff) m_pBuff->syncExport();
}


// Intra-clip frame positioning.
void qtractorAudioClip::seek ( unsigned long iFrame )
{
	if (m_pBuff) m_pBuff->seek(iFrame);
}


// Reset clip state.
void qtractorAudioClip::reset ( bool bLooping )
{
	if (m_pBuff) m_pBuff->reset(bLooping);
}


// Loop positioning.
void qtractorAudioClip::set_loop ( unsigned long iLoopStart,
	unsigned long iLoopEnd )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorAudioClip::loop(%p, %lu, %lu)\n",
		this, iLoopStart, iLoopEnd);
#endif

	if (m_pBuff) m_pBuff->setLoop(iLoopStart, iLoopEnd);
}


// Clip close-commit (record specific)
void qtractorAudioClip::close ( bool bForce )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorAudioClip::close(%p, %d)\n", this, int(bForce));
#endif

	// Enforced to get rid of peak instance...
	if (bForce) {
		if (m_pPeak)
			delete m_pPeak;
		m_pPeak = NULL;
	}

	if (m_pBuff == NULL)
		return;

	// Commit the final clip length (record specific)...
	if (clipLength() < 1)
		setClipLength(m_pBuff->fileLength());

	// Close and ditch stuff...
	delete m_pBuff;
	m_pBuff = NULL;

	// If proven empty, remove the file.
	if (bForce && clipLength() < 1)
		QFile::remove(filename());
}


// Audio clip (re)open method.
void qtractorAudioClip::open (void)
{
	openAudioFile(filename());
}


// Audio clip special process cycle executive.
void qtractorAudioClip::process ( unsigned long iFrameStart,
	unsigned long iFrameEnd )
{
	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (track()->outputBus());
	if (pAudioBus == NULL)
		return;

	// Get the next bunch from the clip...
	unsigned long iClipStart = clipStart();
	unsigned long iClipEnd   = iClipStart + clipLength();
	if (iFrameStart < iClipStart && iFrameEnd > iClipStart) {
		if (m_pBuff->inSync(0, iFrameEnd - iClipStart)) {
			m_pBuff->readMix(pAudioBus->buffer(), iFrameEnd - iClipStart,
				pAudioBus->channels(), iClipStart - iFrameStart, gain(0));
		}
	}
	else if (iFrameStart >= iClipStart && iFrameStart < iClipEnd) {
		if (m_pBuff->inSync(iFrameStart - iClipStart, iFrameEnd - iClipStart)) {
			m_pBuff->readMix(pAudioBus->buffer(), iFrameEnd - iFrameStart,
				pAudioBus->channels(), 0, gain(iFrameStart - iClipStart));
		}
	}
}


// Audio clip paint method.
void qtractorAudioClip::drawClip ( QPainter *pPainter, const QRect& clipRect,
	unsigned long iClipOffset )
{
	// Fill clip background...
	qtractorClip::drawClip(pPainter, clipRect, iClipOffset);

	qtractorSession *pSession = track()->session();
	if (pSession == NULL)
		return;

	// Cache some peak data...
	if (m_pPeak == NULL)
		return;
	unsigned int iPeriod = m_pPeak->period();
	if (iPeriod < 1)
		return;
	unsigned int iChannels = m_pPeak->channels();
	if (iChannels < 1)
		return;
	unsigned long iFrames = m_pPeak->frames();
	if (iFrames < 1)
		return;

	unsigned long iframe = ((iClipOffset + clipOffset()) / iPeriod);
	if (iframe > iFrames)
		return;

	unsigned long nframes
		= (pSession->frameFromPixel(clipRect.width()) / iPeriod);
	if (nframes > iFrames - iframe)
		nframes = iFrames - iframe;
	qtractorAudioPeakFrame *pframes
		= new qtractorAudioPeakFrame [iChannels * nframes];

	// Grab them in...
	m_pPeak->getPeak(pframes, iframe, nframes);

	// Draw peak chart...
	int h1 = (clipRect.height() / iChannels);
	int h2 = (h1 / 2);
	int n, i, j, x, y;
	int kdelta = (pSession->frameFromPixel(1) / iPeriod);

	if (kdelta < 1) {
		// Polygon mode...
		int ymax, yrms;
		unsigned int iPolyPoints = (nframes << 1) + 2;
		QPolygon **pPolyMax = new QPolygon* [iChannels];
		QPolygon **pPolyRms = new QPolygon* [iChannels];
		for (i = 0; i < (int) iChannels; ++i) {
			pPolyMax[i] = new QPolygon(iPolyPoints);
			pPolyRms[i] = new QPolygon(iPolyPoints);
		}
		// Build polygonal vertexes...
		j = 0;
		for (n = 0; n < (int) nframes; ++n) {
			x = clipRect.x() + pSession->pixelFromFrame(n * iPeriod);
			y = clipRect.y() + h2;
			for (i = 0; i < (int) iChannels; ++i, ++j) {
				ymax = (h2 * pframes[j].peakMax) / 255;
				yrms = (h2 * pframes[j].peakRms) / 255;
				pPolyMax[i]->setPoint(n, x, y + ymax);
				pPolyMax[i]->setPoint(iPolyPoints - n - 1, x, y - ymax);
				pPolyRms[i]->setPoint(n, x, y + yrms);
				pPolyRms[i]->setPoint(iPolyPoints - n - 1, x, y - yrms);
				y += h1;
			}
		}
		// Close, draw and free the polygons...
		x = clipRect.right();
		y = clipRect.y() + h2;
		j -= (int) iChannels;
		for (i = 0; i < (int) iChannels; ++i, ++j) {
			ymax = (h2 * pframes[j].peakMax) / 255;
			yrms = (h2 * pframes[j].peakRms) / 255;
			pPolyMax[i]->setPoint(n, x, y + ymax);
			pPolyMax[i]->setPoint(iPolyPoints - n - 1, x, y - ymax);
			pPolyRms[i]->setPoint(n, x, y + yrms);
			pPolyRms[i]->setPoint(iPolyPoints - n - 1, x, y - yrms);
			y += h1;
			pPainter->setBrush(track()->foreground());
			pPainter->drawPolygon(*pPolyMax[i]);
			pPainter->setBrush(track()->foreground().light());
			pPainter->drawPolygon(*pPolyRms[i]);
			delete pPolyMax[i];
			delete pPolyRms[i];
		}
		delete [] pPolyMax;
		delete [] pPolyRms;
		// Done on polygon mode.
	} else {
		// Bar-accumulated mode.
		int v, k;
		int *ymax = new int [iChannels];
		int *yrms = new int [iChannels];
		for (i = 0; i < (int) iChannels; ++i)
			ymax[i] = yrms[i] = 0;
		j = k = 0;
		x = clipRect.x();
		for (n = 0; n < (int) nframes; ++n) {
			y = clipRect.y() + h2;
			if (kdelta < 1)
				x = clipRect.x() + pSession->pixelFromFrame(n * iPeriod);
			for (i = 0; i < (int) iChannels; ++i, ++j) {
				v = (h2 * pframes[j].peakMax) / 255;
				if (ymax[i] < v)
					ymax[i] = v;
				v = (h2 * pframes[j].peakRms) / 255;
				if (yrms[i] < v)
					yrms[i] = v;
				if (kdelta < 1) {
					pPainter->setPen(track()->foreground());
					pPainter->drawLine(x, y - ymax[i], x, y + ymax[i]);
					pPainter->setPen(track()->foreground().light());
					pPainter->drawLine(x, y - yrms[i], x, y + yrms[i]);
					ymax[i] = yrms[i] = 0;
					y += h1;
				}
			}
			if (kdelta < 1) {
				kdelta = pSession->frameFromPixel(x - clipRect.x())
					/ (++k * iPeriod);
			}
			kdelta--;
		}
		// Free (ab)used arrays.
		delete [] yrms;
		delete [] ymax;
		// Done on bar-accumulated mode.
	}

	// Our peak buffer at large.
	delete [] pframes;
}


// Audio clip tool-tip.
QString qtractorAudioClip::toolTip (void) const
{
	QString sToolTip = qtractorClip::toolTip();

	qtractorAudioFile *pFile = NULL;
	if (m_pBuff) {
		pFile = m_pBuff->file();
		if (pFile) {
			sToolTip += QObject::tr("\nAudio:\t%1 channels, %2 Hz")
				.arg(pFile->channels())
				.arg(pFile->sampleRate());
			if (m_pBuff->isTimeStretch())
				sToolTip += QObject::tr("\n\t(%1% time stretch)")
					.arg(100.0f * m_pBuff->timeStretch(), 0, 'g', 3);
		}
	}

	return sToolTip;
}


// Virtual document element methods.
bool qtractorAudioClip::loadClipElement (
	qtractorSessionDocument * /* pDocument */, QDomElement *pElement )
{
	// Load track children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {
		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;
		// Load clip properties..
		if (eChild.tagName() == "filename")
			qtractorAudioClip::setFilename(eChild.text());
		else if (eChild.tagName() == "time-stretch")
			qtractorAudioClip::setTimeStretch(eChild.text().toFloat());
	}

	return true;
}


bool qtractorAudioClip::saveClipElement (
	qtractorSessionDocument *pDocument, QDomElement *pElement )
{
	QDomElement eAudioClip = pDocument->document()->createElement("audio-clip");
	pDocument->saveTextElement("filename",
		qtractorAudioClip::filename(), &eAudioClip);
	pDocument->saveTextElement("time-stretch",
		QString::number(qtractorAudioClip::timeStretch()), &eAudioClip);
	pElement->appendChild(eAudioClip);

	return true;
}


// end of qtractorAudioClip.cpp
