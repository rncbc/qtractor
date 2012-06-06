// qtractorLv2Plugin.h
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifdef CONFIG_LIBSLV2
#include <slv2/slv2.h>
#endif

#ifdef CONFIG_LIBLILV
#include <lilv/lilv.h>
#define SLV2Plugin   LilvPlugin*
#define SLV2Instance LilvInstance*
#define SLV2UIs      LilvUIs*
#define SLV2UI       LilvUI*
#endif

#ifdef CONFIG_LIBSUIL
#include <suil/suil.h>
#endif

#ifdef CONFIG_LV2_EVENT
// LV2 Event/MIDI support.
#include "lv2_event.h"
#include "lv2_event_helpers.h"
#ifndef QTRACTOR_LV2_MIDI_EVENT_ID
#define QTRACTOR_LV2_MIDI_EVENT_ID 1
#endif
#endif

#ifdef CONFIG_LV2_ATOM
// LV2 Atom/MIDI support.
#include "lv2_atom.h"
#include "lv2_atom_helpers.h"
#ifndef QTRACTOR_LV2_MIDI_EVENT_ID
#define QTRACTOR_LV2_MIDI_EVENT_ID 1
#endif
#endif

#ifdef CONFIG_LV2_WORKER
// LV2 Worker/Schedule support.
#include "lv2_worker.h"
// Forward declarations.
class qtractorLv2Worker;
#endif

#if defined(CONFIG_LV2_GTK_UI) || defined(CONFIG_LV2_QT4_UI) || defined(CONFIG_LV2_EXTERNAL_UI)
#define CONFIG_LV2_UI 1
#endif

#ifdef CONFIG_LV2_UI
// LV2 UI support.
#include "lv2_ui.h"
// LV2 UI data/instance access support.
#include "lv2_data_access.h"
#include "lv2_instance_access.h"
#ifdef CONFIG_LV2_ATOM
#include <jack/ringbuffer.h>
#endif
#endif

#ifdef CONFIG_LV2_EXTERNAL_UI
// LV2 External UI support.
#include "lv2_external_ui.h"
#endif

#ifdef CONFIG_LV2_STATE
// LV2 State support.
#include "lv2_atom.h"
#include "lv2_state.h"
#endif

#ifdef CONFIG_LV2_PROGRAMS
// LV2 Programs support.
#include "lv2_programs.h"
#endif

#ifdef CONFIG_LIBLILV

#ifdef CONFIG_LV2_PRESETS
// LV2 Presets support.
#include "lv2_presets.h"
#endif

#ifdef CONFIG_LV2_TIME
// LV2 Time support.
#include "lv2_time.h"
// JACK Transport position support.
#include <jack/transport.h>
#endif

#endif	// CONFIG_LIBLILV


//----------------------------------------------------------------------------
// qtractorLv2PluginType -- LV2 plugin type instance.
//

class qtractorLv2PluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorLv2PluginType(const QString& sUri, SLV2Plugin plugin = NULL)
		: qtractorPluginType(NULL, 0, qtractorPluginType::Lv2),
			m_sUri(sUri), m_slv2_plugin(plugin)	{}

	// Destructor.
	~qtractorLv2PluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Factory method (static)
	static qtractorLv2PluginType *createType(
		const QString& sUri, SLV2Plugin plugin = NULL);

	// LV2 plugin URI (virtual override).
	QString filename() const
		{ return m_sUri; }

	// LV2 descriptor method (static)
	static SLV2Plugin slv2_plugin(const QString& sUri);

	// Specific accessors.
	SLV2Plugin slv2_plugin() const
		{ return m_slv2_plugin; }

	// LV2 World stuff (ref. counted).
	static void slv2_open();
	static void slv2_close();

	// Plugin type listing (static).
	static bool getTypes(qtractorPluginPath& path);

#ifdef CONFIG_LV2_EVENT
	unsigned short midiEventIns()  const { return m_iMidiEventIns;  }
	unsigned short midiEventOuts() const { return m_iMidiEventOuts; }
#endif
#ifdef CONFIG_LV2_ATOM
	unsigned short midiAtomIns()   const { return m_iMidiAtomIns;   }
	unsigned short midiAtomOuts()  const { return m_iMidiAtomOuts;  }
#endif

protected:

	// LV2 plgin URI.
	QString    m_sUri;

	// LV2 descriptor itself.
	SLV2Plugin m_slv2_plugin;

#ifdef CONFIG_LV2_EVENT
	unsigned short m_iMidiEventIns;
	unsigned short m_iMidiEventOuts;
#endif
#ifdef CONFIG_LV2_ATOM
	unsigned short m_iMidiAtomIns;
	unsigned short m_iMidiAtomOuts;
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
	SLV2Plugin slv2_plugin() const;
	SLV2Instance slv2_instance(unsigned short iInstance) const;

	LV2_Handle lv2_handle(unsigned short iInstance) const;

	// Audio port numbers.
	unsigned long audioIn(unsigned short i)
		{ return m_piAudioIns[i]; }
	unsigned long audioOut(unsigned short i)
		{ return m_piAudioOuts[i]; }

#ifdef CONFIG_LV2_UI

	// GUI Editor stuff.
	void openEditor(QWidget *pParent);
	void closeEditor();
	void idleEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	void setEditorTitle(const QString& sTitle);

	// Parameter update method.
	void updateParam(qtractorPluginParam *pParam, float fValue);

	// Idle editor (static).
	static void idleEditorAll();

	// LV2 UI descriptor accessor.
	const LV2UI_Descriptor *lv2_ui_descriptor() const;

	// LV2 UI handle accessor.
	LV2UI_Handle lv2_ui_handle() const;

	// LV2 UI control change method.
	void lv2_ui_write(uint32_t port_index,
		uint32_t buffer_size, uint32_t protocol, const void *buffer);

	// LV2 UI cleanup method.
	void lv2_ui_cleanup() const;

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

#endif

	// URID map/unmap helpers.
	static uint32_t    lv2_urid_map(const char *uri);
	static const char *lv2_urid_unmap(uint32_t id);

#ifdef CONFIG_LV2_PROGRAMS

	// LV2 Programs extension data descriptor accessor.
	const LV2_Programs_Interface *lv2_programs_descriptor(unsigned short iInstance) const;

#ifdef CONFIG_LV2_UI
	// LV2 Programs UI descriptor accessor.
	const LV2_Programs_UI_Interface *lv2_ui_programs_descriptor (void) const;
#endif

	// Bank/program selector override.
	void selectProgram(int iBank, int iProg);

	// Provisional program/patch accessor.
	bool getProgram(int iIndex, Program& program) const;

#endif

#ifdef CONFIG_LIBLILV
#ifdef CONFIG_LV2_PRESETS
	// Refresh and load preset labels listing.
	QStringList presetList() const;
	// Load/Save plugin state from/into a named preset.
	bool loadPreset(const QString& sPreset);
	bool savePreset(const QString& sPreset);
#endif
#ifdef CONFIG_LV2_TIME
	// Update LV2 Time from JACK transport position.
	static void updateTime(
		const jack_transport_state_t state, const jack_position_t *pPos);
#endif
#endif	// CONFIG_LIBLILV

private:

	// Instance variables.
	SLV2Instance  *m_pInstances;

	// List of output control port indexes and data.
	unsigned long *m_piControlOuts;
	float         *m_pfControlOuts;
	float         *m_pfControlOutsLast;

	// List of audio port indexes.
	unsigned long *m_piAudioIns;
	unsigned long *m_piAudioOuts;

#ifdef CONFIG_LV2_EVENT
	// List of LV2 Event/MIDI port indexes.
	unsigned long *m_piMidiEventIns;
	unsigned long *m_piMidiEventOuts;
#endif

#ifdef CONFIG_LV2_ATOM

	// List of LV2 Atom/MIDI port indexes and buffers.
	unsigned long *m_piMidiAtomIns;
	unsigned long *m_piMidiAtomOuts;

	LV2_Atom_Buffer  *m_lv2_atom_buffer_in;
	LV2_Atom_Buffer **m_lv2_atom_buffer_ins;

	LV2_Atom_Buffer  *m_lv2_atom_buffer_out;
	LV2_Atom_Buffer **m_lv2_atom_buffer_outs;

#endif

	// Local copy of features array.
	LV2_Feature  **m_lv2_features;

#ifdef CONFIG_LV2_WORKER
	// LV2 Worker/Schedule support.
	qtractorLv2Worker **m_lv2_workers;
#endif

#ifdef CONFIG_LV2_UI

	int            m_lv2_ui_type;

	QByteArray     m_aEditorTitle;
	bool           m_bEditorVisible;

	volatile bool  m_bEditorClosed;

	SLV2UIs        m_slv2_uis;
	SLV2UI         m_slv2_ui;

	LV2_Extension_Data_Feature m_lv2_data_access;

	LV2_Feature    m_lv2_data_access_feature;
	LV2_Feature    m_lv2_instance_access_feature;

	LV2_Feature  **m_lv2_ui_features;

#ifdef CONFIG_LIBSUIL
	SuilHost      *m_suil_host;
	SuilInstance  *m_suil_instance;
	SuilWidget     m_lv2_ui_widget;
#endif

#if CONFIG_LIBSLV2
	SLV2UIInstance m_slv2_ui_instance;
	LV2UI_Widget   m_lv2_ui_widget;
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
	lv2_external_ui_host m_lv2_ui_external_host;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	LV2_Feature          m_lv2_ui_external_deprecated_feature;
	lv2_external_ui_host m_lv2_ui_external_deprecated_host;
#endif
#endif

#ifdef CONFIG_LIBSLV2
#ifdef CONFIG_LV2_GTK_UI
	// Our own GTK UI widget (parent frame).
	struct _GtkWidget *m_pGtkWindow;
#endif
#endif

#ifdef CONFIG_LV2_QT4_UI
	// Our own Qt4 UI widget (native).
	class EventFilter;
	EventFilter *m_pQt4Filter;
	QWidget     *m_pQt4Widget;
#endif

#endif	// CONFIG_LV2_UI

#ifdef CONFIG_LV2_STATE
	QHash<QString, QByteArray> m_lv2_state_configs;
	QHash<QString, uint32_t>   m_lv2_state_ctypes;
#endif

#ifdef CONFIG_LV2_STATE_FILES
	LV2_Feature                m_lv2_state_map_path_feature;
	LV2_State_Map_Path         m_lv2_state_map_path;
	LV2_Feature                m_lv2_state_make_path_feature;
	LV2_State_Make_Path        m_lv2_state_make_path;
#endif

#ifdef CONFIG_LIBLILV
#ifdef CONFIG_LV2_PRESETS
	// LV2 Presets label-to-uri map.
	QHash<QString, QString> m_lv2_presets;
#endif
#ifdef CONFIG_LV2_TIME
	// LV2 Time designated ports map.
	QHash<int, unsigned long>  m_lv2_time_ports;
#endif
#endif	// CONFIG_LIBLILV
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

	// Destructor.
	~qtractorLv2PluginParam();

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
