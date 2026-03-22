// qtractorMidiControlPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiControlPlugin_h
#define __qtractorMidiControlPlugin_h

#include "qtractorPlugin.h"


// Forward declarations.
class qtractorMidiBus;


//----------------------------------------------------------------------------
// qtractorMidiControlPluginType -- MIDI Controller pseudo-plugin type instance.
//

class qtractorMidiControlPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorMidiControlPluginType()
		: qtractorPluginType(nullptr, 0, qtractorPluginType::Control) {}

	// Factory method (static)
	static qtractorPlugin *createPlugin(qtractorPluginList *pList);

	// Specific named accessors.
	unsigned short channels() const
		{ return 0; }

	// Derived methods.
	bool open();
	void close();

	// Compute the number of instances needed (always one).
	unsigned short instances(unsigned short iChannels, bool /*bMidi*/) const
		{ return (iChannels > 0 ? 1 : 0); }

	// Instance cached-deferred accessors.
	const QString& aboutText();
};


//----------------------------------------------------------------------------
// qtractorMidiControlPlugin -- MIDI Controller pseudo-plugin instance.
//

class qtractorMidiControlPlugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorMidiControlPlugin(qtractorPluginList *pList,
		qtractorMidiControlPluginType *pMidiControlType);

	// Destructor.
	~qtractorMidiControlPlugin();

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

	// Accessors.
	void setControlType(qtractorMidiControl::ControlType ctype);
	qtractorMidiControl::ControlType controlType() const;

	void setControlParam(unsigned short iParam);
	unsigned short controlParam() const;

	void setControlChannel(unsigned short iChannel);
	unsigned short controlChannel() const;

	void setControlLogarithmic(bool bLogarithmic);
	bool isControlLogarithmic() const;

	void setControlInvert(bool bInvert);
	bool isControlInvert() const;

	void setControlBipolar(bool bBipolar);
	bool isControlBipolar() const;

	void setControlAutoConnect(bool bAutoConnect);
	bool isControlAutoConnect() const;

	// Override title/name caption.
	QString title() const;

	// Forward decls.
	class Param;

protected:

	void updateControlBipolar();
	void updateControlAutoConnect();

	// Parameter update method (virtual).
	void updateParam(qtractorPlugin::Param *pParam, float fValue, bool bUpdate);

private:

	// Instance variables.
	qtractorMidiBus *m_pMidiBus;

	qtractorMidiControl::ControlType m_controlType;
	unsigned short m_iControlParam;
	unsigned short m_iControlChannel;
	bool m_bControlLogarithmic;
	bool m_bControlInvert;
	bool m_bControlBipolar;
	bool m_bControlAutoConnect;

	Param *m_pControlValueParam;
};


//----------------------------------------------------------------------------
// qtractorMidiControlPlugin::Param -- MIDI Controller plugin control input port.
//

class qtractorMidiControlPlugin::Param : public qtractorPlugin::Param
{
public:

	// Constructors.
	Param(qtractorMidiControlPlugin *pMidiControlPlugin, unsigned long iIndex)
		: qtractorPlugin::Param(pMidiControlPlugin, iIndex) {}

protected:

	// Property range hints predicate methods.
	bool isBoundedBelow() const { return true; }
	bool isBoundedAbove() const { return true; }
	bool isDefaultValue() const { return true; }
	bool isLogarithmic() const;
	bool isSampleRate() const { return false; }
	bool isInteger() const { return false; }
	bool isToggled() const { return false; }
	bool isDisplay() const { return false; }
};


#endif  // __qtractorMidiControlPlugin_h

// end of qtractorMidiControlPlugin.h
