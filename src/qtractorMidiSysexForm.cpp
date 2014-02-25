// qtractorMidiSysexForm.cpp
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
#include "qtractorMidiSysexForm.h"
#include "qtractorMidiSysex.h"
#include "qtractorMidiEngine.h"
#include "qtractorSession.h"

#include "qtractorMidiFile.h"

#include "qtractorOptions.h"

#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QUrl>

#include <QLineEdit>


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

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	m_pSysexList = NULL;

	m_iDirtyCount  = 0;
	m_iDirtyItem   = 0;
	m_iDirtySysex  = 0;
	m_iUpdateSysex = 0;

	QHeaderView *pHeader = m_ui.SysexListView->header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
#if QT_VERSION >= 0x050000
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(1, QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif

	m_ui.NameComboBox->setInsertPolicy(QComboBox::NoInsert);
	m_ui.NameComboBox->setCompleter(NULL);

	refreshSysex();
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
	QObject::connect(m_ui.NameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(nameChanged(const QString&)));
	QObject::connect(m_ui.NameComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(loadSlot(const QString&)));
	QObject::connect(m_ui.SysexTextEdit,
		SIGNAL(textChanged()),
		SLOT(textChanged()));
	QObject::connect(m_ui.OpenButton,
		SIGNAL(clicked()),
		SLOT(openSlot()));
	QObject::connect(m_ui.SaveButton,
		SIGNAL(clicked()),
		SLOT(saveSlot()));
	QObject::connect(m_ui.DeleteButton,
		SIGNAL(clicked()),
		SLOT(deleteSlot()));
	QObject::connect(m_ui.AddButton,
		SIGNAL(clicked()),
		SLOT(addSlot()));
	QObject::connect(m_ui.UpdateButton,
		SIGNAL(clicked()),
		SLOT(updateSlot()));
	QObject::connect(m_ui.RemoveButton,
		SIGNAL(clicked()),
		SLOT(removeSlot()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(clicked(QAbstractButton *)),
		SLOT(click(QAbstractButton *)));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
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
	filters.append(tr("All files (*.*)"));
	const QString& sTitle  = tr("Import SysEx Files") + " - " QTRACTOR_TITLE;
	const QString& sFilter = filters.join(";;");
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	files = QFileDialog::getOpenFileNames(this,
		sTitle, pOptions->sMidiSysexDir, sFilter, NULL, options);
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
			++m_iDirtyCount;
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
#if 1// QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sPath = QFileDialog::getSaveFileName(this,
		sTitle, pOptions->sMidiSysexDir, sFilter, NULL, options);
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
				tr("The SysEx file already exists:\n\n"
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
			++m_iDirtyCount;
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
			++m_iDirtyCount;
		}
	}

	stabilizeForm();
}


// Load a SysEx item.
void qtractorMidiSysexForm::loadSlot ( const QString& sName )
{
	if (m_iUpdateSysex > 0 || sName.isEmpty())
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	++m_iUpdateSysex;

	QSettings& settings = pOptions->settings();
	settings.beginGroup(sysexGroup());
	loadSysexFile(settings.value(sName).toString());
	settings.endGroup();

	--m_iUpdateSysex;

	refreshSysex();
	stabilizeForm();
}


// Open a SysEx item.
void qtractorMidiSysexForm::openSlot (void)
{
	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// We'll assume that there's an external file...
	QString sFilename;

	// Prompt if file does not currently exist...
	const QString  sExt("syx");
	const QString& sTitle  = tr("Open SysEx") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("SysEx files (*.%1)").arg(sExt);
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to save...
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sFilename = QFileDialog::getOpenFileName(this,
		sTitle, pOptions->sMidiSysexDir, sFilter, NULL, options);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sMidiSysexDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sMidiSysexDir));
	fileDialog.setSidebarUrls(urls);
	if (pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif
	// Have we a filename to load a preset from?
	if (!sFilename.isEmpty()) {
		QFileInfo fi(sFilename);
		if (fi.exists()) {
			// Get it loaded alright...
			++m_iUpdateSysex;
			loadSysexFile(sFilename);
			m_ui.NameComboBox->setEditText(fi.baseName());
			pOptions->sMidiSysexDir = fi.absolutePath();
			++m_iDirtyItem;
			--m_iUpdateSysex;
		}
	}

	refreshSysex();
	stabilizeForm();
}


// Save a SysEx item.
void qtractorMidiSysexForm::saveSlot (void)
{
	const QString& sName = m_ui.NameComboBox->currentText();
	if (sName.isEmpty())
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// The current state preset is about to be saved...
	// this is where we'll make it...
	QSettings& settings = pOptions->settings();
	settings.beginGroup(sysexGroup());
	// Sure, we'll have something complex enough
	// to make it save into an external file...
	const QString sExt("syx");
	QFileInfo fi(QDir(pOptions->sMidiSysexDir), sName + '.' + sExt);
	QString sFilename = fi.absoluteFilePath();
	// Prompt if file does not currently exist...
	if (!fi.exists()) {
		const QString& sTitle  = tr("Save SysEx") + " - " QTRACTOR_TITLE;
		const QString& sFilter = tr("Sysex files (*.%1)").arg(sExt);
	#if 1//QT_VERSION < 0x040400
		// Ask for the filename to save...
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		QFileDialog::Options options = 0;
		if (pOptions->bDontUseNativeDialogs)
			options |= QFileDialog::DontUseNativeDialog;
		sFilename = QFileDialog::getSaveFileName(this,
			sTitle, sFilename, sFilter, NULL, options);
	#else
		// Construct save-file dialog...
		QFileDialog fileDialog(this,
			sTitle, sFilename, sFilter);
		// Set proper open-file modes...
		fileDialog.setAcceptMode(QFileDialog::AcceptSave);
		fileDialog.setFileMode(QFileDialog::AnyFile);
		fileDialog.setDefaultSuffix(sExt);
		// Stuff sidebar...
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		urls.append(QUrl::fromLocalFile(pOptions->sMidiSysexDir));
		fileDialog.setSidebarUrls(urls);
		if (pOptions->bDontUseNativeDialogs)
			fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
		// Show dialog...
		if (fileDialog.exec())
			sFilename = fileDialog.selectedFiles().first();
		else
			sFilename.clear();
	#endif
	} else if (pOptions->bConfirmRemove) {
		if (QMessageBox::warning(parentWidget(),
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to replace SysEx:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(sName),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel) {
			sFilename.clear();
		}
	}
	// We've a filename to save the preset
	if (!sFilename.isEmpty()) {
		if (QFileInfo(sFilename).suffix().isEmpty())
			sFilename += '.' + sExt;
		// Get it saved alright...
		++m_iUpdateSysex;
		saveSysexFile(sFilename);
		settings.setValue(sName, sFilename);
		--m_iUpdateSysex;
		pOptions->sMidiSysexDir = QFileInfo(sFilename).absolutePath();
	}
	settings.endGroup();

	refreshSysex();
	stabilizeForm();
}


// Delete a SysEx item.
void qtractorMidiSysexForm::deleteSlot (void)
{
	const QString& sName = m_ui.NameComboBox->currentText();
	if (sName.isEmpty())
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// A preset entry is about to be removed;
	// prompt user if he/she's sure about this...
	if (pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to delete SysEx:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(sName),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel)
			return;
	}

	// Go ahead...
	++m_iUpdateSysex;

	QSettings& settings = pOptions->settings();
	settings.beginGroup(sysexGroup());
#ifdef QTRACTOR_REMOVE_PRESET_FILES
	const QString& sFilename = settings.value(sName).toString();
	if (QFileInfo(sFilename).exists())
		QFile(sFilename).remove();
#endif
	settings.remove(sName);
	settings.endGroup();

	--m_iUpdateSysex;

	refreshSysex();
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
			m_ui.NameComboBox->currentText(),
			m_ui.SysexTextEdit->toPlainText())
	);

	m_ui.SysexListView->setCurrentItem(pItem);

	++m_iDirtyCount;
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
	pSysex->setName(m_ui.NameComboBox->currentText());
	pSysex->setText(m_ui.SysexTextEdit->toPlainText());
	pSysexItem->update();

	m_ui.SysexListView->setCurrentItem(pItem);

	++m_iDirtyCount;
	m_iDirtyItem = 0;

	stabilizeForm();
}


// Remove a SysEx item from list.
void qtractorMidiSysexForm::removeSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.SysexListView->currentItem();
	if (pItem) {
		delete pItem;
		++m_iDirtyCount;
		m_iDirtyItem = 0;
	}

	stabilizeForm();
}


// Clear all SysEx items from list.
void qtractorMidiSysexForm::clearSlot (void)
{
	++m_iUpdateSysex;

	m_ui.SysexListView->clear();
	m_ui.NameComboBox->lineEdit()->clear();
	m_ui.SysexTextEdit->clear();

	--m_iUpdateSysex;

	++m_iDirtyCount;
	m_iDirtyItem = 0;

	stabilizeForm();
}


// SysEx item name change.
void qtractorMidiSysexForm::nameChanged ( const QString& sName )
{
	if (m_iUpdateSysex > 0)
		return;

	if (!sName.isEmpty() && m_ui.NameComboBox->findText(sName) >= 0)
		++m_iDirtySysex;

	++m_iDirtyItem;
	stabilizeForm();
}


void qtractorMidiSysexForm::textChanged (void)
{
	if (m_iUpdateSysex > 0)
		return;

	++m_iDirtySysex;

	++m_iDirtyItem;
	stabilizeForm();
}


// Reset settings (action button slot).
void qtractorMidiSysexForm::click ( QAbstractButton *pButton )
{
	QDialogButtonBox::ButtonRole role
		= m_ui.DialogButtonBox->buttonRole(pButton);
	if ((role & QDialogButtonBox::ResetRole) == QDialogButtonBox::ResetRole)
		clearSlot();
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

	// AG: Reset the controllers so that changes take effect immediately...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && pSession->midiEngine())
		pSession->midiEngine()->resetAllControllers(true);

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorMidiSysexForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("SysEx settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
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
			++m_iUpdateSysex;
			qtractorMidiSysex *pSysex
				= static_cast<qtractorMidiSysexItem *> (pItem)->sysex();
			m_ui.NameComboBox->setEditText(pSysex->name());
			m_ui.SysexTextEdit->setText(pSysex->text());
			--m_iUpdateSysex;
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

	const QString& sName = m_ui.NameComboBox->currentText();
	bool bEnabled = (!sName.isEmpty());
	bool bExists  = (m_ui.NameComboBox->findText(sName) >= 0);
	if (bEnabled && m_iDirtyItem > 0) {
		qtractorMidiSysex sysex(sName, m_ui.SysexTextEdit->toPlainText());
		bEnabled = (sysex.size() > 0);
	}
	m_ui.SaveButton->setEnabled(bEnabled && (!bExists || m_iDirtySysex > 0));
	m_ui.DeleteButton->setEnabled(bEnabled && bExists);
	m_ui.AddButton->setEnabled(bEnabled);
	m_ui.UpdateButton->setEnabled(bEnabled && pItem != NULL);

	m_ui.DialogButtonBox->button(QDialogButtonBox::Reset)->setEnabled(
		bEnabled || iItemCount > 0);
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(
		m_iDirtyCount > 0 && m_iDirtyItem == 0);
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


// Refresh SysEx names (presets).
void qtractorMidiSysexForm::refreshSysex (void)
{
	if (m_iUpdateSysex > 0)
		return;

	++m_iUpdateSysex;

	const QString sOldName = m_ui.NameComboBox->currentText();
	m_ui.NameComboBox->clear();
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->settings().beginGroup(sysexGroup());
		m_ui.NameComboBox->insertItems(0, pOptions->settings().childKeys());
		pOptions->settings().endGroup();
	}
	m_ui.NameComboBox->setEditText(sOldName);

	m_iDirtySysex = 0;
	--m_iUpdateSysex;
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
		unsigned short iTracks = midifile.tracks();
		for (unsigned int iTrack = 0; iTrack < iTracks; ++iTrack) {
			qtractorMidiSequence seq;
			if (midifile.readTrack(&seq, iTrack)) {
				qtractorMidiEvent *pEvent = seq.events().first();
				while (pEvent) {
					if (pEvent->type() == qtractorMidiEvent::SYSEX) {
						items.append(new qtractorMidiSysexItem(
							new qtractorMidiSysex(
								info.baseName()
								+ '-' + QString::number(iTrack)
								+ '.' + QString::number(++iSysex),
								pEvent->sysex(),
								pEvent->sysex_len())));
					}
					pEvent = pEvent->next();
				}
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


// Preset file handlers.
void qtractorMidiSysexForm::loadSysexFile ( const QString& sFilename )
{
	// Open the source file...
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	bool bResult = false;

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
				QFileInfo info(sFilename);
				qtractorMidiSysex sysex(info.baseName(), pBuff, i);
				m_ui.NameComboBox->setEditText(sysex.name());
				m_ui.SysexTextEdit->setText(sysex.text());
				bResult = true;
				break;
			}
		}
	}

	// Cleanup...
	if (pBuff)
		delete [] pBuff;	

	file.close();

	// We're formerly done.
	QApplication::restoreOverrideCursor();
	
	// Any late warning?
	if (!bResult) {
		// Failure (maybe wrong preset)...
		QMessageBox::critical(this,
			tr("Error") + " - " QTRACTOR_TITLE,
			tr("SysEx could not be loaded:\n\n"
			"\"%1\".\n\n"
			"Sorry.").arg(sFilename),
			QMessageBox::Cancel);		
	}
}


void qtractorMidiSysexForm::saveSysexFile ( const QString& sFilename )
{
	// Open the target file...
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
		return;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Parse the data...
	qtractorMidiSysex sysex(
		m_ui.NameComboBox->currentText(),
		m_ui.SysexTextEdit->toPlainText());

	file.write((const char *) sysex.data(), sysex.size());
	file.close();

	// We're formerly done.
	QApplication::restoreOverrideCursor();
}


// Preset group path name.
QString qtractorMidiSysexForm::sysexGroup (void)
{
	return "/Midi/Sysex/";
}


// end of qtractorMidiSysexForm.cpp
