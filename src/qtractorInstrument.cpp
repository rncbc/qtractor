// qtractorInstrument.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorInstrument.h"

#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QRegExp>
#include <QDate>

#include <QDomDocument>

#include <stdlib.h>
#include <stdint.h>


//----------------------------------------------------------------------
// class qtractorInstrument -- instrument definition instance class.
//

// Retrieve patch/program list for given bank address.
const qtractorInstrumentData& qtractorInstrument::patch ( int iBank ) const
{
	if (m_pData->patches.contains(iBank))
		return m_pData->patches[iBank];

	return m_pData->patches[-1];
}


// Retrieve key/notes list for given (bank, prog) pair.
const qtractorInstrumentData& qtractorInstrument::notes ( int iBank, int iProg ) const
{
	if (m_pData->keys.contains(iBank)) {
		if (m_pData->keys[iBank].contains(iProg)) {
			return m_pData->keys[iBank][iProg];
		} else {
			return m_pData->keys[iBank][-1];
		}
	}
	else if (iBank >= 0)
		return notes(-1, iProg);

	return m_pData->keys[-1][-1];
}


// Check if given (bank, prog) pair is a drum patch.
bool qtractorInstrument::isDrum ( int iBank, int iProg ) const
{
	if (m_pData->drums.contains(iBank)) {
		if (m_pData->drums[iBank].contains(iProg)) {
			return (bool) m_pData->drums[iBank][iProg];
		} else {
			return (bool) m_pData->drums[iBank][-1];
		}
	}
	else if (iBank >= 0)
		return isDrum(-1, iProg);

	return false;
}


//----------------------------------------------------------------------
// class qtractorInstrumentList -- A Cakewalk .ins file container class.
//

// Clear all contents.
void qtractorInstrumentList::clearAll (void)
{
	clear();

	m_patches.clear();
	m_notes.clear();
	m_controllers.clear();
	m_rpns.clear();
	m_nrpns.clear();

	m_files.clear();
}

	
// Special list merge method.
void qtractorInstrumentList::merge ( const qtractorInstrumentList& instruments )
{
	// Maybe its better not merging to itself.
	if (this == &instruments)
		return;
	
	// Names data lists merge...
	mergeDataList(m_patches, instruments.patches());
	mergeDataList(m_notes, instruments.notes());
	mergeDataList(m_controllers, instruments.controllers());
	mergeDataList(m_rpns, instruments.rpns());
	mergeDataList(m_nrpns, instruments.nrpns());

	// Instrument merge...
	qtractorInstrumentList::ConstIterator it = instruments.constBegin();
	const qtractorInstrumentList::ConstIterator& it_end = instruments.constEnd();
	for ( ; it != it_end; ++it) {
		qtractorInstrument& instr = (*this)[it.key()];
		instr = it.value();
	}
}


// Special instrument data list merge method.
void qtractorInstrumentList::mergeDataList (
	qtractorInstrumentDataList& dst, const qtractorInstrumentDataList& src )
{
	qtractorInstrumentDataList::ConstIterator it = src.constBegin();
	const qtractorInstrumentDataList::ConstIterator& it_end = src.constEnd();
	for ( ; it != it_end; ++it)
		dst[it.key()] = it.value();
}


// The official loaded file list.
const QStringList& qtractorInstrumentList::files (void) const
{
	return m_files;
}


// File load method.
bool qtractorInstrumentList::load ( const QString& sFilename )
{
	// Open and read from real file.
	QFile file(sFilename);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// Check for a soundfont...
	if (loadSoundFont(&file)) {
		file.close();
		appendFile(sFilename);
		return true;
	}

	// Check for a MIDINameDocument...
	if (loadMidiNameDocument(&file)) {
		file.close();
		appendFile(sFilename);
		return true;
	}

	// Proceed with regulat Cakewalk .ins file...
	file.seek(0);

	enum FileSection {
		None       = 0,
		PatchNames = 1,
		NoteNames  = 2,
		ControllerNames = 3,
		RpnNames   = 4,
		NrpnNames  = 5,
		InstrDefs  = 6
	} sect = None;

	qtractorInstrument     *pInstrument = NULL;
	qtractorInstrumentData *pData = NULL;

	QRegExp rxTitle   ("^\\[([^\\]]+)\\]$");
	QRegExp rxData    ("^([0-9]+)=(.+)$");
	QRegExp rxBasedOn ("^BasedOn=(.+)$");
	QRegExp rxBankSel ("^BankSelMethod=(0|1|2|3)$");
	QRegExp rxUseNotes("^UsesNotesAsControllers=(0|1)$");
	QRegExp rxControl ("^Control=(.+)$");
	QRegExp rxRpn     ("^RPN=(.+)$");
	QRegExp rxNrpn    ("^NRPN=(.+)$");
	QRegExp rxPatch   ("^Patch\\[([0-9]+|\\*)\\]=(.+)$");
	QRegExp rxKey     ("^Key\\[([0-9]+|\\*),([0-9]+|\\*)\\]=(.+)$");
	QRegExp rxDrum    ("^Drum\\[([0-9]+|\\*),([0-9]+|\\*)\\]=(0|1)$");
	
	const QString s0_127    = "0..127";
	const QString s1_128    = "1..128";
	const QString s0_16383  = "0..16383";
	const QString sAsterisk = "*";

	// Read the file.
	unsigned int iLine = 0;
	QTextStream ts(&file);

	while (!ts.atEnd()) {

		// Read the line.
		++iLine;
		const QString& sLine = ts.readLine().simplified();
		// If not empty, nor a comment, call the server...
		if (sLine.isEmpty() || sLine[0] == ';')
			continue;

		// Check for section intro line...
		if (sLine[0] == '.') {
			if (sLine == ".Patch Names") {
				sect = PatchNames;
			//	m_patches.clear();
				m_patches[s0_127].setName(s0_127);
				m_patches[s1_128].setName(s1_128);
			}
			else if (sLine == ".Note Names") {
				sect = NoteNames;
			//	m_notes.clear();
				m_notes[s0_127].setName(s0_127);
			}
			else if (sLine == ".Controller Names") {
				sect = ControllerNames;
			//	m_controllers.clear();
				m_controllers[s0_127].setName(s0_127);
			}
			else if (sLine == ".RPN Names") {
				sect = RpnNames;
			//	m_rpns.clear();
				m_rpns[s0_16383].setName(s0_16383);
			}
			else if (sLine == ".NRPN Names") {
				sect = NrpnNames;
			//	m_nrpns.clear();
				m_nrpns[s0_16383].setName(s0_16383);
			}
			else if (sLine == ".Instrument Definitions") {
				sect = InstrDefs;
			//  clear();
			}
			else {
				// Unknown section found...
				qWarning("%s(%d): %s: Unknown section.",
					sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
			}
			// Go on...
			continue;
		}

		// Now it depends on the section...
		switch (sect) {
			case PatchNames: {
				if (rxTitle.exactMatch(sLine)) {
					// New patch name...
					const QString& sTitle = rxTitle.cap(1);
					pData = &(m_patches[sTitle]);
					pData->setName(sTitle);
				} else if (rxBasedOn.exactMatch(sLine)) {
					pData->setBasedOn(rxBasedOn.cap(1));
				} else if (rxData.exactMatch(sLine)) {
					(*pData)[rxData.cap(1).toInt()] = rxData.cap(2);
				} else {
					qWarning("%s(%d): %s: Unknown .Patch Names entry.",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			case NoteNames: {
				if (rxTitle.exactMatch(sLine)) {
					// New note name...
					const QString& sTitle = rxTitle.cap(1);
					pData = &(m_notes[sTitle]);
					pData->setName(sTitle);
				} else if (rxBasedOn.exactMatch(sLine)) {
					pData->setBasedOn(rxBasedOn.cap(1));
				} else if (rxData.exactMatch(sLine)) {
					(*pData)[rxData.cap(1).toInt()] = rxData.cap(2);
				} else {
					qWarning("%s(%d): %s: Unknown .Note Names entry.",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			case ControllerNames: {
				if (rxTitle.exactMatch(sLine)) {
					// New controller name...
					const QString& sTitle = rxTitle.cap(1);
					pData = &(m_controllers[sTitle]);
					pData->setName(sTitle);
				} else if (rxBasedOn.exactMatch(sLine)) {
					pData->setBasedOn(rxBasedOn.cap(1));
				} else if (rxData.exactMatch(sLine)) {
					(*pData)[rxData.cap(1).toInt()] = rxData.cap(2);
				} else {
					qWarning("%s(%d): %s: Unknown .Controller Names entry.",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			case RpnNames: {
				if (rxTitle.exactMatch(sLine)) {
					// New RPN name...
					const QString& sTitle = rxTitle.cap(1);
					pData = &(m_rpns[sTitle]);
					pData->setName(sTitle);
				} else if (rxBasedOn.exactMatch(sLine)) {
					pData->setBasedOn(rxBasedOn.cap(1));
				} else if (rxData.exactMatch(sLine)) {
					(*pData)[rxData.cap(1).toInt()] = rxData.cap(2);
				} else {
					qWarning("%s(%d): %s: Unknown .RPN Names entry.",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			case NrpnNames: {
				if (rxTitle.exactMatch(sLine)) {
					// New NRPN name...
					const QString& sTitle = rxTitle.cap(1);
					pData = &(m_nrpns[sTitle]);
					pData->setName(sTitle);
				} else if (rxBasedOn.exactMatch(sLine)) {
					pData->setBasedOn(rxBasedOn.cap(1));
				} else if (rxData.exactMatch(sLine)) {
					(*pData)[rxData.cap(1).toInt()] = rxData.cap(2);
				} else {
					qWarning("%s(%d): %s: Unknown .NRPN Names entry.",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			case InstrDefs: {
				if (rxTitle.exactMatch(sLine)) {
					// New instrument definition...
					const QString& sTitle = rxTitle.cap(1);
					pInstrument = &((*this)[sTitle]);
					pInstrument->setInstrumentName(sTitle);
				} else if (rxBankSel.exactMatch(sLine)) {
					pInstrument->setBankSelMethod(
						rxBankSel.cap(1).toInt());
				} else if (rxUseNotes.exactMatch(sLine)) {
					pInstrument->setUsesNotesAsControllers(
						(bool) rxBankSel.cap(1).toInt());
				} else if (rxPatch.exactMatch(sLine)) {
					int iBank = (rxPatch.cap(1) == sAsterisk
						? -1 : rxPatch.cap(1).toInt());
					pInstrument->setPatch(iBank, m_patches[rxPatch.cap(2)]);
				} else if (rxControl.exactMatch(sLine)) {
					pInstrument->setControllers(m_controllers[rxControl.cap(1)]);
				} else if (rxRpn.exactMatch(sLine)) {
					pInstrument->setRpns(m_rpns[rxRpn.cap(1)]);
				} else if (rxNrpn.exactMatch(sLine)) {
					pInstrument->setNrpns(m_nrpns[rxNrpn.cap(1)]);
				} else if (rxKey.exactMatch(sLine)) {
					int iBank = (rxKey.cap(1) == sAsterisk
						? -1 : rxKey.cap(1).toInt());
					int iProg = (rxKey.cap(2) == sAsterisk
						? -1 : rxKey.cap(2).toInt());
					pInstrument->setNotes(iBank, iProg,	m_notes[rxKey.cap(3)]);
				} else if (rxDrum.exactMatch(sLine)) {
					int iBank = (rxDrum.cap(1) == sAsterisk
						? -1 : rxDrum.cap(1).toInt());
					int iProg = (rxDrum.cap(2) == sAsterisk
						? -1 : rxKey.cap(2).toInt());
					pInstrument->setDrum(iBank, iProg,
						(bool) rxDrum.cap(3).toInt());
				} else {
					qWarning("%s(%d): %s: Unknown .Instrument Definitions entry.",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			default:
				break;
		}
	}

	// Ok. We've read it all.
	file.close();

	// We're in business...
	appendFile(sFilename);

	return true;
}


// File save method.
bool qtractorInstrumentList::save ( const QString& sFilename ) const
{
    // Open and write into real file.
    QFile file(sFilename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

	// A visula separator line.
	const QString sepl = "; -----------------------------"
		"------------------------------------------------";

    // Write the file.
    QTextStream ts(&file);

    ts << sepl << endl;
	ts << "; " << QObject::tr("Cakewalk Instrument Definition File") << endl;
/*
    ts << ";"  << endl;
    ts << "; " << QTRACTOR_TITLE " - " << QObject::tr(QTRACTOR_SUBTITLE) << endl;
    ts << "; " << QObject::tr("Version")
       << ": " QTRACTOR_VERSION << endl;
    ts << "; " << QObject::tr("Build")
       << ": " __DATE__ " " __TIME__ << endl;
*/
    ts << ";"  << endl;
    ts << "; " << QObject::tr("File")
       << ": " << QFileInfo(sFilename).fileName() << endl;
    ts << "; " << QObject::tr("Date")
       << ": " << QDate::currentDate().toString("MMM dd yyyy")
       << " "  << QTime::currentTime().toString("hh:mm:ss") << endl;
    ts << ";"  << endl;

	// - Patch Names...
    ts << sepl << endl << endl;
	ts << ".Patch Names" << endl;
	saveDataList(ts, m_patches);

	// - Note Names...
    ts << sepl << endl << endl;
	ts << ".Note Names" << endl;
	saveDataList(ts, m_notes);

	// - Controller Names...
    ts << sepl << endl << endl;
	ts << ".Controller Names" << endl;
	saveDataList(ts, m_controllers);

	// - RPN Names...
    ts << sepl << endl << endl;
	ts << ".RPN Names" << endl;
	saveDataList(ts, m_rpns);

	// - NRPN Names...
    ts << sepl << endl << endl;
	ts << ".NRPN Names" << endl;
	saveDataList(ts, m_nrpns);

	// - Instrument Definitions...
    ts << sepl << endl << endl;
	ts << ".Instrument Definitions" << endl;
    ts << endl;
	qtractorInstrumentList::ConstIterator iter = constBegin();
	const qtractorInstrumentList::ConstIterator& iter_end = constEnd();
	for ( ; iter != iter_end; ++iter) {
		const qtractorInstrument& instr = *iter;
		ts << "[" << instr.instrumentName() << "]" << endl;
		if (instr.bankSelMethod() > 0)
		    ts << "BankSelMethod=" << instr.bankSelMethod() << endl;
		if (!instr.controllers().name().isEmpty())
			ts << "Control=" << instr.controllers().name() << endl;
		if (!instr.rpns().name().isEmpty())
			ts << "RPN=" << instr.rpns().name() << endl;
		if (!instr.nrpns().name().isEmpty())
			ts << "NRPN=" << instr.nrpns().name() << endl;
		// - Patches...
		const qtractorInstrumentPatches& patches = instr.patches();
		qtractorInstrumentPatches::ConstIterator pit = patches.constBegin();
		const qtractorInstrumentPatches::ConstIterator& pit_end = patches.constEnd();
		for ( ;	pit != pit_end; ++pit) {
			const QString& sPatch = pit.value().name();
			if (!sPatch.isEmpty()) {
				int iBank = pit.key();
				const QString& sBank = (iBank < 0
					? QString("*") : QString::number(iBank));
				ts << "Patch[" << sBank << "]="
				   << sPatch << endl;
			}
		}
		// - Keys...
		const qtractorInstrumentKeys& keys = instr.keys();
		qtractorInstrumentKeys::ConstIterator kit = keys.constBegin();
		const qtractorInstrumentKeys::ConstIterator kit_end = keys.constEnd();
		for ( ; kit != kit_end; ++kit) {
			int iBank = kit.key();
			const QString& sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			const qtractorInstrumentNotes& notes = kit.value();
			qtractorInstrumentNotes::ConstIterator nit = notes.constBegin();
			const qtractorInstrumentNotes::ConstIterator& nit_end = notes.constEnd();
			for ( ; nit != nit_end; ++nit) {
				const QString& sKey = nit.value().name();
				if (!sKey.isEmpty()) {
					int iProg = nit.key();
					const QString& sProg = (iProg < 0
						? QString("*") : QString::number(iProg));
					ts << "Key[" << sBank << "," << sProg << "]="
					   << sKey << endl;
				}
			}
		}
		// - Drums...
		const qtractorInstrumentDrums& drums = instr.drums();
		qtractorInstrumentDrums::ConstIterator dit = drums.constBegin();
		const qtractorInstrumentDrums::ConstIterator& dit_end = drums.constEnd();
		for ( ; dit != dit_end; ++dit) {
			int iBank = dit.key();
			const QString& sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			const qtractorInstrumentDrumFlags& flags = dit.value();
			qtractorInstrumentDrumFlags::ConstIterator fit = flags.constBegin();
			const qtractorInstrumentDrumFlags::ConstIterator& fit_end = flags.constEnd();
			for ( ; fit != fit_end; ++fit) {
				int iProg = fit.key();
				const QString& sProg = (iProg < 0
					? QString("*") : QString::number(iProg));
				ts << "Drum[" << sBank << "," << sProg << "]="
				   << fit.value() << endl;
			}
		}
		ts << endl;
	}

	// Done.
    file.close();

	return true;
}


void qtractorInstrumentList::saveDataList ( QTextStream& ts,
	const qtractorInstrumentDataList& list ) const
{
    ts << endl;
	qtractorInstrumentDataList::ConstIterator it = list.constBegin(); 
	const qtractorInstrumentDataList::ConstIterator& it_end = list.constEnd();
	for ( ; it != it_end; ++it) {
		const QString& sName = it.value().name();
		if (!sName.isEmpty()) {
			ts << "[" << sName << "]" << endl;
			saveData(ts, it.value());
		}
	}
}


void qtractorInstrumentList::saveData ( QTextStream& ts,
	const qtractorInstrumentData& data ) const
{
	if (!data.basedOn().isEmpty())
	    ts << "BasedOn=" << data.basedOn() << endl;
	qtractorInstrumentData::ConstIterator it = data.constBegin();
	const qtractorInstrumentData::ConstIterator& it_end = data.constEnd();
	for ( ; it != it_end; ++it)
		ts << it.key() << "=" << it.value() << endl;
	ts << endl;
}


// SoundFont chunk record header.
typedef struct _SoundFontChunk
{
	char id[4];
	int32_t size;

} SoundFontChunk;


// Special SoundFont loader.
bool qtractorInstrumentList::loadSoundFont ( QFile *pFile )
{
	pFile->seek(0);

	// Check RIFF file header...
	SoundFontChunk chunk;
	pFile->read((char *) &chunk, sizeof(chunk));
	if (::strncmp(chunk.id, "RIFF", 4) != 0)
		return false;

	// Check file id...
	pFile->read(chunk.id, sizeof(chunk.id));
	if (::strncmp(chunk.id, "sfbk", 4) != 0) 
		return false;

	for (;;) {

		pFile->read((char *) &chunk, sizeof(chunk));
		if (pFile->atEnd())
			break;

		int iSize = chunk.size;
		if (::strncmp(chunk.id, "LIST", 4) == 0) {
			// Process a list chunk.
			pFile->read(chunk.id, sizeof(chunk.id));
			iSize -= sizeof(chunk.id);
			if (::strncmp(chunk.id, "pdta", 4) == 0) {
				loadSoundFontPresets(pFile, iSize);
				break; // We have enough!
			}
		}

		// Maybe illegal; ignored; skip it.
		pFile->seek(pFile->pos() + iSize);
	}

	return true;
}


void qtractorInstrumentList::loadSoundFontPresets ( QFile *pFile, int iSize )
{
	// Parse the buffer...
	while (iSize > 0) {

		// Read a sub-chunk...
		SoundFontChunk chunk;
		pFile->read((char *) &chunk, sizeof(chunk));
		if (pFile->atEnd())
			break;
		iSize -= sizeof(chunk);

		if (::strncmp(chunk.id, "phdr", 4) == 0) {
			// Instrument name based on SoundFont file name...
			const QString& sInstrumentName
				= QFileInfo(pFile->fileName()).baseName();
			qtractorInstrument& instr = (*this)[sInstrumentName];
			instr.setInstrumentName(sInstrumentName);		
			// Preset header...
			int iDrums = 0;
			int iPresets = (chunk.size / 38);
			for (int i = 0; i < iPresets; ++i) {
				char name[20];
				int16_t prog;
				int16_t bank;
				pFile->read(name, sizeof(name));
				pFile->read((char *) &(prog), sizeof(int16_t));
				pFile->read((char *) &(bank), sizeof(int16_t));
				pFile->seek(pFile->pos() + 14);
				// Add actual preset name...
				if (i < iPresets - 1) {
					const QString& sBank = QObject::tr("%1 Bank %2")
						.arg(instr.instrumentName()).arg(int(bank));
					qtractorInstrumentData& patch = m_patches[sBank];
					patch.setName(sBank);
					instr.setPatch(int(bank), patch);
					patch[int(prog)] = QString(name).simplified();
					if (bank == 128 && iDrums == 0) {
						instr.setDrum(128, -1, true); // Usual SF2 standard ;)
						++iDrums;
					}
				}
			}
			// Enough done.
			break;
		}

		// Ignored; skip it.
		pFile->seek(pFile->pos() + chunk.size);

		iSize -= chunk.size;
	}
}


// Special MIDINameDocument loader.
bool qtractorInstrumentList::loadMidiNameDocument ( QFile *pFile )
{
	pFile->seek(0);

	QDomDocument doc;
	if (!doc.setContent(pFile))
		return false;

	QDomElement eRoot = doc.documentElement();
	if (eRoot.tagName() != "MIDINameDocument")
		return false;

	for (QDomNode nItem = eRoot.firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		// Convert item node to element...
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		if (sTagName == "MasterDeviceNames")
			loadMidiDeviceNames(&eItem);
	}

	return true;
}


void qtractorInstrumentList::loadMidiDeviceNames ( QDomElement *pElement )
{
	QString sInstrumentName;

	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		// Convert item node to element...
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		const QString& sName = eItem.attribute("Name");
		if (sTagName == "Manufacturer") {
			if (!sInstrumentName.isEmpty())
				sInstrumentName += ' ';
			sInstrumentName += eItem.text();
		}
		else
		if (sTagName == "Model") {
			if (!sInstrumentName.isEmpty())
				sInstrumentName += ' ';
			sInstrumentName += eItem.text();
		}
		else
		if (sTagName == "ChannelNameSet") {
			if (!sInstrumentName.isEmpty())
				sInstrumentName += ' ';
			sInstrumentName += sName;
			qtractorInstrument& instr = (*this)[sInstrumentName];
			instr.setInstrumentName(sInstrumentName);
			loadMidiChannelNameSet(&eItem, instr);
		}
		else
		if (sTagName == "PatchNameList") {
			qtractorInstrument& instr = (*this)[sInstrumentName];
			loadMidiPatchNameList(&eItem, instr, sName);
		}
		else
		if (sTagName == "NoteNameList")
			loadMidiNoteNameList(&eItem, sName);
		else
		if (sTagName == "ControlNameList")
			loadMidiControlNameList(&eItem, sName);
	}
}


void qtractorInstrumentList::loadMidiChannelNameSet (
	QDomElement *pElement, qtractorInstrument& instr )
{
	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		const QString& sName = eItem.attribute("Name");
		if (sTagName == "PatchBank")
			loadMidiPatchBank(&eItem, instr, sName);
		else
		if (sTagName == "PatchNameList")
			loadMidiPatchNameList(&eItem, instr, sName);
		else
		if (sTagName == "NoteNameList")
			loadMidiNoteNameList(&eItem, sName);
		else
		if (sTagName == "ControlNameList")
			loadMidiControlNameList(&eItem, sName);
		else
		if (sTagName == "UsesPatchNameList")
			instr.setPatch(-1, m_patches[sName]);
		else
		if (sTagName == "UsesNotesNameList")
			instr.setNotes(-1, -1, m_notes[sName]);
		else
		if (sTagName == "UsesControlNameList") {
			instr.setControllers(m_controllers[sName]);
			instr.setRpns(m_rpns[sName]);
			instr.setNrpns(m_nrpns[sName]);
		}
	}
}


void qtractorInstrumentList::loadMidiPatchBank (
	QDomElement *pElement, qtractorInstrument& instr, const QString& sName )
{
	int iBank = -1;

	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		QString sSubName = eItem.attribute("Name");
		if (sSubName.isEmpty())
			sSubName = sName;
		if (sTagName == "MIDICommands") {
			iBank = 0;
			for (QDomNode nCommand = eItem.firstChild();
					!nCommand.isNull();
						nCommand = nCommand.nextSibling()) {
				QDomElement eCommand = nCommand.toElement();
				if (eCommand.isNull())
					continue;
				if (eCommand.tagName() == "ControlChange") {
					unsigned short iControl
						= eCommand.attribute("Control").toUShort();
					unsigned short iValue
						= eCommand.attribute("Value").toUShort();
					if (iControl == 0)	// Bank MSB.
						iBank |= ((iValue << 7) & 0x3f80);
					else
					if (iControl == 32)	// Bank LSB.
						iBank |= (iValue & 0x7f);
				}
			}
		}
		else
		if (sTagName == "PatchNameList") {
			loadMidiPatchNameList(&eItem, instr, sSubName);
			instr.setPatch(iBank, m_patches[sSubName]);
		}
		else
		if (sTagName == "UsesPatchNameList")
			instr.setPatch(iBank, m_patches[sSubName]);
	}
}


void qtractorInstrumentList::loadMidiPatchNameList (
	QDomElement *pElement, qtractorInstrument& instr, const QString& sName )
{
	qtractorInstrumentData& patches = m_patches[sName];

	patches.setName(sName);

	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		if (sTagName == "Patch") {
			const QString& sProg = eItem.attribute("Name");
			int iProg = eItem.attribute("ProgramChange").toInt();
			patches[iProg] = sProg;
			for (QDomNode nSubItem = eItem.firstChild();
					!nSubItem.isNull();
						nSubItem = nSubItem.nextSibling()) {
				QDomElement eSubItem = nSubItem.toElement();
				if (eSubItem.isNull())
					continue;
				QString sSubName = eSubItem.attribute("Name");
				if (sSubName.isEmpty())
					sSubName = sProg;
				if (sSubName.isEmpty())
					sSubName = sName;
				const QString& sSubTagName = eSubItem.tagName();
				if (sSubTagName == "NoteNameList")
					loadMidiNoteNameList(&eSubItem, sSubName);
				else
				if (sSubTagName == "ControlNameList")
					loadMidiControlNameList(&eSubItem, sSubName);
				else
				if (sSubTagName == "UsesNoteNameList")
					instr.setNotes(-1, iProg, m_notes[sSubName]);
				else
				if (sSubTagName == "UsesControlNameList") {
					instr.setControllers(m_controllers[sSubName]);
					instr.setRpns(m_rpns[sSubName]);
					instr.setNrpns(m_nrpns[sSubName]);
				}
			}
		}
	}
}


void qtractorInstrumentList::loadMidiNoteNameList (
	QDomElement *pElement, const QString& sName )
{
	qtractorInstrumentData& notes = m_notes[sName];

	notes.setName(sName);

	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		if (sTagName == "Note") {
			const QString& sNote = eItem.attribute("Name");
			unsigned short iNote = eItem.attribute("Number").toUShort();
			notes[int(iNote)] = sNote;
		}
		else
		if (sTagName == "NoteGroup") {
			QString sSubName = eItem.attribute("Name");
			if (!sSubName.isEmpty())
				sSubName += ' ';
			for (QDomNode nSubItem = eItem.firstChild();
					!nSubItem.isNull();
						nSubItem = nSubItem.nextSibling()) {
				QDomElement eSubItem = nSubItem.toElement();
				if (eSubItem.isNull())
					continue;
				const QString& sSubTagName = eSubItem.tagName();
				if (sSubTagName == "Note") {
					const QString& sNote = eSubItem.attribute("Name");
					int iNote = eSubItem.attribute("Number").toInt();
					notes[iNote] = sSubName + sNote;
				}
			}
		}
	}
}


void qtractorInstrumentList::loadMidiControlNameList (
	QDomElement *pElement, const QString& sName )
{
	qtractorInstrumentData& controllers = m_controllers[sName];
	qtractorInstrumentData& rpns = m_rpns[sName];
	qtractorInstrumentData& nrpns = m_nrpns[sName];

	controllers.setName(sName);
	rpns.setName(sName);
	nrpns.setName(sName);

	for (QDomNode nItem = pElement->firstChild();
			!nItem.isNull();
				nItem = nItem.nextSibling()) {
		QDomElement eItem = nItem.toElement();
		if (eItem.isNull())
			continue;
		const QString& sTagName = eItem.tagName();
		if (sTagName == "Control") {
			const QString& sControlType = eItem.attribute("Type");
			const QString& sControl = eItem.attribute("Name");
			int iControl = eItem.attribute("Number").toInt();
			if (sControlType == "NRPN")
				nrpns[iControl] = sControl;
			else
			if (sControlType == "RPN")
				rpns[iControl] = sControl;
			else
		//	if (sControlType == "7bit" || sControlType == "14bit")
				controllers[iControl] = sControl;
		}
	}
}


// end of qtractorInstrument.cpp
