# The LV2 build. Everything here is generated or compiled from the same engine
# the terminal program uses, so the plugin cannot drift away from it.

lv2dir = $(libdir)/lv2/cursynth.lv2

# The engine, and the DSP under it, compiled straight into the shared object.
#
# Linking libmopo.a in instead looks tidier and does not work: a static library
# built for a program is not built as position independent code, and ELF will
# not put that inside a shared object. macOS never notices, because everything
# there is position independent already, so this only shows up on GNU/Linux.
MOPO_CXX = \
  $(top_srcdir)/mopo/src/delay.cpp $(top_srcdir)/mopo/src/envelope.cpp \
  $(top_srcdir)/mopo/src/feedback.cpp $(top_srcdir)/mopo/src/filter.cpp \
  $(top_srcdir)/mopo/src/linear_slope.cpp $(top_srcdir)/mopo/src/midi_lookup.cpp \
  $(top_srcdir)/mopo/src/mono_panner.cpp $(top_srcdir)/mopo/src/operators.cpp \
  $(top_srcdir)/mopo/src/oscillator.cpp $(top_srcdir)/mopo/src/processor.cpp \
  $(top_srcdir)/mopo/src/processor_router.cpp \
  $(top_srcdir)/mopo/src/send_receive.cpp \
  $(top_srcdir)/mopo/src/smooth_filter.cpp \
  $(top_srcdir)/mopo/src/smooth_value.cpp \
  $(top_srcdir)/mopo/src/step_generator.cpp \
  $(top_srcdir)/mopo/src/trigger_operators.cpp \
  $(top_srcdir)/mopo/src/value.cpp $(top_srcdir)/mopo/src/voice_handler.cpp \
  $(top_srcdir)/mopo/src/wave.cpp

ENGINE_CXX = $(top_srcdir)/src/cursynth_engine.cpp \
                 $(top_srcdir)/src/cursynth_strings.cpp

ENGINE_FLAGS = -I$(top_srcdir)/src -I$(top_builddir) \
               -I$(top_srcdir)/mopo/src -I$(srcdir)

noinst_PROGRAMS = gen_ttl
gen_ttl_SOURCES = gen_ttl.cpp
gen_ttl_CPPFLAGS = $(ENGINE_FLAGS) $(LV2_CFLAGS)
nodist_gen_ttl_SOURCES = $(ENGINE_CXX)
gen_ttl_LDADD = $(top_builddir)/mopo/src/libmopo.a

BUNDLE = cursynth.lv2
MODULE = $(BUNDLE)/cursynth$(LV2_MODULE_EXT)

all-local: $(MODULE) $(BUNDLE)/manifest.ttl

# The port list is read off the engine, so adding a control to the synth adds
# it to the plugin's description without anybody remembering to.
$(BUNDLE)/manifest.ttl $(BUNDLE)/cursynth.ttl: gen_ttl$(EXEEXT)
	$(MKDIR_P) $(BUNDLE)
	./gen_ttl$(EXEEXT) $(BUNDLE)
	sed 's|cursynth@MODULE_EXT@|cursynth$(LV2_MODULE_EXT)|' \
	    $(BUNDLE)/manifest.ttl > $(BUNDLE)/manifest.tmp
	mv $(BUNDLE)/manifest.tmp $(BUNDLE)/manifest.ttl

$(MODULE): $(srcdir)/cursynth_lv2.cpp $(srcdir)/cursynth_lv2.h
	$(MKDIR_P) $(BUNDLE)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(ENGINE_FLAGS) $(LV2_CFLAGS) \
	    -fPIC -shared -o $@ $(srcdir)/cursynth_lv2.cpp $(ENGINE_CXX) \
	    $(MOPO_CXX) $(LDFLAGS)

install-data-local: $(MODULE) $(BUNDLE)/manifest.ttl
	$(MKDIR_P) $(DESTDIR)$(lv2dir)
	$(INSTALL_DATA) $(BUNDLE)/manifest.ttl $(DESTDIR)$(lv2dir)/
	$(INSTALL_DATA) $(BUNDLE)/cursynth.ttl $(DESTDIR)$(lv2dir)/
	$(INSTALL_PROGRAM) $(MODULE) $(DESTDIR)$(lv2dir)/

uninstall-local:
	rm -rf $(DESTDIR)$(lv2dir)

clean-local:
	rm -rf $(BUNDLE)

EXTRA_DIST = cursynth_lv2.cpp cursynth_lv2.h gen_ttl.cpp
