// qtractorMidiBuffer.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiBuffer.h"

#include "qtractorSession.h"
#include "qtractorPlugin.h"

#include "qtractorMainForm.h"

#include "qtractorAudioEngine.h"


//----------------------------------------------------------------------
// class qtractorMidiManager -- MIDI internal plugin list manager.
//

// Constructor.
qtractorMidiManager::qtractorMidiManager ( qtractorSession *pSession,
	qtractorPluginList *pPluginList, unsigned int iBufferSize ) :
	m_pSession(pSession),
	m_pPluginList(pPluginList),
	m_directBuffer(iBufferSize),
	m_queuedBuffer(iBufferSize),
	m_pBuffer(NULL),
	m_iBuffer(0)
{
	m_pBuffer = new snd_seq_event_t [bufferSize() << 1];
}

// Destructor.
qtractorMidiManager::~qtractorMidiManager (void)
{
	if (m_pBuffer)
		delete [] m_pBuffer;
}


// Direct buffering.
bool qtractorMidiManager::direct ( snd_seq_event_t *pEvent )
{
	return m_directBuffer.push(pEvent,
		m_pSession->frameFromTick(pEvent->time.tick)); 
}

// Queued buffering.
bool qtractorMidiManager::queued ( snd_seq_event_t *pEvent )
{
	return m_queuedBuffer.push(pEvent,
		m_pSession->frameFromTick(pEvent->time.tick));
}


// Process buffers (merge).
void qtractorMidiManager::process (
	unsigned long iTimeStart, unsigned long iTimeEnd )
{
	clear();

	snd_seq_event_t *pEv1 = m_directBuffer.peek();
	snd_seq_event_t *pEv2 = m_queuedBuffer.peek();

	while ((pEv1 && pEv1->time.tick < iTimeEnd)
		|| (pEv2 && pEv2->time.tick < iTimeEnd)) {
		while ((pEv1 &&  pEv2 && pEv2->time.tick >= pEv1->time.tick)
			|| (pEv1 && !pEv2 && pEv1->time.tick < iTimeEnd)) {
			m_pBuffer[m_iBuffer] = *pEv1;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv1->time.tick > iTimeStart
					? pEv1->time.tick - iTimeStart : 0);
			pEv1 = m_directBuffer.next();
		}
		while ((pEv2 &&  pEv1 && pEv1->time.tick >= pEv2->time.tick)
			|| (pEv2 && !pEv1 && pEv2->time.tick < iTimeEnd)) {
			m_pBuffer[m_iBuffer] = *pEv2;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv2->time.tick > iTimeStart
					? pEv2->time.tick - iTimeStart : 0);
			pEv2 = m_queuedBuffer.next();
		}
	}

	// FIXME: Now's time to process the plugins as usual...
	qtractorAudioBus *pOutputAudioBus
		= static_cast<qtractorAudioBus *> (
			(m_pSession->audioEngine())->buses().first());
	if (pOutputAudioBus) {
		unsigned int nframes = iTimeEnd - iTimeStart;
		m_pPluginList->process(pOutputAudioBus->out(), nframes);
	}
}


// Resets all buffering.
void qtractorMidiManager::reset (void)
{
	m_directBuffer.clear();
	m_queuedBuffer.clear();

	clear();
}


// Factory (proxy) methods.
qtractorMidiManager *qtractorMidiManager::createMidiManager (
	qtractorPluginList *pPluginList )
{
	qtractorMidiManager *pMidiManager = NULL;

	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();
	if (pSession)
		pMidiManager = pSession->createMidiManager(pPluginList);

	return pMidiManager;
}


void qtractorMidiManager::deleteMidiManager ( qtractorMidiManager *pMidiManager )
{
	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();
	if (pSession)
		pSession->deleteMidiManager(pMidiManager);
}


// end of qtractorMidiBuffer.cpp
