// qtractorMidiClip.h
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

#ifndef __qtractorMidiClip_h
#define __qtractorMidiClip_h

#include "qtractorClip.h"
#include "qtractorMidiCursor.h"
#include "qtractorMidiFile.h"

// Forward declartiuons.
class qtractorMidiEditorForm;


//----------------------------------------------------------------------
// class qtractorMidiClip -- MIDI file/sequence clip.
//

class qtractorMidiClip : public qtractorClip
{
public:

	// Constructor.
	qtractorMidiClip(qtractorTrack *pTrack);
	// Copy constructor.
	qtractorMidiClip(const qtractorMidiClip& clip);

	// Destructor.
	~qtractorMidiClip();

	// Clip (re)open method.
	void open();

	// Brand new clip contents new method.
	bool createMidiFile(const QString& sFilename, int iTrackChannel = 0);

	// The main use method.
	bool openMidiFile(const QString& sFilename, int iTrackChannel = 0,
		int iMode = qtractorMidiFile::Read);

	// MIDI file properties accessors.
	void setTrackChannel(unsigned short iTrackChannel)
		{ m_iTrackChannel = iTrackChannel; }
	unsigned short trackChannel() const
		{ return m_iTrackChannel; }

	void setFormat(unsigned short iFormat)
		{ m_iFormat = iFormat; }
	unsigned short format() const
		{ return m_iFormat; }

	// (Meta)Session flag accessors.
	void setSessionFlag(bool bSessionFlag)
		{ m_bSessionFlag = bSessionFlag; }
	bool isSessionFlag() const
		{ return m_bSessionFlag; }

	// Revisionist accessors.
	void setRevision(unsigned short iRevision)
		{ m_iRevision = iRevision; }
	unsigned short revision() const
		{ return m_iRevision; }

	// Revisionist method.
	QString createFilePathRevision(bool bForce = false);

	// Sequence properties accessors.
	qtractorMidiSequence *sequence() const
		{ return (m_pData ? m_pData->sequence() : NULL); }

	unsigned short channel() const
		{ return (m_pData ? m_pData->channel() : 0); }
	int bank() const
		{ return (m_pData ? m_pData->bank() : -1); }
	int prog() const
		{ return (m_pData ? m_pData->prog() : -1); }

	// Statistical cached accessors.
	unsigned char noteMin() const
		{ return m_pData ? m_pData->noteMin() : m_noteMin; }
	unsigned char noteMax() const
		{ return m_pData ? m_pData->noteMax() : m_noteMax; }

	// Intra-clip frame positioning.
	void seek(unsigned long iFrame);

	// Reset clip state.
	void reset(bool bLooping);

	// Loop positioning.
	void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Clip close-commit (record specific)
	void close();

	// MIDI clip special process cycle executive.
	void process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void draw(QPainter *pPainter, const QRect& clipRect,
		unsigned long iClipOffset);

	// Clip editor methods.
	bool startEditor(QWidget *pParent);
	void resetEditor(bool bSelectClear);
	void updateEditor(bool bSelectClear);
	bool queryEditor();

	// MIDI clip tool-tip.
	QString toolTip() const;

	// Auto-save to (possible) new file revision.
	bool saveCopyFile(bool bUpdate);

	// MIDI clip export method.
	typedef void (*ClipExport)(qtractorMidiSequence *, void *);

	bool clipExport(ClipExport pfnClipExport, void *pvArg,
		unsigned long iOffset = 0, unsigned long iLength = 0) const;

	// Default MIDI file format accessors
	// (specific to capture/recording)
	static void setDefaultFormat(unsigned short iFormat);
	static unsigned short defaultFormat();

	// MIDI file hash key.
	class FileKey;

	typedef QHash<FileKey, int> FileHash;

	// Most interesting key/data (ref-counted?)...
	class Key;
	class Data
	{
	public:

		// Constructor.
		Data() : m_pSeq(new qtractorMidiSequence()) {}

		// Destructor.
		~Data() { clear(); delete m_pSeq; }

		// Sequence accessor.
		qtractorMidiSequence *sequence() const
			{ return m_pSeq; }

		// Sequence properties accessors.
		unsigned short channel() const
			{ return m_pSeq->channel(); }

		int bank() const
			{ return m_pSeq->bank(); }
		int prog() const
			{ return m_pSeq->prog(); }

		unsigned char noteMin() const
		   { return m_pSeq->noteMin(); }
		unsigned char noteMax() const
		   { return m_pSeq->noteMax(); }

		// Ref-counting related methods.
		void attach(qtractorMidiClip *pMidiClip)
			{ m_clips.append(pMidiClip); }

		void detach(qtractorMidiClip *pMidiClip)
			{ m_clips.removeAll(pMidiClip); }

		unsigned short count() const
			{ return m_clips.count(); }

		const QList<qtractorMidiClip *>& clips() const
			{ return m_clips; }

		void clear()
			{ m_clips.clear(); }

	private:

		// Interesting variables.
		qtractorMidiSequence *m_pSeq;

		// Ref-counting related stuff.
		QList<qtractorMidiClip *> m_clips;
	};

	typedef QHash<Key, Data *> Hash;

	// Sync all ref-counted filenames.
	void setFilenameEx(const QString& sFilename, bool bUpdate);

	// Sync all ref-counted clip-lengths.
	void setClipLengthEx(unsigned long iClipLength);

	// Sync all ref-counted clip editors.
	void updateEditorEx(bool bSelectClear);
	void resetEditorEx(bool bSelectClear);

	// Sync all ref-counted clip-dirtyness.
	void setDirtyEx(bool bDirty);

	// Manage local hash key.
	void insertHashKey();
	void updateHashKey();
	void removeHashKey();

	// Un/relink (de/clone) local hash data.
	void unlinkHashData();
	void relinkHashData();

	// Whether local hash is being shared.
	bool isHashLinked() const;

	// Make sure the clip hash-table gets reset.
	static void clearHashTable();

protected:

	// Virtual document element methods.
	bool loadClipElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveClipElement(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Private cleanup.
	void closeMidiFile();

private:

	// Instance variables.
	qtractorMidiFile *m_pFile;

	unsigned short m_iTrackChannel;
	unsigned short m_iFormat;
	bool           m_bSessionFlag;

	// Revisionist count.
	unsigned short m_iRevision;

	// Statistical cached variables.
	unsigned char  m_noteMin;
	unsigned char  m_noteMax;

	// Most interesting key/data (ref-counted?)...
	Key  *m_pKey;
	Data *m_pData;

	static Hash g_hashTable;

	// MIDI file hash key.
	static FileHash g_hashFiles;

	// To optimize and keep track of current playback
	// position, mostly like an sequence cursor/iterator.
	qtractorMidiCursor m_playCursor;
	qtractorMidiCursor m_drawCursor;

	// This clip editor form widget.
	qtractorMidiEditorForm *m_pMidiEditorForm;

	// Default MIDI file format (for capture/record)
	static unsigned short g_iDefaultFormat;
};


#endif  // __qtractorMidiClip_h


// end of qtractorMidiClip.h
