// qtractorMidiControlObserverForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiControlObserverForm.h"
#include "qtractorMidiControlObserver.h"
#include "qtractorMidiControlCommand.h"

#include "qtractorMidiEditor.h"

#include "qtractorSession.h"

#include "qtractorMainForm.h"
#include "qtractorMidiEngine.h"
#include "qtractorConnections.h"

#include <QMessageBox>
#include <QPushButton>


//----------------------------------------------------------------------------
// qtractorMidiControlObserverForm -- UI wrapper form.

// Kind of singleton reference.
qtractorMidiControlObserverForm *
qtractorMidiControlObserverForm::g_pMidiObserverForm = NULL;


// Constructor.
qtractorMidiControlObserverForm::qtractorMidiControlObserverForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Make it auto-modeless dialog...
	QDialog::setAttribute(Qt::WA_DeleteOnClose);

	// Populate command list.
	const QIcon iconControlType(":/images/itemProperty.png");
//	m_ui.ControlTypeComboBox->clear();
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEON));
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEOFF));
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::KEYPRESS));
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CONTROLLER));
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::PGMCHANGE));
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CHANPRESS));
	m_ui.ControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::PITCHBEND));

	// Aadd a special Inputs button...
	QPushButton *pInputsButton
		= new QPushButton(QIcon(":/images/itemMidiPortIn.png"), tr("Inputs"));
	m_ui.DialogButtonBox->addButton(pInputsButton, QDialogButtonBox::ActionRole);

	// Start clean.
	m_iDirtyCount = 0;
	m_iDirtySetup = 0;

	// Populate param list.
	// activateControlType(m_ui.ControlTypeComboBox->currentText());

	// Try to fix window geometry.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.ControlTypeComboBox,
		SIGNAL(activated(const QString&)),
		SLOT(activateControlType(const QString&)));
	QObject::connect(m_ui.ChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(change()));
	QObject::connect(m_ui.ParamComboBox,
		SIGNAL(activated(int)),
		SLOT(change()));
	QObject::connect(m_ui.LogarithmicCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(change()));
	QObject::connect(m_ui.FeedbackCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(change()));
	QObject::connect(m_ui.InvertCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(change()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(clicked(QAbstractButton *)),
		SLOT(click(QAbstractButton *)));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));

	QObject::connect(pInputsButton,
		SIGNAL(clicked()),
		SLOT(inputs()));

	// Pseudo-singleton reference setup.
	g_pMidiObserverForm = this;
}


// Pseudo-singleton instance.
qtractorMidiControlObserverForm *
qtractorMidiControlObserverForm::getInstance (void)
{
	return g_pMidiObserverForm;
}


// Pseudo-constructor.
void qtractorMidiControlObserverForm::showInstance (
	qtractorMidiControlObserver *pMidiObserver,
	QWidget *pParent, Qt::WindowFlags wflags )
{
	qtractorMidiControlObserverForm *pMidiObserverForm
		= qtractorMidiControlObserverForm::getInstance();
	if (pMidiObserverForm)
		pMidiObserverForm->close();

	pMidiObserverForm = new qtractorMidiControlObserverForm(pParent, wflags);
	pMidiObserverForm->setMidiObserver(pMidiObserver);
	pMidiObserverForm->show();
}


// Observer accessors.
void qtractorMidiControlObserverForm::setMidiObserver (
	qtractorMidiControlObserver *pMidiObserver )
{
	m_pMidiObserver = pMidiObserver;

	if (m_pMidiObserver == NULL)
		return;
	if (m_pMidiObserver->subject() == NULL)
		return;

	m_iDirtySetup++;

	QDialog::setWindowTitle(
		m_pMidiObserver->subject()->name() + " - " + tr("MIDI Controller"));

	const QString& sControlType
		= qtractorMidiControl::nameFromType(m_pMidiObserver->type());
	m_ui.ControlTypeComboBox->setCurrentIndex(
		m_ui.ControlTypeComboBox->findText(sControlType));
	activateControlType(sControlType);
	m_ui.ChannelSpinBox->setValue(m_pMidiObserver->channel() + 1);
	m_ui.ParamComboBox->setCurrentIndex(m_pMidiObserver->param());

	m_ui.LogarithmicCheckBox->setChecked(m_pMidiObserver->isLogarithmic());
	m_ui.FeedbackCheckBox->setChecked(m_pMidiObserver->isFeedback());
	m_ui.InvertCheckBox->setChecked(m_pMidiObserver->isInvert());

	qtractorMidiControl *pMidiControl
		= qtractorMidiControl::getInstance();
	QPushButton *pResetButton
		= m_ui.DialogButtonBox->button(QDialogButtonBox::Reset);
	if (pResetButton && pMidiControl)
		pResetButton->setEnabled(
			pMidiControl->isMidiObserverMapped(m_pMidiObserver));

	m_iDirtySetup--;
	m_iDirtyCount = 0;
}

qtractorMidiControlObserver *qtractorMidiControlObserverForm::midiObserver (void) const
{
	return m_pMidiObserver;
}


// Pseudo-destructor.
void qtractorMidiControlObserverForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	// Pseudo-singleton reference setup.
	g_pMidiObserverForm = NULL;

	// Sure acceptance and probable destruction (cf. WA_DeleteOnClose).
	QDialog::closeEvent(pCloseEvent);
}


// Process incoming controller event.
void qtractorMidiControlObserverForm::processEvent ( const qtractorCtlEvent& ctle )
{
	const QString& sControlType
		= qtractorMidiControl::nameFromType(ctle.type());
	m_ui.ControlTypeComboBox->setCurrentIndex(
		m_ui.ControlTypeComboBox->findText(sControlType));
	activateControlType(sControlType);
	m_ui.ChannelSpinBox->setValue(ctle.channel() + 1);
	m_ui.ParamComboBox->setCurrentIndex(ctle.param());
}


// List view item activation.
void qtractorMidiControlObserverForm::activateControlType (
	const QString& sControlType )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::activateControlType(\"%s\")",
		sControlType.toUtf8().constData());
#endif

	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(sControlType);
	if (!ctype)
		return;

	const QIcon icon(":/images/itemControllers.png");
	m_ui.ParamComboBox->clear();
	switch (ctype) {
	case qtractorMidiEvent::CHANPRESS:
	case qtractorMidiEvent::PITCHBEND:
		m_ui.ParamTextLabel->setEnabled(false);
		m_ui.ParamComboBox->setEnabled(false);
		break;
	case qtractorMidiEvent::NOTEON:
	case qtractorMidiEvent::NOTEOFF:
	case qtractorMidiEvent::CONTROLLER:
	case qtractorMidiEvent::KEYPRESS:
	case qtractorMidiEvent::PGMCHANGE:
		m_ui.ParamTextLabel->setEnabled(true);
		m_ui.ParamComboBox->setEnabled(true);
		for (unsigned short i = 0; i < 128; ++i) {
			QString sText;
			switch (ctype) {
			case qtractorMidiEvent::NOTEON:
			case qtractorMidiEvent::NOTEOFF:
			case qtractorMidiEvent::KEYPRESS:
				sText = QString("%1 (%2)")
					.arg(qtractorMidiEditor::defaultNoteName(i)).arg(i);
				break;
			case qtractorMidiEvent::CONTROLLER:
				sText = QString("%1 - %2")
					.arg(i).arg(qtractorMidiEditor::defaultControllerName(i));
				break;
			default:
				sText = QString::number(i);
				break;
			}
			m_ui.ParamComboBox->addItem(icon, sText);
		}
		// Fall thru...
	default:
		break;
	}

	// This is enabled by as long there's a value.
	m_ui.LogarithmicCheckBox->setEnabled(
		ctype != qtractorMidiEvent::PGMCHANGE);

	// Try make changes dirty.
	change();
}


// Change settings (anything else slot).
void qtractorMidiControlObserverForm::change (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::change()");
#endif

	m_iDirtyCount++;
//	stabilizeForm();
}


// Reset settings (action button slot).
void qtractorMidiControlObserverForm::click ( QAbstractButton *pButton )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::click(%p)", pButton);
#endif

	QDialogButtonBox::ButtonRole role
		= m_ui.DialogButtonBox->buttonRole(pButton);
	if ((role & QDialogButtonBox::ResetRole) == QDialogButtonBox::ResetRole)
		reset();
}


// Accept settings (OK button slot).
void qtractorMidiControlObserverForm::accept (void)
{
	qtractorMidiControl *pMidiControl
		= qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::accept()");
#endif

	// Get map settings...
	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(
			m_ui.ControlTypeComboBox->currentText());
	unsigned short iChannel = m_ui.ChannelSpinBox->value();
	if (iChannel > 0)
		iChannel--;
	unsigned short iParam = 0;
	if (m_ui.ParamComboBox->isEnabled())
		iParam = m_ui.ParamComboBox->currentIndex();
	bool bLogarithmic = false;
	if (m_ui.LogarithmicCheckBox->isEnabled())
		bLogarithmic = m_ui.LogarithmicCheckBox->isChecked();
	bool bFeedback = false;
	if (m_ui.FeedbackCheckBox->isEnabled())
		bFeedback = m_ui.FeedbackCheckBox->isChecked();
	bool bInvert = false;
	if (m_ui.InvertCheckBox->isEnabled())
		bInvert = m_ui.InvertCheckBox->isChecked();

	// Check whether already mapped...
	qtractorMidiControlObserver *pMidiObserver
		= pMidiControl->findMidiObserver(ctype, iChannel, iParam);
	if (pMidiObserver && pMidiObserver != m_pMidiObserver) {
		if (QMessageBox::warning(this,
			QDialog::windowTitle(),
			tr("MIDI controller is already assigned.\n\n"
			"Do you want to replace the mapping?"),
			QMessageBox::Ok |
			QMessageBox::Cancel) == QMessageBox::Cancel)
			return;
	}

	// Map the damn control....
	pMidiControl->unmapMidiObserver(m_pMidiObserver);
	m_pMidiObserver->setType(ctype);
	m_pMidiObserver->setChannel(iChannel);
	m_pMidiObserver->setParam(iParam);
	m_pMidiObserver->setLogarithmic(bLogarithmic);
	m_pMidiObserver->setFeedback(bFeedback);
	m_pMidiObserver->setInvert(bInvert);
#if 0
	pMidiControl->mapMidiObserver(m_pMidiObserver);
#else
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(new qtractorMidiControlObserverMapCommand(m_pMidiObserver));
#endif

	// Aint't dirty no more...
	m_iDirtyCount = 0;

	// Just go with dialog acceptance...
	QDialog::accept();
	QDialog::close();
}


// Reject settings (Cancel button slot).
void qtractorMidiControlObserverForm::reject (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::reject()");
#endif

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			QDialog::windowTitle(),
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			QMessageBox::Apply |
			QMessageBox::Discard |
			QMessageBox::Cancel)) {
		case QMessageBox::Discard:
			break;
		case QMessageBox::Apply:
			accept();
		default:
			return;
		}
	}

	// Just go with dialog rejection...
	QDialog::reject();
	QDialog::close();
}


// Reset settings (Reset button slot).
void qtractorMidiControlObserverForm::reset (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::reset()");
#endif

#if 0
	qtractorMidiControl *pMidiControl
		= qtractorMidiControl::getInstance();
	if (pMidiControl)
		pMidiControl->unmapMidiObserver(m_pMidiObserver);
#else
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(new qtractorMidiControlObserverUnmapCommand(m_pMidiObserver));
#endif

	// Bail out...
	QDialog::accept();
	QDialog::close();
}


// Show control inputs (Custom button slot).
void qtractorMidiControlObserverForm::inputs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::inputs()");
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->connections()) {
		(pMainForm->connections())->showBus(
			pMidiEngine->controlBus_in(), qtractorBus::Input);
	}
}


// end of qtractorMidiControlObserverForm.cpp
