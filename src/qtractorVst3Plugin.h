// qtractorVst3Plugin.h
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

#ifndef __qtractorVst3Plugin_h
#define __qtractorVst3Plugin_h

#include "qtractorPlugin.h"

#include <QString>
#include <QHash>

#include <alsa/asoundlib.h>


// Forward decls.
class qtractorAudioEngine;

class QWidget;


//----------------------------------------------------------------------
// class qtractorVst3PluginType -- VST3 plugin meta-interface decl.
//

class qtractorVst3PluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorVst3PluginType(qtractorPluginFile *pFile, unsigned long iIndex);

	// Destructor.
	~qtractorVst3PluginType();

	// Factory method (static)
	static qtractorVst3PluginType *createType (
		qtractorPluginFile *pFile, unsigned long iIndex);

	// Executive methods.
	bool open();
	void close();

	// It can be only one...
	unsigned short instances (
		unsigned short iChannels, bool /*bMidi*/) const
		{ return (iChannels > 0 ? 1 : 0); }

	// Instance cached-deferred accessors.
	const QString& aboutText();

	// Forward decls.
	class Impl;

	Impl *impl() const { return m_pImpl; }

private:

	// Instance variables.
	Impl *m_pImpl;
};


//----------------------------------------------------------------------
// class qtractorVst3Plugin -- VST3 plugin instance interface decl.
//

class qtractorVst3Plugin : public qtractorPlugin
{
public:

	// Constructor.
	qtractorVst3Plugin(qtractorPluginList *pList,
		qtractorVst3PluginType *pType);

	// Destructor.
	~qtractorVst3Plugin();

	// Forward decl.
	class Param;

	// Channel/instance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// Parameter update methods.
	void updateParam(qtractorPlugin::Param *pParam, float fValue, bool bUpdate);

	// Parameters update method.
	void updateParamValues(bool bUpdate);

	// Parameter finder (by id).
	qtractorPlugin::Param *findParamId(int id) const;

	// Configuration state stuff.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// Open/close editor widget.
	void openEditor(QWidget *pParent = nullptr);
	void closeEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	void setEditorTitle(const QString& sTitle);

	// Our own editor widget accessor.
	QWidget *editorWidget() const;

	// Processor stuff...
	//
	void process_midi_in(unsigned char *data, unsigned int size,
		unsigned long offset, unsigned short port);
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Plugin current latency (in frames);
	unsigned long latency() const;

	// Provisional program/patch accessor.
	bool getProgram(int iIndex, Program& program) const;

	// Specific MIDI instrument selector.
	void selectProgram(int iBank, int iProg);

	// Plugin preset i/o (configuration from/to state files).
	bool loadPresetFile(const QString& sFilename);
	bool savePresetFile(const QString& sFilename);

	// Common host-time keeper (static)
	static void updateTime(qtractorAudioEngine *pAudioEngine);

	// Host cleanup (static).
	static void clearAll();

	// Forward decls.
	class Impl;

	Impl *impl() const { return m_pImpl; }

protected:

	// Forward decls.
	class Handler;
	class RunLoop;
	class EditorFrame;
	class EditorWidget;
	class ParamQueue;
	class ParamChanges;
	class ParamTransfer;
	class EventList;
	class Stream;

	// Plugin instance initializer.
	void initialize();

	// Internal accessors.
	EditorFrame *editorFrame() const;

private:

	// Instance variables.
	Impl *m_pImpl;

	EditorFrame  *m_pEditorFrame;
	EditorWidget *m_pEditorWidget;

	// Audio I/O buffer pointers.
	float **m_ppIBuffer;
	float **m_ppOBuffer;

	// Dummy I/O buffers.
	float *m_pfIDummy;
	float *m_pfODummy;

	// MIDI Event decoder.
	snd_midi_event_t *m_pMidiParser;

	// Identififier-parameter map.
	QHash<int, qtractorPlugin::Param *> m_paramIds;
};


//----------------------------------------------------------------------------
// qtractorVst3Plugin::Param -- VST3 plugin parameter interface decl.
//
class qtractorVst3Plugin::Param : public qtractorPlugin::Param
{
public:

	// Constructor.
	Param(qtractorVst3Plugin *pPlugin, unsigned long iIndex);

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


#endif  // __qtractorVst3Plugin_h

// end of qtractorVst3Plugin.h
