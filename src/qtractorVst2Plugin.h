// qtractorVst2Plugin.h
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

#ifndef __qtractorVst2Plugin_h
#define __qtractorVst2Plugin_h

#include "qtractorPlugin.h"

// Allow VST 2.3 compability mode.
// #define VST_2_4_EXTENSIONS 0
// #define VST_FORCE_DEPRECATED 1

#include <stdint.h>

#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
#define __cdecl
#endif

#ifdef CONFIG_VESTIGE
#include <vestige.h>
#else
#include <aeffectx.h>
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#if defined(Q_WS_X11)
#define CONFIG_VST2_X11
#endif
#endif

// Forward decls.
class QFile;


//----------------------------------------------------------------------------
// qtractorVst2PluginType -- VST2 plugin type instance.
//

class qtractorVst2PluginType : public qtractorPluginType
{
public:

	// Forward declarations.
	class Effect;

	// Constructor.
	qtractorVst2PluginType(qtractorPluginFile *pFile,
		unsigned long iIndex, Effect *pEffect = nullptr)
		: qtractorPluginType(pFile, iIndex, qtractorPluginType::Vst2),
			m_pEffect(pEffect), m_iFlagsEx(0) {}

	// Destructor.
	~qtractorVst2PluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Specific accessors.
	Effect *effect() const { return m_pEffect; }

	// Factory method (static)
	static qtractorVst2PluginType *createType(
		qtractorPluginFile *pFile, unsigned long iIndex);

	// Effect instance method (static)
	static AEffect *vst2_effect(qtractorPluginFile *pFile);

	// VST2 host dispatcher.
	int vst2_dispatch(
		long opcode, long index, long value, void *ptr, float opt) const;

	// Instance cached-deferred accessors.
	const QString& aboutText();

protected:

	// VST2 flag inquirer.
	bool vst2_canDo(const char *pszCanDo) const;

private:

	// VST2 descriptor reference.
	Effect      *m_pEffect;
	unsigned int m_iFlagsEx;
};


//----------------------------------------------------------------------------
// qtractorVst2Plugin -- VST2 plugin instance.
//

class qtractorVst2Plugin : public qtractorPlugin
{
public:

	// Constructor.
	qtractorVst2Plugin(qtractorPluginList *pList,
		qtractorVst2PluginType *pVst2Type);

	// Destructor.
	~qtractorVst2Plugin();

	// Forward decl.
	class Param;

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Parameter update method.
	void updateParam(qtractorPlugin::Param *pParam, float fValue, bool bUpdate);

	// Bank/program selector override.
	void selectProgram(int iBank, int iProg);

	// Provisional program/patch accessor.
	bool getProgram(int iIndex, Program& program) const;

	// Configuration (CLOB) stuff.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// Plugin current latency (in frames);
	unsigned long latency() const;

	// Plugin preset i/o (configuration from/to (fxp/fxb files).
	bool loadPresetFile(const QString& sFilename);
	bool savePresetFile(const QString& sFilename);

	// GUI Editor stuff.
	void openEditor(QWidget *pParent = nullptr);
	void closeEditor();
	void idleEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	void setEditorTitle(const QString& sTitle);

	// Specific accessors.
	AEffect *vst2_effect(unsigned short iInstance) const;

	// VST2 host dispatcher.
	int vst2_dispatch(unsigned short iInstance,
		long opcode, long index, long value, void *ptr, float opt) const;

	// Our own editor widget accessor.
	QWidget *editorWidget() const;

	// Our own editor widget size accessor.
	void resizeEditor(int w, int h);

	// Global VST2 plugin lookup.
	static qtractorVst2Plugin *findPlugin(AEffect *pVst2Effect);

	// Idle editor (static).
	static void idleEditorAll();

	// Editor widget forward decls.
	class EditorWidget;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_VST2_X11
	// Global X11 event filter.
	static bool x11EventFilter(void *pvEvent);
#endif	// CONFIG_VST2_X11
#endif

	// All parameters update method.
	void updateParamValues(bool bUpdate);

private:

	// Instance variables.
	qtractorVst2PluginType::Effect **m_ppEffects;

	// Audio I/O buffer pointers.
	float **m_ppIBuffer;
	float **m_ppOBuffer;

	// Dummy I/O buffers.
	float *m_pfIDummy;
	float *m_pfODummy;

	// Our own editor widget (parent frame).
	EditorWidget *m_pEditorWidget;

	volatile bool m_bEditorClosed;
};


//----------------------------------------------------------------------------
// qtractorVst2Plugin::Param -- VST2 plugin control input port instance.
//

class qtractorVst2Plugin::Param : public qtractorPlugin::Param
{
public:

	// Constructors.
	Param(qtractorVst2Plugin *pVst2Plugin, unsigned long iIndex);

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

	VstParameterProperties m_props;
};


//----------------------------------------------------------------------
// class qtractorVst2Preset -- VST2 preset file interface.
//

class qtractorVst2Preset
{
public:

	// Constructor.
	qtractorVst2Preset(qtractorVst2Plugin *pVst2Plugin);

	// File loader/saver.
	bool load(const QString& sFilename);
	bool save(const QString& sFilename);

protected:

	// Forward decls.
	struct BaseHeader;
	struct BankHeader;
	struct ProgHeader;
	struct Chunk;

	// Loader methods.
	bool load_bank_progs(QFile& file);
	bool load_prog_params(QFile& file);
	bool load_bank_chunk(QFile& file);
	bool load_prog_chunk(QFile& file);
	bool load_chunk(QFile& file, int preset);

	// Saver methods.
	bool save_bank_progs(QFile& file);
	bool save_prog_params(QFile& file);
	bool save_bank_chunk(QFile& file, const Chunk& chunk);
	bool save_prog_chunk(QFile& file, const Chunk& chunk);
	bool save_chunk(QFile& file, const Chunk& chunk);
	bool get_chunk(Chunk& chunk, int preset);

private:

	// Instance variables.
	qtractorVst2Plugin *m_pVst2Plugin;
};


#endif  // __qtractorVst2Plugin_h

// end of qtractorVst2Plugin.h
