// qtractorFiles.h
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

#ifndef __qtractorFiles_h
#define __qtractorFiles_h

#include "qtractorAudioListView.h"
#include "qtractorMidiListView.h"

#include <QDockWidget>


// Forward declarations.
class qtractorFilesTabWidget;

class QHBoxLayout;
class QToolButton;


//-------------------------------------------------------------------------
// qtractorFiles - File/Groups dockable window.
//

class qtractorFiles : public QDockWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorFiles(QWidget *pParent);
	// Destructor.
	~qtractorFiles();

	// The fixed tab page indexes.
	enum PageIndex { Audio = 0, Midi = 1 };

	// File list view accessors.
	qtractorAudioListView *audioListView() const;
	qtractorMidiListView  *midiListView()  const;

	// Clear evrything on sight.
	void clear();

	// Check whether one of the widgets has focus (oveerride method).
	bool hasFocus() const;

	// Tell whether a file item is currently selected.
	bool isFileSelected() const;

	// File addition Convenience helper methods.
	void addAudioFile (const QString& sFilename);
	void addMidiFile  (const QString& sFilename);

	// File selection Convenience helper methods.
	void selectAudioFile (const QString& sFilename);
	void selectMidiFile  (const QString& sFilename, int iTrackChannel);

	// Audition/pre-listening player methods.
	void setPlayState(bool bOn);
	bool isPlayState() const;

public slots:

	// Cut current file item(s) to clipboard.
	void cutItemSlot();
	// Copy current file item(s) to clipboard.
	void copyItemSlot();
	// Paste file item(s) from clipboard.
	void pasteItemSlot();
	// Remove current group/file item(s).
	void removeItemSlot();
	
protected slots:

	// Add a new group item below the current one.
	void newGroupSlot();
	// Add a new file item below the current group one.
	void openFileSlot();
	// Rename current group/file item.
	void renameItemSlot();
	// Audition/pre-listening player slots.
	void playSlot(bool bOn);
	// Clean-up unused file items.
	void cleanupSlot();

	// Tab page switch slots.
	void pageAudioSlot();
	void pageMidiSlot();

	// Usual stabilizing slot.
	void stabilizeSlot();

protected:

	// Just about to notify main-window that we're closing.
	void closeEvent(QCloseEvent *);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

	// Retrieve current selected file list view.
	qtractorFileListView *currentFileListView() const;

private:

	// File type selection tab widget.
	qtractorFilesTabWidget *m_pTabWidget;

	// Specific file type widgets.
	qtractorAudioListView *m_pAudioListView;
	qtractorMidiListView  *m_pMidiListView;

	// Audition/pre-listening controls.
	QWidget     *m_pPlayWidget;
	QHBoxLayout *m_pPlayLayout;
	QToolButton *m_pPlayButton;
	int          m_iPlayUpdate;

	// List view actions.
	QAction *m_pNewGroupAction;
	QAction *m_pOpenFileAction;
	QAction *m_pCutItemAction;
	QAction *m_pCopyItemAction;
	QAction *m_pPasteItemAction;
	QAction *m_pRenameItemAction;
	QAction *m_pRemoveItemAction;
	QAction *m_pPlayItemAction;
	QAction *m_pCleanupAction;
};


#endif  // __qtractorFiles_h


// end of qtractorFiles.h
