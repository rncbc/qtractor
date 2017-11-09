// qtractorMidiImportForm.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiImportForm_h
#define __qtractorMidiImportForm_h

#include "ui_qtractorMidiImportForm.h"

// forward decalarations.
class qtractorMidiImportExtender;
class qtractorCommand;
class qtractorMidiManager;
class qtractorTrack;
class qtractorEditTrackCommand;


class qtractorMidiImportForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMidiImportForm(qtractorMidiImportExtender *pMidiImportExtender,
		QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	virtual ~qtractorMidiImportForm();

protected slots:

	void accept();
	void reject();

	void stabilizeListButtons();

	void pluginListChanged();
	void addPlugin();
	void removePlugin();
	void moveUpPlugin();
	void moveDownPlugin();

	void changedInstInst(const QString &text);
	void changedDrumInst(const QString &text);

	void changed();
private:

	// Update instruments comboboxes.
	void updateInstruments();
	void updateInstrumentsAdd(
		const QIcon& icon, qtractorMidiManager *pMidiManager);

	// Update instruments comboboxes.
	enum BankType { Instruments, Drums };
	void updateBanks(const QString& sInstrumentName, int iBank, enum BankType bankType);
	bool updateBanksAdd(
		const QIcon& icon, qtractorMidiManager *pMidiManager,
		const QString& sInstrumentName, int iBank, int& iBankIndex,
			QComboBox* pComboBox, QMap<int, int> *pBankMap);

	// Avoid redundancy for empty texts
	// FIXME: move to a more common place and share with e.g with track-form
	const QString getInstrumentNone() { return tr("(No instrument)"); }
	const QString getBankNone() { return tr("(None)"); }

	// Cleanup right before dialog is closed.
	void commonCleanup();

	// The Qt-designer UI struct...
	Ui::qtractorMidiImportForm m_ui;

	// Pointer to object doing the job of MIDI import extension.
	qtractorMidiImportExtender *m_pMidiImportExtender;

	// Keep last command for backout / restore.
	qtractorCommand *m_pLastCommand;

	// Button group around name-type radios
	QButtonGroup* m_groupNameRadios;

	// counter avoiding unwanted handling of change events
	int m_iDirtySetup;

	// Entry in combobox / bank map for instruments.
	QMap<int, int> m_instBanks;
	// Entry in combobox / bank map for drums.
	QMap<int, int> m_drumBanks;
};

#endif // __qtractorMidiImportForm_h

// end of qtractorMidiImportForm.h
