// qtractorTracks.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorTracks_h
#define __qtractorTracks_h

#include <QSplitter>

// Forward declarations.
class qtractorTrackList;
class qtractorTrackTime;
class qtractorTrackView;
class qtractorTrack;
class qtractorClip;
class qtractorClipCommand;
class qtractorClipRangeCommand;
class qtractorClipToolCommand;

class qtractorMidiToolsForm;
class qtractorMidiImportExtender;


//----------------------------------------------------------------------------
// qtractorTracks -- The main session track listview widget.

class qtractorTracks : public QSplitter
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTracks(QWidget *pParent);
	// Destructor.
	~qtractorTracks();

	// Child widgets accessors.
	qtractorTrackList *trackList() const;
	qtractorTrackTime *trackTime() const;
	qtractorTrackView *trackView() const;

	// Update/sync from session tracks.
	void updateContents(bool bRefresh = false);

	// Primordial track management methods.
	qtractorTrack *currentTrack() const;

	bool addTrack();
	bool removeTrack(qtractorTrack *pTrack = NULL);
	bool editTrack(qtractorTrack *pTrack = NULL);
	bool copyTrack(qtractorTrack *pTrack = NULL);

	// Import Audio/MIDI files into new tracks...
	bool addAudioTracks(const QStringList& files,
		unsigned long iClipStart, qtractorTrack *pAfterTrack = NULL);
	bool addMidiTracks(const QStringList& files,
		unsigned long iClipStart, qtractorTrack *pAfterTrack = NULL,
		qtractorMidiImportExtender *pMidiImportExtender = NULL);
	bool addMidiTrackChannel(const QString& sPath, int iTrackChannel,
		unsigned long iClipStart, qtractorTrack *pAfterTrack = NULL);

	// Track-list active maintenance update.
	void updateTrack(qtractorTrack *pTrack = NULL);

	// MIDI track/bus/channel alias active maintenance method.
	void updateMidiTrack(qtractorTrack *pMidiTrack);

	// Primordial clip management methods.
	qtractorClip *currentClip() const;

	bool newClip();
	bool editClip(qtractorClip *pClip = NULL);
	bool unlinkClip(qtractorClip *pClip = NULL);
	bool splitClip(qtractorClip *pClip = NULL);
	bool normalizeClip(qtractorClip *pClip = NULL);
	bool rangeClip(qtractorClip *pClip = NULL);
	bool loopClip(qtractorClip *pClip = NULL);
	bool tempoClip(qtractorClip *pClip = NULL);
	bool crossFadeClip(qtractorClip *pClip = NULL);
	bool executeClipTool(int iTool, qtractorClip *pClip = NULL);
	bool importClips(QStringList files, unsigned long iClipStart = 0);
	bool exportClips();
	bool mergeClips();

	// Whether there's anything currently selected.
	bool isSelected() const;

	// Whether there's any clip currently selected.
	bool isClipSelected() const;

	// Whether there's any curve/automation currently selected.
	bool isCurveSelected() const;

	// Whether there's a single track selection.
	qtractorTrack *singleTrackSelected();

	// Retrieve actual clip selection range.
	void clipSelectedRange(
		unsigned long& iSelectStart,
		unsigned long& iSelectEnd) const;

	// Clipboard methods.
	void cutClipboard();
	void copyClipboard();
	void pasteClipboard();

	// Special paste/repeat prompt.
	void pasteRepeatClipboard();

	// Delete selection method.
	void deleteSelect();

	// Split selection method.
	void splitSelect();

	// Selection methods.
	void selectEditRange(bool bReset = false);
	void selectCurrentTrack(bool bReset = false);
	void selectCurrentTrackRange(bool bReset = false);
	void selectAll();
	void selectNone();
	void selectInvert();

	// Insertion and removal methods.
	bool insertEditRange(qtractorTrack *pTrack = NULL);
	bool removeEditRange(qtractorTrack *pTrack = NULL);

	// Simple main-form redirectors.
	void selectionChangeNotify();
	void contentsChangeNotify();
	void dirtyChangeNotify();

	// Overall selection clear/reset.
	void clearSelect(bool bReset = false);

	// Overall selection update.
	void updateSelect();

	// Overall contents reset.
	void clear();

	// Zoom (view) modes.
	enum { ZoomNone = 0, ZoomHorizontal = 1, ZoomVertical = 2, ZoomAll = 3 };

	void setZoomMode(int iZoomMode);
	int zoomMode() const;

	// Zoom view actuators.
	void zoomIn();
	void zoomOut();
	void zoomReset();

	// Track-list update (current track only).
	void updateTrackList(qtractorTrack *pTrack = NULL);

	// Update/sync recording tracks.
	void updateContentsRecord();

protected:

	// Zoom factor constants.
	enum { ZoomMin = 10, ZoomBase = 100, ZoomMax = 1000, ZoomStep = 10 };

	// Zoom step evaluator.
	int zoomStep() const;

	// Common zoom factor settlers.
	void horizontalZoomStep(int iZoomStep);
	void verticalZoomStep(int iZoomStep);

	// Zoom centering context.
	struct ZoomCenter
	{
		int x, y;
		unsigned long frame;
	};

	// Zoom centering prepare and post methods.
	void zoomCenterPre(ZoomCenter& zc) const;
	void zoomCenterPost(const ZoomCenter& zc);

	// Multi-clip command builders.
	bool normalizeClipCommand(
		qtractorClipCommand *pClipCommand, qtractorClip *pClip);
	bool executeClipToolCommand(
		qtractorClipToolCommand *pClipToolCommand, qtractorClip *pClip,
		qtractorMidiToolsForm *pMidiToolsForm);

	// Common clip-export/merge methods.
	bool mergeExportClips(qtractorClipCommand *pClipCommand);

	// Specialized clip-export/merge methods.
	bool mergeExportAudioClips(qtractorClipCommand *pClipCommand);
	bool mergeExportMidiClips(qtractorClipCommand *pClipCommand);

	bool rangeClipEx(qtractorClip *pClip, bool bLoopSet);

	// Insertion and removal methods (track).
	int insertEditRangeTrack(
		qtractorClipRangeCommand *pClipRangeCommand, qtractorTrack *pTrack,
        unsigned long iInsertStart, unsigned long iInsertEnd,
        unsigned int iInsertOptions) const;
	int removeEditRangeTrack(
		qtractorClipRangeCommand *pClipRangeCommand, qtractorTrack *pTrack,
        unsigned long iRemoveStart, unsigned long iRemoveEnd,
        unsigned int iRemoveOptions) const;

public slots:

	// Track-view update (obviously a slot).
	void updateTrackView();

protected slots:

	// Zoom view slots.
	void horizontalZoomInSlot();
	void horizontalZoomOutSlot();
	void verticalZoomInSlot();
	void verticalZoomOutSlot();
	void viewZoomResetSlot();

private:

	// The main child widgets.
	qtractorTrackList *m_pTrackList;
	qtractorTrackTime *m_pTrackTime;
	qtractorTrackView *m_pTrackView;

	// Zoom mode flag.
	int m_iZoomMode;
};


#endif  // __qtractorTracks_h


// end of qtractorTracks.h
