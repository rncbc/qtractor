// qtractorMixer.cpp
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

#include "qtractorAbout.h"
#include "qtractorMixer.h"

#include "qtractorPluginListView.h"

#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"
#include "qtractorAudioMonitor.h"
#include "qtractorMidiMonitor.h"

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

#include <QSplitter>
#include <QLabel>

#include <QHBoxLayout>
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
	QPushButton::setText(tr("monitor"));

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

qtractorTrack *qtractorMonitorButton::track (void) const
{
	return m_pTrack;
}


// Specific bus accessors.
void qtractorMonitorButton::setBus ( qtractorBus *pBus )
{
	m_pBus = pBus;
	m_pTrack = NULL;
	
	QPushButton::setToolTip(tr("Monitor (thru)"));

	updateMonitor(); // Visitor setup.
}

qtractorBus *qtractorMonitorButton::bus (void) const
{
	return m_pBus;
}


// Visitors overload.
void qtractorMonitorButton::updateValue ( float fValue )
{
	// Avoid self-triggering...
	bool bBlockSignals = QPushButton::blockSignals(true);
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

	if (m_pMeter)
		delete m_pMeter;

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
	const int iFixedHeight = QFontMetrics(font2).lineSpacing() + 4;

	m_pLayout = new QVBoxLayout(this);
	m_pLayout->setMargin(4);
	m_pLayout->setSpacing(4);

	m_pLabel = new IconLabel(/*this*/);
	m_pLabel->setFont(font2);
	m_pLabel->setFixedHeight(iFixedHeight);
	m_pLabel->setBackgroundRole(QPalette::Button);
	m_pLabel->setForegroundRole(QPalette::ButtonText);
	m_pLabel->setAutoFillBackground(true);
	m_pLayout->addWidget(m_pLabel);

	m_pPluginListView = new qtractorPluginListView(/*this*/);
	m_pPluginListView->setFont(font2);
	m_pPluginListView->setFixedHeight(iFixedHeight << 2);
	m_pPluginListView->setTinyScrollBar(true);
	m_pLayout->addWidget(m_pPluginListView);

	const QSizePolicy buttonPolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	m_pButtonLayout = new QHBoxLayout(/*this*/);
	m_pButtonLayout->setMargin(0);
	m_pButtonLayout->setSpacing(2);

	qtractorTrack::TrackType meterType = qtractorTrack::None;
	if (m_pTrack) {
		meterType = m_pTrack->trackType();
		m_pMonitorButton = new qtractorMonitorButton(m_pTrack);
		m_pMonitorButton->setFixedHeight(iFixedHeight);
		m_pMonitorButton->setSizePolicy(buttonPolicy);
		m_pMonitorButton->setFont(font2);
		const QFont font3(font.family(), font.pointSize() - 3);
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
		m_pMonitorButton->setFont(font2);
		m_pBusButton = new QPushButton(/*this*/);
		m_pBusButton->setFixedHeight(iFixedHeight);
		m_pBusButton->setFocusPolicy(Qt::NoFocus);
		m_pBusButton->setSizePolicy(buttonPolicy);
	//	m_pBusButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		m_pBusButton->setFont(font2);
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
	m_pMeter = NULL;
	m_pMidiLabel = NULL;
	int iFixedWidth = 42;
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
			iFixedWidth += 32;
			m_pMeter = new qtractorMidiMeter(pMidiMonitor, this);
			// MIDI Tracks might need to show something,
			// like proper MIDI channel settings...
			if (m_pTrack) {
				m_pMidiLabel = new QLabel(/*m_pMeter->topWidget()*/);
				m_pMidiLabel->setFont(font2);
				m_pMidiLabel->setAlignment(
					Qt::AlignHCenter | Qt::AlignVCenter);
				m_pMeter->topLayout()->insertWidget(1, m_pMidiLabel);
				updateMidiLabel();
			}
			// No panning on MIDI bus monitors and on duplex ones
			// only on the output buses should be enabled...
			if (pMidiBus) {
				if ((m_busMode & qtractorBus::Input) &&
					(m_pBus->busMode() & qtractorBus::Output)) {
					m_pMeter->panSlider()->setEnabled(false);
					m_pMeter->panSpinBox()->setEnabled(false);
					m_pMeter->gainSlider()->setEnabled(false);
					m_pMeter->gainSpinBox()->setEnabled(false);
				}
			}
		}
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}

	// Eventually the right one...
	if (m_pMeter) {
		// Set MIDI controller & automation hooks...
		qtractorMidiControlObserver *pMidiObserver;
		pMidiObserver = m_pMeter->panningObserver();
		if (m_pTrack)
			pMidiObserver->setCurveList(m_pTrack->curveList());
		m_pMeter->addMidiControlAction(m_pMeter->panSlider(), pMidiObserver);
		pMidiObserver = m_pMeter->gainObserver();
		if (m_pTrack)
			pMidiObserver->setCurveList(m_pTrack->curveList());
		m_pMeter->addMidiControlAction(m_pMeter->gainSlider(), pMidiObserver);
		// Finally, add to layout...	
		m_pLayout->addWidget(m_pMeter);
		QObject::connect(m_pMeter->panSlider(),
			SIGNAL(valueChanged(float)),
			SLOT(panningChangedSlot(float)));
		QObject::connect(m_pMeter->panSpinBox(),
			SIGNAL(valueChanged(float)),
			SLOT(panningChangedSlot(float)));
		QObject::connect(m_pMeter->gainSlider(),
			SIGNAL(valueChanged(float)),
			SLOT(gainChangedSlot(float)));
		QObject::connect(m_pMeter->gainSpinBox(),
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
		pal.setColor(QPalette::Button, m_pTrack->foreground().lighter());
		pal.setColor(QPalette::ButtonText, m_pTrack->background().lighter());
		m_pLabel->setPalette(pal);
		m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
		m_pLabel->setIcon(QIcon(":/images/trackAudio.png"));
		sType = tr("(Audio)");
		break;
	case qtractorTrack::Midi:
		m_pLabel->setIcon(QIcon(":/images/trackMidi.png"));
		sType = tr("(MIDI)");
		break;
	case qtractorTrack::None:
	default:
		sType = tr("(None)");
		break;
	}

	m_pLabel->setText(sName);

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


// Child accessors.
qtractorPluginListView *qtractorMixerStrip::pluginListView (void) const
{
	return m_pPluginListView;
}

qtractorMeter *qtractorMixerStrip::meter (void) const
{
	return m_pMeter;
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
	} else {
		m_pRack->updateStrip(this, m_pBus->monitor_out());
	}

	switch (m_pBus->busType()) {
	case qtractorTrack::Audio: {
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
		break;
	}
	case qtractorTrack::Midi: {
		qtractorMidiBus *pMidiBus
			= static_cast<qtractorMidiBus *> (m_pBus);
		if (pMidiBus) {
			if (m_busMode & qtractorBus::Input) {
				m_pPluginListView->setPluginList(pMidiBus->pluginList_in());
			} else {
				m_pPluginListView->setPluginList(pMidiBus->pluginList_out());
			}
		}
		m_pPluginListView->setEnabled(true);
		break;
	}
	case qtractorTrack::None:
	default:
		break;
	}
	
	m_pMonitorButton->setBus(m_pBus);

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

qtractorTrack *qtractorMixerStrip::track (void) const
{
	return m_pTrack;
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
		pal.setColor(QPalette::Window, rgbBase.darker(160));
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
	m_pMeter->setPalette(pal);
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

bool qtractorMixerStrip::isSelected (void) const
{
	return m_bSelected;
}


// Strip refreshment.
void qtractorMixerStrip::refresh (void)
{
	if (m_pMeter) m_pMeter->refresh();
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
	if (m_pMeter == NULL)
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
	if (m_pMeter == NULL)
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

	m_strips.insert(pStrip->meter()->monitor(), pStrip);

	pStrip->show();
}


// Remove a mixer strip from rack list.
void qtractorMixerRack::removeStrip ( qtractorMixerStrip *pStrip )
{
	// Remove this from the workspace layout...
	m_pWorkspaceLayout->removeWidget(pStrip);

	// Don't let current selection hanging...
	if (m_pSelectedStrip == pStrip)
		m_pSelectedStrip = NULL;

	pStrip->hide();

	qtractorMonitor *pMonitor = pStrip->meter()->monitor();
	if (findStrip(pMonitor) == pStrip) {
		m_strips.remove(pMonitor);
		delete pStrip;
	}

	m_pWorkspace->adjustSize();
}


// Find a mixer strip, given its monitor handle.
qtractorMixerStrip *qtractorMixerRack::findStrip ( qtractorMonitor *pMonitor )
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
	Strips::ConstIterator strip = m_strips.constBegin();
	const Strips::ConstIterator& strip_end = m_strips.constEnd();
	for ( ; strip != strip_end; ++strip)
		strip.value()->refresh();
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

bool qtractorMixerRack::isSelectEnabled (void) const
{
	return m_bSelectEnabled;
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

qtractorMixerStrip *qtractorMixerRack::selectedStrip (void) const
{
	return m_pSelectedStrip;
}



// Hacko-list-management marking...
void qtractorMixerRack::markStrips ( int iMark )
{
	m_pWorkspace->setUpdatesEnabled(false);

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
			m_pWorkspaceLayout->removeWidget(pStrip);
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
	// Maybe it's a track strip
	qtractorBus *pBus = NULL;
	if (m_pSelectedStrip)
		pBus = m_pSelectedStrip->bus();
	if (pBus == NULL) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->trackMenu()->exec(pContextMenuEvent->globalPos());
		// Bail out...
		return;
	}

	// Build the device context menu...
	QMenu menu(this);
	QAction *pAction;
	
	pAction = menu.addAction(
		tr("&Inputs"), this, SLOT(busInputsSlot()));
	pAction->setEnabled(pBus->busMode() & qtractorBus::Input);

	pAction = menu.addAction(
		tr("&Outputs"), this, SLOT(busOutputsSlot()));
	pAction->setEnabled(pBus->busMode() & qtractorBus::Output);

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Monitor"), this, SLOT(busMonitorSlot()));
	pAction->setEnabled(
		(pBus->busMode() & qtractorBus::Duplex) == qtractorBus::Duplex);
	pAction->setCheckable(true);
	pAction->setChecked(pBus->isMonitor());

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Buses..."), this, SLOT(busPropertiesSlot()));

	menu.exec(pContextMenuEvent->globalPos());
}


// Mouse click event handler.
void qtractorMixerRack::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (!m_pWorkspace->rect().contains(pMouseEvent->pos()))
		setSelectedStrip(NULL);

	QScrollArea::mousePressEvent(pMouseEvent);
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


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

// Constructor.
qtractorMixer::qtractorMixer ( QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags)
{
	// Surely a name is crucial (e.g. for storing geometry settings)
	QWidget::setObjectName("qtractorMixer");

	m_pSplitter = new QSplitter(Qt::Horizontal, this);
	m_pSplitter->setObjectName("MixerSplitter");
//	m_pSplitter->setChildrenCollapsible(false);
//	m_pSplitter->setOpaqueResize(false);
	m_pSplitter->setHandleWidth(2);

	m_pInputRack  = new qtractorMixerRack(this, tr("Inputs"));
	m_pTrackRack  = new qtractorMixerRack(this, tr("Tracks"));
	m_pTrackRack->setSelectEnabled(true);
	m_pOutputRack = new qtractorMixerRack(this, tr("Outputs"));
#if 0
	m_pSplitter->setStretchFactor(m_pSplitter->indexOf(m_pInputRack), 0);
	m_pSplitter->setStretchFactor(m_pSplitter->indexOf(m_pTrackRack), 1);
	m_pSplitter->setStretchFactor(m_pSplitter->indexOf(m_pOutputRack), 0);
#else
	QSizePolicy policy;
	policy = m_pTrackRack->sizePolicy();
	policy.setHorizontalStretch(1);
	m_pTrackRack->setSizePolicy(policy);
#endif

	// Prepare the layout stuff.
	QHBoxLayout *pLayout = new QHBoxLayout();
	pLayout->setMargin(0);
	pLayout->setSpacing(0);
	pLayout->addWidget(m_pSplitter);
	QWidget::setLayout(pLayout);

	// Some specialties to this kind of dock window...
	QWidget::setMinimumWidth(440);
	QWidget::setMinimumHeight(240);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Mixer") + " - " QTRACTOR_TITLE;
	QWidget::setWindowTitle(sCaption);
	QWidget::setWindowIcon(QIcon(":/images/viewMixer.png"));
	QWidget::setToolTip(sCaption);

	// Get previously saved splitter sizes...
	loadSplitterSizes();
}


// Default destructor.
qtractorMixer::~qtractorMixer (void)
{
	// Save splitter sizes...
	if (m_pSplitter->isVisible())
		saveSplitterSizes();

	// No need to delete child widgets, Qt does it all for us
}


// Notify the main application widget that we're emerging.
void qtractorMixer::showEvent ( QShowEvent *pShowEvent )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();

	QWidget::showEvent(pShowEvent);
}

// Notify the main application widget that we're closing.
void qtractorMixer::hideEvent ( QHideEvent *pHideEvent )
{
	QWidget::hideEvent(pHideEvent);
	
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Just about to notify main-window that we're closing.
void qtractorMixer::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
	// Save splitter sizes...
	saveSplitterSizes();

	QWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// The splitter layout widget accessor.
QSplitter *qtractorMixer::splitter (void) const
{
	return m_pSplitter;
}


// Get previously saved splitter sizes...
// (with some fair default...)
void qtractorMixer::loadSplitterSizes (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<int> sizes;
		sizes.append(140);
		sizes.append(160);
		sizes.append(140);
		pOptions->loadSplitterSizes(m_pSplitter, sizes);
	}
}


// Save splitter sizes...
void qtractorMixer::saveSplitterSizes (void)
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->saveSplitterSizes(m_pSplitter);
}


// Initial minimum widget extents.
QSize qtractorMixer::sizeHint (void) const
{
	return QSize(480, 320);
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
		pMainForm->tracks()->updateTrackList();
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

	// Autonomic resizing of bus splitter sizes...
	int ws, iUpdate = 0;
	QList<int> sizes = m_pSplitter->sizes();
	int& w0 = sizes[0];
	int& w1 = sizes[1];
	int& w2 = sizes[2];
	ws = m_pInputRack->workspace()->width() + 4;
	if (w0 > ws) {
		w1 += (w0 - ws);
		w0  = ws;
		++iUpdate;
	}
	ws = m_pOutputRack->workspace()->width() + 4;
	if (w2 > ws) {
		w1 += (w2 - ws);
		w2  = ws;
		++iUpdate;
	}
	if (iUpdate > 0)
		m_pSplitter->setSizes(sizes);
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


// Keyboard event handler.
void qtractorMixer::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMixer::keyPressEvent(%d)", pKeyEvent->key());
#endif
	int iKey = pKeyEvent->key();
	switch (iKey) {
	case Qt::Key_Escape:
		close();
		break;
	default:
		QWidget::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorMixer.cpp
