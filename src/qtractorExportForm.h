// qtractorExportForm.h
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

#ifndef __qtractorExportForm_h
#define __qtractorExportForm_h

#include "ui_qtractorExportForm.h"

#include "qtractorTrack.h"


//----------------------------------------------------------------------------
// qtractorExportForm -- UI wrapper form.

class qtractorExportForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorExportForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorExportForm();

	void setExportType(qtractorTrack::TrackType exportType);
	qtractorTrack::TrackType exportType() const;

protected slots:

	void accept();
	void reject();
	void browseExportPath();
	void rangeChanged();
	void formatChanged(int);
	void valueChanged();
	void stabilizeForm();

private:

	// The Qt-designer UI struct...
	Ui::qtractorExportForm m_ui;

	// Instance variables...
	qtractorTrack::TrackType m_exportType;

	QString m_sExportType;
	QString m_sExportExt;

	qtractorTimeScale *m_pTimeScale;
};


#endif	// __qtractorExportForm_h


// end of qtractorExportForm.h
