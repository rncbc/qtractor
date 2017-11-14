# qtractor_ladspa_scan.pro
#
NAME = qtractor_ladspa_scan

TARGET = $${NAME}
TEMPLATE = app

include(qtractor_ladspa_scan.pri)

HEADERS += qtractor_ladspa_scan.h config.h
SOURCES += qtractor_ladspa_scan.cpp

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

