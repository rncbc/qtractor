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
	if (m_iDirtyCount > 0) {
		// Make changes permanent...
		m_pClip->setClipName(m_ui.ClipNameLineEdit->text());
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
