// qtractorCurve.h
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

#ifndef __qtractorCurve_h
#define __qtractorCurve_h

#include "qtractorObserver.h"
#include "qtractorMidiSequence.h"

#include <QColor>


// Forward declarations.
class qtractorTimeScale;

class qtractorCurveList;
class qtractorCurveEditList;


//----------------------------------------------------------------------
// class qtractorCurve -- The generic curve declaration.
//

class qtractorCurve : public qtractorList<qtractorCurve>::Link
{
public:

	// Curve modes.
	enum Mode { Hold = 0, Linear = 1, Spline = 2 };

	// Constructor.
	qtractorCurve(qtractorCurveList *pList, qtractorSubject *pSubject,
		Mode mode, unsigned int iMinFrameDist = 3200);

	// Destructor.
	~qtractorCurve();

	// Curve list owner accessor.
	qtractorCurveList *list() const
		{ return m_pList; }

	// Curve subject accessors.
	void setSubject(qtractorSubject *pSubject)
		{ m_observer.setSubject(pSubject); }
	qtractorSubject *subject() const
		{ return m_observer.subject(); }

	// Curve mode accessor.
	void setMode(Mode mode)
		{ m_mode = (m_observer.isToggled() ? Hold : mode); }
	Mode mode() const
		{ return (m_observer.isToggled() ? Hold : m_mode); }

	// Minimum distance between adjacent nodes accessors.
	void setMinFrameDist(unsigned int iMinFrameDist)
		{ m_iMinFrameDist = iMinFrameDist; }
	unsigned int minFrameDist() const
		{ return m_iMinFrameDist; }

	// The curve node declaration.
	struct Node : public qtractorList<Node>::Link
	{
		// Constructor.
		Node(unsigned long iFrame = 0, float fValue = 0.0f)
			: frame(iFrame), value(fValue),
				a(0.0f), b(0.0f), c(0.0f), d(0.0f) {}

		// Node members.
		unsigned long frame;
		float value;
		float a, b, c, d;
	};

	// Node list accessor.
	const qtractorList<Node>& nodes() const
		{ return m_nodes; }

	// Curve list reset method.
	void clear();

	// Node list management methods.
	Node *addNode(unsigned long iFrame, float fValue,
		qtractorCurveEditList *pEditList = NULL);

	void insertNode(Node *pNode);
	void unlinkNode(Node *pNode);
	void removeNode(Node *pNode);

	// Master seeker method.
	Node *seek(unsigned long iFrame)
		{ return m_cursor.seek(iFrame); }

	// Common interpolate methods.
	float value(const Node *pNode, unsigned long iFrame) const;
	float value(unsigned long iFrame);

	// Normalized scale converters.
	float valueFromScale(float fScale) const;
	float scaleFromValue(float fValue) const;

	// Common normalized methods.
	float scale(const Node *pNode, unsigned long iFrame) const
		{ return scaleFromValue(value(pNode, iFrame)); }
	float scale(unsigned long iFrame)
		{ return scaleFromValue(value(iFrame)); }

	// Default value accessors.
	void setDefaultValue(float fDefaultValue);
	float defaultValue() const
		{ return m_tail.value; }

	// Default length accessors.
	void setLength(unsigned long iLength);
	unsigned long length() const
		{ return m_tail.frame; }

	// Refresh all coefficients.
	void update();

	// To optimize and keep track of current frame
	// position, mostly like a sequence cursor/iterator.
	class Cursor
	{
	public:

		// Constructor.
		Cursor(qtractorCurve *pCurve)
			: m_pCurve(pCurve), m_pNode(NULL), m_iFrame(0) {}
		
		// Accessors.
		qtractorCurve *curve() const { return m_pCurve; }
		unsigned long  frame() const { return m_iFrame; }
	
		// Specific methods.
		Node *seek(unsigned long iFrame);
		void reset(Node *pNode = NULL);

		// Interpolate methods.
		float value(const Node *pNode, unsigned long iFrame) const
			{ return m_pCurve->value(pNode, iFrame); }
		float value(unsigned long iFrame)
			{ return value(seek(iFrame), iFrame); }

		// Normalized methods.
		float scale(const Node *pNode, unsigned long iFrame) const
			{ return m_pCurve->scale(pNode, iFrame); }
		float scale(unsigned long iFrame)
			{ return scale(seek(iFrame), iFrame); }

		float scale(const Node *pNode) const
			{ return m_pCurve->scaleFromValue(pNode->value); }

	private:

		// Member variables.
		qtractorCurve *m_pCurve;
		Node          *m_pNode;
		unsigned long  m_iFrame;
	};

	// Internal cursor accessor.
	const Cursor& cursor() const { return m_cursor; }

	// Curve state flags.
	enum State { Idle = 0, Process = 1, Capture = 2, Locked = 4 };

	bool isIdle() const
		{ return (m_state == Idle); }
	bool isProcess() const
		{ return (m_state & Process); }
	bool isCapture() const
		{ return (m_state & Capture); }
	bool isLocked() const
		{ return (m_state & Locked); }

	// Ccapture/process state settlers.
	void setCapture(bool bCapture);
	void setProcess(bool bProcess);
	void setLocked(bool bLocked);

	// The meta-processing automation procedure.
	void process(unsigned long iFrame)
	{
		if (isProcess()) {
			Node *pNode = seek(iFrame);
			if (!isCapture())
				m_observer.setValue(value(pNode, iFrame));
		}
	}

	void process() { process(m_cursor.frame()); }

	// Record automation procedure.
	void capture(unsigned long iFrame)
	{
		if (isCapture())
			addNode(iFrame, m_observer.value(), m_pEditList);
	}

	void capture() { capture(m_cursor.frame()); }

	qtractorCurveEditList *editList() const
		{ return m_pEditList; }

	// Convert MIDI sequence events to curve nodes.
	void readMidiSequence(qtractorMidiSequence *pSeq,
		qtractorMidiEvent::EventType ctype, unsigned short iChannel,
		unsigned short iParam, qtractorTimeScale *pTimeScale);

	// Convert curve nodes to MIDI sequence.
	void writeMidiSequence(qtractorMidiSequence *pSeq,
		qtractorMidiEvent::EventType ctype, unsigned short iChannel,
		unsigned short iParam, qtractorTimeScale *pTimeScale) const;

	// Logarithmic scale mode accessors.
	void setLogarithmic(bool bLogarithmic)
		{ m_bLogarithmic = bLogarithmic; }
	bool isLogarithmic() const
		{ return m_bLogarithmic; }

	// Curve mode accessor.
	void setColor(const QColor& color)
		{ m_color = color; }
	const QColor& color() const
		{ return m_color; }

    // Emptyness status.
    bool isEmpty() const
        { return (m_nodes.count() < 1); }

protected:

	// Snap to minimum distance frame.
	unsigned long frameDist(unsigned long iFrame) const;

	// Node interpolation coefficients updater.
	void updateNode(Node *pNode);
	void updateNodeEx(Node *pNode);

	// Observer for capture.
	class Observer : public qtractorObserver
	{
	public:

		// Constructor.
		Observer(qtractorSubject *pSubject, qtractorCurve *pCurve)
		    : qtractorObserver(pSubject), m_pCurve(pCurve) {}

		// Capture updater.
		void update(bool) { m_pCurve->capture(); }

	private:

		// Own curve reference.
		qtractorCurve *m_pCurve;
	};

private:

	// Curve list owner.
	qtractorCurveList *m_pList;

	// Curve mode.
	Mode m_mode;

	// Minimum distance between adjacent nodes.
	unsigned int m_iMinFrameDist;

	// Capture observer.
	Observer m_observer;

	// The curve node list.
	qtractorList<Node> m_nodes;

	// Default (initial/final) node.
	Node m_tail;

	// Curve state.
	State m_state;

	// Optimizing cursor.
	Cursor m_cursor;

	// Logarithmic scale mode accessors.
	bool m_bLogarithmic;

	// Curve color.
	QColor m_color;

	// Capture (record) edit list.
	qtractorCurveEditList *m_pEditList;
};


//----------------------------------------------------------------------------
// qtractorCurveListProxy -- Automation signal/slot notifier.
//

class qtractorCurveListProxy : public QObject
{
	Q_OBJECT

public:

	qtractorCurveListProxy(QObject *pParent = NULL)
		: QObject(pParent) {}

	void notify() { emit update(); }

signals:

	void update();
};


//----------------------------------------------------------------------
// qtractorCurveEditList -- Curve node edit (command) list.

class qtractorCurveEditList
{
public:

	// Constructor.
	qtractorCurveEditList(qtractorCurve *pCurve)
		: m_pCurve(pCurve) {}

	// Copy cnstructor.
	qtractorCurveEditList(const qtractorCurveEditList& list)
		: m_pCurve(list.curve()) { append(list); }

	// Destructor
	~qtractorCurveEditList() { clear(); }

	// Curve accessor.
	qtractorCurve *curve() const
		{ return m_pCurve; }

	// List predicate accessor.
	bool isEmpty() const
		{ return m_items.isEmpty(); }

	// List methods.
	void addNode(qtractorCurve::Node *pNode)
		{ m_items.append(new Item(AddNode, pNode, pNode->frame)); }
	void moveNode(qtractorCurve::Node *pNode, unsigned long iFrame)
		{ m_items.append(new Item(MoveNode, pNode, iFrame)); }
	void removeNode(qtractorCurve::Node *pNode)
		{ m_items.append(new Item(RemoveNode, pNode)); }

	// List appender.
	void append(const qtractorCurveEditList& list)
	{
		QListIterator<Item *> iter(list.m_items);
		while (iter.hasNext())
			m_items.append(new Item(*iter.next()));
	}

	// List cleanup.
	void clear()
	{
		QListIterator<Item *> iter(m_items);
		while (iter.hasNext()) {
			Item *pItem = iter.next();
			if (pItem->autoDelete)
				delete pItem->node;
		}

		qDeleteAll(m_items);
		m_items.clear();
	}

	// Curve edit list command executive.
	bool execute(bool bRedo = true);

protected:

	// Primitive command types.
	enum Command {
		AddNode, MoveNode, RemoveNode
	};

	// Curve item struct.
	struct Item
	{
		// Item constructor.
		Item(Command cmd, qtractorCurve::Node *pNode,
			unsigned long iFrame = 0) : command(cmd), node(pNode),
				frame(iFrame), value(pNode->value), autoDelete(false) {}

		// Item copy constructor.
		Item(const Item& item)
			: command(item.command), node(item.node),
				frame(item.frame), value(item.value),
				autoDelete(item.autoDelete) {}

		// Item members.
		Command command;
		qtractorCurve::Node *node;
		unsigned long frame;
		float value;
		bool autoDelete;
	};

private:

	// Instance variables.
	qtractorCurve *m_pCurve;
	QList<Item *>  m_items;
};


//----------------------------------------------------------------------------
// qtractorCurveList -- Automation item list
//

class qtractorCurveList : public qtractorList<qtractorCurve>
{
public:

	// Constructor.
	qtractorCurveList() : m_iProcess(0), m_iCapture(0), m_iLocked(0),
		m_pCurrentCurve(NULL) { setAutoDelete(true); }

	// ~Destructor.
	~qtractorCurveList() { clearAll(); }

	// Simple list methods.
	void addCurve(qtractorCurve *pCurve)
		{ append(pCurve); }

	void removeCurve(qtractorCurve *pCurve)
	{
		if (m_pCurrentCurve == pCurve)
			m_pCurrentCurve =  NULL;

		if (pCurve->isProcess())
			updateProcess(false);
		if (pCurve->isCapture())
			updateCapture(false);
		if (pCurve->isLocked())
			updateLocked(false);

	//	remove(pCurve);
	}

	// Set common curve length procedure.
	void setLength(unsigned long iLength)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->setLength(iLength);
			pCurve = pCurve->next();
		}
	}

	unsigned long length() const
		{ return (first() ? first()->length() : 0); }

	// Record automation procedure.
	void capture(unsigned long iFrame)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->capture(iFrame);
			pCurve = pCurve->next();
		}
	}

	// The meta-processing automation procedure.
	void process(unsigned long iFrame)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->process(iFrame);
			pCurve = pCurve->next();
		}
	}

	// Process management.
	void updateProcess(bool bProcess)
	{
		if (bProcess)
			++m_iProcess;
		else
		if (!bProcess)
			--m_iProcess;
	}

	void setProcessAll(bool bProcess)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->setProcess(bProcess);
			pCurve = pCurve->next();
		}
	}

	bool isProcessAll() const
		{ return isProcess() && m_iProcess >= count(); }
	bool isProcess() const
		{ return m_iProcess > 0; }

	// Capture management.
	void updateCapture(bool bCapture)
	{
		if (bCapture)
			++m_iCapture;
		else
			--m_iCapture;
	}

	void setCaptureAll(bool bCapture)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->setCapture(bCapture);
			pCurve = pCurve->next();
		}
	}

	bool isCaptureAll() const
		{ return isCapture() && m_iCapture >= count(); }
	bool isCapture() const
		{ return m_iCapture > 0; }

	// Locked management.
	void updateLocked(bool bLocked)
	{
		if (bLocked)
			++m_iLocked;
		else
			--m_iLocked;
	}

	void setLockedAll(bool bLocked)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->setLocked(bLocked);
			pCurve = pCurve->next();
		}
	}

	bool isLockedAll() const
		{ return isLocked() && m_iLocked >= count(); }
	bool isLocked() const
		{ return m_iLocked > 0; }

	// Check whether there's any captured material.
	bool isEditListEmpty() const
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			qtractorCurveEditList *pEditList = pCurve->editList();
			if (pEditList && !pEditList->isEmpty())
				return false;
			pCurve = pCurve->next();
		}
		return true;
	}

    // Emptyness status.
    bool isEmpty() const
        { return (count() < 1); }

    // Whole list cleaner.
	void clearAll()
	{
		m_pCurrentCurve = NULL;

		clear();

		m_iCapture = 0;
		m_iProcess = 0;
		m_iLocked  = 0;
	}

	// Signal/slot notifier accessor.
	qtractorCurveListProxy *proxy()
		{ return &m_proxy; }

	// Signal/slot notification.
	void notify() { m_proxy.notify(); }

	// Current curve accessors.
	void setCurrentCurve(qtractorCurve *pCurve)
		{ m_pCurrentCurve = pCurve; }
	qtractorCurve *currentCurve() const
		{ return m_pCurrentCurve; }

private:

	// Mass capture/process state counters.
	int m_iProcess;
	int m_iCapture;
	int m_iLocked;

	// Signal/slot notifier.
	qtractorCurveListProxy m_proxy;

	// Current selected curve.
	qtractorCurve *m_pCurrentCurve;
};


#endif  // __qtractorCurve_h


// end of qtractorCurve.h
