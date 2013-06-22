// qtractorDssiPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

	// Parameter update method.
	void updateParam(qtractorPluginParam *pParam, float fValue, bool bUpdate);

	// Bank/program selector override.
	void selectProgram(int iBank, int iProg);

	// Provisional program/patch accessor.
	bool getProgram(int iIndex, Program& program) const;

	// Continuous controller handler.
	void setController(int iController, int iValue);

	// Configuration (CLOB) stuff.
	void configure(const QString& sKey, const QString& sValue);

	// GUI Editor stuff.
	void openEditor(QWidget */*pParent*/);
	void closeEditor();

	// GUI editor visibility state.
	void setEditorVisible(bool bVisible);
	bool isEditorVisible() const;

	// Specific accessors.
	const DSSI_Descriptor *dssi_descriptor() const;

	// Update all control output ports...
	void updateControlOuts(bool bForce = false);

	// Reset(null) internal editor reference.
	void clearEditor();

	// Idle editor update (static)
	static void idleEditorAll();

protected:

	// Post-(re)initializer.
	void resetChannels();

private:

	// Care of multiple instances here.
	class DssiMulti *m_pDssiMulti;

	// Internal editor structure accessor...
	struct DssiEditor *m_pDssiEditor;

	// GUI editor visiability status.
	bool m_bEditorVisible;

	// Controller port map.
	qtractorPluginParam *m_apControllerMap[128];

	// Tracking changes on output control ports.
	float *m_pfControlOutsLast;
};


#endif  // __qtractorDssiPlugin_h

// end of qtractorDssiPlugin.h
