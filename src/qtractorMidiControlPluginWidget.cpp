// qtractorMidiControlPluginWidget.cpp
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

#include "qtractorAbout.h"
#include "qtractorMidiControlPluginWidget.h"
#include "qtractorMidiControlPlugin.h"

#include "qtractorMidiControlTypeGroup.h"

#include "qtractorMainForm.h"


//----------------------------------------------------------------------------
// qtractorMidiControlPluginWidget -- UI wrapper form.


// Constructor.
qtractorMidiControlPluginWidget::qtractorMidiControlPluginWidget (
	QWidget *pParent ) : QWidget(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pControlTypeGroup = new qtractorMidiControlTypeGroup(nullptr,
		m_ui.ControlTypeComboBox, 
		m_ui.ControlParamComboBox,
		m_ui.ControlParamTextLabel);

	// Target object.
	m_pMidiControlPlugin = nullptr;

	m_iDirtySetup = 0;

	// UI signal/slot connections...
	QObject::connect(m_pControlTypeGroup,
		SIGNAL(controlTypeChanged(int)),
		SLOT(changed()));
	QObject::connect(m_pControlTypeGroup,
		SIGNAL(controlParamChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ControlChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ControlLogarithmicCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ControlInvertCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ControlBipolarCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changedBipolar()));
}


// Destructor.
qtractorMidiControlPluginWidget::~qtractorMidiControlPluginWidget (void)
{
	delete m_pControlTypeGroup;
}


// Accessors.
void qtractorMidiControlPluginWidget::setMidiControlPlugin (
	qtractorMidiControlPlugin *pMidiControlPlugin )
{
	m_pMidiControlPlugin = pMidiControlPlugin;

	if (m_pMidiControlPlugin) {
		++m_iDirtySetup;
		m_pControlTypeGroup->setControlType(
			m_pMidiControlPlugin->controlType());
		m_pControlTypeGroup->setControlParam(
			m_pMidiControlPlugin->controlParam());
		m_ui.ControlChannelSpinBox->setValue(
			m_pMidiControlPlugin->controlChannel() + 1);
		m_ui.ControlLogarithmicCheckBox->setChecked(
			m_pMidiControlPlugin->isControlLogarithmic());
		m_ui.ControlInvertCheckBox->setChecked(
			m_pMidiControlPlugin->isControlInvert());
		m_ui.ControlBipolarCheckBox->setChecked(
			m_pMidiControlPlugin->isControlBipolar());
		m_iDirtySetup = 0;
	}
}


qtractorMidiControlPlugin *qtractorMidiControlPluginWidget::midiControlPlugin (void) const
{
	return m_pMidiControlPlugin;
}


// Change settings (anything else slot).
void qtractorMidiControlPluginWidget::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPluginWidget::changed()");
#endif

	if (m_pMidiControlPlugin) {
		m_pMidiControlPlugin->setControlType(
			m_pControlTypeGroup->controlType());
		m_pMidiControlPlugin->setControlParam(
			m_pControlTypeGroup->controlParam());
		m_pMidiControlPlugin->setControlChannel(
			m_ui.ControlChannelSpinBox->value() - 1);
		m_pMidiControlPlugin->setControlLogarithmic(
			m_ui.ControlLogarithmicCheckBox->isChecked());
		m_pMidiControlPlugin->setControlInvert(
			m_ui.ControlInvertCheckBox->isChecked());
		dirtyNotify();
	}
}


void qtractorMidiControlPluginWidget::changedBipolar (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiControlPluginWidget::changedBipolar()");
#endif

	if (m_pMidiControlPlugin) {
		m_pMidiControlPlugin->setControlBipolar(
			m_ui.ControlBipolarCheckBox->isChecked());
		dirtyNotify();
	}

	emit bipolarChanged();
}


void qtractorMidiControlPluginWidget::dirtyNotify (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->dirtyNotifySlot();
}


// end of qtractorMidiControlPluginWidget.cpp
