// qtractorTempoCurveCommand.h
//
/****************************************************************************
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

#ifndef __qtractorTempoCurveCommand_h
#define __qtractorTempoCurveCommand_h

#include "qtractorCommand.h"
#include "qtractorTempoCurve.h"


//----------------------------------------------------------------------
// class qtractorTempoCurveBaseCommand - declaration.
//

class qtractorTempoCurveBaseCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorTempoCurveBaseCommand(const QString& sName);

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Common executive method.
	virtual bool execute(bool bRedo);
};


//----------------------------------------------------------------------
// class qtractorTempoCurveCommand - declaration.
//

class qtractorTempoCurveCommand : public qtractorTempoCurveBaseCommand
{
public:

	// Constructor.
	qtractorTempoCurveCommand(const QString& sName, qtractorTempoCurve *pTempoCurve);

	// Accessor.
	qtractorTempoCurve *tempoCurve() const { return m_pTempoCurve; }

protected:

	// Instance variables.
	qtractorTempoCurve *m_pTempoCurve;
};


//----------------------------------------------------------------------
// class qtractorTempoCurveProcessCommand - declaration.
//

class qtractorTempoCurveProcessCommand : public qtractorTempoCurveCommand
{
public:

	// Constructor.
	qtractorTempoCurveProcessCommand(
		qtractorTempoCurve *pTempoCurve, bool bProcess);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	bool m_bProcess;
};


//----------------------------------------------------------------------
// class qtractorTempoCurveColorCommand - declaration.
//

class qtractorTempoCurveColorCommand : public qtractorTempoCurveCommand
{
public:

	// Constructor.
	qtractorTempoCurveColorCommand(
		qtractorTempoCurve *pTempoCurve, const QColor& color);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	QColor m_color;
};


//----------------------------------------------------------------------
// class qtractorTempoCurveEditCommand - declaration.
//

class qtractorTempoCurveEditCommand : public qtractorTempoCurveCommand
{
public:

	// Constructor.
	qtractorTempoCurveEditCommand(
		const QString& sName, qtractorTempoCurve *pTempoCurve);
	qtractorTempoCurveEditCommand(qtractorTempoCurve *pTempoCurve);

	// Destructor.
	virtual ~qtractorTempoCurveEditCommand();

	// Primitive command methods.
	void addNode(qtractorTimeScale::Node *pNode);
	void moveNode(qtractorTimeScale::Node *pNode,
		float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor, bool bAttached);
	void removeNode(qtractorTimeScale::Node *pNode);

	// Composite predicate.
	bool isEmpty() const;

protected:
	// Virtual executive method.
	bool execute(bool bRedo);

private:
	// Instance variables.
	qtractorTempoCurveEditList m_edits;
};


//----------------------------------------------------------------------
// class qtractorTempoCurveClearCommand - declaration.
//

class qtractorTempoCurveClearCommand : public qtractorTempoCurveEditCommand
{
public:

	// Constructor.
	qtractorTempoCurveClearCommand(qtractorTempoCurve *pTempoCurve);
};


#endif	// __qtractorTempoCurveCommand_h

// end of qtractorTempoCurveCommand.h

