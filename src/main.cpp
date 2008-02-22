// main.cpp
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

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qtractorAbout.h"
#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <QApplication>
#include <QTranslator>
#include <QLocale>

#ifdef CONFIG_VST
#if defined(Q_WS_X11)
#include <X11/Xlib.h>
#endif
#endif


//-------------------------------------------------------------------------
// main - The main program trunk.
//

int main ( int argc, char **argv )
{
#ifdef CONFIG_VST
#if defined(Q_WS_X11)
	XInitThreads();
#endif
#endif

	QApplication app(argc, argv);

	// Load translation support.
	QTranslator translator(0);
	QLocale loc;
	if (loc.language() != QLocale::C) {
		QString sLocName = "qtractor_" + loc.name();
		if (!translator.load(sLocName, ".")) {
			QString sLocPath = CONFIG_PREFIX "/share/locale";
			if (!translator.load(sLocName, sLocPath))
				fprintf(stderr, "Warning: no locale found: %s/%s.qm\n",
					sLocPath.toUtf8().constData(),
					sLocName.toUtf8().constData());
		}
		app.installTranslator(&translator);
	}

	// Construct default settings; override with command line arguments.
	qtractorOptions options;
	if (!options.parse_args(app.argc(), app.argv())) {
		app.quit();
		return 1;
	}

	// Construct, setup and show the main form (a pseudo-singleton).
	qtractorMainForm w;
//	app.setMainWidget(&w);
	w.setOptions(&options);
	w.show();

	// Register the quit signal/slot.
	// app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));

	return app.exec();
}

// end of main.cpp

