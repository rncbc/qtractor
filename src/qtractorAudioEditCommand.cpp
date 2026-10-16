// qtractorAudioEditCommand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEditCommand.h"

#include "qtractorAudioClip.h"
#include "qtractorAudioFile.h"

#include "qtractorSession.h"
#include "qtractorFileList.h"

#include "qtractorMainForm.h"

#include <QApplication>


//----------------------------------------------------------------------
// class qtractorAudioEditCommand - implementation.
//

// Constructor.
qtractorAudioEditCommand::qtractorAudioEditCommand (
	qtractorAudioClip *pAudioClip, const QString& sName,
	CommandType ctype, unsigned long iStart, unsigned long iLength,
	unsigned long iAfter, const QString& sFilename )
	: qtractorCommand(sName), m_pAudioClip(pAudioClip), m_ctype(ctype),
	  m_iStart(iStart), m_iLength(iLength), m_iAfter(iAfter),
	  m_sFilename(sFilename), m_iPrevLength(0), m_iNextLength(0),
	  m_iRedoCount(0)
{
	// Capture the current file path as the "before" state.
	m_sPrevFilename = m_pAudioClip->filename();
	m_iPrevLength   = m_pAudioClip->clipLength();

	// Generate the new revision path that redo() will write to.
	m_sNextFilename = m_pAudioClip->createFilePathRevision(true);

	// Derive the expected clip length after the edit.
	if (m_ctype == DeleteRange && m_iPrevLength > m_iLength)
		m_iNextLength = m_iPrevLength - m_iLength;
	else
	if (m_ctype == InsertRange)
		m_iNextLength = m_iPrevLength + m_iLength;
	else
		m_iNextLength = m_iPrevLength;

	// Commands in this editor always refresh the views.
	setRefresh(true);
}


// Destructor.
qtractorAudioEditCommand::~qtractorAudioEditCommand (void)
{
}


// Redo: apply the edit (write prevFile → nextFile).
bool qtractorAudioEditCommand::redo (void)
{
	return execute(true);
}


// Undo: revert the edit (restore prevFile from nextFile perspective).
bool qtractorAudioEditCommand::undo (void)
{
	return execute(false);
}


//----------------------------------------------------------------------
// qtractorAudioEditCommand::execute -- shared stream-copy engine.
//
// bRedo == true  : read m_prevFile, write m_nextFile, apply the edit
// bRedo == false : read m_nextFile, write m_prevFile, reverse the edit
//
// For DeleteRange the reverse is an InsertRange using the deleted frames
// that were previously written into the "next" file gap position – but
// since we store both full revision files we simply re-stream the
// appropriate full source verbatim (the old file is the correct undo target).
//
bool qtractorAudioEditCommand::execute ( bool bRedo )
{
	if (m_pAudioClip == nullptr)
		return false;

	qtractorTrack *pTrack = m_pAudioClip->track();
	if (pTrack == nullptr)
		return false;

	qtractorSession *pSession = pTrack->session();
	if (pSession == nullptr)
		return false;

	// When undoing, we just hot-swap back to the previous revision.
	// The "previous" file always exists on disk – no rewrite needed.
	if (!bRedo) {
		m_pAudioClip->setFilenameEx(m_sPrevFilename, true);
		m_pAudioClip->setClipLengthEx(m_iPrevLength);
		m_pAudioClip->resetAudioPeakEx();
		return true;
	}

	// ── REDO path: rewrite prevFile → nextFile with the edit applied ──

	// Open source (current clip file).
	qtractorAudioFile *pSrc
		= qtractorAudioFileFactory::createAudioFile(m_sPrevFilename);
	if (pSrc == nullptr)
		return false;
	if (!pSrc->open(m_sPrevFilename, qtractorAudioFile::Read)) {
		delete pSrc;
		return false;
	}

	const unsigned short iChannels   = pSrc->channels();
	const unsigned int   iSampleRate = pSrc->sampleRate();

	if (iChannels < 1) {
		pSrc->close();
		delete pSrc;
		return false;
	}

	// Open destination (new revision file, same format as source).
	qtractorAudioFile *pDst
		= qtractorAudioFileFactory::createAudioFile(
			m_sNextFilename, iChannels, iSampleRate, 4096);
	if (pDst == nullptr) {
		pSrc->close();
		delete pSrc;
		return false;
	}

	if (!pDst->open(m_sNextFilename, qtractorAudioFile::Write)) {
		delete pDst;
		pSrc->close();
		delete pSrc;
		return false;
	}

	// Might take sometime now...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Allocate a per-channel I/O buffer (stack of pointers, heap data).
	const unsigned int iBufSize = 4096;
	float **ppBuf = new float * [iChannels];
	for (unsigned short i = 0; i < iChannels; ++i)
		ppBuf[i] = new float [iBufSize];

	bool bOk = false;

	// Intra-clip start is relative to file beginning (clipOffset already baked in).
	const unsigned long iStart  = m_iStart;
	const unsigned long iLength = m_iLength;

	if (m_ctype == DeleteRange) {
		bOk = deleteFrames(ppBuf, iBufSize, pDst, pSrc, iStart, iLength);
	}
	else
	if (m_ctype == InsertRange) {
		bOk = insertFrames(ppBuf, iBufSize, pDst, pSrc, iStart, iLength);
	}
	else
	if (m_ctype == MoveRange) {
		const unsigned long iAfter = m_iAfter;
		bOk = moveFrames(ppBuf, iBufSize, pDst, pSrc, iStart, iLength, iAfter);
	}

	// Release buffers.
	for (unsigned short i = 0; i < iChannels; ++i)
		delete[] ppBuf[i];
	delete [] ppBuf;

	pSrc->close();
	delete pSrc;

	pDst->close();
	delete pDst;

	// Done with it...
	QApplication::restoreOverrideCursor();

	if (!bOk)
		return false;

	// Hot-swap the clip to the new revision file.
	pSession->files()->addClipItem(qtractorFileList::Audio, m_pAudioClip, true);

	m_pAudioClip->setFilenameEx(m_sNextFilename, true);
	m_pAudioClip->setClipLengthEx(m_iNextLength);
	m_pAudioClip->resetAudioPeakEx();

	pSession->files()->removeClipItem(qtractorFileList::Audio, m_pAudioClip);

	// New file is supposed to get into the bill of materials...
	if (++m_iRedoCount == 1) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->addAudioFile(m_pAudioClip->filename());
	}

	return true;
}


// Operational helpers.
//
bool qtractorAudioEditCommand::deleteFrames (
	float **ppFrames, unsigned int nframes,
	qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
	unsigned long iStart, unsigned long iLength ) const
{
	// Phase 1: frames before the selection (verbatim).
	bool bOk = streamFrames(ppFrames, nframes, pDst, pSrc, 0, iStart);

	// Phase 2: skip selection – resume after iStart + iLength.
	const unsigned long iAfter = iStart + iLength;
	const unsigned long iFrames = pSrc->frames();
	if (bOk && iFrames > iAfter) {
		bOk = streamFrames(ppFrames, nframes,
			pDst, pSrc, iAfter, iFrames - iAfter);
	}

	return bOk;
}


bool qtractorAudioEditCommand::insertFrames (
	float **ppFrames, unsigned int nframes,
	qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
	unsigned long iStart, unsigned long iLength ) const
{
	// Open the clipboard/temp. source file.
	if (m_sFilename.isEmpty())
		return false;

	qtractorAudioFile *pTmp
		= qtractorAudioFileFactory::createAudioFile(m_sFilename);
	if (pTmp == nullptr)
		return false;

	if (!pTmp->open(m_sFilename, qtractorAudioFile::Read)) {
		delete pTmp;
		return false;
	}

	// Phase 1: frames before insert point.
	bool bOk = streamFrames(ppFrames, nframes, pDst, pSrc, 0, iStart);

	// Phase 2: insert the clipboard/temp. frames.
	if (bOk)
		bOk = streamFrames(ppFrames, nframes, pDst, pTmp, 0, iLength);

	// Phase 3: frames after insert point (tail of source).
	const unsigned long iFrames = pSrc->frames();
	if (bOk && iFrames > iStart) {
		bOk = streamFrames(ppFrames, nframes,
			pDst, pSrc, iStart, iFrames - iStart);
	}

	pTmp->close();
	delete pTmp;

	return bOk;
}


bool qtractorAudioEditCommand::moveFrames (
	float **ppFrames, unsigned int nframes,
	qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
	unsigned long iStart, unsigned long iLength,
	unsigned long iAfter ) const
{
	// Open the clipboard/temp. source file.
	if (m_sFilename.isEmpty())
		return false;

	qtractorAudioFile *pTmp
		= qtractorAudioFileFactory::createAudioFile(m_sFilename);
	if (pTmp == nullptr)
		return false;

	if (!pTmp->open(m_sFilename, qtractorAudioFile::Read)) {
		delete pTmp;
		return false;
	}

	bool bOk = false;

	// Phase 1: frames before insert/delete point.
	if (iAfter < iStart)
		bOk = streamFrames(ppFrames, nframes, pDst, pSrc, 0, iAfter);
	else
		bOk = streamFrames(ppFrames, nframes, pDst, pSrc, 0, iStart);

	const unsigned long iAfter2	= iStart + iLength;
	if (bOk && iAfter > iAfter2) {
		bOk = streamFrames(ppFrames, nframes,
			pDst, pSrc, iAfter2, iAfter - iAfter2);
	}

	// Phase 2: insert the clipboard/temp. frames.
	if (bOk)
		bOk = streamFrames(ppFrames, nframes, pDst, pTmp, 0, iLength);

	if (bOk && iAfter < iStart) {
		bOk = streamFrames(ppFrames, nframes,
			pDst, pSrc, iAfter, iStart - iAfter);
	}

	// Phase 3: frames after insert point (tail of source).
	const unsigned long iFrames = pSrc->frames();
	if (iAfter > iAfter2) {
		if (bOk && iFrames > iAfter) {
			bOk = streamFrames(ppFrames, nframes,
				pDst, pSrc, iAfter, iFrames - iAfter);
		}
	} else {
		if (bOk && iFrames > iAfter2) {
			bOk = streamFrames(ppFrames, nframes,
				pDst, pSrc, iAfter2, iFrames - iAfter2);
		}
	}

	pTmp->close();
	delete pTmp;

	return bOk;
}


// Stream frames helper.
bool qtractorAudioEditCommand::streamFrames (
	float **ppFrames, unsigned int nframes,
	qtractorAudioFile *pDst, qtractorAudioFile *pSrc,
	unsigned long iOffset, unsigned long iLength ) const
{
	if (iLength < 1)
		return true;

	if (!pSrc->seek(iOffset))
		return false;

	unsigned int iRemain = iLength;
	while (iRemain > 0) {
		const int nread = pSrc->read(ppFrames, qMin(nframes, iRemain));
		if (nread < 1)
			break;
		pDst->write(ppFrames, nread);
		iRemain -= nread;
	}

	return true;
};


// end of qtractorAudioEditCommand.cpp
