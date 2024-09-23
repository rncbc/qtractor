// qtractorInsertPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2011, Holger Dehnhardt.

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

#ifndef __qtractorInsertPlugin_h
#define __qtractorInsertPlugin_h

#include "qtractorPlugin.h"


// Forward declarations.
class qtractorAudioBus;
class qtractorMidiBus;
class qtractorMidiInputBuffer;
class qtractorMidiOutputBuffer;


//----------------------------------------------------------------------------
// qtractorInsertPluginType -- Insert pseudo-plugin type instance.
//

class qtractorInsertPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorInsertPluginType(unsigned short iChannels)
		: qtractorPluginType(nullptr, iChannels, qtractorPluginType::Insert) {}

	// Factory method (static)
	static qtractorPlugin *createPlugin(
		qtractorPluginList *pList, unsigned short iChannels);

	// Specific named accessors.
	unsigned short channels() const
		{ return index(); }
};


//----------------------------------------------------------------------------
// qtractorAudioInsertPluginType -- Audio-insert pseudo-plugin type instance.
//

class qtractorAudioInsertPluginType : public qtractorInsertPluginType
{
public:

	// Constructor.
	qtractorAudioInsertPluginType(unsigned short iChannels)
		: qtractorInsertPluginType(iChannels) {}

	// Derived methods.
	bool open();
	void close();

	// Compute the number of instances needed
	// for the given input/output audio channels.
	unsigned short instances(unsigned short iChannels, bool /*bMidi*/) const
		{ return (iChannels > 0 && iChannels == audioOuts() ? 1 : 0); }

	// Instance cached-deferred accessors.
	const QString& aboutText();
};


//----------------------------------------------------------------------------
// qtractorMidiInsertPluginType -- MIDI-insert pseudo-plugin type instance.
//

class qtractorMidiInsertPluginType : public qtractorInsertPluginType
{
public:

	// Constructor.
	qtractorMidiInsertPluginType()
		: qtractorInsertPluginType(0) {}

	// Derived methods.
	bool open();
	void close();

	// Compute the number of instances needed.
	unsigned short instances(unsigned short iChannels, bool bMidi) const
		{ return (iChannels > 0 && bMidi ? 1 : 0); }

	// Instance cached-deferred accessors.
	const QString& aboutText();
};


//----------------------------------------------------------------------------
// qtractorInsertPlugin -- Insert pseudo-plugin decl.
//

class qtractorInsertPlugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorInsertPlugin(qtractorPluginList *pList,
		qtractorInsertPluginType *pInsertType)
		: qtractorPlugin(pList, pInsertType) {}

	// Forward decls.
	class Param;
};


//----------------------------------------------------------------------------
// qtractorAudioInsertPlugin -- Audio-insert pseudo-plugin instance.
//

class qtractorAudioInsertPlugin : public qtractorInsertPlugin
{
public:

	// Constructors.
	qtractorAudioInsertPlugin(qtractorPluginList *pList,
		qtractorInsertPluginType *pInsertType);

	// Destructor.
	~qtractorAudioInsertPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Plugin configuration handlers.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// Audio specific accessor.
	qtractorAudioBus *audioBus() const;

	// Override title/name captioon.
	QString title() const;

protected:

	// Plugin configuration (connections).
	void freezeConfigs(int iBusMode);

private:

	// Instance variables.
	qtractorAudioBus *m_pAudioBus;

	Param *m_pSendGainParam;
	Param *m_pDryGainParam;
	Param *m_pWetGainParam;

	// Custom optimized processors.
	void (*m_pfnProcessGain)(float **, unsigned int,
		unsigned int, unsigned short, float);
	void (*m_pfnProcessDryWet)(float **, float **, unsigned int,
		unsigned short, float, float);
};


//----------------------------------------------------------------------------
// qtractorMidiInsertPlugin -- MIDI-insert pseudo-plugin instance.
//

class qtractorMidiInsertPlugin : public qtractorInsertPlugin
{
public:

	// Constructors.
	qtractorMidiInsertPlugin(qtractorPluginList *pList,
		qtractorInsertPluginType *pInsertType);

	// Destructor.
	~qtractorMidiInsertPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Plugin configuration handlers.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// MIDI specific accessor.
	qtractorMidiBus *midiBus() const;

	// Override title/name captioon.
	QString title() const;

protected:

	// Plugin configuration (connections).
	void freezeConfigs(int iBusMode);

private:

	// Instance variables.
	qtractorMidiBus *m_pMidiBus;

	qtractorMidiInputBuffer  *m_pMidiInputBuffer;
	qtractorMidiOutputBuffer *m_pMidiOutputBuffer;

	Param *m_pSendGainParam;
	Param *m_pDryGainParam;
	Param *m_pWetGainParam;
};


//----------------------------------------------------------------------------
// qtractorAuxSendPluginType -- Aux-send pseudo-plugin type instance.
//

class qtractorAuxSendPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorAuxSendPluginType(unsigned short iChannels)
		: qtractorPluginType(nullptr, iChannels, qtractorPluginType::AuxSend) {}

	// Factory method (static)
	static qtractorPlugin *createPlugin(
		qtractorPluginList *pList, unsigned short iChannels);

	// Specific named accessors.
	unsigned short channels() const
		{ return index(); }
};


//----------------------------------------------------------------------------
// qtractorAudioAuxSendPluginType -- Audio aux-send pseudo-plugin type.
//

class qtractorAudioAuxSendPluginType : public qtractorAuxSendPluginType
{
public:

	// Constructor.
	qtractorAudioAuxSendPluginType(unsigned short iChannels)
		: qtractorAuxSendPluginType(iChannels) {}

	// Derived methods.
	bool open();
	void close();

	// Compute the number of instances needed
	// for the given input/output audio channels.
	unsigned short instances(unsigned short iChannels, bool /*bMidi*/) const
		{ return (iChannels > 0 && iChannels == audioOuts() ? 1 : 0); }

	// Instance cached-deferred accesors.
	const QString& aboutText();
};


//----------------------------------------------------------------------------
// qtractorMidiAuxSendPluginType -- MIDI Aux-send pseudo-plugin type.
//

class qtractorMidiAuxSendPluginType : public qtractorAuxSendPluginType
{
public:

	// Constructor.
	qtractorMidiAuxSendPluginType()
		: qtractorAuxSendPluginType(0) {}

	// Derived methods.
	bool open();
	void close();

	// Compute the number of instances needed
	// for the given input/output audio channels.
	unsigned short instances(unsigned short iChannels, bool bMidi) const
		{ return (iChannels > 0 && bMidi ? 1 : 0); }

	// Instance cached-deferred accesors.
	const QString& aboutText();
};


//----------------------------------------------------------------------------
// qtractorAuxSendPlugin -- Aux-send pseudo-plugin decl.
//

class qtractorAuxSendPlugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorAuxSendPlugin(qtractorPluginList *pList,
		qtractorAuxSendPluginType *pAuxSendType)
		: qtractorPlugin(pList, pAuxSendType) {}

	// Forward decls.
	class Param;
};


//----------------------------------------------------------------------------
// qtractorAudioAuxSendPlugin -- Audio aux-send pseudo-plugin instance.
//

class qtractorAudioAuxSendPlugin : public qtractorAuxSendPlugin
{
public:

	// Constructors.
	qtractorAudioAuxSendPlugin(qtractorPluginList *pList,
		qtractorAuxSendPluginType *pAuxSendType);

	// Destructor.
	~qtractorAudioAuxSendPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Plugin configuration handlers.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// Audio bus specific accessors.
	void setAudioBusName(const QString& sAudioBusName, bool bReset = false);
	const QString& audioBusName() const;
	qtractorAudioBus *audioBus() const;

	// Audio bus to appear on plugin lists.
	void updateAudioBusName() const;

	// Override title/name captioon.
	QString title() const;

protected:

	// Do the actual (de)activation.
	void activate();
	void deactivate();

private:

	// Instance variables.
	qtractorAudioBus *m_pAudioBus;
	QString           m_sAudioBusName;

	Param *m_pSendGainParam;

	// Custom optimized processors.
	void (*m_pfnProcessAdd)(float **, float **, unsigned int,
		unsigned int, unsigned short, float);
};


//----------------------------------------------------------------------------
// qtractorMidiAuxSendPlugin -- MIDI aux-send pseudo-plugin instance.
//

class qtractorMidiAuxSendPlugin : public qtractorAuxSendPlugin
{
public:

	// Constructors.
	qtractorMidiAuxSendPlugin(qtractorPluginList *pList,
		qtractorAuxSendPluginType *pAuxSendType);

	// Destructor.
	~qtractorMidiAuxSendPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Plugin configuration handlers.
	void configure(const QString& sKey, const QString& sValue);

	// Plugin configuration/state snapshot.
	void freezeConfigs();
	void releaseConfigs();

	// Audio bus specific accessors.
	void setMidiBusName(const QString& sMidiBusName);
	const QString& midiBusName() const;
	qtractorMidiBus *midiBus() const;

	// Audio bus to appear on plugin lists.
	void updateMidiBusName() const;

	// Override title/name captioon.
	QString title() const;

protected:

	// Do the actual (de)activation.
	void activate();
	void deactivate();

private:

	// Instance variables.
	qtractorMidiBus *m_pMidiBus;
	QString          m_sMidiBusName;

	qtractorMidiOutputBuffer *m_pMidiOutputBuffer;

	Param *m_pSendGainParam;
};


//----------------------------------------------------------------------------
// qtractorInsertPlugin::Param -- Common insert plugin control input port.
//

class qtractorInsertPlugin::Param : public qtractorPlugin::Param
{
public:

	// Constructors.
	Param(qtractorInsertPlugin *pInsertPlugin, unsigned long iIndex)
		: qtractorPlugin::Param(pInsertPlugin, iIndex) {}

	// Port range hints predicate methods.
	bool isBoundedBelow() const { return true; }
	bool isBoundedAbove() const { return true; }
	bool isDefaultValue() const { return true; }
	bool isLogarithmic() const { return true; }
	bool isSampleRate() const { return false; }
	bool isInteger() const { return false; }
	bool isToggled() const { return false; }
	bool isDisplay() const { return false; }
};


//----------------------------------------------------------------------------
// qtractorAuxSendPlugin::Param -- Common aux-send plugin control input port.
//

class qtractorAuxSendPlugin::Param : public qtractorPlugin::Param
{
public:

	// Constructors.
	Param(qtractorAuxSendPlugin *pAuxSendPlugin, unsigned long iIndex)
		: qtractorPlugin::Param(pAuxSendPlugin, iIndex) {}

	// Port range hints predicate methods.
	bool isBoundedBelow() const { return true; }
	bool isBoundedAbove() const { return true; }
	bool isDefaultValue() const { return true; }
	bool isLogarithmic() const { return true; }
	bool isSampleRate() const { return false; }
	bool isInteger() const { return false; }
	bool isToggled() const { return false; }
	bool isDisplay() const { return false; }
};


#endif  // __qtractorInsertPlugin_h

// end of qtractorInsertPlugin.h
