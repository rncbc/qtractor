// qtractorMidiEditor.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorMidiEditor.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditList.h"
#include "qtractorMidiEditTime.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiEditCommand.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"

#include "qtractorMidiToolsForm.h"

#include "qtractorInstrument.h"
#include "qtractorRubberBand.h"
#include "qtractorTimeScale.h"

#include "qtractorSession.h"
#include "qtractorMainForm.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QPixmap>
#include <QFrame>
#include <QIcon>
#include <QPainter>

#include <QFileInfo>
#include <QDir>

#include <QComboBox>
#include <QToolTip>


//----------------------------------------------------------------------------
// MIDI Note Names - Default note names hash map.

static struct
{
	unsigned char note;
	const char *name;

} g_aNoteNames[] = {

	// Diatonic note map...
	{  0, QT_TR_NOOP("C")  },
	{  1, QT_TR_NOOP("C#") },
	{  2, QT_TR_NOOP("D")  },
	{  3, QT_TR_NOOP("D#") },
	{  4, QT_TR_NOOP("E")  },
	{  5, QT_TR_NOOP("F")  },
	{  6, QT_TR_NOOP("F#") },
	{  7, QT_TR_NOOP("G")  },
	{  8, QT_TR_NOOP("G#") },
	{  9, QT_TR_NOOP("A")  },
	{ 10, QT_TR_NOOP("A#") },
	{ 11, QT_TR_NOOP("B")  },

	// GM Drum note map...
	{ 35, QT_TR_NOOP("Acoustic Bass Drum") },
	{ 36, QT_TR_NOOP("Bass Drum 1") },
	{ 37, QT_TR_NOOP("Side Stick") },
	{ 38, QT_TR_NOOP("Acoustic Snare") },
	{ 39, QT_TR_NOOP("Hand Clap") },
	{ 40, QT_TR_NOOP("Electric Snare") },
	{ 41, QT_TR_NOOP("Low Floor Tom") },
	{ 42, QT_TR_NOOP("Closed Hi-Hat") },
	{ 43, QT_TR_NOOP("High Floor Tom") },
	{ 44, QT_TR_NOOP("Pedal Hi-Hat") },
	{ 45, QT_TR_NOOP("Low Tom") },
	{ 46, QT_TR_NOOP("Open Hi-Hat") },
	{ 47, QT_TR_NOOP("Low-Mid Tom") },
	{ 48, QT_TR_NOOP("Hi-Mid Tom") },
	{ 49, QT_TR_NOOP("Crash Cymbal 1") },
	{ 50, QT_TR_NOOP("High Tom") },
	{ 51, QT_TR_NOOP("Ride Cymbal 1") },
	{ 52, QT_TR_NOOP("Chinese Cymbal") },
	{ 53, QT_TR_NOOP("Ride Bell") },
	{ 54, QT_TR_NOOP("Tambourine") },
	{ 55, QT_TR_NOOP("Splash Cymbal") },
	{ 56, QT_TR_NOOP("Cowbell") },
	{ 57, QT_TR_NOOP("Crash Cymbal 2") },
	{ 58, QT_TR_NOOP("Vibraslap") },
	{ 59, QT_TR_NOOP("Ride Cymbal 2") },
	{ 60, QT_TR_NOOP("Hi Bongo") },
	{ 61, QT_TR_NOOP("Low Bongo") },
	{ 62, QT_TR_NOOP("Mute Hi Conga") },
	{ 63, QT_TR_NOOP("Open Hi Conga") },
	{ 64, QT_TR_NOOP("Low Conga") },
	{ 65, QT_TR_NOOP("High Timbale") },
	{ 66, QT_TR_NOOP("Low Timbale") },
	{ 67, QT_TR_NOOP("High Agogo") },
	{ 68, QT_TR_NOOP("Low Agogo") },
	{ 69, QT_TR_NOOP("Cabasa") },
	{ 70, QT_TR_NOOP("Maracas") },
	{ 71, QT_TR_NOOP("Short Whistle") },
	{ 72, QT_TR_NOOP("Long Whistle") },
	{ 73, QT_TR_NOOP("Short Guiro") },
	{ 74, QT_TR_NOOP("Long Guiro") },
	{ 75, QT_TR_NOOP("Claves") },
	{ 76, QT_TR_NOOP("Hi Wood Block") },
	{ 77, QT_TR_NOOP("Low Wood Block") },
	{ 78, QT_TR_NOOP("Mute Cuica") },
	{ 79, QT_TR_NOOP("Open Cuica") },
	{ 80, QT_TR_NOOP("Mute Triangle") },
	{ 81, QT_TR_NOOP("Open Triangle") },

	{  0, NULL }
};

static QHash<unsigned char, QString> g_noteNames;

// Default note name map accessor.
const QString qtractorMidiEditor::defaultNoteName (
	unsigned char note, bool fDrums )
{
	if (fDrums) {
		// Pre-load drum-names hash table...
		if (g_noteNames.isEmpty()) {
			for (int i = 13; g_aNoteNames[i].name; ++i) {
				g_noteNames.insert(
					g_aNoteNames[i].note,
					tr(g_aNoteNames[i].name));
			}
		}
		// Check whether the drum note exists...
		QHash<unsigned char, QString>::ConstIterator iter
			= g_noteNames.constFind(note);
		if (iter != g_noteNames.constEnd())
			return iter.value();
	}

	return tr(g_aNoteNames[note % 12].name) + QString::number((note / 12) - 2);
}


//----------------------------------------------------------------------------
// MIDI Controller Names - Default controller names hash map.

static struct
{
	unsigned char controller;
	const char *name;

} g_aControllerNames[] = {

	{  0, QT_TR_NOOP("Bank Select (coarse)") },
	{  1, QT_TR_NOOP("Modulation Wheel (coarse)") },
	{  2, QT_TR_NOOP("Breath Controller (coarse)") },
	{  4, QT_TR_NOOP("Foot Pedal (coarse)") },
	{  5, QT_TR_NOOP("Portamento Time (coarse)") },
	{  6, QT_TR_NOOP("Data Entry (coarse)") },
	{  7, QT_TR_NOOP("Volume (coarse)") },
	{  8, QT_TR_NOOP("Balance (coarse)") },
	{ 10, QT_TR_NOOP("Pan Position (coarse)") },
	{ 11, QT_TR_NOOP("Expression (coarse)") },
	{ 12, QT_TR_NOOP("Effect Control 1 (coarse)") },
	{ 13, QT_TR_NOOP("Effect Control 2 (coarse)") },
	{ 16, QT_TR_NOOP("General Purpose Slider 1") },
	{ 17, QT_TR_NOOP("General Purpose Slider 2") },
	{ 18, QT_TR_NOOP("General Purpose Slider 3") },
	{ 19, QT_TR_NOOP("General Purpose Slider 4") },
	{ 32, QT_TR_NOOP("Bank Select (fine)") },
	{ 33, QT_TR_NOOP("Modulation Wheel (fine)") },
	{ 34, QT_TR_NOOP("Breath Controller (fine)") },
	{ 36, QT_TR_NOOP("Foot Pedal (fine)") },
	{ 37, QT_TR_NOOP("Portamento Time (fine)") },
	{ 38, QT_TR_NOOP("Data Entry (fine)") },
	{ 39, QT_TR_NOOP("Volume (fine)") },
	{ 40, QT_TR_NOOP("Balance (fine)") },
	{ 42, QT_TR_NOOP("Pan Position (fine)") },
	{ 43, QT_TR_NOOP("Expression (fine)") },
	{ 44, QT_TR_NOOP("Effect Control 1 (fine)") },
	{ 45, QT_TR_NOOP("Effect Control 2 (fine)") },
	{ 64, QT_TR_NOOP("Hold Pedal (on/off)") },
	{ 65, QT_TR_NOOP("Portamento (on/off)") },
	{ 66, QT_TR_NOOP("Sustenuto Pedal (on/off)") },
	{ 67, QT_TR_NOOP("Soft Pedal (on/off)") },
	{ 68, QT_TR_NOOP("Legato Pedal (on/off)") },
	{ 69, QT_TR_NOOP("Hold 2 Pedal (on/off)") },
	{ 70, QT_TR_NOOP("Sound Variation") },
	{ 71, QT_TR_NOOP("Sound Timbre") },
	{ 72, QT_TR_NOOP("Sound Release Time") },
	{ 73, QT_TR_NOOP("Sound Attack Time") },
	{ 74, QT_TR_NOOP("Sound Brightness") },
	{ 75, QT_TR_NOOP("Sound Control 6") },
	{ 76, QT_TR_NOOP("Sound Control 7") },
	{ 77, QT_TR_NOOP("Sound Control 8") },
	{ 78, QT_TR_NOOP("Sound Control 9") },
	{ 79, QT_TR_NOOP("Sound Control 10") },
	{ 80, QT_TR_NOOP("General Purpose Button 1 (on/off)") },
	{ 81, QT_TR_NOOP("General Purpose Button 2 (on/off)") },
	{ 82, QT_TR_NOOP("General Purpose Button 3 (on/off)") },
	{ 83, QT_TR_NOOP("General Purpose Button 4 (on/off)") },
	{ 91, QT_TR_NOOP("Effects Level") },
	{ 92, QT_TR_NOOP("Tremulo Level") },
	{ 93, QT_TR_NOOP("Chorus Level") },
	{ 94, QT_TR_NOOP("Celeste Level") },
	{ 95, QT_TR_NOOP("Phaser Level") },
	{ 96, QT_TR_NOOP("Data Button Increment") },
	{ 97, QT_TR_NOOP("Data Button Decrement") },
	{ 98, QT_TR_NOOP("Non-Registered Parameter (fine)") },
	{ 99, QT_TR_NOOP("Non-Registered Parameter (coarse)") },
	{100, QT_TR_NOOP("Registered Parameter (fine)") },
	{101, QT_TR_NOOP("Registered Parameter (coarse)") },
	{120, QT_TR_NOOP("All Sound Off") },
	{121, QT_TR_NOOP("All Controllers Off") },
	{122, QT_TR_NOOP("Local Keyboard (on/off)") },
	{123, QT_TR_NOOP("All Notes Off") },
	{124, QT_TR_NOOP("Omni Mode Off") },
	{125, QT_TR_NOOP("Omni Mode On") },
	{126, QT_TR_NOOP("Mono Operation") },
	{127, QT_TR_NOOP("Poly Operation") },
	{  0, NULL }
};

static QHash<unsigned char, QString> g_controllerNames;

// Default controller name accessor.
const QString& qtractorMidiEditor::defaultControllerName ( unsigned char controller )
{
	if (g_controllerNames.isEmpty()) {
		// Pre-load controller-names hash table...
		for (int i = 0; g_aControllerNames[i].name; ++i) {
			g_controllerNames.insert(
				g_aControllerNames[i].controller,
				tr(g_aControllerNames[i].name));
		}
	}

	return g_controllerNames[controller];
}


//----------------------------------------------------------------------------
// qtractorMidiEdit::ClipBoard - MIDI editor clipaboard singleton.

// Singleton declaration.
qtractorMidiEditor::ClipBoard qtractorMidiEditor::g_clipboard;


//----------------------------------------------------------------------------
// qtractorMidiEditor -- All-in-one SMF file writer/creator method.

bool qtractorMidiEditor::saveCopyFile ( const QString& sNewFilename,
	const QString& sOldFilename, unsigned short iTrackChannel,
	qtractorMidiSequence *pSeq, qtractorTimeScale *pTimeScale,
	unsigned short iFormat )
{
	qtractorMidiFile file;
	float fTempo = (pTimeScale ? pTimeScale->tempo() : 120.0f);
	unsigned short iTicksPerBeat = (pTimeScale ? pTimeScale->ticksPerBeat() : 96);
	unsigned short iBeatsPerBar  = (pTimeScale ? pTimeScale->beatsPerBar()  : 4);
	unsigned short iSeq, iSeqs = 0;
	qtractorMidiSequence **ppSeqs = NULL;
	const QString sTrackName = QObject::tr("Track %1");
	
	if (pSeq == NULL)
		return false;

	// Open and load the whole source file...
	if (file.open(sOldFilename)) {
		iTicksPerBeat = file.ticksPerBeat();
		iFormat = file.format();
		iSeqs = (iFormat == 1 ? file.tracks() : 16);	
		ppSeqs = new qtractorMidiSequence * [iSeqs];
		for (iSeq = 0; iSeq < iSeqs; ++iSeq) {	
			ppSeqs[iSeq] = new qtractorMidiSequence(
				sTrackName.arg(iSeq + 1), iSeq, iTicksPerBeat);
		}
		if (file.readTracks(ppSeqs, iSeqs)) {
			fTempo = file.tempo();
			iBeatsPerBar = file.beatsPerBar();
		}
		file.close();
	}
	
	// Open and save the whole target file...
	if (!file.open(sNewFilename, qtractorMidiFile::Write))
		return false;

	file.setTempo(fTempo);
	file.setBeatsPerBar(iBeatsPerBar);

	if (ppSeqs == NULL)
		iSeqs = (iFormat == 0 ? 1 : 2);

	// Write SMF header...
	file.writeHeader(iFormat, (iFormat == 0 ? 1 : iSeqs), iTicksPerBeat);

	// Write SMF tracks(s)...
	if (ppSeqs) {
		// Replace the target track-channel events... 
		ppSeqs[iTrackChannel]->replaceEvents(pSeq);
		// Write the whole new tracks...
		file.writeTracks(ppSeqs, iSeqs);
	} else {
		// Most probabley this is a brand new file...
		if (iFormat == 1)
			file.writeTrack(NULL);
		file.writeTrack(pSeq);
	}

	file.close();

	// Free locally allocated track/sequence array.
	if (ppSeqs) {
		for (iSeq = 0; iSeq < iSeq; ++iSeq)
			delete ppSeqs[iSeq];
		delete [] ppSeqs;
	}

	// HACK: This invasive operation is so important that
	// it surely deserves being in the front page...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		pMainForm->appendMessages(
			tr("MIDI file save: \"%1\", track-channel: %2.")
			.arg(sNewFilename).arg(iTrackChannel));
		pMainForm->addMidiFile(sNewFilename);
	}

	return true;
}


// Create filename revision (name says it all).
QString qtractorMidiEditor::createFilePathRevision (
	const QString& sFilename, int iRevision )
{
	QFileInfo fi(sFilename);
	QDir adir(fi.absoluteDir());

	QRegExp rxRevision("(.+)\\-(\\d+)$");
	QString sBasename = fi.baseName();
	if (rxRevision.exactMatch(sBasename)) {
		sBasename = rxRevision.cap(1);
		iRevision = rxRevision.cap(2).toInt();
	}
	
	sBasename += "-%1." + fi.completeSuffix();
	do { fi.setFile(adir, sBasename.arg(++iRevision)); }
	while (fi.exists());

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEditor::createFilePathRevision(\"%s\")",
		fi.absoluteFilePath().toUtf8().constData());
#endif

	return fi.absoluteFilePath();
}


//----------------------------------------------------------------------------
// qtractorMidiEditor -- The main MIDI sequence editor widget.


// Constructor.
qtractorMidiEditor::qtractorMidiEditor ( QWidget *pParent )
		: QSplitter(Qt::Vertical, pParent)
{
	// Initialize instance variables...
	m_pMidiClip = NULL;

	// Event fore/background colors.
	m_foreground = Qt::darkBlue;
	m_background = Qt::blue;

	// Common drag state.
	m_dragState  = DragNone;
	m_resizeMode = ResizeNone;

	m_pEventDrag = NULL;
	m_bEventDragEdit = false;

	m_pRubberBand = NULL;

	// Edit mode flag.
	m_bEditMode = false;

	// Last default editing values.
	m_last.note      = 0x3c;	// middle-C
	m_last.value     = 0x40;
	m_last.pitchBend = 0;
	m_last.duration  = 0;

	// The local mighty command pattern instance.
	m_pCommands = new qtractorCommandList();

	// Local time-scale.
	m_pTimeScale = new qtractorTimeScale();
	m_iOffset = 0;
	m_iLength = 0;

	// Local edit-head/tail positioning.
	m_iEditHead  = 0;
	m_iEditHeadX = 0;
	m_iEditTail  = 0;
	m_iEditTailX = 0;

	// Local play-head positioning.
	m_iPlayHead  = 0;
	m_iPlayHeadX = 0;
	m_bSyncView  = false;

	// Note autition while editing.
	m_bSendNotes = false;

	// Event (note) duration rectangle vs. stick.
	m_bNoteDuration = false;

	// Event (note, velocity) coloring.
	m_bNoteColor  = false;
	m_bValueColor = false;

	// Which widget holds focus on drag-paste?
	m_pEditPaste = NULL;

	// Create child frame widgets...
	QSplitter *pSplitter = new QSplitter(Qt::Horizontal, this);
	QWidget *pVBoxLeft   = new QWidget(pSplitter);
	QWidget *pVBoxRight  = new QWidget(pSplitter);
	QWidget *pHBoxBottom = new QWidget(this);

	// Create child view widgets...
	m_pEditListHeader = new QFrame(pVBoxLeft);
	m_pEditListHeader->setFixedHeight(20);
	m_pEditList  = new qtractorMidiEditList(this, pVBoxLeft);
	m_pEditList->setMinimumWidth(32);
	m_pEditTime  = new qtractorMidiEditTime(this, pVBoxRight);
	m_pEditTime->setFixedHeight(20);
	m_pEditView  = new qtractorMidiEditView(this, pVBoxRight);
	m_pEditEventScale = new qtractorMidiEditEventScale(this, pHBoxBottom);
	m_pEditEvent = new qtractorMidiEditEvent(this, pHBoxBottom);
	m_pEditEventFrame = new QFrame(pHBoxBottom);
	m_pEditList->updateContentsHeight();

	// Create child box layouts...
	QVBoxLayout *pVBoxLeftLayout = new QVBoxLayout(pVBoxLeft);
	pVBoxLeftLayout->setMargin(0);
	pVBoxLeftLayout->setSpacing(0);
	pVBoxLeftLayout->addWidget(m_pEditListHeader);
	pVBoxLeftLayout->addWidget(m_pEditList);
	pVBoxLeft->setLayout(pVBoxLeftLayout);

	QVBoxLayout *pVBoxRightLayout = new QVBoxLayout(pVBoxRight);
	pVBoxRightLayout->setMargin(0);
	pVBoxRightLayout->setSpacing(0);
	pVBoxRightLayout->addWidget(m_pEditTime);
	pVBoxRightLayout->addWidget(m_pEditView);
	pVBoxRight->setLayout(pVBoxRightLayout);

	QHBoxLayout *pHBoxBottomLayout = new QHBoxLayout(pHBoxBottom);
	pHBoxBottomLayout->setMargin(0);
	pHBoxBottomLayout->setSpacing(0);
	pHBoxBottomLayout->addWidget(m_pEditEventScale);
	pHBoxBottomLayout->addWidget(m_pEditEvent);
	pHBoxBottomLayout->addWidget(m_pEditEventFrame);
	pHBoxBottom->setLayout(pHBoxBottomLayout);

//	pSplitter->setOpaqueResize(false);
	pSplitter->setStretchFactor(pSplitter->indexOf(pVBoxLeft), 0);
	pSplitter->setHandleWidth(2);

//	QSplitter::setOpaqueResize(false);
	QSplitter::setStretchFactor(QSplitter::indexOf(pHBoxBottom), 0);
	QSplitter::setHandleWidth(2);

	QSplitter::setWindowIcon(QIcon(":/icons/qtractorMidiEditor.png"));
	QSplitter::setWindowTitle(tr("MIDI Editor"));

	// To have all views in positional sync.
	QObject::connect(m_pEditList, SIGNAL(contentsMoving(int,int)),
		m_pEditView, SLOT(contentsYMovingSlot(int,int)));
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditList, SLOT(contentsYMovingSlot(int,int)));
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditEvent, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pEditEvent, SIGNAL(contentsMoving(int,int)),
		m_pEditTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pEditEvent, SIGNAL(contentsMoving(int,int)),
		m_pEditView, SLOT(contentsXMovingSlot(int,int)));

	QObject::connect(m_pCommands,
		SIGNAL(updateNotifySignal(bool)),
		SLOT(updateNotifySlot(bool)));

	// FIXME: Initial horizontal splitter sizes.
	QList<int> sizes;
	sizes.append(48);
	sizes.append(752);
	pSplitter->setSizes(sizes);
}


// Destructor.
qtractorMidiEditor::~qtractorMidiEditor (void)
{
	resetDragState(NULL);

	// Release local instances.
	delete m_pTimeScale;
	delete m_pCommands;
}


// Close event override to emit respective signal.
void qtractorMidiEditor::closeEvent ( QCloseEvent *pCloseEvent )
{
	emit closeNotifySignal();

	QWidget::closeEvent(pCloseEvent);
}


// Editing sequence accessor.
void qtractorMidiEditor::setMidiClip ( qtractorMidiClip *pMidiClip )
{
	// So, this is the brand new object to edit...
	m_pMidiClip = pMidiClip;

	if (m_pMidiClip) {
		// Now set the editing MIDI sequence alright...
		setOffset(m_pMidiClip->clipStart());
		setLength(m_pMidiClip->clipLength());
		// Set its most outstanding properties...
		qtractorTrack *pTrack = m_pMidiClip->track();
		if (pTrack) {
			setForeground(pTrack->foreground());
			setBackground(pTrack->background());
		}
		// And the last but not least...
		qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
		if (pSeq) {
			// Reset some internal state...
			m_cursor.reset(pSeq);
			m_cursorAt.reset(pSeq);
			// Reset as last on middle note and snap duration...
			m_last.note = (pSeq->noteMin() + pSeq->noteMax()) >> 1;
			if (m_last.note == 0)
				m_last.note = 0x3c; // Default to middle-C.
		}
		// Got clip!
	} else {
		// Reset those little things too..
		setOffset(0);
		setLength(0);
	}

	// All commands reset.
	m_pCommands->clear();
}

qtractorMidiClip *qtractorMidiEditor::midiClip (void) const
{
	return m_pMidiClip;
}


// MIDI clip property accessors.
const QString& qtractorMidiEditor::filename (void) const
{
	return m_pMidiClip->filename();
}

unsigned short qtractorMidiEditor::trackChannel (void) const
{
	return (m_pMidiClip ? m_pMidiClip->trackChannel() : 0);
}

unsigned short qtractorMidiEditor::format (void) const
{
	return (m_pMidiClip ? m_pMidiClip->format() : 0);
}


qtractorMidiSequence *qtractorMidiEditor::sequence (void) const
{
	return (m_pMidiClip ? m_pMidiClip->sequence() : NULL);
}


// Event foreground (outline) color.
void qtractorMidiEditor::setForeground ( const QColor& fore )
{
	m_foreground = fore;
}

const QColor& qtractorMidiEditor::foreground (void) const
{
	return m_foreground;
}


// Event background (fill) color.
void qtractorMidiEditor::setBackground ( const QColor& back )
{
	m_background = back;
}

const QColor& qtractorMidiEditor::background (void) const
{
	return m_background;
}


// Edit (creational) mode.
void qtractorMidiEditor::setEditMode ( bool bEditMode )
{
	resetDragState(NULL);

	m_bEditMode = bEditMode;

//	updateContents();
}

bool qtractorMidiEditor::isEditMode() const
{
	return m_bEditMode;
}


// Local time scale accessor.
qtractorTimeScale *qtractorMidiEditor::timeScale (void) const
{
	return m_pTimeScale;
}


// Time-scale offset (in frames) accessors.
void qtractorMidiEditor::setOffset ( unsigned long iOffset )
{
	m_iOffset = iOffset;
}

unsigned long qtractorMidiEditor::offset (void) const
{
	return m_iOffset;
}


// Time-scale length (in frames) accessors.
void qtractorMidiEditor::setLength ( unsigned long iLength )
{
	m_iLength = iLength;
}

unsigned long qtractorMidiEditor::length (void) const
{
	return m_iLength;
}


// Edit-head/tail positioning.
void qtractorMidiEditor::setEditHead ( unsigned long iEditHead, bool bSync )
{
	if (iEditHead > m_iEditTail)
		setEditTail(iEditHead, bSync);

	if (bSync) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorSession *pSession = pMainForm->session();
			if (pSession)
				pSession->setEditHead(iEditHead);
		}
	}

	m_iEditHead = iEditHead;
	int iEditHeadX
		= m_pTimeScale->pixelFromFrame(iEditHead)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iEditHeadX, iEditHeadX, false);
}

unsigned long qtractorMidiEditor::editHead (void) const
{
	return m_iEditHead;
}

int qtractorMidiEditor::editHeadX (void) const
{
	return m_iEditHeadX;
}


void qtractorMidiEditor::setEditTail ( unsigned long iEditTail, bool bSync )
{
	if (iEditTail < m_iEditHead)
		setEditHead(iEditTail, bSync);

	if (bSync) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorSession *pSession = pMainForm->session();
			if (pSession)
				pSession->setEditTail(iEditTail);
		}
	}

	m_iEditTail = iEditTail;
	int iEditTailX
		= m_pTimeScale->pixelFromFrame(iEditTail)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iEditTailX, iEditTailX, false);
}

unsigned long qtractorMidiEditor::editTail (void) const
{
	return m_iEditTail;
}

int qtractorMidiEditor::editTailX (void) const
{
	return m_iEditTailX;
}


// Play-head positioning.
void qtractorMidiEditor::setPlayHead ( unsigned long iPlayHead, bool bSyncView )
{
	if (bSyncView)
		bSyncView = m_bSyncView;

	m_iPlayHead = iPlayHead;
	int iPlayHeadX
		= m_pTimeScale->pixelFromFrame(iPlayHead)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iPlayHeadX, iPlayHeadX, bSyncView);
}

unsigned long qtractorMidiEditor::playHead (void) const
{
	return m_iPlayHead;
}

int qtractorMidiEditor::playHeadX (void) const
{
	return m_iPlayHeadX;
}


// Play-head follow-ness.
void qtractorMidiEditor::setSyncView ( bool bSyncView )
{
	m_bSyncView = bSyncView;
}

bool qtractorMidiEditor::isSyncView (void) const
{
	return m_bSyncView;
}


// Note autition while editing.
void qtractorMidiEditor::setSendNotes ( bool bSendNotes )
{
	m_bSendNotes = bSendNotes;
}

bool qtractorMidiEditor::isSendNotes (void) const
{
	return m_bSendNotes;
}


// Event value stick vs. duration rectangle.
void qtractorMidiEditor::setNoteDuration ( bool bNoteDuration )
{
	m_bNoteDuration = bNoteDuration;
}

bool qtractorMidiEditor::isNoteDuration (void) const
{
	return m_bNoteDuration;
}


// Event (note, velocity) coloring.
void qtractorMidiEditor::setNoteColor ( bool bNoteColor )
{
	m_bNoteColor = bNoteColor;
}

bool qtractorMidiEditor::isNoteColor (void) const
{
	return m_bNoteColor;
}


void qtractorMidiEditor::setValueColor ( bool bValueColor )
{
	m_bValueColor = bValueColor;
}

bool qtractorMidiEditor::isValueColor (void) const
{
	return m_bValueColor;
}


// Vertical line position drawing.
void qtractorMidiEditor::drawPositionX ( int& iPositionX, int x, bool bSyncView )
{
	// Update track-view position...
	int x0 = m_pEditView->contentsX();
	int x1 = iPositionX - x0;
	int w  = m_pEditView->width();
	int h1 = m_pEditView->height();
	int h2 = m_pEditEvent->height();
	int wm = (w >> 3);

	// Time-line header extents...
	int h0 = m_pEditTime->height();
	int d0 = (h0 >> 1);

	// Restore old position...
	if (iPositionX != x && x1 >= 0 && x1 < w + d0) {
		// Override old view line...
		(m_pEditEvent->viewport())->update(QRect(x1, 0, 1, h2));
		(m_pEditView->viewport())->update(QRect(x1, 0, 1, h1));
		(m_pEditTime->viewport())->update(QRect(x1 - d0, d0, h0, d0));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && (x < x0 || x > x0 + w - wm) && m_dragState == DragNone) {
		// Move it...
		m_pEditView->setContentsPos(x - wm, m_pEditView->contentsY());
	} else {
		// Draw the line, by updating the new region...
		x1 = x - x0;
		if (x1 >= 0 && x1 < w + d0) {
			(m_pEditEvent->viewport())->update(QRect(x1, 0, 1, h2));
			(m_pEditView->viewport())->update(QRect(x1, 0, 1, h1));
			(m_pEditTime->viewport())->update(QRect(x1 - d0, d0, h0, d0));
		}
	}
}


// Child widgets accessors.
QFrame *qtractorMidiEditor::editListHeader (void) const
{
	return m_pEditListHeader;
}

qtractorMidiEditList *qtractorMidiEditor::editList (void) const
{
	return m_pEditList;
}

qtractorMidiEditTime *qtractorMidiEditor::editTime (void) const
{
	return m_pEditTime;
}

qtractorMidiEditView *qtractorMidiEditor::editView (void) const
{
	return m_pEditView;
}

qtractorMidiEditEvent *qtractorMidiEditor::editEvent (void) const
{
	return m_pEditEvent;
}

qtractorMidiEditEventScale *qtractorMidiEditor::editEventScale (void) const
{
	return m_pEditEventScale;
}

QFrame *qtractorMidiEditor::editEventFrame (void) const
{
	return m_pEditEventFrame;
}


// Horizontal zoom factor.
void qtractorMidiEditor::horizontalZoomStep ( int iZoomStep )
{
	int iHorizontalZoom = m_pTimeScale->horizontalZoom() + iZoomStep;
	if (iHorizontalZoom < ZoomMin)
		iHorizontalZoom = ZoomMin;
	else if (iHorizontalZoom > ZoomMax)
		iHorizontalZoom = ZoomMax;
	if (iHorizontalZoom == m_pTimeScale->horizontalZoom())
		return;

	// Fix the local time scale zoom determinant.
	m_pTimeScale->setHorizontalZoom(iHorizontalZoom);
	m_pTimeScale->updateScale();
}


// Vertical zoom factor.
void qtractorMidiEditor::verticalZoomStep ( int iZoomStep )
{
	int iVerticalZoom = m_pTimeScale->verticalZoom() + iZoomStep;
	if (iVerticalZoom < ZoomMin)
		iVerticalZoom = ZoomMin;
	else if (iVerticalZoom > ZoomMax)
		iVerticalZoom = ZoomMax;
	if (iVerticalZoom == m_pTimeScale->verticalZoom())
		return;

	// Hold and try setting new item height...
	int iItemHeight
		= (iVerticalZoom * qtractorMidiEditList::ItemHeightBase) / 100;
	if (iItemHeight < qtractorMidiEditList::ItemHeightMax && iZoomStep > 0)
		iItemHeight++;
	else
	if (iItemHeight > qtractorMidiEditList::ItemHeightMin && iZoomStep < 0)
		iItemHeight--;
	m_pEditList->setItemHeight(iItemHeight);

	// Fix the local vertical view zoom.
	m_pTimeScale->setVerticalZoom(iVerticalZoom);
}


// Zoom view slots.
void qtractorMidiEditor::zoomIn (void)
{
	horizontalZoomStep(+ ZoomStep);
	verticalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorMidiEditor::zoomOut (void)
{
	horizontalZoomStep(- ZoomStep);
	verticalZoomStep(- ZoomStep);
	centerContents();
}


void qtractorMidiEditor::zoomReset (void)
{
	horizontalZoomStep(ZoomBase - m_pTimeScale->horizontalZoom());
	verticalZoomStep(ZoomBase - m_pTimeScale->verticalZoom());
	centerContents();
}


void qtractorMidiEditor::horizontalZoomInSlot (void)
{
	horizontalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorMidiEditor::horizontalZoomOutSlot (void)
{
	horizontalZoomStep(- ZoomStep);
	centerContents();
}


void qtractorMidiEditor::verticalZoomInSlot (void)
{
	verticalZoomStep(+ ZoomStep);
	centerContents();
}

void qtractorMidiEditor::verticalZoomOutSlot (void)
{
	verticalZoomStep(- ZoomStep);
	centerContents();
}


void qtractorMidiEditor::horizontalZoomResetSlot (void)
{
	horizontalZoomStep(ZoomBase - m_pTimeScale->horizontalZoom());
	centerContents();
}

void qtractorMidiEditor::verticalZoomResetSlot (void)
{
	verticalZoomStep(ZoomBase - m_pTimeScale->verticalZoom());
	centerContents();
}


// Alterrnate command action update helper...
void qtractorMidiEditor::updateUndoAction ( QAction *pAction ) const
{
	m_pCommands->updateAction(pAction, m_pCommands->lastCommand());
}

void qtractorMidiEditor::updateRedoAction ( QAction *pAction ) const
{
	m_pCommands->updateAction(pAction, m_pCommands->nextCommand());
}


// Tell whether we can undo last command...
bool qtractorMidiEditor::canUndo (void) const
{
	return (m_pCommands->lastCommand() != NULL);
}

// Tell whether we can redo last command...
bool qtractorMidiEditor::canRedo (void) const
{
	return (m_pCommands->nextCommand() != NULL);
}


// Undo last edit command.
void qtractorMidiEditor::undoCommand (void)
{
	m_pCommands->undo();
}


// Redo last edit command.
void qtractorMidiEditor::redoCommand (void)
{
	m_pCommands->redo();
}


// Whether there's any item currently selected.
bool qtractorMidiEditor::isSelected (void) const
{
	return (m_select.items().count() > 0);
}


// Whether there's any item on the clipboard.
bool qtractorMidiEditor::isClipboard (void)
{
	// Tell whether there's any item on the clipboard.
	return (g_clipboard.items.count() > 0);
}


// Cut current selection to clipboard.
void qtractorMidiEditor::cutClipboard (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isSelected())
		return;

	g_clipboard.clear();

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("cut"));

	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext()) {
		qtractorMidiEditSelect::Item *pItem = iter.next();
		qtractorMidiEvent *pEvent = pItem->event;
		g_clipboard.items.append(new qtractorMidiEvent(*pEvent));
		pEditCommand->removeEvent(pEvent);
	}

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);
}


// Copy current selection to clipboard.
void qtractorMidiEditor::copyClipboard (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isSelected())
		return;

	g_clipboard.clear();

	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent = iter.next()->event;
		g_clipboard.items.append(new qtractorMidiEvent(*pEvent));
	}

	selectionChangeNotify();
}


// Paste from clipboard.
void qtractorMidiEditor::pasteClipboard (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isClipboard())
		return;

	// Formally ellect this one as the target view...
	qtractorScrollView *pScrollView = m_pEditView;
	// That's right :)
	bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	// Reset any current selection, whatsoever...
	m_select.clear();
	resetDragState(pScrollView);

	// This is the edit-view spacifics...
	int h1 = m_pEditList->itemHeight();
	int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	int y0 = (m_pEditEvent->viewport())->height();
	if (m_pEditEvent->eventType() == qtractorMidiEvent::PITCHBEND)
		y0 = ((y0 >> 3) << 2);

	QListIterator<qtractorMidiEvent *> iter(g_clipboard.items);
	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent = iter.next();
		// Common event coords...
		int y;
		int x  = m_pTimeScale->pixelFromTick(pEvent->time());
		int w1 = m_pTimeScale->pixelFromTick(pEvent->duration()) + 1;
		if (w1 < 5)
			w1 = 5;
		// View item...
		QRect rectView;
		if (pEvent->type() == m_pEditView->eventType()) {
			y = ch - h1 * (pEvent->note() + 1);
			rectView.setRect(x, y, w1, h1);
		}
		// Event item...
		QRect rectEvent;
		if (pEvent->type() == m_pEditEvent->eventType()) {
			if (pEvent->type() == qtractorMidiEvent::PITCHBEND)
				y = y0 - (y0 * pEvent->pitchBend()) / 8192;
			else
				y = y0 - (y0 * pEvent->value()) / 128;
			if (!m_bNoteDuration)
				w1 = 5;
			if (y < y0)
				rectEvent.setRect(x, y, w1, y0 - y);
			else if (y > y0)
				rectEvent.setRect(x, y0, w1, y - y0);
			else
				rectEvent.setRect(x, y0 - 2, w1, 4);
		}
		m_select.addItem(pEvent, rectEvent, rectView);
		if (m_pEventDrag == NULL) {
			m_pEventDrag = pEvent;
			m_rectDrag = (bEditView ? rectView : rectEvent);
		}
	}

	// We'll start a brand new floating state...
	m_dragState = DragPaste;
	m_posDrag   = m_rectDrag.topLeft();
	m_posStep   = QPoint(0, 0);

	// This is the one which is holding focus on drag-paste.
	m_pEditPaste = pScrollView;

	// It doesn't matter which one, both pasteable views are due...
	QCursor cursor(QPixmap(":/icons/editPaste.png"), 20, 20);
	m_pEditView->setCursor(cursor);
	m_pEditEvent->setCursor(cursor);

	// Make sure the mouse pointer is properly located...
	const QPoint& pos = pScrollView->viewportToContents(
		pScrollView->viewport()->mapFromGlobal(QCursor::pos()));

	// Let's-a go...
	updateDragMove(pScrollView, pos + m_posStep);
}


// Execute event removal.
void qtractorMidiEditor::deleteSelect (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isSelected())
		return;

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("delete"));

	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext())
		pEditCommand->removeEvent((iter.next())->event);

	m_pCommands->exec(pEditCommand);
}


// Select all/none contents.
void qtractorMidiEditor::selectAll ( bool bSelect, bool bToggle )
{
	// Select all/none view contents.
	if (bSelect) {
		const QRect rect(0, 0,
			m_pEditView->contentsWidth(),
			m_pEditView->contentsHeight());
		selectRect(rect, bToggle, true);
	} else {
		updateContents();
	}
}


// Select everything between a given view rectangle.
void qtractorMidiEditor::selectRect ( const QRect& rect, bool bToggle,
	bool bCommit )
{
	int flags = SelectNone;
	if (bToggle)
		flags |= SelectToggle;
	if (bCommit)
		flags |= SelectCommit;
	updateDragSelect(m_pEditView, rect.normalized(), flags);
	selectionChangeNotify();
}


// Update/sync integral contents.
void qtractorMidiEditor::updateContents (void)
{
	m_select.clear();

	// Update dependant views.
	m_pEditList->updateContentsHeight();
	m_pEditView->updateContentsWidth();

	// Trigger a complete view update...
	m_pEditList->updateContents();
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
	m_pEditEvent->updateContents();
}


// Try to center vertically the edit-view...
void qtractorMidiEditor::centerContents (void)
{
	m_select.clear();

	// Update dependant views.
	m_pEditList->updateContentsHeight();
	m_pEditView->updateContentsWidth();

	// Do the centering...
	qtractorMidiSequence *pSeq = NULL;
	if (m_pMidiClip)
		pSeq = m_pMidiClip->sequence();
	if (pSeq)	{
		int cy = m_pEditView->contentsHeight();
		int h2 = m_pEditList->itemHeight()
			* (pSeq->noteMin() + pSeq->noteMax());
		if (h2 > 0)
			cy -= ((h2 + (m_pEditView->viewport())->height()) >> 1);
		else
			cy >>= 1;
		if (cy < 0)
			cy = 0;
		m_pEditView->setContentsPos(m_pEditView->contentsX(), cy);
	}

	// Trigger a complete view update...
	m_pEditList->updateContents();
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
	m_pEditEvent->updateContents();
}


// Intra-clip tick/time positioning reset.
qtractorMidiEvent *qtractorMidiEditor::seekEvent ( unsigned long iTime )
{
	// Reset seek-forward...
	return m_cursor.reset(m_pMidiClip->sequence(), iTime);
}


// Get event from given contents position.
qtractorMidiEvent *qtractorMidiEditor::eventAt (
	qtractorScrollView *pScrollView, const QPoint& pos, QRect *pRect )
{
	if (m_pMidiClip == NULL)
		return NULL;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return NULL;

	bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	unsigned long iTime = m_pTimeScale->tickFromPixel(pos.x());

	// This is the edit-view spacifics...
	int h1 = m_pEditList->itemHeight();
	int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	int y0 = (m_pEditEvent->viewport())->height();
	if (m_pEditEvent->eventType() == qtractorMidiEvent::PITCHBEND)
		y0 = ((y0 >> 3) << 2);

	bool bController
		= (m_pEditEvent->eventType() == qtractorMidiEvent::CONTROLLER);
	unsigned char controller = m_pEditEvent->controller();

	qtractorMidiEvent *pEvent = m_cursorAt.reset(pSeq, iTime);
#if 0
	if (pEvent && pEvent->prev()) {
		unsigned long iPrevTime = (pEvent->prev())->time();
		while (pEvent && pEvent->time() >= iPrevTime)
			pEvent = pEvent->prev();
		if (pEvent == NULL)
			pEvent = pSeq->events().first();
	}
#endif
	qtractorMidiEvent *pEventAt = NULL;
	while (pEvent && iTime >= pEvent->time()) {
		if (((bEditView && pEvent->type() == m_pEditView->eventType()) ||
			 (!bEditView && (pEvent->type() == m_pEditEvent->eventType() &&
				(!bController || pEvent->controller() == controller))))) {
			// Common event coords...
			int y;
			int x  = m_pTimeScale->pixelFromTick(pEvent->time());
			int w1 = m_pTimeScale->pixelFromTick(pEvent->duration()) + 1;
			if (w1 < 5)
				w1 = 5;
			QRect rect;
			if (bEditView) {
				// View item...
				y = ch - h1 * (pEvent->note() + 1);
				rect.setRect(x, y, w1, h1);
			} else {
				// Event item...
				if (pEvent->type() == qtractorMidiEvent::PITCHBEND)
					y = y0 - (y0 * pEvent->pitchBend()) / 8192;
				else
					y = y0 - (y0 * pEvent->value()) / 128;
				if (!m_bNoteDuration)
					w1 = 5;
				if (y < y0)
					rect.setRect(x, y, w1, y0 - y);
				else if (y > y0)
					rect.setRect(x, y0, w1, y - y0);
				else
					rect.setRect(x, y0 - 2, w1, 4);
			}
			// Do we have a point?
			if (rect.contains(pos)) {
				if (pRect)
					*pRect = rect;
			//	return pEvent;
				pEventAt = pEvent;
			}
		}
		// Maybe next one...
		pEvent = pEvent->next();
	}

	return pEventAt;
}


// Start immediate some drag-edit mode...
qtractorMidiEvent *qtractorMidiEditor::dragEditEvent (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers /*modifiers*/ )
{
	if (m_pMidiClip == NULL)
		return NULL;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return NULL;

	bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	int ch = m_pEditView->contentsHeight();
	int y0 = (m_pEditEvent->viewport())->height();
	int h1 = m_pEditList->itemHeight();

	int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);
	int x1 = m_pTimeScale->pixelSnap(pos.x() + x0) - x0;
	int y1 = 0;

	// This will be the new editing event...
	qtractorMidiEvent *pEvent = new qtractorMidiEvent(
		m_pTimeScale->tickFromPixel(x1),
		bEditView ? m_pEditView->eventType() : m_pEditEvent->eventType());

	switch (pEvent->type()) {
	case qtractorMidiEvent::NOTEON:
	case qtractorMidiEvent::KEYPRESS:
		// Set note event value...
		if (bEditView) {
			pEvent->setNote((ch - pos.y()) / h1);
			pEvent->setVelocity(m_last.value);
		} else {
			pEvent->setNote(m_last.note);
			if (y0 > 0)
				pEvent->setVelocity((127 * (y0 - pos.y())) / y0);
			else
				pEvent->setVelocity(m_last.value);
		}
		// Default duration...
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			unsigned long iDuration = pSeq->ticksPerBeat();
			if (m_pTimeScale->snapPerBeat() > 0)
				iDuration /= m_pTimeScale->snapPerBeat();
			pEvent->setDuration(iDuration);
			// Mark that we've a note pending...
			if (m_bSendNotes)
				m_pEditList->dragNoteOn(pEvent->note(), pEvent->velocity());
		}
		break;
	case qtractorMidiEvent::PITCHBEND:
		// Set pitchbend event value...
		y0 = ((y0 >> 3) << 2);
		if (y0 > 0)
			pEvent->setPitchBend((8191 * (y0 - pos.y())) / y0);
		else
			pEvent->setPitchBend(m_last.pitchBend);
		break;
	case qtractorMidiEvent::CONTROLLER:
		// Set controller event...
		pEvent->setController(m_pEditEvent->controller());
		// Fall thru...
	default:
		// Set generic event value...
		if (y0 > 0)
			pEvent->setValue((127 * (y0 - pos.y())) / y0);
		else
			pEvent->setValue(m_last.value);
		break;
	}

	// Now try to get the visual rectangular coordinates...
	int w1 = m_pTimeScale->pixelFromTick(pEvent->duration()) + 1;
	if (w1 < 5)
		w1 = 5;

	// View item...
	QRect rectView;
	if (pEvent->type() == m_pEditView->eventType() &&
		(pEvent->type() == qtractorMidiEvent::NOTEON ||
			pEvent->type() == qtractorMidiEvent::KEYPRESS)) {
		y1 = ch - h1 * (pEvent->note() + 1);
		rectView.setRect(x1, y1, w1, h1);
	}

	// Event item...
	QRect rectEvent;	
	if (pEvent->type() == m_pEditEvent->eventType()) {
		if (pEvent->type() == qtractorMidiEvent::PITCHBEND) {
			y1 = y0 - (y0 * pEvent->pitchBend()) / 8192;
			if (y1 > y0) {
				h1 = y1 - y0;
				y1 = y0;
			} else {
				h1 = y0 - y1;
			}
		} else { 
			y1 = y0 - (y0 * pEvent->value()) / 128;
			h1 = y0 - y1;
			m_resizeMode = ResizeValueTop;
		}
		if (!m_bNoteDuration)
			w1 = 5;
		if (h1 < 3)
			h1 = 3;
		rectEvent.setRect(x1, y1, w1, h1);
	}

	// Set the correct target rectangle...
	m_rectDrag = (bEditView ? rectView : rectEvent);
	
	// Just add this one the selection...
	m_select.clear();
	m_select.selectItem(pEvent, rectEvent, rectView, true, false);

	// Set the proper resize-mode...
	if (bEditView && pEvent->type() == qtractorMidiEvent::NOTEON) {
		m_resizeMode = ResizeNoteRight;
	} else if (pEvent->type() == qtractorMidiEvent::PITCHBEND) {
		m_resizeMode = (y1 < y0 ? ResizePitchBendTop : ResizePitchBendBottom);
	} else {
		m_resizeMode = ResizeValueTop;
	}

	// Let it be a drag resize mode...
	return pEvent;
}


// Track drag-move-select cursor and mode...
qtractorMidiEvent *qtractorMidiEditor::dragMoveEvent (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers /*modifiers*/ )
{
	qtractorMidiEvent *pEvent = eventAt(pScrollView, pos, &m_rectDrag);

	// Make the anchor event, if any, visible yet...
	m_resizeMode = ResizeNone;
	if (pEvent) {
		bool bEditView
			= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);
		Qt::CursorShape shape = Qt::PointingHandCursor;
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			if (!bEditView && pos.y() < m_rectDrag.top() + 4) {
				m_resizeMode = ResizeValueTop;
				shape = Qt::SplitVCursor;
			} else if (bEditView || m_bNoteDuration) {
				if (pos.x() > m_rectDrag.right() - 4) {
					m_resizeMode = ResizeNoteRight;
					shape = Qt::SplitHCursor;
				} else if (pos.x() < m_rectDrag.left() + 4) {
					m_resizeMode = ResizeNoteLeft;
					shape = Qt::SplitHCursor;
				}
			}
		} else if (!bEditView) {
			if (pEvent->type() == qtractorMidiEvent::PITCHBEND) {
				int y0 = (((m_pEditEvent->viewport())->height() >> 3) << 2);
				if (pos.y() > y0 && pos.y() > m_rectDrag.bottom() - 4) {
					m_resizeMode = ResizePitchBendBottom;
					shape = Qt::SplitVCursor;
				} else if (pos.y() < y0 && pos.y() < m_rectDrag.top() + 4) {
					m_resizeMode = ResizePitchBendTop;
					shape = Qt::SplitVCursor;
				}
			} else if (pos.y() < m_rectDrag.top() + 4) {
				m_resizeMode = ResizeValueTop;
				shape = Qt::SplitVCursor;
			}
		}
		pScrollView->setCursor(QCursor(shape));
	} else if (m_dragState == DragNone) {
		pScrollView->unsetCursor();
	}

	return pEvent;
}


// Start drag-move-selecting...
void qtractorMidiEditor::dragMoveStart ( qtractorScrollView *pScrollView,
	const QPoint& pos, Qt::KeyboardModifiers modifiers )
{
	// Are we already step-moving or pasting something?
	switch (m_dragState) {
	case DragStep:
		// One-click change from drag-step to drag-move...
		m_dragState = DragMove;
		m_posDrag   = m_rectDrag.center();
		m_posStep   = QPoint(0, 0);
		updateDragMove(pScrollView, pos + m_posStep);
		// Fall thru...
	case DragPaste:
		return;
	default:
		break;
	}

	// Force null state.
	resetDragState(pScrollView);

	// Remember what and where we'll be dragging/selecting...
	m_dragState  = DragStart;
	m_posDrag    = pos;
	m_pEventDrag = dragMoveEvent(pScrollView, m_posDrag, modifiers);

	// Check whether we're about to create something...
	if (m_pEventDrag == NULL && m_bEditMode
		&& (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
		m_pEventDrag = dragEditEvent(pScrollView, m_posDrag, modifiers);
		m_bEventDragEdit = true;
		pScrollView->setCursor(QCursor(QPixmap(":/icons/editModeOn.png"), 5, 18));
	} else if (m_resizeMode == ResizeNone) {
		if (m_pEventDrag) {
			pScrollView->setCursor(QCursor(
				static_cast<qtractorScrollView *> (m_pEditView)	== pScrollView
				? Qt::SizeAllCursor : Qt::SizeHorCursor));
		} else {
			pScrollView->setCursor(QCursor(Qt::CrossCursor));
		}
	}

	// Maybe we'll have a note pending...
	if (m_bSendNotes && m_pEventDrag
		&& m_pEventDrag->type() == qtractorMidiEvent::NOTEON)
		m_pEditList->dragNoteOn(m_pEventDrag->note(), m_pEventDrag->velocity());
}


// Update drag-move-selection...
void qtractorMidiEditor::dragMoveUpdate ( qtractorScrollView *pScrollView,
	const QPoint& pos, Qt::KeyboardModifiers modifiers )
{
	int flags = SelectNone;
	
	switch (m_dragState) {
	case DragStart:
		// Did we moved enough around?
		if ((pos - m_posDrag).manhattanLength()
			< QApplication::startDragDistance())
			break;
	#if 0
		// Take care of selection modifier...
		if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0)
			flags |= SelectClear;
	#endif
		// Are we about to move something around?
		if (m_pEventDrag) {
			if (m_resizeMode == ResizeNone) {
				// Start moving... take care of yet initial selection...
				qtractorMidiEditSelect::Item *pItem
					= m_select.findItem(m_pEventDrag);
				if	(pItem == NULL || (pItem->flags & 1) == 0) {
					updateDragSelect(pScrollView,
						QRect(m_posDrag, QSize()), flags | SelectCommit);
				}
				// Start drag-moving...
				m_dragState = DragMove;
				updateDragMove(pScrollView, pos + m_posStep);
			} else {
				// Start resizing... take care of yet initial selection...
				if (!m_bEventDragEdit) {
					updateDragSelect(pScrollView,
						QRect(m_posDrag, QSize()), flags | SelectCommit);
				}
				// Start drag-resizing...
				m_dragState = DragResize;
				updateDragResize(pScrollView, pos);
			}
			break;
		}
		// Just about to start rubber-banding...
		m_dragState = DragSelect;
		// Fall thru...
	case DragSelect:
		// Set new rubber-band extents...
		pScrollView->ensureVisible(pos.x(), pos.y(), 16, 16);
		if (modifiers & Qt::ControlModifier)
			flags |= SelectToggle;
		updateDragSelect(pScrollView, QRect(m_posDrag, pos).normalized(), flags);
		break;
	case DragMove:
	case DragPaste:
		// Drag-moving...
		updateDragMove(pScrollView, pos + m_posStep);
		break;
	case DragResize:
		// Drag-resizeing...
		updateDragResize(pScrollView, pos);
		break;
	case DragStep:
	case DragNone:
	default:
		// Just make cursor tell something...
		dragMoveEvent(pScrollView, pos, modifiers);
		break;
	}
}


// Commit drag-move-selection...
void qtractorMidiEditor::dragMoveCommit ( qtractorScrollView *pScrollView,
	const QPoint& pos, Qt::KeyboardModifiers modifiers )
{
	int flags = qtractorMidiEditor::SelectCommit;

	switch (m_dragState) {
	case DragStart:
		// Were we about to edit-resize something?
		if (m_bEventDragEdit) {
			m_dragState = DragResize;
			executeDragResize(pScrollView, pos);
			break;
		}
		// Take care of selection modifier...
		if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0)
			flags |= SelectClear;
		else
		// Shall we move the playhead?...
		if (m_pEventDrag == NULL) {
			// Direct snap positioning...
			unsigned long iFrame = m_pTimeScale->frameSnap(m_iOffset
				+ m_pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
			// Playhead positioning...
			setPlayHead(iFrame);
			// Immediately commited...
			qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
			if (pMainForm) {
				qtractorSession *pSession = pMainForm->session();
				if (pSession)
					pSession->setPlayHead(iFrame);
			}
		}
		// Fall thru...
	case DragSelect:
		// Terminate selection...
		pScrollView->ensureVisible(pos.x(), pos.y(), 16, 16);
		if (modifiers & Qt::ControlModifier)
			flags |= SelectToggle;
		updateDragSelect(pScrollView, QRect(m_posDrag, pos).normalized(), flags);
		selectionChangeNotify();
		break;
	case DragMove:
		// Move it...
		executeDragMove(pScrollView, pos);
		break;
	case DragPaste:
		// Paste it...
		executeDragPaste(pScrollView, pos);
		break;
	case DragResize:
		// Resize it...
		executeDragResize(pScrollView, pos);
		break;
	case DragStep:
	case DragNone:
	default:
		break;
	}

	// Force null state.
	resetDragState(pScrollView);
}


// Trap for help/tool-tip and leave events.
bool qtractorMidiEditor::dragMoveFilter ( qtractorScrollView *pScrollView,
	QObject *pObject, QEvent *pEvent )
{
	if (static_cast<QWidget *> (pObject) == pScrollView->viewport()) {
		if (pEvent->type() == QEvent::ToolTip) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				const QPoint& pos
					= pScrollView->viewportToContents(pHelpEvent->pos());
				qtractorMidiEvent *pMidiEvent = eventAt(pScrollView, pos);
				if (pMidiEvent) {
					QToolTip::showText(pHelpEvent->globalPos(),
						eventToolTip(pMidiEvent));
					return true;
				}
			}
		}
		else
		if (pEvent->type() == QEvent::Leave	&&
			m_dragState != DragPaste &&
			m_dragState != DragStep) {
			pScrollView->unsetCursor();
			return true;
		}
	}

	// Not handled here.
	return false;
}


// Update the event selection list.
void qtractorMidiEditor::updateDragSelect ( qtractorScrollView *pScrollView,
	const QRect& rectSelect, int flags )
{
	if (m_pMidiClip == NULL)
		return;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return;

	// Rubber-banding only applicable whenever
	// the selection rectangle is not empty...
	bool bRectSelect = !rectSelect.isEmpty();
	if (bRectSelect) {
		// Create rubber-band, if not already...
		if (m_pRubberBand == NULL) {
			m_pRubberBand = new qtractorRubberBand(
				QRubberBand::Rectangle, pScrollView->viewport());
			m_pRubberBand->show();
		}
		// Rubber-band selection...
		m_pRubberBand->setGeometry(QRect(
			pScrollView->contentsToViewport(rectSelect.topLeft()),
			rectSelect.size()));
	}

	// Do the drag-select update properly...

	bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	QRect rectUpdateView(m_select.rectView());
	QRect rectUpdateEvent(m_select.rectEvent());

	if (flags & SelectClear)
		m_select.clear();

	int x1, x2;
	if (bRectSelect) {
		x1 = pScrollView->contentsX();
		x2 = x1 + (pScrollView->viewport())->width();
		if (x1 > rectSelect.left())
			x1 = rectSelect.left();
		if (x2 < rectSelect.right())
			x2 = rectSelect.right();
	} else {
		x1 = x2 = rectSelect.x();
	}

	unsigned long iTickStart = m_pTimeScale->tickFromPixel(x1);
	unsigned long iTickEnd   = m_pTimeScale->tickFromPixel(x2);

	// This is the edit-view spacifics...
	int h1 = m_pEditList->itemHeight();
	int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	int y0 = (m_pEditEvent->viewport())->height();
	if (m_pEditEvent->eventType() == qtractorMidiEvent::PITCHBEND)
		y0 = ((y0 >> 3) << 2);

	bool bController
		= (m_pEditEvent->eventType() == qtractorMidiEvent::CONTROLLER);
	unsigned char controller = m_pEditEvent->controller();

	qtractorMidiEvent *pEvent = m_cursorAt.seek(pSeq, iTickStart);

	qtractorMidiEvent *pEventAt = NULL;
	QRect rectViewAt;
	QRect rectEventAt;

	while (pEvent && iTickEnd >= pEvent->time()) {
		if (((bEditView && pEvent->type() == m_pEditView->eventType() &&
				pEvent->time() + pEvent->duration() >= iTickStart) ||
			 (!bEditView && (pEvent->type() == m_pEditEvent->eventType() &&
				(!bController || pEvent->controller() == controller))))) {
			// Assume unselected...
			bool bSelect = false;
			// Common event coords...
			int y;
			int x  = m_pTimeScale->pixelFromTick(pEvent->time());
			int w1 = m_pTimeScale->pixelFromTick(pEvent->duration()) + 1;
			if (w1 < 5)
				w1 = 5;
			// View item...
			QRect rectView;
			if (pEvent->type() == m_pEditView->eventType()) {
				y = ch - h1 * (pEvent->note() + 1);
				rectView.setRect(x, y, w1, h1);
				if (bEditView)
					bSelect = rectSelect.intersects(rectView);
			}
			// Event item...
			QRect rectEvent;
			if (pEvent->type() == m_pEditEvent->eventType()) {
				if (pEvent->type() == qtractorMidiEvent::PITCHBEND)
					y = y0 - (y0 * pEvent->pitchBend()) / 8192;
				else
					y = y0 - (y0 * pEvent->value()) / 128;
				if (!m_bNoteDuration)
					w1 = 5;
				if (y < y0)
					rectEvent.setRect(x, y, w1, y0 - y);
				else if (y > y0)
					rectEvent.setRect(x, y0, w1, y - y0);
				else
					rectEvent.setRect(x, y0 - 2, w1, 4);
				if (!bEditView)
					bSelect = rectSelect.intersects(rectEvent);
			}
			// Select item...
			if (bRectSelect) {
				m_select.selectItem(pEvent, rectEvent, rectView,
					bSelect, flags & SelectToggle);
			} else if (bSelect) {
				pEventAt    = pEvent;
				rectViewAt  = rectView;
				rectEventAt = rectEvent;
			}
		}
		// Lookup next...
		pEvent = pEvent->next();
	}

	// Most evident single selection...
	if (pEventAt /* && !bRectSelect*/) {
		m_select.selectItem(pEventAt, rectEventAt, rectViewAt,
			true, flags & SelectToggle);
	}

	// Commit selection...
	bool bCommit = (flags & SelectCommit);
	m_select.update(bCommit);

	rectUpdateView = rectUpdateView.unite(m_select.rectView());
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.unite(m_select.rectEvent());
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

	if (bEditView) {
		setEditHead(m_pTimeScale->frameSnap(m_iOffset
			+ m_pTimeScale->frameFromPixel(rectSelect.left())), bCommit);
		setEditTail(m_pTimeScale->frameSnap(m_iOffset
			+ m_pTimeScale->frameFromPixel(rectSelect.right())), bCommit);
	}
}


// Drag-move current selection.
void qtractorMidiEditor::updateDragMove ( qtractorScrollView *pScrollView,
	const QPoint& pos )
{
	pScrollView->ensureVisible(pos.x(), pos.y(), 16, 16);

	bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	QRect rectUpdateView(m_select.rectView().translated(m_posDelta));
	QRect rectUpdateEvent(m_select.rectEvent().translated(m_posDelta.x(), 0));

	QPoint delta(pos - m_posDrag);
	QRect rect(bEditView ? m_select.rectView() : m_select.rectEvent());

	int cw = pScrollView->contentsWidth();
	int dx = delta.x();
	int x0 = m_rectDrag.x() + m_pTimeScale->pixelFromFrame(m_iOffset);
	int x1 = rect.x() + dx;
	if (x1 < 0)
		dx = -(rect.x());
	if (x1 + rect.width() > cw)
		dx = cw - rect.right();
	m_posDelta.setX(m_pTimeScale->pixelSnap(x0 + dx) - x0);

	int h1 = m_pEditList->itemHeight();
	if (bEditView && h1 > 0) {
		int ch = m_pEditView->contentsHeight();
		int y0 = rect.y();
		int y1 = y0 + delta.y();
		if (y1 < 0)
			y1 = 0;
		if (y1 + rect.height() > ch)
			y1 = ch - rect.height();
		m_posDelta.setY(h1 * (y1 / h1) - y0); 
	} else {
		m_posDelta.setY(0);
	}

	rectUpdateView = rectUpdateView.unite(
		m_select.rectView().translated(m_posDelta));
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.unite(
		m_select.rectEvent().translated(m_posDelta.x(), 0));
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

	// Maybe we've change some note pending...
	if (m_bSendNotes && m_pEventDrag
		&& m_pEventDrag->type() == qtractorMidiEvent::NOTEON) {
		int iNote = int(m_pEventDrag->note());
		if (h1 > 0)
			iNote -= (m_posDelta.y() / h1);
		m_pEditList->dragNoteOn(iNote, m_pEventDrag->velocity());
	}
}


// Drag-resize current selection (also editing).
void qtractorMidiEditor::updateDragResize ( qtractorScrollView *pScrollView,
	const QPoint& pos )
{
	pScrollView->ensureVisible(pos.x(), pos.y(), 16, 16);

	QRect rectUpdateView(m_select.rectView().translated(m_posDelta.x(), 0));
	QRect rectUpdateEvent(m_select.rectEvent().translated(m_posDelta));

	QPoint delta(pos - m_posDrag);
	int x0, x1;
	int y0, y1;
	int dx = 0;
	int dy = 0;

	// TODO: Plenty of...
	switch (m_resizeMode) {
	case ResizeNoteLeft:
		dx = delta.x();
		x0 = m_rectDrag.left() + m_pTimeScale->pixelFromFrame(m_iOffset);
		x1 = m_rectDrag.left() + dx;
		if (x1 < 0)
			dx = -(m_rectDrag.left());
		if (x1 > m_rectDrag.right())
			dx = m_rectDrag.width();
		dx = m_pTimeScale->pixelSnap(x0 + dx) - x0;
		break;
	case ResizeNoteRight:
		dx = delta.x();
		x0 = m_rectDrag.right() + m_pTimeScale->pixelFromFrame(m_iOffset);
		x1 = m_rectDrag.right() + dx;
		if (x1 < m_rectDrag.left())
			dx = -(m_rectDrag.width());
		dx = m_pTimeScale->pixelSnap(x0 + dx) - x0;
		break;
	case ResizeValueTop:
	case ResizePitchBendTop:
	case ResizePitchBendBottom:
		y0 = m_rectDrag.bottom();
		y1 = y0 + delta.y();
		if (y1 < 0)
			y1 = 0;
		dy = y1 - y0;
		break;
	default:
		break;
	}

	m_posDelta.setX(dx);
	m_posDelta.setY(dy);
	
	rectUpdateView = rectUpdateView.unite(
		m_select.rectView().translated(m_posDelta.x(), 0));
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.unite(
		m_select.rectEvent().translated(m_posDelta));
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));
}


// Finalize the event drag-move.
void qtractorMidiEditor::executeDragMove ( qtractorScrollView *pScrollView,
	const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragMove(pScrollView, pos + m_posStep);

	long iTimeDelta = 0;
	if (m_posDelta.x() < 0)
		iTimeDelta = -long(m_pTimeScale->tickFromPixel(-m_posDelta.x()));
	else
		iTimeDelta = +long(m_pTimeScale->tickFromPixel(+m_posDelta.x()));

	int h1 = m_pEditList->itemHeight();
	int iNoteDelta = 0;
	if (h1 > 0)
		iNoteDelta = -(m_posDelta.y() / h1);

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("move"));

	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent = (iter.next())->event;
		int iNote = int(pEvent->note()) + iNoteDelta;
		if (iNote < 0)
			iNote = 0;
		if (iNote > 127)
			iNote = 127;
		long iTime = long(pEvent->time()) + iTimeDelta;
		if (iTime < 0)
			iTime = 0;
		pEditCommand->moveEvent(pEvent, iNote, iTime);
	}

	// Make it as an undoable command...
	if (m_pCommands->exec(pEditCommand))
		adjustEditCommand(pEditCommand);
}


// Finalize the event drag-resize (also editing).
void qtractorMidiEditor::executeDragResize ( qtractorScrollView *pScrollView,
	const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragResize(pScrollView, pos);

	long iTimeDelta = 0;
	if (m_posDelta.x() < 0)
		iTimeDelta = -long(m_pTimeScale->tickFromPixel(-m_posDelta.x()));
	else
		iTimeDelta = +long(m_pTimeScale->tickFromPixel(+m_posDelta.x()));

	int h = (m_pEditEvent->viewport())->height();
	int iValueDelta = 0;
	if (h > 0) {
		if (m_resizeMode == ResizePitchBendTop ||
			m_resizeMode == ResizePitchBendBottom)
			iValueDelta = -(m_posDelta.y() * 8192 * 2) / h;
		else
			iValueDelta = -(m_posDelta.y() * 128) / h;
	}

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip,
			m_bEventDragEdit ? tr("edit") : tr("resize"));

	long iTime, iDuration;
	int iValue;
	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent = (iter.next())->event;
		switch (m_resizeMode) {
		case ResizeNoteLeft:
			iTime = long(pEvent->time()) + iTimeDelta;
			iDuration = long(pEvent->duration()) - iTimeDelta;
			if (iTime < 0)
				iTime = 0;
			if (iDuration < 0)
				iDuration = 0;
			if (m_bEventDragEdit) {
				pEvent->setTime(iTime);
				pEvent->setDuration(iDuration);
				pEditCommand->insertEvent(pEvent);
			} else {
				pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
			}
			m_last.note = pEvent->note();
		//	m_last.duration = iDuration;
			break;
		case ResizeNoteRight:
			iTime = pEvent->time();
			iDuration = long(pEvent->duration()) + iTimeDelta;
			if (iDuration < 0)
				iDuration = 0;
			if (m_bEventDragEdit) {
				pEvent->setDuration(iDuration);
				pEditCommand->insertEvent(pEvent);
			} else {
				pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
			}
			m_last.note = pEvent->note();
		//	m_last.duration = iDuration;
			break;
		case ResizeValueTop:
			iValue = int(pEvent->value()) + iValueDelta;
			if (iValue < 0)
				iValue = 0;
			else
			if (iValue > 127)
				iValue = 127;
			if (m_bEventDragEdit) {
				pEvent->setValue(iValue);
				pEditCommand->insertEvent(pEvent);
			} else {
				pEditCommand->resizeEventValue(pEvent, iValue);
			}
			m_last.value = iValue;
			break;
		case ResizePitchBendTop:
		case ResizePitchBendBottom:
			iValue = pEvent->pitchBend() + iValueDelta;
			if (iValue < -8191)
				iValue = -8191;
			else
			if (iValue > +8191)
				iValue = +8191;
			if (m_bEventDragEdit) {
				pEvent->setPitchBend(iValue);
				pEditCommand->insertEvent(pEvent);
			} else {
				pEditCommand->resizeEventValue(pEvent, iValue);
			}
			m_last.pitchBend = iValue;
			break;
		default:
			break;
		}
	}

	// On edit mod we now own the new event...
	if (m_bEventDragEdit)
		m_pEventDrag = NULL;

	// Make it as an undoable command...
	if (m_pCommands->exec(pEditCommand))
		adjustEditCommand(pEditCommand);
}


// Finalize the event drag-paste.
void qtractorMidiEditor::executeDragPaste ( qtractorScrollView *pScrollView,
	const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragMove(pScrollView, pos + m_posStep);

	long iTimeDelta = 0;
	if (m_posDelta.x() < 0)
		iTimeDelta = -long(m_pTimeScale->tickFromPixel(-m_posDelta.x()));
	else
		iTimeDelta = +long(m_pTimeScale->tickFromPixel(+m_posDelta.x()));

	int h1 = m_pEditList->itemHeight();
	int iNoteDelta = 0;
	if (h1 > 0)
		iNoteDelta = -(m_posDelta.y() / h1);

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("paste"));

	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent
			= new qtractorMidiEvent(*(iter.next())->event);
		int iNote = int(pEvent->note()) + iNoteDelta;
		if (iNote < 0)
			iNote = 0;
		if (iNote > 127)
			iNote = 127;
		pEvent->setNote(iNote);
		long iTime = long(pEvent->time()) + iTimeDelta;
		if (iTime < 0)
			iTime = 0;
		pEvent->setTime(iTime);
		pEditCommand->insertEvent(pEvent);
	}

	// Make it as an undoable command...
	if (m_pCommands->exec(pEditCommand))
		adjustEditCommand(pEditCommand);
}


// Visualize the event selection drag-move.
void qtractorMidiEditor::paintDragState ( qtractorScrollView *pScrollView,
	QPainter *pPainter )
{
#ifdef CONFIG_DEBUG_0
	const QRect& rectSelect = (bEditView
		? m_select.rectView() : m_select.rectEvent());
	if (!rectSelect.isEmpty()) {
		pPainter->fillRect(QRect(
			pScrollView->contentsToViewport(rectSelect.topLeft()),
			rectSelect.size()), QColor(0, 0, 255, 40));
	}
#endif

	bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	int x1, y1;
	QListIterator<qtractorMidiEditSelect::Item *> iter(m_select.items());
	while (iter.hasNext()) {
		qtractorMidiEditSelect::Item *pItem = iter.next();
		if ((pItem->flags & 1) == 0)
			continue;
		int c = (pItem->event == m_pEventDrag ? 64 : 0);
		QRect rect = (bEditView ? pItem->rectView : pItem->rectEvent);
		if (m_dragState == DragResize) {
			switch (m_resizeMode) {
			case ResizeNoteLeft:
				x1 = rect.left() + m_posDelta.x();
				if (x1 < 0)
					x1 = 0;
				if (x1 > rect.right())
					x1 = rect.right();
				rect.setLeft(x1);
				break;
			case ResizeNoteRight:
				x1 = rect.right() + m_posDelta.x();
				if (x1 < rect.left())
					x1 = rect.left();
				rect.setRight(x1);
				break;
			case ResizeValueTop:
				if (!bEditView) {
					y1 = rect.top() + m_posDelta.y();
					if (y1 < 0)
						y1 = 0;
					if (y1 > rect.bottom())
						y1 = rect.bottom();
					rect.setTop(y1);
				}
				break;
			case ResizePitchBendTop:
				if (!bEditView) {
					y1 = rect.top() + m_posDelta.y();
					if (y1 < 0)
						y1 = 0;
					if (y1 > rect.bottom()) {
						rect.setTop(rect.bottom());
						rect.setBottom(y1);
					} else {
						rect.setTop(y1);
					}
				}
				break;
			case ResizePitchBendBottom:
				if (!bEditView) {
					y1 = rect.bottom() + m_posDelta.y();
					if (y1 < 0)
						y1 = 0;
					if (y1 > rect.top()) {
						rect.setBottom(rect.top());
						rect.setTop(y1);
					} else {
						rect.setBottom(y1);
					}
				}
				break;
			default:
				break;
			}
		}	// Draw for selection/move...
		else if (bEditView)
			rect.translate(m_posDelta);
		else
			rect.translate(m_posDelta.x(), 0);
		// Paint the damn bastard...
		pPainter->fillRect(QRect(
			pScrollView->contentsToViewport(rect.topLeft()),
			rect.size()), QColor(c, 0, 255 - c, 120));
	}
}


// Reset drag/select/move state.
void qtractorMidiEditor::resetDragState ( qtractorScrollView *pScrollView )
{
	if (m_bEventDragEdit && m_pEventDrag)
		delete m_pEventDrag;

	m_pEventDrag = NULL;
	m_bEventDragEdit = false;

	m_posDelta = QPoint(0, 0);
	m_posStep  = QPoint(0, 0);

	m_pEditPaste = NULL;

	if (m_pRubberBand) {
		m_pRubberBand->hide();
		delete m_pRubberBand;
		m_pRubberBand = NULL;
	}

	if (pScrollView) {
		if (m_dragState != DragNone)
			pScrollView->unsetCursor();
		if (m_dragState == DragMove   ||
			m_dragState == DragResize ||
			m_dragState == DragPaste  ||
			m_dragState == DragStep)
			updateContents();
	}

	if (m_pEditList)
		m_pEditList->dragNoteOn(-1);

	m_dragState  = DragNone;
	m_resizeMode = ResizeNone;
}


// Adjust edit-command result to prevent event overlapping.
bool qtractorMidiEditor::adjustEditCommand (
	qtractorMidiEditCommand *pEditCommand )
{
	if (m_pMidiClip == NULL)
		return false;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return false;

	// HACK: What we're going to do here is about checking the
	// whole sequence, fixing any overlapping note events and
	// adjusting the issued command for proper undo/redo...
	qtractorMidiSequence::NoteMap notes;

	// For each event, do rescan...
	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		unsigned long iTime = pEvent->time();
		unsigned long iTimeEnd = iTime + pEvent->duration();
		// NOTEON: Find previous note event and check overlaps...
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			// Already there?
			unsigned char note = pEvent->note();
			qtractorMidiSequence::NoteMap::Iterator iter = notes.find(note);
			if (iter != notes.end()) {
				qtractorMidiEvent *pPrevEvent = *iter;
				unsigned long iPrevTime = pPrevEvent->time();
				unsigned long iPrevTimeEnd = iPrevTime + pPrevEvent->duration();
				// Left-side event...
				if (iTime > iPrevTime && iTime < iPrevTimeEnd) {
					unsigned long iDuration = pPrevEvent->duration();
					pPrevEvent->setDuration(iTime - iPrevTime);
					if (!pEditCommand->findEvent(pPrevEvent,
							qtractorMidiEditCommand::ResizeEventTime)) {
						pEditCommand->resizeEventTime(
							pPrevEvent, iPrevTime, iDuration);
					}
					// Right-side event...
					if (iTimeEnd < iPrevTimeEnd) {
						qtractorMidiEvent *pNewEvent
							= new qtractorMidiEvent(*pPrevEvent);
						pNewEvent->setTime(iTimeEnd);
						pNewEvent->setDuration(iPrevTimeEnd - iTimeEnd);
						pEditCommand->insertEvent(pNewEvent);
						pSeq->insertEvent(pNewEvent);
					}
				}
				else
				// Loose overlap?...
				if (iTime == iPrevTime || iTimeEnd == iPrevTimeEnd) {
					pSeq->unlinkEvent(pPrevEvent);
					if (!pEditCommand->findEvent(pPrevEvent,
							qtractorMidiEditCommand::RemoveEvent))
						pEditCommand->removeEvent(pPrevEvent);
				}
			}
			// Set as last note...
			notes[note] = pEvent;
		}
		// Iterate next...
		pEvent = pEvent->next();
	}

	return true;
}



// Edit tools form page selector.
void qtractorMidiEditor::executeTool ( int iToolIndex )
{
	if (m_pMidiClip == NULL)
		return;

	qtractorMidiToolsForm toolsForm(this);
	toolsForm.setToolIndex(iToolIndex);
	if (toolsForm.exec()) {
		qtractorMidiEditCommand *pEditCommand
			= toolsForm.editCommand(m_pMidiClip, &m_select,
				m_pTimeScale->tickFromFrame(m_iOffset));
		if (m_pCommands->exec(pEditCommand))
			adjustEditCommand(pEditCommand);
	}		
}


// Command list accessor.
qtractorCommandList *qtractorMidiEditor::commands (void) const
{
	return m_pCommands;
}



// Update instrument defined names for current clip/track.
void qtractorMidiEditor::updateInstrumentNames (void)
{
	m_noteNames.clear();
	m_controllerNames.clear();

	if (m_pMidiClip == NULL)
		return;

	qtractorTrack *pTrack = m_pMidiClip->track();
	if (pTrack == NULL)
		return;

	qtractorMainForm *pMainForm	= qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorInstrumentList *pInstruments = pMainForm->instruments();
	if (pInstruments == NULL)
		return;

	// Get instrument name from patch descriptor...
	QString sInstrument;
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus)
		sInstrument = pMidiBus->patch(pTrack->midiChannel()).instrumentName;
	// Do we have any?...
	if (sInstrument.isEmpty() || !pInstruments->contains(sInstrument)) {
		// At least have a GM Drums (Channel 10) help...
		if (pTrack->midiChannel() == 9) {
			for (int i = 13; g_aNoteNames[i].name; ++i) {
				m_noteNames.insert(
					g_aNoteNames[i].note,
					tr(g_aNoteNames[i].name));
			}
		}
		// No instrument definition...
		return;
	}

	// Finally, got instrument descriptor...
	qtractorInstrumentData::ConstIterator iter;
	const qtractorInstrument& instr = (*pInstruments)[sInstrument];

	// Key note names...
	const qtractorInstrumentData& notes
		= instr.notes(pTrack->midiBank(), pTrack->midiProgram());
	for (iter = notes.constBegin(); iter != notes.constEnd(); ++iter)
		m_noteNames.insert(iter.key(), iter.value());

	// Controller names...
	const qtractorInstrumentData& controllers = instr.control();
	for (iter = controllers.constBegin(); iter != controllers.constEnd(); ++iter)
		m_controllerNames.insert(iter.key(), iter.value());
}


// Note name map accessor.
const QString qtractorMidiEditor::noteName ( unsigned char note ) const
{
	QHash<unsigned char, QString>::ConstIterator iter
		= m_noteNames.constFind(note);
	if (iter == m_noteNames.constEnd())
		return defaultNoteName(note);
	else
		return iter.value();
}


// Controller name map accessor.
const QString& qtractorMidiEditor::controllerName ( unsigned char controller ) const
{
	QHash<unsigned char, QString>::ConstIterator iter
		= m_controllerNames.constFind(controller);
	if (iter == m_controllerNames.constEnd())
		return defaultControllerName(controller);
	else
		return iter.value();
}


// Command execution notification slot.
void qtractorMidiEditor::updateNotifySlot ( bool bRefresh )
{
	if (bRefresh)
		updateContents();

	contentsChangeNotify();
}


// Emit selection/changes.
void qtractorMidiEditor::selectionChangeNotify (void)
{
	emit selectNotifySignal(this);
}

void qtractorMidiEditor::contentsChangeNotify (void)
{
	emit changeNotifySignal(this);
}


// Emit note on/off.
void qtractorMidiEditor::sendNote ( int iNote, int iVelocity )
{
	if (iVelocity == 1)
		iVelocity = m_last.value;

	emit sendNoteSignal(iNote, iVelocity);
}


// MIDI event tool tip helper.
QString qtractorMidiEditor::eventToolTip ( qtractorMidiEvent *pEvent ) const
{
	QString sToolTip = tr("Time:\t%1\nType:\t")
		.arg(m_pTimeScale->textFromTick(pEvent->time()
			+ m_pTimeScale->tickFromFrame(m_iOffset)));

	switch (pEvent->type()) {
//	case qtractorMidiEvent::NOTEOFF:
//		sToolTip += tr("Note Off (%1)").arg(int(pEvent->note()));
//		break;
	case qtractorMidiEvent::NOTEON:
		sToolTip += tr("Note On (%1) %2\nVelocity:\t%3\nDuration:\t%4")
			.arg(int(pEvent->note()))
			.arg(noteName(pEvent->note()))
			.arg(int(pEvent->velocity()))
			.arg(m_pTimeScale->textFromTick(pEvent->duration()));
		break;
	case qtractorMidiEvent::KEYPRESS:
		sToolTip += tr("Key Press (%1) %2\nValue:\t%3")
			.arg(int(pEvent->note()))
			.arg(noteName(pEvent->note()))
			.arg(int(pEvent->value()));
		break;
	case qtractorMidiEvent::CONTROLLER:
		sToolTip += tr("Controller (%1)\nName:\t%2\nValue:\t%3")
			.arg(int(pEvent->controller()))
			.arg(controllerName(int(pEvent->controller())))
			.arg(int(pEvent->value()));
		break;
	case qtractorMidiEvent::PGMCHANGE:
		sToolTip += tr("Pgm Change (%1)").arg(int(pEvent->value()));
		break;
	case qtractorMidiEvent::CHANPRESS:
		sToolTip += tr("Chan Press (%1)").arg(int(pEvent->value()));
		break;
	case qtractorMidiEvent::PITCHBEND:
		sToolTip = tr("Pich Bend (%d)").arg(int(pEvent->pitchBend()));
		break;
	case qtractorMidiEvent::SYSEX:
	{
		unsigned char *data = pEvent->sysex();
		unsigned short len  = pEvent->sysex_len();
		sToolTip += tr("SysEx (%1 bytes)\nData: ").arg(int(len));
		sToolTip += '{';
		sToolTip += ' ';
		for (unsigned short i = 0; i < len; ++i)
			sToolTip += QString().sprintf("%02x ", data[i]);
		sToolTip += '}';
		break;
	}
//	case qtractorMidiEvent::META:
//		sToolTip += tr("Meta");
//		break;
	default:
		sToolTip += tr("Unknown (%1)").arg(int(pEvent->type()));
		break;
	}
	
	// That's it
	return sToolTip;
}


// Keyboard event handler (common).
bool qtractorMidiEditor::keyPress ( qtractorScrollView *pScrollView,
	int iKey, Qt::KeyboardModifiers modifiers )
{
	switch (iKey) {
	case Qt::Key_Insert: // Aha, joking :)
	case Qt::Key_Return:
		if (m_dragState == DragStep) {
			executeDragMove(pScrollView, m_posDrag);
		} else {
			const QPoint& pos = pScrollView->viewportToContents(
				pScrollView->viewport()->mapFromGlobal(QCursor::pos()));
			if (m_dragState == DragMove)
				executeDragMove(pScrollView, pos);
			else if (m_dragState == DragPaste)
				executeDragPaste(pScrollView, pos);
		}
		// Fall thru...
	case Qt::Key_Escape:
		m_dragState = DragStep; // HACK: Force selection clearance!
		resetDragState(pScrollView);
		break;
	case Qt::Key_Home:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(0, 0);
		} else {
			pScrollView->setContentsPos(0, pScrollView->contentsY());
		}
		break;
	case Qt::Key_End:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsWidth()  - pScrollView->width(),
				pScrollView->contentsHeight() - pScrollView->height());
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsWidth()  - pScrollView->width(),
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Left:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() - pScrollView->width(),
				pScrollView->contentsY());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() - 16,
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Right:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() + pScrollView->width(),
				pScrollView->contentsY());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() + 16,
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Up:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - pScrollView->height());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - 16);
		}
		break;
	case Qt::Key_Down:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + pScrollView->height());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + 16);
		}
		break;
	case Qt::Key_PageUp:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(), 16);
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - pScrollView->height());
		}
		break;
	case Qt::Key_PageDown:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsHeight() - pScrollView->height());
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + pScrollView->height());
		}
		break;
	default:
		// Not handled here.
		return false;
	}

	// Make sure we've get focus back...
	pScrollView->setFocus();
	return true;
}


// Keyboard step handler.
bool qtractorMidiEditor::keyStep ( int iKey )
{
	// Only applicable if something is selected...
	if (m_select.items().isEmpty())
		return false;

	// Set initial bound conditions...
	if (m_dragState == DragNone) {
		m_dragState = DragStep;
		m_rectDrag  = m_select.rectView();
		m_posDrag   = m_rectDrag.topLeft();
		m_posStep   = QPoint(0, 0);
		m_pEditView->setCursor(Qt::SizeAllCursor);
		m_pEditEvent->setCursor(Qt::SizeAllCursor);
	}

	// Now to say the truth...
	if (m_dragState != DragMove &&
		m_dragState != DragStep &&
		m_dragState != DragPaste)
		return false;

	int iVerticalStep = m_pEditList->itemHeight();
	unsigned short iSnapPerBeat = m_pTimeScale->snapPerBeat();
	if (iSnapPerBeat < 1)
		iSnapPerBeat = 1;
	int iHorizontalStep
		= (m_pTimeScale->horizontalZoom() * m_pTimeScale->pixelsPerBeat())
			/ (iSnapPerBeat * 100);

	// Now determine which step...
	switch (iKey) {
	case Qt::Key_Left:
		m_posStep.setX(m_posStep.x() - iHorizontalStep);
		break;
	case Qt::Key_Right:
		m_posStep.setX(m_posStep.x() + iHorizontalStep);
		break;
	case Qt::Key_Up:
		m_posStep.setY(m_posStep.y() - iVerticalStep);
		break;
	case Qt::Key_Down:
		m_posStep.setY(m_posStep.y() + iVerticalStep);
		break;
	default:
		return false;
	}


	// Early sanity check...
	const QRect& rect = m_select.rectView();
	QPoint pos = m_posDrag;
	if (m_dragState == DragMove || m_dragState == DragPaste) {
		pos = m_pEditView->viewportToContents(
			m_pEditView->viewport()->mapFromGlobal(QCursor::pos()));
	}

	int x2 = - pos.x();
	int y2 = - pos.y();
	if (m_dragState == DragMove || m_dragState == DragPaste) {
		x2 += (m_posDrag.x() - rect.x());
		y2 += (m_posDrag.y() - rect.y());
	}

	if (m_posStep.x() < x2) {
		m_posStep.setX (x2);
	} else {
		x2 += m_pEditView->contentsWidth() - rect.width();
		if (m_posStep.x() > x2)
			m_posStep.setX (x2);
	}

	if (m_posStep.y() < y2) {
		m_posStep.setY (y2);
	} else {
		y2 += m_pEditView->contentsHeight() - rect.height();
		if (m_posStep.y() > y2)
			m_posStep.setY (y2);
	}

	// Do our deeds...
	updateDragMove(m_pEditView, pos + m_posStep);

	return true;
}


// Focus lost event.
void qtractorMidiEditor::focusOut ( qtractorScrollView *pScrollView )
{
	if (m_dragState == DragStep && m_pEditPaste == pScrollView)
		resetDragState(pScrollView);
}


// end of qtractorMidiEditor.cpp
