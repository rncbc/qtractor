// qtractor_plugin_scan.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "config.h"

#include "qtractor_plugin_scan.h"

#include <QLibrary>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

#include <stdint.h>
#include <dlfcn.h>


#ifdef CONFIG_LADSPA

//----------------------------------------------------------------------
// class qtractor_ladspa_scan -- LADSPA plugin (bare bones) interface
//

// Constructor.
qtractor_ladspa_scan::qtractor_ladspa_scan (void)
	: m_pLibrary(nullptr), m_pLadspaDescriptor(nullptr),
		m_iControlIns(0), m_iControlOuts(0),
		m_iAudioIns(0), m_iAudioOuts(0)
{
}


// destructor.
qtractor_ladspa_scan::~qtractor_ladspa_scan (void)
{
	close();
}


// File loader.
bool qtractor_ladspa_scan::open ( const QString& sFilename )
{
	close();

	if (!QLibrary::isLibrary(sFilename))
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::open(\"%s\")", this,
		sFilename.toUtf8().constData());
#endif

	m_pLibrary = new QLibrary(sFilename);

	return true;
}


// Plugin loader.
bool qtractor_ladspa_scan::open_descriptor ( unsigned long iIndex )
{
	if (m_pLibrary == nullptr)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	// Retrieve the LADSPA descriptor function, if any...
	LADSPA_Descriptor_Function pfnLadspaDescriptor
		= (LADSPA_Descriptor_Function) m_pLibrary->resolve("ladspa_descriptor");
	if (pfnLadspaDescriptor == nullptr) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_ladspa_scan[%p]: plugin does not have DSSI descriptor.", this);
	#endif
		return false;
	}

	// Retrieve LADSPA descriptor if any...
	m_pLadspaDescriptor = (*pfnLadspaDescriptor)(iIndex);
	if (m_pLadspaDescriptor == nullptr)
		return false;

	// Get the official plugin name.
	m_sName = QString::fromLatin1(m_pLadspaDescriptor->Name);

	// Compute and cache port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;

	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;

	for (unsigned long i = 0; i < m_pLadspaDescriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor portType
			= m_pLadspaDescriptor->PortDescriptors[i];
		if (LADSPA_IS_PORT_INPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioIns;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlIns;
		}
		else
		if (LADSPA_IS_PORT_OUTPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioOuts;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlOuts;
		}
	}

	return true;
}


// Plugin uloader.
void qtractor_ladspa_scan::close_descriptor (void)
{
	if (m_pLadspaDescriptor == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::close_descriptor()", this);
#endif

	m_pLadspaDescriptor = nullptr;

	m_sName.clear();

	m_iControlIns  = 0;
	m_iControlOuts = 0;

	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
}


// File unloader.
void qtractor_ladspa_scan::close (void)
{
	close_descriptor();

	if (m_pLibrary == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::close()", this);
#endif
#if 0
	if (m_pLibrary->isLoaded())
		m_pLibrary->unload();
#endif
	delete m_pLibrary;

	m_pLibrary = nullptr;
}


// Check wether plugin is loaded.
bool qtractor_ladspa_scan::isOpen (void) const
{
	if (m_pLibrary == nullptr)
		return false;

	return m_pLibrary->isLoaded();
}


unsigned int qtractor_ladspa_scan::uniqueID() const
	{ return (m_pLadspaDescriptor ? m_pLadspaDescriptor->UniqueID : 0); }

bool qtractor_ladspa_scan::isRealtime() const
{
	if (m_pLadspaDescriptor == nullptr)
		return false;

	return LADSPA_IS_HARD_RT_CAPABLE(m_pLadspaDescriptor->Properties);
}


//-------------------------------------------------------------------------
// The LADSPA plugin stance scan method.
//

static void qtractor_ladspa_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_ladspa_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif
	qtractor_ladspa_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;

	while (plugin.open_descriptor(i)) {
		sout << "LADSPA|";
		sout << plugin.name() << '|';
		sout << plugin.audioIns()   << ':' << plugin.audioOuts()   << '|';
		sout << 0                   << ':' << 0                    << '|';
		sout << plugin.controlIns() << ':' << plugin.controlOuts() << '|';
		QStringList flags;
		if (plugin.isRealtime())
			flags.append("RT");
		sout << flags.join(",") << '|';
		sout << sFilename << '|' << i << '|';
		sout << "0x" << QString::number(plugin.uniqueID(), 16) << '\n';
		plugin.close_descriptor();
		++i;
	}

	plugin.close();

	// Must always give an answer, even if it's a wrong one...
	if (i == 0)
		sout << "qtractor_ladspa_scan: " << sFilename << ": plugin file error.\n";
}

#endif	// CONFIG_LADSPA


#ifdef CONFIG_DSSI

//----------------------------------------------------------------------
// class qtractor_dssi_scan -- LADSPA plugin re bones) interface
//

// Constructor.
qtractor_dssi_scan::qtractor_dssi_scan (void)
	: m_pLibrary(nullptr), m_pLadspaDescriptor(nullptr),
		m_iControlIns(0), m_iControlOuts(0),
		m_iAudioIns(0), m_iAudioOuts(0),
		m_bEditor(false)
{
}


// destructor.
qtractor_dssi_scan::~qtractor_dssi_scan (void)
{
	close();
}


// File loader.
bool qtractor_dssi_scan::open ( const QString& sFilename )
{
	close();

	if (!QLibrary::isLibrary(sFilename))
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::open(\"%s\")", this,
		sFilename.toUtf8().constData());
#endif

	m_pLibrary = new QLibrary(sFilename);

	return true;
}


// Plugin loader.
bool qtractor_dssi_scan::open_descriptor ( unsigned long iIndex )
{
	if (m_pLibrary == nullptr)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	// Retrieve the DSSI descriptor function, if any...
	DSSI_Descriptor_Function pfnDssiDescriptor
		= (DSSI_Descriptor_Function) m_pLibrary->resolve("dssi_descriptor");
	if (pfnDssiDescriptor == nullptr)
		return false;
	if (pfnDssiDescriptor == nullptr) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_dssi_scan[%p]: plugin does not have DSSI descriptor.", this);
	#endif
		return false;
	}

	// Retrieve the DSSI descriptor if any...
	m_pDssiDescriptor = (*pfnDssiDescriptor)(iIndex);
	if (m_pDssiDescriptor == nullptr)
		return false;

	// We're also a LADSPA one...
	m_pLadspaDescriptor = m_pDssiDescriptor->LADSPA_Plugin;
	if (m_pLadspaDescriptor == nullptr)
		return false;

	// Get the official plugin name.
	m_sName = QString::fromLatin1(m_pLadspaDescriptor->Name);

	// Compute and cache port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;

	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;

	for (unsigned long i = 0; i < m_pLadspaDescriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor portType
			= m_pLadspaDescriptor->PortDescriptors[i];
		if (LADSPA_IS_PORT_INPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioIns;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlIns;
		}
		else
		if (LADSPA_IS_PORT_OUTPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioOuts;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlOuts;
		}
	}

	m_bEditor = false;

	// Check for GUI editor executable...
	const QFileInfo fi(m_pLibrary->fileName());
	const QFileInfo gi(fi.dir(), fi.baseName());
	if (gi.isDir()) {
		QDir dir(gi.absoluteFilePath());
		const QString sMask("%1_*");
		QStringList names;
		names.append(sMask.arg(fi.baseName()));
		names.append(sMask.arg(m_pLadspaDescriptor->Label));
		dir.setNameFilters(names);
		m_bEditor = !dir.entryList(QDir::Files | QDir::Executable).isEmpty();
	}


	return true;
}


// Plugin uloader.
void qtractor_dssi_scan::close_descriptor (void)
{
	if (m_pLadspaDescriptor == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::close_descriptor()", this);
#endif

	m_pLadspaDescriptor = nullptr;

	m_sName.clear();

	m_iControlIns  = 0;
	m_iControlOuts = 0;

	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;

	m_bEditor = false;
}


// File unloader.
void qtractor_dssi_scan::close (void)
{
	close_descriptor();

	if (m_pLibrary == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::close()", this);
#endif
#if 0
	if (m_pLibrary->isLoaded())
		m_pLibrary->unload();
#endif
	delete m_pLibrary;

	m_pLibrary = nullptr;
}


// Check wether plugin is loaded.
bool qtractor_dssi_scan::isOpen (void) const
{
	if (m_pLibrary == nullptr)
		return false;

	return m_pLibrary->isLoaded();
}


unsigned int qtractor_dssi_scan::uniqueID() const
	{ return (m_pLadspaDescriptor ? m_pLadspaDescriptor->UniqueID : 0); }

bool qtractor_dssi_scan::isRealtime() const
{
	if (m_pLadspaDescriptor == nullptr)
		return false;

	return LADSPA_IS_HARD_RT_CAPABLE(m_pLadspaDescriptor->Properties);
}


bool qtractor_dssi_scan::isConfigure() const
{
	if (m_pDssiDescriptor == nullptr)
		return false;

	return (m_pDssiDescriptor->configure != nullptr);
}


//-------------------------------------------------------------------------
// The DSSI plugin stance scan method.
//

static void qtractor_dssi_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_dssi_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif
	qtractor_dssi_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;

	while (plugin.open_descriptor(i)) {
		sout << "DSSI|";
		sout << plugin.name() << '|';
		sout << plugin.audioIns()   << ':' << plugin.audioOuts()   << '|';
		sout << 1                   << ':' << 0                    << '|';
		sout << plugin.controlIns() << ':' << plugin.controlOuts() << '|';
		QStringList flags;
		if (plugin.isEditor())
			flags.append("GUI");
		if (plugin.isConfigure())
			flags.append("EXT");
		if (plugin.isRealtime())
			flags.append("RT");
		sout << flags.join(",") << '|';
		sout << sFilename << '|' << i << '|';
		sout << "0x" << QString::number(plugin.uniqueID(), 16) << '\n';
		plugin.close_descriptor();
		++i;
	}

	plugin.close();

	// Must always give an answer, even if it's a wrong one...
	if (i == 0)
		sout << "qtractor_dssi_scan: " << sFilename << ": plugin file error.\n";
}

#endif	// CONFIG_DSSI


#ifdef CONFIG_VST2

#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
#define __cdecl
#endif

#ifdef CONFIG_VESTIGE
#include <vestige.h>
#else
#include <aeffectx.h>
#endif

#if !defined(VST_2_3_EXTENSIONS)
#ifdef CONFIG_VESTIGE
typedef int32_t  VstInt32;
typedef intptr_t VstIntPtr;
#else
typedef long     VstInt32;
typedef long     VstIntPtr;
#endif
#define VSTCALLBACK
#endif

typedef AEffect* (*VST_GetPluginInstance) (audioMasterCallback);

static VstIntPtr VSTCALLBACK qtractor_vst2_scan_callback (AEffect *effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt);

// Current working VST Shell identifier.
static int g_iVst2ShellCurrentId = 0;

// Specific extended flags that saves us
// from calling canDo() in audio callbacks.
enum VST_FlagsEx
{
	effFlagsExCanSendVstEvents          = 1 << 0,
	effFlagsExCanSendVstMidiEvents      = 1 << 1,
	effFlagsExCanSendVstTimeInfo        = 1 << 2,
	effFlagsExCanReceiveVstEvents       = 1 << 3,
	effFlagsExCanReceiveVstMidiEvents   = 1 << 4,
	effFlagsExCanReceiveVstTimeInfo     = 1 << 5,
	effFlagsExCanProcessOffline         = 1 << 6,
	effFlagsExCanUseAsInsert            = 1 << 7,
	effFlagsExCanUseAsSend              = 1 << 8,
	effFlagsExCanMixDryWet              = 1 << 9,
	effFlagsExCanMidiProgramNames       = 1 << 10
};


// Some VeSTige missing opcodes and flags.
#ifdef CONFIG_VESTIGE
const int effSetProgramName = 4;
const int effGetParamLabel = 6;
const int effGetParamDisplay = 7;
const int effGetChunk = 23;
const int effSetChunk = 24;
const int effFlagsProgramChunks = 32;
#endif


//----------------------------------------------------------------------
// class qtractor_vst2_scan -- VST2 plugin re bones) interface
//

// Constructor.
qtractor_vst2_scan::qtractor_vst2_scan (void)
	: m_pLibrary(nullptr), m_pEffect(nullptr), m_iFlagsEx(0), m_bEditor(false)
{
}


// destructor.
qtractor_vst2_scan::~qtractor_vst2_scan (void)
{
	close();
}


// File loader.
bool qtractor_vst2_scan::open ( const QString& sFilename )
{
	close();

	if (!QLibrary::isLibrary(sFilename))
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst2_scan[%p]::open(\"%s\", %lu)", this,
		sFilename.toUtf8().constData());
#endif

	m_pLibrary = new QLibrary(sFilename);

	m_sName = QFileInfo(sFilename).baseName();

	return true;
}


// Plugin loader.
bool qtractor_vst2_scan::open_descriptor ( unsigned long iIndex )
{
	if (m_pLibrary == nullptr)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst2_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	VST_GetPluginInstance pfnGetPluginInstance
		= (VST_GetPluginInstance) m_pLibrary->resolve("VSTPluginMain");
	if (pfnGetPluginInstance == nullptr)
		pfnGetPluginInstance = (VST_GetPluginInstance) m_pLibrary->resolve("main");
	if (pfnGetPluginInstance == nullptr) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst2_scan[%p]: plugin does not have a main entry point.", this);
	#endif
		return false;
	}

	m_pEffect = (*pfnGetPluginInstance)(qtractor_vst2_scan_callback);
	if (m_pEffect == nullptr) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst2_scan[%p]: plugin instance could not be created.", this);
	#endif
		return false;
	}

	// Did VST plugin instantiated OK?
	if (m_pEffect->magic != kEffectMagic) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst2_scan[%p]: plugin is not a valid VST.", this);
	#endif
		m_pEffect = nullptr;
		return false;
	}

	// Check whether it's a VST Shell...
	const int categ = vst2_dispatch(effGetPlugCategory, 0, 0, nullptr, 0.0f);
	if (categ == kPlugCategShell) {
		int id = 0;
		char buf[40];
		unsigned long i = 0;
		for ( ; iIndex >= i; ++i) {
			buf[0] = (char) 0;
			id = vst2_dispatch(effShellGetNextPlugin, 0, 0, (void *) buf, 0.0f);
			if (id == 0 || !buf[0])
				break;
		}
		// Check if we're actually the intended plugin...
		if (i < iIndex || id == 0 || !buf[0]) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst2_scan[%p]: "
				"vst2_shell(%lu) plugin is not a valid VST.", this, iIndex);
		#endif
			m_pEffect = nullptr;
			return false;
		}
		// Make it known...
		g_iVst2ShellCurrentId = id;
		// Re-allocate the thing all over again...
		pfnGetPluginInstance
			= (VST_GetPluginInstance) m_pLibrary->resolve("VSTPluginMain");
		if (pfnGetPluginInstance == nullptr)
			pfnGetPluginInstance = (VST_GetPluginInstance) m_pLibrary->resolve("main");
		if (pfnGetPluginInstance == nullptr) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst2_scan[%p]: "
				"vst2_shell(%lu) plugin does not have a main entry point.", this, iIndex);
		#endif
			m_pEffect = nullptr;
			return false;
		}
		// Does the VST plugin instantiate OK?
		m_pEffect = (*pfnGetPluginInstance)(qtractor_vst2_scan_callback);
		// Not needed anymore, hopefully...
		g_iVst2ShellCurrentId = 0;
		// Don't go further if failed...
		if (m_pEffect == nullptr) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst2_scan[%p]: "
				"vst2_shell(%lu) plugin instance could not be created.", this, iIndex);
		#endif
			return false;
		}
		if (m_pEffect->magic != kEffectMagic) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst2_scan[%p]: "
				"vst2_shell(%lu) plugin is not a valid VST.", this, iIndex);
		#endif
			m_pEffect = nullptr;
			return false;
		}
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst2_scan[%p]: "
			"vst2_shell(%lu) id=0x%x name=\"%s\"", this, i, id, buf);
	#endif
	}
	else
	// Not a VST Shell plugin...
	if (iIndex > 0) {
		m_pEffect = nullptr;
		return false;
	}

//	vst2_dispatch(effIdentify, 0, 0, nullptr, 0.0f);
	vst2_dispatch(effOpen,     0, 0, nullptr, 0.0f);

	// Get label name...
	char szName[256]; ::memset(szName, 0, sizeof(szName));
	if (vst2_dispatch(effGetEffectName, 0, 0, (void *) szName, 0.0f))
		m_sName = szName;

	// Specific inquiries...
	m_iFlagsEx = 0;

	if (vst2_canDo("sendVstEvents"))       m_iFlagsEx |= effFlagsExCanSendVstEvents;
	if (vst2_canDo("sendVstMidiEvent"))    m_iFlagsEx |= effFlagsExCanSendVstMidiEvents;
	if (vst2_canDo("sendVstTimeInfo"))     m_iFlagsEx |= effFlagsExCanSendVstTimeInfo;
	if (vst2_canDo("receiveVstEvents"))    m_iFlagsEx |= effFlagsExCanReceiveVstEvents;
	if (vst2_canDo("receiveVstMidiEvent")) m_iFlagsEx |= effFlagsExCanReceiveVstMidiEvents;
	if (vst2_canDo("receiveVstTimeInfo"))  m_iFlagsEx |= effFlagsExCanReceiveVstTimeInfo;
	if (vst2_canDo("offline"))             m_iFlagsEx |= effFlagsExCanProcessOffline;
	if (vst2_canDo("plugAsChannelInsert")) m_iFlagsEx |= effFlagsExCanUseAsInsert;
	if (vst2_canDo("plugAsSend"))          m_iFlagsEx |= effFlagsExCanUseAsSend;
	if (vst2_canDo("mixDryWet"))           m_iFlagsEx |= effFlagsExCanMixDryWet;
	if (vst2_canDo("midiProgramNames"))    m_iFlagsEx |= effFlagsExCanMidiProgramNames;

	m_bEditor = (m_pEffect->flags & effFlagsHasEditor);

	return true;
}


// Plugin unloader.
void qtractor_vst2_scan::close_descriptor (void)
{
	if (m_pEffect == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst2_scan[%p]::close_descriptor()", this);
#endif

	vst2_dispatch(effClose, 0, 0, 0, 0.0f);

	m_pEffect  = nullptr;
	m_iFlagsEx = 0;
//	m_bEditor  = false;
	m_sName.clear();
}


// File unloader.
void qtractor_vst2_scan::close (void)
{
	if (m_pLibrary == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst2_scan[%p]::close()", this);
#endif

	vst2_dispatch(effClose, 0, 0, 0, 0.0f);

	if (m_pLibrary->isLoaded() && !m_bEditor)
		m_pLibrary->unload();

	delete m_pLibrary;

	m_pLibrary = nullptr;

	m_bEditor = false;
}


// Check wether plugin is loaded.
bool qtractor_vst2_scan::isOpen (void) const
{
	if (m_pLibrary == nullptr)
		return false;

	return m_pLibrary->isLoaded();
}

unsigned int qtractor_vst2_scan::uniqueID() const
	{ return (m_pEffect ? m_pEffect->uniqueID : 0); }

int qtractor_vst2_scan::numPrograms() const
	{ return (m_pEffect ? m_pEffect->numPrograms : 0); }
int qtractor_vst2_scan::numParams() const
	{ return (m_pEffect ? m_pEffect->numParams : 0); }
int qtractor_vst2_scan::numInputs() const
	{ return (m_pEffect ? m_pEffect->numInputs : 0); }
int qtractor_vst2_scan::numOutputs() const
	{ return (m_pEffect ? m_pEffect->numOutputs : 0); }

int qtractor_vst2_scan::numMidiInputs() const
	{ return (m_pEffect && (
		(m_iFlagsEx & effFlagsExCanReceiveVstMidiEvents) ||
		(m_pEffect->flags & effFlagsIsSynth) ? 1 : 0)); }

int qtractor_vst2_scan::numMidiOutputs() const
	{ return ((m_iFlagsEx & effFlagsExCanSendVstMidiEvents) ? 1 : 0); }

bool qtractor_vst2_scan::hasEditor() const
	{ return m_bEditor; }
bool qtractor_vst2_scan::hasProgramChunks() const
	{ return (m_pEffect && (m_pEffect->flags & effFlagsProgramChunks)); }


// VST host dispatcher.
int qtractor_vst2_scan::vst2_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pEffect == nullptr)
		return 0;

#ifdef CONFIG_DEBUG_0
	qDebug("vst2_plugin[%p]::vst2_dispatch(%ld, %ld, %ld, %p, %g)",
		this, opcode, index, value, ptr, opt);
#endif

	return m_pEffect->dispatcher(m_pEffect, opcode, index, value, ptr, opt);
}


// VST flag inquirer.
bool qtractor_vst2_scan::vst2_canDo ( const char *pszCanDo ) const
{
	return (vst2_dispatch(effCanDo, 0, 0, (void *) pszCanDo, 0.0f) > 0);
}


//----------------------------------------------------------------------
// The magnificient host callback, which every VST plugin will call.

static VstIntPtr VSTCALLBACK qtractor_vst2_scan_callback ( AEffect* effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr /*value*/, void */*ptr*/, float opt )
{
	VstIntPtr ret = 0;

	switch (opcode) {
	case audioMasterVersion:
		ret = 2;
		break;
	case audioMasterAutomate:
		effect->setParameter(effect, index, opt);
		break;
	case audioMasterCurrentId:
		ret = (VstIntPtr) g_iVst2ShellCurrentId;
		break;
	case audioMasterGetSampleRate:
		effect->dispatcher(effect, effSetSampleRate, 0, 0, nullptr, 44100.0f);
		break;
	case audioMasterGetBlockSize:
		effect->dispatcher(effect, effSetBlockSize, 0, 1024, nullptr, 0.0f);
		break;
	case audioMasterGetAutomationState:
		ret = 1; // off
		break;
	case audioMasterGetLanguage:
		ret = kVstLangEnglish;
		break;
	default:
		break;
	}

	return ret;
}


//-------------------------------------------------------------------------
// The VST plugin stance scan method.
//

static void qtractor_vst2_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_vst2_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif
	qtractor_vst2_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;
	while (plugin.open_descriptor(i)) {
		sout << "VST2|";
		sout << plugin.name() << '|';
		sout << plugin.numInputs()     << ':' << plugin.numOutputs()     << '|';
		sout << plugin.numMidiInputs() << ':' << plugin.numMidiOutputs() << '|';
		sout << plugin.numParams()     << ':' << 0                       << '|';
		QStringList flags;
		if (plugin.hasEditor())
			flags.append("GUI");
		if (plugin.hasProgramChunks())
			flags.append("EXT");
		flags.append("RT");
		sout << flags.join(",") << '|';
		sout << sFilename << '|' << i << '|';
		sout << "0x" << QString::number(plugin.uniqueID(), 16) << '\n';
		plugin.close_descriptor();
		++i;
	}

	plugin.close();

	// Must always give an answer, even if it's a wrong one...
	if (i == 0)
		sout << "qtractor_vst2_scan: " << sFilename << ": plugin file error.\n";
}

#endif	// CONFIG_VST2


#ifdef CONFIG_VST3

#include "pluginterfaces/vst/ivsthostapplication.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include "pluginterfaces/gui/iplugview.h"

//-----------------------------------------------------------------------------

using namespace Steinberg;

//-----------------------------------------------------------------------------

class qtractor_vst3_scan_host : public Vst::IHostApplication
{
public:

	qtractor_vst3_scan_host ()
	{
		FUNKNOWN_CTOR
	}

	virtual ~qtractor_vst3_scan_host ()
	{
		FUNKNOWN_DTOR
	}

	DECLARE_FUNKNOWN_METHODS

	//--- IHostApplication ----
	//
	tresult PLUGIN_API getName (Vst::String128 name) override
	{
		const QString str("qtractor_plugin_scan");
		const int nsize = qMin(str.length(), 127);
		::memcpy(name, str.utf16(), nsize * sizeof(Vst::TChar));
		name[nsize] = 0;
		return kResultOk;
	}

	tresult PLUGIN_API createInstance (TUID /*cid*/, TUID /*_iid*/, void **obj) override
	{
		*obj = nullptr;
		return kResultFalse;
	}

	FUnknown *get() { return static_cast<Vst::IHostApplication *> (this); }
};


tresult PLUGIN_API qtractor_vst3_scan_host::queryInterface (
	const char *_iid, void **obj )
{
	QUERY_INTERFACE(_iid, obj, FUnknown::iid, IHostApplication)
	QUERY_INTERFACE(_iid, obj, IHostApplication::iid, IHostApplication)

	*obj = nullptr;
	return kNoInterface;
}


uint32 PLUGIN_API qtractor_vst3_scan_host::addRef (void)
	{ return 1;	}

uint32 PLUGIN_API qtractor_vst3_scan_host::release (void)
	{ return 1; }


static qtractor_vst3_scan_host g_vst3HostContext;


//----------------------------------------------------------------------
// class qtractor_vst3_scan::Impl -- VST3 plugin interface impl.
//

class qtractor_vst3_scan::Impl
{
public:

	// Constructor.
	Impl() : m_module(nullptr), m_component(nullptr), m_controller(nullptr) {}

	// destructor.
	~Impl() { close_descriptor(); close(); }

	// File loader.
	bool open ( const QString& sFilename )
	{
		close();

		const QByteArray aFilename = sFilename.toUtf8();
		m_module = ::dlopen(sFilename.toUtf8().constData(), RTLD_LOCAL | RTLD_LAZY);
		if (!m_module)
			return false;

		typedef bool (*VST3_ModuleEntry)(void *);
		const VST3_ModuleEntry module_entry
			= VST3_ModuleEntry(::dlsym(m_module, "ModuleEntry"));
		if (module_entry)
			module_entry(m_module);

		return true;
	}

	bool open_descriptor ( unsigned long iIndex )
	{
		if (!m_module)
			return false;

		close_descriptor();

		typedef IPluginFactory *(*VST3_GetFactory)();
		const VST3_GetFactory get_plugin_factory
			= VST3_GetFactory(::dlsym(m_module, "GetPluginFactory"));
		if (!get_plugin_factory) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst3_scan::Impl[%p]::open_descriptor(%lu)"
				" *** Failed to resolve plug-in factory.", this, iIndex);
		#endif
			return false;
		}

		IPluginFactory *factory = get_plugin_factory();
		if (!factory) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst3_scan::Impl[%p]::open_descriptor(%lu)"
				" *** Failed to retrieve plug-in factory.", this, iIndex);
		#endif
			return false;
		}

		const int32 nclasses = factory->countClasses();

		unsigned long i = 0;

		for (int32 n = 0; n < nclasses; ++n) {

			PClassInfo classInfo;
			if (factory->getClassInfo(n, &classInfo) != kResultOk)
				continue;

			if (::strcmp(classInfo.category, kVstAudioEffectClass))
				continue;

			if (iIndex == i) {

				m_classInfo = classInfo;

				Vst::IComponent *component = nullptr;
				if (factory->createInstance(
						classInfo.cid, Vst::IComponent::iid,
						(void **) &component) != kResultOk) {
				#ifdef CONFIG_DEBUG
					qDebug("qtractor_vst3_scan::Impl[%p]::open_descriptor(%lu)"
						" *** Failed to create plug-in component.", this, iIndex);
				#endif
					return false;
				}

				m_component = owned(component);

				if (m_component->initialize(g_vst3HostContext.get()) != kResultOk) {
				#ifdef CONFIG_DEBUG
					qDebug("qtractor_vst3_scan::Impl[%p]::open_descriptor(%lu)"
						" *** Failed to initialize plug-in component.", this, iIndex);
				#endif
					close_descriptor();
					return false;
				}

				Vst::IEditController *controller = nullptr;
				if (m_component->queryInterface(
						Vst::IEditController::iid,
						(void **) &controller) != kResultOk) {
					TUID controller_cid;
					if (m_component->getControllerClassId(controller_cid) == kResultOk) {
						if (factory->createInstance(
								controller_cid, Vst::IEditController::iid,
								(void **) &controller) != kResultOk) {
						#ifdef CONFIG_DEBUG
							qDebug("qtractor_vst3_scan::Impl[%p]::open_descriptor(%lu)"
								" *** Failed to create plug-in controller.", this, iIndex);
						#endif
						}
						if (controller &&
							controller->initialize(g_vst3HostContext.get()) != kResultOk) {
						#ifdef CONFIG_DEBUG
							qDebug("qtractor_vst3_scan::Impl[%p]::open_descriptor(%lu)"
								" *** Failed to initialize plug-in controller.", this, iIndex);
							controller = nullptr;
						#endif
						}
					}
				}

				if (controller) m_controller = owned(controller);

				// Connect components...
				if (m_component && m_controller) {
					FUnknownPtr<Vst::IConnectionPoint> component_cp(m_component);
					FUnknownPtr<Vst::IConnectionPoint> controller_cp(m_controller);
					if (component_cp && controller_cp) {
						component_cp->connect(controller_cp);
						controller_cp->connect(component_cp);
					}
				}

				return true;
			}

			++i;
		}

		return false;
	}

	void close_descriptor ()
	{
		if (m_component && m_controller) {
			FUnknownPtr<Vst::IConnectionPoint> component_cp(m_component);
			FUnknownPtr<Vst::IConnectionPoint> controller_cp(m_controller);
			if (component_cp && controller_cp) {
				component_cp->disconnect(controller_cp);
				controller_cp->disconnect(component_cp);
			}
		}

		if (m_component && m_controller &&
			FUnknownPtr<Vst::IEditController> (m_component).getInterface()) {
			m_controller->terminate();
		}

		m_controller = nullptr;

		if (m_component) {
			m_component->terminate();
			m_component = nullptr;
		}
	}

	void close ()
	{
		if (!m_module)
			return;

		typedef void (*VST3_ModuleExit)();
		const VST3_ModuleExit module_exit
			= VST3_ModuleExit(::dlsym(m_module, "ModuleExit"));
		if (module_exit)
			module_exit();

		::dlclose(m_module);
		m_module = nullptr;
	}

	// Accessors.
	Vst::IComponent *component() const
		{ return m_component; }
	Vst::IEditController *controller() const
		{ return m_controller; }

	const PClassInfo& classInfo() const
		{ return m_classInfo; }

	int numChannels ( Vst::MediaType type, Vst::BusDirection direction ) const
	{
		if (!m_component)
			return -1;

		int nchannels = 0;
		int nactive = 0;

		const int32 nbuses = m_component->getBusCount(type, direction);
		for (int32 i = 0; i < nbuses; ++i) {
			Vst::BusInfo busInfo;
			if (m_component->getBusInfo(type, direction, i, busInfo) == kResultOk) {
				if (busInfo.busType == Vst::kMain) {
					nchannels += busInfo.channelCount;
					if (busInfo.flags & Vst::BusInfo::kDefaultActive)
						nactive += busInfo.channelCount;
				}
			}
		}

		return (nactive > 0 ? nactive : nchannels);
	}

private:

	// Instance variables.
	void *m_module;

	PClassInfo m_classInfo;

	IPtr<Vst::IComponent> m_component;
	IPtr<Vst::IEditController> m_controller;
};


//----------------------------------------------------------------------
// class qtractor_vst3_scan -- VST3 plugin interface
//

// Constructor.
qtractor_vst3_scan::qtractor_vst3_scan (void) : m_pImpl(new Impl())
{
	clear();
}


// destructor.
qtractor_vst3_scan::~qtractor_vst3_scan (void)
{
	close_descriptor();
	close();

	delete m_pImpl;
}


// File loader.
bool qtractor_vst3_scan::open ( const QString& sFilename )
{
	close();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst3_scan[%p]::open(\"%s\")", this, sFilename.toUtf8().constData());
#endif

	return m_pImpl->open(sFilename);
}


bool qtractor_vst3_scan::open_descriptor ( unsigned long iIndex )
{
	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst3_scan[%p]::open_descriptor( %lu)", this, iIndex);
#endif

	if (!m_pImpl->open_descriptor(iIndex))
		return false;

	const PClassInfo& classInfo = m_pImpl->classInfo();
	m_sName = QString::fromLocal8Bit(classInfo.name);

	m_iUniqueID = qHash(QByteArray(classInfo.cid, sizeof(TUID)));

	m_iAudioIns  = m_pImpl->numChannels(Vst::kAudio, Vst::kInput);
	m_iAudioOuts = m_pImpl->numChannels(Vst::kAudio, Vst::kOutput);
	m_iMidiIns   = m_pImpl->numChannels(Vst::kEvent, Vst::kInput);
	m_iMidiOuts  = m_pImpl->numChannels(Vst::kEvent, Vst::kOutput);

	Vst::IEditController *controller = m_pImpl->controller();
	if (controller) {
		IPtr<IPlugView> editor
			= owned(controller->createView(Vst::ViewType::kEditor));
		m_bEditor = (editor != nullptr);
	}

	m_iControlIns  = 0;
	m_iControlOuts = 0;

	if (controller) {
		const int32 nparams = controller->getParameterCount();
		for (int32 i = 0; i < nparams; ++i) {
			Vst::ParameterInfo paramInfo;
			if (controller->getParameterInfo(i, paramInfo) == kResultOk) {
				if (paramInfo.flags & Vst::ParameterInfo::kIsReadOnly)
					++m_iControlOuts;
				else
				if (paramInfo.flags & Vst::ParameterInfo::kCanAutomate)
					++m_iControlIns;
			}
		}
	}

	return true;
}


// File unloader.
void qtractor_vst3_scan::close_descriptor (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst3_scan[%p]::close_descriptor()", this);
#endif

	m_pImpl->close_descriptor();

	clear();
}


void qtractor_vst3_scan::close (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst3_scan[%p]::close()", this);
#endif

	m_pImpl->close();
}


// Properties.
bool qtractor_vst3_scan::isOpen (void) const
{
	return (m_pImpl->controller() != nullptr);
}


// Cleaner/wiper.
void qtractor_vst3_scan::clear (void)
{
	m_sName.clear();

	m_iUniqueID    = 0;
	m_iControlIns  = 0;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;
	m_bEditor      = false;
}


//-------------------------------------------------------------------------
// qtractor_vst3_scan_file - The main scan procedure.
//

static void qtractor_vst3_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_vst3_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif

	qtractor_vst3_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;
	while (plugin.open_descriptor(i)) {
		sout << "VST3|";
		sout << plugin.name() << '|';
		sout << plugin.audioIns()   << ':' << plugin.audioOuts()   << '|';
		sout << plugin.midiIns()    << ':' << plugin.midiOuts()    << '|';
		sout << plugin.controlIns() << ':' << plugin.controlOuts() << '|';
		QStringList flags;
		if (plugin.hasEditor())
			flags.append("GUI");
		flags.append("EXT");
		flags.append("RT");
		sout << flags.join(",") << '|';
		sout << sFilename << '|' << i << '|';
		sout << "0x" << QString::number(plugin.uniqueID(), 16) << '\n';
		plugin.close_descriptor();
		++i;
	}

	plugin.close();

	// Must always give an answer, even if it's a wrong one...
	if (i == 0)
		sout << "qtractor_vst3_scan: " << sFilename << ": plugin file error.\n";
}

#endif	// CONFIG_VST3


#ifdef CONFIG_CLAP

#include <clap/clap.h>

//----------------------------------------------------------------------
// class qtractor_clap_scan::Impl -- CLAP plugin interface impl.
//

class qtractor_clap_scan::Impl
{
public:

	// Constructor.
	Impl() : m_module(nullptr),
		m_entry(nullptr), m_factory(nullptr), m_plugin(nullptr)
	{
		::memset(&m_host, 0, sizeof(m_host));

		m_host.host_data = this;
		m_host.clap_version = CLAP_VERSION;
		m_host.name = "qtractor_plugin_scan";
		m_host.version = PROJECT_VERSION;
		m_host.vendor = "rncbc.org";
		m_host.url = "https://qtractor.org";
		m_host.get_extension = qtractor_clap_scan::Impl::get_extension;
		m_host.request_restart = qtractor_clap_scan::Impl::request_restart;
		m_host.request_process = qtractor_clap_scan::Impl::request_process;
		m_host.request_callback = qtractor_clap_scan::Impl::request_callback;

		clear();
	}

	// destructor.
	~Impl() { close_descriptor(); close(); }

	static const void *get_extension(const clap_host *host, const char *ext_id)
		{ return nullptr; }

	static void request_restart (const clap_host *host) {}
	static void request_process (const clap_host *host) {}
	static void request_callback(const clap_host *host) {}

	// File loader.
	bool open ( const QString& sFilename )
	{
		close();

		const QByteArray aFilename = sFilename.toUtf8();
		m_module = ::dlopen(aFilename.constData(), RTLD_LOCAL | RTLD_LAZY);
		if (!m_module)
			return false;

		m_entry = reinterpret_cast<const clap_plugin_entry *> (
			::dlsym(m_module, "clap_entry"));
		if (!m_entry)
			return false;

		m_entry->init(aFilename.constData());

		m_factory = static_cast<const clap_plugin_factory *> (
			m_entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
		if (!m_factory)
			return false;

		return true;
	}

	bool open_descriptor ( unsigned long iIndex )
	{
		if (!m_factory)
			return false;

		close_descriptor();

		auto count = m_factory->get_plugin_count(m_factory);
		if (iIndex >= count)
			return false;

		auto desc = m_factory->get_plugin_descriptor(m_factory, iIndex);
		if (!desc) {
			qDebug("qtractor_clap_scan::Impl[%p]::open_descriptor(%lu)"
				" *** No plug-in descriptor.", this, iIndex);
			return false;
		}

		if (!clap_version_is_compatible(desc->clap_version)) {
			qDebug("qtractor_clap_scan::Impl[%p]::open_descriptor(%lu)"
				" *** Incompatible CLAP version:"
				" plug-in is %d.%d.%d, host is %d.%d.%d.", this, iIndex,
				desc->clap_version.major,
				desc->clap_version.minor,
				desc->clap_version.revision,
				CLAP_VERSION.major,
				CLAP_VERSION.minor,
				CLAP_VERSION.revision);
			return false;
		}

		m_plugin = m_factory->create_plugin(m_factory, &m_host, desc->id);
		if (!m_plugin) {
			qDebug("qtractor_clap_scan::Impl[%p]::open_descriptor(%lu)"
				" *** Could not create plug-in with id: %s.", this, iIndex, desc->id);
			return false;
		}

		if (!m_plugin->init(m_plugin)) {
			qDebug("qtractor_clap_scan::Impl[%p]::open_descriptor(%lu)"
				" *** Could not initialize plug-in with id: %s.", this, iIndex, desc->id);
			m_plugin->destroy(m_plugin);
			m_plugin = nullptr;
			return false;
		}

		m_sName = desc->name;
		m_iUniqueID = qHash(desc->id);

		const clap_plugin_audio_ports *audio_ports
			= static_cast<const clap_plugin_audio_ports *> (
				m_plugin->get_extension(m_plugin, CLAP_EXT_AUDIO_PORTS));
		if (audio_ports && audio_ports->count && audio_ports->get) {
			clap_audio_port_info info;
			const uint32_t nins = audio_ports->count(m_plugin, true);
			for (uint32_t i = 0; i < nins; ++i) {
				::memset(&info, 0, sizeof(info));
				if (audio_ports->get(m_plugin, i, true, &info)) {
					if (info.flags & CLAP_AUDIO_PORT_IS_MAIN)
						m_iAudioIns += info.channel_count;
				}
			}
			const uint32_t nouts = audio_ports->count(m_plugin, false);
			for (uint32_t i = 0; i < nouts; ++i) {
				::memset(&info, 0, sizeof(info));
				if (audio_ports->get(m_plugin, i, false, &info)) {
					if (info.flags & CLAP_AUDIO_PORT_IS_MAIN)
						m_iAudioOuts += info.channel_count;
				}
			}
		}

		const clap_plugin_note_ports *note_ports
			= static_cast<const clap_plugin_note_ports *> (
				m_plugin->get_extension(m_plugin, CLAP_EXT_NOTE_PORTS));
		if (note_ports && note_ports->count && note_ports->get) {
			clap_note_port_info info;
			const uint32_t nins = note_ports->count(m_plugin, true);
			for (uint32_t i = 0; i < nins; ++i) {
				::memset(&info, 0, sizeof(info));
				if (note_ports->get(m_plugin, i, true, &info)) {
				//	if (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI)
						++m_iMidiIns;
				}
			}
			const uint32_t nouts = note_ports->count(m_plugin, false);
			for (uint32_t i = 0; i < nouts; ++i) {
				::memset(&info, 0, sizeof(info));
				if (note_ports->get(m_plugin, i, false, &info)) {
				//	if (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI)
						++m_iMidiOuts;
				}
			}
		}

		const clap_plugin_params *params
			= static_cast<const clap_plugin_params *> (
				m_plugin->get_extension(m_plugin, CLAP_EXT_PARAMS));
		if (params && params->count && params->get_info) {
			clap_param_info info;
			const uint32_t nparams = params->count(m_plugin);
			for (uint32_t i = 0; i < nparams; ++i) {
				::memset(&info, 0, sizeof(info));
				if (params->get_info(m_plugin, i, &info)) {
					if (info.flags & CLAP_PARAM_IS_READONLY)
						++m_iControlOuts;
					else
						++m_iControlIns;
				}
			}
		}

		const clap_plugin_gui *gui
			= static_cast<const clap_plugin_gui *> (
				m_plugin->get_extension(m_plugin, CLAP_EXT_GUI));
		if (gui && gui->is_api_supported && gui->create && gui->destroy) {
			m_bEditor = (
				gui->is_api_supported(m_plugin, CLAP_WINDOW_API_X11, false) ||
				gui->is_api_supported(m_plugin, CLAP_WINDOW_API_X11, true));
		}

		const clap_plugin_state *state
			= static_cast<const clap_plugin_state *> (
				m_plugin->get_extension(m_plugin, CLAP_EXT_STATE));
		m_bState = (state && state->save && state->load);

		return true;
	}

	void close_descriptor ()
	{
		if (m_plugin) {
			m_plugin->destroy(m_plugin);
			m_plugin = nullptr;
		}

		clear();
	}

	void close ()
	{
		if (!m_module)
			return;

		m_factory = nullptr;

		if (m_entry) {
			m_entry->deinit();
			m_entry = nullptr;
		}

		::dlclose(m_module);
		m_module = nullptr;
	}

	bool isOpen () const
		{ return (m_module && m_entry && m_factory); }

	// Properties.
	const QString& name() const
		{ return m_sName; }

	unsigned int uniqueID() const
		{ return m_iUniqueID; }

	int controlIns() const
		{ return m_iControlIns; }
	int controlOuts() const
		{ return m_iControlOuts; }

	int audioIns() const
		{ return m_iAudioIns; }
	int audioOuts() const
		{ return m_iAudioOuts; }

	int midiIns() const
		{ return m_iMidiIns; }
	int midiOuts() const
		{ return m_iMidiOuts; }

	bool hasEditor() const
		{ return m_bEditor; }
	bool hasState() const
		{ return m_bState; }

protected:

	// Cleaner/wiper.
	void clear ()
	{
		m_sName.clear();
		m_iUniqueID  = 0;
		m_iControlIns  = 0;
		m_iControlOuts = 0;
		m_iAudioIns  = 0;
		m_iAudioOuts = 0;
		m_iMidiIns   = 0;
		m_iMidiOuts  = 0;
		m_bEditor = false;
		m_bState = false;
	}

private:

	// Instance variables.
	void *m_module;

	const clap_plugin_entry *m_entry;
	const clap_plugin_factory *m_factory;
	const clap_plugin *m_plugin;

	clap_host m_host;

	QString       m_sName;
	unsigned long m_iIndex;
	unsigned int  m_iUniqueID;
	int           m_iControlIns;
	int           m_iControlOuts;
	int           m_iAudioIns;
	int           m_iAudioOuts;
	int           m_iMidiIns;
	int           m_iMidiOuts;
	bool          m_bEditor;
	bool          m_bState;
};


//----------------------------------------------------------------------
// class qtractor_clap_scan -- CLAP plugin (bare bones) interface
//

// Constructor.
qtractor_clap_scan::qtractor_clap_scan (void) : m_pImpl(new Impl())
{
}


// destructor.
qtractor_clap_scan::~qtractor_clap_scan (void)
{
	close_descriptor();
	close();

	delete m_pImpl;
}


// File loader.
bool qtractor_clap_scan::open ( const QString& sFilename )
{
	close();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_clap_scan[%p]::open(\"%s\")", this, sFilename.toUtf8().constData());
#endif

	return m_pImpl->open(sFilename);
}


bool qtractor_clap_scan::open_descriptor ( unsigned long iIndex )
{
	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_clap_scan[%p]::open_descriptor( %lu)", this, iIndex);
#endif

	return m_pImpl->open_descriptor(iIndex);
}


// File unloader.
void qtractor_clap_scan::close_descriptor (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_clap_scan[%p]::close_descriptor()", this);
#endif

	m_pImpl->close_descriptor();
}


void qtractor_clap_scan::close (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_clap_scan[%p]::close()", this);
#endif

	m_pImpl->close();
}


// Properties.
bool qtractor_clap_scan::isOpen (void) const
	{ return m_pImpl->isOpen(); }

const QString& qtractor_clap_scan::name (void) const
	{ return m_pImpl->name(); }

unsigned int qtractor_clap_scan::uniqueID (void) const
	{ return m_pImpl->uniqueID(); }

int qtractor_clap_scan::controlIns (void) const
	{ return m_pImpl->controlIns(); }
int qtractor_clap_scan::controlOuts (void) const
	{ return m_pImpl->controlOuts(); }

int qtractor_clap_scan::audioIns (void) const
	{ return m_pImpl->audioIns(); }
int qtractor_clap_scan::audioOuts (void) const
	{ return m_pImpl->audioOuts(); }

int qtractor_clap_scan::midiIns (void) const
	{ return m_pImpl->midiIns(); }
int qtractor_clap_scan::midiOuts (void) const
	{ return m_pImpl->midiOuts(); }

bool qtractor_clap_scan::hasEditor (void) const
	{ return m_pImpl->hasEditor(); }
bool qtractor_clap_scan::hasState (void) const
	{ return m_pImpl->hasState(); }


//-------------------------------------------------------------------------
// qtractor_clap_scan_file - The main scan procedure.
//

static void qtractor_clap_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_clap_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif

	qtractor_clap_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;
	while (plugin.open_descriptor(i)) {
		sout << "CLAP|";
		sout << plugin.name() << '|';
		sout << plugin.audioIns()   << ':' << plugin.audioOuts()   << '|';
		sout << plugin.midiIns()    << ':' << plugin.midiOuts()    << '|';
		sout << plugin.controlIns() << ':' << plugin.controlOuts() << '|';
		QStringList flags;
		if (plugin.hasEditor())
			flags.append("GUI");
		if (plugin.hasState())
			flags.append("EXT");
		flags.append("RT");
		sout << flags.join(",") << '|';
		sout << sFilename << '|' << i << '|';
		sout << "0x" << QString::number(plugin.uniqueID(), 16) << '\n';
		plugin.close_descriptor();
		++i;
	}

	plugin.close();

	// Must always give an answer, even if it's a wrong one...
	if (i == 0)
		sout << "qtractor_clap_scan: " << sFilename << ": plugin file error.\n";
}


#endif	// CONFIG_CLAP


//-------------------------------------------------------------------------
// main - The main program trunk.
//

#include <QCoreApplication>

int main ( int argc, char **argv )
{
	QCoreApplication app(argc, argv);
#ifdef CONFIG_DEBUG
	qDebug("%s: hello. (version %s)", argv[0], PROJECT_VERSION);
#endif
	QTextStream sin(stdin);
	while (!sin.atEnd()) {
		const QString& sLine = sin.readLine();
		if (!sLine.isEmpty()) {
			const QStringList& req = sLine.split(':');
			const QString& sHint = req.at(0).toUpper();
			const QString& sFilename = req.at(1);
		#ifdef CONFIG_LADSPA
			if (sHint == "LADSPA")
				qtractor_ladspa_scan_file(sFilename);
			else
		#endif
		#ifdef CONFIG_DSSI
			if (sHint == "DSSI")
				qtractor_dssi_scan_file(sFilename);
			else
		#endif
		#ifdef CONFIG_VST2
			if (sHint == "VST2" || sHint == "VST")
				qtractor_vst2_scan_file(sFilename);
			else
		#endif
		#ifdef CONFIG_VST3
			if (sHint == "VST3")
				qtractor_vst3_scan_file(sFilename);
			else
		#endif
		#ifdef CONFIG_CLAP
			if (sHint == "CLAP")
				qtractor_clap_scan_file(sFilename);
			else
		#endif
			break;
		}
	}
#ifdef CONFIG_DEBUG
	qDebug("%s: bye.", argv[0]);
#endif
	return 0;
}


// end of qtractor_plugin_scan.cpp
