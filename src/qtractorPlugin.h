// qtractorPlugin.h
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

#ifndef __qtractorPlugin_h
#define __qtractorPlugin_h

#include "qtractorEngine.h"

#include "qtractorMidiControlObserver.h"

#include <QStringList>
#include <QLibrary>
#include <QSize>

#include <QHash>


// Forward declarations.
class qtractorPluginPath;
class qtractorPluginFile;
class qtractorPluginList;
class qtractorPluginParam;
class qtractorPluginForm;
class qtractorPlugin;

class qtractorPluginListView;
class qtractorPluginListItem;

class qtractorMidiManager;

class qtractorSessionDocument;

class QDomDocument;
class QDomElement;


//----------------------------------------------------------------------------
// qtractorPluginType -- Plugin type instance.
//

class qtractorPluginType
{
public:

	// Have hints for plugin paths.
	enum Hint { Any = 0, Ladspa, Dssi, Vst, Lv2, Insert };

	// Constructor.
	qtractorPluginType(qtractorPluginFile *pFile, unsigned long iIndex,
		Hint typeHint) : m_iUniqueID(0), m_iControlIns(0), m_iControlOuts(0),
			m_iAudioIns(0), m_iAudioOuts(0), m_iMidiIns(0), m_iMidiOuts(0),
			m_bRealtime(false), m_bConfigure(false), m_bEditor(false),
			m_pFile(pFile), m_iIndex(iIndex), m_typeHint(typeHint) {}

	// Destructor (virtual)
	virtual ~qtractorPluginType() {}

	// Main properties accessors.
	qtractorPluginFile *file() const { return m_pFile; }
	unsigned long index() const { return m_iIndex; }
	Hint typeHint() const { return m_typeHint; }

	// Plugin filename accessor (default virtual).
	virtual QString filename() const;

	// Must be derived methods.
	virtual bool open()  = 0;
	virtual void close() = 0;

	// Cache accessors.
	const QString& name()        const { return m_sName;        }
	const QString& label()       const { return m_sLabel;       }
	unsigned long  uniqueID()    const { return m_iUniqueID;    }

	// Port count accessors..
	unsigned short controlIns()  const { return m_iControlIns;  }
	unsigned short controlOuts() const { return m_iControlOuts; }
	unsigned short audioIns()    const { return m_iAudioIns;    }
	unsigned short audioOuts()   const { return m_iAudioOuts;   }
	unsigned short midiIns()     const { return m_iMidiIns;     }
	unsigned short midiOuts()    const { return m_iMidiOuts;    }

	// Attribute accessors.
	bool           isRealtime()  const { return m_bRealtime;    }
	bool           isConfigure() const { return m_bConfigure;   }
	bool           isEditor()    const { return m_bEditor;      }
	bool           isMidi()      const { return m_iMidiIns > 0; }

	// Compute the number of instances needed
	// for the given input/output audio channels.
	virtual unsigned short instances(
		unsigned short iChannels, bool bMidi) const;

	// Plugin type(hint) textual helpers.
	static Hint hintFromText(const QString& sText);
	static QString textFromHint(Hint typeHint);

protected:

	// Cached name strings.
	QString m_sName;
	QString m_sLabel;

	// Cache unique identifier.
	unsigned long  m_iUniqueID;

	// Cached port counts.
	unsigned short m_iControlIns;
	unsigned short m_iControlOuts;
	unsigned short m_iAudioIns;
	unsigned short m_iAudioOuts;
	unsigned short m_iMidiIns;
	unsigned short m_iMidiOuts;

	// Cached flags.
	bool m_bRealtime;
	bool m_bConfigure;
	bool m_bEditor;

private:

	// Instance variables.
	qtractorPluginFile *m_pFile;
	unsigned long m_iIndex;
	Hint m_typeHint;
};


//----------------------------------------------------------------------------
// qtractorDummyPluginType -- Dummy plugin type instance.
//

class qtractorDummyPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorDummyPluginType(
		qtractorPluginFile *pFile, unsigned long iIndex, Hint typeHint);

	// Must be overriden methods.
	bool open();
	void close();

	// Factory method (static)
	static qtractorDummyPluginType *createType(
		qtractorPluginFile *pFile, unsigned long iIndex = 0, Hint typeHint = Vst);
};


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

class qtractorPluginFile : public QLibrary
{
public:

	// Constructor.
	qtractorPluginFile(const QString& sFilename)
		: QLibrary(sFilename) {}

	// Destructor.
	~qtractorPluginFile()
		{ close(); }

	// Helper property accessors.
	QString filename() const { return QLibrary::fileName(); }

	// Executive methods.
	bool open();
	void close();

	// Plugin type listing.
	bool getTypes(qtractorPluginPath& path,
		qtractorPluginType::Hint typeHint = qtractorPluginType::Any);

	// Plugin factory method.
	static qtractorPlugin *createPlugin(qtractorPluginList *pList,
		const QString& sFilename, unsigned long iIndex = 0,
		qtractorPluginType::Hint typeHint = qtractorPluginType::Any);
};


//----------------------------------------------------------------------------
// qtractorPluginPath -- Plugin path helper.
//

class qtractorPluginPath
{
public:

	// Constructor.
	qtractorPluginPath(qtractorPluginType::Hint typeHint = qtractorPluginType::Any)
		: m_typeHint(typeHint) {}

	// Destructor.
	~qtractorPluginPath()
		{ close(); clear(); }

	// Type-hint accessors...
	void setTypeHint(qtractorPluginType::Hint typeHint)
		{ m_typeHint = typeHint; m_paths.clear(); }
	qtractorPluginType::Hint typeHint() const { return m_typeHint; }

	// Main properties accessors.
	void setPaths(const QString& sPaths);
	void setPaths(const QStringList& paths)
		{ m_paths = paths; }
	const QStringList& paths() const { return m_paths; }

	// Executive methods.
	bool open();
	void close();

	// Plugin files list.
	const QList<qtractorPluginFile *>& files() const { return m_files; }

	// Plugin types list accessor.
	const QList<qtractorPluginType *>& types() const { return m_types; }

	// Plugin type adder.
	void addType(qtractorPluginType *pType)	{ m_types.append(pType); }

	// Type list reset method.
	void clear() { qDeleteAll(m_types); m_types.clear(); }

private:

	// Instance variables.
	qtractorPluginType::Hint m_typeHint;

	QStringList m_paths;
	
	// Internal plugin file/type list.
	QList<qtractorPluginFile *> m_files;
	QList<qtractorPluginType *> m_types;
};


//----------------------------------------------------------------------------
// qtractorPluginParam -- Plugin paramater (control input port) instance.
//

class qtractorPluginParam
{
public:

	// Constructor.
	qtractorPluginParam(qtractorPlugin *pPlugin, unsigned long iIndex)
		: m_pPlugin(pPlugin), m_iIndex(iIndex),
			m_fDefaultValue(0.0f), m_subject(0.0f),
			m_observer(&m_subject, this) {}

	// Main properties accessors.
	qtractorPlugin *plugin() const { return m_pPlugin; }
	unsigned long   index()  const { return m_iIndex;  }

	// Parameter name accessors.
	void setName(const QString& sName)
		{ m_observer.setName(sName); }
	const QString& name() const
		{ return m_observer.name(); }

	// Parameter range hints predicate methods.
	virtual bool isBoundedBelow() const = 0;
	virtual bool isBoundedAbove() const = 0;
	virtual bool isDefaultValue() const = 0;
	virtual bool isLogarithmic()  const = 0;
	virtual bool isSampleRate()   const = 0;
	virtual bool isInteger()      const = 0;
	virtual bool isToggled()      const = 0;
	virtual bool isDisplay()      const = 0;

	// Current display value.
	virtual QString display() const
		{ return QString::number(value()); }
	
	// Bounding range values.
	void setMinValue(float fMinValue)
		{ m_observer.setMinValue(fMinValue); }
	float minValue() const
		{ return m_observer.minValue(); }

	void setMaxValue(float fMaxValue)
		{ m_observer.setMaxValue(fMaxValue); }
	float maxValue() const
		{ return m_observer.maxValue(); }
	
	// Default value
	void setDefaultValue(float fDefaultValue);
	float defaultValue() const
		{ return m_fDefaultValue; }
	
	//------------------------------------------------------------------------
	// Observer -- Local dedicated observer.
	
	class Observer : public qtractorMidiControlObserver
	{
	public:
	
		// Constructor.
		Observer(qtractorSubject *pSubject, qtractorPluginParam *pParam)
			: qtractorMidiControlObserver(pSubject), m_pParam(pParam) {}
	
	protected:
	
		// Update feedback.
		void update()
		{
			m_pParam->updateValue(value(), true);
			qtractorMidiControlObserver::update();
		}
	
	private:
	
		// Members.
		qtractorPluginParam *m_pParam;
	};

	// Current parameter value.
	void setValue(float fValue, bool bUpdate);
	float value() const
		{ return m_observer.value(); }
	float prevValue() const
		{ return m_observer.prevValue(); }

	// Parameter update method.
	void updateValue(float fValue, bool bUpdate);

	// Reset-to-default method.
	void reset() { setValue(m_fDefaultValue, true); }

	// Direct parameter subject value.
	qtractorSubject *subject() { return &m_subject; }

	// Specialized observer value.
	Observer *observer() { return &m_observer; }

private:

	// Instance variables.
	qtractorPlugin *m_pPlugin;
	unsigned long m_iIndex;

	// Port default value.
	float m_fDefaultValue;
	
	// Port subject value.
	qtractorSubject m_subject;

	// Port observer manager.
	Observer m_observer;
};


//----------------------------------------------------------------------------
// qtractorPlugin -- Plugin instance.
//

class qtractorPlugin : public qtractorList<qtractorPlugin>::Link
{
public:

	// Constructors.
	qtractorPlugin(qtractorPluginList *pList, qtractorPluginType *pType);

	// Destructor.
	virtual ~qtractorPlugin();

	// DANGER: This one should be use by qtractorPluginList class!
	void setPluginList(qtractorPluginList *pList) { m_pList = pList; }

	// Main properties accessors.
	qtractorPluginList *list() const { return m_pList; }
	qtractorPluginType *type() const { return m_pType; }
	unsigned short instances() const { return m_iInstances; }

	// Chain helper ones.
	unsigned int sampleRate() const;
	unsigned int bufferSize() const;
	unsigned short channels() const;
	bool isMidi() const;
	
	// Activation methods.
	void setActivated(bool bActivated);
	bool isActivated() const
		{ return m_bActivated; }

	// Parameter list accessor.
	void addParam(qtractorPluginParam *pParam)
		{ m_params.insert(pParam->index(), pParam); }

	// An accessible list of parameters.
	typedef QHash<unsigned long, qtractorPluginParam *> Params;
	const Params& params() const
		{ return m_params; }

	// Instance capped number of audio ports.
	unsigned short audioIns() const
		{ return m_pType->audioIns(); }
	unsigned short audioOuts() const
		{ return m_pType->audioOuts(); }

	// Plugin state serialization methods.
	void setValueList(const QStringList& vlist);
	QStringList valueList() const;

	// Reset-to-default method.
	void reset();

	// Channel/instance number settler.
	virtual void setChannels(unsigned short iChannels) = 0;

	// Do the actual (de)activation.
	virtual void activate()   = 0;
	virtual void deactivate() = 0;

	// The main plugin processing procedure.
	virtual void process(
		float **ppIBuffer, float **ppOBuffer, unsigned int nframes) = 0;

	// Parameter update method.
	virtual void updateParam(
		qtractorPluginParam */*pParam*/, float /*fValue*/) {}

	// Specific MIDI instrument selector.
	virtual void selectProgram(int /*iBank*/, int /*iProg*/) {}

	// Program (patch) descriptor.
	struct Program
	{
		int     bank;
		int     prog;
		QString name;
	};

	// Provisional program/patch accessor.
	virtual bool getProgram(int /*iIndex*/, Program& /*program*/) const
		{ return false; }

	// MIDI continuous controller handler.
	virtual void setController(int /*iController*/, int /*iValue*/) {}

	// Plugin configuration handlers.
	virtual void configure(
		const QString& /*sKey*/, const QString& /*sValue*/) {}

	// Plugin configuration/state snapshot.
	virtual void freezeConfigs() {}
	virtual void releaseConfigs() {}

	// GUI Editor stuff.
	virtual void openEditor(QWidget */*pParent*/) {}
	virtual void closeEditor() {};
	virtual void idleEditor()  {};

	// GUI editor visibility state.
	virtual void setEditorVisible(bool /*bVisible*/) {}
	virtual bool isEditorVisible() const
		{ return false; }

	virtual void setEditorTitle(const QString& sTitle)
		{ m_sEditorTitle = sTitle; }
	const QString& editorTitle() const
		{ return m_sEditorTitle; }

	// An accessible list of observers.
	const QList<qtractorPluginListItem *>& items() const
		{ return m_items; }

	// List of observers management.
	void addItem(qtractorPluginListItem *pItem);
	void removeItem(qtractorPluginListItem *pItem);
	void clearItems();

	// Special plugin form accessors.
	bool isFormVisible() const;
	qtractorPluginForm *form();

	// Plugin default preset name accessor (informational)
	void setPreset(const QString& sPreset);
	const QString& preset() const;

	// Plugin preset group - common identification prefix.
	QString presetGroup() const;
	QString presetPrefix() const;

	// Plugin configuration from/to xml file.
	bool loadPreset(const QString& sFilename);
	bool savePreset(const QString& sFilename);

	// Plugin parameter lookup.
	qtractorPluginParam *findParam(unsigned long iIndex) const;

	// Plugin configuration (CLOB) stuff.
	typedef QHash<QString, QString> Configs;

	void setConfigs(const Configs& configs)
		{ m_configs = configs; }
	const Configs& configs() const
		{ return m_configs; }

	void setConfig(const QString& sKey, const QString& sValue)
		{ m_configs[sKey] = sValue; }
	QString config(const QString& sKey) const
		{ return m_configs[sKey]; }

	// Plugin parameter values stuff.
	typedef QHash<unsigned long, float> Values;

	// Plugin parameter/state snapshot.
	void freezeValues();
	void releaseValues();

	void setValues(const Values& values)
		{ m_values = values; }
	const Values& values() const
		{ return m_values; }

	void setValue(unsigned long iIndex, float fValue)
		{ m_values[iIndex] = fValue; }
	float value(unsigned long iIndex) const
		{ return m_values[iIndex]; }

	// Plugin configure realization.
	void realizeConfigs();
	void realizeValues();

	// Plugin configure clearance.
	void clearConfigs()
		{ m_configs.clear(); }
	void clearValues()
		{ m_values.clear(); }

	// Load plugin configuration/parameter values stuff.
	static void loadConfigs(QDomElement *pElement, Configs& configs);
	static void loadValues(QDomElement *pElement, Values& values);

	// Save plugin configuration/parameter values stuff.
	void saveConfigs(QDomDocument *pDocument, QDomElement *pElement);
	void saveValues(QDomDocument *pDocument, QDomElement *pElement);

	// Parameter controllers (MIDI).
	struct Controller
	{
		unsigned long index;
		qtractorMidiControl::ControlType ctype;
		unsigned short channel;
		unsigned short param;
		bool logarithmic;
		bool feedback;
	};

	typedef QList<Controller *> Controllers;

	// Load plugin parameter controllers (MIDI).
	static void loadControllers(QDomElement *pElement, Controllers& controllers);

	// Save plugin parameter controllers (MIDI).
	void saveControllers(qtractorSessionDocument *pDocument, QDomElement *pElement);

	// Map/realize plugin parameter controllers (MIDI).
	void mapControllers(Controllers& controllers);

protected:

	// Instance number settler.
	void setInstances(unsigned short iInstances);

private:

	// Instance variables.
	qtractorPluginList *m_pList;
	qtractorPluginType *m_pType;

	unsigned short m_iInstances;

	bool m_bActivated;

	// List of input control ports (parameters).
	Params m_params;

	// An accessible list of observers.
	QList<qtractorPluginListItem *> m_items;

	// The plugin form reference.
	qtractorPluginForm *m_pForm;
	QString m_sPreset;

	// GUI editor stuff.
	QString m_sEditorTitle;

	// Plugin configuration (CLOB) stuff.
	Configs m_configs;

	// Plugin parameter values (part of configuration).
	Values m_values;
};


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

class qtractorPluginList : public qtractorList<qtractorPlugin>
{
public:

	// Plugin-list type flags.
	enum Flags {
		// Basic flags.
		Audio = 0, Midi = 1,
		Track = 0, Bus  = 2,
		Out   = 0, In   = 4,
		// Composite helper flags.
		AudioTrack  = Audio | Track,
		AudioBus    = Audio | Bus,
		AudioInBus  = Audio | Bus | In,
		AudioOutBus = Audio | Bus | Out,
		MidiTrack   = Midi  | Track,
		MidiBus     = Midi  | Bus, 
		MidiInBus   = Midi  | Bus | In,
		MidiOutBus  = Midi  | Bus | Out
	};

	// Constructor.
	qtractorPluginList(
		unsigned short iChannels,
		unsigned int iBufferSize,
		unsigned int iSampleRate,
		unsigned int iFlags);

	// Destructor.
	~qtractorPluginList();

	// The title to show up on plugin forms...
	void setName(const QString& sName);
	const QString& name() const
		{ return m_sName; }

	// Main-parameters accessor.
	void setBuffer(
		unsigned short iChannels,
		unsigned int iBufferSize,
		unsigned int iSampleRate,
		unsigned int iFlags);

	// Reset and (re)activate all plugin chain.
	void resetBuffer();

	// Brainless accessors.
	unsigned short channels() const { return m_iChannels;   }
	unsigned int sampleRate() const { return m_iSampleRate; }
	unsigned int bufferSize() const { return m_iBufferSize; }
	unsigned int flags()      const { return m_iFlags; }

	// Specific MIDI manager accessor.
	qtractorMidiManager *midiManager() const
		{ return m_pMidiManager; }

	// Special activation methods.
	bool isActivatedAll() const;
	void updateActivated(bool bActivated);

	unsigned int activated() const
		{ return m_iActivated;  }

	// Guarded plugin methods.
	void addPlugin(qtractorPlugin *pPlugin);
	void insertPlugin(qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin);
	void movePlugin(qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin);
	void removePlugin(qtractorPlugin *pPlugin);

	// Clone/copy plugin method.
	qtractorPlugin *copyPlugin(qtractorPlugin *pPlugin);

	// An accessible list of views.
	const QList<qtractorPluginListView *>& views() const
		{ return m_views; }

	// list of views management.
	void addView(qtractorPluginListView *pView);
	void removeView(qtractorPluginListView *pView);

	// The meta-main audio-processing plugin-chain procedure.
	void process(float **ppBuffer, unsigned int nframes);

	// Document element methods.
	bool loadElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);
	bool saveElement(qtractorSessionDocument *pDocument,
		QDomElement *pElement);

	// MIDI manager deferred load/cache specifics.
	void setMidiBank(int iMidiBank)
		{ m_iMidiBank = iMidiBank; }
	int midiBank() const
		{ return m_iMidiBank; }

	void setMidiProg(int iMidiProg)
		{ m_iMidiProg = iMidiProg; }
	int midiProg() const
		{ return m_iMidiProg; }
	
	void setAudioOutputBus(bool bAudioOutputBus)
		{ m_bAudioOutputBus = bAudioOutputBus; }
	bool isAudioOutputBus() const
		{ return m_bAudioOutputBus; }

private:

	// Instance variables.
	unsigned short m_iChannels;
	unsigned int   m_iBufferSize;
	unsigned int   m_iSampleRate;
	unsigned int   m_iFlags;

	// Activation state.
	unsigned int   m_iActivated;

	// Plugin-chain name.
	QString m_sName;

	// An accessible list of observers.
	QList<qtractorPluginListView *> m_views;

	// Specific MIDI manager.
	qtractorMidiManager *m_pMidiManager;

	int  m_iMidiBank;
	int  m_iMidiProg;

	bool m_bAudioOutputBus;

	qtractorBus::ConnectList m_audioOutputs;

	// Internal running buffer chain references.
	float **m_pppBuffers[2];
};


#endif  // __qtractorPlugin_h

// end of qtractorPlugin.h
