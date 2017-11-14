# qtractor_plugin_scan.pro
#
NAME = qtractor_plugin_scan

TARGET = $${NAME}
TEMPLATE = app

include(qtractor_plugin_scan.pri)

HEADERS += qtractor_plugin_scan.h config.h
SOURCES += qtractor_plugin_scan.cpp

unix {

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	isEmpty(BINDIR) {
		BINDIR = $${BINDIR}/bin
	}

	# make install
	INSTALLS += target

	target.path = $${BINDIR}
}

# No GUI support
QT -= gui

