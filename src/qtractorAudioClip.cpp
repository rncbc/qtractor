// qtractorAudioClip.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEngine.h"
#include "qtractorAudioPeak.h"

#include "qtractorDocument.h"

#include "qtractorSession.h"
#include "qtractorFileList.h"

#include <QFileInfo>
#include <QPainter>
#include <QPolygon>

#include <QDomDocument>

#include <math.h>


//----------------------------------------------------------------------
// class qtractorAudioClip::Key -- Audio buffered clip (hash key).
//
class qtractorAudioClip::Key
{
public:

	// Constructor.
	Key(qtractorAudioClip *pAudioClip)
		{ update(pAudioClip); }

	// Key settler.
	void update(qtractorAudioClip *pAudioClip)
	{
		m_pTrack = pAudioClip->track();
		m_sFilename = pAudioClip->filename();;
		m_iClipOffset = pAudioClip->clipOffset();
		m_iClipLength = pAudioClip->clipLength();
		m_fTimeStretch = pAudioClip->timeStretch();
		m_fPitchShift = pAudioClip->pitchShift();
		m_iOverlap = pAudioClip->overlap();
	}

	// Key accessors.
	qtractorTrack *track() const
		{ return m_pTrack; }
	const QString& filename() const
		{ return m_sFilename; }
	unsigned long clipOffset() const
		{ return m_iClipOffset; }
	unsigned long clipLength() const
		{ return m_iClipLength; }
	float timeStretch() const
		{ return m_fTimeStretch; }
	float pitchShift() const
		{ return m_fPitchShift; }
	unsigned int overlap() const
		{ return m_iOverlap; }

	// Match descriminator.
	bool operator== (const Key& other) const
	{
		return m_pTrack       == other.track()
			&& m_sFilename    == other.filename()
			&& m_iClipOffset  == other.clipOffset()
			&& m_iClipLength  == other.clipLength()
			&& m_fTimeStretch == other.timeStretch()
			&& m_fPitchShift  == other.pitchShift()
			&& m_iOverlap     == other.overlap();
	}

private:

	// Interesting variables.
	qtractorTrack *m_pTrack;
	QString        m_sFilename;
	unsigned long  m_iClipOffset;
	unsigned long  m_iClipLength;
	float          m_fTimeStretch;
	float          m_fPitchShift;
	unsigned int   m_iOverlap;
};


uint qHash ( const qtractorAudioClip::Key& key )
{
	return qHash(key.track())
		 ^ qHash(key.filename())
		 ^ qHash(key.clipOffset())
		 ^ qHash(key.clipLength())
		 ^ qHash(long(100.0f * key.timeStretch()))
		 ^ qHash(long(100.0f * key.pitchShift()))
		 ^ qHash(key.overlap());
}


qtractorAudioClip::Hash qtractorAudioClip::g_hashTable;


//----------------------------------------------------------------------
// class qtractorAudioClip -- Audio file/buffer clip.
//

// Constructor.
qtractorAudioClip::qtractorAudioClip ( qtractorTrack *pTrack )
	: qtractorClip(pTrack)
{
	m_pPeak = NULL;
	m_pKey  = NULL;
	m_pData = NULL;

	m_fTimeStretch = 1.0f;
	m_fPitchShift  = 1.0f;

	m_iOverlap = 0;
}

// Copy constructor.
qtractorAudioClip::qtractorAudioClip ( const qtractorAudioClip& clip )
	: qtractorClip(clip.track())
{
	m_pPeak = NULL;
	m_pKey  = NULL;
	m_pData = NULL;

	m_fTimeStretch = clip.timeStretch();
	m_fPitchShift  = clip.pitchShift();

	m_iOverlap = clip.overlap();

	setFilename(clip.filename());
	setClipGain(clip.clipGain());
	setClipName(clip.clipName());

	// Clone the audio peak, if any...
	if (clip.m_pPeak)
		m_pPeak = new qtractorAudioPeak(*clip.m_pPeak);
}


// Destructor.
qtractorAudioClip::~qtractorAudioClip (void)
{
	close();

	if (m_pPeak)
		delete m_pPeak;
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


// Alternate overlap tag.
unsigned int qtractorAudioClip::overlap (void) const
{
	return m_iOverlap;
}


// Alternating overlap test.
bool qtractorAudioClip::isOverlap ( unsigned int iOverlapSize ) const
{
	if (m_pData == NULL)
		return false;

	unsigned long iClipStart = clipStart();
	unsigned long iClipEnd = iClipStart + clipLength() + iOverlapSize;

	QListIterator<qtractorAudioClip *> iter(m_pData->clips());
	while (iter.hasNext()) {
		qtractorAudioClip *pClip = iter.next();
		unsigned long iClipStart2 = pClip->clipStart();
		unsigned long iClipEnd2 = iClipStart2 + pClip->clipLength() + iOverlapSize;
		if ((iClipStart >= iClipStart2 && iClipEnd2 >  iClipStart) ||
			(iClipEnd   >  iClipStart2 && iClipEnd2 >= iClipEnd))
			return true;
	}

	return false;
}


// The main use method.
bool qtractorAudioClip::openAudioFile ( const QString& sFilename, int iMode )
{
	closeAudioFile();

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

	// Save old property (need for peak file ignition)...
	bool bFilenameChanged = (sFilename != filename());

	// Set local properties...
	setFilename(sFilename);
	setDirty(false);

	// Register file path...
	pSession->files()->addClipItem(qtractorFileList::Audio, this, bWrite);

	// New key-data sequence...
	if (!bWrite) {
		m_pKey  = new Key(this);
		m_pData = g_hashTable.value(*m_pKey, NULL);
		if (m_pData) {
			// Check if current clip overlaps any other...
			unsigned int iOverlapSize
				= pSession->audioEngine()->bufferSize() << 2;
			bool bOverlap = isOverlap(iOverlapSize);
			while (bOverlap) {
				++m_iOverlap;
				m_pKey->update(this);
				m_pData = g_hashTable.value(*m_pKey, NULL);
				bOverlap = isOverlap(iOverlapSize);
			}
			// Only if it doesn't overlap any...
			if (m_pData && !bOverlap) {
				m_pData->attach(this);
				// Peak files should also be created on-the-fly...
				qtractorAudioBuffer *pBuff = m_pData->buffer();
				if ((m_pPeak == NULL || bFilenameChanged)
					&& pSession->audioPeakFactory()) {
					if (m_pPeak)
						delete m_pPeak;
					m_pPeak = pSession->audioPeakFactory()->createPeak(
						sFilename, pBuff->timeStretch());
				}
				// Clip name should be clear about it all.
				if (clipName().isEmpty())
					setClipName(shortClipName(QFileInfo(filename()).baseName()));
				return true;
			}
		}
	}

	// Initialize audio buffer container...
	m_pData = new Data(pTrack, iChannels, pSession->sampleRate());
	m_pData->attach(this);

	qtractorAudioBuffer *pBuff = m_pData->buffer();

	pBuff->setOffset(clipOffset());
	pBuff->setLength(clipLength());
	pBuff->setTimeStretch(m_fTimeStretch);
	pBuff->setPitchShift(m_fPitchShift);

	if (!pBuff->open(sFilename, iMode)) {
		delete m_pData;
		m_pData = NULL;
		return false;
	}

	// Default clip length will be the whole file length.
	if (clipLength() == 0)
		setClipLength(pBuff->length() - pBuff->offset());

	// Peak files should also be created on-the-fly?
	if ((m_pPeak == NULL || bFilenameChanged)
		&& pSession->audioPeakFactory()) {
		if (m_pPeak)
			delete m_pPeak;
		m_pPeak = pSession->audioPeakFactory()->createPeak(
			sFilename, pBuff->timeStretch());
		if (bWrite)
			pBuff->setPeak(m_pPeak);
	}

	// Clip name should be clear about it all.
	if (clipName().isEmpty())
		setClipName(shortClipName(QFileInfo(filename()).baseName()));

	// Something might have changed...
	updateHashKey();
	insertHashKey();

	return true;
}


// Private cleanup.
void qtractorAudioClip::closeAudioFile (void)
{
	if (m_pData) {
		m_pData->detach(this);
		if (m_pData->count() < 1) {
			removeHashKey();
			delete m_pData;
		#if 0
			// ATTN: If proven empty, remove the file...
			if (clipLength() < 1)
				QFile::remove(filename());
		#endif
		}
		m_pData = NULL;
		// Unregister file path...
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->files()->removeClipItem(qtractorFileList::Audio, this);
	}

	if (m_pKey) {
		delete m_pKey;
		m_pKey = NULL;
	}
}


// Manage local hash key.
void qtractorAudioClip::insertHashKey (void)
{
	if (m_pKey) g_hashTable.insert(*m_pKey, m_pData);
}


void qtractorAudioClip::updateHashKey (void)
{
	if (m_pKey == NULL)
		m_pKey = new Key(this);
	else
		m_pKey->update(this);
}


void qtractorAudioClip::removeHashKey (void)
{
	if (m_pKey) g_hashTable.remove(*m_pKey);
}


// Unlink (clone) local hash data.
void qtractorAudioClip::unlinkHashData (void)
{
	if (m_pData == NULL)
		return;
	if (m_pData->count() < 2)
		return;

	qtractorAudioBuffer *pBuff = m_pData->buffer();

	Data *pNewData = new Data(track(), pBuff->channels(), pBuff->sampleRate());

	qtractorAudioBuffer *pNewBuff = pNewData->buffer();

	pNewBuff->setOffset(pBuff->offset());
	pNewBuff->setLength(pBuff->length());
	pNewBuff->setTimeStretch(pBuff->timeStretch());
	pNewBuff->setPitchShift(pBuff->pitchShift());

	if (pNewBuff->open(filename())) {
		m_pData->detach(this);
		m_pData = pNewData;
		if (m_pKey) {
			delete m_pKey;
			m_pKey = NULL;
		}
		m_pData->attach(this);
	} else {
		delete pNewData;
	}
}


// Relink local hash data.
void qtractorAudioClip::relinkHashData (void)
{
	if (m_pData == NULL)
		return;
	if (m_pData->count() > 1)
		return;

	removeHashKey();
	updateHashKey();

	Data *pNewData = g_hashTable.value(*m_pKey, NULL);
	if (pNewData == NULL) {
		delete m_pKey;
		m_pKey = NULL;
	} else {
		m_pData->detach(this);
		delete m_pData;
		m_pData = pNewData;
		m_pData->attach(this);
	}

	insertHashKey();
}


// Whether local hash is being shared.
bool qtractorAudioClip::isHashLinked (void) const
{
	return (m_pData && m_pData->count() > 1);
}


// Make sure the clip hash-table gets reset.
void qtractorAudioClip::clearHashTable (void)
{
	g_hashTable.clear();
}


// Direct write method.
void qtractorAudioClip::write ( float **ppBuffer,
	unsigned int iFrames, unsigned short iChannels, unsigned int iOffset )
{
	if (m_pData) m_pData->write(ppBuffer, iFrames, iChannels, iOffset);
}


// Direct sync method.
void qtractorAudioClip::syncExport (void)
{
	if (m_pData) m_pData->syncExport();
}


// Intra-clip frame positioning.
void qtractorAudioClip::seek ( unsigned long iFrame )
{
	if (m_pData) m_pData->seek(iFrame);
}


// Reset clip state.
void qtractorAudioClip::reset ( bool bLooping )
{
	if (m_pData) m_pData->reset(bLooping);
}


// Loop positioning.
void qtractorAudioClip::setLoop (
	unsigned long iLoopStart, unsigned long iLoopEnd )
{
	if (m_pData == NULL)
		return;

	qtractorAudioBuffer *pBuff = m_pData->buffer();
	if (pBuff == NULL)
		return;

	if (iLoopStart == 0 && iLoopEnd >= pBuff->length())
		iLoopStart = iLoopEnd = 0;

	if (iLoopStart == pBuff->loopStart() && iLoopEnd == pBuff->loopEnd())
		return;

	// Check if buffer data has been hash-linked,
	// unlink (clone) it for proper loop isolation...
	if (iLoopStart < iLoopEnd) {
		// Unlink (clone) buffer data...
		unlinkHashData();
	} else {
		// Relink buffer data...
		relinkHashData();
	}

	// A brand new one is up!
	pBuff = m_pData->buffer();
	pBuff->setLoop(iLoopStart, iLoopEnd);
}


// Clip close-commit (record specific)
void qtractorAudioClip::close (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorAudioClip[%p]::close()", this);
#endif

	// Take pretended clip-length...
	qtractorAudioBuffer *pBuff = buffer();
	if (pBuff) {
		// Commit the final clip length (record specific)...
		if (clipLength() < 1)
			setClipLength(pBuff->fileLength());
		else
		// Shall we ditch the current peak file?
		// (don't if closing from recording)
		if (m_pPeak && pBuff->peak() == NULL) {
			delete m_pPeak;
			m_pPeak = NULL;
		}
	}

	// Close and ditch stuff...
	closeAudioFile();
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
	qtractorAudioBuffer *pBuff = buffer();
	if (pBuff == NULL)
		return;

	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (track()->outputBus());
	if (pAudioBus == NULL)
		return;

	// Get the next bunch from the clip...
	unsigned long iClipStart = clipStart();
	if (iClipStart > iFrameEnd)
		return;

	unsigned long iClipEnd = iClipStart + clipLength();
	if (iClipEnd < iFrameStart)
		return;

	unsigned long iOffset
		= (iFrameEnd < iClipEnd ? iFrameEnd : iClipEnd) - iClipStart;

	if (iClipStart > iFrameStart) {
		if (pBuff->inSync(0, iOffset)) {
			pBuff->readMix(
				pAudioBus->buffer(),
				iOffset,
				pAudioBus->channels(),
				iClipStart - iFrameStart,
				gain(iOffset));
		}
	} else {
		if (pBuff->inSync(iFrameStart - iClipStart, iOffset)) {
			pBuff->readMix(
				pAudioBus->buffer(),
				(iFrameEnd < iClipEnd ? iFrameEnd : iClipEnd) - iFrameStart,
				pAudioBus->channels(),
				0,
				gain(iOffset));
		}
	}
}


// Audio clip paint method.
void qtractorAudioClip::draw ( QPainter *pPainter, const QRect& clipRect,
	unsigned long iClipOffset )
{
	qtractorSession *pSession = track()->session();
	if (pSession == NULL)
		return;

	// Cache some peak data...
	if (m_pPeak == NULL)
		return;

	if (!m_pPeak->openRead())
		return;

	unsigned short iPeriod = m_pPeak->period();
	if (iPeriod < 1)
		return;
	unsigned short iChannels = m_pPeak->channels();
	if (iChannels < 1)
		return;

	unsigned long iframe
		= ((iClipOffset + clipOffset()) / iPeriod);
	unsigned long nframes
		= (pSession->frameFromPixel(clipRect.width()) / iPeriod) + 2;

	// Needed an even number of polygon points...
	bool bZoomedIn = (clipRect.width() > int(nframes));
	unsigned int iPolyPoints
		= (bZoomedIn ? nframes: (clipRect.width() >> 1)) << 1;
	if (iPolyPoints < 2)
		return;

	// Grab them in...
	qtractorAudioPeakFile::Frame *pframes = m_pPeak->read(iframe, nframes);
	if (pframes == NULL)
		return;

	// Polygon init...
	unsigned short i;
	QPolygon **pPolyMax = new QPolygon* [iChannels];
	QPolygon **pPolyRms = new QPolygon* [iChannels];
	for (i = 0; i < iChannels; ++i) {
		pPolyMax[i] = new QPolygon(iPolyPoints);
		pPolyRms[i] = new QPolygon(iPolyPoints);
	}

	// Draw peak chart...
	const QColor& fg = track()->foreground();
	int h1 = (clipRect.height() / iChannels);
	int h2 = (h1 >> 1);
	int h2gain = (h2 * m_fractGain.num); 
	int ymax, ymin, yrms;
	unsigned int n, n2;
	int x, y;

	// Build polygonal vertexes...
	if (bZoomedIn) {
		// Zoomed in...
		// - trade peak-frames for pixels.
		n2 = nframes - 1;
		for (n = 0; n < nframes; ++n) {
			x = clipRect.x() + (n * clipRect.width()) / n2;
			y = clipRect.y() + h2;
			for (i = 0; i < iChannels; ++i) {
				ymax = (h2gain * pframes->max) >> m_fractGain.den;
				ymin = (h2gain * pframes->min) >> m_fractGain.den;
				yrms = (h2gain * pframes->rms) >> m_fractGain.den;
				pPolyMax[i]->setPoint(n, x, y - ymax);
				pPolyMax[i]->setPoint(iPolyPoints - n - 1, x, y + ymin);
				pPolyRms[i]->setPoint(n, x, y - yrms);
				pPolyRms[i]->setPoint(iPolyPoints - n - 1, x, y + yrms);
				y += h1; ++pframes;
			}
		}
	} else {
		// Zoomed out...
		// - trade (2) pixels for peak-frames (expensiver).
		int x2, k = 0;
		unsigned int n0 = 0;
		unsigned char v, vmax, vmin, vrms;
		for (x2 = 0; x2 < clipRect.width(); x2 += 2) {
			x = clipRect.x() + x2;
			y = clipRect.y() + h2;
			n = (iChannels * x2 * nframes) / clipRect.width();
			for (i = 0; i < iChannels; ++i) {
				vmax = pframes[n + i].max;
				vmin = pframes[n + i].min;
				vrms = pframes[n + i].rms;;
				for (n2 = n0 + i; n2 < n + i; n2 += iChannels) {
					v = pframes[n2].max;
					if (vmax < v)
						vmax = v;
					v = pframes[n2].min;
					if (vmin < v)
						vmin = v;
					v = pframes[n2].rms;
					if (vrms < v)
						vrms = v;
				}
				ymax = (h2gain * vmax) >> m_fractGain.den;
				ymin = (h2gain * vmin) >> m_fractGain.den;
				yrms = (h2gain * vrms) >> m_fractGain.den;
				pPolyMax[i]->setPoint(k, x, y - ymax);
				pPolyMax[i]->setPoint(iPolyPoints - k - 1, x, y + ymin);
				pPolyRms[i]->setPoint(k, x, y - yrms);
				pPolyRms[i]->setPoint(iPolyPoints - k - 1, x, y + yrms);
				y += h1;
			}
			n0 = n + iChannels;
			++k;
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

	// Done on polygons.
	delete [] pPolyMax;
	delete [] pPolyRms;
}


// Audio clip tool-tip.
QString qtractorAudioClip::toolTip (void) const
{
	QString sToolTip = qtractorClip::toolTip();

	qtractorAudioBuffer *pBuff = buffer();
	if (pBuff) {
		qtractorAudioFile *pFile = pBuff->file();
		if (pFile) {
			sToolTip += QObject::tr("\nAudio:\t%1 channels, %2 Hz")
				.arg(pFile->channels())
				.arg(pFile->sampleRate());
			float fGain = clipGain();
			if (fGain < 0.999f || fGain > 1.001f)
				sToolTip += QObject::tr(" (%1 dB)")
					.arg(20.0f * ::log10f(fGain), 0, 'g', 2);
			if (pBuff->isTimeStretch())
				sToolTip += QObject::tr("\n\t(%1% time stretch)")
					.arg(100.0f * pBuff->timeStretch(), 0, 'g', 3);
			if (pBuff->isPitchShift())
				sToolTip += QObject::tr("\n\t(%1 semitones pitch shift)")
					.arg(12.0f * ::logf(pBuff->pitchShift()) / M_LN2, 0, 'g', 2);
		}
	}

	return sToolTip;
}


// Virtual document element methods.
bool qtractorAudioClip::loadClipElement (
	qtractorDocument * /* pDocument */, QDomElement *pElement )
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
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	QDomElement eAudioClip = pDocument->document()->createElement("audio-clip");
	pDocument->saveTextElement("filename",
		qtractorAudioClip::relativeFilename(pDocument), &eAudioClip);
	pDocument->saveTextElement("time-stretch",
		QString::number(qtractorAudioClip::timeStretch()), &eAudioClip);
	pDocument->saveTextElement("pitch-shift",
		QString::number(qtractorAudioClip::pitchShift()), &eAudioClip);
	pElement->appendChild(eAudioClip);

	return true;
}


// Audio clip export method.
bool qtractorAudioClip::clipExport ( ClipExport pfnClipExport, void *pvArg,
	unsigned long iOffset, unsigned long iLength ) const
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	unsigned short iChannels = 0;
	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (pTrack->outputBus());
	if (pAudioBus)
		iChannels = pAudioBus->channels();
	if (iChannels < 1)
		return false;

	iOffset += clipOffset();
	if (iLength < 1)
		iLength = clipLength();

	qtractorAudioBuffer *pBuff = new qtractorAudioBuffer(
		pTrack->syncThread(), iChannels, pSession->sampleRate());
	pBuff->setOffset(iOffset);
	pBuff->setLength(iLength);
	pBuff->setTimeStretch(timeStretch());
	pBuff->setPitchShift(pitchShift());

	if (!pBuff->open(filename())) {
		delete pBuff;
		return false;
	}

	unsigned short i;
	unsigned int iFrames = pBuff->bufferSize();
	float **ppFrames = new float * [iChannels];
	for (i = 0; i < iChannels; ++i) {
		ppFrames[i] = new float[iFrames];
		::memset(ppFrames[i], 0, iFrames * sizeof(float));
	}

	float fGain = clipGain();
	unsigned long iFrameStart = 0;
	while (iFrameStart < iLength) {
		pBuff->syncExport();
		if (pBuff->inSync(iFrameStart, iFrameStart + iFrames)) {
			int nread = pBuff->read(ppFrames, iFrames);
			if (nread < 1)
				break;
			for (i = 0; i < iChannels; ++i) {
				float *pFrames = ppFrames[i];
				for (int n = 0; n < nread; ++n)
					*pFrames++ *= fGain;
			}
			(*pfnClipExport)(ppFrames, nread, pvArg);
			iFrameStart += nread;
		}
	}

	for (i = 0; i < iChannels; ++i)
		delete ppFrames[i];
	delete [] ppFrames;
	delete pBuff;

	return true;
}


// end of qtractorAudioClip.cpp
