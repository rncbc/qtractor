// qtractor.cpp
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

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QTranslator>
#include <QLocale>

#include <QStyleFactory>


#define CONFIG_QUOTE1(x) #x
#define CONFIG_QUOTED(x) CONFIG_QUOTE1(x)

#if defined(DATADIR)
#define CONFIG_DATADIR CONFIG_QUOTED(DATADIR)
#else
#define CONFIG_DATADIR CONFIG_PREFIX "/share"
#endif


//-------------------------------------------------------------------------
// Singleton application instance stuff (Qt/X11 only atm.)
//

#if defined(Q_WS_X11)

#ifdef CONFIG_VST
#include "qtractorVstPlugin.h"
#endif

#ifdef CONFIG_XUNIQUE

#include <QX11Info>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#define QTRACTOR_XUNIQUE "qtractorApplication"

#endif	// CONFIG_XUNIQUE
#endif	// Q_WS_X11

class qtractorApplication : public QApplication
{
public:

	// Constructor.
	qtractorApplication(int& argc, char **argv) : QApplication(argc, argv),
		m_pQtTranslator(0), m_pMyTranslator(0), m_pWidget(0)	
	{
		// Load translation support.
		QLocale loc;
		if (loc.language() != QLocale::C) {
			// Try own Qt translation...
			m_pQtTranslator = new QTranslator(this);
			QString sLocName = "qt_" + loc.name();
			QString sLocPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
			if (m_pQtTranslator->load(sLocName, sLocPath)) {
				QApplication::installTranslator(m_pQtTranslator);
			} else {
				delete m_pQtTranslator;
				m_pQtTranslator = 0;
			#ifdef CONFIG_DEBUG
				qWarning("Warning: no translation found for '%s' locale: %s/%s.qm",
					loc.name().toUtf8().constData(),
					sLocPath.toUtf8().constData(),
					sLocName.toUtf8().constData());
			#endif
			}
			// Try own application translation...
			m_pMyTranslator = new QTranslator(this);
			sLocName = "qtractor_" + loc.name();
			if (m_pMyTranslator->load(sLocName, sLocPath)) {
				QApplication::installTranslator(m_pMyTranslator);
			} else {
				sLocPath = CONFIG_DATADIR "/qtractor/translations";
				if (m_pMyTranslator->load(sLocName, sLocPath)) {
					QApplication::installTranslator(m_pMyTranslator);
				} else {
					delete m_pMyTranslator;
					m_pMyTranslator = 0;
				#ifdef CONFIG_DEBUG
					qWarning("Warning: no translation found for '%s' locale: %s/%s.qm",
						loc.name().toUtf8().constData(),
						sLocPath.toUtf8().constData(),
						sLocName.toUtf8().constData());
				#endif
				}
			}
		}
	#if defined(Q_WS_X11)
	#ifdef CONFIG_XUNIQUE
		m_pDisplay = QX11Info::display();
		m_aUnique  = XInternAtom(m_pDisplay, QTRACTOR_XUNIQUE, false);
		XGrabServer(m_pDisplay);
		m_wOwner = XGetSelectionOwner(m_pDisplay, m_aUnique);
		XUngrabServer(m_pDisplay);
	#endif
	#endif
	}

	// Destructor.
	~qtractorApplication()
	{
		if (m_pMyTranslator) delete m_pMyTranslator;
		if (m_pQtTranslator) delete m_pQtTranslator;
	}

	// Main application widget accessors.
	void setMainWidget(QWidget *pWidget)
	{
		m_pWidget = pWidget;
	#if defined(Q_WS_X11)
	#ifdef CONFIG_XUNIQUE
		XGrabServer(m_pDisplay);
		m_wOwner = m_pWidget->winId();
		XSetSelectionOwner(m_pDisplay, m_aUnique, m_wOwner, CurrentTime);
		XUngrabServer(m_pDisplay);
	#endif
	#endif
	}

	QWidget *mainWidget() const { return m_pWidget; }

	// Check if another instance is running,
    // and raise its proper main widget...
	bool setup()
	{
	#if defined(Q_WS_X11)
	#ifdef CONFIG_XUNIQUE
		if (m_wOwner != None) {
			// First, notify any freedesktop.org WM
			// that we're about to show the main widget...
			Screen *pScreen = XDefaultScreenOfDisplay(m_pDisplay);
			int iScreen = XScreenNumberOfScreen(pScreen);
			XEvent ev;
			memset(&ev, 0, sizeof(ev));
			ev.xclient.type = ClientMessage;
			ev.xclient.display = m_pDisplay;
			ev.xclient.window = m_wOwner;
			ev.xclient.message_type = XInternAtom(m_pDisplay, "_NET_ACTIVE_WINDOW", false);
			ev.xclient.format = 32;
			ev.xclient.data.l[0] = 0; // Source indication.
			ev.xclient.data.l[1] = 0; // Timestamp.
			ev.xclient.data.l[2] = 0; // Requestor's currently active window (none)
			ev.xclient.data.l[3] = 0;
			ev.xclient.data.l[4] = 0;
			XSelectInput(m_pDisplay, m_wOwner, StructureNotifyMask);
			XSendEvent(m_pDisplay, RootWindow(m_pDisplay, iScreen), false,
				(SubstructureNotifyMask | SubstructureRedirectMask), &ev);
			XSync(m_pDisplay, false);
			XRaiseWindow(m_pDisplay, m_wOwner);
			// And then, let it get caught on destination
			// by QApplication::x11EventFilter...
			QByteArray value = QTRACTOR_XUNIQUE;
			XChangeProperty(
				m_pDisplay,
				m_wOwner,
				m_aUnique,
				m_aUnique, 8,
				PropModeReplace,
				(unsigned char *) value.data(),
				value.length());
			// Done.
			return true;
		}
	#endif
	#endif
		return false;
	}

#if defined(Q_WS_X11)
	bool x11EventFilter(XEvent *pEv)
	{
	#ifdef CONFIG_XUNIQUE
		if (m_pWidget && m_wOwner != None
			&& pEv->type == PropertyNotify
			&& pEv->xproperty.window == m_wOwner
			&& pEv->xproperty.state == PropertyNewValue) {
			// Always check whether our property-flag is still around...
			Atom aType;
			int iFormat = 0;
			unsigned long iItems = 0;
			unsigned long iAfter = 0;
			unsigned char *pData = 0;
			if (XGetWindowProperty(
					m_pDisplay,
					m_wOwner,
					m_aUnique,
					0, 1024,
					false,
					m_aUnique,
					&aType,
					&iFormat,
					&iItems,
					&iAfter,
					&pData) == Success
				&& aType == m_aUnique && iItems > 0 && iAfter == 0) {
				// Avoid repeating it-self...
				XDeleteProperty(m_pDisplay, m_wOwner, m_aUnique);
				// Just make it always shows up fine...
				m_pWidget->show();
				m_pWidget->raise();
				m_pWidget->activateWindow();
			}
			// Free any left-overs...
			if (iItems > 0 && pData)
				XFree(pData);
		}
	#endif
	#ifdef CONFIG_VST
		// Let xevents be processed by VST plugin editors...
		if (qtractorVstPlugin::x11EventFilter(pEv))
			return true;
	#endif
		return QApplication::x11EventFilter(pEv);
	}
#endif
	
private:

	// Translation support.
	QTranslator *m_pQtTranslator;
	QTranslator *m_pMyTranslator;

	// Instance variables.
	QWidget *m_pWidget;

#if defined(Q_WS_X11)
#ifdef CONFIG_XUNIQUE
	Display *m_pDisplay;
	Atom     m_aUnique;
	Window   m_wOwner;
#endif
#endif
};


//-------------------------------------------------------------------------
// stacktrace - Signal crash handler.
//

#ifdef CONFIG_STACKTRACE
#if defined(__GNUC__) && defined(Q_OS_LINUX)

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

void stacktrace ( int signo )
{
	pid_t pid;
	int rc;
	int status = 0;
	char cmd[80];

	// Reinstall default handler; prevent race conditions...
	signal(signo, SIG_DFL);

	static const char *shell  = "/bin/sh";
	static const char *format = "gdb -q --batch --pid=%d"
		" --eval-command='thread apply all bt'";

	snprintf(cmd, sizeof(cmd), format, (int) getpid());

	pid = fork();

	// Fork failure!
	if (pid < 0)
		return;

	// Fork child...
	if (pid == 0) {
		execl(shell, shell, "-c", cmd, NULL);
		_exit(1);
		return;
	}

	// Parent here: wait for child to terminate...
	do { rc = waitpid(pid, &status, 0); }
	while ((rc < 0) && (errno == EINTR));

	// Dispatch any logging, if any...
	QApplication::processEvents(QEventLoop::AllEvents, 3000);

	// Make sure everyone terminates...
	kill(pid, SIGTERM);
	_exit(1);
}

#endif
#endif


//-------------------------------------------------------------------------
// update_palette() - Application palette settler.
//

static bool update_palette ( QPalette& pal, const QString& sCustomColorTheme )
{
	if (sCustomColorTheme.isEmpty())
		return false;

	if (sCustomColorTheme == "Wonton Soup") {
		pal.setColor(QPalette::Active,   QPalette::Window, QColor(73, 78, 88));
		pal.setColor(QPalette::Inactive, QPalette::Window, QColor(73, 78, 88));
		pal.setColor(QPalette::Disabled, QPalette::Window, QColor(64, 68, 77));
		pal.setColor(QPalette::Active,   QPalette::WindowText, QColor(182, 193, 208));
		pal.setColor(QPalette::Inactive, QPalette::WindowText, QColor(182, 193, 208));
		pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(97, 104, 114));
		pal.setColor(QPalette::Active,   QPalette::Base, QColor(60, 64, 72));
		pal.setColor(QPalette::Inactive, QPalette::Base, QColor(60, 64, 72));
		pal.setColor(QPalette::Disabled, QPalette::Base, QColor(52, 56, 63));
		pal.setColor(QPalette::Active,   QPalette::AlternateBase, QColor(67, 71, 80));
		pal.setColor(QPalette::Inactive, QPalette::AlternateBase, QColor(67, 71, 80));
		pal.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(59, 62, 70));
		pal.setColor(QPalette::Active,   QPalette::ToolTipBase, QColor(182, 193, 208));
		pal.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(182, 193, 208));
		pal.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(182, 193, 208));
		pal.setColor(QPalette::Active,   QPalette::ToolTipText, QColor(42, 44, 48));
		pal.setColor(QPalette::Inactive, QPalette::ToolTipText, QColor(42, 44, 48));
		pal.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(42, 44, 48));
		pal.setColor(QPalette::Active,   QPalette::Text, QColor(210, 222, 240));
		pal.setColor(QPalette::Inactive, QPalette::Text, QColor(210, 222, 240));
		pal.setColor(QPalette::Disabled, QPalette::Text, QColor(99, 105, 115));
		pal.setColor(QPalette::Active,   QPalette::Button, QColor(82, 88, 99));
		pal.setColor(QPalette::Inactive, QPalette::Button, QColor(82, 88, 99));
		pal.setColor(QPalette::Disabled, QPalette::Button, QColor(72, 77, 87));
		pal.setColor(QPalette::Active,   QPalette::ButtonText, QColor(210, 222, 240));
		pal.setColor(QPalette::Inactive, QPalette::ButtonText, QColor(210, 222, 240));
		pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(111, 118, 130));
		pal.setColor(QPalette::Active,   QPalette::BrightText, QColor(255, 255, 255));
		pal.setColor(QPalette::Inactive, QPalette::BrightText, QColor(255, 255, 255));
		pal.setColor(QPalette::Disabled, QPalette::BrightText, QColor(255, 255, 255));
		pal.setColor(QPalette::Active,   QPalette::Light, QColor(95, 101, 114));
		pal.setColor(QPalette::Inactive, QPalette::Light, QColor(95, 101, 114));
		pal.setColor(QPalette::Disabled, QPalette::Light, QColor(86, 92, 104));
		pal.setColor(QPalette::Active,   QPalette::Midlight, QColor(84, 90, 101));
		pal.setColor(QPalette::Inactive, QPalette::Midlight, QColor(84, 90, 101));
		pal.setColor(QPalette::Disabled, QPalette::Midlight, QColor(75, 81, 91));
		pal.setColor(QPalette::Active,   QPalette::Dark, QColor(40, 43, 49));
		pal.setColor(QPalette::Inactive, QPalette::Dark, QColor(40, 43, 49));
		pal.setColor(QPalette::Disabled, QPalette::Dark, QColor(35, 38, 43));
		pal.setColor(QPalette::Active,   QPalette::Mid, QColor(63, 68, 76));
		pal.setColor(QPalette::Inactive, QPalette::Mid, QColor(63, 68, 76));
		pal.setColor(QPalette::Disabled, QPalette::Mid, QColor(56, 59, 67));
		pal.setColor(QPalette::Active,   QPalette::Shadow, QColor(29, 31, 35));
		pal.setColor(QPalette::Inactive, QPalette::Shadow, QColor(29, 31, 35));
		pal.setColor(QPalette::Disabled, QPalette::Shadow, QColor(25, 27, 30));
		pal.setColor(QPalette::Active,   QPalette::Highlight, QColor(120, 136, 156));
		pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor(81, 90, 103));
		pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(64, 68, 77));
		pal.setColor(QPalette::Active,   QPalette::HighlightedText, QColor(209, 225, 244));
		pal.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(182, 193, 208));
		pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(97, 104, 114));
		pal.setColor(QPalette::Active,   QPalette::Link, QColor(156, 212, 255));
		pal.setColor(QPalette::Inactive, QPalette::Link, QColor(156, 212, 255));
		pal.setColor(QPalette::Disabled, QPalette::Link, QColor(82, 102, 119));
		pal.setColor(QPalette::Active,   QPalette::LinkVisited, QColor(64, 128, 255));
		pal.setColor(QPalette::Inactive, QPalette::LinkVisited, QColor(64, 128, 255));
		pal.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(54, 76, 119));
	}
	else
	if (sCustomColorTheme == "KXStudio") {
		pal.setColor(QPalette::Active,   QPalette::Window, QColor(17, 17, 17));
		pal.setColor(QPalette::Inactive, QPalette::Window, QColor(17, 17, 17));
		pal.setColor(QPalette::Disabled, QPalette::Window, QColor(14, 14, 14));
		pal.setColor(QPalette::Active,   QPalette::WindowText, QColor(240, 240, 240));
		pal.setColor(QPalette::Inactive, QPalette::WindowText, QColor(240, 240, 240));
		pal.setColor(QPalette::Disabled, QPalette::WindowText, QColor(83, 83, 83));
		pal.setColor(QPalette::Active,   QPalette::Base, QColor(7, 7, 7));
		pal.setColor(QPalette::Inactive, QPalette::Base, QColor(7, 7, 7));
		pal.setColor(QPalette::Disabled, QPalette::Base, QColor(6, 6, 6));
		pal.setColor(QPalette::Active,   QPalette::AlternateBase, QColor(14, 14, 14));
		pal.setColor(QPalette::Inactive, QPalette::AlternateBase, QColor(14, 14, 14));
		pal.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(12, 12, 12));
		pal.setColor(QPalette::Active,   QPalette::ToolTipBase, QColor(4, 4, 4));
		pal.setColor(QPalette::Inactive, QPalette::ToolTipBase, QColor(4, 4, 4));
		pal.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(4, 4, 4));
		pal.setColor(QPalette::Active,   QPalette::ToolTipText, QColor(230, 230, 230));
		pal.setColor(QPalette::Inactive, QPalette::ToolTipText, QColor(230, 230, 230));
		pal.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(230, 230, 230));
		pal.setColor(QPalette::Active,   QPalette::Text, QColor(230, 230, 230));
		pal.setColor(QPalette::Inactive, QPalette::Text, QColor(230, 230, 230));
		pal.setColor(QPalette::Disabled, QPalette::Text, QColor(74, 74, 74));
		pal.setColor(QPalette::Active,   QPalette::Button, QColor(28, 28, 28));
		pal.setColor(QPalette::Inactive, QPalette::Button, QColor(28, 28, 28));
		pal.setColor(QPalette::Disabled, QPalette::Button, QColor(24, 24, 24));
		pal.setColor(QPalette::Active,   QPalette::ButtonText, QColor(240, 240, 240));
		pal.setColor(QPalette::Inactive, QPalette::ButtonText, QColor(240, 240, 240));
		pal.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(90, 90, 90));
		pal.setColor(QPalette::Active,   QPalette::BrightText, QColor(255, 255, 255));
		pal.setColor(QPalette::Inactive, QPalette::BrightText, QColor(255, 255, 255));
		pal.setColor(QPalette::Disabled, QPalette::BrightText, QColor(255, 255, 255));
		pal.setColor(QPalette::Active,   QPalette::Light, QColor(191, 191, 191));
		pal.setColor(QPalette::Inactive, QPalette::Light, QColor(191, 191, 191));
		pal.setColor(QPalette::Disabled, QPalette::Light, QColor(191, 191, 191));
		pal.setColor(QPalette::Active,   QPalette::Midlight, QColor(155, 155, 155));
		pal.setColor(QPalette::Inactive, QPalette::Midlight, QColor(155, 155, 155));
		pal.setColor(QPalette::Disabled, QPalette::Midlight, QColor(155, 155, 155));
		pal.setColor(QPalette::Active,   QPalette::Dark, QColor(129, 129, 129));
		pal.setColor(QPalette::Inactive, QPalette::Dark, QColor(129, 129, 129));
		pal.setColor(QPalette::Disabled, QPalette::Dark, QColor(129, 129, 129));
		pal.setColor(QPalette::Active,   QPalette::Mid, QColor(94, 94, 94));
		pal.setColor(QPalette::Inactive, QPalette::Mid, QColor(94, 94, 94));
		pal.setColor(QPalette::Disabled, QPalette::Mid, QColor(94, 94, 94));
		pal.setColor(QPalette::Active,   QPalette::Shadow, QColor(155, 155, 155));
		pal.setColor(QPalette::Inactive, QPalette::Shadow, QColor(155, 155, 155));
		pal.setColor(QPalette::Disabled, QPalette::Shadow, QColor(155, 155, 155));
		pal.setColor(QPalette::Active,   QPalette::Highlight, QColor(60, 60, 60));
		pal.setColor(QPalette::Inactive, QPalette::Highlight, QColor(34, 34, 34));
		pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(14, 14, 14));
		pal.setColor(QPalette::Active,   QPalette::HighlightedText, QColor(255, 255, 255));
		pal.setColor(QPalette::Inactive, QPalette::HighlightedText, QColor(240, 240, 240));
		pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(83, 83, 83));
		pal.setColor(QPalette::Active,   QPalette::Link, QColor(100, 100, 230));
		pal.setColor(QPalette::Inactive, QPalette::Link, QColor(100, 100, 230));
		pal.setColor(QPalette::Disabled, QPalette::Link, QColor(34, 34, 74));
		pal.setColor(QPalette::Active,   QPalette::LinkVisited, QColor(230, 100, 230));
		pal.setColor(QPalette::Inactive, QPalette::LinkVisited, QColor(230, 100, 230));
		pal.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(74, 34, 74));
		return true;
	}

	return false;
}


//-------------------------------------------------------------------------
// main - The main program trunk.
//

int main ( int argc, char **argv )
{
	Q_INIT_RESOURCE(qtractor);
#ifdef CONFIG_STACKTRACE
#if defined(__GNUC__) && defined(Q_OS_LINUX)
	signal(SIGILL,  stacktrace);
	signal(SIGFPE,  stacktrace);
	signal(SIGSEGV, stacktrace);
	signal(SIGABRT, stacktrace);
	signal(SIGBUS,  stacktrace);
#endif
#endif
	qtractorApplication app(argc, argv);

	// Construct default settings; override with command line arguments.
	qtractorOptions options;
	if (!options.parse_args(app.arguments())) {
		app.quit();
		return 1;
	}

	// Have another instance running?
	if (app.setup()) {
		app.quit();
		return 2;
	}

	// Custom style theme...
	if (!options.sCustomStyleTheme.isEmpty())
		app.setStyle(QStyleFactory::create(options.sCustomStyleTheme));

	// Custom color theme (eg. "KXStudio")...
	QPalette pal(app.palette());
	unsigned int iUpdatePalette = 0;
	if (update_palette(pal, options.sCustomColorTheme))
		++iUpdatePalette;
	// Dark themes grayed/disabled color group fix...
	if (pal.base().color().value() < 0x7f) {
		const QColor& color = pal.window().color();
		const int iGroups = int(QPalette::Active | QPalette::Inactive) + 1;
		for (int i = 0; i < iGroups; ++i) {
			const QPalette::ColorGroup group = QPalette::ColorGroup(i);
			pal.setBrush(group, QPalette::Light,    color.lighter(140));
			pal.setBrush(group, QPalette::Midlight, color.lighter(100));
			pal.setBrush(group, QPalette::Mid,      color.lighter(90));
			pal.setBrush(group, QPalette::Dark,     color.darker(160));
			pal.setBrush(group, QPalette::Shadow,   color.darker(180));
		}
		pal.setColorGroup(QPalette::Disabled,
			pal.windowText().color().darker(),
			pal.button(),
			pal.light(),
			pal.dark(),
			pal.mid(),
			pal.text().color().darker(),
			pal.text().color().lighter(),
			pal.base(),
			pal.window());
	#if QT_VERSION >= 0x050000
		pal.setColor(QPalette::Disabled,
			QPalette::Highlight, pal.mid().color());
		pal.setColor(QPalette::Disabled,
			QPalette::ButtonText, pal.mid().color());
	#endif
		++iUpdatePalette;
	}
	// New palette update?
	if (iUpdatePalette > 0)
		app.setPalette(pal);

	// Set default base font...
	if (options.iBaseFontSize > 0)
		app.setFont(QFont(app.font().family(), options.iBaseFontSize));

	// Construct, setup and show the main form (a pseudo-singleton).
	qtractorMainForm w;
	w.setup(&options);
	w.show();

	// Settle this one as application main widget...
	app.setMainWidget(&w);

	return app.exec();
}


// end of qtractor.cpp
