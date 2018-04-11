// qtractorAudioClip.cpp
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
		m_fClipGain = pAudioClip->clipGain();
		m_fClipPanning = pAudioClip->clipPanning();
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
	float clipGain() const
		{ return m_fClipGain; }
	float clipPanning() const
		{ return m_fClipPanning; }
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
			&& m_fClipGain    == other.clipGain()
			&& m_fClipPanning == other.clipPanning()
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
	float          m_fClipGain;
	float          m_fClipPanning;
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
		 ^ qHash(long(1000.0f * key.clipGain()))
		 ^ qHash(long(1000.0f * key.clipPanning()))
		 ^ qHash(long(1000.0f * key.timeStretch()))
		 ^ qHash(long(1000.0f * key.pitchShift()))
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

	m_bWsolaTimeStretch = qtractorAudioBuffer::isDefaultWsolaTimeStretch();
	m_bWsolaQuickSeek = qtractorAudioBuffer::isDefaultWsolaQuickSeek();

	m_iOverlap = 0;

	m_pFractGains = NULL;
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

	m_bWsolaTimeStretch = clip.isWsolaTimeStretch();
	m_bWsolaQuickSeek   = clip.isWsolaQuickSeek();

	m_iOverlap = clip.overlap();

	m_pFractGains = NULL;

	setFilename(clip.filename());
	setClipName(clip.clipName());
	setClipGain(clip.clipGain());
	setClipPanning(clip.clipPanning());

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


// Alternating overlap test.
bool qtractorAudioClip::isOverlap ( unsigned int iOverlapSize ) const
{
	if (m_pData == NULL)
		return false;

	const unsigned long iClipStart = clipStart();
	const unsigned long iClipEnd = iClipStart + clipLength() + iOverlapSize;

	QListIterator<qtractorAudioClip *> iter(m_pData->clips());
	while (iter.hasNext()) {
		qtractorAudioClip *pClip = iter.next();
		const unsigned long iClipStart2 = pClip->clipStart();
		const unsigned long iClipEnd2
			= iClipStart2 + pClip->clipLength() + iOverlapSize;
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
	const bool bWrite = (iMode & qtractorAudioFile::Write);
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
	const bool bFilenameChanged = (sFilename != filename());

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
			const unsigned int iOverlapSize
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
				if (m_pPeak == NULL || bFilenameChanged) {
					qtractorAudioPeakFactory *pPeakFactory
						= pSession->audioPeakFactory();
					if (pPeakFactory) {
						if (m_pPeak)
							delete m_pPeak;
						m_pPeak = pPeakFactory->createPeak(
							sFilename, pBuff->timeStretch());
					}
				}
				// Gain/panning fractionalizer(tm)...
				updateFractGains(pBuff);
				// Clip name should be clear about it all.
				if (clipName().isEmpty())
					setClipName(shortClipName(QFileInfo(filename()).baseName()));
				return true;
			}
		}
	}

	// Initialize audio buffer container...
	m_pData = new Data(pTrack, iChannels);
	m_pData->attach(this);

	qtractorAudioBuffer *pBuff = m_pData->buffer();

	pBuff->setOffset(clipOffset());
	pBuff->setLength(clipLength());
	pBuff->setGain(clipGain());
	pBuff->setPanning(clipPanning());
	pBuff->setTimeStretch(m_fTimeStretch);
	pBuff->setPitchShift(m_fPitchShift);
	pBuff->setWsolaTimeStretch(m_bWsolaTimeStretch);
	pBuff->setWsolaQuickSeek(m_bWsolaQuickSeek);

	if (!pBuff->open(sFilename, iMode)) {
		delete m_pData;
		m_pData = NULL;
		return false;
	}

	// Gain/panning fractionalizer(tm)...
	updateFractGains(pBuff);

	// Default clip length will be the whole file length.
	if (clipLength() == 0)
		setClipLength(pBuff->length() - pBuff->offset());

	// Peak files should also be created on-the-fly?
	if (m_pPeak == NULL || bFilenameChanged) {
		qtractorAudioPeakFactory *pPeakFactory
			= pSession->audioPeakFactory();
		if (pPeakFactory) {
			if (m_pPeak)
				delete m_pPeak;
			m_pPeak = pPeakFactory->createPeak(
				sFilename, pBuff->timeStretch());
			if (bWrite)
				pBuff->setPeakFile(m_pPeak->peakFile());
		}
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

	if (m_pFractGains) {
		delete [] m_pFractGains;
		m_pFractGains = NULL;
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

	Data *pNewData = new Data(track(), pBuff->channels());

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


// Gain/panning fractionalizer(tm)...
void qtractorAudioClip::updateFractGains ( qtractorAudioBuffer *pBuff )
{
	if (m_pFractGains) {
		delete [] m_pFractGains;
		m_pFractGains = NULL;
	}

	const unsigned short iChannels = pBuff->channels();
	m_pFractGains = new FractGain [iChannels];
	for (unsigned short i = 0; i < iChannels; ++i) {
		FractGain& fractGain = m_pFractGains[i];
		fractGain.num = 1;
		fractGain.den = 8;
		float fGain = clipGain() * pBuff->channelGain(i);
		while(fGain != int(fGain) && fractGain.den < 20) {
			fractGain.den += 2;
			fGain *= 4.0f;
		}
		fractGain.num = int(fGain);
	}
}


// Direct write method.
void qtractorAudioClip::write ( float **ppBuffer,
	unsigned int iFrames, unsigned short iChannels, unsigned int iOffset )
{
	if (m_pData) m_pData->write(ppBuffer, iFrames, iChannels, iOffset);
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
		if (m_pPeak && pBuff->peakFile() == NULL) {
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
	const QString sFilename(filename());
	openAudioFile(sFilename);
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
	const unsigned long iClipStart = clipStart();
	if (iClipStart > iFrameEnd)
		return;

	const unsigned long iClipEnd = iClipStart + clipLength();
	if (iClipEnd < iFrameStart)
		return;

	const unsigned long iOffset
		= (iFrameEnd < iClipEnd ? iFrameEnd : iClipEnd) - iClipStart;

	if (iClipStart > iFrameStart) {
		if (pBuff->inSync(0, iOffset)) {
			pBuff->readMix(
				pAudioBus->buffer(),
				iOffset,
				pAudioBus->channels(),
				iClipStart - iFrameStart,
				fadeInOutGain(iOffset));
		}
	} else {
		if (pBuff->inSync(iFrameStart - iClipStart, iOffset)) {
			pBuff->readMix(
				pAudioBus->buffer(),
				(iFrameEnd < iClipEnd ? iFrameEnd : iClipEnd) - iFrameStart,
				pAudioBus->channels(),
				0,
				fadeInOutGain(iOffset));
		}
	}
}


// Audio clip freewheeling process cycle executive (needed for export).
void qtractorAudioClip::process_export (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
	// Direct sync method.
	if (m_pData) m_pData->syncExport();

	// Normal clip processing...
	process(iFrameStart, iFrameEnd);
}


// Audio clip paint method.
void qtractorAudioClip::draw (
	QPainter *pPainter, const QRect& clipRect, unsigned long iClipOffset )
{
	qtractorSession *pSession = track()->session();
	if (pSession == NULL)
		return;

	// Cache some peak data...
	if (m_pPeak == NULL)
		return;

	const unsigned long iFrameOffset = iClipOffset + clipOffset();
	const int x0 = pSession->pixelFromFrame(iClipOffset);
	const unsigned long iFrameLength
		= pSession->frameFromPixel(x0 + clipRect.width()) - iClipOffset;

	// Grab them in...
	qtractorAudioPeakFile::Frame *pPeakFrames
		= m_pPeak->peakFrames(iFrameOffset, iFrameLength, clipRect.width());
	if (pPeakFrames == NULL)
		return;

	// Make some expectations...
	const unsigned int iPeakLength	= m_pPeak->peakLength();
	if (iPeakLength < 1)
		return;

	// Polygon init...
	unsigned short k;
	const unsigned short iChannels = m_pPeak->channels();
	const unsigned int iPolyPoints = (iPeakLength << 1);
	QPolygon **pPolyMax = new QPolygon* [iChannels];
	QPolygon **pPolyRms = new QPolygon* [iChannels];
	for (k = 0; k < iChannels; ++k) {
		pPolyMax[k] = new QPolygon(iPolyPoints);
		pPolyRms[k] = new QPolygon(iPolyPoints);
	}

	// Draw peak chart...
	const int h1 = (clipRect.height() / iChannels);
	const int h2 = (h1 >> 1);

	int x, y, ymax, ymin, yrms;

	// Build polygonal vertexes...
	const int n2 = int(iPeakLength);
	for (int n = 0; n < n2; ++n) {
		x = clipRect.x() + (n * clipRect.width()) / n2;
		y = clipRect.y() + h2;
		for (k = 0; k < iChannels; ++k) {
			const FractGain& fractGain = m_pFractGains[k];
			const int h2gain = (h2 * fractGain.num);
			ymax = (h2gain * pPeakFrames->max) >> fractGain.den;
			ymin = (h2gain * pPeakFrames->min) >> fractGain.den;
			yrms = (h2gain * pPeakFrames->rms) >> fractGain.den;
			pPolyMax[k]->setPoint(n, x, y - ymax);
			pPolyMax[k]->setPoint(iPolyPoints - n - 1, x, y + ymin);
			pPolyRms[k]->setPoint(n, x, y - yrms);
			pPolyRms[k]->setPoint(iPolyPoints - n - 1, x, y + yrms);
			y += h1; ++pPeakFrames;
		}
	}

	// Close, draw and free the polygons...
	QColor fg(track()->foreground());
	fg.setAlpha(200);
	pPainter->setPen(fg.lighter(140));
	pPainter->setBrush(fg);
	for (k = 0; k < iChannels; ++k) {
		pPainter->drawPolygon(*pPolyMax[k]);
		pPainter->drawPolygon(*pPolyRms[k]);
		delete pPolyRms[k];
		delete pPolyMax[k];
	}

	// Done on polygons.
	delete [] pPolyRms;
	delete [] pPolyMax;
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
			const float fGain = clipGain();
			if (fGain < 0.999f || fGain > 1.001f)
				sToolTip += QObject::tr(" (%1 dB)")
					.arg(20.0f * ::log10f(fGain), 0, 'g', 2);
			const float fPanning = clipPanning();
			if (fPanning < -0.001f || fPanning > +0.001f)
				sToolTip += QObject::tr(" (%1 pan)")
					.arg(fPanning, 0, 'g', 1);
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
	qtractorDocument */*pDocument*/, QDomElement *pElement )
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
		else if (eChild.tagName() == "wsola-time-stretch")
			qtractorAudioClip::setWsolaTimeStretch(
				qtractorDocument::boolFromText(eChild.text()));
		else if (eChild.tagName() == "wsola-quick-seek")
			qtractorAudioClip::setWsolaQuickSeek(
				qtractorDocument::boolFromText(eChild.text()));

	}

	return true;
}


bool qtractorAudioClip::saveClipElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	QDomElement eAudioClip = pDocument->document()->createElement("audio-clip");
	pDocument->saveTextElement("filename",
		qtractorAudioClip::relativeFilename(pDocument), &eAudioClip);
	pDocument->saveTextElement("time-stretch",
		QString::number(qtractorAudioClip::timeStretch()), &eAudioClip);
	pDocument->saveTextElement("pitch-shift",
		QString::number(qtractorAudioClip::pitchShift()), &eAudioClip);
	pDocument->saveTextElement("wsola-time-stretch",
		qtractorDocument::textFromBool(
			qtractorAudioClip::isWsolaTimeStretch()), &eAudioClip);
	pDocument->saveTextElement("wsola-quick-seek",
		qtractorDocument::textFromBool(
			qtractorAudioClip::isWsolaQuickSeek()), &eAudioClip);
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

	qtractorAudioBuffer *pBuff
		= new qtractorAudioBuffer(pTrack->syncThread(), iChannels);

	pBuff->setOffset(iOffset);
	pBuff->setLength(iLength);
	pBuff->setTimeStretch(timeStretch());
	pBuff->setPitchShift(pitchShift());

	if (!pBuff->open(filename())) {
		delete pBuff;
		return false;
	}

	unsigned short i;

	const unsigned int iFrames = pBuff->bufferSize();
	float **ppFrames = new float * [iChannels];
	for (i = 0; i < iChannels; ++i) {
		ppFrames[i] = new float[iFrames];
		::memset(ppFrames[i], 0, iFrames * sizeof(float));
	}

	unsigned long iFrameStart = 0;
	while (iFrameStart < iLength) {
		pBuff->syncExport();
		if (pBuff->inSync(iFrameStart, iFrameStart + iFrames)) {
			const int nread = pBuff->read(ppFrames, iFrames);
			if (nread < 1)
				break;
			for (i = 0; i < iChannels; ++i) {
				const float fGain = clipGain() * pBuff->channelGain(i);
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
