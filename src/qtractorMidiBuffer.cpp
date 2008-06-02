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
#ifdef CONFIG_VST
	m_iVstRefCount(0),
	m_pVstMidiParser(NULL),
	m_pVstMidiBuffer(NULL),
	m_pVstBuffer(NULL),
#endif
	m_iPendingBankMSB(-1),
	m_iPendingBankLSB(-1),
	m_iPendingProg(-1)
{
	m_pBuffer = new snd_seq_event_t [bufferSize() << 1];
}

// Destructor.
qtractorMidiManager::~qtractorMidiManager (void)
{
#ifdef CONFIG_VST
	deleteVstMidiParser();
#endif

	if (m_pBuffer)
		delete [] m_pBuffer;
}


// Direct buffering.
bool qtractorMidiManager::direct ( snd_seq_event_t *pEvent )
{
	if (pEvent->type == SND_SEQ_EVENT_CONTROLLER) {
		int iController = pEvent->data.control.param;
		int iValue      = pEvent->data.control.value;
		switch (iController) {
		case BANK_SELECT_MSB:
			m_iPendingBankMSB = iValue;
			break;
		case BANK_SELECT_LSB:
			m_iPendingBankLSB = iValue;
			break;
		default:
			// Handle controller mappings...
			qtractorPlugin *pPlugin = m_pPluginList->first();
			while (pPlugin) {
				pPlugin->setController(iController, iValue);
				pPlugin = pPlugin->next();
			}
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


// Clears buffers for processing.
void qtractorMidiManager::clear (void)
{
	m_iBuffer = 0;

#ifdef CONFIG_VST
	if (m_pVstMidiParser) {
		snd_midi_event_reset_decode(m_pVstMidiParser);
		snd_midi_event_no_status(m_pVstMidiParser, 1);
		::memset(m_pVstBuffer, 0, sizeof(VstEvents));
	}
#endif
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
		qtractorPlugin *pPlugin = m_pPluginList->first();
		while (pPlugin) {
			pPlugin->selectProgram(iBank, iProg);
			pPlugin = pPlugin->next();
		}
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

#ifdef CONFIG_VST
	if (m_pVstMidiParser) {

		const unsigned int MaxVstMidiEvents = (bufferSize() << 1);

		VstEvents *pVstEvents = (VstEvents *) m_pVstBuffer;
		unsigned int iVstMidiEvent = 0;
	
		for (unsigned int i = 0; i < m_iBuffer; ++i) {
	
			snd_seq_event_t *pEv = &m_pBuffer[i];
	
			VstMidiEvent *pVstMidiEvent = &m_pVstMidiBuffer[iVstMidiEvent];
			::memset(pVstMidiEvent, 0, sizeof(VstMidiEvent));
	
			pVstMidiEvent->type = kVstMidiType;
			pVstMidiEvent->byteSize = sizeof(VstMidiEvent);
			pVstMidiEvent->deltaFrames = pEv->time.tick;
	
			if (snd_midi_event_decode(m_pVstMidiParser,
				(unsigned char *) &pVstMidiEvent->midiData[0], 3, pEv) < 0)
				break;
	
			pVstEvents->events[iVstMidiEvent] = (VstEvent *) pVstMidiEvent;
	
			if (++iVstMidiEvent >= MaxVstMidiEvents)
				break;
		}
	
		pVstEvents->numEvents = iVstMidiEvent;
		pVstEvents->reserved = 0;
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
	m_pSession->lock();

	m_directBuffer.clear();
	m_queuedBuffer.clear();
	m_postedBuffer.clear();

	clear();

	m_pPluginList->resetBuffer();

	m_iPendingBankMSB = -1;
	m_iPendingBankLSB = -1;
	m_iPendingProg    = -1;

	m_pSession->unlock();
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


#ifdef CONFIG_VST

void qtractorMidiManager::createVstMidiParser (void)
{
	if (m_pVstMidiParser)
		return;

	if (snd_midi_event_new(3, &m_pVstMidiParser) == 0) {
		const unsigned int MaxVstMidiEvents = (bufferSize() << 1);
		const unsigned int VstBufferSize = sizeof(VstEvents)
			+ MaxVstMidiEvents * sizeof(VstMidiEvent *);
		m_pVstBuffer = new unsigned char [VstBufferSize];
		m_pVstMidiBuffer = new VstMidiEvent [MaxVstMidiEvents];
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiManager[%p]::createVstMidiParser(%p)",
		this, m_pVstMidiParser);
#endif
}


void qtractorMidiManager::deleteVstMidiParser (void)
{
	if (m_pVstMidiParser == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiManager[%p]::deleteVstMidiParser(%p)",
		this, m_pVstMidiParser);
#endif

	if (m_pVstMidiBuffer) {
		delete [] m_pVstMidiBuffer;
		m_pVstMidiBuffer = NULL;
	}

	if (m_pVstBuffer) {
		delete [] m_pVstBuffer;
		m_pVstBuffer = NULL;
	}

	snd_midi_event_free(m_pVstMidiParser);
	m_pVstMidiParser = NULL;
}

#endif // CONFIG_VST


// Plugin reference counting.
void qtractorMidiManager::addPluginRef ( qtractorPlugin *pPlugin )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiManager[%p]::addPluginRef(%p)", this, pPlugin);
#endif
	if ((pPlugin->type())->typeHint() == qtractorPluginType::Vst) {
#ifdef CONFIG_VST
		if (++m_iVstRefCount == 1)
			createVstMidiParser();
#endif
	}
}


void qtractorMidiManager::removePluginRef ( qtractorPlugin *pPlugin )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiManager[%p]::removePluginRef(%p)", this, pPlugin);
#endif
	if ((pPlugin->type())->typeHint() == qtractorPluginType::Vst) {
#ifdef CONFIG_VST
		if (--m_iVstRefCount == 0)
			deleteVstMidiParser();
#endif
	}
}


// Instrument map builder.
void qtractorMidiManager::updateInstruments (void)
{
	m_instruments.clear();

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		int iIndex = 0;
		qtractorPlugin::Program program;
		Banks& banks = m_instruments[(pPlugin->type())->name()];
		while (pPlugin->getProgram(iIndex++, program)) {
			Bank& bank = banks[program.bank];
			if (bank.name.isEmpty()) {
				bank.name = QObject::tr("%1 - Bank %2")
					.arg(program.bank)
					.arg(banks.count() - 1);
			}
			bank.progs[program.prog] = program.name.simplified();
		}
	}
}


// end of qtractorMidiBuffer.cpp
