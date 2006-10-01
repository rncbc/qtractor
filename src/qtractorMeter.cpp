// qtractorMeter.cpp
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

#include "qtractorMeter.h"
#include "qtractorSlider.h"
#include "qtractorSpinBox.h"

#include <qtooltip.h>
#include <qpainter.h>
#include <qlabel.h>


//----------------------------------------------------------------------------
// qtractorMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorMeterScale::qtractorMeterScale ( qtractorMeter *pMeter,
	QWidget *pParent, const char *pszName ) : QWidget(pParent, pszName)
{
	m_pMeter = pMeter;
	m_iLastY = 0;

	QWidget::setMinimumWidth(12);
//	QWidget::setBackgroundMode(Qt::PaletteMid);
}

// Default destructor.
qtractorMeterScale::~qtractorMeterScale (void)
{
}

// Meter accessor.
qtractorMeter *qtractorMeterScale::meter (void) const
{
	return m_pMeter;
}


// Draw scale line and label; assumes labels drawed from top to bottom.
void qtractorMeterScale::drawLineLabel ( QPainter *p,
	int y, const char* pszLabel )
{
	int iCurrY = QWidget::height() - y;
	int iWidth = QWidget::width();

	const QFontMetrics& fm = p->fontMetrics();
	int iMidHeight = (fm.height() >> 1);

	if (fm.width(pszLabel) < iWidth - 5) {
		p->drawLine(0, iCurrY, 2, iCurrY);
		p->drawLine(iWidth - 3, iCurrY, iWidth - 1, iCurrY);
	}

	if (iCurrY > m_iLastY + iMidHeight) {
		p->drawText(2, iCurrY - iMidHeight, iWidth - 3, fm.height(),
			Qt::AlignHCenter | Qt::AlignVCenter, pszLabel);
		m_iLastY = iCurrY + 1;
	}
}


// Paint event handler.
void qtractorMeterScale::paintEvent ( QPaintEvent * )
{
	QPainter p(this);

	p.setFont(QFont(QWidget::font().family(), 5));
	p.setPen(Qt::darkGray);

	m_iLastY = 0;

	paintScale(&p);
}


// Resize event handler.
void qtractorMeterScale::resizeEvent ( QResizeEvent * )
{
	QWidget::repaint(true);
}


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

// Constructor.
qtractorMeter::qtractorMeter ( QWidget *pParent, const char *pszName )
	: QVBox(pParent, pszName)
{
	QFont font8(font().family(), 8);
	QFontMetrics fm(font8);

	m_pPanSlider   = new qtractorSlider(Qt::Horizontal, this);
	m_pPanSlider->setFixedHeight(20);
	m_pPanSpinBox  = new qtractorSpinBox(this);
	m_pPanSpinBox->setFont(font8);
	m_pPanSpinBox->setFixedHeight(fm.lineSpacing() + 2);
	m_pTopLabel    = new QLabel(this);
	m_pHBox        = new QHBox(this);
	m_pHBox->setSpacing(1);
	m_pGainSlider  = new qtractorSlider(Qt::Vertical, m_pHBox);
	m_pGainSlider->setFixedWidth(20);
	m_pGainSpinBox = new qtractorSpinBox(this);
	m_pGainSpinBox->setFont(font8);
	m_pGainSpinBox->setFixedHeight(fm.lineSpacing() + 2);

	m_iUpdate      = 0;
	m_iPeakFalloff = 0;

	m_pPanSlider->setTickmarks(QSlider::NoMarks);
	m_pPanSlider->setMinValue(-100);
	m_pPanSlider->setMaxValue(+100);
	m_pPanSlider->setPageStep(10);
	m_pPanSlider->setLineStep(1);
	m_pPanSlider->setDefaultValue(0);

	m_pPanSpinBox->setAlignment(Qt::AlignHCenter);
	m_pPanSpinBox->setMinValueFloat(-1.0f);
	m_pPanSpinBox->setMaxValueFloat(+1.0f);
	QToolTip::add(m_pPanSpinBox, tr("Pan"));

	m_pGainSlider->setTickmarks(QSlider::NoMarks);
	m_pGainSlider->setMinValue(0);
	m_pGainSlider->setMaxValue(10000);
	m_pGainSlider->setPageStep(1000);
	m_pGainSlider->setLineStep(100);
	m_pGainSlider->setDefaultValue(0);

	m_pGainSpinBox->setAlignment(Qt::AlignHCenter);
	QToolTip::add(m_pGainSpinBox, tr("Gain"));

	QVBox::setMinimumHeight(140);
	QVBox::setSpacing(2);
	QVBox::setSizePolicy(
		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

	QObject::connect(m_pPanSlider, SIGNAL(valueChanged(int)),
		this, SLOT(panSliderChangedSlot(int)));
	QObject::connect(m_pPanSpinBox, SIGNAL(valueChanged(const QString&)),
		this, SLOT(panSpinBoxChangedSlot(const QString&)));
	QObject::connect(m_pGainSlider, SIGNAL(valueChanged(int)),
		this, SLOT(gainSliderChangedSlot(int)));
	QObject::connect(m_pGainSpinBox, SIGNAL(valueChanged(const QString&)),
		this, SLOT(gainSpinBoxChangedSlot(const QString&)));
}


// Default destructor.
qtractorMeter::~qtractorMeter (void)
{
	// No need to delete child widgets, Qt does it all for us
	delete m_pGainSpinBox;
	delete m_pGainSlider;
	delete m_pHBox;
	delete m_pTopLabel;
	delete m_pPanSlider;
	delete m_pPanSpinBox;
}


// Dynamic layout accessors.
QLabel *qtractorMeter::topLabel (void) const
{
	return m_pTopLabel;
}


QHBox *qtractorMeter::hbox (void) const
{
	return m_pHBox;
}


// Stereo panning accessors.
void qtractorMeter::setPanning ( float fPanning )
{
	m_iUpdate++;
	m_pPanSlider->setValue(int(100.0f * fPanning));
	m_pPanSpinBox->setValueFloat(fPanning);
	m_iUpdate--;
}

float qtractorMeter::panning (void) const
{
	return float(m_pPanSlider->value()) / 100.0f;
}


// Gain accessors.
void qtractorMeter::setGain ( float fGain )
{
	m_iUpdate++;
	m_pGainSlider->setValue(10000 - int(10000.0f * scaleFromGain(fGain)));
	m_pGainSpinBox->setValueFloat(valueFromGain(fGain));
	m_iUpdate--;
}

float qtractorMeter::gain (void) const
{
	return gainFromScale(gainScale());
}

float qtractorMeter::gainScale (void) const
{
	return float(10000 - m_pGainSlider->value()) / 10000.0f;
}


// Common slider/spin-box accessors.
qtractorSlider *qtractorMeter::panSlider (void) const
{
	return m_pPanSlider;
}

qtractorSpinBox *qtractorMeter::panSpinBox (void) const
{
	return m_pPanSpinBox;
}

qtractorSlider *qtractorMeter::gainSlider (void) const
{
	return m_pGainSlider;
}

qtractorSpinBox *qtractorMeter::gainSpinBox (void) const
{
	return m_pGainSpinBox;
}


// Peak falloff mode setting.
void qtractorMeter::setPeakFalloff ( int iPeakFalloff )
{
	m_iPeakFalloff = iPeakFalloff;
}

int qtractorMeter::peakFalloff (void) const
{
	return m_iPeakFalloff;
}


// Panning-meter slider value change slot.
void qtractorMeter::panSliderChangedSlot ( int iValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fPanning = 0.01f * float(iValue);
	if (::fabsf(m_pPanSpinBox->valueFloat() - fPanning) > 0.01f) {
		m_pPanSpinBox->setValueFloat(fPanning);
		emit panChangedSignal(fPanning);
	}

	m_iUpdate--;
}


// Panning-meter spin-box value change slot.
void qtractorMeter::panSpinBoxChangedSlot ( const QString& sValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fPanning = sValue.toFloat();
	if (::fabsf(panning() - fPanning) > 0.01f) {
		m_pPanSlider->setValue(int(100.0f * fPanning));
		emit panChangedSignal(fPanning);
	}

	m_iUpdate--;
}


// Gain-meter slider value change slot.
void qtractorMeter::gainSliderChangedSlot ( int iValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fGain = gainFromScale(float(10000 - iValue) / 10000.0f);
	if (::fabsf(gainFromValue(m_pGainSpinBox->valueFloat()) - fGain) > 0.01f) {
		m_pGainSpinBox->setValueFloat(valueFromGain(fGain));
		emit gainChangedSignal(fGain);
	}

	m_iUpdate--;
}


// Gain-meter spin-box value change slot.
void qtractorMeter::gainSpinBoxChangedSlot ( const QString& sValue )
{
	if (m_iUpdate > 0)
		return;
		
	m_iUpdate++;

	float fGain = gainFromValue(sValue.toFloat());
	if (::fabsf(gain() - fGain) > 0.001f) {
		m_pGainSlider->setValue(10000 - int(10000.0f * scaleFromGain(fGain)));
		emit gainChangedSignal(fGain);
	}

	m_iUpdate--;
}


// end of qtractorMeter.cpp
