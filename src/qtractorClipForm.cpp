// qtractorClipForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include <QFileDialog>
#include <QLineEdit>

// Needed for fabs(), logf() and powf()
#include <math.h>


//----------------------------------------------------------------------------
// qtractorClipForm -- UI wrapper form.

// Constructor.
qtractorClipForm::qtractorClipForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Initialize dirty control state.
	m_pClip       = NULL;
	m_bClipNew    = false;
	m_pTimeScale  = NULL;
	m_iDirtyCount = 0;
	m_iDirtySetup = 0;

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
	QObject::connect(m_ui.TrackChannelSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(trackChannelChanged(int)));
	QObject::connect(m_ui.FramesRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(formatChanged()));
	QObject::connect(m_ui.TimeRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(formatChanged()));
	QObject::connect(m_ui.BbtRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(formatChanged()));
	QObject::connect(m_ui.ClipStartSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipOffsetSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeInLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeInTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeOutLengthSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.FadeOutTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TimeStretchSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.PitchShiftSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
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
	m_iDirtySetup++;

	// Clip properties cloning...
	m_pClip = pClip;
	m_bClipNew = bClipNew;

	// Why not change the dialog icon accordingly?
	if (m_bClipNew)
		QDialog::setWindowIcon(QIcon(":/icons/editClipNew.png"));

	// Copy from global time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
	m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());

	m_ui.ClipStartSpinBox->setTimeScale(m_pTimeScale);
	m_ui.ClipOffsetSpinBox->setTimeScale(m_pTimeScale);
	m_ui.ClipLengthSpinBox->setTimeScale(m_pTimeScale);
	m_ui.FadeInLengthSpinBox->setTimeScale(m_pTimeScale);
	m_ui.FadeOutLengthSpinBox->setTimeScale(m_pTimeScale);
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
	switch (m_pTimeScale->displayFormat()) {
	case qtractorTimeScale::BBT:
		m_ui.BbtRadioButton->setChecked(true);
		break;
	case qtractorTimeScale::Time:
		m_ui.TimeRadioButton->setChecked(true);
		break;
	case qtractorTimeScale::Frames:
	default:
		m_ui.FramesRadioButton->setChecked(true);
		break;
	}

	// Now those things specific on track type...
	const QString sSuffix = m_ui.FilenameComboBox->objectName();
	switch (trackType()) {
	case qtractorTrack::Audio: {
		m_ui.FilenameComboBox->setObjectName("Audio" + sSuffix);
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
	m_iDirtySetup--;
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
		unsigned long iClipStart  = m_ui.ClipStartSpinBox->value();
		unsigned long iClipOffset = m_ui.ClipOffsetSpinBox->value();
		unsigned long iClipLength = m_ui.ClipLengthSpinBox->value();
		unsigned long iFadeInLength = m_ui.FadeInLengthSpinBox->value();
		qtractorClip::FadeType fadeInType
			= fadeTypeFromIndex(m_ui.FadeInTypeComboBox->currentIndex());
		unsigned long iFadeOutLength = m_ui.FadeOutLengthSpinBox->value();
		qtractorClip::FadeType fadeOutType
			= fadeTypeFromIndex(m_ui.FadeOutTypeComboBox->currentIndex());
		float fTimeStretch = 0.01f * m_ui.TimeStretchSpinBox->value();
		float fPitchShift = ::powf(2.0f, m_ui.PitchShiftSpinBox->value() / 12.0f);
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
					iFileChange++;
				}
				break;
			}
			case qtractorTrack::Midi: {
				qtractorMidiClip *pMidiClip
					= static_cast<qtractorMidiClip *> (m_pClip);
				if (pMidiClip) {
					pMidiClip->setTrackChannel(iTrackChannel);
					iFileChange++;
				}
				break;
			}
			case qtractorTrack::None:
			default:
				break;
			}
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
				iFileChange++;
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
						iFileChange++;
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
			// Parameters and/or time-stretching changes...
			if (iClipStart  != m_pClip->clipStart()  ||
				iClipOffset != m_pClip->clipOffset() ||
				iClipLength != m_pClip->clipLength() ||
				fTimeStretch > 0.0f) {
				pClipCommand->resizeClip(m_pClip,
					iClipStart, iClipOffset, iClipLength, fTimeStretch);
			}
			// Pitch-shifting changes...
			if (fPitchShift > 0.0f)
				pClipCommand->pitchShiftClip(m_pClip, fPitchShift);
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
			(pSession->commands())->exec(pClipCommand);
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
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			tr("Apply"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Apply...
			accept();
			return;
		case 1:     // Discard
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
	m_iDirtyCount++;
	stabilizeForm();
}


// Display format has changed.
void qtractorClipForm::formatChanged (void)
{
	qtractorTimeScale::DisplayFormat displayFormat = qtractorTimeScale::Frames;

	if (m_ui.TimeRadioButton->isChecked())
		displayFormat = qtractorTimeScale::Time;
	else
	if (m_ui.BbtRadioButton->isChecked())
		displayFormat= qtractorTimeScale::BBT;

	if (m_pTimeScale) {
		// Set from local time-scale instance...
		m_pTimeScale->setDisplayFormat(displayFormat);
		m_ui.ClipStartSpinBox->updateDisplayFormat();
		m_ui.ClipOffsetSpinBox->updateDisplayFormat();
		m_ui.ClipLengthSpinBox->updateDisplayFormat();
		m_ui.FadeInLengthSpinBox->updateDisplayFormat();
		m_ui.FadeOutLengthSpinBox->updateDisplayFormat();
	}

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
	m_ui.OkPushButton->setEnabled(bValid);
}


// Fade type index converters.
qtractorClip::FadeType qtractorClipForm::fadeTypeFromIndex ( int iIndex ) const
{
	qtractorClip::FadeType fadeType = qtractorClip::Linear;

	switch (iIndex) {
	case 1:
		fadeType = qtractorClip::Quadratic;
		break;
	case 2:
		fadeType = qtractorClip::Cubic;
		break;
	default:
		break;
	}

	return fadeType;
}

int qtractorClipForm::indexFromFadeType ( qtractorClip::FadeType fadeType ) const
{
	int iIndex = 0;	// qtractorClip::Linear

	switch (fadeType) {
	case qtractorClip::Quadratic:
		iIndex = 1;
		break;
	case qtractorClip::Cubic:
		iIndex = 2;
		break;
	default:
		break;
	}

	return iIndex;
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
	QString sFilter;

	switch (trackType()) {
	case qtractorTrack::Audio:
		sType   = tr("Audio");
		sFilter = qtractorAudioFileFactory::filters();
		break;
	case qtractorTrack::Midi:
		sType   = tr("MIDI");
		sFilter = tr("MIDI Files (*.mid)");
		break;
	case qtractorTrack::None:
	default:
		return;
	}

	// Browse for audio file...
	QString sFilename = QFileDialog::getOpenFileName(
		this, tr("%1 Clip File").arg(sType),
		m_ui.FilenameComboBox->currentText(), sFilter);

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


// end of qtractorClipForm.cpp
