// qtractorTimeScaleForm.h
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

#ifndef __qtractorTimeScaleForm_h
#define __qtractorTimeScaleForm_h


#include "ui_qtractorTimeScaleForm.h"

// Forward declarations...
class qtractorTimeScaleListItem;

class QTime;


//----------------------------------------------------------------------------
// qtractorTimeScaleForm -- UI wrapper form.

class qtractorTimeScaleForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorTimeScaleForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);

	void setTimeScale(qtractorTimeScale *pTimeScale);
	qtractorTimeScale *timeScale() const;

	void setFrame(unsigned long iFrame);
	unsigned long frame() const;

	unsigned short bar() const;

	bool isDirty();

protected slots:

	void reject();
	void refresh();

	void selectItem();

	void addItem();
	void updateItem();
	void removeItem();

	void refreshItems();

	void barChanged(int);
	void timeChanged(unsigned long);
	void tempoChanged();
	void changed();

	void tempoTap();
	void tempoFactor();
	void markerColor();

	void contextMenu(const QPoint&);

	void stabilizeForm();

protected:

	enum {

		AddNode      = (1 << 0),
		UpdateNode   = (1 << 1),
		RemoveNode   = (1 << 2),

		AddMarker    = (1 << 3),
		UpdateMarker = (1 << 4),
		RemoveMarker = (1 << 5)
	};

	unsigned int flags() const;

	void setCurrentItem(qtractorTimeScale::Node *pNode, unsigned long iFrame);

	void setCurrentMarker(qtractorTimeScale::Marker *pMarker);

	void ensureVisibleFrame(unsigned long iFrame);

private:

	// The Qt-designer UI struct...
	Ui::qtractorTimeScaleForm m_ui;

	// Instance variables...
	qtractorTimeScale *m_pTimeScale;

	QTime *m_pTempoTap;
	int    m_iTempoTap;
	float  m_fTempoTap;

	int m_iDirtySetup;
	int m_iDirtyCount;
	int m_iDirtyTotal;
};


#endif	// __qtractorTimeScaleForm_h


// end of qtractorTimeScaleForm.h
