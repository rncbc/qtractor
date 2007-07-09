// qtractorInstrument.cpp
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

	return isDrum(-1, iProg);
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
	qtractorInstrumentList::ConstIterator it;
	for (it = instruments.begin(); it != instruments.end(); ++it) {
		qtractorInstrument& instr = (*this)[it.key()];
		instr = it.value();
	}
}


// Special instrument data list merge method.
void qtractorInstrumentList::mergeDataList (
	qtractorInstrumentDataList& dst, const qtractorInstrumentDataList& src )
{
	qtractorInstrumentDataList::ConstIterator it;
	for (it = src.begin(); it != src.end(); ++it)
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

	enum FileSection {
		None         = 0,
		PatchNames   = 1,
		NoteNames    = 2,
		ControlNames = 3,
		RpnNames     = 4,
		NrpnNames    = 5,
		InstrDefs    = 6
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
		iLine++;
		QString sLine = ts.readLine().simplified();
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
				sect = ControlNames;
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
				fprintf(stderr, "%s(%d): %s: Unknown section.\n",
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
					fprintf(stderr, "%s(%d): %s: Unknown .Patch Names entry.\n",
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
					fprintf(stderr, "%s(%d): %s: Unknown .Note Names entry.\n",
						sFilename.toUtf8().constData(), iLine, sLine.toUtf8().constData());
				}
				break;
			}
			case ControlNames: {
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
					fprintf(stderr, "%s(%d): %s: Unknown .Controller Names entry.\n",
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
					fprintf(stderr, "%s(%d): %s: Unknown .RPN Names entry.\n",
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
					fprintf(stderr, "%s(%d): %s: Unknown .NRPN Names entry.\n",
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
					pInstrument->setControl(m_controllers[rxControl.cap(1)]);
				} else if (rxRpn.exactMatch(sLine)) {
					pInstrument->setRpn(m_rpns[rxRpn.cap(1)]);
				} else if (rxNrpn.exactMatch(sLine)) {
					pInstrument->setNrpn(m_nrpns[rxNrpn.cap(1)]);
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
					fprintf(stderr, "%s(%d): %s: Unknown .Instrument Definitions entry.\n",
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
bool qtractorInstrumentList::save ( const QString& sFilename )
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
	qtractorInstrumentList::Iterator iter;
	for (iter = begin(); iter != end(); ++iter) {
		qtractorInstrument& instr = *iter;
		ts << "[" << instr.instrumentName() << "]" << endl;
		if (instr.bankSelMethod() > 0)
		    ts << "BankSelMethod=" << instr.bankSelMethod() << endl;
		if (!instr.control().name().isEmpty())
		    ts << "Control=" << instr.control().name() << endl;
		if (!instr.rpn().name().isEmpty())
		    ts << "RPN=" << instr.rpn().name() << endl;
		if (!instr.nrpn().name().isEmpty())
		    ts << "NRPN=" << instr.nrpn().name() << endl;
		// - Patches...
		qtractorInstrumentPatches::ConstIterator pit;
		for (pit = instr.patches().begin();
				pit != instr.patches().end(); ++pit) {
			int iBank = pit.key();
			const QString sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			ts << "Patch[" << sBank << "]=" << pit.value().name() << endl;
		}
		// - Keys...
		qtractorInstrumentKeys::ConstIterator kit;
		for (kit = instr.keys().begin(); kit != instr.keys().end(); ++kit) {
			int iBank = kit.key();
			const QString sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			const qtractorInstrumentNotes& notes = kit.value();
			qtractorInstrumentNotes::ConstIterator nit;
			for (nit = notes.begin(); nit != notes.end(); ++nit) {
				int iProg = nit.key();
				const QString sProg = (iProg < 0
					? QString("*") : QString::number(iProg));
				ts << "Key[" << sBank << "," << sProg << "]="
				   << nit.value().name() << endl;
			}
		}
		// - Drums...
		qtractorInstrumentDrums::ConstIterator dit;
		for (dit = instr.drums().begin(); dit != instr.drums().end(); ++dit) {
			int iBank = dit.key();
			const QString sBank = (iBank < 0
				? QString("*") : QString::number(iBank));
			const qtractorInstrumentDrumFlags& flags = dit.value();
			qtractorInstrumentDrumFlags::ConstIterator fit;
			for (fit = flags.begin(); fit != flags.end(); ++fit) {
				int iProg = fit.key();
				const QString sProg = (iProg < 0
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
	const qtractorInstrumentDataList& list )
{
    ts << endl;
	qtractorInstrumentDataList::ConstIterator it;
	for (it = list.begin(); it != list.end(); ++it) {
		ts << "[" << it.value().name() << "]" << endl;
		saveData(ts, it.value());
	}
}


void qtractorInstrumentList::saveData ( QTextStream& ts,
	const qtractorInstrumentData& data )
{
	if (!data.basedOn().isEmpty())
	    ts << "BasedOn=" << data.basedOn() << endl;
	qtractorInstrumentData::ConstIterator it;
	for (it = data.begin(); it != data.end(); ++it)
		ts << it.key() << "=" << it.value() << endl;
	ts << endl;
}


// end of qtractorInstrument.h
