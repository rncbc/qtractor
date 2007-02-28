// qtractorPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginForm.h"

#include "qtractorSessionDocument.h"

#include <QDir>


#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#define PATH_SEPARATOR	';'
typedef void (*LADSPA_Procedure_Function)(void);
#else
#define PATH_SEPARATOR	':'
#define PATH_DEFAULT	"/usr/local/lib/ladspa:/usr/lib/ladspa"
#endif

#include <math.h>


//----------------------------------------------------------------------------
// qtractorPluginPath -- Plugin path helper.
//

// Constructor.
qtractorPluginPath::qtractorPluginPath ( const QString& sPaths )
{
	QString sPathStr = sPaths;
	if (sPathStr.isEmpty())
		sPathStr = ::getenv("LADSPA_PATH");
#if defined(PATH_DEFAULT)
	if (sPathStr.isEmpty())
		sPathStr = PATH_DEFAULT;
#endif
	m_paths = sPathStr.split(PATH_SEPARATOR);
}

// Destructor.
qtractorPluginPath::~qtractorPluginPath (void)
{
	close();
}


// Main properties accessors.
const QStringList& qtractorPluginPath::paths (void) const
{
	return m_paths;
}


// Executive methods.
bool qtractorPluginPath::open (void)
{
	close();

	QStringListIterator ipath(m_paths);
	while (ipath.hasNext()) {
		const QDir dir(ipath.next());
		const QStringList& list = dir.entryList(QDir::Files);
		QStringListIterator iter(list);
		while (iter.hasNext()) {
			m_files.append(
				new qtractorPluginFile(dir.absoluteFilePath(iter.next())));
		}
	}

	return (m_files.count() > 0);
}

void qtractorPluginPath::close (void)
{
	qDeleteAll(m_files);
	m_files.clear();
}


// Plugin file list.
QList<qtractorPluginFile *>& qtractorPluginPath::files (void)
{
	return m_files;
}


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

// Constructor.
qtractorPluginFile::qtractorPluginFile ( const QString& sFilename )
	: QLibrary(sFilename)
{
	m_pfnDescriptor = NULL;
}

// Destructor.
qtractorPluginFile::~qtractorPluginFile (void)
{
	close();
}


// Executive methods.
bool qtractorPluginFile::open (void)
{
//	close();

	// ATTN: Not really need has it will be
	// loaded automagically on resolve()...
//	if (!QLibrary::load())
//		return false;

	// Do the openning dance...
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
	LADSPA_Procedure_Function pfnInit
		= (LADSPA_Procedure_Function) QLibrary::resolve("_init");
	if (pfnInit)
		(*pfnInit)();
#endif

	// Retrieve the descriptor function, if any...
	m_pfnDescriptor
		= (LADSPA_Descriptor_Function) QLibrary::resolve("ladspa_descriptor");
	// Nothing must be left behind...
	if (m_pfnDescriptor == NULL)
		close();

	return (m_pfnDescriptor != NULL);
}


void qtractorPluginFile::close (void)
{
	if (!QLibrary::isLoaded())
		return;

	// Nothing should be left behind.-..
	m_pfnDescriptor = NULL;

	qDeleteAll(m_types);
	m_types.clear();

	// Do the closing dance...
#if 0 // defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
	LADSPA_Procedure_Function pfnFini
		= (LADSPA_Procedure_Function) QLibrary::resolve("_fini");
	if (pfnFini)
		(*pfnFini)();
#endif

	QLibrary::unload();
}


// Plugin type list.
QList<qtractorPluginType *>& qtractorPluginFile::types (void)
{
	// Try to fill the types list at this moment...
	if (m_pfnDescriptor && m_types.isEmpty()) {
		// Fill in the complete type list...
		for (unsigned long iIndex = 0; descriptor(iIndex); iIndex++)
			m_types.append(new qtractorPluginType(this, iIndex));
	}

	// Give away our reference.
	return m_types;
}


// Descriptor function method.
const LADSPA_Descriptor *qtractorPluginFile::descriptor ( unsigned long iIndex )
{
	if (m_pfnDescriptor == NULL)
		return NULL;

	return (*m_pfnDescriptor)(iIndex);
}


//----------------------------------------------------------------------------
// qtractorPluginType -- Plugin type instance.
//

// Constructor.
qtractorPluginType::qtractorPluginType ( qtractorPluginFile *pFile,
	unsigned long iIndex )
{
	m_pFile  = pFile;
	m_iIndex = iIndex;

	m_pDescriptor = m_pFile->descriptor(m_iIndex);
}

// Destructor.
qtractorPluginType::~qtractorPluginType (void)
{
}


// Main properties accessors.
qtractorPluginFile *qtractorPluginType::file (void) const
{
	return m_pFile;
}

unsigned long qtractorPluginType::index (void) const
{
	return m_iIndex;
}

const LADSPA_Descriptor *qtractorPluginType::descriptor (void) const
{
	return m_pDescriptor;
}


// Derived accessors.
QString qtractorPluginType::filename (void) const
{
	return m_pFile->fileName();
}

const char *qtractorPluginType::name (void) const
{
	return (m_pDescriptor ? m_pDescriptor->Name : NULL);
}


//----------------------------------------------------------------------------
// qtractorPlugin -- Plugin instance.
//

// Constructors.
qtractorPlugin::qtractorPlugin ( qtractorPluginList *pList,
	qtractorPluginType *pType )
{
	initPlugin(pList, pType->filename(), pType->index());
}

qtractorPlugin::qtractorPlugin ( qtractorPluginList *pList,
	const QString& sFilename, unsigned long iIndex )
{
	initPlugin(pList, sFilename, iIndex);
}

// Destructor.
qtractorPlugin::~qtractorPlugin (void)
{
	// Clear out all dependables...
	qDeleteAll(m_cports);
	m_cports.clear();
	m_items.clear();

	// Cleanup all plugin instances...
	setChannels(0);

	// Rest of stuff goes cleaned too...
	if (m_pType) {
		qtractorPluginFile *pFile = m_pType->file();
		delete m_pType;
		if (pFile)
			delete pFile;
	}
}


// Plugin initializer.
void qtractorPlugin::initPlugin ( qtractorPluginList *pList,
	const QString& sFilename, unsigned long iIndex )
{
	m_pList       = pList;
	m_pType       = NULL;
	m_iInstances  = 0;
	m_phInstances = NULL;
	m_bActivated  = false;
	m_pForm       = NULL;

	qtractorPluginFile *pFile = new qtractorPluginFile(sFilename);
	if (pFile->open()) {
		m_pType = new qtractorPluginType(pFile, iIndex);
		// Get some structural data first...
		const LADSPA_Descriptor *pDescriptor = m_pType->descriptor();
		if (pDescriptor) {
			for (unsigned long i = 0; i < pDescriptor->PortCount; i++) {
				const LADSPA_PortDescriptor portType
					= pDescriptor->PortDescriptors[i];
				if (LADSPA_IS_PORT_INPUT(portType)) {
					if (LADSPA_IS_PORT_AUDIO(portType))
						m_iports.append(i);
					else
					if (LADSPA_IS_PORT_CONTROL(portType))
						m_cports.append(new qtractorPluginPort(this, i));
				}
				else
				if (LADSPA_IS_PORT_OUTPUT(portType)) {
					if (LADSPA_IS_PORT_AUDIO(portType))
						m_oports.append(i);
					else
					if (LADSPA_IS_PORT_CONTROL(portType))
						m_vports.append(i);
				}
			}
			// Finally, instantiate each instance properly...
			setChannels(m_pList->channels());
		}
	}	// Must free anyway...
	else delete pFile;
}


// Channel/intsance number accessors.
void qtractorPlugin::setChannels ( unsigned short iChannels )
{
	// Estimate the (new) number of instances...
	unsigned short iInstances = 0;
	if (iChannels > 0) {
		unsigned short iAudioIns  = m_iports.count();
		unsigned short iAudioOuts = m_oports.count();
		if (iAudioIns == iChannels && iAudioOuts == iChannels)
			iInstances = 1;
		else if ((iAudioIns < 2 || iAudioIns == iChannels) && iAudioOuts < 2)
			iInstances = iChannels;
	}

	// Now see if instance count changed anyhow...
	if (iInstances == m_iInstances)
		return;

	const LADSPA_Descriptor *pDescriptor = descriptor();
	if (pDescriptor == NULL)
		return;

	// Gotta go for a while...
	bool bActivated = isActivated();
	setActivated(false);

	if (m_phInstances) {
		if (pDescriptor->cleanup) {
			for (unsigned short i = 0; i < m_iInstances; i++)
				(*pDescriptor->cleanup)(m_phInstances[i]);
		}
		delete [] m_phInstances;
		m_phInstances = NULL;
	}

	// Set new instance number...
	m_iInstances = iInstances;

	// some sanity required here...
	if (m_iInstances < 1) {
		// We're sorry but dialogs must also go now...
		if (m_pForm) {
			delete m_pForm;
			m_pForm = NULL;
		}
		// Will stay inactive...
		return;
	}

	// FIXME: The dummy value for output control (dummy) port indexes...
	static float s_fDummyData = 0.0f;
	// Allocate new instances...
	m_phInstances = new LADSPA_Handle [m_iInstances];
	for (unsigned short i = 0; i < m_iInstances; i++) {
		// Instantiate them properly first...
		m_phInstances[i] = (*pDescriptor->instantiate)(
			pDescriptor, m_pList->sampleRate());
		// Connect all existing input control ports...
		QListIterator<qtractorPluginPort *> cport(m_cports);
		while (cport.hasNext()) {
			qtractorPluginPort *pPort = cport.next();
			(*pDescriptor->connect_port)(m_phInstances[i],
				pPort->index(), pPort->data());
		}
		// Connect all existing output control (dummy) ports...
		QListIterator<unsigned long> vport(m_vports);
		while (vport.hasNext()) {
			(*pDescriptor->connect_port)(m_phInstances[i],
				vport.next(), &s_fDummyData);
		}
	}

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


unsigned short qtractorPlugin::channels (void) const
{
	return (m_pList ? m_pList->channels() : 0);
}


// Main properties accessors.
qtractorPluginList *qtractorPlugin::list (void) const
{
	return m_pList;
}

qtractorPluginType *qtractorPlugin::type (void) const
{
	return m_pType;
}

unsigned short qtractorPlugin::instances (void) const
{
	return m_iInstances;
}

unsigned int qtractorPlugin::sampleRate (void) const
{
	return m_iSampleRate;
}

LADSPA_Handle qtractorPlugin::handle ( unsigned short iInstance ) const
{
	return (m_phInstances ? m_phInstances[iInstance] : NULL);
}


// Derived accessors.
qtractorPluginFile *qtractorPlugin::file (void) const
{
	return (m_pType ? m_pType->file() : NULL);
}

unsigned long qtractorPlugin::index (void) const
{
	return (m_pType ? m_pType->index() : 0);
}

const LADSPA_Descriptor *qtractorPlugin::descriptor (void) const
{
	return (m_pType ? m_pType->descriptor() : NULL);
}

QString qtractorPlugin::filename (void) const
{
	return (m_pType ? m_pType->filename() : QString::null);
}

const char *qtractorPlugin::name (void) const
{
	return (m_pType ? m_pType->name() : NULL);
}


// Activation methods.
void qtractorPlugin::setActivated ( bool bActivated )
{
	if (bActivated && !m_bActivated) {
		activate();
		m_pList->updateActivated(true);
	} else if (!bActivated && m_bActivated) {
		deactivate();
		m_pList->updateActivated(false);
	}

	m_bActivated = bActivated;

	QListIterator<qtractorPluginListItem *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateActivated();

	if (m_pForm)
		m_pForm->updateActivated();
}

bool qtractorPlugin::isActivated (void) const
{
	return m_bActivated;
}


// Do the actual activation.
void qtractorPlugin::activate (void)
{
	const LADSPA_Descriptor *pDescriptor = descriptor();
	if (pDescriptor == NULL)
		return;

	if (m_phInstances && pDescriptor->activate) {
		for (unsigned short i = 0; i < m_iInstances; i++)
			(*pDescriptor->activate)(m_phInstances[i]);
	}
}


// Do the actual deactivation.
void qtractorPlugin::deactivate (void)
{
	const LADSPA_Descriptor *pDescriptor = descriptor();
	if (pDescriptor == NULL)
		return;

	if (m_phInstances && pDescriptor->deactivate) {
		for (unsigned short i = 0; i < m_iInstances; i++)
			(*pDescriptor->deactivate)(m_phInstances[i]);
	}
}


// The main plugin processing procedure.
void qtractorPlugin::process ( unsigned int nframes )
{
	if (!m_bActivated || m_phInstances == NULL)
		return;

	const LADSPA_Descriptor *pDescriptor = descriptor();
	if (pDescriptor == NULL)
		return;

	for (unsigned short i = 0; i < m_iInstances; i++)
		(*pDescriptor->run)(m_phInstances[i], nframes);
}


// Input control ports list accessor.
QList<qtractorPluginPort *>& qtractorPlugin::cports (void)
{
	return m_cports;
}


// Output control (dummy) port index-list accessors.
const QList<unsigned long>& qtractorPlugin::vports (void) const
{
	return m_vports;
}


// Audio port indexes list accessor.
const QList<unsigned long>& qtractorPlugin::iports (void) const
{
	return m_iports;
}

const QList<unsigned long>& qtractorPlugin::oports (void) const
{
	return m_oports;
}


// An accessible list of observers.
QList<qtractorPluginListItem *>& qtractorPlugin::items (void)
{
	return m_items;
}


// Special plugin form accessors.
bool qtractorPlugin::isVisible (void) const
{
	return (m_pForm ? m_pForm->isVisible() : false);
}

qtractorPluginForm *qtractorPlugin::form (void)
{
	// Take the change and create the form if it doesn't current exist.
	if (m_pForm == NULL) {
		m_pForm = new qtractorPluginForm(0,
			Qt::Tool
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowMinMaxButtonsHint);
		m_pForm->setPreset(m_sPreset);
		m_pForm->setPlugin(this);
	}

	return m_pForm;
}


// Plugin default preset name accessor (informational)
void qtractorPlugin::setPreset ( const QString& sPreset )
{
	m_sPreset = sPreset;

	if (m_pForm)
		m_pForm->setPreset(sPreset);
}

const QString& qtractorPlugin::preset (void)
{
	if (m_pForm)
		m_sPreset = m_pForm->preset();

	return m_sPreset;
}


// Plugin state serialization methods.
void qtractorPlugin::setValues ( const QStringList& vlist )
{
	// Split it up...
	QStringListIterator val(vlist);
	QListIterator<qtractorPluginPort *> iter(m_cports);
	while (iter.hasNext() && val.hasNext())
		iter.next()->setValue(val.next().toFloat());
}

QStringList qtractorPlugin::values (void)
{
	// Join it up...
	QStringList vlist;
	QListIterator<qtractorPluginPort *> iter(m_cports);
	while (iter.hasNext())
		vlist.append(QString::number(iter.next()->value()));

	return vlist;
}


// Plugin preset group - common identification prefix.
QString qtractorPlugin::presetGroup (void) const
{
	QString sGroup;

	// Normalize plugin identification prefix...
	const LADSPA_Descriptor *pDescriptor = descriptor();
	if (pDescriptor) {
		sGroup  = "/Plugin/";
		sGroup += pDescriptor->Label;
		sGroup += '_';
		sGroup += QString::number(pDescriptor->UniqueID);
	}

	return sGroup;
}


// Reset-to-default method.
void qtractorPlugin::reset (void)
{
	QListIterator<qtractorPluginPort *> iter(m_cports);
	while (iter.hasNext())
		iter.next()->reset();
}


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

// Constructor.
qtractorPluginList::qtractorPluginList ( unsigned short iChannels,
	unsigned int iBufferSize, unsigned int iSampleRate )
{
	m_iChannels   = 0;
	m_iBufferSize = 0;
	m_iSampleRate = 0;

	m_iActivated  = 0;

	m_pppBuffers[0] = NULL;
	m_pppBuffers[1] = NULL;

	setBuffer(iChannels, iBufferSize, iSampleRate);
}

// Destructor.
qtractorPluginList::~qtractorPluginList (void)
{
	setBuffer(0, 0, 0);

	// Clear out all dependables...
	m_views.clear();
}


// The title to show up on plugin forms...
void qtractorPluginList::setName ( const QString& sName )
{
	m_sName = sName;

	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		pPlugin->form()->updateCaption();
	}
}

const QString& qtractorPluginList::name (void) const
{
	return m_sName;
}


// Main-parameters accessor.
void qtractorPluginList::setBuffer ( unsigned short iChannels,
	unsigned int iBufferSize, unsigned int iSampleRate )
{
	unsigned short i;

	// Delete old interim buffer...
	if (m_pppBuffers[1]) {
		for (i = 0; i < m_iChannels; i++)
			delete [] m_pppBuffers[1][i];
		delete [] m_pppBuffers[1];
		m_pppBuffers[1] = NULL;
	}

	// Some sanity is in order, at least for now...
	if (iChannels == 0 || iBufferSize == 0 || iSampleRate == 0)
		return;

	// Go, go, go...
	m_iChannels   = iChannels;
	m_iBufferSize = iBufferSize;
	m_iSampleRate = iSampleRate;

	// Allocate new interim buffer...
	if (m_iChannels > 0 && m_iBufferSize > 0) {
		m_pppBuffers[1] = new float * [m_iChannels];
		for (i = 0; i < m_iChannels; i++) {
			m_pppBuffers[1][i] = new float [m_iBufferSize];
			::memset(m_pppBuffers[1][i], 0, m_iBufferSize * sizeof(float));
		}
	}

	// Reset all plugin chain channels...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next())
		pPlugin->setChannels(m_iChannels);
}


// Reset and (re)activate all plugin chain.
void qtractorPluginList::resetBuffer (void)
{
	// Temporarily deactivate all activated plugins...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated())
			pPlugin->deactivate();
	}

	// Reset interim buffer, if any...
	if (m_pppBuffers[1]) {
		for (unsigned short i = 0; i < m_iChannels; i++)
			::memset(m_pppBuffers[1][i], 0, m_iBufferSize * sizeof(float));
	}

	// Restore activation of all previously deactivated plugins...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated())
			pPlugin->activate();
	}
}


// Brainless accessors.
unsigned short qtractorPluginList::channels (void) const
{
	return m_iChannels;
}

unsigned int qtractorPluginList::sampleRate (void) const
{
	return m_iSampleRate;
}

unsigned int qtractorPluginList::bufferSize (void) const
{
	return m_iBufferSize;
}


// Special guard activation methods.
unsigned int qtractorPluginList::activated (void) const
{
	return m_iActivated;
}

bool qtractorPluginList::isActivatedAll (void) const
{
	return (m_iActivated > 0 && m_iActivated >= (unsigned int) count());
}


void qtractorPluginList::updateActivated ( bool bActivated )
{
	if (bActivated) {
		m_iActivated++;
	} else  {
		m_iActivated--;
	}
}


// Add-guarded plugin method.
void qtractorPluginList::addPlugin ( qtractorPlugin *pPlugin )
{
	// We'll get prepared before plugging it in...
	pPlugin->setChannels(m_iChannels);

	// Guess which item we're adding after...
	qtractorPlugin *pPrevPlugin = pPlugin->prev();
	if (pPrevPlugin == NULL && pPlugin->next() == NULL)
		pPrevPlugin = last();

	// Link the plugin into list...
	insertAfter(pPlugin, pPrevPlugin);

	// Now update each observer list-view...
	QListIterator<qtractorPluginListView *> iter(m_views);
	while (iter.hasNext()) {
		qtractorPluginListView *pListView = iter.next();
		// Get the previous one, if any...
		int iItem = pListView->pluginItem(pPrevPlugin) + 1;
		// Add the list-view item...
		qtractorPluginListItem *pItem = new qtractorPluginListItem(pPlugin);
		pListView->insertItem(iItem, pItem);
		pListView->setCurrentItem(pItem);
	}
}


// Remove-guarded plugin method.
void qtractorPluginList::removePlugin ( qtractorPlugin *pPlugin )
{
	// Just unlink the plugin from the list...
	unlink(pPlugin);
	pPlugin->setChannels(0);

	// Now update each observer list-view...
	QMutableListIterator<qtractorPluginListItem *> iter(pPlugin->items());
	while (iter.hasNext()) {
		qtractorPluginListItem *pItem = iter.next();
		iter.remove();
		delete pItem;
	}
}


// Move-guarded plugin method.
void qtractorPluginList::movePlugin ( qtractorPlugin *pPlugin,
	qtractorPlugin *pPrevPlugin )
{
	// Remove and insert back again...
	unlink(pPlugin);
	if (pPrevPlugin) {
		insertAfter(pPlugin, pPrevPlugin);
	} else {
		prepend(pPlugin);
	}

	// Now update each observer list-view...
	QList<qtractorPluginListItem *> items = pPlugin->items();
	QListIterator<qtractorPluginListItem *> iter(items);
	while (iter.hasNext()) {
		qtractorPluginListItem *pItem = iter.next();
		qtractorPluginListView *pListView
			= static_cast<qtractorPluginListView *> (pItem->listWidget());
		if (pListView) {
			int iPrevItem = pListView->pluginItem(pPrevPlugin);
			// Remove the old item...
			delete pItem;
			// Just insert under the track list position...
			pItem = new qtractorPluginListItem(pPlugin);
			pListView->insertItem(iPrevItem, pItem);
			pListView->setCurrentItem(pItem);
		}
	}
}


// An accessible list of observers.
QList<qtractorPluginListView *>& qtractorPluginList::views (void)
{
	return m_views;
}


// The meta-main audio-processing plugin-chain procedure.
void qtractorPluginList::process ( float **ppBuffer, unsigned int nframes )
{
	// Sanity checks...
	if (nframes > m_iBufferSize || m_pppBuffers[1] == NULL)
		return;

	// Start from first input buffer...
	m_pppBuffers[0] = ppBuffer;

	// Buffer binary iterator...
	unsigned short iBuffer = 0;

	// For each plugin in chain (in order, of course...)
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {

		// Must be properly activated...
		if (!pPlugin->isActivated())
			continue;

		// Must have a valid descriptor...
		const LADSPA_Descriptor *pDescriptor = pPlugin->descriptor();
		if (pDescriptor == NULL)
			continue;

		// Set proper buffers for this plugin...
		float **ppIBuffer = m_pppBuffers[  iBuffer & 1];
		float **ppOBuffer = m_pppBuffers[++iBuffer & 1];

		// We'll sweep channels...
		unsigned short iIChannel = 0;
		unsigned short iOChannel = 0;

		// For each plugin instance...
		for (unsigned short i = 0; i < pPlugin->instances(); i++) {
			// For each instance audio input port...
			QListIterator<unsigned long> iport(pPlugin->iports());
			while (iport.hasNext()) {
				(*pDescriptor->connect_port)(pPlugin->handle(i),
					iport.next(), ppIBuffer[iIChannel]);
				if (++iIChannel >= m_iChannels)
					iIChannel = 0;
			}
			// For each instance audio output port...
			QListIterator<unsigned long> oport(pPlugin->oports());
			while (oport.hasNext()) {
				(*pDescriptor->connect_port)(pPlugin->handle(i),
					oport.next(), ppOBuffer[iOChannel]);
				if (++iOChannel >= m_iChannels)
					iOChannel = 0;
			}
			// Wrap channels?...
			if (iIChannel < m_iChannels - 1)
				iIChannel++;
			if (iOChannel < m_iChannels - 1)
				iOChannel++;
		}

		// Time for the real thing...
		pPlugin->process(nframes);
	}

	// Now for the output buffer commitment...
	if (iBuffer & 1) {
		for (unsigned short i = 0; i < m_iChannels; i++) {
			::memcpy(ppBuffer[i], m_pppBuffers[1][i],
				nframes * sizeof(float));
		}
	}
}


// Document element methods.
bool qtractorPluginList::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	// Load plugin-list children...
	for (QDomNode nPlugin = pElement->firstChild();
			!nPlugin.isNull();
				nPlugin = nPlugin.nextSibling()) {
		// Convert clip node to element...
		QDomElement ePlugin = nPlugin.toElement();
		if (ePlugin.isNull())
			continue;
		if (ePlugin.tagName() == "plugin") {
			QString sFilename;
			unsigned long iIndex = 0;
			QString sPreset;
			QStringList vlist;
			bool bActivated = false;
			for (QDomNode nParam = ePlugin.firstChild();
					!nParam.isNull();
						nParam = nParam.nextSibling()) {
				// Convert buses list node to element...
				QDomElement eParam = nParam.toElement();
				if (eParam.isNull())
					continue;
				if (eParam.tagName() == "filename")
					sFilename = eParam.text();
				else
				if (eParam.tagName() == "index")
					iIndex = eParam.text().toULong();
				else
				if (eParam.tagName() == "preset")
					sPreset = eParam.text();
				else
				if (eParam.tagName() == "values")
					vlist = eParam.text().split(',');
				else
				if (eParam.tagName() == "activated")
					bActivated = pDocument->boolFromText(eParam.text());
			}
			if (sFilename.isEmpty())
				continue;
			qtractorPlugin *pPlugin
				= new qtractorPlugin(this, sFilename, iIndex);
			pPlugin->setPreset(sPreset);
			pPlugin->setValues(vlist);
			pPlugin->setActivated(bActivated);
			append(pPlugin);
		}
	}

	return true;
}


bool qtractorPluginList::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	// Save plugins...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		// Create the new plugin element...
		QDomElement ePlugin = pDocument->document()->createElement("plugin");
		pDocument->saveTextElement("filename",
			pPlugin->filename(), &ePlugin);
		pDocument->saveTextElement("index",
			QString::number(pPlugin->index()), &ePlugin);
		pDocument->saveTextElement("preset",
			pPlugin->preset(), &ePlugin);
		pDocument->saveTextElement("values",
			pPlugin->values().join(","), &ePlugin);
		pDocument->saveTextElement("activated",
			pDocument->textFromBool(pPlugin->isActivated()), &ePlugin);
		// Add this plugin...
		pElement->appendChild(ePlugin);
	}

	return true;
}


//----------------------------------------------------------------------------
// qtractorPluginPort -- Plugin input-control port instance.
//

// Constructors.
qtractorPluginPort::qtractorPluginPort (
	qtractorPlugin *pPlugin, unsigned long iIndex )
{
	m_pPlugin = pPlugin;
	m_iIndex  = iIndex;

	const LADSPA_Descriptor *pDescriptor
		= m_pPlugin->descriptor();
	m_portType = pDescriptor->PortDescriptors[m_iIndex];
	const LADSPA_PortRangeHint *pPortRangeHint
		= &(pDescriptor->PortRangeHints[m_iIndex]);
	m_portHints = pPortRangeHint->HintDescriptor;

	// Initialize range values...
	m_fMinValue = 0.0f;
	if (isBoundedBelow()) {
		m_fMinValue = pPortRangeHint->LowerBound;
		if (isSampleRate())
			m_fMinValue *= float(m_pPlugin->sampleRate());
	}

	m_fMaxValue = 4.0f;
	if (isBoundedAbove()) {
		m_fMaxValue = pPortRangeHint->UpperBound;
		if (isSampleRate())
			m_fMaxValue *= float(m_pPlugin->sampleRate());
	}

	// Port default value...
	m_fDefaultValue = 0.0f;
	switch (m_portHints & LADSPA_HINT_DEFAULT_MASK) {
	case LADSPA_HINT_DEFAULT_MINIMUM:
		m_fDefaultValue = m_fMinValue;
		break;
	case LADSPA_HINT_DEFAULT_LOW:
		if (isLogarithmic()) {
			m_fDefaultValue = ::expf(
				::logf(m_fMinValue) * 0.75f + ::logf(m_fMaxValue) * 0.25f);
		} else {
			m_fDefaultValue = (m_fMinValue * 0.75f + m_fMaxValue * 0.25f);
		}
		break;
	case LADSPA_HINT_DEFAULT_MIDDLE:
		if (isLogarithmic()) {
			m_fDefaultValue = ::sqrt(m_fMinValue * m_fMaxValue);
		} else {
			m_fDefaultValue = (m_fMinValue + m_fMaxValue) * 0.5f;
		}
		break;
	case LADSPA_HINT_DEFAULT_HIGH:
		if (isLogarithmic()) {
			m_fDefaultValue = ::expf(
				::logf(m_fMinValue) * 0.25f + ::logf(m_fMaxValue) * 0.75f);
		} else {
			m_fDefaultValue = (m_fMinValue * 0.25f + m_fMaxValue * 0.75f);
		}
		break;
	case LADSPA_HINT_DEFAULT_MAXIMUM:
		m_fDefaultValue = m_fMaxValue;
		break;
	case LADSPA_HINT_DEFAULT_0:
		m_fDefaultValue = 0.0f;
		break;
	case LADSPA_HINT_DEFAULT_1:
		m_fDefaultValue = 1.0f;
		break;
	case LADSPA_HINT_DEFAULT_100:
		m_fDefaultValue = 100.0f;
		break;
	case LADSPA_HINT_DEFAULT_440:
		m_fDefaultValue = 440.0f;
		break;
	}

	// Initialize port value...
	reset();
}

// Destructor.
qtractorPluginPort::~qtractorPluginPort (void)
{
}

// Main properties accessors.
qtractorPlugin *qtractorPluginPort::plugin (void) const
{
	return m_pPlugin;
}

unsigned long qtractorPluginPort::index (void) const
{
	return m_iIndex;
}

const char *qtractorPluginPort::name (void) const
{
	return (m_pPlugin->descriptor())->PortNames[m_iIndex];
}

LADSPA_PortRangeHintDescriptor qtractorPluginPort::hints (void) const
{
	return m_portHints;
}


// Port descriptor predicate methods.
bool qtractorPluginPort::isPortControlIn (void) const
{
	return LADSPA_IS_PORT_CONTROL(m_portType)
		&& LADSPA_IS_PORT_INPUT(m_portType); 
}

bool qtractorPluginPort::isPortControlOut (void) const
{
	return LADSPA_IS_PORT_CONTROL(m_portType)
		&& LADSPA_IS_PORT_OUTPUT(m_portType); 
}

bool qtractorPluginPort::isPortAudioIn (void) const
{
	return LADSPA_IS_PORT_AUDIO(m_portType)
		&& LADSPA_IS_PORT_INPUT(m_portType); 
}

bool qtractorPluginPort::isPortAudioOut (void) const
{
	return LADSPA_IS_PORT_AUDIO(m_portType)
		&& LADSPA_IS_PORT_OUTPUT(m_portType); 
}


// Port range hints predicate methods.
bool qtractorPluginPort::isBoundedBelow (void) const
{
	return LADSPA_IS_HINT_BOUNDED_BELOW(m_portHints);
}

bool qtractorPluginPort::isBoundedAbove (void) const
{
	return LADSPA_IS_HINT_BOUNDED_ABOVE(m_portHints);
}

bool qtractorPluginPort::isDefaultValue (void) const
{
	return LADSPA_IS_HINT_HAS_DEFAULT(m_portHints);
}

bool qtractorPluginPort::isLogarithmic (void) const
{
	return LADSPA_IS_HINT_LOGARITHMIC(m_portHints);
}

bool qtractorPluginPort::isSampleRate (void) const
{
	return LADSPA_IS_HINT_SAMPLE_RATE(m_portHints);
}

bool qtractorPluginPort::isInteger (void) const
{
	return LADSPA_IS_HINT_INTEGER(m_portHints);
}

bool qtractorPluginPort::isToggled (void) const
{
	return LADSPA_IS_HINT_TOGGLED(m_portHints);
}


// Bounding range values.
void qtractorPluginPort::setMinValue ( float fMinValue )
{
	m_fMinValue = fMinValue;
}

float qtractorPluginPort::minValue (void) const
{
	return m_fMinValue;
}


void qtractorPluginPort::setMaxValue ( float fMaxValue )
{
	m_fMaxValue = fMaxValue;
}

float qtractorPluginPort::maxValue (void) const
{
	return m_fMaxValue;
}


// Default value
void qtractorPluginPort::setDefaultValue ( float fDefaultValue )
{
	if (!isDefaultValue())
		return;

	if (isBoundedAbove() && fDefaultValue > m_fMaxValue)
		fDefaultValue = m_fMaxValue;
	else
	if (isBoundedBelow() && fDefaultValue < m_fMinValue)
		fDefaultValue = m_fMinValue;

	m_fDefaultValue = fDefaultValue;
}

float qtractorPluginPort::defaultValue (void) const
{
	return m_fDefaultValue;
}


// Current port value.
void qtractorPluginPort::setValue ( float fValue )
{
	if (fValue == m_fValue)
		return;

	if (isBoundedAbove() && fValue > m_fMaxValue)
		fValue = m_fMaxValue;
	else
	if (isBoundedBelow() && fValue < m_fMinValue)
		fValue = m_fMinValue;

	m_fValue = fValue;
}

float qtractorPluginPort::value (void) const
{
	return m_fValue;
}

float *qtractorPluginPort::data (void)
{
	return &m_fValue;
}



// Reset-to-default method.
void qtractorPluginPort::reset (void)
{
	m_fValue = m_fDefaultValue;
}


// end of qtractorPlugin.cpp
