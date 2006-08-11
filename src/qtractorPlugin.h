// qtractorPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorPlugin_h
#define __qtractorPlugin_h

#include "qtractorList.h"

#include <qstringlist.h>
#include <qptrlist.h>
#include <qlibrary.h>

#include <ladspa.h>

// Forward declarations.
class qtractorPluginFile;
class qtractorPluginType;
class qtractorPluginList;
class qtractorPluginPort;
class qtractorPluginForm;
class qtractorPlugin;

class qtractorPluginListItem;
class qtractorSessionDocument;

class QSettings;
class QDomElement;


//----------------------------------------------------------------------------
// qtractorPluginPath -- Plugin path helper.
//

class qtractorPluginPath
{
public:

	// Constructor.
	qtractorPluginPath(const QString& sPaths = QString::null);
	// Destructor.
	~qtractorPluginPath();

	// Main properties accessors.
	const QStringList& paths() const;

	// Executive methods.
	bool open();
	void close();

	// Plugin file list.
	QPtrList<qtractorPluginFile>& files();

private:

	// Instance variables.
	QStringList m_paths;
	
	// Internal plugin file list.
	QPtrList<qtractorPluginFile> m_files;
};


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

class qtractorPluginFile : public QLibrary
{
public:

	// Constructor.
	qtractorPluginFile(const QString& sFilename);
	// Destructor.
	~qtractorPluginFile();

	// Executive methods.
	bool open();
	void close();

	// Plugin type list.
	QPtrList<qtractorPluginType>& types();

	// Descriptor function method.
	const LADSPA_Descriptor *descriptor(unsigned long iIndex);

private:

	// The main descriptor function.
	LADSPA_Descriptor_Function m_pfnDescriptor;

	// Internal plugin type list.
	QPtrList<qtractorPluginType> m_types;
	
};


//----------------------------------------------------------------------------
// qtractorPluginType -- Plugin type instance.
//

class qtractorPluginType
{
public:

	// Constructor.
	qtractorPluginType(qtractorPluginFile *pFile, unsigned long iIndex);
	// Destructor.
	~qtractorPluginType();

	// Main properties accessors.
	qtractorPluginFile *file() const;
	unsigned long index() const;
	const LADSPA_Descriptor *descriptor() const;

	// Derived accessors.
	QString filename() const;
	const char *name() const;

private:

	// Instance variables.
	qtractorPluginFile *m_pFile;
	unsigned long m_iIndex;
	
	// Descriptor cache.
	const LADSPA_Descriptor *m_pDescriptor;
};


//----------------------------------------------------------------------------
// qtractorPlugin -- Plugin instance.
//

class qtractorPlugin : public qtractorList<qtractorPlugin>::Link
{
public:

	// Constructors.
	qtractorPlugin(qtractorPluginList *pList,
		qtractorPluginType *pType);
	qtractorPlugin(qtractorPluginList *pList,
		const QString& sFilename, unsigned long iIndex);

	// Destructor.
	~qtractorPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);
	unsigned short channels() const;

	// Main properties accessors.
	qtractorPluginList *list() const;
	qtractorPluginType *type() const;
	unsigned short instances() const;
	unsigned int sampleRate() const;
	LADSPA_Handle handle(unsigned short iInstance) const;
	
	// Derived accessors.
	qtractorPluginFile *file() const;
	unsigned long index() const;
	const LADSPA_Descriptor *descriptor() const;
	QString filename() const;
	const char *name() const;

	// Activation methods.
	void setActivated(bool bActivated);
	bool isActivated() const;

	// The main plugin processing procedure.
	void process(unsigned int nframes);

	// Input control ports list accessor.
	QPtrList<qtractorPluginPort>& cports();

	// Audio port index-list accessors.
	const QValueList<unsigned long>& iports() const;
	const QValueList<unsigned long>& oports() const;

	// An accessible list of observers.
	QPtrList<qtractorPluginListItem>& items();

	// Special plugin form accessors.
	bool isVisible() const;
	qtractorPluginForm *form();

	// Plugin state serialization methods.
	void setValues(const QStringList& vlist);
	QStringList values();

	// Preset name list
	QStringList presetList(QSettings& settings) const;

	// Plugin preset management methods.
	bool loadPreset(QSettings& settings, const QString& sPreset);
	bool savePreset(QSettings& settings, const QString& sPreset);
	bool deletePreset(QSettings& settings, const QString& sPreset) const;

	// Reset-to-default method.
	void reset();

protected:

	// Plugin initializer.
	void initPlugin(qtractorPluginList *pList,
		const QString& sFilename, unsigned long iIndex);

	// Plugin preset group - common identification prefix.
	QString presetGroup() const;

private:

	// Instance variables.
	qtractorPluginList *m_pList;
	qtractorPluginType *m_pType;
	unsigned short m_iInstances;
	unsigned int m_iSampleRate;

	LADSPA_Handle *m_phInstances;
	bool m_bActivated;

	// List of input control ports.
	QPtrList<qtractorPluginPort> m_cports;

	// List of audio port indexes.
	QValueList<unsigned long> m_iports;
	QValueList<unsigned long> m_oports;

	// An accessible list of observers.
	QPtrList<qtractorPluginListItem> m_items;

	// The plugin form reference.
	qtractorPluginForm *m_pForm;
};


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

class qtractorPluginList : public qtractorList<qtractorPlugin>
{
public:

	// Constructor.
	qtractorPluginList(unsigned short iChannels,
		unsigned int iBufferSize, unsigned int iSampleRate);
	// Destructor.
	~qtractorPluginList();

	// Main-parameters accessor.
	void setBuffer(unsigned short iChannels,
		unsigned int iBufferSize, unsigned int iSampleRate);

	// Brainless accessors.
	unsigned short channels() const;
	unsigned int sampleRate() const;
	unsigned int bufferSize() const;

	// Special activation methods.
	unsigned int activated() const;
	bool isActivatedAll() const;
	void updateActivated(bool bActivated);
	void setActivatedAll(bool bActivated);

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

	// Activation state.
	unsigned int   m_iActivated;

	// Internal running buffer chain references.
	float **m_pppBuffers[2];
};


//----------------------------------------------------------------------------
// qtractorPluginPort -- Plugin input-control port instance.
//

class qtractorPluginPort
{
public:

	// Constructors.
	qtractorPluginPort(qtractorPlugin *pPlugin, unsigned long iIndex);

	// Destructor.
	~qtractorPluginPort();

	// Main properties accessors.
	qtractorPlugin *plugin() const;
	unsigned long index() const;
	const char *name() const;
	LADSPA_PortRangeHintDescriptor hints() const;

	// Port descriptor predicate methods.
	bool isPortControlIn() const;
	bool isPortControlOut() const;
	bool isPortAudioIn() const;
	bool isPortAudioOut() const;
	
	// Port range hints predicate methods.
	bool isBoundedBelow() const;
	bool isBoundedAbove() const;
	bool isDefaultValue() const;
	bool isLogarithmic() const;
	bool isSampleRate() const;
	bool isInteger() const;
	bool isToggled() const;
	
	// Bounding range values.
	void setMinValue(float fMaxValue);
	float minValue() const;
	void setMaxValue(float fMaxValue);
	float maxValue() const;
	
	// Default value
	void setDefaultValue(float fDefaultValue);
	float defaultValue() const;
	
	// Current port value.
	void setValue(float fValue);
	float value() const;
	float *data();

	// Reset-to-default method.
	void reset();

private:

	// Instance variables.
	qtractorPlugin *m_pPlugin;
	unsigned long   m_iIndex;
	
	LADSPA_PortDescriptor m_portType;
	LADSPA_PortRangeHintDescriptor m_portHints;

	// Port values.
	float m_fMinValue;
	float m_fMaxValue;
	float m_fDefaultValue;
	float m_fValue;
};


#endif  // __qtractorPlugin_h

// end of qtractorPlugin.h
