# qtractor_plugin_scan.pro
#
NAME = qtractor

TARGET = $${NAME}_plugin_scan
TEMPLATE = app

include(qtractor_plugin_scan.pri)

HEADERS += qtractor_plugin_scan.h config.h
SOURCES += qtractor_plugin_scan.cpp

unix {

	isEmpty(PREFIX) {
		PREFIX = /usr/local
	}

	isEmpty(LIBDIR) {
		TARGET_ARCH = $$system(uname -m)
		contains(TARGET_ARCH, x86_64) {
			LIBDIR = $${PREFIX}/lib64
		} else {
			LIBDIR = $${PREFIX}/lib
		}
	}

	TARGET_PATH = $${LIBDIR}/$${NAME}

	# make install
	INSTALLS += target

	target.path = $${TARGET_PATH}
}

# No GUI support
QT -= gui

