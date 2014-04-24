// qtractorMidiEditor.cpp
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorMidiEditor.h"

#include "qtractorMidiEditList.h"
#include "qtractorMidiEditTime.h"
#include "qtractorMidiEditView.h"
#include "qtractorMidiEditEvent.h"

#include "qtractorMidiThumbView.h"

#include "qtractorMidiEditCommand.h"

#include "qtractorMidiEngine.h"
#include "qtractorMidiClip.h"

#include "qtractorMidiToolsForm.h"

#include "qtractorInstrument.h"
#include "qtractorRubberBand.h"
#include "qtractorTimeScale.h"

#include "qtractorSession.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QPixmap>
#include <QFrame>
#include <QIcon>
#include <QPainter>

#include <QFileInfo>
#include <QDir>

#include <QComboBox>
#include <QToolTip>

// Translatable macro contextualizer.
#undef  _TR
#define _TR(x) QT_TR_NOOP(x)


// Follow-playhead: maximum iterations on hold.
#define QTRACTOR_SYNC_VIEW_HOLD 66


// An empty blank reference string .
static QString g_sNoname;


//----------------------------------------------------------------------------
// MIDI Note Names - Default note names hash map.

static struct
{
	unsigned char note;
	const char *name;

} g_aNoteNames[] = {

	// Diatonic note map...
	{  0, _TR("C")     },
	{  1, _TR("C#/Db") },
	{  2, _TR("D")     },
	{  3, _TR("D#/Eb") },
	{  4, _TR("E")     },
	{  5, _TR("F")     },
	{  6, _TR("F#/Gb") },
	{  7, _TR("G")     },
	{  8, _TR("G#/Ab") },
	{  9, _TR("A")     },
	{ 10, _TR("A#/Bb") },
	{ 11, _TR("B")     },

	// GM Drum note map...
	{ 35, _TR("Acoustic Bass Drum") },
	{ 36, _TR("Bass Drum 1") },
	{ 37, _TR("Side Stick") },
	{ 38, _TR("Acoustic Snare") },
	{ 39, _TR("Hand Clap") },
	{ 40, _TR("Electric Snare") },
	{ 41, _TR("Low Floor Tom") },
	{ 42, _TR("Closed Hi-Hat") },
	{ 43, _TR("High Floor Tom") },
	{ 44, _TR("Pedal Hi-Hat") },
	{ 45, _TR("Low Tom") },
	{ 46, _TR("Open Hi-Hat") },
	{ 47, _TR("Low-Mid Tom") },
	{ 48, _TR("Hi-Mid Tom") },
	{ 49, _TR("Crash Cymbal 1") },
	{ 50, _TR("High Tom") },
	{ 51, _TR("Ride Cymbal 1") },
	{ 52, _TR("Chinese Cymbal") },
	{ 53, _TR("Ride Bell") },
	{ 54, _TR("Tambourine") },
	{ 55, _TR("Splash Cymbal") },
	{ 56, _TR("Cowbell") },
	{ 57, _TR("Crash Cymbal 2") },
	{ 58, _TR("Vibraslap") },
	{ 59, _TR("Ride Cymbal 2") },
	{ 60, _TR("Hi Bongo") },
	{ 61, _TR("Low Bongo") },
	{ 62, _TR("Mute Hi Conga") },
	{ 63, _TR("Open Hi Conga") },
	{ 64, _TR("Low Conga") },
	{ 65, _TR("High Timbale") },
	{ 66, _TR("Low Timbale") },
	{ 67, _TR("High Agogo") },
	{ 68, _TR("Low Agogo") },
	{ 69, _TR("Cabasa") },
	{ 70, _TR("Maracas") },
	{ 71, _TR("Short Whistle") },
	{ 72, _TR("Long Whistle") },
	{ 73, _TR("Short Guiro") },
	{ 74, _TR("Long Guiro") },
	{ 75, _TR("Claves") },
	{ 76, _TR("Hi Wood Block") },
	{ 77, _TR("Low Wood Block") },
	{ 78, _TR("Mute Cuica") },
	{ 79, _TR("Open Cuica") },
	{ 80, _TR("Mute Triangle") },
	{ 81, _TR("Open Triangle") },

	{  0, NULL }
};

static QHash<unsigned char, QString> g_noteNames;

// Default note name map accessor.
const QString qtractorMidiEditor::defaultNoteName (
	unsigned char note, bool fDrums )
{
	if (fDrums) {
		// Pre-load drum-names hash table...
		if (g_noteNames.isEmpty()) {
			for (int i = 12; g_aNoteNames[i].name; ++i) {
				g_noteNames.insert(g_aNoteNames[i].note,
					QObject::tr(g_aNoteNames[i].name, "noteName"));
			}
		}
		// Check whether the drum note exists...
		QHash<unsigned char, QString>::ConstIterator iter
			= g_noteNames.constFind(note);
		if (iter != g_noteNames.constEnd())
			return iter.value();
	}

	return QObject::tr(g_aNoteNames[note % 12].name, "noteName")
		+ QString::number((note / 12) - 1);
}


//----------------------------------------------------------------------------
// MIDI Controller Names - Default controller names hash map.

static struct
{
	unsigned char controller;
	const char *name;

} g_aControllerNames[] = {

	{  0, _TR("Bank Select (coarse)") },
	{  1, _TR("Modulation Wheel (coarse)") },
	{  2, _TR("Breath Controller (coarse)") },
	{  4, _TR("Foot Pedal (coarse)") },
	{  5, _TR("Portamento Time (coarse)") },
	{  6, _TR("Data Entry (coarse)") },
	{  7, _TR("Volume (coarse)") },
	{  8, _TR("Balance (coarse)") },
	{ 10, _TR("Pan Position (coarse)") },
	{ 11, _TR("Expression (coarse)") },
	{ 12, _TR("Effect Control 1 (coarse)") },
	{ 13, _TR("Effect Control 2 (coarse)") },
	{ 16, _TR("General Purpose Slider 1") },
	{ 17, _TR("General Purpose Slider 2") },
	{ 18, _TR("General Purpose Slider 3") },
	{ 19, _TR("General Purpose Slider 4") },
	{ 32, _TR("Bank Select (fine)") },
	{ 33, _TR("Modulation Wheel (fine)") },
	{ 34, _TR("Breath Controller (fine)") },
	{ 36, _TR("Foot Pedal (fine)") },
	{ 37, _TR("Portamento Time (fine)") },
	{ 38, _TR("Data Entry (fine)") },
	{ 39, _TR("Volume (fine)") },
	{ 40, _TR("Balance (fine)") },
	{ 42, _TR("Pan Position (fine)") },
	{ 43, _TR("Expression (fine)") },
	{ 44, _TR("Effect Control 1 (fine)") },
	{ 45, _TR("Effect Control 2 (fine)") },
	{ 64, _TR("Hold Pedal (on/off)") },
	{ 65, _TR("Portamento (on/off)") },
	{ 66, _TR("Sustenuto Pedal (on/off)") },
	{ 67, _TR("Soft Pedal (on/off)") },
	{ 68, _TR("Legato Pedal (on/off)") },
	{ 69, _TR("Hold 2 Pedal (on/off)") },
	{ 70, _TR("Sound Variation") },
	{ 71, _TR("Sound Timbre") },
	{ 72, _TR("Sound Release Time") },
	{ 73, _TR("Sound Attack Time") },
	{ 74, _TR("Sound Brightness") },
	{ 75, _TR("Sound Control 6") },
	{ 76, _TR("Sound Control 7") },
	{ 77, _TR("Sound Control 8") },
	{ 78, _TR("Sound Control 9") },
	{ 79, _TR("Sound Control 10") },
	{ 80, _TR("General Purpose Button 1 (on/off)") },
	{ 81, _TR("General Purpose Button 2 (on/off)") },
	{ 82, _TR("General Purpose Button 3 (on/off)") },
	{ 83, _TR("General Purpose Button 4 (on/off)") },
	{ 91, _TR("Effects Level") },
	{ 92, _TR("Tremulo Level") },
	{ 93, _TR("Chorus Level") },
	{ 94, _TR("Celeste Level") },
	{ 95, _TR("Phaser Level") },
	{ 96, _TR("Data Button Increment") },
	{ 97, _TR("Data Button Decrement") },
	{ 98, _TR("Non-Registered Parameter (fine)") },
	{ 99, _TR("Non-Registered Parameter (coarse)") },
	{100, _TR("Registered Parameter (fine)") },
	{101, _TR("Registered Parameter (coarse)") },
	{120, _TR("All Sound Off") },
	{121, _TR("All Controllers Off") },
	{122, _TR("Local Keyboard (on/off)") },
	{123, _TR("All Notes Off") },
	{124, _TR("Omni Mode Off") },
	{125, _TR("Omni Mode On") },
	{126, _TR("Mono Operation") },
	{127, _TR("Poly Operation") },

	{  0, NULL }
};

static QHash<unsigned char, QString> g_controllerNames;

// Default controller name accessor.
const QString& qtractorMidiEditor::defaultControllerName ( unsigned char controller )
{
	if (g_controllerNames.isEmpty()) {
		// Pre-load controller-names hash table...
		for (int i = 0; g_aControllerNames[i].name; ++i) {
			g_controllerNames.insert(g_aControllerNames[i].controller,
				QObject::tr(g_aControllerNames[i].name, "controllerName"));
		}
	}

	QHash<unsigned char, QString>::ConstIterator iter
		= g_controllerNames.constFind(controller);
	if (iter == g_controllerNames.constEnd())
		return g_sNoname;
	else
		return iter.value();
}


//----------------------------------------------------------------------------
// MIDI RPN Names - Default RPN names hash map.

static struct
{
	unsigned short param;
	const char *name;

} g_aRpnNames[] = {

	{  0, _TR("Pitch Bend Sensitivity") },
	{  1, _TR("Fine Tune") },
	{  2, _TR("Coarse Tune") },
	{  3, _TR("Tuning Program") },
	{  4, _TR("Tuning Bank") },

	{  0, NULL }
};

static QMap<unsigned short, QString> g_rpnNames;

// Default RPN map accessor.
const QMap<unsigned short, QString>& qtractorMidiEditor::defaultRpnNames (void)
{
	if (g_rpnNames.isEmpty()) {
		// Pre-load RPN-names hash table...
		for (int i = 0; g_aRpnNames[i].name; ++i) {
			g_rpnNames.insert(g_aRpnNames[i].param,
				QObject::tr(g_aRpnNames[i].name, "rpnName"));
		}
	}

	return g_rpnNames;
}


//----------------------------------------------------------------------------
// MIDI NRPN Names - Default NRPN names hash map.

static struct
{
	unsigned short param;
	const char *name;

} g_aNrpnNames[] = {

	{  136, _TR("Vibrato Rate") },
	{  137, _TR("Vibrato Depth") },
	{  138, _TR("Vibrato Delay") },
	{  160, _TR("Filter Cutoff") },
	{  161, _TR("Filter Resonance") },
	{  227, _TR("EG Attack") },
	{  228, _TR("EG Decay") },
	{  230, _TR("EG Release") },

	// GS Drum NRPN map...
	{ 2560, _TR("Drum Filter Cutoff") },
	{ 2688, _TR("Drum Filter Resonance") },
	{ 2816, _TR("Drum EG Attack") },
	{ 2944, _TR("Drum EG Decay") },
	{ 3072, _TR("Drum Pitch Coarse") },
	{ 3200, _TR("Drum Pitch Fine") },
	{ 3328, _TR("Drum Level") },
	{ 3584, _TR("Drum Pan") },
	{ 3712, _TR("Drum Reverb Send") },
	{ 3840, _TR("Drum Chorus Send") },
	{ 3968, _TR("Drum Variation Send") },

	{    0, NULL }
};

static QMap<unsigned short, QString> g_nrpnNames;

// Default NRPN map accessor.
const QMap<unsigned short, QString>& qtractorMidiEditor::defaultNrpnNames (void)
{
	if (g_nrpnNames.isEmpty()) {
		// Pre-load NRPN-names hash table...
		const QString sDrumNrpnName("%1 (%2)");
		for (int i = 0; g_aNrpnNames[i].name; ++i) {
			const unsigned short param
				= g_aNrpnNames[i].param;
			const QString& sName
				= QObject::tr(g_aNrpnNames[i].name, "nrpnName");
			if (param < 2560) {
				g_nrpnNames.insert(param, sName);
			} else {
				for (int j = 12; g_aNoteNames[j].name; ++j) {
					const unsigned char note = g_aNoteNames[j].note;
					g_nrpnNames.insert(param + note,
						sDrumNrpnName.arg(sName).arg(note));
					//	.arg(defaultNoteName(note, true)));
				}
			}
		}
	}

	return g_nrpnNames;
}


//----------------------------------------------------------------------------
// MIDI Control-14 Names - Default controller names hash map.

static struct
{
	unsigned char controller;
	const char *name;

} g_aControl14Names[] = {

	{  1, _TR("Modulation Wheel (14bit)") },
	{  2, _TR("Breath Controller (14bit)") },
	{  4, _TR("Foot Pedal (14bit)") },
	{  5, _TR("Portamento Time (14bit)") },
	{  7, _TR("Volume (14bit)") },
	{  8, _TR("Balance (14bit)") },
	{ 10, _TR("Pan Position (14bit)") },
	{ 11, _TR("Expression (14bit)") },
	{ 12, _TR("Effect Control 1 (14bit)") },
	{ 13, _TR("Effect Control 2 (14bit)") },
	{ 16, _TR("General Purpose Slider 1 (14bit)") },
	{ 17, _TR("General Purpose Slider 2 (14bit)") },
	{ 18, _TR("General Purpose Slider 3 (14bit)") },
	{ 19, _TR("General Purpose Slider 4 (14bit)") },

	{  0, NULL }
};

static QHash<unsigned char, QString> g_control14Names;

// Default control-14 name accessor.
const QString& qtractorMidiEditor::defaultControl14Name ( unsigned char controller )
{
	if (g_control14Names.isEmpty()) {
		// Pre-load controller-names hash table...
		for (int i = 0; g_aControl14Names[i].name; ++i) {
			g_control14Names.insert(g_aControl14Names[i].controller,
				QObject::tr(g_aControl14Names[i].name, "control14Name"));
		}
	}

	QHash<unsigned char, QString>::ConstIterator iter
		= g_control14Names.constFind(controller);
	if (iter == g_control14Names.constEnd())
		return g_sNoname;
	else
		return iter.value();
}


//----------------------------------------------------------------------------
// MIDI Scale Names - Default scale names table.

static struct
{
	const char   *name;
	unsigned char note[12];

} g_aScaleTab[] = {

	{ _TR("Chromatic"),              { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 } },
	{ _TR("Major"),                  { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9,11 } },
	{ _TR("Minor"),                  { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 8,11 } },
	{ _TR("Melodic Minor (Asc)"),    { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 9,11 } },
	{ _TR("Melodic Minor (Desc)"),   { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 } },
	{ _TR("Whole Tone"),             { 0, 0, 2, 2, 4, 4, 6, 6, 8, 8,10,10 } },
	{ _TR("Pentatonic Major"),       { 0, 0, 2, 2, 4, 4, 4, 7, 7, 9, 9, 9 } },
	{ _TR("Pentatonic Minor"),       { 0, 0, 0, 3, 3, 5, 5, 7, 7, 7,10,10 } },
	{ _TR("Pentatonic Blues"),       { 0, 0, 0, 3, 3, 5, 6, 7, 7, 7,10,10 } },
	{ _TR("Pentatonic Neutral"),     { 0, 0, 2, 2, 2, 5, 5, 7, 7, 7,10,10 } },
	{ _TR("Octatonic (H-W)"),        { 0, 1, 1, 3, 4, 4, 6, 7, 7, 9,10,10 } },
	{ _TR("Octatonic (W-H)"),        { 0, 0, 2, 3, 3, 5, 6, 6, 8, 9, 9,11 } },
	{ _TR("Ionian"),                 { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Major"
	{ _TR("Dorian"),                 { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9,10,10 } },
	{ _TR("Phrygian"),               { 0, 1, 1, 3, 3, 5, 5, 7, 8, 8,10,10 } },
	{ _TR("Lydian"),                 { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9, 9,11 } },
	{ _TR("Mixolydian"),             { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9,10,10 } },
	{ _TR("Aeolian"),                { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Melodic Minor (Descending)"
	{ _TR("Locrian"),                { 0, 1, 1, 3, 3, 5, 6, 6, 8, 8,10,10 } },
	{ _TR("Egyptian"),               { 0, 0, 2, 2, 2, 5, 5, 7, 7, 7,10,10 } },	// identical to "Pentatonic Neutral"
	{ _TR("Eight Tone Spanish"),     { 0, 1, 1, 3, 4, 5, 6, 6, 6, 6,10,10 } },
	{ _TR("Hawaiian"),               { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Melodic Minor (Ascending)"
	{ _TR("Hindu"),                  { 0, 0, 2, 2, 4, 5, 5, 7, 8, 8,10,10 } },
	{ _TR("Hirajoshi"),              { 0, 0, 2, 3, 3, 3, 3, 7, 8, 8, 8, 8 } },
	{ _TR("Hungarian Major"),        { 0, 0, 0, 3, 4, 4, 6, 7, 7, 9,10,10 } },
	{ _TR("Hungarian Minor"),        { 0, 0, 2, 3, 3, 3, 6, 7, 8, 8, 8,11 } },
	{ _TR("Hungarian Gypsy"),        { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8, 8,11 } },
	{ _TR("Japanese (A)"),           { 0, 1, 1, 1, 1, 5, 5, 7, 8, 8, 8, 8 } },
	{ _TR("Japanese (B)"),           { 0, 0, 2, 2, 2, 5, 5, 7, 8, 8, 8, 8 } },
	{ _TR("Jewish (Adonai Malakh)"), { 0, 1, 2, 3, 3, 5, 5, 7, 7, 9,10,10 } },
	{ _TR("Jewish (Ahaba Rabba)"),   { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8,10,10 } },
	{ _TR("Jewish (Magen Abot)"),    { 0, 1, 1, 3, 4, 4, 6, 6, 8, 8,10,11 } },
	{ _TR("Oriental (A)"),           { 0, 1, 1, 1, 4, 5, 6, 6, 8, 8,10,10 } },
	{ _TR("Oriental (B)"),           { 0, 1, 1, 1, 1, 1, 6, 6, 6, 9,10,10 } },
	{ _TR("Oriental (C)"),           { 0, 1, 1, 1, 4, 5, 6, 6, 6, 9,10,10 } },
	{ _TR("Roumanian Minor"),        { 0, 0, 2, 3, 3, 3, 6, 7, 7, 9,10,10 } },
	{ _TR("Neapolitan"),             { 0, 1, 1, 3, 3, 5, 5, 7, 8, 8, 8,11 } },
	{ _TR("Neapolitan Major"),       { 0, 1, 1, 3, 3, 5, 5, 7, 7, 9, 9,11 } },
	{ _TR("Neapolitan Minor"),       { 0, 1, 1, 1, 1, 5, 5, 7, 8, 8,10,10 } },
//	{ _TR("Mohammedan"),             { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Harmonic Minor"
	{ _TR("Overtone"),               { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9,10,10 } },
//	{ _TR("Diatonic"),               { 0, 0, 2, 2, 4, 4, 4, 7, 7, 9, 9, 9 } },	// identical to "Pentatonic Major"
//	{ _TR("Double Harmonic"),        { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Hungarian Gypsy Persian"
	{ _TR("Eight Tone Spanish"),     { 0, 1, 1, 3, 4, 5, 6, 6, 8, 8,10,10 } },
	{ _TR("Leading Whole Tone"),     { 0, 0, 2, 2, 4, 4, 6, 6, 8, 8,10,11 } },
	{ _TR("Nine Tone Scale"),        { 0, 0, 2, 3, 4, 4, 6, 7, 8, 9, 9,11 } },
	{ _TR("Dominant Seventh"),       { 0, 0, 2, 2, 2, 5, 5, 7, 7, 9,10,10 } },
	{ _TR("Augmented"),              { 0, 0, 0, 3, 4, 4, 4, 7, 8, 8, 8,11 } },
	{ _TR("Algerian"),               { 0, 0, 2, 3, 3, 5, 6, 7, 8, 8, 8,11 } },
	{ _TR("Arabian (A)"),            { 0, 0, 2, 3, 3, 5, 6, 6, 8, 9, 9,11 } },	// identical to "Octatonic (W-H)"
	{ _TR("Arabian (B)"),            { 0, 0, 2, 2, 4, 5, 6, 6, 8, 8,10,10 } },
//	{ _TR("Asavari Theta"),          { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Melodic Minor (Descending)"
	{ _TR("Balinese"),               { 0, 1, 1, 3, 3, 3, 3, 7, 8, 8, 8, 8 } },
//	{ _TR("Bilaval Theta"),          { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Major"
//	{ _TR("Bhairav Theta"),          { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Hungarian Gypsy Persian"
//	{ _TR("Bhairavi Theta"),         { 0, 1, 1, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Phrygian"
//	{ _TR("Byzantine"),              { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Hungarian Gypsy Persian"
	{ _TR("Chinese"),                { 0, 0, 0, 0, 4, 4, 6, 7, 7, 7, 7,11 } },
//	{ _TR("Chinese Mongolian"),      { 0, 0, 2, 2, 4, 4, 4, 7, 7, 9, 9, 9 } },	// identical to "Pentatonic Major"
	{ _TR("Diminished"),             { 0, 0, 2, 3, 3, 5, 6, 6, 8, 9, 9,11 } },	// identical to "Octatonic (W-H)"
//	{ _TR("Egyptian"),               { 0, 0, 2, 2, 2, 5, 5, 7, 7, 7,10,10 } },	// identical to "Pentatonic Neutral"
//	{ _TR("Ethiopian (A Raray)"),    { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Major"
//	{ _TR("Ethiopian (Geez & Ezel)"),{ 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Melodic Minor (Descending)"
//	{ _TR("Hawaiian"),               { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Melodic Minor (Ascending)"
//	{ _TR("Hindustan"),              { 0, 0, 2, 2, 4, 5, 5, 7, 8, 8,10,10 } },	// identical to "Hindu"
	{ _TR("Japanese (Ichikosucho)"), { 0, 0, 2, 2, 4, 5, 6, 7, 7, 9, 9,11 } },
	{ _TR("Japanese (Taishikicho)"), { 0, 0, 2, 2, 4, 5, 6, 7, 7, 9,10,11 } },
	{ _TR("Javaneese"),              { 0, 1, 1, 3, 3, 5, 5, 7, 7, 9,10,10 } },
//	{ _TR("Kafi Theta"),             { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9,10,10 } },	// identical to "Dorian"
//	{ _TR("Kalyan Theta"),           { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9, 9,11 } },	// identical to "Lydian"
//	{ _TR("Khamaj Theta"),           { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9,10,10 } },	// identical to "Mixolydian"
//	{ _TR("Madelynian"),             { 0, 1, 1, 3, 3, 5, 6, 6, 8, 8,10,10 } },	// identical to "Locrian"
	{ _TR("Marva Theta"),            { 0, 1, 1, 1, 4, 4, 6, 7, 7, 9, 9,11 } },
#ifdef QTRACTOR_MELA_SCALES
	{ _TR("Mela Bhavapriya"),        { 0, 1, 2, 2, 2, 5, 5, 7, 8, 9, 9, 9 } },
	{ _TR("Mela Chakravakam"),       { 0, 1, 1, 1, 4, 5, 5, 7, 7, 9,10,10 } },
	{ _TR("Mela Chalanata"),         { 0, 0, 0, 3, 4, 5, 5, 7, 7, 7,10,11 } },
//	{ _TR("Mela Charukesi"),         { 0, 0, 2, 2, 4, 5, 5, 7, 8, 8,10,10 } },	// identical to "Hindu"
	{ _TR("Mela Chitrambari"),       { 0, 0, 2, 2, 4, 4, 6, 7, 7, 7,10,11 } },
	{ _TR("Mela Dharmavati"),        { 0, 0, 2, 3, 3, 3, 6, 7, 7, 9, 9,11 } },
	{ _TR("Mela Dhatuvardhani"),     { 0, 0, 0, 3, 4, 4, 6, 7, 8, 8, 8,11 } },
	{ _TR("Mela Dhavalambari"),      { 0, 1, 1, 1, 4, 4, 6, 7, 8, 9, 9, 9 } },
//	{ _TR("Mela Dhenuka"),           { 0, 1, 1, 3, 3, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Neapolitan"
//	{ _TR("Mela Dhirasankarabharana"),{0, 0, 2, 2, 4, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Major"
	{ _TR("Mela Divyamani"),         { 0, 1, 1, 1, 4, 4, 6, 7, 7, 7,10,11 } },
//	{ _TR("Mela Gamanasrama"),       { 0, 1, 1, 1, 4, 4, 6, 7, 7, 9, 9,11 } },	// identical to "Marva Theta"
	{ _TR("Mela Ganamurti"),         { 0, 1, 2, 2, 2, 5, 5, 7, 8, 8, 8,11 } },
	{ _TR("Mela Gangeyabhusani"),    { 0, 0, 0, 3, 4, 5, 5, 7, 8, 8, 8,11 } },
//	{ _TR("Mela Gaurimanohari"),     { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Melodic Minor (Ascending)"
	{ _TR("Mela Gavambodhi"),        { 0, 1, 1, 3, 3, 3, 6, 7, 8, 9, 9, 9 } },
	{ _TR("Mela Gayakapriya"),       { 0, 1, 1, 1, 4, 5, 5, 7, 8, 9, 9, 9 } },
//	{ _TR("Mela Hanumattodi"),       { 0, 1, 1, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Phrygian"
//	{ _TR("Mela Harikambhoji"),      { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9,10,10 } },	// identical to "Mixolydian"
	{ _TR("Mela Hatakambari"),       { 0, 1, 1, 1, 4, 5, 5, 7, 7, 7,10,11 } },
//	{ _TR("Mela Hemavati"),          { 0, 0, 2, 3, 3, 3, 6, 7, 7, 9,10,10 } },	// identical to "Roumanian Minor"
	{ _TR("Mela Jalarnavam"),        { 0, 1, 2, 2, 2, 2, 6, 7, 8, 8,10,10 } },
	{ _TR("Mela Jhalavarali"),       { 0, 1, 2, 2, 2, 2, 6, 7, 8, 8, 8,11 } },
	{ _TR("Mela Jhankaradhvani"),    { 0, 0, 2, 3, 3, 5, 5, 7, 8, 9, 9, 9 } },
	{ _TR("Mela Jyotisvarupini"),    { 0, 0, 0, 3, 4, 4, 6, 7, 8, 8,10,10 } },
	{ _TR("Mela Kamavarardhani"),    { 0, 1, 1, 1, 4, 4, 6, 7, 8, 8, 8,11 } },
//	{ _TR("Mela Kanakangi"),         { 0, 1, 2, 2, 2, 5, 5, 7, 8, 9, 9, 9 } },	// identical to "Mela Bhavapriya"
	{ _TR("Mela Kantamani"),         { 0, 0, 2, 2, 4, 4, 6, 7, 8, 9, 9, 9 } },
//	{ _TR("Mela Kharaharapriya"),    { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9,10,10 } },	// identical to "Dorian"
//	{ _TR("Mela Kiravani"),          { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Harmonic Minor"
//	{ _TR("Mela Kokilapriya"),       { 0, 1, 1, 3, 3, 5, 5, 7, 7, 9, 9,11 } },	// identical to "Neapolitan Major"
	{ _TR("Mela Kosalam"),           { 0, 0, 0, 3, 4, 4, 6, 7, 7, 9, 9,11 } },
	{ _TR("Mela Latangi"),           { 0, 0, 2, 2, 4, 4, 6, 7, 8, 8, 8,11 } },
	{ _TR("Mela Manavati"),          { 0, 1, 2, 2, 2, 5, 5, 7, 7, 9, 9,11 } },
	{ _TR("Mela Mararanjani"),       { 0, 0, 2, 2, 4, 5, 5, 7, 8, 9, 9, 9 } },
//	{ _TR("Mela Mayamalavagaula"),   { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Hungarian Gypsy Persian"
//	{ _TR("Mela Mechakalyani"),      { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9, 9,11 } },	// identical to "Lydian"
	{ _TR("Mela Naganandini"),       { 0, 0, 2, 2, 4, 5, 5, 7, 7, 7,10,11 } },
	{ _TR("Mela Namanarayani"),      { 0, 1, 1, 1, 4, 4, 6, 7, 8, 8,10,10 } },
//	{ _TR("Mela Nasikabhusani"),     { 0, 0, 0, 3, 4, 4, 6, 7, 7, 9,10,10 } },	// identical to "Hungarian Major"
//	{ _TR("Mela Natabhairavi"),      { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Melodic Minor (Descending)"
//	{ _TR("Mela Natakapriya"),       { 0, 1, 1, 3, 3, 5, 5, 7, 7, 9,10,10 } },	// identical to "Javaneese"
	{ _TR("Mela Navanitam"),         { 0, 1, 2, 2, 2, 2, 6, 7, 7, 9,10,10 } },
	{ _TR("Mela Nitimati"),          { 0, 0, 2, 3, 3, 3, 6, 7, 7, 7,10,11 } },
	{ _TR("Mela Pavani"),            { 0, 1, 2, 2, 2, 2, 6, 7, 7, 9, 9,11 } },
	{ _TR("Mela Ragavardhani"),      { 0, 0, 0, 3, 4, 5, 5, 7, 8, 8,10,10 } },
	{ _TR("Mela Raghupriya"),        { 0, 1, 2, 2, 2, 2, 6, 7, 7, 7,10,11 } },
	{ _TR("Mela Ramapriya"),         { 0, 1, 1, 1, 4, 4, 6, 7, 7, 9,10,10 } },
	{ _TR("Mela Rasikapriya"),       { 0, 0, 0, 3, 4, 4, 6, 7, 7, 7,10,11 } },
	{ _TR("Mela Ratnangi"),          { 0, 1, 2, 2, 2, 5, 5, 7, 8, 8,10,10 } },
	{ _TR("Mela Risabhapriya"),      { 0, 0, 2, 2, 4, 4, 6, 7, 8, 8,10,10 } },
	{ _TR("Mela Rupavati"),          { 0, 1, 1, 3, 3, 5, 5, 7, 7, 7,10,11 } },
	{ _TR("Mela Sadvidhamargini"),   { 0, 1, 1, 3, 3, 3, 6, 7, 7, 9,10,10 } },
	{ _TR("Mela Salagam"),           { 0, 1, 2, 2, 2, 2, 6, 7, 8, 9, 9, 9 } },
	{ _TR("Mela Sanmukhapriya"),     { 0, 0, 2, 3, 3, 3, 6, 7, 8, 8,10,10 } },
	{ _TR("Mela Sarasangi"),         { 0, 0, 2, 2, 4, 5, 5, 7, 8, 8, 8,11 } },
	{ _TR("Mela Senavati"),          { 0, 1, 1, 3, 3, 5, 5, 7, 8, 9, 9, 9 } },
//	{ _TR("Mela Simhendramadhyama"), { 0, 0, 2, 3, 3, 3, 6, 7, 8, 8, 8,11 } },	// identical to "Hungarian Minor"
	{ _TR("Mela Subhapantuvarali"),  { 0, 1, 1, 3, 3, 3, 6, 7, 8, 8, 8,11 } },
	{ _TR("Mela Sucharitra"),        { 0, 0, 0, 3, 4, 4, 6, 7, 8, 9, 9, 9 } },
	{ _TR("Mela Sulini"),            { 0, 0, 0, 3, 4, 5, 5, 7, 7, 9, 9,11 } },
	{ _TR("Mela Suryakantam"),       { 0, 1, 1, 1, 4, 5, 5, 7, 7, 9, 9,11 } },
//	{ _TR("Mela Suvarnangi"),        { 0, 1, 2, 2, 2, 2, 6, 7, 7, 9, 9,11 } },	// identical to "Mela Pavani"
	{ _TR("Mela Syamalangi"),        { 0, 0, 2, 3, 3, 3, 6, 7, 8, 9, 9, 9 } },
	{ _TR("Mela Tanarupi"),          { 0, 1, 2, 2, 2, 5, 5, 7, 7, 7,10,11 } },
//	{ _TR("Mela Vaschaspati"),       { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9,10,10 } },	// identical to "Overtone"
	{ _TR("Mela Vagadhisvari"),      { 0, 0, 0, 3, 4, 5, 5, 7, 7, 9,10,10 } },
//	{ _TR("Mela Vakulabharanam"),    { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8,10,10 } },	// identical to "Jewish (Ahaba Rabba)"
	{ _TR("Mela Vanaspati"),         { 0, 1, 2, 2, 2, 5, 5, 7, 7, 9,10,10 } },
	{ _TR("Mela Varunapriya"),       { 0, 0, 2, 3, 3, 5, 5, 7, 7, 7,10,11 } },
//	{ _TR("Mela Visvambari"),        { 0, 1, 1, 1, 4, 4, 6, 7, 7, 7,10,11 } },	// identical to "Mela Divyamani"
	{ _TR("Mela Yagapriya"),         { 0, 0, 0, 3, 4, 5, 5, 7, 8, 9, 9, 9 } },
#endif	// QTRACTOR_MELA_SCALES
//	{ _TR("Mohammedan"),             { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Harmonic Minor"
	{ _TR("Persian"),                { 0, 1, 1, 1, 4, 5, 6, 6, 8, 8, 8,11 } },
	{ _TR("Purvi Theta"),            { 0, 1, 1, 1, 4, 4, 6, 7, 8, 8, 8,11 } },	// identical to "Mela Kamavarardhani"
	{ _TR("Spanish Gypsy"),          { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8,10,10 } },	// identical to "Jewish (Ahaba Rabba)"
	{ _TR("Todi Theta"),             { 0, 1, 1, 3, 3, 3, 6, 7, 8, 8, 8,11 } },	// identical to "Mela Subhapantuvarali"
//	{ _TR("Aux Diminished"),         { 0, 0, 2, 3, 3, 5, 6, 6, 8, 9, 9,11 } },	// identical to "Octatonic (W-H)"
//	{ _TR("Aux Augmented"),          { 0, 0, 2, 2, 4, 4, 6, 6, 8, 8,10,10 } },	// identical to "Whole Tone"
//	{ _TR("Aux Diminished Blues"),   { 0, 1, 1, 3, 4, 4, 6, 7, 7, 9,10,10 } },	// identical to "Octatonic (H-W)"
	{ _TR("Enigmatic"),              { 0, 1, 1, 1, 4, 4, 6, 6, 8, 8,10,11 } },
	{ _TR("Kumoi"),                  { 0, 0, 2, 3, 3, 3, 3, 7, 7, 9, 9, 9 } },
	{ _TR("Lydian Augmented"),       { 0, 0, 2, 2, 4, 4, 6, 6, 8, 9, 9,11 } },
	{ _TR("Pelog"),                  { 0, 1, 1, 3, 3, 3, 3, 7, 8, 8, 8, 8 } },	// identical to "Balinese"
	{ _TR("Prometheus"),             { 0, 0, 2, 2, 4, 4, 6, 6, 6, 9,10,10 } },
	{ _TR("Prometheus Neapolitan"),  { 0, 1, 1, 1, 4, 4, 6, 6, 6, 9,10,10 } },
	{ _TR("Six Tone Symmetrical"),   { 0, 1, 1, 1, 4, 5, 5, 5, 8, 9, 9, 9 } },
	{ _TR("Super Locrian"),          { 0, 1, 1, 3, 4, 4, 6, 6, 8, 8,10,10 } },
	{ _TR("Lydian Minor"),           { 0, 0, 2, 2, 4, 4, 6, 7, 8, 8,10,10 } },	// identical to "Mela Risabhapriya"
	{ _TR("Lydian Diminished"),      { 0, 0, 2, 3, 3, 3, 6, 7, 7, 9, 9,11 } },	// identical to "Mela Dharmavati"
//	{ _TR("Major Locrian"),          { 0, 0, 2, 2, 4, 5, 6, 6, 8, 8,10,10 } },	// identical to "Arabian (B)"
//	{ _TR("Hindu"),                  { 0, 0, 2, 2, 4, 5, 5, 7, 8, 8,10,10 } },	// identical to "Hindu"
//	{ _TR("Diminished Whole Tone"),  { 0, 1, 1, 3, 4, 4, 6, 6, 8, 8,10,10 } },	// identical to "Super Locrian" 
	{ _TR("Half Diminished"),        { 0, 0, 2, 3, 3, 5, 6, 6, 8, 8,10,10 } },
	{ _TR("Bhairav"),                { 0, 1, 1, 1, 4, 5, 5, 7, 8, 8, 8,11 } },	// identical to "Hungarian Gypsy Persian"
	{ _TR("Yaman"),                  { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9, 9,11 } },	// identical to "Lydian"
	{ _TR("Todi"),                   { 0, 1, 1, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Phrygian"
	{ _TR("Jog"),                    { 0, 0, 0, 3, 4, 5, 5, 7, 7, 7,10,10 } },
	{ _TR("Multani"),                { 0, 1, 1, 3, 3, 3, 6, 7, 8, 8, 8,11 } },	// identical to "Mela Subhapantuvarali"
	{ _TR("Darbari"),                { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 } },	// identical to "Melodic Minor (Descending)"
	{ _TR("Malkauns"),               { 0, 0, 0, 3, 3, 5, 5, 5, 8, 8,10,10 } },
	{ _TR("Bhoopali"),               { 0, 0, 2, 2, 4, 4, 4, 7, 7, 9, 9, 9 } },	// identical to "Pentatonic Major"
	{ _TR("Shivaranjani"),           { 0, 0, 2, 3, 3, 3, 3, 7, 7, 9, 9, 9 } },	// identical to "Kumoi"
	{ _TR("Marwa"),                  { 0, 1, 1, 1, 4, 4, 6, 6, 6, 9, 9,11 } },
//	{ _TR("Blues"),                  { 0, 0, 0, 3, 3, 5, 6, 7, 7, 7,10,10 } },  // identical to "Pentatonic Blues"
	{ _TR("Minor 5"),                { 0, 0, 0, 3, 3, 5, 5, 7, 7, 7,10,10 } },	// identical to "Pentatonic Minor"
	{ _TR("Major 5"),                { 0, 0, 0, 0, 4, 5, 5, 7, 7, 7, 7,11 } },
	{ _TR("5"),                      { 0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7 } },
	{ _TR("45"),                     { 0, 0, 0, 0, 0, 5, 5, 7, 7, 7, 7, 7 } },
	{ _TR("457"),                    { 0, 0, 0, 0, 0, 5, 5, 7, 7, 7,10,10 } },
	{ _TR("M 6"),                    { 0, 0, 2, 3, 3, 5, 5, 7, 7, 7,10,11 } },	// identical to "Mela Varunapriya"

	{ NULL,                          { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }
};


// Default scale key note names accessor.
const QStringList& qtractorMidiEditor::scaleKeyNames (void)
{
	static QStringList s_scaleKeys;

	if (s_scaleKeys.isEmpty()) {
		for (int i = 0; i < 12; ++i)
			s_scaleKeys.append(QObject::tr(g_aNoteNames[i].name, "scaleKeyName"));
	}

	return s_scaleKeys;
}

// Default scale type names table accessor.
const QStringList& qtractorMidiEditor::scaleTypeNames (void)
{
	static QStringList s_scaleTypes;

	if (s_scaleTypes.isEmpty()) {
		for (int i = 0; g_aScaleTab[i].name; ++i)
			s_scaleTypes.append(QObject::tr(g_aScaleTab[i].name, "scaleTypeName"));
	}

	return s_scaleTypes;
}

// Scale quantizer method.
unsigned char qtractorMidiEditor::snapToScale (
	unsigned char note, int iKey, int iScale )
{
	const int n = int(note) + (12 - iKey);
	return 12 * ((n / 12) - 1) + iKey + int(g_aScaleTab[iScale].note[n % 12]);
}


//----------------------------------------------------------------------------
// qtractorMidiEdit::ClipBoard - MIDI editor clipaboard singleton.

// Singleton declaration.
qtractorMidiEditor::ClipBoard qtractorMidiEditor::g_clipboard;


//----------------------------------------------------------------------------
// qtractorMidiEdit::DragTimeScale - Specialized drag/time-scale (draft)...

struct qtractorMidiEditor::DragTimeScale
{
	DragTimeScale(qtractorTimeScale *ts, unsigned long offset)
		: cursor(ts)
	{
		node = cursor.seekFrame(offset);
		t0 = node->tickFromFrame(offset);
		x0 = ts->pixelFromFrame(offset);
	}

	qtractorTimeScale::Cursor cursor;
	qtractorTimeScale::Node *node;
	unsigned long t0;
	int x0;
};


//----------------------------------------------------------------------------
// qtractorMidiEditor -- The main MIDI sequence editor widget.


// Constructor.
qtractorMidiEditor::qtractorMidiEditor ( QWidget *pParent )
		: QSplitter(Qt::Vertical, pParent)
{
	// Initialize instance variables...
	m_pMidiClip = NULL;

	// Event fore/background colors.
	m_foreground = Qt::darkBlue;
	m_background = Qt::blue;

	// Common drag state.
	m_dragState  = DragNone;
	m_dragCursor = DragNone;
	m_resizeMode = ResizeNone;

	m_pEventDrag = NULL;
	m_bEventDragEdit = false;

	m_pRubberBand = NULL;

	// Zoom mode flag.
	m_iZoomMode = ZoomAll;

	// Edit mode flags.
	m_bEditMode = false;
	m_bEditModeDraw = false;

	// Snap-to-beat/bar grid/zebra mode.
	m_bSnapZebra = false;
	m_bSnapGrid  = false;

	// Floating tool-tips mode.
	m_bToolTips = true;

	// Last default editing values.
	m_last.note      = 0x3c;	// middle-C
	m_last.value     = 0x40;
	m_last.pitchBend = 0;
	m_last.duration  = 0;

	// The local mighty command pattern instance.
	m_pCommands = new qtractorCommandList();

	// Local time-scale.
	m_pTimeScale = new qtractorTimeScale();

	// The original clip time-scale length/time.
	m_iClipLengthTime = 0;

	// The local time-scale offset/length.
	m_iOffset = 0;
	m_iLength = 0;

	// Local edit-head/tail positioning.
	m_iEditHead  = 0;
	m_iEditHeadX = 0;
	m_iEditTail  = 0;
	m_iEditTailX = 0;

	// Local play-head positioning.
	m_iPlayHead  = 0;
	m_iPlayHeadX = 0;
	m_bSyncView  = false;

	// Note autition while editing.
	m_bSendNotes = false;

	// Event (note) duration rectangle vs. stick.
	m_bNoteDuration = false;

	// Event (note, velocity) coloring.
	m_bNoteColor  = false;
	m_bValueColor = false;

	// Which widget holds focus on drag-paste?
	m_pEditPaste = NULL;

	// Snap-to-scale (aka.in-place scale-quantize) stuff.
	m_iSnapToScaleKey  = 0;
	m_iSnapToScaleType = 0;

	// Temporary sync-view/follow-playhead hold state.
	m_bSyncViewHold = false;
	m_iSyncViewHold = 0;

	// Create child frame widgets...
	QSplitter *pSplitter = new QSplitter(Qt::Horizontal, this);
	QWidget *pVBoxLeft   = new QWidget(pSplitter);
	QWidget *pVBoxRight  = new QWidget(pSplitter);
	QWidget *pHBoxBottom = new QWidget(this);

	// Create child view widgets...
	m_pEditListHeader = new QFrame(pVBoxLeft);
	m_pEditListHeader->setFixedHeight(20);
	m_pEditList  = new qtractorMidiEditList(this, pVBoxLeft);
	m_pEditList->setMinimumWidth(32);
	m_pEditTime  = new qtractorMidiEditTime(this, pVBoxRight);
	m_pEditTime->setFixedHeight(20);
	m_pEditView  = new qtractorMidiEditView(this, pVBoxRight);
	m_pEditEventScale = new qtractorMidiEditEventScale(this, pHBoxBottom);
	m_pEditEvent = new qtractorMidiEditEvent(this, pHBoxBottom);
	m_pEditEventFrame = new QFrame(pHBoxBottom);
	m_pEditList->updateContentsHeight();

	m_pThumbView = new qtractorMidiThumbView(this);

	// Create child box layouts...
	QVBoxLayout *pVBoxLeftLayout = new QVBoxLayout(pVBoxLeft);
	pVBoxLeftLayout->setMargin(0);
	pVBoxLeftLayout->setSpacing(0);
	pVBoxLeftLayout->addWidget(m_pEditListHeader);
	pVBoxLeftLayout->addWidget(m_pEditList);
	pVBoxLeft->setLayout(pVBoxLeftLayout);

	QVBoxLayout *pVBoxRightLayout = new QVBoxLayout(pVBoxRight);
	pVBoxRightLayout->setMargin(0);
	pVBoxRightLayout->setSpacing(0);
	pVBoxRightLayout->addWidget(m_pEditTime);
	pVBoxRightLayout->addWidget(m_pEditView);
	pVBoxRight->setLayout(pVBoxRightLayout);

	QHBoxLayout *pHBoxBottomLayout = new QHBoxLayout(pHBoxBottom);
	pHBoxBottomLayout->setMargin(0);
	pHBoxBottomLayout->setSpacing(0);
	pHBoxBottomLayout->addWidget(m_pEditEventScale);
	pHBoxBottomLayout->addWidget(m_pEditEvent);
	pHBoxBottomLayout->addWidget(m_pEditEventFrame);
	pHBoxBottom->setLayout(pHBoxBottomLayout);

//	pSplitter->setOpaqueResize(false);
	pSplitter->setStretchFactor(pSplitter->indexOf(pVBoxLeft), 0);
	pSplitter->setHandleWidth(2);

//	QSplitter::setOpaqueResize(false);
	QSplitter::setStretchFactor(QSplitter::indexOf(pHBoxBottom), 0);
	QSplitter::setHandleWidth(2);

	QSplitter::setWindowIcon(QIcon(":/images/qtractorMidiEditor.png"));
	QSplitter::setWindowTitle(tr("MIDI Editor"));

	// To have all views in positional sync.
	QObject::connect(m_pEditList, SIGNAL(contentsMoving(int,int)),
		m_pEditView, SLOT(contentsYMovingSlot(int,int)));
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditList, SLOT(contentsYMovingSlot(int,int)));
	QObject::connect(m_pEditView, SIGNAL(contentsMoving(int,int)),
		m_pEditEvent, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pEditEvent, SIGNAL(contentsMoving(int,int)),
		m_pEditTime, SLOT(contentsXMovingSlot(int,int)));
	QObject::connect(m_pEditEvent, SIGNAL(contentsMoving(int,int)),
		m_pEditView, SLOT(contentsXMovingSlot(int,int)));

	QObject::connect(m_pEditView,
		SIGNAL(contentsMoving(int,int)),
		m_pThumbView, SLOT(updateThumb()));

	QObject::connect(m_pCommands,
		SIGNAL(updateNotifySignal(unsigned int)),
		SLOT(updateNotifySlot(unsigned int)));

	// FIXME: Initial horizontal splitter sizes.
	QList<int> sizes;
	sizes.append(48);
	sizes.append(752);
	pSplitter->setSizes(sizes);
}


// Destructor.
qtractorMidiEditor::~qtractorMidiEditor (void)
{
	resetDragState(NULL);

	// Release local instances.
	delete m_pTimeScale;
	delete m_pCommands;
}


// Editing sequence accessor.
void qtractorMidiEditor::setMidiClip ( qtractorMidiClip *pMidiClip )
{
	// So, this is the brand new object to edit...
	m_pMidiClip = pMidiClip;

	if (m_pMidiClip) {
		// Now set the editing MIDI sequence alright...
		setClipLength(m_pMidiClip->clipLength());
		setOffset(m_pMidiClip->clipStart());
		setLength(m_pMidiClip->clipLength());
		// Set its most outstanding properties...
		qtractorTrack *pTrack = m_pMidiClip->track();
		if (pTrack) {
			setForeground(pTrack->foreground());
			setBackground(pTrack->background());
		}
		// And the last but not least...
		qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
		if (pSeq) {
			// Reset some internal state...
			m_cursor.reset(pSeq);
			m_cursorAt.reset(pSeq);
			// Reset as last on middle note and snap duration...
			m_last.note = (pSeq->noteMin() + pSeq->noteMax()) >> 1;
			if (m_last.note == 0)
				m_last.note = 0x3c; // Default to middle-C.
		}
		// Got clip!
	} else {
		// Reset those little things too..
		setClipLength(0);
		setOffset(0);
		setLength(0);
	}

	// All commands reset.
	m_pCommands->clear();
}

qtractorMidiClip *qtractorMidiEditor::midiClip (void) const
{
	return m_pMidiClip;
}


// MIDI clip property accessors.
const QString& qtractorMidiEditor::filename (void) const
{
	return m_pMidiClip->filename();
}

unsigned short qtractorMidiEditor::trackChannel (void) const
{
	return (m_pMidiClip ? m_pMidiClip->trackChannel() : 0);
}

unsigned short qtractorMidiEditor::format (void) const
{
	return (m_pMidiClip ? m_pMidiClip->format() : 0);
}


qtractorMidiSequence *qtractorMidiEditor::sequence (void) const
{
	return (m_pMidiClip ? m_pMidiClip->sequence() : NULL);
}


// Event foreground (outline) color.
void qtractorMidiEditor::setForeground ( const QColor& fore )
{
	m_foreground = fore;
}

const QColor& qtractorMidiEditor::foreground (void) const
{
	return m_foreground;
}


// Event background (fill) color.
void qtractorMidiEditor::setBackground ( const QColor& back )
{
	m_background = back;
}

const QColor& qtractorMidiEditor::background (void) const
{
	return m_background;
}


// Zoom (view) mode.
void qtractorMidiEditor::setZoomMode ( int iZoomMode )
{
	m_iZoomMode = iZoomMode;
}

int qtractorMidiEditor::zoomMode (void) const
{
	return m_iZoomMode;
}



// Zoom ratio accessors.
void qtractorMidiEditor::setHorizontalZoom ( unsigned short iHorizontalZoom )
{
	m_pTimeScale->setHorizontalZoom(iHorizontalZoom);
	m_pTimeScale->updateScale();
}

unsigned short qtractorMidiEditor::horizontalZoom (void) const
{
	return m_pTimeScale->horizontalZoom();
}


void qtractorMidiEditor::setVerticalZoom ( unsigned short iVerticalZoom )
{
	// Hold and try setting new item height...
	int iZoomStep = int(iVerticalZoom) - int(verticalZoom());
	int iItemHeight
		= (iVerticalZoom * qtractorMidiEditList::ItemHeightBase) / 100;
	if (iItemHeight < qtractorMidiEditList::ItemHeightMax && iZoomStep > 0)
		++iItemHeight;
	else
	if (iItemHeight > qtractorMidiEditList::ItemHeightMin && iZoomStep < 0)
		--iItemHeight;
	m_pEditList->setItemHeight(iItemHeight);

	m_pTimeScale->setVerticalZoom(iVerticalZoom);
}

unsigned short qtractorMidiEditor::verticalZoom (void) const
{
	return m_pTimeScale->verticalZoom();
}


// Edit (creational) mode.
void qtractorMidiEditor::setEditMode ( bool bEditMode )
{
	resetDragState(NULL);

	m_bEditMode = bEditMode;

//	updateContents();
}

bool qtractorMidiEditor::isEditMode (void) const
{
	return m_bEditMode;
}


// Edit draw (notes) mode.
void qtractorMidiEditor::setEditModeDraw ( bool bEditModeDraw )
{
	m_bEditModeDraw = bEditModeDraw;
}

bool qtractorMidiEditor::isEditModeDraw (void) const
{
	return m_bEditModeDraw;
}


// Snap-to-bar zebra mode.
void qtractorMidiEditor::setSnapZebra ( bool bSnapZebra )
{
	m_bSnapZebra = bSnapZebra;

//	updateContents();
}

bool qtractorMidiEditor::isSnapZebra (void) const
{
	return m_bSnapZebra;
}


// Snap-to-beat grid mode.
void qtractorMidiEditor::setSnapGrid ( bool bSnapGrid )
{
	m_bSnapGrid = bSnapGrid;

//	updateContents();
}

bool qtractorMidiEditor::isSnapGrid (void) const
{
	return m_bSnapGrid;
}


// Floating tool-tips mode.
void qtractorMidiEditor::setToolTips ( bool bToolTips )
{
	m_bToolTips = bToolTips;
}

bool qtractorMidiEditor::isToolTips (void) const
{
	return m_bToolTips;
}


// Local time scale accessor.
qtractorTimeScale *qtractorMidiEditor::timeScale (void) const
{
	return m_pTimeScale;
}

unsigned long qtractorMidiEditor::timeOffset (void) const
{
	return (m_pTimeScale ? m_pTimeScale->tickFromFrame(m_iOffset) : 0);
}


// The original clip time-scale length/time.
void qtractorMidiEditor::setClipLength ( unsigned long iClipLength )
{
	if (m_pTimeScale) {
		m_iClipLengthTime
			= m_pTimeScale->tickFromFrame(m_iOffset + iClipLength)
			- m_pTimeScale->tickFromFrame(m_iOffset);
	} else {
		m_iClipLengthTime = 0;
	}
}

unsigned long qtractorMidiEditor::clipLength (void) const
{
	if (m_pTimeScale == NULL)
		return 0;

	return m_pTimeScale->frameFromTick(
		m_pTimeScale->tickFromFrame(m_iOffset) + m_iClipLengthTime) - m_iOffset;
}


// Reset original clip time-scale length/time.
void qtractorMidiEditor::resetClipLength (void)
{
	if (m_pMidiClip) setClipLength(m_pMidiClip->clipLength());
}


// Time-scale offset (in frames) accessors.
void qtractorMidiEditor::setOffset ( unsigned long iOffset )
{
	m_iOffset = iOffset;
}

unsigned long qtractorMidiEditor::offset (void) const
{
	return m_iOffset;
}


// Time-scale length (in frames) accessors.
void qtractorMidiEditor::setLength ( unsigned long iLength )
{
	m_iLength = iLength;
}

unsigned long qtractorMidiEditor::length (void) const
{
	return m_iLength;
}


// Edit-head/tail positioning.
void qtractorMidiEditor::setEditHead ( unsigned long iEditHead, bool bSyncView )
{
	if (iEditHead > m_iEditTail)
		setEditTail(iEditHead, bSyncView);
	else
		setSyncViewHoldOn(true);

	if (bSyncView) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->setEditHead(iEditHead);
	}

	m_iEditHead = iEditHead;
	int iEditHeadX
		= m_pTimeScale->pixelFromFrame(iEditHead)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iEditHeadX, iEditHeadX, bSyncView);
}

unsigned long qtractorMidiEditor::editHead (void) const
{
	return m_iEditHead;
}

int qtractorMidiEditor::editHeadX (void) const
{
	return m_iEditHeadX;
}


void qtractorMidiEditor::setEditTail ( unsigned long iEditTail, bool bSyncView )
{
	if (iEditTail < m_iEditHead)
		setEditHead(iEditTail, bSyncView);
	else
		setSyncViewHoldOn(true);

	if (bSyncView) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession)
			pSession->setEditTail(iEditTail);
	}

	m_iEditTail = iEditTail;
	int iEditTailX
		= m_pTimeScale->pixelFromFrame(iEditTail)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iEditTailX, iEditTailX, bSyncView);
}

unsigned long qtractorMidiEditor::editTail (void) const
{
	return m_iEditTail;
}

int qtractorMidiEditor::editTailX (void) const
{
	return m_iEditTailX;
}


// Play-head positioning.
void qtractorMidiEditor::setPlayHead ( unsigned long iPlayHead, bool bSyncView )
{
	if (bSyncView)
		bSyncView = m_bSyncView;

	m_iPlayHead = iPlayHead;
	int iPlayHeadX
		= m_pTimeScale->pixelFromFrame(iPlayHead)
		- m_pTimeScale->pixelFromFrame(m_iOffset);

	drawPositionX(m_iPlayHeadX, iPlayHeadX, bSyncView);
}

unsigned long qtractorMidiEditor::playHead (void) const
{
	return m_iPlayHead;
}

int qtractorMidiEditor::playHeadX (void) const
{
	return m_iPlayHeadX;
}


// Update time-scale to master session.
void qtractorMidiEditor::updateTimeScale (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (m_pTimeScale == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	m_pTimeScale->sync(*pSession->timeScale());

	setOffset(m_pMidiClip->clipStart());
	setLength(m_pMidiClip->clipLength());

	setPlayHead(pSession->playHead(), false);
	setEditHead(pSession->editHead(), false);
	setEditTail(pSession->editTail(), false);
}


// Play-head follow-ness.
void qtractorMidiEditor::setSyncView ( bool bSyncView )
{
	m_bSyncView = bSyncView;
	m_iSyncViewHold = 0;
}

bool qtractorMidiEditor::isSyncView (void) const
{
	return m_bSyncView;
}


// Note autition while editing.
void qtractorMidiEditor::setSendNotes ( bool bSendNotes )
{
	m_bSendNotes = bSendNotes;
}

bool qtractorMidiEditor::isSendNotes (void) const
{
	return m_bSendNotes;
}


// Event value stick vs. duration rectangle.
void qtractorMidiEditor::setNoteDuration ( bool bNoteDuration )
{
	m_bNoteDuration = bNoteDuration;
}

bool qtractorMidiEditor::isNoteDuration (void) const
{
	return m_bNoteDuration;
}


// Event (note, velocity) coloring.
void qtractorMidiEditor::setNoteColor ( bool bNoteColor )
{
	m_bNoteColor = bNoteColor;
}

bool qtractorMidiEditor::isNoteColor (void) const
{
	return m_bNoteColor;
}


void qtractorMidiEditor::setValueColor ( bool bValueColor )
{
	m_bValueColor = bValueColor;
}

bool qtractorMidiEditor::isValueColor (void) const
{
	return m_bValueColor;
}


// Snap-to-scale/quantize key accessor.
void qtractorMidiEditor::setSnapToScaleKey ( int iSnapToScaleKey )
{
	m_iSnapToScaleKey = iSnapToScaleKey;
}

int qtractorMidiEditor::snapToScaleKey (void) const
{
	return m_iSnapToScaleKey;
}


// Snap-to-scale/quantize type accessor.
void qtractorMidiEditor::setSnapToScaleType ( int iSnapToScaleType )
{
	m_iSnapToScaleType = iSnapToScaleType;
}

int qtractorMidiEditor::snapToScaleType (void) const
{
	return m_iSnapToScaleType;
}


// Vertical line position drawing.
void qtractorMidiEditor::drawPositionX ( int& iPositionX, int x, bool bSyncView )
{
	// Update track-view position...
	const int x0 = m_pEditView->contentsX();
	const int w  = m_pEditView->width();
	const int h  = m_pEditView->height();
	const int h2 = m_pEditEvent->height();
	const int wm = (w >> 3);

	// Time-line header extents...
	const int h0 = m_pEditTime->height();
	const int d0 = (h0 >> 1);

	// Restore old position...
	int x1 = iPositionX - x0;
	if (iPositionX != x && x1 >= 0 && x1 < w + d0) {
		// Override old view line...
		(m_pEditEvent->viewport())->update(QRect(x1, 0, 1, h2));
		(m_pEditView->viewport())->update(QRect(x1, 0, 1, h));
		(m_pEditTime->viewport())->update(QRect(x1 - d0, d0, h0, d0));
	}

	// New position is in...
	iPositionX = x;

	// Force position to be in view?
	if (bSyncView && (x < x0 || x > x0 + w - wm)
		&& m_dragState == DragNone && m_dragCursor == DragNone
	//	&& QApplication::mouseButtons() == Qt::NoButton
		&& --m_iSyncViewHold < 0) {
		// Move it...
		m_pEditView->setContentsPos(x - wm, m_pEditView->contentsY());
		m_iSyncViewHold = 0;
	} else {
		// Draw the line, by updating the new region...
		x1 = x - x0;
		if (x1 >= 0 && x1 < w + d0) {
			(m_pEditEvent->viewport())->update(QRect(x1, 0, 1, h2));
			(m_pEditView->viewport())->update(QRect(x1, 0, 1, h));
			(m_pEditTime->viewport())->update(QRect(x1 - d0, d0, h0, d0));
		}
	}
}


// Child widgets accessors.
QFrame *qtractorMidiEditor::editListHeader (void) const
{
	return m_pEditListHeader;
}

qtractorMidiEditList *qtractorMidiEditor::editList (void) const
{
	return m_pEditList;
}

qtractorMidiEditTime *qtractorMidiEditor::editTime (void) const
{
	return m_pEditTime;
}

qtractorMidiEditView *qtractorMidiEditor::editView (void) const
{
	return m_pEditView;
}

qtractorMidiEditEvent *qtractorMidiEditor::editEvent (void) const
{
	return m_pEditEvent;
}

qtractorMidiEditEventScale *qtractorMidiEditor::editEventScale (void) const
{
	return m_pEditEventScale;
}

QFrame *qtractorMidiEditor::editEventFrame (void) const
{
	return m_pEditEventFrame;
}


qtractorMidiThumbView *qtractorMidiEditor::thumbView (void) const
{
	return m_pThumbView;
}


// Horizontal zoom factor.
void qtractorMidiEditor::horizontalZoomStep ( int iZoomStep )
{
	int iHorizontalZoom = horizontalZoom() + iZoomStep;
	if (iHorizontalZoom < ZoomMin)
		iHorizontalZoom = ZoomMin;
	else if (iHorizontalZoom > ZoomMax)
		iHorizontalZoom = ZoomMax;
	if (iHorizontalZoom == horizontalZoom())
		return;

	// Fix the local horizontal view zoom.
	setHorizontalZoom(iHorizontalZoom);
}


// Vertical zoom factor.
void qtractorMidiEditor::verticalZoomStep ( int iZoomStep )
{
	int iVerticalZoom = verticalZoom() + iZoomStep;
	if (iVerticalZoom < ZoomMin)
		iVerticalZoom = ZoomMin;
	else if (iVerticalZoom > ZoomMax)
		iVerticalZoom = ZoomMax;
	if (iVerticalZoom == verticalZoom())
		return;

	// Fix the local vertical view zoom.
	setVerticalZoom(iVerticalZoom);
}


// Zoom view slots.
void qtractorMidiEditor::zoomIn (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	if (m_iZoomMode & ZoomHorizontal)
		horizontalZoomStep(+ ZoomStep);
	if (m_iZoomMode & ZoomVertical)
		verticalZoomStep(+ ZoomStep);

	zoomCenterPost(zc);
}

void qtractorMidiEditor::zoomOut (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	if (m_iZoomMode & ZoomHorizontal)
		horizontalZoomStep(- ZoomStep);
	if (m_iZoomMode & ZoomVertical)
		verticalZoomStep(- ZoomStep);

	zoomCenterPost(zc);
}


void qtractorMidiEditor::zoomReset (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	if (m_iZoomMode & ZoomHorizontal)
		horizontalZoomStep(ZoomBase - m_pTimeScale->horizontalZoom());
	if (m_iZoomMode & ZoomVertical)
		verticalZoomStep(ZoomBase - m_pTimeScale->verticalZoom());

	zoomCenterPost(zc);
}


void qtractorMidiEditor::horizontalZoomInSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(+ ZoomStep);
	zoomCenterPost(zc);
}

void qtractorMidiEditor::horizontalZoomOutSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(- ZoomStep);
	zoomCenterPost(zc);
}


void qtractorMidiEditor::verticalZoomInSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(+ ZoomStep);
	zoomCenterPost(zc);
}

void qtractorMidiEditor::verticalZoomOutSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(- ZoomStep);
	zoomCenterPost(zc);
}


void qtractorMidiEditor::horizontalZoomResetSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	horizontalZoomStep(ZoomBase - m_pTimeScale->horizontalZoom());
	zoomCenterPost(zc);
}

void qtractorMidiEditor::verticalZoomResetSlot (void)
{
	ZoomCenter zc;
	zoomCenterPre(zc);

	verticalZoomStep(ZoomBase - m_pTimeScale->verticalZoom());
	zoomCenterPost(zc);
}


// Alterrnate command action update helper...
void qtractorMidiEditor::updateUndoAction ( QAction *pAction ) const
{
	m_pCommands->updateAction(pAction, m_pCommands->lastCommand());
}

void qtractorMidiEditor::updateRedoAction ( QAction *pAction ) const
{
	m_pCommands->updateAction(pAction, m_pCommands->nextCommand());
}


// Tell whether we can undo last command...
bool qtractorMidiEditor::canUndo (void) const
{
	return (m_pCommands->lastCommand() != NULL);
}

// Tell whether we can redo last command...
bool qtractorMidiEditor::canRedo (void) const
{
	return (m_pCommands->nextCommand() != NULL);
}


// Undo last edit command.
void qtractorMidiEditor::undoCommand (void)
{
	m_pCommands->undo();
}


// Redo last edit command.
void qtractorMidiEditor::redoCommand (void)
{
	m_pCommands->redo();
}


// Whether there's any item currently selected.
bool qtractorMidiEditor::isSelected (void) const
{
	return (m_select.items().count() > 0);
}


// Whether there's any item on the clipboard.
bool qtractorMidiEditor::isClipboard (void)
{
	// Tell whether there's any item on the clipboard.
	return (g_clipboard.items.count() > 0);
}


// Cut current selection to clipboard.
void qtractorMidiEditor::cutClipboard (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isSelected())
		return;

	g_clipboard.clear();

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("cut"));

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		g_clipboard.items.append(new qtractorMidiEvent(*pEvent));
		pEditCommand->removeEvent(pEvent);
	}

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);
}


// Copy current selection to clipboard.
void qtractorMidiEditor::copyClipboard (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isSelected())
		return;

	g_clipboard.clear();

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter)
		g_clipboard.items.append(new qtractorMidiEvent(*iter.key()));

	selectionChangeNotify();
}


// Retrieve current paste period.
// (as from current clipboard width)
unsigned long qtractorMidiEditor::pastePeriod (void) const
{
	const unsigned long t0 = m_pTimeScale->tickFromFrame(m_iOffset);

	unsigned long t1 = 0;
	unsigned long t2 = 0;

	int k = 0;
	QListIterator<qtractorMidiEvent *> iter(g_clipboard.items);
	while (iter.hasNext()) {
		qtractorMidiEvent *pEvent = iter.next();
		unsigned long t = t0 + pEvent->time();
		if (t1 > t || k == 0)
			t1 = t;
		t += pEvent->duration();
		if (t2 < t)
			t2 = t;
		++k;
	}

	return m_pTimeScale->frameFromTick(t2) - m_pTimeScale->frameFromTick(t1);
}


// Paste from clipboard.
void qtractorMidiEditor::pasteClipboard (
	unsigned short iPasteCount, unsigned long iPastePeriod )
{
	if (m_pMidiClip == NULL)
		return;

	if (!isClipboard())
		return;

	// Reset any current selection, whatsoever...
	clearSelect();
	resetDragState(NULL);

	// Multi-paste period...
	if (iPastePeriod < 1)
		iPastePeriod = pastePeriod();

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);
	unsigned long t0 = pNode->tickFromFrame(m_iOffset);

	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);
	const int dx = m_pTimeScale->pixelFromFrame(iPastePeriod);
	
	// This is the edit-view spacifics...
	const int h1 = m_pEditList->itemHeight();
	const int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	int k, x1;
	const unsigned long d0 = t0;
	QListIterator<qtractorMidiEvent *> iter(g_clipboard.items);
	for (unsigned short i = 0; i < iPasteCount; ++i) {
		iter.toFront();
		k = x1 = 0;
		while (iter.hasNext()) {
			qtractorMidiEvent *pEvent = iter.next();
			// Common event coords...
			int y;
			unsigned long t1 = t0 + pEvent->time();
			unsigned long t2 = t1 + pEvent->duration();
			pNode = cursor.seekTick(t1);
			int x  = pNode->pixelFromTick(t1) - 1;
			int w1 = pNode->pixelFromTick(t2) - x;
			if (w1 < 5)
				w1 = 5;
			// View item...
			QRect rectView;
			if (pEvent->type() == m_pEditView->eventType()) {
				y = ch - h1 * (pEvent->note() + 1);
				rectView.setRect(x - x0, y, w1, h1);
			}
			// Event item...
			QRect rectEvent;
			const qtractorMidiEvent::EventType etype = pEvent->type();
			if (etype == m_pEditEvent->eventType()) {
				if (etype == qtractorMidiEvent::REGPARAM    ||
					etype == qtractorMidiEvent::NONREGPARAM ||
					etype == qtractorMidiEvent::CONTROL14)
					y = y0 - (y0 * pEvent->value()) / 16384;
				else
				if (etype == qtractorMidiEvent::PITCHBEND)
					y = y0 - (y0 * pEvent->pitchBend()) / 8192;
				else
					y = y0 - (y0 * pEvent->value()) / 128;
				if (!m_bNoteDuration)
					w1 = 5;
				if (y < y0)
					rectEvent.setRect(x - x0, y, w1, y0 - y);
				else if (y > y0)
					rectEvent.setRect(x - x0, y0, w1, y - y0);
				else
					rectEvent.setRect(x - x0, y0 - 2, w1, 4);
			}
			m_select.addItem(pEvent, rectEvent, rectView, t0 - d0);
			if (x1 > x || k == 0)
				x1 = x;
			++k;
		}
		pNode = cursor.seekTick(x1 + dx);
		t0 += pNode->tickFromPixel(x1 + dx);
		pNode = cursor.seekTick(x1);
		t0 -= pNode->tickFromPixel(x1);
	}

	// Make sure we've a anchor...
	if (m_pEventDrag == NULL)
		m_pEventDrag = m_select.anchorEvent();

	// Formally ellect this one as the target view...
	qtractorScrollView *pScrollView = NULL;
	qtractorMidiEditSelect::Item *pItem = m_select.findItem(m_pEventDrag);
	if (pItem) {
		if (m_pEventDrag->type() == m_pEditView->eventType()) {
			m_rectDrag  = pItem->rectView;
			pScrollView = m_pEditView;
		} else {
			m_rectDrag  = pItem->rectEvent;
			pScrollView = m_pEditEvent;
		}
	}

	// That's right :)
	if (pScrollView == NULL) {
		m_dragState = DragStep; // HACK: Force selection clearance!
		clearSelect();
		resetDragState(NULL);
		return;
	}
	
	// We'll start a brand new floating state...
	m_dragState = m_dragCursor = DragPaste;
	m_posDrag   = m_rectDrag.topLeft();
	m_posStep   = QPoint(0, 0);

	// This is the one which is holding focus on drag-paste.
	m_pEditPaste = pScrollView;

	// It doesn't matter which one, both pasteable views are due...
	QCursor cursr(QPixmap(":/images/editPaste.png"), 20, 20);
	m_pEditView->setCursor(cursr);
	m_pEditEvent->setCursor(cursr);

	// Make sure the mouse pointer is properly located...
	const QPoint& pos = pScrollView->viewportToContents(
		pScrollView->viewport()->mapFromGlobal(QCursor::pos()));

	// Let's-a go...
	updateDragMove(pScrollView, pos + m_posStep);
}


// Execute event removal.
void qtractorMidiEditor::deleteSelect (void)
{
	if (m_pMidiClip == NULL)
		return;

	if (!isSelected())
		return;

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("delete"));

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter)
		pEditCommand->removeEvent(iter.key());

	m_pCommands->exec(pEditCommand);
}


// Select all/none contents.
void qtractorMidiEditor::selectAll (
	qtractorScrollView *pScrollView, bool bSelect, bool bToggle )
{
	// Select all/none view contents.
	if (bSelect) {
		const QRect rect(0, 0,
			pScrollView->contentsWidth(),
			pScrollView->contentsHeight());
		selectRect(pScrollView,	rect, bToggle, true);
	} else {
		clearSelect();
		resetDragState(pScrollView);
		selectionChangeNotify();
	}

	// Make sure main view keeps focus...
	QWidget::activateWindow();
	pScrollView->setFocus();
}


// Select range view contents.
void qtractorMidiEditor::selectRange (
	qtractorScrollView *pScrollView, bool bToggle, bool bCommit )
{
	const int x = m_iEditHeadX;
	const int y = 0;
	const int w = m_iEditTailX - m_iEditHeadX;
	const int h = pScrollView->contentsHeight();

	selectRect(pScrollView, QRect(x, y, w, h), bToggle, bCommit);
}


// Select everything between a given view rectangle.
void qtractorMidiEditor::selectRect ( qtractorScrollView *pScrollView,
	const QRect& rect, bool bToggle, bool bCommit )
{
	int flags = SelectNone;
	if (bToggle)
		flags |= SelectToggle;
	if (bCommit)
		flags |= SelectCommit;
	updateDragSelect(pScrollView, rect.normalized(), flags);
	resetDragState(pScrollView);
	selectionChangeNotify();
}


// Add/remove one single event to current selection.
void qtractorMidiEditor::selectEvent ( qtractorMidiEvent *pEvent, bool bSelect )
{
	if (pEvent == NULL)
		return;

	if (!isEventSelectable(pEvent))
		return;

	QRect rectUpdateView(m_select.rectView());
	QRect rectUpdateEvent(m_select.rectEvent());

	// Select item (or toggle)...
	QRect rectEvent, rectView;
	updateEventRects(pEvent, rectEvent, rectView);
	m_select.selectItem(pEvent, rectEvent, rectView, true, !bSelect);

	// Commit selection...
	m_select.update(true);

	rectUpdateView = rectUpdateView.united(m_select.rectView());
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.united(m_select.rectEvent());
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));
}


// Retrieve current selection.
QList<qtractorMidiEvent *> qtractorMidiEditor::selectedEvents (void) const
{
	QList<qtractorMidiEvent *> list;

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter)
		list.append(iter.key());

	return list;
}


// Selectable event predicate.
bool qtractorMidiEditor::isEventSelectable ( qtractorMidiEvent *pEvent ) const
{
	return (pEvent->type() == m_pEditView->eventType() ||
		(pEvent->type() == m_pEditEvent->eventType() &&
			(m_pEditEvent->eventType() != qtractorMidiEvent::CONTROLLER
			|| pEvent->param() == m_pEditEvent->eventParam())));
}


// Ensuere point visibility depending on view.
void qtractorMidiEditor::ensureVisible (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	if (static_cast<qtractorMidiEditEvent *> (pScrollView) == m_pEditEvent)
		pScrollView->ensureVisible(pos.x(), 0, 16, 0);
	else
		pScrollView->ensureVisible(pos.x(), pos.y(), 16, 16);
}


// Clear all selection.
void qtractorMidiEditor::clearSelect (void)
{
	const QRect rectUpdateView(m_select.rectView());
	const QRect rectUpdateEvent(m_select.rectEvent());

	m_select.clear();

	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));
}


// Update all selection rectangular areas.
void qtractorMidiEditor::updateSelect ( bool bSelectReset )
{
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);
	const unsigned long t0 = pNode->tickFromFrame(m_iOffset);
	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);

	// This is the edit-view specifics...
	const int h1 = m_pEditList->itemHeight();
	const int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		// Common event coords...
		int y;
		const unsigned long t1 = t0 + pEvent->time();
		const unsigned long t2 = t1 + pEvent->duration();
		pNode = cursor.seekTick(t1);
		int x  = pNode->pixelFromTick(t1) - 1;
		int w1 = pNode->pixelFromTick(t2) - x;
		if (w1 < 5)
			w1 = 5;
		// View item...
		const qtractorMidiEvent::EventType etype = pEvent->type();
		if (etype == m_pEditView->eventType()) {
			y = ch - h1 * (pEvent->note() + 1);
			pItem->rectView.setRect(x - x0, y, w1, h1);
		}
		else pItem->rectView.setRect(0, 0, 0, 0);
		// Event item...
		if (etype == m_pEditEvent->eventType()) {
			if (etype == qtractorMidiEvent::REGPARAM    ||
				etype == qtractorMidiEvent::NONREGPARAM ||
				etype == qtractorMidiEvent::CONTROL14)
				y = y0 - (y0 * pEvent->value()) / 16384;
			else
			if (etype == qtractorMidiEvent::PITCHBEND)
				y = y0 - (y0 * pEvent->pitchBend()) / 8192;
			else
				y = y0 - (y0 * pEvent->value()) / 128;
			if (!m_bNoteDuration)
				w1 = 5;
			if (y < y0)
				pItem->rectEvent.setRect(x - x0, y, w1, y0 - y);
			else if (y > y0)
				pItem->rectEvent.setRect(x - x0, y0, w1, y - y0);
			else
				pItem->rectEvent.setRect(x - x0, y0 - 2, w1, 4);
		}
		else pItem->rectEvent.setRect(0, 0, 0, 0);
	}

	// Final touch.
	m_select.commit();

	if (bSelectReset) {
		m_rectDrag = m_select.rectView();
		m_posDrag  = m_rectDrag.topLeft();
		resetDragState(NULL);
	}
}


// Whether there's any events beyond the insertion point (edit-head).
bool qtractorMidiEditor::isInsertable (void) const
{
	if (m_pMidiClip == NULL)
		return false;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return false;

	return m_pTimeScale->tickFromFrame(m_iEditHead)
		 < m_pTimeScale->tickFromFrame(m_iOffset) + pSeq->duration();
}


// Whether there's any selected range (edit-head/tail).
bool qtractorMidiEditor::isSelectable (void) const
{
	return (m_iEditHead < m_iEditTail);
}


// Insert edit range.
void qtractorMidiEditor::insertEditRange (void)
{
	if (m_pMidiClip == NULL)
		return;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return;

	const unsigned long iInsertStart
		= m_pTimeScale->tickFromFrame(m_iEditHead);

	unsigned long iInsertEnd = 0;
	if (m_iEditHead < m_iEditTail) {
		iInsertEnd = m_pTimeScale->tickFromFrame(m_iEditTail);
	} else {
		const unsigned short iBar = m_pTimeScale->barFromFrame(m_iEditHead);
		iInsertEnd = m_pTimeScale->tickFromFrame(m_pTimeScale->frameFromBar(iBar + 1));
	}

	if (iInsertStart >= iInsertEnd)
		return;

	const unsigned long iInsertDuration = iInsertEnd - iInsertStart;
	const unsigned long t0 = m_pTimeScale->tickFromFrame(m_iOffset);

	int iUpdate = 0;
	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("insert range"));

	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		const unsigned long iTime = pEvent->time();
		const unsigned long iDuration = pEvent->duration();
		const unsigned long iEventStart = t0 + iTime;
		const unsigned long iEventEnd = iEventStart + iDuration;
		// Slip/move event...
		if (iEventEnd >= iInsertStart) {
			if (iEventStart < iInsertStart) {
				if (iEventEnd > iInsertStart) {
					// Resize left-event...
					pEditCommand->resizeEventTime(pEvent,
						iTime, iInsertStart - iEventStart);
					// Insert right-event...
					qtractorMidiEvent *pEventEx
						= new qtractorMidiEvent(*pEvent);
					pEventEx->setTime(iInsertEnd - t0);
					pEventEx->setDuration(iEventEnd - iInsertStart);
					pEditCommand->insertEvent(pEventEx);
					++iUpdate;
				}
			} else {
				// Move whole-event...
				pEditCommand->resizeEventTime(pEvent,
					iTime + iInsertDuration, iDuration);
				++iUpdate;
			}
		}
		pEvent = pEvent->next();
	}

	if (iUpdate > 0)
		m_pCommands->exec(pEditCommand);
	else
		delete pEditCommand;
}


// Remove edit range.
void qtractorMidiEditor::removeEditRange (void)
{
	if (m_pMidiClip == NULL)
		return;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return;

	const unsigned long iRemoveStart
		= m_pTimeScale->tickFromFrame(m_iEditHead);

	unsigned long iRemoveEnd   = 0;
	if (m_iEditHead < m_iEditTail) {
		iRemoveEnd = m_pTimeScale->tickFromFrame(m_iEditTail);
	} else {
		unsigned short iBar = m_pTimeScale->barFromFrame(m_iEditHead);
		iRemoveEnd = m_pTimeScale->tickFromFrame(m_pTimeScale->frameFromBar(iBar + 1));
	}

	if (iRemoveStart >= iRemoveEnd)
		return;

	const unsigned long iRemoveDuration = iRemoveEnd - iRemoveStart;
	const unsigned long t0 = m_pTimeScale->tickFromFrame(m_iOffset);

	int iUpdate = 0;
	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("remove range"));

	qtractorMidiEvent *pEvent = pSeq->events().first();
	while (pEvent) {
		const unsigned long iTime = pEvent->time();
		const unsigned long iDuration = pEvent->duration();
		const unsigned long iEventStart = t0 + iTime;
		const unsigned long iEventEnd = iEventStart + iDuration;
		// Slip/move event...
		if (iEventEnd >= iRemoveStart) {
			if (iEventStart < iRemoveStart) {
				// Resize left-event...
				pEditCommand->resizeEventTime(pEvent,
					iTime, iRemoveStart - iEventStart);
				if (iEventEnd > iRemoveEnd) {
					// Insert right-event...
					qtractorMidiEvent *pEventEx
						= new qtractorMidiEvent(*pEvent);
					pEventEx->setTime(iRemoveStart - t0);
					pEventEx->setDuration(iEventEnd - iRemoveEnd);
					pEditCommand->insertEvent(pEventEx);
				}
			}
			else
			if (iEventEnd > iRemoveEnd) {
				if (iEventStart < iRemoveEnd) {
					pEditCommand->resizeEventTime(pEvent,
						iRemoveStart - t0, iEventEnd - iRemoveEnd);
				} else {
					pEditCommand->resizeEventTime(pEvent,
						iTime - iRemoveDuration, iDuration);
				}
			}
			else pEditCommand->removeEvent(pEvent);
			++iUpdate;
		}
		pEvent = pEvent->next();
	}

	if (iUpdate > 0)
		m_pCommands->exec(pEditCommand);
	else
		delete pEditCommand;
}


// Update/sync integral contents.
void qtractorMidiEditor::updateContents (void)
{
	// Update dependant views.
	m_pEditList->updateContentsHeight();
	m_pEditView->updateContentsWidth();

	updateSelect(false);

	// Trigger a complete view update...
	m_pEditList->updateContents();
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
	m_pEditEvent->updateContents();

	m_pThumbView->updateContents();
}


// Try to center vertically the edit-view...
void qtractorMidiEditor::centerContents (void)
{
	// Update dependant views.
	m_pEditList->updateContentsHeight();
	m_pEditView->updateContentsWidth();

	updateSelect(true);

	// Do the centering...
	qtractorMidiSequence *pSeq = NULL;
	if (m_pMidiClip)
		pSeq = m_pMidiClip->sequence();
	if (pSeq)	{
		const int h2 = m_pEditList->itemHeight()
			* (pSeq->noteMin() + pSeq->noteMax());
		int cy = m_pEditView->contentsHeight();
		if (h2 > 0)
			cy -= ((h2 + (m_pEditView->viewport())->height()) >> 1);
		else
			cy >>= 1;
		if (cy < 0)
			cy = 0;
		m_pEditView->setContentsPos(m_pEditView->contentsX(), cy);
	}

	// Update visual cursors anyway...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		setPlayHead(pSession->playHead(), false);
		setEditHead(pSession->editHead(), false);
		setEditTail(pSession->editTail(), false);
	}

	// Trigger a complete view update...
	m_pEditList->updateContents();
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
	m_pEditEvent->updateContents();

	m_pThumbView->updateContents();
}


// Zoom centering prepare method.
// (usually before zoom change)
void qtractorMidiEditor::zoomCenterPre ( ZoomCenter& zc ) const
{
	if (m_pTimeScale == NULL)
		return;

	QWidget *pViewport = m_pEditView->viewport();
	const QRect& rect = pViewport->rect();
	const QPoint& pos = pViewport->mapFromGlobal(QCursor::pos());
	if (rect.contains(pos)) {
		zc.x = pos.x();
		zc.y = pos.y();
	} else {
	#if 0
		zc.x = 0;
		zc.y = 0;
	#else
		zc.x = (rect.width()  >> 1);
		zc.y = (rect.height() >> 1);
	#endif
	}

	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);
	const int cx = m_pEditView->contentsX();
	zc.frame = m_pTimeScale->frameFromPixel(x0 + cx + zc.x);

	const int cy = m_pEditView->contentsY();
	zc.item = (cy + zc.y) / m_pEditList->itemHeight();
}


// Zoom centering post methods.
// (usually after zoom change)
void qtractorMidiEditor::zoomCenterPost ( const ZoomCenter& zc )
{
	if (m_pTimeScale == NULL)
		return;

	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);

	int cx = m_pTimeScale->pixelFromFrame(zc.frame);
	int cy = zc.item * m_pEditList->itemHeight();

	if (cx > zc.x + x0) cx -= zc.x + x0; else cx = 0;
	if (cy > zc.y) cy -= zc.y; else cy = 0;

	// Update dependant views.
	m_pEditList->updateContentsHeight();
	m_pEditView->updateContentsWidth();

	updateSelect(true);

	// Do the centering...
	m_pEditView->setContentsPos(cx, cy);

	// Update visual cursors anyway...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		setPlayHead(pSession->playHead(), false);
		setEditHead(pSession->editHead(), false);
		setEditTail(pSession->editTail(), false);
	}

	// Trigger a complete view update...
	m_pEditList->updateContents();
	m_pEditTime->updateContents();
	m_pEditView->updateContents();
	m_pEditEvent->updateContents();

	m_pThumbView->updateContents();
}


// Reset event cursors.
void qtractorMidiEditor::reset ( bool bSelectClear )
{
	if (bSelectClear)
		m_select.clear();

	// Reset some internal state...
	if (m_pMidiClip) {
		qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
		if (pSeq) {
			m_cursor.reset(pSeq);
			m_cursorAt.reset(pSeq);
		}
	}
}


// Clear all contents.
void qtractorMidiEditor::clear (void)
{
	m_iSyncViewHold = 0;

	m_pCommands->clear();

	if (m_pMidiClip)
		m_pMidiClip->sequence()->clear();

	reset(true);
}


// Intra-clip tick/time positioning reset.
qtractorMidiEvent *qtractorMidiEditor::seekEvent ( unsigned long iTime )
{
	// Reset seek-forward...
	return m_cursor.reset(m_pMidiClip->sequence(), iTime);
}


// Get event from given contents position.
qtractorMidiEvent *qtractorMidiEditor::eventAt (
	qtractorScrollView *pScrollView, const QPoint& pos, QRect *pRect )
{
	if (m_pMidiClip == NULL)
		return NULL;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return NULL;

	const bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);
	const unsigned long t0 = pNode->tickFromFrame(m_iOffset);
	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);

	pNode = cursor.seekPixel(x0 + pos.x());
	unsigned long iTime = pNode->tickFromPixel(x0 + pos.x());
	iTime = (iTime > t0 ? iTime - t0 : 0);

	// This is the edit-view spacifics...
	int h1 = m_pEditList->itemHeight();
	int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	const bool bEventParam
		= (eventType == qtractorMidiEvent::CONTROLLER
		|| eventType == qtractorMidiEvent::REGPARAM
		|| eventType == qtractorMidiEvent::NONREGPARAM
		|| eventType == qtractorMidiEvent::CONTROL14);
	const unsigned short eventParam = m_pEditEvent->eventParam();

	qtractorMidiEvent *pEvent = m_cursorAt.reset(pSeq, iTime);
	qtractorMidiEvent *pEventAt = NULL;
	while (pEvent && iTime >= pEvent->time()) {
		if (((bEditView && pEvent->type() == m_pEditView->eventType()) ||
			 (!bEditView && (pEvent->type() == m_pEditEvent->eventType() &&
				(!bEventParam || pEvent->param() == eventParam))))) {
			// Common event coords...
			int y;
			const unsigned long t1 = t0 + pEvent->time();
			const unsigned long t2 = t1 + pEvent->duration();
			pNode = cursor.seekTick(t1);
			int x = pNode->pixelFromTick(t1) - 1;
			int w1 = pNode->pixelFromTick(t2) - x;
			if (w1 < 5)
				w1 = 5;
			QRect rect;
			if (bEditView) {
				// View item...
				y = ch - h1 * (pEvent->note() + 1);
				rect.setRect(x - x0, y, w1, h1);
			} else {
				// Event item...
				const qtractorMidiEvent::EventType etype = pEvent->type();
				if (etype == qtractorMidiEvent::REGPARAM    ||
					etype == qtractorMidiEvent::NONREGPARAM ||
					etype == qtractorMidiEvent::CONTROL14)
					y = y0 - (y0 * pEvent->value()) / 16384;
				else
				if (etype == qtractorMidiEvent::PITCHBEND)
					y = y0 - (y0 * pEvent->pitchBend()) / 8192;
				else
					y = y0 - (y0 * pEvent->value()) / 128;
				if (!m_bNoteDuration)
					w1 = 5;
				if (y < y0)
					rect.setRect(x - x0, y, w1, y0 - y);
				else if (y > y0)
					rect.setRect(x - x0, y0, w1, y - y0);
				else
					rect.setRect(x - x0, y0 - 2, w1, 4);
			}
			// Do we have a point?
			if (rect.contains(pos)) {
				if (pRect)
					*pRect = rect;
				pEventAt = pEvent;
				// Whether event is also selected...
				if (m_select.findItem(pEventAt))
					break;
			}
		}
		// Maybe next one...
		pEvent = pEvent->next();
	}

	return pEventAt;
}


// Start immediate some drag-edit mode...
qtractorMidiEvent *qtractorMidiEditor::dragEditEvent (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers modifiers )
{
	if (m_pMidiClip == NULL)
		return NULL;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return NULL;

	const bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);
	qtractorMidiEvent::EventType eventType
		= (bEditView ? m_pEditView->eventType() : m_pEditEvent->eventType());

	const int ch = m_pEditView->contentsHeight();

	int h1 = m_pEditList->itemHeight();
	unsigned char note = (ch - pos.y()) / h1;
	if (m_iSnapToScaleType > 0)
		note = snapToScale(note, m_iSnapToScaleKey, m_iSnapToScaleType);

	// Check for note/pitch changes...
	if (bEditView && m_bEventDragEdit && m_pEventDrag
		&& (eventType == qtractorMidiEvent::NOTEON ||
			eventType == qtractorMidiEvent::KEYPRESS)
		&& m_pEventDrag->note() == note)
		return NULL;

	// Must be inside the visible event canvas and
	// not about to drag(draw) event-value resizing...
	if (!bEditView && !m_bEventDragEdit && m_pEventDrag == NULL
		&& isDragEventResize(modifiers))
		return NULL;

	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	const int ymin = 1;
	const int ymax = h0;

	int y = pos.y();

	if (y < ymin) y = ymin; else if (y > ymax) y = ymax;

	// Compute onset time from given horizontal position...
	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);

	int x1 = x0 + pos.x();
	int y1 = 0;

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);	
	const unsigned long t0 = pNode->tickFromFrame(m_iOffset);

	// This would the new event onset time...
	pNode = cursor.seekPixel(x1);
	const unsigned long t1 = pNode->tickSnap(pNode->tickFromPixel(x1), 8) - t0;

	// Check for time/onset changes and whether it's already drawn...
	if (m_bEventDragEdit && m_pEventDrag) {
		const qtractorMidiEditSelect::ItemList& items = m_select.items();
		qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
		const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end
			= items.constEnd();
		for ( ; iter != iter_end; ++iter) {
			qtractorMidiEvent *pEvent = iter.key();
			qtractorMidiEditSelect::Item *pItem = iter.value();
			if (bEditView && pEvent->type() == qtractorMidiEvent::NOTEON) {
				if (pEvent->note() == note) {
					if (t1 >= pEvent->time() &&
						t1 <  pEvent->time() + pEvent->duration()) {
						m_rectDrag = pItem->rectView;
						m_posDrag = pos; // m_rectDrag.topLeft();
						return pEvent;
					}
				}
				else
				if (!m_bEditModeDraw && pEvent == m_pEventDrag) {
					// Bump pitch...
					pEvent->setNote(note);
					pItem->rectView.moveTop(ch - h1 * (note + 1));
					m_select.updateItem(pItem);
					m_rectDrag = pItem->rectView;
					m_posDrag = pos; // m_rectDrag.topLeft();
					resizeEvent(pEvent, timeDelta(pScrollView), 0);
				//	m_posDelta = QPoint(0, 0);
					if (m_bSendNotes)
						m_pEditList->dragNoteOn(note, pEvent->velocity());
					// Bumped.
					return NULL;
				}
			}
			else
			if (t1 == pEvent->time()) {
				m_rectDrag = (bEditView ? pItem->rectView : pItem->rectEvent);
			//	m_posDrag = pos; // m_rectDrag.topLeft();			
				return pEvent;
			}
		}
		// No new events if ain't drawing...
		if (!m_bEditModeDraw)
			return NULL;
	}

	// Create a brand new event...
	qtractorMidiEvent *pEvent = new qtractorMidiEvent(t1, eventType);
	x1 = pNode->pixelFromTick(t0 + t1);

	switch (pEvent->type()) {
	case qtractorMidiEvent::NOTEON:
	case qtractorMidiEvent::KEYPRESS:
		// Set note event value...
		if (bEditView) {
			pEvent->setNote(note);
			pEvent->setVelocity(m_last.value);
		} else {
			pEvent->setNote(m_last.note);
			if (y0 > 0)
				pEvent->setVelocity((128 * (y0 - y)) / y0);
			else
				pEvent->setVelocity(m_last.value);
		}
		// Default duration...
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			unsigned long iDuration = m_pTimeScale->ticksPerBeat();
			if (m_pTimeScale->snapPerBeat() > 0)
				iDuration /= m_pTimeScale->snapPerBeat();
			pEvent->setDuration(iDuration);
			// Mark that we've a note pending...
			if (m_bSendNotes)
				m_pEditList->dragNoteOn(pEvent->note(), pEvent->velocity());
		}
		break;
	case qtractorMidiEvent::REGPARAM:
	case qtractorMidiEvent::NONREGPARAM:
		// Set RPN/NRPN event...
		pEvent->setParam(m_pEditEvent->eventParam());
		if (y0 > 0)
			pEvent->setValue((16384 * (y0 - y)) / y0);
		else
			pEvent->setValue(m_last.value);
		break;
	case qtractorMidiEvent::CONTROL14:
		// Set Control-14 event...
		pEvent->setController(m_pEditEvent->eventParam());
		if (y0 > 0)
			pEvent->setValue((16384 * (y0 - y)) / y0);
		else
			pEvent->setValue(m_last.value);
		break;
	case qtractorMidiEvent::PITCHBEND:
		// Set pitchbend event value...
		if (y0 > 0)
			pEvent->setPitchBend((8192 * (y0 - y)) / y0);
		else
			pEvent->setPitchBend(m_last.pitchBend);
		break;
	case qtractorMidiEvent::CONTROLLER:
		// Set controller event...
		pEvent->setController(m_pEditEvent->eventParam());
		// Fall thru...
	default:
		// Set generic event value...
		if (y0 > 0)
			pEvent->setValue((128 * (y0 - y)) / y0);
		else
			pEvent->setValue(m_last.value);
		break;
	}

	// Now try to get the visual rectangular coordinates...
	int w1 = pNode->pixelFromTick(
		t0 + pEvent->time() + pEvent->duration()) - x1;
	if (w1 < 5)
		w1 = 5;

	// View item...
	QRect rectView;
	if (pEvent->type() == m_pEditView->eventType() &&
		(pEvent->type() == qtractorMidiEvent::NOTEON ||
			pEvent->type() == qtractorMidiEvent::KEYPRESS)) {
		y1 = ch - h1 * (pEvent->note() + 1);
		rectView.setRect(x1 - x0, y1, w1, h1);
	}

	// Event item...
	QRect rectEvent;
	const qtractorMidiEvent::EventType etype = pEvent->type();
	if (etype == m_pEditEvent->eventType()) {
		if (etype == qtractorMidiEvent::REGPARAM    ||
			etype == qtractorMidiEvent::NONREGPARAM ||
			etype == qtractorMidiEvent::CONTROL14) {
			y1 = y0 - (y0 * pEvent->value()) / 16384;
			h1 = y0 - y1;
			m_resizeMode = ResizeValue14;
		}
		else
		if (etype == qtractorMidiEvent::PITCHBEND) {
			y1 = y0 - (y0 * pEvent->pitchBend()) / 8192;
			if (y1 > y0) {
				h1 = y1 - y0;
				y1 = y0;
			} else {
				h1 = y0 - y1;
			}
		} else { 
			y1 = y0 - (y0 * pEvent->value()) / 128;
			h1 = y0 - y1;
			m_resizeMode = ResizeValue;
		}
		if (!m_bNoteDuration)
			w1 = 5;
		if (h1 < 3)
			h1 = 3;
		rectEvent.setRect(x1 - x0, y1, w1, h1);
	}

	// Set the correct target rectangle...
	m_rectDrag = (bEditView ? rectView : rectEvent);
	m_posDrag = pos; // m_rectDrag.topLeft();

	// Just add this one the selection...
	if (!m_bEventDragEdit || m_pEventDrag == NULL)
		clearSelect();

	m_select.selectItem(pEvent, rectEvent, rectView, true, false);

	// Set the proper resize-mode...
	if (bEditView && etype == qtractorMidiEvent::NOTEON)
		m_resizeMode = ResizeNoteRight;
	else
	if (etype == qtractorMidiEvent::REGPARAM    ||
		etype == qtractorMidiEvent::NONREGPARAM ||
		etype == qtractorMidiEvent::CONTROL14)
		m_resizeMode = ResizeValue14;
	else
	if (etype == qtractorMidiEvent::PITCHBEND)
		m_resizeMode = ResizePitchBend;
	else
		m_resizeMode = ResizeValue;

	// Let it be a drag resize mode...
	return pEvent;
}


// Track drag-move-select cursor and mode...
qtractorMidiEvent *qtractorMidiEditor::dragMoveEvent (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers modifiers )
{
	qtractorMidiEvent *pEvent = eventAt(pScrollView, pos, &m_rectDrag);

	// Make the anchor event, if any, visible yet...
	m_resizeMode = ResizeNone;
	if (pEvent) {
		const bool bEditView
			= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);
		Qt::CursorShape shape = Qt::PointingHandCursor;
		const qtractorMidiEvent::EventType etype = pEvent->type();
		if (bEditView) {
			if (etype == qtractorMidiEvent::NOTEON) {
				if (pos.x() > m_rectDrag.right() - 4) {
					m_resizeMode = ResizeNoteRight;
					shape = Qt::SplitHCursor;
				} else if (pos.x() < m_rectDrag.left() + 4) {
					m_resizeMode = ResizeNoteLeft;
					shape = Qt::SplitHCursor;
				}
			}
		} else {
			if (etype == qtractorMidiEvent::NOTEON) {
				if (pos.y() < m_rectDrag.top() + 4) {
					m_resizeMode = ResizeValue;
					shape = Qt::SplitVCursor;
				} else if (m_bNoteDuration) {
					if (pos.x() > m_rectDrag.right() - 4) {
						m_resizeMode = ResizeNoteRight;
						shape = Qt::SplitHCursor;
					} else if (pos.x() < m_rectDrag.left() + 4) {
						m_resizeMode = ResizeNoteLeft;
						shape = Qt::SplitHCursor;
					}
				}
			}
			else
			if (etype == qtractorMidiEvent::PITCHBEND) {
				const int y0
					= (((m_pEditEvent->viewport())->height() & ~1) >> 1);
				if ((pos.y() < y0 && pos.y() < m_rectDrag.top()    + 4) ||
					(pos.y() > y0 && pos.y() > m_rectDrag.bottom() - 4)) {
					m_resizeMode = ResizePitchBend;
					shape = Qt::SplitVCursor;
				}
			}
			if (etype == qtractorMidiEvent::REGPARAM    ||
				etype == qtractorMidiEvent::NONREGPARAM ||
				etype == qtractorMidiEvent::CONTROL14) {
				m_resizeMode = ResizeValue14;
				shape = Qt::SplitVCursor;
			}
			else
			if (pos.y() < m_rectDrag.top() + 4) {
				m_resizeMode = ResizeValue;
				shape = Qt::SplitVCursor;
			}
			if (m_resizeMode == ResizeNone
				&& isDragEventResize(modifiers)) {
				shape = Qt::ArrowCursor;
				pEvent = NULL;
			}
		}
		if ((m_resizeMode == ResizeNoteRight ||
			 m_resizeMode == ResizePitchBend ||
			 m_resizeMode == ResizeValue14   ||
			 m_resizeMode == ResizeValue)
			&& (modifiers & Qt::ControlModifier))
			m_dragCursor = DragRescale;
		else
			m_dragCursor = DragResize;
		pScrollView->setCursor(QCursor(shape));
	}
	else
	if (m_dragState == DragNone) {
		m_dragCursor = DragNone;
		pScrollView->unsetCursor();
	}

	return pEvent;
}


// Start drag-move-selecting...
void qtractorMidiEditor::dragMoveStart (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers modifiers )
{
	// Are we already step-moving or pasting something?
	switch (m_dragState) {
	case DragStep:
		// One-click change from drag-step to drag-move...
		m_dragState = DragMove;
		m_posDrag   = m_rectDrag.center();
		m_posStep   = QPoint(0, 0);
		updateDragMove(pScrollView, pos + m_posStep);
		// Fall thru...
	case DragPaste:
		return;
	default:
		break;
	}

	// Force null state.
	resetDragState(pScrollView);

	// Remember what and where we'll be dragging/selecting...
	m_dragState  = DragStart;
	m_posDrag    = pos;
	m_pEventDrag = dragMoveEvent(pScrollView, m_posDrag, modifiers);

	// Check whether we're about to create something...
	if (m_pEventDrag == NULL && m_bEditMode
		&& (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0) {
		m_dragCursor = m_dragState;
		m_pEventDrag = dragEditEvent(pScrollView, m_posDrag, modifiers);
		m_bEventDragEdit = (m_pEventDrag != NULL);
		pScrollView->setCursor(QCursor(QPixmap(":/images/editModeOn.png"), 5, 18));
	} else if (m_resizeMode == ResizeNone) {
		m_dragCursor = m_dragState;
		if (m_pEventDrag) {
			pScrollView->setCursor(QCursor(
				static_cast<qtractorScrollView *> (m_pEditView)	== pScrollView
				? Qt::SizeAllCursor : Qt::SizeHorCursor));
		} else {
			pScrollView->setCursor(QCursor(Qt::CrossCursor));
		}
	}

	// Maybe we'll have a note pending...
	if (m_bSendNotes && m_pEventDrag
		&& m_pEventDrag->type() == qtractorMidiEvent::NOTEON)
		m_pEditList->dragNoteOn(m_pEventDrag->note(), m_pEventDrag->velocity());
}


// Update drag-move-selection...
void qtractorMidiEditor::dragMoveUpdate (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers modifiers )
{
	int flags = SelectNone;
	
	switch (m_dragState) {
	case DragStart:
		// Did we moved enough around?
		if ((pos - m_posDrag).manhattanLength()
			< QApplication::startDragDistance())
			break;
		// Are we about to move/resize something around?
		if (m_pEventDrag) {
			// Take care of selection modifier...
			if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0
				&& !m_select.findItem(m_pEventDrag))
				flags |= SelectClear;
			if (m_resizeMode == ResizeNone) {
				// Start moving... take care of yet initial selection...
				updateDragSelect(pScrollView,
					QRect(m_posDrag, QSize(1, 1)), flags | SelectCommit);
				// Start drag-moving...
				m_dragState = DragMove;
				updateDragMove(pScrollView, pos + m_posStep);
			} else {
				// Start resizing... take care of yet initial selection...
				if (!m_bEventDragEdit && !m_select.findItem(m_pEventDrag)) {
					updateDragSelect(pScrollView,
						QRect(m_posDrag, QSize(1, 1)), flags | SelectCommit);
				}
				// Start drag-resizing/rescaling...
				if ((m_resizeMode == ResizeNoteRight ||
					 m_resizeMode == ResizePitchBend ||
					 m_resizeMode == ResizeValue14   ||
					 m_resizeMode == ResizeValue)
					&& (modifiers & Qt::ControlModifier)) {
					m_dragState = DragRescale;
					updateDragRescale(pScrollView, pos);
				} else {
					m_dragState = DragResize;
					updateDragResize(pScrollView, pos);
				}
			}
			break;
		}
		// About to drag(draw) event-value reszing...
		if (static_cast<qtractorMidiEditEvent *> (pScrollView) == m_pEditEvent
			&& isDragEventResize(modifiers)) {
			m_dragState = DragEventResize;
			updateDragEventResize(pos);
			break;
		}
		// Just about to start rubber-banding...
		m_dragState = DragSelect;
		// Fall thru...
	case DragSelect: {
		// Set new rubber-band extents...
		ensureVisible(pScrollView, pos);
		if (modifiers & Qt::ControlModifier)
			flags |= SelectToggle;
		const QRect& rect = QRect(m_posDrag, pos).normalized();
		updateDragSelect(pScrollView, rect, flags);
		showToolTip(pScrollView, rect);
		break;
	}
	case DragMove:
	case DragPaste:
		// Drag-moving...
		updateDragMove(pScrollView, pos + m_posStep);
		break;
	case DragRescale:
		// Drag-rescaling...
		updateDragRescale(pScrollView, pos);
		break;
	case DragResize:
		// Drag-resizing...
		updateDragResize(pScrollView, pos);
		// Drag-edit/drawing...
		if (m_bEventDragEdit && m_pEventDrag) {
			qtractorMidiEvent *pEvent
				= dragEditEvent(pScrollView, pos, modifiers);
			if (pEvent && pEvent != m_pEventDrag) {
				resizeEvent(m_pEventDrag,
					timeDelta(pScrollView),
					valueDelta(pScrollView));
				m_posDelta = QPoint(0, 0);
				m_pEventDrag = pEvent;
			}
		}
		break;
	case DragEventResize:
		// Drag(draw) resizing...
		updateDragEventResize(pos);
		break;
	case DragStep:
	case DragNone:
	default:
		// Just make cursor tell something...
		dragMoveEvent(pScrollView, pos, modifiers);
		break;
	}

	// Let note hovering shine...
	m_pEditList->dragNoteOn(pos, -1);
}


// Commit drag-move-selection...
void qtractorMidiEditor::dragMoveCommit (
	qtractorScrollView *pScrollView, const QPoint& pos,
	Qt::KeyboardModifiers modifiers )
{
	int flags = qtractorMidiEditor::SelectCommit;

	switch (m_dragState) {
	case DragStart:
		// Were we about to edit-resize something?
		if (m_bEventDragEdit) {
			m_dragState = DragResize;
			executeDragResize(pScrollView, pos);
			break;
		}
		// Take care of selection modifier...
		if ((modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) == 0)
			flags |= SelectClear;
		else
		// Shall we move the playhead?...
		if (m_pEventDrag == NULL) {
			// Direct snap positioning...
			const unsigned long iFrame = m_pTimeScale->frameSnap(m_iOffset
				+ m_pTimeScale->frameFromPixel(pos.x() > 0 ? pos.x() : 0));
			// Playhead positioning...
			setPlayHead(iFrame);
			// Immediately commited...
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession)
				pSession->setPlayHead(iFrame);
		}
		// Fall thru...
	case DragSelect:
		// Terminate selection...
		ensureVisible(pScrollView, pos);
		if (modifiers & Qt::ControlModifier)
			flags |= SelectToggle;
		updateDragSelect(pScrollView, QRect(m_posDrag, pos).normalized(), flags);
		selectionChangeNotify();
		break;
	case DragMove:
		// Move it...
		executeDragMove(pScrollView, pos);
		break;
	case DragPaste:
		// Paste it...
		executeDragPaste(pScrollView, pos);
		break;
	case DragRescale:
		// Rescale it...
		executeDragRescale(pScrollView, pos);
		break;
	case DragResize:
		// Resize it...
		executeDragResize(pScrollView, pos);
		break;
	case DragEventResize:
		// Resize(by drawing) it...
		executeDragEventResize(pos);
		break;
	case DragStep:
	case DragNone:
	default:
		break;
	}

	// Force null state.
	resetDragState(pScrollView);
}


// Trap for help/tool-tip and leave events.
bool qtractorMidiEditor::dragMoveFilter (
	qtractorScrollView *pScrollView, QObject *pObject, QEvent *pEvent )
{
	if (static_cast<QWidget *> (pObject) == pScrollView->viewport()) {
		if (pEvent->type() == QEvent::ToolTip && m_bToolTips) {
			QHelpEvent *pHelpEvent = static_cast<QHelpEvent *> (pEvent);
			if (pHelpEvent) {
				const QPoint& pos
					= pScrollView->viewportToContents(pHelpEvent->pos());
				qtractorMidiEvent *pMidiEvent = eventAt(pScrollView, pos);
				if (pMidiEvent) {
					QToolTip::showText(
						pHelpEvent->globalPos(),
						eventToolTip(pMidiEvent),
						pScrollView->viewport());
					return true;
				} else {
					const QString sToolTip("%1 (%2)");
					int note = m_last.note;
					if (pScrollView
						== static_cast<qtractorScrollView *> (m_pEditView)) {
						const int ch = m_pEditList->contentsHeight();
						note = (ch - pos.y()) / m_pEditList->itemHeight();
					}
					QToolTip::showText(
						pHelpEvent->globalPos(),
						sToolTip.arg(noteName(note)).arg(note),
						pScrollView->viewport());
					return true;
				}
			}
		}
		else
		if (pEvent->type() == QEvent::Leave	&&
			m_dragState != DragPaste &&
			m_dragState != DragStep) {
			m_dragCursor = DragNone;
			pScrollView->unsetCursor();
			m_pEditList->dragNoteOn(-1);
			return true;
		}
	}

	// Not handled here.
	return false;
}


// Update the event selection list.
void qtractorMidiEditor::updateDragSelect (
	qtractorScrollView *pScrollView, const QRect& rectSelect, int flags )
{
	if (m_pMidiClip == NULL)
		return;

	qtractorMidiSequence *pSeq = m_pMidiClip->sequence();
	if (pSeq == NULL)
		return;

	// Rubber-banding only applicable whenever
	// the selection rectangle is not that empty...
	const bool bRectSelect
		= (rectSelect.width() > 1 || rectSelect.height() > 1);

	if (bRectSelect) {
		// Create rubber-band, if not already...
		if (m_pRubberBand == NULL) {
			m_pRubberBand = new qtractorRubberBand(
				QRubberBand::Rectangle, pScrollView->viewport());
			m_pRubberBand->show();
		}
		// Rubber-band selection...
		m_pRubberBand->setGeometry(QRect(
			pScrollView->contentsToViewport(rectSelect.topLeft()),
			rectSelect.size()));
	}

	// Do the drag-select update properly...
	const bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	QRect rectUpdateView(m_select.rectView());
	QRect rectUpdateEvent(m_select.rectEvent());

	if (flags & SelectClear)
		m_select.clear();

	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);	
	const unsigned long t0 = pNode->tickFromFrame(m_iOffset);

	int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);
	int x1, x2;
	if (bRectSelect) {
		x1 = pScrollView->contentsX();
		x2 = x1 + (pScrollView->viewport())->width();
		if (x1 > rectSelect.left())
			x1 = rectSelect.left();
		if (x2 < rectSelect.right())
			x2 = rectSelect.right();
	} else {
		x1 = x2 = rectSelect.x();
	}

	if (--x0 < 0) x0 = 0;
	if (--x1 < 0) x1 = 0;
	++x2;

	pNode = cursor.seekPixel(x0 + x1);
	unsigned long t1 = pNode->tickFromPixel(x0 + x1);
	const unsigned long iTickStart = (t1 > t0 ? t1 - t0 : 0);

	pNode = cursor.seekPixel(x0 + x2);
	unsigned long t2 = pNode->tickFromPixel(x0 + x2);
	const unsigned long iTickEnd = (t2 > t0 ? t2 - t0 : 0);

	// This is the edit-view spacifics...
	const int h1 = m_pEditList->itemHeight();
	const int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	const bool bEventParam
		= (eventType == qtractorMidiEvent::CONTROLLER
		|| eventType == qtractorMidiEvent::REGPARAM
		|| eventType == qtractorMidiEvent::NONREGPARAM
		|| eventType == qtractorMidiEvent::CONTROL14);
	const unsigned short eventParam = m_pEditEvent->eventParam();

	qtractorMidiEvent *pEvent = m_cursorAt.seek(pSeq, iTickStart);

	qtractorMidiEvent *pEventAt = NULL;
	QRect rectViewAt;
	QRect rectEventAt;

	while (pEvent && iTickEnd >= pEvent->time()) {
		if (((bEditView && pEvent->type() == m_pEditView->eventType()) ||
			 (!bEditView && (pEvent->type() == m_pEditEvent->eventType() &&
				(!bEventParam || pEvent->param() == eventParam))))) {
			// Assume unselected...
			bool bSelect = false;
			// Common event coords...
			int y;
			t1 = t0 + pEvent->time();
			t2 = t1 + pEvent->duration();
			pNode = cursor.seekTick(t1);
			int x  = pNode->pixelFromTick(t1) - 1;
			int w1 = pNode->pixelFromTick(t2) - x;
			if (w1 < 5)
				w1 = 5;
			// View item...
			QRect rectView;
			if (pEvent->type() == m_pEditView->eventType()) {
				y = ch - h1 * (pEvent->note() + 1);
				rectView.setRect(x - x0, y, w1, h1);
				if (bEditView)
					bSelect = rectSelect.intersects(rectView);
			}
			// Event item...
			QRect rectEvent;
			const qtractorMidiEvent::EventType etype = pEvent->type();
			if (etype == eventType) {
				if (etype == qtractorMidiEvent::REGPARAM    ||
					etype == qtractorMidiEvent::NONREGPARAM ||
					etype == qtractorMidiEvent::CONTROL14)
					y = y0 - (y0 * pEvent->value()) / 16384;
				else
				if (pEvent->type() == qtractorMidiEvent::PITCHBEND)
					y = y0 - (y0 * pEvent->pitchBend()) / 8192;
				else
					y = y0 - (y0 * pEvent->value()) / 128;
				if (!m_bNoteDuration)
					w1 = 5;
				if (y < y0)
					rectEvent.setRect(x - x0, y, w1, y0 - y);
				else if (y > y0)
					rectEvent.setRect(x - x0, y0, w1, y - y0);
				else
					rectEvent.setRect(x - x0, y0 - 2, w1, 4);
				if (!bEditView)
					bSelect = rectSelect.intersects(rectEvent);
			}
			// Select item...
			if (bRectSelect) {
				m_select.selectItem(pEvent, rectEvent, rectView,
					bSelect, flags & SelectToggle);
			} else if (bSelect) {
				pEventAt    = pEvent;
				rectViewAt  = rectView;
				rectEventAt = rectEvent;
			}
		}
		// Lookup next...
		pEvent = pEvent->next();
	}

	// Most evident single selection...
	if (pEventAt /* && !bRectSelect*/) {
		m_select.selectItem(pEventAt, rectEventAt, rectViewAt,
			true, flags & SelectToggle);
	}

	// Commit selection...
	const bool bCommit = (flags & SelectCommit);
	m_select.update(bCommit);

	rectUpdateView = rectUpdateView.united(m_select.rectView());
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.united(m_select.rectEvent());
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

#if 0
	if (bEditView) {
		setEditHead(m_pTimeScale->frameSnap(m_iOffset
			+ m_pTimeScale->frameFromPixel(rectSelect.left())), bCommit);
		setEditTail(m_pTimeScale->frameSnap(m_iOffset
			+ m_pTimeScale->frameFromPixel(rectSelect.right())), bCommit);
	}
#endif
}


// Compute current drag time snap (in ticks).
long qtractorMidiEditor::timeSnap ( long iTime ) const
{
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);	
	const unsigned long t0 = pNode->tickFromFrame(m_iOffset);
	const unsigned long t1 = t0 + iTime;
	pNode = cursor.seekTick(t1);
	iTime = long(pNode->tickSnap(t1)) - long(t0);
	return (iTime > 0 ? iTime : 0);
}


// Compute current drag time delta (in ticks).
long qtractorMidiEditor::timeDelta ( qtractorScrollView *pScrollView ) const
{
	DragTimeScale dts(m_pTimeScale, m_iOffset);

	int x1, x2;
	unsigned long t1, t2;

	if (m_pEventDrag) {
		t1 = dts.t0 + m_pEventDrag->time();
		dts.node = dts.cursor.seekTick(t1);
		x1 = dts.node->pixelFromTick(t1);
	} else {
		x1 = dts.x0 + (
			static_cast<qtractorScrollView *> (m_pEditView) == pScrollView
			? m_select.rectView().x() : m_select.rectEvent().x());
		dts.node = dts.cursor.seekPixel(x1);
		t1 = dts.node->tickFromPixel(x1);
	}

	x2 = x1 + m_posDelta.x();
	dts.node = dts.cursor.seekPixel(x2);
	t2 = dts.node->tickFromPixel(x2);

	return long(dts.node->tickSnap(t2)) - long(t1);
//	return long(t2) - long(t1);
}


// Compute current drag note delta.
int qtractorMidiEditor::noteDelta ( qtractorScrollView *pScrollView ) const
{
	int iNoteDelta = 0;

	if (pScrollView == static_cast<qtractorScrollView *> (m_pEditView)) {
		int h1 = m_pEditList->itemHeight();
		if (h1 > 0)
			iNoteDelta = -(m_posDelta.y() / h1);
	}

	return iNoteDelta;
}


// Compute current drag value delta.
int qtractorMidiEditor::valueDelta ( qtractorScrollView *pScrollView ) const
{
	int iValueDelta = 0;

	if (pScrollView == static_cast<qtractorScrollView *> (m_pEditEvent)) {
		const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
		if (h0 > 1) {
			if (m_resizeMode == ResizePitchBend)
				iValueDelta = -(m_posDelta.y() * 8192) / (h0 >> 1);
			else
			if (m_resizeMode == ResizeValue14)
				iValueDelta = -(m_posDelta.y() * 16384) / h0;
			else
				iValueDelta = -(m_posDelta.y() * 128) / h0;
		}
	}

	return iValueDelta;
}


// Apply the event drag-resize (also editing).
void qtractorMidiEditor::resizeEvent (
	qtractorMidiEvent *pEvent, long iTimeDelta, int iValueDelta,
	qtractorMidiEditCommand *pEditCommand )
{
	long iTime, iDuration;
	int iValue;

	switch (m_resizeMode) {
	case ResizeNoteLeft:
		iTime = timeSnap(long(pEvent->time()) + iTimeDelta);
		iDuration = long(pEvent->duration())
			+ (long(pEvent->time()) - iTime);
		if (iDuration < 1)
			iDuration = 1;
		if (m_bEventDragEdit) {
			pEvent->setTime(iTime);
			pEvent->setDuration(iDuration);
			if (pEditCommand)
				pEditCommand->insertEvent(pEvent);
		}
		else
		if (pEditCommand)
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		if (pEvent == m_pEventDrag) {
			m_last.note = pEvent->note();
			m_last.duration = iDuration;
		}
		break;
	case ResizeNoteRight:
		iTime = pEvent->time();
		iDuration = timeSnap(
			long(pEvent->time() + pEvent->duration()) + iTimeDelta) - iTime;
		if (iDuration < 1)
			iDuration = 1;
		if (m_bEventDragEdit) {
			pEvent->setDuration(iDuration);
			if (pEditCommand)
				pEditCommand->insertEvent(pEvent);
		}
		else
		if (pEditCommand)
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		if (pEvent == m_pEventDrag) {
			m_last.note = pEvent->note();
			m_last.duration = iDuration;
		}
		break;
	case ResizeValue:
		iValue = int(pEvent->value()) + iValueDelta;
		if (iValue < 0)
			iValue = 0;
		else
		if (iValue > 127)
			iValue = 127;
		if (m_bEventDragEdit) {
			pEvent->setValue(iValue);
			if (pEditCommand)
				pEditCommand->insertEvent(pEvent);
		}
		else
		if (pEditCommand)
			pEditCommand->resizeEventValue(pEvent, iValue);
		if (pEvent == m_pEventDrag)
			m_last.value = iValue;
		break;
	case ResizeValue14:
		iValue = int(pEvent->value()) + iValueDelta;
		if (iValue < 0)
			iValue = 0;
		else
		if (iValue > 16383)
			iValue = 16383;
		if (m_bEventDragEdit) {
			pEvent->setValue(iValue);
			if (pEditCommand)
				pEditCommand->insertEvent(pEvent);
		}
		else
		if (pEditCommand)
			pEditCommand->resizeEventValue(pEvent, iValue);
		if (pEvent == m_pEventDrag)
			m_last.value = iValue;
		break;
	case ResizePitchBend:
		iValue = pEvent->pitchBend() + iValueDelta;
		if (iValue < -8191)
			iValue = -8191;
		else
		if (iValue > +8191)
			iValue = +8191;
		if (m_bEventDragEdit) {
			pEvent->setPitchBend(iValue);
			if (pEditCommand)
				pEditCommand->insertEvent(pEvent);
		}
		else
		if (pEditCommand)
			pEditCommand->resizeEventValue(pEvent, iValue);
		if (pEvent == m_pEventDrag)
			m_last.pitchBend = iValue;
		// Fall thru...
	default:
		break;
	}

	if (m_bEventDragEdit && pEditCommand == NULL)
		updateEvent(pEvent);
}


// Update event selection rectangle.
void qtractorMidiEditor::updateEvent ( qtractorMidiEvent *pEvent )
{
	qtractorMidiEditSelect::Item *pItem = m_select.findItem(pEvent);
	if (pItem == NULL)
		return;

	// Update selection visual rectangles...
	updateEventRects(pEvent, pItem->rectEvent, pItem->rectView);
	m_select.updateItem(pItem);
}


// Update event visual rectangles.
void qtractorMidiEditor::updateEventRects (
	qtractorMidiEvent *pEvent, QRect& rectEvent, QRect& rectView ) const
{
	qtractorTimeScale::Cursor cursor(m_pTimeScale);
	qtractorTimeScale::Node *pNode = cursor.seekFrame(m_iOffset);
	const unsigned long t0 = pNode->tickFromFrame(m_iOffset);
	const int x0 = m_pTimeScale->pixelFromFrame(m_iOffset);

	// This is the edit-view spacifics...
	const int h1 = m_pEditList->itemHeight();
	const int ch = m_pEditView->contentsHeight(); // + 1;

	// This is the edit-event zero-line...
	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	// Common event coords...
	const unsigned long t1 = t0 + pEvent->time();
	const unsigned long t2 = t1 + pEvent->duration();
	pNode = cursor.seekTick(t1);
	int x  = pNode->pixelFromTick(t1) - 1;
	int w1 = pNode->pixelFromTick(t2) - x;
	if (w1 < 5)
		w1 = 5;

	// View item...
	int y;
	if (pEvent->type() == m_pEditView->eventType()) {
		y = ch - h1 * (pEvent->note() + 1);
		rectView.setRect(x - x0, y, w1, h1);
	}
	else rectView.setRect(0, 0, 0, 0);

	// Event item...
	const qtractorMidiEvent::EventType etype = pEvent->type();
	if (etype == eventType) {
		if (etype == qtractorMidiEvent::REGPARAM    ||
			etype == qtractorMidiEvent::NONREGPARAM ||
			etype == qtractorMidiEvent::CONTROL14)
			y = y0 - (y0 * pEvent->value()) / 16384;
		else
		if (etype == qtractorMidiEvent::PITCHBEND)
			y = y0 - (y0 * pEvent->pitchBend()) / 8192;
		else
			y = y0 - (y0 * pEvent->value()) / 128;
		if (!m_bNoteDuration)
			w1 = 5;
		if (y < y0)
			rectEvent.setRect(x - x0, y, w1, y0 - y);
		else if (y > y0)
			rectEvent.setRect(x - x0, y0, w1, y - y0);
		else
			rectEvent.setRect(x - x0, y0 - 2, w1, 4);
	}
	else rectEvent.setRect(0, 0, 0, 0);
}


// Drag-move current selection.
void qtractorMidiEditor::updateDragMove (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	ensureVisible(pScrollView, pos);

	const bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	QRect rectUpdateView(m_select.rectView().translated(m_posDelta));
	QRect rectUpdateEvent(m_select.rectEvent().translated(m_posDelta.x(), 0));

	QPoint delta(pos - m_posDrag);
	QRect rect(bEditView ? m_select.rectView() : m_select.rectEvent());

	const int cw = pScrollView->contentsWidth();

	int dx = delta.x();

	const int x0 = m_rectDrag.x() + m_pTimeScale->pixelFromFrame(m_iOffset);
	const int x1 = rect.x() + dx;
	if (x1 < 0)
		dx = -(rect.x());
	if (x1 + rect.width() > cw)
		dx = cw - rect.right();
	m_posDelta.setX(m_pTimeScale->pixelSnap(x0 + dx) - x0);

	const int h1 = m_pEditList->itemHeight();

	if (bEditView && h1 > 0) {
		const int ch = m_pEditView->contentsHeight();
		const int y0 = rect.y();
		int y1 = y0 + delta.y();
		if (y1 < 0)
			y1 = 0;
		if (y1 + rect.height() > ch)
			y1 = ch - rect.height();
		unsigned char note = 127 - (y1 / h1);
		if (m_iSnapToScaleType > 0)
			note = snapToScale(note, m_iSnapToScaleKey, m_iSnapToScaleType);
		m_posDelta.setY(h1 * (127 - note) - y0);
	} else {
		m_posDelta.setY(0);
	}

	rectUpdateView = rectUpdateView.united(
		m_select.rectView().translated(m_posDelta));
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.united(
		m_select.rectEvent().translated(m_posDelta.x(), 0));
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

	// Maybe we've change some note pending...
	if (m_bSendNotes && m_pEventDrag
		&& m_pEventDrag->type() == qtractorMidiEvent::NOTEON) {
		int iNote = int(m_pEventDrag->note());
		if (h1 > 0)
			iNote -= (m_posDelta.y() / h1);
		m_pEditList->dragNoteOn(iNote, m_pEventDrag->velocity());
	}

	// Show anchor event tooltip...
	if (m_bToolTips) {
		qtractorMidiEvent *pEvent = m_pEventDrag;
		if (pEvent == NULL)
			pEvent = m_select.anchorEvent();
		if (pEvent) {
			QToolTip::showText(
				QCursor::pos(),
				eventToolTip(pEvent,
					timeDelta(pScrollView), noteDelta(pScrollView), 0),
				pScrollView->viewport());
		}
	}
}


// Drag-rescale current selection.
void qtractorMidiEditor::updateDragRescale (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	ensureVisible(pScrollView, pos);

	QRect rectUpdateView(m_select.rectView());
	QRect rectUpdateEvent(m_select.rectEvent());

	QPoint delta(pos - m_posDrag);

	int x0, x1;
	int y0, y1;

	int dx = 0;
	int dy = 0;

	switch (m_resizeMode) {
	case ResizeNoteRight:
		dx = delta.x();
		x0 = m_rectDrag.right() + m_pTimeScale->pixelFromFrame(m_iOffset);
		x1 = m_rectDrag.right() + dx;
		if (x1 < m_rectDrag.left())
			dx = -(m_rectDrag.width());
		dx = m_pTimeScale->pixelSnap(x0 + dx) - x0;
		break;
	case ResizeValue:
	case ResizeValue14:
	case ResizePitchBend:
		dy = delta.y();
		y0 = m_rectDrag.bottom();
		y1 = y0 + dy;
		dy = y1 - y0;
		// Fall thru...
	default:
		break;
	}

	m_posDelta.setX(dx);
	m_posDelta.setY(dy);

	const QRect& rectView  = (m_pEditView->viewport())->rect();
	const QRect& rectEvent = (m_pEditEvent->viewport())->rect();

	if (dx) {
		rectUpdateView.setLeft(rectView.left());
		rectUpdateView.setRight(rectView.right());
		rectUpdateEvent.setLeft(rectEvent.left());
		rectUpdateEvent.setRight(rectEvent.right());
	}

	if (dy) {
		rectUpdateEvent.setTop(rectEvent.top());
		rectUpdateEvent.setBottom(rectEvent.bottom());
	}

	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

	// Show anchor event tooltip...
	if (m_bToolTips) {
		qtractorMidiEvent *pEvent = m_pEventDrag;
		if (pEvent == NULL)
			pEvent = m_select.anchorEvent();
		if (pEvent) {
			QToolTip::showText(
				QCursor::pos(),
				eventToolTip(pEvent,
					timeDelta(pScrollView), 0, valueDelta(pScrollView)),
				pScrollView->viewport());
		}
	}
}


// Drag-resize current selection (also editing).
void qtractorMidiEditor::updateDragResize (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	ensureVisible(pScrollView, pos);

	QRect rectUpdateView(m_select.rectView().translated(m_posDelta.x(), 0));
	QRect rectUpdateEvent(m_select.rectEvent().translated(m_posDelta));

	QPoint delta(pos - m_posDrag);
	int x0, x1;
	int y0, y1;
	int dx = 0;
	int dy = 0;

	// TODO: Plenty of...
	switch (m_resizeMode) {
	case ResizeNoteLeft:
		dx = delta.x();
		x0 = m_rectDrag.left() + m_pTimeScale->pixelFromFrame(m_iOffset);
		x1 = m_rectDrag.left() + dx;
		if (x1 < 0)
			dx = -(m_rectDrag.left());
		if (x1 > m_rectDrag.right())
			dx = +(m_rectDrag.width());
		dx = m_pTimeScale->pixelSnap(x0 + dx) - x0;
		break;
	case ResizeNoteRight:
		dx = delta.x();
		x0 = m_rectDrag.right() + m_pTimeScale->pixelFromFrame(m_iOffset);
		x1 = m_rectDrag.right() + dx;
		if (x1 < m_rectDrag.left())
			dx = -(m_rectDrag.width());
		dx = m_pTimeScale->pixelSnap(x0 + dx) - x0;
		break;
	case ResizeValue:
	case ResizeValue14:
	case ResizePitchBend:
		dy = delta.y();
		y0 = m_rectDrag.bottom();
		y1 = y0 + dy;
		dy = y1 - y0;
		// Fall thru...
	default:
		break;
	}

	m_posDelta.setX(dx);
	m_posDelta.setY(dy);

	rectUpdateView = rectUpdateView.united(
		m_select.rectView().translated(dx, 0));
	m_pEditView->viewport()->update(QRect(
		m_pEditView->contentsToViewport(rectUpdateView.topLeft()),
		rectUpdateView.size()));

	rectUpdateEvent = rectUpdateEvent.united(
		m_select.rectEvent().translated(dx, dy));
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

	// Show anchor event tooltip...
	if (m_bToolTips) {
		qtractorMidiEvent *pEvent = m_pEventDrag;
		if (pEvent == NULL)
			pEvent = m_select.anchorEvent();
		if (pEvent) {
			QToolTip::showText(
				QCursor::pos(),
				eventToolTip(pEvent,
					timeDelta(pScrollView), 0, valueDelta(pScrollView)),
				pScrollView->viewport());
		}
	}
}


// Drag(draw) event value-resize check.
bool qtractorMidiEditor::isDragEventResize ( Qt::KeyboardModifiers modifiers ) const
{
	if (!m_bEditMode || !m_bEditModeDraw)
		return false;
	if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier))
		return false;

	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end
		= items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		if (pEvent->type() == eventType)
			return true;
	}

	return false;
}


// Drag(draw) event value-resize to current selection...
void qtractorMidiEditor::updateDragEventResize ( const QPoint& pos )
{
	m_pEditEvent->ensureVisible(pos.x(), 0, 16, 0);

	const QPoint delta(pos - m_posDrag);
	if (delta.manhattanLength() < 4)
		return;

	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	if (y0 < 1)
		return;

	const QRect& rectDrag = QRect(m_posDrag, m_posDragEventResize).normalized();
	QRect rectUpdateEvent(m_select.rectEvent().united(rectDrag));

	const int xmin = (delta.x() < 0 ? pos.x() : m_posDrag.x());
	const int xmax = (delta.x() > 0 ? pos.x() : m_posDrag.x());

	const int ymin = 1;
	const int ymax = h0;

	const float m = float(delta.y()) / float(delta.x());
	const float b = float(pos.y()) - m * float(pos.x());

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end
		= items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		if (pEvent->type() != eventType)
			continue;
		const QRect& rectEvent = pItem->rectEvent;
		if (rectEvent.right() < xmin || rectEvent.left() > xmax)
			continue;
		int y = int(m * float(rectEvent.x()) + b);
		if (pEvent->type() == qtractorMidiEvent::PITCHBEND) {
			if (y < ymin) y = ymin; else if (y > ymax) y = ymax;
			if (y > y0) {
				pItem->rectEvent.setBottom(y);
				y = y0;
			} else {
				pItem->rectEvent.setBottom(y0);
			}
		}
		pItem->rectEvent.setTop(y);
		pItem->flags |= 4;
		m_select.updateItem(pItem);
	}

	rectUpdateEvent = rectUpdateEvent.united(m_select.rectEvent());
	m_pEditEvent->viewport()->update(QRect(
		m_pEditEvent->contentsToViewport(rectUpdateEvent.topLeft()),
		rectUpdateEvent.size()));

	m_posDragEventResize = pos;
}


// Finalize the event drag-move.
void qtractorMidiEditor::executeDragMove (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragMove(pScrollView, pos + m_posStep);

	const long iTimeDelta = timeDelta(pScrollView);
	const int  iNoteDelta = noteDelta(pScrollView);

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("move"));

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		int iNote = int(pEvent->note()) + iNoteDelta;
		if (iNote < 0)
			iNote = 0;
		if (iNote > 127)
			iNote = 127;
		const long iTime = long(pEvent->time() + pItem->delta) + iTimeDelta;
	//	if (pEvent == m_pEventDrag)
	//		iTime = timeSnap(iTime);
		pEditCommand->moveEvent(pEvent, iNote, iTime);
	}

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);
}


// Finalize the event drag-resize (also editing).
void qtractorMidiEditor::executeDragResize (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragResize(pScrollView, pos);

	const long iTimeDelta = timeDelta(pScrollView);
	const int iValueDelta = valueDelta(pScrollView);

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip,
			m_bEventDragEdit ? tr("edit") : tr("resize"));

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		if (!m_bEventDragEdit || m_pEventDrag == pEvent)
			resizeEvent(pEvent, iTimeDelta, iValueDelta, pEditCommand);
		else
			resizeEvent(pEvent, 0, 0, pEditCommand);
	}

	// On edit mode we own the new events...
	if (m_bEventDragEdit) {
		m_bEventDragEdit = false;
		m_pEventDrag = NULL;
		m_select.clear();
	}

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);
}


// Finalize the event drag-rescale.
void qtractorMidiEditor::executeDragRescale (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;
	if (m_pEventDrag == NULL)
		return;

	updateDragRescale(pScrollView, pos);

	DragTimeScale *pDts = NULL;

	int x1;
	unsigned long t1 = 0, t2;
	unsigned long d1 = 0, d2;
	int v1 = 0, v2;

	long iTimeDelta = 0;
	int iValueDelta = 0;

	switch (m_resizeMode) {
	case ResizeNoteRight:
		pDts = new DragTimeScale(m_pTimeScale, m_iOffset);
		t1 = pDts->t0 + m_pEventDrag->time();
		pDts->node = pDts->cursor.seekTick(t1);
		x1 = pDts->node->pixelFromTick(t1);
		d1 = m_pEventDrag->duration();
		x1 += m_posDelta.x();
		pDts->node = pDts->cursor.seekPixel(x1);
		t2 = pDts->node->tickFromPixel(x1);
		iTimeDelta = long(pDts->node->tickSnap(t2)) - long(t1);
		break;
	case ResizeValue:
	case ResizeValue14:
		v1 = m_pEventDrag->value();
		iValueDelta = valueDelta(pScrollView);
		break;
	case ResizePitchBend:
		v1 = m_pEventDrag->pitchBend();
		iValueDelta = valueDelta(pScrollView);
		// Fall thru...
	default:
		break;
	}

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("rescale"));

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		switch (m_resizeMode) {
		case ResizeNoteRight:
			if (pDts && d1 > 0) {
				d2 = pEvent->duration() * (d1 + iTimeDelta) / d1;
				if (d2 < 1)
					d2 = 1;
				t2 = pDts->t0 + pEvent->time();
				if (t2 > t1)
					t2 = t1 + (t2 - t1) * (d1 + iTimeDelta) / d1;
				if (t2 < pDts->t0)
					t2 = pDts->t0;
				pEditCommand->resizeEventTime(pEvent, t2 - pDts->t0, d2);
				if (pEvent == m_pEventDrag) {
					m_last.note = pEvent->note();
					m_last.duration = d2;
				}
			}
			break;
		case ResizeValue:
			if (v1) {
				v2 = pEvent->value() * (v1 + iValueDelta) / v1;
				if (v2 < 0)
					v2 = 0;
				else
				if (v2 > 127)
					v2 = 127;
				pEditCommand->resizeEventValue(pEvent, v2);
				if (pEvent == m_pEventDrag)
					m_last.value = v2;
			}
			break;
		case ResizeValue14:
			if (v1) {
				v2 = pEvent->value() * (v1 + iValueDelta) / v1;
				if (v2 < 0)
					v2 = 0;
				else
				if (v2 > 16383)
					v2 = 16383;
				pEditCommand->resizeEventValue(pEvent, v2);
				if (pEvent == m_pEventDrag)
					m_last.value = v2;
			}
			break;
		case ResizePitchBend:
			if (v1) {
				v2 = pEvent->pitchBend() * (v1 + iValueDelta) / v1;
				if (v2 < -8191)
					v2 = -8191;
				else
				if (v2 > +8191)
					v2 = +8191;
				pEditCommand->resizeEventValue(pEvent, v2);
				if (pEvent == m_pEventDrag)
					m_last.pitchBend = v2;
			}
			// Fall thru...
		default:
			break;
		}
	}

	// Local cleanup.
	if (pDts) delete pDts;

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);
}


// Finalize the event drag-paste.
void qtractorMidiEditor::executeDragPaste (
	qtractorScrollView *pScrollView, const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragMove(pScrollView, pos + m_posStep);

	const bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	const long iTimeDelta = timeDelta(pScrollView);
	const int  iNoteDelta = (bEditView ? noteDelta(pScrollView) : 0);

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("paste"));

	QList<qtractorMidiEvent *> events;

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = new qtractorMidiEvent(*iter.key());
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		const long iTime = long(pEvent->time() + pItem->delta) + iTimeDelta;
	//	if (pEvent == m_pEventDrag)
	//		iTime = timeSnap(iTime);
		pEvent->setTime(iTime);
		if (bEditView) {
			int iNote = int(pEvent->note()) + iNoteDelta;
			if (iNote < 0)
				iNote = 0;
			if (iNote > 127)
				iNote = 127;
			pEvent->setNote(iNote);
		}
		else
		if (m_pEditEvent->eventType() == qtractorMidiEvent::CONTROLLER)
			pEvent->setController(m_pEditEvent->eventParam());
		pEditCommand->insertEvent(pEvent);
		events.append(pEvent);
	}

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);

	// Remake current selection alright...
	if (!events.isEmpty()) {
		m_select.clear();
		QListIterator<qtractorMidiEvent *> event_iter(events);
		while (event_iter.hasNext()) {
			qtractorMidiEvent *pEvent = event_iter.next();
			if (pEvent) {
				QRect rectEvent, rectView;
				updateEventRects(pEvent, rectEvent, rectView);
				m_select.addItem(pEvent, rectEvent, rectView);
			}
		}
	}
}


// Apply drag(draw) event value-resize to current selection.
void qtractorMidiEditor::executeDragEventResize ( const QPoint& pos )
{
	if (m_pMidiClip == NULL)
		return;

	updateDragEventResize(pos);

	const qtractorMidiEvent::EventType eventType = m_pEditEvent->eventType();
	const int h0 = ((m_pEditEvent->viewport())->height() & ~1);	// even.
	const int y0 = (eventType == qtractorMidiEvent::PITCHBEND ? h0 >> 1 : h0);

	if (y0 < 1)
		return;

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(m_pMidiClip, tr("resize"));

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end
		= items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		if ((pItem->flags & 4) == 0)
			continue;
		if (pEvent->type() != eventType)
			continue;
		int iValue = 0;
		int y = pItem->rectEvent.top();
		const qtractorMidiEvent::EventType etype = pEvent->type();
		if (etype == qtractorMidiEvent::PITCHBEND) {
			if (y >= y0)
				y  = pItem->rectEvent.bottom();
			iValue = (8192 * (y0 - y)) / y0;
			pEditCommand->resizeEventValue(pEvent, iValue);
			if (pEvent == m_pEventDrag)
				m_last.pitchBend = iValue;
		} else {
			if (etype == qtractorMidiEvent::REGPARAM    ||
				etype == qtractorMidiEvent::NONREGPARAM ||
				etype == qtractorMidiEvent::CONTROL14)
				iValue = (16384 * (y0 - y)) / y0;
			else
				iValue = (128 * (y0 - y)) / y0;
			pEditCommand->resizeEventValue(pEvent, iValue);
			if (pEvent == m_pEventDrag)
				m_last.value = iValue;
		}
		pItem->flags &= ~4;
	}

	// Make it as an undoable command...
	m_pCommands->exec(pEditCommand);
}


// Visualize the event selection drag-move.
void qtractorMidiEditor::paintDragState (
	qtractorScrollView *pScrollView, QPainter *pPainter )
{
#ifdef CONFIG_DEBUG_0
	const QRect& rectSelect = (bEditView
		? m_select.rectView() : m_select.rectEvent());
	if (!rectSelect.isEmpty()) {
		pPainter->fillRect(QRect(
			pScrollView->contentsToViewport(rectSelect.topLeft()),
			rectSelect.size()), QColor(0, 0, 255, 40));
	}
#endif

	const bool bEditView
		= (static_cast<qtractorScrollView *> (m_pEditView) == pScrollView);

	int x1, y1;

	DragTimeScale *pDts = NULL;

	unsigned long t1 = 0, t2;
	unsigned long d1 = 0, d2;
	int v1 = 0;

	long iTimeDelta = 0;
	int iValueDelta = 0;

	if (m_dragState == DragRescale && m_pEventDrag) {
		switch (m_resizeMode) {
		case ResizeNoteRight:
			pDts = new DragTimeScale(m_pTimeScale, m_iOffset);
			t1 = pDts->t0 + m_pEventDrag->time();
			pDts->node = pDts->cursor.seekTick(t1);
			x1 = pDts->node->pixelFromTick(t1);
			d1 = m_pEventDrag->duration();
			x1 += m_posDelta.x();
			pDts->node = pDts->cursor.seekPixel(x1);
			t2 = pDts->node->tickFromPixel(x1);
			iTimeDelta = long(pDts->node->tickSnap(t2)) - long(t1);
			break;
		case ResizeValue:
		case ResizeValue14:
			v1 = m_pEventDrag->value();
			iValueDelta = valueDelta(pScrollView);
			break;
		case ResizePitchBend:
			v1 = m_pEventDrag->pitchBend();
			iValueDelta = valueDelta(pScrollView);
			// Fall thru...
		default:
			break;
		}
	}

	const qtractorMidiEditSelect::ItemList& items = m_select.items();
	qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
	const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = iter.key();
		qtractorMidiEditSelect::Item *pItem = iter.value();
		if ((pItem->flags & 1) == 0)
			continue;
		int c = (pEvent == m_pEventDrag ? 64 : 0);
		QRect rect = (bEditView ? pItem->rectView : pItem->rectEvent);
		if (!m_bEventDragEdit || pEvent == m_pEventDrag) {
			if (m_dragState == DragRescale) {
				switch (m_resizeMode) {
				case ResizeNoteRight:
					if (pDts && d1 > 0) {
						t2 = pDts->t0 + pEvent->time();
						if (t2 > t1)
							t2 = t1 + (t2 - t1) * (d1 + iTimeDelta) / d1;
						if (t2 < pDts->t0)
							t2 = pDts->t0;
						pDts->node = pDts->cursor.seekTick(t2);
						rect.setLeft(
							pDts->node->pixelFromTick(t2) - pDts->x0);
						if (bEditView || m_bNoteDuration) {
							d2 = pEvent->duration() * (d1 + iTimeDelta) / d1;
							if (d2 < 1)
								d2 = 1;
							rect.setRight(
								pDts->node->pixelFromTick(t2 + d2) - pDts->x0);
						}
						else rect.setWidth(5);
					}
					break;
				case ResizeValue:
				case ResizeValue14:
					if (v1) {
						y1 = rect.height() * (v1 + iValueDelta) / v1;
						y1 = rect.bottom() - y1;
						if (y1 < 0)
							y1 = 0;
						rect.setTop(y1);
					}
					break;
				case ResizePitchBend:
					if (v1) {
						const int y0
							= ((m_pEditEvent->viewport())->height() & ~1) >> 1;
						y1 = rect.height() * (v1 + iValueDelta) / v1;
						if (y0 > rect.top()) {
							y1 = rect.bottom() - y1;
							if (y1 < 0)
								y1 = 0;
							if (y1 > rect.bottom()) {
								rect.setTop(rect.bottom());
								rect.setBottom(y1);
							} else {
								rect.setTop(y1);
							}
						}
						else
						if (y0 < rect.bottom()) {
							y1 = rect.top() + y1;
							if (y1 < 0)
								y1 = 0;
							if (y1 < rect.top()) {
								rect.setBottom(rect.top());
								rect.setTop(y1);
							} else {
								rect.setBottom(y1);
							}
						}
					}
					// Fall thru...
				default:
					break;
				}
			}
			else
			if (m_dragState == DragResize) {
				switch (m_resizeMode) {
				case ResizeNoteLeft:
					x1 = rect.left() + m_posDelta.x();
					if (x1 < 0)
						x1 = 0;
					if (x1 > rect.right())
						x1 = rect.right();
					rect.setLeft(x1);
					if (!bEditView && !m_bNoteDuration)
						rect.setWidth(5);
					break;
				case ResizeNoteRight:
					if (bEditView || m_bNoteDuration) {
						x1 = rect.right() + m_posDelta.x();
						if (x1 < rect.left())
							x1 = rect.left();
						rect.setRight(x1);
					}
					break;
				case ResizeValue:
				case ResizeValue14:
					if (!bEditView) {
						y1 = rect.top() + m_posDelta.y();
						if (y1 < 0)
							y1 = 0;
						if (y1 > rect.bottom())
							y1 = rect.bottom();
						rect.setTop(y1);
					}
					break;
				case ResizePitchBend:
					if (!bEditView) {
						const int y0
							= ((m_pEditEvent->viewport())->height() & ~1) >> 1;
						if (y0 > rect.top()) {
							y1 = rect.top() + m_posDelta.y();
							if (y1 < 0)
								y1 = 0;
							if (y1 > rect.bottom()) {
								rect.setTop(rect.bottom());
								rect.setBottom(y1);
							} else {
								rect.setTop(y1);
							}
						}
						else
						if (y0 < rect.bottom()) {
							y1 = rect.bottom() + m_posDelta.y();
							if (y1 < 0)
								y1 = 0;
							if (y1 < rect.top()) {
								rect.setBottom(rect.top());
								rect.setTop(y1);
							} else {
								rect.setBottom(y1);
							}
						}
					}
					// Fall thru...
				default:
					break;
				}
			}	// Draw for selection/move...
			else if (bEditView)
				rect.translate(m_posDelta);
			else
				rect.translate(m_posDelta.x(), 0);
		}
		// Paint the damn bastard...
		pPainter->fillRect(QRect(
			pScrollView->contentsToViewport(rect.topLeft()),
			rect.size()), QColor(c, 0, 255 - c, 120));
	}

	// Local cleanup.
	if (pDts) delete pDts;

	// Paint drag(draw) event-value line...
	if (!bEditView && m_dragState == DragEventResize) {
		QPen pen(Qt::DotLine);
		pen.setColor(Qt::blue);
		pPainter->setPen(pen);
		pPainter->drawLine(
			pScrollView->contentsToViewport(m_posDrag),
			pScrollView->contentsToViewport(m_posDragEventResize));
	}
}


// Reset drag/select/move state.
void qtractorMidiEditor::resetDragState ( qtractorScrollView *pScrollView )
{
	if (m_bEventDragEdit) {
		const qtractorMidiEditSelect::ItemList& items = m_select.items();
		qtractorMidiEditSelect::ItemList::ConstIterator iter = items.constBegin();
		const qtractorMidiEditSelect::ItemList::ConstIterator& iter_end = items.constEnd();
		for ( ; iter != iter_end; ++iter)
			delete iter.key();
		m_select.clear();
	}

	m_pEventDrag = NULL;
	m_bEventDragEdit = false;

	m_posDelta = QPoint(0, 0);
	m_posStep  = QPoint(0, 0);

	m_posDragEventResize = QPoint(0, 0);

	m_pEditPaste = NULL;

	if (m_pRubberBand) {
		m_pRubberBand->hide();
		delete m_pRubberBand;
		m_pRubberBand = NULL;
	}

	if (pScrollView) {
		if (m_dragState != DragNone) {
			m_dragCursor = DragNone;
			pScrollView->unsetCursor();
		}
		if (m_dragState == DragMove    ||
			m_dragState == DragResize  ||
			m_dragState == DragRescale ||
			m_dragState == DragPaste   ||
			m_dragState == DragStep) {
		//	m_select.clear();
			updateContents();
		}
	}
	else
	if (m_pEditList)
		m_pEditList->dragNoteOn(-1);

	m_dragState  = DragNone;
	m_resizeMode = ResizeNone;
}


// Edit tools form page selector.
void qtractorMidiEditor::executeTool ( int iToolIndex )
{
	if (m_pMidiClip == NULL)
		return;

	qtractorMidiToolsForm toolsForm(this);
	toolsForm.setToolIndex(iToolIndex);
	if (toolsForm.exec()) {
		qtractorMidiEditCommand *pEditCommand
			= toolsForm.editCommand(m_pMidiClip, &m_select,
				m_pTimeScale->tickFromFrame(m_iOffset));
		m_pCommands->exec(pEditCommand);
	}

	QWidget::activateWindow();
	m_pEditView->setFocus();
}


// Command list accessor.
qtractorCommandList *qtractorMidiEditor::commands (void) const
{
	return m_pCommands;
}


// Update instrument default note names (nb. drum key names).
void qtractorMidiEditor::updateDefaultDrumNoteNames (void)
{
	for (int i = 12; g_aNoteNames[i].name; ++i) {
		m_noteNames.insert(g_aNoteNames[i].note,
			tr(g_aNoteNames[i].name, "noteName"));
	}
}


// Update instrument default conttroller names.
void qtractorMidiEditor::updateDefaultControllerNames (void)
{
	for (int i = 0; g_aControllerNames[i].name; ++i) {
		m_controllerNames.insert(g_aControllerNames[i].controller,
			tr(g_aControllerNames[i].name, "controllerName"));
	}
}


// Update instrument default RPN contrioller names.
void qtractorMidiEditor::updateDefaultRpnNames (void)
{
	const QMap<unsigned short, QString>& rpns = defaultRpnNames();
	QMap<unsigned short, QString>::ConstIterator rpns_iter
		= rpns.constBegin();
	const QMap<unsigned short, QString>::ConstIterator& rpns_end
		= rpns.constEnd();
	for ( ; rpns_iter != rpns_end; ++rpns_iter)
		m_rpnNames.insert(rpns_iter.key(), rpns_iter.value());
}


// Update instrument default NRPN contrioller names.
void qtractorMidiEditor::updateDefaultNrpnNames (void)
{
	const QMap<unsigned short, QString>& nrpns = defaultNrpnNames();
	QMap<unsigned short, QString>::ConstIterator nrpns_iter
		= nrpns.constBegin();
	const QMap<unsigned short, QString>::ConstIterator& nrpns_end
		= nrpns.constEnd();
	for ( ; nrpns_iter != nrpns_end; ++nrpns_iter)
		m_nrpnNames.insert(nrpns_iter.key(), nrpns_iter.value());
}


// Update instrument defined names for current clip/track.
void qtractorMidiEditor::updateInstrumentNames (void)
{
	m_noteNames.clear();
	m_controllerNames.clear();
	m_rpnNames.clear();
	m_nrpnNames.clear();

	// Update deafault controller names...
	updateDefaultControllerNames();
	updateDefaultRpnNames();
	updateDefaultNrpnNames();

	if (m_pMidiClip == NULL)
		return;

	qtractorTrack *pTrack = m_pMidiClip->track();
	if (pTrack == NULL)
		return;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorInstrumentList *pInstruments = pSession->instruments();
	if (pInstruments == NULL)
		return;

	// Get instrument name from patch descriptor...
	QString sInstrument;
	qtractorMidiBus *pMidiBus
		= static_cast<qtractorMidiBus *> (pTrack->outputBus());
	if (pMidiBus)
		sInstrument = pMidiBus->patch(pTrack->midiChannel()).instrumentName;
	// Do we have any?...
	if (sInstrument.isEmpty() || !pInstruments->contains(sInstrument)) {
		// Default drumk-key note names:
		// at least have a GM Drums (Channel 10) help...
		if (pTrack->midiChannel() == 9)
			updateDefaultDrumNoteNames();
		// No instrument definition...
		return;
	}

	// Finally, got instrument descriptor...
	const qtractorInstrument& instr = (*pInstruments)[sInstrument];

	const int iBank = pTrack->midiBank();
	const int iProg = pTrack->midiProg();

	// Default drumk-key note names:
	// at least have a GM Drums (Channel 10) help...
	if (pTrack->midiChannel() == 9 || instr.isDrum(iBank, iProg))
		updateDefaultDrumNoteNames();

	// Key note names...
	const qtractorInstrumentData& notes = instr.notes(iBank, iProg);
	qtractorInstrumentData::ConstIterator notes_iter
		= notes.constBegin();
	const qtractorInstrumentData::ConstIterator& notes_end
		= notes.constEnd();
	for ( ; notes_iter != notes_end; ++notes_iter)
		m_noteNames.insert(notes_iter.key(), notes_iter.value());

	// Controller names...
	const qtractorInstrumentData& controllers = instr.controllers();
	qtractorInstrumentData::ConstIterator controllers_iter
		= controllers.constBegin();
	const qtractorInstrumentData::ConstIterator& controllers_end
		= controllers.constEnd();
	for ( ; controllers_iter != controllers_end; ++controllers_iter) {
		m_controllerNames.insert(
			controllers_iter.key(),
			controllers_iter.value());
	}

	// RPN names...
	const qtractorInstrumentData& rpns = instr.rpns();
	qtractorInstrumentData::ConstIterator rpns_iter
		= rpns.constBegin();
	const qtractorInstrumentData::ConstIterator& rpns_end
		= rpns.constEnd();
	for ( ; rpns_iter != rpns_end; ++rpns_iter) {
		m_rpnNames.insert(
			rpns_iter.key(),
			rpns_iter.value());
	}

	// NRPN names...
	const qtractorInstrumentData& nrpns = instr.nrpns();
	qtractorInstrumentData::ConstIterator nrpns_iter
		= nrpns.constBegin();
	const qtractorInstrumentData::ConstIterator& nrpns_end
		= nrpns.constEnd();
	for ( ; nrpns_iter != nrpns_end; ++nrpns_iter) {
		m_nrpnNames.insert(
			nrpns_iter.key(),
			nrpns_iter.value());
	}
}


// Note name map accessor.
const QString qtractorMidiEditor::noteName ( unsigned char note ) const
{
	QHash<unsigned char, QString>::ConstIterator iter
		= m_noteNames.constFind(note);
	if (iter == m_noteNames.constEnd())
		return defaultNoteName(note);
	else
		return iter.value();
}


// Controller name map accessor.
const QString& qtractorMidiEditor::controllerName ( unsigned char controller ) const
{
	QHash<unsigned char, QString>::ConstIterator iter
		= m_controllerNames.constFind(controller);
	if (iter == m_controllerNames.constEnd())
		return g_sNoname;//defaultControllerName(controller);
	else
		return iter.value();
}


// RPN/NRPN map accessors.
const QMap<unsigned short, QString>& qtractorMidiEditor::rpnNames (void) const
{
	return m_rpnNames;
}

const QMap<unsigned short, QString>& qtractorMidiEditor::nrpnNames (void) const
{
	return m_nrpnNames;
}


// Control-14 map accessors.
const QString& qtractorMidiEditor::control14Name ( unsigned char controller ) const
{
	return defaultControl14Name(controller);
}


// Command execution notification slot.
void qtractorMidiEditor::updateNotifySlot ( unsigned int flags )
{
	if (flags & qtractorCommand::Refresh)
		updateContents();

	contentsChangeNotify();
}


// Emit selection/changes.
void qtractorMidiEditor::selectionChangeNotify (void)
{
	setSyncViewHoldOn(true);

	emit selectNotifySignal(this);

	m_pThumbView->update();
}

void qtractorMidiEditor::contentsChangeNotify (void)
{
	emit changeNotifySignal(this);
}


// Emit note on/off.
void qtractorMidiEditor::sendNote ( int iNote, int iVelocity )
{
	if (iVelocity == 1)
		iVelocity = m_last.value;

	emit sendNoteSignal(iNote, iVelocity);
}


// MIDI event tool tip helper.
QString qtractorMidiEditor::eventToolTip ( qtractorMidiEvent *pEvent,
	long iTimeDelta, int iNoteDelta, int iValueDelta ) const
{
	long d0 = 0;
	if (m_resizeMode == ResizeNoteRight) {
		d0 = iTimeDelta;
		iTimeDelta = 0;
	}
	else
	if (m_resizeMode == ResizeNoteLeft)
		d0 = -iTimeDelta;

	unsigned long t0 = m_pTimeScale->tickFromFrame(m_iOffset) + pEvent->time();
	t0 = (long(t0) + iTimeDelta < 0 ? 0 : t0 + iTimeDelta);
	QString sToolTip = tr("Time:\t%1\nType:\t")
		.arg(m_pTimeScale->textFromTick(t0));

	switch (pEvent->type()) {
//	case qtractorMidiEvent::NOTEOFF:
//		sToolTip += tr("Note Off (%1)").arg(int(pEvent->note()));
//		break;
	case qtractorMidiEvent::NOTEON:
		d0 = (long(pEvent->duration()) + d0 < 0 ? 0 : pEvent->duration() + d0);
		sToolTip += tr("Note On (%1) %2\nVelocity:\t%3\nDuration:\t%4")
			.arg(int(pEvent->note() + iNoteDelta))
			.arg(noteName(pEvent->note() + iNoteDelta))
			.arg(int(pEvent->velocity() + iValueDelta))
			.arg(m_pTimeScale->textFromTick(t0, true, d0));
		break;
	case qtractorMidiEvent::KEYPRESS:
		sToolTip += tr("Key Press (%1) %2\nValue:\t%3")
			.arg(int(pEvent->note() + iNoteDelta))
			.arg(noteName(pEvent->note() + iNoteDelta))
			.arg(int(pEvent->velocity() + iValueDelta));
		break;
	case qtractorMidiEvent::CONTROLLER:
		sToolTip += tr("Controller (%1)\nName:\t%2\nValue:\t%3")
			.arg(int(pEvent->controller()))
			.arg(controllerName(pEvent->controller()))
			.arg(int(pEvent->value() + iValueDelta));
		break;
	case qtractorMidiEvent::REGPARAM:
		sToolTip += tr("RPN (%1)\nName:\t%2\nValue:\t%3")
			.arg(int(pEvent->param()))
			.arg(rpnNames().value(pEvent->param()))
			.arg(int(pEvent->value() + iValueDelta));
		break;
	case qtractorMidiEvent::NONREGPARAM:
		sToolTip += tr("NRPN (%1)\nName:\t%2\nValue:\t%3")
			.arg(int(pEvent->param()))
			.arg(nrpnNames().value(pEvent->param()))
			.arg(int(pEvent->value() + iValueDelta));
		break;
	case qtractorMidiEvent::CONTROL14:
		sToolTip += tr("Control 14 (%1)\nName:\t%2\nValue:\t%3")
			.arg(int(pEvent->controller()))
			.arg(control14Name(pEvent->controller()))
			.arg(int(pEvent->value() + iValueDelta));
		break;
	case qtractorMidiEvent::PGMCHANGE:
		sToolTip += tr("Pgm Change (%1)")
			.arg(int(pEvent->value() + iValueDelta));
		break;
	case qtractorMidiEvent::CHANPRESS:
		sToolTip += tr("Chan Press (%1)")
			.arg(int(pEvent->value() + iValueDelta));
		break;
	case qtractorMidiEvent::PITCHBEND:
		sToolTip += tr("Pitch Bend (%1)")
			.arg(int(pEvent->pitchBend() + iValueDelta));
		break;
	case qtractorMidiEvent::SYSEX:
	{
		unsigned char *data = pEvent->sysex();
		unsigned short len  = pEvent->sysex_len();
		sToolTip += tr("SysEx (%1 bytes)\nData: ").arg(int(len));
		sToolTip += '{';
		sToolTip += ' ';
		for (unsigned short i = 0; i < len; ++i)
			sToolTip += QString().sprintf("%02x ", data[i]);
		sToolTip += '}';
		break;
	}
//	case qtractorMidiEvent::META:
//		sToolTip += tr("Meta");
//		break;
	default:
		sToolTip += tr("Unknown (%1)").arg(int(pEvent->type()));
		break;
	}
	
	// That's it
	return sToolTip;
}


// Keyboard event handler (common).
bool qtractorMidiEditor::keyPress ( qtractorScrollView *pScrollView,
	int iKey, Qt::KeyboardModifiers modifiers )
{
	switch (iKey) {
	case Qt::Key_Insert: // Aha, joking :)
	case Qt::Key_Return:
		if (m_dragState == DragStep) {
			executeDragMove(m_pEditView, m_posDrag);
		} else {
			const QPoint& pos = pScrollView->viewportToContents(
				pScrollView->viewport()->mapFromGlobal(QCursor::pos()));
			if (m_dragState == DragMove)
				executeDragMove(pScrollView, pos);
			else if (m_dragState == DragPaste)
				executeDragPaste(pScrollView, pos);
		}
		resetDragState(pScrollView);
		break;
	case Qt::Key_Escape:
		m_dragState = DragStep; // HACK: Force selection clearance!
		m_select.clear();
		resetDragState(pScrollView);
		break;
	case Qt::Key_Home:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(0, 0);
		} else {
			pScrollView->setContentsPos(0, pScrollView->contentsY());
		}
		break;
	case Qt::Key_End:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsWidth()  - pScrollView->width(),
				pScrollView->contentsHeight() - pScrollView->height());
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsWidth()  - pScrollView->width(),
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Left:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() - pScrollView->width(),
				pScrollView->contentsY());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() - 16,
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Right:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() + pScrollView->width(),
				pScrollView->contentsY());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX() + 16,
				pScrollView->contentsY());
		}
		break;
	case Qt::Key_Up:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - pScrollView->height());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - 16);
		}
		break;
	case Qt::Key_Down:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + pScrollView->height());
		} else if (!keyStep(iKey)) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + 16);
		}
		break;
	case Qt::Key_PageUp:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(), 16);
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() - pScrollView->height());
		}
		break;
	case Qt::Key_PageDown:
		if (modifiers & Qt::ControlModifier) {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsHeight() - pScrollView->height());
		} else {
			pScrollView->setContentsPos(
				pScrollView->contentsX(),
				pScrollView->contentsY() + pScrollView->height());
		}
		break;
	default:
		// Not handled here.
		return false;
	}

	// Done.
	return true;
}


// Keyboard step handler.
bool qtractorMidiEditor::keyStep ( int iKey )
{
	// Only applicable if something is selected...
	if (m_select.items().isEmpty())
		return false;

	// Set initial bound conditions...
	if (m_dragState == DragNone) {
		m_dragState = m_dragCursor = DragStep;
		m_rectDrag  = m_select.rectView();
		m_posDrag   = m_rectDrag.topLeft();
		m_posStep   = QPoint(0, 0);
		m_pEditView->setCursor(Qt::SizeAllCursor);
		m_pEditEvent->setCursor(Qt::SizeAllCursor);
	}

	// Now to say the truth...
	if (m_dragState != DragMove &&
		m_dragState != DragStep &&
		m_dragState != DragPaste)
		return false;

	// Make sure we've a anchor...
	if (m_pEventDrag == NULL)
		m_pEventDrag = m_select.anchorEvent();

	// Determine vertical step...
	if (iKey == Qt::Key_Up || iKey == Qt::Key_Down)  {
		const int iVerticalStep = m_pEditList->itemHeight();
		const int y0 = m_posDrag.y();
		int y1 = y0 + m_posStep.y();
		if (iKey == Qt::Key_Up)
			y1 -= iVerticalStep;
		else
			y1 += iVerticalStep;
		m_posStep.setY((y1 < 0 ? 0 : y1) - y0);
	}
	else
	// Determine horizontal step...
	if (iKey == Qt::Key_Left || iKey == Qt::Key_Right)  {
		const int x0 = m_posDrag.x() + m_pTimeScale->pixelFromFrame(m_iOffset);
		int iHorizontalStep = 0;
		int x1 = x0 + m_posStep.x();
		qtractorTimeScale::Cursor cursor(m_pTimeScale);
		qtractorTimeScale::Node *pNode = cursor.seekPixel(x1);
		unsigned short iSnapPerBeat = m_pTimeScale->snapPerBeat();
		if (iSnapPerBeat > 0)
			iHorizontalStep = pNode->pixelsPerBeat() / iSnapPerBeat;
		if (iHorizontalStep < 1)
			iHorizontalStep = 1;
		if (iKey == Qt::Key_Left)
			x1 -= iHorizontalStep;
		else
			x1 += iHorizontalStep;
		m_posStep.setX(m_pTimeScale->pixelSnap(x1 < 0 ? 0 : x1) - x0);
	}

	// Early sanity check...
	const QRect& rect = m_select.rectView();
	QPoint pos = m_posDrag;
	if (m_dragState == DragMove || m_dragState == DragPaste) {
		pos = m_pEditView->viewportToContents(
			m_pEditView->viewport()->mapFromGlobal(QCursor::pos()));
	}

	int x2 = - pos.x();
	int y2 = - pos.y();
	if (m_dragState == DragMove || m_dragState == DragPaste) {
		x2 += (m_posDrag.x() - rect.x());
		y2 += (m_posDrag.y() - rect.y());
	}

	if (m_posStep.x() < x2) {
		m_posStep.setX (x2);
	} else {
		x2 += m_pEditView->contentsWidth() - rect.width();
		if (m_posStep.x() > x2)
			m_posStep.setX (x2);
	}

	if (m_posStep.y() < y2) {
		m_posStep.setY (y2);
	} else {
		y2 += m_pEditView->contentsHeight() - rect.height();
		if (m_posStep.y() > y2)
			m_posStep.setY (y2);
	}

	// Do our deeds...
	updateDragMove(m_pEditView, pos + m_posStep);

	return true;
}


// Focus lost event.
void qtractorMidiEditor::focusOut ( qtractorScrollView *pScrollView )
{
	if (m_dragState == DragStep && m_pEditPaste == pScrollView)
		resetDragState(pScrollView);
}


// Show selection tooltip...
void qtractorMidiEditor::showToolTip (
	qtractorScrollView *pScrollView, const QRect& rect ) const
{
	if (pScrollView == NULL)
		return;

	if (!m_bToolTips)
		return;

	if (m_pTimeScale == NULL)
		return;

	const unsigned long iFrameStart = m_pTimeScale->frameSnap(
		m_iOffset + m_pTimeScale->frameFromPixel(rect.left()));
	const unsigned long iFrameEnd = m_pTimeScale->frameSnap(
		iFrameStart + m_pTimeScale->frameFromPixel(rect.width()));

	QToolTip::showText(
		QCursor::pos(),
		tr("Start:\t%1\nEnd:\t%2\nLength:\t%3")
			.arg(m_pTimeScale->textFromFrame(iFrameStart))
			.arg(m_pTimeScale->textFromFrame(iFrameEnd))
			.arg(m_pTimeScale->textFromFrame(iFrameStart, true, iFrameEnd - iFrameStart)),
		pScrollView->viewport());
}


// Temporary sync-view/follow-playhead hold state.
void qtractorMidiEditor::setSyncViewHoldOn ( bool bOn )
{
	m_iSyncViewHold = (m_bSyncViewHold && bOn ? QTRACTOR_SYNC_VIEW_HOLD : 0);
}


void qtractorMidiEditor::setSyncViewHold ( bool bSyncViewHold )
{
	m_bSyncViewHold = bSyncViewHold;
	setSyncViewHoldOn(bSyncViewHold);
}


bool qtractorMidiEditor::isSyncViewHold (void) const
{
	return (m_bSyncViewHold && m_iSyncViewHold > 0);
}


// end of qtractorMidiEditor.cpp
