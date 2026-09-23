# The LV2 build. Everything here is generated or compiled from the same engine
# the terminal program uses, so the plugin cannot drift away from it.

lv2dir = $(libdir)/lv2/cursynth.lv2

ENGINE_SOURCES = $(top_srcdir)/src/cursynth_engine.cpp \
                 $(top_srcdir)/src/cursynth_strings.cpp

ENGINE_FLAGS = -I$(top_srcdir)/src -I$(top_builddir) \
               -I$(top_srcdir)/mopo/src -I$(srcdir)

noinst_PROGRAMS = gen_ttl
gen_ttl_SOURCES = gen_ttl.cpp
gen_ttl_CPPFLAGS = $(ENGINE_FLAGS) $(LV2_CFLAGS)
nodist_gen_ttl_SOURCES = $(ENGINE_SOURCES)
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
	    -fPIC -shared -o $@ $(srcdir)/cursynth_lv2.cpp $(ENGINE_SOURCES) \
	    $(top_builddir)/mopo/src/libmopo.a $(LDFLAGS)

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
