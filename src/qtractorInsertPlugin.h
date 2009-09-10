// qtractorInsertPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorEngine.h"

// Forward declarations.
class qtractorAudioBus;


//----------------------------------------------------------------------------
// qtractorInsertPluginType -- Insert pseudo-plugin type instance.
//

class qtractorInsertPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorInsertPluginType(unsigned short iChannels)
		: qtractorPluginType(NULL, iChannels, qtractorPluginType::Any),
			m_iChannels(iChannels) {}

	// Destructor.
	~qtractorInsertPluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Compute the number of instances needed
	// for the given input/output audio channels.
	unsigned short instances(
		unsigned short iChannels, bool /*bMidi*/) const
		{ return (iChannels == m_iAudioOuts ? 1 : 0); }

	// Factory method (static)
	static qtractorInsertPluginType *createType(unsigned short iChannels);

	// Specific accessors.
	unsigned short channels() const
		{ return m_iChannels; }

private:

	// Number of insert channels
	unsigned short m_iChannels;
};


//----------------------------------------------------------------------------
// qtractorInsertPlugin -- Insert pseudo-plugin instance.
//

class qtractorInsertPlugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorInsertPlugin(qtractorPluginList *pList,
		qtractorInsertPluginType *pInsertType);

	// Destructor.
	~qtractorInsertPlugin();

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

protected:

	// Plugin configuration (connections).
	void freezeConfigs(qtractorBus::BusMode busMode);

private:

	// Instance variables.
	qtractorAudioBus *m_pAudioBus;
};


#endif  // __qtractorInsertPlugin_h

// end of qtractorInsertPlugin.h
