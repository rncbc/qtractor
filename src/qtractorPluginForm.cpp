// qtractorPluginForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorPluginListView.h"

#include "qtractorInsertPlugin.h"

#include "qtractorObserverWidget.h"

#include "qtractorMidiControlObserverForm.h"

#include "qtractorSpinBox.h"

#include "qtractorBusForm.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorEngine.h"

#include "qtractorCurve.h"

#include <QMessageBox>
#include <QTabWidget>
#include <QGridLayout>
#include <QSpacerItem>
#include <QValidator>
#include <QTextEdit>
#include <QLabel>

#include <QFileInfo>
#include <QFileDialog>
#include <QUrl>

#include <QMenu>
#include <QKeyEvent>
#include <QHideEvent>

#include <cmath>


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.
//
int    qtractorPluginForm::g_iIconsRefCount = 0;
QIcon *qtractorPluginForm::g_pIcons[2] = { nullptr, nullptr };

// Constructor.
qtractorPluginForm::qtractorPluginForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags)
{
	// Setup (de)activated icon stuff...
	if (++g_iIconsRefCount == 1) {
		const QPixmap& pmLedOff
			= QIcon::fromTheme("itemLedOff").pixmap(8, 8);
		const QPixmap& pmLedOn
			= QIcon::fromTheme("itemLedOn").pixmap(8, 8);
		const QPixmap& pmLedDim
			= QIcon::fromTheme("itemLedDim").pixmap(8, 8);
		QIcon *pIconActivated = new QIcon();
		pIconActivated->addPixmap(pmLedOff, QIcon::Active, QIcon::Off);
		pIconActivated->addPixmap(pmLedOn, QIcon::Active, QIcon::On);
		QIcon *pIconDeactivated = new QIcon();
		pIconDeactivated->addPixmap(pmLedOff, QIcon::Active, QIcon::Off);
		pIconDeactivated->addPixmap(pmLedDim, QIcon::Active, QIcon::On);
		g_pIcons[0] = pIconActivated;
		g_pIcons[1] = pIconDeactivated;
	}

	// Setup UI struct...
	m_ui.setupUi(this);

	QWidget::setAttribute(Qt::WA_QuitOnClose, false);

	const QFont& font = QWidget::font();
	QWidget::setFont(QFont(font.family(), font.pointSize() - 1));

	m_pPlugin     = nullptr;
	m_iDirtyCount = 0;
	m_iUpdate     = 0;

	m_pDirectAccessParamMenu = new QMenu();
	m_ui.DirectAccessParamPushButton->setMenu(m_pDirectAccessParamMenu);

	m_ui.PresetComboBox->setValidator(
		new QRegularExpressionValidator(
			QRegularExpression("[\\w-]+"), m_ui.PresetComboBox));
	m_ui.PresetComboBox->setInsertPolicy(QComboBox::NoInsert);
	m_ui.PresetComboBox->setCompleter(nullptr);

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	// Some conveniency cleaner helper...
	m_ui.AliasLineEdit->setClearButtonEnabled(true);
#endif

	// UI signal/slot connections...
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changePresetSlot(const QString&)));
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(activated(int)),
		SLOT(loadPresetSlot(int)));
	QObject::connect(m_ui.OpenPresetToolButton,
		SIGNAL(clicked()),
		SLOT(openPresetSlot()));
	QObject::connect(m_ui.SavePresetToolButton,
		SIGNAL(clicked()),
		SLOT(savePresetSlot()));
	QObject::connect(m_ui.DeletePresetToolButton,
		SIGNAL(clicked()),
		SLOT(deletePresetSlot()));
	QObject::connect(m_ui.AliasLineEdit,
		SIGNAL(editingFinished()),
		SLOT(aliasSlot()));
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
		SIGNAL(activated(int)),
		SLOT(changeAuxSendBusNameSlot(int)));
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
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl && m_pPlugin) {
		pMidiControl->unmapMidiObserverWidget(
			m_pPlugin->activateObserver(), m_ui.ActivateToolButton);
	}

	clear();

	delete m_pDirectAccessParamMenu;

	if (--g_iIconsRefCount == 0) {
		for (int i = 0; i < 2; ++i) {
			delete g_pIcons[i];
			g_pIcons[i] = nullptr;
		}
	}
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	clear();

	// Set the new reference...
	m_pPlugin = pPlugin;

	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	// Dispatch any pending updates.
	qtractorSubject::flushQueue(true);

	// Set activate button MIDI controller observer...
	addMidiControlAction(
		m_ui.ActivateToolButton,
		m_pPlugin->activateObserver());

	const qtractorPluginType::Hint typeHint = pType->typeHint();
	const bool bTwoColumnPage
		= (typeHint == qtractorPluginType::Vst2
		|| typeHint == qtractorPluginType::Vst3
		|| typeHint == qtractorPluginType::Clap);
	const int MaxRowsPerPage    = (bTwoColumnPage ? 12 : 8);
	const int MaxColumnsPerPage = (bTwoColumnPage ?  2 : 3);
	const int MaxItemsPerPage   = MaxRowsPerPage * MaxColumnsPerPage;

	const qtractorPlugin::Params& params = m_pPlugin->params();
	const qtractorPlugin::Properties& properties = m_pPlugin->properties();

	int iItems = params.count() + properties.count();
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
	QTabWidget  *pTabWidget  = nullptr;
	QGridLayout *pGridLayout = nullptr;
	QWidget     *pPageWidget = nullptr;

	int iPage = 0;
	const QString sPage = tr("Page %1");
	if (iItems > 0) {
		pTabWidget  = m_ui.TabWidget;
		pGridLayout = new QGridLayout();
		pGridLayout->setContentsMargins(8, 8, 8, 8);
		pGridLayout->setSpacing(4);
		pPageWidget = new QWidget();
		pPageWidget->setLayout(pGridLayout);
		pTabWidget->insertTab(iPage, pPageWidget, sPage.arg(iPage + 1));
		++iPage;
	}

	const qtractorPlugin::PropertyKeys& props = m_pPlugin->propertyKeys();
	qtractorPlugin::PropertyKeys::ConstIterator prop = props.constBegin();
	const qtractorPlugin::PropertyKeys::ConstIterator& prop_end = props.constEnd();
	for ( ; prop != prop_end; ++prop) {
		qtractorPlugin::Property *pProp = prop.value();
		qtractorPluginParamWidget *pPropWidget
			= new qtractorPluginParamWidget(pProp, this);
		if (pProp->isAutomatable()) {
			qtractorMidiControlObserver *pMidiObserver = pProp->observer();
			if (pMidiObserver)
				addMidiControlAction(pPropWidget, pMidiObserver);
		}
		m_paramWidgets.append(pPropWidget);
	}

	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPlugin::Param *pParam = param.value();
		qtractorPluginParamWidget *pParamWidget
			= new qtractorPluginParamWidget(pParam, this);
		qtractorMidiControlObserver *pMidiObserver = pParam->observer();
		if (pMidiObserver)
			addMidiControlAction(pParamWidget, pMidiObserver);
		m_paramWidgets.append(pParamWidget);
	}

	// FIXME: Couldn't stand more than a hundred widgets?
	// or do we have one dedicated editor GUI?
	int iRow = 0;
	int iColumn = 0;

	iColumnsPerPage += (iColumnsPerPage - 1); // Plus gap columns!
	if (!m_paramWidgets.isEmpty())
		pGridLayout->setColumnStretch(iColumn, 1);
	QListIterator<qtractorPluginParamWidget *> iter(m_paramWidgets);
	while (iter.hasNext()) {
		pGridLayout->addWidget(iter.next(), iRow, iColumn);
		if (++iRow >= iRowsPerPage) {
			pGridLayout->addItem(new QSpacerItem(0, 0,
				QSizePolicy::Minimum,
				QSizePolicy::Expanding), iRow, 0, 1, iColumnsPerPage);
			iRow = 0;
			if (++iColumn >= iColumnsPerPage) {
				iColumn = 0;
				if (pTabWidget && iPage < iPages) {
					pGridLayout = new QGridLayout();
					pGridLayout->setContentsMargins(8, 8, 8, 8);
					pGridLayout->setSpacing(4);
					pPageWidget = new QWidget();
					pPageWidget->setLayout(pGridLayout);
					pTabWidget->insertTab(iPage, pPageWidget, sPage.arg(iPage + 1));
					++iPage;
				}
			}
			else
			pGridLayout->setColumnMinimumWidth(iColumn++, 8); // Gap column!
			pGridLayout->setColumnStretch(iColumn, 1);
		}
	}

	// Show editor button if available?
	const bool bEditor = pType->isEditor();
	m_ui.EditToolButton->setVisible(bEditor);
	if (bEditor)
		toggleEditor(m_pPlugin->isEditorVisible());

	// Show insert tool options...
	const bool bInsertPlugin = (typeHint == qtractorPluginType::Insert);
	if (bInsertPlugin) {
		if (pType->index() > 0) { // index == channels > 0 => Audio insert.
			m_ui.SendsToolButton->setIcon(QIcon::fromTheme("itemAudioPortOut"));
			m_ui.ReturnsToolButton->setIcon(QIcon::fromTheme("itemAudioPortIn"));
		} else {
			m_ui.SendsToolButton->setIcon(QIcon::fromTheme("itemMidiPortOut"));
			m_ui.ReturnsToolButton->setIcon(QIcon::fromTheme("itemMidiPortIn"));
		}
	}
	m_ui.SendsToolButton->setVisible(bInsertPlugin);
	m_ui.ReturnsToolButton->setVisible(bInsertPlugin);

	// Show aux-send tool options...
	const bool bAuxSendPlugin = (typeHint == qtractorPluginType::AuxSend);
	m_ui.AuxSendBusNameComboBox->setVisible(bAuxSendPlugin);
	m_ui.AuxSendBusNameLabel->setVisible(bAuxSendPlugin);
	m_ui.AuxSendBusNameToolButton->setVisible(bAuxSendPlugin);

	// Set initial plugin preset name...
	setPreset(m_pPlugin->preset());

	// Set plugin name/caption as title,
	// maybe redundant but necessary...
	m_pPlugin->updateEditorTitle();

	// About page...
	m_ui.NameTextLabel->setText(pType->name());
	m_ui.TypeHintTextLabel->setText(
		qtractorPluginType::textFromHint(typeHint));
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
	m_ui.DirectAccessParamPushButton->setVisible(!params.isEmpty());

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
	if (m_pPlugin == nullptr)
		return;

	if (m_iUpdate > 0)
		return;

	++m_iUpdate;
	m_ui.ActivateToolButton->setIcon(
		*g_pIcons[m_pPlugin->isAutoDeactivated() ? 1 : 0]);
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
	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	m_ui.AuxSendBusNameComboBox->clear();

	if (pType->typeHint() != qtractorPluginType::AuxSend)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	QString sAuxSendBusName;

	if (pType->index() > 0) { // index == channels > 0 => Audio aux-send.
		qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
			= static_cast<qtractorAudioAuxSendPlugin *> (m_pPlugin);
		if (pAudioAuxSendPlugin == nullptr)
			return;
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine == nullptr)
			return;
		qtractorPluginList *pPluginList = pAudioAuxSendPlugin->list();
		if (pPluginList == nullptr)
			return;
		const bool bAudioOutBus
			= (pPluginList->flags() == qtractorPluginList::AudioOutBus);
		QStringList cyclicAudioOutBuses; // Cyclic bus names to avoid.
		const QIcon& iconAudio = QIcon::fromTheme("trackAudio");
		m_ui.AuxSendBusNameComboBox->addItem(iconAudio, tr("(none)"));
		QListIterator<qtractorBus *> iter(pAudioEngine->buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
			if (pBus->busMode() & qtractorBus::Output) {
				qtractorAudioBus *pAudioBus
					= static_cast<qtractorAudioBus *> (pBus);
				if (pAudioBus && pAudioBus->channels() == m_pPlugin->channels()) {
					if (bAudioOutBus && // Skip current or cyclic buses...
						pAudioBus->pluginList_out() == pPluginList) {
						cyclicAudioOutBuses.append(
							pAudioEngine->cyclicAudioOutBuses(pAudioBus));
						continue;
					}
					m_ui.AuxSendBusNameComboBox->addItem(iconAudio,
						pAudioBus->busName());
				}
			}
		}
		// Avoid cyclic bus names...
		QStringListIterator cyclic_iter(cyclicAudioOutBuses);
		while (cyclic_iter.hasNext()) {
			const QString& sBusName = cyclic_iter.next();
			if (!sBusName.isEmpty()) {
				const int iIndex
					= m_ui.AuxSendBusNameComboBox->findText(sBusName);
				if (iIndex >= 0)
					m_ui.AuxSendBusNameComboBox->removeItem(iIndex);
 			}
 		}
		// Proceed...
		sAuxSendBusName = pAudioAuxSendPlugin->audioBusName();
	} else {
		qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
			= static_cast<qtractorMidiAuxSendPlugin *> (m_pPlugin);
		if (pMidiAuxSendPlugin == nullptr)
			return;
		qtractorMidiEngine *pMidiEngine = pSession->midiEngine();
		if (pMidiEngine == nullptr)
			return;
		qtractorPluginList *pPluginList = pMidiAuxSendPlugin->list();
		if (pPluginList == nullptr)
			return;
		const bool bMidiOutBus
			= (pPluginList->flags() == qtractorPluginList::MidiOutBus);
		const QIcon& iconMidi = QIcon::fromTheme("trackMidi");
		m_ui.AuxSendBusNameComboBox->addItem(iconMidi, tr("(none)"));
		QListIterator<qtractorBus *> iter(pMidiEngine->buses2());
		while (iter.hasNext()) {
			qtractorBus *pBus = iter.next();
			if (pBus->busMode() & qtractorBus::Output) {
				qtractorMidiBus *pMidiBus
					= static_cast<qtractorMidiBus *> (pBus);
				if (pMidiBus) {
					if (bMidiOutBus && pMidiBus->pluginList_out() == pPluginList)
						continue;
					m_ui.AuxSendBusNameComboBox->addItem(iconMidi,
						pMidiBus->busName());
				}
			}
		}
		sAuxSendBusName = pMidiAuxSendPlugin->midiBusName();
	}

	int iIndex = m_ui.AuxSendBusNameComboBox->findText(sAuxSendBusName);
	if (iIndex < 0)
		iIndex = 0;
	m_ui.AuxSendBusNameComboBox->setCurrentIndex(iIndex);

	m_pPlugin->updateEditorTitle();
}


// Editor widget methods.
void qtractorPluginForm::toggleEditor ( bool bOn )
{
	if (m_pPlugin == nullptr)
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


void qtractorPluginForm::loadPresetSlot ( int iPreset )
{
	if (m_pPlugin == nullptr)
		return;

	if (m_iUpdate > 0)
		return;

	const QString& sPreset
		= m_ui.PresetComboBox->itemText(iPreset);
	if (sPreset.isEmpty())
		return;

	m_pPlugin->loadPresetEx(sPreset);

	stabilize();
}


void qtractorPluginForm::openPresetSlot (void)
{
	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	const bool bVst2Plugin = (pType->typeHint() == qtractorPluginType::Vst2);
	if (!pType->isConfigure() && !bVst2Plugin)
		return;

	if (m_iUpdate > 0)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	// We'll assume that there's an external file...
	QString sFilename;

	// Prompt if file does not currently exist...
	const QString sExt("qtx");
	QStringList exts(sExt);
	if (bVst2Plugin) {
		exts.append("fxp");
		exts.append("fxb");
	}

	const QString& sTitle
		= tr("Open Preset");

	QStringList filters;
	filters.append(tr("Preset files (*.%1)").arg(exts.join(" *.")));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	// Ask for the filename to save...
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, pOptions->sPresetDir, sFilter, nullptr, options);
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
				tr("Error"),
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
	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	const QString& sPreset = m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == qtractorPlugin::defPreset())
		return;

	// The current state preset is about to be saved...
	// this is where we'll make it!
	if (m_pPlugin->savePreset(sPreset)) {
		refresh();
		return;
	}

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	QSettings& settings = pOptions->settings();
	settings.beginGroup(m_pPlugin->presetGroup());
	// Which mode of preset?
	const bool bVst2Plugin = (pType->typeHint() == qtractorPluginType::Vst2);
	if (pType->isConfigure() || bVst2Plugin) {
		// Sure, we'll have something complex enough
		// to make it save into an external file...
		const QString sExt("qtx");
		QStringList exts(sExt);
		if (bVst2Plugin) { exts.append("fxp"); exts.append("fxb"); }
		QFileInfo fi(settings.value(sPreset).toString());
		if (!fi.exists() || !fi.isFile())
			fi.setFile(QDir(pOptions->sPresetDir),
				pType->label() + '-' + sPreset + '.' + sExt);
		QString sFilename = fi.absoluteFilePath();
		// Prompt if file does not currently exist...
		if (!fi.exists()) {
			const QString& sTitle
				= tr("Save Preset");
			QStringList filters;
			filters.append(tr("Preset files (*.%1)").arg(exts.join(" *.")));
			filters.append(tr("All files (*.*)"));
			const QString& sFilter = filters.join(";;");
			QWidget *pParentWidget = nullptr;
			QFileDialog::Options options;
			if (pOptions->bDontUseNativeDialogs) {
				options |= QFileDialog::DontUseNativeDialog;
				pParentWidget = QWidget::window();
			}
		#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
			// Ask for the filename to save...
			sFilename = QFileDialog::getSaveFileName(pParentWidget,
				sTitle, sFilename, sFilter, nullptr, options);
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
					tr("Error"),
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
	if (m_pPlugin == nullptr)
		return;

	const QString& sPreset =  m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == qtractorPlugin::defPreset())
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	// A preset entry is about to be deleted;
	// prompt user if he/she's sure about this...
	if (pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning"),
			tr("About to delete preset:\n\n"
			"\"%1\" (%2)\n\n"
			"Are you sure?")
			.arg(sPreset)
			.arg(m_pPlugin->title()),
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


// Alias finish-editing slot.
void qtractorPluginForm::aliasSlot (void)
{
	if (m_pPlugin == nullptr)
		return;

	if (m_iUpdate > 0)
		return;

	++m_iUpdate;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(
			new qtractorPluginAliasCommand(m_pPlugin, m_ui.AliasLineEdit->text()));

	--m_iUpdate;
}


// Editor slot.
void qtractorPluginForm::editSlot ( bool bOn )
{
	if (m_pPlugin == nullptr)
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
	if (m_pPlugin == nullptr)
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
void qtractorPluginForm::changeAuxSendBusNameSlot ( int iAuxSendBusName )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	if (pType->typeHint() != qtractorPluginType::AuxSend)
		return;

	const QString& sAuxSendBusName
		= m_ui.AuxSendBusNameComboBox->itemText(iAuxSendBusName);
	if (sAuxSendBusName.isEmpty())
		return;

	pSession->execute(
		new qtractorAuxSendPluginCommand(m_pPlugin, sAuxSendBusName));
}


// Audio bus name (aux-send) browse slot.
void qtractorPluginForm::clickAuxSendBusNameSlot (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	if (pType->typeHint() != qtractorPluginType::AuxSend)
		return;

	qtractorEngine *pEngine = nullptr;
	if (pType->index() > 0) // index == channels > 0 => Audio aux-send.
		pEngine = pSession->audioEngine();
	else
		pEngine = pSession->midiEngine();
	if (pEngine == nullptr)
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

	if (m_pPlugin == nullptr)
		return;

	QAction *pAction;
	const int iDirectAccessParamIndex = m_pPlugin->directAccessParamIndex();
	const qtractorPlugin::Params& params = m_pPlugin->params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPlugin::Param *pParam = param.value();
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
	if (m_pPlugin == nullptr)
		return;

	if (m_iUpdate > 0)
		return;

	// Retrieve direct access parameter index from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction == nullptr)
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
	if (m_pPlugin == nullptr)
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

	// Update the nominal user/title if any...
	m_ui.AliasLineEdit->setText(m_pPlugin->alias());

	m_ui.ActivateToolButton->setChecked(m_pPlugin->isActivated());

//	m_pPlugin->idleEditor();

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
	if (m_pPlugin == nullptr)
		return;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType == nullptr)
		return;

	const bool bVst2Plugin = (pType->typeHint() == qtractorPluginType::Vst2);

	bool bExists  = false;
	bool bEnabled = (m_pPlugin != nullptr);
	m_ui.ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled)
		bEnabled = (pType->controlIns() > 0	|| pType->isConfigure());

	m_ui.PresetComboBox->setEnabled(bEnabled);
	m_ui.OpenPresetToolButton->setVisible(
		bEnabled && (pType->isConfigure() || bVst2Plugin));

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
	if (m_pPlugin == nullptr)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
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
	qtractorPluginParamDisplay(qtractorPlugin::Param *pParam)
		: QLabel(), m_pParam(pParam), m_observer(pParam->subject(), this) {}

	// Observer accessor.
	Observer *observer() { return &m_observer; }

protected:

	void updateDisplay() { QLabel::setText(m_pParam->display()); }

private:

	// Parameter reference.
	qtractorPlugin::Param *m_pParam;

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
	SliderInterface ( qtractorPlugin::Param *pParam )
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
	qtractorPlugin::Param *m_pParam;
};


//----------------------------------------------------------------------------
// qtractorPluginParamWidget -- Plugin parameter/property common widget.
//

// Constructors.
qtractorPluginParamWidget::qtractorPluginParamWidget (
	qtractorPlugin::Param *pParam, QWidget *pParent )
	: QWidget(pParent), m_pParam(pParam)
{
	m_pSlider   = nullptr;
	m_pSpinBox  = nullptr;
	m_pCheckBox = nullptr;
	m_pDisplay  = nullptr;

	m_pCurveButton = nullptr;

	m_pTextEdit   = nullptr;
	m_pComboBox   = nullptr;
	m_pToolButton = nullptr;

	QGridLayout *pGridLayout = new QGridLayout();
	pGridLayout->setContentsMargins(0, 0, 0, 0);
	pGridLayout->setSpacing(4);

	qtractorPlugin::Property *pProp = property();

	if (m_pParam->isToggled()) {
		m_pCheckBox = new qtractorObserverCheckBox(/*this*/);
	//	m_pCheckBox->setMinimumWidth(120);
		m_pCheckBox->setText(m_pParam->name());
		m_pCheckBox->setSubject(m_pParam->subject());
	//	m_pCheckBox->setChecked(m_pParam->value() > 0.1f);
		pGridLayout->addWidget(m_pCheckBox, 0, 0);
		pGridLayout->setColumnStretch(0, 2);
		if (m_pParam->isDisplay()) {
			m_pDisplay = new qtractorPluginParamDisplay(m_pParam);
			m_pDisplay->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
		//	m_pDisplay->setText(m_pParam->display());
			m_pDisplay->setMinimumWidth(64);
			pGridLayout->addWidget(m_pDisplay, 0, 2);
		}
	} else {
		QLabel *pLabel = new QLabel(/*this*/);
		pLabel->setText(m_pParam->name() + ':');
		if (pProp && pProp->isString()) {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
		//	pLabel->setMinimumWidth(120);
			pGridLayout->addWidget(pLabel, 0, 0, 1, 3);
			m_pTextEdit = new QTextEdit(/*this*/);
			m_pTextEdit->setTabChangesFocus(true);
			m_pTextEdit->setMinimumWidth(120);
			m_pTextEdit->setMaximumHeight(60);
			m_pTextEdit->installEventFilter(this);
		//	m_pTextEdit->setPlainText(pProp->variant().toString());
			pGridLayout->addWidget(m_pTextEdit, 1, 0, 2, 3);
		}
		else
		if (pProp && pProp->isPath()) {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
		//	pLabel->setMinimumWidth(120);
			pGridLayout->addWidget(pLabel, 0, 0, 1, 3);
			m_pComboBox = new QComboBox(/*this*/);
			m_pComboBox->setEditable(false);
			m_pComboBox->setMinimumWidth(120);
		//	m_pComboBox->addItem(pProp->variant().toString());
			pGridLayout->addWidget(m_pComboBox, 1, 0, 1, 2);
			pGridLayout->setColumnStretch(0, 2);
			m_pToolButton = new QToolButton(/*this*/);
			m_pToolButton->setIcon(QIcon::fromTheme("fileOpen"));
			pGridLayout->addWidget(m_pToolButton, 1, 2);
		}
		else
		if (m_pParam->isInteger()) {
			pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
			pGridLayout->addWidget(pLabel, 0, 0);
			m_pSpinBox = new qtractorObserverSpinBox(/*this*/);
			m_pSpinBox->setMinimumWidth(64);
			m_pSpinBox->setMaximumWidth(96);
			m_pSpinBox->setDecimals(0);
			m_pSpinBox->setMinimum(m_pParam->minValue());
			m_pSpinBox->setMaximum(m_pParam->maxValue());
			m_pSpinBox->setAlignment(pProp ? Qt::AlignRight : Qt::AlignHCenter);
			m_pSpinBox->setSubject(m_pParam->subject());
		//	m_pSpinBox->setValue(int(m_pParam->value()));
			pGridLayout->addWidget(m_pSpinBox, 0, 1,
				Qt::AlignRight | Qt::AlignVCenter);
			pGridLayout->setColumnStretch(1, 2);
			if (m_pParam->isDisplay()) {
				m_pDisplay = new qtractorPluginParamDisplay(m_pParam);
				m_pDisplay->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
			//	m_pDisplay->setText(m_pParam->display());
				m_pDisplay->setMinimumWidth(64);
				pGridLayout->addWidget(m_pDisplay, 0, 2);
			}
		} else {
			if (m_pParam->isDisplay()) {
				pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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
			m_pSlider->setPageStep(250);
			m_pSlider->setSingleStep(50);
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
				pGridLayout->addWidget(m_pSlider, 1, 0, 1, 3);
				const int iDecimals = m_pParam->decimals();
				m_pSpinBox = new qtractorObserverSpinBox(/*this*/);
				m_pSpinBox->setMinimumWidth(64);
				m_pSpinBox->setMaximumWidth(96);
				m_pSpinBox->setDecimals(iDecimals);
				m_pSpinBox->setMinimum(m_pParam->minValue());
				m_pSpinBox->setMaximum(m_pParam->maxValue());
				m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecimals)));
				m_pSpinBox->setAccelerated(true);
				m_pSpinBox->setSubject(m_pParam->subject());
			//	m_pSpinBox->setValue(m_pParam->value());
				pGridLayout->addWidget(m_pSpinBox, 1, 3);
			}
		}
	}

	QWidget::setLayout(pGridLayout);

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

	if (m_pComboBox) {
		QObject::connect(m_pComboBox,
			SIGNAL(activated(int)),
			SLOT(propertyChanged()));
	}

	if (m_pToolButton) {
		QObject::connect(m_pToolButton,
			SIGNAL(clicked()),
			SLOT(toolButtonClicked()));
	}

	updateCurveButton();
}


// Destructor.
qtractorPluginParamWidget::~qtractorPluginParamWidget (void)
{
	qtractorMidiControl *pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl && m_pParam)
		pMidiControl->unmapMidiObserverWidget(m_pParam->observer(), this);
}


// Param/Property discriminator..
qtractorPlugin::Property *qtractorPluginParamWidget::property (void) const
{
	return dynamic_cast<qtractorPlugin::Property *> (m_pParam);
}


// Refreshner-loader method.
void qtractorPluginParamWidget::refresh (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginParamWidget[%p]::refresh()", this);
#endif

	qtractorPlugin::Property *pProp = property();
	if (pProp && !pProp->isAutomatable()) {
		if (m_pCheckBox) {
			const bool bCheckBox = m_pCheckBox->blockSignals(true);
			m_pCheckBox->setChecked(pProp->variant().toBool());
			m_pCheckBox->blockSignals(bCheckBox);
		}
		if (m_pSpinBox) {
			const bool bSpinBox = m_pSpinBox->blockSignals(true);
			m_pSpinBox->setValue(pProp->variant().toDouble());
			m_pSpinBox->blockSignals(bSpinBox);
		}
		if (m_pTextEdit) {
			const bool bTextEdit = m_pTextEdit->blockSignals(true);
			m_pTextEdit->setPlainText(pProp->variant().toString());
			m_pTextEdit->document()->setModified(false);
			m_pTextEdit->blockSignals(bTextEdit);
		}
		if (m_pComboBox) {
			const bool bComboBox = m_pComboBox->blockSignals(true);
			const QFileInfo fi(pProp->variant().toString().remove('\0'));
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
	} else {
		if (m_pCheckBox)
			m_pCheckBox->observer()->update(true);
		if (m_pSpinBox)
			m_pSpinBox->observer()->update(true);
		if (m_pSlider)
			m_pSlider->observer()->update(true);
		if (m_pDisplay)
			m_pDisplay->observer()->update(true);
	}

	updateCurveButton();
}


// Parameter automation curve status update/refresh.
void qtractorPluginParamWidget::updateCurveButton (void)
{
	qtractorPlugin::Property *pProp = property();
	if (pProp && !pProp->isAutomatable())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	QGridLayout *pGridLayout
		=  static_cast<QGridLayout *> (QWidget::layout());
	if (pGridLayout == nullptr)
		return;

	qtractorMidiControlObserver *pMidiObserver = m_pParam->observer();
	if (pMidiObserver == nullptr)
		return;

	qtractorPluginList *pPluginList = (m_pParam->plugin())->list();
	if (pPluginList == nullptr)
		return;

	qtractorTrack *pTrack = nullptr;
	qtractorCurveList *pCurveList = pPluginList->curveList();
	if (pCurveList && pCurveList == pMidiObserver->curveList())
		pTrack = pSession->findTrackCurveList(pCurveList);
	if (pTrack == nullptr) {
		if (m_pCurveButton) {
			m_pCurveButton->hide();
			delete m_pCurveButton;
			m_pCurveButton = nullptr;
			pGridLayout->setColumnMinimumWidth(3, 0);
		}
		// Bail out!
		return;
	}

	if (m_pCurveButton == nullptr) {
		QSize iconSize(16, 16);
		m_pCurveButton = new QPushButton(/*this*/);
		m_pCurveButton->setFlat(true);
		m_pCurveButton->setIconSize(iconSize);
		m_pCurveButton->setMaximumSize(iconSize + QSize(4, 4));
		pGridLayout->addWidget(m_pCurveButton, 0, 3,
			Qt::AlignRight | Qt::AlignVCenter);
		pGridLayout->setColumnMinimumWidth(3, 22);
		QObject::connect(m_pCurveButton,
			SIGNAL(clicked()),
			SLOT(curveButtonClicked()));
	}

	qtractorCurve *pCurve = nullptr;
	qtractorSubject *pSubject = pMidiObserver->subject();
	if (pSubject)
		pCurve = pSubject->curve();
	if (pCurve && pCurve->isCapture())
		m_pCurveButton->setIcon(QIcon::fromTheme("trackCurveCapture"));
	else
	if (pCurve && pCurve->isProcess())
		m_pCurveButton->setIcon(QIcon::fromTheme("trackCurveProcess"));
	else
	if (pCurve)
		m_pCurveButton->setIcon(QIcon::fromTheme("trackCurveEnabled"));
	else
		m_pCurveButton->setIcon(QIcon::fromTheme("trackCurveNone"));
}


// Parameter value change slot.
void qtractorPluginParamWidget::updateValue ( float fValue )
{
	qtractorPlugin::Property *pProp = property();
	if (pProp)
		propertyChanged();
	else
		m_pParam->updateValue(fValue, true);
}


// Automation curve selector.
void qtractorPluginParamWidget::curveButtonClicked (void)
{
	const QPoint& pos = m_pCurveButton->geometry().bottomLeft();
	qtractorMidiControlObserverForm::midiControlMenu(this, pos);
}


// Property file selector.
void qtractorPluginParamWidget::toolButtonClicked (void)
{
	// Sure we have this...
	if (m_pComboBox == nullptr)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return;

	// Ask for the filename to open...
	const int iIndex = m_pComboBox->currentIndex();
	if (iIndex < 0)
		return;

	QString sFilename = m_pComboBox->itemData(iIndex).toString();

	const QString& sTitle
		= tr("Open File");

	QWidget *pParentWidget = nullptr;
	QFileDialog::Options options;
	if (pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < QT_VERSION_CHECK(4, 4, 0)
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, sFilename, QString(), nullptr, options);
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
		const bool bComboBox = m_pComboBox->blockSignals(true);
		const QFileInfo fi(sFilename);
		const QString& sPath = fi.canonicalFilePath();
		int iIndex = m_pComboBox->findData(sPath);
		if (iIndex < 0) {
			m_pComboBox->insertItem(0, fi.fileName(), sPath);
			iIndex = 0;
		}
		m_pComboBox->setCurrentIndex(iIndex);
		m_pComboBox->setToolTip(sPath);
		m_pComboBox->blockSignals(bComboBox);
		propertyChanged();
	}
}


// Property value change slot.
void qtractorPluginParamWidget::propertyChanged (void)
{
	qtractorPlugin::Property *pProp = property();
	if (pProp == nullptr)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginParamWidget[%p]::propertyChanged()", this);
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
			value = m_pComboBox->itemData(iIndex);
	}

	if (!value.isValid())
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(
			new qtractorPluginPropertyCommand(pProp, value));
}


// Text edit (string) event filter.
bool qtractorPluginParamWidget::eventFilter (
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
