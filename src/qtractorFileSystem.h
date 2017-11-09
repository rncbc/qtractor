// qtractorFileSystem.h
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

#ifndef __qtractorFileSystem_h
#define __qtractorFileSystem_h

#include <QDockWidget>


// forward decls.
class QAction;
class QToolButton;
class QComboBox;
class QTreeView;

class QModelIndex;

class QContextMenuEvent;


//----------------------------------------------------------------------------
// qtractorFileSystem -- Custom composite widget.

class qtractorFileSystem : public QDockWidget
{
	Q_OBJECT

public:

	// ctor.
	qtractorFileSystem(QWidget *pParent = 0);

	// dtor.
	~qtractorFileSystem();

	// Accessors.
	void setRootPath(const QString& sRootPath);
	QString rootPath() const;

	enum Flags {
		AllFiles     = 1,
		SessionFiles = 2,
		AudioFiles   = 4,
		MidiFiles    = 8,
		HiddenFiles  = 0x10
	};

	void setFlags(Flags flags, bool on = true);
	Flags flags() const;

	// Audition/pre-listening player methods.
	void setPlayState(bool bOn);
	bool isPlayState() const;

	// State saver/loader.
	QByteArray saveState() const;
	bool restoreState(const QByteArray& state);

protected slots:

	// Chdir slots.
	void homeClicked();
	void cdUpClicked();

	void rootPathActivated(const QString& sRootPath);
	void treeViewActivated(const QModelIndex& index);

	// Filter slots.
	void filterChanged();

	// Directory gathering thread doing sth...
	void restoreStateLoading(const QString& sPath);
	void restoreStateTimeout();

	// Audition/pre-listening player slots.
	void playSlot(bool bOn);

signals:

	// File entry activated.
	void activated(const QString& sFilename);

protected:

	// Model factory method.
	void updateRootPath(const QString& sRootPath);

	// Model stabilizers.
	void updateRootPath();
	void updateFilter();

	// Just about to notify main-window that we're closing.
	void closeEvent(QCloseEvent *);

	// context-menu event handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// Audition/pre-listening player method.
	void activateFile(const QString& sFilename);

private:

	// Member actions...
	QAction     *m_pHomeAction;
	QAction     *m_pCdUpAction;
	QAction     *m_pAllFilesAction;
	QAction     *m_pSessionFilesAction;
	QAction     *m_pAudioFilesAction;
	QAction     *m_pMidiFilesAction;
	QAction     *m_pHiddenFilesAction;
	QAction     *m_pPlayAction;

	// Member widgets...
	QToolButton *m_pHomeToolButton;
	QToolButton *m_pCdUpToolButton;
	QComboBox   *m_pRootPathComboBox;
	QTreeView   *m_pFileSystemTreeView;

	class FileSystemModel;

	FileSystemModel *m_pFileSystemModel;

	// Restore state current-path.
	QString m_sRestoreStatePath;
};


#endif	// __qtractorFileSystem_h

// end of qtractorFileSystem.h
