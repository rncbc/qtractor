// qtractorDssiPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifdef CONFIG_DSSI

#include "qtractorDssiPlugin.h"

#include "qtractorPluginForm.h"

#include "qtractorMidiBuffer.h"

#include <QFileInfo>
#include <QDir>


#ifdef CONFIG_LIBLO

#include <lo/lo.h>

#include <QProcess>
#include <QMutexLocker>


//----------------------------------------------------------------------------
// qtractorDssiPlugin::DssiEditor -- DSSI GUI Editor instance.
//
 
struct DssiEditor
{
	// Constructor.
	DssiEditor(qtractorDssiPlugin *pDssiPlugin)
		: plugin(pDssiPlugin), target(NULL), source(NULL), path(NULL), busy(0) {}

	// Destructor.
	~DssiEditor() {
		if (target)
			lo_address_free(target);
		if (source)
			lo_address_free(source);
		if (path)
			::free(path);
	}

	// Member variables.
	qtractorDssiPlugin *plugin;

	lo_address target;
	lo_address source;
	char      *path;
	int        busy;
};


static lo_server_thread g_oscThread;

static QString g_sOscPath;
static QMutex  g_oscMutex;

static QHash<QString, DssiEditor *> g_dssiEditors;


static QString osc_label ( qtractorDssiPlugin *pDssiPlugin )
{
	return (pDssiPlugin->type())->label()
		+ '.' + QString::number(ulong(pDssiPlugin), 16);
}


static DssiEditor *osc_find_editor ( const QString& sOscLabel )
{
#ifdef CONFIG_DEBUG_0
	qDebug("osc_find_editor(\"%s\")", sOscLabel.toUtf8().constData());
#endif

	return g_dssiEditors.value(sOscLabel, NULL);
}


static void osc_error ( int num, const char *msg, const char *path )
{
	qWarning("osc_error: server error %d in path \"%s\": %s", num, path, msg);
}


static int osc_send_configure ( DssiEditor *pDssiEditor,
	const char *key, const char *value )
{
	if (pDssiEditor->busy > 0)
		return 1;
	if (pDssiEditor->target == NULL)
		return 1;

#ifdef CONFIG_DEBUG
	qDebug("osc_send_configure: path \"%s\", key \"%s\", value \"%s\"",
		pDssiEditor->path, key, value);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/configure";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(),
		"ss", key, value);

	return 0;
}


static int osc_send_control ( DssiEditor *pDssiEditor,
	int param, float value )
{
	if (pDssiEditor->busy > 0)
		return 1;
	if (pDssiEditor->target == NULL)
		return 1;

#ifdef CONFIG_DEBUG
	qDebug("osc_send_control: path \"%s\", param %d, value %g",
		pDssiEditor->path, param, value);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/control";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(),
		"if", param, value);

	return 0;
}


static int osc_send_program ( DssiEditor *pDssiEditor,
	int bank, int prog )
{
	if (pDssiEditor->busy > 0)
		return 1;
	if (pDssiEditor->target == NULL)
		return 1;

#ifdef CONFIG_DEBUG
	qDebug("osc_send_program: path \"%s\", bank %d, prog %d",
		pDssiEditor->path, bank, prog);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/program";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(),
		"ii", bank, prog);

	return 0;
}


static int osc_send_show ( DssiEditor *pDssiEditor )
{
	if (pDssiEditor->busy > 0)
		return 1;
	if (pDssiEditor->target == NULL)
		return 1;

#ifdef CONFIG_DEBUG
	qDebug("osc_send_show: path \"%s\"", pDssiEditor->path);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/show";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(), "");

	return 0;
}


static int osc_send_hide ( DssiEditor *pDssiEditor )
{
	if (pDssiEditor->busy > 0)
		return 1;
	if (pDssiEditor->target == NULL)
		return 1;

#ifdef CONFIG_DEBUG
	qDebug("osc_send_hide: path \"%s\"", pDssiEditor->path);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/hide";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(), "");

	return 0;
}


static int osc_send_quit ( DssiEditor *pDssiEditor )
{
	if (pDssiEditor->busy > 0)
		return 1;
	if (pDssiEditor->target == NULL)
		return 1;

#ifdef CONFIG_DEBUG
	qDebug("osc_send_quit: path \"%s\"", pDssiEditor->path);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/quit";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(), "");

	return 0;
}


static int osc_update ( DssiEditor *pDssiEditor,
	lo_arg **argv, lo_address source )
{
	const char *url = (const char *) &argv[0]->s;
	const char *host, *port;

#ifdef CONFIG_DEBUG
	qDebug("osc_update: path \"%s\"", url);
#endif

	qtractorDssiPlugin *pDssiPlugin = pDssiEditor->plugin;
	if (pDssiPlugin == NULL)
		return 1;

	++(pDssiEditor->busy);

	if (pDssiEditor->target)
		lo_address_free(pDssiEditor->target);
	host = lo_url_get_hostname(url);
	port = lo_url_get_port(url);
	pDssiEditor->target = lo_address_new(host, port);
	::free((void *) host);
	::free((void *) port);

	if (pDssiEditor->source)
		lo_address_free(pDssiEditor->source);
	host = lo_address_get_hostname(source);
	port = lo_address_get_port(source);
	pDssiEditor->source = lo_address_new(host, port);

	if (pDssiEditor->path)
		::free(pDssiEditor->path);
	pDssiEditor->path = lo_url_get_path(url);

	--(pDssiEditor->busy);

	// Update plugin configuration...
	const qtractorPlugin::Configs& configs = pDssiPlugin->configs();
	qtractorPlugin::Configs::ConstIterator iter = configs.constBegin();
	const qtractorPlugin::Configs::ConstIterator& iter_end = configs.constEnd();
	for (; iter != iter_end; ++iter) {
		osc_send_configure(pDssiEditor,
			iter.key().toUtf8().constData(),
			iter.value().toUtf8().constData());
	}

	// Update program selection...
	qtractorMidiManager *pMidiManager = (pDssiPlugin->list())->midiManager();
	if (pMidiManager && pMidiManager->currentBank() >= 0 && pMidiManager->currentProg() >= 0) {
		osc_send_program(pDssiEditor,
			pMidiManager->currentBank(),
			pMidiManager->currentProg());
	}

	// Update control params...
	const qtractorPlugin::Params& params = pDssiPlugin->params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		osc_send_control(pDssiEditor,
			pParam->index(),
			pParam->value());
	}

	// Update all control output ports...
	pDssiPlugin->updateControlOuts(true);

	return osc_send_show(pDssiEditor);
}


static int osc_configure ( DssiEditor *pDssiEditor, lo_arg **argv )
{
    const char *key   = (const char *) &argv[0]->s;
    const char *value = (const char *) &argv[1]->s;

#ifdef CONFIG_DEBUG
	qDebug("osc_configure: path \"%s\", key \"%s\", value \"%s\"",
		pDssiEditor->path, key, value);
#endif

	qtractorDssiPlugin *pDssiPlugin = pDssiEditor->plugin;
	if (pDssiPlugin == NULL)
		return 1;

	// Save and send configuration to plugin...
	++(pDssiEditor->busy);
	pDssiPlugin->setConfig(key, value);
	pDssiPlugin->configure(key, value);
	--(pDssiEditor->busy);

	return 0;
}


static int osc_control ( DssiEditor *pDssiEditor, lo_arg **argv )
{
    int   param = argv[0]->i;
    float value = argv[1]->f;

#ifdef CONFIG_DEBUG
	qDebug("osc_control: path \"%s\", param %d, value %g",
		pDssiEditor->path, param, value);
#endif

	qtractorDssiPlugin *pDssiPlugin = pDssiEditor->plugin;
	if (pDssiPlugin == NULL)
		return 1;

	// Plugin parameter lookup.
	++(pDssiEditor->busy);
	pDssiPlugin->updateParamValue(param, value, true);
	--(pDssiEditor->busy);

	return 0;
}


static int osc_program ( DssiEditor *pDssiEditor, lo_arg **argv )
{
    int bank = argv[0]->i;
    int prog = argv[1]->i;

#ifdef CONFIG_DEBUG
	qDebug("osc_program: path \"%s\", bank %d, prog %d",
		pDssiEditor->path, bank, prog);
#endif

	qtractorDssiPlugin *pDssiPlugin = pDssiEditor->plugin;
	if (pDssiPlugin == NULL)
		return 1;

	// Bank/Program selection pending...
	++(pDssiEditor->busy);
	pDssiPlugin->selectProgram(bank, prog);
	if ((pDssiPlugin->list())->midiProgramSubject())
		((pDssiPlugin->list())->midiProgramSubject())->setProgram(bank, prog);
	--(pDssiEditor->busy);

	return 0;
}


static int osc_midi ( DssiEditor *pDssiEditor, lo_arg **argv )
{
	static snd_midi_event_t *s_pAlsaCoder = NULL;
	static snd_seq_event_t   s_aAlsaEvent[4];

	unsigned char *data = argv[0]->m;

#ifdef CONFIG_DEBUG
	qDebug("osc_midi: path \"%s\", midi 0x%02x 0x%02x 0x%02x 0x%02x",
		pDssiEditor->path, data[0], data[1], data[2], data[3]);
#endif

	qtractorDssiPlugin *pDssiPlugin = pDssiEditor->plugin;
	if (pDssiPlugin == NULL)
		return 1;

	qtractorMidiManager *pMidiManager = (pDssiPlugin->list())->midiManager();
	if (pMidiManager == NULL)
		return 1;

	if (s_pAlsaCoder == NULL && snd_midi_event_new(4, &s_pAlsaCoder))
		return 1;

	snd_midi_event_reset_encode(s_pAlsaCoder);	
	if (snd_midi_event_encode(s_pAlsaCoder, &data[1], 3, s_aAlsaEvent) < 1)
		return 1;

	// Send the event directly to
	snd_seq_event_t *pEvent = &s_aAlsaEvent[0];
	if (snd_seq_ev_is_channel_type(pEvent))
		pMidiManager->direct(pEvent);

	return 0;
}


static int osc_exiting ( DssiEditor *pDssiEditor )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_exiting: path \"%s\"", pDssiEditor->path);
#endif

	qtractorDssiPlugin *pDssiPlugin	= pDssiEditor->plugin;
	if (pDssiPlugin) {
		pDssiPlugin->clearEditor();
	}

	pDssiEditor->plugin = NULL;

	if (g_dssiEditors.remove(osc_label(pDssiPlugin)) > 0)
		delete pDssiEditor;

	return 0;
}


static int osc_message ( const char *path, const char * /*types*/,
	lo_arg **argv, int /*argc*/, void *data, void * /*user_data*/ )
{
	QMutexLocker locker(&g_oscMutex);

#ifdef CONFIG_DEBUG_0
	printf("osc_message: path \"%s\"", path);
	for (int i = 0; i < argc; ++i) {
		printf(", arg %d '%c' ", i, types[i]);
		lo_arg_pp(lo_type(types[i]), argv[i]);
	}
	printf(", data %p, user_data %p\n", data, user_data);
#endif

	if (::strncmp(path, "/dssi", 5))
		return 1;

	const QString sPath = path;
	const QString& sLabel = sPath.section('/', 2, 2);
	DssiEditor *pDssiEditor = osc_find_editor(sLabel);
	if (pDssiEditor == NULL)
		return 1;

	if (pDssiEditor->busy > 0)
		return 1;

	lo_message message = lo_message(data);
	lo_address source  = lo_message_get_source(message);
	
	const QString& sMethod = sPath.section('/', 3, 3);

	if (sMethod == "update")
		return osc_update(pDssiEditor, argv, source);
	else
	if (sMethod == "configure")
		return osc_configure(pDssiEditor, argv);
	else
	if (sMethod == "control")
		return osc_control(pDssiEditor, argv);
	else
	if (sMethod == "program")
		return osc_program(pDssiEditor, argv);
	else
	if (sMethod == "midi")
		return osc_midi(pDssiEditor, argv);
	else
	if (sMethod == "exiting")
		return osc_exiting(pDssiEditor);

	return 1;
}


static void osc_start (void)
{
	if (g_oscThread)
		return;

#ifdef CONFIG_DEBUG
	qDebug("osc_start()");
#endif

	// Create OSC thread...
	g_oscThread = lo_server_thread_new(NULL, osc_error);
	g_sOscPath  = lo_server_thread_get_url(g_oscThread);
	g_sOscPath += "dssi";

	lo_server_thread_add_method(g_oscThread,
		NULL, NULL, osc_message, NULL);
	lo_server_thread_start(g_oscThread);
}


static void osc_stop (void)
{
#ifdef CONFIG_DEBUG
	qDebug("osc_stop()");
#endif

	qDeleteAll(g_dssiEditors);
	g_dssiEditors.clear();
}


static DssiEditor *osc_open_editor ( qtractorDssiPlugin *pDssiPlugin )
{
	QMutexLocker locker(&g_oscMutex);

#ifdef CONFIG_DEBUG
	qDebug("osc_open_editor(\"%s\")",
		osc_label(pDssiPlugin).toUtf8().constData());
#endif

	if (g_dssiEditors.count() < 1)
		osc_start();

	DssiEditor *pDssiEditor = new DssiEditor(pDssiPlugin);
	g_dssiEditors.insert(osc_label(pDssiPlugin), pDssiEditor);

	return pDssiEditor;
}


static void osc_close_editor ( DssiEditor *pDssiEditor )
{
	QMutexLocker locker(&g_oscMutex);

#ifdef CONFIG_DEBUG
	qDebug("osc_close_editor(\"%s\")",
		osc_label(pDssiEditor->plugin).toUtf8().constData());
#endif

	osc_send_hide(pDssiEditor);
	osc_send_quit(pDssiEditor);
	osc_exiting(pDssiEditor);

	if (g_dssiEditors.count() < 1)
		osc_stop();
}


#endif

//----------------------------------------------------------------------------
// qtractorDssiPlugin::DssiMulti -- DSSI multiple instance pool entry.
//

static float       *g_pDummyBuffer     = NULL;
static unsigned int g_iDummyBufferSize = 0;

class DssiMulti
{
public:

	// Constructor.
	DssiMulti() : m_iSize(0), m_ppPlugins(NULL),
		m_iInstances(0), m_phInstances(NULL),
		m_ppEvents(NULL), m_piEvents(NULL),
		m_iProcess(0), m_iActivated(0), m_iRefCount(0) {}

	// Destructor.
	~DssiMulti()
	{
		if (m_piEvents)
			delete [] m_piEvents;
		if (m_ppEvents)
			delete [] m_ppEvents;
		if (m_phInstances)
			delete [] m_phInstances;
		if (m_ppPlugins)
			delete [] m_ppPlugins;
	}

	// Add plugin instances to registry pool.
	void addPlugin(qtractorDssiPlugin *pDssiPlugin)
	{
		unsigned long iNewInstances = m_iInstances + pDssiPlugin->instances();

		if (iNewInstances >= m_iSize) {
			m_iSize += iNewInstances;
			qtractorDssiPlugin **ppNewPlugins
				= new qtractorDssiPlugin * [m_iSize];
			LADSPA_Handle *phNewInstances
				= new LADSPA_Handle [m_iSize];
			qtractorDssiPlugin **ppOldPlugins = m_ppPlugins;
			LADSPA_Handle *phOldInstances = m_phInstances;
			if (ppOldPlugins && phOldInstances) {
				m_ppPlugins = NULL;
				m_phInstances = NULL;
				for (unsigned long i = 0; i < m_iInstances; ++i) {
					ppNewPlugins[i] = ppOldPlugins[i];
					phNewInstances[i] = phOldInstances[i];
				}
				delete [] phOldInstances;
				delete [] ppOldPlugins;
			}
			m_ppPlugins = ppNewPlugins;
			m_phInstances = phNewInstances;
			if (m_ppEvents)
				delete [] m_ppEvents;
			if (m_piEvents)
				delete [] m_piEvents;
			m_ppEvents = new snd_seq_event_t * [m_iSize];
			m_piEvents = new unsigned long     [m_iSize];
		}

		for (unsigned long i = 0; i < pDssiPlugin->instances(); ++i) {
			unsigned long iInstance = m_iInstances + i;
			m_ppPlugins[iInstance] = pDssiPlugin;
			m_phInstances[iInstance] = pDssiPlugin->ladspa_handle(i);
		}

		m_iInstances = iNewInstances;

		if (g_iDummyBufferSize < pDssiPlugin->list()->bufferSize()) {
			g_iDummyBufferSize = pDssiPlugin->list()->bufferSize();
			if (g_pDummyBuffer)
				delete [] g_pDummyBuffer;
			g_pDummyBuffer = new float [g_iDummyBufferSize];
		}

		reset(pDssiPlugin);
	}

	// Remove plugin instances from registry pool.
	void removePlugin(qtractorDssiPlugin *pDssiPlugin)
	{
		for (unsigned long i = 0; i < m_iInstances; ++i) {
			unsigned long j = i;
			while (j < m_iInstances && m_ppPlugins[j] == pDssiPlugin)
				++j;
			if (j > i) {
				unsigned long k = (j - i);
				for (; j < m_iInstances; ++j, ++i) {
					m_ppPlugins[i] = m_ppPlugins[j];
					m_phInstances[i] = m_phInstances[j];
				}
				m_iInstances -= k;
				break;
			}
		}
	}

	// Process registry pool.
	void process(const DSSI_Descriptor *pDssiDescriptor, unsigned int nframes)
	{
		// Count in only the active instances...
		if (++m_iProcess < m_iActivated)
			return;

		for (unsigned long i = 0; i < m_iInstances; ++i) {
			// Set MIDI event lists...
			qtractorMidiManager *pMidiManager
				= m_ppPlugins[i]->list()->midiManager();
			if (pMidiManager) {
				m_ppEvents[i] = pMidiManager->events();
				m_piEvents[i] = pMidiManager->count();
			} else {
				m_ppEvents[i] = NULL;
				m_piEvents[i] = 0;
			}
		}

		(*pDssiDescriptor->run_multiple_synths)(m_iInstances,
			m_phInstances, nframes, m_ppEvents, m_piEvents);

		m_iProcess = 0;
	}

	// Activation count methods.
	void activate(qtractorDssiPlugin *pDssiPlugin)
		{ m_iActivated += pDssiPlugin->instances(); }
	void deactivate(qtractorDssiPlugin *pDssiPlugin)
		{ m_iActivated -= pDssiPlugin->instances(); reset(pDssiPlugin); }

	// Reference count methods.
	void addRef()
		{ ++m_iRefCount; }
	void removeRef()
		{ if (--m_iRefCount == 0) delete this; }

	// Reset connections.
	void reset(qtractorDssiPlugin *pDssiPlugin)
	{
		// One must connect the ports of all inactive
		// instances somewhere, otherwise it will crash...
		const LADSPA_Descriptor *pLadspaDescriptor
			= pDssiPlugin->ladspa_descriptor();
		unsigned short iAudioIns  = pDssiPlugin->audioIns();
		unsigned short iAudioOuts = pDssiPlugin->audioOuts();
		for (unsigned short i = 0; i < pDssiPlugin->instances(); ++i) {
			LADSPA_Handle handle = pDssiPlugin->ladspa_handle(i);
			for (unsigned short j = 0; j < iAudioIns; ++j) {
				(*pLadspaDescriptor->connect_port)(handle,
					pDssiPlugin->audioIn(j), g_pDummyBuffer);
			}
			for (unsigned short j = 0; j < iAudioOuts; ++j) {
				(*pLadspaDescriptor->connect_port)(handle,
					pDssiPlugin->audioOut(j), g_pDummyBuffer);
			}
		}
	}

private:

	// Member variables.
	unsigned long        m_iSize;
	qtractorDssiPlugin **m_ppPlugins;
	unsigned long        m_iInstances;
	LADSPA_Handle       *m_phInstances;
	snd_seq_event_t    **m_ppEvents;
	unsigned long       *m_piEvents;
	unsigned long        m_iProcess;
	unsigned long        m_iActivated;
	unsigned int         m_iRefCount;
};

// DSSI multiple instance pool
// for each DSSI plugin implementing run_multiple_synths
// one must keep a global registry of all instances here.
static QHash<QString, DssiMulti *> g_dssiHash;


// DSSI multiple instance key helper.
static QString dssi_multi_key ( qtractorDssiPluginType *pDssiType )
{
	return pDssiType->filename() + '_' + pDssiType->label();
}


// DSSI multiple instance entry constructor.
static DssiMulti *dssi_multi_create ( qtractorDssiPluginType *pDssiType )
{
	const DSSI_Descriptor *pDssiDescriptor = pDssiType->dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return NULL;
	if (pDssiDescriptor->run_multiple_synths == NULL)
		return NULL;

	DssiMulti *pDssiMulti = NULL;
	const QString& sKey = dssi_multi_key(pDssiType);
	QHash<QString, DssiMulti *>::const_iterator iter
		= g_dssiHash.constFind(sKey);
	if (iter == g_dssiHash.constEnd()) {
		pDssiMulti = new DssiMulti();
		g_dssiHash.insert(sKey, pDssiMulti);
	} else {
		pDssiMulti = iter.value();
	}
	pDssiMulti->addRef();

	return pDssiMulti;
}


// DSSI multiple instance entry destructor.
void dssi_multi_destroy ( qtractorDssiPluginType *pDssiType )
{
	// Remove hash table entry...
	const QString& sKey = dssi_multi_key(pDssiType);
	QHash<QString, DssiMulti *>::Iterator iter = g_dssiHash.find(sKey);
	if (iter == g_dssiHash.end())
		return;
	iter.value()->removeRef();
	g_dssiHash.erase(iter);

	// On last entry deallocate dummy buffer as well...
	if (g_dssiHash.isEmpty() && g_pDummyBuffer) {
		delete [] g_pDummyBuffer;
		g_pDummyBuffer = NULL;
		g_iDummyBufferSize = 0;
	}
}


//----------------------------------------------------------------------------
// qtractorDssiPluginType -- DSSI plugin type instance.
//

// Derived methods.
bool qtractorDssiPluginType::open (void)
{
	// Do we have a descriptor already?
	if (m_pDssiDescriptor == NULL)
		m_pDssiDescriptor = dssi_descriptor(file(), index());
	if (m_pDssiDescriptor == NULL)
		return false;

	// We're also a LADSPA one...
	m_pLadspaDescriptor = m_pDssiDescriptor->LADSPA_Plugin;

	// Let's get the it's own LADSPA stuff...
	if (!qtractorLadspaPluginType::open()) {
		m_pLadspaDescriptor = NULL;
		m_pDssiDescriptor = NULL;
		return false;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorDssiPluginType[%p]::open() filename=\"%s\" index=%lu",
		this, filename().toUtf8().constData(), index());
#endif

	// Things we have now for granted...
	m_bConfigure = (m_pDssiDescriptor->configure != NULL);
	m_iMidiIns = 1;

#ifdef CONFIG_LIBLO
	// Check for GUI editor exacutable...
	QFileInfo fi(filename());
	QFileInfo gi(fi.dir(), fi.baseName());
	if (gi.isDir()) {
		QDir dir(gi.absoluteFilePath());
		const QString sMask("%1_*");
		QStringList names;
		names.append(sMask.arg(fi.baseName()));
		names.append(sMask.arg(m_pLadspaDescriptor->Label));
		dir.setNameFilters(names);
		QStringList guis = dir.entryList(QDir::Files | QDir::Executable);
		if (!guis.isEmpty()) {
			gi.setFile(dir, guis.first()); // FIXME: Only the first?
			m_sDssiEditor = gi.absoluteFilePath();
			m_bEditor = gi.isExecutable();
		}
    }
#endif

	return true;
}


void qtractorDssiPluginType::close (void)
{
	m_pDssiDescriptor = NULL;
	qtractorLadspaPluginType::close();
}


// Factory method (static)
qtractorDssiPluginType *qtractorDssiPluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Sanity check...
	if (pFile == NULL)
		return NULL;

	// Retrieve DSSI descriptor if any...
	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor(pFile, iIndex);
	if (pDssiDescriptor == NULL)
		return NULL;

	// Yep, most probably its a valid plugin descriptor...
	return new qtractorDssiPluginType(pFile, iIndex, pDssiDescriptor);
}


// Descriptor method (static)
const DSSI_Descriptor *qtractorDssiPluginType::dssi_descriptor (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Retrieve the DSSI descriptor function, if any...
	DSSI_Descriptor_Function pfnDssiDescriptor
		= (DSSI_Descriptor_Function) pFile->resolve("dssi_descriptor");
	if (pfnDssiDescriptor == NULL)
		return NULL;

	// Retrieve DSSI descriptor if any...
	return (*pfnDssiDescriptor)(iIndex);
}


//----------------------------------------------------------------------------
// qtractorDssiPlugin -- DSSI plugin instance.
//

// Constructors.
qtractorDssiPlugin::qtractorDssiPlugin ( qtractorPluginList *pList,
	qtractorDssiPluginType *pDssiType )
	: qtractorLadspaPlugin(pList, pDssiType),
		m_pDssiMulti(NULL), m_pDssiEditor(NULL),
		m_bEditorVisible(false), m_pfControlOutsLast(NULL)
{
	// Check whether we're go into a multiple instance pool.
	m_pDssiMulti = dssi_multi_create(pDssiType);

	// For tracking changes on output control ports.
	unsigned long iControlOuts = pDssiType->controlOuts();
	if (iControlOuts > 0) {
		m_pfControlOutsLast = new float [iControlOuts];
		for (unsigned long j = 0; j < iControlOuts; ++j)
			m_pfControlOutsLast[j] = 0.0f;
	}

	// Extended first instantiantion.
	resetChannels();
}


// Destructor.
qtractorDssiPlugin::~qtractorDssiPlugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);

	// Remove reference from multiple instance pool.
	if (m_pDssiMulti)
		dssi_multi_destroy(static_cast<qtractorDssiPluginType *> (type()));

	// Remove last output control port values seen...
	if (m_pfControlOutsLast)
		delete [] m_pfControlOutsLast;
}


// Channel/instance number accessors.
void qtractorDssiPlugin::setChannels ( unsigned short iChannels )
{
	// (Re)set according to existing instances...
	if (m_pDssiMulti)
		m_pDssiMulti->removePlugin(this);

	// Setup new instances...
	qtractorLadspaPlugin::setChannels(iChannels);

	// Epilogue...
	resetChannels();
}


// Post-(re)initializer.
void qtractorDssiPlugin::resetChannels (void)
{
	// (Re)initialize controller port map, anyway.
	::memset(m_apControllerMap, 0, 128 * sizeof(qtractorPluginParam *));

	// Check how many instances are about there...
	unsigned short iInstances = instances();
	if (iInstances < 1)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorDssiPlugin[%p]::resetChannels() instances=%u", this, iInstances);
#endif

	// (Re)set according to existing instances...
	if (m_pDssiMulti)
		m_pDssiMulti->addPlugin(this);

	// Map all existing input control ports...
	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor
		&& pDssiDescriptor->get_midi_controller_for_port) {
		// Only the first one instance should matter...
		LADSPA_Handle handle = m_phInstances[0];
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			int iController
				= (*pDssiDescriptor->get_midi_controller_for_port)(
					handle, pParam->index());
			if (iController > 0 && DSSI_IS_CC(iController))
				m_apControllerMap[DSSI_CC_NUMBER(iController)] = pParam;
		}
	}
}


// Do the actual activation.
void qtractorDssiPlugin::activate (void)
{
	// Activate as usual...
	if (m_pDssiMulti)
		m_pDssiMulti->activate(this);

	qtractorLadspaPlugin::activate();
}


// Do the actual deactivation.
void qtractorDssiPlugin::deactivate (void)
{
	// Deactivate as usual...
	qtractorLadspaPlugin::deactivate();

	if (m_pDssiMulti)
		m_pDssiMulti->deactivate(this);
}


// The main plugin processing procedure.
void qtractorDssiPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	// Get MIDI manager access...
	qtractorMidiManager *pMidiManager = list()->midiManager();
	if (pMidiManager == NULL) {
		qtractorLadspaPlugin::process(ppIBuffer, ppOBuffer, nframes);
		return;
	}

	if (m_phInstances == NULL)
		return;

	const LADSPA_Descriptor *pLadspaDescriptor = ladspa_descriptor();
	if (pLadspaDescriptor == NULL)
		return;

	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return;
	
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
		LADSPA_Handle handle = m_phInstances[i];
		// For each instance audio input port...
		for (j = 0; j < iAudioIns; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piAudioIns[j], ppIBuffer[iIChannel]);
			if (++iIChannel >= iChannels)
				iIChannel = 0;
		}
		// For each instance audio output port...
		for (j = 0; j < iAudioOuts; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piAudioOuts[j], ppOBuffer[iOChannel]);
			if (++iOChannel >= iChannels)
				iOChannel = 0;
		}
		// Care of multiple instances here...
		if (m_pDssiMulti)
			m_pDssiMulti->process(pDssiDescriptor, nframes);
		// Make it run...
		else if (pDssiDescriptor->run_synth) {
			(*pDssiDescriptor->run_synth)(handle, nframes,
				pMidiManager->events(), pMidiManager->count());
		}
		else (*pLadspaDescriptor->run)(handle, nframes);
		// Wrap channels?...
		if (iIChannel < iChannels - 1)
			++iIChannel;
		if (iOChannel < iChannels - 1)
			++iOChannel;
	}
}


// Parameter update method.
void qtractorDssiPlugin::updateParam (
	qtractorPluginParam *pParam, float fValue, bool bUpdate )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorDssiPlugin[%p]::updateParam(%lu, %g, %d)",
		this, pParam->index(), fValue, int(bUpdate));
#endif

#ifdef CONFIG_LIBLO
	// And update the editor too...
	if (bUpdate && m_pDssiEditor)
		osc_send_control(m_pDssiEditor, pParam->index(), fValue);
#endif
}


// GUI Editor stuff.
void qtractorDssiPlugin::openEditor ( QWidget */*pParent*/ )
{
	qtractorDssiPluginType *pDssiType
		= static_cast<qtractorDssiPluginType *> (type());
	if (pDssiType == NULL)
		return;
	if (!pDssiType->isEditor())
		return;

#ifdef CONFIG_LIBLO

	// Are we already there?
	if (m_pDssiEditor) {
		osc_send_show(m_pDssiEditor);
		m_bEditorVisible = true;
		// Bail out.
		return;
	}

	// Open up a new one...
	m_pDssiEditor = osc_open_editor(this);

	const QString& sDssiEditor = pDssiType->dssi_editor();

	QStringList args;
	args.append(g_sOscPath + '/' + osc_label(this));
	args.append(QFileInfo((pDssiType->file())->filename()).fileName());
	args.append(pDssiType->label());
	args.append(pDssiType->name());

#ifdef CONFIG_DEBUG
	qDebug("qtractorDssiPlugin::openEditor() "
		"path=\"%s\" url=\"%s\" filename=\"%s\" label=\"%s\" name=\"%s\"",
		sDssiEditor.toUtf8().constData(),
		args[0].toUtf8().constData(),
		args[1].toUtf8().constData(),
		args[2].toUtf8().constData(),
		args[3].toUtf8().constData());
#endif

	if (!QProcess::startDetached(sDssiEditor, args)) {
		closeEditor();
		return;
	}

#endif

	m_bEditorVisible = true;
	if (isFormVisible())
		form()->toggleEditor(true);
}


void qtractorDssiPlugin::closeEditor (void)
{
#ifdef CONFIG_LIBLO
	if (m_pDssiEditor)
		osc_close_editor(m_pDssiEditor);
#else
	clearEditor();
#endif
}


// GUI editor visibility state.
void qtractorDssiPlugin::setEditorVisible ( bool bVisible )
{
#ifdef CONFIG_LIBLO

	// Check if still here...
	if (m_pDssiEditor == NULL) {
		if (bVisible) openEditor(form());
		return;
	}

	// Do our deeds...
	if (bVisible)
		osc_send_show(m_pDssiEditor);
	else
		osc_send_hide(m_pDssiEditor);

#endif

	if (isFormVisible())
		form()->toggleEditor(bVisible);
	m_bEditorVisible = bVisible;
}

bool qtractorDssiPlugin::isEditorVisible (void) const
{
	return m_bEditorVisible;
}


// Bank/program selector.
void qtractorDssiPlugin::selectProgram ( int iBank, int iProg )
{
	if (iBank < 0 || iProg < 0)
		return;

	if (m_phInstances == NULL)
		return;

	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return;

	if (pDssiDescriptor->select_program == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorDssiPlugin[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	// For each plugin instance...
	for (unsigned short i = 0; i < instances(); ++i)
		(*pDssiDescriptor->select_program)(m_phInstances[i], iBank, iProg);

#ifdef CONFIG_LIBLO
	// And update the editor too...
	if (m_pDssiEditor)
		osc_send_program(m_pDssiEditor, iBank, iProg);
#endif

	// Reset parameters default value...
	const qtractorPlugin::Params& params = qtractorPlugin::params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		pParam->setDefaultValue(pParam->value());
	}
}


// Provisional program/patch accessor.
bool qtractorDssiPlugin::getProgram ( int iIndex, Program& program ) const
{
	if (m_phInstances == NULL)
		return false;

	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return false;

	if (pDssiDescriptor->get_program == NULL)
		return false;

	// Only first one instance should matter...
	const DSSI_Program_Descriptor *pDssiProgram
		= (*pDssiDescriptor->get_program)(m_phInstances[0], iIndex);
	if (pDssiProgram == NULL)
		return false;

	// Map this to that...
	program.bank = pDssiProgram->Bank;
	program.prog = pDssiProgram->Program;
	program.name = pDssiProgram->Name;

	return true;
}


// MIDI continuous controller handler.
void qtractorDssiPlugin::setController ( int iController, int iValue )
{
	qtractorPluginParam *pParam
		= m_apControllerMap[DSSI_CC_NUMBER(iController)];
	if (pParam == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorDssiPlugin[%p]::setController(%d, %d) index=%d",
		this, iController, iValue, int(pParam->index()));
#endif

	float fValue = float(iValue) / 127.0f;
	pParam->setValue(fValue, true);
}


// Configuration (CLOB) stuff.
void qtractorDssiPlugin::configure ( const QString& sKey, const QString& sValue )
{
	if (m_phInstances == NULL)
		return;

	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return;

	if (pDssiDescriptor->configure == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorDssiPlugin[%p]::configure(\"%s\", \"%s\")",
		this, sKey.toUtf8().constData(), sValue.toUtf8().constData());
#endif

	// For each plugin instance...
	for (unsigned short i = 0; i < instances(); ++i) {
		(*pDssiDescriptor->configure)(m_phInstances[i],
			sKey.toUtf8().constData(),
			sValue.toUtf8().constData());
	}

#ifdef CONFIG_LIBLO
	if (m_pDssiEditor) {
		osc_send_configure(m_pDssiEditor,
			sKey.toUtf8().constData(),
			sValue.toUtf8().constData());
	}
#endif
}


// Specific accessor.
const DSSI_Descriptor *qtractorDssiPlugin::dssi_descriptor (void) const
{
	qtractorDssiPluginType *pDssiType
		= static_cast<qtractorDssiPluginType *> (type());
	return (pDssiType ? pDssiType->dssi_descriptor() : NULL);
}


// Update all control output ports...
void qtractorDssiPlugin::updateControlOuts ( bool bForce )
{
#ifdef CONFIG_LIBLO
	if (m_pDssiEditor && m_piControlOuts && m_pfControlOuts) {
		unsigned long iControlOuts = type()->controlOuts();
		for (unsigned long j = 0; j < iControlOuts; ++j) {
		//	if (::fabs(m_pfControlOuts[j] - m_pfControlOutsLast[j]) > 1e-6f) {
			if (m_pfControlOutsLast[j] != m_pfControlOuts[j] || bForce) {
				osc_send_control(m_pDssiEditor,
					m_piControlOuts[j],
					m_pfControlOuts[j]);
				m_pfControlOutsLast[j] = m_pfControlOuts[j];
			}
		}
	}
#endif
}


// Reset(null) internal editor reference.
void qtractorDssiPlugin::clearEditor (void)
{
	m_pDssiEditor = NULL;

	if (isFormVisible())
		form()->toggleEditor(false);
	m_bEditorVisible = false;
}


// Idle editor update (static)
void qtractorDssiPlugin::idleEditorAll (void)
{
#ifdef CONFIG_LIBLO
	QHash<QString, DssiEditor *>::ConstIterator iter
		= g_dssiEditors.constBegin();
	const QHash<QString, DssiEditor *>::ConstIterator& iter_end
		= g_dssiEditors.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorDssiPlugin *pDssiPlugin = iter.value()->plugin;
		if (pDssiPlugin)
			pDssiPlugin->updateControlOuts();
	}
#endif
}


#endif	// CONFIG_DSSI

// end of qtractorDssiPlugin.cpp
