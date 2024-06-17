// qtractorPluginListView.h
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QListWidget>


// Forward declarations.
class qtractorPlugin;
class qtractorPluginList;
class qtractorPluginForm;

class qtractorRubberBand;


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

	// Last known item rectangle.
	void setDirectAccessWidth(int iDirectAccessWidth)
		{ m_iDirectAccessWidth = iDirectAccessWidth; }
	int directAccessWidth() const
		{ return m_iDirectAccessWidth; }

protected:

	// Common item initializer.
	void initItem(qtractorPlugin *pPlugin);

private:

	// The plugin reference.
	qtractorPlugin *m_pPlugin;

	// Last known item logical width.
	int m_iDirectAccessWidth;

	// Common (de)activated icon/pixmap stuff.
	static QIcon *g_pIcons[3];
	static int    g_iIconsRefCount;
};


//----------------------------------------------------------------------------
// qtractorPluginListView -- Plugin chain list widget instance.
//

class qtractorPluginListView : public QListWidget
{
	Q_OBJECT

public:

	// Construcctor.
	qtractorPluginListView(QWidget *pParent = nullptr);
	// Destructor.
	~qtractorPluginListView();

	// Plugin list accessors.
	void setPluginList(qtractorPluginList *pPluginList);
	qtractorPluginList *pluginList() const;

	// Common tiny scrollbar style decl.
	class TinyScrollBarStyle;

	// Special scrollbar style accessors.
	void setTinyScrollBar(bool bTinyScrollBar);
	bool isTinyScrollBar() const;

	// Plugin list refreshner;
	void refresh();

	// Master clean-up.
	void clear();

	// Get an item index, given the plugin reference...
	int pluginItem(qtractorPlugin *pPlugin);

	// Show insert pseudo-plugin audio bus connections.
	static void insertPluginBus(qtractorPlugin *pPlugin, int iBusMode);

signals:

	// Plugin chain changed somehow.
	void contentsChanged();

public slots:

	// User interaction slots.
	void addPlugin();
	void removePlugin();
	void moveUpPlugin();
	void moveDownPlugin();

protected slots:

	// Generic interaction slots.
	void activatePlugin();
	void activateAllPlugins();
	void deactivateAllPlugins();
	void removeAllPlugins();

	void loadPresetPlugin();
	void directAccessPlugin();
	void propertiesPlugin();
	void editPlugin();

	// Import/export plugin-list slots.
	void importPlugins();
	void exportPlugins();

	// Audio inserts specific slots.
	void addAudioInsertPlugin();
	void addAudioAuxSendPlugin();

	// MIDI inserts specific slots.
	void addMidiInsertPlugin();
	void addMidiAuxSendPlugin();

	// Send/return insert specific slots.
	void insertPluginOutputs();
	void insertPluginInputs();

	// Audio specific slots.
	void audioOutputs();
	void audioOutputBus();
	void audioOutputBusName();
	void audioOutputAutoConnect();

	// Drop item slots.
	void dropMove();
	void dropCopy();
	void dropCancel();

	// Simple click handler.
	void itemDoubleClickedSlot(QListWidgetItem *);
	void itemActivatedSlot(QListWidgetItem *);

protected:

	// Move item on list.
	void moveItem(qtractorPluginListItem *pItem,
		qtractorPluginListItem *pNextItem);

	// Copy item on list.
	void copyItem(qtractorPluginListItem *pItem,
		qtractorPluginListItem *pNextItem);

	// Trap for help/tool-tip events.
	bool eventFilter(QObject *pObject, QEvent *pEvent);

	// To get precize clicking for in-place (de)activation,
	// and for drag-n-drop stuff -- reimplemented virtual methods.
	void mousePressEvent(QMouseEvent *pMouseEvent);
	void mouseMoveEvent(QMouseEvent *pMouseEvent);
	void mouseReleaseEvent(QMouseEvent *pMouseEvent);
	void dragEnterEvent(QDragEnterEvent *pDragEnterEvent);
	void dragMoveEvent(QDragMoveEvent *pDragMoveEvent);
	void dragLeaveEvent(QDragLeaveEvent *);
	void dropEvent(QDropEvent *pDropEvent);

	void wheelEvent(QWheelEvent *pWheelEvent);

	// Drag-n-drop stuff.
	bool canDropEvent(QDropEvent *pDropEvent);
	bool canDropItem(QDropEvent *pDropEvent);

	// Ensure given item is brought to viewport visibility...
	void ensureVisibleItem(qtractorPluginListItem *pItem);

	// Show and move rubber-band item.
	void moveRubberBand(qtractorPluginListItem *pDropItem);

	// Context menu event handler.
	void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

	// MIDI codec methods (static).
	static void encodeItem (QMimeData *pMimeData, qtractorPluginListItem *pItem);
	static bool canDecodeItem(const QMimeData *pMimeData);
	static qtractorPluginListItem *decodeItem(const QMimeData *pMimeData);

	// Show insert pseudo-plugin audio bus connections.
	void insertPluginBus(int iBusMode);
	
	// Direct access parameter handle.
	void dragDirectAccess(const QPoint& pos);

	// Direct access parameter reset (to default value).
	void resetDirectAccess(const QPoint& pos);

	// Initial size-policy hints.
	QSize sizeHint() const;

private:

	// Instance variables.
	qtractorPluginList *m_pPluginList;

	// The current dragging item stuff.
	enum DragState {
		DragNone = 0, DragDirectAccess
	} m_dragState, m_dragCursor;
	
	// The mouse clicked item for in-place (de)activation.
	qtractorPluginListItem *m_pClickedItem;

	// The point from where drag started.
	QPoint m_posDrag;

	// Item we'll eventually drag around.
	qtractorPluginListItem *m_pDragItem;
	// Item we'll eventually drop something.
	qtractorPluginListItem *m_pDropItem;

	// To show the point where drop will go.
	qtractorRubberBand *m_pRubberBand;

	// Common tiny scrollbar style stuff.
	TinyScrollBarStyle *m_pTinyScrollBarStyle;
};


#endif  // __qtractorPluginListView_h

// end of qtractorPluginListView.h
