AC_INIT([fmplayer], [0.1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC_C99
AC_PROG_RANLIB
AM_PROG_AR
AM_PROG_AS

AC_ARG_WITH([jack],
  [AS_HELP_STRING([--without-jack], [disable support for JACK audio])])
AC_ARG_WITH([pulse],
  [AS_HELP_STRING([--without-pulse], [disable support for pulseaudio])])
AC_ARG_WITH([alsa],
  [AS_HELP_STRING([--without-alsa], [disable support for alsa])])

AS_IF([test "x$with_jack" = "xyes"], [
  PKG_CHECK_MODULES([JACK], [jack soxr])
  jack_found=yes
], [test "x$with_jack" != "xno"], [
  PKG_CHECK_MODULES([JACK], [jack soxr], [jack_found=yes], [jack_found=no])
], [
  jack_found=no
])

AS_IF([test "x$with_pulse" = "xyes"], [
  PKG_CHECK_MODULES([PULSE], [libpulse])
  pulse_found=yes
], [test "x$with_pulse" != "xno"], [
  PKG_CHECK_MODULES([PULSE], [libpulse], [pulse_found=yes], [pulse_found=no])
], [
  pulse_found=no
])

AS_IF([test "x$with_alsa" = "xyes"], [
  PKG_CHECK_MODULES([ALSA], [alsa])
  alsa_found=yes
], [test "x$with_alsa" != "xno"], [
  PKG_CHECK_MODULES([ALSA], [alsa], [alsa_found=yes], [alsa_found=no])
], [
  alsa_found=no
])

PKG_CHECK_MODULES([GTK3], [gtk+-3.0 cairo])
PKG_CHECK_MODULES([SNDFILE], [sndfile])

AS_IF([test "x$jack_found" = "xno" -a "x$pulse_found" = "xno" -a "x$alsa_found" = "xno"], [
  AC_MSG_ERROR([No audio output backend found or enabled])
])

AM_CONDITIONAL([ENABLE_JACK], [test "x$jack_found" = "xyes"])
AS_IF([test "x$jack_found" = "xyes"], [
  AC_DEFINE([ENABLE_JACK])
])
AM_CONDITIONAL([ENABLE_PULSE], [test "x$pulse_found" = "xyes"])
AS_IF([test "x$pulse_found" = "xyes"], [
  AC_DEFINE([ENABLE_PULSE])
])
AM_CONDITIONAL([ENABLE_ALSA], [test "x$alsa_found" = "xyes"])
AS_IF([test "x$alsa_found" = "xyes"], [
  AC_DEFINE([ENABLE_ALSA])
])

AC_ARG_ENABLE([neon], AS_HELP_STRING([--enable-neon], [Enable NEON optimized functions for SSG sinc filtering and fmdsp palette lookup. Tested on Cortex-A53 (Raspberry PI 3)]))
AM_CONDITIONAL([ENABLE_NEON], [test "x$enable_neon" = "xyes"])
AS_IF([test "x$enable_neon" = "xyes"], [
  AC_DEFINE([ENABLE_NEON])
])

AC_CHECK_HEADER([emmintrin.h], [emmintrin_found=yes])
AM_CONDITIONAL([ENABLE_SSE], [test "x$emmintrin_found" = "xyes"])
AS_IF([test "x$emmintrin_found" = "xyes"], [
  AC_DEFINE([ENABLE_SSE])
])

AC_ARG_ENABLE([opengl], AS_HELP_STRING([--enable-opengl], [Enable OpenGL rendering for Oscilloscope view (default: enable if found)]))
AS_IF([test "x$enable_opengl" != "xno"], [
  AC_CHECK_LIB([GL], [glGetString], [opengl_found=yes])
  AC_CHECK_HEADER([GL/glcorearb.h], , [opengl_found=])
  AS_IF([test "x$enable_opengl" = "x" -a "x$opengl_found" = "xyes"], [
    enable_opengl=yes
  ])
  AS_IF([test "x$enable_opengl" = "xyes" -a "x$opengl_found" != "xyes"], [
    AC_MSG_ERROR([OpenGL header/library not found (-lGL, GL/glcorearb.h)], [1])
  ])
])
AM_CONDITIONAL([ENABLE_OPENGL], [test "x$enable_opengl" = "xyes"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
