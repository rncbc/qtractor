// qtractorObserver.cpp
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

#include "qtractorAbout.h"
#include "qtractorObserver.h"


//---------------------------------------------------------------------------
// qtractorSubjectQueue - Update/notify subject queue.

class qtractorSubjectQueue
{
public:

	struct QueueItem
	{
		qtractorSubject  *subject;
		qtractorObserver *sender;
	};

	qtractorSubjectQueue ( unsigned int iQueueSize = 1024 )
		: m_iQueueIndex(0), m_iQueueSize(0), m_pQueueItems(NULL)
		{ resize(iQueueSize); }

	~qtractorSubjectQueue ()
		{ clear(); delete [] m_pQueueItems; }

	void clear()
		{ m_iQueueIndex = 0; }

	bool push ( qtractorSubject *pSubject, qtractorObserver *pSender )
	{
		if (m_iQueueIndex >= m_iQueueSize)
			return false;
		pSubject->setQueued(true);
		QueueItem *pItem = &m_pQueueItems[m_iQueueIndex++];
		pItem->subject = pSubject;
		pItem->sender  = pSender;
		return true;
	}

	bool pop (bool bUpdate)
	{
		if (m_iQueueIndex == 0)
			return false;
		QueueItem *pItem = &m_pQueueItems[--m_iQueueIndex];
		qtractorSubject *pSubject = pItem->subject;
		pSubject->notify(pItem->sender, bUpdate);
		pSubject->setQueued(false);
		return true;
	}

	void flush (bool bUpdate)
		{ while (pop(bUpdate)) ; }

	void reset ()
	{
		while (m_iQueueIndex > 0) {
			QueueItem *pItem = &m_pQueueItems[--m_iQueueIndex];
			(pItem->subject)->setQueued(false);
		}
		clear();
	}

	void resize ( unsigned int iNewSize )
	{
		QueueItem *pOldItems = m_pQueueItems;
		QueueItem *pNewItems = new QueueItem [iNewSize];
		if (pOldItems) {
			unsigned int iOldSize = m_iQueueIndex;
			if (iOldSize > iNewSize)
				iOldSize = iNewSize;
			if (iOldSize > 0)
				::memcpy(pNewItems, pOldItems, iOldSize * sizeof(QueueItem));
			m_iQueueSize  = iNewSize;
			m_pQueueItems = pNewItems;
			delete [] pOldItems;
		} else {
			m_iQueueSize  = iNewSize;
			m_pQueueItems = pNewItems;
		}
	}

private:

	unsigned int m_iQueueIndex;
	unsigned int m_iQueueSize;
	QueueItem   *m_pQueueItems;
};


// The local subject queue singleton.
static qtractorSubjectQueue g_subjectQueue;


//---------------------------------------------------------------------------
// qtractorSubject - Scalar parameter value model.

// Constructor.
qtractorSubject::qtractorSubject ( float fValue, float fDefaultValue )
	: m_fValue(fValue), m_bQueued(false), m_fPrevValue(fValue),
		m_fMinValue(0.0f), m_fMaxValue(1.0f), m_fDefaultValue(fDefaultValue),
		m_bToggled(false), m_pCurve(NULL)
{
}

// Destructor.
qtractorSubject::~qtractorSubject (void)
{
	QListIterator<qtractorObserver *> iter(m_observers);
	while (iter.hasNext())
		iter.next()->setSubject(NULL);

	m_observers.clear();
}


// Direct value accessors.
void qtractorSubject::setValue ( float fValue, qtractorObserver *pSender )
{
	if (fValue == m_fValue)
		return;

	if (!m_bQueued) {
		m_fPrevValue = m_fValue;
		g_subjectQueue.push(this, pSender);
	}

	m_fValue = safeValue(fValue);
}


// Observer/view updater.
void qtractorSubject::notify ( qtractorObserver *pSender, bool bUpdate )
{
	QListIterator<qtractorObserver *> iter(m_observers);
	while (iter.hasNext()) {
		qtractorObserver *pObserver = iter.next();
		if (pSender && pSender == pObserver)
			continue;
		pObserver->update(bUpdate);
	}
}


// Queue flush (singleton) -- notify all pending observers.
void qtractorSubject::flushQueue ( bool bUpdate )
{
	g_subjectQueue.flush(bUpdate);
}


// Queue reset (clear).
void qtractorSubject::resetQueue (void)
{
	g_subjectQueue.reset();
}


// end of qtractorObserver.cpp
