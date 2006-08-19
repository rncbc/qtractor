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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

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

#include <qapplication.h>
#include <qpopupmenu.h>
#include <qtooltip.h>
#include <qpainter.h>
#include <qcursor.h>
#include <qheader.h>
#include <qpen.h>


// Base item height (in pixels).
#define QTRACTOR_ITEM_HEIGHT		48
#define QTRACTOR_ITEM_HEIGHT_MIN	(QTRACTOR_ITEM_HEIGHT >> 1)


//----------------------------------------------------------------------------
// qtractorTrackListItem -- Tracks list item.

// Constructors.
qtractorTrackListItem::qtractorTrackListItem ( qtractorTrackList *pTrackList,
	qtractorTrack *pTrack )
	: QListViewItem(pTrackList, pTrackList->lastItem())
{
	initItem(pTrackList, pTrack);
}

qtractorTrackListItem::qtractorTrackListItem ( qtractorTrackList *pTrackList,
	qtractorTrack *pTrack, QListViewItem *pItemAfter )
	: QListViewItem(pTrackList, pItemAfter)
{
	initItem(pTrackList, pTrack);
}


// Destructor.
qtractorTrackListItem::~qtractorTrackListItem (void)
{
	delete m_pRecordButton;
	delete m_pMuteButton;
	delete m_pSoloButton;
}


// Common item initializer.
void qtractorTrackListItem::initItem ( qtractorTrackList *pTrackList,
	qtractorTrack *pTrack )
{
	m_pTrack = pTrack;

	QListViewItem::setMultiLinesEnabled(true);

	// FIXME: Track number's prone to confusion...
	int iTrackNumber = pTrackList->childCount();
	setText(qtractorTrackList::Number, QString::number(iTrackNumber));
	setText(qtractorTrackList::Name, pTrack->trackName());
	setText(qtractorTrackList::Bus, pTrack->inputBusName());
	// qtractorTrackList::Channel
	// qtractorTrackList::Patch
	// qtractorTrackList::Instrument

	const QSize buttonSize(22, 16);
	m_pRecordButton = new qtractorTrackButton(m_pTrack,
		qtractorTrack::Record, buttonSize, pTrackList->viewport());
	m_pMuteButton   = new qtractorTrackButton(m_pTrack,
		qtractorTrack::Mute, buttonSize, pTrackList->viewport());
	m_pSoloButton   = new qtractorTrackButton(m_pTrack,
		qtractorTrack::Solo, buttonSize, pTrackList->viewport());

	pTrackList->addChild(m_pRecordButton);
	pTrackList->addChild(m_pMuteButton);
	pTrackList->addChild(m_pSoloButton);

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


// Tool widget item updater.
void qtractorTrackListItem::updateItem ( bool bShow )
{
	// Just hide every child tool widget?
	if (!bShow) {
		m_pRecordButton->hide();
		m_pMuteButton->hide();
		m_pSoloButton->hide();
		return;
	}

	
	QListView *pListView = QListViewItem::listView();
	int x = pListView->header()->sectionRect(qtractorTrackList::Name).right();
	int y = pListView->itemRect(this).bottom() - 22;
	if (y >= 0) {
		// Move to proper positioning layout...
		x -= m_pSoloButton->width() + 4;
		m_pSoloButton->move(x, y);
		x -= m_pMuteButton->width() + 4;
		m_pMuteButton->move(x, y);
		x -= m_pRecordButton->width() + 4;
		m_pRecordButton->move(x, y);
		// Show up those buttons...
		m_pMuteButton->show();
		m_pSoloButton->show();
		m_pRecordButton->show();
	}
}


// Track list brainless accessor.
qtractorTrackList *qtractorTrackListItem::trackList (void) const
{
	return static_cast<qtractorTrackList *> (QListViewItem::listView());
}


// Track container accessor.
qtractorTrack *qtractorTrackListItem::track (void) const
{
	return m_pTrack;
}


// Update track buttons state.
void qtractorTrackListItem::updateTrackButtons (void)
{
	m_pRecordButton->updateTrack();
	m_pMuteButton->updateTrack();
	m_pSoloButton->updateTrack();
}


// Overriden to set extra text info.
void qtractorTrackListItem::setText ( int iColumn, const QString& sText )
{
	if (m_pTrack == NULL || iColumn != qtractorTrackList::Bus) {
		QListViewItem::setText(iColumn, sText);
		return;
	}

	const QString s = " - -";
	QString sBusText;

	switch (m_pTrack->trackType()) {

		case qtractorTrack::Audio: {
			qtractorAudioBus *pAudioBus;
			pAudioBus = static_cast<qtractorAudioBus *> (m_pTrack->inputBus());
			QListViewItem::setPixmap(qtractorTrackList::Bus,
				QPixmap::fromMimeSource("trackAudio.png"));
			sBusText = (pAudioBus ? pAudioBus->busName() : s);
			pAudioBus = static_cast<qtractorAudioBus *> (m_pTrack->outputBus());
			if (m_pTrack->inputBus() != m_pTrack->outputBus())
				sBusText += '/' + (pAudioBus ? pAudioBus->busName() : s);
			QListViewItem::setText(qtractorTrackList::Bus,
				sBusText + '\n' + QObject::tr("Audio"));
			QListViewItem::setText(qtractorTrackList::Channel,
				pAudioBus ? QString::number(pAudioBus->channels()) : s.left(1));
			QListViewItem::setText(qtractorTrackList::Patch, s);
			QListViewItem::setText(qtractorTrackList::Instrument, s);
			break;
		}

		case qtractorTrack::Midi: {
			qtractorMidiBus *pMidiBus;
			pMidiBus = static_cast<qtractorMidiBus *> (m_pTrack->inputBus());
			QListViewItem::setPixmap(qtractorTrackList::Bus,
				QPixmap::fromMimeSource("trackMidi.png"));
			sBusText = (pMidiBus ? pMidiBus->busName() : s);
			pMidiBus = static_cast<qtractorMidiBus *> (m_pTrack->outputBus());
			if (m_pTrack->inputBus() != m_pTrack->outputBus())
				sBusText += '/' + (pMidiBus ? pMidiBus->busName() : s);
			QListViewItem::setText(qtractorTrackList::Bus,
				sBusText + '\n' + QObject::tr("MIDI"));
			unsigned short iChannel = m_pTrack->midiChannel();
			QListViewItem::setText(qtractorTrackList::Channel,
				QString::number(iChannel + 1));
			// Care of MIDI instrument, program and bank numbers vs.names...
			QString sInstrument = s;
			QString sProgram = s;
			QString sBank;
			if (m_pTrack->midiProgram() >= 0)
				sProgram = QString::number(m_pTrack->midiProgram()) + s;
			if (m_pTrack->midiBank() >= 0)
				sBank = QString::number(m_pTrack->midiBank());
			if (pMidiBus) {
				const qtractorMidiBus::Patch& patch
					= pMidiBus->patch(iChannel);
				if (!patch.instrumentName.isEmpty()
					&& trackList()->instruments()) {
					sInstrument = patch.instrumentName;
					qtractorInstrument& instr
						= (*trackList()->instruments())[patch.instrumentName];
					qtractorInstrumentData& bank
						= instr.patch(m_pTrack->midiBank());
					if (bank.contains(m_pTrack->midiProgram())) {
						sProgram = bank[m_pTrack->midiProgram()];
						sBank = bank.name();
					}
				}
			}
			QListViewItem::setText(qtractorTrackList::Patch,
				sProgram + '\n' + sBank);
			QListViewItem::setText(qtractorTrackList::Instrument, sInstrument);
			break;
		}

		case qtractorTrack::None:
		default: {
			const QString sText  = s + '\n' + QObject::tr("Unknown");
			QListViewItem::setText(qtractorTrackList::Bus, sText);
			QListViewItem::setText(qtractorTrackList::Channel, s);
			QListViewItem::setText(qtractorTrackList::Patch, s);
			QListViewItem::setText(qtractorTrackList::Instrument, s);
			break;
		}
	}
}


// Set track item height.
void qtractorTrackListItem::setItemHeight ( int iItemHeight )
{
	updateItem(false);
	QListViewItem::setHeight(iItemHeight);

	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	int iHeight = (iItemHeight * 100) / pSession->verticalZoom();
	m_pTrack->setHeight(iHeight);

	// Update all remaining items...
	qtractorTrackListItem *pItem
		= static_cast<qtractorTrackListItem *> (nextSibling());
	while (pItem) {
		pItem->updateItem(false);
		pItem = static_cast<qtractorTrackListItem *> (pItem->nextSibling());
	}
}


// Zoom item track's height.
void qtractorTrackListItem::zoomItemHeight ( unsigned short iVerticalZoom )
{
	if (m_pTrack == NULL)
		return;

	int iItemHeight = (m_pTrack->height() * iVerticalZoom) / 100;
	if (iItemHeight > QTRACTOR_ITEM_HEIGHT_MIN)
		QListViewItem::setHeight(iItemHeight);
}


// Overriden view item setup .
void qtractorTrackListItem::setup (void)
{
	QListViewItem::setup();

	if (m_pTrack == NULL)
		return;

	qtractorSession *pSession = m_pTrack->session();
	if (pSession == NULL)
		return;

	int iItemHeight = (m_pTrack->height() * pSession->verticalZoom()) / 100;
	QListViewItem::setHeight(iItemHeight);
}


// Overrriden cell painter.
void qtractorTrackListItem::paintCell ( QPainter *p, const QColorGroup& cg,
		int column, int width, int align )
{
	// Paint the original but with a different background...
	QColorGroup _cg(cg);

	QColor bg = _cg.color(QColorGroup::Background);
	QColor fg = _cg.color(QColorGroup::Foreground);

	bool bSelected = isSelected();
	if (column > qtractorTrackList::Number && bSelected) {
		bg = _cg.color(QColorGroup::Midlight).dark(150);
		fg = _cg.color(QColorGroup::Midlight).light(150);
	}

	switch (column) {
	case qtractorTrackList::Number:
		bg = m_pTrack->foreground().light();
		fg = m_pTrack->background().light();
		break;
	case qtractorTrackList::Name:
		updateItem(true);
		break;
	}

	_cg.setColor(QColorGroup::Base, bg);
	_cg.setColor(QColorGroup::Text, fg);
	_cg.setColor(QColorGroup::Highlight, bg);
	_cg.setColor(QColorGroup::HighlightedText, fg);

	QListViewItem::paintCell(p, _cg, column, width, align);

	// Draw cell frame lines...
	int height = QListViewItem::height();
	p->setPen(bg.light(150));
	p->drawLine(0, 0, 0, height - 1);
	p->drawLine(0, 0, width - 1, 0);
	p->setPen(bg.dark(150));
	p->drawLine(width - 1, 0, width - 1, height - 1);
	p->drawLine(0, height - 1, width - 1, height - 1);
}


//----------------------------------------------------------------------------
// qtractorTrackList -- Track list widget.

// Constructor.
qtractorTrackList::qtractorTrackList ( qtractorTracks *pTracks,
	QWidget *pParent, const char *pszName )
	: QListView(pParent, pszName, WStaticContents | WNoAutoErase)
{
	m_pTracks = pTracks;

	m_dragState  = DragNone;
	m_pItemDrag  = NULL;
	m_iItemDragY = 0;

//	QListView::header()->setClickEnabled(false);
//	QListView::header()->setFixedHeight(QTRACTOR_ITEM_HEIGHT);

	QListView::viewport()->setPaletteBackgroundColor(Qt::darkGray);
	//	QListView::colorGroup().color(QColorGroup::Background));

	QListView::addColumn(tr("Nr"), 26);		// qtractorTrackList::Number
	QListView::addColumn(tr("Track Name"), 120);	// qtractorTrackList::Name
	QListView::addColumn(tr("Bus"));		// qtractorTrackList::Bus
	QListView::addColumn(tr("Ch"), 26);		// qtractorTrackList::Channel
	QListView::addColumn(tr("Patch"));		// qtractorTrackList::Patch
	QListView::addColumn(tr("Instrument"));	// qtractorTrackList::Instrumnet

	QListView::setColumnAlignment(qtractorTrackList::Number, Qt::AlignHCenter);
	QListView::setColumnAlignment(qtractorTrackList::Bus, Qt::AlignTop);
	QListView::setColumnAlignment(qtractorTrackList::Channel, Qt::AlignHCenter);

	QListView::setColumnWidthMode(qtractorTrackList::Name, QListView::Manual);
	QListView::setColumnWidthMode(qtractorTrackList::Channel, QListView::Manual);

//	QListView::setResizeMode(QListView::LastColumn);
	QListView::setSelectionMode(QListView::Single);
	QListView::setAllColumnsShowFocus(true);
	QListView::setItemMargin(4);
	// No sorting column.
	QListView::setSortColumn(-1);
	QListView::setHScrollBarMode(QListView::AlwaysOn);
	QListView::setVScrollBarMode(QListView::AlwaysOff);

	// Simple click handling...
	QObject::connect(
		this, SIGNAL(clicked(QListViewItem*,const QPoint&,int)),
		this, SLOT(clickedSlot(QListViewItem*,const QPoint&,int)));
	// Context menu handling...
	QObject::connect(
		this, SIGNAL(contextMenuRequested(QListViewItem*,const QPoint&,int)),
		this, SLOT(contextMenuSlot(QListViewItem*,const QPoint&,int)));
}


// Main tracks widget accessor.
qtractorTracks *qtractorTrackList::tracks (void) const
{
	return m_pTracks;
}


// Find the list view item from track pointer reference.
qtractorTrackListItem *qtractorTrackList::trackItem ( qtractorTrack *pTrack )
{
	if (pTrack == NULL)
		return NULL;

	QListViewItem *pItem = QListView::firstChild();
	while (pItem) {
		qtractorTrackListItem *pTrackItem
			= static_cast<qtractorTrackListItem *> (pItem);
		if (pTrackItem->track() == pTrack)
			return pTrackItem;
		pItem = pItem->nextSibling();
	}

	return NULL;
}


// Renumber track list items.
void qtractorTrackList::renumberTrackItems ( QListViewItem *pItem )
{
	// Gotta start from somewhere...
	int iTrackNumber = 0;
	if (pItem == NULL) {
		pItem = QListView::firstChild();
	} else {
		iTrackNumber += pItem->text(qtractorTrackList::Number).toInt();
		pItem = pItem->nextSibling();
	}

	// Renumbering of all other remaining items.
	while (pItem) {
		pItem->setText(qtractorTrackList::Number,
			QString::number(++iTrackNumber));
		pItem = pItem->nextSibling();
	}
}


// Zoom all tracks item height.
void qtractorTrackList::zoomItemHeight ( int iVerticalZoom )
{
	QListViewItem *pItem = QListView::firstChild();
	while (pItem) {
		qtractorTrackListItem *pTrackItem
			= static_cast<qtractorTrackListItem *> (pItem);
		if (pTrackItem)
			pTrackItem->zoomItemHeight(iVerticalZoom);
		pItem = pItem->nextSibling();
	}

	// Update track view total contents height...
	m_pTracks->trackView()->updateContentsHeight();
	m_pTracks->trackView()->updateContents();
}


// Instrument list accessor helper.
qtractorInstrumentList *qtractorTrackList::instruments() const
{
	return m_pTracks->instruments();
}


// Trap this widget width for remembering it later.
void qtractorTrackList::resizeEvent ( QResizeEvent *pResizeEvent )
{
	// One shoukld only remember visible sizes.
	if (QListView::isVisible()) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorOptions *pOptions = pMainForm->options();
			if (pOptions)
				pOptions->iTrackListWidth = QListView::size().width();
		}
	}

	QListView::resizeEvent(pResizeEvent);
}


// Overriden to catch early attributes (e.g. header height)
void qtractorTrackList::polish (void)
{
	// Highly recommended (avoid recursion?)
	QListView::polish();

	// Make sure header is polished by now too.
	QListView::header()->polish();

	emit polishNotifySignal();
}


// Handle item height resizing with mouse.
void qtractorTrackList::contentsMousePressEvent ( QMouseEvent *pMouseEvent )
{
	if (m_dragState == DragNone) {
		m_posDrag = pMouseEvent->pos();
		if (m_pItemDrag) {
			QListView::setCursor(QCursor(Qt::SplitVCursor));
			m_dragState = DragResize;
			drawDragLine(m_posDrag);    // Show.
		} else {
			QListView::contentsMousePressEvent(pMouseEvent);
			m_pItemDrag = QListView::itemAt(
				QListView::contentsToViewport(m_posDrag));
			if (m_pItemDrag) {
				QListView::setCursor(QCursor(Qt::PointingHandCursor));
				m_dragState = DragStart;
			}
		}
	}
}


void qtractorTrackList::contentsMouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	// We're already on some item dragging/resizing?...
	switch (m_dragState) {
	case DragMove:
		if (m_pItemDrag) {
			drawDragLine(m_posDrag);	// Hide.
			QListViewItem *pItemDrop = QListView::itemAt(
				QListView::contentsToViewport(pMouseEvent->pos()));
			if (pItemDrop) {
				m_posDrag = QListView::viewportToContents(
					QListView::itemRect(pItemDrop).bottomLeft());
			}
			drawDragLine(m_posDrag);	// Show.
		}
		break;
	case DragResize:
		// Currently resizing an item...
		if (m_pItemDrag) {
			drawDragLine(m_posDrag);    // Hide.
			int y = pMouseEvent->y();
			if (y < m_iItemDragY + QTRACTOR_ITEM_HEIGHT_MIN)
				y = m_iItemDragY + QTRACTOR_ITEM_HEIGHT_MIN;
			m_posDrag.setY(y);
			drawDragLine(m_posDrag);    // Show.
		}
		break;
	case DragStart:
		// About to start dragging an item...
		if (m_pItemDrag && (m_posDrag - pMouseEvent->pos()).manhattanLength()
				> QApplication::startDragDistance()) {
			QListView::setCursor(QCursor(Qt::SizeVerCursor));
			m_dragState = DragMove;
			m_posDrag = QListView::viewportToContents(
				QListView::itemRect(m_pItemDrag).bottomLeft());
			drawDragLine(m_posDrag);	// Show.
		}
		break;
	case DragNone:
	default:
		// Look for the mouse hovering around some item boundary...
		int y = 0;
		QListViewItem *pItem = QListView::firstChild();
		while (pItem) {
			m_iItemDragY = y;
			y += pItem->totalHeight();
			if (pMouseEvent->y() > y - 4 && pMouseEvent->y() < y + 4) {
				m_pItemDrag = pItem;
				QListView::setCursor(QCursor(Qt::SplitVCursor));
				break;
			} else if (m_pItemDrag && m_dragState == DragNone) {
				m_pItemDrag = NULL;
				QListView::unsetCursor();
			}
			pItem = pItem->nextSibling();
		}
		break;
	}

//	QListView::contentsMouseMoveEvent(pMouseEvent);
}


void qtractorTrackList::contentsMouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QListView::contentsMouseReleaseEvent(pMouseEvent);

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
		if (m_pItemDrag) {
			QListViewItem *pItemDrop = QListView::itemAt(
				QListView::contentsToViewport(pMouseEvent->pos()));
			if (pItemDrop) {
				qtractorTrackListItem *pTrackItemDrag
					= static_cast<qtractorTrackListItem *> (m_pItemDrag);
				qtractorTrackListItem *pTrackItemDrop
					= static_cast<qtractorTrackListItem *> (pItemDrop);
				if (pSession && pTrackItemDrag && pTrackItemDrop) {
					qtractorTrack *pTrackDrag = pTrackItemDrag->track();
					qtractorTrack *pTrackDrop = pTrackItemDrop->track();
					if (pTrackDrag && pTrackDrop
						&& pTrackDrag != pTrackDrop
						&& pTrackDrag != pTrackDrop->next()) {
						pMainForm->commands()->exec(
							new qtractorMoveTrackCommand(pMainForm,
								pTrackDrag, pTrackDrop));
					}
				}
			}
		}
		break;
	case DragResize:
		if (m_pItemDrag) {
			drawDragLine(m_posDrag);    // Hide.
			int iItemHeight = pMouseEvent->y() - m_iItemDragY;
			// Check for minimum item height.
			if (iItemHeight < QTRACTOR_ITEM_HEIGHT_MIN)
				iItemHeight = QTRACTOR_ITEM_HEIGHT_MIN;
			// Go for it...
			qtractorTrackListItem *pTrackItem
				= static_cast<qtractorTrackListItem *> (m_pItemDrag);
			if (pTrackItem) {
				pMainForm->commands()->exec(
					new qtractorResizeTrackCommand(pMainForm,
						pTrackItem->track(), iItemHeight));
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
void qtractorTrackList::drawDragLine ( const QPoint& posDrag, int iThickness ) const
{
	QWidget *pViewport = QListView::viewport();
	int w = pViewport->width();
	int h = pViewport->height();

	// Make sure the line doesn't get too off view...
	QPoint pos = QListView::contentsToViewport(posDrag);
	if (pos.y() < 0)
		pos.setY(0);
	if (pos.y() > h)
		pos.setY(h);

	QPainter p(pViewport);

	QPen pen;
	pen.setColor(Qt::gray);
	pen.setWidth(iThickness);

	p.setRasterOp(Qt::NotROP);
	p.setPen(pen);
	p.drawLine(0, pos.y(), w, pos.y());
}


// Reset drag/select/move state.
void qtractorTrackList::resetDragState (void)
{
	// Cancel any dragging out there...
	if (m_dragState == DragMove || m_dragState == DragResize)
		drawDragLine(m_posDrag);    // Hide.

	// Should fallback mouse cursor...
	if (m_dragState != DragNone)
		QListView::unsetCursor();

	// Not dragging anymore.
	m_dragState  = DragNone;
	m_pItemDrag  = NULL;
	m_iItemDragY = 0;
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
		QListView::keyPressEvent(pKeyEvent);
		break;
	}
}


// To have track list in v-sync with main track view.
void qtractorTrackList::contentsMovingSlot ( int /*cx*/, int cy )
{
	if (QListView::contentsY() != cy)
		m_pTracks->setContentsPos(this, QListView::contentsX(), cy);
}


// Context menu request slot.
void qtractorTrackList::contextMenuSlot ( QListViewItem* /*pItem*/,
	const QPoint& pos, int /*col*/ )
{
	// We'll need a reference for issuing commands...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->trackMenu->exec(pos);
}


// Simple click handler.
void qtractorTrackList::clickedSlot ( QListViewItem *pItem,
	const QPoint& /*pos*/, int /*col*/ )
{
	qtractorTrack *pTrack = NULL;
	qtractorTrackListItem *pTrackItem
		= static_cast<qtractorTrackListItem *> (pItem);
	if (pTrackItem)
		pTrack = pTrackItem->track();
	if (pTrack == NULL)
		return;

	// TODO: Maybe something might be done, regarding direct
	// selection of track busses and/or MIDI instruments...
}


void qtractorTrackList::contentsChangeNotify (void)
{
	m_pTracks->contentsChangeNotify();
}


// end of qtractorTrackList.cpp
