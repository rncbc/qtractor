// qtractorTrackButton.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorTrackButton.h"

#if QT_VERSION < 0x040300
#define lighter(x)	light(x)
#define darker(x)	dark(x)
#endif


//----------------------------------------------------------------------------
// qtractorTrackButton -- Track tool button.

// Constructor.
qtractorTrackButton::qtractorTrackButton ( qtractorTrack *pTrack,
	qtractorTrack::ToolType toolType, const QSize& fixedSize,
	QWidget *pParent ) : QToolButton(pParent)
{
	m_pTrack   = pTrack;
	m_toolType = toolType;
	m_iUpdate  = 0;

	QToolButton::setFixedSize(fixedSize);
	QToolButton::setToolButtonStyle(Qt::ToolButtonTextOnly);
	QToolButton::setCheckable(true);

	QToolButton::setFont(
		QFont(QToolButton::font().family(), (fixedSize.height() < 16 ? 5 : 6)));

	QPalette pal(QToolButton::palette());
	m_rgbText = pal.buttonText().color();
	m_rgbOff  = pal.button().color();
	switch (toolType) {
	case qtractorTrack::Record:
		QToolButton::setText("R");
		QToolButton::setToolTip(tr("Record"));
		m_rgbOn = Qt::red;
		break;
	case qtractorTrack::Mute:
		QToolButton::setText("M");
		QToolButton::setToolTip(tr("Mute"));
		m_rgbOn = Qt::yellow;
		break;
	case qtractorTrack::Solo:
		QToolButton::setText("S");
		QToolButton::setToolTip(tr("Solo"));
		m_rgbOn = Qt::cyan;
		break;
	}

	QObject::connect(this, SIGNAL(toggled(bool)), SLOT(toggledSlot(bool)));

	updateTrack();
}


// Special state slot.
void qtractorTrackButton::updateTrack (void)
{
	m_iUpdate++;

	bool bOn = false;

	switch (m_toolType) {
	case qtractorTrack::Record:
		bOn = (m_pTrack && m_pTrack->isRecord());
		break;
	case qtractorTrack::Mute:
		bOn = (m_pTrack && m_pTrack->isMute());
		break;
	case qtractorTrack::Solo:
		bOn = (m_pTrack && m_pTrack->isSolo());
		break;
	}

	QPalette pal(QToolButton::palette());
	pal.setColor(QPalette::ButtonText, bOn ? m_rgbOn.darker() : m_rgbText);
	pal.setColor(QPalette::Button, bOn ? m_rgbOn : m_rgbOff);
	QToolButton::setPalette(pal);

	QToolButton::setChecked(bOn);

	m_iUpdate--;
}


// Special toggle slot.
void qtractorTrackButton::toggledSlot ( bool bOn )
{
	// Avoid self-triggering...
	if (m_iUpdate > 0)
		return;

	// Just emit proper signal...
	emit trackButtonToggled(this, bOn);
}


// Specific accessors.
void qtractorTrackButton::setTrack ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;
	updateTrack();
}

qtractorTrack *qtractorTrackButton::track (void) const
{
	return m_pTrack;
}

qtractorTrack::ToolType qtractorTrackButton::toolType (void) const
{
	return m_toolType;
}


// end of qtractorTrackButton.cpp
