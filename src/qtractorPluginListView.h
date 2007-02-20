// qtractorPluginListView.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorPluginListView_h
#define __qtractorPluginListView_h

#include "qtractorPlugin.h"

#include <QListWidget>
#include <QFrame>

// Forward declarations.
class qtractorPluginListView;
class qtractorSlider;

class QIcon;
class QLabel;
class QCheckBox;
class QGridLayout;
class QDoubleSpinBox;

class QContextMenuEvent;


//----------------------------------------------------------------------------
// qtractorPluginListItem -- Plugin list item.

class qtractorPluginListItem : public QListWidgetItem
{
public:

	// Constructors.
	qtractorPluginListItem(qtractorPlugin *pPlugin);
	// Destructor.
	~qtractorPluginListItem();

	// Plugin container accessor.
	qtractorPlugin *plugin() const;

	// Special plugin form accessor.
	qtractorPluginForm *pluginForm();

	// Activation methods.
	void updateActivated();

protected:

	// Common item initializer.
	void initItem(qtractorPlugin *pPlugin);

private:

	// The plugin reference.
	qtractorPlugin *m_pPlugin;
};


//----------------------------------------------------------------------------
// qtractorPluginListView -- Plugin chain list widget instance.
//

class qtractorPluginListView : public QListWidget
{
	Q_OBJECT

public:

	// Construcctor.
	qtractorPluginListView(QWidget *pParent = 0);
	// Destructor.
	~qtractorPluginListView();

	// Plugin list accessors.
	void setPluginList(qtractorPluginList *pPluginList);
	qtractorPluginList *pluginList() const;

	// Plugin list refreshner;
	void refresh();

	// Get an item index, given the plugin reference...
	int pluginItem(qtractorPlugin *pPlugin);

	// Common pixmap accessors.
	static QIcon *itemIcon(int iIndex);

protected slots:

	// User interaction slots.
	void addPlugin();
	void removePlugin();
	void activatePlugin();
	void activateAllPlugins();
	void deactivateAllPlugins();
	void removeAllPlugins();
	void moveUpPlugin();
	void moveDownPlugin();
	void editPlugin();

	// Simple click handler.
	void itemDoubleClickedSlot(QListWidgetItem *);
	void itemActivatedSlot(QListWidgetItem *);

protected:

	// Move item on list.
	void moveItem(qtractorPluginListItem *pItem,
		qtractorPluginListItem *pPrevItem = 0);

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// To get precize clicking for in-place (de)activation.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);

	// Context menu event handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

private:

	// Instance variables.
	qtractorPluginList *m_pPluginList;

	// The mouse clicked item for in-place (de)activation.
	QListWidgetItem *m_pClickedItem;

	// Common pixmap stuff.
	static QIcon *g_pItemIcons[2];
	static int    g_iItemRefCount;
};


//----------------------------------------------------------------------------
// qtractorPluginPortWidget -- Plugin port widget.
//

class qtractorPluginPortWidget : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginPortWidget(qtractorPluginPort *pPort, QWidget *pParent = 0);
	// Destructor.
	~qtractorPluginPortWidget();

	// Main properties accessors.
	qtractorPluginPort *port() const;

	// Refreshner-loader method.
	void refresh();

signals:

	// Change notification.
	void valueChanged(qtractorPluginPort *, float);

protected slots:

	// Change slots.
	void checkBoxToggled(bool);
	void spinBoxValueChanged(const QString&);
	void sliderValueChanged(int);

protected:

	// Slider conversion methods.
	int   portToSlider(float fValue) const;
	float sliderToPort(int iValue) const;

	// Spin-box decimals helper.
	int   portDecs() const;

private:

	// Instance variables.
	qtractorPluginPort *m_pPort;

	// Some basic layout managers.
	QGridLayout    *m_pGridLayout;

	// Some possible managed widgets.
	QLabel         *m_pLabel;
	QCheckBox      *m_pCheckBox;
	qtractorSlider *m_pSlider;
	QDoubleSpinBox *m_pSpinBox;

	// Avoid cascaded intra-notifications.
	int m_iUpdate;
};


#endif  // __qtractorPluginList_h

// end of qtractorPluginList.h
