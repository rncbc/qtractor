// qtractorSpinBox.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSpinBox.h"

#include <QLineEdit>


//----------------------------------------------------------------------------
// qtractorSpinBox -- A time-scale formatted spin-box widget.

// Constructor.
qtractorSpinBox::qtractorSpinBox ( QWidget *pParent )
	: QAbstractSpinBox(pParent)
{
	m_pTimeScale    = NULL;
	m_iDefaultValue = 0;
	m_iMinimumValue = 0;
	m_iMaximumValue = 0;
	m_iDeltaValue   = 0;
	m_bDeltaValue   = false;

#if QT_VERSION >= 0x040200
	QAbstractSpinBox::setAccelerated(true);
#endif

	QObject::connect(this,
		SIGNAL(editingFinished()),
		SLOT(editingFinishedSlot()));
	QObject::connect(QAbstractSpinBox::lineEdit(),
		SIGNAL(textChanged(const QString&)),
		SLOT(valueChangedSlot(const QString&)));
}


// Destructor.
qtractorSpinBox::~qtractorSpinBox (void)
{
}


// Mark that we got actual value.
void qtractorSpinBox::showEvent ( QShowEvent */*pShowEvent*/ )
{
	QAbstractSpinBox::lineEdit()->setText(textFromValue(m_iDefaultValue));
	QAbstractSpinBox::interpretText();
}


// Time-scale accessors.
void qtractorSpinBox::setTimeScale ( qtractorTimeScale *pTimeScale )
{
	m_pTimeScale = pTimeScale;
}

qtractorTimeScale *qtractorSpinBox::timeScale (void) const
{
	return m_pTimeScale;
}


// Display-format accessors.
qtractorTimeScale::DisplayFormat qtractorSpinBox::displayFormat (void) const
{
	return (m_pTimeScale
		? m_pTimeScale->displayFormat()
		: qtractorTimeScale::Frames);
}

void qtractorSpinBox::updateDisplayFormat (void)
{
	setValue(m_iDefaultValue);
}


// Nominal value (in frames) accessors.
void qtractorSpinBox::setValue ( unsigned long iValue, bool bNotifyChange )
{
	if (iValue < m_iMinimumValue)
		iValue = m_iMinimumValue;
	if (iValue > m_iMaximumValue && m_iMaximumValue > m_iMinimumValue)
		iValue = m_iMaximumValue;
	
	bool bValueChanged = (iValue != m_iDefaultValue);

	m_iDefaultValue = iValue;

	if (QAbstractSpinBox::isVisible()) {
		QAbstractSpinBox::lineEdit()->setText(textFromValue(iValue));
		QAbstractSpinBox::interpretText();
		if (bNotifyChange && bValueChanged)
			emit valueChanged(iValue);
	}
}

unsigned long qtractorSpinBox::value (void) const
{
	if (QAbstractSpinBox::isVisible()) {
		return valueFromText(QAbstractSpinBox::text());
	} else {
		return m_iDefaultValue;
	}
}


// Minimum value (in frames) accessors.
void qtractorSpinBox::setMinimum ( unsigned long iMinimum )
{
	m_iMinimumValue = iMinimum;
}

unsigned long qtractorSpinBox::minimum (void) const
{
	return m_iMinimumValue;
}


// Maximum value (in frames) accessors.
void qtractorSpinBox::setMaximum ( unsigned long iMaximum )
{
	m_iMaximumValue = iMaximum;
}

unsigned long qtractorSpinBox::maximum (void) const
{
	return m_iMaximumValue;
}


// Differential value mode (BBT format only) accessor.
void qtractorSpinBox::setDeltaValue ( bool bDeltaValue, unsigned long iDeltaValue )
{
	m_bDeltaValue = bDeltaValue;
	m_iDeltaValue = iDeltaValue;
}

bool qtractorSpinBox::isDeltaValue (void) const
{
	return m_bDeltaValue;
}

unsigned long qtractorSpinBox::deltaValue (void) const
{
	return m_iDeltaValue;
}


// Inherited/override methods.
QValidator::State qtractorSpinBox::validate ( QString& sText, int& iPos ) const
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSpinBox[%p]::validate(\"%s\",%d)",
		this, sText.toUtf8().constData(), iPos);
#endif

	if (iPos == 0)
		return QValidator::Acceptable;

	const QChar& ch = sText[iPos - 1];
	if (m_pTimeScale) {
		switch (m_pTimeScale->displayFormat()) {
		case qtractorTimeScale::Time:
			if (ch == ':')
				return QValidator::Acceptable;
			// Fall thru.
		case qtractorTimeScale::BBT:
			if (ch == '.')
				return QValidator::Acceptable;
			// Fall thru.
		case qtractorTimeScale::Frames:
		default:
			if (ch.isDigit())
				return QValidator::Acceptable;
			break;
		}
	}

	return QValidator::Invalid;
}


void qtractorSpinBox::fixup ( QString& sText ) const
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSpinBox[%p]::fixup(\"%s\")",
		this, sText.toUtf8().constData());
#endif

	sText = textFromValue(m_iDefaultValue);
}


void qtractorSpinBox::stepBy ( int iSteps )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSpinBox[%p]::stepBy(%d)", this, iSteps);
#endif

	long iValue = long(value());

	if (m_pTimeScale) {
		switch (m_pTimeScale->displayFormat()) {
		case qtractorTimeScale::BBT:
			iSteps *= int(m_pTimeScale->frameFromTick(1));
			break;
		case qtractorTimeScale::Time:
			iSteps *= int(m_pTimeScale->sampleRate());
			break;
		case qtractorTimeScale::Frames:
		default:
			break;
		}
	}

	iValue += iSteps;
	if (iValue < 0)
		iValue = 0;
	setValue(iValue);
}


QAbstractSpinBox::StepEnabled qtractorSpinBox::stepEnabled (void) const
{
	StepEnabled flags = StepUpEnabled;
	if (value() > 0)
		flags |= StepDownEnabled;
	return flags;
}


// Value/text format converters.
unsigned long qtractorSpinBox::valueFromText ( const QString& sText ) const
{
	return (m_pTimeScale
		? m_pTimeScale->frameFromText(sText, m_bDeltaValue, m_iDeltaValue)
		: sText.toULong());
}

QString qtractorSpinBox::textFromValue ( unsigned long iValue ) const
{
	return (m_pTimeScale
		? (m_bDeltaValue
			? m_pTimeScale->textFromFrame(m_iDeltaValue, true, iValue)
			: m_pTimeScale->textFromFrame(iValue))
		: QString::number(iValue));
}


// Pseudo-fixup slot.
void qtractorSpinBox::editingFinishedSlot (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSpinBox[%p]::editingFinishedSlot()", this);
#endif

	// Kind of final fixup.
	setValue(value());
}


// Textual value change notification.
void qtractorSpinBox::valueChangedSlot ( const QString& sText )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorSpinBox[%p]::valueChangedSlot(\"%s\")",
		this, sText.toUtf8().constData());
#endif

	// Forward this...
	emit valueChanged(sText);
}


// end of qtractorSpinBox.cpp
