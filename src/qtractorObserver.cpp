// qtractorObserver.cpp
//
/****************************************************************************
   Copyright (C) 2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

	bool pop ()
	{
		if (m_iQueueIndex == 0)
			return false;
		QueueItem *pItem = &m_pQueueItems[--m_iQueueIndex];
		qtractorSubject *pSubject = pItem->subject;
		pSubject->setQueued(false);
		pSubject->notify(pItem->sender);
		return true;
	}

	void flush ()
		{ while (pop()) ; }

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
qtractorSubject::qtractorSubject ( float fValue )
	: m_fValue(fValue), m_bBusy(false), m_bQueued(false), m_fPrevValue(fValue)
{
}

// Destructor.
qtractorSubject::~qtractorSubject (void)
{
	m_bBusy = true;

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

	m_fValue = fValue;
}

float qtractorSubject::value (void) const
{
	return m_fValue;
}

float qtractorSubject::prevValue (void) const
{
	return m_fPrevValue;
}


// Busy flag predicate.
bool qtractorSubject::isBusy (void) const
{
	return m_bBusy;
}


// Observer/view updater.
void qtractorSubject::notify ( qtractorObserver *pSender )
{
	m_bBusy = true;

	QListIterator<qtractorObserver *> iter(m_observers);
	while (iter.hasNext()) {
		qtractorObserver *pObserver = iter.next();
		if (pSender && pSender == pObserver)
			continue;
		pObserver->update();
	}

	m_bBusy = false;
}


// Observer list accessors.
void qtractorSubject::attach ( qtractorObserver *pObserver )
{
	m_observers.append(pObserver);
}

void qtractorSubject::detach ( qtractorObserver *pObserver )
{
	m_observers.removeAll(pObserver);
}


const QList<qtractorObserver *>& qtractorSubject::observers (void) const
{
	return m_observers;
}


// Queue status accessors.
void qtractorSubject::setQueued ( bool bQueued )
{
	m_bQueued = bQueued;
}

bool qtractorSubject::isQueued (void) const
{
	return m_bQueued;
}


// Queue flush (singleton) -- notify all pending observers.
void qtractorSubject::flushQueue (void)
{
	g_subjectQueue.flush();
}


// Queue reset (clear).
void qtractorSubject::resetQueue (void)
{
	g_subjectQueue.reset();
}


//---------------------------------------------------------------------------
// qtractorObserver - Scalar parameter value control/view.

// Constructor.
qtractorObserver::qtractorObserver ( qtractorSubject *pSubject )
	: m_pSubject(pSubject)
{
	if (m_pSubject) m_pSubject->attach(this);
}

// Virtual destructor.
qtractorObserver::~qtractorObserver (void)
{
	if (m_pSubject) m_pSubject->detach(this);
}


// Subject value accessor.
void qtractorObserver::setSubject ( qtractorSubject *pSubject )
{
	if (m_pSubject /* && pSubject*/)
		m_pSubject->detach(this);
	
	m_pSubject = pSubject;

	if (m_pSubject)
		m_pSubject->attach(this);
}

qtractorSubject *qtractorObserver::subject (void) const
{
	return m_pSubject;
}


// Indirect value accessors.
void qtractorObserver::setValue ( float fValue )
{
	if (m_pSubject) m_pSubject->setValue(fValue, this);
}

float qtractorObserver::value (void) const
{
	return (m_pSubject ? m_pSubject->value() : 0.0f);
}


float qtractorObserver::prevValue (void) const
{
	return (m_pSubject ? m_pSubject->prevValue() : 0.0f);
}


// Busy flag predicate.
bool qtractorObserver::isBusy (void) const
{
	return (m_pSubject ? m_pSubject->isBusy() : true);
}


// end of qtractorObserver.cpp
