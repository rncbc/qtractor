// qtractorPluginForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"

#include "qtractorObserverWidget.h"

#include "qtractorMidiControlObserverForm.h"

#include "qtractorMainForm.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"
#include "qtractorEngine.h"

#include <QMessageBox>
#include <QGridLayout>
#include <QValidator>
#include <QLineEdit>
#include <QLabel>

#include <QFileInfo>
#include <QFileDialog>
#include <QUrl>

#include <QKeyEvent>

#include <math.h>

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
	m_pGridLayout = NULL;
	m_iDirtyCount = 0;
	m_iUpdate     = 0;

    m_ui.PresetComboBox->setValidator(
		new QRegExpValidator(QRegExp("[\\w-]+"), m_ui.PresetComboBox));
	m_ui.PresetComboBox->setInsertPolicy(QComboBox::NoInsert);
#if QT_VERSION >= 0x040200
	m_ui.PresetComboBox->setCompleter(NULL);
#endif

	// Have some effective feedback when toggling on/off...
	QIcon iconParams;
	iconParams.addPixmap(
		QPixmap(":/images/formParamsOff.png"), QIcon::Active, QIcon::Off);
	iconParams.addPixmap(
		QPixmap(":/images/formParamsOn.png"), QIcon::Active, QIcon::On);
	m_ui.ParamsToolButton->setIcon(iconParams);

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
	QObject::connect(m_ui.ParamsToolButton,
		SIGNAL(toggled(bool)),
		SLOT(paramsSlot(bool)));
	QObject::connect(m_ui.EditToolButton,
		SIGNAL(toggled(bool)),
		SLOT(editSlot(bool)));
	QObject::connect(m_ui.SendsToolButton,
		SIGNAL(clicked()),
		SLOT(sendsSlot()));
	QObject::connect(m_ui.ReturnsToolButton,
		SIGNAL(clicked()),
		SLOT(returnsSlot()));
	QObject::connect(m_ui.ActivateToolButton,
		SIGNAL(toggled(bool)),
		SLOT(activateSlot(bool)));
}


// Destructor.
qtractorPluginForm::~qtractorPluginForm (void)
{
	clear();
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	clear();

	// Just in those cases where
	// we've been around before...
	if (m_pGridLayout) {
		delete m_pGridLayout;
		m_pGridLayout = NULL;
	}

	// Set the new reference...
	m_pPlugin = pPlugin;

	if (m_pPlugin == NULL)
		return;

	// Dispatch any pending updates.
	qtractorSubject::flushQueue();

	// Plugin might not have its own editor...
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(4);
	m_pGridLayout->setSpacing(4);
#if QT_VERSION >= 0x040300
	m_pGridLayout->setHorizontalSpacing(16);
#endif
	const QList<qtractorPluginParam *>& params = m_pPlugin->params();
	int iRows = params.count();
	bool bEditor = (m_pPlugin->type())->isEditor();
	// FIXME: Couldn't stand more than a hundred widgets?
	// or do we have one dedicated editor GUI?
	if (!bEditor || iRows < 101) {
		int iCols = 1;
		while (iRows > 12 && iCols < 4)
			iRows = (params.count() / ++iCols);
		if (params.count() % iCols)	// Adjust to balance.
			iRows++;
		int iRow = 0;
		int iCol = 0;
		QListIterator<qtractorPluginParam *> iter(params);
		while (iter.hasNext()) {
			qtractorPluginParam *pParam = iter.next();
			qtractorPluginParamWidget *pParamWidget
				= new qtractorPluginParamWidget(pParam, this);
			m_paramWidgets.insert(pParam->index(), pParamWidget);
			m_pGridLayout->addWidget(pParamWidget, iRow, iCol);
			addMidiControlAction(pParamWidget, pParam->observer());
			if (++iRow >= iRows) {
				iRow = 0;
				iCol++;
			}
		}
	}
	m_ui.ParamsGridWidget->setLayout(m_pGridLayout);

	// Show editor button if available?
	m_ui.EditToolButton->setVisible(bEditor);
	//if (bEditor)
	//	toggleEditor(m_pPlugin->isEditorVisible());

	// Show insert tool options...
	bool bInsertPlugin
		= ((m_pPlugin->type())->typeHint() == qtractorPluginType::Insert);
	m_ui.SendsToolButton->setVisible(bInsertPlugin);
	m_ui.ReturnsToolButton->setVisible(bInsertPlugin);

	// Set initial plugin preset name...
	setPreset(m_pPlugin->preset());
	
	// Set plugin name as title...
	updateCaption();

	// This should trigger paramsSlot(!bEditor)
	// and adjust the size of the params dialog...
	bool bParams = (iRows > 0 && iRows < 101);
	m_ui.ParamsToolButton->setVisible(bParams);
	m_ui.ParamsToolButton->setChecked(bParams);
	paramsSlot(bParams);

	// Clear any initial param update.
	qtractorSubject::resetQueue();

	updateActivated();
	refresh();
	stabilize();
	show();

	// Do we have a dedicated editor GUI?
	if (!bParams && bEditor)
		m_pPlugin->openEditor(this);
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

	m_iUpdate++;

	// Set the toggle button anyway...
	m_ui.EditToolButton->setChecked(bOn);

	m_iUpdate--;
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

	m_iUpdate++;
	m_ui.PresetComboBox->setEditText(sEditText);
	m_iUpdate--;
}

QString qtractorPluginForm::preset (void) const
{
	QString sPreset = m_ui.PresetComboBox->currentText();

	if (sPreset == g_sDefPreset || m_iDirtyCount > 0)
		sPreset.clear();

	return sPreset;
}


// Update form caption.
void qtractorPluginForm::updateCaption (void)
{
	if (m_pPlugin == NULL)
		return;

	QString sCaption = (m_pPlugin->type())->name();

	qtractorPluginList *pPluginList = m_pPlugin->list();
	if (pPluginList && !pPluginList->name().isEmpty())
		sCaption += " - " + pPluginList->name();

	setWindowTitle(sCaption);

	m_pPlugin->setEditorTitle(sCaption);
}


// Update activation state.
void qtractorPluginForm::updateActivated (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;
	m_ui.ActivateToolButton->setChecked(m_pPlugin->isActivated());
	m_iUpdate--;
}


// Update parameter value state.
void qtractorPluginForm::updateParamValue (
	unsigned long iIndex, float fValue, bool bUpdate )
{
	if (m_pPlugin == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::updateParamValue(%lu, %g, %d)",
		this, iIndex, fValue, int(bUpdate));
#endif

	qtractorPluginParam *pParam = m_pPlugin->findParam(iIndex);
	if (pParam)
		pParam->updateValue(fValue, bUpdate);
}


// Update port widget state.
void qtractorPluginForm::updateParamWidget ( unsigned long iIndex )
{
	if (m_pPlugin == NULL)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm[%p]::updateParamWidget(%lu)", this, iIndex);
#endif

	qtractorPluginParamWidget *pParamWidget
		= m_paramWidgets.value(iIndex, NULL);
	if (pParamWidget)
		pParamWidget->refresh();

	// Sure is dirty...
	m_iDirtyCount++;
	stabilize();
}


// Preset management slots...
void qtractorPluginForm::changePresetSlot ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty() && m_ui.PresetComboBox->findText(sPreset) >= 0)
		m_iDirtyCount++;

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
	} else {
		// An existing preset is about to be loaded...
		QSettings& settings = pOptions->settings();
		// Should it be load from known file?...
		if ((m_pPlugin->type())->isConfigure()) {
			settings.beginGroup(m_pPlugin->presetGroup());
			m_pPlugin->loadPreset(settings.value(sPreset).toString());
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
#if QT_VERSION < 0x040400
	// Ask for the filename to save...
	sFilename = QFileDialog::getOpenFileName(this,
		sTitle, pOptions->sPresetDir, sFilter);
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
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif
	// Have we a filename to load a preset from?
	if (!sFilename.isEmpty()) {
		if (m_pPlugin->loadPreset(sFilename)) {
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
	// this is where we'll make it...
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
		#if QT_VERSION < 0x040400
			// Ask for the filename to save...
			sFilename = QFileDialog::getSaveFileName(this,
				sTitle, sFilename, sFilter);
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
			if (m_pPlugin->savePreset(sFilename)) {
				settings.setValue(sPreset, sFilename);
				pOptions->sPresetDir = QFileInfo(sFilename).absolutePath();
			}
		}
	}	// Just leave it to simple parameter value list...
	else settings.setValue(sPreset, m_pPlugin->valueList());
	settings.endGroup();
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
	refresh();

	stabilize();
}


// Params slot.
void qtractorPluginForm::paramsSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	m_ui.ParamsGridWidget->setVisible(bOn);
	if (bOn)
		m_ui.ParamsGridWidget->show();
	else
		m_ui.ParamsGridWidget->hide();

	// Shake it a little bit first, but
	// make it as tight as possible...
	resize(width() + 1, height() + 1);
	adjustSize();

	m_iUpdate--;
}


// Editor slot.
void qtractorPluginForm::editSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	if (bOn)
		m_pPlugin->openEditor(this);
	else
		m_pPlugin->closeEditor();

	m_iUpdate--;
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


// Activation slot.
void qtractorPluginForm::activateSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		pSession->execute(
			new qtractorActivatePluginCommand(m_pPlugin, bOn));

	m_iUpdate--;
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
	m_iUpdate++;

	qtractorSubject::flushQueue();

	const QString sOldPreset = m_ui.PresetComboBox->currentText();
	m_ui.PresetComboBox->clear();
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->settings().beginGroup(m_pPlugin->presetGroup());
		m_ui.PresetComboBox->insertItems(0,
			pOptions->settings().childKeys());
		pOptions->settings().endGroup();
	}
	m_ui.PresetComboBox->addItem(g_sDefPreset);
	m_ui.PresetComboBox->setEditText(sOldPreset);

	ParamWidgets::ConstIterator iter = m_paramWidgets.constBegin();
	for ( ; iter != m_paramWidgets.constEnd(); ++iter)
		iter.value()->refresh();

	m_pPlugin->idleEditor();

	qtractorSubject::resetQueue();

	m_iDirtyCount = 0;
	m_iUpdate--;
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
		bEnabled = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	    bExists  = (m_ui.PresetComboBox->findText(sPreset) >= 0);
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
}


// Keyboard event handler.
void qtractorPluginForm::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorPluginForm::keyPressEvent(%d)", pKeyEvent->key());
#endif
	int iKey = pKeyEvent->key();
	switch (iKey) {
	case Qt::Key_Escape:
		close();
		break;
	default:
		QWidget::keyPressEvent(pKeyEvent);
		break;
	}

	// Make sure we've get focus back...
	QWidget::setFocus();
}


// MIDI controller/observer attachement (context menu)
//
Q_DECLARE_METATYPE(qtractorMidiControlObserver *);

void qtractorPluginForm::addMidiControlAction (
	QWidget *pWidget, qtractorMidiControlObserver *pMidiObserver )
{
	QAction *pAction = new QAction(
		QIcon(":/images/itemControllers.png"),
		tr("MIDI Controller..."), pWidget);

	pAction->setData(
		qVariantFromValue<qtractorMidiControlObserver *> (pMidiObserver));

	QObject::connect(pAction,
		SIGNAL(triggered(bool)),
		SLOT(midiControlActionSlot()));

	pWidget->addAction(pAction);
	pWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
}


void qtractorPluginForm::midiControlActionSlot (void)
{
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction) {
		qtractorMidiControlObserver *pMidiObserver
			= qVariantValue<qtractorMidiControlObserver *> (pAction->data());
		if (pMidiObserver) {
			qtractorMidiControlObserverForm form(this);
			form.setMidiObserver(pMidiObserver);
			if (form.exec()) {
				// TODO; Make it an undoable command?
				qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
				if (pMainForm)
					pMainForm->contentsChanged();
			}
		}
	}
}


//----------------------------------------------------------------------
// class qtractorPluginParamSliderInterface -- Observer interface.
//

// Local converter interface.
class qtractorPluginParamSliderInterface
	: public qtractorObserverSlider::Interface
{
public:

	// Constructor.
	qtractorPluginParamSliderInterface (
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
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(0);
	m_pGridLayout->setSpacing(4);

	m_pLabel    = NULL;
	m_pDisplay  = NULL;

	m_pSlider   = NULL;
	m_pSpinBox  = NULL;
	m_pCheckBox = NULL;

	if (m_pParam->isToggled()) {
		m_pCheckBox = new qtractorObserverCheckBox(/*this*/);
		m_pCheckBox->setText(m_pParam->name());
		m_pCheckBox->setSubject(m_pParam->subject());
	//	m_pCheckBox->setChecked(m_pParam->value() > 0.1f);
		m_pGridLayout->addWidget(m_pCheckBox, 0, 0);
	} else if (m_pParam->isInteger()) {
		m_pGridLayout->setColumnMinimumWidth(0, 120);
		m_pLabel = new QLabel(/*this*/);
		m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_pLabel->setText(m_pParam->name() + ':');
		if (m_pParam->isDisplay()) {
			m_pGridLayout->addWidget(m_pLabel, 0, 0);
		} else {
			m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 2);
		}
		m_pSpinBox = new qtractorObserverSpinBox(/*this*/);
		m_pSpinBox->setMaximumWidth(64);
		m_pSpinBox->setDecimals(0);
		m_pSpinBox->setMinimum(m_pParam->minValue());
		m_pSpinBox->setMaximum(m_pParam->maxValue());
		m_pSpinBox->setAlignment(Qt::AlignHCenter);
		m_pSpinBox->setSubject(m_pParam->subject());
	//	m_pSpinBox->setValue(int(m_pParam->value()));
		if (m_pParam->isDisplay()) {
			m_pGridLayout->addWidget(m_pSpinBox, 0, 1);
			m_pDisplay = new QLabel(/*this*/);
			m_pDisplay->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
			m_pDisplay->setText(m_pParam->display());
		//	m_pDisplay->setFixedWidth(72);
			m_pDisplay->setMinimumWidth(64);
			m_pGridLayout->addWidget(m_pDisplay, 0, 2);
		} else {
			m_pGridLayout->addWidget(m_pSpinBox, 0, 2);
		}
	} else {
		m_pLabel = new QLabel(/*this*/);
		if (m_pParam->isDisplay()) {
			m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		//	m_pLabel->setFixedWidth(72);
			m_pLabel->setMinimumWidth(64);
			m_pLabel->setText(m_pParam->name());
			m_pGridLayout->addWidget(m_pLabel, 0, 0);
		} else {
			m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			m_pLabel->setText(m_pParam->name() + ':');
			m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 3);
		}
		m_pSlider = new qtractorObserverSlider(/*this*/);
		m_pSlider->setInterface(
			new qtractorPluginParamSliderInterface(m_pSlider, m_pParam));
		m_pSlider->setOrientation(Qt::Horizontal);
		m_pSlider->setTickPosition(QSlider::NoTicks);
		m_pSlider->setMinimumWidth(120);
		m_pSlider->setMinimum(0);
		m_pSlider->setMaximum(10000);
		m_pSlider->setPageStep(1000);
		m_pSlider->setSingleStep(100);
		m_pSlider->setDefault(m_pSlider->scaleFromValue(m_pParam->defaultValue()));
		m_pSlider->setSubject(m_pParam->subject());
	//	m_pSlider->setValue(m_pSlider->scaleFromValue(m_pParam->value()));
		if (m_pParam->isDisplay()) {
			m_pGridLayout->addWidget(m_pSlider, 0, 1);
			m_pDisplay = new QLabel(/*this*/);
			m_pDisplay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			m_pDisplay->setText(m_pParam->display());
		//	m_pDisplay->setFixedWidth(72);
			m_pDisplay->setMinimumWidth(64);
			m_pGridLayout->addWidget(m_pDisplay, 0, 2);
		} else {
			m_pGridLayout->addWidget(m_pSlider, 1, 0, 1, 2);
			int iDecimals = paramDecimals();
			m_pSpinBox = new qtractorObserverSpinBox(/*this*/);
			m_pSpinBox->setMaximumWidth(64);
			m_pSpinBox->setDecimals(iDecimals);
			m_pSpinBox->setMinimum(m_pParam->minValue());
			m_pSpinBox->setMaximum(m_pParam->maxValue());
			m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecimals)));
		#if QT_VERSION >= 0x040200
			m_pSpinBox->setAccelerated(true);
		#endif
			m_pSpinBox->setSubject(m_pParam->subject());
		//	m_pSpinBox->setValue(m_pParam->value());
			m_pGridLayout->addWidget(m_pSpinBox, 1, 2);
		}
	}

	QFrame::setLayout(m_pGridLayout);
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
		m_pCheckBox->observer()->update();
	if (m_pSpinBox)
		m_pSpinBox->observer()->update();
	if (m_pSlider)
		m_pSlider->observer()->update();

	if (m_pDisplay)
		m_pDisplay->setText(m_pParam->display());
}


// Spin-box decimals helper.
int qtractorPluginParamWidget::paramDecimals (void) const
{
	int iDecimals = 0;

	float fDecimals = ::log10f(m_pParam->maxValue() - m_pParam->minValue());
	if (fDecimals < -3.0f)
		iDecimals = 6;
	else if (fDecimals < 0.0f)
		iDecimals = 3;
	else if (fDecimals < 1.0f)
		iDecimals = 2;
	else if (fDecimals < 6.0f)
		iDecimals = 1;

	if (m_pParam->isLogarithmic())
		++iDecimals;

	return iDecimals;
}


// end of qtractorPluginForm.cpp
