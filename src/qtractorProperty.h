// qtractorProperty.h
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorProperty_h
#define __qtractorProperty_h


//-------------------------------------------------------------------------
// qtractorProperty -- generic property template.

template<typename T>
class qtractorProperty
{
public:

	// Constructors.
	qtractorProperty() {}
	qtractorProperty(const T& value)
		{ init(value); }

	// Initilalizer method.
	void init(const T& value)
		{ m_oldValue = m_curValue = value; }

	// Property value accessors.
	const T& value() const { return m_curValue; }
	void setValue(const T& value)
		{ m_oldValue = m_curValue; m_curValue = value; }

	// Value change status.
	bool isChanged() const
		{ return (m_curValue != m_oldValue); }

	// Value swap method.
	void swap()
		{ T temp = m_oldValue; m_oldValue = m_curValue; m_curValue = temp; }

private:

	// Instance variables.
	T m_curValue;
	T m_oldValue;
};


#endif  // __qtractorProperty_h


// end of qtractorProperty.h
