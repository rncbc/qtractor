// qtractorCurveCommand.h
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

#ifndef __qtractorCurveCommand_h
#define __qtractorCurveCommand_h

#include "qtractorCommand.h"
#include "qtractorCurve.h"

#include <QList>


//----------------------------------------------------------------------
// class qtractorCurveCommand - declaration.
//

class qtractorCurveCommand : public qtractorCommand
{
public:

	// Constructor.
	qtractorCurveCommand(const QString& sName);
	// Destructor.
	virtual ~qtractorCurveCommand();

	// Primitive command methods.
	void addNode(qtractorCurve *pCurve, qtractorCurve::Node *pNode);
	void removeNode(qtractorCurve *pCurve, qtractorCurve::Node *pNode);

	// Composite predicate.
	bool isEmpty() const;

	// Virtual command methods.
	bool redo();
	bool undo();

protected:

	// Common executive method.
	bool execute(bool bRedo);

private:

	// Primitive command types.
	enum CommandType {
		AddNode, RemoveNode
	};

	// Curve item struct.
	struct Item
	{
		// Item constructor.
		Item(CommandType cmd,
			qtractorCurve *pCurve, qtractorCurve::Node *pNode)
			: command(cmd), curve(pCurve), node(pNode), autoDelete(false) {}
		// Item members.
		CommandType command;
		qtractorCurve *curve;
		qtractorCurve::Node *node;
		bool autoDelete;
	};

	// Instance variables.
	QList<Item *>  m_items;
};


#endif	// __qtractorCurveCommand_h

// end of qtractorCurveCommand.h

