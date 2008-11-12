// qtractorPluginSelectForm.cpp
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
#include "qtractorPluginSelectForm.h"

#include "qtractorOptions.h"

#include <QHeaderView>
#include <QLineEdit>
#include <QPainter>
#include <QRegExp>
#include <QList>


static qtractorPluginPath     g_pluginPath;
static qtractorPluginTypeList g_pluginTypes;

//----------------------------------------------------------------------------
// qtractorPluginSelectForm -- UI wrapper form.

// Constructor.
qtractorPluginSelectForm::qtractorPluginSelectForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_iChannels = 0;
	m_bMidi = false;

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
#ifdef CONFIG_VST
	m_ui.PluginTypeComboBox->addItem(
		qtractorPluginType::textFromHint(qtractorPluginType::Vst));
#endif

	QHeaderView *pHeader = m_ui.PluginListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
//	pHeader->setDefaultSectionSize(240);
	pHeader->setMovable(false);
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
	m_ui.PluginListView->resizeColumnToContents(4);		// Mode.
	pHeader->resizeSection(5, 120);						// Path.
	m_ui.PluginListView->resizeColumnToContents(6);		// Index
	m_ui.PluginListView->resizeColumnToContents(7);		// Instances

#if QT_VERSION >= 0x040200
	m_ui.PluginListView->setSortingEnabled(true);
#endif
	m_ui.PluginListView->sortItems(0, Qt::AscendingOrder);

	m_ui.PluginTypeProgressBar->hide();

	// Initialize conveniency options...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->loadComboBoxHistory(m_ui.PluginSearchComboBox);
		m_ui.PluginSearchComboBox->setEditText(
			pOptions->sPluginSearch);
		m_ui.PluginTypeComboBox->setCurrentIndex(pOptions->iPluginType);
		m_ui.PluginActivateCheckBox->setChecked(pOptions->bPluginActivate);
	}

	// Let the search begin...
	m_ui.PluginSearchComboBox->setFocus();
	
	typeHintChanged(m_ui.PluginTypeComboBox->currentIndex());

	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.PluginResetToolButton,
		SIGNAL(clicked()),
		SLOT(reset()));
	QObject::connect(m_ui.PluginSearchComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(refresh()));
	QObject::connect(m_ui.PluginTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(typeHintChanged(int)));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemSelectionChanged()),
		SLOT(stabilize()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemActivated(QTreeWidgetItem *, int)),
		SLOT(accept()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		SLOT(accept()));
	QObject::connect(m_ui.PluginActivateCheckBox,
		SIGNAL(clicked()),
		SLOT(stabilize()));
	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(close()));
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
		pOptions->bPluginActivate= m_ui.PluginActivateCheckBox->isChecked();
	}
}


// Base number of channels accessors.
void qtractorPluginSelectForm::setChannels ( unsigned short iChannels, bool bMidi )
{
	m_iChannels = iChannels;
	m_bMidi = bMidi;

	refresh();
}


unsigned short qtractorPluginSelectForm::channels (void) const
{
	return m_iChannels;
}


bool qtractorPluginSelectForm::isMidi (void) const
{
	return m_bMidi;
}


// Final selection accessors..
int qtractorPluginSelectForm::pluginCount (void) const
{
	return m_ui.PluginListView->selectedItems().count();
}

QString qtractorPluginSelectForm::pluginFilename ( int iPlugin ) const
{
	return m_ui.PluginListView->selectedItems().at(iPlugin)->text(5);
}

unsigned long qtractorPluginSelectForm::pluginIndex ( int iPlugin ) const
{
	return m_ui.PluginListView->selectedItems().at(iPlugin)->text(6).toULong();
}

qtractorPluginType::Hint qtractorPluginSelectForm::pluginTypeHint ( int iPlugin ) const
{
	const QString& sText
		= m_ui.PluginListView->selectedItems().at(iPlugin)->text(8);
	return qtractorPluginType::hintFromText(sText);
}

bool qtractorPluginSelectForm::isPluginActivated (void) const
{
	return m_ui.PluginActivateCheckBox->isChecked();
}


// Plugin type hint change slot.
void qtractorPluginSelectForm::typeHintChanged ( int iTypeHint )
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	qtractorPluginType::Hint typeHint
		= qtractorPluginType::hintFromText(
			m_ui.PluginTypeComboBox->itemText(iTypeHint));
	if (g_pluginPath.paths().isEmpty() ||
		g_pluginPath.typeHint() != typeHint) {
		g_pluginPath.setTypeHint(typeHint);
		QStringList paths;
		if (typeHint == qtractorPluginType::Any ||
			typeHint == qtractorPluginType::Vst)
			paths += pOptions->vstPaths;
		if (typeHint == qtractorPluginType::Any ||
			typeHint == qtractorPluginType::Dssi)
			paths += pOptions->dssiPaths;
		if (typeHint == qtractorPluginType::Any ||
			typeHint == qtractorPluginType::Ladspa)
			paths += pOptions->ladspaPaths;
		g_pluginPath.setPaths(paths);
		g_pluginPath.open();
		g_pluginTypes.clear();
	}

	refresh();
}


// Reset plugin listing.
void qtractorPluginSelectForm::reset (void)
{
	m_ui.PluginSearchComboBox->lineEdit()->clear();
//	refresh();
}


// Refresh plugin listing.
void qtractorPluginSelectForm::refresh (void)
{
//	if (m_iChannels == 0)
//		return;

	m_ui.PluginListView->clear();

	// FIXME: Should this be a global (singleton) registry?
	if (g_pluginTypes.isEmpty()) {
		// Tell the world we'll take some time...
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		int iFile = 0;
		qtractorPluginType::Hint typeHint
			= qtractorPluginType::hintFromText(
				m_ui.PluginTypeComboBox->currentText());
		m_ui.PluginTypeProgressBar->setMaximum(g_pluginPath.files().count());
		m_ui.PluginTypeProgressBar->show();
		QListIterator<qtractorPluginFile *> file_iter(g_pluginPath.files());
		while (file_iter.hasNext()) {
			m_ui.PluginTypeProgressBar->setValue(++iFile);
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			qtractorPluginFile *pFile = file_iter.next();
			if (pFile->open()) {
				pFile->getTypes(g_pluginTypes, typeHint);
				pFile->close();
			}
		}
		m_ui.PluginTypeProgressBar->hide();
		// We're formerly done.
		QApplication::restoreOverrideCursor();
	}

	QString sSearch = m_ui.PluginSearchComboBox->currentText().simplified();
	QRegExp rx(sSearch.replace(QRegExp("[\\s]+"), ".*"), Qt::CaseInsensitive);

	QStringList cols;
	QList<QTreeWidgetItem *> items;
	QListIterator<qtractorPluginType *> type_iter(g_pluginTypes.list());
	while (type_iter.hasNext()) {
		qtractorPluginType *pType = type_iter.next();
		const QString& sFilename = (pType->file())->filename();
		const QString& sName = pType->name();
		if (rx.isEmpty()
			|| rx.indexIn(sName) >= 0
			|| rx.indexIn(sFilename) >= 0) {
			// Try primary instantiation...
			int iAudioIns    = pType->audioIns();
			int iAudioOuts   = pType->audioOuts();
			int iMidiIns     = pType->midiIns();
			int iMidiOuts    = pType->midiOuts();
			int iControlIns  = pType->controlIns();
			int iControlOuts = pType->controlOuts();
			// FIXME: We don't care whether it's a MIDI chain at this time...
			unsigned short iInstances = pType->instances(m_iChannels, false);
			cols.clear();
			cols << sName;
			cols << QString("%1:%2").arg(iAudioIns).arg(iAudioOuts);
			cols << QString("%1:%2").arg(iMidiIns).arg(iMidiOuts);
			cols << QString("%1:%2").arg(iControlIns).arg(iControlOuts);
			if (pType->isRealtime())
				cols << tr("RT");
			else
				cols << "-";
			cols << sFilename;
			cols << QString::number(pType->index());
			cols << QString::number(iInstances);
			cols << qtractorPluginType::textFromHint(pType->typeHint());
			QTreeWidgetItem *pItem = new QTreeWidgetItem(cols);
			if (iInstances < 1) {
				pItem->setFlags(pItem->flags() & ~Qt::ItemIsSelectable);
				int iColumnCount = m_ui.PluginListView->columnCount();
				for (int i = 0; i < iColumnCount; ++i) {
					pItem->setTextColor(i,
						m_ui.PluginListView->palette().mid().color());
				}
			}
			pItem->setTextAlignment(1, Qt::AlignHCenter);	// Audio
			pItem->setTextAlignment(2, Qt::AlignHCenter);	// MIDI
			pItem->setTextAlignment(3, Qt::AlignHCenter);	// Controls
			pItem->setTextAlignment(4, Qt::AlignHCenter);	// Mode
			items.append(pItem);
		}
	}

	m_ui.PluginResetToolButton->setEnabled(!rx.isEmpty());
	
	m_ui.PluginListView->addTopLevelItems(items);
//	m_ui.PluginListView->resizeColumnToContents(0);			// Name.

	QHeaderView *pHeader = m_ui.PluginListView->header();
	m_ui.PluginListView->sortItems(
		pHeader->sortIndicatorSection(),
		pHeader->sortIndicatorOrder());

	stabilize();
}


// Stabilize slot.
void qtractorPluginSelectForm::stabilize (void)
{
	m_ui.OkPushButton->setEnabled(
		!m_ui.PluginListView->selectedItems().isEmpty());
}


// Accept slot.
void qtractorPluginSelectForm::accept (void)
{
	// Are we done?
	if (!m_ui.PluginListView->selectedItems().isEmpty())
		QDialog::accept();
}


// end of qtractorPluginSelectForm.cpp
