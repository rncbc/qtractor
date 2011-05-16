// qtractorTimeScaleForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorTimeScaleCommand.h"

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorSession.h"

#include "qtractorMainForm.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"

#include <QItemDelegate>

#include <QHeaderView>
#include <QMessageBox>
#include <QTime>
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
		QTreeWidgetItem::setText(2, QString::number(m_pNode->tempo));
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

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize locals.
	m_pTimeScale  = NULL;

	m_pTempoTap   = new QTime();
	m_iTempoTap   = 0;
	m_fTempoTap   = 0.0f;

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

	QObject::connect(m_ui.BarSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(barChanged(int)));
	QObject::connect(m_ui.FrameSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(frameChanged(unsigned long)));
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(tempoChanged(float, unsigned short, unsigned short)));
	QObject::connect(m_ui.TempoPushButton,
		SIGNAL(clicked()),
		SLOT(tempoTap()));

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

	refreshNodes();
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
		++m_iDirtySetup;
		// Make this into view...
		m_ui.BarSpinBox->setValue(pNode->barFromFrame(iFrame) + 1);
		m_ui.FrameSpinBox->setValue(iFrame);
		m_ui.TempoSpinBox->setTempo(pNode->tempo, false);
		m_ui.TempoSpinBox->setBeatsPerBar(pNode->beatsPerBar, false);
		m_ui.TempoSpinBox->setBeatDivisor(pNode->beatDivisor, false);
		// Done.
		m_iDirtySetup = 0;
		// Locate nearest list item...
		setCurrentNode(pNode);
	}

	stabilizeForm();
}


unsigned long qtractorTimeScaleForm::frame (void) const
{
	return m_ui.FrameSpinBox->value();
}


unsigned short qtractorTimeScaleForm::bar (void) const
{
	return m_ui.BarSpinBox->value() - 1;
}


// Current dirty flag accessor.
bool qtractorTimeScaleForm::isDirty (void)
{
	return (m_iDirtyTotal > 0);
}


// Refresh all list and views.
void qtractorTimeScaleForm::refreshNodes (void)
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
}

void qtractorTimeScaleForm::refresh (void)
{
	refreshNodes();
	frameChanged(frame());

	m_iDirtyCount = 0;
}


// Current node list accessors.
void qtractorTimeScaleForm::setCurrentNode ( qtractorTimeScale::Node *pNode )
{
	++m_iDirtySetup;

	int iItemCount = m_ui.TimeScaleListView->topLevelItemCount();
	for (int i = 0; i < iItemCount; ++i) {
		qtractorTimeScaleListItem *pNodeItem
			= static_cast<qtractorTimeScaleListItem *> (
				m_ui.TimeScaleListView->topLevelItem(i));
		if (pNodeItem && pNodeItem->node() == pNode) {
			m_ui.TimeScaleListView->setCurrentItem(pNodeItem);
			break;
		}
	}

	m_iDirtySetup = 0;
}


qtractorTimeScale::Node *qtractorTimeScaleForm::currentNode (void) const
{
	// Get current selected item...
	QTreeWidgetItem *pItem = m_ui.TimeScaleListView->currentItem();
	if (pItem == NULL)
		return NULL;

	// Just make it in current view...
	qtractorTimeScaleListItem *pNodeItem
		= static_cast<qtractorTimeScaleListItem *> (pItem);
	if (pNodeItem == NULL)
		return NULL;

	// That's it...
	return pNodeItem->node();
}


// Make given frame visble at the main tracks view.
void qtractorTimeScaleForm::ensureVisibleFrame ( unsigned long iFrame )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->tracks())
		pMainForm->tracks()->trackView()->ensureVisibleFrame(iFrame);
}


// Time-scale node selection slot.
void qtractorTimeScaleForm::selectNode (void)
{
	if (m_iDirtySetup > 0)
		return;

	// Check if we need an update?...
	qtractorTimeScale::Node *pNode = currentNode();
	if (pNode == NULL)
		return;

	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.UpdatePushButton->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			updateNode();
			// Fall thru...
		case QMessageBox::Discard:
			break;;
		default:    // Cancel.
			return;
		}
	}

	// Get new one into view...
	++m_iDirtySetup;

	m_ui.BarSpinBox->setValue(pNode->bar + 1);
	m_ui.FrameSpinBox->setValue(pNode->frame);
	m_ui.TempoSpinBox->setTempo(pNode->tempo, false);
	m_ui.TempoSpinBox->setBeatsPerBar(pNode->beatsPerBar, false);
	m_ui.TempoSpinBox->setBeatDivisor(pNode->beatDivisor, false);

	ensureVisibleFrame(pNode->frame);

	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	stabilizeForm();
}


// Check whether the current view is elligible for action.
unsigned int qtractorTimeScaleForm::flags (void) const
{
	if (m_pTimeScale == NULL)
		return 0;

	unsigned short iBar = bar();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);

	float fTempo = m_ui.TempoSpinBox->tempo();
	unsigned short iBeatsPerBar = m_ui.TempoSpinBox->beatsPerBar();
	unsigned short iBeatDivisor = m_ui.TempoSpinBox->beatDivisor();

	unsigned int iFlags = 0;

	if (pNode && pNode->bar == iBar) {
		iFlags |= Update;
		if (pNode->prev())
			iFlags |= Remove;
	}
	if (pNode && pNode->tempo == fTempo
	//	&& pNode->beatType == iBeatType
		&& pNode->beatsPerBar == iBeatsPerBar
		&& pNode->beatDivisor == iBeatDivisor)
		iFlags &= ~Update;
	else
		iFlags |= Add;
	if (pNode && pNode->bar == iBar)
		iFlags &= ~Add;

	return iFlags;
}


// Add node method.
void qtractorTimeScaleForm::addNode (void)
{
	if (m_pTimeScale == NULL)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorTimeScaleAddNodeCommand(m_pTimeScale,
			m_ui.FrameSpinBox->value(),
			m_ui.TempoSpinBox->tempo(), 2,
			m_ui.TempoSpinBox->beatsPerBar(),
			m_ui.TempoSpinBox->beatDivisor()));

	refresh();

	++m_iDirtyTotal;
}


// Update current node.
void qtractorTimeScaleForm::updateNode (void)
{
	if (m_pTimeScale == NULL)
		return;

	unsigned short iBar = bar();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);
	unsigned long iFrame = (pNode ? pNode->frameFromBar(iBar) : 0);

	if (pNode == NULL)
		return;
	if (pNode->bar != iBar)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorTimeScaleUpdateNodeCommand(m_pTimeScale, iFrame,
			m_ui.TempoSpinBox->tempo(), 2,
			m_ui.TempoSpinBox->beatsPerBar(),
			m_ui.TempoSpinBox->beatDivisor()));

	refresh();

	++m_iDirtyTotal;
}


// Remove current node.
void qtractorTimeScaleForm::removeNode (void)
{
	if (m_pTimeScale == NULL)
		return;

	unsigned short iBar = bar();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);

	if (pNode == NULL)
		return;
	if (pNode->bar != iBar)
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
			.arg(pNode->tempo)
			.arg(pNode->beatsPerBar)
			.arg(1 << pNode->beatDivisor),
			QMessageBox::Ok | QMessageBox::Cancel)
			== QMessageBox::Cancel)
			return;
	}

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorTimeScaleRemoveNodeCommand(m_pTimeScale, pNode));

	refresh();

	++m_iDirtyTotal;
}


// Make changes due.
void qtractorTimeScaleForm::barChanged ( int iBar )
{
	if (m_pTimeScale == NULL)
		return;
	if (m_iDirtySetup > 0)
		return;

	if (iBar > 0)
		--iBar;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);

	if (pNode) {
		++m_iDirtySetup;
		unsigned long iFrame = pNode->frameFromBar(iBar);
		m_ui.FrameSpinBox->setValue(iFrame);
		m_iDirtySetup = 0;
		setCurrentNode(pNode);
		ensureVisibleFrame(iFrame);
	}

	++m_iDirtyCount;
	stabilizeForm();
}


void qtractorTimeScaleForm::frameChanged ( unsigned long iFrame )
{
	if (m_pTimeScale == NULL)
		return;
	if (m_iDirtySetup > 0)
		return;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

	if (pNode) {
		++m_iDirtySetup;
		m_ui.BarSpinBox->setValue(pNode->barFromFrame(iFrame) + 1);
		m_iDirtySetup = 0;
		setCurrentNode(pNode);
		ensureVisibleFrame(iFrame);
	}

	++m_iDirtyCount;
	stabilizeForm();
}


// Tempo signature has changed.
void qtractorTimeScaleForm::tempoChanged (
	float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTeimeScaleForm::tempoChanged(%g, %u, %u)",
		fTempo, iBeatsPerBar, iBeatDivisor);
#endif

	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;

	changed();
}


void qtractorTimeScaleForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Reject settings (Close button slot).
void qtractorTimeScaleForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("Some settings have been changed.\n\n"
			"Do you want to discard the changes?"),
			QMessageBox::Discard | QMessageBox::Cancel)
			== QMessageBox::Cancel) {
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Tempo tap click.
void qtractorTimeScaleForm::tempoTap (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::tempoTap()");
#endif

	int iTimeTap = m_pTempoTap->restart();
	if (iTimeTap > 200 && iTimeTap < 2000) { // Magic!
		m_fTempoTap += (60000.0f / float(iTimeTap));
		if (++m_iTempoTap > 2) {
			m_fTempoTap /= float(m_iTempoTap);
			m_iTempoTap  = 1; // Median-like averaging...
			m_ui.TempoSpinBox->setTempo(int(m_fTempoTap), true);
		}
	} else {
		m_iTempoTap = 0;
		m_fTempoTap = 0.0f;
	}
}


// Stabilize current form state.
void qtractorTimeScaleForm::stabilizeForm (void)
{
	unsigned int iFlags = flags();
//	m_ui.RefreshPushButton->setEnabled(m_iDirtyCount > 0);
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
		QIcon(":/images/formAdd.png"),
		tr("&Add"), this, SLOT(addNode()));
	pAction->setEnabled(iFlags & Add);

	pAction = menu.addAction(
		QIcon(":/images/formAccept.png"),
		tr("&Update"), this, SLOT(updateNode()));
	pAction->setEnabled(iFlags & Update);

	pAction = menu.addAction(
		QIcon(":/images/formRemove.png"),
		tr("&Remove"), this, SLOT(removeNode()));
	pAction->setEnabled(iFlags & Remove);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/images/formRefresh.png"),
		tr("&Refresh"), this, SLOT(refresh()));
//	pAction->setEnabled(m_iDirtyCount > 0);

//	menu.exec(m_ui.BusListView->mapToGlobal(pos));
	menu.exec(QCursor::pos());
}


// end of qtractorTimeScaleForm.cpp
