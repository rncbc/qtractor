// qtractorBusForm.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorBusForm_h
#define __qtractorBusForm_h

#include "ui_qtractorBusForm.h"

// Forward declarations...
class qtractorBus;


//----------------------------------------------------------------------------
// qtractorBusForm -- UI wrapper form.

class qtractorBusForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorBusForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);

	void setBus(qtractorBus *pBus);
	qtractorBus *bus();

	bool isDirty();

protected slots:

	void reject();
	void refreshBuses();
	void selectBus();
	void createBus();
	void updateBus();
	void deleteBus();
	void changed();
	void stabilizeForm();

	void contextMenu(const QPoint&);

	void midiSysex();

	void addInputPlugin();
	void removeInputPlugin();
	void moveUpInputPlugin();
	void moveDownInputPlugin();

	void addOutputPlugin();
	void removeOutputPlugin();
	void moveUpOutputPlugin();
	void moveDownOutputPlugin();

protected:

	enum { Create = 1, Update = 2, Delete = 4 };

	unsigned int flags() const;

	void showBus(qtractorBus *pBus);
	bool updateBus(qtractorBus *pBus);

	void updateMidiInstruments();
	void updateMidiSysex();

	void resetPluginLists();

private:

	// The Qt-designer UI struct...
	Ui::qtractorBusForm m_ui;

	// Instance variables...
	qtractorBus *m_pBus;
	QTreeWidgetItem *m_pAudioRoot;
	QTreeWidgetItem *m_pMidiRoot;
	int m_iDirtySetup;
	int m_iDirtyCount;
	int m_iDirtyTotal;
};

#endif	// __qtractorBusForm_h


// end of qtractorBusForm.h
