// qtractorMessageList.cpp
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

#include "qtractorMessageList.h"


//----------------------------------------------------------------------
// class qtractorMessageList -- message string list buffer.
//

// Pseudo-singleton instance.
qtractorMessageList *qtractorMessageList::g_pInstance = NULL;


// Constructor.
qtractorMessageList::qtractorMessageList (void)
{
	g_pInstance = this;
}


// Destructor.
qtractorMessageList::~qtractorMessageList (void)
{
	m_items.clear();

	g_pInstance = NULL;
}


// Forfeit methods.
void qtractorMessageList::append ( const QString& sText )
{
	if (g_pInstance)
		g_pInstance->m_items.append(sText);

	qWarning("Warning: %s", sText.toUtf8().constData());
}


bool qtractorMessageList::isEmpty (void)
{
	return (g_pInstance ? g_pInstance->m_items.isEmpty() : true);
}


QStringList qtractorMessageList::items (void)
{
	return (g_pInstance ? g_pInstance->m_items : QStringList());
}


void qtractorMessageList::clear (void)
{
	if (g_pInstance)
		g_pInstance->m_items.clear();
}


// end of qtractorMessageList.cpp
