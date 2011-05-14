// qtractorAudioConnect.h
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioConnect_h
#define __qtractorAudioConnect_h

#include "qtractorConnect.h"

#include <jack/jack.h>

// Forward declarations.
class qtractorAudioPortItem;
class qtractorAudioClientItem;
class qtractorAudioClientListView;
class qtractorAudioConnect;


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
	qtractorAudioClientListView(QWidget *pParent = NULL);
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

	// JACK client accessors.
	jack_client_t *jackClient() const;

	// Icon-set array indexes.
	enum {
		ClientIn	= 0,	// Input client item icon.
		ClientOut	= 1,	// Output client item icon.
		PortIn		= 2,	// Input port item icon.
		PortOut		= 3,	// Output port item icon.
		PortPhysIn	= 4,	// Physical input port item icon.
		PortPhysOut	= 5,	// Physical output port item icon.,
		IconCount	= 6		// Number of icons in local array.
	};

	// Common icon accessor.
	static const QIcon& icon (int iIcon);
	
protected:

	// Virtual Connect/Disconnection primitives.
	bool connectPorts(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort);
	bool disconnectPorts(qtractorPortListItem *pOPort,
		qtractorPortListItem *pIPort);

	// Update port connection references.
	void updateConnections();

	// Update (clear) Audio-buses connect lists (non-virtual).
	void disconnectPortsUpdate(
		qtractorPortListItem *pOPort, qtractorPortListItem *pIPort);

private:

	// Local pixmap-set janitor methods.
	void createIcons();
	void deleteIcons();

	// Local pixmap-set array.
	static QIcon *g_apIcons[IconCount];
	static int    g_iIconsRefCount;
};


#endif  // __qtractorAudioConnect_h

// end of qtractorAudioConnect.h
