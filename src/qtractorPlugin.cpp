// qtractorPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorPluginFactory.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"
#include "qtractorPluginForm.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"

#include "qtractorMainForm.h"

#include "qtractorOptions.h"

#include "qtractorSession.h"
#include "qtractorCurveFile.h"

#include "qtractorMessageList.h"

#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>

#include <QFileInfo>
#include <QFile>
#include <QDir>

#include <math.h>


#if QT_VERSION < 0x040500
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif


// A common function for special platforms...
//
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
typedef void (*qtractorPluginFile_Function)(void);
#endif


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

// Executive methods.
bool qtractorPluginFile::open (void)
{
	// Check whether already open...
	if (++m_iOpenCount > 1)
		return true;

	// ATTN: Not really needed, as it would be
	// loaded automagically on resolve(), but ntl...
	if (!QLibrary::load()) {
		m_iOpenCount = 0;
		return false;
	}

	// Do the openning dance...
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
	qtractorPluginFile_Function pfnInit
		= (qtractorPluginFile_Function) QLibrary::resolve("_init");
	if (pfnInit)
		(*pfnInit)();
#endif

	// Done alright.
	return true;
}


void qtractorPluginFile::close (void)
{
	if (!QLibrary::isLoaded())
		return;

	if (--m_iOpenCount > 0)
		return;

	// Do the closing dance...
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
	qtractorPluginFile_Function pfnFini
		= (qtractorPluginFile_Function) QLibrary::resolve("_fini");
	if (pfnFini)
		(*pfnFini)();
#endif

	// ATTN: Might be really needed, as it would
	// otherwise pile up hosing all available RAM
	// until freed and unloaded on exit();
	// nb. some VST might choke on auto-unload.
	if (m_bAutoUnload) QLibrary::unload();
}


// Plugin file resgistry methods.
qtractorPluginFile::Files qtractorPluginFile::g_files;

qtractorPluginFile *qtractorPluginFile::addFile ( const QString& sFilename )
{
	qtractorPluginFile *pFile = g_files.value(sFilename, NULL);

	if (pFile == NULL && QLibrary::isLibrary(sFilename)) {
		pFile = new qtractorPluginFile(sFilename);
		g_files.insert(pFile->filename(), pFile);
	}

	if (pFile && !pFile->open())
		pFile = NULL;

	if (pFile)
		pFile->addRef();

	return pFile;
}


void qtractorPluginFile::removeFile ( qtractorPluginFile *pFile )
{
	if (pFile && pFile->removeRef()) {
		g_files.remove(pFile->filename());
		delete pFile;
	}
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
		if (bMidi && (m_iMidiIns > 0 || m_iMidiOuts > 0))
			iInstances = 1;
		else
		if (m_iAudioOuts >= iChannels || m_iAudioIns >= iChannels)
			iInstances = 1;
		else
		if (m_iAudioOuts > 0)
			iInstances = (iChannels / m_iAudioOuts);
	}
	return iInstances;
}


// Plugin type(hint) textual helpers (static).
qtractorPluginType::Hint qtractorPluginType::hintFromText (
	const QString& sText )
{
	if (sText == "LADSPA")
		return Ladspa;
	else
	if (sText == "DSSI")
		return Dssi;
	else
	if (sText == "VST")
		return Vst;
	else
	if (sText == "LV2")
		return Lv2;
	else
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
	if (typeHint == Ladspa)
		return "LADSPA";
	else
	if (typeHint == Dssi)
		return "DSSI";
	else
	if (typeHint == Vst)
		return "VST";
	else
	if (typeHint == Lv2)
		return "LV2";
	else
	if (typeHint == Insert)
		return "Insert";
	else
	if (typeHint == AuxSend)
		return "AuxSend";
	else
		return QObject::tr("(Any)");
}


//----------------------------------------------------------------------------
// qtractorPlugin -- Plugin instance.
//

// Default preset name (global).
QString qtractorPlugin::g_sDefPreset = QObject::tr("(default)");


// Constructors.
qtractorPlugin::qtractorPlugin (
	qtractorPluginList *pList, qtractorPluginType *pType )
	: m_pList(pList), m_pType(pType), m_iUniqueID(0), m_iInstances(0),
		m_bActivated(false), m_bAutoDeactivated(false),
		m_activateObserver(this),
		m_iActivateSubjectIndex(0), m_pForm(NULL), m_iEditorType(-1),
		m_iDirectAccessParamIndex(-1)
{
	// Acquire a local unique id in chain...
	if (m_pList && m_pType)
		m_iUniqueID = m_pList->createUniqueID(m_pType);

	// Activate subject properties.
	m_activateSubject.setName(QObject::tr("Activate"));
	m_activateSubject.setToggled(true);
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
	if (m_pType) delete m_pType;
}


// Chain helper ones.
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

// immediate
void qtractorPlugin::setActivated (bool bActivated)
{
	updateActivated(bActivated);

	m_activateObserver.setValue(bActivated ? 1.0f : 0.0f);
}

// queued (GUI invocation)
void qtractorPlugin::setActivatedEx ( bool bActivated )
{
	m_activateSubject.setValue(bActivated ? 1.0f : 0.0f);
}

bool qtractorPlugin::isActivated (void) const
{
	return m_bActivated && !m_bAutoDeactivated;
}


// Avoid save/copy auto-deactivated as deacitvated...
bool qtractorPlugin::isActivatedEx (void) const
{
	return m_bActivated;
}


void qtractorPlugin::autoDeactivatePlugin ( bool bDeactivated )
{
	if (bDeactivated != m_bAutoDeactivated) {
		// deactivate
		if (bDeactivated) {
			// was activated?
			if (m_bActivated) {
				deactivate();
				m_pList->updateActivated(false);
			}
		}
		// reactivate?
		else if (m_bActivated) {
			activate();
			m_pList->updateActivated(true);
		}
		m_bAutoDeactivated = bDeactivated;
	}
}

bool qtractorPlugin::canBeConnectedToOtherTracks (void) const
{
	const qtractorPluginType::Hint hint = m_pType->typeHint();
	return hint == qtractorPluginType::Insert
		|| hint == qtractorPluginType::AuxSend;
}


// Activation stabilizers.
void qtractorPlugin::updateActivated ( bool bActivated )
{
	if (bActivated != m_bActivated) {
		m_bActivated = bActivated;
		const bool bIsConnectedToOtherTracks = canBeConnectedToOtherTracks();
		// Auto-plugin-deactivation overrides standard-activation for plugins
		// without connections to other tracks (Inserts/AuxSends)
		// otherwise user could (de)activate plugin without getting feedback
		if (!m_bAutoDeactivated || bIsConnectedToOtherTracks) {
			if (bActivated)
				activate();
			else
				deactivate();
			m_pList->updateActivated(bActivated);
		}
		// Plugins connected to other tracks activation change
		// auto-plugin-deactivate for all tracks
		if (bIsConnectedToOtherTracks) {
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession)
				pSession->autoDeactivatePlugins(true);
		}
	}
}


void qtractorPlugin::updateActivatedEx ( bool bActivated )
{
	updateActivated(bActivated);

	// Get extra visual feedback as well,
	// iif. we're not exporting/freewheeling...
	//
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	if (pAudioEngine->isFreewheel())
		return;

	QListIterator<qtractorPluginListItem *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateActivated();

	if (m_pForm)
		m_pForm->updateActivated();
}


// Activate observer ctor.
qtractorPlugin::ActivateObserver::ActivateObserver ( qtractorPlugin *pPlugin )
	: qtractorMidiControlObserver(pPlugin->activateSubject()), m_pPlugin(pPlugin)
{
	setCurveList(pPlugin->list()->curveList());
}

// Activate observer updater.
void qtractorPlugin::ActivateObserver::update ( bool bUpdate )
{
	qtractorMidiControlObserver::update(bUpdate);

	m_pPlugin->updateActivatedEx(qtractorMidiControlObserver::value() > 0.5f);
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


// Special plugin form methods.
void qtractorPlugin::openForm ( QWidget *pParent )
{
	// Take the change and create the form if it doesn't current exist.
	const bool bCreate = (m_pForm == NULL);

	if (bCreate) {
		// Build up the plugin form...
		// What style do we create tool childs?
		Qt::WindowFlags wflags = Qt::Window;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bKeepToolsOnTop) {
			wflags |= Qt::Tool;
		#if 0//QTRACTOR_PLUGIN_FORM_TOOL_PARENT
			// Make sure it has a parent...
			if (pParent == NULL)
				pParent = qtractorMainForm::getInstance();
		#endif
		}
		// Do it...
		m_pForm = new qtractorPluginForm(pParent, wflags);
		m_pForm->setPlugin(this);
	}

	// Activate form...
	if (!m_pForm->isVisible()) {
		m_pForm->toggleEditor(isEditorVisible());
		m_pForm->show();
	}

	m_pForm->raise();
	m_pForm->activateWindow();

	if (bCreate)
		moveWidgetPos(m_pForm, m_posForm);
}


void qtractorPlugin::closeForm (void)
{
	if (m_pForm && m_pForm->isVisible())
		m_pForm->hide();
}


bool qtractorPlugin::isFormVisible (void) const
{
	return (m_pForm && m_pForm->isVisible());
}


void qtractorPlugin::toggleFormEditor ( bool bOn )
{
	if (m_pForm && m_pForm->isVisible())
		m_pForm->toggleEditor(bOn);
}


void qtractorPlugin::updateFormDirtyCount (void)
{
	if (m_pForm && m_pForm->isVisible())
		m_pForm->updateDirtyCount();
}


void qtractorPlugin::updateFormAuxSendBusName (void)
{
	if (m_pForm && m_pForm->isVisible())
		m_pForm->updateAuxSendBusName();
}


void qtractorPlugin::refreshForm (void)
{
	if (m_pForm && m_pForm->isVisible())
		m_pForm->refresh();
}


void qtractorPlugin::freezeFormPos (void)
{
	if (m_pForm && m_pForm->isVisible())
		m_posForm = m_pForm->pos();
}


// Move widget to alleged parent center or else...
void qtractorPlugin::moveWidgetPos (
	QWidget *pWidget, const QPoint& wpos ) const
{
	QPoint pos(wpos);

	if (pos.isNull() || pos.x() < 0 || pos.y() < 0) {
		QWidget *pParent = pWidget->parentWidget();
		if (pParent == NULL)
			pParent = qtractorMainForm::getInstance();
		if (pParent && pParent->isVisible()) {
			const QSize& delta = pParent->size() - pWidget->size();
			if (delta.isValid()) {
				pos = pParent->pos() + QPoint(
					delta.width()  >> 1,
					delta.height() >> 1);
			}
		}
	}

	if (!pos.isNull() && pos.x() >= 0 && pos.y() >= 0)
		pWidget->move(pos);
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
	ePreset.setAttribute("version", CONFIG_BUILD_VERSION);

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


// Load an existing preset by name.
bool qtractorPlugin::loadPresetEx ( const QString& sPreset )
{
	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return false;

	if (sPreset.isEmpty() || sPreset == g_sDefPreset) {
		// Reset to default...
		return pSession->execute(new qtractorResetPluginCommand(this));
	}

	bool bResult = loadPreset(sPreset);
	if (bResult) {
		setPreset(sPreset);
		pMainForm->dirtyNotifySlot();
		refreshForm();
	} else {
		// An existing preset is about to be loaded...
		QSettings& settings = pOptions->settings();
		// Should it be load from known file?...
		if (type()->isConfigure()) {
			settings.beginGroup(presetGroup());
			bResult = loadPresetFileEx(settings.value(sPreset).toString());
			settings.endGroup();
			if (bResult) {
				setPreset(sPreset);
				pMainForm->dirtyNotifySlot();
				refreshForm();
			}
		} else {
			//...or make it as usual (parameter list only)...
			settings.beginGroup(presetGroup());
			const QStringList& vlist
				= settings.value(sPreset).toStringList();
			settings.endGroup();
			if (!vlist.isEmpty()) {
				bResult = pSession->execute(
					new qtractorPresetPluginCommand(this, sPreset, vlist));
			}
		}
	}

	return bResult;
}


// Load an existing preset from file.
bool qtractorPlugin::loadPresetFileEx ( const QString& sFilename )
{
	const bool bActivated = isActivated();
	setActivated(false);

	const bool bResult = loadPresetFile(sFilename);

	setActivated(bActivated);
	return bResult;
}


// Plugin parameter lookup.
qtractorPluginParam *qtractorPlugin::findParam ( unsigned long iIndex ) const
{
	return m_params.value(iIndex, NULL);
}

qtractorPluginParam *qtractorPlugin::findParamName ( const QString& sName ) const
{
	return m_paramNames.value(sName, NULL);
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
#if QT_VERSION >= 0x050000
	QListIterator<qtractorPluginListView *> iter(m_pList->views());
	while (iter.hasNext())
		iter.next()->viewport()->update();
#else
	QListIterator<qtractorPluginListItem *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateActivated();
#endif
}


// Plugin configuration/state snapshot.
void qtractorPlugin::freezeConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin[%p]::freezeConfigs()", this);
#endif

	// Do nothing...
}


void qtractorPlugin::releaseConfigs (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin[%p]::releaseConfigs()", this);
#endif

	// Do nothing...
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
		const QString& sName = m_values.names.value(iIndex);
		if (!sName.isEmpty() && !(pParam && sName == pParam->name())) {
			qtractorPluginParam *pParamEx = findParamName(sName);
			if (pParamEx)
				pParam = pParamEx;
		}
		if (pParam)
			pParam->setValue(param.value(), true);
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
	qtractorDocument *pDocument, QDomElement *pElement )
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	unsigned long iActivateSubjectIndex = activateSubjectIndex();
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
			pController->latch = pObserver->isLatch();
			controllers.append(pController);
		}
		if (iActivateSubjectIndex < pParam->index())
			iActivateSubjectIndex = pParam->index();
	}

	qtractorMidiControlObserver *pActivateObserver = activateObserver();
	if (pMidiControl->isMidiObserverMapped(pActivateObserver)) {
		setActivateSubjectIndex(iActivateSubjectIndex + 1); // hack up!
		qtractorMidiControl::Controller *pController
			= new qtractorMidiControl::Controller;
		pController->name = pActivateObserver->subject()->name();
		pController->index = activateSubjectIndex();
		pController->ctype = pActivateObserver->type();
		pController->channel = pActivateObserver->channel();
		pController->param = pActivateObserver->param();
		pController->logarithmic = pActivateObserver->isLogarithmic();
		pController->feedback = pActivateObserver->isFeedback();
		pController->invert = pActivateObserver->isInvert();
		pController->hook = pActivateObserver->isHook();
		pController->latch = pActivateObserver->isLatch();
		controllers.append(pController);
	}

	pMidiControl->saveControllers(pDocument, pElement, controllers);

	qDeleteAll(controllers);
}


// Map/realize plugin parameter controllers (MIDI).
void qtractorPlugin::mapControllers (
	const qtractorMidiControl::Controllers& controllers )
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == NULL)
		return;

	const unsigned long iActivateSubjectIndex = activateSubjectIndex();
	QListIterator<qtractorMidiControl::Controller *> iter(controllers);
	while (iter.hasNext()) {
		qtractorMidiControl::Controller *pController = iter.next();
		qtractorMidiControlObserver *pObserver = NULL;
		if ((iActivateSubjectIndex > 0 &&
			 iActivateSubjectIndex == pController->index) ||
			(activateSubject()->name() == pController->name)) {
			pObserver = activateObserver();
		//	setActivateSubjectIndex(0); // hack down!
		//	iActivateSubjectIndex = 0;
		} else {
			qtractorPluginParam *pParam = NULL;
			if (!pController->name.isEmpty())
				pParam = findParamName(pController->name);
			if (pParam == NULL)
				pParam = findParam(pController->index);
			if (pParam)
				pObserver = pParam->observer();
		}
		if (pObserver) {
			pObserver->setType(pController->ctype);
			pObserver->setChannel(pController->channel);
			pObserver->setParam(pController->param);
			pObserver->setLogarithmic(pController->logarithmic);
			pObserver->setFeedback(pController->feedback);
			pObserver->setInvert(pController->invert);
			pObserver->setHook(pController->hook);
			pObserver->setLatch(pController->latch);
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
	QDomElement *pElement, qtractorCurveFile *pCurveFile )
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
	unsigned long iActivateSubjectIndex = activateSubjectIndex();
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
		if (iActivateSubjectIndex < pParam->index())
			iActivateSubjectIndex = pParam->index();
	}

	// Activate subject curve...
	qtractorCurve *pCurve = activateSubject()->curve();
	if (pCurve) {
		setActivateSubjectIndex(iActivateSubjectIndex + 1); // hack up!
		qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
		pCurveItem->name = pCurve->subject()->name();
		pCurveItem->index = activateSubjectIndex();
		pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
		pCurveItem->channel = 0;
		const unsigned short controller = (iParam % 0x7f);
		if (controller == 0x00 || controller == 0x20)
			++iParam; // Avoid bank-select controllers, please.
		pCurveItem->param = (iParam % 0x7f);
		pCurveItem->mode = pCurve->mode();
		pCurveItem->process = pCurve->isProcess();
		pCurveItem->capture = pCurve->isCapture();
		pCurveItem->locked = pCurve->isLocked();
		pCurveItem->logarithmic = pCurve->isLogarithmic();
		pCurveItem->color = pCurve->color();
		pCurveItem->subject = pCurve->subject();
		pCurveFile->addItem(pCurveItem);
	}

	if (pCurveFile->isEmpty())
		return;

	QString sBaseName(list()->name());
	sBaseName += '_';
	sBaseName += type()->label();
	sBaseName += '_';
	sBaseName += QString::number(uniqueID(), 16);
	sBaseName += "_curve";
	pCurveFile->setFilename(pSession->createFilePath(sBaseName, "mid", true));

	pCurveFile->save(pDocument, pElement, pSession->timeScale());
}


// Apply plugin automation curves (monitor, gain, pan, record, mute, solo).
void qtractorPlugin::applyCurveFile ( qtractorCurveFile *pCurveFile )
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

	qtractorSubject *pActivateSubject = activateSubject();
	unsigned long iActivateSubjectIndex = activateSubjectIndex();

	QListIterator<qtractorCurveFile::Item *> iter(pCurveFile->items());
	while (iter.hasNext()) {
		qtractorCurveFile::Item *pCurveItem = iter.next();
		if ((iActivateSubjectIndex > 0 &&
			 iActivateSubjectIndex == pCurveItem->index) ||
			(pActivateSubject->name() == pCurveItem->name)) {
			pCurveItem->subject = pActivateSubject;
			setActivateSubjectIndex(0); // hack down!
			iActivateSubjectIndex = 0;
		} else {
			qtractorPluginParam *pParam = NULL;
			if (!pCurveItem->name.isEmpty())
				pParam = findParamName(pCurveItem->name);
			if (pParam == NULL)
				pParam = findParam(pCurveItem->index);
			if (pParam)
				pCurveItem->subject = pParam->subject();
		}
	}

	pCurveFile->apply(pSession->timeScale());
}


// Save partial plugin state...
bool qtractorPlugin::savePlugin (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	freezeConfigs();

	qtractorPluginType *pType = type();
	pElement->setAttribute("type",
		qtractorPluginType::textFromHint(pType->typeHint()));

	// Pseudo-plugins don't have a file...
	const QString& sFilename = pType->filename();
	if (!sFilename.isEmpty()) {
		pDocument->saveTextElement("filename",
			sFilename, pElement);
	}

	if (list()->isUniqueID(pType)) {
		pDocument->saveTextElement("unique-id",
			QString::number(uniqueID()), pElement);
	}

	pDocument->saveTextElement("index",
		QString::number(pType->index()), pElement);
	pDocument->saveTextElement("label",
		pType->label(), pElement);
	pDocument->saveTextElement("preset",
		preset(), pElement);
	pDocument->saveTextElement("direct-access-param",
		QString::number(directAccessParamIndex()), pElement);
//	pDocument->saveTextElement("values",
//		valueList().join(","), pElement);
	pDocument->saveTextElement("activated",
		qtractorDocument::textFromBool(isActivatedEx()), pElement);

	// Plugin configuration stuff (CLOB)...
	QDomElement eConfigs = pDocument->document()->createElement("configs");
	saveConfigs(pDocument->document(), &eConfigs);
	pElement->appendChild(eConfigs);

	// Plugin parameter values...
	QDomElement eParams = pDocument->document()->createElement("params");
	saveValues(pDocument->document(), &eParams);
	pElement->appendChild(eParams);

	// May release plugin state...
	releaseConfigs();

	return true;
}


// Save complete plugin state...
bool qtractorPlugin::savePluginEx (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	// Freeze form position, if currently visible...
	freezeFormPos();

	// Save partial plugin state first...
	const bool bResult = savePlugin(pDocument, pElement);

	if (bResult) {
		// Plugin paramneter controllers...
		QDomElement eControllers
			= pDocument->document()->createElement("controllers");
		saveControllers(pDocument, &eControllers);
		pElement->appendChild(eControllers);
		// Save plugin automation...
		qtractorCurveList *pCurveList = list()->curveList();
		if (pCurveList && !pCurveList->isEmpty()) {
			qtractorCurveFile cfile(pCurveList);
			QDomElement eCurveFile
				= pDocument->document()->createElement("curve-file");
			saveCurveFile(pDocument, &eCurveFile, &cfile);
			pElement->appendChild(eCurveFile);
			const unsigned long iActivateSubjectIndex
				= activateSubjectIndex();
			if (iActivateSubjectIndex > 0) {
				pDocument->saveTextElement("activate-subject-index",
					QString::number(iActivateSubjectIndex), pElement);
			}
		}
		// Save editor position...
		const QPoint& posEditor = editorPos();
		if (posEditor.x() >= 0 && posEditor.y() >= 0) {
			pDocument->saveTextElement("editor-pos",
				QString::number(posEditor.x()) + ',' +
				QString::number(posEditor.y()), pElement);
		}
		const QPoint& posForm = formPos();
		if (posForm.x() >= 0 && posForm.y() >= 0) {
			pDocument->saveTextElement("form-pos",
				QString::number(posForm.x()) + ',' +
				QString::number(posForm.y()), pElement);
		}
		const int iEditorType = editorType();
		if (iEditorType >= 0) {
			pDocument->saveTextElement("editor-type",
				QString::number(iEditorType), pElement);
		}
	}

	return bResult;
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
			const float fDecs
				= ::log10f(maxValue() - minValue());
			if (fDecs < 0.0f)
				m_iDecimals = 6;
			else if (fDecs < 3.0f)
				m_iDecimals = 3;
			else if (fDecs < 6.0f)
				m_iDecimals = 1;
		#if 0
			if (isLogarithmic())
				++m_iDecimals;
		#endif
		}
		// Make this permanent...
		m_subject.setToggled(isToggled());
		m_subject.setInteger(isInteger());
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
qtractorPluginList::qtractorPluginList (
	unsigned short iChannels, unsigned int iFlags )
	: m_iChannels(iChannels), m_iFlags(iFlags),
		m_iActivated(0), m_pMidiManager(NULL),
		m_iMidiBank(-1), m_iMidiProg(-1),
		m_pMidiProgramSubject(NULL),
		m_bAutoDeactivated(false),
		m_bLatency(false), m_iLatency(0)
{
	setAutoDelete(true);

	m_pppBuffers[0] = NULL;
	m_pppBuffers[1] = NULL;

	m_pCurveList = new qtractorCurveList();

	m_bAudioOutputBus
		= qtractorMidiManager::isDefaultAudioOutputBus();
	m_bAudioOutputAutoConnect
		= qtractorMidiManager::isDefaultAudioOutputAutoConnect();
	m_bAudioOutputMonitor
		= qtractorMidiManager::isDefaultAudioOutputMonitor();

	m_iAudioInsertActivated = 0;

	setChannels(iChannels, iFlags);
}

// Destructor.
qtractorPluginList::~qtractorPluginList (void)
{
	// Reset allocated channel buffers.
	setChannels(0, 0);

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
void qtractorPluginList::setChannels (
	unsigned short iChannels, unsigned int iFlags )
{
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

	// Go, go, go...
	m_iFlags = iFlags;

	// Allocate new MIDI manager, if applicable...
	if ((iChannels > 0) && (m_iFlags & Midi)) {
		m_pMidiProgramSubject = new MidiProgramSubject(m_iMidiBank, m_iMidiProg);
		m_pMidiManager = qtractorMidiManager::createMidiManager(this);
		qtractorAudioBus *pAudioOutputBus
			= m_pMidiManager->audioOutputBus();
		if (pAudioOutputBus) {
			// Override number of channels from audio output bus...
			iChannels = pAudioOutputBus->channels();
			// Restore it's connections if dedicated...
			if (m_pMidiManager->isAudioOutputBus())
				pAudioOutputBus->outputs().copy(m_audioOutputs);
		}
	}

	// Allocate all new interim buffers...
	setChannelsEx(iChannels, false);

	// FIXME: This should be better managed...
	if (m_pMidiManager)
		m_pMidiManager->updateInstruments();
}


void qtractorPluginList::setChannelsEx (
	unsigned short iChannels, bool bReset )
{
#if 0
	// Maybe we don't need to change a thing here...
	if (iChannels == m_iChannels)
		return;
#endif

	unsigned short i;

	// Delete old interim buffer...
	if (m_pppBuffers[1]) {
		for (i = 0; i < m_iChannels; ++i)
			delete [] m_pppBuffers[1][i];
		delete [] m_pppBuffers[1];
		m_pppBuffers[1] = NULL;
	}

	// Go, go, go...
	m_iChannels = iChannels;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	const unsigned int iBufferSize = pAudioEngine->bufferSize();

	// Allocate new interim buffer...
	if (m_iChannels > 0) {
		m_pppBuffers[1] = new float * [m_iChannels];
		for (i = 0; i < m_iChannels; ++i) {
			m_pppBuffers[1][i] = new float [iBufferSize];
			::memset(m_pppBuffers[1][i], 0, iBufferSize * sizeof(float));
		}
	}

	// Reset all plugin chain channels...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (bReset && m_iChannels > 0) {
			pPlugin->freezeConfigs();
			pPlugin->freezeValues();
		}
		pPlugin->setChannels(m_iChannels);
		if (bReset && m_iChannels > 0) {
			pPlugin->realizeConfigs();
			pPlugin->realizeValues();
			pPlugin->releaseConfigs();
			pPlugin->releaseValues();
		}
	}
}


// Reset and (re)activate all plugin chain.
void qtractorPluginList::resetBuffers (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	const unsigned int iBufferSize = pAudioEngine->bufferSize();

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
			::memset(m_pppBuffers[1][i], 0, iBufferSize * sizeof(float));
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


// Add-guarded plugin method.
void qtractorPluginList::addPlugin ( qtractorPlugin *pPlugin )
{
	// Link the plugin into list...
	insertPlugin(pPlugin, pPlugin->next());
}


// Insert-guarded plugin method.
void qtractorPluginList::insertPlugin (
	qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin )
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

	// update plugins for auto-plugin-deactivation...
	autoDeactivatePlugins(m_bAutoDeactivated, true);
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
		// Move all plugin automation curves...
		qtractorCurveList *pCurveList = pPluginList->curveList();
		// Activation automation/curve...
		qtractorCurve *pCurve = pPlugin->activateSubject()->curve();
		if (pCurve && pCurve->list() == pCurveList) {
			pCurveList->removeCurve(pCurve);
			m_pCurveList->addCurve(pCurve);
		}
		pPlugin->activateObserver()->setCurveList(m_pCurveList);
		// Parameters automation/curves...
		const qtractorPlugin::Params& params = pPlugin->params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			pCurve = pParam->subject()->curve();
			if (pCurve && pCurve->list() == pCurveList) {
				pCurveList->removeCurve(pCurve);
				m_pCurveList->addCurve(pCurve);
			}
			pParam->observer()->setCurveList(m_pCurveList);
		}
		// Now for the real thing...
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

	// update (both) lists for Auto-plugin-deactivation
	autoDeactivatePlugins(m_bAutoDeactivated, true);
	if (pPluginList != this)
		pPluginList->autoDeactivatePlugins(m_bAutoDeactivated, true);
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

	// update Plugins for Auto-plugin-deactivation
	autoDeactivatePlugins(m_bAutoDeactivated, true);
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
	const QString& sFilename = pType->filename();
	qtractorPlugin *pNewPlugin = qtractorPluginFactory::createPlugin(this,
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
		pNewPlugin->setActivated(pPlugin->isActivatedEx());
		pNewPlugin->setDirectAccessParamIndex(
			pPlugin->directAccessParamIndex());
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
	const int iView = m_views.indexOf(pView);
	if (iView >= 0)
		m_views.removeAt(iView);
}


// The meta-main audio-processing plugin-chain procedure.
void qtractorPluginList::process ( float **ppBuffer, unsigned int nframes )
{
	// Sanity checks...
	if (!isActivated())
		return;

	if (ppBuffer == NULL || *ppBuffer == NULL || m_pppBuffers[1] == NULL)
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


// Create/load plugin state.
qtractorPlugin *qtractorPluginList::loadPlugin ( QDomElement *pElement )
{
	qtractorPlugin *pPlugin = NULL;

	QString sFilename;
	QString sLabel;
	QString sPreset;
	QStringList vlist;
	unsigned long iUniqueID = 0;
	unsigned long iIndex = 0;
	bool bActivated = false;
	unsigned long iActivateSubjectIndex = 0;
	long iDirectAccessParamIndex = -1;
	qtractorPlugin::Configs configs;
	qtractorPlugin::ConfigTypes ctypes;
	qtractorPlugin::Values values;
	qtractorMidiControl::Controllers controllers;
	qtractorCurveFile cfile(qtractorPluginList::curveList());
	QPoint posEditor;
	QPoint posForm;
	int iEditorType = -1;

	const QString& sTypeHint = pElement->attribute("type");
	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(sTypeHint);
	for (QDomNode nParam = pElement->firstChild();
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
		if (eParam.tagName() == "activate-subject-index")
			iActivateSubjectIndex = eParam.text().toULong();
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
		else
		if (eParam.tagName() == "editor-pos") {
			const QStringList& sxy = eParam.text().split(',');
			posEditor.setX(sxy.at(0).toInt());
			posEditor.setY(sxy.at(1).toInt());
		}
		else
		if (eParam.tagName() == "form-pos") {
			const QStringList& sxy = eParam.text().split(',');
			posForm.setX(sxy.at(0).toInt());
			posForm.setY(sxy.at(1).toInt());
		}
		else
		if (eParam.tagName() == "editor-type")
			iEditorType = eParam.text().toInt();
	}

	// Try to find some alternative, if it doesn't exist...
	if (checkPluginFile(sFilename, typeHint)) {
		pPlugin = qtractorPluginFactory::createPlugin(this,
			sFilename, iIndex, typeHint);
	}

#if 0
	if (!sFilename.isEmpty() && !sLabel.isEmpty() &&
		((pPlugin == NULL) || ((pPlugin->type())->label() != sLabel))) {
		iIndex = 0;
		do {
			if (pPlugin) delete pPlugin;
			pPlugin = qtractorPluginFile::createPlugin(this,
				sFilename, iIndex++, typeHint);
		} while (pPlugin && (pPlugin->type())->label() != sLabel);
	}
#endif

	if (pPlugin) {
		if (iUniqueID > 0)
			pPlugin->setUniqueID(iUniqueID);
		if (iActivateSubjectIndex > 0)
			pPlugin->setActivateSubjectIndex(iActivateSubjectIndex);
		pPlugin->setPreset(sPreset);
		pPlugin->setConfigs(configs);
		pPlugin->setConfigTypes(ctypes);
		if (!vlist.isEmpty())
			pPlugin->setValueList(vlist);
		if (!values.index.isEmpty())
			pPlugin->setValues(values);
	//	append(pPlugin);
		pPlugin->mapControllers(controllers);
		pPlugin->applyCurveFile(&cfile);
		pPlugin->setDirectAccessParamIndex(iDirectAccessParamIndex);
		pPlugin->setActivated(bActivated); // Later's better!
		pPlugin->setEditorPos(posEditor);
		pPlugin->setFormPos(posForm);
		if (iEditorType >= 0)
			pPlugin->setEditorType(iEditorType);
	} else {
		qtractorMessageList::append(
			QObject::tr("%1(%2): %3 plugin not found.")
				.arg(sFilename).arg(iIndex).arg(sTypeHint));
	}

	// Cleanup.
	qDeleteAll(controllers);
	controllers.clear();

	return pPlugin;
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

	m_bLatency = false;
	m_iLatency = 0;

	// Load plugin-list children...
	for (QDomNode nPlugin = pElement->firstChild();
			!nPlugin.isNull();
				nPlugin = nPlugin.nextSibling()) {

		// Convert plugin node to element...
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
			qtractorPlugin *pPlugin = loadPlugin(&ePlugin);
			if (pPlugin)
				append(pPlugin);
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
		// Load audio output monitor flag...
		if (ePlugin.tagName() == "audio-output-monitor") {
			m_bAudioOutputMonitor = qtractorDocument::boolFromText(ePlugin.text());
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
		// Create the new plugin element...
		QDomElement ePlugin = pDocument->document()->createElement("plugin");
		pPlugin->savePluginEx(pDocument, &ePlugin);
		// Add this plugin...
		pElement->appendChild(ePlugin);
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
		pDocument->saveTextElement("audio-output-monitor",
			qtractorDocument::textFromBool(
				m_pMidiManager->isAudioOutputMonitor()), pElement);
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


void qtractorPluginList::autoDeactivatePlugins ( bool bDeactivated, bool bForce )
{
	if (m_bAutoDeactivated != bDeactivated || bForce) {
		m_bAutoDeactivated  = bDeactivated;
		if (bDeactivated) {
			bool bStopDeactivation = false;
			// pass to all plugins bottom to top / stop for active plugins
			// possibly connected to other tracks
			qtractorPlugin *pPlugin = last();
			for ( ;	pPlugin && !bStopDeactivation; pPlugin = pPlugin->prev()) {
				if (pPlugin->canBeConnectedToOtherTracks())
					bStopDeactivation = pPlugin->isActivated();
				else
					pPlugin->autoDeactivatePlugin(bDeactivated);
			}
			// (re)activate all above stopper
			if (bStopDeactivation) {
				for ( ; pPlugin; pPlugin = pPlugin->prev()) {
					pPlugin->autoDeactivatePlugin(false);
				}
			}
		} else {
			// pass to all plugins top to to bottom
			for (qtractorPlugin *pPlugin = first();
					pPlugin; pPlugin = pPlugin->next()) {
				pPlugin->autoDeactivatePlugin(bDeactivated);
			}
		}
		// inform all views
		QListIterator<qtractorPluginListView *> iter(m_views);
		while (iter.hasNext()) {
			qtractorPluginListView *pListView = iter.next();
			pListView->refresh();
		}
	}
}


bool qtractorPluginList::isAutoDeactivated (void) const
{
	return m_bAutoDeactivated;
}


// Check/sanitize plugin file-path;
bool qtractorPluginList::checkPluginFile (
	QString& sFilename, qtractorPluginType::Hint typeHint ) const
{
	// Care of internal pseudo-plugins...
	if (sFilename.isEmpty()) {
		return (typeHint == qtractorPluginType::Insert)
			|| (typeHint == qtractorPluginType::AuxSend);
	}

	// LV2 plug-ins are identified by URI...
	if (typeHint == qtractorPluginType::Lv2)
		return true;

	// Primary check for plugin pathname...
	QFileInfo fi(sFilename);
	if (fi.exists() && fi.isReadable())
		return true;

	// Otherwise search for an alternative
	// under each respective search paths...
	qtractorPluginFactory *pPluginFactory
		= qtractorPluginFactory::getInstance();
	if (pPluginFactory) {
		const QString fname = fi.fileName();
		QStringListIterator iter(pPluginFactory->pluginPaths(typeHint));
		while (iter.hasNext()) {
			fi.setFile(QDir(iter.next()), fname);
			if (fi.exists() && fi.isReadable()) {
				sFilename = fi.absoluteFilePath();
				return true;
			}
		}
	}

	// No alternative has been found, sorry.
	return false;
}


// Recalculate plugin chain total latency (in frames)...
void qtractorPluginList::resetLatency (void)
{
	m_iLatency = 0;

	if (!m_bLatency)
		return;

	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated()) {
			// HACK: Dummy plugin processing for no single
			// frame, hopefully updating any output ports...
			if (m_iChannels > 0 && m_pppBuffers[1]) {
				float **ppIDummy = m_pppBuffers[1];
				float **ppODummy = ppIDummy;
				pPlugin->process(ppIDummy, ppODummy, 0);
			}
			// Accumulate latency...
			m_iLatency += pPlugin->latency();
		}
	}
}


//-------------------------------------------------------------------------
// qtractorPluginList::Document -- Plugins file import/export helper class.
//

// Constructor.
qtractorPluginList::Document::Document (
	QDomDocument *pDocument, qtractorPluginList *pPluginList )
	: qtractorDocument(pDocument, "plugin-list"), m_pPluginList(pPluginList)
{
}

// Default destructor.
qtractorPluginList::Document::~Document (void)
{
}


// Property accessors.
qtractorPluginList *qtractorPluginList::Document::pluginList (void) const
{
	return m_pPluginList;
}


//-------------------------------------------------------------------------
// qtractorPluginList::Document -- loaders.
//

// External storage simple load method.
bool qtractorPluginList::Document::load ( const QString& sFilename )
{
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// Parse it a-la-DOM :-)
	QDomDocument *pDocument = document();
	if (!pDocument->setContent(&file)) {
		file.close();
		return false;
	}

	file.close();

	QDomElement elem = pDocument->documentElement();
	// Get root element and check for proper taq name.
	if (elem.tagName() != "plugin-list")
		return false;

	return loadElement(&elem);
}


// Elemental loader...
bool qtractorPluginList::Document::loadElement ( QDomElement *pElement )
{
	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return false;

	qtractorImportPluginsCommand *pImportCommand
		= new qtractorImportPluginsCommand();

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		pImportCommand->removePlugin(pPlugin);
	}

	// Load plugin-list children...
	for (QDomNode nPlugin = pElement->firstChild();
			!nPlugin.isNull();
				nPlugin = nPlugin.nextSibling()) {
		// Convert plugin node to element...
		QDomElement ePlugin = nPlugin.toElement();
		if (ePlugin.isNull())
			continue;
		if (ePlugin.tagName() == "plugin") {
			qtractorPlugin *pPlugin = m_pPluginList->loadPlugin(&ePlugin);
			if (pPlugin) {
				pPlugin->realizeConfigs();
				pPlugin->realizeValues();
				pPlugin->releaseConfigs();
				pPlugin->releaseValues();
				pImportCommand->addPlugin(pPlugin);
			}
		}
	}

	pSession->execute(pImportCommand);

	return true;
}


//-------------------------------------------------------------------------
// qtractorPluginList::Document -- savers.
//

// External storage simple save method.
bool qtractorPluginList::Document::save ( const QString& sFilename )
{
	QFile file(sFilename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;

	QDomDocument *pDocument = document();
	QDomElement elem = pDocument->createElement("plugin-list");
	saveElement(&elem);
	pDocument->appendChild(elem);

	QTextStream ts(&file);
	ts << pDocument->toString() << endl;
	file.close();

	return true;
}


// Elemental saver...
bool qtractorPluginList::Document::saveElement ( QDomElement *pElement )
{
	// Save this program version (informational)...
	pElement->setAttribute("version", PACKAGE_STRING);

	// Save plugins...
	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		// Create the new plugin element...
		QDomElement ePlugin = document()->createElement("plugin");
		pPlugin->savePlugin(this, &ePlugin);
		// Add this plugin...
		pElement->appendChild(ePlugin);
	}

	return true;
}


// end of qtractorPlugin.cpp
