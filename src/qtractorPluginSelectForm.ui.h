// qtractorPluginSelectForm.ui.h
//
// ui.h extension file, included from the uic-generated form implementation.
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

#include "qtractorPlugin.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <qptrlist.h>
#include <qregexp.h>

// Plugin selection item.
struct qtractorPluginSelectItem
{
	// Constructor.
	qtractorPluginSelectItem(QListViewItem *pItem)
		: filename(pItem->text(4)), index(pItem->text(5).toULong()) {}
	// Selected item relevant members.
	QString filename;
	unsigned long index;
};

// Plugin selection list.
class qtractorPluginSelectList : public QPtrList<qtractorPluginSelectItem> {};


// Plugin selectable list-view item.
class qtractorPluginSelectListItem : public QListViewItem
{
public:

	// Constructors.
	qtractorPluginSelectListItem(QListView *pListView,
		QListViewItem *pItemAfter) : QListViewItem(pListView, pItemAfter) {}

	// Overrriden virtual cell painter.
	void paintCell(QPainter *p, const QColorGroup& cg,
		int column, int width, int align)
	{
		// Paint the original but maybe with a different color...
		QColorGroup _cg(cg);
		if (!QListViewItem::isSelectable())
			_cg.setColor(QColorGroup::Text, _cg.color(QColorGroup::Mid));
		// Do as usual...
		QListViewItem::paintCell(p, _cg, column, width, align);
	}
};


// Kind of constructor.
void qtractorPluginSelectForm::init (void)
{
	m_pPluginPath = new qtractorPluginPath();
	m_pSelectList = new qtractorPluginSelectList();
	m_pSelectList->setAutoDelete(true);

	m_iChannels = 0;

	PluginListView->setColumnWidthMode(1, QListView::Manual);	// Audio
	PluginListView->setColumnWidth(1, 48);
	PluginListView->setColumnAlignment(1, Qt::AlignHCenter);

	PluginListView->setColumnWidthMode(2, QListView::Manual);	// Control
	PluginListView->setColumnWidth(2, 48);
	PluginListView->setColumnAlignment(2, Qt::AlignHCenter);

	PluginListView->setColumnWidthMode(3, QListView::Manual);	// Mode
	PluginListView->setColumnWidth(3, 32);
	PluginListView->setColumnAlignment(3, Qt::AlignHCenter);

//	PluginListView->setSelectionMode(QListView::Extended);
	PluginListView->setSortColumn(0 /*PluginListView->columns() + 1*/);
//	PluginListView->setAllColumnsShowFocus(true);
//	PluginListView->setShowSortIndicator(true);

	// Initialize conveniency options...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			pOptions->loadComboBoxHistory(PluginSearchComboBox);
			PluginSearchComboBox->setCurrentText(pOptions->sPluginSearch);
		}
	}

	// Let the search begin...
	PluginSearchComboBox->setFocus();
	
	m_pPluginPath->open();

	adjustSize();
}


// Kind of destructor.
void qtractorPluginSelectForm::destroy (void)
{
	// Save other conveniency options, if convenient thought...
	if (m_pSelectList->count() > 0) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorOptions *pOptions = pMainForm->options();
			if (pOptions) {
				pOptions->sPluginSearch = PluginSearchComboBox->currentText();
				pOptions->saveComboBoxHistory(PluginSearchComboBox);
			}
		}
	}
	// Ok, can go on with clean up...
	m_pSelectList->clear();

	delete m_pSelectList;
	delete m_pPluginPath;
}


// Base number of channels accessors.
void qtractorPluginSelectForm::setChannels ( unsigned short iChannels )
{
	m_iChannels = iChannels;
	refresh();
}


unsigned short qtractorPluginSelectForm::channels (void)
{
	return m_iChannels;
}


// Final selection accessors..
int qtractorPluginSelectForm::pluginCount (void)
{
	return m_pSelectList->count();
}

const QString& qtractorPluginSelectForm::pluginFilename ( int iPlugin )
{
	qtractorPluginSelectItem *pSelectItem = m_pSelectList->at(iPlugin);
	return (pSelectItem ? pSelectItem->filename : QString::null);
}

unsigned long qtractorPluginSelectForm::pluginIndex ( int iPlugin )
{
	qtractorPluginSelectItem *pSelectItem = m_pSelectList->at(iPlugin);
	return (pSelectItem ? pSelectItem->index : 0);
}


// Reset plugin listing.
void qtractorPluginSelectForm::reset (void)
{
	PluginSearchComboBox->setCurrentText(QString::null);
//	refresh();
}


// Refresh plugin listing.
void qtractorPluginSelectForm::refresh (void)
{
	QString sSearch = PluginSearchComboBox->currentText().simplifyWhiteSpace();
	QRegExp rx(sSearch.replace(QRegExp("[\\s]+"), ".*"), false);

	PluginListView->setUpdatesEnabled(false);

	PluginListView->clear();
	qtractorPluginSelectListItem *pItem = NULL;
	QPtrList<qtractorPluginFile>& files = m_pPluginPath->files();	
	for (qtractorPluginFile *pFile = files.first();
			pFile; pFile = files.next()) {
		if (pFile->open()) {
			QPtrList<qtractorPluginType>& types = pFile->types();	
			for (qtractorPluginType *pType = types.first();
					pType; pType = types.next()) {
				const LADSPA_Descriptor *pDescriptor = pType->descriptor();
				const QString& sFilename = pType->filename();
				if (rx.isEmpty() || rx.search(pDescriptor->Name) >= 0
					|| rx.search(sFilename) >= 0) {
					pItem = new qtractorPluginSelectListItem(
						PluginListView, pItem);
					pItem->setText(0, pDescriptor->Name);
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
					pItem->setText(1,
						QString("%1:%2").arg(iAudioIns).arg(iAudioOuts));
					pItem->setText(2,
						QString("%1:%2").arg(iControlIns).arg(iControlOuts));
					if (LADSPA_IS_HARD_RT_CAPABLE(pDescriptor->Properties))
						pItem->setText(3, tr("RT"));
					pItem->setText(4, sFilename);
					pItem->setText(5, QString::number(pType->index()));
					pItem->setText(6, QString::number(iInstances));
					pItem->setSelectable(iInstances > 0);
				}
			}
			pFile->close();
		}
	}

	PluginListView->setUpdatesEnabled(true);
	PluginListView->triggerUpdate();

	PluginResetToolButton->setEnabled(!rx.isEmpty());
	
	stabilize();
}


// Stabilize slot.
void qtractorPluginSelectForm::stabilize (void)
{
	bool bSelected = false;

	QListViewItem *pItem = PluginListView->firstChild();
	while (pItem) {
		if (pItem->isSelected()) {
			bSelected = true;
			break;
		}
		pItem = pItem->nextSibling();
	}

	OkPushButton->setEnabled(bSelected);
}


// Accept slot.
void qtractorPluginSelectForm::accept (void)
{
	m_pSelectList->clear();

	QListViewItem *pItem = PluginListView->firstChild();
	while (pItem) {
		if (pItem->isSelected())
			m_pSelectList->append(new qtractorPluginSelectItem(pItem));
		pItem = pItem->nextSibling();
	}

	// Are we done?
	if (m_pSelectList->count() > 0)
		QDialog::accept();
}


// end of qtractorPluginSelectForm.ui.h
