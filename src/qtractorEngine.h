// qtractorEngine.h
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

#ifndef __qtractorEngine_h
#define __qtractorEngine_h

#include "qtractorTrack.h"


// Forward declarations.
class qtractorBus;
class qtractorSessionCursor;
class qtractorCurveFile;

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
	bool open();
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

	qtractorBus *findBus(const QString& sBusName) const;
	qtractorBus *findInputBus(const QString& sInputBusName) const;
	qtractorBus *findOutputBus(const QString& sOutputBusName) const;

	// Exo-buses list managament methods.
	const qtractorList<qtractorBus>& busesEx() const;

	void addBusEx(qtractorBus *pBus);
	void removeBusEx(qtractorBus *pBus);

	qtractorBus *findBusEx(const QString& sBusName) const;

	// Retrieve/restore all connections, on all buses;
	// return the effective number of connection attempts.
	virtual int updateConnects();

	// Document element methods.
	virtual bool loadElement(qtractorDocument *pDocument, QDomElement *pElement) = 0;
	virtual bool saveElement(qtractorDocument *pDocument, QDomElement *pElement) const = 0;

	// Clear/reset all pending connections.
	void clearConnects();

protected:

	// Derived classes must set on this...
	virtual bool init() = 0;
	virtual bool activate() = 0;
	virtual bool start() = 0;
	virtual void stop() = 0;
	virtual void deactivate() = 0;
	virtual void clean() = 0;

	// Retrieve/restore connections, on given buses;
	// return the effective number of connection attempts.
	int updateConnects(qtractorBus *pBus);

private:

	// Device instance variables.
	qtractorSession       *m_pSession;
	qtractorSessionCursor *m_pSessionCursor;

	// Engine running flags.
	bool m_bActivated;
	bool m_bPlaying;

	qtractorList<qtractorBus> m_buses;
	qtractorList<qtractorBus> m_busesEx;
};


//----------------------------------------------------------------------
// class qtractorBus -- Abstract device bus.
//

class qtractorBus : public qtractorList<qtractorBus>::Link
{
public:

	// Bus operation mode bit-flags.
	enum BusMode { None = 0, Input = 1, Output = 2, Duplex = 3, Ex = 4 };

	// Constructor.
	qtractorBus(qtractorEngine *pEngine,
		const QString& sBusName, BusMode busMode, bool bMonitor = false);

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
	void setMonitor(bool bMonitor);
	bool isMonitor() const;

	// Pure virtual activation methods.
	virtual bool open() = 0;
	virtual void close() = 0;

	// I/O bus-monitor accessors.
	virtual qtractorMonitor *monitor_in()  const = 0;
	virtual qtractorMonitor *monitor_out() const = 0;

	// State (monitor) button setup.
	qtractorSubject *monitorSubject() const;
	qtractorMidiControlObserver *monitorObserver() const;

	// State (monitor) notifier (proto-slot).
	void monitorChangeNotify(bool bOn);

	// Load/save bus (monitor, gain, pan) controllers (MIDI).
	void loadControllers(
		QDomElement *pElement, BusMode busMode);
	void saveControllers(qtractorDocument *pDocument,
		QDomElement *pElement, BusMode busMode) const;

	// Map bus (monitor, gain, pan) controllers (MIDI).
	void mapControllers(BusMode busMode);

	// Bus automation curve serialization methods.
	static void loadCurveFile(
		QDomElement *pElement, BusMode busMode, qtractorCurveFile *pCurveFile);
	void saveCurveFile(qtractorDocument *pDocument,
		QDomElement *pElement, BusMode busMode, qtractorCurveFile *pCurveFile) const;
	void applyCurveFile(BusMode busMode, qtractorCurveFile *pCurveFile) const;

	// Connection list stuff.
	struct ConnectItem
	{
		// Default contructor
		ConnectItem() : index(0), client(-1), port (-1) {}
		// Copy contructor
		ConnectItem(const ConnectItem& item)
			: index(item.index),
			client(item.client), port(item.port),
			clientName(item.clientName),
			portName(item.portName) {}
		// Item members.
		unsigned short index;
		int client, port;
		QString clientName;
		QString portName;
	};

	class ConnectList : public QList<ConnectItem *>
	{
	public:
		// Constructor.
		ConnectList() {}
		// Copy onstructor.
		ConnectList(const ConnectList& connects)
			: QList<ConnectItem *>() { copy(connects); }
		// Destructor.
		~ConnectList() { clear(); }
		// Item cleaner...
		void clear()
		{
			qDeleteAll(*this);
			QList<ConnectItem *>::clear();
		}
		// List copy...
		void copy (const ConnectList& connects)
		{
			clear();
			QListIterator<ConnectItem *> iter(connects);
			while (iter.hasNext())
				append(new ConnectItem(*iter.next()));
		}
		// Item finder...
		ConnectItem *findItem(const ConnectItem& item)
		{
			QListIterator<ConnectItem *> iter(*this);
			while (iter.hasNext()) {
				ConnectItem *pItem = iter.next();
				if (pItem->index      == item.index &&
					pItem->clientName == item.clientName &&
					pItem->portName   == item.portName) {
					return pItem;
				}
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
		ConnectList& connects, bool bConnect = false) const = 0;

	// Document element methods.
	static bool loadConnects(ConnectList& connects,
		qtractorDocument *pDocument, QDomElement *pElement);
	static bool saveConnects(ConnectList& connects,
		qtractorDocument *pDocument, QDomElement *pElement);

	// Bus mode textual helper methods.
	static BusMode busModeFromText (const QString& sText);
	static QString textFromBusMode (BusMode busMode);

protected:

	// Bus mode change event.
	virtual void updateBusMode() = 0;

private:

	// Instance variables.
	qtractorEngine *m_pEngine;

	QString m_sBusName;
	BusMode m_busMode;

	// Connections stuff.
	ConnectList m_inputs;
	ConnectList m_outputs;

	// State (monitor) observer stuff.
	qtractorSubject *m_pMonitorSubject;

	qtractorMidiControlObserver *m_pMonitorObserver;

	qtractorMidiControl::Controllers m_controllers_in;
	qtractorMidiControl::Controllers m_controllers_out;
};


#endif  // __qtractorEngine_h


// end of qtractorEngine.h
