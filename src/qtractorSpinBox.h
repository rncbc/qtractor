// qtractorSpinBox.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorSpinBox_h
#define __qtractorSpinBox_h

#include <qtractorTimeScale.h>

#include <QAbstractSpinBox>

// Forward declartions.
class QLineEdit;
class QShowEvent;


//----------------------------------------------------------------------------
// qtractorSpinBox -- A time-scale formatted spin-box widget.

class qtractorSpinBox : public QAbstractSpinBox
{
    Q_OBJECT

public:

	// Constructor.
	qtractorSpinBox(QWidget *pParent = 0);
	// Destructor.
	~qtractorSpinBox();

	// Time-scale accessors.
	void setTimeScale(qtractorTimeScale *pTimeScale);
	qtractorTimeScale *timeScale() const;

	// Display-format accessors.
	qtractorTimeScale::DisplayFormat displayFormat (void) const;
	void updateDisplayFormat();

	// Nominal value (in frames) accessors.
	void setValue(unsigned long iValue, bool bNotifyChange = true);
	unsigned long value() const;

	// Minimum value (in frames) accessors.
	void setMinimum(unsigned long iMinimum);
	unsigned long minimum() const;

	// Maximum value (in frames) accessors.
	void setMaximum(unsigned long iMaximum);
	unsigned long maximum() const;

signals:

	// Common value change notification.
	void valueChanged(unsigned long);
	void valueChanged(const QString&);

protected:

	// Mark that we got actual value.
	void showEvent(QShowEvent *);

	// Inherited/override methods.
	QValidator::State validate(QString& sText,int& iPos) const;
	void fixup(QString& sText) const;
	void stepBy(int iSteps);
	StepEnabled stepEnabled() const;

	// Value/text format converters.
	unsigned long valueFromText(const QString& sText) const;
	QString textFromValue(unsigned long iValue) const;

protected slots:

	// Pseudo-fixup slot.
	void editingFinishedSlot();
	void valueChangedSlot(const QString&);

private:

	// Instance variables.
	qtractorTimeScale *m_pTimeScale;
	unsigned long      m_iDefaultValue;
	unsigned long      m_iMinimumValue;
	unsigned long      m_iMaximumValue;
};


#endif  // __qtractorSpinBox_h


// end of qtractorSpinBox.h
