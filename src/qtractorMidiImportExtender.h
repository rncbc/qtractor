// qtractorMidiImportExtender.h
//
/****************************************************************************
   Copyright (C) 2005-2017, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiImportExtender_h
#define __qtractorMidiImportExtender_h

#include <QString>
#include <QSet>

// forward decalarations.
class qtractorSession;
class qtractorPlugin;
class qtractorPluginList;
class qtractorPluginListDocument;
class qtractorEditTrackCommand;
class qtractorTrack;
class qtractorMidiClip;
class qtractorCommand;
class QDomDocument;


class qtractorMidiImportExtender
{
public:
	// Constructor.
	qtractorMidiImportExtender();
	// Destructor.
	virtual ~qtractorMidiImportExtender();

	// Save all settings to global options.
	void saveSettings();
	// Undo plugin list commands done since creation.
	void backoutCommandList();
	// Restore plugin list command list to the point of creation.
	void restoreCommandList();

	// Accessor/creator for GUI plugin list.
	qtractorPluginList *pluginListForGui();
	// Clear GUI singleton plugin list - do only call on session close!!!
	static void clearPluginList();

	// Methods used during import.
	void prepareTrackForExtension(qtractorTrack *pTrack);
	bool finishTracksForExtension(QList<qtractorTrack *> *pImportedTracks);

	// Track name set types.
	typedef enum { Midifile, Track, PatchName } TrackNameType;

	// Declare all extended settings for MIDI import.
	typedef struct{
		// Plugin list.
		QDomDocument *pPluginDomDocument;
		// Instruments to set.
		QString sMidiImportInstInst;
		QString sMidiImportDrumInst;
		// Banks to set
		int iMidiImportInstBank;
		int iMidiImportDrumBank;
		// Type of track name to set.
		TrackNameType eMidiImportTrackNameType;
	}  exportExtensionsData;

	// Accessor to exteneded settings.
	exportExtensionsData *exportExtensions();
	// Keep auto-deactivation change
	bool m_bAutoDeactivateWasDisabled;

private:

	// XMLize plugin List
	void pluginListToDocument();

	// Pointer to singleton plugin list for display.
	static qtractorPluginList *m_pPluginList;
	// Keep info that plugin list was empied.
	static bool m_bPluginListIsEmpty;

	// Settings member.
	exportExtensionsData m_extendedSettings;

	// Keep last command for backout / restore.
	qtractorCommand *m_pLastUndoCommand;

	// Track number for 'Track n' naming type.
	int m_iTrackNumber;

};

#endif // __qtractorMidiImportExtender_h

// end of qtractorMidiImportExtender.h
