// qtractor_vst_scan.h
//
/****************************************************************************
   Copyright (C) 2016, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractor_vst_scan_h
#define __qtractor_vst_scan_h

#include <QString>

// Forward decls.
class QLibrary;
class AEffect;


//----------------------------------------------------------------------
// class qtractor_vst_scan -- VST plugin (bare bones) interface.
//

class qtractor_vst_scan
{
public:

	// Constructor.
	qtractor_vst_scan();

	// destructor.
	~qtractor_vst_scan();

	// File loader.
	bool open(const QString& sFilename, unsigned long iIndex = 0);
	void close();

	// Properties.
	bool isOpen() const;

	const QString& name() const { return m_sName; }

	unsigned int uniqueID() const;

	int numPrograms() const;
	int numParams() const;
	int numInputs() const;
	int numOutputs() const;

	int numMidiInputs() const;
	int numMidiOutputs() const;

	bool hasEditor() const;
	bool hasProgramChunks() const;

	// VST host dispatcher.
	int vst_dispatch(
		long opcode, long index, long value, void *ptr, float opt) const;

protected:

	// VST flag inquirer.
	bool canDo(const char *pszCanDo) const;

private:

	// Instance variables.
	QLibrary     *m_pLibrary;
	AEffect      *m_pEffect;
	unsigned int  m_iFlagsEx;
	QString       m_sName;
};


#endif // __qtractor_vst_scan_h

// end of qtractor_vst_scan.h
