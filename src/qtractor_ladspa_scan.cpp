// qtractor_ladspa_scan.cpp
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
#include "config.h"

#include "qtractor_ladspa_scan.h"

#include <QLibrary>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

#include <stdint.h>


#ifdef CONFIG_LADSPA

//----------------------------------------------------------------------
// class qtractor_ladspa_scan -- LADSPA plugin re bones) interface
//

// Constructor.
qtractor_ladspa_scan::qtractor_ladspa_scan (void)
	: m_pLibrary(NULL), m_pLadspaDescriptor(NULL),
		m_iControlIns(0), m_iControlOuts(0),
		m_iAudioIns(0), m_iAudioOuts(0)
{
}


// destructor.
qtractor_ladspa_scan::~qtractor_ladspa_scan (void)
{
	close();
}


// File loader.
bool qtractor_ladspa_scan::open ( const QString& sFilename )
{
	close();

	if (!QLibrary::isLibrary(sFilename))
		return false;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::open(\"%s\")", this,
		sFilename.toUtf8().constData());
#endif

	m_pLibrary = new QLibrary(sFilename);

	return true;
}


// Plugin loader.
bool qtractor_ladspa_scan::open_descriptor ( unsigned long iIndex )
{
	if (m_pLibrary == NULL)
		return false;

	close_descriptor();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::open_descriptor(%lu)", this, iIndex);
#endif

	// Retrieve the descriptor function, if any...
	LADSPA_Descriptor_Function pfnLadspaDescriptor
		= (LADSPA_Descriptor_Function) m_pLibrary->resolve("ladspa_descriptor");
	if (pfnLadspaDescriptor == NULL) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_ladspa_scan[%p]: plugin does not have a main entry point.", this);
	#endif
		return false;
	}

	// Retrieve descriptor if any...
	m_pLadspaDescriptor = (*pfnLadspaDescriptor)(iIndex);
	if (m_pLadspaDescriptor == NULL) {
	#ifdef CONFIG_DEBUG
		qDebug("qtractor_ladspa_scan[%p]: plugin instance could not be created.", this);
	#endif
		return false;
	}

	// Get the official plugin name.
	m_sName = QString::fromLatin1(m_pLadspaDescriptor->Name);

	// Compute and cache port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;

	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;

	for (unsigned long i = 0; i < m_pLadspaDescriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor portType
			= m_pLadspaDescriptor->PortDescriptors[i];
		if (LADSPA_IS_PORT_INPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioIns;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlIns;
		}
		else
		if (LADSPA_IS_PORT_OUTPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioOuts;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlOuts;
		}
	}

	return true;
}


// Plugin uloader.
void qtractor_ladspa_scan::close_descriptor (void)
{
	if (m_pLadspaDescriptor == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::close_descriptor()", this);
#endif

	m_pLadspaDescriptor = NULL;

	m_sName.clear();

	m_iControlIns  = 0;
	m_iControlOuts = 0;

	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
}


// File unloader.
void qtractor_ladspa_scan::close (void)
{
	close_descriptor();

	if (m_pLibrary == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractor_ladspa_scan[%p]::close()", this);
#endif

	if (m_pLibrary->isLoaded())
		m_pLibrary->unload();

	delete m_pLibrary;

	m_pLibrary = NULL;
}


// Check wether plugin is loaded.
bool qtractor_ladspa_scan::isOpen (void) const
{
	if (m_pLibrary == NULL)
		return false;

	return m_pLibrary->isLoaded();
}


unsigned int qtractor_ladspa_scan::uniqueID() const
	{ return (m_pLadspaDescriptor ? m_pLadspaDescriptor->UniqueID : 0); }

bool qtractor_ladspa_scan::isRealtime() const
{
	if (m_pLadspaDescriptor == NULL)
		return false;

	return LADSPA_IS_HARD_RT_CAPABLE(m_pLadspaDescriptor->Properties);
}


//-------------------------------------------------------------------------
// The LADSPA plugin stance scan method.
//

static void qtractor_ladspa_scan_file ( const QString& sFilename )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_ladspa_scan_file(\"%s\")", sFilename.toUtf8().constData());
#endif
	qtractor_ladspa_scan plugin;

	if (!plugin.open(sFilename))
		return;

	QTextStream sout(stdout);
	unsigned long i = 0;

	while (plugin.open_descriptor(i)) {
		sout << "LADSPA|";
		sout << plugin.name() << '|';
		sout << plugin.audioIns()   << ':' << plugin.audioOuts()   << '|';
		sout << 0                   << ':' << 0                    << '|';
		sout << plugin.controlIns() << ':' << plugin.controlOuts() << '|';
		QStringList flags;
		if (plugin.isRealtime())
			flags.append("RT");
		sout << flags.join(",") << '|';
		sout << sFilename << '|' << i << '|';
		sout << "0x" << QString::number(plugin.uniqueID(), 16) << '\n';
		plugin.close_descriptor();
		++i;
	}

	plugin.close();

	// Must always give an answer, even if it's wrong...
	if (i == 0)
		sout << "qtractor_ladspa_scan: " << sFilename << ": plugin file error.\n";
}

#endif	// CONFIG_LADSPA


//-------------------------------------------------------------------------
// main - The main program trunk.
//

#include <QCoreApplication>

int main ( int argc, char **argv )
{
	QCoreApplication app(argc, argv);
#ifdef CONFIG_DEBUG
	qDebug("%s: hello. (version %s)", argv[0], CONFIG_BUILD_VERSION);
#endif
	QTextStream sin(stdin);
	while (!sin.atEnd()) {
		const QString& sLine = sin.readLine();
		if (sLine.isEmpty())
			break;
		const QStringList& req = sLine.split(':');
		const QString& sHint = req.at(0).toUpper();
		const QString& sFilename = req.at(1);
	#ifdef CONFIG_LADSPA
		if (sHint == "LADSPA")
			qtractor_ladspa_scan_file(sFilename);
#endif
	}
#ifdef CONFIG_DEBUG
	qDebug("%s: bye.", argv[0]);
#endif
	return 0;
}


// end of qtractor_ladspa_scan.cpp
