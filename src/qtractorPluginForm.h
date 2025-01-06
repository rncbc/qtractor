// qtractorPluginForm.h
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

#ifndef __qtractorPluginForm_h
#define __qtractorPluginForm_h

#include "ui_qtractorPluginForm.h"

#include "qtractorPlugin.h"

#include <QHash>


// Forward declarations...
class qtractorPluginParamWidget;
class qtractorPluginPropertyWidget;

class qtractorMidiControlObserver;

class qtractorObserverCheckBox;
class qtractorObserverSlider;
class qtractorObserverSpinBox;

class qtractorPluginParamDisplay;

class qtractorSpinBox;

class QCheckBox;
class QTextEdit;
class QComboBox;
class QPushButton;
class QToolButton;


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.

class qtractorPluginForm : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginForm(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags());
	// Destructor.
	~qtractorPluginForm();

	void setPlugin(qtractorPlugin *pPlugin);
	qtractorPlugin *plugin() const;

	void setPreset(const QString& sPreset);
	QString preset() const;

	void updateActivated();
	void updateDirtyCount();

	void updateAuxSendBusName();

	void toggleEditor(bool bOn);

	void refresh();
	void clear();

protected slots:

	void changePresetSlot(const QString& sPreset);
	void loadPresetSlot(int iPreset);
	void openPresetSlot();
	void savePresetSlot();
	void deletePresetSlot();

	void aliasSlot();

	void editSlot(bool bOn);
	void activateSlot(bool bOn);

	void currentChangedSlot(int iTab);

	void sendsSlot();
	void returnsSlot();

	void midiControlActionSlot();
	void midiControlMenuSlot(const QPoint& pos);

	void changeAuxSendBusNameSlot(int iAuxSendBusName);
	void clickAuxSendBusNameSlot();

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

	// Form show/hide events (restore/save position).
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

	// Update the about text label (with some varying meta-data)...
	void updateLatencyTextLabel();

private:

	// The Qt-designer UI struct...
	Ui::qtractorPluginForm m_ui;

	// Instance variables...
	qtractorPlugin *m_pPlugin;

	QList<qtractorPluginParamWidget *> m_paramWidgets;

	QMenu *m_pDirectAccessParamMenu;

	int m_iDirtyCount;
	int m_iUpdate;

	// Common (de)activated icon stuff.
	static QIcon *g_pIcons[2];
	static int    g_iIconsRefCount;
};


//----------------------------------------------------------------------------
// qtractorPluginParamWidget -- Plugin parameter/property common widget.
//

class qtractorPluginParamWidget : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginParamWidget(qtractorPlugin::Param *pParam,
		QWidget *pParent = nullptr);

	// Destructor.
	~qtractorPluginParamWidget();

	// Refreshner-loader method.
	void refresh();

protected slots:

	// Parameter value change slot.
	void updateValue(float fValue);

	// Property value change slot.
	void propertyChanged();

	// Property file selector.
	void toolButtonClicked();

	// Automation curve selector.
	void curveButtonClicked();

protected:

	// Param/Property discriminator..
	qtractorPlugin::Property *property () const;

	// Parameter automation curve status update/refresh.
	void updateCurveButton();

	// Text edit (string) event filter.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

private:

	// Local forward declarations.
	class SliderInterface;

	// Instance variables.
	qtractorPlugin::Param *m_pParam;

	// Some possible managed widgets.
	qtractorObserverCheckBox *m_pCheckBox;
	qtractorObserverSlider   *m_pSlider;
	qtractorObserverSpinBox  *m_pSpinBox;

	qtractorPluginParamDisplay *m_pDisplay;

	QPushButton *m_pCurveButton;

	// Non-automatable widgets.
	QTextEdit   *m_pTextEdit;
	QComboBox   *m_pComboBox;
	QToolButton *m_pToolButton;
};


#endif	// __qtractorPluginForm_h


// end of qtractorPluginForm.h
