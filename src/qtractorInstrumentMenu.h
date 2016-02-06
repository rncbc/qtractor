// qtractorInstrumentMenu.h
//
/****************************************************************************
   Copyright (C) 2005-2016, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorInstrumentMenu_h
#define __qtractorInstrumentMenu_h

#include <QObject>
#include <QString>

// Forward decls.
class qtractorTrack;
class qtractorMidiManager;

class QMenu;
class QIcon;
class QStyle;


//----------------------------------------------------------------------
// class qtractorInstrumentMenu -- instrument definition data classes.
//

class qtractorInstrumentMenu : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorInstrumentMenu(QObject *pParent = NULL);

	// Main accessor initiator.
	void updateTrackMenu(qtractorTrack *pTrack, QMenu *pMenu);

protected slots:

	// Menu navigation.
	void updateBankMenu();
	void updateProgMenu();

	// Menu executive.
	void progActionTriggered(bool);

protected:

	bool trackMenuReset(QMenu *pMenu) const;
	bool trackMenuAdd(QMenu *pMenu, const QIcon& icon,
		qtractorMidiManager *pMidiManager, const QString& sCurrentName) const;

	bool bankMenuReset(QMenu *pBankMenu) const;
	bool bankMenuAdd(QMenu *pBankMenu, const QIcon& icon,
		qtractorMidiManager *pMidiManager,
		const QString& sInstrumentName,
		int iCurrentBank) const;

	bool progMenuReset(QMenu *pProgMenu) const;
	bool progMenuAdd(QMenu *pProgMenu, const QIcon& icon,
		qtractorMidiManager *pMidiManager,
		const QString& sInstrumentName,
		int iBank, int iCurrentProg) const;

	// A custome scrollable menu style (static).
	static QStyle *scrollableMenuStyle();

private:

	// Instance variables.
	qtractorTrack *m_pTrack;
};


#endif  // __qtractorInstrumentMenu_h


// end of qtractorInstrumentMenu.h
