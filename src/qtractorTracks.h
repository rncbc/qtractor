// qtractorTracks.h
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
class qtractorInstrumentList;
class qtractorTrackButton;
class qtractorTrackItemWidget;
class qtractorTrackList;
class qtractorTrackTime;
class qtractorTrackView;
class qtractorTrack;
class qtractorSession;
class qtractorClip;


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

	qtractorTrackItemWidget *currentTrackWidget() const;

	// Import Audio/MIDI files into new tracks...
	bool addAudioTracks(QStringList files, unsigned long iClipStart = 0);
	bool addMidiTracks(QStringList files, unsigned long iClipStart = 0);
	bool addMidiTrackChannel(const QString& sPath, int iTrackChannel,
		unsigned long iClipStart = 0);

	// MIDI track/bus/channel alias active maintenance method.
	void updateMidiTrack(qtractorTrack *pMidiTrack);

	// Primordial clip management methods.
	qtractorClip *currentClip() const;

	bool newClip();
	bool editClip(qtractorClip *pClip = NULL);
	bool splitClip(qtractorClip *pClip = NULL);

	// Whether there's any clip currently selected.
	bool isClipSelected() const;

	// Whether there's any clip on clipboard.
	bool isClipboardEmpty() const;

	// Clipboard methods.
	void cutClipboard();
	void copyClipboard();
	void pasteClipboard();

	// Delete selection method.
	void deleteClipSelect();

	// Selection methods.
	void selectEditRange();
	void selectCurrentTrack(bool bReset = false);
	void selectAll(bool bSelect = true);

	// Simple main-form redirectors.
	void selectionChangeNotify();
	void contentsChangeNotify();

	// Overall contents reset.
	void clear();

public slots:

	// Track button notification.
	void trackButtonToggledSlot(qtractorTrackButton *pTrackButton, bool bOn);

	// Zoom view slots.
	void zoomIn();
	void zoomOut();
	void zoomReset();

protected:

	// Zoom factor constants.
	enum { ZoomMin = 10, ZoomBase = 100, ZoomMax = 1000, ZoomStep = 10 };

	// Common zoom factor settlers.
	void horizontalZoomStep(int iZoomStep);
	void verticalZoomStep(int iZoomStep);

	// Try to center horizontally/vertically
	// (usually after zoom change)
	void centerContents();

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
};


#endif  // __qtractorTracks_h


// end of qtractorTracks.h
