// qtractorTimeScaleForm.h
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

	void selectNode();

	void addNode();
	void updateNode();
	void removeNode();

	void barChanged(int);
	void frameChanged(unsigned long);
	void tempoChanged(float, unsigned short, unsigned short);

	void changed();

	void tempoTap();

	void stabilizeForm();

	void contextMenu(const QPoint&);

protected:

	enum { Add = 1, Update = 2, Remove = 4 };

	unsigned int flags() const;

	void refreshNodes();

	void setCurrentNode(qtractorTimeScale::Node *pNode);
	qtractorTimeScale::Node *currentNode() const;

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
