// qtractorMidiEventListView.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiEventListView_h
#define __qtractorMidiEventListView_h

#include <QTreeView>

// Forwards.
class qtractorMidiEditor;
class qtractorMidiEventItemDelegate;
class qtractorMidiEventListModel;
class qtractorMidiEvent;


//----------------------------------------------------------------------------
// qtractorMidiEventListView -- Custom (tree) list view.

class qtractorMidiEventListView : public QTreeView
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiEventListView(QWidget *pParent = 0);

	// Destructor.
	~qtractorMidiEventListView();

	// Settlers.
	void setEditor(qtractorMidiEditor *pEditor);
	qtractorMidiEditor *editor() const;

	// Refreshener.
	void refresh();

	// Locators.
	qtractorMidiEvent *eventOfIndex(const QModelIndex& index) const;
	QModelIndex indexOfEvent(qtractorMidiEvent *pEvent) const;

	QModelIndex indexFromTick(unsigned long iTick) const;
	unsigned long tickFromIndex(const QModelIndex& index) const;

	QModelIndex indexFromFrame(unsigned long iFrame) const;
	unsigned long frameFromIndex(const QModelIndex& index) const;

	// Selections.
	void selectEvent(qtractorMidiEvent *pEvent, bool bSelect = true);

private:

	// Instance variables.
	qtractorMidiEventListModel    *m_pListModel;
	qtractorMidiEventItemDelegate *m_pItemDelegate;
};


#endif  // __qtractorMidiEventListView_h


// end of qtractorMidiEventListView.h
