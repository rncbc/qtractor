// qtractorMidiControl.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.
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

#include <QString>
#include <QHash>


// Forward declarations.
class qtractorTrack;
class qtractorDocument;

class qtractorMidiControlObserver;

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

	// Controller command modes.
	enum CommandMode {
		VALUE         = 1,
		SWITCH_BUTTON = 2,
		PUSH_BUTTON   = 3,
		ENCODER		  = 4
	};

	// Key param masks (wildcard flags).
	enum { TrackParam = 0x4000, TrackParamMask = 0x3fff };

	// MIDI control map key.
	class MapKey
	{
	public:

		// Constructor.
		MapKey(ControlType ctype = qtractorMidiEvent::CONTROLLER,
			unsigned short iChannel = 0, unsigned short iParam = 0, unsigned short iParamLimit = USHRT_MAX)
			: m_ctype(ctype), m_iChannel(iChannel), m_iParam(iParam), m_iParamLimit(iParamLimit) {}

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

		void setParamLimit(unsigned short iParamLimit)
			{ m_iParamLimit = iParamLimit; }
		unsigned short paramLimit() const
			{ return m_iParamLimit; }

		bool isParam() const
			{ return ((m_iParam & TrackParamMask ) == m_iParam); }
		bool isParamTrack() const
			{ return (m_iParam & TrackParam); }

		// Generic key matcher.
		bool match(ControlType ctype, unsigned short iChannel,
			unsigned short iParam ) const
		{
			// make sure, the parameter is between the base value and the limit
			const unsigned short iParamBase = (param() & TrackParamMask);

			return (type() == ctype
				&& (isChannelTrack() || channel() == iChannel)
				&& ((isParamTrack() && iParam >= iParamBase && iParam <= paramLimit() )
					|| param() == iParam ));
		}

		// Hash/map key comparator.
		bool operator== (const MapKey& key) const
		{
			return (key.m_ctype == m_ctype)
				&& (key.m_iChannel == m_iChannel)
				&& (key.m_iParam == m_iParam)
				&& (key.m_iParamLimit == m_iParamLimit);
		}

	private:

		// Instance (key) member variables.
		ControlType    m_ctype;
		unsigned short m_iChannel;
		unsigned short m_iParam;
		unsigned short m_iParamLimit;
	};

	// MIDI control map data value.
	class MapVal
	{
	public:

		// Constructor.
		MapVal(Command command = Command(0), CommandMode commandMode = PUSH_BUTTON, int iTrack = 0, bool bFeedback = false)
			: m_command(command), m_commandMode(commandMode), m_iTrack(iTrack), m_bFeedback(bFeedback) {}

		// Command accessors
		void setCommand(Command command)
			{ m_command = command; }
		Command command() const
			{ return m_command; }

		// CommandMode accessors
		void setCommandMode(CommandMode commandMode)
			{ m_commandMode = commandMode; }
		CommandMode commandMode() const
			{ return m_commandMode; }

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

		// MIDI control track (catch-up) value.
		class Track
		{
		public:

			// Constructor.
			Track(float fValue = 0.0f)
				: m_fValue(fValue), m_bValueSync(false) {}

			// Tracking/catch-up methods.
			bool sync(float fValue, float fOldValue)
			{
				bool bSync = qtractorMidiControl::isSync();
				if (!bSync)
					bSync = m_bValueSync;
				if (!bSync) {
					const float v0 = m_fValue;
					const float v1 = fOldValue;
				#if 0
					if ((fValue > v0 && v1 >= v0 && fValue >= v1) ||
						(fValue < v0 && v0 >= v1 && v1 >= fValue))
						 bSync = true;
				#else
					const float d1 = (v1 - fValue);
					const float d2 = (v1 - v0) * d1;
					bSync = (d2 < 0.001f);
				#endif
				}
				if (bSync) {
					m_fValue = fValue;
					m_bValueSync = true;
				}
				return bSync;
			}

			void syncReset()
				{ m_bValueSync = false; }

			float value() const
				{ return m_fValue; }

		private:

			// Tracking/catch-up members.
			float m_fValue;
			bool  m_bValueSync;
		};

		Track& track(int iTrack)
			{ return m_trackMap[iTrack]; }

		void syncReset(int iTrack)
			{ m_trackMap[iTrack].syncReset(); }

		void clear()
			{ m_trackMap.clear(); }

	private:

		// Instance (value) member variables.
		Command     m_command;
		CommandMode m_commandMode;
		int         m_iTrack;
		bool        m_bFeedback;

		QHash<int, Track> m_trackMap;
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

	// Clear track (catch-up) map.
	void clearControlMap();

	// Insert new controller mappings.
	void mapChannelParam(ControlType ctype,
		unsigned short iChannel, unsigned short iParam,
		unsigned short iParamLimit, Command command, CommandMode commandMode,
		int iTrack = 0, bool bFeedback = false);
	void mapChannelTrack(ControlType ctype, unsigned short iParam,
		unsigned short iParamLimit, Command command, CommandMode commandMode,
		int iTrack = 0, bool bFeedback = false);
	void mapChannelParamTrack(ControlType ctype,
		unsigned short iChannel, unsigned short iParam,
		unsigned short iParamLimit, Command command, CommandMode commandMode,
		int iTrack = 0, bool bFeedback = false);

	// Remove existing controller mapping.
	void unmapChannelParam(ControlType ctype,
		unsigned short iChannel, unsigned short iParam,
		unsigned short iParamLimit);

	// Check if given channel, param triplet is currently mapped.
	bool isChannelParamMapped(ControlType ctype,
		unsigned short iChannel, unsigned short iParam,
		unsigned short iParamLimit) const;

	// Re-send all (track) controllers.
	void sendAllControllers(int iFirstTrack = 0) const;

	void sendController(
		ControlType ctype, unsigned short iChannel,
		unsigned short iParam, unsigned short iValue) const;

	// Process incoming controller messages.
	bool processEvent(const qtractorCtlEvent& ctle);

	// Process incoming command.
	void processTrackCommand(
		Command command, int iTrack, float fValue, bool bCubic = false);
	void processTrackCommand(
		Command command, int iTrack, bool bValue);

	// Control map accessor.
	const ControlMap& controlMap() const { return m_controlMap; }

	// Insert/remove observer mappings.
	void mapMidiObserver(qtractorMidiControlObserver *pMidiObserver);
	void unmapMidiObserver(qtractorMidiControlObserver *pMidiObserver);

	// Observer map predicate.
	bool isMidiObserverMapped(qtractorMidiControlObserver *pMidiObserver) const;

	// Observer finder.
	qtractorMidiControlObserver *findMidiObserver(
		ControlType ctype,
		unsigned short iChannel,
		unsigned short iParam) const;

	// Forward declaration.
	class Document;

	// Document element methods.
	bool loadElement(qtractorDocument *pDocument, QDomElement *pElement);
	bool saveElement(qtractorDocument *pDocument, QDomElement *pElement);

	// Document file methods.
	bool loadDocument(const QString& sFilename);
	bool saveDocument(const QString& sFilename);

	// Parameter controllers (MIDI).
	struct Controller
	{
		QString        name;
		unsigned long  index;
		ControlType    ctype;
		unsigned short channel;
		unsigned short param;
		bool           logarithmic;
		bool           feedback;
		bool           invert;
		bool           hook;
		bool           latch;
	};

	typedef QList<Controller *> Controllers;

	// Load/save meter controllers (MIDI).
	static void loadControllers(
		QDomElement *pElement, Controllers& controllers);
	static void saveControllers(qtractorDocument *pDocument,
		QDomElement *pElement, const Controllers& controllers);

	// Document textual helpers.
	static ControlType typeFromText(const QString& sText);
	static const QString& textFromType(ControlType ctype);

	static ControlType typeFromName(const QString& sName);
	static const QString& nameFromType(ControlType ctype);

	static unsigned short keyFromText(const QString& sText);
	static QString textFromKey(unsigned short iKey);

	static CommandMode commandModeFromText(const QString& sText);
	static const QString& textFromCommandMode(CommandMode commandMode);

	static CommandMode commandModeFromName(const QString& sName);
	static const QString& nameFromCommandMode(CommandMode commandMode);

	static Command commandFromText(const QString& sText);
	static const QString& textFromCommand(Command command);

	static Command commandFromName(const QString& sName);
	static const QString& nameFromCommand(Command command);

	// MIDI control non catch-up/hook global option.
	static void setSync(bool bSync);
	static bool isSync();

protected:

	// Find incoming controller event map.
	ControlMap::Iterator findEvent(const qtractorCtlEvent& ctle);

	// Overloaded controller value senders.
	void sendTrackController(
		ControlType ctype, qtractorTrack *pTrack, Command command,
		unsigned short iChannel, unsigned short iParam) const;
	void sendTrackController(
		int iTrack, Command command, float fValue, bool bCubic);

	// MIDI control scale (7bit vs. 14bit).
	class ControlScale
	{
	public:

		// Constructor
		ControlScale(ControlType ctype, CommandMode commandMode)
		{
			m_iMaxScale = (
				ctype == qtractorMidiEvent::PITCHBEND   ||
				ctype == qtractorMidiEvent::REGPARAM    ||
				ctype == qtractorMidiEvent::NONREGPARAM ||
				ctype == qtractorMidiEvent::CONTROL14
				? 0x3fff : 0x7f);
			m_fMaxScale = float(m_iMaxScale);
			const unsigned short mid = (m_iMaxScale >> 1);
			m_fMidScale = float(mid);
			m_iMidScale = int(mid + 1);
			m_commandMode = commandMode;
		}

		// Scale value converters (unsigned).
		float valueFromMidi(unsigned short iValue) const
			{ return float(iValue) / m_fMaxScale; }
		unsigned short midiFromValue(float fValue) const
			{ return m_fMaxScale * fValue; }

		// Scale value converters (signed).
		float valueSignedFromMidi(unsigned short iValue, float fCurrentValue = 0.0f) const
			{
			float fValue = 0.0f;
			switch(m_commandMode){
			case ENCODER:
				if( iValue == m_iMidScale) return 0.0f;
				fValue = iValue > m_iMidScale ? fCurrentValue - 0.1f : fCurrentValue + 0.1f;
				if( ( fValue >=  -1 ) && ( fValue <= 1 ) )
					return fValue;
				else
					return fCurrentValue;
			case VALUE:
			case SWITCH_BUTTON:
			case PUSH_BUTTON:
			default:
				return float(int(iValue) - m_iMidScale) / m_fMidScale;
			}
		}

		unsigned short midiFromValueSigned(float fValue) const
			{ return m_iMidScale + int(m_fMidScale * fValue); }

		// Scale value converters (toggled).
		float valueToggledFromMidi(unsigned short iValue, float fCurrentValue = 0.0f ) const
		{
			switch(m_commandMode){
			case PUSH_BUTTON:
				// do nohing if value lower than max
				if( iValue < m_iMaxScale)
					return fCurrentValue;
				// else toggle current state
				return( fCurrentValue == 0.0f ? 1.0f : 0.0f );
			case SWITCH_BUTTON:
			case VALUE:
			case ENCODER:
			default:
				return (iValue > m_iMidScale ? 1.0f : 0.0f);
			}
		}
		unsigned short midiFromValueToggled(float fValue) const
			{ return (fValue > 0.5f ? m_iMaxScale : 0); }

	private:

		// Scale helpers factors.
		float m_fMaxScale;
		float m_fMidScale;
		int   m_iMaxScale;
		int   m_iMidScale;

		// Threshold for buttons
		float m_fThreshold;
		CommandMode m_commandMode;
	};

private:

	// MIDI control map.
	ControlMap m_controlMap;

	// MIDI observer map.
	typedef QHash<MapKey, qtractorMidiControlObserver *> ObserverMap;

	ObserverMap m_observerMap;

	// MIDI control non catch-up/hook global option.
	static bool g_bSync;

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
