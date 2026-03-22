// qtractorMidiControlPluginWidget.h
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

#ifndef __qtractorMidiControlPluginWidget_h
#define __qtractorMidiControlPluginWidget_h

#include "ui_qtractorMidiControlPluginWidget.h"


// Forward declarartions.
class qtractorMidiControlTypeGroup;
class qtractorMidiControlPlugin;


//----------------------------------------------------------------------------
// qtractorMidiControlPluginWidget -- UI wrapper form.

class qtractorMidiControlPluginWidget : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiControlPluginWidget(QWidget *pParent = nullptr);
	// Destructor.
	~qtractorMidiControlPluginWidget();

	// Accessors.
	void setMidiControlPlugin(qtractorMidiControlPlugin *pMidiControlPlugin);
	qtractorMidiControlPlugin *midiControlPlugin() const;

	void dirtyNotify();

protected slots:

	void changed();
	void changedBipolar();

signals:

	void bipolarChanged();

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiControlPluginWidget m_ui;

	// Instance variables.
	qtractorMidiControlTypeGroup *m_pControlTypeGroup;

	qtractorMidiControlPlugin *m_pMidiControlPlugin;

	int m_iDirtySetup;
};


#endif	// __qtractorMidiControlPluginWidget_h


// end of qtractorMidiControlPluginWidget.h
