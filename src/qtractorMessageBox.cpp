// qtractorMessageBox.cpp
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMessageBox.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QLabel>


//----------------------------------------------------------------------------
// qtractorMessageBox -- UI wrapper form.

qtractorMessageBox::qtractorMessageBox ( QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags), m_icon(QMessageBox::NoIcon)
{
	QDialog::setWindowFlags(wflags
		| Qt::Dialog
		| Qt::CustomizeWindowHint
		| Qt::WindowTitleHint
		| Qt::WindowCloseButtonHint);

	QGridLayout *pGridLayout = new QGridLayout();
	pGridLayout->setMargin(8);
	pGridLayout->setSpacing(2);

	m_pIconLabel = new QLabel();
	m_pIconLabel->setMargin(8);

	m_pTextLabel = new QLabel();
	m_pTextLabel->setMargin(8);
	m_pTextLabel->setOpenExternalLinks(true);

	m_pCustomButtonLayout = new QVBoxLayout();
	m_pCustomButtonLayout->setMargin(8);
	m_pCustomButtonLayout->setSpacing(2);

	m_pDialogButtonBox = new QDialogButtonBox();

	pGridLayout->addWidget(m_pIconLabel, 0, 0);
	pGridLayout->addWidget(m_pTextLabel, 0, 1);
	pGridLayout->addLayout(m_pCustomButtonLayout, 1, 1);
	pGridLayout->addWidget(m_pDialogButtonBox, 2, 0, 1, 2);
	pGridLayout->setColumnStretch(1, 2);
	pGridLayout->setRowStretch(1, 2);
	QDialog::setLayout(pGridLayout);

	// Dialog commands...
	QObject::connect(m_pDialogButtonBox,
		SIGNAL(clicked(QAbstractButton *)),
		SLOT(standardButtonClicked(QAbstractButton *)));
}


// Accessors.
void qtractorMessageBox::setText ( const QString& sText )
{
	m_pTextLabel->setText(sText);
}

QString qtractorMessageBox::text (void) const
{
	return m_pTextLabel->text();
}


void qtractorMessageBox::setIcon ( QMessageBox::Icon icon )
{
	if (m_icon != icon) {
		m_icon  = icon;
		QMessageBox mbox;
		mbox.setIcon(icon);
		m_pIconLabel->setPixmap(mbox.iconPixmap());
	}
}

QMessageBox::Icon qtractorMessageBox::icon (void) const
{
	return m_icon;
}


void qtractorMessageBox::setIconPixmap ( const QPixmap& pixmap )
{
	m_pIconLabel->setPixmap(pixmap);
}

QPixmap qtractorMessageBox::iconPixmap (void) const
{
	return *m_pIconLabel->pixmap();
}


void qtractorMessageBox::setStandardButtons (
	QMessageBox::StandardButtons buttons )
{
	m_pDialogButtonBox->setStandardButtons(
		QDialogButtonBox::StandardButton(int(buttons)));
}

QMessageBox::StandardButtons qtractorMessageBox::standardButtons (void) const
{
	return QMessageBox::StandardButtons(
		int(m_pDialogButtonBox->standardButtons()));
}


void qtractorMessageBox::addCustomButton ( QAbstractButton *pButton )
{
	m_pCustomButtonLayout->addWidget(pButton);
}


void qtractorMessageBox::addCustomSpacer (void)
{
	m_pCustomButtonLayout->addItem(new QSpacerItem(20, 20,
		QSizePolicy::Minimum, QSizePolicy::Expanding));
}


void qtractorMessageBox::addButton(
	QAbstractButton *pButton, QMessageBox::ButtonRole role )
{
	m_pDialogButtonBox->addButton(pButton,
		QDialogButtonBox::ButtonRole(int(role)));
}


// Dialog slots.
void qtractorMessageBox::standardButtonClicked ( QAbstractButton *pButton )
{
	const QDialogButtonBox::ButtonRole role
		= m_pDialogButtonBox->buttonRole(pButton);

	switch (role) {
	case QDialogButtonBox::AcceptRole:
		// Just go with dialog acceptance.
		QDialog::accept();
		break;
	case QDialogButtonBox::RejectRole:
		// Just go with dialog rejection.
		QDialog::reject();
		break;
	default:
		break;
	}

	QDialog::setResult(int(m_pDialogButtonBox->standardButton(pButton)));
}


// end of qtractorMessageBox.cpp
