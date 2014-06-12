// qtractorMessageList.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMessageList_h
#define __qtractorMessageList_h

#include <QStringList>


//----------------------------------------------------------------------
// class qtractorMessageList -- message string list buffer.
//
class qtractorMessageList
{
public:

	// Constructor.
	qtractorMessageList();
	// Destructor.
	~qtractorMessageList();

	// Forfeit methods.
	static void append(const QString& sText);

	static bool isEmpty();
	static QStringList items();

	static void clear();

private:

	// Instance variable.
	QStringList m_items;

	// Pseudo-singleton instance.
	static qtractorMessageList *g_pInstance;
};

#endif  // __qtractorMessageList_h


// end of qtractorMessageList.h
