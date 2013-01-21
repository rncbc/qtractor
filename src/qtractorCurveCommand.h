// qtractorCurveCommand.h
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

#ifndef __qtractorCurveCommand_h
#define __qtractorCurveCommand_h

#include "qtractorCommand.h"
#include "qtractorCurve.h"


//----------------------------------------------------------------------
// class qtractorCurveBaseCommand - declaration.
//

class qtractorCurveBaseCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorCurveBaseCommand(const QString& sName);

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Common executive method.
	virtual bool execute(bool bRedo);
};


//----------------------------------------------------------------------
// class qtractorCurveCommand - declaration.
//

class qtractorCurveCommand : public qtractorCurveBaseCommand
{
public:

	// Constructor.
	qtractorCurveCommand(const QString& sName, qtractorCurve *pCurve);

	// Accessor.
	qtractorCurve *curve() const { return m_pCurve; }

protected:

	// Instance variables.
	qtractorCurve *m_pCurve;
};


//----------------------------------------------------------------------
// class qtractorCurveListCommand - declaration.
//

class qtractorCurveListCommand : public qtractorCurveBaseCommand
{
public:

	// Constructor.
	qtractorCurveListCommand(const QString& sName, qtractorCurveList *pCurveList);

protected:

	// Instance variables.
	qtractorCurveList *m_pCurveList;
};


//----------------------------------------------------------------------
// class qtractorCurveSelectCommand - declaration.
//

class qtractorCurveSelectCommand : public qtractorCurveListCommand
{
public:

	// Constructor.
	qtractorCurveSelectCommand(
		qtractorCurveList *pCurveList, qtractorCurve *pCurrentCurve);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	qtractorCurve *m_pCurrentCurve;
};


//----------------------------------------------------------------------
// class qtractorCurveModeCommand - declaration.
//

class qtractorCurveModeCommand : public qtractorCurveCommand
{
public:

	// Constructor.
	qtractorCurveModeCommand(
		qtractorCurve *pCurve, qtractorCurve::Mode mode);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	qtractorCurve::Mode m_mode;
};


//----------------------------------------------------------------------
// class qtractorCurveProcessCommand - declaration.
//

class qtractorCurveProcessCommand : public qtractorCurveCommand
{
public:

	// Constructor.
	qtractorCurveProcessCommand(
		qtractorCurve *pCurve, bool bProcess);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	bool m_bProcess;
};


//----------------------------------------------------------------------
// class qtractorCurveCaptureCommand - declaration.
//

class qtractorCurveCaptureCommand : public qtractorCurveCommand
{
public:

	// Constructor.
	qtractorCurveCaptureCommand(
		qtractorCurve *pCurve, bool bCapture);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	bool m_bCapture;
};


//----------------------------------------------------------------------
// class qtractorCurveLogarithmicCommand - declaration.
//

class qtractorCurveLogarithmicCommand : public qtractorCurveCommand
{
public:

	// Constructor.
	qtractorCurveLogarithmicCommand(
		qtractorCurve *pCurve, bool bLogarithmic);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	bool m_bLogarithmic;
};


//----------------------------------------------------------------------
// class qtractorCurveColorCommand - declaration.
//

class qtractorCurveColorCommand : public qtractorCurveCommand
{
public:

	// Constructor.
	qtractorCurveColorCommand(
		qtractorCurve *pCurve, const QColor& color);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	QColor m_color;
};


//----------------------------------------------------------------------
// class qtractorCurveProcessAllCommand - declaration.
//

class qtractorCurveProcessAllCommand : public qtractorCurveListCommand
{
public:

	// Constructor.
	qtractorCurveProcessAllCommand(
		qtractorCurveList *pCurveList, bool bProcessAll);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	bool m_bProcessAll;
};


//----------------------------------------------------------------------
// class qtractorCurveCaptureCommand - declaration.
//

class qtractorCurveCaptureAllCommand : public qtractorCurveListCommand
{
public:

	// Constructor.
	qtractorCurveCaptureAllCommand(
		qtractorCurveList *pCurveList, bool bCaptureAll);

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	bool m_bCaptureAll;
};


//----------------------------------------------------------------------
// class qtractorCurveEditCommand - declaration.
//

class qtractorCurveEditCommand : public qtractorCurveCommand
{
public:

	// Constructor.
	qtractorCurveEditCommand(
		const QString& sName, qtractorCurve *pCurve);
	qtractorCurveEditCommand(qtractorCurve *pCurve);

	// Destructor.
	virtual ~qtractorCurveEditCommand();

	// Primitive command methods.
	void addNode(qtractorCurve::Node *pNode);
	void moveNode(qtractorCurve::Node *pNode, unsigned long iFrame);
	void removeNode(qtractorCurve::Node *pNode);

	void addEditList(qtractorCurveEditList *pEditList);

	// Composite predicate.
	bool isEmpty() const;

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	qtractorCurveEditList m_edits;
};


//----------------------------------------------------------------------
// class qtractorCurveClearCommand - declaration.
//

class qtractorCurveClearCommand : public qtractorCurveEditCommand
{
public:

	// Constructor.
	qtractorCurveClearCommand(qtractorCurve *pCurve);
};


//----------------------------------------------------------------------
// class qtractorCurveClearAllCommand - declaration.
//

class qtractorCurveClearAllCommand : public qtractorCurveListCommand
{
public:

	// Constructor.
	qtractorCurveClearAllCommand(qtractorCurveList *pCurveList);

	// Destructor.
	~qtractorCurveClearAllCommand();

	// Composite predicate.
	bool isEmpty() const;

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	QList<qtractorCurveClearCommand *> m_commands;
};


//----------------------------------------------------------------------
// class qtractorCurveEditListCommand - declaration.
//

class qtractorCurveEditListCommand : public qtractorCurveListCommand
{
public:

	// Constructor.
	qtractorCurveEditListCommand(qtractorCurveList *pCurveList);

	// Destructor.
	~qtractorCurveEditListCommand();

	// Composite predicate.
	bool isEmpty() const;

protected:

	// Virtual executive method.
	bool execute(bool bRedo);

private:

	// Instance variables.
	QList<qtractorCurveEditCommand *> m_curveEditCommands;
};


//----------------------------------------------------------------------
// class qtractorCurveCaptureListCommand - declaration.
//

class qtractorCurveCaptureListCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorCurveCaptureListCommand();

	// Destructor.
	~qtractorCurveCaptureListCommand();

	// Curve list adder.
	void addCurveList(qtractorCurveList *pCurveList);

	// Composite predicate.
	bool isEmpty() const;

	// Virtual command methods.
	bool redo();
	bool undo();

private:

	// Instance variables.
	QList<qtractorCurveEditListCommand *> m_commands;
};


#endif	// __qtractorCurveCommand_h

// end of qtractorCurveCommand.h

