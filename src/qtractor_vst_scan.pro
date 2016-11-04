# qtractor_vst_scan.pro
#
NAME = qtractor_vst_scan

TARGET = $${NAME}
TEMPLATE = app

include(qtractor_vst_scan.pri)

HEADERS += qtractor_vst_scan.h config.h
SOURCES += qtractor_vst_scan.cpp

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

