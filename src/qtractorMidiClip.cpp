// qtractorMidiClip.cpp
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
#include "qtractorMidiClip.h"
#include "qtractorMidiEngine.h"

#include "qtractorSession.h"
#include "qtractorFileList.h"

#include "qtractorDocument.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditorForm.h"

#include "qtractorMainForm.h"

#ifdef QTRACTOR_MIDI_EDITOR_TOOL
#include "qtractorOptions.h"
#endif

#include <QMessageBox>
#include <QFileInfo>
#include <QPainter>

#include <QDomDocument>


#if QT_VERSION < 0x040500
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif


//----------------------------------------------------------------------
// class qtractorMidiClip::Key -- MIDI sequence clip (hash key).
//
class qtractorMidiClip::Key
{
public:

	// Constructor.
	Key(qtractorMidiClip *pMidiClip)
		{ update(pMidiClip); }

	// Key settler.
	void update(qtractorMidiClip *pMidiClip)
	{
		qtractorTrack *pTrack = pMidiClip->track();
		m_sFilename = pMidiClip->filename();;
		m_iClipOffset = pMidiClip->clipOffset();
		m_iClipLength = pMidiClip->clipLength();
		m_iTrackChannel = pMidiClip->trackChannel();
		m_iMidiChannel = (pTrack ? pTrack->midiChannel() : 0);
	}

	// Key accessors.
	const QString& filename() const
		{ return m_sFilename; }
	unsigned long clipOffset() const
		{ return m_iClipOffset; }
	unsigned long clipLength() const
		{ return m_iClipLength; }
	unsigned short trackChannel() const
		{ return m_iTrackChannel; }
	unsigned short midiChannel() const
		{ return m_iMidiChannel; }

	// Match descriminator.
	bool operator== (const Key& other) const
	{
		return m_sFilename     == other.filename()
			&& m_iClipOffset   == other.clipOffset()
			&& m_iClipLength   == other.clipLength()
			&& m_iTrackChannel == other.trackChannel()
			&& m_iMidiChannel  == other.midiChannel();
	}

private:

	// Interesting variables.
	QString        m_sFilename;
	unsigned long  m_iClipOffset;
	unsigned long  m_iClipLength;
	unsigned short m_iTrackChannel;
	unsigned short m_iMidiChannel;
};


uint qHash ( const qtractorMidiClip::Key& key )
{
	return qHash(key.filename())
		 ^ qHash(key.clipOffset())
		 ^ qHash(key.clipLength())
		 ^ qHash(key.trackChannel())
		 ^ qHash(key.midiChannel());
}


qtractorMidiClip::Hash qtractorMidiClip::g_hashTable;


//----------------------------------------------------------------------
// class qtractorMidiClip::FileKey -- MIDI file hash key.
//
class qtractorMidiClip::FileKey
{
public:

	// Constructor.
	FileKey(qtractorMidiClip::Key *pKey) :
		m_sFilename(pKey->filename()),
		m_iTrackChannel(pKey->trackChannel()) {}

	// Key accessors.
	const QString& filename() const
		{ return m_sFilename; }
	unsigned short trackChannel() const
		{ return m_iTrackChannel; }

	// Match descriminator.
	bool operator== (const FileKey& other) const
	{
		return m_sFilename     == other.filename()
			&& m_iTrackChannel == other.trackChannel();
	}

private:

	// Interesting variables.
	QString        m_sFilename;
	unsigned short m_iTrackChannel;
};


uint qHash ( const qtractorMidiClip::FileKey& key )
{
	return qHash(key.filename()) ^ qHash(key.trackChannel());
}


qtractorMidiClip::FileHash qtractorMidiClip::g_hashFiles;


//----------------------------------------------------------------------
// class qtractorMidiClip -- MIDI sequence clip.
//

// Constructor.
qtractorMidiClip::qtractorMidiClip ( qtractorTrack *pTrack )
	: qtractorClip(pTrack)
{
	m_pFile = NULL;
	m_pKey  = NULL;
	m_pData = NULL;

	m_iTrackChannel = 0;
	m_iFormat = defaultFormat();
	m_bSessionFlag = false;
	m_iRevision = 0;

	m_noteMin = 0;
	m_noteMax = 0;

	m_pMidiEditorForm = NULL;
}

// Copy constructor.
qtractorMidiClip::qtractorMidiClip ( const qtractorMidiClip& clip )
	: qtractorClip(clip.track())
{
	m_pFile = NULL;
	m_pKey  = NULL;
	m_pData = NULL;

	setFilename(clip.filename());
	setTrackChannel(clip.trackChannel());
	setClipGain(clip.clipGain());
	setClipName(clip.clipName());

	m_iFormat = clip.format();
	m_bSessionFlag = false;
	m_iRevision = clip.revision();

	m_noteMin = clip.noteMin();
	m_noteMax = clip.noteMax();

	m_pMidiEditorForm = NULL;
}


// Destructor.
qtractorMidiClip::~qtractorMidiClip (void)
{
	close();

	closeMidiFile();
}


// Brand new clip contents new method.
bool qtractorMidiClip::createMidiFile (
	const QString& sFilename, int iTrackChannel )
{
	closeMidiFile();

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiClip[%p]::createMidiFile(\"%s\", %d)", this,
		sFilename.toUtf8().constData(), iTrackChannel);
#endif

	// Self holds the SMF format,
	const unsigned short iFormat = format();

	// Which SMF format?
	unsigned short iTracks = 1;
	if (iFormat == 0) {
		// SMF format 0 (1 track, 1 channel)
		iTrackChannel = pTrack->midiChannel();
	} else {
		// SMF format 1 (2 tracks, 1 channel)
		iTrackChannel = 1;
		++iTracks;
	}

	// Set local properties...
	setFilename(sFilename);
	setTrackChannel(iTrackChannel);
	setDirty(false);

	// Register file path...
	pSession->files()->addClipItem(qtractorFileList::Midi, this, true);

	// Create and open up the MIDI file...
	m_pFile = new qtractorMidiFile();
	if (!m_pFile->open(sFilename, qtractorMidiFile::Write)) {
		delete m_pFile;
		m_pFile = NULL;
		return false;
	}

	// Initialize MIDI event container...
	m_pKey  = new Key(this);
	m_pData = new Data();
	m_pData->attach(this);

	// Right on then...
	insertHashKey();

	qtractorMidiSequence *pSeq = m_pData->sequence();

	pSeq->clear();
	pSeq->setTicksPerBeat(pSession->ticksPerBeat());
	pSeq->setName(shortClipName(QFileInfo(sFilename).baseName()));
	pSeq->setChannel(pTrack->midiChannel());

	// Make it a brand new revision...
	setRevision(1);

	// Write SMF header...
	if (m_pFile->writeHeader(iFormat, iTracks, pSeq->ticksPerBeat())) {
		// Set initial local properties...
		if (m_pFile->tempoMap()) {
			m_pFile->tempoMap()->fromTimeScale(
				pSession->timeScale(),
				pSession->tickFromFrame(clipStart()));
		}
		// Sure this is a brand new file...
		if (iFormat == 1)
			m_pFile->writeTrack(NULL);
		m_pFile->writeTrack(pSeq);
		m_pFile->close();
	}

	// It's there now.
	delete m_pFile;
	m_pFile = NULL;

	// Clip name should be clear about it all.
	if (clipName().isEmpty())
		setClipName(pSeq->name());
	if (clipName().isEmpty())
		setClipName(shortClipName(QFileInfo(filename()).baseName()));

	// Uh oh...
	m_playCursor.reset(pSeq);
	m_drawCursor.reset(pSeq);

	return true;
}


// The main use method.
bool qtractorMidiClip::openMidiFile (
	const QString& sFilename, int iTrackChannel, int iMode )
{
	closeMidiFile();

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiClip[%p]::openMidiFile(\"%s\", %d, %d)", this,
		sFilename.toUtf8().constData(), iTrackChannel, iMode);
#endif

	// Check file primordial state...
	const bool bWrite = (iMode & qtractorMidiFile::Write);

	// Set local properties...
	setFilename(sFilename);
	setTrackChannel(iTrackChannel);
	setDirty(false);

	// Register file path...
	pSession->files()->addClipItem(qtractorFileList::Midi, this, bWrite);

	// New key-data sequence...
	if (!bWrite) {
		m_pKey  = new Key(this);
		m_pData = g_hashTable.value(*m_pKey, NULL);
		if (m_pData) {
			m_pData->attach(this);
			qtractorMidiSequence *pSeq = m_pData->sequence();
			// Clip name should be clear about it all.
			if (clipName().isEmpty())
				setClipName(pSeq->name());
			if (clipName().isEmpty())
				setClipName(shortClipName(QFileInfo(filename()).baseName()));
			// Uh oh...
			m_playCursor.reset(pSeq);
			m_drawCursor.reset(pSeq);
			return true;
		}
	}

	// Create and open up the real MIDI file...
	m_pFile = new qtractorMidiFile();
	if (!m_pFile->open(sFilename, iMode)) {
		delete m_pFile;
		m_pFile = NULL;
		return false;
	}

	// Initialize MIDI event container...
	m_pData = new Data();
	m_pData->attach(this);

	qtractorMidiSequence *pSeq = m_pData->sequence();

	pSeq->clear();
	pSeq->setTicksPerBeat(pSession->ticksPerBeat());

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(clipStart());
	const unsigned long t0 = pNode->tickFromFrame(clipStart());

	if (clipStart() > clipOffset()) {
		const unsigned long iOffset = clipStart() - clipOffset();
		pNode = cursor.seekFrame(iOffset);
		pSeq->setTimeOffset(t0 - pNode->tickFromFrame(iOffset));
	} else {
		pNode = cursor.seekFrame(clipOffset());
		pSeq->setTimeOffset(pNode->tickFromFrame(clipOffset()));
	}

	const unsigned long iClipEnd = clipStart() + clipLength();
	pNode = cursor.seekFrame(iClipEnd);
	pSeq->setTimeLength(pNode->tickFromFrame(iClipEnd) - t0);

	// Initial statistics...
	pSeq->setNoteMin(m_noteMin);
	pSeq->setNoteMax(m_noteMax);

	// Are we on a pre-writing status?
	if (bWrite) {
		// On write mode, iTrackChannel holds the SMF format,
		// so we'll convert it here as properly.
		unsigned short iFormat = 0;
		unsigned short iTracks = 1;
		if (iTrackChannel == 0) {
			// SMF format 0 (1 track, 1 channel)
			iTrackChannel = pTrack->midiChannel();
		} else {
			// SMF format 1 (2 tracks, 1 channel)
			iTrackChannel = 1;
			iFormat = 1;
			++iTracks;
		}
		// That's it.
		setFormat(iFormat);
		// Write SMF header...
		if (m_pFile->writeHeader(iFormat, iTracks, pSeq->ticksPerBeat())) {
			// Set initial local properties...
			if (m_pFile->tempoMap()) {
				m_pFile->tempoMap()->fromTimeScale(
					pSession->timeScale(), pSeq->timeOffset());
			}
		}
		// And initial clip name...
		pSeq->setName(shortClipName(QFileInfo(m_pFile->filename()).baseName()));
		pSeq->setChannel(pTrack->midiChannel());
		// Nothing more as for writing...
	} else {
		// On read mode, SMF format is properly given by open file.
		setFormat(m_pFile->format());
		// Read the event sequence in...
		m_pFile->readTrack(pSeq, iTrackChannel);
		// For immediate feedback, once...
		m_noteMin = pSeq->noteMin();
		m_noteMax = pSeq->noteMax();
		// FIXME: On demand, set session time properties from MIDI file...
		if (m_bSessionFlag) {
		#if 0
			// Import eventual SysEx setup...
			// - take care that given track might not be currently open,
			//   so that we'll resolve MIDI output bus somehow...
			qtractorMidiBus *pMidiBus = NULL;
			qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
			if (pMidiEngine) {
				pMidiBus = static_cast<qtractorMidiBus *> (
					pMidiEngine->findOutputBus(pTrack->outputBusName()));
				if (pMidiBus == NULL) {
					for (qtractorBus *pBus = pMidiEngine->buses().first();
							pBus; pBus = pBus->next()) {
						if (pBus->busMode() & qtractorBus::Output) {
							pMidiBus = static_cast<qtractorMidiBus *> (pBus);
							break;
						}
					}
				}
			}
			// Import eventual SysEx setup...
			if (pMidiBus)
				pMidiBus->importSysexList(pSeq);
		#endif
			// Import tempo map as well...
			if (m_pFile->tempoMap()) {
				m_pFile->tempoMap()->intoTimeScale(pSession->timeScale(), t0);
				pSession->updateTimeScale();
			}
			// Reset session flag now.
			m_bSessionFlag = false;
		}
		// We should have events, otherwise this clip is of no use...
		//if (m_pSeq->events().count() < 1)
		//	return false;
	}

	// Make it a brand new revision...
	// setRevision(1);

	// Default clip length will be whole sequence duration.
	if (clipLength() == 0 && pSeq->timeLength() > pSeq->timeOffset()) {
		const unsigned long t1
			= t0 + (pSeq->timeLength() - pSeq->timeOffset());
		pNode = cursor.seekTick(t1);
		setClipLength(pNode->frameFromTick(t1) - clipStart());
	}

	// Clip name should be clear about it all.
	if (clipName().isEmpty())
		setClipName(pSeq->name());
	if (clipName().isEmpty())
		setClipName(shortClipName(QFileInfo(filename()).baseName()));

	// Uh oh...
	m_playCursor.reset(pSeq);
	m_drawCursor.reset(pSeq);

	// Something might have changed...
	updateHashKey();
	insertHashKey();

	return true;
}


// Private cleanup.
void qtractorMidiClip::closeMidiFile (void)
{
	if (m_pMidiEditorForm) {
		m_pMidiEditorForm->close();
		delete m_pMidiEditorForm;
		m_pMidiEditorForm = NULL;
	}

	if (m_pData) {
		m_pData->detach(this);
		if (m_pData->count() < 1) {
			removeHashKey();
			delete m_pData;
		}
		m_pData = NULL;
		// Unregister file path...
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->files()->removeClipItem(qtractorFileList::Midi, this);

	}

	if (m_pKey) {
		delete m_pKey;
		m_pKey = NULL;
	}

	if (m_pFile) {
		delete m_pFile;
		m_pFile = NULL;
	}
}


// Revisionist method.
QString qtractorMidiClip::createFilePathRevision ( bool bForce )
{
	QString sFilename = filename();

	// Check file-hash reference...
	if (m_iRevision > 0 && m_pKey) {
		FileKey fkey(m_pKey);
		FileHash::ConstIterator fiter = g_hashFiles.constFind(fkey);
		if (fiter != g_hashFiles.constEnd() && fiter.value() > 1)
			m_iRevision = 0;
	}

	if (m_iRevision == 0 || bForce) {
		qtractorTrack *pTrack = track();
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pTrack && pSession)
			sFilename = pSession->createFilePath(pTrack->trackName(), "mid");
		sFilename = qtractorMidiFile::createFilePathRevision(sFilename);
	#ifdef CONFIG_DEBUG
		qDebug("qtractorMidiClip::createFilePathRevision(%d): \"%s\" (%d)",
			int(bForce), sFilename.toUtf8().constData(), m_iRevision);
	#endif
		m_iRevision = 0;
	}

	++m_iRevision;

	return sFilename;
}


// Sync all ref-counted filenames.
void qtractorMidiClip::setFilenameEx (
	const QString& sFilename, bool bUpdate )
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return;

	if (m_pData == NULL)
		return;

	removeHashKey();

	QListIterator<qtractorMidiClip *> iter(m_pData->clips());
	while (iter.hasNext()) {
		qtractorMidiClip *pMidiClip = iter.next();
		pSession->files()->removeClipItem(qtractorFileList::Midi, pMidiClip);
		pMidiClip->setFilename(sFilename);
		pMidiClip->updateHashKey();
		pSession->files()->addClipItem(qtractorFileList::Midi, pMidiClip, true);
		if (bUpdate) {
			pMidiClip->setDirty(false);
			pMidiClip->updateEditor(true);
		}
	}

	insertHashKey();
}


// Sync all ref-counted clip-lengths.
void qtractorMidiClip::setClipLengthEx ( unsigned long iClipLength )
{
	if (m_pData == NULL)
		return;

	removeHashKey();

	QListIterator<qtractorMidiClip *> iter(m_pData->clips());
	while (iter.hasNext()) {
		qtractorMidiClip *pMidiClip = iter.next();
		pMidiClip->setClipLength(iClipLength);
		pMidiClip->updateHashKey();
	}

	insertHashKey();
}


// Sync all ref-counted clip editors.
void qtractorMidiClip::updateEditorEx ( bool bSelectClear )
{
	if (m_pData == NULL)
		return;

	QListIterator<qtractorMidiClip *> iter(m_pData->clips());
	while (iter.hasNext())
		iter.next()->updateEditor(bSelectClear);
}

void qtractorMidiClip::resetEditorEx ( bool bSelectClear )
{
	if (m_pData == NULL)
		return;

	QListIterator<qtractorMidiClip *> iter(m_pData->clips());
	while (iter.hasNext())
		iter.next()->resetEditor(bSelectClear);
}


// Sync all ref-counted clip-dirtyness.
void qtractorMidiClip::setDirtyEx ( bool bDirty )
{
	if (m_pData == NULL)
		return;

	QListIterator<qtractorMidiClip *> iter(m_pData->clips());
	while (iter.hasNext())
		iter.next()->setDirty(bDirty);
}


// Manage local hash key.
void qtractorMidiClip::insertHashKey (void)
{
	if (m_pKey) {
		// Increment file-hash reference...
		FileKey fkey(m_pKey);
		FileHash::Iterator fiter = g_hashFiles.find(fkey);
		if (fiter == g_hashFiles.end())
			fiter =  g_hashFiles.insert(fkey, 0);
		++fiter.value();
		// Insert actual clip-hash reference....
		g_hashTable.insert(*m_pKey, m_pData);
	}
}


void qtractorMidiClip::updateHashKey (void)
{
	if (m_pKey == NULL)
		m_pKey = new Key(this);
	else
		m_pKey->update(this);
}


void qtractorMidiClip::removeHashKey (void)
{
	if (m_pKey) {
		// Decrement file-hash reference...
		FileKey fkey(m_pKey);
		FileHash::Iterator fiter = g_hashFiles.find(fkey);
		if (fiter != g_hashFiles.end()) {
			if (--fiter.value() < 1)
				g_hashFiles.remove(fkey);
		}
		// Remove actual clip-hash reference....
		g_hashTable.remove(*m_pKey);
	}
}


// Unlink (clone) local hash data.
void qtractorMidiClip::unlinkHashData (void)
{
	if (m_pData == NULL)
		return;
	if (m_pData->count() < 2)
		return;

	m_pData->detach(this);

	Data *pNewData = new Data();

	qtractorMidiSequence *pOldSeq = m_pData->sequence();
	qtractorMidiSequence *pNewSeq = pNewData->sequence();

	pNewSeq->setName(pOldSeq->name());
	pNewSeq->setChannel(pOldSeq->channel());
	pNewSeq->setBank(pOldSeq->bank());
	pNewSeq->setProg(pOldSeq->prog());
	pNewSeq->setTicksPerBeat(pOldSeq->ticksPerBeat());
	pNewSeq->setTimeOffset(pOldSeq->timeOffset());
	pNewSeq->setTimeLength(pOldSeq->timeLength());
	pNewSeq->setDuration(pOldSeq->duration());
	pNewSeq->setNoteMin(pOldSeq->noteMin());
	pNewSeq->setNoteMax(pOldSeq->noteMax());
	pNewSeq->copyEvents(pOldSeq);

	m_pData = pNewData;
	m_pData->attach(this);

	updateHashKey();
	insertHashKey();
}


// Relink local hash data.
void qtractorMidiClip::relinkHashData (void)
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
bool qtractorMidiClip::isHashLinked (void) const
{
	return (m_pData && m_pData->count() > 1);
}


// Make sure the clip hash-table gets reset.
void qtractorMidiClip::clearHashTable (void)
{
	g_hashTable.clear();
	g_hashFiles.clear();
}


// Intra-clip playback frame positioning.
void qtractorMidiClip::seek ( unsigned long iFrame )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiClip[%p]::seek(%lu)", this, iFrame);
#endif

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return;

	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == NULL)
		return;

	const unsigned long t0 = pSession->tickFromFrame(clipStart());
	const unsigned long t1 = pSession->tickFromFrame(iFrame);

	// Seek for the nearest sequence event...
	m_playCursor.seek(pSeq, (t1 > t0 ? t1 - t0 : 0));
}


// Reset clip state.
void qtractorMidiClip::reset ( bool /* bLooping */ )
{
	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == NULL)
		return;

	// Reset to the first sequence event...
	m_playCursor.reset(pSeq);
}


// Loop positioning.
void qtractorMidiClip::setLoop (
	unsigned long /* iLoopStart */, unsigned long /* iLoopEnd */ )
{
	// Do nothing?
}


// Clip close-commit (record specific)
void qtractorMidiClip::close (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiClip[%p]::close(%d)\n", this);
#endif

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return;

	// Take pretended clip-length...
	unsigned long iClipLength = clipLength();
	qtractorMidiSequence *pSeq = sequence();
	if (pSeq) {
		if (iClipLength > 0)
			pSeq->setTimeLength(pSession->tickFromFrame(iClipLength));
		// Final read statistics...
		m_noteMin = pSeq->noteMin();
		m_noteMax = pSeq->noteMax();
		// Actual sequence closure...
		pSeq->close();
		// Commit the final clip length...
		if (iClipLength < 1) {
			iClipLength = pSession->frameFromTick(pSeq->duration());
			setClipLength(iClipLength);
		}
	}

	// Now's time to write the whole thing...
	const bool bNewFile
		= (m_pFile && m_pFile->mode() == qtractorMidiFile::Write);
	if (bNewFile && iClipLength > 0 && pSeq) {
		// Write channel tracks...
		if (m_iFormat == 1)
			m_pFile->writeTrack(NULL);	// Setup track (SMF format 1).
		m_pFile->writeTrack(pSeq);		// Channel track.
		m_pFile->close();
	}

	// Just to be sure things get deallocated..
	closeMidiFile();

	// If proven empty, remove the file.
	if (bNewFile && iClipLength < 1)
		QFile::remove(filename());
}


// MIDI clip (re)open method.
void qtractorMidiClip::open (void)
{
	// Go open the proper file...
	openMidiFile(filename(), m_iTrackChannel);
}


// Audio clip special process cycle executive.
void qtractorMidiClip::process (
	unsigned long iFrameStart, unsigned long iFrameEnd )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiClip[%p]::process(%lu, %lu)\n",
		this, iFrameStart, iFrameEnd);
#endif

	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == NULL)
		return;

	// Track mute state...
	const bool bMute = (pTrack->isMute()
		|| (pSession->soloTracks() && !pTrack->isSolo()));

	const unsigned long t0 = pSession->tickFromFrame(clipStart());

	const unsigned long iTimeStart = pSession->tickFromFrame(iFrameStart);
	const unsigned long iTimeEnd   = pSession->tickFromFrame(iFrameEnd);

	// Enqueue the requested events...
	qtractorMidiEvent *pEvent
		= m_playCursor.seek(pSeq, iTimeStart > t0 ? iTimeStart - t0 : 0);
	while (pEvent) {
		const unsigned long t1 = t0 + pEvent->time();
		if (t1 >= iTimeEnd)
			break;
		if (t1 >= iTimeStart
			&& (!bMute || pEvent->type() != qtractorMidiEvent::NOTEON))
			pMidiEngine->enqueue(pTrack, pEvent, t1,
				gain(pSession->frameFromTick(t1) - clipStart()));
		pEvent = pEvent->next();
	}
}


// MIDI clip paint method.
void qtractorMidiClip::draw (
	QPainter *pPainter, const QRect& clipRect, unsigned long iClipOffset )
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return;

	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == NULL)
		return;

	// Check maximum note span...
	int iNoteSpan = (pSeq->noteMax() - pSeq->noteMin()) + 1;
	if (iNoteSpan < 6)
		iNoteSpan = 6;

	const unsigned long iFrameStart = clipStart() + iClipOffset;
	const int cx = pSession->pixelFromFrame(iFrameStart);

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(clipStart());
	const unsigned long t0 = pNode->tickFromFrame(clipStart());

	pNode = cursor.seekFrame(iFrameStart);	
	const unsigned long iTimeStart = pNode->tickFromFrame(iFrameStart);
	pNode = cursor.seekPixel(cx + clipRect.width());	
	const unsigned long iTimeEnd = pNode->tickFromPixel(cx + clipRect.width());

	const QColor& fg = pTrack->foreground();
	pPainter->setPen(fg);
	pPainter->setBrush(fg.lighter());

	const bool bClipRecord = (pTrack->clipRecord() == this);
	const int h1 = clipRect.height() - 2;
	const int h = (h1 / iNoteSpan) + 1;

	qtractorMidiEvent *pEvent
		= m_drawCursor.reset(pSeq, iTimeStart > t0 ? iTimeStart - t0 : 0);
	while (pEvent) {
		unsigned long t1 = t0 + pEvent->time();
		if (t1 >= iTimeEnd)
			break;
		unsigned long t2 = t1 + pEvent->duration();
		if (pEvent->type() == qtractorMidiEvent::NOTEON && t2 >= iTimeStart) {
			pNode = cursor.seekTick(t1);
			const int x = clipRect.x() + pNode->pixelFromTick(t1) - cx;
			const int y = clipRect.bottom()
				- (h1 * (pEvent->note() - pSeq->noteMin() + 1)) / iNoteSpan;
			int w = (pEvent->duration() > 0 || !bClipRecord
				? clipRect.x() + pNode->pixelFromTick(t2) - cx
				: clipRect.right()) - x; // Pending note-off? (while recording)
			if (w < 3) w = 3;
			pPainter->fillRect(x, y, w, h, fg);
			if (w > 4 && h > 3)
				pPainter->fillRect(x + 1, y + 1, w - 4, h - 3, fg.lighter());
			else
			if (w > 3 && h > 2)
				pPainter->fillRect(x + 1, y + 1, w - 3, h - 2, fg.lighter());
		}
		pEvent = pEvent->next();
	}
}


// Clip editor method.
bool qtractorMidiClip::startEditor ( QWidget *pParent )
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	if (m_pMidiEditorForm == NULL) {
		// Build up the editor form...
		// What style do we create tool childs?
		Qt::WindowFlags wflags = Qt::Window
			| Qt::CustomizeWindowHint
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowMinMaxButtonsHint
			| Qt::WindowCloseButtonHint;
	#ifdef QTRACTOR_MIDI_EDITOR_TOOL
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bKeepToolsOnTop)
			wflags |= Qt::Tool;
	#endif
		// Do it...
		m_pMidiEditorForm = new qtractorMidiEditorForm(pParent, wflags);
		// Set its most standing properties...
		m_pMidiEditorForm->show();
		m_pMidiEditorForm->setup(this);
	} else {
		// Just show up the editor form...
		m_pMidiEditorForm->setup();
		m_pMidiEditorForm->show();
	}

	// Get it up any way...
	m_pMidiEditorForm->raise();
	m_pMidiEditorForm->activateWindow();

	return true;
}


// Clip editor reset.
void qtractorMidiClip::resetEditor ( bool bSelectClear )
{
	if (m_pMidiEditorForm) {
		qtractorMidiEditor *pMidiEditor = m_pMidiEditorForm->editor();
		if (pMidiEditor)
			pMidiEditor->reset(bSelectClear);
		m_pMidiEditorForm->resetDirtyCount();
	}
}


// Clip editor update.
void qtractorMidiClip::updateEditor ( bool bSelectClear )
{
	if (m_pMidiEditorForm == NULL)
		return;

	qtractorMidiEditor *pMidiEditor = m_pMidiEditorForm->editor();
	if (pMidiEditor) {
		pMidiEditor->reset(bSelectClear);
		pMidiEditor->setOffset(clipStart());
		pMidiEditor->setLength(clipLength());
		qtractorTrack *pTrack = track();
		if (pTrack) {
			pMidiEditor->setForeground(pTrack->foreground());
			pMidiEditor->setBackground(pTrack->background());
		}
		pMidiEditor->updateContents();
	}

	m_pMidiEditorForm->resetDirtyCount();
	m_pMidiEditorForm->updateInstrumentNames();
	m_pMidiEditorForm->stabilizeForm();
}


// Clip query-close method (return true if editing is done).
bool qtractorMidiClip::queryEditor (void)
{
	if (m_pMidiEditorForm)
		return m_pMidiEditorForm->queryClose();

	// Are any dirty changes pending commit?
	bool bQueryEditor = qtractorClip::queryEditor();
	if (!bQueryEditor) {
		switch (qtractorMidiEditorForm::querySave(filename())) {
		case QMessageBox::Save:	{
			// Save/replace the clip track...
			bQueryEditor = saveCopyFile(true);
			break;
		}
		case QMessageBox::Discard:
			bQueryEditor = true;
			break;
		case QMessageBox::Cancel:
			bQueryEditor = false;
			break;
		}
	}

	return bQueryEditor;
}


// MIDI clip tool-tip.
QString qtractorMidiClip::toolTip (void) const
{
	QString sToolTip = qtractorClip::toolTip() + ' ';

	sToolTip += QObject::tr("(format %1)\nMIDI:\t").arg(m_iFormat);
	if (m_iFormat == 0)
		sToolTip += QObject::tr("Channel %1").arg(m_iTrackChannel + 1);
	else
		sToolTip += QObject::tr("Track %1").arg(m_iTrackChannel);

	if (m_pFile) {
		sToolTip += QObject::tr(", %1 tracks, %2 tpqn")
			.arg(m_pFile->tracks())
			.arg(m_pFile->ticksPerBeat());
	}

	if (clipGain() > 1.0f)
		sToolTip += QObject::tr(" (%1% vol)")
			.arg(100.0f * clipGain(), 0, 'g', 3);

	return sToolTip;
}


// Auto-save to (possible) new file revision.
bool qtractorMidiClip::saveCopyFile ( bool bUpdate )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	// Have a new filename revision...
	const QString& sFilename = createFilePathRevision();

	// Save/replace the clip track...
	if (!qtractorMidiFile::saveCopyFile(
			sFilename, filename(), trackChannel(), format(), sequence(),
			pSession->timeScale(), pSession->tickFromFrame(clipStart())))
		return false;

	// Pre-commit dirty changes...
	setFilenameEx(sFilename, bUpdate);

	// Reference for immediate file addition...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->addMidiFile(sFilename);

	return true;
}


// Virtual document element methods.
bool qtractorMidiClip::loadClipElement (
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
		// Load track state..
		if (eChild.tagName() == "filename")
			qtractorMidiClip::setFilename(eChild.text());
		else if (eChild.tagName() == "track-channel")
			qtractorMidiClip::setTrackChannel(eChild.text().toUShort());
		else if (eChild.tagName() == "revision")
			qtractorMidiClip::setRevision(eChild.text().toUShort());
	}

	return true;
}


bool qtractorMidiClip::saveClipElement (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	QDomElement eMidiClip = pDocument->document()->createElement("midi-clip");
	pDocument->saveTextElement("filename",
		qtractorMidiClip::relativeFilename(pDocument), &eMidiClip);
	pDocument->saveTextElement("track-channel",
		QString::number(qtractorMidiClip::trackChannel()), &eMidiClip);
	pDocument->saveTextElement("revision",
		QString::number(qtractorMidiClip::revision()), &eMidiClip);
	pElement->appendChild(eMidiClip);

	return true;
}


// MIDI clip export method.
bool qtractorMidiClip::clipExport (
	ClipExport pfnClipExport, void *pvArg,
	unsigned long iOffset, unsigned long iLength ) const
{
	qtractorTrack *pTrack = track();
	if (pTrack == NULL)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorMidiSequence *pSeq = sequence();
	if (pSeq == NULL)
		return false;

	if (iLength < 1)
		iLength = clipLength();

	const unsigned short iTicksPerBeat = pSession->ticksPerBeat();
	const unsigned long iTimeOffset = pSeq->timeOffset();

	qtractorTimeScale::Cursor cursor(pSession->timeScale());
	qtractorTimeScale::Node *pNode = cursor.seekFrame(clipStart());
	const unsigned long t0 = pNode->tickFromFrame(clipStart());

	unsigned long f1 = clipStart() + clipOffset() + iOffset;
	pNode = cursor.seekFrame(f1);
	const unsigned long t1 = pNode->tickFromFrame(f1);
	unsigned long iTimeStart = t1 - t0;
	iTimeStart = (iTimeStart > iTimeOffset ? iTimeStart - iTimeOffset : 0);
	pNode = cursor.seekFrame(f1 += iLength);
	const unsigned long iTimeEnd = iTimeStart + pNode->tickFromFrame(f1) - t1;

	qtractorMidiSequence seq(pSeq->name(), pSeq->channel(), iTicksPerBeat);

	seq.setBank(pTrack->midiBank());
	seq.setProg(pTrack->midiProg());

	for (qtractorMidiEvent *pEvent = pSeq->events().first();
			pEvent; pEvent = pEvent->next()) {
		const unsigned long iTime = pEvent->time();
		if (iTime >= iTimeStart && iTime < iTimeEnd) {
			qtractorMidiEvent *pNewEvent = new qtractorMidiEvent(*pEvent);
			pNewEvent->setTime(iTime - iTimeStart);
			if (pNewEvent->type() == qtractorMidiEvent::NOTEON) {
				pNewEvent->setVelocity((unsigned char)
					(clipGain() * float(pNewEvent->velocity())) & 0x7f);
				if (iTime + pEvent->duration() > iTimeEnd)
					pNewEvent->setDuration(iTimeEnd - iTime);
			}
			seq.insertEvent(pNewEvent);
		}
	}

	(*pfnClipExport)(&seq, pvArg);

	return true;
}


// Default MIDI file format (for capture/record) accessors.
unsigned short qtractorMidiClip::g_iDefaultFormat = 0;

void qtractorMidiClip::setDefaultFormat ( unsigned short iFormat )
{
	g_iDefaultFormat = iFormat;
}

unsigned short qtractorMidiClip::defaultFormat (void)
{
	return g_iDefaultFormat;
}


// end of qtractorMidiClip.cpp
