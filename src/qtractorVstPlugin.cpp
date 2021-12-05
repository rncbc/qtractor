// qtractorVstPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2021, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"

#include "qtractorOptions.h"

#if 0//QTRACTOR_VST_EDITOR_TOOL
#include "qtractorMainForm.h"
#endif

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>


#if QT_VERSION < QT_VERSION_CHECK(4, 5, 0)
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif


#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST_X11
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
typedef void (*XEventProc)(XEvent *);
#endif	// CONFIG_VST_X11
#else
#include <QWindow>
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
const int effSetProgramName = 4;
const int effGetParamLabel = 6;
const int effGetParamDisplay = 7;
const int effGetChunk = 23;
const int effSetChunk = 24;
const int effFlagsProgramChunks = 32;
#endif


//---------------------------------------------------------------------
// qtractorVstPlugin::EditorWidget - Helpers for own editor widget.

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST_X11

static int g_iXError = 0;

static int tempXErrorHandler ( Display *, XErrorEvent * )
{
	++g_iXError;
	return 0;
}

static XEventProc getXEventProc ( Display *pDisplay, Window w )
{
	int iSize = 0;
	unsigned long iBytes = 0, iCount = 0;
	unsigned char *pData = nullptr;
	XEventProc eventProc = nullptr;
	Atom aType, aName = XInternAtom(pDisplay, "_XEventProc", false);
#if defined(__x86_64__)
	const long length = 2;
#else
	const long length = 1;
#endif
	g_iXError = 0;

	XErrorHandler oldErrorHandler = XSetErrorHandler(tempXErrorHandler);
	XGetWindowProperty(pDisplay, w, aName, 0, length, false,
		AnyPropertyType, &aType, &iSize, &iCount, &iBytes, &pData);
	if (g_iXError == 0 && iCount > 0 && pData) {
		if (iCount == 1)
			eventProc = (XEventProc) (pData);
		XFree(pData);
	}
	XSetErrorHandler(oldErrorHandler);

	return eventProc;
}

static Window getXChildWindow ( Display *pDisplay, Window w )
{
	Window wRoot = 0, wParent = 0, *pwChildren = nullptr, wChild = 0;
	unsigned int iChildren = 0;

	XQueryTree(pDisplay, w, &wRoot, &wParent, &pwChildren, &iChildren);

	if (iChildren > 0) {
		wChild = pwChildren[0];
		XFree(pwChildren);
	}

	return wChild;
}

#endif // CONFIG_VST_X11
#endif


//----------------------------------------------------------------------------
// qtractorVstPlugin::EditorWidget -- Plugin editor wrapper widget.
//

// Dynamic singleton list of VST editors.
static QList<qtractorVstPlugin::EditorWidget *> g_vstEditors;

class qtractorVstPlugin::EditorWidget : public QWidget
{
public:

	// Constructor.
	EditorWidget(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags())
		: QWidget(pParent, wflags),
	#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#ifdef CONFIG_VST_X11
		m_pDisplay(QX11Info::display()),
		m_wVstEditor(0),
		m_pVstEventProc(nullptr),
		m_bButtonPress(false),
	#endif	// CONFIG_VST_X11
	#else
		m_pWindow(nullptr),
	#endif
		m_pVstPlugin(nullptr)
		{ QWidget::setAttribute(Qt::WA_QuitOnClose, false); }

	// Destructor.
	~EditorWidget() { close(); }

	// Specialized editor methods.
	void open(qtractorVstPlugin *pVstPlugin)
	{
		m_pVstPlugin = pVstPlugin;
		
		// Start the proper (child) editor...
		long  value = 0;
		void *ptr = nullptr;
	#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#ifdef CONFIG_VST_X11
		value = (long) m_pDisplay;
		ptr = (void *) QWidget::winId();
	#endif // CONFIG_VST_X11
	#else
		m_pWindow = new QWindow();
		m_pWindow->create();
		QWidget *pContainer = QWidget::createWindowContainer(m_pWindow, this);
		QVBoxLayout *pVBoxLayout = new QVBoxLayout();
		pVBoxLayout->setContentsMargins(0, 0, 0, 0);
		pVBoxLayout->setSpacing(0);
		pVBoxLayout->addWidget(pContainer);
		QWidget::setLayout(pVBoxLayout);
		ptr = (void *) m_pWindow->winId();
	#endif

		// Launch the custom GUI editor...
		m_pVstPlugin->vst_dispatch(0, effEditOpen, 0, value, ptr, 0.0f);

		// Make it the right size
		struct ERect {
			short top;
			short left;
			short bottom;
			short right;
		} *pRect;

		if (m_pVstPlugin->vst_dispatch(0, effEditGetRect, 0, 0, &pRect, 0.0f)) {
			const int w = pRect->right - pRect->left;
			const int h = pRect->bottom - pRect->top;
			if (w > 0 && h > 0)
				QWidget::setFixedSize(w, h);
		}

	#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#ifdef CONFIG_VST_X11
		m_wVstEditor = getXChildWindow(m_pDisplay, (Window) QWidget::winId());
		if (m_wVstEditor)
			m_pVstEventProc = getXEventProc(m_pDisplay, m_wVstEditor);
	#endif	// CONFIG_VST_X11
	#endif

		g_vstEditors.append(this);
	}

	// Close the editor widget.
	void close()
	{
		QWidget::close();

		if (m_pVstPlugin) {
			m_pVstPlugin->vst_dispatch(0, effEditClose, 0, 0, nullptr, 0.0f);
			m_pVstPlugin = nullptr;
		}

		const int iIndex = g_vstEditors.indexOf(this);
		if (iIndex >= 0)
			g_vstEditors.removeAt(iIndex);

	#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
		if (m_pWindow) {
			m_pWindow->destroy();
			delete m_pWindow;
		}
	#endif
	}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST_X11
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
#endif	// CONFIG_VST_X11
#endif

	qtractorVstPlugin *plugin() const
		{ return m_pVstPlugin; }

protected:

	// Visibility event handlers.
	void showEvent(QShowEvent *pShowEvent)
	{
		QWidget::showEvent(pShowEvent);

		if (m_pVstPlugin)
			m_pVstPlugin->toggleFormEditor(true);
	}

	void closeEvent(QCloseEvent *pCloseEvent)
	{
		if (m_pVstPlugin)
			m_pVstPlugin->toggleFormEditor(false);

		QWidget::closeEvent(pCloseEvent);

		if (m_pVstPlugin)
			m_pVstPlugin->closeEditor();
	}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST_X11
	void moveEvent(QMoveEvent *pMoveEvent)
	{
		QWidget::moveEvent(pMoveEvent);
		if (m_wVstEditor) {
			XMoveWindow(m_pDisplay, m_wVstEditor, 0, 0);
		//	QWidget::update();
		}
	}
#endif	// CONFIG_VST_X11
#endif

private:

	// Instance variables...
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST_X11
	Display   *m_pDisplay;
	Window     m_wVstEditor;
	XEventProc m_pVstEventProc;
	bool       m_bButtonPress;
#endif	// CONFIG_VST_X11
#else
	QWindow   *m_pWindow;
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
	Effect(AEffect *pVstEffect = nullptr) : m_pVstEffect(pVstEffect) {}

	// Specific methods.
	bool open(qtractorPluginFile *pFile, unsigned long iIndex);
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


// Current working VST Shell identifier.
static int g_iVstShellCurrentId = 0;


// Specific methods.
bool qtractorVstPluginType::Effect::open (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Do we have an effect descriptor already?
	if (m_pVstEffect == nullptr)
		m_pVstEffect = qtractorVstPluginType::vst_effect(pFile);
	if (m_pVstEffect == nullptr)
		return false;

	// Check whether it's a VST Shell...
	const int categ = vst_dispatch(effGetPlugCategory, 0, 0, nullptr, 0.0f);
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
			m_pVstEffect = nullptr;
			return false;
		}
		// Make it known...
		g_iVstShellCurrentId = id;
		// Re-allocate the thing all over again...
		m_pVstEffect = qtractorVstPluginType::vst_effect(pFile);
		// Not needed anymore, hopefully...
		g_iVstShellCurrentId = 0;
		// Don't go further if failed...
		if (m_pVstEffect == nullptr)
			return false;
	#ifdef CONFIG_DEBUG//_0
		qDebug("AEffect[%p]::vst_shell(%lu) id=0x%x name=\"%s\"",
			m_pVstEffect, i, id, buf);
	#endif
	}
	else
	// Not a VST Shell plugin...
	if (iIndex > 0) {
		m_pVstEffect = nullptr;
		return false;
	}

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::open(%p, %lu)", m_pVstEffect, pFile, iIndex);
#endif

	vst_dispatch(effOpen, 0, 0, nullptr, 0.0f);
//	vst_dispatch(effIdentify, 0, 0, nullptr, 0);
//	vst_dispatch(effMainsChanged, 0, 0, nullptr, 0.0f);

	return true;
}


void qtractorVstPluginType::Effect::close (void)
{
	if (m_pVstEffect == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::close()", m_pVstEffect);
#endif

//	vst_dispatch(effMainsChanged, 0, 0, nullptr, 0.0f);
	vst_dispatch(effClose, 0, 0, nullptr, 0.0f);

	m_pVstEffect = nullptr;
}


// VST host dispatcher.
int qtractorVstPluginType::Effect::vst_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pVstEffect == nullptr)
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
	if (m_pEffect == nullptr)
		m_pEffect = new Effect();
	if (!m_pEffect->open(file(), index()))
		return false;

	AEffect *pVstEffect = m_pEffect->vst_effect();
	if (pVstEffect == nullptr)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPluginType[%p]::open() filename=\"%s\" index=%lu",
		this, filename().toUtf8().constData(), index());
#endif

	// Retrieve plugin type names.
	char szName[256];
	::memset(szName, 0, sizeof(szName));
	vst_dispatch(effGetEffectName, 0, 0, (void *) szName, 0.0f);
	if (szName[0])
		m_sName = QString::fromLocal8Bit(szName);
	else
		m_sName = QFileInfo(filename()).baseName();
	// Sanitize plugin label.
	m_sLabel = m_sName.simplified().replace(QRegularExpression("[\\s|\\.|\\-]+"), "_");

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

	// HACK: Some native VST plugins with a GUI editor
	// need to skip explicit shared library unloading,
	// on close, in order to avoid mysterious crashes
	// later on session and/or application exit.
	if (m_bEditor) file()->setAutoUnload(false);

	return true;
}


void qtractorVstPluginType::close (void)
{
	if (m_pEffect == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPluginType[%p]::close()", this);
#endif

	m_pEffect->close();

	delete m_pEffect;
	m_pEffect = nullptr;
}


// Factory method (static)
qtractorVstPluginType *qtractorVstPluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Sanity check...
	if (pFile == nullptr)
		return nullptr;

	// Retrieve effect instance if any...
	AEffect *pVstEffect = vst_effect(pFile);
	if (pVstEffect == nullptr)
		return nullptr;

	// Yep, most probably its a valid plugin effect...
	return new qtractorVstPluginType(pFile, iIndex, new Effect(pVstEffect));
}


// Effect instance method (static)
AEffect *qtractorVstPluginType::vst_effect ( qtractorPluginFile *pFile )
{
	VST_GetPluginInstance pfnGetPluginInstance
		= (VST_GetPluginInstance) pFile->resolve("VSTPluginMain");
	if (pfnGetPluginInstance == nullptr)
		pfnGetPluginInstance = (VST_GetPluginInstance) pFile->resolve("main");
	if (pfnGetPluginInstance == nullptr)
		return nullptr;

	// Does the VST plugin instantiate OK?
	AEffect *pVstEffect
		= (*pfnGetPluginInstance)(qtractorVstPlugin_HostCallback);
	if (pVstEffect == nullptr)
		return nullptr;
	if (pVstEffect->magic != kEffectMagic)
		return nullptr;

	return pVstEffect;
}


// VST host dispatcher.
int qtractorVstPluginType::vst_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pEffect == nullptr)
		return 0;

	return m_pEffect->vst_dispatch(opcode, index, value, ptr, opt);
}


// VST flag inquirer.
bool qtractorVstPluginType::vst_canDo ( const char *pszCanDo ) const
{
	if (m_pEffect == nullptr)
		return false;

	return (m_pEffect->vst_dispatch(effCanDo, 0, 0, (void *) pszCanDo, 0.0f) > 0);
}


// Instance cached-deferred accessors.
const QString& qtractorVstPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		char szTemp[256];
		::memset(szTemp, 0, sizeof(szTemp));
		vst_dispatch(effGetProductString, 0, 0, (void *) szTemp, 0.0f);
		if (szTemp[0]) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Product: ");
			m_sAboutText += QString::fromLocal8Bit(szTemp);
		}
		::memset(szTemp, 0, sizeof(szTemp));
		vst_dispatch(effGetVendorString, 0, 0, (void *) szTemp, 0.0f);
		if (szTemp[0]) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Vendor: ");
			m_sAboutText += QString::fromLocal8Bit(szTemp);
		}
		const int iVersion
			= vst_dispatch(effGetVendorVersion, 0, 0, nullptr, 0.0f);
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
qtractorVstPlugin::qtractorVstPlugin (
	qtractorPluginList *pList, qtractorVstPluginType *pVstType )
	: qtractorPlugin(pList, pVstType), m_ppEffects(nullptr),
		m_ppIBuffer(nullptr), m_ppOBuffer(nullptr),
		m_pfIDummy(nullptr), m_pfODummy(nullptr),
		m_pEditorWidget(nullptr), m_bEditorClosed(false)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p] filename=\"%s\" index=%lu typeHint=%d",
		this, type()->filename().toUtf8().constData(),
		type()->index(), int(type()->typeHint()));
#endif

	// Allocate I/O audio buffer pointers.
	const unsigned short iAudioIns  = pVstType->audioIns();
	const unsigned short iAudioOuts = pVstType->audioOuts();
	if (iAudioIns > 0)
		m_ppIBuffer = new float * [iAudioIns];
	if (iAudioOuts > 0)
		m_ppOBuffer = new float * [iAudioOuts];

	// Create all existing parameters...
	const unsigned short iParamCount = pVstType->controlIns();
	for (unsigned short i = 0; i < iParamCount; ++i)
		addParam(new Param(this, i));

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

	if (m_pfIDummy)
		delete [] m_pfIDummy;
	if (m_pfODummy)
		delete [] m_pfODummy;
}


// Channel/instance number accessors.
void qtractorVstPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorVstPluginType *pVstType
		= static_cast<qtractorVstPluginType *> (type());
	if (pVstType == nullptr)
		return;

	// Estimate the (new) number of instances...
	const unsigned short iOldInstances = instances();
	unsigned short iInstances = 0;
	if (iChannels > 0) {
		iInstances = pVstType->instances(iChannels, list()->isMidi());
		// Now see if instance and channel count changed anyhow...
		if (iInstances == iOldInstances && iChannels == channels())
			return;
	}

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setChannelsActivated(iChannels, false);

	// Set new instance number...
	setInstances(iInstances);

	// Close old instances, all the way...
	if (m_ppEffects) {
		qtractorVstPluginType::Effect *pEffect;
		for (unsigned short i = 1; i < iOldInstances; ++i) {
			pEffect = m_ppEffects[i];
			g_vstPlugins.remove(pEffect->vst_effect());
			pEffect->close();
			delete pEffect;
		}
		pEffect = m_ppEffects[0];
		g_vstPlugins.remove(pEffect->vst_effect());
		delete [] m_ppEffects;
		m_ppEffects = nullptr;
	}

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setChannelsActivated(iChannels, bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	const unsigned int iSampleRate = pAudioEngine->sampleRate();
	const unsigned int iBufferSize = pAudioEngine->bufferSize();
	const unsigned int iBufferSizeEx = pAudioEngine->bufferSizeEx();

	// Allocate new instances...
	m_ppEffects = new qtractorVstPluginType::Effect * [iInstances];
	m_ppEffects[0] = pVstType->effect();
	g_vstPlugins.insert(m_ppEffects[0]->vst_effect(), this);
	for (unsigned short i = 1; i < iInstances; ++i) {
		m_ppEffects[i] = new qtractorVstPluginType::Effect();
		m_ppEffects[i]->open(pVstType->file(), pVstType->index());
		g_vstPlugins.insert(m_ppEffects[i]->vst_effect(), this);
	}

	// Allocate the dummy audio I/O buffers...
	const unsigned short iAudioIns = audioIns();
	const unsigned short iAudioOuts = audioOuts();

	if (iChannels < iAudioIns) {
		if (m_pfIDummy)
			delete [] m_pfIDummy;
		m_pfIDummy = new float [iBufferSizeEx];
		::memset(m_pfIDummy, 0, iBufferSizeEx * sizeof(float));
	}

	if (iChannels < iAudioOuts) {
		if (m_pfODummy)
			delete [] m_pfODummy;
		m_pfODummy = new float [iBufferSizeEx];
	//	::memset(m_pfODummy, 0, iBufferSizeEx * sizeof(float));
	}

	// Setup all those instances alright...
	for (unsigned short i = 0; i < iInstances; ++i) {
		// And now all other things as well...
		qtractorVstPluginType::Effect *pEffect = m_ppEffects[i];
	//	pEffect->vst_dispatch(effOpen, 0, 0, nullptr, 0.0f);
		pEffect->vst_dispatch(effSetSampleRate, 0, 0, nullptr, float(iSampleRate));
		pEffect->vst_dispatch(effSetBlockSize,  0, iBufferSize, nullptr, 0.0f);
	//	pEffect->vst_dispatch(effSetProgram, 0, 0, nullptr, 0.0f);
	#if 0 // !VST_FORCE_DEPRECATED
		unsigned short j;
		for (j = 0; j < iAudioIns; ++j)
			pEffect->vst_dispatch(effConnectInput, j, 1, nullptr, 0.0f);
		for (j = 0; j < iAudioOuts; ++j)
			pEffect->vst_dispatch(effConnectOutput, j, 1, nullptr, 0.0f);
	#endif
	}

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// (Re)activate instance if necessary...
	setChannelsActivated(iChannels, bActivated);
}


// Do the actual activation.
void qtractorVstPlugin::activate (void)
{
	for (unsigned short i = 0; i < instances(); ++i) {
		vst_dispatch(i, effMainsChanged, 0, 1, nullptr, 0.0f);
	#ifndef CONFIG_VESTIGE
		vst_dispatch(i, effStartProcess, 0, 0, nullptr, 0.0f);
	#endif
	}
}


// Do the actual deactivation.
void qtractorVstPlugin::deactivate (void)
{
	for (unsigned short i = 0; i < instances(); ++i) {
	#ifndef CONFIG_VESTIGE
		vst_dispatch(i, effStopProcess, 0, 0, nullptr, 0.0f);
	#endif
		vst_dispatch(i, effMainsChanged, 0, 0, nullptr, 0.0f);
	}
}


// The main plugin processing procedure.
void qtractorVstPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_ppEffects == nullptr)
		return;

	// To process MIDI events, if any...
	qtractorMidiManager *pMidiManager = nullptr;
	const unsigned short iMidiIns  = midiIns();
	const unsigned short iMidiOuts = midiOuts();
	if (iMidiIns > 0)
		pMidiManager = list()->midiManager();

	// We'll cross channels over instances...
	const unsigned short iInstances = instances();
	const unsigned short iChannels  = channels();
	const unsigned short iAudioIns  = audioIns();
	const unsigned short iAudioOuts = audioOuts();

	unsigned short iIChannel = 0;
	unsigned short iOChannel = 0;
	unsigned short i, j;

	// For each plugin instance...
	for (i = 0; i < iInstances; ++i) {
		AEffect *pVstEffect = vst_effect(i);
		// For each instance audio input port...
		for (j = 0; j < iAudioIns; ++j) {
			if (iIChannel < iChannels)
				m_ppIBuffer[j] = ppIBuffer[iIChannel++];
			else
				m_ppIBuffer[j] = m_pfIDummy; // dummy input!
		}
		// For each instance audio output port...
		for (j = 0; j < iAudioOuts; ++j) {
			if (iOChannel < iChannels)
				m_ppOBuffer[j] = ppOBuffer[iOChannel++];
			else
				m_ppOBuffer[j] = m_pfODummy; // dummy output!
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
		// Wrap dangling output channels?...
		for (j = iOChannel; j < iChannels; ++j)
			::memset(ppOBuffer[j], 0, nframes * sizeof(float));
	}

	if (pMidiManager && iMidiOuts > 0)
		pMidiManager->vst_events_swap();
}


// Parameter update method.
void qtractorVstPlugin::updateParam (
	qtractorPlugin::Param *pParam, float fValue, bool /*bUpdate*/ )
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
	if (list()->isMidiBus())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	int iIndex = 0;
	if (iBank >= 0 && iProg >= 0)
		iIndex = (iBank << 7) + iProg;

	for (unsigned short i = 0; i < instances(); ++i)
		vst_dispatch(i, effSetProgram, 0, iIndex, nullptr, 0.0f);

	// Reset parameters default value...
	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect) {
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPlugin::Param *pParam = param.value();
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
	if (pVstEffect == nullptr)
		return false;
	if (iIndex < 0 || iIndex >= pVstEffect->numPrograms)
		return false;

	char szName[256];
	::memset(szName, 0, sizeof(szName));
#ifndef CONFIG_VESTIGE
	if (vst_dispatch(0, effGetProgramNameIndexed, iIndex, 0, (void *) szName, 0.0f) == 0) {
#endif
		const int iOldIndex = vst_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);
		vst_dispatch(0, effSetProgram, 0, iIndex, nullptr, 0.0f);
		vst_dispatch(0, effGetProgramName, 0, 0, (void *) szName, 0.0f);
		vst_dispatch(0, effSetProgram, 0, iOldIndex, nullptr, 0.0f);
#ifndef CONFIG_VESTIGE
	}
#endif

	// Map this to that...
	program.bank = 0;
	program.prog = iIndex;
	program.name = QString::fromLocal8Bit(szName);

	return true;
}


// Configuration (CLOB) stuff.
void qtractorVstPlugin::configure (
	const QString& sKey, const QString& sValue )
{
	if (sKey == "chunk") {
		// Load the BLOB (base64 encoded)...
		const QByteArray data
			= qUncompress(QByteArray::fromBase64(sValue.toLatin1()));
		const char *pData = data.constData();
		const int iData = data.size();
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPlugin[%p]::configure() chunk.size=%d", this, iData);
	#endif
		for (unsigned short i = 0; i < instances(); ++i)
			vst_dispatch(i, effSetChunk, 0, iData, (void *) pData, 0.0f);
	}
}


// Plugin configuration/state snapshot.
void qtractorVstPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVstPlugin[%p]::freezeConfigs()", this);
#endif

	// HACK: Make sure all parameter values are in sync,
	// provided freezeConfigs() are always called when
	// saving plugin's state and before parameter values.
	updateParamValues(false);

	// Also, update current editor position...
	if (m_pEditorWidget && m_pEditorWidget->isVisible())
		setEditorPos(m_pEditorWidget->pos());

	if (!type()->isConfigure())
		return;

	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect == nullptr)
		return;

	// Save plugin state into chunk configuration...
	char *pData = nullptr;
	const int iData
		= vst_dispatch(0, effGetChunk, 0, 0, (void *) &pData, 0.0f);
	if (iData < 1 || pData == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::freezeConfigs() chunk.size=%d", this, iData);
#endif

	// Set special plugin configuration item (base64 encoded)...
	QByteArray cdata = qCompress(QByteArray(pData, iData)).toBase64();
	for (int i = cdata.size() - (cdata.size() % 72); i >= 0; i -= 72)
		cdata.insert(i, "\n       "); // Indentation.
	setConfig("chunk", cdata.constData());
}


void qtractorVstPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::releaseConfigs()", this);
#endif

	qtractorPlugin::clearConfigs();
}


// Plugin current latency (in frames);
unsigned long qtractorVstPlugin::latency (void) const
{
	AEffect *pVstEffect = vst_effect(0);
	if (pVstEffect == nullptr)
		return 0;

#ifdef CONFIG_VESTIGE
	const VstInt32 *pInitialDelay
		= (VstInt32 *) &(pVstEffect->empty3[0]);
	return *pInitialDelay;
#else
	return pVstEffect->initialDelay;
#endif
}


// Plugin preset i/o (configuration from/to (fxp/fxb files).
bool qtractorVstPlugin::loadPresetFile ( const QString& sFilename )
{
	bool bResult = false;

	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "fxp" || sExt == "fxb") {
		bResult = qtractorVstPreset(this).load(sFilename);
	} else {
		const int iCurrentProgram
			= vst_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);
		bResult = qtractorPlugin::loadPresetFile(sFilename);
		for (unsigned short i = 0; i < instances(); ++i)
			vst_dispatch(i, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);
	}

	return bResult;
}

bool qtractorVstPlugin::savePresetFile ( const QString& sFilename )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "fxp" || sExt == "fxb")
		return qtractorVstPreset(this).save(sFilename);
	else
		return qtractorPlugin::savePresetFile(sFilename);
}


// Specific accessors.
AEffect *qtractorVstPlugin::vst_effect ( unsigned short iInstance ) const
{
	if (m_ppEffects == nullptr)
		return nullptr;

	return m_ppEffects[iInstance]->vst_effect();
}


// VST host dispatcher.
int qtractorVstPlugin::vst_dispatch( unsigned short iInstance,
	long opcode, long index, long value, void *ptr, float opt) const
{
	if (m_ppEffects == nullptr)
		return 0;

	return m_ppEffects[iInstance]->vst_dispatch(opcode, index, value, ptr, opt);
}


// Open editor.
void qtractorVstPlugin::openEditor ( QWidget *pParent )
{
	// Is it already there?
	if (m_pEditorWidget) {
		if (!m_pEditorWidget->isVisible()) {
			moveWidgetPos(m_pEditorWidget, editorPos());
			m_pEditorWidget->show();
		}
		m_pEditorWidget->raise();
		m_pEditorWidget->activateWindow();
		return;
	}

	// Tell the world we'll (maybe) take some time...
	qtractorPluginList::WaitCursor waiting;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::openEditor(%p)", this, pParent);
#endif

	// Make sure it's not closed...
	m_bEditorClosed = false;

	// What style do we create tool childs?
	Qt::WindowFlags wflags = Qt::Window;
#if 0//QTRACTOR_VST_EDITOR_TOOL
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bKeepToolsOnTop) {
		wflags |= Qt::Tool;
	//	wflags |= Qt::WindowStaysOnTopHint;
		// Make sure it has a parent...
		if (pParent == nullptr)
			pParent = qtractorMainForm::getInstance();
	}
#endif

	// Do it...
	m_pEditorWidget = new EditorWidget(pParent, wflags);
	m_pEditorWidget->open(this);

	// Final stabilization...
	updateEditorTitle();
	moveWidgetPos(m_pEditorWidget, editorPos());
	setEditorVisible(true);
	idleEditor();
}


// Close editor.
void qtractorVstPlugin::closeEditor (void)
{
	if (m_bEditorClosed)
		return;

	m_bEditorClosed = true;

	if (m_pEditorWidget == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

	// Close the parent widget, if any.
	delete m_pEditorWidget;
	m_pEditorWidget = nullptr;
}


// Idle editor.
void qtractorVstPlugin::idleEditor (void)
{
	if (m_pEditorWidget == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVstPlugin[%p]::idleEditor()", this);
#endif

	vst_dispatch(0, effEditIdle, 0, 0, nullptr, 0.0f);

	m_pEditorWidget->update();
}


// GUI editor visibility state.
void qtractorVstPlugin::setEditorVisible ( bool bVisible )
{
	if (m_pEditorWidget) {
		if (!bVisible)
			setEditorPos(m_pEditorWidget->pos());
		m_pEditorWidget->setVisible(bVisible);
		if (bVisible) {
			m_pEditorWidget->raise();
			m_pEditorWidget->activateWindow();
		}
	}
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
		m_pEditorWidget->setFixedSize(w, h);
}


// Global VST plugin lookup.
qtractorVstPlugin *qtractorVstPlugin::findPlugin ( AEffect *pVstEffect )
{
	return g_vstPlugins.value(pVstEffect, nullptr);
}


#ifdef CONFIG_VST_X11
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

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
#endif	// CONFIG_VST_X11


// All parameters update method.
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
			qtractorPlugin::Param *pParam = param.value();
			const float fValue
				= pVstEffect->getParameter(pVstEffect, pParam->index());
			if (pParam->value() != fValue) {
				pParam->setValue(fValue, bUpdate);
				updateFormDirtyCount();
			}
		}
	}
}


//----------------------------------------------------------------------------
// qtractorVstPlugin::Param -- VST plugin control input port instance.
//

// Constructors.
qtractorVstPlugin::Param::Param (
	qtractorVstPlugin *pVstPlugin, unsigned long iIndex )
	: qtractorPlugin::Param(pVstPlugin, iIndex)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVstPlugin::Param[%p] pVstPlugin=%p iIndex=%lu", this, pVstPlugin, iIndex);
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
			qtractorPlugin::Param::setValue(
				pVstEffect->getParameter(pVstEffect, iIndex), true);
	}

	setDefaultValue(qtractorPlugin::Param::value());

	// Initialize port value...
	// reset();
}


// Port range hints predicate methods.
bool qtractorVstPlugin::Param::isBoundedBelow (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
#endif
}

bool qtractorVstPlugin::Param::isBoundedAbove (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
#endif
}

bool qtractorVstPlugin::Param::isDefaultValue (void) const
{
	return true;
}

bool qtractorVstPlugin::Param::isLogarithmic (void) const
{
	return false;
}

bool qtractorVstPlugin::Param::isSampleRate (void) const
{
	return false;
}

bool qtractorVstPlugin::Param::isInteger (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntStep);
#endif
}

bool qtractorVstPlugin::Param::isToggled (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterIsSwitch);
#endif
}

bool qtractorVstPlugin::Param::isDisplay (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return true;
#endif
}


// Current display value.
QString qtractorVstPlugin::Param::display (void) const
{
	qtractorVstPluginType *pVstType = nullptr;
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
	return qtractorPlugin::Param::display();
}


//----------------------------------------------------------------------
// VST host callback file selection helpers.

#ifndef CONFIG_VESTIGE

static VstIntPtr qtractorVstPlugin_openFileSelector (
	AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVstPlugin *pVstPlugin = qtractorVstPlugin::findPlugin(effect);
	if (pVstPlugin == nullptr)
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
		if (pParentWidget)
			pParentWidget = pParentWidget->window();
		const QString& sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg((pVstPlugin->type())->name());
		const QString& sDirectory = pvfs->initialPath;
		const QString& sFilter = filters.join(";;");
		QFileDialog::Options options;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bDontUseNativeDialogs)
			options |= QFileDialog::DontUseNativeDialog;
		if (pvfs->command == kVstFileLoad) {
			sFilename = QFileDialog::getOpenFileName(
				pParentWidget, sTitle, sDirectory, sFilter, nullptr, options);
		} else {
			sFilename = QFileDialog::getSaveFileName(
				pParentWidget, sTitle, sDirectory, sFilter, nullptr, options);
		}
		if (!sFilename.isEmpty()) {
			if (pvfs->returnPath == nullptr) {
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
		if (pParentWidget)
			pParentWidget = pParentWidget->window();
		const QString& sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg((pVstPlugin->type())->name());
		const QFileDialog::Options options
			= QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog;
		const QString& sDirectory
			= QFileDialog::getExistingDirectory(
				pParentWidget, sTitle, pvfs->initialPath, options);
		if (!sDirectory.isEmpty()) {
			if (pvfs->returnPath == nullptr) {
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
	if (pVstPlugin == nullptr)
		return 0;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin_closeFileSelector(%p, %p) command=%d reserved=%d",
		effect, pvfs, int(pvfs->command), int(pvfs->reserved));
#endif

	if (pvfs->reserved == 1 && pvfs->returnPath) {
		delete [] pvfs->returnPath;
		pvfs->returnPath = nullptr;
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
	qtractorVstPlugin *pVstPlugin = nullptr;
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
		ret = (VstIntPtr) g_iVstShellCurrentId;
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
			const unsigned long iPlayHead = pSession->playHead();
			qtractorTimeScale::Cursor& cursor = pSession->timeScale()->cursor();
			qtractorTimeScale::Node *pNode = cursor.seekFrame(iPlayHead);
			s_vstTimeInfo.samplePos = double(iPlayHead);
			s_vstTimeInfo.sampleRate = double(pSession->sampleRate());
			s_vstTimeInfo.flags = 0;
			qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
			if (pSession->isPlaying() || (pAudioEngine && pAudioEngine->isFreewheel()))
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
			qtractorMidiManager *pMidiManager = nullptr;
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
		if (pVstPlugin) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
				if (pAudioEngine)
					ret = (VstIntPtr) pAudioEngine->sampleRate();
			}
		}
		break;

	case audioMasterGetBlockSize:
		VST_HC_DEBUG("audioMasterGetBlockSize");
		pVstPlugin = qtractorVstPlugin::findPlugin(effect);
		if (pVstPlugin) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
				if (pAudioEngine)
					ret = (VstIntPtr) pAudioEngine->bufferSize();
			}
		}
		break;

	case audioMasterGetInputLatency:
		VST_HC_DEBUG("audioMasterGetInputLatency");
		break;

	case audioMasterGetOutputLatency:
		VST_HC_DEBUG("audioMasterGetOutputLatency");
		break;

	case audioMasterGetCurrentProcessLevel:
	//	VST_HC_DEBUG("audioMasterGetCurrentProcessLevel");
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


//-----------------------------------------------------------------------------
// Structures for VST presets (fxb/fxp files)

// Constants copied/stringified from "vstfxstore.h"
//
#define cMagic				"CcnK"		// Root chunk identifier for Programs (fxp) and Banks (fxb).
#define fMagic				"FxCk"		// Regular Program (fxp) identifier.
#define bankMagic			"FxBk"		// Regular Bank (fxb) identifier.
#define chunkPresetMagic	"FPCh"		// Program (fxp) identifier for opaque chunk data.
#define chunkBankMagic		"FBCh"		// Bank (fxb) identifier for opaque chunk data.

#define cMagic_u			0x4b6e6343  // Big-endian version of 'CcnK'


// Common bank/program header structure (fxb/fxp files)
//
struct qtractorVstPreset::BaseHeader
{
	VstInt32 chunkMagic;		// 'CcnK'
	VstInt32 byteSize;			// size of this chunk, excl. magic + byteSize
	VstInt32 fxMagic;			// 'FxCk' (regular) or 'FPCh' (opaque chunk)
	VstInt32 version;			// format version (currently 1)
	VstInt32 fxID;				// fx unique ID
	VstInt32 fxVersion;			// fx version
};


// Program sub-header structure (fxb/fxp files)
//
struct qtractorVstPreset::ProgHeader
{
	VstInt32 numParams;			// number of parameters
	char prgName[28];			// program name (null-terminated ASCII string)
};


// Bank sub-header structure (fxb files)
//
struct qtractorVstPreset::BankHeader
{
	VstInt32 numPrograms;		// number of programs
	VstInt32 currentProgram;	// version 2: current program number
	char future[124];			// reserved, should be zero
};


// Common auxiliary chunk structure (chunked fxb/fxp files)
//
struct qtractorVstPreset::Chunk
{
	VstInt32 size;
	char *data;
};


// Endianess swappers.
//
static inline void fx_endian_swap ( VstInt32& v )
{
	static bool s_endian_swap = (*(VstInt32 *) cMagic == cMagic_u);

	if (s_endian_swap) {
		const unsigned int u = v;
		v = (u << 24) | ((u << 8) & 0xff0000) | ((u >> 8) & 0xff00) | (u >> 24);
	}
}

static inline void fx_endian_swap ( float& v )
{
	fx_endian_swap(*(VstInt32 *) &v);
}


static inline bool fx_is_magic ( VstInt32 v, const char *c )
{
#if 0
	VstInt32 u = *(VstInt32 *) c;
	fx_endian_swap(u);
	return (v == u);
#else
	return (v == *(VstInt32 *) c);
#endif
}


//----------------------------------------------------------------------
// class qtractorVstPreset -- VST preset file interface
//

// Constructor.
qtractorVstPreset::qtractorVstPreset ( qtractorVstPlugin *pVstPlugin )
	: m_pVstPlugin(pVstPlugin)
{
}


// Loader methods.
//
bool qtractorVstPreset::load_bank_progs ( QFile& file )
{
	BankHeader bank_header;
	const int nread_bank = sizeof(bank_header);
	if (file.read((char *) &bank_header, nread_bank) < nread_bank)
		return false;

	fx_endian_swap(bank_header.numPrograms);
	fx_endian_swap(bank_header.currentProgram);

	const int iNumPrograms = int(bank_header.numPrograms);
//	const int iCurrentProgram = int(bank_header.currentProgram);
	const int iCurrentProgram
		= m_pVstPlugin->vst_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);

	for (int iProgram = 0; iProgram < iNumPrograms; ++iProgram) {

		BaseHeader base_header;
		const int nread = sizeof(base_header);
		if (file.read((char *) &base_header, nread) < nread)
			return false;

	//	fx_endian_swap(base_header.chunkMagic);
		fx_endian_swap(base_header.byteSize);
	//	fxendian_swap(base_header.fxMagic);
		fx_endian_swap(base_header.version);
		fx_endian_swap(base_header.fxID);
		fx_endian_swap(base_header.fxVersion);

		if (!fx_is_magic(base_header.chunkMagic, cMagic))
			return false;

		for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i)
			m_pVstPlugin->vst_dispatch(i, effSetProgram, 0, iProgram, nullptr, 0.0f);

		if (fx_is_magic(base_header.fxMagic, fMagic)) {
			if (!load_prog_params(file))
				return false;
		}
		else
		if (fx_is_magic(base_header.fxMagic, chunkPresetMagic)) {
			if (!load_prog_chunk(file))
				return false;
		}
		else return false;
	}

	for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i)
		m_pVstPlugin->vst_dispatch(i, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);

	return true;
}


bool qtractorVstPreset::load_prog_params ( QFile& file )
{
	ProgHeader prog_header;
	const int nread = sizeof(prog_header);
	if (file.read((char *) &prog_header, nread) < nread)
		return false;

	fx_endian_swap(prog_header.numParams);

	for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i)
		m_pVstPlugin->vst_dispatch(i, effSetProgramName, 0, 0, (void *) prog_header.prgName, 0.0f);

	const int iNumParams = int(prog_header.numParams);
	if (iNumParams < 1)
		return false;

	const int nread_params = iNumParams * sizeof(float);
	float *params = new float [iNumParams];
	if (file.read((char *) params, nread_params) < nread_params)
		return false;

	for (int iParam = 0; iParam < iNumParams; ++iParam) {
		fx_endian_swap(params[iParam]);
		for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i) {
			AEffect *pVstEffect = m_pVstPlugin->vst_effect(i);
			if (pVstEffect)
				pVstEffect->setParameter(pVstEffect, iParam, params[iParam]);
		}
	}

	delete [] params;
	return true;
}


bool qtractorVstPreset::load_bank_chunk ( QFile& file )
{
	BankHeader bank_header;
	const int nread = sizeof(bank_header);
	if (file.read((char *) &bank_header, nread) < nread)
		return false;

	const int iCurrentProgram
		= m_pVstPlugin->vst_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);

	const bool bResult = load_chunk(file, 0);

	for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i)
		m_pVstPlugin->vst_dispatch(i, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);

	return bResult;
}


bool qtractorVstPreset::load_prog_chunk ( QFile& file )
{
	ProgHeader prog_header;
	const int nread = sizeof(prog_header);
	if (file.read((char *) &prog_header, nread) < nread)
		return false;

	return load_chunk(file, 1);
}


bool qtractorVstPreset::load_chunk ( QFile& file, int preset )
{
	Chunk chunk;
	const int nread = sizeof(chunk.size);
	if (file.read((char *) &chunk.size, nread) < nread)
		return false;

	fx_endian_swap(chunk.size);

	const int ndata = int(chunk.size);
	chunk.data = new char [ndata];
	if (file.read(chunk.data, ndata) < ndata) {
		delete [] chunk.data;
		return false;
	}

	for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i) {
		m_pVstPlugin->vst_dispatch(i, effSetChunk,
			preset, chunk.size,	(void *) chunk.data, 0.0f);
	}

	delete [] chunk.data;
	return true;
}


// File loader.
//
bool qtractorVstPreset::load ( const QString& sFilename )
{
	if (m_pVstPlugin == nullptr)
		return false;

	const QFileInfo fi(sFilename);
	const QString& sExt = fi.suffix().toLower();
	const bool bFxBank = (sExt == "fxb");
	const bool bFxProg = (sExt == "fxp");

	if (!bFxBank && !bFxProg)
		return false;
	if (!fi.exists())
		return false;

	QFile file(sFilename);

	if (!file.open(QIODevice::ReadOnly))
		return false;

	BaseHeader base_header;
	const int nread_base = sizeof(base_header);
	if (file.read((char *) &base_header, nread_base) < nread_base) {
		file.close();
		return false;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPreset::load(\"%s\")", sFilename.toUtf8().constData());
#endif

//	fx_endian_swap(base_header.chunkMagic);
	fx_endian_swap(base_header.byteSize);
//	fx_endian_swap(base_header.fxMagic);
	fx_endian_swap(base_header.version);
	fx_endian_swap(base_header.fxID);
	fx_endian_swap(base_header.fxVersion);

	bool bResult = false;

	if (!fx_is_magic(base_header.chunkMagic, cMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPresetload() header.chunkMagic is not \"%s\".", cMagic);
	#endif
	}
	else
	if (base_header.fxID != VstInt32(m_pVstPlugin->uniqueID())) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPreset::load() header.fxID != 0x%08lx.", m_pVstPlugin->uniqueID());
	#endif
	}
	else
	if (fx_is_magic(base_header.fxMagic, bankMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPreset::load() header.fxMagic is \"%s\" (regular fxb)", bankMagic);
	#endif
		bResult = load_bank_progs(file);
	}
	else
	if (fx_is_magic(base_header.fxMagic, chunkBankMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPreset::load() header.fxMagic is \"%s\" (chunked fxb)", chunkBankMagic);
	#endif
		bResult = load_bank_chunk(file);
	}
	else
	if (fx_is_magic(base_header.fxMagic, fMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPreset::load() header.fxMagic is \"%s\" (regular fxp)", fMagic);
	#endif
		bResult = load_prog_params(file);
	}
	else
	if (fx_is_magic(base_header.fxMagic, chunkPresetMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVstPreset::load() header.fxMagic is \"%s\" (chunked fxp)", chunkPresetMagic);
	#endif
		bResult = load_prog_chunk(file);
	}
	#ifdef CONFIG_DEBUG
	else qDebug("qtractorVstPreset::load() header.fxMagic not recognized.");
	#endif

	file.close();

	// HACK: Make sure all displayed parameter values are in sync.
	m_pVstPlugin->updateParamValues(false);

	return bResult;
}


// Saver methods.
//
bool qtractorVstPreset::save_bank_progs ( QFile& file )
{
	AEffect *pVstEffect = m_pVstPlugin->vst_effect(0);
	if (pVstEffect == nullptr)
		return false;

	const int iNumPrograms = pVstEffect->numPrograms;
	if (iNumPrograms < 1)
		return false;

	const int iCurrentProgram
		= m_pVstPlugin->vst_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);
	const int iVstVersion
		= m_pVstPlugin->vst_dispatch(0, effGetVstVersion, 0, 0, nullptr, 0.0f);

	BankHeader bank_header;
	::memset(&bank_header, 0, sizeof(bank_header));
	bank_header.numPrograms = iNumPrograms;
	bank_header.currentProgram = iCurrentProgram;

	fx_endian_swap(bank_header.numPrograms);
	fx_endian_swap(bank_header.currentProgram);

	file.write((char *) &bank_header, sizeof(bank_header));

	const bool bChunked
		= (m_pVstPlugin->type())->isConfigure();

	bool bResult = false;

	for (int iProgram = 0; iProgram < iNumPrograms; ++iProgram) {

		m_pVstPlugin->vst_dispatch(0, effSetProgram, 0, iProgram, nullptr, 0.0f);

		BaseHeader base_header;
		::memset(&base_header, 0, sizeof(base_header));
		base_header.chunkMagic = *(VstInt32 *) cMagic;
		base_header.byteSize = 0; // FIXME!
		base_header.fxMagic = *(VstInt32 *) (bChunked ? chunkPresetMagic : fMagic);
		base_header.version = 1;
		base_header.fxID = m_pVstPlugin->uniqueID();
		base_header.fxVersion = iVstVersion;

		// Estimate size of this section...
		base_header.byteSize = sizeof(base_header)
			- sizeof(base_header.chunkMagic)
			- sizeof(base_header.byteSize);

		Chunk chunk;
		if (bChunked) {
			get_chunk(chunk, 1);
			base_header.byteSize += sizeof(chunk.size) + chunk.size;
		} else {
			const int iNumParams  = pVstEffect->numParams;
			base_header.byteSize += sizeof(ProgHeader);
			base_header.byteSize += iNumParams * sizeof(float);
		}

	//	fx_endian_swap(base_header.chunkMagic);
		fx_endian_swap(base_header.byteSize);
	//	fx_endian_swap(base_header.fxMagic);
		fx_endian_swap(base_header.version);
		fx_endian_swap(base_header.fxID);
		fx_endian_swap(base_header.fxVersion);

		file.write((char *) &base_header, sizeof(base_header));

		if (bChunked) {
			bResult = save_prog_chunk(file, chunk);
			if (!bResult) break;
		} else {
			bResult = save_prog_params(file);
			if (!bResult) break;
		}
	}

	m_pVstPlugin->vst_dispatch(0, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);

	return bResult;
}


bool qtractorVstPreset::save_prog_params ( QFile& file )
{
	AEffect *pVstEffect = m_pVstPlugin->vst_effect(0);
	if (pVstEffect == nullptr)
		return false;

	const int iNumParams = pVstEffect->numParams;
	if (iNumParams < 1)
		return false;

	ProgHeader prog_header;
	::memset(&prog_header, 0, sizeof(prog_header));
	prog_header.numParams = iNumParams;

	m_pVstPlugin->vst_dispatch(0, effGetProgramName, 0, 0, (void *) prog_header.prgName, 0.0f);

	fx_endian_swap(prog_header.numParams);

	file.write((char *) &prog_header, sizeof(prog_header));

	float *params = new float [iNumParams];
	for (int iParam = 0; iParam < iNumParams; ++iParam) {
		params[iParam] = pVstEffect->getParameter(pVstEffect, iParam);
		fx_endian_swap(params[iParam]);
	}

	file.write((char *) params, iNumParams * sizeof(float));
	delete [] params;

	return true;
}


bool qtractorVstPreset::save_bank_chunk ( QFile& file, const Chunk& chunk )
{
	AEffect *pVstEffect = m_pVstPlugin->vst_effect(0);
	if (pVstEffect == nullptr)
		return false;

	const int iNumPrograms = pVstEffect->numPrograms;
	const int iCurrentProgram
		= m_pVstPlugin->vst_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);

	BankHeader bank_header;
	::memset(&bank_header, 0, sizeof(bank_header));
	bank_header.numPrograms = iNumPrograms;
	bank_header.currentProgram = iCurrentProgram;

	fx_endian_swap(bank_header.numPrograms);
	fx_endian_swap(bank_header.currentProgram);

	file.write((char *) &bank_header, sizeof(bank_header));

	return save_chunk(file, chunk);
}


bool qtractorVstPreset::save_prog_chunk ( QFile& file, const Chunk& chunk )
{
	AEffect *pVstEffect = m_pVstPlugin->vst_effect(0);
	if (pVstEffect == nullptr)
		return false;

	const int iNumParams = pVstEffect->numParams;

	ProgHeader prog_header;
	::memset(&prog_header, 0, sizeof(prog_header));
	prog_header.numParams = iNumParams;

	m_pVstPlugin->vst_dispatch(0, effGetProgramName,
		0, 0, (void *) prog_header.prgName, 0.0f);

	fx_endian_swap(prog_header.numParams);

	file.write((char *) &prog_header, sizeof(prog_header));

	return save_chunk(file, chunk);
}


bool qtractorVstPreset::save_chunk ( QFile& file, const Chunk& chunk )
{
	const int ndata = int(chunk.size);

	VstInt32 chunk_size = ndata;
	fx_endian_swap(chunk_size);

	file.write((char *) &chunk_size, sizeof(chunk_size));
	file.write((char *)  chunk.data, ndata);

	return true;
}


bool qtractorVstPreset::get_chunk ( Chunk& chunk, int preset )
{
	chunk.data = nullptr;
	chunk.size = m_pVstPlugin->vst_dispatch(0,
		effGetChunk, preset, 0, (void *) &chunk.data, 0.0f);
	return (chunk.size > 0 && chunk.data != nullptr);
}


// File saver.
//
bool qtractorVstPreset::save ( const QString& sFilename )
{
	if (m_pVstPlugin == nullptr)
		return false;

	AEffect *pVstEffect = m_pVstPlugin->vst_effect(0);
	if (pVstEffect == nullptr)
		return false;

	const QFileInfo fi(sFilename);
	const QString& sExt = fi.suffix().toLower();
	const bool bFxBank = (sExt == "fxb");
	const bool bFxProg = (sExt == "fxp");

	if (!bFxBank && !bFxProg)
		return false;

	QFile file(sFilename);

	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPreset::save(\"%s\")", sFilename.toUtf8().constData());
#endif

	const bool bChunked
		= (m_pVstPlugin->type())->isConfigure();
	const int iVstVersion
		= m_pVstPlugin->vst_dispatch(0, effGetVstVersion, 0, 0, nullptr, 0.0f);

	BaseHeader base_header;
	::memset(&base_header, 0, sizeof(base_header));
	base_header.chunkMagic = *(VstInt32 *) cMagic;
	base_header.byteSize = 0;	// FIXME: see below...
	base_header.fxMagic = 0;	//
	base_header.version = 1;
	base_header.fxID = m_pVstPlugin->uniqueID();
	base_header.fxVersion = iVstVersion;

	// Estimate size of this section...
	base_header.byteSize = sizeof(base_header)
		- sizeof(base_header.chunkMagic)
		- sizeof(base_header.byteSize);

	Chunk chunk;
	if (bFxBank) {
		if (bChunked) {
			get_chunk(chunk, 0);
			base_header.byteSize += sizeof(chunk.size) + chunk.size;
			base_header.fxMagic = *(VstInt32 *) chunkBankMagic;
		} else {
			const int iNumParams  = pVstEffect->numParams;
			base_header.byteSize += pVstEffect->numPrograms
				* (sizeof(ProgHeader) + iNumParams * sizeof(float));
			base_header.fxMagic = *(VstInt32 *) bankMagic;
		}
	} else {
		char szName[24]; ::memset(szName, 0, sizeof(szName));
		::strncpy(szName, fi.baseName().toUtf8().constData(), sizeof(szName) - 1);
		for (unsigned short i = 0; i < m_pVstPlugin->instances(); ++i)
			m_pVstPlugin->vst_dispatch(i, effSetProgramName, 0, 0, (void *) szName, 0.0f);
		if (bChunked) {
			get_chunk(chunk, 1);
			base_header.byteSize += sizeof(chunk.size) + chunk.size;
			base_header.fxMagic = *(VstInt32 *) chunkPresetMagic;
		} else {
			const int iNumParams  = pVstEffect->numParams;
			base_header.byteSize += sizeof(ProgHeader);
			base_header.byteSize += iNumParams * sizeof(float);
			base_header.fxMagic = *(VstInt32 *) fMagic;
		}
	}

//	fx_endian_swap(base_header.chunkMagic);
	fx_endian_swap(base_header.byteSize);
//	fx_endian_swap(base_header.fxMagic);
	fx_endian_swap(base_header.version);
	fx_endian_swap(base_header.fxID);
	fx_endian_swap(base_header.fxVersion);

	file.write((char *) &base_header, sizeof(base_header));

	bool bResult = false;
	if (bFxBank) {
		if (bChunked)
			bResult = save_bank_chunk(file, chunk);
		else
			bResult = save_bank_progs(file);
	} else {
		if (bChunked)
			bResult = save_prog_chunk(file, chunk);
		else
			bResult = save_prog_params(file);
	}

	file.close();
	return bResult;
}


#endif	// CONFIG_VST

// end of qtractorVstPlugin.cpp
