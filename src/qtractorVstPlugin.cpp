// qtractorVstPlugin.cpp
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

#include "qtractorAbout.h"

#ifdef CONFIG_VST

#include "qtractorVstPlugin.h"

#include "qtractorPluginForm.h"

#include "qtractorMidiBuffer.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QWidget>

#if QT_VERSION < 0x040500
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif


#if defined(Q_WS_X11)
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
typedef void (*XEventProc)(XEvent *);
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


// Some VeSTige missing opcodes and flags.
#ifdef CONFIG_VESTIGE
const int effGetParamLabel = 6;
const int effGetParamDisplay = 7;
const int effGetChunk = 23;
const int effSetChunk = 24;
const int effFlagsProgramChunks = 32;
#endif


//---------------------------------------------------------------------
// qtractorVstPlugin::EditorWidget - Helpers for own editor widget.

#if defined(Q_WS_X11)

static bool g_bXError = false;

static int tempXErrorHandler ( Display *, XErrorEvent * )
{
	g_bXError = true;
	return 0;
}

static XEventProc getXEventProc ( Display *pDisplay, Window w )
{
	int iSize;
	unsigned long iBytes, iCount;
	unsigned char *pData;
	XEventProc eventProc = NULL;
	Atom aType, aName = XInternAtom(pDisplay, "_XEventProc", false);

	g_bXError = false;
	XErrorHandler oldErrorHandler = XSetErrorHandler(tempXErrorHandler);
	XGetWindowProperty(pDisplay, w, aName, 0, 1, false,
		AnyPropertyType, &aType,  &iSize, &iCount, &iBytes, &pData);
	if (g_bXError == false && iCount == 1)
		eventProc = (XEventProc) (pData);
	XSetErrorHandler(oldErrorHandler);

	return eventProc;
}

static Window getXChildWindow ( Display *pDisplay, Window w )
{
	Window wRoot, wParent, *pwChildren;
	unsigned int iChildren = 0;

	XQueryTree(pDisplay, w, &wRoot, &wParent, &pwChildren, &iChildren);

	return (iChildren > 0 ? pwChildren[0] : 0);
}

#endif // Q_WS_X11


//----------------------------------------------------------------------------
// qtractorVstPlugin::EditorWidget -- Plugin editor wrapper widget.
//

// Dynamic singleton list of VST editors.
static QList<qtractorVstPlugin::EditorWidget *> g_vstEditors;

class qtractorVstPlugin::EditorWidget : public QWidget
{
public:

	// Constructor.
	EditorWidget(QWidget *pParent, Qt::WindowFlags wflags = 0)
		: QWidget(pParent, wflags),
	#if defined(Q_WS_X11)
		m_pDisplay(QX11Info::display()),
		m_wVstEditor(0),
		m_pVstEventProc(NULL),
		m_bButtonPress(false),
	#endif
		m_pVstPlugin(NULL) {}

	// Destructor.
	~EditorWidget() { close(); }

	// Specialized editor methods.
	void open(qtractorVstPlugin *pVstPlugin)
	{
		m_pVstPlugin = pVstPlugin;
		
		// Start the proper (child) editor...
		long  value = 0;
		void *ptr = (void *) winId();
	#if defined(Q_WS_X11)
		value = (long) m_pDisplay;
	#endif

		// Make it the right size
		struct ERect {
			short top;
			short left;
			short bottom;
			short right;
		} *pRect;

		if (m_pVstPlugin->vst_dispatch(0, effEditGetRect, 0, 0, &pRect, 0.0f)) {
			int w = pRect->right - pRect->left;
			int h = pRect->bottom - pRect->top;
			if (w > 0 && h > 0)
				QWidget::setFixedSize(w, h);
		}

		m_pVstPlugin->vst_dispatch(0, effEditOpen, 0, value, ptr, 0.0f);
		
	#if defined(Q_WS_X11)
		m_wVstEditor = getXChildWindow(m_pDisplay, (Window) winId());
		if (m_wVstEditor)
			m_pVstEventProc = getXEventProc(m_pDisplay, m_wVstEditor);
	#endif

		g_vstEditors.append(this);

		// Final stabilization...
		m_pVstPlugin->updateEditorTitle();
		m_pVstPlugin->setEditorVisible(true);
		m_pVstPlugin->idleEditor();
	}

	// Close the editor widget.
	void close()
	{
		QWidget::close();

		if (m_pVstPlugin) {
			m_pVstPlugin->vst_dispatch(0, effEditClose, 0, 0, NULL, 0.0f);
		//	m_pVstPlugin->setEditorVisible(false);
			m_pVstPlugin = NULL;
		}

		const int iIndex = g_vstEditors.indexOf(this);
		if (iIndex >= 0)
			g_vstEditors.removeAt(iIndex);
	}

#if defined(Q_WS_X11)

	// Local X11 event filter.
	bool x11EventFilter(XEvent *pEvent)
	{
		if (m_pVstEventProc && pEvent->xany.window == m_wVstEditor) {
			// Avoid mouse tracking events...
			switch (pEvent->xany.type) {
			case ButtonPress:
				m_bButtonPress = true;
				break;
			case ButtonRelease:
				m_bButtonPress = false;
				break;
			case MotionNotify:
				if (!m_bButtonPress)
					return false;
				// Fall thru...
			default:
				break;
			}
			// Process as intended...
			(*m_pVstEventProc)(pEvent);
			return true;
		} else {
			return false;
		}
	}

#endif

	qtractorVstPlugin *plugin() const
		{ return m_pVstPlugin; }

protected:

	// Visibility event handlers.
	void showEvent(QShowEvent *pShowEvent)
	{
		QWidget::showEvent(pShowEvent);

		if (m_pVstPlugin && m_pVstPlugin->isFormVisible())
			(m_pVstPlugin->form())->toggleEditor(true);
	}

	void closeEvent(QCloseEvent *pCloseEvent)
	{
		if (m_pVstPlugin && m_pVstPlugin->isFormVisible())
			(m_pVstPlugin->form())->toggleEditor(false);

		QWidget::closeEvent(pCloseEvent);

		if (m_pVstPlugin)
			m_pVstPlugin->closeEditor();
	}

	void moveEvent(QMoveEvent *pMoveEvent)
	{
		QWidget::moveEvent(pMoveEvent);
	#if defined(Q_WS_X11)
		if (m_wVstEditor) {
			XMoveWindow(m_pDisplay, m_wVstEditor, 0, 0);
		//	QWidget::update();
		}
	#endif
	}

private:

	// Instance variables...
#if defined(Q_WS_X11)
	Display   *m_pDisplay;
	Window     m_wVstEditor;
	XEventProc m_pVstEventProc;
	bool       m_bButtonPress;
#endif

	qtractorVstPlugin *m_pVstPlugin;
};


//----------------------------------------------------------------------------
// qtractorVstPluginType::Effect -- VST AEffect instance.
//

class qtractorVstPluginType::Effect
{
public:

	// Constructor.
	Effect(AEffect *pVstEffect = NULL) : m_pVstEffect(pVstEffect) {}

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
	if (m_pVstEffect == NULL)
		m_pVstEffect = qtractorVstPluginType::vst_effect(pFile);
	if (m_pVstEffect == NULL)
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::open(%p)", m_pVstEffect, pFile);
#endif

//	vst_dispatch(effIdentify, 0, 0, NULL, 0);
	vst_dispatch(effOpen, 0, 0, NULL, 0.0f);
//	vst_dispatch(effMainsChanged, 0, 0, NULL, 0.0f);

	return true;
}


void qtractorVstPluginType::Effect::close (void)
{
	if (m_pVstEffect == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::close()", m_pVstEffect);
#endif

//	vst_dispatch(effMainsChanged, 0, 0, NULL, 0.0f);
	vst_dispatch(effClose, 0, 0, NULL, 0.0f);

	m_pVstEffect = NULL;
}


// VST host dispatcher.
int qtractorVstPluginType::Effect::vst_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pVstEffect == NULL)
		return 0;

#ifdef CONFIG_DEBUG_0
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
	if (m_pEffect == NULL)
		m_pEffect = new Effect();
	if (!m_pEffect->open(file()))
		return false;

	AEffect *pVstEffect = m_pEffect->vst_effect();
	if (pVstEffect == NULL)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPluginType[%p]::open() filename=\"%s\" index=%lu",
		this, filename().toUtf8().constData(), index());
#endif

	// Retrieve plugin type names.
	char szName[32]; szName[0] = (char) 0;
	vst_dispatch(effGetEffectName, 0, 0, (void *) szName, 0.0f);
	if (szName[0])
		m_sName = szName;
	else
		m_sName = QFileInfo(filename()).baseName();
	// Sanitize plugin label.
	m_sLabel = m_sName.simplified().replace(QRegExp("[\\s|\\.|\\-]+"), "_");

	// Retrieve plugin unique identifier.
#ifdef CONFIG_VESTIGE_OLD
	m_iUniqueID = qHash(m_sLabel);
#else
	m_iUniqueID = pVstEffect->uniqueID;
#endif

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
//	if (vst_canDo("midiProgramNames"))    m_iFlagsEx |= effFlagsExCanMidiProgramNames;

	// Compute and cache port counts...
	m_iControlIns  = pVstEffect->numParams;
	m_iControlOuts = 0;
	m_iAudioIns    = pVstEffect->numInputs;
	m_iAudioOuts   = pVstEffect->numOutputs;
	m_iMidiIns     = ((m_iFlagsEx & effFlagsExCanReceiveVstMidiEvents)
		|| (pVstEffect->flags & effFlagsIsSynth) ? 1 : 0);
	m_iMidiOuts    = ((m_iFlagsEx & effFlagsExCanSendVstMidiEvents) ? 1 : 0);

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = (pVstEffect->flags & effFlagsProgramChunks);
	m_bEditor    = (pVstEffect->flags & effFlagsHasEditor);

	// HACK: JUCE based VST plugins, which are the most found
	// with a GUI editor, need to skip explicit shared library
	// unloading, to avoid mysterious crashes later...
	if (m_bEditor) file()->setAutoUnload(false);

	return true;
}


void qtractorVstPluginType::close (void)
{
	if (m_pEffect == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPluginType[%p]::close()", this);
#endif

	m_pEffect->close();

	delete m_pEffect;
	m_pEffect = NULL;
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

	// Does the VST plugin instantiate OK?
	AEffect *pVstEffect
		= (*pfnGetPluginInstance)(qtractorVstPlugin_HostCallback);
	if (pVstEffect == NULL)
		return NULL;
	if (pVstEffect->magic != kEffectMagic)
		return NULL;

	return pVstEffect;
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


// Instance cached-deferred accesors.
const QString& qtractorVstPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		char szTemp[64];
		szTemp[0] = (char) 0;
		vst_dispatch(effGetProductString, 0, 0, (void *) szTemp, 0.0f);
		if (szTemp[0]) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Product: ");
			m_sAboutText += szTemp;
		}
		szTemp[0] = (char) 0;
		vst_dispatch(effGetVendorString, 0, 0, (void *) szTemp, 0.0f);
		if (szTemp[0]) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Vendor: ");
			m_sAboutText += szTemp;
		}
		const int iVersion = vst_dispatch(effGetVendorVersion, 0, 0, NULL, 0.0f);
		if (iVersion) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Version: ");
			m_sAboutText += QString::number(iVersion);
		}
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorVstPlugin -- VST plugin instance.
//

// Dynamic singleton list of VST plugins.
static QHash<AEffect *, qtractorVstPlugin *> g_vstPlugins;

// Constructors.
qtractorVstPlugin::qtractorVstPlugin ( qtractorPluginList *pList,
	qtractorVstPluginType *pVstType )
	: qtractorPlugin(pList, pVstType), m_ppEffects(NULL),
		m_ppIBuffer(NULL), m_ppOBuffer(NULL), m_pEditorWidget(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p] filename=\"%s\" index=%lu typeHint=%d",
		this, type()->filename().toUtf8().constData(),
		type()->index(), int(type()->typeHint()));
#endif

	// Allocate I/O audio buffer pointers.
	unsigned short iAudioIns  = pVstType->audioIns();
	unsigned short iAudioOuts = pVstType->audioOuts();
	if (iAudioIns > 0)
		m_ppIBuffer = new float * [iAudioIns];
	if (iAudioOuts > 0)
		m_ppOBuffer = new float * [iAudioOuts];

	// Create all existing parameters...
	unsigned short iParamCount = pVstType->controlIns();
	for (unsigned short i = 0; i < iParamCount; ++i)
		addParam(new qtractorVstPluginParam(this, i));

	// Instantiate each instance properly...
	setChannels(channels());
}


// Destructor.
qtractorVstPlugin::~qtractorVstPlugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);

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
	unsigned short iOldInstances = instances();
	unsigned short iInstances
		= pVstType->instances(iChannels, list()->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == iOldInstances)
		return;

	// Gotta go for a while...
	bool bActivated = isActivated();
	setActivated(false);

	// Set new instance number...
	setInstances(iInstances);

	// Close old instances, all the way...
	if (m_ppEffects) {
		for (unsigned short i = 1; i < iOldInstances; ++i) {
			g_vstPlugins.remove(m_ppEffects[i]->vst_effect());
			m_ppEffects[i]->close();
			delete m_ppEffects[i];
		}
		g_vstPlugins.remove(m_ppEffects[0]->vst_effect());
		delete [] m_ppEffects;
		m_ppEffects = NULL;
	}

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	// Allocate new instances...
	m_ppEffects = new qtractorVstPluginType::Effect * [iInstances];
	m_ppEffects[0] = pVstType->effect();
	g_vstPlugins.insert(m_ppEffects[0]->vst_effect(), this);
	for (unsigned short i = 1; i < iInstances; ++i) {
		m_ppEffects[i] = new qtractorVstPluginType::Effect();
		m_ppEffects[i]->open(pVstType->file());
		g_vstPlugins.insert(m_ppEffects[i]->vst_effect(), this);
	}

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// Setup all those instance alright...
	for (unsigned short i = 0; i < iInstances; ++i) {
		// And now all other things as well...
		qtractorVstPluginType::Effect *pEffect = m_ppEffects[i];
		pEffect->vst_dispatch(effSetSampleRate, 0, 0, NULL, float(sampleRate()));
		pEffect->vst_dispatch(effSetBlockSize,  0, bufferSize(), NULL, 0.0f);
	//	pEffect->vst_dispatch(effSetProgram, 0, 0, NULL, 0.0f);
	#if 0 // !VST_FORCE_DEPRECATED
		unsigned short j;
		for (j = 0; j < pVstType->audioIns(); ++j)
			pEffect->vst_dispatch(effConnectInput, j, 1, NULL, 0.0f);
		for (j = 0; j < pVstType->audioOuts(); ++j)
			pEffect->vst_dispatch(effConnectOutput, j, 1, NULL, 0.0f);
	#endif
	}

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Do the actual activation.
void qtractorVstPlugin::activate (void)
{
	for (unsigned short i = 0; i < instances(); ++i) {
		vst_dispatch(i, effMainsChanged, 0, 1, NULL, 0.0f);
	#ifndef CONFIG_VESTIGE
		vst_dispatch(i, effStartProcess, 0, 0, NULL, 0.0f);
	#endif
	}
}


// Do the actual deactivation.
void qtractorVstPlugin::deactivate (void)
{
	for (unsigned short i = 0; i < instances(); ++i) {
	#ifndef CONFIG_VESTIGE
		vst_dispatch(i, effStopProcess, 0, 0, NULL, 0.0f);
	#endif
		vst_dispatch(i, effMainsChanged, 0, 0, NULL, 0.0f);
	}
}


// The main plugin processing procedure.
void qtractorVstPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_ppEffects == NULL)
		return;

	// To process MIDI events, if any...
	qtractorMidiManager *pMidiManager = NULL;
	const unsigned short iMidiIns  = type()->midiIns();
	const unsigned short iMidiOuts = type()->midiOuts();
	if (iMidiIns > 0)
		pMidiManager = list()->midiManager();

	// We'll cross channels over instances...
	const unsigned short iInstances = instances();
	const unsigned short iChannels  = channels();
	const unsigned short iAudioIns  = audioIns();
	const unsigned short iAudioOuts = audioOuts();

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
		// Make it run MIDI, if applicable...
		if (pMidiManager) {
			pVstEffect->dispatcher(pVstEffect,
				effProcessEvents, 0, 0, pMidiManager->vst_events_in(), 0.0f);
		}
		// Make it run audio...
		if (pVstEffect->flags & effFlagsCanReplacing) {
			pVstEffect->processReplacing(
				pVstEffect, m_ppIBuffer, m_ppOBuffer, nframes);
		}
	#if 0 // !VST_FORCE_DEPRECATED
		else {
			pVstEffect->process(
				pVstEffect, m_ppIBuffer, m_ppOBuffer, nframes);
		}
	#endif
		// Wrap channels?...
		if (iIChannel < iChannels - 1)
			++iIChannel;
		if (iOChannel < iChannels - 1)
			++iOChannel;
	}

	if (pMidiManager && iMidiOuts > 0)
		pMidiManager->vst_events_swap();
}


// Parameter update method.
void qtractorVstPlugin::updateParam (
	qtractorPluginParam *pParam, float fValue, bool /*bUpdate*/ )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVstPlugin[%p]::updateParam(%lu, %g, %d)",
		this, pParam->index(), fValue, int(bUpdate));
#endif

	// Maybe we're not pretty instantiated yet...
	for (unsigned short i = 0; i < instances(); ++i) {
		AEffect *pVstEffect = vst_effect(i);
		if (pVstEffect)
			pVstEffect->setParameter(pVstEffect, pParam->index(), fValue);
	}
}


// Bank/program selector.
void qtractorVstPlugin::selectProgram ( int iBank, int iProg )
{
	if (iBank < 0 || iProg < 0)
		return;

	// HACK: We don't change program-preset when
	// we're supposed to be multi-timbral...
	if ((list()->flags() & qtractorPluginList::MidiBus)
		== qtractorPluginList::MidiBus)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	int iIndex = 0;
	if (iBank >= 0 && iProg >= 0)
		iIndex = (iBank << 7) + iProg;

	for (unsigned short i = 0; i < instances(); ++i)
		vst_dispatch(i, effSetProgram, 0, iIndex, NULL, 0.0f);

	// Reset parameters default value...
	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect) {
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			float *pfValue = pParam->subject()->data();
			*pfValue = pVstEffect->getParameter(pVstEffect, pParam->index());
			pParam->setDefaultValue(*pfValue);
		}
	}
}


// Provisional program/patch accessor.
bool qtractorVstPlugin::getProgram ( int iIndex, Program& program ) const
{
	// Iteration sanity check...
	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect == NULL)
		return false;
	if (iIndex < 0 || iIndex >= pVstEffect->numPrograms)
		return false;

	char szName[256]; ::memset(szName, 0, sizeof(szName));
#ifndef CONFIG_VESTIGE
	if (vst_dispatch(0, effGetProgramNameIndexed, iIndex, 0, (void *) szName, 0.0f) == 0) {
#endif
		int iOldIndex = vst_dispatch(0, effGetProgram, 0, 0, NULL, 0.0f);
		vst_dispatch(0, effSetProgram, 0, iIndex, NULL, 0.0f);
		vst_dispatch(0, effGetProgramName, 0, 0, (void *) szName, 0.0f);
		vst_dispatch(0, effSetProgram, 0, iOldIndex, NULL, 0.0f);
#ifndef CONFIG_VESTIGE
	}
#endif

	// Map this to that...
	program.bank = 0;
	program.prog = iIndex;
	program.name = szName;

	return true;
}


// Configuration (CLOB) stuff.
void qtractorVstPlugin::configure ( const QString& sKey, const QString& sValue )
{
	if (sKey == "chunk") {
		// Load the BLOB (base64 encoded)...
		QByteArray data = qUncompress(QByteArray::fromBase64(sValue.toLatin1()));
		const char *pData = data.constData();
		int iData = data.size();
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPlugin[%p]::configure() chunk.size=%d checksum=0x%04x",
			this, iData, qChecksum(pData, iData));
	#endif
		for (unsigned short i = 0; i < instances(); ++i)
			vst_dispatch(i, effSetChunk, 0, iData, (void *) pData, 0.0f);
	}
}


// Plugin configuration/state snapshot.
void qtractorVstPlugin::freezeConfigs (void)
{
	// HACK: Make sure all parameter values are in sync,
	// provided freezeConfigs() are always called when
	// saving plugin's state and before parameter values.
	updateParamValues(false);

	if (!type()->isConfigure())
		return;

	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect == NULL)
		return;

	// Save plugin state into chunk configuration...
	char *pData = NULL;
	int iData = vst_dispatch(0, effGetChunk, 0, 0, (void *) &pData, 0.0f);
	if (iData < 1 || pData == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::freezeConfigs() chunk.size=%d checksum=0x%04x",
		this, iData, qChecksum(pData, iData));
#endif

	// Set special plugin configuration item (base64 encoded)...
	QByteArray data = qCompress(QByteArray(pData, iData)).toBase64();
	for (int i = data.size() - (data.size() % 72); i >= 0; i -= 72)
		data.insert(i, "\n       "); // Indentation.
	setConfig("chunk", data.constData());
}

void qtractorVstPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::releaseConfigs()", this);
#endif

	clearConfigs();
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
void qtractorVstPlugin::openEditor ( QWidget */*pParent*/ )
{
	// Is it already there?
	if (m_pEditorWidget) {
		if (!m_pEditorWidget->isVisible())
			m_pEditorWidget->show();
		m_pEditorWidget->raise();
		m_pEditorWidget->activateWindow();
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::openEditor()", this);
#endif

	// Create the new parent frame...
	Qt::WindowFlags wflags = Qt::Window
		| Qt::CustomizeWindowHint
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowMinMaxButtonsHint
		| Qt::WindowCloseButtonHint;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bKeepToolsOnTop)
		wflags |= Qt::Tool;

	m_pEditorWidget = new EditorWidget(NULL, wflags);
	m_pEditorWidget->open(this);
}


// Close editor.
void qtractorVstPlugin::closeEditor (void)
{
	if (m_pEditorWidget == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::closeEditor()", this);
#endif

	// Close the parent widget, if any.
	delete m_pEditorWidget;
	m_pEditorWidget = NULL;
}


// Idle editor.
void qtractorVstPlugin::idleEditor (void)
{
	if (m_pEditorWidget == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVstPlugin[%p]::idleEditor()", this);
#endif

	vst_dispatch(0, effEditIdle, 0, 0, NULL, 0.0f);

	m_pEditorWidget->update();
}


// GUI editor visibility state.
void qtractorVstPlugin::setEditorVisible ( bool bVisible )
{
	if (m_pEditorWidget) m_pEditorWidget->setVisible(bVisible);
}

bool qtractorVstPlugin::isEditorVisible (void) const
{
	return (m_pEditorWidget ? m_pEditorWidget->isVisible() : false);
}


// Update editor widget caption.
void qtractorVstPlugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	if (m_pEditorWidget)
		m_pEditorWidget->setWindowTitle(sTitle);
}


// Idle editor (static).
void qtractorVstPlugin::idleEditorAll (void)
{
	QListIterator<EditorWidget *> iter(g_vstEditors);
	while (iter.hasNext()) {
		qtractorVstPlugin *pVstPlugin = iter.next()->plugin();
		if (pVstPlugin)
			pVstPlugin->idleEditor();
	}
}


// Our own editor widget accessor.
QWidget *qtractorVstPlugin::editorWidget (void) const
{
	return static_cast<QWidget *> (m_pEditorWidget);
}


// Our own editor widget size accessor.
void qtractorVstPlugin::resizeEditor ( int w, int h )
{
	if (m_pEditorWidget && w > 0 && h > 0)
		m_pEditorWidget->resize(w, h);
}


// Global VST plugin lookup.
qtractorVstPlugin *qtractorVstPlugin::findPlugin ( AEffect *pVstEffect )
{
	return g_vstPlugins.value(pVstEffect, NULL);
}


#if defined(Q_WS_X11)

// Global X11 event filter.
bool qtractorVstPlugin::x11EventFilter ( void *pvEvent )
{
	XEvent *pEvent = (XEvent *) pvEvent;

	QListIterator<EditorWidget *> iter(g_vstEditors);
	while (iter.hasNext()) {
		if (iter.next()->x11EventFilter(pEvent))
			return true;
	}

	return false;
}

#endif


// Parameter update method.
void qtractorVstPlugin::updateParamValues ( bool bUpdate )
{
	// Make sure all cached parameter values are in sync
	// with plugin parameter values; update cache otherwise.
	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect) {
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			float fValue = pVstEffect->getParameter(pVstEffect, pParam->index());
			if (pParam->value() != fValue) {
				pParam->setValue(fValue, bUpdate);
				if (isFormVisible())
					form()->changeParamValue(pParam->index());
			}
		}
	}
}


//----------------------------------------------------------------------------
// qtractorVstPluginParam -- VST plugin control input port instance.
//

// Constructors.
qtractorVstPluginParam::qtractorVstPluginParam (
	qtractorVstPlugin *pVstPlugin, unsigned long iIndex )
	: qtractorPluginParam(pVstPlugin, iIndex)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVstPluginParam[%p] pVstPlugin=%p iIndex=%lu", this, pVstPlugin, iIndex);
#endif

	qtractorVstPluginType *pVstType
		= static_cast<qtractorVstPluginType *> (pVstPlugin->type());
	
	char szName[64]; szName[0] = (char) 0;
	if (pVstType)
		pVstType->vst_dispatch(effGetParamName, iIndex, 0, (void *) szName, 0.0f);
	if (!szName[0])
		::snprintf(szName, sizeof(szName), "Param #%lu", iIndex + 1); // Default dummy name.
	setName(szName);

	setMinValue(0.0f);
	setMaxValue(1.0f);

	::memset(&m_props, 0, sizeof(m_props));

	if (pVstType &&	pVstType->vst_dispatch(
			effGetParameterProperties, iIndex, 0, (void *) &m_props, 0.0f)) {
	#ifdef CONFIG_DEBUG_0
		qDebug("  VstParamProperties(%lu) {", iIndex);
		qDebug("    .label                   = \"%s\"", m_props.label);
		qDebug("    .shortLabel              = \"%s\"", m_props.shortLabel);
		qDebug("    .category                = %d", m_props.category);
		qDebug("    .categoryLabel           = \"%s\"", m_props.categoryLabel);
		qDebug("    .minInteger              = %d", int(m_props.minInteger));
		qDebug("    .maxInteger              = %d", int(m_props.maxInteger));
		qDebug("    .stepInteger             = %d", int(m_props.stepInteger));
		qDebug("    .stepFloat               = %g", m_props.stepFloat);
	#ifndef CONFIG_VESTIGE_OLD
		qDebug("    .smallStepFloat          = %g", m_props.smallStepFloat);
		qDebug("    .largeStepFloat          = %g", m_props.largeStepFloat);
		qDebug("    .largeStepInteger        = %d", int(m_props.largeStepInteger));
		qDebug("    >IsSwitch                = %d", (m_props.flags & kVstParameterIsSwitch ? 1 : 0));
		qDebug("    >UsesIntegerMinMax       = %d", (m_props.flags & kVstParameterUsesIntegerMinMax ? 1 : 0));
		qDebug("    >UsesFloatStep           = %d", (m_props.flags & kVstParameterUsesFloatStep ? 1 : 0));
		qDebug("    >UsesIntStep             = %d", (m_props.flags & kVstParameterUsesIntStep ? 1 : 0));
		qDebug("    >SupportsDisplayIndex    = %d", (m_props.flags & kVstParameterSupportsDisplayIndex ? 1 : 0));
		qDebug("    >SupportsDisplayCategory = %d", (m_props.flags & kVstParameterSupportsDisplayCategory ? 1 : 0));
		qDebug("    >CanRamp                 = %d", (m_props.flags & kVstParameterCanRamp ? 1 : 0));
		qDebug("    .displayIndex            = %d", m_props.displayIndex);
		qDebug("    .numParametersInCategory = %d", m_props.numParametersInCategory);
	#endif
		qDebug("}");
	#endif
		if (isBoundedBelow())
			setMinValue(float(m_props.minInteger));
		if (isBoundedAbove())
			setMaxValue(float(m_props.maxInteger));
	}

	// ATTN: Set default value as initial one...
	if (pVstType && pVstType->effect()) {
		AEffect *pVstEffect = (pVstType->effect())->vst_effect();
		if (pVstEffect)
			qtractorPluginParam::setValue(
				pVstEffect->getParameter(pVstEffect, iIndex), true);
	}

	setDefaultValue(qtractorPluginParam::value());

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
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
#endif
}

bool qtractorVstPluginParam::isBoundedAbove (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
#endif
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
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntStep);
#endif
}

bool qtractorVstPluginParam::isToggled (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterIsSwitch);
#endif
}

bool qtractorVstPluginParam::isDisplay (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return true;
#endif
}


// Current display value.
QString qtractorVstPluginParam::display (void) const
{
	qtractorVstPluginType *pVstType = NULL;
	if (plugin())
		pVstType = static_cast<qtractorVstPluginType *> (plugin()->type());
	if (pVstType) {
		QString sDisplay;
		char szText[64];
		// Grab parameter display value...
		szText[0] = '\0';
		pVstType->vst_dispatch(effGetParamDisplay,
			index(), 0, (void *) szText, 0.0f);
		if (szText[0]) {
			// Got
			sDisplay += QString(szText).trimmed();
			// Grab parameter name...
			szText[0] = '\0';
			pVstType->vst_dispatch(effGetParamLabel,
				index(), 0, (void *) szText, 0.0f);
			if (szText[0])
				sDisplay += ' ' + QString(szText).trimmed();
			// Got it.
			return sDisplay;
		}
	}

	// Default parameter display value...
	return qtractorPluginParam::display();
}


//----------------------------------------------------------------------
// VST host callback file selection helpers.

#ifndef CONFIG_VESTIGE

static VstIntPtr qtractorVstPlugin_openFileSelector (
	AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVstPlugin *pVstPlugin = qtractorVstPlugin::findPlugin(effect);
	if (pVstPlugin == NULL)
		return 0;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin_openFileSelector(%p, %p) command=%d reserved=%d",
		effect, pvfs, int(pvfs->command), int(pvfs->reserved));
#endif

	pvfs->reserved = 0;
	pvfs->nbReturnPath = 0;

    if (pvfs->command == kVstFileLoad || pvfs->command == kVstFileSave) {
		QString sFilename;
		QStringList filters;
		for (int i = 0; i < pvfs->nbFileTypes; ++i) {
			filters.append(QObject::tr("%1 (*.%2)")
				.arg(pvfs->fileTypes[i].name).arg(pvfs->fileTypes[i].dosType));
		}
        filters.append(QObject::tr("All files (*.*)"));
		QWidget *pParentWidget = pVstPlugin->editorWidget();
		const QString& sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg((pVstPlugin->type())->name());
		const QString& sDirectory = pvfs->initialPath;
		const QString& sFilter = filters.join(";;");
		const QFileDialog::Options options = QFileDialog::DontUseNativeDialog;
		if (pvfs->command == kVstFileLoad) {
			sFilename = QFileDialog::getOpenFileName(
				pParentWidget, sTitle, sDirectory, sFilter, NULL, options);
		} else {
			sFilename = QFileDialog::getSaveFileName(
				pParentWidget, sTitle, sDirectory, sFilter, NULL, options);
		}
		if (!sFilename.isEmpty()) {
			if (pvfs->returnPath == NULL) {
				pvfs->returnPath = new char [sFilename.length() + 1];
				pvfs->reserved = 1;
			}
			::strcpy(pvfs->returnPath, sFilename.toUtf8().constData());
			pvfs->nbReturnPath = 1;
		}
    }
	else
	if (pvfs->command == kVstDirectorySelect) {
		QWidget *pParentWidget = pVstPlugin->editorWidget();
		const QString& sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg((pVstPlugin->type())->name());
		const QString& sDirectory
			= QFileDialog::getExistingDirectory(
				pParentWidget, sTitle, pvfs->initialPath,
				QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
		if (!sDirectory.isEmpty()) {
			if (pvfs->returnPath == NULL) {
				pvfs->returnPath = new char [sDirectory.length() + 1];
				pvfs->reserved = 1;
			}
			::strcpy(pvfs->returnPath, sDirectory.toUtf8().constData());
			pvfs->nbReturnPath = 1;
		}
	}

    return pvfs->nbReturnPath;
}


static VstIntPtr qtractorVstPlugin_closeFileSelector (
	AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVstPlugin *pVstPlugin = qtractorVstPlugin::findPlugin(effect);
	if (pVstPlugin == NULL)
		return 0;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin_closeFileSelector(%p, %p) command=%d reserved=%d",
		effect, pvfs, int(pvfs->command), int(pvfs->reserved));
#endif

	if (pvfs->reserved == 1 && pvfs->returnPath) {
		delete [] pvfs->returnPath;
		pvfs->returnPath = NULL;
		pvfs->reserved = 0;
	}

	return 1;
}

#endif	// !CONFIG_VESTIGE


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

static VstIntPtr VSTCALLBACK qtractorVstPlugin_HostCallback ( AEffect *effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt )
{
	VstIntPtr ret = 0;
	qtractorVstPlugin *pVstPlugin = NULL;
	static VstTimeInfo s_vstTimeInfo;
	qtractorSession *pSession = qtractorSession::getInstance();

	switch (opcode) {

	// VST 1.0 opcodes...
	case audioMasterVersion:
		VST_HC_DEBUG("audioMasterVersion");
		ret = 2; // vst2.x
		break;

	case audioMasterAutomate:
		VST_HC_DEBUG("audioMasterAutomate");
	//	effect->setParameter(effect, index, opt);
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			pVstPlugin->updateParamValue(index, opt, false);
		//	QApplication::processEvents();
		}
		break;

	case audioMasterCurrentId:
		VST_HC_DEBUG("audioMasterCurrentId");
		break;

	case audioMasterIdle:
		VST_HC_DEBUG("audioMasterIdle");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			pVstPlugin->updateParamValues(false);
			pVstPlugin->idleEditor();
		//	QApplication::processEvents();
		}
		break;

	case audioMasterGetTime:
	//	VST_HC_DEBUG("audioMasterGetTime");
		if (pSession) {
			::memset(&s_vstTimeInfo, 0, sizeof(s_vstTimeInfo));
			unsigned long iPlayHead = pSession->playHead();
			qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
			qtractorTimeScale::Node *pNode = cursor.seekFrame(iPlayHead);
			s_vstTimeInfo.samplePos = double(iPlayHead);
			s_vstTimeInfo.sampleRate = double(pSession->sampleRate());
			s_vstTimeInfo.flags = 0;
			if (pSession->isPlaying())
				s_vstTimeInfo.flags |= (kVstTransportChanged | kVstTransportPlaying);
			if (pNode) {
				unsigned short bars  = 0;
				unsigned int   beats = 0;
				unsigned long  ticks = pNode->tickFromFrame(iPlayHead) - pNode->tick;
				if (ticks >= (unsigned long) pNode->ticksPerBeat) {
					beats += (unsigned int)  (ticks / pNode->ticksPerBeat);
					ticks -= (unsigned long) (beats * pNode->ticksPerBeat);
				}
				if (beats >= (unsigned int) pNode->beatsPerBar) {
					bars  += (unsigned short) (beats / pNode->beatsPerBar);
				//	beats -= (unsigned int) (bars * pNode->beatsPerBar);
				}
				s_vstTimeInfo.ppqPos = double(pNode->beat + beats)
					+ (double(ticks) / double(pNode->ticksPerBeat));
				s_vstTimeInfo.flags |= kVstPpqPosValid;
				s_vstTimeInfo.tempo  = double(pNode->tempo);
				s_vstTimeInfo.flags |= kVstTempoValid;
				s_vstTimeInfo.barStartPos = double(pNode->beat)
					+ double(bars * pNode->beatsPerBar);
				s_vstTimeInfo.flags |= kVstBarsValid;
				if (pSession->isLooping()) {
					s_vstTimeInfo.cycleStartPos
						= double(pNode->beatFromFrame(pSession->loopStart()));
					s_vstTimeInfo.cycleEndPos
						= double(pNode->beatFromFrame(pSession->loopEnd()));
					s_vstTimeInfo.flags |= kVstCyclePosValid;
				}
				s_vstTimeInfo.timeSigNumerator = pNode->beatsPerBar;
				s_vstTimeInfo.timeSigDenominator = (1 << pNode->beatDivisor);
				s_vstTimeInfo.flags |= kVstTimeSigValid;
			}
			ret = (VstIntPtr) &s_vstTimeInfo;
		}
		break;

	case audioMasterProcessEvents:
		VST_HC_DEBUG("audioMasterProcessEvents");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			qtractorMidiManager *pMidiManager = NULL;
			qtractorPluginList *pPluginList = pVstPlugin->list();
			if (pPluginList)
				pMidiManager = pPluginList->midiManager();
			if (pMidiManager) {
				pMidiManager->vst_events_copy((VstEvents *) ptr);
				ret = 1; // supported and processed.
			}
		}
		break;

	case audioMasterIOChanged:
		VST_HC_DEBUG("audioMasterIOChanged");
		break;

	case audioMasterSizeWindow:
		VST_HC_DEBUG("audioMasterSizeWindow");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			pVstPlugin->resizeEditor(int(index), int(value));
			ret = 1; // supported.
		}
		break;

	case audioMasterGetSampleRate:
		VST_HC_DEBUG("audioMasterGetSampleRate");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin)
			ret = (VstIntPtr) pVstPlugin->sampleRate();
		break;

	case audioMasterGetBlockSize:
		VST_HC_DEBUG("audioMasterGetBlockSize");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin)
			ret = (VstIntPtr) pVstPlugin->bufferSize();
		break;

	case audioMasterGetInputLatency:
		VST_HC_DEBUG("audioMasterGetInputLatency");
		break;

	case audioMasterGetOutputLatency:
		VST_HC_DEBUG("audioMasterGetOutputLatency");
		break;

	case audioMasterGetCurrentProcessLevel:
		VST_HC_DEBUG("audioMasterGetCurrentProcessLevel");
		break;

	case audioMasterGetAutomationState:
		VST_HC_DEBUG("audioMasterGetAutomationState");
		ret = 1; // off.
		break;

#if !defined(VST_2_3_EXTENSIONS) 
	case audioMasterGetSpeakerArrangement:
		VST_HC_DEBUG("audioMasterGetSpeakerArrangement");
		break;
#endif

	case audioMasterGetVendorString:
		VST_HC_DEBUG("audioMasterGetVendorString");
		::strcpy((char *) ptr, QTRACTOR_DOMAIN);
		ret = 1; // ok.
		break;

	case audioMasterGetProductString:
		VST_HC_DEBUG("audioMasterGetProductString");
		::strcpy((char *) ptr, QTRACTOR_TITLE);
		ret = 1; // ok.
		break;

	case audioMasterGetVendorVersion:
		VST_HC_DEBUG("audioMasterGetVendorVersion");
		break;

	case audioMasterVendorSpecific:
		VST_HC_DEBUG("audioMasterVendorSpecific");
		break;

	case audioMasterCanDo:
		VST_HC_DEBUGX("audioMasterCanDo");
		if (::strcmp("receiveVstMidiEvent", (char *) ptr) == 0 ||
			::strcmp("sendVstMidiEvent",    (char *) ptr) == 0 ||
			::strcmp("sendVstTimeInfo",     (char *) ptr) == 0 ||
			::strcmp("midiProgramNames",    (char *) ptr) == 0 ||
			::strcmp("sizeWindow",          (char *) ptr) == 0) {
			ret = 1; // can do.
		}
	#ifndef CONFIG_VESTIGE
		else
		if (::strcmp("openFileSelector",    (char *) ptr) == 0 ||
			::strcmp("closeFileSelector",   (char *) ptr) == 0) {
			ret = 1; // can do.
		}
	#endif
		break;

	case audioMasterGetLanguage:
		VST_HC_DEBUG("audioMasterGetLanguage");
		ret = (VstIntPtr) kVstLangEnglish;
		break;

#if 0 // !VST_FORCE_DEPRECATED
	case audioMasterPinConnected:
		VST_HC_DEBUG("audioMasterPinConnected");
		break;

	// VST 2.0 opcodes...
	case audioMasterWantMidi:
		VST_HC_DEBUG("audioMasterWantMidi");
		break;

	case audioMasterSetTime:
		VST_HC_DEBUG("audioMasterSetTime");
		break;

	case audioMasterTempoAt:
		VST_HC_DEBUG("audioMasterTempoAt");
		if (pSession)
			ret = (VstIntPtr) (pSession->tempo() * 10000.0f);
		break;

	case audioMasterGetNumAutomatableParameters:
		VST_HC_DEBUG("audioMasterGetNumAutomatableParameters");
		break;

	case audioMasterGetParameterQuantization:
		VST_HC_DEBUG("audioMasterGetParameterQuantization");
		ret = 1; // full single float precision
		break;

	case audioMasterNeedIdle:
		VST_HC_DEBUG("audioMasterNeedIdle");
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

	case audioMasterSetOutputSampleRate:
		VST_HC_DEBUG("audioMasterSetOutputSampleRate");
		break;

	case audioMasterSetIcon:
		VST_HC_DEBUG("audioMasterSetIcon");
		break;

	case audioMasterOpenWindow:
		VST_HC_DEBUG("audioMasterOpenWindow");
		break;

	case audioMasterCloseWindow:
		VST_HC_DEBUG("audioMasterCloseWindow");
		break;
#endif

	case audioMasterGetDirectory:
		VST_HC_DEBUG("audioMasterGetDirectory");
		break;

	case audioMasterUpdateDisplay:
		VST_HC_DEBUG("audioMasterUpdateDisplay");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			pVstPlugin->updateParamValues(false);
		//	QApplication::processEvents();
			ret = 1; // supported.
		}
		break;

	case audioMasterBeginEdit:
		VST_HC_DEBUG("audioMasterBeginEdit");
		break;

	case audioMasterEndEdit:
		VST_HC_DEBUG("audioMasterEndEdit");
		break;

#ifndef CONFIG_VESTIGE
	case audioMasterOpenFileSelector:
		VST_HC_DEBUG("audioMasterOpenFileSelector");
		ret = qtractorVstPlugin_openFileSelector(effect, (VstFileSelect *) ptr);
		break;

	case audioMasterCloseFileSelector:
		VST_HC_DEBUG("audioMasterCloseFileSelector");
		ret = qtractorVstPlugin_closeFileSelector(effect, (VstFileSelect *) ptr);
		break;
#endif
	default:
		VST_HC_DEBUG("audioMasterUnknown");
		break;
	}

	return ret;
}


#endif	// CONFIG_VST

// end of qtractorVstPlugin.cpp
