// qtractorLadspaPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorLadspaPlugin_h
#define __qtractorLadspaPlugin_h

#include "qtractorPlugin.h"

#include <ladspa.h>


//----------------------------------------------------------------------------
// qtractorLadspaPluginType -- LADSPA plugin type instance.
//

class qtractorLadspaPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorLadspaPluginType(qtractorPluginFile *pFile, unsigned long iIndex,
		qtractorPluginType::Hint typeHint = qtractorPluginType::Ladspa,
		const LADSPA_Descriptor *pLadspaDescriptor = NULL)
		: qtractorPluginType(pFile, iIndex, typeHint),
			m_pLadspaDescriptor(pLadspaDescriptor) {}

	// Destructor.
	~qtractorLadspaPluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Factory method (static)
	static qtractorLadspaPluginType *createType(
		qtractorPluginFile *pFile, unsigned long iIndex);

	// LADSPA descriptor method (static)
	static const LADSPA_Descriptor *ladspa_descriptor(
		qtractorPluginFile *pFile, unsigned long iIndex);

	// Specific accessors.
	const LADSPA_Descriptor *ladspa_descriptor() const
		{ return m_pLadspaDescriptor; }

	// Instance cached-deferred accesors.
	const QString& aboutText();

protected:

	// LADSPA descriptor itself.
	const LADSPA_Descriptor *m_pLadspaDescriptor;
};


//----------------------------------------------------------------------------
// qtractorLadspaPlugin -- LADSPA plugin instance.
//

class qtractorLadspaPlugin : public qtractorPlugin
{
public:

	// Constructors.
	qtractorLadspaPlugin(qtractorPluginList *pList,
		qtractorLadspaPluginType *pLadspaType);

	// Destructor.
	~qtractorLadspaPlugin();

	// Channel/intsance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Specific accessors.
	const LADSPA_Descriptor *ladspa_descriptor() const;
	LADSPA_Handle ladspa_handle(unsigned short iInstance) const;

	// Audio port numbers.
	unsigned long audioIn(unsigned short i)
		{ return m_piAudioIns[i]; }
	unsigned long audioOut(unsigned short i)
		{ return m_piAudioOuts[i]; }

protected:

	// Instance variables.
	LADSPA_Handle *m_phInstances;

	// List of output control port indexes and data.
	unsigned long *m_piControlOuts;
	float         *m_pfControlOuts;

	// List of audio port indexes.
	unsigned long *m_piAudioIns;
	unsigned long *m_piAudioOuts;
};


//----------------------------------------------------------------------------
// qtractorLadspaPluginParam -- LADSPA plugin control input port instance.
//

class qtractorLadspaPluginParam : public qtractorPluginParam
{
public:

	// Constructors.
	qtractorLadspaPluginParam(qtractorLadspaPlugin *pLadspaPlugin,
		unsigned long iIndex);

	// Destructor.
	~qtractorLadspaPluginParam();

	// Port range hints predicate methods.
	bool isBoundedBelow() const;
	bool isBoundedAbove() const;
	bool isDefaultValue() const;
	bool isLogarithmic() const;
	bool isSampleRate() const;
	bool isInteger() const;
	bool isToggled() const;
	bool isDisplay() const;

private:

	// Instance variables.
	LADSPA_PortRangeHintDescriptor m_portHints;
};


#endif  // __qtractorLadspaPlugin_h

// end of qtractorLadspaPlugin.h
