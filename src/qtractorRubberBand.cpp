// qtractorRubberBand.cpp
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

#include "qtractorRubberBand.h"

#if QT_VERSION >= 0x040201
#include <QStyleHintReturnMask>
#endif


//----------------------------------------------------------------------------
// qtractorRubberBand -- Custom rubber-band widget.

// Local style instance
qtractorRubberBand::Style qtractorRubberBand::g_rubberBandStyle;

// Constructor.
qtractorRubberBand::qtractorRubberBand ( Shape shape, QWidget *widget )
	: QRubberBand(shape, widget)
{
	setStyle(&g_rubberBandStyle);
}


// Custom virtual override.
int qtractorRubberBand::Style::styleHint( StyleHint sh,
	const QStyleOption *opt, const QWidget *widget,
	QStyleHintReturn *hint ) const
{
	int ret = QWindowsStyle::styleHint(sh, opt, widget, hint);
#if QT_VERSION >= 0x040201
	if (sh == QStyle::SH_RubberBand_Mask) {
		QStyleHintReturnMask *mask
			= qstyleoption_cast<QStyleHintReturnMask *> (hint);
		QRect rect(mask->region.boundingRect());
		QRegion regn(rect);
		rect.setX(rect.x() + 3);
		rect.setY(rect.y() + 3);
		rect.setWidth(rect.width() - 3);
		rect.setHeight(rect.height() - 3);
		mask->region = regn.subtracted(QRegion(rect));
	}
#endif
	return ret;
}


// end of qtractorRubberBand.cpp
