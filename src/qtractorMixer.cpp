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

#include "qtractorSession.h"
#include "qtractorTrack.h"
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

// Constructor.
qtractorMixerStrip::qtractorMixerStrip ( qtractorMixerRack *pRack,
	qtractorMonitor *pMonitor, const QString& sName, const QColor& color )
	: QFrame(pRack->workspace()), m_pRack(pRack), m_iMark(0)
{
	const QColorGroup& cg = QFrame::colorGroup();
	QWidget::setPaletteBackgroundColor(cg.button());
	
	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Raised);

	m_pLayout = new QVBoxLayout(this, 4, 4);

	m_pLabel = new QLabel(sName, this);
	m_pLabel->setAlignment(Qt::AlignCenter);
	if (color == Qt::color0) {
		m_pLabel->setPaletteBackgroundColor(cg.button());
	} else {
		m_pLabel->setPaletteBackgroundColor(color.light());
		m_pLabel->setPaletteForegroundColor(color.dark());
	}
	m_pLayout->addWidget(m_pLabel);

	m_pMeter = new qtractorMeter(pMonitor, this);
	m_pLayout->addWidget(m_pMeter);

//	QFrame::setSizePolicy(
//		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));
}


// Default destructor.
qtractorMixerStrip::~qtractorMixerStrip (void)
{
	// No need to delete child widgets, Qt does it all for us
}


// Strip refreshment.
void qtractorMixerStrip::refresh (void)
{
	m_pMeter->refresh();
}


// Context menu request event handler.
void qtractorMixerStrip::contextMenuEvent ( QContextMenuEvent *pContextMenuEvent )
{
	m_pRack->contextMenu(pContextMenuEvent->globalPos(), this);
}


//----------------------------------------------------------------------------
// qtractorMixerRack -- Meter bridge rack.

// Constructor.
qtractorMixerRack::qtractorMixerRack ( QWidget *pParent, const char *pszName )
	: QWidget(pParent, pszName)
{
	m_strips.setAutoDelete(true);

	QWidget::setPaletteBackgroundColor(Qt::darkGray);
	
	m_pRackLayout = new QHBoxLayout(this, 0, 0);
	
	m_pStripHBox = new QHBox(this);
	m_pRackLayout->addWidget(m_pStripHBox);

	m_pStripSpacer
		= new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
	m_pRackLayout->addItem(m_pStripSpacer);
}

// Default destructor.
qtractorMixerRack::~qtractorMixerRack (void)
{
	// No need to delete child widgets, Qt does it all for us
	clear();
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
	m_strips.clear();
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
			delete pStrip;
	}
}


// Context menu request event handler.
void qtractorMixerRack::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	contextMenu(pContextMenuEvent->globalPos(), NULL);
}


// Context menu request event handler.
void qtractorMixerRack::contextMenu ( const QPoint& gpos,
	qtractorMixerStrip * /* pStrip */ )
{
	qtractorMixer *pMixer = static_cast<qtractorMixer *> (parentWidget());
	if (pMixer)
		pMixer->mainForm()->trackMenu->exec(gpos);
}


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

// Constructor.
qtractorMixer::qtractorMixer ( qtractorMainForm *pMainForm,
	QWidget *pParent, const char *pszName )
	: QDockWindow(pParent, pszName), m_pMainForm(pMainForm)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	if (pszName == 0)
		QDockWindow::setName("qtractorMixer");

	m_pSplitter = new QSplitter(Qt::Horizontal, this);

    m_pInputRack  = new qtractorMixerRack(m_pSplitter);
    m_pTrackRack  = new qtractorMixerRack(m_pSplitter);
    m_pOutputRack = new qtractorMixerRack(m_pSplitter);

#if QT_VERSION >= 0x030200
    m_pSplitter->setChildrenCollapsible(false);
#endif	

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
}                 


// Default destructor.
qtractorMixer::~qtractorMixer (void)
{
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


// Update mixer rack, checking if given monitor already exists.
void qtractorMixer::updateRackStrip ( qtractorMixerRack *pRack,
	qtractorMonitor *pMonitor, const QString& sName, const QColor& color )
{
	qtractorMixerStrip *pStrip = pRack->findStrip(pMonitor);
	if (pStrip) {
		pStrip->setMark(0);
	} else {
		pRack->addStrip(new qtractorMixerStrip(pRack, pMonitor, sName, color));
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
				updateRackStrip(m_pInputRack,
					pAudioBus->monitor_in(), pAudioBus->busName() + tr(" In "));
			}
			if (pAudioBus->busMode() & qtractorBus::Output) {
				updateRackStrip(m_pOutputRack,
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
		if (pTrack->trackType() == qtractorTrack::Audio) {
			updateRackStrip(m_pTrackRack,
				pTrack->monitor(), pTrack->trackName(), pTrack->foreground());
		}
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


// end of qtractorMixer.cpp
