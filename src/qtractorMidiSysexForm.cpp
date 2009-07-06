// qtractorMidiSysexForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiSysexForm.h"
#include "qtractorMidiSysex.h"

#include "qtractorMidiFile.h"

#include "qtractorOptions.h"

#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QUrl>


//----------------------------------------------------------------------
// class qtractorMidiSysexItem -- custom list view item.
//

class qtractorMidiSysexItem : public QTreeWidgetItem
{
public:

	// Constructors.
	qtractorMidiSysexItem(qtractorMidiSysex *pSysex)
		: QTreeWidgetItem(), m_pSysex(pSysex)
		{ update(); }
	qtractorMidiSysexItem(QTreeWidget *pTreeWidget,
		QTreeWidgetItem *pItem, qtractorMidiSysex *pSysex)
		: QTreeWidgetItem(pTreeWidget, pItem), m_pSysex(pSysex)
		{ update(); }

	// Destructor.
	~qtractorMidiSysexItem()
		{ delete m_pSysex; }

	// Accessors.
	void setSysex(qtractorMidiSysex *pSysex)
		{ m_pSysex = pSysex; update(); }
	qtractorMidiSysex *sysex() const
		{ return m_pSysex; }

	// Updator.
	void update()
	{
		QTreeWidgetItem::setText(0, m_pSysex->name());
		QTreeWidgetItem::setText(1, QString::number(m_pSysex->size()));
		QTreeWidgetItem::setText(2, m_pSysex->text());
	}

private:

	// Instance variable.
	qtractorMidiSysex *m_pSysex;
};


//----------------------------------------------------------------------
// class qtractorMidiSysexForm -- instrument file manager form.
//

// Constructor.
qtractorMidiSysexForm::qtractorMidiSysexForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pSysexList = NULL;

	m_iDirtyCount = 0;
	m_iDirtyItem  = 0;

	QHeaderView *pHeader = m_ui.SysexListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(1, QHeaderView::ResizeToContents);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	refreshForm();
	stabilizeForm();

//	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.SysexListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ImportButton,
		SIGNAL(clicked()),
		SLOT(importSlot()));
	QObject::connect(m_ui.ExportButton,
		SIGNAL(clicked()),
		SLOT(exportSlot()));
	QObject::connect(m_ui.MoveUpButton,
		SIGNAL(clicked()),
		SLOT(moveUpSlot()));
	QObject::connect(m_ui.MoveDownButton,
		SIGNAL(clicked()),
		SLOT(moveDownSlot()));
	QObject::connect(m_ui.SysexNameEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(textChanged()));
	QObject::connect(m_ui.SysexTextEdit,
		SIGNAL(textChanged()),
		SLOT(textChanged()));
	QObject::connect(m_ui.AddButton,
		SIGNAL(clicked()),
		SLOT(addSlot()));
	QObject::connect(m_ui.UpdateButton,
		SIGNAL(clicked()),
		SLOT(updateSlot()));
	QObject::connect(m_ui.RemoveButton,
		SIGNAL(clicked()),
		SLOT(removeSlot()));
	QObject::connect(m_ui.ClearButton,
		SIGNAL(clicked()),
		SLOT(clearSlot()));
	QObject::connect(m_ui.OkButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qtractorMidiSysexForm::~qtractorMidiSysexForm (void)
{
}


// SysEx list accessors.
void qtractorMidiSysexForm::setSysexList ( qtractorMidiSysexList *pSysexList )
{
	m_pSysexList = pSysexList;

	refreshForm();
	stabilizeForm();
}

qtractorMidiSysexList *qtractorMidiSysexForm::sysexList (void) const
{
	return m_pSysexList;
}


// Import new SysEx into listing.
void qtractorMidiSysexForm::importSlot (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	QStringList files;

	const QString  sExt("syx");
	QStringList filters;
	filters.append(tr("SysEx files (*.%1)").arg(sExt));
	filters.append(tr("MIDI files (*.mid *.smf *.midi)"));
	const QString& sTitle  = tr("Import SysEx Files") + " - " QTRACTOR_TITLE;
	const QString& sFilter = filters.join(";;");
#if QT_VERSION < 0x040400
	// Ask for the filename to open...
	files = QFileDialog::getOpenFileNames(this,
		sTitle, pOptions->sMidiSysexDir, sFilter);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sMidiSysexDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sMidiSysexDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (fileDialog.exec())
		files = fileDialog.selectedFiles();
#endif

	if (files.isEmpty())
		return;

	// Tell that we may take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	m_ui.SysexListView->setUpdatesEnabled(false);

	// Start inserting in the current selected or last item...
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem == NULL) {
		int iLastItem = m_ui.SysexListView->topLevelItemCount() - 1;
		if (iLastItem >= 0)
			pItem = m_ui.SysexListView->topLevelItem(iLastItem);
	}

	// For avery selected SysEx file to load...
	QList<QTreeWidgetItem *> items;
	QStringListIterator ifile(files);
	while (ifile.hasNext()) {
		QApplication::processEvents();
		// Merge the file contents into global container...
		const QString& sPath = ifile.next();
		if (loadSysexItems(items, sPath)) {
			pOptions->sMidiSysexDir = QFileInfo(sPath).absolutePath();
			m_iDirtyCount++;
		}
	}
	m_ui.SysexListView->addTopLevelItems(items);

	// Done waiting.
	m_ui.SysexListView->setUpdatesEnabled(true);
	QApplication::restoreOverrideCursor();

	stabilizeForm();
}


// Export the whole state into a single SysEx file.
void qtractorMidiSysexForm::exportSlot (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	QString sPath;

	const QString  sExt("syx");
	const QString& sTitle  = tr("Export SysEx File") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("SysEx files (*.%1)").arg(sExt);
#if QT_VERSION < 0x040400
	// Ask for the filename to open...
	sPath = QFileDialog::getSaveFileName(this,
		sTitle, pOptions->sMidiSysexDir, sFilter);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sMidiSysexDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sMidiSysexDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (fileDialog.exec())
		sPath = fileDialog.selectedFiles().first();
#endif

	if (sPath.isEmpty())
		return;

	// Enforce .ins extension...
	if (QFileInfo(sPath).suffix() != sExt) {
		sPath += '.' + sExt;
		// Check if already exists...
		if (QFileInfo(sPath).exists()) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("The SysEx file already exists:\n\n"
				"\"%1\"\n\n"
				"Do you want to replace it?")
				.arg(sPath),
				QMessageBox::Yes | QMessageBox::No) == QMessageBox::No)
				return;
		}
	}

	// Tell that we may take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Just save the whole bunch...
	QList<QTreeWidgetItem *> items;
	int iItemCount = m_ui.SysexListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem)
		items.append(m_ui.SysexListView->topLevelItem(iItem));
	QApplication::processEvents();
	if (saveSysexItems(items, sPath))
		pOptions->sMidiSysexDir = QFileInfo(sPath).absolutePath();

	// Done with export.
	QApplication::restoreOverrideCursor();
}


// Move a SysEx item up on the list.
void qtractorMidiSysexForm::moveUpSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem) {
		int iItem = m_ui.SysexListView->indexOfTopLevelItem(pItem);
		if (iItem > 0) {
			pItem = m_ui.SysexListView->takeTopLevelItem(iItem);
			m_ui.SysexListView->insertTopLevelItem(iItem - 1, pItem);
			m_ui.SysexListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	stabilizeForm();
}


// Move a SysEx item down on the list.
void qtractorMidiSysexForm::moveDownSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem) {
		int iItem = m_ui.SysexListView->indexOfTopLevelItem(pItem);
		if (iItem < m_ui.SysexListView->topLevelItemCount() - 1) {
			pItem = m_ui.SysexListView->takeTopLevelItem(iItem);
			m_ui.SysexListView->insertTopLevelItem(iItem + 1, pItem);
			m_ui.SysexListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	stabilizeForm();
}


// Add a SysEx item to list.
void qtractorMidiSysexForm::addSlot (void)
{
	// Start inserting in the current selected or last item...
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem == NULL) {
		int iLastItem = m_ui.SysexListView->topLevelItemCount() - 1;
		if (iLastItem >= 0)
			pItem = m_ui.SysexListView->topLevelItem(iLastItem);
	}

	// Add item...
	pItem = new qtractorMidiSysexItem(
		m_ui.SysexListView, pItem,
		new qtractorMidiSysex(
			m_ui.SysexNameEdit->text(),
			m_ui.SysexTextEdit->toPlainText())
	);

	m_ui.SysexListView->setCurrentItem(pItem);

	m_iDirtyCount++;
	m_iDirtyItem = 0;

	stabilizeForm();
}


// Update a SysEx item from list.
void qtractorMidiSysexForm::updateSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem == NULL)
		return;

	// Update item...
	qtractorMidiSysexItem *pSysexItem
		= static_cast<qtractorMidiSysexItem *> (pItem);
	qtractorMidiSysex *pSysex = pSysexItem->sysex();
	pSysex->setName(m_ui.SysexNameEdit->text());
	pSysex->setText(m_ui.SysexTextEdit->toPlainText());
	pSysexItem->update();

	m_ui.SysexListView->setCurrentItem(pItem);

	m_iDirtyCount++;
	m_iDirtyItem = 0;

	stabilizeForm();
}


// Remove a SysEx item from list.
void qtractorMidiSysexForm::removeSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem) {
		delete pItem;
		m_iDirtyCount++;
		m_iDirtyItem = 0;
	}

	stabilizeForm();
}


// Clear all SysEx items from list.
void qtractorMidiSysexForm::clearSlot (void)
{
	m_ui.SysexListView->clear();
	m_ui.SysexNameEdit->clear();
	m_ui.SysexTextEdit->clear();

	m_iDirtyCount++;
	m_iDirtyItem = 0;

	stabilizeForm();
}


// SysEx item name change.
void qtractorMidiSysexForm::textChanged (void)
{
	m_iDirtyItem++;
	stabilizeForm();
}


// Accept settings (OK button slot).
void qtractorMidiSysexForm::accept (void)
{
	if (m_iDirtyCount == 0)
		return;
	if (m_pSysexList == NULL)
		return;

	// Just reload the whole bunch...
	m_pSysexList->clear();
	int iItemCount = m_ui.SysexListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		qtractorMidiSysexItem *pSysexItem
			= static_cast<qtractorMidiSysexItem *> (
				m_ui.SysexListView->topLevelItem(iItem));
		if (pSysexItem) {
			m_pSysexList->append(
				new qtractorMidiSysex(*(pSysexItem->sysex())));
		}
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorMidiSysexForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("SysEx settings have been changed.") + "\n\n" +
			tr("Do you want to apply the changes?"),
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
void qtractorMidiSysexForm::stabilizeForm (void)
{
	int iItemCount = m_ui.SysexListView->topLevelItemCount();
	m_ui.ExportButton->setEnabled(iItemCount > 0);

	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem) {
		if (m_iDirtyItem == 0) {
			qtractorMidiSysex *pSysex
				= static_cast<qtractorMidiSysexItem *> (pItem)->sysex();
			m_ui.SysexNameEdit->setText(pSysex->name());
			m_ui.SysexTextEdit->setText(pSysex->text());
			m_iDirtyItem = 0;
		}
		int iItem = m_ui.SysexListView->indexOfTopLevelItem(pItem);
		m_ui.MoveUpButton->setEnabled(iItem > 0);
		m_ui.MoveDownButton->setEnabled(iItem < iItemCount - 1);
		m_ui.RemoveButton->setEnabled(true);
	} else {
		m_ui.MoveUpButton->setEnabled(false);
		m_ui.MoveDownButton->setEnabled(false);
		m_ui.RemoveButton->setEnabled(false);
	}

	bool bEnabled = false;
	if (m_iDirtyItem > 0) {
		qtractorMidiSysex sysex(
			m_ui.SysexNameEdit->text(),
			m_ui.SysexTextEdit->toPlainText());
		bEnabled = (!sysex.name().isEmpty() && sysex.size() > 0);
	}
	m_ui.AddButton->setEnabled(bEnabled);
	m_ui.UpdateButton->setEnabled(bEnabled && pItem != NULL);

	m_ui.ClearButton->setEnabled(iItemCount > 0);
	m_ui.OkButton->setEnabled(m_iDirtyCount > 0 && m_iDirtyItem == 0);
}


// Refresh all SysEx definition views.
void qtractorMidiSysexForm::refreshForm (void)
{
	if (m_pSysexList == NULL)
		return;

	// Freeze...
	m_ui.SysexListView->setUpdatesEnabled(false);

	// Files list view...
	m_ui.SysexListView->clear();
	qtractorMidiSysexItem *pItem = NULL;
	QListIterator<qtractorMidiSysex *> iter(*m_pSysexList);
	while (iter.hasNext()) {
		pItem = new qtractorMidiSysexItem(
			m_ui.SysexListView, pItem,
			new qtractorMidiSysex(*iter.next())
		);
	}

	// Bail out...
	m_ui.SysexListView->setUpdatesEnabled(true);

	// Clean.
	m_iDirtyCount = 0;
	m_iDirtyItem = 0;
}


// SysEx file i/o methods.
bool qtractorMidiSysexForm::loadSysexItems (
	QList<QTreeWidgetItem *>& items, const QString& sFilename )
{
	int iSysex = 0;
	QFileInfo info(sFilename);

	// Try on SMF files first...
	qtractorMidiFile midifile;
	if (midifile.open(sFilename)) {
		qtractorMidiSequence seq;
		if (midifile.readTrack(&seq, 0)) {
			qtractorMidiEvent *pEvent = seq.events().first();
			while (pEvent) {
				if (pEvent->type() == qtractorMidiEvent::SYSEX) {
					items.append(new qtractorMidiSysexItem(
						new qtractorMidiSysex(
							info.baseName()
							+ '-' + QString::number(++iSysex),
							pEvent->sysex(),
							pEvent->sysex_len())));
				}
				pEvent = pEvent->next();
			}
		}
		midifile.close();
		return (iSysex > 0);
	}

	// Should be a SysEx file then...
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	unsigned short iBuff = 0;
	unsigned char *pBuff = NULL;
	unsigned short i = 0;

	// Read the file....
	while (!file.atEnd()) {
		// (Re)allocate buffer...
		if (i >= iBuff) {
			unsigned char *pTemp = pBuff;
			iBuff += 1024;
			pBuff  = new unsigned char [iBuff];
			if (pTemp) {
				::memcpy(pBuff, pTemp, i);
				delete [] pTemp;
			}
		}
		// Read the next chunk...
		unsigned short iRead = file.read((char *) pBuff + i, iBuff - i) + i;
		while (i < iRead) {
			if (pBuff[i++] == 0xf7) {
				QString sName = info.baseName();
				if (++iSysex > 1 || i < iRead) {
					sName += '-';
					sName += QString::number(iSysex);
				}
				items.append(new qtractorMidiSysexItem(
					new qtractorMidiSysex(sName, pBuff, i)));
				if (i < iRead) {
					::memmove(pBuff, pBuff + i, iRead -= i);
					i = 0;
				}
			}
		}
	}

	// Cleanup...
	if (pBuff)
		delete [] pBuff;	
	file.close();

	file.close();
	return (iSysex > 0);
}


bool qtractorMidiSysexForm::saveSysexItems (
	const QList<QTreeWidgetItem *>& items, const QString& sFilename ) const
{
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
		return false;

	QListIterator<QTreeWidgetItem *> iter(items);
	while (iter.hasNext()) {
		qtractorMidiSysexItem *pSysexItem
			= static_cast<qtractorMidiSysexItem *> (iter.next());
		if (pSysexItem) {
			qtractorMidiSysex *pSysex = pSysexItem->sysex();
			file.write((const char *) pSysex->data(), pSysex->size());
		}
	}

	file.close();
	return true;
}


// end of qtractorMidiSysexForm.cpp
