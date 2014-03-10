// qtractorPlugin.cpp
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

#include "qtractorAbout.h"
#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"
#include "qtractorPluginForm.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiBuffer.h"

#include "qtractorOptions.h"

#include "qtractorSession.h"
#include "qtractorDocument.h"
#include "qtractorCurveFile.h"

#ifdef CONFIG_LADSPA
#include "qtractorLadspaPlugin.h"
#endif
#ifdef CONFIG_DSSI
#include "qtractorDssiPlugin.h"
#endif
#ifdef CONFIG_VST
#include "qtractorVstPlugin.h"
#endif
#ifdef CONFIG_LV2
#include "qtractorLv2Plugin.h"
#endif

#include "qtractorInsertPlugin.h"

#include <QTextStream>
#include <QFileInfo>
#include <QDir>

#include <QDomDocument>

#include <math.h>


#if QT_VERSION < 0x040500
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif


// A common scheme for (a default) plugin serach paths...
//
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#define PATH_SEP ";"
#else
#define PATH_SEP ":"
#endif

static QString default_paths ( const QString& suffix )
{
	const QString& sep  = QDir::separator();

	const QString& home = QDir::homePath();

	const QString& pre1 = QDir::rootPath() + "usr";
	const QString& pre2 = pre1 + sep + "local";

	const QString& lib0 = "lib";
	const QString& lib1 = pre1 + sep + lib0;
	const QString& lib2 = pre2 + sep + lib0;

#if defined(__x86_64__)
	const QString& x64  = "64";
	const QString& lib3 = lib1 + x64;
	const QString& lib4 = lib2 + x64;
#endif

	QStringList paths;

	paths << home + sep + '.' + suffix;

#if defined(__x86_64__)
//	paths << home + sep + lib0 + x64 + sep + suffix;
	paths << lib4 + sep + suffix;
	paths << lib3 + sep + suffix;
#endif

//	paths << home + sep + lib0 + sep + suffix;
	paths << lib2 + sep + suffix;
	paths << lib1 + sep + suffix;

	return paths.join(PATH_SEP);
}


// A common function for special platforms...
//
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
typedef void (*qtractorPluginFile_Function)(void);
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
	QStringList paths;
	QStringList path_list;
#ifdef CONFIG_LADSPA
	// LADSPA default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Ladspa) {
		paths = m_paths.value(qtractorPluginType::Ladspa);
		if (paths.isEmpty()) {
			sPaths = ::getenv("LADSPA_PATH");
			if (sPaths.isEmpty())
				sPaths = default_paths("ladspa");
			if (!sPaths.isEmpty()) {
				paths = sPaths.split(PATH_SEP);
				m_paths.insert(qtractorPluginType::Ladspa, paths);
			}
		}
		path_list.append(paths);
	}
#endif
#ifdef CONFIG_DSSI
	// DSSI default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Dssi) {
		paths = m_paths.value(qtractorPluginType::Dssi);
		if (paths.isEmpty()) {
			sPaths = ::getenv("DSSI_PATH");
			if (sPaths.isEmpty())
				sPaths = default_paths("dssi");
			if (!sPaths.isEmpty()) {
				paths = sPaths.split(PATH_SEP);
				m_paths.insert(qtractorPluginType::Dssi, paths);
			}
		}
		path_list.append(paths);
	}
#endif
#ifdef CONFIG_VST
	// VST default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Vst) {
		paths = m_paths.value(qtractorPluginType::Vst);
		if (paths.isEmpty()) {
			sPaths = ::getenv("VST_PATH");
			if (sPaths.isEmpty())
				sPaths = default_paths("vst");
			if (!sPaths.isEmpty()) {
				paths = sPaths.split(PATH_SEP);
				m_paths.insert(qtractorPluginType::Vst, paths);
			}
		}
		path_list.append(paths);
	}
#endif
#ifdef CONFIG_LV2
	// LV2 default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Lv2) {
		paths = m_paths.value(qtractorPluginType::Lv2);
		if (paths.isEmpty()) {
			sPaths = ::getenv("LV2_PATH");
			if (sPaths.isEmpty())
				sPaths = default_paths("lv2");
			if (!sPaths.isEmpty()) {
				paths = sPaths.split(PATH_SEP);
				m_paths.insert(qtractorPluginType::Lv2, paths);
			}
		}
		path_list.append(paths);
	#ifdef CONFIG_LV2_PRESETS
		QString sPresetDir;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions)
			sPresetDir = pOptions->sLv2PresetDir;
		if (sPresetDir.isEmpty())
			sPresetDir = QDir::homePath() + QDir::separator() + ".lv2";
		if (!paths.contains(sPresetDir))
			paths.append(sPresetDir);
	#endif
		// HACK: set special environment for LV2...
		::setenv("LV2_PATH", paths.join(PATH_SEP).toUtf8().constData(), 1);
	}
#endif

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginPath[%p]::open() paths=\"%s\" typeHint=%d",
		this, path_list.join(PATH_SEP).toUtf8().constData(), int(m_typeHint));
#endif

	QStringListIterator path_iter(path_list);
	while (path_iter.hasNext()) {
		const QDir dir(path_iter.next());
		const QStringList& file_list = dir.entryList(QDir::Files);
		QStringListIterator file_iter(file_list);
		while (file_iter.hasNext()) {
			const QString& sPath = dir.absoluteFilePath(file_iter.next());
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
void qtractorPluginPath::setPaths (
	qtractorPluginType::Hint typeHint, const QString& sPaths )
{
	m_paths.insert(typeHint, sPaths.split(PATH_SEP));
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

	if (m_bAutoUnload) QLibrary::unload();
}


// Plugin type listing.
bool qtractorPluginFile::getTypes ( qtractorPluginPath& path,
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
				path.addType(pType);
				pType->close();
				++iIndex;
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
				path.addType(pType);
				pType->close();
				++iIndex;
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
				path.addType(pType);
				pType->close();
				++iIndex;
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

	// Attend to insert pseudo-plugin hints...
	if (sFilename.isEmpty()) {
		if (typeHint == qtractorPluginType::Insert) {
			qtractorInsertPluginType *pInsertType
				= qtractorInsertPluginType::createType(iIndex);
			if (pInsertType) {
				if (pInsertType->open())
					return new qtractorInsertPlugin(pList, pInsertType);
				delete pInsertType;
			}
		}
		else
		if (typeHint == qtractorPluginType::AuxSend) {
			qtractorAuxSendPluginType *pAuxSendType
				= qtractorAuxSendPluginType::createType(iIndex);
			if (pAuxSendType) {
				if (pAuxSendType->open())
					return new qtractorAuxSendPlugin(pList, pAuxSendType);
				delete pAuxSendType;
			}
		}
		// Don't bother with anything else.
		return NULL;
	}

#ifdef CONFIG_LV2
	// Try LV2 plugins hints before anything else...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Lv2) {
		qtractorLv2PluginType *pLv2Type
			= qtractorLv2PluginType::createType(sFilename);
		if (pLv2Type) {
			if (pLv2Type->open())
				return new qtractorLv2Plugin(pList, pLv2Type);
			delete pLv2Type;
		}
	}
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

#ifdef CONFIG_LV2
	// Try LV2 plugin types...
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Lv2) {
		qtractorLv2PluginType *pLv2Type
			= qtractorLv2PluginType::createType(sFilename);
		if (pLv2Type) {
			if (pLv2Type->open())
				return new qtractorLv2Plugin(pList, pLv2Type);
			delete pLv2Type;
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

// Plugin filename accessor (default virtual).
QString qtractorPluginType::filename (void) const
{
	return (m_pFile ? m_pFile->filename() : QString());
}


// Compute the number of instances needed
// for the given input/output audio channels.
unsigned short qtractorPluginType::instances (
	unsigned short iChannels, bool bMidi ) const
{
	unsigned short iInstances = 0;
	if (iChannels > 0) {
		if (bMidi && m_iMidiOuts > 0)
			iInstances = 1;
		else
		if (iChannels >= m_iAudioIns && m_iAudioOuts > 0)
			iInstances = (m_iAudioOuts >= iChannels ? 1 : iChannels);
		else
		if (m_iAudioOuts >= iChannels)
			iInstances = (iChannels >= m_iAudioIns ? iChannels : 1);
		else
		if (bMidi && m_iMidiIns > 0)
			iInstances = 1;
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
#ifdef CONFIG_LV2
	if (sText == "LV2")
		return Lv2;
	else
#endif
	if (sText == "Insert")
		return Insert;
	else
	if (sText == "AuxSend")
		return AuxSend;
	else
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
#ifdef CONFIG_LV2
	if (typeHint == Lv2)
		return "LV2";
	else
#endif
	if (typeHint == Insert)
		return "Insert";
	else
	if (typeHint == AuxSend)
		return "AuxSend";
	else
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
	m_sName  = QFileInfo(filename()).baseName();
	m_sLabel = m_sName.simplified().replace(QRegExp("[\\s|\\.|\\-]+"), "_");

	m_iUniqueID = qHash(m_sLabel);

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


// Instance cached-deferred accesors.
const QString& qtractorDummyPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty()) {
		m_sAboutText  = QTRACTOR_TITLE;
		m_sAboutText += ' ';
		m_sAboutText += QObject::tr("Dummy plugin type.");
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_WEBSITE;
		m_sAboutText += '\n';
		m_sAboutText += QTRACTOR_COPYRIGHT;
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorPlugin -- Plugin instance.
//

// Constructors.
qtractorPlugin::qtractorPlugin (
	qtractorPluginList *pList, qtractorPluginType *pType )
	: m_pList(pList), m_pType(pType), m_iUniqueID(0), m_iInstances(0),
		m_bActivated(false), m_pForm(NULL), m_iDirectAccessParamIndex(-1)
{
	// Acquire a local unique id in chain...
	if (m_pList && m_pType)
		m_iUniqueID = m_pList->createUniqueID(m_pType);
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
	// Some sanity required here...
	if (iInstances < 1) {
		// We're sorry but dialogs must also go now...
		closeEditor();
		if (m_pForm) {
			m_pForm->close();
			delete m_pForm;
			m_pForm = NULL;
		}
	}

	m_iInstances = iInstances;
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
//	qSort(m_params); -- does not work with QHash...
	QMap<unsigned long, qtractorPluginParam *> params;
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator& param_end = m_params.constEnd();
	for ( ; param != param_end; ++param)
		params.insert(param.key(), param.value());

	// Split it up...
	clearValues();
	QStringListIterator val(vlist);
	QMapIterator<unsigned long, qtractorPluginParam *> iter(params);
	while (val.hasNext() && iter.hasNext())
		m_values.index[iter.next().key()] = val.next().toFloat();
}

QStringList qtractorPlugin::valueList (void) const
{
//	qSort(m_params); -- does not work with QHash...
	QMap<unsigned long, qtractorPluginParam *> params;
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator& param_end = m_params.constEnd();
	for ( ; param != param_end; ++param)
		params.insert(param.key(), param.value());

	// Join it up...
	QStringList vlist;
	QMapIterator<unsigned long, qtractorPluginParam *> iter(params);
	while (iter.hasNext())
		vlist.append(QString::number(iter.next().value()->value()));

	return vlist;
}


// Reset-to-default method.
void qtractorPlugin::reset (void)
{
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator& param_end = m_params.constEnd();
	for ( ; param != param_end; ++param)
		param.value()->reset();
}


// Update editor title.
void qtractorPlugin::updateEditorTitle (void)
{
	QString sEditorTitle = m_pType->name();

	if (m_pList && !m_pList->name().isEmpty())
		sEditorTitle += " - " + m_pList->name();

	setEditorTitle(sEditorTitle);

	if (m_pForm)
		m_pForm->setWindowTitle(sEditorTitle);
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
			| Qt::CustomizeWindowHint
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowMinMaxButtonsHint
			| Qt::WindowCloseButtonHint;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bKeepToolsOnTop) {
		//	pParent = qtractorMainForm::getInstance();
			wflags |= Qt::Tool;
		}
		// Do it...
		m_pForm = new qtractorPluginForm(pParent, wflags);
		m_pForm->setPlugin(this);
	}

	return m_pForm;
}


// Provisional preset accessors.
QStringList qtractorPlugin::presetList (void) const
{
	QStringList list;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->settings().beginGroup(presetGroup());
		list.append(pOptions->settings().childKeys());
		pOptions->settings().endGroup();
	}

	return list;
}


// Plugin default preset name accessor (informational)
void qtractorPlugin::setPreset ( const QString& sPreset )
{
	m_sPreset = sPreset;

	if (m_pForm)
		m_pForm->setPreset(sPreset);
}

const QString& qtractorPlugin::preset (void) const
{
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
bool qtractorPlugin::loadPresetFile ( const QString& sFilename )
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
		if (eChild.tagName() == "configs" || eChild.tagName() == "configure") {
			// Parse for config entries...
			qtractorPlugin::loadConfigs(&eChild, m_configs, m_ctypes);
		}
		else
		if (eChild.tagName() == "params") {
			// Parse for param entries...
			qtractorPlugin::loadValues(&eChild, m_values);
		}
	}

	// Make it real.
	realizeConfigs();
	realizeValues();

	releaseConfigs();
	releaseValues();

	return true;
}


// Save plugin preset to xml file.
bool qtractorPlugin::savePresetFile ( const QString& sFilename )
{
	freezeConfigs();

	QFileInfo fi(sFilename);

	QDomDocument doc("qtractorPlugin");

	QDomElement ePreset = doc.createElement("preset");
	ePreset.setAttribute("type", presetPrefix());
	ePreset.setAttribute("name", fi.baseName());
	ePreset.setAttribute("version", QTRACTOR_VERSION);

	// Save plugin configs...
	QDomElement eConfigs = doc.createElement("configs");
	saveConfigs(&doc, &eConfigs);
	ePreset.appendChild(eConfigs);

	// Save plugin params...
	QDomElement eParams = doc.createElement("params");
	saveValues(&doc, &eParams);
	ePreset.appendChild(eParams);

	doc.appendChild(ePreset);

	// Finally, we're ready to save to external file.
	QFile file(sFilename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	QTextStream ts(&file);
	ts << doc.toString() << endl;
	file.close();

	releaseConfigs();

	return true;
}


// Plugin parameter lookup.
qtractorPluginParam *qtractorPlugin::findParam ( unsigned long iIndex ) const
{
	return m_params.value(iIndex, NULL);
}

qtractorPluginParam *qtractorPlugin::findParamName ( const QString& sName ) const
{
	return m_paramNames.value(sName);
}



// Direct access parameter
qtractorPluginParam *qtractorPlugin::directAccessParam (void) const
{
	if (isDirectAccessParam())
		return findParam(m_iDirectAccessParamIndex);
	else
		return NULL;
}


void qtractorPlugin::setDirectAccessParamIndex ( long iDirectAccessParamIndex )
{
	m_iDirectAccessParamIndex = iDirectAccessParamIndex;

	updateDirectAccessParam();
}


long qtractorPlugin::directAccessParamIndex (void) const
{
	return m_iDirectAccessParamIndex;
}


bool qtractorPlugin::isDirectAccessParam (void) const
{
	return (m_iDirectAccessParamIndex >= 0);
}


// Write the value to the display item.
void qtractorPlugin::updateDirectAccessParam (void)
{
	QListIterator<qtractorPluginListItem *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateActivated();
}


// Plugin parameter/state snapshot.
void qtractorPlugin::freezeValues (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin[%p]::freezeValues()", this);
#endif

	clearValues();

	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator& param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		unsigned long iIndex = pParam->index();
		m_values.names[iIndex] = pParam->name();
		m_values.index[iIndex] = pParam->value();
	}
}


void qtractorPlugin::releaseValues (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin[%p]::releaseValues()", this);
#endif

	clearValues();
}


// Plugin configure realization.
void qtractorPlugin::realizeConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin[%p]::realizeConfigs()", this);
#endif

	// Set configuration (CLOBs)...
	Configs::ConstIterator config = m_configs.constBegin();
	const Configs::ConstIterator& config_end = m_configs.constEnd();
	for ( ; config != config_end; ++config)
		configure(config.key(), config.value());

	// Set proper bank/program selection...
	qtractorMidiManager *pMidiManager = m_pList->midiManager();
	if (pMidiManager)
		selectProgram(pMidiManager->currentBank(), pMidiManager->currentProg());
}


// Plugin parameter realization.
void qtractorPlugin::realizeValues (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin[%p]::realizeValues()", this);
#endif

	// (Re)set parameter values (initial)...
	ValueIndex::ConstIterator param = m_values.index.constBegin();
	const ValueIndex::ConstIterator& param_end = m_values.index.constEnd();
	for ( ; param != param_end; ++param) {
		unsigned long iIndex = param.key();
		qtractorPluginParam *pParam = findParam(iIndex);
		if (pParam) {
			const QString& sName = m_values.names.value(iIndex);
			if (!sName.isEmpty() && sName != pParam->name())
				pParam = findParamName(sName);
			if (pParam)
				pParam->setValue(param.value(), true);
		}
	}
}


// Load plugin configuration stuff (CLOB).
void qtractorPlugin::loadConfigs (
	QDomElement *pElement, Configs& configs, ConfigTypes& ctypes )
{
	for (QDomNode nConfig = pElement->firstChild();
			!nConfig.isNull();
				nConfig = nConfig.nextSibling()) {
		// Convert config node to element...
		QDomElement eConfig = nConfig.toElement();
		if (eConfig.isNull())
			continue;
		if (eConfig.tagName() == "config") {
			const QString& sKey = eConfig.attribute("key");
			if (!sKey.isEmpty()) {
				configs[sKey] = eConfig.text();
				const QString& sType = eConfig.attribute("type");
				if (!sType.isEmpty())
					ctypes[sKey] = sType;
			}
		}
	}
}


// Load plugin parameter values.
void qtractorPlugin::loadValues ( QDomElement *pElement, Values& values )
{
	for (QDomNode nParam = pElement->firstChild();
			!nParam.isNull(); nParam = nParam.nextSibling()) {
		// Convert node to element, if any.
		QDomElement eParam = nParam.toElement();
		if (eParam.isNull())
			continue;
		// Check for config item...
		if (eParam.tagName() == "param") {
			unsigned long iIndex = eParam.attribute("index").toULong();
			const QString& sName = eParam.attribute("name");
			if (!sName.isEmpty())
				values.names.insert(iIndex, sName);
			values.index.insert(iIndex, eParam.text().toFloat());
		}
	}
}


// Save plugin configuration stuff (CLOB)...
void qtractorPlugin::saveConfigs (
	QDomDocument *pDocument, QDomElement *pElement )
{
	// Save plugin configs...
	Configs::ConstIterator iter = m_configs.constBegin();
	const Configs::ConstIterator& iter_end = m_configs.constEnd();
	for ( ; iter != iter_end; ++iter) {
		QDomElement eConfig = pDocument->createElement("config");
		eConfig.setAttribute("key", iter.key());
		ConfigTypes::ConstIterator ctype = m_ctypes.find(iter.key());
		if (ctype != m_ctypes.constEnd())
			eConfig.setAttribute("type", ctype.value());
		eConfig.appendChild(
			pDocument->createTextNode(iter.value()));
		pElement->appendChild(eConfig);
	}
}


// Save plugin parameter values.
void qtractorPlugin::saveValues (
	QDomDocument *pDocument, QDomElement *pElement )
{
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		QDomElement eParam = pDocument->createElement("param");
		eParam.setAttribute("name", pParam->name());
		eParam.setAttribute("index", QString::number(pParam->index()));
		eParam.appendChild(
			pDocument->createTextNode(QString::number(pParam->value())));
		pElement->appendChild(eParam);
	}
}


// Parameter update executive.
void qtractorPlugin::updateParamValue (
	unsigned long iIndex, float fValue, bool bUpdate )
{
	qtractorPluginParam *pParam = findParam(iIndex);
	if (pParam)
		pParam->updateValue(fValue, bUpdate);
}


// Load plugin parameter controllers (MIDI).
void qtractorPlugin::loadControllers (
	QDomElement *pElement, qtractorMidiControl::Controllers& controllers )
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	pMidiControl->loadControllers(pElement, controllers);
}


// Save plugin parameter controllers (MIDI).
void qtractorPlugin::saveControllers (
	qtractorDocument *pDocument, QDomElement *pElement ) const
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	qtractorMidiControl::Controllers controllers;
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		qtractorMidiControlObserver *pObserver = pParam->observer();
		if (pMidiControl->isMidiObserverMapped(pObserver)) {
			qtractorMidiControl::Controller *pController
				= new qtractorMidiControl::Controller;
			pController->name = pParam->name();
			pController->index = pParam->index();
			pController->ctype = pObserver->type();
			pController->channel = pObserver->channel();
			pController->param = pObserver->param();
			pController->logarithmic = pObserver->isLogarithmic();
			pController->feedback = pObserver->isFeedback();
			pController->invert = pObserver->isInvert();
			pController->hook = pObserver->isHook();
			controllers.append(pController);
		}
	}

	pMidiControl->saveControllers(pDocument, pElement, controllers);

	qDeleteAll(controllers);
}


// Map/realize plugin parameter controllers (MIDI).
void qtractorPlugin::mapControllers (
	const qtractorMidiControl::Controllers& controllers )
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl) {
		QListIterator<qtractorMidiControl::Controller *> iter(controllers);
		while (iter.hasNext()) {
			qtractorMidiControl::Controller *pController = iter.next();
			qtractorPluginParam *pParam = findParam(pController->index);
			if (pParam == NULL)
				continue;
			qtractorMidiControlObserver *pObserver = pParam->observer();
			pObserver->setType(pController->ctype);
			pObserver->setChannel(pController->channel);
			pObserver->setParam(pController->param);
			pObserver->setLogarithmic(pController->logarithmic);
			pObserver->setFeedback(pController->feedback);
			pObserver->setInvert(pController->invert);
			pObserver->setHook(pController->hook);
			pMidiControl->mapMidiObserver(pObserver);
		}
	}
}


// Load plugin automation curves (monitor, gain, pan, record, mute, solo).
void qtractorPlugin::loadCurveFile (
	QDomElement *pElement, qtractorCurveFile *pCurveFile )
{
	if (pCurveFile) pCurveFile->load(pElement);
}


// Save plugin automation curves (monitor, gain, pan, record, mute, solo).
void qtractorPlugin::saveCurveFile ( qtractorDocument *pDocument,
	QDomElement *pElement, qtractorCurveFile *pCurveFile ) const
{
	if (pCurveFile == NULL)
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	pCurveFile->clear();
	pCurveFile->setBaseDir(pSession->sessionDir());
	
	unsigned short iParam = 0;
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		qtractorCurve *pCurve = pParam->subject()->curve();
		if (pCurve) {
			qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
			pCurveItem->name = pParam->name();
			pCurveItem->index = pParam->index();
			if (pParam->isToggled()	|| pParam->isInteger()
				|| !pOptions->bSaveCurve14bit) {
				const unsigned short controller = (iParam % 0x7f);
				if (controller == 0x00 || controller == 0x20)
					++iParam; // Avoid bank-select controllers, please.
				pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
				pCurveItem->param = (iParam % 0x7f);
			} else {
				pCurveItem->ctype = qtractorMidiEvent::NONREGPARAM;
				pCurveItem->param = (iParam % 0x3fff);
			}
			pCurveItem->channel = ((iParam / 0x7f) % 16);
			pCurveItem->mode = pCurve->mode();
			pCurveItem->process = pCurve->isProcess();
			pCurveItem->capture = pCurve->isCapture();
			pCurveItem->locked = pCurve->isLocked();
			pCurveItem->logarithmic = pCurve->isLogarithmic();
			pCurveItem->color = pCurve->color();
			pCurveItem->subject = pCurve->subject();
			pCurveFile->addItem(pCurveItem);
			++iParam;
		}
	}

	if (pCurveFile->isEmpty())
		return;

	QString sBaseName(list()->name());
	sBaseName += '_';
	sBaseName += type()->label();
	sBaseName += '_';
	sBaseName += QString::number(uniqueID(), 16);
	sBaseName += "_curve";
//	int iClipNo = (pCurveFile->filename().isEmpty() ? 0 : 1);
	pCurveFile->setFilename(pSession->createFilePath(sBaseName, "mid", 1));

	pCurveFile->save(pDocument, pElement, pSession->timeScale());
}


// Apply plugin automation curves (monitor, gain, pan, record, mute, solo).
void qtractorPlugin::applyCurveFile ( qtractorCurveFile *pCurveFile ) const
{
	if (pCurveFile == NULL)
		return;
	if (pCurveFile->items().isEmpty())
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pCurveFile->setBaseDir(pSession->sessionDir());

	QListIterator<qtractorCurveFile::Item *> iter(pCurveFile->items());
	while (iter.hasNext()) {
		qtractorCurveFile::Item *pCurveItem = iter.next();
		qtractorPluginParam *pParam = NULL;
		if (!pCurveItem->name.isEmpty())
			pParam = findParamName(pCurveItem->name);
		if (pParam == NULL)
			pParam = findParam(pCurveItem->index);
		if (pParam)
			pCurveItem->subject = pParam->subject();
	}

	pCurveFile->apply(pSession->timeScale());
}


//----------------------------------------------------------------------------
// qtractorPluginParam -- Plugin parameter (control input port) instance.
//

// Current port value.
void qtractorPluginParam::setValue ( float fValue, bool bUpdate )
{
	// Decimals caching....
	if (m_iDecimals < 0) {
		m_iDecimals = 0;
		if (!isInteger()) {
			float fDecs = ::log10f(maxValue() - minValue());
			if (fDecs < -3.0f)
				m_iDecimals = 6;
			else if (fDecs < 0.0f)
				m_iDecimals = 3;
			else if (fDecs < 1.0f)
				m_iDecimals = 2;
			else if (fDecs < 6.0f)
				m_iDecimals = 1;
			if (isLogarithmic())
				++m_iDecimals;
		}
	}

	// Sanitize value...
	if (isBoundedAbove() && fValue > maxValue())
		fValue = maxValue();
	else
	if (isBoundedBelow() && fValue < minValue())
		fValue = minValue();

	m_observer.setValue(fValue);

	// Update specifics.
	if (bUpdate) m_pPlugin->updateParam(this, fValue, true);

	if (m_pPlugin->directAccessParamIndex() == long(m_iIndex))
		m_pPlugin->updateDirectAccessParam();
}


void qtractorPluginParam::updateValue ( float fValue, bool bUpdate )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginParam[%p]::updateValue(%g, %d)", this, fValue, int(bUpdate));
#endif

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		pSession->execute(
			new qtractorPluginParamCommand(this, fValue, bUpdate));
	}
}


// Constructor.
qtractorPluginParam::Observer::Observer ( qtractorPluginParam *pParam )
	: qtractorMidiControlObserver(pParam->subject()), m_pParam(pParam)
{
	setCurveList((pParam->plugin())->list()->curveList());
}


// Virtual observer updater.
void qtractorPluginParam::Observer::update ( bool bUpdate )
{
	qtractorMidiControlObserver::update(bUpdate);

	qtractorPlugin *pPlugin = m_pParam->plugin();
	if (bUpdate && pPlugin->directAccessParamIndex() == long(m_pParam->index()))
		pPlugin->updateDirectAccessParam();
	pPlugin->updateParam(m_pParam, qtractorMidiControlObserver::value(), bUpdate);
}


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

// Constructor.
qtractorPluginList::qtractorPluginList ( unsigned short iChannels,
	unsigned int iBufferSize, unsigned int iSampleRate, unsigned int iFlags )
	: m_iChannels(0), m_iBufferSize(0), m_iSampleRate(0),
		m_iFlags(0), m_iActivated(0), m_pMidiManager(NULL),
		m_iMidiBank(-1), m_iMidiProg(-1), m_pMidiProgramSubject(NULL)
{
	setAutoDelete(true);

	m_pppBuffers[0] = NULL;
	m_pppBuffers[1] = NULL;

	m_pCurveList = new qtractorCurveList();

	m_bAudioOutputBus
		= qtractorMidiManager::isDefaultAudioOutputBus();
	m_bAudioOutputAutoConnect
		= qtractorMidiManager::isDefaultAudioOutputAutoConnect();

	setBuffer(iChannels, iBufferSize, iSampleRate, iFlags);
}

// Destructor.
qtractorPluginList::~qtractorPluginList (void)
{
	// Reset allocated channel buffers.
	setBuffer(0, 0, 0, 0);

	// Clear out all dependables...
	m_views.clear();

	delete m_pCurveList;
}


// The title to show up on plugin forms...
void qtractorPluginList::setName ( const QString& sName )
{
	m_sName = sName;

	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		pPlugin->updateEditorTitle();
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
		for (i = 0; i < m_iChannels; ++i)
			delete [] m_pppBuffers[1][i];
		delete [] m_pppBuffers[1];
		m_pppBuffers[1] = NULL;
	}

	// Destroy any MIDI manager still there...
	if (m_pMidiManager) {
		m_bAudioOutputBus = m_pMidiManager->isAudioOutputBus();
		m_bAudioOutputAutoConnect = m_pMidiManager->isAudioOutputAutoConnect();
		m_sAudioOutputBusName = m_pMidiManager->audioOutputBusName();
		qtractorMidiManager::deleteMidiManager(m_pMidiManager);
		m_pMidiManager = NULL;
	}

	if (m_pMidiProgramSubject) {
		delete m_pMidiProgramSubject;
		m_pMidiProgramSubject = NULL;
	}

	// Set proper sample-rate and flags at once.
	m_iSampleRate = iSampleRate;
	m_iFlags = iFlags;

	// Some sanity is in order, at least for now...
	if (iChannels == 0 || iBufferSize == 0)
		return;

	// Go, go, go...
	m_iChannels   = iChannels;
	m_iBufferSize = iBufferSize;

	// Allocate new interim buffer...
	if (m_iChannels > 0 && m_iBufferSize > 0) {
		m_pppBuffers[1] = new float * [m_iChannels];
		for (i = 0; i < m_iChannels; ++i) {
			m_pppBuffers[1][i] = new float [m_iBufferSize];
			::memset(m_pppBuffers[1][i], 0, m_iBufferSize * sizeof(float));
		}
	}

	// Allocate new MIDI manager, if applicable...
	if (m_iFlags & Midi) {
		m_pMidiProgramSubject = new MidiProgramSubject(m_iMidiBank, m_iMidiProg);
		m_pMidiManager = qtractorMidiManager::createMidiManager(this);
		// Set loaded/cached properties properly...
		m_pMidiManager->setCurrentBank(m_iMidiBank);
		m_pMidiManager->setCurrentProg(m_iMidiProg);
		m_pMidiManager->setAudioOutputBusName(m_sAudioOutputBusName);
		m_pMidiManager->setAudioOutputAutoConnect(m_bAudioOutputAutoConnect);
		m_pMidiManager->setAudioOutputBus(m_bAudioOutputBus);
		if (m_pMidiManager->isAudioOutputBus()) {
			qtractorAudioBus *pAudioOutputBus
				= m_pMidiManager->audioOutputBus();
			if (pAudioOutputBus)
				pAudioOutputBus->outputs().copy(m_audioOutputs);
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
#if 0
	// Save and reset activation count...
	int iActivated = m_iActivated;
	m_iActivated = 0;

	// Temporarily deactivate all activated plugins...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated())
			pPlugin->deactivate();
	}
#endif
	// Reset interim buffer, if any...
	if (m_pppBuffers[1]) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			::memset(m_pppBuffers[1][i], 0, m_iBufferSize * sizeof(float));
	}
#if 0
	// Restore activation of all previously deactivated plugins...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated())
			pPlugin->activate();
	}

	// Restore activation count.
	m_iActivated = iActivated;
#endif
}


// Special guard activation methods.
bool qtractorPluginList::isActivatedAll (void) const
{
	return (m_iActivated > 0 && m_iActivated >= (unsigned int) count());
}


void qtractorPluginList::updateActivated ( bool bActivated )
{
	if (bActivated) {
		++m_iActivated;
	} else  {
		--m_iActivated;
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

	if (pNextPlugin)
		insertBefore(pPlugin, pNextPlugin);
	else
		append(pPlugin);

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
	pPlugin->freezeValues();
	pPlugin->freezeConfigs();

#if 0
	// MIDI bank program whether necessary...
	int iBank = 0;
	int iProg = 0;
	if (m_pMidiManager && m_pMidiManager->currentBank() >= 0)
		iBank = m_pMidiManager->currentBank();
	if (m_pMidiManager && m_pMidiManager->currentProg() >= 0)
		iProg = m_pMidiManager->currentProg();
#endif

	// Filename is empty for insert pseudo-plugins.
	QString sFilename = pType->filename();
	qtractorPlugin *pNewPlugin = qtractorPluginFile::createPlugin(this,
		sFilename, pType->index(), pType->typeHint());
	if (pNewPlugin) {
		pNewPlugin->setPreset(pPlugin->preset());
		pNewPlugin->setConfigs(pPlugin->configs());
		pNewPlugin->setConfigTypes(pPlugin->configTypes());
		pNewPlugin->setValues(pPlugin->values());
		pNewPlugin->realizeConfigs();
		pNewPlugin->realizeValues();
		pNewPlugin->releaseConfigs();
		pNewPlugin->releaseValues();
		pNewPlugin->setActivated(pPlugin->isActivated());
	}

	pPlugin->releaseConfigs();
	pPlugin->releaseValues();

	return pNewPlugin;
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
	if (ppBuffer == NULL || *ppBuffer == NULL)
		return;
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
bool qtractorPluginList::loadElement (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	// Reset some MIDI manager elements...
	m_iMidiBank = -1;
	m_iMidiProg = -1;
	m_bAudioOutputBus = false;
	m_bAudioOutputAutoConnect = false;
	m_sAudioOutputBusName.clear();
	m_audioOutputs.clear();

	// Load plugin-list children...
	for (QDomNode nPlugin = pElement->firstChild();
			!nPlugin.isNull();
				nPlugin = nPlugin.nextSibling()) {

		// Convert clip node to element...
		QDomElement ePlugin = nPlugin.toElement();
		if (ePlugin.isNull())
			continue;
		if (ePlugin.tagName() == "bank")
			setMidiBank(ePlugin.text().toInt());
		else
		if (ePlugin.tagName() == "program")
			setMidiProg(ePlugin.text().toInt());
		else
		if (ePlugin.tagName() == "plugin") {
			QString sFilename;
			unsigned long iUniqueID = 0;
			unsigned long iIndex = 0;
			QString sLabel;
			QString sPreset;
			QStringList vlist;
			bool bActivated = false;
			long iDirectAccessParamIndex = -1;
			qtractorPlugin::Configs configs;
			qtractorPlugin::ConfigTypes ctypes;
			qtractorPlugin::Values values;
			qtractorMidiControl::Controllers controllers;
			qtractorCurveFile cfile(qtractorPluginList::curveList());
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
				if (eParam.tagName() == "unique-id")
					iUniqueID = eParam.text().toULong();
				else
				if (eParam.tagName() == "index")
					iIndex = eParam.text().toULong();
				else
				if (eParam.tagName() == "label")
					sLabel = eParam.text();
				else
				if (eParam.tagName() == "preset")
					sPreset = eParam.text();
				else
				if (eParam.tagName() == "values")
					vlist = eParam.text().split(',');
				else
				if (eParam.tagName() == "activated")
					bActivated = qtractorDocument::boolFromText(eParam.text());
				else
				if (eParam.tagName() == "configs") {
					// Load plugin configuration stuff (CLOB)...
					qtractorPlugin::loadConfigs(&eParam, configs, ctypes);
				}
				else
				if (eParam.tagName() == "params") {
					// Load plugin parameter values...
					qtractorPlugin::loadValues(&eParam, values);
				}
				else
				if (eParam.tagName() == "controllers") {
					// Load plugin parameter controllers...
					qtractorPlugin::loadControllers(&eParam, controllers);
				}
				else
				if (eParam.tagName() == "direct-access-param")
					iDirectAccessParamIndex = eParam.text().toLong();
				else
				if (eParam.tagName() == "curve-file") {
					// Load plugin automation curves...
					qtractorPlugin::loadCurveFile(&eParam, &cfile);
				}
			}
			qtractorPlugin *pPlugin
				= qtractorPluginFile::createPlugin(this,
					sFilename, iIndex, typeHint);
			if (!sFilename.isEmpty() && !sLabel.isEmpty() &&
				((pPlugin == NULL) || ((pPlugin->type())->label() != sLabel))) {
				iIndex = 0;
				do {
					pPlugin = qtractorPluginFile::createPlugin(this,
						sFilename, iIndex++, typeHint);
				} while (pPlugin && (pPlugin->type())->label() != sLabel);
			}
			if (pPlugin) {
				if (iUniqueID > 0)
					pPlugin->setUniqueID(iUniqueID);
				pPlugin->setPreset(sPreset);
				pPlugin->setConfigs(configs);
				pPlugin->setConfigTypes(ctypes);
				if (!vlist.isEmpty())
					pPlugin->setValueList(vlist);
				if (!values.index.isEmpty())
					pPlugin->setValues(values);
				append(pPlugin);
				pPlugin->mapControllers(controllers);
				pPlugin->applyCurveFile(&cfile);
				pPlugin->setDirectAccessParamIndex(iDirectAccessParamIndex);
				pPlugin->setActivated(bActivated); // Later's better!
			}
			// Cleanup.
			qDeleteAll(controllers);
			controllers.clear();
		}
		else
		// Load audio output bus name...
		if (ePlugin.tagName() == "audio-output-bus-name") {
			m_sAudioOutputBusName = ePlugin.text();
		}
		// Load audio output bus flag...
		if (ePlugin.tagName() == "audio-output-bus") {
			m_bAudioOutputBus = qtractorDocument::boolFromText(ePlugin.text());
		}
		else
		// Load audio output auto-connect flag...
		if (ePlugin.tagName() == "audio-output-auto-connect") {
			m_bAudioOutputAutoConnect = qtractorDocument::boolFromText(ePlugin.text());
		}
		else
		// Load audio output connections...
		if (ePlugin.tagName() == "audio-outputs") {
			qtractorBus::loadConnects(m_audioOutputs, pDocument, &ePlugin);
		}
		// Make up audio output bus ...
		setAudioOutputBusName(m_sAudioOutputBusName);
		setAudioOutputAutoConnect(m_bAudioOutputAutoConnect);
		setAudioOutputBus(m_bAudioOutputBus);
	}

	return true;
}


bool qtractorPluginList::saveElement ( qtractorDocument *pDocument,
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
	for (qtractorPlugin *pPlugin = qtractorPluginList::first();
			pPlugin; pPlugin = pPlugin->next()) {

		// Do freeze plugin state...
		pPlugin->freezeConfigs();

		// Create the new plugin element...
		QDomElement ePlugin = pDocument->document()->createElement("plugin");
		qtractorPluginType *pType = pPlugin->type();
		ePlugin.setAttribute("type",
			qtractorPluginType::textFromHint(pType->typeHint()));
		// Pseudo-plugins don't have a file...
		const QString& sFilename = pType->filename();
		if (!sFilename.isEmpty()) {
			pDocument->saveTextElement("filename",
				sFilename, &ePlugin);
		}
		if (isUniqueID(pType)) {
			pDocument->saveTextElement("unique-id",
				QString::number(pPlugin->uniqueID()), &ePlugin);
		}
		pDocument->saveTextElement("index",
			QString::number(pType->index()), &ePlugin);
		pDocument->saveTextElement("label",
			pType->label(), &ePlugin);
		pDocument->saveTextElement("preset",
			pPlugin->preset(), &ePlugin);
		pDocument->saveTextElement("direct-access-param",
			QString::number(pPlugin->directAccessParamIndex()), &ePlugin);
	//	pDocument->saveTextElement("values",
	//		pPlugin->valueList().join(","), &ePlugin);
		pDocument->saveTextElement("activated",
			qtractorDocument::textFromBool(pPlugin->isActivated()), &ePlugin);
		// Plugin configuration stuff (CLOB)...
		QDomElement eConfigs = pDocument->document()->createElement("configs");
		pPlugin->saveConfigs(pDocument->document(), &eConfigs);
		ePlugin.appendChild(eConfigs);
		// Plugin parameter values...
		QDomElement eParams = pDocument->document()->createElement("params");
		pPlugin->saveValues(pDocument->document(), &eParams);
		ePlugin.appendChild(eParams);
		// Plugin paramneter controllers...
		QDomElement eControllers
			= pDocument->document()->createElement("controllers");
		pPlugin->saveControllers(pDocument, &eControllers);
		ePlugin.appendChild(eControllers);
		// Save plugin automation...
		qtractorCurveList *pCurveList = qtractorPluginList::curveList();
		if (pCurveList && !pCurveList->isEmpty()) {
			qtractorCurveFile cfile(pCurveList);
			QDomElement eCurveFile
				= pDocument->document()->createElement("curve-file");
			pPlugin->saveCurveFile(pDocument, &eCurveFile, &cfile);
			ePlugin.appendChild(eCurveFile);
		}

		// Add this plugin...
		pElement->appendChild(ePlugin);

		// May release plugin state...
		pPlugin->releaseConfigs();
	}

	// Save audio output-bus connects...
	if (m_pMidiManager) {
		pDocument->saveTextElement("audio-output-bus-name",
			m_pMidiManager->audioOutputBusName(), pElement);
		pDocument->saveTextElement("audio-output-bus",
			qtractorDocument::textFromBool(
				m_pMidiManager->isAudioOutputBus()), pElement);
		pDocument->saveTextElement("audio-output-auto-connect",
			qtractorDocument::textFromBool(
				m_pMidiManager->isAudioOutputAutoConnect()), pElement);
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


// Acquire a unique plugin identifier in chain.
unsigned long qtractorPluginList::createUniqueID ( qtractorPluginType *pType )
{
	const unsigned long k = pType->uniqueID();
	const unsigned int i = m_uniqueIDs.value(k, 0);
	m_uniqueIDs.insert(k, i + 1);
	return k + i;
}


// Whether unique plugin identifiers are in chain.
bool qtractorPluginList::isUniqueID ( qtractorPluginType *pType ) const
{
	return (m_uniqueIDs.value(pType->uniqueID(), 0) > 1);
}


// end of qtractorPlugin.cpp
