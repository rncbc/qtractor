// qtractorMessages.h
//
/****************************************************************************
   Copyright (C) 2005, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*****************************************************************************/

#ifndef __qtractorMessages_h
#define __qtractorMessages_h

#include <qdockwindow.h>

class QSocketNotifier;
class QTextEdit;


//-------------------------------------------------------------------------
// qtractorMessages - Messages log dockable window.
//

class qtractorMessages : public QDockWindow
{
	Q_OBJECT

public:

	// Constructor.
	qtractorMessages(QWidget *pParent, const char *pszName = 0);
	// Destructor.
	~qtractorMessages();

	// Stdout/stderr capture accessors.
	bool isCaptureEnabled();
	void setCaptureEnabled(bool bCapture);

	// Message font accessors.
	QFont messagesFont();
	void setMessagesFont(const QFont & font);

	// Maximum number of message lines accessors.
	int messagesLimit();
	void setMessagesLimit(int iMessagesLimit);

	// The main utility methods.
	void appendMessages(const QString& s);
	void appendMessagesColor(const QString& s, const QString &c);
	void appendMessagesText(const QString& s);

	void scrollToBottom();

	// Stdout capture functions.
	void appendStdoutBuffer(const QString& s);
	void flushStdoutBuffer();

protected slots:

	// Stdout capture slot.
	void stdoutNotify(int fd);

private:

	// The maximum number of message lines.
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
