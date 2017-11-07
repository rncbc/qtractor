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

	// accessors.
	void setRootPath(const QString& sRootPath);
	QString rootPath() const;

	enum Flags { AllFiles = 1, AudioFiles = 2, MidiFiles = 4, HiddenFiles = 8 };

	void setFlags(Flags flags, bool on = true);
	Flags flags() const;

	// Audition/pre-listening player methods.
	void setPlayState(bool bOn);
	bool isPlayState() const;

	// state saver/loader.
	QByteArray saveState() const;
	bool restoreState(const QByteArray& state);

protected slots:

	// chdir slots.
	void cdUpClicked();
	void homeClicked();

	void rootPathActivated(const QString& sRootPath);
	void treeViewActivated(const QModelIndex& index);

	// filter slots.
	void filterChanged();

	// Audition/pre-listening player slots.
	void playFile(bool bOn);

signals:

	// File entry activated.
	void activated(const QString& sFilename);

protected:

	// model factory method.
	void updateRootPath(const QString& sRootPath);

	// model stabilizers.
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
	QAction     *m_pCdUpAction;
	QAction     *m_pHomeAction;
	QAction     *m_pAllFilesAction;
	QAction     *m_pAudioFilesAction;
	QAction     *m_pMidiFilesAction;
	QAction     *m_pHiddenFilesAction;
	QAction     *m_pPlayFileAction;

	// Member widgets...
	QToolButton *m_pCdUpToolButton;
	QToolButton *m_pHomeToolButton;
	QComboBox   *m_pRootPathComboBox;
	QTreeView   *m_pFileSystemTreeView;

	class FileSystemModel;

	FileSystemModel *m_pFileSystemModel;
};


#endif	// __qtractorFileSystem_h

// end of qtractorFileSystem.h
