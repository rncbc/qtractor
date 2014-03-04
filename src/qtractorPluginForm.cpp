// qtractorPluginForm.cpp
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
#include "qtractorPluginForm.h"
#include "qtractorPluginCommand.h"
#include "qtractorAudioEngine.h"

#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"

#include "qtractorInsertPlugin.h"

#include "qtractorObserverWidget.h"

#include "qtractorMidiControlObserverForm.h"

#include "qtractorBusForm.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorEngine.h"

#include <QMessageBox>
#include <QTabWidget>
#include <QGridLayout>
#include <QValidator>
#include <QLineEdit>
#include <QLabel>

#include <QFileInfo>
#include <QFileDialog>
#include <QUrl>

#include <QMenu>
#include <QKeyEvent>

#include "math.h"


// This shall hold the default preset name.
static QString g_sDefPreset;


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

	if (g_sDefPreset.isEmpty())
		g_sDefPreset = tr("(default)");

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
	QObject::connect(m_ui.SendsToolButton,
		SIGNAL(clicked()),
		SLOT(sendsSlot()));
	QObject::connect(m_ui.ReturnsToolButton,
		SIGNAL(clicked()),
		SLOT(returnsSlot()));
	QObject::connect(m_ui.AudioBusNameComboBox,
		SIGNAL(activated(const QString&)),
		SLOT(changeAudioBusNameSlot(const QString&)));
	QObject::connect(m_ui.AudioBusNameToolButton,
		SIGNAL(clicked()),
		SLOT(clickAudioBusNameSlot()));
	QObject::connect(m_ui.ActivateToolButton,
		SIGNAL(toggled(bool)),
		SLOT(activateSlot(bool)));

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

	qtractorPluginType *pType = m_pPlugin->type();

	const bool bVstPlugin = (pType->typeHint() == qtractorPluginType::Vst);
	const int MaxRowsPerPage    = (bVstPlugin ? 12 : 8);
	const int MaxColumnsPerPage = (bVstPlugin ?  2 : 3);
	const int MaxParamsPerPage  = MaxRowsPerPage * MaxColumnsPerPage;

	const qtractorPlugin::Params& params
		= m_pPlugin->params();
	const int iParams = params.count();

	int iParamsPerPage = iParams;
	int iParamsOnLastPage = 0;
	if (iParamsPerPage > MaxParamsPerPage) {
		iParamsPerPage = MaxParamsPerPage;
		iParamsOnLastPage = (iParams % iParamsPerPage);
		while (iParamsOnLastPage > 0
			&& iParamsOnLastPage < ((3 * iParamsPerPage) >> 2))
			iParamsOnLastPage = (iParams % --iParamsPerPage);
	}

	int iPages = 1;
	int iRowsPerPage = iParamsPerPage;
	int iColumnsPerPage = 1;
	if (iRowsPerPage > MaxRowsPerPage) {
		iPages = (iParams / iParamsPerPage);
		if (iParamsOnLastPage > 0)
			++iPages;
		while (iRowsPerPage > MaxRowsPerPage
			&& iColumnsPerPage < MaxColumnsPerPage)
			iRowsPerPage = (iParamsPerPage / ++iColumnsPerPage);
		if (iParamsPerPage % iColumnsPerPage) // Adjust to balance.
			++iRowsPerPage;
	}

	// Maybe we need a tabbed widget...
	QTabWidget  *pTabWidget  = NULL;
	QGridLayout *pGridLayout = NULL;
	QWidget     *pPageWidget = NULL;

	int iPage = 0;
	const QString sPage = tr("Page %1");
	if (iParams > 0) {
		pTabWidget  = m_ui.TabWidget;
		pGridLayout = new QGridLayout();
		pGridLayout->setMargin(8);
		pGridLayout->setSpacing(4);
		pPageWidget = new QWidget();
		pPageWidget->setLayout(pGridLayout);
		pTabWidget->insertTab(iPage, pPageWidget, sPage.arg(iPage + 1));
		++iPage;
	}

	// FIXME: Couldn't stand more than a hundred widgets?
	// or do we have one dedicated editor GUI?
	int iRow = 0;
	int iColumn = 0;

	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		qtractorPluginParamWidget *pParamWidget
			= new qtractorPluginParamWidget(pParam, this);
		m_paramWidgets.insert(pParam->index(), pParamWidget);
		pGridLayout->addWidget(pParamWidget, iRow, iColumn);
		qtractorMidiControlObserver *pMidiObserver = pParam->observer();
		if (pMidiObserver) {
		//	pMidiObserver->setCurveList(pPlugin->list()->curveList());
			addMidiControlAction(pParamWidget, pMidiObserver);
		}
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
	m_ui.SendsToolButton->setVisible(bInsertPlugin);
	m_ui.ReturnsToolButton->setVisible(bInsertPlugin);

	// Show aux-send tool options...
	const bool bAuxSendPlugin	= (pType->typeHint() == qtractorPluginType::AuxSend );
	m_ui.AudioBusNameComboBox->setVisible(bAuxSendPlugin);
	m_ui.AudioBusNameLabel->setVisible(bAuxSendPlugin);
	m_ui.AudioBusNameToolButton->setVisible(bAuxSendPlugin);
		
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

	// This should trigger paramsSlot(!bEditor)
	// and adjust the size of the params dialog...
	m_ui.DirectAccessParamPushButton->setVisible(iParams > 0);

	// Always first tab/page selected...
	m_ui.TabWidget->setCurrentIndex(0);

	// Clear any initial param update.
	qtractorSubject::resetQueue();

	updateActivated();
	refresh();
	stabilize();
	show();
}


void qtractorPluginForm::activateForm (void)
{
	if (!isVisible()) {
		if (m_pPlugin) toggleEditor(m_pPlugin->isEditorVisible());
		show();
	}

	raise();
	activateWindow();
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
		sEditText = g_sDefPreset;

	++m_iUpdate;
	m_ui.PresetComboBox->setEditText(sEditText);
	--m_iUpdate;
}

QString qtractorPluginForm::preset (void) const
{
	QString sPreset = m_ui.PresetComboBox->currentText();

	if (sPreset == g_sDefPreset || m_iDirtyCount > 0)
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


// Update specific aux-send audio bus settings.
void qtractorPluginForm::updateAudioBusName (void)
{
	if (m_pPlugin == NULL)
		return;

	m_ui.AudioBusNameComboBox->clear();

	const QIcon icon(":/images/trackAudio.png");
	m_ui.AudioBusNameComboBox->addItem(icon, tr("(none)"));

	qtractorAuxSendPlugin *pAuxSendPlugin = NULL;
	if ((m_pPlugin->type())->typeHint() == qtractorPluginType::AuxSend)
		pAuxSendPlugin = static_cast<qtractorAuxSendPlugin *> (m_pPlugin);
	if (pAuxSendPlugin == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	for (qtractorBus *pBus = pAudioEngine->buses().first();
			pBus; pBus = pBus->next()) {
		if (pBus->busMode() & qtractorBus::Output) {
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (pBus);
			if (pAudioBus && pAudioBus->channels() == m_pPlugin->channels())
				m_ui.AudioBusNameComboBox->addItem(icon, pAudioBus->busName());
		}
	}

	const QString& sAudioBusName = pAuxSendPlugin->audioBusName();
	int iIndex = m_ui.AudioBusNameComboBox->findText(sAudioBusName);
	if (iIndex < 0)
		iIndex = 0;
	m_ui.AudioBusNameComboBox->setCurrentIndex(iIndex);
}


// Update port widget state.
void qtractorPluginForm::changeParamValue ( unsigned long /*iIndex*/ )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::changeParamValue(%lu)", this, iIndex);
#endif

#if 0
	qtractorPluginParamWidget *pParamWidget
		= m_paramWidgets.value(iIndex, NULL);
	if (pParamWidget)
		pParamWidget->refresh();
#endif

	// Sure is dirty...
	++m_iDirtyCount;
	stabilize();
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

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	if (sPreset == g_sDefPreset) {
		// Reset to default...
		pSession->execute(
			new qtractorResetPluginCommand(m_pPlugin));
	}
	else
	if (!m_pPlugin->loadPreset(sPreset)) {
		// An existing preset is about to be loaded...
		QSettings& settings = pOptions->settings();
		// Should it be load from known file?...
		if ((m_pPlugin->type())->isConfigure()) {
			settings.beginGroup(m_pPlugin->presetGroup());
			m_pPlugin->loadPresetFile(settings.value(sPreset).toString());
			settings.endGroup();
			refresh();
		} else {
			//...or make it as usual (parameter list only)...
			settings.beginGroup(m_pPlugin->presetGroup());
			QStringList vlist = settings.value(sPreset).toStringList();
			settings.endGroup();
			if (!vlist.isEmpty()) {
				pSession->execute(
					new qtractorPresetPluginCommand(m_pPlugin, sPreset, vlist));
			}
		}
	}

	stabilize();
}


void qtractorPluginForm::openPresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;
	if (!(m_pPlugin->type())->isConfigure())
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
	const QString  sExt("qtx");
	const QString& sTitle  = tr("Open Preset") + " - " QTRACTOR_TITLE;
	const QString& sFilter = tr("Preset files (*.%1)").arg(sExt); 
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to save...
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs)
		options |= QFileDialog::DontUseNativeDialog;
	sFilename = QFileDialog::getOpenFileName(this,
		sTitle, pOptions->sPresetDir, sFilter, NULL, options);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(this,
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
	if (pOptions->bDontUseNativeDialogs)
		fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif
	// Have we a filename to load a preset from?
	if (!sFilename.isEmpty()) {
		if (m_pPlugin->loadPresetFile(sFilename)) {
			// Got it loaded alright...
			QFileInfo fi(sFilename);
			setPreset(fi.baseName()
				.replace((m_pPlugin->type())->label() + '-', QString()));
			pOptions->sPresetDir = fi.absolutePath();
		} else {
			// Failure (maybe wrong plugin)...
			QMessageBox::critical(this,
				tr("Error") + " - " QTRACTOR_TITLE,
				tr("Preset could not be loaded\n"
				"from \"%1\".\n\n"
				"Sorry.").arg(sFilename),
				QMessageBox::Cancel);
		}
	}
	refresh();

	stabilize();
}


void qtractorPluginForm::savePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset = m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// The current state preset is about to be saved...
	// this is where we'll make it!
	if (!m_pPlugin->savePreset(sPreset)) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup(m_pPlugin->presetGroup());
		// Which mode of preset?
		if ((m_pPlugin->type())->isConfigure()) {
			// Sure, we'll have something complex enough
			// to make it save into an external file...
			const QString sExt("qtx");
			QFileInfo fi(QDir(pOptions->sPresetDir),
				(m_pPlugin->type())->label() + '-' + sPreset + '.' + sExt);
			QString sFilename = fi.absoluteFilePath();
			// Prompt if file does not currently exist...
			if (!fi.exists()) {
				const QString& sTitle  = tr("Save Preset") + " - " QTRACTOR_TITLE;
				const QString& sFilter = tr("Preset files (*.%1)").arg(sExt);
			#if 1//QT_VERSION < 0x040400
				// Ask for the filename to save...
				QFileDialog::Options options = 0;
				if (pOptions->bDontUseNativeDialogs)
					options |= QFileDialog::DontUseNativeDialog;
				sFilename = QFileDialog::getSaveFileName(this,
					sTitle, sFilename, sFilter, NULL, options);
			#else
				// Construct save-file dialog...
				QFileDialog fileDialog(this,
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
				if (pOptions->bDontUseNativeDialogs)
					fileDialog.setOptions(QFileDialog::DontUseNativeDialog);
				// Show dialog...
				if (fileDialog.exec())
					sFilename = fileDialog.selectedFiles().first();
				else
					sFilename.clear();
			#endif
			}
			// We've a filename to save the preset
			if (!sFilename.isEmpty()) {
				if (QFileInfo(sFilename).suffix().isEmpty())
					sFilename += '.' + sExt;
				if (m_pPlugin->savePresetFile(sFilename)) {
					settings.setValue(sPreset, sFilename);
					pOptions->sPresetDir = QFileInfo(sFilename).absolutePath();
				}
			}
		}	// Just leave it to simple parameter value list...
		else settings.setValue(sPreset, m_pPlugin->valueList());
		settings.endGroup();
	}

	refresh();
	stabilize();
}


void qtractorPluginForm::deletePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset =  m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
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
	stabilize();
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
		m_pPlugin->openEditor(this);
	else
		m_pPlugin->closeEditor();

	--m_iUpdate;
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
void qtractorPluginForm::changeAudioBusNameSlot ( const QString& sAudioBusName )
{
	if (m_pPlugin == NULL)
		return;

	qtractorAuxSendPlugin *pAuxSendPlugin = NULL;
	if (m_pPlugin->type()->typeHint() == qtractorPluginType::AuxSend)
		pAuxSendPlugin = static_cast<qtractorAuxSendPlugin *> (m_pPlugin);
	if (pAuxSendPlugin == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorAuxSendPluginCommand(pAuxSendPlugin, sAudioBusName));
}


// Audio bus name (aux-send) browse slot.
void qtractorPluginForm::clickAudioBusNameSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	qtractorAuxSendPlugin *pAuxSendPlugin = NULL;
	if (m_pPlugin->type()->typeHint() == qtractorPluginType::AuxSend)
		pAuxSendPlugin = static_cast<qtractorAuxSendPlugin *> (m_pPlugin);
	if (pAuxSendPlugin == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;
	
	// Call here the bus management form.
	qtractorBusForm busForm(this);
	// Pre-select bus...
	const QString& sAudioBusName = m_ui.AudioBusNameComboBox->currentText();
	if (!sAudioBusName.isEmpty())
		busForm.setBus(pAudioEngine->findBus(sAudioBusName));
	// Go for it...
	busForm.exec();

	// Check if any buses have changed...
	if (busForm.isDirty())
		updateAudioBusName();
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


// Parameter-widget refreshner-loader.
void qtractorPluginForm::refresh (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginForm[%p]::refresh()", this);
#endif

	++m_iUpdate;

	qtractorSubject::flushQueue(true);

	const QString sOldPreset = m_ui.PresetComboBox->currentText();
	m_ui.PresetComboBox->clear();
	m_ui.PresetComboBox->insertItems(0, m_pPlugin->presetList());
	m_ui.PresetComboBox->model()->sort(0);
	m_ui.PresetComboBox->addItem(g_sDefPreset);
	m_ui.PresetComboBox->setEditText(sOldPreset);

	ParamWidgets::ConstIterator iter = m_paramWidgets.constBegin();
	const ParamWidgets::ConstIterator& iter_end = m_paramWidgets.constEnd();
	for ( ; iter != iter_end; ++iter)
		iter.value()->refresh();

	updateAudioBusName();

	m_pPlugin->idleEditor();

	qtractorSubject::resetQueue();

	m_iDirtyCount = 0;
	--m_iUpdate;
}


// Preset control.
void qtractorPluginForm::stabilize (void)
{
	bool bExists  = false;
	bool bEnabled = (m_pPlugin != NULL);
	m_ui.ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled) {
		bEnabled = (
			(m_pPlugin->type())->controlIns() > 0 ||
			(m_pPlugin->type())->isConfigure());
	}

	m_ui.PresetComboBox->setEnabled(bEnabled);
	m_ui.OpenPresetToolButton->setVisible(
		bEnabled && (m_pPlugin->type())->isConfigure());

	if (bEnabled) {
		const QString& sPreset = m_ui.PresetComboBox->currentText();
		bEnabled = !sPreset.isEmpty()
			&& sPreset != g_sDefPreset
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
	SliderInterface (
		qtractorObserverSlider *pSlider, qtractorPluginParam *pParam )
		: qtractorObserverSlider::Interface(pSlider), m_pParam(pParam) {}

	// Formerly Pure virtuals.
	float scaleFromValue ( float fValue ) const
		{ return 10000.0f * m_pParam->observer()->scaleFromValue(fValue,
			m_pParam->isLogarithmic()); }

	float valueFromScale ( float fScale ) const
		{ return m_pParam->observer()->valueFromScale((fScale / 10000.0f),
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
	: QFrame(pParent), m_pParam(pParam)
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
		if (m_pParam->isDisplay()) {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		//	pLabel->setFixedWidth(72);
			pLabel->setMinimumWidth(64);
			pLabel->setText(m_pParam->name());
			pGridLayout->addWidget(pLabel, 0, 0);
		} else {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			pLabel->setText(m_pParam->name() + ':');
			pGridLayout->addWidget(pLabel, 0, 0, 1, 3);
		}
		m_pSlider = new qtractorObserverSlider(/*this*/);
		m_pSlider->setInterface(new SliderInterface(m_pSlider, m_pParam));
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
			m_pSpinBox->setMaximumWidth(64);
			m_pSpinBox->setDecimals(iDecimals);
			m_pSpinBox->setMinimum(m_pParam->minValue());
			m_pSpinBox->setMaximum(m_pParam->maxValue());
			m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecimals)));
			m_pSpinBox->setAccelerated(true);
			m_pSpinBox->setSubject(m_pParam->subject());
		//	m_pSpinBox->setValue(m_pParam->value());
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

	QFrame::setLayout(pGridLayout);
//	QFrame::setFrameShape(QFrame::StyledPanel);
//	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setToolTip(m_pParam->name());
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


// end of qtractorPluginForm.cpp
