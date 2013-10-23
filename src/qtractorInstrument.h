// qtractorInstrument.h
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

#ifndef __qtractorInstrument_h
#define __qtractorInstrument_h

#include <QStringList>
#include <QMap>


// Forward declarations.
class QTextStream;
class QFile;

class QDomElement;


//----------------------------------------------------------------------
// class qtractorInstrumentData -- instrument definition data classes.
//

class qtractorInstrumentData
{
public:

	typedef QMap<int, QString> DataMap;

	// Constructor.
	qtractorInstrumentData()
		: m_pData(new DataRef()) {}
	// Copy constructor.
	qtractorInstrumentData(const qtractorInstrumentData& data)
		{ attach(data); }

	// Destructor.
	~qtractorInstrumentData()
		{ detach(); }

	// Assignment operator.
	qtractorInstrumentData& operator= (const qtractorInstrumentData& data)
	{
		if (m_pData != data.m_pData) {
			detach();
			attach(data);
		}
		return *this;
	}

	// Accessor operator.
	QString& operator[] (int iIndex) const
		{ return m_pData->map[iIndex]; }

	// Property accessors.
	void setName(const QString& sName)
		{ m_pData->name = sName; }
	const QString& name() const { return m_pData->name; }

	void setBasedOn(const QString& sBasedOn)
		{ m_pData->basedOn = sBasedOn; }
	const QString& basedOn() const { return m_pData->basedOn; }

	// Indirect iterator stuff.
	typedef DataMap::Iterator Iterator;
	Iterator begin() { return m_pData->map.begin(); }
	Iterator end()   { return m_pData->map.end(); }

	typedef DataMap::ConstIterator ConstIterator;
	ConstIterator constBegin() const { return m_pData->map.constBegin(); }
	ConstIterator constEnd()   const { return m_pData->map.constEnd(); }

	unsigned int count() const { return m_pData->map.count(); }

	bool contains(int iKey) const
		{ return m_pData->map.contains(iKey); }

protected:

	// Copy/clone method.
	void attach(const qtractorInstrumentData& data)
		{  m_pData = data.m_pData; ++(m_pData->refCount); }

	// Destroy method.
	void detach()
		{ if (--(m_pData->refCount) == 0) delete m_pData; }

private:

	// The ref-counted data.
	struct DataRef
	{
		// Default payload constructor.
		DataRef() : refCount(1) {};
		// Payload members.
		int     refCount;
		QString name;
		QString basedOn;
		DataMap map;

	} *m_pData;
};

class qtractorInstrumentDataList
	: public QMap<QString, qtractorInstrumentData> {};

class qtractorInstrumentPatches
	: public QMap<int, qtractorInstrumentData> {};

class qtractorInstrumentNotes
	: public QMap<int, qtractorInstrumentData> {};

class qtractorInstrumentKeys
	: public QMap<int, qtractorInstrumentNotes> {};

class qtractorInstrumentDrumFlags
	: public QMap<int, int> {};

class qtractorInstrumentDrums
	: public QMap<int, qtractorInstrumentDrumFlags> {};


//----------------------------------------------------------------------
// class qtractorInstrument -- instrument definition instance class.
//

class qtractorInstrument
{
public:

	// Constructor.
	qtractorInstrument()
		: m_pData(new DataRef()) {}
	// Copy constructor.
	qtractorInstrument(const qtractorInstrument& instr)
		{ attach(instr); }

	// Destructor.
	~qtractorInstrument()
		{ detach(); }

	// Assignment operator.
	qtractorInstrument& operator= (const qtractorInstrument& instr)
	{
		if (m_pData != instr.m_pData) {
			detach();
			attach(instr);
		}
		return *this;
	}

	// Instrument title property accessors.
	void setInstrumentName(const QString& sInstrumentName)
		{ m_pData->instrumentName = sInstrumentName; }
	const QString& instrumentName() const
		{ return m_pData->instrumentName; }

	// BankSelMethod accessors.
	void setBankSelMethod(int iBankSelMethod)
		{ m_pData->bankSelMethod = iBankSelMethod; }
	int bankSelMethod() const { return m_pData->bankSelMethod; }

	void setUsesNotesAsControllers(bool bUsesNotesAsControllers)
		{ m_pData->usesNotesAsControllers = bUsesNotesAsControllers; }
	bool usesNotesAsControllers() const
		{ return m_pData->usesNotesAsControllers; }

	// Patch banks accessors.
	const qtractorInstrumentPatches& patches() const
		{ return m_pData->patches; }
	const qtractorInstrumentData& patch(int iBank) const;
	void setPatch(int iBank, const qtractorInstrumentData& patch)
		{ m_pData->patches[iBank] = patch; }

	// Control names accessors.
	void setControllersName(const QString& sControllersName)
		{ m_pData->controllers.setName(sControllersName); }
	const QString& controllersName() const
		{ return m_pData->controllers.name(); }
	void setControllers(const qtractorInstrumentData& controllers)
		{ m_pData->controllers = controllers; }
	const qtractorInstrumentData& controllers() const
		{ return m_pData->controllers; }

	// RPN names accessors.
	void setRpnsName(const QString& sRpnName)
		{ m_pData->rpns.setName(sRpnName); }
	const QString& rpnsName() const
		{ return m_pData->rpns.name(); }
	void setRpns(const qtractorInstrumentData& rpns)
		{ m_pData->rpns = rpns; }
	const qtractorInstrumentData& rpns() const
		{ return m_pData->rpns; }

	// NRPN names accessors.
	void setNrpnsName(const QString& sNrpnName)
		{ m_pData->nrpns.setName(sNrpnName); }
	const QString& nrpnsName() const
		{ return m_pData->nrpns.name(); }
	void setNrpns(const qtractorInstrumentData& nrpns)
		{ m_pData->nrpns = nrpns; }
	const qtractorInstrumentData& nrpns() const
		{ return m_pData->nrpns; }

	// Keys banks accessors.
	const qtractorInstrumentData& notes(int iBank, int iProg) const;
	void setNotes(int iBank, int iProg, const qtractorInstrumentData& notes)
		{ m_pData->keys[iBank][iProg] = notes; }
	const qtractorInstrumentKeys& keys() const
		{ return m_pData->keys; }

	// Drumflags banks accessors.
	bool isDrum(int iBank, int iProg) const;
	void setDrum(int iBank, int iProg, bool bDrum)
		{ m_pData->drums[iBank][iProg] = (int) bDrum; }
	const qtractorInstrumentDrums& drums() const
		{ return m_pData->drums; }

protected:

	// Copy/clone method.
	void attach(const qtractorInstrument& instr)
		{  m_pData = instr.m_pData; ++(m_pData->refCount); }

	// Destroy method.
	void detach()
		{ if (--(m_pData->refCount) == 0) delete m_pData; }

private:

	// The ref-counted data.
	struct DataRef
	{
		// Default payload constructor.
		DataRef() : refCount(1),
			bankSelMethod(0), usesNotesAsControllers(false) {};
		// Payload members.
		int                       refCount;
		int                       bankSelMethod;
		bool                      usesNotesAsControllers;
		QString                   instrumentName;
		qtractorInstrumentPatches patches;
		qtractorInstrumentData    controllers;
		qtractorInstrumentData    rpns;
		qtractorInstrumentData    nrpns;
		qtractorInstrumentKeys    keys;
		qtractorInstrumentDrums   drums;

	} *m_pData;
};


//----------------------------------------------------------------------
// class qtractorInstrumentList -- A Cakewalk .ins file container class.
//

class qtractorInstrumentList : public QMap<QString, qtractorInstrument>
{
public:

	// Open file methods.
	bool load(const QString& sFilename);
	bool save(const QString& sFilename) const;

	// The official loaded file list.
	const QStringList& files() const;

	// Manage a file list (out of sync)
	void appendFile(const QString& sFilename)
	    { m_files.append(sFilename); }

	void removeFile(const QString& sFilename)
	{
		int iFile = m_files.indexOf(sFilename);
		if (iFile >= 0)
			m_files.removeAt(iFile);
	}

	// Patch Names definition accessors.
	const qtractorInstrumentDataList& patches() const
		{ return m_patches; }
	const qtractorInstrumentData& patches(const QString& sName)
		{ return m_patches[sName]; }

	// Note Names definition accessors.
	const qtractorInstrumentDataList& notes() const
		{ return m_notes; }
	const qtractorInstrumentData& notes(const QString& sName)
		{ return m_notes[sName]; }

	// Controller Names definition accessors.
	const qtractorInstrumentDataList& controllers() const
		{ return m_controllers; }
	const qtractorInstrumentData& controllers(const QString& sName)
		{ return m_controllers[sName]; }

	// RPN Names definition accessors.
	const qtractorInstrumentDataList& rpns() const
		{ return m_rpns; }
	const qtractorInstrumentData& rpns(const QString& sName)
		{ return m_rpns[sName]; }

	// NRPN Names definition accessors.
	const qtractorInstrumentDataList& nrpns() const
		{ return m_nrpns; }
	const qtractorInstrumentData& nrpns(const QString& sName)
		{ return m_nrpns[sName]; }

	// Clear all contents.
	void clearAll();

	// Special instrument list merge method.
	void merge(const qtractorInstrumentList& instruments);

protected:

	// Internal instrument data list save method helpers.
	void saveDataList(QTextStream& ts, const qtractorInstrumentDataList& list) const;
	void saveData(QTextStream& ts, const qtractorInstrumentData& data) const;

	// Special instrument data list merge method.
	void mergeDataList(qtractorInstrumentDataList& dst,
		const qtractorInstrumentDataList& src);

	// Special SoundFont loader.
	bool loadSoundFont(QFile *pFile);
	void loadSoundFontPresets(QFile *pFile, int iSize);

	// Special MIDINameDocument loader.
	bool loadMidiNameDocument(QFile *pFile);

	void loadMidiDeviceNames(
		QDomElement *pElement);
	void loadMidiChannelNameSet(
		QDomElement *pElement, qtractorInstrument& instr);
	void loadMidiPatchBank(
		QDomElement *pElement, qtractorInstrument& instr, const QString& sName);
	void loadMidiPatchNameList(
		QDomElement *pElement, qtractorInstrument& instr, const QString& sName);
	void loadMidiNoteNameList(
		QDomElement *pElement, const QString& sName);
	void loadMidiControlNameList(
		QDomElement *pElement, const QString& sName);

private:

	// To hold the names definition lists.
	qtractorInstrumentDataList m_patches;
	qtractorInstrumentDataList m_notes;
	qtractorInstrumentDataList m_controllers;
	qtractorInstrumentDataList m_rpns;
	qtractorInstrumentDataList m_nrpns;
	
	// To old the official file list.
	QStringList m_files;
};


#endif  // __qtractorInstrument_h


// end of qtractorInstrument.h
