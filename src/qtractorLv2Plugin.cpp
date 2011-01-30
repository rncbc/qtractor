// qtractorLv2Plugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifdef CONFIG_LV2

#include "qtractorLv2Plugin.h"

#ifdef CONFIG_LV2_EVENT
#include "qtractorMidiBuffer.h"
#endif

// URI map feature.
#include "lv2_uri_map.h"

#ifdef CONFIG_LV2_PERSIST
#define LV2_ATOM_STRING_URI "http://lv2plug.in/ns/ext/atom#String"
#endif

static uint32_t qtractor_lv2_uri_to_id (
	LV2_URI_Map_Callback_Data /*data*/, const char *map, const char *uri )
{
#ifdef CONFIG_LV2_EVENT
	if ((map && strcmp(map, LV2_EVENT_URI) == 0) &&
		strcmp(uri, SLV2_EVENT_CLASS_MIDI) == 0)
		return QTRACTOR_LV2_MIDI_EVENT_ID;
	else
#endif
#ifdef CONFIG_LV2_PERSIST
	if (strcmp(uri, LV2_ATOM_STRING_URI) == 0)
		return QTRACTOR_LV2_ATOM_STRING_ID;
	else
#endif
	return 0;
}

static LV2_URI_Map_Feature g_lv2_uri_map =
	{ NULL, qtractor_lv2_uri_to_id };
static const LV2_Feature g_lv2_uri_map_feature =
	{ LV2_URI_MAP_URI, &g_lv2_uri_map };


#ifdef CONFIG_LV2_EVENT

// LV2 type 0 events (not supported anyway).
static uint32_t qtractor_lv2_event_ref (
	LV2_Event_Callback_Data /*data*/, LV2_Event */*event*/ )
{
	return 0;
}

static LV2_Event_Feature g_lv2_event_ref =
	{ NULL, qtractor_lv2_event_ref, qtractor_lv2_event_ref };
static const LV2_Feature g_lv2_event_ref_feature =
	{ LV2_EVENT_URI, &g_lv2_event_ref };

#endif	// CONFIG_LV2_EVENT


#ifdef CONFIG_LV2_SAVERESTORE

#include "qtractorSession.h"

#include <QByteArray>
#include <QFileInfo>
#include <QFile>
#include <QDir>

static const LV2_Feature g_lv2_saverestore_feature =
	{ LV2_SAVERESTORE_URI, NULL };

#endif	// CONFIG_LV2_SAVERESTORE


#ifdef CONFIG_LV2_PERSIST

static const LV2_Feature g_lv2_persist_feature =
	{ LV2_PERSIST_URI, NULL };

static void qtractor_lv2_persist_store ( void *callback_data,
	const char *key, const void *value, size_t size, uint32_t type )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (callback_data);
	if (pLv2Plugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_persist_store(%p, \"%s\", %d, %d)", pLv2Plugin, key, int(size), int(type));
#endif

	pLv2Plugin->lv2_persist_store(key, value, size, type);
}

static const void *qtractor_lv2_persist_retrieve ( void *callback_data,
	const char *key, size_t *size, uint32_t *type )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (callback_data);
	if (pLv2Plugin == NULL)
		return NULL;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_persist_retrieve(%p, \"%s\")", pLv2Plugin, key);
#endif

	return pLv2Plugin->lv2_persist_retrieve(key, size, type);
}

#endif	// CONFIG_LV2_PERSIST


static const LV2_Feature *g_lv2_features[] =
{
#ifdef CONFIG_LV2_EVENT
	&g_lv2_uri_map_feature,
	&g_lv2_event_ref_feature,
#endif
#ifdef CONFIG_LV2_SAVERESTORE
	&g_lv2_saverestore_feature,
#endif
#ifdef CONFIG_LV2_PERSIST
	&g_lv2_persist_feature,
#endif
	NULL
};


#ifdef CONFIG_LV2_UI

#include "qtractorPluginForm.h"

#define LV2_UI_TYPE_NONE       0
#define LV2_UI_TYPE_GTK        1
#define LV2_UI_TYPE_EXTERNAL   2

static void qtractor_lv2_ui_write (
	LV2UI_Controller ui_controller, uint32_t port_index,
	uint32_t buffer_size, uint32_t format, const void *buffer )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (ui_controller);
	if (pLv2Plugin == NULL)
		return;
	if (buffer_size != 4 || format != 0)
		return;

	float val = *(float *) buffer;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_ui_write(%p, %u, %g)", pLv2Plugin,	port_index, val);
#endif

	// FIXME: Update plugin params...
	qtractorPluginForm *pForm = pLv2Plugin->form();
	if (pForm)
		pForm->updateParamValue(port_index, val, false);
}


#ifdef CONFIG_LV2_EXTERNAL_UI

static void qtractor_lv2_ui_closed ( LV2UI_Controller ui_controller )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (ui_controller);
	if (pLv2Plugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_ui_closed(%p)", pLv2Plugin);
#endif

	// Just flag up the closure...
	pLv2Plugin->setEditorClosed(true);
}

#endif	// CONFIG_LV2_EXTERNAL_UI


#ifdef CONFIG_LV2_GTK_UI

#undef signals // Collides with GTK symbology
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define LV2_GTK_UI_URI "http://lv2plug.in/ns/extensions/ui#GtkUI"

static void qtractor_lv2_gtk_window_destroy (
	GtkWidget *pGtkWindow, gpointer pvArg )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (pvArg);
	if (pLv2Plugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_gtk_window_destroy(%p, %p)", pGtkWindow, pLv2Plugin);
#endif

	// Just flag up the closure...
	pLv2Plugin->closeEditorEx();
}

#endif	// CONFIG_LV2_GTK_UI

#endif	// CONFIG_LV2_UI


// LV2 World stuff (ref. counted).
static SLV2World   g_slv2_world   = NULL;
static SLV2Plugins g_slv2_plugins = NULL;

// Supported port classes.
static SLV2Value g_slv2_input_class      = NULL;
static SLV2Value g_slv2_output_class     = NULL;
static SLV2Value g_slv2_control_class    = NULL;
static SLV2Value g_slv2_audio_class      = NULL;
static SLV2Value g_slv2_event_class      = NULL;
static SLV2Value g_slv2_midi_class       = NULL;

#ifdef CONFIG_LV2_EXTERNAL_UI
static SLV2Value g_slv2_external_ui_class = NULL;
#endif

#ifdef CONFIG_LV2_GTK_UI
static SLV2Value g_slv2_gtk_ui_class = NULL;
#endif

// Supported plugin features.
static SLV2Value g_slv2_realtime_hint    = NULL;

#ifdef CONFIG_LV2_SAVERESTORE
static SLV2Value g_slv2_saverestore_hint = NULL;
#endif

#ifdef CONFIG_LV2_PERSIST
static SLV2Value g_slv2_persist_hint     = NULL;
#endif

// Supported port properties (hints).
static SLV2Value g_slv2_toggled_prop     = NULL;
static SLV2Value g_slv2_integer_prop     = NULL;
static SLV2Value g_slv2_sample_rate_prop = NULL;
static SLV2Value g_slv2_logarithmic_prop = NULL;


//----------------------------------------------------------------------------
// qtractorLv2PluginType -- LV2 plugin type instance.
//

// Derived methods.
bool qtractorLv2PluginType::open (void)
{
	// Do we have a descriptor already?
	if (m_slv2_plugin == NULL)
		m_slv2_plugin = slv2_plugin(m_sUri);
	if (m_slv2_plugin == NULL)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType[%p]::open() uri=\"%s\"",
		this, filename().toUtf8().constData());
#endif

	// Retrieve plugin type names.
	SLV2Value name = slv2_plugin_get_name(m_slv2_plugin);
	if (name) {
		m_sName = slv2_value_as_string(name);
		slv2_value_free(name);
	} else {
		m_sName = filename();
		int iIndex = m_sName.lastIndexOf('/');
		if (iIndex > 0)
			m_sName = m_sName.right(m_sName.length() - iIndex - 1);
	}

	// Sanitize plugin label.
	m_sLabel = m_sName.simplified().replace(QRegExp("[\\s|\\.|\\-]+"), "_");

	// Retrieve plugin unique identifier.
	m_iUniqueID = qHash(m_sUri);

	// Compute and cache port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

	unsigned long iNumPorts = slv2_plugin_get_num_ports(m_slv2_plugin);
	for (unsigned long i = 0; i < iNumPorts; ++i) {
		SLV2Port port = slv2_plugin_get_port_by_index(m_slv2_plugin, i);
		if (port) {
			if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_control_class)) {
				if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_input_class))
					m_iControlIns++;
				else
				if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_output_class))
					m_iControlOuts++;
			}
			else
			if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_audio_class)) {
				if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_input_class))
					m_iAudioIns++;
				else
				if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_output_class))
					m_iAudioOuts++;
			}
			else
			if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_event_class) ||
				slv2_port_is_a(m_slv2_plugin, port, g_slv2_midi_class)) {
				if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_input_class))
					m_iMidiIns++;
				else
				if (slv2_port_is_a(m_slv2_plugin, port, g_slv2_output_class))
					m_iMidiOuts++;
			}
		}
	}

	// Cache flags.
	m_bRealtime = slv2_plugin_has_feature(m_slv2_plugin, g_slv2_realtime_hint);

	m_bConfigure = false;
#ifdef CONFIG_LV2_SAVERESTORE
	m_bConfigure = m_bConfigure ||
		slv2_plugin_has_feature(m_slv2_plugin, g_slv2_saverestore_hint);
#endif
#ifdef CONFIG_LV2_PERSIST
	m_bConfigure = m_bConfigure ||
		slv2_plugin_has_feature(m_slv2_plugin, g_slv2_persist_hint);
#endif

#ifdef CONFIG_LV2_UI
	// Check the UI inventory...
	SLV2UIs uis = slv2_plugin_get_uis(m_slv2_plugin);
	if (uis) {
		int iNumUIs = slv2_uis_size(uis);
		for (int i = 0; i < iNumUIs; ++i) {
			SLV2UI ui = slv2_uis_get_at(uis, i);
		#ifdef CONFIG_LV2_EXTERNAL_UI
			if (slv2_ui_is_a(ui, g_slv2_external_ui_class)) {
				m_bEditor = true;
				break;
			}
		#endif
		#ifdef CONFIG_LV2_GTK_UI
			if (slv2_ui_is_a(ui, g_slv2_gtk_ui_class)) {
				m_bEditor = true;
				break;
			}
		#endif
		}
		slv2_uis_free(uis);
	}
#endif

	// Done.
	return true;
}


void qtractorLv2PluginType::close (void)
{
	m_slv2_plugin = NULL;
}


// Factory method (static)
qtractorLv2PluginType *qtractorLv2PluginType::createType (
	const QString& sUri, SLV2Plugin plugin )
{
	// Sanity check...
	if (sUri.isEmpty())
		return NULL;

	if (plugin == NULL)
		plugin = slv2_plugin(sUri);
	if (plugin == NULL)
		return NULL;

	// Yep, most probably its a valid plugin descriptor...
	return new qtractorLv2PluginType(sUri, plugin);
}


// Descriptor method (static)
SLV2Plugin qtractorLv2PluginType::slv2_plugin ( const QString& sUri )
{
	if (g_slv2_plugins == NULL)
		return NULL;

	// Retrieve plugin descriptor if any...
	SLV2Value uri = slv2_value_new_uri(g_slv2_world, sUri.toUtf8().constData());
	if (uri == NULL)
		return NULL;

	SLV2Plugin plugin = slv2_plugins_get_by_uri(g_slv2_plugins, uri);

	slv2_value_free(uri);
	return plugin;
}


// LV2 World stuff (ref. counted).
void qtractorLv2PluginType::slv2_open (void)
{
	if (g_slv2_plugins)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType::slv2_open()");
#endif

	// Find all installed plugins.
	g_slv2_world = slv2_world_new();
	slv2_world_load_all(g_slv2_world);
	g_slv2_plugins = slv2_world_get_all_plugins(g_slv2_world);

	// Set up the port classes we support.
	g_slv2_input_class   = slv2_value_new_uri(g_slv2_world, SLV2_PORT_CLASS_INPUT);
	g_slv2_output_class  = slv2_value_new_uri(g_slv2_world, SLV2_PORT_CLASS_OUTPUT);
	g_slv2_control_class = slv2_value_new_uri(g_slv2_world, SLV2_PORT_CLASS_CONTROL);
	g_slv2_audio_class   = slv2_value_new_uri(g_slv2_world, SLV2_PORT_CLASS_AUDIO);
	g_slv2_event_class   = slv2_value_new_uri(g_slv2_world, SLV2_PORT_CLASS_EVENT);
	g_slv2_midi_class    = slv2_value_new_uri(g_slv2_world, SLV2_EVENT_CLASS_MIDI);

#ifdef CONFIG_LV2_EXTERNAL_UI
	g_slv2_external_ui_class = slv2_value_new_uri(g_slv2_world,	LV2_EXTERNAL_UI_URI);
#endif

#ifdef CONFIG_LV2_GTK_UI
	g_slv2_gtk_ui_class = slv2_value_new_uri(g_slv2_world, LV2_GTK_UI_URI);
#endif

	// Set up the feature we may want to know (as hints).
	g_slv2_realtime_hint = slv2_value_new_uri(g_slv2_world,
		SLV2_NAMESPACE_LV2 "hardRtCapable");

#ifdef CONFIG_LV2_SAVERESTORE
	g_slv2_saverestore_hint = slv2_value_new_uri(g_slv2_world,
		LV2_SAVERESTORE_URI);
#endif
#ifdef CONFIG_LV2_PERSIST
	g_slv2_persist_hint = slv2_value_new_uri(g_slv2_world,
		LV2_PERSIST_URI);
#endif

	// Set up the port properties we support (as hints).
	g_slv2_toggled_prop = slv2_value_new_uri(g_slv2_world,
		SLV2_NAMESPACE_LV2 "toggled");
	g_slv2_integer_prop = slv2_value_new_uri(g_slv2_world,
		SLV2_NAMESPACE_LV2 "integer");
	g_slv2_sample_rate_prop = slv2_value_new_uri(g_slv2_world,
		SLV2_NAMESPACE_LV2 "sampleRate");
	g_slv2_logarithmic_prop = slv2_value_new_uri(g_slv2_world,
		"http://lv2plug.in/ns/dev/extportinfo#logarithmic");
}


void qtractorLv2PluginType::slv2_close (void)
{
	if (g_slv2_plugins == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType::slv2_close()");
#endif

	// Clean up.
	slv2_value_free(g_slv2_toggled_prop);
	slv2_value_free(g_slv2_integer_prop);
	slv2_value_free(g_slv2_sample_rate_prop);
	slv2_value_free(g_slv2_logarithmic_prop);

#ifdef CONFIG_LV2_PERSIST
	slv2_value_free(g_slv2_persist_hint);
#endif
#ifdef CONFIG_LV2_SAVERESTORE
	slv2_value_free(g_slv2_saverestore_hint);
#endif

	slv2_value_free(g_slv2_realtime_hint);

	slv2_value_free(g_slv2_input_class);
	slv2_value_free(g_slv2_output_class);
	slv2_value_free(g_slv2_control_class);
	slv2_value_free(g_slv2_audio_class);
	slv2_value_free(g_slv2_event_class);
	slv2_value_free(g_slv2_midi_class);

#ifdef CONFIG_LV2_EXTERNAL_UI
	slv2_value_free(g_slv2_external_ui_class);
#endif

#ifdef CONFIG_LV2_GTK_UI
	slv2_value_free(g_slv2_gtk_ui_class);
#endif

	slv2_plugins_free(g_slv2_world, g_slv2_plugins);
	slv2_world_free(g_slv2_world);

	g_slv2_input_class   = NULL;
	g_slv2_output_class  = NULL;
	g_slv2_control_class = NULL;
	g_slv2_audio_class   = NULL;
	g_slv2_event_class   = NULL;
	g_slv2_midi_class    = NULL;

#ifdef CONFIG_LV2_EXTERNAL_UI
	g_slv2_external_ui_class = NULL;
#endif

#ifdef CONFIG_LV2_GTK_UI
	g_slv2_gtk_ui_class = NULL;
#endif

	g_slv2_plugins = NULL;
	g_slv2_world   = NULL;
}


// Plugin type listing (static).
bool qtractorLv2PluginType::getTypes ( qtractorPluginPath& path )
{
	if (g_slv2_plugins == NULL)
		return false;

	unsigned long iIndex = 0;

	unsigned int iNumPlugins = slv2_plugins_size(g_slv2_plugins);
	for (unsigned int i = 0; i < iNumPlugins; ++i) {
		SLV2Plugin  plugin = slv2_plugins_get_at(g_slv2_plugins, i);
		const char *pszUri = slv2_value_as_uri(slv2_plugin_get_uri(plugin));
		qtractorLv2PluginType *pLv2Type
			= qtractorLv2PluginType::createType(pszUri, plugin);
		if (pLv2Type) {
			if (pLv2Type->open()) {
				path.addType(pLv2Type);
				pLv2Type->close();
				iIndex++;
			} else {
				delete pLv2Type;
			}
		}
	}

	return (iIndex > 0);
}


//----------------------------------------------------------------------------
// qtractorLv2Plugin -- LV2 plugin instance.
//

// Dynamic singleton list of LV2 plugins with external UIs open.
static QList<qtractorLv2Plugin *> g_lv2Plugins;

// Constructors.
qtractorLv2Plugin::qtractorLv2Plugin ( qtractorPluginList *pList,
	qtractorLv2PluginType *pLv2Type )
	: qtractorPlugin(pList, pLv2Type)
		, m_pInstances(NULL)
		, m_piControlOuts(NULL)
		, m_pfControlOuts(NULL)
		, m_pfControlOutsLast(NULL)
		, m_piAudioIns(NULL)
		, m_piAudioOuts(NULL)
	#ifdef CONFIG_LV2_EVENT
		, m_piMidiIns(NULL)
	#endif
	#ifdef CONFIG_LV2_UI
		, m_lv2_ui_type(LV2_UI_TYPE_NONE)
		, m_bEditorVisible(false)
		, m_bEditorClosed(false)
		, m_slv2_uis(NULL)
		, m_slv2_ui(NULL)
		, m_slv2_ui_instance(NULL)
		, m_lv2_ui_features(NULL)
		, m_lv2_ui_widget(NULL)
	#ifdef CONFIG_LV2_GTK_UI
		, m_pGtkWindow(NULL)
	#endif
	#endif
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p] uri=\"%s\"",
		this, pLv2Type->filename().toUtf8().constData());
#endif

	// Get some structural data first...
	SLV2Plugin plugin = pLv2Type->slv2_plugin();
	if (plugin) {
		unsigned short iControlOuts = pLv2Type->controlOuts();
		unsigned short iAudioIns    = pLv2Type->audioIns();
		unsigned short iAudioOuts   = pLv2Type->audioOuts();
		if (iAudioIns > 0)
			m_piAudioIns = new unsigned long [iAudioIns];
		if (iAudioOuts > 0)
			m_piAudioOuts = new unsigned long [iAudioOuts];
		if (iControlOuts > 0) {
			m_piControlOuts = new unsigned long [iControlOuts];
			m_pfControlOuts = new float [iControlOuts];
			m_pfControlOutsLast = new float [iControlOuts];
		}
		iControlOuts = iAudioIns = iAudioOuts = 0;
	#ifdef CONFIG_LV2_EVENT
		unsigned short iMidiIns = pLv2Type->midiIns();
		if (iMidiIns > 0)
			m_piMidiIns = new unsigned long [iMidiIns];
		iMidiIns = 0;
	#endif
		unsigned long iNumPorts = slv2_plugin_get_num_ports(plugin);
		for (unsigned long i = 0; i < iNumPorts; ++i) {
			SLV2Port port = slv2_plugin_get_port_by_index(plugin, i);
			if (port) {
				if (slv2_port_is_a(plugin, port, g_slv2_input_class)) {
					if (slv2_port_is_a(plugin, port, g_slv2_audio_class))
						m_piAudioIns[iAudioIns++] = i;
					else
				#ifdef CONFIG_LV2_EVENT
					if (slv2_port_is_a(plugin, port, g_slv2_event_class) ||
						slv2_port_is_a(plugin, port, g_slv2_midi_class))
						m_piMidiIns[iMidiIns++] = i;
					else
				#endif
					if (slv2_port_is_a(plugin, port, g_slv2_control_class))
						addParam(new qtractorLv2PluginParam(this, i));
				}
				else
				if (slv2_port_is_a(plugin, port, g_slv2_output_class)) {
					if (slv2_port_is_a(plugin, port, g_slv2_audio_class))
						m_piAudioOuts[iAudioOuts++] = i;
					else
					if (slv2_port_is_a(plugin, port, g_slv2_control_class)) {
						m_piControlOuts[iControlOuts] = i;
						m_pfControlOuts[iControlOuts] = 0.0f;
						m_pfControlOutsLast[iControlOuts] = 0.0f;
						++iControlOuts;
					}
				}
			}
		}
		// FIXME: instantiate each instance properly...
		qtractorLv2Plugin::setChannels(channels());
	}
}


// Destructor.
qtractorLv2Plugin::~qtractorLv2Plugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);

	// Free up all the rest...
#ifdef CONFIG_LV2_EVENT
	if (m_piMidiIns)
		delete [] m_piMidiIns;
#endif
	if (m_piAudioOuts)
		delete [] m_piAudioOuts;
	if (m_piAudioIns)
		delete [] m_piAudioIns;
	if (m_piControlOuts)
		delete [] m_piControlOuts;
	if (m_pfControlOuts)
		delete [] m_pfControlOuts;
	if (m_pfControlOutsLast)
		delete [] m_pfControlOutsLast;
}


// Channel/instance number accessors.
void qtractorLv2Plugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorPluginType *pType = type();
	if (pType == NULL)
		return;

	// Estimate the (new) number of instances...
	unsigned short iInstances
		= pType->instances(iChannels, pType->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == instances() && !pType->isMidi())
		return;

	SLV2Plugin plugin = slv2_plugin();
	if (plugin == NULL)
		return;

	// Gotta go for a while...
	bool bActivated = isActivated();
	setActivated(false);

	if (m_pInstances) {
		for (unsigned short i = 0; i < instances(); ++i) {
			SLV2Instance instance = m_pInstances[i];
			if (instance)
				slv2_instance_free(instance);
		}
		delete [] m_pInstances;
		m_pInstances = NULL;
	}

	// Set new instance number...
	setInstances(iInstances);
	if (iInstances < 1) {
		setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

#ifdef CONFIG_LV2_EVENT
	qtractorMidiManager *pMidiManager = NULL;
	unsigned short iMidiIns = pType->midiIns();
	if (iMidiIns > 0)
		pMidiManager = list()->midiManager();
#endif

	// We'll need output control (not dummy anymore) port indexes...
	unsigned short iControlOuts = pType->controlOuts();
	// Allocate new instances...
	m_pInstances = new SLV2Instance [iInstances];
	for (unsigned short i = 0; i < iInstances; ++i) {
		// Instantiate them properly first...
		SLV2Instance instance
			= slv2_plugin_instantiate(plugin, sampleRate(), g_lv2_features);
		if (instance) {
			// (Dis)connect all ports...
			unsigned long iNumPorts = slv2_plugin_get_num_ports(plugin);
			for (unsigned long k = 0; k < iNumPorts; ++k)
				slv2_instance_connect_port(instance, k, NULL);
			// Connect all existing input control ports...
			const qtractorPlugin::Params& params = qtractorPlugin::params();
			qtractorPlugin::Params::ConstIterator param = params.constBegin();
			for ( ; param != params.constEnd(); ++param) {
				qtractorPluginParam *pParam = param.value();
				slv2_instance_connect_port(instance,
					pParam->index(), pParam->subject()->data());
			}
		#ifdef CONFIG_LV2_EVENT
			// Connect all existing input MIDI ports...
			if (pMidiManager) {
				for (unsigned short j = 0; j < iMidiIns; ++j) {
					slv2_instance_connect_port(instance,
						m_piMidiIns[j], pMidiManager->lv2_buffer());
				}
			}
		#endif
			// Connect all existing output control ports...
			for (unsigned short j = 0; j < iControlOuts; ++j) {
				slv2_instance_connect_port(instance,
					m_piControlOuts[j], &m_pfControlOuts[j]);
			}
		}
		// This is it...
		m_pInstances[i] = instance;
	}

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Specific accessors.
SLV2Plugin qtractorLv2Plugin::slv2_plugin (void) const
{
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	return (pLv2Type ? pLv2Type->slv2_plugin() : NULL);
}


SLV2Instance qtractorLv2Plugin::slv2_instance ( unsigned short iInstance ) const
{
	return (m_pInstances ? m_pInstances[iInstance] : NULL);
}


LV2_Handle qtractorLv2Plugin::lv2_handle ( unsigned short iInstance ) const
{
	SLV2Instance instance = slv2_instance(iInstance);
	return (instance ? slv2_instance_get_handle(instance) : NULL);
}


// Do the actual activation.
void qtractorLv2Plugin::activate (void)
{
	if (m_pInstances) {
		for (unsigned short i = 0; i < instances(); ++i) {
			SLV2Instance instance = m_pInstances[i];
			if (instance)
				slv2_instance_activate(instance);
		}
	}
}


// Do the actual deactivation.
void qtractorLv2Plugin::deactivate (void)
{
	if (m_pInstances) {
		for (unsigned short i = 0; i < instances(); ++i) {
			SLV2Instance instance = m_pInstances[i];
			if (instance)
				slv2_instance_deactivate(instance);
		}
	}
}


// The main plugin processing procedure.
void qtractorLv2Plugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_pInstances == NULL)
		return;

	SLV2Plugin plugin = slv2_plugin();
	if (plugin == NULL)
		return;

	// We'll cross channels over instances...
	unsigned short iInstances = instances();
	unsigned short iChannels  = channels();
	unsigned short iAudioIns  = audioIns();
	unsigned short iAudioOuts = audioOuts();
	unsigned short iIChannel  = 0;
	unsigned short iOChannel  = 0;
	unsigned short i, j;

	// For each plugin instance...
	for (i = 0; i < iInstances; ++i) {
		SLV2Instance instance = m_pInstances[i];
		if (instance) {
			// For each instance audio input port...
			for (j = 0; j < iAudioIns; ++j) {
				slv2_instance_connect_port(instance,
					m_piAudioIns[j], ppIBuffer[iIChannel]);
				if (++iIChannel >= iChannels)
					iIChannel = 0;
			}
			// For each instance audio output port...
			for (j = 0; j < iAudioOuts; ++j) {
				slv2_instance_connect_port(instance,
					m_piAudioOuts[j], ppOBuffer[iOChannel]);
				if (++iOChannel >= iChannels)
					iOChannel = 0;
			}
			// Make it run...
			slv2_instance_run(instance, nframes);
			// Wrap channels?...
			if (iIChannel < iChannels - 1)
				iIChannel++;
			if (iOChannel < iChannels - 1)
				iOChannel++;
		}
	}
}


#ifdef CONFIG_LV2_UI

// Open editor.
void qtractorLv2Plugin::openEditor ( QWidget * /*pParent*/ )
{
	if (m_lv2_ui_widget) {
		setEditorVisible(true);
		return;
	}

	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::openEditor()", this);
#endif

	// Check the UI inventory...
	m_slv2_uis = slv2_plugin_get_uis(pLv2Type->slv2_plugin());
	if (m_slv2_uis == NULL)
		return;

	int iNumUIs = slv2_uis_size(m_slv2_uis);
	for (int i = 0; i < iNumUIs; ++i) {
		SLV2UI ui = slv2_uis_get_at(m_slv2_uis, i);
	#ifdef CONFIG_LV2_EXTERNAL_UI
		if (slv2_ui_is_a(ui, g_slv2_external_ui_class)) {
			m_lv2_ui_type = LV2_UI_TYPE_EXTERNAL;
			m_slv2_ui = ui;
			break;
		}
	#endif
	#ifdef CONFIG_LV2_GTK_UI
		if (slv2_ui_is_a(ui, g_slv2_gtk_ui_class)) {
			m_lv2_ui_type = LV2_UI_TYPE_GTK;
			m_slv2_ui = ui;
			break;
		}
	#endif
	}

	if (m_slv2_ui == NULL)
		return;

	SLV2Instance instance = slv2_instance(0);
	if (instance == NULL)
		return;
	
	const LV2_Descriptor *descriptor = slv2_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return;

	m_aEditorTitle = editorTitle().toUtf8();

	int iFeatures = 0;
	while (g_lv2_features[iFeatures]) { ++iFeatures; }

	m_lv2_ui_features = new LV2_Feature * [iFeatures + 4];
	for (int i = 0; i < iFeatures; ++i)
		m_lv2_ui_features[i] = (LV2_Feature *) g_lv2_features[i];

	m_lv2_data_access.data_access = descriptor->extension_data;
	m_lv2_data_access_feature.URI = LV2_DATA_ACCESS_URI;
	m_lv2_data_access_feature.data = &m_lv2_data_access;
	m_lv2_ui_features[iFeatures++] = &m_lv2_data_access_feature;

	m_lv2_instance_access_feature.URI = LV2_INSTANCE_ACCESS_URI;
	m_lv2_instance_access_feature.data = slv2_instance_get_handle(instance);
	m_lv2_ui_features[iFeatures++] = &m_lv2_instance_access_feature;

#ifdef CONFIG_LV2_EXTERNAL_UI
	m_lv2_ui_external.ui_closed = qtractor_lv2_ui_closed;
	m_lv2_ui_external.plugin_human_id = m_aEditorTitle.constData();
	m_lv2_ui_feature.URI = LV2_EXTERNAL_UI_URI;
	m_lv2_ui_feature.data = &m_lv2_ui_external;
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_feature;
#endif

	m_lv2_ui_features[iFeatures] = NULL;

	m_slv2_ui_instance = slv2_ui_instantiate(pLv2Type->slv2_plugin(),
		m_slv2_ui, qtractor_lv2_ui_write, this, m_lv2_ui_features);

	const LV2UI_Descriptor *ui_descriptor = lv2_ui_descriptor();
	if (ui_descriptor && ui_descriptor->port_event) {
		LV2UI_Handle ui_handle = lv2_ui_handle();
		if (ui_handle) {
			const qtractorPlugin::Params& params = qtractorPlugin::params();
			qtractorPlugin::Params::ConstIterator param = params.constBegin();
			for ( ; param != params.constEnd(); ++param) {
				qtractorPluginParam *pParam = param.value();
				float fValue = pParam->value();
				(*ui_descriptor->port_event)(ui_handle,
					pParam->index(), 4, 0, &fValue);
			}
			unsigned long iControlOuts = pLv2Type->controlOuts();
			for (unsigned long j = 0; j < iControlOuts; ++j) {
				(*ui_descriptor->port_event)(ui_handle,
					m_piControlOuts[j], 4, 0, &m_pfControlOuts[j]);
			}
		}
	}

	if (m_slv2_ui_instance) {
		m_lv2_ui_widget = slv2_ui_instance_get_widget(m_slv2_ui_instance);
	#ifdef CONFIG_LV2_GTK_UI
		if (m_lv2_ui_type == LV2_UI_TYPE_GTK) {
			// Create embeddable native window...
			m_pGtkWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
			gtk_window_set_resizable(GTK_WINDOW(m_pGtkWindow), 1);
			gtk_window_set_title(
				GTK_WINDOW(m_pGtkWindow),
				m_aEditorTitle.constData());
			// Add plugin widget into our new window container... 
			gtk_container_add(
				GTK_CONTAINER(m_pGtkWindow),
				static_cast<GtkWidget *> (m_lv2_ui_widget));
			g_signal_connect(
				G_OBJECT(m_pGtkWindow), "destroy",
				G_CALLBACK(qtractor_lv2_gtk_window_destroy), this);
		//	gtk_widget_show_all(m_pGtkWindow);
		}
	#endif
		g_lv2Plugins.append(this);
	}
	
	setEditorVisible(true);

//	idleEditor();
}


// Close editor.
void qtractorLv2Plugin::closeEditor (void)
{
	if (m_lv2_ui_widget == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

#ifdef CONFIG_LV2_GTK_UI
	if (m_lv2_ui_type == LV2_UI_TYPE_GTK) {
		const LV2UI_Descriptor *ui_descriptor = lv2_ui_descriptor();
		if (ui_descriptor && ui_descriptor->cleanup) {
			LV2UI_Handle ui_handle = lv2_ui_handle();
			if (ui_handle)
				(*ui_descriptor->cleanup)(ui_handle);
		}
		if (m_pGtkWindow) {
			GtkWidget *pGtkWindow = m_pGtkWindow;
			m_pGtkWindow = NULL;
			gtk_widget_destroy(pGtkWindow);
		}
	}
#endif

	m_lv2_ui_type = LV2_UI_TYPE_NONE;
	
	int iLv2Plugin = g_lv2Plugins.indexOf(this);
	if (iLv2Plugin >= 0)
		g_lv2Plugins.removeAt(iLv2Plugin);

	if (m_slv2_ui_instance) {
		slv2_ui_instance_free(m_slv2_ui_instance);
		m_slv2_ui_instance = NULL;
		m_lv2_ui_widget = NULL;
	}

	if (m_lv2_ui_features) {
		delete [] m_lv2_ui_features;
		m_lv2_ui_features = NULL;
	}

	if (m_slv2_uis) {
		slv2_uis_free(m_slv2_uis);
		m_slv2_uis = NULL;
		m_slv2_ui = NULL;
	}
}


// Idle editor.
void qtractorLv2Plugin::idleEditor (void)
{
	if (m_lv2_ui_widget == NULL)
		return;

	if (m_piControlOuts && m_pfControlOuts && m_pfControlOutsLast) {
		const LV2UI_Descriptor *ui_descriptor = lv2_ui_descriptor();
		if (ui_descriptor && ui_descriptor->port_event) {
			LV2UI_Handle ui_handle = lv2_ui_handle();
			if (ui_handle) {
				unsigned long iControlOuts = type()->controlOuts();
				for (unsigned short j = 0; j < iControlOuts; ++j) {
					if (m_pfControlOutsLast[j] != m_pfControlOuts[j]) {
						(*ui_descriptor->port_event)(ui_handle,
							m_piControlOuts[j], 4, 0, &m_pfControlOuts[j]);
						m_pfControlOutsLast[j] = m_pfControlOuts[j];
					}
				}
			}
		}
	}

#ifdef CONFIG_LV2_EXTERNAL_UI
	if (m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
		LV2_EXTERNAL_UI_RUN((lv2_external_ui *) m_lv2_ui_widget);
#endif

	// Do we need some clean-up...?
    if (isEditorClosed()) {
		setEditorClosed(false);
		if (isFormVisible())
			form()->toggleEditor(false);
		m_bEditorVisible = false;
		// Do really close now.
		closeEditor();
	}
}


#ifdef CONFIG_LV2_GTK_UI

void qtractorLv2Plugin::closeEditorEx (void)
{
	if (m_pGtkWindow) {
		m_pGtkWindow = NULL;	
		setEditorClosed(true);
	}
}

#endif


// GUI editor visibility state.
void qtractorLv2Plugin::setEditorVisible ( bool bVisible )
{
	if (m_lv2_ui_widget == NULL)
		return;

	if (!m_bEditorVisible && bVisible) {
		switch (m_lv2_ui_type) {
		case LV2_UI_TYPE_EXTERNAL:
		#ifdef CONFIG_LV2_EXTERNAL_UI
			LV2_EXTERNAL_UI_SHOW((lv2_external_ui *) m_lv2_ui_widget);
		#endif
			break;
		case LV2_UI_TYPE_GTK:
		#ifdef CONFIG_LV2_GTK_UI
			if (m_pGtkWindow) gtk_widget_show_all(m_pGtkWindow);
		#endif
			break;
		}
		m_bEditorVisible = true;
	}
	else
	if (m_bEditorVisible && !bVisible) {
		switch (m_lv2_ui_type) {
		case LV2_UI_TYPE_EXTERNAL:
		#ifdef CONFIG_LV2_EXTERNAL_UI
			LV2_EXTERNAL_UI_HIDE((lv2_external_ui *) m_lv2_ui_widget);
		#endif
			break;
		case LV2_UI_TYPE_GTK:
		#ifdef CONFIG_LV2_GTK_UI
			if (m_pGtkWindow) gtk_widget_hide_all(m_pGtkWindow);
		#endif
			break;
		}
		m_bEditorVisible = false;
	}
}

bool qtractorLv2Plugin::isEditorVisible (void) const
{
	return m_bEditorVisible;
}


// Update editor widget caption.
void qtractorLv2Plugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	if (m_lv2_ui_features) {
		m_aEditorTitle = sTitle.toUtf8();
	#ifdef CONFIG_LV2_EXTERNAL_UI
		m_lv2_ui_external.plugin_human_id = m_aEditorTitle.constData();
	#endif
	#ifdef CONFIG_LV2_GTK_UI
		if (m_pGtkWindow) {
			gtk_window_set_title(
				GTK_WINDOW(m_pGtkWindow),
				m_aEditorTitle.constData());
		}
	#endif
	}
}


// Parameter update method.
void qtractorLv2Plugin::updateParam (
	qtractorPluginParam *pParam, float fValue )
{
	const LV2UI_Descriptor *ui_descriptor = lv2_ui_descriptor();
	if (ui_descriptor == NULL)
		return;
	if (ui_descriptor->port_event == NULL)
		return;

	LV2UI_Handle ui_handle = lv2_ui_handle();
	if (ui_handle == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::updateParam(%lu, %g)",
		this, pParam->index(), fValue); 
#endif

	(*ui_descriptor->port_event)(ui_handle, pParam->index(), 4, 0, &fValue);
}


// Idle editor (static).
void qtractorLv2Plugin::idleEditorAll (void)
{
	QListIterator<qtractorLv2Plugin *> iter(g_lv2Plugins);
	while (iter.hasNext())
		iter.next()->idleEditor();
}


// LV2 UI descriptor accessor.
const LV2UI_Descriptor *qtractorLv2Plugin::lv2_ui_descriptor (void) const
{
	if (m_slv2_ui_instance == NULL)
		return NULL;
		
	return slv2_ui_instance_get_descriptor(m_slv2_ui_instance);
}


// LV2 UI handle accessor.
LV2UI_Handle qtractorLv2Plugin::lv2_ui_handle (void) const
{
	if (m_slv2_ui_instance == NULL)
		return NULL;
		
	return slv2_ui_instance_get_handle(m_slv2_ui_instance);
}

#endif	// CONFIG_LV2_UI


// Configuration (restore) stuff.
void qtractorLv2Plugin::configure ( const QString& sKey, const QString& sValue )
{
#ifdef CONFIG_LV2_SAVERESTORE

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QDir dir(pSession->sessionDir());

	if (sKey.section(':', 0, 0) == "saverestore") {
		QFileInfo fi(dir, sValue);
		if (fi.exists() && fi.isReadable()) {
			const QByteArray aName = sKey.section(':', 1).toUtf8();
			const QByteArray aPath = fi.absoluteFilePath().toUtf8();
			LV2SR_File sr_file;
			sr_file.name = (char *) aName.constData();
			sr_file.path = (char *) aPath.constData();
			sr_file.must_copy = 0;
		#ifdef CONFIG_DEBUG
			qDebug("qtractorLv2Plugin[%p]::configure() name=\"%s\" path=\"%s\"",
				this, sr_file.name, sr_file.path);
		#endif
			const LV2SR_File *ppFiles[2];
			ppFiles[0] = &sr_file;
			ppFiles[1] = NULL;
			for (unsigned short i = 0; i < instances(); ++i)
				lv2_sr_restore(i, ppFiles);
		}
	}

#endif	// CONFIG_LV2_SAVERESTORE
}


// Plugin configuration/state (save) snapshot.
void qtractorLv2Plugin::freezeConfigs (void)
{
	if (!type()->isConfigure())
		return;
	
#ifdef CONFIG_LV2_PERSIST

	for (unsigned short i = 0; i < instances(); ++i) {
		const LV2_Persist *persist = lv2_persist_descriptor(i);
		if (persist) {
			LV2_Handle handle = lv2_handle(i);
			if (handle)
				(*persist->save)(handle, qtractor_lv2_persist_store, this);
		}
	}

#endif	// CONFIG_LV2_PERSIST

#ifdef CONFIG_LV2_SAVERESTORE
	
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QDir dir(pSession->sessionDir());
	
	LV2SR_File **ppFiles = NULL;
	if (!lv2_sr_save(0, dir.absolutePath().toUtf8().constData(), &ppFiles))
		return;
	if (ppFiles == NULL)
		return;

	for (int i = 0; ppFiles[i]; ++i) {
		LV2SR_File *pFile = ppFiles[i];
	#ifdef CONFIG_DEBUG
		qDebug("qtractorLv2Plugin[%p]::freezeConfigs(%d) name=\"%s\" path=\"%s\"",
			this, i, pFile->name, pFile->path);
	#endif
		const QString sName = pFile->name;
		const QString sPath = pFile->path;
		QFile file(sPath);
		if (file.exists()) {
			// Always copy/rename the damn file...
			QString sNewFile;
			const QString& sPreset = preset();
			if (sPreset.isEmpty()) {
				const QString& sSessionName = pSession->sessionName();
				if (!sSessionName.isEmpty()) {
					sNewFile += qtractorSession::sanitize(sSessionName);
					sNewFile += '-';
				}
				const QString& sListName = list()->name();
				if (!sListName.isEmpty()) {
					sNewFile += qtractorSession::sanitize(sListName);
					sNewFile += '-';
				}
			}
			sNewFile += type()->label();
			sNewFile += '-';
			if (sPreset.isEmpty()) {
				sNewFile += QString::number(type()->uniqueID(), 16);
			} else {
				sNewFile += qtractorSession::sanitize(sPreset);
			}
			const QFileInfo fi(dir, sNewFile + "-lv2.sav");
			const QString sNewPath = fi.absoluteFilePath();
			if (fi.exists()) QFile::remove(sNewPath);
			if (pFile->must_copy) {
				file.copy(sNewPath);
			} else {
				file.rename(sNewPath);
			}
			setConfig("saverestore:" + sName, fi.fileName());
		}
		::free(pFile->path);
		::free(pFile->name);
		::free(pFile);
	}
	::free(ppFiles);

#endif	// CONFIG_LV2_SAVERESTORE
}

// Plugin configuration/state (load) realization.
void qtractorLv2Plugin::realizeConfigs (void)
{
#ifdef CONFIG_LV2_PERSIST

	m_lv2_persist_configs.clear();

	Configs::ConstIterator config = configs().constBegin();
	for (; config != configs().constEnd(); ++config)
		m_lv2_persist_configs.insert(config.key(), config.value().toUtf8());

	for (unsigned short i = 0; i < instances(); ++i) {
		const LV2_Persist *persist = lv2_persist_descriptor(i);
		if (persist) {
			LV2_Handle handle = lv2_handle(i);
			if (handle)
				(*persist->restore)(handle, qtractor_lv2_persist_retrieve, this);
		}
	}

#endif	// CONFIG_LV2_PERSIST

	qtractorPlugin::realizeConfigs();
}


// Plugin configuration/state release.
void qtractorLv2Plugin::releaseConfigs (void)
{
#ifdef CONFIG_LV2_PERSIST
	m_lv2_persist_configs.clear();
#endif

	qtractorPlugin::releaseConfigs();
}


#ifdef CONFIG_LV2_SAVERESTORE

// LV2 Save/Restore extension data descriptor accessor.
const LV2SR_Descriptor *qtractorLv2Plugin::lv2_sr_descriptor (
	unsigned short iInstance ) const
{
	SLV2Instance instance = slv2_instance(iInstance);
	if (instance == NULL)
		return NULL;

	const LV2_Descriptor *descriptor = slv2_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return NULL;
	if (descriptor->extension_data == NULL)
		return NULL;

	return (const LV2SR_Descriptor *)
		(*descriptor->extension_data)(LV2_SAVERESTORE_URI);
}


bool qtractorLv2Plugin::lv2_sr_save ( unsigned short iInstance,
	const char *pszDirectory, LV2SR_File ***pppFiles )
{
	const LV2SR_Descriptor *sr_descriptor = lv2_sr_descriptor(iInstance);
	if (sr_descriptor == NULL)
		return false;
	if (sr_descriptor->save == NULL)
		return false;

	LV2_Handle handle = lv2_handle(iInstance);
	if (handle == NULL)
		return false;

	char *pszError = (*sr_descriptor->save)(handle, pszDirectory, pppFiles);
	if (pszError) {
		qDebug("lv2_sr_save: Failed to save plugin state: %s", pszError);
		::free(pszError);
		return false;
	}

	return true;
}


bool qtractorLv2Plugin::lv2_sr_restore ( unsigned short iInstance,
	const LV2SR_File **ppFiles )
{
	const LV2SR_Descriptor *sr_descriptor = lv2_sr_descriptor(iInstance);
	if (sr_descriptor == NULL)
		return false;
	if (sr_descriptor->restore == NULL)
		return false;

	LV2_Handle handle = lv2_handle(iInstance);
	if (handle == NULL)
		return false;

	char *pszError = (*sr_descriptor->restore)(handle, ppFiles);
	if (pszError) {
		qDebug("lv2_sr_restore: Failed to restore plugin state: %s", pszError);
		::free(pszError);
		return false;
	}

	return true;
}

#endif	// CONFIG_LV2_SAVERESTORE


#ifdef CONFIG_LV2_PERSIST

// LV2 Persist extension data descriptor accessor.
const LV2_Persist *qtractorLv2Plugin::lv2_persist_descriptor (
	unsigned short iInstance ) const
{
	SLV2Instance instance = slv2_instance(iInstance);
	if (instance == NULL)
		return NULL;

	const LV2_Descriptor *descriptor = slv2_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return NULL;
	if (descriptor->extension_data == NULL)
		return NULL;

	return (const LV2_Persist *)
		(*descriptor->extension_data)(LV2_PERSIST_URI);
}


void qtractorLv2Plugin::lv2_persist_store (
	const char *key, const void *value, size_t size, uint32_t type )
{
	if (value && type == QTRACTOR_LV2_ATOM_STRING_ID)
		setConfig(key, QString::fromUtf8((const char *) value, (int) size - 1));
}


const void *qtractorLv2Plugin::lv2_persist_retrieve (
	const char *key, size_t *size, uint32_t *type )
{
	const QByteArray& data = m_lv2_persist_configs[key];

	if (size)
		*size = data.size();
	if (type)
		*type = QTRACTOR_LV2_ATOM_STRING_ID;

	return data.constData();
}


#endif	// CONFIG_LV2_PERSIST


//----------------------------------------------------------------------------
// qtractorLv2PluginParam -- LV2 plugin control input port instance.
//

// Constructors.
qtractorLv2PluginParam::qtractorLv2PluginParam (
	qtractorLv2Plugin *pLv2Plugin, unsigned long iIndex )
	: qtractorPluginParam(pLv2Plugin, iIndex), m_iPortHints(None)
{
	SLV2Plugin plugin = pLv2Plugin->slv2_plugin();
	SLV2Port port = slv2_plugin_get_port_by_index(plugin, iIndex);

	// Set nominal parameter name...
	SLV2Value name = slv2_port_get_name(plugin, port);
	setName(slv2_value_as_string(name));
	slv2_value_free(name);

	// Get port properties and set hints...
	if (slv2_port_has_property(plugin, port, g_slv2_toggled_prop))
		m_iPortHints |= Toggled;
	if (slv2_port_has_property(plugin, port, g_slv2_integer_prop))
		m_iPortHints |= Integer;
	if (slv2_port_has_property(plugin, port, g_slv2_sample_rate_prop))
		m_iPortHints |= SampleRate;
	if (slv2_port_has_property(plugin, port, g_slv2_logarithmic_prop))
		m_iPortHints |= Logarithmic;

	// Initialize range values...
	SLV2Value vdef, vmin, vmax;
	slv2_port_get_range(plugin, port, &vdef, &vmin, &vmax);

	setMinValue(vmin ? slv2_value_as_float(vmin) : 0.0f);
	setMaxValue(vmax ? slv2_value_as_float(vmax) : 1.0f);

	setDefaultValue(vdef ? slv2_value_as_float(vdef) : 0.0f);

	if (vdef) slv2_value_free(vdef);
	if (vmin) slv2_value_free(vmin);
	if (vmax) slv2_value_free(vmax);

	// Have scale points (display values)
	// m_display.clear();
	SLV2ScalePoints points = slv2_port_get_scale_points(plugin, port);
	if (points) {
		unsigned short iNumPoints = slv2_scale_points_size(points);
		for (unsigned short i = 0; i < iNumPoints; ++i) {
			SLV2ScalePoint point = slv2_scale_points_get_at(points, i);
			SLV2Value value = slv2_scale_point_get_value(point);
			SLV2Value label = slv2_scale_point_get_label(point);
			if (value && label) {
				float   fValue = slv2_value_as_float(value);
				QString sLabel = slv2_value_as_string(label);
				m_display.insert(QString::number(fValue), sLabel);
			}
		}
		slv2_scale_points_free(points);
	}

	// Initialize port value...
	reset();
}


// Destructor.
qtractorLv2PluginParam::~qtractorLv2PluginParam (void)
{
}


// Port range hints predicate methods.
bool qtractorLv2PluginParam::isBoundedBelow (void) const
{
	return true;
}

bool qtractorLv2PluginParam::isBoundedAbove (void) const
{
	return true;
}

bool qtractorLv2PluginParam::isDefaultValue (void) const
{
	return true;
}

bool qtractorLv2PluginParam::isLogarithmic (void) const
{
	return (m_iPortHints & Logarithmic);
}

bool qtractorLv2PluginParam::isSampleRate (void) const
{
	return (m_iPortHints & SampleRate);
}

bool qtractorLv2PluginParam::isInteger (void) const
{
	return (m_iPortHints & Integer);
}

bool qtractorLv2PluginParam::isToggled (void) const
{
	return (m_iPortHints & Toggled);
}

bool qtractorLv2PluginParam::isDisplay (void) const
{
	return !m_display.isEmpty();
}


// Current display value.
QString qtractorLv2PluginParam::display (void) const
{
	// Check if current value is mapped...
	if (isDisplay()) {
		const QString& sValue = QString::number(value());
		if (m_display.contains(sValue))
			return m_display.value(sValue);
	}

	// Default parameter display value...
	return qtractorPluginParam::display();
}


#endif	// CONFIG_LV2

// end of qtractorLv2Plugin.cpp
