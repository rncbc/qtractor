// qtractorMessages.cpp
//
/****************************************************************************
   Copyright (C) 2005-2021, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorAbout.h"
#include "qtractorMessages.h"

#include "qtractorMainForm.h"

#include <QSocketNotifier>

#include <QFile>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextStream>
#include <QTextBlock>
#include <QDateTime>
#include <QIcon>

#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
#include <unistd.h>
#include <fcntl.h>
#endif

// The default maximum number of message lines.
#define QTRACTOR_MESSAGES_MAXLINES  1000

// Notification pipe descriptors
#define QTRACTOR_MESSAGES_FDNIL    -1
#define QTRACTOR_MESSAGES_FDREAD    0
#define QTRACTOR_MESSAGES_FDWRITE   1

// Deprecated QTextStreamFunctions/Qt namespaces workaround.
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#define endl	Qt::endl
#endif


//-------------------------------------------------------------------------
// qtractorMessagesTextView - Messages log dockable child window.
//

class qtractorMessagesTextView : public QTextBrowser
{
public:

	// Constructor.
	qtractorMessagesTextView(QWidget *pParent) : QTextBrowser(pParent) {}

protected:

	// Minimum recommended.
	QSize sizeHint() const { return QTextBrowser::minimumSize(); }
};


//-------------------------------------------------------------------------
// qtractorMessages - Messages log dockable window.
//

// Constructor.
qtractorMessages::qtractorMessages ( QWidget *pParent )
	: QDockWidget(pParent)
{
	// Surely a name is crucial (e.g.for storing geometry settings)
	QDockWidget::setObjectName("qtractorMessages");

	// Initialize stdout capture stuff.
	m_pStdoutNotifier = nullptr;
	m_fdStdout[QTRACTOR_MESSAGES_FDREAD]  = QTRACTOR_MESSAGES_FDNIL;
	m_fdStdout[QTRACTOR_MESSAGES_FDWRITE] = QTRACTOR_MESSAGES_FDNIL;

	// Create local text view widget.
	m_pMessagesTextView = new qtractorMessagesTextView(this);
//  QFont font(m_pMessagesTextView->font());
//  font.setFamily("Fixed");
//  m_pMessagesTextView->setFont(font);
	m_pMessagesTextView->setLineWrapMode(QTextEdit::NoWrap);
//	m_pMessagesTextView->setReadOnly(true);
//	m_pMessagesTextView->setUndoRedoEnabled(false);
//	m_pMessagesTextView->setTextFormat(Qt::LogText);

	// Initialize default message limit.
	m_iMessagesLines = 0;
	setMessagesLimit(QTRACTOR_MESSAGES_MAXLINES);

	m_pMessagesLog = nullptr;

	// Prepare the dockable window stuff.
	QDockWidget::setWidget(m_pMessagesTextView);
//	QDockWidget::setFeatures(QDockWidget::AllDockWidgetFeatures);
	QDockWidget::setAllowedAreas(
		Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
	// Some specialties to this kind of dock window...
	QDockWidget::setMinimumHeight(120);

	// Finally set the default caption and tooltip.
	const QString& sCaption = tr("Messages");
	QDockWidget::setWindowTitle(sCaption);
	QDockWidget::setWindowIcon(QIcon(":/images/viewMessages.png"));
	QDockWidget::setToolTip(sCaption);
}


// Destructor.
qtractorMessages::~qtractorMessages (void)
{
	// Turn off and close logging.
	setLogging(false);

	// No more notifications.
	if (m_pStdoutNotifier)
		delete m_pStdoutNotifier;

	// No need to delete child widgets, Qt does it all for us.
}


// Just about to notify main-window that we're closing.
void qtractorMessages::closeEvent ( QCloseEvent * /*pCloseEvent*/ )
{
	QDockWidget::hide();

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->stabilizeForm();
}


#if defined(Q_CC_GNU) || defined(Q_CC_MINGW)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

// Set stdout/stderr blocking mode.
bool qtractorMessages::stdoutBlock ( int fd, bool bBlock ) const
{
#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
	const int iFlags = ::fcntl(fd, F_GETFL, 0);
	const bool bNonBlock = bool(iFlags & O_NONBLOCK);
	if (bBlock && bNonBlock)
		bBlock = (::fcntl(fd, F_SETFL, iFlags & ~O_NONBLOCK) == 0);
	else
	if (!bBlock && !bNonBlock)
		bBlock = (::fcntl(fd, F_SETFL, iFlags |  O_NONBLOCK) != 0);
#endif
	return bBlock;
}


// Own stdout/stderr socket notifier slot.
void qtractorMessages::stdoutNotify ( int fd )
{
#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
	// Set non-blocking reads, if not already...
	const bool bBlock = stdoutBlock(fd, false);
	// Read as much as is available...
	QString sTemp;
	char achBuffer[1024];
	const int cchBuffer = sizeof(achBuffer) - 1;
	int cchRead = ::read(fd, achBuffer, cchBuffer);
	while (cchRead > 0) {
		achBuffer[cchRead] = (char) 0;
		sTemp.append(achBuffer);
		cchRead = (bBlock ? 0 : ::read(fd, achBuffer, cchBuffer));
	}
	// Needs to be non-empty...
	if (!sTemp.isEmpty())
		appendStdoutBuffer(sTemp);
#endif
}

#if defined(Q_CC_GNU) || defined(Q_CC_MINGW)
#pragma GCC diagnostic pop
#endif


// Stdout buffer handler -- now splitted by complete new-lines...
void qtractorMessages::appendStdoutBuffer ( const QString& s )
{
	m_sStdoutBuffer.append(s);

	processStdoutBuffer();
}

void qtractorMessages::processStdoutBuffer (void)
{
	const int iLength = m_sStdoutBuffer.lastIndexOf('\n');
	if (iLength > 0) {
		const QString& sTemp = m_sStdoutBuffer.left(iLength);
		m_sStdoutBuffer.remove(0, iLength + 1);
		QStringList list = sTemp.split('\n');
		QStringListIterator iter(list);
		while (iter.hasNext())
			appendMessagesText(iter.next());
	}
}


// Stdout flusher -- show up any unfinished line...
void qtractorMessages::flushStdoutBuffer (void)
{
	if (!m_sStdoutBuffer.isEmpty()) {
		processStdoutBuffer();
		m_sStdoutBuffer.clear();
	}
}


// Stdout capture accessors.
bool qtractorMessages::isCaptureEnabled (void) const
{
	return (m_pStdoutNotifier != nullptr);
}

void qtractorMessages::setCaptureEnabled ( bool bCapture )
{
	// Flush current buffer.
	flushStdoutBuffer();

#ifdef CONFIG_DEBUG
	bCapture = false;
#endif

#if !defined(__WIN32__) && !defined(_WIN32) && !defined(WIN32)
	// Destroy if already enabled.
	if (!bCapture && m_pStdoutNotifier) {
		delete m_pStdoutNotifier;
		m_pStdoutNotifier = nullptr;
		// Close the notification pipes.
		if (m_fdStdout[QTRACTOR_MESSAGES_FDREAD] != QTRACTOR_MESSAGES_FDNIL) {
			::close(m_fdStdout[QTRACTOR_MESSAGES_FDREAD]);
			m_fdStdout[QTRACTOR_MESSAGES_FDREAD]  = QTRACTOR_MESSAGES_FDNIL;
		}
	}
	// Are we going to make up the capture?
	if (bCapture && m_pStdoutNotifier == nullptr && ::pipe(m_fdStdout) == 0) {
		::dup2(m_fdStdout[QTRACTOR_MESSAGES_FDWRITE], STDOUT_FILENO);
		::dup2(m_fdStdout[QTRACTOR_MESSAGES_FDWRITE], STDERR_FILENO);
		stdoutBlock(m_fdStdout[QTRACTOR_MESSAGES_FDWRITE], false);
		m_pStdoutNotifier = new QSocketNotifier(
			m_fdStdout[QTRACTOR_MESSAGES_FDREAD], QSocketNotifier::Read, this);
		QObject::connect(m_pStdoutNotifier,
			SIGNAL(activated(int)),
			SLOT(stdoutNotify(int)));
	}
#endif
}


// Message font accessors.
QFont qtractorMessages::messagesFont (void) const
{
	return m_pMessagesTextView->font();
}

void qtractorMessages::setMessagesFont( const QFont& font )
{
	m_pMessagesTextView->setFont(font);
}


// Maximum number of message lines accessors.
int qtractorMessages::messagesLimit (void) const
{
	return m_iMessagesLimit;
}

void qtractorMessages::setMessagesLimit ( int iMessagesLimit )
{
	m_iMessagesLimit = iMessagesLimit;
	m_iMessagesHigh  = iMessagesLimit + (iMessagesLimit >> 2);
}


// Messages logging stuff.
bool qtractorMessages::isLogging (void) const
{
	return (m_pMessagesLog != nullptr);
}

void qtractorMessages::setLogging ( bool bEnabled, const QString& sFilename )
{
	if (m_pMessagesLog) {
		appendMessages(tr("Logging stopped --- %1 ---")
			.arg(QDateTime::currentDateTime().toString()));
		m_pMessagesLog->close();
		delete m_pMessagesLog;
		m_pMessagesLog = nullptr;
	}

	if (bEnabled) {
		m_pMessagesLog = new QFile(sFilename);
		if (m_pMessagesLog->open(QIODevice::Text | QIODevice::Append)) {
			appendMessages(tr("Logging started --- %1 ---")
				.arg(QDateTime::currentDateTime().toString()));
		} else {
			delete m_pMessagesLog;
			m_pMessagesLog = nullptr;
		}
	}
}


// Messages log output method.
void qtractorMessages::appendMessagesLog ( const QString& s )
{
	if (m_pMessagesLog) {
		QTextStream(m_pMessagesLog)
			<< QTime::currentTime().toString("hh:mm:ss.zzz")
			<< ' ' << s << endl;
		m_pMessagesLog->flush();
	}
}

// Messages widget output method.
void qtractorMessages::appendMessagesLine ( const QString& s )
{
	// Check for message line limit...
	if (m_iMessagesLines > m_iMessagesHigh) {
		m_pMessagesTextView->setUpdatesEnabled(false);
		QTextCursor textCursor(m_pMessagesTextView->document()->begin());
		while (m_iMessagesLines > m_iMessagesLimit) {
			// Move cursor extending selection
			// from start to next line-block...
			textCursor.movePosition(
				QTextCursor::NextBlock, QTextCursor::KeepAnchor);
			--m_iMessagesLines;
		}
		// Remove the excessive line-blocks...
		textCursor.removeSelectedText();
		m_pMessagesTextView->setUpdatesEnabled(true);
	}

	m_pMessagesTextView->append(s);
	++m_iMessagesLines;
}


// The main utility methods.
void qtractorMessages::appendMessages ( const QString& s )
{
	appendMessagesColor(s, Qt::gray);
}

void qtractorMessages::appendMessagesColor ( const QString& s, const QColor& rgb )
{
	appendMessagesLine("<font color=\"" + rgb.name() + "\">" + s + "</font>");
	appendMessagesLog(s);
}

void qtractorMessages::appendMessagesText ( const QString& s )
{
	appendMessagesLine(s);
	appendMessagesLog(s);
}


// History reset.
void qtractorMessages::clear (void)
{
	m_iMessagesLines = 0;
	m_pMessagesTextView->clear();
}


// end of qtractorMessages.cpp
