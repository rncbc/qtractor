// qtractorLv2Plugin.h
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorLv2Plugin_h
#define __qtractorLv2Plugin_h

#include "qtractorPlugin.h"

#include <lilv/lilv.h>

#ifdef CONFIG_LV2_EVENT
// LV2 Event/MIDI support.
#include "lv2/lv2plug.in/ns/ext/event/event.h"
#include "lv2/lv2plug.in/ns/ext/event/event-helpers.h"
#endif

#ifdef CONFIG_LV2_ATOM
// LV2 Atom/MIDI support.
#include "lv2_atom_helpers.h"
#endif

#ifndef QTRACTOR_LV2_MIDI_EVENT_ID
#define QTRACTOR_LV2_MIDI_EVENT_ID 1
#endif

#ifdef CONFIG_LV2_WORKER
// LV2 Worker/Schedule support.
#include "lv2/lv2plug.in/ns/ext/worker/worker.h"
// Forward declarations.
class qtractorLv2Worker;
#endif

#ifdef CONFIG_LV2_UI
// LV2 UI support.
#ifdef CONFIG_LIBSUIL
#include <suil/suil.h>
#endif
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"
// LV2 UI data/instance access support.
#include "lv2/lv2plug.in/ns/ext/data-access/data-access.h"
#include "lv2/lv2plug.in/ns/ext/instance-access/instance-access.h"
#ifdef CONFIG_LV2_ATOM
#include <jack/ringbuffer.h>
#endif
#ifdef CONFIG_LV2_EXTERNAL_UI
// LV2 External UI support.
#include "lv2_external_ui.h"
#endif
#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_GTK2
#include <QWindow>
#endif	// CONFIG_LV2_UI_GTK2
#endif
#endif	// CONFIG_LV2_UI


#ifdef CONFIG_LV2_STATE
// LV2 State support.
#include "lv2/lv2plug.in/ns/ext/state/state.h"
#endif

#ifdef CONFIG_LV2_PROGRAMS
// LV2 Programs support.
#include "lv2_programs.h"
#endif

#ifdef CONFIG_LV2_PRESETS
// LV2 Presets support.
#include "lv2/lv2plug.in/ns/ext/presets/presets.h"
#endif

#ifdef CONFIG_LV2_TIME
// LV2 Time support.
#include "lv2/lv2plug.in/ns/ext/time/time.h"
// Forward decl.
class qtractorAudioEngine;
#endif

#ifdef CONFIG_LV2_OPTIONS
// LV2 Options support.
#include "lv2/lv2plug.in/ns/ext/options/options.h"
#endif

#ifdef CONFIG_LV2_PATCH
// LV2 Patch/properties support.
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include <QVariant>
#endif


//----------------------------------------------------------------------------
// qtractorLv2PluginType -- LV2 plugin type instance.
//

class qtractorLv2PluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorLv2PluginType(const QString& sUri, LilvPlugin *plugin = NULL)
		: qtractorPluginType(NULL, 0, qtractorPluginType::Lv2),
			m_sUri(sUri), m_lv2_plugin(plugin)	{}

	// Destructor.
	~qtractorLv2PluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Factory method (static)
	static qtractorLv2PluginType *createType(const QString& sUri);

	// LV2 plugin URI (virtual override).
	QString filename() const
		{ return m_sUri; }

	// LV2 descriptor method (static)
	static LilvPlugin *lv2_plugin(const QString& sUri);

	// Specific accessors.
	LilvPlugin *lv2_plugin() const
		{ return m_lv2_plugin; }

	// LV2 World stuff (ref. counted).
	static void lv2_open();
	static void lv2_close();

	// Plugin type (URI) listing (static).
	static QStringList lv2_plugins();

#ifdef CONFIG_LV2_EVENT
	unsigned short eventIns()   const { return m_iEventIns;   }
	unsigned short eventOuts()  const { return m_iEventOuts;  }
#endif
#ifdef CONFIG_LV2_ATOM
	unsigned short atomIns()    const { return m_iAtomIns;    }
	unsigned short atomOuts()   const { return m_iAtomOuts;   }
#endif

#ifdef CONFIG_LV2_UI_SHOW
	// Check for LV2 UI Show interface.
	bool lv2_ui_show_interface(LilvUI *ui) const;
#endif

	// Instance cached-deferred accesors.
	const QString& aboutText();

protected:

	// LV2 plugin URI.
	QString    m_sUri;

	// LV2 descriptor itself.
	LilvPlugin *m_lv2_plugin;

#ifdef CONFIG_LV2_EVENT
	unsigned short m_iEventIns;
	unsigned short m_iEventOuts;
#endif
#ifdef CONFIG_LV2_ATOM
	unsigned short m_iAtomIns;
	unsigned short m_iAtomOuts;
#endif
};


//----------------------------------------------------------------------------
// qtractorLv2Plugin -- LV2 plugin instance.
//

class qtractorLv2Plugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorLv2Plugin(qtractorPluginList *pList,
		qtractorLv2PluginType *pLv2Type);

	// Destructor.
	~qtractorLv2Plugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Specific accessors.
	LilvPlugin *lv2_plugin() const;
	LilvInstance *lv2_instance(unsigned short iInstance) const;

	LV2_Handle lv2_handle(unsigned short iInstance) const;

	// Audio port numbers.
	unsigned long audioIn(unsigned short i)
		{ return m_piAudioIns[i]; }
	unsigned long audioOut(unsigned short i)
		{ return m_piAudioOuts[i]; }

#ifdef CONFIG_LV2_UI

	// GUI Editor stuff.
	void openEditor(QWidget *pParent = NULL);
	void closeEditor();
	void idleEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	// GUI editor window title methods.
	void setEditorTitle(const QString& sTitle);
	void updateEditorTitleEx();

	// GUI editor window (re)position methods.
	void saveEditorPos();
	void loadEditorPos();

	// Parameter update method.
	void updateParam(qtractorPluginParam *pParam, float fValue, bool bUpdate);

	// Idle editor (static).
	static void idleEditorAll();

	// LV2 UI control change method.
	void lv2_ui_port_write(uint32_t port_index,
		uint32_t buffer_size, uint32_t protocol, const void *buffer);

	// LV2 UI portMap method.
	uint32_t lv2_ui_port_index(const char *port_symbol);

#ifdef CONFIG_LV2_UI_TOUCH
	// LV2 UI touch control (ui->host).
	void lv2_ui_touch(uint32_t port_index, bool grabbed);
#endif

	// LV2 UI resize control (host->ui).
	void lv2_ui_resize(const QSize& size);

	// GUI editor closed state.
	void setEditorClosed(bool bClosed)
		{ m_bEditorClosed = bClosed; }
	bool isEditorClosed() const
		{ return m_bEditorClosed; }

	void closeEditorEx();

#endif

	// Plugin configuration/state (save) snapshot.
	void freezeConfigs();

	// Plugin configuration/state (load) realization.
	void realizeConfigs();

	// Plugin configuration/state release.
	void releaseConfigs();

	// Plugin current latency (in frames);
	unsigned long latency() const
		{ return (m_pfLatency ? *m_pfLatency : 0.0f); }

#ifdef CONFIG_LV2_WORKER
	// LV2 Worker/Schedule extension data interface accessor.
	const LV2_Worker_Interface *lv2_worker_interface(unsigned short iInstance) const;
#endif

#ifdef CONFIG_LV2_STATE

	// LV2 State extension data interface accessor.
	const LV2_State_Interface *lv2_state_interface(unsigned short iInstance) const;

	LV2_State_Status lv2_state_store(
		uint32_t key, const void *value, size_t size, uint32_t type, uint32_t flags);
	const void *lv2_state_retrieve(
		uint32_t key, size_t *size, uint32_t *type, uint32_t *flags);

	// Load default plugin state.
	void lv2_state_load_default();

#endif

#ifdef CONFIG_LV2_STATE_FILES
	// LV2 State save directory (when not the default session one).
	const QString& lv2_state_save_dir() const;
#endif

	// URID map/unmap helpers.
	static LV2_URID    lv2_urid_map(const char *uri);
	static const char *lv2_urid_unmap(LV2_URID id);

#ifdef CONFIG_LV2_PROGRAMS

	// LV2 Programs extension data descriptor accessor.
	const LV2_Programs_Interface *lv2_programs_descriptor(unsigned short iInstance) const;

	// Bank/program selector override.
	void selectProgram(int iBank, int iProg);

	// Provisional program/patch accessor.
	bool getProgram(int iIndex, Program& program) const;

	// Program/patch notification.
	void lv2_program_changed(int iIndex);

#endif

#ifdef CONFIG_LV2_PRESETS

	// Refresh and load preset labels listing.
	QStringList presetList() const;

	// Load/Save plugin state from/into a named preset.
	bool loadPreset(const QString& sPreset);
	bool savePreset(const QString& sPreset);

	// Delete plugin state preset (from file-system).
	bool deletePreset(const QString& sPreset);

	// Whether given preset is internal/read-only.
	bool isReadOnlyPreset(const QString& sPreset) const;

#endif

#ifdef CONFIG_LV2_PATCH

	// LV2 Patch/properties support...
	void lv2_property_changed(LV2_URID key, const LV2_Atom *value);
	void lv2_property_update(LV2_URID key);

	// LV2 Patch/property registry item.
	//
	class Property
	{
	public:

		Property(const LilvNode *property);

		LV2_URID key() const
			{ return m_key; }
		const QString& uri() const
			{ return m_uri; }
		const QString& name() const
			{ return m_name; }
		LV2_URID type() const
			{ return m_type; }

		bool isToggled() const;
		bool isInteger() const;
		bool isString()  const;
		bool isPath()    const;

		float minValue() const
			{ return m_min; }
		float maxValue() const
			{ return m_max; }
		float defValue() const
			{ return m_def; }

		void setValue(const QVariant& value)
			{ m_value = value; }
		const QVariant& value() const
			{ return m_value; }

	private:

		LV2_URID m_key;
		QString  m_uri;
		QString  m_name;
		LV2_URID m_type;

		float    m_min;
		float    m_max;
		float    m_def;

		QVariant m_value;
	};

	// LV2 Patch/properties registry.
	typedef QMap<QString, Property *> Properties;

	// LV2 Patch/properties registry accessor.
	const Properties& lv2_properties() const
		{ return m_lv2_properties; }

#endif	// CONFIG_LV2_PATCH

#ifdef CONFIG_LV2_TIME
	// Update LV2 Time from JACK transport position.
	static void updateTime(qtractorAudioEngine *pAudioEngine);
	static void updateTimePost();
#ifdef CONFIG_LV2_TIME_POSITION
	// Make ready LV2 Time position.
	void lv2_time_position_changed();
#endif
#endif

#ifdef CONFIG_LV2_UI
protected:

	// Alternate UI instantiation stuff...
	bool lv2_ui_instantiate(
		const char *ui_host_uri, const char *plugin_uri,
		const char *ui_uri,	const char *ui_type_uri,
		const char *ui_bundle_path, const char *ui_binary_path);

	void lv2_ui_port_event(
		uint32_t port_index, uint32_t buffer_size,
		uint32_t format, const void *buffer);

	const void *lv2_ui_extension_data(const char *uri);

#endif	// CONFIG_LV2_UI

#ifdef CONFIG_LV2_PATCH
	// LV2 Patch/properties inventory.
	void lv2_patch_properties(const char *pszPatch);
#endif

private:

	// Instance variables.
	LilvInstance **m_ppInstances;

	// List of output control port indexes and data.
	unsigned long *m_piControlOuts;
	float         *m_pfControlOuts;
	float         *m_pfControlOutsLast;

	// List of audio port indexes.
	unsigned long *m_piAudioIns;
	unsigned long *m_piAudioOuts;

	// Dummy I/O buffers.
	float *m_pfIDummy;
	float *m_pfODummy;

#ifdef CONFIG_LV2_EVENT
	// List of LV2 Event/MIDI port indexes.
	unsigned long *m_piEventIns;
	unsigned long *m_piEventOuts;
#endif

#ifdef CONFIG_LV2_ATOM

	// List of LV2 Atom/MIDI port indexes and buffers.
	unsigned long *m_piAtomIns;
	unsigned long *m_piAtomOuts;

	LV2_Atom_Buffer **m_lv2_atom_buffer_ins;
	LV2_Atom_Buffer **m_lv2_atom_buffer_outs;

	unsigned long  m_lv2_atom_midi_port_in;
	unsigned long  m_lv2_atom_midi_port_out;

#endif

	// Local copy of features array.
	LV2_Feature  **m_lv2_features;

#ifdef CONFIG_LV2_WORKER
	// LV2 Worker/Schedule support.
	qtractorLv2Worker *m_lv2_worker;
#endif

#ifdef CONFIG_LV2_UI

	int            m_lv2_ui_type;

	QByteArray     m_aEditorTitle;
	bool           m_bEditorVisible;

	volatile bool  m_bEditorClosed;

	LilvUIs       *m_lv2_uis;
	LilvUI        *m_lv2_ui;

	LV2_Extension_Data_Feature m_lv2_ui_data_access;

	LV2_Feature    m_lv2_ui_data_access_feature;
	LV2_Feature    m_lv2_ui_instance_access_feature;

	LV2_Feature  **m_lv2_ui_features;

	// Alternate UI instantiation stuff.
	QLibrary      *m_lv2_ui_library;

	const LV2UI_Descriptor *m_lv2_ui_descriptor;

	LV2UI_Port_Map m_lv2_ui_port_map;
	LV2_Feature    m_lv2_ui_port_map_feature;

	// Common UI instantiation stuff.
	LV2UI_Handle   m_lv2_ui_handle;
	LV2UI_Widget   m_lv2_ui_widget;

#ifdef CONFIG_LIBSUIL
	SuilHost      *m_suil_host;
	SuilInstance  *m_suil_instance;
#endif

#ifdef CONFIG_LV2_ATOM

	// LV2 Atom control (ring)buffers for UI updates.
	struct ControlEvent
	{
		uint32_t index;
		uint32_t protocol;
		uint32_t size;
		uint8_t  body[];
	};

	jack_ringbuffer_t *m_ui_events;
	jack_ringbuffer_t *m_plugin_events;

#endif

#ifdef CONFIG_LV2_EXTERNAL_UI
	LV2_Feature          m_lv2_ui_external_feature;
	LV2_External_UI_Host m_lv2_ui_external_host;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	LV2_Feature          m_lv2_ui_external_deprecated_feature;
#endif
#endif

	// Our own Qt UI widget (native).
	class EventFilter;

	EventFilter *m_pQtFilter;
	QWidget     *m_pQtWidget;
	bool         m_bQtDelete;

	// Changed UI params hash-queue.
	QHash<unsigned long, float> m_ui_params;

#ifdef CONFIG_LV2_UI_TOUCH
	LV2UI_Touch m_lv2_ui_touch;
	LV2_Feature m_lv2_ui_touch_feature;
	QHash<unsigned long, bool> m_ui_params_touch;
#endif

#ifdef CONFIG_LV2_UI_IDLE
	// LV2 UI Idle extension data interface.
	const LV2UI_Idle_Interface *m_lv2_ui_idle_interface;
#endif
#ifdef CONFIG_LV2_UI_SHOW
	// LV2 UI Show extension data interface.
	const LV2UI_Show_Interface *m_lv2_ui_show_interface;
#endif

#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_GTK2
	struct _GtkWidget *m_pGtkWindow;
	QWindow           *m_pQtWindow;
#endif	// CONFIG_LV2_UI_GTK2
#ifdef CONFIG_LV2_UI_X11
	LV2UI_Resize m_lv2_ui_resize;
	LV2_Feature  m_lv2_ui_resize_feature;
	LV2_Feature  m_lv2_ui_parent_feature;
#endif	// CONFIG_LV2_UI_X11
#endif

#endif	// CONFIG_LV2_UI

#ifdef CONFIG_LV2_STATE
	QHash<QString, QByteArray> m_lv2_state_configs;
	QHash<QString, uint32_t>   m_lv2_state_ctypes;
	LV2_Feature                m_lv2_state_load_default_feature;
#endif

#ifdef CONFIG_LV2_STATE_FILES
	LV2_Feature                m_lv2_state_map_path_feature;
	LV2_State_Map_Path         m_lv2_state_map_path;
#ifdef CONFIG_LV2_STATE_MAKE_PATH
	LV2_Feature                m_lv2_state_make_path_feature;
	LV2_State_Make_Path        m_lv2_state_make_path;
#endif
	QString                    m_lv2_state_save_dir;
#endif

#ifdef CONFIG_LV2_PROGRAMS
	LV2_Feature                m_lv2_programs_host_feature;
	LV2_Programs_Host          m_lv2_programs_host;
#endif

#ifdef CONFIG_LV2_PRESETS
	// LV2 Presets label-to-uri map.
	QHash<QString, QString>    m_lv2_presets;
#endif

#ifdef CONFIG_LV2_TIME
	// LV2 Time designated ports map.
	QHash<unsigned long, int>  m_lv2_time_ports;
#ifdef CONFIG_LV2_TIME_POSITION
	// LV2 Time position port enabled index.
	bool          m_lv2_time_position_enabled;
	unsigned long m_lv2_time_position_port_in;
	unsigned int  m_lv2_time_position_changed;
#endif
#endif

#ifdef CONFIG_LV2_PATCH
	// LV2 Patch/properties support.
	unsigned long m_lv2_patch_port_in;
	unsigned int  m_lv2_patch_changed;
	// LV2 Patch/properties registry.
	Properties m_lv2_properties;
#endif

#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
	LV2_Feature        m_lv2_options_feature;
	LV2_Options_Option m_lv2_options[5];
	uint32_t           m_iMinBlockLength;
	uint32_t           m_iMaxBlockLength;
	uint32_t           m_iNominalBlockLength;
	uint32_t           m_iSequenceSize;
#endif
#ifdef CONFIG_LV2_UI
	LV2_Feature        m_lv2_ui_options_feature;
	LV2_Options_Option m_lv2_ui_options[4];
	float              m_fUpdateRate;
	double             m_dSampleRate;
#endif
#endif

	// Plugin current latency output control port;
	float *m_pfLatency;
};


//----------------------------------------------------------------------------
// qtractorLv2PluginParam -- LV2 plugin control input port instance.
//

class qtractorLv2PluginParam : public qtractorPluginParam
{
public:

	// Constructors.
	qtractorLv2PluginParam(qtractorLv2Plugin *pLv2Plugin,
		unsigned long iIndex);

	// Port range hints predicate methods.
	bool isBoundedBelow() const;
	bool isBoundedAbove() const;
	bool isDefaultValue() const;
	bool isLogarithmic() const;
	bool isSampleRate() const;
	bool isInteger() const;
	bool isToggled() const;
	bool isDisplay() const;

	// Current display value.
	QString display() const;

private:

	// Port bit-wise hints.
	enum {
		None        = 0,
		Toggled     = 1,
		Integer     = 2,
		SampleRate  = 4,
		Logarithmic = 8,
	};

	// Instance variables.
	unsigned int m_iPortHints;

	QHash<QString, QString> m_display;
};


#endif  // __qtractorLv2Plugin_h

// end of qtractorLv2Plugin.h
