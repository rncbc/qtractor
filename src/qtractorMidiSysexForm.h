// qtractorMidiSysexForm.h
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

#ifndef __qtractorMidiSysexForm_h
#define __qtractorMidiSysexForm_h

#include "ui_qtractorMidiSysexForm.h"


// Forward declarations...
class qtractorMidiSysexList;


//----------------------------------------------------------------------------
// qtractorMidiSysexForm -- UI wrapper form.

class qtractorMidiSysexForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiSysexForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorMidiSysexForm();

	// SysEx list accessors.
	void setSysexList(qtractorMidiSysexList *pSysexList);
	qtractorMidiSysexList *sysexList() const;

protected slots:

	void click(QAbstractButton *);

	void accept();
	void reject();

	void importSlot();
	void exportSlot();
	void moveUpSlot();
	void moveDownSlot();

	void nameChanged(const QString&);
	void textChanged();

	void openSlot();
	void loadSlot(const QString&);
	void saveSlot();
	void deleteSlot();

	void addSlot();
	void updateSlot();
	void removeSlot();
	void clearSlot();

	void refreshForm();
	void stabilizeForm();

protected:

	// SysEx file i/o methods.
	bool loadSysexItems(
		QList<QTreeWidgetItem *>& items, const QString& sFilename);
	bool saveSysexItems(
		const QList<QTreeWidgetItem *>& items, const QString& sFilename) const;

	void loadSysexFile(const QString& sFilename);
	void saveSysexFile(const QString& sFilename);

	// Refresh SysEx names (presets).
	void refreshSysex();

	// SysEx preset group path name.
	static QString sysexGroup();

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiSysexForm m_ui;

	// Main editable data structure.
	qtractorMidiSysexList *m_pSysexList;

	// Instance variables...
	int m_iDirtyCount;
	int m_iDirtyItem;
	int m_iDirtySysex;
	int m_iUpdateSysex;
};


#endif	// __qtractorMidiSysexForm_h


// end of qtractorMidiSysexForm.h
