// qtractorAudioClip.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
	m_fPitchShift  = 1.0f;
}

// Copy constructor.
qtractorAudioClip::qtractorAudioClip ( const qtractorAudioClip& clip )
	: qtractorClip(clip.track())
{
	m_pPeak = NULL;
	m_pBuff = NULL;

	m_fTimeStretch = clip.timeStretch();
	m_fPitchShift  = clip.pitchShift();

	setFilename(clip.filename());

	// Clone the audio peak, if any...
	if (clip.m_pPeak)
		m_pPeak = new qtractorAudioPeak(*clip.m_pPeak);
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


// Pitch-shift factor.
void qtractorAudioClip::setPitchShift ( float fPitchShift )
{
	m_fPitchShift = fPitchShift;
}

float qtractorAudioClip::pitchShift (void) const
{
	return m_fPitchShift;
}


// The main use method.
bool qtractorAudioClip::openAudioFile ( const QString& sFilename, int iMode )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioClip[%p]::openAudioFile(\"%s\", %d)",
		this, sFilename.toUtf8().constData(), iMode);
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
	m_pBuff->setPitchShift(m_fPitchShift);

	if (!m_pBuff->open(sFilename, iMode)) {
		delete m_pBuff;
		m_pBuff = NULL;
		return false;
	}

	// Peak files should also be created on-the-fly?
	if ((m_pPeak == NULL || sFilename != filename())
		&& pSession->audioPeakFactory()) {
		if (m_pPeak)
			delete m_pPeak;
		m_pPeak = pSession->audioPeakFactory()->createPeak(
			sFilename, m_pBuff->timeStretch());
		if (bWrite)
			m_pBuff->setPeak(m_pPeak);
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
	qDebug("qtractorAudioClip[%p]::set_loop(%lu, %lu)",
		this, iLoopStart, iLoopEnd);
#endif

	if (m_pBuff) m_pBuff->setLoop(iLoopStart, iLoopEnd);
}


// Clip close-commit (record specific)
void qtractorAudioClip::close ( bool bForce )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioClip[%p]::close(%d)\n", this, int(bForce));
#endif

	if (m_pBuff == NULL)
		return;

	// Commit the final clip length (record specific)...
	if (clipLength() < 1)
		setClipLength(m_pBuff->fileLength());
	else
	// Shall we ditch the current peak file?
	// (don't if closing from recording)
	if (bForce && m_pPeak && m_pBuff->peak() == NULL) {
		delete m_pPeak;
		m_pPeak = NULL;
	}

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
void qtractorAudioClip::process (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	if (m_pBuff == NULL)
		return;

	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (track()->outputBus());
	if (pAudioBus == NULL)
		return;

	// Get the next bunch from the clip...
	unsigned long iClipStart = clipStart();
	unsigned long iClipEnd   = iClipStart + clipLength();
	if (iFrameStart < iClipStart && iFrameEnd > iClipStart) {
		unsigned long iOffset = iFrameEnd - iClipStart;
		if (m_pBuff->inSync(0, iOffset)) {
			m_pBuff->readMix(pAudioBus->buffer(), iOffset,
				pAudioBus->channels(), iClipStart - iFrameStart, gain(iOffset));
		}
	}
	else
	if (iFrameStart >= iClipStart && iFrameStart < iClipEnd) {
		unsigned long iOffset = iFrameEnd - iClipStart;
		if (m_pBuff->inSync(iFrameStart - iClipStart, iOffset)) {
			m_pBuff->readMix(pAudioBus->buffer(), iFrameEnd - iFrameStart,
				pAudioBus->channels(), 0, gain(iOffset));
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

	if (!m_pPeak->openRead())
		return;

	unsigned int iPeriod = m_pPeak->period();
	if (iPeriod < 1)
		return;
	unsigned int iChannels = m_pPeak->channels();
	if (iChannels < 1)
		return;

	unsigned long iframe
		= ((iClipOffset + clipOffset()) / iPeriod);
	unsigned long nframes
		= (pSession->frameFromPixel(clipRect.width()) / iPeriod) + 1;

	// Grab them in...
	qtractorAudioPeakFile::Frame *pframes = m_pPeak->read(iframe, nframes);
	if (pframes == NULL)
		return;

	// Draw peak chart...
	const QColor& fg = track()->foreground();
	int h1 = (clipRect.height() / iChannels);
	int h2 = (h1 / 2);
	unsigned int n, i;
	int x, y;

	// Polygon mode...
	int ymax, yrms;
	bool bZoomedIn = (clipRect.width() > int(nframes));
	unsigned int iPolyPoints
		= (bZoomedIn ? nframes : (clipRect.width() >> 1)) << 1;
	QPolygon **pPolyMax = new QPolygon* [iChannels];
	QPolygon **pPolyRms = new QPolygon* [iChannels];
	for (i = 0; i < iChannels; ++i) {
		pPolyMax[i] = new QPolygon(iPolyPoints);
		pPolyRms[i] = new QPolygon(iPolyPoints);
	}

	// Build polygonal vertexes...
	if (bZoomedIn) {
		// Zoomed in...
		// - rade peak-frames for pixels.
		for (n = 0; n < nframes; ++n) {
			x = clipRect.x() + (n * clipRect.width()) / nframes;
			y = clipRect.y() + h2;
			for (i = 0; i < iChannels; ++i, ++pframes) {
				ymax = (h2 * pframes->max) >> 8;
				yrms = (h2 * pframes->rms) >> 8;
				pPolyMax[i]->setPoint(n, x, y + ymax);
				pPolyMax[i]->setPoint(iPolyPoints - n - 1, x, y - ymax);
				pPolyRms[i]->setPoint(n, x, y + yrms);
				pPolyRms[i]->setPoint(iPolyPoints - n - 1, x, y - yrms);
				y += h1;
			}
		}
	} else {
		// Zoomed out...
		// - trade (2) pixels for peak-frames (expensiver).
		int x2, k = 0;
		unsigned int n2, n0 = 0;
		unsigned char v, vmax, vrms;
		for (x2 = 0; x2 < clipRect.width(); x2 += 2, ++k) {
			x = clipRect.x() + x2;
			y = clipRect.y() + h2;
			n = (iChannels * x2 * nframes) / clipRect.width();
			for (i = 0; i < iChannels; ++i) {
				vmax = pframes[n + i].max;
				vrms = pframes[n + i].rms;;
				for (n2 = n0 + i; n2 < n + i; n2 += iChannels) {
					v = pframes[n2].max;
					if (vmax < v)
						vmax = v;
					v = pframes[n2].rms;
					if (vrms < v)
						vrms = v;
				}
				ymax = (h2 * vmax) >> 8;
				yrms = (h2 * vrms) >> 8;
				pPolyMax[i]->setPoint(k, x, y + ymax);
				pPolyMax[i]->setPoint(iPolyPoints - k - 1, x, y - ymax);
				pPolyRms[i]->setPoint(k, x, y + yrms);
				pPolyRms[i]->setPoint(iPolyPoints - k - 1, x, y - yrms);
				y += h1;
			}
			n0 = n + iChannels;
		}
	}

	// Close, draw and free the polygons...
	pPainter->setPen(fg.lighter(140));
	for (i = 0; i < iChannels; ++i) {
		pPainter->setBrush(fg);
		pPainter->drawPolygon(*pPolyMax[i]);
		pPainter->setBrush(fg.lighter(120));
		pPainter->drawPolygon(*pPolyRms[i]);
		delete pPolyMax[i];
		delete pPolyRms[i];
	}
	// Done on polygon mode.
	delete [] pPolyMax;
	delete [] pPolyRms;
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
			if (m_pBuff->isPitchShift())
				sToolTip += QObject::tr("\n\t(%1% pitch shift)")
					.arg(100.0f * m_pBuff->pitchShift(), 0, 'g', 3);
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
		else if (eChild.tagName() == "pitch-shift")
			qtractorAudioClip::setPitchShift(eChild.text().toFloat());
	}

	return true;
}


bool qtractorAudioClip::saveClipElement (
	qtractorSessionDocument *pDocument, QDomElement *pElement )
{
	QDomElement eAudioClip = pDocument->document()->createElement("audio-clip");
	pDocument->saveTextElement("filename",
		qtractorAudioClip::relativeFilename(), &eAudioClip);
	pDocument->saveTextElement("time-stretch",
		QString::number(qtractorAudioClip::timeStretch()), &eAudioClip);
	pDocument->saveTextElement("pitch-shift",
		QString::number(qtractorAudioClip::pitchShift()), &eAudioClip);
	pElement->appendChild(eAudioClip);

	return true;
}


// end of qtractorAudioClip.cpp
