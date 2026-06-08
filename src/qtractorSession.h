// qtractorSession.h
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

#ifndef __qtractorSession_h
#define __qtractorSession_h

#include "qtractorAtomic.h"
#include "qtractorTrack.h"
#include "qtractorTimeScale.h"

#include "qtractorDocument.h"

#include <QHash>


// Forward declarations.
class qtractorClip;

// Special forward declarations.
class qtractorMidiEngine;
class qtractorAudioEngine;
class qtractorAudioPeakFactory;
class qtractorSessionCursor;
class qtractorMidiManager;
class qtractorInstrumentList;
class qtractorCommandList;
class qtractorCommand;
class qtractorFileList;
class qtractorFiles;


//-------------------------------------------------------------------------
// qtractorSession -- Session container (singleton).

class qtractorSession
{
public:

	// Constructor.
	qtractorSession();

	// Default destructor.
	~qtractorSession();

	// Open/close session engine(s).
	bool init();
	bool open();
	void close();

	// Reset session.
	void clear();

	// Session directory path accessors.
	void setSessionDir(const QString& sSessionDir);
	const QString& sessionDir() const
		{ return m_props.sessionDir; }

	// Session filename accessors.
	void setSessionName(const QString& sSessionName)
		{ m_props.sessionName = sSessionName; }
	const QString& sessionName() const
		{ return m_props.sessionName; }

	// Session description accessors.
	void setDescription(const QString& sDescription)
		{ m_props.description = sDescription; }
	const QString& description() const
		{ return m_props.description; }

	// Session startlength mod-accessors.
	void updateSession(
		unsigned long iSessionStart = 0, unsigned long iSessionEnd = 0);
	unsigned long sessionStart() const
		{ return m_iSessionStart; }
	unsigned long sessionEnd() const
		{ return m_iSessionEnd; }

	// Time-scale helper accessors.
	qtractorTimeScale *timeScale()
		{ return &(m_props.timeScale); }

	// Device engine common client name accessors.
	void setClientName(const QString& sClientName)
		{ m_sClientName = sClientName; }
	const QString& clientName() const
		{ return m_sClientName; }

	// Sample rate accessors.
	void setSampleRate(unsigned int iSampleRate)
		{ m_props.timeScale.setSampleRate(iSampleRate); }
	unsigned int sampleRate() const
		{ return m_props.timeScale.sampleRate(); }

	// Session tempo accessors.
	void setTempo(float fTempo);
	float tempo() const { return m_props.timeScale.tempo(); }

	// Tempo beat type accessors.
	void setBeatType(unsigned short iBeatType)
		{ m_props.timeScale.setBeatType(iBeatType); }
	unsigned short beatType() const
		{ return m_props.timeScale.beatType(); }

	// Resolution accessors.
	void setTicksPerBeat(unsigned short iTicksPerBeat);
	unsigned short ticksPerBeat() const
		{ return m_props.timeScale.ticksPerBeat(); }

	// Beats/Bar(measure) accessors.
	void setBeatsPerBar(unsigned short iBeatsPerBar)
		{ m_props.timeScale.setBeatsPerBar(iBeatsPerBar); }
	unsigned short beatsPerBar() const
		{ return m_props.timeScale.beatsPerBar(); }

	// Time signature (denominator) accessors.
	void setBeatDivisor(unsigned short iBeatDivisor)
		{ m_props.timeScale.setBeatDivisor(iBeatDivisor); }
	unsigned short beatDivisor() const
		{ return m_props.timeScale.beatDivisor(); }

	// Horizontal zoom factor.
	void setHorizontalZoom(unsigned short iHorizontalZoom)
		{ m_props.timeScale.setHorizontalZoom(iHorizontalZoom); }
	unsigned short horizontalZoom() const
		{ return m_props.timeScale.horizontalZoom(); }

	// Vertical zoom factor.
	void setVerticalZoom(unsigned short iVerticalZoom)
		{ m_props.timeScale.setVerticalZoom(iVerticalZoom); }
	unsigned short verticalZoom() const
		{ return m_props.timeScale.verticalZoom(); }

	// Pixels per beat (width).	
	void setPixelsPerBeat(unsigned short iPixelsPerBeat)
		{ m_props.timeScale.setPixelsPerBeat(iPixelsPerBeat); }
	unsigned short pixelsPerBeat() const
		{ return m_props.timeScale.pixelsPerBeat(); }

	// Beat divisor (snap) accessors.
	void setSnapPerBeat(unsigned short iSnapPerBeat)
		{ m_props.timeScale.setSnapPerBeat(iSnapPerBeat); }
	unsigned short snapPerBeat(void) const
		{ return m_props.timeScale.snapPerBeat(); }

	// Pixel/Tick number conversion.
	unsigned long tickFromPixel(unsigned int x)
		{ return m_props.timeScale.tickFromPixel(x); }
	unsigned int pixelFromTick(unsigned long iTick)
		{ return m_props.timeScale.pixelFromTick(iTick); }

	// Pixel/Frame number conversion.
	unsigned long frameFromPixel(unsigned int x) const
		{ return m_props.timeScale.frameFromPixel(x); }
	unsigned int pixelFromFrame(unsigned long iFrame) const
		{ return m_props.timeScale.pixelFromFrame(iFrame); }

	// Beat/frame conversion.
	unsigned long frameFromBeat(unsigned int iBeat)
		{ return m_props.timeScale.frameFromBeat(iBeat); }
	unsigned int beatFromFrame(unsigned long iFrame)
		{ return m_props.timeScale.beatFromFrame(iFrame); }

	// Tick/Frame number conversion.
	unsigned long frameFromTick(unsigned long iTick)
		{ return m_props.timeScale.frameFromTick(iTick); }
	unsigned long tickFromFrame(unsigned long iFrame)
		{ return m_props.timeScale.tickFromFrame(iFrame); }

	// Tick/Frame range conversion (delta conversion).
	unsigned long frameFromTickRange(
		unsigned long iTickStart, unsigned long iTickEnd, bool bOffset = false)
		{ return m_props.timeScale.frameFromTickRange(iTickStart, iTickEnd, bOffset); }
	unsigned long tickFromFrameRange(
		unsigned long iFrameStart, unsigned long iFrameEnd, bool bOffset = false)
		{ return m_props.timeScale.tickFromFrameRange(iFrameStart, iFrameEnd, bOffset); }

	// Beat/frame snap filters.
	unsigned long tickSnap(unsigned long iTick)
		{ return m_props.timeScale.tickSnap(iTick); }
	unsigned long frameSnap(unsigned long iFrame)
		{ return m_props.timeScale.frameSnap(iFrame); }
	unsigned int pixelSnap(unsigned int x)
		{ return m_props.timeScale.pixelSnap(x); }

	// Frame/locate (SMPTE) conversion.
	unsigned long frameFromLocate(unsigned int iLocate) const
		{ return (iLocate * m_props.timeScale.sampleRate()) / 30; }
	unsigned int locateFromFrame(unsigned long iFrame) const
		{ return (30 * iFrame) / m_props.timeScale.sampleRate(); }

	// Song position pointer (SPP=MIDI beats) to frame converters.
	unsigned long frameFromSongPos(unsigned int iSongPos)
		{ return frameFromTick((iSongPos * ticksPerBeat()) >> 2); }
	unsigned int  songPosFromFrame(unsigned long iFrame)
		{ return ((tickFromFrame(iFrame) << 2) / ticksPerBeat()); }

	// Update time scale divisor factors.
	void updateTimeScale();
	void updateTimeScaleEx();

	// Update time resolution divisor factors.
	void updateTimeResolution(); 

	// Update from disparate sample-rate.
	void updateSampleRate(unsigned int iSampleRate);

	// Track list management methods.
	const qtractorList<qtractorTrack>& tracks() const { return m_tracks; }

	void addTrack(qtractorTrack *pTrack);
	void insertTrack(qtractorTrack *pTrack, qtractorTrack *pPrevTrack = nullptr);
	void moveTrack(qtractorTrack *pTrack, qtractorTrack *pNextTrack);
	void updateTrack(qtractorTrack *pTrack);
	void unlinkTrack(qtractorTrack *pTrack);

	qtractorTrack *trackAt(int iTrack) const
		{ return m_tracks.at(iTrack); }

	// Current number of record-armed tracks.
	void setRecordTracks(bool bRecord);
	unsigned int recordTracks() const
		{ return m_iRecordTracks; }

	// Current number of mued tracks.
	void setMuteTracks(bool bMute);
	unsigned int muteTracks() const
		{ return m_iMuteTracks; }

	// Current number of solo tracks.
	void setSoloTracks(bool bSolo);
	unsigned int soloTracks() const
		{ return m_iSoloTracks; }

	// Temporary current track accessors.
	void setCurrentTrack(qtractorTrack *pTrack);
	qtractorTrack *currentTrack() const
		{ return m_pCurrentTrack; }

	// Temporary current track predicates.
	bool isTrackMonitor(qtractorTrack *pTrack) const;
	bool isTrackMidiChannel(qtractorTrack *pTrack,
		unsigned short iChannel) const;

	// Session cursor factory methods.
	qtractorSessionCursor *createSessionCursor(unsigned long iFrame = 0,
		qtractorTrack::TrackType syncType = qtractorTrack::None);
	void unlinkSessionCursor(qtractorSessionCursor *pSessionCursor);

	// Reset (reactivate) all plugin chains...
	void resetAllPlugins();

	// Force (deactivate) all plugin chains...
	void deactivateAllPlugins();

	// Force (disconnect) all existing connections...
	void disconnectAll();

	// Device engine accessors.
	qtractorAudioEngine *audioEngine() const { return m_pAudioEngine; }
	qtractorMidiEngine  *midiEngine()  const { return m_pMidiEngine;  }

	// Wait for application stabilization.
	static void stabilize(int msecs = 20);

	// Consolidated session engine activation status.
	bool isActivated() const;

	// Session RT-safe pseudo-locking primitives.
	bool acquire();
	void release();
	void lock();
	void unlock();

	// Re-entrancy check.
	bool isBusy() const;

	// Consolidated session engine start status.
	void setPlaying(bool bPlaying);
	bool isPlaying() const;

	// Shutdown procedure.
	void shutdown();

	// (Hazardous) bi-directional locate method.
	void seek(unsigned long iFrame, bool bSync = false);

	// Playhead positioning.
	void setPlayHead(unsigned long iPlayHead);
	void setPlayHeadEx(unsigned long iPlayHead);
	unsigned long playHead() const;

	// Auto-backward play-head positioning.
	void setPlayHeadAutoBackward(unsigned long iPlayHead);
	unsigned long playHeadAutoBackward() const;

	// Edit-head positioning.
	void setEditHead(unsigned long iEditHead);
	unsigned long editHead() const { return m_iEditHead; }

	// Edit-tail positioning.
	void setEditTail(unsigned long iEditTail);
	unsigned long editTail() const  { return m_iEditTail; }

	// Session loop points accessors.
	void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd);
	unsigned long loopStart() const
		{ return m_iLoopStart; }
	unsigned long loopEnd() const
		{ return m_iLoopEnd; }
	bool isLooping() const
		{ return (m_iLoopStart < m_iLoopEnd); }

	unsigned long loopStartTime() const
		{ return m_iLoopStartTime; }
	unsigned long loopEndTime() const
		{ return m_iLoopEndTime; }

	// Session punch points accessors.
	void setPunch(unsigned long iPunchIn, unsigned long iPunchOut);
	unsigned long punchIn() const
		{ return m_iPunchIn; }
	unsigned long punchOut() const
		{ return m_iPunchOut; }
	bool isPunching() const
		{ return (m_iPunchIn < m_iPunchOut); }

	unsigned long punchInTime() const
		{ return m_iPunchInTime; }
	unsigned long punchOutTime() const
		{ return m_iPunchOutTime; }

	// Absolute frame time and offset.
	unsigned long frameTime() const;
	unsigned long frameTimeEx() const;

	// Sanitize a given name.
	static QString sanitize(const QString& s); 

	// Provide an unique track-name if applicable,
	// append an incremental numerical suffix...
	QString uniqueTrackName(const QString& sTrackName) const;

	void acquireTrackName(qtractorTrack *pTrack);
	void releaseTrackName(qtractorTrack *pTrack);

	// Transient file-name registry methods as far to
	// avoid duplicates across load/save/record cycles...
	void acquireFilePath(const QString& sFilename);
	void releaseFilePath(const QString& sFilename);

	// Create a brand new filename (absolute file path).
	QString createFilePath(
		const QString& sBaseName, const QString& sExt, bool bAcquire = false);

	// Session directory relative/absolute file path helpers.
	QString relativeFilePath(const QString& sFilename) const;
	QString absoluteFilePath(const QString& sFilename) const;

	// Consolidated session record state.
	void setRecording(bool bRecording);
	bool isRecording() const { return m_bRecording; }

	// Loop-recording/take mode.
	void setLoopRecordingMode(int iLoopRecordingMode)
		{ m_iLoopRecordingMode = iLoopRecordingMode; }
	int loopRecordingMode() const
		{ return m_iLoopRecordingMode; }

	// Track recording specifics.
	unsigned short audioRecord() const { return m_iAudioRecord; }
	unsigned short midiRecord()  const { return m_iMidiRecord;  }

	// Special track-immediate methods.
	void trackRecord(qtractorTrack *pTrack, bool bRecord,
		unsigned long iClipStart, unsigned long iFrameTime);
	void trackMute(qtractorTrack *pTrack, bool bMute);
	void trackSolo(qtractorTrack *pTrack, bool bSolo);

	// Special auto-plugin-deactivation
	void autoDeactivatePlugins(bool bForce = false);
	void setAutoDeactivate(bool bOn);
	bool isAutoDeactivate() const { return m_bAutoDeactivate; }

	// Audio peak factory accessor.
	qtractorAudioPeakFactory *audioPeakFactory() const
		{ return m_pAudioPeakFactory; }

	// MIDI track tagging specifics.
	void acquireMidiTag(qtractorTrack *pTrack);
	void releaseMidiTag(qtractorTrack *pTrack);
	unsigned short midiTag() const { return m_iMidiTag; }

	// MIDI session/tracks instrument/controller patching (conditional).
	void resetAllMidiControllers(bool bForceImmediate);

	// MIDI manager list accessors.
	void addMidiManager(qtractorMidiManager *pMidiManager);
	void removeMidiManager(qtractorMidiManager *pMidiManager);

	const qtractorList<qtractorMidiManager>& midiManagers() const
		{ return m_midiManagers; }

	// Auto time-stretching global flag (when tempo changes)
	void setAutoTimeStretch(bool bAutoTimeStretch)
		{ m_bAutoTimeStretch = bAutoTimeStretch; }
	bool isAutoTimeStretch() const
		{ return m_bAutoTimeStretch; }

	// Session special process cycle executive.
	void process(qtractorSessionCursor *pSessionCursor,
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Session special process record executive (audio recording only).
	void process_record(
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Session special process automation executive.
	void process_curve(unsigned long iFrame);

	// Forward declaration.
	class Document;

	// Document element methods.
	bool loadElement(Document *pDocument, QDomElement *pElement);
	bool saveElement(Document *pDocument, QDomElement *pElement);

	// Session property structure.
	struct Properties
	{
		// Default constructor.
		Properties()
			{ clear(); }
		// Copy constructor.
		Properties(const Properties& props)
			{ copy(props); }
		// Assignment operator,
		Properties& operator=(const Properties& props)
			{ return copy(props); }
		// Helper copy method.
		Properties& copy(const Properties& props);
		// Helper clear/reset method.
		void clear();
		// Members.
		QString sessionDir;
		QString sessionName;
		QString description;
		// Intrinsic time scale.
		qtractorTimeScale timeScale;
	};

	// Alternate properties accessor.
	Properties& properties() { return m_props; }

	// Session command executive (undo/redo)
	bool execute(qtractorCommand *pCommand);

	// Session command list reference (undo/redo)
	qtractorCommandList *commands() const
		{ return m_pCommands; }

	// Instrument names mapping.
	qtractorInstrumentList *instruments() const
		{ return m_pInstruments; }

	// Manage curve-lists to specific tracks.
	void acquireTrackCurveList(qtractorTrack *pTrack);
	void releaseTrackCurveList(qtractorTrack *pTrack);

	// Find track of specific curve-list.
	qtractorTrack *findTrackCurveList(qtractorCurveList *pCurveList) const;

	// Find track of specific name.
	qtractorTrack *findTrack(const QString& sTrackName) const;

	// Session files registry accessor.
	qtractorFileList *files() const { return m_pFiles; }

	// Rename session files...
	void renameSession(const QString& sOldName, const QString& sNewName);

	// MIDI time adjust to/from official high resolution queue (64bit).
	unsigned long timep ( unsigned long time ) const
		{ return m_props.timeScale.timep(time); }
	unsigned long timeq ( unsigned long time ) const
		{ return m_props.timeScale.timeq(time); }

	// Pseudo-singleton instance accessor.
	static qtractorSession *getInstance();

private:

	// check if plugin can be auto deactivated
	// for now private - maybe helpful for others?
	bool canTrackBeAutoDeactivated(qtractorTrack *pTrack) const;
	// Restore activation state
	void undoAutoDeactivatePlugins();

	Properties     m_props;             // Session properties.

	unsigned long  m_iSessionStart;     // Session start in frames.
	unsigned long  m_iSessionEnd;		// Session end in frames.

	unsigned int   m_iRecordTracks;     // Current number of record-armed tracks.
	unsigned int   m_iMuteTracks;       // Current number of muted tracks.
	unsigned int   m_iSoloTracks;       // Current number of solo tracks.

	qtractorTrack *m_pCurrentTrack;     // Temporary current track.

	// The track list.
	qtractorList<qtractorTrack> m_tracks;

	// Managed session cursors.
	qtractorList<qtractorSessionCursor> m_cursors;

	// Device engine common client name.
	QString m_sClientName;

	// Device engine instances.
	qtractorMidiEngine  *m_pMidiEngine;
	qtractorAudioEngine *m_pAudioEngine;

	// Audio peak factory (singleton) instance.
	qtractorAudioPeakFactory *m_pAudioPeakFactory;

	// Track recording counts.
	unsigned short m_iAudioRecord;
	unsigned short m_iMidiRecord;

	// MIDI track tagging specifics.
	unsigned short m_iMidiTag;
	QList<unsigned short> m_midiTags;

	// Base edit members.
	unsigned long m_iEditHead;
	unsigned long m_iEditTail;
	// Session time-normalized edit points.
	unsigned long m_iEditHeadTime;
	unsigned long m_iEditTailTime;

	// Session loop points.
	unsigned long m_iLoopStart;
	unsigned long m_iLoopEnd;
	// Session time-normalized loop points.
	unsigned long m_iLoopStartTime;
	unsigned long m_iLoopEndTime;

	// Session punch points.
	unsigned long m_iPunchIn;
	unsigned long m_iPunchOut;
	// Session time-normalized punch points.
	unsigned long m_iPunchInTime;
	unsigned long m_iPunchOutTime;

	unsigned long m_iPlayHeadAutoBackward;

	// Consolidated record state.
	bool m_bRecording;

	// Loop-recording/take mode.
	int m_iLoopRecordingMode;

	// Auto time-stretching global flag (when tempo changes)
	bool m_bAutoTimeStretch;

	// Auto disable plugins flag
	bool m_bAutoDeactivate;

	// MIDI plugin manager list.
	qtractorList<qtractorMidiManager> m_midiManagers;

	// RT-safeness hackish lock-mutex.
	qtractorAtomic m_locks;
	qtractorAtomic m_mutex;

	// Instrument names mapping.
	qtractorInstrumentList *m_pInstruments;

	// Session command executive (undo/redo)
	qtractorCommandList *m_pCommands;

	// Curve-to-track mapping.
	QHash<qtractorCurveList *, qtractorTrack *> m_curves;

	// File registry.
	qtractorFileList *m_pFiles;

	// Transient file-name registry.
	QStringList m_filePaths;

	// Track-name registry.
	QHash<QString, qtractorTrack *> m_trackNames;

	// The pseudo-singleton instance.
	static qtractorSession *g_pSession;
};


//-------------------------------------------------------------------------
// qtractorSession::Document -- Session file import/export helper class.
//

class qtractorSession::Document : public qtractorDocument
{
public:

	// Constructor.
	Document(QDomDocument *pDocument,
		qtractorSession *pSession, qtractorFiles *pFiles);
	// Default destructor.
	~Document();

	// Property accessors.
	qtractorSession *session() const
		{ return m_pSession; }
	qtractorFiles *files() const
		{ return m_pFiles; }

protected:

	// Elemental loader/savers...
	bool loadElement(QDomElement *pElement);
	bool saveElement(QDomElement *pElement);

private:

	// Instance variables.
	qtractorSession *m_pSession;
	qtractorFiles   *m_pFiles;
};


#endif  // __qtractorSession_h


// end of qtractorSession.h
