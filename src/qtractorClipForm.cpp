// qtractorClipForm.cpp
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

// Needed for fabs(), logf() and powf()
#include <math.h>

static inline float log10f2 ( float x )
	{ return (x > 0.0f ? 20.0f * ::log10f(x) : -60.0f); }

static inline float pow10f2 ( float x )
	{ return ::powf(10.0f, 0.05f * x); }

// Translatable macro contextualizer.
#undef  _TR
#define _TR(x) QT_TR_NOOP(x)


//----------------------------------------------------------------------------
// Fade types curves.

const char *g_aFadeTypeNames[] = {

	_TR("Linear"),		// Linear (obvious:)
	_TR("Quadratic 1"),	// InQuad
	_TR("Quadratic 2"),	// OutQuad
	_TR("Quadratic 3"),	// InOutQuad
	_TR("Cubic 1"),		// InCubic
	_TR("Cubic 2"),		// OutCubic
	_TR("Cubic 3"),		// InOutCubic

	NULL
};

struct FadeTypeInfo
{
	QString name;
	QIcon iconFadeIn;
	QIcon iconFadeOut;
};

static QHash<int, FadeTypeInfo> g_fadeTypes;


static void initFadeTypes (void)
{
	if (g_fadeTypes.isEmpty()) {
		const QPixmap pmFadeIn(":/images/fadeIn.png");
		const QPixmap pmFadeOut(":/images/fadeOut.png");
		for (int i = 0; g_aFadeTypeNames[i]; ++i) {
			FadeTypeInfo& info = g_fadeTypes[i];
			info.name = QObject::tr(g_aFadeTypeNames[i], "fadeType");
			info.iconFadeIn  = pmFadeIn.copy(i << 4, 0, 16, 16);
			info.iconFadeOut = pmFadeOut.copy(i << 4, 0, 16, 16);
		}
	}
}


//----------------------------------------------------------------------------
// qtractorClipForm -- UI wrapper form.

// Constructor.
qtractorClipForm::qtractorClipForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize dirty control state.
	m_pClip       = NULL;
	m_bClipNew    = false;
	m_pTimeScale  = NULL;
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

	// Try to set minimal window positioning.
	m_ui.TrackChannelTextLabel->hide();
	m_ui.TrackChannelSpinBox->hide();
	m_ui.AudioClipGroupBox->hide();
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.ClipNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.FilenameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(filenameChanged(const QString&)));
	QObject::connect(m_ui.FilenameToolButton,
		SIGNAL(clicked()),
		SLOT(browseFilename()));
	QObject::connect(m_ui.ClipGainSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.TrackChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(trackChannelChanged(int)));
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
void qtractorClipForm::setClip ( qtractorClip *pClip, bool bClipNew )
{
	// Initialize conveniency options...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Mark that we're changing thing's here...
	++m_iDirtySetup;

	// Clip properties cloning...
	m_pClip = pClip;
	m_bClipNew = bClipNew;

	// Why not change the dialog icon accordingly?
	if (m_bClipNew)
		QDialog::setWindowIcon(QIcon(":/images/editClipNew.png"));

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
		m_ui.GainVolumeGroupBox->setTitle(tr("&Gain:"));
		m_ui.ClipGainSpinBox->setSuffix(tr(" dB"));
		m_ui.ClipGainSpinBox->setRange(-60.0f, +24.0f);
		m_ui.ClipGainSpinBox->setValue(log10f2(m_pClip->clipGain()));
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
		m_ui.AudioClipGroupBox->setVisible(true);
	#ifndef CONFIG_LIBRUBBERBAND
		m_ui.PitchShiftTextLabel->setEnabled(false);
		m_ui.PitchShiftSpinBox->setEnabled(false);	
	#endif
		break;
	}
	case qtractorTrack::Midi: {
		m_ui.FilenameComboBox->setObjectName("Midi" + sSuffix);
		m_ui.GainVolumeGroupBox->setTitle(tr("&Volume:"));
		m_ui.ClipGainSpinBox->setSuffix(tr(" %"));
		m_ui.ClipGainSpinBox->setRange(0.0f, 1200.0f);
		m_ui.ClipGainSpinBox->setValue(100.0f * m_pClip->clipGain());
		qtractorMidiClip *pMidiClip
			= static_cast<qtractorMidiClip *> (m_pClip);
		if (pMidiClip)
			m_ui.TrackChannelSpinBox->setValue(pMidiClip->trackChannel());
		m_ui.TrackChannelTextLabel->setVisible(true);
		m_ui.TrackChannelSpinBox->setVisible(true);
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

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->loadComboBoxHistory(m_ui.FilenameComboBox);

	// Finally set clip filename...
	m_ui.FilenameComboBox->setEditText(m_pClip->filename());

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
	if (m_pClip == NULL)
		return;
	if (!m_pClip->queryEditor())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Save settings...
	if (m_iDirtyCount > 0) {
		// Cache the changed settings (if any)...
		qtractorClipCommand *pClipCommand = NULL;
		qtractorTrack::TrackType clipType = trackType();
		const QString& sFilename  = m_ui.FilenameComboBox->currentText();
		unsigned short iTrackChannel = m_ui.TrackChannelSpinBox->value();
		const QString& sClipName  = m_ui.ClipNameLineEdit->text();
		float fClipGain = 1.0f;
		float fTimeStretch = 0.0f;
		float fPitchShift = 0.0f;
		switch (clipType) {
		case qtractorTrack::Audio:
			fClipGain = pow10f2(m_ui.ClipGainSpinBox->value());
			fTimeStretch = 0.01f * m_ui.TimeStretchSpinBox->value();
			fPitchShift = ::powf(2.0f, m_ui.PitchShiftSpinBox->value() / 12.0f);
			break;
		case qtractorTrack::Midi:
			fClipGain = 0.01f * m_ui.ClipGainSpinBox->value();
			break;
		default:
			break;
		}
		unsigned long iClipStart  = m_ui.ClipStartSpinBox->value();
		unsigned long iClipOffset = m_ui.ClipOffsetSpinBox->value();
		unsigned long iClipLength = m_ui.ClipLengthSpinBox->value();
		unsigned long iFadeInLength = m_ui.FadeInLengthSpinBox->value();
		qtractorClip::FadeType fadeInType
			= fadeTypeFromIndex(m_ui.FadeInTypeComboBox->currentIndex());
		unsigned long iFadeOutLength = m_ui.FadeOutLengthSpinBox->value();
		qtractorClip::FadeType fadeOutType
			= fadeTypeFromIndex(m_ui.FadeOutTypeComboBox->currentIndex());
		int iFileChange = 0;
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
			// Gain/volume...
			m_pClip->setClipGain(fClipGain);
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
					if (::fabs(fTimeStretch - pAudioClip->timeStretch()) < 0.001f)
						fTimeStretch = 0.0f;
					if (::fabs(fPitchShift - pAudioClip->pitchShift()) < 0.001f)
						fPitchShift = 0.0f;
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
			// Gain/volume...
			if (::fabs(fClipGain - m_pClip->clipGain()) > 0.001f)
				pClipCommand->gainClip(m_pClip, fClipGain);
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
void qtractorClipForm::changed (void)
{
	++m_iDirtyCount;
	stabilizeForm();
}


// Display format has changed.
void qtractorClipForm::formatChanged ( int iDisplayFormat )
{
	bool bBlockSignals = m_ui.FormatComboBox->blockSignals(true);
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
	unsigned long iClipLength = m_ui.ClipLengthSpinBox->value();
	m_ui.FadeInTypeComboBox->setEnabled(
		m_ui.FadeInLengthSpinBox->value() > 0);
	m_ui.FadeInLengthSpinBox->setMaximum(iClipLength);
	m_ui.FadeOutTypeComboBox->setEnabled(
		m_ui.FadeOutLengthSpinBox->value() > 0);
	m_ui.FadeOutLengthSpinBox->setMaximum(iClipLength);

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
	qtractorTrack *pTrack = NULL;
	if (m_pClip)
		pTrack = m_pClip->track();

	return (pTrack ? pTrack->trackType() : qtractorTrack::None);
}


// Browse for audio clip filename.
void qtractorClipForm::browseFilename (void)
{
	QString sType;
	QString sExt;
	QString sFilter;

	qtractorTrack::TrackType clipType = trackType();
	switch (clipType) {
	case qtractorTrack::Audio:
		sType   = tr("Audio");
		sExt    = qtractorAudioFileFactory::defaultExt();
		sFilter = qtractorAudioFileFactory::filters();
		break;
	case qtractorTrack::Midi:
		sType   = tr("MIDI");
		sExt    = "mid";
		sFilter = tr("MIDI files (*.%1 *.smf *.midi)").arg(sExt);
		break;
	case qtractorTrack::None:
	default:
		return;
	}

	// Browse for file...
	QString sFilename;

	const QString& sTitle = tr("%1 Clip File").arg(sType) + " - " QTRACTOR_TITLE;
#if 1//QT_VERSION < 0x040400
	QFileDialog::Options options = 0;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sFilename = QFileDialog::getOpenFileName(this,
		sTitle, m_ui.FilenameComboBox->currentText(), sFilter, NULL, options);
#else
	QFileDialog fileDialog(this,
		sTitle, m_ui.FilenameComboBox->currentText(), sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
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
		if (pOptions->bDontUseNativeDialogs)
			fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	}
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
	if (pSession == NULL)
		return;

	// Do nothing else if file is invalid...
	QFileInfo fi(sFilename);

	if (!fi.exists()) {
		changed();
		return;
	}

	// Give as clip name hint if blank or new...
	if (m_ui.ClipNameLineEdit->text().isEmpty() || m_bClipNew)
		m_ui.ClipNameLineEdit->setText(fi.baseName());

	// Depending on the clip/track type,
	// set clip length to the file length (in frames)...
	unsigned long iClipLength = 0;

	switch (trackType()) {
	case qtractorTrack::Midi: {
		qtractorMidiFile file;
		if (file.open(sFilename)) {
			qtractorMidiSequence seq;
			seq.setTicksPerBeat(pSession->ticksPerBeat());
			if (file.readTrack(&seq, iTrackChannel) && seq.duration() > 0)
				iClipLength = pSession->frameFromTick(seq.duration());
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
