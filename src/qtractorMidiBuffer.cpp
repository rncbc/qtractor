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

#include "qtractorAbout.h"
#include "qtractorMidiBuffer.h"

#include "qtractorSession.h"
#include "qtractorPlugin.h"

#include "qtractorMainForm.h"

#include "qtractorAudioEngine.h"


// Specific controller definitions
#define BANK_SELECT_MSB		0x00
#define BANK_SELECT_LSB		0x20


//----------------------------------------------------------------------
// class qtractorMidiManager -- MIDI internal plugin list manager.
//

// Constructor.
qtractorMidiManager::qtractorMidiManager ( qtractorSession *pSession,
	qtractorPluginList *pPluginList, unsigned int iBufferSize ) :
	m_pSession(pSession),
	m_pPluginList(pPluginList),
	m_directBuffer(iBufferSize >> 2),
	m_queuedBuffer(iBufferSize),
	m_postedBuffer(iBufferSize >> 1),
	m_pBuffer(NULL), m_iBuffer(0),
	m_iPendingBankMSB(-1),
	m_iPendingBankLSB(-1),
	m_iPendingProg(-1)
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
	if (pEvent->type == SND_SEQ_EVENT_CONTROLLER) {
		switch (pEvent->data.control.param) {
		case BANK_SELECT_MSB:
			m_iPendingBankMSB = pEvent->data.control.value;
			break;
		case BANK_SELECT_LSB:
			m_iPendingBankLSB = pEvent->data.control.value;
			break;
		}
	}
	else if (pEvent->type == SND_SEQ_EVENT_PGMCHANGE)
		m_iPendingProg = pEvent->data.control.value;

	return m_directBuffer.push(pEvent);
}

// Queued buffering.
bool qtractorMidiManager::queued ( snd_seq_event_t *pEvent )
{
	unsigned long iTick = m_pSession->frameFromTick(pEvent->time.tick);

	if (pEvent->type == SND_SEQ_EVENT_NOTE) {
		snd_seq_event_t ev = *pEvent;
		ev.type = SND_SEQ_EVENT_NOTEON;
		if (!m_queuedBuffer.push(&ev, iTick))
			return false;
		iTick += m_pSession->frameFromTick(ev.data.note.duration);
		ev.type = SND_SEQ_EVENT_NOTEOFF;
		ev.data.note.velocity = 0;
		ev.data.note.duration = 0;
		return m_postedBuffer.insert(&ev, iTick);
	}

	return m_queuedBuffer.push(pEvent, iTick);
}


// Process buffers (merge).
void qtractorMidiManager::process (
	unsigned long iTimeStart, unsigned long iTimeEnd )
{
	clear();

	// Check for programn change...
	if (m_iPendingProg >= 0) {
		int iBank = 0;
		int iProg = m_iPendingProg;
		if (m_iPendingBankLSB >= 0) {
			if (m_iPendingBankMSB >= 0)
				iBank = (m_iPendingBankMSB << 7) + m_iPendingBankLSB;
			else
				iBank = m_iPendingBankLSB;
		}
		else if (m_iPendingBankMSB >= 0)
			iBank = m_iPendingBankMSB;
		// Make the change (should be RT safe...)
		m_pPluginList->selectProgram(iBank, iProg);
		// Reset pending status.
		m_iPendingBankMSB = -1;
		m_iPendingBankLSB = -1;
		m_iPendingProg    = -1;
	}

	// Maerge events in buffer for plugin processing...
	snd_seq_event_t *pEv0 = m_directBuffer.peek();
	snd_seq_event_t *pEv1 = m_queuedBuffer.peek();
	snd_seq_event_t *pEv2 = m_postedBuffer.peek();

	// Direct events...
	while (pEv0) {
		m_pBuffer[m_iBuffer++] = *pEv0;
		pEv0 = m_directBuffer.next();
	}

	// Queued/posted events...
	while ((pEv1 && pEv1->time.tick < iTimeEnd)
		|| (pEv2 && pEv2->time.tick < iTimeEnd)) {
		while (pEv1 && pEv1->time.tick < iTimeEnd
			&& ((pEv2 && pEv2->time.tick >= pEv1->time.tick) || !pEv2)) {
			m_pBuffer[m_iBuffer] = *pEv1;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv1->time.tick > iTimeStart
					? pEv1->time.tick - iTimeStart : 0);
			pEv1 = m_queuedBuffer.next();
		}
		while (pEv2 && pEv2->time.tick < iTimeEnd
			&& ((pEv1 && pEv1->time.tick >= pEv2->time.tick) || !pEv1)) {
			m_pBuffer[m_iBuffer] = *pEv2;
			m_pBuffer[m_iBuffer++].time.tick
				= (pEv2->time.tick > iTimeStart
					? pEv2->time.tick - iTimeStart : 0);
			pEv2 = m_postedBuffer.next();
		}
	}

#ifdef CONFIG_DEBUG
	for (unsigned int i = 0; i < m_iBuffer; ++i) {
		snd_seq_event_t *pEv = &m_pBuffer[i];
		// - show event for debug purposes...
		fprintf(stderr, "MIDI Seq %05d 0x%02x", pEv->time.tick, pEv->type);
		if (pEv->type == SND_SEQ_EVENT_SYSEX) {
			fprintf(stderr, " sysex {");
			unsigned char *data = (unsigned char *) pEv->data.ext.ptr;
			for (unsigned int i = 0; i < pEv->data.ext.len; i++)
				fprintf(stderr, " %02x", data[i]);
			fprintf(stderr, " }\n");
		} else {
			for (unsigned int i = 0; i < sizeof(pEv->data.raw8.d); i++)
				fprintf(stderr, " %3d", pEv->data.raw8.d[i]);
			fprintf(stderr, "\n");
		}
	}
#endif

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
	m_postedBuffer.clear();

	clear();

	m_pPluginList->resetBuffer();

	m_iPendingBankMSB = -1;
	m_iPendingBankLSB = -1;
	m_iPendingProg    = -1;
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
