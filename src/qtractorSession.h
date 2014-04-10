// qtractorSession.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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


// Forward declarations.
class qtractorClip;

// Special forward declarations.
class qtractorMidiEngine;
class qtractorAudioEngine;
class qtractorAudioPeakFactory;
class qtractorSessionCursor;
class qtractorSessionDocument;
class qtractorMidiManager;
class qtractorInstrumentList;
class qtractorCommandList;
class qtractorCommand;
class qtractorFileList;

class QDomElement;


//-------------------------------------------------------------------------
// qtractorSession -- Session container (singleton).

class qtractorSession
{
public:

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
	const QString& sessionDir() const;

	// Session filename accessors.
	void setSessionName(const QString& sSessionName);
	const QString& sessionName() const;

	// Session description accessors.
	void setDescription(const QString& sDescription);
	const QString& description() const;

	// Session startlength mod-accessors.
	void updateSession(
		unsigned long iSessionStart = 0, unsigned long iSessionEnd = 0);
	unsigned long sessionStart() const;
	unsigned long sessionEnd() const;

	// Time-scale helper accessors.
	qtractorTimeScale *timeScale();

	// Device engine common client name accessors.
	void setClientName(const QString& sClientName);
	const QString& clientName() const;

	// Sample rate accessors.
	void setSampleRate(unsigned int iSampleRate);
	unsigned int sampleRate() const;

	// Session tempo accessors.
	void setTempo(float fTempo);
	float tempo() const;

	// Tempo beat type accessors.
	void setBeatType(unsigned short iBeatType);
	unsigned short beatType() const;

	// Resolution accessors.
	void setTicksPerBeat(unsigned short iTicksPerBeat);
	unsigned short ticksPerBeat() const;

	// Beats/Bar(measure) accessors.
	void setBeatsPerBar(unsigned short iBeatsPerBar);
	unsigned short beatsPerBar() const;

	// Time signature (denominator) accessors.
	void setBeatDivisor(unsigned short iBeatDivisor);
	unsigned short beatDivisor() const;

	// Horizontal zoom factor.
	void setHorizontalZoom(unsigned short iHorizontalZoom);
	unsigned short horizontalZoom() const;

	// Vertical zoom factor.
	void setVerticalZoom(unsigned short iVerticalZoom);
	unsigned short verticalZoom() const;

	// Pixels per beat (width).	
	void setPixelsPerBeat(unsigned short iPixelsPerBeat);
	unsigned short pixelsPerBeat() const;

	// Beat divisor (snap) accessors.
	void setSnapPerBeat(unsigned short iSnapPerBeat);
	unsigned short snapPerBeat(void) const;

	// Pixel/Tick number conversion.
	unsigned long tickFromPixel(unsigned int x);
	unsigned int pixelFromTick(unsigned long iTick);

	// Pixel/Frame number conversion.
	unsigned long frameFromPixel(unsigned int x) const;
	unsigned int pixelFromFrame(unsigned long iFrame) const;

	// Beat/frame conversion.
	unsigned long frameFromBeat(unsigned int iBeat);
	unsigned int beatFromFrame(unsigned long iFrame);

	// Tick/Frame number conversion.
	unsigned long frameFromTick(unsigned long iTick);
	unsigned long tickFromFrame(unsigned long iFrame);

	// Tick/Frame range conversion (delta conversion).
	unsigned long frameFromTickRange(
	    unsigned long iTickStart, unsigned long iTickEnd);
	unsigned long tickFromFrameRange(
	    unsigned long iFrameStart, unsigned long iFrameEnd);

	// Beat/frame snap filters.
	unsigned long tickSnap(unsigned long iTick);
	unsigned long frameSnap(unsigned long iFrame);
	unsigned int pixelSnap(unsigned int x);

	// Frame/locate (SMPTE) conversion.
	unsigned long frameFromLocate(unsigned long iLocate) const;
	unsigned long locateFromFrame(unsigned long iFrame) const;

	// Song position pointer (SPP=MIDI beats) to frame converters.
	unsigned long  frameFromSongPos(unsigned short iSongPos);
	unsigned short songPosFromFrame(unsigned long iFrame);

	// Update time scale divisor factors.
	void updateTimeScale();

	// Update time resolution divisor factors.
	void updateTimeResolution(); 

	// Update from disparate sample-rate.
	void updateSampleRate(unsigned int iSampleRate);

	// Track list management methods.
	const qtractorList<qtractorTrack>& tracks() const;

	void addTrack(qtractorTrack *pTrack);
	void insertTrack(qtractorTrack *pTrack, qtractorTrack *pPrevTrack = NULL);
	void moveTrack(qtractorTrack *pTrack, qtractorTrack *pNextTrack);
	void updateTrack(qtractorTrack *pTrack);
	void unlinkTrack(qtractorTrack *pTrack);

	qtractorTrack *trackAt(int iTrack) const;

	// Current number of record-armed tracks.
	void setRecordTracks(bool bRecord);
	unsigned int recordTracks() const;

	// Current number of mued tracks.
	void setMuteTracks(bool bMute);
	unsigned int muteTracks() const;

	// Current number of solo tracks.
	void setSoloTracks(bool bSolo);
	unsigned int soloTracks() const;

	// Temporary current track accessors.
	void setCurrentTrack(qtractorTrack *pTrack);
	qtractorTrack *currentTrack() const;

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

	// Device engine accessors.
	qtractorMidiEngine  *midiEngine() const;
	qtractorAudioEngine *audioEngine() const;

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
	void setPlayHead(unsigned long iFrame);
	unsigned long playHead() const;

	// Edit-head positioning.
	void setEditHead(unsigned long iEditHead);
	unsigned long editHead() const;

	// Edit-tail positioning.
	void setEditTail(unsigned long iEditTail);
	unsigned long editTail() const;

	// Session loop points accessors.
	void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd);
	unsigned long loopStart() const;
	unsigned long loopEnd() const;
	bool isLooping() const;

	// Session punch points accessors.
	void setPunch(unsigned long iPunchIn, unsigned long iPunchOut);
	unsigned long punchIn() const;
	unsigned long punchOut() const;
	bool isPunching() const;

	unsigned long punchInTime() const;
	unsigned long punchOutTime() const;

	// Absolute frame time and offset.
	unsigned long frameTime() const;
	unsigned long frameTimeEx() const;

	// Sanitize a given name.
	static QString sanitize(const QString& s); 

	// Create a brand new filename (absolute file path).
	QString createFilePath(
		const QString& sTrackName, const QString& sExt, int iClipNo = 0);

	// Consolidated session record state.
	void setRecording(bool bRecording);
	bool isRecording() const;

	// Loop-recording/take mode.
	void setLoopRecordingMode(int iLoopRecordingMode);
	int loopRecordingMode() const;

	// Loop-recording/take state.
	bool isLoopRecording() const;

	// Track recording specifics.
	unsigned short audioRecord() const;
	unsigned short midiRecord() const;

	// Special track-immediate methods.
	void trackRecord(qtractorTrack *pTrack, bool bRecord,
		unsigned long iClipStart, unsigned long iFrameTime);
	void trackMute(qtractorTrack *pTrack, bool bMute);
	void trackSolo(qtractorTrack *pTrack, bool bSolo);

	// Audio peak factory accessor.
	qtractorAudioPeakFactory *audioPeakFactory() const;

	// MIDI track tagging specifics.
	unsigned short midiTag() const;
	void acquireMidiTag(qtractorTrack *pTrack);
	void releaseMidiTag(qtractorTrack *pTrack);

	// MIDI session/tracks instrument/controller patching (conditional).
	void setMidiPatch(bool bForceImmediate);

	// MIDI manager list accessors.
	void addMidiManager(qtractorMidiManager *pMidiManager);
	void removeMidiManager(qtractorMidiManager *pMidiManager);

	const qtractorList<qtractorMidiManager>& midiManagers() const;

	// Auto time-stretching global flag (when tempo changes)
	void setAutoTimeStretch(bool bAutoTimeStretch);
	bool isAutoTimeStretch() const;

	// Session special process cycle executive.
	void process(qtractorSessionCursor *pSessionCursor,
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Session special process record executive (audio recording only).
	void process_record(
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Session special process automation executive.
	void process_curve(unsigned long iFrame);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument, QDomElement *pElement);

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
	Properties& properties();

	// Session command executive (undo/redo)
	bool execute(qtractorCommand *pCommand);

	// Session command list reference (undo/redo)
	qtractorCommandList *commands() const;

	// Instrument names mapping.
	qtractorInstrumentList *instruments() const;

	// Find track of specific curve-list.
	qtractorTrack *findTrack(qtractorCurveList *pCurveList) const;

	// Session files registry accessor.
	qtractorFileList *files() const;

	// Singleton instance accessor.
	static qtractorSession *getInstance();

	// Singleton destroyer.
	static void Destroy();

protected:

	// Constructor.
	qtractorSession();

private:

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

	// Consolidated record state.
	bool m_bRecording;

	// Loop-recording/take mode.
	int m_iLoopRecordingMode;

	// Auto time-stretching global flag (when tempo changes)
	bool m_bAutoTimeStretch;

	// MIDI plugin manager list.
	qtractorList<qtractorMidiManager> m_midiManagers;

	// RT-safeness hackish lock-mutex.
	qtractorAtomic m_locks;
	qtractorAtomic m_mutex;

	// Re-entrancy mutex.
	qtractorAtomic m_busy;

	// Instrument names mapping.
	qtractorInstrumentList *m_pInstruments;

	// Session command executive (undo/redo)
	qtractorCommandList *m_pCommands;

	// Curve-to-track mapping.
	QHash<qtractorCurveList *, qtractorTrack *> m_curves;

	// File registry.
	qtractorFileList *m_pFiles;

	// The singleton instance.
	static qtractorSession *g_pSession;
};


#endif  // __qtractorSession_h


// end of qtractorSession.h
