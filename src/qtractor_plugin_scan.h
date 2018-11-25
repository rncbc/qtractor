// qtractor_plugin_scan.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractor_dssi_scan_h
#define __qtractor_dssi_scan_h

#include <QString>

// Forward decls.
class QLibrary;


#ifdef CONFIG_LADSPA

#include <ladspa.h>

//----------------------------------------------------------------------
// class qtractor_ladspa_scan -- LADSPA plugin (bare bones) interface.
//

class qtractor_ladspa_scan
{
public:

	// Constructor.
	qtractor_ladspa_scan();

	// destructor.
	~qtractor_ladspa_scan();

	// File loader.
	bool open(const QString& sFilename);
	bool open_descriptor(unsigned long iIndex = 0);
	void close_descriptor();
	void close();

	// Properties.
	bool isOpen() const;

	const QString& name() const
		{ return m_sName; }

	unsigned int uniqueID() const;

	int controlIns() const
		{ return m_iControlIns; }
	int controlOuts() const
		{ return m_iControlOuts; }

	int audioIns() const
		{ return m_iAudioIns; }
	int audioOuts() const
		{ return m_iAudioOuts; }

	bool isRealtime() const;

private:

	// Instance variables.
	QLibrary *m_pLibrary;

	const LADSPA_Descriptor *m_pLadspaDescriptor;

	QString m_sName;

	int m_iControlIns;
	int m_iControlOuts;

	int m_iAudioIns;
	int m_iAudioOuts;
};

#endif	// CONFIG_LADSPA


#ifdef CONFIG_DSSI

#include <dssi.h>

//----------------------------------------------------------------------
// class qtractor_dssi_scan -- DSSI plugin (bare bones) interface.
//

class qtractor_dssi_scan
{
public:

	// Constructor.
	qtractor_dssi_scan();

	// destructor.
	~qtractor_dssi_scan();

	// File loader.
	bool open(const QString& sFilename);
	bool open_descriptor(unsigned long iIndex = 0);
	void close_descriptor();
	void close();

	// Properties.
	bool isOpen() const;

	const QString& name() const
		{ return m_sName; }

	unsigned int uniqueID() const;

	int controlIns() const
		{ return m_iControlIns; }
	int controlOuts() const
		{ return m_iControlOuts; }

	int audioIns() const
		{ return m_iAudioIns; }
	int audioOuts() const
		{ return m_iAudioOuts; }

	bool isRealtime() const;
	bool isConfigure() const;

	bool isEditor() const
		{ return m_bEditor; }

private:

	// Instance variables.
	QLibrary *m_pLibrary;

	const DSSI_Descriptor   *m_pDssiDescriptor;
	const LADSPA_Descriptor *m_pLadspaDescriptor;

	QString m_sName;

	int m_iControlIns;
	int m_iControlOuts;

	int m_iAudioIns;
	int m_iAudioOuts;

	bool m_bEditor;
};

#endif	// CONFIG_DSSI


#ifdef CONFIG_VST

//----------------------------------------------------------------------
// class qtractor_vst_scan -- VST plugin (bare bones) interface.
//

#ifdef CONFIG_VESTIGE
typedef struct _AEffect AEffect;
#else
struct AEffect;
#endif

class qtractor_vst_scan
{
public:

	// Constructor.
	qtractor_vst_scan();

	// destructor.
	~qtractor_vst_scan();

	// File loader.
	bool open(const QString& sFilename);
	bool open_descriptor(unsigned long iIndex = 0);
	void close_descriptor();
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
	bool          m_bEditor;
	QString       m_sName;
};

#endif	// CONFIG_VST


#endif	// __qtractor_plugin_scan_h

// end of qtractor_plugin_scan.h
