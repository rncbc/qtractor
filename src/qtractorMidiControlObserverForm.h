// qtractorMidiControlObserverForm.h
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiControlObserverForm_h
#define __qtractorMidiControlObserverForm_h

#include "ui_qtractorMidiControlObserverForm.h"

#include "qtractorMidiControl.h"


// Forward declarartions.
class qtractorMidiControlObserver;
class qtractorMidiControlTypeGroup;
class qtractorCtlEvent;

class QCloseEvent;
class QAction;


//----------------------------------------------------------------------------
// qtractorMidiControlObserverForm -- UI wrapper form.

class qtractorMidiControlObserverForm : public QDialog
{
	Q_OBJECT

public:

	// Pseudo-singleton instance.
	static qtractorMidiControlObserverForm *getInstance();

	// Pseudo-constructors.
	static void showInstance(
		qtractorMidiControlObserver *pMidiObserver,
		QWidget *pMidiObserverWidget = nullptr,
		QWidget *pParent = nullptr, Qt::WindowFlags wflags = Qt::WindowFlags());

	static void showInstance(QAction *pMidiObserverAction,
		QWidget *pParent = nullptr, Qt::WindowFlags wflags = Qt::WindowFlags());

	// Observer accessors.
	void setMidiObserver(qtractorMidiControlObserver *pMidiObserver);
	qtractorMidiControlObserver *midiObserver() const;

	// Observer (widget) accessors.
	void setMidiObserverWidget(QWidget *pMidiObserverWidget);
	QWidget *midiObserverWidget() const;

	// Action (control) observer accessors.
	void setMidiObserverAction(QAction *pMidiObserverAction);
	QAction *midiObserverAction() const;

	// Process incoming controller event.
	void processEvent(const qtractorCtlEvent& ctle);

	// MIDI controller/observer attachment (context menu) activator.
	static QAction *addMidiControlAction(QWidget *pParent,
		QWidget *pWidget, qtractorMidiControlObserver *pMidiObserver);
	static void midiControlAction(QWidget *pParent, QAction *pAction);
	static void midiControlMenu(QWidget *pParent, const QPoint& pos);

protected slots:

	void change();

	void click(QAbstractButton *);

	void accept();
	void reject();
	void inputs();
	void outputs();
	void reset();

	void stabilizeForm();

protected:

	// Constructor.
	qtractorMidiControlObserverForm(QWidget *pParent = nullptr,
		Qt::WindowFlags wflags = Qt::WindowFlags());
	// Destructor.
	~qtractorMidiControlObserverForm();

	// Pseudo-destructor.
	void cleanup();

	void closeEvent(QCloseEvent *pCloseEvent);

	// Add esquisite automation menu actions...
	static void addMidiControlMenu(
		QMenu *pMenu, qtractorMidiControlObserver *pMidiObserver);

private:

	// The Qt-designer UI struct...
	Ui::qtractorMidiControlObserverForm m_ui;

	// Target object.
	qtractorMidiControlObserver *m_pMidiObserver;

	// Target widget.
	QWidget *m_pMidiObserverWidget;

	// Proxy object.
	QAction *m_pMidiObserverAction;

	// Instance variables.
	int m_iDirtyCount;
	int m_iDirtySetup;

	qtractorMidiControlTypeGroup *m_pControlTypeGroup;

	// Pseudo-singleton instance.
	static qtractorMidiControlObserverForm *g_pMidiObserverForm;
};


#endif	// __qtractorMidiControlObserverForm_h


// end of qtractorMidiControlObserverForm.h
