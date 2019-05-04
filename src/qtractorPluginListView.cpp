// qtractorPluginListView.cpp
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

#include "qtractorPluginListView.h"

#include "qtractorPluginFactory.h"
#include "qtractorPluginCommand.h"
#include "qtractorPluginSelectForm.h"
#include "qtractorPluginForm.h"

#include "qtractorRubberBand.h"

#include "qtractorSession.h"
#include "qtractorOptions.h"

#include "qtractorMainForm.h"

#include "qtractorAudioEngine.h"
#include "qtractorMidiEngine.h"
#include "qtractorMidiManager.h"
#include "qtractorConnections.h"

#include "qtractorInsertPlugin.h"

#include <QItemDelegate>
#include <QPainter>
#include <QMenu>
#include <QToolTip>
#include <QScrollBar>

#include <QFileDialog>
#include <QMessageBox>
#include <QDomDocument>

#include <QMouseEvent>
#include <QDragLeaveEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QContextMenuEvent>

#if QT_VERSION >= 0x050000
#include <QMimeData>
#include <QDrag>
#endif


//----------------------------------------------------------------------------
// qtractorPluginListView::TinyScrollBarStyle -- Custom tiny scrollbar style.
//
#if QT_VERSION < 0x040600
#include <QCDEStyle>
#else
#include <QProxyStyle>
class QCDEStyle : public QProxyStyle {};
#endif

class qtractorPluginListView::TinyScrollBarStyle : public QCDEStyle
{
protected:

	// Custom virtual override.
	int pixelMetric(PixelMetric pm,
		const QStyleOption *option = 0, const QWidget *pWidget = 0) const
	{
		if (pm == QStyle::PM_ScrollBarExtent)
			return 8;

		return QCDEStyle::pixelMetric(pm, option, pWidget);
	}
};


//----------------------------------------------------------------------------
// qtractorPluginListItemDelegate -- Plugin list view item delgate.

class qtractorPluginListItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	qtractorPluginListItemDelegate(QListWidget *pListWidget)
		: QItemDelegate(pListWidget), m_pListWidget(pListWidget) {}

protected:

	// Overridden paint method.
	void paint(QPainter *pPainter, const QStyleOptionViewItem& option,
		const QModelIndex& index) const
	{
		qtractorPluginListItem *pItem
			= static_cast<qtractorPluginListItem *> (
				m_pListWidget->item(index.row()));
		// nb. Unselectable items get special grayed out painting...
		if (pItem) {
			pPainter->save();
			const QPalette& pal = option.palette;
			QColor rgbBack;
			QColor rgbFore;
			if (option.state & QStyle::State_HasFocus) {
				rgbBack = pal.highlight().color();
				rgbFore = pal.highlightedText().color();
			} else {
				rgbBack = pal.window().color();
				rgbFore = pal.windowText().color();
			}
			// Fill the background...
			pPainter->fillRect(option.rect, rgbBack);
			const QSize& iconSize = m_pListWidget->iconSize();
			// Draw the direct access parameter value status...
			QPolygon polyg(3);
			qtractorPlugin *pPlugin = pItem->plugin();
			qtractorPluginParam *pDirectAccessParam = NULL;
			qtractorMidiControlObserver *pDirectAccessObserver = NULL;
			if (pPlugin)
				pDirectAccessParam = pPlugin->directAccessParam();
			if (pDirectAccessParam)
				pDirectAccessObserver = pDirectAccessParam->observer();
			if (pDirectAccessObserver) {
				const float fScale = pDirectAccessObserver->scaleFromValue(
					pDirectAccessParam->value(),
					pDirectAccessParam->isLogarithmic());
				QRect rectValue = option.rect
					.adjusted(iconSize.width(), 1, -2, -2);
				const int iDirectAccessWidth = rectValue.width();
				pItem->setDirectAccessWidth(iDirectAccessWidth);
				rectValue.setWidth(int(fScale * float(iDirectAccessWidth)));
				pPainter->fillRect(rectValue, rgbBack.lighter(140));
				polyg.setPoint(0, rectValue.right(), rectValue.top() + 1);
				polyg.setPoint(1, rectValue.right() - 2, rectValue.bottom() + 1);
				polyg.setPoint(2, rectValue.right() + 2, rectValue.bottom() + 1);
			}
			// Draw the icon...
			QRect rect = option.rect;
			pPainter->drawPixmap(rect.left() + 2,
				rect.top() + ((rect.height() - iconSize.height()) >> 1) + 1,
				pItem->icon().pixmap(iconSize));
			// Draw the text...
			rect.setLeft(iconSize.width() + 2);
			pPainter->setPen(rgbFore);
			pPainter->drawText(rect,
				Qt::AlignLeft | Qt::AlignVCenter, pItem->text());
			// Draw frame lines...
			pPainter->setPen(rgbBack.lighter(150));
			pPainter->drawLine(
				option.rect.left(),  option.rect.top(),
				option.rect.right(), option.rect.top());
			pPainter->setPen(rgbBack.darker(150));
			pPainter->drawLine(
				option.rect.left(),  option.rect.bottom(),
				option.rect.right(), option.rect.bottom());
			if (pDirectAccessObserver) {
				pPainter->setRenderHint(QPainter::Antialiasing, true);
				pPainter->setBrush(rgbBack.darker(160));
				pPainter->drawPolygon(polyg);
				pPainter->setPen(rgbBack.lighter(180));
				pPainter->drawLine(polyg.at(0), polyg.at(1));
				pPainter->setRenderHint(QPainter::Antialiasing, false);
			}
			pPainter->restore();
		//	if (option.state & QStyle::State_HasFocus)
		//		QItemDelegate::drawFocus(pPainter, option, option.rect);
		} else {
			// Others do as default...
			QItemDelegate::paint(pPainter, option, index);
		}
	}

	// Override height-hint.
	QSize sizeHint (
		const QStyleOptionViewItem& option, const QModelIndex& index ) const
	{
		QSize size(QItemDelegate::sizeHint(option, index));
		size.setHeight(16);
		return size;
	}

private:

	QListWidget *m_pListWidget;
};


//----------------------------------------------------------------------------
// qtractorPluginListItem -- Plugins list item.

// Constructors.
qtractorPluginListItem::qtractorPluginListItem ( qtractorPlugin *pPlugin )
	: QListWidgetItem(), m_pPlugin(pPlugin), m_iDirectAccessWidth(0)
{
	m_pPlugin->addItem(this);

	QString sText;

	qtractorPluginType *pType = m_pPlugin->type();
	if (pType->index() > 0) {
		const QString& sAudioItem = QObject::tr("%1 (Audio)");
		if (pType->typeHint() == qtractorPluginType::Insert) {
			qtractorAudioInsertPlugin *pAudioInsertPlugin
				= static_cast<qtractorAudioInsertPlugin *> (m_pPlugin);
			if (pAudioInsertPlugin) {
				qtractorAudioBus *pAudioBus = pAudioInsertPlugin->audioBus();
				if (pAudioBus) {
					sText = sAudioItem.arg(pAudioBus->busName());
					sText.replace('_', ' '); // Humanize text!
				}
			}
		}
		else
		if (pType->typeHint() == qtractorPluginType::AuxSend) {
			qtractorAudioAuxSendPlugin *pAudioAuxSendPlugin
				= static_cast<qtractorAudioAuxSendPlugin *> (m_pPlugin);
			if (pAudioAuxSendPlugin)
				sText = sAudioItem.arg(pAudioAuxSendPlugin->audioBusName());
		}
	} else {
		const QString& sMidiItem = QObject::tr("%1 (MIDI)");
		if (pType->typeHint() == qtractorPluginType::Insert) {
			qtractorMidiInsertPlugin *pMidiInsertPlugin
				= static_cast<qtractorMidiInsertPlugin *> (m_pPlugin);
			if (pMidiInsertPlugin) {
				qtractorMidiBus *pMidiBus = pMidiInsertPlugin->midiBus();
				if (pMidiBus) {
					sText = sMidiItem.arg(pMidiBus->busName());
					sText.replace('_', ' '); // Humanize text!
				}
			}
		}
		else
		if (pType->typeHint() == qtractorPluginType::AuxSend) {
			qtractorMidiAuxSendPlugin *pMidiAuxSendPlugin
				= static_cast<qtractorMidiAuxSendPlugin *> (m_pPlugin);
			if (pMidiAuxSendPlugin)
				sText = sMidiItem.arg(pMidiAuxSendPlugin->midiBusName());
		}
	}

	QListWidgetItem::setText(sText.isEmpty() ? pType->name() : sText);

	updateActivated();
}


// Destructor.
qtractorPluginListItem::~qtractorPluginListItem (void)
{
	m_pPlugin->removeItem(this);
}


// Plugin container accessor.
qtractorPlugin *qtractorPluginListItem::plugin (void) const
{
	return m_pPlugin;
}


// Activation methods.
void qtractorPluginListItem::updateActivated (void)
{
	QListWidgetItem::setIcon(
		*qtractorPluginListView::itemIcon(
			m_pPlugin && m_pPlugin->isActivated() ? 1 : 0));
}


//----------------------------------------------------------------------------
// qtractorPluginListView -- Plugin chain list widget instance.
//
int    qtractorPluginListView::g_iItemRefCount = 0;
QIcon *qtractorPluginListView::g_pItemIcons[2] = { NULL, NULL };

// Construcctor.
qtractorPluginListView::qtractorPluginListView ( QWidget *pParent )
	: QListWidget(pParent), m_pPluginList(NULL), m_pClickedItem(NULL)
{
	if (++g_iItemRefCount == 1) {
		g_pItemIcons[0] = new QIcon(":/images/itemLedOff.png");
		g_pItemIcons[1] = new QIcon(":/images/itemLedOn.png");
	}

	// Drag-and-drop stuff.
	m_dragCursor  = DragNone;
	m_dragState   = DragNone;
	m_pDragItem   = NULL;
	m_pDropItem   = NULL;
	m_pRubberBand = NULL;

	// Common tiny scrollbar style stuff.
	m_pTinyScrollBarStyle = NULL;

//	QListWidget::setDragEnabled(true);
	QListWidget::setAcceptDrops(true);
	QListWidget::setDropIndicatorShown(true);
	QListWidget::setAutoScroll(true);

	QListWidget::setIconSize(QSize(10, 10));
	QListWidget::setItemDelegate(new qtractorPluginListItemDelegate(this));
	QListWidget::setSelectionMode(QAbstractItemView::SingleSelection);
	QListWidget::setMouseTracking(true);

	QListWidget::viewport()->setBackgroundRole(QPalette::Window);

	QListWidget::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//	QListWidget::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QListWidget::setFrameShape(QFrame::Panel);
	QListWidget::setFrameShadow(QFrame::Sunken);

	// Trap for help/tool-tips events.
	QListWidget::viewport()->installEventFilter(this);

	// Double-click handling...
	QObject::connect(this,
		SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		SLOT(itemDoubleClickedSlot(QListWidgetItem*)));
#if 0
	QObject::connect(this,
		SIGNAL(itemActivated(QListWidgetItem*)),
		SLOT(itemActivatedSlot(QListWidgetItem*)));
#endif
}


// Destructor.
qtractorPluginListView::~qtractorPluginListView (void)
{
	// No need to delete child widgets, Qt does it all for us
	clear();

	setPluginList(NULL);

	if (m_pTinyScrollBarStyle) {
		delete m_pTinyScrollBarStyle;
		m_pTinyScrollBarStyle = NULL;
	}

	if (--g_iItemRefCount == 0) {
		for (int i = 0; i < 2; ++i) {
			delete g_pItemIcons[i];
			g_pItemIcons[i] = NULL;
		}
	}
}


// Plugin list accessors.
void qtractorPluginListView::setPluginList ( qtractorPluginList *pPluginList )
{
	if (m_pPluginList)
		m_pPluginList->removeView(this);

	m_pPluginList = pPluginList;

	if (m_pPluginList)
		m_pPluginList->addView(this);

	refresh();
}

qtractorPluginList *qtractorPluginListView::pluginList (void) const
{
	return m_pPluginList;
}


// Special scrollbar style accessors.
void qtractorPluginListView::setTinyScrollBar ( bool bTinyScrollBar )
{
	if (bTinyScrollBar) {
		m_pTinyScrollBarStyle = new TinyScrollBarStyle();
		QListWidget::verticalScrollBar()->setStyle(m_pTinyScrollBarStyle);
		QListWidget::horizontalScrollBar()->setStyle(m_pTinyScrollBarStyle);
	} else {
		QListWidget::verticalScrollBar()->setStyle(NULL);
		QListWidget::horizontalScrollBar()->setStyle(NULL);
		if (m_pTinyScrollBarStyle) {
			delete m_pTinyScrollBarStyle;
			m_pTinyScrollBarStyle = NULL;
		}
	}
}


// Plugin list refreshner
void qtractorPluginListView::refresh (void)
{
	clear();

	if (m_pPluginList) {
		QListWidget::setUpdatesEnabled(false);
		for (qtractorPlugin *pPlugin = m_pPluginList->first();
				pPlugin; pPlugin = pPlugin->next()) {
			QListWidget::addItem(new qtractorPluginListItem(pPlugin));
		}
		QListWidget::setUpdatesEnabled(true);
	}
}


// Master clean-up.
void qtractorPluginListView::clear (void)
{
	dragLeaveEvent(NULL);

	QListWidget::clear();
}


// Get an item index, given the plugin reference...
int qtractorPluginListView::pluginItem ( qtractorPlugin *pPlugin )
{
	const int iItemCount = QListWidget::count();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		qtractorPluginListItem *pItem
			= static_cast<qtractorPluginListItem *> (QListWidget::item(iItem));
		if (pItem && pItem->plugin() == pPlugin)
			return iItem;
	}

	return -1;
}


// Move item on list.
void qtractorPluginListView::moveItem (
	qtractorPluginListItem *pItem, qtractorPluginListItem *pNextItem )
{
	if (pItem == NULL)
		return;

	if (m_pPluginList == NULL)
		return;

	// The plugin to be moved...	
	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	// To be after this one...
	qtractorPlugin *pNextPlugin = NULL;
	if (pNextItem)
		pNextPlugin = pNextItem->plugin();

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorMovePluginCommand(pPlugin, pNextPlugin, m_pPluginList));

	emit contentsChanged();
}


// Copy item on list.
void qtractorPluginListView::copyItem (
	qtractorPluginListItem *pItem, qtractorPluginListItem *pNextItem )
{
	if (pItem == NULL)
		return;

	if (m_pPluginList == NULL)
		return;

	// The plugin to be moved...	
	qtractorPlugin *pPlugin = pItem->plugin();
	// Clone/copy the new plugin here...
	if (pPlugin)
		pPlugin = m_pPluginList->copyPlugin(pPlugin);
	if (pPlugin == NULL)
		return;

	// To be after this one...
	qtractorPlugin *pNextPlugin = NULL;
	if (pNextItem)
		pNextPlugin = pNextItem->plugin();

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorInsertPluginCommand(
			tr("copy plugin"), pPlugin, pNextPlugin));

	emit contentsChanged();
}


// Add a new plugin slot.
void qtractorPluginListView::addPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginSelectForm selectForm(this);
	selectForm.setPluginList(m_pPluginList);
	if (!selectForm.exec())
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	bool bOpenEditor = false;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		bOpenEditor = pOptions->bOpenEditor;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Make it a undoable command...
	qtractorAddPluginCommand *pAddPluginCommand
		= new qtractorAddPluginCommand();

	for (int i = 0; i < selectForm.pluginCount(); ++i) {
		// Add an actual plugin item...
		qtractorPlugin *pPlugin
			= qtractorPluginFactory::createPlugin(m_pPluginList,
				selectForm.pluginFilename(i),
				selectForm.pluginIndex(i),
				selectForm.pluginTypeHint(i));
		if (pPlugin) {
			pPlugin->setActivated(selectForm.isPluginActivated());
			pAddPluginCommand->addPlugin(pPlugin);
		}
	}

	// Instantiate selected plugins...
	pSession->execute(pAddPluginCommand);

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	// Show plugin forms/editors right away...
	QListIterator<qtractorPlugin *> iter(pAddPluginCommand->plugins());
	while (iter.hasNext()) {
		qtractorPlugin *pPlugin = iter.next();
		if (bOpenEditor && (pPlugin->type())->isEditor())
			pPlugin->openEditor();
		else
			pPlugin->openForm();
	}

	emit contentsChanged();
}


// Remove an existing plugin slot.
void qtractorPluginListView::removePlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(new qtractorRemovePluginCommand(pPlugin));

	emit contentsChanged();
}


// Activate existing plugin slot.
void qtractorPluginListView::activatePlugin (void)
{
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorActivatePluginCommand(pPlugin, !pPlugin->isActivated()));

	emit contentsChanged();
}


// Activate all plugins.
void qtractorPluginListView::activateAllPlugins (void)
{
	if (m_pPluginList == NULL)
		return;

	// Check whether everyone is already activated...
	if (m_pPluginList->count() < 1 || m_pPluginList->isActivatedAll())
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorActivatePluginCommand *pActivateAllCommand
		= new qtractorActivatePluginCommand(NULL, true);
	pActivateAllCommand->setName(tr("activate all plugins"));

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (!pPlugin->isActivated())
			pActivateAllCommand->addPlugin(pPlugin);
	}

	pSession->execute(pActivateAllCommand);

	emit contentsChanged();
}


// Dectivate all plugins.
void qtractorPluginListView::deactivateAllPlugins (void)
{
	if (m_pPluginList == NULL)
		return;

	// Check whether everyone is already dectivated...
	if (!m_pPluginList->isActivated())
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorActivatePluginCommand *pDeactivateAllCommand
		= new qtractorActivatePluginCommand(NULL, false);
	pDeactivateAllCommand->setName(tr("deactivate all plugins"));

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated())
			pDeactivateAllCommand->addPlugin(pPlugin);
	}

	pSession->execute(pDeactivateAllCommand);

	emit contentsChanged();
}


// Remove all plugins.
void qtractorPluginListView::removeAllPlugins (void)
{
	if (m_pPluginList == NULL)
		return;

	// Check whether there's any...
	if (m_pPluginList->count() < 1)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorRemovePluginCommand *pRemoveAllCommand
		= new qtractorRemovePluginCommand();
	pRemoveAllCommand->setName(tr("remove all plugins"));

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		pRemoveAllCommand->addPlugin(pPlugin);
	}

	pSession->execute(pRemoveAllCommand);

	emit contentsChanged();
}


// Move an existing plugin upward slot.
void qtractorPluginListView::moveUpPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	const int iNextItem = QListWidget::row(pItem) - 1;
	if (iNextItem < 0)
		return;

	qtractorPluginListItem *pNextItem
		= static_cast<qtractorPluginListItem *> (
			QListWidget::item(iNextItem));
	if (pNextItem == NULL)
		return;

	moveItem(pItem, pNextItem);
}


// Move an existing plugin downward slot.
void qtractorPluginListView::moveDownPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	const int iNextItem = QListWidget::row(pItem) + 1;
	if (iNextItem >= QListWidget::count())
		return;

	qtractorPluginListItem *pNextItem
		= static_cast<qtractorPluginListItem *> (
			QListWidget::item(iNextItem + 1));

	moveItem(pItem, pNextItem);
}


// Load a plugin preset name.
void qtractorPluginListView::loadPresetPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	// Retrieve preset-name from action text...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction == NULL)
		return;

	const QString sPreset
		= pAction->text().remove('&');
	if (sPreset.isEmpty())
		return;

	pPlugin->loadPresetEx(sPreset);
}


// Select a direct access parameter index.
void qtractorPluginListView::directAccessPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	// Retrieve direct access parameter index from action data...
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction == NULL)
		return;

	const int iDirectAccessParamIndex = pAction->data().toInt();

	// Make it a undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;
	
	pSession->execute(
		new qtractorDirectAccessParamCommand(pPlugin, iDirectAccessParamIndex));
}


// Show/hide an existing plugin form slot.
void qtractorPluginListView::propertiesPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	if (pPlugin->isFormVisible())
		pPlugin->closeForm();
	else
		pPlugin->openForm();
}


// Show/hide an existing plugin editor (GUI) slot.
void qtractorPluginListView::editPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;


	if (pPlugin->isEditorVisible())
		pPlugin->closeEditor();
	else
		pPlugin->openEditor();
}


// Import plugin-list slot.
void qtractorPluginListView::importPlugins (void)
{
	if (m_pPluginList == NULL)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// Default file-name extension (suffix)...
	const QString sExt("xml");

	// We'll assume that there's an external file...
	QString sFilename;

	// Construct the import-from-file dialog...
	const QString& sTitle
		= tr("Import Plugins") + " - " QTRACTOR_TITLE;

	QStringList filters;
	filters.append(tr("XML files (*.%1)").arg(sExt));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = NULL;
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to open...
	sFilename = QFileDialog::getOpenFileName(pParentWidget,
		sTitle, pOptions->sPluginsDir, sFilter, NULL, options);
#else
	// Construct open-file dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, pOptions->sPluginsDir, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
	fileDialog.setFileMode(QFileDialog::ExistingFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sPluginsDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
#endif

	// Do we have any?
	if (sFilename.isEmpty())
		return;

	// Save chosen directory as default...
	pOptions->sPluginsDir = QFileInfo(sFilename).absolutePath();

	// Maybe ask whether we may actually reset the current list...
	if (m_pPluginList->count() > 0 && pOptions->bConfirmRemove) {
		if (QMessageBox::warning(this,
			tr("Warning") + " - " QTRACTOR_TITLE,
			tr("About to remove and import all plugins:\n\n"
			"\"%1\"\n\n"
			"Are you sure?")
			.arg(QFileInfo(sFilename).fileName()),
			QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Cancel) {
			return;
		}
	}

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Import plugin-list state from XML document...
	//
	QDomDocument doc("qtractorPluginList");
	qtractorPluginList::Document(&doc, m_pPluginList).load(sFilename);

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	emit contentsChanged();
}


// Export plugin-list slot.
void qtractorPluginListView::exportPlugins (void)
{
	if (m_pPluginList == NULL)
		return;

	// Check whether there's any...
	if (m_pPluginList->count() < 1)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// We'll need this, sure.
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == NULL)
		return;

	// The default file-name suffix/extension...
	const QString sExt("xml");

	// Suggest a brand new filename ...
	QString sPluginsName = qtractorSession::sanitize(pSession->sessionName());
	if (!sPluginsName.isEmpty())
		sPluginsName += '-';
	sPluginsName += qtractorSession::sanitize(m_pPluginList->name());

	// If there are any existing, similar file-names,
	// add and increment a numeric version suffix...
	QFileInfo fi(pOptions->sPluginsDir, sPluginsName + '.' + sExt);
	int iFileNo = 0;
	while (fi.exists()) {
		fi.setFile(pOptions->sPluginsDir, sPluginsName
			+ '-' + QString::number(++iFileNo) + '.' + sExt);
	}

	// Got that...
	QString sFilename = fi.absoluteFilePath();

	// Construct the export-to-file dialog...
	const QString& sTitle
		= tr("Export Plugins") + " - " QTRACTOR_TITLE;

	QStringList filters;
	filters.append(tr("XML files (*.%1)").arg(sExt));
	filters.append(tr("All files (*.*)"));
	const QString& sFilter = filters.join(";;");

	QWidget *pParentWidget = NULL;
	QFileDialog::Options options = 0;
	if (pOptions->bDontUseNativeDialogs) {
		options |= QFileDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}
#if 1//QT_VERSION < 0x040400
	// Ask for the filename to save...
	sFilename = QFileDialog::getSaveFileName(pParentWidget,
		sTitle, sFilename, sFilter, NULL, options);
#else
	// Construct save-file dialog...
	QFileDialog fileDialog(pParentWidget,
		sTitle, sFilename, sFilter);
	// Set proper open-file modes...
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix(sExt);
	// Stuff sidebar...
	QList<QUrl> urls(fileDialog.sidebarUrls());
	urls.append(QUrl::fromLocalFile(pOptions->sSessionDir));
	urls.append(QUrl::fromLocalFile(pOptions->sPluginsDir));
	fileDialog.setSidebarUrls(urls);
	fileDialog.setOptions(options);
	// Show dialog...
	if (fileDialog.exec())
		sFilename = fileDialog.selectedFiles().first();
	else
		sFilename.clear();
#endif

	// Do we have any?
	if (sFilename.isEmpty())
		return;

	// Save chosen directory as default...
	pOptions->sPluginsDir = QFileInfo(sFilename).absolutePath();

#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginListView::exportPlugins(\"%s\")",
		sFilename.toUtf8().constData());
#endif

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Export plugin-list state to XML document...
	//
	QDomDocument doc("qtractorPluginList");
	qtractorPluginList::Document(&doc, m_pPluginList).save(sFilename);

	// We're formerly done.
	QApplication::restoreOverrideCursor();
}


// Add an audio-insert pseudo-plugin slot.
void qtractorPluginListView::addAudioInsertPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Create our special pseudo-plugin type...
	qtractorPlugin *pPlugin
		= qtractorInsertPluginType::createPlugin(
			m_pPluginList, m_pPluginList->channels());

	if (pPlugin) {
		// Make it a undoable command...
		pSession->execute(new qtractorAddInsertPluginCommand(pPlugin));
		// Show the plugin form right away...
		pPlugin->openForm();
	}

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	emit contentsChanged();
}


// Add an audio-insert pseudo-plugin slot.
void qtractorPluginListView::addAudioAuxSendPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Create our special pseudo-plugin type...
	qtractorPlugin *pPlugin
		= qtractorAuxSendPluginType::createPlugin(
			m_pPluginList, m_pPluginList->channels());

	if (pPlugin) {
		// Make it a undoable command...
		pSession->execute(new qtractorAddAuxSendPluginCommand(pPlugin));
		// Show the plugin form right away...
		pPlugin->openForm();
	}

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	emit contentsChanged();
}


// Add a MIDI-insert pseudo-plugin slot.
void qtractorPluginListView::addMidiInsertPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Create our special pseudo-plugin type...
	qtractorPlugin *pPlugin
		= qtractorInsertPluginType::createPlugin(m_pPluginList, 0); // MIDI!

	if (pPlugin) {
		// Make it a undoable command...
		pSession->execute(new qtractorAddInsertPluginCommand(pPlugin));
		// Show the plugin form right away...
		pPlugin->openForm();
	}

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	emit contentsChanged();
}


// Add a MIDI-insert pseudo-plugin slot.
void qtractorPluginListView::addMidiAuxSendPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Tell the world we'll take some time...
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	// Create our special pseudo-plugin type...
	qtractorPlugin *pPlugin
		= qtractorAuxSendPluginType::createPlugin(m_pPluginList, 0); // MIDI!

	if (pPlugin) {
		// Make it a undoable command...
		pSession->execute(new qtractorAddAuxSendPluginCommand(pPlugin));
		// Show the plugin form right away...
		pPlugin->openForm();
	}

	// We're formerly done.
	QApplication::restoreOverrideCursor();

	emit contentsChanged();
}


// Send/return insert specific slots.
void qtractorPluginListView::insertPluginOutputs (void)
{
	insertPluginBus(qtractorBus::Output);
}

void qtractorPluginListView::insertPluginInputs (void)
{
	insertPluginBus(qtractorBus::Input);
}


// Show insert pseudo-plugin audio bus connections.
void qtractorPluginListView::insertPluginBus ( int iBusMode )
{
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem == NULL)
		return;

	insertPluginBus(pItem->plugin(), iBusMode);
}


// Show insert pseudo-plugin audio bus connections (static)
void qtractorPluginListView::insertPluginBus (
	qtractorPlugin *pPlugin, int iBusMode )
{
	if (pPlugin == NULL)
		return;

	qtractorPluginType *pType = pPlugin->type();
	if (pType->typeHint() == qtractorPluginType::Insert) {
		// Might be either an audio or MIDI insert pseudo-plugin...
		qtractorBus *pInsertPluginBus = NULL;
		if (pType->index() > 0) {
			qtractorAudioInsertPlugin *pAudioInsertPlugin
				= static_cast<qtractorAudioInsertPlugin *> (pPlugin);
			if (pAudioInsertPlugin)
				pInsertPluginBus = pAudioInsertPlugin->audioBus();
		} else {
			qtractorMidiInsertPlugin *pMidiInsertPlugin
				= static_cast<qtractorMidiInsertPlugin *> (pPlugin);
			if (pMidiInsertPlugin)
				pInsertPluginBus = pMidiInsertPlugin->midiBus();
		}
		// Show insert bus connections...
		if (pInsertPluginBus) {
			qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
			if (pMainForm && pMainForm->connections()) {
				(pMainForm->connections())->showBus(
					pInsertPluginBus, qtractorBus::BusMode(iBusMode));
			}
		}
	}
}


// Audio specific slots.
void qtractorPluginListView::audioOutputs (void)
{
	qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager && pMidiManager->audioOutputBus()) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm && pMainForm->connections()) {
			(pMainForm->connections())->showBus(
				pMidiManager->audioOutputBus(), qtractorBus::Output);
		}
	}
}


void qtractorPluginListView::audioOutputBus (void)
{
	qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager == NULL)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorAudioOutputBusCommand(pMidiManager,
			!pMidiManager->isAudioOutputBus(), // Toggle!
			pMidiManager->isAudioOutputAutoConnect(),
			pMidiManager->audioOutputBusName()));

	emit contentsChanged();
}


void qtractorPluginListView::audioOutputBusName (void)
{
	QAction *pAction = qobject_cast<QAction *> (sender());
	if (pAction == NULL)
		return;

	const QString sAudioOutputBusName
		= pAction->text().remove('&');
	if (sAudioOutputBusName.isEmpty())
		return;

	qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager == NULL)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorAudioOutputBusCommand(pMidiManager,
			false, // Turn dedicated audio bus off!
			pMidiManager->isAudioOutputAutoConnect(),
			sAudioOutputBusName));

	emit contentsChanged();
}


void qtractorPluginListView::audioOutputAutoConnect (void)
{
	qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager == NULL)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorAudioOutputBusCommand(pMidiManager,
			pMidiManager->isAudioOutputBus(),
			!pMidiManager->isAudioOutputAutoConnect(), // Toggle!
			pMidiManager->audioOutputBusName()));

	emit contentsChanged();
}


void qtractorPluginListView::audioOutputMonitor (void)
{
	qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
	if (pMidiManager == NULL)
		return;

	// Make it an undoable command...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	pSession->execute(
		new qtractorAudioOutputMonitorCommand(pMidiManager,
			!pMidiManager->isAudioOutputMonitor()));

	emit contentsChanged();
}


// Show an existing plugin form slot.
void qtractorPluginListView::itemDoubleClickedSlot ( QListWidgetItem *item )
{
	itemActivatedSlot(item);
}

void qtractorPluginListView::itemActivatedSlot ( QListWidgetItem *item )
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (item);
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	bool bOpenEditor = false;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		bOpenEditor = pOptions->bOpenEditor;

	if (QApplication::keyboardModifiers()
		& (Qt::ShiftModifier | Qt::ControlModifier))
		bOpenEditor = !bOpenEditor;

	if (bOpenEditor && (pPlugin->type())->isEditor())
		pPlugin->openEditor();
	else
		pPlugin->openForm();
}


// Trap for help/tool-tip events.
bool qtractorPluginListView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	QWidget *pViewport = QListWidget::viewport();
	if (static_cast<QWidget *> (pObject) == pViewport) {
		if (pEvent->type() == QEvent::ToolTip) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				qtractorPluginListItem *pItem
					= static_cast<qtractorPluginListItem *> (
						QListWidget::itemAt(pHelpEvent->pos()));
				qtractorPlugin *pPlugin = NULL;
				if (pItem)
					pPlugin = pItem->plugin();
				if (pPlugin) {
					QString sToolTip = pItem->text(); // (pPlugin->type())->name();
					if (pPlugin->isDirectAccessParam()) {
						qtractorPluginParam *pDirectAccessParam
							= pPlugin->directAccessParam();
						if (pDirectAccessParam) {
							sToolTip.append(QString("\n(%1: %2)")
								.arg(pDirectAccessParam->name())
								.arg(pDirectAccessParam->display()));
						}
					}
					QToolTip::showText(pHelpEvent->globalPos(),
						sToolTip, pViewport);
					return true;
				}
			}
		}
		else
		if (pEvent->type() == QEvent::Leave) {
			m_dragCursor = DragNone;
			QListWidget::unsetCursor();
			return true;
		}
	}

	// Not handled here.
	return QListWidget::eventFilter(pObject, pEvent);
}


// trap the wheel event to change the value of the direcgAccessParameter
void qtractorPluginListView::wheelEvent ( QWheelEvent *pWheelEvent )
{
	const QPoint& pos = pWheelEvent->pos();
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::itemAt(pos));
	if (pItem) {
		qtractorPlugin *pPlugin = pItem->plugin();
		qtractorPluginParam *pDirectAccessParam = NULL;
		qtractorMidiControlObserver *pDirectAccessObserver = NULL;
		if (pPlugin)
			pDirectAccessParam = pPlugin->directAccessParam();
		if (pDirectAccessParam)
			pDirectAccessObserver = pDirectAccessParam->observer();
		if (pDirectAccessObserver) {
			const bool bLogarithmic = pDirectAccessParam->isLogarithmic();
			float fValue = pDirectAccessObserver->value();
			const float fScale = pDirectAccessObserver->scaleFromValue(
				fValue, bLogarithmic);
			float fDelta = (pWheelEvent->delta() < 0 ? -0.1f : +0.1f);
			if (!pDirectAccessParam->isInteger())
				fDelta *= 0.5f;
			fValue = pDirectAccessObserver->valueFromScale(
				fScale + fDelta, bLogarithmic);
			pDirectAccessParam->updateValue(fValue, true);
			return;
		}
	}

	// Not handled here.
	QListWidget::wheelEvent(pWheelEvent);
}


// To get precize clicking for in-place (de)activation;
// handle mouse events for drag-and-drop stuff.
void qtractorPluginListView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	dragLeaveEvent(NULL);

	const QPoint& pos = pMouseEvent->pos();
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::itemAt(pos));

	if (pItem && pos.x() > 0 && pos.x() < QListWidget::iconSize().width())
		m_pClickedItem = pItem;

	if (pMouseEvent->button() == Qt::LeftButton) {
		m_posDrag   = pos;
		m_pDragItem = pItem;
		dragDirectAccess(pos);
	}
	else // Reset to default value...
	if (pMouseEvent->button() == Qt::MidButton) {
		resetDirectAccess(pos);
	}

	QListWidget::mousePressEvent(pMouseEvent);
}


void qtractorPluginListView::mouseMoveEvent ( QMouseEvent *pMouseEvent )
{
	QListWidget::mouseMoveEvent(pMouseEvent);

	const QPoint& pos = pMouseEvent->pos();
	if ((pMouseEvent->buttons() & Qt::LeftButton) && m_pDragItem) {
		if (m_dragCursor != DragNone)
			m_dragState = m_dragCursor;
		if (m_dragState != DragNone) {
			dragDirectAccess(pos);
			return;
		}
		if ((pos - m_posDrag).manhattanLength()
			>= QApplication::startDragDistance()) {
			// We'll start dragging something alright...
			QMimeData *pMimeData = new QMimeData();
			encodeItem(pMimeData, m_pDragItem);
			QDrag *pDrag = new QDrag(this);
			pDrag->setMimeData(pMimeData);
			pDrag->setPixmap(m_pDragItem->icon().pixmap(16));
			pDrag->setHotSpot(QPoint(-4, -12));
			pDrag->exec(Qt::CopyAction | Qt::MoveAction);
			// We've dragged and maybe dropped it by now...
			m_pDragItem = NULL;
		}
	}
	else
	if (m_dragState == DragNone)
		dragDirectAccess(pos);
}


void qtractorPluginListView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QListWidget::mouseReleaseEvent(pMouseEvent);
	
	const QPoint& pos = pMouseEvent->pos();
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::itemAt(pos));
	if (pItem && pos.x() > 0 && pos.x() < QListWidget::iconSize().width()
		&& m_pClickedItem == pItem) {
		qtractorPlugin *pPlugin = pItem->plugin();
		if (pPlugin) {
			// Make it a undoable command...
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				pSession->execute(
					new qtractorActivatePluginCommand(pPlugin,
						!pPlugin->isActivated()));
				emit contentsChanged();
			}
		}
	}

	dragLeaveEvent(NULL);
}


// Drag-n-drop stuff;
// track on the current drop item position.
bool qtractorPluginListView::canDropEvent ( QDropEvent *pDropEvent )
{
	if (m_pPluginList == NULL)
		return false;

	if (!canDecodeItem(pDropEvent->mimeData()))
		return false;

	if (m_pDragItem == NULL)
		m_pDragItem	= decodeItem(pDropEvent->mimeData());
	if (m_pDragItem == NULL)
		return false;

	qtractorPluginListItem *pDropItem
		= static_cast<qtractorPluginListItem *> (
			QListWidget::itemAt(pDropEvent->pos()));
	if (pDropItem && pDropItem != m_pDropItem)
		ensureVisibleItem(pDropItem);

	// Cannot drop onto itself or over the one below...
	qtractorPlugin *pPlugin = m_pDragItem->plugin();
	if (pPlugin == NULL)
		return false;
	if (pDropItem) {
		if (pDropItem->plugin() == pPlugin
			|| (pDropItem->plugin())->prev() == pPlugin)
			return false;
	} else if (m_pPluginList->last() == pPlugin)
		return false;

	// All that to check whether it will get properly instantiated.
	const unsigned short iChannels = m_pPluginList->channels();
	const bool bMidi = m_pPluginList->isMidi();
	if ((pPlugin->type())->instances(iChannels, bMidi) < 1)
		return false;

	// This is the place we'll drop something
	// (append if null)
	m_pDropItem = pDropItem;

	return true;
}

bool qtractorPluginListView::canDropItem (  QDropEvent *pDropEvent  )
{
	const bool bCanDropItem = canDropEvent(pDropEvent);
	
	if (bCanDropItem)
		moveRubberBand(m_pDropItem);
	else 
	if (m_pRubberBand && m_pRubberBand->isVisible())
		m_pRubberBand->hide();

	return bCanDropItem;
}


// Ensure given item is brought to viewport visibility...
void qtractorPluginListView::ensureVisibleItem ( qtractorPluginListItem *pItem )
{
	const int iItem = QListWidget::row(pItem);
	if (iItem > 0) {
		qtractorPluginListItem *pItemAbove
			= static_cast<qtractorPluginListItem *> (
				QListWidget::item(iItem - 1));
		if (pItemAbove)
			QListWidget::scrollToItem(pItemAbove);
	}

	QListWidget::scrollToItem(pItem);
}


void qtractorPluginListView::dragEnterEvent ( QDragEnterEvent *pDragEnterEvent )
{
#if 0
	if (canDropItem(pDragEnterEvent)) {
		if (!pDragEnterEvent->isAccepted()) {
			pDragEnterEvent->setDropAction(Qt::MoveAction);
			pDragEnterEvent->accept();
		}
	} else {
		pDragEnterEvent->ignore();
	}
#else
	// Always accept the drag-enter event,
	// so let we deal with it during move later...
	pDragEnterEvent->accept();
#endif
}


void qtractorPluginListView::dragMoveEvent ( QDragMoveEvent *pDragMoveEvent )
{
	if (canDropItem(pDragMoveEvent)) {
		if (!pDragMoveEvent->isAccepted()) {
			pDragMoveEvent->setDropAction(Qt::MoveAction);
			pDragMoveEvent->accept();
		}
	} else {
		pDragMoveEvent->ignore();
	}
}


void qtractorPluginListView::dragLeaveEvent ( QDragLeaveEvent * )
{
	if (m_pRubberBand) {
		delete m_pRubberBand;
		m_pRubberBand = NULL;
	}

	// Should fallback mouse cursor...
	if (m_dragCursor != DragNone)
		QListWidget::unsetCursor();

	m_dragCursor = DragNone;
	m_dragState  = DragNone;

	m_pClickedItem = NULL;

	m_pDragItem = NULL;
	m_pDropItem = NULL;

}


void qtractorPluginListView::dropEvent ( QDropEvent *pDropEvent )
{
	if (!canDropItem(pDropEvent)) {
		dragLeaveEvent(NULL);
		return;
	}

	// If we aren't in the same list,
	// or care for keyboard modifiers...
	if (((m_pDragItem->plugin())->list() == m_pPluginList) ||
		(pDropEvent->keyboardModifiers() & Qt::ShiftModifier)) {
		dropMove();
	}
	else
	if (pDropEvent->keyboardModifiers() & Qt::ControlModifier) {
		dropCopy();
	} else {
		// We'll better have a drop menu...
		QMenu menu(this);
		menu.addAction(tr("&Move Here"), this, SLOT(dropMove()));
		menu.addAction(tr("&Copy Here"), this, SLOT(dropCopy()));
		menu.addSeparator();
		menu.addAction(QIcon(":/images/formReject.png"),
			tr("C&ancel"), this, SLOT(dropCancel()));
		menu.exec(QListWidget::mapToGlobal(pDropEvent->pos()));
	}

	dragLeaveEvent(NULL);
}


// Drop item slots.
void qtractorPluginListView::dropMove (void)
{
	moveItem(m_pDragItem, m_pDropItem);
	QListWidget::setFocus();
}

void qtractorPluginListView::dropCopy (void)
{
	copyItem(m_pDragItem, m_pDropItem);
	QListWidget::setFocus();
}

void qtractorPluginListView::dropCancel (void)
{
	// Do nothing...
}


// Draw a dragging separator line.
void qtractorPluginListView::moveRubberBand ( qtractorPluginListItem *pDropItem )
{
	// Create the rubber-band if there's none...
	if (m_pRubberBand == NULL) {
		m_pRubberBand = new qtractorRubberBand(
			QRubberBand::Line, QListWidget::viewport());
	#if 0
		QPalette pal(m_pRubberBand->palette());
		pal.setColor(m_pRubberBand->foregroundRole(), pal.highlight().color());
		m_pRubberBand->setPalette(pal);
		m_pRubberBand->setBackgroundRole(QPalette::NoRole);
	#endif
	}

	// Just move it...
	QRect rect;
	if (pDropItem) {
		rect = QListWidget::visualItemRect(pDropItem);
		rect.setTop(rect.top() - 1);
	} else {
		pDropItem = static_cast<qtractorPluginListItem *> (
			QListWidget::item(QListWidget::count() - 1));
		if (pDropItem) {
			rect = QListWidget::visualItemRect(pDropItem);
			rect.setTop(rect.bottom() - 1);
		} else {
			rect = QListWidget::viewport()->rect();
		}
	}

	// Set always this height:
	rect.setHeight(3);

	m_pRubberBand->setGeometry(rect);

	// Ah, and make it visible, of course...
	if (!m_pRubberBand->isVisible())
		m_pRubberBand->show();
}


// Context menu event handler.
void qtractorPluginListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	if (m_pPluginList == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	QMenu menu(this);
	QAction *pAction;

	const int iItemCount = QListWidget::count();
	const bool bEnabled  = (iItemCount > 0);

	int iItem = -1;
	qtractorPlugin *pPlugin = NULL;
	qtractorPluginType *pType = NULL;
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem) {
		iItem = QListWidget::row(pItem);
		pPlugin = pItem->plugin();
		pType = pPlugin->type();
	}

	pAction = menu.addAction(
		QIcon(":/images/formCreate.png"),
		tr("&Add Plugin..."), this, SLOT(addPlugin()));
//	pAction->setEnabled(true);

	QMenu *pInsertsMenu = menu.addMenu(tr("I&nserts"));

	QMenu *pAudioInsertsMenu = pInsertsMenu->addMenu(
		QIcon(":/images/trackAudio.png"), tr("&Audio"));
	pAudioInsertsMenu->setEnabled(m_pPluginList->channels() > 0);
	pAction = pAudioInsertsMenu->addAction(
		QIcon(":/images/formAdd.png"),
		tr("Add &Insert"), this, SLOT(addAudioInsertPlugin()));
	pAction = pAudioInsertsMenu->addAction(
		QIcon(":/images/formAdd.png"),
		tr("Add &Aux Send"), this, SLOT(addAudioAuxSendPlugin()));
	pAction->setEnabled(
		m_pPluginList->flags() != qtractorPluginList::AudioOutBus);
	pAudioInsertsMenu->addSeparator();
	const bool bAudioInsertPlugin = (pType
		&& pType->typeHint() == qtractorPluginType::Insert
		&& pType->index() > 0);
	pAction = pAudioInsertsMenu->addAction(
		QIcon(":/images/itemAudioPortOut.png"),
		tr("&Sends"), this, SLOT(insertPluginOutputs()));
	pAction->setEnabled(bAudioInsertPlugin);
	pAction = pAudioInsertsMenu->addAction(
		QIcon(":/images/itemAudioPortIn.png"),
		tr("&Returns"), this, SLOT(insertPluginInputs()));
	pAction->setEnabled(bAudioInsertPlugin);

	QMenu *pMidiInsertsMenu = pInsertsMenu->addMenu(
		QIcon(":/images/trackMidi.png"), tr("&MIDI"));
	pMidiInsertsMenu->setEnabled(m_pPluginList->isMidi());
	pAction = pMidiInsertsMenu->addAction(
		QIcon(":/images/formAdd.png"),
		tr("Add &Insert"), this, SLOT(addMidiInsertPlugin()));
	pAction = pMidiInsertsMenu->addAction(
		QIcon(":/images/formAdd.png"),
		tr("Add &Aux Send"), this, SLOT(addMidiAuxSendPlugin()));
	pAction->setEnabled(
		m_pPluginList->flags() != qtractorPluginList::MidiOutBus);
	pMidiInsertsMenu->addSeparator();
	const bool bMidiInsertPlugin = (pType
		&& pType->typeHint() == qtractorPluginType::Insert
		&& pType->index() == 0);
	pAction = pMidiInsertsMenu->addAction(
		QIcon(":/images/itemMidiPortOut.png"),
		tr("&Sends"), this, SLOT(insertPluginOutputs()));
	pAction->setEnabled(bMidiInsertPlugin);
	pAction = pMidiInsertsMenu->addAction(
		QIcon(":/images/itemMidiPortIn.png"),
		tr("&Returns"), this, SLOT(insertPluginInputs()));
	pAction->setEnabled(bMidiInsertPlugin);
	menu.addSeparator();

	const bool bAutoDeactivated = m_pPluginList->isAutoDeactivated();
	pAction = menu.addAction(
		tr("Ac&tivate"), this, SLOT(activatePlugin()));
	pAction->setCheckable(true);
	pAction->setChecked(pPlugin && pPlugin->isActivated());
	pAction->setEnabled(pPlugin && !bAutoDeactivated);

	const bool bActivatedAll = m_pPluginList->isActivatedAll();
	pAction = menu.addAction(
		tr("Acti&vate All"), this, SLOT(activateAllPlugins()));
	pAction->setCheckable(true);
	pAction->setChecked(bActivatedAll);
	pAction->setEnabled(bEnabled && !bActivatedAll && !bAutoDeactivated);

	const bool bDeactivatedAll = !m_pPluginList->isActivated();
	pAction = menu.addAction(
		tr("Deactivate Al&l"), this, SLOT(deactivateAllPlugins()));
	pAction->setCheckable(true);
	pAction->setChecked(bDeactivatedAll);
	pAction->setEnabled(bEnabled && !bDeactivatedAll && !bAutoDeactivated);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/images/formRemove.png"),
		tr("&Remove"), this, SLOT(removePlugin()));
	pAction->setEnabled(pPlugin != NULL);

	pAction = menu.addAction(
		tr("Re&move All"), this, SLOT(removeAllPlugins()));
	pAction->setEnabled(bEnabled);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/images/formMoveUp.png"),
		tr("Move &Up"), this, SLOT(moveUpPlugin()));
	pAction->setEnabled(pItem && iItem > 0);

	pAction = menu.addAction(
		QIcon(":/images/formMoveDown.png"),
		tr("Move &Down"), this, SLOT(moveDownPlugin()));
	pAction->setEnabled(pItem && iItem < iItemCount - 1);

	menu.addSeparator();

	QMenu *pPresetMenu = menu.addMenu(tr("Pre&set"));
	if (pPlugin) {
		const QStringList& presets = pPlugin->presetList();
		QStringListIterator iter(presets);
		while (iter.hasNext()) {
			const QString& sPreset = iter.next();
			pAction = pPresetMenu->addAction(
				sPreset, this, SLOT(loadPresetPlugin()));
			pAction->setCheckable(true);
			pAction->setChecked(sPreset == pPlugin->preset());
		}
		if (presets.count() > 0)
			pPresetMenu->addSeparator();
		pAction = pPresetMenu->addAction(
			qtractorPlugin::defPreset(), this, SLOT(loadPresetPlugin()));
		pAction->setCheckable(false);
	}
	pPresetMenu->setEnabled(pPlugin != NULL);

	QMenu *pDirectAccessParamMenu = menu.addMenu(tr("Dire&ct Access"));
	if (pPlugin) {
		const int iDirectAccessParamIndex = pPlugin->directAccessParamIndex();
		const qtractorPlugin::Params& params = pPlugin->params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			const int iParamIndex = int(param.key());
			pAction = pDirectAccessParamMenu->addAction(
				pParam->name(), this, SLOT(directAccessPlugin()));
			pAction->setCheckable(true);
			pAction->setChecked(iDirectAccessParamIndex == iParamIndex);
			pAction->setData(iParamIndex);
		}
		const bool bParams = (params.count() > 0);
		if (bParams) {
			pDirectAccessParamMenu->addSeparator();
			pAction = pDirectAccessParamMenu->addAction(
				tr("&None"), this, SLOT(directAccessPlugin()));
			pAction->setCheckable(true);
			pAction->setChecked(iDirectAccessParamIndex < 0);
			pAction->setData(int(-1));
		}
		pDirectAccessParamMenu->setEnabled(bParams);
	}
	else pDirectAccessParamMenu->setEnabled(false);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/images/trackProperties.png"),
		tr("&Properties..."), this, SLOT(propertiesPlugin()));
	pAction->setCheckable(true);
	pAction->setChecked(pPlugin && pPlugin->isFormVisible());
	pAction->setEnabled(pItem != NULL);

	pAction = menu.addAction(
		QIcon(":/images/formEdit.png"),
		tr("&Edit"), this, SLOT(editPlugin()));
	pAction->setCheckable(true);
	pAction->setChecked(pPlugin && pPlugin->isEditorVisible());
	pAction->setEnabled(pType && pType->isEditor());

	menu.addSeparator();

	pAction = menu.addAction(
		tr("&Import..."), this, SLOT(importPlugins()));
//	pAction->setEnabled(true);

	pAction = menu.addAction(
		tr("E&xport..."), this, SLOT(exportPlugins()));
	pAction->setEnabled(bEnabled);

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	qtractorMidiManager *pMidiManager = m_pPluginList->midiManager();
	if (pAudioEngine && pMidiManager) {
		menu.addSeparator();
		const QIcon& iconAudio = QIcon(":/images/trackAudio.png");
		const bool bAudioOutputBus = pMidiManager->isAudioOutputBus();
		qtractorAudioBus *pAudioOutputBus = pMidiManager->audioOutputBus();
		QMenu *pAudioMenu = menu.addMenu(iconAudio, "Audi&o");
		pAction = pAudioMenu->addAction(
			tr("&Outputs"), this, SLOT(audioOutputs()));
		pAction->setEnabled(pAudioOutputBus != NULL);
		pAudioMenu->addSeparator();
		for (qtractorBus *pBus = pAudioEngine->buses().first();
				pBus; pBus = pBus->next()) {
			if (pBus->busMode() & qtractorBus::Output) {
				const QString& sBusName = pBus->busName();
				pAction = pAudioMenu->addAction(iconAudio,
					sBusName, this, SLOT(audioOutputBusName()));
				pAction->setCheckable(true);
				pAction->setChecked(pAudioOutputBus != NULL
					&& sBusName == pAudioOutputBus->busName());
				pAction->setEnabled(!bAudioOutputBus);
			}
		}
		pAudioMenu->addSeparator();
		pAction = pAudioMenu->addAction(
			tr("&Dedicated"), this, SLOT(audioOutputBus()));
		pAction->setCheckable(true);
		pAction->setChecked(bAudioOutputBus);
		pAction = pAudioMenu->addAction(
			tr("&Auto-connect"), this, SLOT(audioOutputAutoConnect()));
		pAction->setCheckable(true);
		pAction->setChecked(pMidiManager->isAudioOutputAutoConnect());
		pAction->setEnabled(bAudioOutputBus);
		pAudioMenu->addSeparator();
		pAction = pAudioMenu->addAction(
			tr("&Meters"), this, SLOT(audioOutputMonitor()));
		pAction->setCheckable(true);
		pAction->setChecked(pMidiManager->isAudioOutputMonitor());
	}

	menu.exec(pContextMenuEvent->globalPos());
}


// Common pixmap accessors.
QIcon *qtractorPluginListView::itemIcon ( int iIndex )
{
	return g_pItemIcons[iIndex];
}


static const char *c_pszPluginListItemMimeType = "qtractor/plugin-item";

// Encoder method (static).
void qtractorPluginListView::encodeItem ( QMimeData *pMimeData,
	qtractorPluginListItem *pItem )
{
	// Allocate the data array...
	pMimeData->setData(c_pszPluginListItemMimeType,
		QByteArray((const char *) &pItem, sizeof(qtractorPluginListItem *)));
}

// Decode trial method (static).
bool qtractorPluginListView::canDecodeItem ( const QMimeData *pMimeData )
{
	return pMimeData->hasFormat(c_pszPluginListItemMimeType);
}

// Decode method (static).
qtractorPluginListItem *qtractorPluginListView::decodeItem (
	const QMimeData *pMimeData )
{
	qtractorPluginListItem *pItem = NULL;

	QByteArray data = pMimeData->data(c_pszPluginListItemMimeType);
	if (data.size() == sizeof(qtractorPluginListItem *))
		::memcpy(&pItem, data.constData(), sizeof(qtractorPluginListItem *));

	return pItem;
}


// Direct access parameter handle.
void qtractorPluginListView::dragDirectAccess ( const QPoint& pos )
{
	qtractorPluginListItem *pItem = m_pDragItem;
	if (pItem == NULL)
		pItem = static_cast<qtractorPluginListItem *> (QListWidget::itemAt(pos));
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	qtractorPluginParam *pDirectAccessParam	
		= pPlugin->directAccessParam();
	if (pDirectAccessParam == NULL)
		return;

	qtractorMidiControlObserver *pDirectAccessObserver
		= pDirectAccessParam->observer();
	if (pDirectAccessObserver == NULL)
		return;

	const bool bLogarithmic = pDirectAccessParam->isLogarithmic();
	const int iDirectAccessWidth = pItem->directAccessWidth();
	const QRect& rectItem = QListWidget::visualItemRect(pItem)
		.adjusted(QListWidget::iconSize().width(), 1, -2, -2);

	if (m_dragState == DragNone) {
		const float fValue
			= pDirectAccessParam->value();
		const float fScale
			= pDirectAccessObserver->scaleFromValue(fValue, bLogarithmic);
		const int x = rectItem.x() + int(fScale * float(iDirectAccessWidth));
		if (pos.x() > x - 5 && pos.x() < x + 5) {
			m_dragCursor = DragDirectAccess;
			QListWidget::setCursor(QCursor(Qt::PointingHandCursor));
		} else {
			QListWidget::unsetCursor();
			m_dragCursor = DragNone;
		}
	}
	else
	if (m_dragState == DragDirectAccess) {
		const float fScale
			= float(pos.x() - rectItem.x()) / float(iDirectAccessWidth);
		const float fValue
			= pDirectAccessObserver->valueFromScale(fScale, bLogarithmic);
		pDirectAccessParam->updateValue(fValue, true);
		QWidget *pViewport = QListWidget::viewport();
		QToolTip::showText(pViewport->mapToGlobal(pos),
			QString("%1\n(%2: %3)")
				.arg(pItem->text()) // (pType->name();
				.arg(pDirectAccessParam->name())
				.arg(pDirectAccessParam->display()), pViewport);
	}
}



// Direct access parameter reset (to default value).
void qtractorPluginListView::resetDirectAccess ( const QPoint& pos )
{
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::itemAt(pos));
	if (pItem == NULL)
		return;

	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	qtractorPluginParam *pDirectAccessParam
		= pPlugin->directAccessParam();
	if (pDirectAccessParam == NULL)
		return;

	qtractorMidiControlObserver *pDirectAccessObserver
		= pDirectAccessParam->observer();
	if (pDirectAccessObserver == NULL)
		return;

	const float fDefaultValue
		= pDirectAccessObserver->defaultValue();
	pDirectAccessParam->updateValue(fDefaultValue, true);
}


// Initial size-policy hints.
QSize qtractorPluginListView::sizeHint (void) const
{
	const QFont& font = QListWidget::font();
	const int iItemHeight = QFontMetrics(font).lineSpacing() + 4;
	return QSize(180, iItemHeight << 2);
}


// end of qtractorPluginListView.cpp
