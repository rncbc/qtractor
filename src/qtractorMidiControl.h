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
class qtractorDocument;

class QDomElement;


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

	// MIDI control map key.
	class MapKey
	{
	public:

		// Constructor.
		MapKey(unsigned short iChannel = 0, unsigned short iController = 0)
			: m_iChannel(iChannel), m_iController(iController) {}

		// Channel accessors.
		void setChannel(unsigned short iChannel)
			{ m_iChannel = iChannel; }
		unsigned short channel() const
			{ return m_iChannel; }

		bool isChannel() const
			{ return ((m_iChannel & MaskParam) == m_iChannel); }
		bool isChannelParam() const
			{ return (m_iChannel & TrackParam); }

		// Controller accessors.
		void setController (unsigned short iController)
			{ m_iController = iController; }
		unsigned short controller() const
			{ return m_iController; }

		bool isController() const
			{ return ((m_iController & MaskParam ) == m_iController); }
		bool isControllerParam() const
			{ return (m_iController & TrackParam); }

		// Generic key matcher.
		bool match (unsigned short iChannel, unsigned iController) const
		{
			return (isChannelParam() || channel() == iChannel)
				&& (isControllerParam() || controller() == iController);
		}

		// Hash key comparator.
		bool operator== ( const MapKey& key ) const
		{
			return (key.m_iChannel == m_iChannel)
				&& (key.m_iController == m_iController);
		}

	private:

		// Instance (key) member variables.
		unsigned short m_iChannel;
		unsigned short m_iController;
	};

	// MIDI control map data value.
	class MapVal
	{
	public:

		// Constructor.
		MapVal(Command command = TrackNone, int iParam = 0, bool bFeedback = false)
			: m_command(command), m_iParam(iParam), m_bFeedback(bFeedback) {}

		// Command accessors
		void setCommand(Command command)
			{ m_command = command; }
		Command command() const
			{ return m_command; }

		// Parameter accessor (eg. track delta-index)
		void setParam(int iParam)
			{ m_iParam = iParam; }
		int param() const
			{ return m_iParam; }

		// Feedback flag accessor.
		void setFeedback(bool bFeedback)
			{ m_bFeedback = bFeedback; }
		int isFeedback() const
			{ return m_bFeedback; }

	private:

		// Instance (value) member variables.
		Command m_command;
		int     m_iParam;
		bool    m_bFeedback;
	};

	// MIDI control map type.
	typedef QHash<MapKey, MapVal> ControlMap;

	// Constructor.
	qtractorMidiControl();

	// Destructor.
	~qtractorMidiControl();

	static qtractorMidiControl *getInstance();

	// Clear control map (reset to default).
	void clear();

	// Insert new controller mappings.
	void mapChannelController(
		unsigned short iChannel, unsigned short iController,
		Command command, int iParam = 0, bool bFeedback = false);
	void mapChannelParamController (unsigned short iController,
		Command command, int iParam = 0, bool bFeedback = false);
	void mapChannelControllerParam(unsigned short iChannel,
		Command command, int iParam = 0, bool bFeedback = false);

	// Remove existing controller mapping.
	void unmapChannelController(
		unsigned short iChannel, unsigned short iController);

	// Check if given channel, controller pair is currently mapped.
	bool isChannelControllerMapped(
		unsigned short iChannel, unsigned short iController) const;

	// Re-send all controllers.
	void sendAllControllers() const;

	// Process incoming controller messages.
	bool processEvent(qtractorMidiControlEvent *pEvent) const;

	// Process incoming command.
	void processCommand(Command command, int iParam, float fValue) const;
	void processCommand(Command command, int iParam, bool bValue) const;

	// Control map accessor.
	const ControlMap& controlMap() const { return m_controlMap; }

	// Forward declaration.
	class Document;

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement);

	// Document file methods.
	bool loadDocument(const QString& sFilename);
	bool saveDocument(const QString& sFilename);

	// Document textual helpers.
	static unsigned short keyFromText(const QString& sText);
	static QString textFromKey(unsigned short iKey);

	static Command commandFromText(const QString& sText);
	static QString textFromCommand(Command command);

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


// Hash key function
inline uint qHash ( const qtractorMidiControl::MapKey& key )
{
	return qHash(key.channel() ^ key.controller());
}


#endif  // __qtractorMidiControl_h

// end of qtractorMidiControl.h
