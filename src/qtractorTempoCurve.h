// qtractorTempoCurve.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2018, spog aka Samo Pogačnik. All rights reserved.

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

#ifndef __qtractorTempoCurve_h
#define __qtractorTempoCurve_h

#include "qtractorObserver.h"
#include "qtractorMidiSequence.h"

#include <QColor>
#include <QObject>

#include "qtractorTimeScale.h"


// Forward declarations.
class qtractorTimeScale;

class qtractorTempoCurveEditList;


//----------------------------------------------------------------------
// class qtractorTempoCurve -- The tempo curve declaration.
//

class qtractorTempoCurve : public qtractorList<qtractorTempoCurve>::Link
{
public:

	// Constructor.
	qtractorTempoCurve(qtractorTimeScale *pTimeScale, qtractorSubject *pSubject);

	// Destructor.
	~qtractorTempoCurve();

	// Curve subject accessors.
	void setSubject(qtractorSubject *pSubject)
		{ m_observer.setSubject(pSubject); }
	qtractorSubject *subject() const
		{ return m_observer.subject(); }

	// Time-scale accessor.
	qtractorTimeScale *timeScale() const { return m_pTimeScale; }

	// Curve list reset method.
	void clear();

	// Master seeker method.
	qtractorTimeScale::Node *seek(unsigned long iFrame)
		{ return m_pTimeScale->cursor().seekFrame(iFrame); }

	// Common interpolate methods.
	float value(const qtractorTimeScale::Node *pNode, unsigned long iFrame) const;
	float value(unsigned long iFrame);

	// Normalized scale converters.
	float valueFromScale(float fScale) const;
	float scaleFromValue(float fValue) const;

	// Common normalized methods.
	float scale(const qtractorTimeScale::Node *pNode, unsigned long iFrame) const
		{ return scaleFromValue(value(pNode, iFrame)); }
	float scale(unsigned long iFrame)
		{ return scaleFromValue(value(iFrame)); }

	float scale(const qtractorTimeScale::Node *pNode) const
		{ return scaleFromValue(pNode->tempo); }

	// Refresh all coefficients.
	void update();

	// Curve state flags.
	enum State { Idle = 0, Process = 1, Locked = 2 };

	bool isIdle() const
		{ return (m_state == Idle); }
	bool isProcess() const
		{ return (m_state & Process); }
	bool isLocked() const
		{ return (m_state & Locked); }

	void setProcess(bool bProcess);
	void setLocked(bool bLocked);

	// The meta-processing automation procedure.
	void process(unsigned long iFrame)
	{
		if (isProcess()) {
			qtractorTimeScale::Node *pNode = seek(iFrame);
				m_observer.setValue(value(pNode, iFrame));
		}
	}

	// Curve mode accessor.
	void setColor(const QColor& color)
		{ m_color = color; }
	const QColor& color() const
		{ return m_color; }

	// Emptyness status.
	bool isEmpty() const {
		if (m_pTimeScale)
			return (m_pTimeScale->nodes().count() < 1);
		else
			return true;
	}

protected:
	// Node interpolation coefficients updater.
	void updateNode(qtractorTimeScale::Node *pNode);
	void updateNodeEx(qtractorTimeScale::Node *pNode);

	// Observer for capture.
	class Observer : public qtractorObserver
	{
	public:

		// Constructor.
		Observer(qtractorSubject *pSubject, qtractorTempoCurve *pTempoCurve)
		    : qtractorObserver(pSubject), m_pTempoCurve(pTempoCurve) {}

		// Capture updater.
		void update(bool) {}

	private:

		// Own curve reference.
		qtractorTempoCurve *m_pTempoCurve;
	};

private:
	// Instance variables...
	qtractorTimeScale *m_pTimeScale;

	// Capture observer.
	Observer m_observer;

	// Minimum distance between adjacent nodes.
	unsigned int m_iMinFrameDist;

	// Curve state.
	State m_state;

	// Curve color.
	QColor m_color;

	// Capture (record) edit list.
	qtractorTempoCurveEditList *m_pEditList;
};

//----------------------------------------------------------------------
// qtractorTempCurveEditList -- Curve node edit (command) list.

class qtractorTempoCurveEditList
{
public:

	// Constructor.
	qtractorTempoCurveEditList(qtractorTempoCurve *pTempoCurve)
		: m_pTempoCurve(pTempoCurve) {}

	// Copy cnstructor.
	qtractorTempoCurveEditList(const qtractorTempoCurveEditList& list)
		: m_pTempoCurve(list.tempoCurve()) { append(list); }

	// Destructor
	~qtractorTempoCurveEditList() { clear(); }

	// Curve accessor.
	qtractorTempoCurve *tempoCurve() const
		{ return m_pTempoCurve; }

	// List predicate accessor.
	bool isEmpty() const
		{ return m_items.isEmpty(); }

	// List methods.
	void addNode(qtractorTimeScale::Node *pNode)
		{ m_items.append(new Item(AddNode, pNode, pNode->frame)); }
	void moveNode(qtractorTimeScale::Node *pNode, float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor)
		{ m_items.append(new Item(MoveNode, pNode, fTempo, iBeatsPerBar, iBeatDivisor)); }
	void removeNode(qtractorTimeScale::Node *pNode)
		{ m_items.append(new Item(RemoveNode, pNode)); }

	// List appender.
	void append(const qtractorTempoCurveEditList& list)
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
		Item(Command cmd, qtractorTimeScale::Node *pNode,
			float fTempo = 120.0f, unsigned short iBeatsPerBar = 4, unsigned short iBeatDivisor = 2)
			: command(cmd), node(pNode),
				tempo(fTempo), beatsPerBar(iBeatsPerBar), beatDivisor(iBeatDivisor), autoDelete(false) {}

		// Item copy constructor.
		Item(const Item& item)
			: command(item.command), node(item.node),
				tempo(item.tempo),
				beatsPerBar(item.beatsPerBar), beatDivisor(item.beatDivisor),
				autoDelete(item.autoDelete) {}

		// Item members.
		Command command;
		qtractorTimeScale::Node *node;
		float tempo;
		unsigned short beatsPerBar;
		unsigned short beatDivisor;
		bool autoDelete;
	};

private:

	// Instance variables.
	qtractorTempoCurve *m_pTempoCurve;
	QList<Item *>  m_items;
};


#endif  // __qtractorTempCurve_h


// end of qtractorTempoCurve.h
