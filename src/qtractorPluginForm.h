// qtractorPluginForm.h
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

#ifndef __qtractorPluginForm_h
#define __qtractorPluginForm_h

#include "ui_qtractorPluginForm.h"

#include <QFrame>
#include <QHash>


// Forward declarations...
class qtractorPlugin;
class qtractorPluginParam;
class qtractorPluginParamWidget;

class qtractorMidiControlObserver;

class qtractorObserverCheckBox;
class qtractorObserverSlider;
class qtractorObserverSpinBox;

class qtractorPluginParamDisplay;


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

	void updateActivated();

	void changeParamValue(unsigned long iIndex);

	void updateAudioBusName();

	void activateForm();

	void refresh();
	void clear();

	void toggleEditor(bool bOn);

protected slots:

	void changePresetSlot(const QString& sPreset);
	void loadPresetSlot(const QString& sPreset);
	void openPresetSlot();
	void savePresetSlot();
	void deletePresetSlot();
	void editSlot(bool bOn);
	void sendsSlot();
	void returnsSlot();
	void activateSlot(bool bOn);

	void midiControlActionSlot();
	void midiControlMenuSlot(const QPoint& pos);

	void changeAudioBusNameSlot(const QString& sAudioBusName);
	void clickAudioBusNameSlot();

	void updateDirectAccessParamSlot();
	void changeDirectAccessParamSlot();

protected:

	void stabilize();

	// Show insert pseudo-plugin audio bus connections.
	void insertPluginBus(int iBusMode);

	// MIDI controller/observer attachement (context menu)
	void addMidiControlAction(
		QWidget *pWidget, qtractorMidiControlObserver *pObserver);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *);

private:

	// The Qt-designer UI struct...
	Ui::qtractorPluginForm m_ui;

	// Instance variables...
	qtractorPlugin *m_pPlugin;

	typedef QHash<unsigned long, qtractorPluginParamWidget *> ParamWidgets;
	ParamWidgets m_paramWidgets;

	QMenu *m_pDirectAccessParamMenu;

	int m_iDirtyCount;
	int m_iUpdate;
};


//----------------------------------------------------------------------------
// qtractorPluginParamWidget -- Plugin port widget.
//

class qtractorPluginParamWidget : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginParamWidget(qtractorPluginParam *pParam,
		QWidget *pParent = NULL);

	// Main properties accessors.
	qtractorPluginParam *param() const
		{ return m_pParam; }

	// Refreshner-loader method.
	void refresh();

protected slots:

	// Parameter value change slot.
	void updateValue(float fValue);

private:

	// Local forward declarations.
	class SliderInterface;

	// Instance variables.
	qtractorPluginParam *m_pParam;

	// Some possible managed widgets.
	qtractorObserverCheckBox *m_pCheckBox;
	qtractorObserverSlider   *m_pSlider;
	qtractorObserverSpinBox  *m_pSpinBox;

	qtractorPluginParamDisplay *m_pDisplay;
};


#endif	// __qtractorPluginForm_h


// end of qtractorPluginForm.h
