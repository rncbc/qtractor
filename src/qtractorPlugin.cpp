// qtractorPlugin.cpp
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

#include "qtractorAbout.h"
#include "qtractorPlugin.h"
#include "qtractorPluginFactory.h"
#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"
#include "qtractorPluginForm.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"

#include "qtractorInsertPlugin.h"

#include "qtractorMainForm.h"
#include "qtractorOptions.h"

#include "qtractorSession.h"
#include "qtractorCurveFile.h"

#include "qtractorFileList.h"

#include "qtractorMessageList.h"

#include <QDomDocument>
#include <QDomElement>
#include <QTextStream>

#include <QLibrary>
#include <QFileInfo>
#include <QFile>
#include <QDir>

#include <algorithm>

#include <cmath>

#include <dlfcn.h>


#if QT_VERSION < QT_VERSION_CHECK(4, 5, 0)
namespace Qt {
const WindowFlags WindowCloseButtonHint = WindowFlags(0x08000000);
}
#endif

// Deprecated QTextStreamFunctions/Qt namespaces workaround.
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#define endl	Qt::endl
#endif


//----------------------------------------------------------------------------
// qtractorPluginFile -- Plugin file library instance.
//

// Executive methods.
bool qtractorPluginFile::open (void)
{
	// Check whether already open...
	if (m_module || ++m_iOpenCount > 1)
		return true;

	// Do the opening dance...
	if (m_module == nullptr) {
		const QByteArray aFilename = m_sFilename.toUtf8();
		m_module = ::dlopen(aFilename.constData(), RTLD_LOCAL | RTLD_LAZY);
	}

	// Done alright.
	return (m_module != nullptr);
}


void qtractorPluginFile::close (void)
{
	if (!m_module || --m_iOpenCount > 0)
		return;

	// ATTN: Might be really needed, as it would
	// otherwise pile up hosing all available RAM
	// until freed and unloaded on exit();
	// nb. some VST might choke on auto-unload.
	if (m_bAutoUnload) {
		::dlclose(m_module);
		m_module = nullptr;
	}
}


// Symbol resolver.
void *qtractorPluginFile::resolve ( const char *symbol )
{
	return (m_module ? ::dlsym(m_module, symbol) : nullptr);
}


// Plugin file resgistry methods.
qtractorPluginFile::Files qtractorPluginFile::g_files;

qtractorPluginFile *qtractorPluginFile::addFile ( const QString& sFilename )
{
	qtractorPluginFile *pFile = g_files.value(sFilename, nullptr);

	if (pFile == nullptr && QLibrary::isLibrary(sFilename)
	#ifdef CONFIG_CLAP
		|| QFileInfo(sFilename).suffix() == "clap"
	#endif
	) {
		pFile = new qtractorPluginFile(sFilename);
		g_files.insert(pFile->filename(), pFile);
	}

	if (pFile && !pFile->open())
		pFile = nullptr;

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
	if (sText == "VST" || sText == "VST2")
		return Vst2;
	else
	if (sText == "VST3")
		return Vst3;
	else
	if (sText == "CLAP")
		return Clap;
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
	if (typeHint == Vst2)
		return "VST2";
	else
	if (typeHint == Vst3)
		return "VST3";
	else
	if (typeHint == Clap)
		return "CLAP";
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
		m_iActivated(0), m_bActivated(false), m_bAutoDeactivated(false),
		m_activateObserver(this), m_iActivateSubjectIndex(0),
		m_pLastUpdatedParam(nullptr), m_pLastUpdatedProperty(nullptr),
		m_pForm(nullptr), m_iEditorType(-1),
		m_iDirectAccessParamIndex(-1)
{
	// Acquire a local unique id in chain...
	if (m_pList && m_pType)
		m_iUniqueID = m_pList->createUniqueID(m_pType);

	// Set default instance label...
	if (m_pType)
		m_sLabel = m_pType->label();

	// Activate subject properties.
	m_activateSubject.setName(QObject::tr("Activate"));
	m_activateSubject.setToggled(true);
}


// Destructor.
qtractorPlugin::~qtractorPlugin (void)
{
	// Clear out all dependables...
	clearItems();
	clearParams();
	clearProperties();

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
		closeForm(true);
	}

	m_iInstances = iInstances;
}


// Activation methods.

// immediate
void qtractorPlugin::setActivated ( bool bActivated )
{
	updateActivated(bActivated);

	setActivatedEx(bActivated);
}

bool qtractorPlugin::isActivated (void) const
{
	return m_bActivated;
}


// queued (GUI invocation)
void qtractorPlugin::setActivatedEx ( bool bActivated )
{
	m_activateSubject.setValue(bActivated ? 1.0f : 0.0f);
}

bool qtractorPlugin::isActivatedEx (void) const
{
	return (m_activateSubject.value() > 0.5f);
}


// auto-(de)activation
bool qtractorPlugin::isAutoActivated (void) const
{
	return m_bActivated && !m_bAutoDeactivated;
}

bool qtractorPlugin::isAutoDeactivated (void) const
{
	return m_bActivated && m_bAutoDeactivated;
}



void qtractorPlugin::autoDeactivatePlugin ( bool bDeactivated )
{
	if (bDeactivated != m_bAutoDeactivated) {
		// deactivate?
		if (bDeactivated) {
			// was activated?
			if (m_bActivated) {
				m_iActivated = 0;
				deactivate();
				if (m_pList)
					m_pList->updateActivated(false);
			}
		}
		else
		// reactivate?
		if (m_bActivated) {
			m_iActivated = 0;
			activate();
			if (m_pList)
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
	if (( bActivated && !m_bActivated) ||
		(!bActivated &&  m_bActivated)) {
		// First time activate?
		if (bActivated && m_iActivated == 0) {
			activate();
			++m_iActivated;
		}
		// Let the change be...
		m_bActivated = bActivated;
		// Last time deactivate?
		if (!bActivated && m_iActivated == 0)
			deactivate();
		// Auto-plugin-deactivation overrides standard-activation for plugins
		// without connections to other tracks (Inserts/AuxSends)
		// otherwise user could (de)activate plugin without getting feedback
		const bool bIsConnectedToOtherTracks = canBeConnectedToOtherTracks();
		if (!m_bAutoDeactivated || bIsConnectedToOtherTracks) {
			if (m_pList)
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
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	if (pAudioEngine->isFreewheel())
		return;

	QListIterator<qtractorPluginListItem *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateActivated();

	updateFormActivated();
}


// Internal activation methods.
void qtractorPlugin::setChannelsActivated (
	unsigned short iChannels, bool bActivated )
{
	if (iChannels > 0) {
		// First time activation?...
		if (!bActivated) ++m_iActivated;
		setActivated(bActivated);
		if (!bActivated) m_iActivated = 0;
	} else {
		m_iActivated = 0;
		updateActivated(bActivated);
	}
}


// Internal deactivation cleanup.
void qtractorPlugin::cleanup (void)
{
	m_bActivated = false;

	setChannels(0);
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

	m_pPlugin->updateActivatedEx(m_pPlugin->isActivatedEx());
}


// Plugin state serialization methods.
void qtractorPlugin::setValueList ( const QStringList& vlist )
{
//	qSort(m_params); -- does not work with QHash...
	Params params;
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator& param_end = m_params.constEnd();
	for ( ; param != param_end; ++param)
		params.insert(param.key(), param.value());

	// Split it up...
	clearValues();
	QStringListIterator val(vlist);
	QMapIterator<unsigned long, Param *> iter(params);
	while (val.hasNext() && iter.hasNext())
		m_values.index[iter.next().key()] = val.next().toFloat();
}

QStringList qtractorPlugin::valueList (void) const
{
//	qSort(m_params); -- does not work with QHash...
	Params params;
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator& param_end = m_params.constEnd();
	for ( ; param != param_end; ++param)
		params.insert(param.key(), param.value());

	// Join it up...
	QStringList vlist;
	QMapIterator<unsigned long, Param *> iter(params);
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


// Default copy/pass-through plugin processing procedure. (virtual)
void qtractorPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	const unsigned short iChannels = channels();
	for (unsigned short i = 0; i < iChannels; ++i)
		::memcpy(ppOBuffer[i], ppIBuffer[i], nframes * sizeof(float));
}


// Nominal plugin user-title (virtual).
QString qtractorPlugin::title (void) const
{
	QString sTitle = m_sAlias;

	if (sTitle.isEmpty() && m_pType)
		sTitle = m_pType->name();

	return sTitle;
}


// Update editor title.
void qtractorPlugin::updateEditorTitle (void)
{
	QString sEditorTitle;

	if (m_pType && !alias().isEmpty())
		sEditorTitle.append(QString("%1: ").arg(m_pType->name()));

	sEditorTitle.append(title());

	if (m_pList && !m_pList->name().isEmpty())
		sEditorTitle.append(QString(" - %1").arg(m_pList->name()));

	setEditorTitle(sEditorTitle);

	if (m_pForm) {
		sEditorTitle = editorTitle();
		if (m_pType
			&& m_pType->typeHint() == qtractorPluginType::AuxSend
			&& alias().isEmpty())
			sEditorTitle = QObject::tr("Aux Send: %1").arg(sEditorTitle);
		m_pForm->setWindowTitle(sEditorTitle);
	}
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


// Paremeters list accessors.
void qtractorPlugin::addParam ( qtractorPlugin::Param *pParam )
{
	pParam->reset();
	if (pParam->isLogarithmic())
		pParam->observer()->setLogarithmic(true);
	m_params.insert(pParam->index(), pParam);
	m_paramNames.insert(pParam->name(), pParam);
}


void qtractorPlugin::removeParam ( qtractorPlugin::Param *pParam )
{
	m_paramNames.remove(pParam->name());
	m_params.remove(pParam->index());
}


void qtractorPlugin::clearParams (void)
{
	qDeleteAll(m_params);
	m_params.clear();
	m_paramNames.clear();
}


// Properties registry accessor.
void qtractorPlugin::addProperty ( qtractorPlugin::Property *pProp )
{
	m_properties.insert(pProp->index(), pProp);
	m_propertyKeys.insert(pProp->key(), pProp);
}


void qtractorPlugin::removeProperty ( qtractorPlugin::Property *pProp )
{
	m_propertyKeys.remove(pProp->name());
	m_properties.remove(pProp->index());
}


void qtractorPlugin::clearProperties (void)
{
	qDeleteAll(m_properties);
	m_properties.clear();
	m_propertyKeys.clear();
}


// Special plugin form methods.
void qtractorPlugin::openForm ( QWidget *pParent )
{
	// Take the change and create the form if it doesn't current exist.
	const bool bCreate = (m_pForm == nullptr);

	if (bCreate) {
		// Build up the plugin form...
		// What style do we create tool childs?
		Qt::WindowFlags wflags = Qt::Window;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && pOptions->bKeepToolsOnTop) {
			wflags |= Qt::Tool;
		//	wflags |= Qt::WindowStaysOnTopHint;
		#if 0//QTRACTOR_PLUGIN_FORM_TOOL_PARENT
			// Make sure it has a parent...
			if (pParent == nullptr)
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


void qtractorPlugin::closeForm ( bool bForce )
{
	if (m_pForm == nullptr)
		return;

	if (bForce) {
		m_pForm->close();
		delete m_pForm;
		m_pForm = nullptr;
	}
	else
	if (m_pForm->isVisible())
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


void qtractorPlugin::updateFormActivated (void)
{
	if (m_pForm && m_pForm->isVisible())
		m_pForm->updateActivated();
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
	QWidget *pWidget, const QPoint& pos ) const
{
	QPoint wpos(pos);

	if (wpos.isNull() || wpos.x() < 0 || wpos.y() < 0) {
		QWidget *pParent = pWidget->parentWidget();
		if (pParent == nullptr)
			pParent = qtractorMainForm::getInstance();
		if (pParent) {
			QRect wrect(pWidget->geometry());
			wrect.moveCenter(pParent->geometry().center());
			wpos = wrect.topLeft();
		}
	}

	if (!wpos.isNull() && wpos.x() >= 0 && wpos.y() >= 0)
		pWidget->move(wpos);
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

	std::sort(list.begin(), list.end());

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
	freezeValues();

	QFileInfo fi(sFilename);

	QDomDocument doc("qtractorPlugin");

	QDomElement ePreset = doc.createElement("preset");
	ePreset.setAttribute("type", presetPrefix());
	ePreset.setAttribute("name", fi.baseName());
	ePreset.setAttribute("version", PROJECT_VERSION);

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
	if (pOptions == nullptr)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == nullptr)
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
qtractorPlugin::Param *qtractorPlugin::paramFromName ( const QString& sName ) const
{
	return m_paramNames.value(sName, nullptr);
}


// Direct access parameter
qtractorPlugin::Param *qtractorPlugin::directAccessParam (void) const
{
	if (isDirectAccessParam())
		return findParam(m_iDirectAccessParamIndex);
	else
		return nullptr;
}


void qtractorPlugin::setDirectAccessParamIndex ( long iDirectAccessParamIndex )
{
	m_iDirectAccessParamIndex = iDirectAccessParamIndex;

	updateListViews();
}


long qtractorPlugin::directAccessParamIndex (void) const
{
	return m_iDirectAccessParamIndex;
}


bool qtractorPlugin::isDirectAccessParam (void) const
{
	return (m_iDirectAccessParamIndex >= 0);
}


// Get all or some visual changes be announced....
void qtractorPlugin::updateListViews ( bool bRefresh )
{
	if (m_pList) {
		QListIterator<qtractorPluginListView *> iter(m_pList->views());
		while (iter.hasNext()) {
			if (bRefresh)
				iter.next()->refresh();
			else
				iter.next()->viewport()->update();
		}
	}
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
		Param *pParam = param.value();
		if (pParam->isValueEnabled()) {
			const unsigned long iIndex = pParam->index();
			m_values.names[iIndex] = pParam->name();
			m_values.index[iIndex] = pParam->value();
		}
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
	qtractorMidiManager *pMidiManager = nullptr;
	if (m_pList)
		pMidiManager = m_pList->midiManager();
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
		const unsigned long iIndex = param.key();
		Param *pParam = findParam(iIndex);
		const QString& sName = m_values.names.value(iIndex);
		if (!sName.isEmpty() && !(pParam && sName == pParam->name())) {
			Param *pParamEx = m_paramNames.value(sName, nullptr);
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
			const unsigned long iIndex = eParam.attribute("index").toULong();
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
#if 0
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		Param *pParam = param.value();
		QDomElement eParam = pDocument->createElement("param");
		eParam.setAttribute("name", pParam->name());
		eParam.setAttribute("index", QString::number(pParam->index()));
		eParam.appendChild(
			pDocument->createTextNode(QString::number(pParam->value())));
		pElement->appendChild(eParam);
	}
#else
	ValueIndex::ConstIterator param = m_values.index.constBegin();
	const ValueIndex::ConstIterator& param_end = m_values.index.constEnd();
	for ( ; param != param_end; ++param) {
		const unsigned long iIndex = param.key();
		const QString& sName = m_values.names.value(iIndex);
		QDomElement eParam = pDocument->createElement("param");
		eParam.setAttribute("name", sName);
		eParam.setAttribute("index", QString::number(iIndex));
		eParam.appendChild(
			pDocument->createTextNode(QString::number(param.value())));
		pElement->appendChild(eParam);
	}
#endif
}


// Parameter update executive.
void qtractorPlugin::updateParamValue (
	unsigned long iIndex, float fValue, bool bUpdate )
{
	Param *pParam = findParam(iIndex);
	if (pParam)
		pParam->updateValue(fValue, bUpdate);
}


// Load plugin parameter controllers (MIDI).
void qtractorPlugin::loadControllers (
	QDomElement *pElement, qtractorMidiControl::Controllers& controllers )
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == nullptr)
		return;

	pMidiControl->loadControllers(pElement, controllers);
}


// Save plugin parameter controllers (MIDI).
void qtractorPlugin::saveControllers (
	qtractorDocument *pDocument, QDomElement *pElement )
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl == nullptr)
		return;

	qtractorMidiControl::Controllers controllers;

	unsigned long iActivateSubjectIndex = activateSubjectIndex();
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		Param *pParam = param.value();
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

	Properties::ConstIterator prop = m_properties.constBegin();
	const Properties::ConstIterator prop_end = m_properties.constEnd();
	for ( ; prop != prop_end; ++prop) {
		Property *pProp = prop.value();
		qtractorMidiControlObserver *pObserver = pProp->observer();
		if (pMidiControl->isMidiObserverMapped(pObserver)) {
			qtractorMidiControl::Controller *pController
				= new qtractorMidiControl::Controller;
			pController->name = pProp->key();
			pController->index = pProp->key_index();
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
	if (pMidiControl == nullptr)
		return;

	const unsigned long iActivateSubjectIndex = activateSubjectIndex();
	QListIterator<qtractorMidiControl::Controller *> iter(controllers);
	while (iter.hasNext()) {
		qtractorMidiControl::Controller *pController = iter.next();
		qtractorMidiControlObserver *pObserver = nullptr;
		if ((iActivateSubjectIndex > 0 &&
			 iActivateSubjectIndex == pController->index) ||
			(activateSubject()->name() == pController->name)) {
			pObserver = activateObserver();
		//	setActivateSubjectIndex(0); // hack down!
		//	iActivateSubjectIndex = 0;
		} else {
			Param *pParam = nullptr;
			Property *pProp = nullptr;
			if (!pController->name.isEmpty()) {
				pProp = m_propertyKeys.value(pController->name, nullptr);
				if (pProp && pController->index == pProp->key_index())
					pObserver = pProp->observer();
				else
					pParam = m_paramNames.value(pController->name, nullptr);
			}
			if (pParam == nullptr && pProp == nullptr)
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
	if (pCurveFile == nullptr)
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	pCurveFile->clear();
	pCurveFile->setBaseDir(pSession->sessionDir());

	unsigned short iItem = 0;
	unsigned long iActivateSubjectIndex = activateSubjectIndex();
	Params::ConstIterator param = m_params.constBegin();
	const Params::ConstIterator param_end = m_params.constEnd();
	for ( ; param != param_end; ++param) {
		Param *pParam = param.value();
		qtractorCurve *pCurve = pParam->subject()->curve();
		if (pCurve) {
			qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
			pCurveItem->name = pParam->name();
			pCurveItem->index = pParam->index();
			if (pParam->isToggled()	|| pParam->isInteger()) {
				const unsigned short controller = (iItem % 0x7f);
				if (controller == 0x00 || controller == 0x20)
					++iItem; // Avoid bank-select controllers, please.
				pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
				pCurveItem->param = (iItem % 0x7f);
			} else {
				pCurveItem->ctype = qtractorMidiEvent::NONREGPARAM;
				pCurveItem->param = (iItem % 0x3fff);
			}
			pCurveItem->channel = ((iItem / 0x7f) % 16);
			pCurveItem->mode = pCurve->mode();
			pCurveItem->process = pCurve->isProcess();
			pCurveItem->capture = pCurve->isCapture();
			pCurveItem->locked = pCurve->isLocked();
			pCurveItem->logarithmic = pCurve->isLogarithmic();
			pCurveItem->color = pCurve->color();
			pCurveItem->subject = pCurve->subject();
			pCurveFile->addItem(pCurveItem);
			++iItem;
		}
		if (iActivateSubjectIndex < pParam->index())
			iActivateSubjectIndex = pParam->index();
	}

	Properties::ConstIterator prop = m_properties.constBegin();
	const Properties::ConstIterator prop_end = m_properties.constEnd();
	for ( ; prop != prop_end; ++prop) {
		Property *pProp = prop.value();
		if (!pProp->isAutomatable())
			continue;
		qtractorCurve *pCurve = pProp->subject()->curve();
		if (pCurve) {
			qtractorCurveFile::Item *pCurveItem = new qtractorCurveFile::Item;
			pCurveItem->name = pProp->key();
			pCurveItem->index = pProp->key_index();
			if (pProp->isToggled()	|| pProp->isInteger()) {
				const unsigned short controller = (iItem % 0x7f);
				if (controller == 0x00 || controller == 0x20)
					++iItem; // Avoid bank-select controllers, please.
				pCurveItem->ctype = qtractorMidiEvent::CONTROLLER;
				pCurveItem->param = (iItem % 0x7f);
			} else {
				pCurveItem->ctype = qtractorMidiEvent::NONREGPARAM;
				pCurveItem->param = (iItem % 0x3fff);
			}
			pCurveItem->channel = ((iItem / 0x7f) % 16);
			pCurveItem->mode = pCurve->mode();
			pCurveItem->process = pCurve->isProcess();
			pCurveItem->capture = pCurve->isCapture();
			pCurveItem->locked = pCurve->isLocked();
			pCurveItem->logarithmic = pCurve->isLogarithmic();
			pCurveItem->color = pCurve->color();
			pCurveItem->subject = pCurve->subject();
			pCurveFile->addItem(pCurveItem);
			++iItem;
		}
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
		const unsigned short controller = (iItem % 0x7f);
		if (controller == 0x00 || controller == 0x20)
			++iItem; // Avoid bank-select controllers, please.
		pCurveItem->param = (iItem % 0x7f);
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

	const bool bTemporary = pDocument->isTemporary();
	const QString& sFilename
		= pSession->createFilePath(sBaseName, "mid", !bTemporary);
	pSession->files()->addFileItem(qtractorFileList::Midi, sFilename, bTemporary);
	pCurveFile->setFilename(sFilename);

	pCurveFile->save(pDocument, pElement, pSession->timeScale());
}


// Apply plugin automation curves (monitor, gain, pan, record, mute, solo).
void qtractorPlugin::applyCurveFile ( qtractorCurveFile *pCurveFile )
{
	if (pCurveFile == nullptr)
		return;
	if (pCurveFile->items().isEmpty())
		return;

	qtractorCurveList *pCurveList = pCurveFile->list();
	if (pCurveList == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
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
			Param *pParam = nullptr;
			Property *pProp = nullptr;
			if (!pCurveItem->name.isEmpty()) {
				pProp = m_propertyKeys.value(pCurveItem->name, nullptr);
				if (pProp && pCurveItem->index == pProp->key_index())
					pCurveItem->subject = pProp->subject();
				else
					pParam = m_paramNames.value(pCurveItem->name, nullptr);
			}
			if (pParam == nullptr && pProp == nullptr)
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
	freezeValues();

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

	const QString& sLabel = label();
	if (!sLabel.isEmpty())
		pDocument->saveTextElement("label", sLabel, pElement);

	const QString& sAlias = alias();
	if (!sAlias.isEmpty())
		pDocument->saveTextElement("alias", sAlias, pElement);

	const QString& sPreset = preset();
	if (!sPreset.isEmpty())
		pDocument->saveTextElement("preset", sPreset, pElement);

	const long iDirectAccessParamIndex = directAccessParamIndex();
	if (iDirectAccessParamIndex >= 0) {
		pDocument->saveTextElement("direct-access-param",
			QString::number(iDirectAccessParamIndex), pElement);
	}

	pDocument->saveTextElement("activated",
		qtractorDocument::textFromBool(isActivated()), pElement);

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
	releaseValues();

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
		if (iEditorType > 0) {
			pDocument->saveTextElement("editor-type",
				QString::number(iEditorType), pElement);
		}
	}

	return bResult;
}


//----------------------------------------------------------------------------
// qtractorPlugin::Param -- Plugin parameter (control input port) instance.
//

// Current port value.
void qtractorPlugin::Param::setValue ( float fValue, bool bUpdate )
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
		m_pPlugin->updateListViews();
}


// Parameter update executive method.
void qtractorPlugin::Param::updateValue ( float fValue, bool bUpdate )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPlugin::Param[%p]::updateValue(%g, %d)", this, fValue, int(bUpdate));
#endif

	// If immediately the same, make it directly dirty...
	if (m_pPlugin->isLastUpdatedParam(this)) {
		setValue(fValue, bUpdate);
		m_pPlugin->updateFormDirtyCount();
		return;
	}

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		pSession->execute(
			new qtractorPluginParamCommand(this, fValue, bUpdate));
	//	m_pPlugin->setLastUpdatedParam(this);
	}
}


// Virtual observer updater.
void qtractorPlugin::Param::update ( float fValue, bool bUpdate )
{
	qtractorPlugin *pPlugin = plugin();
	if (bUpdate && pPlugin->directAccessParamIndex() == long(index()))
		pPlugin->updateListViews();
	pPlugin->updateParam(this, fValue, bUpdate);
}


// Constructor.
qtractorPlugin::Param::Observer::Observer ( Param *pParam )
	: qtractorMidiControlObserver(pParam->subject()), m_pParam(pParam)
{
	setCurveList((pParam->plugin())->list()->curveList());
}


// Virtual observer updater.
void qtractorPlugin::Param::Observer::update ( bool bUpdate )
{
	m_pParam->update(qtractorMidiControlObserver::value(), bUpdate);

	qtractorMidiControlObserver::update(bUpdate);
}


//----------------------------------------------------------------------------
// qtractorPlugin::Property -- Plugin property (aka. parameter) instance.
//

// Current property value.
void qtractorPlugin::Property::setVariant ( const QVariant& value, bool bUpdate )
{
	// Whether it's a scalar value...
	//
	if (isAutomatable() && !bUpdate) {
		// Sanitize value...
		float fValue = value.toFloat();
		if (fValue > maxValue())
			fValue = maxValue();
		else
		if (fValue < minValue())
			fValue = minValue();
		setValue(fValue, false);
	}

	// Set main real value anyhow...
	m_value = value;
}


// Virtual observer updater.
void qtractorPlugin::Property::update ( float fValue, bool bUpdate )
{
	setVariant(fValue, bUpdate);
}


//----------------------------------------------------------------------------
// qtractorPluginList -- Plugin chain list instance.
//

// Constructor.
qtractorPluginList::qtractorPluginList (
	unsigned short iChannels, unsigned int iFlags )
	: m_iChannels(iChannels), m_iFlags(iFlags),
		m_iActivated(0), m_pMidiManager(nullptr),
		m_iMidiBank(-1), m_iMidiProg(-1),
		m_pMidiProgramSubject(nullptr),
		m_bAutoDeactivated(false),
		m_bAudioOutputMonitor(false),
		m_bLatency(false), m_iLatency(0)
{
	setAutoDelete(true);

	m_pppBuffers[0] = nullptr;
	m_pppBuffers[1] = nullptr;

	m_pCurveList = new qtractorCurveList();

	m_bAudioOutputBus
		= qtractorMidiManager::isDefaultAudioOutputBus();
	m_bAudioOutputAutoConnect
		= qtractorMidiManager::isDefaultAudioOutputAutoConnect();

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


// Set all plugin chain number of channels.
void qtractorPluginList::setChannels (
	unsigned short iChannels, unsigned int iFlags )
{
	// Destroy any MIDI manager still there...
	if (m_pMidiManager) {
		m_bAudioOutputBus = m_pMidiManager->isAudioOutputBus();
		m_bAudioOutputAutoConnect = m_pMidiManager->isAudioOutputAutoConnect();
		m_sAudioOutputBusName = m_pMidiManager->audioOutputBusName();
		m_bAudioOutputMonitor = m_pMidiManager->isAudioOutputMonitor();
		m_pMidiManager->setAudioOutputMonitorEx(false);
		qtractorMidiManager::deleteMidiManager(m_pMidiManager);
		m_pMidiManager = nullptr;
	}

	if (m_pMidiProgramSubject) {
		delete m_pMidiProgramSubject;
		m_pMidiProgramSubject = nullptr;
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
	setChannelsEx(iChannels);

	// Reset all plugins number of channels...
	const bool bAudioOuts = resetChannels(iChannels, false);

	// FIXME: This should be better managed...
	if (m_pMidiManager) {
		m_pMidiManager->setAudioOutputMonitorEx(bAudioOuts);
		m_pMidiManager->updateInstruments();
	}
}


void qtractorPluginList::setChannelsEx ( unsigned short iChannels )
{
#if 0
	// Maybe we don't need to change a thing here...
	if (iChannels == m_iChannels)
		return;
#endif

	// Delete old interim buffer...
	if (m_pppBuffers[1]) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			delete [] m_pppBuffers[1][i];
		delete [] m_pppBuffers[1];
		m_pppBuffers[1] = nullptr;
	}

	// Go, go, go...
	m_iChannels = iChannels;

	// Allocate new interim buffers...
	if (m_iChannels > 0) {
		qtractorAudioEngine *pAudioEngine = nullptr;
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pAudioEngine = pSession->audioEngine();
		if (pAudioEngine) {
			const unsigned int iBufferSizeEx = pAudioEngine->bufferSizeEx();
			m_pppBuffers[1] = new float * [m_iChannels];
			for (unsigned short i = 0; i < m_iChannels; ++i) {
				m_pppBuffers[1][i] = new float [iBufferSizeEx];
				::memset(m_pppBuffers[1][i], 0, iBufferSizeEx * sizeof(float));
			}
		}	// Gone terribly wrong...
		else m_iChannels = 0;
	}
}


// Reset all plugin chain number of channels.
bool qtractorPluginList::resetChannels (
	unsigned short iChannels, bool bReset )
{
	// Whether to turn on/off any audio monitors/meters later...
	unsigned short iAudioOuts = 0;

	// Reset all plugin chain channels...
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (bReset && iChannels > 0) {
			pPlugin->freezeConfigs();
			pPlugin->freezeValues();
		}
		pPlugin->setChannels(iChannels);
		if (bReset && iChannels > 0) {
			pPlugin->realizeConfigs();
			pPlugin->realizeValues();
			pPlugin->releaseConfigs();
			pPlugin->releaseValues();
		}
		iAudioOuts += pPlugin->audioOuts();
	}

	// Turn on/off audio monitors/meters whether applicable...
	return (iAudioOuts > 0);
}


// Reset and (re)activate all plugin chain.
void qtractorPluginList::resetBuffers (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == nullptr)
		return;

	const unsigned int iBufferSizeEx = pAudioEngine->bufferSizeEx();

	// Reset interim buffer, if any...
	if (m_pppBuffers[1]) {
		for (unsigned short i = 0; i < m_iChannels; ++i)
			::memset(m_pppBuffers[1][i], 0, iBufferSizeEx * sizeof(float));
	}
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

	// Update plugins for auto-plugin-deactivation...
	autoDeactivatePlugins(m_bAutoDeactivated, true);
}


// Move-guarded plugin method.
void qtractorPluginList::movePlugin (
	qtractorPlugin *pPlugin, qtractorPlugin *pNextPlugin )
{
	// Source sanity...
	if (pPlugin == nullptr)
		return;

	qtractorPluginList *pPluginList = pPlugin->list();
	if (pPluginList == nullptr)
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
			qtractorPlugin::Param *pParam = param.value();
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
	if (pType == nullptr)
		return nullptr;

	// Clone the plugin instance...
	pPlugin->freezeConfigs();
	pPlugin->freezeValues();

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
		pNewPlugin->setAlias(pPlugin->alias());
		pNewPlugin->setPreset(pPlugin->preset());
		// Special case for audio Aux-sends copied into output buses...
		if ((flags() & qtractorPluginList::AudioOutBus) &&
			(pType->typeHint() == qtractorPluginType::AuxSend) &&
			(pType->index() > 0)) { // index == channels > 0 => Audio aux-send.
			qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
				= static_cast<qtractorAudioAuxSendPlugin *> (pNewPlugin);
			if (pAudioAuxSendPlugin) {
				pAudioAuxSendPlugin->setAudioBusName(QString());
				pAudioAuxSendPlugin->freezeConfigs();
			}
		} else {
			// All other cases, proceed as usual...
			pNewPlugin->setConfigs(pPlugin->configs());
			pNewPlugin->setConfigTypes(pPlugin->configTypes());
		}
		pNewPlugin->setValues(pPlugin->values());
		pNewPlugin->realizeConfigs();
		pNewPlugin->realizeValues();
		pNewPlugin->releaseConfigs();
		pNewPlugin->releaseValues();
		pNewPlugin->setActivated(pPlugin->isActivated());
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

	if (ppBuffer == nullptr || *ppBuffer == nullptr || m_pppBuffers[1] == nullptr)
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
	qtractorPlugin *pPlugin = nullptr;

	QString sFilename;
	QString sLabel;
	QString sAlias;
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
		if (eParam.tagName() == "alias")
			sAlias = eParam.text();
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
		((pPlugin == nullptr) || (pPlugin->label() != sLabel))) {
		iIndex = 0;
		do {
			if (pPlugin) delete pPlugin;
			pPlugin = qtractorPluginFile::createPlugin(this,
				sFilename, iIndex++, typeHint);
		} while (pPlugin && (pPlugin->label() != sLabel));
	}
#endif

	if (pPlugin) {
		if (iUniqueID > 0)
			pPlugin->setUniqueID(iUniqueID);
		if (!sLabel.isEmpty())
			pPlugin->setLabel(sLabel);
		if (!sAlias.isEmpty())
			pPlugin->setAlias(sAlias);
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
		// Load audio output bus flag...
		if (ePlugin.tagName() == "audio-output-bus") {
			m_bAudioOutputBus = qtractorDocument::boolFromText(ePlugin.text());
		}
		else
		// Load audio output bus name...
		if (ePlugin.tagName() == "audio-output-bus-name") {
			m_sAudioOutputBusName = ePlugin.text();
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
		// Create the new plugin element...
		QDomElement ePlugin = pDocument->document()->createElement("plugin");
		pPlugin->savePluginEx(pDocument, &ePlugin);
		// Add this plugin...
		pElement->appendChild(ePlugin);
	}

	// Save audio output-bus connects...
	if (m_pMidiManager) {
		const bool bAudioOutputBus
			= m_pMidiManager->isAudioOutputBus();
		pDocument->saveTextElement("audio-output-bus",
			qtractorDocument::textFromBool(bAudioOutputBus), pElement);
		const QString& sAudioOutputBusName
			= m_pMidiManager->audioOutputBusName();
		if (!sAudioOutputBusName.isEmpty())
			pDocument->saveTextElement("audio-output-bus-name",
				sAudioOutputBusName, pElement);
		pDocument->saveTextElement("audio-output-auto-connect",
			qtractorDocument::textFromBool(
				m_pMidiManager->isAudioOutputAutoConnect()), pElement);
		if (bAudioOutputBus) {
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
		unsigned short iAudioOuts = 0;
		if (bDeactivated) {
			bool bStopDeactivation = false;
			// Pass to all plugins bottom to top;
			// stop for any active plugins that are
			// possibly connected to other tracks...
			qtractorPlugin *pPlugin = last();
			for ( ;	pPlugin && !bStopDeactivation; pPlugin = pPlugin->prev()) {
				if (pPlugin->canBeConnectedToOtherTracks())
					bStopDeactivation = pPlugin->isAutoActivated();
				else
					pPlugin->autoDeactivatePlugin(bDeactivated);
				iAudioOuts += pPlugin->audioOuts();
			}
			// (Re)activate all above stopper...
			for ( ; pPlugin; pPlugin = pPlugin->prev()) {
				if (bStopDeactivation)
					pPlugin->autoDeactivatePlugin(false);
				iAudioOuts += pPlugin->audioOuts();
			}
		} else {
			// Pass to all plugins top to to bottom...
			for (qtractorPlugin *pPlugin = first();
					pPlugin; pPlugin = pPlugin->next()) {
				pPlugin->autoDeactivatePlugin(bDeactivated);
				iAudioOuts += pPlugin->audioOuts();
			}
		}
		// Take the chance to turn on/off automagically
		// the audio monitors/meters, when applicable...
		qtractorMidiManager *pMidiManager = midiManager();
		if (pMidiManager)
			pMidiManager->setAudioOutputMonitorEx(iAudioOuts > 0);
		// Inform all views...
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
unsigned long qtractorPluginList::currentLatency (void) const
{
	unsigned long iLatency = 0;

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
			iLatency += pPlugin->latency();
		}
	}

	return iLatency;
}


// Plugin editors (GUI) visibility (auto-focus).
void qtractorPluginList::setEditorVisibleAll ( bool bVisible )
{
	for (qtractorPlugin *pPlugin = first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isEditorVisible())
			pPlugin->setEditorVisible(bVisible);
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
	if (pSession == nullptr)
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
	pElement->setAttribute("version", PROJECT_TITLE " " PROJECT_VERSION);

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


//-------------------------------------------------------------------------
// class qtractorPliuginList::WaitCursor - A waiting (hour-glass) helper.
//

// Constructor.
qtractorPluginList::WaitCursor::WaitCursor (void)
{
	// Tell the world we'll (maybe) take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

// Destructor.
qtractorPluginList::WaitCursor::~WaitCursor (void)
{
	// We're formerly done.
	QApplication::restoreOverrideCursor();
}


// end of qtractorPlugin.cpp
