// qtractorMixer.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiManager.h"

#include "qtractorObserverWidget.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorTracks.h"
#include "qtractorConnections.h"
#include "qtractorTrackCommand.h"
#include "qtractorEngineCommand.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMidiControlObserver.h"

#include "qtractorPlugin.h"
#include "qtractorCurve.h"

#include "qtractorMainForm.h"
#include "qtractorBusForm.h"

#include <QGridLayout>
#include <QVBoxLayout>

#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QMouseEvent>

#include <QPainter>

#ifdef CONFIG_GRADIENT
#include <QLinearGradient>
#endif


//----------------------------------------------------------------------------
// qtractorMonitorButton -- Monitor observer tool button.

// Constructors.
qtractorMonitorButton::qtractorMonitorButton (
	qtractorTrack *pTrack, QWidget *pParent )
	: qtractorMidiControlButton(pParent)
{
	initMonitorButton();

	setTrack(pTrack);
}


qtractorMonitorButton::qtractorMonitorButton (
	qtractorBus *pBus, QWidget *pParent )
	: qtractorMidiControlButton(pParent)
{
	initMonitorButton();

	setBus(pBus);
}


// Destructor.
qtractorMonitorButton::~qtractorMonitorButton (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == nullptr)
		return;

	qtractorMidiControlObserver *pMidiObserver = nullptr;

	if (m_pTrack)
		pMidiObserver = m_pTrack->monitorObserver();
	else
	if (m_pBus) {
		pMidiObserver = m_pBus->monitorObserver();
	}

	if (pMidiObserver)
		pMidiControl->unmapMidiObserverWidget(pMidiObserver, this);
}


// Common initializer.
void qtractorMonitorButton::initMonitorButton (void)
{
	QPushButton::setIconSize(QSize(8, 8));
	QPushButton::setIcon(QIcon::fromTheme("itemLedOff"));
	QPushButton::setText(' ' + tr("monitor"));
	QPushButton::setCheckable(true);

	QObject::connect(this, SIGNAL(toggled(bool)), SLOT(toggledSlot(bool)));
}


// Specific track accessors.
void qtractorMonitorButton::setTrack ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;
	m_pBus = nullptr;

//	QPushButton::setToolTip(tr("Monitor (rec)"));

	updateMonitor(); // Visitor setup.
}


// Specific bus accessors.
void qtractorMonitorButton::setBus ( qtractorBus *pBus )
{
	m_pBus = pBus;
	m_pTrack = nullptr;
	
//	QPushButton::setToolTip(tr("Monitor (thru)"));

	updateMonitor(); // Visitor setup.
}


// Visitors overload.
void qtractorMonitorButton::updateValue ( float fValue )
{
	// Avoid self-triggering...
	const bool bBlockSignals = QPushButton::blockSignals(true);
	if (fValue > 0.0f) {
		QPushButton::setIcon(QIcon::fromTheme("itemLedOn"));
		QPushButton::setChecked(true);
	} else {
		QPushButton::setIcon(QIcon::fromTheme("itemLedOff"));
		QPushButton::setChecked(false);
	}
	QPushButton::blockSignals(bBlockSignals);
}


// Special toggle slot.
void qtractorMonitorButton::toggledSlot ( bool bOn )
{
	// Just emit proper signal...
	if (m_pTrack)
		m_pTrack->monitorChangeNotify(bOn);
	else
	if (m_pBus)
		m_pBus->monitorChangeNotify(bOn);
}


// Monitor state button setup.
void qtractorMonitorButton::updateMonitor (void)
{
	if (m_pTrack) {
		setSubject(m_pTrack->monitorSubject());
		qtractorMidiControlObserver *pMidiObserver
			= m_pTrack->monitorObserver();
		if (pMidiObserver) {
			pMidiObserver->setCurveList(m_pTrack->curveList());
			addMidiControlAction(pMidiObserver);
		}
	}
	else
	if (m_pBus) {
		if ((m_pBus->busMode() & qtractorBus::Duplex) == qtractorBus::Duplex) {
			setSubject(m_pBus->monitorSubject());
			addMidiControlAction(m_pBus->monitorObserver());
			QPushButton::setEnabled(true);
		} else {
			QPushButton::setEnabled(false);
		}
	}

	observer()->update(true);
}


//----------------------------------------------------------------------------
// qtractorMixerStrip::IconLabel -- Custom mixer strip title widget.

class qtractorMixerStrip::IconLabel : public QLabel
{
public:

	// Constructor.
	IconLabel(QWidget *pParent = nullptr) : QLabel(pParent) {}

	// Icon accessors.
	void setIcon(const QIcon& icon)
		{ m_icon = icon; }
	const QIcon& icon() const
		{ return m_icon; }

protected:

	// Custom paint event.
	void paintEvent(QPaintEvent *)
	{
		QPainter painter(this);
		QRect rect(QLabel::rect());
		const int x = rect.x() + 1;
		const int y = rect.y() + ((rect.height() - 16) >> 1) + 1;
		painter.drawPixmap(x, y, m_icon.pixmap(16));
		rect.adjust(+16, +2, -1, 0);
		painter.drawText(rect, QLabel::alignment(), QLabel::text());
	}

private:

	// Instance variables.
	QIcon m_icon;
};

	
//----------------------------------------------------------------------------
// qtractorMixerStrip -- Mixer strip widget.

// Constructors.
qtractorMixerStrip::qtractorMixerStrip (
	qtractorMixerRack *pRack, qtractorBus *pBus, qtractorBus::BusMode busMode )
	: QFrame(pRack->workspace()), m_pRack(pRack),
		m_pBus(pBus), m_busMode(busMode), m_pTrack(nullptr)
{
	initMixerStrip();
}

qtractorMixerStrip::qtractorMixerStrip (
	qtractorMixerRack *pRack, qtractorTrack *pTrack )
	: QFrame(pRack->workspace()), m_pRack(pRack),
		m_pBus(nullptr), m_busMode(qtractorBus::None), m_pTrack(pTrack)
{
	initMixerStrip();
}


// Default destructor.
qtractorMixerStrip::~qtractorMixerStrip (void)
{
	// Take special care to nullify the audio output monitor
	// on MIDI track or buses, avoid its removal at meter's dtor...
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack)
		meterType = m_pTrack->trackType();
	else
	if (m_pBus)
		meterType = m_pBus->busType();
	if (meterType == qtractorTrack::Midi) {
		qtractorMidiMixerMeter *pMidiMixerMeter
			= static_cast<qtractorMidiMixerMeter *> (m_pMixerMeter);
		if (pMidiMixerMeter)
			pMidiMixerMeter->setAudioOutputMonitor(nullptr);
	}

	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl&& m_pMixerMeter) {
		qtractorMidiControlObserver *pMidiObserver;
		pMidiObserver = m_pMixerMeter->monitor()->panningObserver();
		pMidiControl->unmapMidiObserverWidget(
			pMidiObserver, m_pMixerMeter->panSlider());
		pMidiObserver = m_pMixerMeter->monitor()->gainObserver();
		pMidiControl->unmapMidiObserverWidget(
			pMidiObserver, m_pMixerMeter->gainSlider());
	}

	// No need to delete child widgets, Qt does it all for us
#if 0
	if (m_pMidiLabel)
		delete m_pMidiLabel;

	if (m_pMixerMeter)
		delete m_pMixerMeter;

	if (m_pSoloButton)
		delete m_pSoloButton;
	if (m_pMuteButton)
		delete m_pMuteButton;
	if (m_pRecordButton)
		delete m_pRecordButton;

	if (m_pMonitorButton)
		delete m_pMonitorButton;
	if (m_pBusButton)
		delete m_pBusButton;

	delete m_pButtonLayout;
	delete m_pPluginListView;

	delete m_pLabel;
	delete m_pLayout;
#endif
}


// Common mixer-strip initializer.
void qtractorMixerStrip::initMixerStrip (void)
{
	m_iMark = 0;

	const QFont& font = QFrame::font();
	const QFont font2(font.family(), font.pointSize() - 2);
	const QFont font3(font.family(), font.pointSize() - 3);
	const int iFixedHeight = QFontMetrics(font2).lineSpacing() + 4;

	QFrame::setFont(font2);

	m_pLayout = new QVBoxLayout(this);
	m_pLayout->setContentsMargins(4, 4, 4, 4);
	m_pLayout->setSpacing(4);

	m_pLabel = new IconLabel(/*this*/);
//	m_pLabel->setFont(font2);
	m_pLabel->setFixedHeight(iFixedHeight);
	m_pLabel->setBackgroundRole(QPalette::Button);
	m_pLabel->setForegroundRole(QPalette::ButtonText);
	m_pLabel->setAutoFillBackground(true);
	m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	m_pLayout->addWidget(m_pLabel);

	m_pRibbon = new QFrame(/*this*/);
	m_pRibbon->setFixedHeight(4);
	m_pRibbon->setBackgroundRole(QPalette::Button);
	m_pRibbon->setForegroundRole(QPalette::ButtonText);
	m_pRibbon->setAutoFillBackground(true);
	m_pLayout->addWidget(m_pRibbon);

	m_pPluginListView = new qtractorPluginListView(/*this*/);
	m_pPluginListView->setFont(font3);
	m_pPluginListView->setMinimumHeight(iFixedHeight << 1);
	m_pPluginListView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	m_pPluginListView->setTinyScrollBar(true);
	m_pLayout->addWidget(m_pPluginListView, 1);

	const QSizePolicy buttonPolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	m_pButtonLayout = new QHBoxLayout(/*this*/);
	m_pButtonLayout->setContentsMargins(0, 0, 0, 0);
	m_pButtonLayout->setSpacing(2);

	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		m_pMonitorButton = new qtractorMonitorButton(m_pTrack);
		m_pMonitorButton->setFixedHeight(iFixedHeight);
		m_pMonitorButton->setSizePolicy(buttonPolicy);
		m_pMonitorButton->setFont(font3);
		m_pRecordButton
			= new qtractorTrackButton(m_pTrack, qtractorTrack::Record);
		m_pRecordButton->setFixedHeight(iFixedHeight);
		m_pRecordButton->setSizePolicy(buttonPolicy);
		m_pRecordButton->setFont(font3);
		m_pMuteButton
			= new qtractorTrackButton(m_pTrack, qtractorTrack::Mute);
		m_pMuteButton->setFixedHeight(iFixedHeight);
		m_pMuteButton->setSizePolicy(buttonPolicy);
		m_pMuteButton->setFont(font3);
		m_pSoloButton
			= new qtractorTrackButton(m_pTrack, qtractorTrack::Solo);
		m_pSoloButton->setFixedHeight(iFixedHeight);
		m_pSoloButton->setSizePolicy(buttonPolicy);
		m_pSoloButton->setFont(font3);
		m_pButtonLayout->addWidget(m_pRecordButton);
		m_pButtonLayout->addWidget(m_pMuteButton);
		m_pButtonLayout->addWidget(m_pSoloButton);
		m_pBusButton = nullptr;
	}
	else
	if (m_pBus) {
		meterType = m_pBus->busType();
		m_pMonitorButton = new qtractorMonitorButton(m_pBus);
		m_pMonitorButton->setFixedHeight(iFixedHeight);
		m_pMonitorButton->setSizePolicy(buttonPolicy);
		m_pMonitorButton->setFont(font3);
		m_pBusButton = new QPushButton(/*this*/);
		m_pBusButton->setFixedHeight(iFixedHeight);
		m_pBusButton->setFocusPolicy(Qt::NoFocus);
		m_pBusButton->setSizePolicy(buttonPolicy);
	//	m_pBusButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		m_pBusButton->setFont(font3);
		m_pBusButton->setText(
			m_busMode & qtractorBus::Input ? tr("inputs") : tr("outputs"));
		m_pBusButton->setToolTip(tr("Connect %1").arg(m_pBusButton->text()));
		m_pButtonLayout->addWidget(m_pBusButton);
		QObject::connect(m_pBusButton,
			SIGNAL(clicked()),
			SLOT(busButtonSlot()));
		m_pRecordButton = nullptr;
		m_pMuteButton   = nullptr;
		m_pSoloButton   = nullptr;
	}

	m_pLayout->addWidget(m_pMonitorButton);
	m_pLayout->addLayout(m_pButtonLayout);
	
	// Now, there's whether we are Audio or MIDI related...
	m_pMixerMeter = nullptr;
	m_pMidiLabel = nullptr;
	int iFixedWidth = 64;
	QPalette pal(m_pRibbon->palette());
	switch (meterType) {
	case qtractorTrack::Audio: {
		// Set header ribbon color (Audio)...
		pal.setColor(QPalette::Button,
			qtractorAudioMeter::color(qtractorAudioMeter::Color10dB));
		// Type cast for proper audio monitor...
		qtractorAudioMonitor *pAudioMonitor = nullptr;
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
			const int iAudioChannels = pAudioMonitor->channels();
			iFixedWidth += (iAudioChannels < 3 ? 24 : 8 * iAudioChannels);
			m_pMixerMeter = new qtractorAudioMixerMeter(pAudioMonitor, this);
		}
		m_pPluginListView->setEnabled(true);
		break;
	}
	case qtractorTrack::Midi: {
		// Set header ribbon color (MIDI)...
		pal.setColor(QPalette::Button,
			qtractorMidiMeter::color(qtractorMidiMeter::ColorOver));
		// Type cast for proper MIDI monitor...
		qtractorMidiMonitor *pMidiMonitor = nullptr;
		qtractorMidiBus *pMidiBus = nullptr;
		if (m_pTrack) {
			pMidiMonitor
				= static_cast<qtractorMidiMonitor *> (m_pTrack->monitor());
			m_pPluginListView->setPluginList(m_pTrack->pluginList());
			m_pPluginListView->setEnabled(true);
		} else {
			pMidiBus = static_cast<qtractorMidiBus *> (m_pBus);
			if (pMidiBus) {
				if (m_busMode & qtractorBus::Input) {
					m_pPluginListView->setPluginList(
						pMidiBus->pluginList_in());
					pMidiMonitor = pMidiBus->midiMonitor_in();
				} else {
					m_pPluginListView->setPluginList(
						pMidiBus->pluginList_out());
					pMidiMonitor = pMidiBus->midiMonitor_out();
				}
			}
			m_pPluginListView->setEnabled(true);
		}
		// Have we a MIDI monitor/meter?...
		if (pMidiMonitor) {
			iFixedWidth += 24;
			qtractorMidiMixerMeter *pMidiMixerMeter
				= new qtractorMidiMixerMeter(pMidiMonitor, this);
			// MIDI Tracks might need to show something,
			// like proper MIDI channel settings...
			if (m_pTrack) {
				m_pMidiLabel = new QLabel(/*m_pMeter->topWidget()*/);
			//	m_pMidiLabel->setFont(font2);
				m_pMidiLabel->setAlignment(
					Qt::AlignHCenter | Qt::AlignVCenter);
				pMidiMixerMeter->topLayout()->insertWidget(1, m_pMidiLabel);
				updateMidiLabel();
			}
			// No panning on MIDI bus monitors and on duplex ones
			// only on the output buses should be enabled...
			if (pMidiBus) {
				if ((m_busMode & qtractorBus::Input)
					&& (m_pBus->busMode() & qtractorBus::Output)) {
					pMidiMixerMeter->panSlider()->setEnabled(false);
					pMidiMixerMeter->panSpinBox()->setEnabled(false);
					pMidiMixerMeter->gainSlider()->setEnabled(false);
					pMidiMixerMeter->gainSpinBox()->setEnabled(false);
				}
			}
			// Apply the combo-meter posssibility...
			qtractorMidiManager *pMidiManager = nullptr;
			qtractorPluginList *pPluginList = m_pPluginListView->pluginList();
			if (pPluginList)
				pMidiManager = pPluginList->midiManager();
			if (pMidiManager && pMidiManager->isAudioOutputMonitor()) {
				pMidiMixerMeter->setAudioOutputMonitor(
					pMidiManager->audioOutputMonitor());
			}
			// Ready.
			m_pMixerMeter = pMidiMixerMeter;
		}
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}
	m_pRibbon->setPalette(pal);

	// Eventually the right one...
	if (m_pMixerMeter) {
		// Set MIDI controller & automation hooks...
		qtractorMidiControlObserver *pMidiObserver;
		pMidiObserver = m_pMixerMeter->monitor()->panningObserver();
		if (m_pTrack)
			pMidiObserver->setCurveList(m_pTrack->curveList());
		m_pMixerMeter->addMidiControlAction(m_pMixerMeter->panSlider(), pMidiObserver);
		pMidiObserver = m_pMixerMeter->monitor()->gainObserver();
		if (m_pTrack)
			pMidiObserver->setCurveList(m_pTrack->curveList());
		m_pMixerMeter->addMidiControlAction(m_pMixerMeter->gainSlider(), pMidiObserver);
		// Finally, add to layout...	
		m_pLayout->addWidget(m_pMixerMeter, 4);
		QObject::connect(m_pMixerMeter->panSlider(),
			SIGNAL(valueChanged(float)),
			SLOT(panningChangedSlot(float)));
		QObject::connect(m_pMixerMeter->panSpinBox(),
			SIGNAL(valueChanged(float)),
			SLOT(panningChangedSlot(float)));
		QObject::connect(m_pMixerMeter->gainSlider(),
			SIGNAL(valueChanged(float)),
			SLOT(gainChangedSlot(float)));
		QObject::connect(m_pMixerMeter->gainSpinBox(),
			SIGNAL(valueChanged(float)),
			SLOT(gainChangedSlot(float)));
	}

	QFrame::setFrameShape(QFrame::Panel);
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
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack)
		meterType = m_pTrack->trackType();
	else if (m_pBus)
		meterType = m_pBus->busType();
	if (meterType == qtractorTrack::Audio) {
		qtractorAudioMonitor *pAudioMonitor
			= static_cast<qtractorAudioMonitor *> (pMonitor);
		if (pAudioMonitor) {
			const int iOldWidth = QFrame::width();
			const int iAudioChannels = pAudioMonitor->channels();
			const int iFixedWidth = 64
				+ (iAudioChannels < 3 ? 24 : 8 * iAudioChannels);
			if (iFixedWidth != iOldWidth) {
				QFrame::setFixedWidth(iFixedWidth);
				m_pRack->updateWorkspace();
			}
		}
	}
	else
	if (meterType == qtractorTrack::Midi) {
		qtractorMidiMixerMeter *pMidiMixerMeter
			= static_cast<qtractorMidiMixerMeter *> (m_pMixerMeter);
		if (pMidiMixerMeter) {
			qtractorPluginList *pPluginList = nullptr;
			if (m_pBus) {
				if (m_busMode & qtractorBus::Input)
					pPluginList = m_pBus->pluginList_in();
				else
					pPluginList = m_pBus->pluginList_in();
			}
			else
			if (m_pTrack)
				pPluginList = m_pTrack->pluginList();
			if (pPluginList) {
				qtractorMidiManager *pMidiManager
					= pPluginList->midiManager();
				if (pMidiManager && pMidiManager->isAudioOutputMonitor()) {
					pMidiMixerMeter->setAudioOutputMonitor(
						pMidiManager->audioOutputMonitor());
				} else {
					pMidiMixerMeter->setAudioOutputMonitor(nullptr);
				}
			}
		}
	}

	if (m_pMixerMeter)
		m_pMixerMeter->setMonitor(pMonitor);
}

qtractorMonitor *qtractorMixerStrip::monitor (void) const
{
	return (m_pMixerMeter ? m_pMixerMeter->monitor() : nullptr);
}


// Common mixer-strip caption title updater.
void qtractorMixerStrip::updateName (void)
{
	QIcon icon;
	QString sName;
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		sName = m_pTrack->shortTrackName();
		QPalette pal(m_pLabel->palette());
		const QColor& bg = m_pTrack->foreground().lighter();
		QColor fg = m_pTrack->background().lighter();
		if (qAbs(bg.value() - fg.value()) < 0x33)
			fg.setHsv(fg.hue(), fg.saturation(), (255 - fg.value()), 200);
		pal.setColor(QPalette::Button, bg);
		pal.setColor(QPalette::ButtonText, fg);
		m_pLabel->setPalette(pal);
		const QString& sTrackIcon
			= m_pTrack->trackIcon();
		icon = QIcon::fromTheme(sTrackIcon);
		if (icon.isNull())
			icon = QIcon(sTrackIcon);
	} else if (m_pBus) {
		meterType = m_pBus->busType();
		sName = m_pBus->busName();
	}

	QString sType;
	switch (meterType) {
	case qtractorTrack::Audio:
		if (icon.isNull())
			icon = QIcon::fromTheme("trackAudio");
		sType = tr("(Audio)");
		break;
	case qtractorTrack::Midi:
		if (icon.isNull())
			icon = QIcon::fromTheme("trackMidi");
		sType = tr("(MIDI)");
		break;
	case qtractorTrack::None:
	default:
		sType = tr("(None)");
		break;
	}

	m_pLabel->setIcon(icon);
	m_pLabel->setText(sName);
	m_pLabel->update(); // Make sure icon and text gets visibly updated!

	if (m_pTrack)
		QFrame::setToolTip(m_pTrack->trackName() + ' ' + sType);
	else
	if (m_pBus) {
		const QString& sMode
			= (m_busMode & qtractorBus::Input ? tr("In") : tr("Out"));
		QFrame::setToolTip(sName + ' ' + sMode + ' ' + sType);
	}
}


// MIDI (channel) label updater.
void qtractorMixerStrip::updateMidiLabel (void)
{
	if (m_pTrack == nullptr)
		return;

	if (m_pMidiLabel == nullptr)
		return;

	QString sOmni;
	if (m_pTrack->isMidiOmni())
		sOmni += '*';
	m_pMidiLabel->setText(
		sOmni + QString::number(m_pTrack->midiChannel() + 1));
}


// Mixer strip clear/suspend delegates
void qtractorMixerStrip::clear (void)
{
	m_pPluginListView->setEnabled(false);
	m_pPluginListView->setPluginList(nullptr);

	m_pRack->updateStrip(this, nullptr);
}


// Bus property accessors.
void qtractorMixerStrip::setBus ( qtractorBus *pBus )
{
	// Must be actual bus...
	if (m_pBus == nullptr || pBus == nullptr)
		return;

	m_pBus = pBus;

	if (m_busMode & qtractorBus::Input) {
		m_pRack->updateStrip(this, m_pBus->monitor_in());
		m_pPluginListView->setPluginList(m_pBus->pluginList_in());
	} else {
		m_pRack->updateStrip(this, m_pBus->monitor_out());
		m_pPluginListView->setPluginList(m_pBus->pluginList_out());
	}

	m_pPluginListView->setEnabled(true);

	m_pMonitorButton->setBus(m_pBus);

	updateName();
}


// Track property accessors.
void qtractorMixerStrip::setTrack ( qtractorTrack *pTrack )
{
	// Must be actual track...
	if (m_pTrack == nullptr || pTrack == nullptr)
		return;

	m_pTrack = pTrack;

	m_pRack->updateStrip(this, m_pTrack->monitor());

	m_pPluginListView->setPluginList(m_pTrack->pluginList());
	m_pPluginListView->setEnabled(true);

	m_pMonitorButton->setTrack(m_pTrack);

	m_pRecordButton->setTrack(m_pTrack);
	m_pMuteButton->setTrack(m_pTrack);
	m_pSoloButton->setTrack(m_pTrack);

	updateMidiLabel();
	updateName();
}


// Selection methods.
void qtractorMixerStrip::setSelected ( bool bSelected )
{
	m_bSelected = bSelected;

	QPalette pal;
	QColor rgbBase;
	if (m_bSelected) {
		rgbBase = pal.midlight().color();
		pal.setColor(QPalette::WindowText,
			pal.highlightedText().color());
		pal.setColor(QPalette::Window,
			rgbBase.darker(150));
	} else {
		rgbBase	= pal.window().color();
		pal.setColor(QPalette::WindowText,
			pal.windowText().color());
		pal.setColor(QPalette::Window,
			rgbBase);
	}
#ifdef CONFIG_GRADIENT
	const QSize& hint = QFrame::sizeHint();
	QLinearGradient grad(0, 0, hint.width() >> 1, hint.height());
	if (m_bSelected) {
		grad.setColorAt(0.4, rgbBase.darker(150));
		grad.setColorAt(0.6, rgbBase.darker(130));
		grad.setColorAt(1.0, rgbBase.darker());
	} else {
		grad.setColorAt(0.4, rgbBase);
		grad.setColorAt(0.6, rgbBase.lighter(105));
		grad.setColorAt(1.0, rgbBase.darker(130));
	}
	const QBrush brush
		= pal.brush(QPalette::Window);
	pal.setBrush(QPalette::Window, grad);
#endif
	QFrame::setPalette(pal);
#ifdef CONFIG_GRADIENT
	pal.setBrush(QPalette::Window, brush);
#endif
	m_pPluginListView->setPalette(pal);
	m_pMonitorButton->setPalette(pal);
	if (m_pBusButton)
		m_pBusButton->setPalette(pal);
	if (m_pRecordButton)
		m_pRecordButton->setPalette(pal);
	if (m_pMuteButton)
		m_pMuteButton->setPalette(pal);
	if (m_pSoloButton)
		m_pSoloButton->setPalette(pal);
	if (m_pMixerMeter)
		m_pMixerMeter->setPalette(pal);
#ifdef CONFIG_GRADIENT
	if (m_pRecordButton)
		m_pRecordButton->observer()->update(true);
	if (m_pMuteButton)
		m_pMuteButton->observer()->update(true);
	if (m_pSoloButton)
		m_pSoloButton->observer()->update(true);
#endif
}


// Mouse selection event handlers.
void qtractorMixerStrip::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	QFrame::mousePressEvent(pMouseEvent);

	if (m_pTrack)
		m_pRack->setSelectedStrip(this);
}


// Mouse selection event handlers.
void qtractorMixerStrip::mouseDoubleClickEvent ( QMouseEvent */*pMouseEvent*/ )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	if (m_pTrack) {
		pMainForm->trackProperties();
	} else {
		busPropertiesSlot();
	}
}


// Bus connections dispatcher.
void qtractorMixerStrip::busConnections ( qtractorBus::BusMode busMode )
{
	if (m_pBus == nullptr)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
		return;

	// Here we go...
	pMainForm->connections()->showBus(m_pBus, busMode);
}


// Bus pass-through dispatcher.
void qtractorMixerStrip::busMonitor ( bool bMonitor )
{
	if (m_pBus == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Here we go...
	pSession->execute(
		new qtractorBusMonitorCommand(m_pBus, bMonitor));
}


// Track monitor dispatcher.
void qtractorMixerStrip::trackMonitor ( bool bMonitor )
{
	if (m_pTrack == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Here we go...
	pSession->execute(
		new qtractorTrackMonitorCommand(m_pTrack, bMonitor));
}


// Show/edit bus input connections.
void qtractorMixerStrip::busInputsSlot (void)
{
	busConnections(qtractorBus::Input);
}


// Show/edit bus output connections.
void qtractorMixerStrip::busOutputsSlot (void)
{
	busConnections(qtractorBus::Output);
}


// Toggle bus passthru flag.
void qtractorMixerStrip::busMonitorSlot (void)
{
	if (m_pBus)
		busMonitor(!m_pBus->isMonitor());
}


// Show/edit bus properties form.
void qtractorMixerStrip::busPropertiesSlot (void)
{
	if (m_pBus) {
		qtractorBusForm busForm(m_pRack);
		busForm.setBus(m_pBus);
		busForm.exec();
	}
}


// Bus connections button slot
void qtractorMixerStrip::busButtonSlot (void)
{
	busConnections(m_busMode);
}


// Pan-meter slider value change slot.
void qtractorMixerStrip::panningChangedSlot ( float fPanning )
{
	if (m_pMixerMeter == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMixerStrip[%p]::panningChangedSlot(%g)", this, fPanning);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Put it in the form of an undoable command...
	if (m_pTrack) {
		pSession->execute(
			new qtractorTrackPanningCommand(m_pTrack, fPanning));
	} else if (m_pBus) {
		pSession->execute(
			new qtractorBusPanningCommand(m_pBus, m_busMode, fPanning));
	}
}


// Gain-meter slider value change slot.
void qtractorMixerStrip::gainChangedSlot ( float fGain )
{
	if (m_pMixerMeter == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMixerStrip[%p]::gainChangedSlot(%g)", this, fGain);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	// Put it in the form of an undoable command...
	if (m_pTrack) {
		pSession->execute(
			new qtractorTrackGainCommand(m_pTrack, fGain));
	} else if (m_pBus) {
		pSession->execute(
			new qtractorBusGainCommand(m_pBus, m_busMode, fGain));
	}
}


// Update a MIDI mixer strip, given its MIDI manager handle.
void qtractorMixerStrip::updateMidiManager ( qtractorMidiManager *pMidiManager )
{
	qtractorMidiMixerMeter *pMidiMixerMeter
		= static_cast<qtractorMidiMixerMeter *> (m_pMixerMeter);
	if (pMidiMixerMeter == nullptr)
		return;

	// Apply the combo-meter posssibility...
	if (pMidiManager && pMidiManager->isAudioOutputMonitor()) {
		pMidiMixerMeter->setAudioOutputMonitor(
			pMidiManager->audioOutputMonitor());
	} else {
		pMidiMixerMeter->setAudioOutputMonitor(nullptr);
	}
}


// Retrieve the MIDI manager from a mixer strip, if any....
qtractorMidiManager *qtractorMixerStrip::midiManager (void) const
{
	qtractorPluginList *pPluginList = nullptr;

	if (m_pTrack && m_pTrack->trackType() == qtractorTrack::Midi) {
		pPluginList = m_pTrack->pluginList();
	}
	else
	if (m_pBus && m_pBus->busType() == qtractorTrack::Midi) {
		if ((m_busMode & qtractorBus::Input) &&
			(m_pBus->busMode() & qtractorBus::Input)) {
			pPluginList = m_pBus->pluginList_in();
		}
		else
		if ((m_busMode & qtractorBus::Output) &&
			(m_pBus->busMode() & qtractorBus::Output)) {
			pPluginList = m_pBus->pluginList_out();
		}
	}

	return (pPluginList ? pPluginList->midiManager() : nullptr);
}


//----------------------------------------------------------------------------
// qtractorMixerRackWidget -- Meter bridge rack widget impl.

// Constructor.
qtractorMixerRackWidget::qtractorMixerRackWidget (
	qtractorMixerRack *pRack ) : QScrollArea(pRack), m_pRack(pRack)
{
	m_pWorkspaceLayout = new QGridLayout();
	m_pWorkspaceLayout->setContentsMargins(0, 0, 0, 0);
	m_pWorkspaceLayout->setSpacing(0);

	m_pWorkspaceWidget = new QWidget(this);
	m_pWorkspaceWidget->setSizePolicy(
		QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
	m_pWorkspaceWidget->setLayout(m_pWorkspaceLayout);

	QScrollArea::viewport()->setBackgroundRole(QPalette::Dark);
//	QScrollArea::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwayOn);
	QScrollArea::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QScrollArea::setWidget(m_pWorkspaceWidget);
}


// Default destructor.
qtractorMixerRackWidget::~qtractorMixerRackWidget (void)
{
	// No need to delete child widgets, Qt does it all for us
}


// Add/remove a mixer strip to/from rack workspace.
void qtractorMixerRackWidget::addStrip ( qtractorMixerStrip *pStrip )
{
	m_pWorkspaceLayout->addWidget(pStrip, 0, m_pWorkspaceLayout->count());
}


void qtractorMixerRackWidget::removeStrip ( qtractorMixerStrip *pStrip )
{
	m_pWorkspaceLayout->removeWidget(pStrip);
}


// Resize event handler.
void qtractorMixerRackWidget::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QScrollArea::resizeEvent(pResizeEvent);

	updateWorkspace();
}


// Context menu request event handler.
void qtractorMixerRackWidget::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	// Maybe it's a track strip
	qtractorBus *pBus = nullptr;
	qtractorMixerStrip *pStrip = m_pRack->stripAt(pContextMenuEvent->pos());
	if (pStrip)
		pBus = pStrip->bus();
	if (pBus == nullptr || pStrip == nullptr) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			pMainForm->stabilizeForm();
			pMainForm->trackMenu()->exec(pContextMenuEvent->globalPos());
		}
		// Bail out...
		return;
	}

	// Build the device context menu...
	QMenu menu(this);
	QAction *pAction;

	pAction = menu.addAction(
		tr("&Inputs"), pStrip, SLOT(busInputsSlot()));
	pAction->setEnabled(pBus->busMode() & qtractorBus::Input);

	pAction = menu.addAction(
		tr("&Outputs"), pStrip, SLOT(busOutputsSlot()));
	pAction->setEnabled(pBus->busMode() & qtractorBus::Output);

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Monitor"), pStrip, SLOT(busMonitorSlot()));
	pAction->setEnabled(
		(pBus->busMode() & qtractorBus::Duplex) == qtractorBus::Duplex);
	pAction->setCheckable(true);
	pAction->setChecked(pBus->isMonitor());

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Buses..."), pStrip, SLOT(busPropertiesSlot()));

	menu.exec(pContextMenuEvent->globalPos());
}


// Mouse click event handler.
void qtractorMixerRackWidget::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (!m_pWorkspaceWidget->rect().contains(pMouseEvent->pos()))
		m_pRack->setSelectedStrip(nullptr);

	QScrollArea::mousePressEvent(pMouseEvent);
}


// Multi-row workspace layout method.
void qtractorMixerRackWidget::updateWorkspace (void)
{
	QWidget *pViewport = QScrollArea::viewport();
	const int h = pViewport->height();
	const int w = pViewport->width();

	const int nitems = m_pWorkspaceLayout->count();
	if (nitems > 0) {
		const int nrows = h / sizeHint().height();
		const int ncols = nitems / (nrows > 0 ? nrows : 1);
		QLayoutItem *items[nitems];
		for (int i = 0; i < nitems; ++i)
			items[i] = m_pWorkspaceLayout->takeAt(0);
		int row = 0;
		int col = 0;
		int wth = 0;
		for (int i = 0; i < nitems; ++i) {
			QLayoutItem *item = items[i];
			m_pWorkspaceLayout->addItem(item, row, col++);
			// Auto-grid layout...
			const int wi = item->sizeHint().width(); wth += wi;
			if (wth > (w - wi) && row < nrows && col > ncols) {
				wth = 0;
				col = 0;
				++row;
			}
		}
	}

//	m_pWorkspaceWidget->setMinimumWidth(w);
	m_pWorkspaceWidget->setFixedHeight(h);
	m_pWorkspaceWidget->adjustSize();
}


//----------------------------------------------------------------------------
// qtractorMixerRackTitleBarWidget -- Mixer strip rack title-bar.

class qtractorMixerRackTitleBarWidget : public QWidget
{
public:

	// Constructor.
	qtractorMixerRackTitleBarWidget (
		qtractorMixerRack *pRack, const QString& sTitle ) : QWidget(pRack)
	{
		const QFont& font = QWidget::font();
		QWidget::setFont(QFont(font.family(), font.pointSize() - 3));

		QHBoxLayout *pHBoxLayout = new QHBoxLayout();
		pHBoxLayout->setSpacing(4);
		pHBoxLayout->setContentsMargins(2, 2, 2, 2);
	#if 0
		QFrame *pLeftFrame = new QFrame();
		pLeftFrame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
		pHBoxLayout->addWidget(pLeftFrame, 1);
	#endif
		pHBoxLayout->addWidget(new QLabel(sTitle));
	#if 0
		QFrame *pRightFrame = new QFrame();
		pRightFrame->setFrameStyle(QFrame::HLine | QFrame::Sunken);
		pHBoxLayout->addWidget(pRightFrame, 1);
	#endif
		QWidget::setLayout(pHBoxLayout);
	}
};


//----------------------------------------------------------------------------
// qtractorMixerRack -- Mixer strip rack.

// Constructor.
qtractorMixerRack::qtractorMixerRack (
	qtractorMixer *pMixer, const QString& sTitle )
	: QDockWidget(sTitle, pMixer), m_pMixer(pMixer),
		m_pSelectedStrip(nullptr), m_pSelectedStrip2(nullptr),
		m_pRackWidget(new qtractorMixerRackWidget(this))
{
	QDockWidget::setObjectName(sTitle);	// TODO: make this an unique-id.
	QDockWidget::setTitleBarWidget(
		new qtractorMixerRackTitleBarWidget(this, sTitle));
	QDockWidget::setToolTip(sTitle);

	QDockWidget::setWidget(m_pRackWidget);

	QDockWidget::setFeatures(
		QDockWidget::DockWidgetClosable |
		QDockWidget::DockWidgetMovable  |
		QDockWidget::DockWidgetFloatable); // FIXME: is floatable necessary?

//	QDockWidget::setAllowedAreas(Qt::AllDockWidgetAreas);
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
	// Add this to the workspace layout...
	m_pRackWidget->addStrip(pStrip);

	qtractorMonitor *pMonitor = pStrip->monitor();
	if (pMonitor)
		m_strips.insert(pMonitor, pStrip);

	pStrip->show();
}


// Remove a mixer strip from rack list.
void qtractorMixerRack::removeStrip ( qtractorMixerStrip *pStrip )
{
	// Don't let current selection hanging...
	if (m_pSelectedStrip == pStrip || m_pSelectedStrip2 == pStrip) {
		m_pSelectedStrip = nullptr;
		m_pSelectedStrip2 = nullptr;
	}

	// Remove this from the workspace layout...
	m_pRackWidget->removeStrip(pStrip);

	pStrip->hide();

	qtractorMonitor *pMonitor = pStrip->monitor();
	if (pMonitor) {
		m_strips.remove(pMonitor);
		delete pStrip;
	}

	m_pRackWidget->workspace()->adjustSize();
}


// Find a mixer strip, given its rack workspace position.
qtractorMixerStrip *qtractorMixerRack::stripAt ( const QPoint& pos ) const
{
	Strips::ConstIterator iter = m_strips.constBegin();
	const Strips::ConstIterator& iter_end = m_strips.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMixerStrip *pStrip = iter.value();
		if (pStrip && pStrip->frameGeometry().contains(pos))
			return pStrip;
	}

	return nullptr;
}


// Find a mixer strip, given its monitor handle.
qtractorMixerStrip *qtractorMixerRack::findStrip ( qtractorMonitor *pMonitor ) const
{
	return m_strips.value(pMonitor, nullptr);
}


// Update a mixer strip on rack list.
void qtractorMixerRack::updateStrip (
	qtractorMixerStrip *pStrip, qtractorMonitor *pMonitor )
{
	qtractorMonitor *pOldMonitor = pStrip->monitor();
	if (pOldMonitor)
		m_strips.remove(pOldMonitor);

	pStrip->setMonitor(pMonitor);

	if (pMonitor)
		m_strips.insert(pMonitor, pStrip);
}


// Complete rack recycle.
void qtractorMixerRack::clear (void)
{
	m_pSelectedStrip = nullptr;
	m_pSelectedStrip2 = nullptr;

	qDeleteAll(m_strips);
	m_strips.clear();
}


// Selection stuff.
void qtractorMixerRack::setSelectedStrip ( qtractorMixerStrip *pStrip )
{
	if (m_pSelectedStrip == pStrip)
		return;

	if (m_pSelectedStrip)
		m_pSelectedStrip->setSelected(false);

	m_pSelectedStrip = pStrip;

	if (m_pSelectedStrip) {
		m_pSelectedStrip->setSelected(true);
		const int wm = (m_pSelectedStrip->width() >> 1);
		m_pRackWidget->ensureVisible(
			m_pSelectedStrip->pos().x() + wm, 0, wm, 0);
	}

	emit selectionChanged();
}


void qtractorMixerRack::setSelectedStrip2 ( qtractorMixerStrip *pStrip )
{
	if (m_pSelectedStrip2 == m_pSelectedStrip)
		return;

	if (m_pSelectedStrip2)
		m_pSelectedStrip2->setSelected(false);

	m_pSelectedStrip2 = pStrip;

	if (m_pSelectedStrip2) {
		m_pSelectedStrip2->setSelected(true);
		const int wm = (m_pSelectedStrip2->width() >> 1);
		m_pRackWidget->ensureVisible(
			m_pSelectedStrip2->pos().x() + wm, 0, wm, 0);
	}
}


// Hacko-list-management marking...
void qtractorMixerRack::markStrips ( int iMark )
{
	m_pRackWidget->workspace()->setUpdatesEnabled(false);

	Strips::ConstIterator strip = m_strips.constBegin();
	const Strips::ConstIterator& strip_end = m_strips.constEnd();
	for ( ; strip != strip_end; ++strip)
		strip.value()->setMark(iMark);
}


void qtractorMixerRack::cleanStrips ( int iMark )
{
	Strips::Iterator strip = m_strips.begin();
	const Strips::Iterator& strip_end = m_strips.end();
	while (strip != strip_end) {
		qtractorMixerStrip *pStrip = strip.value();
		if (pStrip->mark() == iMark) {
			// Remove from the workspace layout...
			m_pRackWidget->removeStrip(pStrip);
			// Don't let current selection hanging...
			if (m_pSelectedStrip == pStrip)
				m_pSelectedStrip = nullptr;
			// Hide strip...
			pStrip->hide();
			// Remove from list...
			strip = m_strips.erase(strip);
			// and finally get rid of it.
			delete pStrip;
		}
		else ++strip;
	}

	m_pRackWidget->updateWorkspace();
	m_pRackWidget->workspace()->setUpdatesEnabled(true);
}


// Find a mixer strip, given its MIDI-manager handle.
qtractorMixerStrip *qtractorMixerRack::findMidiManagerStrip (
	qtractorMidiManager *pMidiManager ) const
{
	if (pMidiManager == nullptr)
		return nullptr;

	Strips::ConstIterator strip = m_strips.constBegin();
	const Strips::ConstIterator& strip_end = m_strips.constEnd();
	for ( ; strip != strip_end; ++strip) {
		qtractorMixerStrip *pStrip = strip.value();
		if (pStrip->midiManager() == pMidiManager)
			return pStrip;
	}

	return nullptr;
}


// Find all the MIDI mixer strip, given an audio output bus handle.
QList<qtractorMixerStrip *> qtractorMixerRack::findAudioOutputBusStrips (
	qtractorAudioBus *pAudioOutputBus ) const
{
	QList<qtractorMixerStrip *> strips;

	if (pAudioOutputBus == nullptr)
		return strips;

	Strips::ConstIterator strip = m_strips.constBegin();
	const Strips::ConstIterator& strip_end = m_strips.constEnd();
	for ( ; strip != strip_end; ++strip) {
		qtractorMixerStrip *pStrip = strip.value();
		qtractorMidiManager *pMidiManager = pStrip->midiManager();
		if (pMidiManager &&
			pMidiManager->audioOutputBus() == pAudioOutputBus) {
			strips.append(pStrip);
		}
	}

	return strips;
}


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

// Constructor.
qtractorMixer::qtractorMixer ( QWidget *pParent, Qt::WindowFlags wflags )
	: QMainWindow(pParent, wflags)
{
	// Surely a name is crucial (e.g. for storing geometry settings)
	QMainWindow::setObjectName("qtractorMixer");

	m_pInputRack  = new qtractorMixerRack(this, tr("Inputs"));
	m_pTrackRack  = new qtractorMixerRack(this, tr("Tracks"));
	m_pOutputRack = new qtractorMixerRack(this, tr("Outputs"));

	// Some specialties to this kind of dock window...
	QMainWindow::setMinimumWidth(480);
	QMainWindow::setMinimumHeight(320);

	// Finally set the default caption and tooltip.
	const QString& sTitle = tr("Mixer");
	QMainWindow::setWindowTitle(sTitle);
	QMainWindow::setWindowIcon(QIcon::fromTheme("qtractorMixer"));
	QMainWindow::setToolTip(sTitle);

	QMainWindow::addDockWidget(Qt::LeftDockWidgetArea,  m_pInputRack);
	QMainWindow::addDockWidget(Qt::RightDockWidgetArea, m_pOutputRack);

	m_pTrackRack->setFeatures(QDockWidget::NoDockWidgetFeatures);
	m_pTrackRack->setAllowedAreas(Qt::NoDockWidgetArea);
	QMainWindow::setCentralWidget(m_pTrackRack);

	// Get previously saved splitter sizes...
	loadMixerState();
}


// Default destructor.
qtractorMixer::~qtractorMixer (void)
{
	// Save splitter sizes...
	if (QMainWindow::isVisible())
		saveMixerState();

	// No need to delete child widgets, Qt does it all for us
}


// Just about to notify main-window that we're closing.
void qtractorMixer::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
	// Save splitter sizes...
	saveMixerState();

	QMainWindow::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Get previously saved dockable state...
void qtractorMixer::loadMixerState (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("Mixer");
		const QByteArray& aDockables
			= pOptions->settings().value("/Layout/DockWindows").toByteArray();
		if (!aDockables.isEmpty())
			QMainWindow::restoreState(aDockables);
		settings.endGroup();
	}
}


// Save dockables state...
void qtractorMixer::saveMixerState (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("Mixer");
		settings.setValue("/Layout/DockWindows", QMainWindow::saveState());
		settings.endGroup();
	}
}


// Update mixer rack, checking if given monitor already exists.
void qtractorMixer::updateBusStrip ( qtractorMixerRack *pRack,
	qtractorBus *pBus, qtractorBus::BusMode busMode, bool bReset )
{
	qtractorMonitor *pMonitor
		= (busMode == qtractorBus::Input ?
			pBus->monitor_in() : pBus->monitor_out());

	qtractorMixerStrip *pStrip = pRack->findStrip(pMonitor);
	if (pStrip == nullptr) {
		pRack->addStrip(new qtractorMixerStrip(pRack, pBus, busMode));
	} else {
		pStrip->setMark(0);
		if (bReset)
			pStrip->setBus(pBus);
	}

	pBus->mapControllers(busMode);
}


void qtractorMixer::updateTrackStrip ( qtractorTrack *pTrack, bool bReset )
{
	qtractorMixerStrip *pStrip = m_pTrackRack->findStrip(pTrack->monitor());
	if (pStrip == nullptr) {
		m_pTrackRack->addStrip(new qtractorMixerStrip(m_pTrackRack, pTrack));
	} else {
		pStrip->setMark(0);
		if (bReset)
			pStrip->setTrack(pTrack);
	}

	pTrack->mapControllers();
	pTrack->applyCurveFile(pTrack->curveFile());

	qtractorCurveList *pCurveList = pTrack->curveList();
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pCurveList && pMainForm && pMainForm->tracks()) {
		pMainForm->tracks()->updateTrackList(pTrack);
		QObject::connect(
			pCurveList->proxy(), SIGNAL(update()),
			pMainForm->tracks(), SLOT(updateTrackView()));
	}
}


// Current selected track accessors.
void qtractorMixer::setCurrentTrack ( qtractorTrack *pTrack )
{
	qtractorMixerStrip *pInputStrip = nullptr;
	qtractorMixerStrip *pTrackStrip = nullptr;
	qtractorMixerStrip *pOutputStrip = nullptr;
	qtractorMixerStrip *pOutputStrip2 = nullptr;

	if (pTrack) {
		qtractorBus *pInputBus = pTrack->inputBus();
		if (pInputBus)
			pInputStrip = m_pInputRack->findStrip(pInputBus->monitor_in());
		pTrackStrip = m_pTrackRack->findStrip(pTrack->monitor());
		qtractorBus *pOutputBus = pTrack->outputBus();
		if (pOutputBus)
			pOutputStrip = m_pOutputRack->findStrip(pOutputBus->monitor_out());
		if (pTrack->trackType() == qtractorTrack::Midi && pTrack->pluginList()) {
			qtractorMidiManager *pMidiManager
				= (pTrack->pluginList())->midiManager();
			if (pMidiManager
				&& pMidiManager->isAudioOutputMonitor()
				&& !pMidiManager->isAudioOutputBus()) {
				qtractorAudioBus *pAudioOutputBus
					= pMidiManager->audioOutputBus();
				if (pAudioOutputBus) {
					pOutputStrip2 = m_pOutputRack->findStrip(
						pAudioOutputBus->monitor_out());
				}
			}
		}
	}

	m_pInputRack->setSelectedStrip(pInputStrip);
	m_pTrackRack->setSelectedStrip(pTrackStrip);
	m_pOutputRack->setSelectedStrip(pOutputStrip);
	m_pOutputRack->setSelectedStrip2(pOutputStrip2);
}


qtractorTrack *qtractorMixer::currentTrack (void) const
{
	qtractorMixerStrip *pTrackStrip = m_pTrackRack->selectedStrip();
	if (pTrackStrip)
		return pTrackStrip->track();
	else
		return nullptr;
}


// Update buses'racks.
void qtractorMixer::updateBuses ( bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTrack *pCurrentTrack = currentTrack();

	if (bReset) {
		m_pInputRack->clear();
		m_pOutputRack->clear();		
	}

	m_pInputRack->markStrips(1);
	m_pOutputRack->markStrips(1);

	// Audio buses first...
	QListIterator audio_iter(pSession->audioEngine()->buses2());
	while (audio_iter.hasNext()) {
		qtractorBus *pBus = audio_iter.next();
		if (pBus == nullptr)
			continue;
		if (pBus->busMode() & qtractorBus::Input)
			updateBusStrip(m_pInputRack, pBus, qtractorBus::Input);
		if (pBus->busMode() & qtractorBus::Output)
			updateBusStrip(m_pOutputRack, pBus, qtractorBus::Output);
	}

	// MIDI buses are next...
	QListIterator midi_iter(pSession->midiEngine()->buses2());
	while (midi_iter.hasNext()) {
		qtractorBus *pBus = midi_iter.next();
		if (pBus == nullptr)
			continue;
		if (pBus->busMode() & qtractorBus::Input)
			updateBusStrip(m_pInputRack, pBus, qtractorBus::Input);
		if (pBus->busMode() & qtractorBus::Output)
			updateBusStrip(m_pOutputRack, pBus, qtractorBus::Output);
	}

	m_pOutputRack->cleanStrips(1);
	m_pInputRack->cleanStrips(1);

	setCurrentTrack(pCurrentTrack);
}


// Update tracks'rack.
void qtractorMixer::updateTracks ( bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorTrack *pCurrentTrack = currentTrack();
	bool bCurrentTrack = false;

	if (bReset)
		m_pTrackRack->clear();

	m_pTrackRack->markStrips(1);

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		updateTrackStrip(pTrack);
		if (pCurrentTrack == pTrack)
			bCurrentTrack = true;
	}

	m_pTrackRack->cleanStrips(1);

	setCurrentTrack(bCurrentTrack ? pCurrentTrack : nullptr);
}


// Update a MIDI mixer strip, given its MIDI manager handle.
void qtractorMixer::updateMidiManagerStrip ( qtractorMidiManager *pMidiManager )
{
	qtractorTrack *pCurrentTrack = currentTrack();

	qtractorMixerStrip *
		pStrip = m_pTrackRack->findMidiManagerStrip(pMidiManager);
	if (pStrip == nullptr)
		pStrip = m_pOutputRack->findMidiManagerStrip(pMidiManager);
	if (pStrip == nullptr)
		pStrip = m_pInputRack->findMidiManagerStrip(pMidiManager);
	if (pStrip)
		pStrip->updateMidiManager(pMidiManager);

	setCurrentTrack(pCurrentTrack);
}


// Find a MIDI mixer strip, given its MIDI manager handle.
QList<qtractorMixerStrip *> qtractorMixer::findAudioOutputBusStrips (
	qtractorAudioBus *pAudioOutputBus ) const
{
	QList<qtractorMixerStrip *> strips;
	strips.append(m_pTrackRack->findAudioOutputBusStrips(pAudioOutputBus));
	strips.append(m_pOutputRack->findAudioOutputBusStrips(pAudioOutputBus));
	strips.append(m_pInputRack->findAudioOutputBusStrips(pAudioOutputBus));
	return strips;
}


// Complete mixer recycle.
void qtractorMixer::clear (void)
{
	m_pInputRack->clear();
	m_pTrackRack->clear();
	m_pOutputRack->clear();
}


// Update mixer automatic multi-row strip/grid layout.
void qtractorMixer::updateWorkspaces (void)
{
	m_pInputRack->updateWorkspace();
	m_pTrackRack->updateWorkspace();
	m_pOutputRack->updateWorkspace();
}


// Keyboard event handler.
void qtractorMixer::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMixer::keyPressEvent(%d)", pKeyEvent->key());
#endif
	const int iKey = pKeyEvent->key();
	switch (iKey) {
	case Qt::Key_Escape:
		close();
		break;
	default:
		QMainWindow::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorMixer.cpp

