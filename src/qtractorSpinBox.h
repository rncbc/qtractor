// qtractorSpinBox.h
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

#ifndef __qtractorSpinBox_h
#define __qtractorSpinBox_h

#include <qtractorTimeScale.h>

#include <QAbstractSpinBox>

// Forward declartions.
class QLineEdit;
class QShowEvent;


//----------------------------------------------------------------------------
// qtractorTimeSpinBox -- A time-scale formatted spin-box widget.

class qtractorTimeSpinBox : public QAbstractSpinBox
{
    Q_OBJECT

public:

	// Constructor.
	qtractorTimeSpinBox(QWidget *pParent = 0);

	// Time-scale accessors.
	void setTimeScale(qtractorTimeScale *pTimeScale);
	qtractorTimeScale *timeScale() const;

	// Display-format accessors.
	void setDisplayFormat(qtractorTimeScale::DisplayFormat displayFormat);
	qtractorTimeScale::DisplayFormat displayFormat() const;
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

	// Differential value mode (BBT format only) accessor.
	void setDeltaValue(bool bDeltaValue, unsigned long iDeltaValue = 0);
	bool isDeltaValue() const;
	unsigned long deltaValue() const;

	// Editing stabilizer.
	unsigned long valueFromText() const;

signals:

	// Common value change notification.
	void valueChanged(unsigned long);
	void valueChanged(const QString&);

	// Display format change notification.
	void displayFormatChanged(int);

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

	// Common value/text setlers.
	bool updateValue(unsigned long iValue, bool bNotifyChange);
	void updateText();

	// Local context menu handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

protected slots:

	// Pseudo-fixup slot.
	void editingFinishedSlot();
	void valueChangedSlot(const QString&);

private:

	// Instance variables.
	qtractorTimeScale *m_pTimeScale;
	qtractorTimeScale::DisplayFormat m_displayFormat;

	unsigned long      m_iValue;
	unsigned long      m_iMinimumValue;
	unsigned long      m_iMaximumValue;
	unsigned long      m_iDeltaValue;
	bool               m_bDeltaValue;

	int m_iValueChanged;
};


//----------------------------------------------------------------------------
// qtractorTempoSpinBox -- A tempo/time-signature formatted spin-box widget.

class qtractorTempoSpinBox : public QAbstractSpinBox
{
    Q_OBJECT

public:

	// Constructor.
	qtractorTempoSpinBox(QWidget *pParent = 0);

	// Nominal tempo value (BPM) accessors.
	void setTempo(float fTempo, bool bNotifyChange = true);
	float tempo() const;

	// Nominal time-signature numerator (beats/bar) accessors.
	void setBeatsPerBar(unsigned short iBeatsPerBar, bool bNotifyChange = true);
	unsigned short beatsPerBar() const;

	// Nominal time-signature denominator (beat-divisor) accessors.
	void setBeatDivisor(unsigned short iBeatDivisor, bool bNotifyChange = true);
	unsigned short beatDivisor() const;

signals:

	// Common value change notification.
	void valueChanged(float, unsigned short, unsigned short);
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
	float tempoFromText(const QString& sText) const;
	unsigned short beatsPerBarFromText(const QString& sText) const;
	unsigned short beatDivisorFromText(const QString& sText) const;

	QString textFromValue(float fTempo,
		unsigned short iBeatsPerBar, unsigned short iBeatDivisor) const;

	// Common value/text setlers.
	bool updateValue(float fTempo, unsigned short iBeatsPerBar,
		unsigned short iBeatDivisor, bool bNotifyChange);
	void updateText();

protected slots:

	// Pseudo-fixup slot.
	void valueChangedSlot(const QString&);
	void editingFinishedSlot();

private:

	// Instance variables.
	float          m_fTempo;
	unsigned short m_iBeatsPerBar;
	unsigned short m_iBeatDivisor;

	int m_iValueChanged;
};


#endif  // __qtractorSpinBox_h


// end of qtractorSpinBox.h
