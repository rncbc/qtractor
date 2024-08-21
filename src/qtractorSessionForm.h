// qtractorSessionForm.h
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

#ifndef __qtractorSessionForm_h
#define __qtractorSessionForm_h

#include "ui_qtractorSessionForm.h"

#include "qtractorSession.h"


//----------------------------------------------------------------------------
// qtractorSessionForm -- UI wrapper form.

class qtractorSessionForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorSessionForm(QWidget *pParent = nullptr);
	// Destructor.
	~qtractorSessionForm();

	void setSession(qtractorSession *pSession, bool bSessionDir);
	const qtractorSession::Properties& properties();
	bool isSessionDirEnabled() const;

protected slots:

	void accept();
	void reject();
	void changed();

	void changeSessionName(const QString& sSessionName);
	void changeAutoSessionDir(bool bOn);
	void changeSessionDir(const QString& sSessionDir);
	void browseSessionDir();

protected:

	void stabilizeForm();

private:

	// The Qt-designer UI struct...
	Ui::qtractorSessionForm m_ui;

	// Instance variables...
	qtractorSession::Properties m_props;

	bool    m_bNewSession;
	bool    m_bSessionDir;
	QString m_sSessionDir;

	int m_iDirtyCount;
};


#endif	// __qtractorSessionForm_h


// end of qtractorSessionForm.h
