// qtractorClipForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMainForm.h"

#include <QMessageBox>
#include <QLineEdit>


//----------------------------------------------------------------------------
// qtractorClipForm -- UI wrapper form.

// Constructor.
qtractorClipForm::qtractorClipForm (
	QWidget *pParent, Qt::WFlags wflags ) : QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Initialize dirty control state.
	m_pClip = NULL;
	m_iDirtyCount = 0;

	// Try to restore old window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.ClipNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipStartSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipOffsetSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ClipLengthSpinBox,
		SIGNAL(valueChanged(int)),
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
}


// Populate (setup) dialog controls from settings descriptors.
void qtractorClipForm::setClip ( qtractorClip *pClip )
{
	// Clip properties cloning...
	m_pClip = pClip;

	// Initialize dialog widgets...
	m_ui.ClipNameLineEdit->setText(m_pClip->clipName());
	m_ui.ClipStartSpinBox->setValue(m_pClip->clipStart());
	m_ui.ClipOffsetSpinBox->setValue(m_pClip->clipOffset());
	m_ui.ClipLengthSpinBox->setValue(m_pClip->clipLength());

	// Backup clean.
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
	// Save options...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (m_iDirtyCount > 0 && m_pClip && pMainForm) {
		// Make changes undoable...
		qtractorClipCommand *pClipCommand
			= new qtractorClipCommand(tr("edit clip"));
		pClipCommand->renameClip(m_pClip, m_ui.ClipNameLineEdit->text());
		unsigned long iClipStart  = m_ui.ClipStartSpinBox->value();
		unsigned long iClipOffset = m_ui.ClipOffsetSpinBox->value();
		unsigned long iClipLength = m_ui.ClipLengthSpinBox->value();
		if (iClipStart  != m_pClip->clipStart()  ||
			iClipOffset != m_pClip->clipOffset() ||
			iClipLength != m_pClip->clipLength()) {
			pClipCommand->resizeClip(m_pClip,
				iClipStart, iClipOffset, iClipLength);
		}
		// Do it (but make it undoable)...
		pMainForm->commands()->exec(pClipCommand);
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


// Stabilize current form state.
void qtractorClipForm::stabilizeForm (void)
{
	bool bValid = (m_iDirtyCount > 0);
	bValid = bValid && !m_ui.ClipNameLineEdit->text().isEmpty();
	m_ui.OkPushButton->setEnabled(bValid);
}


// end of qtractorClipForm.cpp
