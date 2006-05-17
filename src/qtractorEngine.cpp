// qtractorEngine.cpp
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

#include "qtractorEngine.h"

#include "qtractorSessionCursor.h"


//----------------------------------------------------------------------
// class qtractorEngine -- Abstract device engine instance (singleton).
//

// Constructor.
qtractorEngine::qtractorEngine ( qtractorSession *pSession,
	qtractorTrack::TrackType syncType )
{
	m_pSession       = pSession;
	m_pSessionCursor = m_pSession->createSessionCursor(0, syncType);
	m_bActivated     = false;
	m_bPlaying       = false;

	m_busses.setAutoDelete(true);
}

// Destructor.
qtractorEngine::~qtractorEngine (void)
{
	clear();

	if (m_pSessionCursor)
		delete m_pSessionCursor;
}


// Busses list clear.
void qtractorEngine::clear (void)
{
	m_busses.clear();
}


// Session accessor.
qtractorSession *qtractorEngine::session (void) const
{
	return m_pSession;
}


// Session cursor accessor.
qtractorSessionCursor *qtractorEngine::sessionCursor (void) const
{
	return m_pSessionCursor;
}


// Engine type accessor.
qtractorTrack::TrackType qtractorEngine::syncType (void) const
{
	return m_pSessionCursor->syncType();
}


// Client name accessor.
const QString& qtractorEngine::clientName (void) const
{
	return m_sClientName;
}


// Activation status accessor.
bool qtractorEngine::isActivated(void) const
{
	return m_bActivated;
}


// Busses list managament methods.
qtractorList<qtractorBus>& qtractorEngine::busses (void)
{
	return m_busses;
}


// Add a bus to a device engine.
void qtractorEngine::addBus ( qtractorBus *pBus )
{
	m_busses.append(pBus);
}


// Remove a bus from a device.
void qtractorEngine::removeBus ( qtractorBus *pBus )
{
	m_busses.remove(pBus);
}


// Find a device bus by name
qtractorBus *qtractorEngine::findBus ( const QString& sBusName )
{
	for (qtractorBus *pBus = m_busses.first();
			pBus; pBus = pBus->next()) {
		if (pBus->busName() == sBusName)
			return pBus;
	}

	return NULL;
}


// Device engine activation method.
bool qtractorEngine::open ( const QString& sClientName )
{
//	close();

	// Damn it.
	if (m_pSession == NULL)
		return false;

	// Call derived initialization...
	if (!init(sClientName))
		return false;

	// Set actual client name...
	m_sClientName = sClientName;

	// Update the session cursor tracks...
	m_pSessionCursor->reset();

	// Open all busses (allocated and register ports...)
	qtractorBus *pBus = m_busses.first();
	while (pBus) {
		if (!pBus->open()) {
			close();
			return false;
		}
		pBus = pBus->next();
	}

	// Nows time to activate...
	if (!activate()) {
		close();
		return false;
	}

	// We're now ready and running...
	m_bActivated = true;

	return true;
}


// Device engine deactivation method.
void qtractorEngine::close (void)
{
	// Save current activation state...
	bool bActivated = m_bActivated;
	// We're stopping now...
	m_bActivated = false;

	if (bActivated) {
		// Deactivate the derived engine first.
		deactivate();
		// Close all dependant busses...
		for (qtractorBus *pBus = m_busses.first();
				pBus; pBus = pBus->next()) {
			pBus->close();
		}
	}

	// Clean-up everything, finally
	clean();
}


// Engine state methods.
void qtractorEngine::setPlaying ( bool bPlaying )
{
	m_pSessionCursor->setFrameTime(0);

	if (bPlaying && !m_bPlaying) {
		m_bPlaying = start();
	}
	else
	if (!bPlaying && m_bPlaying) {
		m_bPlaying = false;
		stop();
	}
}

bool qtractorEngine::isPlaying(void) const
{
	return m_bPlaying;
}



//----------------------------------------------------------------------
// class qtractorBus -- Managed ALSA sequencer port set
//

// Constructor.
qtractorBus::qtractorBus ( qtractorEngine *pEngine,
	const QString& sBusName, BusMode busMode )
{
	m_pEngine   = pEngine;
	m_sBusName  = sBusName;
	m_busMode   = busMode;
}

// Destructor.
qtractorBus::~qtractorBus (void)
{
}


// Engine accessor.
qtractorEngine *qtractorBus::engine (void) const
{
	return m_pEngine;
}


// Bus type accessor.
qtractorTrack::TrackType qtractorBus::busType (void) const
{
	return (m_pEngine ? m_pEngine->syncType() : qtractorTrack::None);
}


// Bus name accessors.
void qtractorBus::setBusName ( const QString& sBusName )
{
	m_sBusName = sBusName;
}

const QString& qtractorBus::busName (void) const
{
	return m_sBusName;
}


// Bus mode property accessor.
void qtractorBus::setBusMode ( qtractorBus::BusMode mode )
{
	m_busMode = mode;
}

qtractorBus::BusMode qtractorBus::busMode (void) const
{
	return m_busMode;
}


// end of qtractorEngine.cpp
