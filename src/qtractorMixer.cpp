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

#include "qtractorMixer.h"
#include "qtractorMeter.h"
#include "qtractorMonitor.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTrack.h"
#include "qtractorTracks.h"
#include "qtractorTrackButton.h"
#include "qtractorAudioEngine.h"

#include "qtractorMainForm.h"

#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qtooltip.h>
#include <qpopupmenu.h>


//----------------------------------------------------------------------------
// qtractorMixerStrip -- Mixer strip widget.

// Constructors.
qtractorMixerStrip::qtractorMixerStrip ( qtractorMixerRack *pRack,
	qtractorMonitor *pMonitor, const QString& sName )
	: QFrame(pRack->workspace()),
		m_pRack(pRack), m_pTrack(NULL)
{
	initMixerStrip(pMonitor, sName);
}

qtractorMixerStrip::qtractorMixerStrip ( qtractorMixerRack *pRack,
	qtractorTrack * pTrack)
	: QFrame(pRack->workspace()),
		m_pRack(pRack), m_pTrack(pTrack)
{
	initMixerStrip(pTrack->monitor(), pTrack->trackName());
}


// Default destructor.
qtractorMixerStrip::~qtractorMixerStrip (void)
{
	// No need to delete child widgets, Qt does it all for us
}

// Common mixer-strip initializer.
void qtractorMixerStrip::initMixerStrip ( qtractorMonitor *pMonitor,
	const QString& sName )
{
	m_iMark = 0;

	int iWidth = 16 * (2 + pMonitor->channels());
	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setFixedWidth(iWidth);	
//	QFrame::setSizePolicy(
//		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

	m_pLayout = new QVBoxLayout(this, 4, 4);

	QFont font6(QFrame::font().family(), 6);
	QFontMetrics fm(font6);

	m_pLabel = new QLabel(this);
	m_pLabel->setFont(font6);
	m_pLabel->setFixedHeight(fm.lineSpacing());
	if (fm.width(sName) < iWidth)
		m_pLabel->setAlignment(Qt::AlignHCenter);
		
	m_pLayout->addWidget(m_pLabel);
	QToolTip::add(m_pLabel, sName);

	if (m_pTrack) {
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
		m_pButtonLayout = NULL;
		m_pRecordButton = NULL;
		m_pMuteButton   = NULL;
		m_pSoloButton   = NULL;
	}

	m_pMeter = new qtractorMeter(pMonitor, this);
	m_pLayout->addWidget(m_pMeter);

	if (m_pTrack) {
		m_pLabel->setPaletteBackgroundColor(m_pTrack->foreground().light());
		m_pLabel->setPaletteForegroundColor(m_pTrack->background().light());
	}

	setName(sName);
	setSelected(false);
}


// Child properties accessors.
void qtractorMixerStrip::setMonitor ( qtractorMonitor *pMonitor )
{
	m_pMeter->setMonitor(pMonitor);
}

qtractorMonitor *qtractorMixerStrip::monitor (void) const
{
	return m_pMeter->monitor();
}


void qtractorMixerStrip::setName ( const QString& sName )
{
	m_pLabel->setText(sName);

	QToolTip::remove(m_pLabel);
	QToolTip::add(m_pLabel, sName);
}

QString qtractorMixerStrip::name (void) const
{
	return m_pLabel->text();
}


// Child accessors.
qtractorMeter *qtractorMixerStrip::meter (void) const
{
	return m_pMeter;
}


// Special properties accessors.
void qtractorMixerStrip::setTrack ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;

	if (m_pTrack) {
		setName(m_pTrack->trackName());
		m_pLabel->setPaletteBackgroundColor(m_pTrack->foreground().light());
		m_pLabel->setPaletteForegroundColor(m_pTrack->background().light());
		m_pRecordButton->setTrack(m_pTrack);
		m_pMuteButton->setTrack(m_pTrack);
		m_pSoloButton->setTrack(m_pTrack);
		m_pMeter->setMonitor(m_pTrack->monitor());
	}
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
}


// Strip refreshment.
void qtractorMixerStrip::refresh (void)
{
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


//----------------------------------------------------------------------------
// qtractorMixerRack -- Meter bridge rack.

// Constructor.
qtractorMixerRack::qtractorMixerRack ( qtractorMixer *pMixer, int iAlignment )
	: QWidget(pMixer->splitter()), m_pMixer(pMixer),
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
void qtractorMixerRack::mouseDoubleClickEvent ( QMouseEvent *pMouseEvent )
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

	m_pInputRack  = new qtractorMixerRack(this);
	m_pTrackRack  = new qtractorMixerRack(this);
	m_pTrackRack->setSelectEnabled(true);
	m_pOutputRack = new qtractorMixerRack(this, Qt::AlignRight);
	
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
	qtractorMonitor *pMonitor, const QString& sName )
{
	qtractorMixerStrip *pStrip = pRack->findStrip(pMonitor);
	if (pStrip == NULL) {
		pRack->addStrip(new qtractorMixerStrip(pRack, pMonitor, sName));
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

	// FIXME: Only audio busses, for the time being...
	for (qtractorBus *pBus = pSession->audioEngine()->busses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			if (pAudioBus->busMode() & qtractorBus::Input) {
				updateBusStrip(m_pInputRack,
					pAudioBus->monitor_in(), pAudioBus->busName() + tr(" In "));
			}
			if (pAudioBus->busMode() & qtractorBus::Output) {
				updateBusStrip(m_pOutputRack,
					pAudioBus->monitor_out(), pAudioBus->busName() + tr(" Out "));
			}
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

	// FIXME: Only audio tracks, in the time being...
	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		if (pTrack->trackType() == qtractorTrack::Audio)
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
