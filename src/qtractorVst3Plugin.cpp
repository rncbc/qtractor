// qtractorVst3Plugin.cpp
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

#ifdef CONFIG_VST3

#include "qtractorVst3Plugin.h"

#include "qtractorSession.h"
#include "qtractorSessionCursor.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"

#include "pluginterfaces/vst/ivsthostapplication.h"
#include "pluginterfaces/vst/ivstpluginterfacesupport.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstunits.h"

#include "pluginterfaces/gui/iplugview.h"

#include "pluginterfaces/base/ibstream.h"

#include "base/source/fobject.h"

#include <QWidget>
#include <QVBoxLayout>

#include <QTimer>

#include <QTimerEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>

#include <QFileInfo>
#include <QMap>

#include <QRegularExpression>


#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#define CONFIG_VST3_XCB
#endif

#ifdef CONFIG_VST3_XCB
#include <xcb/xcb.h>
#endif


using namespace Steinberg;
using namespace Linux;


//-----------------------------------------------------------------------------
// A Vst::String128 to QString converter.
//
static inline
QString fromTChar ( const Vst::TChar *str )
{
	return QString::fromUtf16(reinterpret_cast<const char16_t *> (str));
}


//-----------------------------------------------------------------------------
// class qtractorVst3PluginHost -- VST3 plugin host context decl.
//

class qtractorVst3PluginHost : public Vst::IHostApplication
{
public:

	// Constructor.
	qtractorVst3PluginHost ();

	// Destructor.
	virtual ~qtractorVst3PluginHost ();

	DECLARE_FUNKNOWN_METHODS

	//--- IHostApplication ---
	//
	tresult PLUGIN_API getName (Vst::String128 name) override;
	tresult PLUGIN_API createInstance (TUID cid, TUID _iid, void **obj) override;

	FUnknown *get() { return static_cast<Vst::IHostApplication *> (this); }

	// QTimer stuff...
	//
	void startTimer (int msecs);
	void stopTimer ();

	int timerInterval() const;

	// RunLoop adapters...
	//
	tresult registerEventHandler (IEventHandler *handler, FileDescriptor fd);
	tresult unregisterEventHandler (IEventHandler *handler);

	tresult registerTimer (ITimerHandler *handler, TimerInterval msecs);
	tresult unregisterTimer (ITimerHandler *handler);

	// Executive methods.
	//
	void processTimers();
	void processEventHandlers();

#ifdef CONFIG_VST3_XCB
	void openXcbConnection();
	void closeXcbConnection();
#endif

	// Common host time-keeper context accessors.
	Vst::ProcessContext *processContext();

	void processAddRef();
	void processReleaseRef();

	// Common host time-keeper process context.
	void updateProcessContext(qtractorAudioEngine *pAudioEngine);

	// Cleanup.
	void clear();

protected:

	class PlugInterfaceSupport;

	class Attribute;
	class AttributeList;
	class Message;

	class Timer;

private:

	// Instance members.
	IPtr<PlugInterfaceSupport> m_plugInterfaceSupport;

	Timer *m_pTimer;

	unsigned int m_timerRefCount;

	struct TimerItem
	{
		TimerItem(ITimerHandler *h, TimerInterval i)
			: handler(h), interval(i), counter(0) {}

		ITimerHandler *handler;
		TimerInterval  interval;
		TimerInterval  counter;
	};

	QHash<ITimerHandler *, TimerItem *> m_timers;
	QMultiHash<IEventHandler *, int> m_fileDescriptors;

#ifdef CONFIG_VST3_XCB
	xcb_connection_t   *m_pXcbConnection;
	int                 m_iXcbFileDescriptor;
#endif	// defined(XCB_TEST)

	Vst::ProcessContext m_processContext;
	unsigned int        m_processRefCount;
};


//-----------------------------------------------------------------------------
//
class qtractorVst3PluginHost::PlugInterfaceSupport
	: public FObject, public Vst::IPlugInterfaceSupport
{
public:

	// Constructor.
	PlugInterfaceSupport ()
	{
		addPluInterfaceSupported(Vst::IComponent::iid);
		addPluInterfaceSupported(Vst::IAudioProcessor::iid);
		addPluInterfaceSupported(Vst::IEditController::iid);
		addPluInterfaceSupported(Vst::IConnectionPoint::iid);
		addPluInterfaceSupported(Vst::IUnitInfo::iid);
	//	addPluInterfaceSupported(Vst::IUnitData::iid);
		addPluInterfaceSupported(Vst::IProgramListData::iid);
		addPluInterfaceSupported(Vst::IMidiMapping::iid);
	//	addPluInterfaceSupported(Vst::IEditController2::iid);
	}

	OBJ_METHODS (PlugInterfaceSupport, FObject)
	REFCOUNT_METHODS (FObject)
	DEFINE_INTERFACES
		DEF_INTERFACE (Vst::IPlugInterfaceSupport)
	END_DEFINE_INTERFACES (FObject)

	//--- IPlugInterfaceSupport ----
	//
	tresult PLUGIN_API isPlugInterfaceSupported (const TUID _iid) override
	{
		if (m_fuids.contains(QString::fromLocal8Bit(_iid)))
			return kResultOk;
		else
			return kResultFalse;
	}

protected:

	void addPluInterfaceSupported(const TUID& _iid)
		{ m_fuids.append(QString::fromLocal8Bit(_iid)); }

private:

	// Instance members.
	QList<QString> m_fuids;
};


//-----------------------------------------------------------------------------
//
class qtractorVst3PluginHost::Attribute
{
public:

	enum Type
	{
		kInteger,
		kFloat,
		kString,
		kBinary
	};

	// Constructors.
	Attribute (int64 value) : m_size(0), m_type(kInteger)
		{ m_v.intValue = value; }

	Attribute (double value) : m_size(0), m_type(kFloat)
		{ m_v.floatValue = value; }

	Attribute (const Vst::TChar *value, uint32 size)
		: m_size(size), m_type(kString)
	{
		m_v.stringValue = new Vst::TChar[size];
		::memcpy(m_v.stringValue, value, size * sizeof (Vst::TChar));
	}

	Attribute (const void *value, uint32 size)
		: m_size(size), m_type(kBinary)
	{
		m_v.binaryValue = new char[size];
		::memcpy(m_v.binaryValue, value, size);
	}

	// Destructor.
	~Attribute ()
	{
		if (m_size)
			delete [] m_v.binaryValue;
	}

	// Accessors.
	int64 intValue () const
		{ return m_v.intValue; }

	double floatValue () const
		{ return m_v.floatValue; }

	const Vst::TChar *stringValue ( uint32& stringSize )
	{
		stringSize = m_size;
		return m_v.stringValue;
	}

	const void *binaryValue ( uint32& binarySize )
	{
		binarySize = m_size;
		return m_v.binaryValue;
	}

	Type getType () const
		{ return m_type; }

protected:

	// Instance members.
	union v
	{
		int64  intValue;
		double floatValue;
		Vst::TChar *stringValue;
		char  *binaryValue;

	} m_v;

	uint32 m_size;
	Type m_type;
};


//-----------------------------------------------------------------------------
//
class qtractorVst3PluginHost::AttributeList : public Vst::IAttributeList
{
public:

	// Constructor.
	AttributeList ()
	{
		FUNKNOWN_CTOR
	}

	// Destructor.
	virtual ~AttributeList ()
	{
		qDeleteAll(m_list);
		m_list.clear();

		FUNKNOWN_DTOR
	}

	DECLARE_FUNKNOWN_METHODS

	//--- IAttributeList ---
	//
	tresult PLUGIN_API setInt (AttrID aid, int64 value) override
	{
		removeAttrID(aid);
		m_list.insert(aid, new Attribute(value));
		return kResultTrue;
	}

	tresult PLUGIN_API getInt (AttrID aid, int64& value) override
	{
		Attribute *attr = m_list.value(aid, nullptr);
		if (attr) {
			value = attr->intValue();
			return kResultTrue;
		}
		return kResultFalse;
	}

	tresult PLUGIN_API setFloat (AttrID aid, double value) override
	{
		removeAttrID(aid);
		m_list.insert(aid, new Attribute(value));
		return kResultTrue;
	}

	tresult PLUGIN_API getFloat (AttrID aid, double& value) override
	{
		Attribute *attr = m_list.value(aid, nullptr);
		if (attr) {
			value = attr->floatValue();
			return kResultTrue;
		}
		return kResultFalse;
	}

	tresult PLUGIN_API setString (AttrID aid, const Vst::TChar *string) override
	{
		removeAttrID(aid);
		m_list.insert(aid, new Attribute(string, fromTChar(string).length()));
		return kResultTrue;
	}

	tresult PLUGIN_API getString (AttrID aid, Vst::TChar *string, uint32 size) override
	{
		Attribute *attr = m_list.value(aid, nullptr);
		if (attr) {
			uint32 size2 = 0;
			const Vst::TChar *string2 = attr->stringValue(size2);
			::memcpy(string, string2, qMin(size, size2) * sizeof(Vst::TChar));
			return kResultTrue;
		}
		return kResultFalse;
	}

	tresult PLUGIN_API setBinary (AttrID aid, const void* data, uint32 size) override
	{
		removeAttrID(aid);
		m_list.insert(aid, new Attribute(data, size));
		return kResultTrue;
	}

	tresult PLUGIN_API getBinary (AttrID aid, const void*& data, uint32& size) override
	{
		Attribute *attr = m_list.value(aid, nullptr);
		if (attr) {
			data = attr->binaryValue(size);
			return kResultTrue;
		}
		size = 0;
		return kResultFalse;
	}

protected:

	void removeAttrID (AttrID aid)
	{
		Attribute *attr = m_list.value(aid, nullptr);
		if (attr) {
			delete attr;
			m_list.remove(aid);
		}
	}

private:

	// Instance members.
	QHash<QString, Attribute *> m_list;
};

IMPLEMENT_FUNKNOWN_METHODS (qtractorVst3PluginHost::AttributeList, IAttributeList, IAttributeList::iid)


//-----------------------------------------------------------------------------
//
class qtractorVst3PluginHost::Message : public Vst::IMessage
{
public:

	// Constructor.
	Message () : m_messageId(nullptr), m_attributeList(nullptr)
	{
		FUNKNOWN_CTOR
	}

	// Destructor.
	virtual ~Message ()
	{
		setMessageID(nullptr);

		if (m_attributeList)
			m_attributeList->release();

		FUNKNOWN_DTOR
	}

	DECLARE_FUNKNOWN_METHODS

	//--- IMessage ---
	//
	const char *PLUGIN_API getMessageID () override
		{ return m_messageId; }

	void PLUGIN_API setMessageID (const char *messageId) override
	{
		if (m_messageId)
			delete [] m_messageId;

		m_messageId = nullptr;

		if (messageId) {
			size_t len = strlen(messageId) + 1;
			m_messageId = new char[len];
			::strcpy(m_messageId, messageId);
		}
	}

	Vst::IAttributeList* PLUGIN_API getAttributes () override
	{
		if (!m_attributeList)
			m_attributeList = new AttributeList();

		return m_attributeList;
	}

protected:

	// Instance members.
	char *m_messageId;

	AttributeList *m_attributeList;
};

IMPLEMENT_FUNKNOWN_METHODS (qtractorVst3PluginHost::Message, IMessage, IMessage::iid)


//-----------------------------------------------------------------------------
// class qtractorVst3PluginHost::Timer -- VST3 plugin host timer impl.
//

class qtractorVst3PluginHost::Timer : public QTimer
{
public:

	// Constructor.
	Timer (qtractorVst3PluginHost *pHost) : QTimer(), m_pHost(pHost) {}

	// Main method.
	void start (int msecs)
	{
		const int DEFAULT_MSECS = 30;

		int iInterval = QTimer::interval();
		if (iInterval == 0)
			iInterval = DEFAULT_MSECS;
		if (iInterval > msecs)
			iInterval = msecs;

		QTimer::start(iInterval);
	}

protected:

	void timerEvent (QTimerEvent *pTimerEvent)
	{
		if (pTimerEvent->timerId() == QTimer::timerId()) {
			m_pHost->processTimers();
			m_pHost->processEventHandlers();
		}
	}

private:

	// Instance members.
	qtractorVst3PluginHost *m_pHost;
};


//-----------------------------------------------------------------------------
// class qtractorVst3PluginHost -- VST3 plugin host context impl.
//

// Constructor.
qtractorVst3PluginHost::qtractorVst3PluginHost (void)
{
	FUNKNOWN_CTOR

	m_plugInterfaceSupport = owned(NEW PlugInterfaceSupport());

	m_pTimer = new Timer(this);

	m_timerRefCount = 0;

#ifdef CONFIG_VST3_XCB
	m_pXcbConnection = nullptr;
	m_iXcbFileDescriptor = 0;
#endif

	m_processRefCount = 0;
}


// Destructor.
qtractorVst3PluginHost::~qtractorVst3PluginHost (void)
{
	clear();

	delete m_pTimer;

	m_plugInterfaceSupport = nullptr;

	FUNKNOWN_DTOR
}



//--- IHostApplication ---
//
tresult PLUGIN_API qtractorVst3PluginHost::getName ( Vst::String128 name )
{
	const QString str("qtractorVst3PluginHost");
	const int nsize = qMin(str.length(), 127);
	::memcpy(name, str.utf16(), nsize * sizeof(Vst::TChar));
	name[nsize] = 0;
	return kResultOk;
}


tresult PLUGIN_API qtractorVst3PluginHost::createInstance (
	TUID cid, TUID _iid, void **obj )
{
	const FUID classID (FUID::fromTUID(cid));
	const FUID interfaceID (FUID::fromTUID(_iid));

	if (classID == Vst::IMessage::iid &&
		interfaceID == Vst::IMessage::iid) {
		*obj = new Message();
		return kResultOk;
	}
	else
	if (classID == Vst::IAttributeList::iid &&
		interfaceID == Vst::IAttributeList::iid) {
		*obj = new AttributeList();
		return kResultOk;
	}

	*obj = nullptr;
	return kResultFalse;
}


tresult PLUGIN_API qtractorVst3PluginHost::queryInterface (
	const char *_iid, void **obj )
{
	QUERY_INTERFACE(_iid, obj, FUnknown::iid, IHostApplication)
	QUERY_INTERFACE(_iid, obj, IHostApplication::iid, IHostApplication)

	if (m_plugInterfaceSupport &&
		m_plugInterfaceSupport->queryInterface(_iid, obj) == kResultOk)
		return kResultOk;

	*obj = nullptr;
	return kResultFalse;
}


uint32 PLUGIN_API qtractorVst3PluginHost::addRef (void)
	{ return 1;	}

uint32 PLUGIN_API qtractorVst3PluginHost::release (void)
	{ return 1; }


// QTimer stuff...
//
void qtractorVst3PluginHost::startTimer ( int msecs )
	{ if (++m_timerRefCount == 1) m_pTimer->start(msecs); }

void qtractorVst3PluginHost::stopTimer (void)
	{ if (m_timerRefCount > 0 && --m_timerRefCount == 0) m_pTimer->stop(); }

int qtractorVst3PluginHost::timerInterval (void) const
	{ return m_pTimer->interval(); }


// IRunLoop stuff...
//
tresult qtractorVst3PluginHost::registerEventHandler (
	IEventHandler *handler, FileDescriptor fd )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3PluginHost::registerEventHandler(%p, %d)", handler, int(fd));
#endif
	m_fileDescriptors.insert(handler, int(fd));
	return kResultOk;
}


tresult qtractorVst3PluginHost::unregisterEventHandler ( IEventHandler *handler )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3PluginHost::unregisterEventHandler(%p)", handler);
#endif
	m_fileDescriptors.remove(handler);
	return kResultOk;
}


tresult qtractorVst3PluginHost::registerTimer (
	ITimerHandler *handler, TimerInterval msecs )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3PluginHost::registerTimer(%p, %u)", handler, uint(msecs));
#endif
	m_timers.insert(handler, new TimerItem(handler, msecs));
	m_pTimer->start(int(msecs));
	return kResultOk;
}


tresult qtractorVst3PluginHost::unregisterTimer ( ITimerHandler *handler )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3PluginHost::unregisterTimer(%p)", handler);
#endif
	m_timers.remove(handler);
	if (m_timers.isEmpty())
		m_pTimer->stop();
	return kResultOk;
}


// Executive methods.
//
void qtractorVst3PluginHost::processTimers (void)
{
	foreach (TimerItem *timer, m_timers) {
		timer->counter += timerInterval();
		if (timer->counter >= timer->interval) {
			timer->handler->onTimer();
			timer->counter = 0;
		}
	}
}


void qtractorVst3PluginHost::processEventHandlers (void)
{
	int nfds = 0;

	fd_set rfds;
	fd_set wfds;
	fd_set efds;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

#ifdef CONFIG_VST3_XCB
	if (m_iXcbFileDescriptor) {
		FD_SET(m_iXcbFileDescriptor, &rfds);
		FD_SET(m_iXcbFileDescriptor, &wfds);
		FD_SET(m_iXcbFileDescriptor, &efds);
		nfds = qMax(nfds, m_iXcbFileDescriptor);
	}
#endif

	QMultiHash<IEventHandler *, int>::ConstIterator iter
		= m_fileDescriptors.constBegin();
	for ( ; iter != m_fileDescriptors.constEnd(); ++iter) {
		foreach (int fd, m_fileDescriptors.values(iter.key())) {
			FD_SET(fd, &rfds);
			FD_SET(fd, &wfds);
			FD_SET(fd, &efds);
			nfds = qMax(nfds, fd);
		}
	}

	timeval timeout;
	timeout.tv_sec  = 0;
	timeout.tv_usec = 1000 * timerInterval();

	const int result = ::select(nfds, &rfds, &wfds, nullptr, &timeout);
	if (result > 0)	{
		iter = m_fileDescriptors.constBegin();
		for ( ; iter != m_fileDescriptors.constEnd(); ++iter) {
			foreach (int fd, m_fileDescriptors.values(iter.key())) {
				if (FD_ISSET(fd, &rfds) ||
					FD_ISSET(fd, &wfds) ||
					FD_ISSET(fd, &efds)) {
					IEventHandler *handler = iter.key();
					handler->onFDIsSet(fd);
				}
			}
		}
	}
}


#ifdef CONFIG_VST3_XCB

void qtractorVst3PluginHost::openXcbConnection (void)
{
	if (m_pXcbConnection == nullptr) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3PluginHost::openXcbConnection()");
	#endif
		m_pXcbConnection = ::xcb_connect(nullptr, nullptr);
		m_iXcbFileDescriptor = ::xcb_get_file_descriptor(m_pXcbConnection);
	}
}

void qtractorVst3PluginHost::closeXcbConnection (void)
{
	if (m_pXcbConnection) {
		::xcb_disconnect(m_pXcbConnection);
		m_pXcbConnection = nullptr;
		m_iXcbFileDescriptor = 0;
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3PluginHost::closeXcbConnection()");
	#endif
	}
}

#endif	// CONFIG_VST3_XCB


// Common host time-keeper context accessor.
Vst::ProcessContext *qtractorVst3PluginHost::processContext (void)
{
	return &m_processContext;
}


void qtractorVst3PluginHost::processAddRef (void)
{
	++m_processRefCount;
}


void qtractorVst3PluginHost::processReleaseRef (void)
{
	if (m_processRefCount > 0) --m_processRefCount;
}


// Common host time-keeper process context.
void qtractorVst3PluginHost::updateProcessContext (
	qtractorAudioEngine *pAudioEngine )
{
	if (m_processRefCount < 1)
		return;

	jack_position_t pos;
	jack_transport_state_t state;

	if (pAudioEngine->isFreewheel()) {
		pos.frame = pAudioEngine->sessionCursor()->frame();
		pAudioEngine->timebase(&pos, 0);
		state = JackTransportRolling; // Fake transport rolling...
	} else {
		state = jack_transport_query(pAudioEngine->jackClient(), &pos);
	}

	if (state == JackTransportRolling)
		m_processContext.state |=  Vst::ProcessContext::kPlaying;
	else
		m_processContext.state &= ~Vst::ProcessContext::kPlaying;

	m_processContext.sampleRate = pos.frame_rate;
	m_processContext.projectTimeSamples = pos.frame;

	if (pos.valid & JackPositionBBT) {
		m_processContext.state |= Vst::ProcessContext::kTempoValid;
		m_processContext.tempo  = pos.beats_per_minute;
		m_processContext.state |= Vst::ProcessContext::kTimeSigValid;
		m_processContext.timeSigNumerator = pos.beats_per_bar;
		m_processContext.timeSigDenominator = pos.beat_type;
	} else {
		m_processContext.state &= ~Vst::ProcessContext::kTempoValid;
		m_processContext.state &= ~Vst::ProcessContext::kTimeSigValid;
	}
}


// Cleanup.
void qtractorVst3PluginHost::clear (void)
{
#ifdef CONFIG_VST3_XCB
	closeXcbConnection();
#endif

	m_timerRefCount = 0;
	m_processRefCount = 0;

	qDeleteAll(m_timers);
	m_timers.clear();

	m_fileDescriptors.clear();

	::memset(&m_processContext, 0, sizeof(Vst::ProcessContext));
}


// Host singleton.
static qtractorVst3PluginHost g_hostContext;


//----------------------------------------------------------------------
// class qtractorVst3PluginType::Impl -- VST3 plugin meta-interface impl.
//

class qtractorVst3PluginType::Impl
{
public:

	// Constructor.
	Impl (qtractorPluginFile *pFile) : m_pFile(pFile),
			m_component(nullptr), m_controller(nullptr),
			m_unitInfos(nullptr), m_iUniqueID(0) {}

	// Destructor.
	~Impl () { close(); }

	// Executive methods.
	bool open (unsigned long iIndex);
	void close ();

	// Accessors.
	Vst::IComponent *component () const
		{ return m_component; }
	Vst::IEditController *controller () const
		{ return m_controller; }

	Vst::IUnitInfo *unitInfos () const
		{ return m_unitInfos; }

	const QString& name () const
		{ return m_sName; }
	const QString& category () const
		{ return m_sCategory; }
	const QString& subCategories () const
		{ return m_sSubCategories; }
	const QString& vendor () const
		{ return m_sVendor; }
	const QString& email () const
		{ return m_sEmail; }
	const QString& url () const
		{ return m_sUrl; }
	const QString& version () const
		{ return m_sVersion; }
	const QString& sdkVersion () const
		{ return m_sSdkVersion; }

	unsigned long uniqueID () const
		{ return m_iUniqueID; }

	int numChannels (Vst::MediaType type, Vst::BusDirection direction) const;

private:

	// Instance members.
	qtractorPluginFile *m_pFile;

	IPtr<Vst::IComponent> m_component;
	IPtr<Vst::IEditController> m_controller;

	IPtr<Vst::IUnitInfo> m_unitInfos;

	QString m_sName;
	QString m_sCategory;
	QString m_sSubCategories;
	QString m_sVendor;
	QString m_sEmail;
	QString m_sUrl;
	QString m_sVersion;
	QString m_sSdkVersion;

	unsigned long m_iUniqueID;
};


//----------------------------------------------------------------------
// class qtractorVst3PluginType::Impl -- VST3 plugin interface impl.
//

// Executive methods.
//
bool qtractorVst3PluginType::Impl::open ( unsigned long iIndex )
{
	close();

	typedef bool (PLUGIN_API *VST3_ModuleEntry)(void *);
	const VST3_ModuleEntry module_entry
		= reinterpret_cast<VST3_ModuleEntry> (m_pFile->resolve("ModuleEntry"));
	if (module_entry)
		module_entry(m_pFile->module());

	typedef IPluginFactory *(PLUGIN_API *VST3_GetPluginFactory)();
	const VST3_GetPluginFactory get_plugin_factory
		= reinterpret_cast<VST3_GetPluginFactory> (m_pFile->resolve("GetPluginFactory"));
	if (!get_plugin_factory) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
			" *** Failed to resolve plug-in factory.", this,
			m_pFile->filename().toUtf8().constData(), iIndex);
	#endif
		return false;
	}

	IPluginFactory *factory = get_plugin_factory();
	if (!factory) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
			" *** Failed to retrieve plug-in factory.", this,
			m_pFile->filename().toUtf8().constData(), iIndex);
	#endif
		return false;
	}

	PFactoryInfo factoryInfo;
	if (factory->getFactoryInfo(&factoryInfo) != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
			" *** Failed to retrieve plug-in factory information.", this,
			m_pFile->filename().toUtf8().constData(), iIndex);
	#endif
		return false;
	}

	IPluginFactory2 *factory2 = FUnknownPtr<IPluginFactory2> (factory);
	IPluginFactory3 *factory3 = FUnknownPtr<IPluginFactory3> (factory);
	if (factory3)
		factory3->setHostContext(g_hostContext.get());

	const int32 nclasses = factory->countClasses();

	unsigned long i = 0;

	for (int32 n = 0; n < nclasses; ++n) {

		PClassInfo classInfo;
		if (factory->getClassInfo(n, &classInfo) != kResultOk)
			continue;

		if (::strcmp(classInfo.category, kVstAudioEffectClass))
			continue;

		if (iIndex == i) {

			PClassInfoW classInfoW;
			if (factory3 && factory3->getClassInfoUnicode(n, &classInfoW) == kResultOk) {
				m_sName = fromTChar(classInfoW.name);
				m_sCategory = QString::fromLocal8Bit(classInfoW.category);
				m_sSubCategories = QString::fromLocal8Bit(classInfoW.subCategories);
				m_sVendor = fromTChar(classInfoW.vendor);
				m_sVersion = fromTChar(classInfoW.version);
				m_sSdkVersion = fromTChar(classInfoW.sdkVersion);
			} else {
				PClassInfo2 classInfo2;
				if (factory2 && factory2->getClassInfo2(n, &classInfo2) == kResultOk) {
					m_sName = QString::fromLocal8Bit(classInfo2.name);
					m_sCategory = QString::fromLocal8Bit(classInfo2.category);
					m_sSubCategories = QString::fromLocal8Bit(classInfo2.subCategories);
					m_sVendor = QString::fromLocal8Bit(classInfo2.vendor);
					m_sVersion = QString::fromLocal8Bit(classInfo2.version);
					m_sSdkVersion = QString::fromLocal8Bit(classInfo2.sdkVersion);
				} else {
					m_sName = QString::fromLocal8Bit(classInfo.name);
					m_sCategory = QString::fromLocal8Bit(classInfo.category);
					m_sSubCategories.clear();
					m_sVendor.clear();
					m_sVersion.clear();
					m_sSdkVersion.clear();
				}
			}

			if (m_sVendor.isEmpty())
				m_sVendor = QString::fromLocal8Bit(factoryInfo.vendor);
			if (m_sEmail.isEmpty())
				m_sEmail = QString::fromLocal8Bit(factoryInfo.email);
			if (m_sUrl.isEmpty())
				m_sUrl = QString::fromLocal8Bit(factoryInfo.url);

			m_iUniqueID = qHash(QByteArray(classInfo.cid, sizeof(TUID)));

			Vst::IComponent *component = nullptr;
			if (factory->createInstance(
					classInfo.cid, Vst::IComponent::iid,
					(void **) &component) != kResultOk) {
			#ifdef CONFIG_DEBUG
				qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
					" *** Failed to create plug-in component.", this,
					m_pFile->filename().toUtf8().constData(), iIndex);
			#endif
				return false;
			}

			m_component = owned(component);

			if (m_component->initialize(g_hostContext.get()) != kResultOk) {
			#ifdef CONFIG_DEBUG
				qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
					" *** Failed to initialize plug-in component.", this,
					m_pFile->filename().toUtf8().constData(), iIndex);
			#endif
				close();
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
						qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
							" *** Failed to create plug-in controller.", this,
							m_pFile->filename().toUtf8().constData(), iIndex);
					#endif
					}
					if (controller &&
						controller->initialize(g_hostContext.get()) != kResultOk) {
					#ifdef CONFIG_DEBUG
						qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
							" *** Failed to initialize plug-in controller.", this,
							m_pFile->filename().toUtf8().constData(), iIndex);
						controller = nullptr;
					#endif
					}
				}
			}

			if (controller) m_controller = owned(controller);

			Vst::IUnitInfo *unitInfos = nullptr;
			if (m_component->queryInterface(
					Vst::IUnitInfo::iid,
					(void **) &unitInfos) != kResultOk) {
				if (m_controller &&
					m_controller->queryInterface(
						Vst::IUnitInfo::iid,
						(void **) &unitInfos) != kResultOk) {
				#ifdef CONFIG_DEBUG
					qDebug("qtractorVst3PluginType::Impl[%p]::open(\"%s\", %lu)"
						" *** Failed to create plug-in units information.", this,
						m_pFile->filename().toUtf8().constData(), iIndex);
				#endif
				}
			}

			if (unitInfos) m_unitInfos = owned(unitInfos);

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


void qtractorVst3PluginType::Impl::close (void)
{
	if (m_component && m_controller) {
		FUnknownPtr<Vst::IConnectionPoint> component_cp(m_component);
		FUnknownPtr<Vst::IConnectionPoint> controller_cp(m_controller);
		if (component_cp && controller_cp) {
			component_cp->disconnect(controller_cp);
			controller_cp->disconnect(component_cp);
		}
	}

	m_unitInfos = nullptr;

	if (m_component && m_controller &&
		FUnknownPtr<Vst::IEditController> (m_component).getInterface()) {
		m_controller->terminate();
	}

	m_controller = nullptr;

	if (m_component) {
		m_component->terminate();
		m_component = nullptr;
		typedef bool (PLUGIN_API *VST3_ModuleExit)();
		const VST3_ModuleExit module_exit
			= reinterpret_cast<VST3_ModuleExit> (m_pFile->resolve("ModuleExit"));
		if (module_exit)
			module_exit();
	}
}


int qtractorVst3PluginType::Impl::numChannels (
	Vst::MediaType type, Vst::BusDirection direction ) const
{
	if (!m_component)
		return -1;

	int nchannels = 0;

	const int32 nbuses = m_component->getBusCount(type, direction);
	for (int32 i = 0; i < nbuses; ++i) {
		Vst::BusInfo busInfo;
		if (m_component->getBusInfo(type, direction, i, busInfo) == kResultOk) {
			if ((busInfo.busType == Vst::kMain) ||
				(busInfo.flags & Vst::BusInfo::kDefaultActive))
				nchannels += busInfo.channelCount;
		}
	}

	return nchannels;
}


//----------------------------------------------------------------------
// class qtractorVst3PluginType -- VST3 plugin meta-interface impl.
//

// Constructor.
qtractorVst3PluginType::qtractorVst3PluginType (
	qtractorPluginFile *pFile, unsigned long iIndex )
	: qtractorPluginType(pFile, iIndex, Vst3), m_pImpl(new Impl(pFile))
{
}


// Destructor.
qtractorVst3PluginType::~qtractorVst3PluginType (void)
{
	close();

	delete m_pImpl;
}


// Factory method (static)
qtractorVst3PluginType *qtractorVst3PluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	return new qtractorVst3PluginType(pFile, iIndex);
}


// Executive methods.
bool qtractorVst3PluginType::open (void)
{
	close();

	if (!m_pImpl->open(index()))
		return false;

	// Properties...
	m_sName = m_pImpl->name();
	m_sLabel = m_sName.simplified().replace(QRegularExpression("[\\s|\\.|\\-]+"), "_");
	m_iUniqueID = m_pImpl->uniqueID();

	m_iAudioIns  = m_pImpl->numChannels(Vst::kAudio, Vst::kInput);
	m_iAudioOuts = m_pImpl->numChannels(Vst::kAudio, Vst::kOutput);
	m_iMidiIns   = m_pImpl->numChannels(Vst::kEvent, Vst::kInput);
	m_iMidiOuts  = m_pImpl->numChannels(Vst::kEvent, Vst::kOutput);

	Vst::IEditController *controller = m_pImpl->controller();
	if (controller) {
		IPtr<IPlugView> editor =
			owned(controller->createView(Vst::ViewType::kEditor));
		m_bEditor = (editor != nullptr);
	}

	m_bRealtime  = true;
	m_bConfigure = true;

	m_iControlIns  = 0;
	m_iControlOuts = 0;

	if (controller) {
		const int32 nparams = controller->getParameterCount();
		for (int32 i = 0; i < nparams; ++i) {
			Vst::ParameterInfo paramInfo;
			::memset(&paramInfo, 0, sizeof(Vst::ParameterInfo));
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


void qtractorVst3PluginType::close (void)
{
	m_pImpl->close();
}


// Instance cached-deferred accessors.
const QString& qtractorVst3PluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
	#if 0
		m_sAboutText += QObject::tr("Name: ");
		m_sAboutText += m_pImpl->name();
	#endif
		QString sText = m_pImpl->version();
		if (!sText.isEmpty()) {
		//	m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Version: ");
			m_sAboutText += sText;
		}
		sText = m_pImpl->sdkVersion();
		if (!sText.isEmpty()) {
			m_sAboutText += '\t';
			m_sAboutText += '(';
			m_sAboutText += sText;
			m_sAboutText += ')';
		}
	#if 0
		sText = m_pImpl->category();
		if (!sText.isEmpty()) {
			m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Category: ");
			m_sAboutText += sText;
		}
	#endif
		sText = m_pImpl->subCategories();
		if (!sText.isEmpty()) {
			m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Categories: ");
			m_sAboutText += sText;
		}
		sText = m_pImpl->vendor();
		if (!sText.isEmpty()) {
			m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Vendor: ");
			m_sAboutText += sText;
		}
	#if 0
		sText = m_pImpl->email();
		if (!sText.isEmpty()) {
			m_sAboutText += '\n';
		//	m_sAboutText += QObject::tr("Email: ");
			m_sAboutText += sText;
		}
	#endif
		sText = m_pImpl->url();
		if (!sText.isEmpty()) {
			m_sAboutText += '\n';
		//	m_sAboutText += QObject::tr("URL: ");
			m_sAboutText += sText;
		}
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------
// class qtractorVst3Plugin::ParamQueue -- VST3 plugin parameter queue impl.
//

class qtractorVst3Plugin::ParamQueue : public Vst::IParamValueQueue
{
public:

	// Constructor.
	ParamQueue (int32 nsize = 8)
		: m_id(Vst::kNoParamId), m_queue(nullptr), m_nsize(0), m_ncount(0)
		{ FUNKNOWN_CTOR	resize(nsize); }

	// Destructor.
	virtual ~ParamQueue ()
		{ resize(0); FUNKNOWN_DTOR }

	DECLARE_FUNKNOWN_METHODS

	//--- IParamValueQueue ---
	//
	Vst::ParamID PLUGIN_API getParameterId () override
		{ return m_id; }

	int32 PLUGIN_API getPointCount () override
		{ return m_ncount; }

	tresult PLUGIN_API getPoint (
		int32 index, int32& offset, Vst::ParamValue& value ) override
	{
		if (index < 0 || index >= m_ncount)
			return kResultFalse;

		const QueueItem& item = m_queue[index];
		offset = item.offset;
		value = item.value;
		return kResultOk;
	}

	tresult PLUGIN_API addPoint (
		int32 offset, Vst::ParamValue value, int32& index ) override
	{
		int32 i = 0;

		for ( ; i < m_ncount; ++i) {
			QueueItem& item = m_queue[i];
			if (item.offset > offset)
				break;
			if (item.offset == offset) {
				item.value = value;
				index = i;
				return kResultOk;
			}
		}

		if (i >= m_nsize)
			resize(m_nsize); // warning: non RT-safe!

		index = i;

		QueueItem& item = m_queue[index];
		item.value = value;
		item.offset = offset;
		i = m_ncount++;
		while (i > index) {
			QueueItem& item2 = m_queue[i];
			QueueItem& item1 = m_queue[--i];
			item2.value  = item1.value;
			item2.offset = item1.offset;
		}

		return kResultOk;
	}

	// Helper methods.
	//
	void setParameterId (Vst::ParamID id) { m_id = id; }

	void takeFrom (ParamQueue& queue)
	{
		m_id     = queue.m_id;
		m_queue  = queue.m_queue;
		m_nsize  = queue.m_nsize;
		m_ncount = queue.m_ncount;

		queue.m_id     = Vst::kNoParamId;
		queue.m_queue  = nullptr;
		queue.m_nsize  = 0;
		queue.m_ncount = 0;
	}

	void clear () { m_ncount = 0; }

protected:

	void resize (int32 nsize)
	{
		const int32 nsize2 = (nsize << 1);
		if (m_nsize != nsize2) {
			QueueItem *old_queue = m_queue;
			m_queue = nullptr;
			m_nsize = nsize2;
			if (m_nsize > 0) {
				m_queue = new QueueItem [m_nsize];
				if (m_ncount > m_nsize)
					m_ncount = m_nsize;
				for (int32 i = 0; old_queue && i < m_ncount; ++i)
					m_queue[i] = old_queue[i];
			}
			if (old_queue)
				delete [] old_queue;
		}
	}

private:

	// Instance members.
	struct QueueItem
	{
		QueueItem (Vst::ParamValue val = 0.0, int32 offs = 0)
			: value(val), offset(offs) {}

		Vst::ParamValue value;
		int32 offset;
	};

	Vst::ParamID m_id;
	QueueItem *m_queue;
	int32 m_nsize;
	volatile int32 m_ncount;
};

IMPLEMENT_FUNKNOWN_METHODS (qtractorVst3Plugin::ParamQueue, Vst::IParamValueQueue, Vst::IParamValueQueue::iid)


//----------------------------------------------------------------------
// class qtractorVst3Plugin::ParamChanges -- VST3 plugin parameter changes impl.
//

class qtractorVst3Plugin::ParamChanges : public Vst::IParameterChanges
{
public:

	// Constructor.
	ParamChanges (int32 nsize = 4)
		: m_queues(nullptr), m_nsize(0), m_ncount(0)
		{ FUNKNOWN_CTOR resize(nsize); }

	// Destructor.
	virtual ~ParamChanges ()
		{ resize(0); FUNKNOWN_DTOR }

	DECLARE_FUNKNOWN_METHODS

	//--- IParameterChanges ----
	//
	int32 PLUGIN_API getParameterCount () override
		{ return m_ncount; }

	Vst::IParamValueQueue *PLUGIN_API getParameterData (int32 index) override
	{
		if (index >= 0 && index < m_ncount)
			return &m_queues[index];
		else
			return nullptr;
	}

	Vst::IParamValueQueue *PLUGIN_API addParameterData (
		const Vst::ParamID& id, int32& index ) override
	{
		int32 i = 0;

		for ( ; i < m_ncount; ++i) {
			ParamQueue *queue = &m_queues[i];
			if (queue->getParameterId() == id) {
				index = i;
				return queue;
			}
		}

		if (i >= m_nsize)
			resize(m_nsize); // warning: non RT-safe!
		if (i >= m_ncount)
			++m_ncount;

		index = i;

		ParamQueue *queue = &m_queues[index];
		queue->setParameterId(id);
		return queue;
	}

	// Helper methods.
	//
	void clear ()
	{
		for (int32 i = 0; i < m_ncount; ++i)
			m_queues[i].clear();

		m_ncount = 0;
	}

protected:

	void resize (int32 nsize)
	{
		const int32 nsize2 = (nsize << 1);
		if (m_nsize != nsize2) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractorVst3Plugin::ParamChanges[%p]::resize(%d)", this, nsize);
		#endif
			ParamQueue *old_queues = m_queues;
			m_queues = nullptr;
			m_nsize = nsize2;
			if (m_nsize > 0) {
				m_queues = new ParamQueue [m_nsize];
				if (m_ncount > m_nsize)
					m_ncount = m_nsize;
				for (int32 i = 0; old_queues && i < m_ncount; ++i)
					m_queues[i].takeFrom(old_queues[i]);
			}
			if (old_queues)
				delete [] old_queues;
		}
	}

private:

	// Instance members.
	ParamQueue *m_queues;
	int32 m_nsize;
	volatile int32 m_ncount;
};

IMPLEMENT_FUNKNOWN_METHODS (qtractorVst3Plugin::ParamChanges, Vst::IParameterChanges, Vst::IParameterChanges::iid)


//----------------------------------------------------------------------
// class qtractorVst3Plugin::EventList -- VST3 plugin event list impl.
//

class qtractorVst3Plugin::EventList : public Vst::IEventList
{
public:

	// Constructor.
	EventList (uint32 nsize = 0x100)
		: m_events(nullptr), m_nsize(0), m_ncount(0)
		{ resize(nsize); FUNKNOWN_CTOR }

	// Destructor.
	virtual ~EventList ()
		{ resize(0); FUNKNOWN_DTOR }

	DECLARE_FUNKNOWN_METHODS

	//--- IEventList ---
	//
	int32 PLUGIN_API getEventCount () override
		{ return m_ncount; }

	tresult PLUGIN_API getEvent (int32 index, Vst::Event& event) override
	{
		if (index < 0 || index >= m_nsize)
			return kInvalidArgument;

		event = m_events[index];
		return kResultOk;
	}

	tresult PLUGIN_API addEvent (Vst::Event& event) override
	{
		if (m_ncount >= m_nsize)
			resize(m_nsize);  // warning: non RT-safe!

		m_events[m_ncount++] = event;
		return kResultOk;
	}

	// Helper methods.
	//
	void clear () { m_ncount = 0; }

protected:

	void resize (int32 nsize)
	{
		const int32 nsize2 = (nsize << 1);
		if (m_nsize != nsize2) {
		#ifdef CONFIG_DEBUG
			qDebug("qtractorVst3Plugin::EventList[%p]::resize(%d)", this, nsize);
		#endif
			Vst::Event *old_events = m_events;
			m_events = nullptr;
			m_nsize = nsize2;
			if (m_nsize > 0) {
				m_events = new Vst::Event [m_nsize];
				if (m_ncount > m_nsize)
					m_ncount = m_nsize;
				if (old_events)
					::memcpy(&m_events[0], old_events,
						sizeof(Vst::Event) * m_ncount);
				if (m_nsize > m_ncount)
					::memset(&m_events[m_ncount], 0,
						sizeof(Vst::Event) * (m_nsize - m_ncount));
			}
			if (old_events)
				delete [] old_events;
		}
	}

private:

	// Instance members.
	Vst::Event *m_events;
	int32 m_nsize;
	volatile int32 m_ncount;
};

IMPLEMENT_FUNKNOWN_METHODS (qtractorVst3Plugin::EventList, IEventList, IEventList::iid)


//----------------------------------------------------------------------
// class qtractorVst3Plugin::Impl -- VST3 plugin interface impl.
//

class qtractorVst3Plugin::Impl
{
public:

	// Constructor.
	Impl (qtractorVst3Plugin *pPlugin);

	// Destructor.
	~Impl ();

	// Do the actual (de)activation.
	void activate ();
	void deactivate ();

	// Editor controller methods.
	bool openEditor ();
	void closeEditor ();

	tresult notify (Vst::IMessage *message);

	IPlugView *plugView () const { return m_plugView; }

	// Audio processor methods.
	bool process_reset (qtractorAudioEngine *pAudioEngine);
	void process_midi_in (unsigned char *data, unsigned int size,
		unsigned long offset, unsigned short port);
	void process (float **ins, float **outs, unsigned int nframes);

	// Plugin current latency (in frames);
	unsigned long latency () const;

	// Set/add a parameter value/point.
	void setParameter (
		Vst::ParamID id, Vst::ParamValue value, uint32 offset);

	// Total parameter count.
	int32 parameterCount () const;

	// Get current parameter value.
	Vst::ParamValue getParameter (Vst::ParamID id) const;

	// Parameter info accessors.
	bool getParameterInfo (int32 index, Vst::ParameterInfo& paramInfo) const;

	// Program names list accessor.
	const QList<QString>& programs () const
		{ return m_programs; }

	// Program-change selector.
	void selectProgram (int iBank, int iProg);

	// Plugin preset/state snapshot accessors.
	bool setState (const QByteArray& data);
	bool getState (QByteArray& data);

	// MIDI event buffer accessors.
	EventList& events_in  () { return m_events_in;  }
	EventList& events_out () { return m_events_out; }

protected:

	// Plugin module initializer.
	void initialize ();

	// Cleanup.
	void clear ();

	// Channel/bus (de)activation helper method.
	void activate (Vst::IComponent *component,
		Vst::MediaType type, Vst::BusDirection direction, bool state);

private:

	// Instance variables.
	qtractorVst3Plugin *m_pPlugin;

	IPtr<Handler>   m_handler;
	IPtr<IPlugView> m_plugView;

	Vst::IAudioProcessor *m_processor;

	volatile bool m_processing;

	ParamChanges m_params_in;
//	ParamChanges m_params_out;

	EventList m_events_in;
	EventList m_events_out;

	// Processor buffers.
	Vst::AudioBusBuffers m_buffers_in;
	Vst::AudioBusBuffers m_buffers_out;

	// Processor data.
	Vst::ProcessData m_process_data;

	// Program-change parameter info.
	Vst::ParameterInfo m_programParamInfo;

	// Program name list.
	QList<QString> m_programs;

	// MIDI controller assignment hash key/map.
	struct MidiMapKey
	{
		MidiMapKey (int16 po = 0, int16 ch = 0, int16 co = 0)
			: port(po), channel(ch), controller(co) {}
		MidiMapKey (const MidiMapKey& key)
			: port(key.port), channel(key.channel), controller(key.controller) {}

		bool operator< (const MidiMapKey& key) const
		{
			if (port != key.port)
				return (port < key.port);
			else
			if (channel != key.channel)
				return (channel < key.channel);
			else
				return (controller < key.controller);
		}

		int16 port;
		int16 channel;
		int16 controller;
	};

	QMap<MidiMapKey, Vst::ParamID> m_midiMap;
};


//----------------------------------------------------------------------
// class qtractorVst3Plugin::Handler -- VST3 plugin interface handler.
//

class qtractorVst3Plugin::Handler
	: public Vst::IComponentHandler
	, public Vst::IConnectionPoint
{
public:

	// Constructor.
	Handler (qtractorVst3Plugin *pPlugin)
		: m_pPlugin(pPlugin) { FUNKNOWN_CTOR }

	// Destructor.
	virtual ~Handler () { FUNKNOWN_DTOR }

	DECLARE_FUNKNOWN_METHODS

	//--- IComponentHandler ---
	//
	tresult PLUGIN_API beginEdit (Vst::ParamID /*id*/) override
	{
	#ifdef CONFIG_DEBUG_0
		qDebug("vst3_text_plugin::Handler[%p]::beginEdit(%d)", this, int(id));
	#endif
		return kResultOk;
	}

	tresult PLUGIN_API performEdit (Vst::ParamID id, Vst::ParamValue value) override
	{
	#ifdef CONFIG_DEBUG
		qDebug("vst3_text_plugin::Handler[%p]::performEdit(%d, %g)", this, int(id), float(value));
	#endif
		m_pPlugin->impl()->setParameter(id, value, 0);
		qtractorPlugin::Param *pParam = m_pPlugin->findParamId(int(id));
		if (pParam)
			pParam->updateValue(float(value), false);
		return kResultOk;
	}

	tresult PLUGIN_API endEdit (Vst::ParamID /*id*/) override
	{
	#ifdef CONFIG_DEBUG_0
		qDebug("vst3_text_plugin::Handler[%p]::endEdit(%d)", this, int(id));
	#endif
		return kResultOk;
	}

	tresult PLUGIN_API restartComponent (int32 flags) override
	{
	#ifdef CONFIG_DEBUG
		qDebug("vst3_text_plugin::Handler[%p]::restartComponent(0x%08x)", this, flags);
	#endif
		if (flags & Vst::kParamValuesChanged)
			m_pPlugin->updateParamValues(false);
		else
		if (flags & Vst::kReloadComponent) {
			m_pPlugin->impl()->deactivate();
			m_pPlugin->impl()->activate();
		}
		return kResultOk;
	}

	//--- IConnectionPoint ---
	//
	tresult PLUGIN_API connect (Vst::IConnectionPoint *other) override
		{ return (other ? kResultOk : kInvalidArgument); }

	tresult PLUGIN_API disconnect (Vst::IConnectionPoint *other) override
		{ return (other ? kResultOk : kInvalidArgument); }

	tresult PLUGIN_API notify (Vst::IMessage *message) override
		{ return m_pPlugin->impl()->notify(message); }

private:

	// Instance client.
	qtractorVst3Plugin *m_pPlugin;
};


tresult PLUGIN_API qtractorVst3Plugin::Handler::queryInterface (
	const char *_iid, void **obj )
{
	QUERY_INTERFACE(_iid, obj, FUnknown::iid, IComponentHandler)
	QUERY_INTERFACE(_iid, obj, IComponentHandler::iid, IComponentHandler)
	QUERY_INTERFACE(_iid, obj, IConnectionPoint::iid, IConnectionPoint)

	*obj = nullptr;
	return kNoInterface;
}


uint32 PLUGIN_API qtractorVst3Plugin::Handler::addRef (void)
	{ return 1000;	}

uint32 PLUGIN_API qtractorVst3Plugin::Handler::release (void)
	{ return 1000; }


//----------------------------------------------------------------------
// class qtractorVst3Plugin::RunLoop -- VST3 plugin editor run-loop impl.
//

class qtractorVst3Plugin::RunLoop : public IRunLoop
{
public:

	//--- IRunLoop ---
	//
	tresult PLUGIN_API registerEventHandler (IEventHandler *handler, FileDescriptor fd) override
		{ return g_hostContext.registerEventHandler(handler, fd); }

	tresult PLUGIN_API unregisterEventHandler (IEventHandler *handler) override
		{ return g_hostContext.unregisterEventHandler(handler); }

	tresult PLUGIN_API registerTimer (ITimerHandler *handler, TimerInterval msecs) override
		{ return g_hostContext.registerTimer(handler, msecs); }

	tresult PLUGIN_API unregisterTimer (ITimerHandler *handler) override
		{ return g_hostContext.unregisterTimer(handler); }

	tresult PLUGIN_API queryInterface (const TUID _iid, void **obj) override
	{
		if (FUnknownPrivate::iidEqual(_iid, FUnknown::iid) ||
			FUnknownPrivate::iidEqual(_iid, IRunLoop::iid)) {
			addRef();
			*obj = this;
			return kResultOk;
		}

		*obj = nullptr;
		return kNoInterface;
	}

	uint32 PLUGIN_API addRef  () override { return 1001; }
	uint32 PLUGIN_API release () override { return 1001; }
};


//----------------------------------------------------------------------
// class qtractorVst3Plugin::EditorFrame -- VST3 plugin editor frame interface impl.
//

class qtractorVst3Plugin::EditorFrame : public IPlugFrame
{
public:

	// Constructor.
	EditorFrame (IPlugView *plugView, QWidget *widget)
		: m_plugView(plugView), m_widget(widget),
			m_runLoop(nullptr), m_resizing(false)
	{
		m_runLoop = owned(NEW RunLoop());
		m_plugView->setFrame(this);
	}

	// Destructor.
	virtual ~EditorFrame ()
	{
		m_plugView->setFrame(nullptr);
		m_runLoop = nullptr;
	}

	// Accessors.
	IPlugView *plugView () const
		{ return m_plugView; }
	RunLoop *runLoop () const
		{ return m_runLoop; }

	//--- IPlugFrame ---
	//
	tresult PLUGIN_API resizeView (IPlugView *plugView, ViewRect *rect) override
	{
		if (!rect || !plugView || plugView != m_plugView)
			return kInvalidArgument;

		if (!m_widget || m_resizing)
			return kResultFalse;

		m_resizing = true;
		const QSize size(
			rect->right  - rect->left,
			rect->bottom - rect->top);
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::EditorFrame[%p]::resizeView(%p, %p) size=(%d, %d)",
			this, plugView, rect, size.width(), size.height());
	#endif
		if (m_plugView->canResize() != kResultOk)
			m_widget->setFixedSize(size);
		else
			m_widget->resize(size);
		m_resizing = false;

		ViewRect rect0;
		if (m_plugView->getSize(&rect0) != kResultOk)
			return kInternalError;

		const QSize size0(
			rect0.right  - rect0.left,
			rect0.bottom - rect0.top);
		if (size != size0)
			m_plugView->onSize(rect);

		return kResultOk;
	}

	tresult PLUGIN_API queryInterface (const TUID _iid, void **obj) override
	{
		if (FUnknownPrivate::iidEqual(_iid, FUnknown::iid) ||
			FUnknownPrivate::iidEqual(_iid, IPlugFrame::iid)) {
			addRef();
			*obj = this;
			return kResultOk;
		}

		return m_runLoop->queryInterface(_iid, obj);
	}

	uint32 PLUGIN_API addRef  () override { return 1002; }
	uint32 PLUGIN_API release () override { return 1002; }

private:

	// Instance members.
	IPlugView *m_plugView;
	QWidget *m_widget;
	IPtr<RunLoop> m_runLoop;
	bool m_resizing;
};


//------------------------------------------------------------------------
// qtractorVst3Plugin::Stream - Memory based stream for IBStream impl.

class qtractorVst3Plugin::Stream : public IBStream
{
public:

	// Constructors.
	Stream () : m_pos(0)
		{ FUNKNOWN_CTOR }
	Stream (const QByteArray& data) : m_data(data), m_pos(0)
		{ FUNKNOWN_CTOR }

	// Destructor.
	virtual ~Stream () { FUNKNOWN_DTOR }

	DECLARE_FUNKNOWN_METHODS

	//--- IBStream ---
	//
	tresult PLUGIN_API read (void *buffer, int32 nbytes, int32 *nread) override
	{
		if (m_pos + nbytes > m_data.size()) {
			const int32 nsize = int32(m_data.size() - m_pos);
			if (nsize > 0) {
				nbytes = nsize;
			} else {
				nbytes = 0;
				m_pos = int64(m_data.size());
			}
		}

		if (nbytes > 0) {
			::memcpy(buffer, m_data.data() + m_pos, nbytes);
			m_pos += nbytes;
		}

		if (nread)
			*nread = nbytes;

		return kResultOk;
	}

	tresult PLUGIN_API write (void *buffer, int32 nbytes, int32 *nwrite) override
	{
		if (buffer == nullptr)
			return kInvalidArgument;

		const int32 nsize = m_pos + nbytes;
		if (nsize > m_data.size())
			m_data.resize(nsize);

		if (m_pos >= 0 && nbytes > 0) {
			::memcpy(m_data.data() + m_pos, buffer, nbytes);
			m_pos += nbytes;
		}
		else nbytes = 0;

		if (nwrite)
			*nwrite = nbytes;

		return kResultOk;
	}

	tresult PLUGIN_API seek (int64 pos, int32 mode, int64 *npos) override
	{
		if (mode == kIBSeekSet)
			m_pos = pos;
		else
		if (mode == kIBSeekCur)
			m_pos += pos;
		else
		if (mode == kIBSeekEnd)
			m_pos = m_data.size() - pos;

		if (m_pos < 0)
			m_pos = 0;
		else
		if (m_pos > m_data.size())
			m_pos = m_data.size();

		if (npos)
			*npos = m_pos;

		return kResultTrue;
	}

	tresult PLUGIN_API tell (int64 *npos) override
	{
		if (npos) {
			*npos = m_pos;
			return kResultOk;
		} else {
			return kInvalidArgument;
		}
	}

	// Other accessors.
	//
	const QByteArray& data() const { return m_data; }

protected:

	// Instance members.
	QByteArray m_data;
	int64      m_pos;
};

IMPLEMENT_FUNKNOWN_METHODS (qtractorVst3Plugin::Stream, IBStream, IBStream::iid)


//----------------------------------------------------------------------
// class qtractorVst3Plugin::Impl -- VST3 plugin interface impl.
//

// Constructor.
qtractorVst3Plugin::Impl::Impl ( qtractorVst3Plugin *pPlugin )
	: m_pPlugin(pPlugin), m_handler(nullptr), m_plugView(nullptr),
		m_processor(nullptr), m_processing(false)
{
	initialize();
}


// Destructor.
qtractorVst3Plugin::Impl::~Impl (void)
{
	closeEditor();

	deactivate();

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType) {
		Vst::IEditController *controller = pType->impl()->controller();
		if (controller)
			controller->setComponentHandler(nullptr);
	}

	m_processor = nullptr;
	m_handler = nullptr;

	clear();
}


// Plugin module initializer.
void qtractorVst3Plugin::Impl::initialize (void)
{
	clear();

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return;
#if 0//HACK: Plugin-type might be already open via plugin-factory...
	if (!pType->open())
		return;
#endif
	Vst::IComponent *component = pType->impl()->component();
	if (!component)
		return;

	Vst::IEditController *controller = pType->impl()->controller();
	if (controller) {
		m_handler = owned(NEW Handler(m_pPlugin));
		controller->setComponentHandler(m_handler);
	}

	m_processor = FUnknownPtr<Vst::IAudioProcessor> (component);

	if (controller) {
		const int32 nparams = controller->getParameterCount();
		for (int32 i = 0; i < nparams; ++i) {
			Vst::ParameterInfo paramInfo;
			::memset(&paramInfo, 0, sizeof(Vst::ParameterInfo));
			if (controller->getParameterInfo(i, paramInfo) == kResultOk) {
				if (m_programParamInfo.unitId != Vst::UnitID(-1))
					continue;
				if (paramInfo.flags & Vst::ParameterInfo::kIsProgramChange)
					m_programParamInfo = paramInfo;
			}
		}
		if (m_programParamInfo.unitId != Vst::UnitID(-1)) {
			Vst::IUnitInfo *unitInfos = pType->impl()->unitInfos();
			if (unitInfos) {
				const int32 nunits = unitInfos->getUnitCount();
				for (int32 i = 0; i < nunits; ++i) {
					Vst::UnitInfo unitInfo;
					if (unitInfos->getUnitInfo(i, unitInfo) != kResultOk)
						continue;
					if (unitInfo.id != m_programParamInfo.unitId)
						continue;
					const int32 nlists = unitInfos->getProgramListCount();
					for (int32 j = 0; j < nlists; ++j) {
						Vst::ProgramListInfo programListInfo;
						if (unitInfos->getProgramListInfo(j, programListInfo) != kResultOk)
							continue;
						if (programListInfo.id != unitInfo.programListId)
							continue;
						const int32 nprograms = programListInfo.programCount;
						for (int32 k = 0; k < nprograms; ++k) {
							Vst::String128 name;
							if (unitInfos->getProgramName(
									programListInfo.id, k, name) == kResultOk)
								m_programs.append(fromTChar(name));
						}
						break;
					}
				}
			}
		}
		if (m_programs.isEmpty() && m_programParamInfo.stepCount > 0) {
			const int32 nprograms = m_programParamInfo.stepCount + 1;
			for (int32 k = 0; k < nprograms; ++k) {
				const Vst::ParamValue value
					= Vst::ParamValue(k)
					/ Vst::ParamValue(m_programParamInfo.stepCount);
				Vst::String128 name;
				if (controller->getParamStringByValue(
						m_programParamInfo.id, value, name) == kResultOk)
					m_programs.append(fromTChar(name));
			}
		}
	}

	if (controller) {
		const int32 nports = pType->midiIns();
		FUnknownPtr<Vst::IMidiMapping> midiMapping(controller);
		if (midiMapping && nports > 0) {
			for (int16 i = 0; i < Vst::kCountCtrlNumber; ++i) { // controllers...
				for (int32 j = 0; j < nports; ++j) { // ports...
					for (int16 k = 0; k < 16; ++k) { // channels...
						Vst::ParamID id = Vst::kNoParamId;
						if (midiMapping->getMidiControllerAssignment(
								j, k, Vst::CtrlNumber(i), id) == kResultOk) {
							m_midiMap.insert(MidiMapKey(j, k, i), id);
						}
					}
				}
			}
		}
	}
}


// Do the actual (de)activation.
void qtractorVst3Plugin::Impl::activate (void)
{
	if (m_processing)
		return;

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return;

	Vst::IComponent *component = pType->impl()->component();
	if (component && m_processor) {
		activate(component, Vst::kAudio, Vst::kInput,  true);
		activate(component, Vst::kAudio, Vst::kOutput, true);
		activate(component, Vst::kEvent, Vst::kInput,  true);
		activate(component, Vst::kEvent, Vst::kOutput, true);
		component->setActive(true);
		m_processor->setProcessing(true);
		g_hostContext.processAddRef();
		m_processing = true;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin::Impl[%p]::activate() processing=%d", this, int(m_processing));
#endif
}


void qtractorVst3Plugin::Impl::deactivate (void)
{
	if (!m_processing)
		return;

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return;

	Vst::IComponent *component = pType->impl()->component();
	if (component && m_processor) {
		g_hostContext.processReleaseRef();
		m_processor->setProcessing(false);
		component->setActive(false);
		m_processing = false;
		activate(component, Vst::kEvent, Vst::kOutput, false);
		activate(component, Vst::kEvent, Vst::kInput,  false);
		activate(component, Vst::kAudio, Vst::kOutput, false);
		activate(component, Vst::kAudio, Vst::kInput,  false);
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin::Impl[%p]::deactivate() processing=%d", this, int(m_processing));
#endif
}


// Editor controller methods.
bool qtractorVst3Plugin::Impl::openEditor (void)
{
	closeEditor();

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return false;

#ifdef CONFIG_VST3_XCB
	g_hostContext.openXcbConnection();
#endif

	g_hostContext.startTimer(200);

	Vst::IEditController *controller = pType->impl()->controller();
	if (controller)
		m_plugView = owned(controller->createView(Vst::ViewType::kEditor));

	return (m_plugView != nullptr);
}


void qtractorVst3Plugin::Impl::closeEditor (void)
{
	m_plugView = nullptr;

	g_hostContext.stopTimer();

#ifdef CONFIG_VST3_XCB
	g_hostContext.closeXcbConnection();
#endif
}


tresult qtractorVst3Plugin::Impl::notify ( Vst::IMessage *message )
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return kInternalError;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin::Impl[%p]::notify(%p)", this, message);
#endif

	Vst::IComponent *component = pType->impl()->component();
	FUnknownPtr<Vst::IConnectionPoint> component_cp(component);
	if (component_cp)
		component_cp->notify(message);

	Vst::IEditController *controller = pType->impl()->controller();
	FUnknownPtr<Vst::IConnectionPoint> controller_cp(controller);
	if (controller_cp)
		controller_cp->notify(message);

	return kResultOk;
}


// Audio processor stuff (TODO)...
bool qtractorVst3Plugin::Impl::process_reset (
	qtractorAudioEngine *pAudioEngine )
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return false;

	if (!m_processor)
		return false;

//	deactivate();

	// Initialize running state...
	m_params_in.clear();
//	m_params_out.clear();

	m_events_in.clear();
	m_events_out.clear();

	const bool         bFreewheel  = pAudioEngine->isFreewheel();
	const unsigned int iSampleRate = pAudioEngine->sampleRate();
	const unsigned int iBufferSize = pAudioEngine->bufferSize();
	const unsigned int iBufferSizeEx = pAudioEngine->bufferSizeEx();

	Vst::ProcessSetup setup;
	setup.processMode        = (bFreewheel ? Vst::kOffline :  Vst::kRealtime);
	setup.symbolicSampleSize = Vst::kSample32;
	setup.maxSamplesPerBlock = iBufferSizeEx;
	setup.sampleRate         = float(iSampleRate);

	if (m_processor->setupProcessing(setup) != kResultOk)
		return false;

	// Setup processor audio I/O buffers...
	m_buffers_in.silenceFlags      = 0;
	m_buffers_in.numChannels       = pType->audioIns();
	m_buffers_in.channelBuffers32  = nullptr;

	m_buffers_out.silenceFlags     = 0;
	m_buffers_out.numChannels      = pType->audioOuts();
	m_buffers_out.channelBuffers32 = nullptr;

	// Setup processor data struct...
	m_process_data.numSamples             = iBufferSize;
	m_process_data.symbolicSampleSize     = Vst::kSample32;

	if (pType->audioIns() > 0) {
		m_process_data.numInputs          = 1;
		m_process_data.inputs             = &m_buffers_in;
	} else {
		m_process_data.numInputs          = 0;
		m_process_data.inputs             = nullptr;
	}

	if (pType->audioOuts() > 0) {
		m_process_data.numOutputs         = 1;
		m_process_data.outputs            = &m_buffers_out;
	} else {
		m_process_data.numOutputs         = 0;
		m_process_data.outputs            = nullptr;
	}

	m_process_data.processContext         = g_hostContext.processContext();
	m_process_data.inputEvents            = &m_events_in;
	m_process_data.outputEvents           = &m_events_out;
	m_process_data.inputParameterChanges  = &m_params_in;
	m_process_data.outputParameterChanges = nullptr; //&m_params_out;

//	activate();

	return true;
}


void qtractorVst3Plugin::Impl::process_midi_in (
	unsigned char *data, unsigned int size,
	unsigned long offset, unsigned short port )
{
	for (uint32_t i = 0; i < size; ++i) {

		// channel status
		const int channel = (data[i] & 0x0f);// + 1;
		const int status  = (data[i] & 0xf0);

		// all system common/real-time ignored
		if (status == 0xf0)
			continue;

		// check data size (#1)
		if (++i >= size)
			break;

		// channel key
		const int key = (data[i] & 0x7f);

		// program change
		if (status == 0xc0) {
			// TODO: program-change...
			continue;
		}

		// after-touch
		if (status == 0xd0) {
			const MidiMapKey mkey(port, channel, Vst::kAfterTouch);
			const Vst::ParamID id = m_midiMap.value(mkey, Vst::kNoParamId);
			if (id != Vst::kNoParamId) {
				const float pre = float(key) / 127.0f;
				setParameter(id, Vst::ParamValue(pre), offset);
			}
			continue;
		}

		// check data size (#2)
		if (++i >= size)
			break;

		// channel value (normalized)
		const int value = (data[i] & 0x7f);

		Vst::Event event;
		::memset(&event, 0, sizeof(Vst::Event));
		event.busIndex = port;
		event.sampleOffset = offset;
		event.flags = Vst::Event::kIsLive;

		// note on
		if (status == 0x90) {
			event.type = Vst::Event::kNoteOnEvent;
			event.noteOn.noteId = -1;
			event.noteOn.channel = channel;
			event.noteOn.pitch = key;
			event.noteOn.velocity = float(value) / 127.0f;
			m_events_in.addEvent(event);
		}
		// note off
		else if (status == 0x80) {
			event.type = Vst::Event::kNoteOffEvent;
			event.noteOff.noteId = -1;
			event.noteOff.channel = channel;
			event.noteOff.pitch = key;
			event.noteOff.velocity = float(value) / 127.0f;
			m_events_in.addEvent(event);
		}
		// key pressure/poly.aftertouch
		else if (status == 0xa0) {
			event.type = Vst::Event::kPolyPressureEvent;
			event.polyPressure.channel = channel;
			event.polyPressure.pitch = key;
			event.polyPressure.pressure = float(value) / 127.0f;
			m_events_in.addEvent(event);
		}
		// control-change
		else if (status == 0xb0) {
			const MidiMapKey mkey(port, channel, key);
			const Vst::ParamID id = m_midiMap.value(mkey, Vst::kNoParamId);
			if (id != Vst::kNoParamId) {
				const float val = float(value) / 127.0f;
				setParameter(id, Vst::ParamValue(val), offset);
			}
		}
		// pitch-bend
		else if (status == 0xe0) {
			const MidiMapKey mkey(port, channel, Vst::kPitchBend);
			const Vst::ParamID id = m_midiMap.value(mkey, Vst::kNoParamId);
			if (id != Vst::kNoParamId) {
				const float pitchbend
					= float(key + (value << 7)) / float(0x3fff);
				setParameter(id, Vst::ParamValue(pitchbend), offset);
			}
		}
	}
}


void qtractorVst3Plugin::Impl::process (
	float **ins, float **outs, unsigned int nframes )
{
	if (!m_processor)
		return;

	if (!m_processing)
		return;

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return;

//	m_params_out.clear();
	m_events_out.clear();

	m_buffers_in.channelBuffers32 = ins;
	m_buffers_out.channelBuffers32 = outs;
	m_process_data.numSamples = nframes;

	if (m_processor->process(m_process_data) != kResultOk) {
		qWarning("qtractorVst3Plugin::Impl[%p]::process() FAILED!", this);
	}

	m_events_in.clear();
	m_params_in.clear();
}


// Plugin current latency (in frames);
unsigned long qtractorVst3Plugin::Impl::latency (void) const
{
	if (m_processor)
		return m_processor->getLatencySamples();
	else
		return 0;
}


// Set/add a parameter value/point.
void qtractorVst3Plugin::Impl::setParameter (
	Vst::ParamID id, Vst::ParamValue value, uint32 offset )
{
	int32 index = 0;
	Vst::IParamValueQueue *queue = m_params_in.addParameterData(id, index);
	if (queue && (queue->addPoint(offset, value, index) != kResultOk)) {
		qWarning("qtractorVst3Plugin::Impl[%p]::setParameter(%u, %g, %u) FAILED!",
			this, id, value, offset);
	}
}


// Get current parameter value.
Vst::ParamValue qtractorVst3Plugin::Impl::getParameter ( Vst::ParamID id ) const
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return 0.0;

	Vst::IEditController *controller = pType->impl()->controller();
	if (controller)
		return controller->getParamNormalized(id);
	else
		return 0.0;
}


// Total parameter count.
int32 qtractorVst3Plugin::Impl::parameterCount (void) const
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return 0;

	Vst::IEditController *controller = pType->impl()->controller();
	if (controller)
		return controller->getParameterCount();
	else
		return 0;
}


// Parameter info accessor
bool qtractorVst3Plugin::Impl::getParameterInfo (
	int32 index, Vst::ParameterInfo& paramInfo ) const
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return false;

	Vst::IEditController *controller = pType->impl()->controller();
	if (controller)
		return (controller->getParameterInfo(index, paramInfo) == kResultOk);
	else
		return false;
}


// Program-change selector.
void qtractorVst3Plugin::Impl::selectProgram ( int iBank, int iProg )
{
	if (iBank < 0 || iProg < 0)
		return;

	const Vst::ParamID id = m_programParamInfo.id;
	if (id == Vst::kNoParamId)
		return;

	if (m_programParamInfo.stepCount < 1)
		return;

	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return;

	Vst::IEditController *controller = pType->impl()->controller();
	if (!controller)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin::Impl[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	int iIndex = 0;
	if (iBank >= 0 && iProg >= 0)
		iIndex = (iBank << 7) + iProg;

	const Vst::ParamValue value
		= controller->plainParamToNormalized(id, float(iIndex));
//		= Vst::ParamValue(iIndex)
//		/ Vst::ParamValue(m_programParamInfo.stepCount);
	setParameter(id, value, 0);
	controller->setParamNormalized(id, value);
}


// Plugin preset/state snapshot accessors.
bool qtractorVst3Plugin::Impl::setState ( const QByteArray& data )
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return false;

	Vst::IComponent *component = pType->impl()->component();
	if (!component)
		return false;

	Vst::IEditController *controller = pType->impl()->controller();
	if (!controller)
		return false;

	Stream state(data);

	if (component->setState(&state) != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::Impl[%p]::setState()"
			" IComponent::setState() FAILED!", this);
	#endif
		return false;
	}

	if (controller->setComponentState(&state) != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::Impl[%p]::setState()"
			" IEditController::setComponentState() FAILED!", this);
	#endif
		return false;
	}

	return true;
}


bool qtractorVst3Plugin::Impl::getState ( QByteArray& data )
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return false;

	Vst::IComponent *component = pType->impl()->component();
	if (!component)
		return false;

	Stream state;

	if (component->getState(&state) != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::Impl[%p]::getState()"
			" Vst::IComponent::getState() FAILED!", this);
	#endif
		return false;
	}

	data = state.data();
	return true;
}


// Cleanup.
void qtractorVst3Plugin::Impl::clear (void)
{
	::memset(&m_programParamInfo, 0, sizeof(Vst::ParameterInfo));
	m_programParamInfo.id = Vst::kNoParamId;
	m_programParamInfo.unitId = Vst::UnitID(-1);
	m_programs.clear();

	m_midiMap.clear();
}


void qtractorVst3Plugin::Impl::activate ( Vst::IComponent *component,
	Vst::MediaType type, Vst::BusDirection direction, bool state )
{
	const int32 nbuses = component->getBusCount(type, direction);
	for (int32 i = 0; i < nbuses; ++i) {
		Vst::BusInfo busInfo;
		if (component->getBusInfo(type, direction, i, busInfo) == kResultOk) {
			if (busInfo.flags & Vst::BusInfo::kDefaultActive) {
				component->activateBus(type, direction, i, state);
			}
		}
	}
}


//----------------------------------------------------------------------
// class qtractorVst3Plugin::EditorWidget -- VST3 plugin editor widget decl.
//


class qtractorVst3Plugin::EditorWidget : public QWidget
{
public:

	EditorWidget(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags());

	~EditorWidget ();

	void setPlugin (qtractorVst3Plugin *pPlugin)
		{ m_pPlugin = pPlugin; }
	qtractorVst3Plugin *plugin () const
		{ return m_pPlugin; }

	WId parentWinId() const { return QWidget::winId(); }

protected:

	void resizeEvent(QResizeEvent *pResizeEvent);
	void showEvent(QShowEvent *pShowEvent);
	void closeEvent(QCloseEvent *pCloseEvent);

private:

	qtractorVst3Plugin *m_pPlugin;
	bool m_resizing;
};


//----------------------------------------------------------------------
// class qtractorVst3Plugin::EditorWidget -- VST3 plugin editor widget impl.
//

qtractorVst3Plugin::EditorWidget::EditorWidget (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags), m_pPlugin(nullptr), m_resizing(false)
{
}


qtractorVst3Plugin::EditorWidget::~EditorWidget (void)
{
}


void qtractorVst3Plugin::EditorWidget::resizeEvent ( QResizeEvent *pResizeEvent )
{
	if (m_resizing)
		return;

	IPlugView *plugView = nullptr;
	EditorFrame *pEditorFrame = nullptr;
	if (m_pPlugin)
		pEditorFrame = m_pPlugin->editorFrame();
	if (pEditorFrame)
		plugView = pEditorFrame->plugView();
	if (plugView && plugView->canResize() == kResultOk) {
		const QSize& size = pResizeEvent->size();
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::EditorWidget[%p]::resizeEvent(%d, %d)",
			this, size.width(), size.height());
	#endif
		ViewRect rect;
		rect.left   = 0;
		rect.top    = 0;
		rect.right  = size.width();
		rect.bottom = size.height();
		if (plugView->checkSizeConstraint(&rect) == kResultOk) {
			const QSize size2(
				rect.right - rect.left,
				rect.bottom - rect.top);
			if (size2 != size) {
				m_resizing = true;
				QWidget::resize(size2);
				m_resizing = false;
			}
		}
		plugView->onSize(&rect);
	}
}


void qtractorVst3Plugin::EditorWidget::showEvent ( QShowEvent *pShowEvent )
{
	QWidget::showEvent(pShowEvent);

	if (m_pPlugin)
		m_pPlugin->toggleFormEditor(true);
}


void qtractorVst3Plugin::EditorWidget::closeEvent ( QCloseEvent *pCloseEvent )
{
	if (m_pPlugin)
		m_pPlugin->toggleFormEditor(false);

	QWidget::closeEvent(pCloseEvent);

	if (m_pPlugin)
		m_pPlugin->closeEditor();
}


//----------------------------------------------------------------------------
// qtractorVst3Plugin::Param::Impl -- VST3 plugin parameter interface impl.
//

class qtractorVst3Plugin::Param::Impl
{
public:

	// Constructor.
	Impl(const Vst::ParameterInfo& paramInfo)
		: m_paramInfo(paramInfo) {}

	// Accessors.
	const Vst::ParameterInfo& paramInfo() const
		{ return m_paramInfo; }

private:

	// Instance members.
	Vst::ParameterInfo m_paramInfo;
};


//----------------------------------------------------------------------
// class qtractorVst3Plugin -- VST3 plugin interface impl.
//

// Buffer size large enough to hold a regular MIDI channel event.
const long c_iMaxMidiData = 4;

// Constructor.
qtractorVst3Plugin::qtractorVst3Plugin (
	qtractorPluginList *pList, qtractorVst3PluginType *pType )
	: qtractorPlugin(pList, pType), m_pImpl(new Impl(this)),
		m_pEditorFrame(nullptr), m_pEditorWidget(nullptr),
		m_ppIBuffer(nullptr), m_ppOBuffer(nullptr),
		m_pfIDummy(nullptr), m_pfODummy(nullptr),
		m_pMidiParser(nullptr)
{
	initialize();
}


// Destructor.
qtractorVst3Plugin::~qtractorVst3Plugin (void)
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

	// Deallocate MIDI decoder.
	if (m_pMidiParser)
		snd_midi_event_free(m_pMidiParser);

	delete m_pImpl;
}


// Plugin instance initializer.
void qtractorVst3Plugin::initialize (void)
{
	// Allocate I/O audio buffer pointers.
	const unsigned short iAudioIns  = audioIns();
	const unsigned short iAudioOuts = audioOuts();
	if (iAudioIns > 0)
		m_ppIBuffer = new float * [iAudioIns];
	if (iAudioOuts > 0)
		m_ppOBuffer = new float * [iAudioOuts];

	const int32 nparams = m_pImpl->parameterCount();
#if CONFIG_DEBUG
	qDebug(" --- Parameters (nparams = %d) ---", nparams);
#endif
	for (int32 i = 0; i < nparams; ++i) {
		Vst::ParameterInfo paramInfo;
		::memset(&paramInfo, 0, sizeof(Vst::ParameterInfo));
		if (m_pImpl->getParameterInfo(i, paramInfo)) {
			if ( (paramInfo.flags & Vst::ParameterInfo::kCanAutomate) &&
				!(paramInfo.flags & Vst::ParameterInfo::kIsReadOnly)) {
				Param *pParam = new Param(this, i);
				m_paramIds.insert(int(paramInfo.id), pParam);
				addParam(pParam);
			}
		}
	}

	if (midiIns() > 0 &&
		snd_midi_event_new(c_iMaxMidiData, &m_pMidiParser) == 0)
		snd_midi_event_no_status(m_pMidiParser, 1);

	// Instantiate each instance properly...
	setChannels(channels());
}


// Channel/instance number accessors.
void qtractorVst3Plugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (type());
	if (pType == nullptr)
		return;

	// Estimate the (new) number of instances...
	const unsigned short iOldInstances = instances();
	unsigned short iInstances = 0;
	if (iChannels > 0) {
		iInstances = pType->instances(iChannels, list()->isMidi());
		// Now see if instance and channel count changed anyhow...
		if (iInstances == iOldInstances && iChannels == channels())
			return;
	}

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setChannelsActivated(iChannels, false);

	// Set new instance number...
	setInstances(iInstances);

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setChannelsActivated(iChannels, bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	const unsigned int iBufferSizeEx = pAudioEngine->bufferSizeEx();

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

	if (m_pMidiParser)
		snd_midi_event_reset_decode(m_pMidiParser);

	// Setup all those instances alright...
	m_pImpl->process_reset(pAudioEngine);

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// (Re)activate instance if necessary...
	setChannelsActivated(iChannels, bActivated);
}


// Do the actual (de)activation.
void qtractorVst3Plugin::activate (void)
{
	m_pImpl->activate();
}


void qtractorVst3Plugin::deactivate (void)
{
	m_pImpl->deactivate();
}


// Parameter update method.
void qtractorVst3Plugin::updateParam (
	qtractorPlugin::Param *pParam, float fValue, bool /*bUpdate*/ )
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (type());
	if (pType == nullptr)
		return;

	Vst::IEditController *controller = pType->impl()->controller();
	if (!controller)
		return;

	Param *pVst3Param = static_cast<Param *> (pParam);
	if (pVst3Param == nullptr)
		return;
	if (pVst3Param->impl() == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVst3Plugin[%p]::updateParam(%lu, %g, %d)",
		this, pParam->index(), fValue, int(bUpdate));
#endif

	const Vst::ParamID id = pVst3Param->impl()->paramInfo().id;
	const Vst::ParamValue value = Vst::ParamValue(fValue);
	m_pImpl->setParameter(id, value, 0);
	controller->setParamNormalized(id, value);
}


// All parameters update method.
void qtractorVst3Plugin::updateParamValues ( bool bUpdate )
{
	// Make sure all cached parameter values are in sync
	// with plugin parameter values; update cache otherwise.
	const qtractorPlugin::Params& params = qtractorPlugin::params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		Param *pParam = static_cast<Param *> (param.value());
		if (pParam && pParam->impl()) {
			Vst::ParamID id = pParam->impl()->paramInfo().id;
			const float fValue = float(m_pImpl->getParameter(id));
			if (pParam->value() != fValue) {
				pParam->setValue(fValue, bUpdate);
				updateFormDirtyCount();
			}
		}
	}
}


// Parameter finder (by id).
qtractorPlugin::Param *qtractorVst3Plugin::findParamId ( int id ) const
{
	return m_paramIds.value(id, nullptr);
}


// Configuration state stuff.
void qtractorVst3Plugin::configure (
	const QString& sKey, const QString& sValue )
{
	if (sKey == "state") {
		// Load the BLOB (base64 encoded)...
		const QByteArray data
			= qUncompress(QByteArray::fromBase64(sValue.toLatin1()));
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin[%p]::configure() data.size=%d", this, int(data.size()));
	#endif
		m_pImpl->setState(data);
	}
}


// Plugin configuration/state snapshot.
void qtractorVst3Plugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorVst3Plugin[%p]::freezeConfigs()", this);
#endif

	// HACK: Make sure all parameter values are in sync,
	// provided freezeConfigs() are always called when
	// saving plugin's state and before parameter values.
	updateParamValues(false);

	// Update current editor position...
	if (m_pEditorWidget && m_pEditorWidget->isVisible())
		setEditorPos(m_pEditorWidget->pos());

	if (!type()->isConfigure())
		return;

	QByteArray data;

	if (!m_pImpl->getState(data))
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVstPlugin[%p]::freezeConfigs() chunk.size=%d", this, int(data.size()));
#endif

	// Set special plugin configuration item (base64 encoded)...
	QByteArray cdata = qCompress(data).toBase64();
	for (int i = cdata.size() - (cdata.size() % 72); i >= 0; i -= 72)
		cdata.insert(i, "\n       "); // Indentation.
	setConfig("state", cdata.constData());
}


void qtractorVst3Plugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin[%p]::releaseConfigs()", this);
#endif

	qtractorPlugin::clearConfigs();
}



// Open/close editor widget.
void qtractorVst3Plugin::openEditor ( QWidget *pParent )
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (type());
	if (pType == nullptr)
		return;

	if (!pType->isEditor())
		return;

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
	qDebug("qtractorVst3Plugin[%p]::openEditor(%p)", this, pParent);
#endif

	if (!m_pImpl->openEditor())
		return;

	IPlugView *plugView = m_pImpl->plugView();
	if (!plugView)
		return;

	if (plugView->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID) != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::[%p]::openEditor(%p)"
			" *** X11 Window platform is not supported (%s).", this, pParent,
			kPlatformTypeX11EmbedWindowID);
	#endif
		return;
	}

	m_pEditorWidget = new EditorWidget(pParent, Qt::Window);
	m_pEditorWidget->setAttribute(Qt::WA_QuitOnClose, false);
	m_pEditorWidget->setWindowTitle(pType->name());
	m_pEditorWidget->setPlugin(this);

	m_pEditorFrame = new EditorFrame(plugView, m_pEditorWidget);

	void *wid = (void *) m_pEditorWidget->parentWinId();
	if (plugView->attached(wid, kPlatformTypeX11EmbedWindowID) != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::[%p]::openEditor(%p)"
			" *** Failed to create/attach editor window.", this, pParent);
	#endif
		closeEditor();
		return;
	}

	// Final stabilization...
	updateEditorTitle();
	moveWidgetPos(m_pEditorWidget, editorPos());
	setEditorVisible(true);
}


void qtractorVst3Plugin::closeEditor (void)
{
	if (m_pEditorWidget == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

	IPlugView *plugView = m_pImpl->plugView();
	if (plugView && plugView->removed() != kResultOk) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin::[%p]::closeEditor()"
			" *** Failed to remove/detach window.", this);
	#endif
	}

	delete m_pEditorWidget;
	m_pEditorWidget = nullptr;

	if (m_pEditorFrame) {
		delete m_pEditorFrame;
		m_pEditorFrame = nullptr;
	}

	m_pImpl->closeEditor();
}


// GUI editor visibility state.
void qtractorVst3Plugin::setEditorVisible ( bool bVisible )
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


bool qtractorVst3Plugin::isEditorVisible (void) const
{
	return (m_pEditorWidget ? m_pEditorWidget->isVisible() : false);
}


// Update editor widget caption.
void qtractorVst3Plugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	if (m_pEditorWidget)
		m_pEditorWidget->setWindowTitle(sTitle);
}


// Our own editor widget/frame accessors.
QWidget *qtractorVst3Plugin::editorWidget (void) const
{
	return m_pEditorWidget;
}

qtractorVst3Plugin::EditorFrame *qtractorVst3Plugin::editorFrame (void) const
{
	return m_pEditorFrame;
}


// Processor stuff...
//

void qtractorVst3Plugin::process_midi_in (
	unsigned char *data, unsigned int size,
	unsigned long offset, unsigned short port )
{
	m_pImpl->process_midi_in(data, size, offset, port);
}


void qtractorVst3Plugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	// To process MIDI events, if any...
	qtractorMidiManager *pMidiManager = nullptr;
	const unsigned short iMidiIns  = midiIns();
	const unsigned short iMidiOuts = midiOuts();
	if (iMidiIns > 0 || iMidiOuts > 0)
		pMidiManager = list()->midiManager();

	// Process MIDI input stream, if any...
	if (pMidiManager && m_pMidiParser) {
		qtractorMidiBuffer *pMidiBuffer = pMidiManager->buffer_in();
		const unsigned int iEventCount = pMidiBuffer->count();
		for (unsigned int i = 0; i < iEventCount; ++i) {
			snd_seq_event_t *pEv = pMidiBuffer->at(i);
			unsigned char midiData[c_iMaxMidiData];
			unsigned char *pMidiData = &midiData[0];
			long iMidiData = sizeof(midiData);
			iMidiData = snd_midi_event_decode(m_pMidiParser,
				pMidiData, iMidiData, pEv);
			if (iMidiData < 0)
				break;
			m_pImpl->process_midi_in(pMidiData, iMidiData, pEv->time.tick, 0);
		}
	}

	const unsigned short iChannels  = channels();
	const unsigned short iAudioIns  = audioIns();
	const unsigned short iAudioOuts = audioOuts();

	unsigned short iIChannel = 0;
	unsigned short iOChannel = 0;
	unsigned short i;

	// For each audio input port...
	for (i = 0; i < iAudioIns; ++i) {
		if (iIChannel < iChannels)
			m_ppIBuffer[i] = ppIBuffer[iIChannel++];
		else
			m_ppIBuffer[i] = m_pfIDummy; // dummy input!
	}

	// For each audio output port...
	for (i = 0; i < iAudioOuts; ++i) {
		if (iOChannel < iChannels)
			m_ppOBuffer[i] = ppOBuffer[iOChannel++];
		else
			m_ppOBuffer[i] = m_pfODummy; // dummy output!
	}

	// Run the main processor routine...
	//
	m_pImpl->process(m_ppIBuffer, m_ppOBuffer, nframes);

	// Wrap dangling output channels?...
	for (i = iOChannel; i < iChannels; ++i)
		::memset(ppOBuffer[i], 0, nframes * sizeof(float));

	// Process MIDI output stream, if any...
	if (pMidiManager) {
		if (iMidiOuts > 0) {
			qtractorMidiBuffer *pMidiBuffer = pMidiManager->buffer_out();
			EventList& events_out = m_pImpl->events_out();
			const int32 nevents = events_out.getEventCount();
			for (int32 i = 0; i < nevents; ++i) {
				Vst::Event event;
				if (events_out.getEvent(i, event) == kResultOk) {
					snd_seq_event_t ev;
					snd_seq_ev_clear(&ev);
					switch (event.type) {
					case Vst::Event::kNoteOnEvent:
						ev.type = SND_SEQ_EVENT_NOTEON;
						ev.data.note.channel  = event.noteOn.channel;
						ev.data.note.note     = event.noteOn.pitch;
						ev.data.note.velocity = event.noteOn.velocity;
						break;
					case Vst::Event::kNoteOffEvent:
						ev.type = SND_SEQ_EVENT_NOTEOFF;
						ev.data.note.channel  = event.noteOff.channel;
						ev.data.note.note     = event.noteOff.pitch;
						ev.data.note.velocity = event.noteOff.velocity;
						break;
					case Vst::Event::kPolyPressureEvent:
						ev.type = SND_SEQ_EVENT_KEYPRESS;
						ev.data.note.channel  = event.polyPressure.channel;
						ev.data.note.note     = event.polyPressure.pitch;
						ev.data.note.velocity = event.polyPressure.pressure;
						break;
					}
					if (ev.type != SND_SEQ_EVENT_NONE)
						pMidiBuffer->push(&ev, event.sampleOffset);
				}
			}
			pMidiManager->vst3_buffer_swap();
		} else {
			pMidiManager->resetOutputBuffers();
		}
	}
}


// Plugin current latency (in frames);
unsigned long qtractorVst3Plugin::latency (void) const
{
	return m_pImpl->latency();
}


// Provisional program/patch accessor.
bool qtractorVst3Plugin::getProgram ( int iIndex, Program& program ) const
{
	const QList<QString>& programs
		= m_pImpl->programs();

	if (iIndex < 0 || iIndex >= programs.count())
		return false;

	// Map this to that...
	program.bank = 0;
	program.prog = iIndex;
	program.name = programs.at(iIndex);

	return true;
}

// Specific MIDI instrument selector.
void qtractorVst3Plugin::selectProgram ( int iBank, int iProg )
{
	// HACK: We don't change program-preset when
	// we're supposed to be multi-timbral...
	if (list()->isMidiBus())
		return;

	m_pImpl->selectProgram(iBank, iProg);

	// HACK: Make sure all displayed parameter values are in sync.
	updateParamValues(false);
}


// Plugin preset i/o (configuration from/to state files).
bool qtractorVst3Plugin::loadPresetFile ( const QString& sFilename )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "qtx")
		return qtractorPlugin::loadPresetFile(sFilename);

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin[%p]::loadPresetFile(\"%s\")",
		this, sFilename.toUtf8().constData());
#endif

	QFile file(sFilename);

	if (!file.open(QFile::ReadOnly)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin[%p]::loadPresetFile(\"%s\")"
			" QFile::open(QFile::ReadOnly) FAILED!",
			this, sFilename.toUtf8().constData());
	#endif
		return false;
	}

	const bool bResult
		= m_pImpl->setState(file.readAll());

	file.close();
	// HACK: Make sure all displayed parameter values are in sync.
	updateParamValues(false);
	return bResult;
}


bool qtractorVst3Plugin::savePresetFile ( const QString& sFilename )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "qtx")
		return qtractorPlugin::savePresetFile(sFilename);

#ifdef CONFIG_DEBUG
	qDebug("qtractorVst3Plugin[%p]::savePresetFile(\"%s\")",
		this, sFilename.toUtf8().constData());
#endif

	QByteArray data;

	if (!m_pImpl->getState(data))
		return false;

	QFile file(sFilename);

	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorVst3Plugin[%p]::savePresetFile(\"%s\")"
			" QFile::open(QFile::WriteOnly|QFile::Truncate) FAILED!",
			this, sFilename.toUtf8().constData());
	#endif
		return false;
	}

	file.write(data);
	file.close();
	return true;
}


// Common host-time keeper (static)
void qtractorVst3Plugin::updateTime ( qtractorAudioEngine *pAudioEngine )
{
	g_hostContext.updateProcessContext(pAudioEngine);
}


// Host cleanup (static).
void qtractorVst3Plugin::clearAll (void)
{
	g_hostContext.clear();
}


//----------------------------------------------------------------------------
// qtractorVst3PluginParam -- VST3 plugin parameter interface decl.
//

// Constructor.
qtractorVst3Plugin::Param::Param (
	qtractorVst3Plugin *pPlugin, unsigned long iIndex )
	: qtractorPlugin::Param(pPlugin, iIndex), m_pImpl(nullptr)
{
	qtractorVst3PluginType *pType
		= static_cast<qtractorVst3PluginType *> (pPlugin->type());
	if (pType) {
		Vst::IEditController *controller = pType->impl()->controller();
		if (controller) {
			const unsigned long iMaxIndex = controller->getParameterCount();
			if (iIndex < iMaxIndex) {
				Vst::ParameterInfo paramInfo;
				::memset(&paramInfo, 0, sizeof(Vst::ParameterInfo));
				if (controller->getParameterInfo(iIndex, paramInfo) == kResultOk) {
					m_pImpl = new Impl(paramInfo);
					setName(fromTChar(paramInfo.title));
					setMinValue(0.0f);
					setMaxValue(1.0f);
					setDefaultValue(controller->getParamNormalized(paramInfo.id));
				}
			}
		}
	}
}


// Destructor.
qtractorVst3Plugin::Param::~Param (void)
{
	if (m_pImpl) delete m_pImpl;
}


// Port range hints predicate methods.
bool qtractorVst3Plugin::Param::isBoundedBelow (void) const
{
	return true;
}

bool qtractorVst3Plugin::Param::isBoundedAbove (void) const
{
	return true;
}

bool qtractorVst3Plugin::Param::isDefaultValue (void) const
{
	return true;
}

bool qtractorVst3Plugin::Param::isLogarithmic (void) const
{
	return false;
}

bool qtractorVst3Plugin::Param::isSampleRate (void) const
{
	return false;
}

bool qtractorVst3Plugin::Param::isInteger (void) const
{
	if (m_pImpl)
		return (m_pImpl->paramInfo().stepCount > 1);
	else
		return false;
}

bool qtractorVst3Plugin::Param::isToggled (void) const
{
	if (m_pImpl)
		return (m_pImpl->paramInfo().stepCount == 1);
	else
		return false;
}

bool qtractorVst3Plugin::Param::isDisplay (void) const
{
	return true;
}


// Current display value.
QString qtractorVst3Plugin::Param::display (void) const
{
	qtractorVst3PluginType *pType = nullptr;
	if (plugin())
		pType = static_cast<qtractorVst3PluginType *> (plugin()->type());
	if (pType && m_pImpl) {
		Vst::IEditController *controller = pType->impl()->controller();
		if (controller) {
			const Vst::ParamID id
				= m_pImpl->paramInfo().id;
			const Vst::ParamValue val
				= Vst::ParamValue(value());
			Vst::String128 str;
			if (controller->getParamStringByValue(id, val, str) == kResultOk)
				return fromTChar(str);
		}
	}

	// Default parameter display value...
	return qtractorPlugin::Param::display();
}


#endif	// CONFIG_VST3

// end of qtractorVst3Plugin.cpp
