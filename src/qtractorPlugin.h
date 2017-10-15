// qtractorPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QLibrary>

#include <QStringList>
#include <QPoint>
#include <QSize>

#include <QMap>


// Forward declarations.
class qtractorPluginList;
class qtractorPluginParam;
class qtractorPluginForm;
class qtractorPlugin;

class qtractorPluginListView;
class qtractorPluginListItem;

class qtractorMidiManager;

class qtractorDocument;
class qtractorCurveList;
class qtractorCurveFile;

class QDomDocument;
class QDomElement;


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

class qtractorPluginFile : public QLibrary
{
public:

	// Constructor.
	qtractorPluginFile(const QString& sFilename, bool bAutoUnload = true)
		: QLibrary(sFilename), m_bAutoUnload(bAutoUnload),
			m_iOpenCount(0), m_iRefCount(0) {}

	// Destructor.
	~qtractorPluginFile()
		{ close(); }

	// Helper property accessors.
	QString filename() const { return QLibrary::fileName(); }

	// Executive methods.
	bool open();
	void close();

	// Auto-unload flag accessors.
	void setAutoUnload(bool bAutoUnload)
		{ m_bAutoUnload = bAutoUnload; }
	bool isAutoUnload() const
		{ return m_bAutoUnload; }

	// Ref-counting methods.
	void addRef()
		{ ++m_iRefCount; }
	bool removeRef()
		{ return (--m_iRefCount < 1); }

	// Plugin file resgistry.
	typedef QHash<QString, qtractorPluginFile *> Files;

	// Plugin file resgistry methods.
	static qtractorPluginFile *addFile(const QString& sFilename);
	static void removeFile(qtractorPluginFile *pFile);

private:

	// Instance variables.
	bool m_bAutoUnload;

	unsigned int m_iOpenCount;
	unsigned int m_iRefCount;

	// Global plugin-files.
	static Files g_files;
};


//----------------------------------------------------------------------------
// qtractorPluginType -- Plugin type instance.
//

class qtractorPluginType
{
public:

	// Have hints for plugin paths.
	enum Hint { Any = 0, Ladspa, Dssi, Vst, Lv2, Insert, AuxSend };

	// Constructor.
	qtractorPluginType(qtractorPluginFile *pFile, unsigned long iIndex,
		Hint typeHint) : m_iUniqueID(0), m_iControlIns(0), m_iControlOuts(0),
			m_iAudioIns(0), m_iAudioOuts(0), m_iMidiIns(0), m_iMidiOuts(0),
			m_bRealtime(false), m_bConfigure(false), m_bEditor(false),
			m_pFile(pFile), m_iIndex(iIndex), m_typeHint(typeHint) {}

	// Destructor (virtual)
	virtual ~qtractorPluginType()
		{ qtractorPluginFile::removeFile(m_pFile); }

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
	const QString& name()  const { return m_sName;  }
	const QString& label() const { return m_sLabel; }

	unsigned long uniqueID() const { return m_iUniqueID; }

	// Port count accessors..
	unsigned short controlIns()  const { return m_iControlIns;  }
	unsigned short controlOuts() const { return m_iControlOuts; }
	unsigned short audioIns()    const { return m_iAudioIns;    }
	unsigned short audioOuts()   const { return m_iAudioOuts;   }
	unsigned short midiIns()     const { return m_iMidiIns;     }
	unsigned short midiOuts()    const { return m_iMidiOuts;    }

	// Attribute accessors.
	bool isRealtime()  const { return m_bRealtime;  }
	bool isConfigure() const { return m_bConfigure; }
	bool isEditor()    const { return m_bEditor;    }

	bool isMidi() const { return m_iMidiIns + m_iMidiOuts > 0; }

	// Compute the number of instances needed
	// for the given input/output audio channels.
	virtual unsigned short instances(
		unsigned short iChannels, bool bMidi) const;

	// Plugin factory method.
	static qtractorPlugin *createPlugin(qtractorPluginList *pList,
		const QString& sFilename, unsigned long iIndex, Hint typeHint);

	// Plugin type(hint) textual helpers.
	static Hint hintFromText(const QString& sText);
	static QString textFromHint(Hint typeHint);

	// Instance cached-deferred methods.
	virtual const QString& aboutText() { return m_sAboutText; }

protected:

	// Cached name strings.
	QString m_sName;
	QString m_sLabel;

	// Cache unique identifier.
	unsigned long m_iUniqueID;

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

	// Instance cached-deferred variables.
	QString m_sAboutText;

private:

	// Instance variables.
	qtractorPluginFile *m_pFile;
	unsigned long m_iIndex;
	Hint m_typeHint;
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
			m_subject(0.0f), m_observer(this), m_iDecimals(-1) {}

	// Virtual destructor.
	virtual ~qtractorPluginParam() {}

	// Main properties accessors.
	qtractorPlugin *plugin() const { return m_pPlugin; }
	unsigned long   index()  const { return m_iIndex;  }

	// Parameter name accessors.
	void setName(const QString& sName)
		{ m_subject.setName(sName); }
	const QString& name() const
		{ return m_subject.name(); }

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
		{ return QString::number(value(), 'f', decimals()); }
	
	// Bounding range values.
	void setMinValue(float fMinValue)
		{ m_subject.setMinValue(fMinValue); }
	float minValue() const
		{ return m_subject.minValue(); }

	void setMaxValue(float fMaxValue)
		{ m_subject.setMaxValue(fMaxValue); }
	float maxValue() const
		{ return m_subject.maxValue(); }
	
	// Default value
	void setDefaultValue(float fDefaultValue)
		{ m_subject.setDefaultValue(fDefaultValue); }
	float defaultValue() const
		{ return m_subject.defaultValue(); }
	
	// Current parameter value.
	void setValue(float fValue, bool bUpdate);
	float value() const
		{ return m_observer.value(); }
	float prevValue() const
		{ return m_observer.prevValue(); }

	// Parameter update method.
	void updateValue(float fValue, bool bUpdate);

	// Reset-to-default method.
	void reset() { setValue(defaultValue(), true); }

	// Direct parameter subject value.
	qtractorSubject *subject() { return &m_subject; }

	// Specialized observer value.
	qtractorMidiControlObserver *observer() { return &m_observer; }

	// Parameter decimals helper (cached).
	int decimals() const
		{ return m_iDecimals; }

private:

	// Instance variables.
	qtractorPlugin *m_pPlugin;
	unsigned long   m_iIndex;

	// Port subject value.
	qtractorSubject m_subject;

	// Port observer manager.
	class Observer : public qtractorMidiControlObserver
	{
	public:

		// Constructor.
		Observer(qtractorPluginParam *pParam);

	protected:

		// Virtual observer updater.
		void update(bool bUpdate);

	private:

		// Instance members.
		qtractorPluginParam *m_pParam;

	} m_observer;

	// Decimals cache.
	int m_iDecimals;
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

	// DANGER: This one should be used by qtractorPluginList class only!
	void setPluginList(qtractorPluginList *pList) { m_pList = pList; }

	// Main properties accessors.
	qtractorPluginList *list() const { return m_pList; }
	qtractorPluginType *type() const { return m_pType; }

	unsigned short instances() const { return m_iInstances; }

	// Chain helper ones.
	unsigned short channels() const;

	// Unique ID methods.
	void setUniqueID(unsigned long iUniqueID)
		{ m_iUniqueID = iUniqueID; }
	unsigned long uniqueID() const
		{ return m_iUniqueID; }

	// Activation additional info sender supplies.
	enum ActivationInfoType { Default, Create, Copy, SaveSession };

	// Activation methods.
	void setActivated(bool bActivated, ActivationInfoType info = Default);
	void setActivatedEx(bool bActivated);
	bool isActivated(ActivationInfoType info = Default) const;

	// Activate subject accessors.
	qtractorSubject *activateSubject()
		{ return &m_activateSubject; }
	qtractorMidiControlObserver *activateObserver()
		{ return &m_activateObserver; }

	// Activate pseudo-parameter port index.
	void setActivateSubjectIndex (unsigned long iIndex)
		{ m_iActivateSubjectIndex = iIndex; }
	unsigned long activateSubjectIndex() const
		{ return m_iActivateSubjectIndex; }

	// An accessible list of parameters.
	typedef QMap<unsigned long, qtractorPluginParam *> Params;
	const Params& params() const
		{ return m_params; }

	typedef QHash<QString, qtractorPluginParam *> ParamNames;
	const ParamNames& paramNames() const
		{ return m_paramNames; }

	// Parameter list accessor.
	void addParam(qtractorPluginParam *pParam)
	{
		pParam->reset();
		if (pParam->isLogarithmic())
			pParam->observer()->setLogarithmic(true);
		m_params.insert(pParam->index(), pParam);
		m_paramNames.insert(pParam->name(), pParam);
	}

	// Instance capped number of audio ports.
	unsigned short audioIns() const
		{ return m_pType->audioIns(); }
	unsigned short audioOuts() const
		{ return m_pType->audioOuts(); }

	// Instance capped number of MIDI ports.
	unsigned short midiIns() const
		{ return m_pType->midiIns(); }
	unsigned short midiOuts() const
		{ return m_pType->midiOuts(); }

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
		qtractorPluginParam */*pParam*/, float /*fValue*/, bool /*bUpdate*/) {}

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
	virtual void freezeConfigs()  {}
	virtual void releaseConfigs() {}

	// Plugin configure realization.
	virtual void realizeConfigs();

	// Plugin configure clearance/release.
	virtual void clearConfigs() { m_configs.clear(); m_ctypes.clear(); }

	// GUI Editor stuff.
	virtual void openEditor(QWidget */*pParent*/= NULL) {}
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

	void updateEditorTitle();

	void setEditorPos(const QPoint& pos)
		{ m_posEditor = pos; }
	const QPoint& editorPos() const
		{ return m_posEditor; }

	// Move widget to alleged parent center or else...
	void moveWidgetPos(QWidget *pWidget, const QPoint& wpos) const;

	// Custom GUI editor type index preference.
	void setEditorType(int iEditorType)
		{ m_iEditorType = iEditorType; }
	int editorType() const
		{ return m_iEditorType; }

	// An accessible list of observers.
	const QList<qtractorPluginListItem *>& items() const
		{ return m_items; }

	// List of observers management.
	void addItem(qtractorPluginListItem *pItem);
	void removeItem(qtractorPluginListItem *pItem);
	void clearItems();

	// Special plugin form methods.
	void openForm(QWidget *pParent = NULL);
	void closeForm();

	bool isFormVisible() const;

	void toggleFormEditor(bool bOn);
	void updateFormDirtyCount();
	void updateFormAuxSendBusName();
	void refreshForm();

	void freezeFormPos();

	void setFormPos(const QPoint& pos)
		{ m_posForm = pos; }
	const QPoint& formPos() const
		{ return m_posForm; }

	// Provisional preset accessors.
	virtual QStringList presetList() const;

	virtual bool loadPreset(const QString& /*sPreset*/)
		{ return false; }
	virtual bool savePreset(const QString& /*sPreset*/)
		{ return false; }
	virtual bool deletePreset(const QString& /*sPreset*/)
		{ return false; }

	virtual bool isReadOnlyPreset(const QString& /*sPreset*/) const
		{ return false; }

	// Plugin default preset name accessor (informational)
	void setPreset(const QString& sPreset);
	const QString& preset() const;

	// Plugin preset group - common identification prefix.
	QString presetGroup() const;
	QString presetPrefix() const;

	// Default preset name (global).
	static const QString& defPreset()
		{ return g_sDefPreset; }

	// Plugin configuration from/to xml file.
	virtual bool loadPresetFile(const QString& sFilename);
	virtual bool savePresetFile(const QString& sFilename);

	// Load an existing preset by name/file.
	bool loadPresetEx(const QString& sPreset);
	bool loadPresetFileEx(const QString& sFilename);

	// Plugin parameter lookup.
	qtractorPluginParam *findParam(unsigned long iIndex) const;
	qtractorPluginParam *findParamName(const QString& sName) const;

	// Plugin configuration (CLOB) stuff.
	typedef QHash<QString, QString> Configs;

	void setConfigs(const Configs& configs)
		{ m_configs = configs; }
	const Configs& configs() const
		{ return m_configs; }

	void setConfig(const QString& sKey, const QString& sValue)
		{ m_configs[sKey] = sValue; }
	const QString& config(const QString& sKey)
		{ return m_configs[sKey]; }

	// Plugin configuration (types) stuff.
	typedef QHash<QString, QString> ConfigTypes;

	void setConfigTypes(const ConfigTypes& ctypes)
		{ m_ctypes = ctypes; }
	const ConfigTypes& configTypes() const
		{ return m_ctypes; }

	void setConfigType(const QString& sKey, const QString& sType)
		{ m_ctypes[sKey] = sType; }
	const QString& configType(const QString& sKey)
		{ return m_ctypes[sKey]; }

	// Plugin parameter values stuff.
	typedef QHash<unsigned long, QString> ValueNames;
	typedef QHash<unsigned long, float>   ValueIndex;

	typedef struct
	{
		ValueNames names;
		ValueIndex index;

	} Values;

	void setValues(const Values& values)
		{ m_values = values; }
	const Values& values() const
		{ return m_values; }

	// Plugin parameter/state snapshot.
	void freezeValues();
	void releaseValues();

	// Plugin parameter/state realization.
	void realizeValues();

	// Plugin parameter/state clearance.
	void clearValues() { m_values.names.clear(); m_values.index.clear(); }

	// Load plugin configuration/parameter values stuff.
	static void loadConfigs(
		QDomElement *pElement, Configs& configs, ConfigTypes& ctypes);
	static void loadValues(QDomElement *pElement, Values& values);

	// Save plugin configuration/parameter values stuff.
	void saveConfigs(QDomDocument *pDocument, QDomElement *pElement);
	void saveValues(QDomDocument *pDocument, QDomElement *pElement);

	// Load/save plugin parameter controllers (MIDI).
	static void loadControllers(
		QDomElement *pElement, qtractorMidiControl::Controllers& controllers);
	void saveControllers(
		qtractorDocument *pDocument, QDomElement *pElement);

	// Map/realize plugin parameter controllers (MIDI).
	void mapControllers(const qtractorMidiControl::Controllers& controllers);

	// Plugin automation curve serialization methods.
	static void loadCurveFile(
	    QDomElement *pElement, qtractorCurveFile *pCurveFile);
	void saveCurveFile(qtractorDocument *pDocument,
		QDomElement *pElement, qtractorCurveFile *pCurveFile);
	void applyCurveFile (qtractorCurveFile *pCurveFile);

	// Direct access parameter accessors.
	qtractorPluginParam *directAccessParam() const;
	void setDirectAccessParamIndex(long iDirectAccessParamIndex);
	long directAccessParamIndex() const;
	bool isDirectAccessParam() const;
	void updateDirectAccessParam();

	// Parameter update executive.
	void updateParamValue(unsigned long iIndex, float fValue, bool bUpdate);

	// Auto-plugin-deactivation
	void autoPluginDeactivate(bool bDeactivated);
	bool canBeConnectedToOtherTracks();
protected:

	// Instance number settler.
	void setInstances(unsigned short iInstances);

	// Activation stabilizers.
	void updateActivated(bool bActivated);
	void updateActivatedEx(bool bActivated);

private:

	// Instance variables.
	qtractorPluginList *m_pList;
	qtractorPluginType *m_pType;

	// Unique identifier in chain.
	unsigned long m_iUniqueID;

	// Number of instances in chain node.
	unsigned short m_iInstances;

	// Activation flag.
	bool m_bActivated;

	// Auto-plugin-deactivation flag
	bool m_bAutoDeactivated;

	// Activate subject value.
	qtractorSubject m_activateSubject;

	// Activate observer manager.
	class ActivateObserver : public qtractorMidiControlObserver
	{
	public:

		// Constructor.
		ActivateObserver(qtractorPlugin *pPlugin);

	protected:

		// Virtual observer updater.
		void update(bool bUpdate);

	private:

		// Instance members.
		qtractorPlugin *m_pPlugin;

	} m_activateObserver;

	// Activate pseudo-parameter port index.
	unsigned long m_iActivateSubjectIndex;

	// List of input control ports (parameters).
	Params m_params;

	// List of parameters (by name).
	ParamNames m_paramNames;

	// An accessible list of observers.
	QList<qtractorPluginListItem *> m_items;

	// The plugin form reference.
	qtractorPluginForm *m_pForm;

	QString m_sPreset;
	QPoint  m_posForm;

	// GUI editor stuff.
	QString m_sEditorTitle;
	QPoint  m_posEditor;

	// Custom GUI editor type index preference.
	int m_iEditorType;

	// Plugin configuration (CLOB) stuff.
	Configs m_configs;

	// Plugin configuration (type) stuff.
	ConfigTypes m_ctypes;

	// Plugin parameter values (part of configuration).
	Values m_values;

	// Direct access parameter, if any.
	long m_iDirectAccessParamIndex;

	// Default preset name.
	static QString g_sDefPreset;
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
	qtractorPluginList(unsigned short iChannels, unsigned int iFlags);

	// Destructor.
	~qtractorPluginList();

	// The title to show up on plugin forms...
	void setName(const QString& sName);
	const QString& name() const
		{ return m_sName; }

	// Main-parameters accessors.
	void setChannels(unsigned short iChannels, unsigned int iFlags);
	void setChannelsEx(unsigned short iChannels, bool bReset = false);

	// Reset and (re)activate all plugin chain.
	void resetBuffers();

	// Brainless accessors.
	unsigned short channels() const { return m_iChannels; }
	unsigned int flags()      const { return m_iFlags; }

	// Brainless helpers.
	bool isMidi()    const { return (m_iFlags & Midi); }
	bool isMidiBus() const { return (m_iFlags & MidiBus) == MidiBus; }

	// Specific automation curve list accessor.
	qtractorCurveList *curveList() const
		{ return m_pCurveList; }

	// Specific MIDI manager accessor.
	qtractorMidiManager *midiManager() const
		{ return m_pMidiManager; }

	// Special activation methods.
	void updateActivated(bool bActivated)
	{
		if (bActivated)
			++m_iActivated;
		else
		if (m_iActivated > 0)
			--m_iActivated;
	}

	bool isActivatedAll() const
		{ return (m_iActivated > 0 && m_iActivated >= (unsigned int) count()); }
	unsigned int isActivated() const
		{ return (m_iActivated > 0);  }

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
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement);

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

	void setAudioOutputAutoConnect(bool bAudioOutputAutoConnect)
		{ m_bAudioOutputAutoConnect = bAudioOutputAutoConnect; }
	bool isAudioOutputAutoConnect() const
		{ return m_bAudioOutputAutoConnect; }

	void setAudioOutputBusName(const QString& sAudioOutputBusName)
		{ m_sAudioOutputBusName = sAudioOutputBusName; }
	const QString& audioOutputBusName() const
		{ return m_sAudioOutputBusName; }

	// MIDI bank/program observable subject.
	class MidiProgramSubject : public qtractorSubject
	{
	public:

		// Constructor.
		MidiProgramSubject(int iBank, int iProg)
			: qtractorSubject(valueFromProgram(iBank, iProg))
			{ setMaxValue(valueFromProgram(0x3fff, 0x7f)); }

		// Bank/program setter.
		void setProgram(int iBank, int iProg)
			{ setValue(valueFromProgram(iBank, iProg)); }

		// Bank/program getters.
		int bank() const { return bankFromValue(value()); }
		int prog() const { return progFromValue(value()); }

	protected:

		// Bank/program value helpers.
		static float valueFromProgram(int iBank, int iProg)
		{
			if (iBank < 0) iBank = 0;
			if (iProg < 0) iProg = 0;
			return float((iBank << 7) | (iProg & 0x7f));
		}

		static int bankFromValue(float fValue)
			{ return ((int(fValue) >> 7) & 0x3fff); }
		static int progFromValue(float fValue)
			{ return (int(fValue) & 0x7f); }
	};

	MidiProgramSubject *midiProgramSubject() const
		{ return m_pMidiProgramSubject; }

	// Acquire a unique plugin identifier in chain.
	unsigned long createUniqueID(qtractorPluginType *pType);

	// Whether unique plugin identifiers are in chain.
	bool isUniqueID(qtractorPluginType *pType) const;

	// Special audio inserts activation state methods.
	void setAudioInsertActivated(bool bAudioInsertActivated)
	{
		if (bAudioInsertActivated)
			++m_iAudioInsertActivated;
		else
		if (m_iAudioInsertActivated > 0)
			--m_iAudioInsertActivated;
	}

	bool isAudioInsertActivated() const
		{ return (m_iAudioInsertActivated > 0); }

	// Special auto-deactivate methods
	void autoPluginDeactivate(bool bDeactivated, bool bForce = false);
	bool isAutoDeactivated();

protected:

	// Check/sanitize plugin file-path.
	bool checkPluginFile(QString& sFilename,
		qtractorPluginType::Hint typeHint) const;

private:

	// Instance variables.
	unsigned short m_iChannels;
	unsigned int   m_iFlags;

	// Activation state.
	unsigned int   m_iActivated;

	// Plugin-chain name.
	QString m_sName;

	// An accessible list of observers.
	QList<qtractorPluginListView *> m_views;

	// Specific automation curve list accessor.
	qtractorCurveList *m_pCurveList;

	// Specific MIDI manager.
	qtractorMidiManager *m_pMidiManager;

	int  m_iMidiBank;
	int  m_iMidiProg;

	bool m_bAudioOutputBus;
	bool m_bAudioOutputAutoConnect;
	QString m_sAudioOutputBusName;

	qtractorBus::ConnectList m_audioOutputs;

	// Audio inserts activation state.
	unsigned int m_iAudioInsertActivated;

	// Internal running buffer chain references.
	float **m_pppBuffers[2];

	// MIDI bank/program observable subject.
	MidiProgramSubject *m_pMidiProgramSubject;

	// Plugin registry (chain unique ids.)
	QHash<unsigned long, unsigned int> m_uniqueIDs;

	// Auto-plugin-deactivation
	bool m_bAutoDeactivate;
};


#endif  // __qtractorPlugin_h

// end of qtractorPlugin.h
