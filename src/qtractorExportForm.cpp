// qtractorExportForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2023, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTracks.h"

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
qtractorExportForm::qtractorExportForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);
#if QT_VERSION < QT_VERSION_CHECK(6, 1, 0)
	QDialog::setWindowIcon(QIcon(":/images/qtractor.png"));
#endif
	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::ApplicationModal);

	// Initialize dirty control state.
	m_exportType = qtractorTrack::None;
	m_pTimeScale = nullptr;

	// Deafult title prefix.
	m_sExportTitle = tr("Export");

	// Try to restore old window positioning.
	m_ui.ExportTypeWidget->hide();
//	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.ExportPathComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(exportPathChanged(const QString&)));
	QObject::connect(m_ui.ExportPathToolButton,
		SIGNAL(clicked()),
		SLOT(exportPathClicked()));

	QObject::connect(m_ui.AudioExportTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(audioExportTypeChanged(int)));
	QObject::connect(m_ui.AudioExportFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(stabilizeForm()));

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
	QObject::connect(m_ui.AddTrackCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
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


// Default window title (prefix).
void qtractorExportForm::setExportTitle ( const QString& sExportTitle )
{
	m_sExportTitle = sExportTitle;
}

const QString& qtractorExportForm::exportTitle (void) const
{
	return m_sExportTitle;
}

// Populate (setup) dialog controls from settings descriptors.
void qtractorExportForm::setExportType ( qtractorTrack::TrackType exportType )
{
	// Export type...
	m_exportType = exportType;

	QIcon icon;
	qtractorEngine  *pEngine  = nullptr;
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
			m_ui.ExportTypeWidget->removeWidget(m_ui.MidiExportTypePage);
			break;
		case qtractorTrack::Midi:
			pEngine = pSession->midiEngine();
			icon = QIcon(":/images/trackMidi.png");
			m_sExportType = tr("MIDI");
			m_sExportExt  = "mid";
			m_ui.ExportTypeWidget->removeWidget(m_ui.AudioExportTypePage);
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
	if (pOptions) {
		pOptions->loadComboBoxHistory(m_ui.ExportPathComboBox);
		switch (m_exportType) {
		case qtractorTrack::Audio: {
			// Audio options...
			QString sAudioExportExt = pOptions->sAudioExportExt;
			int iAudioExportType    = pOptions->iAudioExportType;
			int iAudioExportFormat  = pOptions->iAudioExportFormat;
			int iAudioExportQuality = pOptions->iAudioExportQuality;
			if (sAudioExportExt.isEmpty())
				sAudioExportExt = pOptions->sAudioCaptureExt;
			if (iAudioExportType < 0)
				iAudioExportType = pOptions->iAudioCaptureType;
			if (iAudioExportFormat < 0)
				iAudioExportFormat = pOptions->iAudioCaptureFormat;
			if (iAudioExportQuality < 0)
				iAudioExportQuality = pOptions->iAudioCaptureQuality;
			m_ui.AudioExportTypeComboBox->setCurrentType(
				sAudioExportExt, iAudioExportType);
			m_ui.AudioExportFormatComboBox->setCurrentIndex(iAudioExportFormat);
			m_ui.AudioExportQualitySpinBox->setValue(iAudioExportQuality);
			m_sExportExt = m_ui.AudioExportTypeComboBox->currentExt();
			m_ui.AddTrackCheckBox->setChecked(pOptions->bExportAddTrack);
			m_ui.AddTrackCheckBox->show();
			break;
		}
		case qtractorTrack::Midi: {
			// Initial MIDI options...
			int iMidiExportFormat = pOptions->iMidiExportFormat;
			if (iMidiExportFormat < 0)
				iMidiExportFormat = pOptions->iMidiCaptureFormat;
			m_ui.MidiExportFormatComboBox->setCurrentIndex(iMidiExportFormat);
			m_ui.AddTrackCheckBox->hide();
			break;
		}
		case qtractorTrack::None:
		default:
			break;
		}
	}

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
			windowTitleEx(m_sExportTitle, m_sExportType));
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
	switch (pOptions ? RangeType(pOptions->iExportRangeType) : Session) {
	case Custom:
		m_ui.CustomRangeRadioButton->setChecked(true);
		break;
	case Edit:
		if (pSession && pSession->editHead() < pSession->editTail()) {
			m_ui.EditRangeRadioButton->setChecked(true);
			break;
		}
		// Fall thru...
	case Punch:
		if (pSession && pSession->isPunching()) {
			m_ui.PunchRangeRadioButton->setChecked(true);
			break;
		}
		// Fall thru...
	case Loop:
		if (pSession && pSession->isLooping()) {
			m_ui.LoopRangeRadioButton->setChecked(true);
			break;
		}
		// Fall thru...
	case Session:
	default:
		m_ui.SessionRangeRadioButton->setChecked(true);
		break;
	}
	rangeChanged();

	// Shake it a little bit first, but
	// make it as tight as possible...
	m_ui.ExportTypeWidget->show();
	QDialog::resize(width() - 1, height() - 1);
//	adjustSize();

	// Done.
	stabilizeForm();
}


// Retrieve the current export type, if the case arises.
qtractorTrack::TrackType qtractorExportForm::exportType (void) const
{
	return m_exportType;
}


// Choose the target filename of export.
void qtractorExportForm::exportPathClicked (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	QString sExportPath = m_ui.ExportPathComboBox->currentText();
	if (sExportPath.isEmpty())
		sExportPath = pSession->sessionDir();

	// Actual browse for the file...
	const QString& sTitle
		= tr("Export %1 File").arg(m_sExportType);

	QStringList filters;
	switch (m_exportType) {
	case qtractorTrack::Audio:
		filters.append(qtractorAudioFileFactory::filters());
		break;
	case qtractorTrack::Midi:
		filters.append(tr("MIDI files (*.%1 *.smf *.midi)").arg(m_sExportExt));
		// Fall-thru...
	case qtractorTrack::None:
	default:
		filters.append(tr("All files (*.*)"));
		break;
	}

	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	sExportPath = QFileDialog::getSaveFileName(pParentWidget,
		sTitle, sExportPath, sFilter, nullptr, options);
#else
	QFileDialog fileDialog(pParentWidget,
		sTitle, sExportPath, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(m_sExportExt);
	// Stuff sidebar...
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
	}
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sExportPath = fileDialog.selectedFiles().first();
	else
		sExportPath.clear();
#endif

	// Have we cancelled it?
	if (sExportPath.isEmpty() || sExportPath.at(0) == '.')
		return;

	// Enforce default file extension...
	if (QFileInfo(sExportPath).suffix().isEmpty())
		sExportPath += '.' + m_sExportExt;

	// Finallly set as wanted...
	m_ui.ExportPathComboBox->setEditText(sExportPath);
	m_ui.ExportPathComboBox->setFocus();

	stabilizeForm();
}


// Export path changed.
void qtractorExportForm::exportPathChanged ( const QString& sExportPath )
{
	if (m_exportType == qtractorTrack::Audio) {
		const QString& sExportExt
			= QFileInfo(sExportPath).suffix().toLower();
		if (sExportExt != m_sExportExt) {
			const int iIndex
				= m_ui.AudioExportTypeComboBox->indexOf(sExportExt);
			if (iIndex >= 0) {
				m_ui.AudioExportTypeComboBox->setCurrentIndex(iIndex);
				m_sExportExt = sExportExt;
				audioExportTypeUpdate(iIndex);
			}
		}
	}

	stabilizeForm();
}


// Audio file type changed.
void qtractorExportForm::audioExportTypeChanged ( int iIndex )
{
	if (m_exportType == qtractorTrack::Audio) {
		const void *handle
			= m_ui.AudioExportTypeComboBox->handleOf(iIndex);
		if (handle) {
			m_sExportExt = m_ui.AudioExportTypeComboBox->currentExt(handle);
			QString sExportPath = m_ui.ExportPathComboBox->currentText();
			const QString& sExportExt
				= QFileInfo(sExportPath).suffix().toLower();
			if (sExportExt != m_sExportExt) {
				if (sExportExt.isEmpty())
					sExportPath += '.' + m_sExportExt;
				else
					sExportPath.replace('.' + sExportExt, '.' + m_sExportExt);
				m_ui.ExportPathComboBox->setEditText(sExportPath);
			}
		}
		audioExportTypeUpdate(iIndex);
	}

	stabilizeForm();
}


// Range type has changed.
void qtractorExportForm::rangeChanged (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	unsigned long iExportStart = 0;
	unsigned long iExportEnd   = 0;

	if (m_ui.CustomRangeRadioButton->isChecked()) {
		iExportStart = pOptions->iExportRangeStart;
		iExportEnd   = pOptions->iExportRangeEnd;
	}
	else
	if (m_ui.EditRangeRadioButton->isChecked()) {
		iExportStart = pSession->editHead();
		iExportEnd   = pSession->editTail();
	}
	else
	if (m_ui.PunchRangeRadioButton->isChecked()) {
		iExportStart = pSession->punchIn();
		iExportEnd   = pSession->punchOut();
	}
	else
	if (m_ui.LoopRangeRadioButton->isChecked()) {
		iExportStart = pSession->loopStart();
		iExportEnd   = pSession->loopEnd();
	}

	if (iExportStart >= iExportEnd) {
		iExportStart = pSession->sessionStart();
		iExportEnd   = pSession->sessionEnd();
	}
#if 0
	if (iExportEnd > pSession->sessionEnd())
		iExportEnd = pSession->sessionEnd();
#endif
	m_ui.ExportStartSpinBox->setValue(iExportStart, false);
	m_ui.ExportEndSpinBox->setValue(iExportEnd, false);

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
	const bool bBlockSignals
		= m_ui.FormatComboBox->blockSignals(true);

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
	// General options validy check...
	const QString& sExportPath
		= m_ui.ExportPathComboBox->currentText();
	bool bValid = !sExportPath.isEmpty() &&
		QFileInfo(sExportPath).dir().exists();

	// Audio options stabilizing and validy check...
	if (m_exportType == qtractorTrack::Audio) {
		const qtractorAudioFileFactory::FileFormat *pFormat
			= static_cast<const qtractorAudioFileFactory::FileFormat *> (
				m_ui.AudioExportTypeComboBox->currentHandle());
		const bool bSndFile
			= (pFormat && pFormat->type == qtractorAudioFileFactory::SndFile);
		m_ui.AudioExportFormatTextLabel->setEnabled(bSndFile);
		m_ui.AudioExportFormatComboBox->setEnabled(bSndFile);
		const bool bVorbisFile
			= (pFormat && pFormat->type == qtractorAudioFileFactory::VorbisFile);
		m_ui.AudioExportQualityTextLabel->setEnabled(bVorbisFile);
		m_ui.AudioExportQualitySpinBox->setEnabled(bVorbisFile);
		if (bValid && pFormat == nullptr)
			bValid = false;
		if (bValid) {
			const int iFormat = m_ui.AudioExportFormatComboBox->currentIndex();
			bValid = qtractorAudioFileFactory::isValidFormat(pFormat, iFormat);
		}
	}

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);
}


// Audio file type changed aftermath.
void qtractorExportForm::audioExportTypeUpdate ( int iIndex )
{
	const void *handle
		= m_ui.AudioExportTypeComboBox->handleOf(iIndex);
	const qtractorAudioFileFactory::FileFormat *pFormat
		= static_cast<const qtractorAudioFileFactory::FileFormat *> (handle);
	if (handle && pFormat) {
		const bool bBlockSignals
			= m_ui.AudioExportFormatComboBox->blockSignals(true);
		int iFormat = m_ui.AudioExportFormatComboBox->currentIndex();
		while (iFormat > 0 && // Retry down to PCM Signed 16-Bit...
			!qtractorAudioFileFactory::isValidFormat(pFormat, iFormat))
			--iFormat;
		m_ui.AudioExportFormatComboBox->setCurrentIndex(iFormat);
		m_ui.AudioExportFormatComboBox->blockSignals(bBlockSignals);
	}
}


// Retrieve current audio file suffix.
const QString& qtractorExportForm::exportExt (void) const
{
	return m_sExportExt;
}


// Retrieve current aliased file format index...
int qtractorExportForm::audioExportFormat (void) const
{
	if (m_exportType != qtractorTrack::Audio)
		return -1;

	const qtractorAudioFileFactory::FileFormat *pFormat
		= static_cast<const qtractorAudioFileFactory::FileFormat *> (
			m_ui.AudioExportTypeComboBox->currentHandle());
	if (pFormat == nullptr)
		return -1;

	if (pFormat->type == qtractorAudioFileFactory::VorbisFile)
		return m_ui.AudioExportQualitySpinBox->value();
	else
		return m_ui.AudioExportFormatComboBox->currentIndex();
}


int qtractorExportForm::midiExportFormat (void) const
{
	if (m_exportType != qtractorTrack::Midi)
		return -1;

	return m_ui.MidiExportFormatComboBox->currentIndex();
}


// Save export options (settings).
void qtractorExportForm::saveExportOptions (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	pOptions->saveComboBoxHistory(m_ui.ExportPathComboBox);
	switch (m_exportType) {
	case qtractorTrack::Audio: {
		// Audio options...
		const void *handle = m_ui.AudioExportTypeComboBox->currentHandle();
		pOptions->sAudioExportExt = m_ui.AudioExportTypeComboBox->currentExt(handle);;
		pOptions->iAudioExportType = m_ui.AudioExportTypeComboBox->currentType(handle);
		pOptions->iAudioExportFormat = m_ui.AudioExportFormatComboBox->currentIndex();
		pOptions->iAudioExportQuality = m_ui.AudioExportQualitySpinBox->value();
		pOptions->bExportAddTrack = m_ui.AddTrackCheckBox->isChecked();
		break;
	}
	case qtractorTrack::Midi:
		// MIDI options...
		pOptions->iMidiExportFormat = m_ui.MidiExportFormatComboBox->currentIndex();
		// Fall-thru...
	case qtractorTrack::None:
	default:
		break;
	}

	if (m_ui.CustomRangeRadioButton->isChecked()) {
		pOptions->iExportRangeType = Custom;
		pOptions->iExportRangeStart = m_ui.ExportStartSpinBox->value();
		pOptions->iExportRangeEnd = m_ui.ExportEndSpinBox->value();
	}
	else
	if (m_ui.EditRangeRadioButton->isChecked())
		pOptions->iExportRangeType = Edit;
	else
	if (m_ui.PunchRangeRadioButton->isChecked())
		pOptions->iExportRangeType = Punch;
	else
	if (m_ui.LoopRangeRadioButton->isChecked())
		pOptions->iExportRangeType = Loop;
	else
//	if (m_ui.SessionRangeRadioButton->isChecked())
		pOptions->iExportRangeType = Session;
}


//----------------------------------------------------------------------------
// qtractorExportTrackForm -- UI wrapper form.

// Constructor.
qtractorExportTrackForm::qtractorExportTrackForm ( QWidget *pParent )
	: qtractorExportForm(pParent)
{
}


// Destructor.
qtractorExportTrackForm::~qtractorExportTrackForm (void)
{
}


// Make up window/dialog title.
QString qtractorExportTrackForm::windowTitleEx (
	const QString& sExportTitle, const QString& sExportType ) const
{
	return tr("%1 %2 Tracks").arg(sExportTitle).arg(sExportType);
}


// Stabilize current form state.
void qtractorExportTrackForm::stabilizeForm (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Range options stabilizing...
	m_ui.LoopRangeRadioButton->setEnabled(pSession->isLooping());
	m_ui.PunchRangeRadioButton->setEnabled(pSession->isPunching());
	m_ui.EditRangeRadioButton->setEnabled(
		pSession->editHead() < pSession->editTail());

	const unsigned long iExportStart = m_ui.ExportStartSpinBox->value();
	const unsigned long iExportEnd = m_ui.ExportEndSpinBox->value();

	m_ui.ExportStartSpinBox->setMaximum(iExportEnd);
	m_ui.ExportEndSpinBox->setMinimum(iExportStart);

	qtractorExportForm::stabilizeForm();

	QPushButton *pPushButton = m_ui.DialogButtonBox->button(QDialogButtonBox::Ok);
	if (pPushButton && pPushButton->isEnabled()) {
		pPushButton->setEnabled(
			m_ui.ExportBusNameListBox->currentItem() != nullptr &&
			m_ui.ExportStartSpinBox->value() < m_ui.ExportEndSpinBox->value());
	}
}


// Executive slots -- accept settings (OK button slot).
void qtractorExportTrackForm::accept (void)
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
			tr("Warning"),
			tr("The file already exists:\n\n"
			"\"%1\"\n\n"
			"Do you want to replace it?")
			.arg(sExportPath),
			QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
			m_ui.ExportPathComboBox->setFocus();
			return;
		}
	}

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	// It can take a minute...
	m_ui.ExportPathTextLabel->setEnabled(false);
	m_ui.ExportPathComboBox->setEnabled(false);
	m_ui.ExportPathToolButton->setEnabled(false);
	m_ui.ExportTypeWidget->setEnabled(false);
	m_ui.ExportBusGroupBox->setEnabled(false);
	m_ui.ExportRangeGroupBox->setEnabled(false);
	m_ui.FormatGroupBox->setEnabled(false);
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	// Carry on...
	switch (m_exportType) {
	case qtractorTrack::Audio: {
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
			// Go...
			const bool bResult = pAudioEngine->fileExport(
				sExportPath, exportBuses,
				m_ui.ExportStartSpinBox->value(),
				m_ui.ExportEndSpinBox->value(),
				audioExportFormat());
			// Done.
			QApplication::restoreOverrideCursor();
			if (bResult) {
				// Add new tracks if necessary...
				qtractorTracks *pTracks = pMainForm->tracks();
				if (pTracks && m_ui.AddTrackCheckBox->isChecked()) {
					pTracks->addAudioTracks(
						QStringList(sExportPath),
						pAudioEngine->exportStart(),
						pAudioEngine->exportOffset(),
						pAudioEngine->exportLength(),
						pTracks->currentTrack());
				}
				else pMainForm->addAudioFile(sExportPath);
				// Log the success...
				pMainForm->appendMessages(
					tr("Audio file export: \"%1\" complete.")
					.arg(sExportPath));
			} else {
				// Log the failure...
				pMainForm->appendMessagesError(
					tr("Audio file export:\n\n\"%1\"\n\nfailed.")
					.arg(sExportPath));
			}
			// HACK: Reset all (internal) MIDI controllers...
			qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
			if (pMidiEngine)
				pMidiEngine->resetAllControllers(true); // Force immediate.
		}
		break;
	}
	case qtractorTrack::Midi: {
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
			// Go...
			const bool bResult = pMidiEngine->fileExport(
				sExportPath, exportBuses,
				m_ui.ExportStartSpinBox->value(),
				m_ui.ExportEndSpinBox->value(),
				midiExportFormat());
			// Done.
			QApplication::restoreOverrideCursor();
			if (bResult) {
			#if 0
				// Add new tracks if necessary...
				qtractorTracks *pTracks = pMainForm->tracks();
				if (pTracks && m_ui.AddTrackCheckBox->isChecked()) {
					pTracks->addMidiTracks(
						QStringList(sExportPath),
						m_ui.ExportStartSpinBox->value(),
						pTracks->currentTrack());
				}
				else
			#endif
				pMainForm->addMidiFile(sExportPath);
				// Log the success...
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
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}

	// Save other conveniency options...
	saveExportOptions();

	// Just go with dialog acceptance.
	qtractorExportForm::accept();
}


// Executive slots -- reject settings (Cancel button slot).
void qtractorExportTrackForm::reject (void)
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
	qtractorExportForm::reject();
}


//----------------------------------------------------------------------------
// qtractorExportClipForm -- UI wrapper form.

// Constructor.
qtractorExportClipForm::qtractorExportClipForm ( QWidget *pParent )
	: qtractorExportForm(pParent)
{
	// Hide and disable all unecessary stuff.
	m_ui.ExportRangeGroupBox->setEnabled(false);
	m_ui.ExportRangeGroupBox->setVisible(false);

	m_ui.ExportBusGroupBox->setEnabled(false);
	m_ui.ExportBusGroupBox->setVisible(false);

	m_ui.FormatGroupBox->setEnabled(false);
	m_ui.FormatGroupBox->setVisible(false);

	m_ui.AddTrackCheckBox->setEnabled(false);
	m_ui.AddTrackCheckBox->setVisible(false);

	adjustSize();
}


// Destructor.
qtractorExportClipForm::~qtractorExportClipForm (void)
{
}


// Make up window/dialog title.
QString qtractorExportClipForm::windowTitleEx (
	const QString& sExportTitle, const QString& sExportType ) const
{
	return tr("%1 %2 Clips").arg(sExportTitle).arg(sExportType);
}


// Settle/retrieve the export path.
void qtractorExportClipForm::setExportPath ( const QString& sExportPath )
{
	m_ui.ExportPathComboBox->setCurrentText(sExportPath);
}

QString qtractorExportClipForm::exportPath (void) const
{
	return m_ui.ExportPathComboBox->currentText();
}


// Executive slots -- accept settings (OK button slot).
void qtractorExportClipForm::accept (void)
{
	saveExportOptions();

	qtractorExportForm::accept();
}


// end of qtractorExportForm.cpp
