// qtractorAudioEngine.cpp
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioEngine.h"
#include "qtractorAudioBuffer.h"

#include "qtractorSessionCursor.h"
#include "qtractorSessionDocument.h"
#include "qtractorMidiEngine.h"

#include "qtractorClip.h"

#include <qapplication.h>


//----------------------------------------------------------------------
// qtractorAudioEngine_process -- JACK client process callback.
//

static int qtractorAudioEngine_process ( jack_nframes_t nframes, void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	return pAudioEngine->process(nframes);
}


//----------------------------------------------------------------------
// qtractorAudioEngine_shutdown -- JACK client shutdown callback.
//

static void qtractorAudioEngine_shutdown ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QCustomEvent(pAudioEngine->notifyShutdownType(), pAudioEngine));
	}
}


//----------------------------------------------------------------------
// qtractorAudioEngine_xrun -- JACK client XRUN callback.
//

static int qtractorAudioEngine_xrun ( void *pvArg )
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (pvArg);

	if (pAudioEngine->notifyWidget()) {
		QApplication::postEvent(pAudioEngine->notifyWidget(),
			new QCustomEvent(pAudioEngine->notifyXrunType(), pAudioEngine));
	}

	return 0;
}


//----------------------------------------------------------------------
// class qtractorAudioEngine -- JACK client instance (singleton).
//

// Constructor.
qtractorAudioEngine::qtractorAudioEngine ( qtractorSession *pSession )
	: qtractorEngine(pSession, qtractorTrack::Audio)
{
	m_pJackClient = NULL;

	m_pNotifyWidget       = NULL;
	m_eNotifyShutdownType = QEvent::None;
	m_eNotifyXrunType     = QEvent::None;
}


// JACK client descriptor accessor.
jack_client_t *qtractorAudioEngine::jackClient (void) const
{
	return m_pJackClient;
}


// Event notifier widget settings.
void qtractorAudioEngine::setNotifyWidget ( QWidget *pNotifyWidget )
{
	m_pNotifyWidget = pNotifyWidget;
}

void qtractorAudioEngine::setNotifyShutdownType ( QEvent::Type eNotifyShutdownType )
{
	m_eNotifyShutdownType = eNotifyShutdownType;
}

void qtractorAudioEngine::setNotifyXrunType ( QEvent::Type eNotifyXrunType )
{
	m_eNotifyXrunType = eNotifyXrunType;
}


QWidget *qtractorAudioEngine::notifyWidget (void) const
{
	return m_pNotifyWidget;
}

QEvent::Type qtractorAudioEngine::notifyShutdownType (void) const
{
	return m_eNotifyShutdownType;
}

QEvent::Type qtractorAudioEngine::notifyXrunType (void) const
{
	return m_eNotifyXrunType;
}


// Device engine initialization method.
bool qtractorAudioEngine::init ( const QString& sClientName )
{
	// Try open a new client...
	m_pJackClient = jack_client_new(sClientName.latin1());
	if (m_pJackClient == NULL)
		return false;

	// ATTN: First thing to remember to set session sample rate.
	session()->setSampleRate(jack_get_sample_rate(m_pJackClient));

	return true;
}


// Device engine activation method.
bool qtractorAudioEngine::activate (void)
{
	// Set our main engine processor callbacks.
	jack_set_process_callback(m_pJackClient,
			qtractorAudioEngine_process, this);

	// And some other event callbacks...
    jack_set_xrun_callback(m_pJackClient, qtractorAudioEngine_xrun, this);
    jack_on_shutdown(m_pJackClient, qtractorAudioEngine_shutdown, this);

	// Time to activate ourselves...
	jack_activate(m_pJackClient);

	// Now, do all auto-connection stuff (if applicable...)
	for (qtractorBus *pBus = busses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *>(pBus);
		if (pAudioBus)
			pAudioBus->autoConnect();
	}

	// We're now ready and running...
	return true;
}


// Device engine start method.
bool qtractorAudioEngine::start (void)
{
	if (!isActivated())
	    return false;

	// We're now ready and running...
	return true;
}


// Device engine stop method.
void qtractorAudioEngine::stop (void)
{
	if (!isActivated())
	    return;

	// WTF?...
}


// Device engine deactivation method.
void qtractorAudioEngine::deactivate (void)
{
	// We're stopping now...
	setPlaying(false);

	// Deactivate the JACK client first.
	if (m_pJackClient)
		jack_deactivate(m_pJackClient);
}


// Device engine cleanup method.
void qtractorAudioEngine::clean (void)
{
	// Close the JACK client, finally.
	if (m_pJackClient) {
		jack_client_close(m_pJackClient);
		m_pJackClient = NULL;
	}
}


// Process cycle executive.
int qtractorAudioEngine::process ( unsigned int nframes )
{
	// Don't bother with a thing, if not running.
	if (!isActivated())
		return 1;

	// Prepare current audio busses...
	for (qtractorBus *pBus = busses().first();
		pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus)
			pAudioBus->process(nframes);
	}

	// Don't go any further, if not playing.
	if (!isPlaying())
		return 1;

	// Make sure we have an actual session cursor...
	qtractorSession *pSession = session();
	if (pSession == NULL)
		return 1;
	qtractorSessionCursor *pAudioCursor = sessionCursor();
	if (pAudioCursor == NULL)
	    return 1;

	// This the legal process cycle frame range...
	unsigned long iFrameStart = pAudioCursor->frame();
	unsigned long iFrameEnd   = iFrameStart + nframes;

	// Split processing, in case we're looping...
	if (pSession->isLooping() && iFrameStart < pSession->loopEnd()) {
		// Loop-length might be shorter than the buffer-period... 
		while (iFrameEnd > pSession->loopEnd()) {
			// Process the remaining until end-of-loop...
			pSession->process(pAudioCursor, iFrameStart, pSession->loopEnd());
			// Reset to start-of-loop...
			iFrameStart = pSession->loopStart();
			iFrameEnd   = iFrameStart + (iFrameEnd - pSession->loopEnd());
			pAudioCursor->seek(iFrameStart);
		}
	}

	// Regular range...
	pSession->process(pAudioCursor, iFrameStart, iFrameEnd);

	// Sync with loop boundaries (unlikely?)...
	if (pSession->isLooping() && iFrameEnd >= pSession->loopEnd())
		iFrameEnd = pSession->loopStart() + (iFrameEnd - pSession->loopEnd());

	// Prepare advance for next cycle.
	pAudioCursor->seek(iFrameEnd);
	pAudioCursor->process(nframes);

	// Always sync to MIDI output thread...
	pSession->midiEngine()->sync();

	// Check whether we're setting loops asynchronously...
	if (qtractorAudioBufferThread::Instance().loopSync())
		pSession->setLoopCommit();

	// Process session stuff...
	return 1;
}


// Document element methods.
bool qtractorAudioEngine::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	qtractorEngine::clear();

	// Load session children...
	for (QDomNode nChild = pElement->firstChild();
			!nChild.isNull();
				nChild = nChild.nextSibling()) {

		// Convert node to element...
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		if (eChild.tagName() == "audio-bus") {
			bool bAutoConnect = false;
			unsigned short iChannels = 2;
			QString sBusName = eChild.attribute("name");
			qtractorBus::BusMode busMode
				= pDocument->loadBusMode(eChild.attribute("mode"));
			for (QDomNode nProp = eChild.firstChild();
					!nProp.isNull();
						nProp = nProp.nextSibling()) {
				// Convert audio-bus property to element...
				QDomElement eProp = nProp.toElement();
				if (eProp.isNull())
					continue;
				if (eProp.tagName() == "channels")
					iChannels = eProp.text().toUShort();
				else if (eProp.tagName() == "auto-connect")
					bAutoConnect = pDocument->boolFromText(eProp.text());
			}
			qtractorAudioBus *pAudioBus	= new qtractorAudioBus(
				sBusName, busMode, iChannels, bAutoConnect);
			qtractorEngine::addBus(pAudioBus);
		}
	}

	return true;
}


bool qtractorAudioEngine::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	// Save audio busses...
	for (qtractorBus *pBus = qtractorEngine::busses().first();
			pBus; pBus = pBus->next()) {
		qtractorAudioBus *pAudioBus
			= static_cast<qtractorAudioBus *> (pBus);
		if (pAudioBus) {
			// Create the new audio bus element...
			QDomElement eAudioBus
				= pDocument->document()->createElement("audio-bus");
			eAudioBus.setAttribute("name",
				pAudioBus->busName());
			eAudioBus.setAttribute("mode",
				pDocument->saveBusMode(pAudioBus->busMode()));
			pDocument->saveTextElement("channels",
				QString::number(pAudioBus->channels()),
					&eAudioBus);
			pDocument->saveTextElement("auto-connect",
				pDocument->textFromBool(pAudioBus->isAutoConnected()),
					&eAudioBus);
			pElement->appendChild(eAudioBus);
		}
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorAudioBus -- Managed JACK port set
//

// Constructor.
qtractorAudioBus::qtractorAudioBus ( const QString& sBusName,
	BusMode mode, unsigned short iChannels, bool bAutoConnect )
	: qtractorBus(sBusName, mode)
{
	m_iChannels    = iChannels;
	m_bAutoConnect = bAutoConnect;

	m_ppIPorts     = NULL;
	m_ppOPorts     = NULL;

	m_ppIBuffer    = NULL;
	m_ppOBuffer    = NULL;
}


// Channel number property accessor.
void qtractorAudioBus::setChannels ( unsigned short iChannels )
{
	m_iChannels = iChannels;
}

unsigned short qtractorAudioBus::channels (void) const
{
	return m_iChannels;
}


// Auto-connection predicate.
void qtractorAudioBus::setAutoConnected ( bool bAutoConnect )
{
	m_bAutoConnect = bAutoConnect;
}

bool qtractorAudioBus::isAutoConnected (void) const
{
	return m_bAutoConnect;
}


// Register and pre-allocate bus port buffers.
bool qtractorAudioBus::open (void)
{
	close();

	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return false;
	if (pAudioEngine->jackClient() == NULL)
		return false;

	unsigned short i;
	
	if (busMode() & qtractorBus::Input) {
		// Register and allocate input port buffers...
		m_ppIPorts  = new jack_port_t * [m_iChannels];
		m_ppIBuffer = new float * [m_iChannels];
		const QString sIPortName(busName() + "/in_%1");
		for (i = 0; i < m_iChannels; i++) {
			m_ppIPorts[i] = jack_port_register(
				pAudioEngine->jackClient(),
				sIPortName.arg(i + 1).latin1(), JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsInput, 0);
			m_ppIBuffer[i] = NULL;
		}
	}

	if (busMode() & qtractorBus::Output) {
		// Register and allocate output port buffers...
		m_ppOPorts  = new jack_port_t * [m_iChannels];
		m_ppOBuffer = new float * [m_iChannels];
		const QString sOPortName(busName() + "/out_%1");
		for (i = 0; i < m_iChannels; i++) {
			m_ppOPorts[i] = jack_port_register(
				pAudioEngine->jackClient(),
				sOPortName.arg(i + 1).latin1(), JACK_DEFAULT_AUDIO_TYPE,
				JackPortIsOutput, 0);
			m_ppOBuffer[i] = NULL;
		}
	}

	return true;
}


// Unregister and post-free bus port buffers.
void qtractorAudioBus::close (void)
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;
	if (pAudioEngine->jackClient() == NULL)
		return;

	unsigned short i;

	if (busMode() & qtractorBus::Input) {
		// Free input ports.
		if (m_ppIPorts) {
			for (i = 0; i < m_iChannels; i++) {
				if (m_ppIPorts[i]) {
					jack_port_unregister(
						pAudioEngine->jackClient(), m_ppIPorts[i]);
					m_ppIPorts[i] = NULL;
				}
			}
			delete [] m_ppIPorts;
			m_ppIPorts = NULL;
		}
		// Free input Buffers.
		if (m_ppIBuffer)
			delete [] m_ppIBuffer;
		m_ppIBuffer = NULL;
	}

	if (busMode() & qtractorBus::Output) {
		// Free output ports.
		if (m_ppOPorts) {
			for (i = 0; i < m_iChannels; i++) {
				if (m_ppOPorts[i]) {
					jack_port_unregister(
						pAudioEngine->jackClient(), m_ppOPorts[i]);
					m_ppOPorts[i] = NULL;
				}
			}
			delete [] m_ppOPorts;
			m_ppOPorts = NULL;
		}
		// Free output Buffers.
		if (m_ppOBuffer)
			delete [] m_ppOBuffer;
		m_ppOBuffer = NULL;
	}
}


// Auto-connect to physical ports.
void qtractorAudioBus::autoConnect (void)
{
	qtractorAudioEngine *pAudioEngine
		= static_cast<qtractorAudioEngine *> (engine());
	if (pAudioEngine == NULL)
		return;
	if (pAudioEngine->jackClient() == NULL)
		return;

	if (!m_bAutoConnect)
		return;

	unsigned short i;

	if (busMode() & qtractorBus::Input) {
		const char **ppszOPorts
			= jack_get_ports(pAudioEngine->jackClient(), 0, 0,
				JackPortIsOutput | JackPortIsPhysical);
		if (ppszOPorts) {
			const QString sIPortName = pAudioEngine->clientName()
				+ ':' + busName() + "/in_%1";
			for (i = 0; i < m_iChannels && ppszOPorts[i]; i++) {
				jack_connect(pAudioEngine->jackClient(),
					ppszOPorts[i], sIPortName.arg(i + 1).latin1());
			}
			::free(ppszOPorts);
		}
	}

	if (busMode() & qtractorBus::Output) {
		const char **ppszIPorts
			= jack_get_ports(pAudioEngine->jackClient(), 0, 0,
				JackPortIsInput | JackPortIsPhysical);
		if (ppszIPorts) {
			const QString sOPortName = pAudioEngine->clientName()
				+ ':' + busName() + "/out_%1";
			for (i = 0; i < m_iChannels && ppszIPorts[i]; i++) {
				jack_connect(pAudioEngine->jackClient(),
					sOPortName.arg(i + 1).latin1(), ppszIPorts[i]);
			}
			::free(ppszIPorts);
		}
	}
}


// Process cycle (preparator only).
void qtractorAudioBus::process ( unsigned int nframes )
{
	for (unsigned short i = 0; i < m_iChannels; i++) {
		if (busMode() & qtractorBus::Input) {
			m_ppIBuffer[i] = static_cast<float *>
				(jack_port_get_buffer(m_ppIPorts[i], nframes));
		}
		if (busMode() & qtractorBus::Output) {
			m_ppOBuffer[i] = static_cast<float *>
				(jack_port_get_buffer(m_ppOPorts[i], nframes));
			::memset(m_ppOBuffer[i], 0, nframes * sizeof(float));
		}
	}
}


// Frame buffer accessors.
float **qtractorAudioBus::in (void) const
{
	return m_ppIBuffer;
}

float **qtractorAudioBus::out (void) const
{
	return m_ppOBuffer;
}


// end of qtractorAudioEngine.cpp
