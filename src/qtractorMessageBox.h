// qtractorMessageBox.h
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

#ifndef __qtractorMessageBox_h
#define __qtractorMessageBox_h

#include <QDialog>
#include <QMessageBox>


// Forward decls.
class QVBoxLayout;
class QAbstractButton;
class QDialogButtonBox;
class QLabel;


//----------------------------------------------------------------------------
// qtractorMessageBox -- UI wrapper form.

class qtractorMessageBox: public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMessageBox(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);

	// Accessors.
	void setText(const QString& sText);
	QString text() const;

	void setIcon(QMessageBox::Icon icon);
	QMessageBox::Icon icon() const;

	void setIconPixmap(const QPixmap& pixmap);
	QPixmap iconPixmap() const;

	void setStandardButtons(QMessageBox::StandardButtons buttons);
	QMessageBox::StandardButtons standardButtons() const;

	void addCustomButton(QAbstractButton *pButton);
	void addCustomSpacer();

	void addButton(QAbstractButton *pButton, QMessageBox::ButtonRole role);

protected slots:

	// Dialog slots.
	void standardButtonClicked(QAbstractButton *);

private:

	// UI structs.
	QMessageBox::Icon m_icon;
	QLabel           *m_pIconLabel;
	QLabel           *m_pTextLabel;
	QVBoxLayout      *m_pCustomButtonLayout;
	QDialogButtonBox *m_pDialogButtonBox;
};


#endif	// __qtractorMessageBox_h

// end of qtractorMessageBox.h

