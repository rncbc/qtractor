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

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#include "qtractorPluginListView.h"
#include "qtractorPluginCommand.h"

#include "qtractorPluginSelectForm.h"
#include "qtractorPluginForm.h"

#include "qtractorMainForm.h"

#include "qtractorSlider.h"
#include "qtractorSpinBox.h"

#include <qheader.h>
#include <qpainter.h>
#include <qpopupmenu.h>

#include <qlayout.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qtooltip.h>


//----------------------------------------------------------------------------
// qtractorTinyScrollBarStyle -- Custom style to have some tiny scrollbars
//
#include <qcdestyle.h>

class qtractorTinyScrollBarStyle : public QCDEStyle
{
public:

	// Custom virtual override.
	int pixelMetric( PixelMetric pm, const QWidget *pWidget = 0 ) const
	{
		if (pm == QStyle::PM_ScrollBarExtent)
			return 8;

		return QCDEStyle::pixelMetric(pm, pWidget);
	}
};

static qtractorTinyScrollBarStyle g_tinyScrollBarStyle;



//----------------------------------------------------------------------------
// qtractorPluginListItem -- Plugins list item.

// Constructors.
qtractorPluginListItem::qtractorPluginListItem (
	qtractorPluginListView *pListView, qtractorPlugin *pPlugin )
	: QListViewItem(pListView, pListView->lastItem())
{
	initItem(pPlugin);
}

qtractorPluginListItem::qtractorPluginListItem (
	qtractorPluginListView *pListView, qtractorPlugin *pPlugin,
	QListViewItem *pItemAfter )
	: QListViewItem(pListView, pItemAfter)
{
	initItem(pPlugin);
}


// Destructor.
qtractorPluginListItem::~qtractorPluginListItem (void)
{
	if (m_pPlugin)
		m_pPlugin->items().remove(this);
}


// Common item initializer.
void qtractorPluginListItem::initItem ( qtractorPlugin *pPlugin )
{
	m_pPlugin = pPlugin;

	updateActivated();
	QListViewItem::setText(1, m_pPlugin->name());

	m_pPlugin->items().append(this);
}


// Plugin container accessor.
qtractorPlugin *qtractorPluginListItem::plugin (void) const
{
	return m_pPlugin;
}


// Activation methods.
void qtractorPluginListItem::updateActivated (void)
{
	QListViewItem::setPixmap(0,
		*qtractorPluginListView::itemPixmap(
			m_pPlugin && m_pPlugin->isActivated() ? 1 : 0));
}


// Overrriden cell painter.
void qtractorPluginListItem::paintCell ( QPainter *p, const QColorGroup& cg,
		int column, int width, int align )
{
	// Paint the original but with a different background...
	QColorGroup _cg(cg);

	QColor bg = _cg.color(QColorGroup::Button);
	QColor fg = _cg.color(QColorGroup::ButtonText);

	if (isSelected()) {
		bg = _cg.color(QColorGroup::Midlight).dark(150);
		fg = _cg.color(QColorGroup::Midlight).light(150);
	}

	_cg.setColor(QColorGroup::Base, bg);
	_cg.setColor(QColorGroup::Text, fg);
	_cg.setColor(QColorGroup::Highlight, bg);
	_cg.setColor(QColorGroup::HighlightedText, fg);

	QListViewItem::paintCell(p, _cg, column, width, align);

	// Draw cell frame lines...
	int height = QListViewItem::height();
	p->setPen(bg.light(150));
//	p->drawLine(0, 0, 0, height - 1);
	p->drawLine(0, 0, width - 1, 0);
	p->setPen(bg.dark(150));
//	p->drawLine(width - 1, 0, width - 1, height - 1);
	p->drawLine(0, height - 1, width - 1, height - 1);
}


//----------------------------------------------------------------------------
// qtractorPluginListView -- Plugin chain list widget instance.
//
int      qtractorPluginListView::g_iItemRefCount   = 0;
QPixmap *qtractorPluginListView::g_pItemPixmaps[2] = { NULL, NULL };

// Construcctor.
qtractorPluginListView::qtractorPluginListView (
	QWidget *pParent, const char *pszName )
	: QListView(pParent, pszName), m_pPluginList(NULL)
{
	if (++g_iItemRefCount == 1) {
		g_pItemPixmaps[0] = new QPixmap(
			QPixmap::fromMimeSource("itemLedOff.png"));
		g_pItemPixmaps[1] = new QPixmap(
			QPixmap::fromMimeSource("itemLedOn.png"));
	}

	QListView::setFont(QFont(QListView::font().family(), 6));

	QListView::verticalScrollBar()->setStyle(&g_tinyScrollBarStyle);
	QListView::horizontalScrollBar()->setStyle(&g_tinyScrollBarStyle);

	QListView::header()->hide();

	QListView::addColumn(QString::null, 10);	// State
	QListView::addColumn(QString::null);		// Name

	QListView::setAllColumnsShowFocus(true);
    QListView::setResizeMode(QListView::LastColumn);
	QListView::setSortColumn(-1);

	QListView::viewport()->setPaletteBackgroundColor(
		QListView::colorGroup().color(QColorGroup::Background));

//	QListView::setHScrollBarMode(QScrollView::AlwaysOff);
//	QListView::setVScrollBarMode(QScrollView::AlwaysOn);

	// Simple click handling...
	QObject::connect(
		this, SIGNAL(clicked(QListViewItem*, const QPoint&, int)),
		this, SLOT(clickItem(QListViewItem*, const QPoint&, int)));
	// Simple click handling...
	QObject::connect(
		this, SIGNAL(doubleClicked(QListViewItem *, const QPoint&, int)),
		this, SLOT(doubleClickItem(QListViewItem *, const QPoint&, int)));
	QObject::connect(
		this, SIGNAL(returnPressed(QListViewItem *)),
		this, SLOT(returnPressItem(QListViewItem *)));
}


// Destructor.
qtractorPluginListView::~qtractorPluginListView (void)
{
	// No need to delete child widgets, Qt does it all for us
	if (--g_iItemRefCount == 0) {
		for (int i = 0; i < 2; i++) {
			delete g_pItemPixmaps[i];
			g_pItemPixmaps[i] = NULL;
		}
	}
}


// Plugin list accessors.
void qtractorPluginListView::setPluginList ( qtractorPluginList *pPluginList )
{
	if (m_pPluginList)
		m_pPluginList->views().remove(this);

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
	QListView::clear();

	if (m_pPluginList) {
		qtractorPluginListItem *pItem = NULL;
		for (qtractorPlugin *pPlugin = m_pPluginList->first();
				pPlugin; pPlugin = pPlugin->next()) {
			pItem = new qtractorPluginListItem(this, pPlugin, pItem);
		}
	}
}

// Find an item, given the plugin reference...
qtractorPluginListItem *qtractorPluginListView::pluginItem (
	qtractorPlugin *pPlugin )
{
	if (pPlugin == NULL)
		return NULL;

	QListViewItem *pItem = QListView::firstChild();
	while (pItem) {
		qtractorPluginListItem *pPluginItem
			= static_cast<qtractorPluginListItem *> (pItem);
		if (pPluginItem->plugin() == pPlugin)
			return pPluginItem;
		pItem = pItem->nextSibling();
	}

	return NULL;
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
		= static_cast<qtractorPluginListItem *> (QListView::selectedItem());
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
		= static_cast<qtractorPluginListItem *> (QListView::selectedItem());
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

	QListViewItem *pItem  = QListView::selectedItem();
	if (pItem == NULL)
		return;

	QListViewItem *pPrevItem = pItem->itemAbove();
	if (pPrevItem)
		pPrevItem = pPrevItem->itemAbove();

	moveItem(static_cast<qtractorPluginListItem *> (pItem),
		static_cast<qtractorPluginListItem *> (pPrevItem));
}


// Move an existing plugin downward slot.
void qtractorPluginListView::moveDownPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	QListViewItem *pItem  = QListView::selectedItem();
	if (pItem == NULL)
		return;

	moveItem(static_cast<qtractorPluginListItem *> (pItem),
		static_cast<qtractorPluginListItem *> (pItem->itemBelow()));
}


// Show/hide an existing plugin form slot.
void qtractorPluginListView::editPlugin (void)
{
	if (m_pPluginList == NULL)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListView::selectedItem());
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
		pPluginForm->setActiveWindow();
	}
}


// Toggle show an existing plugin activation slot.
void qtractorPluginListView::clickItem (
	QListViewItem *item, const QPoint& /*pos*/, int col )
{
	if (m_pPluginList == NULL || col != 0)
		return;

	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (item);
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


// Show an existing plugin form slot.
void qtractorPluginListView::doubleClickItem (
	QListViewItem *item, const QPoint& /*pos*/, int col )
{
	if (col != 1)
		return;

	returnPressItem(item);
}

void qtractorPluginListView::returnPressItem ( QListViewItem *item )
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
	pPluginForm->setActiveWindow();
}


// Context menu event handler.
void qtractorPluginListView::contextMenuEvent (
	QContextMenuEvent *pContextMenuEvent )
{
	if (m_pPluginList == NULL)
		return;

	int iItemID;
	QPopupMenu* pContextMenu = new QPopupMenu(this);

	bool bEnabled = (QListView::childCount() > 0);

	qtractorPlugin *pPlugin = NULL;
	qtractorPluginListItem *pItem
		= static_cast<qtractorPluginListItem *> (QListView::selectedItem());
	if (pItem)
		pPlugin = pItem->plugin();

	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formCreate.png")),
		tr("Add Plugin..."), this, SLOT(addPlugin()));
//	pContextMenu->setItemEnabled(iItemID, true);

	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formRemove.png")),
		tr("Remove Plugin"), this, SLOT(removePlugin()));
	pContextMenu->setItemEnabled(iItemID, pPlugin != NULL);

	pContextMenu->insertSeparator();

	iItemID = pContextMenu->insertItem(
		tr("Activate"), this, SLOT(activatePlugin()));
	pContextMenu->setItemEnabled(iItemID, pPlugin != NULL);
	pContextMenu->setItemChecked(iItemID, pPlugin && pPlugin->isActivated());

	pContextMenu->insertSeparator();

	bool bActivatedAll   = m_pPluginList->isActivatedAll();
	iItemID = pContextMenu->insertItem(
		tr("Activate All"), this, SLOT(activateAllPlugins()));
	pContextMenu->setItemEnabled(iItemID, bEnabled && !bActivatedAll);
	pContextMenu->setItemChecked(iItemID, bActivatedAll);

	bool bDeactivatedAll = (m_pPluginList->activated() < 1);
	iItemID = pContextMenu->insertItem(
		tr("Deactivate All"), this, SLOT(deactivateAllPlugins()));
	pContextMenu->setItemEnabled(iItemID, bEnabled && !bDeactivatedAll);
	pContextMenu->setItemChecked(iItemID, bDeactivatedAll);

	pContextMenu->insertSeparator();

	iItemID = pContextMenu->insertItem(
		tr("Remove All"), this, SLOT(removeAllPlugins()));
	pContextMenu->setItemEnabled(iItemID, bEnabled);

	pContextMenu->insertSeparator();

	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formMoveUp.png")),
		tr("Move Up"), this, SLOT(moveUpPlugin()));
	pContextMenu->setItemEnabled(iItemID, pItem && pItem->itemAbove() != NULL);

	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formMoveDown.png")),
		tr("Move Down"), this, SLOT(moveDownPlugin()));
	pContextMenu->setItemEnabled(iItemID, pItem && pItem->itemBelow() != NULL);

	pContextMenu->insertSeparator();

	iItemID = pContextMenu->insertItem(
		QIconSet(QPixmap::fromMimeSource("formEdit.png")),
		tr("Edit"), this, SLOT(editPlugin()));
	pContextMenu->setItemEnabled(iItemID, pItem != NULL);
	pContextMenu->setItemChecked(iItemID, pPlugin && pPlugin->isVisible());

	pContextMenu->exec(pContextMenuEvent->globalPos());

	delete pContextMenu;
}


// Common pixmap accessors.
QPixmap *qtractorPluginListView::itemPixmap ( int iIndex )
{
	return g_pItemPixmaps[iIndex];
}


//----------------------------------------------------------------------------
// qtractorPluginPortWidget -- Plugin port widget.
//

// Constructor.
qtractorPluginPortWidget::qtractorPluginPortWidget (
	qtractorPluginPort *pPort, QWidget *pParent, const char *pszName )
	: QFrame(pParent, pszName), m_pPort(pPort), m_iUpdate(0)
{
	m_pGridLayout = NULL;
	m_pLabel      = NULL;
	m_pSlider     = NULL;
	m_pSpinBox    = NULL;
	m_pCheckBox   = NULL;

	const QString sColon = ":";
	if (m_pPort->isToggled()) {
		m_pGridLayout = new QGridLayout(this, 1, 3, 4);
		m_pCheckBox = new QCheckBox(this);
		m_pCheckBox->setText(m_pPort->name());
	//	m_pCheckBox->setChecked(m_pPort->value() > 0.1f);
		m_pGridLayout->addMultiCellWidget(m_pCheckBox, 0, 0, 0, 2);
		QObject::connect(m_pCheckBox, SIGNAL(toggled(bool)),
			this, SLOT(checkBoxToggled(bool)));
	} else if (m_pPort->isInteger()) {
		m_pGridLayout = new QGridLayout(this, 1, 3, 4);
		m_pGridLayout->setColSpacing(0, 120);
		m_pLabel = new QLabel(this);
		m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_pLabel->setText(m_pPort->name() + sColon);
		m_pGridLayout->addMultiCellWidget(m_pLabel, 0, 0, 0, 1);
		m_pSpinBox = new qtractorSpinBox(this, NULL, 0);
		m_pSpinBox->setAlignment(Qt::AlignHCenter);
		m_pSpinBox->setMaximumWidth(80);
		m_pSpinBox->setMinValue(int(m_pPort->minValue()));
		m_pSpinBox->setMaxValue(int(m_pPort->maxValue()));
	//	m_pSpinBox->setValue(int(m_pPort->value()));
		m_pGridLayout->addWidget(m_pSpinBox, 0, 2);
		QObject::connect(m_pSpinBox, SIGNAL(valueChanged(const QString&)),
			this, SLOT(spinBoxValueChanged(const QString&)));
	} else {
		m_pGridLayout = new QGridLayout(this, 2, 3, 4);
		m_pLabel = new QLabel(this);
		m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
		m_pLabel->setText(m_pPort->name() + sColon);
		m_pGridLayout->addMultiCellWidget(m_pLabel, 0, 0, 0, 2);
		m_pSlider = new qtractorSlider(Qt::Horizontal, this);
		m_pSlider->setMinimumWidth(120);
		m_pSlider->setMinValue(0);
		m_pSlider->setMaxValue(10000);
		m_pSlider->setPageStep(1000);
		m_pSlider->setLineStep(100);
		m_pSlider->setDefaultValue(portToSlider(m_pPort->defaultValue()));
	//	m_pSlider->setValue(portToSlider(m_pPort->value()));
		m_pGridLayout->addMultiCellWidget(m_pSlider, 1, 1, 0, 1);
		m_pSpinBox = new qtractorSpinBox(this, NULL, portDecs());
		m_pSpinBox->setMaximumWidth(80);
		m_pSpinBox->setMinValueFloat(m_pPort->minValue());
		m_pSpinBox->setMaxValueFloat(m_pPort->maxValue());
	//	m_pSpinBox->setValueFloat(m_pPort->value());
		m_pGridLayout->addWidget(m_pSpinBox, 1, 2);
		QObject::connect(m_pSlider, SIGNAL(valueChanged(int)),
			this, SLOT(sliderValueChanged(int)));
		QObject::connect(m_pSpinBox, SIGNAL(valueChanged(const QString&)),
			this, SLOT(spinBoxValueChanged(const QString&)));
	}

//	QFrame::setFrameShape(QFrame::StyledPanel);
//	QFrame::setFrameShadow(QFrame::Raised);
	QToolTip::add(this, m_pPort->name());
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
		m_pSpinBox->setValueFloat(m_pPort->value());
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
			m_pSpinBox->setValueFloat(fValue);
		emit valueChanged(m_pPort, fValue);
	}

	m_iUpdate--;
}


// end of qtractorPluginListView.cpp
