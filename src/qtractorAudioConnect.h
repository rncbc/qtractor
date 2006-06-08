// qtractorAudioConnect.h
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

#ifndef __qtractorAudioConnect_h
#define __qtractorAudioConnect_h

#include "qtractorConnect.h"

#include <jack/jack.h>

// Forward declarations.
class qtractorAudioPortItem;
class qtractorAudioClientItem;
class qtractorAudioClientListView;
class qtractorAudioConnect;

// Pixmap-set array indexes.
#define QTRACTOR_AUDIO_CLIENT_IN		0	// Input client item pixmap.
#define QTRACTOR_AUDIO_CLIENT_OUT		1	// Output client item pixmap.
#define QTRACTOR_AUDIO_PORT_IN			2	// Input port item pixmap.
#define QTRACTOR_AUDIO_PORT_OUT			3	// Output port item pixmap.
#define QTRACTOR_AUDIO_PORT_PHYS_IN		4	// Physical input port item pixmap.
#define QTRACTOR_AUDIO_PORT_PHYS_OUT	5	// Physical output port item pixmap.
#define QTRACTOR_AUDIO_PIXMAPS			6	// Number of pixmaps in local array.


//----------------------------------------------------------------------
// qtractorAudioPortItem -- Jack port list item.
//

class qtractorAudioPortItem : public qtractorPortListItem
{
public:

	// Constructor.
	qtractorAudioPortItem(qtractorAudioClientItem *pClientItem,
		const QString& sPortName, jack_port_t *pJackPort);
	// Default destructor.
	~qtractorAudioPortItem();

	// Jack handles accessors.
	jack_client_t *jackClient() const;
	jack_port_t   *jackPort()   const;

private:

	// Instance variables.
	jack_port_t *m_pJackPort;
};


//----------------------------------------------------------------------
// qtractorAudioClientItem -- Jack client list item.
//

class qtractorAudioClientItem : public qtractorClientListItem
{
public:

	// Constructor.
	qtractorAudioClientItem(qtractorAudioClientListView *pClientListView,
		const QString& sClientName);
	// Default destructor.
	~qtractorAudioClientItem();

	// Jack client accessors.
	jack_client_t *jackClient() const;
};


//----------------------------------------------------------------------
// qtractorAudioClientListView -- Jack client list view.
//

class qtractorAudioClientListView : public qtractorClientListView
{
public:

	// Constructor.
	qtractorAudioClientListView(QWidget *pParent = 0, const char *pszName = 0);
	// Default destructor.
	~qtractorAudioClientListView();

	// Jack connect accessors.
	jack_client_t *jackClient() const;

	// Client:port refreshner (return newest item count).
	int updateClientPorts();
};


//----------------------------------------------------------------------------
// qtractorAudioConnect -- Connections model integrated object.
//

class qtractorAudioConnect : public qtractorConnect
{
public:

	// Constructor.
	qtractorAudioConnect(
		qtractorAudioClientListView *pOListView,
		qtractorAudioClientListView *pIListView,
		qtractorConnectorView *pConnectorView);

	// Default destructor.
	~qtractorAudioConnect();

	// Jack client accessors.
	void setJackClient(jack_client_t *pJackClient);
	jack_client_t *jackClient() const;

	// Common pixmap accessor.
	static const QPixmap& pixmap (int iPixmap);
	
protected:

	// Virtual Connect/Disconnection primitives.
	bool connectPorts(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort);
	bool disconnectPorts(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort);

	// Update port connection references.
	void updateConnections();

private:

	// Local pixmap-set janitor methods.
	void createIconPixmaps();
	void deleteIconPixmaps();

	// Instance variables.
	jack_client_t *m_pJackClient;

	// Local pixmap-set array.
	static QPixmap *g_apPixmaps[QTRACTOR_AUDIO_PIXMAPS];
	static int      g_iPixmapsRefCount;
};


#endif  // __qtractorAudioConnect_h

// end of qtractorAudioConnect.h
