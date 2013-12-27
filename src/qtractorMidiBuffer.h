// qtractorMidiBuffer.h
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

#ifndef __qtractorMidiBuffer_h
#define __qtractorMidiBuffer_h

#include "qtractorAbout.h"
#include "qtractorList.h"

#ifdef CONFIG_VST
#include "qtractorVstPlugin.h"
#ifndef CONFIG_MIDI_PARSER
#define CONFIG_MIDI_PARSER 1
#endif
#endif

#if defined(CONFIG_LV2_EVENT) || defined(CONFIG_LV2_ATOM)
#include "qtractorLv2Plugin.h"
#ifndef CONFIG_MIDI_PARSER
#define CONFIG_MIDI_PARSER 1
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <alsa/asoundlib.h>

#include <QString>
#include <QMap>


// Forward declarations.
class qtractorTimeScale;
class qtractorPluginList;
class qtractorPlugin;
class qtractorAudioBus;

class qtractorMidiManagerThread;


//----------------------------------------------------------------------
// class qtractorMidiBuffer -- MIDI event FIFO buffer/cache declaration.
//

class qtractorMidiBuffer
{
public:

	// Minimum buffer size
	enum { MinBufferSize = 0x400 };

	// Constructor.
	qtractorMidiBuffer(unsigned int iBufferSize = MinBufferSize) :
		m_pBuffer(NULL), m_iBufferSize(0), m_iBufferMask(0),
		m_iWriteIndex(0), m_iReadIndex(0)
	{
		// Adjust size to nearest power-of-two, if necessary.
		m_iBufferSize = MinBufferSize;
		while (m_iBufferSize < iBufferSize)
			m_iBufferSize <<= 1;
		m_iBufferMask = (m_iBufferSize - 1);
		m_pBuffer = new snd_seq_event_t [m_iBufferSize];
	}

	// Destructor.
	~qtractorMidiBuffer() { if (m_pBuffer) delete [] m_pBuffer; }

	// Implementation properties.
	unsigned int bufferSize() const { return m_iBufferSize; }

	// Clears the buffer.
	void clear() { m_iReadIndex = m_iWriteIndex = 0; }	

	// Returns nonzero if there aren't any events available.
	bool isEmpty() const { return (m_iReadIndex == m_iWriteIndex); }

	// Returns a pointer to the first of the output events. 
	snd_seq_event_t *peek() const
		{ return (isEmpty() ? NULL : &m_pBuffer[m_iReadIndex]); }

	// Read next event from buffer.
	snd_seq_event_t *next()
	{
		if (!isEmpty())	++m_iReadIndex &= m_iBufferMask;
		return peek();
	}

	// Read event from buffer.
	snd_seq_event_t *pop()
	{
		unsigned int iReadIndex = m_iReadIndex;
		if (iReadIndex == m_iWriteIndex)
			return NULL;
		m_iReadIndex = (iReadIndex + 1) & m_iBufferMask;
		return &m_pBuffer[iReadIndex];
	}

	// Write event to buffer.
	bool push(snd_seq_event_t *pEvent, unsigned long iTick = 0)
	{
		unsigned int iWriteIndex = (m_iWriteIndex + 1) & m_iBufferMask;
		if (iWriteIndex == m_iReadIndex)
			return false;
		m_pBuffer[m_iWriteIndex] = *pEvent;
		m_pBuffer[m_iWriteIndex].time.tick = iTick;
		m_iWriteIndex = iWriteIndex;
		return true;
	}

	// Write event to buffer (ordered).
	bool insert(snd_seq_event_t *pEvent, unsigned long iTick = 0)
	{
		unsigned int iWriteIndex = (m_iWriteIndex + 1) & m_iBufferMask;
		if (iWriteIndex == m_iReadIndex)
			return false;
		unsigned int i = m_iWriteIndex;
		unsigned int j = i;
		for (;;) {
			--i &= m_iBufferMask;
			if (j == m_iReadIndex
				|| iTick >= m_pBuffer[i].time.tick) {
				m_pBuffer[j] = *pEvent;
				m_pBuffer[j].time.tick = iTick;
				break;
			}
			m_pBuffer[j] = m_pBuffer[i];
			j = i;
		}
		m_iWriteIndex = iWriteIndex;
		return true;
	}

	// Returns number of events currently available.
	unsigned int count() const
	{
		unsigned int iWriteIndex = m_iWriteIndex;
		unsigned int iReadIndex  = m_iReadIndex;
		if (iWriteIndex > iReadIndex) {
			return (iWriteIndex - iReadIndex);
		} else {
			return (iWriteIndex - iReadIndex + m_iBufferSize) & m_iBufferMask;
		}
	}

	// Reset events in buffer.
	void reset(unsigned long iTick = 0)
	{
		unsigned int i = m_iReadIndex;
		while (i != m_iWriteIndex) {
			m_pBuffer[i].time.tick = iTick;
			++i &= m_iBufferMask;
		}
	}

private:

	// Instance variables.
	snd_seq_event_t *m_pBuffer;
	unsigned int m_iBufferSize;
	unsigned int m_iBufferMask;
	unsigned int m_iWriteIndex;
	unsigned int m_iReadIndex;
};


//----------------------------------------------------------------------
// class qtractorMidiManager -- MIDI internal plugin list manager.
//

class qtractorMidiManager : public qtractorList<qtractorMidiManager>::Link
{
public:

	// Constructor.
	qtractorMidiManager(qtractorPluginList *pPluginList,
		unsigned int iBufferSize = qtractorMidiBuffer::MinBufferSize);

	// Destructor.
	~qtractorMidiManager();

	// Implementation properties.
	qtractorPluginList *pluginList() const
		{ return m_pPluginList; }

	unsigned int bufferSize() const
		{ return m_queuedBuffer.bufferSize(); }

	// Clears buffers for processing.
	void clear();

	// Event buffer accessor. 
	snd_seq_event_t *events() const { return m_pBuffer; }

	// Returns number of events result of process.
	unsigned int count() const { return m_iBuffer; }

	// Direct buffering.
	bool direct(snd_seq_event_t *pEvent);

	// Queued buffering.
	bool queued(qtractorTimeScale *pTimeScale,
		snd_seq_event_t *pEvent, unsigned long iTime, long iFrameStart);

	// Process buffers.
	void process(unsigned long iTimeStart, unsigned long iTimeEnd);

	// Process buffers (in asynchronous controller thread).
	void processSync();

	// Resets all buffering.
	void reset();

	// Sync thread state flags accessors.
	void setWaitSync(bool bWaitSync);
	bool isWaitSync() const;

	// Factory (proxy) methods.
	static qtractorMidiManager *createMidiManager(
		qtractorPluginList *pPluginList);
	static void deleteMidiManager(
		qtractorMidiManager *pMidiManager);

	// Some default factory options.
	static void setDefaultAudioOutputBus(bool bAudioOutputBus);
	static bool isDefaultAudioOutputBus();

	static void setDefaultAudioOutputAutoConnect(bool bAudioOutputAutoConnect);
	static bool isDefaultAudioOutputAutoConnect();

#ifdef CONFIG_VST
	// VST event buffer accessors...
	VstEvents *vst_events_in() const
		{ return (VstEvents *) m_ppVstBuffers[m_iEventBuffer & 1]; }
	VstEvents *vst_events_out() const
		{ return (VstEvents *) m_ppVstBuffers[(m_iEventBuffer + 1) & 1]; }
	// Copy VST event buffer (output)...
	void vst_events_copy(VstEvents *pVstBuffer);
	// Swap VST event buffers...
	void vst_events_swap();
#endif

#ifdef CONFIG_LV2_EVENT
	// LV2 event buffer accessors...
	LV2_Event_Buffer *lv2_events_in() const
		{ return m_ppLv2EventBuffers[m_iEventBuffer & 1]; }
	LV2_Event_Buffer *lv2_events_out() const
		{ return m_ppLv2EventBuffers[(m_iEventBuffer + 1) & 1]; }
	// Swap LV2 event buffers...
	void lv2_events_swap();
#endif

#ifdef CONFIG_LV2_ATOM
	// LV2 atom buffer accessors...
	LV2_Atom_Buffer *lv2_atom_buffer_in() const
		{ return m_ppLv2AtomBuffers[m_iEventBuffer & 1]; }
	LV2_Atom_Buffer *lv2_atom_buffer_out() const
		{ return m_ppLv2AtomBuffers[(m_iEventBuffer + 1) & 1]; }
	// Swap LV2 atom buffers...
	void lv2_atom_buffer_swap();
	// Resize LV2 atom buffers if necessary.
	void lv2_atom_buffer_resize(unsigned int iMinBufferSize);
#endif

	// Audio output bus mode accessors.
	void setAudioOutputBus(bool bAudioOutputBus);
	bool isAudioOutputBus() const
		{ return m_bAudioOutputBus; }
	void setAudioOutputBusName(const QString& sAudioOutputBusName)
		{ m_sAudioOutputBusName = sAudioOutputBusName; }
	const QString& audioOutputBusName() const
		{ return m_sAudioOutputBusName; }
	qtractorAudioBus *audioOutputBus() const
		{ return m_pAudioOutputBus; }
	void resetAudioOutputBus();

	// Audio output bus defaults accessors.
	void setAudioOutputAutoConnect(bool bAudioOutputAutoConnect)
		{ m_bAudioOutputAutoConnect = bAudioOutputAutoConnect; }
	bool isAudioOutputAutoConnect() const
		{ return m_bAudioOutputAutoConnect; }

	// Current bank selection accessors.
	void setCurrentBank(int iBank)
		{ m_iCurrentBank = iBank; }
	int currentBank() const
		{ return m_iCurrentBank; }

	// Current program selection accessors.
	void setCurrentProg(int iProg)
		{ m_iCurrentProg = iProg; }
	int currentProg() const
		{ return m_iCurrentProg; }

	// MIDI Instrument collection map-types.
	typedef QMap<int, QString> Progs;

	struct Bank
	{
		QString name;
		Progs   progs;
	};

	typedef QMap<int, Bank> Banks;
	typedef QMap<QString, Banks> Instruments;

	// Instrument map builder.
	void updateInstruments();

	// Instrument map accessor.
	const Instruments& instruments() const
		{ return m_instruments; }

protected:

	// Audio output (de)activation methods.
	void createAudioOutputBus();
	void deleteAudioOutputBus();

	// Swap event buffers (in for out and vice-versa)
	void swapEventBuffers();

private:

	// Instance variables
	qtractorPluginList *m_pPluginList;

	qtractorMidiBuffer  m_directBuffer;
	qtractorMidiBuffer  m_queuedBuffer;
	qtractorMidiBuffer  m_postedBuffer;

	qtractorMidiBuffer  m_controllerBuffer;

	snd_seq_event_t    *m_pBuffer;
	unsigned int        m_iBuffer;

	volatile bool       m_bWaitSync;

#ifdef CONFIG_MIDI_PARSER
	snd_midi_event_t   *m_pMidiParser;
#endif

	unsigned short      m_iEventBuffer;

#ifdef CONFIG_VST
	VstMidiEvent       *m_ppVstMidiBuffers[2];
	unsigned char      *m_ppVstBuffers[2];
#endif

#ifdef CONFIG_LV2_EVENT
	LV2_Event_Buffer   *m_ppLv2EventBuffers[2];
#endif
#ifdef CONFIG_LV2_ATOM
	LV2_Atom_Buffer    *m_ppLv2AtomBuffers[2];
	unsigned int        m_iLv2AtomBufferSize;
#endif

	bool                m_bAudioOutputBus;
	QString             m_sAudioOutputBusName;
	bool                m_bAudioOutputAutoConnect;
	qtractorAudioBus   *m_pAudioOutputBus;

	int m_iCurrentBank;
	int m_iCurrentProg;

	int m_iPendingBankMSB;
	int m_iPendingBankLSB;
	int m_iPendingProg;

	Instruments m_instruments;

	// Aync manager thread.
	static qtractorMidiManagerThread *g_pSyncThread;
	static unsigned int g_iSyncThreadRefCount;

	// Global factory options.
	static bool g_bAudioOutputBus;
	static bool g_bAudioOutputAutoConnect;
};


#endif  // __qtractorMidiBuffer_h

// end of qtractorMidiBuffer.h
