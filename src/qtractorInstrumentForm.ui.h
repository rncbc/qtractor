// qtractorInstrumentForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorInstrument.h"

#include "qtractorOptions.h"

#include <qfiledialog.h>
#include <qmessagebox.h>
#include <qpixmap.h>


//----------------------------------------------------------------------
// class qtractorInstrumentGroupItem -- custom group list view item.
//

class qtractorInstrumentGroupItem : public QListViewItem
{
public:

	// Constructors.
	qtractorInstrumentGroupItem(QListViewItem *pParent, QListViewItem *pAfter)
		: QListViewItem(pParent, pAfter) { init(); }
	qtractorInstrumentGroupItem(QListView *pListView, QListViewItem *pAfter)
		: QListViewItem(pListView, pAfter) { init(); }

protected:

	// Initializer.
	void init() { setPixmap(0, QPixmap::fromMimeSource("itemGroup.png")); }

	// To show up whether its open or not.
	void setOpen(bool bOpen)
	{
		setPixmap(0, QPixmap::fromMimeSource(
				bOpen ? "itemGroupOpen.png" : "itemGroup.png"));
		QListViewItem::setOpen(bOpen);
	}
};


//----------------------------------------------------------------------
// class qtractorInstrumentForm -- instrument file manager form.
//

// Kind of post-constructor.
void qtractorInstrumentForm::init (void)
{
	m_pInstruments = NULL;
	m_pOptions     = NULL;
	m_iDirtyCount  = 0;

	InstrumentsListView->setSorting(-1);
	NamesListView->setSorting(-1);
	FilesListView->setSorting(-1);

	InstrumentsListView->setSelectionMode(QListView::NoSelection);
	NamesListView->setSelectionMode(QListView::NoSelection);
/*
	InstrumentsListView->setColumnWidth(0, InstrumentsListView->width());
	NamesListView->setColumnWidth(0, NamesListView->width());
	FilesListView->setColumnWidth(0, FilesListView->width());
*/
	adjustSize();
}


// Kind of pre-destructor.
void qtractorInstrumentForm::destroy (void)
{
}


// Instrument list accessors.
void qtractorInstrumentForm::setInstruments (
	qtractorInstrumentList *pInstruments )
{
	m_pInstruments = pInstruments;

	refreshForm();
	stabilizeForm();
}


qtractorInstrumentList *qtractorInstrumentForm::instruments (void)
{
	return m_pInstruments;
}


// General options accessors.
void qtractorInstrumentForm::setOptions (
	qtractorOptions *pOptions )
{
	m_pOptions = pOptions;
}


qtractorOptions *qtractorInstrumentForm::options (void)
{
	return m_pOptions;
}


// Import new intrument file(s) into listing.
void qtractorInstrumentForm::importSlot (void)
{
	if (m_pOptions == NULL)
		return;
	if (m_pInstruments == NULL)
		return;

	QStringList files = QFileDialog::getOpenFileNames(
			tr("Instrument Files (*.ins)"),     // Filter files.
			m_pOptions->sInstrumentDir,         // Start here.
			this, 0,                            // Parent and name (none)
			tr("Import Instrument Files")       // Caption.
	);

	if (files.isEmpty())
		return;

	// Remember this last directory...
	
	// For avery selected instrument file to load...
	QListViewItem *pItem = NULL;
	for (QStringList::Iterator iter = files.begin();
			iter != files.end(); iter++) {
		// Merge the file contents into global container...
		const QString& sPath = *iter;
		if (m_pInstruments->load(sPath)) {
			// Start inserting in the current selected or last item...
			if (pItem == NULL) {
				pItem = FilesListView->selectedItem();
				if (pItem)
					pItem->setSelected(false);
				else
					pItem = FilesListView->lastItem();
			}
			// New item on the block :-)
			pItem = new QListViewItem(FilesListView, pItem);
			if (pItem) {
				QFileInfo info(sPath);
				pItem->setPixmap(0, QPixmap::fromMimeSource("itemFile.png"));
				pItem->setText(0, info.fileName());
				pItem->setText(1, sPath);
				pItem->setSelected(true);
				FilesListView->setCurrentItem(pItem);
				m_pOptions->sInstrumentDir = info.dirPath(true);
			//	m_iDirtyCount++;
			}
		}
	}

	// May refresh the whole form?
	refreshForm();
	stabilizeForm();
}


// Remove a file from instrument list.
void qtractorInstrumentForm::removeSlot (void)
{
	if (m_pInstruments == NULL)
		return;

	QListViewItem *pItem = FilesListView->selectedItem();
	if (pItem) {
		m_pInstruments->removeFile(pItem->text(1));
		delete pItem;
		m_iDirtyCount++;
	}

	stabilizeForm();
}


// Move a file up on the instrument list.
void qtractorInstrumentForm::moveUpSlot (void)
{
	QListViewItem *pItem = FilesListView->selectedItem();
	if (pItem) {
		QListViewItem *pItemAbove = pItem->itemAbove();
		if (pItemAbove) {
			pItem->setSelected(false);
			pItemAbove = pItemAbove->itemAbove();
			if (pItemAbove) {
				pItem->moveItem(pItemAbove);
			} else {
				FilesListView->takeItem(pItem);
				FilesListView->insertItem(pItem);
			}
			pItem->setSelected(true);
			FilesListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	stabilizeForm();
}


// Move a file down on the instrument list.
void qtractorInstrumentForm::moveDownSlot (void)
{
	QListViewItem *pItem = FilesListView->selectedItem();
	if (pItem) {
		QListViewItem *pItemBelow = pItem->nextSibling();
		if (pItemBelow) {
			pItem->setSelected(false);
			pItem->moveItem(pItemBelow);
			pItem->setSelected(true);
			FilesListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	stabilizeForm();
}


// Reload the complete instrument definitions, from list.
void qtractorInstrumentForm::reloadSlot (void)
{
	if (m_pInstruments == NULL)
		return;

	// Ooops...
	m_pInstruments->clearAll();

	// Load each file in order...
	for (QListViewItem *pItem = FilesListView->firstChild();
			pItem; pItem = pItem->nextSibling()) {
		m_pInstruments->load(pItem->text(1));
	}
	// Not dirty anymore...
	m_iDirtyCount = 0;

	refreshForm();
	stabilizeForm();
}


// Export the whole state into a single instrument file.
void qtractorInstrumentForm::exportSlot (void)
{
	if (m_pOptions == NULL)
		return;
	if (m_pInstruments == NULL)
		return;

	QString sPath = QFileDialog::getSaveFileName(
			m_pOptions->sInstrumentDir,         // Start here.
			tr("Instrument Files (*.ins)"),     // Filter files.
			this, 0,                            // Parent and name (none)
			tr("Export Instrument File")        // Caption.
	);

	if (sPath.isEmpty())
		return;

	// Enforce .ins extension...
	if (QFileInfo(sPath).extension().isEmpty())
		sPath += ".ins";

	// Check if already exists...
	if (QFileInfo(sPath).exists()) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("The instrument file already exists:\n\n"
			"\"%1\"\n\n"
			"Do you want to replace it?")
			.arg(sPath),
			tr("Replace"), tr("Cancel")) > 0)
			return;
	}

	// Just save the whole bunch...
	if (m_pInstruments->save(sPath))
		m_pOptions->sInstrumentDir = QFileInfo(sPath).dirPath(true);
}


// Accept settings (OK button slot).
void qtractorInstrumentForm::accept (void)
{
	// If we're dirty do a complete and final reload...
	if (m_iDirtyCount > 0)
		reloadSlot();

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorInstrumentForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Instrument settings have been changed.") + "\n\n" +
			tr("Do you want to apply the changes?"),
			tr("Apply"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Apply...
			accept();
			return;
		case 1:     // Discard
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Stabilize form status.
void qtractorInstrumentForm::stabilizeForm (void)
{
	QListViewItem *pSelectedItem = FilesListView->selectedItem();
	if (pSelectedItem) {
		RemovePushButton->setEnabled(true);
		MoveUpPushButton->setEnabled(pSelectedItem->itemAbove() != NULL);
		MoveDownPushButton->setEnabled(pSelectedItem->nextSibling() != NULL);
	} else {
		RemovePushButton->setEnabled(false);
		MoveUpPushButton->setEnabled(false);
		MoveDownPushButton->setEnabled(false);
	}

	ReloadPushButton->setEnabled(m_iDirtyCount > 0);
	ExportPushButton->setEnabled(m_pInstruments->count() > 0);
}


// Refresh all instrument definition views.
void qtractorInstrumentForm::refreshForm (void)
{
	// Files list view...
	FilesListView->clear();
	QListViewItem *pFileItem = NULL;
	for (QStringList::ConstIterator iter = m_pInstruments->files().begin();
			iter != m_pInstruments->files().end(); ++iter) {
		const QString& sPath = *iter;
		pFileItem = new QListViewItem(FilesListView, pFileItem);
		pFileItem->setPixmap(0, QPixmap::fromMimeSource("itemFile.png"));
		pFileItem->setText(0, QFileInfo(sPath).fileName());
		pFileItem->setText(1, sPath);
	}

	// Instruments list view...
	InstrumentsListView->clear();
	QListViewItem *pInstrItem = NULL;
	QListViewItem *pChildItem = NULL;
	qtractorInstrumentList::Iterator iter;
	for (iter = m_pInstruments->begin();
			iter != m_pInstruments->end(); ++iter) {
		qtractorInstrument& instr = iter.data();
		// Instrument Name...
		pInstrItem = new QListViewItem(InstrumentsListView, pInstrItem);
		pInstrItem->setText(0, instr.instrumentName());
		pInstrItem->setPixmap(0, QPixmap::fromMimeSource("itemInstrument.png"));
		// - Patches Names for Banks...
		pChildItem = new qtractorInstrumentGroupItem(pInstrItem, pChildItem);
		pChildItem->setText(0, tr("Patch Names for Banks"));
		QListViewItem *pBankItem = NULL;
		qtractorInstrumentPatches::Iterator pat;
		for (pat = instr.patches().begin();
				pat != instr.patches().end(); ++pat) {
			pBankItem = new QListViewItem(pChildItem, pBankItem);
			int iBank = pat.key();
			const QString sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			pBankItem->setText(0,
				QString("%1 = %2").arg(sBank).arg(pat.data().name()));
			pBankItem->setPixmap(0,
				QPixmap::fromMimeSource("itemPatches.png"));
			// Patches/Progs...
			qtractorInstrumentData& patch = instr.patch(iBank);
			QListViewItem *pProgItem = NULL;
			if (!patch.basedOn().isEmpty()) {
				pProgItem = new QListViewItem(pBankItem, pProgItem);
				pProgItem->setText(0,
					QString("Based On = %1").arg(patch.basedOn()));
				pProgItem->setPixmap(0,
					QPixmap::fromMimeSource("itemProperty.png"));
			}
			qtractorInstrumentData::Iterator it;
			for (it = patch.begin(); it != patch.end(); ++it) {
				int iProg = it.key();
				pProgItem = new QListViewItem(pBankItem, pProgItem);
				pProgItem->setText(0,
					QString("%1 = %2").arg(iProg).arg(it.data()));
				if (instr.isDrum(iBank, iProg))
					listInstrumentData(pProgItem, instr.notes(iBank, iProg));
			}
		}
		// - Controller Names...
		if (instr.control().count() > 0) {
			pChildItem = new QListViewItem(pInstrItem, pChildItem);
			pChildItem->setText(0,
				tr("Controller Names = %1").arg(instr.control().name()));
			pChildItem->setPixmap(0,
				QPixmap::fromMimeSource("itemControllers.png"));
			listInstrumentData(pChildItem, instr.control());
		}
		// - RPN Names...
		if (instr.rpn().count() > 0) {
			pChildItem = new QListViewItem(pInstrItem, pChildItem);
			pChildItem->setText(0,
				tr("RPN Names = %1").arg(instr.rpn().name()));
			pChildItem->setPixmap(0,
				QPixmap::fromMimeSource("itemRpns.png"));
			listInstrumentData(pChildItem, instr.rpn());
		}
		// - NRPN Names...
		if (instr.nrpn().count() > 0) {
			pChildItem = new QListViewItem(pInstrItem, pChildItem);
			pChildItem->setText(0,
				tr("NRPN Names = %1").arg(instr.nrpn().name()));
			pChildItem->setPixmap(0,
				QPixmap::fromMimeSource("itemNrpns.png"));
			listInstrumentData(pChildItem, instr.nrpn());
		}
		// - BankSelMethod...
		pChildItem = new QListViewItem(pInstrItem, pChildItem);
		pChildItem->setText(0,
			tr("Bank Select Method = %1")
			.arg(bankSelMethod(instr.bankSelMethod())));
		pChildItem->setPixmap(0,
			QPixmap::fromMimeSource("itemProperty.png"));
	}

	// Names list view...
	NamesListView->clear();
	if (m_pInstruments->count() > 0) {
		QListViewItem *pListItem = NULL;
		// - Patch Names...
		pListItem = new qtractorInstrumentGroupItem(NamesListView, pListItem);
		pListItem->setText(0, tr("Patch Names"));
		listInstrumentDataList(pListItem, m_pInstruments->patches(),
			QPixmap::fromMimeSource("itemPatches.png"));
		// - Note Names...
		pListItem = new qtractorInstrumentGroupItem(NamesListView, pListItem);
		pListItem->setText(0, tr("Note Names"));
		listInstrumentDataList(pListItem, m_pInstruments->notes(),
			QPixmap::fromMimeSource("itemNotes.png"));
		// - Controller Names...
		pListItem = new qtractorInstrumentGroupItem(NamesListView, pListItem);
		pListItem->setText(0, tr("Controller Names"));
		listInstrumentDataList(pListItem, m_pInstruments->controllers(),
			QPixmap::fromMimeSource("itemControllers.png"));
		// - RPN Names...
		pListItem = new qtractorInstrumentGroupItem(NamesListView, pListItem);
		pListItem->setText(0, tr("RPN Names"));
		listInstrumentDataList(pListItem, m_pInstruments->rpns(),
			QPixmap::fromMimeSource("itemRpns.png"));
		// - NRPN Names...
		pListItem = new qtractorInstrumentGroupItem(NamesListView, pListItem);
		pListItem->setText(0, tr("NRPN Names"));
		listInstrumentDataList(pListItem, m_pInstruments->nrpns(),
			QPixmap::fromMimeSource("itemNrpns.png"));
		// - Bank Select Methods...
		pListItem = new qtractorInstrumentGroupItem(NamesListView, pListItem);
		pListItem->setText(0, tr("Bank Select Methods"));
		if (m_pInstruments->count() > 0) {
			pChildItem = NULL;
			for (int iBankSelMethod = 0; iBankSelMethod < 4; iBankSelMethod++) {
				pChildItem = new qtractorInstrumentGroupItem(pListItem, pChildItem);
				pChildItem->setText(0,
					tr("%1 = %2").arg(iBankSelMethod)
					.arg(bankSelMethod(iBankSelMethod)));
				pChildItem->setPixmap(0,
					QPixmap::fromMimeSource("itemProperty.png"));
			}
		}
	}
}


void qtractorInstrumentForm::listInstrumentData (
	QListViewItem *pParentItem, qtractorInstrumentData& data )
{
	QListViewItem *pItem = NULL;
	if (!data.basedOn().isEmpty()) {
		pItem = new QListViewItem(pParentItem, pItem);
		pItem->setText(0,
			tr("Based On = %1").arg(data.basedOn()));
		pItem->setPixmap(0,
			QPixmap::fromMimeSource("itemProperty.png"));
	}
	qtractorInstrumentData::Iterator it;
	for (it = data.begin(); it != data.end(); ++it) {
		pItem = new QListViewItem(pParentItem, pItem);
		pItem->setText(0,
			QString("%1 = %2").arg(it.key()).arg(it.data()));
	}
}


void qtractorInstrumentForm::listInstrumentDataList (
	QListViewItem *pParentItem, qtractorInstrumentDataList& list,
	const QPixmap& pixmap )
{
	QListViewItem *pItem = NULL;
	qtractorInstrumentDataList::Iterator it;
	for (it = list.begin(); it != list.end(); ++it) {
		pItem = new QListViewItem(pParentItem, pItem);
		pItem->setText(0, it.data().name());
		pItem->setPixmap(0, pixmap);
		listInstrumentData(pItem, it.data());
	}
}


QString qtractorInstrumentForm::bankSelMethod ( int iBankSelMethod )
{
	QString sText;
	switch (iBankSelMethod) {
		case 0:  sText = tr("Normal");   break;
		case 1:  sText = tr("Bank MSB"); break;
		case 2:  sText = tr("Bank LSB"); break;
		case 3:  sText = tr("Patch");    break;
		default: sText = tr("Unknown");  break;
	}
	return sText;
}


// end of qtractorInstrumentForm.ui.h
