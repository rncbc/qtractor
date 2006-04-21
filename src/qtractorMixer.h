// qtractorMixer.h
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

#ifndef __qtractorMixer_h
#define __qtractorMixer_h

#include <qdockwindow.h>
#include <qptrlist.h>
#include <qframe.h>


// Forward declarations.
class qtractorMixerStrip;
class qtractorMixerRack;
class qtractorMixer;

class qtractorMonitor;
class qtractorMeter;

class qtractorMainForm;
class qtractorSession;
class qtractorTrack;

class QHBox;
class QHBoxLayout;
class QVBoxLayout;
class QSpacerItem;

class QLabel;
class QSplitter;


//----------------------------------------------------------------------------
// qtractorMixerStrip -- Mixer strip widget.

class qtractorMixerStrip : public QFrame
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMixerStrip(qtractorMixerRack *pRack,
		qtractorMonitor *pMonitor, const QString& sName);
	// Default destructor.
	~qtractorMixerStrip();

	// Delegated properties accessors.
	void setMonitor(qtractorMonitor *pMonitor);
	qtractorMonitor *monitor() const;

	void setName(const QString& sName);
	QString name() const;

	void setForeground(const QColor& fg);
	const QColor& foreground() const;

	void setBackground(const QColor& bg);
	const QColor& background() const;

	// Child accessors.
	qtractorMeter *meter() const;

	// Selection methods.
	void setSelected(bool bSelected);
	bool isSelected() const;

	// Strip refreshment.
	void refresh();

	// Hacko-list-management marking...
	void setMark(int iMark);
	int mark() const;

protected:

	// Mouse selection event handler.
	void mousePressEvent(QMouseEvent *);
	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

private:

	// Local instance variables.
	qtractorMixerRack *m_pRack;
	
	// Local widgets.
	QVBoxLayout   *m_pLayout;
	QLabel        *m_pLabel;
	qtractorMeter *m_pMeter;
	
	// Selection stuff.
	bool m_bSelected;

	// Hacko-list-management mark...
	int m_iMark;
};


//----------------------------------------------------------------------------
// qtractorMixerRack -- Mixer strip rack.

class qtractorMixerRack : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMixerRack(qtractorMixer *pMixer, int iAlignment = Qt::AlignLeft);
	// Default destructor.
	~qtractorMixerRack();

	// The strip workspace.
	QHBox *workspace() const;

	// Strip list primitive methods.
	void addStrip(qtractorMixerStrip *pStrip);
	void removeStrip(qtractorMixerStrip *pStrip);

	// Find a mixer strip, given its monitor handle.
	qtractorMixerStrip *findStrip(qtractorMonitor *pMonitor);
	
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

	// Common context menu handler.
	void contextMenu(const QPoint& gpos, qtractorMixerStrip *pStrip);

signals:

	// Selection changed signal.
	void selectionChanged();

protected:

	// Context menu request event handler.
	void contextMenuEvent(QContextMenuEvent *);

private:

	// Instance properties.
	qtractorMixer *m_pMixer;

	// Layout widgets.
	QHBoxLayout *m_pRackLayout;
	QHBox       *m_pStripHBox;
	QSpacerItem *m_pStripSpacer;

	// The Strips list.
	QPtrList<qtractorMixerStrip> m_strips;
	
	// Selection stuff.
	bool                m_bSelectEnabled;
	qtractorMixerStrip *m_pSelectedStrip;
};


//----------------------------------------------------------------------------
// qtractorMixer -- Mixer widget.

class qtractorMixer : public QDockWindow
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMixer(qtractorMainForm *pMainForm);
	// Default destructor.
	~qtractorMixer();

	// Main application form accessors.
	qtractorMainForm *mainForm() const;

	// Session accessors.
	qtractorSession *session() const;

	// The splitter layout widget accessor.
	QSplitter *splitter() const;

	// The mixer strips rack accessors.
	qtractorMixerRack *inputRack()  const;
	qtractorMixerRack *trackRack()  const;
	qtractorMixerRack *outputRack() const;
	
	// Update busses and tracks'racks.
	void updateBusses();
	void updateTracks();

	// Complete mixer refreshment.
	void refresh();

	// Complete mixer recycle.
	void clear();

protected:

	// Update mixer rack, checking if given monitor already exists.
	void updateBusStrip(qtractorMixerRack *pRack,
		qtractorMonitor *pMonitor, const QString& sName);
	void updateTrackStrip(qtractorTrack *pTrack);

private:

	// Main application form reference.
	qtractorMainForm *m_pMainForm;

	// Child controls.
	QSplitter *m_pSplitter;

	qtractorMixerRack *m_pInputRack;
	qtractorMixerRack *m_pTrackRack;
	qtractorMixerRack *m_pOutputRack;
};


#endif  // __qtractorMixer_h

// end of qtractorMixer.h
