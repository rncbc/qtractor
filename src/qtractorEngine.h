// qtractorEngine.h
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

#ifndef __qtractorEngine_h
#define __qtractorEngine_h

#include "qtractorSession.h"

#include <QList>

// Forward declarations.
class qtractorBus;
class qtractorSessionCursor;
class qtractorSessionDocument;
class qtractorMonitor;

class QDomElement;


//----------------------------------------------------------------------
// class qtractorEngine -- Abstract device engine instance (singleton).
//

class qtractorEngine
{
public:

	// Constructor.
	qtractorEngine(qtractorSession *pSession,
		qtractorTrack::TrackType syncType);
	// Destructor.
	virtual ~qtractorEngine();

	// Device engine activation methods.
	bool open(const QString& sClientName);
	void close();

	// Buses list clear.
	void clear();

	// Session helper accessor.
	qtractorSession *session() const;

	// Session cursor accessor.
	qtractorSessionCursor *sessionCursor() const;

	// Engine type method.
	qtractorTrack::TrackType syncType() const;

	// Client name accessor.
	const QString& clientName() const;

	// Engine status methods.
	bool isActivated() const;

	// Engine state methods.
	void setPlaying(bool bPlaying);
	bool isPlaying() const;

	// Buses list managament methods.
	const qtractorList<qtractorBus>& buses() const;

	void addBus(qtractorBus *pBus);
	void removeBus(qtractorBus *pBus);

	qtractorBus *findBus(const QString& sBusName);

	// Retrieve/restore all connections, on all buses;
	// return the effective number of connection attempts.
	virtual int updateConnects();

	// Document element methods.
	virtual bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;
	virtual bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement) = 0;

protected:

	// Derived classes must set on this...
	virtual bool init(const QString& sClientName) = 0;
	virtual bool activate() = 0;
	virtual bool start() = 0;
	virtual void stop() = 0;
	virtual void deactivate() = 0;
	virtual void clean() = 0;

private:

	// Device instance variables.
	qtractorSession       *m_pSession;
	qtractorSessionCursor *m_pSessionCursor;

	QString m_sClientName;

	// Engine running flags.
	bool m_bActivated;
	bool m_bPlaying;

	qtractorList<qtractorBus> m_buses;
};


//----------------------------------------------------------------------
// class qtractorBus -- Abstract device bus.
//

class qtractorBus : public qtractorList<qtractorBus>::Link
{
public:

	// Bus operation mode bit-flags.
	enum BusMode { None = 0, Input = 1, Output = 2, Duplex = 3 };

	// Constructor.
	qtractorBus(qtractorEngine *pEngine,
		const QString& sBusName, BusMode busMode = Duplex,
		bool bPassthru = false);

	// Destructor.
	virtual ~qtractorBus();

	// Device accessor.
	qtractorEngine *engine() const;

	// Bus type method.
	qtractorTrack::TrackType busType() const;

	// Bus name accessors.
	void setBusName(const QString& sBusName);
	const QString& busName() const;

	// Bus mode property accessor.
	void setBusMode(BusMode busMode);
	BusMode busMode() const;

	// Pass-thru mode accessor.
	void setPassthru(bool bPassthru);
	bool isPassthru() const;

	// Pure virtual activation methods.
	virtual bool open() = 0;
	virtual void close() = 0;

	// I/O bus-monitor accessors.
	virtual qtractorMonitor *monitor_in()  const = 0;
	virtual qtractorMonitor *monitor_out() const = 0;

	// Connection list stuff.
	struct ConnectItem
	{
		unsigned short index;
		QString clientName;
		QString portName;
	};

	class ConnectList : public QList<ConnectItem *>
	{
	public:
		// Constructor
		ConnectList() {}
		// Destructor.
		~ConnectList() { qDeleteAll(*this); }
		// Item finder...
		ConnectItem *findItem(const ConnectItem& item)
		{
			QListIterator<ConnectItem *> iter(*this);
			while (iter.hasNext()) {
				ConnectItem *pItem = iter.next();
				if (pItem->index      == item.index &&
					pItem->clientName == item.clientName &&
					pItem->portName   == item.portName) 
					return pItem;
			}
			return NULL;
		}
	};

	// Connection lists accessors.
	ConnectList& inputs()  { return m_inputs;  }
	ConnectList& outputs() { return m_outputs; }
	
	// Retrieve/restore client:port connections;
	// return the effective number of connection attempts.
	virtual int updateConnects(BusMode busMode,
		ConnectList& connects, bool bConnect = false) = 0;

	// Document element methods.
	bool loadConnects(ConnectList& connects,
		qtractorSessionDocument *pDocument, QDomElement *pElement);
	bool saveConnects(ConnectList& connects,
		qtractorSessionDocument *pDocument, QDomElement *pElement);

protected:

	// Bus mode change event.
	virtual void updateBusMode() = 0;

private:

	// Instance variables.
	qtractorEngine *m_pEngine;

	QString m_sBusName;
	BusMode m_busMode;
	bool m_bPassthru;

	// Connections stuff.
	ConnectList m_inputs;
	ConnectList m_outputs;
};


#endif  // __qtractorEngine_h


// end of qtractorEngine.h
