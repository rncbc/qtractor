// qtractorVst2Plugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2023, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifdef CONFIG_VST2

#include "qtractorVst2Plugin.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"

#include "qtractorOptions.h"

#if 0//QTRACTOR_VST2_EDITOR_TOOL
#include "qtractorMainForm.h"
#endif

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>

#include <QRegularExpression>

#include <QVBoxLayout>
#include <QIcon>

#if QT_VERSION < QT_VERSION_CHECK(4, 5, 0)
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif


#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST2_X11
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
typedef void (*XEventProc)(XEvent *);
#endif	// CONFIG_VST2_X11
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

static VstIntPtr VSTCALLBACK qtractorVst2Plugin_HostCallback (AEffect* effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt);

// Specific extended flags that saves us
// from calling canDo() in audio callbacks.
enum qtractorVst2PluginFlagsEx
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
const int effGetProgramNameIndexed = 29;
const int effFlagsProgramChunks = 32;
#endif


//---------------------------------------------------------------------
// qtractorVst2Plugin::EditorWidget - Helpers for own editor widget.

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST2_X11

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

#endif // CONFIG_VST2_X11
#endif


//----------------------------------------------------------------------------
// qtractorVst2Plugin::EditorWidget -- Plugin editor wrapper widget.
//

// Dynamic singleton list of VST2 editors.
static QList<qtractorVst2Plugin::EditorWidget *> g_vst2Editors;

class qtractorVst2Plugin::EditorWidget : public QWidget
{
public:

	// Constructor.
	EditorWidget(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags())
		: QWidget(pParent, wflags),
	#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#ifdef CONFIG_VST2_X11
		m_pDisplay(QX11Info::display()),
		m_wVst2Editor(0),
		m_pVst2EventProc(nullptr),
		m_bButtonPress(false),
	#endif	// CONFIG_VST2_X11
	#else
		m_pWindow(nullptr),
	#endif
		m_pVst2Plugin(nullptr)
		{ QWidget::setAttribute(Qt::WA_QuitOnClose, false); }

	// Destructor.
	~EditorWidget() { close(); }

	// Specialized editor methods.
	void open(qtractorVst2Plugin *pVst2Plugin)
	{
		m_pVst2Plugin = pVst2Plugin;
		
		// Start the proper (child) editor...
		long  value = 0;
		void *ptr = nullptr;
	#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#ifdef CONFIG_VST2_X11
		value = (long) m_pDisplay;
		ptr = (void *) QWidget::winId();
	#endif // CONFIG_VST2_X11
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
		m_pVst2Plugin->vst2_dispatch(0, effEditOpen, 0, value, ptr, 0.0f);

		// Make it the right size
		struct ERect {
			short top;
			short left;
			short bottom;
			short right;
		} *pRect;

		if (m_pVst2Plugin->vst2_dispatch(0, effEditGetRect, 0, 0, &pRect, 0.0f)) {
			const int w = pRect->right - pRect->left;
			const int h = pRect->bottom - pRect->top;
			if (w > 0 && h > 0)
				QWidget::setFixedSize(w, h);
		}

	#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
	#ifdef CONFIG_VST2_X11
		m_wVst2Editor = getXChildWindow(m_pDisplay, (Window) QWidget::winId());
		if (m_wVst2Editor)
			m_pVst2EventProc = getXEventProc(m_pDisplay, m_wVst2Editor);
	#endif	// CONFIG_VST2_X11
	#endif

		g_vst2Editors.append(this);
	}

	// Close the editor widget.
	void close()
	{
		QWidget::close();

		if (m_pVst2Plugin) {
			m_pVst2Plugin->vst2_dispatch(0, effEditClose, 0, 0, nullptr, 0.0f);
			m_pVst2Plugin = nullptr;
		}

		const int iIndex = g_vst2Editors.indexOf(this);
		if (iIndex >= 0)
			g_vst2Editors.removeAt(iIndex);

	#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
		if (m_pWindow) {
			m_pWindow->destroy();
			delete m_pWindow;
		}
	#endif
	}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST2_X11
	// Local X11 event filter.
	bool x11EventFilter(XEvent *pEvent)
	{
		if (m_pVst2EventProc && pEvent->xany.window == m_wVst2Editor) {
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
			(*m_pVst2EventProc)(pEvent);
			return true;
		} else {
			return false;
		}
	}
#endif	// CONFIG_VST2_X11
#endif

	qtractorVst2Plugin *plugin() const
		{ return m_pVst2Plugin; }

protected:

	// Visibility event handlers.
	void showEvent(QShowEvent *pShowEvent)
	{
		QWidget::showEvent(pShowEvent);

		if (m_pVst2Plugin)
			m_pVst2Plugin->toggleFormEditor(true);
	}

	void closeEvent(QCloseEvent *pCloseEvent)
	{
		if (m_pVst2Plugin)
			m_pVst2Plugin->toggleFormEditor(false);

		QWidget::closeEvent(pCloseEvent);

		if (m_pVst2Plugin)
			m_pVst2Plugin->closeEditor();
	}

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST2_X11
	void moveEvent(QMoveEvent *pMoveEvent)
	{
		QWidget::moveEvent(pMoveEvent);
		if (m_wVst2Editor) {
			XMoveWindow(m_pDisplay, m_wVst2Editor, 0, 0);
		//	QWidget::update();
		}
	}
#endif	// CONFIG_VST2_X11
#endif

private:

	// Instance variables...
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST2_X11
	Display   *m_pDisplay;
	Window     m_wVst2Editor;
	XEventProc m_pVst2EventProc;
	bool       m_bButtonPress;
#endif	// CONFIG_VST2_X11
#else
	QWindow   *m_pWindow;
#endif

	qtractorVst2Plugin *m_pVst2Plugin;
};


//----------------------------------------------------------------------------
// qtractorVst2PluginType::Effect -- VST2 AEffect instance.
//

class qtractorVst2PluginType::Effect
{
public:

	// Constructor.
	Effect(AEffect *pVst2Effect = nullptr) : m_pVst2Effect(pVst2Effect) {}

	// Specific methods.
	bool open(qtractorPluginFile *pFile, unsigned long iIndex);
	void close();

	// Specific accessors.
	AEffect *vst2_effect() const { return m_pVst2Effect; }

	// VST2 host dispatcher.
	int vst2_dispatch(
		long opcode, long index, long value, void *ptr, float opt) const;

private:

	// VST2 descriptor itself.
	AEffect *m_pVst2Effect;
};


// Current working VST2 Shell identifier.
static int g_iVst2ShellCurrentId = 0;


// Specific methods.
bool qtractorVst2PluginType::Effect::open (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Do we have an effect descriptor already?
	if (m_pVst2Effect == nullptr)
		m_pVst2Effect = qtractorVst2PluginType::vst2_effect(pFile);
	if (m_pVst2Effect == nullptr)
		return false;

	// Check whether it's a VST2 Shell...
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
			m_pVst2Effect = nullptr;
			return false;
		}
		// Make it known...
		g_iVst2ShellCurrentId = id;
		// Re-allocate the thing all over again...
		m_pVst2Effect = qtractorVst2PluginType::vst2_effect(pFile);
		// Not needed anymore, hopefully...
		g_iVst2ShellCurrentId = 0;
		// Don't go further if failed...
		if (m_pVst2Effect == nullptr)
			return false;
	#ifdef CONFIG_DEBUG//_0
		qDebug("AEffect[%p]::vst2_shell(%lu) id=0x%x name=\"%s\"",
			m_pVst2Effect, i, id, buf);
	#endif
	}
	else
	// Not a VST2 Shell plugin...
	if (iIndex > 0) {
		m_pVst2Effect = nullptr;
		return false;
	}

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::open(%p, %lu)", m_pVst2Effect, pFile, iIndex);
#endif

	vst2_dispatch(effOpen, 0, 0, nullptr, 0.0f);
//	vst2_dispatch(effIdentify, 0, 0, nullptr, 0);
//	vst2_dispatch(effMainsChanged, 0, 0, nullptr, 0.0f);

	return true;
}


void qtractorVst2PluginType::Effect::close (void)
{
	if (m_pVst2Effect == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::close()", m_pVst2Effect);
#endif

//	vst2_dispatch(effMainsChanged, 0, 0, nullptr, 0.0f);
	vst2_dispatch(effClose, 0, 0, nullptr, 0.0f);

	m_pVst2Effect = nullptr;
}


// VST2 host dispatcher.
int qtractorVst2PluginType::Effect::vst2_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pVst2Effect == nullptr)
		return 0;

#ifdef CONFIG_DEBUG_0
	qDebug("AEffect[%p]::vst2_dispatch(%ld, %ld, %ld, %p, %g)",
		m_pVst2Effect, opcode, index, value, ptr, opt);
#endif

	return m_pVst2Effect->dispatcher(m_pVst2Effect, opcode, index, value, ptr, opt);
}


//----------------------------------------------------------------------------
// qtractorVst2PluginType -- VST2 plugin type instance.
//

// Derived methods.
bool qtractorVst2PluginType::open (void)
{
	// Do we have an effect descriptor already?
	if (m_pEffect == nullptr)
		m_pEffect = new Effect();
	if (!m_pEffect->open(file(), index()))
		return false;

	AEffect *pVst2Effect = m_pEffect->vst2_effect();
	if (pVst2Effect == nullptr)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2PluginType[%p]::open() filename=\"%s\" index=%lu",
		this, filename().toUtf8().constData(), index());
#endif

	// Retrieve plugin type names.
	char szName[256];
	::memset(szName, 0, sizeof(szName));
	vst2_dispatch(effGetEffectName, 0, 0, (void *) szName, 0.0f);
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
	m_iUniqueID = pVst2Effect->uniqueID;
#endif

	// Specific inquiries...
	m_iFlagsEx = 0;
//	if (vst2_canDo("sendVstEvents"))       m_iFlagsEx |= effFlagsExCanSendVstEvents;
	if (vst2_canDo("sendVstMidiEvent"))    m_iFlagsEx |= effFlagsExCanSendVstMidiEvents;
//	if (vst2_canDo("sendVstTimeInfo"))     m_iFlagsEx |= effFlagsExCanSendVstTimeInfo;
//	if (vst2_canDo("receiveVstEvents"))    m_iFlagsEx |= effFlagsExCanReceiveVstEvents;
	if (vst2_canDo("receiveVstMidiEvent")) m_iFlagsEx |= effFlagsExCanReceiveVstMidiEvents;
//	if (vst2_canDo("receiveVstTimeInfo"))  m_iFlagsEx |= effFlagsExCanReceiveVstTimeInfo;
//	if (vst2_canDo("offline"))             m_iFlagsEx |= effFlagsExCanProcessOffline;
//	if (vst2_canDo("plugAsChannelInsert")) m_iFlagsEx |= effFlagsExCanUseAsInsert;
//	if (vst2_canDo("plugAsSend"))          m_iFlagsEx |= effFlagsExCanUseAsSend;
//	if (vst2_canDo("mixDryWet"))           m_iFlagsEx |= effFlagsExCanMixDryWet;
//	if (vst2_canDo("midiProgramNames"))    m_iFlagsEx |= effFlagsExCanMidiProgramNames;

	// Compute and cache port counts...
	m_iControlIns  = pVst2Effect->numParams;
	m_iControlOuts = 0;
	m_iAudioIns    = pVst2Effect->numInputs;
	m_iAudioOuts   = pVst2Effect->numOutputs;
	m_iMidiIns     = ((m_iFlagsEx & effFlagsExCanReceiveVstMidiEvents)
		|| (pVst2Effect->flags & effFlagsIsSynth) ? 1 : 0);
	m_iMidiOuts    = ((m_iFlagsEx & effFlagsExCanSendVstMidiEvents) ? 1 : 0);

	// Cache flags.
	m_bRealtime  = true;
	m_bConfigure = (pVst2Effect->flags & effFlagsProgramChunks);
	m_bEditor    = (pVst2Effect->flags & effFlagsHasEditor);

	// HACK: Some native VST2 plugins with a GUI editor
	// need to skip explicit shared library unloading,
	// on close, in order to avoid mysterious crashes
	// later on session and/or application exit.
	if (m_bEditor) file()->setAutoUnload(false);

	return true;
}


void qtractorVst2PluginType::close (void)
{
	if (m_pEffect == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2PluginType[%p]::close()", this);
#endif

	m_pEffect->close();

	delete m_pEffect;
	m_pEffect = nullptr;
}


// Factory method (static)
qtractorVst2PluginType *qtractorVst2PluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Sanity check...
	if (pFile == nullptr)
		return nullptr;

	// Retrieve effect instance if any...
	AEffect *pVst2Effect = vst2_effect(pFile);
	if (pVst2Effect == nullptr)
		return nullptr;

	// Yep, most probably its a valid plugin effect...
	return new qtractorVst2PluginType(pFile, iIndex, new Effect(pVst2Effect));
}


// Effect instance method (static)
AEffect *qtractorVst2PluginType::vst2_effect ( qtractorPluginFile *pFile )
{
	VST_GetPluginInstance pfnGetPluginInstance
		= (VST_GetPluginInstance) pFile->resolve("VSTPluginMain");
	if (pfnGetPluginInstance == nullptr)
		pfnGetPluginInstance = (VST_GetPluginInstance) pFile->resolve("main");
	if (pfnGetPluginInstance == nullptr)
		return nullptr;

	// Does the VST2 plugin instantiate OK?
	AEffect *pVst2Effect
		= (*pfnGetPluginInstance)(qtractorVst2Plugin_HostCallback);
	if (pVst2Effect == nullptr)
		return nullptr;
	if (pVst2Effect->magic != kEffectMagic)
		return nullptr;

	return pVst2Effect;
}


// VST2 host dispatcher.
int qtractorVst2PluginType::vst2_dispatch (
	long opcode, long index, long value, void *ptr, float opt ) const
{
	if (m_pEffect == nullptr)
		return 0;

	return m_pEffect->vst2_dispatch(opcode, index, value, ptr, opt);
}


// VST2 flag inquirer.
bool qtractorVst2PluginType::vst2_canDo ( const char *pszCanDo ) const
{
	if (m_pEffect == nullptr)
		return false;

	return (m_pEffect->vst2_dispatch(effCanDo, 0, 0, (void *) pszCanDo, 0.0f) > 0);
}


// Instance cached-deferred accessors.
const QString& qtractorVst2PluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		char szTemp[256];
		::memset(szTemp, 0, sizeof(szTemp));
		vst2_dispatch(effGetProductString, 0, 0, (void *) szTemp, 0.0f);
		if (szTemp[0]) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Product: ");
			m_sAboutText += QString::fromLocal8Bit(szTemp);
		}
		::memset(szTemp, 0, sizeof(szTemp));
		vst2_dispatch(effGetVendorString, 0, 0, (void *) szTemp, 0.0f);
		if (szTemp[0]) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Vendor: ");
			m_sAboutText += QString::fromLocal8Bit(szTemp);
		}
		const int iVersion
			= vst2_dispatch(effGetVendorVersion, 0, 0, nullptr, 0.0f);
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
// qtractorVst2Plugin -- VST2 plugin instance.
//

// Dynamic singleton list of VST2 plugins.
static QHash<AEffect *, qtractorVst2Plugin *> g_vst2Plugins;

// Constructors.
qtractorVst2Plugin::qtractorVst2Plugin (
	qtractorPluginList *pList, qtractorVst2PluginType *pVst2Type )
	: qtractorPlugin(pList, pVst2Type), m_ppEffects(nullptr),
		m_ppIBuffer(nullptr), m_ppOBuffer(nullptr),
		m_pfIDummy(nullptr), m_pfODummy(nullptr),
		m_pEditorWidget(nullptr), m_bEditorClosed(false)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin[%p] filename=\"%s\" index=%lu typeHint=%d",
		this, type()->filename().toUtf8().constData(),
		type()->index(), int(type()->typeHint()));
#endif

	// Allocate I/O audio buffer pointers.
	const unsigned short iAudioIns  = pVst2Type->audioIns();
	const unsigned short iAudioOuts = pVst2Type->audioOuts();
	if (iAudioIns > 0)
		m_ppIBuffer = new float * [iAudioIns];
	if (iAudioOuts > 0)
		m_ppOBuffer = new float * [iAudioOuts];

	// Create all existing parameters...
	const unsigned short iParamCount = pVst2Type->controlIns();
	for (unsigned short i = 0; i < iParamCount; ++i)
		addParam(new Param(this, i));

	// Instantiate each instance properly...
	setChannels(channels());
}


// Destructor.
qtractorVst2Plugin::~qtractorVst2Plugin (void)
{
	// Cleanup all plugin instances...
	cleanup();	// setChannels(0);

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
void qtractorVst2Plugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorVst2PluginType *pVst2Type
		= static_cast<qtractorVst2PluginType *> (type());
	if (pVst2Type == nullptr)
		return;

	// Estimate the (new) number of instances...
	const unsigned short iOldInstances = instances();
	unsigned short iInstances = 0;
	if (iChannels > 0) {
		iInstances = pVst2Type->instances(iChannels, list()->isMidi());
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
		qtractorVst2PluginType::Effect *pEffect;
		for (unsigned short i = 1; i < iOldInstances; ++i) {
			pEffect = m_ppEffects[i];
			g_vst2Plugins.remove(pEffect->vst2_effect());
			pEffect->close();
			delete pEffect;
		}
		pEffect = m_ppEffects[0];
		g_vst2Plugins.remove(pEffect->vst2_effect());
		delete [] m_ppEffects;
		m_ppEffects = nullptr;
	}

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setChannelsActivated(iChannels, bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	const unsigned int iSampleRate = pAudioEngine->sampleRate();
	const unsigned int iBufferSizeEx = pAudioEngine->bufferSizeEx();

	// Allocate new instances...
	m_ppEffects = new qtractorVst2PluginType::Effect * [iInstances];
	m_ppEffects[0] = pVst2Type->effect();
	g_vst2Plugins.insert(m_ppEffects[0]->vst2_effect(), this);
	for (unsigned short i = 1; i < iInstances; ++i) {
		m_ppEffects[i] = new qtractorVst2PluginType::Effect();
		m_ppEffects[i]->open(pVst2Type->file(), pVst2Type->index());
		g_vst2Plugins.insert(m_ppEffects[i]->vst2_effect(), this);
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
		qtractorVst2PluginType::Effect *pEffect = m_ppEffects[i];
	//	pEffect->vst2_dispatch(effOpen, 0, 0, nullptr, 0.0f);
		pEffect->vst2_dispatch(effSetSampleRate, 0, 0, nullptr, float(iSampleRate));
		pEffect->vst2_dispatch(effSetBlockSize,  0, iBufferSizeEx, nullptr, 0.0f);
	//	pEffect->vst2_dispatch(effSetProgram, 0, 0, nullptr, 0.0f);
	#if 0 // !VST_FORCE_DEPRECATED
		unsigned short j;
		for (j = 0; j < iAudioIns; ++j)
			pEffect->vst2_dispatch(effConnectInput, j, 1, nullptr, 0.0f);
		for (j = 0; j < iAudioOuts; ++j)
			pEffect->vst2_dispatch(effConnectOutput, j, 1, nullptr, 0.0f);
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
void qtractorVst2Plugin::activate (void)
{
	for (unsigned short i = 0; i < instances(); ++i) {
		vst2_dispatch(i, effMainsChanged, 0, 1, nullptr, 0.0f);
	#ifndef CONFIG_VESTIGE
		vst2_dispatch(i, effStartProcess, 0, 0, nullptr, 0.0f);
	#endif
	}
}


// Do the actual deactivation.
void qtractorVst2Plugin::deactivate (void)
{
	for (unsigned short i = 0; i < instances(); ++i) {
	#ifndef CONFIG_VESTIGE
		vst2_dispatch(i, effStopProcess, 0, 0, nullptr, 0.0f);
	#endif
		vst2_dispatch(i, effMainsChanged, 0, 0, nullptr, 0.0f);
	}
}


// The main plugin processing procedure.
void qtractorVst2Plugin::process (
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
		AEffect *pVst2Effect = vst2_effect(i);
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
			pVst2Effect->dispatcher(pVst2Effect,
				effProcessEvents, 0, 0, pMidiManager->vst2_events_in(), 0.0f);
		}
		// Make it run audio...
		if (pVst2Effect->flags & effFlagsCanReplacing) {
			pVst2Effect->processReplacing(
				pVst2Effect, m_ppIBuffer, m_ppOBuffer, nframes);
		}
	#if 0 // !VST_FORCE_DEPRECATED
		else {
			pVst2Effect->process(
				pVst2Effect, m_ppIBuffer, m_ppOBuffer, nframes);
		}
	#endif
		// Wrap dangling output channels?...
		for (j = iOChannel; j < iChannels; ++j)
			::memset(ppOBuffer[j], 0, nframes * sizeof(float));
	}

	if (pMidiManager && iMidiOuts > 0)
		pMidiManager->vst2_events_swap();
}


// Parameter update method.
void qtractorVst2Plugin::updateParam (
	qtractorPlugin::Param *pParam, float fValue, bool /*bUpdate*/ )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVst2Plugin[%p]::updateParam(%lu, %g, %d)",
		this, pParam->index(), fValue, int(bUpdate));
#endif

	// Maybe we're not pretty instantiated yet...
	for (unsigned short i = 0; i < instances(); ++i) {
		AEffect *pVst2Effect = vst2_effect(i);
		if (pVst2Effect)
			pVst2Effect->setParameter(pVst2Effect, pParam->index(), fValue);
	}
}


// Bank/program selector.
void qtractorVst2Plugin::selectProgram ( int iBank, int iProg )
{
	if (iBank < 0 || iProg < 0)
		return;

	// HACK: We don't change program-preset when
	// we're supposed to be multi-timbral...
	if (list()->isMidiBus())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	int iIndex = 0;
	if (iBank >= 0 && iProg >= 0)
		iIndex = (iBank << 7) + iProg;

	for (unsigned short i = 0; i < instances(); ++i)
		vst2_dispatch(i, effSetProgram, 0, iIndex, nullptr, 0.0f);

	// Reset parameters default value...
	AEffect *pVst2Effect = vst2_effect(0);
	if (pVst2Effect) {
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPlugin::Param *pParam = param.value();
			float *pfValue = pParam->subject()->data();
			*pfValue = pVst2Effect->getParameter(pVst2Effect, pParam->index());
			pParam->setDefaultValue(*pfValue);
		}
	}
}


// Provisional program/patch accessor.
bool qtractorVst2Plugin::getProgram ( int iIndex, Program& program ) const
{
	// Iteration sanity check...
	AEffect *pVst2Effect = vst2_effect(0);
	if (pVst2Effect == nullptr)
		return false;
	if (iIndex < 0 || iIndex >= pVst2Effect->numPrograms)
		return false;

	char szName[256];
	::memset(szName, 0, sizeof(szName));
	if (vst2_dispatch(0, effGetProgramNameIndexed, iIndex, 0, (void *) szName, 0.0f) == 0) {
	#if 0//QTRACTOR_VST2_GET_PROGRAM_OLD
		const int iOldIndex = vst2_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);
		vst2_dispatch(0, effSetProgram, 0, iIndex, nullptr, 0.0f);
		vst2_dispatch(0, effGetProgramName, 0, 0, (void *) szName, 0.0f);
		vst2_dispatch(0, effSetProgram, 0, iOldIndex, nullptr, 0.0f);
	#else
		::snprintf(szName, sizeof(szName) - 1, "Program %d", iIndex + 1);
	#endif
	}

	// Map this to that...
	program.bank = 0;
	program.prog = iIndex;
	program.name = QString::fromLocal8Bit(szName);

	return true;
}


// Configuration (CLOB) stuff.
void qtractorVst2Plugin::configure (
	const QString& sKey, const QString& sValue )
{
	if (sKey == "chunk") {
		// Load the BLOB (base64 encoded)...
		const QByteArray data
			= qUncompress(QByteArray::fromBase64(sValue.toLatin1()));
		const char *pData = data.constData();
		const int iData = data.size();
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst2Plugin[%p]::configure() chunk.size=%d", this, iData);
	#endif
		for (unsigned short i = 0; i < instances(); ++i)
			vst2_dispatch(i, effSetChunk, 0, iData, (void *) pData, 0.0f);
	}
}


// Plugin configuration/state snapshot.
void qtractorVst2Plugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVst2Plugin[%p]::freezeConfigs()", this);
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

	clearConfigs();

	AEffect *pVst2Effect = vst2_effect(0);
	if (pVst2Effect == nullptr)
		return;

	// Save plugin state into chunk configuration...
	char *pData = nullptr;
	const int iData
		= vst2_dispatch(0, effGetChunk, 0, 0, (void *) &pData, 0.0f);
	if (iData < 1 || pData == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin[%p]::freezeConfigs() chunk.size=%d", this, iData);
#endif

	// Set special plugin configuration item (base64 encoded)...
	QByteArray cdata = qCompress(QByteArray(pData, iData)).toBase64();
	for (int i = cdata.size() - (cdata.size() % 72); i >= 0; i -= 72)
		cdata.insert(i, "\n       "); // Indentation.
	setConfig("chunk", cdata.constData());
}


void qtractorVst2Plugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin[%p]::releaseConfigs()", this);
#endif

	qtractorPlugin::clearConfigs();
}


// Plugin current latency (in frames);
unsigned long qtractorVst2Plugin::latency (void) const
{
	AEffect *pVst2Effect = vst2_effect(0);
	if (pVst2Effect == nullptr)
		return 0;

#ifdef CONFIG_VESTIGE
	const VstInt32 *pInitialDelay
		= (VstInt32 *) &(pVst2Effect->empty3[0]);
	return *pInitialDelay;
#else
	return pVst2Effect->initialDelay;
#endif
}


// Plugin preset i/o (configuration from/to (fxp/fxb files).
bool qtractorVst2Plugin::loadPresetFile ( const QString& sFilename )
{
	bool bResult = false;

	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "fxp" || sExt == "fxb") {
		bResult = qtractorVst2Preset(this).load(sFilename);
	} else {
		const int iCurrentProgram
			= vst2_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);
		bResult = qtractorPlugin::loadPresetFile(sFilename);
		for (unsigned short i = 0; i < instances(); ++i)
			vst2_dispatch(i, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);
	}

	return bResult;
}

bool qtractorVst2Plugin::savePresetFile ( const QString& sFilename )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "fxp" || sExt == "fxb")
		return qtractorVst2Preset(this).save(sFilename);
	else
		return qtractorPlugin::savePresetFile(sFilename);
}


// Specific accessors.
AEffect *qtractorVst2Plugin::vst2_effect ( unsigned short iInstance ) const
{
	if (m_ppEffects == nullptr)
		return nullptr;

	return m_ppEffects[iInstance]->vst2_effect();
}


// VST2 host dispatcher.
int qtractorVst2Plugin::vst2_dispatch( unsigned short iInstance,
	long opcode, long index, long value, void *ptr, float opt) const
{
	if (m_ppEffects == nullptr)
		return 0;

	return m_ppEffects[iInstance]->vst2_dispatch(opcode, index, value, ptr, opt);
}


// Open editor.
void qtractorVst2Plugin::openEditor ( QWidget *pParent )
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
	qDebug("qtractorVst2Plugin[%p]::openEditor(%p)", this, pParent);
#endif

	// Make sure it's not closed...
	m_bEditorClosed = false;

	// What style do we create tool childs?
	Qt::WindowFlags wflags = Qt::Window;
#if 0//QTRACTOR_VST2_EDITOR_TOOL
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
void qtractorVst2Plugin::closeEditor (void)
{
	if (m_bEditorClosed)
		return;

	m_bEditorClosed = true;

	if (m_pEditorWidget == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

	// Close the parent widget, if any.
	delete m_pEditorWidget;
	m_pEditorWidget = nullptr;
}


// Idle editor.
void qtractorVst2Plugin::idleEditor (void)
{
	if (m_pEditorWidget == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVst2Plugin[%p]::idleEditor()", this);
#endif

	vst2_dispatch(0, effEditIdle, 0, 0, nullptr, 0.0f);

	m_pEditorWidget->update();
}


// GUI editor visibility state.
void qtractorVst2Plugin::setEditorVisible ( bool bVisible )
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


bool qtractorVst2Plugin::isEditorVisible (void) const
{
	return (m_pEditorWidget ? m_pEditorWidget->isVisible() : false);
}


// Update editor widget caption.
void qtractorVst2Plugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	if (m_pEditorWidget) {
		m_pEditorWidget->setWindowTitle(sTitle);
		m_pEditorWidget->setWindowIcon(QIcon(":/images/qtractorPlugin.svg"));
	}
}


// Idle editor (static).
void qtractorVst2Plugin::idleEditorAll (void)
{
	QListIterator<EditorWidget *> iter(g_vst2Editors);
	while (iter.hasNext()) {
		qtractorVst2Plugin *pVst2Plugin = iter.next()->plugin();
		if (pVst2Plugin)
			pVst2Plugin->idleEditor();
	}
}


// Our own editor widget accessor.
QWidget *qtractorVst2Plugin::editorWidget (void) const
{
	return static_cast<QWidget *> (m_pEditorWidget);
}


// Our own editor widget size accessor.
void qtractorVst2Plugin::resizeEditor ( int w, int h )
{
	if (m_pEditorWidget && w > 0 && h > 0)
		m_pEditorWidget->setFixedSize(w, h);
}


// Global VST2 plugin lookup.
qtractorVst2Plugin *qtractorVst2Plugin::findPlugin ( AEffect *pVst2Effect )
{
	return g_vst2Plugins.value(pVst2Effect, nullptr);
}


#ifdef CONFIG_VST2_X11
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

// Global X11 event filter.
bool qtractorVst2Plugin::x11EventFilter ( void *pvEvent )
{
	XEvent *pEvent = (XEvent *) pvEvent;

	QListIterator<EditorWidget *> iter(g_vst2Editors);
	while (iter.hasNext()) {
		if (iter.next()->x11EventFilter(pEvent))
			return true;
	}

	return false;
}

#endif
#endif	// CONFIG_VST2_X11


// All parameters update method.
void qtractorVst2Plugin::updateParamValues ( bool bUpdate )
{
	int nupdate = 0;

	// Make sure all cached parameter values are in sync
	// with plugin parameter values; update cache otherwise.
	AEffect *pVst2Effect = vst2_effect(0);
	if (pVst2Effect) {
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPlugin::Param *pParam = param.value();
			const float fValue
				= pVst2Effect->getParameter(pVst2Effect, pParam->index());
			if (pParam->value() != fValue) {
				pParam->setValue(fValue, bUpdate);
				++nupdate;
			}
		}
	}

	if (nupdate > 0)
		updateFormDirtyCount();
}


//----------------------------------------------------------------------------
// qtractorVst2Plugin::Param -- VST2 plugin control input port instance.
//

// Constructors.
qtractorVst2Plugin::Param::Param (
	qtractorVst2Plugin *pVst2Plugin, unsigned long iIndex )
	: qtractorPlugin::Param(pVst2Plugin, iIndex)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVst2Plugin::Param[%p] pVst2Plugin=%p iIndex=%lu", this, pVst2Plugin, iIndex);
#endif

	qtractorVst2PluginType *pVst2Type
		= static_cast<qtractorVst2PluginType *> (pVst2Plugin->type());
	
	char szName[64]; szName[0] = (char) 0;
	if (pVst2Type)
		pVst2Type->vst2_dispatch(effGetParamName, iIndex, 0, (void *) szName, 0.0f);
	if (!szName[0])
		::snprintf(szName, sizeof(szName), "Param #%lu", iIndex + 1); // Default dummy name.
	setName(szName);

	setMinValue(0.0f);
	setMaxValue(1.0f);

	::memset(&m_props, 0, sizeof(m_props));

	if (pVst2Type &&	pVst2Type->vst2_dispatch(
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
	if (pVst2Type && pVst2Type->effect()) {
		AEffect *pVst2Effect = (pVst2Type->effect())->vst2_effect();
		if (pVst2Effect)
			qtractorPlugin::Param::setValue(
				pVst2Effect->getParameter(pVst2Effect, iIndex), true);
	}

	setDefaultValue(qtractorPlugin::Param::value());

	// Initialize port value...
	// reset();
}


// Port range hints predicate methods.
bool qtractorVst2Plugin::Param::isBoundedBelow (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
#endif
}

bool qtractorVst2Plugin::Param::isBoundedAbove (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntegerMinMax);
#endif
}

bool qtractorVst2Plugin::Param::isDefaultValue (void) const
{
	return true;
}

bool qtractorVst2Plugin::Param::isLogarithmic (void) const
{
	return false;
}

bool qtractorVst2Plugin::Param::isSampleRate (void) const
{
	return false;
}

bool qtractorVst2Plugin::Param::isInteger (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterUsesIntStep);
#endif
}

bool qtractorVst2Plugin::Param::isToggled (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return (m_props.flags & kVstParameterIsSwitch);
#endif
}

bool qtractorVst2Plugin::Param::isDisplay (void) const
{
#ifdef CONFIG_VESTIGE_OLD
	return false;
#else
	return true;
#endif
}


// Current display value.
QString qtractorVst2Plugin::Param::display (void) const
{
	qtractorVst2PluginType *pVst2Type = nullptr;
	if (plugin())
		pVst2Type = static_cast<qtractorVst2PluginType *> (plugin()->type());
	if (pVst2Type) {
		QString sDisplay;
		char szText[64];
		// Grab parameter display value...
		szText[0] = '\0';
		pVst2Type->vst2_dispatch(effGetParamDisplay,
			index(), 0, (void *) szText, 0.0f);
		if (szText[0]) {
			// Got
			sDisplay += QString(szText).trimmed();
			// Grab parameter name...
			szText[0] = '\0';
			pVst2Type->vst2_dispatch(effGetParamLabel,
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
// VST2 host callback file selection helpers.

#ifndef CONFIG_VESTIGE

static VstIntPtr qtractorVst2Plugin_openFileSelector (
	AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVst2Plugin *pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
	if (pVst2Plugin == nullptr)
		return 0;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin_openFileSelector(%p, %p) command=%d reserved=%d",
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
		QWidget *pParentWidget = pVst2Plugin->editorWidget();
		if (pParentWidget)
			pParentWidget = pParentWidget->window();
		const QString& sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg(pVst2Plugin->title());
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
		QWidget *pParentWidget = pVst2Plugin->editorWidget();
		if (pParentWidget)
			pParentWidget = pParentWidget->window();
		const QString& sTitle = QString("%1 - %2")
			.arg(pvfs->title).arg(pVst2Plugin->title());
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


static VstIntPtr qtractorVst2Plugin_closeFileSelector (
	AEffect *effect, VstFileSelect *pvfs )
{
	qtractorVst2Plugin *pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
	if (pVst2Plugin == nullptr)
		return 0;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst2Plugin_closeFileSelector(%p, %p) command=%d reserved=%d",
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
#define VST2_HC_FMT "qtractorVst2Plugin_HostCallback(%p, %s(%d), %d, %d, %p, %g)"
#define VST2_HC_DEBUG(s)  qDebug(VST2_HC_FMT, \
	effect, (s), int(opcode), int(index), int(value), ptr, opt)
#define VST2_HC_DEBUGX(s) qDebug(VST2_HC_FMT " [%s]", \
	effect, (s), int(opcode), int(index), int(value), ptr, opt, (char *) ptr)
#else
#define VST2_HC_DEBUG(s)
#define VST2_HC_DEBUGX(s)
#endif

static VstIntPtr VSTCALLBACK qtractorVst2Plugin_HostCallback ( AEffect *effect,
	VstInt32 opcode, VstInt32 index, VstIntPtr value, void *ptr, float opt )
{
	VstIntPtr ret = 0;
	qtractorVst2Plugin *pVst2Plugin = nullptr;
	static VstTimeInfo s_vst2TimeInfo;
	qtractorSession *pSession = qtractorSession::getInstance();

	switch (opcode) {

	// VST 1.0 opcodes...
	case audioMasterVersion:
		VST2_HC_DEBUG("audioMasterVersion");
		ret = 2; // vst2.x
		break;

	case audioMasterAutomate:
		VST2_HC_DEBUG("audioMasterAutomate");
	//	effect->setParameter(effect, index, opt);
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			pVst2Plugin->updateParamValue(index, opt, false);
		//	QApplication::processEvents();
		}
		break;

	case audioMasterCurrentId:
		VST2_HC_DEBUG("audioMasterCurrentId");
		ret = (VstIntPtr) g_iVst2ShellCurrentId;
		break;

	case audioMasterIdle:
		VST2_HC_DEBUG("audioMasterIdle");
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			pVst2Plugin->updateParamValues(false);
			pVst2Plugin->idleEditor();
		//	QApplication::processEvents();
		}
		break;

	case audioMasterGetTime:
	//	VST2_HC_DEBUG("audioMasterGetTime");
		::memset(&s_vst2TimeInfo, 0, sizeof(s_vst2TimeInfo));
		if (pSession) {
			qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
			if (pAudioEngine) {
				const qtractorAudioEngine::TimeInfo& timeInfo
					= pAudioEngine->timeInfo();
				s_vst2TimeInfo.samplePos = double(timeInfo.frame);
				s_vst2TimeInfo.sampleRate = double(timeInfo.sampleRate);
				s_vst2TimeInfo.flags = 0;
				if (timeInfo.playing)
					s_vst2TimeInfo.flags |= (kVstTransportChanged | kVstTransportPlaying);
				s_vst2TimeInfo.flags |= kVstPpqPosValid;
				s_vst2TimeInfo.ppqPos = double(timeInfo.beats);
				s_vst2TimeInfo.flags |= kVstBarsValid;
				s_vst2TimeInfo.barStartPos = double(timeInfo.barBeats);
				s_vst2TimeInfo.flags |= kVstTempoValid;
				s_vst2TimeInfo.tempo  = double(timeInfo.tempo);
				s_vst2TimeInfo.flags |= kVstTimeSigValid;
				s_vst2TimeInfo.timeSigNumerator = timeInfo.beatsPerBar;
				s_vst2TimeInfo.timeSigDenominator = timeInfo.beatType;
			}
		}
		ret = (VstIntPtr) &s_vst2TimeInfo;
		break;

	case audioMasterProcessEvents:
		VST2_HC_DEBUG("audioMasterProcessEvents");
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			qtractorMidiManager *pMidiManager = nullptr;
			qtractorPluginList *pPluginList = pVst2Plugin->list();
			if (pPluginList)
				pMidiManager = pPluginList->midiManager();
			if (pMidiManager) {
				pMidiManager->vst2_events_copy((VstEvents *) ptr);
				ret = 1; // supported and processed.
			}
		}
		break;

	case audioMasterIOChanged:
		VST2_HC_DEBUG("audioMasterIOChanged");
		break;

	case audioMasterSizeWindow:
		VST2_HC_DEBUG("audioMasterSizeWindow");
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			pVst2Plugin->resizeEditor(int(index), int(value));
			ret = 1; // supported.
		}
		break;

	case audioMasterGetSampleRate:
		VST2_HC_DEBUG("audioMasterGetSampleRate");
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
				if (pAudioEngine)
					ret = (VstIntPtr) pAudioEngine->sampleRate();
			}
		}
		break;

	case audioMasterGetBlockSize:
		VST2_HC_DEBUG("audioMasterGetBlockSize");
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
				if (pAudioEngine)
					ret = (VstIntPtr) pAudioEngine->blockSize();
			}
		}
		break;

	case audioMasterGetInputLatency:
		VST2_HC_DEBUG("audioMasterGetInputLatency");
		break;

	case audioMasterGetOutputLatency:
		VST2_HC_DEBUG("audioMasterGetOutputLatency");
		break;

	case audioMasterGetCurrentProcessLevel:
	//	VST2_HC_DEBUG("audioMasterGetCurrentProcessLevel");
		break;

	case audioMasterGetAutomationState:
		VST2_HC_DEBUG("audioMasterGetAutomationState");
		ret = 1; // off.
		break;

#if !defined(VST_2_3_EXTENSIONS) 
	case audioMasterGetSpeakerArrangement:
		VST2_HC_DEBUG("audioMasterGetSpeakerArrangement");
		break;
#endif

	case audioMasterGetVendorString:
		VST2_HC_DEBUG("audioMasterGetVendorString");
		::strcpy((char *) ptr, QTRACTOR_DOMAIN);
		ret = 1; // ok.
		break;

	case audioMasterGetProductString:
		VST2_HC_DEBUG("audioMasterGetProductString");
		::strcpy((char *) ptr, QTRACTOR_TITLE);
		ret = 1; // ok.
		break;

	case audioMasterGetVendorVersion:
		VST2_HC_DEBUG("audioMasterGetVendorVersion");
		break;

	case audioMasterVendorSpecific:
		VST2_HC_DEBUG("audioMasterVendorSpecific");
		break;

	case audioMasterCanDo:
		VST2_HC_DEBUGX("audioMasterCanDo");
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
		VST2_HC_DEBUG("audioMasterGetLanguage");
		ret = (VstIntPtr) kVstLangEnglish;
		break;

#if 0 // !VST_FORCE_DEPRECATED
	case audioMasterPinConnected:
		VST2_HC_DEBUG("audioMasterPinConnected");
		break;

	// VST 2.0 opcodes...
	case audioMasterWantMidi:
		VST2_HC_DEBUG("audioMasterWantMidi");
		break;

	case audioMasterSetTime:
		VST2_HC_DEBUG("audioMasterSetTime");
		break;

	case audioMasterTempoAt:
		VST2_HC_DEBUG("audioMasterTempoAt");
		if (pSession)
			ret = (VstIntPtr) (pSession->tempo() * 10000.0f);
		break;

	case audioMasterGetNumAutomatableParameters:
		VST2_HC_DEBUG("audioMasterGetNumAutomatableParameters");
		break;

	case audioMasterGetParameterQuantization:
		VST2_HC_DEBUG("audioMasterGetParameterQuantization");
		ret = 1; // full single float precision
		break;

	case audioMasterNeedIdle:
		VST2_HC_DEBUG("audioMasterNeedIdle");
		break;

	case audioMasterGetPreviousPlug:
		VST2_HC_DEBUG("audioMasterGetPreviousPlug");
		break;

	case audioMasterGetNextPlug:
		VST2_HC_DEBUG("audioMasterGetNextPlug");
		break;

	case audioMasterWillReplaceOrAccumulate:
		VST2_HC_DEBUG("audioMasterWillReplaceOrAccumulate");
		ret = 1;
		break;

	case audioMasterSetOutputSampleRate:
		VST2_HC_DEBUG("audioMasterSetOutputSampleRate");
		break;

	case audioMasterSetIcon:
		VST2_HC_DEBUG("audioMasterSetIcon");
		break;

	case audioMasterOpenWindow:
		VST2_HC_DEBUG("audioMasterOpenWindow");
		break;

	case audioMasterCloseWindow:
		VST2_HC_DEBUG("audioMasterCloseWindow");
		break;
#endif

	case audioMasterGetDirectory:
		VST2_HC_DEBUG("audioMasterGetDirectory");
		break;

	case audioMasterUpdateDisplay:
		VST2_HC_DEBUG("audioMasterUpdateDisplay");
		pVst2Plugin = qtractorVst2Plugin::findPlugin(effect);
		if (pVst2Plugin) {
			pVst2Plugin->updateParamValues(false);
		//	QApplication::processEvents();
			ret = 1; // supported.
		}
		break;

	case audioMasterBeginEdit:
		VST2_HC_DEBUG("audioMasterBeginEdit");
		break;

	case audioMasterEndEdit:
		VST2_HC_DEBUG("audioMasterEndEdit");
		break;

#ifndef CONFIG_VESTIGE
	case audioMasterOpenFileSelector:
		VST2_HC_DEBUG("audioMasterOpenFileSelector");
		ret = qtractorVst2Plugin_openFileSelector(effect, (VstFileSelect *) ptr);
		break;

	case audioMasterCloseFileSelector:
		VST2_HC_DEBUG("audioMasterCloseFileSelector");
		ret = qtractorVst2Plugin_closeFileSelector(effect, (VstFileSelect *) ptr);
		break;
#endif
	default:
		VST2_HC_DEBUG("audioMasterUnknown");
		break;
	}

	return ret;
}


//-----------------------------------------------------------------------------
// Structures for VST2 presets (fxb/fxp files)

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
struct qtractorVst2Preset::BaseHeader
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
struct qtractorVst2Preset::ProgHeader
{
	VstInt32 numParams;			// number of parameters
	char prgName[28];			// program name (null-terminated ASCII string)
};


// Bank sub-header structure (fxb files)
//
struct qtractorVst2Preset::BankHeader
{
	VstInt32 numPrograms;		// number of programs
	VstInt32 currentProgram;	// version 2: current program number
	char future[124];			// reserved, should be zero
};


// Common auxiliary chunk structure (chunked fxb/fxp files)
//
struct qtractorVst2Preset::Chunk
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
// class qtractorVst2Preset -- VST2 preset file interface
//

// Constructor.
qtractorVst2Preset::qtractorVst2Preset ( qtractorVst2Plugin *pVst2Plugin )
	: m_pVst2Plugin(pVst2Plugin)
{
}


// Loader methods.
//
bool qtractorVst2Preset::load_bank_progs ( QFile& file )
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
		= m_pVst2Plugin->vst2_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);

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

		for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i)
			m_pVst2Plugin->vst2_dispatch(i, effSetProgram, 0, iProgram, nullptr, 0.0f);

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

	for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i)
		m_pVst2Plugin->vst2_dispatch(i, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);

	return true;
}


bool qtractorVst2Preset::load_prog_params ( QFile& file )
{
	ProgHeader prog_header;
	const int nread = sizeof(prog_header);
	if (file.read((char *) &prog_header, nread) < nread)
		return false;

	fx_endian_swap(prog_header.numParams);

	for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i)
		m_pVst2Plugin->vst2_dispatch(i, effSetProgramName, 0, 0, (void *) prog_header.prgName, 0.0f);

	const int iNumParams = int(prog_header.numParams);
	if (iNumParams < 1)
		return false;

	const int nread_params = iNumParams * sizeof(float);
	float *params = new float [iNumParams];
	if (file.read((char *) params, nread_params) < nread_params)
		return false;

	for (int iParam = 0; iParam < iNumParams; ++iParam) {
		fx_endian_swap(params[iParam]);
		for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i) {
			AEffect *pVst2Effect = m_pVst2Plugin->vst2_effect(i);
			if (pVst2Effect)
				pVst2Effect->setParameter(pVst2Effect, iParam, params[iParam]);
		}
	}

	delete [] params;
	return true;
}


bool qtractorVst2Preset::load_bank_chunk ( QFile& file )
{
	BankHeader bank_header;
	const int nread = sizeof(bank_header);
	if (file.read((char *) &bank_header, nread) < nread)
		return false;

	const int iCurrentProgram
		= m_pVst2Plugin->vst2_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);

	const bool bResult = load_chunk(file, 0);

	for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i)
		m_pVst2Plugin->vst2_dispatch(i, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);

	return bResult;
}


bool qtractorVst2Preset::load_prog_chunk ( QFile& file )
{
	ProgHeader prog_header;
	const int nread = sizeof(prog_header);
	if (file.read((char *) &prog_header, nread) < nread)
		return false;

	return load_chunk(file, 1);
}


bool qtractorVst2Preset::load_chunk ( QFile& file, int preset )
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

	for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i) {
		m_pVst2Plugin->vst2_dispatch(i, effSetChunk,
			preset, chunk.size,	(void *) chunk.data, 0.0f);
	}

	delete [] chunk.data;
	return true;
}


// File loader.
//
bool qtractorVst2Preset::load ( const QString& sFilename )
{
	if (m_pVst2Plugin == nullptr)
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
	qDebug("qtractorVst2Preset::load(\"%s\")", sFilename.toUtf8().constData());
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
		qDebug("qtractorVst2Presetload() header.chunkMagic is not \"%s\".", cMagic);
	#endif
	}
	else
	if (base_header.fxID != VstInt32(m_pVst2Plugin->uniqueID())) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst2Preset::load() header.fxID != 0x%08lx.", m_pVst2Plugin->uniqueID());
	#endif
	}
	else
	if (fx_is_magic(base_header.fxMagic, bankMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst2Preset::load() header.fxMagic is \"%s\" (regular fxb)", bankMagic);
	#endif
		bResult = load_bank_progs(file);
	}
	else
	if (fx_is_magic(base_header.fxMagic, chunkBankMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst2Preset::load() header.fxMagic is \"%s\" (chunked fxb)", chunkBankMagic);
	#endif
		bResult = load_bank_chunk(file);
	}
	else
	if (fx_is_magic(base_header.fxMagic, fMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst2Preset::load() header.fxMagic is \"%s\" (regular fxp)", fMagic);
	#endif
		bResult = load_prog_params(file);
	}
	else
	if (fx_is_magic(base_header.fxMagic, chunkPresetMagic)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst2Preset::load() header.fxMagic is \"%s\" (chunked fxp)", chunkPresetMagic);
	#endif
		bResult = load_prog_chunk(file);
	}
	#ifdef CONFIG_DEBUG
	else qDebug("qtractorVst2Preset::load() header.fxMagic not recognized.");
	#endif

	file.close();

	// HACK: Make sure all displayed parameter values are in sync.
	m_pVst2Plugin->updateParamValues(false);

	return bResult;
}


// Saver methods.
//
bool qtractorVst2Preset::save_bank_progs ( QFile& file )
{
	if (m_pVst2Plugin == nullptr)
		return false;

	AEffect *pVst2Effect = m_pVst2Plugin->vst2_effect(0);
	if (pVst2Effect == nullptr)
		return false;

	const int iNumPrograms = pVst2Effect->numPrograms;
	if (iNumPrograms < 1)
		return false;

	const int iCurrentProgram
		= m_pVst2Plugin->vst2_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);
	const int iVst2Version
		= m_pVst2Plugin->vst2_dispatch(0, effGetVstVersion, 0, 0, nullptr, 0.0f);

	BankHeader bank_header;
	::memset(&bank_header, 0, sizeof(bank_header));
	bank_header.numPrograms = iNumPrograms;
	bank_header.currentProgram = iCurrentProgram;

	fx_endian_swap(bank_header.numPrograms);
	fx_endian_swap(bank_header.currentProgram);

	file.write((char *) &bank_header, sizeof(bank_header));

	const bool bChunked
		= (m_pVst2Plugin->type())->isConfigure();

	bool bResult = false;

	for (int iProgram = 0; iProgram < iNumPrograms; ++iProgram) {

		m_pVst2Plugin->vst2_dispatch(0, effSetProgram, 0, iProgram, nullptr, 0.0f);

		BaseHeader base_header;
		::memset(&base_header, 0, sizeof(base_header));
		base_header.chunkMagic = *(VstInt32 *) cMagic;
		base_header.byteSize = 0; // FIXME!
		base_header.fxMagic = *(VstInt32 *) (bChunked ? chunkPresetMagic : fMagic);
		base_header.version = 1;
		base_header.fxID = m_pVst2Plugin->uniqueID();
		base_header.fxVersion = iVst2Version;

		// Estimate size of this section...
		base_header.byteSize = sizeof(base_header)
			- sizeof(base_header.chunkMagic)
			- sizeof(base_header.byteSize);

		Chunk chunk;
		if (bChunked) {
			get_chunk(chunk, 1);
			base_header.byteSize += sizeof(chunk.size) + chunk.size;
		} else {
			const int iNumParams  = pVst2Effect->numParams;
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

	m_pVst2Plugin->vst2_dispatch(0, effSetProgram, 0, iCurrentProgram, nullptr, 0.0f);

	return bResult;
}


bool qtractorVst2Preset::save_prog_params ( QFile& file )
{
	if (m_pVst2Plugin == nullptr)
		return false;

	AEffect *pVst2Effect = m_pVst2Plugin->vst2_effect(0);
	if (pVst2Effect == nullptr)
		return false;

	const int iNumParams = pVst2Effect->numParams;
	if (iNumParams < 1)
		return false;

	ProgHeader prog_header;
	::memset(&prog_header, 0, sizeof(prog_header));
	prog_header.numParams = iNumParams;

	m_pVst2Plugin->vst2_dispatch(0, effGetProgramName, 0, 0, (void *) prog_header.prgName, 0.0f);

	fx_endian_swap(prog_header.numParams);

	file.write((char *) &prog_header, sizeof(prog_header));

	float *params = new float [iNumParams];
	for (int iParam = 0; iParam < iNumParams; ++iParam) {
		params[iParam] = pVst2Effect->getParameter(pVst2Effect, iParam);
		fx_endian_swap(params[iParam]);
	}

	file.write((char *) params, iNumParams * sizeof(float));
	delete [] params;

	return true;
}


bool qtractorVst2Preset::save_bank_chunk ( QFile& file, const Chunk& chunk )
{
	if (m_pVst2Plugin == nullptr)
		return false;

	AEffect *pVst2Effect = m_pVst2Plugin->vst2_effect(0);
	if (pVst2Effect == nullptr)
		return false;

	const int iNumPrograms = pVst2Effect->numPrograms;
	const int iCurrentProgram
		= m_pVst2Plugin->vst2_dispatch(0, effGetProgram, 0, 0, nullptr, 0.0f);

	BankHeader bank_header;
	::memset(&bank_header, 0, sizeof(bank_header));
	bank_header.numPrograms = iNumPrograms;
	bank_header.currentProgram = iCurrentProgram;

	fx_endian_swap(bank_header.numPrograms);
	fx_endian_swap(bank_header.currentProgram);

	file.write((char *) &bank_header, sizeof(bank_header));

	return save_chunk(file, chunk);
}


bool qtractorVst2Preset::save_prog_chunk ( QFile& file, const Chunk& chunk )
{
	if (m_pVst2Plugin == nullptr)
		return false;

	AEffect *pVst2Effect = m_pVst2Plugin->vst2_effect(0);
	if (pVst2Effect == nullptr)
		return false;

	const int iNumParams = pVst2Effect->numParams;

	ProgHeader prog_header;
	::memset(&prog_header, 0, sizeof(prog_header));
	prog_header.numParams = iNumParams;

	m_pVst2Plugin->vst2_dispatch(0, effGetProgramName,
		0, 0, (void *) prog_header.prgName, 0.0f);

	fx_endian_swap(prog_header.numParams);

	file.write((char *) &prog_header, sizeof(prog_header));

	return save_chunk(file, chunk);
}


bool qtractorVst2Preset::save_chunk ( QFile& file, const Chunk& chunk )
{
	const int ndata = int(chunk.size);

	VstInt32 chunk_size = ndata;
	fx_endian_swap(chunk_size);

	file.write((char *) &chunk_size, sizeof(chunk_size));
	file.write((char *)  chunk.data, ndata);

	return true;
}


bool qtractorVst2Preset::get_chunk ( Chunk& chunk, int preset )
{
	chunk.data = nullptr;
	chunk.size = m_pVst2Plugin->vst2_dispatch(0,
		effGetChunk, preset, 0, (void *) &chunk.data, 0.0f);
	return (chunk.size > 0 && chunk.data != nullptr);
}


// File saver.
//
bool qtractorVst2Preset::save ( const QString& sFilename )
{
	if (m_pVst2Plugin == nullptr)
		return false;

	AEffect *pVst2Effect = m_pVst2Plugin->vst2_effect(0);
	if (pVst2Effect == nullptr)
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
	qDebug("qtractorVst2Preset::save(\"%s\")", sFilename.toUtf8().constData());
#endif

	const bool bChunked
		= (m_pVst2Plugin->type())->isConfigure();
	const int iVst2Version
		= m_pVst2Plugin->vst2_dispatch(0, effGetVstVersion, 0, 0, nullptr, 0.0f);

	BaseHeader base_header;
	::memset(&base_header, 0, sizeof(base_header));
	base_header.chunkMagic = *(VstInt32 *) cMagic;
	base_header.byteSize = 0;	// FIXME: see below...
	base_header.fxMagic = 0;	//
	base_header.version = 1;
	base_header.fxID = m_pVst2Plugin->uniqueID();
	base_header.fxVersion = iVst2Version;

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
			const int iNumParams  = pVst2Effect->numParams;
			base_header.byteSize += pVst2Effect->numPrograms
				* (sizeof(ProgHeader) + iNumParams * sizeof(float));
			base_header.fxMagic = *(VstInt32 *) bankMagic;
		}
	} else {
		char szName[24]; ::memset(szName, 0, sizeof(szName));
		::strncpy(szName, fi.baseName().toUtf8().constData(), sizeof(szName) - 1);
		for (unsigned short i = 0; i < m_pVst2Plugin->instances(); ++i)
			m_pVst2Plugin->vst2_dispatch(i, effSetProgramName, 0, 0, (void *) szName, 0.0f);
		if (bChunked) {
			get_chunk(chunk, 1);
			base_header.byteSize += sizeof(chunk.size) + chunk.size;
			base_header.fxMagic = *(VstInt32 *) chunkPresetMagic;
		} else {
			const int iNumParams  = pVst2Effect->numParams;
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


#endif	// CONFIG_VST2

// end of qtractorVst2Plugin.cpp
