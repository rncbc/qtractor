# qtractor_dssi_scan.pro
#
NAME = qtractor_dssi_scan

TARGET = $${NAME}
TEMPLATE = app

include(qtractor_dssi_scan.pri)

HEADERS += qtractor_dssi_scan.h config.h
SOURCES += qtractor_dssi_scan.cpp

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

