// qtractorLv2Plugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

// URI map feature for event types (MIDI).
#include "lv2_uri_map.h"

static uint32_t qtractor_lv2_uri_to_id (
	LV2_URI_Map_Callback_Data /*data*/, const char *map, const char *uri )
{
	if (strcmp(map, LV2_EVENT_URI) == 0 &&
		strcmp(uri, SLV2_EVENT_CLASS_MIDI) == 0)
		return QTRACTOR_LV2_MIDI_EVENT_ID;
	else
		return 0;
}

// LV2 type 0 events (not supported anyway).
static uint32_t qtractor_lv2_event_ref (
	LV2_Event_Callback_Data /*data*/, LV2_Event */*event*/ )
{
	return 0;
}

static LV2_URI_Map_Feature g_lv2_uri_map =
	{ NULL, qtractor_lv2_uri_to_id };
static const LV2_Feature g_lv2_uri_map_feature =
	{ "http://lv2plug.in/ns/ext/uri-map", &g_lv2_uri_map };

static LV2_Event_Feature g_lv2_event_ref =
	{ NULL, qtractor_lv2_event_ref, qtractor_lv2_event_ref };
static const LV2_Feature g_lv2_event_ref_feature =
	{ "http://lv2plug.in/ns/ext/event", &g_lv2_event_ref };

static const LV2_Feature *g_lv2_features[] =
	{ &g_lv2_uri_map_feature, &g_lv2_event_ref_feature, NULL };

#else

static const LV2_Feature *g_lv2_features[] = { NULL };

#endif	// !CONFIG_LV2_EVENT


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

// Supported plugin features.
static SLV2Value g_slv2_realtime_hint    = NULL;

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
	m_sName = slv2_value_as_string(name);
	slv2_value_free(name);

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

	// Set up the feature we may want to know (as hints).
	g_slv2_realtime_hint = slv2_value_new_uri(g_slv2_world,
		SLV2_NAMESPACE_LV2 "hardRtCapable");

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

	slv2_value_free(g_slv2_realtime_hint);

	slv2_value_free(g_slv2_input_class);
	slv2_value_free(g_slv2_output_class);
	slv2_value_free(g_slv2_control_class);
	slv2_value_free(g_slv2_audio_class);
	slv2_value_free(g_slv2_event_class);
	slv2_value_free(g_slv2_midi_class);

	slv2_plugins_free(g_slv2_world, g_slv2_plugins);
	slv2_world_free(g_slv2_world);

	g_slv2_input_class   = NULL;
	g_slv2_output_class  = NULL;
	g_slv2_control_class = NULL;
	g_slv2_audio_class   = NULL;
	g_slv2_event_class   = NULL;
	g_slv2_midi_class    = NULL;

	g_slv2_plugins = NULL;
	g_slv2_world   = NULL;
}


// Plugin type listing (static).
bool qtractorLv2PluginType::getTypes ( qtractorPluginTypeList& types )
{
	if (g_slv2_plugins == NULL)
		return false;

	unsigned int iNumPlugins = slv2_plugins_size(g_slv2_plugins);
	for (unsigned int i = 0; i < iNumPlugins; ++i) {
		SLV2Plugin  plugin = slv2_plugins_get_at(g_slv2_plugins, i);
		const char *pszUri = slv2_value_as_uri(slv2_plugin_get_uri(plugin));
		qtractorLv2PluginType *pLv2Type
			= qtractorLv2PluginType::createType(pszUri, plugin);
		if (pLv2Type) {
			if (pLv2Type->open()) {
				types.append(pLv2Type);
			} else {
				delete pLv2Type;
			}
		}
	}

	return true;
}


//----------------------------------------------------------------------------
// qtractorLv2Plugin -- LV2 plugin instance.
//

// Constructors.
qtractorLv2Plugin::qtractorLv2Plugin ( qtractorPluginList *pList,
	qtractorLv2PluginType *pLv2Type )
	: qtractorPlugin(pList, pLv2Type), m_pInstances(NULL),
		m_piAudioIns(NULL), m_piAudioOuts(NULL)
	#ifdef CONFIG_LV2_EVENT
		, m_piMidiIns(NULL)
	#endif
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p] uri=\"%s\"",
		this, pLv2Type->filename().toUtf8().constData());
#endif

	// Get some structural data first...
	SLV2Plugin plugin = pLv2Type->slv2_plugin();
	if (plugin) {
		unsigned short iAudioIns  = pLv2Type->audioIns();
		unsigned short iAudioOuts = pLv2Type->audioOuts();
		if (iAudioIns > 0)
			m_piAudioIns = new unsigned long [iAudioIns];
		if (iAudioOuts > 0)
			m_piAudioOuts = new unsigned long [iAudioOuts];
		iAudioIns = iAudioOuts = 0;
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
	if (iInstances == instances())
		return;

	SLV2Plugin plugin = slv2_plugin();
	if (plugin == NULL)
		return;

	// Gotta go for a while...
	bool bActivated = isActivated();
	setActivated(false);

	if (m_pInstances) {
		for (unsigned short i = 0; i < instances(); ++i)
			slv2_instance_free(m_pInstances[i]);
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

	// Allocate new instances...
	m_pInstances = new SLV2Instance [iInstances];
	for (unsigned short i = 0; i < iInstances; ++i) {
		// Instantiate them properly first...
		SLV2Instance instance
			= slv2_plugin_instantiate(plugin, sampleRate(), g_lv2_features);
		// (Dis)connect all ports...
		unsigned long iNumPorts = slv2_plugin_get_num_ports(plugin);
		for (unsigned long k = 0; k < iNumPorts; ++k)
			slv2_instance_connect_port(instance, k, NULL);
		// Connect all existing input control ports...
		QListIterator<qtractorPluginParam *> param(params());
		while (param.hasNext()) {
			qtractorPluginParam *pParam = param.next();
			slv2_instance_connect_port(instance,
				pParam->index(), pParam->data());
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
		// This is it...
		m_pInstances[i] = instance;
	}

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();

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


// Do the actual activation.
void qtractorLv2Plugin::activate (void)
{
	if (m_pInstances) {
		for (unsigned short i = 0; i < instances(); ++i)
			slv2_instance_activate(m_pInstances[i]);
	}
}


// Do the actual deactivation.
void qtractorLv2Plugin::deactivate (void)
{
	if (m_pInstances) {
		for (unsigned short i = 0; i < instances(); ++i)
			slv2_instance_deactivate(m_pInstances[i]);
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

	setDefaultValue(vdef && slv2_value_is_float(vdef)
		? slv2_value_as_float(vdef) : 0.0f);
	setMinValue(vmin && slv2_value_is_float(vmin)
		? slv2_value_as_float(vmin) : 0.0f);
	setMaxValue(vmax && slv2_value_is_float(vmax)
		? slv2_value_as_float(vmax) : 1.0f);

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
			if (value && label && slv2_value_is_float(value)) {
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
