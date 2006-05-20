// qtractorMixer.cpp
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
#include "qtractorMixer.h"
#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"
#include "qtractorAudioMonitor.h"
#include "qtractorMidiMonitor.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTrack.h"
#include "qtractorTracks.h"
#include "qtractorTrackButton.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMainForm.h"

#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qtooltip.h>
#include <qpopupmenu.h>

// Some useful MIDI magig numbers.
#define MIDI_CC_VOLUME	7
#define MIDI_CC_PAN		10


//----------------------------------------------------------------------------
// qtractorMixerStrip -- Mixer strip widget.

// Constructors.
qtractorMixerStrip::qtractorMixerStrip ( qtractorMixerRack *pRack,
	qtractorBus *pBus, qtractorBus::BusMode busMode )
	: QFrame(pRack->workspace()), m_pRack(pRack),
		m_pBus(pBus), m_busMode(busMode), m_pTrack(NULL)
{
	initMixerStrip();
}

qtractorMixerStrip::qtractorMixerStrip ( qtractorMixerRack *pRack,
	qtractorTrack * pTrack)
	: QFrame(pRack->workspace()), m_pRack(pRack),
		m_pBus(NULL), m_busMode(qtractorBus::None),	m_pTrack(pTrack)
{
	initMixerStrip();
}


// Default destructor.
qtractorMixerStrip::~qtractorMixerStrip (void)
{
	// No need to delete child widgets, Qt does it all for us
}


// Common mixer-strip initializer.
void qtractorMixerStrip::initMixerStrip (void)
{
	m_iMark = 0;

	QFont font6(QFrame::font().family(), 6);
	QFontMetrics fm(font6);

	m_pLayout = new QVBoxLayout(this, 4, 4);

	m_pLabel = new QLabel(this);
	m_pLabel->setFont(font6);
	m_pLabel->setFixedHeight(fm.lineSpacing());

	m_pLayout->addWidget(m_pLabel);

	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		m_pBusButton = NULL;
		m_pButtonLayout = new QHBoxLayout(m_pLayout, 2);
		const QSize buttonSize(16, 14);
		m_pRecordButton = new qtractorTrackButton(m_pTrack,
			qtractorTrackButton::Record, buttonSize, this);
		m_pMuteButton   = new qtractorTrackButton(m_pTrack,
			qtractorTrackButton::Mute, buttonSize, this);
		m_pSoloButton   = new qtractorTrackButton(m_pTrack,
			qtractorTrackButton::Solo, buttonSize, this);
		m_pButtonLayout->addWidget(m_pRecordButton);
		m_pButtonLayout->addWidget(m_pMuteButton);
		m_pButtonLayout->addWidget(m_pSoloButton);
	//	m_pLayout->addLayout(m_pButtonLayout);
		qtractorTracks *pTracks = m_pRack->mixer()->mainForm()->tracks();
		QObject::connect(m_pRecordButton, SIGNAL(trackChanged(qtractorTrack *)),
			pTracks, SLOT(trackChangedSlot(qtractorTrack *)));
		QObject::connect(m_pMuteButton, SIGNAL(trackChanged(qtractorTrack *)),
			pTracks, SLOT(trackChangedSlot(qtractorTrack *)));
		QObject::connect(m_pSoloButton, SIGNAL(trackChanged(qtractorTrack *)),
			pTracks, SLOT(trackChangedSlot(qtractorTrack *)));
	} else {
		meterType = m_pBus->busType();
		const QString& sText = m_pRack->name().lower();
		m_pBusButton = new QToolButton(this);
		m_pBusButton->setFixedHeight(14);
		m_pBusButton->setFont(font6);
		m_pBusButton->setUsesTextLabel(true);
		m_pBusButton->setTextLabel(sText);
		QToolTip::add(m_pBusButton, tr("Connect %1").arg(sText));
		m_pLayout->addWidget(m_pBusButton);
		QObject::connect(m_pBusButton, SIGNAL(clicked()),
			this, SLOT(busButtonSlot()));
		m_pButtonLayout = NULL;
		m_pRecordButton = NULL;
		m_pMuteButton   = NULL;
		m_pSoloButton   = NULL;
	}

	// Now, there's whether we are Audio or MIDI related...
	m_pMeter = NULL;
	int iFixedWidth = 2 * 16;
	switch (meterType) {
	case qtractorTrack::Audio: {
		// Type cast for proper audio monitor...
		qtractorAudioMonitor *pAudioMonitor = NULL;
		if (m_pTrack) {
			pAudioMonitor
				= static_cast<qtractorAudioMonitor *> (m_pTrack->monitor());
		} else {
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (m_pBus);
			if (pAudioBus) {
				if (m_busMode & qtractorBus::Input)
					pAudioMonitor = pAudioBus->audioMonitor_in();
				else
					pAudioMonitor = pAudioBus->audioMonitor_out();
			}
		}
		// Have we an audio monitor/meter?...
		if (pAudioMonitor) {
			iFixedWidth += 16 * pAudioMonitor->channels();
			m_pMeter = new qtractorAudioMeter(pAudioMonitor, this);
		}
		break;
	}
	case qtractorTrack::Midi: {
		// Type cast for proper MIDI monitor...
		qtractorMidiMonitor *pMidiMonitor = NULL;
		if (m_pTrack) {
			pMidiMonitor
				= static_cast<qtractorMidiMonitor *> (m_pTrack->monitor());
		} else {
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (m_pBus);
			if (pMidiBus) {
				if (m_busMode & qtractorBus::Input)
					pMidiMonitor = pMidiBus->midiMonitor_in();
				else
					pMidiMonitor = pMidiBus->midiMonitor_out();
			}
		}
		// Have we a MIDI monitor/meter?...
		if (pMidiMonitor) {
			iFixedWidth += 16 * 2;
			m_pMeter = new qtractorMidiMeter(pMidiMonitor, this);
		}
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}

	// Eventually the right one...
	if (m_pMeter) {
		m_pLayout->addWidget(m_pMeter);
		QObject::connect(m_pMeter, SIGNAL(panChangedSignal()),
			this, SLOT(panChangedSlot()));
		QObject::connect(m_pMeter, SIGNAL(gainChangedSignal()),
			this, SLOT(gainChangedSlot()));
	}

	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setFixedWidth(iFixedWidth);	
//	QFrame::setSizePolicy(
//		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

	updateName();
	setSelected(false);
}


// Child properties accessors.
void qtractorMixerStrip::setMonitor ( qtractorMonitor *pMonitor )
{
	if (m_pMeter)
		m_pMeter->setMonitor(pMonitor);
}

qtractorMonitor *qtractorMixerStrip::monitor (void) const
{
	return (m_pMeter ? m_pMeter->monitor() : NULL);
}


// Common mixer-strip caption title updater.
void qtractorMixerStrip::updateName (void)
{
	QString sName;
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		sName = m_pTrack->trackName();
		m_pLabel->setPaletteBackgroundColor(m_pTrack->foreground().light());
		m_pLabel->setPaletteForegroundColor(m_pTrack->background().light());
		m_pLabel->setAlignment(Qt::AlignAuto);
	} else if (m_pBus) {
		meterType = m_pBus->busType();
		if (m_busMode & qtractorBus::Input) {
			sName = tr("%1 In").arg(m_pBus->busName());
		} else {
			sName = tr("%1 Out").arg(m_pBus->busName());
		}
		m_pLabel->setAlignment(Qt::AlignHCenter);
	}
	m_pLabel->setText(sName);

	QString sSuffix;
	switch (meterType) {
	case qtractorTrack::Audio:
		sSuffix = tr("(Audio)");
		break;
	case qtractorTrack::Midi:
		sSuffix = tr("(MIDI)");
		break;
	case qtractorTrack::None:
	default:
		sSuffix = tr("(None)");
		break;
	}

	QToolTip::remove(this);
	QToolTip::add(this, sName + ' ' + sSuffix);
}



// Child accessors.
qtractorMeter *qtractorMixerStrip::meter (void) const
{
	return m_pMeter;
}


// Bus property accessors.
void qtractorMixerStrip::setBus ( qtractorBus *pBus )
{
	// Must be actual bus...
	if (m_pBus == NULL || pBus == NULL)
		return;

	m_pBus = pBus;

	if (m_busMode & qtractorBus::Input) {
		setMonitor(m_pBus->monitor_in());
	} else {
		setMonitor(m_pBus->monitor_out());
	}

	updateName();
}

qtractorBus *qtractorMixerStrip::bus (void) const
{
	return m_pBus;
}


// Track property accessors.
void qtractorMixerStrip::setTrack ( qtractorTrack *pTrack )
{
	// Must be actual track...
	if (m_pTrack == NULL || pTrack == NULL)
		return;

	m_pTrack = pTrack;

	if (m_pTrack) {
		m_pRecordButton->setTrack(m_pTrack);
		m_pMuteButton->setTrack(m_pTrack);
		m_pSoloButton->setTrack(m_pTrack);
		setMonitor(m_pTrack->monitor());
	}

	updateName();
}

qtractorTrack *qtractorMixerStrip::track (void) const
{
	return m_pTrack;
}


// Selection methods.
void qtractorMixerStrip::setSelected ( bool bSelected )
{
	m_bSelected = bSelected;

	const QColorGroup& cg = QFrame::colorGroup();
	if (m_bSelected) {
		QFrame::setPaletteBackgroundColor(cg.color(QColorGroup::Midlight).dark(150));
		QFrame::setPaletteForegroundColor(cg.color(QColorGroup::Midlight).light(150));
	} else { 
		QFrame::setPaletteBackgroundColor(cg.button());
		QFrame::setPaletteForegroundColor(cg.text());
	}
}

bool qtractorMixerStrip::isSelected (void) const
{
	return m_bSelected;
}


// Update track buttons state.
void qtractorMixerStrip::updateTrackButtons (void)
{
	if (m_pRecordButton)
		m_pRecordButton->updateTrack();
	if (m_pMuteButton)
		m_pMuteButton->updateTrack();
	if (m_pSoloButton)
		m_pSoloButton->updateTrack();

	m_pRack->mixer()->mainForm()->tracks()->selectionChangeNotify();
}


// Strip refreshment.
void qtractorMixerStrip::refresh (void)
{
	if (m_pMeter)
		m_pMeter->refresh();
}


// Hacko-list-management marking...
void qtractorMixerStrip::setMark ( int iMark )
{
	m_iMark = iMark;
}

int qtractorMixerStrip::mark (void) const
{
	return m_iMark;
}

// Mouse selection event handlers.
void qtractorMixerStrip::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mousePressEvent(pMouseEvent);

	m_pRack->setSelectedStrip(this);
}


// Bus connection button slot
void qtractorMixerStrip::busButtonSlot (void)
{
	if (m_pBus == NULL)
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorMixerStrip::busButtonSlot() name=\"%s\"\n", m_pLabel->text().latin1());
#endif
}


// Pan-meter value change slot.
void qtractorMixerStrip::panChangedSlot (void)
{
	if (m_pMeter == NULL)
		return;

	// Make sure we're on a MIDI track mixer strip...
	if (m_pTrack == NULL)
		return;
	if (m_pTrack->trackType() != qtractorTrack::Midi)
		return;
	// Now we gotta make sure of proper MIDI bus...
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (m_pTrack->bus());
	if (pMidiBus == NULL)
		return;

	// Let it go...
	pMidiBus->setController(m_pTrack->midiChannel(),
		MIDI_CC_PAN, int(63.0f * (1.0f + m_pMeter->panning())) + 1);
}

// Gain-meter value change slot.
void qtractorMixerStrip::gainChangedSlot (void)
{
	if (m_pMeter == NULL)
		return;

	// Make sure we're on a MIDI mixer strip...
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack)
		meterType = m_pTrack->trackType();
	else if (m_pBus)
		meterType = m_pBus->busType();
	// Gotta be sure we're on MIDI only...
	if (meterType != qtractorTrack::Midi)
		return;

	// Now we gotta make sure of proper MIDI bus...
	qtractorMidiBus *pMidiBus = NULL;
	if (m_pTrack) {
		pMidiBus = static_cast<qtractorMidiBus *> (m_pTrack->bus());
		if (pMidiBus) {
			pMidiBus->setController(m_pTrack->midiChannel(),
				MIDI_CC_VOLUME, int(127.0f * m_pMeter->gain()));
		}
	} else {
		pMidiBus = static_cast<qtractorMidiBus *> (m_pBus);
		if (pMidiBus) {
			// Build some Universal SysEx and let it go...
		}
	}
}



//----------------------------------------------------------------------------
// qtractorMixerRack -- Meter bridge rack.

// Constructor.
qtractorMixerRack::qtractorMixerRack ( qtractorMixer *pMixer,
	const QString& sName, int iAlignment )
	: QWidget(pMixer->splitter()), m_pMixer(pMixer), m_sName(sName),
		m_bSelectEnabled(false), m_pSelectedStrip(NULL)
{
	m_strips.setAutoDelete(true);

	QWidget::setPaletteBackgroundColor(Qt::darkGray);
	
	m_pRackLayout = new QHBoxLayout(this, 0, 0);
	m_pStripSpacer
		= new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);

	if (iAlignment & Qt::AlignRight)
		m_pRackLayout->addItem(m_pStripSpacer);

	m_pStripHBox = new QHBox(this);
	m_pRackLayout->addWidget(m_pStripHBox);

	if (iAlignment & Qt::AlignLeft)
		m_pRackLayout->addItem(m_pStripSpacer);
}


// Default destructor.
qtractorMixerRack::~qtractorMixerRack (void)
{
	// No need to delete child widgets, Qt does it all for us
	clear();
}


// The main mixer widget accessor.
qtractorMixer *qtractorMixerRack::mixer (void) const
{
	return m_pMixer;
}


// Rack name accessors.
void qtractorMixerRack::setName ( const QString& sName )
{
	m_sName = sName;
}

const QString& qtractorMixerRack::name (void) const
{
	return m_sName;
}


// Add a mixer strip to rack list.
void qtractorMixerRack::addStrip ( qtractorMixerStrip *pStrip )
{
	m_strips.append(pStrip);

	pStrip->show();
}


// Remove a mixer strip from rack list.
void qtractorMixerRack::removeStrip ( qtractorMixerStrip *pStrip )
{
	// Don't let current selection hanging...
	if (m_pSelectedStrip == pStrip)
		m_pSelectedStrip = NULL;

	pStrip->hide();

	m_strips.remove(pStrip);
}


// Find a mixer strip, given its monitor handle.
qtractorMixerStrip *qtractorMixerRack::findStrip ( qtractorMonitor *pMonitor )
{
	for (qtractorMixerStrip *pStrip = m_strips.first();
			pStrip; pStrip = m_strips.next()) {
		if ((pStrip->meter())->monitor() == pMonitor)
			return pStrip;
	}
	
	return NULL;
}


// Current strip count.
int qtractorMixerRack::stripCount (void) const
{
	return m_strips.count();
}


// The strip workspace.
QHBox *qtractorMixerRack::workspace (void) const
{
	return m_pStripHBox;
}


// Complete rack refreshment.
void qtractorMixerRack::refresh (void)
{
	for (qtractorMixerStrip *pStrip = m_strips.first();
			pStrip; pStrip = m_strips.next()) {
		pStrip->refresh();
	}
}


// Complete rack recycle.
void qtractorMixerRack::clear (void)
{
	m_pSelectedStrip = NULL;
	m_strips.clear();
}


// Selection stuff.
void qtractorMixerRack::setSelectEnabled ( bool bSelectEnabled )
{
	m_bSelectEnabled = bSelectEnabled;

	if (!m_bSelectEnabled && m_pSelectedStrip) {
		m_pSelectedStrip->setSelected(false);
		m_pSelectedStrip = NULL;
	}
}

bool qtractorMixerRack::isSelectEnabled (void) const
{
	return m_bSelectEnabled;
}


void qtractorMixerRack::setSelectedStrip ( qtractorMixerStrip *pStrip )
{
	if (m_bSelectEnabled && m_pSelectedStrip != pStrip) {
		if (m_pSelectedStrip)
			m_pSelectedStrip->setSelected(false);
		m_pSelectedStrip = pStrip;
		if (m_pSelectedStrip) {
			m_pSelectedStrip->setSelected(true);
			emit selectionChanged();
		}
	}
}

qtractorMixerStrip *qtractorMixerRack::selectedStrip (void) const
{
	return m_pSelectedStrip;
}



// Hacko-list-management marking...
void qtractorMixerRack::markStrips ( int iMark )
{
	for (qtractorMixerStrip *pStrip = m_strips.first();
			pStrip; pStrip = m_strips.next()) {
		pStrip->setMark(iMark);
	}
}

void qtractorMixerRack::cleanStrips ( int iMark )
{
	for (qtractorMixerStrip *pStrip = m_strips.last();
			pStrip; pStrip = m_strips.prev()) {
		if (pStrip->mark() == iMark)
			removeStrip(pStrip);
	}
}


// Mouse selection event handlers.
void qtractorMixerRack::mouseDoubleClickEvent ( QMouseEvent * /*pMouseEvent*/ )
{
	if (m_bSelectEnabled)
		m_pMixer->mainForm()->trackProperties();
}


// Context menu request event handler.
void qtractorMixerRack::contextMenuEvent ( QContextMenuEvent *pContextMenuEvent )
{
	if (m_bSelectEnabled)
		m_pMixer->mainForm()->trackMenu->exec(pContextMenuEvent->globalPos());
}


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

// Constructor.
qtractorMixer::qtractorMixer ( qtractorMainForm *pMainForm )
	: QDockWindow(pMainForm, "qtractorMixer"), m_pMainForm(pMainForm)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	// QDockWindow::setName("qtractorMixer");

	m_pSplitter = new QSplitter(Qt::Horizontal, this, "Mixer");
	m_pSplitter->setChildrenCollapsible(false);

	m_pInputRack  = new qtractorMixerRack(this, tr("Inputs"));
	m_pTrackRack  = new qtractorMixerRack(this, tr("Tracks"));
	m_pTrackRack->setSelectEnabled(true);
	m_pOutputRack = new qtractorMixerRack(this, tr("Outputs"), Qt::AlignRight);
	
	// Prepare the dockable window stuff.
	QDockWindow::setWidget(m_pSplitter);
	QDockWindow::setOrientation(Qt::Horizontal);
	QDockWindow::setCloseMode(QDockWindow::Always);
	QDockWindow::setResizeEnabled(true);
	// Some specialties to this kind of dock window...
	QDockWindow::setFixedExtentHeight(120);
	QDockWindow::setFixedExtentWidth(240);

	// Finally set the default caption and tooltip.
	QString sCaption = tr("Mixer");
	QToolTip::add(this, sCaption);
	QDockWindow::setCaption(sCaption);
	QDockWindow::setIcon(QPixmap::fromMimeSource("qtractorTracks.png"));

	// Get previously saved splitter sizes,
	// (with afair default...)
	QValueList<int> sizes;
	sizes.append(0);
	sizes.append(120);
	sizes.append(0);
	m_pMainForm->options()->loadSplitterSizes(m_pSplitter, sizes);
}


// Default destructor.
qtractorMixer::~qtractorMixer (void)
{
	// Get previously saved splitter sizes...
	m_pMainForm->options()->saveSplitterSizes(m_pSplitter);

	// No need to delete child widgets, Qt does it all for us
}


// Main application form accessors.
qtractorMainForm *qtractorMixer::mainForm (void) const
{
	return m_pMainForm;
}


// Session accessor.
qtractorSession *qtractorMixer::session (void) const
{
	return m_pMainForm->session();
}


// The splitter layout widget accessor.
QSplitter *qtractorMixer::splitter (void) const
{
	return m_pSplitter;
}


// The mixer strips rack accessors.
qtractorMixerRack *qtractorMixer::inputRack (void) const
{
	return m_pInputRack;
}

qtractorMixerRack *qtractorMixer::trackRack (void) const
{
	return m_pTrackRack;
}

qtractorMixerRack *qtractorMixer::outputRack (void) const
{
	return m_pOutputRack;
}


// Update mixer rack, checking if given monitor already exists.
void qtractorMixer::updateBusStrip ( qtractorMixerRack *pRack,
	qtractorBus *pBus, qtractorBus::BusMode busMode )
{
	qtractorMonitor *pMonitor
		= (busMode == qtractorBus::Input ?
			pBus->monitor_in() : pBus->monitor_out());

	qtractorMixerStrip *pStrip = pRack->findStrip(pMonitor);
	if (pStrip == NULL) {
		pRack->addStrip(new qtractorMixerStrip(pRack, pBus, busMode));
	} else {
		pStrip->setMark(0);
	}
}


void qtractorMixer::updateTrackStrip ( qtractorTrack *pTrack )
{
	qtractorMixerStrip *pStrip = m_pTrackRack->findStrip(pTrack->monitor());
	if (pStrip == NULL) {
		m_pTrackRack->addStrip(new qtractorMixerStrip(m_pTrackRack, pTrack));
	} else {
		pStrip->setMark(0);
	}
}


// Update busses'racks.
void qtractorMixer::updateBusses (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	m_pInputRack->markStrips(1);
	m_pOutputRack->markStrips(1);

	// Audio busses first...
	for (qtractorBus *pBus = pSession->audioEngine()->busses().first();
			pBus; pBus = pBus->next()) {
		if (pBus->busMode() & qtractorBus::Input) {
			updateBusStrip(m_pInputRack, pBus, qtractorBus::Input);
		}
		if (pBus->busMode() & qtractorBus::Output) {
			updateBusStrip(m_pOutputRack, pBus, qtractorBus::Output);
		}
	}

	// MIDI busses are next...
	for (qtractorBus *pBus = pSession->midiEngine()->busses().first();
			pBus; pBus = pBus->next()) {
		if (pBus->busMode() & qtractorBus::Input) {
			updateBusStrip(m_pInputRack, pBus, qtractorBus::Input);
		}
		if (pBus->busMode() & qtractorBus::Output) {
			updateBusStrip(m_pOutputRack, pBus, qtractorBus::Output);
		}
	}

	m_pOutputRack->cleanStrips(1);
	m_pInputRack->cleanStrips(1);
}


// Update tracks'rack.
void qtractorMixer::updateTracks (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	m_pTrackRack->markStrips(1);

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		updateTrackStrip(pTrack);
	}

	m_pTrackRack->cleanStrips(1);
}


// Complete mixer refreshment.
void qtractorMixer::refresh (void)
{
	m_pInputRack->refresh();
	m_pTrackRack->refresh();
	m_pOutputRack->refresh();
}


// Complete mixer recycle.
void qtractorMixer::clear (void)
{
	m_pInputRack->clear();
	m_pTrackRack->clear();
	m_pOutputRack->clear();
}


// Track button notification.
void qtractorMixer::trackChangedSlot ( qtractorTrack *pTrack )
{
	qtractorMixerStrip *pStrip = m_pTrackRack->findStrip(pTrack->monitor());
	if (pStrip)
		pStrip->updateTrackButtons();
}


// end of qtractorMixer.cpp
