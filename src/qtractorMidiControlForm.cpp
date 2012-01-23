// qtractorMidiControlForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiControlForm.h"

#include "qtractorOptions.h"

// Needed for controller names.
#include "qtractorMidiEditor.h"

#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QUrl>


//----------------------------------------------------------------------
// class qtractorMidiControlForm -- MIDI controller file manager form.
//

// Constructor.
qtractorMidiControlForm::qtractorMidiControlForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	m_iDirtyCount = 0;
	m_iDirtyMap = 0;
	m_iUpdating = 0;

	QHeaderView *pHeader = m_ui.FilesListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	pHeader = m_ui.ControlMapListView->header();
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

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
	m_ui.ControlTypeComboBox->setCurrentIndex(3); // Controller.

//	m_ui.ChannelComboBox->clear();
	m_ui.ChannelComboBox->addItem("*");
	for (unsigned short iChannel = 0; iChannel < 16; ++iChannel)
		m_ui.ChannelComboBox->addItem(textFromChannel(iChannel));

	const QIcon iconCommand(":/images/itemChannel.png");
//	m_ui.CommandComboBox->clear();
	m_ui.CommandComboBox->addItem(iconCommand,
		qtractorMidiControl::nameFromCommand(qtractorMidiControl::TRACK_GAIN));
	m_ui.CommandComboBox->addItem(iconCommand,
		qtractorMidiControl::nameFromCommand(qtractorMidiControl::TRACK_PANNING));
	m_ui.CommandComboBox->addItem(iconCommand,
		qtractorMidiControl::nameFromCommand(qtractorMidiControl::TRACK_MONITOR));
	m_ui.CommandComboBox->addItem(iconCommand,
		qtractorMidiControl::nameFromCommand(qtractorMidiControl::TRACK_RECORD));
	m_ui.CommandComboBox->addItem(iconCommand,
		qtractorMidiControl::nameFromCommand(qtractorMidiControl::TRACK_MUTE));
	m_ui.CommandComboBox->addItem(iconCommand,
		qtractorMidiControl::nameFromCommand(qtractorMidiControl::TRACK_SOLO));

	stabilizeTypeChange();

	refreshFiles();
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.FilesListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ControlMapListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeForm()));
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
	QObject::connect(m_ui.ControlTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(typeChangedSlot()));
	QObject::connect(m_ui.ChannelComboBox,
		SIGNAL(activated(int)),
		SLOT(keyChangedSlot()));
	QObject::connect(m_ui.ParamComboBox,
		SIGNAL(activated(int)),
		SLOT(keyChangedSlot()));
	QObject::connect(m_ui.TrackCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(keyChangedSlot()));
	QObject::connect(m_ui.TrackSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(valueChangedSlot()));
	QObject::connect(m_ui.CommandComboBox,
		SIGNAL(activated(int)),
		SLOT(valueChangedSlot()));
	QObject::connect(m_ui.FeedbackCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(valueChangedSlot()));
	QObject::connect(m_ui.MapPushButton,
		SIGNAL(clicked()),
		SLOT(mapSlot()));
	QObject::connect(m_ui.UnmapPushButton,
		SIGNAL(clicked()),
		SLOT(unmapSlot()));
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
qtractorMidiControlForm::~qtractorMidiControlForm (void)
{
}


// Reject settings (Cancel button slot).
void qtractorMidiControlForm::reject (void)
{
	// Check if there's any pending changes...
	if (m_iDirtyMap > 0)
		reloadSlot();

	if (m_iDirtyMap == 0)
		QDialog::reject();
}


// Import new intrument file(s) into listing.
void qtractorMidiControlForm::importSlot (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	QStringList files;

	const QString  sExt("qtc");
	const QString& sTitle  = tr("Import Controller Files") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Controller files (*.%1)").arg(sExt);
#if QT_VERSION < 0x040400
	// Ask for the filename to open...
	files = QFileDialog::getOpenFileNames(this,
		sTitle, pOptions->sMidiControlDir, sFilter);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sMidiControlDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sMidiControlDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (fileDialog.exec())
		files = fileDialog.selectedFiles();
#endif

	if (files.isEmpty())
		return;

	// Remember this last directory...
	
	// For avery selected controller file to load...
	QTreeWidgetItem *pItem = NULL;
	QStringListIterator iter(files);
	while (iter.hasNext()) {
		// Merge the file contents into global container...
		const QString& sPath = iter.next();
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
			pOptions->sMidiControlDir = info.absolutePath();
		}
	}

	// Make effect immediately.
	reloadSlot();
}


// Remove a file from controller list.
void qtractorMidiControlForm::removeSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem == NULL)
		return;

	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bConfirmRemove) {
		// Show the warning...
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove controller file:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(pItem->text(1)),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel)
			return;
	}

	// Just do it!
	delete pItem;

	// Effect immediate.
	reloadSlot();
}


// Move a file up on the controller list.
void qtractorMidiControlForm::moveUpSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		int iItem = m_ui.FilesListView->indexOfTopLevelItem(pItem);
		if (iItem > 0) {
			pItem = m_ui.FilesListView->takeTopLevelItem(iItem);
			m_ui.FilesListView->insertTopLevelItem(iItem - 1, pItem);
			m_ui.FilesListView->setCurrentItem(pItem);
		}
	}

	reloadSlot();
}


// Move a file down on the controller list.
void qtractorMidiControlForm::moveDownSlot (void)
{
	QTreeWidgetItem *pItem = m_ui.FilesListView->currentItem();
	if (pItem) {
		int iItem = m_ui.FilesListView->indexOfTopLevelItem(pItem);
		if (iItem < m_ui.FilesListView->topLevelItemCount() - 1) {
			pItem = m_ui.FilesListView->takeTopLevelItem(iItem);
			m_ui.FilesListView->insertTopLevelItem(iItem + 1, pItem);
			m_ui.FilesListView->setCurrentItem(pItem);
		}
	}

	reloadSlot();
}


// Map current channel/control command.
void qtractorMidiControlForm::mapSlot (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(
			m_ui.ControlTypeComboBox->currentText());
	unsigned short iChannel = channelFromText(
		m_ui.ChannelComboBox->currentText());
	unsigned short iParam = paramFromText(ctype,
		m_ui.ParamComboBox->currentText());
	if (m_ui.TrackCheckBox->isChecked()
		&& (iChannel & qtractorMidiControl::TrackParam) == 0)
		iParam |= qtractorMidiControl::TrackParam;

	qtractorMidiControl::Command command
		= qtractorMidiControl::commandFromName(
			m_ui.CommandComboBox->currentText());
	int iTrack = m_ui.TrackSpinBox->value();
	bool bFeedback = m_ui.FeedbackCheckBox->isChecked();

	pMidiControl->mapChannelParam(
		ctype, iChannel, iParam, command, iTrack, bFeedback);

	m_iDirtyCount = 0;
	++m_iDirtyMap;

	refreshControlMap();
}


// Unmap current channel/control command.
void qtractorMidiControlForm::unmapSlot (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(
			m_ui.ControlTypeComboBox->currentText());
	unsigned short iChannel = channelFromText(
		m_ui.ChannelComboBox->currentText());
	unsigned short iParam = paramFromText(ctype,
		m_ui.ParamComboBox->currentText());
	if (m_ui.TrackCheckBox->isChecked()
		&& (iChannel & qtractorMidiControl::TrackParam) == 0)
		iParam |= qtractorMidiControl::TrackParam;

	pMidiControl->unmapChannelParam(ctype, iChannel, iParam);

	m_iDirtyCount = 0;
	++m_iDirtyMap;

	refreshControlMap();
}


// Export the whole state into a single controller file.
void qtractorMidiControlForm::exportSlot (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	QString sPath;

	const QString  sExt("qtc");
	const QString& sTitle  = tr("Export Controller File") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Controller files (*.%1)").arg(sExt);
#if 1 // QT_VERSION < 0x040400
	// Ask for the filename to open...
	sPath = QFileDialog::getSaveFileName(this,
		sTitle, pOptions->sMidiControlDir, sFilter);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, pOptions->sMidiControlDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sMidiControlDir));
	fileDialog.setSidebarUrls(urls);
	// Show dialog...
	if (fileDialog.exec())
		sPath = fileDialog.selectedFiles().first();
#endif

	if (sPath.isEmpty())
		return;

	// Enforce .qtc extension...
	if (QFileInfo(sPath).suffix().isEmpty()) {
		sPath += '.' + sExt;
		// Check if already exists...
		if (QFileInfo(sPath).exists()) {
			if (QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("The controller file already exists:\n\n"
				"\"%1\"\n\n"
				"Do you want to replace it?")
				.arg(sPath),
				QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
				return;
		}
	}

	// Just save the whole bunch...
	if (pMidiControl->saveDocument(sPath)) {
		pOptions->sMidiControlDir = QFileInfo(sPath).absolutePath();
		if (m_iDirtyMap > 0 &&
			QMessageBox::warning(this,
				tr("Warning") + " - " QTRACTOR_TITLE,
				tr("Saved controller mappings may not be effective\n"
				"the next time you start this program.\n\n"
				"\"%1\"\n\n"
				"Do you want to apply to controller files?")
				.arg(sPath),
				QMessageBox::Apply |
				QMessageBox::Ignore) == QMessageBox::Apply) {
			// Apply by append...
			pOptions->midiControlFiles.clear();
			pOptions->midiControlFiles.append(sPath);
			// Won't be dirty anymore.
			m_iDirtyMap = 0;
			// Make it renew...
			refreshFiles();
			reloadSlot();
		}
	}
}


// Reload the complete controller configurations, from list.
void qtractorMidiControlForm::reloadSlot (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	// Check if there's any pending map changes...
	if (m_iDirtyMap > 0 && !pMidiControl->controlMap().isEmpty() &&
		QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Controller mappings have been changed.") + "\n\n" +
			tr("Do you want to save the changes?"),
			QMessageBox::Save |
			QMessageBox::Discard) == QMessageBox::Save) {
			// Export is save... :)
			exportSlot();
			return;
	}

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// Ooops...
	pMidiControl->clear();
	pOptions->midiControlFiles.clear();

	// Load each file in order...
	int iItemCount = m_ui.FilesListView->topLevelItemCount();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QTreeWidgetItem *pItem = m_ui.FilesListView->topLevelItem(iItem);
		if (pItem) {
			const QString& sPath = pItem->text(1);
			if (pMidiControl->loadDocument(sPath))
				pOptions->midiControlFiles.append(sPath);
		}
	}

	// Not dirty anymore...
	m_iDirtyCount = 0;
	m_iDirtyMap = 0;

	refreshControlMap();
}


// Mapping type field have changed..
void qtractorMidiControlForm::typeChangedSlot (void)
{
	if (m_iUpdating > 0)
		return;

	++m_iDirtyCount;

	stabilizeTypeChange();
	stabilizeKeyChange();
}

void qtractorMidiControlForm::stabilizeTypeChange (void)
{
	m_ui.ParamComboBox->clear();
//	m_ui.ParamComboBox->addItem("*");
	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(
			m_ui.ControlTypeComboBox->currentText());
	const QIcon iconParam(":/images/itemControllers.png");
	for (unsigned short iParam = 0; iParam < 128; ++iParam)
		m_ui.ParamComboBox->addItem(iconParam, textFromParam(ctype, iParam));
}


// Mapping key fields have changed..
void qtractorMidiControlForm::keyChangedSlot (void)
{
	if (m_iUpdating > 0)
		return;

	++m_iDirtyCount;

	stabilizeKeyChange();
}

void qtractorMidiControlForm::stabilizeKeyChange (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	const QString& sType    = m_ui.ControlTypeComboBox->currentText();
	const QString& sChannel = m_ui.ChannelComboBox->currentText();
	const QString& sParam   = m_ui.ParamComboBox->currentText();

	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(sType);
	unsigned short iChannel = channelFromText(sChannel);
	unsigned short iParam = paramFromText(ctype, sParam);
	if (m_ui.TrackCheckBox->isChecked()
		&& (iChannel & qtractorMidiControl::TrackParam) == 0)
		iParam |= qtractorMidiControl::TrackParam;

	bool bMapped = pMidiControl->isChannelParamMapped(ctype, iChannel, iParam);

	if (bMapped) {
		QList<QTreeWidgetItem *> items
			=  m_ui.ControlMapListView->findItems(sType, Qt::MatchExactly, 0);
		QListIterator<QTreeWidgetItem *> iter(items);
		while (iter.hasNext()) {
			QTreeWidgetItem *pItem = iter.next();
			if ((iParam & qtractorMidiControl::TrackParam) == 0
				&& pItem->text(3)[0] == '+')
				continue;
			if (pItem->text(1) == sChannel && pItem->text(2) == sParam) {
				++m_iUpdating;
				m_ui.ControlMapListView->setCurrentItem(pItem);
				--m_iUpdating;
				break;
			}
		}
	}

	m_ui.TrackCheckBox->setEnabled(
		(iChannel & qtractorMidiControl::TrackParam) == 0);

	m_ui.MapPushButton->setEnabled(!bMapped && m_iDirtyCount > 0);
	m_ui.UnmapPushButton->setEnabled(bMapped);
}


// Mapping value fields have changed..
void qtractorMidiControlForm::valueChangedSlot (void)
{
	if (m_iUpdating > 0)
		return;

	++m_iDirtyCount;

	stabilizeValueChange();
}

void qtractorMidiControlForm::stabilizeValueChange (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	const QString& sType    = m_ui.ControlTypeComboBox->currentText();
	const QString& sChannel = m_ui.ChannelComboBox->currentText();
	const QString& sParam   = m_ui.ParamComboBox->currentText();

	qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::typeFromName(sType);
	unsigned short iChannel = channelFromText(sChannel);
	unsigned short iParam = paramFromText(ctype, sParam);
	if (m_ui.TrackCheckBox->isChecked()
		&& (iChannel & qtractorMidiControl::TrackParam) == 0)
		iParam |= qtractorMidiControl::TrackParam;

	bool bMapped = pMidiControl->isChannelParamMapped(ctype, iChannel, iParam);

	if (bMapped) {
		qtractorMidiControl::Command command
			= qtractorMidiControl::commandFromName(
				m_ui.CommandComboBox->currentText());
		int iTrack = m_ui.TrackSpinBox->value();
		bool bFeedback = m_ui.FeedbackCheckBox->isChecked();
		pMidiControl->mapChannelParam(
			ctype, iChannel, iParam, command, iTrack, bFeedback);
		m_iDirtyCount = 0;
		++m_iDirtyMap;
		refreshControlMap();
	}
}


// Stabilize form status.
void qtractorMidiControlForm::stabilizeForm (void)
{
	if (m_iUpdating > 0)
		return;

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

	pItem = m_ui.ControlMapListView->currentItem();
	if (pItem) {
		++m_iUpdating;
		m_ui.ControlTypeComboBox->setCurrentIndex(
			m_ui.ControlTypeComboBox->findText(pItem->text(0)));
		stabilizeTypeChange();
		m_ui.ChannelComboBox->setCurrentIndex(
			m_ui.ChannelComboBox->findText(pItem->text(1)));
		m_ui.ParamComboBox->setCurrentIndex(
			m_ui.ParamComboBox->findText(pItem->text(2)));
		const QString& sText = pItem->text(3);
		m_ui.TrackCheckBox->setChecked(sText[0] == '+');
		m_ui.TrackSpinBox->setValue(sText.toInt());
		m_ui.CommandComboBox->setCurrentIndex(
			m_ui.CommandComboBox->findText(pItem->text(4)));
		m_ui.FeedbackCheckBox->setChecked(pItem->text(5) == tr("Yes"));
		--m_iUpdating;
	}

	m_ui.ReloadPushButton->setEnabled(m_iDirtyMap > 0);

	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl)
		m_ui.ExportPushButton->setEnabled(
			!pMidiControl->controlMap().isEmpty());

	stabilizeKeyChange();
}


// Refresh all controller definition views.
void qtractorMidiControlForm::refreshFiles (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// Freeze...
	m_ui.FilesListView->setUpdatesEnabled(false);

	// Files list view...
	m_ui.FilesListView->clear();
	QList<QTreeWidgetItem *> files;
	QStringListIterator iter(pOptions->midiControlFiles);
	while (iter.hasNext()) {
		const QString& sPath = iter.next();
		QTreeWidgetItem *pFileItem = new QTreeWidgetItem();
		pFileItem->setIcon(0, QIcon(":/images/itemFile.png"));
		pFileItem->setText(0, QFileInfo(sPath).completeBaseName());
		pFileItem->setText(1, sPath);
		files.append(pFileItem);
	}
	m_ui.FilesListView->addTopLevelItems(files);

	// Bail out...
	m_ui.FilesListView->setUpdatesEnabled(true);

	refreshControlMap();
}


// Refresh controller map view.
void qtractorMidiControlForm::refreshControlMap (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	// Freeze...
	m_ui.ControlMapListView->setUpdatesEnabled(false);

	// Control map list view...
	m_ui.ControlMapListView->clear();
	QList<QTreeWidgetItem *> items;
	const qtractorMidiControl::ControlMap& controlMap = pMidiControl->controlMap();
	qtractorMidiControl::ControlMap::ConstIterator it = controlMap.constBegin();
	const QIcon	iconControlType(":/images/itemProperty.png");
	const QIcon	iconParam(":/images/itemControllers.png");
	const QIcon	iconCommand(":/images/itemChannel.png");
	for ( ; it != controlMap.constEnd(); ++it) {
		const qtractorMidiControl::MapKey& key = it.key();
		const qtractorMidiControl::MapVal& val = it.value();
		QTreeWidgetItem *pItem = new QTreeWidgetItem();
		pItem->setIcon(0, iconControlType);
		pItem->setText(0, qtractorMidiControl::nameFromType(key.type()));
		pItem->setText(1, textFromChannel(key.channel()));
		pItem->setIcon(2, iconParam);
		pItem->setText(2, textFromParam(key.type(), key.param()));
		QString sText;
		if (key.isParamTrack())
			sText += "+ ";
		pItem->setText(3, sText + QString::number(val.track()));
		pItem->setIcon(4, iconCommand);
		pItem->setText(4, qtractorMidiControl::nameFromCommand(val.command()));
		pItem->setText(5, val.isFeedback() ? tr("Yes") : tr("No"));
		items.append(pItem);
	}
	m_ui.ControlMapListView->addTopLevelItems(items);

	// Bail out...
	m_ui.ControlMapListView->setUpdatesEnabled(true);

	stabilizeForm();
}


// Channel text conversion helpers.
unsigned short qtractorMidiControlForm::channelFromText (
	const QString& sText ) const
{
	if (sText == "*" || sText.isEmpty())
		return qtractorMidiControl::TrackParam;
	else
		return sText.toUShort() - 1;
}

QString qtractorMidiControlForm::textFromChannel (
	unsigned short iChannel ) const
{
	if (iChannel & qtractorMidiControl::TrackParam)
		return "*"; // "TrackParam";
	else
		return QString::number(iChannel + 1);
}


// Controller parameter text conversion helpers.
unsigned short qtractorMidiControlForm::paramFromText (
	qtractorMidiControl::ControlType /*ctype*/, const QString& sText ) const
{
	return sText.section(' ', 0, 0).toUShort();	
}

QString qtractorMidiControlForm::textFromParam (
	qtractorMidiControl::ControlType ctype, unsigned short iParam ) const
{
	iParam &= qtractorMidiControl::TrackParamMask;

	QString sText = QString::number(iParam);
	switch (ctype) {
	case qtractorMidiEvent::NOTEON:
	case qtractorMidiEvent::NOTEOFF:
	case qtractorMidiEvent::KEYPRESS:
		sText += " - "	+ qtractorMidiEditor::defaultNoteName(iParam);
		break;
	case qtractorMidiEvent::CONTROLLER:
		sText += " - "	+ qtractorMidiEditor::defaultControllerName(iParam);
		break;
	default:
		break;
	}

	return sText;
}


// end of qtractorMidiControlForm.cpp
