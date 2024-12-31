// qtractor.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractor.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include "qtractorPaletteForm.h"

#include <QLibraryInfo>
#include <QTranslator>
#include <QLocale>

#include <QDir>

#include <QStyleFactory>


#ifndef CONFIG_PREFIX
#define CONFIG_PREFIX	"/usr/local"
#endif

#ifndef CONFIG_BINDIR
#define CONFIG_BINDIR	CONFIG_PREFIX "/bin"
#endif

#ifndef CONFIG_DATADIR
#define CONFIG_DATADIR	CONFIG_PREFIX "/share"
#endif

#ifndef CONFIG_LIBDIR
#if defined(__x86_64__)
#define CONFIG_LIBDIR	CONFIG_PREFIX "/lib64"
#else
#define CONFIG_LIBDIR	CONFIG_PREFIX "/lib"
#endif
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt4/plugins"
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt5/plugins"
#else
#define CONFIG_PLUGINSDIR CONFIG_LIBDIR "/qt6/plugins"
#endif

#ifdef CONFIG_X11
#ifdef CONFIG_VST2
#include "qtractorVst2Plugin.h"
#endif
#endif


//-------------------------------------------------------------------------
// Singleton application instance stuff (Qt/X11 only atm.)
//

#ifdef CONFIG_XUNIQUE

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11

#define QTRACTOR_XUNIQUE "qtractorApplication"

#include <unistd.h> /* for gethostname() */

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#endif	// CONFIG_X11
#else
#include <QSharedMemory>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <QNativeIpcKey>
#endif
#include <QLocalServer>
#include <QLocalSocket>
#include <QHostInfo>
#endif

#endif	// CONFIG_XUNIQUE


// Constructor.
qtractorApplication::qtractorApplication ( int& argc, char **argv )
	: QApplication(argc, argv),
		m_pQtTranslator(nullptr), m_pMyTranslator(nullptr), m_pWidget(nullptr)	
#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11
	, m_pDisplay(nullptr)
	, m_aUnique(0)
	, m_wOwner(0)
#endif	// CONFIG_X11
#else
	, m_pMemory(nullptr)
	, m_pServer(nullptr)
#endif
#endif	// CONFIG_XUNIQUE
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 1, 0)
	QApplication::setApplicationName(QTRACTOR_TITLE);
	QApplication::setApplicationDisplayName(QTRACTOR_TITLE);
	//	QTRACTOR_TITLE " - " + QObject::tr(QTRACTOR_SUBTITLE));
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
	QApplication::setDesktopFileName(
		QString("org.rncbc.%1").arg(PROJECT_NAME));
#endif
	QString sVersion(PROJECT_VERSION);
	sVersion += '\n';
	sVersion += QString("Qt: %1").arg(qVersion());
#if defined(QT_STATIC)
	sVersion += "-static";
#endif
	QApplication::setApplicationVersion(sVersion);
#endif
	// Load translation support.
	QLocale loc;
	if (loc.language() != QLocale::C) {
		// Try own Qt translation...
		m_pQtTranslator = new QTranslator(this);
		QString sLocName = "qt_" + loc.name();
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QString sLocPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
	#else
		QString sLocPath = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
	#endif
		if (m_pQtTranslator->load(sLocName, sLocPath)) {
			QApplication::installTranslator(m_pQtTranslator);
		} else {
			delete m_pQtTranslator;
			m_pQtTranslator = nullptr;
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
			sLocPath = QApplication::applicationDirPath();
			sLocPath.remove(CONFIG_BINDIR);
			sLocPath.append(CONFIG_DATADIR "/qtractor/translations");
			if (m_pMyTranslator->load(sLocName, sLocPath)) {
				QApplication::installTranslator(m_pMyTranslator);
			} else {
				delete m_pMyTranslator;
				m_pMyTranslator = nullptr;
			#ifdef CONFIG_DEBUG
				qWarning("Warning: no translation found for '%s' locale: %s/%s.qm",
					loc.name().toUtf8().constData(),
					sLocPath.toUtf8().constData(),
					sLocName.toUtf8().constData());
			#endif
			}
		}
	}
}


// Destructor.
qtractorApplication::~qtractorApplication (void)
{
#ifdef CONFIG_XUNIQUE
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
	clearServer();
#endif
#endif	// CONFIG_XUNIQUE
	if (m_pMyTranslator) delete m_pMyTranslator;
	if (m_pQtTranslator) delete m_pQtTranslator;
}

// Main application widget accessors.
void qtractorApplication::setMainWidget ( QWidget *pWidget )
{
	m_pWidget = pWidget;
#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11
	m_wOwner = m_pWidget->winId();
	if (m_pDisplay && m_wOwner) {
		XGrabServer(m_pDisplay);
		XSetSelectionOwner(m_pDisplay, m_aUnique, m_wOwner, CurrentTime);
		XUngrabServer(m_pDisplay);
	}
#endif	// CONFIG_X11
#endif
#endif	// CONFIG_XUNIQUE
}


// Check if another instance is running,
// and raise its proper main widget...
bool qtractorApplication::setup (void)
{
#ifdef CONFIG_XUNIQUE
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#ifdef CONFIG_X11
	m_pDisplay = QX11Info::display();
	if (m_pDisplay) {
		QString sUnique = QTRACTOR_XUNIQUE;
		QString sUserName = QString::fromUtf8(::getenv("USER"));
		if (sUserName.isEmpty())
			sUserName = QString::fromUtf8(::getenv("USERNAME"));
		if (!sUserName.isEmpty()) {
			sUnique += ':';
			sUnique += sUserName;
		}
		char szHostName[255];
		if (::gethostname(szHostName, sizeof(szHostName)) == 0) {
			sUnique += '@';
			sUnique += QString::fromUtf8(szHostName);
		}
		m_aUnique = XInternAtom(m_pDisplay, sUnique.toUtf8().constData(), false);
		XGrabServer(m_pDisplay);
		m_wOwner = XGetSelectionOwner(m_pDisplay, m_aUnique);
		XUngrabServer(m_pDisplay);
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
			const QByteArray value = QTRACTOR_XUNIQUE;
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
	}
#endif	// CONFIG_X11
#else
	return setupServer();
#endif
#else
	return false;
#endif	// !CONFIG_XUNIQUE
}


#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

#ifdef CONFIG_X11
#ifdef CONFIG_XUNIQUE

void qtractorApplication::x11PropertyNotify ( Window w )
{
	if (m_pDisplay && m_pWidget && m_wOwner == w) {
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
			m_pWidget->showNormal();
			m_pWidget->raise();
			m_pWidget->activateWindow();
		}
		// Free any left-overs...
		if (iItems > 0 && pData)
			XFree(pData);
	}
}
#endif	// CONFIG_XUNIQUE

bool qtractorApplication::x11EventFilter ( XEvent *pEv )
{
#ifdef CONFIG_XUNIQUE
	if (pEv->type == PropertyNotify
		&& pEv->xproperty.state == PropertyNewValue)
		x11PropertyNotify(pEv->xproperty.window);
#endif	// CONFIG_XUNIQUE
#ifdef CONFIG_VST2
	// Let xevents be processed by VST2 plugin editors...
	if (qtractorVst2Plugin::x11EventFilter(pEv))
		return true;
#endif
	return QApplication::x11EventFilter(pEv);
}
#endif	// CONFIG_X11

#else

#ifdef CONFIG_XUNIQUE

// Local server/shmem setup.
bool qtractorApplication::setupServer (void)
{
	clearServer();

	m_sUnique = QCoreApplication::applicationName();
	QString sUserName = QString::fromUtf8(::getenv("USER"));
	if (sUserName.isEmpty())
		sUserName = QString::fromUtf8(::getenv("USERNAME"));
	if (!sUserName.isEmpty()) {
		m_sUnique += ':';
		m_sUnique += sUserName;
	}
	m_sUnique += '@';
	m_sUnique += QHostInfo::localHostName();
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
	const QNativeIpcKey nativeKey
		= QSharedMemory::legacyNativeKey(m_sUnique);
#if defined(Q_OS_UNIX)
	m_pMemory = new QSharedMemory(nativeKey);
	m_pMemory->attach();
	delete m_pMemory;
#endif
	m_pMemory = new QSharedMemory(nativeKey);
#else
#if defined(Q_OS_UNIX)
	m_pMemory = new QSharedMemory(m_sUnique);
	m_pMemory->attach();
	delete m_pMemory;
#endif
	m_pMemory = new QSharedMemory(m_sUnique);
#endif

	bool bServer = false;
	const qint64 pid = QCoreApplication::applicationPid();
	struct Data { qint64 pid; };
	if (m_pMemory->create(sizeof(Data))) {
		m_pMemory->lock();
		Data *pData = static_cast<Data *> (m_pMemory->data());
		if (pData) {
			pData->pid = pid;
			bServer = true;
		}
		m_pMemory->unlock();
	}
	else
	if (m_pMemory->attach()) {
		m_pMemory->lock(); // maybe not necessary?
		Data *pData = static_cast<Data *> (m_pMemory->data());
		if (pData)
			bServer = (pData->pid == pid);
		m_pMemory->unlock();
	}

	if (bServer) {
		QLocalServer::removeServer(m_sUnique);
		m_pServer = new QLocalServer();
		m_pServer->setSocketOptions(QLocalServer::UserAccessOption);
		m_pServer->listen(m_sUnique);
		QObject::connect(m_pServer,
			 SIGNAL(newConnection()),
			 SLOT(newConnectionSlot()));
	} else {
		QLocalSocket socket;
		socket.connectToServer(m_sUnique);
		if (socket.state() == QLocalSocket::ConnectingState)
			socket.waitForConnected(200);
		if (socket.state() == QLocalSocket::ConnectedState) {
			socket.write(QCoreApplication::arguments().join(' ').toUtf8());
			socket.flush();
			socket.waitForBytesWritten(200);
		}
	}

	return !bServer;
}


// Local server/shmem cleanup.
void qtractorApplication::clearServer (void)
{
	if (m_pServer) {
		m_pServer->close();
		delete m_pServer;
		m_pServer = nullptr;
	}

	if (m_pMemory) {
		delete m_pMemory;
		m_pMemory = nullptr;
	}

	m_sUnique.clear();
}


// Local server conection slot.
void qtractorApplication::newConnectionSlot (void)
{
	QLocalSocket *pSocket = m_pServer->nextPendingConnection();
	QObject::connect(pSocket,
		SIGNAL(readyRead()),
		SLOT(readyReadSlot()));
}

// Local server data-ready slot.
void qtractorApplication::readyReadSlot (void)
{
	QLocalSocket *pSocket = qobject_cast<QLocalSocket *> (sender());
	if (pSocket) {
		const qint64 nread = pSocket->bytesAvailable();
		if (nread > 0) {
			const QByteArray data = pSocket->read(nread);
			// Just make it always shows up fine...
			if (m_pWidget) {
				m_pWidget->showNormal();
				m_pWidget->raise();
				m_pWidget->activateWindow();
			}
			// Reset the server...
			setupServer();
		}
	}
}

#endif	// CONFIG_XUNIQUE
#endif


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
	::signal(signo, SIG_DFL);

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
		execl(shell, shell, "-c", cmd, nullptr);
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
// main - The main program trunk.
//

int main ( int argc, char **argv )
{
	Q_INIT_RESOURCE(qtractor);
#ifdef CONFIG_STACKTRACE
#if defined(__GNUC__) && defined(Q_OS_LINUX)
	::signal(SIGILL,  stacktrace);
	::signal(SIGFPE,  stacktrace);
	::signal(SIGSEGV, stacktrace);
	::signal(SIGABRT, stacktrace);
	::signal(SIGBUS,  stacktrace);
#endif
#endif
#if defined(Q_OS_LINUX) && !defined(CONFIG_WAYLAND)
	::setenv("QT_QPA_PLATFORM", "xcb", 0);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
#if QT_VERSION <  QT_VERSION_CHECK(6, 0, 0)
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
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

	// Custom style sheet (QSS)...
	if (!options.sCustomStyleSheet.isEmpty()) {
		QFile file(options.sCustomStyleSheet);
		if (file.open(QFile::ReadOnly)) {
			app.setStyleSheet(QString::fromUtf8(file.readAll()));
			file.close();
		}
	}

	// Custom style theme...
	QString sPluginsPath = QApplication::applicationDirPath();
	sPluginsPath.remove(CONFIG_BINDIR);
	sPluginsPath.append(CONFIG_PLUGINSDIR);
	if (QDir(sPluginsPath).exists())
		app.addLibraryPath(sPluginsPath);
	if (!options.sCustomStyleTheme.isEmpty())
		app.setStyle(QStyleFactory::create(options.sCustomStyleTheme));

	// Custom color theme (eg. "KXStudio")...
	const QChar sep = QDir::separator();
	QString sPalettePath = QApplication::applicationDirPath();
	sPalettePath.remove(CONFIG_BINDIR);
	sPalettePath.append(CONFIG_DATADIR);
	sPalettePath.append(sep);
	sPalettePath.append(PROJECT_NAME);
	sPalettePath.append(sep);
	sPalettePath.append("palette");
	if (QDir(sPalettePath).exists()) {
		QStringList names;
		names.append("KXStudio");
		names.append("Wonton Soup");
		QStringListIterator name_iter(names);
		while (name_iter.hasNext()) {
			const QString& name = name_iter.next();
			const QFileInfo fi(sPalettePath, name + ".conf");
			if (fi.isReadable()) {
				qtractorPaletteForm::addNamedPaletteConf(
					&options.settings(), name, fi.absoluteFilePath());
			}
		}
	}

	QPalette pal(app.palette());
	if (qtractorPaletteForm::namedPalette(
			&options.settings(), options.sCustomColorTheme, pal))
		app.setPalette(pal);

	// Set default base font...
	if (options.iBaseFontSize > 0)
		app.setFont(QFont(app.font().family(), options.iBaseFontSize));

	// Set custom icon theme...
	QStringList icon_paths;
	if (!options.sCustomIconsTheme.isEmpty())
		icon_paths << options.sCustomIconsTheme;
	icon_paths << ":images";
	QIcon::setFallbackSearchPaths(icon_paths);

	// Construct, setup and show the main form (a pseudo-singleton).
	qtractorMainForm w;
	w.setup(&options);
//	w.show();

	// Settle this one as application main widget...
	app.setMainWidget(&w);

	return app.exec();
}


// end of qtractor.cpp

