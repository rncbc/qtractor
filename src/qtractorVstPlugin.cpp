// qtractorVstPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"

#ifdef CONFIG_VST

#include "qtractorVstPlugin.h"

#include "qtractorPluginForm.h"

#include "qtractorMainForm.h"
#include "qtractorSession.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QWidget>

#if defined(Q_WS_X11)
#include <QX11Info>
#endif

#if !defined(VST_2_3_EXTENSIONS) 
typedef long VstInt32;
typedef long VstIntPtr;
#define VSTCALLBACK
#endif

typedef AEffect* (*VST_GetPluginInstance) (audioMasterCallback);

static VstIntPtr VSTCALLBACK qtractorVstPlugin_HostCallback (AEffect* effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt);

// Specific extended flags that saves us
// from calling canDo() in audio callbacks.
enum qtractorVstPluginFlagsEx
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


//----------------------------------------------------------------------------
// qtractorVstPluginType::Effect -- VST AEffect instance.
//

class qtractorVstPluginType::Effect
{
public:

	// Constructor.
	Effect(AEffect *pVstEffect = NULL) : m_pVstEffect(pVstEffect) {}

	// Destructor.
	~Effect() { close(); }

	// Specific methods.
	bool open(qtractorPluginFile *pFile);
	void close();

	// Specific accessors.
	AEffect *vst_effect() const { return m_pVstEffect; }

	// VST host dispatcher.
	int vst_dispatch(
		long opcode, long index, long value, void *ptr, float opt) const;

private:

	// VST descriptor itself.
	AEffect *m_pVstEffect;
};


// Specific methods.
bool qtractorVstPluginType::Effect::open ( qtractorPluginFile *pFile )
{
	// Do we have an effect descriptor already?
	if (m_pVstEffect)
		return true;

	m_pVstEffect = qtractorVstPluginType::vst_effect(pFile);
	if (m_pVstEffect == NULL)
		return false;

	// Did VST plugin instantiated OK?
	if (m_pVstEffect->magic != kEffectMagic)
		return false;

//	vst_dispatch(effIdentify, 0, 0, NULL, 0);
	vst_dispatch(effOpen,0, 0, NULL, 0.0f);
	vst_dispatch(effMainsChanged, 0, 0, NULL, 0.0f);

	return true;
}


void qtractorVstPluginType::Effect::close (void)
{
	vst_dispatch(effClose, 0, 0, 0, 0.0f);

	m_pVstEffect = NULL;
}


// VST host dispatcher.
int qtractorVstPluginType::Effect::vst_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pVstEffect == NULL)
		return 0;

#ifdef CONFIG_DEBUG
	qDebug("AEffect[%p]::dispatcher(%ld, %ld, %ld, %p, %g)",
		m_pVstEffect, opcode, index, value, ptr, opt);
#endif

	return m_pVstEffect->dispatcher(m_pVstEffect, opcode, index, value, ptr, opt);
}


//----------------------------------------------------------------------------
// qtractorVstPluginType -- VST plugin type instance.
//

// Derived methods.
bool qtractorVstPluginType::open (void)
{
	// Do we have an effect descriptor already?
	if (m_pEffect == NULL) {
		m_pEffect = new Effect();
		if (!m_pEffect->open(file())) {
			delete m_pEffect;
			m_pEffect = NULL;
			return false;
		}
	}

	// We need the internal descriptor here...
	AEffect *pVstEffect = m_pEffect->vst_effect();
	if (pVstEffect == NULL)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPluginType[%p]::open() filename=\"%s\" index=%lu",
		this, file()->filename().toUtf8().constData(), index());
#endif

	// Retrieve plugin type names.
	char szName[256];
	if (vst_dispatch(effGetEffectName, 0, 0, (void *) szName, 0.0f))
		m_sName = szName;
	else
		m_sName = QFileInfo(file()->filename()).baseName();
	m_sLabel = m_sName;

	// Retrieve plugin unique identifier.
	m_iUniqueID = pVstEffect->uniqueID;

	// Specific inquiries...
	m_iFlagsEx = 0;
//	if (vst_canDo("sendVstEvents"))       m_iFlagsEx |= effFlagsExCanSendVstEvents;
	if (vst_canDo("sendVstMidiEvent"))    m_iFlagsEx |= effFlagsExCanSendVstMidiEvents;
//	if (vst_canDo("sendVstTimeInfo"))     m_iFlagsEx |= effFlagsExCanSendVstTimeInfo;
//	if (vst_canDo("receiveVstEvents"))    m_iFlagsEx |= effFlagsExCanReceiveVstEvents;
	if (vst_canDo("receiveVstMidiEvent")) m_iFlagsEx |= effFlagsExCanReceiveVstMidiEvents;
//	if (vst_canDo("receiveVstTimeInfo"))  m_iFlagsEx |= effFlagsExCanReceiveVstTimeInfo;
//	if (vst_canDo("offline"))             m_iFlagsEx |= effFlagsExCanProcessOffline;
//	if (vst_canDo("plugAsChannelInsert")) m_iFlagsEx |= effFlagsExCanUseAsInsert;
//	if (vst_canDo("plugAsSend"))          m_iFlagsEx |= effFlagsExCanUseAsSend;
//	if (vst_canDo("mixDryWet"))           m_iFlagsEx |= effFlagsExCanMixDryWet;
	if (vst_canDo("midiProgramNames"))    m_iFlagsEx |= effFlagsExCanMidiProgramNames;

	// Compute and cache port counts...
	m_iControlIns  = pVstEffect->numParams;
	m_iControlOuts = 0;
	m_iAudioIns    = pVstEffect->numInputs;
	m_iAudioOuts   = pVstEffect->numOutputs;
	m_iMidiIns     = ((m_iFlagsEx & effFlagsExCanReceiveVstMidiEvents)
		|| (pVstEffect->flags & effFlagsIsSynth) ? 1 : 0);
	m_iMidiOuts    = ((m_iFlagsEx & effFlagsExCanSendVstMidiEvents) ? 1 : 0);

	// Cache flags.
	m_bRealtime = true;
	m_bEditor   = (pVstEffect->flags & effFlagsHasEditor);

	// Set to program zero...
	vst_dispatch(effSetProgram, 0, 0, NULL, 0.0f);

	return true;
}


void qtractorVstPluginType::close (void)
{
	if (m_pEffect) {
		delete m_pEffect;
		m_pEffect = NULL;
	}
}


// Factory method (static)
qtractorVstPluginType *qtractorVstPluginType::createType (
	qtractorPluginFile *pFile )
{
	// Sanity check...
	if (pFile == NULL)
		return NULL;

	// Retrieve effect instance if any...
	AEffect *pVstEffect = vst_effect(pFile);
	if (pVstEffect == NULL)
		return NULL;

	// Yep, most probably its a valid plugin effect...
	return new qtractorVstPluginType(pFile, new Effect(pVstEffect));
}


// Effect instance method (static)
AEffect *qtractorVstPluginType::vst_effect ( qtractorPluginFile *pFile )
{
	VST_GetPluginInstance pfnGetPluginInstance
		= (VST_GetPluginInstance) pFile->resolve("VSTPluginMain");
	if (pfnGetPluginInstance == NULL)
		pfnGetPluginInstance = (VST_GetPluginInstance) pFile->resolve("main");
	if (pfnGetPluginInstance == NULL)
		return NULL;

	return (*pfnGetPluginInstance)(qtractorVstPlugin_HostCallback);
}


// VST host dispatcher.
int qtractorVstPluginType::vst_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pEffect == NULL)
		return 0;

	return m_pEffect->vst_dispatch(opcode, index, value, ptr, opt);
}


// VST flag inquirer.
bool qtractorVstPluginType::vst_canDo ( const char *pszCanDo ) const
{
	if (m_pEffect == NULL)
		return false;

	return (m_pEffect->vst_dispatch(effCanDo, 0, 0, (void *) pszCanDo, 0.0f) > 0);
}


//----------------------------------------------------------------------------
// qtractorVstPlugin -- VST plugin instance.
//

// Dynamic singleton list of VST plugins.
QList<qtractorVstPlugin *> qtractorVstPlugin::g_vstPlugins;

// Constructors.
qtractorVstPlugin::qtractorVstPlugin ( qtractorPluginList *pList,
	qtractorVstPluginType *pVstType )
	: qtractorPlugin(pList, pVstType), m_ppEffects(NULL),
		m_ppIBuffer(NULL), m_ppOBuffer(NULL), m_bIdleTimer(false)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p] filename=\"%s\" index=%lu typeHint=%d",
		this, (type()->file())->filename().toUtf8().constData(),
		type()->index(), int(type()->typeHint()));
#endif

	// Allocate I/O audio buffer pointers.
	unsigned short iAudioIns  = pVstType->audioIns();
	unsigned short iAudioOuts = pVstType->audioOuts();
	if (iAudioIns > 0)
		m_ppIBuffer = new float * [iAudioIns];
	if (iAudioOuts > 0)
		m_ppOBuffer = new float * [iAudioOuts];

	// Instantiate each instance properly...
	setChannels(channels());

	// Create all existing parameters...
	unsigned short iParamCount = pVstType->controlIns();
	for (unsigned short i = 0; i < iParamCount; ++i)
		addParam(new qtractorVstPluginParam(this, i));

	// Add to global list, now.
	g_vstPlugins.append(this);
}


// Destructor.
qtractorVstPlugin::~qtractorVstPlugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);

	// Remove from global list, now.
	int iVstPlugin = g_vstPlugins.indexOf(this);
	if (iVstPlugin >= 0)
		g_vstPlugins.removeAt(iVstPlugin);

	// Deallocate I/O audio buffer pointers.
	if (m_ppIBuffer)
		delete [] m_ppIBuffer;
	if (m_ppOBuffer)
		delete [] m_ppOBuffer;
}


// Channel/instance number accessors.
void qtractorVstPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorVstPluginType *pVstType
		= static_cast<qtractorVstPluginType *> (type());
	if (pVstType == NULL)
		return;
		
	// Estimate the (new) number of instances...
	unsigned short iInstances
		= pVstType->instances(iChannels, pVstType->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == instances())
		return;

	// Gotta go for a while...
	bool bActivated = isActivated();
	setActivated(false);

	if (m_ppEffects) {
		for (unsigned short i = 1; i < instances(); ++i)
			delete m_ppEffects[i];
		delete [] m_ppEffects;
		m_ppEffects = NULL;
	}

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1)
		return;

	// Allocate new instances...
	m_ppEffects = new  qtractorVstPluginType::Effect * [iInstances];
	m_ppEffects[0] = pVstType->effect();
	for (unsigned short i = 1; i < iInstances; ++i) {
		m_ppEffects[i] = new qtractorVstPluginType::Effect();
		m_ppEffects[i]->open(pVstType->file());
	}

	// Setup all those instance alright...
	for (unsigned short i = 0; i < iInstances; ++i) {
		qtractorVstPluginType::Effect *pEffect = m_ppEffects[i];
		pEffect->vst_dispatch(effSetSampleRate, 0, 0, NULL, float(sampleRate()));
		pEffect->vst_dispatch(effSetBlockSize,  0, bufferSize(), NULL, 0.0f);
	}

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Do the actual activation.
void qtractorVstPlugin::activate (void)
{
	for (unsigned short i = 0; i < instances(); ++i)
		vst_dispatch(i, effMainsChanged, 0, 1, NULL, 0.0f);
}


// Do the actual deactivation.
void qtractorVstPlugin::deactivate (void)
{
	for (unsigned short i = 0; i < instances(); ++i)
		vst_dispatch(i, effMainsChanged, 0, 0, NULL, 0.0f);
}


// The main plugin processing procedure.
void qtractorVstPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_ppEffects == NULL)
		return;
	if (m_ppIBuffer == NULL || m_ppOBuffer == NULL)
		return;

	// We'll cross channels over instances...
	unsigned short iInstances = instances();
	unsigned short iChannels  = channels();
	unsigned short iAudioIns  = audioInsCap();
	unsigned short iAudioOuts = audioOutsCap();
	unsigned short iIChannel  = 0;
	unsigned short iOChannel  = 0;
	unsigned short i, j;

	// For each plugin instance...
	for (i = 0; i < iInstances; ++i) {
		AEffect *pVstEffect = vst_effect(i);
		// For each instance audio input port...
		for (j = 0; j < iAudioIns; ++j) {
			m_ppIBuffer[j] = ppIBuffer[iIChannel];
			if (++iIChannel >= iChannels)
				iIChannel = 0;
		}
		// For each instance audio output port...
		for (j = 0; j < iAudioOuts; ++j) {
			m_ppOBuffer[j] = ppOBuffer[iOChannel];
			::memset(m_ppOBuffer[j], 0, nframes * sizeof(float));
			if (++iOChannel >= iChannels)
				iOChannel = 0;
		}
		// Make it run...
		if (pVstEffect->flags & effFlagsCanReplacing) {
			pVstEffect->processReplacing(
				pVstEffect, m_ppIBuffer, m_ppOBuffer, nframes);
		} else {
			pVstEffect->process(
				pVstEffect, m_ppIBuffer, m_ppOBuffer, nframes);
		}
		// Wrap channels?...
		if (iIChannel < iChannels - 1)
			iIChannel++;
		if (iOChannel < iChannels - 1)
			iOChannel++;
	}
}


// Specific accessors.
AEffect *qtractorVstPlugin::vst_effect ( unsigned short iInstance ) const
{
	if (m_ppEffects == NULL)
		return NULL;

	return m_ppEffects[iInstance]->vst_effect();
}


// VST host dispatcher.
int qtractorVstPlugin::vst_dispatch( unsigned short iInstance,
	long opcode, long index, long value, void *ptr, float opt) const
{
	if (m_ppEffects == NULL)
		return 0;

	return m_ppEffects[iInstance]->vst_dispatch(opcode, index, value, ptr, opt);
}


// Open editor.
void qtractorVstPlugin::openEditor ( QWidget *pParent )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::openEditor(%p)", this, pParent);
#endif
	long  value = 0;
	void *ptr = (void *) pParent->winId();
#if defined(Q_WS_X11)
	value = (long) QX11Info::display();
#endif
	vst_dispatch(0, effEditOpen, 0, value, ptr, 0.0f);
}


// Close editor.
void qtractorVstPlugin::closeEditor (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::closeEditor()", this);
#endif
	vst_dispatch(0, effEditClose, 0, 0, NULL, 0.0f);
}


// Idle editor.
void qtractorVstPlugin::idleEditor (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::idleEditor()", this);
#endif
	vst_dispatch(0, effEditIdle, 0, 0, NULL, 0.0f);
}


// Idle editor (static).
void qtractorVstPlugin::idleEditorAll (void)
{
	QListIterator<qtractorVstPlugin *> iter(g_vstPlugins);
	while (iter.hasNext())
		iter.next()->idleEditor();
}


// Idle editor (static).
void qtractorVstPlugin::idleTimerAll (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin::idleTimerAll()");
#endif
	QListIterator<qtractorVstPlugin *> iter(g_vstPlugins);
	while (iter.hasNext()) {
		qtractorVstPlugin *pVstPlugin = iter.next();
		if (pVstPlugin->isIdleTimer()) {
			// Dispatch idleness for all instances...
			unsigned short iBusyTimer = 0;
			for (unsigned short i = 0; pVstPlugin->instances(); ++i) {
				if (!pVstPlugin->vst_dispatch(0, effIdle, 0, 0, NULL, 0.0f))
					iBusyTimer++;
			}
			// Are we into idleness still?
			if (iBusyTimer > 0)
				pVstPlugin->setIdleTimer(false);
		}
	}
}


// Retrieve editor size extents.
QSize qtractorVstPlugin::editorSize (void)
{
	QSize esize;

	struct ERect {
		short top;
		short left;
		short bottom;
		short right;
	} *erect;

	if (vst_dispatch(0, effEditGetRect, 0, 0, &erect, 0.0f)) {
		esize.setWidth(erect->right - erect->left);
		esize.setHeight(erect->bottom - erect->top);
	}

	return esize;
}


// Global VST plugin lookup.
qtractorVstPlugin *qtractorVstPlugin::findPlugin ( AEffect *pVstEffect )
{
	QListIterator<qtractorVstPlugin *> iter(g_vstPlugins);
	while (iter.hasNext()) {
		qtractorVstPlugin *pVstPlugin = iter.next();
		for (unsigned short i = 0; i < pVstPlugin->instances(); ++i) {
			if (pVstPlugin->vst_effect(i) == pVstEffect)
				return pVstPlugin;
		}
	}

	return NULL;
}


//----------------------------------------------------------------------------
// qtractorVstPluginParam -- VST plugin control input port instance.
//

// Constructors.
qtractorVstPluginParam::qtractorVstPluginParam (
	qtractorVstPlugin *pVstPlugin, unsigned long iIndex )
	: qtractorPluginParam(pVstPlugin, iIndex)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPluginParam[%p] pVstPlugin=%p iIndex=%lu", this, pVstPlugin, iIndex);
#endif

	char szName[64];
	::snprintf(szName, sizeof(szName), "Param #%lu", iIndex); // Default dummy name.
	pVstPlugin->vst_dispatch(0, effGetParamName, iIndex, 0, (void *) szName, 0.0f);
	setName(szName);

	setMinValue(0.0f);
	setMaxValue(1.0f);

	::memset(&m_props, 0, sizeof(m_props));

	if (pVstPlugin->vst_dispatch(0, effGetParameterProperties,
			iIndex, 0, (void *) &m_props, 0.0f)) {
#ifdef CONFIG_DEBUG
		qDebug("  VstParamProperties(%lu) {", iIndex);
		qDebug("    .stepFloat               = %g", m_props.stepFloat);
		qDebug("    .smallStepFloat          = %g", m_props.smallStepFloat);
		qDebug("    .largeStepFloat          = %g", m_props.largeStepFloat);
		qDebug("    .label                   = \"%s\"", m_props.label);
		qDebug("    >IsSwitch                = %d", (m_props.flags & kVstParameterIsSwitch ? 1 : 0));
		qDebug("    >UsesIntegerMinMax       = %d", (m_props.flags & kVstParameterUsesIntegerMinMax ? 1 : 0));
		qDebug("    >UsesFloatStep           = %d", (m_props.flags & kVstParameterUsesFloatStep ? 1 : 0));
		qDebug("    >UsesIntStep             = %d", (m_props.flags & kVstParameterUsesIntStep ? 1 : 0));
		qDebug("    >SupportsDisplayIndex    = %d", (m_props.flags & kVstParameterSupportsDisplayIndex ? 1 : 0));
		qDebug("    >SupportsDisplayCategory = %d", (m_props.flags & kVstParameterSupportsDisplayCategory ? 1 : 0));
		qDebug("    >CanRamp                 = %d", (m_props.flags & kVstParameterCanRamp ? 1 : 0));
		qDebug("    .minInteger              = %d", int(m_props.minInteger));
		qDebug("    .maxInteger              = %d", int(m_props.maxInteger));
		qDebug("    .stepInteger             = %d", int(m_props.stepInteger));
		qDebug("    .largeStepInteger        = %d", int(m_props.largeStepInteger));
		qDebug("    .shortLabel              = \"%s\"", m_props.shortLabel);
		qDebug("    .displayIndex            = %d", m_props.displayIndex);
		qDebug("    .category                = %d", m_props.category);
		qDebug("    .numParametersInCategory = %d", m_props.numParametersInCategory);
		qDebug("    .categoryLabel           = \"%s\"", m_props.categoryLabel);
		qDebug("}");
#endif
		if (isBoundedBelow())
			setMinValue(float(m_props.minInteger));
		if (isBoundedAbove())
			setMaxValue(float(m_props.maxInteger));
	}


	// ATTN: Set default value as initial one...
	setDefaultValue(value());

	// Initialize port value...
	// reset();
}


// Destructor.
qtractorVstPluginParam::~qtractorVstPluginParam (void)
{
}


// Port range hints predicate methods.
bool qtractorVstPluginParam::isBoundedBelow (void) const
{
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
}

bool qtractorVstPluginParam::isBoundedAbove (void) const
{
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
}

bool qtractorVstPluginParam::isDefaultValue (void) const
{
	return true;
}

bool qtractorVstPluginParam::isLogarithmic (void) const
{
	return false;
}

bool qtractorVstPluginParam::isSampleRate (void) const
{
	return false;
}

bool qtractorVstPluginParam::isInteger (void) const
{
	return (m_props.flags & kVstParameterUsesIntStep);
}

bool qtractorVstPluginParam::isToggled (void) const
{
	return (m_props.flags & kVstParameterIsSwitch);
}

bool qtractorVstPluginParam::isDisplay (void) const
{
	return true;
}


// Current display value.
QString qtractorVstPluginParam::display (void) const
{
	qtractorVstPlugin *pVstPlugin
		= static_cast<qtractorVstPlugin *> (plugin());
	if (pVstPlugin) {
		QString sDisplay;
		char szText[64];
		// Grab parameter display value...
		szText[0] = '\0';
		pVstPlugin->vst_dispatch(0,
			effGetParamDisplay, index(), 0, (void *) szText, 0.0f);
		if (szText[0]) {
			// Got
			sDisplay += QString(szText).trimmed();
			// Grab parameter name...
			szText[0] = '\0';
			pVstPlugin->vst_dispatch(0,
				effGetParamLabel, index(), 0, (void *) szText, 0.0f);
			if (szText[0])
				sDisplay += ' ' + QString(szText).trimmed();
			// Got it.
			return sDisplay;
		}
	}

	// Default parameter display value...
	return qtractorPluginParam::display();
}


// Current parameter value.
void qtractorVstPluginParam::setValue ( float fValue )
{
	qtractorPluginParam::setValue(fValue);

	qtractorVstPlugin *pVstPlugin
		= static_cast<qtractorVstPlugin *> (plugin());
	if (pVstPlugin == NULL)
		return;

	for (unsigned short i = 0; i < pVstPlugin->instances(); ++i) {
		AEffect *pVstEffect = pVstPlugin->vst_effect(i);
		if (pVstEffect) {
			pVstEffect->setParameter(pVstEffect, index(), fValue);
		}
	}
}

float qtractorVstPluginParam::value (void) const
{
	float fValue = qtractorPluginParam::value();

	qtractorVstPlugin *pVstPlugin
		= static_cast<qtractorVstPlugin *> (plugin());
	if (pVstPlugin) {
		AEffect *pVstEffect = pVstPlugin->vst_effect(0);
		if (pVstEffect)
			fValue = pVstEffect->getParameter(pVstEffect, index());
	}

	return fValue;
}


//----------------------------------------------------------------------
// VST host callback file selection helpers.

static VstIntPtr qtractorVstPlugin_openFileSelector ( AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVstPlugin *pVstPlugin = qtractorVstPlugin::findPlugin(effect);
	if (pVstPlugin == NULL)
		return 0;

	qDebug("qtractorVstPlugin_openFileSelector(%p, %p) command=%d reserved=%d",
		effect, pvfs, int(pvfs->command), int(pvfs->reserved));

	pvfs->reserved = 0;
	pvfs->nbReturnPath = 0;

    if (pvfs->command == kVstFileLoad || pvfs->command == kVstFileSave) {
		QString sFilename;
        QStringList filters;
        for (int i = 0; i < pvfs->nbFileTypes; ++i) {
            filters.append(QObject::tr("%1 (*.%2)")
				.arg(pvfs->fileTypes[i].name).arg(pvfs->fileTypes[i].dosType));
		}
        filters.append(QObject::tr("All Files (*.*)"));
		QString sFilter = filters.join(";;");
		QString sDirectory = pvfs->initialPath;
		QString sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg((pVstPlugin->type())->name());
		if (pvfs->command == kVstFileLoad) {
			sFilename = QFileDialog::getOpenFileName(
				(pVstPlugin->form())->editorWidget(),
				sTitle, sDirectory, sFilter);
		} else {
			sFilename = QFileDialog::getSaveFileName(
				(pVstPlugin->form())->editorWidget(),
				sTitle, sDirectory, sFilter);
		}
		if (!sFilename.isEmpty()) {
			if (pvfs->returnPath == NULL) {
				pvfs->returnPath = new char [sFilename.length() + 1];
				pvfs->reserved   = 1;
			}
			::strcpy(pvfs->returnPath, sFilename.toUtf8().constData());
			pvfs->nbReturnPath = 1;
		}
    }
	else
	if (pvfs->command == kVstDirectorySelect) {
		QString sDirectory = pvfs->initialPath;
		QString sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg((pVstPlugin->type())->name());
		sDirectory = QFileDialog::getExistingDirectory(
			(pVstPlugin->form())->editorWidget(),
			sTitle, sDirectory);
		if (!sDirectory.isEmpty()) {
			if (pvfs->returnPath == NULL) {
				pvfs->returnPath = new char [sDirectory.length() + 1];
				pvfs->reserved   = 1;
			}
			::strcpy(pvfs->returnPath, sDirectory.toUtf8().constData());
			pvfs->nbReturnPath = 1;
		}
	}

    return pvfs->nbReturnPath;
}


static VstIntPtr qtractorVstPlugin_closeFileSelector ( AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVstPlugin *pVstPlugin = qtractorVstPlugin::findPlugin(effect);
	if (pVstPlugin == NULL)
		return 0;

	qDebug("qtractorVstPlugin_closeFileSelector(%p, %p) command=%d reserved=%d",
		effect, pvfs, int(pvfs->command), int(pvfs->reserved));

    if (pvfs->reserved == 1 && pvfs->returnPath) {
        delete [] pvfs->returnPath;
        pvfs->returnPath = NULL;
        pvfs->reserved   = 0;
    }

	return 1;
}

//----------------------------------------------------------------------
// The magnificient host callback, which every VSTi plugin will call.

#ifdef CONFIG_DEBUG
#define VST_HC_FMT "qtractorVstPlugin_HostCallback(%p, %s(%d), %d, %d, %p, %g)"
#define VST_HC_DEBUG(s)  qDebug(VST_HC_FMT, \
	effect, (s), int(opcode), int(index), int(value), ptr, opt)
#define VST_HC_DEBUGX(s) qDebug(VST_HC_FMT " [%s]", \
	effect, (s), int(opcode), int(index), int(value), ptr, opt, (char *) ptr)
#else
#define VST_HC_DEBUG(s)
#define VST_HC_DEBUGX(s)
#endif

static VstIntPtr VSTCALLBACK qtractorVstPlugin_HostCallback ( AEffect* effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt )
{
	VstIntPtr ret = 0;
	qtractorVstPlugin *pVstPlugin = NULL;
	static VstTimeInfo s_vstTimeInfo;
	qtractorSession  *pSession  = NULL;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pSession = pMainForm->session();

	switch (opcode) {

	// VST 1.0 opcodes...
	case audioMasterVersion:
		VST_HC_DEBUG("audioMasterVersion");
		ret = 2;
		break;

	case audioMasterAutomate:
		VST_HC_DEBUG("audioMasterAutomate");
	//	effect->setParameter(effect, index, opt);
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			qtractorPluginForm *pForm = pVstPlugin->form();
			if (pForm)
				pForm->updateParam(index);
			QApplication::processEvents();
		}
		break;

	case audioMasterCurrentId:
		VST_HC_DEBUG("audioMasterCurrentId");
		break;

	case audioMasterIdle:
		VST_HC_DEBUG("audioMasterIdle");
		qtractorVstPlugin::idleEditorAll();
		QApplication::processEvents();
		break;

	case audioMasterPinConnected:
		VST_HC_DEBUG("audioMasterPinConnected");
		break;

	// VST 2.0 opcodes...
	case audioMasterWantMidi:
		VST_HC_DEBUG("audioMasterWantMidi");
		break;

	case audioMasterGetTime:
		VST_HC_DEBUG("audioMasterGetTime");
		if (pSession) {
			::memset(&s_vstTimeInfo, 0, sizeof(s_vstTimeInfo));
			s_vstTimeInfo.samplePos  = pSession->playHead();
			s_vstTimeInfo.sampleRate = pSession->sampleRate();
			ret = (long) &s_vstTimeInfo;
		}
		break;

	case audioMasterProcessEvents:
		VST_HC_DEBUG("audioMasterProcessEvents");
		break;

	case audioMasterSetTime:
		VST_HC_DEBUG("audioMasterSetTime");
		break;

	case audioMasterTempoAt:
		VST_HC_DEBUG("audioMasterTempoAt");
		if (pSession)
			ret = (long) (pSession->tempo() * 10000.0f);
		break;

	case audioMasterGetNumAutomatableParameters:
		VST_HC_DEBUG("audioMasterGetNumAutomatableParameters");
		break;

	case audioMasterGetParameterQuantization:
		VST_HC_DEBUG("audioMasterGetParameterQuantization");
		ret = 1; // full single float precision
		break;

	case audioMasterIOChanged:
		VST_HC_DEBUG("audioMasterIOChanged");
		break;

	case audioMasterNeedIdle:
		VST_HC_DEBUG("audioMasterNeedIdle");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin && !pVstPlugin->isIdleTimer()) {
			effect->dispatcher(effect, effIdle, 0, 0, NULL, 0.0f);
			pVstPlugin->setIdleTimer(true);
		}
		break;

	case audioMasterSizeWindow:
		VST_HC_DEBUG("audioMasterSizeWindow");
		break;

	case audioMasterGetSampleRate:
		VST_HC_DEBUG("audioMasterGetSampleRate");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			effect->dispatcher(effect,
				effSetSampleRate, 0, 0, NULL, float(pVstPlugin->sampleRate()));
		}
		break;

	case audioMasterGetBlockSize:
		VST_HC_DEBUG("audioMasterGetBlockSize");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			effect->dispatcher(effect,
				effSetBlockSize, 0, pVstPlugin->bufferSize(), NULL, 0.0f);
		}
		break;

	case audioMasterGetInputLatency:
		VST_HC_DEBUG("audioMasterGetInputLatency");
		break;

	case audioMasterGetOutputLatency:
		VST_HC_DEBUG("audioMasterGetOutputLatency");
		break;

	case audioMasterGetPreviousPlug:
		VST_HC_DEBUG("audioMasterGetPreviousPlug");
		break;

	case audioMasterGetNextPlug:
		VST_HC_DEBUG("audioMasterGetNextPlug");
		break;

	case audioMasterWillReplaceOrAccumulate:
		VST_HC_DEBUG("audioMasterWillReplaceOrAccumulate");
		ret = 1;
		break;

	case audioMasterGetCurrentProcessLevel:
		VST_HC_DEBUG("audioMasterGetCurrentProcessLevel");
		break;

	case audioMasterGetAutomationState:
		VST_HC_DEBUG("audioMasterGetAutomationState");
		ret = 1; // off
		break;

	case audioMasterSetOutputSampleRate:
		VST_HC_DEBUG("audioMasterSetOutputSampleRate");
		break;

#if !defined(VST_2_3_EXTENSIONS) 
	case audioMasterGetSpeakerArrangement:
		VST_HC_DEBUG("audioMasterGetSpeakerArrangement");
		break;
#endif

	case audioMasterGetVendorString:
		VST_HC_DEBUG("audioMasterGetVendorString");
		::strcpy((char *) ptr, QTRACTOR_DOMAIN);
		break;

	case audioMasterGetProductString:
		VST_HC_DEBUG("audioMasterGetProductString");
		::strcpy((char *) ptr, QTRACTOR_TITLE);
		break;

	case audioMasterGetVendorVersion:
		VST_HC_DEBUG("audioMasterGetVendorVersion");
		break;

	case audioMasterVendorSpecific:
		VST_HC_DEBUG("audioMasterVendorSpecific");
		break;

	case audioMasterSetIcon:
		VST_HC_DEBUG("audioMasterSetIcon");
		break;

	case audioMasterCanDo:
		VST_HC_DEBUGX("audioMasterCanDo");
		if (::strcmp("sendVstMidiEvent",    (char *) ptr) == 0 ||
			::strcmp("receiveVstMidiEvent", (char *) ptr) == 0 ||
			::strcmp("midiProgramNames",    (char *) ptr) == 0 ||
			::strcmp("openFileSelector",    (char *) ptr) == 0 ||
			::strcmp("closeFileSelector",   (char *) ptr) == 0) {
			ret = 1;
		}
		break;

	case audioMasterGetLanguage:
		VST_HC_DEBUG("audioMasterGetLanguage");
		ret = kVstLangEnglish;
		break;

	case audioMasterOpenWindow:
		VST_HC_DEBUG("audioMasterOpenWindow");
		break;

	case audioMasterCloseWindow:
		VST_HC_DEBUG("audioMasterCloseWindow");
		break;

	case audioMasterGetDirectory:
		VST_HC_DEBUG("audioMasterGetDirectory");
		break;

	case audioMasterUpdateDisplay:
		VST_HC_DEBUG("audioMasterUpdateDisplay");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			qtractorPluginForm *pPluginForm = pVstPlugin->form();
			if (pPluginForm)
				pPluginForm->refresh();
			QApplication::processEvents();
		}
		break;

	case audioMasterBeginEdit:
		VST_HC_DEBUG("audioMasterBeginEdit");
		break;

	case audioMasterEndEdit:
		VST_HC_DEBUG("audioMasterEndEdit");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			qtractorPluginForm *pForm = pVstPlugin->form();
			if (pForm)
				pForm->updateParam(index);
			QApplication::processEvents();
		}
		break;

	case audioMasterOpenFileSelector:
		VST_HC_DEBUG("audioMasterOpenFileSelector");
		ret = qtractorVstPlugin_openFileSelector(effect, (VstFileSelect *) ptr);
		break;

	case audioMasterCloseFileSelector:
		VST_HC_DEBUG("audioMasterCloseFileSelector");
		ret = qtractorVstPlugin_closeFileSelector(effect, (VstFileSelect *) ptr);
		break;

	default:
		VST_HC_DEBUG("audioMasterUnknown");
		break;
	}

	return ret;
}


#endif	// CONFIG_VST

// end of qtractorVstPlugin.cpp
