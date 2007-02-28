// qtractorMixer.cpp
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

#include "qtractorAbout.h"
#include "qtractorMixer.h"

#include "qtractorPluginListView.h"

#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"
#include "qtractorAudioMonitor.h"
#include "qtractorMidiMonitor.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorConnections.h"
#include "qtractorTrackButton.h"
#include "qtractorTrackCommand.h"
#include "qtractorEngineCommand.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorSlider.h"

#include "qtractorMainForm.h"
#include "qtractorBusForm.h"

#include <QSplitter>
#include <QFrame>
#include <QLabel>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMouseEvent>


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
		m_pBus(NULL), m_busMode(qtractorBus::None), m_pTrack(pTrack)
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

	m_pLayout = new QVBoxLayout(this);
	m_pLayout->setMargin(4);
	m_pLayout->setSpacing(4);

	m_pLabel = new QLabel(this);
	m_pLabel->setFont(font6);
	m_pLabel->setFixedHeight(fm.lineSpacing());
	m_pLabel->setBackgroundRole(QPalette::Button);
	m_pLabel->setForegroundRole(QPalette::ButtonText);
	m_pLabel->setAutoFillBackground(true);
	m_pLayout->addWidget(m_pLabel);

	m_pPluginListView = new qtractorPluginListView(this);
	m_pPluginListView->setFixedHeight(4 * fm.lineSpacing());
	m_pLayout->addWidget(m_pPluginListView);

	const QSizePolicy buttonPolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		m_pBusButton = NULL;
		m_pButtonLayout = new QHBoxLayout();
		m_pButtonLayout->setSpacing(2);
		const QSize buttonSize(16, 14);
		m_pRecordButton = new qtractorTrackButton(m_pTrack,
			qtractorTrack::Record, buttonSize, this);
		m_pRecordButton->setSizePolicy(buttonPolicy);
		m_pMuteButton = new qtractorTrackButton(m_pTrack,
			qtractorTrack::Mute, buttonSize, this);
		m_pMuteButton->setSizePolicy(buttonPolicy);
		m_pSoloButton = new qtractorTrackButton(m_pTrack,
			qtractorTrack::Solo, buttonSize, this);
		m_pSoloButton->setSizePolicy(buttonPolicy);
		m_pButtonLayout->addWidget(m_pRecordButton);
		m_pButtonLayout->addWidget(m_pMuteButton);
		m_pButtonLayout->addWidget(m_pSoloButton);
		m_pLayout->addLayout(m_pButtonLayout);
		qtractorMixer *pMixer = m_pRack->mixer();
		QObject::connect(m_pRecordButton,
			SIGNAL(trackButtonToggled(qtractorTrackButton *, bool)),
			pMixer, SLOT(trackButtonToggledSlot(qtractorTrackButton *, bool)));
		QObject::connect(m_pMuteButton,
			SIGNAL(trackButtonToggled(qtractorTrackButton *, bool)),
			pMixer, SLOT(trackButtonToggledSlot(qtractorTrackButton *, bool)));
		QObject::connect(m_pSoloButton,
			SIGNAL(trackButtonToggled(qtractorTrackButton *, bool)),
			pMixer, SLOT(trackButtonToggledSlot(qtractorTrackButton *, bool)));
	} else {
		meterType = m_pBus->busType();
		const QString& sText = m_pRack->name();
		m_pBusButton = new QToolButton(this);
		m_pBusButton->setFixedHeight(14);
		m_pBusButton->setSizePolicy(buttonPolicy);
		m_pBusButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		m_pBusButton->setFont(font6);
		m_pBusButton->setText(sText);
		m_pBusButton->setToolTip(tr("Connect %1").arg(sText));
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
	int iFixedWidth = 38;
	switch (meterType) {
	case qtractorTrack::Audio: {
		// Type cast for proper audio monitor...
		qtractorAudioMonitor *pAudioMonitor = NULL;
		if (m_pTrack) {
			pAudioMonitor
				= static_cast<qtractorAudioMonitor *> (m_pTrack->monitor());
			m_pPluginListView->setPluginList(m_pTrack->pluginList());
		} else {
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (m_pBus);
			if (pAudioBus) {
				if (m_busMode & qtractorBus::Input) {
					m_pPluginListView->setPluginList(
						pAudioBus->pluginList_in());
					pAudioMonitor = pAudioBus->audioMonitor_in();
				} else {
					m_pPluginListView->setPluginList(
						pAudioBus->pluginList_out());
					pAudioMonitor = pAudioBus->audioMonitor_out();
				}
			}
		}
		// Have we an audio monitor/meter?...
		if (pAudioMonitor) {
			iFixedWidth += 16 * (pAudioMonitor->channels() < 2
				? 2 : pAudioMonitor->channels());
			m_pMeter = new qtractorAudioMeter(pAudioMonitor, this);
		}
		m_pPluginListView->setEnabled(true);
		break;
	}
	case qtractorTrack::Midi: {
		// Type cast for proper MIDI monitor...
		qtractorMidiMonitor *pMidiMonitor = NULL;
		if (m_pTrack) {
			pMidiMonitor
				= static_cast<qtractorMidiMonitor *> (m_pTrack->monitor());
		} else if (m_pBus) {
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
			iFixedWidth += 32;
			m_pMeter = new qtractorMidiMeter(pMidiMonitor, this);
			// No panning on MIDI bus monitors and on duplex ones
			// only the output gain (volume) should be enabled...
			if (m_pBus) {
				m_pMeter->panSlider()->setEnabled(false);
				m_pMeter->panSpinBox()->setEnabled(false);
				if ((m_busMode & qtractorBus::Input) &&
					(m_pBus->busMode() & qtractorBus::Output)) {
					m_pMeter->gainSlider()->setEnabled(false);
					m_pMeter->gainSpinBox()->setEnabled(false);
				}
			}
		}
		m_pPluginListView->setEnabled(false);
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}

	// Eventually the right one...
	if (m_pMeter) {
		m_pLayout->addWidget(m_pMeter);
		QObject::connect(m_pMeter, SIGNAL(panChangedSignal(float)),
			this, SLOT(panChangedSlot(float)));
		QObject::connect(m_pMeter, SIGNAL(gainChangedSignal(float)),
			this, SLOT(gainChangedSlot(float)));
	}

	QFrame::setFrameShape(QFrame::StyledPanel);
	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setFixedWidth(iFixedWidth);
//	QFrame::setSizePolicy(
//		QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding));

	QFrame::setBackgroundRole(QPalette::Window);
//	QFrame::setForegroundRole(QPalette::WindowText);
	QFrame::setAutoFillBackground(true);

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
		QPalette pal(m_pLabel->palette());
		pal.setColor(QPalette::Button, m_pTrack->foreground().light());
		pal.setColor(QPalette::ButtonText, m_pTrack->background().light());
		m_pLabel->setPalette(pal);
		m_pLabel->setAlignment(Qt::AlignLeft);
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

	QFrame::setToolTip(sName + ' ' + sSuffix);
}



// Child accessors.
qtractorPluginListView *qtractorMixerStrip::pluginListView (void) const
{
	return m_pPluginListView;
}

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

	if (m_pBus->busType() == qtractorTrack::Audio) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (m_pBus);
		if (pAudioBus) {
			if (m_busMode & qtractorBus::Input) {
				m_pPluginListView->setPluginList(pAudioBus->pluginList_in());
			} else {
				m_pPluginListView->setPluginList(pAudioBus->pluginList_out());
			}
		}
		m_pPluginListView->setEnabled(true);
	} else {
		m_pPluginListView->setEnabled(false);
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

	m_pPluginListView->setPluginList(m_pTrack->pluginList());
	m_pPluginListView->setEnabled(
		m_pTrack->trackType() == qtractorTrack::Audio);

	m_pRecordButton->setTrack(m_pTrack);
	m_pMuteButton->setTrack(m_pTrack);
	m_pSoloButton->setTrack(m_pTrack);

	setMonitor(m_pTrack->monitor());

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

	QPalette pal;
	if (m_bSelected) {
		pal.setColor(QPalette::Window, pal.midlight().color().dark(150));
		pal.setColor(QPalette::WindowText, pal.midlight().color().light(150));
	}
	QFrame::setPalette(pal);
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


// Mouse selection event handlers.
void qtractorMixerStrip::mouseDoubleClickEvent ( QMouseEvent * /*pMouseEvent*/ )
{
	if (m_pTrack) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->trackProperties();
	} else if (m_pBus) {
		qtractorBusForm busForm(this);
		busForm.setBus(m_pBus);
		busForm.exec();
	}
}


// Bus connection button slot
void qtractorMixerStrip::busButtonSlot (void)
{
	if (m_pBus == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorMixerStrip::busButtonSlot() name=\"%s\"\n",
		m_pLabel->text().toUtf8().constData());
#endif

	// Here we go...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->connections()->showBus(m_pBus, m_busMode);
}


// Pan-meter slider value change slot.
void qtractorMixerStrip::panChangedSlot ( float fPanning )
{
	if (m_pMeter == NULL)
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorMixerStrip::panChangedSlot(%.3g)\n", fPanning);
#endif

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Put it in the form of an undoable command...
	if (m_pTrack) {
		pMainForm->commands()->exec(
			new qtractorTrackPanningCommand(pMainForm, m_pTrack, fPanning));
	} else if (m_pBus) {
		pMainForm->commands()->exec(
			new qtractorBusPanningCommand(pMainForm,
				m_pBus, m_busMode, fPanning));
	}
}


// Gain-meter slider value change slot.
void qtractorMixerStrip::gainChangedSlot ( float fGain )
{
	if (m_pMeter == NULL)
		return;

#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorMixerStrip::gainChangedSlot(%.3g)\n", fGain);
#endif

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Put it in the form of an undoable command...
	if (m_pTrack) {
		pMainForm->commands()->exec(
			new qtractorTrackGainCommand(pMainForm, m_pTrack, fGain));
	} else if (m_pBus) {
		pMainForm->commands()->exec(
			new qtractorBusGainCommand(pMainForm,
				m_pBus, m_busMode, fGain));
	}
}


//----------------------------------------------------------------------------
// qtractorMixerRack -- Meter bridge rack.

// Constructor.
qtractorMixerRack::qtractorMixerRack (
	qtractorMixer *pMixer, const QString& sName )
	: QScrollArea(pMixer->splitter()), m_pMixer(pMixer), m_sName(sName),
		m_bSelectEnabled(false), m_pSelectedStrip(NULL)
{
	m_pWorkspaceLayout = new QHBoxLayout();
	m_pWorkspaceLayout->setMargin(0);
	m_pWorkspaceLayout->setSpacing(0);

	m_pWorkspace = new QWidget(this);
	m_pWorkspace->setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	m_pWorkspace->setLayout(m_pWorkspaceLayout);

	QScrollArea::viewport()->setBackgroundRole(QPalette::Dark);
//	QScrollArea::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwayOn);
	QScrollArea::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QScrollArea::setWidget(m_pWorkspace);
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
	// Add this to the workspace layout...
	m_pWorkspaceLayout->addWidget(pStrip);

	m_strips.append(pStrip);
	pStrip->show();
}


// Find a mixer strip, given its monitor handle.
qtractorMixerStrip *qtractorMixerRack::findStrip ( qtractorMonitor *pMonitor )
{
	QListIterator<qtractorMixerStrip *> iter(m_strips);
	while (iter.hasNext()) {
		qtractorMixerStrip *pStrip = iter.next();
		if (pStrip->meter() && (pStrip->meter())->monitor() == pMonitor)
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
QWidget *qtractorMixerRack::workspace (void) const
{
	return m_pWorkspace;
}


// Complete rack refreshment.
void qtractorMixerRack::refresh (void)
{
	QListIterator<qtractorMixerStrip *> iter(m_strips);
	while (iter.hasNext())
		iter.next()->refresh();
}


// Complete rack recycle.
void qtractorMixerRack::clear (void)
{
	m_pSelectedStrip = NULL;

	qDeleteAll(m_strips);
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
	m_pWorkspace->setUpdatesEnabled(false);

	QListIterator<qtractorMixerStrip *> iter(m_strips);
	while (iter.hasNext())
		iter.next()->setMark(iMark);
}

void qtractorMixerRack::cleanStrips ( int iMark )
{
	QMutableListIterator<qtractorMixerStrip *> iter(m_strips);
	while (iter.hasNext()) {
		qtractorMixerStrip *pStrip = iter.next();
		if (pStrip->mark() == iMark) {
			// Don't let current selection hanging...
			if (m_pSelectedStrip == pStrip)
				m_pSelectedStrip = NULL;
			// Hide strip...
			pStrip->hide();
			// Remove from list...
			iter.remove();
			// and finally get rid of it.
			delete pStrip;
		}
	}

	m_pWorkspace->adjustSize();
	m_pWorkspace->setUpdatesEnabled(true);
}


// Resize event handler.
void qtractorMixerRack::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QScrollArea::resizeEvent(pResizeEvent);

//	m_pWorkspace->setMinimumWidth(QScrollArea::viewport()->width());
	m_pWorkspace->setFixedHeight(QScrollArea::viewport()->height());
	m_pWorkspace->adjustSize();
}



// Context menu request event handler.
void qtractorMixerRack::contextMenuEvent ( QContextMenuEvent *pContextMenuEvent )
{
	if (m_bSelectEnabled) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->trackMenu()->exec(pContextMenuEvent->globalPos());
	}
}


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

// Constructor.
qtractorMixer::qtractorMixer ( QWidget *pParent )
	: QWidget(pParent, Qt::Tool
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowMinMaxButtonsHint)
{
	// Surely a name is crucial (e.g. for storing geometry settings)
	QWidget::setObjectName("qtractorMixer");

	m_pSplitter = new QSplitter(Qt::Horizontal, this);
	m_pSplitter->setObjectName("MixerSplitter");
	m_pSplitter->setChildrenCollapsible(false);
//	m_pSplitter->setOpaqueResize(false);
	m_pSplitter->setHandleWidth(2);

	m_pInputRack  = new qtractorMixerRack(this, tr("Inputs"));
	m_pTrackRack  = new qtractorMixerRack(this, tr("Tracks"));
	m_pTrackRack->setSelectEnabled(true);
	m_pOutputRack = new qtractorMixerRack(this, tr("Outputs"));

	m_pSplitter->setStretchFactor(m_pSplitter->indexOf(m_pInputRack), 0);
	m_pSplitter->setStretchFactor(m_pSplitter->indexOf(m_pOutputRack), 0);

	// Prepare the layout stuff.
	QHBoxLayout *pLayout = new QHBoxLayout();
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pSplitter);
	QWidget::setLayout(pLayout);

	// Some specialties to this kind of dock window...
	QWidget::setMinimumWidth(440);
	QWidget::setMinimumHeight(120);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Mixer") + " - " QTRACTOR_TITLE;
	QWidget::setWindowTitle(sCaption);
	QWidget::setWindowIcon(QIcon(":/icons/viewMixer.png"));
	QWidget::setToolTip(sCaption);

	// Get previously saved splitter sizes,
	// (with some fair default...)
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			QList<int> sizes;
			sizes.append(140);
			sizes.append(160);
			sizes.append(140);
			pOptions->loadSplitterSizes(m_pSplitter, sizes);
		}
	}
}


// Default destructor.
qtractorMixer::~qtractorMixer (void)
{
	// Save splitter sizes...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions)
			pOptions->saveSplitterSizes(m_pSplitter);
	}

	// No need to delete child widgets, Qt does it all for us
}


// Notify the main application widget that we're emerging.
void qtractorMixer::showEvent ( QShowEvent *pShowEvent )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorMixer::showEvent()\n");
#endif

    qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
    if (pMainForm)
        pMainForm->stabilizeForm();

    QWidget::showEvent(pShowEvent);
}

// Notify the main application widget that we're closing.
void qtractorMixer::hideEvent ( QHideEvent *pHideEvent )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorMixer::hideEvent()\n");
#endif

    QWidget::hideEvent(pHideEvent);

    qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
    if (pMainForm)
        pMainForm->stabilizeForm();
}


// Just about to notify main-window that we're closing.
void qtractorMixer::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
#ifdef CONFIG_DEBUG_0
	fprintf(stderr, "qtractorMixer::closeEvent()\n");
#endif

	QWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Session accessor.
qtractorSession *qtractorMixer::session (void) const
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	return (pMainForm ? pMainForm->session() : NULL);
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
	qtractorBus *pBus, qtractorBus::BusMode busMode, bool bReset )
{
	qtractorMonitor *pMonitor
		= (busMode == qtractorBus::Input ?
			pBus->monitor_in() : pBus->monitor_out());

	qtractorMixerStrip *pStrip = pRack->findStrip(pMonitor);
	if (pStrip == NULL) {
		pRack->addStrip(new qtractorMixerStrip(pRack, pBus, busMode));
	} else {
		pStrip->setMark(0);
		if (bReset)
			pStrip->setBus(pBus);
	}
}


void qtractorMixer::updateTrackStrip ( qtractorTrack *pTrack, bool bReset )
{
	qtractorMixerStrip *pStrip = m_pTrackRack->findStrip(pTrack->monitor());
	if (pStrip == NULL) {
		m_pTrackRack->addStrip(new qtractorMixerStrip(m_pTrackRack, pTrack));
	} else {
		pStrip->setMark(0);
		if (bReset)
			pStrip->setTrack(pTrack);
	}
}


// Update buses'racks.
void qtractorMixer::updateBuses (void)
{
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return;

	m_pInputRack->markStrips(1);
	m_pOutputRack->markStrips(1);

	// Audio buses first...
	for (qtractorBus *pBus = pSession->audioEngine()->buses().first();
			pBus; pBus = pBus->next()) {
		if (pBus->busMode() & qtractorBus::Input)
			updateBusStrip(m_pInputRack, pBus, qtractorBus::Input);
		if (pBus->busMode() & qtractorBus::Output)
			updateBusStrip(m_pOutputRack, pBus, qtractorBus::Output);
	}

	// MIDI buses are next...
	for (qtractorBus *pBus = pSession->midiEngine()->buses().first();
			pBus; pBus = pBus->next()) {
		if (pBus->busMode() & qtractorBus::Input)
			updateBusStrip(m_pInputRack, pBus, qtractorBus::Input);
		if (pBus->busMode() & qtractorBus::Output)
			updateBusStrip(m_pOutputRack, pBus, qtractorBus::Output);
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
void qtractorMixer::trackButtonToggledSlot (
	qtractorTrackButton *pTrackButton, bool bOn )
{
	// Put it in the form of an undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorTrackButtonCommand(pMainForm, pTrackButton, bOn));
}


// end of qtractorMixer.cpp
