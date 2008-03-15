// qtractorMessages.h
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMessages_h
#define __qtractorMessages_h

#include <QDockWidget>

class QSocketNotifier;
class QTextEdit;


//-------------------------------------------------------------------------
// qtractorMessages - Messages log dockable window.
//

class qtractorMessages : public QDockWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMessages(QWidget *pParent);
	// Destructor.
	~qtractorMessages();

	// Stdout/stderr capture accessors.
	bool isCaptureEnabled() const;
	void setCaptureEnabled(bool bCapture);

	// Message font accessors.
	QFont messagesFont() const;
	void setMessagesFont(const QFont & font);

	// Maximum number of message lines accessors.
	int messagesLimit() const;
	void setMessagesLimit(int iMessagesLimit);

	// The main utility methods.
	void appendMessages(const QString& s);
	void appendMessagesColor(const QString& s, const QString &c);
	void appendMessagesText(const QString& s);

	// Stdout capture functions.
	void appendStdoutBuffer(const QString& s);
	void flushStdoutBuffer();

	// History reset.
	void clear();
	
protected:

	// Just about to notify main-window that we're closing.
	void closeEvent(QCloseEvent *);

protected slots:

	// Stdout capture slot.
	void stdoutNotify(int fd);

private:

	// The maximum number of message lines and current count.
	int m_iMessagesLines;
	int m_iMessagesLimit;
	int m_iMessagesHigh;

	// The textview main widget.
	QTextEdit *m_pTextView;

	// Stdout capture variables.
	QSocketNotifier *m_pStdoutNotifier;
	QString          m_sStdoutBuffer;
	int              m_fdStdout[2];
};


#endif  // __qtractorMessages_h


// end of qtractorMessages.h
