# qtractor.pro
#
TEMPLATE = subdirs
SUBDIRS = src qtractor_vst_scan

qtractor_vst_scan.file = src/qtractor_vst_scan.pro

src.depends = qtractor_vst_scan
