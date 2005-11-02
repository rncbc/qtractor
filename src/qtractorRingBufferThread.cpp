// qtractorRingBufferThread.cpp
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

#include "qtractorRingBuffer.h"


//----------------------------------------------------------------------
// class qtractorRingBufferThread -- Ring-cache manager thread (singleton).
//

// Initialize singleton instance pointer.
qtractorRingBufferThread *qtractorRingBufferThread::g_pInstance = NULL;


// Singleton instance accessor.
qtractorRingBufferThread& qtractorRingBufferThread::Instance (void)
{
	if (g_pInstance == NULL) {
		// Create the singleton instance...
		g_pInstance = new qtractorRingBufferThread();
		std::atexit(Destroy);
		g_pInstance->start(QThread::HighPriority);
		// Give it a bit of time to startup...
		QThread::msleep(100);
 #ifdef DEBUG
		fprintf(stderr, "qtractorRingBufferThread::Instance(%p)\n", g_pInstance);
 #endif
	}
	return *g_pInstance;
}


// Singleton instance destroyer.
void qtractorRingBufferThread::Destroy (void)
{
	if (g_pInstance) {
#ifdef DEBUG
		fprintf(stderr, "qtractorRingBufferThread::Destroy(%p)\n", g_pInstance);
#endif
		// Try to wake and terminate executive thread.
		g_pInstance->setRunState(false);
		if (g_pInstance->running())
			g_pInstance->sync();
		// Give it a bit of time to cleanup...
		QThread::msleep(100);
		// OK. We're done with ourselves.
		delete g_pInstance;
		g_pInstance = NULL;
	}
}


// Constructor.
qtractorRingBufferThread::qtractorRingBufferThread (void)
	: QThread()
{
	m_bRunState = false;
	m_list.setAutoDelete(false);
}


// Ring-cache list manager methods.
void qtractorRingBufferThread::attach ( qtractorRingBufferBase *pRingBuffer )
{
	QMutexLocker locker(&m_mutex);

	if (m_list.find(pRingBuffer) < 0)
		m_list.append(pRingBuffer);
}

void qtractorRingBufferThread::detach ( qtractorRingBufferBase *pRingBuffer )
{
	QMutexLocker locker(&m_mutex);

	if (m_list.find(pRingBuffer) >= 0)
		m_list.remove(pRingBuffer);
}



// Run state accessor.
void qtractorRingBufferThread::setRunState ( bool bRunState )
{
	m_bRunState = bRunState;
}

bool qtractorRingBufferThread::runState (void) const
{
	return m_bRunState;
}


// Wake from executive wait condition.
void qtractorRingBufferThread::sync (void)
{
	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef DEBUG_0
	else fprintf(stderr, "qtractorRingBufferThread::sync(%p): tryLock() failed.\n", this);
	msleep(5);
#endif
}


// Thread run executive.
void qtractorRingBufferThread::run (void)
{
#ifdef DEBUG
	fprintf(stderr, "qtractorRingBufferThread::run(%p): started.\n", this);
#endif

	m_bRunState = true;

	m_mutex.lock();
	while (m_bRunState) {
		qtractorRingBufferBase *pRingBuffer = m_list.first();
		while (pRingBuffer) {
			pRingBuffer->sync();
			pRingBuffer = m_list.next();
		}
		m_cond.wait(&m_mutex);
	}
	m_mutex.unlock();

#ifdef DEBUG
	fprintf(stderr, "qtractorRingBufferThread::run(%p): stopped.\n", this);
#endif
}


// end of qtractorRingBufferThread.cpp
