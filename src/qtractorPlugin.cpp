// qtractorPlugin.cpp
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

#include "qtractorAbout.h"
#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginForm.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiBuffer.h"

#include "qtractorOptions.h"

#include "qtractorSessionDocument.h"

#ifdef CONFIG_LADSPA
#include "qtractorLadspaPlugin.h"
#endif
#ifdef CONFIG_DSSI
#include "qtractorDssiPlugin.h"
#endif
#ifdef CONFIG_VST
#include "qtractorVstPlugin.h"
#endif

#include <QDomDocument>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

typedef void (*qtractorPluginFile_Function)(void);

#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#define PATH_SEP ";"
#else
#define PATH_SEP ":"
#endif

#if defined(__x86_64__)
#define PATH_LIB "/lib64"
#else
#define PATH_LIB "/lib"
#endif

#define PATH_PRE1 "/usr/local" PATH_LIB
#define PATH_PRE2 "/usr" PATH_LIB

// Default plugin paths...
#ifdef CONFIG_LADSPA
#define LADSPA_PATH PATH_PRE1 "/ladspa" PATH_SEP PATH_PRE2 "/ladspa"
#endif

#ifdef CONFIG_DSSI
#define DSSI_PATH PATH_PRE1 "/dssi" PATH_SEP PATH_PRE2 "/dssi"
#endif

#ifdef CONFIG_VST
#define VST_PATH PATH_PRE1 "/vst" PATH_SEP PATH_PRE2 "/vst"
#endif


//----------------------------------------------------------------------------
// qtractorPluginPath -- Plugin path helper.
//

// Executive methods.
bool qtractorPluginPath::open (void)
{
	close();

	// Get paths based on hints...	
	QString sPaths;
#ifdef CONFIG_LADSPA
	// LADSPA default path...
	if (m_typeHint == qtractorPluginType::Any ||
		(m_typeHint == qtractorPluginType::Ladspa && m_paths.isEmpty())) {
		sPaths = ::getenv("LADSPA_PATH");
		if (sPaths.isEmpty())
			sPaths = LADSPA_PATH;
		if (!sPaths.isEmpty())
			m_paths << sPaths.split(PATH_SEP);
	}
#endif
#ifdef CONFIG_DSSI
	// DSSI default path...
	if (m_typeHint == qtractorPluginType::Any ||
		(m_typeHint == qtractorPluginType::Dssi && m_paths.isEmpty())) {
		sPaths = ::getenv("DSSI_PATH");
		if (sPaths.isEmpty())
			sPaths = DSSI_PATH;
		if (!sPaths.isEmpty())
			m_paths << sPaths.split(PATH_SEP);
	}
#endif
#ifdef CONFIG_VST
	// VST default path...
	if (m_typeHint == qtractorPluginType::Any ||
		(m_typeHint == qtractorPluginType::Vst && m_paths.isEmpty())) {
		sPaths = ::getenv("VST_PATH");
		if (sPaths.isEmpty())
			sPaths = VST_PATH;
		if (!sPaths.isEmpty())
			m_paths << sPaths.split(PATH_SEP);
	}
#endif

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginPath[%p]::open() paths=\"%s\"",
		this, m_paths.join(PATH_SEP).toUtf8().constData());
#endif

	QStringListIterator ipath(m_paths);
	while (ipath.hasNext()) {
		const QDir dir(ipath.next());
		const QStringList& list = dir.entryList(QDir::Files);
		QStringListIterator iter(list);
		while (iter.hasNext()) {
			const QString& sPath = dir.absoluteFilePath(iter.next());
			if (QLibrary::isLibrary(sPath))
				m_files.append(new qtractorPluginFile(sPath));
		}
	}

	return (m_files.count() > 0);
}


void qtractorPluginPath::close (void)
{
	qDeleteAll(m_files);
	m_files.clear();
}


// Helper methods.
void qtractorPluginPath::setPaths ( const QString& sPaths )
{
	m_paths = sPaths.split(PATH_SEP);
}


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

// Executive methods.
bool qtractorPluginFile::open (void)
{
	close();

	// ATTN: Not really needed, as it would be
	// loaded automagically on resolve()...
	if (!QLibrary::load())
		return false;

	// Do the openning dance...
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
	qtractorPluginFile_Function pfnInit
		= (qtractorPluginFile_Function) QLibrary::resolve("_init");
	if (pfnInit)
		(*pfnInit)();
#endif

	return true;
}


void qtractorPluginFile::close (void)
{
	if (!QLibrary::isLoaded())
		return;

	// Do the closing dance...
#if 0 // defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
	qtractorPluginFile_Function pfnFini
		= (qtractorPluginFile_Function) QLibrary::resolve("_fini");
	if (pfnFini)
		(*pfnFini)();
#endif

	QLibrary::unload();
}


// Plugin type listing.
bool qtractorPluginFile::getTypes ( qtractorPluginTypeList& types,
	qtractorPluginType::Hint typeHint )
{
	// Try to fill the types list at this moment...
	qtractorPluginType *pType;
	unsigned long iIndex = 0;

#ifdef CONFIG_DSSI
	// Try DSSI plugin types first...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Dssi) {
		while ((pType
			= qtractorDssiPluginType::createType(this, iIndex)) != NULL) {
			if (pType->open()) {
				types.append(pType);
				pType->close();
				iIndex++;
			} else {
				delete pType;
				break;
			}
		}
	}
	// Have we found some, already?
	if (iIndex > 0)
		return true;
#endif

#ifdef CONFIG_LADSPA
	// Try LADSPA plugin types...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Ladspa) {
		while ((pType
			= qtractorLadspaPluginType::createType(this, iIndex)) != NULL) {
			if (pType->open()) {
				types.append(pType);
				pType->close();
				iIndex++;
			} else {
				delete pType;
				break;
			}
		}
	}
	// Have we found some, already?
	if (iIndex > 0)
		return true;
#endif

#ifdef CONFIG_VST
	// Try VST plugin types...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Vst) {
		// Need to look at the options...
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bDummyVstScan)
			pType = qtractorDummyPluginType::createType(this);
		else
			pType = qtractorVstPluginType::createType(this);
		if (pType) {
			if (pType->open()) {
				types.append(pType);
				pType->close();
				iIndex++;
			} else {
				delete pType;
			}
		}
	}
#endif

	// Have we something?
	return (iIndex > 0);
}


// Plugin factory method (static).
qtractorPlugin *qtractorPluginFile::createPlugin (
	qtractorPluginList *pList,
	const QString& sFilename, unsigned long iIndex,
	qtractorPluginType::Hint typeHint )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginFile::createPlugin(%p, \"%s\", %lu, %d)",
		pList, sFilename.toUtf8().constData(), iIndex, int(typeHint));
#endif

	// Try to fill the types list at this moment...
	qtractorPluginFile *pFile = new qtractorPluginFile(sFilename);
	if (!pFile->open()) {
		delete pFile;
		return NULL;
	}

#ifdef CONFIG_DSSI
	// Try DSSI plugin types first...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Dssi) {
		qtractorDssiPluginType *pDssiType
			= qtractorDssiPluginType::createType(pFile, iIndex);
		if (pDssiType) {
			if (pDssiType->open())
				return new qtractorDssiPlugin(pList, pDssiType);
			delete pDssiType;
		}
	}
#endif

#ifdef CONFIG_LADSPA
	// Try LADSPA plugin types...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Ladspa) {
		qtractorLadspaPluginType *pLadspaType
			= qtractorLadspaPluginType::createType(pFile, iIndex);
		if (pLadspaType) {
			if (pLadspaType->open())
				return new qtractorLadspaPlugin(pList, pLadspaType);
			delete pLadspaType;
		}
	}
#endif

#ifdef CONFIG_VST
	// Try VST plugin types...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Vst) {
		qtractorVstPluginType *pVstType
			= qtractorVstPluginType::createType(pFile);
		if (pVstType) {
			if (pVstType->open())
				return new qtractorVstPlugin(pList, pVstType);
			delete pVstType;
		}
	}
#endif

	// Bad luck, no valid plugin found...
	delete pFile;
	return NULL;
}


//----------------------------------------------------------------------------
// qtractorPluginType -- Plugin type instance.
//

// Compute the number of instances needed
// for the given input/output audio channels.
unsigned short qtractorPluginType::instances (
	unsigned short iChannels, bool bMidi ) const
{
	unsigned short iInstances = 0;
	if (iChannels > 0 && m_iAudioOuts > 0
		&& ((bMidi && m_iMidiIns > 0) || !bMidi /* || m_iAudioIns > 0 */)) {
		if (iChannels >= m_iAudioIns)
			iInstances = (m_iAudioOuts >= iChannels ? 1 : iChannels);
		else
		if (m_iAudioOuts >= iChannels)
			iInstances = (iChannels >= m_iAudioIns ? iChannels : 1);
	}
	return iInstances;
}


// Plugin type(hint) textual helpers (static).
qtractorPluginType::Hint qtractorPluginType::hintFromText (
	const QString& sText )
{
#ifdef CONFIG_LADSPA
	if (sText == "LADSPA")
		return Ladspa;
	else
#endif
#ifdef CONFIG_DSSI
	if (sText == "DSSI")
		return Dssi;
	else
#endif
#ifdef CONFIG_VST
	if (sText == "VST")
		return Vst;
	else
#endif
	return Any;
}

QString qtractorPluginType::textFromHint (
	qtractorPluginType::Hint typeHint )
{
#ifdef CONFIG_LADSPA
	if (typeHint == Ladspa)
		return "LADSPA";
	else
#endif
#ifdef CONFIG_DSSI
	if (typeHint == Dssi)
		return "DSSI";
	else
#endif
#ifdef CONFIG_VST
	if (typeHint == Vst)
		return "VST";
	else
#endif
	return QObject::tr("(Any)");
}


//----------------------------------------------------------------------------
// qtractorDummyPluginType -- Dummy plugin type instance.
//

// Constructor.
qtractorDummyPluginType::qtractorDummyPluginType (
	qtractorPluginFile *pFile, unsigned long iIndex, Hint typeHint)
	: qtractorPluginType(pFile, iIndex, typeHint)
{
}

// Must be overriden methods.
bool qtractorDummyPluginType::open (void)
{
	m_sName  = QFileInfo(file()->filename()).baseName();
	m_sLabel = m_sName.simplified().replace(QRegExp("[\\s|\\.|\\-]+"), "_");

	m_iUniqueID = 0;
	for (int i = 0; i < m_sLabel.length(); ++i)
		m_iUniqueID += int(m_sLabel[i].toAscii());

	// Fake the rest...
	m_iAudioIns  = 2;
	m_iAudioOuts = 2;
	m_iMidiIns   = 1;

	return true;
}


void qtractorDummyPluginType::close (void)
{
}


// Factory method (static)
qtractorDummyPluginType *qtractorDummyPluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex, Hint typeHint )
{
	// Sanity check...
	if (pFile == NULL)
		return NULL;

	// Yep, most probably its a dummy plugin effect...
	return new qtractorDummyPluginType(pFile, iIndex, typeHint);
}


//----------------------------------------------------------------------------
// qtractorPlugin -- Plugin instance.
//

// Constructors.
qtractorPlugin::qtractorPlugin (
	qtractorPluginList *pList, qtractorPluginType *pType )
	: m_pList(pList), m_pType(pType), m_iInstances(0),
		m_bActivated(false), m_pForm(NULL)
{
#if 0
	// Open this...
	if (m_pType) {
		qtractorPluginFile *pFile = m_pType->file();
		if (pFile && pFile->open())
			m_pType->open();
	}
#endif
}


// Destructor.
qtractorPlugin::~qtractorPlugin (void)
{
	// Clear out all dependables...
	clearItems();

	// Clear out all dependables...
	qDeleteAll(m_params);
	m_params.clear();

	// Rest of stuff goes cleaned too...
	if (m_pType) {
		qtractorPluginFile *pFile = m_pType->file();
		delete m_pType;
		if (pFile)
			delete pFile;
	}
}


// Chain helper ones.
unsigned int qtractorPlugin::sampleRate (void) const
{
	return (m_pList ? m_pList->sampleRate() : 0);
}

unsigned int qtractorPlugin::bufferSize (void) const
{
	return (m_pList ? m_pList->bufferSize() : 0);
}

unsigned short qtractorPlugin::channels (void) const
{
	return (m_pList ? m_pList->channels() : 0);
}


// Set the internal instance count...
void qtractorPlugin::setInstances ( unsigned short iInstances )
{
	m_iInstances = iInstances;

	// Some sanity required here...
	if (m_iInstances < 1) {
		// We're sorry but dialogs must also go now...
		closeEditor();
		if (m_pForm) {
			m_pForm->close();
			delete m_pForm;
			m_pForm = NULL;
		}
	}
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


// Plugin state serialization methods.
void qtractorPlugin::setValueList ( const QStringList& vlist )
{
	// Split it up...
	m_values.clear();
	QStringListIterator val(vlist);
	for (unsigned long iIndex = 0; val.hasNext(); ++iIndex)
		m_values[iIndex] = val.next().toFloat();
}

QStringList qtractorPlugin::valueList (void) const
{
	// Join it up...
	QStringList vlist;
	QListIterator<qtractorPluginParam *> iter(m_params);
	while (iter.hasNext()) {
		qtractorPluginParam *pParam = iter.next();
		int iIndex = pParam->index();
		while (vlist.count() < iIndex + 1)
			vlist.append(QString());
		vlist[iIndex] = QString::number(pParam->value());
	}

	return vlist;
}


// Reset-to-default method.
void qtractorPlugin::reset (void)
{
	QListIterator<qtractorPluginParam *> iter(m_params);
	while (iter.hasNext())
		iter.next()->reset();
}


// List of observers management.
void qtractorPlugin::addItem ( qtractorPluginListItem *pItem )
{
	m_items.append(pItem);
}


void qtractorPlugin::removeItem ( qtractorPluginListItem *pItem )
{
	int iItem = m_items.indexOf(pItem);
	if (iItem >= 0)
		m_items.removeAt(iItem);
}


void qtractorPlugin::clearItems (void)
{
	qDeleteAll(m_items);
	m_items.clear();
}


// Special plugin form accessors.
bool qtractorPlugin::isFormVisible (void) const
{
	return (m_pForm ? m_pForm->isVisible() : false);
}

qtractorPluginForm *qtractorPlugin::form (void)
{
	// Take the change and create the form if it doesn't current exist.
	if (m_pForm == NULL) {
		// Build up the plugin form...
		// What style do we create tool childs?
		QWidget *pParent = NULL;
		Qt::WindowFlags wflags = Qt::Window
		#if QT_VERSION >= 0x040200
			| Qt::CustomizeWindowHint
		#endif
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowMinMaxButtonsHint;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bKeepToolsOnTop) {
		//	pParent = qtractorMainForm::getInstance();
			wflags |= Qt::Tool;
		}
		// Do it...
		m_pForm = new qtractorPluginForm(pParent, wflags);
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


// Plugin preset group - common identification group/prefix.
QString qtractorPlugin::presetGroup (void) const
{
	return "/Plugin/" + presetPrefix();
}

// Normalize plugin identification prefix...
QString qtractorPlugin::presetPrefix (void) const
{
	return m_pType->label() + '_' + QString::number(m_pType->uniqueID());
}


// Load plugin preset from xml file.
bool qtractorPlugin::loadPreset ( const QString& sFilename )
{
	// Open file...
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	// Parse it a-la-DOM :-)
	QDomDocument doc("qtractorPlugin");
	if (!doc.setContent(&file)) {
		file.close();
		return false;
	}
	file.close();

	// Get root element.
	QDomElement ePreset = doc.documentElement();
	if (ePreset.tagName() != "preset")
		return false;
	// Check if it's on the correct plugin preset...
	if (ePreset.attribute("type") != presetPrefix())
		return false;

	// Reset any old configs.
	clearConfigs();
	clearValues();

	// Now parse for children...
	for (QDomNode nChild = ePreset.firstChild();
			!nChild.isNull(); nChild = nChild.nextSibling()) {

		// Convert node to element, if any.
		QDomElement eChild = nChild.toElement();
		if (eChild.isNull())
			continue;

		// Check for preset item...
		if (eChild.tagName() == "configs") {
			// Parse for config entries...
			for (QDomNode nConfig = eChild.firstChild();
					!nConfig.isNull(); nConfig = nConfig.nextSibling()) {
				// Convert node to element, if any.
				QDomElement eConfig = nConfig.toElement();
				if (eConfig.isNull())
					continue;
				// Check for config item...
				if (eConfig.tagName() == "config")
					m_configs[eConfig.attribute("key")] = eConfig.text();
			}
		}
		else
		if (eChild.tagName() == "params") {
			// Parse for param entries...
			for (QDomNode nParam = eChild.firstChild();
					!nParam.isNull(); nParam = nParam.nextSibling()) {
				// Convert node to element, if any.
				QDomElement eParam = nParam.toElement();
				if (eParam.isNull())
					continue;
				// Check for config item...
				if (eParam.tagName() == "param")
					m_values[eParam.attribute("index").toULong()]
						= eParam.text().toFloat();
			}
		}
	}

	// Make it real.
	realizeConfigs();
	realizeValues();

	releaseConfigs();

	return true;
}


// Save plugin preset to xml file.
bool qtractorPlugin::savePreset ( const QString& sFilename )
{
	QFileInfo fi(sFilename);

	QDomDocument doc("qtractorPlugin");

	QDomElement ePreset = doc.createElement("preset");
	ePreset.setAttribute("type", presetPrefix());
	ePreset.setAttribute("name", fi.baseName());
	ePreset.setAttribute("version", QTRACTOR_VERSION);

	// Save plugin configs...
	freezeConfigs();
	QDomElement eConfigs = doc.createElement("configs");
	Configs::ConstIterator iter = m_configs.constBegin();
	for (; iter != m_configs.constEnd(); ++iter) {
		QDomElement eConfig = doc.createElement("config");
		eConfig.setAttribute("key", iter.key());
		eConfig.appendChild(
			doc.createTextNode(iter.value()));
		eConfigs.appendChild(eConfig);
	}
	ePreset.appendChild(eConfigs);
	releaseConfigs();

	// Save plugin params...
	QDomElement eParams = doc.createElement("params");
	QListIterator<qtractorPluginParam *> param(m_params);
	while (param.hasNext()) {
		qtractorPluginParam *pParam = param.next();
		QDomElement eParam = doc.createElement("param");
		eParam.setAttribute("name", pParam->name());
		eParam.setAttribute("index", QString::number(pParam->index()));
		eParam.appendChild(
			doc.createTextNode(QString::number(pParam->value())));
		eParams.appendChild(eParam);
	}
	ePreset.appendChild(eParams);

	doc.appendChild(ePreset);

	// Finally, we're ready to save to external file.
	QFile file(sFilename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	QTextStream ts(&file);
	ts << doc.toString() << endl;
	file.close();

	return true;
}


// Plugin parameter lookup.
qtractorPluginParam *qtractorPlugin::findParam ( unsigned long iIndex ) const
{
	QListIterator<qtractorPluginParam *> iter(m_params);
	while (iter.hasNext()) {
		qtractorPluginParam *pParam = iter.next();
		if (pParam->index() == iIndex)
			return pParam;
	}

	return NULL;
}


// Plugin configure realization.
void qtractorPlugin::realizeConfigs (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorPlugin[%p]::realizeConfigs()", this);
#endif

	// Set configuration (CLOBs)...
	Configs::ConstIterator config = m_configs.constBegin();
	for (; config != m_configs.constEnd(); ++config)
		configure(config.key(), config.value());

	// Set proper bank/program selection...
	qtractorMidiManager *pMidiManager = m_pList->midiManager();
	if (pMidiManager)
		selectProgram(pMidiManager->currentBank(), pMidiManager->currentProg());

	// (Re)set parameter values (initial)...
	Values::ConstIterator param = m_values.constBegin();
	for (; param != m_values.constEnd(); ++param) {
		qtractorPluginParam *pParam = findParam(param.key());
		if (pParam)
			pParam->setValue(param.value());
	}
}


// Plugin parameter realization.
void qtractorPlugin::realizeValues (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorPlugin[%p]::realizeValues()", this);
#endif

	// (Re)set parameter values (initial)...
	Values::ConstIterator param = m_values.constBegin();
	for (; param != m_values.constEnd(); ++param) {
		qtractorPluginParam *pParam = findParam(param.key());
		if (pParam)
			pParam->setValue(param.value());
	}
}


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

// Constructor.
qtractorPluginList::qtractorPluginList ( unsigned short iChannels,
	unsigned int iBufferSize, unsigned int iSampleRate, unsigned int iFlags )
	: m_iChannels(0), m_iBufferSize(0), m_iSampleRate(0),
		m_iFlags(0), m_iActivated(0), m_pMidiManager(NULL)
		
{
	m_pppBuffers[0] = NULL;
	m_pppBuffers[1] = NULL;

	setBuffer(iChannels, iBufferSize, iSampleRate, iFlags);
}

// Destructor.
qtractorPluginList::~qtractorPluginList (void)
{
	// Reset allocated channel buffers.
	setBuffer(0, 0, 0, 0);

	// Now that's one manager less.
	if (m_pMidiManager)
		qtractorMidiManager::deleteMidiManager(m_pMidiManager);

	// Clear out all dependables...
	m_views.clear();
}


// The title to show up on plugin forms...
void qtractorPluginList::setName ( const QString& sName )
{
	m_sName = sName;

	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isFormVisible())
			(pPlugin->form())->updateCaption();
	}

	if (m_pMidiManager)
		m_pMidiManager->resetAudioOutputBus();
}


// Main-parameters accessor.
void qtractorPluginList::setBuffer ( unsigned short iChannels,
	unsigned int iBufferSize, unsigned int iSampleRate, unsigned int iFlags )
{
	unsigned short i;

	// Delete old interim buffer...
	if (m_pppBuffers[1]) {
		for (i = 0; i < m_iChannels; i++)
			delete [] m_pppBuffers[1][i];
		delete [] m_pppBuffers[1];
		m_pppBuffers[1] = NULL;
	}

	// Set proper flags at once.
	m_iFlags = iFlags;

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

	// FIXME: This should be better managed...
	if (m_pMidiManager)
		m_pMidiManager->updateInstruments();
}


// Reset and (re)activate all plugin chain.
void qtractorPluginList::resetBuffer (void)
{
	// Save and reset activation count...
	int iActivated = m_iActivated;
	m_iActivated = 0;

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

	// Restore activation count.
	m_iActivated = iActivated;
}


// Special guard activation methods.
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
	// Link the plugin into list...
	insertPlugin(pPlugin, pPlugin->next());
}


// Insert-guarded plugin method.
void qtractorPluginList::insertPlugin ( qtractorPlugin *pPlugin,
	qtractorPlugin *pNextPlugin )
{
	// We'll get prepared before plugging it in...
	pPlugin->setChannels(m_iChannels);

	addPluginRef(pPlugin);
	if (pNextPlugin)
		insertBefore(pPlugin, pNextPlugin);
	else
		append(pPlugin);

	if (pPlugin->isActivated())
		updateActivated(true);

	// Now update each observer list-view...
	QListIterator<qtractorPluginListView *> iter(m_views);
	while (iter.hasNext()) {
		qtractorPluginListView *pListView = iter.next();
		int iNextItem = pListView->count();
		if (pNextPlugin)
			iNextItem = pListView->pluginItem(pNextPlugin);
		qtractorPluginListItem *pNextItem = new qtractorPluginListItem(pPlugin);
		pListView->insertItem(iNextItem, pNextItem);
		pListView->setCurrentItem(pNextItem);
	}
}


// Move-guarded plugin method.
void qtractorPluginList::movePlugin (
	qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin )
{
	// Source sanity...
	if (pPlugin == NULL)
		return;

	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == NULL)
		return;

	// Remove and insert back again...
	pPluginList->unlink(pPlugin);
	pPluginList->removePluginRef(pPlugin);

	addPluginRef(pPlugin);
	if (pNextPlugin) {
		insertBefore(pPlugin, pNextPlugin);
	} else {
		append(pPlugin);
	}

	// DANGER: Gasp, we might be not the same...
	if (pPluginList != this) {
		pPlugin->setPluginList(this);
		pPlugin->setChannels(channels());
		if (pPlugin->isActivated()) {
			pPluginList->updateActivated(false);
			updateActivated(true);
		}
	}

	// Now update each observer list-view:
	// - take all items...
	QListIterator<qtractorPluginListItem *> item(pPlugin->items());
	while (item.hasNext())
		delete item.next();
	// - give them back into the right position...
	QListIterator<qtractorPluginListView *> view(m_views);
	while (view.hasNext()) {
		qtractorPluginListView *pListView = view.next();
		int iNextItem = pListView->count();
		if (pNextPlugin)
			iNextItem = pListView->pluginItem(pNextPlugin);
		qtractorPluginListItem *pNextItem
			= new qtractorPluginListItem(pPlugin);
		pListView->insertItem(iNextItem, pNextItem);
		pListView->setCurrentItem(pNextItem);
	}
}


// Remove-guarded plugin method.
void qtractorPluginList::removePlugin ( qtractorPlugin *pPlugin )
{
	// Just unlink the plugin from the list...
	unlink(pPlugin);
	removePluginRef(pPlugin);

	if (pPlugin->isActivated())
		updateActivated(false);

	pPlugin->setChannels(0);
	pPlugin->clearItems();
}


// Clone/copy plugin method.
qtractorPlugin *qtractorPluginList::copyPlugin ( qtractorPlugin *pPlugin )
{
	qtractorPluginType *pType = pPlugin->type();
	if (pType == NULL)
		return NULL;

	// Clone the plugin instance...
	pPlugin->freezeConfigs();

	// MIDI bank program whether necessary...
	int iBank = 0;
	int iProg = 0;
	if (m_pMidiManager && m_pMidiManager->currentBank() >= 0)
		iBank = m_pMidiManager->currentBank();
	if (m_pMidiManager && m_pMidiManager->currentProg() >= 0)
		iProg = m_pMidiManager->currentProg();

	qtractorPlugin *pNewPlugin = qtractorPluginFile::createPlugin(this,
		(pType->file())->filename(), pType->index(), pType->typeHint());
	if (pNewPlugin) {
		pNewPlugin->setPreset(pPlugin->preset());
		pNewPlugin->setConfigs(pPlugin->configs());
		pNewPlugin->setValues(pPlugin->values());
		pNewPlugin->setActivated(pPlugin->isActivated());
	}

	pPlugin->releaseConfigs();

	return pNewPlugin;
}


// Plugin management helpers.
void qtractorPluginList::addPluginRef ( qtractorPlugin *pPlugin )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginList[%p]::addPluginRef(%p)", this, pPlugin);
#endif
	if ((m_iFlags & Midi) && m_pMidiManager == NULL && count() == 0)
		m_pMidiManager = qtractorMidiManager::createMidiManager(this);

	if (m_pMidiManager)
		m_pMidiManager->addPluginRef(pPlugin);
}

void qtractorPluginList::removePluginRef ( qtractorPlugin *pPlugin )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginList[%p]::removePluginRef(%p)", this, pPlugin);
#endif
	if (m_pMidiManager) {
		m_pMidiManager->removePluginRef(pPlugin);
		if (count() == 0) {
			qtractorMidiManager::deleteMidiManager(m_pMidiManager);
			m_pMidiManager = NULL;
		}
	}
}


// List of views management.
void qtractorPluginList::addView ( qtractorPluginListView *pView )
{
	m_views.append(pView);
}


void qtractorPluginList::removeView ( qtractorPluginListView *pView )
{
	int iView = m_views.indexOf(pView);
	if (iView >= 0)
		m_views.removeAt(iView);
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

		// Set proper buffers for this plugin...
		float **ppIBuffer = m_pppBuffers[  iBuffer & 1];
		float **ppOBuffer = m_pppBuffers[++iBuffer & 1];

		// Time for the real thing...
		pPlugin->process(ppIBuffer, ppOBuffer, nframes);
	}

	// Now for the output buffer commitment...
	if (iBuffer & 1) {
		for (unsigned short i = 0; i < m_iChannels; ++i) {
			::memcpy(ppBuffer[i], m_pppBuffers[1][i],
				nframes * sizeof(float));
		}
	}
}


// Document element methods.
bool qtractorPluginList::loadElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	int iBank = 0;
	int iProg = 0;

	// Load plugin-list children...
	for (QDomNode nPlugin = pElement->firstChild();
			!nPlugin.isNull();
				nPlugin = nPlugin.nextSibling()) {

		// Convert clip node to element...
		QDomElement ePlugin = nPlugin.toElement();
		if (ePlugin.isNull())
			continue;
		if (ePlugin.tagName() == "bank")
			iBank = ePlugin.text().toInt();
		else
		if (ePlugin.tagName() == "program")
			iProg = ePlugin.text().toInt();
		else
		if (ePlugin.tagName() == "plugin") {
			QString sFilename;
			unsigned long iIndex = 0;
			QString sPreset;
			QStringList vlist;
			bool bActivated = false;
			qtractorPlugin::Configs configs;
			qtractorPluginType::Hint typeHint
				= qtractorPluginType::hintFromText(
					ePlugin.attribute("type"));
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
				else
				if (eParam.tagName() == "configure") {
					// Load plugin configuration stuff (CLOB)...
					for (QDomNode nConfig = eParam.firstChild();
							!nConfig.isNull();
								nConfig = nConfig.nextSibling()) {
						// Convert config node to element...
						QDomElement eConfig = nConfig.toElement();
						if (eConfig.isNull())
							continue;
						if (eConfig.tagName() == "config")
							configs[eConfig.attribute("key")] = eConfig.text();
					}
				}
			}
			if (sFilename.isEmpty())
				continue;
			qtractorPlugin *pPlugin
				= qtractorPluginFile::createPlugin(this,
					sFilename, iIndex, typeHint);
			if (pPlugin) {
				pPlugin->setPreset(sPreset);
				pPlugin->setConfigs(configs);
				pPlugin->setValueList(vlist);
				pPlugin->setActivated(bActivated);
				addPluginRef(pPlugin);
				append(pPlugin);
			}
		}
		else
		// Load audio output bus flag...
		if (ePlugin.tagName() == "audio-output-bus") {
			if (m_pMidiManager) {
				m_pMidiManager->setAudioOutputBus(
					pDocument->boolFromText(ePlugin.text()));
			}
		}
		else
		// Load audio output connections...
		if (ePlugin.tagName() == "audio-outputs") {
			if (m_pMidiManager && m_pMidiManager->isAudioOutputBus()) {
				qtractorAudioBus *pAudioBus = m_pMidiManager->audioOutputBus();
				if (pAudioBus) {
					pAudioBus->loadConnects(
						pAudioBus->outputs(), pDocument, &ePlugin);
				}
			}
		}
	}

	// Set current bank/program properly...
	if (m_pMidiManager) {
		m_pMidiManager->setCurrentBank(iBank);
		m_pMidiManager->setCurrentProg(iProg);
	}

	return true;
}


bool qtractorPluginList::saveElement ( qtractorSessionDocument *pDocument,
	QDomElement *pElement )
{
	// Save current MIDI bank/program setting...
	if (m_pMidiManager && m_pMidiManager->currentBank() >= 0)
		pDocument->saveTextElement("bank",
			QString::number(m_pMidiManager->currentBank()), pElement);
	if (m_pMidiManager && m_pMidiManager->currentProg() >= 0)
		pDocument->saveTextElement("program",
			QString::number(m_pMidiManager->currentProg()), pElement);

	// Save plugins...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {

		// Do freeze plugin state...
		pPlugin->freezeConfigs();

		// Create the new plugin element...
		QDomElement ePlugin = pDocument->document()->createElement("plugin");
		ePlugin.setAttribute("type", qtractorPluginType::textFromHint(
			(pPlugin->type())->typeHint()));
		pDocument->saveTextElement("filename",
			((pPlugin->type())->file())->filename(), &ePlugin);
		pDocument->saveTextElement("index",
			QString::number((pPlugin->type())->index()), &ePlugin);
		pDocument->saveTextElement("preset",
			pPlugin->preset(), &ePlugin);
		pDocument->saveTextElement("values",
			pPlugin->valueList().join(","), &ePlugin);
		pDocument->saveTextElement("activated",
			pDocument->textFromBool(pPlugin->isActivated()), &ePlugin);
		// Plugin configuration stuff (CLOB)...
		QDomElement eConfigs
			= pDocument->document()->createElement("configure");
		const qtractorPlugin::Configs& configs = pPlugin->configs();
		qtractorPlugin::Configs::ConstIterator iter = configs.constBegin();
		for ( ; iter != configs.constEnd(); ++iter) {
			QDomElement eConfig
				= pDocument->document()->createElement("config");
			eConfig.setAttribute("key", iter.key());
			eConfig.appendChild(
				pDocument->document()->createTextNode(iter.value()));
			eConfigs.appendChild(eConfig);
		}
		ePlugin.appendChild(eConfigs);
		// Add this plugin...
		pElement->appendChild(ePlugin);

		// May release plugin state...
		pPlugin->releaseConfigs();
	}

	// Save audio output-bus connects...
	if (m_pMidiManager) {
		pDocument->saveTextElement("audio-output-bus",
			pDocument->textFromBool(m_pMidiManager->isAudioOutputBus()), pElement);
		if (m_pMidiManager->isAudioOutputBus()) {
			qtractorAudioBus *pAudioBus = m_pMidiManager->audioOutputBus();
			if (pAudioBus) {
				QDomElement eOutputs
					= pDocument->document()->createElement("audio-outputs");
				qtractorBus::ConnectList outputs;
				pAudioBus->updateConnects(qtractorBus::Output, outputs);
				pAudioBus->saveConnects(outputs, pDocument, &eOutputs);
				pElement->appendChild(eOutputs);
			}
		}
	}

	return true;
}


//----------------------------------------------------------------------------
// qtractorPluginParam -- Plugin parameter (control input port) instance.
//

// Default value
void qtractorPluginParam::setDefaultValue ( float fDefaultValue )
{
//	if (!isDefaultValue())
//		return;

	if (isBoundedAbove() && fDefaultValue > m_fMaxValue)
		fDefaultValue = m_fMaxValue;
	else
	if (isBoundedBelow() && fDefaultValue < m_fMinValue)
		fDefaultValue = m_fMinValue;

	m_fDefaultValue = fDefaultValue;
}


// Current port value.
void qtractorPluginParam::setValue ( float fValue )
{
	if (isBoundedAbove() && fValue > m_fMaxValue)
		fValue = m_fMaxValue;
	else
	if (isBoundedBelow() && fValue < m_fMinValue)
		fValue = m_fMinValue;

	if (fValue == m_fValue)
		return;

	m_fValue = fValue;

	// Update specifics.
	m_pPlugin->updateParam(this, m_fValue);
}


// end of qtractorPlugin.cpp
