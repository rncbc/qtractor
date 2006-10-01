// qtractorMidiConnect.h
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

#ifndef __qtractorMidiConnect_h
#define __qtractorMidiConnect_h

#include "qtractorConnect.h"

#include <alsa/asoundlib.h>

// Forward declarations.
class qtractorMidiPortItem;
class qtractorMidiClientItem;
class qtractorMidiClientListView;
class qtractorMidiConnect;


//----------------------------------------------------------------------
// qtractorMidiPortItem -- Alsa port list item.
//

class qtractorMidiPortItem : public qtractorPortListItem
{
public:

	// Constructor.
	qtractorMidiPortItem(qtractorMidiClientItem *pClientItem,
		const QString& sPortName, int iAlsaPort);
	// Default destructor.
	~qtractorMidiPortItem();

	// Jack handles accessors.
	int alsaClient() const;
	int alsaPort()   const;

private:

	// Instance variables.
	int m_iAlsaPort;
};


//----------------------------------------------------------------------
// qtractorMidiClientItem -- Alsa client list item.
//

class qtractorMidiClientItem : public qtractorClientListItem
{
public:

	// Constructor.
	qtractorMidiClientItem(qtractorMidiClientListView *pClientListView,
		const QString& sClientName, int iAlsaClient);
	// Default destructor.
	~qtractorMidiClientItem();

	// Jack client accessors.
	int alsaClient() const;

	// Port finder by id.
	qtractorMidiPortItem *findPortItem(int iAlsaPort);

private:

	// Instance variables.
	int m_iAlsaClient;
};


//----------------------------------------------------------------------
// qtractorMidiClientListView -- Alsa client list view.
//

class qtractorMidiClientListView : public qtractorClientListView
{
public:

	// Constructor.
	qtractorMidiClientListView(QWidget *pParent = 0, const char *pszName = 0);
	// Default destructor.
	~qtractorMidiClientListView();

	// Alsa sequencer accessor.
	snd_seq_t *alsaSeq() const;

	// Client finder by id.
	qtractorMidiClientItem *findClientItem(int iAlsaClient);
	// Client port finder by id.
	qtractorMidiPortItem *findClientPortItem(int iAlsaClient, int iAlsaPort);

	// Client:port refreshner (return newest item count).
	int updateClientPorts();
};


//----------------------------------------------------------------------------
// qtractorMidiConnect -- Connections model integrated object.
//

class qtractorMidiConnect : public qtractorConnect
{
public:

	// Constructor.
	qtractorMidiConnect(
		qtractorMidiClientListView *pOListView,
		qtractorMidiClientListView *pIListView,
		qtractorConnectorView *pConnectorView);

	// Default destructor.
	~qtractorMidiConnect();

	// Alsa sequencer accessor.
	void setAlsaSeq(snd_seq_t *pAlsaSeq);
	snd_seq_t *alsaSeq() const;

	// Pixmap-set array indexes.
	enum {
		ClientIn	= 0,	// Input client item pixmap.
		ClientOut	= 1,	// Output client item pixmap.
		PortIn		= 2,	// Input port item pixmap.
		PortOut		= 3,	// Output port item pixmap.
		PixmapCount	= 4		// Number of pixmaps in local array.
	};

	// Common pixmap accessor.
	static const QPixmap& pixmap(int iPixmap);

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
	snd_seq_t *m_pAlsaSeq;

	// Local static pixmap-set array.
	static QPixmap *g_apPixmaps[PixmapCount];
	static int      g_iPixmapsRefCount;
};


#endif  // __qtractorMidiConnect_h

// end of qtractorMidiConnect.h
