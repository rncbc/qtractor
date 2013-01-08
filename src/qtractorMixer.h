// qtractorMixer.h
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMixer_h
#define __qtractorMixer_h

#include "qtractorEngine.h"

#include "qtractorTrackButton.h"

#include <QScrollArea>
#include <QFrame>
#include <QHash>


// Forward declarations.
class qtractorMixerStrip;
class qtractorMixerRack;
class qtractorMixer;
class qtractorMeter;

class qtractorPluginListView;

class QHBoxLayout;
class QVBoxLayout;

class QLabel;
class QSplitter;
class QPushButton;


//----------------------------------------------------------------------------
// qtractorMonitorButton -- Monitor observer tool button.

class qtractorMonitorButton : public qtractorMidiControlButton
{
	Q_OBJECT

public:

	// Constructors.
	qtractorMonitorButton(qtractorTrack *pTrack, QWidget *pParent = 0);
	qtractorMonitorButton(qtractorBus *pBus, QWidget *pParent = 0);

	// Specific track accessors.
	void setTrack(qtractorTrack *pTrack);
	qtractorTrack *track() const;

	// Specific bus accessors.
	void setBus(qtractorBus *pBus);
	qtractorBus *bus() const;

protected slots:

	// Special toggle slot.
	void toggledSlot(bool bOn);

protected:

	// Common initializer.
	void initMonitorButton();

	// Visitor setup.
	void updateMonitor();

	// Visitors overload.
	void updateValue(float fValue);

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
	qtractorBus   *m_pBus;
};


//----------------------------------------------------------------------------
// qtractorMixerStrip -- Mixer strip widget.

class qtractorMixerStrip : public QFrame
{
	Q_OBJECT

public:

	// Constructors.
	qtractorMixerStrip(qtractorMixerRack *pRack, qtractorBus *pBus,
		qtractorBus::BusMode busMode);
	qtractorMixerStrip(qtractorMixerRack *pRack, qtractorTrack *pTrack);

	// Default destructor.
	~qtractorMixerStrip();

	// Clear/suspend delegates.
	void clear();

	// Delegated properties accessors.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	// Child accessors.
	qtractorPluginListView *pluginListView() const;
	qtractorMeter *meter() const;

	// Bus property accessors.
	void setBus(qtractorBus *pBus);
	qtractorBus *bus() const;

	// Track property accessors.
	void setTrack(qtractorTrack *pTrack);
	qtractorTrack *track() const;

	// Selection methods.
	void setSelected(bool bSelected);
	bool isSelected() const;

	// Strip refreshment.
	void refresh();

	// Hacko-list-management marking...
	void setMark(int iMark);
	int mark() const;

	// Special bus dispatchers.
	void busConnections(qtractorBus::BusMode busMode);
	void busMonitor(bool bMonitor);

	// Track monitor dispatcher.
	void trackMonitor(bool bMonitor);

protected slots:

	// Bus connections button notification.
	void busButtonSlot();

	// Meter slider change slots.
	void panningChangedSlot(float);
	void gainChangedSlot(float);

protected:

	// Common mixer-strip initializer.
	void initMixerStrip();

	void updateMidiLabel();
	void updateName();

	// Mouse selection event handlers.
	void mousePressEvent(QMouseEvent *);

	// Mouse selection event handlers.
	void mouseDoubleClickEvent(QMouseEvent *);

private:

	// Local instance variables.
	qtractorMixerRack *m_pRack;

	qtractorBus *m_pBus;
	qtractorBus::BusMode m_busMode;

	qtractorTrack *m_pTrack;

	// Local widgets.
	class IconLabel;

	QVBoxLayout            *m_pLayout;
	IconLabel              *m_pLabel;
	qtractorPluginListView *m_pPluginListView;
	QHBoxLayout            *m_pButtonLayout;
	qtractorMonitorButton  *m_pMonitorButton;
	qtractorTrackButton    *m_pRecordButton;
	qtractorTrackButton    *m_pMuteButton;
	qtractorTrackButton    *m_pSoloButton;
	qtractorMeter          *m_pMeter;
	QPushButton            *m_pBusButton;
	QLabel                 *m_pMidiLabel;

	// Selection stuff.
	bool m_bSelected;

	// Hacko-list-management mark...
	int m_iMark;
};


//----------------------------------------------------------------------------
// qtractorMixerRack -- Mixer strip rack.

class qtractorMixerRack : public QScrollArea
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMixerRack(qtractorMixer *pMixer, const QString& sName);
	// Default destructor.
	~qtractorMixerRack();

	// The main mixer widget accessor.
	qtractorMixer *mixer() const;

	// Rack name accessor.
	void setName(const QString& sName);
	const QString& name() const;

	// The mixer strip workspace.
	QWidget *workspace() const;

	// Strip list primitive methods.
	void addStrip(qtractorMixerStrip *pStrip);
	void removeStrip(qtractorMixerStrip *pStrip);

	// Find a mixer strip, given its monitor handle.
	qtractorMixerStrip *findStrip(qtractorMonitor *pMonitor);

	// Update a mixer strip on rack list.
	void updateStrip(qtractorMixerStrip *pStrip, qtractorMonitor *pMonitor);

	// Current Strip count.
	int stripCount() const;

	// Complete rack refreshment.
	void refresh();

	// Complete rack recycle.
	void clear();

	// Selection stuff.
	void setSelectEnabled(bool bSelectEnabled);
	bool isSelectEnabled() const;
	
	void setSelectedStrip(qtractorMixerStrip *pStrip);
	qtractorMixerStrip *selectedStrip() const;

	// Hacko-list-management marking...
	void markStrips(int iMark);
	void cleanStrips(int iMark);

public slots:

	// Bus context menu slots.
	void busInputsSlot();
	void busOutputsSlot();
	void busMonitorSlot();
	void busPropertiesSlot();

signals:

	// Selection changed signal.
	void selectionChanged();

protected:

	// Resize event handler.
	void resizeEvent(QResizeEvent *pResizeEvent);

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

	// Mouse click event handler.
	void mousePressEvent(QMouseEvent *);

private:

	// Instance properties.
	qtractorMixer *m_pMixer;

	// Rack title.
	QString m_sName;
	
	// Layout widgets.
	QWidget     *m_pWorkspace;
	QHBoxLayout *m_pWorkspaceLayout;

	// The Strips list.
	typedef QHash<qtractorMonitor *, qtractorMixerStrip *> Strips;
	Strips m_strips;
	
	// Selection stuff.
	bool                m_bSelectEnabled;
	qtractorMixerStrip *m_pSelectedStrip;
};


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

class qtractorMixer : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMixer(QWidget *pParent, Qt::WindowFlags wflags = 0);
	// Default destructor.
	~qtractorMixer();

	// The splitter layout widget accessor.
	QSplitter *splitter() const;

	// The mixer strips rack accessors.
	qtractorMixerRack *inputRack()  const;
	qtractorMixerRack *trackRack()  const;
	qtractorMixerRack *outputRack() const;
	
	// Update buses and tracks'racks.
	void updateBuses(bool bReset = false);
	void updateTracks(bool bReset = false);

	// Update mixer rack, checking whether the monitor actually exists.
	void updateBusStrip(qtractorMixerRack *pRack, qtractorBus *pBus,
		qtractorBus::BusMode busMode, bool bReset = false);
	void updateTrackStrip(qtractorTrack *pTrack, bool bReset = false);

	// Complete mixer refreshment.
	void refresh();

	// Complete mixer recycle.
	void clear();

protected:

	// Notify the main application widget that we're closing.
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

	// Just about to notify main-window that we're closing.
	void closeEvent(QCloseEvent *);

	// Keyboard event handler.
	void keyPressEvent(QKeyEvent *);

	// Special splitter-size persistence methods.
	void loadSplitterSizes();
	void saveSplitterSizes();

	// Initial minimum widget extents.
	QSize sizeHint() const;

private:

	// Child controls.
	QSplitter *m_pSplitter;

	qtractorMixerRack *m_pInputRack;
	qtractorMixerRack *m_pTrackRack;
	qtractorMixerRack *m_pOutputRack;
};


#endif  // __qtractorMixer_h

// end of qtractorMixer.h
