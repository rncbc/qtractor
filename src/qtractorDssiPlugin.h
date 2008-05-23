// qtractorDssiPlugin.h
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

#ifndef __qtractorDsiiPlugin_h
#define __qtractorDssiPlugin_h

#include "qtractorLadspaPlugin.h"

#include <dssi.h>


//----------------------------------------------------------------------------
// qtractorDssiPluginType -- DSSI plugin type instance.
//

class qtractorDssiPluginType : public qtractorLadspaPluginType
{
public:

	// Constructor.
	qtractorDssiPluginType(qtractorPluginFile *pFile, unsigned long iIndex,
		const DSSI_Descriptor *pDssiDescriptor = NULL)
		: qtractorLadspaPluginType(pFile, iIndex, qtractorPluginType::Dssi),
			m_pDssiDescriptor(pDssiDescriptor) {}

	// Destructor.
	~qtractorDssiPluginType()
		{ close(); }

	// Derived methods.
	bool open();
	void close();

	// Factory method (static)
	static qtractorDssiPluginType *createType(
		qtractorPluginFile *pFile, unsigned long iIndex);

	// DSSI descriptor method (static)
	static const DSSI_Descriptor *dssi_descriptor(
		qtractorPluginFile *pFile, unsigned long iIndex);

	// Specific accessors.
	const DSSI_Descriptor *dssi_descriptor() const
		{ return m_pDssiDescriptor; }

	const QString& dssi_editor() const
		{ return m_sDssiEditor;  }

private:

	// DSSI descriptor itself.
	const DSSI_Descriptor *m_pDssiDescriptor;

	// DSSI GUI excutable filename.
	QString m_sDssiEditor;
};


//----------------------------------------------------------------------------
// qtractorDssiPlugin -- DSSI plugin instance.
//

class qtractorDssiPlugin : public qtractorLadspaPlugin
{
public:

	// Constructors.
	qtractorDssiPlugin(qtractorPluginList *pList,
		qtractorDssiPluginType *pDssiType);

	// Destructor.
	~qtractorDssiPlugin();

	// Channel/instance number accessors.
	void setChannels(unsigned short iChannels);

	// Do the actual (de)activation.
	void activate();
	void deactivate();

	// The main plugin processing procedure.
	void process(float **ppIBuffer, float **ppOBuffer, unsigned int nframes);

	// Bank/program selector override.
	void selectProgram(int iBank, int iProg);

	// GUI Editor stuff.
	void openEditor(QWidget */*pParent*/);
	void closeEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	// Specific accessors.
	const DSSI_Descriptor *dssi_descriptor() const;

private:

	// GUI editor visiability status.
	bool m_bEditorVisible;
};


//----------------------------------------------------------------------------
// qtractorDssiPluginParam -- DSSI plugin control input port instance.
//

class qtractorDssiPluginParam : public qtractorLadspaPluginParam
{
public:

	// Constructors.
	qtractorDssiPluginParam(qtractorDssiPlugin *pDssiPlugin,
		unsigned long iIndex);

	// Destructor.
	~qtractorDssiPluginParam();
};


#endif  // __qtractorDssiPlugin_h

// end of qtractorDssiPlugin.h
