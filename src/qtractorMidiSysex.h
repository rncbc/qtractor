// qtractorMidiSysex.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiSysex_h
#define __qtractorMidiSysex_h

#include <QString>
#include <QList>

#include <stdio.h>


//----------------------------------------------------------------------
// class qtractorMidiSysex -- MIDI SysEx data bank item.
//

class qtractorMidiSysex
{
public:

	// Constructors
	qtractorMidiSysex(const QString& sName,
		unsigned char *pSysex, unsigned short iSysex)
		: m_sName(sName), m_pSysex(NULL), m_iSysex(0)
		{ setData(pSysex, iSysex); }
	qtractorMidiSysex(const QString& sName, const QString& sText)
		: m_sName(sName), m_pSysex(NULL), m_iSysex(0)
		{ setText(sText); }

	// Copy consructor.
	qtractorMidiSysex(const qtractorMidiSysex& sysex)
		: m_sName(sysex.name()), m_pSysex(NULL), m_iSysex(0)
		{ setData(sysex.data(), sysex.size()); }

	// Destructors;
	~qtractorMidiSysex() { clear(); }

	// Name key accessors.
	void setName(const QString& sName)
		{ m_sName = sName; }
	const QString& name() const
		{ return m_sName; }

	// Binary data accessors.
	void setData(unsigned char *pSysex, unsigned short iSysex)
	{
		if (pSysex && iSysex > 0
			&& pSysex[0] == 0xf0
			&& pSysex[iSysex - 1] == 0xf7) {
			if (m_pSysex) delete [] m_pSysex;
			m_iSysex = iSysex;
			m_pSysex = new unsigned char [iSysex];
			::memcpy(m_pSysex, pSysex, iSysex);
		}
	}

	unsigned char *data() const { return m_pSysex; }
	unsigned short size() const { return m_iSysex; }

	// Text(hex) data accessors.
	void setText(const QString& sText)
	{
		const QByteArray& data = QByteArray::fromHex(sText.toLatin1());
		setData((unsigned char *) data.data(), data.size());
	}

	QString text() const
	{
		QString sText; char hex[4];
		for (unsigned short i = 0; i < m_iSysex; ++i) {
			::snprintf(hex, sizeof(hex), "%02x", m_pSysex[i]);
			sText += hex;
			if (i < m_iSysex - 1) sText += ' ';
		}
		return sText;
	}

	// Cleanup.
	void clear()
	{
		m_sName.clear();
		if (m_pSysex) {
			delete [] m_pSysex;
			m_pSysex = NULL;
			m_iSysex = 0;
		}
	}

private:

	// Instance variables.
	QString         m_sName;
	unsigned char  *m_pSysex;
	unsigned short  m_iSysex;
};


//----------------------------------------------------------------------
// class qtractorMidiSysexList -- MIDI SysEx data bank list.
//

class qtractorMidiSysexList	: public QList<qtractorMidiSysex *>
{
public:

	// Destructor.
	~qtractorMidiSysexList() { clear(); }

	// Destroyer.
	void clear() { qDeleteAll(*this); QList<qtractorMidiSysex *>::clear(); }
};


#endif  // __qtractorMidiSysex_h


// end of qtractorMidiSysex.h
