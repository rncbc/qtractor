// qtractorTrackList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTrack.h"
#include "qtractorTracks.h"
#include "qtractorTrackView.h"
#include "qtractorTrackCommand.h"
#include "qtractorTrackButton.h"

#include "qtractorInstrument.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"

#include "qtractorMidiBuffer.h"
#include "qtractorPlugin.h"

#include "qtractorRubberBand.h"

#include "qtractorMainForm.h"
#include "qtractorMixer.h"

#include "qtractorCurve.h"

#include <QHeaderView>

#include <QApplication>
#include <QHBoxLayout>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>

#ifdef CONFIG_GRADIENT
#include <QLinearGradient>
#endif


//----------------------------------------------------------------------------
// qtractorCurveButton -- Track automation curve menu button.

class qtractorCurveButton : public QPushButton
{
//	Q_OBJECT

public:

	// Constructor.
	qtractorCurveButton(qtractorTrack *pTrack, QWidget *pParent)
		: QPushButton(pParent), m_pTrack(pTrack)
		{ QPushButton::setFocusPolicy(Qt::NoFocus); }

	// Button state updater.
	void updateTrack()
	{
		qtractorCurveList *pCurveList = m_pTrack->curveList();
		if (pCurveList) {
			QPalette pal;
			if (pCurveList->isCapture()) {
				pal.setColor(QPalette::Button, Qt::darkRed);
				pal.setColor(QPalette::ButtonText, Qt::red);
			}
			else
			if (pCurveList->isProcess()) {
				pal.setColor(QPalette::Button, Qt::darkGreen);
				pal.setColor(QPalette::ButtonText, Qt::green);
			}
			QPushButton::setPalette(pal);
			QString sToolTip(QObject::tr("Automation (%1)"));
			qtractorSubject *pSubject = NULL;
			qtractorCurve *pCurrentCurve = pCurveList->currentCurve();
			if (pCurrentCurve)
				pSubject = pCurrentCurve->subject();
			if (pSubject)
				QPushButton::setToolTip(sToolTip.arg(pSubject->name()));
			else
				QPushButton::setToolTip(sToolTip.arg(QObject::tr("none")));
		}
	}

protected:

	// Virtual trap to set current track
	// before showing the automation menu...
	bool hitButton(const QPoint& pos) const
	{
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorTracks *pTracks = pMainForm->tracks();
			if (pTracks)
				pTracks->trackList()->setCurrentTrack(m_pTrack);
		}

		return QPushButton::hitButton(pos);
	}

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
};


//----------------------------------------------------------------------------
// qtractorTrackListHeaderModel -- Track-list header model.

// Constructor.
qtractorTrackListHeaderModel::qtractorTrackListHeaderModel ( QObject *pParent )
	: QAbstractListModel(pParent)
{
	m_headerText
		<< tr("Nr")
		<< tr("Track Name")
		<< tr("Bus")
		<< tr("Ch")
		<< tr("Patch")
		<< tr("Instrument");
};

QVariant qtractorTrackListHeaderModel::headerData (
	int section, Qt::Orientation orient, int role ) const
{
	if (orient == Qt::Horizontal) {
		switch (role) {
		case Qt::DisplayRole:
			return m_headerText.at(section);
		case Qt::TextAlignmentRole:
			if (section == qtractorTrackList::Number ||
				section == qtractorTrackList::Channel)
				return int(Qt::AlignHCenter | Qt::AlignVCenter);
			else
				return int(Qt::AlignLeft | Qt::AlignVCenter);
		}
	}	

	return QVariant();
}


//----------------------------------------------------------------------------
// qtractorTrackItemWidget -- Track button layout widget.

// Constructor.
qtractorTrackItemWidget::qtractorTrackItemWidget (
	qtractorTrackList *pTrackList, qtractorTrack *pTrack )
	: QWidget(pTrackList->viewport())
{
	QWidget::setBackgroundRole(QPalette::Window);

	QHBoxLayout *pHBoxLayout = new QHBoxLayout();
	pHBoxLayout->setMargin(2);
	pHBoxLayout->setSpacing(2);

	const QFont& font = QWidget::font();
	const QFont font2(font.family(), font.pointSize() - 2);
	const int iFixedHeight = QFontMetrics(font).lineSpacing() + 4;

	const QSize buttonSize(22, iFixedHeight);

	m_pRecordButton = new qtractorTrackButton(pTrack, qtractorTrack::Record);
	m_pRecordButton->setFixedSize(buttonSize);
	m_pRecordButton->setFont(font2);

	m_pMuteButton = new qtractorTrackButton(pTrack, qtractorTrack::Mute);
	m_pMuteButton->setFixedSize(buttonSize);
	m_pMuteButton->setFont(font2);

	m_pSoloButton = new qtractorTrackButton(pTrack, qtractorTrack::Solo);
	m_pSoloButton->setFixedSize(buttonSize);
	m_pSoloButton->setFont(font2);

	m_pCurveButton = new qtractorCurveButton(pTrack, this);
	m_pCurveButton->setFixedSize(QSize(32, iFixedHeight));
	m_pCurveButton->setFont(font2);
	m_pCurveButton->setText("A");
	m_pCurveButton->setToolTip(QObject::tr("Automation"));

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		m_pCurveButton->setMenu(pMainForm->trackCurveMenu());

//	pHBoxLayout->addStretch();
	pHBoxLayout->addWidget(m_pRecordButton);
	pHBoxLayout->addWidget(m_pMuteButton);
	pHBoxLayout->addWidget(m_pSoloButton);
	pHBoxLayout->addWidget(m_pCurveButton);
	QWidget::setLayout(pHBoxLayout);
}


//----------------------------------------------------------------------------
// qtractorTrackList -- Track list widget.

// Constructor.
qtractorTrackList::qtractorTrackList ( qtractorTracks *pTracks, QWidget *pParent )
	: qtractorScrollView(pParent)
{
	m_pTracks = pTracks;

	m_iCurrentTrack = -1;

	m_dragState  = DragNone;
	m_iDragTrack = -1;
	m_iDragY     = 0;

	m_pRubberBand = NULL;

	m_iUpdateContents = 0;

	m_pPixmap[IconAudio] = new QPixmap(":/images/trackAudio.png");
	m_pPixmap[IconMidi]  = new QPixmap(":/images/trackMidi.png");

	// Allocate local header.
	m_pHeader = new QHeaderView(Qt::Horizontal, qtractorScrollView::viewport());
	m_pHeader->setModel(new qtractorTrackListHeaderModel(this));
	m_pHeader->setHighlightSections(false);
	m_pHeader->setStretchLastSection(true);
	m_pHeader->setSortIndicatorShown(false);
	// Default section sizes...
	m_pHeader->resizeSection(Number, 26);
	m_pHeader->resizeSection(Name, 120);
	m_pHeader->resizeSection(Channel, 24);

//	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
//	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);
	qtractorScrollView::setMouseTracking(true);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

	QObject::connect(m_pHeader,
		SIGNAL(sectionResized(int,int,int)),
		SLOT(updateHeader()));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackList::~qtractorTrackList (void)
{
	delete m_pPixmap[IconAudio];
	delete m_pPixmap[IconMidi];
}


// Main tracks widget accessor.
qtractorTracks *qtractorTrackList::tracks (void) const
{
	return m_pTracks;
}


// Loacl header view accessor.
QHeaderView *qtractorTrackList::header (void) const
{
	return m_pHeader;
}


// Track-list model item constructor
qtractorTrackList::Item::Item ( qtractorTrackList *pTrackList,
	qtractorTrack *pTrack ) : track(pTrack), widget(NULL)
{
	update(pTrackList);
}


// Track-list model item destructor
qtractorTrackList::Item::~Item (void)
{
	if (widget) delete widget;
}


// Track-list model item bank/program names helper.
bool qtractorTrackList::Item::updateBankProgram (
	qtractorMidiManager *pMidiManager, const QString& sInstrument,
	QString& sBank, QString& sProgram ) const
{
	if (pMidiManager == NULL)
		return false;

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrument))
		return false;

	const qtractorMidiManager::Banks& banks
		= list[sInstrument];
	const int iBank = track->midiBank();
	if (banks.contains(iBank)) {
		const qtractorMidiManager::Bank& bank
			= banks[iBank];
		const int iProg = track->midiProg();
		if (bank.progs.contains(iProg)) {
			sProgram = QString("%1 - %2").arg(iProg)
				.arg(bank.progs[iProg]);
			sBank = bank.name;
		}
	}

	return true;
}


// Track-list model item cache updater.
void qtractorTrackList::Item::update ( qtractorTrackList *pTrackList )
{
	text.clear();

	// Default initialization?
	if (track == NULL)
		return;

	if (widget == NULL) {
		widget = new qtractorTrackItemWidget(pTrackList, track);
		widget->lower();
	}

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
			// MIDI channels...
			QString sOmni;
			if (track->isMidiOmni())
				sOmni += '*';
			const unsigned short iChannel = track->midiChannel();
			text << sOmni + QString::number(iChannel + 1);
			// Care of MIDI instrument, program and bank numbers vs.names...
			QString sInstrument = s;
			QString sProg = s;
			QString sBank;
			if (track->midiProg() >= 0)
				sProg = QString::number(track->midiProg() + 1) + s;
			if (track->midiBank() >= 0)
				sBank = QString::number(track->midiBank());
			if (pMidiBus) {
				const qtractorMidiBus::Patch& patch
					= pMidiBus->patch(iChannel);
				if (!patch.instrumentName.isEmpty()) {
					sInstrument = patch.instrumentName;
					bool bMidiManager = updateBankProgram(
						(track->pluginList())->midiManager(),
						sInstrument, sBank, sProg);
					if (!bMidiManager && pMidiBus->pluginList_out()) {
						bMidiManager = updateBankProgram(
							(pMidiBus->pluginList_out())->midiManager(),
							sInstrument, sBank, sProg);
					}
					if (!bMidiManager) {
						qtractorInstrumentList *pInstruments = NULL;
						qtractorSession *pSession
							= qtractorSession::getInstance();
						if (pSession)
							pInstruments = pSession->instruments();
						if (pInstruments
							&& pInstruments->contains(sInstrument)) {
							qtractorInstrument& instr
								= (*pInstruments)[sInstrument];
							const qtractorInstrumentData& bank
								= instr.patch(track->midiBank());
							if (bank.contains(track->midiProg())) {
								sProg = bank[track->midiProg()];
								sBank = bank.name();
							}
						}
					}
				}
			}
			// This is it, MIDI Patch/Bank...
			text << sProg + '\n' + sBank << sInstrument;
			break;
		}

		case qtractorTrack::None:
		default: {
			text << s + '\n' + QObject::tr("Unknown") << s << s << s;
			break;
		}
	}

	widget->curveButton()->updateTrack();
}


// Find the list view item from track pointer reference.
int qtractorTrackList::trackRow ( qtractorTrack *pTrack ) const
{
	int iTrack = 0;
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		if (pTrack == iter.next()->track)
			return iTrack;
		++iTrack;
	}

	return -1;
}


// Find track row of given viewport point...
int qtractorTrackList::trackRowAt ( const QPoint& pos )
{
	const int cy = qtractorScrollView::contentsY();
	const int  y = cy + pos.y() - m_pHeader->sizeHint().height();
	const int  h = qtractorScrollView::viewport()->height();

	int y1, y2;
	y1 = y2 = 0;
	int iTrack = 0;
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext() && y2 < cy + h) {
		y1  = y2;
		y2 += (iter.next()->track)->zoomHeight();
		if (y >= y1 && y < y2)
			return iTrack;
		++iTrack;
	}

	return -1;
}


// Find track column of given viewport point...
int qtractorTrackList::trackColumnAt ( const QPoint& pos )
{
	const int cy = qtractorScrollView::contentsY();
	const int  y = cy + pos.y() - m_pHeader->sizeHint().height();

	if (y > 0 && y < qtractorScrollView::contentsHeight()) {
		const int cx = qtractorScrollView::contentsX();
		const int  x = cx + pos.x();
		int x1, x2;
		x1 = x2 = 0;
		const int iColCount = m_pHeader->count();
		for (int iCol = 0; iCol < iColCount; ++iCol) {
			x1  = x2;
			x2 += m_pHeader->sectionSize(iCol);
			if (x > x1 && x < x2)
				return iCol;
		}
	}

	return -1;
}


// Find the track pointer reference from list view item row.
qtractorTrack *qtractorTrackList::track ( int iTrack ) const
{
	if (iTrack < 0 || iTrack >= m_items.count())
		return NULL;

	return m_items.at(iTrack)->track;
}


// Retrive the given track row rectangular (in viewport coordinates).
QRect qtractorTrackList::trackRect ( int iTrack ) const
{
	QRect rect;

	if (iTrack >= 0 && iTrack < m_items.count()) {
		rect.setX(0);
		rect.setWidth(qtractorScrollView::viewport()->width());
		int y1, y2;
		y1 = y2 = m_pHeader->sizeHint().height();
		QListIterator<Item *> iter(m_items);
		while (iter.hasNext()) {
			y1  = y2;
			y2 += (iter.next()->track)->zoomHeight();
			if (iTrack == 0) {
				rect.setY(y1 - qtractorScrollView::contentsY());
				rect.setHeight(y2 - y1);
				break;
			}
			--iTrack;
		}
	}

	return rect;
}


// Insert a track item; return actual track row added.
int qtractorTrackList::insertTrack ( int iTrack, qtractorTrack *pTrack )
{
	if (iTrack < 0)
		iTrack = m_items.count();

	m_items.insert(iTrack, new Item(this, pTrack));

	return iTrack;
}


// Remove a track item; return remaining track row.
int qtractorTrackList::removeTrack ( int iTrack )
{
	if (iTrack < 0 || iTrack >= m_items.count())
		return -1;

	delete m_items.at(iTrack);
	m_items.removeAt(iTrack);

	if (m_iCurrentTrack >= m_items.count())
		m_iCurrentTrack  = m_items.count() - 1;

	return (iTrack < m_items.count() ? iTrack : -1);
}


// Manage current track row by index.
void qtractorTrackList::setCurrentTrackRow ( int iTrack )
{
	int iCurrentTrack = m_iCurrentTrack;
	if (iTrack < 0 || iTrack >= m_items.count())
		iCurrentTrack = -1;
	else
		iCurrentTrack = iTrack;
	if (iCurrentTrack == m_iCurrentTrack)
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTrackList::setCurrentTrackRow(%d)", iCurrentTrack);
#endif

	m_iCurrentTrack = iCurrentTrack;

	// Make sure the new current track is visible...
	if (!ensureVisibleRect(trackRect(m_iCurrentTrack)))
		updateContents();

	emit selectionChanged();
}

int qtractorTrackList::currentTrackRow (void) const
{
	return m_iCurrentTrack;
}

int qtractorTrackList::trackRowCount (void) const
{
	return m_items.count();
}


// Current selected track reference.
void qtractorTrackList::setCurrentTrack ( qtractorTrack *pTrack )
{
	setCurrentTrackRow(trackRow(pTrack));
}

qtractorTrack *qtractorTrackList::currentTrack (void) const
{
	if (m_iCurrentTrack < 0 || m_iCurrentTrack >= m_items.count())
		return NULL;

	return m_items.at(m_iCurrentTrack)->track;
}


// Find the list view item from track pointer reference.
void qtractorTrackList::updateTrack ( qtractorTrack *pTrack )
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		if (pTrack == NULL || pTrack == pItem->track) {
			// Force update the data...
			pItem->update(this);
			// Specific bail out...
			if (pTrack)
				break;
		}
	}

	updateContents();
}


// Main table cleaner.
void qtractorTrackList::clear (void)
{
	m_iCurrentTrack = -1;

	m_dragState  = DragNone;
	m_iDragTrack = -1;
	m_iDragY     = 0;

	if (m_pRubberBand)
		delete m_pRubberBand;
	m_pRubberBand = NULL;

	qDeleteAll(m_items);
	m_items.clear();
}


// Update all tracks item height.
void qtractorTrackList::updateContentsHeight (void)
{
	// Remember to give some room to drop something at the bottom...
	int iContentsHeight
		= m_pHeader->sizeHint().height() + (qtractorTrack::HeightBase << 1);
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		qtractorTrack *pTrack = iter.next()->track;
		pTrack->updateZoomHeight();
		iContentsHeight += pTrack->zoomHeight();
	}

	qtractorScrollView::resizeContents(
		qtractorScrollView::contentsWidth(), iContentsHeight);

	// Update track view total contents height...
	qtractorTrackView *pTrackView = m_pTracks->trackView();
	pTrackView->updateContentsHeight();
	pTrackView->updateContents();

	updateContents();
}


// Rectangular contents update.
void qtractorTrackList::updateContents ( const QRect& rect )
{
	if (m_iUpdateContents > 0)
		return;

	++m_iUpdateContents;

	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents(rect);
	
	--m_iUpdateContents;
}


// Overall contents update.
void qtractorTrackList::updateContents (void)
{
	if (m_iUpdateContents > 0)
		return;

	++m_iUpdateContents;

	updatePixmap(
		qtractorScrollView::contentsX(), qtractorScrollView::contentsY());

	qtractorScrollView::updateContents();

	--m_iUpdateContents;
}


// Resize event handler.
void qtractorTrackList::resizeEvent ( QResizeEvent *pResizeEvent )
{
	qtractorScrollView::resizeEvent(pResizeEvent);

	updateHeader();
}


// Update header extents.
void qtractorTrackList::updateHeader (void)
{
	// Find out wich is the largest header width
	// and enforce to let it know it like so...
	const int iColCount = m_pHeader->count() - 1;
	int iContentsWidth = (qtractorScrollView::viewport()->width() >> 1);
	for (int iCol = 0; iCol < iColCount; ++iCol)
		iContentsWidth += m_pHeader->sectionSize(iCol);
	m_pHeader->setFixedWidth(iContentsWidth);

	qtractorScrollView::resizeContents(
		iContentsWidth, qtractorScrollView::contentsHeight());

	updateContents();
}


// Draw table cell.
void qtractorTrackList::drawCell ( QPainter *pPainter, int iRow, int iCol,
	const QRect& rect ) const
{
	const QPalette& pal = qtractorScrollView::palette();
	const Item *pItem = m_items.at(iRow);
	QColor bg, fg;
	if (iCol == Number) {
		bg = (pItem->track)->foreground().lighter();
		fg = (pItem->track)->background().lighter();
	} else if (m_iCurrentTrack == iRow) {
		bg = pal.midlight().color().darker(160);
		fg = pal.highlightedText().color();
	} else {
		bg = pal.window().color();
		fg = pal.windowText().color();
	}

	// Draw text and decorations if any...
	QRect rectText(rect.topLeft() + QPoint(4, 4), rect.size() - QSize(8, 8));
#ifdef CONFIG_GRADIENT
	QLinearGradient grad(0, rect.top(), 0, rect.bottom());
	grad.setColorAt(0.4, bg);
	grad.setColorAt(1.0, bg.darker(120));
	pPainter->fillRect(rect, grad);
#else
	pPainter->fillRect(rect, bg);
#endif
	pPainter->setPen(fg);
	if (iCol == Number) {
		pPainter->drawText(rectText,
			Qt::AlignHCenter | Qt::AlignTop,
			QString::number(iRow + 1));
	} else if (iCol == Channel) {
		pPainter->drawText(rectText,
			Qt::AlignHCenter | Qt::AlignTop,
			pItem->text.at(iCol - 1));
	} else {
		if (iCol == Bus) {
			const QPixmap *pPixmap = NULL;
			switch ((pItem->track)->trackType()) {
			case qtractorTrack::Audio:
				pPixmap = m_pPixmap[IconAudio];
				break;
			case qtractorTrack::Midi:
				pPixmap = m_pPixmap[IconMidi];
				break;
			case qtractorTrack::None:
			default:
				break;
			}
			if (pPixmap) {
				pPainter->drawPixmap(rectText.x(), rectText.y(), *pPixmap);
				rectText.setLeft(rectText.left() + pPixmap->width() + 4);
			}
		}
		pPainter->drawText(rectText,
			Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
			pItem->text.at(iCol - 1));
	}

	// Do some simple embossing...
	pPainter->setPen(bg.lighter(150));
	pPainter->drawLine(rect.left(), rect.top(), rect.left(), rect.bottom());
	pPainter->drawLine(rect.left(), rect.top(), rect.right(), rect.top());
	pPainter->setPen(bg.darker(150));
	pPainter->drawLine(rect.right(), rect.top(), rect.right(), rect.bottom());
	pPainter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
}


// (Re)create the complete view pixmap.
void qtractorTrackList::updatePixmap ( int cx, int cy )
{
	QWidget *pViewport = qtractorScrollView::viewport();
	const int w = pViewport->width();
	const int h = pViewport->height();

	if (w < 1 || h < 1)
		return;

	const QPalette& pal = qtractorScrollView::palette();

	m_pixmap = QPixmap(w, h);
	m_pixmap.fill(pal.window().color());

	QPainter painter(&m_pixmap);
	painter.initFrom(pViewport);

	// Update actual contents size...
	m_pHeader->setOffset(cx);

	// Draw all cells...
	const int iColCount = m_pHeader->count();
	const int hh = m_pHeader->sizeHint().height();
	// Account for the item dropping headroom...
	const int ch = qtractorScrollView::contentsHeight()
		- (qtractorTrack::HeightBase << 1);

	int x, y1, y2, h1;
	y1 = y2 = 0;
	int iTrack = 0;
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		h1  = (pItem->track)->zoomHeight();
		y1  = y2;
		y2 += h1;
		if (y2 > cy && y1 < cy + h) {
			// Dispatch to paint this track...
			QRect rect(0, y1 - cy + hh, w, h1);
			x = 0;
			for (int iCol = 0; iCol < iColCount; ++iCol) {
				const int dx = m_pHeader->sectionSize(iCol);
				if (x + dx > cx && x < cx + w) {
					rect.setX(x - cx);
					rect.setWidth(dx);
					drawCell(&painter, iTrack, iCol, rect);
					if (iCol == Name && pItem->widget) {
						QRect rectWidget = rect;
						QSize sizeWidget = (pItem->widget)->sizeHint();
						rectWidget.setTop(rect.bottom() - sizeWidget.height());
						rectWidget.setLeft(rect.right() - sizeWidget.width());
						if (rectWidget.height() < h1) {
							(pItem->widget)->setGeometry(rectWidget);
							(pItem->widget)->show();
						} else {
							(pItem->widget)->hide();
						}
					}
				}
				else if (iCol == Name && pItem->widget)
					(pItem->widget)->hide();
				x += dx;
			}
		}
		else if (pItem->widget) (pItem->widget)->hide();
		++iTrack;
	}

	if (cy + h > ch) {
		painter.setPen(pal.mid().color());
		painter.drawLine(0, ch - cy, w, ch - cy);
		painter.fillRect(0, ch - cy + 1, w, cy + h - ch, pal.dark().color());
	}
}


// Draw the time scale.
void qtractorTrackList::drawContents ( QPainter *pPainter, const QRect& rect )
{
	pPainter->drawPixmap(rect, m_pixmap, rect);
}


// To have keyline in v-sync with main view.
void qtractorTrackList::contentsYMovingSlot ( int /*cx*/, int cy )
{
	if (qtractorScrollView::contentsY() != cy)
		qtractorScrollView::setContentsPos(qtractorScrollView::contentsX(), cy);
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



// Handle mouse double-clicks.
void qtractorTrackList::mouseDoubleClickEvent ( QMouseEvent * /*pMouseEvent*/ )
{
	// Avoid lingering mouse press/release states...
	resetDragState();

	// We'll need a reference for issuing commands...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->trackProperties();
}


// Handle item selection/dragging -- mouse button press.
void qtractorTrackList::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	const QPoint& pos = pMouseEvent->pos();

	// Select current track...
	const int iTrack = trackRowAt(pos);
	setCurrentTrackRow(iTrack);

	// Look for the mouse hovering around some item boundary...
	if (iTrack >= 0) {
		// Special attitude, only of interest on
		// the first-left column (track-number)...
		if (trackColumnAt(pos) == Number) {
			m_pTracks->selectCurrentTrack((pMouseEvent->modifiers()
				& (Qt::ShiftModifier | Qt::ControlModifier)) == 0);
		}
		if (pMouseEvent->button() == Qt::LeftButton) {
			// Try for drag-resize...
			m_posDrag = pos;
			if (m_iDragTrack >= 0) {
				m_dragState  = DragResize;
			} else {
				m_iDragTrack = iTrack;
				m_iDragY     = 0;
				m_dragState  = DragStart;
				qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
			}
		}
	}

	// Probably this would be done ahead,
	// just 'coz we're using ruberbands, which are
	// in fact based on real widgets (WM entities)...
//	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorTrackList::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	const QPoint& pos = pMouseEvent->pos();

	// We're already on some item dragging/resizing?...
	switch (m_dragState) {
	case DragMove:
		// Currently moving an item...
		if (m_iDragTrack >= 0) {
			const int iTrack = trackRowAt(pos);
			if (iTrack >= 0) {
				const QRect& rect = trackRect(iTrack);
				m_posDrag = rect.topLeft();
				moveRubberBand(m_posDrag);
				ensureVisibleRect(rect);
			} else {
				const QRect& rect = trackRect(m_items.count() - 1);
				m_posDrag = rect.bottomLeft();
				moveRubberBand(m_posDrag);
			}
		}
		break;
	case DragResize:
		// Currently resizing an item...
		if (m_iDragTrack >= 0) {
			int y = pos.y();
			if (y < m_iDragY + qtractorTrack::HeightMin)
				y = m_iDragY + qtractorTrack::HeightMin;
			m_posDrag.setY(y);
			moveRubberBand(m_posDrag);
		}
		break;
	case DragStart:
		// About to start dragging an item...
		if (m_iDragTrack >= 0
			&& (m_posDrag - pos).manhattanLength()
				> QApplication::startDragDistance()) {
			qtractorScrollView::setCursor(QCursor(Qt::SizeVerCursor));
			m_dragState = DragMove;
			m_posDrag   = trackRect(m_iDragTrack).topLeft();
			moveRubberBand(m_posDrag);
		}
		break;
	case DragNone: {
		// Look for the mouse hovering around first column item boundary...
		int iTrack = trackRowAt(pos);
		if (iTrack >= 0 && trackColumnAt(pos) == Number) {
			m_posDrag  = pos;
			QRect rect = trackRect(iTrack);
			if (pos.y() >= rect.top() && pos.y() < rect.top() + 4) {
				if (--iTrack >= 0) {
					if (m_iDragTrack < 0)
						qtractorScrollView::setCursor(QCursor(Qt::SplitVCursor));
					m_iDragTrack = iTrack;
					m_iDragY = trackRect(iTrack).top();
					break;
				}
			}
			else
			if (pos.y() >= rect.bottom() - 4 && pos.y() < rect.bottom() + 4) {
				if (m_iDragTrack < 0)
					qtractorScrollView::setCursor(QCursor(Qt::SplitVCursor));
				m_iDragTrack = iTrack;
				m_iDragY = rect.top();
				break;
			}
		}
		// If something has been going on, turn it off...
 		if (m_iDragTrack >= 0) {
			qtractorScrollView::unsetCursor();
			m_iDragTrack = -1;
			m_iDragY = 0;
		}
		break;
	}
	default:
		break;
	}

//	qtractorScrollView::mouseMoveEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse button release.
void qtractorTrackList::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
//	qtractorScrollView::mouseReleaseEvent(pMouseEvent);

	// We'll need a reference for issuing commands...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	const QPoint& pos = pMouseEvent->pos();

	// If we were resizing, now's time to let
	// things know that have changed somehow...
	switch (m_dragState) {
	case DragMove:
		if (m_iDragTrack >= 0) {
			qtractorTrack *pTrack = track(m_iDragTrack);
			if (pTrack) {
				qtractorTrack *pTrackDrop = NULL;
				const int iTrack = trackRowAt(pos);
				if (iTrack >= 0)
					pTrackDrop = track(iTrack);
				if (pTrack != pTrackDrop
					&& (pTrackDrop == NULL || pTrack != pTrackDrop->prev())) {
					pSession->execute(
						new qtractorMoveTrackCommand(pTrack, pTrackDrop));
				}
			}
		}
		break;
	case DragResize:
		if (m_iDragTrack >= 0) {
			int iZoomHeight = pos.y() - m_iDragY;
			// Check for minimum item height.
			if (iZoomHeight < qtractorTrack::HeightMin)
				iZoomHeight = qtractorTrack::HeightMin;
			// Go for it...
			qtractorTrack *pTrack = track(m_iDragTrack);
			if (pTrack) {
				pSession->execute(
					new qtractorResizeTrackCommand(pTrack, iZoomHeight));
			}
		}
		// Fall thru...
	case DragStart:
	case DragNone:
	default:
		break;
	}

	// Force null state.
	resetDragState();
}


// Handle zoom with mouse wheel.
void qtractorTrackList::wheelEvent ( QWheelEvent *pWheelEvent )
{
	if (pWheelEvent->modifiers() & Qt::ControlModifier) {
		const int delta = pWheelEvent->delta();
		if (delta > 0)
			m_pTracks->zoomIn();
		else
			m_pTracks->zoomOut();
	}
	else qtractorScrollView::wheelEvent(pWheelEvent);
}


// Draw a dragging separator line.
void qtractorTrackList::moveRubberBand ( const QPoint& posDrag )
{
	// Create the rubber-band if there's none...
	if (m_pRubberBand == NULL) {
		m_pRubberBand = new qtractorRubberBand(
			QRubberBand::Line, qtractorScrollView::viewport());
	//	QPalette pal(m_pRubberBand->palette());
	//	pal.setColor(m_pRubberBand->foregroundRole(), Qt::blue);
	//	m_pRubberBand->setPalette(pal);
	//	m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	}
	
	// Just move it
	m_pRubberBand->setGeometry(
		QRect(0, posDrag.y(), qtractorScrollView::viewport()->width(), 4));

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Make sure the given (track) rectangle is visible.
bool qtractorTrackList::ensureVisibleRect ( const QRect& rect )
{
	if (!rect.isValid())
		return false;
		
	const int hh = m_pHeader->sizeHint().height();
	const int cx = qtractorScrollView::contentsX();
	const int cy = qtractorScrollView::contentsY();

	if (rect.top() < hh) {
		qtractorScrollView::ensureVisible(cx, cy + rect.top(), 0, 24);
		return true;
	}

	if (rect.bottom() > qtractorScrollView::viewport()->height()) {
		qtractorScrollView::ensureVisible(cx, cy + rect.bottom() - hh, 0, 24);
		return true;
	}

	return false;
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
		qtractorScrollView::unsetCursor();

	// Not dragging anymore.
	m_dragState  = DragNone;
	m_iDragTrack = -1;
	m_iDragY     = 0;
}


// Keyboard event handler.
void qtractorTrackList::keyPressEvent ( QKeyEvent *pKeyEvent )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorTrackList::keyPressEvent(%d)", pKeyEvent->key());
#endif
	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
		resetDragState();
		break;
	case Qt::Key_Home:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			setCurrentTrackRow(0);
		} else {
			qtractorScrollView::setContentsPos(
				0, qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_End:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			setCurrentTrackRow(m_items.count() - 1);
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsWidth(),
				qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_Left:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() - qtractorScrollView::width(),
				qtractorScrollView::contentsY());
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() - 16,
				qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_Right:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() + qtractorScrollView::width(),
				qtractorScrollView::contentsY());
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX() + 16,
				qtractorScrollView::contentsY());
		}
		break;
	case Qt::Key_Up:
		if (m_iCurrentTrack > 0)
			setCurrentTrackRow(m_iCurrentTrack - 1);
		break;
	case Qt::Key_Down:
		if (m_iCurrentTrack < m_items.count() - 1)
			setCurrentTrackRow(m_iCurrentTrack + 1);
		break;
	case Qt::Key_PageUp:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(), 0);
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY()
					- qtractorScrollView::height());
		}
		break;
	case Qt::Key_PageDown:
		if (pKeyEvent->modifiers() & Qt::ControlModifier) {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsHeight());
		} else {
			qtractorScrollView::setContentsPos(
				qtractorScrollView::contentsX(),
				qtractorScrollView::contentsY()
					+ qtractorScrollView::height());
		}
		break;
	default:
		qtractorScrollView::keyPressEvent(pKeyEvent);
		break;
	}
}


// end of qtractorTrackList.cpp
