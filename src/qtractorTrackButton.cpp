// qtractorTrackButton.cpp
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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorTrackButton.h"
#include "qtractorTrack.h"

#include <qtooltip.h>


//----------------------------------------------------------------------------
// qtractorTrackButton -- Track tool button.

// Constructor.
qtractorTrackButton::qtractorTrackButton ( qtractorTrack *pTrack,
	ToolType toolType, const QSize& fixedSize,
	QWidget *pParent, const char *pszName )
	: QToolButton(pParent, pszName)
{
	m_pTrack   = pTrack;
	m_toolType = toolType;

	QToolButton::setFixedSize(fixedSize);
	QToolButton::setUsesTextLabel(true);
	QToolButton::setToggleButton(true);

	QToolButton::setFont(
		QFont(QToolButton::font().family(), (fixedSize.height() < 16 ? 5 : 6)));

	m_rgbOff = QToolButton::paletteBackgroundColor();
	switch (toolType) {
	case Record:
		QToolButton::setTextLabel("R");
		QToolTip::add(this, tr("Record"));
		m_rgbOn = Qt::red;
		break;
	case Mute:
		QToolButton::setTextLabel("M");
		QToolTip::add(this, tr("Mute"));
		m_rgbOn = Qt::yellow;
		break;
	case Solo:
		QToolButton::setTextLabel("S");
		QToolTip::add(this, tr("Solo"));
		m_rgbOn = Qt::cyan;
		break;
	}

	QObject::connect(this, SIGNAL(toggled(bool)), SLOT(toggledSlot(bool)));

	updateTrack();
}


// Special state slot.
void qtractorTrackButton::updateTrack (void)
{
	bool bOn = false;

	switch (m_toolType) {
	case Record:
		bOn = (m_pTrack && m_pTrack->isRecord());
		break;
	case Mute:
		bOn = (m_pTrack && m_pTrack->isMute());
		break;
	case Solo:
		bOn = (m_pTrack && m_pTrack->isSolo());
		break;
	}

	QToolButton::setOn(bOn);
	QToolButton::setPaletteBackgroundColor(bOn ? m_rgbOn : m_rgbOff);
}


// Special toggle slot.
void qtractorTrackButton::toggledSlot ( bool bOn )
{
	if (m_pTrack == NULL)
		return;

	// Do the proper tool action immediately...
	switch (m_toolType) {
	case Record:
		m_pTrack->setRecord(bOn);
		break;
	case Mute:
		m_pTrack->setMute(bOn);
		break;
	case Solo:
		m_pTrack->setSolo(bOn);
		break;
	}

	// Do some feedback...
	QToolButton::setPaletteBackgroundColor(bOn ? m_rgbOn : m_rgbOff);

	emit trackChanged(m_pTrack);
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

qtractorTrackButton::ToolType qtractorTrackButton::toolType (void) const
{
	return m_toolType;
}


// end of qtractorTrackButton.cpp
