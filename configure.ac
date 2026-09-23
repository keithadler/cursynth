#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([cursynth], [1.7.0], [https://github.com/keithadler/cursynth/issues])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/cursynth.cpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX([c++ g++ cxx])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AM_PROG_CC_C_O
AM_PROG_AR

# Look for pkg-config here, at the top, rather than leaving it to be pulled in
# by the first thing that needs it. The first thing that needed it was inside
# the GNU/Linux branch of a case statement, so on any other system it was never
# run and every later pkg-config check quietly answered no.
PKG_PROG_PKG_CONFIG

# gettext is installed off the default include path on macOS, and where it
# lives depends on the processor: Homebrew moved from /usr/local to
# /opt/homebrew when Apple Silicon arrived. cursynth used to hard code the old
# path, which is why it stopped building on every Mac sold after 2020 and why
# "cannot find libintl.h" is the most reported problem in its history. Look for
# it instead, and carry on without it if it is not there.
AC_ARG_WITH([gettext-prefix],
  [AS_HELP_STRING([--with-gettext-prefix=DIR],
                  [directory where gettext is installed])],
  [cursynth_gettext_prefix="$withval"],
  [cursynth_gettext_prefix=auto])

AC_MSG_CHECKING([where to find libintl.h])
AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <libintl.h>]])],
  [AC_MSG_RESULT([the default include path])],
  [cursynth_intl_dir=""
   if test "x$cursynth_gettext_prefix" != xauto; then
     cursynth_intl_search="$cursynth_gettext_prefix"
   else
     cursynth_intl_search=""
     if brew --prefix gettext >/dev/null 2>&1; then
       cursynth_intl_search="`brew --prefix gettext 2>/dev/null`"
     fi
     cursynth_intl_search="$cursynth_intl_search /opt/homebrew/opt/gettext \
                           /usr/local/opt/gettext /opt/local /usr/pkg"
   fi
   for cursynth_d in $cursynth_intl_search; do
     if test -f "$cursynth_d/include/libintl.h"; then
       cursynth_intl_dir="$cursynth_d"
       break
     fi
   done
   if test -n "$cursynth_intl_dir"; then
     CPPFLAGS="$CPPFLAGS -I$cursynth_intl_dir/include"
     LDFLAGS="$LDFLAGS -L$cursynth_intl_dir/lib"
     AC_MSG_RESULT([$cursynth_intl_dir])
   else
     AC_MSG_RESULT([nowhere; building without translation])
   fi])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18.3])

# Checks for libraries.
AS_IF([test "x$USE_NLS" = xyes], [AC_CHECK_LIB([intl], [gettext])])

# Ask pkg-config where curses is rather than assuming <ncurses.h> sits on the
# default include path. On MSYS2 it does not: the headers are under ncursesw,
# so the build got as far as the first source file and stopped. The same is
# true of the wide character build on several GNU/Linux distributions.
cursynth_have_curses=no
PKG_CHECK_MODULES([NCURSES], [ncursesw],
  [cursynth_have_curses=yes],
  [PKG_CHECK_MODULES([NCURSES], [ncurses],
    [cursynth_have_curses=yes],
    [cursynth_have_curses=no])])
AS_IF([test "x$cursynth_have_curses" = xyes],
  [CPPFLAGS="$CPPFLAGS $NCURSES_CFLAGS"
   LIBS="$LIBS $NCURSES_LIBS"],
  [AC_CHECK_LIB([ncurses], [curs_set], [],
     [AC_MSG_ERROR([cursynth needs ncurses. Install libncurses-dev, or
                    mingw-w64-x86_64-ncurses on MSYS2.])])])

AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h float.h libintl.h limits.h locale.h math.h ncurses.h stddef.h stdlib.h string.h strings.h sys/ioctl.h sys/time.h unistd.h])

# Get flags for rtaudio and rtmidi.
AC_CANONICAL_HOST
AC_MSG_CHECKING(for audio API)
case $host in
  *-*-netbsd*)
    AC_MSG_RESULT(using OSS)
    api="$api -D__LINUX_OSS__"
    LIBS="$LIBS -lossaudio"
    AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(cursynth requires the pthread library!))
  ;;

  *-*-linux*)
  AC_ARG_WITH(jack, [  --with-jack = choose JACK server support (Mac OSX and GNU/Linux only)], [
    api="$api -D__UNIX_JACK__"
    AC_MSG_RESULT(using JACK)
    AC_CHECK_LIB(jack, jack_client_open, , AC_MSG_ERROR(JACK support requires the jack library!))
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(Jack support also requires the asound library!))], )

  # Look for ALSA flag
  AC_ARG_WITH(alsa, [  --with-alsa = choose native ALSA API support (GNU/Linux only)], [
    api="$api -D__LINUX_ALSA__"
    req="$req alsa"
    AC_MSG_RESULT(using ALSA)
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(ALSA support requires the asound library!))], )

  # Look for PULSE flag
  AC_ARG_WITH(pulse, [  --with-pulse = choose PulseAudio API support (GNU/Linux only)], [
    api="$api -D__LINUX_PULSE__"
    req="$req libpulse-simple"
    AC_MSG_RESULT(using PulseAudio)
    PKG_CHECK_MODULES([PULSE], [libpulse-simple], , AC_MSG_ERROR(PulseAudio support requires the pulse-simple library!))
        LIBS="$LIBS `pkg-config --libs libpulse-simple`" ], )

  # Look for OSS flag
  AC_ARG_WITH(oss, [  --with-oss = choose OSS API support (GNU/Linux only)], [
    api="$api -D__LINUX_OSS__"
    AC_MSG_RESULT(using OSS)], )

  # If no audio api flags specified, use ALSA
  if [test "$api" == "";] then
    AC_MSG_RESULT(using ALSA)
    AC_SUBST( api, [-D__LINUX_ALSA__] )
    req="$req alsa"
    AC_CHECK_LIB(asound, snd_pcm_open, , AC_MSG_ERROR(ALSA support requires the asound library!))
  fi

  AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(cursynth requires the pthread library!))
  ;;

  *-apple*)
  AC_ARG_WITH(jack, [  --with-jack = choose JACK server support (unix only)], [
    api="$api -D__UNIX_JACK__"
    AC_MSG_RESULT(using JACK)
    AC_CHECK_LIB(jack, jack_client_open, , AC_MSG_ERROR(JACK support requires the jack library!))], )

  # Look for Core flag
  AC_ARG_WITH(core, [  --with-core = choose CoreAudio API support (MAC OSX only)], [
    api="$api -D__MACOSX_CORE__"
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h, [], [AC_MSG_ERROR(CoreAudio header files not found!)] )
    LIBS="$LIBS -framework CoreAudio -framework CoreMIDI -framework CoreFoundation" ], )

  # If no audio api flags specified, use CoreAudio
  if [test "$api" == ""; ] then
    AC_SUBST( api, [-D__MACOSX_CORE__] )
    AC_MSG_RESULT(using CoreAudio)
    AC_CHECK_HEADER(CoreAudio/CoreAudio.h,
      [],
      [AC_MSG_ERROR(CoreAudio header files not found!)] )
    LIBS="$LIBS -framework CoreAudio -framework CoreMIDI -framework CoreFoundation"
  fi

  AC_CHECK_LIB(pthread, pthread_create, , AC_MSG_ERROR(cursynth requires the pthread library!))
  ;;

  *-mingw32*)
  AC_ARG_WITH(asio, [  --with-asio = choose ASIO API support (Windows only)], [
    api="$api -D__WINDOWS_ASIO__"
    AC_MSG_RESULT(using ASIO)
    AC_SUBST( objects, ["asio.o asiodrivers.o asiolist.o iasiothiscallresolver.o"] ) ], )

  # Look for DirectSound flag
  AC_ARG_WITH(ds, [  --with-ds = choose DirectSound API support (Windows only)], [
    api="$api -D__WINDOWS_DS__"
    AC_MSG_RESULT(using DirectSound)
    LIBS="-ldsound -lwinmm $LIBS" ], )

  # If no audio api flags specified, use DirectSound
  if [test "$api" == "";] then
    AC_SUBST( api, [-D__WINDOWS_DS__] )
    AC_MSG_RESULT(using DirectSound)
    LIBS="-ldsound -lwinmm $LIBS"
  fi

  LIBS="-lole32 $LIBS"
  ;;

  *)
  # Default case for unknown realtime systems.
  AC_MSG_ERROR(Unknown system type for realtime support!)
  ;;
esac

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([dup2 floor gettimeofday memset modf pow rmdir strcasecmp strchr strdup strerror])

# Two people reported that cursynth would not build because makeinfo was
# missing. makeinfo builds the manual. A missing manual is a missing manual; it
# is not a reason to refuse to build a synthesizer, so the manual is built when
# the tool for it is present and skipped when it is not.
AC_CHECK_PROG([CURSYNTH_MAKEINFO], [makeinfo], [yes], [no])
AM_CONDITIONAL([BUILD_DOC], [test "x$CURSYNTH_MAKEINFO" = xyes])
AS_IF([test "x$CURSYNTH_MAKEINFO" = xno],
  [AC_MSG_WARN([makeinfo not found: the manual will not be built. Install texinfo if you want it.])])

# The LV2 plugin, which was asked for in 2014 and is still in the README's list
# of things to do. Built when the LV2 headers are there and skipped when they
# are not, since a missing plugin should not stop the program being built.
PKG_CHECK_MODULES([LV2], [lv2 >= 1.10], [have_lv2=yes], [have_lv2=no])
AM_CONDITIONAL([BUILD_LV2], [test "x$have_lv2" = xyes])
case $host in
  *-apple*)  LV2_MODULE_EXT=".dylib" ;;
  *-mingw*)  LV2_MODULE_EXT=".dll" ;;
  *)         LV2_MODULE_EXT=".so" ;;
esac
AC_SUBST([LV2_MODULE_EXT])
AS_IF([test "x$have_lv2" = xno],
  [AC_MSG_WARN([LV2 headers not found: the plugin will not be built.])])

# dlopen lives in libc on some systems and libdl on others; the little LV2 host
# in the tests needs whichever it is.
AC_SEARCH_LIBS([dlopen], [dl dld], [LIBDL="$ac_cv_search_dlopen"], [LIBDL=])
AS_IF([test "x$LIBDL" = "xnone required"], [LIBDL=])
AC_SUBST([LIBDL])

AC_CONFIG_SUBDIRS([mopo
                   rtaudio
                   rtmidi])

AC_CONFIG_FILES([Makefile po/Makefile.in
                 src/Makefile
                 tests/Makefile
                 lv2/Makefile
                 cJSON/Makefile
                 doc/Makefile])
AC_OUTPUT
