// qtractorClipForm.cpp
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

#include "qtractorClipForm.h"

#include "qtractorAbout.h"
#include "qtractorClip.h"
#include "qtractorClipCommand.h"

#include "qtractorAudioClip.h"
#include "qtractorMidiClip.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include "qtractorMainForm.h"

#include <QMessageBox>
#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QUrl>

// Needed for logf() and powf()
#include <cmath>

static inline float log10f2 ( float x )
	{ return (x > 0.0f ? 20.0f * ::log10f(x) : -60.0f); }

static inline float pow10f2 ( float x )
	{ return ::powf(10.0f, 0.05f * x); }


//----------------------------------------------------------------------------
// Fade types curves.

struct FadeTypeInfo
{
	QString name;
	QIcon iconFadeIn;
	QIcon iconFadeOut;
};

static QHash<int, FadeTypeInfo> g_fadeTypes;


void qtractorClipForm::initFadeTypes (void)
{
	const char *s_aFadeTypeNames[] = {

		QT_TR_NOOP("Linear"),		// Linear (obvious:)
		QT_TR_NOOP("Quadratic 1"),	// InQuad
		QT_TR_NOOP("Quadratic 2"),	// OutQuad
		QT_TR_NOOP("Quadratic 3"),	// InOutQuad
		QT_TR_NOOP("Cubic 1"),		// InCubic
		QT_TR_NOOP("Cubic 2"),		// OutCubic
		QT_TR_NOOP("Cubic 3"),		// InOutCubic

		nullptr
	};

	if (g_fadeTypes.isEmpty()) {
		const QPixmap& pmFadeIn
			= QIcon::fromTheme("fadeIn").pixmap(7 * 16, 16);
		const QPixmap& pmFadeOut
			= QIcon::fromTheme("fadeOut").pixmap(7 * 16, 16);
		for (int i = 0; s_aFadeTypeNames[i]; ++i) {
			FadeTypeInfo& info = g_fadeTypes[i];
			info.name = tr(s_aFadeTypeNames[i]);
			info.iconFadeIn  = pmFadeIn.copy(i << 4, 0, 16, 16);
			info.iconFadeOut = pmFadeOut.copy(i << 4, 0, 16, 16);
		}
	}
}


//----------------------------------------------------------------------------
// qtractorClipForm -- UI wrapper form.

// Constructor.
qtractorClipForm::qtractorClipForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize dirty control state.
	m_pClip       = nullptr;
	m_bClipNew    = false;
	m_pTimeScale  = nullptr;
	m_iDirtyCount = 0;
	m_iDirtySetup = 0;

	initFadeTypes();
	m_ui.FadeInTypeComboBox->clear();
	m_ui.FadeOutTypeComboBox->clear();
	for (int i = 0; i < g_fadeTypes.count(); ++i) {
		const FadeTypeInfo& info = g_fadeTypes.value(i);
		m_ui.FadeInTypeComboBox->addItem(info.iconFadeIn, info.name);
		m_ui.FadeOutTypeComboBox->addItem(info.iconFadeOut, info.name);
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	// Some conveniency cleaner helpers...
	m_ui.ClipNameLineEdit->setClearButtonEnabled(true);
	m_ui.FilenameComboBox->lineEdit()->setClearButtonEnabled(true);
#endif

	// Try to set minimal window positioning.
	m_ui.TrackChannelTextLabel->hide();
	m_ui.TrackChannelSpinBox->hide();
	m_ui.AudioClipGroupBox->hide();
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.ClipNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(clipNameChanged(const QString&)));
	QObject::connect(m_ui.FilenameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(filenameChanged(const QString&)));
	QObject::connect(m_ui.FilenameToolButton,
		SIGNAL(clicked()),
		SLOT(browseFilename()));
	QObject::connect(m_ui.TrackChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(trackChannelChanged(int)));
	QObject::connect(m_ui.ClipGainSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipPanningSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.FormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ClipStartSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(clipStartChanged(unsigned long)));
	QObject::connect(m_ui.ClipStartSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ClipOffsetSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipOffsetSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ClipLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipOffsetSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.FadeInLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeInLengthSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.FadeInTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeOutLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeOutLengthSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.FadeOutTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TimeStretchSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.PitchShiftSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.WsolaTimeStretchCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.WsolaQuickSeekCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipMuteCheckBox,
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
qtractorClipForm::~qtractorClipForm (void)
{
	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorClipForm::setClip ( qtractorClip *pClip )
{
	// Initialize conveniency options...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Mark that we're changing thing's here...
	++m_iDirtySetup;

	QString sFilename;
	if (pClip)
		sFilename = pClip->filename();

	// Clip properties cloning...
	m_pClip = pClip;
	m_bClipNew = sFilename.isEmpty();

	// Why not change the dialog icon accordingly?
	if (m_bClipNew)
		QDialog::setWindowIcon(QIcon::fromTheme("clipNew"));

	// Copy from global time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
	m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());

	m_ui.ClipStartSpinBox->setTimeScale(m_pTimeScale);
	m_ui.ClipOffsetSpinBox->setTimeScale(m_pTimeScale);
	m_ui.ClipLengthSpinBox->setTimeScale(m_pTimeScale);
	m_ui.FadeInLengthSpinBox->setTimeScale(m_pTimeScale);
	m_ui.FadeOutLengthSpinBox->setTimeScale(m_pTimeScale);
	// These have special delta formats...
	clipStartChanged(m_pClip->clipStart());
	// Initialize dialog widgets...
	m_ui.ClipNameLineEdit->setText(m_pClip->clipName());
	// Parameters...
	m_ui.ClipStartSpinBox->setValue(m_pClip->clipStart());
	m_ui.ClipOffsetSpinBox->setValue(m_pClip->clipOffset());
	m_ui.ClipLengthSpinBox->setValue(m_pClip->clipLength());
	// Fade In/Out...
	m_ui.FadeInLengthSpinBox->setValue(m_pClip->fadeInLength());
	m_ui.FadeInTypeComboBox->setCurrentIndex(
		indexFromFadeType(m_pClip->fadeInType()));
	m_ui.FadeOutLengthSpinBox->setValue(m_pClip->fadeOutLength());
	m_ui.FadeOutTypeComboBox->setCurrentIndex(
		indexFromFadeType(m_pClip->fadeOutType()));

	// Set proper time scales display format...
	m_ui.FormatComboBox->setCurrentIndex(
		int(m_pTimeScale->displayFormat()));

	// Now those things specific on track type...
	const QString sSuffix = m_ui.FilenameComboBox->objectName();
	switch (trackType()) {
	case qtractorTrack::Audio: {
		m_ui.FilenameComboBox->setObjectName("Audio" + sSuffix);
		m_ui.ClipGainTextLabel->setText(tr("&Gain:"));
		m_ui.ClipGainSpinBox->setSuffix(tr(" dB"));
		m_ui.ClipGainSpinBox->setRange(-60.0f, +24.0f);
		m_ui.ClipGainSpinBox->setValue(log10f2(m_pClip->clipGain()));
		m_ui.ClipPanningSpinBox->setRange(-1.0f, +1.0f);
		m_ui.ClipPanningSpinBox->setValue(pClip->clipPanning());
		qtractorAudioClip *pAudioClip
			= static_cast<qtractorAudioClip *> (m_pClip);
		if (pAudioClip) {
			m_ui.TimeStretchSpinBox->setValue(
				100.0f * pAudioClip->timeStretch());
			m_ui.PitchShiftSpinBox->setValue(
				12.0f * ::logf(pAudioClip->pitchShift()) / M_LN2);
		}
		m_ui.TrackChannelTextLabel->setVisible(false);
		m_ui.TrackChannelSpinBox->setVisible(false);
		m_ui.ClipPanningTextLabel->setVisible(true);
		m_ui.ClipPanningSpinBox->setVisible(true);
		m_ui.AudioClipGroupBox->setVisible(true);
	#ifdef CONFIG_LIBRUBBERBAND
		m_ui.WsolaTimeStretchCheckBox->setChecked(pAudioClip->isWsolaTimeStretch());
	#else
		m_ui.PitchShiftTextLabel->setEnabled(false);
		m_ui.PitchShiftSpinBox->setEnabled(false);	
		m_ui.WsolaTimeStretchCheckBox->setEnabled(false);
		m_ui.WsolaTimeStretchCheckBox->setChecked(true);
	#endif
		m_ui.WsolaQuickSeekCheckBox->setChecked(pAudioClip->isWsolaQuickSeek());
		break;
	}
	case qtractorTrack::Midi: {
		m_ui.FilenameComboBox->setObjectName("Midi" + sSuffix);
		m_ui.ClipGainTextLabel->setText(tr("&Volume:"));
		m_ui.ClipGainSpinBox->setSuffix(tr(" %"));
		m_ui.ClipGainSpinBox->setRange(0.0f, 1200.0f);
		m_ui.ClipGainSpinBox->setValue(100.0f * m_pClip->clipGain());
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (m_pClip);
		if (pMidiClip)
			m_ui.TrackChannelSpinBox->setValue(pMidiClip->trackChannel());
		m_ui.TrackChannelTextLabel->setVisible(true);
		m_ui.TrackChannelSpinBox->setVisible(true);
		m_ui.ClipPanningTextLabel->setVisible(false);
		m_ui.ClipPanningSpinBox->setVisible(false);
		m_ui.AudioClipGroupBox->setVisible(false);
		break;
	}
	case qtractorTrack::None:
	default:
		m_ui.TrackChannelTextLabel->setVisible(false);
		m_ui.TrackChannelSpinBox->setVisible(false);
		m_ui.AudioClipGroupBox->setVisible(false);
		break;
	}

	m_ui.ClipMuteCheckBox->setChecked(m_pClip->isClipMute());

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->loadComboBoxHistory(m_ui.FilenameComboBox);

	// Finally set clip filename...
	m_ui.FilenameComboBox->setEditText(sFilename);

	// Shake it a little bit first, but
	// make it as tight as possible...
	// resize(width() - 1, height() - 1);
	adjustSize();

	// Backup clean.
	--m_iDirtySetup;
	m_iDirtyCount = 0;

	// Done.
	stabilizeForm();
}


// Retrieve the accepted clip, if the case arises.
qtractorClip *qtractorClipForm::clip (void) const
{
	return m_pClip;
}


// Accept settings (OK button slot).
void qtractorClipForm::accept (void)
{
	// Sanity check...
	if (m_pClip == nullptr)
		return;
	if (!m_pClip->queryEditor())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Save settings...
	if (m_iDirtyCount > 0) {
		// Cache the changed settings (if any)...
		qtractorClipCommand *pClipCommand = nullptr;
		qtractorTrack::TrackType clipType = trackType();
		const QString& sFilename = m_ui.FilenameComboBox->currentText();
		const unsigned short iTrackChannel = m_ui.TrackChannelSpinBox->value();
		const QString& sClipName = m_ui.ClipNameLineEdit->text().trimmed();
		float fClipGain = 1.0f;
		float fClipPanning = 0.0f;
		float fTimeStretch = 0.0f;
		float fPitchShift = 0.0f;
		bool bWsolaTimeStretch = qtractorAudioBuffer::isDefaultWsolaTimeStretch();
		bool bWsolaQuickSeek = qtractorAudioBuffer::isDefaultWsolaQuickSeek();
		switch (clipType) {
		case qtractorTrack::Audio:
			fClipGain = pow10f2(m_ui.ClipGainSpinBox->value());
			fClipPanning = m_ui.ClipPanningSpinBox->value();
			fTimeStretch = 0.01f * m_ui.TimeStretchSpinBox->value();
			fPitchShift = ::powf(2.0f, m_ui.PitchShiftSpinBox->value() / 12.0f);
			bWsolaTimeStretch = m_ui.WsolaTimeStretchCheckBox->isChecked();
			bWsolaQuickSeek = m_ui.WsolaQuickSeekCheckBox->isChecked();
			break;
		case qtractorTrack::Midi:
			fClipGain = 0.01f * m_ui.ClipGainSpinBox->value();
			break;
		default:
			break;
		}
		const unsigned long iClipStart = m_ui.ClipStartSpinBox->value();
		const unsigned long iClipOffset = m_ui.ClipOffsetSpinBox->value();
		const unsigned long iClipLength = m_ui.ClipLengthSpinBox->value();
		const unsigned long iFadeInLength = m_ui.FadeInLengthSpinBox->value();
		qtractorClip::FadeType fadeInType
			= fadeTypeFromIndex(m_ui.FadeInTypeComboBox->currentIndex());
		const unsigned long iFadeOutLength = m_ui.FadeOutLengthSpinBox->value();
		qtractorClip::FadeType fadeOutType
			= fadeTypeFromIndex(m_ui.FadeOutTypeComboBox->currentIndex());
		const bool bClipMute = m_ui.ClipMuteCheckBox->isChecked();
		int iFileChange = 0;
		int iWsolaChange = 0;
		// It depends whether we're adding a new clip or not...
		if (m_bClipNew) {
			// Just set new clip properties...
			pClipCommand = new qtractorClipCommand(tr("new clip"));
			m_pClip->setClipName(sClipName);
			// Filename...
			m_pClip->setFilename(sFilename);
			// Track-channel and time-stretching...
			switch (clipType) {
			case qtractorTrack::Audio: {
				qtractorAudioClip *pAudioClip
					= static_cast<qtractorAudioClip *> (m_pClip);
				if (pAudioClip) {
					pAudioClip->setTimeStretch(fTimeStretch);
					pAudioClip->setPitchShift(fPitchShift);
					pAudioClip->setWsolaTimeStretch(bWsolaTimeStretch);
					pAudioClip->setWsolaQuickSeek(bWsolaQuickSeek);
					++iFileChange;
				}
				break;
			}
			case qtractorTrack::Midi: {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (m_pClip);
				if (pMidiClip) {
					pMidiClip->setTrackChannel(iTrackChannel);
					++iFileChange;
				}
				break;
			}
			case qtractorTrack::None:
			default:
				break;
			}
			// Gain/panning...
			m_pClip->setClipGain(fClipGain);
			m_pClip->setClipPanning(fClipPanning);
			// Parameters...
			m_pClip->setClipStart(iClipStart);
			m_pClip->setClipOffset(iClipOffset);
			m_pClip->setClipLength(iClipLength);
			// Fade in...
			m_pClip->setFadeInLength(iFadeInLength);
			m_pClip->setFadeInType(fadeInType);
			// Fade out...
			m_pClip->setFadeOutLength(iFadeOutLength);
			m_pClip->setFadeOutType(fadeOutType);
			// Mute...
			m_pClip->setClipMute(bClipMute);
			// Ready it...
			pClipCommand->addClip(m_pClip, m_pClip->track());
			// Ready new.
		} else {
			// Make changes (incrementally) undoable...
			pClipCommand = new qtractorClipCommand(tr("edit clip"));
			pClipCommand->renameClip(m_pClip, sClipName);
			// Filename changes...
			if (sFilename != m_pClip->filename())
				++iFileChange;
			// Track-channel and time-stretch issues...
			switch (clipType) {
			case qtractorTrack::Audio: {
				qtractorAudioClip *pAudioClip
					= static_cast<qtractorAudioClip *> (m_pClip);
				if (pAudioClip) {
					if (qAbs(fTimeStretch - pAudioClip->timeStretch()) < 0.001f)
						fTimeStretch = 0.0f;
					if (qAbs(fPitchShift - pAudioClip->pitchShift()) < 0.001f)
						fPitchShift = 0.0f;
					const bool bOldWsolaTimeStretch = pAudioClip->isWsolaTimeStretch();
					if (( bOldWsolaTimeStretch && !bWsolaTimeStretch) ||
						(!bOldWsolaTimeStretch &&  bWsolaTimeStretch))
						++iWsolaChange;
					const bool bOldWsolaQuickSeek = pAudioClip->isWsolaQuickSeek();
					if (( bOldWsolaQuickSeek   && !bWsolaQuickSeek  ) ||
						(!bOldWsolaQuickSeek   &&  bWsolaQuickSeek  ))
						++iWsolaChange;
				}
				break;
			}
			case qtractorTrack::Midi: {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (m_pClip);
				if (pMidiClip) {
					if (iTrackChannel != pMidiClip->trackChannel())
						++iFileChange;
				}
				break;
			}
			case qtractorTrack::None:
			default:
				break;
			}
			// Filename and/or track-channel changes...
			if (iFileChange > 0)
				pClipCommand->fileClip(m_pClip, sFilename, iTrackChannel);
			// Gain/nning...
			if (qAbs(fClipGain - m_pClip->clipGain()) > 0.001f)
				pClipCommand->gainClip(m_pClip, fClipGain);
			if (qAbs(fClipPanning - m_pClip->clipPanning()) > 0.001f)
				pClipCommand->panningClip(m_pClip, fClipPanning);
			// Parameters and/or time-stretching changes...
			if (iClipStart  != m_pClip->clipStart()  ||
				iClipOffset != m_pClip->clipOffset() ||
				iClipLength != m_pClip->clipLength() ||
				fTimeStretch > 0.0f || fPitchShift > 0.0f) {
				pClipCommand->resizeClip(m_pClip,
					iClipStart, iClipOffset, iClipLength,
					fTimeStretch, fPitchShift);
			}
			// Fade in changes...
			if (iFadeInLength != m_pClip->fadeInLength()
				|| fadeInType != m_pClip->fadeInType())
				pClipCommand->fadeInClip(m_pClip, iFadeInLength, fadeInType);
			// Fade out changes...
			if (iFadeOutLength != m_pClip->fadeOutLength()
				|| fadeOutType != m_pClip->fadeOutType())
				pClipCommand->fadeOutClip(m_pClip, iFadeOutLength, fadeOutType);
			// WSOLA audio clip options...
			if (iWsolaChange > 0)
				pClipCommand->wsolaClip(m_pClip, bWsolaTimeStretch, bWsolaQuickSeek);
			// Mute...
			if (( bClipMute && !m_pClip->isClipMute()) ||
				(!bClipMute &&  m_pClip->isClipMute()))
				pClipCommand->muteClip(m_pClip, bClipMute);
			// Ready edit.
		}
		// Do it (by making it undoable)...
		if (pClipCommand)
			pSession->execute(pClipCommand);
		// Account for a new file in game...
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (iFileChange > 0 && pMainForm) {
			switch (clipType) {
			case qtractorTrack::Audio:
				pMainForm->addAudioFile(sFilename);
				break;
			case qtractorTrack::Midi:
				pMainForm->addMidiFile(sFilename);
				break;
			case qtractorTrack::None:
			default:
				break;
			}
			// Save history conveniency options...
			qtractorOptions *pOptions = qtractorOptions::getInstance();
			if (pOptions)
				pOptions->saveComboBoxHistory(m_ui.FilenameComboBox);
		}
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorClipForm::reject (void)
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
void qtractorClipForm::changed (void)
{
	++m_iDirtyCount;
	stabilizeForm();
}


// Display format has changed.
void qtractorClipForm::formatChanged ( int iDisplayFormat )
{
	const bool bBlockSignals = m_ui.FormatComboBox->blockSignals(true);
	m_ui.FormatComboBox->setCurrentIndex(iDisplayFormat);

	qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.ClipStartSpinBox->setDisplayFormat(displayFormat);
	m_ui.ClipOffsetSpinBox->setDisplayFormat(displayFormat);
	m_ui.ClipLengthSpinBox->setDisplayFormat(displayFormat);
	m_ui.FadeInLengthSpinBox->setDisplayFormat(displayFormat);
	m_ui.FadeOutLengthSpinBox->setDisplayFormat(displayFormat);

	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	m_ui.FormatComboBox->blockSignals(bBlockSignals);

	stabilizeForm();
}


// Stabilize current form state.
void qtractorClipForm::stabilizeForm (void)
{
	const QString& sFilename = m_ui.FilenameComboBox->currentText();
	const unsigned long iClipLength = m_ui.ClipLengthSpinBox->value();
	m_ui.FadeInTypeComboBox->setEnabled(
		m_ui.FadeInLengthSpinBox->value() > 0);
	m_ui.FadeInLengthSpinBox->setMaximum(iClipLength);
	m_ui.FadeOutTypeComboBox->setEnabled(
		m_ui.FadeOutLengthSpinBox->value() > 0);
	m_ui.FadeOutLengthSpinBox->setMaximum(iClipLength);
	m_ui.WsolaQuickSeekCheckBox->setEnabled(
		m_ui.WsolaTimeStretchCheckBox->isChecked());

	bool bValid = (m_iDirtyCount > 0);
	bValid = bValid && !m_ui.ClipNameLineEdit->text().isEmpty();
	bValid = bValid && !sFilename.isEmpty() && QFileInfo(sFilename).exists();
	bValid = bValid && (iClipLength > 0);
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);
}


// Fade type index converters.
qtractorClip::FadeType qtractorClipForm::fadeTypeFromIndex ( int iIndex ) const
{
	return qtractorClip::FadeType(iIndex);
}

int qtractorClipForm::indexFromFadeType ( qtractorClip::FadeType fadeType ) const
{
	return int(fadeType);
}


// Retrieve current clip/track type.
qtractorTrack::TrackType qtractorClipForm::trackType (void) const
{
	qtractorTrack *pTrack = nullptr;
	if (m_pClip)
		pTrack = m_pClip->track();

	return (pTrack ? pTrack->trackType() : qtractorTrack::None);
}


// Browse for audio clip filename.
void qtractorClipForm::browseFilename (void)
{
	QString sType;
	QString sExt;
	QStringList filters;

	qtractorTrack::TrackType clipType = trackType();
	switch (clipType) {
	case qtractorTrack::Audio:
		sType = tr("Audio");
		sExt = qtractorAudioFileFactory::defaultExt();
		filters = qtractorAudioFileFactory::filters();
		break;
	case qtractorTrack::Midi:
		sType = tr("MIDI");
		sExt = "mid";
		filters.append(tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt));
		filters.append(tr("All files (*.*)"));
		break;
	case qtractorTrack::None:
	default:
		return;
	}

	const QString& sFilter = filters.join(";;");

	// Browse for file...
	QString sFilename;

	const QString& sTitle
		= tr("%1 Clip File").arg(sType);

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, m_ui.FilenameComboBox->currentText(), sFilter, nullptr, options);
#else
	QFileDialog fileDialog(pParentWidget,
		sTitle, m_ui.FilenameComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	if (pOptions) {
		QList<QUrl> urls(fileDialog.sidebarUrls());
		urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
		switch (clipType) {
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
		sFilename = fileDialog.selectedFiles().first();
#endif

	if (!sFilename.isEmpty()) {
		m_ui.FilenameComboBox->setEditText(sFilename);
		m_ui.FilenameComboBox->setFocus();
	//	changed();
	}
}


// Adjust clip length to (new) selected file.
void qtractorClipForm::fileChanged (
	const QString& sFilename, unsigned short iTrackChannel )
{
	if (m_iDirtySetup > 0)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Do nothing else if file is invalid...
	QFileInfo fi(sFilename);

	if (!fi.exists()) {
		changed();
		return;
	}

	// Give as clip name hint if blank or new...
	if (m_ui.ClipNameLineEdit->text().isEmpty())
		m_ui.ClipNameLineEdit->setText(fi.baseName());

	// Depending on the clip/track type,
	// set clip length to the file length (in frames)...
	unsigned long iClipLength = 0;

	switch (trackType()) {
	case qtractorTrack::Midi: {
		qtractorMidiFile file;
		if (file.open(sFilename)) {
			const unsigned short p = pSession->ticksPerBeat();
			const unsigned short q = file.ticksPerBeat();
			const unsigned long iTrackDuration
				= file.readTrackDuration(iTrackChannel);
			if (iTrackDuration > 0) {
				const unsigned long duration
					= uint64_t(iTrackDuration * p) / q;
				iClipLength = pSession->frameFromTick(duration);
			}
			file.close();
		}
		break;
	}
	case qtractorTrack::Audio: {
		qtractorAudioFile *pFile
			= qtractorAudioFileFactory::createAudioFile(sFilename);
		if (pFile) {
			if (pFile->open(sFilename)) {
				iClipLength = pFile->frames();
				if (pFile->sampleRate() > 0
					&& pFile->sampleRate() != pSession->sampleRate()) {
					iClipLength = (unsigned long) (iClipLength
						* float(pSession->sampleRate())
						/ float(pFile->sampleRate()));
				}
				pFile->close();
			}
			delete pFile;
		}
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}

	// Done.
	m_ui.ClipOffsetSpinBox->setValue(0);
	m_ui.ClipLengthSpinBox->setValue(iClipLength);

	changed();
}


void qtractorClipForm::clipNameChanged ( const QString& sClipName )
{
	// Give as clip name hint if blank or new...
	if (sClipName.isEmpty()) {
		const QString& sFilename
			= m_ui.FilenameComboBox->currentText();
		if (!sFilename.isEmpty())
			m_ui.ClipNameLineEdit->setText(QFileInfo(sFilename).baseName());
	}

	changed();
}


void qtractorClipForm::filenameChanged ( const QString& sFilename )
{
	fileChanged(sFilename, m_ui.TrackChannelSpinBox->value());
}


void qtractorClipForm::trackChannelChanged ( int iTrackChannel )
{
	fileChanged(m_ui.FilenameComboBox->currentText(), iTrackChannel);
}


// Adjust delta-value spin-boxes to new anchor frame.
void qtractorClipForm::clipStartChanged ( unsigned long iClipStart )
{
	m_ui.ClipOffsetSpinBox->setDeltaValue(true, iClipStart);
	m_ui.ClipLengthSpinBox->setDeltaValue(true, iClipStart);
	m_ui.FadeInLengthSpinBox->setDeltaValue(true, iClipStart);
	m_ui.FadeOutLengthSpinBox->setDeltaValue(true, iClipStart);

	if (m_iDirtySetup == 0)
		changed();
}


// end of qtractorClipForm.cpp
