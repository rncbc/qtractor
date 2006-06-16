// qtractorAudioEngine.h
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

#ifndef __qtractorAudioEngine_h
#define __qtractorAudioEngine_h

#include "qtractorEngine.h"

#include <jack/jack.h>

#include <qevent.h>


// Forward declarations.
class qtractorAudioBus;
class qtractorAudioMonitor;


//----------------------------------------------------------------------
// class qtractorAudioEngine -- JACK client instance (singleton).
//

class qtractorAudioEngine : public qtractorEngine
{
public:

	// Constructor.
	qtractorAudioEngine(qtractorSession *pSession);

	// JACK client descriptor accessor.
	jack_client_t *jackClient() const;

	// Process cycle executive.
	int process(unsigned int nframes);

	// Idle process cycle executive.
	int process_idle(unsigned int nframes);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

	// Event notifier widget settings.
	void setNotifyWidget       (QWidget *pNotifyWidget);
	void setNotifyShutdownType (QEvent::Type eNotifyShutdownType);
	void setNotifyXrunType     (QEvent::Type eNotifyXrunType);
	void setNotifyPortType     (QEvent::Type eNotifyPortType);

	QWidget     *notifyWidget() const;
	QEvent::Type notifyShutdownType() const;
	QEvent::Type notifyXrunType() const;
	QEvent::Type notifyPortType() const;

protected:

	// Concrete device (de)activation methods.
	bool init(const QString& sClientName);
	bool activate();
	bool start();
	void stop();
	void deactivate();
	void clean();

private:

	// Audio device instance variables.
	jack_client_t *m_pJackClient;

	// The event notifier widget.
	QWidget      *m_pNotifyWidget;
	QEvent::Type  m_eNotifyShutdownType;
	QEvent::Type  m_eNotifyXrunType;
	QEvent::Type  m_eNotifyPortType;
};


//----------------------------------------------------------------------
// class qtractorAudioBus -- Managed JACK port set
//

class qtractorAudioBus : public qtractorBus
{
public:

	// Constructor.
	qtractorAudioBus(qtractorAudioEngine *pAudioEngine,
		const QString& sBusName, BusMode busMode = Duplex,
		unsigned short iChannels = 2, bool bAutoConnect = true);

	// Destructor.
	~qtractorAudioBus();

	// Channel number property accessor.
	void setChannels(unsigned short iChannels);
	unsigned short channels() const;

	// Auto-connection predicate.
	void setAutoConnected(bool bAutoConnect);
	bool isAutoConnected() const;

	// Concrete activation methods.
	bool open();
	void close();

	// Auto-connect to physical ports.
	void autoConnect();

	// Process cycle (preparator only).
	void process_prepare(unsigned int nframes);
	void process_commit(unsigned int nframes);

	// Bus-buffering methods.
	void buffer_prepare(unsigned int nframes);
	void buffer_commit(unsigned int nframes, float fGain = 1.0f);

	float **buffer() const;

	// Frame buffer accessors.
	float **in()  const;
	float **out() const;

	// Virtual I/O bus-monitor accessors.
	qtractorMonitor *monitor_in()  const;
	qtractorMonitor *monitor_out() const;

	// Audio I/O bus-monitor accessors.
	qtractorAudioMonitor *audioMonitor_in()  const;
	qtractorAudioMonitor *audioMonitor_out() const;

private:

	// Instance variables.
	unsigned short m_iChannels;
	bool           m_bAutoConnect;

	// Specific monitor instances.
	qtractorAudioMonitor *m_pIAudioMonitor;
	qtractorAudioMonitor *m_pOAudioMonitor;

	// Specific JACK ports stuff.
	jack_port_t    **m_ppIPorts;
	jack_port_t    **m_ppOPorts;
	float          **m_ppIBuffer;
	float          **m_ppOBuffer;
	float          **m_ppXBuffer;
};


#endif  // __qtractorAudioEngine_h


// end of qtractorAudioEngine.h
