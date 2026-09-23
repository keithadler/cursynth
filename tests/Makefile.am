# cursynth had no tests at all. These judge the oscillator by its spectrum and
# by its shape, because those are the only things about an oscillator a
# listener can hear, and they play the plugin rather than only building it.

check_PROGRAMS = test_wave
TESTS = test_wave

test_wave_SOURCES = test_wave.cpp
test_wave_CPPFLAGS = -I$(top_srcdir)/mopo/src -I$(top_builddir)
test_wave_LDADD = $(top_builddir)/mopo/src/libmopo.a

if BUILD_LV2
# test_lv2 is the host, not the test: lv2_check.sh drives it at several volume
# settings and decides whether the answers are right.
check_PROGRAMS += test_lv2
TESTS += lv2_check.sh

test_lv2_SOURCES = test_lv2.cpp
test_lv2_CPPFLAGS = $(LV2_CFLAGS)
test_lv2_LDADD = $(LIBDL)

AM_TESTS_ENVIRONMENT = \
  CURSYNTH_BUNDLE='$(abs_top_builddir)/lv2/cursynth.lv2'; export CURSYNTH_BUNDLE; \
  CURSYNTH_HOST='$(abs_builddir)/test_lv2$(EXEEXT)'; export CURSYNTH_HOST;
endif

EXTRA_DIST = lv2_check.sh
