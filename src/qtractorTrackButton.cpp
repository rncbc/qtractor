// qtractorTrackButton.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiControlObserverForm.h"

#if QT_VERSION < 0x040300
#define lighter(x)	light(x)
#define darker(x)	dark(x)
#endif


//----------------------------------------------------------------------------
// qtractorMidiControlButton -- MIDI controller observer tool button.

// Constructor.
qtractorMidiControlButton::qtractorMidiControlButton ( QWidget *pParent )
	: qtractorObserverWidget<QPushButton> (pParent), m_pMidiControlAction(NULL)

{
	QPushButton::setFocusPolicy(Qt::NoFocus);
//	QPushButton::setToolButtonStyle(Qt::ToolButtonTextOnly);
	QPushButton::setCheckable(true);
}


// MIDI controller/observer attachment (context menu) activator.
//
Q_DECLARE_METATYPE(qtractorMidiControlObserver *);

void qtractorMidiControlButton::addMidiControlAction (
	qtractorMidiControlObserver *pMidiObserver )
{
	if (m_pMidiControlAction)
		QPushButton::removeAction(m_pMidiControlAction);
		
	m_pMidiControlAction = new QAction(
		QIcon(":/images/itemControllers.png"),
		tr("MIDI Controller..."), this);

	m_pMidiControlAction->setData(
		qVariantFromValue<qtractorMidiControlObserver *> (pMidiObserver));

	QObject::connect(m_pMidiControlAction,
		SIGNAL(triggered(bool)),
		SLOT(midiControlActionSlot()));

	QPushButton::addAction(m_pMidiControlAction);
	QPushButton::setContextMenuPolicy(Qt::ActionsContextMenu);
}


void qtractorMidiControlButton::midiControlActionSlot (void)
{
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction && pAction == m_pMidiControlAction) {
		qtractorMidiControlObserver *pMidiObserver
			= qVariantValue<qtractorMidiControlObserver *> (pAction->data());
		if (pMidiObserver) {
			qtractorMidiControlObserverForm form(parentWidget());
			form.setMidiObserver(pMidiObserver);
			form.exec();
		}
	}
}


//----------------------------------------------------------------------------
// qtractorTrackButton -- Track tool button (observer).

// Constructor.
qtractorTrackButton::qtractorTrackButton ( qtractorTrack *pTrack,
	qtractorTrack::ToolType toolType, const QSize& fixedSize,
	QWidget *pParent ) : qtractorMidiControlButton(pParent)
{
	m_pTrack   = pTrack;
	m_toolType = toolType;
	m_iUpdate  = 0;

	QPushButton::setFixedSize(fixedSize);
	QPushButton::setFont(
		QFont(QPushButton::font().family(), (fixedSize.height() < 16 ? 5 : 6)));
	
	QPalette pal(QPushButton::palette());
	m_rgbText = pal.buttonText().color();
	m_rgbOff  = pal.button().color();
	switch (m_toolType) {
	case qtractorTrack::Record:
		QPushButton::setText("R");
		QPushButton::setToolTip(tr("Record"));
		m_rgbOn = Qt::red;
		break;
	case qtractorTrack::Mute:
		QPushButton::setText("M");
		QPushButton::setToolTip(tr("Mute"));
		m_rgbOn = Qt::yellow;
		break;
	case qtractorTrack::Solo:
		QPushButton::setText("S");
		QPushButton::setToolTip(tr("Solo"));
		m_rgbOn = Qt::cyan;
		break;
	}

	updateTrack(); // Visitor setup.

	QObject::connect(this, SIGNAL(toggled(bool)), SLOT(toggledSlot(bool)));
}


// Visitors overload.
void qtractorTrackButton::updateValue ( float fValue )
{
	m_iUpdate++;

	bool bOn = (fValue > 0.0f);

	QPalette pal(QPushButton::palette());
	pal.setColor(QPalette::ButtonText, bOn ? m_rgbOn.darker() : m_rgbText);
	pal.setColor(QPalette::Button, bOn ? m_rgbOn : m_rgbOff);
	QPushButton::setPalette(pal);

	QPushButton::setChecked(bOn);

	m_iUpdate--;
}


// Special toggle slot.
void qtractorTrackButton::toggledSlot ( bool bOn )
{
	// Avoid self-triggering...
	if (m_iUpdate > 0)
		return;

	// Just emit proper signal...
	m_pTrack->stateChangeNotify(m_toolType, bOn);
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


// Track state (record, mute, solo) button setup.
void qtractorTrackButton::updateTrack (void)
{
	switch (m_toolType) {
	case qtractorTrack::Record:
		setSubject(m_pTrack->recordSubject());
		addMidiControlAction(m_pTrack->recordObserver());
		break;
	case qtractorTrack::Mute:
		setSubject(m_pTrack->muteSubject());
		addMidiControlAction(m_pTrack->muteObserver());
		break;
	case qtractorTrack::Solo:
		setSubject(m_pTrack->soloSubject());
		addMidiControlAction(m_pTrack->soloObserver());
		break;
	}

	observer()->update();
}


// end of qtractorTrackButton.cpp
