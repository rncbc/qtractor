// qtractorMessages.h
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

#ifndef __qtractorMessages_h
#define __qtractorMessages_h

#include <QDockWidget>

// Forward declarations.
class qtractorMessagesTextEdit;

class QSocketNotifier;
class QFile;


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

	// Logging settings.
	bool isLogging() const;
	void setLogging(bool bEnabled, const QString& sFilename = QString());

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

	// Message executives.
	void appendMessagesLine(const QString& s);
	void appendMessagesLog(const QString& s);

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
	qtractorMessagesTextEdit *m_pMessagesTextView;

	// Stdout capture variables.
	QSocketNotifier *m_pStdoutNotifier;
	QString          m_sStdoutBuffer;
	int              m_fdStdout[2];

	// Logging stuff.
	QFile *m_pMessagesLog;	
};


#endif  // __qtractorMessages_h


// end of qtractorMessages.h
