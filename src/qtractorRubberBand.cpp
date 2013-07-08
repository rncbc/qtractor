// qtractorRubberBand.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorRubberBand.h"

#include <QStyleHintReturnMask>


//----------------------------------------------------------------------------
// qtractorRubberBand -- Custom rubber-band widget.

// Constructor.
qtractorRubberBand::qtractorRubberBand ( Shape shape, QWidget *widget, int thick )
	: QRubberBand(shape, widget)
{
	m_pStyle = new qtractorRubberBand::Style(thick);

	setStyle(m_pStyle);
}

// Destructor.
qtractorRubberBand::~qtractorRubberBand (void)
{
	delete m_pStyle;
}


// Rubberband thickness accessor.
void qtractorRubberBand::setThickness ( int thick )
{
	m_pStyle->thickness = thick;
}

int qtractorRubberBand::thickness (void) const
{
	return m_pStyle->thickness;
}


// Custom virtual override.
int qtractorRubberBand::Style::styleHint ( StyleHint sh,
	const QStyleOption *opt, const QWidget *widget,
	QStyleHintReturn *hint ) const
{
	int ret = QCommonStyle::styleHint(sh, opt, widget, hint);
	if (sh == QStyle::SH_RubberBand_Mask) {
		QStyleHintReturnMask *mask
			= qstyleoption_cast<QStyleHintReturnMask *> (hint);
		QRect rect(mask->region.boundingRect());
		QRegion regn(rect);
		rect.setX(rect.x() + thickness);
		rect.setY(rect.y() + thickness);
		rect.setWidth(rect.width() - thickness);
		rect.setHeight(rect.height() - thickness);
		mask->region = regn.subtracted(QRegion(rect));
	}
	return ret;
}


// end of qtractorRubberBand.cpp
