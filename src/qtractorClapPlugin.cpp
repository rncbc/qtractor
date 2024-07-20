// qtractorClapPlugin.cpp
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

#include "qtractorAbout.h"

#ifdef CONFIG_CLAP

#include "qtractorClapPlugin.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"
#include "qtractorCurve.h"

#include <clap/clap.h>

#include <QFileInfo>
#include <QWidget>
#include <QIcon>

#include <QTimer>
#include <QTimerEvent>

#include <QSocketNotifier>

#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>

#include <QRegularExpression>

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
#include <QCoreApplication>
#endif

#if 0//QTRACTOR_CLAP_EDITOR_TOOL
#include "qtractorOptions.h"
#endif

#include "qtractorMainForm.h"


//-----------------------------------------------------------------------------
// class qtractorClapPluginHost -- CLAP plugin host (singleton) decl.
//

class qtractorClapPluginHost
{
public:

	// Constructor.
	qtractorClapPluginHost ();

	// Destructor.
	~qtractorClapPluginHost ();

	// Common CLAP host struct initialization...
	//
	static void setup ( clap_host *host, void *host_data )
	{
		::memset(host, 0, sizeof(clap_host));

		host->host_data = host_data;
		host->clap_version = CLAP_VERSION;
		host->name = "qtractorClapPlugin";
		host->version = PROJECT_VERSION;
		host->vendor = QTRACTOR_DOMAIN;
		host->url = QTRACTOR_WEBSITE;
		host->get_extension = get_extension;
		host->request_restart = request_restart;
		host->request_process = request_process;
		host->request_callback = request_callback;
	}

	// Dummy host callbacks...
	//
	static const void *get_extension(
		const clap_host *host, const char *ext_id)
		{ return nullptr; }

	static void request_restart (const clap_host *host) {}
	static void request_process (const clap_host *host) {}
	static void request_callback(const clap_host *host) {}

	// Timer support...
	//
	void startTimer (int msecs);
	void stopTimer ();

	int timerInterval() const;

	bool register_timer (
		const clap_host *host, uint32_t period_ms, clap_id *timer_id);
	bool unregister_timer (
		const clap_host *host, clap_id timer_id);

	void process_timers();

	// POSIX FD support...
	//
	bool register_posix_fd (
		const clap_host_t *host, int fd, clap_posix_fd_flags_t flags);
	bool modify_posix_fd (
		const clap_host_t *host, int fd, clap_posix_fd_flags_t flags);
	bool unregister_posix_fd (
		const clap_host_t *host, int fd);

	void process_posix_fds(
		const clap_host *host, int fd, clap_posix_fd_flags_t flags);

	// Transport info accessor.
	const clap_event_transport *transport() const
		{ return &m_transport; }

	void transportAddRef()
		{ ++m_transportRefCount; }
	void transportReleaseRef()
		{ if (m_transportRefCount > 0) --m_transportRefCount; }

	// Common host-time keeper (static)
	void updateTransport(qtractorAudioEngine *pAudioEngine);

	// Cleanup.
	void clear(const clap_host *host);
	void clear();

	struct Key
	{
		Key(const clap_host *h, int i)
			: host(h), id(i) {}
		Key(const Key& key)
			: host(key.host), id(key.id) {}

		bool operator== (const Key& key) const
			{ return (key.host == host && key.id == id); }

		const clap_host *host;
		int id;
	};

protected:

	class Timer;

private:

	// Instance members.
	Timer *m_pTimer;

	uint32_t m_timerRefCount;

	struct TimerItem
	{
		TimerItem(uint32_t msecs)
			: interval(msecs), counter(0) {}

		uint32_t interval;
		uint32_t counter;
	};

	QHash<const clap_host *, clap_id> m_timer_ids;
	QHash<Key, TimerItem *>           m_timers;

	struct PosixFdItem
	{
		PosixFdItem(int fd, clap_posix_fd_flags_t flags)
			: r_notifier(nullptr), w_notifier(nullptr) { reset(fd, flags); }

		~PosixFdItem() { reset(0, 0); }

		void reset(int fd, clap_posix_fd_flags_t flags)
		{
			if (r_notifier) { delete r_notifier; r_notifier = nullptr; }
			if (flags & CLAP_POSIX_FD_READ)
				r_notifier = new QSocketNotifier(fd, QSocketNotifier::Read);
			if (w_notifier) { delete w_notifier; w_notifier = nullptr; }
			if (flags & CLAP_POSIX_FD_WRITE)
				w_notifier = new QSocketNotifier(fd, QSocketNotifier::Write);
		}

		QSocketNotifier *r_notifier;
		QSocketNotifier *w_notifier;
	};

	QHash<Key, PosixFdItem *> m_posix_fds;

	// Transport info.
	clap_event_transport m_transport;
	unsigned int m_transportRefCount;
};


static uint qHash ( const qtractorClapPluginHost::Key& key )
{
	return qHash(key.host) ^ qHash(key.id);
}


//-----------------------------------------------------------------------------
// class qtractorClapPluginHost::Timer -- CLAP plugin host timer impl.
//

class qtractorClapPluginHost::Timer : public QTimer
{
public:

	// Constructor.
	Timer (qtractorClapPluginHost *pHost) : QTimer(), m_pHost(pHost) {}

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
		if (pTimerEvent->timerId() == QTimer::timerId())
			m_pHost->process_timers();
	}

private:

	// Instance members.
	qtractorClapPluginHost *m_pHost;
};


// Host singleton.
static qtractorClapPluginHost g_host;


//----------------------------------------------------------------------
// class qtractorClapPluginType::Impl -- CLAP plugin meta-interface impl.
//

class qtractorClapPluginType::Impl
{
public:

	// Constructor.
	Impl (qtractorPluginFile *pFile) : m_pFile(pFile),
		m_entry(nullptr), m_factory(nullptr), m_descriptor(nullptr)
		{ qtractorClapPluginHost::setup(&m_host, this); }

	// Destructor.
	~Impl () { close(); }

	// Executive methods.
	bool open (unsigned long iIndex);
	void close ();

	const clap_plugin *create_plugin(const clap_host *host);

	const clap_host *host() const
		{ return &m_host; }
	const clap_plugin_descriptor *descriptor() const
		{ return m_descriptor; }

private:

	// Instance members.
	qtractorPluginFile *m_pFile;

	const clap_plugin_entry *m_entry;
	const clap_plugin_factory *m_factory;

	const clap_plugin_descriptor *m_descriptor;

	clap_host m_host;
};


//----------------------------------------------------------------------
// class qtractorClapPluginType::Impl -- CLAP plugin interface impl.
//

// Executive methods.
//
bool qtractorClapPluginType::Impl::open ( unsigned long iIndex )
{
	close();

	m_entry = reinterpret_cast<const clap_plugin_entry *> (
		m_pFile->resolve("clap_entry"));
	if (!m_entry)
		return false;

	const QByteArray aFilename
		= m_pFile->filename().toUtf8();
	m_entry->init(aFilename.constData());

	m_factory = static_cast<const clap_plugin_factory *> (
		m_entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
	if (!m_factory)
		return false;

	auto count = m_factory->get_plugin_count(m_factory);
	if (iIndex >= count) {
		qDebug("qtractorClapPluginType::Impl[%p]::open(%lu)"
			" *** Bad plug-in index.", this, iIndex);
		return false;
	}

	m_descriptor = m_factory->get_plugin_descriptor(m_factory, iIndex);
	if (!m_descriptor) {
		qDebug("qtractorClapPluginType::Impl[%p]::open(%lu)"
			" *** No plug-in descriptor.", this, iIndex);
		return false;
	}

	if (!clap_version_is_compatible(m_descriptor->clap_version)) {
		qDebug("qtractorClapPluginType::Impl[%p]::open(%lu)"
			" *** Incompatible CLAP version:"
			" plug-in is %d.%d.%d, host is %d.%d.%d.", this, iIndex,
			m_descriptor->clap_version.major,
			m_descriptor->clap_version.minor,
			m_descriptor->clap_version.revision,
			CLAP_VERSION.major,
			CLAP_VERSION.minor,
			CLAP_VERSION.revision);
		return false;
	}

	return true;
}


const clap_plugin *qtractorClapPluginType::Impl::create_plugin (
	const clap_host *host )
{
	if (!m_factory || !m_descriptor)
		return nullptr;

	const clap_plugin *plugin
		= m_factory->create_plugin(m_factory, host, m_descriptor->id);
	if (!plugin) {
		qDebug("qtractorClapPluginType::Impl[%p]::create_plugin(%p)"
			" *** Could not create plug-in with id: %s.", this, host, m_descriptor->id);
	}
	else
	if (!plugin->init(plugin)) {
		qDebug("qtractorClapPluginType::Impl[%p]::create_plugin(%p)"
			" *** Could not initialize plug-in with id: %s.", this, host, m_descriptor->id);
		plugin->destroy(plugin);
		plugin = nullptr;
	}

	return plugin;
}


void qtractorClapPluginType::Impl::close (void)
{
	m_descriptor = nullptr;
	m_factory = nullptr;

	if (m_entry) {
		m_entry->deinit();
		m_entry = nullptr;
	}
}


//----------------------------------------------------------------------
// class qtractorClapPluginType -- CLAP plugin meta-interface impl.
//

// Constructor.
qtractorClapPluginType::qtractorClapPluginType (
	qtractorPluginFile *pFile, unsigned long iIndex )
	: qtractorPluginType(pFile, iIndex, Clap), m_pImpl(new Impl(pFile)),
		m_iMidiDialectIns(0), m_iMidiDialectOuts(0)
{
}

// Destructor.
qtractorClapPluginType::~qtractorClapPluginType (void)
{
	close();

	delete m_pImpl;
}


// Factory method (static)
qtractorClapPluginType *qtractorClapPluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	return new qtractorClapPluginType(pFile, iIndex);
}


// Executive methods.
bool qtractorClapPluginType::open (void)
{
	close();

	if (!m_pImpl->open(index()))
		return false;

	const clap_plugin_descriptor *descriptor
		= m_pImpl->descriptor();
	if (!descriptor)
		return false;

	const clap_plugin *plugin = m_pImpl->create_plugin(m_pImpl->host());
	if (!plugin)
		return false;

	m_sName = QString::fromUtf8(descriptor->name);
	m_sLabel = m_sName.simplified().replace(QRegularExpression("[\\s|\\.|\\-]+"), "_");
	m_iUniqueID = qHash(descriptor->id);

	m_iAudioIns = 0;
	m_iAudioOuts = 0;
	const clap_plugin_audio_ports *audio_ports
		= static_cast<const clap_plugin_audio_ports *> (
			plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
	if (audio_ports && audio_ports->count && audio_ports->get) {
		clap_audio_port_info info;
		const uint32_t nins = audio_ports->count(plugin, true);
		for (uint32_t i = 0; i < nins; ++i) {
			::memset(&info, 0, sizeof(info));
			if (audio_ports->get(plugin, i, true, &info)) {
				if (info.flags & CLAP_AUDIO_PORT_IS_MAIN)
					m_iAudioIns += info.channel_count;
			}
		}
		const uint32_t nouts = audio_ports->count(plugin, false);
		for (uint32_t i = 0; i < nouts; ++i) {
			::memset(&info, 0, sizeof(info));
			if (audio_ports->get(plugin, i, false, &info)) {
				if (info.flags & CLAP_AUDIO_PORT_IS_MAIN)
					m_iAudioOuts += info.channel_count;
			}
		}
	}

	m_iMidiIns = 0;
	m_iMidiOuts = 0;
	m_iMidiDialectIns = 0;
	m_iMidiDialectOuts = 0;
	const clap_plugin_note_ports *note_ports
		= static_cast<const clap_plugin_note_ports *> (
			plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
	if (note_ports && note_ports->count && note_ports->get) {
		clap_note_port_info info;
		const uint32_t nins = note_ports->count(plugin, true);
		for (uint32_t i = 0; i < nins; ++i) {
			::memset(&info, 0, sizeof(info));
			if (note_ports->get(plugin, i, true, &info)) {
				if (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI)
					++m_iMidiDialectIns;
				++m_iMidiIns;
			}
		}
		const uint32_t nouts = note_ports->count(plugin, false);
		for (uint32_t i = 0; i < nouts; ++i) {
			::memset(&info, 0, sizeof(info));
			if (note_ports->get(plugin, i, false, &info)) {
				if (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI)
					++m_iMidiDialectOuts;
				++m_iMidiOuts;
			}
		}
	}

	m_iControlIns = 0;
	m_iControlOuts = 0;
	const clap_plugin_params *params
		= static_cast<const clap_plugin_params *> (
			plugin->get_extension(plugin, CLAP_EXT_PARAMS));
	if (params && params->count && params->get_info) {
		const uint32_t nparams = params->count(plugin);
		for (uint32_t i = 0; i < nparams; ++i) {
			clap_param_info param_info;
			::memset(&param_info, 0, sizeof(param_info));
			if (params->get_info(plugin, i, &param_info)) {
				if (param_info.flags & CLAP_PARAM_IS_READONLY)
					++m_iControlOuts;
				else
				if (param_info.flags & CLAP_PARAM_IS_AUTOMATABLE)
					++m_iControlIns;
			}
		}
	}

	m_bEditor = false;
	const clap_plugin_gui *gui
		= static_cast<const clap_plugin_gui *> (
			plugin->get_extension(plugin, CLAP_EXT_GUI));
	if (gui && gui->is_api_supported && gui->create && gui->destroy) {
		m_bEditor = (
			gui->is_api_supported(plugin, CLAP_WINDOW_API_X11,     false) ||
			gui->is_api_supported(plugin, CLAP_WINDOW_API_X11,     true)  ||
			gui->is_api_supported(plugin, CLAP_WINDOW_API_WAYLAND, false) ||
			gui->is_api_supported(plugin, CLAP_WINDOW_API_WAYLAND, true)
		);
	}

	m_bRealtime = true;
	const clap_plugin_state *state
		= static_cast<const clap_plugin_state *> (
			plugin->get_extension(plugin, CLAP_EXT_STATE));
	m_bConfigure = (state && state->save && state->load);

	plugin->destroy(plugin);

	m_sAboutText.clear();
#if 0
	m_sAboutText += QObject::tr("Name: ");
	m_sAboutText += m_sName;
#endif
	QString sText = QString::fromUtf8(descriptor->version);
	if (!sText.isEmpty()) {
	//	m_sAboutText += '\n';
		m_sAboutText += QObject::tr("Version: ");
		m_sAboutText += sText;
	}
	sText = QString("CLAP %1.%2.%3")
		.arg(descriptor->clap_version.major)
		.arg(descriptor->clap_version.minor)
		.arg(descriptor->clap_version.revision);
	if (!sText.isEmpty()) {
		m_sAboutText += ' '; // '\t'?
		m_sAboutText += '(';
		m_sAboutText += sText;
		m_sAboutText += ')';
	}
	sText = QString::fromUtf8(descriptor->vendor);
	if (!sText.isEmpty()) {
		m_sAboutText += '\n';
		m_sAboutText += QObject::tr("Vendor: ");
		m_sAboutText += sText;
	}
	sText = QString::fromUtf8(descriptor->url);
	if (!sText.isEmpty()) {
		m_sAboutText += '\n';
	//	m_sAboutText += QObject::tr("URL: ");
		m_sAboutText += sText;
	}
#if 0
	sText = QString::fromUtf8(descriptor->manual_url);
	if (!sText.isEmpty()) {
		m_sAboutText += '\n';
		m_sAboutText += QObject::tr("Manual: ");
		m_sAboutText += sText;
	}
	sText = QString::fromUtf8(descriptor->support_url);
	if (!sText.isEmpty()) {
		m_sAboutText += '\n';
		m_sAboutText += QObject::tr("Support: ");
		m_sAboutText += sText;
	}
#endif
	sText = QString::fromUtf8(descriptor->description);
	if (!sText.isEmpty()) {
		m_sAboutText += '\n';
		m_sAboutText += sText;
	}
	QStringList features;
	for (int i = 0; descriptor->features[i]; ++i)
		features << QString::fromUtf8(descriptor->features[i]);
	sText = features.join(' ');
	if (!sText.isEmpty()) {
		m_sAboutText += '\n';
	//	m_sAboutText += QObject::tr("Features: ");
		m_sAboutText += '(';
		m_sAboutText += sText;
		m_sAboutText += ')';
	}

	return true;
}


void qtractorClapPluginType::close (void)
{
	m_pImpl->close();
}


//----------------------------------------------------------------------
// class qtractorClapPlugin::Impl -- CLAP plugin interface impl.
//

class qtractorClapPlugin::Impl
{
public:

	// Constructor.
	Impl (qtractorClapPlugin *pPlugin);

	// Destructor.
	~Impl ();

	// Implementation accessors.
	const clap_plugin *plugin() const
		{ return m_plugin; }
	const clap_plugin_gui *gui() const
		{ return m_gui; }

	const clap_plugin_note_name *note_names () const
		{ return m_note_names; }

	// Do the actual (de)activation.
	void activate ();
	void deactivate (bool force = false);

	// Audio processor methods.
	bool process_reset(qtractorAudioEngine *pAudioEngine);
	void process_midi_in (unsigned char *data, unsigned int size,
		unsigned long offset, unsigned short port);
	void process (float **ins, float **outs, unsigned int nframes);

	// Plugin current latency (in frames);
	unsigned long latency () const;

	// Total parameter count.
	unsigned long getParameterCount() const
		{ return m_param_infos.count(); }

	clap_id getParameterId (unsigned long iIndex) const
		{ return m_param_ids.value(iIndex, CLAP_INVALID_ID); }

	// Parameter info accessor.
	const clap_param_info *getParameterInfo (clap_id id) const
		{ return m_param_infos.value(id, nullptr); }

	// Set/add a parameter value/point.
	void setParameter (clap_id id, double alue);

	// Get current parameter value.
	double getParameter (clap_id id) const;

	// Get current parameter value text.
	QString getParameterText (clap_id id, double value) const;

	// Events buffer decl.
	//
	class EventList
	{
	public:

		EventList ( uint32_t nsize = 1024, uint32_t ncapacity = 8 )
			: m_nsize(0), m_eheap(nullptr),
				m_ehead(nullptr), m_etail(nullptr), m_ihead(0)
		{
			resize(nsize);

			m_elist.reserve(ncapacity);

			::memset(&m_ins, 0, sizeof(m_ins));
			m_ins.ctx  = this;
			m_ins.size = &events_in_size;
			m_ins.get  = &events_in_get;

			::memset(&m_outs, 0, sizeof(m_outs));
			m_outs.ctx = this;
			m_outs.try_push = &events_out_push;
		}

		~EventList () { resize(0); }

		const clap_input_events *ins () const
			{ return &m_ins; }
		const clap_output_events *outs () const
			{ return &m_outs; }

		bool push ( const clap_event_header *eh )
		{
			const uint32_t ntail = m_etail - m_eheap;
			const uint32_t nsize = ntail + eh->size;
			if (m_nsize < nsize)
				resize(nsize << 1);
			const uint32_t ncapacity = m_elist.capacity();
			if (m_elist.size() >= ncapacity)
				m_elist.reserve(ncapacity << 1);
			m_elist.push_back(ntail);
			::memcpy(m_etail, eh, eh->size);
			m_etail += eh->size;

			return true;
		}

		const clap_event_header *get ( uint32_t index ) const
		{
			const clap_event_header *ret = nullptr;
			if (index + m_ihead < m_elist.size()) {
				ret = reinterpret_cast<const clap_event_header *> (
					m_eheap + m_elist.at(index + m_ihead));
			}
			return ret;
		}

		const clap_event_header *pop ()
		{
			const clap_event_header *ret = nullptr;
			if (m_ihead < m_elist.size() && m_ehead < m_etail) {
				ret = reinterpret_cast<const clap_event_header *> (m_ehead);
				m_ehead += ret->size;
				++m_ihead;
			}
			else clear();
			return ret;
		}

		size_t size () const
			{ return m_elist.size() - m_ihead; }

		bool empty () const
			{ return (m_etail == m_ehead); }

		void clear ()
		{
			m_ehead = m_eheap;
			m_etail = m_ehead;
			m_ihead = 0;
			m_elist.clear();
		}

	protected:

		void resize ( uint32_t nsize )
		{
			uint8_t *old_eheap = m_eheap;
			uint8_t *old_ehead = m_ehead;
			uint8_t *old_etail = m_etail;
			m_eheap = nullptr;
			m_ehead = m_eheap;
			m_etail = m_ehead;
			m_nsize = nsize;
			if (m_nsize > 0) {
				m_eheap = new uint8_t [m_nsize];
				m_ehead = m_eheap;
				m_etail = m_ehead;
				if (old_etail > old_ehead) {
					const uint32_t ntail
						= old_etail - old_ehead;
					::memcpy(m_ehead, old_ehead, ntail);
					m_etail += ntail;
				}
			}
			if (old_eheap)
				delete [] old_eheap;
		}

		static uint32_t events_in_size (
			const clap_input_events *ins )
		{
			const EventList *elist
				= static_cast<const EventList *> (ins->ctx);
			return elist->size();
		}

		static const clap_event_header_t *events_in_get (
			const struct clap_input_events *ins, uint32_t index )
		{
			const EventList *elist
				= static_cast<const EventList *> (ins->ctx);
			return elist->get(index);
		}

		static bool events_out_push (
			const clap_output_events *outs,
			const clap_event_header *eh )
		{
			EventList *elist = static_cast<EventList *> (outs->ctx);
			return elist->push(eh);
		}

	private:

		uint32_t m_nsize;
		uint8_t *m_eheap;
		uint8_t *m_ehead;
		uint8_t *m_etail;
		uint32_t m_ihead;

		std::vector<uint32_t> m_elist;

		clap_input_events  m_ins;
		clap_output_events m_outs;
	};

	// Event buffer accessors.
	EventList& events_in  () { return m_events_in;  }
	EventList& events_out () { return m_events_out; }

	EventList& params_out () { return m_params_out; }

	// Plugin preset/state snapshot accessors.
	bool setState (const QByteArray& data);
	bool getState (QByteArray& data);

	// Plugin timer support callback.
	void plugin_on_timer(clap_id timer_id);
	// Plugin POSIX FD support callback.
	void plugin_on_posix_fd(int fd, clap_posix_fd_flags_t flags);

	// Plugin set/load state stream callbacks.
	static int64_t plugin_state_read (
		const clap_istream *stream, void *buffer, uint64_t size);

	int64_t plugin_state_read_buffer(void *buffer, uint64_t size);

	// Plugin get/save state stream callbacks.
	static int64_t plugin_state_write (
		const clap_ostream *stream, const void *buffer, uint64_t size);

	int64_t plugin_state_write_buffer(const void *buffer, uint64_t size);

	// Plugin parameters flush.
	void plugin_params_flush ();

	// Reinitialize the plugin instance...
	void plugin_request_restart ();

	// MIDI specific dialect port counters (accessors).
	int midiDialectIns() const;
	int midiDialectOuts() const;

protected:

	// Plugin module (de)initializers.
	void initialize ();
	void deinitialize ();

	// Parameters info/ids (de)initializer.
	void addParamInfos();
	void clearParamInfos();

	// Host extensions interface.
	//
	static const void *get_extension(
		const clap_host *host, const char *ext_id);

	// Main host callbacks...
	//
	static void host_request_restart (const clap_host *host);
	static void host_request_process (const clap_host *host);
	static void host_request_callback (const clap_host *host);

//	void plugin_request_restart ();
	void plugin_request_process ();
	void plugin_request_callback ();

	// Host LOG callbacks...
	//
	static void host_log (
		const clap_host *host, clap_log_severity severity, const char *msg);

	static const constexpr clap_host_log g_host_log = {
		Impl::host_log,
	};

	// Host GUI callbacks...
	//
	static void host_gui_resize_hints_changed (
		const clap_host *host);
	static bool host_gui_request_resize (
		const clap_host *host, uint32_t width, uint32_t height);
	static bool host_gui_request_show (
		const clap_host *host);
	static bool host_gui_request_hide (
		const clap_host *host);
	static void host_gui_closed (
		const clap_host *host, bool was_destroyed);

	static const constexpr clap_host_gui g_host_gui = {
		Impl::host_gui_resize_hints_changed,
		Impl::host_gui_request_resize,
		Impl::host_gui_request_show,
		Impl::host_gui_request_hide,
		Impl::host_gui_closed,
	};

	void plugin_gui_resize_hints_changed (void);
	bool plugin_gui_request_resize(uint32_t width, uint32_t height);
	bool plugin_gui_request_show(void);
	bool plugin_gui_request_hide(void);
	void plugin_gui_closed (bool was_destroyed);

	// Host Parameters callbacks...
	//
	static void host_params_rescan (
		const clap_host *host, clap_param_rescan_flags flags);
	static void host_params_clear (
		const clap_host *host, clap_id param_id, clap_param_clear_flags flags);
	static void host_params_request_flush (
		const clap_host *host);

	static const constexpr clap_host_params g_host_params = {
		Impl::host_params_rescan,
		Impl::host_params_clear,
		Impl::host_params_request_flush,
	};

	void plugin_params_rescan (clap_param_rescan_flags flags);
	void plugin_params_clear (clap_id param_id, clap_param_clear_flags flags);
	void plugin_params_request_flush ();

	// Host Audio Ports support callbacks...
	//
	static bool host_audio_ports_is_rescan_flag_supported (
		const clap_host *host, uint32_t flag);
	static void host_audio_ports_rescan (
		const clap_host *host, uint32_t flags);

	static const constexpr clap_host_audio_ports g_host_audio_ports = {
		Impl::host_audio_ports_is_rescan_flag_supported,
		Impl::host_audio_ports_rescan,
	};

	// Host Note Ports support callbacks...
	//
	static uint32_t host_note_ports_supported_dialects (
		const clap_host *host);
	static void host_note_ports_rescan (
		const clap_host *host, uint32_t flags);

	static const constexpr clap_host_note_ports g_host_note_ports = {
		Impl::host_note_ports_supported_dialects,
		Impl::host_note_ports_rescan,
	};

	// Host Latency callbacks...
	//
	static void host_latency_changed (
		const clap_host *host);

	static const constexpr clap_host_latency g_host_latency = {
		Impl::host_latency_changed,
	};

	void plugin_latency_changed ();

	// Host Timer support callbacks...
	//
	static bool host_register_timer (
		const clap_host *host, uint32_t period_ms, clap_id *timer_id);
	static bool host_unregister_timer (
		const clap_host *host, clap_id timer_id);

	static const constexpr clap_host_timer_support g_host_timer_support = {
		Impl::host_register_timer,
		Impl::host_unregister_timer,
	};

	// Host POSIX FD support callbacks...
	//
	static bool host_register_posix_fd (
		const clap_host *host, int fd, clap_posix_fd_flags_t flags);
	static bool host_modify_posix_fd (
		const clap_host *host, int fd, clap_posix_fd_flags_t flags);
	static bool host_unregister_posix_fd (
		const clap_host *host, int fd);

	static const constexpr clap_host_posix_fd_support g_host_posix_fd_support = {
		Impl::host_register_posix_fd,
		Impl::host_modify_posix_fd,
		Impl::host_unregister_posix_fd,
	};

	// Host thread-check callbacks...
	//
	static bool host_is_main_thread (
		const clap_host *host);
	static bool host_is_audio_thread (
		const clap_host *host);

	static const constexpr clap_host_thread_check g_host_thread_check = {
		Impl::host_is_main_thread,
		Impl::host_is_audio_thread,
	};

	// Host thread-pool callbacks...
	//
	static bool host_thread_pool_request_exec (
		const clap_host *host, uint32_t num_tasks);

	static const constexpr clap_host_thread_pool g_host_thread_pool = {
		Impl::host_thread_pool_request_exec,
	};

	// Host state callbacks...
	//
	static void host_state_mark_dirty (
		const clap_host *host);

	static const constexpr clap_host_state g_host_state = {
		Impl::host_state_mark_dirty,
	};

	void plugin_state_mark_dirty ();

	// Host Note-names callbacks...
	//
	static void host_note_name_changed (
		const clap_host *host);

	static const constexpr clap_host_note_name g_host_note_name = {
		Impl::host_note_name_changed,
	};

	void plugin_note_name_changed ();

	// Transfer parameter changes...
	void process_params_out ();

private:

	// Instance variables.
	qtractorClapPlugin *m_pPlugin;

	const clap_plugin *m_plugin;

	const clap_plugin_params *m_params;

	QHash<unsigned long, clap_id> m_param_ids;
	QHash<clap_id, const clap_param_info *> m_param_infos;

	const clap_plugin_timer_support *m_timer_support;
	const clap_plugin_posix_fd_support *m_posix_fd_support;

	const clap_plugin_gui *m_gui;
	const clap_plugin_state *m_state;

	const clap_plugin_note_name *m_note_names;

	volatile bool m_params_flush;

	volatile bool m_activated;
	volatile bool m_sleeping;
	volatile bool m_processing;
	volatile bool m_restarting;

	clap_host m_host;

	// Processor parameters.
	unsigned int m_srate;
	unsigned int m_nframes;
	unsigned int m_nframes_max;

	// Audio processor buffers.
	clap_audio_buffer m_audio_ins;
	clap_audio_buffer m_audio_outs;

	// Events processor buffers.
	EventList m_events_in;
	EventList m_events_out;

	// Parameters processor queue.
	EventList m_params_out;

	// Process context.
	clap_process m_process;

	// Working state stream buffer.
	QByteArray m_state_data;
};


//-----------------------------------------------------------------------------
// class qtractorClapPluginHost -- CLAP plugin host context impl.
//

// Constructor.
qtractorClapPluginHost::qtractorClapPluginHost (void)
{
	m_pTimer = new Timer(this);

	m_timerRefCount = 0;
	m_transportRefCount = 0;

	::memset(&m_transport, 0, sizeof(m_transport));
}

// Destructor.
qtractorClapPluginHost::~qtractorClapPluginHost (void)
{
	clear();

	delete m_pTimer;
}


// Timer support...
//
void qtractorClapPluginHost::startTimer ( int msecs )
	{ if (++m_timerRefCount == 1) m_pTimer->start(msecs); }

void qtractorClapPluginHost::stopTimer (void)
	{ if (m_timerRefCount > 0 && --m_timerRefCount == 0) m_pTimer->stop(); }

int qtractorClapPluginHost::timerInterval (void) const
	{ return m_pTimer->interval(); }


bool qtractorClapPluginHost::register_timer (
	const clap_host *host, uint32_t period_ms, clap_id *timer_id )
{
	const clap_id id
		= m_timer_ids.value(host, 0);
	m_timer_ids.insert(host, id + 1);

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPluginHost::register_timer(%p, %u, %u)", host, period_ms, id);
#endif
	*timer_id = id;
	m_timers.insert(Key(host, id), new TimerItem(period_ms));
	m_pTimer->start(int(period_ms));
	return true;
}


bool qtractorClapPluginHost::unregister_timer (
	const clap_host *host, clap_id timer_id )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPluginHost::unregister_timer(%p, %u)", host, timer_id);
#endif
	const Key key(host, timer_id);
	TimerItem *timer = m_timers.value(key, nullptr);
	if (timer)
		delete timer;
	m_timers.remove(key);
	if (m_timers.isEmpty())
		m_pTimer->stop();
	return true;
}


// POSIX FD support...
//
bool qtractorClapPluginHost::register_posix_fd (
	const clap_host_t *host, int fd, clap_posix_fd_flags_t flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPluginHost::register_posix_fd(%p, %d, 0x%04x)", host, fd, flags);
#endif
	const Key key(host, fd);
	PosixFdItem *posix_fd = m_posix_fds.value(key, nullptr);
	if (posix_fd) {
		posix_fd->reset(fd, flags);
	} else {
		posix_fd = new PosixFdItem(fd, flags);
		m_posix_fds.insert(key, posix_fd);
	}

	if (posix_fd->r_notifier) {
		QObject::connect(posix_fd->r_notifier,
			&QSocketNotifier::activated, [this, host, fd] {
			process_posix_fds(host, fd, CLAP_POSIX_FD_READ); });
	}

	if (posix_fd->w_notifier) {
		QObject::connect(posix_fd->w_notifier,
			&QSocketNotifier::activated, [this, host, fd] {
			process_posix_fds(host, fd, CLAP_POSIX_FD_WRITE); });
	}

	return true;
}


bool qtractorClapPluginHost::modify_posix_fd (
	const clap_host_t *host, int fd, clap_posix_fd_flags_t flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPluginHost::modify_posix_fd(%p, %d, 0x%04x)", host, fd, flags);
#endif
	return register_posix_fd(host, fd, flags);
}


bool qtractorClapPluginHost::unregister_posix_fd (
	const clap_host_t *host, int fd )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPluginHost::unregister_posix_fd(%p, %d)", host, fd);
#endif
	const Key key(host, fd);
	PosixFdItem *posix_fd = m_posix_fds.value(key, nullptr);
	if (posix_fd)
		delete posix_fd;
	m_posix_fds.remove(key);
	return true;
}


// Cleanup.
void qtractorClapPluginHost::clear ( const clap_host *host )
{
	QList<Key> keys;
	QList<TimerItem *> timers;

	QHash<Key, TimerItem *>::ConstIterator timer_iter = m_timers.constBegin();
	const QHash<Key, TimerItem *>::ConstIterator& timer_end = m_timers.constEnd();
	for ( ; timer_iter != timer_end; ++timer_iter) {
		const Key& key = timer_iter.key();
		TimerItem *timer = timer_iter.value();
		if (key.host == host) {
			timers.append(timer);
			keys.append(key);
		}
	}

	foreach (const Key& key, keys)
		m_timers.remove(key);
	qDeleteAll(timers);

	keys.clear();
	QList<PosixFdItem *> posix_fds;

	QHash<Key, PosixFdItem *>::ConstIterator posix_fd_iter = m_posix_fds.constBegin();
	const QHash<Key, PosixFdItem *>::ConstIterator& posix_fd_end = m_posix_fds.constEnd();
	for ( ; posix_fd_iter != posix_fd_end; ++posix_fd_iter) {
		const Key& key = posix_fd_iter.key();
		PosixFdItem *posix_fd = posix_fd_iter.value();
		if (key.host == host) {
			posix_fds.append(posix_fd);
			keys.append(key);
		}
	}

	foreach (const Key& key, keys)
		m_posix_fds.remove(key);
	qDeleteAll(posix_fds);
}


void qtractorClapPluginHost::clear (void)
{
	m_timerRefCount = 0;
	m_transportRefCount = 0;

	qDeleteAll(m_timers);
	m_timers.clear();
	m_timer_ids.clear();

	qDeleteAll(m_posix_fds);
	m_posix_fds.clear();
}


void qtractorClapPluginHost::process_timers (void)
{
	QHash<Key, TimerItem *>::ConstIterator iter = m_timers.constBegin();
	const QHash<Key, TimerItem *>::ConstIterator& iter_end = m_timers.constEnd();
	for ( ; iter != iter_end; ++iter) {
		TimerItem *timer = iter.value();
		timer->counter += timerInterval();
		if (timer->counter >= timer->interval) {
			const Key& key = iter.key();
			qtractorClapPlugin::Impl *host_data
				= static_cast<qtractorClapPlugin::Impl *> (key.host->host_data);
			if (host_data)
				host_data->plugin_on_timer(key.id);
			timer->counter = 0;
		}
	}
}


void qtractorClapPluginHost::process_posix_fds (
	const clap_host *host, int fd, clap_posix_fd_flags_t flags )
{
	qtractorClapPlugin::Impl *host_data
		= static_cast<qtractorClapPlugin::Impl *> (host->host_data);
	if (host_data)
		host_data->plugin_on_posix_fd(fd, flags);
}


// Common host-time keeper (static)
void qtractorClapPluginHost::updateTransport ( qtractorAudioEngine *pAudioEngine )
{
	if (m_transportRefCount < 1)
		return;

	const qtractorAudioEngine::TimeInfo& timeInfo
		= pAudioEngine->timeInfo();

	if (timeInfo.playing)
		m_transport.flags |=  CLAP_TRANSPORT_IS_PLAYING;
	else
		m_transport.flags &= ~CLAP_TRANSPORT_IS_PLAYING;

	m_transport.flags |= CLAP_TRANSPORT_HAS_SECONDS_TIMELINE;
	m_transport.song_pos_seconds = CLAP_SECTIME_FACTOR *
		double(timeInfo.frame) / double(pAudioEngine->sampleRate());

	m_transport.flags |= CLAP_TRANSPORT_HAS_BEATS_TIMELINE;
	m_transport.song_pos_beats = CLAP_BEATTIME_FACTOR *	double(timeInfo.beats);
	m_transport.bar_start = CLAP_BEATTIME_FACTOR * double(timeInfo.barBeats);
	m_transport.bar_number = timeInfo.bar;

	m_transport.flags |= CLAP_TRANSPORT_HAS_TEMPO;
	m_transport.tempo  = timeInfo.tempo;
	m_transport.flags |= CLAP_TRANSPORT_HAS_TIME_SIGNATURE;
	m_transport.tsig_num = uint16_t(timeInfo.beatsPerBar);
	m_transport.tsig_denom = uint16_t(timeInfo.beatType);
}



//----------------------------------------------------------------------
// class qtractorClapPlugin::Impl -- CLAP plugin interface impl.
//

// Constructor.
qtractorClapPlugin::Impl::Impl ( qtractorClapPlugin *pPlugin )
	: m_pPlugin(pPlugin), m_plugin(nullptr), m_params(nullptr),
		m_timer_support(nullptr), m_posix_fd_support(nullptr),
		m_gui(nullptr), m_state(nullptr), m_note_names(nullptr),
		m_params_flush(false), m_activated(false), m_sleeping(false),
		m_processing(false), m_restarting(false),
		m_srate(44100), m_nframes(0), m_nframes_max(0)
{
	qtractorClapPluginHost::setup(&m_host, this);
	m_host.get_extension = qtractorClapPlugin::Impl::get_extension;
	m_host.request_restart = qtractorClapPlugin::Impl::host_request_restart;
	m_host.request_process = qtractorClapPlugin::Impl::host_request_process;
	m_host.request_callback = qtractorClapPlugin::Impl::host_request_callback;

	initialize();
}


// Destructor.
qtractorClapPlugin::Impl::~Impl (void)
{
	deinitialize();
}


// Plugin module initializer.
void qtractorClapPlugin::Impl::initialize (void)
{
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return;
#if 0//HACK: Plugin-type might be already open via plugin-factory...
	if (!pType->open())
		return;
#endif
	m_plugin = pType->impl()->create_plugin(&m_host);
	if (!m_plugin)
		return;

	m_params = static_cast<const clap_plugin_params *> (
		m_plugin->get_extension(m_plugin, CLAP_EXT_PARAMS));

	m_timer_support = static_cast<const clap_plugin_timer_support *> (
		m_plugin->get_extension(m_plugin, CLAP_EXT_TIMER_SUPPORT));
	m_posix_fd_support = static_cast<const clap_plugin_posix_fd_support *> (
		m_plugin->get_extension(m_plugin, CLAP_EXT_POSIX_FD_SUPPORT));

	m_gui = static_cast<const clap_plugin_gui *> (
		m_plugin->get_extension(m_plugin, CLAP_EXT_GUI));
	m_state	= static_cast<const clap_plugin_state *> (
		m_plugin->get_extension(m_plugin, CLAP_EXT_STATE));
	m_note_names = static_cast<const clap_plugin_note_name *> (
		m_plugin->get_extension(m_plugin, CLAP_EXT_NOTE_NAME));

	addParamInfos();
}


// Plugin module deinitializer.
void qtractorClapPlugin::Impl::deinitialize (void)
{
	g_host.clear(&m_host);

	deactivate(true);

	clearParamInfos();

	if (m_plugin) {
		m_plugin->destroy(m_plugin);
		m_plugin = nullptr;
	}

	m_params = nullptr;

	m_timer_support = nullptr;
	m_posix_fd_support = nullptr;

	m_gui = nullptr;
	m_state = nullptr;
	m_note_names = nullptr;
}


// Parameters info/ids (de)initializer.
void qtractorClapPlugin::Impl::addParamInfos (void)
{
	if (m_params && m_params->count && m_params->get_info) {
		const uint32_t nparams = m_params->count(m_plugin);
		for (uint32_t i = 0; i < nparams; ++i) {
			clap_param_info *param_info = new clap_param_info;
			::memset(param_info, 0, sizeof(clap_param_info));
			if (m_params->get_info(m_plugin, i, param_info)) {
				m_param_ids.insert(i, param_info->id);
				m_param_infos.insert(param_info->id, param_info);
			}
		}
	}
}


void qtractorClapPlugin::Impl::clearParamInfos (void)
{
	qDeleteAll(m_param_infos);
	m_param_infos.clear();
	m_param_ids.clear();
}


// Do the actual (de)activation.
void qtractorClapPlugin::Impl::activate (void)
{
	if (!m_plugin)
		return;

	if (m_sleeping) {
		m_sleeping = false;
		return;
	}

	if (m_activated)
		return;

	if (m_srate < 2 || m_nframes < 2 || m_nframes_max < 2)
		return;

	plugin_params_request_flush();

	if (!m_plugin->activate(m_plugin, double(m_srate), m_nframes, m_nframes_max))
		return;

	m_activated = true;

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl[%p]::activate() processing=%d", this, int(m_processing));
#endif
}


void qtractorClapPlugin::Impl::deactivate ( bool force )
{
	if (!m_plugin)
		return;

	if (!m_activated)
		return;

	if (!m_sleeping && !force) {
		m_sleeping = true;
		return;
	}

	m_activated = false;

	m_plugin->deactivate(m_plugin);

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl[%p]::deactivate() processing=%d", this, int(m_processing));
#endif
}


// Audio processor stuff.
bool qtractorClapPlugin::Impl::process_reset (
	qtractorAudioEngine *pAudioEngine )
{
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (m_pPlugin->type());
	if (pType == nullptr)
		return false;

	deactivate();

	m_srate = pAudioEngine->sampleRate();
	m_nframes = pAudioEngine->bufferSize();
	m_nframes_max = pAudioEngine->bufferSizeEx();

	::memset(&m_audio_ins, 0, sizeof(m_audio_ins));
	m_audio_ins.channel_count = pType->audioIns();

	::memset(&m_audio_outs, 0, sizeof(m_audio_outs));
	m_audio_outs.channel_count = pType->audioOuts();

	m_events_in.clear();
	m_events_out.clear();

	::memset(&m_process, 0, sizeof(m_process));
	if (pType->audioIns() > 0) {
		m_process.audio_inputs = &m_audio_ins;
		m_process.audio_inputs_count = 1;
	}
	if (pType->audioOuts() > 0) {
		m_process.audio_outputs = &m_audio_outs;
		m_process.audio_outputs_count = 1;
	}
	m_process.in_events  = m_events_in.ins();
	m_process.out_events = m_events_out.outs();
	m_process.frames_count = pAudioEngine->blockSize();
	m_process.steady_time = 0;
	m_process.transport = g_host.transport();

	activate();

	return true;
}


void qtractorClapPlugin::Impl::process_midi_in (
	unsigned char *data, unsigned int size,
	unsigned long offset, unsigned short port )
{
	const int midi_dialect_ins = midiDialectIns();

	for (unsigned int i = 0; i < size; ++i) {

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
		// after-touch
		if ((midi_dialect_ins > 0) &&
			(status == 0xc0 || status == 0xd0)) {
			clap_event_midi ev;
			ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
			ev.header.type = CLAP_EVENT_MIDI;
			ev.header.time = offset;
			ev.header.flags = 0;
			ev.header.size = sizeof(ev);
			ev.port_index = port;
			ev.data[0] = status | channel;
			ev.data[1] = key;
			ev.data[2] = 0;
			m_events_in.push(&ev.header);
			continue;
		}

		// check data size (#2)
		if (++i >= size)
			break;

		// channel value (normalized)
		const int value = (data[i] & 0x7f);

		// note on
		if (status == 0x90) {
			clap_event_note ev;
			ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
			ev.header.type = CLAP_EVENT_NOTE_ON;
			ev.header.time = offset;
			ev.header.flags = 0;
			ev.header.size = sizeof(ev);
			ev.note_id = -1;
			ev.port_index = port;
			ev.key = key;
			ev.channel = channel;
			ev.velocity = value / 127.0;
			m_events_in.push(&ev.header);
		}
		else
		// note off
		if (status == 0x80) {
			clap_event_note ev;
			ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
			ev.header.type = CLAP_EVENT_NOTE_OFF;
			ev.header.time = offset;
			ev.header.flags = 0;
			ev.header.size = sizeof(ev);
			ev.note_id = -1;
			ev.port_index = port;
			ev.key = key;
			ev.channel = channel;
			ev.velocity = value / 127.0;
			m_events_in.push(&ev.header);
		}
		else
		// key pressure/poly.aftertouch
		// control-change
		// pitch-bend
		if ((midi_dialect_ins > 0) &&
			(status == 0xa0 || status == 0xb0 || status == 0xe0)) {
			clap_event_midi ev;
			ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
			ev.header.type = CLAP_EVENT_MIDI;
			ev.header.time = offset;
			ev.header.flags = 0;
			ev.header.size = sizeof(ev);
			ev.port_index = port;
			ev.data[0] = status | channel;
			ev.data[1] = key;
			ev.data[2] = value;
			m_events_in.push(&ev.header);
		}
	}
}


void qtractorClapPlugin::Impl::process (
	float **ins, float **outs, unsigned int nframes )
{
	if (!m_plugin)
		return;

	if (!m_activated)
		return;

	if (!m_processing && !m_sleeping) {
		plugin_params_flush();
		g_host.transportAddRef();
		m_processing = m_plugin->start_processing(m_plugin);
	}
	else
	if (m_processing && (m_sleeping || m_restarting)) {
		m_plugin->stop_processing(m_plugin);
		m_processing = false;
		g_host.transportReleaseRef();
		if (m_plugin->reset && !m_restarting)
			m_plugin->reset(m_plugin);
	}

	if (m_processing) {
		// Run main processing...
		m_audio_ins.data32 = ins;
		m_audio_outs.data32 = outs;
		m_events_out.clear();
		m_process.frames_count = nframes;
		m_plugin->process(m_plugin, &m_process);
		m_process.steady_time += nframes;
		m_events_in.clear();
		// Transfer parameter changes...
		process_params_out();
	}
}


// Plugin current latency (in frames);
unsigned long qtractorClapPlugin::Impl::latency (void) const
{
	if (m_plugin) {
		const clap_plugin_latency *latency
			= static_cast<const clap_plugin_latency *> (
				m_plugin->get_extension(m_plugin, CLAP_EXT_LATENCY));
		if (latency && latency->get)
			return latency->get(m_plugin);
	}

	return 0;
}


// Set/add a parameter value/point.
void qtractorClapPlugin::Impl::setParameter (
	clap_id id, double value )
{
	if (m_plugin) {
		const clap_param_info *param_info
			= m_param_infos.value(id, nullptr);
		if (param_info) {
			 clap_event_param_value ev;
			 ::memset(&ev, 0, sizeof(ev));
			 ev.header.time = 0;
			 ev.header.type = CLAP_EVENT_PARAM_VALUE;
			 ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
			 ev.header.flags = 0;
			 ev.header.size = sizeof(ev);
			 ev.param_id = param_info->id;
			 ev.cookie = param_info->cookie;
			 ev.port_index = 0;
			 ev.key = -1;
			 ev.channel = -1;
			 ev.value = value;
			 m_events_in.push(&ev.header);
		}
	}
}


// Get current parameter value.
double qtractorClapPlugin::Impl::getParameter ( clap_id id ) const
{
	double value = 0.0;

	if (m_plugin && m_params && m_params->get_value) {
		const clap_param_info *param_info
			= m_param_infos.value(id, nullptr);
		if (param_info)
			m_params->get_value(m_plugin, param_info->id, &value);
	}

	return value;
}


// Get current parameter value text.
QString qtractorClapPlugin::Impl::getParameterText (
	clap_id id, double value ) const
{
	QString sText;

	if (m_plugin && m_params && m_params->value_to_text) {
		const clap_param_info *param_info
			= m_param_infos.value(id, nullptr);
		if (param_info) {
			char szTemp[256];
			if (m_params->value_to_text(m_plugin,
					param_info->id, value,
					szTemp, sizeof(szTemp)))
				sText = QString::fromUtf8(szTemp);
		}
	}

	return sText;
}


// Plugin preset/state snapshot accessors.
bool qtractorClapPlugin::Impl::setState ( const QByteArray& data )
{
	if (!m_plugin)
		return false;

	if (m_state && m_state->load) {
		clap_istream stream;
		::memset(&stream, 0, sizeof(stream));
		stream.ctx = this;
		stream.read = Impl::plugin_state_read;
		m_state_data = data;
		if (m_state->load(m_plugin, &stream)) {
			m_state_data.clear();
			return true;
		}
	}

	return false;
}


bool qtractorClapPlugin::Impl::getState ( QByteArray& data )
{
	if (!m_plugin)
		return false;

	if (m_state && m_state->save) {
		clap_ostream stream;
		::memset(&stream, 0, sizeof(stream));
		stream.ctx = this;
		stream.write = Impl::plugin_state_write;
		m_state_data.clear();
		if (m_state->save(m_plugin, &stream)) {
			data = m_state_data;
			m_state_data.clear();
			return (data.size() > 0);
		}
	}

	return false;
}


// Plugin set/load state stream callbacks.
int64_t qtractorClapPlugin::Impl::plugin_state_read (
	const clap_istream *stream, void *buffer, uint64_t size )
{
	Impl *pImpl = static_cast<Impl *> (stream->ctx);
	return (pImpl ? pImpl->plugin_state_read_buffer(buffer, size) : -1);
}

int64_t qtractorClapPlugin::Impl::plugin_state_read_buffer (
	void *buffer, uint64_t size )
{
	if (size > m_state_data.size())
		size = m_state_data.size();
	::memcpy(buffer, m_state_data.constData(), size);
	m_state_data.remove(0, size);
	return size;
}


// Plugin get/save state stream callbacks.
int64_t qtractorClapPlugin::Impl::plugin_state_write (
	const clap_ostream *stream, const void *buffer, uint64_t size )
{
	Impl *pImpl = static_cast<Impl *> (stream->ctx);
	return (pImpl ? pImpl->plugin_state_write_buffer(buffer, size) : -1);
}

int64_t qtractorClapPlugin::Impl::plugin_state_write_buffer (
	const void *buffer, uint64_t size )
{
	m_state_data.append(QByteArray((const char *) buffer, size));
	return size;
}


// Plugin parameters flush.
void qtractorClapPlugin::Impl::plugin_params_flush (void)
{
	if (!m_plugin)
		return;

	if (!m_params_flush || m_processing)
		return;

	m_params_flush = false;

	m_events_in.clear();
	m_events_out.clear();

	if (m_params && m_params->flush) {
		m_params->flush(m_plugin, m_events_in.ins(), m_events_out.outs());
		process_params_out();
		m_events_out.clear();
	}
}


// Host extensions interface.
//
const void *qtractorClapPlugin::Impl::get_extension (
	const clap_host *host, const char *ext_id )
{
	const Impl *host_data = static_cast<const Impl *> (host->host_data);
	if (host_data) {
	#ifdef CONFIG_DEBUG_0
		qDebug("qtractorClapPlugin::Impl::get_extension(%p, \"%s\")", host, ext_id);
	#endif
		if (::strcmp(ext_id, CLAP_EXT_LOG) == 0)
			return &host_data->g_host_log;
		else
		if (::strcmp(ext_id, CLAP_EXT_GUI) == 0)
			return &host_data->g_host_gui;
		else
		if (::strcmp(ext_id, CLAP_EXT_PARAMS) == 0)
			return &host_data->g_host_params;
		else
		if (::strcmp(ext_id, CLAP_EXT_AUDIO_PORTS) == 0)
			return &host_data->g_host_audio_ports;
		else
		if (::strcmp(ext_id, CLAP_EXT_NOTE_PORTS) == 0)
			return &host_data->g_host_note_ports;
		else
		if (::strcmp(ext_id, CLAP_EXT_LATENCY) == 0)
			return &host_data->g_host_latency;
		else
		if (::strcmp(ext_id, CLAP_EXT_TIMER_SUPPORT) == 0)
			return &host_data->g_host_timer_support;
		else
		if (::strcmp(ext_id, CLAP_EXT_POSIX_FD_SUPPORT) == 0)
			return &host_data->g_host_posix_fd_support;
		else
		if (::strcmp(ext_id, CLAP_EXT_THREAD_CHECK) == 0)
			return &host_data->g_host_thread_check;
		else
		if (::strcmp(ext_id, CLAP_EXT_THREAD_POOL) == 0)
			return &host_data->g_host_thread_pool;
		else
		if (::strcmp(ext_id, CLAP_EXT_STATE) == 0)
			return &host_data->g_host_state;
		else
		if (::strcmp(ext_id, CLAP_EXT_NOTE_NAME) == 0)
			return &host_data->g_host_note_name;
	}
	return nullptr;
}


// Main host callbacks...
//
void qtractorClapPlugin::Impl::host_request_restart ( const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("clap_test_plugin::Impl::host_request_restart(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_request_restart();
}


void qtractorClapPlugin::Impl::host_request_process ( const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_request_process(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_request_process();
}


void qtractorClapPlugin::Impl::host_request_callback ( const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_request_callback(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_request_callback();
}


// Main plugin callbacks...
//
void qtractorClapPlugin::Impl::plugin_request_restart (void)
{
	if (m_restarting || qtractorAudioEngine::isProcessing())
		return;

	m_restarting = true;

	while (m_processing) {
		qtractorSession::stabilize();
	}

	m_restarting = false;

	deactivate(true);

	if (m_pPlugin)
		m_pPlugin->restart();

	activate();
}


void qtractorClapPlugin::Impl::plugin_request_process (void)
{
	// TODO: ?...
	//
}


void qtractorClapPlugin::Impl::plugin_request_callback (void)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	QMetaObject::invokeMethod(QCoreApplication::instance(),
		[this]{	m_plugin->on_main_thread(m_plugin); },
		Qt::QueuedConnection);
#endif
}


// Host LOG callbacks...
//
void qtractorClapPlugin::Impl::host_log (
	const clap_host *host, clap_log_severity severity, const char *msg )
{
	switch (severity) {
	case CLAP_LOG_DEBUG:
		qDebug("clap_log: Debug: %s", msg);
		break;
	case CLAP_LOG_INFO:
		qInfo("clap_log: Info: %s", msg);
		break;
	case CLAP_LOG_WARNING:
		qWarning("clap_log: Warning: %s", msg);
		break;
	case CLAP_LOG_ERROR:
		qWarning("clap_log: Error: %s", msg);
		break;
	case CLAP_LOG_FATAL:
		qWarning("clap_log: Fatal: %s", msg);
		break;
	case CLAP_LOG_HOST_MISBEHAVING:
		qWarning("clap_log: Host misbehaving: %s", msg);
		break;
	default:
		qDebug("clap_log: Unknown: %s", msg);
		break;
	}
}


// Host GUI callbacks...
//
void qtractorClapPlugin::Impl::host_gui_resize_hints_changed (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_gui_resize_hints_changed(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_gui_resize_hints_changed();
}


bool qtractorClapPlugin::Impl::host_gui_request_resize (
	const clap_host *host, uint32_t width, uint32_t height )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_gui_request_resize(%p, %d, %d)", host, width, height);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	return (pImpl ? pImpl->plugin_gui_request_resize(width, height) : false);
}


bool qtractorClapPlugin::Impl::host_gui_request_show (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_gui_request_show(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	return (pImpl ? pImpl->plugin_gui_request_show() : false);
}


bool qtractorClapPlugin::Impl::host_gui_request_hide (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_gui_request_hide(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	return (pImpl ? pImpl->plugin_gui_request_hide() : false);
}


void qtractorClapPlugin::Impl::host_gui_closed (
	const clap_host *host, bool was_destroyed )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_gui_closed(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_gui_closed(was_destroyed);
}


// Plugin GUI callbacks...
//
void qtractorClapPlugin::Impl::plugin_gui_resize_hints_changed (void)
{
	// TODO: ?...
	//
}


bool qtractorClapPlugin::Impl::plugin_gui_request_resize (
	uint32_t width, uint32_t height )
{
	if (m_pPlugin == nullptr)
		return false;

	QWidget *pWidget = m_pPlugin->editorWidget();
	if (pWidget == nullptr)
		return false;

	const QSize& max_size = pWidget->maximumSize();
	const QSize& min_size = pWidget->minimumSize();

	if (min_size.width() == max_size.width() && width != max_size.width())
		pWidget->setFixedWidth(width);
	if (min_size.height() == max_size.height() && height != max_size.height())
		pWidget->setFixedHeight(height);

	pWidget->resize(width, height);
	return true;
}


bool qtractorClapPlugin::Impl::plugin_gui_request_show (void)
{
	if (m_pPlugin == nullptr)
		return false;

	QWidget *pWidget = m_pPlugin->editorWidget();
	if (pWidget == nullptr)
		return false;

	pWidget->show();
	return true;
}


bool qtractorClapPlugin::Impl::plugin_gui_request_hide (void)
{
	if (m_pPlugin == nullptr)
		return false;

	QWidget *pWidget = m_pPlugin->editorWidget();
	if (pWidget == nullptr)
		return false;

	pWidget->hide();
	return true;
}


void qtractorClapPlugin::Impl::plugin_gui_closed ( bool was_destroyed )
{
	if (m_pPlugin)
		m_pPlugin->closeEditor();
}


// Host Parameters callbacks...
//
void qtractorClapPlugin::Impl::host_params_rescan (
	const clap_host *host, clap_param_rescan_flags flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_params_rescan(%p, 0x%04x)", host, flags);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_params_rescan(flags);
}


void qtractorClapPlugin::Impl::host_params_clear (
	const clap_host *host, clap_id param_id, clap_param_clear_flags flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_params_clear(%p, %u, 0x%04x)", host, param_id, flags);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_params_clear(param_id, flags);
}


void qtractorClapPlugin::Impl::host_params_request_flush (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorClapPlugin::Impl::host_params_request_flush(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_params_request_flush();
}


// Plugin Parameters callbacks...
//
void qtractorClapPlugin::Impl::plugin_params_rescan (
	clap_param_rescan_flags flags )
{
	if (m_pPlugin == nullptr)
		return;

	if (flags & CLAP_PARAM_RESCAN_VALUES)
		m_pPlugin->updateParamValues(false);
	else
	if (flags & (CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_TEXT)) {
		m_pPlugin->closeForm(true);
		m_pPlugin->clearParams();
		clearParamInfos();
		addParamInfos();
		m_pPlugin->addParams();
	}
	else
	if (flags & CLAP_PARAM_RESCAN_ALL)
		m_pPlugin->request_restart();
}


void qtractorClapPlugin::Impl::plugin_params_clear (
	clap_id param_id, clap_param_clear_flags flags )
{
	if (m_pPlugin == nullptr)
		return;

	if (!flags || param_id == CLAP_INVALID_ID)
		return;

	// Clear all automation curves, if any...
	qtractorSubject::resetQueue();

	// Clear any automation curves and direct access
	// references to the plugin parameter (param_id)...
	qtractorPlugin::Param *pParam = m_pPlugin->findParamId(int(param_id));
	if (pParam)
		m_pPlugin->clearParam(pParam);

	// And mark it dirty, of course...
	m_pPlugin->updateDirtyCount();
}


void qtractorClapPlugin::Impl::plugin_params_request_flush (void)
{
	m_params_flush = true;

//	plugin_params_flush();
}


// Host Audio Ports support callbacks...
//
bool qtractorClapPlugin::Impl::host_audio_ports_is_rescan_flag_supported (
	const clap_host *host, uint32_t flag )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_audio_ports_is_rescan_flag_supported(%p, 0x%04x)", host, flag);
#endif
	// Not supported.
	//
	return false;
}


void qtractorClapPlugin::Impl::host_audio_ports_rescan (
	const clap_host *host, uint32_t flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_audio_ports_rescan(%p, 0x%04x)", host, flags);
#endif
	// Not supported.
	//
}


// Host Note Ports support callbacks...
//
uint32_t qtractorClapPlugin::Impl::host_note_ports_supported_dialects (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_note_ports_supported_dialects(%p)", host);
#endif
	// Only MIDI 1.0 is scrictly supported.
	//
	return CLAP_NOTE_DIALECT_MIDI;
}


void qtractorClapPlugin::Impl::host_note_ports_rescan (
	const clap_host *host, uint32_t flags )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_note_ports_rescan(%p, 0x%04x)", host, flags);
#endif
	// Not supported.
	//
}


// Host Latency callbacks...
//
void qtractorClapPlugin::Impl::host_latency_changed (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_latency_changed(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_latency_changed();
}


// Plugin Latency callbacks...
//
void qtractorClapPlugin::Impl::plugin_latency_changed (void)
{
	if (m_pPlugin)
		m_pPlugin->request_restart();
}


// Host Timer support callbacks...
//
bool qtractorClapPlugin::Impl::host_register_timer (
	const clap_host *host, uint32_t period_ms, clap_id *timer_id )
{
	return g_host.register_timer(host, period_ms, timer_id);
}


bool qtractorClapPlugin::Impl::host_unregister_timer (
	const clap_host *host, clap_id timer_id )
{
	return g_host.unregister_timer(host, timer_id);
}


// Plugin Timer support callbacks...
//
void qtractorClapPlugin::Impl::plugin_on_timer ( clap_id timer_id )
{
	if (m_timer_support && m_timer_support->on_timer && m_plugin)
		m_timer_support->on_timer(m_plugin, timer_id);
}


// Host POSIX FD support callbacks...
//
bool qtractorClapPlugin::Impl::host_register_posix_fd (
	const clap_host *host, int fd, clap_posix_fd_flags_t flags )
{
	return g_host.register_posix_fd(host, fd, flags);
}


bool qtractorClapPlugin::Impl::host_modify_posix_fd (
	const clap_host *host, int fd, clap_posix_fd_flags_t flags )
{
	return g_host.modify_posix_fd(host, fd, flags);
}


bool qtractorClapPlugin::Impl::host_unregister_posix_fd (
	const clap_host *host, int fd )
{
	return g_host.unregister_posix_fd(host, fd);
}


// Plugin POSIX FD support callbacks...
//
void qtractorClapPlugin::Impl::plugin_on_posix_fd (
	int fd, clap_posix_fd_flags_t flags )
{
	if (m_posix_fd_support && m_posix_fd_support->on_fd && m_plugin)
		m_posix_fd_support->on_fd(m_plugin, fd, flags);
}


// Host thread-check callbacks...
//
bool qtractorClapPlugin::Impl::host_is_main_thread (
	const clap_host *host )
{
	return !qtractorAudioEngine::isProcessing();
}


bool qtractorClapPlugin::Impl::host_is_audio_thread (
	const clap_host *host )
{
	return  qtractorAudioEngine::isProcessing();
}


// Host thread-pool callbacks...
//
bool qtractorClapPlugin::Impl::host_thread_pool_request_exec (
	const clap_host *host, uint32_t num_tasks )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_thread_pool_request_exec(%p, %d)", host, num_tasks);
#endif
	// TODO: ?...
	//
	return false;
}


// Host state callbacks...
//
void qtractorClapPlugin::Impl::host_state_mark_dirty (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_state_mark_dirty(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_state_mark_dirty();
}


// Plugin state callbacks...
//
void qtractorClapPlugin::Impl::plugin_state_mark_dirty (void)
{
	if (m_pPlugin)
		m_pPlugin->updateDirtyCount();
}


// Host Note-names callbacks...
//
void qtractorClapPlugin::Impl::host_note_name_changed (
	const clap_host *host )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::Impl::host_note_name_changed(%p)", host);
#endif
	Impl *pImpl = static_cast<Impl *> (host->host_data);
	if (pImpl) pImpl->plugin_note_name_changed();
}


// Plugin Note-name callbacks...
//
void qtractorClapPlugin::Impl::plugin_note_name_changed (void)
{
	if (m_pPlugin)
		m_pPlugin->updateNoteNames();
}


// Transfer parameter changes...
void qtractorClapPlugin::Impl::process_params_out (void)
{
	const uint32_t nevents = m_events_out.size();
	for (uint32_t i = 0; i < nevents; ++i) {
		const clap_event_header *eh = m_events_out.get(i);
		if (eh && (
			eh->type == CLAP_EVENT_PARAM_VALUE ||
			eh->type == CLAP_EVENT_PARAM_GESTURE_BEGIN ||
			eh->type == CLAP_EVENT_PARAM_GESTURE_END)) {
			m_params_out.push(eh);
		}
	}
}


// MIDI specific dialect port counters (accessors).
int qtractorClapPlugin::Impl::midiDialectIns (void) const
{
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (m_pPlugin->type());
	return (pType ? pType->midiDialectIns() : 0);
}


int qtractorClapPlugin::Impl::midiDialectOuts (void) const
{
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (m_pPlugin->type());
	return (pType ? pType->midiDialectOuts() : 0);
}


//----------------------------------------------------------------------------
// qtractorClapPlugin::Param::Impl -- CLAP plugin parameter interface impl.
//

class qtractorClapPlugin::Param::Impl
{
public:

	// Constructor.
	Impl(const clap_param_info& param_info)
		: m_param_info(param_info) {}

	// Accessors.
	const clap_param_info& param_info() const
		{ return m_param_info; }

private:

	// Instance members.
	clap_param_info m_param_info;
};


//----------------------------------------------------------------------
// class qtractorClapPlugin::EditorWidget -- CLAP plugin editor widget decl.
//

class qtractorClapPlugin::EditorWidget : public QWidget
{
public:

	EditorWidget(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags());

	~EditorWidget ();

	void setPlugin (qtractorClapPlugin *pPlugin)
		{ m_pPlugin = pPlugin; }
	qtractorClapPlugin *plugin () const
		{ return m_pPlugin; }

	WId parentWinId() const { return QWidget::winId(); }

protected:

	void resizeEvent(QResizeEvent *pResizeEvent);
	void showEvent(QShowEvent *pShowEvent);
	void closeEvent(QCloseEvent *pCloseEvent);

private:

	qtractorClapPlugin *m_pPlugin;
	bool m_resizing;
};


//----------------------------------------------------------------------
// class qtractorClapPlugin::EditorWidget -- CLAP plugin editor widget impl.
//

qtractorClapPlugin::EditorWidget::EditorWidget (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags), m_pPlugin(nullptr), m_resizing(false)
{
}


qtractorClapPlugin::EditorWidget::~EditorWidget (void)
{
}


void qtractorClapPlugin::EditorWidget::resizeEvent (
	QResizeEvent *pResizeEvent )
{
	if (m_resizing)
		return;

	if (m_pPlugin == nullptr)
		return;

	const QSize& size = pResizeEvent->size();
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin::EditorWidget[%p]::resizeEvent(%d, %d)",
		this, size.width(), size.height());
#endif

	const clap_plugin *plugin
		= m_pPlugin->impl()->plugin();
	if (!plugin)
		return;

	const clap_plugin_gui *gui
		= m_pPlugin->impl()->gui();
	if (!gui)
		return;

	bool can_resize = false;
	if (gui->can_resize)
		can_resize = gui->can_resize(plugin);

	uint32_t width = 0;
	uint32_t height = 0;
	if (gui->get_size)
		gui->get_size(plugin, &width, &height);

	if (can_resize) {
		clap_gui_resize_hints hints;
		::memset(&hints, 0, sizeof(hints));
		if (gui->get_resize_hints)
			gui->get_resize_hints(plugin, &hints);
		if (hints.can_resize_horizontally)
			width = size.width();
		if (hints.can_resize_vertically)
			height = size.height();
		if (gui->adjust_size)
			gui->adjust_size(plugin, &width, &height);
		if (gui->set_size)
			gui->set_size(plugin, width, height);
	}

	if (width != size.width() || height != size.height()) {
		m_resizing = true;
		QWidget::resize(width, height);
		m_resizing = false;
	}
}


void qtractorClapPlugin::EditorWidget::showEvent ( QShowEvent *pShowEvent )
{
	QWidget::showEvent(pShowEvent);

	if (m_pPlugin)
		m_pPlugin->toggleFormEditor(true);
}


void qtractorClapPlugin::EditorWidget::closeEvent ( QCloseEvent *pCloseEvent )
{
	if (m_pPlugin)
		m_pPlugin->toggleFormEditor(false);

	QWidget::closeEvent(pCloseEvent);

	if (m_pPlugin)
		m_pPlugin->closeEditor();
}


//----------------------------------------------------------------------
// class qtractorClapPlugin -- CLAP plugin interface impl.
//

// Dynamic singleton list of CLAP plugins.
static QList<qtractorClapPlugin *> g_clapPlugins;

// Buffer size large enough to hold a regular MIDI channel event.
const long c_iMaxMidiData = 4;

// Constructor.
qtractorClapPlugin::qtractorClapPlugin (
   qtractorPluginList *pList, qtractorClapPluginType *pType )
	: qtractorPlugin(pList, pType), m_pImpl(new Impl(this)),
		m_bEditorCreated(false),
		m_bEditorVisible(false), m_pEditorWidget(nullptr),
		m_ppIBuffer(nullptr), m_ppOBuffer(nullptr),
		m_pfIDummy(nullptr), m_pfODummy(nullptr),
		m_pMidiParser(nullptr)
{
	initialize();
}


// Destructor.
qtractorClapPlugin::~qtractorClapPlugin (void)
{
	deinitialize();

	delete m_pImpl;
}


// Plugin instance initializer.
void qtractorClapPlugin::initialize (void)
{
	addParams();

	// Allocate I/O audio buffer pointers.
	const unsigned short iAudioIns  = audioIns();
	const unsigned short iAudioOuts = audioOuts();
	if (iAudioIns > 0)
		m_ppIBuffer = new float * [iAudioIns];
	if (iAudioOuts > 0)
		m_ppOBuffer = new float * [iAudioOuts];

	if (midiIns() > 0 &&
		snd_midi_event_new(c_iMaxMidiData, &m_pMidiParser) == 0)
		snd_midi_event_no_status(m_pMidiParser, 1);

	// Instantiate each instance properly...
	setChannels(channels());
}


// Plugin instance de-initializer.
void qtractorClapPlugin::deinitialize (void)
{
	// Cleanup all plugin instances...
	cleanup();	// setChannels(0);

	clearParams();
	clearNoteNames();

	// Deallocate I/O audio buffer pointers.
	if (m_ppIBuffer) {
		delete [] m_ppIBuffer;
		m_ppIBuffer = nullptr;
	}
	if (m_ppOBuffer) {
		delete [] m_ppOBuffer;
		m_ppOBuffer = nullptr;
	}

	if (m_pfIDummy) {
		delete [] m_pfIDummy;
		m_pfIDummy = nullptr;
	}
	if (m_pfODummy) {
		delete [] m_pfODummy;
		m_pfODummy = nullptr;
	}

	// Deallocate MIDI decoder.
	if (m_pMidiParser) {
		snd_midi_event_free(m_pMidiParser);
		m_pMidiParser = nullptr;
	}
}


// Channel/instance number accessors.
void qtractorClapPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (type());
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

	// Close old instances, all the way...
	const int iClapPlugin = g_clapPlugins.indexOf(this);
	if (iClapPlugin >= 0)
		g_clapPlugins.removeAt(iClapPlugin);

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setChannelsActivated(iChannels, bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::setChannels(%u) instances=%u",
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

	// Finally add it to the CLAP plugin roster...
	g_clapPlugins.append(this);

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// Initialize note-names cache.
	updateNoteNames();

	// (Re)activate instance if necessary...
	setChannelsActivated(iChannels, bActivated);
}


// Do the actual (de)activation.
void qtractorClapPlugin::activate (void)
{
	m_pImpl->activate();
}


void qtractorClapPlugin::deactivate (void)
{
	m_pImpl->deactivate();
}


// Instance parameters initializer.
void qtractorClapPlugin::addParams (void)
{
	const unsigned long nparams
		= m_pImpl->getParameterCount();
#ifdef CONFIG_DEBUG
	qDebug(" --- Parameters (nparams = %lu) ---", nparams);
#endif
	for (unsigned long i = 0; i < nparams; ++i) {
		const clap_id id = m_pImpl->getParameterId(i);
		if (id == CLAP_INVALID_ID)
			continue;
		const clap_param_info *param_info
			= m_pImpl->getParameterInfo(id);
		if (param_info) {
			if ( (param_info->flags & CLAP_PARAM_IS_AUTOMATABLE) &&
				!(param_info->flags & CLAP_PARAM_IS_READONLY)) {
				Param *pParam = new Param(this, i);
				m_paramIds.insert(int(param_info->id), pParam);
				addParam(pParam);
			}
		}
	}
}


void qtractorClapPlugin::clearParams (void)
{
	m_paramIds.clear();
	m_paramValues.clear();

	qtractorPlugin::clearParams();
}


void qtractorClapPlugin::clearParam ( qtractorPlugin::Param *pParam )
{
	if (pParam == nullptr)
		return;

	qtractorSubject *pSubject = pParam->subject();
	if (pSubject) {
		qtractorCurve *pCurve = pSubject->curve();
		if (pCurve) {
			qtractorCurveList *pCurveList = pCurve->list();
			if (pCurveList)
				pCurveList->removeCurve(pCurve);
			pSubject->setCurve(nullptr);
			delete pCurve;
		}
	}

	// Clear direct access, if any...
	if (directAccessParamIndex() == pParam->index())
		setDirectAccessParamIndex(-1);
}


// Parameter update method.
void qtractorClapPlugin::updateParam (
	qtractorPlugin::Param *pParam, float fValue, bool /*bUpdate*/ )
{
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (type());
	if (pType == nullptr)
		return;

	Param *pClapParam = static_cast<Param *> (pParam);
	if (pClapParam == nullptr)
		return;
	if (pClapParam->impl() == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorClapPlugin[%p]::updateParam(%lu, %g, %d)",
		this, pParam->index(), fValue, int(bUpdate));
#endif

	const clap_id id = pClapParam->impl()->param_info().id;
	const double value = double(fValue);
	m_pImpl->setParameter(id, value);
}


// All parameters update method.
void qtractorClapPlugin::updateParamValues ( bool bUpdate )
{
	int nupdate = 0;

	// Make sure all cached parameter values are in sync
	// with plugin parameter values; update cache otherwise.
	const qtractorPlugin::Params& params = qtractorPlugin::params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		Param *pParam = static_cast<Param *> (param.value());
		if (pParam && pParam->impl()) {
			const clap_id id = pParam->impl()->param_info().id;
			const float fValue = float(m_pImpl->getParameter(id));
			if (pParam->value() != fValue) {
				pParam->setValue(fValue, bUpdate);
				++nupdate;
			}
		}
	}

	if (nupdate > 0)
		updateFormDirtyCount();
}


// Parameter finder (by id).
qtractorPlugin::Param *qtractorClapPlugin::findParamId ( int id ) const
{
	return m_paramIds.value(id, nullptr);
}


// Configuration state stuff.
void qtractorClapPlugin::configure (
	const QString& sKey, const QString& sValue )
{
	if (sKey == "state") {
		// Load the BLOB (base64 encoded)...
		const QByteArray data
			= qUncompress(QByteArray::fromBase64(sValue.toLatin1()));
	#ifdef CONFIG_DEBUG
		qDebug("qtractorClapPlugin[%p]::configure() data.size=%d", this, int(data.size()));
	#endif
		m_pImpl->setState(data);
	}
}


// Plugin configuration/state snapshot.
void qtractorClapPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorClapPlugin[%p]::freezeConfigs()", this);
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

	clearConfigs();

	QByteArray data;

	if (!m_pImpl->getState(data))
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::freezeConfigs() data.size=%d", this, int(data.size()));
#endif

	// Set special plugin configuration item (base64 encoded)...
	QByteArray cdata = qCompress(data).toBase64();
	for (int i = cdata.size() - (cdata.size() % 72); i >= 0; i -= 72)
		cdata.insert(i, "\n       "); // Indentation.
	setConfig("state", cdata.constData());
}


void qtractorClapPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::releaseConfigs()", this);
#endif

	qtractorPlugin::clearConfigs();
}



// Provisional note name accessor.
bool qtractorClapPlugin::getNoteName ( int iIndex, NoteName& note ) const
{
	if (iIndex < 0 || iIndex >= m_noteNames.count())
		return false;

	note = *m_noteNames.at(iIndex);
	return true;
}


// Update/clear instrument/note names cache.
void qtractorClapPlugin::updateNoteNames (void)
{
	clearNoteNames();

	const clap_plugin *plugin = m_pImpl->plugin();
	if (!plugin)
		return;

	const clap_plugin_note_name *note_names
		= m_pImpl->note_names();;
	if (note_names && note_names->count && note_names->get) {
		const int ncount = note_names->count(plugin);
		for (int i = 0; i < ncount; ++i) {
			clap_note_name note_name;
			::memset(&note_name, 0, sizeof(note_name));
			if (note_names->get(plugin, i, &note_name)) {
				NoteName *note = new NoteName;
				note->bank = -1;
				note->prog = -1;
				note->note = note_name.key;
				note->name = note_name.name;
				m_noteNames.append(note);
			}
		}
	}
}


// Clear instrument/note names cache.
void qtractorClapPlugin::clearNoteNames (void)
{
	qDeleteAll(m_noteNames);
	m_noteNames.clear();
}


// Open/close editor widget.
void qtractorClapPlugin::openEditor ( QWidget *pParent )
{
	qtractorClapPluginType *pType
		= static_cast<qtractorClapPluginType *> (type());
	if (pType == nullptr)
		return;

	if (!pType->isEditor())
		return;

	// Is it already there?
	if (m_bEditorCreated && m_pEditorWidget) {
		if (!m_pEditorWidget->isVisible()) {
			moveWidgetPos(m_pEditorWidget, editorPos());
			m_pEditorWidget->show();
		}
		m_pEditorWidget->raise();
		m_pEditorWidget->activateWindow();
		return;
	}

	const clap_plugin *plugin = m_pImpl->plugin();
	if (!plugin)
		return;

	const clap_plugin_gui *gui = m_pImpl->gui();
	if (!gui)
		return;

	// Tell the world we'll (maybe) take some time...
	qtractorPluginList::WaitCursor waiting;

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::openEditor(%p)", this, pParent);
#endif

	const WId parent_wid
		= (pParent ? pParent->winId() : WId(nullptr));

	clap_window w;
	w.api = CLAP_WINDOW_API_X11;
	w.x11 = parent_wid;

	bool is_floating = false;
	if (!gui->is_api_supported(plugin, w.api, false))
		is_floating = gui->is_api_supported(plugin, w.api, true);

	if (!gui->create(plugin, w.api, is_floating)) {
		qWarning("qtractorClapPlugin[%p]::openEditor: could not create the plugin GUI.", this);
		return;
	}

	const QString& sTitle = pType->name();

	if (is_floating) {
		gui->set_transient(plugin, &w);
		gui->suggest_title(plugin, sTitle.toUtf8().constData());
	} else {
		// What style do we create tool childs?
		Qt::WindowFlags wflags = Qt::Window;
	#if 0//QTRACTOR_CLAP_EDITOR_TOOL
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bKeepToolsOnTop) {
			wflags |= Qt::Tool;
		//	wflags |= Qt::WindowStaysOnTopHint;
			// Make sure it has a parent...
			if (pParent == nullptr)
				pParent = qtractorMainForm::getInstance();
		}
	#endif
		m_pEditorWidget = new EditorWidget(pParent, wflags);
		m_pEditorWidget->setAttribute(Qt::WA_QuitOnClose, false);
		m_pEditorWidget->setWindowTitle(sTitle);
		m_pEditorWidget->setWindowIcon(QIcon(":/images/qtractorPlugin.svg"));
		w.x11 = m_pEditorWidget->parentWinId();
		if (!gui->set_parent(plugin, &w)) {
			qWarning("qtractorClapPlugin[%p]::openEditor: could not embbed the plugin GUI.", this);
			delete m_pEditorWidget;
			m_pEditorWidget = nullptr;
			gui->destroy(plugin);
			return;
		}
		bool can_resize = false;
		uint32_t width  = 0;
		uint32_t height = 0;
		clap_gui_resize_hints hints;
		::memset(&hints, 0, sizeof(hints));
		if (gui->can_resize)
			can_resize = gui->can_resize(plugin);
		if (gui->get_resize_hints && !gui->get_resize_hints(plugin, &hints))
			qWarning("qtractorClapPlugin[%p]::openEditor: could not get the resize hints of the plugin GUI.", this);
		if (gui->get_size && !gui->get_size(plugin, &width, &height))
			qWarning("qtractorClapPlugin[%p]::openEditor: could not get the size of the plugin GUI.", this);
		if (width > 0 && (!hints.can_resize_horizontally || !can_resize))
			m_pEditorWidget->setFixedWidth(width);
		if (height > 0 && (!hints.can_resize_vertically || !can_resize))
			m_pEditorWidget->setFixedHeight(height);
		if (width > 0 && height > 0)
			m_pEditorWidget->resize(width, height);
		m_pEditorWidget->setPlugin(this);
	}

	g_host.startTimer(200);

	m_bEditorCreated = true;
	m_bEditorVisible = false;

	// Final stabilization...
	updateEditorTitle();
	if (m_pEditorWidget)
		moveWidgetPos(m_pEditorWidget, editorPos());
	setEditorVisible(true);
}


void qtractorClapPlugin::closeEditor (void)
{
	if (!m_bEditorCreated)
		return;

	const clap_plugin *plugin = m_pImpl->plugin();
	if (!plugin)
		return;

	const clap_plugin_gui *gui = m_pImpl->gui();
	if (!gui)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

	g_host.stopTimer();

	gui->destroy(plugin);

	m_bEditorCreated = false;
	m_bEditorVisible = false;

	if (m_pEditorWidget) {
		delete m_pEditorWidget;
		m_pEditorWidget = nullptr;
	}
}


// Parameters post-update method.
void qtractorClapPlugin::idleEditor (void)
{
	int nupdate = 0;

	Impl::EventList& params_out = m_pImpl->params_out();
	const clap_event_header *eh = params_out.pop();
	for ( ; eh; eh = params_out.pop()) {
		int param_id = CLAP_INVALID_ID;
		double value = 0.0;
		// Check if we're not middle of a gesture...
		if (eh->type == CLAP_EVENT_PARAM_GESTURE_BEGIN) {
			const clap_event_param_gesture *ev
				= reinterpret_cast<const clap_event_param_gesture *> (eh);
			if (ev && ev->param_id != CLAP_INVALID_ID)
				m_paramValues.insert(int(ev->param_id), 0.0);
		}
		else
		if (eh->type == CLAP_EVENT_PARAM_GESTURE_END) {
			const clap_event_param_gesture *ev
				= reinterpret_cast<const clap_event_param_gesture *> (eh);
			if (ev && ev->param_id != CLAP_INVALID_ID) {
				param_id = int(ev->param_id);
				value = m_paramValues.value(param_id, 0.0);
				m_paramValues.remove(param_id);
			}
		}
		else
		if (eh->type == CLAP_EVENT_PARAM_VALUE) {
			const clap_event_param_value *ev
				= reinterpret_cast<const clap_event_param_value *> (eh);
			if (ev && ev->param_id != CLAP_INVALID_ID) {
				param_id = ev->param_id;
				value = ev->value;
				if (m_paramValues.contains(param_id)) {
					m_paramValues.insert(param_id, value);
					param_id = CLAP_INVALID_ID;
				}
			}
		}
		// Actual make the change...
		if (param_id != CLAP_INVALID_ID) {
			qtractorPlugin::Param *pParam = findParamId(param_id);
			if (pParam) {
				pParam->setValue(float(value), false);
				++nupdate;
			}
		}
	}
	params_out.clear();

	if (nupdate > 0)
		updateDirtyCount();
}


// GUI editor visibility state.
void qtractorClapPlugin::setEditorVisible ( bool bVisible )
{
	if (!m_bEditorCreated)
		return;

	const clap_plugin *plugin = m_pImpl->plugin();
	if (!plugin)
		return;

	const clap_plugin_gui *gui = m_pImpl->gui();
	if (!gui)
		return;

	if (m_pEditorWidget && !bVisible)
		setEditorPos(m_pEditorWidget->pos());

	if (bVisible && !m_bEditorVisible) {
		gui->show(plugin);
		if (m_pEditorWidget)
			m_pEditorWidget->show();
		m_bEditorVisible = true;
	}
	else
	if (!bVisible && m_bEditorVisible) {
		gui->hide(plugin);
		if (m_pEditorWidget)
			m_pEditorWidget->hide();
		m_bEditorVisible = false;
	}

	if (m_pEditorWidget && bVisible) {
		m_pEditorWidget->raise();
		m_pEditorWidget->activateWindow();
	}
}


bool qtractorClapPlugin::isEditorVisible (void) const
{
	return m_bEditorVisible;
}


// Update editor widget caption.
void qtractorClapPlugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	if (!m_bEditorCreated)
		return;

	const clap_plugin *plugin = m_pImpl->plugin();
	if (!plugin)
		return;

	const clap_plugin_gui *gui = m_pImpl->gui();
	if (!gui)
		return;

	if (m_pEditorWidget)
		m_pEditorWidget->setWindowTitle(sTitle);
	else
		gui->suggest_title(plugin, sTitle.toUtf8().constData());
}


// GUI editor widget handle (if not floating).
QWidget *qtractorClapPlugin::editorWidget (void) const
{
	return m_pEditorWidget;
}


// GUI editor created/active state.
bool qtractorClapPlugin::isEditorCreated (void) const
{
	return m_bEditorCreated;
}


// Processor stuff...
//

void qtractorClapPlugin::process_midi_in (
	unsigned char *data, unsigned int size,
	unsigned long offset, unsigned short port )
{
	m_pImpl->process_midi_in(data, size, offset, port);
}


void qtractorClapPlugin::process (
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
			Impl::EventList& events_out = m_pImpl->events_out();
			const uint32_t nevents = events_out.size();
			for (uint32_t i = 0; i < nevents; ++i) {
				const clap_event_header *eh
				  = events_out.get(i);
				if (eh) {
					snd_seq_event_t ev;
					snd_seq_ev_clear(&ev);
					switch (eh->type) {
					case CLAP_EVENT_NOTE_ON: {
						const clap_event_note *en
							= reinterpret_cast<const clap_event_note *> (eh);
						if (en) {
							ev.type = SND_SEQ_EVENT_NOTEON;
							ev.data.note.channel  = en->channel;
							ev.data.note.note     = en->key;
							ev.data.note.velocity = en->velocity;
						}
						break;
					}
					case CLAP_EVENT_NOTE_OFF: {
						const clap_event_note *en
							= reinterpret_cast<const clap_event_note *> (eh);
						if (en) {
							ev.type = SND_SEQ_EVENT_NOTEOFF;
							ev.data.note.channel  = en->channel;
							ev.data.note.note     = en->key;
							ev.data.note.velocity = en->velocity;
						}
						break;
					}
					case CLAP_EVENT_MIDI: {
						const clap_event_midi *em
							= reinterpret_cast<const clap_event_midi *> (eh);
						if (em) {
							unsigned char *pMidiData = (unsigned char *) &em->data[0];
							long iMidiData = sizeof(em->data);
							iMidiData = snd_midi_event_encode(m_pMidiParser,
								pMidiData, iMidiData, &ev);
							if (iMidiData < 1)
								ev.type = SND_SEQ_EVENT_NONE;
						}
						break;
					}}
					if (ev.type != SND_SEQ_EVENT_NONE)
						pMidiBuffer->push(&ev, eh->time);
				}
			}
			pMidiManager->swapOutputBuffers();
		} else {
			pMidiManager->resetOutputBuffers();
		}
	}
}


// Plugin current latency (in frames);
unsigned long qtractorClapPlugin::latency (void) const
{
	return m_pImpl->latency();
}


// Plugin preset i/o (configuration from/to state files).
bool qtractorClapPlugin::loadPresetFile ( const QString& sFilename )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "qtx")
		return qtractorPlugin::loadPresetFile(sFilename);

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::loadPresetFile(\"%s\")",
		this, sFilename.toUtf8().constData());
#endif

	QFile file(sFilename);

	if (!file.open(QFile::ReadOnly)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorClapPlugin[%p]::loadPresetFile(\"%s\")"
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


bool qtractorClapPlugin::savePresetFile ( const QString& sFilename )
{
	const QString& sExt = QFileInfo(sFilename).suffix().toLower();
	if (sExt == "qtx")
		return qtractorPlugin::savePresetFile(sFilename);

#ifdef CONFIG_DEBUG
	qDebug("qtractorClapPlugin[%p]::savePresetFile(\"%s\")",
		this, sFilename.toUtf8().constData());
#endif

	QByteArray data;

	if (!m_pImpl->getState(data))
		return false;

	QFile file(sFilename);

	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractorClapPlugin[%p]::savePresetFile(\"%s\")"
			" QFile::open(QFile::WriteOnly|QFile::Truncate) FAILED!",
			this, sFilename.toUtf8().constData());
	#endif
		return false;
	}

	file.write(data);
	file.close();
	return true;
}


// Make up some others dirty...
void qtractorClapPlugin::updateDirtyCount (void)
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->dirtyNotifySlot();

	updateFormDirtyCount();
}


// Idle editor (static).
void qtractorClapPlugin::idleEditorAll (void)
{
	QListIterator<qtractorClapPlugin *> iter(g_clapPlugins);
	while (iter.hasNext())
		iter.next()->idleEditor();
}


// Request to reinitialize th plugin instance...
void qtractorClapPlugin::request_restart (void)
{
	if (m_pImpl)
		m_pImpl->plugin_request_restart();
}


void qtractorClapPlugin::restart (void)
{
	// Clear all automation curves, if any...
	qtractorSubject::resetQueue();

	foreach (qtractorPlugin::Param *pParam, m_paramIds)
		clearParam(pParam);

	deinitialize();
	initialize();
}


// Common host-time keeper (static)
void qtractorClapPlugin::updateTime ( qtractorAudioEngine *pAudioEngine )
{
	g_host.updateTransport(pAudioEngine);
}


// Host cleanup (static).
void qtractorClapPlugin::clearAll (void)
{
	g_clapPlugins.clear();

	g_host.clear();
}


//----------------------------------------------------------------------------
// qtractorClapPlugin::Param -- CLAP plugin parameter interface decl.
//
// Constructors.
qtractorClapPlugin::Param::Param (
	qtractorClapPlugin *pPlugin, unsigned long iIndex )
	: qtractorPlugin::Param(pPlugin, iIndex), m_pImpl(nullptr)
{
	const clap_param_info *param_info = nullptr;
	const clap_id id = pPlugin->impl()->getParameterId(iIndex);
	if (id != CLAP_INVALID_ID)
		param_info = pPlugin->impl()->getParameterInfo(id);
	if (param_info) {
		m_pImpl = new Impl(*param_info);
		setName(QString::fromUtf8(param_info->name));
		setMinValue(float(param_info->min_value));
		setMaxValue(float(param_info->max_value));
		setDefaultValue(float(param_info->default_value));
	}
}


// Destructor.
qtractorClapPlugin::Param::~Param (void)
{
	if (m_pImpl) delete m_pImpl;
}


// Port range hints predicate methods.
bool qtractorClapPlugin::Param::isBoundedBelow (void) const
{
	return true;
}

bool qtractorClapPlugin::Param::isBoundedAbove (void) const
{
	return true;
}

bool qtractorClapPlugin::Param::isDefaultValue (void) const
{
	return true;
}

bool qtractorClapPlugin::Param::isLogarithmic (void) const
{
	return false;
}

bool qtractorClapPlugin::Param::isSampleRate (void) const
{
	return false;
}

bool qtractorClapPlugin::Param::isInteger (void) const
{
	bool ret = false;
	if (m_pImpl) {
		const clap_param_info& param_info
			= m_pImpl->param_info();
		ret = (param_info.flags & CLAP_PARAM_IS_STEPPED);
	}
	return ret;
}

bool qtractorClapPlugin::Param::isToggled (void) const
{
	bool ret = false;
	if (m_pImpl) {
		const clap_param_info& param_info
			= m_pImpl->param_info();
		ret = ((param_info.flags & CLAP_PARAM_IS_STEPPED)
			&& (param_info.min_value == 0.0)
			&& (param_info.max_value == 1.0));
	}
	return ret;
}

bool qtractorClapPlugin::Param::isDisplay (void) const
{
	return true;
}


// Current display value.
QString qtractorClapPlugin::Param::display (void) const
{
	QString sText;

	qtractorClapPlugin *pPlugin
		= static_cast<qtractorClapPlugin *> (plugin());
	if (pPlugin && m_pImpl) {
		const clap_id id = m_pImpl->param_info().id;
		const double value = pPlugin->impl()->getParameter(id);
		sText = pPlugin->impl()->getParameterText(id, value);
	}

	// Default parameter display value...
	return sText;
}


#endif	// CONFIG_CLAP

// end of qtractorClapPlugin.cpp
