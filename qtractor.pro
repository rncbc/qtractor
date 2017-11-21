# qtractor.pro
#
TEMPLATE = subdirs
SUBDIRS = src plugin_scan

plugin_scan.file = src/qtractor_plugin_scan.pro

src.depends = plugin_scan
