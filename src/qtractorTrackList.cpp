// qtractorTrackList.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiManager.h"

#include "qtractorPluginListView.h"

#include "qtractorRubberBand.h"

#include "qtractorMainForm.h"
#include "qtractorThumbView.h"
#include "qtractorMixer.h"

#include "qtractorCurve.h"

#include "qtractorAudioMonitor.h"
#include "qtractorMidiMonitor.h"
#include "qtractorAudioMeter.h"
#include "qtractorMidiMeter.h"

#include "qtractorOptions.h"

#include "qtractorAudioFile.h"
#include "qtractorMidiFile.h"

#include <QHeaderView>
#include <QAbstractListModel>

#include <QApplication>
#include <QHBoxLayout>

#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QUrl>

#if QT_VERSION >= 0x050000
#include <QMimeData>
#endif


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
// qtractorTrackList::HeaderModel -- Track-list header model.

class qtractorTrackList::HeaderModel : public QAbstractListModel
{
public:

	// Constructor.
	HeaderModel(QObject *pParent = 0);

	QVariant headerData(int section, Qt::Orientation orient, int role) const;

	int rowCount(const QModelIndex&) const
		{ return 0; }

	int columnCount(const QModelIndex&) const
		{ return m_headerText.count(); }

	QVariant data(const QModelIndex&, int) const
		{ return QVariant(); }

private:

	// Model variables.
	QStringList m_headerText;
};


// Constructor.
qtractorTrackList::HeaderModel::HeaderModel ( QObject *pParent )
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


// Header model data.
QVariant qtractorTrackList::HeaderModel::headerData (
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
		case Qt::SizeHintRole:
			if (section == qtractorTrackList::Number ||
				section == qtractorTrackList::Channel)
				return QSize(24, 24);
			else
			if (section == qtractorTrackList::Name)
				return QSize(120, 24);
			else
				return QSize(100, 24);
		}
	}	

	return QVariant();
}


//----------------------------------------------------------------------------
// qtractorTrackListButtons -- Track button layout widget.

// Constructor.
qtractorTrackListButtons::qtractorTrackListButtons (
	qtractorTrack *pTrack, QWidget *pParent ) : QWidget(pParent)
{
	QWidget::setBackgroundRole(QPalette::Window);

	QHBoxLayout *pHBoxLayout = new QHBoxLayout();
	pHBoxLayout->setMargin(2);
	pHBoxLayout->setSpacing(2);

	const QFont& font = QWidget::font();
	const QFont font2(font.family(), font.pointSize() - 2);
	const int iFixedHeight = QFontMetrics(font).lineSpacing() + 4;

	const QSize buttonSize(22, iFixedHeight);

	m_pRecordButton = new qtractorTrackButton(pTrack, qtractorTrack::Record, this);
	m_pRecordButton->setFixedSize(buttonSize);
	m_pRecordButton->setFont(font2);

	m_pMuteButton = new qtractorTrackButton(pTrack, qtractorTrack::Mute, this);
	m_pMuteButton->setFixedSize(buttonSize);
	m_pMuteButton->setFont(font2);

	m_pSoloButton = new qtractorTrackButton(pTrack, qtractorTrack::Solo, this);
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

	pHBoxLayout->addStretch();
	pHBoxLayout->addWidget(m_pRecordButton);
	pHBoxLayout->addWidget(m_pMuteButton);
	pHBoxLayout->addWidget(m_pSoloButton);
	pHBoxLayout->addWidget(m_pCurveButton);
	QWidget::setLayout(pHBoxLayout);
}


// Refresh color (palette) state buttons
void qtractorTrackListButtons::updateTrackButtons (void)
{
	m_pRecordButton->updateTrackButton();
	m_pMuteButton->updateTrackButton();
	m_pSoloButton->updateTrackButton();
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
	m_pHeader->setModel(new HeaderModel(this));
	m_pHeader->setHighlightSections(false);
	m_pHeader->setStretchLastSection(true);
	m_pHeader->setSortIndicatorShown(false);
	m_pHeader->setMinimumSectionSize(24);

	// Default section sizes...
	const int iColCount = m_pHeader->count() - 1;
	for (int iCol = 0; iCol < iColCount; ++iCol)
		m_pHeader->resizeSection(iCol, m_pHeader->sectionSizeHint(iCol));

//	qtractorScrollView::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	qtractorScrollView::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	qtractorScrollView::viewport()->setFocusPolicy(Qt::ClickFocus);
//	qtractorScrollView::viewport()->setFocusProxy(this);
	qtractorScrollView::viewport()->setAcceptDrops(true);
//	qtractorScrollView::setDragAutoScroll(false);
	qtractorScrollView::setMouseTracking(true);

	const QFont& font = qtractorScrollView::font();
	qtractorScrollView::setFont(QFont(font.family(), font.pointSize() - 1));

	// Load header section (column) sizes...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("Tracks");
		const QByteArray& aHeaderView
			= pOptions->settings().value("/TrackList/HeaderView").toByteArray();
		if (!aHeaderView.isEmpty())
			m_pHeader->restoreState(aHeaderView);
		settings.endGroup();
	}

	QObject::connect(m_pHeader,
		SIGNAL(sectionResized(int,int,int)),
		SLOT(updateHeaderSize(int,int,int)));
	QObject::connect(m_pHeader,
		SIGNAL(sectionHandleDoubleClicked(int)),
		SLOT(resetHeaderSize(int)));

//	QObject::connect(this, SIGNAL(contentsMoving(int,int)),
//		this, SLOT(updatePixmap(int,int)));
}


// Destructor.
qtractorTrackList::~qtractorTrackList (void)
{
	// Save header section (column) sizes...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("Tracks");
		settings.setValue("/TrackList/HeaderView", m_pHeader->saveState());
		settings.endGroup();
	}

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
qtractorTrackList::Item::Item ( qtractorTrack *pTrack )
	: track(pTrack), flags(0), buttons(NULL), plugins(NULL), meters(NULL)
{
	const QString s;

	text << track->trackName() << s << s << s << s;
}


// Track-list model item destructor
qtractorTrackList::Item::~Item (void)
{
	if (meters)  delete meters;
	if (plugins) delete plugins;
	if (buttons) delete buttons;
}


// Track-list model item bank/program names helper.
bool qtractorTrackList::Item::updateBankProgNames (
	qtractorMidiManager *pMidiManager, const QString& sInstrumentName,
	QString& sBankName, QString& sProgName ) const
{
	if (pMidiManager == NULL)
		return false;

	const qtractorMidiManager::Instruments& list
		= pMidiManager->instruments();
	if (!list.contains(sInstrumentName))
		return false;

	const qtractorMidiManager::Banks& banks
		= list[sInstrumentName];
	const int iBank = track->midiBank();
	if (banks.contains(iBank)) {
		const qtractorMidiManager::Bank& bank
			= banks[iBank];
		const int iProg = track->midiProg();
		if (bank.progs.contains(iProg)) {
			sBankName = bank.name;
			sProgName = QString("%1 - %2").arg(iProg)
				.arg(bank.progs[iProg]);
		}
	}

	return true;
}


// Track-list model item cache updater.
void qtractorTrackList::Item::updateItem ( qtractorTrackList *pTrackList )
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	text.clear();

	// Default initialization?
	if (track == NULL)
		return;

	if (buttons == NULL) {
		buttons = new qtractorTrackListButtons(track, pTrackList->viewport());
		buttons->lower();
	}

	if (!pOptions->bTrackListPlugins && plugins) {
		delete plugins;
		plugins = NULL;
	}

	if (meters) {
		delete meters;
		meters = NULL;
	}

	updateIcon(pTrackList);

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
			// Re-create the audio meter...
			if (pOptions->bTrackListMeters && meters == NULL) {
				qtractorAudioMonitor *pAudioMonitor
					= static_cast<qtractorAudioMonitor *> (track->monitor());
				if (pAudioMonitor) {
					const int iAudioChannels = pAudioMonitor->channels();
					if (iAudioChannels > 0) {
						const int iColWidth
							= pTrackList->header()->sectionSize(Channel);
						const int iMinWidth = iColWidth / iAudioChannels - 1;
						if (iMinWidth > 3) {
							meters = new qtractorAudioMeter(
								pAudioMonitor, pTrackList->viewport());
							meters->lower();
						}
					}
				}
			}
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
			QString sInstrumentName = s;
			QString sProgName = s;
			QString sBankName;
			const int iProg = track->midiProg();
			if (iProg >= 0)
				sProgName = QString::number(iProg + 1) + s;
			const int iBank = track->midiBank();
			if (iBank >= 0)
				sBankName = QString::number(iBank);
			if (pMidiBus) {
				const qtractorMidiBus::Patch& patch
					= pMidiBus->patch(iChannel);
				if (!patch.instrumentName.isEmpty()) {
					sInstrumentName = patch.instrumentName;
					bool bMidiManager = updateBankProgNames(
						(track->pluginList())->midiManager(),
						sInstrumentName, sBankName, sProgName);
					if (!bMidiManager && pMidiBus->pluginList_out()) {
						bMidiManager = updateBankProgNames(
							(pMidiBus->pluginList_out())->midiManager(),
							sInstrumentName, sBankName, sProgName);
					}
					if (!bMidiManager) {
						qtractorInstrumentList *pInstruments = NULL;
						qtractorSession *pSession
							= qtractorSession::getInstance();
						if (pSession)
							pInstruments = pSession->instruments();
						if (pInstruments
							&& pInstruments->contains(sInstrumentName)) {
							const qtractorInstrument& instr
								= pInstruments->value(sInstrumentName);
							const qtractorInstrumentData& bank
								= instr.patch(iBank);
							if (bank.contains(iProg)) {
								sProgName = bank[iProg];
								sBankName = bank.name();
							}
						}
					}
				}
			}
			// This is it, MIDI Patch/Bank...
			text << sProgName + '\n' + sBankName << sInstrumentName;
			// Re-create the MIDI meter...
			if (pOptions->bTrackListMeters && meters == NULL) {
				qtractorMidiMonitor *pMidiMonitor
					= static_cast<qtractorMidiMonitor *> (track->monitor());
				if (pMidiMonitor) {
					qtractorMidiComboMeter *pMidiComboMeter
						= new qtractorMidiComboMeter(
							pMidiMonitor, pTrackList->viewport());
					qtractorMidiManager *pMidiManager
						= (track->pluginList())->midiManager();
					if (pMidiManager && pMidiManager->isAudioOutputMonitor()) {
						pMidiComboMeter->setAudioOutputMonitor(
							pMidiManager->audioOutputMonitor());
					}
					meters = pMidiComboMeter;
					meters->lower();
				}
			}
			break;
		}

		case qtractorTrack::None:
		default: {
			text << s + '\n' + QObject::tr("Unknown") << s << s << s;
			break;
		}
	}

	if (pOptions->bTrackListPlugins && plugins == NULL) {
		const QFont& font = pTrackList->font();
		plugins = new qtractorPluginListView(pTrackList->viewport());
		plugins->setFont(QFont(font.family(), font.pointSize() - 2));
		plugins->setTinyScrollBar(true);
		plugins->setPluginList(track->pluginList());
		plugins->lower();
	}

	buttons->curveButton()->updateTrack();
}


void qtractorTrackList::Item::updateIcon ( qtractorTrackList *pTrackList )
{
	const QPixmap pm(track->trackIcon());
	if (!pm.isNull()) {
		const int h0 = track->zoomHeight() - 4; // Account for track nr.
		const int h1 = (h0 < qtractorTrack::HeightMin ? h0 : h0 - 12);
		const int w0 = pTrackList->header()->sectionSize(Number) - 4;
		const int w1 = (w0 < h1 ? w0 : h1);
		icon = pm.scaled(w1, w1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	else icon = pm; // Null pixmap!
}


// Find the list view item from track pointer reference.
int qtractorTrackList::trackRow ( qtractorTrack *pTrack ) const
{
	int iTrack = 0;
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		if (iter.next()->track == pTrack)
			return iTrack;
		++iTrack;
	}
	return -1;
}


// Find track row of given contents point...
int qtractorTrackList::trackRowAt ( const QPoint& pos )
{
	const int y = pos.y() - m_pHeader->sizeHint().height();

	int y1, y2;
	y1 = y2 = 0;
	int iTrack = 0;
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext() && y2 < qtractorScrollView::contentsHeight()) {
		y1  = y2;
		y2 += (iter.next()->track)->zoomHeight();
		if (y >= y1 && y < y2)
			return iTrack;
		++iTrack;
	}

	return -1;
}


// Find track column of given contents point...
int qtractorTrackList::trackColumnAt ( const QPoint& pos )
{
	const int y = pos.y() - m_pHeader->sizeHint().height();

	if (y > 0 && y < qtractorScrollView::contentsHeight()) {
		const int x = pos.x();
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


// Retrive the given track row rectangular (in contents coordinates).
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
				rect.setY(y1);
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
	clearSelect();

	if (iTrack < 0)
		iTrack = m_items.count();

	Item *pItem = new Item(pTrack);
	m_items.insert(iTrack, pItem);
	m_tracks.insert(pTrack, pItem);

	return iTrack;
}


// Remove a track item; return remaining track row.
int qtractorTrackList::removeTrack ( int iTrack )
{
	if (iTrack < 0 || iTrack >= m_items.count())
		return -1;

	clearSelect();

	if (m_select.contains(iTrack))
		m_select.remove(iTrack);

	Item *pItem = m_items.at(iTrack);
	m_tracks.remove(pItem->track);
	m_items.removeAt(iTrack);
	delete pItem;

	m_iCurrentTrack = -1;

	const int iTrackCount = m_items.count();
	return (iTrack < iTrackCount ? iTrack : iTrackCount - 1);
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

	if (m_iCurrentTrack < 0)
		clearSelect();

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
	Item *pItem = m_tracks.value(pTrack, NULL);
	if (pItem)
		pItem->updateItem(this);

	updateContents();
}


// Update the list view item from MIDI manager pointer reference.
void qtractorTrackList::updateMidiTrackItem ( qtractorMidiManager *pMidiManager )
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		Item *pItem = iter.next();
		qtractorTrack *pTrack = pItem->track;
		if (pTrack && pTrack->trackType() == qtractorTrack::Midi) {
			qtractorPluginList *pPluginList = pTrack->pluginList();
			if (pPluginList && pPluginList->midiManager() == pMidiManager) {
				qtractorMidiComboMeter *pMidiComboMeter
					= static_cast<qtractorMidiComboMeter *> (pItem->meters);
				if (pMidiComboMeter) {
					if (pMidiManager->isAudioOutputMonitor()) {
						pMidiComboMeter->setAudioOutputMonitor(
							pMidiManager->audioOutputMonitor());
					} else {
						pMidiComboMeter->setAudioOutputMonitor(NULL);
					}
				}
				break;
			}
		}
	}
}


// Track-button colors (palette) update.
void qtractorTrackList::updateTrackButtons (void)
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext()) {
		const Item *pItem = iter.next();
		if (pItem->buttons)
			pItem->buttons->updateTrackButtons();
	}
}


// Update all track-items/icons methods.
void qtractorTrackList::updateItems (void)
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateItem(this);

	updateContents();
}


void qtractorTrackList::updateIcons (void)
{
	QListIterator<Item *> iter(m_items);
	while (iter.hasNext())
		iter.next()->updateIcon(this);

	updateContents();
}


// Main table cleaner.
void qtractorTrackList::clear (void)
{
	m_select.clear();

	m_iCurrentTrack = -1;

	m_dragState  = DragNone;
	m_iDragTrack = -1;
	m_iDragY     = 0;

	if (m_pRubberBand)
		delete m_pRubberBand;
	m_pRubberBand = NULL;

	qDeleteAll(m_items);
	m_items.clear();
	m_tracks.clear();
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


// Check/update header resize.
void qtractorTrackList::updateHeaderSize ( int iCol, int, int iColSize )
{
	const bool bBlockSignals = m_pHeader->blockSignals(true);
	if (iCol == Name && iColSize < 110) {
		// Make sure this column stays legible...
		m_pHeader->resizeSection(iCol, 110);
	}
	else
	if (iCol == Number) {
		// Resize all icons anyway...
		updateIcons();
	}
	else
	if (iCol == Channel) {
		// Reset all meter sizes anyway...
		updateItems();
	}
	m_pHeader->blockSignals(bBlockSignals);

	updateHeader();
}


// Reset header extents.
void qtractorTrackList::resetHeaderSize ( int iCol )
{
	const bool bBlockSignals = m_pHeader->blockSignals(true);
	m_pHeader->resizeSection(iCol, m_pHeader->sectionSizeHint(iCol));
	if (iCol == Number) {
		// Resize all icons anyway...
		QListIterator<Item *> iter(m_items);
		while (iter.hasNext())
			iter.next()->updateIcon(this);
	}
	m_pHeader->blockSignals(bBlockSignals);

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


// Selection method.
void qtractorTrackList::selectTrack ( int iTrack, bool bSelect, bool bToggle )
{
	Item *pItem = m_items.at(iTrack);

	if (m_select.contains(iTrack)) {
		const unsigned int flags = pItem->flags;
		if ( (!bSelect && (flags & 2) == 0) ||
			(( bSelect && (flags & 3) == 3) && bToggle))
			pItem->flags &= ~1;
		else
		if ( ( bSelect && (flags & 2) == 0) ||
			((!bSelect && (flags & 3) == 2) && bToggle))
			pItem->flags |=  1;
	}
	else
	if (bSelect) {
		pItem->flags = 1;
		m_select.insert(iTrack, pItem);
	}
}


// Selection commit method.
void qtractorTrackList::updateSelect ( bool bCommit )
{
	// Remove unselected...
	QHash<int, Item *>::Iterator iter = m_select.begin();
	const QHash<int, Item *>::Iterator& iter_end = m_select.end();
	while (iter != iter_end) {
		Item *pItem = iter.value();
		if (bCommit) {
			if (pItem->flags & 1)
				pItem->flags |=  2;
			else
				pItem->flags &= ~2;
		}
		if ((pItem->flags & 3) == 0)
			iter = m_select.erase(iter);
		else
			++iter;
	}

	updateContents();
}


// Selection clear method.
void qtractorTrackList::clearSelect (void)
{
	int iUpdate = 0;

	// Clear all selected...
	QHash<int, Item *>::ConstIterator iter = m_select.constBegin();
	const QHash<int, Item *>::ConstIterator& iter_end = m_select.constEnd();
	for ( ; iter != iter_end; ++iter) {
		iter.value()->flags = 0;
		++iUpdate;
	}

	m_select.clear();

	if (iUpdate > 0)
		updateContents();
}


// Retrieve all current seleceted tracks but one.
QList<qtractorTrack *> qtractorTrackList::selectedTracks (
	qtractorTrack *pTrackEx, bool bAllTracks ) const
{
	QList<qtractorTrack *> tracks;

	// Grab current selected tracks; also check
	// whether given track is currently selected...
	bool bSelected = false;
	QHash<int, Item *>::ConstIterator iter = m_select.constBegin();
	const QHash<int, Item *>::ConstIterator& iter_end = m_select.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorTrack *pTrack = iter.value()->track;
		if (pTrackEx == pTrack)
			bSelected = true;
		else
			tracks.append(pTrack);
	}

	// Currentt selection is either empty or
	// given track is not currently selected...
	if (!bSelected) {
		tracks.clear();
		// Optionally apply to all tracks
		if (bAllTracks) {
			QListIterator<Item *> iter(m_items);
			while (iter.hasNext()) {
				qtractorTrack *pTrack = iter.next()->track;
				if (pTrackEx != pTrack)
					tracks.append(pTrack);
			}
		}
	}

	return tracks;
}


// Draw table cell.
void qtractorTrackList::drawCell (
	QPainter *pPainter, int iRow, int iCol, const QRect& rect ) const
{
	const QPalette& pal = qtractorScrollView::palette();
	const Item *pItem = m_items.at(iRow);
	QColor bg, fg;
	if (iCol == Number) {
		bg = (pItem->track)->foreground().lighter();
		fg = (pItem->track)->background().lighter();
	} else if (pItem->flags & 1) {
		bg = pal.highlight().color();
		fg = pal.highlightedText().color();
		if (m_iCurrentTrack == iRow)
			bg = bg.darker(140);
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
		if (pItem->icon.isNull()
			|| rect.height() > qtractorTrack::HeightMin + 4) {
			pPainter->drawText(rectText,
				Qt::AlignHCenter | Qt::AlignTop,
				QString::number(iRow + 1));
		}
		if (!pItem->icon.isNull()) {
			const int x = rect.left()
				+ ((rect.width() - pItem->icon.width()) >> 1);
			const int y = rect.bottom()	- (pItem->icon.height() + 2);
			pPainter->drawPixmap(x, y, pItem->icon) ;
		}
	} else if (iCol == Channel) {
		if ((pItem->track)->trackType() == qtractorTrack::Midi
			|| pItem->meters == NULL) {
			pPainter->drawText(rectText,
				Qt::AlignHCenter | Qt::AlignTop,
				pItem->text.at(iCol - 1));
		}
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
			if (pItem->plugins) {
				QPalette pal2(pal);
				if (m_iCurrentTrack == iRow) {
					const QColor& rgbBase
						= pal2.midlight().color();
					pal2.setColor(QPalette::WindowText,
						pal2.highlightedText().color());
					pal2.setColor(QPalette::Window,
						rgbBase.darker(150));
				} else {
					const QColor& rgbBase = pal2.window().color();
					pal2.setColor(QPalette::WindowText,
						pal2.windowText().color());
					pal2.setColor(QPalette::Window,
						rgbBase);
				}
				pItem->plugins->setPalette(pal2);
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
					if (iCol == Name && pItem->buttons) {
						const QSize sizeButtons
							= (pItem->buttons)->sizeHint();
						QRect rectButtons = rect;
						rectButtons.setTop(rect.bottom() - sizeButtons.height());
						rectButtons.setLeft(rect.right() - sizeButtons.width());
						if (rectButtons.height() < h1) {
							(pItem->buttons)->setGeometry(rectButtons);
							(pItem->buttons)->show();
						} else {
							(pItem->buttons)->hide();
						}
					}
					else
					if (iCol == Bus && pItem->plugins) {
						if (rect.height() > qtractorTrack::HeightBase) {
							const int dy1 = qtractorTrack::HeightBase
								- qtractorTrack::HeightMin;
							(pItem->plugins)->setGeometry(
								rect.adjusted(+4, dy1, -3, -2));
							(pItem->plugins)->show();
						} else {
							(pItem->plugins)->hide();
						}
					}
					else
					if (iCol == Channel && pItem->meters) {
						const int dy1
							= ((pItem->track)->trackType()
								== qtractorTrack::Midi ? 20 : 4);
						(pItem->meters)->setGeometry(
							rect.adjusted(+4, dy1, -3, -2));
						(pItem->meters)->show();
					}
				}
				else if (iCol == Name && pItem->buttons)
					(pItem->buttons)->hide();
				else if (iCol == Bus && pItem->plugins)
					(pItem->plugins)->hide();
				else if (iCol == Channel && pItem->meters)
					(pItem->meters)->hide();
				x += dx;
			}
		}
		else {
			 if (pItem->buttons)
				(pItem->buttons)->hide();
			 if (pItem->plugins)
				(pItem->plugins)->hide();
			 if (pItem->meters)
				(pItem->meters)->hide();
		}
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
	if (pMainForm) {
		pMainForm->stabilizeForm();
		pMainForm->trackMenu()->exec(pContextMenuEvent->globalPos());
	}
}



// Handle mouse double-clicks.
void qtractorTrackList::mouseDoubleClickEvent ( QMouseEvent *pMouseEvent )
{
	qtractorTrack *pTrack = NULL;

	if (pMouseEvent->button() == Qt::LeftButton) {
		const QPoint& pos
			= qtractorScrollView::viewportToContents(pMouseEvent->pos());
		const int iTrack = trackRowAt(pos);
		if (iTrack >= 0 && trackColumnAt(pos) == Number) {
			const QRect& rect = trackRect(iTrack);
			if (iTrack > 0 && pos.y() >= rect.top() && pos.y() < rect.top() + 4)
				pTrack = track(iTrack - 1);
			else
			if (pos.y() >= rect.bottom() - 4 && pos.y() < rect.bottom() + 4)
				pTrack = track(iTrack);
		}
	}

	// Avoid lingering mouse press/release states...
	resetDragState();

	// Do we reset track height or go for its properties?
	if (pTrack) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			const int iZoomHeight = qtractorTrack::HeightBase;
			pSession->execute(
				new qtractorResizeTrackCommand(pTrack, iZoomHeight));
		}
	} else {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->trackProperties();
	}
}


// Handle item selection/dragging -- mouse button press.
void qtractorTrackList::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	// Now set ready for drag something...
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

	// Select current track...
	const int iTrack = trackRowAt(pos);

	// Make it current anyway...
	setCurrentTrackRow(iTrack);

	if (pMouseEvent->button() == Qt::LeftButton) {
		// Try for drag-move/resize/select later...
		m_dragState = DragStart;
		m_posDrag = pos;
		// Look for the mouse hovering around some item boundary...
		if (iTrack >= 0) {
			// Make current row always selected...
			const Qt::KeyboardModifiers& modifiers = pMouseEvent->modifiers();
			if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
				clearSelect();
			} else {
				selectTrack(iTrack, true, (modifiers & Qt::ControlModifier));
				updateSelect(true);
			}
			if (m_iDragTrack >= 0) {
				// Most probably DragResize is next...
				qtractorScrollView::setCursor(QCursor(Qt::SplitVCursor));
			} else {
				// Most probably DragMove is next...
				qtractorScrollView::setCursor(QCursor(Qt::PointingHandCursor));
				m_iDragTrack = iTrack;
				m_iDragY = 0;
			}
		}
	}

//	qtractorScrollView::mousePressEvent(pMouseEvent);
}


// Handle item selection/dragging -- mouse pointer move.
void qtractorTrackList::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

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
			// Go for it, immediately...
			qtractorTrack *pTrack = track(m_iDragTrack);
			if (pTrack) {
				const int iZoomHeight = y - m_iDragY;
				pTrack->setZoomHeight(iZoomHeight);
				Item *pItem = m_tracks.value(pTrack, NULL);
				if (pItem)
					pItem->updateIcon(this);
				m_pTracks->trackView()->updateContents();
				updateContentsHeight();
			}
		}
		break;
	case DragSelect: {
		// Currently selecting an group of items...
		if (m_iDragTrack >= 0 && m_iCurrentTrack >= 0) {
			const int iTrack = trackRowAt(pos);
			ensureVisibleRect(trackRect(iTrack));
			if (iTrack >= 0 && m_iDragTrack != iTrack) {
				const Qt::KeyboardModifiers& modifiers = pMouseEvent->modifiers();
				const bool bToggle = (modifiers & Qt::ControlModifier);
				if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0)
					selectTrack(m_iCurrentTrack, true, bToggle);
				if (iTrack < m_iCurrentTrack) {
					for (int i = iTrack; i < m_iCurrentTrack; ++i)
						selectTrack(i, true, bToggle);
				} else if (iTrack > m_iCurrentTrack) {
					for (int i = iTrack; i > m_iCurrentTrack; --i)
						selectTrack(i, true, bToggle);
				}
				if ((m_iDragTrack > iTrack && m_iDragTrack > m_iCurrentTrack) ||
					(m_iDragTrack < iTrack && m_iDragTrack < m_iCurrentTrack)) {
					const bool bSelect = !m_select.contains(m_iDragTrack);
					selectTrack(m_iDragTrack, bSelect, bToggle);
				}
			//	selectTrack(iTrack, true, bToggle);
				updateSelect(false);
				m_iDragTrack = iTrack;
			}
		}
		// Update the rubber-band anyway...
		moveRubberBand(QRect(m_posDrag, pos));
		break;
	}
	case DragStart:
		// About to start dragging an item...
		if ((m_posDrag - pos).manhattanLength()
				> QApplication::startDragDistance()) {
			if (m_iDragTrack >= 0) {
				if (trackColumnAt(pos) == Number) {
					qtractorScrollView::setCursor(QCursor(Qt::SizeVerCursor));
					const QRect& rect = trackRect(m_iDragTrack);
					if (m_iDragTrack > 0 &&
						m_posDrag.y() >= rect.top() &&
						m_posDrag.y() <  rect.top() + 4) {
						m_dragState = DragResize;
						m_posDrag = rect.topLeft();
						--m_iDragTrack;
					}
					else
					if (m_posDrag.y() >= rect.bottom() - 4 &&
						m_posDrag.y() <  rect.bottom() + 4) {
						m_dragState = DragResize;
						m_posDrag = rect.bottomLeft();
					} else {
						m_dragState = DragMove;
						m_posDrag = rect.topLeft();
					}
					moveRubberBand(m_posDrag);
				} else {
					qtractorScrollView::setCursor(QCursor(Qt::CrossCursor));
					m_dragState = DragSelect;
					moveRubberBand(QRect(m_posDrag, pos));
				}
				// Special case if one wishes to undo a track's height...
				if (m_dragState == DragResize) {
					qtractorTrack *pTrack = track(m_iDragTrack);
					qtractorSession *pSession = qtractorSession::getInstance();
					if (pTrack && pSession) {
						const int iZoomHeight = pTrack->zoomHeight();
						pSession->commands()->push(
							new qtractorResizeTrackCommand(pTrack, iZoomHeight));
						m_pTracks->dirtyChangeNotify();
					}
				}
			}
		}
		break;
	case DragNone: {
		// Look for the mouse hovering around first column item boundary...
		int iTrack = trackRowAt(pos);
		if (iTrack >= 0 && trackColumnAt(pos) == Number) {
			m_posDrag = pos;
			const QRect& rect = trackRect(iTrack);
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
		// Fall thru...
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

	const QPoint& pos
		= qtractorScrollView::viewportToContents(pMouseEvent->pos());

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
					clearSelect();
					pSession->execute(
						new qtractorMoveTrackCommand(pTrack, pTrackDrop));
				}
			}
		}
		break;
	case DragSelect:
		if (m_iDragTrack >= 0)
			updateSelect(true);
		break;
	case DragStart:
		// Special attitude, only of interest on
		// the first left-most column (track-number)...
		if (trackColumnAt(pos) == Number) {
			m_pTracks->selectCurrentTrack((pMouseEvent->modifiers()
				& (Qt::ShiftModifier | Qt::ControlModifier)) == 0);
			qtractorScrollView::setFocus(); // Get focus back anyway.
		}
		break;
	case DragResize: {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->thumbView()->updateContents();
		break;
	}
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


// Drag-n-drop event handlers.
void qtractorTrackList::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
	const QMimeData *pMimeData = pDragEnterEvent->mimeData();
	if (pMimeData && pMimeData->hasUrls())
		pDragEnterEvent->accept();
	else
		pDragEnterEvent->ignore();
}


void qtractorTrackList::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	// Now set ready for drag something...
	const QPoint& pos
		= qtractorScrollView::viewportToContents(pDragMoveEvent->pos());

	// Select current track...
	const int iTrack = trackRowAt(pos);

	// Make it current anyway...
	setCurrentTrackRow(iTrack);
}


void qtractorTrackList::dropEvent ( QDropEvent *pDropEvent )
{
	// Can we decode it as Audio/MIDI files?
	const QMimeData *pMimeData = pDropEvent->mimeData();
	if (pMimeData == NULL || !pMimeData->hasUrls())
		return;

	// Let's see how many files there are
	// to split between audio/MIDI files...
	QStringList audio_files;
	QStringList midi_files;

	QListIterator<QUrl> iter(pMimeData->urls());
	while (iter.hasNext()) {
		const QString& sPath = iter.next().toLocalFile();
		if (sPath.isEmpty())
			continue;
		// Try first as a MIDI file...
		qtractorMidiFile file;
		if (file.open(sPath)) {
			midi_files.append(sPath);
			file.close();
			continue;
		}
		// Then as an audio file ?
		qtractorAudioFile *pFile
			= qtractorAudioFileFactory::createAudioFile(sPath);
		if (pFile) {
			if (pFile->open(sPath)) {
				audio_files.append(sPath);
				pFile->close();
			}
			delete pFile;
			continue;
		}
	}


	// Depending on import type...
	qtractorSession *pSession = qtractorSession::getInstance();
	const unsigned long iClipStart = (pSession ? pSession->editHead() : 0);
	qtractorTrack *pAfterTrack = currentTrack();

	if (!midi_files.isEmpty())
		m_pTracks->addMidiTracks(midi_files, iClipStart, pAfterTrack);
	if (!audio_files.isEmpty())
		m_pTracks->addAudioTracks(audio_files, iClipStart, 0, 0, pAfterTrack);

	if (midi_files.isEmpty() && audio_files.isEmpty()) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm)
			pMainForm->dropEvent(pDropEvent);
	}
}


// Draw a dragging separator line.
void qtractorTrackList::moveRubberBand ( const QPoint& posDrag )
{
	const QPoint& pos
		= qtractorScrollView::contentsToViewport(posDrag);

	// Create the rubber-band if there's none...
	if (m_pRubberBand == NULL) {
		m_pRubberBand = new qtractorRubberBand(
			QRubberBand::Line, qtractorScrollView::viewport());
	#if 0
		QPalette pal(m_pRubberBand->palette());
		pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
		m_pRubberBand->setPalette(pal);
		m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	#endif
	}
	
	// Just move it
	m_pRubberBand->setGeometry(
		QRect(0, pos.y() - 1, qtractorScrollView::viewport()->width(), 3));

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Draw a lasso rectangle.
void qtractorTrackList::moveRubberBand ( const QRect& rectDrag )
{
	QRect rect(rectDrag.normalized());

	rect.moveTopLeft(qtractorScrollView::contentsToViewport(rect.topLeft()));

	// Create the rubber-band if there's none...
	if (m_pRubberBand == NULL) {
		m_pRubberBand = new qtractorRubberBand(
			QRubberBand::Rectangle, qtractorScrollView::viewport());
	#if 0
		QPalette pal(m_pRubberBand->palette());
		pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
		m_pRubberBand->setPalette(pal);
		m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	#endif
	}

	// Just move it
	m_pRubberBand->setGeometry(rect);

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

	if (rect.top() < cy + hh) {
		qtractorScrollView::ensureVisible(cx, rect.top(), 0, 24);
		return true;
	}

	if (rect.bottom() > cy + qtractorScrollView::viewport()->height()) {
		qtractorScrollView::ensureVisible(cx, rect.bottom() - hh, 0, 24);
		return true;
	}

	return false;
}


// Reset drag/select/move state.
void qtractorTrackList::resetDragState (void)
{
	// Cancel any dragging out there...
	// Just hide the rubber-band...
	if (m_pRubberBand) {
	//	m_pRubberBand->hide();
		delete m_pRubberBand;
		m_pRubberBand = NULL;
	}

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
	const Qt::KeyboardModifiers& modifiers = pKeyEvent->modifiers();

	switch (pKeyEvent->key()) {
	case Qt::Key_Escape:
		resetDragState();
		clearSelect();
		break;
	case Qt::Key_Home:
		if (m_iCurrentTrack  > 0) {
			const int iTrack = 0;
			if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
				const bool bToggle = (modifiers & Qt::ControlModifier);
				for (int i = iTrack; m_iCurrentTrack >= i; ++i)
					selectTrack(i, true, bToggle);
				updateSelect(true);
			}
			setCurrentTrackRow(iTrack);
		}
		break;
	case Qt::Key_End:
		if (m_iCurrentTrack  < m_items.count() - 1) {
			const int iTrack = m_items.count() - 1;
			if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
				const bool bToggle = (modifiers & Qt::ControlModifier);
				for (int i = iTrack; i >= m_iCurrentTrack; --i)
					selectTrack(i, true, bToggle);
				updateSelect(true);
			}
			setCurrentTrackRow(iTrack);
		}
		break;
	case Qt::Key_Left:
		if (modifiers & Qt::ControlModifier) {
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
		if (modifiers & Qt::ControlModifier) {
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
		if (m_iCurrentTrack > 0) {
			const int iTrack = m_iCurrentTrack - 1;
			if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
				const bool bToggle = (modifiers & Qt::ControlModifier);
				selectTrack(m_iCurrentTrack, true, bToggle);
				selectTrack(iTrack, true, bToggle);
				updateSelect(true);
			}
			setCurrentTrackRow(iTrack);
		}
		break;
	case Qt::Key_Down:
		if (m_iCurrentTrack < m_items.count() - 1) {
			const int iTrack = m_iCurrentTrack + 1;
			if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
				const bool bToggle = (modifiers & Qt::ControlModifier);
				selectTrack(m_iCurrentTrack, true, bToggle);
				selectTrack(iTrack, true, bToggle);
				updateSelect(true);
			}
			setCurrentTrackRow(iTrack);
		}
		break;
	case Qt::Key_PageUp:
		if (modifiers & Qt::ControlModifier) {
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
		if (modifiers & Qt::ControlModifier) {
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
