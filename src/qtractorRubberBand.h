// qtractorRubberBand.h
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

#ifndef __qtractorRubberBand_h
#define __qtractorRubberBand_h

#include <QRubberBand>
#if QT_VERSION < 0x050000
#include <QWindowsStyle>
#else
#include <QCommonStyle>
class QWindowsStyle : public QCommonStyle {};
#endif


//----------------------------------------------------------------------------
// qtractorRubberBand -- Custom rubber-band widget.

class qtractorRubberBand : public QRubberBand
{
public:

	// Constructor.
	qtractorRubberBand(Shape shape, QWidget *widget = 0, int thick = 1);
	// Destructor.
	~qtractorRubberBand();

	// Rubberband thickness accessor.
	void setThickness(int thick);
	int thickness() const;

private:

	// qtractorRubberBandStyle -- Custom rubber-band style.
	class Style : public QWindowsStyle
	{
	public:

		// Constructor.
		Style(int thick) : QWindowsStyle(), thickness(thick) {}

		// Custom virtual override.
		int styleHint( StyleHint sh,
			const QStyleOption *opt = 0, const QWidget *widget = 0,
			QStyleHintReturn *hint = 0 ) const;

		// Rubberband thickness.
		int thickness;
	};

	// Local style instance
	Style *m_pStyle;
};


#endif  // __qtractorRubberBand_h


// end of qtractorRubberBand.h
