// qtractorList.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorList_h
#define __qtractorList_h


//----------------------------------------------------------------------
// class qtractorList -- Doubly-linked list base.
//

template <class Node>
class qtractorList
{
public:

	// Default constructor.
	qtractorList() : m_pFirst(0), m_pLast(0), m_iCount(0),
		m_pFreeList(0), m_bAutoDelete(false) {}
	// Default destructor.
	~qtractorList() { clear(); }

	// Accessors.
	Node *first() const { return m_pFirst; }
	Node *last()  const { return m_pLast;  }

	int count() const { return m_iCount; }

	// Property accessors.
	void setAutoDelete(bool bAutoDelete) { m_bAutoDelete = bAutoDelete; }
	bool autoDelete() const { return m_bAutoDelete; }

	// Insert methods.
	void insertAfter(Node *pNode, Node *pPrevNode = 0);
	void insertBefore(Node *pNode, Node *pNextNode = 0);

	// Unlink/remove methods.
	void unlink(Node *pNode);
	void remove(Node *pNode);

	// Reset method.
	void clear();

	// Random accessors.
	Node *at (int iNode) const;

	// Aliased methods and operators.
	void prepend(Node *pNode) { insertBefore(pNode); }
	void append(Node *pNode)  { insertAfter(pNode);  }

	Node *operator[] (int iNode) const { return at(iNode); }

	// Node searcher.
	int find(Node *pNode) const;

	//----------------------------------------------------------------------
	// class qtractorList<Node>::Link -- Base list node.
	//

	class Link
	{
	public:

		// Constructor.
		Link() : m_pPrev(0), m_pNext(0), m_pNextFree(0) {}

		// Linked node getters.
		Node *prev() const { return m_pPrev; }
		Node *next() const { return m_pNext; }

		// Linked node setters.
		void setPrev(Node *pPrev) { m_pPrev = pPrev; }
		void setNext(Node *pNext) { m_pNext = pNext; }

		// Linked free node accessors.
		Node *nextFree() const { return m_pNextFree; }
		void setNextFree(Node *pNextFree) { m_pNextFree = pNextFree; }

	private:

		// Instance variables.
		Node *m_pPrev;
		Node *m_pNext;

		Node *m_pNextFree;
	};

	//----------------------------------------------------------------------
	// class qtractorList<Node>::Iterator -- List iterator (aka cursor).
	//

	class Iterator
	{
	public:

		// Constructors.
		Iterator(qtractorList<Node>& list)
			: m_list(list), m_pNode(0) {}
		Iterator(const Iterator& it)
			: m_list(it.list()), m_pNode(it.node()) {}

		// Iterator methods.
		Iterator& first() { m_pNode = m_list.first(); return *this; }
		Iterator& next()  { m_pNode = m_pNode->next(); return *this; }
		Iterator& prev()  { m_pNode = m_pNode->prev(); return *this; }
		Iterator& last()  { m_pNode = m_list.last(); return *this; }

		// Operator methods

		Iterator& operator= (const Iterator& iter)
			{ m_pNode = iter.m_pNode; return *this; }
		Iterator& operator= (Node *pNode)
			{ m_pNode = pNode; return *this; }

		Iterator& operator++ ()
			{ return next(); }
		Iterator  operator++ (int)
			{ Iterator it(*this); next(); return it; }

		Iterator& operator-- ()
			{ return prev(); }
		Iterator  operator-- (int)
			{ Iterator it(*this); prev(); return it; }

		// Simple accessors.
		const qtractorList<Node>& list() const { return m_list; }
		Node *node() const { return m_pNode; }

	private:

		// Instance variables.
		qtractorList<Node>& m_list;
		// Current cursory node reference.
		Node *m_pNode;
	};

private:

	// Instance variables.
	Node *m_pFirst;
	Node *m_pLast;
	int m_iCount;
	// The reclaimed freelist.
	Node *m_pFreeList;
	bool m_bAutoDelete;
};


// Insert methods.

template <class Node>
void qtractorList<Node>::insertAfter ( Node *pNode, Node *pPrevNode )
{
	if (pPrevNode == 0)
		pPrevNode = m_pLast;
	
	pNode->setPrev(pPrevNode);
	if (pPrevNode) {
		pNode->setNext(pPrevNode->next());
		if (pPrevNode->next())
			(pPrevNode->next())->setPrev(pNode);
		else
			m_pLast = pNode;
		pPrevNode->setNext(pNode);
	} else {
		m_pFirst = m_pLast = pNode;
		pNode->setNext(0);
	}

	++m_iCount;
}

template <class Node>
void qtractorList<Node>::insertBefore ( Node *pNode, Node *pNextNode )
{
	if (pNextNode == 0)
		pNextNode = m_pFirst;

	pNode->setNext(pNextNode);
	if (pNextNode) {
		pNode->setPrev(pNextNode->prev());
		if (pNextNode->prev())
			(pNextNode->prev())->setNext(pNode);
		else
			m_pFirst = pNode;
		pNextNode->setPrev(pNode);
	} else {
		m_pLast = m_pFirst = pNode;
		pNode->setPrev(0);
	}

	++m_iCount;
}


// Unlink method.
template <class Node>
void qtractorList<Node>::unlink ( Node *pNode )
{
	if (pNode->prev())
		(pNode->prev())->setNext(pNode->next());
	else
		m_pFirst = pNode->next();

	if (pNode->next())
		(pNode->next())->setPrev(pNode->prev());
	else
		m_pLast = pNode->prev();

	--m_iCount;
}


// Remove method.
template <class Node>
void qtractorList<Node>::remove ( Node *pNode )
{
	unlink(pNode);

	// Add it to the alternate free list.
	if (m_bAutoDelete) {
		Node *pNextFree = m_pFreeList;
		pNode->setNextFree(pNextFree);
		m_pFreeList = pNode;
	}
}


// Reset methods.
template <class Node>
void qtractorList<Node>::clear (void)
{
	// Remove pending items.
	Node *pLast = m_pLast;
	while (pLast) {
		remove(pLast);
		pLast = m_pLast;
	}

	// Free the free-list altogether...
	Node *pFreeList = m_pFreeList;
	while (pFreeList) {
		Node *pNextFree = pFreeList->nextFree();
		delete pFreeList;
		pFreeList = pNextFree;
	}

	// Force clen up.
	m_pFirst = m_pLast = 0;
	m_iCount = 0;
	m_pFreeList = 0;
}


// Random accessor.
template <class Node>
Node *qtractorList<Node>::at ( int iNode ) const
{
	int i;
	Node *pNode;

	if (iNode < 0 || iNode >= m_iCount)
		return 0;

	if (iNode > (m_iCount >> 1)) {
		for (i = m_iCount - 1, pNode = m_pLast;
				pNode && i > iNode;
					--i, pNode = pNode->prev())
			;
	} else {
		for (i = 0, pNode = m_pFirst;
				pNode && i < iNode;
					++i, pNode = pNode->next())
			;
	}

	return pNode;
}


// Node searcher.
template <class Node>
int qtractorList<Node>::find ( Node *pNode ) const
{
	int iNode = 0;
	Node *pNextNode = m_pFirst;

	while (pNextNode) {
		if (pNode == pNextNode)
			return iNode;
		pNextNode = pNextNode->next();
		++iNode;
	}

	return (-1);
}


#endif  // __qtractorList_h


// end of qtractorList.h
