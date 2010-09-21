// qtractorMidiControl.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2009, gizzmo aka Mathias Krause. 

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

#include "qtractorCtlEvent.h"

#include <QHash>

// Forward declarations.
class qtractorTrack;
class qtractorDocument;
class qtractorCtlEvent;

class QDomElement;


//----------------------------------------------------------------------
// qtractorMidiControl -- MIDI control map (singleton).
//

class qtractorMidiControl
{
public:

	// Controller types.
	typedef qtractorMidiEvent::EventType ControlType;

	// Controller command types.
	enum Command {
		TRACK_GAIN    = 1,
		TRACK_PANNING = 2,
		TRACK_MONITOR = 3,
		TRACK_RECORD  = 4,
		TRACK_MUTE    = 5,
		TRACK_SOLO    = 6
	};

	// Key param masks (wildcard flags).
	enum { TrackParam = 0x4000, TrackParamMask = 0x3fff };

	// MIDI control map key.
	class MapKey
	{
	public:

		// Constructor.
		MapKey(ControlType ctype = qtractorMidiEvent::CONTROLLER,
			unsigned short iChannel = 0, unsigned short iParam = 0)
			: m_ctype(ctype), m_iChannel(iChannel), m_iParam(iParam) {}

		// Type accessors.
		void setType(ControlType ctype)
			{ m_ctype = ctype; }
		ControlType type() const
			{ return m_ctype; }

		// Channel accessors.
		void setChannel(unsigned short iChannel)
			{ m_iChannel = iChannel; }
		unsigned short channel() const
			{ return m_iChannel; }

		bool isChannel() const
			{ return ((m_iChannel & TrackParamMask) == m_iChannel); }
		bool isChannelTrack() const
			{ return (m_iChannel & TrackParam); }

		// Controller accessors.
		void setParam(unsigned short iParam)
			{ m_iParam = iParam; }
		unsigned short param() const
			{ return m_iParam; }

		bool isParam() const
			{ return ((m_iParam & TrackParamMask ) == m_iParam); }
		bool isParamTrack() const
			{ return (m_iParam & TrackParam); }

		// Generic key matcher.
		bool match(ControlType ctype,
			unsigned short iChannel, unsigned short iParam) const
		{
			return (type() == ctype 
				&& (isChannelTrack() || channel() == iChannel)
				&& (isParamTrack() || param() == iParam));
		}

		// Hash key comparator.
		bool operator== (const MapKey& key) const
		{
			return (key.m_ctype == m_ctype)
				&& (key.m_iChannel == m_iChannel)
				&& (key.m_iParam == m_iParam);
		}

	private:

		// Instance (key) member variables.
		ControlType    m_ctype;
		unsigned short m_iChannel;
		unsigned short m_iParam;
	};

	// MIDI control map data value.
	class MapVal
	{
	public:

		// Constructor.
		MapVal(Command command = Command(0), int iTrack = 0, bool bFeedback = false)
			: m_command(command), m_iTrack(iTrack), m_bFeedback(bFeedback) {}

		// Command accessors
		void setCommand(Command command)
			{ m_command = command; }
		Command command() const
			{ return m_command; }

		// Track offset accessor.
		void setTrack(int iTrack)
			{ m_iTrack = iTrack; }
		int track() const
			{ return m_iTrack; }

		// Feedback flag accessor.
		void setFeedback(bool bFeedback)
			{ m_bFeedback = bFeedback; }
		int isFeedback() const
			{ return m_bFeedback; }

	private:

		// Instance (value) member variables.
		Command m_command;
		int     m_iTrack;
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
	void mapChannelParam(ControlType ctype,
		unsigned short iChannel, unsigned short iParam,
		Command command, int iTrack = 0, bool bFeedback = false);
	void mapChannelTrack(ControlType ctype, unsigned short iParam,
		Command command, int iTrack = 0, bool bFeedback = false);
	void mapChannelParamTrack(ControlType ctype,
		unsigned short iChannel, unsigned short iParam,
		Command command, int iTrack = 0, bool bFeedback = false);

	// Remove existing controller mapping.
	void unmapChannelParam(ControlType ctype,
		unsigned short iChannel, unsigned short iParam);

	// Check if given channel, param triplet is currently mapped.
	bool isChannelParamMapped(ControlType ctype,
		unsigned short iChannel, unsigned short iParam) const;

	// Re-send all (track) controllers.
	void sendAllControllers(int iFirstTrack = 0) const;

	// Process incoming controller messages.
	bool processEvent(const qtractorCtlEvent& ctle) const;

	// Process incoming command.
	void processTrackCommand(
		Command command, int iTrack, float fValue, bool bCubic = false) const;
	void processTrackCommand(
		Command command, int iTrack, bool bValue) const;

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
	static ControlType typeFromText(const QString& sText);
	static const QString& textFromType(ControlType ctype);

	static ControlType typeFromName(const QString& sName);
	static const QString& nameFromType(ControlType ctype);

	static unsigned short keyFromText(const QString& sText);
	static QString textFromKey(unsigned short iKey);

	static Command commandFromText(const QString& sText);
	static const QString& textFromCommand(Command command);

	static Command commandFromName(const QString& sName);
	static const QString& nameFromCommand(Command command);

protected:

	// Find incoming controller event map.
	ControlMap::ConstIterator findEvent(const qtractorCtlEvent& ctle) const;

	// Overloaded controller value senders.
	void sendTrackController(
		ControlType ctype, qtractorTrack *pTrack, Command command,
		unsigned short iChannel, unsigned short iParam) const;
	void sendTrackController(int iTrack, Command command, int iValue) const;

	void sendController(ControlType ctype,
		unsigned short iChannel, unsigned short iParam, int iValue) const;

private:

	// MIDI control map.
	ControlMap m_controlMap;

	// Pseudo-singleton instance.
	static qtractorMidiControl *g_pMidiControl;
};


// Hash key function
inline uint qHash ( const qtractorMidiControl::MapKey& key )
{
	return qHash(uint(key.type()) ^ key.channel() ^ key.param());
}


#endif  // __qtractorMidiControl_h

// end of qtractorMidiControl.h
