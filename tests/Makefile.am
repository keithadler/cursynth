# cursynth had no tests at all. These judge the oscillator by its spectrum and
# by its shape, because those are the only things about an oscillator a
# listener can hear.

check_PROGRAMS = test_wave

test_wave_SOURCES = test_wave.cpp
test_wave_CPPFLAGS = -I$(top_srcdir)/mopo/src -I$(top_builddir)
test_wave_LDADD = $(top_builddir)/mopo/src/libmopo.a

TESTS = $(check_PROGRAMS)
