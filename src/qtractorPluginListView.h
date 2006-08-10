// qtractorPluginListView.h
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

#ifndef __qtractorPluginListView_h
#define __qtractorPluginListView_h

#include "qtractorPlugin.h"

#include <qlistview.h>
#include <qframe.h>

// Forward declarations.
class qtractorPluginListView;

class QGridLayout;
class QLabel;
class QCheckBox;

class qtractorSlider;
class qtractorSpinBox;


//----------------------------------------------------------------------------
// qtractorPluginListItem -- Plugin list item.

class qtractorPluginListItem : public QListViewItem
{
public:

	// Constructors.
	qtractorPluginListItem(qtractorPluginListView *pListView,
		qtractorPlugin *pPlugin);
	qtractorPluginListItem(qtractorPluginListView *pListView,
		qtractorPlugin *pPlugin, QListViewItem *pItemAfter);

	// Destructor.
	~qtractorPluginListItem();

	// Plugin container accessor.
	qtractorPlugin *plugin() const;

	// Special plugin form accessor.
	qtractorPluginForm *setPluginForm(qtractorPluginForm *pPluginForm);
	qtractorPluginForm *pluginForm();

	// Activation methods.
	void updateActivated();

	// Overrriden virtual cell painter.
	void paintCell(QPainter *p, const QColorGroup& cg,
		int column, int width, int align);

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

class qtractorPluginListView : public QListView
{
	Q_OBJECT

public:

	// Construcctor.
	qtractorPluginListView(QWidget *pParent = 0, const char *pszName = 0);
	// Destructor.
	~qtractorPluginListView();

	// Plugin list accessors.
	void setPluginList(qtractorPluginList *pPluginList);
	qtractorPluginList *pluginList() const;

	// Common pixmap accessors.
	static QPixmap *itemPixmap(int iIndex);

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
	void clickItem(QListViewItem *item, const QPoint& pos, int col);
	void doubleClickItem(QListViewItem *item, const QPoint& pos, int col);
	void returnPressItem(QListViewItem *item);

protected:

	// Move item on list.
	void moveItem(qtractorPluginListItem *pItem,
		qtractorPluginListItem *pPrevItem = 0);

	// Context menu event handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

private:

	// Instance variables.
	qtractorPluginList *m_pPluginList;
	
	// Common pixmap stuff.
	static QPixmap *g_pItemPixmaps[2];
	static int      g_iItemRefCount;
};


//----------------------------------------------------------------------------
// qtractorPluginPortWidget -- Plugin port widget.
//

class qtractorPluginPortWidget : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginPortWidget(qtractorPluginPort *pPort,
		QWidget *pParent = 0, const char *pszName = 0);
	// Destructor.
	~qtractorPluginPortWidget();

	// Main properties accessors.
	qtractorPluginPort *port() const;

	// Refreshner-loader method.
	void refresh();

signals:

	// Change notification.
	void valueChanged(float);

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
	QGridLayout     *m_pGridLayout;

	// Some possible managed widgets.
	QLabel          *m_pLabel;
	QCheckBox       *m_pCheckBox;
	qtractorSlider  *m_pSlider;
	qtractorSpinBox *m_pSpinBox;

	// Avoid cascaded intra-notifications.
	int m_iUpdate;
};


#endif  // __qtractorPluginList_h

// end of qtractorPluginList.h
