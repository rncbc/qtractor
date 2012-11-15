// qtractorTimeScaleForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2012, rncbc aka Rui Nuno Capela. All rights reserved.

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
		case 3: // Marker
		default:
			opt.displayAlignment = Qt::AlignLeft;
			break;
		}
		QItemDelegate::paint(painter, opt, index);
	}
};


//----------------------------------------------------------------------
// class qtractorTimeScaleListItem -- Custom time-scale tempo node item.
//

class qtractorTimeScaleListItem : public QTreeWidgetItem
{
public:

	// Constructors.
	qtractorTimeScaleListItem(QTreeWidget *pTreeWidget,
		qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Node *pNode,
		qtractorTimeScale::Marker *pMarker)
		: QTreeWidgetItem(pTreeWidget), m_pNode(pNode), m_pMarker(pMarker)
	{
		const QChar dash = '-';

		if (pNode) {
			QTreeWidgetItem::setText(0, QString::number(m_pNode->bar + 1));
			QTreeWidgetItem::setText(1, pTimeScale->textFromTick(m_pNode->tick));
			QTreeWidgetItem::setText(2, QString("%1 %2/%3")
				.arg(m_pNode->tempo, 0, 'f', 1)
				.arg(m_pNode->beatsPerBar)
				.arg(1 << m_pNode->beatDivisor));
		}
		else QTreeWidgetItem::setText(2, dash);

		if (pMarker) {
			if (pNode == NULL) {
				unsigned int iBar = pTimeScale->barFromFrame(pMarker->frame);
				QTreeWidgetItem::setText(0, QString::number(iBar + 1));
				QTreeWidgetItem::setText(1, pTimeScale->textFromFrame(pMarker->frame));
			}
			QTreeWidgetItem::setText(3, pMarker->text);
		}
		else QTreeWidgetItem::setText(3, dash);
	}

	// Node accessor.
	qtractorTimeScale::Node *node() const { return m_pNode; }

	// Marker accessor.
	qtractorTimeScale::Marker *marker() const { return m_pMarker; }

private:

	// Instance variables.
	qtractorTimeScale::Node   *m_pNode;
	qtractorTimeScale::Marker *m_pMarker;
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
		SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
		SLOT(selectItem()));
	QObject::connect(m_ui.TimeScaleListView,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(contextMenu(const QPoint&)));

	QObject::connect(m_ui.BarSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(barChanged(int)));
	QObject::connect(m_ui.TimeSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(timeChanged(unsigned long)));
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(tempoChanged(float, unsigned short, unsigned short)));
	QObject::connect(m_ui.TempoPushButton,
		SIGNAL(clicked()),
		SLOT(tempoTap()));
	QObject::connect(m_ui.MarkerLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(markerChanged(const QString&)));

	QObject::connect(m_ui.RefreshPushButton,
		SIGNAL(clicked()),
		SLOT(refresh()));
	QObject::connect(m_ui.AddPushButton,
		SIGNAL(clicked()),
		SLOT(addItem()));
	QObject::connect(m_ui.UpdatePushButton,
		SIGNAL(clicked()),
		SLOT(updateItem()));
	QObject::connect(m_ui.RemovePushButton,
		SIGNAL(clicked()),
		SLOT(removeItem()));
	QObject::connect(m_ui.ClosePushButton,
		SIGNAL(clicked()),
		SLOT(reject()));

	stabilizeForm();
}


// Time-scale accessor.
void qtractorTimeScaleForm::setTimeScale ( qtractorTimeScale *pTimeScale )
{
	m_pTimeScale = pTimeScale;

	m_ui.TimeSpinBox->setTimeScale(m_pTimeScale);

	refreshItems();
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

	++m_iDirtySetup;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);
	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

	if (pNode) {
		// Make this into view...
		m_ui.BarSpinBox->setValue(pNode->barFromFrame(iFrame) + 1);
		m_ui.TimeSpinBox->setValue(iFrame);
		m_ui.TempoSpinBox->setTempo(pNode->tempo, false);
		m_ui.TempoSpinBox->setBeatsPerBar(pNode->beatsPerBar, false);
		m_ui.TempoSpinBox->setBeatDivisor(pNode->beatDivisor, false);
	}

	qtractorTimeScale::MarkerCursor markers(m_pTimeScale);
	qtractorTimeScale::Marker *pMarker = markers.seekFrame(iFrame);

	if (pMarker && pMarker->frame == iFrame)
		m_ui.MarkerLineEdit->setText(pMarker->text);
	else
		m_ui.MarkerLineEdit->clear();

	// Done.
	m_iDirtySetup = 0;

	// Locate nearest list item...
	if (pNode) setCurrentItem(pNode, iFrame);

	stabilizeForm();
}


unsigned long qtractorTimeScaleForm::frame (void) const
{
	return m_ui.TimeSpinBox->value();
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
void qtractorTimeScaleForm::refreshItems (void)
{
	if (m_pTimeScale == NULL)
		return;

	// (Re)Load complete tempo-map listing ...
	m_ui.TimeScaleListView->clear();

	qtractorTimeScale::Node *pNode = m_pTimeScale->nodes().first();
	qtractorTimeScale::Marker *pMarker = m_pTimeScale->markers().first();

	while (pNode) {
		while (pMarker && pMarker->frame < pNode->frame) {
			new qtractorTimeScaleListItem(
				m_ui.TimeScaleListView, m_pTimeScale, NULL, pMarker);
			pMarker = pMarker->next();
		}
		if (pMarker && pMarker->frame == pNode->frame) {
			new qtractorTimeScaleListItem(
				m_ui.TimeScaleListView, m_pTimeScale, pNode, pMarker);
			pMarker = pMarker->next();
		} else {
			new qtractorTimeScaleListItem(
				m_ui.TimeScaleListView, m_pTimeScale, pNode, NULL);
		}
		pNode = pNode->next();
	}

	while (pMarker) {
		new qtractorTimeScaleListItem(
			m_ui.TimeScaleListView, m_pTimeScale, NULL, pMarker);
		pMarker = pMarker->next();
	}
}

void qtractorTimeScaleForm::refresh (void)
{
	refreshItems();
	timeChanged(frame());

	m_iDirtyCount = 0;
}


// Current node list accessors.
void qtractorTimeScaleForm::setCurrentItem (
	qtractorTimeScale::Node *pNode, unsigned long iFrame )
{
	++m_iDirtySetup;

	qtractorTimeScale::MarkerCursor markers(m_pTimeScale);
	qtractorTimeScale::Marker *pMarker = markers.seekFrame(iFrame);

	int iItemCount = m_ui.TimeScaleListView->topLevelItemCount();
	for (int i = iItemCount - 1; i >= 0; --i) {
		qtractorTimeScaleListItem *pListItem
			= static_cast<qtractorTimeScaleListItem *> (
				m_ui.TimeScaleListView->topLevelItem(i));
		if (pListItem && (pListItem->node() == pNode
			|| (pMarker && pListItem->marker() == pMarker))) {
			m_ui.TimeScaleListView->setCurrentItem(pListItem);
			break;
		}
	}

	m_iDirtySetup = 0;
}


// Make given frame visble at the main tracks view.
void qtractorTimeScaleForm::ensureVisibleFrame ( unsigned long iFrame )
{
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm && pMainForm->tracks())
		pMainForm->tracks()->trackView()->ensureVisibleFrame(iFrame);
}


// Time-scale node selection slot.
void qtractorTimeScaleForm::selectItem (void)
{
	if (m_pTimeScale == NULL)
		return;

	if (m_iDirtySetup > 0)
		return;

	// Check if we need an update?...
	qtractorTimeScaleListItem *pListItem
		= static_cast<qtractorTimeScaleListItem *> (
			m_ui.TimeScaleListView->currentItem());

	if (pListItem == NULL)
		return;

	qtractorTimeScale::Node   *pNode   = pListItem->node();
	qtractorTimeScale::Marker *pMarker = pListItem->marker();

	if (pNode == NULL && pMarker == NULL)
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
			updateItem();
			// Fall thru...
		case QMessageBox::Discard:
			break;;
		default:    // Cancel.
			return;
		}
	}

	// Get new one into view...
	++m_iDirtySetup;

	if (pNode) {
		m_ui.BarSpinBox->setValue(pNode->bar + 1);
		m_ui.TimeSpinBox->setValue(pNode->frame);
		m_ui.TempoSpinBox->setTempo(pNode->tempo, false);
		m_ui.TempoSpinBox->setBeatsPerBar(pNode->beatsPerBar, false);
		m_ui.TempoSpinBox->setBeatDivisor(pNode->beatDivisor, false);
		ensureVisibleFrame(pNode->frame);
	}

	if (pMarker) {
		m_ui.MarkerLineEdit->setText(pMarker->text);
		if (pNode == NULL) {
			unsigned int iBar = m_pTimeScale->barFromFrame(pMarker->frame);
			m_ui.BarSpinBox->setValue(iBar + 1);
			m_ui.TimeSpinBox->setValue(pMarker->frame);
			ensureVisibleFrame(pMarker->frame);
		}
	}
	else m_ui.MarkerLineEdit->clear();

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

	float fTempo = m_ui.TempoSpinBox->tempo();
	unsigned short iBeatsPerBar = m_ui.TempoSpinBox->beatsPerBar();
	unsigned short iBeatDivisor = m_ui.TempoSpinBox->beatDivisor();

	unsigned short iBar = bar();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);

	if (pNode && pNode->bar == iBar) {
		iFlags |= Node;
		iFlags |= Update;
		if (pNode->prev())
			iFlags |= Remove;
	}
	if (pNode && pNode->tempo == fTempo
	//	&& pNode->beatType == iBeatType
		&& pNode->beatsPerBar == iBeatsPerBar
		&& pNode->beatDivisor == iBeatDivisor) {
		if ((iFlags & (Node | Marker)) == Node)
			iFlags &= ~Update;
	} else {
		iFlags |= Node;
		iFlags |= Add;
	}
	if (pNode && pNode->bar == iBar) {
		if ((iFlags & (Node | Marker)) == Node)
			iFlags &= ~Add;
	}

	unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
	qtractorTimeScale::MarkerCursor markers(m_pTimeScale);
	qtractorTimeScale::Marker *pMarker = markers.seekFrame(iFrame);

	const QString& sMarkerText = m_ui.MarkerLineEdit->text().simplified();

	if (pMarker && pMarker->frame == iFrame) {
		iFlags |= Marker;
		iFlags |= Update;
		iFlags |= Remove;
	}
	if (pMarker && pMarker->text == sMarkerText) {
		if ((iFlags & (Node | Marker)) == Marker)
			iFlags &= ~Update;
	} else if (!sMarkerText.isEmpty()) {
		iFlags |= Marker;
		iFlags |= Add;
	}
	if (pMarker && pMarker->frame == iFrame) {
		if ((iFlags & (Node | Marker)) == Marker)
			iFlags &= ~Add;
	}

	return iFlags;
}


// Add node method.
void qtractorTimeScaleForm::addItem (void)
{
	if (m_pTimeScale == NULL)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	unsigned int  iFlags = flags();
	unsigned long iFrame = frame();

	if ((iFlags & Add) == 0)
		return;

	if (iFlags & Node) {
		pSession->execute(
			new qtractorTimeScaleAddNodeCommand(
				m_pTimeScale, iFrame,
				m_ui.TempoSpinBox->tempo(), 2,
				m_ui.TempoSpinBox->beatsPerBar(),
				m_ui.TempoSpinBox->beatDivisor()));
	}

	if (iFlags & Marker) {
		pSession->execute(
			new qtractorTimeScaleAddMarkerCommand(
				m_pTimeScale, iFrame,
				m_ui.MarkerLineEdit->text()));
	}

	refresh();

	++m_iDirtyTotal;
}


// Update current node.
void qtractorTimeScaleForm::updateItem (void)
{
	if (m_pTimeScale == NULL)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	unsigned int   iFlags = flags();
	unsigned short iBar   = bar();

	if ((iFlags & Update) == 0)
		return;

	if (iFlags & Node) {
		qtractorTimeScale::Cursor cursor(m_pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);
		if (pNode && pNode->bar == iBar) {
			unsigned long iFrame = pNode->frameFromBar(iBar);
			pSession->execute(
				new qtractorTimeScaleUpdateNodeCommand(
					m_pTimeScale, iFrame,
					m_ui.TempoSpinBox->tempo(), 2,
					m_ui.TempoSpinBox->beatsPerBar(),
					m_ui.TempoSpinBox->beatDivisor()));
			++m_iDirtyTotal;
		}
	}

	if (iFlags & Marker) {
		unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
		qtractorTimeScale::MarkerCursor markers(m_pTimeScale);
		qtractorTimeScale::Marker *pMarker = markers.seekFrame(iFrame);
		if (pMarker && pMarker->frame == iFrame) {
			pSession->execute(
				new qtractorTimeScaleUpdateMarkerCommand(
					m_pTimeScale, iFrame,
					m_ui.MarkerLineEdit->text().simplified()));
			++m_iDirtyTotal;

		}
	}

	refresh();
}


// Remove current node.
void qtractorTimeScaleForm::removeItem (void)
{
	if (m_pTimeScale == NULL)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	unsigned int   iFlags = flags();
	unsigned short iBar   = bar();

	if ((iFlags & Remove) == 0)
		return;

	if (iFlags & Node) {
		qtractorTimeScale::Cursor cursor(m_pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);
		if (pNode && pNode->bar == iBar && pNode->prev()) {
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
					.arg(pNode->tempo, 0, 'f', 1)
					.arg(pNode->beatsPerBar)
					.arg(1 << pNode->beatDivisor),
					QMessageBox::Ok | QMessageBox::Cancel)
					== QMessageBox::Cancel)
					return;
			}
			// Go!...
			pSession->execute(
				new qtractorTimeScaleRemoveNodeCommand(m_pTimeScale, pNode));
			++m_iDirtyTotal;
		}
	}

	if (iFlags & Marker) {
		unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
		qtractorTimeScale::MarkerCursor markers(m_pTimeScale);
		qtractorTimeScale::Marker *pMarker = markers.seekFrame(iFrame);
		if (pMarker && pMarker->frame == iFrame) {
			// Go! we just don't ask user about a thing...
			pSession->execute(
				new qtractorTimeScaleRemoveMarkerCommand(
					m_pTimeScale, pMarker));
			++m_iDirtyTotal;

		}
	}

	refresh();
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
		m_ui.TimeSpinBox->setValue(iFrame);
		m_iDirtySetup = 0;
		setCurrentItem(pNode, iFrame);
		ensureVisibleFrame(iFrame);
	}

	++m_iDirtyCount;
	stabilizeForm();
}


void qtractorTimeScaleForm::timeChanged ( unsigned long iFrame )
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
		setCurrentItem(pNode, iFrame);
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
	qDebug("qtractorTimeScaleForm::tempoChanged(%g, %u, %u)",
		fTempo, iBeatsPerBar, iBeatDivisor);
#endif

	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;

	changed();
}


// Marker text has changed.
void qtractorTimeScaleForm::markerChanged (
	const QString& sText )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::markerChanged(\"%s\")",
		sText.toUtf8().constData());
#endif

	changed();
}


// Something has changed.
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
		tr("&Add"), this, SLOT(addItem()));
	pAction->setEnabled(iFlags & Add);

	pAction = menu.addAction(
		QIcon(":/images/formAccept.png"),
		tr("&Update"), this, SLOT(updateItem()));
	pAction->setEnabled(iFlags & Update);

	pAction = menu.addAction(
		QIcon(":/images/formRemove.png"),
		tr("&Remove"), this, SLOT(removeItem()));
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
