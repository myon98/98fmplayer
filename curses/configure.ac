AC_INIT([fmplayer], [0.1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC_C99

PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
AC_ARG_WITH([samplerate], AS_HELP_STRING([--with-samplerate], [Use libsamplerate and output at native samplerate (needed on NetBSD)]))
AS_IF([test "x$with_samplerate" = "xyes"], [
  PKG_CHECK_MODULES([SAMPLERATE], [samplerate])
  AC_DEFINE([HAVE_SAMPLERATE])
])
AX_WITH_CURSES
AM_ICONV

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
