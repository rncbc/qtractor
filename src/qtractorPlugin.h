// qtractorPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorList.h"

#include <QStringList>
#include <QLibrary>
#include <QSize>


// Forward declarations.
class qtractorPluginFile;
class qtractorPluginList;
class qtractorPluginParam;
class qtractorPluginForm;
class qtractorPlugin;

class qtractorPluginListView;
class qtractorPluginListItem;

class qtractorSessionDocument;
class QDomElement;


//----------------------------------------------------------------------------
// qtractorPluginType -- Plugin type instance.
//

class qtractorPluginType
{
public:

	// Have hints for plugin paths.
	enum Hint { Any = 0, Ladspa, Dssi, Vst };

	// Constructor.
	qtractorPluginType(qtractorPluginFile *pFile, unsigned long iIndex,
		Hint typeHint) : m_iUniqueID(0), m_iControlIns(0), m_iControlOuts(0),
			m_iAudioIns(0), m_iAudioOuts(0), m_iMidiIns(0), m_iMidiOuts(0),
			m_bRealtime(false), m_bEditor(false),
			m_pFile(pFile), m_iIndex(iIndex), m_typeHint(typeHint) {}

	// Destructor (virtual)
	virtual ~qtractorPluginType() {}

	// Main properties accessors.
	qtractorPluginFile *file() const { return m_pFile; }
	unsigned long index() const { return m_iIndex; }
	Hint typeHint() const { return m_typeHint; }

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
	bool m_bEditor;

private:

	// Instance variables.
	qtractorPluginFile *m_pFile;
	unsigned long m_iIndex;
	Hint m_typeHint;
};


//----------------------------------------------------------------------------
// qtractorPluginTypeList -- Plugin type list instance.
//

class qtractorPluginTypeList
{
public:

	// Constructor.
	qtractorPluginTypeList() {}
	// Destructor
	~qtractorPluginTypeList()
		{ clear(); }

	// Simple list management method.
	void append(qtractorPluginType *pType)
		{ m_list.append(pType); }

	// List reset method.
	void clear() { qDeleteAll(m_list); m_list.clear(); }

	// List contents predicate.
	bool isEmpty() const { return m_list.isEmpty(); }

	// List accessor.
	const QList<qtractorPluginType *>& list() const { return m_list; }

private:

	// Instance variables (just the list)
	QList<qtractorPluginType *> m_list;
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
	bool getTypes(qtractorPluginTypeList& types,
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
		{ close(); }

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

	// Plugin file list.
	const QList<qtractorPluginFile *>& files() const { return m_files; }

private:

	// Instance variables.
	qtractorPluginType::Hint m_typeHint;

	QStringList m_paths;
	
	// Internal plugin file list.
	QList<qtractorPluginFile *> m_files;
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
	bool isActivated() const { return m_bActivated; }

	// Parameter list accessor.
	void addParam(qtractorPluginParam *pParam)
		{ m_params.append(pParam); }

	// An accessible list of parameters.
	const QList<qtractorPluginParam *>& params() const { return m_params; }

	// Plugin state serialization methods.
	void setValues(const QStringList& vlist);
	QStringList values() const;

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

	// GUI Editor stuff.
	virtual void openEditor(QWidget */*pParent*/) {}
	virtual void closeEditor() {};
	virtual void idleEditor()  {};

	virtual void setEditorTitle(const QString& sTitle)
		{ m_sEditorTitle = sTitle; }
	const QString& editorTitle() const
		{ return m_sEditorTitle; }

	bool isEditorVisible() const
		{ return m_bEditorVisible; }

	// An accessible list of observers.
	const QList<qtractorPluginListItem *>& items() const { return m_items; }

	// List of observers management.
	void addItem(qtractorPluginListItem *pItem);
	void removeItem(qtractorPluginListItem *pItem);
	void clearItems();

	// Special plugin form accessors.
	bool isVisible() const;
	qtractorPluginForm *form();

	// Plugin default preset name accessor (informational)
	void setPreset(const QString& sName);
	const QString& preset();

	// Plugin preset group - common identification prefix.
	QString presetGroup() const;

	// Plugin parameter lookup.
	qtractorPluginParam *findParam(unsigned long iIndex) const;

protected:

	// Instance number settler.
	void setInstances(unsigned short iInstances);

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible)
		{ m_bEditorVisible = bVisible; }

	// Instance capped number of audio ports.
	unsigned short audioInsCap() const
		{ return m_iAudioInsCap; }
	unsigned short audioOutsCap() const
		{ return m_iAudioOutsCap; }
	
private:

	// Instance variables.
	qtractorPluginList *m_pList;
	qtractorPluginType *m_pType;

	unsigned short m_iInstances;

	unsigned short m_iAudioInsCap;
	unsigned short m_iAudioOutsCap;

	bool m_bActivated;

	// List of input control ports (parameters).
	QList<qtractorPluginParam *> m_params;

	// An accessible list of observers.
	QList<qtractorPluginListItem *> m_items;

	// The plugin form reference.
	qtractorPluginForm *m_pForm;
	QString m_sPreset;

	// GUI editor stuff.
	QString m_sEditorTitle;
	bool    m_bEditorVisible;
};


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

class qtractorPluginList : public qtractorList<qtractorPlugin>
{
public:

	// Constructor.
	qtractorPluginList(
		unsigned short iChannels,
		unsigned int iBufferSize,
		unsigned int iSampleRate,
		bool bMidi = false);

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
		bool bMidi = false);

	// Reset and (re)activate all plugin chain.
	void resetBuffer();

	// Brainless accessors.
	unsigned short channels() const { return m_iChannels;   }
	unsigned int sampleRate() const { return m_iSampleRate; }
	unsigned int bufferSize() const { return m_iBufferSize; }
	bool         isMidi()     const { return m_bMidi; }

	// Special activation methods.
	unsigned int activated() const  { return m_iActivated;  }
	bool isActivatedAll() const;
	void updateActivated(bool bActivated);

	// Guarded plugin methods.
	void addPlugin(qtractorPlugin *pPlugin);
	void insertPlugin(qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin);
	void movePlugin(qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin);
	void removePlugin(qtractorPlugin *pPlugin);

	// Clone/copy plugin method.
	qtractorPlugin *copyPlugin(qtractorPlugin *pPlugin);

	// An accessible list of views.
	const QList<qtractorPluginListView *>& views() const { return m_views; }

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

private:

	// Instance variables.
	unsigned short m_iChannels;
	unsigned int   m_iBufferSize;
	unsigned int   m_iSampleRate;
	bool           m_bMidi;

	// Activation state.
	unsigned int   m_iActivated;

	// Plugin-chain name.
	QString m_sName;

	// An accessible list of observers.
	QList<qtractorPluginListView *> m_views;

	// Internal running buffer chain references.
	float **m_pppBuffers[2];
};


//----------------------------------------------------------------------------
// qtractorPluginParam -- Plugin paramater (control input port) instance.
//

class qtractorPluginParam
{
public:

	// Constructors.
	qtractorPluginParam(qtractorPlugin *pPlugin, unsigned long iIndex)
		: m_pPlugin(pPlugin), m_iIndex(iIndex),
			m_fMinValue(0.0f), m_fMaxValue(0.0f),
			m_fDefaultValue(0.0f), m_fValue(0.0f) {}

	// Destructor.
	virtual ~qtractorPluginParam() {}

	// Main properties accessors.
	qtractorPlugin *plugin() const { return m_pPlugin; }
	unsigned long   index()  const { return m_iIndex;  }

	// Parameter name accessors.
	void setName(const QString& sName)
		{ m_sName = sName.trimmed(); }
	const QString& name() const
		{ return m_sName; }

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
		{ m_fMinValue = fMinValue; }
	float minValue() const
		{ return m_fMinValue; }

	void setMaxValue(float fMaxValue)
		{ m_fMaxValue = fMaxValue; }
	float maxValue() const
		{ return m_fMaxValue; }
	
	// Default value
	void setDefaultValue(float fDefaultValue);
	float defaultValue() const
		{ return m_fDefaultValue; }
	
	// Current parameter value.
	virtual void setValue(float fValue);
	virtual float value() const
		{ return m_fValue; }

	// Reset-to-default method.
	virtual void reset()
		{ setValue(m_fDefaultValue); }

	// Direct parameter value.
	float *data() { return &m_fValue; }

private:

	// Instance variables.
	qtractorPlugin *m_pPlugin;
	unsigned long m_iIndex;

	// Parameter name/label.
	QString m_sName;

	// Port values.
	float m_fMinValue;
	float m_fMaxValue;
	float m_fDefaultValue;
	float m_fValue;
};


#endif  // __qtractorPlugin_h

// end of qtractorPlugin.h
