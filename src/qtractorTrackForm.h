// qtractorTrackForm.h
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

#ifndef __qtractorTrackForm_h
#define __qtractorTrackForm_h

#include "ui_qtractorTrackForm.h"

#include "qtractorTrack.h"

#include <QMap>


// Forward declarations...
class qtractorMidiBus;
class qtractorMidiManager;
class qtractorCommand;


//----------------------------------------------------------------------------
// qtractorTrackForm -- UI wrapper form.

class qtractorTrackForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTrackForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorTrackForm();

	void setTrack(qtractorTrack *pTrack);
	qtractorTrack *track() const;

	const qtractorTrack::Properties& properties() const;

	qtractorTrack::TrackType trackType() const;

	void setMidiProgram(int iBank, int iProg);

protected slots:

	void accept();
	void reject();

	void stabilizeForm();
	void changed();

	void pluginListChanged();

	void trackTypeChanged();
	void inputBusNameChanged(const QString& sBusName);
	void outputBusNameChanged(const QString& sBusName);
	void busNameClicked();

	void channelChanged(int iChannel);
	void instrumentChanged(const QString& sInstrumentName);
	void bankSelMethodChanged(int iBankSelMethod);
	void bankChanged();
	void progChanged();

	void foregroundColorChanged(const QString& sText);
	void backgroundColorChanged(const QString& sText);
	void selectForegroundColor();
	void selectBackgroundColor();

	void addPlugin();
	void removePlugin();
	void moveUpPlugin();
	void moveDownPlugin();

protected:

	qtractorMidiBus *midiBus() const;

	int midiBank() const;
	int midiProg() const;

	void updateInstruments();
	void updateInstrumentsAdd(
		const QIcon& icon, qtractorMidiManager *pMidiManager);

	void updateTrackType(qtractorTrack::TrackType trackType);

	void updateChannel(int iChannel, int iBankSelMethod, int iBank, int iProg);
	void updateBanks(const QString& sInstrumentName, int iBankSelMethod, int iBank, int iProg);
	void updatePrograms(const QString& sInstrumentName, int iBank, int iProg);

	bool updateBanksAdd(const QIcon& icon, qtractorMidiManager *pMidiManager,
		const QString& sInstrumentName, int iBank, int& iBankIndex);
	bool updateProgramsAdd(const QIcon& icon, qtractorMidiManager *pMidiManager,
		const QString& sInstrumentName, int iBank, int iProg, int& iProgIndex);

	void updateColorItem(QComboBox *pComboBox, const QColor& color);
	void updateColorText(QComboBox *pComboBox, const QColor& color);

	QColor colorItem(QComboBox *pComboBox);

	// Save/load default bus names...
	void loadDefaultBusNames(qtractorTrack::TrackType trackType);
	void saveDefaultBusNames(qtractorTrack::TrackType trackType) const;

private:

	// The Qt-designer UI struct...
	Ui::qtractorTrackForm m_ui;

	// Instance variables...
	qtractorTrack *m_pTrack;
	qtractorTrack::Properties m_props;
	qtractorMidiBus *m_pMidiBus;
	QMap<int, int> m_banks;
	QMap<int, int> m_progs;
	int m_iDirtySetup;
	int m_iDirtyCount;
	int m_iDirtyPatch;
	qtractorMidiBus *m_pOldMidiBus;
	int m_iOldChannel;
	QString m_sOldInstrumentName;
	int m_iOldBankSelMethod;
	int m_iOldBank;
	int m_iOldProg;

	// Keep last acceptable command.
	qtractorCommand *m_pLastCommand;

	// MIDI bank/program observer.
	class MidiProgramObserver;

	MidiProgramObserver *m_pMidiProgramObserver;

	// Default bus names...
	static QString g_sAudioInputBusName;
	static QString g_sAudioOutputBusName;
	static QString g_sMidiInputBusName;
	static QString g_sMidiOutputBusName;
};


#endif	// __qtractorTrackForm_h


// end of qtractorTrackForm.h
