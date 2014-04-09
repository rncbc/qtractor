// qtractorFiles.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorFiles.h"

#include "qtractorMainForm.h"

#include <QTabWidget>
#include <QHBoxLayout>
#include <QToolButton>

#include <QApplication>
#include <QClipboard>

#include <QContextMenuEvent>

#if QT_VERSION >= 0x050000
#include <QMimeData>
#include <QDrag>
#endif


//-------------------------------------------------------------------------
// qtractorFilesTabWidget - File/Groups dockable child window.
//

class qtractorFilesTabWidget : public QTabWidget
{
public:

	// Constructor.
	qtractorFilesTabWidget(QWidget *pParent) : QTabWidget(pParent) {}

protected:

	// Minimum recommended.
	QSize sizeHint() const { return QTabWidget::minimumSize(); }
};


//-------------------------------------------------------------------------
// qtractorFiles - File/Groups dockable window.
//

// Constructor.
qtractorFiles::qtractorFiles ( QWidget *pParent )
	: QDockWidget(pParent)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	QDockWidget::setObjectName("qtractorFiles");

	// Create file type selection tab widget.
	const QFont& font = QDockWidget::font();
	m_pTabWidget = new qtractorFilesTabWidget(this);
	m_pTabWidget->setFont(QFont(font.family(), font.pointSize() - 1));
	m_pTabWidget->setTabPosition(QTabWidget::South);
	// Create local tabs.
	m_pAudioListView = new qtractorAudioListView();
	m_pMidiListView  = new qtractorMidiListView();
	// Add respective...
	m_pTabWidget->addTab(m_pAudioListView, tr("Audio"));
	m_pTabWidget->addTab(m_pMidiListView, tr("MIDI"));
	// Icons...
	m_pTabWidget->setTabIcon(qtractorFiles::Audio, QIcon(":/images/trackAudio.png"));
	m_pTabWidget->setTabIcon(qtractorFiles::Midi,  QIcon(":/images/trackMidi.png"));
	m_pTabWidget->setUsesScrollButtons(false);

	// Player button (initially disabled)...
	m_pPlayWidget = new QWidget(m_pTabWidget);
	m_pPlayLayout = new QHBoxLayout(/*m_pPlayWidget*/);
	m_pPlayLayout->setMargin(2);
	m_pPlayLayout->setSpacing(2);
	m_pPlayWidget->setLayout(m_pPlayLayout);

	m_iPlayUpdate = 0;
	m_pPlayButton = new QToolButton(m_pPlayWidget);
	m_pPlayButton->setIcon(QIcon(":/images/transportPlay.png"));
	m_pPlayButton->setToolTip(tr("Play file"));
	m_pPlayButton->setCheckable(true);
	m_pPlayButton->setEnabled(false);
	m_pPlayLayout->addWidget(m_pPlayButton);
	m_pTabWidget->setCornerWidget(m_pPlayWidget, Qt::BottomRightCorner);

	// Common file list-view actions...
	m_pNewGroupAction = new QAction(
		QIcon(":/images/itemGroup.png"), tr("New &Group..."), this);
	m_pOpenFileAction = new QAction(
		QIcon(":/images/itemFile.png"), tr("Add &Files..."), this);
	m_pCutItemAction = new QAction(
		QIcon(":/images/editCut.png"), tr("Cu&t"), NULL);
	m_pCopyItemAction = new QAction(
		QIcon(":/images/editCopy.png"), tr("&Copy"), NULL);
	m_pPasteItemAction = new QAction(
		QIcon(":/images/editPaste.png"), tr("&Paste"), NULL);
	m_pRenameItemAction = new QAction(
		QIcon(":/images/formEdit.png"), tr("Re&name"), this);
	m_pRemoveItemAction = new QAction(
		QIcon(":/images/formRemove.png"), tr("&Remove"), NULL);
	m_pPlayItemAction = new QAction(
		QIcon(":/images/transportPlay.png"), tr("Pla&y"), this);
	m_pPlayItemAction->setCheckable(true);
	m_pCleanupAction = new QAction(tr("Cl&eanup"), this);

//	m_pNewGroupAction->setShortcut(tr("Ctrl+G"));
//	m_pOpenFileAction->setShortcut(tr("Ctrl+F"));
	m_pCutItemAction->setShortcut(tr("Ctrl+X"));
	m_pCopyItemAction->setShortcut(tr("Ctrl+C"));
	m_pPasteItemAction->setShortcut(tr("Ctrl+V"));
//	m_pRenameItemAction->setShortcut(tr("Ctrl+N"));
	m_pRemoveItemAction->setShortcut(tr("Del"));
//	m_pPlayItemAction->setShortcut(tr("Ctrl+Y"));
//	m_pCleanupAction->setShortcut(tr("Ctrl+E"));

	// Some actions surely need those
	// shortcuts firmly attached...
#if 0
	QDockWidget::addAction(m_pNewGroupAction);
	QDockWidget::addAction(m_pOpenFileAction);
	QDockWidget::addAction(m_pCutItemAction);
	QDockWidget::addAction(m_pCopyItemAction);
	QDockWidget::addAction(m_pPasteItemAction);
	QDockWidget::addAction(m_pRenameItemAction);
	QDockWidget::addAction(m_pRemoveItemAction);
	QDockWidget::addAction(m_pPlayItemAction);
	QDockWidget::addAction(m_pCleanupAction);
#endif
	// Prepare the dockable window stuff.
	QDockWidget::setWidget(m_pTabWidget);
	QDockWidget::setFeatures(QDockWidget::AllDockWidgetFeatures);
	QDockWidget::setAllowedAreas(
		Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	// Some specialties to this kind of dock window...
	QDockWidget::setMinimumWidth(160);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Files");
	QDockWidget::setWindowTitle(sCaption);
	QDockWidget::setWindowIcon(QIcon(":/images/viewFiles.png"));
	QDockWidget::setToolTip(sCaption);

	// Make it initially stable...
	stabilizeSlot();

	// Child widgets signal/slots... 
	QObject::connect(m_pTabWidget,
		SIGNAL(currentChanged(int)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pAudioListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pAudioListView,
		SIGNAL(itemClicked(QTreeWidgetItem*,int)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pMidiListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pMidiListView,
		SIGNAL(itemClicked(QTreeWidgetItem*,int)),
		SLOT(stabilizeSlot()));
	QObject::connect(m_pNewGroupAction,
		SIGNAL(triggered(bool)),
		SLOT(newGroupSlot()));
	QObject::connect(m_pOpenFileAction,
		SIGNAL(triggered(bool)),
		SLOT(openFileSlot()));
	QObject::connect(m_pCutItemAction,
		SIGNAL(triggered(bool)),
		SLOT(cutItemSlot()));
	QObject::connect(m_pCopyItemAction,
		SIGNAL(triggered(bool)),
		SLOT(copyItemSlot()));
	QObject::connect(m_pPasteItemAction,
		SIGNAL(triggered(bool)),
		SLOT(pasteItemSlot()));
	QObject::connect(m_pRenameItemAction,
		SIGNAL(triggered(bool)),
		SLOT(renameItemSlot()));
	QObject::connect(m_pRemoveItemAction,
		SIGNAL(triggered(bool)),
		SLOT(removeItemSlot()));
	QObject::connect(m_pPlayItemAction,
		SIGNAL(triggered(bool)),
		SLOT(playSlot(bool)));
	QObject::connect(m_pCleanupAction,
		SIGNAL(triggered(bool)),
		SLOT(cleanupSlot()));

	QObject::connect(m_pPlayButton,
		SIGNAL(toggled(bool)),
		SLOT(playSlot(bool)));

	QObject::connect(QApplication::clipboard(),
		SIGNAL(dataChanged()),
		SLOT(stabilizeSlot()));
}


// Destructor.
qtractorFiles::~qtractorFiles (void)
{
	delete m_pNewGroupAction;
	delete m_pOpenFileAction;
	delete m_pCutItemAction;
	delete m_pCopyItemAction;
	delete m_pPasteItemAction;
	delete m_pRenameItemAction;
	delete m_pRemoveItemAction;
	delete m_pCleanupAction;
	delete m_pPlayItemAction;

	// No need to delete child widgets, Qt does it all for us.
}


// Just about to notify main-window that we're closing.
void qtractorFiles::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
	QDockWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Audio file list view accessor.
qtractorAudioListView *qtractorFiles::audioListView (void) const
{
	return m_pAudioListView;
}


// MIDI file list view accessor.
qtractorMidiListView *qtractorFiles::midiListView (void) const
{
	return m_pMidiListView;
}


// Clear everything on sight.
void qtractorFiles::clear (void)
{
	m_pAudioListView->clear();
	m_pMidiListView->clear();

	setPlayState(false);
}


// Check whether one of the widgets has focus (oveerride method)
bool qtractorFiles::hasFocus (void) const
{
	if (m_pAudioListView->hasFocus() || m_pMidiListView->hasFocus())
		return true;

	return QDockWidget::hasFocus();
}


// Tell whether a file item is currently selected.
bool qtractorFiles::isFileSelected (void) const
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView) {
		QTreeWidgetItem *pItem = pFileListView->currentItem();
		return (pItem && pItem->type() == qtractorFileListView::FileItem);
	}

	return false;
}


// Audio file addition convenience method.
void qtractorFiles::addAudioFile ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
	m_pAudioListView->addFileItem(sFilename);
}


// MIDI file addition convenience method.
void qtractorFiles::addMidiFile ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
	m_pMidiListView->addFileItem(sFilename);
}


// Audio file selection convenience method.
void qtractorFiles::selectAudioFile ( const QString& sFilename )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
	m_pAudioListView->selectFileItem(sFilename);
}


// MIDI file selection convenience method.
void qtractorFiles::selectMidiFile ( const QString& sFilename,
	int iTrackChannel )
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
	m_pMidiListView->selectFileItem(sFilename, iTrackChannel);
}


// Audition/pre-listening player methods.
void qtractorFiles::setPlayState ( bool bOn )
{
	++m_iPlayUpdate;
	m_pPlayItemAction->setChecked(bOn);
	m_pPlayButton->setChecked(bOn);
	--m_iPlayUpdate;
}

bool qtractorFiles::isPlayState (void) const
{
	return m_pPlayItemAction->isChecked();
}


// Retrieve current selected file list view.
qtractorFileListView *qtractorFiles::currentFileListView (void) const
{
	switch (m_pTabWidget->currentIndex()) {
	case qtractorFiles::Audio:
		return m_pAudioListView;
	case qtractorFiles::Midi:
		return m_pMidiListView;
	default:
		return NULL;
	}
}


// Open and add a new file item below the current group one.
void qtractorFiles::openFileSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->openFile();
}


// Add a new group item below the current one.
void qtractorFiles::newGroupSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->newGroup();
}


// Cut current file item(s) to clipboard.
void qtractorFiles::cutItemSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->copyItem(true);
}


// Copy current file item(s) to clipboard.
void qtractorFiles::copyItemSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->copyItem(false);
}


// Paste file item(s) from clipboard.
void qtractorFiles::pasteItemSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->pasteItem();
}


// Rename current group/file item.
void qtractorFiles::renameItemSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->renameItem();
}


// Remove current group/file item.
void qtractorFiles::removeItemSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->removeItem();
}


// Current item change slots.
void qtractorFiles::stabilizeSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView) {
		QTreeWidgetItem *pItem = pFileListView->currentItem();
		m_pCutItemAction->setEnabled(
			pItem && pItem->type() == qtractorFileListView::FileItem);
		m_pCopyItemAction->setEnabled(
			pItem && pItem->type() == qtractorFileListView::FileItem);
		m_pPasteItemAction->setEnabled(
			(QApplication::clipboard()->mimeData())->hasUrls());
		m_pRenameItemAction->setEnabled(
			pItem && pItem->type() == qtractorFileListView::GroupItem);
		m_pRemoveItemAction->setEnabled(
			pItem && pItem->type() != qtractorFileListView::ChannelItem);
		m_pCleanupAction->setEnabled(pFileListView->topLevelItemCount() > 0);
		bool bPlayEnabled = (
			pItem && pItem->type() != qtractorFileListView::GroupItem);
		m_pPlayItemAction->setEnabled(bPlayEnabled);
		m_pPlayButton->setEnabled(bPlayEnabled);
	}

	// Stabilize main form as well...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Audition/pre-listening player slot.
void qtractorFiles::playSlot ( bool bOn )
{
	if (m_iPlayUpdate > 0)
		return;

	setPlayState(bOn);

	if (bOn) {
		qtractorFileListView *pFileListView = currentFileListView();
		if (pFileListView)
			pFileListView->activateItem();
	}
}


// Clean-up unused file items.
void qtractorFiles::cleanupSlot (void)
{
	qtractorFileListView *pFileListView = currentFileListView();
	if (pFileListView)
		pFileListView->cleanup();
}


// Tab page switch slots.
void qtractorFiles::pageAudioSlot (void)
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Audio);
}

void qtractorFiles::pageMidiSlot (void)
{
	m_pTabWidget->setCurrentIndex(qtractorFiles::Midi);
}


// Context menu request event handler.
void qtractorFiles::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	QMenu menu(this);

	// Construct context menu.
	menu.addAction(m_pNewGroupAction);
	menu.addAction(m_pOpenFileAction);
	menu.addSeparator();
	menu.addAction(m_pCutItemAction);
	menu.addAction(m_pCopyItemAction);
	menu.addAction(m_pPasteItemAction);
	menu.addSeparator();
	menu.addAction(m_pRenameItemAction);
	menu.addSeparator();
	menu.addAction(m_pRemoveItemAction);
	menu.addAction(m_pCleanupAction);
	menu.addSeparator();
	menu.addAction(m_pPlayItemAction);

	menu.addSeparator();

	// Switch page options...
	switch (m_pTabWidget->currentIndex()) {
	case qtractorFiles::Audio:
		menu.addAction(QIcon(":/images/trackMidi.png"),
			tr("MIDI Files"), this, SLOT(pageMidiSlot()));
		break;
	case qtractorFiles::Midi:
		menu.addAction(QIcon(":/images/trackAudio.png"),
			tr("Audio Files"), this, SLOT(pageAudioSlot()));
		break;
	default:
		break;
	}
	
	menu.exec(pContextMenuEvent->globalPos());
}


// end of qtractorFiles.cpp
