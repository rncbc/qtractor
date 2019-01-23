// qtractorMixer.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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


// Common initializer.
void qtractorMonitorButton::initMonitorButton (void)
{
	QIcon icons;
	icons.addPixmap(QPixmap(":/images/itemLedOff.png"),
		QIcon::Normal, QIcon::Off);
	icons.addPixmap(QPixmap(":/images/itemLedOn.png"),
		QIcon::Normal, QIcon::On);
	QPushButton::setIcon(icons);
	QPushButton::setText(' ' + tr("monitor"));

	QObject::connect(this, SIGNAL(toggled(bool)), SLOT(toggledSlot(bool)));
}


// Specific track accessors.
void qtractorMonitorButton::setTrack ( qtractorTrack *pTrack )
{
	m_pTrack = pTrack;
	m_pBus = NULL;

	QPushButton::setToolTip(tr("Monitor (rec)"));

	updateMonitor(); // Visitor setup.
}


// Specific bus accessors.
void qtractorMonitorButton::setBus ( qtractorBus *pBus )
{
	m_pBus = pBus;
	m_pTrack = NULL;
	
	QPushButton::setToolTip(tr("Monitor (thru)"));

	updateMonitor(); // Visitor setup.
}


// Visitors overload.
void qtractorMonitorButton::updateValue ( float fValue )
{
	// Avoid self-triggering...
	const bool bBlockSignals = QPushButton::blockSignals(true);
	QPushButton::setChecked(fValue > 0.0f);
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
	IconLabel(QWidget *pParent = NULL) : QLabel(pParent) {}

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
		painter.drawPixmap(rect.x(), rect.y(), m_icon.pixmap(rect.height()));
		rect.setX(rect.x() + rect.height() + 1);
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
		m_pBus(pBus), m_busMode(busMode), m_pTrack(NULL)
{
	initMixerStrip();
}

qtractorMixerStrip::qtractorMixerStrip (
	qtractorMixerRack *pRack, qtractorTrack *pTrack )
	: QFrame(pRack->workspace()), m_pRack(pRack),
		m_pBus(NULL), m_busMode(qtractorBus::None), m_pTrack(pTrack)
{
	initMixerStrip();
}


// Default destructor.
qtractorMixerStrip::~qtractorMixerStrip (void)
{
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
	m_pLayout->setMargin(4);
	m_pLayout->setSpacing(4);

	m_pLabel = new IconLabel(/*this*/);
//	m_pLabel->setFont(font2);
	m_pLabel->setFixedHeight(iFixedHeight);
	m_pLabel->setBackgroundRole(QPalette::Button);
	m_pLabel->setForegroundRole(QPalette::ButtonText);
	m_pLabel->setAutoFillBackground(true);
	m_pLayout->addWidget(m_pLabel);

	m_pPluginListView = new qtractorPluginListView(/*this*/);
	m_pPluginListView->setFont(font3);
//	m_pPluginListView->setFixedHeight(iFixedHeight << 2);
	m_pPluginListView->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	m_pPluginListView->setTinyScrollBar(true);
	m_pLayout->addWidget(m_pPluginListView, 1);

	const QSizePolicy buttonPolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	m_pButtonLayout = new QHBoxLayout(/*this*/);
	m_pButtonLayout->setMargin(0);
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
		m_pBusButton = NULL;
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
		m_pRecordButton = NULL;
		m_pMuteButton   = NULL;
		m_pSoloButton   = NULL;
	}

	m_pLayout->addWidget(m_pMonitorButton);
	m_pLayout->addLayout(m_pButtonLayout);
	
	// Now, there's whether we are Audio or MIDI related...
	m_pMixerMeter = NULL;
	m_pMidiLabel = NULL;
	int iFixedWidth = 54;
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
			const int iAudioChannels = pAudioMonitor->channels();
			iFixedWidth += 12 * (iAudioChannels < 2	? 2 : iAudioChannels);
			m_pMixerMeter = new qtractorAudioMixerMeter(pAudioMonitor, this);
		}
		m_pPluginListView->setEnabled(true);
		break;
	}
	case qtractorTrack::Midi: {
		// Type cast for proper MIDI monitor...
		qtractorMidiMonitor *pMidiMonitor = NULL;
		qtractorMidiBus *pMidiBus = NULL;
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
			qtractorMidiManager *pMidiManager = NULL;
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
			const int iFixedWidth = 54
				+ 12 * (iAudioChannels < 2 ? 2 : iAudioChannels);
			if (iFixedWidth != iOldWidth) {
				QFrame::setFixedWidth(iFixedWidth);
				m_pRack->updateWorkspace();
			}
		}
	}

	if (m_pMixerMeter)
		m_pMixerMeter->setMonitor(pMonitor);
}

qtractorMonitor *qtractorMixerStrip::monitor (void) const
{
	return (m_pMixerMeter ? m_pMixerMeter->monitor() : NULL);
}


// Common mixer-strip caption title updater.
void qtractorMixerStrip::updateName (void)
{
	QPixmap icon;
	QString sName;
	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		sName = m_pTrack->trackName();
		QPalette pal(m_pLabel->palette());
		pal.setColor(QPalette::Button, m_pTrack->foreground().lighter());
		pal.setColor(QPalette::ButtonText, m_pTrack->background().lighter());
		m_pLabel->setPalette(pal);
		m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		if (icon.load(m_pTrack->trackIcon()))
			icon = icon.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	} else if (m_pBus) {
		meterType = m_pBus->busType();
		if (m_busMode & qtractorBus::Input) {
			sName = tr("%1 In").arg(m_pBus->busName());
		} else {
			sName = tr("%1 Out").arg(m_pBus->busName());
		}
		m_pLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	}

	QString sType;
	switch (meterType) {
	case qtractorTrack::Audio:
		if (icon.isNull())
			icon.load(":/images/trackAudio.png");
		sType = tr("(Audio)");
		break;
	case qtractorTrack::Midi:
		if (icon.isNull())
			icon.load(":/images/trackMidi.png");
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

	QFrame::setToolTip(sName + ' ' + sType);
}


// MIDI (channel) label updater.
void qtractorMixerStrip::updateMidiLabel (void)
{
	if (m_pTrack == NULL)
		return;

	if (m_pMidiLabel == NULL)
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
	m_pPluginListView->setPluginList(NULL);

	m_pRack->updateStrip(this, NULL);
}


// Bus property accessors.
void qtractorMixerStrip::setBus ( qtractorBus *pBus )
{
	// Must be actual bus...
	if (m_pBus == NULL || pBus == NULL)
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
	if (m_pTrack == NULL || pTrack == NULL)
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
#ifdef CONFIG_GRADIENT
	const QSize& hint = QFrame::sizeHint();
	QLinearGradient grad(0, 0, hint.width() >> 1, hint.height());
	if (m_bSelected) {
		const QColor& rgbBase = pal.midlight().color();
		pal.setColor(QPalette::WindowText, pal.highlightedText().color());
		pal.setColor(QPalette::Window, rgbBase.darker(150));
		grad.setColorAt(0.4, rgbBase.darker(150));
		grad.setColorAt(0.6, rgbBase.darker(130));
		grad.setColorAt(1.0, rgbBase.darker());
	} else {
		const QColor& rgbBase = pal.window().color();
		pal.setColor(QPalette::WindowText, pal.windowText().color());
		pal.setColor(QPalette::Window, rgbBase);
		grad.setColorAt(0.4, rgbBase);
		grad.setColorAt(0.6, rgbBase.lighter(105));
		grad.setColorAt(1.0, rgbBase.darker(130));
	}
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
	pal.setBrush(QPalette::Window, grad);
#else
	if (m_bSelected) {
		const QColor& rgbBase = pal.midlight().color();
		pal.setColor(QPalette::WindowText, rgbBase.lighter());
		pal.setColor(QPalette::Window, rgbBase.darker(150));
	} else {
		const QColor& rgbBase = pal.button().color();
	//	pal.setColor(QPalette::WindowText, rgbBase.darker());
		pal.setColor(QPalette::Window, rgbBase);
	}
#endif
	QFrame::setPalette(pal);
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

	m_pRack->setSelectedStrip(this);
}


// Mouse selection event handlers.
void qtractorMixerStrip::mouseDoubleClickEvent ( QMouseEvent */*pMouseEvent*/ )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	if (m_pTrack) {
		pMainForm->trackProperties();
	} else {
		m_pRack->busPropertiesSlot();
	}
}


// Bus connections dispatcher.
void qtractorMixerStrip::busConnections ( qtractorBus::BusMode busMode )
{
	if (m_pBus == NULL)
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Here we go...
	pMainForm->connections()->showBus(m_pBus, busMode);
}


// Bus pass-through dispatcher.
void qtractorMixerStrip::busMonitor ( bool bMonitor )
{
	if (m_pBus == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Here we go...
	pSession->execute(
		new qtractorBusMonitorCommand(m_pBus, bMonitor));
}


// Track monitor dispatcher.
void qtractorMixerStrip::trackMonitor ( bool bMonitor )
{
	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Here we go...
	pSession->execute(
		new qtractorTrackMonitorCommand(m_pTrack, bMonitor));
}


// Bus connections button slot
void qtractorMixerStrip::busButtonSlot (void)
{
	busConnections(m_busMode);
}


// Pan-meter slider value change slot.
void qtractorMixerStrip::panningChangedSlot ( float fPanning )
{
	if (m_pMixerMeter == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMixerStrip[%p]::panningChangedSlot(%g)", this, fPanning);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
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
	if (m_pMixerMeter == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMixerStrip[%p]::gainChangedSlot(%g)", this, fGain);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
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
	if (pMidiMixerMeter == NULL)
		return;

	// Apply the combo-meter posssibility...
	if (pMidiManager && pMidiManager->isAudioOutputMonitor()) {
		pMidiMixerMeter->setAudioOutputMonitor(
			pMidiManager->audioOutputMonitor());
	} else {
		pMidiMixerMeter->setAudioOutputMonitor(NULL);
	}
}


// Retrieve the MIDI manager from a mixer strip, if any....
qtractorMidiManager *qtractorMixerStrip::midiManager (void) const
{
	qtractorPluginList *pPluginList = NULL;

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

	return (pPluginList ? pPluginList->midiManager() : NULL);
}


//----------------------------------------------------------------------------
// qtractorMixerRackWidget -- Meter bridge rack widget impl.

// Constructor.
qtractorMixerRackWidget::qtractorMixerRackWidget (
	qtractorMixerRack *pRack ) : QScrollArea(pRack), m_pRack(pRack)
{
	m_pWorkspaceLayout = new QGridLayout();
	m_pWorkspaceLayout->setMargin(0);
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
	qtractorBus *pBus = NULL;
	qtractorMixerStrip *pSelectedStrip = m_pRack->selectedStrip();
	if (pSelectedStrip)
		pBus = pSelectedStrip->bus();
	if (pBus == NULL) {
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
		tr("&Inputs"), m_pRack, SLOT(busInputsSlot()));
	pAction->setEnabled(pBus->busMode() & qtractorBus::Input);

	pAction = menu.addAction(
		tr("&Outputs"), m_pRack, SLOT(busOutputsSlot()));
	pAction->setEnabled(pBus->busMode() & qtractorBus::Output);

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Monitor"), m_pRack, SLOT(busMonitorSlot()));
	pAction->setEnabled(
		(pBus->busMode() & qtractorBus::Duplex) == qtractorBus::Duplex);
	pAction->setCheckable(true);
	pAction->setChecked(pBus->isMonitor());

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Buses..."), m_pRack, SLOT(busPropertiesSlot()));

	menu.exec(pContextMenuEvent->globalPos());
}


// Mouse click event handler.
void qtractorMixerRackWidget::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (!m_pWorkspaceWidget->rect().contains(pMouseEvent->pos()))
		m_pRack->setSelectedStrip(NULL);

	QScrollArea::mousePressEvent(pMouseEvent);
}


// Multi-row workspace layout method.
void qtractorMixerRackWidget::updateWorkspace (void)
{
	QWidget *pViewport = QScrollArea::viewport();
	const int h = pViewport->height();
	const int w = pViewport->width();

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	const bool bAutoGridLayout = (pOptions && pOptions->bMixerAutoGridLayout);
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
			if (bAutoGridLayout) {
				const int wi = item->sizeHint().width(); wth += wi;
				if (wth > (w - wi) && row < nrows && col >= ncols) {
					wth = 0;
					col = 0;
					++row;
				}
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
		pHBoxLayout->setMargin(2);
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
		m_bSelectEnabled(false), m_pSelectedStrip(NULL),
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

	QDockWidget::setAllowedAreas(Qt::AllDockWidgetAreas);
}


// Default destructor.
qtractorMixerRack::~qtractorMixerRack (void)
{
	// No need to delete child widgets, Qt does it all for us
	clear();
}


// The mixer strip workspace methods.
void qtractorMixerRack::ensureVisible ( int x, int y, int xm, int ym )
{
	m_pRackWidget->ensureVisible(x, y, xm, ym);
}


// Add a mixer strip to rack list.
void qtractorMixerRack::addStrip ( qtractorMixerStrip *pStrip )
{
	// Add this to the workspace layout...
	m_pRackWidget->addStrip(pStrip);

	m_strips.insert(pStrip->meter()->monitor(), pStrip);

	pStrip->show();
}


// Remove a mixer strip from rack list.
void qtractorMixerRack::removeStrip ( qtractorMixerStrip *pStrip )
{
	// Don't let current selection hanging...
	if (m_pSelectedStrip == pStrip)
		m_pSelectedStrip = NULL;

	// Remove this from the workspace layout...
	m_pRackWidget->removeStrip(pStrip);

	pStrip->hide();

	qtractorMonitor *pMonitor = pStrip->meter()->monitor();
	if (findStrip(pMonitor) == pStrip) {
		m_strips.remove(pMonitor);
		delete pStrip;
	}

	m_pRackWidget->workspace()->adjustSize();
}


// Find a mixer strip, given its monitor handle.
qtractorMixerStrip *qtractorMixerRack::findStrip ( qtractorMonitor *pMonitor ) const
{
	return m_strips.value(pMonitor, NULL);
}


// Update a mixer strip on rack list.
void qtractorMixerRack::updateStrip (
	qtractorMixerStrip *pStrip, qtractorMonitor *pMonitor )
{
	qtractorMonitor *pOldMonitor = pStrip->meter()->monitor();
	if (findStrip(pOldMonitor) == pStrip)
		m_strips.remove(pOldMonitor);
	
	pStrip->setMonitor(pMonitor);

	if (pMonitor)
		m_strips.insert(pMonitor, pStrip);
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

	if (m_pSelectedStrip) {
		if (!m_bSelectEnabled)
			m_pSelectedStrip->setSelected(false);
		m_pSelectedStrip = NULL;
	}
}


void qtractorMixerRack::setSelectedStrip ( qtractorMixerStrip *pStrip )
{
	if (m_pSelectedStrip != pStrip) {
		if (m_bSelectEnabled && m_pSelectedStrip)
			m_pSelectedStrip->setSelected(false);
		m_pSelectedStrip = pStrip;
		if (m_bSelectEnabled && m_pSelectedStrip)
			m_pSelectedStrip->setSelected(true);
		emit selectionChanged();
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
				m_pSelectedStrip = NULL;
			// Hide strip...
			pStrip->hide();
			// Remove from list...
			strip = m_strips.erase(strip);
			// and finally get rid of it.
			delete pStrip;
		}
		else ++strip;
	}

	m_pRackWidget->workspace()->adjustSize();
	m_pRackWidget->workspace()->setUpdatesEnabled(true);
}


// Show/edit bus input connections.
void qtractorMixerRack::busInputsSlot (void)
{
	qtractorMixerStrip *pStrip = m_pSelectedStrip;
	if (pStrip)
		pStrip->busConnections(qtractorBus::Input);
}


// Show/edit bus output connections.
void qtractorMixerRack::busOutputsSlot (void)
{
	qtractorMixerStrip *pStrip = m_pSelectedStrip;
	if (pStrip)
		pStrip->busConnections(qtractorBus::Output);
}


// Toggle bus passthru flag.
void qtractorMixerRack::busMonitorSlot (void)
{
	qtractorMixerStrip *pStrip = m_pSelectedStrip;
	if (pStrip && pStrip->bus())
		pStrip->busMonitor(!(pStrip->bus())->isMonitor());
}


// Show/edit bus properties form.
void qtractorMixerRack::busPropertiesSlot (void)
{
	qtractorMixerStrip *pStrip = m_pSelectedStrip;
	if (pStrip && pStrip->bus()) {
		qtractorBusForm busForm(this);
		busForm.setBus(pStrip->bus());
		busForm.exec();
	}
}


// Find a mixer strip, given its MIDI-manager handle.
qtractorMixerStrip *qtractorMixerRack::findMidiManagerStrip (
	qtractorMidiManager *pMidiManager ) const
{
	if (pMidiManager == NULL)
		return NULL;

	Strips::ConstIterator strip = m_strips.constBegin();
	const Strips::ConstIterator& strip_end = m_strips.constEnd();
	for ( ; strip != strip_end; ++strip) {
		qtractorMixerStrip *pStrip = strip.value();
		if (pStrip->midiManager() == pMidiManager)
			return pStrip;
	}

	return NULL;
}


// Find all the MIDI mixer strip, given an audio output bus handle.
QList<qtractorMixerStrip *> qtractorMixerRack::findAudioOutputBusStrips (
	qtractorAudioBus *pAudioOutputBus ) const
{
	QList<qtractorMixerStrip *> strips;

	if (pAudioOutputBus == NULL)
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
	m_pTrackRack->setSelectEnabled(true);
	m_pOutputRack = new qtractorMixerRack(this, tr("Outputs"));

	// Some specialties to this kind of dock window...
	QMainWindow::setMinimumWidth(480);
	QMainWindow::setMinimumHeight(320);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Mixer") + " - " QTRACTOR_TITLE;
	QMainWindow::setWindowTitle(sCaption);
	QMainWindow::setWindowIcon(QIcon(":/images/viewMixer.png"));
	QMainWindow::setToolTip(sCaption);

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


// Notify the main application widget that we're emerging.
void qtractorMixer::showEvent ( QShowEvent *pShowEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();

	QMainWindow::showEvent(pShowEvent);
}

// Notify the main application widget that we're closing.
void qtractorMixer::hideEvent ( QHideEvent *pHideEvent )
{
	QMainWindow::hideEvent(pHideEvent);
	
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
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
	if (pStrip == NULL) {
		pRack->addStrip(new qtractorMixerStrip(pRack, pBus, busMode));
	} else {
		pStrip->setMark(0);
		if (bReset)
			pStrip->setBus(pBus);
	}

	pBus->mapControllers(busMode);
#if 0
	qtractorAudioBus *pAudioBus;
	qtractorMidiBus  *pMidiBus;

	switch (pBus->busType()) {
	case qtractorTrack::Audio:
		pAudioBus = static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			pAudioBus->applyCurveFile(busMode,
				(busMode == qtractorBus::Input
					? pAudioBus->curveFile_in()
					: pAudioBus->curveFile_out()));
		}
		break;
	case qtractorTrack::Midi:
		pMidiBus = static_cast<qtractorMidiBus *> (pBus);
		if (pMidiBus) {
			pMidiBus->applyCurveFile(busMode,
				(busMode == qtractorBus::Input
					? pMidiBus->curveFile_in()
					: pMidiBus->curveFile_out()));
		}
		break;
	default:
		break;
	}
#endif
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


// Update buses'racks.
void qtractorMixer::updateBuses ( bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (bReset) {
		m_pInputRack->clear();
		m_pOutputRack->clear();		
	}

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
void qtractorMixer::updateTracks ( bool bReset )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (bReset)
		m_pTrackRack->clear();

	m_pTrackRack->markStrips(1);

	for (qtractorTrack *pTrack = pSession->tracks().first();
			pTrack; pTrack = pTrack->next()) {
		updateTrackStrip(pTrack);
	}

	m_pTrackRack->cleanStrips(1);
}


// Update a MIDI mixer strip, given its MIDI manager handle.
void qtractorMixer::updateMidiManagerStrip ( qtractorMidiManager *pMidiManager )
{
	qtractorMixerStrip *
		pStrip = m_pTrackRack->findMidiManagerStrip(pMidiManager);
	if (pStrip == NULL)
		pStrip = m_pOutputRack->findMidiManagerStrip(pMidiManager);
	if (pStrip == NULL)
		pStrip = m_pInputRack->findMidiManagerStrip(pMidiManager);
	if (pStrip)
		pStrip->updateMidiManager(pMidiManager);
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

