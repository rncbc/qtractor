// qtractorInstrumentForm.cpp
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

#include "qtractorInstrumentForm.h"

#include "qtractorAbout.h"
#include "qtractorInstrument.h"
#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QUrl>


//----------------------------------------------------------------------
// class qtractorInstrumentGroupItem -- custom group list view item.
//

class qtractorInstrumentGroupItem : public QTreeWidgetItem
{
public:

	// Constructors.
	qtractorInstrumentGroupItem()
		: QTreeWidgetItem(qtractorInstrumentForm::GroupItem)
		{ initGroupItem(); }
	qtractorInstrumentGroupItem(QTreeWidgetItem *pParent, QTreeWidgetItem *pAfter)
		: QTreeWidgetItem(pParent, pAfter, qtractorInstrumentForm::GroupItem)
		{ initGroupItem(); }

protected:

	// Initializer.
	void initGroupItem() { setIcon(0, QIcon(":/icons/itemGroup.png")); }
};


//----------------------------------------------------------------------
// class qtractorInstrumentForm -- instrument file manager form.
//

// Constructor.
qtractorInstrumentForm::qtractorInstrumentForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_iDirtyCount = 0;

	QHeaderView *pHeader = m_ui.InstrumentsListView->header();
	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	pHeader = m_ui.FilesListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	pHeader = m_ui.NamesListView->header();
	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	refreshForm();
	stabilizeForm();

	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.FilesListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.NamesListView,
		SIGNAL(itemCollapsed(QTreeWidgetItem*)),
		SLOT(itemCollapsed(QTreeWidgetItem*)));
	QObject::connect(m_ui.NamesListView,
		SIGNAL(itemExpanded(QTreeWidgetItem*)),
		SLOT(itemExpanded(QTreeWidgetItem*)));
	QObject::connect(m_ui.InstrumentsListView,
		SIGNAL(itemCollapsed(QTreeWidgetItem*)),
		SLOT(itemCollapsed(QTreeWidgetItem*)));
	QObject::connect(m_ui.InstrumentsListView,
		SIGNAL(itemExpanded(QTreeWidgetItem*)),
		SLOT(itemExpanded(QTreeWidgetItem*)));
	QObject::connect(m_ui.ImportPushButton,
		SIGNAL(clicked()),
		SLOT(importSlot()));
	QObject::connect(m_ui.RemovePushButton,
		SIGNAL(clicked()),
		SLOT(removeSlot()));
	QObject::connect(m_ui.MoveUpPushButton,
		SIGNAL(clicked()),
		SLOT(moveUpSlot()));
	QObject::connect(m_ui.MoveDownPushButton,
		SIGNAL(clicked()),
		SLOT(moveDownSlot()));
	QObject::connect(m_ui.ReloadPushButton,
		SIGNAL(clicked()),
		SLOT(reloadSlot()));
	QObject::connect(m_ui.ExportPushButton,
		SIGNAL(clicked()),
		SLOT(exportSlot()));
	QObject::connect(m_ui.ClosePushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qtractorInstrumentForm::~qtractorInstrumentForm (void)
{
}


// Import new intrument file(s) into listing.
void qtractorInstrumentForm::importSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	QStringList files;

	const QString  sExt("ins");
	const QString& sTitle  = tr("Import Instrument Files") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Instrument files (*.%1)").arg(sExt);
#if QT_VERSION < 0x040400
	// Ask for the filename to open...
	files = QFileDialog::getOpenFileNames(this,
		sTitle, pOptions->sInstrumentDir, sFilter);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sInstrumentDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sInstrumentDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (fileDialog.exec())
		files = fileDialog.selectedFiles();
#endif

	if (files.isEmpty())
		return;

	// Remember this last directory...
	
	// For avery selected instrument file to load...
	QTreeWidgetItem *pItem = NULL;
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// Merge the file contents into global container...
		const QString& sPath = iter.next();
		if (pInstruments->load(sPath)) {
			// Start inserting in the current selected or last item...
			if (pItem == NULL)
				pItem = m_ui.FilesListView->currentItem();
			if (pItem == NULL) {
				int iLastItem = m_ui.FilesListView->topLevelItemCount() - 1;
				if (iLastItem >= 0)
					pItem = m_ui.FilesListView->topLevelItem(iLastItem);
			}
			// New item on the block :-)
			pItem = new QTreeWidgetItem(m_ui.FilesListView, pItem);
			if (pItem) {
				QFileInfo info(sPath);
				pItem->setIcon(0, QIcon(":/icons/itemFile.png"));
				pItem->setText(0, info.fileName());
				pItem->setText(1, sPath);
				m_ui.FilesListView->setCurrentItem(pItem);
				pOptions->sInstrumentDir = info.absolutePath();
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
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		pInstruments->removeFile(pItem->text(1));
		delete pItem;
		m_iDirtyCount++;
	}

	stabilizeForm();
}


// Move a file up on the instrument list.
void qtractorInstrumentForm::moveUpSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		int iItem = m_ui.FilesListView->indexOfTopLevelItem(pItem);
		if (iItem > 0) {
			pItem = m_ui.FilesListView->takeTopLevelItem(iItem);
			m_ui.FilesListView->insertTopLevelItem(iItem - 1, pItem);
			m_ui.FilesListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	stabilizeForm();
}


// Move a file down on the instrument list.
void qtractorInstrumentForm::moveDownSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		int iItem = m_ui.FilesListView->indexOfTopLevelItem(pItem);
		if (iItem < m_ui.FilesListView->topLevelItemCount() - 1) {
			pItem = m_ui.FilesListView->takeTopLevelItem(iItem);
			m_ui.FilesListView->insertTopLevelItem(iItem + 1, pItem);
			m_ui.FilesListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	stabilizeForm();
}


// Reload the complete instrument definitions, from list.
void qtractorInstrumentForm::reloadSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Ooops...
	pInstruments->clearAll();

	// Load each file in order...
	int iItemCount = m_ui.FilesListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; iItem++) {
		QTreeWidgetItem *pItem = m_ui.FilesListView->topLevelItem(iItem);
		if (pItem) 
			pInstruments->load(pItem->text(1));
	}

	// Not dirty anymore...
	m_iDirtyCount = 0;

	refreshForm();
	stabilizeForm();
}


// Export the whole state into a single instrument file.
void qtractorInstrumentForm::exportSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	QString sPath;

	const QString  sExt("ins");
	const QString& sTitle  = tr("Export Instrument File") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Instrument files (*.%1)").arg(sExt);
#if QT_VERSION < 0x040400
	// Ask for the filename to open...
	sPath = QFileDialog::getOpenFileName(this,
		sTitle, pOptions->sInstrumentDir, sFilter);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sInstrumentDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sInstrumentDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (fileDialog.exec())
		sPath = fileDialog.selectedFiles().first();
#endif

	if (sPath.isEmpty())
		return;

	// Enforce .ins extension...
	if (QFileInfo(sPath).suffix().isEmpty())
		sPath += '.' + sExt;

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
	if (pInstruments->save(sPath))
		pOptions->sInstrumentDir = QFileInfo(sPath).absolutePath();
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
	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		int iItem = m_ui.FilesListView->indexOfTopLevelItem(pItem);
		int iItemCount = m_ui.FilesListView->topLevelItemCount();
		m_ui.RemovePushButton->setEnabled(true);
		m_ui.MoveUpPushButton->setEnabled(iItem > 0);
		m_ui.MoveDownPushButton->setEnabled(iItem < iItemCount - 1);
	} else {
		m_ui.RemovePushButton->setEnabled(false);
		m_ui.MoveUpPushButton->setEnabled(false);
		m_ui.MoveDownPushButton->setEnabled(false);
	}

	m_ui.ReloadPushButton->setEnabled(m_iDirtyCount > 0);

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && pSession->instruments())
		m_ui.ExportPushButton->setEnabled((pSession->instruments())->count() > 0);
	else
		m_ui.ExportPushButton->setEnabled(false);
}


// Refresh all instrument definition views.
void qtractorInstrumentForm::refreshForm (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Freeze...
	m_ui.InstrumentsListView->setUpdatesEnabled(false);
	m_ui.FilesListView->setUpdatesEnabled(false);
	m_ui.NamesListView->setUpdatesEnabled(false);

	// Files list view...
	m_ui.FilesListView->clear();
	QList<QTreeWidgetItem *> files;
	QStringListIterator ifile(pInstruments->files());
	while (ifile.hasNext()) {
		const QString& sPath = ifile.next();
		QTreeWidgetItem *pFileItem = new QTreeWidgetItem();
		pFileItem->setIcon(0, QIcon(":/icons/itemFile.png"));
		pFileItem->setText(0, QFileInfo(sPath).fileName());
		pFileItem->setText(1, sPath);
		files.append(pFileItem);
	}
	m_ui.FilesListView->addTopLevelItems(files);

	// Instruments list view...
	m_ui.InstrumentsListView->clear();
	QList<QTreeWidgetItem *> instrs;
	qtractorInstrumentList::ConstIterator iter;
	for (iter = pInstruments->begin();
			iter != pInstruments->end(); ++iter) {
		const qtractorInstrument& instr = iter.value();
		// Instrument Name...
		QTreeWidgetItem *pChildItem = NULL;
		QTreeWidgetItem *pInstrItem = new QTreeWidgetItem();
		pInstrItem->setIcon(0, QIcon(":/icons/itemInstrument.png"));
		pInstrItem->setText(0, instr.instrumentName());
		// - Patches Names for Banks...
		pChildItem = new qtractorInstrumentGroupItem(pInstrItem, pChildItem);
		pChildItem->setText(0, tr("Patch Names for Banks"));
		QTreeWidgetItem *pBankItem = NULL;
		qtractorInstrumentPatches::ConstIterator pat;
		for (pat = instr.patches().begin();
				pat != instr.patches().end(); ++pat) {
			pBankItem = new QTreeWidgetItem(pChildItem, pBankItem);
			int iBank = pat.key();
			const QString sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			pBankItem->setIcon(0, QIcon(":/icons/itemPatches.png"));
			pBankItem->setText(0,
				QString("%1 = %2").arg(sBank).arg(pat.value().name()));
			// Patches/Progs...
			const qtractorInstrumentData& patch = instr.patch(iBank);
			QTreeWidgetItem *pProgItem = NULL;
			if (!patch.basedOn().isEmpty()) {
				pProgItem = new QTreeWidgetItem(pBankItem, pProgItem);
				pProgItem->setIcon(0, QIcon(":/icons/itemProperty.png"));
				pProgItem->setText(0,
					QString("Based On = %1").arg(patch.basedOn()));
			}
			qtractorInstrumentData::ConstIterator it;
			for (it = patch.constBegin(); it != patch.constEnd(); ++it) {
				int iProg = it.key();
				pProgItem = new QTreeWidgetItem(pBankItem, pProgItem);
				pProgItem->setText(0,
					QString("%1 = %2").arg(iProg).arg(it.value()));
				if (instr.isDrum(iBank, iProg))
					listInstrumentData(pProgItem, instr.notes(iBank, iProg));
			}
		}
		// - Controller Names...
		if (instr.control().count() > 0) {
			pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
			pChildItem->setIcon(0, QIcon(":/icons/itemControllers.png"));
			pChildItem->setText(0,
				tr("Controller Names = %1").arg(instr.control().name()));
			listInstrumentData(pChildItem, instr.control());
		}
		// - RPN Names...
		if (instr.rpn().count() > 0) {
			pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
			pChildItem->setIcon(0, QIcon(":/icons/itemRpns.png"));
			pChildItem->setText(0,
				tr("RPN Names = %1").arg(instr.rpn().name()));
			listInstrumentData(pChildItem, instr.rpn());
		}
		// - NRPN Names...
		if (instr.nrpn().count() > 0) {
			pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
			pChildItem->setIcon(0, QIcon(":/icons/itemNrpns.png"));
			pChildItem->setText(0,
				tr("NRPN Names = %1").arg(instr.nrpn().name()));
			listInstrumentData(pChildItem, instr.nrpn());
		}
		// - BankSelMethod...
		pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
		pChildItem->setIcon(0, QIcon(":/icons/itemProperty.png"));
		pChildItem->setText(0,
			tr("Bank Select Method = %1")
			.arg(bankSelMethod(instr.bankSelMethod())));
		instrs.append(pInstrItem);
	}
	m_ui.InstrumentsListView->addTopLevelItems(instrs);

	// Names list view...
	m_ui.NamesListView->clear();
	QList<QTreeWidgetItem *> names;
	if (pInstruments->count() > 0) {
		QTreeWidgetItem *pListItem = NULL;
		// - Patch Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Patch Names"));
		listInstrumentDataList(pListItem, pInstruments->patches(),
			QIcon(":/icons/itemPatches.png"));
		names.append(pListItem);
		// - Note Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Note Names"));
		listInstrumentDataList(pListItem, pInstruments->notes(),
			QIcon(":/icons/itemNotes.png"));
		names.append(pListItem);
		// - Controller Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Controller Names"));
		listInstrumentDataList(pListItem, pInstruments->controllers(),
			QIcon(":/icons/itemControllers.png"));
		names.append(pListItem);
		// - RPN Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("RPN Names"));
		listInstrumentDataList(pListItem, pInstruments->rpns(),
			QIcon(":/icons/itemRpns.png"));
		names.append(pListItem);
		// - NRPN Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("NRPN Names"));
		listInstrumentDataList(pListItem, pInstruments->nrpns(),
			QIcon(":/icons/itemNrpns.png"));
		names.append(pListItem);
		// - Bank Select Methods...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Bank Select Methods"));
		if (pInstruments->count() > 0) {
			QTreeWidgetItem *pChildItem = NULL;
			for (int iBankSelMethod = 0; iBankSelMethod < 4; iBankSelMethod++) {
				pChildItem = new qtractorInstrumentGroupItem(pListItem, pChildItem);
				pChildItem->setIcon(0, QIcon(":/icons/itemProperty.png"));
				pChildItem->setText(0,
					tr("%1 = %2").arg(iBankSelMethod)
					.arg(bankSelMethod(iBankSelMethod)));
			}
		}
		names.append(pListItem);
	}
	m_ui.NamesListView->addTopLevelItems(names);

	// Bail out...
	m_ui.NamesListView->setUpdatesEnabled(true);
	m_ui.FilesListView->setUpdatesEnabled(true);
	m_ui.InstrumentsListView->setUpdatesEnabled(true);
}


void qtractorInstrumentForm::itemCollapsed ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/icons/itemGroup.png"));
}


void qtractorInstrumentForm::itemExpanded ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/icons/itemGroupOpen.png"));
}


void qtractorInstrumentForm::listInstrumentData (
	QTreeWidgetItem *pParentItem, const qtractorInstrumentData& data )
{
	QTreeWidgetItem *pItem = NULL;
	if (!data.basedOn().isEmpty()) {
		pItem = new QTreeWidgetItem(pParentItem, pItem);
		pItem->setIcon(0, QIcon(":/icons/itemProperty.png"));
		pItem->setText(0,
			tr("Based On = %1").arg(data.basedOn()));
	}
	qtractorInstrumentData::ConstIterator it;
	for (it = data.constBegin(); it != data.constEnd(); ++it) {
		pItem = new QTreeWidgetItem(pParentItem, pItem);
		pItem->setText(0,
			QString("%1 = %2").arg(it.key()).arg(it.value()));
	}
}


void qtractorInstrumentForm::listInstrumentDataList (
	QTreeWidgetItem *pParentItem, const qtractorInstrumentDataList& list,
	const QIcon& icon )
{
	QTreeWidgetItem *pItem = NULL;
	qtractorInstrumentDataList::ConstIterator it;
	for (it = list.begin(); it != list.end(); ++it) {
		pItem = new QTreeWidgetItem(pParentItem, pItem);
		pItem->setIcon(0, icon);
		pItem->setText(0, it.value().name());
		listInstrumentData(pItem, it.value());
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


// end of qtractorInstrumentForm.cpp
