// qtractorSessionForm.cpp
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

#include "qtractorSessionForm.h"

#include "qtractorAbout.h"
#include "qtractorSession.h"

#include "qtractorOptions.h"

#include <QFileDialog>
#include <QUrl>

#include <QMessageBox>
#include <QPushButton>
#include <QValidator>
#include <QLineEdit>

#include <math.h>


//----------------------------------------------------------------------------
// qtractorSessionForm -- UI wrapper form.

// Constructor.
qtractorSessionForm::qtractorSessionForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize conveniency options...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->loadComboBoxHistory(m_ui.SessionDirComboBox);

	// Setup some specific validators.
	m_ui.SampleRateComboBox->setValidator(
		new QIntValidator(m_ui.SampleRateComboBox));

	// Fill-up snap-per-beat items...
	const QIcon snapIcon(":/images/itemBeat.png");
	const QStringList& snapItems = qtractorTimeScale::snapItems(0);
	QStringListIterator snapIter(snapItems);
	m_ui.SnapPerBeatComboBox->clear();
	m_ui.SnapPerBeatComboBox->setIconSize(QSize(8, 16));
//	snapIter.toFront();
	if (snapIter.hasNext())
		m_ui.SnapPerBeatComboBox->addItem(
			QIcon(":/images/itemNone.png"), snapIter.next());
	while (snapIter.hasNext())
		m_ui.SnapPerBeatComboBox->addItem(snapIcon, snapIter.next());
//	m_ui.SnapPerBeatComboBox->insertItems(0, snapItems);

	// Initialize dirty control state.
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.SessionNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionDirComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.SessionDirToolButton,
		SIGNAL(clicked()),
		SLOT(browseSessionDir()));
	QObject::connect(m_ui.DescriptionTextEdit,
		SIGNAL(textChanged()),
		SLOT(changed()));
	QObject::connect(m_ui.SampleRateComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(changed()));
	QObject::connect(m_ui.TicksPerBeatSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.SnapPerBeatComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.PixelsPerBeatSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.HorizontalZoomSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.VerticalZoomSpinBox,
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
qtractorSessionForm::~qtractorSessionForm (void)
{
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorSessionForm::setSession ( qtractorSession *pSession )
{
	// Session properties cloning...
	m_props = pSession->properties();

	// Initialize dialog widgets...
	m_ui.SessionNameLineEdit->setText(m_props.sessionName);
	m_ui.SessionDirComboBox->setEditText(m_props.sessionDir);
	m_ui.DescriptionTextEdit->setPlainText(m_props.description);
	// Time properties...
	m_ui.SampleRateComboBox->setEditText(
		QString::number(m_props.timeScale.sampleRate()));
	m_ui.SampleRateTextLabel->setEnabled(!pSession->isActivated());
	m_ui.SampleRateComboBox->setEnabled(!pSession->isActivated());
	m_ui.TempoSpinBox->setTempo(m_props.timeScale.tempo(), false);
	m_ui.TempoSpinBox->setBeatsPerBar(m_props.timeScale.beatsPerBar(), false);
	m_ui.TempoSpinBox->setBeatDivisor(m_props.timeScale.beatDivisor(), false);
	m_ui.TicksPerBeatSpinBox->setValue(int(m_props.timeScale.ticksPerBeat()));
	// View properties...
	m_ui.SnapPerBeatComboBox->setCurrentIndex(
		qtractorTimeScale::indexFromSnap(m_props.timeScale.snapPerBeat()));
	m_ui.PixelsPerBeatSpinBox->setValue(int(m_props.timeScale.pixelsPerBeat()));
	m_ui.HorizontalZoomSpinBox->setValue(int(m_props.timeScale.horizontalZoom()));
	m_ui.VerticalZoomSpinBox->setValue(int(m_props.timeScale.verticalZoom()));

	// Start editing session name, if empty...
	if (m_props.sessionName.isEmpty())
		m_ui.SessionNameLineEdit->setFocus();

	// Backup clean.
	m_iDirtyCount = 0;

	// Done.
	stabilizeForm();
}


// Retrieve the accepted session properties, if the case arises.
const qtractorSession::Properties& qtractorSessionForm::properties (void)
{
	return m_props;
}


// Accept settings (OK button slot).
void qtractorSessionForm::accept (void)
{
	// Check if session directory is new...
	QDir dir;
	const QString& sSessionDir = m_ui.SessionDirComboBox->currentText();
	while (!dir.exists(sSessionDir)) {
		// Ask user...
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Session directory does not exist:\n\n"
			"\"%1\"\n\n"
			"Do you want to create it?").arg(sSessionDir),
			QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel)
			return;
		// Proceed...
		dir.mkdir(sSessionDir);
	}

	// Save options...
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_props.sessionName = m_ui.SessionNameLineEdit->text();
		m_props.sessionDir  = m_ui.SessionDirComboBox->currentText();
		m_props.description = m_ui.DescriptionTextEdit->toPlainText();
		// Time properties...
		m_props.timeScale.setSampleRate(
			m_ui.SampleRateComboBox->currentText().toUInt());
		m_props.timeScale.setTempo(m_ui.TempoSpinBox->tempo());
		m_props.timeScale.setBeatType(2);
		m_props.timeScale.setBeatsPerBar(m_ui.TempoSpinBox->beatsPerBar());
		m_props.timeScale.setBeatDivisor(m_ui.TempoSpinBox->beatDivisor());
		m_props.timeScale.setTicksPerBeat(m_ui.TicksPerBeatSpinBox->value());
		// View properties...
		m_props.timeScale.setSnapPerBeat(qtractorTimeScale::snapFromIndex(
			m_ui.SnapPerBeatComboBox->currentIndex()));
		m_props.timeScale.setPixelsPerBeat(m_ui.PixelsPerBeatSpinBox->value());
		m_props.timeScale.setHorizontalZoom(m_ui.HorizontalZoomSpinBox->value());
		m_props.timeScale.setVerticalZoom(m_ui.VerticalZoomSpinBox->value());
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Save other conveniency options...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->saveComboBoxHistory(m_ui.SessionDirComboBox);

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorSessionForm::reject (void)
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
void qtractorSessionForm::changed (void)
{
	++m_iDirtyCount;
	stabilizeForm();
}


// Stabilize current form state.
void qtractorSessionForm::stabilizeForm (void)
{
	QFileInfo fi(QFileInfo(m_ui.SessionDirComboBox->currentText()).path());

	bool bValid = !m_ui.SessionNameLineEdit->text().isEmpty();
	bValid = bValid && fi.isDir() && fi.isReadable() && fi.isWritable();
//	bValid = bValid && !m_ui.DescriptionTextEdit->text().isEmpty();
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);
}


// Browse for session directory.
void qtractorSessionForm::browseSessionDir (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	const QString& sTitle = tr("Session Directory") + " - " QTRACTOR_TITLE;
#if 1// QT_VERSION < 0x040400
	QFileDialog::Options options = QFileDialog::ShowDirsOnly;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	QString sSessionDir = QFileDialog::getExistingDirectory(this,                                  // Parent.
		sTitle, m_ui.SessionDirComboBox->currentText());
#else
	// Construct open-directory dialog...
	QFileDialog fileDialog(this,
		sTitle, m_ui.SessionDirComboBox->currentText());
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::DirectoryOnly);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	fileDialog.setSidebarUrls(urls);
	if (pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (!fileDialog.exec())
		return;
	QString sSessionDir = fileDialog.selectedFiles().first();
#endif

    if (sSessionDir.isEmpty())
		return;

	m_ui.SessionDirComboBox->setEditText(sSessionDir);
	m_ui.SessionDirComboBox->setFocus();
	changed();
}


// end of qtractorSessionForm.cpp
