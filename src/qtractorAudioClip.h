// qtractorAudioClip.h
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

#ifndef __qtractorAudioClip_h
#define __qtractorAudioClip_h

#include "qtractorClip.h"
#include "qtractorAudioBuffer.h"

#include "qtractorCommand.h"


// Forward declarations.
class qtractorAudioPeak;
class qtractorAudioEditorForm;


//----------------------------------------------------------------------
// class qtractorAudioClip -- Audio file/buffer clip.
//

class qtractorAudioClip : public qtractorClip
{
public:

	// Constructor.
	qtractorAudioClip(qtractorTrack *pTrack);
	// Copy constructor.
	qtractorAudioClip(const qtractorAudioClip& clip);

	// Destructor.
	~qtractorAudioClip();

	// Time-stretching.
	void setTimeStretch(float fTimeStretch)
		 { m_fTimeStretch = fTimeStretch; }
	float timeStretch() const
		 { return m_fTimeStretch; }

	// Pitch-shifting.
	void setPitchShift(float fPitchShift)
		{ m_fPitchShift = fPitchShift; }
	float pitchShift() const
		{ return m_fPitchShift; }

	// Time-stretch/pitch-shifting engine mode flags.
	void setStretcherFlags(unsigned int iStretcherFlags)
		 { m_iStretcherFlags = iStretcherFlags; }
	unsigned int stretcherFlags() const
		 { return m_iStretcherFlags; }

	void setStretcherFlag(unsigned int iStretcherFlag, bool bOn)
	{
		if (bOn)
			m_iStretcherFlags |=  iStretcherFlag;
		else
			m_iStretcherFlags &= ~iStretcherFlag;
	}

	bool isStretcherFlag (unsigned int iStretcherFlag) const
		{ return (m_iStretcherFlags & iStretcherFlag); }

	// Alternating overlap tag.
	unsigned int overlap() const
		 { return m_iOverlap; }

	// Clip (re)open method.
	void open();

	// The main use method.
	bool openAudioFile(const QString& sFilename,
		int iMode = qtractorAudioFile::Read);

	// Audio properties accessors.
	qtractorAudioBuffer *buffer() const
		{ return (m_pData ? m_pData->buffer() : nullptr); }

	// Direct write method.
	void write(float **ppBuffer, unsigned int iFrames,
		unsigned short iChannels = 0, unsigned int iOffset = 0);

	// Intra-clip frame positioning.
	void seek(unsigned long iFrame);

	// Reset clip state.
	void reset(bool bLooping);

	// Loop positioning.
	void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Clip close-commit (record specific)
	void close();

	// Audio clip special process cycle executive.
	void process(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Audio clip freewheeling process cycle executive (needed for export).
	void process_export(unsigned long iFrameStart, unsigned long iFrameEnd);

	// Clip paint method.
	void draw(QPainter *pPainter,
		const QRect& clipRect, unsigned long iClipOffset);

	// Clip update method (no-op).
	void update() {}

	// Clip editor methods.
	bool startEditor(QWidget *pParent = nullptr);
	void updateEditor(bool bSelectClear);
	void updateEditorContents();
	void updateEditorTimeScale();
	bool queryEditor();

	// Audio clip tool-tip.
	QString toolTip() const;

	// Auto-save to (possible) new file revision.
	bool saveCopyFile(const QString& sFilename, bool bUpdate);

	// Audio clip export method.
	typedef void (*ClipExport)(float **, unsigned int, void *);

	bool clipExport(ClipExport pfnClipExport, void *pvArg,
		unsigned long iOffset = 0, unsigned long iLength = 0) const;

	// Gain fractionalizers(tm)...
	typedef struct { int num, den; } FractGain;

	FractGain *fractGains() const
		{ return m_pFractGains; }

	// Audio file hash key.
	class FileKey;

	typedef QHash<FileKey, int> FileHash;

	// Most interesting key/data (ref-counted?)...
	class Key;
	class Data
	{
	public:

		// Constructor.
		Data(qtractorTrack *pTrack, unsigned short iChannels)
			: m_pBuff(new qtractorAudioBuffer(
				pTrack->syncThread(), iChannels)),
			  m_pCommands(new qtractorCommandList()) {}
		// Destructor.
		~Data() { clear(); delete m_pCommands; delete m_pBuff; }

		// Buffer accessor.
		qtractorAudioBuffer *buffer() const
			{ return m_pBuff; }

		// Direct write method.
		void write (float **ppBuffer, unsigned int iFrames,
			unsigned short iChannels, unsigned int iOffset)
			{ m_pBuff->write(ppBuffer, iFrames, iChannels, iOffset); }

		// Direct sync method.
		void syncExport()
			{ m_pBuff->syncExport(); }

		// Intra-clip frame positioning.
		void seek(unsigned long iFrame)
			{ m_pBuff->seek(iFrame); }

		// Reset buffer state.
		void reset(bool bLooping)
			{ m_pBuff->reset(bLooping); }

		// Loop positioning.
		void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd)
			{ m_pBuff->setLoop(iLoopStart, iLoopEnd); }

		// Ref-counting related methods.
		void attach(qtractorAudioClip *pAudioClip)
			{ m_clips.append(pAudioClip); }

		void detach(qtractorAudioClip *pAudioClip)
			{ m_clips.removeAll(pAudioClip); }

		unsigned short count() const
			{ return m_clips.count(); }

		const QList<qtractorAudioClip *>& clips() const
			{ return m_clips; }

		void clear()
			{ m_clips.clear(); }

		// Commands (undo/redo) accessor.
		qtractorCommandList *commands() const
			{ return m_pCommands; }

	private:

		// Interesting variables.
		qtractorAudioBuffer *m_pBuff;

		qtractorCommandList *m_pCommands;

		// Ref-counting related stuff.
		QList<qtractorAudioClip *> m_clips;
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

	// Make sure the clip hash-table gets reset.
	static void clearHashTable();

	// Revisionist accessors.
	void setRevision(unsigned short iRevision)
		{ m_iRevision = iRevision; }
	unsigned short revision() const
		{ return m_iRevision; }

	// Revisionist method.
	QString createFilePathRevision(bool bForce = false);

	// Local command-list (undo/redo) accessor.
	qtractorCommandList *commands() const
		{ return (m_pData ? m_pData->commands() : nullptr); }

protected:

	// Virtual document element methods.
	bool loadClipElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveClipElement(qtractorDocument *pDocument, QDomElement *pElement);

	// Private cleanup.
	void closeAudioFile();

	// Alternating overlap test.
	bool isOverlap(unsigned int iOverlapSize) const;

	// Gain/panning fractionalizer(tm)...
	void updateFractGains(qtractorAudioBuffer *pBuff);

private:

	// Instance variables.
	qtractorAudioPeak *m_pPeak;

	float m_fTimeStretch;
	float m_fPitchShift;

	unsigned int m_iStretcherFlags;

	// Alternate overlap tag.
	unsigned int m_iOverlap;

	FractGain *m_pFractGains;

	// Most interesting key/data (ref-counted?)...
	Key  *m_pKey;
	Data *m_pData;

	static Hash g_hashTable;

	// Audio file hash key.
	static FileHash g_hashFiles;

	// Revisionist count.
	unsigned short m_iRevision;

	// This clip editor form widget.
	qtractorAudioEditorForm *m_pAudioEditorForm;
};


#endif  // __qtractorAudioClip_h


// end of qtractorAudioClip.h
