// qtractorTrack.h
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorTrack_h
#define __qtractorTrack_h

#include "qtractorList.h"

#include "qtractorMidiControl.h"

#include <QColor>


// Forward declarations.
class qtractorSession;
class qtractorDocument;
class qtractorInstrumentList;
class qtractorPluginList;
class qtractorMonitor;
class qtractorClip;
class qtractorBus;

class qtractorSubject;
class qtractorMidiControlObserver;
class qtractorAudioBufferThread;
class qtractorCurveList;
class qtractorCurveFile;
class qtractorCurve;

// Special forward declarations.
class QDomElement;
class QPainter;
class QRect;


//-------------------------------------------------------------------------
// qtractorTrack -- Track container.

class qtractorTrack : public qtractorList<qtractorTrack>::Link
{
public:

	// Track type symbology.
	enum TrackType { None = 0, Audio, Midi };
	// Tool button specific types:
	enum ToolType  { Record, Mute, Solo };

	// Constructor.
	qtractorTrack(qtractorSession *pSession, TrackType trackType = None);
	// Default constructor.
	~qtractorTrack();

	// Reset track.
	void clear();

	// Track open/close methods.
	bool open();
	void close();

	// Session accessor.
	qtractorSession *session() const;

	// Track name accessors.
	void setTrackName(const QString& sTrackName);
	const QString& trackName() const;
	QString shortTrackName() const;
	void updateTrackName();

	static QString shortTrackName(const QString& sTrackName);

	// Track icon (filename) accessors.
	void setTrackIcon(const QString& sTrackIcon);
	const QString& trackIcon() const;

	// Track type accessors.
	void setTrackType(TrackType trackType);
	TrackType trackType() const;

	// Record monitoring state accessors.
	void setMonitor(bool bMonitor);
	bool isMonitor() const;

	// Record status accessors.
	void setRecord(bool bRecord);
	bool isRecord() const;

	// Mute status accessors.
	void setMute(bool bMute);
	bool isMute() const;

	// Solo status accessors.
	void setSolo(bool bSolo);
	bool isSolo() const;

	// Track gain (volume) accessor.
	void setGain(float fGain);
	float gain() const;
	float prevGain() const;

	// Track stereo-panning accessor.
	void setPanning(float fPanning);
	float panning() const;
	float prevPanning() const;

	// MIDI specific: track-tag accessors.
	void setMidiTag(unsigned short iMidiTag);
	unsigned short midiTag() const;

	// MIDI specific: omni (capture) mode acessors.
	void setMidiOmni(bool bMidiOmni);
	bool isMidiOmni() const;

	// MIDI specific: channel acessors.
	void setMidiChannel(unsigned short iMidiChannel);
	unsigned short midiChannel() const;

	// MIDI specific: bank select method acessors (optional).
	void setMidiBankSelMethod(int iMidiBankSelMethod);
	int midiBankSelMethod() const;

	// MIDI specific: bank acessors (optional).
	void setMidiBank(int iMidiBank);
	int midiBank() const;

	// MIDI specific: program acessors (optional).
	void setMidiProg(int iMidiProg);
	int midiProg() const;

	// MIDI drum mode (UI).
	void setMidiDrums(bool bMidiDrums);
	bool isMidiDrums() const;

	// MIDI specific: track-minimum note.
	void setMidiNoteMin(unsigned char note);
	unsigned char midiNoteMin() const;

	// MIDI specific: track-maximum note.
	void setMidiNoteMax(unsigned char note);
	unsigned char midiNoteMax() const;

	// MIDI specific volume controller.
	void setMidiVolume(unsigned char vol, bool bUpdate = false);
	unsigned char midiVolume() const;

	// MIDI specific panning controller.
	void setMidiPanning(unsigned char pan, bool bUpdate = false);
	unsigned char midiPanning() const;

	// Assigned bus name accessors.
	void setInputBusName(const QString& sBusName);
	const QString& inputBusName() const;
	void setOutputBusName(const QString& sBusName);
	const QString& outputBusName() const;

	// Assigned bus accessors.
	qtractorBus *inputBus() const;
	qtractorBus *outputBus() const;

	// Track monitor accessors.
	qtractorMonitor *monitor() const;

	// Track plugin-chain accessor.
	qtractorPluginList *pluginList() const;

	// Plugin latency compensation accessors.
	void setPluginListLatency(bool bPluginListLatency);
	bool isPluginListLatency() const;

	// Base height (in pixels).
	enum { HeightMin = 24, HeightBase = 96 };

	// Normalized view height accessors.
	void updateHeight();
	void setHeight(int iHeight);
	int height() const;

	// Visual height accessors.
	void updateZoomHeight();
	void setZoomHeight(int iZoomHeight);
	int zoomHeight() const;
	int zoomHeightBase() const;

	// Clip list management methods.
	const qtractorList<qtractorClip>& clips() const;

	void addClip(qtractorClip *pClip);
	void addClipEx(qtractorClip *pClip);
	void insertClip(qtractorClip *pClip);
	void removeClipEx(qtractorClip *pClip);
	void removeClip(qtractorClip *pClip);

	// Current clip on record (capture).
	void setClipRecord(qtractorClip *pClipRecord);
	qtractorClip *clipRecord() const;

	// Current clip on record absolute start frame (capture).
	void setClipRecordStart(unsigned long iClipRecordStart);
	unsigned long clipRecordStart() const;
	unsigned long clipRecordEnd(unsigned long iFrameTime) const;

	void setClipRecordEx(bool bClipRecordEx);
	bool isClipRecordEx() const;

	// Background color accessors.
	void setBackground(const QColor& bg);
	const QColor& background() const;

	// Foreground color accessors.
	void setForeground(const QColor& fg);
	const QColor& foreground() const;

	// Generate a default track color.
	static QColor trackColor(int iTrack);

	// Track special process cycle executive.
	void process(qtractorClip *pClip,
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Track freewheeling process cycle executive (needed for export).
	void process_export(qtractorClip *pClip,
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Track special process record executive (audio recording only).
	void process_record(
		unsigned long iFrameStart, unsigned long iFrameEnd);

	// Track special process automation executive.
	void process_curve(unsigned long iFrame);

	// Track paint method.
	void drawTrack(QPainter *pPainter, const QRect& trackRect,
		unsigned long iTrackStart, unsigned long iTrackEnd,
		qtractorClip *pClip = nullptr);

	// MIDI track instrument patching.
	void setMidiPatch(qtractorInstrumentList *pInstruments);

	// Track loop point setler.
	void setLoop(unsigned long iLoopStart, unsigned long iLoopEnd);

	// Update all clips editors.
	void updateClipEditors();

	// Audio buffer ring-cache (playlist) methods.
	qtractorAudioBufferThread *syncThread();

	// Track state (monitor, record, mute, solo) button setup.
	qtractorSubject *monitorSubject() const;
	qtractorSubject *recordSubject() const;
	qtractorSubject *muteSubject() const;
	qtractorSubject *soloSubject() const;

	qtractorMidiControlObserver *monitorObserver() const;
	qtractorMidiControlObserver *recordObserver() const;
	qtractorMidiControlObserver *muteObserver() const;
	qtractorMidiControlObserver *soloObserver() const;

	// Track state (monitor) notifier (proto-slot).
	void monitorChangeNotify(bool bOn);

	// Track state (record, mute, solo) notifier (proto-slot).
	void stateChangeNotify(ToolType toolType, bool bOn);

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Load/save track state (record, mute, solo) controllers (MIDI).
	void loadControllers(QDomElement *pElement);
	void saveControllers(qtractorDocument *pDocument, QDomElement *pElement) const;

	// Map track state (record, mute, solo) controllers (MIDI).
	void mapControllers();

	// Track automation curve list accessor.
	qtractorCurveList *curveList() const;

	// Track automation curve serializer accessor.
	qtractorCurveFile *curveFile() const;

	// Track automation current curve accessor.
	void setCurrentCurve(qtractorCurve *pCurrentCurve);
	qtractorCurve *currentCurve() const;

	// Track automation curve serialization methods.
	static void loadCurveFile(
		QDomElement *pElement, qtractorCurveFile *pCurveFile);
	void saveCurveFile(qtractorDocument *pDocument,
		QDomElement *pElement, qtractorCurveFile *pCurveFile) const;
	void applyCurveFile(qtractorCurveFile *pCurveFile) const;

	// Track properties structure.
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
		QString        trackName;
		QString        trackIcon;
		TrackType      trackType;
		bool           monitor;
		bool           record;
		bool           mute;
		bool           solo;
		float          gain;
		float          panning;
		QString        inputBusName;
		QString        outputBusName;
		bool           pluginListLatency;
		bool           midiOmni;
		unsigned short midiChannel;
		int            midiBankSelMethod;
		int            midiBank;
		int            midiProg;
		bool           midiDrums;
		QColor         foreground;
		QColor         background;
	};

	// Alternate properties accessor.
	void setProperties(const Properties& props);
	Properties& properties();

	// Reset state properties (as needed on copy/duplicate)
	void resetProperties();

	// Track type textual helper methods.
	static TrackType trackTypeFromText (const QString& sText);
	static QString   textFromTrackType (TrackType trackType);

	// Take(record) descriptor/id registry methods.
	class TakeInfo;

	TakeInfo *takeInfo(int iTakeID) const;
	int takeInfoId(TakeInfo *pTakeInfo) const;

	int takeInfoNew(TakeInfo *pTakeInfo) const;
	void takeInfoAdd(int iTakeID, TakeInfo *pTakeInfo) const;

	void clearTakeInfo() const;

	// Update tracks/list-view.
	void updateTrack();
	void updateMidiTrack();
	void updateMidiClips();

	// Update all plugin forms, if visible.
	void refreshPluginForms();

	// Default track color saturation factor [0..500].
	static void setTrackColorSaturation(int iTrackColorSaturation);
	static int trackColorSaturation();

private:

	qtractorSession *m_pSession;    // Session reference.

	Properties       m_props;       // Track properties.

	qtractorBus     *m_pInputBus;   // Track assigned input bus.
	qtractorBus     *m_pOutputBus;  // Track assigned input bus.

	qtractorMonitor *m_pMonitor;    // Track monitor.

	unsigned short   m_iMidiTag;    // MIDI specific: track-tag;

	unsigned char    m_midiNoteMax; // MIDI specific: track-maximum note;
	unsigned char    m_midiNoteMin; // MIDI specific: track-minimum note.

	unsigned char    m_midiVolume;  // MIDI specific: track-volume;
	unsigned char    m_midiPanning; // MIDI specific: track-panning.

	int              m_iHeight;     // View height (normalized).
	int              m_iHeightBase; // View height (base).
	int              m_iZoomHeight; // View height (zoomed).

	qtractorList<qtractorClip> m_clips; // List of clips.

	qtractorClip *m_pClipRecord;        // Current clip on record (capture).
	unsigned long m_iClipRecordStart;   // Current clip on record start frame.

	bool m_bClipRecordEx;               // Current clip on record/overdub flag.

	qtractorPluginList *m_pPluginList;	// Plugin chain (audio).

	// Audio buffer ring-cache (playlist).
	qtractorAudioBufferThread *m_pSyncThread;

	// MIDI track/channel (volume, panning) observers.
	class MidiVolumeObserver;
	class MidiPanningObserver;

	MidiVolumeObserver  *m_pMidiVolumeObserver;
	MidiPanningObserver *m_pMidiPanningObserver;

	// State (monitor, record, mute, solo) observer stuff.
	qtractorMidiControlObserver *m_pMonitorObserver;

	class StateObserver;

	StateObserver   *m_pRecordObserver;
	StateObserver   *m_pMuteObserver;
	StateObserver   *m_pSoloObserver;

	qtractorSubject *m_pMonitorSubject;
	qtractorSubject *m_pRecordSubject;
	qtractorSubject *m_pMuteSubject;
	qtractorSubject *m_pSoloSubject;

	qtractorMidiControl::Controllers m_controllers;

	qtractorCurveFile *m_pCurveFile;

	// Take(record) descriptor/id registry.
	mutable QHash<int, TakeInfo *> m_idtakes;
	mutable QHash<TakeInfo *, int> m_takeids;

	// MIDI bank/program observer.
	class MidiProgramObserver;

	MidiProgramObserver *m_pMidiProgramObserver;

	// Default track color saturation factor [0..500].
	static int g_iTrackColorSaturation;
};


#endif  // __qtractorTrack_h


// end of qtractorTrack.h
