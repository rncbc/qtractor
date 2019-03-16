// qtractorPluginForm.cpp
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
#include "qtractorPluginForm.h"
#include "qtractorPluginCommand.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"

#ifdef CONFIG_LV2_PATCH
#include "qtractorLv2Plugin.h"
#endif

#include "qtractorInsertPlugin.h"

#include "qtractorObserverWidget.h"

#include "qtractorMidiControlObserverForm.h"

#include "qtractorSpinBox.h"

#include "qtractorBusForm.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorEngine.h"

#include <QMessageBox>
#include <QTabWidget>
#include <QGridLayout>
#include <QValidator>
#include <QTextEdit>
#include <QLabel>

#include <QFileInfo>
#include <QFileDialog>
#include <QUrl>

#include <QMenu>
#include <QKeyEvent>
#include <QHideEvent>

#include "math.h"


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.

// Constructor.
qtractorPluginForm::qtractorPluginForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	const QFont& font = QWidget::font();
	QWidget::setFont(QFont(font.family(), font.pointSize() - 1));

	m_pPlugin     = NULL;
	m_iDirtyCount = 0;
	m_iUpdate     = 0;

	m_pDirectAccessParamMenu = new QMenu();
	m_ui.DirectAccessParamPushButton->setMenu(m_pDirectAccessParamMenu);

	m_ui.PresetComboBox->setValidator(
		new QRegExpValidator(QRegExp("[\\w-]+"), m_ui.PresetComboBox));
	m_ui.PresetComboBox->setInsertPolicy(QComboBox::NoInsert);
	m_ui.PresetComboBox->setCompleter(NULL);

	// Have some effective feedback when toggling on/off...
	QIcon iconActivate;
	iconActivate.addPixmap(
		QPixmap(":/images/itemLedOff.png"), QIcon::Active, QIcon::Off);
	iconActivate.addPixmap(
		QPixmap(":/images/itemLedOn.png"), QIcon::Active, QIcon::On);
	m_ui.ActivateToolButton->setIcon(iconActivate);

	// UI signal/slot connections...
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changePresetSlot(const QString&)));
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(loadPresetSlot(const QString&)));
	QObject::connect(m_ui.OpenPresetToolButton,
		SIGNAL(clicked()),
		SLOT(openPresetSlot()));
	QObject::connect(m_ui.SavePresetToolButton,
		SIGNAL(clicked()),
		SLOT(savePresetSlot()));
	QObject::connect(m_ui.DeletePresetToolButton,
		SIGNAL(clicked()),
		SLOT(deletePresetSlot()));
	QObject::connect(m_ui.EditToolButton,
		SIGNAL(toggled(bool)),
		SLOT(editSlot(bool)));
	QObject::connect(m_ui.ActivateToolButton,
		SIGNAL(toggled(bool)),
		SLOT(activateSlot(bool)));

	QObject::connect(m_ui.TabWidget,
		SIGNAL(currentChanged(int)),
		SLOT(currentChangedSlot(int)));

	QObject::connect(m_ui.SendsToolButton,
		SIGNAL(clicked()),
		SLOT(sendsSlot()));
	QObject::connect(m_ui.ReturnsToolButton,
		SIGNAL(clicked()),
		SLOT(returnsSlot()));
	QObject::connect(m_ui.AuxSendBusNameComboBox,
		SIGNAL(activated(const QString&)),
		SLOT(changeAuxSendBusNameSlot(const QString&)));
	QObject::connect(m_ui.AuxSendBusNameToolButton,
		SIGNAL(clicked()),
		SLOT(clickAuxSendBusNameSlot()));

	QObject::connect(m_pDirectAccessParamMenu,
		SIGNAL(aboutToShow()),
		SLOT(updateDirectAccessParamSlot()));
}


// Destructor.
qtractorPluginForm::~qtractorPluginForm (void)
{
	clear();

	delete m_pDirectAccessParamMenu;
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	clear();

	// Set the new reference...
	m_pPlugin = pPlugin;

	if (m_pPlugin == NULL)
		return;

	// Dispatch any pending updates.
	qtractorSubject::flushQueue(true);

	// Set activate button MIDI controller observer...
	addMidiControlAction(
		m_ui.ActivateToolButton,
		m_pPlugin->activateObserver());

	qtractorPluginType *pType = m_pPlugin->type();
	const bool bVstPlugin = (pType->typeHint() == qtractorPluginType::Vst);
	const int MaxRowsPerPage    = (bVstPlugin ? 12 : 8);
	const int MaxColumnsPerPage = (bVstPlugin ?  2 : 3);
	const int MaxItemsPerPage   = MaxRowsPerPage * MaxColumnsPerPage;

	const qtractorPlugin::Params& params = m_pPlugin->params();
	const int iParams = params.count();

	int iItems = iParams;
#ifdef CONFIG_LV2_PATCH
	qtractorLv2Plugin *pLv2Plugin = NULL;
	if (pType->typeHint() == qtractorPluginType::Lv2)
		pLv2Plugin = static_cast<qtractorLv2Plugin *> (m_pPlugin);
	if (pLv2Plugin)
		iItems += pLv2Plugin->lv2_properties().count();
#endif
	int iItemsPerPage = iItems;
	int iItemsOnLastPage = 0;
	if (iItemsPerPage > MaxItemsPerPage) {
		iItemsPerPage = MaxItemsPerPage;
		iItemsOnLastPage = (iItems % iItemsPerPage);
		while (iItemsOnLastPage > 0
			&& iItemsOnLastPage < ((3 * iItemsPerPage) >> 2))
			iItemsOnLastPage = (iItems % --iItemsPerPage);
	}

	int iPages = 1;
	int iRowsPerPage = iItemsPerPage;
	int iColumnsPerPage = 1;
	if (iRowsPerPage > MaxRowsPerPage) {
		iPages = (iItems / iItemsPerPage);
		if (iItemsOnLastPage > 0)
			++iPages;
		while (iRowsPerPage > MaxRowsPerPage
			&& iColumnsPerPage < MaxColumnsPerPage)
			iRowsPerPage = (iItemsPerPage / ++iColumnsPerPage);
		if (iItemsPerPage % iColumnsPerPage) // Adjust to balance.
			++iRowsPerPage;
	}

	// Maybe we need a tabbed widget...
	QTabWidget  *pTabWidget  = NULL;
	QGridLayout *pGridLayout = NULL;
	QWidget     *pPageWidget = NULL;

	int iPage = 0;
	const QString sPage = tr("Page %1");
	if (iItems > 0) {
		pTabWidget  = m_ui.TabWidget;
		pGridLayout = new QGridLayout();
		pGridLayout->setMargin(8);
		pGridLayout->setSpacing(4);
		pPageWidget = new QWidget();
		pPageWidget->setLayout(pGridLayout);
		pTabWidget->insertTab(iPage, pPageWidget, sPage.arg(iPage + 1));
		++iPage;
	}

	QWidgetList widgets;

#ifdef CONFIG_LV2_PATCH
	if (pLv2Plugin) {
		const qtractorLv2Plugin::Properties& props = pLv2Plugin->lv2_properties();
		qtractorLv2Plugin::Properties::ConstIterator prop = props.constBegin();
		const qtractorLv2Plugin::Properties::ConstIterator& prop_end = props.constEnd();
		for ( ; prop != prop_end; ++prop) {
			qtractorLv2Plugin::Property *pProp = prop.value();
			qtractorPluginPropertyWidget *pPropWidget
				= new qtractorPluginPropertyWidget(pLv2Plugin, pProp->key());
			m_propWidgets.append(pPropWidget);
			widgets.append(pPropWidget);
		}
	}
#endif

	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		qtractorPluginParamWidget *pParamWidget
			= new qtractorPluginParamWidget(pParam, this);
		qtractorMidiControlObserver *pMidiObserver = pParam->observer();
		if (pMidiObserver)
			addMidiControlAction(pParamWidget, pMidiObserver);
		m_paramWidgets.append(pParamWidget);
		widgets.append(pParamWidget);
	}

	// FIXME: Couldn't stand more than a hundred widgets?
	// or do we have one dedicated editor GUI?
	int iRow = 0;
	int iColumn = 0;

	QListIterator<QWidget *> iter(widgets);
	while (iter.hasNext()) {
		pGridLayout->addWidget(iter.next(), iRow, iColumn);
		if (++iRow >= iRowsPerPage) {
			iRow = 0;
			if (++iColumn >= iColumnsPerPage) {
				iColumn = 0;
				if (pTabWidget && iPage < iPages) {
					pGridLayout = new QGridLayout();
					pGridLayout->setMargin(8);
					pGridLayout->setSpacing(4);
					pPageWidget = new QWidget();
					pPageWidget->setLayout(pGridLayout);
					pTabWidget->insertTab(iPage, pPageWidget, sPage.arg(iPage + 1));
					++iPage;
				}
			}
		}
	}

	// Show editor button if available?
	const bool bEditor = pType->isEditor();
	m_ui.EditToolButton->setVisible(bEditor);
	if (bEditor)
		toggleEditor(m_pPlugin->isEditorVisible());

	// Show insert tool options...
	const bool bInsertPlugin = (pType->typeHint() == qtractorPluginType::Insert);
	if (bInsertPlugin) {
		if (pType->index() > 0) {
			m_ui.SendsToolButton->setIcon(QIcon(":/images/itemAudioPortOut.png"));
			m_ui.ReturnsToolButton->setIcon(QIcon(":/images/itemAudioPortIn.png"));
		} else {
			m_ui.SendsToolButton->setIcon(QIcon(":/images/itemMidiPortOut.png"));
			m_ui.ReturnsToolButton->setIcon(QIcon(":/images/itemMidiPortIn.png"));
		}
	}
	m_ui.SendsToolButton->setVisible(bInsertPlugin);
	m_ui.ReturnsToolButton->setVisible(bInsertPlugin);

	// Show aux-send tool options...
	const bool bAuxSendPlugin = (pType->typeHint() == qtractorPluginType::AuxSend);
	m_ui.AuxSendBusNameComboBox->setVisible(bAuxSendPlugin);
	m_ui.AuxSendBusNameLabel->setVisible(bAuxSendPlugin);
	m_ui.AuxSendBusNameToolButton->setVisible(bAuxSendPlugin);

	// Set initial plugin preset name...
	setPreset(m_pPlugin->preset());

	// Set plugin name as title,
	// maybe redundant but necessary...
	m_pPlugin->updateEditorTitle();

	// About page...
	m_ui.NameTextLabel->setText(pType->name());
	m_ui.TypeHintTextLabel->setText(
		qtractorPluginType::textFromHint(pType->typeHint()));
	QString sAboutText = pType->aboutText();
	sAboutText += '\n';
	sAboutText += '\n';
	sAboutText += tr("%1 [%2], %3 instance(s), %4 channel(s).")
		.arg(pType->filename())
		.arg(pType->index())
		.arg(m_pPlugin->instances())
		.arg(m_pPlugin->channels());
	m_ui.AboutTextLabel->setText(sAboutText);
	updateLatencyTextLabel();

	// This should trigger paramsSlot(!bEditor)
	// and adjust the size of the params dialog...
	m_ui.DirectAccessParamPushButton->setVisible(iParams > 0);

	// Always first tab/page selected...
	m_ui.TabWidget->setCurrentIndex(0);

	// Clear any initial param update.
	qtractorSubject::resetQueue();

	updateActivated();
	refresh();
}


// Plugin accessor.
qtractorPlugin *qtractorPluginForm::plugin (void) const
{
	return m_pPlugin;
}


// Plugin preset accessors.
void qtractorPluginForm::setPreset ( const QString& sPreset )
{
	QString sEditText = sPreset;

	if (sEditText.isEmpty())
		sEditText = qtractorPlugin::defPreset();

	++m_iUpdate;
	const bool bBlockSignals
		= m_ui.PresetComboBox->blockSignals(true);

	const int iIndex = m_ui.PresetComboBox->findText(sEditText);
	if (iIndex >= 0)
		m_ui.PresetComboBox->setCurrentIndex(iIndex);
	else
		m_ui.PresetComboBox->setEditText(sEditText);

	m_ui.PresetComboBox->blockSignals(bBlockSignals);
	--m_iUpdate;
}

QString qtractorPluginForm::preset (void) const
{
	QString sPreset = m_ui.PresetComboBox->currentText();

	if (sPreset == qtractorPlugin::defPreset() || m_iDirtyCount > 0)
		sPreset.clear();

	return sPreset;
}


// Update activation state.
void qtractorPluginForm::updateActivated (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	++m_iUpdate;
	m_ui.ActivateToolButton->setChecked(m_pPlugin->isActivated());
	--m_iUpdate;
}


// Update port widget state.
void qtractorPluginForm::updateDirtyCount (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::updateDirtyCount()", this);
#endif

	// Sure is dirty...
	++m_iDirtyCount;
	stabilize();
}


// Update specific aux-send bus name settings.
void qtractorPluginForm::updateAuxSendBusName (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (m_pPlugin == NULL)
		return;

	m_ui.AuxSendBusNameComboBox->clear();

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType->typeHint() != qtractorPluginType::AuxSend)
		return;

	QString sAuxSendBusName;

	if (pType->index() > 0) {
		qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
			= static_cast<qtractorAudioAuxSendPlugin *> (m_pPlugin);
		if (pAudioAuxSendPlugin == NULL)
			return;
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine == NULL)
			return;
		const QIcon iconAudio(":/images/trackAudio.png");
		m_ui.AuxSendBusNameComboBox->addItem(iconAudio, tr("(none)"));
		for (qtractorBus *pBus = pAudioEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				qtractorAudioBus *pAudioBus
					= static_cast<qtractorAudioBus *> (pBus);
				if (pAudioBus && pAudioBus->channels() == m_pPlugin->channels())
					m_ui.AuxSendBusNameComboBox->addItem(iconAudio,
						pAudioBus->busName());
			}
		}
		sAuxSendBusName = pAudioAuxSendPlugin->audioBusName();
	} else {
		qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
			= static_cast<qtractorMidiAuxSendPlugin *> (m_pPlugin);
		if (pMidiAuxSendPlugin == NULL)
			return;
		qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
		if (pMidiEngine == NULL)
			return;
		const QIcon iconMidi(":/images/trackMidi.png");
		m_ui.AuxSendBusNameComboBox->addItem(iconMidi, tr("(none)"));
		for (qtractorBus *pBus = pMidiEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				qtractorMidiBus *pMidiBus
					= static_cast<qtractorMidiBus *> (pBus);
				if (pMidiBus)
					m_ui.AuxSendBusNameComboBox->addItem(iconMidi,
						pMidiBus->busName());
			}
		}
		sAuxSendBusName = pMidiAuxSendPlugin->midiBusName();
	}

	int iIndex = m_ui.AuxSendBusNameComboBox->findText(sAuxSendBusName);
	if (iIndex < 0)
		iIndex = 0;
	m_ui.AuxSendBusNameComboBox->setCurrentIndex(iIndex);
}


// Editor widget methods.
void qtractorPluginForm::toggleEditor ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	++m_iUpdate;
	m_ui.EditToolButton->setChecked(bOn);
	--m_iUpdate;
}


// Preset management slots...
void qtractorPluginForm::changePresetSlot ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty() && m_ui.PresetComboBox->findText(sPreset) >= 0)
		++m_iDirtyCount;

	stabilize();
}


void qtractorPluginForm::loadPresetSlot ( const QString& sPreset )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0 || sPreset.isEmpty())
		return;

	m_pPlugin->loadPresetEx(sPreset);

	stabilize();
}


void qtractorPluginForm::openPresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	const bool bVstPlugin = (pType->typeHint() == qtractorPluginType::Vst);
	if (!pType->isConfigure() && !bVstPlugin)
		return;

	if (m_iUpdate > 0)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// We'll assume that there's an external file...
	QString sFilename;

	// Prompt if file does not currently exist...
	const QString sExt("qtx");
	QStringList exts(sExt);
	if (bVstPlugin) {
		exts.append("fxp");
		exts.append("fxb");
	}

	const QString& sTitle
		= tr("Open Preset") + " - " QTRACTOR_TITLE;

	QStringList filters;
	filters.append(tr("Preset files (*.%1)").arg(exts.join(" *.")));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = NULL;
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to save...
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, pOptions->sPresetDir, sFilter, NULL, options);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, pOptions->sPresetDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sPresetDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif
	// Have we a filename to load a preset from?
	if (!sFilename.isEmpty()) {
		if (m_pPlugin->loadPresetFile(sFilename)) {
			// Got it loaded alright...
			const QFileInfo fi(sFilename);
			setPreset(fi.baseName().replace(pType->label() + '-', QString()));
			pOptions->sPresetDir = fi.absolutePath();
		} else {
			// Failure (maybe wrong plugin)...
			QMessageBox::critical(this,
				tr("Error") + " - " QTRACTOR_TITLE,
				tr("Preset could not be loaded from file:\n\n"
				"\"%1\".\n\n"
				"Sorry.").arg(sFilename),
				QMessageBox::Cancel);
		}
	}

	refresh();
}


void qtractorPluginForm::savePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset = m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == qtractorPlugin::defPreset())
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// The current state preset is about to be saved...
	// this is where we'll make it!
	if (m_pPlugin->savePreset(sPreset)) {
		refresh();
		return;
	}

	QSettings& settings = pOptions->settings();
	settings.beginGroup(m_pPlugin->presetGroup());
	// Which mode of preset?
	qtractorPluginType *pType = m_pPlugin->type();
	const bool bVstPlugin = (pType->typeHint() == qtractorPluginType::Vst);
	if (pType->isConfigure() || bVstPlugin) {
		// Sure, we'll have something complex enough
		// to make it save into an external file...
		const QString sExt("qtx");
		QStringList exts(sExt);
		if (bVstPlugin) { exts.append("fxp"); exts.append("fxb"); }
		QFileInfo fi(settings.value(sPreset).toString());
		if (!fi.exists() || !fi.isFile())
			fi.setFile(QDir(pOptions->sPresetDir),
				pType->label() + '-' + sPreset + '.' + sExt);
		QString sFilename = fi.absoluteFilePath();
		// Prompt if file does not currently exist...
		if (!fi.exists()) {
			const QString& sTitle
				= tr("Save Preset") + " - " QTRACTOR_TITLE;
			QStringList filters;
			filters.append(tr("Preset files (*.%1)").arg(exts.join(" *.")));
			filters.append(tr("All files (*.*)"));
			const QString& sFilter = filters.join(";;");
			QWidget *pParentWidget = NULL;
			QFileDialog::Options options = 0;
			if (pOptions->bDontUseNativeDialogs) {
				options |= QFileDialog::DontUseNativeDialog;
				pParentWidget = QWidget::window();
			}
		#if 1//QT_VERSION < 0x040400
			// Ask for the filename to save...
			sFilename = QFileDialog::getSaveFileName(pParentWidget,
				sTitle, sFilename, sFilter, NULL, options);
		#else
			// Construct save-file dialog...
			QFileDialog fileDialog(pParentWidget,
				sTitle, sFilename, sFilter);
			// Set proper open-file modes...
			fileDialog.setAcceptMode(QFileDialog::AcceptSave);
			fileDialog.setFileMode(QFileDialog::AnyFile);
			fileDialog.setDefaultSuffix(sExt);
			// Stuff sidebar...
			QList<QUrl> urls(fileDialog.sidebarUrls());
			urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
			urls.append(QUrl::fromLocalFile(pOptions->sPresetDir));
			fileDialog.setSidebarUrls(urls);
			fileDialog.setOptions(options);
			// Show dialog...
			if (fileDialog.exec())
				sFilename = fileDialog.selectedFiles().first();
			else
				sFilename.clear();
		#endif
		}
		// We've a filename to save the preset...
		if (!sFilename.isEmpty() && sFilename.at(0) != '.') {
			fi.setFile(sFilename);
			if (fi.suffix().isEmpty())
				sFilename += '.' + sExt;
			if (m_pPlugin->savePresetFile(sFilename)) {
				settings.setValue(sPreset, sFilename);
				pOptions->sPresetDir = fi.absolutePath();
			} else {
				// Failure (maybe wrong suffix)...
				QMessageBox::critical(this,
					tr("Error") + " - " QTRACTOR_TITLE,
					tr("Preset could not be saved to file:\n\n"
					"\"%1\".\n\n"
					"Sorry.").arg(sFilename),
					QMessageBox::Cancel);
			}
		}
	}	// Just leave it to simple parameter value list...
	else settings.setValue(sPreset, m_pPlugin->valueList());
	settings.endGroup();

	refresh();
}


void qtractorPluginForm::deletePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset =  m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == qtractorPlugin::defPreset())
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// A preset entry is about to be deleted;
	// prompt user if he/she's sure about this...
	if (pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to delete preset:\n\n"
			"\"%1\" (%2)\n\n"
			"Are you sure?")
			.arg(sPreset)
			.arg((m_pPlugin->type())->name()),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel)
			return;
	}

	// Go ahead...
	if (!m_pPlugin->deletePreset(sPreset)) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup(m_pPlugin->presetGroup());
	#ifdef QTRACTOR_REMOVE_PRESET_FILES
		if ((m_pPlugin->type())->isConfigure()) {
			const QString& sFilename = settings.value(sPreset).toString();
			if (QFileInfo(sFilename).exists())
				QFile(sFilename).remove();
		}
	#endif
		settings.remove(sPreset);
		settings.endGroup();
	}

	refresh();
}


// Editor slot.
void qtractorPluginForm::editSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	++m_iUpdate;

	if (bOn)
		m_pPlugin->openEditor();
	else
		m_pPlugin->closeEditor();

	--m_iUpdate;
}


// Activation slot.
void qtractorPluginForm::activateSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	++m_iUpdate;

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(
			new qtractorActivatePluginCommand(m_pPlugin, bOn));

	--m_iUpdate;
}


// Tab page change slot.
void qtractorPluginForm::currentChangedSlot ( int iTab )
{
	// Make sure we're in the "About" page...
	if (iTab < m_ui.TabWidget->count() - 1)
		return;

	updateLatencyTextLabel();
}


// Outputs (Sends) slot.
void qtractorPluginForm::sendsSlot (void)
{
	qtractorPluginListView::insertPluginBus(m_pPlugin, qtractorBus::Output);
}


// Inputs (Returns) slot.
void qtractorPluginForm::returnsSlot (void)
{
	qtractorPluginListView::insertPluginBus(m_pPlugin, qtractorBus::Input);
}


// Audio bus name (aux-send) select slot.
void qtractorPluginForm::changeAuxSendBusNameSlot ( const QString& sAuxSendBusName )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (m_pPlugin == NULL)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType->typeHint() != qtractorPluginType::AuxSend)
		return;

	pSession->execute(
		new qtractorAuxSendPluginCommand(m_pPlugin, sAuxSendBusName));
}


// Audio bus name (aux-send) browse slot.
void qtractorPluginForm::clickAuxSendBusNameSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (m_pPlugin == NULL)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType->typeHint() != qtractorPluginType::AuxSend)
		return;

	qtractorEngine *pEngine = NULL;
	if (pType->index() > 0)
		pEngine = pSession->audioEngine();
	else
		pEngine = pSession->midiEngine();
	if (pEngine == NULL)
		return;

	// Call here the bus management form.
	qtractorBusForm busForm(this);
	// Pre-select bus...
	const QString& sAuxSendBusName
		= m_ui.AuxSendBusNameComboBox->currentText();
	if (!sAuxSendBusName.isEmpty())
		busForm.setBus(pEngine->findBus(sAuxSendBusName));
	// Go for it...
	busForm.exec();

	// Check if any buses have changed...
	if (busForm.isDirty())
		updateAuxSendBusName();
}


// Direct access parameter slots
void qtractorPluginForm::updateDirectAccessParamSlot (void)
{
	m_pDirectAccessParamMenu->clear();

	if (m_pPlugin == NULL)
		return;

	QAction *pAction;
	const int iDirectAccessParamIndex = m_pPlugin->directAccessParamIndex();
	const qtractorPlugin::Params& params = m_pPlugin->params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		const int iParamIndex = int(param.key());
		pAction = m_pDirectAccessParamMenu->addAction(
			pParam->name(), this, SLOT(changeDirectAccessParamSlot()));
		pAction->setCheckable(true);
		pAction->setChecked(iDirectAccessParamIndex == iParamIndex);
		pAction->setData(iParamIndex);
	}

	if (!params.isEmpty())
		m_pDirectAccessParamMenu->addSeparator();
	pAction = m_pDirectAccessParamMenu->addAction(
		tr("&None"), this, SLOT(changeDirectAccessParamSlot()));
	pAction->setCheckable(true);
	pAction->setChecked(iDirectAccessParamIndex < 0);
	pAction->setData(int(-1));
}


void qtractorPluginForm::changeDirectAccessParamSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	// Retrieve direct access parameter index from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction == NULL)
		return;

	const int iDirectAccessParamIndex = pAction->data().toInt();

	++m_iUpdate;

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(
			new qtractorDirectAccessParamCommand(m_pPlugin, iDirectAccessParamIndex));

	--m_iUpdate;
}


// Parameter-widget refreshner-loader.
void qtractorPluginForm::refresh (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::refresh()", this);
#endif

	++m_iUpdate;
	const bool bBlockSignals
		= m_ui.PresetComboBox->blockSignals(true);

	qtractorSubject::flushQueue(true);

	const QString sOldPreset = m_ui.PresetComboBox->currentText();
	m_ui.PresetComboBox->clear();
	m_ui.PresetComboBox->insertItems(0, m_pPlugin->presetList());
	m_ui.PresetComboBox->model()->sort(0);
	m_ui.PresetComboBox->addItem(qtractorPlugin::defPreset());
	const int iIndex = m_ui.PresetComboBox->findText(sOldPreset);
	if (iIndex >= 0)
		m_ui.PresetComboBox->setCurrentIndex(iIndex);
	else
		m_ui.PresetComboBox->setEditText(sOldPreset);

//	m_pPlugin->idleEditor();

	QListIterator<qtractorPluginPropertyWidget *> prop_iter(m_propWidgets);
	while (prop_iter.hasNext())
		prop_iter.next()->refresh();

	QListIterator<qtractorPluginParamWidget *> param_iter(m_paramWidgets);
	while (param_iter.hasNext())
		param_iter.next()->refresh();

	updateAuxSendBusName();

	qtractorSubject::resetQueue();

	updateLatencyTextLabel();

	m_iDirtyCount = 0;
	m_ui.PresetComboBox->blockSignals(bBlockSignals);
	--m_iUpdate;

	stabilize();
}


// Preset control.
void qtractorPluginForm::stabilize (void)
{
	qtractorPluginType *pType = m_pPlugin->type();
	const bool bVstPlugin = (pType->typeHint() == qtractorPluginType::Vst);

	bool bExists  = false;
	bool bEnabled = (m_pPlugin != NULL);
	m_ui.ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled)
		bEnabled = (pType->controlIns() > 0	|| pType->isConfigure());

	m_ui.PresetComboBox->setEnabled(bEnabled);
	m_ui.OpenPresetToolButton->setVisible(
		bEnabled && (pType->isConfigure() || bVstPlugin));

	if (bEnabled) {
		const QString& sPreset = m_ui.PresetComboBox->currentText();
		bEnabled = !sPreset.isEmpty()
			&& sPreset != qtractorPlugin::defPreset()
			&& !m_pPlugin->isReadOnlyPreset(sPreset);
		bExists	= (m_ui.PresetComboBox->findText(sPreset) >= 0);
	}

	m_ui.SavePresetToolButton->setEnabled(
		bEnabled && (!bExists || m_iDirtyCount > 0));
	m_ui.DeletePresetToolButton->setEnabled(
		bEnabled && bExists);
}


// Clear up plugin form...
void qtractorPluginForm::clear (void)
{
	if (m_pPlugin)
		m_pPlugin->closeEditor();

	qDeleteAll(m_paramWidgets);
	m_paramWidgets.clear();

	qDeleteAll(m_propWidgets);
	m_propWidgets.clear();

	m_pDirectAccessParamMenu->clear();	
}


// Keyboard event handler.
void qtractorPluginForm::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm::keyPressEvent(%d)", pKeyEvent->key());
#endif
	const int iKey = pKeyEvent->key();
	switch (iKey) {
	case Qt::Key_Escape:
		close();
		break;
	default:
		QWidget::keyPressEvent(pKeyEvent);
		break;
	}
}


// Form show event (refresh).
void qtractorPluginForm::showEvent ( QShowEvent *pShowEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::showEvent()", this);
#endif

	QWidget::showEvent(pShowEvent);

	// Make sure all plugin-data is up-to-date...
	refresh();
}


// Form hide event (save visible position).
void qtractorPluginForm::hideEvent ( QHideEvent *pHideEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::hideEvent()", this);
#endif

	// Save current form position...
	if (m_pPlugin)
		m_pPlugin->setFormPos(QWidget::pos());

	QWidget::hideEvent(pHideEvent);
}


// MIDI controller/observer attachment (context menu)
void qtractorPluginForm::addMidiControlAction (
	QWidget *pWidget, qtractorMidiControlObserver *pMidiObserver )
{
	qtractorMidiControlObserverForm::addMidiControlAction(
		this, pWidget, pMidiObserver);
}


void qtractorPluginForm::midiControlActionSlot (void)
{
	qtractorMidiControlObserverForm::midiControlAction(
		this, qobject_cast<QAction *> (sender()));
}


void qtractorPluginForm::midiControlMenuSlot ( const QPoint& pos )
{
	qtractorMidiControlObserverForm::midiControlMenu(
		qobject_cast<QWidget *> (sender()), pos);
}


// Update the about text label (with some varying meta-data)...
void qtractorPluginForm::updateLatencyTextLabel (void)
{
	if (m_pPlugin == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const unsigned long iLatency = m_pPlugin->latency();
	if (iLatency > 0) {
		const float fLatencyMs
			= 1000.0f * float(iLatency) / float(pSession->sampleRate());
		m_ui.LatencyTextLabel->setText(
			tr("Latency: %1 ms (%2 frames)")
				.arg(QString::number(fLatencyMs, 'f', 1))
				.arg(iLatency));
	} else {
		m_ui.LatencyTextLabel->setText(tr("(no latency)"));
	}
}


//----------------------------------------------------------------------
// class qtractorPluginParamDisplay -- Observer display label.
//

class qtractorPluginParamDisplay : public QLabel
{
public:

	// Local observer.
	class Observer : public qtractorObserver
	{
	public:

		// Constructor.
		Observer(qtractorSubject *pSubject, qtractorPluginParamDisplay *pDisplay)
			: qtractorObserver(pSubject), m_pDisplay(pDisplay) {}

		// Observer updater.
		void update(bool bUpdate)
			{ if (bUpdate) m_pDisplay->updateDisplay(); }

	private:

		// Members.
		qtractorPluginParamDisplay *m_pDisplay;
	};

	// Constructor.
	qtractorPluginParamDisplay(qtractorPluginParam *pParam)
		: QLabel(), m_pParam(pParam), m_observer(pParam->subject(), this) {}

	// Observer accessor.
	Observer *observer() { return &m_observer; }

protected:

	void updateDisplay() { QLabel::setText(m_pParam->display()); }

private:

	// Parameter reference.
	qtractorPluginParam *m_pParam;

	// Observer instance.
	Observer m_observer;
};


//----------------------------------------------------------------------
// class qtractorPluginParamWidget::SliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorPluginParamWidget::SliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Constructor.
	SliderInterface ( qtractorPluginParam *pParam )
		: m_pParam(pParam) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 10000.0f * m_pParam->observer()->scaleFromValue(fValue,
			m_pParam->isLogarithmic()); }

	float valueFromScale ( float fScale ) const
		{ return m_pParam->observer()->valueFromScale(0.0001f * fScale,
			m_pParam->isLogarithmic()); }

private:

	// Instance references.
	qtractorPluginParam *m_pParam;
};


//----------------------------------------------------------------------------
// qtractorPluginParamWidget -- Plugin port widget.
//

// Constructor.
qtractorPluginParamWidget::qtractorPluginParamWidget (
	qtractorPluginParam *pParam, QWidget *pParent )
	: QWidget(pParent), m_pParam(pParam)
{
	m_pSlider   = NULL;
	m_pSpinBox  = NULL;
	m_pCheckBox = NULL;
	m_pDisplay  = NULL;

	QGridLayout *pGridLayout = new QGridLayout();
	pGridLayout->setMargin(0);
	pGridLayout->setSpacing(4);

	if (m_pParam->isToggled()) {
		m_pCheckBox = new qtractorObserverCheckBox(/*this*/);
		m_pCheckBox->setText(m_pParam->name());
		m_pCheckBox->setSubject(m_pParam->subject());
	//	m_pCheckBox->setChecked(m_pParam->value() > 0.1f);
		pGridLayout->addWidget(m_pCheckBox, 0, 0);
	} else if (m_pParam->isInteger()) {
		pGridLayout->setColumnMinimumWidth(0, 120);
		QLabel *pLabel = new QLabel(/*this*/);
		pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		pLabel->setText(m_pParam->name() + ':');
		pGridLayout->addWidget(pLabel, 0, 0);
		m_pSpinBox = new qtractorObserverSpinBox(/*this*/);
		m_pSpinBox->setMaximumWidth(64);
		m_pSpinBox->setDecimals(0);
		m_pSpinBox->setMinimum(m_pParam->minValue());
		m_pSpinBox->setMaximum(m_pParam->maxValue());
		m_pSpinBox->setAlignment(Qt::AlignHCenter);
		m_pSpinBox->setSubject(m_pParam->subject());
	//	m_pSpinBox->setValue(int(m_pParam->value()));
		pGridLayout->addWidget(m_pSpinBox, 0, 1);
		m_pDisplay = new qtractorPluginParamDisplay(m_pParam);
		m_pDisplay->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	//	m_pDisplay->setText(m_pParam->display());
	//	m_pDisplay->setFixedWidth(72);
		m_pDisplay->setMinimumWidth(64);
		pGridLayout->addWidget(m_pDisplay, 0, 2);
	} else {
		QLabel *pLabel = new QLabel(/*this*/);
		pLabel->setText(m_pParam->name() + ':');
		if (m_pParam->isDisplay()) {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		//	pLabel->setFixedWidth(72);
			pLabel->setMinimumWidth(64);
			pGridLayout->addWidget(pLabel, 0, 0);
		} else {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			pGridLayout->addWidget(pLabel, 0, 0, 1, 3);
		}
		m_pSlider = new qtractorObserverSlider(/*this*/);
		m_pSlider->setInterface(new SliderInterface(m_pParam));
		m_pSlider->setOrientation(Qt::Horizontal);
		m_pSlider->setTickPosition(QSlider::NoTicks);
		m_pSlider->setMinimumWidth(120);
		m_pSlider->setMinimum(0);
		m_pSlider->setMaximum(10000);
		m_pSlider->setPageStep(1000);
		m_pSlider->setSingleStep(100);
		m_pSlider->setSubject(m_pParam->subject());
	//	m_pSlider->setValue(m_pSlider->scaleFromValue(m_pParam->value()));
		if (m_pParam->isDisplay()) {
			pGridLayout->addWidget(m_pSlider, 0, 1);
			m_pDisplay = new qtractorPluginParamDisplay(m_pParam);
			m_pDisplay->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
		//	m_pDisplay->setText(m_pParam->display());
		//	m_pDisplay->setFixedWidth(72);
			m_pDisplay->setMinimumWidth(64);
			pGridLayout->addWidget(m_pDisplay, 0, 2);
		} else {
			pGridLayout->addWidget(m_pSlider, 1, 0, 1, 2);
			const int iDecimals = m_pParam->decimals();
			m_pSpinBox = new qtractorObserverSpinBox(/*this*/);
			m_pSpinBox->setDecimals(iDecimals);
			m_pSpinBox->setMinimum(m_pParam->minValue());
			m_pSpinBox->setMaximum(m_pParam->maxValue());
			m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecimals)));
			m_pSpinBox->setAccelerated(true);
			m_pSpinBox->setSubject(m_pParam->subject());
		//	m_pSpinBox->setValue(m_pParam->value());
			m_pSpinBox->setMaximumWidth(64);
			pGridLayout->addWidget(m_pSpinBox, 1, 2);
		}
	}

	if (m_pCheckBox) {
		QObject::connect(m_pCheckBox,
			SIGNAL(valueChanged(float)),
			SLOT(updateValue(float)));
	}

	if (m_pSpinBox) {
		QObject::connect(m_pSpinBox,
			SIGNAL(valueChanged(float)),
			SLOT(updateValue(float)));
	}

	if (m_pSlider) {
		QObject::connect(m_pSlider,
			SIGNAL(valueChanged(float)),
			SLOT(updateValue(float)));
	}

	QWidget::setLayout(pGridLayout);
	QWidget::setToolTip(m_pParam->name());
}


// Refreshner-loader method.
void qtractorPluginParamWidget::refresh (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginParamWidget[%p]::refresh()", this);
#endif

	if (m_pCheckBox)
		m_pCheckBox->observer()->update(true);
	if (m_pSpinBox)
		m_pSpinBox->observer()->update(true);
	if (m_pSlider)
		m_pSlider->observer()->update(true);
	if (m_pDisplay)
		m_pDisplay->observer()->update(true);
}


// Parameter value change slot.
void qtractorPluginParamWidget::updateValue ( float fValue )
{
	m_pParam->updateValue(fValue, true);
}


//----------------------------------------------------------------------------
// qtractorPluginPropertyWidget -- Plugin property widget.
//

// Constructor.
qtractorPluginPropertyWidget::qtractorPluginPropertyWidget (
	qtractorPlugin *pPlugin, unsigned long iProperty, QWidget *pParent )
	: QWidget(pParent), m_pPlugin(pPlugin), m_iProperty(iProperty)
{
	m_pCheckBox   = NULL;
	m_pSpinBox    = NULL;
	m_pTextEdit   = NULL;
	m_pComboBox   = NULL;
	m_pToolButton = NULL;

	QGridLayout *pGridLayout = new QGridLayout();
	pGridLayout->setMargin(0);
	pGridLayout->setSpacing(4);

#ifdef CONFIG_LV2_PATCH
	qtractorPluginType *pType = m_pPlugin->type();
	qtractorLv2Plugin *pLv2Plugin = NULL;
	qtractorLv2Plugin::Property *pLv2Prop = NULL;
	if (pType && pType->typeHint() == qtractorPluginType::Lv2)
		pLv2Plugin = static_cast<qtractorLv2Plugin *> (m_pPlugin);
	if (pLv2Plugin) {
		const LV2_URID key = m_iProperty;
		const char *pszKey = qtractorLv2Plugin::lv2_urid_unmap(key);
		if (pszKey)
			pLv2Prop = pLv2Plugin->lv2_properties().value(pszKey, NULL);
	}
#endif

#ifdef CONFIG_LV2_PATCH
	if (pLv2Prop) {
	//	pGridLayout->setColumnMinimumWidth(0, 120);
		pGridLayout->setColumnMinimumWidth(2, 32);
		if (pLv2Prop->isToggled()) {
			m_pCheckBox = new QCheckBox(/*this*/);
		//	m_pCheckBox->setMinimumWidth(120);
			m_pCheckBox->setText(pLv2Prop->name());
		//	m_pCheckBox->setChecked(pLv2Prop->value().toBool());
			pGridLayout->addWidget(m_pCheckBox, 0, 0, 1, 3);
		} else {
			QLabel *pLabel = new QLabel(/*this*/);
			pLabel->setText(pLv2Prop->name() + ':');
			if (pLv2Prop->isString()) {
				pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			//	pLabel->setMinimumWidth(120);
				pGridLayout->addWidget(pLabel, 0, 0, 1, 3);
				m_pTextEdit = new QTextEdit(/*this*/);
				m_pTextEdit->setTabChangesFocus(true);
				m_pTextEdit->setMinimumWidth(120);
				m_pTextEdit->setMaximumHeight(60);
				m_pTextEdit->installEventFilter(this);
			//	m_pTextEdit->setPlainText(pLv2Prop->value().toString());
				pGridLayout->addWidget(m_pTextEdit, 1, 0, 2, 3);
			}
			else
			if (pLv2Prop->isPath()) {
				pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			//	pLabel->setMinimumWidth(120);
				pGridLayout->addWidget(pLabel, 0, 0, 1, 3);
				m_pComboBox = new QComboBox(/*this*/);
				m_pComboBox->setEditable(false);
				m_pComboBox->setMinimumWidth(120);
			//	m_pComboBox->addItem(pLv2Prop->value().toString());
				pGridLayout->addWidget(m_pComboBox, 1, 0, 1, 2);
				m_pToolButton = new QToolButton(/*this*/);
				m_pToolButton->setIcon(QIcon(":/images/fileOpen.png"));
				pGridLayout->addWidget(m_pToolButton, 1, 2);
			} else {
				pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
			//	pLabel->setMinimumWidth(120);
				pGridLayout->addWidget(pLabel, 0, 0);
				const bool bIsInteger = pLv2Prop->isInteger();
				m_pSpinBox = new qtractorSpinBox(/*this*/);
				m_pSpinBox->setMinimumWidth(64);
				m_pSpinBox->setMaximumWidth(96);
				m_pSpinBox->setDecimals(bIsInteger ? 0 : 3);
				m_pSpinBox->setMinimum(pLv2Prop->minValue());
				m_pSpinBox->setMaximum(pLv2Prop->maxValue());
				m_pSpinBox->setAlignment(Qt::AlignRight);
				m_pSpinBox->setSingleStep(bIsInteger ? 1.0f : 0.001f);
				m_pSpinBox->setAccelerated(true);
			//	m_pSpinBox->setValue(pLv2Prop->value().toDouble());
				pGridLayout->addWidget(m_pSpinBox, 0, 1);
			}
		}
	}
#endif	// CONFIG_LV2_PATCH

	if (m_pCheckBox) {
		QObject::connect(m_pCheckBox,
			SIGNAL(toggled(bool)),
			SLOT(propertyChanged()));
	}

	if (m_pSpinBox) {
		QObject::connect(m_pSpinBox,
			SIGNAL(valueChangedEx(double)),
			SLOT(propertyChanged()));
	}

	if (m_pComboBox) {
		QObject::connect(m_pComboBox,
			SIGNAL(activated(int)),
			SLOT(propertyChanged()));
	}

	if (m_pToolButton) {
		QObject::connect(m_pToolButton,
			SIGNAL(clicked()),
			SLOT(buttonClicked()));
	}

	QWidget::setLayout(pGridLayout);

#ifdef CONFIG_LV2_PATCH
	if (pLv2Prop) QWidget::setToolTip(pLv2Prop->name());
#endif
}


// Refreshner-loader method.
void qtractorPluginPropertyWidget::refresh (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginPropertyWidget[%p]::refresh()", this);
#endif

#ifdef CONFIG_LV2_PATCH
	qtractorPluginType *pType = m_pPlugin->type();
	qtractorLv2Plugin *pLv2Plugin = NULL;
	if (pType && pType->typeHint() == qtractorPluginType::Lv2)
		pLv2Plugin = static_cast<qtractorLv2Plugin *> (m_pPlugin);
	if (pLv2Plugin) {
		qtractorLv2Plugin::Property *pLv2Prop = NULL;
		const LV2_URID key = m_iProperty;
		const char *pszKey = qtractorLv2Plugin::lv2_urid_unmap(key);
		if (pszKey)
			pLv2Prop = pLv2Plugin->lv2_properties().value(pszKey, NULL);
		if (pLv2Prop) {
			if (m_pCheckBox) {
				const bool bCheckBox = m_pCheckBox->blockSignals(true);
				m_pCheckBox->setChecked(pLv2Prop->value().toBool());
				m_pCheckBox->blockSignals(bCheckBox);
			}
			if (m_pSpinBox) {
				const bool bSpinBox = m_pSpinBox->blockSignals(true);
				m_pSpinBox->setValue(pLv2Prop->value().toDouble());
				m_pSpinBox->blockSignals(bSpinBox);
			}
			if (m_pTextEdit) {
				const bool bTextEdit = m_pTextEdit->blockSignals(true);
				m_pTextEdit->setPlainText(pLv2Prop->value().toString());
				m_pTextEdit->document()->setModified(false);
				m_pTextEdit->blockSignals(bTextEdit);
			}
			if (m_pComboBox) {
				const bool bComboBox = m_pComboBox->blockSignals(true);
				const QFileInfo fi(pLv2Prop->value().toString());
				const QString& sPath = fi.canonicalFilePath();
				int iIndex = m_pComboBox->findData(sPath);
				if (iIndex < 0) {
					m_pComboBox->insertItem(0, fi.fileName(), sPath);
					iIndex = 0;
				}
				m_pComboBox->setCurrentIndex(iIndex);
				m_pComboBox->setToolTip(sPath);
				m_pComboBox->blockSignals(bComboBox);
			}
		}
	}
#endif
}


// Property file selector.
void qtractorPluginPropertyWidget::buttonClicked (void)
{
	// Sure we have this...
	if (m_pComboBox == NULL)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// Ask for the filename to open...
	const int iIndex = m_pComboBox->currentIndex();
	if (iIndex < 0)
		return;

	QString sFilename = m_pComboBox->itemData(iIndex).toString();

	const QString& sTitle
		= tr("Open File") + " - " QTRACTOR_TITLE;

	QWidget *pParentWidget = NULL;
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < 0x040400
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, sFilename, QString(), NULL, options);
#else
	// Construct open-file dialog...
	QFileDialog fileDialog(pParentWidget, sTitle, sFilename);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif
	if (!sFilename.isEmpty()) {
		const QFileInfo fi(sFilename);
		const QString& sPath = fi.canonicalFilePath();
		int iIndex = m_pComboBox->findData(sPath);
		if (iIndex < 0) {
			m_pComboBox->insertItem(0, fi.fileName(), sPath);
			iIndex = 0;
		}
		m_pComboBox->setCurrentIndex(iIndex);
		m_pComboBox->setToolTip(sPath);
		propertyChanged();
	}
}


// Property value change slot.
void qtractorPluginPropertyWidget::propertyChanged (void)
{
	if (m_pPlugin == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginPropertyWidget[%p]::propertyChanged()", this);
#endif

	QVariant value;

	if (m_pCheckBox)
		value = bool(m_pCheckBox->isChecked());
	if (m_pSpinBox)
		value = float(m_pSpinBox->value());
	if (m_pTextEdit && m_pTextEdit->document()->isModified()) {
		value = m_pTextEdit->toPlainText();
		m_pTextEdit->document()->setModified(false);
	}
	if (m_pComboBox) {
		const int iIndex = m_pComboBox->currentIndex();
		if (iIndex >= 0)
			value = m_pComboBox->itemData(iIndex).toString();
	}

	if (!value.isValid())
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(
			new qtractorPluginPropertyCommand(m_pPlugin, m_iProperty, value));
}


// Text edit (string) event filter.
bool qtractorPluginPropertyWidget::eventFilter (
	QObject *pObject, QEvent *pEvent )
{
	if (qobject_cast<QTextEdit *> (pObject) == m_pTextEdit) {
		if (pEvent->type() == QEvent::KeyPress) {
			QKeyEvent *pKeyEvent = static_cast<QKeyEvent*> (pEvent);
			if (pKeyEvent->key() == Qt::Key_Return
				&& (pKeyEvent->modifiers() &
					(Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
				propertyChanged();
				return true;
			}
		}
		else
		if (pEvent->type() == QEvent::FocusOut)
			propertyChanged();
	}

	return QWidget::eventFilter(pObject, pEvent);
}


// end of qtractorPluginForm.cpp
