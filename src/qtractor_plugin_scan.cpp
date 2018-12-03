// qtractor_plugin_scan.cpp
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
#include "config.h"

#include "qtractor_plugin_scan.h"

#include <QLibrary>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

#include <stdint.h>


#ifdef CONFIG_LADSPA

//----------------------------------------------------------------------
// class qtractor_ladspa_scan -- LADSPA plugin re bones) interface
//

// Constructor.
qtractor_ladspa_scan::qtractor_ladspa_scan (void)
	: m_pLibrary(NULL), m_pLadspaDescriptor(NULL),
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
	if (m_pLibrary == NULL)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	// Retrieve the LADSPA descriptor function, if any...
	LADSPA_Descriptor_Function pfnLadspaDescriptor
		= (LADSPA_Descriptor_Function) m_pLibrary->resolve("ladspa_descriptor");
	if (pfnLadspaDescriptor == NULL) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_ladspa_scan[%p]: plugin does not have DSSI descriptor.", this);
	#endif
		return false;
	}

	// Retrieve LADSPA descriptor if any...
	m_pLadspaDescriptor = (*pfnLadspaDescriptor)(iIndex);
	if (m_pLadspaDescriptor == NULL)
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
	if (m_pLadspaDescriptor == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::close_descriptor()", this);
#endif

	m_pLadspaDescriptor = NULL;

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

	if (m_pLibrary == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::close()", this);
#endif
#if 0
	if (m_pLibrary->isLoaded())
		m_pLibrary->unload();
#endif
	delete m_pLibrary;

	m_pLibrary = NULL;
}


// Check wether plugin is loaded.
bool qtractor_ladspa_scan::isOpen (void) const
{
	if (m_pLibrary == NULL)
		return false;

	return m_pLibrary->isLoaded();
}


unsigned int qtractor_ladspa_scan::uniqueID() const
	{ return (m_pLadspaDescriptor ? m_pLadspaDescriptor->UniqueID : 0); }

bool qtractor_ladspa_scan::isRealtime() const
{
	if (m_pLadspaDescriptor == NULL)
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
	: m_pLibrary(NULL), m_pLadspaDescriptor(NULL),
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
	if (m_pLibrary == NULL)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	// Retrieve the DSSI descriptor function, if any...
	DSSI_Descriptor_Function pfnDssiDescriptor
		= (DSSI_Descriptor_Function) m_pLibrary->resolve("dssi_descriptor");
	if (pfnDssiDescriptor == NULL)
		return NULL;
	if (pfnDssiDescriptor == NULL) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_dssi_scan[%p]: plugin does not have DSSI descriptor.", this);
	#endif
		return false;
	}

	// Retrieve the DSSI descriptor if any...
	m_pDssiDescriptor = (*pfnDssiDescriptor)(iIndex);
	if (m_pDssiDescriptor == NULL)
		return false;

	// We're also a LADSPA one...
	m_pLadspaDescriptor = m_pDssiDescriptor->LADSPA_Plugin;
	if (m_pLadspaDescriptor == NULL)
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

	// Check for GUI editor exacutable...
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
	if (m_pLadspaDescriptor == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::close_descriptor()", this);
#endif

	m_pLadspaDescriptor = NULL;

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

	if (m_pLibrary == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_dssi_scan[%p]::close()", this);
#endif
#if 0
	if (m_pLibrary->isLoaded())
		m_pLibrary->unload();
#endif
	delete m_pLibrary;

	m_pLibrary = NULL;
}


// Check wether plugin is loaded.
bool qtractor_dssi_scan::isOpen (void) const
{
	if (m_pLibrary == NULL)
		return false;

	return m_pLibrary->isLoaded();
}


unsigned int qtractor_dssi_scan::uniqueID() const
	{ return (m_pLadspaDescriptor ? m_pLadspaDescriptor->UniqueID : 0); }

bool qtractor_dssi_scan::isRealtime() const
{
	if (m_pLadspaDescriptor == NULL)
		return false;

	return LADSPA_IS_HARD_RT_CAPABLE(m_pLadspaDescriptor->Properties);
}


bool qtractor_dssi_scan::isConfigure() const
{
	if (m_pDssiDescriptor == NULL)
		return false;

	return (m_pDssiDescriptor->configure != NULL);
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


#ifdef CONFIG_VST

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

static VstIntPtr VSTCALLBACK qtractor_vst_scan_callback (AEffect *effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt);

// Current working VST Shell identifier.
static int g_iVstShellCurrentId = 0;

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
// class qtractor_vst_scan -- VST plugin re bones) interface
//

// Constructor.
qtractor_vst_scan::qtractor_vst_scan (void)
	: m_pLibrary(NULL), m_pEffect(NULL), m_iFlagsEx(0), m_bEditor(false)
{
}


// destructor.
qtractor_vst_scan::~qtractor_vst_scan (void)
{
	close();
}


// File loader.
bool qtractor_vst_scan::open ( const QString& sFilename )
{
	close();

	if (!QLibrary::isLibrary(sFilename))
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst_scan[%p]::open(\"%s\", %lu)", this,
		sFilename.toUtf8().constData());
#endif

	m_pLibrary = new QLibrary(sFilename);

	m_sName = QFileInfo(sFilename).baseName();

	return true;
}


// Plugin loader.
bool qtractor_vst_scan::open_descriptor ( unsigned long iIndex )
{
	if (m_pLibrary == NULL)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	VST_GetPluginInstance pfnGetPluginInstance
		= (VST_GetPluginInstance) m_pLibrary->resolve("VSTPluginMain");
	if (pfnGetPluginInstance == NULL)
		pfnGetPluginInstance = (VST_GetPluginInstance) m_pLibrary->resolve("main");
	if (pfnGetPluginInstance == NULL) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst_scan[%p]: plugin does not have a main entry point.", this);
	#endif
		return false;
	}

	m_pEffect = (*pfnGetPluginInstance)(qtractor_vst_scan_callback);
	if (m_pEffect == NULL) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst_scan[%p]: plugin instance could not be created.", this);
	#endif
		return false;
	}

	// Did VST plugin instantiated OK?
	if (m_pEffect->magic != kEffectMagic) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst_scan[%p]: plugin is not a valid VST.", this);
	#endif
		m_pEffect = NULL;
		return false;
	}

	// Check whether it's a VST Shell...
	const int categ = vst_dispatch(effGetPlugCategory, 0, 0, NULL, 0.0f);
	if (categ == kPlugCategShell) {
		int id = 0;
		char buf[40];
		unsigned long i = 0;
		for ( ; iIndex >= i; ++i) {
			buf[0] = (char) 0;
			id = vst_dispatch(effShellGetNextPlugin, 0, 0, (void *) buf, 0.0f);
			if (id == 0 || !buf[0])
				break;
		}
		// Check if we're actually the intended plugin...
		if (i < iIndex || id == 0 || !buf[0]) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst_scan[%p]: "
				"vst_shell(%lu) plugin is not a valid VST.", this, iIndex);
		#endif
			m_pEffect = NULL;
			return false;
		}
		// Make it known...
		g_iVstShellCurrentId = id;
		// Re-allocate the thing all over again...
		pfnGetPluginInstance
			= (VST_GetPluginInstance) m_pLibrary->resolve("VSTPluginMain");
		if (pfnGetPluginInstance == NULL)
			pfnGetPluginInstance = (VST_GetPluginInstance) m_pLibrary->resolve("main");
		if (pfnGetPluginInstance == NULL) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst_scan[%p]: "
				"vst_shell(%lu) plugin does not have a main entry point.", this, iIndex);
		#endif
			m_pEffect = NULL;
			return false;
		}
		// Does the VST plugin instantiate OK?
		m_pEffect = (*pfnGetPluginInstance)(qtractor_vst_scan_callback);
		// Not needed anymore, hopefully...
		g_iVstShellCurrentId = 0;
		// Don't go further if failed...
		if (m_pEffect == NULL) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst_scan[%p]: "
				"vst_shell(%lu) plugin instance could not be created.", this, iIndex);
		#endif
			return false;
		}
		if (m_pEffect->magic != kEffectMagic) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractor_vst_scan[%p]: "
				"vst_shell(%lu) plugin is not a valid VST.", this, iIndex);
		#endif
			m_pEffect = NULL;
			return false;
		}
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_vst_scan[%p]: "
			"vst_shell(%lu) id=0x%x name=\"%s\"", this, i, id, buf);
	#endif
	}
	else
	// Not a VST Shell plugin...
	if (iIndex > 0) {
		m_pEffect = NULL;
		return false;
	}

//	vst_dispatch(effIdentify, 0, 0, NULL, 0.0f);
	vst_dispatch(effOpen,     0, 0, NULL, 0.0f);

	// Get label name...
	char szName[256]; ::memset(szName, 0, sizeof(szName));
	if (vst_dispatch(effGetEffectName, 0, 0, (void *) szName, 0.0f))
		m_sName = szName;

	// Specific inquiries...
	m_iFlagsEx = 0;

	if (canDo("sendVstEvents"))       m_iFlagsEx |= effFlagsExCanSendVstEvents;
	if (canDo("sendVstMidiEvent"))    m_iFlagsEx |= effFlagsExCanSendVstMidiEvents;
	if (canDo("sendVstTimeInfo"))     m_iFlagsEx |= effFlagsExCanSendVstTimeInfo;
	if (canDo("receiveVstEvents"))    m_iFlagsEx |= effFlagsExCanReceiveVstEvents;
	if (canDo("receiveVstMidiEvent")) m_iFlagsEx |= effFlagsExCanReceiveVstMidiEvents;
	if (canDo("receiveVstTimeInfo"))  m_iFlagsEx |= effFlagsExCanReceiveVstTimeInfo;
	if (canDo("offline"))             m_iFlagsEx |= effFlagsExCanProcessOffline;
	if (canDo("plugAsChannelInsert")) m_iFlagsEx |= effFlagsExCanUseAsInsert;
	if (canDo("plugAsSend"))          m_iFlagsEx |= effFlagsExCanUseAsSend;
	if (canDo("mixDryWet"))           m_iFlagsEx |= effFlagsExCanMixDryWet;
	if (canDo("midiProgramNames"))    m_iFlagsEx |= effFlagsExCanMidiProgramNames;

	m_bEditor = (m_pEffect->flags & effFlagsHasEditor);

	return true;
}


// Plugin unloader.
void qtractor_vst_scan::close_descriptor (void)
{
	if (m_pEffect == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst_scan[%p]::close_descriptor()", this);
#endif

	vst_dispatch(effClose, 0, 0, 0, 0.0f);

	m_pEffect  = NULL;
	m_iFlagsEx = 0;
//	m_bEditor  = false;
	m_sName.clear();
}


// File unloader.
void qtractor_vst_scan::close (void)
{
	if (m_pLibrary == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_vst_scan[%p]::close()", this);
#endif

	vst_dispatch(effClose, 0, 0, 0, 0.0f);

	if (m_pLibrary->isLoaded() && !m_bEditor)
		m_pLibrary->unload();

	delete m_pLibrary;

	m_pLibrary = NULL;

	m_bEditor = false;
}


// Check wether plugin is loaded.
bool qtractor_vst_scan::isOpen (void) const
{
	if (m_pLibrary == NULL)
		return false;

	return m_pLibrary->isLoaded();
}

unsigned int qtractor_vst_scan::uniqueID() const
	{ return (m_pEffect ? m_pEffect->uniqueID : 0); }

int qtractor_vst_scan::numPrograms() const
	{ return (m_pEffect ? m_pEffect->numPrograms : 0); }
int qtractor_vst_scan::numParams() const
	{ return (m_pEffect ? m_pEffect->numParams : 0); }
int qtractor_vst_scan::numInputs() const
	{ return (m_pEffect ? m_pEffect->numInputs : 0); }
int qtractor_vst_scan::numOutputs() const
	{ return (m_pEffect ? m_pEffect->numOutputs : 0); }

int qtractor_vst_scan::numMidiInputs() const
	{ return (m_pEffect && (
		(m_iFlagsEx & effFlagsExCanReceiveVstMidiEvents) ||
		(m_pEffect->flags & effFlagsIsSynth) ? 1 : 0)); }

int qtractor_vst_scan::numMidiOutputs() const
	{ return ((m_iFlagsEx & effFlagsExCanSendVstMidiEvents) ? 1 : 0); }

bool qtractor_vst_scan::hasEditor() const
	{ return m_bEditor; }
bool qtractor_vst_scan::hasProgramChunks() const
	{ return (m_pEffect && (m_pEffect->flags & effFlagsProgramChunks)); }


// VST host dispatcher.
int qtractor_vst_scan::vst_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pEffect == NULL)
		return 0;

#ifdef CONFIG_DEBUG_0
	qDebug("vst_plugin[%p]::vst_dispatch(%ld, %ld, %ld, %p, %g)",
		this, opcode, index, value, ptr, opt);
#endif

	return m_pEffect->dispatcher(m_pEffect, opcode, index, value, ptr, opt);
}


// VST flag inquirer.
bool qtractor_vst_scan::canDo ( const char *pszCanDo ) const
{
	return (vst_dispatch(effCanDo, 0, 0, (void *) pszCanDo, 0.0f) > 0);
}


//----------------------------------------------------------------------
// The magnificient host callback, which every VST plugin will call.

static VstIntPtr VSTCALLBACK qtractor_vst_scan_callback ( AEffect* effect,
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
		ret = (VstIntPtr) g_iVstShellCurrentId;
		break;
	case audioMasterGetSampleRate:
		effect->dispatcher(effect, effSetSampleRate, 0, 0, NULL, 44100.0f);
		break;
	case audioMasterGetBlockSize:
		effect->dispatcher(effect, effSetBlockSize, 0, 1024, NULL, 0.0f);
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

static void qtractor_vst_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_vst_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif
	qtractor_vst_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;
	while (plugin.open_descriptor(i)) {
		sout << "VST|";
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
		sout << "qtractor_vst_scan: " << sFilename << ": plugin file error.\n";
}

#endif	// CONFIG_VST


//-------------------------------------------------------------------------
// main - The main program trunk.
//

#include <QCoreApplication>

int main ( int argc, char **argv )
{
	QCoreApplication app(argc, argv);
#ifdef CONFIG_DEBUG
	qDebug("%s: hello. (version %s)", argv[0], CONFIG_BUILD_VERSION);
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
		#ifdef CONFIG_VST
			if (sHint == "VST")
				qtractor_vst_scan_file(sFilename);
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
