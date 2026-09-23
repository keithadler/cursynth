if BUILD_DOC
DOC_SUBDIR = doc
else
DOC_SUBDIR =
endif

if BUILD_LV2
LV2_SUBDIR = lv2
else
LV2_SUBDIR =
endif

SUBDIRS = cJSON rtaudio rtmidi mopo src $(LV2_SUBDIR) tests po $(DOC_SUBDIR)
DIST_SUBDIRS = cJSON rtaudio rtmidi mopo src lv2 tests po doc
EXTRA_DIST = config.rpath patches

patchesdir = $(pkgdatadir)/patches
patches_DATA = $(top_srcdir)/patches/*.mite

ACLOCAL_AMFLAGS = -I m4
LDFLAGS += -L/usr/local/opt/gettext/lib
CPPFLAGS += -I/usr/local/opt/gettext/include
