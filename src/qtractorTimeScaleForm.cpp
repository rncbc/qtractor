// qtractorTimeScaleForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTimeScaleForm.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorSession.h"

#include <QItemDelegate>

#include <QHeaderView>
#include <QMessageBox>
#include <QMenu>


//----------------------------------------------------------------------
// class qtractorTimeScaleItemDelegate -- Custom time-scale item delegate.
//

class qtractorTimeScaleItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	qtractorTimeScaleItemDelegate(QObject *pParent = 0)
		: QItemDelegate(pParent) {}

protected:

	// Modified render method.
	void paint(QPainter *painter,
		const QStyleOptionViewItem& option, const QModelIndex& index ) const
	{
		QStyleOptionViewItem opt(option);
		switch (index.column()) {
		case 0: // Bar
		case 1: // Time
		case 2: // Tempo
			opt.displayAlignment = Qt::AlignHCenter;
			break;
		case 3: // Signature
		default:
			opt.displayAlignment = Qt::AlignLeft;
			break;
		}
		QItemDelegate::paint(painter, opt, index);
	}
};

//----------------------------------------------------------------------
// class qtractorTimeScaleListItem -- Custom time-scale list item.
//

class qtractorTimeScaleListItem : public QTreeWidgetItem
{
public:

	// Constructor.
	qtractorTimeScaleListItem(QTreeWidget *pTreeWidget,
		qtractorTimeScale *pTimeScale, qtractorTimeScale::Node *pNode)
		: QTreeWidgetItem(pTreeWidget), m_pNode(pNode)
	{
		QTreeWidgetItem::setText(0, QString::number(m_pNode->bar + 1));
		QTreeWidgetItem::setText(1, pTimeScale->textFromTick(m_pNode->tick));
		QTreeWidgetItem::setText(2, QString::number(m_pNode->tempoEx()));
		QTreeWidgetItem::setText(3, QString("%1 / %2")
			.arg(m_pNode->beatsPerBar).arg(1 << m_pNode->beatDivisor));
	}

	// Node accessors.
	qtractorTimeScale::Node *node() const { return m_pNode; }

private:

	// Instance variables.
	qtractorTimeScale::Node *m_pNode;
};


//----------------------------------------------------------------------------
// qtractorTimeScaleForm -- UI wrapper form.

// Constructor.
qtractorTimeScaleForm::qtractorTimeScaleForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Initialize locals.
	m_pTimeScale  = NULL;

	m_iDirtySetup = 0;
	m_iDirtyCount = 0;
	m_iDirtyTotal = 0;

	QHeaderView *pHeader = m_ui.TimeScaleListView->header();
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
	pHeader->setMovable(false);

	m_ui.TimeScaleListView->setItemDelegate(
		new qtractorTimeScaleItemDelegate(m_ui.TimeScaleListView));
	m_ui.TimeScaleListView->setContextMenuPolicy(Qt::CustomContextMenu);

	// (Re)initial contents.
	// Default is main session time-scale of course...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		setTimeScale(pSession->timeScale());

	// Try to restore normal window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.TimeScaleListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(selectNode()));
	QObject::connect(m_ui.TimeScaleListView,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(contextMenu(const QPoint&)));

	QObject::connect(m_ui.FrameSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.BeatTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.BeatsPerBarSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.BeatDivisorComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));

	QObject::connect(m_ui.RefreshPushButton,
		SIGNAL(clicked()),
		SLOT(refresh()));
	QObject::connect(m_ui.AddPushButton,
		SIGNAL(clicked()),
		SLOT(addNode()));
	QObject::connect(m_ui.UpdatePushButton,
		SIGNAL(clicked()),
		SLOT(updateNode()));
	QObject::connect(m_ui.RemovePushButton,
		SIGNAL(clicked()),
		SLOT(removeNode()));
	QObject::connect(m_ui.ClosePushButton,
		SIGNAL(clicked()),
		SLOT(reject()));

	stabilizeForm();
}


// Time-scale accessor.
void qtractorTimeScaleForm::setTimeScale ( qtractorTimeScale *pTimeScale )
{
	m_pTimeScale = pTimeScale;

	m_ui.FrameSpinBox->setTimeScale(m_pTimeScale);

	refresh();
}

qtractorTimeScale *qtractorTimeScaleForm::timeScale (void) const
{
	return m_pTimeScale;
}


// Select(ed) node by frame (time)
void qtractorTimeScaleForm::setFrame ( unsigned long iFrame )
{
	if (m_pTimeScale == NULL)
		return;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

	if (pNode) {
		m_iDirtySetup++;
		m_ui.FrameSpinBox->setValue(iFrame);
		m_ui.TempoSpinBox->setValue(pNode->tempo);
		m_ui.BeatTypeComboBox->setCurrentIndex(pNode->beatType - 1);
		m_ui.BeatsPerBarSpinBox->setValue(int(pNode->beatsPerBar));
		m_ui.BeatDivisorComboBox->setCurrentIndex(pNode->beatDivisor - 1);
		m_iDirtySetup = 0;
	}

	stabilizeForm();
}

unsigned long qtractorTimeScaleForm::frame (void) const
{
	return m_ui.FrameSpinBox->value();
}


// Current dirty flag accessor.
bool qtractorTimeScaleForm::isDirty (void)
{
	return (m_iDirtyTotal > 0);
}


// Refresh all list and views.
void qtractorTimeScaleForm::refresh (void)
{
	if (m_pTimeScale == NULL)
		return;

	// (Re)Load complete tempo-map listing ...
	m_ui.TimeScaleListView->clear();
	qtractorTimeScale::Node *pNode
		= m_pTimeScale->nodes().first();
	while (pNode) {
		new qtractorTimeScaleListItem(
			m_ui.TimeScaleListView, m_pTimeScale, pNode);
		pNode = pNode->next();
	}

	m_iDirtyCount = 0;

	stabilizeForm();
}


// Time-scale node selection slot.
void qtractorTimeScaleForm::selectNode (void)
{
	// Get current selected item, must not be a root one...
	QTreeWidgetItem *pItem = m_ui.TimeScaleListView->currentItem();
	if (pItem == NULL)
		return;

	// Just make it in current view...
	qtractorTimeScaleListItem *pNodeItem
		= static_cast<qtractorTimeScaleListItem *> (pItem);
	if (pNodeItem == NULL)
		return;

	// Check if we need an update?...
	qtractorTimeScale::Node *pNode = pNodeItem->node();
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			tr("Apply"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Apply
			updateNode();
			// Fall thru...
		case 1:     // Discard
			break;;
		default:    // Cancel.
			return;
		}
	}

	// Get new one into view...
	m_iDirtySetup++;

	m_ui.FrameSpinBox->setValue(pNode->frame);
	m_ui.TempoSpinBox->setValue(pNode->tempo);
	m_ui.BeatTypeComboBox->setCurrentIndex(pNode->beatType - 1);
	m_ui.BeatsPerBarSpinBox->setValue(int(pNode->beatsPerBar));
	m_ui.BeatDivisorComboBox->setCurrentIndex(pNode->beatDivisor - 1);

	// FIXME: Don't let tempo beat type be modified...
	m_ui.BeatTypeComboBox->setEnabled(false);

	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	stabilizeForm();
}


// Check whether the current view is elligible for action.
unsigned int qtractorTimeScaleForm::flags (void) const
{
	if (m_pTimeScale == NULL)
		return 0;

	unsigned int iFlags = 0;

	unsigned long iFrame = frame();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

	float          fTempo = m_ui.TempoSpinBox->value();
	unsigned short iBeatType = m_ui.BeatTypeComboBox->currentIndex() + 1;
	unsigned short iBeatsPerBar = m_ui.BeatsPerBarSpinBox->value();
	unsigned short iBeatDivisor = m_ui.BeatDivisorComboBox->currentIndex() + 1;

	if (pNode && pNode->prev())
		iFlags |= Remove;

	if (pNode && pNode->frame == iFrame)
		iFlags |= Update;
	if (pNode && pNode->tempo == fTempo
		&& pNode->beatType == iBeatType
		&& pNode->beatsPerBar == iBeatsPerBar
		&& pNode->beatDivisor == iBeatDivisor)
		iFlags &= ~Update;
	else
		iFlags |= Add;
	if (pNode && pNode->frame == iFrame)
		iFlags &= ~Add;

	return iFlags;
}


// Add node method.
void qtractorTimeScaleForm::addNode (void)
{
	if (m_pTimeScale == NULL)
		return;

	// TODO: Make it as an undoable command...
	m_pTimeScale->addNode(
		m_ui.FrameSpinBox->value(),
		m_ui.TempoSpinBox->value(),
		m_ui.BeatTypeComboBox->currentIndex() + 1,
		m_ui.BeatsPerBarSpinBox->value(),
		m_ui.BeatDivisorComboBox->currentIndex() + 1);

	refresh();

	m_iDirtyTotal++;
}


// Update current node.
void qtractorTimeScaleForm::updateNode (void)
{
	if (m_pTimeScale == NULL)
		return;

	unsigned long iFrame = frame();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

//	pNode->frame = iFrame;
	pNode->tempo = m_ui.TempoSpinBox->value();
	pNode->beatType = m_ui.BeatTypeComboBox->currentIndex() + 1;
	pNode->beatsPerBar = m_ui.BeatsPerBarSpinBox->value();
	pNode->beatDivisor = m_ui.BeatDivisorComboBox->currentIndex() + 1;

	// TODO: Make it as an undoable command...
	m_pTimeScale->updateNode(pNode);

	refresh();

	m_iDirtyTotal++;
}


// Remove current node.
void qtractorTimeScaleForm::removeNode (void)
{
	if (m_pTimeScale == NULL)
		return;

	unsigned long iFrame = frame();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

	if (pNode == NULL)
		return;
	if (pNode->prev() == NULL)
		return;
 
	// Prompt user if he/she's sure about this...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bConfirmRemove) {
		// Show the warning...
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove tempo node:\n\n"
			"%1 (%2) %3  %4/%5\n\n"
			"Are you sure?")
			.arg(pNode->bar + 1)
			.arg(m_pTimeScale->textFromTick(pNode->tick))
			.arg(pNode->tempoEx())
			.arg(pNode->beatsPerBar)
			.arg(1 << pNode->beatDivisor),
			tr("OK"), tr("Cancel")) > 0)
			return;
	}

	// TODO: Make it as an undoable command...
	m_pTimeScale->removeNode(pNode);

	refresh();

	m_iDirtyTotal++;
}


// Make changes due.
void qtractorTimeScaleForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	m_iDirtyCount++;
	stabilizeForm();
}


// Reject settings (Close button slot).
void qtractorTimeScaleForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to discard the changes?"),
			tr("Discard"), tr("Cancel"))) {
		case 0:     // Discard
			break;
		default:    // Cancel.
			bReject = false;
			break;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Stabilize current form state.
void qtractorTimeScaleForm::stabilizeForm (void)
{
	unsigned int iFlags = flags();
	m_ui.RefreshPushButton->setEnabled(m_iDirtyCount > 0);
	m_ui.AddPushButton->setEnabled(iFlags & Add);
	m_ui.UpdatePushButton->setEnabled(iFlags & Update);
	m_ui.RemovePushButton->setEnabled(iFlags & Remove);
}


// Time-scale list view context menu handler.
void qtractorTimeScaleForm::contextMenu ( const QPoint& /*pos*/ )
{
	// Build the device context menu...
	QMenu menu(this);
	QAction *pAction;

	unsigned int iFlags = flags();
	
	pAction = menu.addAction(
		QIcon(":/icons/formCreate.png"),
		tr("&Add"), this, SLOT(addNode()));
	pAction->setEnabled(iFlags & Add);

	pAction = menu.addAction(
		QIcon(":/icons/formAccept.png"),
		tr("&Update"), this, SLOT(updateNode()));
	pAction->setEnabled(iFlags & Update);

	pAction = menu.addAction(
		QIcon(":/icons/formRemove.png"),
		tr("&Remove"), this, SLOT(removeNode()));
	pAction->setEnabled(iFlags & Remove);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/icons/formRefresh.png"),
		tr("&Refresh"), this, SLOT(refresh()));
	pAction->setEnabled(m_iDirtyCount > 0);

//	menu.exec(m_ui.BusListView->mapToGlobal(pos));
	menu.exec(QCursor::pos());
}


// end of qtractorTimeScaleForm.cpp
