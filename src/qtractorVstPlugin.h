// qtractorVstPlugin.h
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

#ifndef __qtractorVstPlugin_h
#define __qtractorVstPlugin_h

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

#if QT_VERSION < 0x050000
#if defined(Q_WS_X11)
#define CONFIG_VST_X11
#endif
#else
#if defined(QT_X11EXTRAS_LIB)
#define CONFIG_VST_X11
#endif
#endif

// Forward decls.
class QFile;


//----------------------------------------------------------------------------
// qtractorVstPluginType -- VST plugin type instance.
//

class qtractorVstPluginType : public qtractorPluginType
{
public:

	// Forward declarations.
	class Effect;

	// Constructor.
	qtractorVstPluginType(qtractorPluginFile *pFile,
		unsigned long iIndex, Effect *pEffect = NULL)
		: qtractorPluginType(pFile, iIndex, qtractorPluginType::Vst),
			m_pEffect(pEffect), m_iFlagsEx(0) {}

	// Destructor.
	~qtractorVstPluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Specific accessors.
	Effect *effect() const { return m_pEffect; }

	// Factory method (static)
	static qtractorVstPluginType *createType(
		qtractorPluginFile *pFile, unsigned long iIndex);

	// Effect instance method (static)
	static AEffect *vst_effect(qtractorPluginFile *pFile);

	// VST host dispatcher.
	int vst_dispatch(
		long opcode, long index, long value, void *ptr, float opt) const;

	// Instance cached-deferred accesors.
	const QString& aboutText();

protected:

	// VST flag inquirer.
	bool vst_canDo(const char *pszCanDo) const;

private:

	// VST descriptor reference.
	Effect      *m_pEffect;
	unsigned int m_iFlagsEx;
};


//----------------------------------------------------------------------------
// qtractorVstPlugin -- VST plugin instance.
//

class qtractorVstPlugin : public qtractorPlugin
{
public:

	// Constructor.
	qtractorVstPlugin(qtractorPluginList *pList,
		qtractorVstPluginType *pVstType);

	// Destructor.
	~qtractorVstPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Parameter update method.
	void updateParam(qtractorPluginParam *pParam, float fValue, bool bUpdate);

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
	void openEditor(QWidget *pParent = NULL);
	void closeEditor();
	void idleEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	void setEditorTitle(const QString& sTitle);

	// Specific accessors.
	AEffect *vst_effect(unsigned short iInstance) const;

	// VST host dispatcher.
	int vst_dispatch(unsigned short iInstance,
		long opcode, long index, long value, void *ptr, float opt) const;

	// Our own editor widget accessor.
	QWidget *editorWidget() const;

	// Our own editor widget size accessor.
	void resizeEditor(int w, int h);

	// Global VST plugin lookup.
	static qtractorVstPlugin *findPlugin(AEffect *pVstEffect);

	// Idle editor (static).
	static void idleEditorAll();

	// Editor widget forward decls.
	class EditorWidget;

#ifdef CONFIG_VST_X11
#if QT_VERSION < 0x050000
	// Global X11 event filter.
	static bool x11EventFilter(void *pvEvent);
#endif
#endif

	// Parameter update method.
	void updateParamValues(bool bUpdate);

private:

	// Instance variables.
	qtractorVstPluginType::Effect **m_ppEffects;

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
// qtractorVstPluginParam -- VST plugin control input port instance.
//

class qtractorVstPluginParam : public qtractorPluginParam
{
public:

	// Constructors.
	qtractorVstPluginParam(qtractorVstPlugin *pVstPlugin,
		unsigned long iIndex);

	// Destructor.
	~qtractorVstPluginParam();

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
// class qtractorVstPreset -- VST preset file interface.
//

class qtractorVstPreset
{
public:

	// Constructor.
	qtractorVstPreset(qtractorVstPlugin *pVstPlugin);

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
	qtractorVstPlugin *m_pVstPlugin;
};


#endif  // __qtractorVstPlugin_h

// end of qtractorVstPlugin.h
