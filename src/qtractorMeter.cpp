// qtractorMeter.cpp
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

#include "qtractorMeter.h"
#include "qtractorSlider.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QLabel>

#include <QPaintEvent>
#include <QResizeEvent>

#include <math.h>


//----------------------------------------------------------------------------
// qtractorMeterScale -- Meter bridge scale widget.

// Constructor.
qtractorMeterScale::qtractorMeterScale ( qtractorMeter *pMeter,
	QWidget *pParent ) : QWidget(pParent)
{
	m_pMeter = pMeter;
	m_iLastY = 0;

	QWidget::setMinimumWidth(16);
//	QWidget::setBackgroundRole(QPalette::Mid);

	const QFont& font = QWidget::font();
	QWidget::setFont(QFont(font.family(), font.pointSize() - 2));
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
	int y, const QString& sLabel )
{
	int iCurrY = QWidget::height() - y;
	int iWidth = QWidget::width();

	const QFontMetrics& fm = p->fontMetrics();
	int iMidHeight = (fm.height() >> 1);

	if (iCurrY < iMidHeight || iCurrY > (m_iLastY + iMidHeight)) {
		if (fm.width(sLabel) < iWidth - 5)
			p->drawLine(iWidth - 3, iCurrY, iWidth - 1, iCurrY);
		p->drawText(0, iCurrY - iMidHeight, iWidth - 3, fm.height(),
			Qt::AlignHCenter | Qt::AlignVCenter, sLabel);
		m_iLastY = iCurrY + 1;
	}
}


// Paint event handler.
void qtractorMeterScale::paintEvent ( QPaintEvent * )
{
	QPainter painter(this);

	m_iLastY = 0;

#ifndef CONFIG_GRADIENT
	painter.setPen(Qt::darkGray);
#endif

	paintScale(&painter);
}


//----------------------------------------------------------------------------
// qtractorMeter -- Meter bridge slot widget.

// Constructor.
qtractorMeter::qtractorMeter ( QWidget *pParent )
	: QWidget(pParent)
{
	const QFont& font = QWidget::font();
	QFont font7(font.family(), font.pointSize() - 1);
	QFontMetrics fm(font7);

	m_pVBoxLayout = new QVBoxLayout();
	m_pVBoxLayout->setMargin(0);
	m_pVBoxLayout->setSpacing(2);
	QWidget::setLayout(m_pVBoxLayout);

	m_pPanSlider = new qtractorSlider(Qt::Horizontal/*, this*/);
	m_pPanSlider->setFixedHeight(20);
	m_pVBoxLayout->addWidget(m_pPanSlider);

	m_pPanSpinBox = new QDoubleSpinBox(/*this*/);
	m_pPanSpinBox->setFont(font7);
	m_pPanSpinBox->setFixedHeight(fm.lineSpacing() + 2);
	m_pVBoxLayout->addWidget(m_pPanSpinBox);

	m_pTopWidget = new QWidget(/*this*/);
	m_pTopLayout = new QHBoxLayout();
	m_pTopLayout->setMargin(0);
	m_pTopLayout->setSpacing(2);
	m_pTopWidget->setLayout(m_pTopLayout);
	m_pVBoxLayout->addWidget(m_pTopWidget);

	m_pBoxWidget = new QWidget(/*this*/);
	m_pBoxLayout = new QHBoxLayout();
	m_pBoxLayout->setMargin(2);
	m_pBoxLayout->setSpacing(2);
	m_pBoxWidget->setLayout(m_pBoxLayout);
	m_pVBoxLayout->addWidget(m_pBoxWidget);

	m_pGainSlider = new qtractorSlider(Qt::Vertical/*, m_pBoxWidget*/);
	m_pGainSlider->setFixedWidth(20);
	m_pBoxLayout->addWidget(m_pGainSlider);

	m_pGainSpinBox = new QDoubleSpinBox(/*this*/);
	m_pGainSpinBox->setFont(font7);
	m_pGainSpinBox->setFixedHeight(fm.lineSpacing() + 2);
	m_pVBoxLayout->addWidget(m_pGainSpinBox);

	m_iUpdate      = 0;
	m_iPeakFalloff = 0;

	m_pPanSlider->setTickPosition(QSlider::NoTicks);
	m_pPanSlider->setMinimum(-100);
	m_pPanSlider->setMaximum(+100);
	m_pPanSlider->setPageStep(10);
	m_pPanSlider->setSingleStep(1);
	m_pPanSlider->setDefault(0);

	m_pPanSpinBox->setDecimals(1);
	m_pPanSpinBox->setMinimum(-1.0f);
	m_pPanSpinBox->setMaximum(+1.0f);
	m_pPanSpinBox->setSingleStep(0.1f);
	m_pPanSpinBox->setAlignment(Qt::AlignHCenter);
	m_pPanSpinBox->setToolTip(tr("Pan"));

	m_pGainSlider->setTickPosition(QSlider::NoTicks);
	m_pGainSlider->setMinimum(0);
	m_pGainSlider->setMaximum(10000);
	m_pGainSlider->setPageStep(1000);
	m_pGainSlider->setSingleStep(100);
	m_pGainSlider->setDefault(10000);

	m_pGainSpinBox->setAlignment(Qt::AlignHCenter);
	m_pGainSpinBox->setDecimals(1);
	m_pGainSpinBox->setToolTip(tr("Gain"));

	QWidget::setMinimumHeight(140);
	QWidget::setSizePolicy(
		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

	QObject::connect(m_pPanSlider,
		SIGNAL(valueChanged(int)),
		SLOT(panSliderChangedSlot(int)));
	QObject::connect(m_pPanSpinBox,
		SIGNAL(valueChanged(const QString&)),
		SLOT(panSpinBoxChangedSlot(const QString&)));
	QObject::connect(m_pGainSlider,
		SIGNAL(valueChanged(int)),
		SLOT(gainSliderChangedSlot(int)));
	QObject::connect(m_pGainSpinBox,
		SIGNAL(valueChanged(const QString&)),
		SLOT(gainSpinBoxChangedSlot(const QString&)));
}


// Default destructor.
qtractorMeter::~qtractorMeter (void)
{
	// No need to delete child widgets, Qt does it all for us
}


// Dynamic layout accessors.
QWidget *qtractorMeter::topWidget (void) const
{
	return m_pTopWidget;
}

QHBoxLayout *qtractorMeter::topLayout (void) const
{
	return m_pTopLayout;
}


QWidget *qtractorMeter::boxWidget (void) const
{
	return m_pBoxWidget;
}

QHBoxLayout *qtractorMeter::boxLayout (void) const
{
	return m_pBoxLayout;
}


// Stereo panning accessors.
void qtractorMeter::setPanning ( float fPanning )
{
	m_iUpdate++;
	m_pPanSlider->setValue(::lroundf(100.0f * fPanning));
	m_pPanSpinBox->setValue(fPanning);
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
	m_pGainSlider->setValue(::lroundf(10000.0f * scaleFromGain(fGain)));
	m_pGainSpinBox->setValue(valueFromGain(fGain));
	m_iUpdate--;
}

float qtractorMeter::gain (void) const
{
	return gainFromScale(gainScale());
}

float qtractorMeter::gainScale (void) const
{
	return float(m_pGainSlider->value()) / 10000.0f;
}


// Common slider/spin-box accessors.
qtractorSlider *qtractorMeter::panSlider (void) const
{
	return m_pPanSlider;
}

QDoubleSpinBox *qtractorMeter::panSpinBox (void) const
{
	return m_pPanSpinBox;
}

qtractorSlider *qtractorMeter::gainSlider (void) const
{
	return m_pGainSlider;
}

QDoubleSpinBox *qtractorMeter::gainSpinBox (void) const
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
	m_pPanSpinBox->setValue(fPanning);
	emit panChangedSignal(fPanning);
	m_iUpdate--;
}


// Panning-meter spin-box value change slot.
void qtractorMeter::panSpinBoxChangedSlot ( const QString& sValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;
	float fPanning = sValue.toFloat();
	m_pPanSlider->setValue(::lroundf(100.0f * fPanning));
	emit panChangedSignal(fPanning);
	m_iUpdate--;
}


// Gain-meter slider value change slot.
void qtractorMeter::gainSliderChangedSlot ( int iValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;
	float fGain = gainFromScale(float(iValue) / 10000.0f);
	m_pGainSpinBox->setValue(valueFromGain(fGain));
	emit gainChangedSignal(fGain);
	m_iUpdate--;
}


// Gain-meter spin-box value change slot.
void qtractorMeter::gainSpinBoxChangedSlot ( const QString& sValue )
{
	if (m_iUpdate > 0)
		return;
		
	m_iUpdate++;
	float fGain = gainFromValue(sValue.toFloat());
	m_pGainSlider->setValue(::lroundf(10000.0f * scaleFromGain(fGain)));
	emit gainChangedSignal(fGain);
	m_iUpdate--;
}


// end of qtractorMeter.cpp
