// qtractorMidiClip.h
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QPoint>
#include <QSize>


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

	unsigned short format() const
		{ return (m_pData ? m_pData->format() : 0); }

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
	int bankSelMethod() const
		{ return (m_pData ? m_pData->bankSelMethod() : -1); }
	int bank() const
		{ return (m_pData ? m_pData->bank() : -1); }
	int prog() const
		{ return (m_pData ? m_pData->prog() : -1); }

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

	// MIDI clip freewheeling process cycle executive (needed for export).
	void process_export(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void draw(QPainter *pPainter,
		const QRect& clipRect, unsigned long iClipOffset);

	// Clip update method (rolling stats).
	void update();

	// Clip editor methods.
	bool startEditor(QWidget *pParent = NULL);
	void updateEditor(bool bSelectClear);
	void updateEditorContents();
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
		Data(unsigned short iFormat)
			: m_iFormat(iFormat), m_pSeq(new qtractorMidiSequence()) {}

		// Destructor.
		~Data() { clear(); delete m_pSeq; }

		// Originial format accessor.
		unsigned short format() const
			{ return m_iFormat; }

		// Sequence accessor.
		qtractorMidiSequence *sequence() const
			{ return m_pSeq; }

		// Sequence properties accessors.
		unsigned short channel() const
			{ return m_pSeq->channel(); }

		int bankSelMethod() const
			{ return m_pSeq->bankSelMethod(); }
		int bank() const
			{ return m_pSeq->bank(); }
		int prog() const
			{ return m_pSeq->prog(); }

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
		unsigned short m_iFormat;

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

	// Check whether a MIDI clip is hash-linked to another.
	bool isLinkedClip (qtractorMidiClip *pMidiClip) const;

	// Get all hash-linked clips (including self).
	QList<qtractorMidiClip *> linkedClips() const;

	// Make sure the clip hash-table gets reset.
	static void clearHashTable();

	// MIDI clip editor position/size accessors.
	void setEditorPos(const QPoint& pos)
		{ m_posEditor = pos; }
	const QPoint& editorPos() const
		{ return m_posEditor; }

	void setEditorSize(const QSize& size)
		{ m_sizeEditor = size; }
	const QSize& editorSize() const
		{ return m_sizeEditor; }

	// Ghost track setting.
	void setGhostTrackName(const QString& sGhostTrackName)
		{ m_sGhostTrackName = sGhostTrackName; }
	const QString& ghostTrackName() const
		{ return m_sGhostTrackName; }

protected:

	// Virtual document element methods.
	bool loadClipElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveClipElement(qtractorDocument *pDocument, QDomElement *pElement);

	// Private cleanup.
	void closeMidiFile();

	// MIDI clip freewheeling event enqueue method (needed for export).
	void enqueue_export(qtractorTrack *pTrack,
		qtractorMidiEvent *pEvent, unsigned long iTime, float fGain) const;

private:

	// Instance variables.
	qtractorMidiFile *m_pFile;

	unsigned short m_iTrackChannel;

	bool m_bSessionFlag;

	// Revisionist count.
	unsigned short m_iRevision;

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

	// And for geometry it was last seen...
	QPoint m_posEditor;
	QSize m_sizeEditor;

	// Ghost track setting.
	QString m_sGhostTrackName;

	// Default MIDI file format (for capture/record)
	static unsigned short g_iDefaultFormat;
};


#endif  // __qtractorMidiClip_h


// end of qtractorMidiClip.h
