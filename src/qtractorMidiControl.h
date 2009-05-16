// qtractorMidiControl.h
//
/****************************************************************************
   Copyright (C) 2005-2009, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiControl_h
#define __qtractorMidiControl_h

#include <QEvent>
#include <QHash>

// Forward declarations.
class qtractorTrack;


//----------------------------------------------------------------------
// qtractorMidiControlEvent - MIDI Control custom event.
//

class qtractorMidiControlEvent : public QEvent
{
public:

	// Contructor.
	qtractorMidiControlEvent(QEvent::Type eType, unsigned short iChannel,
		unsigned char controller, unsigned char value)
		: QEvent(eType), m_channel(iChannel),
			m_controller(controller), m_value(value) {}

	// Accessors.
	unsigned short channel()    const { return m_channel; }
	unsigned short controller() const { return m_controller; }
	unsigned short value()      const { return m_value; }

private:

	// Instance variables.
	unsigned short m_channel;
	unsigned short m_controller;
	unsigned short m_value;
};


//----------------------------------------------------------------------
// qtractorMidiControl -- MIDI control map (singleton).
//

class qtractorMidiControl
{
public:

	// Forward declarations
	class MapKey;
	class MapVal;

	typedef QHash<MapKey, MapVal> ControlMap;

	// Controller command types.
	enum Command {
		TrackNone    = 0,
		TrackGain    = 1,
		TrackPanning = 2,
		TrackMonitor = 3,
		TrackRecord  = 4,
		TrackMute    = 5,
		TrackSolo    = 6
	};

	// Key param masks (wildcard flags).
	enum { TrackParam = 0x4000, MaskParam = 0x3fff };

	// Constructor.
	qtractorMidiControl();

	// Destructor.
	~qtractorMidiControl();

	static qtractorMidiControl *getInstance();

	// Clear control map (reset to default).
	void clear();

	// Insert new contrller mappings.
	void mapChannelController(
		unsigned short iChannel, unsigned short iController,
		Command command, int iParam = 0, bool bFeedback = false);
	void mapChannelParamController (unsigned short iController,
		Command command, int iParam = 0, bool bFeedback = false);
	void mapChannelControllerParam(unsigned short iChannel,
		Command command, int iParam = 0, bool bFeedback = false);

	// Re-send all controllers.
	void sendAllControllers() const;

	// Process incoming controller messages.
	bool processEvent(qtractorMidiControlEvent *pEvent) const;

	// Process incoming command.
	void processCommand(Command command, int iParam, float fValue) const;
	void processCommand(Command command, int iParam, bool bValue) const;

protected:

	// Find incoming controller event map.
	ControlMap::ConstIterator findEvent(qtractorMidiControlEvent *pEvent) const;

	// Overloaded controller value senders.
	void sendParamController(Command command, int iParam, int iValue) const;
	void sendTrackController(qtractorTrack *pTrack, Command command,
		unsigned short iChannel, unsigned short iController) const;
	void sendController(
		unsigned short iChannel, unsigned short iController, int iValue) const;

private:

	// MIDI control map.
	ControlMap m_controlMap;

	// Pseudo-singleton instance.
	static qtractorMidiControl *g_pMidiControl;
};


#endif  // __qtractorMidiControl_h

// end of qtractorMidiControl.h
