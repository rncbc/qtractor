// qtractorTrackList.cpp
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

#include "qtractorAbout.h"
#include "qtractorTrackList.h"
#include "qtractorTrackView.h"
#include "qtractorTrackCommand.h"
#include "qtractorTrackButton.h"
#include "qtractorTracks.h"
#include "qtractorSession.h"
#include "qtractorMixer.h"

#include "qtractorOptions.h"
#include "qtractorInstrument.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorMainForm.h"

#include <QApplication>
#include <QPainter>
#include <QCursor>
#include <QPixmap>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>

#include <QAbstractListModel>
#include <QItemDelegate>
#include <QVBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QScrollBar>

#include <QRubberBand>


//----------------------------------------------------------------------------
// qtractorTrackListModel -- Track-list model.

class qtractorTrackListModel : public QAbstractListModel
{
public:

	// Constructor.
	qtractorTrackListModel(QObject *pParent = 0);
	// Destructor.
	~qtractorTrackListModel();

	int rowCount(const QModelIndex& parent = QModelIndex()) const;
	int columnCount(const QModelIndex& parent = QModelIndex()) const;

	QVariant headerData(int section, Qt::Orientation orient, int role) const;
	QVariant data(const QModelIndex& index, int role) const;

	int insertTrack(int iTrack, qtractorTrack *pTrack);
	int removeTrack(int iTrack);

	int trackRow(qtractorTrack *pTrack) const;

	void setTrack(int row, qtractorTrack *pTrack);
	qtractorTrack *track(int row) const;

	void updateTrack(qtractorTrack *pTrack);

	void clear();

private:

	// Model variables.
	QStringList m_headerText;

	// Model cache item.
	struct Item
	{
		// Constructor
		Item(qtractorTrack *pTrack = NULL)
			: track(pTrack) { update(); }
		// Item updater.
		void update();
		// Item members.
		qtractorTrack *track;
		QStringList    text;
	};

	// Model cache item list.
	QList<Item *> m_items;
};


//----------------------------------------------------------------------------
// qtractorTrackItemDelegate -- track-list view item delgate.

class qtractorTrackItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	qtractorTrackItemDelegate(QObject *pParent)
		: QItemDelegate(pParent) {}

	// Overridden paint method.
	void paint ( QPainter *pPainter, const QStyleOptionViewItem& option,
		const QModelIndex& index) const
	{
		const qtractorTrackListModel *pTrackModel
			= static_cast<const qtractorTrackListModel *> (index.model());
		qtractorTrack *pTrack = pTrackModel->track(index.row());
		if (pTrack == NULL)
			return;
		const bool bSelected = (option.state & QStyle::State_Selected);
		QColor bg, fg;
		if (index.column() == qtractorTrackList::Number) {
			bg = pTrack->foreground().light();
			fg = pTrack->background().light();
		} else if (bSelected) {
			bg = option.palette.color(QPalette::Midlight).dark(150);
			fg = option.palette.color(QPalette::Midlight).light(150);
		} else {
			bg = option.palette.color(QPalette::Window);
			fg = option.palette.color(QPalette::WindowText);
		}
		// Draw text and decorations if any...
		const QRect& rect = option.rect;
		QRect rectText(
			rect.topLeft() + QPoint(4, 4), rect.size() - QSize(8, 8));
		pPainter->save();
		pPainter->fillRect(rect, bg);
		pPainter->setPen(fg);
		if (index.column() == qtractorTrackList::Number ||
			index.column() == qtractorTrackList::Channel) {
			pPainter->drawText(rectText,
				Qt::AlignHCenter | Qt::AlignTop,
				QString::number(index.row() + 1));
		} else {
			if (index.column() == qtractorTrackList::Bus) {
				QPixmap pm = index.data(Qt::DecorationRole).value<QPixmap>();
				pPainter->drawPixmap(rectText.x(), rectText.y(), pm);
				rectText.setLeft(rectText.left() + pm.width() + 4);
			}
			pPainter->drawText(rectText,
				Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				index.data(Qt::DisplayRole).toString());
		}
		// Do some simple embossing...
		pPainter->setPen(bg.light(150));
		pPainter->drawLine(
			rect.left(), rect.top(), rect.left(), rect.bottom());
		pPainter->drawLine(
			rect.left(), rect.top(), rect.right(), rect.top());
		if (bSelected) {
			pPainter->setPen(bg.dark(150));
			pPainter->drawLine(
				rect.right(), rect.top(), rect.right(), rect.bottom());
			pPainter->drawLine(
				rect.left(), rect.bottom(), rect.right(), rect.bottom());
		}
		pPainter->restore();
	}
};



//----------------------------------------------------------------------------
// qtractorTrackItemWidget -- Track button layout widget.

class qtractorTrackItemWidget : public QWidget
{
//	Q_OBJECT

public:

	// Constructor.
	qtractorTrackItemWidget(qtractorTrackList *pTrackList,
		qtractorTrack *pTrack ) : QWidget(pTrackList)
	{
		QWidget::setBackgroundRole(QPalette::Window);

		QVBoxLayout *pVBoxLayout = new QVBoxLayout();
		pVBoxLayout->setMargin(4);
		pVBoxLayout->setSpacing(0);

		QHBoxLayout *pHBoxLayout = new QHBoxLayout();
		pHBoxLayout->setMargin(0);
		pHBoxLayout->setSpacing(4);

		const QSize buttonSize(22, 16);
		m_pRecordButton = new qtractorTrackButton(pTrack,
			qtractorTrack::Record, buttonSize, this);
		m_pMuteButton   = new qtractorTrackButton(pTrack,
			qtractorTrack::Mute, buttonSize, this);
		m_pSoloButton   = new qtractorTrackButton(pTrack,
			qtractorTrack::Solo, buttonSize, this);
	
		pHBoxLayout->addStretch();
		pHBoxLayout->addWidget(m_pRecordButton);
		pHBoxLayout->addWidget(m_pMuteButton);
		pHBoxLayout->addWidget(m_pSoloButton);

		pVBoxLayout->addStretch();
		pVBoxLayout->addLayout(pHBoxLayout);
		QWidget::setLayout(pVBoxLayout);

		qtractorTracks *pTracks = pTrackList->tracks();
		QObject::connect(m_pRecordButton,
			SIGNAL(trackButtonToggled(qtractorTrackButton *, bool)),
			pTracks, SLOT(trackButtonToggledSlot(qtractorTrackButton *, bool)));
		QObject::connect(m_pMuteButton,
			SIGNAL(trackButtonToggled(qtractorTrackButton *, bool)),
			pTracks, SLOT(trackButtonToggledSlot(qtractorTrackButton *, bool)));
		QObject::connect(m_pSoloButton,
			SIGNAL(trackButtonToggled(qtractorTrackButton *, bool)),
			pTracks, SLOT(trackButtonToggledSlot(qtractorTrackButton *, bool)));
	}

	// Public feedbacker.
	void updateTrack()
	{
		m_pRecordButton->updateTrack();
		m_pMuteButton->updateTrack();
		m_pSoloButton->updateTrack();
	}

private:

	// The local child widgets.
	qtractorTrackButton *m_pRecordButton;
	qtractorTrackButton *m_pMuteButton;
	qtractorTrackButton *m_pSoloButton;
};


//----------------------------------------------------------------------------
// qtractorTrackListModel -- Track-list model.

// Constructor.
qtractorTrackListModel::qtractorTrackListModel ( QObject *pParent )
	: QAbstractListModel(pParent)
{
	m_headerText
		<< tr("Nr")
		<< tr("Track Name")
		<< tr("Bus")
		<< tr("Ch")
		<< tr("Patch")
		<< tr("Instrument")
		<< QString::null;
};


// Destructor.
qtractorTrackListModel::~qtractorTrackListModel (void)
{
	qDeleteAll(m_items);
}


int qtractorTrackListModel::rowCount ( const QModelIndex& /*parent*/ ) const
{
	return m_items.count();
}


int qtractorTrackListModel::columnCount ( const QModelIndex& /*parent*/ ) const
{
	return m_headerText.count();
}


QVariant qtractorTrackListModel::headerData (
	int section, Qt::Orientation orient, int role ) const
{
	if (orient == Qt::Horizontal) {
		if (role == Qt::DisplayRole)
			return m_headerText.at(section);
		else
		if (role == Qt::TextAlignmentRole
			&& (section == qtractorTrackList::Number
				|| section == qtractorTrackList::Channel)) {
			return int(Qt::AlignHCenter | Qt::AlignVCenter);
		}
	}

	return QVariant();
}


QVariant qtractorTrackListModel::data (
	const QModelIndex& index, int role ) const
{
	if (role == Qt::DisplayRole) {
		if (index.column() > qtractorTrackList::Number) {
			return m_items.at(index.row())->text.at(index.column() - 1);
		} else {
			return QString(index.row() + 1);
		}
	}
	else
	if (role == Qt::DecorationRole
		&& index.column() == qtractorTrackList::Bus) {
		switch (track(index.row())->trackType()) {
			case qtractorTrack::Audio:
				return QPixmap(":/icons/trackAudio.png");
			case qtractorTrack::Midi:
				return QPixmap(":/icons/trackMidi.png");
			case qtractorTrack::None:
			default:
				break;
		}
	}

	return QVariant();
}


int qtractorTrackListModel::insertTrack ( int iTrack, qtractorTrack *pTrack )
{
	if (iTrack < 0)
		iTrack = m_items.count();

	QAbstractListModel::beginInsertRows(QModelIndex(), iTrack, iTrack);
	m_items.insert(iTrack, new Item(pTrack));
	QAbstractListModel::endInsertRows();

	return iTrack;
}


int qtractorTrackListModel::removeTrack ( int iTrack )
{
	if (iTrack < 0 || iTrack >= m_items.count())
		return -1;

	QAbstractListModel::beginRemoveRows(QModelIndex(), iTrack, iTrack);
	delete m_items.at(iTrack);
	m_items.removeAt(iTrack);
	QAbstractListModel::endRemoveRows();

	return (iTrack < m_items.count() ? iTrack : -1);
}


int qtractorTrackListModel::trackRow ( qtractorTrack *pTrack ) const
{
	int row = 0;

	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		if (pTrack == iter.next()->track)
			return row;
		++row;
	}

	return -1;
}


void qtractorTrackListModel::setTrack ( int row, qtractorTrack *pTrack )
{
	if (row < 0 || row >= m_items.count())
		return;

	Item *pItem = m_items.at(row);
	pItem->track = pTrack;
	pItem->update();
}

qtractorTrack *qtractorTrackListModel::track ( int row ) const
{
	if (row < 0 || row >= m_items.count())
		return NULL;

	return m_items.at(row)->track;
}


void qtractorTrackListModel::updateTrack ( qtractorTrack *pTrack )
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pTrack == pItem->track) {
			pItem->update();
			break;
		}
	}
}


void qtractorTrackListModel::clear (void)
{
	QAbstractListModel::beginRemoveRows(QModelIndex(), 0, m_items.count() - 1);
	qDeleteAll(m_items);
	m_items.clear();
	QAbstractListModel::endRemoveRows();
}


// Track-list model item cache updater.
void qtractorTrackListModel::Item::update (void)
{
	text.clear();

	// Default initialization?
	if (track == NULL)
		return;

	// Track name...
	text << track->trackName();

	const QString s = " - -";

	QString sBusText = track->inputBusName();
	if (track->inputBusName() != track->outputBusName())
		sBusText += '/' + track->outputBusName();

	switch (track->trackType()) {

		case qtractorTrack::Audio: {
			// Audio Bus name...
			text << sBusText + '\n' + QObject::tr("Audio");
			// Audio channels...
			qtractorAudioBus *pAudioBus
				= static_cast<qtractorAudioBus *> (track->outputBus());
			text << (pAudioBus ?
				QString::number(pAudioBus->channels()) : s.right(1));
			// Fillers...
			text << s << s;
			break;
		}

		case qtractorTrack::Midi: {
			// MIDI Bus name...
			text << sBusText + '\n' + QObject::tr("MIDI");
			qtractorMidiBus *pMidiBus
				= static_cast<qtractorMidiBus *> (track->outputBus());
			// Audio channels...
			unsigned short iChannel = track->midiChannel();
			text << QString::number(iChannel + 1);
			// Care of MIDI instrument, program and bank numbers vs.names...
			QString sInstrument = s;
			QString sProgram = s;
			QString sBank;
			if (track->midiProgram() >= 0)
				sProgram = QString::number(track->midiProgram()) + s;
			if (track->midiBank() >= 0)
				sBank = QString::number(track->midiBank());
			if (pMidiBus) {
				qtractorMainForm *pMainForm
					= qtractorMainForm::getInstance();
				const qtractorMidiBus::Patch& patch
					= pMidiBus->patch(iChannel);
				if (!patch.instrumentName.isEmpty()
					&& pMainForm && pMainForm->instruments()) {
					sInstrument = patch.instrumentName;
					qtractorInstrument& instr
						= (*pMainForm->instruments())[patch.instrumentName];
					qtractorInstrumentData& bank
						= instr.patch(track->midiBank());
					if (bank.contains(track->midiProgram())) {
						sProgram = bank[track->midiProgram()];
						sBank = bank.name();
					}
				}
			}
			// This is it, MIDI Patch/Bank...
			text << sProgram + '\n' + sBank << sInstrument;
			break;
		}

		case qtractorTrack::None:
		default: {
			text << s + '\n' + QObject::tr("Unknown") << s << s << s;
			break;
		}
	}
	
	// Final empty dummy-stretch column...
	text << QString::null;
}


//----------------------------------------------------------------------------
// qtractorTrackList -- Track list widget.

// Constructor.
qtractorTrackList::qtractorTrackList (
	qtractorTracks *pTracks, QWidget *pParent )
	: QTableView(pParent)
{
	m_pTracks = pTracks;

	m_dragState  = DragNone;
	m_iDragTrack = -1;
	m_iDragY     = 0;

	m_pRubberBand = NULL;

	m_iContentsYMoving = 0;

	m_pListModel = new qtractorTrackListModel(this);
	QTableView::setModel(m_pListModel);
	QTableView::setItemDelegate(new qtractorTrackItemDelegate(this));
	QTableView::setSelectionMode(QAbstractItemView::SingleSelection);
	QTableView::setSelectionBehavior(QAbstractItemView::SelectRows);
	QTableView::setMouseTracking(true);

//	QTableView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	QTableView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QTableView::verticalHeader()->hide();

	QHeaderView *pHeader = QTableView::horizontalHeader();
	pHeader->setHighlightSections(false);
	pHeader->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	pHeader->setStretchLastSection(true);
	pHeader->setClickable(false);
	
	pHeader->resizeSection(Number, 26);
	pHeader->resizeSection(Name, 120);
	pHeader->resizeSection(Channel, 22);

	QPalette pal(QTableView::palette());
	pal.setColor(QPalette::Base, pal.color(QPalette::Dark));
	pal.setColor(QPalette::Highlight, pal.color(QPalette::Mid).dark(120));
	pal.setColor(QPalette::HighlightedText, pal.color(QPalette::Midlight));
	QTableView::setPalette(pal);

	// Simple double-click handling...
	QObject::connect(this,
		SIGNAL(doubleClicked(const QModelIndex&)),
		SLOT(doubleClickedSlot(const QModelIndex&)));
	// Vertical position handling...
	QObject::connect(QTableView::verticalScrollBar(),
		SIGNAL(valueChanged(int)),
		SLOT(contentsYChangedSlot()));
}


// Main tracks widget accessor.
qtractorTracks *qtractorTrackList::tracks (void) const
{
	return m_pTracks;
}


// Find the list view item from track pointer reference.
int qtractorTrackList::trackRow ( qtractorTrack *pTrack ) const
{
	return m_pListModel->trackRow(pTrack);
}


// Find the track pointer reference from list view item row.
qtractorTrack *qtractorTrackList::track ( int iTrack ) const
{
	return m_pListModel->track(iTrack);
}


// Insert a track item; return actual track row added.
int qtractorTrackList::insertTrack ( int iTrack, qtractorTrack *pTrack )
{
	iTrack = m_pListModel->insertTrack(iTrack, pTrack);
	if (iTrack >= 0) {
		const QModelIndex& index = m_pListModel->index(iTrack, Name);
		if (index.isValid()) {
			QTableView::setRowHeight(iTrack, pTrack->zoomHeight());
			QTableView::setIndexWidget(index,
				new qtractorTrackItemWidget(this, pTrack));
		}
	}
	return iTrack;
}


// Remove a track item; return remaining track row.
int qtractorTrackList::removeTrack ( int iTrack )
{
	return m_pListModel->removeTrack(iTrack);
}


// Select a track item.
void qtractorTrackList::selectTrack ( int iTrack )
{
	QTableView::setCurrentIndex(
		m_pListModel->index(iTrack,	QTableView::currentIndex().column()));
}


// Retrieves current selected track reference.
qtractorTrack *qtractorTrackList::currentTrack (void) const
{
	return m_pListModel->track(QTableView::currentIndex().row());
}


// Find the list view item from track pointer reference.
void qtractorTrackList::updateTrack ( qtractorTrack *pTrack )
{
	m_pListModel->updateTrack(pTrack);

	int iTrack = m_pListModel->trackRow(pTrack);
	if (iTrack < 0)
		return;

	const QModelIndex& index =  m_pListModel->index(iTrack, Name);
	qtractorTrackItemWidget *pTrackItemWidget
		= static_cast<qtractorTrackItemWidget *> (QTableView::indexWidget(index));
	if (pTrackItemWidget)
		pTrackItemWidget->updateTrack();
}


// Main table cleaner.
void qtractorTrackList::clear (void)
{
	m_pListModel->clear();
}


// Zoom all tracks item height.
void qtractorTrackList::updateZoomHeight (void)
{
	qtractorSession *pSession = m_pTracks->session();
	if (pSession == NULL)
		return;

	int iTrack = 0;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		pTrack->updateZoomHeight();
		QTableView::setRowHeight(iTrack, pTrack->zoomHeight());
		pTrack = pTrack->next();
		iTrack++;
	}

	// Update track view total contents height...
	m_pTracks->trackView()->updateContentsHeight();
	m_pTracks->trackView()->updateContents();
}


// Handle item height resizing and track move with mouse.
void qtractorTrackList::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	QTableView::mousePressEvent(pMouseEvent);

	if (m_dragState == DragNone) {
		// Look for the mouse hovering around some item boundary...
		QModelIndex index = QTableView::indexAt(pMouseEvent->pos());
		if (index.isValid()) {
			m_posDrag = pMouseEvent->pos();
			if (m_iDragTrack >= 0) {
				m_dragState  = DragResize;
			} else {
				m_iDragTrack = index.row();
				m_iDragY     = 0;
				m_dragState  = DragStart;
				QTableView::setCursor(QCursor(Qt::PointingHandCursor));
			}
		}
	}
}


void qtractorTrackList::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// We're already on some item dragging/resizing?...
	switch (m_dragState) {
	case DragMove:
		// Currently moving an item...
		if (m_iDragTrack >= 0) {
			const QModelIndex& index = QTableView::indexAt(pMouseEvent->pos());
			if (index.isValid()) {
				m_posDrag = QTableView::visualRect(index).topLeft();
				moveRubberBand(m_posDrag);
			}
		}
		break;
	case DragResize:
		// Currently resizing an item...
		if (m_iDragTrack >= 0) {
			int y = pMouseEvent->y();
			if (y < m_iDragY + ItemHeightMin)
				y = m_iDragY + ItemHeightMin;
			m_posDrag.setY(y);
			moveRubberBand(m_posDrag);
		}
		break;
	case DragStart:
		// About to start dragging an item...
		if (m_iDragTrack >= 0
			&& (m_posDrag - pMouseEvent->pos()).manhattanLength()
				> QApplication::startDragDistance()) {
			QTableView::setCursor(QCursor(Qt::SizeVerCursor));
			m_dragState = DragMove;
			m_posDrag = QTableView::visualRect(
				m_pListModel->index(m_iDragTrack, 0)).topLeft();
			moveRubberBand(m_posDrag);
		}
		break;
	case DragNone: {
		// Look for the mouse hovering around first column item boundary...
		QModelIndex index = QTableView::indexAt(pMouseEvent->pos());
		if (index.isValid() && index.column() == Number) {
			m_posDrag  = pMouseEvent->pos();
			QRect rect = QTableView::visualRect(index);
			if (pMouseEvent->y() >= rect.top()
				&& pMouseEvent->y() < rect.top() + 4) {
				index = m_pListModel->index(index.row() - 1, index.column());
				if (index.isValid()) {
					if (m_iDragTrack < 0)
						QTableView::setCursor(QCursor(Qt::SplitVCursor));
					m_iDragTrack = index.row();
					m_iDragY = QTableView::visualRect(index).top();
					return;
				}
			} else if (pMouseEvent->y() >= rect.bottom() - 4
				&& pMouseEvent->y() < rect.bottom() + 4) {
				if (m_iDragTrack < 0)
					QTableView::setCursor(QCursor(Qt::SplitVCursor));
				m_iDragTrack = index.row();
				m_iDragY = rect.top();
				return;
			}
		}
		// If something has been going on, turn it off...
 		if (m_iDragTrack >= 0) {
			QTableView::unsetCursor();
			m_iDragTrack = -1;
			m_iDragY = 0;
		}
		// Bail out, avoid default nasty
		// mouse hovering behavior...
		return;
	}
	default:
		break;
	}

	QTableView::mouseMoveEvent(pMouseEvent);
}


void qtractorTrackList::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QTableView::mouseReleaseEvent(pMouseEvent);

	// We'll need a reference for issuing commands...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;
	qtractorSession *pSession = pMainForm->session();
	if (pSession == NULL)
		return;

	// If we were resizing, now's time to let
	// things know that have changed somehow...
	switch (m_dragState) {
	case DragMove:
		if (m_iDragTrack >= 0) {
			const QModelIndex& index = QTableView::indexAt(pMouseEvent->pos());
			if (index.isValid()) {
				qtractorTrack *pTrackDrag = m_pListModel->track(m_iDragTrack);
				qtractorTrack *pTrackDrop = m_pListModel->track(index.row());
				if (pTrackDrag && pTrackDrop
					&& pTrackDrag != pTrackDrop
					&& pTrackDrag != pTrackDrop->prev()) {
					pMainForm->commands()->exec(
						new qtractorMoveTrackCommand(pMainForm,
							pTrackDrag, pTrackDrop));
				}
			}
		}
		break;
	case DragResize:
		if (m_iDragTrack >= 0) {
			int iZoomHeight = pMouseEvent->y() - m_iDragY;
			// Check for minimum item height.
			if (iZoomHeight < ItemHeightMin)
				iZoomHeight = ItemHeightMin;
			// Go for it...
			qtractorTrack *pTrack = m_pListModel->track(m_iDragTrack);
			if (pTrack) {
				pMainForm->commands()->exec(
					new qtractorResizeTrackCommand(pMainForm,
						pTrack, iZoomHeight));
			}
		}
		// Fall thru...
	case DragStart:
	case DragNone:
	default:
		break;
	}

	resetDragState();
}


// Draw a dragging separator line.
void qtractorTrackList::moveRubberBand ( const QPoint& posDrag )
{

	// Create the rubber-band if there's none...
	if (m_pRubberBand == NULL) {
		m_pRubberBand = new QRubberBand(
			QRubberBand::Line, QTableView::viewport());
		QPalette pal(m_pRubberBand->palette());
		pal.setColor(m_pRubberBand->foregroundRole(), Qt::blue);
		m_pRubberBand->setPalette(pal);
		m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	}
	
	// Just move it
	m_pRubberBand->setGeometry(
		QRect(0, posDrag.y(), QTableView::viewport()->width(), 4));

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Reset drag/select/move state.
void qtractorTrackList::resetDragState (void)
{
	// Cancel any dragging out there...
	// Just hide the rubber-band...
	if (m_pRubberBand)
		m_pRubberBand->hide();

	// Should fallback mouse cursor...
	if (m_dragState != DragNone || m_iDragTrack >= 0)
		QTableView::unsetCursor();

	// Not dragging anymore.
	m_dragState  = DragNone;
	m_iDragTrack = -1;
	m_iDragY     = 0;
}


// Keyboard event handler.
void qtractorTrackList::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG
	fprintf(stderr, "qtractorTrackList::keyPressEvent(key=%d)\n", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
		resetDragState();
		break;
	default:
		QTableView::keyPressEvent(pKeyEvent);
		break;
	}
}


// Context menu event handler.
void qtractorTrackList::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	// We'll need a reference for issuing commands...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->trackMenu()->exec(pContextMenuEvent->globalPos());
}


// Trap current index changes.
void qtractorTrackList::currentChanged ( const QModelIndex& curr,
	const QModelIndex& prev )
{
	QTableView::currentChanged(curr, prev);

	// We take care only on row changes...
	if (curr.row() != prev.row())
		emit selectionChanged();
}


// To have dircet access to track properties.
void qtractorTrackList::doubleClickedSlot ( const QModelIndex& /*index*/ )
{
	// We'll need a reference for issuing commands...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->trackProperties();
}


// Trap size changes.
void qtractorTrackList::resizeEvent ( QResizeEvent *pResizeEvent )
{
	QTableView::resizeEvent(pResizeEvent);

	contentsYChangedSlot();
}


// Vertical offset position change slot.
void qtractorTrackList::contentsYChangedSlot (void)
{
	if (m_iContentsYMoving > 0)
		return;

	m_iContentsYMoving++;
	m_pTracks->trackView()->verticalScrollBar()->setSliderPosition(
		QTableView::verticalOffset());
	m_iContentsYMoving--;
}


// To have track list in v-sync with main track view.
void qtractorTrackList::contentsYMovingSlot ( int /*cx*/, int cy )
{
	if (m_iContentsYMoving > 0)
		return;

	int iTrackViewMax = m_pTracks->trackView()->verticalScrollBar()->maximum();
	if (iTrackViewMax > 0) {
		m_iContentsYMoving++;
		int iTrackListMax = QTableView::verticalScrollBar()->maximum();
		QTableView::verticalScrollBar()->setSliderPosition(
			(cy * iTrackListMax) / iTrackViewMax);
		m_iContentsYMoving--;
	} else {
		QTableView::verticalScrollBar()->setSliderPosition(0);
	}
}


// end of qtractorTrackList.cpp
