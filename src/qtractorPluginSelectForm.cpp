// qtractorPluginSelectForm.cpp
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
#include "qtractorPluginSelectForm.h"
#include "qtractorPluginFactory.h"

#include "qtractorOptions.h"

#include <QApplication>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QPainter>
#include <QList>

#include <QRegularExpression>


//----------------------------------------------------------------------------
// qtractorPluginSelectForm -- UI wrapper form.

// Constructor.
qtractorPluginSelectForm::qtractorPluginSelectForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	m_pPluginList = nullptr;

	// Populate plugin type hints...
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Any));
#ifdef CONFIG_LADSPA
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Ladspa));
#endif
#ifdef CONFIG_DSSI
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Dssi));
#endif
#ifdef CONFIG_VST2
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Vst2));
#endif
#ifdef CONFIG_VST3
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Vst3));
#endif
#ifdef CONFIG_CLAP
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Clap));
#endif
#ifdef CONFIG_LV2
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Lv2));
#endif

	QHeaderView *pHeader = m_ui.PluginListView->header();
//	pHeader->setDefaultSectionSize(240);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setMovable(false);
#endif
	pHeader->setStretchLastSection(true);

	QTreeWidgetItem *pHeaderItem = m_ui.PluginListView->headerItem();
	pHeaderItem->setTextAlignment(0, Qt::AlignLeft);	// Name.
	pHeaderItem->setTextAlignment(5, Qt::AlignLeft);	// Filename.
	pHeaderItem->setTextAlignment(6, Qt::AlignLeft);	// Index.
	pHeaderItem->setTextAlignment(7, Qt::AlignLeft);	// Instances.
	pHeaderItem->setTextAlignment(8, Qt::AlignLeft);	// Type.

	pHeader->resizeSection(0, 240);						// Name.
	m_ui.PluginListView->resizeColumnToContents(1);		// Audio.
	m_ui.PluginListView->resizeColumnToContents(2);		// MIDI.
	m_ui.PluginListView->resizeColumnToContents(3);		// Controls.
	m_ui.PluginListView->resizeColumnToContents(4);		// Modes.
	pHeader->resizeSection(5, 120);						// Path.
	m_ui.PluginListView->resizeColumnToContents(6);		// Index
	m_ui.PluginListView->resizeColumnToContents(7);		// Instances

	m_ui.PluginListView->setSortingEnabled(true);
	m_ui.PluginListView->sortItems(0, Qt::AscendingOrder);

	m_ui.PluginScanProgressBar->setMaximum(100);
	m_ui.PluginScanProgressBar->hide();

	// Initialize conveniency options...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->loadComboBoxHistory(m_ui.PluginSearchComboBox);
		m_ui.PluginSearchComboBox->setEditText(
			pOptions->sPluginSearch);
		m_ui.PluginTypeComboBox->setCurrentIndex(pOptions->iPluginType);
	}

	// Let the search begin...
	m_ui.PluginSearchComboBox->setFocus();
	
	adjustSize();

	// Restore last seen dialog position and extent...
	if (pOptions)
		pOptions->loadWidgetGeometry(this, true);

	// UI signal/slot connections...
#if QT_VERSION < QT_VERSION_CHECK(5, 2, 0)
	QObject::connect(m_ui.PluginResetToolButton,
		SIGNAL(clicked()),
		SLOT(reset()));
#else
	m_ui.PluginResetToolButton->hide();
	// Some conveniency cleaner helper...
	m_ui.PluginSearchComboBox->lineEdit()->setClearButtonEnabled(true);
#endif
	QObject::connect(m_ui.PluginSearchComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(refresh()));
	QObject::connect(m_ui.PluginTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(typeHintChanged(int)));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemSelectionChanged()),
		SLOT(stabilize()));
//	QObject::connect(m_ui.PluginListView,
//		SIGNAL(itemActivated(QTreeWidgetItem *, int)),
//		SLOT(accept()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		SLOT(accept()));
	QObject::connect(m_ui.PluginRescanPushButton,
		SIGNAL(clicked()),
		SLOT(rescan()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));

	qtractorPluginFactory *pPluginFactory
		= qtractorPluginFactory::getInstance();
	if (pPluginFactory) {
		QObject::connect(pPluginFactory,
			SIGNAL(scanned(int)),
			SLOT(scanned(int)));
	}

	typeHintChanged(m_ui.PluginTypeComboBox->currentIndex());
}


// Destructor.
qtractorPluginSelectForm::~qtractorPluginSelectForm (void)
{
	// Save other conveniency options, if convenient thought...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->iPluginType = m_ui.PluginTypeComboBox->currentIndex();
		pOptions->sPluginSearch = m_ui.PluginSearchComboBox->currentText();
		pOptions->saveComboBoxHistory(m_ui.PluginSearchComboBox);
		// Save aslast seen dialog position and extent...
		pOptions->saveWidgetGeometry(this, true);
	}
}


// Base number of channels accessors.
void qtractorPluginSelectForm::setPluginList ( qtractorPluginList *pPluginList )
{
	m_pPluginList = pPluginList;

	refresh();
}

qtractorPluginList *qtractorPluginSelectForm::pluginList (void) const
{
	return m_pPluginList;
}


// Final selection accessors..
int qtractorPluginSelectForm::pluginCount (void) const
{
	return m_selectedItems.count();
}

QString qtractorPluginSelectForm::pluginFilename ( int iPlugin ) const
{
	return m_selectedItems.at(iPlugin)->text(5);
}

unsigned long qtractorPluginSelectForm::pluginIndex ( int iPlugin ) const
{
	return m_selectedItems.at(iPlugin)->text(6).toULong();
}

qtractorPluginType::Hint qtractorPluginSelectForm::pluginTypeHint ( int iPlugin ) const
{
	const QString& sText = m_selectedItems.at(iPlugin)->text(8);
	return qtractorPluginType::hintFromText(sText);
}


// Plugin type hint change slot.
void qtractorPluginSelectForm::typeHintChanged ( int iTypeHint )
{
	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->itemText(iTypeHint));
#if 0
	// Rescan not applicable to LV2 plug-in world.
	m_ui.PluginRescanPushButton->setVisible(
		typeHint != qtractorPluginType::Lv2);
#endif
	qtractorPluginFactory *pPluginFactory
		= qtractorPluginFactory::getInstance();
	if (pPluginFactory && pPluginFactory->typeHint() != typeHint) {
		pPluginFactory->setTypeHint(typeHint);
		pPluginFactory->clear();
	}

	refresh();
}


// Reset plugin listing.
void qtractorPluginSelectForm::reset (void)
{
	m_ui.PluginSearchComboBox->lineEdit()->clear();
//	refresh();
}


// Rescan plugin listing.
void qtractorPluginSelectForm::rescan (void)
{
	qtractorPluginFactory *pPluginFactory
		= qtractorPluginFactory::getInstance();
	if (pPluginFactory) {
		const int iTypeHint
			= m_ui.PluginTypeComboBox->currentIndex();
		qtractorPluginType::Hint typeHint
			= qtractorPluginType::hintFromText(
				m_ui.PluginTypeComboBox->itemText(iTypeHint));
		if (QApplication::keyboardModifiers()
			& (Qt::ShiftModifier | Qt::ControlModifier))
			pPluginFactory->clearAll(typeHint);
		else
			pPluginFactory->clear();
		pPluginFactory->setRescan(true);
	}

	refresh();
}


// Refresh plugin listing.
void qtractorPluginSelectForm::refresh (void)
{
	m_ui.PluginListView->clear();

	qtractorPluginFactory *pPluginFactory
		= qtractorPluginFactory::getInstance();
	if (pPluginFactory == nullptr)
		return;

	// FIXME: Should this be a global (singleton) registry?
	if (pPluginFactory->types().isEmpty()) {
		// Tell the world we'll take some time...
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		m_ui.PluginRescanPushButton->setEnabled(false);
		m_ui.DialogButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
		m_ui.PluginScanProgressBar->show();
		pPluginFactory->scan();
		m_ui.PluginScanProgressBar->hide();
		m_ui.DialogButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
		m_ui.PluginRescanPushButton->setEnabled(true);
		// We're formerly done.
		QApplication::restoreOverrideCursor();
	}

	if (m_pPluginList == nullptr) {
		stabilize();
		return;
	}

	const unsigned short iChannels = m_pPluginList->channels();
	const bool bMidi = m_pPluginList->isMidi();

	QString sSearch = m_ui.PluginSearchComboBox->currentText().simplified();
	const QRegularExpression rx(sSearch.replace(
		QRegularExpression("[\\s]+"), ".*"),
		QRegularExpression::CaseInsensitiveOption);

	QStringList cols;
	QList<QTreeWidgetItem *> items;
	QListIterator<qtractorPluginType *> type_iter(pPluginFactory->types());
	while (type_iter.hasNext()) {
		qtractorPluginType *pType = type_iter.next();
		const QString& sFilename = pType->filename();
		const QString& sName = pType->name();
		if (rx.pattern().isEmpty()
			|| rx.match(sName).hasMatch()
			|| rx.match(sFilename).hasMatch()) {
			// Try primary instantiation...
			const int iAudioIns    = pType->audioIns();
			const int iAudioOuts   = pType->audioOuts();
			const int iMidiIns     = pType->midiIns();
			const int iMidiOuts    = pType->midiOuts();
			const int iControlIns  = pType->controlIns();
			const int iControlOuts = pType->controlOuts();
			// All that to check whether it will get properly instantiated.
			const unsigned short iInstances
				= pType->instances(iChannels, bMidi);
			cols.clear();
			cols << sName;
			cols << QString("%1:%2").arg(iAudioIns).arg(iAudioOuts);
			cols << QString("%1:%2").arg(iMidiIns).arg(iMidiOuts);
			cols << QString("%1:%2").arg(iControlIns).arg(iControlOuts);
			QStringList modes;
			if (pType->isEditor())
				modes << tr("GUI");
			if (pType->isConfigure())
				modes << tr("EXT");
			if (pType->isRealtime())
				modes << tr("RT");
			if (modes.isEmpty())
				cols << "-";
			else
				cols << modes.join(",");
			cols << sFilename;
			cols << QString::number(pType->index());
			cols << QString::number(iInstances);
			cols << qtractorPluginType::textFromHint(pType->typeHint());
			QTreeWidgetItem *pItem = new QTreeWidgetItem(cols);
			if (iInstances < 1) {
				pItem->setFlags(pItem->flags() & ~Qt::ItemIsSelectable);
				const int iColumnCount = m_ui.PluginListView->columnCount();
				const QPalette& pal = m_ui.PluginListView->palette();
				const QColor& rgbForeground
					= pal.color(QPalette::Disabled, QPalette::WindowText);
				for (int i = 0; i < iColumnCount; ++i)
					pItem->setForeground(i, rgbForeground);
			}
			pItem->setTextAlignment(1, Qt::AlignHCenter);	// Audio
			pItem->setTextAlignment(2, Qt::AlignHCenter);	// MIDI
			pItem->setTextAlignment(3, Qt::AlignHCenter);	// Controls
			pItem->setTextAlignment(4, Qt::AlignHCenter);	// Modes
			items.append(pItem);
		}
	}
	m_ui.PluginListView->addTopLevelItems(items);

	QHeaderView *pHeader = m_ui.PluginListView->header();
	m_ui.PluginListView->sortItems(
		pHeader->sortIndicatorSection(),
		pHeader->sortIndicatorOrder());

	m_ui.PluginResetToolButton->setEnabled(!rx.pattern().isEmpty());

	stabilize();
}


// Refresh plugin scan progress.
void qtractorPluginSelectForm::scanned ( int iPercent )
{
	m_ui.PluginScanProgressBar->setValue(iPercent);
}


// Stabilize slot.
void qtractorPluginSelectForm::stabilize (void)
{
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(
		!m_ui.PluginListView->selectedItems().isEmpty());
}


// Accept slot.
void qtractorPluginSelectForm::accept (void)
{
	// Are we done?
	m_selectedItems = m_ui.PluginListView->selectedItems();
	if (!m_selectedItems.isEmpty())
		QDialog::accept();
}


// end of qtractorPluginSelectForm.cpp
