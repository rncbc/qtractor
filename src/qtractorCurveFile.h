// qtractorCurveFile.h
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

#ifndef __qtractorCurveFile_h
#define __qtractorCurveFile_h

#include "qtractorCurve.h"


// Forward declarations.
class qtractorDocument;

class QDomElement;


//----------------------------------------------------------------------
// class qtractorCurveFile -- Automation curve file interface decl.
//

class qtractorCurveFile
{
public:

	// Controller types.
	typedef qtractorMidiEvent::EventType ControlType;

	// Constructor.
	qtractorCurveFile(qtractorCurveList *pCurveList)
		: m_pCurveList(pCurveList), m_iCurrentCurve(-1) {}

	// Destructor.	
	~qtractorCurveFile() { clear(); }

	// Curve list accessor.
	qtractorCurveList *list() const
		{ return m_pCurveList; }

	// Base directory path.
	void setBaseDir(const QString& sBaseDir)
		{ m_sBaseDir = sBaseDir; }
	const QString& baseDir() const
		{ return m_sBaseDir; }

	// Filename accessors.
	void setFilename(const QString& sFilename)
		{ m_sFilename = sFilename; }
	const QString& filename() const
		{ return m_sFilename; }

	// Current curve index accesors.
	void setCurrentCurve(int iCurrentCurve)
		{ m_iCurrentCurve = iCurrentCurve; }
	int currentCurve() const
		{ return m_iCurrentCurve; }

	// Curve item escriptor.
	struct Item
	{
		QString          name;
		unsigned long    index;
		ControlType      ctype;
		unsigned short   channel;
		unsigned short   param;
		qtractorCurve::Mode mode;
		bool             process;
		bool             capture;
		bool             locked;
		bool             logarithmic;
		QColor           color;
		qtractorSubject *subject;
	};

	// Curve item list accessors.
	const QList<Item *>& items() const
		{ return m_items; }

	void addItem(Item *pCurveItem)
		{ m_items.append(pCurveItem); }
	
	// Curve item list emptyness predicate.
	bool isEmpty() const
		{ return m_items.isEmpty(); }

	// Curve item list cleanup.
	void clear()
	{
		qDeleteAll(m_items);
		m_items.clear();

		m_iCurrentCurve = -1;
	}

	// Curve item list serialization methods.
	void load(QDomElement *pElement);
	void save(qtractorDocument *pDocument,
		QDomElement *pElement, qtractorTimeScale *pTimeScale) const;
	void apply(qtractorTimeScale *pTimeScale);

	// Text/curve-mode converters...
	static qtractorCurve::Mode modeFromText(const QString& sText);
	static QString textFromMode(qtractorCurve::Mode mode);

private:

	// Instance variables.
	qtractorCurveList *m_pCurveList;
	QString            m_sBaseDir;
	QString            m_sFilename;

	QList<Item *>      m_items;

	int                m_iCurrentCurve;
};


#endif  // __qtractorCurveFile_h

