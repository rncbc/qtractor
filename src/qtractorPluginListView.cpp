// qtractorPluginListView.cpp
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

#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"

#include "qtractorPluginSelectForm.h"
#include "qtractorPluginForm.h"

#include "qtractorMainForm.h"

#include "qtractorSlider.h"

#include <QItemDelegate>
#include <QPainter>
#include <QIcon>
#include <QMenu>
#include <QToolTip>

#include <QScrollBar>
#include <QLabel>
#include <QCheckBox>
#include <QGridLayout>
#include <QDoubleSpinBox>

#include <QContextMenuEvent>

#include <math.h>


//----------------------------------------------------------------------------
// qtractorTinyScrollBarStyle -- Custom style to have some tiny scrollbars
//
#include <QCDEStyle>

class qtractorTinyScrollBarStyle : public QCDEStyle
{
public:

	// Custom virtual override.
	int pixelMetric( PixelMetric pm,
		const QStyleOption *option = 0,	const QWidget *pWidget = 0 ) const
	{
		if (pm == QStyle::PM_ScrollBarExtent)
			return 8;

		return QCDEStyle::pixelMetric(pm, option, pWidget);
	}
};

static qtractorTinyScrollBarStyle g_tinyScrollBarStyle;



//----------------------------------------------------------------------------
// qtractorPluginListItemDelegate -- Plugin list view item delgate.

class qtractorPluginListItemDelegate : public QItemDelegate
{
public:

	// Constructor.
	qtractorPluginListItemDelegate(QListWidget *pListWidget)
		: QItemDelegate(pListWidget), m_pListWidget(pListWidget) {}

	// Overridden paint method.
	void paint(QPainter *pPainter, const QStyleOptionViewItem& option,
		const QModelIndex& index) const
	{
		// Unselectable items get special grayed out painting...
		QListWidgetItem *pItem = m_pListWidget->item(index.row());
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
			// Draw the icon...
			QRect rect = option.rect;
			const QSize& iconSize = m_pListWidget->iconSize();
			pPainter->drawPixmap(1, rect.top() + 1,
				pItem->icon().pixmap(iconSize));
			// Draw the text...
			rect.setLeft(iconSize.width());
			pPainter->setPen(rgbFore);
			pPainter->drawText(rect,
				Qt::AlignLeft | Qt::AlignVCenter, pItem->text());
			// Draw frame lines...
			pPainter->setPen(rgbBack.light(150));
			pPainter->drawLine(
				option.rect.left(),  option.rect.top(),
				option.rect.right(), option.rect.top());
			pPainter->setPen(rgbBack.dark(150));
			pPainter->drawLine(
				option.rect.left(),  option.rect.bottom(),
				option.rect.right(), option.rect.bottom());
			pPainter->restore();
		//	if (option.state & QStyle::State_HasFocus)
		//		QItemDelegate::drawFocus(pPainter, option, option.rect);
		} else {
			// Others do as default...
			QItemDelegate::paint(pPainter, option, index);
		}
	}

private:

	QListWidget *m_pListWidget;
};


//----------------------------------------------------------------------------
// qtractorPluginListItem -- Plugins list item.

// Constructors.
qtractorPluginListItem::qtractorPluginListItem ( qtractorPlugin *pPlugin )
	: QListWidgetItem()
{
	m_pPlugin = pPlugin;

	m_pPlugin->items().append(this);

	QListWidgetItem::setText(m_pPlugin->name());

	updateActivated();
}

// Destructor.
qtractorPluginListItem::~qtractorPluginListItem (void)
{
	int iItem = m_pPlugin->items().indexOf(this);
	if (iItem >= 0)
		m_pPlugin->items().removeAt(iItem);
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
		g_pItemIcons[0] = new QIcon(":/icons/itemLedOff.png");
		g_pItemIcons[1] = new QIcon(":/icons/itemLedOn.png");
	}

	QListWidget::setFont(QFont(QListWidget::font().family(), 6));
	QListWidget::verticalScrollBar()->setStyle(&g_tinyScrollBarStyle);
	QListWidget::horizontalScrollBar()->setStyle(&g_tinyScrollBarStyle);

	QListWidget::setIconSize(QSize(10, 10));
	QListWidget::setItemDelegate(new qtractorPluginListItemDelegate(this));
	QListWidget::setSelectionMode(QAbstractItemView::SingleSelection);

	QListWidget::viewport()->setBackgroundRole(QPalette::Window);

	QListWidget::setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//	QListWidget::setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// Trap for help/tool-tips events.
	QListWidget::viewport()->installEventFilter(this);

	// Double-click handling...
	QObject::connect(this,
		SIGNAL(itemDoubleClicked(QListWidgetItem*)),
		SLOT(itemDoubleClickedSlot(QListWidgetItem*)));
	QObject::connect(this,
		SIGNAL(itemActivated(QListWidgetItem*)),
		SLOT(itemActivatedSlot(QListWidgetItem*)));
}


// Destructor.
qtractorPluginListView::~qtractorPluginListView (void)
{
	// No need to delete child widgets, Qt does it all for us
	setPluginList(NULL);

	if (--g_iItemRefCount == 0) {
		for (int i = 0; i < 2; i++) {
			delete g_pItemIcons[i];
			g_pItemIcons[i] = NULL;
		}
	}
}


// Plugin list accessors.
void qtractorPluginListView::setPluginList ( qtractorPluginList *pPluginList )
{
	if (m_pPluginList) {
		int iView = m_pPluginList->views().indexOf(this);
		if (iView >= 0)
			m_pPluginList->views().removeAt(iView);
	}

	m_pPluginList = pPluginList;

	if (m_pPluginList) {
		m_pPluginList->setAutoDelete(true);
		m_pPluginList->views().append(this);
	}

	refresh();
}

qtractorPluginList *qtractorPluginListView::pluginList (void) const
{
	return m_pPluginList;
}

// Plugin list refreshner
void qtractorPluginListView::refresh (void)
{
	QListWidget::setUpdatesEnabled(false);
	QListWidget::clear();
	if (m_pPluginList) {
		for (qtractorPlugin *pPlugin = m_pPluginList->first();
				pPlugin; pPlugin = pPlugin->next()) {
			QListWidget::addItem(new qtractorPluginListItem(pPlugin));
		}
	}
	QListWidget::setUpdatesEnabled(true);
}

// Get an item index, given the plugin reference...
int qtractorPluginListView::pluginItem ( qtractorPlugin *pPlugin )
{
	if (pPlugin == NULL)
		return -1;

	int iItemCount = QListWidget::count();
	for (int iItem = 0; iItem < iItemCount; ++iItem) {
		QListWidgetItem *pItem = QListWidget::item(iItem);
		qtractorPluginListItem *pPluginItem
			= static_cast<qtractorPluginListItem *> (pItem);
		if (pPluginItem->plugin() == pPlugin)
			return iItem;
	}

	return -1;
}


// Move item on list.
void qtractorPluginListView::moveItem (
	qtractorPluginListItem *pItem, qtractorPluginListItem *pPrevItem ) 
{
	if (pItem == NULL)
		return;

	// The plugin to be moved...	
	qtractorPlugin *pPlugin = pItem->plugin();
	if (pPlugin == NULL)
		return;

	// To be after this one...
	qtractorPlugin *pPrevPlugin = NULL;
	if (pPrevItem)
		pPrevPlugin = pPrevItem->plugin();

	// Make it an undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorMovePluginCommand(pMainForm, pPlugin, pPrevPlugin));
}


// Add a new plugin slot.
void qtractorPluginListView::addPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginSelectForm selectForm(this);
	selectForm.setChannels(m_pPluginList->channels());
	if (!selectForm.exec())
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	// Make it a undoable command...
	qtractorAddPluginCommand *pAddPluginCommand
		= new qtractorAddPluginCommand(pMainForm);

	for (int i = 0; i < selectForm.pluginCount(); i++) {
		// Add an actual plugin item...
		qtractorPlugin *pPlugin
			= new qtractorPlugin(m_pPluginList,
				selectForm.pluginFilename(i),
				selectForm.pluginIndex(i));
		pAddPluginCommand->plugins().append(pPlugin);
	}

	pMainForm->commands()->exec(pAddPluginCommand);
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorRemovePluginCommand(pMainForm, pPlugin));
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorActivatePluginCommand(pMainForm,
				pPlugin, !pPlugin->isActivated()));
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorActivatePluginCommand *pActivateAllCommand
		= new qtractorActivatePluginCommand(pMainForm, NULL, true);
	pActivateAllCommand->setName(tr("activate all plugins"));

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (!pPlugin->isActivated())
			pActivateAllCommand->plugins().append(pPlugin);
	}

	pMainForm->commands()->exec(pActivateAllCommand);
}


// Dectivate all plugins.
void qtractorPluginListView::deactivateAllPlugins (void)
{
	if (m_pPluginList == NULL)
		return;

	// Check whether everyone is already dectivated...
	if (m_pPluginList->activated() < 1)
		return;

	// Make it an undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorActivatePluginCommand *pDeactivateAllCommand
		= new qtractorActivatePluginCommand(pMainForm, NULL, false);
	pDeactivateAllCommand->setName(tr("deactivate all plugins"));

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		if (pPlugin->isActivated())
			pDeactivateAllCommand->plugins().append(pPlugin);
	}

	pMainForm->commands()->exec(pDeactivateAllCommand);
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
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	qtractorRemovePluginCommand *pRemoveAllCommand
		= new qtractorRemovePluginCommand(pMainForm);
	pRemoveAllCommand->setName(tr("remove all plugins"));

	for (qtractorPlugin *pPlugin = m_pPluginList->first();
			pPlugin; pPlugin = pPlugin->next()) {
		pRemoveAllCommand->plugins().append(pPlugin);
	}

	pMainForm->commands()->exec(pRemoveAllCommand);
}


// Move an existing plugin upward slot.
void qtractorPluginListView::moveUpPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	QListWidgetItem *pItem = QListWidget::currentItem();
	if (pItem == NULL)
		return;

	int iPrevItem = QListWidget::row(pItem) - 1;
	if (iPrevItem < 0)
		return;

	QListWidgetItem *pPrevItem = QListWidget::item(iPrevItem);
	if (pPrevItem == NULL)
		return;

	moveItem(static_cast<qtractorPluginListItem *> (pItem),
		static_cast<qtractorPluginListItem *> (pPrevItem));
}


// Move an existing plugin downward slot.
void qtractorPluginListView::moveDownPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	QListWidgetItem *pItem = QListWidget::currentItem();
	if (pItem == NULL)
		return;

	int iPrevItem = QListWidget::row(pItem) + 1;
	if (iPrevItem >= QListWidget::count())
		return;

	QListWidgetItem *pPrevItem = QListWidget::item(iPrevItem);
	if (pPrevItem == NULL)
		return;

	moveItem(static_cast<qtractorPluginListItem *> (pItem),
		static_cast<qtractorPluginListItem *> (pPrevItem));
}


// Show/hide an existing plugin form slot.
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

	qtractorPluginForm *pPluginForm = pPlugin->form();
	if (pPluginForm->isVisible())
		pPluginForm->hide();
	else {
		pPluginForm->show();	
		pPluginForm->raise();
		pPluginForm->activateWindow();
	}
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

	qtractorPluginForm *pPluginForm = pPlugin->form();
	if (!pPluginForm->isVisible())
		pPluginForm->show();	
	pPluginForm->raise();
	pPluginForm->activateWindow();
}


// Trap for help/tool-tip events.
bool qtractorPluginListView::eventFilter ( QObject *pObject, QEvent *pEvent )
{
	if (static_cast<QWidget *> (pObject) == QListWidget::viewport()
		&& pEvent->type() == QEvent::ToolTip) {
		QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
		if (pHelpEvent) {
			QListWidgetItem *pItem = QListWidget::itemAt(pHelpEvent->pos());
			qtractorPluginListItem *pPluginItem
				= static_cast<qtractorPluginListItem *> (pItem);
			qtractorPlugin *pPlugin = NULL;
			if (pPluginItem)
				pPlugin = pPluginItem->plugin();
			if (pPlugin) {
				QToolTip::showText(
					pHelpEvent->globalPos(), pPlugin->name());
				return true;
			}
		}
	}

	// Not handled here.
	return QListWidget::eventFilter(pObject, pEvent);
}


// To get precize clicking for in-place (de)activation.
void qtractorPluginListView::mousePressEvent ( QMouseEvent *pMouseEvent )
{
	QListWidget::mousePressEvent(pMouseEvent);
	
	const QPoint& pos = pMouseEvent->pos();
	QListWidgetItem *pItem = QListWidget::itemAt(pos);
	if (pItem && pos.x() > 0 && pos.x() < QListWidget::iconSize().width())
		m_pClickedItem = pItem;
}

void qtractorPluginListView::mouseReleaseEvent ( QMouseEvent *pMouseEvent )
{
	QListWidget::mouseReleaseEvent(pMouseEvent);
	
	const QPoint& pos = pMouseEvent->pos();
	QListWidgetItem *pItem = QListWidget::itemAt(pos);
	if (pItem && pos.x() > 0 && pos.x() <= QListWidget::iconSize().width()
		&& pItem == m_pClickedItem) {
		qtractorPluginListItem *pPluginItem
			= static_cast<qtractorPluginListItem *> (pItem);
		if (pPluginItem) {
			qtractorPlugin *pPlugin = pPluginItem->plugin();
			if (pPlugin) {
				// Make it a undoable command...
				qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
				if (pMainForm) {
					pMainForm->commands()->exec(
						new qtractorActivatePluginCommand(pMainForm,
							pPlugin, !pPlugin->isActivated()));
				}
			}
		}
	}
	
	m_pClickedItem = NULL;
}


// Context menu event handler.
void qtractorPluginListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	if (m_pPluginList == NULL)
		return;

	QMenu menu(this);
	QAction *pAction;

	int iItem = -1;
	int iItemCount = QListWidget::count();
	bool bEnabled  = (iItemCount > 0);

	qtractorPlugin *pPlugin = NULL;
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListWidget::currentItem());
	if (pItem) {
		iItem = QListWidget::row(pItem);
		pPlugin = pItem->plugin();
	}

	pAction = menu.addAction(
		QIcon(":/icons/formCreate.png"),
		tr("Add Plugin..."), this, SLOT(addPlugin()));
//	pAction->setEnabled(true);

	pAction = menu.addAction(
		QIcon(":/icons/formRemove.png"),
		tr("Remove Plugin"), this, SLOT(removePlugin()));
	pAction->setEnabled(pPlugin != NULL);

	menu.addSeparator();

	pAction = menu.addAction(
		tr("Activate"), this, SLOT(activatePlugin()));
	pAction->setCheckable(true);
	pAction->setChecked(pPlugin && pPlugin->isActivated());
	pAction->setEnabled(pPlugin != NULL);

	menu.addSeparator();

	bool bActivatedAll = m_pPluginList->isActivatedAll();
	pAction = menu.addAction(
		tr("Activate All"), this, SLOT(activateAllPlugins()));
	pAction->setCheckable(true);
	pAction->setChecked(bActivatedAll);
	pAction->setEnabled(bEnabled && !bActivatedAll);

	bool bDeactivatedAll = (m_pPluginList->activated() < 1);
	pAction = menu.addAction(
		tr("Deactivate All"), this, SLOT(deactivateAllPlugins()));
	pAction->setCheckable(true);
	pAction->setChecked(bDeactivatedAll);
	pAction->setEnabled(bEnabled && !bDeactivatedAll);

	menu.addSeparator();

	pAction = menu.addAction(
		tr("Remove All"), this, SLOT(removeAllPlugins()));
	pAction->setEnabled(bEnabled);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/icons/formMoveUp.png"),
		tr("Move Up"), this, SLOT(moveUpPlugin()));
	pAction->setEnabled(pItem && iItem > 0);

	pAction = menu.addAction(
		QIcon(":/icons/formMoveDown.png"),
		tr("Move Down"), this, SLOT(moveDownPlugin()));
	pAction->setEnabled(pItem && iItem < iItemCount - 1);

	menu.addSeparator();

	pAction = menu.addAction(
		QIcon(":/icons/formEdit.png"),
		tr("Edit"), this, SLOT(editPlugin()));
	pAction->setCheckable(true);
	pAction->setChecked(pPlugin && pPlugin->isVisible());
	pAction->setEnabled(pItem != NULL);

	menu.exec(pContextMenuEvent->globalPos());
}


// Common pixmap accessors.
QIcon *qtractorPluginListView::itemIcon ( int iIndex )
{
	return g_pItemIcons[iIndex];
}


//----------------------------------------------------------------------------
// qtractorPluginPortWidget -- Plugin port widget.
//

// Constructor.
qtractorPluginPortWidget::qtractorPluginPortWidget (
	qtractorPluginPort *pPort, QWidget *pParent )
	: QFrame(pParent), m_pPort(pPort), m_iUpdate(0)
{
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(4);
	m_pGridLayout->setSpacing(4);

	m_pLabel      = NULL;
	m_pSlider     = NULL;
	m_pSpinBox    = NULL;
	m_pCheckBox   = NULL;

	const QString sColon = ":";
	if (m_pPort->isToggled()) {
		m_pCheckBox = new QCheckBox(this);
		m_pCheckBox->setText(m_pPort->name());
	//	m_pCheckBox->setChecked(m_pPort->value() > 0.1f);
		m_pGridLayout->addWidget(m_pCheckBox, 0, 0);
		QObject::connect(m_pCheckBox,
			SIGNAL(toggled(bool)),
			SLOT(checkBoxToggled(bool)));
	} else if (m_pPort->isInteger()) {
		m_pGridLayout->setColumnMinimumWidth(0, 120);
		m_pLabel = new QLabel(this);
		m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_pLabel->setText(m_pPort->name() + sColon);
		m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 2);
		m_pSpinBox = new QDoubleSpinBox(this);
		m_pSpinBox->setMaximumWidth(80);
		m_pSpinBox->setDecimals(0);
		m_pSpinBox->setMinimum(m_pPort->minValue());
		m_pSpinBox->setMaximum(m_pPort->maxValue());
		m_pSpinBox->setAlignment(Qt::AlignHCenter);
	//	m_pSpinBox->setValue(int(m_pPort->value()));
		m_pGridLayout->addWidget(m_pSpinBox, 0, 2);
		QObject::connect(m_pSpinBox,
			SIGNAL(valueChanged(const QString&)),
			SLOT(spinBoxValueChanged(const QString&)));
	} else {
		m_pLabel = new QLabel(this);
		m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
		m_pLabel->setText(m_pPort->name() + sColon);
		m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 3);
		m_pSlider = new qtractorSlider(Qt::Horizontal, this);
		m_pSlider->setTickPosition(QSlider::NoTicks);
		m_pSlider->setMinimumWidth(120);
		m_pSlider->setMinimum(0);
		m_pSlider->setMaximum(10000);
		m_pSlider->setPageStep(1000);
		m_pSlider->setSingleStep(100);
		m_pSlider->setDefault(portToSlider(m_pPort->defaultValue()));
	//	m_pSlider->setValue(portToSlider(m_pPort->value()));
		m_pGridLayout->addWidget(m_pSlider, 1, 0, 1, 2);
		m_pSpinBox = new QDoubleSpinBox(this);
		m_pSpinBox->setMaximumWidth(80);
		m_pSpinBox->setDecimals(portDecs());
		m_pSpinBox->setMinimum(m_pPort->minValue());
		m_pSpinBox->setMaximum(m_pPort->maxValue());
	//	m_pSpinBox->setValue(m_pPort->value());
		m_pGridLayout->addWidget(m_pSpinBox, 1, 2);
		QObject::connect(m_pSlider,
			SIGNAL(valueChanged(int)),
			SLOT(sliderValueChanged(int)));
		QObject::connect(m_pSpinBox,
			SIGNAL(valueChanged(const QString&)),
			SLOT(spinBoxValueChanged(const QString&)));
	}

	QFrame::setLayout(m_pGridLayout);
//	QFrame::setFrameShape(QFrame::StyledPanel);
//	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setToolTip(m_pPort->name());
}

// Destructor.
qtractorPluginPortWidget::~qtractorPluginPortWidget (void)
{
}


// Main properties accessors.
qtractorPluginPort *qtractorPluginPortWidget::port (void) const
{
	return m_pPort;
}


// Refreshner-loader method.
void qtractorPluginPortWidget::refresh (void)
{
	m_iUpdate++;
	if (m_pPort->isToggled()) {
		m_pCheckBox->setChecked(m_pPort->value() > 0.1f);
	} else if (m_pPort->isInteger()) {
		m_pSpinBox->setValue(int(m_pPort->value()));
	} else {
		m_pSlider->setValue(portToSlider(m_pPort->value()));
		m_pSpinBox->setValue(m_pPort->value());
	}
	m_iUpdate--;
}


// Slider conversion methods.
int qtractorPluginPortWidget::portToSlider ( float fValue ) const
{
	int   iValue = 0;
	float fScale = m_pPort->maxValue() - m_pPort->minValue();
	if (fScale > 1E-6f)
		iValue = int((10000.0f * (fValue - m_pPort->minValue())) / fScale);

	return iValue;
}

float qtractorPluginPortWidget::sliderToPort ( int iValue ) const
{
	float fDelta = m_pPort->maxValue() - m_pPort->minValue();
	return m_pPort->minValue() + (float(iValue) * fDelta) / 10000.0f;
}


// Spin-box decimals helper.
int qtractorPluginPortWidget::portDecs (void) const
{
	int   iDecs = 0;
	float fDecs = ::log10f(m_pPort->maxValue() - m_pPort->minValue());
	if (fDecs < 0.0f)
		iDecs = 3;
	else if (fDecs < 1.0f)
		iDecs = 2;
	else if (fDecs < 3.0f)
		iDecs = 1;

	return iDecs;

}

// Change slots.
void qtractorPluginPortWidget::checkBoxToggled ( bool bOn )
{
	float fValue = (bOn ? 1.0f : 0.0f);

//	m_pPort->setValue(fValue);
	emit valueChanged(m_pPort, fValue);
}

void qtractorPluginPortWidget::spinBoxValueChanged ( const QString& sText )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fValue = 0.0f;
	if (m_pPort->isInteger()) {
		fValue = float(sText.toInt());
	} else {
		fValue = sText.toFloat();
	}

	//	Don't let be no-changes...
	if (::fabsf(m_pPort->value() - fValue)
		> ::powf(10.0f, - float(portDecs()))) {
		if (m_pSlider)
			m_pSlider->setValue(portToSlider(fValue));
		emit valueChanged(m_pPort, fValue);
	}

	m_iUpdate--;
}

void qtractorPluginPortWidget::sliderValueChanged ( int iValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fValue = sliderToPort(iValue);
	//	Don't let be no-changes...
	if (::fabsf(m_pPort->value() - fValue)
		> ::powf(10.0f, - float(portDecs()))) {
		if (m_pSpinBox)
			m_pSpinBox->setValue(fValue);
		emit valueChanged(m_pPort, fValue);
	}

	m_iUpdate--;
}


// end of qtractorPluginListView.cpp
