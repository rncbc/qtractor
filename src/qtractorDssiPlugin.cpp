// qtractorDssiPlugin.cpp
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

#ifdef CONFIG_DSSI

#include "qtractorDssiPlugin.h"

#include "qtractorPluginForm.h"

#include "qtractorMidiBuffer.h"

#include <QFileInfo>
#include <QDir>


#ifdef CONFIG_LIBLO

#include <lo/lo.h>

#include <QProcess>


// DSSI GUI Editor instance.
struct DssiEditor
{
	// Constructor.
	DssiEditor(qtractorDssiPlugin *pPlugin)
		: plugin(pPlugin), target(NULL), source(NULL), path(NULL) {}

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
};


static lo_server_thread    g_oscThread;
static QString             g_sOscPath;
static QList<DssiEditor *> g_dssiEditors;


static DssiEditor *osc_find_editor ( qtractorDssiPlugin *pDssiPlugin )
{
	QListIterator<DssiEditor *> iter(g_dssiEditors);
	while (iter.hasNext()) {
		DssiEditor *pDssiEditor = iter.next();
		if (pDssiEditor->plugin == pDssiPlugin)
			return pDssiEditor;
	}

	return NULL;
}


static QString osc_label ( qtractorDssiPlugin *pDssiPlugin )
{
	return (pDssiPlugin->type())->label()
		+ '.' + QString::number(ulong(pDssiPlugin), 16);
}


static DssiEditor *osc_find_editor_by_label ( const QString& sOscLabel )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_find_editor_by_label(\"%s\")", sOscLabel.toUtf8().constData());
#endif

	QListIterator<DssiEditor *> iter(g_dssiEditors);
	while (iter.hasNext()) {
		DssiEditor *pDssiEditor = iter.next();
		qtractorDssiPlugin *pDssiPlugin = pDssiEditor->plugin; 
		if (pDssiPlugin && osc_label(pDssiPlugin) == sOscLabel)
			return pDssiEditor;
	}

	return NULL;
}


static void osc_error ( int num, const char *msg, const char *path )
{
	qWarning("osc_error: server error %d in path \"%s\": %s", num, path, msg);
}


static int osc_show ( DssiEditor *pDssiEditor )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_show: path \"%s\"", pDssiEditor->path);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/show";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(), "");

	return 0;
}


static int osc_hide ( DssiEditor *pDssiEditor )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_hide: path \"%s\"", pDssiEditor->path);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/hide";
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

	return osc_show(pDssiEditor);
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

	qtractorDssiPluginType *pDssiType
		= static_cast<qtractorDssiPluginType *> (pDssiPlugin->type());
	if (pDssiType == NULL)
		return 1;

	const DSSI_Descriptor *pDssiDescriptor
		= pDssiType->dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return 1;

	if (pDssiDescriptor->configure) {
		for (unsigned int i = 0; i < pDssiPlugin->instances(); ++i) {
			LADSPA_Handle ladspaHandle
				= pDssiPlugin->ladspa_handle(i);
			if (ladspaHandle)
				(*pDssiDescriptor->configure)(ladspaHandle, key, value);
		}
	}

	QString sPath(pDssiEditor->path);
	sPath += "/configure";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(),
		"ss", key, value);
	
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
	qtractorPluginParam *pParam = pDssiPlugin->findParam(param);
	if (pParam)
		pParam->setValue(value);

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
	pDssiPlugin->selectProgram(bank, prog);

	return 0;
}


static int osc_midi ( DssiEditor *pDssiEditor, lo_arg **argv )
{
	static snd_midi_event_t *s_pAlsaCoder = NULL;
	static snd_seq_event_t   s_aAlsaEvent[8];

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

	if (s_pAlsaCoder == NULL && snd_midi_event_new(8, &s_pAlsaCoder))
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


static int osc_quit ( DssiEditor *pDssiEditor )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_quit: path \"%s\"", pDssiEditor->path);
#endif

	QString sPath(pDssiEditor->path);
	sPath += "/quit";
	lo_send(pDssiEditor->target, sPath.toUtf8().constData(), "");

	return 0;
}


static int osc_exiting ( DssiEditor *pDssiEditor )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_exiting: path \"%s\"", pDssiEditor->path);
#endif

	qtractorDssiPlugin *pDssiPlugin	= pDssiEditor->plugin;
	if (pDssiPlugin && pDssiPlugin->isFormVisible())
		(pDssiPlugin->form())->toggleEditor(false);

	pDssiEditor->plugin = NULL;

	int iDssiEditor = g_dssiEditors.indexOf(pDssiEditor);
	if (iDssiEditor >= 0) {
		g_dssiEditors.removeAt(iDssiEditor);
		delete pDssiEditor;
	}

	return 0;
}


static int osc_message ( const char *path, const char *types,
	lo_arg **argv, int argc, void *data, void *user_data )
{
#ifdef CONFIG_DEBUG
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
	DssiEditor *pDssiEditor = osc_find_editor_by_label(sLabel);
	if (pDssiEditor == NULL)
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


static void osc_open_editor ( qtractorDssiPlugin *pDssiPlugin )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_open_editor(\"%s\")",
		osc_label(pDssiPlugin).toUtf8().constData());
#endif

	if (g_dssiEditors.count() < 1)
		osc_start();

	g_dssiEditors.append(new DssiEditor(pDssiPlugin));

}


static void osc_close_editor ( qtractorDssiPlugin *pDssiPlugin )
{
#ifdef CONFIG_DEBUG
	qDebug("osc_close_editor(\"%s\")",
		osc_label(pDssiPlugin).toUtf8().constData());
#endif

	DssiEditor *pDssiEditor = osc_find_editor(pDssiPlugin);
	if (pDssiEditor) {
		osc_hide(pDssiEditor);
		osc_quit(pDssiEditor);
		osc_exiting(pDssiEditor);
	}

	if (g_dssiEditors.count() < 1)
		osc_stop();
}


#endif


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
		this, file()->filename().toUtf8().constData(), index());
#endif

	// Things we have now for granted...
	m_iMidiIns = 1;

#ifdef CONFIG_LIBLO
	// Check for GUI editor exacutable...
	QFileInfo fi(file()->filename());
	QFileInfo gi(fi.dir(), fi.baseName());
	if (gi.isDir()) {
		QDir dir(gi.absoluteFilePath(), fi.baseName() + "_*");
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
		m_ppEvents(NULL), m_ppCounts(NULL),
		m_bEditorVisible(false)
{
	selectProgram(0, 0);
}


// Destructor.
qtractorDssiPlugin::~qtractorDssiPlugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);
}


// Channel/instance number accessors.
void qtractorDssiPlugin::setChannels ( unsigned short iChannels )
{
	// Clean up...
	if (m_ppEvents) {
		delete [] m_ppEvents;
		m_ppEvents = NULL;
	}

	if (m_ppCounts) {
		delete [] m_ppCounts;
		m_ppCounts = NULL;
	}

	// Setup new instances...
	qtractorLadspaPlugin::setChannels(iChannels);

	// Epilogue...
	unsigned short iInstances = instances();
	if (iInstances > 0) {
		m_ppEvents = new snd_seq_event_t * [iInstances];
		m_ppCounts = new unsigned long     [iInstances];
	}
}


// Do the actual activation.
void qtractorDssiPlugin::activate (void)
{
	qtractorLadspaPlugin::activate();
}


// Do the actual deactivation.
void qtractorDssiPlugin::deactivate (void)
{
	qtractorLadspaPlugin::deactivate();
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

	if (m_ppEvents == NULL || m_ppCounts == NULL)
		return;

	const LADSPA_Descriptor *pLadspaDescriptor = ladspa_descriptor();
	if (pLadspaDescriptor == NULL)
		return;

	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor == NULL)
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
		m_ppEvents[i] = pMidiManager->events();
		m_ppCounts[i] = pMidiManager->count();
		// Make it run...
		if (pDssiDescriptor->run_multiple_synths) {
			// Multiple run on last iteration only...
			if (i == iInstances - 1) {
				(*pDssiDescriptor->run_multiple_synths)(iInstances,
					m_phInstances, nframes, m_ppEvents, m_ppCounts);
			}
		}
		else if (pDssiDescriptor->run_synth) {
			(*pDssiDescriptor->run_synth)(handle, nframes,
				m_ppEvents[i], m_ppCounts[i]);
		}
		else (*pLadspaDescriptor->run)(handle, nframes);
		// Wrap channels?...
		if (iIChannel < iChannels - 1)
			iIChannel++;
		if (iOChannel < iChannels - 1)
			iOChannel++;
	}
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
	DssiEditor *pDssiEditor = osc_find_editor(this);
	if (pDssiEditor) {
		if (!m_bEditorVisible) {
			osc_show(pDssiEditor);
			m_bEditorVisible = true;
		}
		// Bail out.
		return;
	}

	// Open up a new one...
	osc_open_editor(this);

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
		osc_close_editor(this);
		return;
	}

#endif

	m_bEditorVisible = true;
	if (isFormVisible())
		form()->toggleEditor(true);
}


void qtractorDssiPlugin::closeEditor (void)
{
	if (isFormVisible())
		form()->toggleEditor(false);
	m_bEditorVisible = false;

#ifdef CONFIG_LIBLO
	osc_close_editor(this);
#endif
}


// GUI editor visibility state.
void qtractorDssiPlugin::setEditorVisible ( bool bVisible )
{
#ifdef CONFIG_LIBLO

	// Check if still here...
	DssiEditor *pDssiEditor = osc_find_editor(this);
	if (pDssiEditor == NULL) {
		if (bVisible) openEditor(form());
		return;
	}

	// Do our deeds...
	if (bVisible)
		osc_show(pDssiEditor);
	else
		osc_hide(pDssiEditor);

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
	if (m_phInstances == NULL)
		return;

	const DSSI_Descriptor *pDssiDescriptor = dssi_descriptor();
	if (pDssiDescriptor == NULL)
		return;

	if (pDssiDescriptor->select_program) {
		// For each plugin instance...
		for (unsigned short i = 0; i < instances(); ++i)
			(*pDssiDescriptor->select_program)(m_phInstances[i], iBank, iProg);
	}
}


// Specific accessor.
const DSSI_Descriptor *qtractorDssiPlugin::dssi_descriptor (void) const
{
	qtractorDssiPluginType *pDssiType
		= static_cast<qtractorDssiPluginType *> (type());
	return (pDssiType ? pDssiType->dssi_descriptor() : NULL);
}


//----------------------------------------------------------------------------
// qtractorDssiPluginParam -- DSSI plugin control input port instance.
//

// Constructors.
qtractorDssiPluginParam::qtractorDssiPluginParam (
	qtractorDssiPlugin *pDssiPlugin, unsigned long iIndex )
	: qtractorLadspaPluginParam(pDssiPlugin, iIndex)
{
}


// Destructor.
qtractorDssiPluginParam::~qtractorDssiPluginParam (void)
{
}

#endif	// CONFIG_DSSI

// end of qtractorDssiPlugin.cpp
