// qtractorSpinBox.cpp
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

#include "qtractorSpinBox.h"

#include <QLineEdit>


//----------------------------------------------------------------------------
// qtractorSpinBox -- A time-scale formatted spin-box widget.

// Constructor.
qtractorSpinBox::qtractorSpinBox ( QWidget *pParent )
	: QAbstractSpinBox(pParent)
{
	m_pTimeScale    = NULL;
	m_displayFormat = Frames;
	m_iDefaultValue = 0;
	m_iMinimumValue = 0;
	m_iMaximumValue = 0;

	QAbstractSpinBox::setAccelerated(true);

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
void qtractorSpinBox::setDisplayFormat (
	qtractorSpinBox::DisplayFormat displayFormat )
{
	m_displayFormat = displayFormat;

	QLineEdit *pLineEdit = QAbstractSpinBox::lineEdit();
	if (pLineEdit) {
		switch (m_displayFormat) {
		case BBT:
			pLineEdit->setInputMask("D.9D.999");
			break;
		case Time:
			pLineEdit->setInputMask("09:99:99.999");
			break;
		case Frames:
		default:
			pLineEdit->setInputMask(QString::null);
			break;
		}
	}

	setValue(m_iDefaultValue);
}

qtractorSpinBox::DisplayFormat qtractorSpinBox::displayFormat (void) const
{
	return m_displayFormat;
}


// Nominal value (in frames) accessors.
void qtractorSpinBox::setValue ( unsigned long iValue )
{
	if (iValue < m_iMinimumValue)
		iValue = m_iMinimumValue;
	if (iValue > m_iMaximumValue && m_iMaximumValue > m_iMinimumValue)
		iValue = m_iMaximumValue;
	
	bool bValueChanged = (iValue != m_iDefaultValue);

	m_iDefaultValue = iValue;
	QAbstractSpinBox::lineEdit()->setText(textFromValue(iValue));
	QAbstractSpinBox::interpretText();

	if (bValueChanged)
		emit valueChanged(iValue);
}

unsigned long qtractorSpinBox::value (void) const
{
	return valueFromText(QAbstractSpinBox::text());
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


// Inherited/override methods.
QValidator::State qtractorSpinBox::validate ( QString& sText, int& iPos ) const
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorSpinBox[%p]::validate(\"%s\",%d)\n",
		this, sText.toUtf8().constData(), iPos);
#endif

	if (iPos == 0)
		return QValidator::Acceptable;

	const QChar& ch = sText[iPos - 1];
	switch (m_displayFormat) {
	case Time:
		if (ch == ':')
			return QValidator::Acceptable;
		// Fall thru.
	case BBT:
		if (ch == '.')
			return QValidator::Acceptable;
		// Fall thru.
	case Frames:
	default:
		if (ch.isDigit())
			return QValidator::Acceptable;
		break;
	}

	return QValidator::Invalid;
}


void qtractorSpinBox::fixup ( QString& sText ) const
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorSpinBox[%p]::fixup(\"%s\")\n",
		this, sText.toUtf8().constData());
#endif

	sText = textFromValue(m_iDefaultValue);
}


void qtractorSpinBox::stepBy ( int iSteps )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorSpinBox[%p]::stepBy(%d)\n", this, iSteps);
#endif

	long iValue = long(value());

	switch (m_displayFormat) {
	case BBT:
		if (m_pTimeScale)
			iSteps *= int(m_pTimeScale->frameFromTick(1));
		break;
	case Time:
		if (m_pTimeScale)
			iSteps *= int(m_pTimeScale->sampleRate());
 		break;
	case Frames:
	default:
		break;
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
	unsigned long iValue;

	switch (m_displayFormat) {
	case BBT:
		if (m_pTimeScale) {
			// Time frame code in bars.beats.ticks ...
			unsigned int  bars  = sText.section('.', 0, 0).toUInt();
			unsigned int  beats = sText.section('.', 1, 1).toUInt();
			unsigned long ticks = sText.section('.', 2).toULong();
			if (bars > 0)
				bars--;
			if (beats > 0)
				beats--;
			beats += bars  * m_pTimeScale->beatsPerBar();
			ticks += beats * m_pTimeScale->ticksPerBeat();
			iValue = m_pTimeScale->frameFromTick(ticks);
			break;
		}
		// Fall thru...
	case Time:
		if (m_pTimeScale) {
			// Time frame code in hh:mm:ss.zzz ...
			unsigned int hh = sText.section(':', 0, 0).toUInt();
			unsigned int mm = sText.section(':', 1, 1).toUInt();
			float secs = sText.section(':', 2).toFloat();
			mm   += 60 * hh;
			secs += 60.f * (float) mm;
			iValue = (unsigned long) ::lroundf(
				secs * (float) m_pTimeScale->sampleRate());
			break;
		}
		// Fall thru...
	case Frames:
	default:
		iValue = sText.toULong();
		break;
	}

	return iValue;
}


QString qtractorSpinBox::textFromValue ( unsigned long iValue ) const
{
	QString sText;

	switch (m_displayFormat) {
	case BBT:
		if (m_pTimeScale) {
			// Time frame code in bars.beats.ticks ...
			unsigned int bars, beats;
			unsigned long ticks = m_pTimeScale->tickFromFrame(iValue);
			bars = beats = 0;
			if (ticks >= (unsigned long) m_pTimeScale->ticksPerBeat()) {
				beats  = (unsigned int)  (ticks / m_pTimeScale->ticksPerBeat());
				ticks -= (unsigned long) (beats * m_pTimeScale->ticksPerBeat());
			}
			if (beats >= (unsigned int) m_pTimeScale->beatsPerBar()) {
				bars   = (unsigned int) (beats / m_pTimeScale->beatsPerBar());
				beats -= (unsigned int) (bars  * m_pTimeScale->beatsPerBar());
			}
			sText.sprintf("%u.%02u.%03lu", bars + 1, beats + 1, ticks);
			break;
		}
		// Fall thru...
	case Time:
		if (m_pTimeScale) {
			// Time frame code in hh:mm:ss.zzz ...
			unsigned int hh, mm, ss, zzz;
			float secs = (float) iValue / (float) m_pTimeScale->sampleRate();
			hh = mm = ss = 0;
			if (secs >= 3600.0f) {
				hh = (unsigned int) (secs / 3600.0f);
				secs -= (float) hh * 3600.0f;
			}
			if (secs >= 60.0f) {
				mm = (unsigned int) (secs / 60.0f);
				secs -= (float) mm * 60.0f;
			}
			if (secs >= 0.0f) {
				ss = (unsigned int) secs;
				secs -= (float) ss;
			}
			zzz = (unsigned int) (secs * 1000.0f);
			sText.sprintf("%02u:%02u:%02u.%03u", hh, mm, ss, zzz);
			break;
		}
		// Fall thru...
	case Frames:
	default:
		sText = QString::number(iValue);
		break;
	}

	return sText;
}


// Pseudo-fixup slot.
void qtractorSpinBox::editingFinishedSlot (void)
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorSpinBox[%p]::editingFinishedSlot()\n", this);
#endif

	// Kind of final fixup.
	setValue(value());
}


// Textual value change notification.
void qtractorSpinBox::valueChangedSlot ( const QString& sText )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorSpinBox[%p]::valueChangedSlot(\"%s\")\n",
		this, sText.toUtf8().constData());
#endif

	// Forward this...
	emit valueChanged(sText);
}


// end of qtractorSpinBox.cpp
