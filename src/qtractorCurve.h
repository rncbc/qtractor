// qtractorCurve.h
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

	// Curve subject accessor.
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
	Node *addNode(unsigned long iFrame, float fValue, bool bSmooth = true);
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

	// Curve state.
	enum State { Idle = 0, Enabled = 1, Process = 2, Capture = 4 };

	bool isIdle() const
		{ return (m_state == Idle); }
	bool isEnabled() const
		{ return (m_state & Enabled); }
	bool isProcess() const
		{ return (m_state & Process); }
	bool isCapture() const
		{ return (m_state & Capture); }
	bool isProcessEnabled() const
		{ return isEnabled() && isProcess(); }
	bool isCaptureEnabled() const
		{ return isEnabled() && isCapture(); }

	// Enabled/capture/process state settlers.
	void setEnabled(bool bEnabled);
	void setCapture(bool bCapture);
	void setProcess(bool bProcess);

	// Record automation procedure.
	void capture(unsigned long iFrame)
		{ if (isCaptureEnabled()) addNode(iFrame, m_observer.value()); }
	void capture() { capture(m_cursor.frame()); }

	// The meta-processing automation procedure.
	void process(unsigned long iFrame)
	{
		if (isProcessEnabled()) {
			Node *pNode = seek(iFrame);
			if (!isCapture())
				m_observer.setValue(value(pNode, iFrame));
		}
	}

	void process() { process(m_cursor.frame()); }

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

	// Dirtyness status.
	bool isDirty() const
		{ return m_iDirtyCount > 0; }

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
		void update() { m_pCurve->capture(); }

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

	// Dirtyness counter.
	int m_iDirtyCount;
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


//----------------------------------------------------------------------------
// qtractorCurveList -- Automation item list
//

class qtractorCurveList : public qtractorList<qtractorCurve>
{
public:

	// Constructor.
	qtractorCurveList() : m_iEnabled(0),
		m_iCapture(0), m_iProcess(0), m_pCurrentCurve(NULL)
		{ setAutoDelete(true); }

	// ~Destructor.
	~qtractorCurveList() { clearAll(); }

	// Simple list methods.
	void addCurve(qtractorCurve *pCurve)
	{
		append(pCurve);

		pCurve->setEnabled(true);
	}

	void removeCurve(qtractorCurve *pCurve)
	{
		if (m_pCurrentCurve == pCurve)
			m_pCurrentCurve =  NULL;

		pCurve->setEnabled(false);

		if (pCurve->isProcess())
			updateProcess(false);
		if (pCurve->isCapture())
			updateCapture(false);

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

	// Enabled management.
	void updateEnabled(bool bEnabled)
	{
		if (bEnabled)
			++m_iEnabled;
		else
			--m_iEnabled;
	}

	void setEnabledAll(bool bEnabled)
	{
		qtractorCurve *pCurve = first();
		while (pCurve) {
			pCurve->setEnabled(bEnabled);
			pCurve = pCurve->next();
		}
	}

	bool isEnabledAll() const
		{ return isEnabled() && m_iEnabled >= count(); }
	bool isEnabled() const
		{ return m_iEnabled > 0; }

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
	bool isCaptureEnabled() const
		{ return isEnabled() && isCapture(); }

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
	bool isProcessEnabled() const
		{ return isEnabled() && isProcess(); }

	// Whole list cleaner.
	void clearAll()
	{
		m_pCurrentCurve = NULL;

		clear();

		m_iEnabled = 0;
		m_iCapture = 0;
		m_iProcess = 0;
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
	int m_iEnabled;
	int m_iCapture;
	int m_iProcess;

	// Signal/slot notifier.
	qtractorCurveListProxy m_proxy;

	// Current selected curve.
	qtractorCurve *m_pCurrentCurve;
};


#endif  // __qtractorCurve_h


// end of qtractorCurve.h
