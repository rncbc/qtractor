// qtractorInstrumentForm.cpp
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

#include "qtractorAbout.h"
#include "qtractorInstrumentForm.h"
#include "qtractorInstrument.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QApplication>
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
	void initGroupItem() { setIcon(0, QIcon(":/images/itemGroup.png")); }
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

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	m_pInstruments = NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_pInstruments = pSession->instruments();

	m_iDirtyCount = 0;

	QHeaderView *pHeader = m_ui.InstrumentsListView->header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif

	pHeader = m_ui.FilesListView->header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif

	pHeader = m_ui.NamesListView->header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif

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


// Instrument list accessors.
void qtractorInstrumentForm::setInstruments ( qtractorInstrumentList *pInstruments )
{
	m_pInstruments = pInstruments;

	refreshForm();
	stabilizeForm();
}

qtractorInstrumentList *qtractorInstrumentForm::instruments (void) const
{
	return m_pInstruments;
}


// Import new intrument file(s) into listing.
void qtractorInstrumentForm::importSlot (void)
{
	if (m_pInstruments == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	QStringList files;

	const QString  sExt("ins");
	const QString& sTitle  = tr("Import Instrument Files") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Instrument files (*.%1 *.sf2 *.midnam)").arg(sExt);
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	files = QFileDialog::getOpenFileNames(this,
		sTitle, pOptions->sInstrumentDir, sFilter, NULL, options);
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
	if (pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		files = fileDialog.selectedFiles();
#endif

	if (files.isEmpty())
		return;

	// Tell that we may take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	
	// For avery selected instrument file to load...
	QTreeWidgetItem *pItem = NULL;
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// Merge the file contents into global container...
		const QString& sPath = iter.next();
		if (m_pInstruments->load(sPath)) {
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
				pItem->setIcon(0, QIcon(":/images/itemFile.png"));
				pItem->setText(0, info.completeBaseName());
				pItem->setText(1, sPath);
				m_ui.FilesListView->setCurrentItem(pItem);
				pOptions->sInstrumentDir = info.absolutePath();
			//	++m_iDirtyCount;
			}
		}
	}

	// May refresh the whole form?
	refreshForm();
	stabilizeForm();

	// Done waiting.
	QApplication::restoreOverrideCursor();
}


// Remove a file from instrument list.
void qtractorInstrumentForm::removeSlot (void)
{
	if (m_pInstruments == NULL)
		return;

	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		delete pItem;
		++m_iDirtyCount;
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
			++m_iDirtyCount;
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
			++m_iDirtyCount;
		}
	}

	stabilizeForm();
}


// Reload the complete instrument definitions, from list.
void qtractorInstrumentForm::reloadSlot (void)
{
	if (m_pInstruments == NULL)
		return;

	// Tell that we may take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Ooops...
	m_pInstruments->clearAll();

	// Load each file in order...
	int iItemCount = m_ui.FilesListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = m_ui.FilesListView->topLevelItem(iItem);
		if (pItem) 
			m_pInstruments->load(pItem->text(1));
	}

	// We're clear.
	m_iDirtyCount = 0;

	refreshForm();
	stabilizeForm();

	// Done with reload.
	QApplication::restoreOverrideCursor();
}


// Export the whole state into a single instrument file.
void qtractorInstrumentForm::exportSlot (void)
{
	if (m_pInstruments == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	QString sPath;

	const QString  sExt("ins");
	const QString& sTitle  = tr("Export Instrument File") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Instrument files (*.%1)").arg(sExt);
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sPath = QFileDialog::getSaveFileName(this,
		sTitle, pOptions->sInstrumentDir, sFilter, NULL, options);
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
	if (pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sPath = fileDialog.selectedFiles().first();
#endif

	if (sPath.isEmpty())
		return;

	// Enforce .ins extension...
	if (QFileInfo(sPath).suffix().isEmpty()) {
		sPath += '.' + sExt;
		// Check if already exists...
		if (QFileInfo(sPath).exists()) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("The instrument file already exists:\n\n"
				"\"%1\"\n\n"
				"Do you want to replace it?")
				.arg(sPath),
				QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
				return;
		}
	}

	// Tell that we may take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Just save the whole bunch...
	if (m_pInstruments->save(sPath))
		pOptions->sInstrumentDir = QFileInfo(sPath).absolutePath();

	// Done with export.
	QApplication::restoreOverrideCursor();
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
			tr("Instrument settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			QMessageBox::Apply |
			QMessageBox::Discard |
			QMessageBox::Cancel)) {
		case QMessageBox::Apply:
			accept();
			return;
		case QMessageBox::Discard:
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
		int iItemCount = m_ui.FilesListView->topLevelItemCount();
		int iItem = m_ui.FilesListView->indexOfTopLevelItem(pItem);
		m_ui.RemovePushButton->setEnabled(true);
		m_ui.MoveUpPushButton->setEnabled(iItem > 0);
		m_ui.MoveDownPushButton->setEnabled(iItem < iItemCount - 1);
	} else {
		m_ui.RemovePushButton->setEnabled(false);
		m_ui.MoveUpPushButton->setEnabled(false);
		m_ui.MoveDownPushButton->setEnabled(false);
	}

	m_ui.ReloadPushButton->setEnabled(m_iDirtyCount > 0);
	m_ui.ExportPushButton->setEnabled(
		m_pInstruments && m_pInstruments->count() > 0);
}


// Refresh all instrument definition views.
void qtractorInstrumentForm::refreshForm (void)
{
	if (m_pInstruments == NULL)
		return;

	// Freeze...
	m_ui.InstrumentsListView->setUpdatesEnabled(false);
	m_ui.FilesListView->setUpdatesEnabled(false);
	m_ui.NamesListView->setUpdatesEnabled(false);

	// Files list view...
	m_ui.FilesListView->clear();
	QList<QTreeWidgetItem *> files;
	QStringListIterator ifile(m_pInstruments->files());
	while (ifile.hasNext()) {
		const QString& sPath = ifile.next();
		QTreeWidgetItem *pFileItem = new QTreeWidgetItem();
		pFileItem->setIcon(0, QIcon(":/images/itemFile.png"));
		pFileItem->setText(0, QFileInfo(sPath).completeBaseName());
		pFileItem->setText(1, sPath);
		files.append(pFileItem);
	}
	m_ui.FilesListView->addTopLevelItems(files);

	// Instruments list view...
	m_ui.InstrumentsListView->clear();
	QList<QTreeWidgetItem *> instrs;
	qtractorInstrumentList::ConstIterator iter
		= m_pInstruments->constBegin();
	const qtractorInstrumentList::ConstIterator& iter_end
		= m_pInstruments->constEnd();
	for ( ; iter != iter_end; ++iter) {
		const qtractorInstrument& instr = iter.value();
		// Instrument Name...
		QTreeWidgetItem *pChildItem = NULL;
		QTreeWidgetItem *pInstrItem = new QTreeWidgetItem();
		pInstrItem->setIcon(0, QIcon(":/images/itemInstrument.png"));
		pInstrItem->setText(0, instr.instrumentName());
		// - Patches Names for Banks...
		pChildItem = new qtractorInstrumentGroupItem(pInstrItem, pChildItem);
		pChildItem->setText(0, tr("Patch Names for Banks"));
		QTreeWidgetItem *pBankItem = NULL;
		const qtractorInstrumentPatches& patches = instr.patches();
		qtractorInstrumentPatches::ConstIterator pat
			= patches.constBegin();
		const qtractorInstrumentPatches::ConstIterator& pat_end
			= patches.constEnd();
		for ( ; pat != pat_end; ++pat) {
			pBankItem = new QTreeWidgetItem(pChildItem, pBankItem);
			int iBank = pat.key();
			const QString sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			pBankItem->setIcon(0, QIcon(":/images/itemPatches.png"));
			pBankItem->setText(0,
				QString("%1 = %2").arg(sBank).arg(pat.value().name()));
			// Patches/Progs...
			const qtractorInstrumentData& patch = instr.patch(iBank);
			QTreeWidgetItem *pProgItem = NULL;
			if (!patch.basedOn().isEmpty()) {
				pProgItem = new QTreeWidgetItem(pBankItem, pProgItem);
				pProgItem->setIcon(0, QIcon(":/images/itemProperty.png"));
				pProgItem->setText(0,
					QString("Based On = %1").arg(patch.basedOn()));
			}
			qtractorInstrumentData::ConstIterator it
				= patch.constBegin();
			const qtractorInstrumentData::ConstIterator& it_end
				= patch.constEnd();
			for ( ; it != it_end; ++it) {
				int iProg = it.key();
				pProgItem = new QTreeWidgetItem(pBankItem, pProgItem);
				pProgItem->setText(0,
					QString("%1 = %2").arg(iProg).arg(it.value()));
				if (instr.isDrum(iBank, iProg))
					listInstrumentData(pProgItem, instr.notes(iBank, iProg));
			}
		}
		// - Controller Names...
		if (instr.controllers().count() > 0) {
			pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
			pChildItem->setIcon(0, QIcon(":/images/itemControllers.png"));
			pChildItem->setText(0,
				tr("Controller Names = %1").arg(instr.controllers().name()));
			listInstrumentData(pChildItem, instr.controllers());
		}
		// - RPN Names...
		if (instr.rpns().count() > 0) {
			pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
			pChildItem->setIcon(0, QIcon(":/images/itemRpns.png"));
			pChildItem->setText(0,
				tr("RPN Names = %1").arg(instr.rpns().name()));
			listInstrumentData(pChildItem, instr.rpns());
		}
		// - NRPN Names...
		if (instr.nrpns().count() > 0) {
			pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
			pChildItem->setIcon(0, QIcon(":/images/itemNrpns.png"));
			pChildItem->setText(0,
				tr("NRPN Names = %1").arg(instr.nrpns().name()));
			listInstrumentData(pChildItem, instr.nrpns());
		}
		// - BankSelMethod...
		pChildItem = new QTreeWidgetItem(pInstrItem, pChildItem);
		pChildItem->setIcon(0, QIcon(":/images/itemProperty.png"));
		pChildItem->setText(0,
			tr("Bank Select Method = %1")
			.arg(bankSelMethod(instr.bankSelMethod())));
		instrs.append(pInstrItem);
	}
	m_ui.InstrumentsListView->addTopLevelItems(instrs);

	// Names list view...
	m_ui.NamesListView->clear();
	QList<QTreeWidgetItem *> names;
	if (m_pInstruments->count() > 0) {
		QTreeWidgetItem *pListItem = NULL;
		// - Patch Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Patch Names"));
		listInstrumentDataList(pListItem, m_pInstruments->patches(),
			QIcon(":/images/itemPatches.png"));
		names.append(pListItem);
		// - Note Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Note Names"));
		listInstrumentDataList(pListItem, m_pInstruments->notes(),
			QIcon(":/images/itemNotes.png"));
		names.append(pListItem);
		// - Controller Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Controller Names"));
		listInstrumentDataList(pListItem, m_pInstruments->controllers(),
			QIcon(":/images/itemControllers.png"));
		names.append(pListItem);
		// - RPN Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("RPN Names"));
		listInstrumentDataList(pListItem, m_pInstruments->rpns(),
			QIcon(":/images/itemRpns.png"));
		names.append(pListItem);
		// - NRPN Names...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("NRPN Names"));
		listInstrumentDataList(pListItem, m_pInstruments->nrpns(),
			QIcon(":/images/itemNrpns.png"));
		names.append(pListItem);
		// - Bank Select Methods...
		pListItem = new qtractorInstrumentGroupItem();
		pListItem->setText(0, tr("Bank Select Methods"));
		if (m_pInstruments->count() > 0) {
			QTreeWidgetItem *pChildItem = NULL;
			for (int iBankSelMethod = 0; iBankSelMethod < 4; ++iBankSelMethod) {
				pChildItem = new qtractorInstrumentGroupItem(pListItem, pChildItem);
				pChildItem->setIcon(0, QIcon(":/images/itemProperty.png"));
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
		pItem->setIcon(0, QIcon(":/images/itemGroup.png"));
}


void qtractorInstrumentForm::itemExpanded ( QTreeWidgetItem *pItem )
{
	if (pItem->type() == GroupItem)
		pItem->setIcon(0, QIcon(":/images/itemGroupOpen.png"));
}


void qtractorInstrumentForm::listInstrumentData (
	QTreeWidgetItem *pParentItem, const qtractorInstrumentData& data )
{
	QTreeWidgetItem *pItem = NULL;
	if (!data.basedOn().isEmpty()) {
		pItem = new QTreeWidgetItem(pParentItem, pItem);
		pItem->setIcon(0, QIcon(":/images/itemProperty.png"));
		pItem->setText(0,
			tr("Based On = %1").arg(data.basedOn()));
	}
	qtractorInstrumentData::ConstIterator it
		= data.constBegin();
	const qtractorInstrumentData::ConstIterator& it_end
		= data.constEnd();
	for ( ; it != it_end; ++it) {
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
	qtractorInstrumentDataList::ConstIterator it
		= list.constBegin();
	const qtractorInstrumentDataList::ConstIterator& it_end
		= list.constEnd();
	for ( ; it != it_end; ++it) {
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
