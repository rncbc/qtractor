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

#include "QString"

// forward decalarations.
class qtractorPluginList;
class qtractorPluginListDocument;
class qtractorEditTrackCommand;
class qtractorTrack;
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
	// Undo commands done since creation.
	void backoutCommandList();
	// Restore command list to the point of creation.
	void restoreCommandList();

	// Accessor to plugin list.
	qtractorPluginList *pluginList();
	// Clear singleton plugin-list.
	static void	clearPluginList();

	// Methods used during import.
	qtractorEditTrackCommand *modifyBareTrackAndGetEditCommand(qtractorTrack *pTrack);
	bool modifyFinishedTrack(qtractorTrack *pTrack);

	// Track name set types.
	typedef enum { Midifile, Track, PatchName } TrackNameType;

	// All extended settings except plugin list
	typedef struct{
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

private:

	// XMLize plugin List
	void pluginListToDocument();

	// pointer to singleton plugin-list
	static qtractorPluginList *m_pPluginList;

	// Plugin list settings keeper.
	QDomDocument *m_pPluginDomDocument;
	qtractorPluginListDocument *m_pPluginListDocument;

	// Counter for 'Track n' name-type
	int m_iTrackNumber;

	// All settings except plugin list.
	exportExtensionsData m_tExtendedSettings;

	// Keep last command for backout / restore.
	qtractorCommand *m_pLastCommand;
};

#endif // __qtractorMidiImportExtender_h

// end of qtractorMidiImportExtender.h
