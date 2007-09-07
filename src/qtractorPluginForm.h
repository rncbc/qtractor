// qtractorPluginForm.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorPluginForm_h
#define __qtractorPluginForm_h

#include "ui_qtractorPluginForm.h"

#include <QList>


// Forward declarations...
class qtractorPlugin;
class qtractorPluginPort;
class qtractorPluginPortWidget;


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.

class qtractorPluginForm : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorPluginForm();

    void setPlugin(qtractorPlugin *pPlugin);
    qtractorPlugin *plugin() const;

	void setPreset(const QString& sPreset);
	QString preset() const;

	void updateCaption();
    void updateActivated();
	void updatePort(qtractorPluginPort *pPort);

    void refresh();

public slots:

    void changePresetSlot(const QString& sPreset);
    void loadPresetSlot(const QString& sPreset);
    void savePresetSlot();
    void deletePresetSlot();
    void activateSlot(bool bOn);
	void valueChangeSlot(qtractorPluginPort *pPort, float fValue);

protected:

    void stabilize();

private:

	// The Qt-designer UI struct...
	Ui::qtractorPluginForm m_ui;

	// Instance variables...
	qtractorPlugin *m_pPlugin;
	QGridLayout *m_pGridLayout;
	QList<qtractorPluginPortWidget *> m_portWidgets;
	int m_iDirtyCount;
	int m_iUpdate;
};


#endif	// __qtractorPluginForm_h


// end of qtractorPluginForm.h
