// qtractorEngine.h
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

#ifndef __qtractorEngine_h
#define __qtractorEngine_h

#include "qtractorSession.h"


// Forward declarations.
class qtractorBus;
class qtractorSessionCursor;
class qtractorSessionDocument;

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

	// Busses list clear.
	void clear();

	// Session helper accessor.
	qtractorSession *session() const;

	// Session cursor sync type accessor.
	qtractorTrack::TrackType syncType() const;

	// Session cursor accessor.
	qtractorSessionCursor *sessionCursor() const;

	// Client name accessor.
	const QString& clientName() const;

	// Engine status methods.
	bool isActivated() const;

	// Engine state methods.
	void setPlaying(bool bPlaying);
	bool isPlaying() const;

	// Busses list managament methods.
	qtractorList<qtractorBus>& busses();

	void addBus(qtractorBus *pBus);
	void removeBus(qtractorBus *pBus);

	qtractorBus *findBus(const QString& sBusName);

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
	qtractorSession         *m_pSession;
	qtractorTrack::TrackType m_syncType;
	qtractorSessionCursor   *m_pSessionCursor;

	QString m_sClientName;

	// Engine running flags.
	bool m_bActivated;
	bool m_bPlaying;

	qtractorList<qtractorBus> m_busses;
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
	qtractorBus(const QString& sBusName, BusMode mode = Duplex);

	// Destructor.
	virtual ~qtractorBus();

	// Device accessor.
	void setEngine(qtractorEngine *pEngine);
	qtractorEngine *engine() const;

	// Bus name accessors.
	void setBusName(const QString& sBusName);
	const QString& busName() const;

	// Bus mode property accessor.
	void setBusMode(BusMode mode);
	BusMode busMode() const;

	// Pure virtual activation methods.
	virtual bool open() = 0;
	virtual void close() = 0;

private:

	// Instance variables.
	qtractorEngine *m_pEngine;

	QString m_sBusName;
	BusMode m_busMode;
};


#endif  // __qtractorEngine_h


// end of qtractorEngine.h
