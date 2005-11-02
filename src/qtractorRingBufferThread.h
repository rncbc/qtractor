// qtractorRingBufferThread.h
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

#ifndef __qtractorRingBufferThread_h
#define __qtractorRingBufferThread_h

#include <qthread.h>
#include <qptrlist.h>

class qtractorRingBufferBase;


//----------------------------------------------------------------------
// class qtractorRingBufferThread -- Ring-cache manager thread (singleton).
//

class qtractorRingBufferThread : public QThread
{
public:

	// Singleton instance accessor.
	static qtractorRingBufferThread& Instance();
	// Singleton destroyer.
	static void Destroy();

	// Audio file list manager methods.
	void attach(qtractorRingBufferBase *pRingBuffer);
	void detach(qtractorRingBufferBase *pRingBuffer);

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync();

protected:

	// Constructor.
	qtractorRingBufferThread();

	// The main thread executive.
	void run();

private:

	// The singleton instance.
	static qtractorRingBufferThread* g_pInstance;

	// Whether the thread is logically running.
	bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;

	// The list of managed audio buffers.
	QPtrList<qtractorRingBufferBase> m_list;
};


#endif  // __qtractorRingBufferThread_h


// end of qtractorRingBufferThread.h
