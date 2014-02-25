// qtractorExportForm.cpp
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

#include "qtractorExportForm.h"

#include "qtractorAbout.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorAudioFile.h"

#include "qtractorMainForm.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>


//----------------------------------------------------------------------------
// qtractorExportForm -- UI wrapper form.

// Constructor.
qtractorExportForm::qtractorExportForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::ApplicationModal);

	// Initialize dirty control state.
	m_exportType = qtractorTrack::None;
	m_pTimeScale = NULL;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.ExportPathComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ExportPathToolButton,
		SIGNAL(clicked()),
		SLOT(browseExportPath()));
	QObject::connect(m_ui.SessionRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.LoopRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.PunchRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.EditRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.CustomRangeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(rangeChanged()));
	QObject::connect(m_ui.ExportBusNameListBox,
		SIGNAL(currentRowChanged(int)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ExportStartSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(valueChanged()));
	QObject::connect(m_ui.ExportStartSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ExportEndSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(valueChanged()));
	QObject::connect(m_ui.ExportEndSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.FormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorExportForm::~qtractorExportForm (void)
{
	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorExportForm::setExportType ( qtractorTrack::TrackType exportType )
{
	// Export type...
	m_exportType = exportType;

	QIcon icon;
	qtractorEngine  *pEngine  = NULL;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Copy from global time-scale instance...
		if (m_pTimeScale)
			delete m_pTimeScale;
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.ExportStartSpinBox->setTimeScale(m_pTimeScale);
		m_ui.ExportEndSpinBox->setTimeScale(m_pTimeScale);
		switch (m_exportType) {
		case qtractorTrack::Audio:
			pEngine = pSession->audioEngine();
			icon = QIcon(":/images/trackAudio.png");
			m_sExportType = tr("Audio");
			m_sExportExt  = qtractorAudioFileFactory::defaultExt();
			break;
		case qtractorTrack::Midi:
			pEngine = pSession->midiEngine();
			icon = QIcon(":/images/trackMidi.png");
			m_sExportType = tr("MIDI");
			m_sExportExt  = "mid";
			break;
		case qtractorTrack::None:
		default:
			m_sExportType.clear();
			m_sExportExt.clear();
			break;
		}
	}

	// Grab export file history, one that might me useful...
	m_ui.ExportPathComboBox->setObjectName(
		m_sExportType + m_ui.ExportPathComboBox->objectName());
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->loadComboBoxHistory(m_ui.ExportPathComboBox);

	// Suggest a brand new export filename...
	if (pSession) {
		m_ui.ExportPathComboBox->setEditText(
			pSession->createFilePath("export", m_sExportExt));
	}

	// Fill in the output bus names list...
	m_ui.ExportBusNameListBox->clear();
	if (pEngine) {
		QDialog::setWindowIcon(icon);
		QDialog::setWindowTitle(
			tr("Export %1").arg(m_sExportType) + " - " QTRACTOR_TITLE);
		for (qtractorBus *pBus = pEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output)
				m_ui.ExportBusNameListBox->addItem(
					new QListWidgetItem(icon, pBus->busName()));
		}
	}
	
	// Set proper time scales display format...
	if (m_pTimeScale) {
		m_ui.FormatComboBox->setCurrentIndex(
			int(m_pTimeScale->displayFormat()));
	}

	// Populate range values...
	m_ui.SessionRangeRadioButton->setChecked(true);
//	rangeChanged();

	// Done.
	stabilizeForm();
}


// Retrieve the current export type, if the case arises.
qtractorTrack::TrackType qtractorExportForm::exportType (void) const
{
	return m_exportType;
}


// Accept settings (OK button slot).
void qtractorExportForm::accept (void)
{
	// Must always be a export bus target...
	const QList<QListWidgetItem *>& exportBusNameItems
		= m_ui.ExportBusNameListBox->selectedItems();
	if (exportBusNameItems.isEmpty())
		return;

	// Enforce (again) default file extension...
	QString sExportPath = m_ui.ExportPathComboBox->currentText();

	if (QFileInfo(sExportPath).suffix().isEmpty())
		sExportPath += '.' + m_sExportExt;

	// Check (again) wether the file already exists...
	if (QFileInfo(sExportPath).exists()) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("The file already exists:\n\n"
			"\"%1\"\n\n"
			"Do you want to replace it?")
			.arg(sExportPath),
			QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
			m_ui.ExportPathComboBox->setFocus();
			return;
		}
	}

	qtractorSession  *pSession  = qtractorSession::getInstance();
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pSession && pMainForm) {
		// It can take a minute...
		m_ui.ExportPathTextLabel->setEnabled(false);
		m_ui.ExportPathComboBox->setEnabled(false);
		m_ui.ExportPathToolButton->setEnabled(false);
		m_ui.ExportBusGroupBox->setEnabled(false);
		m_ui.ExportRangeGroupBox->setEnabled(false);
		m_ui.FormatGroupBox->setEnabled(false);
		m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
		// Carry on...
		if (m_exportType == qtractorTrack::Audio) {
			// Audio file export...
			qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
			if (pAudioEngine) {
				// Get the export buses by name...
				QList<qtractorAudioBus *> exportBuses;
				QListIterator<QListWidgetItem *> iter(exportBusNameItems);
				while (iter.hasNext()) {
					qtractorAudioBus *pExportBus
						= static_cast<qtractorAudioBus *> (
							pAudioEngine->findOutputBus(iter.next()->text()));
					if (pExportBus)
						exportBuses.append(pExportBus);
				}
				// Log this event...
				pMainForm->appendMessages(
					tr("Audio file export: \"%1\" started...")
					.arg(sExportPath));
				// Do the export as commanded...
				QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
				bool bResult = pAudioEngine->fileExport(
					sExportPath, exportBuses,
					m_ui.ExportStartSpinBox->value(),
					m_ui.ExportEndSpinBox->value());
				QApplication::restoreOverrideCursor();
				if (bResult) {
					// Log the success...
					pMainForm->addAudioFile(sExportPath);
					pMainForm->appendMessages(
						tr("Audio file export: \"%1\" complete.")
						.arg(sExportPath));
				} else {
					// Log the failure...
					pMainForm->appendMessagesError(
						tr("Audio file export:\n\n\"%1\"\n\nfailed.")
						.arg(sExportPath));
				}
			}
		}
		else
		if (m_exportType == qtractorTrack::Midi) {
			// MIDI file export...
			qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
			if (pMidiEngine) {
				// Get the export buses by name...
				QList<qtractorMidiBus *> exportBuses;
				QListIterator<QListWidgetItem *> iter(exportBusNameItems);
				while (iter.hasNext()) {
					qtractorMidiBus *pExportBus
						= static_cast<qtractorMidiBus *> (
							pMidiEngine->findOutputBus(iter.next()->text()));
					if (pExportBus)
						exportBuses.append(pExportBus);
				}
				// Log this event...
				pMainForm->appendMessages(
					tr("MIDI file export: \"%1\" started...")
					.arg(sExportPath));
				// Do the export as commanded...
				QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
				bool bResult = pMidiEngine->fileExport(
					sExportPath, exportBuses,
					m_ui.ExportStartSpinBox->value(),
					m_ui.ExportEndSpinBox->value());
				QApplication::restoreOverrideCursor();
				if (bResult) {
					// Log the success...
					pMainForm->addMidiFile(sExportPath);
					pMainForm->appendMessages(
						tr("MIDI file export: \"%1\" complete.")
						.arg(sExportPath));
				} else {
					// Log the failure...
					pMainForm->appendMessagesError(
						tr("MIDI file export:\n\n\"%1\"\n\nfailed.")
						.arg(sExportPath));
				}
			}
		}
		// Save other conveniency options...
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions)
			pOptions->saveComboBoxHistory(m_ui.ExportPathComboBox);
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorExportForm::reject (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// It can take a minute...
		m_ui.DialogButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
		switch (m_exportType) {
		case qtractorTrack::Audio: {
			// Audio file export...
			qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
			if (pAudioEngine)
				pAudioEngine->setExporting(false);
			break;
		}
		case qtractorTrack::Midi:
		default:
			break;
		}
	}

	// Bail out...
	QDialog::reject();
}


// Choose the target filename of export.
void qtractorExportForm::browseExportPath (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	QString sExportPath = m_ui.ExportPathComboBox->currentText();
	if (sExportPath.isEmpty())
		sExportPath = pSession->sessionDir();

	// Actual browse for the file...
	const QString& sTitle  = tr("Export %1 File").arg(m_sExportType) + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("%1 files (*.%1)").arg(m_sExportExt);
#if 1//QT_VERSION < 0x040400
	QFileDialog::Options options = 0;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sExportPath = QFileDialog::getSaveFileName(this,
		sTitle, sExportPath, sFilter, NULL, options);
#else
	QFileDialog fileDialog(this,
		sTitle, sExportPath, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(m_sExportExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		switch (m_exportType) {
		case qtractorTrack::Audio:
			urls.append(QUrl::fromLocalFile(pOptions->sAudioDir));
			break;
		case qtractorTrack::Midi:
			urls.append(QUrl::fromLocalFile(pOptions->sMidiDir));
			break;
		case qtractorTrack::None:
		default:
			break;
		}
		fileDialog.setSidebarUrls(urls);
		if (pOptions->bDontUseNativeDialogs)
			fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	}
	// Show dialog...
	if (fileDialog.exec())
		sExportPath = fileDialog.selectedFiles().first();
	else
		sExportPath.clear();
#endif

	// Have we cancelled it?
	if (sExportPath.isEmpty())
		return;

	// Enforce default file extension...
	if (QFileInfo(sExportPath).suffix().isEmpty())
		sExportPath += '.' + m_sExportExt;

	// Finallly set as wanted...
	m_ui.ExportPathComboBox->setEditText(sExportPath);
	m_ui.ExportPathComboBox->setFocus();

	stabilizeForm();
}


// Display format has changed.
void qtractorExportForm::rangeChanged (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (m_ui.SessionRangeRadioButton->isChecked()) {
		m_ui.ExportStartSpinBox->setValue(pSession->sessionStart(), false);
		m_ui.ExportEndSpinBox->setValue(pSession->sessionEnd(), false);
	}
	else
	if (m_ui.LoopRangeRadioButton->isChecked()) {
		m_ui.ExportStartSpinBox->setValue(pSession->loopStart(), false);
		m_ui.ExportEndSpinBox->setValue(pSession->loopEnd(), false);
	}
	else
	if (m_ui.PunchRangeRadioButton->isChecked()) {
		m_ui.ExportStartSpinBox->setValue(pSession->punchIn(), false);
		m_ui.ExportEndSpinBox->setValue(pSession->punchOut(), false);
	}
	else
	if (m_ui.EditRangeRadioButton->isChecked()) {
		m_ui.ExportStartSpinBox->setValue(pSession->editHead(), false);
		m_ui.ExportEndSpinBox->setValue(pSession->editTail(), false);
	}

	stabilizeForm();
}


// Range values have changed.
void qtractorExportForm::valueChanged (void)
{
	m_ui.CustomRangeRadioButton->setChecked(true);

	stabilizeForm();
}


// Display format has changed.
void qtractorExportForm::formatChanged ( int iDisplayFormat )
{
	bool bBlockSignals = m_ui.FormatComboBox->blockSignals(true);
	m_ui.FormatComboBox->setCurrentIndex(iDisplayFormat);

	qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.ExportStartSpinBox->setDisplayFormat(displayFormat);
	m_ui.ExportEndSpinBox->setDisplayFormat(displayFormat);

	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	m_ui.FormatComboBox->blockSignals(bBlockSignals);

	stabilizeForm();
}


// Stabilize current form state.
void qtractorExportForm::stabilizeForm (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	m_ui.LoopRangeRadioButton->setEnabled(pSession->isLooping());
	m_ui.PunchRangeRadioButton->setEnabled(pSession->isPunching());
	m_ui.EditRangeRadioButton->setEnabled(
		pSession->editHead() < pSession->editTail());

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(
		!m_ui.ExportPathComboBox->currentText().isEmpty() &&
		m_ui.ExportBusNameListBox->currentItem() != NULL &&
		m_ui.ExportStartSpinBox->value() < m_ui.ExportEndSpinBox->value());
}


// end of qtractorExportForm.cpp
