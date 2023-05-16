// qtractorClapPlugin.h
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

#ifndef __qtractorClapPlugin_h
#define __qtractorClapPlugin_h

#include "qtractorPlugin.h"

#include <QString>
#include <QHash>

#include <alsa/asoundlib.h>


// Forward decls.
class qtractorAudioEngine;

class QWidget;


//----------------------------------------------------------------------
// class qtractorClapPluginType -- CLAP plugin meta-interface decl.
//

class qtractorClapPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorClapPluginType(qtractorPluginFile *pFile, unsigned long iIndex);

	// Destructor.
	~qtractorClapPluginType();

	// Factory method (static)
	static qtractorClapPluginType *createType (
		qtractorPluginFile *pFile, unsigned long iIndex);

	// Executive methods.
	bool open();
	void close();

	// It can be only one...
	unsigned short instances (
		unsigned short iChannels, bool /*bMidi*/) const
		{ return (iChannels > 0 ? 1 : 0); }

	// Instance cached-deferred accessors.
	const QString& aboutText()
		{ return m_sAboutText; }

	int midiDialectIns() const
		{ return m_iMidiDialectIns; }
	int midiDialectOuts() const
		{ return m_iMidiDialectOuts; }

	// Forward decls.
	class Impl;

	Impl *impl() const { return m_pImpl; }

protected:

	// Instance cached-deferred properties.
	QString m_sAboutText;

private:

	// Instance variables.   
	Impl *m_pImpl;

	int m_iMidiDialectIns;
	int m_iMidiDialectOuts;
};


//----------------------------------------------------------------------
// class qtractorClapPlugin -- CLAP plugin instance interface decl.
//

class qtractorClapPlugin : public qtractorPlugin
{
public:
   
	// Constructor.
	qtractorClapPlugin(qtractorPluginList *pList,
		qtractorClapPluginType *pType);

	// Destructor.
	~qtractorClapPlugin();

	// Forward decl.
	class Param;

	// Channel/instance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// Instance parameters (de)initializers.
	void addParams();
	void clearParams();

	// Clear a specific parameter.
	void clearParam(qtractorPlugin::Param *pParam);

	// Parameter update methods.
	void updateParam(qtractorPlugin::Param *pParam, float fValue, bool bUpdate);

	// Parameters update methods.
	void updateParamValues(bool bUpdate);

	// Parameter finder (by id).
	qtractorPlugin::Param *findParamId(int id) const;

	// Configuration state stuff.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// Provisional note name accessor.
	bool getNoteName(int iIndex, NoteName& note) const;

	// Update instrument/note names cache.
	void updateNoteNames();

	// Open/close editor widget.
	void openEditor(QWidget *pParent = nullptr);
	void closeEditor();

	// Parameters post-update methods.
	void idleEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	// Update editor widget caption.
	void setEditorTitle(const QString& sTitle);

	// GUI editor widget handle (if not floating).
	QWidget *editorWidget() const;

	// GUI editor created/active state.
	bool isEditorCreated() const;

	// Processor stuff...
	//
	void process_midi_in(unsigned char *data, unsigned int size,
		unsigned long offset, unsigned short port);
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Plugin current latency (in frames);
	unsigned long latency() const;

	// Plugin preset i/o (configuration from/to state files).
	bool loadPresetFile(const QString& sFilename);
	bool savePresetFile(const QString& sFilename);

	// Reinitialize the plugin instance.
	void request_restart();
	void restart();

	// Idle editor.
	static void idleEditorAll();

	// Common host-time keeper (static)
	static void updateTime(qtractorAudioEngine *pAudioEngine);

	// Host cleanup (static).
	static void clearAll();

	// Forward decls.
	class Impl;

	Impl *impl() const { return m_pImpl; }

protected:

	// Forward decls.
	class EditorWidget;

	// Plugin instance (de)initializers.
	void initialize();
	void deinitialize();

	// Clear instrument/note names cache.
	void clearNoteNames();

	// Make up some others dirty...
	void updateDirtyCount();

private:

	// Instance variables.
	qtractorClapPluginType *m_pType;

	Impl *m_pImpl;

	// GUI Editor stuff...
	bool m_bEditorCreated;
	bool m_bEditorVisible;

	EditorWidget *m_pEditorWidget;

	// Audio I/O buffer pointers.
	float **m_ppIBuffer;
	float **m_ppOBuffer;

	// Dummy I/O buffers.
	float *m_pfIDummy;
	float *m_pfODummy;

	// MIDI Event decoder.
	snd_midi_event_t *m_pMidiParser;

	// Identififier-parameters map.
	QHash<int, qtractorPlugin::Param *> m_paramIds;
	QHash<int, double> m_paramValues;

	// Note-names cache.
	QList<NoteName *> m_noteNames;
};


//----------------------------------------------------------------------------
// qtractorClapPlugin::Param -- CLAP plugin parameter interface decl.
//

class qtractorClapPlugin::Param : public qtractorPlugin::Param
{
public:

	// Constructors.
	Param(qtractorClapPlugin *pPlugin, unsigned long iIndex);

	// Destructor.
	~Param();

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

	// Forward decls.
	class Impl;

	Impl *impl() const { return m_pImpl; }

private:

	// Instance variables.
	Impl *m_pImpl;
};


#endif  // __qtractorClapPlugin_h

// end of qtractorClapPlugin.h
