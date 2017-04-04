AC_INIT([fmplayer], [0.1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC_C99
AC_PROG_RANLIB
AM_PROG_AR
AM_PROG_AS

dnl AM_PATH_SDL2([2.0.5])
PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 cairo])

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
