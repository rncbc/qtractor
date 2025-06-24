// qtractorPluginSelectForm.h
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorPluginSelectForm_h
#define __qtractorPluginSelectForm_h

#include "ui_qtractorPluginSelectForm.h"

#include "qtractorPlugin.h"


//----------------------------------------------------------------------------
// qtractorPluginSelectForm -- UI wrapper form.

class qtractorPluginSelectForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginSelectForm(QWidget *pParent = nullptr);
	// Destructor.
	~qtractorPluginSelectForm();

	void setPluginList(qtractorPluginList *pPluginList);
	qtractorPluginList *pluginList() const;

	int pluginCount() const;
	QString pluginFilename(int iPlugin) const;
	unsigned long pluginIndex(int iPlugin) const;
	qtractorPluginType::Hint pluginTypeHint(int iPlugin) const;

protected slots:

	void typeHintChanged(int iTypeHint);

	void reset();
	void rescan();
	void refresh();
	void scanned(int iPercent);
	void stabilize();
	void accept();

private:

	// The Qt-designer UI struct...
	Ui::qtractorPluginSelectForm m_ui;

	qtractorPluginList *m_pPluginList;

	QList<QTreeWidgetItem *> m_selectedItems;
};


#endif	// __qtractorPluginSelectForm_h


// end of qtractorPluginSelectForm.h
