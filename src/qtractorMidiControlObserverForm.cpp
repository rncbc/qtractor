// qtractorMidiControlObserverForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiControlTypeGroup.h"

#include "qtractorActionControl.h"

#include "qtractorSession.h"

#include "qtractorMainForm.h"
#include "qtractorMidiEngine.h"
#include "qtractorConnections.h"

#include "qtractorCurve.h"
#include "qtractorTracks.h"
#include "qtractorTrackList.h"

#include "qtractorCurveCommand.h"

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

	m_pControlTypeGroup = new qtractorMidiControlTypeGroup(NULL,
		m_ui.ControlTypeComboBox, m_ui.ParamComboBox, m_ui.ParamTextLabel);

	// Target object.
	m_pMidiObserver = NULL;

	// Proxy object.
	m_pMidiObserverAction = NULL;

	// Start clean.
	m_iDirtyCount = 0;
	m_iDirtySetup = 0;

	// Populate param list.
	// activateControlType(m_ui.ControlTypeComboBox->currentIndex());

	// Try to fix window geometry.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_pControlTypeGroup,
		SIGNAL(controlTypeChanged(int)),
		SLOT(change()));
	QObject::connect(m_pControlTypeGroup,
		SIGNAL(controlParamChanged(int)),
		SLOT(change()));
	QObject::connect(m_ui.ChannelSpinBox,
		SIGNAL(valueChanged(int)),
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
	QObject::connect(m_ui.HookCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(change()));
	QObject::connect(m_ui.LatchCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(change()));
	QObject::connect(m_ui.InputsPushButton,
		SIGNAL(clicked()),
		SLOT(inputs()));
	QObject::connect(m_ui.OutputsPushButton,
		SIGNAL(clicked()),
		SLOT(outputs()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(clicked(QAbstractButton *)),
		SLOT(click(QAbstractButton *)));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));

	// Pseudo-singleton reference setup.
	g_pMidiObserverForm = this;
}


// Destructor.
qtractorMidiControlObserverForm::~qtractorMidiControlObserverForm (void)
{
	delete m_pControlTypeGroup;
}


// Pseudo-singleton instance.
qtractorMidiControlObserverForm *
qtractorMidiControlObserverForm::getInstance (void)
{
	return g_pMidiObserverForm;
}


// Pseudo-constructors.
void qtractorMidiControlObserverForm::showInstance (
	qtractorMidiControlObserver *pMidiObserver,
	QWidget *pParent, Qt::WindowFlags wflags )
{
	qtractorMidiControlObserverForm *pMidiObserverForm
		= qtractorMidiControlObserverForm::getInstance();
	if (pMidiObserverForm)
		pMidiObserverForm->close();

	if (pMidiObserver == NULL)
		return;
	if (pMidiObserver->subject() == NULL)
		return;

	pMidiObserverForm = new qtractorMidiControlObserverForm(pParent, wflags);
	pMidiObserverForm->setMidiObserver(pMidiObserver);
	pMidiObserverForm->show();
}


void qtractorMidiControlObserverForm::showInstance (
	QAction *pMidiObserverAction, QWidget *pParent, Qt::WindowFlags wflags )
{
	qtractorMidiControlObserverForm *pMidiObserverForm
		= qtractorMidiControlObserverForm::getInstance();
	if (pMidiObserverForm)
		pMidiObserverForm->close();

	pMidiObserverForm = new qtractorMidiControlObserverForm(pParent, wflags);
	pMidiObserverForm->setMidiObserverAction(pMidiObserverAction);
	pMidiObserverForm->show();
}


// Observer accessors.
void qtractorMidiControlObserverForm::setMidiObserver (
	qtractorMidiControlObserver *pMidiObserver )
{
	++m_iDirtySetup;

	m_pMidiObserver = pMidiObserver;

	QDialog::setWindowTitle(
		m_pMidiObserver->subject()->name() + " - " + tr("MIDI Controller"));

	m_pControlTypeGroup->setControlType(m_pMidiObserver->type());
	m_pControlTypeGroup->setControlParam(m_pMidiObserver->param());

	m_ui.ChannelSpinBox->setValue(m_pMidiObserver->channel() + 1);

	const bool bDecimal = m_pMidiObserver->isDecimal();
	const bool bToggled = m_pMidiObserver->isToggled();

	m_ui.LogarithmicCheckBox->setChecked(m_pMidiObserver->isLogarithmic());
	m_ui.LogarithmicCheckBox->setEnabled(bDecimal);

	m_ui.FeedbackCheckBox->setChecked(m_pMidiObserver->isFeedback());
	m_ui.FeedbackCheckBox->setEnabled(true);

	m_ui.InvertCheckBox->setChecked(m_pMidiObserver->isInvert());
	m_ui.InvertCheckBox->setEnabled(true);

	m_ui.HookCheckBox->setChecked(m_pMidiObserver->isHook());
	m_ui.HookCheckBox->setEnabled(bDecimal);

	m_ui.LatchCheckBox->setChecked(m_pMidiObserver->isLatch());
	m_ui.LatchCheckBox->setEnabled(bToggled);

	qtractorMidiControl *pMidiControl
		= qtractorMidiControl::getInstance();
	QPushButton *pResetButton
		= m_ui.DialogButtonBox->button(QDialogButtonBox::Reset);
	if (pResetButton && pMidiControl)
		pResetButton->setEnabled(
			pMidiControl->isMidiObserverMapped(m_pMidiObserver));

	--m_iDirtySetup;
	m_iDirtyCount = 0;

	stabilizeForm();
}

qtractorMidiControlObserver *qtractorMidiControlObserverForm::midiObserver (void) const
{
	return m_pMidiObserver;
}


// Action (control) observer accessors.
void qtractorMidiControlObserverForm::setMidiObserverAction (
	QAction *pMidiObserverAction )
{
	qtractorActionControl *pActionControl
		= qtractorActionControl::getInstance();
	if (pActionControl == NULL)
		return;

	qtractorActionControl::MidiObserver *pMidiObserver
		= pActionControl->addMidiObserver(pMidiObserverAction);
	if (pMidiObserver) {
		m_pMidiObserverAction = pMidiObserverAction;
		setMidiObserver(pMidiObserver);
	}
}

QAction *qtractorMidiControlObserverForm::midiObserverAction (void) const
{
	return m_pMidiObserverAction;
}


// Pseudo-destructor.
void qtractorMidiControlObserverForm::closeEvent ( QCloseEvent *pCloseEvent )
{
	// Cleanup.
	m_pMidiObserverAction = NULL;
	m_pMidiObserver = NULL;

	// Pseudo-singleton reference setup.
	g_pMidiObserverForm = NULL;

	// Sure acceptance and probable destruction (cf. WA_DeleteOnClose).
	QDialog::closeEvent(pCloseEvent);
}


// Process incoming controller event.
void qtractorMidiControlObserverForm::processEvent ( const qtractorCtlEvent& ctle )
{
	m_pControlTypeGroup->setControlType(ctle.type());
	m_pControlTypeGroup->setControlParam(ctle.param());

	m_ui.ChannelSpinBox->setValue(ctle.channel() + 1);
}


// Change settings (anything else slot).
void qtractorMidiControlObserverForm::change (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::change()");
#endif

	++m_iDirtyCount;
	stabilizeForm();
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
	const qtractorMidiControl::ControlType ctype
		= m_pControlTypeGroup->controlType();

	unsigned short iChannel = m_ui.ChannelSpinBox->value();
	if (iChannel > 0)
		--iChannel;

	unsigned short iParam = 0;
	if (m_ui.ParamComboBox->isEnabled())
		iParam = m_pControlTypeGroup->controlParam();

	bool bLogarithmic = m_pMidiObserver->isLogarithmic();
	if (m_ui.LogarithmicCheckBox->isEnabled())
		bLogarithmic = m_ui.LogarithmicCheckBox->isChecked();
	bool bFeedback = m_pMidiObserver->isFeedback();
	if (m_ui.FeedbackCheckBox->isEnabled())
		bFeedback = m_ui.FeedbackCheckBox->isChecked();
	bool bInvert = m_pMidiObserver->isInvert();
	if (m_ui.InvertCheckBox->isEnabled())
		bInvert = m_ui.InvertCheckBox->isChecked();
	bool bHook = m_pMidiObserver->isHook();
	if (m_ui.HookCheckBox->isEnabled())
		bHook = m_ui.HookCheckBox->isChecked();
	bool bLatch = m_pMidiObserver->isLatch();
	if (m_ui.LatchCheckBox->isEnabled())
		bLatch = m_ui.LatchCheckBox->isChecked();

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
	m_pMidiObserver->setHook(bHook);
	m_pMidiObserver->setLatch(bLatch);

	if (m_pMidiObserverAction) {
		pMidiControl->mapMidiObserver(m_pMidiObserver);
	} else {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) pSession->execute(
			new qtractorMidiControlObserverMapCommand(m_pMidiObserver));
	}

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

	// Remove any (QAction) MIDI observer mapping...
	if (m_pMidiObserverAction) {
		qtractorMidiControl *pMidiControl
			= qtractorMidiControl::getInstance();
		if (pMidiControl
			&& !pMidiControl->isMidiObserverMapped(m_pMidiObserver)) {
			qtractorActionControl *pActionControl
				= qtractorActionControl::getInstance();
			if (pActionControl)
				pActionControl->removeMidiObserver(m_pMidiObserverAction);
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

	if (m_pMidiObserverAction) {
		qtractorMidiControl *pMidiControl
			= qtractorMidiControl::getInstance();
		qtractorActionControl *pActionControl
			= qtractorActionControl::getInstance();
		if (pMidiControl && pActionControl) {
			pMidiControl->unmapMidiObserver(m_pMidiObserver);
			pActionControl->removeMidiObserver(m_pMidiObserverAction);
		}
	} else {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) pSession->execute(
			new qtractorMidiControlObserverUnmapCommand(m_pMidiObserver));
	}

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
	if (pMainForm && pMainForm->connections() && pMidiEngine->controlBus_in()) {
		(pMainForm->connections())->showBus(
			pMidiEngine->controlBus_in(), qtractorBus::Input);
	}
}


// Show control outputs (Custom button slot).
void qtractorMidiControlObserverForm::outputs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiControlObserverForm::outputs()");
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
	if (pMidiEngine == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->connections() && pMidiEngine->controlBus_out()) {
		(pMainForm->connections())->showBus(
			pMidiEngine->controlBus_out(), qtractorBus::Output);
	}
}


// Update control widget state.
void qtractorMidiControlObserverForm::stabilizeForm (void)
{
	qtractorMidiEngine *pMidiEngine = NULL;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pMidiEngine = pSession->midiEngine();
	if (pMidiEngine) {
		const bool bFeedback = m_ui.FeedbackCheckBox->isChecked();
		m_ui.InputsPushButton->setEnabled(
			pMidiEngine->controlBus_in() != NULL);
		m_ui.OutputsPushButton->setEnabled(
			bFeedback && pMidiEngine->controlBus_out() != NULL);
	} else {
		m_ui.InputsPushButton->setEnabled(false);
		m_ui.OutputsPushButton->setEnabled(false);
	}
}


// MIDI controller/observer attachment (context menu) activator.
//
Q_DECLARE_METATYPE(qtractorMidiControlObserver *);

QAction *qtractorMidiControlObserverForm::addMidiControlAction (
	QWidget *pParent, QWidget *pWidget, qtractorMidiControlObserver *pMidiObserver )
{
	QAction *pAction = new QAction(
		QIcon(":/images/itemControllers.png"),
		tr("&MIDI Controller..."), pWidget);

	pAction->setData(
		qVariantFromValue<qtractorMidiControlObserver *> (pMidiObserver));

	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		pParent, SLOT(midiControlActionSlot()));

	pWidget->addAction(pAction);

	pWidget->setContextMenuPolicy(Qt::CustomContextMenu);
	QObject::connect(
		pWidget, SIGNAL(customContextMenuRequested(const QPoint&)),
		pParent, SLOT(midiControlMenuSlot(const QPoint&)));

	return pAction;
}


void qtractorMidiControlObserverForm::midiControlAction (
	QWidget *pParent, QAction *pAction )
{
	if (pAction == NULL)
		return;

	qtractorMidiControlObserver *pMidiObserver
		= pAction->data().value<qtractorMidiControlObserver *> ();
	if (pMidiObserver)
		qtractorMidiControlObserverForm::showInstance(pMidiObserver, pParent);
}


void qtractorMidiControlObserverForm::midiControlMenu (
	QWidget *pWidget, const QPoint& pos )
{
	if (pWidget == NULL)
		return;

	QAction *pMidiControlAction = NULL;
	qtractorMidiControlObserver *pMidiObserver = NULL;
	QListIterator<QAction *> iter(pWidget->actions());
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		pMidiObserver
			= pAction->data().value<qtractorMidiControlObserver *> ();
		if (pMidiObserver) {
			pMidiControlAction = pAction;
			break;
		}
	}

	if (pMidiControlAction == NULL)
		return;
	if (pMidiObserver == NULL)
		return;

	QMenu menu(pWidget);
	menu.addAction(pMidiControlAction);

	qtractorMidiControlObserverForm::addMidiControlMenu(&menu, pMidiObserver);

	menu.exec(pWidget->mapToGlobal(pos));
}


// Add esquisite automation menu actions...
void qtractorMidiControlObserverForm::addMidiControlMenu (
	QMenu *pMenu, qtractorMidiControlObserver *pMidiObserver )
{
	qtractorCurveList *pCurveList = pMidiObserver->curveList();
	if (pCurveList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTrack *pTrack = pSession->findTrack(pCurveList);
	if (pTrack == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorTracks *pTracks = pMainForm->tracks();
	if (pTracks == NULL)
		return;

	qtractorCurve *pCurve = NULL;
	qtractorSubject *pSubject = pMidiObserver->subject();
	if (pSubject)
		pCurve = pSubject->curve();

	if (pCurve && pCurveList->currentCurve() != pCurve) {
		pCurveList->setCurrentCurve(pCurve);
		pTracks->updateTrackView();
	}

	pTracks->trackList()->setCurrentTrack(pTrack);

	QAction *pAction;

	pMenu->addSeparator();

	pAction = pMenu->addAction(tr("&Automation"));
	pAction->setCheckable(true);
	pAction->setChecked(pCurve && pCurve == pCurveList->currentCurve());
	pAction->setData(qVariantFromValue(pMidiObserver));
	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		pMainForm, SLOT(trackCurveSelect(bool)));

	pMenu->addSeparator();

	QMenu *pTrackCurveModeMenu = pMainForm->trackCurveModeMenu();
	pTrackCurveModeMenu->setEnabled(pCurve != NULL);
	pMenu->addMenu(pTrackCurveModeMenu);

	pAction = pMenu->addAction(tr("&Lock"));
	pAction->setCheckable(true);
	pAction->setChecked(pCurve && pCurve->isLocked());
	pAction->setEnabled(pCurve != NULL);
	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		pMainForm, SLOT(trackCurveLocked(bool)));

	pMenu->addSeparator();

	QIcon iconProcess;
	iconProcess.addPixmap(
		QPixmap(":/images/trackCurveProcess.png"), QIcon::Normal, QIcon::On);
	iconProcess.addPixmap(
		QPixmap(":/images/trackCurveEnabled.png"), QIcon::Normal, QIcon::Off);
	iconProcess.addPixmap(
		QPixmap(":/images/trackCurveNone.png"), QIcon::Disabled, QIcon::Off);

	pAction = pMenu->addAction(iconProcess, tr("&Play"));
	pAction->setCheckable(true);
	pAction->setChecked(pCurve && pCurve->isProcess());
	pAction->setEnabled(pCurve != NULL);
	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		pMainForm, SLOT(trackCurveProcess(bool)));

	QIcon iconCapture;
	iconCapture.addPixmap(
		QPixmap(":/images/trackCurveCapture.png"), QIcon::Normal, QIcon::On);
	iconCapture.addPixmap(
		QPixmap(":/images/trackCurveEnabled.png"), QIcon::Normal, QIcon::Off);
	iconProcess.addPixmap(
		QPixmap(":/images/trackCurveNone.png"), QIcon::Disabled, QIcon::Off);

	pAction = pMenu->addAction(iconCapture, tr("&Record"));
	pAction->setCheckable(true);
	pAction->setChecked(pCurve && pCurve->isCapture());
	pAction->setEnabled(pCurve != NULL);
	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		pMainForm, SLOT(trackCurveCapture(bool)));

	pMenu->addSeparator();

	pAction = pMenu->addAction(tr("&Clear"));
	pAction->setEnabled(pCurve != NULL);
	QObject::connect(
		pAction, SIGNAL(triggered(bool)),
		pMainForm, SLOT(trackCurveClear()));
}


// end of qtractorMidiControlObserverForm.cpp
