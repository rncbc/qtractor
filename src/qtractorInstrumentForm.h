// qtractorInstrumentForm.h
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

#ifndef __qtractorInstrumentForm_h
#define __qtractorInstrumentForm_h

#include "ui_qtractorInstrumentForm.h"


// Forward declarations...
class qtractorInstrumentData;
class qtractorInstrumentDataList;
class qtractorInstrumentList;


//----------------------------------------------------------------------------
// qtractorInstrumentForm -- UI wrapper form.

class qtractorInstrumentForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorInstrumentForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorInstrumentForm();

	// Special group item types.
	enum { GroupItem = QTreeWidgetItem::UserType + 1 };

	// Instrument list accessors.
	void setInstruments(qtractorInstrumentList *pInstruments);
	qtractorInstrumentList *instruments() const;

protected slots:

    void accept();
    void reject();

    void importSlot();
    void removeSlot();
    void moveUpSlot();
    void moveDownSlot();
    void reloadSlot();
    void exportSlot();

    void stabilizeForm();
    void refreshForm();

	void itemCollapsed(QTreeWidgetItem*);
	void itemExpanded(QTreeWidgetItem*);

protected:

    void listInstrumentData(QTreeWidgetItem *pParentItem,
		const qtractorInstrumentData& data);
    void listInstrumentDataList(QTreeWidgetItem *pParentItem,
		const qtractorInstrumentDataList& list, const QIcon& icon);

    QString bankSelMethod(int iBankSelMethod);

private:

	// The Qt-designer UI struct...
	Ui::qtractorInstrumentForm m_ui;

	// Main editable data structure.
	qtractorInstrumentList *m_pInstruments;

	// Instance variables...
	int m_iDirtyCount;
};


#endif	// __qtractorInstrumentForm_h


// end of qtractorInstrumentForm.h
