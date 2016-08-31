// qtractorInstrumentMenu.cpp
//
/****************************************************************************
   Copyright (C) 2005-2016, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorInstrumentMenu.h"
#include "qtractorInstrument.h"

#include "qtractorTrack.h"
#include "qtractorTrackCommand.h"
#include "qtractorSession.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiManager.h"

#include <QMenu>
#include <QProxyStyle>


//----------------------------------------------------------------------
// class qtractorInstrumentMenu -- instrument definition data classes.
//

// Constructor.
qtractorInstrumentMenu::qtractorInstrumentMenu ( QObject *pParent )
	: QObject(pParent), m_pTrack(NULL)
{
}

// Main accessor initiator.
void qtractorInstrumentMenu::updateTrackMenu (
	qtractorTrack *pTrack, QMenu *pMenu )
{
	if (pTrack == NULL)
		return;

	m_pTrack = pTrack;

	trackMenuReset(pMenu);
}


bool qtractorInstrumentMenu::trackMenuReset ( QMenu *pMenu ) const
{
	pMenu->clear();
//	pMenu->setStyle(scrollableMenuStyle());

	if (m_pTrack == NULL)
		return false;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return false;

	QString sCurrentName;
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pTrack->outputBus());
	if (pMidiBus) {
		const unsigned short iChannel = m_pTrack->midiChannel();
		const qtractorMidiBus::Patch& patch = pMidiBus->patch(iChannel);
		sCurrentName = patch.instrumentName;
	}

	// Instrument sub-menu...
	const QIcon icon(":/images/itemInstrument.png");

	trackMenuAdd(pMenu, icon,
		(m_pTrack->pluginList())->midiManager(), sCurrentName);

	if (pMidiBus && pMidiBus->pluginList_out()) {
		trackMenuAdd(pMenu, icon,
			(pMidiBus->pluginList_out())->midiManager(), sCurrentName);
	}

	qtractorInstrumentList::ConstIterator iter = pInstruments->constBegin();
	const qtractorInstrumentList::ConstIterator& iter_end = pInstruments->constEnd();
	for ( ; iter != iter_end; ++iter) {
		const qtractorInstrument& instr = iter.value();
		const QString& sInstrumentName = instr.instrumentName();
		if (sInstrumentName.isEmpty()) continue;
		QMenu *pBankMenu = pMenu->addMenu(icon, sInstrumentName);
		QAction *pAction = pBankMenu->menuAction();
		pAction->setCheckable(true);
		pAction->setChecked(sInstrumentName == sCurrentName);
		QObject::connect(pBankMenu,
			SIGNAL(aboutToShow()),
			SLOT(updateBankMenu()));
	}

	return true;
}

bool qtractorInstrumentMenu::trackMenuAdd (
	QMenu *pMenu, const QIcon& icon,
	qtractorMidiManager *pMidiManager,
	const QString& sCurrentName ) const
{
	if (pMidiManager == NULL)
		return false;

	pMidiManager->updateInstruments();

	// Instrument sub-menu...
	const qtractorMidiManager::Instruments& list = pMidiManager->instruments();
	qtractorMidiManager::Instruments::ConstIterator iter = list.constBegin();
	const qtractorMidiManager::Instruments::ConstIterator& iter_end = list.constEnd();
	for ( ; iter != iter_end; ++iter) {
		const QString& sInstrumentName = iter.key();
		if (sInstrumentName.isEmpty()) continue;
		QMenu *pBankMenu = pMenu->addMenu(icon, sInstrumentName);
		QAction *pAction = pBankMenu->menuAction();
		pAction->setCheckable(true);
		pAction->setChecked(sInstrumentName == sCurrentName);
		QObject::connect(pBankMenu,
			SIGNAL(aboutToShow()),
			SLOT(updateBankMenu()));
	}

	return true;
}


// Track/Instrument bank sub-menu stabilizers.
void qtractorInstrumentMenu::updateBankMenu (void)
{
	QMenu *pBankMenu = qobject_cast<QMenu *> (sender());
	if (pBankMenu)
		bankMenuReset(pBankMenu);
}

bool qtractorInstrumentMenu::bankMenuReset ( QMenu *pBankMenu ) const
{
	pBankMenu->clear();
//	pBankMenu->setStyle(scrollableMenuStyle());

	if (m_pTrack == NULL)
		return false;

	const QString sInstrumentName
		= pBankMenu->title().remove('&');
	const int iCurrentBank = m_pTrack->midiBank();

	// Instrument plug-in banks sub-menu...
	const QIcon icon(":/images/itemPatches.png");

	if (bankMenuAdd(pBankMenu, icon,
		(m_pTrack->pluginList())->midiManager(),
		sInstrumentName, iCurrentBank))
		return true;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pTrack->outputBus());
	if (pMidiBus && pMidiBus->pluginList_out()
		&& bankMenuAdd(pBankMenu, icon,
			(pMidiBus->pluginList_out())->midiManager(),
			sInstrumentName, iCurrentBank))
		return true;

	// Instrument bank/patches sub-menu...
	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return false;

	if (pInstruments->contains(sInstrumentName)) {
		const qtractorInstrument& instr = pInstruments->value(sInstrumentName);
		const qtractorInstrumentPatches& patches = instr.patches();
		qtractorInstrumentPatches::ConstIterator patch_iter = patches.constBegin();
		const qtractorInstrumentPatches::ConstIterator& patch_end = patches.constEnd();
		for ( ; patch_iter != patch_end; ++patch_iter) {
			const int iBank = patch_iter.key();
			const qtractorInstrumentData& patch = patch_iter.value();
			const QString& sBankName = patch.name();
			if (iBank < 0 || sBankName.isEmpty()) continue;
			QMenu *pProgMenu = pBankMenu->addMenu(icon, sBankName);
			QAction *pAction = pProgMenu->menuAction();
			pAction->setCheckable(true);
			pAction->setChecked(iBank == iCurrentBank);
			pAction->setData(iBank);
			QObject::connect(pProgMenu,
				SIGNAL(aboutToShow()),
				SLOT(updateProgMenu()));
		}
	}

	// There should always be a dummy bank (-1)...
	if (pBankMenu->actions().count() > 0)
		pBankMenu->addSeparator();

	QMenu *pProgMenu = pBankMenu->addMenu(tr("(None)"));
	QAction *pAction = pProgMenu->menuAction();
	pAction->setCheckable(true);
	pAction->setChecked(iCurrentBank < 0);
	pAction->setData(-1);
	QObject::connect(pProgMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateProgMenu()));

	return true;
}

bool qtractorInstrumentMenu::bankMenuAdd (
	QMenu *pBankMenu, const QIcon& icon,
	qtractorMidiManager *pMidiManager,
	const QString& sInstrumentName,
	int iCurrentBank ) const
{
	if (pMidiManager == NULL)
		return false;

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrumentName))
		return false;

	// Instrument plug-in banks sub-menu...
	const qtractorMidiManager::Banks& banks = list.value(sInstrumentName);
	qtractorMidiManager::Banks::ConstIterator bank_iter = banks.constBegin();
	const qtractorMidiManager::Banks::ConstIterator& bank_end = banks.constEnd();
	for ( ; bank_iter != bank_end; ++bank_iter) {
		const int iBank = bank_iter.key();
		if (iBank < 0) continue;
		const qtractorMidiManager::Bank& bank = bank_iter.value();
		QMenu *pProgMenu = pBankMenu->addMenu(icon, bank.name);
		QAction *pAction = pProgMenu->menuAction();
		pAction->setCheckable(true);
		pAction->setChecked(iBank == iCurrentBank);
		pAction->setData(iBank);
		QObject::connect(pProgMenu,
			SIGNAL(aboutToShow()),
			SLOT(updateProgMenu()));
	}

	// There should always be a dummy bank (-1)...
	if (pBankMenu->actions().count() > 0)
		pBankMenu->addSeparator();

	QMenu *pProgMenu = pBankMenu->addMenu(tr("(None)"));
	QAction *pAction = pProgMenu->menuAction();
	pAction->setCheckable(true);
	pAction->setChecked(iCurrentBank < 0);
	pAction->setData(-1);
	QObject::connect(pProgMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateProgMenu()));

	return true;
}



// Track/Instrument bank sub-menu stabilizers.
void qtractorInstrumentMenu::updateProgMenu (void)
{
	QMenu *pProgMenu = qobject_cast<QMenu *> (sender());
	if (pProgMenu)
		progMenuReset(pProgMenu);
}

bool qtractorInstrumentMenu::progMenuReset ( QMenu *pProgMenu ) const
{
	pProgMenu->clear();
	pProgMenu->setStyle(scrollableMenuStyle());

	if (m_pTrack == NULL)
		return false;

	QAction *pBankAction = pProgMenu->menuAction();
	if (pBankAction == NULL)
		return false;

	QMenu *pBankMenu = NULL;
	QListIterator<QWidget *> iter(pBankAction->associatedWidgets());
	while (iter.hasNext() && pBankMenu == NULL)
		pBankMenu = qobject_cast<QMenu *> (iter.next());
	if (pBankMenu == NULL)
		return false;

	const QString sInstrumentName
		= pBankMenu->title().remove('&');
	const int iBank = pBankAction->data().toInt();
	const int iCurrentProg = m_pTrack->midiProg();

	// Instrument plugin programs sub-menu...
	const QIcon icon(":/images/itemChannel.png");

	if (progMenuAdd(pProgMenu, icon,
		(m_pTrack->pluginList())->midiManager(),
		sInstrumentName, iBank, iCurrentProg))
		return true;

	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pTrack->outputBus());
	if (pMidiBus && pMidiBus->pluginList_out()
		&& progMenuAdd(pProgMenu, icon,
			(pMidiBus->pluginList_out())->midiManager(),
			sInstrumentName, iBank, iCurrentProg))
		return true;

	// Instrument programs sub-menu...
	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return false;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return false;

	if (pInstruments->contains(sInstrumentName)) {
		const qtractorInstrument& instr = pInstruments->value(sInstrumentName);
		const qtractorInstrumentPatches& patches = instr.patches();
		const qtractorInstrumentData& patch = patches[iBank];
		qtractorInstrumentData::ConstIterator prog_iter = patch.constBegin();
		const qtractorInstrumentData::ConstIterator& prog_end = patch.constEnd();
		for ( ; prog_iter != prog_end; ++prog_iter) {
			const int iProg = prog_iter.key();
			if (iProg < 0) continue;
			const QString& sProgName = prog_iter.value();
			QAction *pAction = pProgMenu->addAction(icon, sProgName);
			pAction->setCheckable(true);
			pAction->setChecked(iProg == iCurrentProg);
			pAction->setData(iProg);
			QObject::connect(pAction,
				SIGNAL(triggered(bool)),
				SLOT(progActionTriggered(bool)));
		}
	}

	// There should always be a dummy program (-1)...
	if (pProgMenu->actions().count() > 0)
		pProgMenu->addSeparator();

	QAction *pAction = pProgMenu->addAction(tr("(None)"));
	pAction->setCheckable(true);
	pAction->setChecked(iCurrentProg < 0);
	pAction->setData(-1);
	QObject::connect(pAction,
		SIGNAL(triggered(bool)),
		SLOT(progActionTriggered(bool)));

	return true;
}


bool qtractorInstrumentMenu::progMenuAdd (
	QMenu *pProgMenu, const QIcon& icon,
	qtractorMidiManager *pMidiManager,
	const QString& sInstrumentName,
	int iBank, int iCurrentProg ) const
{
	if (pMidiManager == NULL)
		return false;

	// Instrument plugin programs sub-menu...
	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrumentName))
		return false;

	const qtractorMidiManager::Banks& banks
		= list.value(sInstrumentName);
	if (!banks.contains(iBank))
		return false;

	const QString sProg("%1 - %2");
	const qtractorMidiManager::Progs& progs = banks.value(iBank).progs;
	qtractorMidiManager::Progs::ConstIterator prog_iter = progs.constBegin();
	const qtractorMidiManager::Progs::ConstIterator& prog_end = progs.constEnd();
	for ( ; prog_iter != prog_end; ++prog_iter) {
		const int iProg = prog_iter.key();
		if (iProg < 0) continue;
		const QString& sProgName = sProg.arg(iProg).arg(prog_iter.value());
		QAction *pAction = pProgMenu->addAction(icon, sProgName);
		pAction->setCheckable(true);
		pAction->setChecked(iProg == iCurrentProg);
		pAction->setData(iProg);
		QObject::connect(pAction,
			SIGNAL(triggered(bool)),
			SLOT(progActionTriggered(bool)));
	}

	// There should always be a dummy program (-1)...
	if (pProgMenu->actions().count() > 0)
		pProgMenu->addSeparator();

	QAction *pAction = pProgMenu->addAction(tr("(None)"));
	pAction->setCheckable(true);
	pAction->setChecked(iCurrentProg < 0);
	pAction->setData(-1);
	QObject::connect(pAction,
		SIGNAL(triggered(bool)),
		SLOT(progActionTriggered(bool)));

	return true;
}


// Track/Instrument patch selection.
void qtractorInstrumentMenu::progActionTriggered ( bool /*bOn*/ )
{
	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	QAction *pProgAction = qobject_cast<QAction *> (sender());
	if (pProgAction) {
		const int iProg = pProgAction->data().toInt();
		QListIterator<QWidget *> bank_iter(pProgAction->associatedWidgets());
		while (bank_iter.hasNext()) {
			QMenu *pBankMenu = qobject_cast<QMenu *> (bank_iter.next());
			if (pBankMenu == NULL)
				continue;
			QAction *pBankAction = pBankMenu->menuAction();
			if (pBankAction) {
				const int iBank = pBankAction->data().toInt();
				QListIterator<QWidget *> iter(pBankAction->associatedWidgets());
				while (iter.hasNext()) {
					QMenu *pMenu = qobject_cast<QMenu *> (iter.next());
					if (pMenu) {
						const QString sInstrumentName
							= pMenu->title().remove('&');
						// Make it an undoable command...
						pSession->execute(
							new qtractorTrackInstrumentCommand(
								m_pTrack, sInstrumentName, iBank, iProg));
						// Done.
						m_pTrack = NULL;
						break;
					}
				}
			}
			// Bail out.
			break;
		}
	}
}


// A custom scrollable menu style (static).
QStyle *qtractorInstrumentMenu::scrollableMenuStyle (void)
{
	class ScrollableMenuStyle : public QProxyStyle
	{
	protected:

		int styleHint(StyleHint shint, const QStyleOption *sopt,
			const QWidget *widget, QStyleHintReturn * hret) const
		{
			if (shint == QStyle::SH_Menu_Scrollable)
				return 1;
			else
				return QProxyStyle::styleHint(shint, sopt, widget, hret);
		}
	};

	static ScrollableMenuStyle s_scrollableMenuStyle;

	return &s_scrollableMenuStyle;
}


// end of qtractorInstrumentMenu.cpp
