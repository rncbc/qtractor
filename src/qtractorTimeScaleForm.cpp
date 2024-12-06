// qtractorTimeScaleForm.cpp
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
#include <QElapsedTimer>
#include <QMenu>

#include <QColorDialog>

#include <cmath>


//----------------------------------------------------------------------
// class qtractorTimeScaleItemDelegate -- Custom time-scale item delegate.
//

class qtractorTimeScaleItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	qtractorTimeScaleItemDelegate(QObject *pParent = nullptr)
		: QItemDelegate(pParent) {}

protected:

	// Modified render method.
	void paint(QPainter *painter,
		const QStyleOptionViewItem& option, const QModelIndex& index ) const
	{
		QStyleOptionViewItem opt(option);
		switch (index.column()) {
		case 0: // Bar
		case 2: // Tempo
		case 3: // Key
			opt.displayAlignment = Qt::AlignCenter;
			break;
		default:
			opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
			break;
		}
		QItemDelegate::paint(painter, opt, index);
	}

	QSize sizeHint (
		const QStyleOptionViewItem& option, const QModelIndex& index ) const
	{
		return QItemDelegate::sizeHint(option, index) + QSize(4, 4);
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
		qtractorTimeScaleListItem *pListItem,
		qtractorTimeScale *pTimeScale,
		qtractorTimeScale::Node *pNode,
		qtractorTimeScale::Marker *pMarker,
		qtractorTimeScale::DisplayFormat displayFormat)
		: QTreeWidgetItem(pTreeWidget, pListItem),
			m_pNode(pNode), m_pMarker(pMarker)
	{
		const QChar dash = '-';

		if (pNode) {
			QTreeWidgetItem::setText(0, QString::number(m_pNode->bar + 1));
			QTreeWidgetItem::setText(1, pTimeScale->textFromFrameEx(
				displayFormat, m_pNode->frame));
			QTreeWidgetItem::setText(2, QString("%1 %2/%3")
				.arg(m_pNode->tempo)
				.arg(m_pNode->beatsPerBar)
				.arg(1 << m_pNode->beatDivisor));
		}
		else QTreeWidgetItem::setText(2, dash);

		if (pMarker) {
			if (pNode == nullptr) {
				const unsigned int iBar
					= pTimeScale->barFromFrame(pMarker->frame);
				QTreeWidgetItem::setText(0, QString::number(iBar + 1));
				QTreeWidgetItem::setText(1, pTimeScale->textFromFrameEx(
					displayFormat, pMarker->frame));
			}
			QTreeWidgetItem::setText(3,
				qtractorTimeScale::keySignatureName(
					pMarker->accidentals, pMarker->mode));
			if (!pMarker->text.isEmpty()) {
				QTreeWidgetItem::setText(4, pMarker->text);
				QTreeWidgetItem::setForeground(4, pMarker->color);
			}
			else QTreeWidgetItem::setText(4, dash);
		} else {
			QTreeWidgetItem::setText(3, dash);
			QTreeWidgetItem::setText(4, dash);
		}
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
qtractorTimeScaleForm::qtractorTimeScaleForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	// Initialize locals.
	m_pTimeScale  = nullptr;

	m_pTempoTap   = new QElapsedTimer();
	m_iTempoTap   = 0;
	m_fTempoTap   = 0.0f;

	m_iDirtySetup = 0;
	m_iDirtyCount = 0;
	m_iDirtyTotal = 0;

	QHeaderView *pHeader = m_ui.TimeScaleListView->header();
	pHeader->setDefaultAlignment(Qt::AlignLeft);
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
//	pHeader->setSectionResizeMode(QHeaderView::Custom);
	pHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
	pHeader->setSectionsMovable(false);
#else
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setResizeMode(QHeaderView::ResizeToContents);
	pHeader->setMovable(false);
#endif
	QAbstractItemModel *pHeaderModel = pHeader->model();
	pHeaderModel->setHeaderData(0, Qt::Horizontal,
		Qt::AlignCenter, Qt::TextAlignmentRole); // Bar
	pHeaderModel->setHeaderData(2, Qt::Horizontal,
		Qt::AlignCenter, Qt::TextAlignmentRole); // Tempo
	pHeaderModel->setHeaderData(3, Qt::Horizontal,
		Qt::AlignCenter, Qt::TextAlignmentRole); // Key

	m_ui.TimeScaleListView->setItemDelegate(
		new qtractorTimeScaleItemDelegate(m_ui.TimeScaleListView));
	m_ui.TimeScaleListView->setContextMenuPolicy(Qt::CustomContextMenu);

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	// Some conveniency cleaner helper...
	m_ui.MarkerTextLineEdit->setClearButtonEnabled(true);
#endif

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
	QObject::connect(m_ui.TimeSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(refreshItems()));

	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(tempoChanged()));
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(const QString&)),
		SLOT(tempoChanged()));
	QObject::connect(m_ui.TempoTapPushButton,
		SIGNAL(clicked()),
		SLOT(tempoTap()));
	QObject::connect(m_ui.TempoFactorPushButton,
		SIGNAL(clicked()),
		SLOT(tempoFactor()));

	QObject::connect(m_ui.KeySignatureAccidentalsComboBox,
		SIGNAL(activated(int)),
		SLOT(accidentalsChanged(int)));
	QObject::connect(m_ui.KeySignatureModeComboBox,
		SIGNAL(activated(int)),
		SLOT(modeChanged(int)));

	QObject::connect(m_ui.MarkerTextLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.MarkerColorToolButton,
		SIGNAL(clicked()),
		SLOT(markerColor()));

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


// Destructor.
qtractorTimeScaleForm::~qtractorTimeScaleForm (void)
{
	delete m_pTempoTap;
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
	if (m_pTimeScale == nullptr)
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
		m_ui.TempoSpinBox->setBeatDivisor(pNode->beatDivisor, true);
	}

	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(iFrame);
	if (pMarker && pMarker->frame == iFrame)
		setCurrentMarker(pMarker);
	else
		setCurrentMarker(nullptr);
	if (pMarker && iFrame >= pMarker->frame)
		setCurrentKeySignature(pMarker);
	else
		setCurrentKeySignature(nullptr);

	// Done.
	m_iDirtySetup = 0;

	// Locate nearest list item...
	if (pNode)
		setCurrentItem(pNode, iFrame);

	ensureVisibleFrame(iFrame);

	stabilizeForm();

	// HACK: Force focus to Bar location entry field...
	m_ui.BarSpinBox->setFocus();
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
	if (m_pTimeScale == nullptr)
		return;

	// (Re)Load complete tempo-map listing ...
	m_ui.TimeScaleListView->clear();

	const qtractorTimeScale::DisplayFormat displayFormat
		= m_ui.TimeSpinBox->displayFormat();

	qtractorTimeScale::Node *pNode = m_pTimeScale->nodes().first();
	qtractorTimeScale::Marker *pMarker = m_pTimeScale->markers().first();

	qtractorTimeScaleListItem *pListItem = nullptr;
	while (pNode) {
		while (pMarker && pMarker->frame < pNode->frame) {
			pListItem = new qtractorTimeScaleListItem(m_ui.TimeScaleListView,
				pListItem, m_pTimeScale, nullptr, pMarker, displayFormat);
			pMarker = pMarker->next();
		}
		if (pMarker && pMarker->frame == pNode->frame) {
			pListItem = new qtractorTimeScaleListItem(m_ui.TimeScaleListView,
				pListItem, m_pTimeScale, pNode, pMarker, displayFormat);
			pMarker = pMarker->next();
		} else {
			pListItem = new qtractorTimeScaleListItem(m_ui.TimeScaleListView,
				pListItem, m_pTimeScale, pNode, nullptr, displayFormat);
		}
		pNode = pNode->next();
	}

	while (pMarker) {
		pListItem = new qtractorTimeScaleListItem(m_ui.TimeScaleListView,
			pListItem, m_pTimeScale, nullptr, pMarker, displayFormat);
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

	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(iFrame);

	const int iItemCount = m_ui.TimeScaleListView->topLevelItemCount();
	for (int i = iItemCount - 1; i >= 0; --i) {
		qtractorTimeScaleListItem *pListItem
			= static_cast<qtractorTimeScaleListItem *> (
				m_ui.TimeScaleListView->topLevelItem(i));
		if (pListItem && (pListItem->node() == pNode || (pMarker
			&& pListItem->marker() == pMarker && iFrame >= pMarker->frame))) {
			m_ui.TimeScaleListView->setCurrentItem(pListItem);
			break;
		}
	}

	m_iDirtySetup = 0;
}


// Set current marker text & color...
void qtractorTimeScaleForm::setCurrentMarker (
	qtractorTimeScale::Marker *pMarker )
{
	QPalette pal;
	if (pMarker) {
		pal.setColor(QPalette::Text, pMarker->color);
	} else {
		QColor color = Qt::darkGray;
		qtractorOptions *pOptions = qtractorOptions::getInstance();
		if (pOptions && !pOptions->sMarkerColor.isEmpty())
		#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
			color = QColor::fromString(pOptions->sMarkerColor);
		#else
			color = QColor(pOptions->sMarkerColor);
		#endif
		pal.setColor(QPalette::Text, color);
	}
	m_ui.MarkerTextLineEdit->setPalette(pal);

	if (pMarker)
		m_ui.MarkerTextLineEdit->setText(pMarker->text);
	else
		m_ui.MarkerTextLineEdit->clear();
}


// Set current key-signature...
void qtractorTimeScaleForm::setCurrentKeySignature (
	qtractorTimeScale::Marker *pMarker )
{
	int iAccidentals = qtractorTimeScale::MinAccidentals;
	int iMode = -1;

	if (pMarker) {
		iAccidentals = pMarker->accidentals;
		iMode = pMarker->mode;
	}

	updateKeySignatures(iAccidentals, iMode);
}


// Refresh key signatures.
void qtractorTimeScaleForm::updateKeySignatures (
	int iAccidentals, int iMode )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::updateKeySignatures(%d, %d)",
		iAccidentals, iMode);
#endif

	const bool bBlockAccidentals
		= m_ui.KeySignatureAccidentalsComboBox->blockSignals(true);
	const bool bBlockMode
		= m_ui.KeySignatureModeComboBox->blockSignals(true);

	m_ui.KeySignatureAccidentalsComboBox->clear();

	int iIndex = 0;
	int iData = qtractorTimeScale::MinAccidentals;
	while (qtractorTimeScale::MaxAccidentals >= iData) {
		m_ui.KeySignatureAccidentalsComboBox->addItem(
			qtractorTimeScale::keySignatureName(iData, (iMode < 0 ? 0 : iMode), 0));
		m_ui.KeySignatureAccidentalsComboBox->setItemData(iIndex++, iData++);
	}

	iIndex = m_ui.KeySignatureAccidentalsComboBox->findData(iAccidentals);
	if (iIndex < 0) {
		iIndex = 0;
		iMode = -1;
	}
	m_ui.KeySignatureAccidentalsComboBox->setCurrentIndex(iIndex);
	m_ui.KeySignatureModeComboBox->setCurrentIndex(iMode + 1);

	m_ui.KeySignatureAccidentalsComboBox->blockSignals(bBlockAccidentals);
	m_ui.KeySignatureModeComboBox->blockSignals(bBlockMode);
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
	if (m_pTimeScale == nullptr)
		return;

	if (m_iDirtySetup > 0)
		return;

	// Check if we need an update?...
	qtractorTimeScaleListItem *pListItem
		= static_cast<qtractorTimeScaleListItem *> (
			m_ui.TimeScaleListView->currentItem());

	if (pListItem == nullptr)
		return;

	qtractorTimeScale::Node   *pNode   = pListItem->node();
	qtractorTimeScale::Marker *pMarker = pListItem->marker();

	if (pNode == nullptr && pMarker == nullptr)
		return;

	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.UpdatePushButton->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			updateItem();
			// Fall thru...
		case QMessageBox::Discard:
			break;
		default: // Cancel.
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
		m_ui.TempoSpinBox->setBeatDivisor(pNode->beatDivisor, true);
		ensureVisibleFrame(pNode->frame);
	}

	if (pMarker && pNode == nullptr) {
		const unsigned int iBar = m_pTimeScale->barFromFrame(pMarker->frame);
		m_ui.BarSpinBox->setValue(iBar + 1);
		m_ui.TimeSpinBox->setValue(pMarker->frame);
		ensureVisibleFrame(pMarker->frame);
	}

	setCurrentMarker(pMarker);
	setCurrentKeySignature(pMarker);

	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	stabilizeForm();
}


// Check whether the current view is elligible for action.
unsigned int qtractorTimeScaleForm::flags (void) const
{
	if (m_pTimeScale == nullptr)
		return 0;

	unsigned int iFlags = 0;

	const float fTempo = m_ui.TempoSpinBox->tempo();
	const unsigned short iBeatsPerBar = m_ui.TempoSpinBox->beatsPerBar();
	const unsigned short iBeatDivisor = m_ui.TempoSpinBox->beatDivisor();

	const unsigned short iBar = bar();
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);

	if (pNode && pNode->bar == iBar) {
		iFlags |= UpdateNode;
		if (pNode->prev())
			iFlags |= RemoveNode;
	}
	if (pNode
		&& qAbs(pNode->tempo - fTempo) < 0.001f
	//	&& pNode->beatType == iBeatType
		&& pNode->beatsPerBar == iBeatsPerBar
		&& pNode->beatDivisor == iBeatDivisor)
		iFlags &= ~UpdateNode;
	else
		iFlags |=  AddNode;
	if (pNode && pNode->bar == iBar)
		iFlags &= ~AddNode;
	if (pNode
		&& (pNode = pNode->next())	// real assignment
		&& qAbs(pNode->tempo - fTempo) < 0.001f
	//	&& pNode->beatType == iBeatType
		&& pNode->beatsPerBar == iBeatsPerBar
		&& pNode->beatDivisor == iBeatDivisor)
		iFlags &= ~AddNode;

	const unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(iFrame);

	const QString& sMarkerText
		= m_ui.MarkerTextLineEdit->text().simplified();
	const QColor& rgbMarkerColor
		= m_ui.MarkerTextLineEdit->palette().text().color();

	int iAccidentals = qtractorTimeScale::MinAccidentals;
	const int iIndex
		= m_ui.KeySignatureAccidentalsComboBox->currentIndex();
	if (iIndex > 0)
		iAccidentals = m_ui.KeySignatureAccidentalsComboBox->itemData(iIndex).toInt();
	const int iMode = m_ui.KeySignatureModeComboBox->currentIndex() - 1;

	if (pMarker && pMarker->frame == iFrame) {
		if (!sMarkerText.isEmpty())
			iFlags |= UpdateMarker;
		iFlags |= RemoveMarker;
		if (qtractorTimeScale::isKeySignature(iAccidentals, iMode))
			iFlags |= UpdateKeySignature;
	}

	if (pMarker
		&& pMarker->text == sMarkerText
		&& pMarker->color == rgbMarkerColor)
		iFlags &= ~UpdateMarker;
	else
	if (!sMarkerText.isEmpty())
		iFlags |=  AddMarker;

	if (pMarker
		&& pMarker->accidentals == iAccidentals
		&& pMarker->mode == iMode)
		iFlags &= ~UpdateKeySignature;
	else
	if (qtractorTimeScale::isKeySignature(iAccidentals, iMode))
		iFlags |=  AddKeySignature;

	if (pMarker && pMarker->frame == iFrame) {
		iFlags &= ~AddMarker;
		iFlags &= ~AddKeySignature;
	}

	return iFlags;
}


// Add node method.
void qtractorTimeScaleForm::addItem (void)
{
	if (m_pTimeScale == nullptr)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	const unsigned int  iFlags = flags();
	const unsigned long iFrame = frame();

	if (iFlags & AddNode) {
		pSession->execute(
			new qtractorTimeScaleAddNodeCommand(
				m_pTimeScale, iFrame,
				m_ui.TempoSpinBox->tempo(), 2,
				m_ui.TempoSpinBox->beatsPerBar(),
				m_ui.TempoSpinBox->beatDivisor()));
		++m_iDirtyTotal;
	}

	if (iFlags & AddMarker) {
		pSession->execute(
			new qtractorTimeScaleAddMarkerCommand(
				m_pTimeScale, iFrame,
				m_ui.MarkerTextLineEdit->text().simplified(),
				m_ui.MarkerTextLineEdit->palette().text().color()));
		++m_iDirtyTotal;
	}

	if (iFlags & AddKeySignature) {
		int iAccidentals = qtractorTimeScale::MinAccidentals;
		const int iIndex = m_ui.KeySignatureAccidentalsComboBox->currentIndex();
		if (iIndex > 0)
			iAccidentals = m_ui.KeySignatureAccidentalsComboBox->itemData(iIndex).toInt();
		const int iMode = m_ui.KeySignatureModeComboBox->currentIndex() - 1;
		if (qtractorTimeScale::isKeySignature(iAccidentals, iMode)) {
			pSession->execute(
				new qtractorTimeScaleAddKeySignatureCommand(
					m_pTimeScale, iFrame, iAccidentals, iMode));
			++m_iDirtyTotal;
		}
	}

	refresh();
}


// Update current node.
void qtractorTimeScaleForm::updateItem (void)
{
	if (m_pTimeScale == nullptr)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	const unsigned int   iFlags = flags();
	const unsigned short iBar   = bar();

	if (iFlags & UpdateNode) {
		qtractorTimeScale::Cursor cursor(m_pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);
		if (pNode && pNode->bar == iBar) {
			const unsigned long iFrame = pNode->frameFromBar(iBar);
			pSession->execute(
				new qtractorTimeScaleUpdateNodeCommand(
					m_pTimeScale, iFrame,
					m_ui.TempoSpinBox->tempo(), 2,
					m_ui.TempoSpinBox->beatsPerBar(),
					m_ui.TempoSpinBox->beatDivisor()));
			++m_iDirtyTotal;
		}
	}

	if (iFlags & UpdateMarker) {
		const unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
		qtractorTimeScale::Marker *pMarker
			= m_pTimeScale->markers().seekFrame(iFrame);
		if (pMarker && pMarker->frame == iFrame) {
			pSession->execute(
				new qtractorTimeScaleUpdateMarkerCommand(
					m_pTimeScale, iFrame,
					m_ui.MarkerTextLineEdit->text().simplified(),
					m_ui.MarkerTextLineEdit->palette().text().color()));
			++m_iDirtyTotal;
		}
	}

	if (iFlags & UpdateKeySignature) {
		const unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
		qtractorTimeScale::Marker *pMarker
			= m_pTimeScale->markers().seekFrame(iFrame);
		if (pMarker && pMarker->frame == iFrame) {
			int iAccidentals = qtractorTimeScale::MinAccidentals;
			const int iIndex = m_ui.KeySignatureAccidentalsComboBox->currentIndex();
			if (iIndex > 0)
				iAccidentals = m_ui.KeySignatureAccidentalsComboBox->itemData(iIndex).toInt();
			const int iMode = m_ui.KeySignatureModeComboBox->currentIndex() - 1;
			if (qtractorTimeScale::isKeySignature(iAccidentals, iMode)) {
				pSession->execute(
					new qtractorTimeScaleUpdateKeySignatureCommand(
						m_pTimeScale, iFrame, iAccidentals, iMode));
				++m_iDirtyTotal;
			}
		}
	}

	refresh();
}


// Remove current node.
void qtractorTimeScaleForm::removeItem (void)
{
	if (m_pTimeScale == nullptr)
		return;

	// Make it as an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	const unsigned int   iFlags = flags();
	const unsigned short iBar   = bar();

	if (iFlags & RemoveNode) {
		qtractorTimeScale::Cursor cursor(m_pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);
		if (pNode && pNode->bar == iBar && pNode->prev()) {
			// Prompt user if he/she's sure about this...
			qtractorOptions *pOptions = qtractorOptions::getInstance();
			if (pOptions && pOptions->bConfirmRemove) {
				// Show the warning...
				if (QMessageBox::warning(this,
					tr("Warning"),
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
			// Go!...
			pSession->execute(
				new qtractorTimeScaleRemoveNodeCommand(m_pTimeScale, pNode));
			++m_iDirtyTotal;
		}
	}

	if (iFlags & RemoveMarker) {
		const unsigned long iFrame = m_pTimeScale->frameFromBar(iBar);
		qtractorTimeScale::Marker *pMarker
			= m_pTimeScale->markers().seekFrame(iFrame);
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
	if (m_pTimeScale == nullptr)
		return;
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtySetup;

	if (iBar > 0) --iBar;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekBar(iBar);

	const unsigned long iFrame = (pNode ? pNode->frameFromBar(iBar) : 0);

	m_ui.TimeSpinBox->setValue(iFrame);

	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(iFrame);
	if (pMarker && pMarker->frame == iFrame)
		setCurrentMarker(pMarker);
	else
		setCurrentMarker(nullptr);
	if (pMarker && iFrame >= pMarker->frame)
		setCurrentKeySignature(pMarker);
	else
		setCurrentKeySignature(nullptr);

	m_iDirtySetup = 0;

	// Locate nearest list item...
	if (pNode)
		setCurrentItem(pNode, iFrame);
	ensureVisibleFrame(iFrame);

	++m_iDirtyCount;
	stabilizeForm();
}


void qtractorTimeScaleForm::timeChanged ( unsigned long iFrame )
{
	if (m_pTimeScale == nullptr)
		return;
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtySetup;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(iFrame);

	if (pNode) {
		iFrame = pNode->frameSnapToBar(iFrame);
		pNode = cursor.seekFrame(iFrame);
	}

	if (pNode)
		m_ui.BarSpinBox->setValue(pNode->barFromFrame(iFrame) + 1);

	qtractorTimeScale::Marker *pMarker
		= m_pTimeScale->markers().seekFrame(iFrame);
	if (pMarker && pMarker->frame == iFrame)
		setCurrentMarker(pMarker);
	else
		setCurrentMarker(nullptr);
	if (pMarker && iFrame >= pMarker->frame)
		setCurrentKeySignature(pMarker);
	else
		setCurrentKeySignature(nullptr);

	m_iDirtySetup = 0;

	// Locate nearest list item...
	if (pNode)
		setCurrentItem(pNode, iFrame);
	ensureVisibleFrame(iFrame);

	++m_iDirtyCount;
	stabilizeForm();
}


// Tempo/Time-signature has changed.
void qtractorTimeScaleForm::tempoChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::tempoChanged()");
#endif

	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;

	changed();
}


// Key signature has changed.
void qtractorTimeScaleForm::accidentalsChanged ( int iIndex )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::accidentalsChanged(%d)", iIndex);
#endif

	int iAccidentals = qtractorTimeScale::MinAccidentals;
	int iMode = -1;
	if (iIndex > 0) {
		iMode = m_ui.KeySignatureModeComboBox->currentIndex() - 1;
		if (iMode < 0)
			iMode = 0;
		iAccidentals = m_ui.KeySignatureAccidentalsComboBox->itemData(iIndex).toInt();
	}

	updateKeySignatures(iAccidentals, iMode);

	changed();
}


void qtractorTimeScaleForm::modeChanged ( int iMode )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::modeChanged(%d)", iMode);
#endif

	int iAccidentals = qtractorTimeScale::MinAccidentals;
	if (iMode > 0) {
		iAccidentals = 0;
		const int iIndex
			= m_ui.KeySignatureAccidentalsComboBox->currentIndex();
		if (iIndex > 0)
			iAccidentals = m_ui.KeySignatureAccidentalsComboBox->itemData(iIndex).toInt();
	}

	updateKeySignatures(iAccidentals, iMode - 1);

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
			tr("Warning"),
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


// Tempo factor perform click.
void qtractorTimeScaleForm::tempoFactor (void)
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return;

	const float fTempoFactor = float(m_ui.TempoFactorSpinBox->value());

	qtractorTimeScaleCommand *pTimeScaleCommand
		= new qtractorTimeScaleCommand(tr("tempo factor"));

	qtractorTimeScale::Node *pNode = m_pTimeScale->nodes().last();
	for ( ; pNode; pNode = pNode->prev()) {
		pTimeScaleCommand->addNodeCommand(
			new qtractorTimeScaleUpdateNodeCommand(
				m_pTimeScale, pNode->frame,
				fTempoFactor * pNode->tempo,
				pNode->beatType, pNode->beatsPerBar,
				pNode->beatDivisor));
	}

	if (pSession->execute(pTimeScaleCommand))
		++m_iDirtyTotal;

	refreshItems();
}


// Tempo tap click.
void qtractorTimeScaleForm::tempoTap (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::tempoTap()");
#endif

	const int iTimeTap = m_pTempoTap->restart();

	if (iTimeTap < 200 || iTimeTap > 2000) { // Magic!
		m_iTempoTap = 0;
		m_fTempoTap = 0.0f;
		return;
	}

	const float fTempoTap = ::rintf(60000.0f / float(iTimeTap));
#if 0
	m_fTempoTap += fTempoTap;
	if (++m_iTempoTap > 2) {
		m_fTempoTap /= float(m_iTempoTap);
		m_ui.TempoSpinBox->setTempo(::rintf(m_fTempoTap), false);
		m_iTempoTap	= 1; // Median-like averaging...
	}
#else
	if (m_fTempoTap  > 0.0f) {
		m_fTempoTap *= 0.5f;
		m_fTempoTap += 0.5f * fTempoTap;
	} else {
		m_fTempoTap  = fTempoTap;
	}
	if (++m_iTempoTap > 2) {
		m_ui.TempoSpinBox->setTempo(::rintf(m_fTempoTap), true);
		m_iTempoTap	 = 1; // Median-like averaging...
		m_fTempoTap  = fTempoTap;
	}
#endif
}


// Marker color selection.
void qtractorTimeScaleForm::markerColor (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTimeScaleForm::markerColor()");
#endif

	QPalette pal(m_ui.MarkerTextLineEdit->palette());

	QWidget *pParentWidget = nullptr;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	QColorDialog::ColorDialogOptions options;
	if (pOptions && pOptions->bDontUseNativeDialogs) {
		options |= QColorDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}

	const QColor& color	= QColorDialog::getColor(
		pal.text().color(), pParentWidget,
		tr("Marker Color"), options);

	if (color.isValid()) {
		pal.setColor(QPalette::Text, color);
		m_ui.MarkerTextLineEdit->setPalette(pal);
		if (pOptions)
			pOptions->sMarkerColor = color.name();
		changed();
	}
}


// Stabilize current form state.
void qtractorTimeScaleForm::stabilizeForm (void)
{
	const unsigned int iFlags = flags();
//	m_ui.RefreshPushButton->setEnabled(m_iDirtyCount > 0);
	m_ui.AddPushButton->setEnabled(iFlags & (AddNode | AddMarker | AddKeySignature));
	m_ui.UpdatePushButton->setEnabled(iFlags & (UpdateNode | UpdateMarker | UpdateKeySignature));
	m_ui.RemovePushButton->setEnabled(iFlags & (RemoveNode | RemoveMarker));
}


// Time-scale list view context menu handler.
void qtractorTimeScaleForm::contextMenu ( const QPoint& /*pos*/ )
{
	// Build the device context menu...
	QMenu menu(this);
	QAction *pAction;

	const unsigned int iFlags = flags();
	
	pAction = menu.addAction(
		QIcon::fromTheme("formAdd"),
		tr("&Add"), this, SLOT(addItem()));
	pAction->setEnabled(iFlags & (AddNode | AddMarker | AddKeySignature));

	pAction = menu.addAction(
		QIcon::fromTheme("formAccept"),
		tr("&Update"), this, SLOT(updateItem()));
	pAction->setEnabled(iFlags & (UpdateNode | UpdateMarker | UpdateKeySignature));

	pAction = menu.addAction(
		QIcon::fromTheme("formRemove"),
		tr("&Remove"), this, SLOT(removeItem()));
	pAction->setEnabled(iFlags & (RemoveNode | RemoveMarker));

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon::fromTheme("formRefresh"),
		tr("&Refresh"), this, SLOT(refresh()));
//	pAction->setEnabled(m_iDirtyCount > 0);

//	menu.exec(m_ui.BusListView->mapToGlobal(pos));
	menu.exec(QCursor::pos());
}


// end of qtractorTimeScaleForm.cpp
