// qtractorFileSystem.cpp
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorFileSystem.h"

#include "qtractorAudioFile.h"
#include "qtractorDocument.h"
#include "qtractorMainForm.h"

#include <QToolButton>
#include <QComboBox>
#include <QTreeView>

#include <QFileSystemModel>

#include <QGridLayout>
#include <QHeaderView>

#include <QContextMenuEvent>

#include <QTimer>


//----------------------------------------------------------------------------
// qtractorFileSystem::FileSystemModel -- Custom file-system model.

class qtractorFileSystem::FileSystemModel : public QFileSystemModel
{
public:

	// ctor.
	FileSystemModel(QObject *pParent = 0) : QFileSystemModel(pParent) {}

protected:

	// flags override.
	Qt::ItemFlags flags(const QModelIndex& index) const
	{
		Qt::ItemFlags ret = QFileSystemModel::flags(index);

		if (QFileSystemModel::isDir(index))
			ret &= ~Qt::ItemIsDragEnabled;

		return ret;
	}
};


//----------------------------------------------------------------------------
// qdirview_wigdet -- Custom composite widget.

// ctor.
qtractorFileSystem::qtractorFileSystem ( QWidget *pParent )
	: QDockWidget(pParent)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	QDockWidget::setObjectName("qtractorFileSystem");

	// Setup member widgets...
	m_pHomeAction = new QAction(
		QIcon(":images/itemHome.png"), tr("&Home"), this);
	m_pCdUpAction = new QAction(
		QIcon(":images/itemCdUp.png"), tr("&Up"), this);

	m_pAllFilesAction = new QAction(tr("Al&l Files"), this);
	m_pAllFilesAction->setCheckable(true);

	m_pSessionFilesAction = new QAction(
		QIcon(":images/itemSessionFile.png"), tr("&Session"), this);
	m_pSessionFilesAction->setCheckable(true);

	m_pAudioFilesAction = new QAction(
		QIcon(":images/itemAudioFile.png"), tr("&Audio"), this);
	m_pAudioFilesAction->setCheckable(true);

	m_pMidiFilesAction = new QAction(
		QIcon(":images/itemMidiFile.png"), tr("&MIDI"), this);
	m_pMidiFilesAction->setCheckable(true);

	m_pHiddenFilesAction = new QAction(tr("H&idden"), this);
	m_pHiddenFilesAction->setCheckable(true);

	m_pPlayAction = new QAction(
		QIcon(":/images/transportPlay.png"), tr("&Play"), this);
	m_pPlayAction->setCheckable(true);

	// Setup member widgets...
	m_pHomeToolButton = new QToolButton();
	m_pHomeToolButton->setDefaultAction(m_pHomeAction);
	m_pHomeToolButton->setFocusPolicy(Qt::NoFocus);

	m_pCdUpToolButton = new QToolButton();
	m_pCdUpToolButton->setDefaultAction(m_pCdUpAction);
	m_pCdUpToolButton->setFocusPolicy(Qt::NoFocus);

	m_pRootPathComboBox = new QComboBox();
	m_pRootPathComboBox->setEditable(true);
//	m_pRootPathComboBox->setInsertPolicy(QComboBox::NoInsert);

	m_pFileSystemTreeView = new QTreeView();
	m_pFileSystemTreeView->setAnimated(true);
	m_pFileSystemTreeView->setIndentation(20);
	m_pFileSystemTreeView->setSortingEnabled(true);
	m_pFileSystemTreeView->sortByColumn(0, Qt::AscendingOrder);
	m_pFileSystemTreeView->setDragEnabled(true);
	m_pFileSystemTreeView->viewport()->setAcceptDrops(false);
	m_pFileSystemTreeView->setSelectionBehavior(
		QAbstractItemView::SelectRows);
	m_pFileSystemTreeView->setSelectionMode(
		QAbstractItemView::ExtendedSelection);

	m_pFileSystemModel = NULL;

	QHeaderView *pHeaderView = m_pFileSystemTreeView->header();
#if 0
#if QT_VERSION < 0x050000
	pHeaderView->setResizeMode(QHeaderView::ResizeToContents);
#else
	pHeaderView->setSectionResizeMode(QHeaderView::ResizeToContents);
#endif
#else
	pHeaderView->setStretchLastSection(true);
	pHeaderView->setDefaultSectionSize(120);
	pHeaderView->resizeSection(0, 240); // Name
	pHeaderView->resizeSection(1,  80); // Size
	pHeaderView->resizeSection(2,  60); // Type
	pHeaderView->resizeSection(3, 120);	// Date
#endif

	// Setup UI layout...
	QWidget *pGridWidget = new QWidget();
	QGridLayout *pGridLayout = new QGridLayout();
	pGridLayout->setMargin(0);
	pGridLayout->setSpacing(2);
	pGridLayout->addWidget(m_pHomeToolButton, 0, 0);
	pGridLayout->addWidget(m_pCdUpToolButton, 0, 1);
	pGridLayout->addWidget(m_pRootPathComboBox, 0, 2);
	pGridLayout->addWidget(m_pFileSystemTreeView, 1, 0, 1, 3);
	pGridLayout->setColumnStretch(2, 2);
	pGridLayout->setRowStretch(1, 4);
	pGridWidget->setLayout(pGridLayout);

	// Prepare the dockable window stuff.
	QDockWidget::setWidget(pGridWidget);
	QDockWidget::setFeatures(QDockWidget::AllDockWidgetFeatures);
	QDockWidget::setAllowedAreas(
		Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	// Some specialties to this kind of dock window...
	QDockWidget::setMinimumWidth(160);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("File System");
	QDockWidget::setWindowTitle(sCaption);
	QDockWidget::setWindowIcon(QIcon(":/images/viewFileSystem.png"));
	QDockWidget::setToolTip(sCaption);

	// Setup signal/slot connections...
	QObject::connect(m_pHomeAction,
		SIGNAL(triggered(bool)),
		SLOT(homeClicked()));
	QObject::connect(m_pCdUpAction,
		SIGNAL(triggered(bool)),
		SLOT(cdUpClicked()));

	QObject::connect(m_pAllFilesAction,
		SIGNAL(triggered(bool)),
		SLOT(filterChanged()));
	QObject::connect(m_pSessionFilesAction,
		SIGNAL(triggered(bool)),
		SLOT(filterChanged()));
	QObject::connect(m_pAudioFilesAction,
		SIGNAL(triggered(bool)),
		SLOT(filterChanged()));
	QObject::connect(m_pMidiFilesAction,
		SIGNAL(triggered(bool)),
		SLOT(filterChanged()));
	QObject::connect(m_pHiddenFilesAction,
		SIGNAL(triggered(bool)),
		SLOT(filterChanged()));
	QObject::connect(m_pPlayAction,
		SIGNAL(triggered(bool)),
		SLOT(playSlot(bool)));

	QObject::connect(m_pRootPathComboBox,
		SIGNAL(activated(const QString&)),
		SLOT(rootPathActivated(const QString&)));
	QObject::connect(m_pFileSystemTreeView,
		SIGNAL(doubleClicked(const QModelIndex&)),
		SLOT(treeViewActivated(const QModelIndex&)));

	updateRootPath(QDir::homePath());
}


// dtor.
qtractorFileSystem::~qtractorFileSystem (void)
{
	// Setup member widgets...
	m_pFileSystemTreeView->setModel(NULL);

	if (m_pFileSystemModel)
		delete m_pFileSystemModel;

	delete m_pPlayAction;
	delete m_pHiddenFilesAction;
	delete m_pMidiFilesAction;
	delete m_pAudioFilesAction;
	delete m_pSessionFilesAction;
	delete m_pAllFilesAction;
	delete m_pCdUpAction;
	delete m_pHomeAction;
}


// Accessors.
void qtractorFileSystem::setRootPath ( const QString& sRootPath )
{
	updateRootPath(sRootPath);
}

QString qtractorFileSystem::rootPath (void) const
{
	QString sRootPath;

	FileSystemModel *pFileSystemModel
		= static_cast<FileSystemModel *> (m_pFileSystemTreeView->model());
	if (pFileSystemModel)
		sRootPath = pFileSystemModel->rootPath();

	return sRootPath;
}


void qtractorFileSystem::setFlags ( Flags flags, bool on )
{
	if (flags & AllFiles)
		m_pAllFilesAction->setChecked(on);
	else
	if (flags & (SessionFiles | AudioFiles | MidiFiles))
		m_pAllFilesAction->setChecked(!on);
	if (flags & SessionFiles)
		m_pSessionFilesAction->setChecked(on);
	if (flags & AudioFiles)
		m_pAudioFilesAction->setChecked(on);
	if (flags & MidiFiles)
		m_pMidiFilesAction->setChecked(on);
	if (flags & HiddenFiles)
		m_pHiddenFilesAction->setChecked(on);
}


qtractorFileSystem::Flags qtractorFileSystem::flags (void) const
{
	int flags = 0;

	if (m_pAllFilesAction->isChecked())
		flags |= AllFiles;
	if (m_pSessionFilesAction->isChecked())
		flags |= SessionFiles;
	if (m_pAudioFilesAction->isChecked())
		flags |= AudioFiles;
	if (m_pMidiFilesAction->isChecked())
		flags |= MidiFiles;
	if (m_pHiddenFilesAction->isChecked())
		flags |= HiddenFiles;

	return Flags(flags);
}


// Model factory method.
void qtractorFileSystem::updateRootPath ( const QString& sRootPath )
{
	QString sCurrentPath;
	QModelIndex index = m_pFileSystemTreeView->currentIndex();
	if (index.isValid() && m_pFileSystemModel)
		sCurrentPath = m_pFileSystemModel->filePath(index);

	const QFileInfo info(sRootPath);
	if (info.isDir() && info.isReadable()) {
		FileSystemModel *pFileSystemModel = new FileSystemModel(this);
		QObject::connect(pFileSystemModel,
			SIGNAL(directoryLoaded(const QString&)),
			SLOT(restoreStateLoading(const QString&)));
		pFileSystemModel->setReadOnly(true);
		pFileSystemModel->setRootPath(sRootPath);
		m_pFileSystemTreeView->setModel(pFileSystemModel);
		if (m_pFileSystemModel)
			delete m_pFileSystemModel;
		m_pFileSystemModel = pFileSystemModel;
		index = m_pFileSystemModel->index(sRootPath);
		if (index.isValid()) {
			m_pFileSystemTreeView->setRootIndex(index);
			m_pFileSystemTreeView->setExpanded(index, true);
		}
		index = m_pFileSystemModel->index(sCurrentPath);
		if (index.isValid())
			m_pFileSystemTreeView->setCurrentIndex(index);
	}

	updateRootPath();
}


// Model stabilizers.
void qtractorFileSystem::updateRootPath (void)
{
	m_pRootPathComboBox->clear();

	if (m_pFileSystemModel) {
		const QString& sRootPath = m_pFileSystemModel->rootPath();
		m_pHomeAction->setEnabled(sRootPath != QDir::homePath());
		m_pRootPathComboBox->addItem(sRootPath);
		QDir dir(sRootPath);
		while (dir.cdUp())
			m_pRootPathComboBox->addItem(dir.absolutePath());
	}

	m_pCdUpAction->setEnabled(m_pRootPathComboBox->count() > 1);

	updateFilter();
}


void qtractorFileSystem::updateFilter (void)
{
	if (m_pFileSystemModel == NULL)
		return;

	QStringList filters;
	const QString sExtMask("*.%1");
	if (m_pSessionFilesAction->isChecked()) {
		filters.append(sExtMask.arg("qtr"));
		filters.append(sExtMask.arg(qtractorDocument::defaultExt()));
		filters.append(sExtMask.arg(qtractorDocument::templateExt()));
	#ifdef CONFIG_LIBZ
		filters.append(sExtMask.arg(qtractorDocument::archiveExt()));
	#endif
	}
	if (m_pAudioFilesAction->isChecked()) {
		QStringListIterator iter(qtractorAudioFileFactory::exts());
		while (iter.hasNext())
			filters.append(sExtMask.arg(iter.next()));
	}
	if (m_pMidiFilesAction->isChecked()) {
		filters.append(sExtMask.arg("midi"));
		filters.append(sExtMask.arg("mid"));
		filters.append(sExtMask.arg("smf"));
	}
	m_pFileSystemModel->setNameFilters(filters);

	if (filters.isEmpty()) {
		const bool bBlockSignals = m_pAllFilesAction->blockSignals(true);
		m_pAllFilesAction->setChecked(true);
		m_pAllFilesAction->blockSignals(bBlockSignals);
	}

	m_pFileSystemModel->setNameFilterDisables(
		m_pAllFilesAction->isChecked());

	QDir::Filters flags = m_pFileSystemModel->filter();
	if (m_pHiddenFilesAction->isChecked())
		flags |=  QDir::Hidden;
	else
		flags &= ~QDir::Hidden;
	m_pFileSystemModel->setFilter(flags);
}


// Chdir slots.
void qtractorFileSystem::cdUpClicked (void)
{
	if (m_pFileSystemModel) {
		const QString sRootPath
			= m_pFileSystemModel->rootPath();
		QDir dir(sRootPath);
		if (dir.cdUp())
			updateRootPath(dir.absolutePath());
	}
}


void qtractorFileSystem::homeClicked (void)
{
	updateRootPath(QDir::homePath());
}


void qtractorFileSystem::rootPathActivated ( const QString& sRootPath )
{
	updateRootPath(sRootPath);
}


void qtractorFileSystem::treeViewActivated ( const QModelIndex& index )
{
	if (m_pFileSystemModel) {
		const QString& sFilename
			= m_pFileSystemModel->filePath(index);
		if (m_pFileSystemModel->isDir(index))
			updateRootPath(sFilename);
		else
			activateFile(sFilename);
	}
}


// Filter slots.
void qtractorFileSystem::filterChanged (void)
{
	updateFilter();
}


// Just about to notify main-window that we're closing.
void qtractorFileSystem::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
	QDockWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


// Context-menu event handler.
void qtractorFileSystem::contextMenuEvent ( QContextMenuEvent *pContextMenuEvent )
{
	QMenu menu(this);

	menu.addAction(m_pHomeAction);
	menu.addAction(m_pCdUpAction);
	menu.addSeparator();
	menu.addAction(m_pAllFilesAction);
	menu.addAction(m_pSessionFilesAction);
	menu.addAction(m_pAudioFilesAction);
	menu.addAction(m_pMidiFilesAction);
	menu.addAction(m_pHiddenFilesAction);
	menu.addSeparator();
	menu.addAction(m_pPlayAction);

	menu.exec(pContextMenuEvent->globalPos());
}


// Audition/pre-listening player methods.
void qtractorFileSystem::setPlayState ( bool bOn )
{
	const bool bBlockSignals = m_pPlayAction->blockSignals(true);
	m_pPlayAction->setChecked(bOn);
	m_pPlayAction->blockSignals(bBlockSignals);
}


bool qtractorFileSystem::isPlayState (void) const
{
	return m_pPlayAction->isChecked();
}


// Audition/pre-listening player slot.
void qtractorFileSystem::playSlot ( bool bOn )
{
	if (m_pFileSystemModel && bOn) {
		const QModelIndex& index = m_pFileSystemTreeView->currentIndex();
		if (index.isValid())
			activateFile(m_pFileSystemModel->filePath(index));
		else
			setPlayState(false);
	}
	else setPlayState(false);
}


void qtractorFileSystem::activateFile ( const QString& sFilename )
{
	const QFileInfo info(sFilename);
	if (info.isFile() && info.isReadable()) {
		setPlayState(true);
		emit activated(info.absoluteFilePath());
	} else {
		setPlayState(false);
	}
}


// State saver.
QByteArray qtractorFileSystem::saveState (void) const
{
#if QT_VERSION < 0x050400
	QList<QByteArray> list;
#else
	QByteArrayList list;
#endif

	// 1. header-view state...
	list.append(m_pFileSystemTreeView->header()->saveState());

	// 2. file-filter flags...
	list.append(QByteArray::number(int(flags())));

	if (m_pFileSystemModel) {
		// 3. current-path...
		QModelIndex index = m_pFileSystemTreeView->currentIndex();
		list.append(m_pFileSystemModel->filePath(index).toUtf8());
		// 4. root-path and expanded/open folders...
		const QString& sRootPath
			= m_pFileSystemModel->rootPath();
		index = m_pFileSystemModel->index(sRootPath);
		while (index.isValid()) {
			if (m_pFileSystemTreeView->isExpanded(index))
				list.append(m_pFileSystemModel->filePath(index).toUtf8());
			index = m_pFileSystemTreeView->indexBelow(index);
		}
	}

#if QT_VERSION < 0x050400
	QByteArray state;
	QListIterator<QByteArray> iter(list);
	while (iter.hasNext()) {
		state.append(iter.next());
		state.append(':');
	}
	return state;
#else	
	return list.join(':');
#endif
}


// State loader.
bool qtractorFileSystem::restoreState ( const QByteArray& state )
{
	int i = 0;
	QString sCurrentPath;
	m_sRestoreStatePath.clear();
#if QT_VERSION < 0x050400
	QListIterator<QByteArray> iter(state.split(':'));
#else
	QByteArrayListIterator iter(state.split(':'));
#endif
	while (iter.hasNext()) {
		const QByteArray& data = iter.next();
		switch (++i) {
		case 1: // header-view state...
			m_pFileSystemTreeView->header()->restoreState(data);
			break;
		case 2: // file-filter flags...
			setFlags(Flags(data.toInt()));
			break;
		case 3: // current-path...
			sCurrentPath = QString::fromUtf8(data);
			break;
		case 4: // root-path...
			updateRootPath(QString::fromUtf8(data));
			break;
		default:
			// expanded/open folders...
			if (m_pFileSystemModel) {
				const QModelIndex& index
					= m_pFileSystemModel->index(QString::fromUtf8(data));
				if (index.isValid())
					m_pFileSystemTreeView->setExpanded(index, true);
			}
			break;
		}
	}

	if (m_pFileSystemModel && !sCurrentPath.isEmpty()) {
		const QModelIndex& index
			= m_pFileSystemModel->index(sCurrentPath);
		if (index.isValid()) {
			m_sRestoreStatePath = QFileInfo(sCurrentPath).absolutePath();
			m_pFileSystemTreeView->setCurrentIndex(index);
		}
	}

	return (i > 0);
}


// Directory gathering thread doing sth...
void qtractorFileSystem::restoreStateLoading ( const QString& sPath )
{
	if (m_sRestoreStatePath.isEmpty())
		return;

	if (sPath == m_sRestoreStatePath)
		QTimer::singleShot(200, this, SLOT(restoreStateTimeout()));
}


void qtractorFileSystem::restoreStateTimeout (void)
{
	if (m_sRestoreStatePath.isEmpty())
		return;

	const QModelIndex& index
		= m_pFileSystemTreeView->currentIndex();
	if (index.isValid())
		m_pFileSystemTreeView->scrollTo(index);

	m_sRestoreStatePath.clear();
}


// end of qtractorFileSystem.cpp
