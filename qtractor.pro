# qtractor.pro
#
TEMPLATE = subdirs
SUBDIRS = src ladspa_scan vst_scan

ladspa_scan.file = src/qtractor_ladspa_scan.pro
vst_scan.file = src/qtractor_vst_scan.pro

src.depends = ladspa_scan vst_scan
