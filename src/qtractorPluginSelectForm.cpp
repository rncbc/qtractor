// qtractorPluginSelectForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2006, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorPluginSelectForm.h"


#include "qtractorPlugin.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <QHeaderView>
#include <QLineEdit>
#include <QPainter>
#include <QRegExp>
#include <QList>


//----------------------------------------------------------------------------
// qtractorPluginSelectForm -- UI wrapper form.

// Constructor.
qtractorPluginSelectForm::qtractorPluginSelectForm (
	QWidget *pParent, Qt::WFlags wflags ) : QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pPluginPath = new qtractorPluginPath();

	m_iChannels = 0;

	QHeaderView *pHeader = m_ui.PluginListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
//	pHeader->setDefaultSectionSize(240);
	pHeader->setMovable(false);
	pHeader->setStretchLastSection(true);

	QTreeWidgetItem *pHeaderItem = m_ui.PluginListView->headerItem();
	pHeaderItem->setTextAlignment(0, Qt::AlignLeft);	// Name.
	pHeaderItem->setTextAlignment(4, Qt::AlignLeft);	// Filename.
	pHeaderItem->setTextAlignment(5, Qt::AlignLeft);	// Index.
	pHeaderItem->setTextAlignment(6, Qt::AlignLeft);	// Instances.

	pHeader->resizeSection(0, 240);						// Name.
	m_ui.PluginListView->resizeColumnToContents(1);		// Audio.
	m_ui.PluginListView->resizeColumnToContents(2);		// Controls.
	m_ui.PluginListView->resizeColumnToContents(3);		// Mode.
//	pHeader->resizeSection(4, 240);						// Path.
	m_ui.PluginListView->resizeColumnToContents(5);		// Index

	// Initialize conveniency options...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			pOptions->loadComboBoxHistory(m_ui.PluginSearchComboBox);
			m_ui.PluginSearchComboBox->lineEdit()->setText(
				pOptions->sPluginSearch);
		}
	}

	// Let the search begin...
	m_ui.PluginSearchComboBox->setFocus();
	
	m_pPluginPath->open();

	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.PluginResetToolButton,
		SIGNAL(clicked()),
		SLOT(reset()));
	QObject::connect(m_ui.PluginSearchComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(refresh()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemSelectionChanged()),
		SLOT(stabilize()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemActivated(QTreeWidgetItem *, int)),
		SLOT(accept()));
	QObject::connect(m_ui.PluginListView,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		SLOT(accept()));
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			pOptions->sPluginSearch = m_ui.PluginSearchComboBox->currentText();
			pOptions->saveComboBoxHistory(m_ui.PluginSearchComboBox);
		}
	}

	delete m_pPluginPath;
}


// Base number of channels accessors.
void qtractorPluginSelectForm::setChannels ( unsigned short iChannels )
{
	m_iChannels = iChannels;
	refresh();
}


unsigned short qtractorPluginSelectForm::channels (void) const
{
	return m_iChannels;
}


// Final selection accessors..
int qtractorPluginSelectForm::pluginCount (void) const
{
	return m_ui.PluginListView->selectedItems().count();
}

QString qtractorPluginSelectForm::pluginFilename ( int iPlugin ) const
{
	return m_ui.PluginListView->selectedItems().at(iPlugin)->text(4);
}

unsigned long qtractorPluginSelectForm::pluginIndex ( int iPlugin ) const
{
	return m_ui.PluginListView->selectedItems().at(iPlugin)->text(5).toULong();
}


// Reset plugin listing.
void qtractorPluginSelectForm::reset (void)
{
	m_ui.PluginSearchComboBox->lineEdit()->setText(QString::null);
//	refresh();
}


// Refresh plugin listing.
void qtractorPluginSelectForm::refresh (void)
{
	if (m_pPluginPath == NULL)
		return;

	QString sSearch = m_ui.PluginSearchComboBox->currentText().simplified();
	QRegExp rx(sSearch.replace(QRegExp("[\\s]+"), ".*"), Qt::CaseInsensitive);

	m_ui.PluginListView->setUpdatesEnabled(false);

	m_ui.PluginListView->clear();
	QStringList cols;
	QList<QTreeWidgetItem *> items;
	QListIterator<qtractorPluginFile *> file_iter(m_pPluginPath->files());
	while (file_iter.hasNext()) {
		qtractorPluginFile *pFile = file_iter.next();
		if (pFile->open()) {
			QListIterator<qtractorPluginType *> type_iter(pFile->types());
			while (type_iter.hasNext()) {
				qtractorPluginType *pType = type_iter.next();
				const LADSPA_Descriptor *pDescriptor = pType->descriptor();
				const QString& sFilename = pType->filename();
				if (rx.isEmpty()
					|| rx.indexIn(pDescriptor->Name) >= 0
					|| rx.indexIn(sFilename) >= 0) {
					int iAudioIns    = 0;
					int iAudioOuts   = 0;
					int iControlIns  = 0;
					int iControlOuts = 0;
					for (unsigned long i = 0; i < pDescriptor->PortCount; i++) {
						const LADSPA_PortDescriptor portType
							= pDescriptor->PortDescriptors[i];
						if (LADSPA_IS_PORT_INPUT(portType)) {
							if (LADSPA_IS_PORT_AUDIO(portType))
								iAudioIns++;
							else
							if (LADSPA_IS_PORT_CONTROL(portType))
								iControlIns++;
						}
						else
						if (LADSPA_IS_PORT_OUTPUT(portType)) {
							if (LADSPA_IS_PORT_AUDIO(portType))
								iAudioOuts++;
							else
							if (LADSPA_IS_PORT_CONTROL(portType))
								iControlOuts++;
						}
					}
					unsigned short iInstances = 0;
					if (iAudioIns == m_iChannels && iAudioOuts == m_iChannels)
						iInstances = 1;
					else if ((iAudioIns < 2 || iAudioIns == m_iChannels)
						&& iAudioOuts < 2)
						iInstances = m_iChannels;
					cols.clear();
					cols << pDescriptor->Name;
					cols << QString("%1:%2").arg(iAudioIns).arg(iAudioOuts);
					cols << QString("%1:%2").arg(iControlIns).arg(iControlOuts);
					if (LADSPA_IS_HARD_RT_CAPABLE(pDescriptor->Properties))
						cols << tr("RT");
					else
						cols << "-";
					cols << sFilename;
					cols << QString::number(pType->index());
					cols << QString::number(iInstances);
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
					pItem->setTextAlignment(2, Qt::AlignHCenter);	// Controls
					pItem->setTextAlignment(3, Qt::AlignHCenter);	// Mode
					items.append(pItem);
				}
			}
			pFile->close();
		}
	}

	m_ui.PluginListView->addTopLevelItems(items);
//	m_ui.PluginListView->resizeColumnToContents(0);					// Name.
	m_ui.PluginListView->setUpdatesEnabled(true);

	m_ui.PluginResetToolButton->setEnabled(!rx.isEmpty());
	
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
