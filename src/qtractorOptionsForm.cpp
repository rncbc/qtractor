// qtractorOptionsForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorSession.h"

#include "qtractorPluginFactory.h"

#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"

#include "qtractorPluginSelectForm.h"

#include "qtractorPaletteForm.h"

#include <QFileDialog>
#include <QFontDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QValidator>
#include <QHeaderView>
#include <QUrl>

#include <QStyleFactory>


// Needed for logf() and powf()
#include <cmath>

static inline float log10f2 ( float x )
	{ return (x > 0.0f ? 20.0f * ::log10f(x) : -60.0f); }

static inline float pow10f2 ( float x )
	{ return ::powf(10.0f, 0.05f * x); }


//----------------------------------------------------------------------------
// Available session formats/ext-suffixes.

static QHash<QString, QString> g_sessionFormats;


void qtractorOptionsForm::initSessionFormats (void)
{
	static struct SessionFormat
	{
		const char *name;
		const char *ext;

	} s_aSessionFormats[] = {

		{ QT_TR_NOOP("XML Default (*.%1)"), "qtr" },
		{ QT_TR_NOOP("XML Regular (*.%1)"), "qts" },
		{ QT_TR_NOOP("ZIP Archive (*.%1)"), "qtz" },

		{ nullptr, nullptr }
	};

	if (g_sessionFormats.isEmpty()) {
		for (int i = 0; s_aSessionFormats[i].name; ++i) {
			SessionFormat& sf = s_aSessionFormats[i];
			g_sessionFormats.insert(sf.ext, tr(sf.name));
		}
	}

}


// Default (empty/blank) name.
static const char *g_pszDefName = QT_TRANSLATE_NOOP("qtractorOptionsForm", "(default)");


//----------------------------------------------------------------------------
// qtractorOptionsForm -- UI wrapper form.

// Constructor.
qtractorOptionsForm::qtractorOptionsForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);
#if QT_VERSION < QT_VERSION_CHECK(6, 1, 0)
	QDialog::setWindowIcon(QIcon(":/images/qtractor.png"));
#endif
	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::ApplicationModal);

	// No settings descriptor initially (the caller will set it).
	m_pOptions = nullptr;

	// Have some deafult time-scale for instance...
	m_pTimeScale = nullptr;

	// Meter colors array setup.
	m_paAudioMeterColors = new QColor [qtractorAudioMeter::ColorCount - 1];
	m_paMidiMeterColors  = new QColor [qtractorMidiMeter::ColorCount  - 1];

	int iColor;
	for (iColor = 0; iColor < qtractorAudioMeter::ColorCount - 1; ++iColor)
		m_paAudioMeterColors[iColor] = qtractorAudioMeter::defaultColor(iColor);
	for (iColor = 0; iColor < qtractorMidiMeter::ColorCount - 1; ++iColor)
		m_paMidiMeterColors[iColor] = qtractorMidiMeter::defaultColor(iColor);

	m_iDirtyMeterColors = 0;

	// Time-scale and audio metronome setup.
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.AudioMetroOffsetSpinBox->setTimeScale(m_pTimeScale);
		m_ui.AudioMetroOffsetSpinBox->setMaximum(m_pTimeScale->sampleRate());
		m_ui.AudioMetroOffsetSpinBox->setDeltaValue(true);
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	// Some conveniency cleaner helpers...
	m_ui.SessionTemplatePathComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.MetroBarFilenameComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.MetroBeatFilenameComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.MessagesLogPathComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.PluginPathComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.PluginBlacklistComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.Lv2PresetDirComboBox->lineEdit()->setClearButtonEnabled(true);
#endif

	// Populate the session format combo-box.
	initSessionFormats();

	m_ui.SessionFormatComboBox->clear();
	QHash<QString, QString>::ConstIterator sf_iter
		= g_sessionFormats.constBegin();
	const QHash<QString, QString>::ConstIterator& sf_end
		= g_sessionFormats.constEnd();
	int iSessionFormat = 0;
	for ( ; sf_iter != sf_end; ++sf_iter) {
		const QString& sSessionExt = sf_iter.key();
		const QString& sSessionFormat = sf_iter.value();
		m_ui.SessionFormatComboBox->insertItem(
			iSessionFormat++,
			sSessionFormat.arg(sSessionExt),
			sSessionExt);
	}

	// Populate the MIDI capture quantize combo-box.
	const QIcon& snapIcon = QIcon::fromTheme("itemBeat");
	const QStringList& snapItems = qtractorTimeScale::snapItems();
	QStringListIterator snapIter(snapItems);
	m_ui.MidiCaptureQuantizeComboBox->clear();
	m_ui.MidiCaptureQuantizeComboBox->setIconSize(QSize(8, 16));
//	snapIter.toFront();
	if (snapIter.hasNext())
		m_ui.MidiCaptureQuantizeComboBox->addItem(
			QIcon::fromTheme("itemNone"), snapIter.next());
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
#ifdef CONFIG_VST2
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Vst2));
#endif
#ifdef CONFIG_VST3
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Vst3));
#endif
#ifdef CONFIG_CLAP
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Clap));
#endif
#ifdef CONFIG_LV2
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Lv2));
#endif

#ifndef CONFIG_LV2_PRESETS
	m_ui.Lv2PresetDirLabel->hide();
	m_ui.Lv2PresetDirComboBox->hide();
	m_ui.Lv2PresetDirToolButton->hide();
#endif

	// Initialize dirty control state.
	m_iDirtyCount = 0;
	m_iDirtyCustomColorThemes = 0;

	m_iDirtyLadspaPaths = 0;
	m_iDirtyDssiPaths   = 0;
	m_iDirtyVst2Paths   = 0;
	m_iDirtyVst3Paths   = 0;
	m_iDirtyClapPaths   = 0;
	m_iDirtyLv2Paths    = 0;

	m_iDirtyBlacklist   = 0;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.AudioCaptureTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(audioCaptureTypeChanged(int)));
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
	QObject::connect(m_ui.TimebaseCheckBox,
		SIGNAL(stateChanged(int)),
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
	QObject::connect(m_ui.AudioCountInModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioCountInBeatsSpinBox,
		SIGNAL(valueChanged(int)),
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
	QObject::connect(m_ui.AudioMetroOffsetSpinBox,
		SIGNAL(valueChanged(unsigned long)),
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
	QObject::connect(m_ui.MidiDriftCorrectCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiPlayerBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiResetAllControllersCheckBox,
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
	QObject::connect(m_ui.MidiCountInModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.MidiCountInBeatsSpinBox,
		SIGNAL(valueChanged(int)),
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
	QObject::connect(m_ui.MidiMetroOffsetSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ConfirmRemoveCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ConfirmArchiveCheckBox,
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
	QObject::connect(m_ui.KeepEditorsOnTopCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TrackViewDropSpanCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ShiftKeyModifierCheckBox,
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
		SLOT(displayFormatChanged(int)));
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
	QObject::connect(m_ui.CustomColorThemeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.CustomColorThemeToolButton,
		SIGNAL(clicked()),
		SLOT(editCustomColorThemes()));
	QObject::connect(m_ui.CustomStyleThemeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.CustomStyleSheetComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.CustomStyleSheetToolButton,
		SIGNAL(clicked()),
		SLOT(chooseCustomStyleSheet()));
	QObject::connect(m_ui.CustomIconsThemeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.CustomIconsThemeToolButton,
		SIGNAL(clicked()),
		SLOT(chooseCustomIconsTheme()));
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
	QObject::connect(m_ui.PluginPathToolButton,
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
#ifdef CONFIG_LV2_PRESETS
	QObject::connect(m_ui.Lv2PresetDirComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.Lv2PresetDirToolButton,
		SIGNAL(clicked()),
		SLOT(chooseLv2PresetDir()));
#endif
	QObject::connect(m_ui.AudioOutputBusCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.AudioOutputAutoConnectCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.OpenEditorCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QueryEditorTypeCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.PluginBlacklistComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changePluginBlacklist(const QString&)));
	QObject::connect(m_ui.PluginBlacklistToolButton,
		SIGNAL(clicked()),
		SLOT(choosePluginBlacklist()));
	QObject::connect(m_ui.PluginBlacklistAddToolButton,
		SIGNAL(clicked()),
		SLOT(addPluginBlacklist()));
	QObject::connect(m_ui.PluginBlacklistWidget,
		SIGNAL(itemSelectionChanged()),
		SLOT(selectPluginBlacklist()));
	QObject::connect(m_ui.PluginBlacklistRemoveToolButton,
		SIGNAL(clicked()),
		SLOT(removePluginBlacklist()));
	QObject::connect(m_ui.PluginBlacklistClearToolButton,
		SIGNAL(clicked()),
		SLOT(clearPluginBlacklist()));
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
	QObject::connect(m_ui.TrackColorSaturationSpinBox,
		SIGNAL(valueChanged(int)),
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
	delete [] m_paMidiMeterColors;
	delete [] m_paAudioMeterColors;

	if (m_pTimeScale) delete m_pTimeScale;
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorOptionsForm::setOptions ( qtractorOptions *pOptions )
{
	// Set reference descriptor.
	m_pOptions = pOptions;

	// Initialize conveniency options...
	m_pOptions->loadComboBoxHistory(m_ui.MetroBarFilenameComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.MetroBeatFilenameComboBox);
	m_pOptions->loadComboBoxFileHistory(m_ui.CustomStyleSheetComboBox);
	m_pOptions->loadComboBoxFileHistory(m_ui.CustomIconsThemeComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.PluginPathComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.MessagesLogPathComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.SessionTemplatePathComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.Lv2PresetDirComboBox);
	m_pOptions->loadComboBoxHistory(m_ui.PluginBlacklistComboBox);

	// Time-scale related options...
	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(m_pOptions->iDisplayFormat);
	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	// Audio options.
	m_ui.AudioCaptureTypeComboBox->setCurrentType(
		m_pOptions->sAudioCaptureExt, m_pOptions->iAudioCaptureType);
	m_ui.AudioCaptureFormatComboBox->setCurrentIndex(m_pOptions->iAudioCaptureFormat);
	m_ui.AudioCaptureQualitySpinBox->setValue(m_pOptions->iAudioCaptureQuality);
	m_ui.AudioResampleTypeComboBox->setCurrentIndex(m_pOptions->iAudioResampleType);
	m_ui.TransportModeComboBox->setCurrentIndex(m_pOptions->iTransportMode);
	m_ui.TimebaseCheckBox->setChecked(m_pOptions->bTimebase);
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

	// Have some audio metronome default sample files...
	//
	const QChar sep = QDir::separator();
	QString sAudioPath = QApplication::applicationDirPath();
	sAudioPath.remove(CONFIG_BINDIR);
	sAudioPath.append(CONFIG_DATADIR);
	sAudioPath.append(sep);
	sAudioPath.append(PROJECT_NAME);
	sAudioPath.append(sep);
	sAudioPath.append("audio");

	const QFileInfo metro_bar(sAudioPath, "metro_bar.wav");
	if (metro_bar.isFile() && metro_bar.isReadable()) {
		const QString& sMetroBarFilename
			= metro_bar.absoluteFilePath();
		if (m_pOptions->sMetroBarFilename.isEmpty())
			m_pOptions->sMetroBarFilename = sMetroBarFilename;
		if (m_ui.MetroBarFilenameComboBox->findText(sMetroBarFilename) < 0)
			m_ui.MetroBarFilenameComboBox->addItem(sMetroBarFilename);
	}

	const QFileInfo metro_beat(sAudioPath, "metro_beat.wav");
	if (metro_beat.isFile() && metro_beat.isReadable()) {
		const QString& sMetroBeatFilename
			= metro_beat.absoluteFilePath();
		if (m_pOptions->sMetroBeatFilename.isEmpty())
			m_pOptions->sMetroBeatFilename = sMetroBeatFilename;
		if (m_ui.MetroBeatFilenameComboBox->findText(sMetroBeatFilename) < 0)
			m_ui.MetroBeatFilenameComboBox->addItem(sMetroBeatFilename);
	}

	// Audio metronome options.
	m_ui.AudioMetronomeCheckBox->setChecked(m_pOptions->bAudioMetronome);
	m_ui.AudioCountInModeComboBox->setCurrentIndex(m_pOptions->iAudioCountInMode);
	m_ui.AudioCountInBeatsSpinBox->setValue(m_pOptions->iAudioCountInBeats);
	m_ui.MetroBarFilenameComboBox->setEditText(m_pOptions->sMetroBarFilename);
	m_ui.MetroBarGainSpinBox->setValue(log10f2(m_pOptions->fMetroBarGain));
	m_ui.MetroBeatFilenameComboBox->setEditText(m_pOptions->sMetroBeatFilename);
	m_ui.MetroBeatGainSpinBox->setValue(log10f2(m_pOptions->fMetroBeatGain));
	m_ui.AudioMetroBusCheckBox->setChecked(m_pOptions->bAudioMetroBus);
	m_ui.AudioMetroAutoConnectCheckBox->setChecked(m_pOptions->bAudioMetroAutoConnect);
	m_ui.AudioMetroOffsetSpinBox->setDisplayFormat(displayFormat);
	m_ui.AudioMetroOffsetSpinBox->setValue(m_pOptions->iAudioMetroOffset);

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
	m_ui.MidiDriftCorrectCheckBox->setChecked(m_pOptions->bMidiDriftCorrect);
	m_ui.MidiPlayerBusCheckBox->setChecked(m_pOptions->bMidiPlayerBus);
	m_ui.MidiResetAllControllersCheckBox->setChecked(m_pOptions->bMidiResetAllControllers);

	// MIDI control options.
	m_ui.MidiMmcModeComboBox->setCurrentIndex(m_pOptions->iMidiMmcMode);
	m_ui.MidiMmcDeviceComboBox->setCurrentIndex(m_pOptions->iMidiMmcDevice);
	m_ui.MidiSppModeComboBox->setCurrentIndex(m_pOptions->iMidiSppMode);
	m_ui.MidiClockModeComboBox->setCurrentIndex(m_pOptions->iMidiClockMode);
	m_ui.MidiControlBusCheckBox->setChecked(m_pOptions->bMidiControlBus);

	// MIDI metronome options.
	m_ui.MidiMetronomeCheckBox->setChecked(m_pOptions->bMidiMetronome);
	m_ui.MidiCountInModeComboBox->setCurrentIndex(m_pOptions->iMidiCountInMode);
	m_ui.MidiCountInBeatsSpinBox->setValue(m_pOptions->iMidiCountInBeats);
	m_ui.MetroChannelSpinBox->setValue(m_pOptions->iMetroChannel + 1);
	updateMetroNoteNames();
	m_ui.MetroBarNoteComboBox->setCurrentIndex(m_pOptions->iMetroBarNote);
	m_ui.MetroBarVelocitySpinBox->setValue(m_pOptions->iMetroBarVelocity);
	m_ui.MetroBarDurationSpinBox->setValue(m_pOptions->iMetroBarDuration);
	m_ui.MetroBeatNoteComboBox->setCurrentIndex(m_pOptions->iMetroBeatNote);
	m_ui.MetroBeatVelocitySpinBox->setValue(m_pOptions->iMetroBeatVelocity);
	m_ui.MetroBeatDurationSpinBox->setValue(m_pOptions->iMetroBeatDuration);
	m_ui.MidiMetroBusCheckBox->setChecked(m_pOptions->bMidiMetroBus);
	m_ui.MidiMetroOffsetSpinBox->setValue(m_pOptions->iMidiMetroOffset);

	// Meter colors array setup.
	int iColor;
	for (iColor = 0; iColor < qtractorAudioMeter::ColorCount - 1; ++iColor)
		m_paAudioMeterColors[iColor] = qtractorAudioMeter::color(iColor);
	for (iColor = 0; iColor < qtractorMidiMeter::ColorCount - 1; ++iColor)
		m_paMidiMeterColors[iColor] = qtractorMidiMeter::color(iColor);

	// Default meter color levels shown...
	m_ui.AudioMeterLevelComboBox->setCurrentIndex(qtractorAudioMeter::Color10dB);
	m_ui.MidiMeterLevelComboBox->setCurrentIndex(qtractorMidiMeter::ColorOver);
	changeAudioMeterLevel(m_ui.AudioMeterLevelComboBox->currentIndex());
	changeMidiMeterLevel(m_ui.MidiMeterLevelComboBox->currentIndex());

	// Custom display options...
	resetCustomColorThemes(m_pOptions->sCustomColorTheme);
	resetCustomStyleThemes(m_pOptions->sCustomStyleTheme);

	m_pOptions->setComboBoxCurrentFile(
		m_ui.CustomStyleSheetComboBox,
		m_pOptions->sCustomStyleSheet);
	m_pOptions->setComboBoxCurrentFile(
		m_ui.CustomIconsThemeComboBox,
		m_pOptions->sCustomIconsTheme);

	// Load Display options...
	QFont font;
	// Messages font.
	if (m_pOptions->sMessagesFont.isEmpty()
		|| !font.fromString(m_pOptions->sMessagesFont))
		font = QFont("Monospace", 8);
	QPalette pal(m_ui.MessagesFontTextLabel->palette());
	pal.setColor(QPalette::Window, pal.base().color());
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

	// Default track color saturation issue..
	m_ui.TrackColorSaturationSpinBox->setValue(m_pOptions->iTrackColorSaturation);

	// Logging options...
	m_ui.MessagesLogCheckBox->setChecked(m_pOptions->bMessagesLog);
	m_ui.MessagesLogPathComboBox->setEditText(m_pOptions->sMessagesLogPath);

	// Other options finally.
	m_ui.ConfirmRemoveCheckBox->setChecked(m_pOptions->bConfirmRemove);
	m_ui.ConfirmArchiveCheckBox->setChecked(m_pOptions->bConfirmArchive);
	m_ui.StdoutCaptureCheckBox->setChecked(m_pOptions->bStdoutCapture);
	m_ui.CompletePathCheckBox->setChecked(m_pOptions->bCompletePath);
	m_ui.PeakAutoRemoveCheckBox->setChecked(m_pOptions->bPeakAutoRemove);
	m_ui.KeepToolsOnTopCheckBox->setChecked(m_pOptions->bKeepToolsOnTop);
	m_ui.KeepEditorsOnTopCheckBox->setChecked(m_pOptions->bKeepEditorsOnTop);
	m_ui.TrackViewDropSpanCheckBox->setChecked(m_pOptions->bTrackViewDropSpan);
	m_ui.ShiftKeyModifierCheckBox->setChecked(m_pOptions->bShiftKeyModifier);
	m_ui.MidButtonModifierCheckBox->setChecked(m_pOptions->bMidButtonModifier);
	m_ui.MaxRecentFilesSpinBox->setValue(m_pOptions->iMaxRecentFiles);
	m_ui.LoopRecordingModeComboBox->setCurrentIndex(m_pOptions->iLoopRecordingMode);
	m_ui.DisplayFormatComboBox->setCurrentIndex(m_pOptions->iDisplayFormat);
	if (m_pOptions->iBaseFontSize > 0)
		m_ui.BaseFontSizeComboBox->setEditText(QString::number(m_pOptions->iBaseFontSize));
	else
		m_ui.BaseFontSizeComboBox->setCurrentIndex(0);

	// Session options...
	int iSessionFormat = m_ui.SessionFormatComboBox->findData(m_pOptions->sSessionExt);
	if (iSessionFormat < 0)
		iSessionFormat = 0;
	m_ui.SessionFormatComboBox->setCurrentIndex(iSessionFormat);
	m_ui.SessionTemplateCheckBox->setChecked(m_pOptions->bSessionTemplate);
	m_ui.SessionTemplatePathComboBox->setEditText(m_pOptions->sSessionTemplatePath);
	m_ui.SessionBackupCheckBox->setChecked(m_pOptions->bSessionBackup);
	m_ui.SessionBackupModeComboBox->setCurrentIndex(m_pOptions->iSessionBackupMode);
	m_ui.SessionAutoSaveCheckBox->setChecked(m_pOptions->bAutoSaveEnabled);
	m_ui.SessionAutoSaveSpinBox->setValue(m_pOptions->iAutoSavePeriod);

	// Plugin path initialization...
	m_ladspaPaths = m_pOptions->ladspaPaths;
	m_dssiPaths   = m_pOptions->dssiPaths;
	m_vst2Paths   = m_pOptions->vst2Paths;
	m_vst3Paths   = m_pOptions->vst3Paths;
	m_lv2Paths    = m_pOptions->lv2Paths;

	m_ui.Lv2PresetDirComboBox->setEditText(m_pOptions->sLv2PresetDir);

	qtractorPluginFactory *pPluginFactory = qtractorPluginFactory::getInstance();
	if (pPluginFactory) {
	#ifdef CONFIG_LADSPA
		if (m_ladspaPaths.isEmpty())
			m_ladspaPaths = pPluginFactory->pluginPaths(qtractorPluginType::Ladspa);
	#endif
	#ifdef CONFIG_DSSI
		if (m_dssiPaths.isEmpty())
			m_dssiPaths = pPluginFactory->pluginPaths(qtractorPluginType::Dssi);
	#endif
	#ifdef CONFIG_VST2
		if (m_vst2Paths.isEmpty())
			m_vst2Paths = pPluginFactory->pluginPaths(qtractorPluginType::Vst2);
	#endif
	#ifdef CONFIG_VST3
		if (m_vst3Paths.isEmpty())
			m_vst3Paths = pPluginFactory->pluginPaths(qtractorPluginType::Vst3);
	#endif
	#ifdef CONFIG_CLAP
		if (m_clapPaths.isEmpty())
			m_clapPaths = pPluginFactory->pluginPaths(qtractorPluginType::Clap);
	#endif
	#ifdef CONFIG_LV2
		if (m_lv2Paths.isEmpty())
			m_lv2Paths = pPluginFactory->pluginPaths(qtractorPluginType::Lv2);
	#endif
	}

	// Plugin instruments options.
	m_ui.AudioOutputBusCheckBox->setChecked(m_pOptions->bAudioOutputBus);
	m_ui.AudioOutputAutoConnectCheckBox->setChecked(m_pOptions->bAudioOutputAutoConnect);
	m_ui.OpenEditorCheckBox->setChecked(m_pOptions->bOpenEditor);
	m_ui.QueryEditorTypeCheckBox->setChecked(m_pOptions->bQueryEditorType);

	int iPluginType = m_pOptions->iPluginType - 1;
	if (iPluginType < 0)
		iPluginType = 0;
	m_ui.PluginTypeComboBox->setCurrentIndex(iPluginType);
	m_ui.PluginPathComboBox->setEditText(QString());

	m_ui.PluginBlacklistWidget->clear();
	m_ui.PluginBlacklistWidget->addItems(pPluginFactory->blacklist());
	m_ui.PluginBlacklistComboBox->setEditText(QString());

	choosePluginType(iPluginType);
	selectPluginBlacklist();

#ifdef CONFIG_DEBUG
	m_ui.StdoutCaptureCheckBox->setEnabled(false);
#endif

	// Done. Restart clean.
	m_iDirtyCount = 0;

	m_iDirtyLadspaPaths = 0;
	m_iDirtyDssiPaths   = 0;
	m_iDirtyVst2Paths   = 0;
	m_iDirtyVst3Paths   = 0;
	m_iDirtyClapPaths   = 0;
	m_iDirtyLv2Paths    = 0;

	m_iDirtyBlacklist   = 0;

	stabilizeForm();
}


// Retrieve the editing options, if the case arises.
qtractorOptions *qtractorOptionsForm::options (void) const
{
	return m_pOptions;
}


// Spacial meter colors dirty flag.
bool qtractorOptionsForm::isDirtyMeterColors (void) const
{
	return (m_iDirtyMeterColors > 0);
}


// Special custom color themes dirty flag.
bool qtractorOptionsForm::isDirtyCustomColorThemes (void) const
{
	return (m_iDirtyCustomColorThemes > 0);
}

// Accept settings (OK button slot).
void qtractorOptionsForm::accept (void)
{
	// Save options...
	if (m_iDirtyCount > 0) {
		// Audio options...
		const void *handle = m_ui.AudioCaptureTypeComboBox->currentHandle();
		m_pOptions->sAudioCaptureExt     = m_ui.AudioCaptureTypeComboBox->currentExt(handle);
		m_pOptions->iAudioCaptureType    = m_ui.AudioCaptureTypeComboBox->currentType(handle);
		m_pOptions->iAudioCaptureFormat  = m_ui.AudioCaptureFormatComboBox->currentIndex();
		m_pOptions->iAudioCaptureQuality = m_ui.AudioCaptureQualitySpinBox->value();
		m_pOptions->iAudioResampleType   = m_ui.AudioResampleTypeComboBox->currentIndex();
		m_pOptions->iTransportMode       = m_ui.TransportModeComboBox->currentIndex();
		m_pOptions->bTimebase            = m_ui.TimebaseCheckBox->isChecked();
		m_pOptions->bAudioAutoTimeStretch = m_ui.AudioAutoTimeStretchCheckBox->isChecked();
		m_pOptions->bAudioWsolaTimeStretch = m_ui.AudioWsolaTimeStretchCheckBox->isChecked();
		m_pOptions->bAudioWsolaQuickSeek = m_ui.AudioWsolaQuickSeekCheckBox->isChecked();
		m_pOptions->bAudioPlayerBus      = m_ui.AudioPlayerBusCheckBox->isChecked();
		m_pOptions->bAudioPlayerAutoConnect = m_ui.AudioPlayerAutoConnectCheckBox->isChecked();
		// Audio metronome options.
		m_pOptions->bAudioMetronome      = m_ui.AudioMetronomeCheckBox->isChecked();
		m_pOptions->iAudioCountInMode    = m_ui.AudioCountInModeComboBox->currentIndex();
		m_pOptions->iAudioCountInBeats   = m_ui.AudioCountInBeatsSpinBox->value();
		m_pOptions->sMetroBarFilename    = m_ui.MetroBarFilenameComboBox->currentText();
		m_pOptions->fMetroBarGain        = pow10f2(m_ui.MetroBarGainSpinBox->value());
		m_pOptions->sMetroBeatFilename   = m_ui.MetroBeatFilenameComboBox->currentText();
		m_pOptions->fMetroBeatGain       = pow10f2(m_ui.MetroBeatGainSpinBox->value());
		m_pOptions->bAudioMetroBus       = m_ui.AudioMetroBusCheckBox->isChecked();
		m_pOptions->bAudioMetroAutoConnect = m_ui.AudioMetroAutoConnectCheckBox->isChecked();
		m_pOptions->iAudioMetroOffset    = m_ui.AudioMetroOffsetSpinBox->value();
		// MIDI options...
		m_pOptions->iMidiCaptureFormat   = m_ui.MidiCaptureFormatComboBox->currentIndex();
		m_pOptions->iMidiCaptureQuantize = m_ui.MidiCaptureQuantizeComboBox->currentIndex();
		m_pOptions->iMidiQueueTimer      = m_ui.MidiQueueTimerComboBox->itemData(
			m_ui.MidiQueueTimerComboBox->currentIndex()).toInt();
		m_pOptions->bMidiDriftCorrect    = m_ui.MidiDriftCorrectCheckBox->isChecked();
		m_pOptions->bMidiPlayerBus       = m_ui.MidiPlayerBusCheckBox->isChecked();
		m_pOptions->bMidiResetAllControllers = m_ui.MidiResetAllControllersCheckBox->isChecked();
		m_pOptions->iMidiMmcMode         = m_ui.MidiMmcModeComboBox->currentIndex();
		m_pOptions->iMidiMmcDevice       = m_ui.MidiMmcDeviceComboBox->currentIndex();
		m_pOptions->iMidiSppMode         = m_ui.MidiSppModeComboBox->currentIndex();
		m_pOptions->iMidiClockMode       = m_ui.MidiClockModeComboBox->currentIndex();
		m_pOptions->bMidiControlBus      = m_ui.MidiControlBusCheckBox->isChecked();
		// MIDI metronome options.
		m_pOptions->bMidiMetronome       = m_ui.MidiMetronomeCheckBox->isChecked();
		m_pOptions->iMidiCountInMode     = m_ui.MidiCountInModeComboBox->currentIndex();
		m_pOptions->iMidiCountInBeats    = m_ui.MidiCountInBeatsSpinBox->value();
		m_pOptions->iMetroChannel        = m_ui.MetroChannelSpinBox->value() - 1;
		m_pOptions->iMetroBarNote        = m_ui.MetroBarNoteComboBox->currentIndex();
		m_pOptions->iMetroBarVelocity    = m_ui.MetroBarVelocitySpinBox->value();
		m_pOptions->iMetroBarDuration    = m_ui.MetroBarDurationSpinBox->value();
		m_pOptions->iMetroBeatNote       = m_ui.MetroBeatNoteComboBox->currentIndex();
		m_pOptions->iMetroBeatVelocity   = m_ui.MetroBeatVelocitySpinBox->value();
		m_pOptions->iMetroBeatDuration   = m_ui.MetroBeatDurationSpinBox->value();
		m_pOptions->bMidiMetroBus        = m_ui.MidiMetroBusCheckBox->isChecked();
		m_pOptions->iMidiMetroOffset     = m_ui.MidiMetroOffsetSpinBox->value();
		// Display options...
		m_pOptions->bConfirmRemove       = m_ui.ConfirmRemoveCheckBox->isChecked();
		m_pOptions->bConfirmArchive      = m_ui.ConfirmArchiveCheckBox->isChecked();
		m_pOptions->bStdoutCapture       = m_ui.StdoutCaptureCheckBox->isChecked();
		m_pOptions->bCompletePath        = m_ui.CompletePathCheckBox->isChecked();
		m_pOptions->bPeakAutoRemove      = m_ui.PeakAutoRemoveCheckBox->isChecked();
		m_pOptions->bKeepToolsOnTop      = m_ui.KeepToolsOnTopCheckBox->isChecked();
		m_pOptions->bKeepEditorsOnTop    = m_ui.KeepEditorsOnTopCheckBox->isChecked();
		m_pOptions->bTrackViewDropSpan   = m_ui.TrackViewDropSpanCheckBox->isChecked();
		m_pOptions->bShiftKeyModifier    = m_ui.ShiftKeyModifierCheckBox->isChecked();
		m_pOptions->bMidButtonModifier   = m_ui.MidButtonModifierCheckBox->isChecked();
		m_pOptions->iMaxRecentFiles      = m_ui.MaxRecentFilesSpinBox->value();
		m_pOptions->iLoopRecordingMode   = m_ui.LoopRecordingModeComboBox->currentIndex();
		m_pOptions->iDisplayFormat       = m_ui.DisplayFormatComboBox->currentIndex();
		m_pOptions->iBaseFontSize        = m_ui.BaseFontSizeComboBox->currentText().toInt();
		// Plugin paths...
		m_pOptions->iPluginType          = m_ui.PluginTypeComboBox->currentIndex() + 1;
		if (m_iDirtyLadspaPaths > 0)
			m_pOptions->ladspaPaths      = m_ladspaPaths;
		if (m_iDirtyDssiPaths > 0)
			m_pOptions->dssiPaths        = m_dssiPaths;
		if (m_iDirtyVst2Paths > 0)
			m_pOptions->vst2Paths        = m_vst2Paths;
		if (m_iDirtyVst3Paths > 0)
			m_pOptions->vst3Paths        = m_vst3Paths;
		if (m_iDirtyClapPaths > 0)
			m_pOptions->clapPaths        = m_clapPaths;
		if (m_iDirtyLv2Paths > 0) {
			m_pOptions->lv2Paths         = m_lv2Paths;
			m_pOptions->sLv2PresetDir    = m_ui.Lv2PresetDirComboBox->currentText();
		}
		// Plugin instruments options.
		m_pOptions->bAudioOutputBus      = m_ui.AudioOutputBusCheckBox->isChecked();
		m_pOptions->bAudioOutputAutoConnect = m_ui.AudioOutputAutoConnectCheckBox->isChecked();
		m_pOptions->bOpenEditor          = m_ui.OpenEditorCheckBox->isChecked();
		m_pOptions->bQueryEditorType     = m_ui.QueryEditorTypeCheckBox->isChecked();
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
		const int iSessionFormat         = m_ui.SessionFormatComboBox->currentIndex();
		m_pOptions->sSessionExt          = m_ui.SessionFormatComboBox->itemData(iSessionFormat).toString();
		m_pOptions->bSessionBackup       = m_ui.SessionBackupCheckBox->isChecked();
		m_pOptions->iSessionBackupMode   = m_ui.SessionBackupModeComboBox->currentIndex();
		m_pOptions->bAutoSaveEnabled     = m_ui.SessionAutoSaveCheckBox->isChecked();
		m_pOptions->iAutoSavePeriod      = m_ui.SessionAutoSaveSpinBox->value();
		// Custom colors.
		int iColor;
		for (iColor = 0; iColor < qtractorAudioMeter::ColorCount - 1; ++iColor)
			qtractorAudioMeter::setColor(iColor, m_paAudioMeterColors[iColor]);
		for (iColor = 0; iColor < qtractorMidiMeter::ColorCount - 1; ++iColor)
			qtractorMidiMeter::setColor(iColor, m_paMidiMeterColors[iColor]);
		// Transport display preferences...
		m_pOptions->bSyncViewHold = m_ui.SyncViewHoldCheckBox->isChecked();
		// Dialogs preferences...
		m_pOptions->bUseNativeDialogs = m_ui.UseNativeDialogsCheckBox->isChecked();
		m_pOptions->bDontUseNativeDialogs = !m_pOptions->bUseNativeDialogs;
		// Default track color saturation issue..
		m_pOptions->iTrackColorSaturation = m_ui.TrackColorSaturationSpinBox->value();
		// Custom options..
		if (m_ui.CustomColorThemeComboBox->currentIndex() > 0)
			m_pOptions->sCustomColorTheme = m_ui.CustomColorThemeComboBox->currentText();
		else
			m_pOptions->sCustomColorTheme.clear();
		if (m_ui.CustomStyleThemeComboBox->currentIndex() > 0)
			m_pOptions->sCustomStyleTheme = m_ui.CustomStyleThemeComboBox->currentText();
		else
			m_pOptions->sCustomStyleTheme.clear();
		m_pOptions->sCustomStyleSheet = m_pOptions->comboBoxCurrentFile(m_ui.CustomStyleSheetComboBox);
		m_pOptions->sCustomIconsTheme = m_pOptions->comboBoxCurrentFile(m_ui.CustomIconsThemeComboBox);
		// Reset dirty flags.
		qtractorPluginFactory *pPluginFactory
			= qtractorPluginFactory::getInstance();
		if (pPluginFactory) {
			if (m_iDirtyLadspaPaths > 0) {
				pPluginFactory->updatePluginPaths(qtractorPluginType::Ladspa);
				pPluginFactory->clearAll(qtractorPluginType::Ladspa);
			}
			if (m_iDirtyDssiPaths > 0){
				pPluginFactory->updatePluginPaths(qtractorPluginType::Dssi);
				pPluginFactory->clearAll(qtractorPluginType::Dssi);
			}
			if (m_iDirtyVst2Paths > 0){
				pPluginFactory->updatePluginPaths(qtractorPluginType::Vst2);
				pPluginFactory->clearAll(qtractorPluginType::Vst2);
			}
			if (m_iDirtyVst3Paths > 0){
				pPluginFactory->updatePluginPaths(qtractorPluginType::Vst3);
				pPluginFactory->clearAll(qtractorPluginType::Vst3);
			}
			if (m_iDirtyClapPaths > 0){
				pPluginFactory->updatePluginPaths(qtractorPluginType::Clap);
				pPluginFactory->clearAll(qtractorPluginType::Clap);
			}
			if (m_iDirtyLv2Paths > 0){
				pPluginFactory->updatePluginPaths(qtractorPluginType::Lv2);
				pPluginFactory->clearAll(qtractorPluginType::Lv2);
			}
			if (m_iDirtyBlacklist > 0) {
				QStringList blacklist;
				const int iBlacklistCount
					= m_ui.PluginBlacklistWidget->count();
				for (int iBlacklist = 0; iBlacklist < iBlacklistCount; ++iBlacklist) {
					const QListWidgetItem *pItem
						= m_ui.PluginBlacklistWidget->item(iBlacklist);
					if (pItem) blacklist.append(pItem->text());
				}
				pPluginFactory->setBlacklist(blacklist);
			}
		}
		m_iDirtyCount = 0;
	}

	// Save other conveniency options...
	m_pOptions->saveComboBoxHistory(m_ui.MetroBarFilenameComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.MetroBeatFilenameComboBox);
	m_pOptions->saveComboBoxFileHistory(m_ui.CustomStyleSheetComboBox);
	m_pOptions->saveComboBoxFileHistory(m_ui.CustomIconsThemeComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.PluginPathComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.MessagesLogPathComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.SessionTemplatePathComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.Lv2PresetDirComboBox);
	m_pOptions->saveComboBoxHistory(m_ui.PluginBlacklistComboBox);

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
			tr("Warning"),
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


// Audio file type changed.
void qtractorOptionsForm::audioCaptureTypeChanged ( int iIndex )
{
	const void *handle
		= m_ui.AudioCaptureTypeComboBox->handleOf(iIndex);
	const qtractorAudioFileFactory::FileFormat *pFormat
		= static_cast<const qtractorAudioFileFactory::FileFormat *> (handle);
	if (handle && pFormat) {
		const bool bBlockSignals
			= m_ui.AudioCaptureFormatComboBox->blockSignals(true);
		int iFormat = m_ui.AudioCaptureFormatComboBox->currentIndex();
		while (iFormat > 0 && // Retry down to PCM Signed 16-Bit...
			!qtractorAudioFileFactory::isValidFormat(pFormat, iFormat))
			--iFormat;
		m_ui.AudioCaptureFormatComboBox->setCurrentIndex(iFormat);
		m_ui.AudioCaptureFormatComboBox->blockSignals(bBlockSignals);
	}

	changed();
}


// Choose audio metronome filenames.
void qtractorOptionsForm::chooseMetroBarFilename (void)
{
	QString sFilename = getOpenAudioFileName(
		tr("Metronome Bar Audio File"),
		m_ui.MetroBarFilenameComboBox->currentText());

	if (sFilename.isEmpty())
		return;

	m_ui.MetroBarFilenameComboBox->setEditText(sFilename);

	changed();
}

void qtractorOptionsForm::chooseMetroBeatFilename (void)
{
	QString sFilename = getOpenAudioFileName(
		tr("Metronome Beat Audio File"),
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
	const int iOldBarNote  = m_ui.MetroBarNoteComboBox->currentIndex();
	const int iOldBeatNote = m_ui.MetroBeatNoteComboBox->currentIndex();

	// Populate the Metronome notes.
	m_ui.MetroBarNoteComboBox->clear();
	m_ui.MetroBeatNoteComboBox->clear();
	const bool bDrums = (m_ui.MetroChannelSpinBox->value() == 10);
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


// Display format has changed.
void qtractorOptionsForm::displayFormatChanged ( int iDisplayFormat )
{
	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.AudioMetroOffsetSpinBox->setDisplayFormat(displayFormat);

	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	changed();
}


// Custom color palette theme manager.
void qtractorOptionsForm::editCustomColorThemes (void)
{
	qtractorPaletteForm form(this);
	form.setSettings(&m_pOptions->settings());

	QString sCustomColorTheme;
	int iDirtyCustomColorTheme = 0;

	const int iCustomColorTheme
		= m_ui.CustomColorThemeComboBox->currentIndex();
	if (iCustomColorTheme > 0) {
		sCustomColorTheme = m_ui.CustomColorThemeComboBox->itemText(iCustomColorTheme);
		form.setPaletteName(sCustomColorTheme);
	}

	if (form.exec() == QDialog::Accepted) {
		sCustomColorTheme = form.paletteName();
		++iDirtyCustomColorTheme;
	}

	if (iDirtyCustomColorTheme > 0 || form.isDirty()) {
		++m_iDirtyCustomColorThemes;
		resetCustomColorThemes(sCustomColorTheme);
		changed();
	}
}


// Custom color palette themes settler.
void qtractorOptionsForm::resetCustomColorThemes (
	const QString& sCustomColorTheme )
{
	m_ui.CustomColorThemeComboBox->clear();
	m_ui.CustomColorThemeComboBox->addItem(
		tr(g_pszDefName));
	m_ui.CustomColorThemeComboBox->addItems(
		qtractorPaletteForm::namedPaletteList(&m_pOptions->settings()));

	int iCustomColorTheme = 0;
	if (!sCustomColorTheme.isEmpty()) {
		iCustomColorTheme = m_ui.CustomColorThemeComboBox->findText(
			sCustomColorTheme);
		if (iCustomColorTheme < 0)
			iCustomColorTheme = 0;
	}
	m_ui.CustomColorThemeComboBox->setCurrentIndex(iCustomColorTheme);
}


// Custom widget style themes settler.
void qtractorOptionsForm::resetCustomStyleThemes (
	const QString& sCustomStyleTheme )
{
	m_ui.CustomStyleThemeComboBox->clear();
	m_ui.CustomStyleThemeComboBox->addItem(
		tr(g_pszDefName));
	m_ui.CustomStyleThemeComboBox->addItems(
		QStyleFactory::keys());

	int iCustomStyleTheme = 0;
	if (!sCustomStyleTheme.isEmpty()) {
		iCustomStyleTheme = m_ui.CustomStyleThemeComboBox->findText(
			sCustomStyleTheme);
		if (iCustomStyleTheme < 0)
			iCustomStyleTheme = 0;
	}
	m_ui.CustomStyleThemeComboBox->setCurrentIndex(iCustomStyleTheme);
}


void qtractorOptionsForm::chooseCustomStyleSheet (void)
{
	if (m_pOptions == nullptr)
		return;

	QString sFilename = m_ui.CustomStyleSheetComboBox->currentText();

	const QString  sExt("qss");
	const QString& sTitle  = tr("Open Style Sheet");

	QStringList filters;
	filters.append(tr("Style Sheet files (*.%1)").arg(sExt));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, sFilename, sFilter, nullptr, options);
#else
	QFileDialog fileDialog(pParentWidget,
		sTitle, sFilename, sFilter);
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFiles);
	fileDialog.setDefaultSuffix(sExt);
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif

	if (sFilename.isEmpty())
		return;

	if (m_pOptions->setComboBoxCurrentFile(
			m_ui.CustomStyleSheetComboBox, sFilename)) {
		changed();
	}
}


// Custom icons theme (directory) chooser.
void qtractorOptionsForm::chooseCustomIconsTheme (void)
{
	if (m_pOptions == nullptr)
		return;

	QString sIconsDir = m_ui.CustomIconsThemeComboBox->currentText();

	const QString& sTitle
		= tr("Icons Theme Directory");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options = QFileDialog::ShowDirsOnly;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1// QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the directory...
	sIconsDir = QFileDialog::getExistingDirectory(pParentWidget,
		sTitle, sIconsDir, options);
#else
	// Construct open-directory dialog...
	QFileDialog fileDialog(pParentWidget, sTitle, sIconsDir);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::DirectoryOnly);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sIconsDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sLv2PresetDir = fileDialog.selectedFiles().first();
#endif

	if (sIconsDir.isEmpty())
		return;

	// TODO: Make sure the chosen directory has some image files...
	//
	if (m_pOptions->setComboBoxCurrentFile(
			m_ui.CustomIconsThemeComboBox, sIconsDir)) {
		changed();
	}
}


// Audio meter level index change.
void qtractorOptionsForm::changeAudioMeterLevel ( int iColor )
{
	const QColor& color = m_paAudioMeterColors[iColor];
	m_ui.AudioMeterColorLineEdit->setText(color.name());
}


// MIDI meter level index change.
void qtractorOptionsForm::changeMidiMeterLevel ( int iColor )
{
	const QColor& color = m_paMidiMeterColors[iColor];
	m_ui.MidiMeterColorLineEdit->setText(color.name());
}


// Audio meter color change.
void qtractorOptionsForm::changeAudioMeterColor ( const QString& sColor )
{
	const QColor& color = QColor(sColor);
	if (color.isValid()) {
		updateColorText(m_ui.AudioMeterColorLineEdit, color);
		m_paAudioMeterColors[m_ui.AudioMeterLevelComboBox->currentIndex()] = color;
		++m_iDirtyMeterColors;
		changed();
	}
}


// MIDI meter color change.
void qtractorOptionsForm::changeMidiMeterColor ( const QString& sColor )
{
	const QColor& color = QColor(sColor);
	if (color.isValid()) {
		updateColorText(m_ui.MidiMeterColorLineEdit, color);
		m_paMidiMeterColors[m_ui.MidiMeterLevelComboBox->currentIndex()] = color;
		++m_iDirtyMeterColors;
		changed();
	}
}


// Audio meter color selection.
void qtractorOptionsForm::chooseAudioMeterColor (void)
{
	const QString& sTitle
		= tr("Audio Meter Color");

	QWidget *pParentWidget = nullptr;
	QColorDialog::ColorDialogOptions options;
	if (m_pOptions && m_pOptions->bDontUseNativeDialogs) {
		options |= QColorDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}

	const QColor& color = QColorDialog::getColor(
		QColor(m_ui.AudioMeterColorLineEdit->text()),
		pParentWidget, sTitle, options);

	if (color.isValid())
		m_ui.AudioMeterColorLineEdit->setText(color.name());
}


// Midi meter color selection.
void qtractorOptionsForm::chooseMidiMeterColor (void)
{
	const QString& sTitle
		= tr("MIDI Meter Color");

	QWidget *pParentWidget = nullptr;
	QColorDialog::ColorDialogOptions options;
	if (m_pOptions && m_pOptions->bDontUseNativeDialogs) {
		options |= QColorDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}

	const QColor& color = QColorDialog::getColor(
		QColor(m_ui.MidiMeterColorLineEdit->text()),
		pParentWidget, sTitle, options);

	if (color.isValid())
		m_ui.MidiMeterColorLineEdit->setText(color.name());
}


// Reset all meter colors from default.
void qtractorOptionsForm::resetMeterColors (void)
{
	// Reset colors.
	int iColor;
	for (iColor = 0; iColor < qtractorAudioMeter::ColorCount - 1; ++iColor)
		m_paAudioMeterColors[iColor] = qtractorAudioMeter::defaultColor(iColor);
	for (iColor = 0; iColor < qtractorMidiMeter::ColorCount - 1; ++iColor)
		m_paMidiMeterColors[iColor] = qtractorMidiMeter::defaultColor(iColor);

	// Update current display...
	changeAudioMeterLevel(m_ui.AudioMeterLevelComboBox->currentIndex());
	changeMidiMeterLevel(m_ui.MidiMeterLevelComboBox->currentIndex());

	++m_iDirtyMeterColors;
	changed();
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
	if (m_pOptions == nullptr)
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
	case qtractorPluginType::Vst2:
		paths = m_vst2Paths;
		break;
	case qtractorPluginType::Vst3:
		paths = m_vst3Paths;
		break;
	case qtractorPluginType::Clap:
		paths = m_clapPaths;
		break;
	case qtractorPluginType::Lv2:
		paths = m_lv2Paths;
		// Fall thru...
	default:
		break;
	}

	m_ui.PluginPathListWidget->clear();
	m_ui.PluginPathListWidget->addItems(paths);

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

	const QString& sTitle
		= tr("Plug-in Directory");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options = QFileDialog::ShowDirsOnly;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the directory...
	sPluginPath = QFileDialog::getExistingDirectory(pParentWidget,
		sTitle, m_ui.PluginPathComboBox->currentText(), options);
#else
	// Construct open-directory dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, m_ui.PluginPathComboBox->currentText());
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::DirectoryOnly);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
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
		++m_iDirtyLadspaPaths;
		break;
	case qtractorPluginType::Dssi:
		m_dssiPaths.append(sPluginPath);
		++m_iDirtyDssiPaths;
		break;
	case qtractorPluginType::Vst2:
		m_vst2Paths.append(sPluginPath);
		++m_iDirtyVst2Paths;
		break;
	case qtractorPluginType::Vst3:
		m_vst3Paths.append(sPluginPath);
		++m_iDirtyVst3Paths;
		break;
	case qtractorPluginType::Clap:
		m_clapPaths.append(sPluginPath);
		++m_iDirtyClapPaths;
		break;
	case qtractorPluginType::Lv2:
		m_lv2Paths.append(sPluginPath);
		++m_iDirtyLv2Paths;
		break;
	default:
		return;
	}

	m_ui.PluginPathListWidget->addItem(sPluginPath);
	m_ui.PluginPathListWidget->setCurrentRow(
		m_ui.PluginPathListWidget->count() - 1);

	const int i = m_ui.PluginPathComboBox->findText(sPluginPath);
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
	const int iPluginPath = m_ui.PluginPathListWidget->currentRow();

	m_ui.PluginPathRemoveToolButton->setEnabled(iPluginPath >= 0);
	m_ui.PluginPathUpToolButton->setEnabled(iPluginPath > 0);
	m_ui.PluginPathDownToolButton->setEnabled(iPluginPath >= 0
		&& iPluginPath < m_ui.PluginPathListWidget->count() - 1);
}


// Remove current plugin path.
void qtractorOptionsForm::removePluginPath (void)
{
	const int iPluginPath = m_ui.PluginPathListWidget->currentRow();
	if (iPluginPath < 0)
		return;

	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->currentText());
	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		m_ladspaPaths.removeAt(iPluginPath);
		++m_iDirtyLadspaPaths;
		break;
	case qtractorPluginType::Dssi:
		m_dssiPaths.removeAt(iPluginPath);
		++m_iDirtyDssiPaths;
		break;
	case qtractorPluginType::Vst2:
		m_vst2Paths.removeAt(iPluginPath);
		++m_iDirtyVst2Paths;
		break;
	case qtractorPluginType::Vst3:
		m_vst3Paths.removeAt(iPluginPath);
		++m_iDirtyVst3Paths;
		break;
	case qtractorPluginType::Clap:
		m_clapPaths.removeAt(iPluginPath);
		++m_iDirtyClapPaths;
		break;
	case qtractorPluginType::Lv2:
		m_lv2Paths.removeAt(iPluginPath);
		++m_iDirtyLv2Paths;
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
	const int iPluginPath = m_ui.PluginPathListWidget->currentRow();
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
		++m_iDirtyLadspaPaths;
		break;
	case qtractorPluginType::Dssi:
		sPluginPath = m_dssiPaths.takeAt(iPluginPath);
		m_dssiPaths.insert(iPluginPath - 1, sPluginPath);
		++m_iDirtyDssiPaths;
		break;
	case qtractorPluginType::Vst2:
		sPluginPath = m_vst2Paths.takeAt(iPluginPath);
		m_vst2Paths.insert(iPluginPath - 1, sPluginPath);
		++m_iDirtyVst2Paths;
		break;
	case qtractorPluginType::Vst3:
		sPluginPath = m_vst3Paths.takeAt(iPluginPath);
		m_vst3Paths.insert(iPluginPath - 1, sPluginPath);
		++m_iDirtyVst3Paths;
		break;
	case qtractorPluginType::Clap:
		sPluginPath = m_clapPaths.takeAt(iPluginPath);
		m_clapPaths.insert(iPluginPath - 1, sPluginPath);
		++m_iDirtyClapPaths;
		break;
	case qtractorPluginType::Lv2:
		sPluginPath = m_lv2Paths.takeAt(iPluginPath);
		m_lv2Paths.insert(iPluginPath - 1, sPluginPath);
		++m_iDirtyLv2Paths;
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
	const int iPluginPath = m_ui.PluginPathListWidget->currentRow();
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
		++m_iDirtyLadspaPaths;
		break;
	case qtractorPluginType::Dssi:
		sPluginPath = m_dssiPaths.takeAt(iPluginPath);
		m_dssiPaths.insert(iPluginPath + 1, sPluginPath);
		++m_iDirtyDssiPaths;
		break;
	case qtractorPluginType::Vst2:
		sPluginPath = m_vst2Paths.takeAt(iPluginPath);
		m_vst2Paths.insert(iPluginPath + 1, sPluginPath);
		++m_iDirtyVst2Paths;
		break;
	case qtractorPluginType::Vst3:
		sPluginPath = m_vst3Paths.takeAt(iPluginPath);
		m_vst3Paths.insert(iPluginPath + 1, sPluginPath);
		++m_iDirtyVst3Paths;
		break;
	case qtractorPluginType::Clap:
		sPluginPath = m_clapPaths.takeAt(iPluginPath);
		m_clapPaths.insert(iPluginPath + 1, sPluginPath);
		++m_iDirtyClapPaths;
		break;
	case qtractorPluginType::Lv2:
		sPluginPath = m_lv2Paths.takeAt(iPluginPath);
		m_lv2Paths.insert(iPluginPath + 1, sPluginPath);
		++m_iDirtyLv2Paths;
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

	const QString& sTitle
		= tr("LV2 Presets Directory");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options = QFileDialog::ShowDirsOnly;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1// QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the directory...
	sLv2PresetDir = QFileDialog::getExistingDirectory(pParentWidget,
		sTitle, sLv2PresetDir, options);
#else
	// Construct open-directory dialog...
	QFileDialog fileDialog(pParentWidget, sTitle, sLv2PresetDir);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::DirectoryOnly);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sLv2PresetDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sLv2PresetDir = fileDialog.selectedFiles().first();
#endif

	if (!sLv2PresetDir.isEmpty()) {
		m_ui.Lv2PresetDirComboBox->setEditText(sLv2PresetDir);
		m_ui.Lv2PresetDirComboBox->setFocus();
	}

	++m_iDirtyLv2Paths;
	changed();
}


// Change plugin path.
void qtractorOptionsForm::changePluginBlacklist ( const QString& /*sBlacklist*/ )
{
	selectPluginBlacklist();
	stabilizeForm();
}


// Browse for plugin blacklist path.
void qtractorOptionsForm::choosePluginBlacklist (void)
{
	QString sFilename;

	const QString& sTitle
		= tr("Plug-in Blacklist");

	QStringList exts;
	exts.append("so");
#ifdef CONFIG_CLAP
	exts.append("clap");
#endif

	QStringList filters;
	filters.append(tr("Plug-in files (*.%1)").arg(exts.join(" *.")));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the filename to open...
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, m_ui.PluginBlacklistComboBox->currentText(), sFilter, nullptr, options);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, m_ui.PluginBlacklistComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif

	if (!sFilename.isEmpty()) {
		m_ui.PluginBlacklistComboBox->setEditText(sFilename);
		m_ui.PluginBlacklistComboBox->setFocus();
	}

	selectPluginBlacklist();
	stabilizeForm();
}


// Add chosen plugin blacklist path.
void qtractorOptionsForm::addPluginBlacklist (void)
{
	const QString& sBlacklist
		= m_ui.PluginBlacklistComboBox->currentText();
	if (sBlacklist.isEmpty())
		return;

	m_ui.PluginBlacklistWidget->addItem(sBlacklist);
	m_ui.PluginBlacklistWidget->setCurrentRow(
		m_ui.PluginBlacklistWidget->count() - 1);

	const int i = m_ui.PluginBlacklistComboBox->findText(sBlacklist);
	if (i >= 0)
		m_ui.PluginBlacklistComboBox->removeItem(i);
	m_ui.PluginBlacklistComboBox->insertItem(0, sBlacklist);
	m_ui.PluginBlacklistComboBox->setEditText(QString());

	m_ui.PluginBlacklistWidget->setFocus();

	++m_iDirtyBlacklist;

	selectPluginBlacklist();
	changed();
}


// Select current plugin path.
void qtractorOptionsForm::selectPluginBlacklist (void)
{
	const int iBlacklist
		= m_ui.PluginBlacklistWidget->currentRow();

	m_ui.PluginBlacklistRemoveToolButton->setEnabled(iBlacklist >= 0);
	m_ui.PluginBlacklistClearToolButton->setEnabled(
		m_ui.PluginBlacklistWidget->count() > 0);
}


// Remove current plugin path.
void qtractorOptionsForm::removePluginBlacklist (void)
{
	const int iBlacklist
		= m_ui.PluginBlacklistWidget->currentRow();
	if (iBlacklist < 0)
		return;

	QListWidgetItem *pItem = m_ui.PluginBlacklistWidget->takeItem(iBlacklist);
	if (pItem)
		delete pItem;

	++m_iDirtyBlacklist;

	selectPluginBlacklist();
	changed();
}


// Clear blacklist paths.
void qtractorOptionsForm::clearPluginBlacklist (void)
{
	m_ui.PluginBlacklistWidget->clear();

	++m_iDirtyBlacklist;

	selectPluginBlacklist();
	changed();
}


// The messages font selection dialog.
void qtractorOptionsForm::chooseMessagesFont (void)
{
	const QString& sTitle
		= tr("Messages Font");

	QWidget *pParentWidget = nullptr;
	QFontDialog::FontDialogOptions options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFontDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}

	bool bOk = false;
	const QFont font = QFontDialog::getFont(&bOk,
		m_ui.MessagesFontTextLabel->font(), pParentWidget, sTitle, options);
	if (bOk) {
		m_ui.MessagesFontTextLabel->setFont(font);
		m_ui.MessagesFontTextLabel->setText(
			font.family() + ' ' + QString::number(font.pointSize()));
		changed();
	}
}


// Messages log path browse slot.
void qtractorOptionsForm::chooseMessagesLogPath (void)
{
	QString sFilename;

	const QString  sExt("log");
	const QString& sTitle
		= tr("Messages Log");

	QStringList filters;
	filters.append(tr("Log files (*.%1)").arg(sExt));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the filename to open...
	sFilename = QFileDialog::getSaveFileName(pParentWidget,
		sTitle, m_ui.MessagesLogPathComboBox->currentText(), sFilter, nullptr, options);
#else
	// Construct open-file dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, m_ui.MessagesLogPathComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif

	if (!sFilename.isEmpty() && sFilename.at(0) != '.') {
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
	const QString& sTitle
		= tr("Session Template");

	QStringList filters;
	filters.append(tr("Session template files (*.qtr *.qts *.%1)").arg(sExt));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the filename to open...
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, m_ui.SessionTemplatePathComboBox->currentText(), sFilter, nullptr, options);
#else
	// Construct open-files dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, m_ui.SessionTemplatePathComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
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
	const qtractorAudioFileFactory::FileFormat *pFormat
		= static_cast<const qtractorAudioFileFactory::FileFormat *> (
			m_ui.AudioCaptureTypeComboBox->currentHandle());

	const bool bSndFile
		= (pFormat && pFormat->type == qtractorAudioFileFactory::SndFile);
	m_ui.AudioCaptureFormatTextLabel->setEnabled(bSndFile);
	m_ui.AudioCaptureFormatComboBox->setEnabled(bSndFile);

	const bool bVorbisFile
		= (pFormat && pFormat->type == qtractorAudioFileFactory::VorbisFile);
	m_ui.AudioCaptureQualityTextLabel->setEnabled(bVorbisFile);
	m_ui.AudioCaptureQualitySpinBox->setEnabled(bVorbisFile);

	bool bValid = (m_iDirtyCount > 0);
	if (bValid) {
		const int iFormat = m_ui.AudioCaptureFormatComboBox->currentIndex();
		bValid  = qtractorAudioFileFactory::isValidFormat(pFormat, iFormat);
	}

	m_ui.AudioWsolaQuickSeekCheckBox->setEnabled(
		m_ui.AudioWsolaTimeStretchCheckBox->isChecked());

	m_ui.AudioPlayerAutoConnectCheckBox->setEnabled(
		m_ui.AudioPlayerBusCheckBox->isChecked());

	const bool bAudioMetronome = m_ui.AudioMetronomeCheckBox->isChecked();
	m_ui.AudioCountInModeLabel->setEnabled(bAudioMetronome);
	m_ui.AudioCountInModeComboBox->setEnabled(bAudioMetronome);
	m_ui.AudioCountInBeatsSpinBox->setEnabled(
		bAudioMetronome && m_ui.AudioCountInModeComboBox->currentIndex() > 0);
	m_ui.MetroBarFilenameTextLabel->setEnabled(bAudioMetronome);
	m_ui.MetroBarFilenameComboBox->setEnabled(bAudioMetronome);
	m_ui.MetroBarFilenameToolButton->setEnabled(bAudioMetronome);
	bool bAudioMetroBar = false;
	if (bAudioMetronome) {
		const QFileInfo fi(m_ui.MetroBarFilenameComboBox->currentText());
		bAudioMetroBar = fi.isFile() && fi.isReadable();
		bValid = bAudioMetroBar;
	}
	m_ui.MetroBarGainTextLabel->setEnabled(bAudioMetroBar);
	m_ui.MetroBarGainSpinBox->setEnabled(bAudioMetroBar);
	m_ui.MetroBeatFilenameTextLabel->setEnabled(bAudioMetronome);
	m_ui.MetroBeatFilenameComboBox->setEnabled(bAudioMetronome);
	m_ui.MetroBeatFilenameToolButton->setEnabled(bAudioMetronome);
	bool bAudioMetroBeat = false;
	if (bAudioMetronome) {
		const QFileInfo fi(m_ui.MetroBeatFilenameComboBox->currentText());
		bAudioMetroBeat = fi.isFile() && fi.isReadable();
		bValid = bAudioMetroBeat;
	}
	m_ui.MetroBeatGainTextLabel->setEnabled(bAudioMetroBeat);
	m_ui.MetroBeatGainSpinBox->setEnabled(bAudioMetroBeat);
	m_ui.AudioMetroBusCheckBox->setEnabled(bAudioMetronome);
	m_ui.AudioMetroOffsetTextLabel->setEnabled(bAudioMetronome);
	m_ui.AudioMetroOffsetSpinBox->setEnabled(bAudioMetronome);
	m_ui.AudioMetroAutoConnectCheckBox->setEnabled(
		bAudioMetronome && m_ui.AudioMetroBusCheckBox->isChecked());

	const bool bMmcMode =(m_ui.MidiMmcModeComboBox->currentIndex() > 0);
	m_ui.MidiMmcDeviceTextLabel->setEnabled(bMmcMode);
	m_ui.MidiMmcDeviceComboBox->setEnabled(bMmcMode);

	const bool bMidiMetronome = m_ui.MidiMetronomeCheckBox->isChecked();
	m_ui.MidiCountInModeLabel->setEnabled(bMidiMetronome);
	m_ui.MidiCountInModeComboBox->setEnabled(bMidiMetronome);
	m_ui.MidiCountInBeatsSpinBox->setEnabled(
		bMidiMetronome && m_ui.MidiCountInModeComboBox->currentIndex() > 0);
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
	m_ui.MidiMetroOffsetTextLabel->setEnabled(bMidiMetronome);
	m_ui.MidiMetroOffsetSpinBox->setEnabled(bMidiMetronome);

	const bool bMessagesLog = m_ui.MessagesLogCheckBox->isChecked();
	m_ui.MessagesLogPathComboBox->setEnabled(bMessagesLog);
	m_ui.MessagesLogPathToolButton->setEnabled(bMessagesLog);
	if (bMessagesLog && bValid) {
		const QString& sPath = m_ui.MessagesLogPathComboBox->currentText();
		bValid = !sPath.isEmpty();
	}

	const bool bSessionTemplate = m_ui.SessionTemplateCheckBox->isChecked();
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

	const QString& sBlacklist = m_ui.PluginBlacklistComboBox->currentText();
	m_ui.PluginBlacklistAddToolButton->setEnabled(
		!sBlacklist.isEmpty()
		&& m_ui.PluginBlacklistWidget->findItems(
			sBlacklist, Qt::MatchExactly).isEmpty());

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
	const QString& sFilter
		= qtractorAudioFileFactory::filters().join(";;");

	QString sAudioFile;

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (m_pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the filename to open...
	sAudioFile = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, sFilename, sFilter, nullptr, options);
#else
	// Construct open-file dialog...
	QFileDialog fileDialog(pParentWidget, sTitle, sFilename, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(qtractorAudioFileFactory::defaultExt());
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(m_pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(m_pOptions->sAudioDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sAudioFile = fileDialog.selectedFiles().first();
#endif

	return sAudioFile;
}


// end of qtractorOptionsForm.cpp

