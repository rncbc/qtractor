// qtractorTracks.h
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <qsplitter.h>
#include <qtoolbutton.h>

// Forward declarations.
class qtractorInstrumentList;
class qtractorTrackButton;
class qtractorTrackList;
class qtractorTrackTime;
class qtractorTrackView;
class qtractorTrack;
class qtractorSession;


class QScrollView;


//----------------------------------------------------------------------------
// qtractorTracks -- The main session track listview widget.

class qtractorTracks : public QSplitter
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTracks(QWidget *pParent, const char *pszName = 0);
	// Destructor.
	~qtractorTracks();

	// Session helper accessor.
	qtractorSession *session() const;

	// Instrument list accessor.
	qtractorInstrumentList *instruments() const;

	// Child widgets accessors.
	qtractorTrackList *trackList() const;
	qtractorTrackTime *trackTime() const;
	qtractorTrackView *trackView() const;

	// Common scroll view positional sync method.
	void setContentsPos(QScrollView *pScrollView, int cx, int cy);

	// Update/sync from session tracks.
	void updateContents(bool bRefresh = false);

	// Primordial track management methods.
	qtractorTrack *currentTrack() const;
	bool addTrack();
	bool removeTrack(qtractorTrack *pTrack = NULL);
	bool editTrack(qtractorTrack *pTrack = NULL);

	// Import Audio/MIDI files into new tracks...
	bool addAudioTracks(QStringList files, unsigned long iClipStart = 0);
	bool addMidiTracks(QStringList files, unsigned long iClipStart = 0);
	bool addMidiTrackChannel(const QString& sPath, int iTrackChannel,
		unsigned long iClipStart = 0);

	// MIDI track/bus/channel alias active maintenance method.
	void updateMidiTrack(qtractorTrack *pMidiTrack);

	// Whether there's any clip currently selected.
	bool isClipSelected() const;

	// Whether there's any clip on clipboard.
	bool isClipboardEmpty() const;

	// Clipboard methods.
	void cutClipSelect();
	void copyClipSelect();
	void pasteClipSelect();

	// Delete selection method.
	void deleteClipSelect();

	// Selection methods.
	void selectEditRange();
	void selectCurrentTrack(bool bReset = false);
	void selectAll(bool bSelect = true);

	// Simple main-form redirectors.
	void selectionChangeNotify();
	void contentsChangeNotify();

protected:

	// Zoom factor constants.
	enum { ZoomMin = 10, ZoomBase = 100, ZoomMax = 1000, ZoomStep = 10 };

	// Common zoom factor settlers.
	void horizontalZoomStep(int iZoomStep);
	void verticalZoomStep(int iZoomStep);
					
	// Close event override to save some geometry settings.
	virtual void closeEvent(QCloseEvent *pCloseEvent);

public slots:

	// Track button notification.
	void trackButtonToggledSlot(qtractorTrackButton *pTrackButton, bool bOn);

protected slots:

	// Early track list stabilization.
	void trackListPolishSlot();

	// Zoom view slots.
	void horizontalZoomInSlot();
	void horizontalZoomOutSlot();
	void verticalZoomInSlot();
	void verticalZoomOutSlot();
	void viewZoomToolSlot();

signals:

	// Emitted on late close.
	void closeNotifySignal();

private:

	// The main child widgets.
	qtractorTrackList *m_pTrackList;
	qtractorTrackTime *m_pTrackTime;
	qtractorTrackView *m_pTrackView;

	// To avoid contents sync moving recursion.
	int m_iContentsMoving;
};


#endif  // __qtractorTracks_h


// end of qtractorTracks.h
