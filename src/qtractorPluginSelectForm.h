// qtractorPluginSelectForm.h
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
	qtractorPluginSelectForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorPluginSelectForm();

	void setChannels(unsigned short iChannels, bool bMidi = false);
	unsigned short channels() const;
	bool isMidi() const;

	int pluginCount() const;
	QString pluginFilename(int iPlugin) const;
	unsigned long pluginIndex(int iPlugin) const;
	qtractorPluginType::Hint pluginTypeHint(int iPlugin) const;
	bool isPluginActivated() const;

protected slots:

	void typeHintChanged(int iTypeHint);

	void reset();
	void refresh();
	void stabilize();
	void accept();

private:

	// The Qt-designer UI struct...
	Ui::qtractorPluginSelectForm m_ui;

	unsigned short m_iChannels;
	bool m_bMidi;

	QList<QTreeWidgetItem *> m_selectedItems;
};


#endif	// __qtractorPluginSelectForm_h


// end of qtractorPluginSelectForm.h
