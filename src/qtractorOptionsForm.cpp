// qtractorOptionsForm.cpp
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

#include "qtractorOptionsForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"

#include "qtractorAudioFile.h"
#include "qtractorMidiTimer.h"
#include "qtractorMidiEditor.h"
#include "qtractorTimeScale.h"
#include "qtractorPlugin.h"

#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"

#include <QFileDialog>
#include <QFontDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QValidator>
#include <QHeaderView>
#include <QUrl>

// Needed for fabs(), logf() and powf()
#include <math.h>

static inline float log10f2 ( float x )
	{ return (x > 0.0f ? 20.0f * ::log10f(x) : -60.0f); }

static inline float pow10f2 ( float x )
	{ return ::powf(10.0f, 0.05f * x); }


// Translatable macro contextualizer.
#undef  _TR
#define _TR(x) QT_TR_NOOP(x)

// Available session formats/ext-suffixes.
static struct
{
	const char *name;
	const char *ext;

} g_aSessionFormats[] = {

	{ _TR("XML Default (*.%1)"), "qtr" },
	{ _TR("XML Regular (*.%1)"), "qts" },
	{ _TR("ZIP Archive (*.%1)"), "qtz" },

	{ NULL, NULL }
};


//----------------------------------------------------------------------------
// qtractorOptionsForm -- UI wrapper form.

// Constructor.
qtractorOptionsForm::qtractorOptionsForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::ApplicationModal);

	// No settings descriptor initially (the caller will set it).
	m_pOptions = NULL;

	// Populate the session format combo-box.
	m_ui.SessionFormatComboBox->clear();
	for (int i = 0; g_aSessionFormats[i].ext; ++i) {
		m_ui.SessionFormatComboBox->addItem(
			tr(g_aSessionFormats[i].name).arg(g_aSessionFormats[i].ext));
	}

	// Populate the capture file type combo-box.
	m_ui.AudioCaptureTypeComboBox->clear();
	int iFormat = 0;
	const qtractorAudioFileFactory::FileFormats& list
		= qtractorAudioFileFactory::formats();
	QListIterator<qtractorAudioFileFactory::FileFormat *> iter(list);
	while (iter.hasNext()) {
		qtractorAudioFileFactory::FileFormat *pFormat = iter.next();
		if (pFormat->type != qtractorAudioFileFactory::MadFile)
			m_ui.AudioCaptureTypeComboBox->addItem(pFormat->name, iFormat);
		++iFormat;
	}

	// Populate the audio capture sample format combo-box.
	m_ui.AudioCaptureFormatComboBox->clear();
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Signed 16-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Signed 24-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Signed 32-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Float  32-Bit"));
	m_ui.AudioCaptureFormatComboBox->addItem(tr("Float  64-Bit"));

	// Populate the MIDI capture file format combo-box.
	m_ui.MidiCaptureFormatComboBox->clear();
	m_ui.MidiCaptureFormatComboBox->addItem(tr("SMF Format 0"));
	m_ui.MidiCaptureFormatComboBox->addItem(tr("SMF Format 1"));

	// Populate the MIDI capture quantize combo-box.
	const QIcon snapIcon(":/images/itemBeat.png");
	const QStringList& snapItems = qtractorTimeScale::snapItems(0);
	QStringListIterator snapIter(snapItems);
	m_ui.MidiCaptureQuantizeComboBox->clear();
	m_ui.MidiCaptureQuantizeComboBox->setIconSize(QSize(8, 16));
//	snapIter.toFront();
	if (snapIter.hasNext())
		m_ui.MidiCaptureQuantizeComboBox->addItem(
			QIcon(":/images/itemNone.png"), snapIter.next());
	while (snapIter.hasNext())
		m_ui.MidiCaptureQuantizeComboBox->addItem(snapIcon, snapIter.next());
//	m_ui.MidiCaptureQuantizeComboBox->insertItems(0, items);

	// Populate the MMC device combo-box.
	m_ui.MidiMmcDeviceComboBox->clear();
	for (unsigned char mmcDevice = 0; mmcDevice < 0x7f; ++mmcDevice)
		m_ui.MidiMmcDeviceComboBox->addItem(QString::number(int(mmcDevice)));
	m_ui.MidiMmcDeviceComboBox->addItem(tr("(Any)"));

//	updateMetroNoteNames();

	// Plugin path types...
	m_ui.PluginTypeComboBox->clear();
#ifdef CONFIG_LADSPA
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Ladspa));
#endif
#ifdef CONFIG_DSSI
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Dssi));
#endif
#ifdef CONFIG_VST
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Vst));
#else
	m_ui.PluginExperimentalGroupBox->hide();
#endif
#ifdef CONFIG_LV2
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Lv2));
#else
	m_ui.Lv2DynManifestCheckBox->hide();
#endif

#ifndef CONFIG_LV2_PRESETS
	m_ui.Lv2PresetDirLabel->hide();
	m_ui.Lv2PresetDirComboBox->hide();
	m_ui.Lv2PresetDirToolButton->hide();
#endif

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.AudioCaptureTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioCaptureFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioCaptureQualitySpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioResampleTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TransportModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioAutoTimeStretchCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioWsolaTimeStretchCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioWsolaQuickSeekCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioPlayerBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioPlayerAutoConnectCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioMetronomeCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBarFilenameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBarFilenameToolButton,
		SIGNAL(clicked()),
		SLOT(chooseMetroBarFilename()));
	QObject::connect(m_ui.MetroBarGainSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBeatFilenameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBeatFilenameToolButton,
		SIGNAL(clicked()),
		SLOT(chooseMetroBeatFilename()));
	QObject::connect(m_ui.MetroBeatGainSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioMetroBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioMetroAutoConnectCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiCaptureFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiCaptureQuantizeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiQueueTimerComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiPlayerBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiMmcModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiMmcDeviceComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiSppModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiClockModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiControlBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiMetronomeCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(updateMetroNoteNames()));
	QObject::connect(m_ui.MetroBarNoteComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBarVelocitySpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBarDurationSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBeatNoteComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBeatVelocitySpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MetroBeatDurationSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiMetroBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ConfirmRemoveCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.StdoutCaptureCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.CompletePathCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.PeakAutoRemoveCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.KeepToolsOnTopCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TrackViewDropSpanCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidButtonModifierCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.LoopRecordingModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.DisplayFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MaxRecentFilesSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.BaseFontSizeComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionTemplateCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionTemplatePathComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionTemplatePathToolButton,
		SIGNAL(clicked()),
		SLOT(chooseSessionTemplatePath()));
	QObject::connect(m_ui.SessionBackupCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionBackupModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionAutoSaveCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionAutoSaveSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioMeterLevelComboBox,
		SIGNAL(activated(int)),
		SLOT(changeAudioMeterLevel(int)));
	QObject::connect(m_ui.MidiMeterLevelComboBox,
		SIGNAL(activated(int)),
		SLOT(changeMidiMeterLevel(int)));
	QObject::connect(m_ui.AudioMeterColorLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changeAudioMeterColor(const QString&)));
	QObject::connect(m_ui.MidiMeterColorLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changeMidiMeterColor(const QString&)));
	QObject::connect(m_ui.AudioMeterColorToolButton,
		SIGNAL(clicked()),
		SLOT(chooseAudioMeterColor()));
	QObject::connect(m_ui.MidiMeterColorToolButton,
		SIGNAL(clicked()),
		SLOT(chooseMidiMeterColor()));
	QObject::connect(m_ui.ResetMeterColorsPushButton,
		SIGNAL(clicked()),
		SLOT(resetMeterColors()));
	QObject::connect(m_ui.PluginTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(choosePluginType(int)));
	QObject::connect(m_ui.PluginPathComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changePluginPath(const QString&)));
	QObject::connect(m_ui.PluginPathBrowseToolButton,
		SIGNAL(clicked()),
		SLOT(choosePluginPath()));
	QObject::connect(m_ui.PluginPathAddToolButton,
		SIGNAL(clicked()),
		SLOT(addPluginPath()));
	QObject::connect(m_ui.PluginPathListWidget,
		SIGNAL(itemSelectionChanged()),
		SLOT(selectPluginPath()));
	QObject::connect(m_ui.PluginPathRemoveToolButton,
		SIGNAL(clicked()),
		SLOT(removePluginPath()));
	QObject::connect(m_ui.PluginPathUpToolButton,
		SIGNAL(clicked()),
		SLOT(moveUpPluginPath()));
	QObject::connect(m_ui.PluginPathDownToolButton,
		SIGNAL(clicked()),
		SLOT(moveDownPluginPath()));
	QObject::connect(m_ui.Lv2PresetDirComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.Lv2PresetDirToolButton,
		SIGNAL(clicked()),
		SLOT(chooseLv2PresetDir()));
	QObject::connect(m_ui.AudioOutputBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioOutputAutoConnectCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.OpenEditorCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.DummyVstScanCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.Lv2DynManifestCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SaveCurve14bitCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MessagesFontPushButton,
		SIGNAL(clicked()),
		SLOT(chooseMessagesFont()));
	QObject::connect(m_ui.MessagesLimitCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MessagesLimitLinesSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MessagesLogCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MessagesLogPathComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.MessagesLogPathToolButton,
		SIGNAL(clicked()),
		SLOT(chooseMessagesLogPath()));
	QObject::connect(m_ui.SyncViewHoldCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.UseNativeDialogsCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorOptionsForm::~qtractorOptionsForm (void)
{
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorOptionsForm::setOptions ( qtractorOptions *pOptions )
{
	// Set reference descriptor.
	m_pOptions = pOptions;

	// Initialize conveniency options...
	m_pOptions->loadComboBoxHistory(m_ui.MetroBarFilenameComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.MetroBeatFilenameComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.PluginPathComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.MessagesLogPathComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.SessionTemplatePathComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.Lv2PresetDirComboBox);

	// Audio options.
	int iIndex  = 0;
	int iFormat = 0;
	const qtractorAudioFileFactory::FileFormats& list
		= qtractorAudioFileFactory::formats();
	QListIterator<qtractorAudioFileFactory::FileFormat *> iter(list);
	while (iter.hasNext()) {
		qtractorAudioFileFactory::FileFormat *pFormat = iter.next();
		if (m_pOptions->sAudioCaptureExt == pFormat->ext
			&& m_pOptions->iAudioCaptureType == pFormat->data) {
			iIndex = m_ui.AudioCaptureTypeComboBox->findData(iFormat);
			break;
		}
		++iFormat;
	}
	m_ui.AudioCaptureTypeComboBox->setCurrentIndex(iIndex);
	m_ui.AudioCaptureFormatComboBox->setCurrentIndex(m_pOptions->iAudioCaptureFormat);
	m_ui.AudioCaptureQualitySpinBox->setValue(m_pOptions->iAudioCaptureQuality);
	m_ui.AudioResampleTypeComboBox->setCurrentIndex(m_pOptions->iAudioResampleType);
	m_ui.TransportModeComboBox->setCurrentIndex(m_pOptions->iTransportMode);
	m_ui.AudioAutoTimeStretchCheckBox->setChecked(m_pOptions->bAudioAutoTimeStretch);
#ifdef CONFIG_LIBRUBBERBAND
	m_ui.AudioWsolaTimeStretchCheckBox->setChecked(m_pOptions->bAudioWsolaTimeStretch);
#else
	m_ui.AudioWsolaTimeStretchCheckBox->setChecked(true);
	m_ui.AudioWsolaTimeStretchCheckBox->setEnabled(false);
#endif
	m_ui.AudioWsolaQuickSeekCheckBox->setChecked(m_pOptions->bAudioWsolaQuickSeek);
	m_ui.AudioPlayerBusCheckBox->setChecked(m_pOptions->bAudioPlayerBus);
	m_ui.AudioPlayerAutoConnectCheckBox->setChecked(m_pOptions->bAudioPlayerAutoConnect);

#ifndef CONFIG_LIBSAMPLERATE
	m_ui.AudioResampleTypeTextLabel->setEnabled(false);
	m_ui.AudioResampleTypeComboBox->setEnabled(false);
#endif

	// Audio metronome options.
	m_ui.AudioMetronomeCheckBox->setChecked(m_pOptions->bAudioMetronome);
	m_ui.MetroBarFilenameComboBox->setEditText(m_pOptions->sMetroBarFilename);
	m_ui.MetroBarGainSpinBox->setValue(log10f2(m_pOptions->fMetroBarGain));
	m_ui.MetroBeatFilenameComboBox->setEditText(m_pOptions->sMetroBeatFilename);
	m_ui.MetroBeatGainSpinBox->setValue(log10f2(m_pOptions->fMetroBeatGain));
	m_ui.AudioMetroBusCheckBox->setChecked(m_pOptions->bAudioMetroBus);
	m_ui.AudioMetroAutoConnectCheckBox->setChecked(m_pOptions->bAudioMetroAutoConnect);

	// MIDI capture/export options.
	m_ui.MidiCaptureFormatComboBox->setCurrentIndex(m_pOptions->iMidiCaptureFormat);
	m_ui.MidiCaptureQuantizeComboBox->setCurrentIndex(m_pOptions->iMidiCaptureQuantize);

	// MIDI playback options.
	qtractorMidiTimer timer;
	m_ui.MidiQueueTimerComboBox->clear();
	for (int i = 0; i < timer.count(); ++i)
		m_ui.MidiQueueTimerComboBox->addItem(timer.name(i), timer.key(i));
	m_ui.MidiQueueTimerComboBox->setCurrentIndex(
		timer.indexOf(m_pOptions->iMidiQueueTimer));
	m_ui.MidiPlayerBusCheckBox->setChecked(m_pOptions->bMidiPlayerBus);

	// MIDI control options.
	m_ui.MidiMmcModeComboBox->setCurrentIndex(m_pOptions->iMidiMmcMode);
	m_ui.MidiMmcDeviceComboBox->setCurrentIndex(m_pOptions->iMidiMmcDevice);
	m_ui.MidiSppModeComboBox->setCurrentIndex(m_pOptions->iMidiSppMode);
	m_ui.MidiClockModeComboBox->setCurrentIndex(m_pOptions->iMidiClockMode);
	m_ui.MidiControlBusCheckBox->setChecked(m_pOptions->bMidiControlBus);

	// MIDI metronome options.
	m_ui.MidiMetronomeCheckBox->setChecked(m_pOptions->bMidiMetronome);
	m_ui.MetroChannelSpinBox->setValue(m_pOptions->iMetroChannel + 1);
	updateMetroNoteNames();
	m_ui.MetroBarNoteComboBox->setCurrentIndex(m_pOptions->iMetroBarNote);
	m_ui.MetroBarVelocitySpinBox->setValue(m_pOptions->iMetroBarVelocity);
	m_ui.MetroBarDurationSpinBox->setValue(m_pOptions->iMetroBarDuration);
	m_ui.MetroBeatNoteComboBox->setCurrentIndex(m_pOptions->iMetroBeatNote);
	m_ui.MetroBeatVelocitySpinBox->setValue(m_pOptions->iMetroBeatVelocity);
	m_ui.MetroBeatDurationSpinBox->setValue(m_pOptions->iMetroBeatDuration);
	m_ui.MidiMetroBusCheckBox->setChecked(m_pOptions->bMidiMetroBus);

	// Custom colors.
	int iColor;
	for (iColor = 0; iColor < AudioMeterColors; ++iColor)
		m_audioMeterColors[iColor] = qtractorAudioMeter::color(iColor);
	for (iColor = 0; iColor < MidiMeterColors; ++iColor)
		m_midiMeterColors[iColor] = qtractorMidiMeter::color(iColor);

	// Default meter color levels shown...
	m_ui.AudioMeterLevelComboBox->setCurrentIndex(AudioMeterColors - 1);
	m_ui.MidiMeterLevelComboBox->setCurrentIndex(MidiMeterColors - 1);
	changeAudioMeterLevel(m_ui.AudioMeterLevelComboBox->currentIndex());
	changeMidiMeterLevel(m_ui.MidiMeterLevelComboBox->currentIndex());

	// Load Display options...
	QFont font;
	// Messages font.
	if (m_pOptions->sMessagesFont.isEmpty()
		|| !font.fromString(m_pOptions->sMessagesFont))
		font = QFont("Monospace", 8);
	QPalette pal(m_ui.MessagesFontTextLabel->palette());
	pal.setColor(QPalette::Background, pal.base().color());
	m_ui.MessagesFontTextLabel->setPalette(pal);
	m_ui.MessagesFontTextLabel->setFont(font);
	m_ui.MessagesFontTextLabel->setText(
		font.family() + " " + QString::number(font.pointSize()));

	// Messages limit option.
	m_ui.MessagesLimitCheckBox->setChecked(m_pOptions->bMessagesLimit);
	m_ui.MessagesLimitLinesSpinBox->setValue(m_pOptions->iMessagesLimitLines);

	// Transport display preferences...
	m_ui.SyncViewHoldCheckBox->setChecked(m_pOptions->bSyncViewHold);

	// Dialogs preferences...
	m_ui.UseNativeDialogsCheckBox->setChecked(m_pOptions->bUseNativeDialogs);

	// Logging options...
	m_ui.MessagesLogCheckBox->setChecked(m_pOptions->bMessagesLog);
	m_ui.MessagesLogPathComboBox->setEditText(m_pOptions->sMessagesLogPath);

	// Other options finally.
	m_ui.ConfirmRemoveCheckBox->setChecked(m_pOptions->bConfirmRemove);
	m_ui.StdoutCaptureCheckBox->setChecked(m_pOptions->bStdoutCapture);
	m_ui.CompletePathCheckBox->setChecked(m_pOptions->bCompletePath);
	m_ui.PeakAutoRemoveCheckBox->setChecked(m_pOptions->bPeakAutoRemove);
	m_ui.KeepToolsOnTopCheckBox->setChecked(m_pOptions->bKeepToolsOnTop);
	m_ui.TrackViewDropSpanCheckBox->setChecked(m_pOptions->bTrackViewDropSpan);
	m_ui.MidButtonModifierCheckBox->setChecked(m_pOptions->bMidButtonModifier);
	m_ui.MaxRecentFilesSpinBox->setValue(m_pOptions->iMaxRecentFiles);
	m_ui.LoopRecordingModeComboBox->setCurrentIndex(m_pOptions->iLoopRecordingMode);
	m_ui.DisplayFormatComboBox->setCurrentIndex(m_pOptions->iDisplayFormat);
	if (m_pOptions->iBaseFontSize > 0)
		m_ui.BaseFontSizeComboBox->setEditText(QString::number(m_pOptions->iBaseFontSize));
	else
		m_ui.BaseFontSizeComboBox->setCurrentIndex(0);

	// Session options...
	m_ui.SessionFormatComboBox->setCurrentIndex(
		sessionFormatFromExt(m_pOptions->sSessionExt));
	m_ui.SessionTemplateCheckBox->setChecked(m_pOptions->bSessionTemplate);
	m_ui.SessionTemplatePathComboBox->setEditText(m_pOptions->sSessionTemplatePath);
	m_ui.SessionBackupCheckBox->setChecked(m_pOptions->bSessionBackup);
	m_ui.SessionBackupModeComboBox->setCurrentIndex(m_pOptions->iSessionBackupMode);
	m_ui.SessionAutoSaveCheckBox->setChecked(m_pOptions->bAutoSaveEnabled);
	m_ui.SessionAutoSaveSpinBox->setValue(m_pOptions->iAutoSavePeriod);

	// Plugin path initialization...
	m_ladspaPaths = m_pOptions->ladspaPaths;
	m_dssiPaths   = m_pOptions->dssiPaths;
	m_vstPaths    = m_pOptions->vstPaths;
	m_lv2Paths    = m_pOptions->lv2Paths;

	m_ui.Lv2PresetDirComboBox->setEditText(m_pOptions->sLv2PresetDir);

	// Plugin instruments options.
	m_ui.AudioOutputBusCheckBox->setChecked(m_pOptions->bAudioOutputBus);
	m_ui.AudioOutputAutoConnectCheckBox->setChecked(m_pOptions->bAudioOutputAutoConnect);
	m_ui.OpenEditorCheckBox->setChecked(m_pOptions->bOpenEditor);
	m_ui.DummyVstScanCheckBox->setChecked(m_pOptions->bDummyVstScan);
	m_ui.Lv2DynManifestCheckBox->setChecked(m_pOptions->bLv2DynManifest);
	m_ui.SaveCurve14bitCheckBox->setChecked(m_pOptions->bSaveCurve14bit);

	int iPluginType = m_pOptions->iPluginType - 1;
	if (iPluginType < 0)
		iPluginType = 0;
	m_ui.PluginTypeComboBox->setCurrentIndex(iPluginType);
	m_ui.PluginPathComboBox->setEditText(QString());

	choosePluginType(iPluginType);

#ifdef CONFIG_DEBUG
	m_ui.StdoutCaptureCheckBox->setEnabled(false);
#endif

	// Done. Restart clean.
	m_iDirtyCount = 0;
	stabilizeForm();
}


// Retrieve the editing options, if the case arises.
qtractorOptions *qtractorOptionsForm::options (void) const
{
	return m_pOptions;
}


// Accept settings (OK button slot).
void qtractorOptionsForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Audio options...
		int iFormat	= m_ui.AudioCaptureTypeComboBox->itemData(
			m_ui.AudioCaptureTypeComboBox->currentIndex()).toInt();
		const qtractorAudioFileFactory::FileFormat *pFormat
			= qtractorAudioFileFactory::formats().at(iFormat);
		m_pOptions->sAudioCaptureExt     = pFormat->ext;
		m_pOptions->iAudioCaptureType    = pFormat->data;
		m_pOptions->iAudioCaptureFormat  = m_ui.AudioCaptureFormatComboBox->currentIndex();
		m_pOptions->iAudioCaptureQuality = m_ui.AudioCaptureQualitySpinBox->value();
		m_pOptions->iAudioResampleType   = m_ui.AudioResampleTypeComboBox->currentIndex();
		m_pOptions->iTransportMode       = m_ui.TransportModeComboBox->currentIndex();
		m_pOptions->bAudioAutoTimeStretch = m_ui.AudioAutoTimeStretchCheckBox->isChecked();
		m_pOptions->bAudioWsolaTimeStretch = m_ui.AudioWsolaTimeStretchCheckBox->isChecked();
		m_pOptions->bAudioWsolaQuickSeek = m_ui.AudioWsolaQuickSeekCheckBox->isChecked();
		m_pOptions->bAudioPlayerBus      = m_ui.AudioPlayerBusCheckBox->isChecked();
		m_pOptions->bAudioPlayerAutoConnect = m_ui.AudioPlayerAutoConnectCheckBox->isChecked();
		// Audio metronome options.
		m_pOptions->bAudioMetronome      = m_ui.AudioMetronomeCheckBox->isChecked();
		m_pOptions->sMetroBarFilename    = m_ui.MetroBarFilenameComboBox->currentText();
		m_pOptions->fMetroBarGain        = pow10f2(m_ui.MetroBarGainSpinBox->value());
		m_pOptions->sMetroBeatFilename   = m_ui.MetroBeatFilenameComboBox->currentText();
		m_pOptions->fMetroBeatGain       = pow10f2(m_ui.MetroBeatGainSpinBox->value());
		m_pOptions->bAudioMetroBus       = m_ui.AudioMetroBusCheckBox->isChecked();
		m_pOptions->bAudioMetroAutoConnect = m_ui.AudioMetroAutoConnectCheckBox->isChecked();
		// MIDI options...
		m_pOptions->iMidiCaptureFormat   = m_ui.MidiCaptureFormatComboBox->currentIndex();
		m_pOptions->iMidiCaptureQuantize = m_ui.MidiCaptureQuantizeComboBox->currentIndex();
		m_pOptions->iMidiQueueTimer      = m_ui.MidiQueueTimerComboBox->itemData(
			m_ui.MidiQueueTimerComboBox->currentIndex()).toInt();
		m_pOptions->bMidiPlayerBus       = m_ui.MidiPlayerBusCheckBox->isChecked();
		m_pOptions->iMidiMmcMode         = m_ui.MidiMmcModeComboBox->currentIndex();
		m_pOptions->iMidiMmcDevice       = m_ui.MidiMmcDeviceComboBox->currentIndex();
		m_pOptions->iMidiSppMode         = m_ui.MidiSppModeComboBox->currentIndex();
		m_pOptions->iMidiClockMode       = m_ui.MidiClockModeComboBox->currentIndex();
		m_pOptions->bMidiControlBus      = m_ui.MidiControlBusCheckBox->isChecked();
		// MIDI metronome options.
		m_pOptions->bMidiMetronome       = m_ui.MidiMetronomeCheckBox->isChecked();
		m_pOptions->iMetroChannel        = m_ui.MetroChannelSpinBox->value() - 1;
		m_pOptions->iMetroBarNote        = m_ui.MetroBarNoteComboBox->currentIndex();
		m_pOptions->iMetroBarVelocity    = m_ui.MetroBarVelocitySpinBox->value();
		m_pOptions->iMetroBarDuration    = m_ui.MetroBarDurationSpinBox->value();
		m_pOptions->iMetroBeatNote       = m_ui.MetroBeatNoteComboBox->currentIndex();
		m_pOptions->iMetroBeatVelocity   = m_ui.MetroBeatVelocitySpinBox->value();
		m_pOptions->iMetroBeatDuration   = m_ui.MetroBeatDurationSpinBox->value();
		m_pOptions->bMidiMetroBus        = m_ui.MidiMetroBusCheckBox->isChecked();
		// Display options...
		m_pOptions->bConfirmRemove       = m_ui.ConfirmRemoveCheckBox->isChecked();
		m_pOptions->bStdoutCapture       = m_ui.StdoutCaptureCheckBox->isChecked();
		m_pOptions->bCompletePath        = m_ui.CompletePathCheckBox->isChecked();
		m_pOptions->bPeakAutoRemove      = m_ui.PeakAutoRemoveCheckBox->isChecked();
		m_pOptions->bKeepToolsOnTop      = m_ui.KeepToolsOnTopCheckBox->isChecked();
		m_pOptions->bTrackViewDropSpan   = m_ui.TrackViewDropSpanCheckBox->isChecked();
		m_pOptions->bMidButtonModifier   = m_ui.MidButtonModifierCheckBox->isChecked();
		m_pOptions->iMaxRecentFiles      = m_ui.MaxRecentFilesSpinBox->value();
		m_pOptions->iLoopRecordingMode   = m_ui.LoopRecordingModeComboBox->currentIndex();
		m_pOptions->iDisplayFormat       = m_ui.DisplayFormatComboBox->currentIndex();
		m_pOptions->iBaseFontSize        = m_ui.BaseFontSizeComboBox->currentText().toInt();
		// Plugin paths...
		m_pOptions->iPluginType          = m_ui.PluginTypeComboBox->currentIndex() + 1;
		m_pOptions->ladspaPaths          = m_ladspaPaths;
		m_pOptions->dssiPaths            = m_dssiPaths;
		m_pOptions->vstPaths             = m_vstPaths;
		m_pOptions->lv2Paths             = m_lv2Paths;
		m_pOptions->sLv2PresetDir        = m_ui.Lv2PresetDirComboBox->currentText();
		// Plugin instruments options.
		m_pOptions->bAudioOutputBus      = m_ui.AudioOutputBusCheckBox->isChecked();
		m_pOptions->bAudioOutputAutoConnect = m_ui.AudioOutputAutoConnectCheckBox->isChecked();
		m_pOptions->bOpenEditor          = m_ui.OpenEditorCheckBox->isChecked();
		m_pOptions->bDummyVstScan        = m_ui.DummyVstScanCheckBox->isChecked();
		m_pOptions->bLv2DynManifest      = m_ui.Lv2DynManifestCheckBox->isChecked();
		m_pOptions->bSaveCurve14bit      = m_ui.SaveCurve14bitCheckBox->isChecked();
		// Messages options...
		m_pOptions->sMessagesFont        = m_ui.MessagesFontTextLabel->font().toString();
		m_pOptions->bMessagesLimit       = m_ui.MessagesLimitCheckBox->isChecked();
		m_pOptions->iMessagesLimitLines  = m_ui.MessagesLimitLinesSpinBox->value();
		// Logging options...
		m_pOptions->bMessagesLog         = m_ui.MessagesLogCheckBox->isChecked();
		m_pOptions->sMessagesLogPath     = m_ui.MessagesLogPathComboBox->currentText();
		// Session options...
		m_pOptions->bSessionTemplate     = m_ui.SessionTemplateCheckBox->isChecked();
		m_pOptions->sSessionTemplatePath = m_ui.SessionTemplatePathComboBox->currentText();
		m_pOptions->sSessionExt = sessionExtFromFormat(
			m_ui.SessionFormatComboBox->currentIndex());
		m_pOptions->bSessionBackup       = m_ui.SessionBackupCheckBox->isChecked();
		m_pOptions->iSessionBackupMode   = m_ui.SessionBackupModeComboBox->currentIndex();
		m_pOptions->bAutoSaveEnabled     = m_ui.SessionAutoSaveCheckBox->isChecked();
		m_pOptions->iAutoSavePeriod      = m_ui.SessionAutoSaveSpinBox->value();
		// Custom colors.
		int iColor;
		for (iColor = 0; iColor < AudioMeterColors; ++iColor)
			qtractorAudioMeter::setColor(iColor, m_audioMeterColors[iColor]);
		for (iColor = 0; iColor < MidiMeterColors; ++iColor)
			qtractorMidiMeter::setColor(iColor, m_midiMeterColors[iColor]);
		// Transport display preferences...
		m_pOptions->bSyncViewHold = m_ui.SyncViewHoldCheckBox->isChecked();
		// Dialogs preferences...
		m_pOptions->bUseNativeDialogs = m_ui.UseNativeDialogsCheckBox->isChecked();
		m_pOptions->bDontUseNativeDialogs = !m_pOptions->bUseNativeDialogs;
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Save other conveniency options...
	m_pOptions->saveComboBoxHistory(m_ui.MetroBarFilenameComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.MetroBeatFilenameComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.PluginPathComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.MessagesLogPathComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.SessionTemplatePathComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.Lv2PresetDirComboBox);

	// Save/commit to disk.
	m_pOptions->saveOptions();

	// Just go with dialog acceptance
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorOptionsForm::reject (void)
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
			tr("Some settings have been changed.\n\n"
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


// Dirty up settings.
void qtractorOptionsForm::changed (void)
{
	++m_iDirtyCount;
	stabilizeForm();
}


// Choose audio metronome filenames.
void qtractorOptionsForm::chooseMetroBarFilename (void)
{
	QString sFilename = getOpenAudioFileName(
		tr("Metronome Bar Audio File") + " - " QTRACTOR_TITLE,
		m_ui.MetroBarFilenameComboBox->currentText());

	if (sFilename.isEmpty())
		return;

	m_ui.MetroBarFilenameComboBox->setEditText(sFilename);

	changed();
}

void qtractorOptionsForm::chooseMetroBeatFilename (void)
{
	QString sFilename = getOpenAudioFileName(
		tr("Metronome Beat Audio File") + " - " QTRACTOR_TITLE,
		m_ui.MetroBeatFilenameComboBox->currentText());

	if (sFilename.isEmpty())
		return;

	m_ui.MetroBeatFilenameComboBox->setEditText(sFilename);

	changed();
}


// The metronome note names changer.
void qtractorOptionsForm::updateMetroNoteNames (void)
{
	// Save current selection...
	int iOldBarNote  = m_ui.MetroBarNoteComboBox->currentIndex();
	int iOldBeatNote = m_ui.MetroBeatNoteComboBox->currentIndex();

	// Populate the Metronome notes.
	m_ui.MetroBarNoteComboBox->clear();
	m_ui.MetroBeatNoteComboBox->clear();
	bool bDrums = (m_ui.MetroChannelSpinBox->value() == 10);
	QStringList items;
	const QString sItem("%1 (%2)");
	for (int i = 0; i < 128; ++i) {
		items.append(sItem
			.arg(qtractorMidiEditor::defaultNoteName(i, bDrums)).arg(i));
	}
	m_ui.MetroBarNoteComboBox->insertItems(0, items);
	m_ui.MetroBeatNoteComboBox->insertItems(0, items);

	// Restore old selection...
	m_ui.MetroBarNoteComboBox->setCurrentIndex(iOldBarNote);
	m_ui.MetroBeatNoteComboBox->setCurrentIndex(iOldBeatNote);

	changed();
}


// Audio meter level index change.
void qtractorOptionsForm::changeAudioMeterLevel ( int iColor )
{
	const QColor& color = m_audioMeterColors[iColor];
	m_ui.AudioMeterColorLineEdit->setText(color.name());
}


// MIDI meter level index change.
void qtractorOptionsForm::changeMidiMeterLevel ( int iColor )
{
	const QColor& color = m_midiMeterColors[iColor];
	m_ui.MidiMeterColorLineEdit->setText(color.name());
}


// Audio meter color change.
void qtractorOptionsForm::changeAudioMeterColor ( const QString& sColor )
{
	const QColor& color = QColor(sColor);
	if (color.isValid()) {
		updateColorText(m_ui.AudioMeterColorLineEdit, color);
		m_audioMeterColors[m_ui.AudioMeterLevelComboBox->currentIndex()] = color;
		changed();
	}
}


// MIDI meter color change.
void qtractorOptionsForm::changeMidiMeterColor ( const QString& sColor )
{
	const QColor& color = QColor(sColor);
	if (color.isValid()) {
		updateColorText(m_ui.MidiMeterColorLineEdit, color);
		m_midiMeterColors[m_ui.MidiMeterLevelComboBox->currentIndex()] = color;
		changed();
	}
}


// Audio meter color selection.
void qtractorOptionsForm::chooseAudioMeterColor (void)
{
	const QColor& color = QColorDialog::getColor(
		QColor(m_ui.AudioMeterColorLineEdit->text()), this,
		tr("Audio Meter Color") + " - " QTRACTOR_TITLE);

	if (color.isValid())
		m_ui.AudioMeterColorLineEdit->setText(color.name());
}


// Midi meter color selection.
void qtractorOptionsForm::chooseMidiMeterColor (void)
{
	const QColor& color = QColorDialog::getColor(
		QColor(m_ui.MidiMeterColorLineEdit->text()), this,
		tr("MIDI Meter Color") + " - " QTRACTOR_TITLE);

	if (color.isValid())
		m_ui.MidiMeterColorLineEdit->setText(color.name());
}


// Reset all meter colors from default.
void qtractorOptionsForm::resetMeterColors (void)
{
	// Reset colors.
	int iColor;
	for (iColor = 0; iColor < AudioMeterColors; ++iColor)
		m_audioMeterColors[iColor] = qtractorAudioMeter::defaultColor(iColor);
	for (iColor = 0; iColor < MidiMeterColors; ++iColor)
		m_midiMeterColors[iColor] = qtractorMidiMeter::defaultColor(iColor);

	// Update current display...
	changeAudioMeterLevel(m_ui.AudioMeterLevelComboBox->currentIndex());
	changeMidiMeterLevel(m_ui.MidiMeterLevelComboBox->currentIndex());
}


// Update color item visual text.
void qtractorOptionsForm::updateColorText (
	QLineEdit *pLineEdit, const QColor& color )
{
	QPalette pal(color);
	pal.setColor(QPalette::Base, color);
	pLineEdit->setPalette(pal);
}


// Change plugin type.
void qtractorOptionsForm::choosePluginType ( int iPluginType )
{
	if (m_pOptions == NULL)
		return;

	QStringList paths;
	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->itemText(iPluginType));

	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		paths = m_ladspaPaths;
		break;
	case qtractorPluginType::Dssi:
		paths = m_dssiPaths;
		break;
	case qtractorPluginType::Vst:
		paths = m_vstPaths;
		break;
	case qtractorPluginType::Lv2:
		paths = m_lv2Paths;
		// Fall thru...
	default:
		break;
	}

	m_ui.PluginPathListWidget->clear();
	QStringListIterator iter(paths);
	while (iter.hasNext())
		m_ui.PluginPathListWidget->addItem(iter.next());

	selectPluginPath();
	stabilizeForm();
}


// Change plugin path.
void qtractorOptionsForm::changePluginPath ( const QString& /*sPluginPath*/ )
{
	selectPluginPath();
	stabilizeForm();
}


// Browse for plugin path.
void qtractorOptionsForm::choosePluginPath (void)
{
	QString sPluginPath;

	const QString& sTitle = tr("Plug-in Directory") + " - " QTRACTOR_TITLE;
#if 1//QT_VERSION < 0x040400
	// Ask for the directory...
	QFileDialog::Options options = QFileDialog::ShowDirsOnly;
	if (m_pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
    sPluginPath = QFileDialog::getExistingDirectory(this,
		sTitle, m_ui.PluginPathComboBox->currentText(), options);
#else
	// Construct open-directory dialog...
	QFileDialog fileDialog(this,
		sTitle, m_ui.PluginPathComboBox->currentText());
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::DirectoryOnly);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	if (m_pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sPluginPath = fileDialog.selectedFiles().first();
#endif

	if (!sPluginPath.isEmpty()) {
		m_ui.PluginPathComboBox->setEditText(sPluginPath);
		m_ui.PluginPathComboBox->setFocus();
	}

	selectPluginPath();
	stabilizeForm();
}


// Add chosen plugin path.
void qtractorOptionsForm::addPluginPath (void)
{
	const QString& sPluginPath = m_ui.PluginPathComboBox->currentText();
	if (sPluginPath.isEmpty())
		return;

	if (!QDir(sPluginPath).exists())
		return;

	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->currentText());
	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		m_ladspaPaths.append(sPluginPath);
		break;
	case qtractorPluginType::Dssi:
		m_dssiPaths.append(sPluginPath);
		break;
	case qtractorPluginType::Vst:
		m_vstPaths.append(sPluginPath);
		break;
	case qtractorPluginType::Lv2:
		m_lv2Paths.append(sPluginPath);
		break;
	default:
		return;
	}

	m_ui.PluginPathListWidget->addItem(sPluginPath);
	m_ui.PluginPathListWidget->setCurrentRow(
		m_ui.PluginPathListWidget->count() - 1);

	int i = m_ui.PluginPathComboBox->findText(sPluginPath);
	if (i >= 0)
		m_ui.PluginPathComboBox->removeItem(i);
	m_ui.PluginPathComboBox->insertItem(0, sPluginPath);
	m_ui.PluginPathComboBox->setEditText(QString());

	m_ui.PluginPathListWidget->setFocus();

	selectPluginPath();
	changed();
}


// Select current plugin path.
void qtractorOptionsForm::selectPluginPath (void)
{
	int iPluginPath = m_ui.PluginPathListWidget->currentRow();

	m_ui.PluginPathRemoveToolButton->setEnabled(iPluginPath >= 0);
	m_ui.PluginPathUpToolButton->setEnabled(iPluginPath > 0);
	m_ui.PluginPathDownToolButton->setEnabled(iPluginPath >= 0
		&& iPluginPath < m_ui.PluginPathListWidget->count() - 1);
}


// Remove current plugin path.
void qtractorOptionsForm::removePluginPath (void)
{
	int iPluginPath = m_ui.PluginPathListWidget->currentRow();
	if (iPluginPath < 0)
		return;

	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->currentText());
	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		m_ladspaPaths.removeAt(iPluginPath);
		break;
	case qtractorPluginType::Dssi:
		m_dssiPaths.removeAt(iPluginPath);
		break;
	case qtractorPluginType::Vst:
		m_vstPaths.removeAt(iPluginPath);
		break;
	case qtractorPluginType::Lv2:
		m_lv2Paths.removeAt(iPluginPath);
		break;
	default:
		return;
	}

	QListWidgetItem *pItem = m_ui.PluginPathListWidget->takeItem(iPluginPath);
	if (pItem)
		delete pItem;

	selectPluginPath();
	changed();
}


// Move up plugin path on search order.
void qtractorOptionsForm::moveUpPluginPath (void)
{
	int iPluginPath = m_ui.PluginPathListWidget->currentRow();
	if (iPluginPath < 1)
		return;

	QString sPluginPath;
	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->currentText());
	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		sPluginPath = m_ladspaPaths.takeAt(iPluginPath);
		m_ladspaPaths.insert(iPluginPath - 1, sPluginPath);
		break;
	case qtractorPluginType::Dssi:
		sPluginPath = m_dssiPaths.takeAt(iPluginPath);
		m_dssiPaths.insert(iPluginPath - 1, sPluginPath);
		break;
	case qtractorPluginType::Vst:
		sPluginPath = m_vstPaths.takeAt(iPluginPath);
		m_vstPaths.insert(iPluginPath - 1, sPluginPath);
		break;
	case qtractorPluginType::Lv2:
		sPluginPath = m_lv2Paths.takeAt(iPluginPath);
		m_lv2Paths.insert(iPluginPath - 1, sPluginPath);
		break;
	default:
		return;
	}

	QListWidgetItem *pItem = m_ui.PluginPathListWidget->takeItem(iPluginPath);
	if (pItem) {
		pItem->setSelected(false);
		m_ui.PluginPathListWidget->insertItem(iPluginPath - 1, pItem);
		pItem->setSelected(true);
		m_ui.PluginPathListWidget->setCurrentItem(pItem);
	}

	selectPluginPath();
	changed();
}


// Move down plugin path on search order.
void qtractorOptionsForm::moveDownPluginPath (void)
{
	int iPluginPath = m_ui.PluginPathListWidget->currentRow();
	if (iPluginPath >= m_ui.PluginPathListWidget->count() - 1)
		return;

	QString sPluginPath;
	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->currentText());
	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		sPluginPath = m_ladspaPaths.takeAt(iPluginPath);
		m_ladspaPaths.insert(iPluginPath + 1, sPluginPath);
		break;
	case qtractorPluginType::Dssi:
		sPluginPath = m_dssiPaths.takeAt(iPluginPath);
		m_dssiPaths.insert(iPluginPath + 1, sPluginPath);
		break;
	case qtractorPluginType::Vst:
		sPluginPath = m_vstPaths.takeAt(iPluginPath);
		m_vstPaths.insert(iPluginPath + 1, sPluginPath);
		break;
	case qtractorPluginType::Lv2:
		sPluginPath = m_lv2Paths.takeAt(iPluginPath);
		m_lv2Paths.insert(iPluginPath + 1, sPluginPath);
		break;
	default:
		return;
	}

	QListWidgetItem *pItem = m_ui.PluginPathListWidget->takeItem(iPluginPath);
	if (pItem) {
		pItem->setSelected(false);
		m_ui.PluginPathListWidget->insertItem(iPluginPath + 1, pItem);
		pItem->setSelected(true);
		m_ui.PluginPathListWidget->setCurrentItem(pItem);
	}

	selectPluginPath();
	changed();
}


// Browse for LV2 Presets directory.
void qtractorOptionsForm::chooseLv2PresetDir (void)
{
	QString sLv2PresetDir = m_ui.Lv2PresetDirComboBox->currentText();
	if (sLv2PresetDir.isEmpty())
		sLv2PresetDir = QDir::homePath() + QDir::separator() + ".lv2";

	const QString& sTitle = tr("LV2 Presets Directory") + " - " QTRACTOR_TITLE;
#if 1// QT_VERSION < 0x040400
	// Ask for the directory...
	QFileDialog::Options options = QFileDialog::ShowDirsOnly;
	if (m_pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sLv2PresetDir = QFileDialog::getExistingDirectory(this,
		sTitle, sLv2PresetDir, options);
#else
	// Construct open-directory dialog...
	QFileDialog fileDialog(this,
		sTitle, sLv2PresetDir);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::DirectoryOnly);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sLv2PresetDir));
	fileDialog.setSidebarUrls(urls);
	if (m_pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sLv2PresetDir = fileDialog.selectedFiles().first();
#endif

	if (!sLv2PresetDir.isEmpty()) {
		m_ui.Lv2PresetDirComboBox->setEditText(sLv2PresetDir);
		m_ui.Lv2PresetDirComboBox->setFocus();
	}

	changed();
}


// The messages font selection dialog.
void qtractorOptionsForm::chooseMessagesFont (void)
{
	bool  bOk  = false;
	QFont font = QFontDialog::getFont(&bOk,
		m_ui.MessagesFontTextLabel->font(), this);
	if (bOk) {
		m_ui.MessagesFontTextLabel->setFont(font);
		m_ui.MessagesFontTextLabel->setText(
			font.family() + " " + QString::number(font.pointSize()));
		changed();
	}
}


// Messages log path browse slot.
void qtractorOptionsForm::chooseMessagesLogPath (void)
{
	QString sFilename;

	const QString  sExt("log");
	const QString& sTitle  = tr("Messages Log") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Log files (*.%1)").arg(sExt); 
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (m_pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sFilename = QFileDialog::getSaveFileName(this,
		sTitle, m_ui.MessagesLogPathComboBox->currentText(), sFilter, NULL, options);
#else
	// Construct open-file dialog...
	QFileDialog fileDialog(this,
		sTitle, m_ui.MessagesLogPathComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	if (m_pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif

	if (!sFilename.isEmpty()) {
		m_ui.MessagesLogPathComboBox->setEditText(sFilename);
		m_ui.MessagesLogPathComboBox->setFocus();
		changed();
	}
}


// Session template path browse slot.
void qtractorOptionsForm::chooseSessionTemplatePath (void)
{
	QString sFilename;

	const QString  sExt("qtt");
	const QString& sTitle  = tr("Session Template") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Session template files (*.qtr *.qts *.%1)").arg(sExt); 
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (m_pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sFilename = QFileDialog::getOpenFileName(this,
		sTitle, m_ui.SessionTemplatePathComboBox->currentText(), sFilter, NULL, options);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(this,
		sTitle, m_ui.SessionTemplatePathComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	if (m_pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif

	if (sFilename.isEmpty())
		return;

	m_ui.SessionTemplatePathComboBox->setEditText(sFilename);
	m_ui.SessionTemplatePathComboBox->setFocus();
	changed();
}


// Stabilize current form state.
void qtractorOptionsForm::stabilizeForm (void)
{
	m_ui.MessagesLimitLinesSpinBox->setEnabled(
		m_ui.MessagesLimitCheckBox->isChecked());

	// Audio options validy check...
	int iIndex  = m_ui.AudioCaptureTypeComboBox->currentIndex();
	int iFormat	= m_ui.AudioCaptureTypeComboBox->itemData(iIndex).toInt();
	const qtractorAudioFileFactory::FileFormat *pFormat
		= qtractorAudioFileFactory::formats().at(iFormat);

	bool bSndFile
		= (pFormat && pFormat->type == qtractorAudioFileFactory::SndFile);
	m_ui.AudioCaptureFormatTextLabel->setEnabled(bSndFile);
	m_ui.AudioCaptureFormatComboBox->setEnabled(bSndFile);

	bool bVorbisFile
		= (pFormat && pFormat->type == qtractorAudioFileFactory::VorbisFile);
	m_ui.AudioCaptureQualityTextLabel->setEnabled(bVorbisFile);
	m_ui.AudioCaptureQualitySpinBox->setEnabled(bVorbisFile);

	bool bValid = (m_iDirtyCount > 0);
	if (bValid) {
		iFormat = m_ui.AudioCaptureFormatComboBox->currentIndex();
		bValid  = qtractorAudioFileFactory::isValidFormat(pFormat, iFormat);
	}

	m_ui.AudioWsolaQuickSeekCheckBox->setEnabled(
		m_ui.AudioWsolaTimeStretchCheckBox->isChecked());

	m_ui.AudioPlayerAutoConnectCheckBox->setEnabled(
		m_ui.AudioPlayerBusCheckBox->isChecked());

	bool bAudioMetronome = m_ui.AudioMetronomeCheckBox->isChecked();
	m_ui.MetroBarFilenameTextLabel->setEnabled(bAudioMetronome);
	m_ui.MetroBarFilenameComboBox->setEnabled(bAudioMetronome);
	m_ui.MetroBarFilenameToolButton->setEnabled(bAudioMetronome);
	m_ui.MetroBarGainTextLabel->setEnabled(bAudioMetronome);
	m_ui.MetroBarGainSpinBox->setEnabled(bAudioMetronome);
	m_ui.MetroBeatFilenameTextLabel->setEnabled(bAudioMetronome);
	m_ui.MetroBeatFilenameComboBox->setEnabled(bAudioMetronome);
	m_ui.MetroBeatFilenameToolButton->setEnabled(bAudioMetronome);
	m_ui.MetroBeatGainTextLabel->setEnabled(bAudioMetronome);
	m_ui.MetroBeatGainSpinBox->setEnabled(bAudioMetronome);
	m_ui.AudioMetroBusCheckBox->setEnabled(bAudioMetronome);

	m_ui.AudioMetroAutoConnectCheckBox->setEnabled(
		bAudioMetronome && m_ui.AudioMetroBusCheckBox->isChecked());

	bool bMmcMode =(m_ui.MidiMmcModeComboBox->currentIndex() > 0);
	m_ui.MidiMmcDeviceTextLabel->setEnabled(bMmcMode);
	m_ui.MidiMmcDeviceComboBox->setEnabled(bMmcMode);

	bool bMidiMetronome = m_ui.MidiMetronomeCheckBox->isChecked();
	m_ui.MetroChannelTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroChannelSpinBox->setEnabled(bMidiMetronome);
	m_ui.MetroBarNoteTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroBarNoteComboBox->setEnabled(bMidiMetronome);
	m_ui.MetroBarVelocityTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroBarVelocitySpinBox->setEnabled(bMidiMetronome);
	m_ui.MetroBarDurationTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroBarDurationSpinBox->setEnabled(bMidiMetronome);
	m_ui.MetroBeatNoteTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroBeatNoteComboBox->setEnabled(bMidiMetronome);
	m_ui.MetroBeatVelocityTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroBeatVelocitySpinBox->setEnabled(bMidiMetronome);
	m_ui.MetroBeatDurationTextLabel->setEnabled(bMidiMetronome);
	m_ui.MetroBeatDurationSpinBox->setEnabled(bMidiMetronome);
	m_ui.MidiMetroBusCheckBox->setEnabled(bMidiMetronome);

	bool bMessagesLog = m_ui.MessagesLogCheckBox->isChecked();
	m_ui.MessagesLogPathComboBox->setEnabled(bMessagesLog);
	m_ui.MessagesLogPathToolButton->setEnabled(bMessagesLog);
	if (bMessagesLog && bValid) {
		const QString& sPath = m_ui.MessagesLogPathComboBox->currentText();
		bValid = !sPath.isEmpty();
	}

	bool bSessionTemplate = m_ui.SessionTemplateCheckBox->isChecked();
	m_ui.SessionTemplatePathComboBox->setEnabled(bSessionTemplate);
	m_ui.SessionTemplatePathToolButton->setEnabled(bSessionTemplate);
	if (bSessionTemplate && bValid) {
		const QString& sPath = m_ui.SessionTemplatePathComboBox->currentText();
		bValid = !sPath.isEmpty() && QFileInfo(sPath).exists();
	}

	m_ui.SessionBackupModeComboBox->setEnabled(
		m_ui.SessionBackupCheckBox->isChecked());

	m_ui.SessionAutoSaveSpinBox->setEnabled(
		m_ui.SessionAutoSaveCheckBox->isChecked());

	const QString& sPluginPath = m_ui.PluginPathComboBox->currentText();
	m_ui.PluginPathAddToolButton->setEnabled(
		!sPluginPath.isEmpty() && QDir(sPluginPath).exists()
		&& m_ui.PluginPathListWidget->findItems(
			sPluginPath, Qt::MatchExactly).isEmpty());

	if (bValid) {
		const QString& sLv2PresetDir = m_ui.Lv2PresetDirComboBox->currentText();
		bValid = sLv2PresetDir.isEmpty() || QDir(sLv2PresetDir).exists();
	}

	m_ui.AudioOutputAutoConnectCheckBox->setEnabled(
		m_ui.AudioOutputBusCheckBox->isChecked());

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);
}


// Browse for an existing audio filename.
QString qtractorOptionsForm::getOpenAudioFileName (
	const QString& sTitle, const QString& sFilename )
{
	QString sAudioFile;

#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	QFileDialog::Options options = 0;
	if (m_pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sAudioFile = QFileDialog::getOpenFileName(this,
		sTitle, sFilename, qtractorAudioFileFactory::filters(), NULL, options);
#else
	// Construct open-file dialog...
	QFileDialog fileDialog(this,
		sTitle, sFilename, qtractorAudioFileFactory::filters());
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(qtractorAudioFileFactory::defaultExt());
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(m_pOptions->sAudioDir));
	fileDialog.setSidebarUrls(urls);
	if (m_pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sAudioFile = fileDialog.selectedFiles().first();
#endif

	return sAudioFile;
}


// Session format ext/suffix helpers.
int qtractorOptionsForm::sessionFormatFromExt ( const QString& sSessionExt ) const
{
	int iSessionFormat = 0;

	for (int i = 1; g_aSessionFormats[i].ext; ++i) {
		if (g_aSessionFormats[i].ext == sSessionExt) {
			iSessionFormat = i;
			break;
		}
	}

	return iSessionFormat;
}

QString qtractorOptionsForm::sessionExtFromFormat ( int iSessionFormat ) const
{
	if (iSessionFormat < 0 || iSessionFormat > 2)
		iSessionFormat = 0;

	return g_aSessionFormats[iSessionFormat].ext;
}


// end of qtractorOptionsForm.cpp

